#include "predictive_clock.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
namespace {
void require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
aa::Observation obs(std::int64_t time,std::optional<float> fraction,aa::Presence presence=aa::Presence::Present,std::uint64_t source=7){
    aa::Observation result;result.capturedMs=time;result.source=source;result.detection.presence=presence;
    result.detection.remainingFraction=fraction;result.detection.stacks=3;return result;
}
aa::ClockPrediction feed(aa::PredictiveClock& clock,std::int64_t time,std::optional<float> fraction,aa::Presence presence=aa::Presence::Present){
    return clock.update(obs(time,fraction,presence),time,750,7);
}
void trainSpeed(aa::PredictiveClock& clock,std::int64_t start){
    feed(clock,start,.9f);feed(clock,start+500,.8f);feed(clock,start+1000,.7f);
}
void cycle(aa::PredictiveClock& clock,std::int64_t start,float endpoint=.3f){
    trainSpeed(clock,start);feed(clock,start+1500,.6f);feed(clock,start+2000,.5f);
    feed(clock,start+2500,.4f);feed(clock,start+3000,endpoint);
    feed(clock,start+3010,{},aa::Presence::Absent);
}
}
int main(){try{
    aa::PredictiveClock clock;
    require(!feed(clock,1000,{}).fraction,"inventou tempo sem amostras");
    feed(clock,1100,.9f);feed(clock,1600,.8f);
    require(!feed(clock,1700,{}).fraction,"previu com velocidade insuficiente");
    clock.reset();trainSpeed(clock,2000);
    auto prediction=feed(clock,3250,{});
    require(prediction.fraction&&prediction.estimated&&std::abs(*prediction.fraction-.65f)<.01f,"nao atravessou relogio encoberto");
    auto duplicate=clock.update(obs(3250,{}),3350,750,7);
    require(duplicate.fraction&&*duplicate.fraction<*prediction.fraction,"timestamp duplicado congelou projecao");
    require(!clock.update(obs(3250,{}),4000,750,7).fraction,"captura expirada manteve previsao");
    require(!feed(clock,4100,{}).fraction,"reaproveitou velocidade expirada");
    clock.reset();trainSpeed(clock,5000);
    auto renewed=feed(clock,6100,.95f);
    require(renewed.fraction&&*renewed.fraction>.9f,"renovacao nao reiniciou aro");
    require(!feed(clock,6200,{}).fraction,"renovacao usou velocidade do ciclo anterior");
    clock.reset();trainSpeed(clock,7000);
    auto changed=obs(8100,{});changed.detection.stacks=2;
    require(clock.update(changed,8100,750,7).fraction.has_value(),"stacks reiniciou relogio");
    require(!feed(clock,8200,{},aa::Presence::Unknown).fraction,"desconhecido manteve previsao");
    require(!feed(clock,8300,{}).fraction,"desconhecido nao invalidou ciclo");
    clock.reset();trainSpeed(clock,9000);
    require(!clock.update(obs(10100,{},aa::Presence::Present,8),10100,750,8).fraction,"nova fonte herdou velocidade");
    trainSpeed(clock,11000);
    require(!clock.update(obs(12000,.7f),11999,750,7).fraction,"frame futuro aceito");
    clock.reset();trainSpeed(clock,13000);
    require(!feed(clock,13999,{}).fraction,"timestamp regressivo manteve previsao");
    clock.reset();cycle(clock,15000);
    auto one=feed(clock,19000,.6f);
    require(one.fraction&&std::abs(*one.fraction-.6f)<.01f,"aprendeu endpoint com um desaparecimento");
    clock.reset();cycle(clock,20000);cycle(clock,24000);cycle(clock,28000);
    auto learned=feed(clock,32000,.6f);
    require(learned.fraction&&learned.estimated&&std::abs(*learned.fraction-(.6f-.3f)/.7f)<.02f,"nao ajustou aro a desaparecimentos concordantes");
    require(!feed(clock,32100,{},aa::Presence::Absent).fraction,"ausencia manteve aro aprendido");
    trainSpeed(clock,32200);feed(clock,33700,.6f);feed(clock,34200,.5f);
    feed(clock,34700,.4f);feed(clock,35200,.3f);
    const auto contradicted=feed(clock,35700,.2f);
    require(contradicted.fraction&&std::abs(*contradicted.fraction-.2f)<.01f&&!contradicted.estimated,
            "leitura abaixo do endpoint manteve aro zerado enquanto buff presente");
    feed(clock,36200,.1f);feed(clock,36210,{},aa::Presence::Absent);
    require(std::abs(*feed(clock,36300,.6f).fraction-.6f)<.01f,"endpoint contradito continuou aplicado");
    aa::PredictiveClock isolated;
    require(std::abs(*feed(isolated,32000,.6f).fraction-.6f)<.01f,"leitores compartilham endpoint");
    clock.reset();cycle(clock,33000);cycle(clock,37000,.5f);cycle(clock,41000);
    require(std::abs(*feed(clock,45000,.6f).fraction-.6f)<.01f,"aprendeu desaparecimentos discordantes");
    clock.reset();trainSpeed(clock,46000);feed(clock,48000,{},aa::Presence::Absent);
    require(!feed(clock,48100,{}).fraction,"lacuna de captura ensinou previsao");
    clock.reset();feed(clock,49000,std::numeric_limits<float>::quiet_NaN());
    require(!feed(clock,49100,{}).fraction,"NaN contaminou previsao");
    clock.reset();feed(clock,50000,.9f);feed(clock,50500,.8f);feed(clock,51000,.3f);
    require(!feed(clock,51100,{}).fraction,"velocidade inconsistente gerou previsao");
    clock.reset();cycle(clock,52000);
    feed(clock,55010,{},aa::Presence::Absent);feed(clock,55020,{},aa::Presence::Absent);
    require(std::abs(*feed(clock,55100,.6f).fraction-.6f)<.01f,"ausencias repetidas contaram como novos ciclos");
    clock.reset();feed(clock,56000,.9f);
    clock.update(obs(56000,.8f),56100,750,7);clock.update(obs(56000,.7f),56200,750,7);
    require(!feed(clock,56300,{}).fraction,"timestamp repetido treinou velocidade");
    clock.reset();trainSpeed(clock,57000);feed(clock,58500,.6f);feed(clock,59000,.5f);
    feed(clock,59100,{},aa::Presence::Unknown);feed(clock,59200,{},aa::Presence::Absent);
    cycle(clock,60000);cycle(clock,64000);
    require(std::abs(*feed(clock,68000,.6f).fraction-.6f)<.01f,"ciclo interrompido por desconhecido ensinou endpoint");
    clock.reset();for(int n=0;n<10;++n)feed(clock,69000+n*150,.95f-n*.03f);
    require(feed(clock,70500,{}).estimated,"sequencia da integracao nao treinou previsao");
    require(!clock.update(obs(70500,{}),70500,750,8).fraction,"source divergente aceitou previsao");
    std::cout<<"Relogio preditivo: todos os cenarios passaram\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
