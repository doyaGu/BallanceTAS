#include "LuaApi.h"

#include "Logger.h"
#include <stdexcept>

#include "TASEngine.h"
#include "ScriptContext.h"
#include "ScriptContextManager.h"
#include "SharedDataManager.h"
#include "MessageBus.h"
#include "LuaScheduler.h"

#include <atomic>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>

// ===================================================================
//  Context Communication API Registration
// ===================================================================

void LuaApi::RegisterContextCommunicationApi(sol::table &tas, ScriptContext *context) {
    if (!context) {
        throw std::runtime_error("LuaApi::RegisterContextCommunicationApi requires a valid ScriptContext");
    }

    auto *contextManager = context->GetScriptContextManager();
    if (!contextManager) {
        Log::Warn("[%s] ScriptContextManager not available for context communication APIs.", context->GetName().c_str());
        return;
    }

    auto *sharedData = contextManager->GetSharedData();
    auto *messageBus = contextManager->GetMessageBus();

    if (!sharedData || !messageBus) {
        Log::Warn("[%s] SharedDataManager or MessageBus not available.", context->GetName().c_str());
        return;
    }

    std::string contextName = context->GetName();

    // ===================================================================
    // Shared Data API (tas.shared.*)
    // ===================================================================

    sol::table shared = tas["shared"] = tas.create();

    // tas.shared.set(key, value, options?) - Set shared data
    shared["set"] = sol::overload(
        [sharedData](const std::string &key, sol::object value) -> bool {
            if (!sharedData) {
                throw sol::error("shared.set: SharedDataManager not available");
            }
            if (key.empty()) {
                throw sol::error("shared.set: key cannot be empty");
            }
            SharedDataManager::SetOptions options;
            return sharedData->Set(key, value, options);
        },
        [sharedData](const std::string &key, sol::object value, sol::table options) -> bool {
            if (!sharedData) {
                throw sol::error("shared.set: SharedDataManager not available");
            }
            if (key.empty()) {
                throw sol::error("shared.set: key cannot be empty");
            }
            SharedDataManager::SetOptions setOpts;
            if (options["ttl"].valid()) {
                setOpts.ttl_ms = options["ttl"].get<int>();
            }
            return sharedData->Set(key, value, setOpts);
        }
    );

    // tas.shared.get(key, default?) - Get shared data
    shared["get"] = sol::overload(
        [sharedData, context](const std::string &key) -> sol::object {
            if (!sharedData || !context) {
                return sol::nil;
            }
            if (key.empty()) {
                return sol::nil;
            }
            sol::state_view lua = context->GetLuaState();
            return sharedData->Get(lua, key, sol::nil);
        },
        [sharedData, context](const std::string &key, sol::object defaultValue) -> sol::object {
            if (!sharedData || !context) {
                return defaultValue;
            }
            if (key.empty()) {
                return defaultValue;
            }
            sol::state_view lua = context->GetLuaState();
            return sharedData->Get(lua, key, defaultValue);
        }
    );

    // tas.shared.delete(key) - Delete shared data
    shared["delete"] = [sharedData](const std::string &key) -> bool {
        if (!sharedData) {
            return false;
        }
        if (key.empty()) {
            return false;
        }
        return sharedData->Remove(key);
    };

    // tas.shared.exists(key) - Check if key exists
    shared["exists"] = [sharedData](const std::string &key) -> bool {
        if (!sharedData) {
            return false;
        }
        if (key.empty()) {
            return false;
        }
        return sharedData->Has(key);
    };

    // tas.shared.watch(key, callback) - Watch for changes to a key
    shared["watch"] = [sharedData, contextManager, contextName](const std::string &key, sol::function callback) {
        if (!sharedData || !contextManager) {
            throw sol::error("shared.watch: SharedDataManager or ContextManager not available");
        }
        if (key.empty()) {
            throw sol::error("shared.watch: key cannot be empty");
        }
        if (!callback.valid()) {
            throw sol::error("shared.watch: callback must be a valid function");
        }
        // Get shared_ptr to context for lifetime tracking
        auto contextPtr = contextManager->GetContext(contextName);
        if (!contextPtr) {
            throw sol::error("shared.watch: context no longer exists");
        }
        sharedData->Watch(contextName, contextPtr, key, callback);
    };

    // tas.shared.unwatch(key) - Stop watching a key
    shared["unwatch"] = [sharedData, contextName](const std::string &key) {
        if (!sharedData) {
            return;
        }
        if (key.empty()) {
            return;
        }
        sharedData->Unwatch(contextName, key);
    };

    // tas.shared.clear() - Clear all shared data
    shared["clear"] = [sharedData]() {
        if (!sharedData) {
            return;
        }
        sharedData->Clear();
    };

    // tas.shared.keys() - Get all keys
    shared["keys"] = [sharedData, context]() -> sol::table {
        if (!sharedData || !context) {
            throw sol::error("shared.keys: SharedDataManager or Context not available");
        }
        sol::state_view lua = context->GetLuaState();
        sol::table keysTable = lua.create_table();
        auto keys = sharedData->GetKeys();
        for (size_t i = 0; i < keys.size(); ++i) {
            keysTable[i + 1] = keys[i];
        }
        return keysTable;
    };

    // tas.shared.size() - Get number of entries
    shared["size"] = [sharedData]() -> size_t {
        if (!sharedData) {
            return 0;
        }
        return sharedData->GetSize();
    };

    // ===================================================================
    // Message API (tas.message.*)
    // ===================================================================

    sol::table message = tas["message"] = tas.create();

    // Helper to convert priority string to enum (case-insensitive)
    auto getPriority = [](sol::optional<std::string> priorityStr) -> MessageBus::Priority {
        if (!priorityStr.has_value()) {
            return MessageBus::Priority::Normal;
        }
        std::string p = priorityStr.value();
        // Convert to lowercase for case-insensitive comparison
        std::transform(p.begin(), p.end(), p.begin(), ::tolower);
        if (p == "low") return MessageBus::Priority::Low;
        if (p == "high") return MessageBus::Priority::High;
        if (p == "critical") return MessageBus::Priority::Critical;
        return MessageBus::Priority::Normal;
    };

    // tas.message.send(target, type, data, priority?) - Send message to specific context
    message["send"] = sol::overload(
        [messageBus, contextName](const std::string &target, const std::string &type, sol::object data) -> bool {
            if (!messageBus) {
                throw sol::error("message.send: MessageBus not available");
            }
            if (target.empty() || type.empty()) {
                throw sol::error("message.send: target and type cannot be empty");
            }
            return messageBus->SendMessage(contextName, target, type, data);
        },
        [messageBus, contextName, getPriority](const std::string &target, const std::string &type,
                                               sol::object data, std::string priority) -> bool {
            if (!messageBus) {
                throw sol::error("message.send: MessageBus not available");
            }
            if (target.empty() || type.empty()) {
                throw sol::error("message.send: target and type cannot be empty");
            }
            return messageBus->SendMessage(contextName, target, type, data, getPriority(priority));
        }
    );

    // tas.message.broadcast(type, data, priority?) - Broadcast message to all contexts
    message["broadcast"] = sol::overload(
        [messageBus, contextName](const std::string &type, sol::object data) -> bool {
            if (!messageBus) {
                throw sol::error("message.broadcast: MessageBus not available");
            }
            if (type.empty()) {
                throw sol::error("message.broadcast: type cannot be empty");
            }
            return messageBus->BroadcastMessage(contextName, type, data);
        },
        [messageBus, contextName, getPriority](const std::string &type, sol::object data,
                                               std::string priority) -> bool {
            if (!messageBus) {
                throw sol::error("message.broadcast: MessageBus not available");
            }
            if (type.empty()) {
                throw sol::error("message.broadcast: type cannot be empty");
            }
            return messageBus->BroadcastMessage(contextName, type, data, getPriority(priority));
        }
    );

    // tas.message.request(target, type, data, timeout?) - Send request and wait for response (async)
    message["request"] = sol::overload(
        [messageBus, context, contextName](const std::string &target, const std::string &type, sol::object data) -> sol::object {
            if (!messageBus || !context) {
                throw sol::error("message.request: MessageBus or Context not available");
            }
            if (target.empty() || type.empty()) {
                throw sol::error("message.request: target and type cannot be empty");
            }

            // Generate correlation ID for this request
            static std::atomic<uint64_t> s_NextCorrelationId{1};
            uint64_t id = s_NextCorrelationId.fetch_add(1, std::memory_order_relaxed);
            std::ostringstream oss;
            oss << "req_" << std::setw(16) << std::setfill('0') << id;
            std::string correlationId = oss.str();

            // Send request message async
            if (!messageBus->SendRequestAsync(contextName, target, type, data, correlationId)) {
                throw sol::error("message.request: Failed to send request message");
            }

            // Yield and wait for response (default 5000ms timeout)
            auto *scheduler = context->GetScheduler();
            if (!scheduler) {
                throw sol::error("message.request: Scheduler not available");
            }

            return scheduler->YieldWaitForMessageResponse(correlationId, 5000);
        },
        [messageBus, context, contextName](const std::string &target, const std::string &type,
                                           sol::object data, int timeout) -> sol::object {
            if (!messageBus || !context) {
                throw sol::error("message.request: MessageBus or Context not available");
            }
            if (target.empty() || type.empty()) {
                throw sol::error("message.request: target and type cannot be empty");
            }

            // Generate correlation ID
            static std::atomic<uint64_t> s_NextCorrelationId{1};
            uint64_t id = s_NextCorrelationId.fetch_add(1, std::memory_order_relaxed);
            std::ostringstream oss;
            oss << "req_" << std::setw(16) << std::setfill('0') << id;
            std::string correlationId = oss.str();

            // Send request message async
            if (!messageBus->SendRequestAsync(contextName, target, type, data, correlationId)) {
                throw sol::error("message.request: Failed to send request message");
            }

            // Yield and wait for response
            auto *scheduler = context->GetScheduler();
            if (!scheduler) {
                throw sol::error("message.request: Scheduler not available");
            }

            return scheduler->YieldWaitForMessageResponse(correlationId, timeout);
        }
    );

    // tas.message.on(type, callback) - Register handler for message type
    message["on"] = [messageBus, contextManager, contextName](const std::string &type, sol::function callback) {
        if (!messageBus || !contextManager) {
            throw sol::error("message.on: MessageBus or ContextManager not available");
        }
        if (type.empty()) {
            throw sol::error("message.on: type cannot be empty");
        }
        if (!callback.valid()) {
            throw sol::error("message.on: callback must be a valid function");
        }
        // Get shared_ptr to context for lifetime tracking
        auto contextPtr = contextManager->GetContext(contextName);
        if (!contextPtr) {
            throw sol::error("message.on: context no longer exists");
        }
        messageBus->RegisterLuaHandler(contextName, contextPtr, type, callback);
    };

    // tas.message.off(type) - Remove handler for message type
    message["off"] = [messageBus, contextName](const std::string &type) {
        if (!messageBus) {
            return;
        }
        if (type.empty()) {
            return;
        }
        messageBus->RemoveHandler(contextName, type);
    };

    // tas.message.reply(message, response_data) - Reply to a request message
    message["reply"] = [messageBus, contextName](sol::table requestMsg, sol::object responseData) -> bool {
        if (!messageBus) {
            throw sol::error("message.reply: MessageBus not available");
        }
        // Extract correlation ID from the request message
        sol::optional<std::string> correlationId = requestMsg["correlation_id"];
        if (!correlationId.has_value() || correlationId.value().empty()) {
            throw sol::error("message.reply: message has no correlation_id (not a request)");
        }

        // Extract sender from the request message (becomes target for response)
        sol::optional<std::string> sender = requestMsg["sender"];
        if (!sender.has_value() || sender.value().empty()) {
            throw sol::error("message.reply: message has no sender field");
        }

        // Send the response back to the requester
        return messageBus->SendResponse(contextName, sender.value(), correlationId.value(), responseData);
    };

    // tas.message.pending_count() - Get number of pending messages
    message["pending_count"] = [messageBus]() -> size_t {
        if (!messageBus) {
            return 0;
        }
        return messageBus->GetPendingMessageCount();
    };

    // tas.message.stats() - Get message bus statistics
    message["stats"] = [messageBus, context]() -> sol::table {
        if (!messageBus || !context) {
            throw sol::error("message.stats: MessageBus or Context not available");
        }
        sol::state_view lua = context->GetLuaState();
        sol::table stats = lua.create_table();

        size_t droppedMessages = 0;
        size_t queueSize = messageBus->GetQueueStats(&droppedMessages);

        stats["queue_size"] = queueSize;
        stats["dropped_messages"] = droppedMessages;

        return stats;
    };

    // ===================================================================
    // Context Info API (tas.context.*)
    // ===================================================================

    sol::table ctx = tas["context"] = tas.create();

    // tas.context.get_name() - Get current context name
    ctx["get_name"] = [contextName]() -> std::string {
        return contextName;
    };

    // tas.context.get_type() - Get context type
    ctx["get_type"] = [context]() -> std::string {
        if (!context) {
            return "unknown";
        }
        switch (context->GetType()) {
        case ScriptContextType::Global: return "global";
        case ScriptContextType::Level: return "level";
        case ScriptContextType::Custom: return "custom";
        default: return "unknown";
        }
    };

    // tas.context.get_priority() - Get context priority
    ctx["get_priority"] = [context]() -> int {
        if (!context) {
            return 0;
        }
        return context->GetPriority();
    };

    // tas.context.list() - List all active contexts
    ctx["list"] = [contextManager, context]() -> sol::table {
        if (!contextManager || !context) {
            throw sol::error("context.list: ContextManager or Context not available");
        }
        sol::state_view lua = context->GetLuaState();
        sol::table list = lua.create_table();

        auto contexts = contextManager->GetAllContexts();
        int index = 1;
        for (const auto &ctx : contexts) {
            if (ctx) {
                sol::table info = lua.create_table();
                info["name"] = ctx->GetName();
                info["priority"] = ctx->GetPriority();
                switch (ctx->GetType()) {
                case ScriptContextType::Global:
                    info["type"] = "global";
                    break;
                case ScriptContextType::Level:
                    info["type"] = "level";
                    break;
                case ScriptContextType::Custom:
                    info["type"] = "custom";
                    break;
                default:
                    info["type"] = "unknown";
                    break;
                }
                info["is_executing"] = ctx->IsExecuting();
                list[index++] = info;
            }
        }

        return list;
    };

    // tas.context.subscribe(event_name) - Subscribe to global game event
    ctx["subscribe"] = [contextManager, contextName](const std::string &eventName) {
        if (!contextManager) {
            throw sol::error("context.subscribe: ContextManager not available");
        }
        if (eventName.empty()) {
            throw sol::error("context.subscribe: event name cannot be empty");
        }
        contextManager->SubscribeToEvent(contextName, eventName);
    };

    // tas.context.unsubscribe(event_name) - Unsubscribe from global game event
    ctx["unsubscribe"] = [contextManager, contextName](const std::string &eventName) {
        if (!contextManager) {
            return;
        }
        if (eventName.empty()) {
            return;
        }
        contextManager->UnsubscribeFromEvent(contextName, eventName);
    };
}
