#include "recognition.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objbase.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
aa::Image resize(const aa::Image& source, int size) {
    aa::Image out{size, size, std::vector<std::uint8_t>(size * size * 4)};
    for (int y=0; y<size; ++y) for (int x=0; x<size; ++x) {
        const auto from=(std::size_t(y*source.height/size)*source.width+x*source.width/size)*4;
        std::copy_n(source.bgra.data()+from,4,out.bgra.data()+(std::size_t(y)*size+x)*4);
    }
    return out;
}
aa::Image place(const aa::Image& source, int width, int height, int left, int top) {
    aa::Image out{width,height,std::vector<std::uint8_t>(std::size_t(width)*height*4,24)};
    for(int y=0;y<source.height;++y) std::copy_n(source.bgra.data()+std::size_t(y)*source.width*4,source.width*4,
        out.bgra.data()+(std::size_t(top+y)*width+left)*4);
    return out;
}
}
int main(int argc,char** argv) {
    const HRESULT com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    try {
        const std::filesystem::path assets=argc>1?argv[1]:"assets";
        aa::Recognizer recognizer(assets);
        const auto three=aa::loadImage(assets/"assassin-3.png");
        const auto two=aa::loadImage(assets/"assassin-2.png");
        const auto none=aa::loadImage(assets/"assassin-none.png");
        check(three.valid() && two.valid() && none.valid(),"Referencias reais decodificadas");
        const auto result=recognizer.recognize(three,64);
        check(result.presence==aa::Presence::Present,"Buff real com 3 deve estar presente");
        check(result.stacks==3u,"Numero real 3 deve ser lido separadamente");
        check(recognizer.recognize(two,64).stacks==2u,"Numero real 2 deve ser lido");
        const auto noNumber=recognizer.recognize(none,64);
        check(noNumber.presence==aa::Presence::Present && !noNumber.stacks,"Sem numero nao implica 1");
        for(int size: {24,32,44,64,80,100}) {
            const auto shifted=place(resize(three,size),size+91,size+53,71,31);
            const auto found=recognizer.recognize(shifted,size);
            std::cout<<"escala "<<size<<" presenca "<<int(found.presence)<<" stacks "<<found.stacks.value_or(0)<<" confianca "<<found.confidence<<"\n";
            check(found.presence==aa::Presence::Present && found.stacks==3u,"Busca em posicao e escala calibradas");
            check(std::abs(found.icon.x-71)<=2 && std::abs(found.icon.y-31)<=2,"Regiao relativa a ROI");
            check(recognizer.recognize(resize(two,size),size).stacks==2u,"Escala do contador 2");
            check(!recognizer.recognize(resize(none,size),size).stacks,"Sem numero em qualquer escala");
        }
        aa::Image blank{160,80,std::vector<std::uint8_t>(160*80*4,30)};
        check(recognizer.recognize(blank,64).presence==aa::Presence::Unknown,"Frame uniforme nao confirma ausencia");
        const aa::Image black{160,80,std::vector<std::uint8_t>(160*80*4,0)};
        check(recognizer.recognize(black,64).presence==aa::Presence::Unknown,"Frame preto nao confirma ausencia");
        check(recognizer.recognize({},64).presence==aa::Presence::Unknown,"Frame invalido nao pode indicar ausencia");
        check(recognizer.recognize(three,0).presence==aa::Presence::Unknown,"Calibracao invalida deve falhar conservadoramente");
        check(recognizer.recognize(aa::loadImage(assets/"assassin-screen-3.png"),64).stacks==3u,"Contador 3 em captura real independente");
        for(const auto* name:{"other-food.png","other-buff.png"})
            check(recognizer.recognize(aa::loadImage(assets/name),64).presence==aa::Presence::Absent,"Outros buffs nao sao Espirito Assassino");
        auto purple=blank;
        for(std::size_t i=0;i<purple.bgra.size();i+=4) { purple.bgra[i]=220;purple.bgra[i+1]=30;purple.bgra[i+2]=190; }
        check(recognizer.recognize(purple,64).presence!=aa::Presence::Present,"Cor roxa sozinha nao identifica buff");
        auto dark=three;
        for(int y=7;y<57;++y) for(int x=7;x<57;++x) {
            if((x-32)*(x-32)+(y-32)*(y-32)>24*24 || (x>33 && y>31)) continue;
            if(x<32 || y<32) for(int c=0;c<3;++c) dark.bgra[(y*64+x)*4+c]=static_cast<std::uint8_t>(dark.bgra[(y*64+x)*4+c]*.30f);
        }
        check(recognizer.recognize(dark,64).stacks==3u,"Escurecimento radial simulado preserva identidade e contador");
        auto unreadable=three;
        for(int y=32;y<64;++y) for(int x=34;x<64;++x) for(int c=0;c<3;++c) unreadable.bgra[(y*64+x)*4+c]=0;
        check(recognizer.recognize(unreadable,64).presence==aa::Presence::Present && !recognizer.recognize(unreadable,64).stacks,"Numero oculto e desconhecido");
        auto duplicate=place(three,180,70,0,0);
        for(int y=0;y<64;++y) std::copy_n(two.bgra.data()+y*64*4,64*4,duplicate.bgra.data()+(y*180+110)*4);
        check(recognizer.recognize(duplicate,64).presence==aa::Presence::Unknown,"Dois candidatos nao selecionam stack arbitrario");
        aa::Recognizer missing(assets/"nao-existe");
        check(missing.recognize(three,64).presence==aa::Presence::Unknown,"Referencia ausente nunca confirma ausencia do buff");
        missing.setReference(none);
        check(missing.recognize(three,64).presence==aa::Presence::Present,"Referencia cadastrada deve substituir a identidade");
        missing.setReference({});
        check(missing.recognize(three,64).presence==aa::Presence::Unknown,"Referencia invalida apaga identidade anterior");
        struct RealCase { const char* file; aa::Presence presence; std::optional<unsigned> stacks; };
        const RealCase liveCases[]={
            {"12025625-stacks-unknown.png",aa::Presence::Absent,{}},
            {"12036046-stacks-unknown.png",aa::Presence::Present,{}},
            {"12038093-stacks-2.png",aa::Presence::Present,2u},
            {"12038609-stacks-unknown.png",aa::Presence::Present,2u},
            {"12038671-stacks-2.png",aa::Presence::Present,2u},
            {"12038734-stacks-unknown.png",aa::Presence::Present,2u},
            {"12039187-stacks-2.png",aa::Presence::Present,2u},
            {"12039500-stacks-unknown.png",aa::Presence::Present,2u},
            {"12040093-stacks-2.png",aa::Presence::Present,2u},
            {"12040156-stacks-unknown.png",aa::Presence::Present,3u},
            {"12040312-stacks-3.png",aa::Presence::Present,3u},
            {"12042765-stacks-unknown.png",aa::Presence::Present,3u},
            {"12042796-stacks-3.png",aa::Presence::Present,3u},
            {"12044562-stacks-unknown.png",aa::Presence::Present,3u},
            {"12044640-stacks-3.png",aa::Presence::Present,3u},
            {"12044656-stacks-unknown.png",aa::Presence::Present,3u},
            {"12044812-stacks-3.png",aa::Presence::Present,3u},
            {"12046375-stacks-unknown.png",aa::Presence::Present,3u},
            {"12046484-stacks-3.png",aa::Presence::Present,3u},
            {"12046531-stacks-unknown.png",aa::Presence::Present,3u},
            {"12046609-stacks-3.png",aa::Presence::Present,3u},
            {"12047375-stacks-unknown.png",aa::Presence::Absent,{}}
        };
        const auto live=assets.parent_path()/"tests"/"fixtures"/"recognition-live";
        int failures=0;
        for(const auto& sample:liveCases) {
            const auto actual=recognizer.recognize(aa::loadImage(live/sample.file),64);
            if(actual.presence!=sample.presence || actual.stacks!=sample.stacks) {
                ++failures;
                std::cerr<<"Amostra "<<sample.file<<": esperado "<<sample.stacks.value_or(0)<<", obtido "<<actual.stacks.value_or(0)<<"\n";
            }
        }
        std::cout<<"Amostras reais: "<<(std::size(liveCases)-failures)<<"/"<<std::size(liveCases)<<" corretas\n";
        check(failures==0,"ROIs reais distinguem 2, 3, sem numero e ausente sem oscilar");
        check(recognizer.recognize(aa::loadImage(live/"12040312-stacks-3.png"),64).stacks==3u,"Estado inicial com 3 real");
        check(!recognizer.recognize(aa::loadImage(live/"12036046-stacks-unknown.png"),64).stacks,"Numero desaparecido nao retem 3 anterior");
        for(const auto* extension:{".png",".bmp"}) {
            const auto path=std::filesystem::temp_directory_path()/(std::string("aa-image-roundtrip")+extension);
            aa::saveImage(three,path);
            const auto read=aa::loadImage(path);
            check(read.width==64 && read.height==64,"WIC preserva dimensoes ao salvar");
            for(std::size_t i=0;i<three.bgra.size();++i) if(i%4!=3) check(read.bgra[i]==three.bgra[i],"WIC preserva canais RGB");
            std::filesystem::remove(path);
        }
        const auto benchmark=place(resize(three,44),420,110,217,31);
        const auto start=std::chrono::steady_clock::now();
        for(int i=0;i<20;++i) check(recognizer.recognize(benchmark,44).stacks==3u,"Reconhecimento repetido permanece estavel");
        std::cout<<"ROI 420x110 media ms: "<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/20<<"\n";
        std::cout<<"reconhecimento: verificacoes passaram\n";
        if(SUCCEEDED(com)) CoUninitialize();
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"FALHOU: "<<e.what()<<"\n";
        if(SUCCEEDED(com)) CoUninitialize();
        return 1;
    }
}
