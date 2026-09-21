#include "recognition.h"
#include "../resources/resource.h"
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
aa::Image genericStatus() {
    aa::Image out{64,64,std::vector<std::uint8_t>(64*64*4,255)};
    for(int y=0;y<64;++y) for(int x=0;x<64;++x) {
        auto* pixel=out.bgra.data()+(std::size_t(y)*64+x)*4;
        pixel[0]=static_cast<std::uint8_t>(20+(x/8)%2*25);
        pixel[1]=static_cast<std::uint8_t>(70+(y/8)%3*30);
        pixel[2]=static_cast<std::uint8_t>(25+(x*3+y*5)%57);
    }
    return out;
}
aa::Image withCounter(aa::Image image,const char* glyph) {
    // Glifos sintéticos independentes dos recursos de Espírito Assassino.
    for(int y=0;y<7;++y) for(int x=0;x<5;++x) if(glyph[y*5+x]=='1')
        for(int dy=0;dy<3;++dy) for(int dx=0;dx<3;++dx)
            for(int channel=0;channel<3;++channel) image.bgra[((36+y*3+dy)*64+41+x*3+dx)*4+channel]=255;
    return image;
}
void checkCircularSearch(const aa::Recognizer& recognizer,const aa::Image& three) {
    aa::Image blankCircle{64,64,std::vector<std::uint8_t>(64*64*4,0)};
    check(recognizer.recognize(blankCircle,32,aa::RegionShape::Circle).presence==aa::Presence::Unknown,"circulo uniforme deve ficar desconhecido");
    blankCircle.bgra[0]=255;
    check(recognizer.recognize(blankCircle,32,aa::RegionShape::Circle).presence==aa::Presence::Unknown,"pixel fora do circulo confirmou ausencia em interior uniforme");
    check(recognizer.recognize(blankCircle,32).presence==aa::Presence::Absent,"fixture retangular com informacao visual mudou");
    for(int size:{32,64,100}) {
        const auto icon=resize(three,size);
        check(recognizer.recognize(icon,size,aa::RegionShape::Circle).stacks==3u,"circulo justo recortou o contador de 3");
    }
    const auto icon=resize(three,32);
    auto outside=place(icon,192,192,0,0);
    check(recognizer.recognize(outside,32).presence==aa::Presence::Present,"fixture externa nao reconhecida no retangulo");
    check(recognizer.recognize(outside,32,aa::RegionShape::Circle).presence!=aa::Presence::Present,"status externo acionou busca circular");
    auto both=outside;
    for(int y=0;y<32;++y)for(int x=0;x<32;++x) {
        const auto from=(static_cast<std::size_t>(y)*32+x)*4,to=(static_cast<std::size_t>(y+80)*192+x+80)*4;
        std::copy_n(icon.bgra.data()+from,4,both.bgra.data()+to);
        // O candidato interno é ligeiramente menos parecido que o externo.
        if(x<16&&y<16)both.bgra[to+1]=static_cast<std::uint8_t>(std::min(255,static_cast<int>(both.bgra[to+1])+10));
    }
    const auto found=recognizer.recognize(both,32,aa::RegionShape::Circle);
    check(found.presence==aa::Presence::Present&&found.stacks==3u&&std::abs(found.icon.x-80)<=2&&std::abs(found.icon.y-80)<=2,
          "candidato externo escondeu interno ou causou ambiguidade circular");
    check(recognizer.recognize(both,32).presence==aa::Presence::Unknown,"fixture dupla nao produz ambiguidade retangular");
    check(recognizer.recognize(place(icon,80,64,20,16),32,aa::RegionShape::Circle).presence==aa::Presence::Unknown,
          "busca circular aceitou geometria nao quadrada");
    check(recognizer.recognize(three,64,static_cast<aa::RegionShape>(8)).presence==aa::Presence::Unknown,"formato invalido nao bloqueou busca");
}
void checkGenericCounters() {
    const auto identity=genericStatus();
    const auto five=withCounter(identity,"11111" "10000" "10000" "11111" "00001" "00001" "11111");
    const auto seven=withCounter(identity,"11111" "00001" "00010" "00100" "01000" "01000" "01000");
    aa::Recognizer custom;
    custom.setReference(identity);
    check(custom.recognize(five,64).presence==aa::Presence::Present && !custom.recognize(five,64).stacks,
        "Status generico sem amostras reconhece presenca sem inventar contador");
    check(custom.setStackReference(5,five) && custom.setStackReference(7,seven),"Cadastrar amostras rotuladas 5 e 7");
    for(int size:{24,32,44,64,80,100}) {
        check(custom.recognize(resize(five,size),size).stacks==5u,"Contador generico 5 em escala calibrada");
        check(custom.recognize(resize(seven,size),size).stacks==7u,"Contador generico 7 em escala calibrada");
        check(!custom.recognize(resize(identity,size),size).stacks,"Status generico sem numero continua desconhecido");
    }
    check(custom.recognize(place(five,160,96,73,19),64).stacks==5u,"Contador generico localizado dentro da ROI");
    aa::Recognizer independent;
    independent.setReference(identity);
    check(independent.setStackReference(7,seven),"Outro status recebe sua propria amostra");
    check(!independent.recognize(five,64).stacks && independent.recognize(seven,64).stacks==7u,
        "Status independente nao reutiliza amostra 5 de outra instancia");
    check(custom.setStackReference(7,five),"Substituir amostra de um rotulo cadastrado");
    check(!custom.recognize(five,64).stacks,"Amostras iguais com rotulos diferentes deixam contador ambiguo");
    check(!custom.recognize(seven,64).stacks,"Substituicao remove a amostra antiga do rotulo");
    check(independent.recognize(seven,64).stacks==7u,"Substituicao nao altera outra instancia");
    custom.clearStackReferences();
    check(custom.recognize(five,64).presence==aa::Presence::Present && !custom.recognize(five,64).stacks,
        "Limpar contadores preserva identidade e remove todas as amostras");
    for(unsigned value:{1u,99u}) {
        custom.clearStackReferences();
        check(custom.setStackReference(value,five),"Limites 1 e 99 aceitam amostras rotuladas");
        check(custom.recognize(five,64).stacks==value,"O rotulo cadastrado define o valor reconhecido");
        check(!custom.recognize(identity,64).stacks,"Nem rotulo 1 permite inferir contador sem numero visivel");
    }
    for(unsigned value:{0u,100u,~0u}) check(!custom.setStackReference(value,seven),"Rotulo fora de 1 a 99 deve ser recusado");
    for(const auto& invalid:{aa::Image{},resize(seven,23),resize(seven,257)})
        check(!custom.setStackReference(99,invalid),"Imagem de contador invalida deve ser recusada");
    check(custom.recognize(five,64).stacks==99u && !custom.recognize(seven,64).stacks,
        "Cadastro invalido nao substitui nem adiciona amostras");
    custom.setReference(identity);
    check(!custom.recognize(five,64).stacks,"Trocar referencia limpa contadores cadastrados");
    check(!custom.setStackReference(1,identity),"Recorte sem numero deve ser recusado como amostra");
    check(!custom.recognize(identity,64).stacks,"Amostra sem silhueta numerica nao pode gerar contador 1");
    for(const auto& invalid:{aa::Image{},resize(identity,23),resize(identity,257)}) {
        custom.setReference(invalid);
        check(custom.recognize(identity,64).presence==aa::Presence::Unknown,"Referencia fora dos limites apaga identidade anterior");
    }
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
        aa::Recognizer embedded;
        const auto sameImage=[](const aa::Image& a,const aa::Image& b){
            return a.width==b.width&&a.height==b.height&&a.bgra==b.bgra;
        };
        check(sameImage(aa::loadImageResource(IDR_ASSASSIN_NONE),none),"Referencia sem numero embutida difere do arquivo original");
        check(sameImage(aa::loadImageResource(IDR_ASSASSIN_2),two),"Referencia 2 embutida difere do arquivo original");
        check(sameImage(aa::loadImageResource(IDR_ASSASSIN_3),three),"Referencia 3 embutida difere do arquivo original");
        bool rejectedResource=false;
        try { aa::loadImageResource(65535); } catch(const std::runtime_error&) { rejectedResource=true; }
        check(rejectedResource,"Recurso embutido ausente deve informar erro");
        const auto result=recognizer.recognize(three,64);
        check(result.presence==aa::Presence::Present,"Buff real com 3 deve estar presente");
        check(result.stacks==3u,"Numero real 3 deve ser lido separadamente");
        checkCircularSearch(recognizer,three);
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
        bool rejectedModel=false;
        try { aa::Recognizer missing(assets/"nao-existe"); } catch(const std::runtime_error&) { rejectedModel=true; }
        check(rejectedModel,"Modelo externo ausente deve informar erro sem ocultar a falha");
        aa::Recognizer custom;
        custom.setReference(none);
        check(custom.recognize(three,64).presence==aa::Presence::Present,"Referencia cadastrada deve substituir a identidade");
        check(!custom.recognize(three,64).stacks && !custom.recognize(two,64).stacks,
            "Status cadastrado nao pode herdar contadores do preset");
        int singleClassFailures=0;
        for(unsigned value:{2u,3u}) {
            custom.setReference(value==2?two:three);
            check(custom.setStackReference(value,value==2?two:three),"Cadastrar somente a contagem desejada");
            for(int size:{24,32,44,64,80,100}) {
                check(custom.setStackReference(value,resize(value==2?two:three,size)),
                    "Contador visivel pode ser cadastrado em diferentes escalas");
                check(custom.recognize(resize(value==2?two:three,size),size).stacks==value,
                    "Amostra unica reconhece sua propria contagem");
            }
            custom.setReference(value==2?three:two);
            check(custom.setStackReference(value,value==2?two:three),"A identidade pode conter outro numero visivel");
            for(int size:{24,32,44,64,80,100}) {
                const auto other=custom.recognize(resize(value==2?three:two,size),size);
                check(other.presence==aa::Presence::Present,"Identidade com numero visivel deve reconhecer presenca");
                if(other.stacks) {
                    ++singleClassFailures;
                    std::cerr<<"Classe unica "<<value<<" confundiu outro numero na escala "<<size<<"\n";
                }
            }
        }
        custom.setReference(none);
        for(int size:{24,32,44,64,80,100}) {
            if(custom.setStackReference(1,resize(none,size))) {
                ++singleClassFailures;
                std::cerr<<"Recorte real sem numero foi aceito como amostra na escala "<<size<<"\n";
            }
            if(custom.recognize(resize(none,size),size).stacks) {
                ++singleClassFailures;
                std::cerr<<"Recorte real sem numero produziu contador 1 na escala "<<size<<"\n";
            }
        }
        check(singleClassFailures==0,"Amostra unica nao pode absorver outro numero ou recorte sem contador");
        custom.setReference({});
        check(custom.recognize(three,64).presence==aa::Presence::Unknown,"Referencia invalida apaga identidade anterior");
        checkGenericCounters();
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
        aa::Recognizer onlyTwo,onlyThree;
        onlyTwo.clearStackReferences();
        onlyThree.clearStackReferences();
        check(onlyTwo.setStackReference(2,two) && onlyThree.setStackReference(3,three),"Contadores individuais para capturas reais");
        int failures=0;
        for(const auto& sample:liveCases) {
            const auto screenshot=aa::loadImage(live/sample.file);
            const auto actual=recognizer.recognize(screenshot,64);
            const auto packaged=embedded.recognize(screenshot,64);
            check(packaged.presence==actual.presence&&packaged.stacks==actual.stacks&&packaged.confidence==actual.confidence,
                "Modelo embutido mudou a leitura de uma captura real");
            check(onlyTwo.recognize(screenshot,64).stacks==(sample.stacks==2u?sample.stacks:std::nullopt),
                "Classe unica 2 distingue outros numeros e ausencia de numero nos recortes reais");
            check(onlyThree.recognize(screenshot,64).stacks==(sample.stacks==3u?sample.stacks:std::nullopt),
                "Classe unica 3 distingue outros numeros e ausencia de numero nos recortes reais");
            if(actual.presence!=sample.presence || actual.stacks!=sample.stacks) {
                ++failures;
                std::cerr<<"Amostra "<<sample.file<<": esperado "<<sample.stacks.value_or(0)<<", obtido "<<actual.stacks.value_or(0)<<"\n";
            }
        }
        std::cout<<"Amostras reais: "<<(std::size(liveCases)-failures)<<"/"<<std::size(liveCases)<<" corretas\n";
        check(failures==0,"ROIs reais distinguem 2, 3, sem numero e ausente sem oscilar");
        for(const auto* name:{"other-food.png","other-buff.png"})
            check(embedded.recognize(aa::loadImage(assets/name),64).presence==aa::Presence::Absent,
                "Modelo embutido confundiu outro buff com Espirito Assassino");
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
