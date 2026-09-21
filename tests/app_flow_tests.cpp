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
void tab(App& app,int page){app.command(Tab0+page,BN_CLICKED);require(app.page==page,"navegação não mudou de página");}
void choose(App& app,int id,int index,bool list=false){SendMessageW(app.item(id),list?LB_SETCURSEL:CB_SETCURSEL,index,0);app.command(id,list?LBN_SELCHANGE:CBN_SELCHANGE);}
bool shows(App& app,const wchar_t* phrase){return std::any_of(app.controls.begin(),app.controls.end(),[&](HWND control){return text(control).find(phrase)!=std::wstring::npos;});}
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
        w.sets={{L"set1",L"Adagas",{rule}},{L"set2",L"Cajado",{}}};w.activeSetId=L"set1";
        app.commit(w);app.selectedStatusId=L"s1";
        WNDCLASSW cls{};cls.hInstance=app.instance;cls.lpfnWndProc=testProc;cls.lpszClassName=L"AlbionAppFlowTests";cls.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_BTNFACE+1);RegisterClassW(&cls);
        app.window=CreateWindowExW(0,cls.lpszClassName,L"Validação do painel",WS_OVERLAPPED|WS_CAPTION,20,20,880,740,nullptr,nullptr,app.instance,&app);
        app.target=CreateWindowExW(0,L"STATIC",L"Alvo do teste",WS_POPUP,0,0,800,600,nullptr,nullptr,app.instance,nullptr);
        require(app.window&&app.target,"janelas de teste não criadas");app.makeUI();
        require(!IsWindowVisible(app.window)&&!app.rebuilding,"construção mostrou janela originalmente oculta ou deixou bloqueio ativo");
        {
            const auto start=app.item(Start);const auto source=app.source;
            app.running=true;tab(app,0);choose(app,HudList,0);choose(app,SetList,0);
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
        screenshot(app,pictures,L"ui-monitor.png");
        choose(app,HudList,1);require(app.workspace.activeHudId==L"h2"&&app.workspace.activeSetId==L"set1"&&app.set()->rules[0].condition.stacks==3,"troca de HUD alterou o set");
        choose(app,SetList,1);require(app.workspace.activeHudId==L"h2"&&app.workspace.activeSetId==L"set2","troca de set alterou a HUD");choose(app,SetList,0);

        app.hud()->areas[0].iconCalibrated=false;
        tab(app,2);app.start();
        require(app.page==0&&!app.running&&app.recognizers.empty()&&app.overlays.empty(),"origem sem calibração iniciou leitura");
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

        tab(app,1);require(app.item(DeleteHud)&&app.item(AreaName)&&!app.item(RuleName)&&!app.item(CaptureStatus),"HUD mistura regras ou status");
        {
            const auto original=app.workspace;
            app.hud()->areas.push_back({L"Q",{380,400,64,64},48,false});
            choose(app,AreaList,1,true);
            require(!app.item(CalibrateArea)&&shows(app,L"Não precisa medir ícone"),"destino E pede medição de ícone");
            require(aa::readinessIssues(app.workspace).empty(),"destino não medido bloqueia a regra");
            screenshot(app,pictures,L"ui-hud-destino.png");
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
        SetWindowTextW(app.item(HudName),L"Ultrawide");tab(app,2);require(app.hud()->name==L"Ultrawide","navegação perdeu nome da HUD");
        require(app.item(StatusName)&&app.item(CaptureStack)&&!app.item(HudName)&&!app.item(RuleName),"Status mistura outros editores");
        SetWindowTextW(app.item(StatusName),L"");app.rebuildUIWithDraft();require(text(app.item(StatusName)).empty()&&app.selectedStatus()->name==L"Espírito Assassino","DPI perdeu rascunho ou gravou texto inválido");
        SetWindowTextW(app.item(StatusName),L"Carga da adaga");tab(app,3);require(app.workspace.statuses[0].name==L"Carga da adaga"&&app.rule()->statusId==L"s1","renomear status quebrou vínculo");
        require(app.item(RuleName)&&!app.item(StatusName)&&!app.item(HudName),"Regras misturam outros editores");
        require(SendMessageW(app.item(ConditionBox),CB_GETCURSEL,0,0)==2&&text(app.item(Stacks))==L"3","condição ou stack incorreto na edição");
        screenshot(app,pictures,L"ui-regras.png");
        require(SendMessageW(app.item(EffectBox),CB_GETCOUNT,0,0)==4&&app.item(SampleColor),"efeitos e captura de cor ausentes");
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
        custom.statuses[1].stacks={{5,app.storeImage(L"s2",aa::loadImageResource(IDR_ASSASSIN_3))}};
        app.commit(custom);app.makeUI();screenshot(app,pictures,L"ui-status.png");
        auto presenceReader=aa::MonitorReader{app.workspace.statuses[1],app.hud()->areas[0],false};
        presenceReader.status.stacks[0].path=L"Z:\\amostra-ausente-de-teste.png";
        const auto presenceOnly=makeRecognizer(presenceReader);
        const auto reference=aa::loadImageResource(IDR_ASSASSIN_NONE);
        require(presenceOnly->recognize(reference,reference.width).presence==aa::Presence::Present,"presença exigiu amostra de stacks ausente");
        presenceReader.needsStacks=true;blocked=false;
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
        app.command(NewRule,BN_CLICKED);app.command(NewRule,BN_CLICKED);require(app.set()->rules.size()==3&&app.set()->rules[1].condition.name!=app.set()->rules[2].condition.name,"novas regras colidem");
        app.command(MoveRuleUp,BN_CLICKED);require(app.selectedRule==1,"prioridade não mudou");app.command(DeleteRule,BN_CLICKED);require(app.set()->rules.size()==2,"regra não removida");

        tab(app,1);app.command(NewHud,BN_CLICKED);require(app.hud()->areas.empty()&&app.hud()->clientWidth==0&&app.workspace.statuses.size()==2&&app.set()->rules.size()==2,"nova HUD copiou tela ou alterou biblioteca/set");
        const auto created=app.workspace.activeHudId;confirmDeleteHud(app,IDNO);require(app.workspace.activeHudId==created&&app.workspace.huds.size()==3,"cancelamento excluiu HUD");
        confirmDeleteHud(app,IDYES);require(app.workspace.huds.size()==2&&app.workspace.activeHudId!=created&&app.workspace.statuses.size()==2&&app.set()->rules.size()==2,"excluir HUD alterou biblioteca/set");
        choose(app,HudList,0);tab(app,3);app.selectedRule=0;app.makeUI();
        const auto screen=screenOf(app.target);app.hud()->monitorDpi=screen.dpi;app.hud()->monitorDevice=screen.device;
        app.current={{{aa::Presence::Present,3,1.0f,{},{}},static_cast<std::int64_t>(GetTickCount64()),app.source}};
        app.latest=app.current;app.pending=true;app.running=true;const auto oldSource=app.source;
        app.testAction();require(!app.running&&app.source>oldSource&&!app.pending&&app.current.empty()&&app.latest.empty(),"teste visual preservou leitura anterior");
        require(app.previewUntil>GetTickCount64()&&app.previewUntil<=GetTickCount64()+5000,"teste visual sem limite de cinco segundos");
        app.previewUntil=GetTickCount64()-1;app.updateHighlight();require(app.previewUntil==0&&!app.running,"teste expirado reiniciou leitura");
        app.previewUntil=GetTickCount64()+5000;app.stop();require(app.previewUntil==0,"Parar não encerrou teste visual");

        auto emptyHud=app.workspace;while(!emptyHud.huds.empty())aa::eraseHud(emptyHud,emptyHud.huds.front().id);app.commit(emptyHud);app.load();
        require(app.workspace.huds.empty()&&app.workspace.activeHudId.empty()&&app.workspace.sets.size()==2,"última HUD reapareceu ou excluiu sets");
        {
            TestApp imported;imported.configureStorage(folder/L"legacy"/L"settings.ini");
            aa::Settings legacy;legacy.referencePath=app.workspace.statuses[1].referencePath;legacy.rule.stacks=3;
            aa::saveSettings(imported.settingsPath.wstring(),legacy);imported.load();
            require(imported.workspace.statuses.size()==1&&!imported.workspace.statuses[0].builtinAssassin&&aa::stackValues(imported.workspace.statuses[0])==std::vector<unsigned>({2,3}),"migração não materializou amostras de contadores legados");
            for(const auto& sample:imported.workspace.statuses[0].stacks)require(aa::loadImage(sample.path).valid(),"amostra legada não gravada");
            imported.load();require(imported.workspace.statuses[0].stacks.size()==2,"migração repetida duplicou amostras");
        }
        std::cout<<"Fluxos de quatro páginas, CRUD, cancelamento, persistência, contadores genéricos, DPI e teste temporário aprovados; sem teste em jogo.\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
