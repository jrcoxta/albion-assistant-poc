// Exercita estado e persistência em janelas do próprio harness, sem usar o jogo.
// Não valida aparência, foco real ou visibilidade da borda; estes exigem desktop desbloqueado.
#include "../src/main.cpp"
#include <iostream>

namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
struct TestApp : App {
    ~TestApp() {
        stop();
        if (badge) DestroyWindow(badge);
        if (window) DestroyWindow(window);
        if (target) DestroyWindow(target);
        if (font) DeleteObject(font);
        if (titleFont) DeleteObject(titleFont);
        std::error_code ignored;
        const auto resolved=std::filesystem::weakly_canonical(directory,ignored);
        const auto temporary=std::filesystem::weakly_canonical(std::filesystem::temp_directory_path(),ignored);
        if(!ignored&&resolved.parent_path()==temporary&&resolved.filename().wstring().starts_with(L"albion-app-flow-"))
            std::filesystem::remove_all(resolved,ignored);
    }
};
}

int main() {
    try {
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        TestApp app;
        app.instance=GetModuleHandleW(nullptr);
        app.directory=std::filesystem::temp_directory_path()/
            (L"albion-app-flow-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64()));
        std::filesystem::create_directories(app.directory);
        app.settingsPath=app.directory/L"settings.ini";
        app.profilesDir=app.directory/L"hud-profiles";
        app.settings.hudName=L"Notebook";
        app.settings.referencePath=L"referencia-de-teste.png";
        app.settings.rule.color=RGB(40,255,120);
        app.settings.rule.stacks=2;
        app.persist();
        auto monitor=app.settings;monitor.hudName=L"Monitor 34";monitor.rule.stacks=3;
        aa::saveHudProfile(app.profilesDir,monitor);

        app.window=CreateWindowExW(0,L"STATIC",L"Harness do painel",WS_OVERLAPPED|WS_CAPTION,
            20,20,880,740,nullptr,nullptr,app.instance,nullptr);
        app.target=CreateWindowExW(0,L"STATIC",L"Alvo simulado",WS_POPUP,
            0,0,800,600,nullptr,nullptr,app.instance,nullptr);
        require(app.window&&app.target,"janelas do harness não criadas");
        app.overlay.initialize(app.instance);
        app.makeUI();

        SetWindowTextW(app.item(Stacks),L"");
        SendMessageW(app.item(Color),CB_SETCURSEL,3,0);
        app.rebuildUIWithDraft();
        require(text(app.item(Stacks)).empty()&&SendMessageW(app.item(Color),CB_GETCURSEL,0,0)==3,
            "reconstrução por DPI validou ou descartou edição incompleta");
        SendMessageW(app.item(ConditionBox),CB_SETCURSEL,static_cast<WPARAM>(aa::Condition::Present),0);
        app.readEditor();
        require(app.settings.rule.condition==aa::Condition::Present,"presença tentou validar contador desabilitado");
        SendMessageW(app.item(ConditionBox),CB_SETCURSEL,static_cast<WPARAM>(aa::Condition::StacksEqual),0);
        SetWindowTextW(app.item(Stacks),L"2");

        SetWindowTextW(app.item(HudName),L"Monitor 34");
        SetWindowTextW(app.item(Stacks),L"2");
        for(std::size_t i=0;i<app.profiles.size();++i)
            if(app.profiles[i].settings.hudName==L"Monitor 34")
                SendMessageW(app.item(HudList),CB_SETCURSEL,i,0);
        app.loadHud();
        require(app.settings.rule.stacks==3,"Carregar salvou edições sobre a HUD escolhida");
        require(aa::loadSettings((app.profilesDir/L"Notebook.ini").wstring()).rule.stacks==2,
            "Carregar alterou outra HUD");

        SetWindowTextW(app.item(HudName),L"Notebook");
        app.readEditor();
        bool rejected=false;
        try { app.persist(); } catch(const std::invalid_argument&) { rejected=true; }
        require(rejected,"renomear sobrescreveu uma HUD alheia");
        app.newHud();
        require(app.settings.referencePath==L"referencia-de-teste.png"&&app.settings.rule.stacks==3,
            "Nova HUD perdeu a ação ou referência salva");
        require(!app.settings.buffs.valid()&&!app.settings.highlight.valid()&&!app.settings.iconCalibrated,
            "Nova HUD reutilizou coordenadas anteriores");

        const auto screen=screenOf(app.target);
        app.settings.clientWidth=screen.width;app.settings.clientHeight=screen.height;
        app.settings.monitorDevice=screen.device;app.settings.monitorDpi=screen.dpi;
        app.settings.highlight={20,20,80,80};
        app.current={{aa::Presence::Present,3,1.0f,{},{}},static_cast<std::int64_t>(GetTickCount64()),app.source};
        app.latest=app.current;app.pending=true;app.running=true;
        const auto oldSource=app.source;
        app.testHighlight();
        require(!app.running&&app.source>oldSource&&!app.pending,
            "teste visual não encerrou a sessão de leitura");
        require(app.current.detection.presence==aa::Presence::Unknown&&app.latest.source==0,
            "teste visual deixou uma observação anterior ou sintética");
        require(app.previewUntil>GetTickCount64()&&app.previewUntil<=GetTickCount64()+5000,
            "demonstração não está limitada a cinco segundos");
        app.previewUntil=GetTickCount64()-1;
        app.updateHighlight();
        require(app.previewUntil==0&&!app.running&&!app.lit,
            "demonstração vencida não encerrou ou iniciou leitura implicitamente");
        app.previewUntil=GetTickCount64()+5000;
        app.stop();
        require(app.previewUntil==0,"Parar não cancelou demonstração");
        std::cout << "Estado dos fluxos de carregar, nova HUD, DPI, colisão de nome e teste do destaque aprovado (sem validação visual)\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
