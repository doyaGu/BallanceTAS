#pragma once

#include <atomic>
#include <memory>
#include <cstdint>
#include <stdexcept>
#include <sol/sol.hpp>
#include <yyjson.h>

/**
 * @brief Reference-counted shared buffer for zero-copy message passing
 *
 * SharedBuffer provides a thread-safe, reference-counted memory buffer that can be
 * shared across script contexts without copying. This is critical for performance
 * when passing large data structures (e.g., arrays, tables, binary data) between contexts.
 *
 * Design Goals:
 * - Zero-copy sharing between contexts
 * - Thread-safe reference counting
 * - Automatic memory management
 * - Support for arbitrary data types
 * - Minimal overhead for small buffers
 *
 * Use Cases:
 * - Sharing large Lua tables between contexts
 * - Passing binary data (e.g., screenshots, sensor data)
 * - Avoiding serialization overhead for structured data
 *
 * Thread Safety:
 * - Reference counting uses atomic operations
 * - Multiple threads can hold references concurrently
 * - Last reference triggers deallocation
 *
 * Example Usage:
 * @code
 *   // Context A: Create and share
 *   auto buffer = SharedBuffer::Create(1024);
 *   std::memcpy(buffer->Data(), myData, 1024);
 *   SendMessage("contextB", "data", buffer);
 *
 *   // Context B: Receive and use (zero-copy)
 *   auto buffer = message.GetSharedBuffer();
 *   ProcessData(buffer->Data(), buffer->Size());
 * @endcode
 */
class SharedBuffer {
public:
    /**
     * @brief Creates a new shared buffer with specified size
     * @param size Size in bytes
     * @return Shared pointer to buffer
     */
    static std::shared_ptr<SharedBuffer> Create(size_t size) {
        if (size == 0) {
            throw std::invalid_argument("SharedBuffer size must be > 0");
        }
        if (size > GetMaxSize()) {
            throw std::invalid_argument("SharedBuffer size exceeds maximum");
        }
        return std::shared_ptr<SharedBuffer>(new SharedBuffer(size));
    }

    /**
     * @brief Creates a shared buffer from existing data (copies data)
     * @param data Source data pointer
     * @param size Size in bytes
     * @return Shared pointer to buffer
     */
    static std::shared_ptr<SharedBuffer> CreateFrom(const void* data, size_t size) {
        auto buffer = Create(size);
        std::memcpy(buffer->Data(), data, size);
        return buffer;
    }

    /**
     * @brief Creates a typed shared buffer (C++ objects)
     * @tparam T Type of object to store
     * @param value Value to copy into buffer
     * @return Shared pointer to buffer
     */
    template <typename T>
    static std::shared_ptr<SharedBuffer> CreateTyped(const T& value) {
        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
        auto buffer = Create(sizeof(T));
        std::memcpy(buffer->Data(), &value, sizeof(T));
        return buffer;
    }

    /**
     * @brief Gets mutable pointer to buffer data
     * @return Pointer to data (size() bytes)
     *
     * WARNING: Modifying shared buffer data is NOT thread-safe!
     * Only modify before sharing, or use external synchronization.
     */
    uint8_t* Data() { return m_Data; }

    /**
     * @brief Gets const pointer to buffer data
     * @return Const pointer to data (size() bytes)
     */
    const uint8_t* Data() const { return m_Data; }

    /**
     * @brief Gets buffer size in bytes
     * @return Size in bytes
     */
    size_t Size() const { return m_Size; }

    /**
     * @brief Gets buffer as typed pointer (C++ objects)
     * @tparam T Type to cast to
     * @return Typed pointer
     *
     * WARNING: No type checking! Ensure correct type at runtime.
     */
    template <typename T>
    T* As() {
        if (m_Size < sizeof(T)) {
            throw std::runtime_error("SharedBuffer too small for type T");
        }
        return reinterpret_cast<T*>(m_Data);
    }

    /**
     * @brief Gets buffer as const typed pointer
     */
    template <typename T>
    const T* As() const {
        if (m_Size < sizeof(T)) {
            throw std::runtime_error("SharedBuffer too small for type T");
        }
        return reinterpret_cast<const T*>(m_Data);
    }

    /**
     * @brief Copies data into buffer
     * @param data Source data
     * @param size Size in bytes
     * @param offset Offset in buffer to write to
     */
    void Write(const void* data, size_t size, size_t offset = 0) {
        if (offset + size > m_Size) {
            throw std::out_of_range("SharedBuffer write exceeds buffer size");
        }
        std::memcpy(m_Data + offset, data, size);
    }

    /**
     * @brief Copies data from buffer
     * @param data Destination buffer
     * @param size Size in bytes
     * @param offset Offset in buffer to read from
     */
    void Read(void* data, size_t size, size_t offset = 0) const {
        if (offset + size > m_Size) {
            throw std::out_of_range("SharedBuffer read exceeds buffer size");
        }
        std::memcpy(data, m_Data + offset, size);
    }

    /**
     * @brief Creates a deep copy of this buffer
     * @return New shared buffer with copied data
     */
    std::shared_ptr<SharedBuffer> Clone() const {
        return CreateFrom(m_Data, m_Size);
    }

    /**
     * @brief Maximum buffer size (1MB by default, configurable)
     */
    static constexpr size_t MaxSize = 1024 * 1024;  // 1MB

    /**
     * @brief Sets maximum buffer size (call before creating any buffers)
     * @param maxSize New maximum size in bytes
     */
    static void SetMaxSize(size_t maxSize) {
        s_MaxSize = maxSize;
    }

    /**
     * @brief Gets current maximum buffer size
     * @return Maximum size in bytes
     */
    static size_t GetMaxSize() {
        return s_MaxSize;
    }

    ~SharedBuffer() {
        delete[] m_Data;
    }

private:
    explicit SharedBuffer(size_t size)
        : m_Size(size)
        , m_Data(new uint8_t[size])
    {
        // Zero-initialize for safety
        std::memset(m_Data, 0, size);
    }

    // Non-copyable (use Clone() for deep copy)
    SharedBuffer(const SharedBuffer&) = delete;
    SharedBuffer& operator=(const SharedBuffer&) = delete;

    size_t m_Size;
    uint8_t* m_Data;

    // Global max size setting
    static inline size_t s_MaxSize = MaxSize;
};

/**
 * @brief Helper for creating SharedBuffer from Lua tables (JSON serialization)
 *
 * Provides zero-copy sharing of Lua tables between contexts by serializing to JSON.
 * Uses yyjson for high-performance JSON serialization/deserialization.
 *
 * Example usage:
 * @code
 *   // Context A: Serialize Lua table to SharedBuffer
 *   local data = {health = 100, position = {x = 10, y = 20}}
 *   local buffer = tas.shared_buffer.from_table(data)
 *   tas.send_message("contextB", "player_state", buffer)
 *
 *   // Context B: Deserialize SharedBuffer to Lua table
 *   local buffer = message:get_shared_buffer()
 *   local data = buffer:to_table()
 *   print(data.health)  -- 100
 * @endcode
 */
class SharedBufferSerializer {
public:
    /**
     * @brief Serializes a Lua table to SharedBuffer using JSON
     * @param table Lua table to serialize
     * @return SharedBuffer containing JSON data
     * @throws std::runtime_error if serialization fails
     */
    static std::shared_ptr<SharedBuffer> FromLuaTable(sol::table table) {
        // Convert Lua table to yyjson document
        yyjson_mut_doc* doc = yyjson_mut_doc_new(nullptr);
        yyjson_mut_val* root = LuaToYYJSON(doc, table);
        yyjson_mut_doc_set_root(doc, root);

        // Serialize to JSON string
        size_t json_len;
        char* json_str = yyjson_mut_write(doc, 0, &json_len);
        if (!json_str) {
            yyjson_mut_doc_free(doc);
            throw std::runtime_error("Failed to serialize Lua table to JSON");
        }

        // Create SharedBuffer from JSON string
        auto buffer = SharedBuffer::CreateFrom(json_str, json_len);

        // Cleanup
        free(json_str);
        yyjson_mut_doc_free(doc);

        return buffer;
    }

    /**
     * @brief Deserializes a SharedBuffer to Lua table using JSON
     * @param lua Lua state
     * @param buffer SharedBuffer containing JSON data
     * @return Lua table
     * @throws std::runtime_error if deserialization fails
     */
    static sol::table ToLuaTable(sol::state_view lua, std::shared_ptr<SharedBuffer> buffer) {
        if (!buffer) {
            throw std::invalid_argument("SharedBuffer cannot be null");
        }

        // Parse JSON from buffer
        yyjson_doc* doc = yyjson_read((const char*)buffer->Data(), buffer->Size(), 0);
        if (!doc) {
            throw std::runtime_error("Failed to parse JSON from SharedBuffer");
        }

        yyjson_val* root = yyjson_doc_get_root(doc);
        if (!root) {
            yyjson_doc_free(doc);
            throw std::runtime_error("JSON document has no root");
        }

        // Convert to Lua table
        sol::table result = YYJSONToLua(lua, root);

        // Cleanup
        yyjson_doc_free(doc);

        return result;
    }

private:
    /**
     * @brief Converts Lua object to yyjson value
     */
    static yyjson_mut_val* LuaToYYJSON(yyjson_mut_doc* doc, sol::object obj) {
        sol::type type = obj.get_type();

        switch (type) {
            case sol::type::nil:
                return yyjson_mut_null(doc);

            case sol::type::boolean:
                return yyjson_mut_bool(doc, obj.as<bool>());

            case sol::type::number:
                // Check if it's an integer or floating point
                if (obj.is<int>()) {
                    return yyjson_mut_int(doc, obj.as<int64_t>());
                } else {
                    return yyjson_mut_real(doc, obj.as<double>());
                }

            case sol::type::string:
                return yyjson_mut_strcpy(doc, obj.as<std::string>().c_str());

            case sol::type::table: {
                sol::table tbl = obj;

                // Check if it's an array (sequential integer keys starting from 1)
                bool is_array = true;
                size_t expected_index = 1;
                size_t count = 0;

                for (const auto& pair : tbl) {
                    if (!pair.first.is<int>() || pair.first.as<int>() != expected_index) {
                        is_array = false;
                        break;
                    }
                    expected_index++;
                    count++;
                }

                if (is_array && count > 0) {
                    // Array
                    yyjson_mut_val* arr = yyjson_mut_arr(doc);
                    for (size_t i = 1; i <= count; ++i) {
                        sol::object elem = tbl[i];
                        yyjson_mut_arr_append(arr, LuaToYYJSON(doc, elem));
                    }
                    return arr;
                } else {
                    // Object
                    yyjson_mut_val* obj_val = yyjson_mut_obj(doc);
                    for (const auto& pair : tbl) {
                        std::string key;
                        if (pair.first.is<std::string>()) {
                            key = pair.first.as<std::string>();
                        } else if (pair.first.is<int>()) {
                            key = std::to_string(pair.first.as<int>());
                        } else {
                            continue;  // Skip non-string/int keys
                        }

                        yyjson_mut_val* val = LuaToYYJSON(doc, pair.second);
                        yyjson_mut_obj_add(obj_val, yyjson_mut_strcpy(doc, key.c_str()), val);
                    }
                    return obj_val;
                }
            }

            default:
                // Unsupported types (function, userdata, thread) -> null
                return yyjson_mut_null(doc);
        }
    }

    /**
     * @brief Converts yyjson value to Lua object
     */
    static sol::object YYJSONToLua(sol::state_view lua, yyjson_val* val) {
        yyjson_type type = yyjson_get_type(val);

        switch (type) {
            case YYJSON_TYPE_NULL:
                return sol::nil;

            case YYJSON_TYPE_BOOL:
                return sol::make_object(lua, yyjson_get_bool(val));

            case YYJSON_TYPE_NUM: {
                if (yyjson_is_int(val)) {
                    return sol::make_object(lua, yyjson_get_int(val));
                } else {
                    return sol::make_object(lua, yyjson_get_real(val));
                }
            }

            case YYJSON_TYPE_STR:
                return sol::make_object(lua, std::string(yyjson_get_str(val)));

            case YYJSON_TYPE_ARR: {
                sol::table arr = lua.create_table();
                size_t idx = 1;
                size_t max_idx;
                yyjson_val* elem;
                yyjson_arr_foreach(val, idx, max_idx, elem) {
                    arr[idx] = YYJSONToLua(lua, elem);
                }
                return arr;
            }

            case YYJSON_TYPE_OBJ: {
                sol::table obj = lua.create_table();
                size_t idx, max;
                yyjson_val *key, *obj_val;
                yyjson_obj_foreach(val, idx, max, key, obj_val) {
                    std::string key_str = yyjson_get_str(key);
                    obj[key_str] = YYJSONToLua(lua, obj_val);
                }
                return obj;
            }

            default:
                return sol::nil;
        }
    }
};
