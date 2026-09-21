#include "recognition.h"
#include "../resources/resource.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
namespace aa {
namespace {
bool validReference(const Image& image) {
    return image.valid() && image.width>=24 && image.height>=24 && image.width<=256 && image.height<=256;
}
struct Color { float b,g,r; };
Color color(const Image& image,int x,int y) {
    const auto* p=image.bgra.data()+(std::size_t(y)*image.width+x)*4;
    const float total=static_cast<float>(p[0])+p[1]+p[2]+1.f;
    return {p[0]/total,p[1]/total,p[2]/total};
}
float difference(Color a,Color b) { return std::abs(a.b-b.b)+std::abs(a.g-b.g)+std::abs(a.r-b.r); }
bool white(const Image& image,int x,int y) {
    if(x<0 || y<0 || x>=image.width || y>=image.height) return false;
    const auto* p=image.bgra.data()+(std::size_t(y)*image.width+x)*4;
    const auto lo=std::min({p[0],p[1],p[2]}),hi=std::max({p[0],p[1],p[2]});
    return lo>125 && lo>hi*.78f;
}
bool hasCounterInk(const Image& image) {
    // O aro branco do ícone não basta: exigir tinta também no interior do contador.
    int ink=0;
    for(int y=image.height*49/100;y<image.height*80/100;++y)
        for(int x=image.width*53/100;x<image.width*80/100;++x) ink+=white(image,x,y);
    return ink>=std::max(3,image.width*image.height/200);
}
float digitScore(const Image& roi,Region icon,const Image& reference) {
    float best=0;
    // The real counter is white with a black outline, in the lower right.
    // Comparing its silhouette never derives stacks from the icon's color.
    const int firstX=icon.width*53/100,firstY=icon.height*49/100;
    const int lastX=icon.width*94/100,lastY=icon.height*98/100;
    // O relógio pode trocar a referência de identidade vencedora; os recortes
    // reais diferem alguns pixels em relação ao aro. Alinhar o dígito de forma
    // independente evita confundir essa variação com um contador ilegível.
    const int alignment=std::max(1,icon.width/16);
    for(int dy=-alignment;dy<=alignment;++dy) for(int dx=-alignment;dx<=alignment;++dx) {
        int expected=0,observed=0,intersection=0;
        for(int y=firstY;y<lastY;++y) for(int x=firstX;x<lastX;++x) {
            const bool a=white(reference,x*reference.width/icon.width,y*reference.height/icon.height);
            const bool b=white(roi,icon.x+x+dx,icon.y+y+dy);
            expected+=a; observed+=b; intersection+=a && b;
        }
        if(expected>=3 && observed>=3) best=std::max(best,2.f*intersection/(expected+observed));
    }
    return best;
}
}
Recognizer::Recognizer(const std::filesystem::path& assetsDir) {
    const auto load=[&](const wchar_t* name,int id){
        return assetsDir.empty()?loadImageResource(id):loadImage(assetsDir/name);
    };
    references_.push_back(load(L"assassin-none.png",IDR_ASSASSIN_NONE));
    stackReferences_.push_back({2,load(L"assassin-2.png",IDR_ASSASSIN_2)});
    stackReferences_.push_back({3,load(L"assassin-3.png",IDR_ASSASSIN_3)});
    for(const auto& reference:stackReferences_) references_.push_back(reference.image);
}
void Recognizer::setReference(const Image& image) {
    references_.clear();
    clearStackReferences();
    if(validReference(image)) references_.push_back(image);
}
void Recognizer::clearStackReferences() {
    stackReferences_.clear();
}
bool Recognizer::setStackReference(unsigned value,const Image& image) {
    if(value<1 || value>99 || !validReference(image) || !hasCounterInk(image)) return false;
    for(auto& reference:stackReferences_) if(reference.value==value) {
        reference.image=image;
        return true;
    }
    stackReferences_.push_back({value,image});
    return true;
}
Detection Recognizer::recognize(const Image& image,int iconSize,RegionShape searchShape) const {
    Detection out;
    const Region search{0,0,image.width,image.height,searchShape};
    if(!image.valid() || references_.empty() || iconSize<24 || iconSize>256 ||
        image.width<iconSize || image.height<iconSize || !search.valid()) { out.detail="Captura, referencia ou calibracao invalida"; return out; }
    std::array<int,3> minimum{255,255,255},maximum{};
    bool informative=false;
    for(std::size_t i=0;i<image.bgra.size();i+=4) {
        if(searchShape==RegionShape::Circle && !search.contains(
            static_cast<double>((i/4)%image.width)+.5,
            static_cast<double>((i/4)/image.width)+.5)) continue;
        for(int c=0;c<3;++c) {
            minimum[c]=std::min(minimum[c],int(image.bgra[i+c]));
            maximum[c]=std::max(maximum[c],int(image.bgra[i+c]));
            informative|=maximum[c]-minimum[c]>12;
        }
        if(informative) break;
    }
    if(!informative) { out.detail="Captura sem informacao visual suficiente"; return out; }
    struct Sample { int x,y; Color value; };
    std::vector<std::vector<Sample>> patterns;
    for(const auto& ref:references_) {
        auto& points=patterns.emplace_back();
        for(int y=0;y<13;++y) for(int x=0;x<13;++x) {
            const float u=(x+.5f)/13.f,v=(y+.5f)/13.f;
            if((u-.5f)*(u-.5f)+(v-.5f)*(v-.5f)>.155f || (u>.52f && v>.50f)) continue;
            points.push_back({static_cast<int>(u*iconSize),static_cast<int>(v*iconSize),
                color(ref,static_cast<int>(u*ref.width),static_cast<int>(v*ref.height))});
        }
    }
    auto errorAt=[&](int left,int top) {
        // A forma limita os centros desde a busca/refino, sem recortar o contador.
        if(searchShape==RegionShape::Circle&&!search.contains(left+iconSize/2.0,top+iconSize/2.0))return 2.f;
        float best=2;
        for(const auto& points:patterns) {
            float total=0;
            for(const auto& p:points) total+=difference(p.value,color(image,left+p.x,top+p.y));
            best=std::min(best,total/static_cast<float>(points.size()));
        }
        return best;
    };
    struct Candidate { int x,y; float error; };
    std::vector<Candidate> candidates;
    const int step=std::max(1,iconSize/16);
    // ponytail: exhaustive coarse ROI scan; a pyramid is only needed if measured ROI cost grows.
    for(int y=0;y<=image.height-iconSize;y+=step) for(int x=0;x<=image.width-iconSize;x+=step) {
        const float error=errorAt(x,y);
        if(error<.28f) candidates.push_back({x,y,error});
    }
    // Include right/bottom edges even when dimensions do not align with the coarse step.
    for(int y=0;y<=image.height-iconSize;y+=step) {
        const int x=image.width-iconSize; const float error=errorAt(x,y);
        if(error<.28f) candidates.push_back({x,y,error});
    }
    for(int x=0;x<=image.width-iconSize;++x) {
        const int y=image.height-iconSize; const float error=errorAt(x,y);
        if(error<.28f) candidates.push_back({x,y,error});
    }
    std::sort(candidates.begin(),candidates.end(),[](auto a,auto b){return a.error<b.error;});
    Candidate best{0,0,2};
    for(std::size_t i=0;i<std::min<std::size_t>(candidates.size(),12);++i) {
        const auto c=candidates[i];
        for(int y=std::max(0,c.y-step);y<=std::min(image.height-iconSize,c.y+step);++y)
            for(int x=std::max(0,c.x-step);x<=std::min(image.width-iconSize,c.x+step);++x) {
                const float error=errorAt(x,y);
                if(error<best.error) best={x,y,error};
            }
    }
    out.confidence=std::clamp(1.f-best.error,0.f,1.f);
    if(best.error>.19f) {
        out.presence=best.error<.25f?Presence::Unknown:Presence::Absent;
        out.detail=out.presence==Presence::Absent?"Buff nao localizado":"Identidade incerta";
        return out;
    }
    for(const auto& c:candidates) {
        if(c.error<.19f && (std::abs(c.x-best.x)>iconSize/2 || std::abs(c.y-best.y)>iconSize/2)) {
            out.detail="Mais de um candidato ao buff";
            return out;
        }
    }
    out.presence=Presence::Present;
    out.icon={best.x,best.y,iconSize,iconSize};
    float bestScore=0,secondScore=0;
    unsigned bestValue=0;
    for(const auto& reference:stackReferences_) {
        const float score=digitScore(image,out.icon,reference.image);
        if(score>bestScore) {
            secondScore=bestScore;
            bestScore=score;
            bestValue=reference.value;
        } else secondScore=std::max(secondScore,score);
    }
    // A margem entre classes não protege quando o número observado não foi cadastrado.
    if(bestScore>.80f && bestScore-secondScore>.065f) out.stacks=bestValue;
    out.detail=out.stacks?"Buff e contador reconhecidos":"Buff presente; contador desconhecido";
    return out;
}
}
