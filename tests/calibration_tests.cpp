#include "calibration.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objbase.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

aa::Image resize(const aa::Image& source, int size) {
    aa::Image out{size, size, std::vector<std::uint8_t>(std::size_t(size) * size * 4)};
    for (int y = 0; y < size; ++y) for (int x = 0; x < size; ++x) {
        const auto from = (std::size_t(y * source.height / size) * source.width + x * source.width / size) * 4;
        std::copy_n(source.bgra.data() + from, 4, out.bgra.data() + (std::size_t(y) * size + x) * 4);
    }
    return out;
}

aa::Image place(const aa::Image& source, int width, int height, int left, int top) {
    aa::Image out{width, height, std::vector<std::uint8_t>(std::size_t(width) * height * 4, 24)};
    for (int y = 0; y < source.height; ++y) {
        std::copy_n(source.bgra.data() + std::size_t(y) * source.width * 4, source.width * 4,
                    out.bgra.data() + (std::size_t(top + y) * width + left) * 4);
    }
    return out;
}

bool closeTo(const aa::Region& actual, int x, int y, int size, int tolerance = -1) {
    if (tolerance < 0) tolerance = std::max(3, size / 32);
    return std::abs(actual.x - x) <= tolerance && std::abs(actual.y - y) <= tolerance &&
           std::abs(actual.width - size) <= tolerance && actual.width == actual.height;
}

void checkThrows(const aa::Image& image, aa::Region region, const char* message) {
    try {
        static_cast<void>(aa::cropImage(image, region));
    } catch (const std::invalid_argument&) {
        return;
    }
    throw std::runtime_error(message);
}
}

int main(int argc, char** argv) {
    const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    try {
        const std::filesystem::path assets = argc > 1 ? argv[1] : "assets";
        aa::Recognizer recognizer(assets);
        const auto icon = aa::loadImage(assets / "assassin-3.png");

        const auto cropped = aa::cropImage(icon, {7, 9, 13, 11});
        check(cropped.width == 13 && cropped.height == 11, "Recorte preserva dimensoes pedidas");
        for (int y = 0; y < cropped.height; ++y) for (int x = 0; x < cropped.width; ++x) {
            const auto source = (std::size_t(y + 9) * icon.width + x + 7) * 4;
            const auto target = (std::size_t(y) * cropped.width + x) * 4;
            check(std::equal(icon.bgra.data() + source, icon.bgra.data() + source + 4,
                             cropped.bgra.data() + target),
                  "Recorte copia os pixels corretos");
        }
        checkThrows(icon, {-1, 0, 1, 1}, "Recorte negativo deve falhar");
        checkThrows(icon, {0, 0, 0, 1}, "Recorte vazio deve falhar");
        checkThrows(icon, {60, 60, 8, 8}, "Recorte fora da imagem deve falhar");
        checkThrows({}, {0, 0, 1, 1}, "Imagem invalida deve falhar");

        int maximumPositionError = 0;
        int maximumSizeError = 0;
        double syntheticElapsed = 0;
        double syntheticMaximum = 0;
        int syntheticCount = 0;
        for (const int size : {24, 32, 44, 60, 64, 80, 100, 128, 192, 256}) {
            const int left = 17 + size / 3;
            const int top = 13 + size / 5;
            const auto image = place(resize(icon, size), size * 3 + 29, size * 2 + 31, left, top);
            const auto suggestionStarted = std::chrono::steady_clock::now();
            const auto found = aa::suggestIcon(image, left + size / 2, top + size / 2, recognizer);
            const auto suggestionElapsed = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - suggestionStarted).count();
            syntheticElapsed += suggestionElapsed;
            syntheticMaximum = std::max(syntheticMaximum, suggestionElapsed);
            ++syntheticCount;
            check(found.has_value(), "Clique central deve sugerir o icone em varias escalas");
            if (!closeTo(*found, left, top, size)) {
                std::cerr << "escala esperada " << size << " em " << left << ',' << top
                          << "; obtida " << found->width << " em " << found->x << ',' << found->y << "\n";
            }
            check(closeTo(*found, left, top, size), "Sugestao deve preservar posicao e escala");
            maximumPositionError = std::max({maximumPositionError, std::abs(found->x - left), std::abs(found->y - top)});
            maximumSizeError = std::max(maximumSizeError, std::abs(found->width - size));
            check(found->x <= left + size / 2 && left + size / 2 < found->x + found->width &&
                  found->y <= top + size / 2 && top + size / 2 < found->y + found->height,
                  "Sugestao deve conter o clique");
        }

        const auto topLeft = place(icon, 150, 130, 0, 0);
        const auto foundTopLeft = aa::suggestIcon(topLeft, 2, 3, recognizer);
        check(foundTopLeft && closeTo(*foundTopLeft, 0, 0, 64),
              "Icone no canto superior esquerdo deve ser encontrado");
        const auto bottomRight = place(icon, 150, 130, 86, 66);
        const auto foundBottomRight = aa::suggestIcon(bottomRight, 148, 128, recognizer);
        check(foundBottomRight && closeTo(*foundBottomRight, 86, 66, 64),
              "Icone no canto inferior direito deve ser encontrado");

        const auto fullCanvas = place(icon, 2880, 1800, 1379, 811);
        const auto fullCanvasStarted = std::chrono::steady_clock::now();
        const auto foundOnFullCanvas = aa::suggestIcon(fullCanvas, 1411, 843, recognizer);
        const auto fullCanvasElapsed = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - fullCanvasStarted).count();
        check(foundOnFullCanvas && closeTo(*foundOnFullCanvas, 1379, 811, 64, 3),
              "Clique no screenshot 2880x1800 deve localizar o icone 64px localmente");

        check(!aa::suggestIcon(topLeft, 100, 90, recognizer), "Clique fora do icone nao deve aceitar candidato proximo");
        check(!aa::suggestIcon(topLeft, -1, 10, recognizer), "Clique negativo deve falhar conservadoramente");
        check(!aa::suggestIcon(topLeft, 150, 10, recognizer), "Clique fora da imagem deve falhar conservadoramente");
        check(!aa::suggestIcon({}, 0, 0, recognizer), "Imagem invalida nao deve produzir sugestao");
        const aa::Image blank{180, 120, std::vector<std::uint8_t>(180 * 120 * 4, 30)};
        check(!aa::suggestIcon(blank, 90, 60, recognizer), "Imagem uniforme nao deve produzir sugestao");
        check(!aa::suggestIcon(aa::loadImage(assets / "other-buff.png"), 32, 32, recognizer),
              "Outro buff nao deve ser sugerido");
        check(!aa::suggestIcon(aa::loadImage(assets / "other-food.png"), 32, 32, recognizer),
              "Comida nao deve ser sugerida");

        auto ambiguous = place(icon, 128, 64, 0, 0);
        for (int y = 0; y < 64; ++y) {
            std::copy_n(icon.bgra.data() + std::size_t(y) * 64 * 4, 64 * 4,
                        ambiguous.bgra.data() + (std::size_t(y) * 128 + 64) * 4);
        }
        check(!aa::suggestIcon(ambiguous, 64, 32, recognizer),
              "Clique na divisao de candidatos identicos deve permanecer ambiguo");

        const auto live = assets.parent_path() / "tests" / "fixtures" / "recognition-live";
        const char* presentCases[] = {
            "12036046-stacks-unknown.png", "12038093-stacks-2.png", "12038609-stacks-unknown.png",
            "12038671-stacks-2.png", "12038734-stacks-unknown.png", "12039187-stacks-2.png",
            "12039500-stacks-unknown.png", "12040093-stacks-2.png", "12040156-stacks-unknown.png",
            "12040312-stacks-3.png", "12042765-stacks-unknown.png", "12042796-stacks-3.png",
            "12044562-stacks-unknown.png", "12044640-stacks-3.png", "12044656-stacks-unknown.png",
            "12044812-stacks-3.png", "12046375-stacks-unknown.png", "12046484-stacks-3.png",
            "12046531-stacks-unknown.png", "12046609-stacks-3.png"
        };
        int liveMatches = 0;
        const auto started = std::chrono::steady_clock::now();
        for (const auto* file : presentCases) {
            const auto found = aa::suggestIcon(aa::loadImage(live / file), 65, 44, recognizer);
            if (found && closeTo(*found, 34, 12, 64, 3)) ++liveMatches;
        }
        const auto elapsed = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - started).count();
        check(liveMatches == std::size(presentCases), "Todas as ROIs reais presentes devem sugerir o icone");
        check(!aa::suggestIcon(aa::loadImage(live / "12025625-stacks-unknown.png"), 65, 44, recognizer),
              "ROI real sem o buff nao deve sugerir icone");
        check(!aa::suggestIcon(aa::loadImage(live / "12047375-stacks-unknown.png"), 65, 44, recognizer),
              "Segunda ROI real sem o buff nao deve sugerir icone");

        std::cout << "ROIs reais: " << liveMatches << "/" << std::size(presentCases)
                  << " presentes localizadas; 2/2 ausentes rejeitadas\n";
        std::cout << "Sinteticos 24..256: erro maximo de posicao " << maximumPositionError
                  << " px; tamanho " << maximumSizeError << " px; media "
                  << syntheticElapsed / syntheticCount << " ms; maximo " << syntheticMaximum << " ms\n";
        std::cout << "Canvas 2880x1800 com icone 64px: " << fullCanvasElapsed << " ms\n";
        std::cout << "Sugestao local media ms: " << elapsed / std::size(presentCases) << "\n";
        std::cout << "calibracao: verificacoes passaram\n";
        if (SUCCEEDED(com)) CoUninitialize();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FALHOU: " << e.what() << "\n";
        if (SUCCEEDED(com)) CoUninitialize();
        return 1;
    }
}
