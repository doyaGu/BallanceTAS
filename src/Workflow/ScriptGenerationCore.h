#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "ScriptGenerator.h"

struct ScriptGenerationStatsView {
    size_t totalFrames = 0;
    size_t totalBlocks = 0;
    size_t keyEvents = 0;
};

std::string BuildGeneratedLuaScript(const std::vector<FrameData> &frames,
                                    const std::vector<InputBlock> &blocks,
                                    const GenerationOptions &options,
                                    ScriptGenerationStatsView stats);

std::string BuildGeneratedLuaScriptForTesting(const std::vector<FrameData> &frames,
                                              const GenerationOptions &options);

std::string BuildGeneratedManifest(const GenerationOptions &options,
                                   ScriptGenerationStatsView stats);

std::string BuildGeneratedManifestForTesting(const GenerationOptions &options,
                                             ScriptGenerationStatsView stats);
