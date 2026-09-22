// Exercita somente janelas e arquivos do próprio teste; não interage com o jogo.
#include "app.h"
#include "theme.h"
#include <objbase.h>
#include "../resources/resource.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <thread>

using namespace aaapp;
namespace {
bool forceRebuildPaint=false;
int intermediatePaints=0;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct TestApp:App {
    ~TestApp(){
        stop();if(window)DestroyWindow(window);if(target)DestroyWindow(target);window=nullptr;target=nullptr;
        std::error_code ignored;
        const auto resolved=std::filesystem::weakly_canonical(directory,ignored);
        const auto temporary=std::filesystem::weakly_canonical(std::filesystem::temp_directory_path(),ignored);
        if(!ignored&&resolved.parent_path()==temporary&&resolved.filename().wstring().starts_with(L"albion-app-flow-"))std::filesystem::remove_all(resolved,ignored);
    }
};
LRESULT CALLBACK testProc(HWND window,UINT message,WPARAM wp,LPARAM lp){
    auto* app=reinterpret_cast<App*>(GetWindowLongPtrW(window,GWLP_USERDATA));
    if(message==WM_NCCREATE){app=static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(app));}
    if(message==WM_PARENTNOTIFY&&app&&app->rebuilding&&forceRebuildPaint)UpdateWindow(window);
    if(message==WM_PAINT&&app){if(app->rebuilding)++intermediatePaints;app->paint();return 0;}
    return DefWindowProcW(window,message,wp,lp);
}
void tab(App& app,int page){const int physical[]={3,0,1,2};app.command(Tab0+physical[page],BN_CLICKED);require(app.page==physical[page],"navegação não mudou de página");}
void choose(App& app,int id,int index,bool list=false){SendMessageW(app.item(id),list?LB_SETCURSEL:CB_SETCURSEL,index,0);app.command(id,list?LBN_SELCHANGE:CBN_SELCHANGE);}
bool shows(App& app,const wchar_t* phrase){return std::any_of(app.controls.begin(),app.controls.end(),[&](HWND control){return text(control).find(phrase)!=std::wstring::npos;});}
void createNamed(App& app,int command,const wchar_t* name,bool cancel=false,bool invalid=false){
    const bool inlineName=(command==NewHud&&app.workspace.huds.empty())||(command==NewArea&&app.hud()&&app.hud()->areas.empty())||
        (command==NewStatus&&app.workspace.statuses.empty())||(command==NewSet&&app.workspace.sets.empty())||(command==NewRule&&app.set()&&app.set()->rules.empty());
    if(auto field=inlineName?app.item(720):nullptr){
        SetWindowTextW(field,name);
        if(cancel)return;
        if(invalid){bool rejected=false;try{app.command(command,BN_CLICKED);}catch(const std::exception&){rejected=true;}require(rejected,"nome inválido aceito");return;}
        app.command(command,BN_CLICKED);return;
    }
    const DWORD ownerThread=GetCurrentThreadId();std::atomic<bool> answered=false;
    std::thread responder([&]{
        const auto deadline=GetTickCount64()+2000;
        while(GetTickCount64()<deadline&&!answered){
            struct Context{HWND owner;const wchar_t* name;bool cancel,invalid,handled=false;} context{app.window,name,cancel,invalid};
            EnumThreadWindows(ownerThread,[](HWND dialog,LPARAM lp)->BOOL{
                auto& c=*reinterpret_cast<Context*>(lp);
                if(GetWindow(dialog,GW_OWNER)!=c.owner||!GetDlgItem(dialog,710))return TRUE;
                SetDlgItemTextW(dialog,710,c.name);
                SendMessageW(dialog,WM_COMMAND,c.cancel?IDCANCEL:IDOK,0);
                if(c.invalid&&IsWindow(dialog))SendMessageW(dialog,WM_COMMAND,IDCANCEL,0);
                c.handled=true;return FALSE;
            },reinterpret_cast<LPARAM>(&context));
            if(context.handled){answered=true;break;}Sleep(10);
        }
    });
    try{
        if(command==NewHud&&app.page==0&&app.item(HudList)){
            SendMessageW(app.item(HudList),CB_SETCURSEL,app.workspace.huds.size(),0);
            app.command(HudList,CBN_SELCHANGE);
        }else app.command(command,BN_CLICKED);
    }catch(...){responder.join();throw;}
    responder.join();require(answered,"cadastro deve pedir nome antes de criar");
}
void confirmDeleteHud(App& app,int answer){
    // Responde exclusivamente ao diálogo pertencente a esta janela/processo de teste.
    const DWORD ownerThread=GetCurrentThreadId();std::atomic<bool> answered=false;
    std::thread responder([&]{
        const auto deadline=GetTickCount64()+4000;
        while(GetTickCount64()<deadline&&!answered){
            struct Context{HWND owner;int answer;bool handled=false;} context{app.window,answer};
            EnumThreadWindows(ownerThread,[](HWND candidate,LPARAM lp)->BOOL{
                auto& c=*reinterpret_cast<Context*>(lp);wchar_t type[64]{};GetClassNameW(candidate,type,64);
                if(GetWindow(candidate,GW_OWNER)==c.owner&&std::wstring(type)==L"#32770"){
                    PostMessageW(candidate,WM_COMMAND,c.answer,0);c.handled=true;return FALSE;
                }return TRUE;
            },reinterpret_cast<LPARAM>(&context));
            if(context.handled){answered=true;break;}Sleep(10);
        }
    });
    try{app.command(DeleteHud,BN_CLICKED);}catch(...){answered=true;responder.join();throw;}
    responder.join();require(answered,"diálogo de exclusão da HUD não foi apresentado");
}
void screenshot(App& app,const std::filesystem::path& folder,const wchar_t* name){
    if(folder.empty())return;
    ShowWindow(app.window,SW_SHOWNOACTIVATE);UpdateWindow(app.window);
    RECT bounds{};GetClientRect(app.window,&bounds);auto screen=GetDC(app.window);auto memory=CreateCompatibleDC(screen);
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=bounds.right;info.bmiHeader.biHeight=-bounds.bottom;
    info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;void* pixels=nullptr;
    auto bitmap=CreateDIBSection(screen,&info,DIB_RGB_COLORS,&pixels,nullptr,0);auto old=SelectObject(memory,bitmap);
    const bool printed=PrintWindow(app.window,memory,PW_CLIENTONLY)!=FALSE;GdiFlush();
    aa::Image image;image.width=bounds.right;image.height=bounds.bottom;
    image.bgra.assign(static_cast<const std::uint8_t*>(pixels),static_cast<const std::uint8_t*>(pixels)+static_cast<std::size_t>(image.width)*image.height*4);
    for(std::size_t i=3;i<image.bgra.size();i+=4)image.bgra[i]=255;
    SelectObject(memory,old);DeleteObject(bitmap);DeleteDC(memory);ReleaseDC(app.window,screen);ShowWindow(app.window,SW_HIDE);
    require(printed,"renderização de teste indisponível");std::filesystem::create_directories(folder);aa::saveImage(image,folder/name);
}
}
int wmain(int argc,wchar_t** argv){
    const auto com=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    struct ComEnd{HRESULT hr;~ComEnd(){if(SUCCEEDED(hr))CoUninitialize();}} cleanup{com};
    try{
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        TestApp app;app.instance=GetModuleHandleW(nullptr);
        const auto folder=std::filesystem::temp_directory_path()/(L"albion-app-flow-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64()));
        app.configureStorage(folder/L"personalizado.ini");
        require(app.workspacePath==folder/L"personalizado-workspace.ini"&&app.directory==folder,"dados de teste não isolados");
        aa::Workspace w;w.nextId=100;
        aa::HudLayout notebook;notebook.id=L"h1";notebook.name=L"Notebook";notebook.clientWidth=800;notebook.clientHeight=600;
        notebook.areas={{L"Meus status",{20,20,250,60},48,true},{L"Habilidade E",{300,400,64,64},48,false}};
        auto large=notebook;large.id=L"h2";large.name=L"Monitor 34";large.areas[0].region.x=80;
        w.huds={notebook,large};w.activeHudId=L"h1";
        w.statuses={{L"s1",L"Espírito Assassino",false,true,{},{}},{L"s2",L"Veneno",true,false,{},{}}};
        aa::StatusRule rule;rule.id=L"r1";rule.statusId=L"s1";rule.sourceArea=L"Meus status";rule.targetArea=L"Habilidade E";
        rule.condition.name=L"Preparar golpe";rule.condition.condition=aa::Condition::StacksEqual;rule.condition.stacks=3;
        rule.stackSamples={{3,app.storeImage(L"r1",aa::loadImageResource(IDR_ASSASSIN_3))}};
        w.sets={{L"set1",L"Adagas",{rule}},{L"set2",L"Cajado",{}}};w.activeSetId=L"set1";
        app.commit(w);app.selectedStatusId=L"s1";
        WNDCLASSW cls{};cls.hInstance=app.instance;cls.lpfnWndProc=testProc;cls.lpszClassName=L"AlbionAppFlowTests";cls.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_BTNFACE+1);RegisterClassW(&cls);
        app.window=CreateWindowExW(0,cls.lpszClassName,L"Validação do painel",WS_OVERLAPPED|WS_CAPTION,20,20,880,740,nullptr,nullptr,app.instance,&app);
        app.target=CreateWindowExW(0,L"STATIC",L"Alvo do teste",WS_POPUP,0,0,800,600,nullptr,nullptr,app.instance,nullptr);
        require(app.window&&app.target,"janelas de teste não criadas");app.makeUI();
        require(app.item(HudList)!=nullptr&&!app.item(HudName)&&!app.item(NewHud)&&!app.item(Start),"HUD existente deve ter seletor sem formulário de nome/criação");
        require(!IsWindowVisible(app.window)&&!app.rebuilding,"construção mostrou janela originalmente oculta ou deixou bloqueio ativo");
        {
            app.commit({});app.makeUI();
            require(app.item(NewHud)&&!app.item(HudName)&&!app.item(AreaList),"primeiro uso deve mostrar somente orientação e criação da HUD");
            require(app.item(720)!=nullptr,"primeira HUD precisa aceitar nome antes de criar");
            SetWindowTextW(app.item(720),L"Nome em edição");app.rebuildUIWithDraft();
            require(text(app.item(720))==L"Nome em edição","reconstrução apagou nome do novo cadastro");
            createNamed(app,NewHud,L"Minha tela");
            require(app.hud()&&app.hud()->name==L"Minha tela"&&app.item(NewArea)&&!app.item(AreaName),"HUD sem áreas deve oferecer criação sem editor vazio");
            createNamed(app,NewArea,L"E");
            require(app.area()&&app.area()->name==L"E"&&app.item(SelectArea)&&!app.item(Save),"área deve ser editável e salva automaticamente");
            require(aa::loadWorkspace(app.workspacePath,{}).huds.front().areas.front().name==L"E","criação direta não persistiu a área");
            tab(app,2);
            require(app.item(NewStatus)&&!app.item(StatusName)&&!app.item(CaptureStatus),"biblioteca vazia não deve exibir editor desabilitado");
            createNamed(app,NewStatus,L"Meu status");
            require(app.item(CaptureStatus)&&!app.item(Save),"status deve permitir captura com edição automática");
            tab(app,3);
            require(app.item(NewSet)&&!app.item(RuleName),"perfis vazios não devem exibir editor de regra");
            createNamed(app,NewSet,L"Meu perfil");
            require(app.item(NewRule)&&!app.item(RuleName),"perfil vazio deve orientar criação da primeira regra");
            createNamed(app,NewRule,L"Meu destaque");
            require(app.item(RuleName)&&!app.item(Save),"regra deve abrir para edição automática");
            app.commit(w);app.selectedArea=-1;app.selectedRule=-1;app.selectedStatusId=L"s1";app.page=0;app.makeUI();
        }
        {
            tab(app,0);const auto start=app.item(Start);const auto source=app.source;
            app.running=true;choose(app,HudList,0);choose(app,SetList,0);
            require(app.item(Start)==start&&app.running&&app.source==source,"seleção já ativa reconstruiu painel ou interrompeu leitura");
            app.running=false;
            ShowWindow(app.window,SW_SHOWNOACTIVATE);
            SetActiveWindow(app.window);
            SetFocus(app.item(Validity));SetWindowTextW(app.item(Validity),L"731");SendMessageW(app.item(Validity),EM_SETSEL,1,2);
            if(GetFocus()!=app.item(Validity))std::cerr<<"initial focus: enabled="<<IsWindowEnabled(app.item(Validity))<<" visible="<<IsWindowVisible(app.item(Validity))<<" parent="<<IsWindowVisible(app.window)<<" error="<<GetLastError()<<'\n';
            require(GetFocus()==app.item(Validity),"teste não conseguiu estabelecer foco antes da reconstrução");
            forceRebuildPaint=true;app.rebuildUIWithDraft();forceRebuildPaint=false;
            require(intermediatePaints==0,"reconstrução expôs pintura intermediária do painel");
            DWORD begin=0,end=0;SendMessageW(app.item(Validity),EM_GETSEL,reinterpret_cast<WPARAM>(&begin),reinterpret_cast<LPARAM>(&end));
            if(!IsWindowVisible(app.window)||GetFocus()!=app.item(Validity)||text(app.item(Validity))!=L"731"||begin!=1||end!=2)
                std::cerr<<"rebuild: visible="<<IsWindowVisible(app.window)<<" focus="<<(GetFocus()==app.item(Validity))<<" draft="<<(text(app.item(Validity))==L"731")<<" selection="<<begin<<","<<end<<'\n';
            require(IsWindowVisible(app.window)&&GetFocus()==app.item(Validity)&&text(app.item(Validity))==L"731"&&begin==1&&end==2,
                    "reconstrução perdeu visibilidade, foco, rascunho ou cursor");
            SetWindowTextW(app.item(Validity),L"750");
            try{theme::RedrawLock redraw(app.window);throw std::runtime_error("falha controlada");}catch(const std::exception&){}
            require(IsWindowVisible(app.window)&&!GetPropW(app.window,L"Albion.Theme.Redraw")&&!GetPropW(app.window,L"SysSetRedraw"),
                    "exceção deixou pintura bloqueada");
            ShowWindow(app.window,SW_HIDE);app.rebuildUIWithDraft();require(!IsWindowVisible(app.window),"restauração de rascunho exibiu painel oculto");
        }
        const auto pictures=argc>1?std::filesystem::absolute(argv[1]):std::filesystem::path{};
        require(app.item(Start)&&!app.item(RuleName)&&!app.item(StatusName)&&!app.item(AreaName),"Monitor mistura editores");
        require(app.item(ShareOverlay)&&SendMessageW(app.item(ShareOverlay),BM_GETCHECK,0,0)==BST_UNCHECKED,
                "Monitor nao oferece compartilhamento do overlay desativado por padrao");
        SendMessageW(app.item(ShareOverlay),BM_SETCHECK,BST_CHECKED,0);app.command(ShareOverlay,BN_CLICKED);
        require(app.showOverlayInCapture&&app.workspace.shareOverlayInCapture&&
                aa::loadWorkspace(app.workspacePath,{}).shareOverlayInCapture,
                "compartilhamento do overlay nao atualizou e salvou a preferencia");
        screenshot(app,pictures,L"ui-monitor.png");
        {
            const auto original=app.workspace;
            tab(app,1);createNamed(app,NewHud,L"HUD criada uma vez");
            require(app.workspace.huds.size()==3&&app.hud()->name==L"HUD criada uma vez",
                    "criar HUD exige outro Novo ou deixa nome provisório");
            const auto selected=app.workspace.activeHudId;
            createNamed(app,NewHud,L"Cancelado",true);
            require(app.workspace.huds.size()==3&&app.workspace.activeHudId==selected&&app.hud()->name==L"HUD criada uma vez",
                    "cancelar criação salvou, perdeu rascunho ou mudou seleção");
            createNamed(app,NewHud,L"   ",false,true);createNamed(app,NewHud,L"hud criada uma vez",false,true);
            require(app.workspace.huds.size()==3&&app.workspace.activeHudId==selected,"nome inválido ou repetido criou HUD");
            createNamed(app,NewArea,L"Buffs criados uma vez");
            require(app.hud()->areas.size()==1&&app.area()->name==L"Buffs criados uma vez","criação da área perdeu nome/seleção");
            SetWindowTextW(app.item(AreaName),L"Área em edição");createNamed(app,NewArea,L"Cancelada",true);
            require(app.area()->name==L"Buffs criados uma vez"&&text(app.item(AreaName))==L"Área em edição","cancelar perdeu ou gravou rascunho da área");
            SetWindowTextW(app.item(AreaName),L"Buffs criados uma vez");createNamed(app,NewArea,L"buffs criados uma vez",false,true);
            require(app.hud()->areas.size()==1&&app.area()->name==L"Buffs criados uma vez","cancelar/rejeitar área alterou a seleção");
            tab(app,2);createNamed(app,NewStatus,L"Status criado uma vez");
            require(app.workspace.statuses.size()==3&&app.selectedStatus()->name==L"Status criado uma vez"&&!app.selectedStatus()->builtinAssassin&&app.selectedStatus()->stacks.empty(),
                    "criação do status perdeu nome ou herdou preset");
            SetWindowTextW(app.item(StatusName),L"Status em edição");createNamed(app,NewStatus,L"Cancelado",true);
            require(app.selectedStatus()->name==L"Status criado uma vez"&&text(app.item(StatusName))==L"Status em edição","cancelar perdeu ou gravou rascunho do status");
            SetWindowTextW(app.item(StatusName),L"Status criado uma vez");createNamed(app,NewStatus,L"status criado uma vez",false,true);
            require(app.workspace.statuses.size()==3&&app.selectedStatus()->name==L"Status criado uma vez","cancelar/rejeitar status alterou a seleção");
            tab(app,3);createNamed(app,NewSet,L"Set criado uma vez");
            SetWindowTextW(app.item(SetName),L"Set em edição");createNamed(app,NewSet,L"Cancelado",true);
            require(app.set()->name==L"Set criado uma vez"&&text(app.item(SetName))==L"Set em edição","cancelar perdeu ou gravou rascunho do set");
            SetWindowTextW(app.item(SetName),L"Set criado uma vez");createNamed(app,NewSet,L"set criado uma vez",false,true);
            require(app.workspace.sets.size()==3&&app.set()->name==L"Set criado uma vez","cancelar/rejeitar set alterou a seleção");
            createNamed(app,NewRule,L"Regra criada uma vez");
            require(app.workspace.sets.size()==3&&app.set()->rules.size()==1&&app.rule()->condition.name==L"Regra criada uma vez",
                    "criação de set/regra exige outro Novo");
            createNamed(app,NewRule,L"REGRA CRIADA UMA VEZ",false,true);
            SetWindowTextW(app.item(RuleName),L"Regra em edição");createNamed(app,NewRule,L"Cancelada",true);
            require(app.rule()->condition.name==L"Regra criada uma vez"&&text(app.item(RuleName))==L"Regra em edição","cancelar perdeu ou gravou rascunho da regra");
            require(app.set()->rules.size()==1,"criação aceitou regra duplicada");
            SetWindowTextW(app.item(RuleName),L"Regra editada");app.command(Save,BN_CLICKED);
            wchar_t savedRule[256]{};SendMessageW(app.item(RuleList),LB_GETTEXT,0,reinterpret_cast<LPARAM>(savedRule));
            require(std::wstring(savedRule)==L"Regra editada"&&aa::loadWorkspace(app.workspacePath,{}).sets.back().rules.front().condition.name==L"Regra editada",
                    "Salvar não refletiu edição na lista e no arquivo");
            app.commit(original);app.selectedStatusId=L"s1";app.selectedArea=-1;app.selectedRule=-1;app.page=3;app.makeUI();
        }
        choose(app,HudList,1);require(app.workspace.activeHudId==L"h2"&&app.workspace.activeSetId==L"set1"&&app.set()->rules[0].condition.stacks==3,"troca de HUD alterou o set");
        choose(app,SetList,1);require(app.workspace.activeHudId==L"h2"&&app.workspace.activeSetId==L"set2","troca de set alterou a HUD");choose(app,SetList,0);

        app.hud()->areas[0].iconCalibrated=false;
        tab(app,2);app.start();
        require(app.page==3&&!app.running&&app.recognizers.empty()&&app.overlays.empty(),"origem sem calibração iniciou leitura");
        require(!app.capturePreview.valid(),"referência do status apareceu como captura após início bloqueado");
        require(app.referencePreview.valid(),"início bloqueado perdeu a referência da biblioteca");
        require(app.error.find(L"Meus status")!=std::wstring::npos&&app.error.find(L"Monitor 34")!=std::wstring::npos&&
            app.error.find(L"Medir ícone de status")!=std::wstring::npos,"aviso não informa a HUD, a área e como calibrar");
        screenshot(app,pictures,L"ui-monitor-blocked.png");
        app.hud()->areas[0].iconCalibrated=true;
        require(aa::readinessIssues(app.workspace).empty(),"calibração válida manteve pendência");
        // Um frame entregue pelo capturador só pode ser substituído por outra captura.
        const aa::Image captured{250,60,std::vector<std::uint8_t>(250*60*4,37)};
        app.running=true;app.latestSource=app.source;app.latestImage=captured;app.pending=true;app.consume();app.stop();
        require(app.capturePreview.bgra==captured.bgra,"Monitor não recebeu o frame capturado");
        // Abrir o painel pausa a captura, mas deve permitir consultar a leitura
        // anterior sem que ela mantenha um destaque aceso.
        const auto previousPlan=app.plan;
        app.plan=aa::makeMonitorPlan(app.workspace);app.running=true;++app.source;
        app.latestSource=app.source;
        app.latest={{{aa::Presence::Present,3u,1.0f,{},{}},static_cast<std::int64_t>(GetTickCount64()),app.source}};
        app.latestImage=captured;app.pending=true;app.consume();
        app.latest={{{},0,app.source}};app.latestError=L"Janela alvo não está em primeiro plano.";app.pending=true;app.consume();
        require(shows(app,L"Última leitura")&&shows(app,L"stacks 3")&&!app.lit.front(),
                "Abrir painel apagou o diagnóstico anterior ou manteve destaque sem captura atual");
        screenshot(app,pictures,L"ui-monitor-ultima-leitura.png");
        const auto readingNow=static_cast<std::int64_t>(GetTickCount64());
        app.current=app.lastReadings;app.current.front().capturedMs=readingNow;app.evaluateReadings(readingNow,true);
        require(app.lit.front(),"controle positivo não acendeu com leitura recente e alvo em foco");
        app.plan.actions.front().rule.followClock=true;app.plan.readers.front().needsClock=true;
        for(int n=0;n<10;++n){
            app.current.front().capturedMs=1000+n*150;
            app.current.front().detection.remainingFraction=.95f-n*.03f;
            app.evaluateReadings(1000+n*150,true);
        }
        app.current.front().capturedMs=2500;app.current.front().detection.remainingFraction.reset();
        const auto predicted=app.evaluateReadings(2500,true);
        require(app.lit.front()&&predicted.front()&&*predicted.front()<.70f&&*predicted.front()>.60f,
                "aro sumiu ao encobrir relogio apesar de status atual e velocidade consistente");
        require(!app.current.front().detection.remainingFraction,"estimativa sobrescreveu observacao original");
        app.current.front().detection.stacks=2u;
        require(!app.evaluateReadings(2510,true).front()&&!app.lit.front(),"previsao manteve destaque com stacks incorretos");
        app.current.front().detection.stacks=3u;
        require(!app.evaluateReadings(2520,false).front()&&!app.lit.front(),"previsao manteve destaque fora do jogo");
        require(!app.evaluateReadings(2530,true).front(),"previsao atravessou pausa sem reaprender");
        app.current={{{},0,app.source}};app.evaluateReadings(readingNow,true);
        require(!app.lit.front(),"histórico acendeu destaque sem captura atual com jogo em foco");
        app.current=app.lastReadings;app.current.front().capturedMs=readingNow-app.workspace.validityMs;
        app.evaluateReadings(readingNow,true);require(!app.lit.front(),"histórico acendeu destaque com captura expirada");
        ++app.source;app.refreshStatus();
        require(!shows(app,L"Última leitura"),"nova fonte mostrou diagnóstico da fonte anterior");
        app.stop();app.refreshStatus();
        require(!shows(app,L"Última leitura"),"Parar conservou diagnóstico de uma sessão anterior");
        app.plan=previousPlan;
        tab(app,2);
        require(app.capturePreview.width==captured.width&&app.capturePreview.height==captured.height&&app.capturePreview.bgra==captured.bgra,
            "abrir Status substituiu a captura pela referência");
        app.hud()->areas[0].iconCalibrated=false;app.start();
        require(!app.capturePreview.valid(),"nova tentativa bloqueada preservou captura da sessão anterior");
        app.hud()->areas[0].iconCalibrated=true;
        app.capturePreview=captured;
        const auto savedValidity=app.workspace.validityMs;
        SetWindowTextW(app.item(Validity),L"");bool invalidStart=false;
        try{app.start();}catch(const std::exception&){invalidStart=true;app.stop();}
        require(invalidStart&&!app.capturePreview.valid(),"campo inválido manteve captura antiga na tentativa de iniciar");
        require(app.workspace.validityMs==savedValidity&&aa::loadWorkspace(app.workspacePath,{}).validityMs==savedValidity,
            "início com campo inválido alterou ajuste salvo");
        SetWindowTextW(app.item(Validity),std::to_wstring(savedValidity).c_str());
        app.hud()->clientWidth=900;app.start();
        require(!app.running&&std::any_of(app.controls.begin(),app.controls.end(),[](HWND control){
            return text(control).find(L"A HUD não corresponde à resolução/escala atual")!=std::wstring::npos;
        }),"orientação de tela incompatível desapareceu ao resumir o rodapé");
        screenshot(app,pictures,L"ui-monitor-screen-mismatch.png");
        app.hud()->clientWidth=800;

        tab(app,1);require(app.item(HudOptions)&&!app.item(HudName)&&!app.item(NewHud)&&!app.item(DeleteHud)&&app.item(AreaName)&&!app.item(RuleName)&&!app.item(CaptureStatus),"HUD mistura ações ou outros cadastros");
        {
            const auto original=app.workspace;
            app.hud()->areas.push_back({L"Q",{380,400,64,64},48,false});
            choose(app,AreaList,1,true);
            require(!app.item(CalibrateArea)&&shows(app,L"Não precisa medir ícone"),"destino E pede medição de ícone");
            require(aa::readinessIssues(app.workspace).empty(),"destino não medido bloqueia a regra");
            screenshot(app,pictures,L"ui-hud-destino.png");
            app.area()->region.shape=aa::RegionShape::Circle;app.makeUI();
            require(shows(app,L"Área circular salva")&&!app.item(CalibrateArea),"destino circular perdeu forma na interface ou exigiu medição");
            app.saveEditor();require(aa::loadWorkspace(app.workspacePath,{}).huds[1].areas[1].region.shape==aa::RegionShape::Circle,
                "interface não preservou formato da área");
            screenshot(app,pictures,L"ui-hud-circulo.png");
            app.area()->region.shape=aa::RegionShape::Rectangle;
            app.area()->iconCalibrated=true;app.area()->iconSize=57;app.makeUI();
            require(!app.item(CalibrateArea)&&!shows(app,L"57 px"),"destino mostra medição de ícone sem uso");
            choose(app,AreaList,2,true);
            require(app.item(CalibrateArea)&&shows(app,L"opcional")&&!shows(app,L"pendente"),"área Q sem regra apresenta medição como pendência");
            screenshot(app,pictures,L"ui-hud-sem-regra.png");
            app.area()->region={};app.makeUI();
            require(!IsWindowEnabled(app.item(CalibrateArea)),"medição habilitada antes de selecionar a área");
            choose(app,AreaList,0,true);app.area()->iconCalibrated=false;app.makeUI();
            require(IsWindowEnabled(app.item(CalibrateArea))&&!aa::readinessIssues(app.workspace).empty(),"origem deixou de exigir medição");
            screenshot(app,pictures,L"ui-hud-leitura.png");
            app.area()->iconCalibrated=true;
            auto other=rule;other.id=L"r-other";other.sourceArea=L"HABILIDADE E";other.targetArea=L"Meus status";other.condition.enabled=false;
            app.workspace.sets[1].rules.push_back(other);app.makeUI();choose(app,AreaList,1,true);
            require(app.item(CalibrateArea)&&shows(app,L"buscar status e receber destaque")&&app.area()->iconSize==57,
                "uso duplo em outro set perdeu medição salva ou foi ignorado");
            screenshot(app,pictures,L"ui-hud-uso-duplo.png");
            tab(app,0);choose(app,SetList,1);tab(app,1);choose(app,AreaList,1,true);
            require(app.item(CalibrateArea)&&shows(app,L"buscar status e receber destaque"),"uso da HUD depende apenas do set ativo");
            app.saveEditor();const auto saved=aa::loadWorkspace(app.workspacePath,{});
            require(saved.huds[1].areas[1].iconCalibrated&&saved.huds[1].areas[1].iconSize==57&&
                saved.sets[0].rules[0].targetArea==original.sets[0].rules[0].targetArea,"interface alterou medição ou regra salva");
            app.commit(original);app.selectedArea=0;app.makeUI();
        }
        screenshot(app,pictures,L"ui-huds.png");
        createNamed(app,HudName,L"Ultrawide");
        require(app.hud()->name==L"Ultrawide"&&text(app.item(HudList))==L"Ultrawide"&&shows(app,L"HUD ativa: Ultrawide"),"renomear deve atualizar seletor e cabeçalho juntos");
        require(aa::loadWorkspace(app.workspacePath,{}).activeHudId==app.workspace.activeHudId,"renomear mudou a identidade da HUD");
        const auto renamed=aa::loadWorkspace(app.workspacePath,{});
        require(std::any_of(renamed.huds.begin(),renamed.huds.end(),[&](const auto& h){return h.id==app.workspace.activeHudId&&h.name==L"Ultrawide";}),"nome renomeado não persistiu");
        createNamed(app,HudName,L"Cancelado",true);require(app.hud()->name==L"Ultrawide","cancelar renomeação alterou HUD");
        createNamed(app,HudName,L"   ",false,true);require(app.hud()->name==L"Ultrawide","nome vazio alterou HUD");
        tab(app,2);
        require(app.item(StatusName)&&!app.item(StatusKind)&&!app.item(AddPreset)&&!app.item(CaptureStack)&&!app.item(HudName)&&!app.item(RuleName),"Status exibe opcoes que pertencem a regra");
        SetWindowTextW(app.item(StatusName),L"");app.rebuildUIWithDraft();require(text(app.item(StatusName)).empty()&&app.selectedStatus()->name==L"Espírito Assassino","DPI perdeu rascunho ou gravou texto inválido");
        SetWindowTextW(app.item(StatusName),L"Carga da adaga");tab(app,3);require(app.workspace.statuses[0].name==L"Carga da adaga"&&app.rule()->statusId==L"s1","renomear status quebrou vínculo");
        require(app.item(RuleName)&&!app.item(StatusName)&&!app.item(HudName),"Regras misturam outros editores");
        require(SendMessageW(app.item(ConditionBox),CB_GETCURSEL,0,0)==2&&text(app.item(Stacks))==L"3","condição ou stack incorreto na edição");
        require(app.item(CaptureRuleStack),"regra de stacks nao oferece captura da propria amostra");
        screenshot(app,pictures,L"ui-regras.png");
        require(SendMessageW(app.item(EffectBox),CB_GETCOUNT,0,0)==4&&app.item(SampleColor),"efeitos e captura de cor ausentes");
        require(app.item(FollowClock)&&SendMessageW(app.item(FollowClock),BM_GETCHECK,0,0)==BST_UNCHECKED,"opcao de acompanhar relogio ausente ou ligada no legado");
        {
            SendMessageW(app.item(FollowClock),BM_CLICK,0,0);app.rebuildUIWithDraft();
            require(SendMessageW(app.item(FollowClock),BM_GETCHECK,0,0)==BST_CHECKED,"DPI perdeu opcao de relogio");
            app.saveEditor();require(aa::loadWorkspace(app.workspacePath,{}).sets[0].rules[0].followClock,"interface nao persistiu acompanhamento");
            screenshot(app,pictures,L"ui-regra-relogio.png");
            choose(app,ConditionBox,1);
            require(!IsWindowEnabled(app.item(FollowClock)),"ausencia permitiu mostrar tempo de status ausente");
            app.saveEditor();require(!app.rule()->followClock,"regra de ausencia manteve relogio");
            choose(app,ConditionBox,2);app.saveEditor();
        }
        {
            ShowWindow(app.window,SW_SHOWNOACTIVATE);SetActiveWindow(app.window);
            const auto checkbox=app.item(Enabled);SetFocus(checkbox);const auto checked=SendMessageW(checkbox,BM_GETCHECK,0,0);
            SendMessageW(checkbox,WM_KEYDOWN,VK_SPACE,0);SendMessageW(checkbox,WM_KEYUP,VK_SPACE,0);
            require(SendMessageW(checkbox,BM_GETCHECK,0,0)!=checked,"checkbox com tema não responde ao Espaço");
            SendMessageW(checkbox,WM_KEYDOWN,VK_SPACE,0);SendMessageW(checkbox,WM_KEYUP,VK_SPACE,0);
            const auto combo=app.item(EffectBox);SetFocus(combo);SendMessageW(combo,CB_SETCURSEL,0,0);
            SendMessageW(combo,CB_SHOWDROPDOWN,TRUE,0);SendMessageW(combo,WM_KEYDOWN,VK_DOWN,0);SendMessageW(combo,WM_KEYDOWN,VK_RETURN,0);
            require(SendMessageW(combo,CB_GETCURSEL,0,0)==1&&!SendMessageW(combo,CB_GETDROPPEDSTATE,0,0),"combo com tema não aceita escolha pelo teclado");
            ShowWindow(app.window,SW_HIDE);
        }
        for(int effect=0;effect<4;++effect){choose(app,EffectBox,effect);app.saveEditor();require(static_cast<int>(aa::loadWorkspace(app.workspacePath,{}).sets[0].rules[0].effect)==effect,"efeito da interface não persiste");}
        {
            const auto before=app.workspace;
            aa::Image colored{64,64,std::vector<std::uint8_t>(64*64*4,255)};
            for(std::size_t i=0;i<colored.bgra.size();i+=4){colored.bgra[i]=30;colored.bgra[i+1]=50;colored.bgra[i+2]=220;}
            require(app.applyActionColor(colored),"cor vermelha válida não foi aplicada");
            const auto capturedColor=app.rule()->condition.color;
            require(GetRValue(capturedColor)>GetBValue(capturedColor)&&
                    aa::loadWorkspace(app.workspacePath,{}).sets[0].rules[0].condition.color==capturedColor,
                    "cor capturada inverteu canais ou não foi salva");
            app.makeUI();app.saveEditor();require(app.rule()->condition.color==capturedColor,"editor substituiu cor capturada pela paleta");
            require(!app.applyActionColor({})&&app.rule()->condition.color==capturedColor,"imagem inválida apagou cor salva");
            std::fill(colored.bgra.begin(),colored.bgra.end(),std::uint8_t{100});
            require(!app.applyActionColor(colored)&&aa::loadWorkspace(app.workspacePath,{}).sets[0].rules[0].condition.color==capturedColor,
                    "imagem neutra alterou cor salva");
            app.hud()->areas.push_back({L"Habilidade Q",{410,420,70,50},48,false});app.makeUI();
            choose(app,TargetArea,2);ShowWindow(app.window,SW_SHOWNOACTIVATE);
            bool enteredCapture=false,captureFailed=false;
            try{app.sampleActionColor([&](HWND target,RECT region)->aa::Image{
                enteredCapture=true;const RECT expected{410,420,480,470};
                require(target==app.target&&EqualRect(&region,&expected),"captura de cor não usa o destino recém-selecionado");
                require(app.selecting&&!IsWindowVisible(app.window)&&!app.running,"captura não ocultou painel e pausou leitura");
                throw std::runtime_error("captura indisponível simulada");
            });}catch(const std::exception& failure){captureFailed=std::string(failure.what())=="captura indisponível simulada";}
            require(enteredCapture&&captureFailed&&!app.selecting&&IsWindowVisible(app.window)&&app.rule()->condition.color==capturedColor,
                    "falha após ocultar painel não restaurou janela ou cor anterior");
            require(app.rule()->targetArea==L"Habilidade Q"&&app.plan.captureArea.width==0,"captura pontual ampliou captura contínua ou perdeu destino salvo");
            ShowWindow(app.window,SW_HIDE);
            app.hud()->clientWidth=900;bool failed=false;
            try{app.sampleActionColor();}catch(const std::exception&){failed=true;}
            require(failed&&!app.selecting&&app.rule()->condition.color==capturedColor,"tela incompatível capturou ou perdeu cor anterior");
            app.commit(before);app.makeUI();
        }
        choose(app,RuleStatus,1);require(SendMessageW(app.item(Stacks),CB_GETCOUNT,0,0)==0,"status personalizado herdou contadores do exemplo");
        tab(app,2);require(app.set()->rules[0].statusId==L"s2"&&app.set()->rules[0].condition.stacks==3,"rascunho sem amostra impediu navegação ou mudou valor");
        require(!aa::readinessIssues(app.workspace).empty(),"regra sem referência ficou pronta");
        choose(app,StatusList,1,true);
        bool blocked=false;try{app.command(DeleteStatus,BN_CLICKED);}catch(const std::invalid_argument&){blocked=true;}require(blocked&&app.workspace.statuses.size()==2,"exclusão removeu status usado");
        app.error.clear();
        auto custom=app.workspace;custom.statuses[1].referencePath=app.storeImage(L"s2",aa::loadImageResource(IDR_ASSASSIN_NONE));
        const auto five=app.storeImage(L"s2",aa::loadImageResource(IDR_ASSASSIN_3));
        for(auto& profile:custom.sets)for(auto& configured:profile.rules)if(configured.statusId==L"s2")configured.stackSamples={{5,five}};
        app.commit(custom);app.makeUI();screenshot(app,pictures,L"ui-status.png");
        auto presenceReader=aa::MonitorReader{app.workspace.statuses[1],app.hud()->areas[0],false};
        const auto presenceOnly=makeRecognizer(presenceReader);
        const auto reference=aa::loadImageResource(IDR_ASSASSIN_NONE);
        require(presenceOnly->recognize(reference,reference.width).presence==aa::Presence::Present,"presença exigiu amostra de stacks ausente");
        {
            const auto cleanClock=aa::loadImageResource(IDR_ASSASSIN_CLOCK);
            require(app.item(CaptureClock)&&!presenceOnly->clockReady(),"controle do relogio ausente ou leitura nao solicitada foi ativada");
            const auto before=app.workspace;const auto statusBefore=*app.selectedStatus();
            bool invalid=false;try{app.applyClockReference({});}catch(const std::exception&){invalid=true;}
            require(invalid&&app.selectedStatus()->clockReferencePath.empty(),"referencia temporal invalida foi gravada");
            app.applyClockReference(cleanClock);
            require(!app.selectedStatus()->clockReferencePath.empty()&&app.selectedStatus()->referencePath==statusBefore.referencePath&&
                app.selectedStatus()->builtinAssassin==statusBefore.builtinAssassin&&app.selectedStatus()->stacks.size()==statusBefore.stacks.size()&&
                app.hud()->areas[0].region.x==before.huds[1].areas[0].region.x,"referencia temporal alterou identidade/stacks/HUD");
            auto clockReader=presenceReader;clockReader.needsClock=true;clockReader.status=*app.selectedStatus();
            require(makeRecognizer(clockReader)->clockReady(),"referencia temporal cadastrada nao carrega");
            clockReader.status.clockReferencePath=L"Z:\\relogio-ausente.png";
            auto missingClock=makeRecognizer(clockReader);
            require(!missingClock->clockReady()&&missingClock->recognize(reference,64).presence==aa::Presence::Present,
                "relogio ausente bloqueou reconhecimento existente");
            clockReader.status.builtinAssassin=true;
            require(!makeRecognizer(clockReader)->clockReady(),"caminho temporal explicito invalido caiu no preset");
            clockReader.status.clockReferencePath=app.storeImage(L"clock",cleanClock);
            {std::ofstream broken(std::filesystem::path(clockReader.status.clockReferencePath),std::ios::binary|std::ios::trunc);broken<<"invalid";}
            require(!makeRecognizer(clockReader)->clockReady(),"referencia corrompida nao desativou apenas o relogio");
            clockReader.status.clockReferencePath.clear();
            require(makeRecognizer(clockReader)->clockReady(),"relogio do exemplo nao carrega quando solicitado");
            clockReader.area.iconSize=38;
            const auto liveClock=aa::loadImage(std::filesystem::path(__FILE__).parent_path()/L"fixtures"/L"recognition-clock"/L"120-77561887.png");
            require(makeRecognizer(clockReader)->recognizeNearSize(liveClock,38).remainingFraction.has_value(),
                    "monitor nao selecionou referencia temporal nativa para HUD de 38/40 px");
            app.makeUI();screenshot(app,pictures,L"ui-status-relogio.png");
            app.commit(before);app.makeUI();
        }
        presenceReader.status.stacks={{5,L"Z:\\amostra-ausente-de-teste.png"}};presenceReader.needsStacks=true;blocked=false;
        try{(void)makeRecognizer(presenceReader);}catch(const std::exception&){blocked=true;}require(blocked,"contador sem amostra iniciou");
        presenceReader.needsStacks=false;presenceReader.status.referencePath=(app.directory/L"oversized.png").wstring();
        aa::saveImage({512,512,std::vector<std::uint8_t>(512*512*4,30)},presenceReader.status.referencePath);
        blocked=false;try{(void)makeRecognizer(presenceReader);}catch(const std::exception& error){blocked=std::string(error.what()).find("Veneno")!=std::string::npos;}
        require(blocked,"referência fora do limite não bloqueou início com nome do status");
        {
            // Dois símbolos distintos presentes na mesma ROI; remover um não deve apagar o outro.
            const auto food=aa::loadImage(std::filesystem::path(__FILE__).parent_path().parent_path()/L"assets"/L"other-food.png");
            aa::Workspace multi;multi.activeHudId=L"hud";multi.activeSetId=L"set";
            multi.huds={{L"hud",L"Teste visual",300,200,0,L"",{{L"Status",{0,0,160,80},64,true},{L"A",{0,100,40,40},48,false},{L"B",{60,100,40,40},48,false}}}};
            multi.statuses={{L"a",L"Carga",false,true,{},{}},{L"b",L"Comida",false,false,app.storeImage(L"food",food),{}}};
            auto first=rule;first.id=L"first";first.statusId=L"a";first.sourceArea=L"Status";first.targetArea=L"A";first.condition.condition=aa::Condition::Present;
            auto second=first;second.id=L"second";second.statusId=L"b";second.targetArea=L"B";second.condition.name=L"Comida presente";
            multi.sets={{L"set",L"Dois status",{first,second}}};const auto plan=aa::makeMonitorPlan(multi);
            const auto a=makeRecognizer(plan.readers[0]),b=makeRecognizer(plan.readers[1]);
            aa::Image roi{160,80,std::vector<std::uint8_t>(160*80*4,24)};
            const auto place=[&](const aa::Image& icon,int left){
                for(int y=0;y<64;++y)for(int x=0;x<64;++x){
                    const auto from=(static_cast<std::size_t>(y*icon.height/64)*icon.width+x*icon.width/64)*4;
                    std::copy_n(icon.bgra.data()+from,4,roi.bgra.data()+(static_cast<std::size_t>(y+8)*160+x+left)*4);
                }
            };
            place(reference,4);place(food,90);
            auto observe=[&]{return std::vector<aa::Observation>{{a->recognize(roi,64),1000,11},{b->recognize(roi,64),1000,11}};};
            auto observations=observe();require(aa::evaluateMonitor(plan,observations,1000,750,11)==std::vector<bool>({true,true}),"duas identidades distintas não acionaram suas ações");
            for(int y=0;y<80;++y)std::fill_n(roi.bgra.data()+static_cast<std::size_t>(y)*160*4,80*4,std::uint8_t{24});
            observations=observe();require(aa::evaluateMonitor(plan,observations,1000,750,11)==std::vector<bool>({false,true}),"retirar um status interferiu na leitura do outro");
        }
        tab(app,3);require(SendMessageW(app.item(Stacks),CB_GETCOUNT,0,0)==1&&SendMessageW(app.item(Stacks),CB_GETCURSEL,0,0)==CB_ERR,"amostra nova substituiu valor salvo silenciosamente");
        choose(app,Stacks,0);choose(app,EffectBox,1);app.saveEditor();require(app.rule()->condition.stacks==5&&app.rule()->effect==aa::OverlayEffect::Glow,"regra não salvou contador genérico e brilho");
        choose(app,ConditionBox,0);app.saveEditor();require(app.rule()->condition.condition==aa::Condition::Present&&!IsWindowEnabled(app.item(Stacks)),"presença usa contador");
        choose(app,ConditionBox,1);app.saveEditor();require(app.rule()->condition.condition==aa::Condition::Absent,"ausência mapeada incorretamente");
        createNamed(app,NewRule,L"Segunda regra");createNamed(app,NewRule,L"Terceira regra");require(app.set()->rules.size()==3&&app.set()->rules[1].condition.name!=app.set()->rules[2].condition.name,"novas regras colidem");
        app.command(MoveRuleUp,BN_CLICKED);require(app.selectedRule==1,"prioridade não mudou");app.command(DeleteRule,BN_CLICKED);require(app.set()->rules.size()==2,"regra não removida");

        tab(app,1);createNamed(app,NewHud,L"Outra tela");require(app.hud()->areas.empty()&&app.hud()->clientWidth==0&&app.workspace.statuses.size()==2&&app.set()->rules.size()==2,"nova HUD copiou tela ou alterou biblioteca/set");
        const auto created=app.workspace.activeHudId;confirmDeleteHud(app,IDNO);require(app.workspace.activeHudId==created&&app.workspace.huds.size()==3,"cancelamento excluiu HUD");
        confirmDeleteHud(app,IDYES);require(app.workspace.huds.size()==2&&app.workspace.activeHudId!=created&&app.workspace.statuses.size()==2&&app.set()->rules.size()==2,"excluir HUD alterou biblioteca/set");
        require(app.item(HudList)!=nullptr,"HUDs restantes precisam continuar acessíveis após excluir a ativa");
        choose(app,HudList,0);tab(app,3);app.selectedRule=0;app.makeUI();
        const auto screen=screenOf(app.target);app.hud()->monitorDpi=screen.dpi;app.hud()->monitorDevice=screen.device;
        app.hud()->areas[1].region.shape=aa::RegionShape::Circle;
        choose(app,ConditionBox,0);SendMessageW(app.item(FollowClock),BM_SETCHECK,BST_CHECKED,0);
        app.current={{{aa::Presence::Present,3,1.0f,{},{}},static_cast<std::int64_t>(GetTickCount64()),app.source}};
        app.latest=app.current;app.pending=true;app.running=true;const auto oldSource=app.source;
        app.testAction();require(!app.running&&app.source>oldSource&&!app.pending&&app.current.empty()&&app.latest.empty(),"teste visual preservou leitura anterior");
        require(app.previewTarget.shape==aa::RegionShape::Circle,"teste de destaque perdeu a forma circular do destino");
        require(app.previewClock&&shows(app,L"Simulação de 5 segundos"),"teste do aro nao foi identificado como simulacao");
        require(app.previewUntil>GetTickCount64()&&app.previewUntil<=GetTickCount64()+5000,"teste visual sem limite de cinco segundos");
        app.previewUntil=GetTickCount64()-1;app.updateHighlight();require(app.previewUntil==0&&!app.running,"teste expirado reiniciou leitura");
        app.previewUntil=GetTickCount64()+5000;app.stop();require(app.previewUntil==0,"Parar não encerrou teste visual");

        auto emptyHud=app.workspace;while(!emptyHud.huds.empty())aa::eraseHud(emptyHud,emptyHud.huds.front().id);app.commit(emptyHud);app.load();
        require(app.workspace.huds.empty()&&app.workspace.activeHudId.empty()&&app.workspace.sets.size()==2,"última HUD reapareceu ou excluiu sets");
        {
            TestApp imported;imported.configureStorage(folder/L"legacy"/L"settings.ini");
            aa::Settings legacy;legacy.referencePath=app.workspace.statuses[1].referencePath;legacy.rule.stacks=3;
            aa::saveSettings(imported.settingsPath.wstring(),legacy);imported.load();
            require(imported.workspace.statuses.size()==1&&!imported.workspace.statuses[0].builtinAssassin&&imported.workspace.statuses[0].stacks.empty()&&imported.workspace.sets[0].rules[0].stackSamples.size()==2,"migração não moveu amostras para regra");
            imported.load();require(imported.workspace.sets[0].rules[0].stackSamples.size()==2,"migração repetida duplicou amostras da regra");
        }
        std::cout<<"Fluxos de quatro páginas, CRUD, cancelamento, persistência, contadores genéricos, DPI e teste temporário aprovados; sem teste em jogo.\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
