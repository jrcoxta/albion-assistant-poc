#pragma once
#include <windows.h>
#include <commctrl.h>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include "workspace.h"
#include "monitor.h"
#include "capture.h"
#include "recognition.h"
#include "overlay.h"
#include "selection.h"

namespace aaapp {
constexpr UINT ResultMessage = WM_APP + 1;
constexpr UINT ActivateMessage = WM_APP + 2;
inline constexpr wchar_t AppWindowClass[] = L"AlbionAssistant";
enum Id {
    Connect=100, Start, Stop, TestAction, Save, HudList, HudName, NewHud, DeleteHud,
    AreaList, AreaName, NewArea, DeleteArea, SelectArea, CalibrateArea,
    StatusList, StatusName, StatusKind, NewStatus, DeleteStatus, CaptureStatus, ImportStatus,
    SampleValue, CaptureStack, StackList, DeleteStack, AddPreset,
    SetList, SetName, NewSet, DeleteSet, RuleList, RuleName, NewRule, DeleteRule,
    RuleStatus, SourceArea, TargetArea, ConditionBox, Stacks, EffectBox, Color, Enabled,
    MoveRuleUp, MoveRuleDown, Validity, SampleColor, FollowClock, CaptureClock, ClockHint, Tab0=250
};
std::wstring widen(const std::string& value);
std::wstring text(HWND window);
RECT rect(aa::Region area);
bool fits(aa::Region area, int width, int height);
struct Screen { int width=0,height=0; unsigned dpi=0; std::wstring device; POINT origin{}; };
Screen screenOf(HWND target);
struct PickedImage { aa::Region area; aa::Image image; Screen screen; };
std::unique_ptr<aa::Recognizer> makeRecognizer(const aa::MonitorReader& reader);

struct App {
    HINSTANCE instance{};
    HWND window{}, target{}, statusLabel{}, badge{};
    HFONT font{}, titleFont{};
    std::filesystem::path directory, settingsPath, workspacePath;
    aa::Workspace workspace;
    int page=0, selectedArea=-1, selectedRule=-1;
    std::wstring selectedStatusId;
    double dpi=1;
    bool rebuilding=false, selecting=false, running=false;
    bool diagnostics=false, showOverlayInCapture=false;
    std::wstring error=L"Conecte ao jogo. Escolha uma HUD e um set para iniciar.", hotkeyWarning;
    std::vector<HWND> controls;
    aa::Image capturePreview, referencePreview;
    aa::DesktopCapture capture;
    aa::MonitorPlan plan;
    std::vector<std::unique_ptr<aa::Recognizer>> recognizers;
    std::vector<std::unique_ptr<Overlay>> overlays;
    std::unique_ptr<Overlay> testOverlay;
    std::vector<aa::Observation> current, latest, lastReadings;
    std::vector<bool> lit;
    std::mutex mutex;
    std::atomic<bool> pending=false;
    aa::Image latestImage;
    std::wstring latestError;
    std::uint64_t source=0, latestSource=0, previewUntil=0;
    bool previewClock=false;
    aa::Region previewTarget;
    std::ofstream trace;
    std::string lastTrace;

    ~App();
    void configureStorage(const std::filesystem::path& settingsFile={});
    void load();
    // Salvar cópia antes de substituir estado em memória. Nenhuma escrita em legados.
    void commit(aa::Workspace changed);
    aa::HudLayout* hud();
    aa::SetProfile* set();
    aa::StatusDefinition* selectedStatus();
    aa::HudArea* area();
    aa::StatusRule* rule();
    int px(int value) const;
    HWND item(int id) const;
    HWND control(const wchar_t* type,const wchar_t* label,DWORD style,int x,int y,int w,int h,int id=0);
    void label(const wchar_t* value,int x,int y,int w,int h=22);
    void button(const wchar_t* value,int id,int x,int y,int w);
    void edit(const wchar_t* value,int id,int x,int y,int w);
    int number(int id,int minimum,int maximum);
    // Implementação da interface e CRUD em app_ui.cpp.
    void makeUI();
    void rebuildUIWithDraft();
    aa::Workspace editorValues();
    void saveEditor();
    void command(int id,int notification);
    void updateRuleChoices();
    void refreshStatus();
    void paint();
    // Runtime compartilhado, implementado em app.cpp.
    bool connect();
    bool geometryMatches() const;
    void stop();
    void start();
    void consume();
    std::vector<std::optional<float>> evaluateReadings(std::int64_t now,bool targetReady);
    void updateHighlight();
    void testAction();
    void sampleActionColor(const std::function<aa::Image(HWND,RECT)>& captureFrame={});
    bool applyActionColor(const aa::Image& image);
    void applyClockReference(const aa::Image& image);
    std::optional<PickedImage> pick(aa::SelectionKind kind, const aa::Recognizer* reference=nullptr,
                                  aa::RegionShape shape=aa::RegionShape::Rectangle);
    // Grava referência nova sob nome único. Retorna caminho; chamador faz commit do status.
    std::wstring storeImage(const std::wstring& statusId,const aa::Image& image);
};
LRESULT CALLBACK appProc(HWND window,UINT message,WPARAM wParam,LPARAM lParam);
}
