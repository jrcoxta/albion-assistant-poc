#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "image.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace aa {

struct CaptureFrame {
    Image image;
    std::int64_t capturedMs = 0;
    bool available = false;
    std::string error;
};

class DesktopCapture {
public:
    DesktopCapture();
    ~DesktopCapture();
    DesktopCapture(const DesktopCapture&) = delete;
    DesktopCapture& operator=(const DesktopCapture&) = delete;

    // start/stop são serializados pela UI. start encerra a sessão anterior.
    // ROI relativa ao cliente; timestamp no domínio de GetTickCount64.
    // Callback síncrono no worker, sem fila; deve retornar prontamente.
    // NÃO chamar start/stop nem destruir DesktopCapture dentro do callback:
    // stop notifica o worker e aguarda seu término (join).
    void start(HWND target, RECT clientRoi, std::function<void(CaptureFrame)> callback);
    void stop();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace aa
