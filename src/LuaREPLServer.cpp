// REPL feature requires boost::asio - disable if not available
#ifdef ENABLE_REPL
#include "Logger.h"

#include "LuaREPLServer.h"
#include "TASEngine.h"
#include "ScriptContextManager.h"
#include "ScriptContext.h"

#include <iostream>
#include <sstream>
#include <algorithm>
#include <functional>
#include <iomanip>

using namespace boost::asio;
using TcpSocket = ip::tcp::socket;
using TcpEndpoint = ip::tcp::endpoint;

// =============================================================================
// ClientSession Implementation
// =============================================================================

ClientSession::ClientSession(TcpSocket socket, LuaREPLServer *server)
    : m_Socket(std::move(socket)), m_Server(server), m_ReadBuffer(MAX_MESSAGE_SIZE), m_MessageBuffer() {
    if (!m_Server) {
        throw std::invalid_argument("ClientSession requires valid LuaREPLServer");
    }
}

ClientSession::~ClientSession() {
    Disconnect();
}

void ClientSession::Start() {
    try {
        // Send welcome message
        repl::AuthResponse welcome;
        welcome.Success = !m_Server->m_RequireAuth;
        welcome.Message = "Connected to BallanceTAS Lua REPL";
        welcome.CurrentTick = m_Server->GetCurrentTick();
        welcome.Paused = m_Server->IsTickingPaused();

        repl::Message msg;
        msg.Type = repl::MessageType::AUTH;
        msg.SetPayload(welcome);

        SendMessage(msg);

        if (!m_Server->m_RequireAuth) {
            SetAuthenticated(true);
        }

        DoRead();
    } catch (const std::exception &e) {
        if (m_Server->m_Engine) {
            m_Server->Log::Error(
                "REPL client %s failed to start: %s",
                GetEndpoint().c_str(), e.what()
            );
        }
        Disconnect();
    }
}

void ClientSession::SendMessage(const repl::Message &message) {
    if (!m_Connected.load()) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_WriteMutex);

    bool writeInProgress = !m_WriteQueue.empty();
    m_WriteQueue.push(message);

    if (!writeInProgress) {
        DoWrite();
    }
}

void ClientSession::Disconnect() {
    bool expected = true;
    if (m_Connected.compare_exchange_strong(expected, false)) {
        boost::system::error_code ec;
        if (m_Socket.is_open()) {
            m_Socket.shutdown(TcpSocket::shutdown_both, ec);
            m_Socket.close(ec);
        }
    }
}

std::string ClientSession::GetEndpoint() const {
    try {
        if (m_Socket.is_open()) {
            auto endpoint = m_Socket.remote_endpoint();
            return endpoint.address().to_string() + ":" + std::to_string(endpoint.port());
        }
    } catch (const std::exception &) {
        // Fall through to return unknown
    }
    return "unknown";
}

void ClientSession::DoRead() {
    if (!m_Connected.load()) {
        return;
    }

    auto self = shared_from_this();

    m_Socket.async_read_some(
        buffer(m_ReadBuffer),
        [this, self](boost::system::error_code ec, std::size_t bytesTransferred) {
            if (!ec && m_Connected.load()) {
                try {
                    // Append to message buffer
                    m_MessageBuffer.insert(m_MessageBuffer.end(),
                                           m_ReadBuffer.begin(),
                                           m_ReadBuffer.begin() + bytesTransferred);

                    ProcessReceivedData(m_MessageBuffer);
                    DoRead(); // Continue reading
                } catch (const std::exception &e) {
                    if (m_Server->m_Engine) {
                        m_Server->Log::Error(
                            "REPL client %s message processing error: %s",
                            GetEndpoint().c_str(), e.what()
                        );
                    }
                    Disconnect();
                }
            } else if (ec != error::operation_aborted) {
                if (m_Server->m_Engine && ec != error::eof) {
                    m_Server->Log::Info(
                        "REPL client %s disconnected: %s",
                        GetEndpoint().c_str(), ec.message().c_str()
                    );
                }
                m_Server->OnClientDisconnect(self);
            }
        }
    );
}

void ClientSession::DoWrite() {
    if (m_WriteQueue.empty() || !m_Connected.load()) {
        return;
    }

    auto self = shared_from_this();
    auto &message = m_WriteQueue.front();

    try {
        // Serialize message
        std::stringstream ss;
        msgpack::pack(ss, message);
        auto data = std::make_shared<std::string>(ss.str());

        async_write(
            m_Socket,
            buffer(*data),
            [this, self, data](boost::system::error_code ec, std::size_t /*bytesTransferred*/) {
                std::lock_guard<std::mutex> lock(m_WriteMutex);

                if (!m_WriteQueue.empty()) {
                    m_WriteQueue.pop();
                }

                if (!ec && m_Connected.load()) {
                    if (!m_WriteQueue.empty()) {
                        DoWrite();
                    }
                } else {
                    if (m_Server->m_Engine && ec != error::operation_aborted) {
                        m_Server->Log::Error(
                            "REPL client %s write error: %s",
                            GetEndpoint().c_str(), ec.message().c_str()
                        );
                    }
                    Disconnect();
                }
            }
        );
    } catch (const std::exception &e) {
        if (m_Server->m_Engine) {
            m_Server->Log::Error(
                "REPL client %s serialization error: %s",
                GetEndpoint().c_str(), e.what()
            );
        }
        Disconnect();
    }
}

void ClientSession::ProcessReceivedData(const std::vector<uint8_t> &data) {
    size_t offset = 0;

    while (offset < data.size()) {
        try {
            // Try to unpack a complete message
            size_t off = 0; // Track how much was consumed
            msgpack::object_handle handle = msgpack::unpack(
                reinterpret_cast<const char *>(data.data()) + offset,
                data.size() - offset,
                off // This output parameter receives the number of bytes consumed
            );

            auto message = handle.get().as<repl::Message>();
            HandleMessage(message);

            // Use the correct consumed size from msgpack::unpack
            if (off == 0) {
                // Safety check to prevent infinite loop
                break;
            }

            offset += off;
        } catch (const msgpack::insufficient_bytes &) {
            // Need more data - keep what we have
            break;
        } catch (const std::exception &e) {
            if (m_Server->m_Engine) {
                m_Server->Log::Error(
                    "REPL client %s message parse error: %s",
                    GetEndpoint().c_str(), e.what()
                );
            }
            throw; // Propagate to disconnect client
        }
    }

    // Remove processed data from buffer
    if (offset > 0) {
        m_MessageBuffer.erase(m_MessageBuffer.begin(), m_MessageBuffer.begin() + offset);
    }

    // Prevent buffer from growing too large
    if (m_MessageBuffer.size() > MAX_MESSAGE_SIZE) {
        throw std::runtime_error("Message buffer overflow");
    }
}

void ClientSession::HandleMessage(const repl::Message &message) {
    switch (message.Type) {
    case repl::MessageType::AUTH: {
        auto authReq = message.GetPayload<repl::AuthRequest>();
        m_Server->HandleAuthRequest(shared_from_this(), authReq);
        break;
    }
    case repl::MessageType::COMMAND: {
        if (!IsAuthenticated()) {
            repl::Message errorMsg;
            errorMsg.Type = repl::MessageType::ERROR;
            errorMsg.SetPayload(std::string("Authentication required"));
            SendMessage(errorMsg);
            return;
        }
        auto command = message.GetPayload<repl::REPLCommand>();
        m_Server->HandleCommand(shared_from_this(), command);
        break;
    }
    case repl::MessageType::CONTROL: {
        if (!IsAuthenticated()) {
            repl::Message errorMsg;
            errorMsg.Type = repl::MessageType::ERROR;
            errorMsg.SetPayload(std::string("Authentication required"));
            SendMessage(errorMsg);
            return;
        }
        auto control = message.GetPayload<repl::ControlCommand>();
        m_Server->HandleControlCommand(shared_from_this(), control);
        break;
    }
    default:
        if (m_Server->m_Engine) {
            m_Server->Log::Warn(
                "REPL client %s sent unknown message type: %d",
                GetEndpoint().c_str(), static_cast<int>(message.Type)
            );
        }
        break;
    }
}

// =============================================================================
// LuaREPLServer Implementation
// =============================================================================

LuaREPLServer::LuaREPLServer(TASEngine *engine) : m_Engine(engine) {
    if (!m_Engine) {
        throw std::invalid_argument("LuaREPLServer requires a valid TASEngine instance");
    }
}

LuaREPLServer::~LuaREPLServer() {
    Shutdown();
}

bool LuaREPLServer::Initialize(uint16_t port, const std::string &authToken) {
    if (m_Initialized.load()) {
        if (m_Engine) {
            Log::Warn("LuaREPLServer already initialized");
        }
        return true;
    }

    try {
        m_Port = port;
        m_AuthToken = authToken;
        m_RequireAuth = !authToken.empty();

        m_IOContext = std::make_unique<io_context>();
        m_Acceptor = std::make_unique<TcpAcceptor>(*m_IOContext, TcpEndpoint(ip::tcp::v4(), port));

        // Configure acceptor
        m_Acceptor->set_option(ip::tcp::acceptor::reuse_address(true));

        m_Initialized.store(true);

        if (m_Engine) {
            Log::Info("LuaREPLServer initialized on port %d", port);
            if (m_RequireAuth) {
                Log::Info("Authentication required for REPL connections");
            }
        }

        return true;
    } catch (const std::exception &e) {
        if (m_Engine) {
            Log::Error("Failed to initialize LuaREPLServer: %s", e.what());
        }
        return false;
    }
}

bool LuaREPLServer::Start() {
    if (!m_Initialized.load()) {
        if (m_Engine) {
            Log::Error("LuaREPLServer not initialized");
        }
        return false;
    }

    if (m_Running.load()) {
        if (m_Engine) {
            Log::Warn("LuaREPLServer already running");
        }
        return true;
    }

    try {
        m_ShouldStop.store(false);
        m_Running.store(true);

        // Start accepting connections
        DoAccept();

        // Start I/O thread
        m_IOThread = std::make_unique<std::thread>([this]() { RunIOContext(); });

        if (m_Engine) {
            Log::Info("LuaREPLServer started on port %d", m_Port);
        }

        return true;
    } catch (const std::exception &e) {
        if (m_Engine) {
            Log::Error("Failed to start LuaREPLServer: %s", e.what());
        }
        m_Running.store(false);
        return false;
    }
}

void LuaREPLServer::Stop() {
    if (!m_Running.load()) return;

    if (m_Engine) {
        Log::Info("Stopping LuaREPLServer...");
    }

    m_ShouldStop.store(true);
    m_Running.store(false);

    // Stop accepting new connections
    if (m_Acceptor) {
        boost::system::error_code ec;
        m_Acceptor->close(ec);
    }

    // Disconnect all clients
    {
        std::lock_guard<std::mutex> lock(m_ClientsMutex);
        for (auto &client : m_Clients) {
            client->Disconnect();
        }
        m_Clients.clear();
    }

    // Stop I/O context
    if (m_IOContext) {
        m_IOContext->stop();
    }

    // Wait for I/O thread
    if (m_IOThread &&m_IOThread
    ->
    joinable()
    )
    {
        m_IOThread->join();
        m_IOThread.reset();
    }

    // Clear command queues
    {
        std::lock_guard<std::mutex> lock(m_ScheduledCommandsMutex);
        m_ScheduledCommands.clear();
    }
    {
        std::lock_guard<std::mutex> lock(m_ImmediateCommandsMutex);
        while (!m_ImmediateCommands.empty()) {
            m_ImmediateCommands.pop();
        }
    }
    {
        std::lock_guard<std::mutex> lock(m_ResultsMutex);
        while (!m_PendingResults.empty()) {
            m_PendingResults.pop();
        }
    }

    if (m_Engine) {
        Log::Info("LuaREPLServer stopped");
    }
}

void LuaREPLServer::Shutdown() {
    if (!m_Initialized.load()) return;

    Stop();

    m_Acceptor.reset();
    m_IOContext.reset();

    m_Initialized.store(false);

    if (m_Engine) {
        Log::Info("LuaREPLServer shutdown complete");
    }
}

void LuaREPLServer::OnTickStart(size_t currentTick) {
    if (!m_Running.load()) return;

    m_CurrentTick = currentTick;

    // Check if we should pause for stepping
    if (m_TickingPaused.load() && !m_StepRequested.load()) {
        return; // Don't process commands when paused and no step requested
    }

    // Process scheduled commands for this tick
    std::vector < ScheduledCommand > commandsToExecute;
    {
        std::lock_guard<std::mutex> lock(m_ScheduledCommandsMutex);
        auto it = m_ScheduledCommands.find(currentTick);
        if (it != m_ScheduledCommands.end()) {
            commandsToExecute = std::move(it->second);
            m_ScheduledCommands.erase(it);
        }
    }

    // Execute scheduled commands
    for (const auto &scheduledCmd : commandsToExecute) {
        auto [success, result] = ExecuteLuaCommand(scheduledCmd.Command.LuaCode, currentTick);

        repl::CommandResult cmdResult;
        cmdResult.CommandId = scheduledCmd.Command.Id;
        cmdResult.Success = success;
        cmdResult.Result = success ? result : "";
        cmdResult.Error = success ? "" : result;
        cmdResult.ExecutionTick = currentTick;

        SendResultToClient(scheduledCmd.Session, cmdResult);
    }

    // Reset step request
    m_StepRequested.store(false);
}

void LuaREPLServer::OnTickEnd(size_t currentTick) {
    if (!m_Running.load()) return;

    // Send tick notifications to clients
    BroadcastTickNotification(currentTick);

    // Process pending results
    std::queue < PendingResult > results;
    {
        std::lock_guard<std::mutex> lock(m_ResultsMutex);
        results.swap(m_PendingResults);
    }

    while (!results.empty()) {
        const auto &pendingResult = results.front();

        if (auto session = pendingResult.Session.lock()) {
            repl::Message msg;
            msg.Type = repl::MessageType::RESULT;
            msg.SetPayload(pendingResult.Result);
            session->SendMessage(msg);
        }

        results.pop();
    }
}

void LuaREPLServer::ProcessImmediateCommands() {
    if (!m_Running.load()) return;

    std::queue < ScheduledCommand > commands;
    {
        std::lock_guard<std::mutex> lock(m_ImmediateCommandsMutex);
        commands.swap(m_ImmediateCommands);
    }

    while (!commands.empty()) {
        const auto &scheduledCmd = commands.front();

        auto [success, result] = ExecuteLuaCommand(scheduledCmd.Command.LuaCode, m_CurrentTick);

        repl::CommandResult cmdResult;
        cmdResult.CommandId = scheduledCmd.Command.Id;
        cmdResult.Success = success;
        cmdResult.Result = success ? result : "";
        cmdResult.Error = success ? "" : result;
        cmdResult.ExecutionTick = m_CurrentTick;

        SendResultToClient(scheduledCmd.Session, cmdResult);

        commands.pop();
    }
}

void LuaREPLServer::PauseTicking() {
    m_TickingPaused.store(true);
    if (m_Engine) {
        Log::Info("REPL tick processing paused");
    }
}

void LuaREPLServer::ResumeTicking() {
    m_TickingPaused.store(false);
    if (m_Engine) {
        Log::Info("REPL tick processing resumed");
    }
}

void LuaREPLServer::StepOneTick() {
    if (m_TickingPaused.load()) {
        m_StepRequested.store(true);
        if (m_Engine) {
            Log::Info("REPL step requested for tick %zu", m_CurrentTick + 1);
        }
    }
}

size_t LuaREPLServer::GetClientCount() const {
    std::lock_guard<std::mutex> lock(m_ClientsMutex);
    return m_Clients.size();
}

void LuaREPLServer::HandleAuthRequest(std::shared_ptr<ClientSession> session, const repl::AuthRequest &request) {
    repl::AuthResponse response;

    if (!m_RequireAuth || request.Token == m_AuthToken) {
        session->SetAuthenticated(true);
        response.Success = true;
        response.Message = "Authentication successful";
    } else {
        response.Success = false;
        response.Message = "Invalid authentication token";
    }

    response.CurrentTick = m_CurrentTick;
    response.Paused = m_TickingPaused.load();

    repl::Message msg;
    msg.Type = repl::MessageType::AUTH;
    msg.SetPayload(response);
    session->SendMessage(msg);

    if (m_Engine) {
        Log::Info(
            "REPL client %s authentication %s",
            session->GetEndpoint().c_str(),
            response.Success ? "succeeded" : "failed"
        );
    }
}

void LuaREPLServer::HandleCommand(std::shared_ptr<ClientSession> session, const repl::REPLCommand &command) {
    ScheduleCommand(command, session);
}

void LuaREPLServer::HandleControlCommand(std::shared_ptr<ClientSession> session, const repl::ControlCommand &control) {
    repl::ControlResponse response;
    response.Action = control.Action;
    response.Success = true;

    if (control.Action == "pause") {
        PauseTicking();
        response.Message = "Ticking paused";
    } else if (control.Action == "resume") {
        ResumeTicking();
        response.Message = "Ticking resumed";
    } else if (control.Action == "step") {
        StepOneTick();
        session->SetStepping(true);
        response.Message = "Step requested";
    } else if (control.Action == "step_mode") {
        bool enable = (control.Value == "on" || control.Value == "true");
        session->SetStepping(enable);
        response.Message = enable ? "Step mode enabled" : "Step mode disabled";
    } else if (control.Action == "wait_tick") {
        try {
            size_t targetTick = std::stoull(control.Value);
            session->SetWaitingForTick(true, targetTick);
            response.Message = "Waiting for tick " + std::to_string(targetTick);
        } catch (const std::exception &) {
            response.Success = false;
            response.Message = "Invalid tick number";
        }
    } else {
        response.Success = false;
        response.Message = "Unknown control command";
    }

    repl::Message msg;
    msg.Type = repl::MessageType::CONTROL;
    msg.SetPayload(response);
    session->SendMessage(msg);
}

void LuaREPLServer::OnClientDisconnect(std::shared_ptr<ClientSession> session) {
    std::lock_guard<std::mutex> lock(m_ClientsMutex);
    auto it = std::find(m_Clients.begin(), m_Clients.end(), session);
    if (it != m_Clients.end()) {
        m_Clients.erase(it);
        if (m_Engine) {
            Log::Info("REPL client %s removed", session->GetEndpoint().c_str());
        }
    }
}

std::pair<bool, std::string> LuaREPLServer::ExecuteLuaCommand(const std::string &code, size_t executionTick) {
    try {
        auto *contextManager = m_Engine->GetScriptContextManager();
        if (!contextManager) {
            return {false, "Script context manager not available"};
        }

        auto globalContext = contextManager->GetOrCreateGlobalContext();
        if (!globalContext) {
            return {false, "Global script context unavailable"};
        }

        auto &lua = globalContext->GetLuaState();

        // Add tick context
        lua["_repl_current_tick"] = executionTick;
        lua["_repl_paused"] = m_TickingPaused.load();

        // Try as statement first
        auto result = lua.safe_script(code, &sol::script_pass_on_error);

        if (!result.valid()) {
            // Try as expression with return
            std::string expr = "return " + code;
            result = lua.safe_script(expr, &sol::script_pass_on_error);
        }

        if (result.valid()) {
            if (result.get_type() == sol::type::none) {
                return {true, ""};
            } else {
                return {true, FormatLuaValue(result)};
            }
        } else {
            sol::error err = result;
            return {false, err.what()};
        }
    } catch (const std::exception &e) {
        return {false, e.what()};
    }
}

void LuaREPLServer::ScheduleCommand(const repl::REPLCommand &command, std::shared_ptr<ClientSession> session) {
    // Validate command size to prevent abuse
    if (command.LuaCode.size() > 8192) {
        // 8KB limit
        repl::CommandResult errorResult;
        errorResult.CommandId = command.Id;
        errorResult.Success = false;
        errorResult.Error = "Command too large (max 8KB)";
        errorResult.ExecutionTick = m_CurrentTick;
        SendResultToClient(std::weak_ptr<ClientSession>(session), errorResult);
        return;
    }

    ScheduledCommand scheduledCmd(command, std::weak_ptr<ClientSession>(session));

    switch (command.Mode) {
    case repl::CommandExecutionMode::IMMEDIATE:
    case repl::CommandExecutionMode::PAUSED_ONLY: {
        if (command.Mode == repl::CommandExecutionMode::PAUSED_ONLY && !m_TickingPaused.load()) {
            repl::CommandResult errorResult;
            errorResult.CommandId = command.Id;
            errorResult.Success = false;
            errorResult.Error = "Command requires paused state";
            errorResult.ExecutionTick = m_CurrentTick;
            SendResultToClient(std::weak_ptr<ClientSession>(session), errorResult);
            return;
        }

        std::lock_guard<std::mutex> lock(m_ImmediateCommandsMutex);
        if (m_ImmediateCommands.size() < MAX_COMMAND_QUEUE_SIZE) {
            m_ImmediateCommands.push(std::move(scheduledCmd));
        } else {
            // Queue full, send error
            repl::CommandResult errorResult;
            errorResult.CommandId = command.Id;
            errorResult.Success = false;
            errorResult.Error = "Command queue full";
            errorResult.ExecutionTick = m_CurrentTick;
            SendResultToClient(std::weak_ptr<ClientSession>(session), errorResult);
        }
        break;
    }
    case repl::CommandExecutionMode::NEXT_TICK: {
        std::lock_guard<std::mutex> lock(m_ScheduledCommandsMutex);
        auto &tickCommands = m_ScheduledCommands[m_CurrentTick + 1];
        if (tickCommands.size() < MAX_COMMAND_QUEUE_SIZE) {
            tickCommands.push_back(std::move(scheduledCmd));
        } else {
            // Queue full for this tick
            repl::CommandResult errorResult;
            errorResult.CommandId = command.Id;
            errorResult.Success = false;
            errorResult.Error = "Tick command queue full";
            errorResult.ExecutionTick = m_CurrentTick;
            SendResultToClient(std::weak_ptr<ClientSession>(session), errorResult);
        }
        break;
    }
    case repl::CommandExecutionMode::SPECIFIC_TICK: {
        if (command.TargetTick < m_CurrentTick) {
            // Can't schedule for past ticks
            repl::CommandResult errorResult;
            errorResult.CommandId = command.Id;
            errorResult.Success = false;
            errorResult.Error = "Cannot schedule command for past tick";
            errorResult.ExecutionTick = m_CurrentTick;
            SendResultToClient(std::weak_ptr<ClientSession>(session), errorResult);
            return;
        }

        std::lock_guard<std::mutex> lock(m_ScheduledCommandsMutex);
        auto &tickCommands = m_ScheduledCommands[command.TargetTick];
        if (tickCommands.size() < MAX_COMMAND_QUEUE_SIZE) {
            tickCommands.push_back(std::move(scheduledCmd));
        } else {
            // Queue full for this tick
            repl::CommandResult errorResult;
            errorResult.CommandId = command.Id;
            errorResult.Success = false;
            errorResult.Error = "Tick command queue full";
            errorResult.ExecutionTick = m_CurrentTick;
            SendResultToClient(std::weak_ptr<ClientSession>(session), errorResult);
        }
        break;
    }
    }
}

void LuaREPLServer::SendResultToClient(std::weak_ptr<ClientSession> weakSession, const repl::CommandResult &result) {
    if (auto session = weakSession.lock()) {
        repl::Message msg;
        msg.Type = repl::MessageType::RESULT;
        msg.SetPayload(result);
        session->SendMessage(msg);
    }
}

void LuaREPLServer::BroadcastTickNotification(size_t tick) {
    repl::TickNotification notification;
    notification.Tick = tick;
    notification.Paused = m_TickingPaused.load();

    repl::Message msg;
    msg.Type = repl::MessageType::TICK_NOTIFICATION;
    msg.SetPayload(notification);

    std::lock_guard<std::mutex> lock(m_ClientsMutex);
    for (auto &client : m_Clients) {
        if (client->IsAuthenticated() && (client->IsStepping() || client->IsWaitingForTick())) {
            // Check if wait condition is met
            bool waitComplete = false;
            if (client->IsWaitingForTick() && tick >= client->GetWaitTargetTick()) {
                client->SetWaitingForTick(false);
                waitComplete = true;
            }

            // Create notification with wait complete status
            if (waitComplete) {
                repl::TickNotification specialNotification = notification;
                specialNotification.WaitComplete = true;
                repl::Message specialMsg;
                specialMsg.Type = repl::MessageType::TICK_NOTIFICATION;
                specialMsg.SetPayload(specialNotification);
                client->SendMessage(specialMsg);
            } else {
                client->SendMessage(msg);
            }
        }
    }
}

void LuaREPLServer::DoAccept() {
    if (m_ShouldStop.load()) {
        return;
    }

    m_Acceptor->async_accept(
        [this](boost::system::error_code ec, TcpSocket socket) {
            if (!ec && !m_ShouldStop.load()) {
                // Check client limit
                {
                    std::lock_guard<std::mutex> lock(m_ClientsMutex);
                    if (m_Clients.size() >= MAX_CLIENTS) {
                        if (m_Engine) {
                            Log::Warn("REPL client limit reached, rejecting connection");
                        }
                        socket.close();
                        DoAccept(); // Continue accepting
                        return;
                    }
                }

                try {
                    auto session = std::make_shared<ClientSession>(std::move(socket), this);

                    {
                        std::lock_guard<std::mutex> lock(m_ClientsMutex);
                        m_Clients.push_back(session);
                    }

                    if (m_Engine) {
                        Log::Info("REPL client connected: %s", session->GetEndpoint().c_str());
                    }

                    session->Start();
                } catch (const std::exception &e) {
                    if (m_Engine) {
                        Log::Error("Failed to create client session: %s", e.what());
                    }
                }
            }

            if (!m_ShouldStop.load()) {
                DoAccept(); // Continue accepting connections
            }
        }
    );
}

void LuaREPLServer::RunIOContext() {
    try {
        if (m_Engine) {
            Log::Info("REPL I/O thread started");
        }

        m_IOContext->run();

        if (m_Engine) {
            Log::Info("REPL I/O thread stopped");
        }
    } catch (const std::exception &e) {
        if (m_Engine) {
            Log::Error("REPL I/O context error: %s", e.what());
        }
    }
}

std::string LuaREPLServer::FormatLuaValue(const sol::object &obj) {
    switch (obj.get_type()) {
    case sol::type::lua_nil:
        return "nil";
    case sol::type::boolean:
        return obj.as<bool>() ? "true" : "false";
    case sol::type::number: {
        double num = obj.as<double>();
        // Check if it's an integer
        if (num == std::floor(num) && num >= std::numeric_limits<int64_t>::min() && num <= std::numeric_limits<
            int64_t>::max()) {
            return std::to_string(static_cast<int64_t>(num));
        } else {
            return std::to_string(num);
        }
    }
    case sol::type::string:
        return obj.as<std::string>();
    case sol::type::table: {
        std::ostringstream oss;
        oss << "{ ";
        sol::table table = obj.as<sol::table>();
        bool first = true;
        size_t count = 0;

        try {
            for (const auto &pair : table) {
                if (!first) oss << ", ";
                oss << FormatLuaValue(pair.first) << " = " << FormatLuaValue(pair.second);
                first = false;
                if (++count > 20) {
                    oss << ", ... (truncated)";
                    break;
                }
            }
        } catch (const std::exception &) {
            oss << "error formatting table";
        }

        oss << " }";
        return oss.str();
    }
    case sol::type::function:
        return "[function]";
    case sol::type::userdata:
        return "[userdata]";
    case sol::type::lightuserdata:
        return "[lightuserdata]";
    case sol::type::thread:
        return "[thread]";
    default:
        return "[unknown]";
    }
}

#endif // ENABLE_REPL
