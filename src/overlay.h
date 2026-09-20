#pragma once
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
private:
    HWND window_ = nullptr;
    HDC memoryDC_ = nullptr;
    HBITMAP bitmap_ = nullptr;
    void* pixels_ = nullptr;
    int width_ = 0, height_ = 0;
    bool drawn_ = false;
    bool captureVisible_ = false;
    COLORREF color_ = RGB(255, 191, 0);
    bool draw(int width, int height);
    void hide();
};
