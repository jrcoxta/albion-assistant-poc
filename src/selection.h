#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "image.h"
#include "model.h"

#include <optional>

namespace aa {

class Recognizer;

enum class SelectionKind {
    Buffs,
    Icon,
    Highlight,
};

// Exibe uma captura congelada em pixels do cliente. Retorna coordenadas
// relativas ao snapshot somente após confirmação explícita; qualquer forma de
// fechamento/cancelamento retorna std::nullopt.
std::optional<Region> selectRegion(HWND owner,
                                   HWND target,
                                   const Image& snapshot,
                                   POINT origin,
                                   SelectionKind kind,
                                   const Recognizer* recognizer);

} // namespace aa
