#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>
#include <commdlg.h>
#include <objbase.h>
#include <algorithm>
#include <atomic>
#include <chrono>
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

namespace {
constexpr UINT ResultMessage=WM_APP+1;
enum Id { Connect=100, Buffs, Highlight, LoadReference, Save, Start, Stop, Sample,
    Profile, Name, ConditionBox, Stacks, IconSize, Enabled, Color, Validity };
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
struct App {
    HINSTANCE instance{}; HWND window{},target{},selector{},status{},regions{}; HFONT font{},titleFont{};
    std::filesystem::path directory,settingsPath;
    aa::Settings settings; std::unique_ptr<aa::Recognizer> recognizer;
    aa::DesktopCapture capture; Overlay overlay;
    std::mutex mutex; std::atomic<bool> pending=false;
    aa::Observation latest,current; aa::Image latestImage,preview;
    std::wstring latestError,error=L"Conecte ao Albion e selecione as duas áreas.";
    std::uint64_t source=0; bool running=false; bool lit=false;
    bool diagnostics=false,showOverlayInCapture=false; std::ofstream trace; std::string lastTrace; unsigned sampleCount=0;
    int selection=0; bool firstPoint=false; POINT first{},cursor{}; RECT selectionClient{};
    double dpi=1; std::vector<HWND> controls;
    int px(int v)const{return static_cast<int>(v*dpi);}
    HWND control(const wchar_t* cls,const wchar_t* label,DWORD style,int x,int y,int w,int h,int id=0) {
        HWND c=CreateWindowExW(0,cls,label,WS_CHILD|WS_VISIBLE|style,px(x),px(y),px(w),px(h),window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),instance,nullptr);
        SendMessageW(c,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE); controls.push_back(c); return c;
    }
    HWND item(int id)const{return GetDlgItem(window,id);}
    void label(const wchar_t*s,int x,int y,int w,int h=22){control(L"STATIC",s,0,x,y,w,h);}
    void button(const wchar_t*s,int id,int x,int y,int w){control(L"BUTTON",s,WS_TABSTOP|BS_PUSHBUTTON,x,y,w,30,id);}
    void edit(const wchar_t*s,int id,int x,int y,int w){control(L"EDIT",s,WS_TABSTOP|WS_BORDER|ES_AUTOHSCROLL,x,y,w,26,id);}
    void makeUI(){
        for(auto c:controls)DestroyWindow(c);controls.clear();
        if(font)DeleteObject(font);if(titleFont)DeleteObject(titleFont);
        dpi=GetDpiForWindow(window)/96.0;
        font=CreateFontW(-px(15),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
        titleFont=CreateFontW(-px(24),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
        HWND title=control(L"STATIC",L"Albion Assistant  ·  POC",0,22,16,700,35);SendMessageW(title,WM_SETFONT,reinterpret_cast<WPARAM>(titleFont),TRUE);
        label(L"Espírito Assassino → Golpe Fantasma  |  Captura externa ao vivo",22,55,740);
        button(L"1. Conectar ao Albion",Connect,22,91,194);
        button(L"2. Área dos buffs",Buffs,228,91,180);
        button(L"3. Área do destaque",Highlight,420,91,187);
        button(L"Ícone de referência…",LoadReference,619,91,181);
        regions=control(L"STATIC",L"",0,22,131,778,42);
        label(L"Perfil",22,185,125);edit(settings.rule.profile.c_str(),Profile,22,209,235);
        label(L"Nome da regra",276,185,300);edit(settings.rule.name.c_str(),Name,276,209,524);
        label(L"Quando o ícone monitorado…",22,250,270);
        HWND cond=control(L"COMBOBOX",L"",WS_TABSTOP|CBS_DROPDOWNLIST,22,275,285,140,ConditionBox);
        for(auto s:{L"Estiver com stacks iguais a",L"Estiver presente",L"Estiver ausente"})SendMessageW(cond,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(s));
        SendMessageW(cond,CB_SETCURSEL,static_cast<WPARAM>(settings.rule.condition),0);
        edit(std::to_wstring(settings.rule.stacks).c_str(),Stacks,320,275,50);
        label(L"Ação: destacar área",393,250,210);
        HWND color=control(L"COMBOBOX",L"",WS_TABSTOP|CBS_DROPDOWNLIST,393,275,190,135,Color);
        for(auto s:{L"Borda dourada",L"Borda verde",L"Borda azul",L"Borda magenta"})SendMessageW(color,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(s));
        COLORREF colors[]={RGB(255,191,0),RGB(40,255,120),RGB(60,180,255),RGB(240,80,255)};
        int ci=0;for(int i=0;i<4;i++)if(colors[i]==settings.rule.color)ci=i;SendMessageW(color,CB_SETCURSEL,ci,0);
        HWND enabled=control(L"BUTTON",L"Regra ativa",WS_TABSTOP|BS_AUTOCHECKBOX,620,275,178,26,Enabled);SendMessageW(enabled,BM_SETCHECK,settings.rule.enabled?BST_CHECKED:BST_UNCHECKED,0);
        label(L"Diâmetro do ícone (px)",22,313,190);edit(std::to_wstring(settings.iconSize).c_str(),IconSize,208,311,65);
        label(L"Validade da leitura (ms)",302,313,190);edit(std::to_wstring(settings.validityMs).c_str(),Validity,498,311,75);
        button(L"Salvar configuração",Save,619,310,181);
        button(L"Iniciar leitura  ·  F9",Start,22,357,205);button(L"Parar",Stop,239,357,90);button(L"Salvar amostra",Sample,341,357,158);
        label(L"F8: voltar ao painel  ·  Esc: cancelar seleção",516,363,289,35);
        status=control(L"STATIC",L"",0,22,405,778,65);
        label(L"Última captura da região · confira o estado acima para saber se está atualizada",22,480,778);
        refreshRegions();refreshStatus();InvalidateRect(window,nullptr,TRUE);
    }
    int number(int id,int min,int max){auto s=text(item(id));std::size_t end=0;int n=std::stoi(s,&end);if(end!=s.size()||n<min||n>max)throw std::runtime_error("Valor numérico fora do intervalo permitido.");return n;}
    void readEditor(){
        settings.rule.profile=text(item(Profile));settings.rule.name=text(item(Name));
        if(settings.rule.profile.empty()||settings.rule.name.empty())throw std::runtime_error("Informe o perfil e o nome da regra.");
        settings.rule.condition=static_cast<aa::Condition>(SendMessageW(item(ConditionBox),CB_GETCURSEL,0,0));
        settings.rule.stacks=static_cast<unsigned>(number(Stacks,0,4));
        if(settings.rule.condition==aa::Condition::StacksEqual && settings.rule.stacks!=2 && settings.rule.stacks!=3)
            throw std::runtime_error("Esta POC reconhece stacks 2 ou 3. Use um desses valores para a condição de stacks.");
        settings.iconSize=number(IconSize,24,256);settings.validityMs=number(Validity,1,60000);
        settings.rule.enabled=SendMessageW(item(Enabled),BM_GETCHECK,0,0)==BST_CHECKED;
        COLORREF colors[]={RGB(255,191,0),RGB(40,255,120),RGB(60,180,255),RGB(240,80,255)};
        int ci=static_cast<int>(SendMessageW(item(Color),CB_GETCURSEL,0,0));settings.rule.color=colors[std::clamp(ci,0,3)];
    }
    void persist(){aa::saveSettings(settingsPath.wstring(),settings);}
    void refreshRegions(){
        auto regionText=[](aa::Region r){return r.valid()?std::to_wstring(r.x)+L", "+std::to_wstring(r.y)+L" · "+std::to_wstring(r.width)+L" × "+std::to_wstring(r.height)+L" px":L"não selecionada";};
        auto s=L"Buffs: "+regionText(settings.buffs)+L"     |     Destaque: "+regionText(settings.highlight)+L"\nSeleção por dois cliques: canto superior esquerdo e canto inferior direito.";
        SetWindowTextW(regions,s.c_str());
    }
    void refreshStatus(){
        auto now=static_cast<std::int64_t>(GetTickCount64());std::wstring s;
        if(!running)s=L"PARADO · "+error;
        else if(!error.empty())s=L"LEITURA INDISPONÍVEL · "+error;
        else if(now-current.capturedMs>=settings.validityMs)s=L"LEITURA EXPIRADA · destaque apagado";
        else {
            auto&d=current.detection;
            s=d.presence==aa::Presence::Absent?L"BUFF AUSENTE":d.presence==aa::Presence::Unknown?L"LEITURA INCERTA":L"BUFF IDENTIFICADO";
            s+=L" · Stacks: "+(d.stacks?std::to_wstring(*d.stacks):L"desconhecidos");
            s+=lit?L" · DESTAQUE ATIVO":L" · sem destaque";
            s+=L"\n"+widen(d.detail)+L" · idade da captura: "+std::to_wstring(std::max<std::int64_t>(0,now-current.capturedMs))+L" ms";
        }
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
        if(settings.clientWidth&& (settings.clientWidth!=r.right||settings.clientHeight!=r.bottom)){
            settings.buffs={};settings.highlight={};error=L"Resolução mudou. Selecione novamente as áreas da HUD.";
        }else error=L"Albion conectado. Selecione as áreas ou inicie a leitura.";
        settings.clientWidth=r.right;settings.clientHeight=r.bottom;refreshRegions();refreshStatus();return true;
    }
    void stop(){capture.stop();running=false;++source;current={};{std::lock_guard lock(mutex);latest={};latestImage={};latestError.clear();pending=false;}lit=false;if(target)overlay.update(target,{},false);error=L"Leitura parada.";if(status)refreshStatus();}
    void updateHighlight(){
        auto now=static_cast<std::int64_t>(GetTickCount64());RECT client{};
        bool geometry=target&&GetClientRect(target,&client)&&client.right==settings.clientWidth&&client.bottom==settings.clientHeight;
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
        if(selector)return;readEditor();stop();if(!target&&!connect())return;
        if(IsIconic(target))ShowWindow(target,SW_RESTORE);
        RECT cr{};GetClientRect(target,&cr);
        if(cr.right!=settings.clientWidth||cr.bottom!=settings.clientHeight||!fits(settings.buffs,cr.right,cr.bottom)||!fits(settings.highlight,cr.right,cr.bottom))
            throw std::runtime_error("Selecione as áreas dos buffs e do destaque na resolução atual.");
        RECT overlap{},buffArea=rect(settings.buffs),highlightArea=rect(settings.highlight);
        InflateRect(&highlightArea,6,6);
        if(showOverlayInCapture && IntersectRect(&overlap,&buffArea,&highlightArea))
            throw std::runtime_error("No diagnóstico visual, selecione o destaque fora da região dos buffs.");
        recognizer=std::make_unique<aa::Recognizer>(directory/L"assets");
        if(!settings.referencePath.empty())recognizer->setReference(aa::loadImage(settings.referencePath));
        persist();error=L"Aguardando a primeira captura.";running=true;auto runSource=++source;int size=settings.iconSize;
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
    void finishSelection(POINT a,POINT b){
        aa::Region r{std::min(a.x,b.x),std::min(a.y,b.y),std::abs(b.x-a.x),std::abs(b.y-a.y)};
        if(r.width<8||r.height<8){error=L"Área muito pequena. Selecione novamente.";}
        else{if(selection==1)settings.buffs=r;else settings.highlight=r;persist();error=L"Área salva. Confira o diâmetro do ícone e inicie a leitura.";}
        DestroyWindow(selector);selector=nullptr;ShowWindow(window,SW_SHOW);SetForegroundWindow(window);refreshRegions();refreshStatus();
    }
    void selectReference(){
        stop();wchar_t path[32768]{};OPENFILENAMEW ofn{sizeof(ofn)};ofn.hwndOwner=window;ofn.lpstrFilter=L"Imagem de referência (PNG/BMP)\0*.png;*.bmp\0";ofn.lpstrFile=path;ofn.nMaxFile=32768;ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
        if(GetOpenFileNameW(&ofn)){aa::loadImage(path);settings.referencePath=path;persist();error=L"Referência cadastrada. Leitura de stacks disponível para dígitos 2 e 3.";refreshStatus();}
    }
    void saveSample(){
        if(!preview.valid())throw std::runtime_error("Ainda não existe imagem capturada. Inicie com o jogo em primeiro plano.");
        auto folder=directory/L"samples";std::filesystem::create_directories(folder);auto file=folder/(std::to_wstring(GetTickCount64())+L".png");aa::saveImage(preview,file);error=L"Amostra salva em "+file.wstring();refreshStatus();
    }
    void paint(){
        PAINTSTRUCT ps{};HDC dc=BeginPaint(window,&ps);RECT r{px(22),px(511),px(800),px(615)};
        HBRUSH brush=CreateSolidBrush(RGB(30,34,41));FillRect(dc,&r,brush);DeleteObject(brush);
        if(preview.valid()){
            double factor=std::min(static_cast<double>(r.right-r.left)/preview.width,static_cast<double>(r.bottom-r.top)/preview.height);
            int w=static_cast<int>(preview.width*factor),h=static_cast<int>(preview.height*factor);
            BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=preview.width;bi.bmiHeader.biHeight=-preview.height;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
            SetStretchBltMode(dc,COLORONCOLOR);StretchDIBits(dc,r.left,r.top,w,h,0,0,preview.width,preview.height,preview.bgra.data(),&bi,DIB_RGB_COLORS,SRCCOPY);
        }EndPaint(window,&ps);
    }
};
LRESULT CALLBACK selectorProc(HWND w,UINT m,WPARAM wp,LPARAM lp){
    auto*a=reinterpret_cast<App*>(GetWindowLongPtrW(w,GWLP_USERDATA));
    if(m==WM_NCCREATE){a=static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);SetWindowLongPtrW(w,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(a));}
    if(!a)return DefWindowProcW(w,m,wp,lp);
    switch(m){
    case WM_MOUSEMOVE:a->cursor={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};InvalidateRect(w,nullptr,TRUE);return 0;
    case WM_LBUTTONDOWN:{POINT p{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};if(!a->firstPoint){a->first=p;a->firstPoint=true;InvalidateRect(w,nullptr,TRUE);}else{
        try{a->finishSelection(a->first,p);}catch(const std::exception&e){DestroyWindow(w);a->selector=nullptr;ShowWindow(a->window,SW_SHOW);SetForegroundWindow(a->window);a->error=widen(e.what());a->refreshStatus();MessageBoxW(a->window,a->error.c_str(),L"Não foi possível salvar a seleção",MB_OK|MB_ICONWARNING);}
    }return 0;}
    case WM_KEYDOWN:if(wp==VK_ESCAPE){DestroyWindow(w);a->selector=nullptr;ShowWindow(a->window,SW_SHOW);SetForegroundWindow(a->window);}return 0;
    case WM_CLOSE:DestroyWindow(w);return 0;
    case WM_DESTROY:if(a->selector==w){a->selector=nullptr;if(IsWindow(a->window)){ShowWindow(a->window,SW_SHOW);SetForegroundWindow(a->window);}}return 0;
    case WM_PAINT:{PAINTSTRUCT ps{};HDC dc=BeginPaint(w,&ps);RECT r{};GetClientRect(w,&r);FillRect(dc,&r,static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(255,255,255));SelectObject(dc,a->titleFont);
        std::wstring hint=a->firstPoint?L"Clique no canto oposto para confirmar · Esc cancela":L"Clique no primeiro canto da área específica · Esc cancela";
        TextOutW(dc,30,30,hint.c_str(),static_cast<int>(hint.size()));HPEN pen=CreatePen(PS_SOLID,2,RGB(0,255,220));auto old=SelectObject(dc,pen);SelectObject(dc,GetStockObject(NULL_BRUSH));
        MoveToEx(dc,a->cursor.x,0,nullptr);LineTo(dc,a->cursor.x,r.bottom);MoveToEx(dc,0,a->cursor.y,nullptr);LineTo(dc,r.right,a->cursor.y);
        if(a->firstPoint)Rectangle(dc,a->first.x,a->first.y,a->cursor.x,a->cursor.y);SelectObject(dc,old);DeleteObject(pen);EndPaint(w,&ps);return 0;}
    }return DefWindowProcW(w,m,wp,lp);
}
void App::choose(int kind){
    readEditor();stop();if(!target&&!connect())return;if(IsIconic(target))ShowWindow(target,SW_RESTORE);RECT cr{};GetClientRect(target,&cr);if(cr.right<=0||cr.bottom<=0)throw std::runtime_error("Janela indisponível para seleção.");
    if(settings.clientWidth!=cr.right||settings.clientHeight!=cr.bottom){settings.buffs={};settings.highlight={};}
    settings.clientWidth=cr.right;settings.clientHeight=cr.bottom;selection=kind;firstPoint=false;POINT p{};ClientToScreen(target,&p);
    ShowWindow(window,SW_HIDE);SetForegroundWindow(target);
    selector=CreateWindowExW(WS_EX_TOPMOST|WS_EX_LAYERED|WS_EX_APPWINDOW,L"AlbionPocSelector",L"Selecionar área específica",WS_POPUP,p.x,p.y,cr.right,cr.bottom,nullptr,nullptr,instance,this);
    if(!selector){ShowWindow(window,SW_SHOW);throw std::runtime_error("Não foi possível abrir o seletor.");}
    SetLayeredWindowAttributes(selector,0,105,LWA_ALPHA);ShowWindow(selector,SW_SHOW);SetForegroundWindow(selector);SetFocus(selector);
}
LRESULT CALLBACK appProc(HWND w,UINT m,WPARAM wp,LPARAM lp){
    auto*a=reinterpret_cast<App*>(GetWindowLongPtrW(w,GWLP_USERDATA));
    if(m==WM_NCCREATE){a=static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);a->window=w;SetWindowLongPtrW(w,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(a));}
    if(!a)return DefWindowProcW(w,m,wp,lp);
    try{switch(m){
    case WM_CREATE:a->makeUI();SetTimer(w,1,50,nullptr);RegisterHotKey(w,1,MOD_NOREPEAT,VK_F8);RegisterHotKey(w,2,MOD_NOREPEAT,VK_F9);return 0;
    case WM_COMMAND:if(HIWORD(wp)==BN_CLICKED){switch(LOWORD(wp)){
        case Connect:a->connect();break;case Buffs:a->choose(1);break;case Highlight:a->choose(2);break;
        case LoadReference:a->selectReference();break;case Start:a->start();break;case Stop:a->stop();break;
        case Save:a->readEditor();a->stop();a->persist();a->error=L"Configuração salva. Inicie a leitura para aplicar.";a->refreshStatus();break;
        case Sample:a->saveSample();break;
    }}return 0;
    case WM_HOTKEY:if(a->selector)return 0;if(wp==1){ShowWindow(w,SW_RESTORE);SetForegroundWindow(w);}else if(wp==2){if(a->running)a->stop();else a->start();}return 0;
    case ResultMessage:a->consume();return 0;
    case WM_TIMER:a->updateHighlight();a->refreshStatus();{RECT r{a->px(22),a->px(511),a->px(800),a->px(615)};InvalidateRect(w,&r,FALSE);}return 0;
    case WM_PAINT:a->paint();return 0;
    case WM_DPICHANGED:{a->stop();auto*r=reinterpret_cast<RECT*>(lp);SetWindowPos(w,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER);a->readEditor();a->makeUI();return 0;}
    case WM_CLOSE:DestroyWindow(w);return 0;
    case WM_DESTROY:a->stop();if(a->selector){auto selection=a->selector;a->selector=nullptr;DestroyWindow(selection);}KillTimer(w,1);UnregisterHotKey(w,1);UnregisterHotKey(w,2);PostQuitMessage(0);return 0;
    }}catch(const std::exception&e){a->stop();a->error=widen(e.what());a->refreshStatus();MessageBoxW(w,a->error.c_str(),L"Albion Assistant",MB_OK|MB_ICONWARNING);}
    return DefWindowProcW(w,m,wp,lp);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int show){
    try{SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);ComScope com;App app;app.instance=instance;
        wchar_t exe[32768]{};GetModuleFileNameW(nullptr,exe,32768);app.directory=std::filesystem::path(exe).parent_path();app.settingsPath=app.directory/L"settings.ini";
        int argc=0;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);for(int i=1;i<argc;i++){if(std::wstring(argv[i])==L"--settings"&&i+1<argc)app.settingsPath=argv[++i];else if(std::wstring(argv[i])==L"--diagnostics")app.diagnostics=true;else if(std::wstring(argv[i])==L"--show-overlay-in-capture")app.showOverlayInCapture=true;}LocalFree(argv);
        app.showOverlayInCapture=app.showOverlayInCapture&&app.diagnostics;
        app.settings=aa::loadSettings(app.settingsPath.wstring());app.overlay.initialize(instance);app.overlay.setCaptureVisible(app.showOverlayInCapture);
        WNDCLASSW wc{};wc.hInstance=instance;wc.lpfnWndProc=appProc;wc.lpszClassName=L"AlbionAssistantPoc";wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_BTNFACE+1);RegisterClassW(&wc);
        wc.lpfnWndProc=selectorProc;wc.lpszClassName=L"AlbionPocSelector";wc.hCursor=LoadCursorW(nullptr,IDC_CROSS);RegisterClassW(&wc);
        auto dpi=GetDpiForSystem();RECT bounds{0,0,MulDiv(824,dpi,96),MulDiv(640,dpi,96)};AdjustWindowRectExForDpi(&bounds,WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,FALSE,0,dpi);
        if(!CreateWindowExW(0,L"AlbionAssistantPoc",L"Albion Assistant — POC ao vivo",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,CW_USEDEFAULT,CW_USEDEFAULT,bounds.right-bounds.left,bounds.bottom-bounds.top,nullptr,nullptr,instance,&app))throw std::runtime_error("Falha ao abrir aplicativo.");
        ShowWindow(app.window,show);MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0){if(!IsDialogMessageW(app.window,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}}
        if(app.font)DeleteObject(app.font);if(app.titleFont)DeleteObject(app.titleFont);return 0;
    }catch(const std::exception&e){MessageBoxW(nullptr,widen(e.what()).c_str(),L"Albion Assistant — erro",MB_OK|MB_ICONERROR);return 1;}
}
