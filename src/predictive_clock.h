#pragma once
#include "model.h"
#include <array>
namespace aa {
struct ClockPrediction { std::optional<float> fraction; bool estimated=false; };
class PredictiveClock {
public:
    ClockPrediction update(const Observation&,std::int64_t nowMs,int validityMs,std::uint64_t source);
    void reset();
private:
    void clearCycle();
    ClockPrediction render(std::int64_t nowMs) const;
    std::uint64_t source_=0;
    std::int64_t lastFrame_=0,lastNow_=0,readTime_=0,anchorTime_=0;
    bool haveFrame_=false,present_=false,observed_=false;
    std::optional<float> read_,anchor_;
    float start_=0,minimum_=1,speed_=0;
    unsigned intervals_=0;
    std::array<float,3> endings_{};
    unsigned endingCount_=0;
    std::optional<float> endpoint_;
};
}
