#include "LuaScheduler.h"

#include "Logger.h"
#include <stdexcept>
#include <utility>

#include "EventManager.h"
#include "TASEngine.h"
#include "ScriptContext.h"
#include "ScriptContextManager.h"
#include "MessageBus.h"

EventWaitTask::EventWaitTask(std::string eventName, TASEngine *engine, EventManager *eventManager)
    : m_EventName(std::move(eventName)), m_Engine(engine), m_EventManager(eventManager), m_EventReceived(false) {
    if (!m_EventManager) {
        throw std::runtime_error("EventWaitTask requires an event manager instance");
    }

    std::function<void()> callback = [this]() {
        m_EventReceived = true;
    };
    m_ListenerId = m_EventManager->RegisterListener(m_EventName, callback, true);

    if (m_ListenerId == EventManager::kInvalidListenerId && m_Engine) {
        Log::Error("EventWaitTask: Failed to register listener for event '%s'",
                                     m_EventName.c_str());
        m_EventReceived = true; // Prevent the coroutine from stalling forever
    }
}

EventWaitTask::~EventWaitTask() {
    if (m_EventManager && m_ListenerId != EventManager::kInvalidListenerId) {
        m_EventManager->UnregisterListener(m_EventName, m_ListenerId);
        m_ListenerId = EventManager::kInvalidListenerId;
    }
}

// ============================================================================
// MessageResponseTask Implementation
// ============================================================================

MessageResponseTask::MessageResponseTask(std::string correlationId, TASEngine *engine, int timeoutTicks)
    : m_CorrelationId(std::move(correlationId)),
      m_Engine(engine),
      m_TimeoutTicks(timeoutTicks),
      m_ResponseReceived(false) {
}

MessageResponseTask::~MessageResponseTask() {
    // No cleanup needed - responses are managed by MessageBus
}

bool MessageResponseTask::IsComplete() {
    // Check for timeout
    if (m_TimeoutTicks <= 0) {
        if (m_Engine) {
            Log::Warn("MessageResponseTask: Timeout waiting for response (correlation_id: %s)",
                                       m_CorrelationId.c_str());
        }
        return true;  // Timeout
    }

    // Try to get response from MessageBus
    auto *contextManager = m_Engine ? m_Engine->GetScriptContextManager() : nullptr;
    auto *messageBus = contextManager ? contextManager->GetMessageBus() : nullptr;

    if (!messageBus) {
        if (m_Engine) {
            Log::Error("MessageResponseTask: MessageBus not available");
        }
        return true;  // Error - complete to avoid hanging
    }

    auto response = messageBus->TryGetResponse(m_CorrelationId);
    if (response.has_value()) {
        // Response received! Store the serialized value
        m_ResponseReceived = true;
        m_ResponseData = response->data;  // Store MessageBus::Message::SerializedValue as std::any
        return true;
    }

    // Still waiting - decrement timeout
    --m_TimeoutTicks;
    return false;
}

sol::object MessageResponseTask::GetResponse(sol::state_view lua) const {
    if (!m_ResponseReceived || !m_ResponseData.has_value()) {
        return sol::make_object(lua, sol::nil);
    }

    try {
        // Extract the SerializedValue and convert to Lua object
        const auto &serializedValue = std::any_cast<const MessageBus::Message::SerializedValue &>(m_ResponseData);
        return serializedValue.ToLuaObject(lua);
    } catch (const std::bad_any_cast &) {
        if (m_Engine) {
            Log::Error("MessageResponseTask: Failed to cast response data");
        }
        return sol::make_object(lua, sol::nil);
    }
}

LuaScheduler::LuaScheduler(TASEngine *engine, ScriptContext *context)
    : m_Engine(engine), m_Context(context), m_CurrentThread(nullptr) {
    if (!m_Engine) {
        throw std::runtime_error("LuaScheduler requires a valid TASEngine instance");
    }
    if (!m_Context) {
        throw std::runtime_error("LuaScheduler requires a valid ScriptContext");
    }
}

sol::state &LuaScheduler::GetLuaState() const {
    return m_Context->GetLuaState();
}

void LuaScheduler::StartCoroutine(sol::coroutine co) {
    m_ThreadValidator.AssertOwnership();

    // Check if coroutine is valid
    if (!co.valid()) {
        Log::Error("StartCoroutine: invalid coroutine provided");
        return;
    }

    // Create a new thread context for the coroutine
    auto thread = std::make_shared<detail::SchedulerCothread>(GetLuaState(), co);

    // Push it onto the stack to set the current execution context
    m_ThreadStack.push(thread);
    m_CurrentThread = thread;

    // Start the coroutine
    auto result = m_CurrentThread->coroutine();

    // The coroutine has either finished or yielded.
    // If it did NOT yield, its execution is over, so we can pop it from the stack.
    // If it DID yield, a task was already created by a YieldXxx function,
    // and it remains on the stack as part of the execution context until it's done.
    if (m_CurrentThread->coroutine.status() != sol::call_status::yielded) {
        m_ThreadStack.pop();
    }

    // Reset current thread pointer to the top of the stack
    if (m_ThreadStack.empty()) {
        m_CurrentThread = nullptr;
    } else {
        m_CurrentThread = m_ThreadStack.top();
    }

    // Handle any errors
    if (!result.valid()) {
        sol::error err = result;
        Log::Error("Coroutine error: %s", err.what());
    }
}

void LuaScheduler::StartCoroutine(sol::function func) {
    m_ThreadValidator.AssertOwnership();

    if (!func.valid()) {
        Log::Error("StartCoroutine: invalid function provided");
        return;
    }

    // Create a new thread context for the coroutine
    auto thread = std::make_shared<detail::SchedulerCothread>(GetLuaState(), func);

    // Push it onto the stack to set the current execution context
    m_ThreadStack.push(thread);
    m_CurrentThread = thread;

    // Start the coroutine
    auto result = m_CurrentThread->coroutine();

    // The coroutine has either finished or yielded.
    // If it did NOT yield, its execution is over, so we can pop it from the stack.
    // If it DID yield, a task was already created by a YieldXxx function,
    // and it remains on the stack as part of the execution context until it's done.
    if (m_CurrentThread->coroutine.status() != sol::call_status::yielded) {
        m_ThreadStack.pop();
    }

    // Reset current thread pointer to the top of the stack
    if (m_ThreadStack.empty()) {
        m_CurrentThread = nullptr;
    } else {
        m_CurrentThread = m_ThreadStack.top();
    }

    // Handle any errors
    if (!result.valid()) {
        sol::error err = result;
        Log::Error("Coroutine error: %s", err.what());
    }
}

sol::coroutine LuaScheduler::StartCoroutineAndTrack(sol::function func) {
    if (!func.valid()) {
        Log::Error("StartCoroutineAndTrack: invalid function provided");
        return {};
    }

    // Create new thread for the coroutine
    auto thread = std::make_shared<detail::SchedulerCothread>(GetLuaState(), func);

    // Create an immediate task that will cause the coroutine to start on next tick
    auto task = std::make_shared<ImmediateTask>();

    // Add to task list
    detail::SchedulerThreadTask threadTask;
    threadTask.thread = thread;
    threadTask.task = task;
    m_Tasks.push_back(threadTask);

    // Return the coroutine reference for tracking
    return thread->coroutine;
}

sol::coroutine LuaScheduler::StartCoroutineAndTrack(sol::coroutine co) {
    if (!co.valid()) {
        Log::Error("StartCoroutineAndTrack: invalid coroutine provided");
        return {};
    }

    // Create thread for existing coroutine (handles thread management internally)
    auto thread = std::make_shared<detail::SchedulerCothread>(GetLuaState(), co);

    // Create an immediate task
    auto task = std::make_shared<ImmediateTask>();

    // Add to task list
    detail::SchedulerThreadTask threadTask;
    threadTask.thread = thread;
    threadTask.task = task;
    m_Tasks.push_back(threadTask);

    // Return the coroutine reference for tracking
    return thread->coroutine;
}

void LuaScheduler::AddCoroutineTask(sol::coroutine co) {
    m_ThreadValidator.AssertOwnership();

    if (!co.valid()) {
        Log::Error("AddCoroutineTask: invalid coroutine provided");
        return;
    }

    // Create thread for existing coroutine
    auto thread = std::make_shared<detail::SchedulerCothread>(GetLuaState(), co);

    // Create an immediate task
    auto task = std::make_shared<ImmediateTask>();

    // Add to task list
    detail::SchedulerThreadTask threadTask;
    threadTask.thread = thread;
    threadTask.task = task;
    m_Tasks.push_back(threadTask);
}

void LuaScheduler::AddCoroutineTask(sol::function func) {
    m_ThreadValidator.AssertOwnership();

    if (!func.valid()) {
        Log::Error("AddCoroutineTask: invalid function provided");
        return;
    }

    // For functions, create new thread directly
    auto thread = std::make_shared<detail::SchedulerCothread>(GetLuaState(), func);

    // Create an immediate task
    auto task = std::make_shared<ImmediateTask>();

    // Add to task list
    detail::SchedulerThreadTask threadTask;
    threadTask.thread = thread;
    threadTask.task = task;
    m_Tasks.push_back(threadTask);
}

void LuaScheduler::Tick() {
    m_ThreadValidator.AssertOwnership();

    // Process coroutine-based tasks
    for (auto i = m_Tasks.begin(); i != m_Tasks.end();) {
        // If the thread is dead, remove it
        if (i->thread->coroutine.status() != sol::call_status::yielded) {
            i = m_Tasks.erase(i);
            continue;
        }

        // Is this task complete?
        if (i->task->IsComplete()) {
            // Get the thread task
            detail::SchedulerThreadTask thread_task = *i;
            // Remove it from the pending list
            i = m_Tasks.erase(i);

            // Set the current thread
            m_ThreadStack.push(thread_task.thread);
            m_CurrentThread = thread_task.thread;

            // Resume the thread
            auto result = thread_task.thread->coroutine();

            // Reset current thread
            m_ThreadStack.pop();
            if (m_ThreadStack.empty()) {
                m_CurrentThread = nullptr;
            } else {
                m_CurrentThread = m_ThreadStack.top();
            }

            // Handle any errors
            if (!result.valid()) {
                sol::error err = result;
                Log::Error("Coroutine resume error: %s", err.what());
            }
        } else {
            ++i;
        }
    }

    // Process background tasks
    for (auto i = m_BackgroundTasks.begin(); i != m_BackgroundTasks.end();) {
        if ((*i)->IsComplete()) {
            i = m_BackgroundTasks.erase(i);
        } else {
            ++i;
        }
    }
}

void LuaScheduler::Clear() {
    m_ThreadValidator.AssertOwnership();

    m_Tasks.clear();
    m_BackgroundTasks.clear();
    m_CurrentThread = nullptr;
    // Clear the thread stack
    while (!m_ThreadStack.empty()) {
        m_ThreadStack.pop();
    }
}

bool LuaScheduler::IsRunning() const {
    return !m_Tasks.empty() || !m_BackgroundTasks.empty();
}

size_t LuaScheduler::GetTaskCount() const {
    return m_Tasks.size() + m_BackgroundTasks.size();
}

void LuaScheduler::YieldTicks(int ticks) {
    m_ThreadValidator.AssertOwnership();

    if (ticks <= 0) {
        Log::Error("YieldTicks: tick count must be positive");
        return;
    }

    if (!m_CurrentThread) {
        Log::Error("YieldTicks called outside of coroutine context");
        return;
    }

    Yield(std::make_shared<TickWaitTask>(ticks));
}

void LuaScheduler::YieldUntil(sol::function predicate) {
    if (!predicate.valid()) {
        Log::Error("YieldUntil: invalid predicate function");
        return;
    }

    if (!m_CurrentThread) {
        Log::Error("YieldUntil called outside of coroutine context");
        return;
    }

    Yield(std::make_shared<PredicateWaitTask>(predicate));
}

void LuaScheduler::YieldCoroutines(const std::vector<sol::coroutine> &coroutines) {
    if (coroutines.empty()) {
        Log::Error("YieldCoroutines: no coroutines to wait for");
        return;
    }

    if (!m_CurrentThread) {
        Log::Error("YieldCoroutines called outside of coroutine context");
        return;
    }

    Yield(std::make_shared<CoroutineWaitTask>(coroutines));
}

void LuaScheduler::YieldRace(const std::vector<sol::coroutine> &coroutines) {
    if (coroutines.empty()) {
        Log::Error("YieldRace: no coroutines to wait for");
        return;
    }

    if (!m_CurrentThread) {
        Log::Error("YieldRace called outside of coroutine context");
        return;
    }

    Yield(std::make_shared<RaceTask>(coroutines));
}

void LuaScheduler::YieldWaitForEvent(const std::string &event_name) {
    if (event_name.empty()) {
        Log::Error("YieldWaitForEvent: event name cannot be empty");
        return;
    }

    if (!m_CurrentThread) {
        Log::Error("YieldWaitForEvent called outside of coroutine context");
        return;
    }

    EventManager *eventManager = m_Context->GetEventManager();
    if (!eventManager) {
        throw std::runtime_error("YieldWaitForEvent: Context event manager not available");
    }

    Yield(std::make_shared<EventWaitTask>(event_name, m_Engine, eventManager));
}

sol::object LuaScheduler::YieldWaitForMessageResponse(const std::string &correlationId, int timeoutMs) {
    if (correlationId.empty()) {
        Log::Error("YieldWaitForMessageResponse: correlation_id cannot be empty");
        sol::state_view lua = GetLuaState();
        return sol::make_object(lua, sol::nil);
    }

    if (!m_CurrentThread) {
        Log::Error("YieldWaitForMessageResponse called outside of coroutine context");
        sol::state_view lua = GetLuaState();
        return sol::make_object(lua, sol::nil);
    }

    // Convert timeout from milliseconds to ticks (assuming 60 FPS)
    constexpr int TICKS_PER_SECOND = 60;
    int timeoutTicks = (timeoutMs * TICKS_PER_SECOND) / 1000;
    if (timeoutTicks <= 0) timeoutTicks = 300; // Default 5 seconds

    // Create the task and yield
    auto task = std::make_shared<MessageResponseTask>(correlationId, m_Engine, timeoutTicks);
    Yield(task);

    // After resuming, get the response
    sol::state_view lua = GetLuaState();
    return task->GetResponse(lua);
}

void LuaScheduler::StartRepeatFor(sol::function task, int ticks) {
    m_ThreadValidator.AssertOwnership();

    if (!task.valid()) {
        Log::Error("StartRepeatFor: invalid task function");
        return;
    }

    if (ticks <= 0) {
        Log::Error("StartRepeatFor: tick count must be positive");
        return;
    }

    auto repeatTask = std::make_shared<RepeatForTicksTask>(task, ticks);
    m_BackgroundTasks.push_back(repeatTask);
}

void LuaScheduler::StartRepeatUntil(sol::function task, sol::function condition) {
    m_ThreadValidator.AssertOwnership();

    if (!task.valid()) {
        Log::Error("StartRepeatUntil: invalid task function");
        return;
    }

    if (!condition.valid()) {
        Log::Error("StartRepeatUntil: invalid condition function");
        return;
    }

    auto repeatTask = std::make_shared<RepeatUntilTask>(task, condition);
    m_BackgroundTasks.push_back(repeatTask);
}

void LuaScheduler::StartRepeatWhile(sol::function task, sol::function condition) {
    m_ThreadValidator.AssertOwnership();

    if (!task.valid()) {
        Log::Error("StartRepeatWhile: invalid task function");
        return;
    }

    if (!condition.valid()) {
        Log::Error("StartRepeatWhile: invalid condition function");
        return;
    }

    auto repeatTask = std::make_shared<RepeatWhileTask>(task, condition);
    m_BackgroundTasks.push_back(repeatTask);
}

void LuaScheduler::StartDelay(sol::function task, int delayTicks) {
    m_ThreadValidator.AssertOwnership();

    if (!task.valid()) {
        Log::Error("StartDelay: invalid task function");
        return;
    }

    if (delayTicks < 0) {
        Log::Error("StartDelay: delay ticks cannot be negative");
        return;
    }

    auto delayTask = std::make_shared<DelayTask>(task, delayTicks);
    m_BackgroundTasks.push_back(delayTask);
}

void LuaScheduler::StartTimeout(sol::function task, int timeoutTicks) {
    m_ThreadValidator.AssertOwnership();

    if (!task.valid()) {
        Log::Error("StartTimeout: invalid task function");
        return;
    }

    if (timeoutTicks <= 0) {
        Log::Error("StartTimeout: timeout must be positive");
        return;
    }

    auto timeoutTask = std::make_shared<TimeoutTask>(task, timeoutTicks);
    m_BackgroundTasks.push_back(timeoutTask);
}

void LuaScheduler::StartDebounce(sol::function task, int debounceTicks) {
    m_ThreadValidator.AssertOwnership();

    if (!task.valid()) {
        Log::Error("StartDebounce: invalid task function");
        return;
    }

    if (debounceTicks <= 0) {
        Log::Error("StartDebounce: debounce ticks must be positive");
        return;
    }

    auto debounceTask = std::make_shared<DebounceTask>(task, debounceTicks);
    m_BackgroundTasks.push_back(debounceTask);
}

void LuaScheduler::StartSequence(const std::vector<sol::function> &tasks) {
    m_ThreadValidator.AssertOwnership();

    if (tasks.empty()) {
        Log::Error("StartSequence: no tasks provided");
        return;
    }

    auto sequenceTask = std::make_shared<SequenceTask>(tasks);
    m_BackgroundTasks.push_back(sequenceTask);
}

void LuaScheduler::StartRetry(sol::function task, int maxAttempts) {
    m_ThreadValidator.AssertOwnership();

    if (!task.valid()) {
        Log::Error("StartRetry: invalid task function");
        return;
    }

    if (maxAttempts <= 0) {
        Log::Error("StartRetry: max attempts must be positive");
        return;
    }

    auto retryTask = std::make_shared<RetryTask>(task, maxAttempts);
    m_BackgroundTasks.push_back(retryTask);
}

void LuaScheduler::StartParallel(const std::vector<sol::function> &functions) {
    m_ThreadValidator.AssertOwnership();

    for (const auto &func : functions) {
        if (func.valid()) {
            AddCoroutineTask(func);
        }
    }
}

void LuaScheduler::StartParallel(const std::vector<sol::coroutine> &coroutines) {
    m_ThreadValidator.AssertOwnership();

    for (const auto &co : coroutines) {
        if (co.valid()) {
            AddCoroutineTask(co);
        }
    }
}

void LuaScheduler::Yield(std::shared_ptr<SchedulerTask> task) {
    detail::SchedulerThreadTask thread_task;
    thread_task.thread = m_CurrentThread;
    thread_task.task = std::move(task);
    m_Tasks.push_back(thread_task);
}
