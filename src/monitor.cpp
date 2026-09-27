#include "monitor.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace aa {
namespace {
Rule actionOf(const StatusRule& rule) { return rule.triggers.empty() ? rule.condition : rule.action; }
std::vector<RuleTrigger> triggersOf(const StatusRule& rule) {
    if (!rule.triggers.empty()) return rule.triggers;
    return {{L"legacy-" + rule.id, rule.statusId, rule.sourceArea, rule.condition, rule.stackSamples, rule.clockReferencePath}};
}
bool sameSamples(const std::vector<StackSample>& left,const std::vector<StackSample>& right) {
    return left.size()==right.size()&&std::equal(left.begin(),left.end(),right.begin(),[](const auto& a,const auto& b){return a.value==b.value&&a.path==b.path;});
}
}
MonitorPlan makeMonitorPlan(const Workspace& workspace){
    if(!readinessIssues(workspace).empty())throw std::runtime_error("Incomplete configuration for monitoring.");
    const auto hud=std::find_if(workspace.huds.begin(),workspace.huds.end(),[&](const auto& h){return h.id==workspace.activeHudId;});
    const auto set=std::find_if(workspace.sets.begin(),workspace.sets.end(),[&](const auto& s){return s.id==workspace.activeSetId;});
    MonitorPlan result;
    if(hud==workspace.huds.end()||set==workspace.sets.end())throw std::runtime_error("Choose a HUD and a set.");
    const auto includeCapture=[&](const Region& area){
        if(!result.captureArea.valid()){result.captureArea=area;result.captureArea.shape=RegionShape::Rectangle;return;}
        const int right=std::max(result.captureArea.x+result.captureArea.width,area.x+area.width),bottom=std::max(result.captureArea.y+result.captureArea.height,area.y+area.height);
        result.captureArea.x=std::min(result.captureArea.x,area.x);result.captureArea.y=std::min(result.captureArea.y,area.y);
        result.captureArea.width=right-result.captureArea.x;result.captureArea.height=bottom-result.captureArea.y;
    };
    for(const auto& link:set->rules){
        if(!link.enabled)continue;
        const auto found=findRule(workspace,link.ruleId);
        if(!found)throw std::runtime_error("Missing rule dependency.");
        auto rule=*found;
        rule.action.enabled=rule.condition.enabled=true;
        for(auto& trigger:rule.triggers)trigger.condition.enabled=true;
        const auto action=actionOf(rule);
        const auto target=std::find_if(hud->areas.begin(),hud->areas.end(),[&](const auto& a){return sameName(a.name,rule.targetArea);});
        if(target==hud->areas.end())throw std::runtime_error("Missing rule target.");
        MonitorAction monitored{rule,target->region,{}};
        if(rule.onlyWhenReady)includeCapture(target->region);
        for(const auto& trigger:triggersOf(rule)) {
            if(trigger.kind==TriggerKind::Health) {
                const auto area=std::find_if(hud->areas.begin(),hud->areas.end(),[&](const auto& a){return sameName(a.name,trigger.healthArea);});
                if(area==hud->areas.end())throw std::runtime_error("Missing health area.");
                auto reader=std::find_if(result.readers.begin(),result.readers.end(),[&](const auto& r){
                    return r.kind==MonitorReaderKind::Health&&sameName(r.area.name,trigger.healthArea);
                });
                const std::size_t index=static_cast<std::size_t>(reader-result.readers.begin());
                if(reader==result.readers.end()) { MonitorReader health;health.kind=MonitorReaderKind::Health;health.area=*area;result.readers.push_back(std::move(health));includeCapture(area->region); }
                monitored.readers.push_back(index);
                continue;
            }
            const auto status=std::find_if(workspace.statuses.begin(),workspace.statuses.end(),[&](const auto& s){return s.id==trigger.statusId;});
            if(status==workspace.statuses.end())throw std::runtime_error("Missing rule dependency.");
            auto materialized=*status;
            if(trigger.condition.condition==Condition::StacksEqual&&!trigger.stackSamples.empty())materialized.stacks=trigger.stackSamples;
            if(rule.followClock&&!trigger.clockReferencePath.empty())materialized.clockReferencePath=trigger.clockReferencePath;
            auto reader=std::find_if(result.readers.begin(),result.readers.end(),[&](const auto& r){
                return r.status.id==trigger.statusId&&sameName(r.area.name,trigger.sourceArea)&&sameSamples(r.status.stacks,materialized.stacks)&&r.status.clockReferencePath==materialized.clockReferencePath;
            });
            std::size_t index=static_cast<std::size_t>(reader-result.readers.begin());
            if(reader==result.readers.end()){
                const auto area=std::find_if(hud->areas.begin(),hud->areas.end(),[&](const auto& a){return sameName(a.name,trigger.sourceArea);});
                if(area==hud->areas.end())throw std::runtime_error("Missing rule dependency.");
                MonitorReader statusReader;statusReader.status=std::move(materialized);statusReader.area=*area;result.readers.push_back(std::move(statusReader));
                includeCapture(area->region);
            }
            if(trigger.condition.condition==Condition::StacksEqual)result.readers[index].needsStacks=true;
            if(rule.followClock&&trigger.condition.condition!=Condition::Absent)result.readers[index].needsClock=true;
            monitored.readers.push_back(index);
        }
        monitored.reader=monitored.readers.empty()?0:monitored.readers.front();
        monitored.healthLatches.assign(monitored.readers.size(),false);
        result.actions.push_back(std::move(monitored));
    }
    if(result.readers.empty())throw std::runtime_error("Enable at least one rule in this profile."); return result;
}
std::vector<bool> evaluateMonitor(const MonitorPlan& plan,const std::vector<Observation>& observations,std::int64_t nowMs,int validityMs,std::uint64_t source,std::vector<std::optional<float>>* remainingFractions,const std::vector<bool>* ready){
    std::vector<bool> active(plan.actions.size(),false);if(remainingFractions)remainingFractions->assign(plan.actions.size(),std::nullopt);if(observations.size()!=plan.readers.size())return active;
    for(std::size_t i=0;i<plan.actions.size();++i){
        const auto& a=plan.actions[i];const auto action=actionOf(a.rule);const auto triggers=triggersOf(a.rule);std::optional<std::size_t> matching;
        // Uma condição Status anterior pode vencer o OU. Mesmo assim, uma
        // captura inválida da Vida deve apagar sua histerese para o próximo frame.
        for(std::size_t k=0;k<a.readers.size()&&k<triggers.size();++k)if(triggers[k].kind==TriggerKind::Health&&
            k<a.healthLatches.size()){
            const auto index=a.readers[k];
            if(index>=observations.size()||observations[index].capturedMs>nowMs||
                nowMs-observations[index].capturedMs>validityMs||observations[index].source!=source||
                !observations[index].healthFraction||!std::isfinite(*observations[index].healthFraction)||
                *observations[index].healthFraction<0||*observations[index].healthFraction>1)
                a.healthLatches[k]=false;
        }
        for(std::size_t k=0;k<a.readers.size()&&k<triggers.size();++k) {
            if(a.readers[k]>=observations.size())continue;
            const auto& trigger=triggers[k];const auto& observation=observations[a.readers[k]];
            bool matches=false;
            if(trigger.kind==TriggerKind::Health) {
                const bool fresh=observation.capturedMs<=nowMs&&nowMs-observation.capturedMs<=validityMs&&observation.source==source&&
                    observation.healthFraction&&std::isfinite(*observation.healthFraction)&&*observation.healthFraction>=0&&*observation.healthFraction<=1;
                if(fresh) {
                    const float threshold=static_cast<float>(trigger.healthPercent)/100.f;
                    const bool latched=k<a.healthLatches.size()&&a.healthLatches[k];
                    matches=trigger.healthComparison==HealthComparison::AtMost ? *observation.healthFraction<=(latched?threshold+.02f:threshold) :
                        *observation.healthFraction>=(latched?threshold-.02f:threshold);
                    if(k<a.healthLatches.size())a.healthLatches[k]=matches;
                } else if(k<a.healthLatches.size())a.healthLatches[k]=false;
            } else matches=evaluate(trigger.condition,observation,nowMs,validityMs,source);
            if(matches){matching=k;break;}
        }
        if(!matching)continue;
        bool destinationReady=true;
        const auto sameDestination=[&](const MonitorAction& other){const auto& r=other.target;
            return sameName(other.rule.targetArea,a.rule.targetArea)||
                (r.x==a.target.x&&r.y==a.target.y&&r.width==a.target.width&&r.height==a.target.height);};
        // A disponibilidade é um veto da habilidade no destino, inclusive para
        // regras antigas que apontem aos mesmos pixels com outro nome.
        for(std::size_t j=0;j<plan.actions.size();++j)if(plan.actions[j].rule.onlyWhenReady&&sameDestination(plan.actions[j]))
            destinationReady &= ready&&ready->size()==plan.actions.size()&&(*ready)[j];
        if(!destinationReady)continue;
        bool occupied=false;for(std::size_t j=0;j<i;++j)if(active[j]&&sameDestination(plan.actions[j])){occupied=true;break;} active[i]=!occupied;
        const auto& trigger=triggers[*matching];const auto& detection=observations[a.readers[*matching]].detection;
        if(remainingFractions&&active[i]&&a.rule.followClock&&trigger.condition.condition!=Condition::Absent&&detection.presence==Presence::Present&&detection.remainingFraction&&std::isfinite(*detection.remainingFraction)&&*detection.remainingFraction>=0&&*detection.remainingFraction<=1)(*remainingFractions)[i]=detection.remainingFraction;
    } return active;
}
}
