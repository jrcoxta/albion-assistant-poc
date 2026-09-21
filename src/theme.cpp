#include "theme.h"
#include <commctrl.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <algorithm>
#include <string>

namespace aaapp::theme {
namespace {
constexpr wchar_t RoleKey[]=L"Albion.Theme.Role",ActiveKey[]=L"Albion.Theme.Active";
constexpr wchar_t HoverKey[]=L"Albion.Theme.Hover",LockKey[]=L"Albion.Theme.Redraw";
constexpr UINT_PTR WindowSubclass=1,ControlSubclass=2;
struct Brushes {
    HBRUSH background=CreateSolidBrush(Background),panel=CreateSolidBrush(Panel),field=CreateSolidBrush(Field);
    ~Brushes(){DeleteObject(background);DeleteObject(panel);DeleteObject(field);}
};
Brushes& brushes(){static Brushes value;return value;}
Role roleOf(HWND control){return static_cast<Role>(reinterpret_cast<INT_PTR>(GetPropW(control,RoleKey)));}
std::wstring caption(HWND control){std::wstring value(static_cast<std::size_t>(GetWindowTextLengthW(control))+1,L'\0');GetWindowTextW(control,value.data(),static_cast<int>(value.size()));value.resize(wcslen(value.c_str()));return value;}
bool isClass(HWND control,const wchar_t* expected){wchar_t name[32]{};GetClassNameW(control,name,32);return _wcsicmp(name,expected)==0;}
int unit(HWND control,int value){
    LOGFONTW font{};const auto handle=reinterpret_cast<HFONT>(SendMessageW(control,WM_GETFONT,0,0));
    if(handle&&GetObjectW(handle,sizeof(font),&font))return std::max(1,MulDiv(value,std::abs(font.lfHeight),15));
    return std::max(1,MulDiv(value,static_cast<int>(GetDpiForWindow(control)),96));
}
void textIn(HDC dc,HWND control,const std::wstring& value,RECT bounds,COLORREF color,UINT flags){
    const int saved=SaveDC(dc);SelectObject(dc,reinterpret_cast<HFONT>(SendMessageW(control,WM_GETFONT,0,0)));
    SetBkMode(dc,TRANSPARENT);SetTextColor(dc,color);DrawTextW(dc,value.c_str(),-1,&bounds,flags);RestoreDC(dc,saved);
}
COLORREF controlBackground(HWND control){
    RECT rect{};GetWindowRect(control,&rect);MapWindowPoints(nullptr,GetParent(control),reinterpret_cast<POINT*>(&rect),2);
    RECT parent{};GetClientRect(GetParent(control),&parent);
    return parent.bottom>0&&rect.top>=parent.bottom*128/700&&rect.top<parent.bottom*613/700?Panel:Background;
}
void borderFor(HDC dc,HWND control,RECT bounds){frame(dc,bounds,GetFocus()==control?Gold:Border);}
void drawButton(HWND control,HDC dc){
    RECT bounds{};GetClientRect(control,&bounds);
    const auto role=roleOf(control);const auto state=SendMessageW(control,BM_GETSTATE,0,0);
    const bool enabled=IsWindowEnabled(control)!=FALSE,hover=GetPropW(control,HoverKey)!=nullptr,active=GetPropW(control,ActiveKey)!=nullptr;
    const bool checkbox=(GetWindowLongPtrW(control,GWL_STYLE)&BS_TYPEMASK)==BS_AUTOCHECKBOX;
    const bool primary=role==Role::Primary||active;
    COLORREF color=checkbox?controlBackground(control):primary?Accent:RGB(38,43,51);
    if(enabled&&(state&BST_PUSHED))color=primary?RGB(113,27,38):RGB(28,32,39);
    else if(enabled&&hover)color=primary?RGB(181,49,63):RGB(51,58,69);
    if(!enabled&&!checkbox)color=RGB(29,32,38);
    fill(dc,bounds,color);
    if(checkbox){
        const int size=std::min(unit(control,16),static_cast<int>(bounds.bottom)-4);
        RECT box{2,(bounds.bottom-size)/2,2+size,(bounds.bottom+size)/2};
        const bool checked=SendMessageW(control,BM_GETCHECK,0,0)==BST_CHECKED;
        fill(dc,box,checked&&enabled?Accent:Field);frame(dc,box,GetFocus()==control?Gold:Border);
        if(checked){
            const auto pen=CreatePen(PS_SOLID,unit(control,2),enabled?Text:Muted);const auto old=SelectObject(dc,pen);
            MoveToEx(dc,box.left+size/5,box.top+size/2,nullptr);LineTo(dc,box.left+size*2/5,box.bottom-size/4);LineTo(dc,box.right-size/6,box.top+size/4);
            SelectObject(dc,old);DeleteObject(pen);
        }
        bounds.left=box.right+unit(control,8);
        textIn(dc,control,caption(control),bounds,enabled?Text:Muted,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);
    }else{
        frame(dc,bounds,GetFocus()==control?Gold:primary?RGB(193,61,72):Border);
        if(role==Role::Tab&&active){auto stripe=bounds;stripe.top=stripe.bottom-unit(control,3);fill(dc,stripe,Gold);}
        InflateRect(&bounds,-unit(control,7),0);
        textIn(dc,control,caption(control),bounds,enabled?Text:RGB(116,125,139),DT_SINGLELINE|DT_CENTER|DT_VCENTER|DT_END_ELLIPSIS);
    }
}
void drawCombo(HWND control,HDC dc){
    RECT bounds{};GetClientRect(control,&bounds);fill(dc,bounds,Field);borderFor(dc,control,bounds);
    RECT label=bounds;label.left+=unit(control,9);label.right-=unit(control,28);
    if(roleOf(control)==Role::ColorChoice){
        const auto index=SendMessageW(control,CB_GETCURSEL,0,0);
        if(index>=0){const int size=unit(control,12);RECT swatch{label.left,(bounds.bottom-size)/2,label.left+size,(bounds.bottom+size)/2};fill(dc,swatch,static_cast<COLORREF>(SendMessageW(control,CB_GETITEMDATA,index,0)));label.left+=size+unit(control,8);}
    }
    textIn(dc,control,caption(control),label,IsWindowEnabled(control)?Text:Muted,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX);
    const int x=bounds.right-unit(control,14),y=bounds.bottom/2;
    const auto pen=CreatePen(PS_SOLID,unit(control,1),IsWindowEnabled(control)?Muted:Border);const auto old=SelectObject(dc,pen);
    MoveToEx(dc,x-unit(control,4),y-unit(control,2),nullptr);LineTo(dc,x,y+unit(control,2));LineTo(dc,x+unit(control,4),y-unit(control,2));SelectObject(dc,old);DeleteObject(pen);
}
void drawBadge(HWND control,HDC dc){
    RECT bounds{};GetClientRect(control,&bounds);fill(dc,bounds,Background);frame(dc,bounds,Accent);
    InflateRect(&bounds,-unit(control,8),0);textIn(dc,control,caption(control),bounds,Gold,DT_SINGLELINE|DT_CENTER|DT_VCENTER|DT_END_ELLIPSIS);
}
LRESULT CALLBACK controlProc(HWND control,UINT message,WPARAM wp,LPARAM lp,UINT_PTR,DWORD_PTR){
    const bool customPaint=isClass(control,L"BUTTON")||isClass(control,L"COMBOBOX")||roleOf(control)==Role::Badge;
    if(customPaint&&(message==WM_PAINT||message==WM_PRINTCLIENT)){
        PAINTSTRUCT paint{};const auto dc=message==WM_PAINT?BeginPaint(control,&paint):reinterpret_cast<HDC>(wp);
        const int saved=SaveDC(dc);
        if(roleOf(control)==Role::Badge)drawBadge(control,dc);else if(isClass(control,L"BUTTON"))drawButton(control,dc);else drawCombo(control,dc);
        RestoreDC(dc,saved);if(message==WM_PAINT)EndPaint(control,&paint);return 0;
    }
    if(customPaint&&message==WM_ERASEBKGND)return 1;
    if(message==WM_MOUSEMOVE&&!GetPropW(control,HoverKey)){
        SetPropW(control,HoverKey,reinterpret_cast<HANDLE>(1));TRACKMOUSEEVENT track{sizeof(track),TME_LEAVE,control,0};TrackMouseEvent(&track);InvalidateRect(control,nullptr,FALSE);
    }
    if(message==WM_MOUSELEAVE){RemovePropW(control,HoverKey);InvalidateRect(control,nullptr,FALSE);}
    if(message==WM_NCDESTROY){RemovePropW(control,RoleKey);RemovePropW(control,ActiveKey);RemovePropW(control,HoverKey);RemoveWindowSubclass(control,controlProc,ControlSubclass);return DefSubclassProc(control,message,wp,lp);}
    const auto result=DefSubclassProc(control,message,wp,lp);
    if(message==WM_SETFOCUS||message==WM_KILLFOCUS||message==WM_ENABLE||message==BM_SETCHECK||message==BM_SETSTATE||message==CB_SETCURSEL||message==WM_SETTEXT){
        RedrawWindow(control,nullptr,nullptr,RDW_INVALIDATE|RDW_FRAME);
    }
    if((message==WM_NCPAINT||message==WM_SETFOCUS||message==WM_KILLFOCUS)&&
        (GetWindowLongPtrW(control,GWL_STYLE)&WS_BORDER)){
        const auto dc=GetWindowDC(control);RECT bounds{};GetWindowRect(control,&bounds);OffsetRect(&bounds,-bounds.left,-bounds.top);borderFor(dc,control,bounds);ReleaseDC(control,dc);
    }
    return result;
}
void drawItem(const DRAWITEMSTRUCT& draw){
    const bool selected=(draw.itemState&ODS_SELECTED)!=0;
    fill(draw.hDC,draw.rcItem,selected?RGB(82,35,45):Field);
    if(draw.itemID==static_cast<UINT>(-1))return;
    const bool combo=draw.CtlType==ODT_COMBOBOX;
    const auto length=SendMessageW(draw.hwndItem,combo?CB_GETLBTEXTLEN:LB_GETTEXTLEN,draw.itemID,0);
    if(length<0)return;
    std::wstring value(static_cast<std::size_t>(length)+1,L'\0');SendMessageW(draw.hwndItem,combo?CB_GETLBTEXT:LB_GETTEXT,draw.itemID,reinterpret_cast<LPARAM>(value.data()));value.resize(static_cast<std::size_t>(length));
    auto label=draw.rcItem;label.left+=unit(draw.hwndItem,9);label.right-=unit(draw.hwndItem,5);
    if(roleOf(draw.hwndItem)==Role::ColorChoice){const int size=unit(draw.hwndItem,12);RECT swatch{label.left,(label.top+label.bottom-size)/2,label.left+size,(label.top+label.bottom+size)/2};fill(draw.hDC,swatch,static_cast<COLORREF>(draw.itemData));label.left+=size+unit(draw.hwndItem,8);}
    textIn(draw.hDC,draw.hwndItem,value,label,(draw.itemState&ODS_DISABLED)?Muted:Text,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX);
    if(selected){auto stripe=draw.rcItem;stripe.right=stripe.left+unit(draw.hwndItem,3);fill(draw.hDC,stripe,Accent);}
    if(draw.itemState&ODS_FOCUS){auto focus=draw.rcItem;InflateRect(&focus,-1,-1);frame(draw.hDC,focus,Gold);}
}
LRESULT CALLBACK windowProc(HWND window,UINT message,WPARAM wp,LPARAM lp,UINT_PTR,DWORD_PTR){
    switch(message){
    case WM_ERASEBKGND:return 1;
    case WM_CTLCOLORSTATIC:case WM_CTLCOLOREDIT:case WM_CTLCOLORLISTBOX:case WM_CTLCOLORBTN:{
        const auto dc=reinterpret_cast<HDC>(wp);const auto control=reinterpret_cast<HWND>(lp);
        const auto role=roleOf(control);const bool field=message==WM_CTLCOLOREDIT||message==WM_CTLCOLORLISTBOX||isClass(control,L"EDIT");
        const auto background=field?Field:controlBackground(control);
        SetTextColor(dc,!IsWindowEnabled(control)?Muted:role==Role::Title?Gold:role==Role::Muted?Muted:Text);SetBkColor(dc,background);SetBkMode(dc,TRANSPARENT);
        return reinterpret_cast<LRESULT>(field?brushes().field:background==Panel?brushes().panel:brushes().background);
    }
    case WM_DRAWITEM:{const auto& draw=*reinterpret_cast<DRAWITEMSTRUCT*>(lp);if(draw.CtlType==ODT_COMBOBOX||draw.CtlType==ODT_LISTBOX){const int saved=SaveDC(draw.hDC);drawItem(draw);RestoreDC(draw.hDC,saved);return TRUE;}break;}
    case WM_NOTIFY:{
        const auto* notify=reinterpret_cast<NMHDR*>(lp);
        if(notify&&notify->code==NM_CUSTOMDRAW&&isClass(notify->hwndFrom,L"BUTTON")){
            const auto& draw=*reinterpret_cast<NMCUSTOMDRAW*>(lp);
            if(draw.dwDrawStage==CDDS_PREPAINT){const int saved=SaveDC(draw.hdc);drawButton(notify->hwndFrom,draw.hdc);RestoreDC(draw.hdc,saved);return CDRF_SKIPDEFAULT;}
        }
        break;
    }
    case WM_MEASUREITEM:{auto& measure=*reinterpret_cast<MEASUREITEMSTRUCT*>(lp);if(measure.CtlType==ODT_COMBOBOX||measure.CtlType==ODT_LISTBOX){measure.itemHeight=static_cast<UINT>(unit(window,28));return TRUE;}break;}
    case WM_NCDESTROY:RemoveWindowSubclass(window,windowProc,WindowSubclass);break;
    }
    return DefSubclassProc(window,message,wp,lp);
}
}
void fill(HDC dc,RECT bounds,COLORREF color){SetDCBrushColor(dc,color);FillRect(dc,&bounds,reinterpret_cast<HBRUSH>(GetStockObject(DC_BRUSH)));}
void frame(HDC dc,RECT bounds,COLORREF color){SetDCBrushColor(dc,color);FrameRect(dc,&bounds,reinterpret_cast<HBRUSH>(GetStockObject(DC_BRUSH)));}
void attachWindow(HWND window){
    SetWindowSubclass(window,windowProc,WindowSubclass,0);
    SetWindowLongPtrW(window,GWL_STYLE,GetWindowLongPtrW(window,GWL_STYLE)|WS_CLIPCHILDREN);
    const BOOL dark=TRUE;DwmSetWindowAttribute(window,DWMWA_USE_IMMERSIVE_DARK_MODE,&dark,sizeof(dark));
    const COLORREF captionColor=Background,textColor=Text;
    DwmSetWindowAttribute(window,DWMWA_CAPTION_COLOR,&captionColor,sizeof(captionColor));DwmSetWindowAttribute(window,DWMWA_TEXT_COLOR,&textColor,sizeof(textColor));
}
void styleControl(HWND control,Role role){
    SetPropW(control,RoleKey,reinterpret_cast<HANDLE>(static_cast<INT_PTR>(role)));
    SetWindowSubclass(control,controlProc,ControlSubclass,0);
    if(isClass(control,L"EDIT")){
        SendMessageW(control,EM_SETMARGINS,EC_LEFTMARGIN|EC_RIGHTMARGIN,MAKELPARAM(unit(control,8),unit(control,8)));
        SetWindowTheme(control,L"",L"");
    }
    if(isClass(control,L"LISTBOX"))SetWindowTheme(control,L"",L"");
}
void setActive(HWND control,bool active){SetPropW(control,ActiveKey,reinterpret_cast<HANDLE>(static_cast<INT_PTR>(active)));InvalidateRect(control,nullptr,FALSE);}
RedrawLock::RedrawLock(HWND window):window_(window){
    const auto depth=reinterpret_cast<INT_PTR>(GetPropW(window_,LockKey));outer_=depth==0;
    SetPropW(window_,LockKey,reinterpret_cast<HANDLE>(depth+1));
    if(outer_){
        // WM_SETREDRAW(FALSE) pode retirar o foco ao remover WS_VISIBLE.
        // Capture antes; restaure somente após WM_SETREDRAW(TRUE), ainda antes da pintura.
        const auto focus=GetFocus();
        if(focus&&IsChild(window_,focus)){
            focusId_=GetDlgCtrlID(focus);editFocus_=isClass(focus,L"EDIT");
            if(editFocus_)SendMessageW(focus,EM_GETSEL,reinterpret_cast<WPARAM>(&selectionStart_),reinterpret_cast<LPARAM>(&selectionEnd_));
        }
        visible_=(GetWindowLongPtrW(window_,GWL_STYLE)&WS_VISIBLE)!=0;
        if(visible_)SendMessageW(window_,WM_SETREDRAW,FALSE,0);
    }
}
RedrawLock::~RedrawLock(){
    const auto depth=reinterpret_cast<INT_PTR>(GetPropW(window_,LockKey));
    if(outer_){
        RemovePropW(window_,LockKey);if(visible_)SendMessageW(window_,WM_SETREDRAW,TRUE,0);
        if(focusId_>0)if(const auto focus=GetDlgItem(window_,focusId_);focus&&IsWindowEnabled(focus)){
            SetFocus(focus);if(editFocus_)SendMessageW(focus,EM_SETSEL,selectionStart_,selectionEnd_);
        }
        RedrawWindow(window_,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_FRAME|RDW_UPDATENOW);
    }
    else SetPropW(window_,LockKey,reinterpret_cast<HANDLE>(depth-1));
}
}
