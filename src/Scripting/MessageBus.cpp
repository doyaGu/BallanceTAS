#include "MessageBus.h"

#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <unordered_map>
#include <algorithm>

#include "Logger.h"
#include "TASEngine.h"

static tas::lua::LuaValue MakeStringEntry(const std::string &value) {
    return tas::lua::LuaValue{value};
}

static std::shared_ptr<tas::lua::LuaValue::Table> MakeMessageTable(const MessageBus::Message &msg) {
    auto table = std::make_shared<tas::lua::LuaValue::Table>();
    auto add = [&](std::string key, tas::lua::LuaValue value) {
        table->entries.push_back({
            tas::lua::LuaValue::Key{std::move(key)},
            std::make_shared<tas::lua::LuaValue>(std::move(value))
        });
    };

    add("sender", MakeStringEntry(msg.senderContext));
    add("target", MakeStringEntry(msg.targetContext));
    add("type", MakeStringEntry(msg.messageType));
    add("data", msg.data.value);
    if (!msg.correlationId.empty()) {
        add("correlation_id", MakeStringEntry(msg.correlationId));
    }
    add("is_request", tas::lua::LuaValue{!msg.isResponse && !msg.correlationId.empty()});
    return table;
}

MessageBus::MessageBus(TASEngine *engine) : m_Engine(engine), m_MessageQueue(m_QueueConfig.maxQueueSize) {
    if (!m_Engine) {
        throw std::runtime_error("MessageBus requires a valid TASEngine instance.");
    }
}

MessageBus::~MessageBus() {
    Shutdown();
}

bool MessageBus::Initialize() {
    if (m_IsInitialized) {
        Log::Warn("MessageBus already initialized.");
        return true;
    }

    Log::Info("Initializing MessageBus...");

    try {
        // Clear any existing messages and handlers
        ClearMessages();
        {
            std::lock_guard<std::mutex> lock(m_HandlersMutex);
            m_Handlers.clear();
        }

        m_IsInitialized = true;
        Log::Info("MessageBus initialized successfully.");
        return true;
    } catch (const std::exception &e) {
        Log::Error("Failed to initialize MessageBus: %s", e.what());
        return false;
    }
}

void MessageBus::Shutdown() {
    if (!m_IsInitialized) return;

    Log::Info("Shutting down MessageBus...");

    try {
        ClearMessages();
        {
            std::lock_guard<std::mutex> lock(m_HandlersMutex);
            m_Handlers.clear();
        }

        m_IsInitialized = false;
        Log::Info("MessageBus shutdown complete.");
    } catch (const std::exception &e) {
        if (m_Engine) {
            Log::Error("Exception during MessageBus shutdown: %s", e.what());
        }
    }
}

bool MessageBus::SendMessage(const std::string &senderContext,
                             const std::string &targetContext,
                             const std::string &messageType,
                             const tas::lua::LuaValue &data,
                             Priority priority) {
    if (messageType.empty()) {
        Log::Warn("[%s] MessageBus: Cannot send message with empty type to '%s'.",
                  senderContext.c_str(), targetContext.c_str());
        return false;
    }

    try {
        Message::SerializedValue payload(data);

        // NEW: Check message size limits (Sprint 2)
        size_t estimatedSize = payload.EstimateSize();

        if (m_QueueConfig.rejectOversized && estimatedSize > m_QueueConfig.maxMessageSize) {
            Log::Error(
                "[%s] MessageBus: Message '%s' to '%s' exceeds size limit (%zu > %zu bytes). "
                "Use tas.shared_buffer and tas.message.send_ref() for large data.",
                senderContext.c_str(), messageType.c_str(), targetContext.c_str(),
                estimatedSize, m_QueueConfig.maxMessageSize
            );
            return false;
        }

        if (m_QueueConfig.warnOnLargeMessage && estimatedSize > m_QueueConfig.warnThreshold) {
            Log::Warn(
                "[%s] MessageBus: Large message detected (%zu bytes) for '%s' to '%s'. "
                "Consider using tas.shared_buffer and tas.message.send_ref() for better performance.",
                senderContext.c_str(), estimatedSize, messageType.c_str(), targetContext.c_str()
            );
        }

        Message msg(senderContext, targetContext, messageType, std::move(payload), priority);
        return EnqueueMessage(std::move(msg));
    } catch (const std::exception &e) {
        Log::Error("[%s] MessageBus: Failed to send message '%s' to '%s': %s",
                   senderContext.c_str(), messageType.c_str(), targetContext.c_str(), e.what());
        return false;
    }
}

bool MessageBus::BroadcastMessage(const std::string &senderContext,
                                  const std::string &messageType,
                                  const tas::lua::LuaValue &data,
                                  Priority priority) {
    return SendMessage(senderContext, "*", messageType, data, priority);
}

void MessageBus::RegisterHandler(const std::string &contextName,
                                 const std::string &messageType,
                                 MessageHandler handler) {
    if (contextName.empty() || messageType.empty()) {
        Log::Warn("MessageBus: Cannot register handler with empty context or message type.");
        return;
    }

    try {
        std::lock_guard<std::mutex> lock(m_HandlersMutex);
        // Create HandlerEntry with empty context (for non-Lua handlers without lifetime tracking)
        HandlerEntry entry(std::weak_ptr<ScriptContext>(), std::move(handler), ++m_HandlerGeneration);
        m_Handlers[contextName][messageType].push_back(std::move(entry));
    } catch (const std::exception &e) {
        Log::Error("[%s] MessageBus: Failed to register handler for '%s': %s",
                   contextName.c_str(), messageType.c_str(), e.what());
    }
}

void MessageBus::RegisterLuaHandler(const std::string &contextName,
                                    std::weak_ptr<ScriptContext> contextPtr,
                                    const std::string &messageType,
                                    tas::lua::LuaFunction luaHandler) {
    if (!luaHandler.IsValid()) {
        Log::Warn("MessageBus: Cannot register invalid Lua handler.");
        return;
    }

    auto luaHandlerRef = std::make_shared<tas::lua::LuaFunction>(std::move(luaHandler));
    const bool trackContextLifetime = !contextPtr.expired();

    // Wrap Lua function in a C++ handler that validates context lifetime
    MessageHandler handler = [luaHandlerRef, contextPtr, trackContextLifetime, contextName, messageType](const Message &msg) {
        // Validate context is still alive before invoking handler
        auto context = contextPtr.lock();
        if (trackContextLifetime && !context) {
            // Context has been destroyed, skip this handler
            Log::Warn("MessageBus: Handler skipped for destroyed context '%s' (type: %s)",
                      contextName.c_str(), messageType.c_str());
            return;
        }

        try {
            auto result = luaHandlerRef->Call(1, 0, [&](lua_State *state) {
                msg.data.value.Push(state);
            });
            if (result.IsError()) {
                Log::Error("MessageBus: Lua handler error (%s, %s): %s",
                           contextName.c_str(), messageType.c_str(), result.GetError().message.c_str());
            }
        } catch (const std::exception &e) {
            Log::Error("MessageBus: Exception in Lua handler (%s, %s): %s",
                       contextName.c_str(), messageType.c_str(), e.what());
        }
    };

    // Store handler with context lifetime tracking
    try {
        std::lock_guard<std::mutex> lock(m_HandlersMutex);
        HandlerEntry entry(contextPtr, std::move(handler), ++m_HandlerGeneration);
        m_Handlers[contextName][messageType].push_back(std::move(entry));
        Log::Info("[%s] Registered handler for message type '%s' (generation: %llu).",
                  contextName.c_str(), messageType.c_str(), m_HandlerGeneration);
    } catch (const std::exception &e) {
        Log::Error("[%s] MessageBus: Failed to register Lua handler for '%s': %s",
                   contextName.c_str(), messageType.c_str(), e.what());
    }
}

void MessageBus::RemoveHandler(const std::string &contextName, const std::string &messageType) {
    std::lock_guard<std::mutex> lock(m_HandlersMutex);
    auto contextIt = m_Handlers.find(contextName);
    if (contextIt != m_Handlers.end()) {
        contextIt->second.erase(messageType);
        if (contextIt->second.empty()) {
            m_Handlers.erase(contextIt);
        }
    }
}

void MessageBus::RemoveAllHandlers(const std::string &contextName) {
    std::lock_guard<std::mutex> lock(m_HandlersMutex);
    m_Handlers.erase(contextName);
}

bool MessageBus::EnqueueMessage(Message message) {
    // Lock-free enqueue with priority
    // Priority is extracted from the message and passed to the queue
    int priority = static_cast<int>(message.priority);

    // Attempt to enqueue (wait-free for producers)
    bool success = m_MessageQueue.Enqueue(std::move(message), priority);

    if (!success) {
        // Queue is full - handle overflow policy
        switch (m_QueueConfig.overflowPolicy) {
        case OverflowPolicy::DropNewest:
            // Message was already rejected by queue
            m_DroppedMessageCount.fetch_add(1, std::memory_order_relaxed);
            Log::Warn("MessageBus: Queue full, dropping newest message (type: %s).",
                      message.messageType.c_str());
            return false;

        case OverflowPolicy::DropOldest:
            // NOTE: Lock-free queue doesn't support DropOldest efficiently
            // Falling back to DropNewest behavior (documented limitation)
            m_DroppedMessageCount.fetch_add(1, std::memory_order_relaxed);
            Log::Warn("MessageBus: Queue full, dropping newest message "
                "(DropOldest not supported with lock-free queue).");
            return false;

        case OverflowPolicy::Block:
            // This is not recommended for game loop - log warning
            Log::Warn("MessageBus: Queue full with Block policy - this may cause frame hitches!");
            m_DroppedMessageCount.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
    }

    return true;
}

bool MessageBus::SendRequestAsync(const std::string &senderContext,
                                  const std::string &targetContext,
                                  const std::string &requestType,
                                  const tas::lua::LuaValue &data,
                                  const std::string &correlationId) {
    if (correlationId.empty()) {
        Log::Error("[%s] MessageBus: Cannot send async request to '%s' with empty correlation ID.",
                   senderContext.c_str(), targetContext.c_str());
        return false;
    }

    try {
        Message::SerializedValue payload(data);
        Message requestMsg(senderContext, targetContext, requestType, std::move(payload),
                           Priority::High, correlationId, false);

        return EnqueueMessage(std::move(requestMsg));
    } catch (const std::exception &e) {
        Log::Error("[%s] MessageBus: Failed to send async request '%s' to '%s': %s",
                   senderContext.c_str(), requestType.c_str(), targetContext.c_str(), e.what());
        return false;
    }
}

tas::lua::LuaValue MessageBus::SendRequest(const std::string &senderContext,
                                           const std::string &targetContext,
                                           const std::string &requestType,
                                           const tas::lua::LuaValue &data,
                                           int timeoutMs) {
    if (timeoutMs <= 0) {
        timeoutMs = m_QueueConfig.requestTimeoutMs;
    }

    // Generate unique correlation ID
    std::string correlationId = GenerateCorrelationId();

    // Send request message with correlation ID
    try {
        Message::SerializedValue payload(data);
        Message requestMsg(senderContext, targetContext, requestType, std::move(payload),
                           Priority::High, correlationId, false);

        if (!EnqueueMessage(std::move(requestMsg))) {
            Log::Error("[%s] MessageBus: Failed to enqueue request '%s' to '%s'.",
                       senderContext.c_str(), requestType.c_str(), targetContext.c_str());
            return tas::lua::LuaValue();
        }
    } catch (const std::exception &e) {
        Log::Error("[%s] MessageBus: Failed to serialize request payload for '%s': %s",
                   senderContext.c_str(), requestType.c_str(), e.what());
        return tas::lua::LuaValue();
    }

    // Wait for response
    auto response = WaitForResponse(correlationId, timeoutMs);
    if (response.has_value()) {
        return response->data.value;
    } else {
        Log::Warn("[%s] MessageBus: Request '%s' to '%s' timeout (correlation: %s)",
                  senderContext.c_str(), requestType.c_str(), targetContext.c_str(),
                  correlationId.c_str());
        return tas::lua::LuaValue();
    }
}

bool MessageBus::SendResponse(const std::string &senderContext,
                              const std::string &targetContext,
                              const std::string &correlationId,
                              const tas::lua::LuaValue &responseData) {
    if (correlationId.empty()) {
        Log::Warn("[%s] MessageBus: Cannot send response to '%s' with empty correlation ID.",
                  senderContext.c_str(), targetContext.c_str());
        return false;
    }

    try {
        Message::SerializedValue payload(responseData);
        Message responseMsg(senderContext, targetContext, "response",
                            std::move(payload), Priority::High, correlationId, true);

        if (!EnqueueMessage(std::move(responseMsg))) {
            return false;
        }
    } catch (const std::exception &e) {
        Log::Error("[%s] MessageBus: Failed to serialize response payload to '%s': %s",
                   senderContext.c_str(), targetContext.c_str(), e.what());
        return false;
    }

    return true;
}

std::string MessageBus::GenerateCorrelationId() {
    uint64_t id = m_NextCorrelationId.fetch_add(1, std::memory_order_relaxed);
    std::ostringstream oss;
    oss << "req_" << std::setw(16) << std::setfill('0') << id;
    return oss.str();
}

std::optional<MessageBus::Message> MessageBus::WaitForResponse(const std::string &correlationId,
                                                               int timeoutMs) {
    std::unique_lock<std::mutex> lock(m_ResponseMutex);

    auto timeout = std::chrono::milliseconds(timeoutMs);
    auto deadline = std::chrono::steady_clock::now() + timeout;

    // Wait for response or timeout
    while (true) {
        // Check if response is available
        auto it = m_PendingResponses.find(correlationId);
        if (it != m_PendingResponses.end()) {
            Message response = std::move(it->second);
            m_PendingResponses.erase(it);
            return response;
        }

        // ACCURACY FIX: Check deadline BEFORE waiting to handle spurious wakeups correctly
        auto now = std::chrono::steady_clock::now();
        if (now >= deadline) {
            // Timeout - no response received
            return std::nullopt;
        }

        // Wait with timeout (may wake spuriously)
        m_ResponseCV.wait_until(lock, deadline);

        // Loop will check response availability again and re-check deadline
    }
}

void MessageBus::NotifyResponse(const Message &response) {
    std::lock_guard<std::mutex> lock(m_ResponseMutex);
    m_PendingResponses[response.correlationId] = response;
    m_ResponseCV.notify_all();
}

void MessageBus::ProcessMessages() {
    if (!m_IsInitialized) return;

    // Drain queue directly (lock-free dequeue by single consumer)
    // Messages are automatically delivered in priority order
    while (auto maybeMessage = m_MessageQueue.Dequeue()) {
        const Message &msg = *maybeMessage;

        try {
            // Check if this is a response message
            if (msg.isResponse) {
                // Notify waiting thread
                NotifyResponse(msg);
            } else {
                // Deliver normal message to handlers
                DeliverMessage(msg);
            }
        } catch (const std::exception &e) {
            Log::Error("[%s] MessageBus: Exception delivering message '%s' to '%s': %s",
                       msg.senderContext.c_str(), msg.messageType.c_str(),
                       msg.targetContext.c_str(), e.what());
        }
    }
}

size_t MessageBus::GetPendingMessageCount() const {
    // Lock-free approximate size (eventually consistent)
    return m_MessageQueue.Size();
}

void MessageBus::ClearMessages() {
    // Drain all messages (lock-free)
    while (m_MessageQueue.Dequeue().has_value()) {
        // Discard messages
    }
}

size_t MessageBus::GetQueueStats(size_t *outDroppedMessages) const {
    // Lock-free statistics retrieval
    if (outDroppedMessages) {
        *outDroppedMessages = m_DroppedMessageCount.load(std::memory_order_relaxed);
    }

    return m_MessageQueue.Size();
}

std::optional<MessageBus::Message> MessageBus::TryGetResponse(const std::string &correlationId) {
    std::lock_guard<std::mutex> lock(m_ResponseMutex);
    auto it = m_PendingResponses.find(correlationId);
    if (it != m_PendingResponses.end()) {
        Message response = std::move(it->second);
        m_PendingResponses.erase(it);
        return response;
    }
    return std::nullopt;
}

void MessageBus::DeliverMessage(const Message &message) {
    std::vector<std::pair<std::string, std::vector<HandlerEntry>>> deliveries;
    {
        std::lock_guard<std::mutex> lock(m_HandlersMutex);

        if (message.targetContext == "*") {
            for (const auto &[contextName, handlerMap] : m_Handlers) {
                if (contextName == message.senderContext) {
                    continue;
                }

                auto typeIt = handlerMap.find(message.messageType);
                if (typeIt != handlerMap.end()) {
                    deliveries.emplace_back(contextName, typeIt->second);
                }
            }
        } else {
            auto ctxIt = m_Handlers.find(message.targetContext);
            if (ctxIt != m_Handlers.end()) {
                auto typeIt = ctxIt->second.find(message.messageType);
                if (typeIt != ctxIt->second.end()) {
                    deliveries.emplace_back(message.targetContext, typeIt->second);
                }
            }
        }
    }

    for (const auto &entry : deliveries) {
        InvokeHandlers(entry.first, entry.second, message);
    }
}

void MessageBus::InvokeHandlers(const std::string &contextName,
                                const std::vector<HandlerEntry> &handlerEntries,
                                const Message &message) {
    for (const auto &entry : handlerEntries) {
        try {
            // Invoke handler (context lifetime is already validated in the handler lambda)
            entry.handler(message);
        } catch (const std::exception &e) {
            Log::Error("MessageBus: Exception in handler (%s, %s): %s",
                       contextName.c_str(), message.messageType.c_str(), e.what());
        }
    }
}

size_t MessageBus::Message::SerializedValue::EstimateSize() const {
    return 0;
}
