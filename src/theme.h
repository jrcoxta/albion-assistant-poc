#pragma once
#include <windows.h>

namespace aaapp::theme {
inline constexpr COLORREF Background=RGB(16,18,22), Panel=RGB(26,29,35), Field=RGB(18,21,26);
inline constexpr COLORREF Text=RGB(232,234,239), Muted=RGB(167,175,189), Gold=RGB(212,181,124);
inline constexpr COLORREF Accent=RGB(157,39,52), Border=RGB(65,71,82);
enum class Role {Normal, Muted, Title, Primary, Tab, ColorChoice, Badge};
void attachWindow(HWND window);
void styleControl(HWND control,Role role=Role::Normal);
void setActive(HWND control,bool active);
void fill(HDC dc,RECT bounds,COLORREF color);
void frame(HDC dc,RECT bounds,COLORREF color);

// Aninhável: makeUI pode ser parte de uma restauração maior de rascunhos.
// WM_SETREDRAW(TRUE) só é enviado quando FALSE foi enviado a uma janela visível.
class RedrawLock {
public:
    explicit RedrawLock(HWND window);
    ~RedrawLock();
    RedrawLock(const RedrawLock&)=delete;
    RedrawLock& operator=(const RedrawLock&)=delete;
private:
    HWND window_;
    bool outer_=false, visible_=false;
    int focusId_=0;
    bool editFocus_=false;
    DWORD selectionStart_=0,selectionEnd_=0;
};
}
