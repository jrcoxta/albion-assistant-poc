#include "app.h"
#include "theme.h"
#include "diagnostic_log.h"
#include <shellapi.h>
#include <objbase.h>
#include <shlobj.h>
#include <stdexcept>
#include "../resources/resource.h"

namespace aaapp {
std::wstring diagnosticHint(){
    const auto path=diagnostic_log::location();
    return path.empty()?L"\n\nNão foi possível gravar o registro de erros em %LOCALAPPDATA%\\AlbionAssistant\\logs."
                       :L"\n\nRegistro de erros: "+path;
}
LRESULT CALLBACK appProc(HWND window,UINT message,WPARAM wParam,LPARAM lParam){
    auto* app=reinterpret_cast<App*>(GetWindowLongPtrW(window,GWLP_USERDATA));
    if(message==WM_NCCREATE){app=static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);app->window=window;SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(app));}
    if(!app)return DefWindowProcW(window,message,wParam,lParam);
    try{switch(message){
    case WM_CREATE:{app->makeUI();SetTimer(window,1,50,nullptr);
        const bool panel=RegisterHotKey(window,1,MOD_NOREPEAT,VK_F8)!=FALSE;
        const bool reading=RegisterHotKey(window,2,MOD_NOREPEAT,VK_F9)!=FALSE;
        if(!panel||!reading){app->hotkeyWarning=L"Atalho ocupado; use os botões do painel.";app->refreshStatus();}return 0;}
    case WM_COMMAND:if(!app->rebuilding)app->command(LOWORD(wParam),HIWORD(wParam));return 0;
    case WM_HOTKEY:
        if(app->selecting)return 0;
        if(wParam==1){if(app->previewUntil)app->stop();ShowWindow(window,SW_RESTORE);SetForegroundWindow(window);}
        else if(wParam==2){if(app->running||app->previewUntil){app->stop();app->error=L"Leitura parada.";app->refreshStatus();}else app->start();}return 0;
    case ResultMessage:app->consume();return 0;
    case ActivateMessage:if(!app->selecting){if(app->previewUntil)app->stop();ShowWindow(window,SW_RESTORE);SetForegroundWindow(window);}return 0;
    case WM_TIMER:if(!app->selecting){app->updateHighlight();app->refreshStatus();}return 0;
    case WM_PAINT:app->paint();return 0;
    case WM_DPICHANGED:{app->stop();const auto* bounds=reinterpret_cast<RECT*>(lParam);SetWindowPos(window,nullptr,bounds->left,bounds->top,bounds->right-bounds->left,bounds->bottom-bounds->top,SWP_NOZORDER);app->rebuildUIWithDraft();return 0;}
    case WM_CLOSE:
        try{app->saveEditor();}catch(const std::exception& error){
            diagnostic_log::event("ui.close_save_failed");
            if(const auto* fs=dynamic_cast<const std::filesystem::filesystem_error*>(&error))
                diagnostic_log::detail("ui.close_save_filesystem",static_cast<unsigned>(fs->code().value()));
            const auto question=widen(error.what())+L"\n\nFechar sem salvar a edição atual? Os dados já salvos serão mantidos."+diagnosticHint();
            if(MessageBoxW(window,question.c_str(),L"Edição incompleta",MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2)!=IDYES)return 0;
        }
        DestroyWindow(window);return 0;
    case WM_DESTROY:app->stop();if(app->badge){DestroyWindow(app->badge);app->badge=nullptr;}KillTimer(window,1);UnregisterHotKey(window,1);UnregisterHotKey(window,2);PostQuitMessage(0);return 0;
    }}catch(const std::exception& error){
        diagnostic_log::detail("ui.exception_message",message);
        if(message==WM_COMMAND)diagnostic_log::detail("ui.command",LOWORD(wParam));
        app->stop();app->error=widen(error.what());
        if(message==WM_CREATE){diagnostic_log::event("ui.create_failed");return -1;}
        app->refreshStatus();
        const auto notice=app->error+diagnosticHint();
        MessageBoxW(window,notice.c_str(),L"Albion Assistant",MB_OK|MB_ICONWARNING);
    }
    return DefWindowProcW(window,message,wParam,lParam);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int show){
    using namespace aaapp;
    const char* stage="startup.begin";
    try{
        wchar_t local[MAX_PATH]{};
        const auto folderResult=SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA,nullptr,SHGFP_TYPE_CURRENT,local);
        if(SUCCEEDED(folderResult))
            diagnostic_log::configure(std::filesystem::path(local)/L"AlbionAssistant");
        diagnostic_log::event("startup.version_" ALBION_VERSION);
        const auto mutex=CreateMutexW(nullptr,FALSE,L"Local\\AlbionAssistant");const bool alreadyOpen=GetLastError()==ERROR_ALREADY_EXISTS;
        if(!mutex){const auto code=GetLastError();diagnostic_log::win32("startup.CreateMutexW",code);throw std::runtime_error("Não foi possível iniciar o aplicativo (Win32 "+std::to_string(code)+").");}
        const std::unique_ptr<void,decltype(&CloseHandle)> lock(mutex,&CloseHandle);
        if(alreadyOpen){for(int attempt=0;attempt<20;++attempt){if(auto existing=FindWindowW(AppWindowClass,nullptr)){DWORD process=0;GetWindowThreadProcessId(existing,&process);AllowSetForegroundWindow(process);PostMessageW(existing,ActivateMessage,0,0);break;}Sleep(100);}return 0;}
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        const auto com=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
        struct ComEnd{HRESULT hr;~ComEnd(){if(SUCCEEDED(hr))CoUninitialize();}} comEnd{com};
        App app;app.instance=instance;std::filesystem::path settingsFile;
        int argc=0;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);
        for(int i=1;i<argc;++i){const std::wstring arg=argv[i];if(arg==L"--settings"&&i+1<argc)settingsFile=argv[++i];else if(arg==L"--diagnostics")app.diagnostics=true;else if(arg==L"--show-overlay-in-capture")app.showOverlayInCapture=true;}
        LocalFree(argv);stage="startup.storage";app.configureStorage(settingsFile);
        stage="startup.workspace";app.load();
        WNDCLASSW cls{};cls.hInstance=instance;cls.lpfnWndProc=appProc;cls.lpszClassName=AppWindowClass;cls.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(IDI_APP));cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);
        if(!RegisterClassW(&cls)){const auto code=GetLastError();diagnostic_log::win32("startup.RegisterClassW",code);throw std::runtime_error("Falha ao registrar janela (Win32 "+std::to_string(code)+").");}
        const auto dpi=GetDpiForSystem();RECT bounds{0,0,MulDiv(860,dpi,96),MulDiv(700,dpi,96)};
        const auto style=WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX|WS_CLIPCHILDREN;AdjustWindowRectExForDpi(&bounds,style,FALSE,0,dpi);
        stage="startup.window";
        if(!CreateWindowExW(0,AppWindowClass,L"Albion Assistant",style,CW_USEDEFAULT,CW_USEDEFAULT,bounds.right-bounds.left,bounds.bottom-bounds.top,nullptr,nullptr,instance,&app)){
            const auto code=GetLastError();diagnostic_log::win32("startup.CreateWindowExW",code);
            if(app.error!=L"Conecte ao jogo. Escolha uma HUD e um set para iniciar.")
                throw std::runtime_error(utf8(app.error));
            throw std::runtime_error("Falha ao abrir aplicativo (Win32 "+std::to_string(code)+").");
        }
        diagnostic_log::event("startup.ready");
        ShowWindow(app.window,show);MSG message{};while(GetMessageW(&message,nullptr,0,0)>0){if(!IsDialogMessageW(app.window,&message)){TranslateMessage(&message);DispatchMessageW(&message);}}return 0;
    }catch(const std::exception& error){
        diagnostic_log::event(stage);
        const auto notice=widen(error.what())+diagnosticHint();
        MessageBoxW(nullptr,notice.c_str(),L"Albion Assistant — erro",MB_OK|MB_ICONERROR);return 1;
    }
}
