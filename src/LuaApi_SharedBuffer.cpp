#include "LuaApi.h"

#include "Logger.h"
#include "TASEngine.h"
#include "ScriptContext.h"
#include "SharedBuffer.h"

// ===================================================================
//  SharedBuffer Lua API Registration
// ===================================================================

void LuaApi::RegisterSharedBufferApi(sol::table &tas, ScriptContext *context) {
    if (!context) {
        throw std::runtime_error("LuaApi::RegisterSharedBufferApi requires a valid ScriptContext");
    }

    std::string logPrefix = "[" + context->GetName() + "]";
    sol::state &lua = context->GetLuaState();

    // Register SharedBuffer userdata type
    sol::usertype<SharedBuffer> buffer_type = lua.new_usertype<SharedBuffer>(
        "SharedBuffer",
        sol::no_constructor // Use factory functions instead
    );

    // --- Properties ---

    // buffer:size() - Get buffer size in bytes
    buffer_type["size"] = &SharedBuffer::Size;

    // --- Read/Write Methods ---

    // buffer:read_u8(offset) - Read unsigned 8-bit integer
    buffer_type["read_u8"] = [](const SharedBuffer &self, size_t offset) -> uint8_t {
        if (offset >= self.Size()) {
            throw sol::error("SharedBuffer read_u8: offset out of bounds");
        }
        return self.Data()[offset];
    };

    // buffer:write_u8(offset, value) - Write unsigned 8-bit integer
    buffer_type["write_u8"] = [](SharedBuffer &self, size_t offset, uint8_t value) {
        if (offset >= self.Size()) {
            throw sol::error("SharedBuffer write_u8: offset out of bounds");
        }
        self.Data()[offset] = value;
    };

    // buffer:read_u16(offset) - Read unsigned 16-bit integer (little-endian)
    buffer_type["read_u16"] = [](const SharedBuffer &self, size_t offset) -> uint16_t {
        if (offset + sizeof(uint16_t) > self.Size()) {
            throw sol::error("SharedBuffer read_u16: offset out of bounds");
        }
        uint16_t value;
        std::memcpy(&value, self.Data() + offset, sizeof(uint16_t));
        return value;
    };

    // buffer:write_u16(offset, value) - Write unsigned 16-bit integer (little-endian)
    buffer_type["write_u16"] = [](SharedBuffer &self, size_t offset, uint16_t value) {
        if (offset + sizeof(uint16_t) > self.Size()) {
            throw sol::error("SharedBuffer write_u16: offset out of bounds");
        }
        std::memcpy(self.Data() + offset, &value, sizeof(uint16_t));
    };

    // buffer:read_u32(offset) - Read unsigned 32-bit integer (little-endian)
    buffer_type["read_u32"] = [](const SharedBuffer &self, size_t offset) -> uint32_t {
        if (offset + sizeof(uint32_t) > self.Size()) {
            throw sol::error("SharedBuffer read_u32: offset out of bounds");
        }
        uint32_t value;
        std::memcpy(&value, self.Data() + offset, sizeof(uint32_t));
        return value;
    };

    // buffer:write_u32(offset, value) - Write unsigned 32-bit integer (little-endian)
    buffer_type["write_u32"] = [](SharedBuffer &self, size_t offset, uint32_t value) {
        if (offset + sizeof(uint32_t) > self.Size()) {
            throw sol::error("SharedBuffer write_u32: offset out of bounds");
        }
        std::memcpy(self.Data() + offset, &value, sizeof(uint32_t));
    };

    // buffer:read_i32(offset) - Read signed 32-bit integer (little-endian)
    buffer_type["read_i32"] = [](const SharedBuffer &self, size_t offset) -> int32_t {
        if (offset + sizeof(int32_t) > self.Size()) {
            throw sol::error("SharedBuffer read_i32: offset out of bounds");
        }
        int32_t value;
        std::memcpy(&value, self.Data() + offset, sizeof(int32_t));
        return value;
    };

    // buffer:write_i32(offset, value) - Write signed 32-bit integer (little-endian)
    buffer_type["write_i32"] = [](SharedBuffer &self, size_t offset, int32_t value) {
        if (offset + sizeof(int32_t) > self.Size()) {
            throw sol::error("SharedBuffer write_i32: offset out of bounds");
        }
        std::memcpy(self.Data() + offset, &value, sizeof(int32_t));
    };

    // buffer:read_f32(offset) - Read 32-bit float (little-endian)
    buffer_type["read_f32"] = [](const SharedBuffer &self, size_t offset) -> float {
        if (offset + sizeof(float) > self.Size()) {
            throw sol::error("SharedBuffer read_f32: offset out of bounds");
        }
        float value;
        std::memcpy(&value, self.Data() + offset, sizeof(float));
        return value;
    };

    // buffer:write_f32(offset, value) - Write 32-bit float (little-endian)
    buffer_type["write_f32"] = [](SharedBuffer &self, size_t offset, float value) {
        if (offset + sizeof(float) > self.Size()) {
            throw sol::error("SharedBuffer write_f32: offset out of bounds");
        }
        std::memcpy(self.Data() + offset, &value, sizeof(float));
    };

    // buffer:read_f64(offset) - Read 64-bit double (little-endian)
    buffer_type["read_f64"] = [](const SharedBuffer &self, size_t offset) -> double {
        if (offset + sizeof(double) > self.Size()) {
            throw sol::error("SharedBuffer read_f64: offset out of bounds");
        }
        double value;
        std::memcpy(&value, self.Data() + offset, sizeof(double));
        return value;
    };

    // buffer:write_f64(offset, value) - Write 64-bit double (little-endian)
    buffer_type["write_f64"] = [](SharedBuffer &self, size_t offset, double value) {
        if (offset + sizeof(double) > self.Size()) {
            throw sol::error("SharedBuffer write_f64: offset out of bounds");
        }
        std::memcpy(self.Data() + offset, &value, sizeof(double));
    };

    // buffer:read_string(offset, length) - Read string (null-terminated if length not specified)
    buffer_type["read_string"] = [](const SharedBuffer &self, size_t offset, sol::optional<size_t> length) -> std::string {
        if (offset >= self.Size()) {
            throw sol::error("SharedBuffer read_string: offset out of bounds");
        }

        if (length.has_value()) {
            // Fixed-length read
            size_t len = length.value();
            if (offset + len > self.Size()) {
                throw sol::error("SharedBuffer read_string: length exceeds buffer size");
            }
            return std::string(reinterpret_cast<const char *>(self.Data() + offset), len);
        } else {
            // Null-terminated read
            const char *str = reinterpret_cast<const char *>(self.Data() + offset);
            size_t remaining = self.Size() - offset;

            // Find null terminator or end of buffer
            size_t len = 0;
            while (len < remaining && str[len] != '\0') {
                ++len;
            }

            return std::string(str, len);
        }
    };

    // buffer:write_string(offset, str) - Write string (without null terminator)
    buffer_type["write_string"] = [](SharedBuffer &self, size_t offset, const std::string &str) {
        if (offset + str.size() > self.Size()) {
            throw sol::error("SharedBuffer write_string: string exceeds buffer size");
        }
        std::memcpy(self.Data() + offset, str.data(), str.size());
    };

    // buffer:write_string_z(offset, str) - Write null-terminated string
    buffer_type["write_string_z"] = [](SharedBuffer &self, size_t offset, const std::string &str) {
        if (offset + str.size() + 1 > self.Size()) {
            throw sol::error("SharedBuffer write_string_z: string exceeds buffer size");
        }
        std::memcpy(self.Data() + offset, str.data(), str.size());
        self.Data()[offset + str.size()] = '\0';
    };

    // buffer:fill(value, offset, length) - Fill buffer with byte value
    buffer_type["fill"] = [](SharedBuffer &self, uint8_t value, sol::optional<size_t> offset, sol::optional<size_t> length) {
        size_t start = offset.value_or(0);
        size_t len = length.value_or(self.Size() - start);

        if (start >= self.Size()) {
            throw sol::error("SharedBuffer fill: offset out of bounds");
        }
        if (start + len > self.Size()) {
            throw sol::error("SharedBuffer fill: length exceeds buffer size");
        }

        std::memset(self.Data() + start, value, len);
    };

    // buffer:clone() - Create a deep copy
    buffer_type["clone"] = &SharedBuffer::Clone;

    // buffer:to_hex([offset], [length]) - Convert to hex string
    buffer_type["to_hex"] = [](const SharedBuffer &self, sol::optional<size_t> offset, sol::optional<size_t> length) -> std::string {
        size_t start = offset.value_or(0);
        size_t len = length.value_or(self.Size() - start);

        if (start >= self.Size()) {
            throw sol::error("SharedBuffer to_hex: offset out of bounds");
        }
        if (start + len > self.Size()) {
            throw sol::error("SharedBuffer to_hex: length exceeds buffer size");
        }

        std::string hex;
        hex.reserve(len * 2);
        static const char hex_chars[] = "0123456789abcdef";

        for (size_t i = 0; i < len; ++i) {
            uint8_t byte = self.Data()[start + i];
            hex.push_back(hex_chars[(byte >> 4) & 0x0F]);
            hex.push_back(hex_chars[byte & 0x0F]);
        }

        return hex;
    };

    // --- Create nested 'shared_buffer' table for factory functions ---
    sol::table sb = tas["shared_buffer"] = tas.create();

    // tas.shared_buffer.create(size) - Create new buffer
    sb["create"] = [logPrefix](size_t size) -> std::shared_ptr<SharedBuffer> {
        try {
            return SharedBuffer::Create(size);
        } catch (const std::exception &e) {
            Log::Error("%s Error in shared_buffer.create: %s", logPrefix.c_str(), e.what());
            throw;
        }
    };

    // tas.shared_buffer.from_string(str) - Create buffer from string
    sb["from_string"] = [logPrefix](const std::string &str) -> std::shared_ptr<SharedBuffer> {
        try {
            return SharedBuffer::CreateFrom(str.data(), str.size());
        } catch (const std::exception &e) {
            Log::Error("%s Error in shared_buffer.from_string: %s", logPrefix.c_str(), e.what());
            throw;
        }
    };

    // tas.shared_buffer.from_hex(hex_str) - Create buffer from hex string
    sb["from_hex"] = [logPrefix](const std::string &hex_str) -> std::shared_ptr<SharedBuffer> {
        try {
            if (hex_str.size() % 2 != 0) {
                throw sol::error("shared_buffer.from_hex: hex string must have even length");
            }

            size_t size = hex_str.size() / 2;
            auto buffer = SharedBuffer::Create(size);

            for (size_t i = 0; i < size; ++i) {
                char high = hex_str[i * 2];
                char low = hex_str[i * 2 + 1];

                auto hex_to_nibble = [](char c) -> uint8_t {
                    if (c >= '0' && c <= '9') return c - '0';
                    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                    throw sol::error("shared_buffer.from_hex: invalid hex character");
                };

                uint8_t byte = (hex_to_nibble(high) << 4) | hex_to_nibble(low);
                buffer->Data()[i] = byte;
            }

            return buffer;
        } catch (const std::exception &e) {
            Log::Error("%s Error in shared_buffer.from_hex: %s", logPrefix.c_str(), e.what());
            throw;
        }
    };

    // tas.shared_buffer.get_max_size() - Get maximum buffer size
    sb["get_max_size"] = []() -> size_t {
        return SharedBuffer::GetMaxSize();
    };

    // tas.shared_buffer.set_max_size(size) - Set maximum buffer size
    sb["set_max_size"] = [logPrefix](size_t size) {
        try {
            SharedBuffer::SetMaxSize(size);
            Log::Info("%s SharedBuffer max size set to %zu bytes", logPrefix.c_str(), size);
        } catch (const std::exception &e) {
            Log::Error("%s Error in shared_buffer.set_max_size: %s", logPrefix.c_str(), e.what());
            throw;
        }
    };
}
