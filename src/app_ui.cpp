#include "app.h"
#include "panel_layout.h"
#include "theme.h"
#include "../resources/resource.h"
#include <commdlg.h>
#include <algorithm>
#include <iterator>
#include <stdexcept>

namespace aaapp {
namespace {
enum {ActiveNames=600, MonitorSummary, RulePhrase, ActiveSetName};
constexpr COLORREF Colors[]={RGB(255,191,0),RGB(40,255,120),RGB(60,180,255),RGB(240,80,255)};
const wchar_t* ColorNames[]={L"Dourado",L"Verde",L"Azul",L"Magenta"};
const wchar_t* EffectNames[]={L"Borda",L"Brilho",L"Pulso",L"Halo"};

struct RebuildScope {
    App& app;bool previous;
    theme::RedrawLock redraw;
    explicit RebuildScope(App& value):app(value),previous(value.rebuilding),redraw(value.window){
        app.rebuilding=true;
    }
    ~RebuildScope(){app.rebuilding=previous;}
};

int selection(HWND control, bool list=false) {
    return static_cast<int>(SendMessageW(control,list?LB_GETCURSEL:CB_GETCURSEL,0,0));
}
void choose(HWND control,int index,bool list=false) {
    SendMessageW(control,list?LB_SETCURSEL:CB_SETCURSEL,static_cast<WPARAM>(index),0);
}
int add(HWND control,const std::wstring& value,bool list=false) {
    return static_cast<int>(SendMessageW(control,list?LB_ADDSTRING:CB_ADDSTRING,0,reinterpret_cast<LPARAM>(value.c_str())));
}
void selectText(HWND control,const std::wstring& value) {
    choose(control,static_cast<int>(SendMessageW(control,CB_FINDSTRINGEXACT,static_cast<WPARAM>(-1),reinterpret_cast<LPARAM>(value.c_str()))));
}
template<class Collection> auto* byId(Collection& collection,const std::wstring& id) {
    auto found=std::find_if(collection.begin(),collection.end(),[&](const auto& value){return value.id==id;});
    return found==collection.end()?nullptr:&*found;
}
template<class Collection> int indexOf(const Collection& collection,const std::wstring& id) {
    for(std::size_t i=0;i<collection.size();++i)if(collection[i].id==id)return static_cast<int>(i);
    return -1;
}
template<class Collection> std::wstring uniqueName(const Collection& collection,const std::wstring& base) {
    auto candidate=base;
    for(unsigned suffix=2;std::any_of(collection.begin(),collection.end(),[&](const auto& entry){return aa::sameName(entry.name,candidate);});++suffix)
        candidate=base+L" "+std::to_wstring(suffix);
    return candidate;
}
std::wstring nameFrom(HWND control) {
    auto name=text(control);
    const auto begin=name.find_first_not_of(L" \t\r\n"),end=name.find_last_not_of(L" \t\r\n");
    if(begin==std::wstring::npos)throw std::runtime_error("Informe um nome antes de salvar ou trocar de seção.");
    name=name.substr(begin,end-begin+1);
    if(name.size()>251)throw std::runtime_error("Use um nome com no máximo 251 caracteres.");
    return name;
}
template<class Collection> void checkName(const Collection& collection,const std::wstring& id,const std::wstring& name) {
    if(std::any_of(collection.begin(),collection.end(),[&](const auto& value){return value.id!=id&&aa::sameName(value.name,name);}))
        throw std::runtime_error("Esse nome já está em uso. Escolha outro nome.");
}
bool matches(const aa::HudLayout& hud,const Screen& screen) {
    return hud.clientWidth==screen.width&&hud.clientHeight==screen.height&&
        (!hud.monitorDpi||!screen.dpi||hud.monitorDpi==screen.dpi)&&
        (hud.monitorDevice.empty()||screen.device.empty()||hud.monitorDevice==screen.device);
}
void confirmGeometry(const aa::HudLayout& hud,const Screen& screen) {
    if(hud.clientWidth>0&&!matches(hud,screen))
        throw std::runtime_error("Esta HUD pertence a outra tela ou escala. Crie uma nova HUD para preservar as áreas já salvas.");
}
aa::Condition conditionFrom(int index) {
    return index==2?aa::Condition::StacksEqual:index==1?aa::Condition::Absent:aa::Condition::Present;
}
int conditionIndex(aa::Condition condition) {
    return condition==aa::Condition::StacksEqual?2:condition==aa::Condition::Absent?1:0;
}
struct AreaUsage {bool read=false,highlight=false;};
AreaUsage areaUsage(const aa::Workspace& workspace,const std::wstring& name) {
    AreaUsage usage;
    for(const auto& set:workspace.sets)for(const auto& rule:set.rules) {
        usage.read|=aa::sameName(rule.sourceArea,name);
        usage.highlight|=aa::sameName(rule.targetArea,name);
    }
    return usage;
}
std::wstring regionText(const aa::HudArea& area,AreaUsage usage) {
    if(!area.region.valid())return L"Área ainda não selecionada. Escolha Retângulo ou Círculo no seletor.";
    auto value=area.region.shape==aa::RegionShape::Circle?
        L"Área circular salva: "+std::to_wstring(area.region.width)+L" px de diâmetro.\n":
        L"Área retangular salva: "+std::to_wstring(area.region.width)+L" × "+std::to_wstring(area.region.height)+L" px.\n";
    if(usage.highlight&&!usage.read)return value+L"Uso: receber destaque.\nNão precisa medir ícone. A área define o destaque.";
    value+=usage.read?(usage.highlight?L"Uso: buscar status e receber destaque.\n":L"Uso: buscar status.\n"):
        L"Sem uso nas regras. Área pronta para destaque.\n";
    if(area.iconCalibrated)value+=L"Ícone para busca de status: "+std::to_wstring(area.iconSize)+L" px.";
    else if(usage.read)value+=usage.highlight?L"Destaque pronto. Meça o ícone apenas para a busca.":L"Meça um ícone de status dentro desta área.";
    else value+=L"Medição opcional, apenas para buscar status.";
    return value;
}
void setIfChanged(HWND control,const std::wstring& value) {
    if(control&&text(control)!=value)SetWindowTextW(control,value.c_str());
}
bool createsItem(int id){return id==NewHud||id==NewArea||id==NewStatus||id==NewSet||id==NewRule;}
void validateNewName(App& app,int id,const std::wstring& name){
    aa::Workspace values;
    try{values=app.editorValues();}
    catch(const std::exception& error){throw std::runtime_error(std::string("Cancele este cadastro e corrija a edição anterior: ")+error.what());}
    if(id==NewHud||id==HudName)checkName(values.huds,id==HudName?values.activeHudId:L"",name);
    else if(id==NewStatus)checkName(values.statuses,L"",name);
    else if(id==NewSet)checkName(values.sets,L"",name);
    else if(id==NewArea){
        const auto hud=byId(values.huds,values.activeHudId);
        if(!hud)throw std::runtime_error("Crie uma HUD antes de adicionar áreas.");
        for(const auto& area:hud->areas)if(aa::sameName(area.name,name))throw std::runtime_error("Já existe uma área com esse nome nesta HUD.");
    }else{
        const auto set=byId(values.sets,values.activeSetId);
        if(!set)throw std::runtime_error("Crie um set antes de adicionar regras.");
        for(const auto& rule:set->rules)if(aa::sameName(rule.condition.name,name))throw std::runtime_error("Já existe uma regra com esse nome neste set.");
    }
}
struct NamePrompt{App& app;int command;const wchar_t* title;const wchar_t* hint;std::wstring name;};
INT_PTR CALLBACK namePromptProc(HWND dialog,UINT message,WPARAM wp,LPARAM lp){
    auto* prompt=reinterpret_cast<NamePrompt*>(GetWindowLongPtrW(dialog,DWLP_USER));
    try{
        if(message==WM_INITDIALOG){
            prompt=reinterpret_cast<NamePrompt*>(lp);SetWindowLongPtrW(dialog,DWLP_USER,lp);
            auto& app=prompt->app;theme::attachWindow(dialog);SetWindowTextW(dialog,prompt->title);
            RECT bounds{0,0,app.px(480),app.px(248)};
            AdjustWindowRectExForDpi(&bounds,static_cast<DWORD>(GetWindowLongPtrW(dialog,GWL_STYLE)),FALSE,0,GetDpiForWindow(dialog));
            RECT parent{};GetWindowRect(app.window,&parent);
            const int width=bounds.right-bounds.left,height=bounds.bottom-bounds.top;
            SetWindowPos(dialog,nullptr,parent.left+(parent.right-parent.left-width)/2,parent.top+(parent.bottom-parent.top-height)/2,width,height,SWP_NOZORDER);
            const auto control=[&](const wchar_t* type,const wchar_t* title,DWORD style,int x,int y,int w,int h,int id){
                auto field=CreateWindowExW(0,type,title,WS_CHILD|WS_VISIBLE|style,app.px(x),app.px(y),app.px(w),app.px(h),dialog,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),app.instance,nullptr);
                if(!field)throw std::runtime_error("Não foi possível abrir o cadastro.");
                SendMessageW(field,WM_SETFONT,reinterpret_cast<WPARAM>(app.font),FALSE);theme::styleControl(field,id==IDOK?theme::Role::Primary:theme::Role::Normal);return field;
            };
            control(L"STATIC",L"Nome",0,24,20,432,22,0);
            const auto name=control(L"EDIT",prompt->name.c_str(),WS_TABSTOP|WS_BORDER|ES_AUTOHSCROLL,24,46,432,30,710);
            SendMessageW(name,EM_SETLIMITTEXT,251,0);
            control(L"STATIC",prompt->hint,0,24,89,432,44,0);
            control(L"STATIC",L"",0,24,140,432,44,711);
            control(L"BUTTON",prompt->command==HudName?L"Salvar":L"Criar",WS_TABSTOP|BS_DEFPUSHBUTTON,228,196,108,32,IDOK);
            control(L"BUTTON",L"Cancelar",WS_TABSTOP|BS_PUSHBUTTON,348,196,108,32,IDCANCEL);
            SetFocus(name);SendMessageW(name,EM_SETSEL,0,-1);return FALSE;
        }
        if(message==WM_PAINT){PAINTSTRUCT paint{};auto dc=BeginPaint(dialog,&paint);RECT bounds{};GetClientRect(dialog,&bounds);theme::fill(dc,bounds,theme::Background);EndPaint(dialog,&paint);return TRUE;}
        if(message==WM_CLOSE){EndDialog(dialog,IDCANCEL);return TRUE;}
        if(message==WM_COMMAND&&prompt){
            if(LOWORD(wp)==IDCANCEL){EndDialog(dialog,IDCANCEL);return TRUE;}
            if(LOWORD(wp)==IDOK){
                auto name=nameFrom(GetDlgItem(dialog,710));validateNewName(prompt->app,prompt->command,name);
                prompt->name=std::move(name);EndDialog(dialog,IDOK);return TRUE;
            }
            if(LOWORD(wp)==710&&HIWORD(wp)==EN_CHANGE){SetDlgItemTextW(dialog,711,L"");return TRUE;}
        }
    }catch(const std::exception& error){SetDlgItemTextW(dialog,711,widen(error.what()).c_str());SetFocus(GetDlgItem(dialog,710));return TRUE;}
    return FALSE;
}
std::optional<std::wstring> requestName(App& app,int id){
    NamePrompt prompt{app,id,L"Nova regra",L"Depois de criar, escolha o status, a condição e o destaque.",{}};
    if(id==NewHud){prompt.title=L"Nova HUD";prompt.hint=L"Use um nome para esta tela ou layout, como Monitor 34.\nDepois, selecione as áreas do jogo.";}
    else if(id==HudName){prompt.title=L"Renomear HUD";prompt.hint=L"Altere o nome desta HUD. Suas áreas e regras serão mantidas.";prompt.name=app.hud()->name;}
    else if(id==NewArea){prompt.title=L"Nova área";prompt.hint=L"Use um nome como Meus status ou E.\nDepois, selecione onde essa área fica no jogo.";}
    else if(id==NewStatus){prompt.title=L"Novo status";prompt.hint=L"Dê um nome ao buff ou debuff.\nDepois, capture ou importe sua imagem.";}
    else if(id==NewSet){prompt.title=L"Novo perfil";prompt.hint=L"Use o nome da arma ou do conjunto que está usando.\nDepois, adicione suas regras.";}
    struct alignas(DWORD) Template{DLGTEMPLATE dialog{};WORD menu=0,windowClass=0,title=0;} layout;
    layout.dialog.style=WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME;layout.dialog.cx=240;layout.dialog.cy=150;
    struct SelectionGuard{bool& selecting;bool previous;~SelectionGuard(){selecting=previous;}} guard{app.selecting,app.selecting};app.selecting=true;
    const auto result=DialogBoxIndirectParamW(app.instance,&layout.dialog,app.window,namePromptProc,reinterpret_cast<LPARAM>(&prompt));
    if(result==-1)throw std::runtime_error("Não foi possível abrir o cadastro.");
    return result==IDOK?std::optional<std::wstring>{std::move(prompt.name)}:std::nullopt;
}
}

int App::px(int value) const {return static_cast<int>(value*dpi);}
HWND App::item(int id) const {return GetDlgItem(window,id);}
HWND App::control(const wchar_t* type,const wchar_t* value,DWORD style,int x,int y,int w,int h,int id) {
    auto child=CreateWindowExW(0,type,value,WS_CHILD|WS_VISIBLE|style,px(x),px(y),px(w),px(h),window,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),instance,nullptr);
    if(!child)throw std::runtime_error("Não foi possível abrir os controles do painel.");
    SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(font),FALSE);
    theme::styleControl(child,_wcsicmp(type,L"STATIC")==0?theme::Role::Muted:id==Color?theme::Role::ColorChoice:theme::Role::Normal);
    if(_wcsicmp(type,L"COMBOBOX")==0){SendMessageW(child,CB_SETITEMHEIGHT,static_cast<WPARAM>(-1),px(22));SendMessageW(child,CB_SETITEMHEIGHT,0,px(28));}
    if(_wcsicmp(type,L"LISTBOX")==0)SendMessageW(child,LB_SETITEMHEIGHT,0,px(32));
    controls.push_back(child);return child;
}
void App::label(const wchar_t* value,int x,int y,int w,int h) {control(L"STATIC",value,0,x,y,w,h);}
void App::button(const wchar_t* value,int id,int x,int y,int w) {
    auto child=control(L"BUTTON",value,WS_TABSTOP|BS_PUSHBUTTON,x,y,w,30,id);
    if(id==Save||id==Start)theme::styleControl(child,theme::Role::Primary);
}
void App::edit(const wchar_t* value,int id,int x,int y,int w) {
    auto field=control(L"EDIT",value,WS_TABSTOP|WS_BORDER|ES_AUTOHSCROLL,x,y,w,26,id);
    SendMessageW(field,EM_SETLIMITTEXT,251,0);
}
int App::number(int id,int minimum,int maximum) {
    try {
        auto value=text(item(id));std::size_t end=0;const auto parsed=std::stoi(value,&end);
        if(end!=value.size()||parsed<minimum||parsed>maximum)throw std::out_of_range("intervalo");
        return parsed;
    }catch(const std::exception&) {
        throw std::runtime_error("Informe um número entre "+std::to_string(minimum)+" e "+std::to_string(maximum)+".");
    }
}

void App::makeUI() {
    theme::attachWindow(window);RebuildScope rebuild(*this);
    for(auto child:controls)DestroyWindow(child);
    controls.clear();statusLabel=nullptr;
    if(font)DeleteObject(font);if(titleFont)DeleteObject(titleFont);
    const auto windowDpi=GetDpiForWindow(window);dpi=windowDpi/96.0;
    MONITORINFO monitor{};monitor.cbSize=sizeof(monitor);
    if(GetMonitorInfoW(MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),&monitor)) {
        RECT frame{};const auto style=static_cast<DWORD>(GetWindowLongPtrW(window,GWL_STYLE));
        AdjustWindowRectExForDpi(&frame,style,FALSE,0,windowDpi);
        const int availableWidth=monitor.rcWork.right-monitor.rcWork.left-(frame.right-frame.left);
        const int availableHeight=monitor.rcWork.bottom-monitor.rcWork.top-(frame.bottom-frame.top);
        dpi=aa::panelScale(availableWidth,availableHeight,dpi);
        const int width=px(860)+frame.right-frame.left,height=px(700)+frame.bottom-frame.top;
        RECT old{};GetWindowRect(window,&old);
        const int x=std::clamp(static_cast<int>(old.left),static_cast<int>(monitor.rcWork.left),static_cast<int>(monitor.rcWork.right)-width);
        const int y=std::clamp(static_cast<int>(old.top),static_cast<int>(monitor.rcWork.top),static_cast<int>(monitor.rcWork.bottom)-height);
        SetWindowPos(window,nullptr,x,y,width,height,SWP_NOZORDER|SWP_NOACTIVATE);
    }
    font=CreateFontW(-px(15),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    titleFont=CreateFontW(-px(23),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    auto title=control(L"STATIC",L"ALBION ASSISTANT",0,42,15,790,34);
    SendMessageW(title,WM_SETFONT,reinterpret_cast<WPARAM>(titleFont),FALSE);theme::styleControl(title,theme::Role::Title);
    control(L"STATIC",L"",SS_ENDELLIPSIS,24,53,394,26,ActiveNames);
    control(L"STATIC",L"",SS_ENDELLIPSIS,430,53,402,26,ActiveSetName);
    const wchar_t* tabs[]={L"HUDs",L"Status",L"Perfis e regras",L"Monitorar"};
    for(int i=0;i<4;++i){button(tabs[i],Tab0+i,24+i*206,88,194);theme::styleControl(item(Tab0+i),theme::Role::Tab);theme::setActive(item(Tab0+i),i==page);}
    auto combo=[&](int id,int x,int y,int width) {return control(L"COMBOBOX",L"",WS_TABSTOP|CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS|WS_VSCROLL,x,y,width,240,id);};
    auto list=[&](int id,int x,int y,int width,int height) {return control(L"LISTBOX",L"",WS_TABSTOP|WS_BORDER|WS_VSCROLL|LBS_NOTIFY|LBS_NOINTEGRALHEIGHT|LBS_OWNERDRAWFIXED|LBS_HASSTRINGS,x,y,width,height,id);};
    auto hudChoices=[&](int x,int y,int width) {
        auto field=combo(HudList,x,y,width);for(const auto& value:workspace.huds)add(field,value.name);
        if(page==0)add(field,L"+ Criar HUD");
        choose(field,indexOf(workspace.huds,workspace.activeHudId));
    };
    auto setChoices=[&](int x,int y,int width) {
        auto field=combo(SetList,x,y,width);for(const auto& value:workspace.sets)add(field,value.name);
        choose(field,indexOf(workspace.sets,workspace.activeSetId));
    };
    if(page==3) {
        label(L"HUD desta tela",24,136,386);hudChoices(24,158,386);
        label(L"Set de regras",430,136,402);setChoices(430,158,402);
        button(L"Conectar ao jogo",Connect,24,202,158);button(L"Iniciar leitura",Start,194,202,150);button(L"Parar",Stop,356,202,100);
        label(L"Validade da leitura (ms)",478,207,216);edit(std::to_wstring(workspace.validityMs).c_str(),Validity,702,204,130);
        control(L"BUTTON",L"Mostrar overlay no compartilhamento",WS_TABSTOP|BS_AUTOCHECKBOX,478,231,354,18,ShareOverlay);
        SendMessageW(item(ShareOverlay),BM_SETCHECK,showOverlayInCapture?BST_CHECKED:BST_UNCHECKED,0);
        label(L"Leituras e destaques",24,250,808);
        control(L"EDIT",L"",WS_TABSTOP|WS_BORDER|WS_VSCROLL|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL,24,275,808,159,MonitorSummary);
        label(L"Última captura da área monitorada",24,449,620);button(L"Salvar ajuste",Save,664,444,168);
    } else if(page==0) {
        if(!hud()) {
            const bool firstHud=workspace.huds.empty();
            auto heading=control(L"STATIC",firstHud?L"Sua tela, do seu jeito":L"Escolha uma HUD para continuar",0,64,210,720,40);
            SendMessageW(heading,WM_SETFONT,reinterpret_cast<WPARAM>(titleFont),FALSE);
            if(firstHud) {
                label(L"Crie uma HUD para organizar as áreas do jogo que você quer acompanhar.",64,266,720,30);
                label(L"Depois, marque onde os status aparecem e onde os destaques devem surgir.\nVocê pode guardar uma HUD para cada tela ou organização do jogo.",64,309,700,60);
            } else {
                label(L"Suas outras HUDs continuam salvas. Selecione a que deseja editar.",64,266,720,30);
                hudChoices(64,320,500);
            }
            if(firstHud){label(L"Nome da HUD",64,386,500);edit(L"",720,64,412,500);}
            button(firstHud?L"Criar HUD":L"Criar outra HUD",NewHud,64,462,300);
            theme::styleControl(item(NewHud),theme::Role::Primary);
        } else {
        label(L"HUD selecionada",24,136,466);hudChoices(24,158,466);
        button(L"Renomear HUD",HudName,510,156,154);button(L"Excluir HUD",DeleteHud,678,156,154);
        auto screen=hud()&&hud()->clientWidth>0?std::to_wstring(hud()->clientWidth)+L" × "+std::to_wstring(hud()->clientHeight)+L" px · DPI "+std::to_wstring(hud()->monitorDpi):L"A tela será registrada ao selecionar a primeira área.";
        label(screen.c_str(),24,207,632,38);
        if(hud()->areas.empty()) {
            selectedArea=-1;
            auto heading=control(L"STATIC",L"Adicione a primeira área",0,64,298,700,36);
            SendMessageW(heading,WM_SETFONT,reinterpret_cast<WPARAM>(titleFont),FALSE);
            label(L"Dê um nome como Meus status ou Habilidade E.\nEm seguida, selecione a posição no jogo usando um círculo ou retângulo.",64,350,700,60);
            label(L"Nome da área",64,423,500);edit(L"",720,64,450,500);
            button(L"Criar área",NewArea,64,502,240);
            theme::styleControl(item(NewArea),theme::Role::Primary);
        } else {
        label(L"Áreas desta HUD",24,250,238);
        auto areas=list(AreaList,24,275,238,249);
        if(hud())for(const auto& value:hud()->areas)add(areas,value.name,true);
        if(hud()&&(selectedArea<0||selectedArea>=static_cast<int>(hud()->areas.size())))selectedArea=hud()->areas.empty()?-1:0;
        choose(areas,selectedArea,true);
        label(L"Nome da área",282,275,550);edit(area()?area()->name.c_str():L"",AreaName,282,299,550);
        const auto usage=area()?areaUsage(workspace,area()->name):AreaUsage{};
        button(L"Selecionar área",SelectArea,282,347,258);
        if(area()&&(usage.read||!usage.highlight))button(usage.read?L"Medir ícone de status":L"Medir ícone (opcional)",CalibrateArea,558,347,274);
        label(area()?regionText(*area(),usage).c_str():L"Crie uma área para ler status ou receber um destaque.",282,395,550,76);
        const bool circularRead=area()&&area()->region.shape==aa::RegionShape::Circle&&areaUsage(workspace,area()->name).read;
        label(circularRead?L"Enquadre o ícone inteiro; centros fora do círculo são ignorados.\nUse nomes iguais em HUDs diferentes para reutilizar seus sets.\nAo renomear uma área, atualize também as regras.":
            L"Use nomes iguais em HUDs diferentes para reutilizar seus sets.\nRenomear uma área exige atualizar o nome nas regras correspondentes.",282,479,550,60);
        button(L"Nova área",NewArea,24,547,112);button(L"Excluir área",DeleteArea,148,547,114);
        theme::styleControl(item(SelectArea),theme::Role::Primary);
        label(L"Alterações salvas automaticamente",282,553,550);
        for(int id:{AreaName,DeleteArea,SelectArea})EnableWindow(item(id),area()!=nullptr);
        EnableWindow(item(CalibrateArea),area()&&area()->region.valid());
        }
        }
    } else if(page==1) {
        if(workspace.statuses.empty()) {
            auto heading=control(L"STATIC",L"O que você quer acompanhar?",0,64,210,720,40);
            SendMessageW(heading,WM_SETFONT,reinterpret_cast<WPARAM>(titleFont),FALSE);
            label(L"Cadastre a imagem de um buff ou debuff que aparece no jogo.\nDepois, use esse status nas regras dos seus perfis.",64,276,700,60);
            label(L"Nome do status",64,357,500);edit(L"",720,64,384,500);
            button(L"Criar status",NewStatus,64,445,300);
            theme::styleControl(item(NewStatus),theme::Role::Primary);
        } else {
        label(L"Biblioteca compartilhada",24,136,238);auto statuses=list(StatusList,24,160,238,364);
        for(const auto& value:workspace.statuses)add(statuses,value.name,true);
        if(!selectedStatus()&&!workspace.statuses.empty())selectedStatusId=workspace.statuses.front().id;
        choose(statuses,indexOf(workspace.statuses,selectedStatusId),true);
        label(L"Nome do status",282,136,550);edit(selectedStatus()?selectedStatus()->name.c_str():L"",StatusName,282,160,550);
        button(L"Capturar referência",CaptureStatus,282,211,258);button(L"Importar imagem",ImportStatus,558,211,274);
        const auto status=selectedStatus();
        const auto referenceInfo=status?std::wstring(status->builtinAssassin?L"Identidade: exemplo incluído.":status->referencePath.empty()?L"Identidade: capture ou importe uma referência.":L"Identidade: referência salva.")+L"\nRelógio: "+(!status->clockReferencePath.empty()?L"referência salva.":status->builtinAssassin?L"referência do exemplo incluída.":L"referência ainda não capturada."):L"Crie um status para cadastrar qualquer buff ou debuff.";
        label(referenceInfo.c_str(),282,255,550,44);
        label(L"Imagem de referência",282,304,550);
        button(L"Capturar relógio",CaptureClock,282,491,260);
        button(L"Novo status",NewStatus,24,547,112);button(L"Excluir status",DeleteStatus,148,547,114);
        label(L"Alterações salvas automaticamente",282,553,550);
        label(L"Renove o status antes de capturar o relógio; selecione o ícone inteiro, sem sombra.",24,584,808,24);
        referencePreview={};
        if(selectedStatus()) {
            try {
                if(!selectedStatus()->referencePath.empty())referencePreview=aa::loadImage(selectedStatus()->referencePath);
                else if(selectedStatus()->builtinAssassin)referencePreview=aa::loadImageResource(IDR_ASSASSIN_NONE);
            }catch(const std::exception& e){error=L"Não foi possível abrir a referência: "+widen(e.what());}
        }
        for(int id:{StatusName,CaptureStatus,ImportStatus,DeleteStatus,CaptureClock,Save})EnableWindow(item(id),selectedStatus()!=nullptr);
        }
    } else {
        if(!set()) {
            auto heading=control(L"STATIC",L"Organize os destaques do seu set",0,64,210,720,40);
            SendMessageW(heading,WM_SETFONT,reinterpret_cast<WPARAM>(titleFont),FALSE);
            label(L"Um perfil reúne suas regras: qual status observar, quando agir\ne onde mostrar o destaque na tela.",64,276,700,60);
            if(!workspace.sets.empty()){label(L"Escolha um perfil salvo",64,342,500);setChoices(64,369,500);}
            else {label(L"Nome do perfil",64,342,500);edit(L"",720,64,369,500);}
            button(L"Criar perfil",NewSet,64,445,260);
            theme::styleControl(item(NewSet),theme::Role::Primary);
        } else {
        label(L"Perfis salvos",24,136,284);setChoices(24,158,284);
        label(L"Nome do perfil",326,136,326);edit(set()->name.c_str(),SetName,326,158,326);
        button(L"Novo perfil",NewSet,670,156,162);button(L"Excluir perfil",DeleteSet,670,197,162);
        if(set()->rules.empty()) {
            selectedRule=-1;
            auto heading=control(L"STATIC",L"Crie a primeira regra deste perfil",0,64,298,720,40);
            SendMessageW(heading,WM_SETFONT,reinterpret_cast<WPARAM>(titleFont),FALSE);
            label(L"Quando um status atender à condição escolhida,\na regra mostra um destaque na área que você indicar.",64,350,700,60);
            label(L"Nome da regra",64,423,500);edit(L"",720,64,450,500);
            button(L"Criar regra",NewRule,64,502,260);
            theme::styleControl(item(NewRule),theme::Role::Primary);
        } else {
        label(L"A primeira regra verdadeira vence se duas usam o mesmo destino.",24,205,628,30);
        label(L"Regras em ordem de prioridade",24,240,238);auto rules=list(RuleList,24,265,238,255);
        if(set())for(const auto& value:set()->rules)add(rules,value.condition.name,true);
        if(set()&&(selectedRule<0||selectedRule>=static_cast<int>(set()->rules.size())))selectedRule=set()->rules.empty()?-1:0;
        choose(rules,selectedRule,true);
        label(L"Nome da regra",282,240,380);edit(rule()?rule()->condition.name.c_str():L"",RuleName,282,263,380);
        control(L"BUTTON",L"Ativada",WS_TABSTOP|BS_AUTOCHECKBOX,682,263,150,26,Enabled);
        SendMessageW(item(Enabled),BM_SETCHECK,rule()&&rule()->condition.enabled?BST_CHECKED:BST_UNCHECKED,0);
        label(L"Status",282,302,260);auto statuses=combo(RuleStatus,282,324,260);
        for(const auto& value:workspace.statuses)add(statuses,value.name);
        choose(statuses,rule()?indexOf(workspace.statuses,rule()->statusId):-1);
        label(L"Área de origem",560,302,272);auto sourceAreas=combo(SourceArea,560,324,272);
        label(L"Condição",282,361,260);auto condition=combo(ConditionBox,282,383,260);
        add(condition,L"Estiver presente");add(condition,L"Estiver ausente");add(condition,L"Tiver exatamente os stacks");choose(condition,rule()?conditionIndex(rule()->condition.condition):0);
        label(L"Stacks",560,361,110);combo(Stacks,560,383,110);
        label(L"Destaque",688,361,144);auto effect=combo(EffectBox,688,383,144);for(auto name:EffectNames)add(effect,name);choose(effect,rule()?static_cast<int>(rule()->effect):0);
        label(L"Área de destino",282,420,260);auto targetAreas=combo(TargetArea,282,442,260);
        if(hud())for(const auto& value:hud()->areas){add(sourceAreas,value.name);add(targetAreas,value.name);}
        if(rule())for(auto pair:{std::pair{sourceAreas,rule()->sourceArea},std::pair{targetAreas,rule()->targetArea}}) {
            if(!pair.second.empty()&&SendMessageW(pair.first,CB_FINDSTRINGEXACT,static_cast<WPARAM>(-1),reinterpret_cast<LPARAM>(pair.second.c_str()))==CB_ERR)add(pair.first,pair.second);
            selectText(pair.first,pair.second);
        }
        label(L"Cor do destaque",560,420,272);auto colors=combo(Color,560,442,124);int chosenColor=0;
        button(L"Cor da habilidade",SampleColor,694,440,138);
        for(int i=0;i<4;++i){auto row=add(colors,ColorNames[i]);SendMessageW(colors,CB_SETITEMDATA,row,Colors[i]);if(rule()&&rule()->condition.color==Colors[i])chosenColor=i;}
        if(rule()&&std::find(std::begin(Colors),std::end(Colors),rule()->condition.color)==std::end(Colors)) {
            chosenColor=add(colors,L"Cor salva");SendMessageW(colors,CB_SETITEMDATA,chosenColor,rule()->condition.color);
        }
        choose(colors,chosenColor);
        if(rule()&&rule()->condition.condition==aa::Condition::StacksEqual) {
            label(L"Nova amostra",282,474,94);edit(std::to_wstring(rule()->condition.stacks).c_str(),SampleValue,378,472,58);
            button(L"Capturar amostra",CaptureRuleStack,446,470,160);
            const auto sample=std::find_if(rule()->stackSamples.begin(),rule()->stackSamples.end(),[&](const auto& value){return value.value==rule()->condition.stacks;});
            if(sample!=rule()->stackSamples.end())button(L"Excluir amostra",DeleteRuleStack,616,470,216);
        }
        control(L"BUTTON",L"Aro com previsão de tempo (experimental)",WS_TABSTOP|BS_AUTOCHECKBOX,282,502,550,26,FollowClock);
        SendMessageW(item(FollowClock),BM_SETCHECK,rule()&&rule()->followClock?BST_CHECKED:BST_UNCHECKED,0);
        control(L"STATIC",L"",SS_ENDELLIPSIS,282,531,550,20,RulePhrase);
        control(L"STATIC",L"",0,282,588,550,24,ClockHint);
        button(L"Nova regra",NewRule,24,535,112);button(L"Excluir regra",DeleteRule,148,535,114);
        button(L"Subir",MoveRuleUp,24,574,112);button(L"Descer",MoveRuleDown,148,574,114);
        button(L"Testar destaque por 5 s",TestAction,282,554,256);
        for(int id:{SetName,DeleteSet,NewRule,Save})EnableWindow(item(id),set()!=nullptr);
        for(int id:{RuleName,Enabled,RuleStatus,SourceArea,ConditionBox,Stacks,EffectBox,TargetArea,Color,SampleColor,CaptureRuleStack,DeleteRuleStack,TestAction,DeleteRule,MoveRuleUp,MoveRuleDown})if(item(id))EnableWindow(item(id),rule()!=nullptr);
        updateRuleChoices();
        }
        }
    }
    statusLabel=control(L"STATIC",L"",0,36,627,784,39);theme::styleControl(statusLabel);
    label(L"F8: voltar ao painel / encerrar teste     F9: iniciar ou parar leitura",24,674,808,22);
    refreshStatus();
}

void App::rebuildUIWithDraft() {
    RebuildScope rebuild(*this);
    struct Draft {int id;std::wstring value;LRESULT selected;};
    std::vector<Draft> edits,choices;
    for(int id:std::initializer_list<int>{HudName,AreaName,StatusName,SampleValue,SetName,RuleName,Validity,720})if(item(id))edits.push_back({id,text(item(id)),0});
    for(int id:{RuleStatus,SourceArea,TargetArea,ConditionBox,Stacks,EffectBox,Color})if(item(id))choices.push_back({id,text(item(id)),selection(item(id))});
    const auto enabled=item(Enabled)?SendMessageW(item(Enabled),BM_GETCHECK,0,0):BST_UNCHECKED;
    const auto followClock=item(FollowClock)?SendMessageW(item(FollowClock),BM_GETCHECK,0,0):BST_UNCHECKED;
    const auto stackSelected=item(StackList)?selection(item(StackList),true):-1;
    makeUI();
    for(const auto& draft:edits)SetWindowTextW(item(draft.id),draft.value.c_str());
    for(const auto& draft:choices)if(draft.id!=Stacks)choose(item(draft.id),static_cast<int>(draft.selected));
    if(item(Enabled))SendMessageW(item(Enabled),BM_SETCHECK,enabled,0);
    if(item(FollowClock))SendMessageW(item(FollowClock),BM_SETCHECK,followClock,0);
    updateRuleChoices();
    for(const auto& draft:choices)if(draft.id==Stacks)selectText(item(Stacks),draft.value);
    if(item(StackList))choose(item(StackList),stackSelected,true);
    updateRuleChoices();refreshStatus();
}

aa::Workspace App::editorValues() {
    if(rebuilding||controls.empty())return workspace;
    auto changed=workspace;
    if(page==3&&item(Validity))changed.validityMs=number(Validity,1,60000);
    if(page==0)if(auto currentHud=byId(changed.huds,changed.activeHudId)) {
        if(selectedArea>=0&&selectedArea<static_cast<int>(currentHud->areas.size())) {
            auto name=nameFrom(item(AreaName));
            for(std::size_t i=0;i<currentHud->areas.size();++i)if(static_cast<int>(i)!=selectedArea&&aa::sameName(currentHud->areas[i].name,name))
                throw std::runtime_error("Já existe uma área com esse nome nesta HUD.");
            currentHud->areas[static_cast<std::size_t>(selectedArea)].name=std::move(name);
        }
    }
    if(page==1)if(auto status=byId(changed.statuses,selectedStatusId)) {
        status->name=nameFrom(item(StatusName));checkName(changed.statuses,status->id,status->name);
    }
    if(page==2)if(auto currentSet=byId(changed.sets,changed.activeSetId)) {
        currentSet->name=nameFrom(item(SetName));checkName(changed.sets,currentSet->id,currentSet->name);
        if(selectedRule>=0&&selectedRule<static_cast<int>(currentSet->rules.size())) {
            auto& value=currentSet->rules[static_cast<std::size_t>(selectedRule)];
            value.condition.name=nameFrom(item(RuleName));value.condition.profile=currentSet->name;
            const auto statusIndex=selection(item(RuleStatus));
            const auto statusId=statusIndex>=0&&statusIndex<static_cast<int>(changed.statuses.size())?changed.statuses[static_cast<std::size_t>(statusIndex)].id:L"";
            if(value.statusId!=statusId){value.stackSamples.clear();value.clockReferencePath.clear();}
            value.statusId=statusId;
            value.sourceArea=text(item(SourceArea));value.targetArea=text(item(TargetArea));
            value.condition.condition=conditionFrom(selection(item(ConditionBox)));
            if(value.condition.condition==aa::Condition::StacksEqual) {
                const auto row=selection(item(Stacks));
                std::vector<unsigned> allowed;for(const auto& sample:value.stackSamples)allowed.push_back(sample.value);
                const auto stacks=row<0?0U:static_cast<unsigned>(SendMessageW(item(Stacks),CB_GETITEMDATA,row,0));
                // Rascunhos sem amostra continuam editáveis; a preparação do monitor explica o bloqueio.
                if(std::find(allowed.begin(),allowed.end(),stacks)!=allowed.end())value.condition.stacks=stacks;
            }
            value.condition.enabled=SendMessageW(item(Enabled),BM_GETCHECK,0,0)==BST_CHECKED;
            value.followClock=value.condition.condition!=aa::Condition::Absent&&SendMessageW(item(FollowClock),BM_GETCHECK,0,0)==BST_CHECKED;
            const auto effect=selection(item(EffectBox));value.effect=static_cast<aa::OverlayEffect>(std::clamp(effect,0,3));
            const auto color=selection(item(Color));
            if(color>=0)value.condition.color=static_cast<std::uint32_t>(SendMessageW(item(Color),CB_GETITEMDATA,color,0));
        }
    }
    return changed;
}
void App::saveEditor() {
    if(rebuilding||controls.empty())return;
    auto changed=editorValues();stop();commit(std::move(changed));
}

void App::updateRuleChoices() {
    if(page!=2||!item(Stacks))return;
    auto stackControl=item(Stacks);const auto old=selection(stackControl);
    unsigned selected=old>=0?static_cast<unsigned>(SendMessageW(stackControl,CB_GETITEMDATA,old,0)):rule()?rule()->condition.stacks:0;
    const auto statusIndex=selection(item(RuleStatus));
    const auto status=statusIndex>=0&&statusIndex<static_cast<int>(workspace.statuses.size())?&workspace.statuses[static_cast<std::size_t>(statusIndex)]:nullptr;
    std::vector<unsigned> values;if(rule()&&status&&rule()->statusId==status->id)for(const auto& sample:rule()->stackSamples)values.push_back(sample.value);
    std::sort(values.begin(),values.end());values.erase(std::unique(values.begin(),values.end()),values.end());
    SendMessageW(stackControl,CB_RESETCONTENT,0,0);int active=-1;
    for(auto value:values){auto row=add(stackControl,std::to_wstring(value));SendMessageW(stackControl,CB_SETITEMDATA,row,value);if(value==selected)active=row;}
    choose(stackControl,active);
    const bool needsStacks=selection(item(ConditionBox))==2;
    EnableWindow(stackControl,rule()&&needsStacks&&!values.empty());
    const bool canFollow=rule()&&selection(item(ConditionBox))!=1;
    EnableWindow(item(FollowClock),canFollow);
    if(!canFollow)SendMessageW(item(FollowClock),BM_SETCHECK,BST_UNCHECKED,0);
    std::wstring clockHint;
    if(canFollow&&SendMessageW(item(FollowClock),BM_GETCHECK,0,0)==BST_CHECKED) {
        std::error_code ignored;
        const bool hasReference=status&&(!status->clockReferencePath.empty()?std::filesystem::is_regular_file(status->clockReferencePath,ignored):status->builtinAssassin);
        clockHint=hasReference?L"Aprende nesta sessão; término estimado após ciclos semelhantes.":L"Relógio sem referência. Capture em Status; a aura continua ativa.";
    }
    setIfChanged(item(ClockHint),clockHint);
    auto phrase=std::wstring(L"Quando ")+(status?status->name:L"o status escolhido")+L", na área "+(text(item(SourceArea)).empty()?L"de origem":text(item(SourceArea)))+L", ";
    if(needsStacks&&values.empty())phrase=L"Informe o valor e capture uma amostra deste status ao lado.";
    else if(needsStacks&&active<0)phrase=L"Escolha um valor de stacks cadastrado. O valor anterior não tem amostra para este status.";
    else {
        phrase+=needsStacks?L"tiver "+text(stackControl)+L" stacks":selection(item(ConditionBox))==1?L"estiver ausente":L"estiver presente";
        phrase+=L", destacar "+(text(item(TargetArea)).empty()?L"a área de destino":text(item(TargetArea)))+L" com "+text(item(EffectBox))+L" ("+text(item(Color))+L").";
    }
    setIfChanged(item(RulePhrase),rule()?phrase:L"Crie uma regra e escolha o status, as áreas e o destaque.");
}

void App::command(int id,int notification) {
    if(rebuilding||selecting)return;
    if(id==HudList&&notification==CBN_SELCHANGE&&page==0&&selection(item(HudList))==static_cast<int>(workspace.huds.size())){
        choose(item(HudList),indexOf(workspace.huds,workspace.activeHudId));
        command(NewHud,BN_CLICKED);return;
    }
    if(id==HudName&&notification==BN_CLICKED&&hud()){
        const auto name=requestName(*this,HudName);if(!name)return;
        auto changed=editorValues();byId(changed.huds,changed.activeHudId)->name=*name;
        commit(std::move(changed));error=L"HUD renomeada.";makeUI();return;
    }
    if(id==ShareOverlay&&notification==BN_CLICKED){
        const bool visible=SendMessageW(item(ShareOverlay),BM_GETCHECK,0,0)==BST_CHECKED;
        auto changed=workspace;changed.shareOverlayInCapture=visible;commit(std::move(changed));showOverlayInCapture=visible;
        for(auto& overlay:overlays)overlay->setCaptureVisible(visible);
        if(testOverlay)testOverlay->setCaptureVisible(visible);
        error=visible?L"Overlay incluído no compartilhamento.":L"Overlay oculto em capturas e compartilhamentos.";refreshStatus();return;
    }
    if((notification==EN_CHANGE&&(id==HudName||id==AreaName||id==StatusName||id==SetName||id==RuleName||id==Validity))||
       (notification==CBN_SELCHANGE&&(id==RuleStatus||id==ConditionBox||id==Stacks||id==EffectBox||id==Color||id==SourceArea||id==TargetArea))||
       (notification==BN_CLICKED&&(id==Enabled||id==FollowClock))){
        error=L"Alteração não salva.";refreshStatus();
    }
    if(notification==EN_KILLFOCUS&&(id==HudName||id==AreaName||id==StatusName||id==SetName||id==RuleName||id==Validity)){
        saveEditor();error=L"Alteração salva.";refreshStatus();return;
    }
    if((id==FollowClock||id==Enabled)&&notification==BN_CLICKED){updateRuleChoices();saveEditor();error=L"Alteração salva.";refreshStatus();return;}
    if(notification==CBN_SELCHANGE&&(id==RuleStatus||id==ConditionBox||id==Stacks||id==EffectBox||id==Color||id==SourceArea||id==TargetArea)){
        updateRuleChoices();saveEditor();error=L"Alteração salva.";
        if(id==ConditionBox||id==RuleStatus)makeUI();else refreshStatus();return;
    }
    const bool navigation=(notification==CBN_SELCHANGE&&(id==HudList||id==SetList))||
        (notification==LBN_SELCHANGE&&(id==StatusList||id==AreaList||id==RuleList));
    if(navigation) {
        const bool list=id==StatusList||id==AreaList||id==RuleList;const auto chosen=selection(item(id),list);
        const int currentSelection=id==HudList?indexOf(workspace.huds,workspace.activeHudId):id==SetList?indexOf(workspace.sets,workspace.activeSetId):id==StatusList?indexOf(workspace.statuses,selectedStatusId):id==AreaList?selectedArea:selectedRule;
        if(chosen==currentSelection)return;
        try{saveEditor();}catch(...) {
            const int previous=id==HudList?indexOf(workspace.huds,workspace.activeHudId):id==SetList?indexOf(workspace.sets,workspace.activeSetId):id==StatusList?indexOf(workspace.statuses,selectedStatusId):id==AreaList?selectedArea:selectedRule;
            choose(item(id),previous,list);throw;
        }
        auto changed=workspace;const auto oldStatusId=selectedStatusId;const int oldArea=selectedArea,oldRule=selectedRule;
        if(id==HudList&&chosen>=0&&chosen<static_cast<int>(changed.huds.size())){changed.activeHudId=changed.huds[static_cast<std::size_t>(chosen)].id;selectedArea=-1;}
        if(id==SetList&&chosen>=0&&chosen<static_cast<int>(changed.sets.size())){changed.activeSetId=changed.sets[static_cast<std::size_t>(chosen)].id;selectedRule=-1;}
        if(id==StatusList&&chosen>=0&&chosen<static_cast<int>(changed.statuses.size()))selectedStatusId=changed.statuses[static_cast<std::size_t>(chosen)].id;
        if(id==AreaList)selectedArea=chosen;
        if(id==RuleList)selectedRule=chosen;
        try{commit(std::move(changed));}catch(...){
            selectedStatusId=oldStatusId;selectedArea=oldArea;selectedRule=oldRule;
            const int previous=id==HudList?indexOf(workspace.huds,workspace.activeHudId):id==SetList?indexOf(workspace.sets,workspace.activeSetId):id==StatusList?indexOf(workspace.statuses,selectedStatusId):id==AreaList?selectedArea:selectedRule;
            choose(item(id),previous,list);throw;
        }
        error=L"Seleção atualizada. As alterações anteriores foram salvas.";makeUI();return;
    }
    if(notification!=BN_CLICKED)return;
    if(id>=Tab0&&id<Tab0+4){if(page==id-Tab0)return;saveEditor();page=id-Tab0;if(page==3)capturePreview={};error=L"Alterações salvas.";makeUI();return;}
    if(id==Stop){stop();error=L"Leitura parada. Os destaques estão apagados.";refreshStatus();return;}
    if(id==Connect){saveEditor();connect();refreshStatus();return;}
    if(id==Start){start();refreshStatus();return;}
    if(id==TestAction){testAction();refreshStatus();return;}
    if(id==SampleColor){sampleActionColor();refreshStatus();return;}
    if(id==CaptureClock){
        saveEditor();stop();
        auto chosen=pick(aa::SelectionKind::Icon,nullptr,aa::RegionShape::Circle);
        if(chosen)applyClockReference(chosen->image);else error=L"Captura cancelada. A referência do relógio foi mantida.";
        makeUI();return;
    }
    if(id==Enabled){updateRuleChoices();return;}
    if(id==Save){
        saveEditor();error=page==1?L"HUD e áreas salvas.":page==2?L"Status salvo na biblioteca.":page==3?L"Set e regras salvos.":L"Ajuste salvo.";
        if(page==3&&rule()&&rule()->condition.condition==aa::Condition::StacksEqual) {
            const auto currentStatus=byId(workspace.statuses,rule()->statusId);
            const auto values=currentStatus?aa::stackValues(*currentStatus):std::vector<unsigned>{};
            if(std::find(values.begin(),values.end(),rule()->condition.stacks)==values.end())error=L"Rascunho salvo. Cadastre uma amostra na aba Status antes de usar esta condição de stacks.";
        }
        makeUI();return;
    }
    const bool action=id==NewHud||id==DeleteHud||id==NewArea||id==DeleteArea||id==SelectArea||id==CalibrateArea||id==NewStatus||id==DeleteStatus||
        id==CaptureStatus||id==ImportStatus||id==CaptureStack||id==CaptureRuleStack||id==DeleteRuleStack||id==DeleteStack||id==AddPreset||id==NewSet||id==DeleteSet||id==NewRule||id==DeleteRule||id==MoveRuleUp||id==MoveRuleDown;
    if(!action)return;
    std::optional<std::wstring> newName;
    if(createsItem(id)){
        const bool inlineName=item(720)&&((id==NewHud&&workspace.huds.empty())||(id==NewArea&&hud()&&hud()->areas.empty())||
            (id==NewStatus&&workspace.statuses.empty())||(id==NewSet&&workspace.sets.empty())||(id==NewRule&&set()&&set()->rules.empty()));
        if(inlineName){newName=nameFrom(item(720));validateNewName(*this,id,*newName);}
        else newName=requestName(*this,id);
        if(!newName)return;
        saveEditor();
    }else saveEditor();
    auto changed=workspace;stop();
    auto currentHud=byId(changed.huds,changed.activeHudId);auto currentSet=byId(changed.sets,changed.activeSetId);
    auto status=byId(changed.statuses,selectedStatusId);
    const auto oldStatusId=selectedStatusId;const int oldArea=selectedArea,oldRule=selectedRule;int focus=0;
    switch(id) {
    case NewHud: {
        if(changed.huds.size()>=64)throw std::runtime_error("O limite é de 64 HUDs. Exclua uma HUD para criar outra.");
        aa::HudLayout value;value.id=aa::newId(changed);value.name=*newName;changed.activeHudId=value.id;
        changed.huds.push_back(std::move(value));selectedArea=-1;focus=720;error=L"HUD criada. Adicione as áreas necessárias.";break;
    }
    case DeleteHud:
        if(!currentHud)return;
        if(MessageBoxW(window,(L"Excluir a HUD “"+currentHud->name+L"” e suas áreas?\nOs status e sets serão preservados.").c_str(),L"Excluir HUD",MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2)!=IDYES)return;
        aa::eraseHud(changed,currentHud->id);selectedArea=-1;error=L"HUD excluída. Status e sets preservados.";break;
    case NewArea:
        if(!currentHud)throw std::runtime_error("Crie uma HUD antes de adicionar áreas.");
        if(currentHud->areas.size()>=32)throw std::runtime_error("O limite é de 32 áreas por HUD.");
        currentHud->areas.push_back({*newName,{},48,false});selectedArea=static_cast<int>(currentHud->areas.size())-1;
        focus=SelectArea;error=L"Área criada. Selecione sua posição no jogo.";break;
    case DeleteArea:
        if(!currentHud||!area())return;
        currentHud->areas.erase(currentHud->areas.begin()+selectedArea);selectedArea=-1;
        error=L"Área excluída. Regras que usam esse nome precisam de outra área correspondente.";break;
    case SelectArea:case CalibrateArea: {
        if(!currentHud||!area())throw std::runtime_error("Crie e escolha uma área primeiro.");
        if(target&&IsWindow(target))confirmGeometry(*currentHud,screenOf(target));
        if(id==CalibrateArea&&!area()->region.valid())throw std::runtime_error("Selecione a área antes de medir um ícone de status dentro dela.");
        auto chosen=pick(id==SelectArea?aa::SelectionKind::Highlight:aa::SelectionKind::Icon,nullptr,
                         id==SelectArea?area()->region.shape:aa::RegionShape::Circle);
        if(!chosen){error=L"Seleção cancelada. Os dados anteriores foram mantidos.";refreshStatus();return;}
        confirmGeometry(*currentHud,chosen->screen);auto& value=currentHud->areas[static_cast<std::size_t>(selectedArea)];
        if(id==SelectArea) {
            currentHud->clientWidth=chosen->screen.width;currentHud->clientHeight=chosen->screen.height;currentHud->monitorDpi=chosen->screen.dpi;currentHud->monitorDevice=chosen->screen.device;
            value.region=chosen->area;value.iconCalibrated=false;error=L"Área salva.";
        } else {
            const auto& r=chosen->area;const auto& a=value.region;
            if(r.x<a.x||r.y<a.y||r.x+r.width>a.x+a.width||r.y+r.height>a.y+a.height||!a.contains(r.x+r.width/2.0,r.y+r.height/2.0))
                throw std::runtime_error("Selecione um ícone inteiro dentro da área escolhida.");
            value.iconSize=r.width;value.iconCalibrated=true;error=L"Tamanho do ícone de status salvo para a busca nesta área.";
        }
        break;
    }
    case NewStatus:case AddPreset: {
        if(changed.statuses.size()>=64)throw std::runtime_error("O limite é de 64 status. Exclua um status sem regras para criar outro.");
        aa::StatusDefinition value;value.id=aa::newId(changed);value.name=id==AddPreset?uniqueName(changed.statuses,L"Espírito Assassino"):*newName;value.builtinAssassin=id==AddPreset;
        selectedStatusId=value.id;changed.statuses.push_back(std::move(value));focus=StatusName;
        error=id==AddPreset?L"Exemplo adicionado à biblioteca com amostras de 2 e 3 stacks.":L"Status criado. Capture a imagem de referência.";break;
    }
    case DeleteStatus:
        if(!status)return;aa::eraseStatus(changed,status->id);selectedStatusId.clear();error=L"Status excluído da biblioteca.";break;
    case CaptureStatus:case ImportStatus: {
        if(!status)throw std::runtime_error("Crie e escolha um status primeiro.");
        aa::Image image;
        if(id==CaptureStatus) {
            auto chosen=pick(aa::SelectionKind::Icon,nullptr);if(!chosen){error=L"Captura cancelada. A referência anterior foi mantida.";refreshStatus();return;}image=std::move(chosen->image);
        } else {
            wchar_t path[32768]{};OPENFILENAMEW dialog{sizeof(dialog)};dialog.hwndOwner=window;
            dialog.lpstrFilter=L"Imagem de referência (PNG/BMP)\0*.png;*.bmp\0";dialog.lpstrFile=path;dialog.nMaxFile=32768;dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
            if(!GetOpenFileNameW(&dialog))return;image=aa::loadImage(path);
        }
        status->referencePath=storeImage(status->id,image);status->builtinAssassin=false;status->stacks.clear();status->clockReferencePath.clear();
        error=L"Referência salva. Cadastre novamente as amostras de stacks para esta imagem.";break;
    }
    case CaptureStack: {
        if(!status)throw std::runtime_error("Escolha um status para cadastrar sua amostra.");
        if(!status->builtinAssassin&&status->referencePath.empty())throw std::runtime_error("Cadastre a referência do status antes das amostras de stacks.");
        const auto value=static_cast<unsigned>(number(SampleValue,1,99));auto chosen=pick(aa::SelectionKind::Icon,nullptr);
        if(!chosen){error=L"Captura da amostra cancelada.";refreshStatus();return;}
        aa::Recognizer sampleValidator;
        if(!sampleValidator.setStackReference(value,chosen->image))
            throw std::runtime_error("Não identifiquei um contador branco legível no canto inferior direito. Capture o ícone inteiro enquanto o número estiver visível.");
        const auto path=storeImage(status->id,chosen->image);
        auto found=std::find_if(status->stacks.begin(),status->stacks.end(),[&](const auto& entry){return entry.value==value;});
        if(found==status->stacks.end())status->stacks.push_back({value,path});else found->path=path;
        error=L"Amostra de "+std::to_wstring(value)+L" stacks salva.";break;
    }
    case CaptureRuleStack: {
        if(!currentSet||selectedRule<0||selectedRule>=static_cast<int>(currentSet->rules.size()))throw std::runtime_error("Escolha uma regra primeiro.");
        auto& currentRule=currentSet->rules[static_cast<std::size_t>(selectedRule)];
        auto* ruleStatus=byId(changed.statuses,currentRule.statusId);
        if(!ruleStatus||(!ruleStatus->builtinAssassin&&ruleStatus->referencePath.empty()))throw std::runtime_error("Cadastre a referência do status antes da amostra de stacks.");
        const auto value=static_cast<unsigned>(number(SampleValue,1,99));auto chosen=pick(aa::SelectionKind::Icon,nullptr);
        if(!chosen){error=L"Captura da amostra cancelada.";refreshStatus();return;}
        aa::Recognizer validator;
        if(!validator.setStackReference(value,chosen->image))throw std::runtime_error("Não identifiquei um contador branco legível no canto inferior direito. Capture o ícone inteiro enquanto o número estiver visível.");
        const auto path=storeImage(ruleStatus->id,chosen->image);
        auto found=std::find_if(currentRule.stackSamples.begin(),currentRule.stackSamples.end(),[&](const auto& entry){return entry.value==value;});
        if(found==currentRule.stackSamples.end())currentRule.stackSamples.push_back({value,path});else found->path=path;
        currentRule.condition.stacks=value;error=L"Amostra de "+std::to_wstring(value)+L" stacks salva.";break;
    }
    case DeleteRuleStack: {
        if(!currentSet||selectedRule<0||selectedRule>=static_cast<int>(currentSet->rules.size()))throw std::runtime_error("Escolha uma regra primeiro.");
        auto& currentRule=currentSet->rules[static_cast<std::size_t>(selectedRule)];
        const auto value=currentRule.condition.stacks;
        std::erase_if(currentRule.stackSamples,[&](const auto& sample){return sample.value==value;});
        error=L"Amostra de "+std::to_wstring(value)+L" stacks excluída. A regra ficará pendente até receber outra captura.";break;
    }
    case DeleteStack: {
        if(!status)return;const auto row=selection(item(StackList),true);if(row<0)throw std::runtime_error("Escolha a amostra de stacks para excluir.");
        const auto value=static_cast<unsigned>(SendMessageW(item(StackList),LB_GETITEMDATA,row,0));
        const auto found=std::find_if(status->stacks.begin(),status->stacks.end(),[&](const auto& entry){return entry.value==value;});
        if(found==status->stacks.end())throw std::runtime_error("Esta amostra faz parte do exemplo incluído. Capture outra referência para cadastrar somente suas próprias amostras.");
        status->stacks.erase(found);error=L"Amostra excluída. Confira as regras que usam esse valor de stacks.";break;
    }
    case NewSet: {
        if(changed.sets.size()>=64)throw std::runtime_error("O limite é de 64 sets. Exclua um set para criar outro.");
        aa::SetProfile value;value.id=aa::newId(changed);value.name=*newName;changed.activeSetId=value.id;
        changed.sets.push_back(std::move(value));selectedRule=-1;focus=720;error=L"Perfil criado. Adicione uma regra.";break;
    }
    case DeleteSet:
        if(!currentSet)return;
        if(MessageBoxW(window,(L"Excluir o set “"+currentSet->name+L"” e suas regras?\nAs HUDs e a biblioteca de status serão preservadas.").c_str(),L"Excluir set",MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2)!=IDYES)return;
        aa::eraseSet(changed,currentSet->id);selectedRule=-1;error=L"Set excluído. HUDs e status preservados.";break;
    case NewRule: {
        if(!currentSet)throw std::runtime_error("Crie um set antes de adicionar regras.");
        if(currentSet->rules.size()>=32)throw std::runtime_error("O limite é de 32 regras por set.");
        aa::StatusRule value;value.id=aa::newId(changed);value.condition.name=*newName;
        value.condition.profile=currentSet->name;value.condition.condition=aa::Condition::Present;value.condition.stacks=1;
        if(!changed.statuses.empty())value.statusId=changed.statuses.front().id;
        if(currentHud&&!currentHud->areas.empty()){value.sourceArea=currentHud->areas.front().name;value.targetArea=currentHud->areas.back().name;}
        currentSet->rules.push_back(std::move(value));selectedRule=static_cast<int>(currentSet->rules.size())-1;focus=RuleName;error=L"Regra criada. Escolha a origem, a condição e o destino do destaque.";break;
    }
    case DeleteRule:
        if(!currentSet||!rule())return;currentSet->rules.erase(currentSet->rules.begin()+selectedRule);selectedRule=-1;error=L"Regra excluída.";break;
    case MoveRuleUp:case MoveRuleDown: {
        if(!currentSet||!rule())return;const int next=selectedRule+(id==MoveRuleUp?-1:1);
        if(next<0||next>=static_cast<int>(currentSet->rules.size()))return;
        std::swap(currentSet->rules[static_cast<std::size_t>(selectedRule)],currentSet->rules[static_cast<std::size_t>(next)]);selectedRule=next;error=L"Prioridade da regra atualizada.";break;
    }
    default:return;
    }
    try{commit(std::move(changed));}catch(...){selectedStatusId=oldStatusId;selectedArea=oldArea;selectedRule=oldRule;throw;}
    makeUI();if(focus){SetFocus(item(focus));if(focus==StatusName||focus==RuleName)SendMessageW(item(focus),EM_SETSEL,0,-1);}
}

void App::refreshStatus() {
    if(!window)return;
    setIfChanged(item(ActiveNames),L"HUD ativa: "+(hud()?hud()->name:L"nenhuma"));
    setIfChanged(item(ActiveSetName),L"Set ativo: "+(set()?set()->name:L"nenhum"));
    std::wstring state=error.empty()&&!running?L"Leitura parada. Escolha uma HUD e um set para iniciar.":error;
    if(previewUntil)state=L"TESTE VISUAL · Simulação de 5 segundos, sem medir o buff. F8 encerra.";
    else if(running) {
        if(!geometryMatches())state=L"A tela ou escala mudou. Destaques apagados; escolha uma HUD correspondente.";
        else if(GetForegroundWindow()!=target)state=L"Jogo em segundo plano. Os destaques ficam apagados até voltar ao Albion.";
        else if(!error.empty())state=L"Leitura: "+error;
        else state=L"Leitura ativa. Cada status é acompanhado na área indicada abaixo.";
    }
    else if(page==3&&error.starts_with(L"Antes de iniciar:"))state=L"Leitura não iniciada. Veja as pendências em Leituras e destaques.";
    if(!hotkeyWarning.empty())state+=L"\n"+hotkeyWarning;
    setIfChanged(statusLabel,state);
    if(page!=3)return;
    EnableWindow(item(Start),!running);EnableWindow(item(Stop),running||previewUntil!=0);
    std::wstring summary;
    if(!running) {
        summary=L"Escolha uma HUD e um set. Configure áreas em HUDs, imagens em Status e condições em Sets e regras.\r\n";
        if(target&&IsWindow(target)) {
            try{auto screen=screenOf(target);summary+=L"Jogo conectado: "+std::to_wstring(screen.width)+L" × "+std::to_wstring(screen.height)+L" px.\r\n";}catch(const std::exception&){summary+=L"Janela do jogo indisponível. Conecte novamente.\r\n";}
        } else summary+=L"Jogo ainda não conectado.\r\n";
        if(error.starts_with(L"Antes de iniciar:"))summary+=error;
        else {
            const auto issues=aa::readinessIssues(workspace);
            if(issues.empty())summary+=L"Configuração pronta para iniciar.\r\n";
            else for(const auto& issue:issues)summary+=L"• "+issue+L"\r\n";
        }
    } else {
        const auto now=static_cast<std::int64_t>(GetTickCount64());
        for(std::size_t i=0;i<plan.readers.size();++i) {
            summary+=plan.readers[i].status.name+L" · "+plan.readers[i].area.name+L": ";
            const bool fresh=i<current.size()&&current[i].source==source&&current[i].capturedMs>0&&
                now>=current[i].capturedMs&&now-current[i].capturedMs<workspace.validityMs;
            const aa::Observation* shown=fresh?&current[i]:nullptr;
            if(!fresh) {
                summary+=GetForegroundWindow()!=target?L"leitura pausada fora do jogo":L"sem imagem recente";
                if(i<lastReadings.size()&&lastReadings[i].source==source&&lastReadings[i].capturedMs>0) {
                    const auto age=std::max<std::int64_t>(0,now-lastReadings[i].capturedMs)/1000;
                    summary+=L"\r\n  Última leitura (há "+std::to_wstring(age)+L" s; não aciona destaque): ";
                    shown=&lastReadings[i];
                }
            }
            if(shown) {
                const auto& detection=shown->detection;
                if(detection.presence==aa::Presence::Unknown)summary+=L"não confirmado";
                else if(detection.presence==aa::Presence::Absent)summary+=L"ausente";
                else summary+=L"identificado · stacks "+(detection.stacks?std::to_wstring(*detection.stacks):L"desconhecidos");
                if(plan.readers[i].needsClock) {
                    if(i>=recognizers.size()||!recognizers[i]->clockReady())summary+=L" · relógio indisponível: capture a referência na aba Status";
                    else if(detection.remainingFraction)summary+=L" · relógio observado: "+std::to_wstring(static_cast<int>(*detection.remainingFraction*100))+L"%";
                    else summary+=L" · relógio sem leitura confiável";
                    if(i<current.size()&&shown==&current[i]&&i<clockPredictions.size()&&clockPredictions[i].estimated&&clockPredictions[i].fraction)
                        summary+=L" · aro estimado: "+std::to_wstring(static_cast<int>(*clockPredictions[i].fraction*100))+L"%";
                }
            }
            summary+=L"\r\n";
        }
        if(plan.readers.empty())summary=L"Aguardando o início das leituras.\r\n";
        for(std::size_t i=0;i<plan.actions.size();++i)
            summary+=plan.actions[i].rule.condition.name+L" → "+plan.actions[i].rule.targetArea+(i<lit.size()&&lit[i]?L": DESTAQUE ATIVO":L": apagado")+L"\r\n";
    }
    setIfChanged(item(MonitorSummary),summary);
}

void App::paint() {
    PAINTSTRUCT paint{};const auto targetDc=BeginPaint(window,&paint);RECT client{};GetClientRect(window,&client);
    const auto buffer=CreateCompatibleDC(targetDc);const auto bitmap=CreateCompatibleBitmap(targetDc,client.right,client.bottom);
    const auto dc=buffer&&bitmap?buffer:targetDc;const auto oldBitmap=buffer&&bitmap?SelectObject(buffer,bitmap):nullptr;
    const int saved=SaveDC(dc);
    theme::fill(dc,client,theme::Background);
    theme::fill(dc,{0,0,client.right,px(3)},theme::Accent);
    theme::fill(dc,{px(24),px(20),px(28),px(42)},theme::Accent);
    theme::fill(dc,{px(12),px(128),px(848),px(613)},theme::Panel);
    theme::frame(dc,{px(12),px(128),px(848),px(613)},RGB(39,44,53));
    theme::frame(dc,{px(24),px(619),px(832),px(668)},RGB(54,48,49));
    theme::fill(dc,{px(24),px(619),px(27),px(668)},theme::Accent);
    if(page==3||(page==1&&selectedStatus())) {
        const auto& preview=page==3?capturePreview:referencePreview;
        const RECT bounds=page==3?RECT{px(24),px(486),px(832),px(602)}:RECT{px(282),px(330),px(544),px(481)};
        theme::fill(dc,bounds,theme::Field);theme::frame(dc,bounds,theme::Border);
        if(preview.valid()) {
            const auto factor=std::min(static_cast<double>(bounds.right-bounds.left)/preview.width,static_cast<double>(bounds.bottom-bounds.top)/preview.height);
            const int width=static_cast<int>(preview.width*factor),height=static_cast<int>(preview.height*factor);
            BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=preview.width;info.bmiHeader.biHeight=-preview.height;
            info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
            SetStretchBltMode(dc,COLORONCOLOR);StretchDIBits(dc,bounds.left+(bounds.right-bounds.left-width)/2,bounds.top+(bounds.bottom-bounds.top-height)/2,width,height,0,0,preview.width,preview.height,preview.bgra.data(),&info,DIB_RGB_COLORS,SRCCOPY);
        } else {
            SetBkMode(dc,TRANSPARENT);SetTextColor(dc,theme::Muted);SelectObject(dc,font);auto message=bounds;
            DrawTextW(dc,page==3?(running?L"Aguardando a primeira captura do jogo...":L"Sem captura. Inicie a leitura após resolver as pendências."):L"Cadastre uma imagem de referência.",-1,&message,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        }
    }
    RestoreDC(dc,saved);
    if(buffer&&bitmap){BitBlt(targetDc,0,0,client.right,client.bottom,buffer,0,0,SRCCOPY);SelectObject(buffer,oldBitmap);}
    if(bitmap)DeleteObject(bitmap);if(buffer)DeleteDC(buffer);
    EndPaint(window,&paint);
}
}
