#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
namespace aa {
enum class RegionShape { Rectangle = 0, Circle = 1 };
struct Region {
    int x=0,y=0,width=0,height=0;
    RegionShape shape=RegionShape::Rectangle;
    bool valid() const {
        return x>=0 && y>=0 && width>0 && height>0 &&
            (shape==RegionShape::Rectangle || (shape==RegionShape::Circle && width==height));
    }
    bool contains(double pointX,double pointY) const {
        if(!valid() || pointX<x || pointY<y || pointX>=static_cast<double>(x)+width || pointY>=static_cast<double>(y)+height)return false;
        if(shape==RegionShape::Rectangle)return true;
        const double dx=(pointX-x-width/2.0)/(width/2.0),dy=(pointY-y-height/2.0)/(height/2.0);
        return dx*dx+dy*dy<=1.0;
    }
};
enum class Presence { Unknown, Absent, Present };
enum class Condition { StacksEqual, Present, Absent };
struct Detection { Presence presence=Presence::Unknown; std::optional<unsigned> stacks; float confidence=0; Region icon; std::string detail; };
struct Rule { std::wstring name=L"Espírito Assassino: 3 stacks"; std::wstring profile=L"Mortíficos"; bool enabled=true; Condition condition=Condition::StacksEqual; unsigned stacks=3; std::uint32_t color=0x00BFFF; };
struct Settings {
    Region buffs, highlight;
    int clientWidth=0,clientHeight=0;
    int iconSize=48;
    int validityMs=750;
    Rule rule;
    std::wstring referencePath;
    std::wstring hudName=L"Minha HUD";
    std::wstring monitorDevice;
    unsigned monitorDpi=0;
    bool iconCalibrated=false;
};
struct Observation { Detection detection; std::int64_t capturedMs=0; std::uint64_t source=0; };
bool evaluate(const Rule& rule, const Observation& observation, std::int64_t nowMs, int validityMs, std::uint64_t source);
Settings loadSettings(const std::wstring& path);
void saveSettings(const std::wstring& path, const Settings& settings);
}
