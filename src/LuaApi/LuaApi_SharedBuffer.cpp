#include "LuaApi.h"

#include "../LuaRuntime/LuaStackGuard.h"
#include "../LuaRuntime/LuaUserdata.h"

#include "Logger.h"
#include "ScriptContext.h"
#include "SharedBuffer.h"

#include <cctype>
#include <cstdint>
#include <cstring>
#include <exception>
#include <memory>
#include <string>
#include <vector>

constexpr const char *kSharedBufferMt = "BallanceTAS.SharedBuffer";
constexpr uint8_t kValueNil = 0;
constexpr uint8_t kValueBool = 1;
constexpr uint8_t kValueInteger = 2;
constexpr uint8_t kValueNumber = 3;
constexpr uint8_t kValueString = 4;
constexpr uint8_t kValueTable = 5;
constexpr int kMaxSerializedDepth = 16;

using SharedBufferHandle = std::shared_ptr<SharedBuffer>;

static SharedBufferHandle *CheckBufferHandle(lua_State *L, int index) {
    auto *handle = tas::lua::CheckUserdata<SharedBufferHandle>(L, index, kSharedBufferMt);
    if (!handle || !*handle) {
        luaL_error(L, "SharedBuffer: invalid buffer");
    }
    return handle;
}

static SharedBuffer &CheckBuffer(lua_State *L, int index) {
    return **CheckBufferHandle(L, index);
}

static const SharedBuffer &CheckConstBuffer(lua_State *L, int index) {
    return **CheckBufferHandle(L, index);
}

static void PushBuffer(lua_State *L, SharedBufferHandle buffer) {
    tas::lua::PushOwnedUserdata<SharedBufferHandle>(L, kSharedBufferMt, std::move(buffer));
}

template <typename T>
static void AppendPod(std::vector<uint8_t> &out, T value) {
    const auto *bytes = reinterpret_cast<const uint8_t *>(&value);
    out.insert(out.end(), bytes, bytes + sizeof(T));
}

static void AppendString(std::vector<uint8_t> &out, const char *data, size_t length) {
    AppendPod<uint32_t>(out, static_cast<uint32_t>(length));
    out.insert(out.end(), reinterpret_cast<const uint8_t *>(data), reinterpret_cast<const uint8_t *>(data) + length);
}

static bool SerializeLuaValue(lua_State *L, int index, std::vector<uint8_t> &out, int depth) {
    if (depth <= 0) {
        luaL_error(L, "shared_buffer.from_table: table nesting exceeds limit");
        return false;
    }

    index = lua_absindex(L, index);
    const int type = lua_type(L, index);
    switch (type) {
    case LUA_TNIL:
        out.push_back(kValueNil);
        return true;
    case LUA_TBOOLEAN:
        out.push_back(kValueBool);
        out.push_back(lua_toboolean(L, index) ? 1 : 0);
        return true;
    case LUA_TNUMBER:
        if (lua_isinteger(L, index)) {
            out.push_back(kValueInteger);
            AppendPod<lua_Integer>(out, lua_tointeger(L, index));
        } else {
            out.push_back(kValueNumber);
            AppendPod<lua_Number>(out, lua_tonumber(L, index));
        }
        return true;
    case LUA_TSTRING: {
        size_t length = 0;
        const char *data = lua_tolstring(L, index, &length);
        out.push_back(kValueString);
        AppendString(out, data ? data : "", length);
        return true;
    }
    case LUA_TTABLE: {
        out.push_back(kValueTable);
        const size_t countOffset = out.size();
        AppendPod<uint32_t>(out, 0);
        uint32_t count = 0;
        lua_pushnil(L);
        while (lua_next(L, index) != 0) {
            if (!lua_isinteger(L, -2) && !lua_isstring(L, -2)) {
                luaL_error(L, "shared_buffer.from_table: table keys must be integer or string");
                return false;
            }
            SerializeLuaValue(L, -2, out, depth - 1);
            SerializeLuaValue(L, -1, out, depth - 1);
            ++count;
            lua_pop(L, 1);
        }
        std::memcpy(out.data() + countOffset, &count, sizeof(count));
        return true;
    }
    default:
        luaL_error(L, "shared_buffer.from_table: unsupported value type '%s'", lua_typename(L, type));
        return false;
    }
}

template <typename T>
static T ReadPodValue(lua_State *L, const uint8_t *&cursor, const uint8_t *end, const char *context) {
    if (static_cast<size_t>(end - cursor) < sizeof(T)) {
        luaL_error(L, "%s: truncated buffer", context);
    }
    T value{};
    std::memcpy(&value, cursor, sizeof(T));
    cursor += sizeof(T);
    return value;
}

static void PushSerializedValue(lua_State *L, const uint8_t *&cursor, const uint8_t *end, int depth) {
    if (depth <= 0) {
        luaL_error(L, "shared_buffer.to_table: table nesting exceeds limit");
    }
    if (cursor >= end) {
        luaL_error(L, "shared_buffer.to_table: truncated buffer");
    }

    const uint8_t tag = *cursor++;
    switch (tag) {
    case kValueNil:
        lua_pushnil(L);
        return;
    case kValueBool:
        if (cursor >= end) {
            luaL_error(L, "shared_buffer.to_table: truncated boolean");
        }
        lua_pushboolean(L, *cursor++ != 0);
        return;
    case kValueInteger:
        lua_pushinteger(L, ReadPodValue<lua_Integer>(L, cursor, end, "shared_buffer.to_table"));
        return;
    case kValueNumber:
        lua_pushnumber(L, ReadPodValue<lua_Number>(L, cursor, end, "shared_buffer.to_table"));
        return;
    case kValueString: {
        const uint32_t length = ReadPodValue<uint32_t>(L, cursor, end, "shared_buffer.to_table");
        if (static_cast<size_t>(end - cursor) < length) {
            luaL_error(L, "shared_buffer.to_table: truncated string");
        }
        lua_pushlstring(L, reinterpret_cast<const char *>(cursor), length);
        cursor += length;
        return;
    }
    case kValueTable: {
        const uint32_t count = ReadPodValue<uint32_t>(L, cursor, end, "shared_buffer.to_table");
        lua_newtable(L);
        for (uint32_t i = 0; i < count; ++i) {
            PushSerializedValue(L, cursor, end, depth - 1);
            PushSerializedValue(L, cursor, end, depth - 1);
            lua_settable(L, -3);
        }
        return;
    }
    default:
        luaL_error(L, "shared_buffer.to_table: invalid value tag");
    }
}

static size_t CheckSize(lua_State *L, int index, const char *name) {
    const lua_Integer value = luaL_checkinteger(L, index);
    if (value < 0) {
        luaL_error(L, "%s must be non-negative", name);
    }
    return static_cast<size_t>(value);
}

static size_t OptionalSize(lua_State *L, int index, size_t fallback, const char *name) {
    if (lua_isnoneornil(L, index)) {
        return fallback;
    }
    return CheckSize(L, index, name);
}

static void CheckRange(lua_State *L, const SharedBuffer &buffer, size_t offset, size_t width, const char *functionName) {
    if (offset > buffer.Size() || width > buffer.Size() - offset) {
        luaL_error(L, "%s: offset out of bounds", functionName);
    }
}

template <typename T>
static int ReadPod(lua_State *L, const char *functionName) {
    const auto &buffer = CheckConstBuffer(L, 1);
    const size_t offset = CheckSize(L, 2, "offset");
    CheckRange(L, buffer, offset, sizeof(T), functionName);

    T value{};
    std::memcpy(&value, buffer.Data() + offset, sizeof(T));
    if constexpr (std::is_floating_point_v<T>) {
        lua_pushnumber(L, static_cast<lua_Number>(value));
    } else {
        lua_pushinteger(L, static_cast<lua_Integer>(value));
    }
    return 1;
}

template <typename T>
static int WritePod(lua_State *L, const char *functionName) {
    auto &buffer = CheckBuffer(L, 1);
    const size_t offset = CheckSize(L, 2, "offset");
    CheckRange(L, buffer, offset, sizeof(T), functionName);

    T value{};
    if constexpr (std::is_floating_point_v<T>) {
        value = static_cast<T>(luaL_checknumber(L, 3));
    } else {
        value = static_cast<T>(luaL_checkinteger(L, 3));
    }
    std::memcpy(buffer.Data() + offset, &value, sizeof(T));
    return 0;
}

static int BufferSize(lua_State *L) {
    lua_pushinteger(L, static_cast<lua_Integer>(CheckConstBuffer(L, 1).Size()));
    return 1;
}

static int ReadU8(lua_State *L) {
    const auto &buffer = CheckConstBuffer(L, 1);
    const size_t offset = CheckSize(L, 2, "offset");
    CheckRange(L, buffer, offset, 1, "SharedBuffer read_u8");
    lua_pushinteger(L, static_cast<lua_Integer>(buffer.Data()[offset]));
    return 1;
}

static int WriteU8(lua_State *L) {
    auto &buffer = CheckBuffer(L, 1);
    const size_t offset = CheckSize(L, 2, "offset");
    CheckRange(L, buffer, offset, 1, "SharedBuffer write_u8");
    buffer.Data()[offset] = static_cast<uint8_t>(luaL_checkinteger(L, 3));
    return 0;
}

static int ReadU16(lua_State *L) { return ReadPod<uint16_t>(L, "SharedBuffer read_u16"); }
static int WriteU16(lua_State *L) { return WritePod<uint16_t>(L, "SharedBuffer write_u16"); }
static int ReadU32(lua_State *L) { return ReadPod<uint32_t>(L, "SharedBuffer read_u32"); }
static int WriteU32(lua_State *L) { return WritePod<uint32_t>(L, "SharedBuffer write_u32"); }
static int ReadI32(lua_State *L) { return ReadPod<int32_t>(L, "SharedBuffer read_i32"); }
static int WriteI32(lua_State *L) { return WritePod<int32_t>(L, "SharedBuffer write_i32"); }
static int ReadF32(lua_State *L) { return ReadPod<float>(L, "SharedBuffer read_f32"); }
static int WriteF32(lua_State *L) { return WritePod<float>(L, "SharedBuffer write_f32"); }
static int ReadF64(lua_State *L) { return ReadPod<double>(L, "SharedBuffer read_f64"); }
static int WriteF64(lua_State *L) { return WritePod<double>(L, "SharedBuffer write_f64"); }

static int ReadString(lua_State *L) {
    const auto &buffer = CheckConstBuffer(L, 1);
    const size_t offset = CheckSize(L, 2, "offset");
    if (offset >= buffer.Size()) {
        return luaL_error(L, "SharedBuffer read_string: offset out of bounds");
    }

    size_t length = 0;
    if (lua_isnoneornil(L, 3)) {
        const char *data = reinterpret_cast<const char *>(buffer.Data() + offset);
        const size_t remaining = buffer.Size() - offset;
        while (length < remaining && data[length] != '\0') {
            ++length;
        }
    } else {
        length = CheckSize(L, 3, "length");
        CheckRange(L, buffer, offset, length, "SharedBuffer read_string");
    }

    lua_pushlstring(L, reinterpret_cast<const char *>(buffer.Data() + offset), length);
    return 1;
}

static int WriteString(lua_State *L) {
    auto &buffer = CheckBuffer(L, 1);
    const size_t offset = CheckSize(L, 2, "offset");
    size_t length = 0;
    const char *data = luaL_checklstring(L, 3, &length);
    CheckRange(L, buffer, offset, length, "SharedBuffer write_string");
    std::memcpy(buffer.Data() + offset, data, length);
    return 0;
}

static int WriteStringZ(lua_State *L) {
    auto &buffer = CheckBuffer(L, 1);
    const size_t offset = CheckSize(L, 2, "offset");
    size_t length = 0;
    const char *data = luaL_checklstring(L, 3, &length);
    CheckRange(L, buffer, offset, length + 1, "SharedBuffer write_string_z");
    std::memcpy(buffer.Data() + offset, data, length);
    buffer.Data()[offset + length] = '\0';
    return 0;
}

static int Fill(lua_State *L) {
    auto &buffer = CheckBuffer(L, 1);
    const auto value = static_cast<uint8_t>(luaL_checkinteger(L, 2));
    const size_t offset = OptionalSize(L, 3, 0, "offset");
    if (offset >= buffer.Size()) {
        return luaL_error(L, "SharedBuffer fill: offset out of bounds");
    }
    const size_t length = OptionalSize(L, 4, buffer.Size() - offset, "length");
    CheckRange(L, buffer, offset, length, "SharedBuffer fill");
    std::memset(buffer.Data() + offset, value, length);
    return 0;
}

static int Clone(lua_State *L) {
    PushBuffer(L, CheckConstBuffer(L, 1).Clone());
    return 1;
}

static int ToHex(lua_State *L) {
    const auto &buffer = CheckConstBuffer(L, 1);
    const size_t offset = OptionalSize(L, 2, 0, "offset");
    if (offset >= buffer.Size()) {
        return luaL_error(L, "SharedBuffer to_hex: offset out of bounds");
    }
    const size_t length = OptionalSize(L, 3, buffer.Size() - offset, "length");
    CheckRange(L, buffer, offset, length, "SharedBuffer to_hex");

    std::string hex;
    hex.reserve(length * 2);
    constexpr char kHex[] = "0123456789abcdef";
    for (size_t i = 0; i < length; ++i) {
        const uint8_t byte = buffer.Data()[offset + i];
        hex.push_back(kHex[(byte >> 4) & 0x0F]);
        hex.push_back(kHex[byte & 0x0F]);
    }
    lua_pushlstring(L, hex.data(), hex.size());
    return 1;
}

static int ToString(lua_State *L) {
    const auto &buffer = CheckConstBuffer(L, 1);
    lua_pushfstring(L, "SharedBuffer(size=%d)", static_cast<int>(buffer.Size()));
    return 1;
}

static int ToTable(lua_State *L) {
    const auto &buffer = CheckConstBuffer(L, 1);
    const uint8_t *cursor = buffer.Data();
    const uint8_t *end = cursor + buffer.Size();
    PushSerializedValue(L, cursor, end, kMaxSerializedDepth);
    if (cursor != end) {
        return luaL_error(L, "shared_buffer.to_table: trailing data");
    }
    return 1;
}

static int Create(lua_State *L) {
    const size_t size = CheckSize(L, 1, "size");
    try {
        PushBuffer(L, SharedBuffer::Create(size));
        return 1;
    } catch (const std::exception &e) {
        Log::Error("Error in shared_buffer.create: %s", e.what());
        return luaL_error(L, "shared_buffer.create: %s", e.what());
    }
}

static int FromString(lua_State *L) {
    size_t length = 0;
    const char *data = luaL_checklstring(L, 1, &length);
    try {
        PushBuffer(L, SharedBuffer::CreateFrom(data, length));
        return 1;
    } catch (const std::exception &e) {
        Log::Error("Error in shared_buffer.from_string: %s", e.what());
        return luaL_error(L, "shared_buffer.from_string: %s", e.what());
    }
}

static int HexNibble(lua_State *L, char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    luaL_error(L, "shared_buffer.from_hex: invalid hex character");
    return 0;
}

static int FromHex(lua_State *L) {
    size_t hexLength = 0;
    const char *hex = luaL_checklstring(L, 1, &hexLength);
    if ((hexLength % 2) != 0) {
        return luaL_error(L, "shared_buffer.from_hex: hex string must have even length");
    }

    const size_t size = hexLength / 2;
    try {
        auto buffer = SharedBuffer::Create(size);
        for (size_t i = 0; i < size; ++i) {
            const int high = HexNibble(L, hex[i * 2]);
            const int low = HexNibble(L, hex[i * 2 + 1]);
            buffer->Data()[i] = static_cast<uint8_t>((high << 4) | low);
        }
        PushBuffer(L, std::move(buffer));
        return 1;
    } catch (const std::exception &e) {
        Log::Error("Error in shared_buffer.from_hex: %s", e.what());
        return luaL_error(L, "shared_buffer.from_hex: %s", e.what());
    }
}

static int FromTable(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    std::vector<uint8_t> encoded;
    encoded.reserve(256);
    SerializeLuaValue(L, 1, encoded, kMaxSerializedDepth);
    try {
        PushBuffer(L, SharedBuffer::CreateFrom(encoded.data(), encoded.size()));
        return 1;
    } catch (const std::exception &e) {
        Log::Error("Error in shared_buffer.from_table: %s", e.what());
        return luaL_error(L, "shared_buffer.from_table: %s", e.what());
    }
}

static int GetMaxSize(lua_State *L) {
    lua_pushinteger(L, static_cast<lua_Integer>(SharedBuffer::GetMaxSize()));
    return 1;
}

static int SetMaxSize(lua_State *L) {
    const size_t size = CheckSize(L, 1, "size");
    SharedBuffer::SetMaxSize(size);
    Log::Info("SharedBuffer max size set to %zu bytes", size);
    return 0;
}

static void SetFunction(lua_State *L, const char *name, lua_CFunction function) {
    lua_pushcfunction(L, function);
    lua_setfield(L, -2, name);
}

static void RegisterFactoryTable(lua_State *L) {
    lua_getglobal(L, "tas");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setglobal(L, "tas");
    }

    lua_newtable(L);
    SetFunction(L, "create", Create);
    SetFunction(L, "from_string", FromString);
    SetFunction(L, "from_hex", FromHex);
    SetFunction(L, "from_table", FromTable);
    SetFunction(L, "get_max_size", GetMaxSize);
    SetFunction(L, "set_max_size", SetMaxSize);
    lua_pushvalue(L, -1);
    lua_setglobal(L, "SharedBuffer");
    lua_setfield(L, -2, "shared_buffer");
    lua_pop(L, 1);
}

void LuaApi::RegisterSharedBufferApi(lua_State *state, ScriptContext *context) {
    if (!state || !context) {
        throw std::runtime_error("LuaApi::RegisterSharedBufferApi requires a valid Lua state and ScriptContext");
    }

    tas::lua::LuaStackGuard guard(state);
    tas::lua::LuaUserdataRegistry<SharedBufferHandle>(state, kSharedBufferMt)
        .Method("size", BufferSize)
        .Method("read_u8", ReadU8)
        .Method("write_u8", WriteU8)
        .Method("read_u16", ReadU16)
        .Method("write_u16", WriteU16)
        .Method("read_u32", ReadU32)
        .Method("write_u32", WriteU32)
        .Method("read_i32", ReadI32)
        .Method("write_i32", WriteI32)
        .Method("read_f32", ReadF32)
        .Method("write_f32", WriteF32)
        .Method("read_f64", ReadF64)
        .Method("write_f64", WriteF64)
        .Method("read_string", ReadString)
        .Method("write_string", WriteString)
        .Method("write_string_z", WriteStringZ)
        .Method("fill", Fill)
        .Method("clone", Clone)
        .Method("to_hex", ToHex)
        .Method("to_table", ToTable)
        .MetaMethod("__tostring", ToString);

    RegisterFactoryTable(state);
}
