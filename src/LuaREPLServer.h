#pragma once

// REPL feature requires boost::asio - disable if not available
#ifdef ENABLE_REPL

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <queue>
#include <functional>
#include <atomic>
#include <mutex>
#include <condition_variable>

#include <boost/asio.hpp>
#include <msgpack.hpp>
#include <sol/sol.hpp>

// Forward declarations
class TASEngine;

namespace repl {
    /**
     * @enum CommandExecutionMode
     * @brief Defines when and how a command should be executed.
     */
    enum class CommandExecutionMode : uint8_t {
        IMMEDIATE     = 0, // Execute immediately on main thread
        NEXT_TICK     = 1, // Execute at start of next tick
        SPECIFIC_TICK = 2, // Execute at specific tick number
        PAUSED_ONLY   = 3  // Only execute when paused
    };

    /**
     * @struct REPLCommand
     * @brief Represents a command to be executed.
     */
    struct REPLCommand {
        uint32_t Id;
        std::string LuaCode;
        CommandExecutionMode Mode;
        size_t TargetTick;

        MSGPACK_DEFINE(Id, LuaCode, Mode, TargetTick);
    };

    /**
     * @struct CommandResult
     * @brief Represents the result of command execution.
     */
    struct CommandResult {
        uint32_t CommandId;
        bool Success;
        std::string Result;
        std::string Error;
        size_t ExecutionTick;

        MSGPACK_DEFINE(CommandId, Success, Result, Error, ExecutionTick);
    };

    /**
     * @struct TickNotification
     * @brief Notification sent to clients about tick updates.
     */
    struct TickNotification {
        size_t Tick;
        bool Paused;
        bool WaitComplete = false;

        MSGPACK_DEFINE(Tick, Paused, WaitComplete);
    };

    /**
     * @struct ControlCommand
     * @brief Control commands for REPL server state.
     */
    struct ControlCommand {
        std::string Action; // "pause", "resume", "step", "step_mode", "wait_tick"
        std::string Value;  // Additional data for the command

        MSGPACK_DEFINE(Action, Value);
    };

    /**
     * @struct ControlResponse
     * @brief Response to control commands.
     */
    struct ControlResponse {
        std::string Action;
        bool Success;
        std::string Message;

        MSGPACK_DEFINE(Action, Success, Message);
    };

    /**
     * @struct AuthRequest
     * @brief Authentication request from client.
     */
    struct AuthRequest {
        std::string Token;

        MSGPACK_DEFINE (Token);
    };

    /**
     * @struct AuthResponse
     * @brief Authentication response to client.
     */
    struct AuthResponse {
        bool Success;
        std::string Message;
        size_t CurrentTick;
        bool Paused;

        MSGPACK_DEFINE(Success, Message, CurrentTick, Paused);
    };

    /**
     * @enum MessageType
     * @brief Types of messages exchanged between client and server.
     */
    enum class MessageType : uint8_t {
        AUTH              = 0,
        COMMAND           = 1,
        CONTROL           = 2,
        RESULT            = 3,
        TICK_NOTIFICATION = 4,
        ERROR             = 5
    };

    /**
     * @struct Message
     * @brief Base message structure with type and payload.
     */
    struct Message {
        MessageType Type;
        std::vector<uint8_t> Payload;

        template <typename T>
        void SetPayload(const T &data) {
            std::stringstream ss;
            msgpack::pack(ss, data);
            auto str = ss.str();
            Payload.assign(str.begin(), str.end());
        }

        template <typename T>
        T GetPayload() const {
            auto handle = msgpack::unpack(reinterpret_cast<const char *>(Payload.data()), Payload.size());
            return handle.get().as<T>();
        }

        MSGPACK_DEFINE(Type, Payload);
    };
} // namespace repl

/**
 * @class ClientSession
 * @brief Represents a single client connection with async I/O.
 *
 * Handles all network communication for a single client, including authentication,
 * command processing, and state management. Thread-safe for concurrent access.
 */
class ClientSession : public std::enable_shared_from_this<ClientSession> {
public:
    using TcpSocket = boost::asio::ip::tcp::socket;

    explicit ClientSession(TcpSocket socket, class LuaREPLServer *server);
    ~ClientSession();

    // Non-copyable, non-movable
    ClientSession(const ClientSession &) = delete;
    ClientSession &operator=(const ClientSession &) = delete;

    void Start();
    void SendMessage(const repl::Message &message);
    void Disconnect();

    // Thread-safe client state accessors
    bool IsAuthenticated() const { return m_Authenticated.load(); }
    void SetAuthenticated(bool auth) { m_Authenticated.store(auth); }

    bool IsStepping() const { return m_Stepping.load(); }
    void SetStepping(bool step) { m_Stepping.store(step); }

    bool IsWaitingForTick() const { return m_WaitingForTick.load(); }

    void SetWaitingForTick(bool waiting, size_t target = 0) {
        m_WaitingForTick.store(waiting);
        m_WaitTargetTick.store(target);
    }

    size_t GetWaitTargetTick() const { return m_WaitTargetTick.load(); }

    std::string GetEndpoint() const;
    uint32_t GetNextCommandId() { return m_NextCommandId.fetch_add(1); }

private:
    void DoRead();
    void DoWrite();
    void HandleMessage(const repl::Message &message);
    void ProcessReceivedData(const std::vector<uint8_t> &data);

    TcpSocket m_Socket;
    LuaREPLServer *m_Server;
    std::queue<repl::Message> m_WriteQueue;
    std::vector<uint8_t> m_ReadBuffer;
    std::vector<uint8_t> m_MessageBuffer; // Accumulates partial messages
    std::mutex m_WriteMutex;

    // Thread-safe client state
    std::atomic<bool> m_Authenticated{false};
    std::atomic<bool> m_Stepping{false};
    std::atomic<bool> m_WaitingForTick{false};
    std::atomic<size_t> m_WaitTargetTick{0};
    std::atomic<uint32_t> m_NextCommandId{1};
    std::atomic<bool> m_Connected{true};

    static constexpr size_t MAX_MESSAGE_SIZE = 64 * 1024; // 64KB
};

/**
 * @class LuaREPLServer
 * @brief Production-grade Lua REPL server with tick-perfect synchronization.
 *
 * Provides deterministic command execution for TAS debugging and development.
 * All game thread operations are lock-free for maximum performance.
 * Network operations run on separate threads with proper synchronization.
 */
class LuaREPLServer {
public:
    using TcpAcceptor = boost::asio::ip::tcp::acceptor;
    using TcpEndpoint = boost::asio::ip::tcp::endpoint;

    explicit LuaREPLServer(TASEngine *engine);
    ~LuaREPLServer();

    // Non-copyable, non-movable
    LuaREPLServer(const LuaREPLServer &) = delete;
    LuaREPLServer &operator=(const LuaREPLServer &) = delete;

    // =========================================================================
    // Lifecycle Management
    // =========================================================================

    /**
     * @brief Initializes the REPL server.
     * @param port The port to listen on (default: 7878)
     * @param authToken Optional authentication token for security
     * @return True if initialization was successful.
     */
    bool Initialize(uint16_t port = 7878, const std::string &authToken = "");

    /**
     * @brief Starts the REPL server.
     * @return True if server started successfully.
     */
    bool Start();

    /**
     * @brief Stops the REPL server and disconnects all clients.
     */
    void Stop();

    /**
     * @brief Shuts down the REPL server completely.
     */
    void Shutdown();

    // =========================================================================
    // Tick Synchronization (Main Game Thread Only)
    // =========================================================================

    /**
     * @brief Called at the start of each game tick to process scheduled commands.
     * MUST be called from the main game thread.
     * @param currentTick The current game tick number.
     */
    void OnTickStart(size_t currentTick);

    /**
     * @brief Called at the end of each game tick to send notifications.
     * MUST be called from the main game thread.
     * @param currentTick The current game tick number.
     */
    void OnTickEnd(size_t currentTick);

    /**
     * @brief Processes immediate commands.
     * Must be called from the main game thread (can be called multiple times per tick).
     */
    void ProcessImmediateCommands();

    // =========================================================================
    // Debugging Controls
    // =========================================================================

    /**
     * @brief Pauses tick processing for step-by-step debugging.
     */
    void PauseTicking();

    /**
     * @brief Resumes normal tick processing.
     */
    void ResumeTicking();

    /**
     * @brief Advances by one tick when in paused mode.
     */
    void StepOneTick();

    // =========================================================================
    // State Queries
    // =========================================================================

    bool IsRunning() const { return m_Running.load(); }
    bool IsTickingPaused() const { return m_TickingPaused.load(); }
    size_t GetClientCount() const;
    uint16_t GetPort() const { return m_Port; }
    size_t GetCurrentTick() const { return m_CurrentTick; }

    // =========================================================================
    // Configuration
    // =========================================================================

    void SetAuthenticationRequired(bool required) { m_RequireAuth = required; }
    void SetAuthenticationToken(const std::string &token) { m_AuthToken = token; }

    // =========================================================================
    // Internal Message Handling (Called by ClientSession)
    // =========================================================================

    void HandleAuthRequest(std::shared_ptr<ClientSession> session, const repl::AuthRequest &request);
    void HandleCommand(std::shared_ptr<ClientSession> session, const repl::REPLCommand &command);
    void HandleControlCommand(std::shared_ptr<ClientSession> session, const repl::ControlCommand &control);
    void OnClientDisconnect(std::shared_ptr<ClientSession> session);

private:
    // =========================================================================
    // Internal Types
    // =========================================================================

    struct ScheduledCommand {
        repl::REPLCommand Command;
        std::weak_ptr<ClientSession> Session;

        ScheduledCommand(repl::REPLCommand cmd, std::weak_ptr<ClientSession> sess)
            : Command(std::move(cmd)), Session(std::move(sess)) {
        }
    };

    struct PendingResult {
        repl::CommandResult Result;
        std::weak_ptr<ClientSession> Session;

        PendingResult(repl::CommandResult res, std::weak_ptr<ClientSession> sess)
            : Result(std::move(res)), Session(std::move(sess)) {
        }
    };

    // =========================================================================
    // Command Execution (Main Thread Only)
    // =========================================================================

    std::pair<bool, std::string> ExecuteLuaCommand(const std::string &code, size_t executionTick);
    void ScheduleCommand(const repl::REPLCommand &command, std::shared_ptr<ClientSession> session);
    void SendResultToClient(std::weak_ptr<ClientSession> session, const repl::CommandResult &result);
    void BroadcastTickNotification(size_t tick);
    std::string FormatLuaValue(const sol::object &obj);

    // =========================================================================
    // Network Operations (I/O Thread)
    // =========================================================================

    void DoAccept();
    void RunIOContext();

    // =========================================================================
    // Core Members
    // =========================================================================

    TASEngine *m_Engine;

    // Network components
    std::unique_ptr<boost::asio::io_context> m_IOContext;
    std::unique_ptr<TcpAcceptor> m_Acceptor;
    std::unique_ptr<std::thread> m_IOThread;

    // Server state (atomic for thread-safety)
    std::atomic<bool> m_Initialized{false};
    std::atomic<bool> m_Running{false};
    std::atomic<bool> m_ShouldStop{false};
    uint16_t m_Port{7878};

    // Authentication
    bool m_RequireAuth{false};
    std::string m_AuthToken;

    // Tick synchronization (main thread only)
    std::atomic<bool> m_TickingPaused{false};
    std::atomic<bool> m_StepRequested{false};
    size_t m_CurrentTick{0}; // Only accessed from main thread

    // Client management
    std::vector<std::shared_ptr<ClientSession>> m_Clients;
    mutable std::mutex m_ClientsMutex;

    // Command queues (lock-free where possible)
    // Main thread writes, I/O threads read
    std::unordered_map<size_t, std::vector<ScheduledCommand>> m_ScheduledCommands;
    std::queue<ScheduledCommand> m_ImmediateCommands;
    std::queue<PendingResult> m_PendingResults;

    // Synchronization primitives
    std::mutex m_ScheduledCommandsMutex;
    std::mutex m_ImmediateCommandsMutex;
    std::mutex m_ResultsMutex;

    // Constants
    static constexpr size_t MAX_CLIENTS = 10;
    static constexpr size_t MAX_COMMAND_QUEUE_SIZE = 1000;
    static constexpr std::chrono::milliseconds SOCKET_TIMEOUT{5000};
};

#endif // ENABLE_REPL
