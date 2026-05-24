#include "ScriptGenerationCore.h"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <set>
#include <sstream>
#include <variant>

class LuaScriptBuilder {
public:
    explicit LuaScriptBuilder(const GenerationOptions &options) : m_Options(options) {
        UpdateIndent();
    }

    void Indent() {
        ++m_IndentLevel;
        UpdateIndent();
    }

    void Unindent() {
        if (m_IndentLevel > 0) {
            --m_IndentLevel;
            UpdateIndent();
        }
    }

    void AddLine(const std::string &line) {
        m_SS << m_CurrentIndent << line << "\n";
    }

    void AddComment(const std::string &comment) {
        m_SS << m_CurrentIndent << "-- " << comment << "\n";
    }

    void AddBlankLine() {
        m_SS << "\n";
    }

    void AddSeparator(const std::string &title = "") {
        if (!m_Options.addSectionSeparators) {
            return;
        }

        AddBlankLine();
        AddComment(std::string(60, '='));
        if (!title.empty()) {
            AddComment(title);
            AddComment(std::string(60, '='));
        }
        AddBlankLine();
    }

    void AddMainFunction() {
        AddComment("Main TAS function - called when the script starts");
        AddLine("function main()");
        Indent();
    }

    void CloseMainFunction() {
        AddBlankLine();
        AddComment("Script completed successfully");
        AddLine("tas.log(\"TAS script completed.\")");
        Unindent();
        AddLine("end");
    }

    std::string GetScript() const {
        return m_SS.str();
    }

private:
    void UpdateIndent() {
        m_CurrentIndent = std::string(static_cast<size_t>(m_IndentLevel * m_Options.indentSize), ' ');
    }

    std::stringstream m_SS;
    int m_IndentLevel = 0;
    std::string m_CurrentIndent;
    const GenerationOptions &m_Options;
};

static int TransitionOrder(KeyTransition transition) {
    switch (transition) {
    case KeyTransition::Pressed: return 0;
    case KeyTransition::PressedAndReleased: return 1;
    case KeyTransition::Released: return 2;
    default: return 3;
    }
}

static std::vector<KeyEvent> NormalizeKeyEvents(std::vector<KeyEvent> events) {
    std::sort(events.begin(), events.end(), [](const KeyEvent &a, const KeyEvent &b) {
        if (a.frame != b.frame) {
            return a.frame < b.frame;
        }
        if (a.key != b.key) {
            return a.key < b.key;
        }
        return TransitionOrder(a.transition) < TransitionOrder(b.transition);
    });

    std::vector<KeyEvent> normalized;
    normalized.reserve(events.size());

    for (size_t i = 0; i < events.size(); ++i) {
        const KeyEvent &event = events[i];
        if (event.transition == KeyTransition::Pressed && i + 1 < events.size()) {
            const KeyEvent &next = events[i + 1];
            if (next.frame == event.frame &&
                next.key == event.key &&
                next.transition == KeyTransition::Released) {
                normalized.emplace_back(event.frame, event.key, KeyTransition::PressedAndReleased);
                ++i;
                continue;
            }
        }
        normalized.push_back(event);
    }

    return normalized;
}

std::string BuildGeneratedLuaScriptForTesting(const std::vector<FrameData> &frames,
                                              const GenerationOptions &options) {
    ScriptGenerationStatsView stats;
    std::vector<InputBlock> blocks;

    if (!frames.empty()) {
        InputBlock block;
        block.startFrame = frames.front().frameIndex;
        block.endFrame = frames.back().frameIndex;

        RawInputState previousState;
        for (const auto &frame : frames) {
            auto keyEvents = DetectScriptKeyTransitions(previousState, frame.inputState, frame.frameIndex);
            stats.keyEvents += keyEvents.size();
            block.keyEvents.insert(block.keyEvents.end(), keyEvents.begin(), keyEvents.end());
            block.gameEvents.insert(block.gameEvents.end(), frame.events.begin(), frame.events.end());
            previousState = frame.inputState;
        }

        blocks.push_back(std::move(block));
    }

    return BuildGeneratedLuaScript(frames, blocks, options, stats);
}

std::string BuildGeneratedManifest(const GenerationOptions &options,
                                   ScriptGenerationStatsView stats) {
    std::stringstream ss;

    ss << "-- Auto-generated manifest for " << options.projectName << "\n";
    ss << "return {\n";
    ss << "  name = \"" << options.projectName << "\",\n";
    ss << "  author = \"" << options.authorName << "\",\n";
    ss << "  level = \"" << options.targetLevel << "\",\n";
    ss << "  entry_script = \"main.lua\",\n";
    ss << "  description = \"" << options.description << "\",\n";
    ss << "  update_rate = " << options.updateRate << ",\n";
    ss << "\n";
    ss << "  -- Generation metadata\n";
    ss << "  generated_by = \"BallanceTAS ScriptGenerator\",\n";
    ss << "  generation_date = \"" << []() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream date;
        date << std::put_time(std::localtime(&time_t), "%Y-%m-%d");
        return date.str();
    }() << "\",\n";
    ss << "  key_events = " << stats.keyEvents << ",\n";
    ss << "  total_frames = " << stats.totalFrames << ",\n";
    ss << "  blocks = " << stats.totalBlocks << "\n";
    ss << "}\n";

    return ss.str();
}

std::string BuildGeneratedManifestForTesting(const GenerationOptions &options,
                                             ScriptGenerationStatsView stats) {
    return BuildGeneratedManifest(options, stats);
}

std::string BuildGeneratedLuaScript(const std::vector<FrameData> &frames,
                                    const std::vector<InputBlock> &blocks,
                                    const GenerationOptions &options,
                                    ScriptGenerationStatsView stats) {
    LuaScriptBuilder builder(options);

    builder.AddComment("TAS script for Ballance");
    builder.AddComment("Project: " + options.projectName);
    builder.AddComment("Generated on: " + []() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }());
    builder.AddComment("Total key events: " + std::to_string(stats.keyEvents));
    builder.AddSeparator();

    builder.AddMainFunction();

    std::set<std::string> currentlyPressed;
    std::vector<std::pair<size_t, std::variant<KeyEvent, GameEvent>>> allEvents;
    std::vector<KeyEvent> keyEvents;

    for (const auto &block : blocks) {
        for (const auto &keyEvent : block.keyEvents) {
            keyEvents.push_back(keyEvent);
        }
        for (const auto &gameEvent : block.gameEvents) {
            allEvents.emplace_back(gameEvent.frame, gameEvent);
        }
    }

    for (const auto &keyEvent : NormalizeKeyEvents(std::move(keyEvents))) {
        allEvents.emplace_back(keyEvent.frame, keyEvent);
    }

    std::sort(allEvents.begin(), allEvents.end(), [](const auto &a, const auto &b) {
        if (a.first == b.first) {
            const bool aIsGame = std::holds_alternative<GameEvent>(a.second);
            const bool bIsGame = std::holds_alternative<GameEvent>(b.second);
            if (aIsGame != bIsGame) {
                return aIsGame && !bIsGame;
            }
            if (!aIsGame && !bIsGame) {
                const auto &aKey = std::get<KeyEvent>(a.second);
                const auto &bKey = std::get<KeyEvent>(b.second);
                return TransitionOrder(aKey.transition) < TransitionOrder(bKey.transition);
            }
            return false;
        }
        return a.first < b.first;
    });

    size_t lastFrame = 0;
    if (!allEvents.empty() && allEvents[0].first > 0) {
        const size_t initialWait = allEvents[0].first;
        if (options.addFrameComments) {
            builder.AddComment("Wait " + std::to_string(initialWait) + " frames to start");
        }
        builder.AddLine("tas.wait_ticks(" + std::to_string(initialWait) + ")");
        lastFrame = allEvents[0].first;
    }

    for (size_t i = 0; i < allEvents.size(); ++i) {
        const auto &[frameNumber, event] = allEvents[i];

        const int64_t waitFrames = static_cast<int64_t>(frameNumber) - static_cast<int64_t>(lastFrame);
        if (waitFrames > 0) {
            if (options.addFrameComments) {
                builder.AddComment("Wait " + std::to_string(waitFrames) +
                                   " frames (to frame " + std::to_string(frameNumber) + ")");
            }
            builder.AddLine("tas.wait_ticks(" + std::to_string(waitFrames) + ")");
        }

        if (std::holds_alternative<KeyEvent>(event)) {
            const auto &keyEvent = std::get<KeyEvent>(event);
            if (keyEvent.transition == KeyTransition::Pressed) {
                currentlyPressed.insert(keyEvent.key);
                if (options.addFrameComments) {
                    builder.AddComment("Press " + keyEvent.key + " at frame " + std::to_string(keyEvent.frame));
                }
                builder.AddLine("tas.key_down(\"" + keyEvent.key + "\")");
            } else if (keyEvent.transition == KeyTransition::Released) {
                currentlyPressed.erase(keyEvent.key);
                if (options.addFrameComments) {
                    builder.AddComment("Release " + keyEvent.key + " at frame " + std::to_string(keyEvent.frame));
                }
                builder.AddLine("tas.key_up(\"" + keyEvent.key + "\")");
            } else if (keyEvent.transition == KeyTransition::PressedAndReleased) {
                if (options.addFrameComments) {
                    builder.AddComment("Press and release " + keyEvent.key +
                                       " in single frame " + std::to_string(keyEvent.frame));
                }
                builder.AddLine("tas.press(\"" + keyEvent.key + "\")");
            }
        } else {
            const auto &gameEvent = std::get<GameEvent>(event);
            if (options.addEventAnchors) {
                builder.AddComment("GAME EVENT: " + gameEvent.eventName +
                                   (gameEvent.eventData != 0 ? " (data: " + std::to_string(gameEvent.eventData) + ")" : "") +
                                   " at frame " + std::to_string(gameEvent.frame));
            }
        }

        lastFrame = frameNumber;

        if (options.addSectionSeparators && (i + 1) % 20 == 0 && i + 1 < allEvents.size()) {
            builder.AddBlankLine();
            builder.AddComment("--- Section " + std::to_string((i + 1) / 20 + 1) + " ---");
            builder.AddBlankLine();
        }
    }

    if (!frames.empty()) {
        const size_t finalRecordingFrame = frames.back().frameIndex;
        const int64_t finalWait = static_cast<int64_t>(finalRecordingFrame) - static_cast<int64_t>(lastFrame);
        if (finalWait > 0) {
            builder.AddBlankLine();
            if (options.addFrameComments) {
                builder.AddComment("Wait until end of recording (frame " + std::to_string(finalRecordingFrame) + ")");
            }
            builder.AddLine("tas.wait_ticks(" + std::to_string(finalWait) + ")");
        }

        if (!currentlyPressed.empty()) {
            builder.AddBlankLine();
            builder.AddComment("Recording ended - release all remaining pressed keys");
            for (const auto &key : currentlyPressed) {
                if (options.addFrameComments) {
                    builder.AddComment("Release " + key + " at end of recording (frame " +
                                       std::to_string(finalRecordingFrame) + ")");
                }
                builder.AddLine("tas.key_up(\"" + key + "\")");
            }
        }
    }

    builder.CloseMainFunction();
    return builder.GetScript();
}
