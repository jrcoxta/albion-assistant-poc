#include "health_reader.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
aa::Image healthImage(float fraction,bool text){
    constexpr int width=240,height=42,left=24,right=216,top=14,bottom=28;
    aa::Image image{width,height,std::vector<std::uint8_t>(std::size_t(width)*height*4,28)};
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
        for(const auto [input,expected]:{std::pair{.48f,.48f},std::pair{.49f,.49f},std::pair{.50f,.50f},std::pair{1.f,1.f}}){
            const auto result=aa::readHealthFraction(healthImage(input,true),calibration);
            require(result&&near(*result,expected),"percentual de vida incorreto");
        }
        require(!aa::readHealthFraction({},calibration),"imagem invalida gerou leitura");
        std::cout<<"Leitura de barra calibrada aprovada\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
