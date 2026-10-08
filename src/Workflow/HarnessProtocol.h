#pragma once

#include <cstddef>
#include <string>
#include <string_view>

struct HarnessRequest {
    unsigned int protocol = 1;
    std::string project;
    size_t maxTicks = 20000;
};

bool ParseHarnessRequest(std::string_view json, HarnessRequest &request, std::string &error);

std::string BuildHarnessResult(bool passed,
                               const std::string &project,
                               const std::string &message,
                               size_t tick);
