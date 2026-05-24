#pragma once

#include <atomic>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>

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
    static std::shared_ptr<SharedBuffer> CreateFrom(const void *data, size_t size) {
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
    static std::shared_ptr<SharedBuffer> CreateTyped(const T &value) {
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
    uint8_t *Data() { return m_Data; }

    /**
     * @brief Gets const pointer to buffer data
     * @return Const pointer to data (size() bytes)
     */
    const uint8_t *Data() const { return m_Data; }

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
    T *As() {
        if (m_Size < sizeof(T)) {
            throw std::runtime_error("SharedBuffer too small for type T");
        }
        return reinterpret_cast<T *>(m_Data);
    }

    /**
     * @brief Gets buffer as const typed pointer
     */
    template <typename T>
    const T *As() const {
        if (m_Size < sizeof(T)) {
            throw std::runtime_error("SharedBuffer too small for type T");
        }
        return reinterpret_cast<const T *>(m_Data);
    }

    /**
     * @brief Copies data into buffer
     * @param data Source data
     * @param size Size in bytes
     * @param offset Offset in buffer to write to
     */
    void Write(const void *data, size_t size, size_t offset = 0) {
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
    void Read(void *data, size_t size, size_t offset = 0) const {
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
    static constexpr size_t MaxSize = 1024 * 1024; // 1MB

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

    // Non-copyable (use Clone() for deep copy)
    SharedBuffer(const SharedBuffer &) = delete;
    SharedBuffer &operator=(const SharedBuffer &) = delete;

private:
    explicit SharedBuffer(size_t size) : m_Size(size), m_Data(new uint8_t[size]) {
        // Zero-initialize for safety
        std::memset(m_Data, 0, size);
    }

    size_t m_Size;
    uint8_t *m_Data;

    // Global max size setting
    static inline size_t s_MaxSize = MaxSize;
};

