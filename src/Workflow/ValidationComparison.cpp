/**
 * @file ValidationComparison.cpp
 * @brief Dependency-light frame comparison used by validation and regression tests.
 */

#include "ValidationService.h"

#include <algorithm>
#include <cmath>
#include <limits>

ValidationReport ValidationService::CompareFrameData(const std::vector<FrameData> &live,
                                                     const std::vector<FrameData> &reference,
                                                     const ValidationConfig &config) {
    ValidationReport report;
    report.hasReference = true;
    report.referenceFrames = reference.size();
    report.liveFrames = live.size();
    report.comparedFrames = std::min(live.size(), reference.size());

    const float positionEpsilon = std::isfinite(config.positionEpsilon)
                                      ? std::max(0.0f, config.positionEpsilon)
                                      : 0.0f;
    const float velocityEpsilon = std::isfinite(config.velocityEpsilon)
                                      ? std::max(0.0f, config.velocityEpsilon)
                                      : 0.0f;

    for (size_t index = 0; index < report.comparedFrames; ++index) {
        const auto &livePhysics = live[index].physics;
        const auto &referencePhysics = reference[index].physics;

        float positionError =
            (livePhysics.position - referencePhysics.position).Magnitude();
        float velocityError =
            (livePhysics.velocity - referencePhysics.velocity).Magnitude();
        if (!std::isfinite(positionError)) {
            positionError = std::numeric_limits<float>::infinity();
        }
        if (!std::isfinite(velocityError)) {
            velocityError = std::numeric_limits<float>::infinity();
        }
        const size_t frame = live[index].frameIndex;

        if (positionError > report.maxPositionError) {
            report.maxPositionError = positionError;
            report.maxPositionFrame = frame;
        }
        if (velocityError > report.maxVelocityError) {
            report.maxVelocityError = velocityError;
            report.maxVelocityFrame = frame;
        }

        if (live[index].frameIndex != reference[index].frameIndex ||
            positionError > positionEpsilon || velocityError > velocityEpsilon) {
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
