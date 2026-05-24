#include "RuntimeSession.h"

#include <chrono>
#include <utility>

#include "EventBus.h"
#include "GameEvents.h"

RuntimeSession::RuntimeSession() {
    InitializeTransitionTable();
}

RuntimeSession::RuntimeSession(EventBus *eventBus, Hooks hooks)
    : m_EventBus(eventBus), m_Hooks(std::move(hooks)) {
    InitializeTransitionTable();
}

void RuntimeSession::InitializeTransitionTable() {
    m_TransitionTable[{State::Idle, Event::StartRecording}] = State::PendingRecord;
    m_TransitionTable[{State::Idle, Event::StartScriptPlayback}] = State::PendingScriptPlayback;
    m_TransitionTable[{State::Idle, Event::StartRecordPlayback}] = State::PendingRecordPlayback;
    m_TransitionTable[{State::Idle, Event::StartTranslation}] = State::PendingTranslation;

    m_TransitionTable[{State::PendingRecord, Event::LevelLoadStart}] = State::Recording;
    m_TransitionTable[{State::PendingScriptPlayback, Event::LevelLoadStart}] = State::PlayingScript;
    m_TransitionTable[{State::PendingScriptPlayback, Event::LevelStart}] = State::PlayingScript;
    m_TransitionTable[{State::PendingRecordPlayback, Event::LevelLoadStart}] = State::PlayingRecord;
    m_TransitionTable[{State::PendingTranslation, Event::LevelLoadStart}] = State::Translating;

    m_TransitionTable[{State::PendingRecord, Event::Stop}] = State::Idle;
    m_TransitionTable[{State::PendingScriptPlayback, Event::Stop}] = State::Idle;
    m_TransitionTable[{State::PendingRecordPlayback, Event::Stop}] = State::Idle;
    m_TransitionTable[{State::PendingTranslation, Event::Stop}] = State::Idle;

    m_TransitionTable[{State::Recording, Event::Stop}] = State::Idle;
    m_TransitionTable[{State::PlayingScript, Event::Stop}] = State::Idle;
    m_TransitionTable[{State::PlayingRecord, Event::Stop}] = State::Idle;
    m_TransitionTable[{State::Translating, Event::Stop}] = State::Idle;
    m_TransitionTable[{State::Paused, Event::Stop}] = State::Idle;

    m_TransitionTable[{State::PlayingScript, Event::Pause}] = State::Paused;
    m_TransitionTable[{State::PlayingRecord, Event::Pause}] = State::Paused;

    for (auto state : {
             State::PendingRecord, State::PendingScriptPlayback,
             State::PendingRecordPlayback, State::PendingTranslation,
             State::Recording, State::PlayingScript, State::PlayingRecord,
             State::Translating, State::Paused
         }) {
        m_TransitionTable[{state, Event::LevelEnd}] = State::Idle;
        m_TransitionTable[{state, Event::Error}] = State::Idle;
    }

    for (auto state : {
             State::Idle,
             State::PendingRecord, State::PendingScriptPlayback,
             State::PendingRecordPlayback, State::PendingTranslation,
             State::Recording, State::PlayingScript, State::PlayingRecord,
             State::Translating, State::Paused
         }) {
        m_TransitionTable[{state, Event::Shutdown}] = State::ShuttingDown;
    }
}

Result<void> RuntimeSession::StartRecording(RecordingOptions options) {
    m_RecordingValidation = options.validation;
    m_Project = nullptr;
    m_RequestedPlaybackType = PlaybackType::None;
    return Transition(Event::StartRecording);
}

Result<void> RuntimeSession::StartPlayback(TASProject *project, PlaybackType type, PlaybackOptions options) {
    if (type == PlaybackType::None) {
        return Result<void>::Error("Invalid playback type", "runtime_session", ErrorSeverity::Warning);
    }

    m_Project = project;
    m_RequestedPlaybackType = type;
    m_PlaybackValidation = options.validation;
    return Transition(type == PlaybackType::Script
                          ? Event::StartScriptPlayback
                          : Event::StartRecordPlayback);
}

Result<void> RuntimeSession::StartTranslation(TASProject *project, TranslationOptions) {
    m_Project = project;
    m_RequestedPlaybackType = PlaybackType::None;
    return Transition(Event::StartTranslation);
}

Result<void> RuntimeSession::Stop(StopOptions options) {
    return Transition(Event::Stop, options);
}

Result<void> RuntimeSession::Pause() {
    return Transition(Event::Pause);
}

Result<void> RuntimeSession::Resume() {
    return Transition(Event::Resume);
}

Result<void> RuntimeSession::Shutdown() {
    return Transition(Event::Shutdown);
}

Result<void> RuntimeSession::OnLevelLoadStart() {
    return Transition(Event::LevelLoadStart);
}

Result<void> RuntimeSession::OnLevelStart() {
    auto result = Transition(Event::LevelStart);
    if (!result.IsOk() && result.GetError().severity != ErrorSeverity::Warning) {
        return result;
    }

    if (!m_PlaybackValidation || GetPlaybackType() != PlaybackType::Script || !IsPlaying()) {
        return Result<void>::Ok();
    }
    if (m_Hooks.isValidationActive && m_Hooks.isValidationActive()) {
        return Result<void>::Ok();
    }
    if (m_Hooks.startValidationForPlayback) {
        auto validationResult = m_Hooks.startValidationForPlayback(
            m_Hooks.currentPlaybackProject ? m_Hooks.currentPlaybackProject() : m_Project);
        if (!validationResult.IsOk()) {
            return validationResult;
        }
    }
    return Result<void>::Ok();
}

Result<void> RuntimeSession::OnLevelEnd(StopOptions options) {
    return Transition(Event::LevelEnd, options);
}

Result<void> RuntimeSession::OnPlaybackCompleted(PlaybackType completedType) {
    if (completedType != GetPlaybackType()) {
        return Result<void>::Ok();
    }
    if (!IsPlaying() && !IsPaused()) {
        return Result<void>::Ok();
    }
    return Stop({false});
}

Result<void> RuntimeSession::OnTranslationCompleted() {
    if (!IsTranslating()) {
        return Result<void>::Ok();
    }
    return Stop({false});
}

RuntimeSession::SessionSnapshot RuntimeSession::SnapshotState() const {
    SessionSnapshot snapshot;
    snapshot.state = m_State;
    snapshot.pausedFromState = m_PausedFromState;
    snapshot.playbackType = GetPlaybackType();
    snapshot.isIdle = IsIdle();
    snapshot.isRecording = IsRecording();
    snapshot.isPlaying = IsPlaying();
    snapshot.isTranslating = IsTranslating();
    snapshot.isPaused = IsPaused();
    snapshot.isShuttingDown = IsShuttingDown();
    snapshot.isPending = IsPending();
    snapshot.isPendingRecord = IsPendingRecord();
    snapshot.isPendingPlay = IsPendingPlay();
    snapshot.isPendingTranslate = IsPendingTranslate();
    return snapshot;
}

PlaybackType RuntimeSession::GetPlaybackType() const {
    switch (m_State) {
    case State::PendingScriptPlayback:
    case State::PlayingScript:
        return PlaybackType::Script;
    case State::PendingRecordPlayback:
    case State::PlayingRecord:
        return PlaybackType::Record;
    case State::Paused:
        if (m_PausedFromState == State::PlayingScript) {
            return PlaybackType::Script;
        }
        if (m_PausedFromState == State::PlayingRecord) {
            return PlaybackType::Record;
        }
        break;
    default:
        break;
    }
    return m_RequestedPlaybackType;
}

bool RuntimeSession::IsPending() const {
    return m_State == State::PendingRecord ||
           m_State == State::PendingScriptPlayback ||
           m_State == State::PendingRecordPlayback ||
           m_State == State::PendingTranslation;
}

Result<void> RuntimeSession::Transition(Event event, StopOptions stopOptions) {
    const State requestedTargetState = FindTransitionTarget(m_State, event);
    if (requestedTargetState == m_State) {
        RecordTransition(m_State, event, m_State, false);
        return Result<void>::Error(
            std::string("Invalid runtime session transition: ") +
            StateToString(m_State) + " -> " + EventToString(event),
            "runtime_session",
            ErrorSeverity::Warning);
    }

    const State oldState = m_State;
    auto result = TransitionToState(requestedTargetState, stopOptions);
    if (result.IsOk()) {
        RecordTransition(oldState, event, m_State, true);
    } else {
        RecordTransition(oldState, event, requestedTargetState, false);
    }
    return result;
}

Result<void> RuntimeSession::TransitionToState(State nextState, StopOptions stopOptions) {
    const State oldState = m_State;
    auto exitResult = ExitState(oldState, nextState, stopOptions);
    if (!exitResult.IsOk()) {
        return exitResult;
    }

    if (nextState == State::Paused) {
        m_PausedFromState = oldState;
    }

    m_State = nextState;
    auto enterResult = EnterState(nextState, oldState);
    if (!enterResult.IsOk()) {
        m_State = State::Idle;
        return enterResult;
    }

    PublishStateChanged(oldState, m_State);
    return Result<void>::Ok();
}

RuntimeSession::State RuntimeSession::FindTransitionTarget(State state, Event event) const {
    if (state == State::Paused && event == Event::Resume) {
        return m_PausedFromState;
    }

    auto it = m_TransitionTable.find({state, event});
    return it == m_TransitionTable.end() ? state : it->second;
}

Result<void> RuntimeSession::EnterState(State state, State previousState) {
    Result<void> result = Result<void>::Ok();
    switch (state) {
    case State::PendingRecord:
        if (m_Hooks.prepareRecording) {
            result = m_Hooks.prepareRecording({m_RecordingValidation});
        }
        break;
    case State::Recording:
        if (m_Hooks.activateRecording) {
            result = m_Hooks.activateRecording();
        }
        break;
    case State::PendingScriptPlayback:
        if (m_Hooks.preparePlayback) {
            result = m_Hooks.preparePlayback(m_Project, PlaybackType::Script);
        }
        break;
    case State::PendingRecordPlayback:
        if (m_Hooks.preparePlayback) {
            result = m_Hooks.preparePlayback(m_Project, PlaybackType::Record);
        }
        break;
    case State::PlayingScript:
    case State::PlayingRecord:
        if (previousState == State::Paused) {
            if (m_Hooks.resumePlayback) {
                m_Hooks.resumePlayback();
            }
        } else if (m_Hooks.activatePlayback) {
            result = m_Hooks.activatePlayback();
        }
        break;
    case State::PendingTranslation:
        if (m_Hooks.prepareTranslation) {
            result = m_Hooks.prepareTranslation(m_Project);
        }
        break;
    case State::Translating:
        if (m_Hooks.activateTranslation) {
            result = m_Hooks.activateTranslation();
        }
        break;
    case State::Paused:
        if (m_Hooks.pausePlayback) {
            m_Hooks.pausePlayback();
        }
        break;
    case State::ShuttingDown:
        if (m_Hooks.stopValidationImmediate) {
            m_Hooks.stopValidationImmediate();
        }
        if (m_Hooks.stopPlaybackImmediate) {
            m_Hooks.stopPlaybackImmediate();
        }
        if (m_Hooks.stopRecordingImmediate) {
            m_Hooks.stopRecordingImmediate();
        }
        if (m_Hooks.stopTranslationImmediate) {
            m_Hooks.stopTranslationImmediate();
        }
        break;
    case State::Idle:
        m_Project = nullptr;
        m_RequestedPlaybackType = PlaybackType::None;
        m_RecordingValidation = false;
        m_PlaybackValidation = false;
        break;
    }
    if (!result.IsOk()) {
        return result;
    }
    if (m_Hooks.onStateEntered) {
        m_Hooks.onStateEntered(state, previousState);
    }
    return result;
}

Result<void> RuntimeSession::ExitState(State state, State nextState, StopOptions stopOptions) {
    switch (state) {
    case State::PendingRecord:
        if (nextState != State::Recording) {
            if ((!m_Hooks.isRecordingPrepared || m_Hooks.isRecordingPrepared()) &&
                m_Hooks.stopRecordingGraceful) {
                return m_Hooks.stopRecordingGraceful();
            }
        }
        break;
    case State::Recording:
        if (nextState == State::ShuttingDown) {
            return Result<void>::Ok();
        } else if ((!m_Hooks.isRecordingActive || m_Hooks.isRecordingActive()) &&
                   m_Hooks.stopRecordingGraceful) {
            return m_Hooks.stopRecordingGraceful();
        }
        break;
    case State::PendingScriptPlayback:
    case State::PendingRecordPlayback:
        if (nextState != State::PlayingScript && nextState != State::PlayingRecord) {
            if ((!m_Hooks.isPlaybackPrepared || m_Hooks.isPlaybackPrepared()) &&
                m_Hooks.stopPlaybackGraceful) {
                return m_Hooks.stopPlaybackGraceful(stopOptions.clearProject);
            }
        }
        break;
    case State::PlayingScript:
    case State::PlayingRecord:
        if (nextState == State::Paused) {
            return Result<void>::Ok();
        }
        if (nextState == State::ShuttingDown) {
            return Result<void>::Ok();
        } else if ((!m_Hooks.isPlaybackActiveOrPaused || m_Hooks.isPlaybackActiveOrPaused())) {
            if (GetPlaybackType() == PlaybackType::Script &&
                m_Hooks.isValidationActive && m_Hooks.isValidationActive() &&
                m_Hooks.stopValidationGraceful) {
                auto validationResult = m_Hooks.stopValidationGraceful();
                if (!validationResult.IsOk()) {
                    return validationResult;
                }
            }
            if (m_Hooks.stopPlaybackGraceful) {
                return m_Hooks.stopPlaybackGraceful(stopOptions.clearProject);
            }
        }
        break;
    case State::Paused:
        if (nextState != State::PlayingScript && nextState != State::PlayingRecord) {
            if (nextState == State::ShuttingDown) {
                return Result<void>::Ok();
            } else {
                if (m_Hooks.isValidationActive && m_Hooks.isValidationActive() &&
                    m_Hooks.stopValidationGraceful) {
                    auto validationResult = m_Hooks.stopValidationGraceful();
                    if (!validationResult.IsOk()) {
                        return validationResult;
                    }
                }
                if (m_Hooks.stopPlaybackGraceful) {
                    return m_Hooks.stopPlaybackGraceful(stopOptions.clearProject);
                }
            }
        }
        break;
    case State::PendingTranslation:
        if (nextState != State::Translating) {
            if ((!m_Hooks.isTranslationPrepared || m_Hooks.isTranslationPrepared()) &&
                m_Hooks.stopTranslationGraceful) {
                return m_Hooks.stopTranslationGraceful(stopOptions.clearProject);
            }
        }
        break;
    case State::Translating:
        if (nextState == State::ShuttingDown) {
            return Result<void>::Ok();
        } else if ((!m_Hooks.isTranslationActive || m_Hooks.isTranslationActive()) &&
                   m_Hooks.stopTranslationGraceful) {
            return m_Hooks.stopTranslationGraceful(stopOptions.clearProject);
        }
        break;
    default:
        break;
    }
    return Result<void>::Ok();
}

void RuntimeSession::RecordTransition(State fromState, Event event, State toState, bool succeeded) {
    const auto now = std::chrono::system_clock::now();
    const auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    m_History.push_back({
        fromState,
        event,
        toState,
        static_cast<uint64_t>(timestamp),
        succeeded
    });

    if (m_History.size() > MAX_HISTORY_SIZE) {
        m_History.erase(m_History.begin());
    }
}

void RuntimeSession::PublishStateChanged(State oldState, State newState) {
    if (!m_EventBus) {
        return;
    }
    m_EventBus->Publish(TASStateChangedEvent{
        static_cast<int>(oldState),
        static_cast<int>(newState)
    });
}

const char *RuntimeSession::StateToString(State state) {
    switch (state) {
    case State::Idle: return "Idle";
    case State::PendingRecord: return "PendingRecord";
    case State::Recording: return "Recording";
    case State::PendingScriptPlayback: return "PendingScriptPlayback";
    case State::PendingRecordPlayback: return "PendingRecordPlayback";
    case State::PlayingScript: return "PlayingScript";
    case State::PlayingRecord: return "PlayingRecord";
    case State::PendingTranslation: return "PendingTranslation";
    case State::Translating: return "Translating";
    case State::Paused: return "Paused";
    case State::ShuttingDown: return "ShuttingDown";
    default: return "Unknown";
    }
}

const char *RuntimeSession::EventToString(Event event) {
    switch (event) {
    case Event::StartRecording: return "StartRecording";
    case Event::StartScriptPlayback: return "StartScriptPlayback";
    case Event::StartRecordPlayback: return "StartRecordPlayback";
    case Event::StartTranslation: return "StartTranslation";
    case Event::Stop: return "Stop";
    case Event::Pause: return "Pause";
    case Event::Resume: return "Resume";
    case Event::LevelLoadStart: return "LevelLoadStart";
    case Event::LevelStart: return "LevelStart";
    case Event::LevelEnd: return "LevelEnd";
    case Event::Shutdown: return "Shutdown";
    case Event::Error: return "Error";
    default: return "Unknown";
    }
}
