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
    set.rules[1].glow=true;w.sets.push_back(set);
    auto plan=aa::makeMonitorPlan(w);
    require(plan.readers.size()==2&&plan.actions.size()==3,"duplicou reconhecimento do mesmo status/regiao");
    require(plan.actions[0].reader==plan.actions[2].reader,"regras do mesmo par nao compartilham observacao");
    require(plan.captureArea.x==10&&plan.captureArea.y==20&&plan.captureArea.width==200&&plan.captureArea.height==120,"ROI agregada incorreta");
    require(plan.actions[1].rule.glow&&plan.actions[1].target.x==300,"acao/posicao nao preservada");
    require(plan.readers[0].needsStacks&&!plan.readers[1].needsStacks,"presença/ausência exigiu contadores");
    std::vector<aa::Observation> obs={{{aa::Presence::Present,3,1,{},{}},1000,7},{{aa::Presence::Absent,{},1,{},{}},1000,7}};
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
