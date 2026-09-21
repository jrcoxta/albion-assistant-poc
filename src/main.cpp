#include "app.h"
#include "theme.h"
#include <shellapi.h>
#include <objbase.h>
#include <stdexcept>
#include "../resources/resource.h"

namespace aaapp {
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
            const auto question=widen(error.what())+L"\n\nFechar sem salvar a edição atual? Os dados já salvos serão mantidos.";
            if(MessageBoxW(window,question.c_str(),L"Edição incompleta",MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2)!=IDYES)return 0;
        }
        DestroyWindow(window);return 0;
    case WM_DESTROY:app->stop();if(app->badge){DestroyWindow(app->badge);app->badge=nullptr;}KillTimer(window,1);UnregisterHotKey(window,1);UnregisterHotKey(window,2);PostQuitMessage(0);return 0;
    }}catch(const std::exception& error){app->stop();app->error=widen(error.what());app->refreshStatus();MessageBoxW(window,app->error.c_str(),L"Albion Assistant",MB_OK|MB_ICONWARNING);}
    return DefWindowProcW(window,message,wParam,lParam);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int show){
    using namespace aaapp;
    try{
        const auto mutex=CreateMutexW(nullptr,FALSE,L"Local\\AlbionAssistant");const bool alreadyOpen=GetLastError()==ERROR_ALREADY_EXISTS;
        if(!mutex)throw std::runtime_error("Não foi possível iniciar o aplicativo.");
        const std::unique_ptr<void,decltype(&CloseHandle)> lock(mutex,&CloseHandle);
        if(alreadyOpen){for(int attempt=0;attempt<20;++attempt){if(auto existing=FindWindowW(AppWindowClass,nullptr)){DWORD process=0;GetWindowThreadProcessId(existing,&process);AllowSetForegroundWindow(process);PostMessageW(existing,ActivateMessage,0,0);break;}Sleep(100);}return 0;}
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        const auto com=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
        struct ComEnd{HRESULT hr;~ComEnd(){if(SUCCEEDED(hr))CoUninitialize();}} comEnd{com};
        App app;app.instance=instance;std::filesystem::path settingsFile;
        int argc=0;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);
        for(int i=1;i<argc;++i){const std::wstring arg=argv[i];if(arg==L"--settings"&&i+1<argc)settingsFile=argv[++i];else if(arg==L"--diagnostics")app.diagnostics=true;else if(arg==L"--show-overlay-in-capture")app.showOverlayInCapture=true;}
        LocalFree(argv);app.showOverlayInCapture=app.showOverlayInCapture&&app.diagnostics;app.configureStorage(settingsFile);app.load();
        WNDCLASSW cls{};cls.hInstance=instance;cls.lpfnWndProc=appProc;cls.lpszClassName=AppWindowClass;cls.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(IDI_APP));cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);
        RegisterClassW(&cls);const auto dpi=GetDpiForSystem();RECT bounds{0,0,MulDiv(860,dpi,96),MulDiv(700,dpi,96)};
        const auto style=WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX|WS_CLIPCHILDREN;AdjustWindowRectExForDpi(&bounds,style,FALSE,0,dpi);
        if(!CreateWindowExW(0,AppWindowClass,L"Albion Assistant",style,CW_USEDEFAULT,CW_USEDEFAULT,bounds.right-bounds.left,bounds.bottom-bounds.top,nullptr,nullptr,instance,&app))throw std::runtime_error("Falha ao abrir aplicativo.");
        ShowWindow(app.window,show);MSG message{};while(GetMessageW(&message,nullptr,0,0)>0){if(!IsDialogMessageW(app.window,&message)){TranslateMessage(&message);DispatchMessageW(&message);}}return 0;
    }catch(const std::exception& error){MessageBoxW(nullptr,widen(error.what()).c_str(),L"Albion Assistant — erro",MB_OK|MB_ICONERROR);return 1;}
}
