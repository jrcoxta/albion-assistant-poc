#include "monitor.h"
#include <algorithm>
#include <stdexcept>
namespace aa {
MonitorPlan makeMonitorPlan(const Workspace& workspace){
    if(!readinessIssues(workspace).empty())throw std::runtime_error("Configuração incompleta para monitorar.");
    const auto hud=std::find_if(workspace.huds.begin(),workspace.huds.end(),[&](const auto& h){return h.id==workspace.activeHudId;});
    const auto set=std::find_if(workspace.sets.begin(),workspace.sets.end(),[&](const auto& s){return s.id==workspace.activeSetId;});
    MonitorPlan result;
    if(hud==workspace.huds.end()||set==workspace.sets.end())throw std::runtime_error("Escolha uma HUD e um set.");
    for(const auto& rule:set->rules){
        if(!rule.condition.enabled)continue;
        auto reader=std::find_if(result.readers.begin(),result.readers.end(),[&](const auto& r){return r.status.id==rule.statusId&&sameName(r.area.name,rule.sourceArea);});
        std::size_t index=static_cast<std::size_t>(reader-result.readers.begin());
        if(reader==result.readers.end()){
            const auto status=std::find_if(workspace.statuses.begin(),workspace.statuses.end(),[&](const auto& s){return s.id==rule.statusId;});
            const auto area=std::find_if(hud->areas.begin(),hud->areas.end(),[&](const auto& a){return sameName(a.name,rule.sourceArea);});
            if(status==workspace.statuses.end()||area==hud->areas.end())throw std::runtime_error("Dependência da regra ausente.");
            result.readers.push_back({*status,*area});
            const auto r=area->region;
            if(!result.captureArea.valid()){result.captureArea=r;result.captureArea.shape=RegionShape::Rectangle;}
            else{
                const int right=std::max(result.captureArea.x+result.captureArea.width,r.x+r.width);
                const int bottom=std::max(result.captureArea.y+result.captureArea.height,r.y+r.height);
                result.captureArea.x=std::min(result.captureArea.x,r.x);result.captureArea.y=std::min(result.captureArea.y,r.y);
                result.captureArea.width=right-result.captureArea.x;result.captureArea.height=bottom-result.captureArea.y;
            }
        }
        if(rule.condition.condition==Condition::StacksEqual)result.readers[index].needsStacks=true;
        const auto target=std::find_if(hud->areas.begin(),hud->areas.end(),[&](const auto& a){return sameName(a.name,rule.targetArea);});
        if(target==hud->areas.end())throw std::runtime_error("Destino da regra ausente.");
        result.actions.push_back({rule,target->region,index});
    }
    if(result.readers.empty())throw std::runtime_error("Ative ao menos uma regra neste set.");
    return result;
}
std::vector<bool> evaluateMonitor(const MonitorPlan& plan,const std::vector<Observation>& observations,
                                 std::int64_t nowMs,int validityMs,std::uint64_t source){
    std::vector<bool> active(plan.actions.size(),false);
    if(observations.size()!=plan.readers.size())return active;
    for(std::size_t i=0;i<plan.actions.size();++i){
        const auto& a=plan.actions[i];
        if(a.reader>=observations.size()||!evaluate(a.rule.condition,observations[a.reader],nowMs,validityMs,source))continue;
        bool occupied=false;
        for(std::size_t j=0;j<i;++j)if(active[j]&&sameName(plan.actions[j].rule.targetArea,a.rule.targetArea)){occupied=true;break;}
        active[i]=!occupied;
    }
    return active;
}
}
