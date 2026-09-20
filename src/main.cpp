#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>
#include <commdlg.h>
#include <objbase.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include "capture.h"
#include "recognition.h"
#include "model.h"
#include "overlay.h"
#include "profiles.h"
#include "calibration.h"
#include "selection.h"
#include "panel_layout.h"

namespace {
constexpr UINT ResultMessage=WM_APP+1;
enum Id { Connect=100, Buffs, Highlight, LoadReference, Save, Start, Stop, Sample,
    Profile, Name, ConditionBox, Stacks, IconSize, Enabled, Color, Validity,
    HudList,HudName,LoadHud,NewHud,SelectIcon,TestHighlight,Advanced,Back,Next,ResetReference,Tab0=200 };
std::wstring widen(const std::string& text) {
    if(text.empty()) return {};
    int n=MultiByteToWideChar(CP_UTF8,0,text.data(),static_cast<int>(text.size()),nullptr,0);
    std::wstring value(n,L' ');
    MultiByteToWideChar(CP_UTF8,0,text.data(),static_cast<int>(text.size()),value.data(),n);
    return value;
}
std::wstring text(HWND w) { int n=GetWindowTextLengthW(w); std::wstring s(n+1,L'\0'); GetWindowTextW(w,s.data(),n+1); s.resize(n); return s; }
RECT rect(aa::Region r) { return {r.x,r.y,r.x+r.width,r.y+r.height}; }
bool fits(aa::Region r,int w,int h) { return r.valid() && r.x<w && r.y<h && r.width<=w-r.x && r.height<=h-r.y; }
struct ComScope { HRESULT hr=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED); ~ComScope(){if(SUCCEEDED(hr))CoUninitialize();} };
struct Screen { int width=0,height=0;unsigned dpi=0;std::wstring device;POINT origin{}; };
Screen screenOf(HWND target){
    Screen s;RECT r{};MONITORINFOEXW info{};info.cbSize=sizeof(info);
    if(!IsWindow(target)||!GetClientRect(target,&r)||r.right<=0||r.bottom<=0||!ClientToScreen(target,&s.origin)||
       !GetMonitorInfoW(MonitorFromWindow(target,MONITOR_DEFAULTTONEAREST),&info))
        throw std::runtime_error("Janela indisponível. Conecte ao Albion novamente.");
    s.width=r.right;s.height=r.bottom;s.dpi=GetDpiForWindow(target);s.device=info.szDevice;return s;
}
struct App {
    HINSTANCE instance{}; HWND window{},target{},status{}; HFONT font{},titleFont{};
    std::filesystem::path directory,settingsPath,profilesDir;
    std::vector<aa::HudProfile> profiles;
    HWND screenLabel{},profileHint{},phrase{},badge{},regionLabels[3]{};
    int page=0,panel=-1;bool advanced=false,extra=false,selecting=false;
    std::uint64_t previewUntil=0;
    aa::Settings settings; std::wstring loadedHudName; std::unique_ptr<aa::Recognizer> recognizer;
    aa::DesktopCapture capture; Overlay overlay;
    std::mutex mutex; std::atomic<bool> pending=false;
    aa::Observation latest,current; aa::Image latestImage,preview;
    std::wstring latestError,error=L"Conecte ao Albion e siga as etapas de configuração.",hotkeyWarning;
    std::uint64_t source=0; bool running=false; bool lit=false;
    bool diagnostics=false,showOverlayInCapture=false; std::ofstream trace; std::string lastTrace; unsigned sampleCount=0;
    struct Control {HWND hwnd;int panel;bool extra;};
    double dpi=1; std::vector<Control> controls;
    int px(int v)const{return static_cast<int>(v*dpi);}
    HWND control(const wchar_t* cls,const wchar_t* label,DWORD style,int x,int y,int w,int h,int id=0) {
        HWND c=CreateWindowExW(0,cls,label,WS_CHILD|WS_VISIBLE|style,px(x),px(y),px(w),px(h),window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),instance,nullptr);
        SendMessageW(c,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE); controls.push_back({c,panel,extra}); return c;
    }
    HWND item(int id)const{return GetDlgItem(window,id);}
    void label(const wchar_t*s,int x,int y,int w,int h=22){control(L"STATIC",s,0,x,y,w,h);}
    void button(const wchar_t*s,int id,int x,int y,int w){control(L"BUTTON",s,WS_TABSTOP|BS_PUSHBUTTON,x,y,w,30,id);}
    void edit(const wchar_t*s,int id,int x,int y,int w){control(L"EDIT",s,WS_TABSTOP|WS_BORDER|ES_AUTOHSCROLL,x,y,w,26,id);}
    void makeUI(){
        for(auto c:controls)DestroyWindow(c.hwnd);controls.clear();
        if(font)DeleteObject(font);if(titleFont)DeleteObject(titleFont);
        const auto windowDpi=GetDpiForWindow(window);
        dpi=windowDpi/96.0;
        MONITORINFO monitor{};monitor.cbSize=sizeof(monitor);
        if(GetMonitorInfoW(MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),&monitor)){
            RECT frame{};
            const auto style=static_cast<DWORD>(GetWindowLongPtrW(window,GWL_STYLE));
            AdjustWindowRectExForDpi(&frame,style,FALSE,0,windowDpi);
            int availableWidth=monitor.rcWork.right-monitor.rcWork.left-(frame.right-frame.left);
            int availableHeight=monitor.rcWork.bottom-monitor.rcWork.top-(frame.bottom-frame.top);
            dpi=aa::panelScale(availableWidth,availableHeight,dpi);
            int width=px(860)+frame.right-frame.left,height=px(700)+frame.bottom-frame.top;
            RECT currentRect{};GetWindowRect(window,&currentRect);
            int x=std::clamp(static_cast<int>(currentRect.left),static_cast<int>(monitor.rcWork.left),static_cast<int>(monitor.rcWork.right)-width);
            int y=std::clamp(static_cast<int>(currentRect.top),static_cast<int>(monitor.rcWork.top),static_cast<int>(monitor.rcWork.bottom)-height);
            SetWindowPos(window,nullptr,x,y,width,height,SWP_NOZORDER|SWP_NOACTIVATE);
        }
        font=CreateFontW(-px(15),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
        titleFont=CreateFontW(-px(23),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
        panel=-1;extra=false;
        auto title=control(L"STATIC",L"Albion Assistant",0,24,18,808,34);SendMessageW(title,WM_SETFONT,reinterpret_cast<WPARAM>(titleFont),TRUE);
        label(L"Sua HUD, suas ações. Configure uma vez e salve para cada tela.",24,56,808);
        const wchar_t* tabs[]={L"1  Conectar",L"2  Selecionar",L"3  Criar ação",L"4  Conferir e usar"};
        for(int i=0;i<4;++i)button(tabs[i],Tab0+i,24+i*206,88,194);
        label(L"HUDs salvas",24,137,290);
        control(L"COMBOBOX",L"",WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,24,159,290,180,HudList);
        button(L"Carregar",LoadHud,326,157,90);button(L"Nova HUD",NewHud,428,157,118);
        label(L"Nome desta HUD",562,137,270);edit(settings.hudName.c_str(),HudName,562,159,270);
        panel=0;
        title=control(L"STATIC",L"Vamos encontrar o jogo",0,24,216,808,34);SendMessageW(title,WM_SETFONT,reinterpret_cast<WPARAM>(titleFont),TRUE);
        label(L"Abra o Albion em janela ou janela sem bordas.\nDepois, conecte e escolha uma HUD salva ou prepare uma nova.",24,259,808,50);
        button(L"Conectar ao Albion",Connect,24,322,250);
        screenLabel=control(L"STATIC",L"Nenhum jogo conectado.",0,24,378,808,48);
        profileHint=control(L"STATIC",L"Cada HUD guarda posições e tamanhos. Confira a seleção ao trocar de tela.",0,24,445,808,70);
        panel=1;
        title=control(L"STATIC",L"Escolha o que observar e destacar",0,24,216,808,34);SendMessageW(title,WM_SETFONT,reinterpret_cast<WPARAM>(titleFont),TRUE);
        label(L"Selecione sobre uma imagem congelada. Confirme a prévia para salvar; Esc cancela.",24,257,808,36);
        const wchar_t* names[]={L"Região onde seus buffs aparecem",L"Ícone de Espírito Assassino",L"Destino: Golpe Fantasma"};
        const wchar_t* actions[]={L"Selecionar região",L"Selecionar ícone",L"Selecionar destaque"};
        int ids[]={Buffs,SelectIcon,Highlight};
        for(int i=0;i<3;++i){label(names[i],24,303+i*74,570,25);regionLabels[i]=control(L"STATIC",L"",0,24,331+i*74,570,36);button(actions[i],ids[i],620,307+i*74,212);}
        panel=2;
        title=control(L"STATIC",L"Quando acontecer, faça isto",0,24,216,808,34);SendMessageW(title,WM_SETFONT,reinterpret_cast<WPARAM>(titleFont),TRUE);
        phrase=control(L"STATIC",L"",0,24,258,808,68);
        label(L"Quando o ícone monitorado…",24,330,310);
        auto cond=control(L"COMBOBOX",L"",WS_TABSTOP|CBS_DROPDOWNLIST,24,357,310,160,ConditionBox);
        for(auto t:{L"Tiver stacks iguais a",L"Estiver presente",L"Estiver ausente"})SendMessageW(cond,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(t));
        SendMessageW(cond,CB_SETCURSEL,static_cast<WPARAM>(settings.rule.condition),0);
        edit(std::to_wstring(settings.rule.stacks).c_str(),Stacks,350,357,86);
        label(L"Destacar a área com…",470,330,362);
        auto color=control(L"COMBOBOX",L"",WS_TABSTOP|CBS_DROPDOWNLIST,470,357,362,160,Color);
        for(auto t:{L"Borda dourada",L"Borda verde",L"Borda azul",L"Borda magenta"})SendMessageW(color,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(t));
        COLORREF colors[]={RGB(255,191,0),RGB(40,255,120),RGB(60,180,255),RGB(240,80,255)};
        int ci=0;for(int i=0;i<4;++i)if(colors[i]==settings.rule.color)ci=i;SendMessageW(color,CB_SETCURSEL,ci,0);
        auto active=control(L"BUTTON",L"Regra ativa",WS_TABSTOP|BS_AUTOCHECKBOX,24,407,260,28,Enabled);SendMessageW(active,BM_SETCHECK,settings.rule.enabled?BST_CHECKED:BST_UNCHECKED,0);
        label(L"Nome da regra",24,452,475);edit(settings.rule.name.c_str(),Name,24,480,475);
        label(L"Conjunto de equipamento",530,452,302);edit(settings.rule.profile.c_str(),Profile,530,480,302);
        panel=3;
        title=control(L"STATIC",L"Confira antes de usar",0,24,216,808,34);SendMessageW(title,WM_SETFONT,reinterpret_cast<WPARAM>(titleFont),TRUE);
        label(L"Teste a posição e a cor por 5 segundos. Depois, inicie a leitura para usar sua regra.",24,259,808,35);
        button(L"Testar destaque · 5 s",TestHighlight,24,302,208);button(L"Iniciar leitura · F9",Start,244,302,247);button(L"Parar",Stop,503,302,101);
        auto adv=control(L"BUTTON",L"Avançado",WS_TABSTOP|BS_AUTOCHECKBOX,640,305,180,26,Advanced);SendMessageW(adv,BM_SETCHECK,advanced?BST_CHECKED:BST_UNCHECKED,0);
        extra=true;
        label(L"Ícone (px)",24,350,82);edit(std::to_wstring(settings.iconSize).c_str(),IconSize,112,348,55);
        label(L"Validade (ms)",180,350,112);edit(std::to_wstring(settings.validityMs).c_str(),Validity,296,348,62);
        button(L"Outra referência…",LoadReference,374,345,173);button(L"Usar preset",ResetReference,557,345,122);button(L"Salvar amostra",Sample,690,345,142);
        extra=false;
        label(L"Última imagem capturada ou selecionada · confira abaixo se a leitura está atualizada",24,398,808,30);
        panel=-1;
        status=control(L"STATIC",L"",0,24,550,808,75);
        button(L"Voltar",Back,24,646,110);label(L"F8: painel · F9: iniciar / parar",152,652,292);
        button(L"Salvar HUD e ação",Save,454,646,200);button(L"Continuar",Next,680,646,152);
        refreshProfiles();refreshRegions();updatePhrase();showPage(page);refreshScreen();refreshStatus();
    }
    void rebuildUIWithDraft(){
        // Mudar de monitor não deve validar nem perder um campo ainda em edição.
        std::vector<std::pair<int,std::wstring>> draft;
        for(int id:{HudName,Name,Profile,Stacks,IconSize,Validity})draft.emplace_back(id,text(item(id)));
        const auto condition=SendMessageW(item(ConditionBox),CB_GETCURSEL,0,0);
        const auto color=SendMessageW(item(Color),CB_GETCURSEL,0,0);
        const auto enabled=SendMessageW(item(Enabled),BM_GETCHECK,0,0);
        const auto selectedHud=text(item(HudList));
        makeUI();
        for(const auto& [id,value]:draft)SetWindowTextW(item(id),value.c_str());
        SendMessageW(item(ConditionBox),CB_SETCURSEL,condition,0);
        SendMessageW(item(Color),CB_SETCURSEL,color,0);
        SendMessageW(item(Enabled),BM_SETCHECK,enabled,0);
        const auto index=SendMessageW(item(HudList),CB_FINDSTRINGEXACT,static_cast<WPARAM>(-1),reinterpret_cast<LPARAM>(selectedHud.c_str()));
        SendMessageW(item(HudList),CB_SETCURSEL,index,0);
        updatePhrase();
    }
    void showPage(int value){
        page=std::clamp(value,0,3);
        for(auto c:controls)ShowWindow(c.hwnd,(c.panel<0||c.panel==page)&&(!c.extra||advanced)?SW_SHOW:SW_HIDE);
        for(int i=0;i<4;++i)SendMessageW(item(Tab0+i),BM_SETSTATE,i==page,0);
        EnableWindow(item(Back),page>0);EnableWindow(item(Next),page<3);InvalidateRect(window,nullptr,TRUE);
    }
    void updatePhrase(){
        auto condition=SendMessageW(item(ConditionBox),CB_GETCURSEL,0,0);
        const wchar_t* colors[]={L"borda dourada",L"borda verde",L"borda azul",L"borda magenta"};
        int ci=std::clamp(static_cast<int>(SendMessageW(item(Color),CB_GETCURSEL,0,0)),0,3);
        std::wstring when=condition==0?L"tiver "+text(item(Stacks))+L" stacks":condition==1?L"estiver presente":L"estiver ausente";
        auto name=settings.referencePath.empty()?L"Espírito Assassino":L"o ícone cadastrado";
        auto line=std::wstring(L"Quando ")+name+L" "+when+L",\ndestacar a área selecionada de Golpe Fantasma com "+colors[ci]+L".";
        SetWindowTextW(phrase,line.c_str());EnableWindow(item(Stacks),condition==0);
    }
    int number(int id,int min,int max){
        try{auto s=text(item(id));std::size_t end=0;int n=std::stoi(s,&end);
            if(end!=s.size()||n<min||n>max)throw std::out_of_range("intervalo");return n;
        }catch(const std::exception&){throw std::runtime_error("Preencha o campo numérico com um valor entre "+std::to_string(min)+" e "+std::to_string(max)+".");}
    }
    void readEditor(){
        auto previous=settings;
        try{
        settings.hudName=text(item(HudName));
        if(settings.hudName.empty())throw std::runtime_error("Dê um nome para esta HUD.");
        settings.rule.profile=text(item(Profile));settings.rule.name=text(item(Name));
        if(settings.rule.profile.empty()||settings.rule.name.empty())throw std::runtime_error("Informe o perfil e o nome da regra.");
        settings.rule.condition=static_cast<aa::Condition>(SendMessageW(item(ConditionBox),CB_GETCURSEL,0,0));
        if(settings.rule.condition==aa::Condition::StacksEqual)
            settings.rule.stacks=static_cast<unsigned>(number(Stacks,2,3));
        settings.iconSize=number(IconSize,24,256);settings.validityMs=number(Validity,1,60000);
        settings.rule.enabled=SendMessageW(item(Enabled),BM_GETCHECK,0,0)==BST_CHECKED;
        COLORREF colors[]={RGB(255,191,0),RGB(40,255,120),RGB(60,180,255),RGB(240,80,255)};
        int ci=static_cast<int>(SendMessageW(item(Color),CB_GETCURSEL,0,0));settings.rule.color=colors[std::clamp(ci,0,3)];
        }catch(...){settings=std::move(previous);throw;}
    }
    void persist(){
        aa::saveHudProfile(profilesDir,settings,loadedHudName);loadedHudName=settings.hudName;
        aa::saveSettings(settingsPath.wstring(),settings);refreshProfiles();
    }
    void refreshProfiles(){
        profiles=aa::listHudProfiles(profilesDir);SendMessageW(item(HudList),CB_RESETCONTENT,0,0);int selected=-1;
        for(std::size_t i=0;i<profiles.size();++i){
            if(profiles[i].settings.hudName==settings.hudName)selected=static_cast<int>(i);
            SendMessageW(item(HudList),CB_ADDSTRING,0,reinterpret_cast<LPARAM>(profiles[i].settings.hudName.c_str()));
        }
        SendMessageW(item(HudList),CB_SETCURSEL,selected,0);EnableWindow(item(LoadHud),!profiles.empty());
    }
    void refreshScreen(){
        if(!target||!IsWindow(target)){SetWindowTextW(screenLabel,L"Nenhum jogo conectado. Clique em Conectar ao Albion.");return;}
        if(IsIconic(target)){SetWindowTextW(screenLabel,L"Jogo minimizado. Clique em Conectar para restaurá-lo.");return;}
        auto s=screenOf(target);
        auto line=L"Jogo conectado · "+std::to_wstring(s.width)+L" × "+std::to_wstring(s.height)+L" pixels\nConfira se a HUD corresponde a esta janela.";
        SetWindowTextW(screenLabel,line.c_str());
        std::wstring hint=L"Nenhuma HUD compatível encontrada. Use Nova HUD ou ajuste a seleção atual.";
        for(const auto& p:profiles)if(aa::matchesScreen(p.settings,s.width,s.height,s.dpi,s.device)){
            hint=L"Sugestão para esta tela: "+p.settings.hudName+L".\nSelecione na lista e clique em Carregar. Confira se o layout do jogo continua igual.";break;
        }
        SetWindowTextW(profileHint,hint.c_str());
    }
    void loadHud(){
        int index=static_cast<int>(SendMessageW(item(HudList),CB_GETCURSEL,0,0));
        if(index<0||index>=static_cast<int>(profiles.size())){error=L"Escolha uma HUD na lista para carregar.";refreshStatus();return;}
        auto path=profiles[index].path;stop();auto loaded=aa::loadSettings(path.wstring());
        aa::saveSettings(settingsPath.wstring(),loaded);settings=std::move(loaded);loadedHudName=settings.hudName;preview={};makeUI();
        error=L"HUD carregada. Confira as áreas antes de iniciar.";refreshStatus();
    }
    void newHud(){
        stop();auto saved=std::filesystem::exists(settingsPath)?aa::loadSettings(settingsPath.wstring()):aa::Settings{};
        auto previous=settings;auto previousLoaded=loadedHudName;settings={};settings.rule=saved.rule;settings.referencePath=saved.referencePath;
        profiles=aa::listHudProfiles(profilesDir);settings.hudName=aa::uniqueHudName(profiles,L"Nova HUD");loadedHudName.clear();preview={};
        try{persist();}catch(...){settings=std::move(previous);loadedHudName=std::move(previousLoaded);throw;}
        page=1;makeUI();error=L"Nova HUD criada. Dê um nome acima e selecione as três informações.";refreshStatus();
        SetFocus(item(HudName));SendMessageW(item(HudName),EM_SETSEL,0,-1);
    }
    bool geometryMatches(){
        if(!target||!IsWindow(target)||IsIconic(target))return false;
        try{auto screen=screenOf(target);return aa::matchesScreen(settings,screen.width,screen.height,screen.dpi,screen.device);}
        catch(...){return false;}
    }
    void refreshRegions(){
        auto area=[](aa::Region r){return r.valid()?L"Selecionada · "+std::to_wstring(r.width)+L" × "+std::to_wstring(r.height)+L" px":L"Pendente · marque dois cantos da região.";};
        SetWindowTextW(regionLabels[0],area(settings.buffs).c_str());
        auto icon=settings.iconCalibrated?L"Calibrado · "+std::to_wstring(settings.iconSize)+L" px · selecione de novo se a escala mudar.":L"Pendente · ative o buff no jogo e aponte seu ícone.";
        SetWindowTextW(regionLabels[1],icon.c_str());SetWindowTextW(regionLabels[2],area(settings.highlight).c_str());
    }
    void refreshStatus(){
        if(!status)return;
        auto now=static_cast<std::int64_t>(GetTickCount64());std::wstring s;
        if(previewUntil)s=L"TESTE DO DESTAQUE · A leitura está pausada.\nA borda de demonstração dura 5 segundos. F8 volta ao painel.";
        else if(!running)s=error;
        else if(!geometryMatches())s=L"É necessário ajustar a seleção. A janela ou escala mudou.\nPare e carregue outra HUD ou refaça a etapa 2.";
        else if(GetForegroundWindow()!=target)s=L"Jogo em segundo plano · destaque apagado.\nVolte ao Albion para retomar a leitura.";
        else if(!error.empty())s=L"Não consegui ler a tela agora. Pare e conecte novamente.\nDetalhe: "+error;
        else if(now-current.capturedMs>=settings.validityMs)s=L"Aguardando uma imagem recente · destaque apagado.";
        else {
            auto& d=current.detection;
            s=d.presence==aa::Presence::Unknown?L"Não consegui confirmar o ícone · destaque apagado.":
              d.presence==aa::Presence::Absent?L"Lendo normalmente · buff ausente.":L"Lendo normalmente · buff identificado.";
            if(d.presence==aa::Presence::Present)s+=L"\nStacks: "+(d.stacks?std::to_wstring(*d.stacks):L"ainda sem número legível")+(lit?L" · DESTAQUE ATIVO":L" · sem destaque");
            if(!settings.rule.enabled)s+=L"  A regra está desativada.";
        }
        if(!hotkeyWarning.empty())s+=L"\n"+hotkeyWarning;
        SetWindowTextW(status,s.c_str());
    }
    bool connect(){
        HWND found=nullptr;
        EnumWindows([](HWND w,LPARAM p)->BOOL{
            wchar_t title[256]{};GetWindowTextW(w,title,256);
            if(std::wstring(title)!=L"Albion Online Client")return TRUE;
            DWORD pid{};GetWindowThreadProcessId(w,&pid);
            HANDLE h=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
            if(!h)return TRUE;wchar_t path[1024]{};DWORD n=1024;BOOL ok=QueryFullProcessImageNameW(h,0,path,&n);CloseHandle(h);
            if(ok&&_wcsicmp(std::filesystem::path(path).filename().c_str(),L"Albion-Online.exe")==0){*reinterpret_cast<HWND*>(p)=w;return FALSE;}return TRUE;
        },reinterpret_cast<LPARAM>(&found));
        if(!found){error=L"Abra o Albion Online antes de conectar.";refreshStatus();return false;}
        stop();target=found;if(IsIconic(target))ShowWindow(target,SW_RESTORE);RECT r{};GetClientRect(target,&r);
        refreshProfiles();refreshScreen();
        error=geometryMatches()?L"Jogo conectado. Confira o layout da HUD e continue.":L"Jogo conectado. Carregue uma HUD compatível ou faça a seleção na etapa 2.";
        refreshRegions();refreshStatus();return true;
    }
    void stop(){capture.stop();running=false;previewUntil=0;if(badge)ShowWindow(badge,SW_HIDE);++source;current={};{std::lock_guard lock(mutex);latest={};latestImage={};latestError.clear();pending=false;}lit=false;if(target)overlay.update(target,{},false);error=L"Leitura parada.";if(status)refreshStatus();}
    void updateHighlight(){
        auto now=static_cast<std::int64_t>(GetTickCount64());
        bool geometry=geometryMatches();
        if(previewUntil){
            bool active=static_cast<std::uint64_t>(now)<previewUntil&&geometry;
            overlay.setColor(settings.rule.color);overlay.update(target,rect(settings.highlight),active);
            if(active&&GetForegroundWindow()==target){
                auto s=screenOf(target);int remaining=static_cast<int>((previewUntil-now+999)/1000);
                auto text=L"TESTE DO DESTAQUE · "+std::to_wstring(remaining)+L" s · F8 encerra";
                SetWindowTextW(badge,text.c_str());
                SetWindowPos(badge,HWND_TOPMOST,s.origin.x+(s.width-px(450))/2,s.origin.y+px(60),px(450),px(42),SWP_NOACTIVATE|SWP_SHOWWINDOW);
            }else if(badge)ShowWindow(badge,SW_HIDE);
            if(!active){previewUntil=0;error=L"Teste encerrado. Se a posição estiver correta, inicie a leitura.";ShowWindow(window,SW_RESTORE);SetForegroundWindow(window);}
            return;
        }
        lit=running&&geometry&&GetForegroundWindow()==target&&aa::evaluate(settings.rule,current,now,settings.validityMs,source);
        overlay.setColor(settings.rule.color);overlay.update(target,rect(settings.highlight),lit);
        if(diagnostics && running){
            std::string key=std::to_string(static_cast<int>(current.detection.presence))+","+(current.detection.stacks?std::to_string(*current.detection.stacks):"?")+","+(lit?"on":"off");
            if(key!=lastTrace){
                lastTrace=key;
                if(!trace.is_open()){trace.open(directory/L"diagnostics.csv",std::ios::app);trace<<"captured_ms,observed_ms,presence,stacks,highlight,score\n";}
                trace<<current.capturedMs<<','<<now<<','<<key<<','<<current.detection.confidence<<'\n';trace.flush();
                if(preview.valid() && current.detection.presence!=aa::Presence::Unknown && sampleCount<30){
                    ++sampleCount;auto folder=directory/L"samples";std::filesystem::create_directories(folder);
                    aa::saveImage(preview,folder/(std::to_wstring(now)+L"-stacks-"+(current.detection.stacks?std::to_wstring(*current.detection.stacks):L"unknown")+L".png"));
                }
            }
        }
    }
    void start(){
        if(selecting)return;readEditor();stop();if(!target&&!connect())return;
        if(IsIconic(target))ShowWindow(target,SW_RESTORE);
        RECT cr{};GetClientRect(target,&cr);
        if(!geometryMatches()||!settings.iconCalibrated||settings.buffs.width<settings.iconSize||settings.buffs.height<settings.iconSize||!fits(settings.buffs,cr.right,cr.bottom)||!fits(settings.highlight,cr.right,cr.bottom)){
            showPage(1);error=L"É necessário ajustar a seleção. Carregue uma HUD desta tela ou selecione as três informações da etapa 2.";refreshStatus();return;
        }
        RECT overlap{},buffArea=rect(settings.buffs),highlightArea=rect(settings.highlight);
        InflateRect(&highlightArea,6,6);
        if(showOverlayInCapture && IntersectRect(&overlap,&buffArea,&highlightArea))
            throw std::runtime_error("No diagnóstico visual, selecione o destaque fora da região dos buffs.");
        recognizer=std::make_unique<aa::Recognizer>(directory/L"assets");
        if(!settings.referencePath.empty())recognizer->setReference(aa::loadImage(settings.referencePath));
        persist();error=L"Aguardando a primeira captura.";running=true;auto runSource=++source;int size=settings.iconSize;
        showPage(3);
        SetForegroundWindow(target);
        try{capture.start(target,rect(settings.buffs),[this,runSource,size,announced=false](aa::CaptureFrame frame) mutable {
            if(diagnostics&&!announced){announced=true;std::ofstream report(directory/L"capture-first.txt");report<<"available="<<frame.available<<" timestamp="<<frame.capturedMs<<" size="<<frame.image.width<<'x'<<frame.image.height<<" error="<<frame.error<<" hwnd="<<window<<'\n';}
            aa::Observation obs;obs.source=runSource;obs.capturedMs=frame.capturedMs;
            std::wstring reason=widen(frame.error);
            if(frame.available){try{obs.detection=recognizer->recognize(frame.image,size);}catch(const std::exception&e){reason=widen(e.what());}}
            {std::lock_guard lock(mutex);latest=std::move(obs);latestImage=std::move(frame.image);latestError=std::move(reason);}
            if(!pending.exchange(true) && !PostMessageW(window,ResultMessage,0,0))pending=false;
        });}catch(...){running=false;throw;}
    }
    void consume(){
        {std::lock_guard lock(mutex);pending=false;if(!running||latest.source!=source)return;current=latest;if(latestImage.valid())preview=std::move(latestImage);error=latestError;}
        updateHighlight();
    }
    void choose(int kind);
    void testHighlight(){
        if(selecting)return;readEditor();stop();if(!target&&!connect())return;
        if(IsIconic(target))ShowWindow(target,SW_RESTORE);auto s=screenOf(target);
        if(!geometryMatches()||!fits(settings.highlight,s.width,s.height)){
            showPage(1);error=L"Selecione o destino do destaque nesta tela antes de testar.";refreshStatus();return;
        }
        persist();
        if(!badge){
            badge=CreateWindowExW(WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE|WS_EX_TOPMOST|WS_EX_TOOLWINDOW,
                L"STATIC",L"TESTE DO DESTAQUE",WS_POPUP|SS_CENTER|SS_CENTERIMAGE,0,0,1,1,nullptr,nullptr,instance,nullptr);
            if(!badge)throw std::runtime_error("Não foi possível mostrar o aviso do modo de teste.");
            SetLayeredWindowAttributes(badge,0,245,LWA_ALPHA);
        }
        SendMessageW(badge,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);
        previewUntil=GetTickCount64()+5000;SetForegroundWindow(target);updateHighlight();refreshStatus();
    }
    void selectReference(){
        readEditor();stop();wchar_t path[32768]{};OPENFILENAMEW ofn{sizeof(ofn)};ofn.hwndOwner=window;ofn.lpstrFilter=L"Imagem de referência (PNG/BMP)\0*.png;*.bmp\0";ofn.lpstrFile=path;ofn.nMaxFile=32768;ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
        if(GetOpenFileNameW(&ofn)){aa::loadImage(path);settings.referencePath=path;settings.iconCalibrated=false;persist();updatePhrase();refreshRegions();error=L"Referência cadastrada. Selecione o ícone na etapa 2 para calibrar o tamanho. Stacks disponíveis: 2 e 3.";refreshStatus();}
    }
    void saveSample(){
        if(!preview.valid())throw std::runtime_error("Ainda não existe imagem capturada. Inicie com o jogo em primeiro plano.");
        auto folder=directory/L"samples";std::filesystem::create_directories(folder);auto file=folder/(std::to_wstring(GetTickCount64())+L".png");aa::saveImage(preview,file);error=L"Amostra salva em "+file.wstring();refreshStatus();
    }
    void paint(){
        PAINTSTRUCT ps{};HDC dc=BeginPaint(window,&ps);if(page!=3){EndPaint(window,&ps);return;}RECT r{px(24),px(432),px(832),px(530)};
        HBRUSH brush=CreateSolidBrush(RGB(30,34,41));FillRect(dc,&r,brush);DeleteObject(brush);
        if(preview.valid()){
            double factor=std::min(static_cast<double>(r.right-r.left)/preview.width,static_cast<double>(r.bottom-r.top)/preview.height);
            int w=static_cast<int>(preview.width*factor),h=static_cast<int>(preview.height*factor);
            BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=preview.width;bi.bmiHeader.biHeight=-preview.height;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
            SetStretchBltMode(dc,COLORONCOLOR);StretchDIBits(dc,r.left,r.top,w,h,0,0,preview.width,preview.height,preview.bgra.data(),&bi,DIB_RGB_COLORS,SRCCOPY);
        }EndPaint(window,&ps);
    }
};
void App::choose(int kind){
    if(selecting)return;readEditor();stop();if(!target&&!connect())return;
    if(IsIconic(target))ShowWindow(target,SW_RESTORE);
    auto before=screenOf(target);
    if(kind==3&&(!geometryMatches()||!fits(settings.buffs,before.width,before.height))){
        error=L"Selecione primeiro a região dos buffs desta tela; depois aponte o ícone dentro dela.";refreshStatus();return;
    }
    if(static_cast<std::int64_t>(before.width)*before.height>64000000)throw std::runtime_error("A janela é grande demais para a seleção. Use o jogo em um único monitor.");
    selecting=true;ShowWindow(window,SW_HIDE);SetForegroundWindow(target);
    try{
        std::mutex sampleMutex;std::condition_variable arrived;
        aa::Image snapshot;std::string failure;const auto requested=static_cast<std::int64_t>(GetTickCount64());
        aa::DesktopCapture single;
        single.start(target,{0,0,before.width,before.height},[&](aa::CaptureFrame frame){
            std::lock_guard lock(sampleMutex);
            if(frame.available&&frame.capturedMs>=requested&&!snapshot.valid()){snapshot=std::move(frame.image);arrived.notify_one();}
            else if(!frame.error.empty())failure=frame.error;
        });
        {std::unique_lock lock(sampleMutex);arrived.wait_for(lock,std::chrono::milliseconds(3000),[&]{return snapshot.valid();});}
        single.stop();
        if(!snapshot.valid())throw std::runtime_error("Não consegui congelar a imagem do jogo. Volte ao Albion e tente novamente. "+failure);
        aa::Recognizer reference(directory/L"assets");
        if(!settings.referencePath.empty())reference.setReference(aa::loadImage(settings.referencePath));
        auto mode=kind==1?aa::SelectionKind::Buffs:kind==2?aa::SelectionKind::Highlight:aa::SelectionKind::Icon;
        auto chosen=aa::selectRegion(window,target,snapshot,before.origin,mode,&reference);
        if(chosen){
            auto after=screenOf(target);
            if(after.width!=before.width||after.height!=before.height||after.dpi!=before.dpi||after.device!=before.device)
                throw std::runtime_error("A tela mudou durante a seleção. Refaça a seleção na tela atual.");
            if(kind==3&&(!fits(*chosen,before.width,before.height)||chosen->x<settings.buffs.x||chosen->y<settings.buffs.y||
               chosen->x+chosen->width>settings.buffs.x+settings.buffs.width||chosen->y+chosen->height>settings.buffs.y+settings.buffs.height))
                throw std::runtime_error("Escolha o ícone dentro da região dos buffs. Aumente essa região se necessário.");
            auto previous=settings;auto previousLoaded=loadedHudName;
            const bool calibratedScreen=settings.clientWidth>0&&settings.clientHeight>0;
            if(calibratedScreen&&!aa::matchesScreen(settings,before.width,before.height,before.dpi,before.device)){
                auto name=settings.hudName+L" - "+std::to_wstring(before.width)+L"x"+std::to_wstring(before.height);
                settings.hudName=aa::uniqueHudName(aa::listHudProfiles(profilesDir),name);loadedHudName.clear();
                settings.buffs={};settings.highlight={};settings.iconCalibrated=false;
            }
            settings.clientWidth=before.width;settings.clientHeight=before.height;settings.monitorDpi=before.dpi;settings.monitorDevice=before.device;
            if(kind==1)settings.buffs=*chosen;
            else if(kind==2)settings.highlight=*chosen;
            else{settings.iconSize=chosen->width;settings.iconCalibrated=true;SetWindowTextW(item(IconSize),std::to_wstring(settings.iconSize).c_str());}
            try{persist();}catch(...){settings=std::move(previous);loadedHudName=std::move(previousLoaded);throw;}
            preview=aa::cropImage(snapshot,*chosen);
            error=kind==3?L"Ícone confirmado. O tamanho foi obtido da sua seleção, sem digitar pixels.":L"Área confirmada e salva nesta HUD.";
        }else error=L"Seleção cancelada. A calibração anterior foi mantida.";
    }catch(...){selecting=false;ShowWindow(window,SW_SHOW);SetForegroundWindow(window);throw;}
    selecting=false;ShowWindow(window,SW_SHOW);SetForegroundWindow(window);
    SetWindowTextW(item(HudName),settings.hudName.c_str());refreshRegions();refreshScreen();refreshStatus();
}
LRESULT CALLBACK appProc(HWND w,UINT m,WPARAM wp,LPARAM lp){
    auto*a=reinterpret_cast<App*>(GetWindowLongPtrW(w,GWLP_USERDATA));
    if(m==WM_NCCREATE){a=static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);a->window=w;SetWindowLongPtrW(w,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(a));}
    if(!a)return DefWindowProcW(w,m,wp,lp);
    try{switch(m){
    case WM_CREATE:{a->makeUI();SetTimer(w,1,50,nullptr);
        const bool panelHotkey=RegisterHotKey(w,1,MOD_NOREPEAT,VK_F8)!=FALSE;
        const bool readingHotkey=RegisterHotKey(w,2,MOD_NOREPEAT,VK_F9)!=FALSE;
        if(!panelHotkey||!readingHotkey){a->hotkeyWarning=L"Atalho ocupado. Feche outra instância e reabra este painel, ou use os botões.";a->refreshStatus();}
        return 0;}
    case WM_COMMAND:
        if((HIWORD(wp)==CBN_SELCHANGE&&(LOWORD(wp)==ConditionBox||LOWORD(wp)==Color))||
           (HIWORD(wp)==EN_CHANGE&&LOWORD(wp)==Stacks)){a->updatePhrase();return 0;}
        if(HIWORD(wp)==BN_CLICKED){
        if(LOWORD(wp)>=Tab0&&LOWORD(wp)<Tab0+4){a->readEditor();a->stop();a->showPage(LOWORD(wp)-Tab0);return 0;}
        switch(LOWORD(wp)){
        case Connect:a->connect();break;case Buffs:a->choose(1);break;case Highlight:a->choose(2);break;case SelectIcon:a->choose(3);break;
        case LoadReference:a->selectReference();break;case Start:a->start();break;case Stop:a->stop();break;
        case ResetReference:a->readEditor();a->stop();a->settings.referencePath.clear();a->settings.iconCalibrated=false;a->persist();a->updatePhrase();a->refreshRegions();a->error=L"Preset restaurado. Selecione o ícone na etapa 2.";a->refreshStatus();break;
        case Save:a->readEditor();a->stop();a->persist();a->refreshScreen();a->error=L"HUD e ação salvas. A configuração estará disponível na lista de HUDs.";a->refreshStatus();break;
        case Sample:a->saveSample();break;
        case LoadHud:a->loadHud();break;case NewHud:a->newHud();break;case TestHighlight:a->testHighlight();break;
        case Advanced:a->advanced=SendMessageW(a->item(Advanced),BM_GETCHECK,0,0)==BST_CHECKED;a->showPage(a->page);break;
        case Back:case Next:a->readEditor();a->stop();a->showPage(a->page+(LOWORD(wp)==Next?1:-1));break;
    }}return 0;
    case WM_HOTKEY:if(a->selecting)return 0;if(wp==1){if(a->previewUntil)a->stop();ShowWindow(w,SW_RESTORE);SetForegroundWindow(w);}else if(wp==2){if(a->running||a->previewUntil)a->stop();else a->start();}return 0;
    case ResultMessage:a->consume();return 0;
    case WM_TIMER:if(a->selecting)return 0;a->updateHighlight();a->refreshStatus();if(a->page==3){RECT r{a->px(24),a->px(432),a->px(832),a->px(530)};InvalidateRect(w,&r,FALSE);}return 0;
    case WM_PAINT:a->paint();return 0;
    case WM_DPICHANGED:{a->stop();auto*r=reinterpret_cast<RECT*>(lp);SetWindowPos(w,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER);a->rebuildUIWithDraft();return 0;}
    case WM_CLOSE:DestroyWindow(w);return 0;
    case WM_DESTROY:a->stop();if(a->badge)DestroyWindow(a->badge);KillTimer(w,1);UnregisterHotKey(w,1);UnregisterHotKey(w,2);PostQuitMessage(0);return 0;
    }}catch(const std::exception&e){a->stop();a->error=widen(e.what());a->refreshStatus();MessageBoxW(w,a->error.c_str(),L"Albion Assistant",MB_OK|MB_ICONWARNING);}
    return DefWindowProcW(w,m,wp,lp);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int show){
    try{SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);ComScope com;App app;app.instance=instance;
        wchar_t exe[32768]{};GetModuleFileNameW(nullptr,exe,32768);app.directory=std::filesystem::path(exe).parent_path();app.settingsPath=app.directory/L"settings.ini";
        int argc=0;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);for(int i=1;i<argc;i++){if(std::wstring(argv[i])==L"--settings"&&i+1<argc)app.settingsPath=argv[++i];else if(std::wstring(argv[i])==L"--diagnostics")app.diagnostics=true;else if(std::wstring(argv[i])==L"--show-overlay-in-capture")app.showOverlayInCapture=true;}LocalFree(argv);
        app.showOverlayInCapture=app.showOverlayInCapture&&app.diagnostics;
        app.settingsPath=std::filesystem::absolute(app.settingsPath);
        app.profilesDir=app.settingsPath.parent_path()/L"hud-profiles";app.settings=aa::loadSettings(app.settingsPath.wstring());app.overlay.initialize(instance);app.overlay.setCaptureVisible(app.showOverlayInCapture);
        app.profiles=aa::listHudProfiles(app.profilesDir);
        if(std::filesystem::exists(app.settingsPath)){
            const bool namedActive=aa::hasExplicitHudName(app.settingsPath);
            auto loaded=std::find_if(app.profiles.begin(),app.profiles.end(),[&](const auto& profile){return profile.settings.hudName==app.settings.hudName;});
            if(namedActive&&loaded!=app.profiles.end())app.loadedHudName=app.settings.hudName;
            else{
                app.settings.hudName=aa::uniqueHudName(app.profiles,app.settings.hudName);
                aa::saveHudProfile(app.profilesDir,app.settings,L"");aa::saveSettings(app.settingsPath.wstring(),app.settings);app.loadedHudName=app.settings.hudName;
            }
        }
        WNDCLASSW wc{};wc.hInstance=instance;wc.lpfnWndProc=appProc;wc.lpszClassName=L"AlbionAssistantPoc";wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_BTNFACE+1);RegisterClassW(&wc);
        auto dpi=GetDpiForSystem();RECT bounds{0,0,MulDiv(860,dpi,96),MulDiv(700,dpi,96)};AdjustWindowRectExForDpi(&bounds,WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,FALSE,0,dpi);
        if(!CreateWindowExW(0,L"AlbionAssistantPoc",L"Albion Assistant — Configuração guiada",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,CW_USEDEFAULT,CW_USEDEFAULT,bounds.right-bounds.left,bounds.bottom-bounds.top,nullptr,nullptr,instance,&app))throw std::runtime_error("Falha ao abrir aplicativo.");
        ShowWindow(app.window,show);MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0){if(!IsDialogMessageW(app.window,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}}
        if(app.font)DeleteObject(app.font);if(app.titleFont)DeleteObject(app.titleFont);return 0;
    }catch(const std::exception&e){MessageBoxW(nullptr,widen(e.what()).c_str(),L"Albion Assistant — erro",MB_OK|MB_ICONERROR);return 1;}
}
