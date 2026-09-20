#include "../src/selection.cpp"

#include <chrono>
#include <stdexcept>
#include <thread>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

HWND waitForSelector() {
    for (int attempt = 0; attempt < 200; ++attempt) {
        if (HWND selector = FindWindowW(L"AlbionAssistantFrozenSelector", nullptr))
            return selector;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    std::terminate();
}

void postSelector(UINT message, WPARAM wParam = 0, LPARAM lParam = 0) {
    require(PostMessageW(waitForSelector(), message, wParam, lParam) != FALSE,
            "não foi possível enviar mensagem ao seletor");
}

} // namespace

int main() {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    const auto ordinary = aa::manualIconRegion({10, 20}, {50, 45}, 200, 100);
    require(ordinary && ordinary->x == 10 && ordinary->y == 20,
            "origem do quadrado manual incorreta");
    require(ordinary->width == 40 && ordinary->height == 40,
            "seleção manual não ficou quadrada");

    const auto nearEdge = aa::manualIconRegion({95, 75}, {99, 79}, 100, 80);
    require(nearEdge && nearEdge->x == 76 && nearEdge->y == 56,
            "quadrado não foi deslocado para dentro da imagem");
    require(nearEdge->width == 24 && nearEdge->height == 24,
            "tamanho mínimo do ícone não foi aplicado");
    require(!aa::manualIconRegion({0, 0}, {300, 20}, 400, 200),
            "ícone maior que 256 px foi aceito");
    require(!aa::manualIconRegion({0, 0}, {10, 10}, 20, 20),
            "imagem menor que um ícone foi aceita");
    require(!aa::manualIconRegion({10, 0}, {20, 150}, 100, 200),
            "quadrado impossível de manter nos limites foi aceito");

    constexpr int width = 800;
    constexpr int height = 600;
    aa::Image layoutImage{width, height, {}};
    aa::SelectorState scale{};
    scale.snapshot = &layoutImage;
    scale.kind = aa::SelectionKind::Icon;
    for (const UINT requested : {192U, 240U}) {
        scale.dpi = aa::effectiveUiDpi(requested, width, height);
        require(scale.dpi == 114, "DPI visual não foi limitado pelo espaço disponível");
        const auto controls = aa::controlLayout(scale, width);
        const auto inside = [](const RECT& rect) {
            return rect.left >= 0 && rect.top >= 0 && rect.right <= width &&
                   rect.bottom <= height && rect.right > rect.left && rect.bottom > rect.top;
        };
        require(inside(controls.use) && inside(controls.manual) && inside(controls.cancel) &&
                    inside(controls.close),
                "controles ficaram fora do snapshot em DPI alto");
        require(aa::previewFits(scale), "prévia desapareceu em snapshot 800x600 e DPI alto");
        scale.toolbarAtBottom = true;
        const auto bottomControls = aa::controlLayout(scale, width);
        require(inside(bottomControls.use) && inside(bottomControls.manual) &&
                    inside(bottomControls.cancel) && inside(bottomControls.close),
                "controles no rodapé ficaram fora do snapshot em DPI alto");
        require(aa::previewFits(scale), "prévia desapareceu com painel no rodapé");
        scale.toolbarAtBottom = false;
    }
    require(aa::effectiveUiDpi(192, 1920, 1080) == 192,
            "DPI foi reduzido mesmo com espaço suficiente");

    HWND owner = CreateWindowExW(0,
                                 L"STATIC",
                                 L"owner",
                                 WS_POPUP | WS_VISIBLE,
                                 20,
                                 20,
                                 80,
                                 40,
                                 nullptr,
                                 nullptr,
                                 GetModuleHandleW(nullptr),
                                 nullptr);
    HWND target = CreateWindowExW(0,
                                  L"STATIC",
                                  L"target",
                                  WS_POPUP | WS_VISIBLE,
                                  120,
                                  100,
                                  width,
                                  height,
                                  nullptr,
                                  nullptr,
                                  GetModuleHandleW(nullptr),
                                  nullptr);
    require(owner && target, "janelas do harness não foram criadas");
    ShowWindow(owner, SW_HIDE);
    POINT origin{};
    require(ClientToScreen(target, &origin) != FALSE, "origem do alvo indisponível");
    aa::Image image{width,
                    height,
                    std::vector<std::uint8_t>(static_cast<std::size_t>(width) * height * 4)};
    aa::SelectorState geometry{};
    geometry.target = target;
    geometry.snapshot = &image;
    geometry.origin = origin;
    require(geometry.sameTargetGeometry(), "geometria estável foi rejeitada");

    bool topLevelAppWindow = false;
    std::jthread confirmer([&] {
        HWND selector = waitForSelector();
        topLevelAppWindow = GetWindow(selector, GW_OWNER) == nullptr &&
                            (GetWindowLongPtrW(selector, GWL_EXSTYLE) & WS_EX_APPWINDOW) != 0;
        PostMessageW(selector, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(20, 300));
        PostMessageW(selector, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(100, 400));
        PostMessageW(selector, WM_KEYDOWN, VK_RETURN, 0);
    });
    const auto selected =
        aa::selectRegion(owner, target, image, origin, aa::SelectionKind::Highlight, nullptr);
    confirmer.join();
    require(topLevelAppWindow, "seletor não é uma janela top level WS_EX_APPWINDOW");
    require(selected && selected->x == 20 && selected->y == 300,
            "origem da seleção confirmada incorreta");
    require(selected->width == 80 && selected->height == 100,
            "dimensões da seleção confirmada incorretas");
    require(IsWindowVisible(owner) != FALSE, "owner não reapareceu após confirmação");

    ShowWindow(owner, SW_HIDE);
    std::jthread toolbarMover([] {
        HWND selector = waitForSelector();
        PostMessageW(selector, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(300, 20));
        PostMessageW(selector, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(400, 40));
        PostMessageW(selector, WM_KEYDOWN, VK_F2, 0);
        PostMessageW(selector, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(300, 20));
        PostMessageW(selector, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(400, 40));
        PostMessageW(selector, WM_KEYDOWN, VK_RETURN, 0);
    });
    const auto movedSelection =
        aa::selectRegion(owner, target, image, origin, aa::SelectionKind::Highlight, nullptr);
    toolbarMover.join();
    require(movedSelection && movedSelection->x == 300 && movedSelection->y == 20 &&
                movedSelection->width == 100 && movedSelection->height == 20,
            "F2 não liberou a mesma área antes coberta pelo painel");

    ShowWindow(owner, SW_HIDE);
    std::jthread escCanceller([] { postSelector(WM_KEYDOWN, VK_ESCAPE); });
    require(!aa::selectRegion(owner, target, image, origin, aa::SelectionKind::Buffs, nullptr),
            "Esc não cancelou");
    escCanceller.join();
    require(IsWindowVisible(owner) != FALSE, "owner não reapareceu após Esc");

    ShowWindow(owner, SW_HIDE);
    std::jthread altF4Canceller([] { postSelector(WM_SYSKEYDOWN, VK_F4); });
    require(!aa::selectRegion(owner, target, image, origin, aa::SelectionKind::Buffs, nullptr),
            "Alt+F4 não cancelou");
    altF4Canceller.join();
    require(IsWindowVisible(owner) != FALSE, "owner não reapareceu após Alt+F4");

    ShowWindow(owner, SW_HIDE);
    std::jthread closeButtonCanceller([] {
        postSelector(WM_COMMAND, MAKEWPARAM(aa::CloseButton, BN_CLICKED));
    });
    require(!aa::selectRegion(owner, target, image, origin, aa::SelectionKind::Buffs, nullptr),
            "botão de fechar não cancelou");
    closeButtonCanceller.join();
    require(IsWindowVisible(owner) != FALSE, "owner não reapareceu após botão de fechar");

    ShowWindow(owner, SW_HIDE);
    std::jthread ownerDestroyer([owner] {
        waitForSelector();
        PostMessageW(owner, WM_CLOSE, 0, 0);
    });
    require(!aa::selectRegion(owner, target, image, origin, aa::SelectionKind::Buffs, nullptr),
            "destruição do owner não encerrou o seletor por conta própria");
    ownerDestroyer.join();
    require(IsWindow(owner) == FALSE, "owner destruído foi reaberto");

    DestroyWindow(target);
}
