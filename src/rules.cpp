#include "model.h"
namespace aa {
bool evaluate(const Rule& rule, const Observation& observation, std::int64_t nowMs,
              int validityMs, std::uint64_t source) {
    if (!rule.enabled || source == 0 || observation.source != source || validityMs <= 0 ||
        observation.capturedMs <= 0 || nowMs < observation.capturedMs ||
        nowMs - observation.capturedMs >= validityMs)
        return false;

    const auto& detection = observation.detection;
    switch (rule.condition) {
    case Condition::StacksEqual:
        return detection.presence == Presence::Present && rule.stacks <= 4 &&
               detection.stacks && *detection.stacks == rule.stacks;
    case Condition::Present:
        return detection.presence == Presence::Present;
    case Condition::Absent:
        return detection.presence == Presence::Absent;
    default:
        return false;
    }
}
}
