#include "monitor.h"
#include <stdexcept>
#include <iostream>
namespace {
void require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
aa::StatusRule rule(const wchar_t* id,const wchar_t* status,const wchar_t* source,const wchar_t* target,aa::Condition condition){
    aa::StatusRule r;r.id=id;r.statusId=status;r.sourceArea=source;r.targetArea=target;r.condition.name=id;r.condition.condition=condition;return r;
}
}
int main(){try{
    aa::Workspace w;w.nextId=20;w.activeHudId=L"hud";w.activeSetId=L"set";
    aa::HudLayout hud;hud.id=L"hud";hud.name=L"Notebook";hud.clientWidth=1000;hud.clientHeight=800;
    hud.areas={{L"Buffs",{10,20,200,50},24,true},{L"Debuffs",{10,100,160,40},24,true},
               {L"Q",{300,500,40,40},48,false},{L"E",{400,500,40,40},48,false}};
    w.huds.push_back(hud);
    w.statuses={{L"a",L"Status A",false,true,{},{}},{L"b",L"Status B",true,true,{},{}}};
    aa::SetProfile set;set.id=L"set";set.name=L"Set de teste";
    set.rules={rule(L"r1",L"a",L"Buffs",L"E",aa::Condition::StacksEqual),
               rule(L"r2",L"b",L"Debuffs",L"Q",aa::Condition::Present),
               rule(L"r3",L"a",L"buffs",L"E",aa::Condition::Present)};
    set.rules[1].effect=aa::OverlayEffect::Pulse;w.sets.push_back(set);
    auto plan=aa::makeMonitorPlan(w);
    require(plan.readers.size()==2&&plan.actions.size()==3,"duplicou reconhecimento do mesmo status/regiao");
    require(plan.actions[0].reader==plan.actions[2].reader,"regras do mesmo par nao compartilham observacao");
    require(plan.captureArea.x==10&&plan.captureArea.y==20&&plan.captureArea.width==200&&plan.captureArea.height==120,"ROI agregada incorreta");
    require(plan.actions[1].rule.effect==aa::OverlayEffect::Pulse&&plan.actions[1].target.x==300,"acao/posicao nao preservada");
    require(plan.readers[0].needsStacks&&!plan.readers[1].needsStacks,"presença/ausência exigiu contadores");
    require(!plan.readers[0].needsClock&&!plan.readers[1].needsClock,"regra antiga solicitou relogio");
    {
        auto scoped=w;
        const auto fixture=std::filesystem::path(__FILE__).wstring();
        scoped.sets[0].rules[0].stackSamples={{3,fixture}};
        scoped.sets[0].rules[2].condition.condition=aa::Condition::StacksEqual;
        scoped.sets[0].rules[2].condition.stacks=2;
        scoped.sets[0].rules[2].stackSamples={{2,fixture}};
        const auto isolated=aa::makeMonitorPlan(scoped);
        require(isolated.readers.size()==3,"regras com amostras distintas compartilharam leitor");
        require(isolated.readers[0].status.stacks.size()==1&&isolated.readers[0].status.stacks[0].value==3,
                "leitor da primeira regra nao recebeu somente sua amostra");
        require(isolated.readers[2].status.stacks.size()==1&&isolated.readers[2].status.stacks[0].value==2,
                "leitor da segunda regra nao recebeu somente sua amostra");
    }
    {
        auto clocks=w;clocks.sets[0].rules[2].followClock=true;
        clocks.sets[0].rules[1].followClock=true;clocks.sets[0].rules[1].condition.condition=aa::Condition::Absent;
        const auto timed=aa::makeMonitorPlan(clocks);
        require(timed.readers.size()==2&&timed.readers[0].needsClock&&!timed.readers[1].needsClock,
                "relogio nao compartilha origem ou foi exigido por ausencia");
        clocks.sets[0].rules[2].condition.enabled=false;
        require(!aa::makeMonitorPlan(clocks).readers[0].needsClock,"regra desativada exigiu relogio");
    }
    {
        auto circular=w;
        circular.huds[0].areas[0].region={10,20,80,80,aa::RegionShape::Circle};
        circular.huds[0].areas[3].region.shape=aa::RegionShape::Circle;
        const auto shaped=aa::makeMonitorPlan(circular);
        require(shaped.captureArea.shape==aa::RegionShape::Rectangle&&shaped.captureArea.valid()&&shaped.captureArea.width==160&&shaped.captureArea.height==120,
                "uniao de origens circulares nao ficou retangular");
        require(shaped.readers[0].area.region.shape==aa::RegionShape::Circle&&shaped.readers[1].area.region.shape==aa::RegionShape::Rectangle&&
                shaped.actions[0].target.shape==aa::RegionShape::Circle&&shaped.actions[1].target.shape==aa::RegionShape::Rectangle,
                "forma de origem/destino foi perdida ou contaminou outra area");
    }
    std::vector<aa::Observation> obs={{{aa::Presence::Present,3,1,{},{}},1000,7},{{aa::Presence::Absent,{},1,{},{}},1000,7}};
    {
        auto timed=plan;for(auto& action:timed.actions)action.rule.followClock=true;
        auto readings=obs;readings[0].detection.remainingFraction=.3f;
        std::vector<std::optional<float>> remaining;
        const auto evaluate=[&](int now=1000,std::uint64_t source=7){return aa::evaluateMonitor(timed,readings,now,750,source,&remaining);};
        require(evaluate()==std::vector<bool>({true,false,false})&&remaining[0]==.3f&&!remaining[1]&&!remaining[2],"aro nao pertence somente a acao vencedora");
        readings[0].detection.remainingFraction=.9f;evaluate();require(remaining[0]==.9f,"renovacao com mesmos stacks nao aumentou aro");
        readings[0].detection.remainingFraction.reset();
        require(evaluate()[0]&&!remaining[0],"relogio incerto manteve aro ou apagou aura valida");
        readings[0].detection.remainingFraction=.5f;readings[0].detection.stacks=2;
        require(evaluate()==std::vector<bool>({false,false,true})&&!remaining[0]&&remaining[2]==.5f,"troca de prioridade reaproveitou aro da acao antiga");
        readings[1].detection.presence=aa::Presence::Present;readings[1].detection.remainingFraction=.7f;
        require(evaluate()[1]&&remaining[1]==.7f&&remaining[2]==.5f,"relogios de origens distintas se contaminaram");
        require(evaluate(1750)==std::vector<bool>({false,false,false})&&!remaining[1]&&!remaining[2],"dados expirados mantiveram aros");
        require(evaluate(1000,8)==std::vector<bool>({false,false,false})&&!remaining[1]&&!remaining[2],"fonte antiga manteve aros");
        readings[0].detection.presence=aa::Presence::Unknown;
        require(!evaluate()[2]&&!remaining[2],"status desconhecido manteve aro");
        timed.actions[0].rule.condition.condition=aa::Condition::Absent;readings[0].detection.presence=aa::Presence::Absent;
        require(evaluate()[0]&&!remaining[0],"regra de ausencia mostrou tempo restante");
        readings.clear();require(evaluate()==std::vector<bool>({false,false,false})&&!remaining[0]&&!remaining[1],"lote incompleto manteve aro anterior");
    }
    require(aa::evaluateMonitor(plan,obs,1000,750,7)==std::vector<bool>({true,false,false}),"prioridade de destino ou condicao incorreta");
    obs[0].detection.stacks=2;obs[1].detection.presence=aa::Presence::Present;
    require(aa::evaluateMonitor(plan,obs,1000,750,7)==std::vector<bool>({false,true,true}),"regras de status diferentes se contaminaram");
    obs[0].detection.presence=aa::Presence::Unknown;
    require(aa::evaluateMonitor(plan,obs,1000,750,7)==std::vector<bool>({false,true,false}),"desconhecido reaproveitou acao anterior");
    obs[1].capturedMs=250;
    require(aa::evaluateMonitor(plan,obs,1000,750,7)==std::vector<bool>({false,false,false}),"expiracao de leitura nao apagou acao");
    require(aa::evaluateMonitor(plan,obs,1000,750,8)==std::vector<bool>({false,false,false}),"fonte antiga reaproveitada");
    obs.pop_back();require(aa::evaluateMonitor(plan,obs,1000,750,7)==std::vector<bool>({false,false,false}),"lote parcial deveria apagar tudo");
    w.sets[0].rules[0].sourceArea=L"faltando";
    bool rejected=false;try{aa::makeMonitorPlan(w);}catch(const std::exception&){rejected=true;}
    require(rejected,"regra sem area iniciou silenciosamente");
    w.sets[0].rules.erase(w.sets[0].rules.begin());
    w.statuses[0].stacks={{5,L"Z:\\arquivo-ausente-de-teste.png"}};
    require(aa::readinessIssues(w).empty(),"presença depende de arquivo de contagem");
    auto presencePlan=aa::makeMonitorPlan(w);require(!presencePlan.readers[1].needsStacks,"presença carregaria contadores");
    w.sets[0].rules[1].condition.condition=aa::Condition::StacksEqual;w.sets[0].rules[1].condition.stacks=5;
    require(!aa::readinessIssues(w).empty(),"contagem com amostra ausente não bloqueou");
    std::cout<<"Plano multi-status, compartilhamento, acoes independentes, prioridade e expiracao verificados\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
