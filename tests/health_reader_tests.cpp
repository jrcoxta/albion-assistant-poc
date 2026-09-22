#include "health_reader.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
aa::Image healthImage(float fraction,bool text,bool header=false){
    constexpr int width=240,height=42,left=24,right=216,top=14,bottom=28;
    aa::Image image{width,height,std::vector<std::uint8_t>(std::size_t(width)*height*4,28)};
    if(header)for(int y=2;y<5;++y)for(int x=8;x<232;++x){
        auto* pixel=image.bgra.data()+(std::size_t(y)*width+x)*4;
        pixel[0]=97;pixel[1]=128;pixel[2]=176;pixel[3]=255;
    }
    const int fill=left+static_cast<int>(std::lround((right-left)*fraction));
    for(int y=top;y<bottom;++y)for(int x=left;x<right;++x){
        auto* pixel=image.bgra.data()+(std::size_t(y)*width+x)*4;
        if(x<fill){pixel[0]=28;pixel[1]=42;pixel[2]=190;pixel[3]=255;}
        else {pixel[0]=25;pixel[1]=25;pixel[2]=55;pixel[3]=255;}
    }
    if(text)for(int x=90;x<150;++x)for(int y=19;y<23;++y){auto* pixel=image.bgra.data()+(std::size_t(y)*width+x)*4;pixel[0]=pixel[1]=pixel[2]=245;pixel[3]=255;}
    return image;
}
bool near(float actual,float expected){return std::abs(actual-expected)<=.02f;}
}

int main(){
    try{
        const auto calibration=aa::calibrateHealth(healthImage(1.f,true));
        require(calibration.valid(),"calibracao da barra cheia falhou");
        for(const auto [input,expected]:{std::pair{.05f,.05f},std::pair{.48f,.48f},std::pair{.49f,.49f},std::pair{.50f,.50f},std::pair{1.f,1.f}}){
            const auto result=aa::readHealthFraction(healthImage(input,true),calibration);
            require(result&&near(*result,expected),"percentual de vida incorreto");
        }
        require(!aa::readHealthFraction({},calibration),"imagem invalida gerou leitura");
        const auto withHeader=aa::calibrateHealth(healthImage(1.f,true,true));
        require(withHeader.valid()&&withHeader.y==14&&withHeader.x==24&&withHeader.width==192,
                "cabecalho marrom foi confundido com a barra cheia");
        const auto reading=aa::readHealthFraction(healthImage(.43f,true,true),withHeader);
        require(reading&&near(*reading,.43f),"barra abaixo do cabecalho nao leu 43 por cento");
        auto obscured=healthImage(1.f,false,true);
        for(int y=14;y<28;++y)for(int x=90;x<150;++x){
            auto* pixel=obscured.bgra.data()+(std::size_t(y)*obscured.width+x)*4;
            pixel[0]=pixel[1]=pixel[2]=245;
        }
        const auto obscuredCalibration=aa::calibrateHealth(obscured);
        require(obscuredCalibration.valid()&&obscuredCalibration.x==24&&obscuredCalibration.width==192,
                "texto sobre toda a barra encurtou a largura calibrada");
        auto noisy=healthImage(.43f,true,true);
        for(int x=60;x<110;++x){
            auto* pixel=noisy.bgra.data()+(std::size_t(27)*noisy.width+x)*4;
            pixel[0]=pixel[1]=pixel[2]=245;
        }
        const auto noisyReading=aa::readHealthFraction(noisy,withHeader);
        require(noisyReading&&near(*noisyReading,.43f),"uma linha ruidosa anulou a leitura de vida");
        auto artifact=healthImage(.43f,true,true);
        for(int y=14;y<28;++y)for(int x=200;x<203;++x){
            auto* pixel=artifact.bgra.data()+(std::size_t(y)*artifact.width+x)*4;
            pixel[0]=28;pixel[1]=42;pixel[2]=190;
        }
        const auto artifactReading=aa::readHealthFraction(artifact,withHeader);
        require(artifactReading&&near(*artifactReading,.43f),"artefato vermelho distante inflou o percentual de vida");
        auto blank=healthImage(0.f,false,true);
        require(!aa::readHealthFraction(blank,withHeader),"barra ausente foi tratada como vida zero");
        for(int y=14;y<28;++y)for(int x=36;x<66;++x){
            auto* pixel=blank.bgra.data()+(std::size_t(y)*blank.width+x)*4;
            pixel[0]=28;pixel[1]=42;pixel[2]=190;
        }
        require(!aa::readHealthFraction(blank,withHeader),"vermelho solto sem barra ativou vida baixa");
        auto distant=healthImage(.43f,false,true);
        for(int y=14;y<28;++y)for(int x=180;x<212;++x){
            auto* pixel=distant.bgra.data()+(std::size_t(y)*distant.width+x)*4;
            pixel[0]=28;pixel[1]=42;pixel[2]=190;
        }
        const auto distantReading=aa::readHealthFraction(distant,withHeader);
        require(distantReading&&near(*distantReading,.43f),"trecho vermelho distante inflou a vida");
        const aa::HealthCalibration oldHeader{8,2,224,3,176,128,97};
        require(!oldHeader.valid(),"calibracao antiga do cabecalho continua valida");
        std::cout<<"Leitura de barra calibrada aprovada\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
