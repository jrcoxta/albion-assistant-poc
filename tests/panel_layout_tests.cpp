#include "panel_layout.h"
#include <cmath>
#include <stdexcept>

int main() {
    struct Screen { int width, height; double dpi; };
    for (const auto screen : {Screen{1348, 680, 1.0}, Screen{1348, 676, 1.25},
                             Screen{1900, 980, 1.5}, Screen{2850, 1680, 2.0},
                             Screen{3400, 1340, 1.25}}) {
        const double scale = aa::panelScale(screen.width, screen.height, screen.dpi);
        if (scale <= 0 || scale > screen.dpi ||
            std::lround(860 * scale) > screen.width ||
            std::lround(700 * scale) > screen.height)
            throw std::runtime_error("painel não cabe na área útil");
    }
    if (aa::panelScale(1900, 1000, 1.25) != 1.25)
        throw std::runtime_error("DPI foi reduzido sem necessidade");
}
