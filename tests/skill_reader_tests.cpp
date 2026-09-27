#include "skill_reader.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

int main() {
    try {
        aa::Image ready{48, 48, std::vector<std::uint8_t>(48 * 48 * 4, 255)};
        for (int y = 0; y < 48; ++y) for (int x = 0; x < 48; ++x) {
            const auto i = (static_cast<std::size_t>(y) * 48 + x) * 4;
            ready.bgra[i] = static_cast<std::uint8_t>(80 + x * 2);
            ready.bgra[i+1] = static_cast<std::uint8_t>(75 + y * 2);
            ready.bgra[i+2] = 210;
        }
        if (!aa::skillReady(ready, ready)) throw std::runtime_error("referencia pronta rejeitada");
        auto movingBackground = ready;
        for (int y = 0; y < 48; ++y) for (int x = 0; x < 48; ++x) {
            const double dx = x + 0.5 - 24, dy = y + 0.5 - 24;
            if (dx * dx + dy * dy <= 24 * 24) continue;
            const auto i = (static_cast<std::size_t>(y) * 48 + x) * 4;
            movingBackground.bgra[i] = 15;
            movingBackground.bgra[i+1] = 12;
            movingBackground.bgra[i+2] = 4;
        }
        if (!aa::skillReady(movingBackground, ready, aa::RegionShape::Circle))
            throw std::runtime_error("cenario reproduzido: fundo fora do icone circular apagou habilidade pronta");
        if (aa::skillReady(movingBackground, ready, aa::RegionShape::Rectangle))
            throw std::runtime_error("retangulo ignorou cantos da habilidade");
        auto cooldown = ready;
        for (int y = 9; y < 31; ++y) for (int x = 8; x < 36; ++x) {
            const auto i = (static_cast<std::size_t>(y) * 48 + x) * 4;
            for (int c = 0; c < 3; ++c) cooldown.bgra[i+c] /= 2;
        }
        if (aa::skillReady(cooldown, ready)) throw std::runtime_error("cooldown gerou pronta");
        auto partial = ready;
        for (int y = 8; y < 31; ++y) for (int x = 8; x < 10; ++x) {
            const auto i = (static_cast<std::size_t>(y) * 48 + x) * 4;
            for (int c = 0; c < 3; ++c) partial.bgra[i+c] /= 2;
        }
        if (aa::skillReady(partial, ready)) throw std::runtime_error("setor pequeno de recarga gerou pronta");
        auto border = ready;
        for (int x = 0; x < 48; ++x) {
            const auto i = static_cast<std::size_t>(x) * 4;
            for (int c = 0; c < 3; ++c) border.bgra[i+c] /= 2;
        }
        if (aa::skillReady(border, ready)) throw std::runtime_error("borda de recarga gerou pronta");
        auto innerBorder = ready;
        for (int x = 14; x < 34; ++x) {
            const auto i = (static_cast<std::size_t>(3) * 48 + x) * 4;
            for (int c = 0; c < 3; ++c) innerBorder.bgra[i+c] /= 2;
        }
        auto darkReference = ready;
        for (int x = 0; x < 48; ++x) {
            const auto i = static_cast<std::size_t>(x) * 4;
            darkReference.bgra[i] = darkReference.bgra[i+1] = darkReference.bgra[i+2] = 8;
        }
        auto darkBorder = darkReference;
        for (int x = 0; x < 48; ++x) {
            const auto i = static_cast<std::size_t>(x) * 4;
            darkBorder.bgra[i] = 45;
        }
        if (aa::skillReady(darkBorder, darkReference)) throw std::runtime_error("borda escura de recarga passou despercebida");
        auto bottom = ready;
        for (int y = 43; y < 48; ++y) for (int x = 8; x < 36; ++x) {
            const auto i = (static_cast<std::size_t>(y) * 48 + x) * 4;
            for (int c = 0; c < 3; ++c) bottom.bgra[i+c] /= 2;
        }
        if (aa::skillReady(bottom, ready)) throw std::runtime_error("rodape de recarga gerou pronta");
        if (aa::skillReady(bottom, ready, aa::RegionShape::Circle) ||
            aa::skillReady(innerBorder, ready, aa::RegionShape::Circle) ||
            aa::skillReady(partial, ready, aa::RegionShape::Circle) ||
            aa::skillReady(cooldown, ready, aa::RegionShape::Circle))
            throw std::runtime_error("mascara circular ignorou recarga dentro do icone");
        aa::Image largerReady{62, 62, std::vector<std::uint8_t>(62 * 62 * 4, 255)};
        for (std::size_t i = 0; i < largerReady.bgra.size(); i += 4) {
            largerReady.bgra[i] = 80; largerReady.bgra[i+1] = 130; largerReady.bgra[i+2] = 200;
        }
        auto firstShadow = largerReady;
        for (int x = 25; x < 31; ++x) {
            const auto i = (static_cast<std::size_t>(3) * 62 + x) * 4;
            for (int c = 0; c < 3; ++c) firstShadow.bgra[i+c] /= 2;
        }
        if (aa::skillReady(firstShadow, largerReady, aa::RegionShape::Circle))
            throw std::runtime_error("seis pixels internos de recarga geraram pronta");
        auto diagonalShadow = largerReady;
        for (int offset = 0; offset < 6; ++offset) {
            const auto i = (static_cast<std::size_t>(offset + 4) * 62 + 34 + offset) * 4;
            for (int c = 0; c < 3; ++c) diagonalShadow.bgra[i+c] /= 2;
        }
        if (aa::skillReady(diagonalShadow, largerReady, aa::RegionShape::Circle))
            throw std::runtime_error("sombra diagonal interna gerou pronta");
        auto varyingRim = largerReady;
        for (int y = 0; y < 62; ++y) for (int x = 0; x < 62; ++x) {
            const int dx = 2*x+1-62, dy = 2*y+1-62;
            if (dx*dx+dy*dy > 62*62 || dx*dx+dy*dy <= 58*58) continue;
            const auto i = (static_cast<std::size_t>(y)*62+x)*4;
            for (int c = 0; c < 3; ++c) varyingRim.bgra[i+c] /= 2;
        }
        if (!aa::skillReady(varyingRim, largerReady, aa::RegionShape::Circle))
            throw std::runtime_error("aro externo instavel apagou habilidade pronta");
        auto isolatedNoise = largerReady;
        for (int x : {18, 43}) {
            const auto i = (static_cast<std::size_t>(30) * 62 + x) * 4;
            for (int c = 0; c < 3; ++c) isolatedNoise.bgra[i+c] /= 2;
        }
        if (!aa::skillReady(isolatedNoise, largerReady, aa::RegionShape::Circle))
            throw std::runtime_error("ruido interno isolado apagou a habilidade pronta");
        if (aa::skillReady(ready, ready, static_cast<aa::RegionShape>(8)) ||
            aa::skillReady({48, 40, std::vector<std::uint8_t>(48 * 40 * 4, 255)}, ready,
                           aa::RegionShape::Circle))
            throw std::runtime_error("formato invalido foi aceito");
        if (aa::skillReady({}, ready) || aa::skillReady(ready, {})) throw std::runtime_error("leitura invalida gerou pronta");
        std::cout << "Comparacao conservadora de icone aprovada\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
