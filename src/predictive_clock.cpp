#include "predictive_clock.h"
#include <algorithm>
#include <cmath>
namespace aa {
void PredictiveClock::clearCycle(){
    present_=false;observed_=false;read_.reset();anchor_.reset();
    readTime_=anchorTime_=0;start_=0;minimum_=1;speed_=0;intervals_=0;
}
void PredictiveClock::reset(){*this=PredictiveClock{};}
ClockPrediction PredictiveClock::render(std::int64_t nowMs) const {
    if(!present_||!read_)return {};
    const bool predict=intervals_>=2&&speed_>0;
    if(!observed_&&!predict)return {};
    const auto elapsed=nowMs-readTime_;
    if(elapsed<0||(!observed_&&elapsed*speed_>1))return {};
    float fraction=*read_;
    if(predict)fraction-=speed_*static_cast<float>(elapsed);
    if(endpoint_)fraction=(fraction-*endpoint_)/(1-*endpoint_);
    return {std::clamp(fraction,0.f,1.f),endpoint_.has_value()||!observed_||(predict&&elapsed>0)};
}
ClockPrediction PredictiveClock::update(const Observation& observation,std::int64_t nowMs,int validityMs,std::uint64_t source){
    if(validityMs<=0||source==0||observation.source!=source||observation.capturedMs<=0||
       observation.capturedMs>nowMs||nowMs-observation.capturedMs>=validityMs){reset();return {};}
    if(haveFrame_&&(source_!=source||nowMs<lastNow_||observation.capturedMs<lastFrame_)){reset();return {};}
    if(haveFrame_&&observation.capturedMs-lastFrame_>=validityMs)clearCycle();
    lastNow_=nowMs;source_=source;
    if(haveFrame_&&observation.capturedMs==lastFrame_)return render(nowMs);
    const auto previousFrame=lastFrame_;
    lastFrame_=observation.capturedMs;haveFrame_=true;
    if(observation.detection.presence==Presence::Unknown){clearCycle();return {};}
    if(observation.detection.presence==Presence::Absent){
        if(present_&&read_&&intervals_>=2&&speed_>0&&start_>=.65f&&start_-minimum_>=.25f){
            const auto terminalTime=previousFrame+(lastFrame_-previousFrame)/2;
            const float ending=*read_-speed_*static_cast<float>(terminalTime-readTime_);
            if(ending>=0&&ending<.65f){
                endings_[endingCount_%endings_.size()]=ending;++endingCount_;
                if(endingCount_>=endings_.size()){
                    const auto [low,high]=std::minmax_element(endings_.begin(),endings_.end());
                    // ponytail: consumo recorrente pode imitar expiração; estimativa só da sessão.
                    if(*high-*low<=.06f)endpoint_=(endings_[0]+endings_[1]+endings_[2])/3;
                    else endpoint_.reset();
                }
            }
        }
        clearCycle();return {};
    }
    present_=true;observed_=false;
    auto fraction=observation.detection.remainingFraction;
    if(!fraction||!std::isfinite(*fraction)||*fraction<0||*fraction>1)return render(nowMs);
    if(endpoint_&&*fraction<*endpoint_-.06f){endpoint_.reset();endingCount_=0;}
    if(read_&&*fraction>*read_+.12f){clearCycle();present_=true;}
    if(!anchor_){anchor_=fraction;anchorTime_=lastFrame_;start_=*fraction;}
    else if(*anchor_-*fraction>=.04f&&lastFrame_>anchorTime_){
        const float speed=(*anchor_-*fraction)/static_cast<float>(lastFrame_-anchorTime_);
        if(intervals_&&std::abs(speed-speed_)<=speed_*.35f){speed_=(speed_+speed)/2;++intervals_;}
        else {speed_=speed;intervals_=1;}
        anchor_=fraction;anchorTime_=lastFrame_;
    }
    minimum_=std::min(minimum_,*fraction);read_=fraction;readTime_=lastFrame_;observed_=true;
    return render(nowMs);
}
}
