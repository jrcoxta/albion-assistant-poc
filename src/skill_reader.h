#pragma once
#include "image.h"
#include "model.h"

namespace aa {
// Uma referência capturada com a habilidade pronta. Incerto e recarga nunca
// autorizam um destaque; a referência precisa ser da mesma habilidade/HUD.
bool skillReady(const Image& current, const Image& readyReference,
                RegionShape shape = RegionShape::Rectangle);
}
