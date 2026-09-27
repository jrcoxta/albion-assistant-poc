#include "app.h"
#include "calibration.h"
#include "theme.h"
#include "skill_reader.h"
#include "../resources/resource.h"
#include <shlobj.h>
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <stdexcept>

namespace aaapp {
namespace {
aa::Image captureOnce(HWND target, RECT region){
    std::mutex mutex;std::condition_variable arrived;aa::Image snapshot;std::string failure;
    const auto requested=static_cast<std::int64_t>(GetTickCount64());aa::DesktopCapture single;
    single.start(target,region,[&](aa::CaptureFrame frame){
        std::lock_guard lock(mutex);
        if(frame.available&&frame.capturedMs>=requested&&!snapshot.valid()){
            snapshot=std::move(frame.image);arrived.notify_one();
        }else if(!frame.error.empty())failure=frame.error;
    });
    {std::unique_lock lock(mutex);arrived.wait_for(lock,std::chrono::milliseconds(3000),[&]{return snapshot.valid();});}
    single.stop();
    if(!snapshot.valid())throw std::runtime_error("Não consegui capturar o jogo. Volte ao Albion e tente novamente. "+failure);
    return snapshot;
}
class CapturePanelGuard {
public:
    explicit CapturePanelGuard(App& app):app_(app),visible_(IsWindowVisible(app.window)!=FALSE){
        app_.selecting=true;ShowWindow(app_.window,SW_HIDE);SetForegroundWindow(app_.target);
    }
    ~CapturePanelGuard(){
        app_.selecting=false;
        if(visible_){ShowWindow(app_.window,SW_SHOW);SetForegroundWindow(app_.window);}
    }
private:
    App& app_;bool visible_;
};
}
std::wstring widen(const std::string& value){
    if(value.empty())return {};
    const int size=MultiByteToWideChar(CP_UTF8,0,value.data(),static_cast<int>(value.size()),nullptr,0);
    std::wstring result(size,L' ');MultiByteToWideChar(CP_UTF8,0,value.data(),static_cast<int>(value.size()),result.data(),size);return result;
}
std::wstring text(HWND window){const int count=GetWindowTextLengthW(window);std::wstring value(count+1,L'\0');GetWindowTextW(window,value.data(),count+1);value.resize(count);return value;}
RECT rect(aa::Region area){return {area.x,area.y,area.x+area.width,area.y+area.height};}
bool fits(aa::Region area,int width,int height){return area.valid()&&area.x<width&&area.y<height&&area.width<=width-area.x&&area.height<=height-area.y;}
std::unique_ptr<aa::Recognizer> makeRecognizer(const aa::MonitorReader& reader){
    try{
    auto recognizer=std::make_unique<aa::Recognizer>();
    aa::Image reference;
    if(!reader.status.builtinAssassin){
        reference=aa::loadImage(reader.status.referencePath);
        if(!reference.valid()||reference.width<24||reference.height<24||reference.width>256||reference.height>256)
            throw std::runtime_error("A referência deve ter entre 24 e 256 pixels. Recapture o ícone na aba Status.");
        recognizer->setReference(reference);
    }
    if(reader.needsStacks){
        for(const auto& sample:reader.status.stacks)
            if(!recognizer->setStackReference(sample.value,aa::loadImage(sample.path)))throw std::runtime_error("Uma amostra de contador é inválida. Recapture esse valor na biblioteca de status.");
    }else recognizer->clearStackReferences();
    if(reader.needsClock) {
        // O relógio é opcional: uma referência temporal ruim não desativa presença/stacks.
        try {
            const bool assassinClock=reader.status.builtinAssassin ||
                (reference.valid()&&reference.width==reference.height&&
                 aa::Recognizer().recognize(reference,reference.width).presence==aa::Presence::Present);
            if(!reader.status.clockReferencePath.empty()){
                if(recognizer->setClockReference(aa::loadImage(reader.status.clockReferencePath))&&assassinClock){
                    const int resource=std::abs(reader.area.iconSize-40)<std::abs(reader.area.iconSize-64)?IDR_ASSASSIN_CLOCK_40:IDR_ASSASSIN_CLOCK;
                    recognizer->setClockFallback(aa::loadImageResource(resource),resource==IDR_ASSASSIN_CLOCK_40);
                }
            }else if(assassinClock){
                const int resource=std::abs(reader.area.iconSize-40)<std::abs(reader.area.iconSize-64)?IDR_ASSASSIN_CLOCK_40:IDR_ASSASSIN_CLOCK;
                recognizer->setClockReference(aa::loadImageResource(resource),resource==IDR_ASSASSIN_CLOCK_40);
            }
        }catch(const std::exception&){recognizer->setClockReference({});}
    }
    return recognizer;
    }catch(const std::exception& error){
        const int size=WideCharToMultiByte(CP_UTF8,0,reader.status.name.data(),static_cast<int>(reader.status.name.size()),nullptr,0,nullptr,nullptr);
        std::string name(size,' ');WideCharToMultiByte(CP_UTF8,0,reader.status.name.data(),static_cast<int>(reader.status.name.size()),name.data(),size,nullptr,nullptr);
        throw std::runtime_error("Status \""+name+"\": "+error.what());
    }
}
Screen screenOf(HWND target){
    Screen result;RECT client{};MONITORINFOEXW monitor{};monitor.cbSize=sizeof(monitor);
    if(!IsWindow(target)||!GetClientRect(target,&client)||client.right<=0||client.bottom<=0||!ClientToScreen(target,&result.origin)||
       !GetMonitorInfoW(MonitorFromWindow(target,MONITOR_DEFAULTTONULL),&monitor))throw std::runtime_error("Janela indisponível ou fora dos monitores. Conecte ao Albion novamente.");
    result.width=client.right;result.height=client.bottom;result.dpi=GetDpiForWindow(target);result.device=monitor.szDevice;return result;
}
std::string utf8(const std::wstring& value){
    if(value.empty())return {};
    const int size=WideCharToMultiByte(CP_UTF8,0,value.data(),static_cast<int>(value.size()),nullptr,0,nullptr,nullptr);
    if(!size)throw std::runtime_error("Não foi possível codificar a mensagem de tela.");
    std::string result(size,' ');WideCharToMultiByte(CP_UTF8,0,value.data(),static_cast<int>(value.size()),result.data(),size,nullptr,nullptr);return result;
}
bool matchesHudScreen(const aa::HudLayout& hud,const Screen& screen){
    return hud.clientWidth>0&&hud.clientHeight>0&&hud.clientWidth==screen.width&&hud.clientHeight==screen.height&&
        (!hud.monitorDpi||!screen.dpi||hud.monitorDpi==screen.dpi);
}
std::wstring hudScreenIssue(const aa::HudLayout& hud,const Screen& screen){
    if(matchesHudScreen(hud,screen))return {};
    const auto dpi=[](unsigned value){return value?std::to_wstring(value):L"desconhecido";};
    const bool sizeChanged=hud.clientWidth!=screen.width||hud.clientHeight!=screen.height;
    return L"HUD \""+hud.name+L"\": "+std::to_wstring(hud.clientWidth)+L" × "+std::to_wstring(hud.clientHeight)+L" px, DPI "+dpi(hud.monitorDpi)+
        L"; jogo: "+std::to_wstring(screen.width)+L" × "+std::to_wstring(screen.height)+L" px, DPI "+dpi(screen.dpi)+
        (sizeChanged?L". O tamanho da janela mudou; escolha ou calibre uma HUD para este tamanho.":
            L". O DPI mudou; confira a escala do Windows ou calibre uma HUD para ela.");
}
App::~App(){stop();if(badge)DestroyWindow(badge);if(font)DeleteObject(font);if(titleFont)DeleteObject(titleFont);}
void App::configureStorage(const std::filesystem::path& settingsFile){
    if(settingsFile.empty()){
        wchar_t folder[MAX_PATH]{};
        if(FAILED(SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA,nullptr,SHGFP_TYPE_CURRENT,folder)))throw std::runtime_error("Pasta de dados indisponível.");
        settingsPath=std::filesystem::path(folder)/L"AlbionAssistant"/L"settings.ini";
    }else settingsPath=std::filesystem::absolute(settingsFile);
    directory=settingsPath.parent_path();std::filesystem::create_directories(directory);
    workspacePath=directory/(settingsPath.filename()==L"settings.ini"?L"workspace.ini":settingsPath.stem().wstring()+L"-workspace.ini");
}
void App::load(){
    std::vector<aa::StackSample> legacyStacks;
    if(!std::filesystem::exists(workspacePath)&&(std::filesystem::exists(settingsPath)||std::filesystem::exists(directory/L"hud-profiles"))){
        // O legado usava estes dígitos para qualquer referência. Materializar uma vez mantém
        // essa capacidade nas configurações importadas, sem herança nos cadastros novos.
        legacyStacks={{2,storeImage(L"legacy",aa::loadImageResource(IDR_ASSASSIN_2))},{3,storeImage(L"legacy",aa::loadImageResource(IDR_ASSASSIN_3))}};
    }
    workspace=aa::loadWorkspace(workspacePath,settingsPath,legacyStacks);
    showOverlayInCapture=showOverlayInCapture||workspace.shareOverlayInCapture;
    if(!workspace.statuses.empty())selectedStatusId=workspace.statuses.front().id;
}
void App::commit(aa::Workspace changed){aa::saveWorkspace(workspacePath,changed);workspace=std::move(changed);}
aa::HudLayout* App::hud(){for(auto& h:workspace.huds)if(h.id==workspace.activeHudId)return &h;return nullptr;}
aa::SetProfile* App::set(){for(auto& s:workspace.sets)if(s.id==workspace.activeSetId)return &s;return nullptr;}
aa::StatusDefinition* App::selectedStatus(){for(auto& s:workspace.statuses)if(s.id==selectedStatusId)return &s;return nullptr;}
aa::HudArea* App::area(){auto* h=hud();return h&&selectedArea>=0&&selectedArea<static_cast<int>(h->areas.size())?&h->areas[selectedArea]:nullptr;}
aa::StatusRule* App::rule(){auto* s=set();return s&&selectedRule>=0&&selectedRule<static_cast<int>(s->rules.size())?&s->rules[selectedRule]:nullptr;}
bool App::geometryMatches()const{
    if(!target||!IsWindow(target)||IsIconic(target))return false;
    try{
        auto screen=screenOf(target);
        for(const auto& h:workspace.huds)if(h.id==workspace.activeHudId)return matchesHudScreen(h,screen);
    }catch(const std::exception&){}
    return false;
}
bool App::connect(){
    HWND found=nullptr;
    EnumWindows([](HWND candidate,LPARAM data)->BOOL{
        wchar_t title[256]{};GetWindowTextW(candidate,title,256);if(std::wstring(title)!=L"Albion Online Client")return TRUE;
        DWORD pid=0;GetWindowThreadProcessId(candidate,&pid);HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
        if(!process)return TRUE;wchar_t path[1024]{};DWORD length=1024;
        const BOOL ok=QueryFullProcessImageNameW(process,0,path,&length);CloseHandle(process);
        if(ok&&_wcsicmp(std::filesystem::path(path).filename().c_str(),L"Albion-Online.exe")==0){*reinterpret_cast<HWND*>(data)=candidate;return FALSE;}return TRUE;
    },reinterpret_cast<LPARAM>(&found));
    if(!found){error=L"Abra o Albion Online em janela ou janela sem bordas antes de conectar.";refreshStatus();return false;}
    stop();target=found;if(IsIconic(target))ShowWindow(target,SW_RESTORE);
    error=geometryMatches()?L"Jogo conectado. HUD compatível com esta tela.":L"Jogo conectado. Escolha ou configure uma HUD para esta tela.";refreshStatus();return true;
}
void App::stop(){
    capture.stop();running=false;previewUntil=0;++source;
    if(badge)ShowWindow(badge,SW_HIDE);
    for(auto& o:overlays)o->update(target,{},false);overlays.clear();
    if(testOverlay)testOverlay->update(target,{},false);
    recognizers.clear();readyReferences.clear();current.clear();currentReady.clear();lastReadings.clear();clocks.clear();clockPredictions.clear();lit.assign(plan.actions.size(),false);
    {std::lock_guard lock(mutex);latest.clear();latestReady.clear();latestSource=0;latestImage={};latestError.clear();pending=false;}
}
void App::start(){
    if(selecting)return;
    capturePreview={};InvalidateRect(window,nullptr,FALSE);
    saveEditor();stop();
    if((!target||!IsWindow(target))&&!connect())return;
    if(IsIconic(target))ShowWindow(target,SW_RESTORE);
    auto issues=aa::readinessIssues(workspace);
    try{
        const auto screen=screenOf(target);
        if(const auto* layout=hud()){
            const auto mismatch=hudScreenIssue(*layout,screen);
            if(!mismatch.empty())issues.insert(issues.begin(),mismatch);
        }
    }catch(const std::exception&){issues.insert(issues.begin(),L"Janela do jogo indisponível ou fora dos monitores. Conecte novamente.");}
    if(!issues.empty()){
        error=L"Antes de iniciar:\r\n";for(std::size_t i=0;i<std::min<std::size_t>(issues.size(),4);++i)error+=L"• "+issues[i]+L"\r\n";
        page=3;makeUI();return;
    }
    plan=aa::makeMonitorPlan(workspace);
    if(static_cast<std::int64_t>(plan.captureArea.width)*plan.captureArea.height>64000000)throw std::runtime_error("As regiões abrangem uma área grande demais. Use o jogo em um único monitor.");
    for(const auto& read:plan.readers)recognizers.push_back(read.kind==aa::MonitorReaderKind::Status?makeRecognizer(read):nullptr);
    for(const auto& action:plan.actions){
        if(action.rule.onlyWhenReady&&showOverlayInCapture)
            throw std::runtime_error("Desative 'Incluir overlay no compartilhamento' para ler a habilidade sem interferência do próprio destaque.");
        if(showOverlayInCapture){
            const auto padding=aa::overlayEffectPadding(action.rule.effect,action.target.width,action.target.height,action.target.shape);
            RECT destination=rect(action.target);InflateRect(&destination,padding,padding);
            for(const auto& reader:plan.readers){RECT overlap{},origin=rect(reader.area.region);if(IntersectRect(&overlap,&destination,&origin))throw std::runtime_error("No diagnóstico visual, a ação precisa ficar fora das regiões observadas.");}
        }
        auto overlay=std::make_unique<Overlay>();overlay->setCaptureVisible(showOverlayInCapture);overlay->initialize(instance);
        overlay->setColor(action.rule.triggers.empty()?action.rule.condition.color:action.rule.action.color);overlay->setEffect(action.rule.effect);overlay->setShape(action.target.shape);overlays.push_back(std::move(overlay));
        aa::Image reference;
        if(action.rule.onlyWhenReady){
            const auto* layout=hud();
            const auto found=std::find_if(layout->areas.begin(),layout->areas.end(),[&](const auto& area){return aa::sameName(area.name,action.rule.targetArea);});
            if(found==layout->areas.end())throw std::runtime_error("Área de habilidade não encontrada na HUD.");
            reference=aa::loadImage(found->readyReferencePath);
            if(!reference.valid()||reference.width!=action.target.width||reference.height!=action.target.height)
                throw std::runtime_error("Imagem de habilidade incompatível com a área. Capture-a novamente na HUD.");
        }
        readyReferences.push_back(std::move(reference));
    }
    current.resize(plan.readers.size());lit.assign(plan.actions.size(),false);running=true;const auto runSource=++source;
    error.clear();page=3;makeUI();SetForegroundWindow(target);
    try{
        capture.start(target,rect(plan.captureArea),[this,runSource](aa::CaptureFrame frame){
            std::vector<aa::Observation> batch(plan.readers.size());std::wstring failure=widen(frame.error);
            std::vector<bool> readyBatch(plan.actions.size(),false);
            for(std::size_t i=0;i<plan.readers.size();++i){
                batch[i].source=runSource;batch[i].capturedMs=frame.capturedMs;
                if(!frame.available)continue;
                try{auto roi=plan.readers[i].area.region;roi.x-=plan.captureArea.x;roi.y-=plan.captureArea.y;
                    const auto image=aa::cropImage(frame.image,roi);
                    if(plan.readers[i].kind==aa::MonitorReaderKind::Health)batch[i].healthFraction=aa::readHealthFraction(image,plan.readers[i].area.healthCalibration);
                    else batch[i].detection=recognizers[i]->recognizeNearSize(image,plan.readers[i].area.iconSize,roi.shape);
                }catch(const std::exception& e){failure=widen(e.what());}
            }
            if(frame.available)for(std::size_t i=0;i<plan.actions.size();++i)if(plan.actions[i].rule.onlyWhenReady){
                try{auto roi=plan.actions[i].target;roi.x-=plan.captureArea.x;roi.y-=plan.captureArea.y;
                    readyBatch[i]=aa::skillReady(aa::cropImage(frame.image,roi),readyReferences[i],roi.shape);
                }catch(const std::exception& e){failure=widen(e.what());}
            }
            {std::lock_guard lock(mutex);latest=std::move(batch);latestReady=std::move(readyBatch);latestSource=runSource;latestImage=std::move(frame.image);latestError=std::move(failure);}
            if(!pending.exchange(true)&&!PostMessageW(window,ResultMessage,0,0))pending=false;
        });
    }catch(...){stop();throw;}
}
void App::consume(){
    {std::lock_guard lock(mutex);pending=false;if(!running||latestSource!=source)return;current=latest;currentReady=latestReady;if(latestImage.valid())capturePreview=std::move(latestImage);error=latestError;}
    // Histórico somente informativo: evaluateMonitor continua recebendo current.
    if(!current.empty()&&std::all_of(current.begin(),current.end(),[this](const auto& observation){
        return observation.source==source&&observation.capturedMs>0;
    }))lastReadings=current;
    updateHighlight();refreshStatus();if(page==3){RECT area{px(24),px(486),px(832),px(602)};InvalidateRect(window,&area,FALSE);}
}
std::vector<std::optional<float>> App::evaluateReadings(std::int64_t now,bool targetReady){
    std::vector<std::optional<float>> remaining(plan.actions.size());
    clockPredictions.assign(plan.readers.size(),{});
    if(running&&targetReady&&current.size()==plan.readers.size()){
        if(clocks.size()!=plan.readers.size())clocks.assign(plan.readers.size(),{});
        auto predicted=current;
        for(std::size_t i=0;i<current.size();++i)if(plan.readers[i].needsClock){
            clockPredictions[i]=clocks[i].update(current[i],now,workspace.validityMs,source);
            predicted[i].detection.remainingFraction=clockPredictions[i].fraction;
        }
        auto ready=currentReady;
        const auto captured=current.front().capturedMs;
        if(captured<=0||captured>now||now-captured>std::min(workspace.validityMs,250))ready.assign(plan.actions.size(),false);
        lit=aa::evaluateMonitor(plan,predicted,now,workspace.validityMs,source,&remaining,&ready);
    }else{
        clocks.clear();lit.assign(plan.actions.size(),false);
    }
    return remaining;
}
void App::updateHighlight(){
    const auto now=static_cast<std::int64_t>(GetTickCount64());const bool geometry=geometryMatches();
    if(previewUntil){
        const bool active=static_cast<std::uint64_t>(now)<previewUntil&&geometry;
        if(testOverlay){testOverlay->setRemaining(active&&previewClock?std::optional<float>{static_cast<float>(previewUntil-now)/5000.f}:std::nullopt);testOverlay->update(target,rect(previewTarget),active);}
        if(active&&GetForegroundWindow()==target){
            const auto screen=screenOf(target);auto caption=std::wstring(previewClock?L"SIMULAÇÃO DO ARO · ":L"TESTE DA AÇÃO · ")+std::to_wstring((previewUntil-now+999)/1000)+L" s · F8 encerra";
            SetWindowTextW(badge,caption.c_str());SetWindowPos(badge,HWND_TOPMOST,screen.origin.x+(screen.width-px(450))/2,screen.origin.y+px(40),px(450),px(42),SWP_NOACTIVATE|SWP_SHOWWINDOW);
        }else if(badge)ShowWindow(badge,SW_HIDE);
        if(!active){previewUntil=0;error=L"Teste da ação encerrado. Ele não inicia a leitura das regras.";ShowWindow(window,SW_RESTORE);SetForegroundWindow(window);}
        return;
    }
    const auto remaining=evaluateReadings(now,geometry&&GetForegroundWindow()==target);
    for(std::size_t i=0;i<overlays.size();++i) {
        overlays[i]->setRemaining(remaining[i]);overlays[i]->update(target,rect(plan.actions[i].target),lit[i]);
    }
    if(diagnostics&&running){std::string state;for(bool on:lit)state+=on?'1':'0';
        if(state!=lastTrace){lastTrace=state;if(!trace.is_open())trace.open(directory/L"diagnostics.csv",std::ios::app);trace<<now<<",actions,"<<state<<'\n';trace.flush();}}
}
void App::testAction(){
    if(selecting)return;saveEditor();stop();if((!target||!IsWindow(target))&&!connect())return;
    if(IsIconic(target))ShowWindow(target,SW_RESTORE);
    auto* action=rule();auto* layout=hud();
    if(!action||!layout||!geometryMatches())throw std::runtime_error("Escolha uma regra e uma HUD compatível com esta tela para testar a ação.");
    const auto destination=std::find_if(layout->areas.begin(),layout->areas.end(),[&](const auto& a){return aa::sameName(a.name,action->targetArea);});
    if(destination==layout->areas.end()||!fits(destination->region,layout->clientWidth,layout->clientHeight))throw std::runtime_error("Selecione a área de destino desta regra na página HUDs.");
    previewTarget=destination->region;
    previewClock=action->followClock&&action->condition.condition!=aa::Condition::Absent;
    if(!testOverlay){testOverlay=std::make_unique<Overlay>();testOverlay->setCaptureVisible(showOverlayInCapture);testOverlay->initialize(instance);}
    testOverlay->setColor(action->triggers.empty()?action->condition.color:action->action.color);testOverlay->setEffect(action->effect);testOverlay->setShape(previewTarget.shape);
    if(!badge){badge=CreateWindowExW(WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE|WS_EX_TOPMOST|WS_EX_TOOLWINDOW,L"STATIC",L"TESTE DA AÇÃO",WS_POPUP|SS_CENTER|SS_CENTERIMAGE,0,0,1,1,nullptr,nullptr,instance,nullptr);
        if(!badge)throw std::runtime_error("Não foi possível mostrar o aviso de teste.");
        theme::styleControl(badge,theme::Role::Badge);SetLayeredWindowAttributes(badge,0,245,LWA_ALPHA);}
    SendMessageW(badge,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);previewUntil=GetTickCount64()+5000;SetForegroundWindow(target);updateHighlight();refreshStatus();
}
bool App::applyActionColor(const aa::Image& image){
    if(!rule())throw std::runtime_error("Escolha uma regra para capturar a cor da habilidade.");
    const auto color=aa::skillAccentColor(image);
    if(!color){error=L"Não encontrei uma cor nítida. A cor atual foi mantida.";return false;}
    auto changed=workspace;
    for(auto& profile:changed.sets)if(profile.id==workspace.activeSetId) {
        auto& selected=profile.rules.at(static_cast<std::size_t>(selectedRule));selected.action.color=*color;selected.condition.color=*color;
    }
    commit(std::move(changed));error=L"Cor capturada e salva. Capture novamente para atualizar.";return true;
}
void App::applyClockReference(const aa::Image& image){
    if(!selectedStatus())throw std::runtime_error("Escolha um status para guardar a referência do relógio.");
    aa::Recognizer validator;
    if(!validator.setClockReference(image))throw std::runtime_error("Selecione o ícone inteiro e iluminado, com 24 a 256 pixels.");
    auto changed=workspace;
    for(auto& status:changed.statuses)if(status.id==selectedStatusId)status.clockReferencePath=storeImage(status.id,image);
    commit(std::move(changed));error=L"Referência do relógio salva. Ative o acompanhamento nas regras desejadas.";
}
void App::sampleActionColor(const std::function<aa::Image(HWND,RECT)>& captureFrame){
    if(selecting)return;
    saveEditor();stop();
    if((!target||!IsWindow(target))&&!connect())return;
    if(IsIconic(target))ShowWindow(target,SW_RESTORE);
    const auto* action=rule();const auto* layout=hud();
    if(!action||!layout||!geometryMatches())throw std::runtime_error("Escolha uma regra e uma HUD compatível com esta tela para capturar a cor.");
    const auto destination=std::find_if(layout->areas.begin(),layout->areas.end(),[&](const auto& area){return aa::sameName(area.name,action->targetArea);});
    if(destination==layout->areas.end()||!fits(destination->region,layout->clientWidth,layout->clientHeight))
        throw std::runtime_error("Selecione a área de destino desta regra na página HUDs.");
    if(static_cast<std::int64_t>(destination->region.width)*destination->region.height>64000000)
        throw std::runtime_error("Selecione somente a área da habilidade para capturar a cor.");
    aa::Image image;
    {
        CapturePanelGuard restore(*this);
        image=captureFrame?captureFrame(target,rect(destination->region)):captureOnce(target,rect(destination->region));
        if(!geometryMatches()||GetForegroundWindow()!=target)
            throw std::runtime_error("A tela ou a janela ativa mudou durante a captura. A cor anterior foi mantida.");
    }
    (void)applyActionColor(image);makeUI();
}
void App::calibrateHealth(){
    if(selecting)return;
    saveEditor();stop();
    if((!target||!IsWindow(target))&&!connect())return;
    const auto* selected=area();const auto* layout=hud();
    if(!selected||!layout||!selected->region.valid()||selected->region.shape!=aa::RegionShape::Rectangle)
        throw std::runtime_error("Selecione uma área retangular que contenha a barra de vida antes de calibrar.");
    if(!geometryMatches())throw std::runtime_error("Escolha uma HUD compatível com a tela atual antes de calibrar a vida.");
    aa::Image image;
    {
        CapturePanelGuard restore(*this);
        image=captureOnce(target,rect(selected->region));
        if(!geometryMatches()||GetForegroundWindow()!=target)
            throw std::runtime_error("A tela ou a janela ativa mudou durante a captura. A calibração anterior foi mantida.");
    }
    const auto calibration=aa::calibrateHealth(image);
    if(!calibration.valid())throw std::runtime_error("Não encontrei uma barra vermelha cheia. Deixe a vida em 100% e selecione uma área que contenha somente a barra.");
    auto changed=workspace;const auto saved=std::find_if(changed.huds.begin(),changed.huds.end(),[&](const auto& value){return value.id==changed.activeHudId;});
    if(saved==changed.huds.end()||selectedArea<0||selectedArea>=static_cast<int>(saved->areas.size()))throw std::runtime_error("Área da HUD indisponível.");
    saved->areas[static_cast<std::size_t>(selectedArea)].healthCalibration=calibration;
    commit(std::move(changed));error=L"Vida cheia calibrada. A condição passará a ler o percentual desta área.";
}
void App::captureReadySkill(){
    if(selecting)return;
    saveEditor();stop();
    if((!target||!IsWindow(target))&&!connect())return;
    const auto* selected=area();const auto* layout=hud();
    if(!selected||!layout||!fits(selected->region,layout->clientWidth,layout->clientHeight)||
        selected->region.width<24||selected->region.height<24||selected->region.width>256||selected->region.height>256||!geometryMatches())
        throw std::runtime_error("Selecione uma habilidade de 24 a 256 pixels na HUD atual.");
    aa::Image image;
    {
        CapturePanelGuard restore(*this);
        image=captureOnce(target,rect(selected->region));
        if(!geometryMatches()||GetForegroundWindow()!=target)
            throw std::runtime_error("A tela mudou durante a captura. A imagem anterior foi mantida.");
    }
    auto changed=workspace;
    for(auto& layoutValue:changed.huds)if(layoutValue.id==changed.activeHudId)
        {auto& stored=layoutValue.areas.at(static_cast<std::size_t>(selectedArea));stored.readyReferencePath=storeImage(L"skill",image);stored.readyConfirmed=false;}
    commit(std::move(changed));error=L"Imagem salva. Confirme que a habilidade estava fora do cooldown ao capturar.";
}
std::optional<PickedImage> App::pick(aa::SelectionKind kind,const aa::Recognizer* reference,aa::RegionShape shape){
    if(selecting)return std::nullopt;stop();if((!target||!IsWindow(target))&&!connect())return std::nullopt;
    if(IsIconic(target))ShowWindow(target,SW_RESTORE);const auto before=screenOf(target);
    if(static_cast<std::int64_t>(before.width)*before.height>64000000)throw std::runtime_error("Use o jogo em um único monitor para selecionar.");
    CapturePanelGuard restore(*this);
        auto snapshot=captureOnce(target,{0,0,before.width,before.height});
        auto chosen=aa::selectRegion(window,target,snapshot,before.origin,kind,reference,shape);std::optional<PickedImage> result;
        if(chosen){const auto after=screenOf(target);
            if(after.width!=before.width||after.height!=before.height||after.dpi!=before.dpi||after.origin.x!=before.origin.x||after.origin.y!=before.origin.y)
                throw std::runtime_error("O tamanho, a escala ou a posição da janela mudou durante a seleção. Tente novamente.");
            if(!fits(*chosen,before.width,before.height))throw std::runtime_error("A região selecionada está fora da janela.");
            result=PickedImage{*chosen,aa::cropImage(snapshot,*chosen),before};}
        return result;
}
std::wstring App::storeImage(const std::wstring& statusId,const aa::Image& image){
    if(!image.valid()||image.width<24||image.height<24||image.width>256||image.height>256)throw std::runtime_error("Selecione um ícone de 24 a 256 pixels.");
    // Nome reservado pelo sistema: não derivar caminho de texto/ID editável do usuário.
    (void)statusId;const auto folder=directory/L"status-images";std::filesystem::create_directories(folder);wchar_t temporary[MAX_PATH]{};
    if(!GetTempFileNameW(folder.c_str(),L"img",0,temporary))throw std::runtime_error("Não foi possível reservar uma referência visual.");
    const std::filesystem::path temp=temporary;const auto file=folder/(temp.stem().wstring()+L".png");
    try{aa::saveImage(image,temp);if(!MoveFileExW(temp.c_str(),file.c_str(),MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Não foi possível salvar a referência visual.");}
    catch(...){DeleteFileW(temp.c_str());throw;}return file.wstring();
}
}
