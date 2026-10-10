/**
 * @file ValidationComparison.cpp
 * @brief Dependency-light frame comparison used by validation and regression tests.
 */

#include "ValidationService.h"

#include <algorithm>
#include <cmath>
#include <limits>

bool ValidationService::PhysicsDiverges(const PhysicsData &live, const PhysicsData &reference,
                                        const ValidationConfig &config,
                                        float &positionError, float &velocityError) {
    positionError = (live.position - reference.position).Magnitude();
    velocityError = (live.velocity - reference.velocity).Magnitude();
    if (!std::isfinite(positionError)) {
        positionError = std::numeric_limits<float>::infinity();
    }
    if (!std::isfinite(velocityError)) {
        velocityError = std::numeric_limits<float>::infinity();
    }
    if (config.exact) {
        return live.position.x != reference.position.x ||
               live.position.y != reference.position.y ||
               live.position.z != reference.position.z ||
               live.velocity.x != reference.velocity.x ||
               live.velocity.y != reference.velocity.y ||
               live.velocity.z != reference.velocity.z;
    }
    const float positionEpsilon = std::isfinite(config.positionEpsilon)
                                      ? std::max(0.0f, config.positionEpsilon)
                                      : 0.0f;
    const float velocityEpsilon = std::isfinite(config.velocityEpsilon)
                                      ? std::max(0.0f, config.velocityEpsilon)
                                      : 0.0f;
    return positionError > positionEpsilon || velocityError > velocityEpsilon;
}

ValidationReport ValidationService::CompareFrameData(const std::vector<FrameData> &live,
                                                     const std::vector<FrameData> &reference,
                                                     const ValidationConfig &config) {
    ValidationReport report;
    report.hasReference = true;
    report.referenceFrames = reference.size();
    report.liveFrames = live.size();
    report.comparedFrames = std::min(live.size(), reference.size());

    for (size_t index = 0; index < report.comparedFrames; ++index) {
        float positionError = 0.0f;
        float velocityError = 0.0f;
        const bool diverged = PhysicsDiverges(live[index].physics, reference[index].physics,
                                              config, positionError, velocityError);
        const size_t frame = live[index].frameIndex;

        if (positionError > report.maxPositionError) {
            report.maxPositionError = positionError;
            report.maxPositionFrame = frame;
        }
        if (velocityError > report.maxVelocityError) {
            report.maxVelocityError = velocityError;
            report.maxVelocityFrame = frame;
        }

        if (live[index].frameIndex != reference[index].frameIndex || diverged) {
            if (report.divergentFrames == 0) {
                report.firstDivergenceFrame = frame;
            }
            ++report.divergentFrames;
        }
    }

    if (live.size() != reference.size()) {
        if (report.divergentFrames == 0) {
            const auto &longer = live.size() > reference.size() ? live : reference;
            report.firstDivergenceFrame = longer[report.comparedFrames].frameIndex;
        }
        report.divergentFrames += live.size() > reference.size()
                                      ? live.size() - reference.size()
                                      : reference.size() - live.size();
    }

    report.identical = report.comparedFrames > 0 &&
                       report.divergentFrames == 0 &&
                       live.size() == reference.size();
    return report;
}
