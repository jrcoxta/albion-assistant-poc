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
std::optional<float> radialRemaining(const Image& image,Region icon,const Image& reference) {
    if(!reference.valid()) return {};
    struct Point { int x,y; float angle; const std::uint8_t* reference; };
    std::vector<Point> points;
    constexpr float pi=3.14159265358979323846f;
    // O aro não recebe a mesma sombra. O contador e o centro também não
    // oferecem uma frente legível; a máscara não estima a área encoberta.
    const int step=std::max(1,icon.width/48);
    for(int y=0;y<icon.height;y+=step) for(int x=0;x<icon.width;x+=step) {
        const float u=(x+.5f)/icon.width,v=(y+.5f)/icon.height;
        const float radius=std::hypot(u-.5f,v-.5f);
        if(radius<.15f || radius>.31f || (u>.52f && v>.50f)) continue;
        const auto* pixel=reference.bgra.data()+(std::size_t(static_cast<int>(v*reference.height))*reference.width+
            static_cast<int>(u*reference.width))*4;
        if(std::max({pixel[0],pixel[1],pixel[2]})<50) continue;
        float angle=std::atan2(u-.5f,.5f-v)*180/pi;
        if(angle<0)angle+=360;
        points.push_back({x,y,angle,pixel});
    }
    if(points.size()<40) return {};
    // A busca de identidade ignora brilho e pode variar alguns pixels com a
    // sombra. Refinar pela cromaticidade antes de comparar o relógio.
    float alignmentError=2;
    int alignedX=icon.x,alignedY=icon.y;
    const int alignment=std::max(2,icon.width/32);
    for(int dy=-alignment;dy<=alignment;++dy) for(int dx=-alignment;dx<=alignment;++dx) {
        const int left=icon.x+dx,top=icon.y+dy;
        if(left<0 || top<0 || left+icon.width>image.width || top+icon.height>image.height) continue;
        float error=0;
        for(const auto& p:points) {
            const float total=static_cast<float>(p.reference[0])+p.reference[1]+p.reference[2]+1;
            error+=difference({p.reference[0]/total,p.reference[1]/total,p.reference[2]/total},color(image,left+p.x,top+p.y));
        }
        error/=static_cast<float>(points.size());
        if(error<alignmentError) { alignmentError=error; alignedX=left; alignedY=top; }
    }
    if(alignmentError>.055f) return {};
    std::vector<float> ratios;
    for(const auto& p:points) {
        const auto* observed=image.bgra.data()+(std::size_t(alignedY+p.y)*image.width+alignedX+p.x)*4;
        float product=0,squared=0;
        for(int c=0;c<3;++c) { product+=static_cast<float>(observed[c])*p.reference[c]; squared+=static_cast<float>(p.reference[c])*p.reference[c]; }
        ratios.push_back(product/squared);
    }
    auto sorted=ratios;
    std::sort(sorted.begin(),sorted.end());
    // O primeiro quadrante pode conter poucos pixels de sombra plena; o
    // percentil 10 mistura a frente suavizada com esse nível nas ROIs reais.
    const float dark=sorted[sorted.size()/20],light=sorted[sorted.size()*19/20],contrast=light-dark;
    // Ambos os níveis precisam estar visíveis. Uma referência já sombreada
    // pode produzir razões acima de 1, e uma tela escura não define progresso.
    if(light<.85f || light>1.12f || dark<.10f || dark>.65f || contrast<.30f ||
        sorted[sorted.size()*97/100]>1.18f) return {};
    std::array<float,361> errors{};
    float bestError=1;
    int bestAngle=0;
    for(int angle=1;angle<360;++angle) {
        float error=0;
        for(std::size_t i=0;i<points.size();++i)
            error+=std::abs(ratios[i]-(points[i].angle<angle?dark:light));
        error/=static_cast<float>(points.size())*contrast;
        errors[angle]=error;
        if(error<bestError) { bestError=error; bestAngle=angle; }
    }
    if(bestError>.075f || bestAngle<15 || bestAngle>345) return {};
    int first=bestAngle,last=bestAngle;
    for(int angle=1;angle<360;++angle) if(errors[angle]<=bestError+.012f) { first=std::min(first,angle); last=std::max(last,angle); }
    if(last-first>16) return {};
    // A grade de pixels pode dar o mesmo ajuste para vários graus. Usar o
    // centro da faixa evita favorecer sua primeira extremidade, sobretudo em40px.
    bestAngle=(first+last)/2;
    int startTotal=0,startDark=0,endTotal=0,endLight=0,before=0,after=0,beforeTotal=0,afterTotal=0;
    for(std::size_t i=0;i<points.size();++i) {
        const float angle=points[i].angle,shade=(light-ratios[i])/contrast;
        if(angle>3 && angle<12) { ++startTotal; startDark+=shade>.55f; }
        if(angle>348 && angle<357) { ++endTotal; endLight+=shade<.35f; }
        if(angle>bestAngle-8 && angle<bestAngle-2) { ++beforeTotal; before+=shade>.65f; }
        if(angle>bestAngle+2 && angle<bestAngle+8) { ++afterTotal; after+=shade<.35f; }
    }
    // Exigir a origem escura no topo e suporte observado dos dois lados evita
    // tratar uma referência parcialmente escura ou o contador como a frente.
    // Em 40 px certas janelas angulares contêm só quatro pixels. Exigir os
    // quatro corretos nesses casos, mantendo cinco quando a grade os oferece.
    if(startTotal<4 || endTotal<4 || startDark<startTotal*.75f || endLight<endTotal*.75f ||
       beforeTotal<4 || afterTotal<4 || before<std::min(5,beforeTotal) || after<std::min(5,afterTotal)) return {};
    return 1.f-bestAngle/360.f;
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
    // Renderizações nativas pequenas preservam a silhueta do contador;
    // não mudam a identidade nem os limiares de confiança.
    stackReferences_.push_back({2,load(L"assassin-2-40.png",IDR_ASSASSIN_2_40)});
    stackReferences_.push_back({3,load(L"assassin-3-40.png",IDR_ASSASSIN_3_40)});
}
void Recognizer::setReference(const Image& image) {
    references_.clear();
    clearStackReferences();
    clockReference_={};
    if(validReference(image)) references_.push_back(image);
}
void Recognizer::clearStackReferences() {
    stackReferences_.clear();
}
bool Recognizer::setClockReference(const Image& image) {
    clockReference_={};
    if(!validReference(image) || image.width!=image.height) return false;
    clockReference_=image;
    return true;
}
bool Recognizer::clockReady() const { return clockReference_.valid(); }
bool Recognizer::setStackReference(unsigned value,const Image& image) {
    if(value<1 || value>99 || !validReference(image) || !hasCounterInk(image)) return false;
    // Uma amostra cadastrada substitui todas as variantes desse rótulo.
    std::erase_if(stackReferences_,[value](const auto& reference){return reference.value==value;});
    stackReferences_.push_back({value,image});
    return true;
}
Detection Recognizer::recognizeNearSize(const Image& image,int iconSize,RegionShape searchShape) const {
    auto result=recognize(image,iconSize,searchShape);
    // A borda selecionada pode medir dois pixels além do ícone renderizado.
    // A identidade tolera isso, mas o aro temporal não; reaproveitar somente
    // sua fração de uma escala vizinha que localize o mesmo ícone.
    if(result.presence==Presence::Present&&clockReady()&&!result.remainingFraction) {
        std::optional<Detection> timed;
        for(int offset:{-2,-1,1,2}) {
            const int size=iconSize+offset;
            if(size<24||size>256||size>image.width||size>image.height)continue;
            auto candidate=recognize(image,size,searchShape);
            const auto distance=std::hypot((candidate.icon.x+candidate.icon.width/2.0)-(result.icon.x+result.icon.width/2.0),
                                           (candidate.icon.y+candidate.icon.height/2.0)-(result.icon.y+result.icon.height/2.0));
            if(candidate.presence==Presence::Present&&candidate.remainingFraction&&distance<=iconSize/6.0&&
               (!timed||candidate.confidence>timed->confidence))timed=std::move(candidate);
        }
        if(timed)result.remainingFraction=timed->remainingFraction;
    }
    // Não reinterpretar uma captura inválida, uniforme ou ambígua. A tolerância
    // corrige somente o tamanho manual, sem diminuir os limiares de identidade.
    if(result.presence==Presence::Present ||
       (result.detail!="Buff nao localizado" && result.detail!="Identidade incerta"))return result;
    std::optional<Detection> found;
    for(int offset:{-2,-1,1,2}) {
        const int size=iconSize+offset;
        if(size<24||size>256||size>image.width||size>image.height)continue;
        auto candidate=recognize(image,size,searchShape);
        if(candidate.detail=="Mais de um candidato ao buff")return candidate;
        if(candidate.presence==Presence::Present) {
            if(found&&(std::abs((candidate.icon.x+candidate.icon.width/2.0)-(found->icon.x+found->icon.width/2.0))>iconSize/2.0 ||
                       std::abs((candidate.icon.y+candidate.icon.height/2.0)-(found->icon.y+found->icon.height/2.0))>iconSize/2.0)) {
                result={};result.detail="Mais de um candidato ao buff";return result;
            }
            if(!found||candidate.confidence>found->confidence)found=std::move(candidate);
        }else if(candidate.confidence>result.confidence)result=std::move(candidate);
    }
    return found?*found:result;
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
    if(clockReady()) out.remainingFraction=radialRemaining(image,out.icon,clockReference_);
    float bestScore=0,secondScore=0;
    unsigned bestValue=0;
    std::array<float,100> classScores{};
    for(const auto& reference:stackReferences_)
        classScores[reference.value]=std::max(classScores[reference.value],digitScore(image,out.icon,reference.image));
    // A margem compara valores distintos, não variantes do mesmo número.
    for(unsigned value=1;value<classScores.size();++value) {
        const float score=classScores[value];
        if(score>bestScore) {
            secondScore=bestScore;
            bestScore=score;
            bestValue=value;
        } else secondScore=std::max(secondScore,score);
    }
    // A margem entre classes não protege quando o número observado não foi cadastrado.
    if(bestScore>.80f && bestScore-secondScore>.065f) out.stacks=bestValue;
    out.detail=out.stacks?"Buff e contador reconhecidos":"Buff presente; contador desconhecido";
    return out;
}
}
