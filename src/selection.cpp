#include "selection.h"

#include "calibration.h"
#include "recognition.h"

#include <windowsx.h>

#include <algorithm>
#include <exception>
#include <mutex>
#include <string>

namespace aa {
namespace {

constexpr wchar_t SelectorClass[] = L"AlbionAssistantFrozenSelector";
constexpr int UseButton = 1001;
constexpr int ManualButton = 1002;
constexpr int CancelButton = 1003;
constexpr int CloseButton = 1004;
constexpr int ToolbarLogicalHeight = 92;
constexpr UINT_PTR OwnerTimer = 1;

UINT effectiveUiDpi(UINT requested, int width, int height) {
    if (requested == 0) requested = 96;
    const auto widthCap = static_cast<UINT>(
        std::max(1LL, static_cast<long long>(width) * 96 / 672));
    const auto heightCap = static_cast<UINT>(
        std::max(1LL, static_cast<long long>(height) * 96 / 330));
    return std::min({requested, widthCap, heightCap});
}

bool fits(const Region& region, int width, int height) {
    return region.valid() && region.x <= width - region.width &&
           region.y <= height - region.height;
}

std::optional<Region> manualIconRegion(POINT first, POINT second, int width, int height) {
    if (width < 24 || height < 24) return std::nullopt;

    const int left = std::min(first.x, second.x);
    const int top = std::min(first.y, second.y);
    const int extent = std::max(std::max(first.x, second.x) - left,
                                std::max(first.y, second.y) - top);
    if (extent > 256) return std::nullopt;

    const int size = std::max(24, extent);
    if (size > width || size > height) return std::nullopt;
    const int x = std::clamp(left, 0, width - size);
    const int y = std::clamp(top, 0, height - size);
    return Region{x, y, size, size};
}

Region cornersRegion(POINT first, POINT second) {
    const int left = std::min(first.x, second.x);
    const int top = std::min(first.y, second.y);
    return {left,
            top,
            std::max(first.x, second.x) - left,
            std::max(first.y, second.y) - top};
}

bool validForKind(const Region& region, SelectionKind kind, int width, int height) {
    if (!fits(region, width, height)) return false;
    if (kind == SelectionKind::Icon)
        return region.width == region.height && region.width >= 24 && region.width <= 256;
    if (kind == SelectionKind::Buffs) return region.width >= 24 && region.height >= 24;
    return region.width >= 8 && region.height >= 8;
}

struct SelectorState {
    HWND owner = nullptr;
    HWND target = nullptr;
    HWND window = nullptr;
    HWND useButton = nullptr;
    HWND manualButton = nullptr;
    HWND cancelButton = nullptr;
    HWND closeButton = nullptr;
    HFONT font = nullptr;
    bool ownsFont = false;
    UINT dpi = 96;
    const Image* snapshot = nullptr;
    const Recognizer* recognizer = nullptr;
    POINT origin{};
    POINT first{};
    POINT cursor{};
    SelectionKind kind = SelectionKind::Buffs;
    bool manual = false;
    bool hasFirst = false;
    bool toolbarAtBottom = false;
    std::optional<Region> candidate;
    std::optional<Region> result;
    std::wstring status;

    int px(int logical) const {
        return MulDiv(logical, static_cast<int>(dpi), 96);
    }

    int toolbarHeight() const {
        return px(ToolbarLogicalHeight);
    }

    int toolbarTop() const {
        return toolbarAtBottom ? std::max(0, snapshot->height - toolbarHeight()) : 0;
    }

    void updateStatus(std::wstring text) {
        status = std::move(text);
        if (window) InvalidateRect(window, nullptr, FALSE);
    }

    void updateUseButton() const {
        if (useButton) EnableWindow(useButton, candidate.has_value());
    }

    void setCandidate(std::optional<Region> region) {
        candidate = std::move(region);
        hasFirst = false;
        updateUseButton();
        if (window) InvalidateRect(window, nullptr, FALSE);
    }

    void selectManually() {
        manual = true;
        setCandidate(std::nullopt);
        updateStatus(L"Clique no primeiro canto. Depois, clique no canto oposto.");
        if (window) SetFocus(window);
    }

    void moveToolbar();

    bool sameTargetGeometry() const {
        if (!IsWindow(target)) return false;
        RECT client{};
        POINT currentOrigin{};
        return GetClientRect(target, &client) && ClientToScreen(target, &currentOrigin) &&
               client.left == 0 && client.top == 0 && client.right == snapshot->width &&
               client.bottom == snapshot->height && currentOrigin.x == origin.x &&
               currentOrigin.y == origin.y;
    }

    void accept() {
        if (!candidate || !validForKind(*candidate, kind, snapshot->width, snapshot->height)) {
            updateStatus(L"Faça uma seleção válida antes de confirmar.");
            return;
        }
        if (!sameTargetGeometry()) {
            MessageBoxW(window,
                        L"A janela do jogo mudou de tamanho ou posição. Capture outra imagem e recalibre.",
                        L"Seleção cancelada",
                        MB_OK | MB_ICONWARNING);
            result.reset();
            DestroyWindow(window);
            return;
        }
        result = candidate;
        DestroyWindow(window);
    }

    void cancel() {
        result.reset();
        if (window) DestroyWindow(window);
    }

    void click(POINT point) {
        if (point.x < 0 || point.y < 0 || point.x >= snapshot->width ||
            point.y >= snapshot->height)
            return;
        const int panelWidth = std::min(px(640), std::max(0, snapshot->width - px(32)));
        const int panelLeft = (snapshot->width - panelWidth) / 2;
        const int panelTop = toolbarTop();
        if (point.y >= panelTop + px(8) && point.y < panelTop + toolbarHeight() &&
            point.x >= panelLeft &&
            point.x < panelLeft + panelWidth)
            return;

        if (kind == SelectionKind::Icon && !manual) {
            if (!recognizer) {
                updateStatus(L"Reconhecimento indisponível. Use Ajustar manual ou pressione M.");
                return;
            }
            std::optional<Region> suggested;
            try {
                suggested = suggestIcon(*snapshot, point.x, point.y, *recognizer);
            } catch (const std::exception&) {
                setCandidate(std::nullopt);
                updateStatus(L"Não foi possível analisar este ponto. Use Ajustar manual ou pressione M.");
                return;
            }
            if (suggested && validForKind(*suggested, kind, snapshot->width, snapshot->height)) {
                setCandidate(std::move(suggested));
                updateStatus(L"Sugestão encontrada. Confira a prévia e confirme em Usar seleção.");
            } else {
                setCandidate(std::nullopt);
                updateStatus(L"Ícone não reconhecido com segurança. Use Ajustar manual ou pressione M.");
            }
            return;
        }

        if (candidate) setCandidate(std::nullopt);
        if (!hasFirst) {
            first = point;
            hasFirst = true;
            updateStatus(L"Agora clique no canto oposto. Esc cancela.");
            return;
        }

        std::optional<Region> selected;
        if (kind == SelectionKind::Icon)
            selected = manualIconRegion(first, point, snapshot->width, snapshot->height);
        else
            selected = cornersRegion(first, point);

        if (!selected || !validForKind(*selected, kind, snapshot->width, snapshot->height)) {
            hasFirst = false;
            if (kind == SelectionKind::Icon)
                updateStatus(L"O ícone deve formar um quadrado de 24 a 256 px. Selecione novamente.");
            else if (kind == SelectionKind::Buffs)
                updateStatus(L"A área dos buffs precisa comportar pelo menos um ícone. Selecione novamente.");
            else
                updateStatus(L"A área é pequena demais. Selecione novamente.");
            return;
        }

        setCandidate(std::move(selected));
        updateStatus(L"Confira a área e a prévia. Confirme em Usar seleção.");
    }
};

void setControlFont(HWND control, HFONT font) {
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
}

struct ControlLayout {
    RECT use{};
    RECT manual{};
    RECT cancel{};
    RECT close{};
};

RECT controlRect(int x, int y, int width, int height) {
    return {x, y, x + width, y + height};
}

ControlLayout controlLayout(const SelectorState& state, int width) {
    const bool icon = state.kind == SelectionKind::Icon;
    const int gap = state.px(10);
    const int useWidth = state.px(128);
    const int manualWidth = state.px(132);
    const int cancelWidth = state.px(92);
    const int total = useWidth + cancelWidth + gap + (icon ? manualWidth + gap : 0);
    int x = std::max(state.px(8), (width - total) / 2);
    const int y = state.toolbarTop() + state.px(50);
    const int height = state.px(30);
    ControlLayout layout{};
    layout.use = controlRect(x, y, useWidth, height);
    x += useWidth + gap;
    if (icon) {
        layout.manual = controlRect(x, y, manualWidth, height);
        x += manualWidth + gap;
    }
    layout.cancel = controlRect(x, y, cancelWidth, height);
    layout.close = controlRect(std::max(state.px(4), width - state.px(42)),
                               state.toolbarTop() + state.px(8),
                               state.px(34),
                               state.px(28));
    return layout;
}

void positionControl(HWND control, const RECT& rect) {
    SetWindowPos(control,
                 nullptr,
                 rect.left,
                 rect.top,
                 rect.right - rect.left,
                 rect.bottom - rect.top,
                 SWP_NOZORDER);
}

void layoutControls(SelectorState& state, int width) {
    const auto layout = controlLayout(state, width);
    positionControl(state.useButton, layout.use);
    if (state.kind == SelectionKind::Icon) positionControl(state.manualButton, layout.manual);
    positionControl(state.cancelButton, layout.cancel);
    positionControl(state.closeButton, layout.close);
}

void SelectorState::moveToolbar() {
    toolbarAtBottom = !toolbarAtBottom;
    layoutControls(*this, snapshot->width);
    if (window) InvalidateRect(window, nullptr, FALSE);
}

void createControls(SelectorState& state) {
    const HINSTANCE instance = GetModuleHandleW(nullptr);
    state.useButton = CreateWindowExW(0,
                                      L"BUTTON",
                                      L"Usar seleção",
                                      WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                                      0,
                                      0,
                                      0,
                                      0,
                                      state.window,
                                      reinterpret_cast<HMENU>(static_cast<INT_PTR>(UseButton)),
                                      instance,
                                      nullptr);
    state.manualButton = CreateWindowExW(0,
                                         L"BUTTON",
                                         L"Ajustar manual",
                                         WS_CHILD | WS_TABSTOP |
                                             (state.kind == SelectionKind::Icon ? WS_VISIBLE : 0U),
                                         0,
                                         0,
                                         0,
                                         0,
                                         state.window,
                                         reinterpret_cast<HMENU>(static_cast<INT_PTR>(ManualButton)),
                                         instance,
                                         nullptr);
    state.cancelButton = CreateWindowExW(0,
                                         L"BUTTON",
                                         L"Cancelar",
                                         WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                         0,
                                         0,
                                         0,
                                         0,
                                         state.window,
                                         reinterpret_cast<HMENU>(static_cast<INT_PTR>(CancelButton)),
                                         instance,
                                         nullptr);
    state.closeButton = CreateWindowExW(0,
                                        L"BUTTON",
                                        L"×",
                                        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                        0,
                                        0,
                                        0,
                                        0,
                                        state.window,
                                        reinterpret_cast<HMENU>(static_cast<INT_PTR>(CloseButton)),
                                        instance,
                                        nullptr);
    setControlFont(state.useButton, state.font);
    setControlFont(state.manualButton, state.font);
    setControlFont(state.cancelButton, state.font);
    setControlFont(state.closeButton, state.font);
    state.updateUseButton();
    layoutControls(state, state.snapshot->width);
}

void paintSnapshot(HDC dc, const Image& image) {
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = image.width;
    info.bmiHeader.biHeight = -image.height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    SetDIBitsToDevice(dc,
                      0,
                      0,
                      static_cast<DWORD>(image.width),
                      static_cast<DWORD>(image.height),
                      0,
                      0,
                      0,
                      static_cast<UINT>(image.height),
                      image.bgra.data(),
                      &info,
                      DIB_RGB_COLORS);
}

void paintSelection(HDC dc, const SelectorState& state) {
    Region shown{};
    bool hasShown = false;
    if (state.candidate) {
        shown = *state.candidate;
        hasShown = true;
    } else if (state.hasFirst) {
        shown = cornersRegion(state.first, state.cursor);
        hasShown = true;
    }

    const COLORREF accent = state.kind == SelectionKind::Highlight ? RGB(255, 190, 30)
                                                                   : RGB(0, 238, 210);
    HPEN pen = CreatePen(PS_SOLID, 3, accent);
    const HGDIOBJ previousPen = SelectObject(dc, pen);
    const HGDIOBJ previousBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    if (hasShown)
        Rectangle(dc, shown.x, shown.y, shown.x + shown.width, shown.y + shown.height);
    else {
        MoveToEx(dc, state.cursor.x - 12, state.cursor.y, nullptr);
        LineTo(dc, state.cursor.x + 13, state.cursor.y);
        MoveToEx(dc, state.cursor.x, state.cursor.y - 12, nullptr);
        LineTo(dc, state.cursor.x, state.cursor.y + 13);
    }
    SelectObject(dc, previousBrush);
    SelectObject(dc, previousPen);
    DeleteObject(pen);
}

bool previewFits(const SelectorState& state) {
    const int preview = state.px(176);
    const int y = state.toolbarAtBottom ? state.px(18)
                                        : state.toolbarHeight() + state.px(18);
    return y + preview + state.px(28) < state.snapshot->height;
}

void paintPreview(HDC dc, const SelectorState& state, const BITMAPINFO& info) {
    if (!state.candidate) return;
    const int preview = state.px(176);
    const int margin = state.px(18);
    const int x = std::max(margin, state.snapshot->width - preview - margin);
    const int y = state.toolbarAtBottom ? state.px(18)
                                        : state.toolbarHeight() + state.px(18);
    if (!previewFits(state)) return;

    RECT background{x - state.px(6),
                    y - state.px(6),
                    x + preview + state.px(6),
                    y + preview + state.px(28)};
    HBRUSH brush = CreateSolidBrush(RGB(17, 19, 24));
    FillRect(dc, &background, brush);
    DeleteObject(brush);
    int previewWidth = preview;
    int previewHeight = preview;
    if (state.candidate->width > state.candidate->height) {
        previewHeight = std::max(
            1,
            static_cast<int>(static_cast<long long>(preview) * state.candidate->height /
                             state.candidate->width));
    } else if (state.candidate->height > state.candidate->width) {
        previewWidth = std::max(
            1,
            static_cast<int>(static_cast<long long>(preview) * state.candidate->width /
                             state.candidate->height));
    }
    const int previewX = x + (preview - previewWidth) / 2;
    const int previewY = y + (preview - previewHeight) / 2;
    SetStretchBltMode(dc, COLORONCOLOR);
    StretchDIBits(dc,
                  previewX,
                  previewY,
                  previewWidth,
                  previewHeight,
                  state.candidate->x,
                  state.candidate->y,
                  state.candidate->width,
                  state.candidate->height,
                  state.snapshot->bgra.data(),
                  &info,
                  DIB_RGB_COLORS,
                  SRCCOPY);

    const std::wstring dimensions = std::to_wstring(state.candidate->width) + L" × " +
                                    std::to_wstring(state.candidate->height) + L" px";
    RECT label{x,
               y + preview + state.px(3),
               x + preview,
               y + preview + state.px(24)};
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(245, 245, 245));
    DrawTextW(dc,
              dimensions.c_str(),
              static_cast<int>(dimensions.size()),
              &label,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void paint(SelectorState& state) {
    PAINTSTRUCT paintState{};
    HDC dc = BeginPaint(state.window, &paintState);
    paintSnapshot(dc, *state.snapshot);
    paintSelection(dc, state);

    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = state.snapshot->width;
    info.bmiHeader.biHeight = -state.snapshot->height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    paintPreview(dc, state, info);

    const int panelWidth =
        std::min(state.px(640), std::max(0, state.snapshot->width - state.px(32)));
    const int panelLeft = (state.snapshot->width - panelWidth) / 2;
    const int panelTop = state.toolbarTop();
    RECT panel{panelLeft,
               panelTop + state.px(8),
               panelLeft + panelWidth,
               panelTop + state.toolbarHeight()};
    HBRUSH brush = CreateSolidBrush(RGB(17, 19, 24));
    FillRect(dc, &panel, brush);
    DeleteObject(brush);
    RECT textRect{panel.left + state.px(12),
                  panel.top + state.px(5),
                  panel.right - state.px(12),
                  panel.top + state.px(42)};
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(245, 245, 245));
    SelectObject(dc, state.font);
    const std::wstring displayed = L"F2: mover painel  ·  " + state.status;
    DrawTextW(dc,
              displayed.c_str(),
              static_cast<int>(displayed.size()),
              &textRect,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    EndPaint(state.window, &paintState);
}

LRESULT CALLBACK selectorProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<SelectorState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        state = static_cast<SelectorState*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        state->window = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    }
    if (!state) return DefWindowProcW(window, message, wParam, lParam);

    switch (message) {
    case WM_CREATE:
        state->dpi = effectiveUiDpi(GetDpiForWindow(window),
                                    state->snapshot->width,
                                    state->snapshot->height);
        state->font = CreateFontW(-state->px(15),
                                  0,
                                  0,
                                  0,
                                  FW_NORMAL,
                                  FALSE,
                                  FALSE,
                                  FALSE,
                                  DEFAULT_CHARSET,
                                  OUT_DEFAULT_PRECIS,
                                  CLIP_DEFAULT_PRECIS,
                                  CLEARTYPE_QUALITY,
                                  DEFAULT_PITCH | FF_DONTCARE,
                                  L"Segoe UI");
        state->ownsFont = state->font != nullptr;
        if (!state->font)
            state->font = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        createControls(*state);
        SetTimer(window, OwnerTimer, 100, nullptr);
        return 0;
    case WM_SIZE:
        layoutControls(*state, LOWORD(lParam));
        return 0;
    case WM_MOUSEMOVE:
        state->cursor = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        InvalidateRect(window, nullptr, FALSE);
        return 0;
    case WM_LBUTTONDOWN:
        state->click({GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});
        return 0;
    case WM_COMMAND:
        if (HIWORD(wParam) == BN_CLICKED) {
            if (LOWORD(wParam) == UseButton)
                state->accept();
            else if (LOWORD(wParam) == ManualButton)
                state->selectManually();
            else if (LOWORD(wParam) == CancelButton || LOWORD(wParam) == CloseButton)
                state->cancel();
        }
        return 0;
    case WM_TIMER:
        if (wParam == OwnerTimer && state->owner && !IsWindow(state->owner)) state->cancel();
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        paint(*state);
        return 0;
    case WM_CLOSE:
        state->cancel();
        return 0;
    case WM_DESTROY:
        KillTimer(window, OwnerTimer);
        if (state->ownsFont) DeleteObject(state->font);
        state->font = nullptr;
        state->ownsFont = false;
        state->window = nullptr;
        return 0;
    default:
        return DefWindowProcW(window, message, wParam, lParam);
    }
}

bool registerSelectorClass() {
    static std::once_flag once;
    static bool registered = false;
    std::call_once(once, [] {
        WNDCLASSEXW windowClass{};
        windowClass.cbSize = sizeof(windowClass);
        windowClass.lpfnWndProc = selectorProc;
        windowClass.hInstance = GetModuleHandleW(nullptr);
        windowClass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32515));
        windowClass.lpszClassName = SelectorClass;
        registered = RegisterClassExW(&windowClass) != 0 ||
                     GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
    });
    return registered;
}

struct OwnerRestore {
    HWND owner = nullptr;
    bool wasEnabled = false;
    ~OwnerRestore() {
        if (!IsWindow(owner)) return;
        EnableWindow(owner, wasEnabled ? TRUE : FALSE);
        ShowWindow(owner, SW_SHOW);
        SetForegroundWindow(owner);
    }
};

struct DpiRestore {
    DPI_AWARENESS_CONTEXT previous = nullptr;
    ~DpiRestore() {
        if (previous) SetThreadDpiAwarenessContext(previous);
    }
};

} // namespace

std::optional<Region> selectRegion(HWND owner,
                                   HWND target,
                                   const Image& snapshot,
                                   POINT origin,
                                   SelectionKind kind,
                                   const Recognizer* recognizer) {
    OwnerRestore ownerRestore{owner, IsWindow(owner) && IsWindowEnabled(owner)};
    if (IsWindow(owner)) EnableWindow(owner, FALSE);
    if (!snapshot.valid() || !IsWindow(target) || !registerSelectorClass()) return std::nullopt;

    DpiRestore dpiRestore{SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)};
    SelectorState state{};
    state.owner = owner;
    state.target = target;
    state.snapshot = &snapshot;
    state.recognizer = recognizer;
    state.origin = origin;
    state.kind = kind;
    state.manual = kind != SelectionKind::Icon;
    state.cursor = {snapshot.width / 2, snapshot.height / 2};
    if (kind == SelectionKind::Icon)
        state.status = L"Clique no ícone para buscar automaticamente. M: ajuste manual · Esc: cancelar.";
    else
        state.status = L"Clique no primeiro canto. Depois, clique no canto oposto.";

    const HINSTANCE instance = GetModuleHandleW(nullptr);
    HWND window = CreateWindowExW(WS_EX_TOPMOST | WS_EX_APPWINDOW,
                                  SelectorClass,
                                  L"Selecionar região",
                                  WS_POPUP | WS_CLIPCHILDREN,
                                  origin.x,
                                  origin.y,
                                  snapshot.width,
                                  snapshot.height,
                                  nullptr,
                                  nullptr,
                                  instance,
                                  &state);
    if (!window) return std::nullopt;

    ShowWindow(window, SW_SHOW);
    UpdateWindow(window);
    SetForegroundWindow(window);
    SetFocus(window);

    bool repostQuit = false;
    int quitCode = 0;
    MSG message{};
    while (state.window) {
        const BOOL fetched = GetMessageW(&message, nullptr, 0, 0);
        if (fetched <= 0) {
            if (fetched == 0) {
                repostQuit = true;
                quitCode = static_cast<int>(message.wParam);
            }
            if (state.window) DestroyWindow(state.window);
            break;
        }

        const bool belongsToSelector = message.hwnd == state.window ||
                                       (message.hwnd && IsChild(state.window, message.hwnd));
        if (belongsToSelector && message.message == WM_KEYDOWN) {
            if (message.wParam == VK_F2) {
                state.moveToolbar();
                continue;
            }
            if (message.wParam == VK_ESCAPE) {
                state.cancel();
                continue;
            }
            if (message.wParam == VK_RETURN) {
                state.accept();
                continue;
            }
            if (kind == SelectionKind::Icon && message.wParam == 'M') {
                state.selectManually();
                continue;
            }
        }
        if (belongsToSelector && message.message == WM_SYSKEYDOWN &&
            message.wParam == VK_F4) {
            state.cancel();
            continue;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    if (repostQuit) PostQuitMessage(quitCode);
    return state.result;
}

} // namespace aa
