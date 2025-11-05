#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <functional>
#include <optional>
#include <condition_variable>
#include <atomic>
#include <any>

#include <sol/sol.hpp>

#include "LockFreeMPSCQueue.h"
#include "SharedBuffer.h"

// Forward declarations
class TASEngine;
class ScriptContext;

/**
 * @class MessageBus
 * @brief Manages asynchronous message passing between script contexts.
 *
 * This class provides a priority-based message queue system with bounded queues,
 * backpressure handling, and request/response patterns for inter-context communication.
 * Messages are delivered asynchronously during the next tick.
 *
 * Lua API:
 *   tas.send_message(target_context, message_type, data, priority)
 *   tas.broadcast_message(message_type, data, priority)
 *   tas.request(target_context, request_type, data, timeout_ms) -> response
 *   tas.on_message(message_type, handler)
 *   tas.remove_message_handler(message_type)
 */
class MessageBus {
public:
    /**
     * @brief Message priority levels (higher value = higher priority).
     */
    enum class Priority {
        Low      = 0,
        Normal   = 5,
        High     = 10,
        Critical = 15
    };

    /**
     * @brief Overflow policy when message queue is full.
     */
    enum class OverflowPolicy {
        DropOldest, // Drop oldest message to make room
        DropNewest, // Drop the new message (reject)
        Block       // Wait until space is available (not recommended)
    };

    /**
     * @brief Configuration for message queue bounds.
     */
    struct QueueConfig {
        size_t maxQueueSize = 1000; // Maximum messages in queue
        OverflowPolicy overflowPolicy = OverflowPolicy::DropOldest;
        bool enablePriority = true;  // Use priority queue (always enabled with lock-free queue)
        int requestTimeoutMs = 5000; // Default timeout for requests

        // Message size limits (Sprint 2)
        size_t maxMessageSize = 1024 * 1024; // 1MB default max message size
        size_t warnThreshold = 100 * 1024;   // 100KB - warn if message exceeds this
        bool warnOnLargeMessage = true;      // Enable warnings for large messages
        bool rejectOversized = true;         // Reject messages exceeding maxMessageSize
    };

    /**
     * @brief Represents a message being sent between contexts.
     */
    struct Message {
        std::string senderContext; // Name of the sending context
        std::string targetContext; // Name of the target context ("*" for broadcast)
        std::string messageType;   // Type of message (used to route to handlers)
        struct SerializedValue {
            enum class Type {
                Nil,
                Boolean,
                Number,
                String,
                Array,
                Table,
                SharedBufferRef // NEW: Reference to shared buffer (zero-copy)
            };

            Type type = Type::Nil;
            std::any data; // For SharedBufferRef: std::shared_ptr<SharedBuffer>

            SerializedValue() = default;

            SerializedValue(Type t, std::any payload) : type(t), data(std::move(payload)) {}

            sol::object ToLuaObject(sol::state_view lua) const;
            static SerializedValue FromLuaObject(sol::object obj);

            // NEW: Factory methods for SharedBuffer
            static SerializedValue FromSharedBuffer(std::shared_ptr<SharedBuffer> buffer);
            std::shared_ptr<SharedBuffer> AsSharedBuffer() const;

            // NEW: Get estimated size in bytes
            size_t EstimateSize() const;
        } data;

        Priority priority;         // Message priority
        std::string correlationId; // Correlation ID for request/response pattern
        bool isResponse;           // True if this is a response message

        Message() : priority(Priority::Normal), isResponse(false) {
        }

        Message(std::string sender, std::string target, std::string type,
                SerializedValue payload,
                Priority prio = Priority::Normal,
                std::string corrId = "", bool isResp = false)
            : senderContext(std::move(sender)),
              targetContext(std::move(target)),
              messageType(std::move(type)),
              data(std::move(payload)),
              priority(prio),
              correlationId(std::move(corrId)),
              isResponse(isResp) {
        }

        // Comparison operator for priority queue (higher priority comes first)
        bool operator<(const Message &other) const {
            return static_cast<int>(priority) < static_cast<int>(other.priority);
        }
    };

    /**
     * @brief Message handler callback type.
     */
    using MessageHandler = std::function<void(const Message &)>;

    explicit MessageBus(TASEngine *engine);
    ~MessageBus();

    // MessageBus is not copyable or movable
    MessageBus(const MessageBus &) = delete;
    MessageBus &operator=(const MessageBus &) = delete;

    /**
     * @brief Initializes the message bus.
     * @return True if initialization was successful.
     */
    bool Initialize();

    /**
     * @brief Shuts down the message bus and clears all messages.
     */
    void Shutdown();

    /**
     * @brief Sends a message to a specific context.
     * @param senderContext Name of the sending context.
     * @param targetContext Name of the target context.
     * @param messageType Type of message.
     * @param data Message payload.
     * @param priority Message priority (default: Normal).
     * @return True if message was queued successfully, false if queue is full and policy rejects.
     */
    bool SendMessage(const std::string &senderContext,
                     const std::string &targetContext,
                     const std::string &messageType,
                     sol::object data,
                     Priority priority = Priority::Normal);

    /**
     * @brief Broadcasts a message to all contexts.
     * @param senderContext Name of the sending context.
     * @param messageType Type of message.
     * @param data Message payload.
     * @param priority Message priority (default: Normal).
     * @return True if message was queued successfully.
     */
    bool BroadcastMessage(const std::string &senderContext,
                          const std::string &messageType,
                          sol::object data,
                          Priority priority = Priority::Normal);

    /**
     * @brief Sends a request and waits for a response (synchronous from caller's perspective).
     * @param senderContext Name of the sending context.
     * @param targetContext Name of the target context.
     * @param requestType Type of request.
     * @param data Request payload.
     * @param timeoutMs Timeout in milliseconds (0 = use default).
     * @return Response data if received within timeout, or nil if timeout.
     */
    sol::object SendRequest(const std::string &senderContext,
                            const std::string &targetContext,
                            const std::string &requestType,
                            sol::object data,
                            int timeoutMs = 0);

    /**
     * @brief Sends a request without waiting (async).
     * @param senderContext Name of the sending context.
     * @param targetContext Name of the target context.
     * @param requestType Type of request.
     * @param data Request payload.
     * @param correlationId Correlation ID for matching responses.
     * @return True if request was queued successfully.
     */
    bool SendRequestAsync(const std::string &senderContext,
                          const std::string &targetContext,
                          const std::string &requestType,
                          sol::object data,
                          const std::string &correlationId);

    /**
     * @brief Sends a response to a request.
     * @param senderContext Name of the responding context.
     * @param targetContext Name of the requesting context.
     * @param correlationId Correlation ID from the original request.
     * @param responseData Response payload.
     * @return True if response was queued successfully.
     */
    bool SendResponse(const std::string &senderContext,
                      const std::string &targetContext,
                      const std::string &correlationId,
                      sol::object responseData);

    /**
     * @brief Registers a message handler for a context.
     * @param contextName Name of the context registering the handler.
     * @param messageType Type of message to handle.
     * @param handler Callback function to invoke when message is received.
     */
    void RegisterHandler(const std::string &contextName,
                         const std::string &messageType,
                         MessageHandler handler);

    /**
     * @brief Registers a Lua message handler for a context.
     * @param contextName Name of the context registering the handler.
     * @param contextPtr Weak pointer to the context (for lifetime tracking).
     * @param messageType Type of message to handle.
     * @param luaHandler Lua function to invoke when message is received.
     */
    void RegisterLuaHandler(const std::string &contextName,
                            std::weak_ptr<ScriptContext> contextPtr,
                            const std::string &messageType,
                            sol::function luaHandler);

    /**
     * @brief Removes all message handlers for a specific message type in a context.
     * @param contextName Name of the context.
     * @param messageType Type of message.
     */
    void RemoveHandler(const std::string &contextName, const std::string &messageType);

    /**
     * @brief Removes all message handlers for a context.
     * @param contextName Name of the context.
     */
    void RemoveAllHandlers(const std::string &contextName);

    /**
     * @brief Processes all pending messages in the queue.
     * This should be called once per tick.
     */
    void ProcessMessages();

    /**
     * @brief Gets the number of pending messages.
     * @return Number of messages in the queue.
     */
    size_t GetPendingMessageCount() const;

    /**
     * @brief Clears all pending messages.
     */
    void ClearMessages();

    /**
     * @brief Gets the current queue configuration.
     * @return Reference to the queue configuration.
     */
    QueueConfig &GetQueueConfig() { return m_QueueConfig; }

    /**
     * @brief Sets the queue configuration.
     * @param config The new queue configuration.
     */
    void SetQueueConfig(const QueueConfig &config) { m_QueueConfig = config; }

    /**
     * @brief Gets queue statistics.
     * @param outDroppedMessages Number of messages dropped due to overflow.
     * @return Current queue size.
     */
    size_t GetQueueStats(size_t *outDroppedMessages = nullptr) const;

    /**
     * @brief Tries to get a response without blocking.
     * @param correlationId The correlation ID to check for.
     * @return Response message if available, or empty optional if not ready.
     */
    std::optional<Message> TryGetResponse(const std::string &correlationId);

private:
    /**
     * @brief Represents a handler entry with context lifetime tracking.
     */
    struct HandlerEntry {
        std::weak_ptr<ScriptContext> context; // Weak pointer to track context lifetime
        MessageHandler handler;               // Handler callback function
        uint64_t generation;                  // Generation counter for versioning

        HandlerEntry() : generation(0) {}

        HandlerEntry(std::weak_ptr<ScriptContext> ctx, MessageHandler h, uint64_t gen)
            : context(std::move(ctx)), handler(std::move(h)), generation(gen) {}
    };

    /**
     * @brief Delivers a message to its target context(s).
     * @param message The message to deliver.
     */
    void DeliverMessage(const Message &message);

    /**
     * @brief Invokes message handlers for a specific context and message type.
     * @param contextName Name of the context.
     * @param handlerEntries Handler entries to invoke (with context lifetime tracking).
     * @param message The message to deliver.
     */
    void InvokeHandlers(const std::string &contextName,
                        const std::vector<HandlerEntry> &handlerEntries,
                        const Message &message);

    using SerializedTable = std::unordered_map<std::string, Message::SerializedValue>;
    using SerializedArray = std::vector<std::pair<size_t, Message::SerializedValue>>;

    static SerializedTable SerializeTable(const sol::table &table);
    static sol::table DeserializeTable(sol::state_view lua, const SerializedTable &data);
    static sol::table DeserializeArray(sol::state_view lua, const SerializedArray &data);

    /**
     * @brief Enqueues a message with overflow handling.
     * @param message The message to enqueue.
     * @return True if message was queued, false if rejected by overflow policy.
     */
    bool EnqueueMessage(Message message);

    /**
     * @brief Generates a unique correlation ID for request/response.
     * @return Unique correlation ID string.
     */
    std::string GenerateCorrelationId();

    /**
     * @brief Waits for a response with the given correlation ID.
     * @param correlationId The correlation ID to wait for.
     * @param timeoutMs Timeout in milliseconds.
     * @return Response message if received, or empty optional if timeout.
     */
    std::optional<Message> WaitForResponse(const std::string &correlationId, int timeoutMs);

    /**
     * @brief Notifies waiting threads that a response has arrived.
     * @param response The response message.
     */
    void NotifyResponse(const Message &response);

    // Core references
    TASEngine *m_Engine;

    // Configuration
    QueueConfig m_QueueConfig;

    // LOCK ORDERING & THREAD SAFETY:
    // ===============================
    // MessageBus uses three mutexes to protect different subsystems:
    // 1. m_QueueMutex: Protects message queue operations
    // 2. m_HandlersMutex: Protects handler registration/removal
    // 3. m_ResponseMutex: Protects request/response tracking
    //
    // LOCK HIERARCHY (always acquire in this order to prevent deadlock):
    //   1. m_HandlersMutex (if needed)
    //   2. m_QueueMutex (if needed)
    //   3. m_ResponseMutex (if needed)
    //
    // IMPORTANT RULES:
    // - Never acquire m_HandlersMutex while holding m_QueueMutex
    // - Never acquire m_QueueMutex while holding m_ResponseMutex
    // - ProcessMessages() holds NO locks while invoking handlers (prevents callback deadlocks)
    // - Handler invocation may call back into MessageBus (e.g., SendResponse)
    //   This is safe because handlers are called without holding any locks
    //
    // EXAMPLE SAFE PATTERN:
    //   ProcessMessages() {
    //     1. Lock m_QueueMutex, copy messages, unlock
    //     2. For each message:
    //        a. Lock m_HandlersMutex, copy handlers, unlock
    //        b. Invoke handlers WITHOUT holding any locks
    //        c. Handlers may safely call SendMessage/SendResponse
    //   }

    // Message queue (lock-free MPSC priority queue)
    // NOTE: LockFreeMPSCQueue replaces both m_MessageQueue and m_SimpleQueue
    // Priority support is always enabled with lock-free queue
    LockFreeMPSCQueue<Message, 15> m_MessageQueue;

    // Statistics
    std::atomic<size_t> m_DroppedMessageCount{0};

    // Message handlers: contextName -> messageType -> HandlerEntries
    mutable std::mutex m_HandlersMutex;
    std::unordered_map<std::string, std::unordered_map<std::string, std::vector<HandlerEntry>>> m_Handlers;
    uint64_t m_HandlerGeneration = 0; // Global generation counter for handler versioning

    // Request/Response tracking
    mutable std::mutex m_ResponseMutex;
    std::condition_variable m_ResponseCV;
    std::unordered_map<std::string, Message> m_PendingResponses;
    std::atomic<uint64_t> m_NextCorrelationId{1};

    // Initialization state
    bool m_IsInitialized = false;
};
