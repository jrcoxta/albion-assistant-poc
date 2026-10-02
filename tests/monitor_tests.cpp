#include "monitor.h"
#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <iostream>
namespace {
void require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
aa::StatusRule rule(const wchar_t* id,const wchar_t* status,const wchar_t* source,const wchar_t* target,aa::Condition condition){
    aa::StatusRule r;r.id=id;r.statusId=status;r.sourceArea=source;r.targetArea=target;r.condition.name=id;r.condition.condition=condition;return r;
}
aa::StatusRule& linked(aa::Workspace& w,std::size_t index){
    const auto id=w.sets.at(0).rules.at(index).ruleId;
    auto found=std::find_if(w.rules.begin(),w.rules.end(),[&](const auto& value){return value.id==id;});
    if(found==w.rules.end())throw std::runtime_error("Regra do perfil inexistente.");
    return *found;
}
void measureEvaluation(const char* label,const aa::MonitorPlan& plan) {
    aa::Observation reading;reading.detection.presence=aa::Presence::Present;
    reading.detection.stacks=3;reading.capturedMs=1000;reading.source=7;
    const std::vector<aa::Observation> observations(plan.readers.size(),reading);
    std::vector<double> micros;micros.reserve(400);
    std::size_t active=0;
    for(int iteration=0;iteration<450;++iteration){
        const auto start=std::chrono::steady_clock::now();
        const auto result=aa::evaluateMonitor(plan,observations,1000,750,7);
        const auto finished=std::chrono::steady_clock::now();
        active+=static_cast<std::size_t>(std::count(result.begin(),result.end(),true));
        if(iteration>=50)micros.push_back(std::chrono::duration<double,std::micro>(finished-start).count());
    }
    std::sort(micros.begin(),micros.end());
    std::cout<<"benchmark monitor "<<label<<": leitores="<<plan.readers.size()<<" acoes="<<plan.actions.size()
             <<" mediana="<<micros[micros.size()/2]<<" us p95="<<micros[micros.size()*95/100]
             <<" us ("<<active<<" acionamentos de controle)\n";
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
    w.rules={rule(L"r1",L"a",L"Buffs",L"E",aa::Condition::StacksEqual),
                rule(L"r2",L"b",L"Debuffs",L"Q",aa::Condition::Present),
                rule(L"r3",L"a",L"buffs",L"E",aa::Condition::Present)};
    w.rules[1].effect=aa::OverlayEffect::Pulse;set.rules={{L"r1",true},{L"r2",true},{L"r3",true}};w.sets.push_back(set);
    auto plan=aa::makeMonitorPlan(w);
    require(plan.readers.size()==2&&plan.actions.size()==3,"duplicou reconhecimento do mesmo status/regiao");
    require(plan.actions[0].reader==plan.actions[2].reader,"regras do mesmo par nao compartilham observacao");
    require(plan.captureArea.x==10&&plan.captureArea.y==20&&plan.captureArea.width==200&&plan.captureArea.height==120,"ROI agregada incorreta");
    require(plan.actions[1].rule.effect==aa::OverlayEffect::Pulse&&plan.actions[1].target.x==300,"acao/posicao nao preservada");
    require(plan.readers[0].needsStacks&&!plan.readers[1].needsStacks,"presença/ausência exigiu contadores");
    require(!plan.readers[0].needsClock&&!plan.readers[1].needsClock,"regra antiga solicitou relogio");
    {
        auto shared=w;
        shared.sets.push_back({L"set2",L"Outro perfil",{{L"r2",true}}});
        shared.sets[0].rules[1].enabled=false;
        shared.rules[1].effect=aa::OverlayEffect::Flames;
        shared.activeSetId=L"set2";
        const auto secondPlan=aa::makeMonitorPlan(shared);
        require(secondPlan.actions.size()==1&&secondPlan.actions[0].rule.id==L"r2"&&
                secondPlan.actions[0].rule.effect==aa::OverlayEffect::Flames,
                "edicao compartilhada nao chegou ao segundo perfil ativo");
        shared.activeSetId=L"set";
        const auto firstPlan=aa::makeMonitorPlan(shared);
        require(firstPlan.actions.size()==2&&std::none_of(firstPlan.actions.begin(),firstPlan.actions.end(),
                [](const auto& action){return action.rule.id==L"r2";}),
                "desativar um vinculo afetou outro perfil ou ignorou o perfil ativo");
    }
    {
        auto scoped=w;
        const auto fixture=std::filesystem::path(__FILE__).wstring();
        linked(scoped,0).stackSamples={{3,fixture}};
        linked(scoped,2).condition.condition=aa::Condition::StacksEqual;
        linked(scoped,2).condition.stacks=2;
        linked(scoped,2).stackSamples={{2,fixture}};
        const auto isolated=aa::makeMonitorPlan(scoped);
        require(isolated.readers.size()==3,"regras com amostras distintas compartilharam leitor");
        require(isolated.readers[0].status.stacks.size()==1&&isolated.readers[0].status.stacks[0].value==3,
                "leitor da primeira regra nao recebeu somente sua amostra");
        require(isolated.readers[2].status.stacks.size()==1&&isolated.readers[2].status.stacks[0].value==2,
                "leitor da segunda regra nao recebeu somente sua amostra");
    }
    {
        auto clocks=w;linked(clocks,2).followClock=true;
        linked(clocks,1).followClock=true;linked(clocks,1).condition.condition=aa::Condition::Absent;
        const auto timed=aa::makeMonitorPlan(clocks);
        require(timed.readers.size()==2&&timed.readers[0].needsClock&&!timed.readers[1].needsClock,
                "relogio nao compartilha origem ou foi exigido por ausencia");
        clocks.sets[0].rules[2].enabled=false;
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
        auto guarded=w;
        guarded.huds[0].areas[3].readyReferencePath=std::filesystem::path(__FILE__).wstring();
        guarded.huds[0].areas[3].readyConfirmed=true;
        linked(guarded,0).onlyWhenReady=true;
        const auto guardedPlan=aa::makeMonitorPlan(guarded);
        require(guardedPlan.captureArea.y+guardedPlan.captureArea.height==540,
                "captura nao incluiu a habilidade de destino");
        auto repeated=guardedPlan;repeated.actions[2].rule.onlyWhenReady=true;
        const auto owners=aa::readyComparisonSources(repeated);
        require(owners.size()==3&&owners[0]==0&&owners[1]==1&&owners[2]==0,
                "duas regras da mesma habilidade repetiram comparacao por frame");
        repeated.actions[2].rule.targetArea=L"Outra referencia no mesmo pixel";
        require(aa::readyComparisonSources(repeated)[2]==2,
                "areas diferentes no mesmo pixel herdaram referencia de habilidade alheia");
        repeated.actions[2].rule.targetArea=repeated.actions[0].rule.targetArea;
        ++repeated.actions[2].target.x;
        require(aa::readyComparisonSources(repeated)[2]==2,
                "posicao divergente compartilhou comparacao de habilidade pronta");
        require(aa::evaluateMonitor(guardedPlan,obs,1000,750,7)==std::vector<bool>({false,false,false}),
                "regra seguinte acendeu a habilidade em cooldown");
        const std::vector<bool> ready{true,false,false};
        require(aa::evaluateMonitor(guardedPlan,obs,1000,750,7,nullptr,&ready)==std::vector<bool>({true,false,false}),
                "habilidade pronta nao habilitou a regra prioritaria");
        const std::vector<bool> cooldown{false,false,false};
        require(aa::evaluateMonitor(guardedPlan,obs,1000,750,7,nullptr,&cooldown)==std::vector<bool>({false,false,false}),
                "cooldown nao bloqueou outras regras do mesmo destino");
        guarded.huds[0].areas.push_back({L"E com outro nome",{400,500,40,40},48,false});
        linked(guarded,2).targetArea=L"E com outro nome";
        const auto aliasPlan=aa::makeMonitorPlan(guarded);
        require(aa::evaluateMonitor(aliasPlan,obs,1000,750,7,nullptr,&cooldown)==std::vector<bool>({false,false,false}),
                "area com outro nome no mesmo icone ignorou cooldown");
        require(aa::evaluateMonitor(aliasPlan,obs,1000,750,7,nullptr,&ready)==std::vector<bool>({true,false,false}),
                "area duplicada no mesmo icone ultrapassou a prioridade");
        guarded.huds[0].areas[3].replaceRegion({400,500,40,40});
        require(!aa::readinessIssues(guarded).empty(),"mover a habilidade conservou uma amostra antiga");
    }
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
    {
        auto alternatives=w;
        const auto assets=std::filesystem::path(__FILE__).parent_path().parent_path()/L"assets";
        alternatives.statuses[1].builtinAssassin=false;
        alternatives.statuses[1].referencePath=(assets/L"other-buff.png").wstring();
        alternatives.statuses[1].stacks={{3,(assets/L"assassin-3-40.png").wstring()}};
        alternatives.statuses[1].clockReferencePath.clear();
        auto& action=linked(alternatives,0);
        action.action=action.condition;action.followClock=true;
        auto first=aa::RuleTrigger{L"assassin",L"a",L"Buffs",action.condition,{},L""};
        auto second=aa::RuleTrigger{L"fender",L"b",L"Debuffs",action.condition,{},L""};
        action.triggers={first,second};
        alternatives.sets[0].rules={{action.id,true}};
        const auto alternatePlan=aa::makeMonitorPlan(alternatives);
        require(alternatePlan.readers.size()==2&&alternatePlan.readers[0].needsClock&&alternatePlan.readers[1].needsClock,
                "regra OU nao solicitou leitura temporal separada por status");
        auto current=std::vector<aa::Observation>{{{aa::Presence::Present,2u,1.f,{},{}},1000,7},
                                                   {{aa::Presence::Present,3u,1.f,{},{}},1000,7}};
        current[0].detection.remainingFraction=.6f;
        std::vector<std::optional<float>> remaining;
        require(aa::evaluateMonitor(alternatePlan,current,1000,750,7,&remaining)==std::vector<bool>({true})&&
                !remaining[0],"ramo Fender sem relogio herdou tempo do Assassino nao correspondente");
        current[0].detection.stacks=3u;
        require(aa::evaluateMonitor(alternatePlan,current,1000,750,7,&remaining)==std::vector<bool>({true})&&
                remaining[0]==.6f,"ramo Assassino com tres stacks perdeu seu relogio observado");
        current[0].detection.presence=aa::Presence::Absent;
        require(aa::evaluateMonitor(alternatePlan,current,1000,750,7,&remaining)==std::vector<bool>({true})&&
                !remaining[0],"aro anterior persistiu ao mudar para ramo sem relogio");
    }
    {
        auto composite=w;
        auto& combined=linked(composite,0); combined.action=combined.condition;
        aa::RuleTrigger first; first.id=L"t-first"; first.statusId=combined.statusId; first.sourceArea=combined.sourceArea; first.condition=combined.condition;
        aa::RuleTrigger alternate; alternate.id=L"t-extra"; alternate.statusId=L"b"; alternate.sourceArea=L"Debuffs"; alternate.condition.condition=aa::Condition::Present;
        combined.triggers={first,alternate};
        const auto compositePlan=aa::makeMonitorPlan(composite);
        require(compositePlan.actions[0].readers.size()==2,"acao composta nao criou um leitor por condicao");
        auto alternateObservations=obs; alternateObservations[0].detection.presence=aa::Presence::Absent; alternateObservations[1].detection.presence=aa::Presence::Present;
        require(aa::evaluateMonitor(compositePlan,alternateObservations,1000,750,7)[0],"segunda condicao OU nao ativou a mesma acao");
    }
    {
        auto health=w;
        health.huds[0].areas[0].healthCalibration={10,4,180,5,190,42,28};
        auto& combined=linked(health,0); combined.action=combined.condition;
        aa::RuleTrigger life; life.id=L"vida-49"; life.kind=aa::TriggerKind::Health; life.healthArea=L"Buffs";
        life.healthComparison=aa::HealthComparison::AtMost; life.healthPercent=49;
        combined.triggers={life};
        auto healthPlan=aa::makeMonitorPlan(health);
        const auto lifeReader=std::find_if(healthPlan.readers.begin(),healthPlan.readers.end(),[](const auto& reader){return reader.kind==aa::MonitorReaderKind::Health;});
        require(lifeReader!=healthPlan.readers.end(),"gatilho de vida nao criou leitor proprio");
        const auto lifeIndex=static_cast<std::size_t>(lifeReader-healthPlan.readers.begin());
        std::vector<aa::Observation> healthReadings(healthPlan.readers.size());
        for(auto& reading:healthReadings){reading.capturedMs=1000;reading.source=7;}
        healthReadings[lifeIndex].healthFraction=.49f;
        require(aa::evaluateMonitor(healthPlan,healthReadings,1000,750,7)[0],"49 por cento nao ativou o limite");
        healthReadings[lifeIndex].healthFraction=.50f;
        require(aa::evaluateMonitor(healthPlan,healthReadings,1000,750,7)[0],"histerese soltou antes de 51 por cento");
        healthReadings[lifeIndex].healthFraction=.52f;
        require(!aa::evaluateMonitor(healthPlan,healthReadings,1000,750,7)[0],"52 por cento manteve o limite 49 ativo");
        healthReadings[lifeIndex].healthFraction.reset();
        require(!aa::evaluateMonitor(healthPlan,healthReadings,1000,750,7)[0],"vida incerta ativou a regra");
        healthReadings[lifeIndex].healthFraction=.50f;
        require(!aa::evaluateMonitor(healthPlan,healthReadings,1000,750,7)[0],"vida nova sem limiar reutilizou a histerese incerta");
        healthReadings[lifeIndex].healthFraction=.49f;
        require(aa::evaluateMonitor(healthPlan,healthReadings,1749,750,7)[0],"vida ainda valida nao ativou a regra");
        require(!aa::evaluateMonitor(healthPlan,healthReadings,1750,750,7)[0],"vida no limite da validade manteve destaque");
        healthReadings[lifeIndex].healthFraction=.50f;
        require(!aa::evaluateMonitor(healthPlan,healthReadings,1000,750,7)[0],"vida expirada manteve histerese");
        healthReadings[lifeIndex].healthFraction=.49f;
        healthReadings[lifeIndex].source=0;
        require(!aa::evaluateMonitor(healthPlan,healthReadings,1000,750,0)[0],"fonte zero ativou regra de vida");
        healthReadings[lifeIndex].capturedMs=0;healthReadings[lifeIndex].source=7;
        require(!aa::evaluateMonitor(healthPlan,healthReadings,1000,750,7)[0],"captura sem instante ativou vida");
        auto mixed=health;
        mixed.huds[0].areas[3].readyReferencePath=std::filesystem::path(__FILE__).wstring();
        mixed.huds[0].areas[3].readyConfirmed=true;
        auto& mixedRule=linked(mixed,0);mixedRule.onlyWhenReady=true;
        aa::RuleTrigger statusTrigger;statusTrigger.id=L"status-alt";statusTrigger.kind=aa::TriggerKind::Status;
        statusTrigger.statusId=L"a";statusTrigger.sourceArea=L"Buffs";statusTrigger.condition.condition=aa::Condition::Present;
        mixedRule.triggers={statusTrigger,life};
        const auto mixedPlan=aa::makeMonitorPlan(mixed);
        std::vector<aa::Observation> readings(mixedPlan.readers.size());
        for(auto& reading:readings){reading.capturedMs=1000;reading.source=7;}
        const auto statusIndex=mixedPlan.actions[0].readers[0],healthIndex=mixedPlan.actions[0].readers[1];
        const std::vector<bool> ready(mixedPlan.actions.size(),true),cooldown(mixedPlan.actions.size(),false);
        readings[statusIndex].detection.presence=aa::Presence::Absent;
        readings[healthIndex].healthFraction=.49f;
        require(aa::evaluateMonitor(mixedPlan,readings,1000,750,7,nullptr,&ready)[0],"vida 49 nao armou latch");
        readings[statusIndex].detection.presence=aa::Presence::Present;
        readings[healthIndex].healthFraction.reset();
        require(!aa::evaluateMonitor(mixedPlan,readings,1000,750,7,nullptr,&cooldown)[0],"cooldown nao apagou a acao");
        readings[statusIndex].detection.presence=aa::Presence::Absent;
        readings[healthIndex].healthFraction=.50f;
        require(!aa::evaluateMonitor(mixedPlan,readings,1000,750,7,nullptr,&ready)[0],"status OU vida conservou histerese invalida");
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
    linked(w,0).sourceArea=L"faltando";
    bool rejected=false;try{aa::makeMonitorPlan(w);}catch(const std::exception&){rejected=true;}
    require(rejected,"regra sem area iniciou silenciosamente");
    w.sets[0].rules.erase(w.sets[0].rules.begin());
    w.statuses[0].stacks={{5,L"Z:\\arquivo-ausente-de-teste.png"}};
    require(aa::readinessIssues(w).empty(),"presença depende de arquivo de contagem");
    auto presencePlan=aa::makeMonitorPlan(w);require(!presencePlan.readers[1].needsStacks,"presença carregaria contadores");
    linked(w,1).condition.condition=aa::Condition::StacksEqual;linked(w,1).condition.stacks=5;
    require(!aa::readinessIssues(w).empty(),"contagem com amostra ausente não bloqueou");
    {
        auto single=plan;
        single.readers.resize(1);single.actions.resize(1);
        single.actions[0].rule.action=single.actions[0].rule.condition;
        single.actions[0].rule.triggers={{L"benchmark",L"a",L"Buffs",single.actions[0].rule.condition,{},{}}};
        measureEvaluation("1",single);
        auto shared=single;shared.actions.push_back(single.actions[0]);
        shared.actions[1].rule.id=L"benchmark-2";
        measureEvaluation("2 compartilhadas",shared);
        auto many=single;
        for(int i=1;i<32;++i) {
            auto action=single.actions[0];action.rule.id=L"benchmark-"+std::to_wstring(i);
            many.actions.push_back(std::move(action));
        }
        measureEvaluation("32 compartilhadas",many);
        for(std::size_t i=1;i<many.actions.size();++i) {
            many.readers.push_back(single.readers[0]);
            many.actions[i].readers={i};many.actions[i].reader=i;
            many.actions[i].rule.targetArea=L"Destino "+std::to_wstring(i);
            many.actions[i].target.x+=static_cast<int>(i*45);
        }
        measureEvaluation("32 distintas",many);
    }
    std::cout<<"Plano multi-status, compartilhamento, acoes independentes, prioridade e expiracao verificados\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
