#pragma once
#include "overlay_effect.h"
#include <windows.h>

// Construção, uso e destruição exclusivamente na thread da UI.
class Overlay {
public:
    Overlay() = default;
    ~Overlay();
    Overlay(const Overlay&) = delete;
    Overlay& operator=(const Overlay&) = delete;
    void initialize(HINSTANCE instance);
    void setCaptureVisible(bool enabled);
    void update(HWND target, RECT icon, bool highlight);
    void setColor(COLORREF color) { if (color_ != color) { color_ = color; drawn_ = false; } }
    void setEffect(aa::OverlayEffect effect) { if (effect_ != effect) { effect_ = effect; drawn_ = false; } }
    void setShape(aa::RegionShape shape) { if (shape_ != shape) { shape_ = shape; drawn_ = false; } }
    void setRemaining(std::optional<float> remaining);
private:
    HWND window_ = nullptr;
    HDC memoryDC_ = nullptr;
    HBITMAP bitmap_ = nullptr;
    void* pixels_ = nullptr;
    int width_ = 0, height_ = 0;
    bool drawn_ = false;
    bool captureVisible_ = false;
    aa::OverlayEffect effect_ = aa::OverlayEffect::Border;
    aa::RegionShape shape_ = aa::RegionShape::Rectangle;
    std::optional<float> remaining_;
    COLORREF color_ = RGB(255, 191, 0);
    POINT source_{};
    SIZE size_{};
    BYTE opacity_ = 255;
    bool draw(int iconWidth, int iconHeight);
    void hide();
};
