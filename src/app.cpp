#include "app.h"
#include "calibration.h"
#include "../resources/resource.h"
#include <shlobj.h>
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <stdexcept>

namespace aaapp {
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
    if(!reader.status.builtinAssassin){
        const auto image=aa::loadImage(reader.status.referencePath);
        if(!image.valid()||image.width<24||image.height<24||image.width>256||image.height>256)
            throw std::runtime_error("A referência deve ter entre 24 e 256 pixels. Recapture o ícone na aba Status.");
        recognizer->setReference(image);
    }
    if(reader.needsStacks){
        for(const auto& sample:reader.status.stacks)
            if(!recognizer->setStackReference(sample.value,aa::loadImage(sample.path)))throw std::runtime_error("Uma amostra de contador é inválida. Recapture esse valor na biblioteca de status.");
    }else recognizer->clearStackReferences();
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
       !GetMonitorInfoW(MonitorFromWindow(target,MONITOR_DEFAULTTONEAREST),&monitor))throw std::runtime_error("Janela indisponível. Conecte ao Albion novamente.");
    result.width=client.right;result.height=client.bottom;result.dpi=GetDpiForWindow(target);result.device=monitor.szDevice;return result;
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
    workspace=aa::loadWorkspace(workspacePath,settingsPath,legacyStacks);if(!workspace.statuses.empty())selectedStatusId=workspace.statuses.front().id;
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
        for(const auto& h:workspace.huds)if(h.id==workspace.activeHudId)
            return h.clientWidth==screen.width&&h.clientHeight==screen.height&&(!h.monitorDpi||!screen.dpi||h.monitorDpi==screen.dpi)&&
                (h.monitorDevice.empty()||screen.device.empty()||h.monitorDevice==screen.device);
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
    recognizers.clear();current.clear();lit.assign(plan.actions.size(),false);
    {std::lock_guard lock(mutex);latest.clear();latestSource=0;latestImage={};latestError.clear();pending=false;}
}
void App::start(){
    if(selecting)return;saveEditor();stop();
    if((!target||!IsWindow(target))&&!connect())return;
    if(IsIconic(target))ShowWindow(target,SW_RESTORE);
    auto issues=aa::readinessIssues(workspace);
    if(!geometryMatches())issues.insert(issues.begin(),L"A HUD não corresponde à resolução/escala atual. Escolha outra ou crie uma HUD para esta tela.");
    if(!issues.empty()){
        error=L"Antes de iniciar:\n";for(std::size_t i=0;i<std::min<std::size_t>(issues.size(),4);++i)error+=L"• "+issues[i]+L"\n";
        page=0;makeUI();return;
    }
    plan=aa::makeMonitorPlan(workspace);
    if(static_cast<std::int64_t>(plan.captureArea.width)*plan.captureArea.height>64000000)throw std::runtime_error("As regiões abrangem uma área grande demais. Use o jogo em um único monitor.");
    for(const auto& read:plan.readers)recognizers.push_back(makeRecognizer(read));
    for(const auto& action:plan.actions){
        if(showOverlayInCapture){
            RECT destination=rect(action.target);InflateRect(&destination,12,12);
            for(const auto& reader:plan.readers){RECT overlap{},origin=rect(reader.area.region);if(IntersectRect(&overlap,&destination,&origin))throw std::runtime_error("No diagnóstico visual, a ação precisa ficar fora das regiões observadas.");}
        }
        auto overlay=std::make_unique<Overlay>();overlay->setCaptureVisible(showOverlayInCapture);overlay->initialize(instance);
        overlay->setColor(action.rule.condition.color);overlay->setGlow(action.rule.glow);overlays.push_back(std::move(overlay));
    }
    current.resize(plan.readers.size());lit.assign(plan.actions.size(),false);running=true;const auto runSource=++source;
    error.clear();page=0;makeUI();SetForegroundWindow(target);
    try{
        capture.start(target,rect(plan.captureArea),[this,runSource](aa::CaptureFrame frame){
            std::vector<aa::Observation> batch(plan.readers.size());std::wstring failure=widen(frame.error);
            for(std::size_t i=0;i<plan.readers.size();++i){
                batch[i].source=runSource;batch[i].capturedMs=frame.capturedMs;
                if(!frame.available)continue;
                try{auto roi=plan.readers[i].area.region;roi.x-=plan.captureArea.x;roi.y-=plan.captureArea.y;
                    batch[i].detection=recognizers[i]->recognize(aa::cropImage(frame.image,roi),plan.readers[i].area.iconSize);
                }catch(const std::exception& e){failure=widen(e.what());}
            }
            {std::lock_guard lock(mutex);latest=std::move(batch);latestSource=runSource;latestImage=std::move(frame.image);latestError=std::move(failure);}
            if(!pending.exchange(true)&&!PostMessageW(window,ResultMessage,0,0))pending=false;
        });
    }catch(...){stop();throw;}
}
void App::consume(){
    {std::lock_guard lock(mutex);pending=false;if(!running||latestSource!=source)return;current=latest;if(latestImage.valid())preview=std::move(latestImage);error=latestError;}
    updateHighlight();refreshStatus();if(page==0){RECT area{px(24),px(486),px(832),px(602)};InvalidateRect(window,&area,FALSE);}
}
void App::updateHighlight(){
    const auto now=static_cast<std::int64_t>(GetTickCount64());const bool geometry=geometryMatches();
    if(previewUntil){
        const bool active=static_cast<std::uint64_t>(now)<previewUntil&&geometry;
        if(testOverlay)testOverlay->update(target,rect(previewTarget),active);
        if(active&&GetForegroundWindow()==target){
            const auto screen=screenOf(target);auto caption=L"TESTE DA AÇÃO · "+std::to_wstring((previewUntil-now+999)/1000)+L" s · F8 encerra";
            SetWindowTextW(badge,caption.c_str());SetWindowPos(badge,HWND_TOPMOST,screen.origin.x+(screen.width-px(450))/2,screen.origin.y+px(40),px(450),px(42),SWP_NOACTIVATE|SWP_SHOWWINDOW);
        }else if(badge)ShowWindow(badge,SW_HIDE);
        if(!active){previewUntil=0;error=L"Teste da ação encerrado. Ele não inicia a leitura das regras.";ShowWindow(window,SW_RESTORE);SetForegroundWindow(window);}
        return;
    }
    if(running&&geometry&&GetForegroundWindow()==target)lit=aa::evaluateMonitor(plan,current,now,workspace.validityMs,source);
    else lit.assign(plan.actions.size(),false);
    for(std::size_t i=0;i<overlays.size();++i)overlays[i]->update(target,rect(plan.actions[i].target),lit[i]);
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
    if(!testOverlay){testOverlay=std::make_unique<Overlay>();testOverlay->setCaptureVisible(showOverlayInCapture);testOverlay->initialize(instance);}
    testOverlay->setColor(action->condition.color);testOverlay->setGlow(action->glow);
    if(!badge){badge=CreateWindowExW(WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE|WS_EX_TOPMOST|WS_EX_TOOLWINDOW,L"STATIC",L"TESTE DA AÇÃO",WS_POPUP|SS_CENTER|SS_CENTERIMAGE,0,0,1,1,nullptr,nullptr,instance,nullptr);
        if(!badge)throw std::runtime_error("Não foi possível mostrar o aviso de teste.");SetLayeredWindowAttributes(badge,0,245,LWA_ALPHA);}
    SendMessageW(badge,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);previewUntil=GetTickCount64()+5000;SetForegroundWindow(target);updateHighlight();refreshStatus();
}
std::optional<PickedImage> App::pick(aa::SelectionKind kind,const aa::Recognizer* reference){
    if(selecting)return std::nullopt;stop();if((!target||!IsWindow(target))&&!connect())return std::nullopt;
    if(IsIconic(target))ShowWindow(target,SW_RESTORE);const auto before=screenOf(target);
    if(static_cast<std::int64_t>(before.width)*before.height>64000000)throw std::runtime_error("Use o jogo em um único monitor para selecionar.");
    selecting=true;ShowWindow(window,SW_HIDE);SetForegroundWindow(target);
    try{
        std::mutex sampleMutex;std::condition_variable arrived;aa::Image snapshot;std::string failure;
        const auto requested=static_cast<std::int64_t>(GetTickCount64());aa::DesktopCapture single;
        single.start(target,{0,0,before.width,before.height},[&](aa::CaptureFrame frame){
            std::lock_guard lock(sampleMutex);if(frame.available&&frame.capturedMs>=requested&&!snapshot.valid()){snapshot=std::move(frame.image);arrived.notify_one();}else if(!frame.error.empty())failure=frame.error;
        });
        {std::unique_lock lock(sampleMutex);arrived.wait_for(lock,std::chrono::milliseconds(3000),[&]{return snapshot.valid();});}
        single.stop();if(!snapshot.valid())throw std::runtime_error("Não consegui capturar o jogo. Volte ao Albion e tente novamente. "+failure);
        auto chosen=aa::selectRegion(window,target,snapshot,before.origin,kind,reference);std::optional<PickedImage> result;
        if(chosen){const auto after=screenOf(target);
            if(after.width!=before.width||after.height!=before.height||after.dpi!=before.dpi||after.device!=before.device)throw std::runtime_error("A tela mudou durante a seleção. Tente novamente.");
            if(!fits(*chosen,before.width,before.height))throw std::runtime_error("A região selecionada está fora da janela.");
            result=PickedImage{*chosen,aa::cropImage(snapshot,*chosen),before};preview=result->image;}
        selecting=false;ShowWindow(window,SW_SHOW);SetForegroundWindow(window);return result;
    }catch(...){selecting=false;ShowWindow(window,SW_SHOW);SetForegroundWindow(window);throw;}
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
