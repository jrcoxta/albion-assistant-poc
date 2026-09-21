#include "../src/selection.cpp"

#include <objbase.h>

#include <chrono>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << std::endl;
        throw std::runtime_error(message);
    }
}

HWND waitForSelector() {
    for (int attempt = 0; attempt < 200; ++attempt) {
        HWND selector = nullptr;
        while ((selector = FindWindowExW(nullptr, selector, L"AlbionAssistantFrozenSelector", nullptr))) {
            DWORD processId = 0;
            GetWindowThreadProcessId(selector, &processId);
            if (processId == GetCurrentProcessId() && IsWindowVisible(selector) &&
                GetDlgItem(selector, aa::UseButton))
                return selector;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    std::terminate();
}

void postSelector(UINT message, WPARAM wParam = 0, LPARAM lParam = 0) {
    require(PostMessageW(waitForSelector(), message, wParam, lParam) != FALSE,
            "não foi possível enviar mensagem ao seletor");
}

void saveSelectorSnapshot(HWND selector, const std::filesystem::path& output) {
    RECT client{};
    require(GetClientRect(selector, &client) != FALSE, "tamanho do seletor indisponível");
    aa::Image captured{client.right, client.bottom,
                       std::vector<std::uint8_t>(static_cast<std::size_t>(client.right) *
                                                  client.bottom * 4)};
    HDC dc = CreateCompatibleDC(nullptr);
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = captured.width;
    info.bmiHeader.biHeight = -captured.height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    void* pixels = nullptr;
    HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    require(dc && bitmap && pixels, "superfície da captura indisponível");
    HGDIOBJ previous = SelectObject(dc, bitmap);
    const bool printed = PrintWindow(selector, dc, PW_CLIENTONLY) != FALSE;
    GdiFlush();
    std::memcpy(captured.bgra.data(), pixels, captured.bgra.size());
    SelectObject(dc, previous);
    DeleteObject(bitmap);
    DeleteDC(dc);
    require(printed, "PrintWindow não desenhou a janela do seletor");
    for (std::size_t index = 3; index < captured.bgra.size(); index += 4)
        captured.bgra[index] = 255;
    require(SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)),
            "WIC não pôde inicializar para gravar a captura");
    aa::saveImage(captured, output);
    CoUninitialize();
}

} // namespace

int main(int argc, char** argv) {
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

    for (const POINT edge : {POINT{103, 104}, POINT{97, 104}, POINT{103, 96}, POINT{97, 96}}) {
        const auto circle = aa::circleRegion({100, 100}, edge);
        require(circle.x == 95 && circle.y == 95 && circle.width == 10 && circle.height == 10 &&
                    circle.shape == aa::RegionShape::Circle,
                "raio não usa a distância euclidiana em todos os quadrantes");
        require(aa::validForKind(circle, aa::SelectionKind::Highlight, 200, 200),
                "círculo de destaque válido foi rejeitado");
        require(!aa::validForKind(circle, aa::SelectionKind::Icon, 200, 200) &&
                    !aa::validForKind(circle, aa::SelectionKind::Buffs, 200, 200),
                "círculo menor que 24 px foi aceito como ícone/leitura");
    }
    const auto rounded = aa::circleRegion({50, 50}, {60, 60});
    require(rounded.x == 36 && rounded.y == 36 && rounded.width == 28,
            "raio diagonal não foi arredondado ao pixel mais próximo");
    const auto boundary = aa::circleRegion({12, 12}, {24, 12});
    require(aa::validForKind(boundary, aa::SelectionKind::Icon, 24, 24),
            "círculo de 24 px tangente aos limites foi rejeitado");
    for (const POINT center : {POINT{11, 12}, POINT{12, 11}, POINT{13, 12}, POINT{12, 13}}) {
        const auto outside = aa::circleRegion(center, {center.x + 12, center.y});
        require(!aa::validForKind(outside, aa::SelectionKind::Icon, 24, 24),
                "círculo fora da imagem foi aceito ou teve seu centro deslocado");
    }
    require(!aa::validForKind(aa::circleRegion({100, 100}, {100, 100}),
                              aa::SelectionKind::Highlight, 400, 400),
            "círculo sem raio foi aceito");
    require(!aa::validForKind(aa::circleRegion({100, 100}, {103, 100}),
                              aa::SelectionKind::Highlight, 400, 400),
            "círculo de destaque menor que 8 px foi aceito");
    require(aa::validForKind(aa::circleRegion({128, 128}, {256, 128}),
                             aa::SelectionKind::Icon, 300, 300) &&
                !aa::validForKind(aa::circleRegion({129, 129}, {258, 129}),
                                  aa::SelectionKind::Icon, 300, 300),
            "limite de 256 px do ícone circular não foi respeitado");

    constexpr int width = 800;
    constexpr int height = 600;
    aa::Image layoutImage{width, height, {}};
    aa::SelectorState scale{};
    scale.snapshot = &layoutImage;
    scale.kind = aa::SelectionKind::Icon;
    scale.shape = aa::RegionShape::Circle;
    for (const UINT requested : {192U, 240U}) {
        scale.dpi = aa::effectiveUiDpi(requested, width, height);
        require(scale.dpi == 114, "DPI visual não foi limitado pelo espaço disponível");
        const auto controls = aa::controlLayout(scale, width);
        const auto inside = [](const RECT& rect) {
            return rect.left >= 0 && rect.top >= 0 && rect.right <= width &&
                   rect.bottom <= height && rect.right > rect.left && rect.bottom > rect.top;
        };
        require(inside(controls.use) && inside(controls.manual) && inside(controls.cancel) &&
                    inside(controls.close) && inside(controls.rectangle) && inside(controls.circle),
                "controles ficaram fora do snapshot em DPI alto");
        require(aa::previewFits(scale), "prévia desapareceu em snapshot 800x600 e DPI alto");
        scale.toolbarAtBottom = true;
        const auto bottomControls = aa::controlLayout(scale, width);
        require(inside(bottomControls.use) && inside(bottomControls.manual) &&
                    inside(bottomControls.cancel) && inside(bottomControls.close) &&
                    inside(bottomControls.rectangle) && inside(bottomControls.circle),
                "controles no rodapé ficaram fora do snapshot em DPI alto");
        require(aa::previewFits(scale), "prévia desapareceu com painel no rodapé");
        scale.toolbarAtBottom = false;
    }
    require(aa::effectiveUiDpi(192, 1920, 1080) == 192,
            "DPI foi reduzido mesmo com espaço suficiente");

    aa::SelectorState switching{};
    switching.snapshot = &layoutImage;
    switching.kind = aa::SelectionKind::Icon;
    switching.candidate = aa::Region{200, 200, 24, 24};
    switching.hasFirst = true;
    switching.setShape(aa::RegionShape::Circle);
    require(!switching.candidate && !switching.hasFirst && switching.manual && !switching.result,
            "trocar forma não reiniciou a seleção pendente sem confirmar");
    switching.click({300, 300});
    switching.click({330, 340});
    require(switching.candidate && switching.candidate->x == 250 &&
                switching.candidate->y == 250 && switching.candidate->width == 100 &&
                switching.candidate->shape == aa::RegionShape::Circle,
            "dois cliques no modo circular não usaram centro e raio");
    switching.selectManually();
    require(!switching.candidate && !switching.hasFirst &&
                switching.shape == aa::RegionShape::Circle,
            "Recomeçar perdeu a forma circular ou manteve a seleção anterior");
    switching.click({300, 300});
    switching.setShape(aa::RegionShape::Rectangle);
    require(!switching.hasFirst && !switching.candidate && !switching.result,
            "trocar forma após primeiro clique manteve um centro pendente");

    HDC drawing = CreateCompatibleDC(nullptr);
    BITMAPINFO drawingInfo{};
    drawingInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    drawingInfo.bmiHeader.biWidth = width;
    drawingInfo.bmiHeader.biHeight = -height;
    drawingInfo.bmiHeader.biPlanes = 1;
    drawingInfo.bmiHeader.biBitCount = 32;
    void* drawingPixels = nullptr;
    HBITMAP bitmap = CreateDIBSection(drawing, &drawingInfo, DIB_RGB_COLORS,
                                      &drawingPixels, nullptr, 0);
    require(drawing && bitmap && drawingPixels, "superfície de desenho não foi criada");
    HGDIOBJ oldBitmap = SelectObject(drawing, bitmap);
    PatBlt(drawing, 0, 0, width, height, BLACKNESS);
    aa::SelectorState circularDrawing{};
    circularDrawing.snapshot = &layoutImage;
    circularDrawing.kind = aa::SelectionKind::Highlight;
    circularDrawing.shape = aa::RegionShape::Circle;
    circularDrawing.candidate = aa::Region{100, 100, 100, 100, aa::RegionShape::Circle};
    aa::paintSelection(drawing, circularDrawing);
    require(GetPixel(drawing, 150, 100) == RGB(255, 190, 30) &&
                GetPixel(drawing, 150, 150) == RGB(255, 190, 30),
            "desenho circular não mostra aro e guia central");
    require(GetPixel(drawing, 100, 100) != RGB(255, 190, 30),
            "círculo foi desenhado como retângulo");
    aa::Image previewImage{width, height,
                           std::vector<std::uint8_t>(static_cast<std::size_t>(width) * height * 4,
                                                      120)};
    circularDrawing.snapshot = &previewImage;
    circularDrawing.kind = aa::SelectionKind::Icon;
    aa::paintPreview(drawing, circularDrawing, drawingInfo);
    require(GetPixel(drawing, 694, 146) == RGB(0, 238, 210) &&
                GetPixel(drawing, 694, 234) == RGB(0, 238, 210),
            "prévia não mostra o aro circular e o centro");
    require(GetPixel(drawing, 616, 156) == RGB(120, 120, 120),
            "prévia apagou os pixels fora do aro que podem conter stacks");
    SelectObject(drawing, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(drawing);

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
    bool shapeControlsVisible = false;
    std::jthread confirmer([&] {
        HWND selector = waitForSelector();
        topLevelAppWindow = GetWindow(selector, GW_OWNER) == nullptr &&
                            (GetWindowLongPtrW(selector, GWL_EXSTYLE) & WS_EX_APPWINDOW) != 0;
        HWND rectangle = GetDlgItem(selector, 1005);
        HWND circle = GetDlgItem(selector, 1006);
        shapeControlsVisible = rectangle && circle && IsWindowVisible(rectangle) &&
                               IsWindowVisible(circle) &&
                               SendMessageW(rectangle, BM_GETCHECK, 0, 0) == BST_CHECKED &&
                               SendMessageW(circle, BM_GETCHECK, 0, 0) == BST_UNCHECKED;
        PostMessageW(selector, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(20, 300));
        PostMessageW(selector, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(100, 400));
        PostMessageW(selector, WM_KEYDOWN, VK_RETURN, 0);
    });
    const auto selected =
        aa::selectRegion(owner, target, image, origin, aa::SelectionKind::Highlight, nullptr);
    confirmer.join();
    require(shapeControlsVisible,
            "seletor não mostra Retângulo/Círculo com a forma atual marcada");
    require(topLevelAppWindow, "seletor não é uma janela top level WS_EX_APPWINDOW");
    require(selected && selected->x == 20 && selected->y == 300,
            "origem da seleção confirmada incorreta");
    require(selected->width == 80 && selected->height == 100,
            "dimensões da seleção confirmada incorretas");
    require(IsWindowVisible(owner) != FALSE, "owner não reapareceu após confirmação");

    for (const auto kind : {aa::SelectionKind::Buffs, aa::SelectionKind::Icon,
                            aa::SelectionKind::Highlight}) {
        bool initialCircleChecked = false;
        ShowWindow(owner, SW_HIDE);
        std::jthread circleConfirmer([&] {
            HWND selector = waitForSelector();
            initialCircleChecked =
                SendMessageW(GetDlgItem(selector, aa::CircleButton), BM_GETCHECK, 0, 0) ==
                BST_CHECKED;
            PostMessageW(selector, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(400, 400));
            PostMessageW(selector, WM_COMMAND, MAKEWPARAM(aa::RectangleButton, BN_CLICKED), 0);
            PostMessageW(selector, WM_COMMAND, MAKEWPARAM(aa::CircleButton, BN_CLICKED), 0);
            PostMessageW(selector, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(300, 300));
            PostMessageW(selector, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(320, 315));
            PostMessageW(selector, WM_KEYDOWN, VK_RETURN, 0);
        });
        const auto selectedCircle = aa::selectRegion(owner, target, image, origin, kind, nullptr,
                                                     aa::RegionShape::Circle);
        circleConfirmer.join();
        require(initialCircleChecked, "forma circular inicial não aparece marcada");
        require(selectedCircle && selectedCircle->x == 275 && selectedCircle->y == 275 &&
                    selectedCircle->width == 50 && selectedCircle->height == 50 &&
                    selectedCircle->shape == aa::RegionShape::Circle,
                "interface não confirmou círculo após alternar a forma e clicar centro/raio");
    }

    if (argc > 1) {
        const std::filesystem::path output = argv[1];
        std::filesystem::create_directories(output);
        require(SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)),
                "WIC não pôde inicializar para carregar a amostra do harness");
        const auto asset = std::filesystem::path(__FILE__).parent_path().parent_path() /
                           "assets" / "assassin-3.png";
        const auto sample = aa::loadImage(asset);
        CoUninitialize();
        for (int y = 0; y < 50; ++y)
            for (int x = 0; x < 50; ++x) {
                const auto from = static_cast<std::size_t>(
                    (y * sample.height / 50) * sample.width + x * sample.width / 50) * 4;
                const auto to = static_cast<std::size_t>((y + 275) * width + x + 275) * 4;
                std::copy_n(sample.bgra.data() + from, 4, image.bgra.data() + to);
            }
        for (const auto shape : {aa::RegionShape::Rectangle, aa::RegionShape::Circle}) {
            std::jthread previewCapture([&] {
                HWND selector = waitForSelector();
                const bool circle = shape == aa::RegionShape::Circle;
                SendMessageW(selector, WM_LBUTTONDOWN, MK_LBUTTON,
                              circle ? MAKELPARAM(300, 300) : MAKELPARAM(275, 275));
                SendMessageW(selector, WM_LBUTTONDOWN, MK_LBUTTON,
                              circle ? MAKELPARAM(320, 315) : MAKELPARAM(325, 325));
                saveSelectorSnapshot(selector, output /
                    (circle ? "selector-circle.png" : "selector-rectangle.png"));
                PostMessageW(selector, WM_KEYDOWN, VK_RETURN, 0);
            });
            const auto previewSelected = aa::selectRegion(owner, target, image, origin,
                aa::SelectionKind::Icon, nullptr, shape);
            previewCapture.join();
            require(previewSelected && previewSelected->shape == shape,
                    "captura estática não preservou a forma selecionada");
        }
    }

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
    std::jthread escCanceller([] {
        postSelector(WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(300, 300));
        postSelector(WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(340, 300));
        postSelector(WM_KEYDOWN, VK_ESCAPE);
    });
    require(!aa::selectRegion(owner, target, image, origin, aa::SelectionKind::Buffs, nullptr,
                              aa::RegionShape::Circle),
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
