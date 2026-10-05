#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "workspace.h"
#include "diagnostic_log.h"
#include "profiles.h"
#include <algorithm>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>

namespace aa {
namespace {
[[noreturn]] void storageFailure(const char* stage, const char* message) {
    const auto reason=GetLastError();
    diagnostic_log::win32(stage,reason);
    throw std::runtime_error(std::string(message)+" (Win32 "+std::to_string(reason)+").");
}
constexpr std::size_t entityLimit = 64, childLimit = 32, nameLimit = 251, textLimit = 32000;
constexpr std::uintmax_t fileLimit = 32 * 1024 * 1024;
struct CaseLess {
    bool operator()(const std::wstring& a, const std::wstring& b) const {
        if (a.empty() || b.empty()) return a.empty() && !b.empty();
        return CompareStringOrdinal(a.c_str(), static_cast<int>(a.size()), b.c_str(),
                                    static_cast<int>(b.size()), TRUE) == CSTR_LESS_THAN;
    }
};
using Names = std::set<std::wstring, CaseLess>;
void require(bool valid, const char* message) { if (!valid) throw std::invalid_argument(message); }
bool unicode(const std::wstring& value) {
    for (std::size_t i = 0; i < value.size(); ++i) {
        const auto c = static_cast<unsigned>(value[i]);
        if (c >= 0xD800 && c <= 0xDBFF) {
            if (++i == value.size() || value[i] < 0xDC00 || value[i] > 0xDFFF) return false;
        } else if (c >= 0xDC00 && c <= 0xDFFF) return false;
    }
    return true;
}
bool space(wchar_t c) {
    WORD kind = 0;
    return GetStringTypeW(CT_CTYPE1, &c, 1, &kind) != FALSE && (kind & C1_SPACE) != 0;
}
void textValid(const std::wstring& value, std::size_t maximum = textLimit) {
    require(value.size() <= maximum && unicode(value), "Texto Unicode invalido ou longo demais.");
    require(std::none_of(value.begin(), value.end(), [](wchar_t c) { return c < 32 || c == 127; }),
            "Texto contem caractere de controle.");
}
void nameValid(const std::wstring& value, bool emptyAllowed = false) {
    textValid(value, nameLimit);
    require((emptyAllowed && value.empty()) || (!value.empty() && !space(value.front()) && !space(value.back())),
            "Nome vazio ou com espacos nas extremidades.");
}
void idValid(const std::wstring& value, bool emptyAllowed = false) {
    require((emptyAllowed || !value.empty()) && value.size() <= 64 &&
        std::all_of(value.begin(), value.end(), [](wchar_t c) {
            return (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') ||
                   (c >= L'0' && c <= L'9') || c == L'-' || c == L'_';
        }), "ID invalido.");
}
template<class T> const T* byId(const std::vector<T>& values, const std::wstring& id) {
    const auto it = std::find_if(values.begin(), values.end(), [&](const auto& item) { return item.id == id; });
    return it == values.end() ? nullptr : &*it;
}
bool inBounds(const HudLayout& hud, const Region& r) {
    return r.valid() && static_cast<std::int64_t>(r.x) + r.width <= hud.clientWidth &&
           static_cast<std::int64_t>(r.y) + r.height <= hud.clientHeight;
}
bool emptyRegion(const Region& r) { return r.x == 0 && r.y == 0 && r.width == 0 && r.height == 0; }
std::string utf8Description(const std::wstring& value) {
    const auto length=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),nullptr,0,nullptr,nullptr);
    if(length<=0)return "[texto indisponivel]";
    std::string result(static_cast<std::size_t>(length),'\0');
    if(WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),result.data(),length,nullptr,nullptr)!=length)
        return "[texto indisponivel]";
    return result;
}
void validate(const Workspace& w) {
    require(w.nextId > 0 && w.validityMs >= 1 && w.validityMs <= 60000, "Parametros do workspace invalidos.");
    require(w.huds.size() <= entityLimit && w.statuses.size() <= entityLimit && w.sets.size() <= entityLimit &&
            w.rules.size() <= entityLimit * childLimit,
             "Limite de 64 HUDs, status ou sets excedido.");
    Names ids, hudNames, statusNames, setNames, ruleNames;
    const auto entity = [&](const auto& value, Names& names) {
        idValid(value.id); nameValid(value.name);
        require(ids.insert(value.id).second, "ID duplicado.");
        require(names.insert(value.name).second, "Nome duplicado na categoria.");
    };
    for (const auto& hud : w.huds) {
        entity(hud, hudNames); textValid(hud.monitorDevice, 1024);
        require(hud.clientWidth >= 0 && hud.clientWidth <= 32768 && hud.clientHeight >= 0 && hud.clientHeight <= 32768 &&
                (hud.clientWidth == 0) == (hud.clientHeight == 0) && hud.monitorDpi <= 10000,
                "Dimensoes ou DPI da HUD invalidos.");
        require(hud.areas.size() <= childLimit, "Limite de 32 regioes por HUD excedido.");
        Names areas;
        for (const auto& area : hud.areas) {
            nameValid(area.name); require(areas.insert(area.name).second, "Nome de regiao duplicado na HUD.");
            textValid(area.readyReferencePath);
            require(!area.readyConfirmed || !area.readyReferencePath.empty(), "Confirmacao de habilidade sem imagem.");
            require(area.region.shape == RegionShape::Rectangle || area.region.shape == RegionShape::Circle, "Formato de regiao invalido.");
            require(emptyRegion(area.region) || inBounds(hud, area.region), "Regiao fora das dimensoes da HUD.");
            require(area.iconSize >= 24 && area.iconSize <= 256, "Escala do icone invalida.");
            if (area.healthCalibration.valid()) require(area.healthCalibration.x >= 0 && area.healthCalibration.y >= 0 &&
                area.healthCalibration.x + area.healthCalibration.width <= area.region.width &&
                area.healthCalibration.y + area.healthCalibration.height <= area.region.height, "Calibracao de vida fora da area.");
        }
    }
    for (const auto& status : w.statuses) {
        entity(status, statusNames); textValid(status.referencePath); textValid(status.clockReferencePath);
        require(status.stacks.size() <= 99, "Limite de 99 amostras de stacks excedido.");
        std::set<unsigned> labels;
        for (const auto& sample : status.stacks) {
            require(sample.value >= 1 && sample.value <= 99 && labels.insert(sample.value).second,
                    "Rotulo de stacks invalido ou duplicado.");
            textValid(sample.path);
        }
    }
    for (const auto& rule : w.rules) {
            idValid(rule.id); require(ids.insert(rule.id).second, "ID de regra duplicado.");
            idValid(rule.statusId, true);
            require(rule.statusId.empty() || byId(w.statuses, rule.statusId), "Regra referencia status inexistente.");
            nameValid(rule.sourceArea, true); nameValid(rule.targetArea, true);
            nameValid(rule.condition.name); textValid(rule.condition.profile, nameLimit);
            require(ruleNames.insert(rule.condition.name).second, "Nome de regra duplicado na biblioteca.");
            require(rule.condition.condition == Condition::StacksEqual || rule.condition.condition == Condition::Present ||
                    rule.condition.condition == Condition::Absent, "Condicao de regra invalida.");
            require(rule.condition.stacks >= 1 && rule.condition.stacks <= 99 && rule.condition.color <= 0xFFFFFF,
                    "Stacks ou cor da regra invalidos.");
            require(rule.effect >= OverlayEffect::Border && rule.effect <= OverlayEffect::Flames, "Efeito de regra invalido.");
            textValid(rule.clockReferencePath);
            require(rule.stackSamples.size() <= 99, "Limite de 99 amostras de stacks excedido.");
            std::set<unsigned> samples;
            for(const auto& sample:rule.stackSamples) {
                require(sample.value>=1&&sample.value<=99&&samples.insert(sample.value).second,"Rotulo de stacks invalido ou duplicado.");
                textValid(sample.path);
            }
            for (const auto& trigger : rule.triggers) {
                require(trigger.kind == TriggerKind::Status || trigger.kind == TriggerKind::Health, "Tipo de gatilho invalido.");
                if (trigger.kind == TriggerKind::Health) {
                    nameValid(trigger.healthArea, true);
                    require(trigger.healthComparison == HealthComparison::AtMost || trigger.healthComparison == HealthComparison::AtLeast,
                            "Comparacao de vida invalida.");
                    require(trigger.healthPercent >= 1 && trigger.healthPercent <= 100, "Percentual de vida invalido.");
                }
            }
    }
    for (const auto& set : w.sets) {
        entity(set, setNames);
        require(set.rules.size() <= childLimit, "Limite de 32 regras por perfil excedido.");
        Names links;
        for (const auto& link : set.rules) {
            idValid(link.ruleId);
            if(!byId(w.rules,link.ruleId))throw std::invalid_argument("Perfil \""+utf8Description(set.name)+"\" referencia regra inexistente: "+utf8Description(link.ruleId));
            if(!links.insert(link.ruleId).second)throw std::invalid_argument("Perfil \""+utf8Description(set.name)+"\" repete a regra: "+utf8Description(link.ruleId));
        }
    }
    idValid(w.activeHudId, true); idValid(w.activeSetId, true);
    require(w.activeHudId.empty() || byId(w.huds, w.activeHudId), "HUD ativa inexistente.");
    require(w.activeSetId.empty() || byId(w.sets, w.activeSetId), "Set ativo inexistente.");
}

// As APIs nativas aceitam chaves/secoes repetidas e ignoram texto malformado.
// Conferir a estrutura antes da leitura impede que isso esconda dados corrompidos.
struct IniReader {
    std::filesystem::path path;
    std::map<std::wstring, Names, CaseLess> remaining;
    Names visited;
    explicit IniReader(const std::filesystem::path& file) : path(std::filesystem::absolute(file)) {
        const auto size = std::filesystem::file_size(path);
        require(size >= 2 && size <= fileLimit && size % 2 == 0, "Tamanho de arquivo de workspace invalido.");
        std::ifstream input(path, std::ios::binary);
        unsigned char bom[2]{};
        input.read(reinterpret_cast<char*>(bom), 2);
        require(bom[0] == 0xFF && bom[1] == 0xFE, "Workspace precisa estar em UTF-16 LE.");
        std::wstring content(static_cast<std::size_t>(size / 2 - 1), L'\0');
        input.read(reinterpret_cast<char*>(content.data()), static_cast<std::streamsize>(size - 2));
        require(input.good() && unicode(content) && content.find(L'\0') == std::wstring::npos,
                "Workspace incompleto ou com Unicode invalido.");
        std::wstring section;
        for (std::size_t begin = 0; begin < content.size();) {
            const auto end = content.find(L'\n', begin);
            auto line = content.substr(begin, end == std::wstring::npos ? std::wstring::npos : end - begin);
            begin = end == std::wstring::npos ? content.size() : end + 1;
            if (!line.empty() && line.back() == L'\r') line.pop_back();
            const auto first = line.find_first_not_of(L" \t");
            if (first == std::wstring::npos || line[first] == L';') continue;
            line = line.substr(first, line.find_last_not_of(L" \t") - first + 1);
            if (line.front() == L'[') {
                require(line.size() >= 3 && line.back() == L']', "Secao INI malformada.");
                section = line.substr(1, line.size() - 2);
                require(remaining.emplace(section, Names{}).second, "Secao INI duplicada.");
            } else {
                const auto equal = line.find(L'=');
                require(!section.empty() && equal != std::wstring::npos && equal > 0, "Chave INI malformada.");
                const auto key = line.substr(0, equal);
                require(key.find_first_of(L" \t\r\n") == std::wstring::npos &&
                        remaining.at(section).insert(key).second, "Chave INI invalida ou duplicada.");
                require(line.size() < 32767, "Linha INI longa demais.");
            }
        }
    }
    std::wstring text(const std::wstring& section, const wchar_t* key) {
        const auto found = remaining.find(section);
        require(found != remaining.end() && found->second.erase(key) == 1, "Campo obrigatorio ausente no workspace.");
        visited.insert(section);
        std::wstring value(32768, L'\0');
        const auto n = GetPrivateProfileStringW(section.c_str(), key, L"\x1", value.data(),
                                               static_cast<DWORD>(value.size()), path.c_str());
        require(n < value.size() - 1, "Campo INI longo demais, leitura seria truncada.");
        value.resize(n); require(value != L"\x1", "Campo INI nao pode ser lido.");
        return value;
    }
    bool contains(const std::wstring& section, const wchar_t* key) const {
        const auto found = remaining.find(section);
        return found != remaining.end() && found->second.contains(key);
    }
    unsigned number(const std::wstring& section, const wchar_t* key, unsigned maximum) {
        const auto value = text(section, key);
        require(!value.empty(), "Numero INI vazio.");
        unsigned result = 0;
        for (const auto c : value) {
            require(c >= L'0' && c <= L'9', "Numero INI invalido.");
            const auto digit = static_cast<unsigned>(c - L'0');
            require(result <= maximum / 10 && (result < maximum / 10 || digit <= maximum % 10), "Numero INI fora do limite.");
            result = result * 10 + digit;
        }
        return result;
    }
    void finish() const {
        for (const auto& [section, keys] : remaining) {
            if(!visited.contains(section)||!keys.empty()){
                const auto& key=keys.empty()?section:*keys.begin();
                const auto narrow=[](const std::wstring& value){std::string result;for(const auto c:value)result.push_back(c<128?static_cast<char>(c):'?');return result;};
                throw std::invalid_argument("Secao/campo inesperado no workspace: "+narrow(section)+"/"+narrow(key));
            }
        }
    }
};
std::wstring indexed(const std::wstring& prefix, std::size_t index) { return prefix + L"." + std::to_wstring(index); }
std::wstring boundedName(const std::wstring& value, std::size_t limit);

Workspace readWorkspace(const std::filesystem::path& file,unsigned* storedSchema=nullptr) {
    IniReader in(file);
    const auto schema=in.number(L"workspace", L"schema", 6);
    require(schema>=1&&schema<=6, "Schema de workspace nao suportado.");
    Workspace w;
    w.nextId = in.number(L"workspace", L"nextId", std::numeric_limits<unsigned>::max());
    w.validityMs = static_cast<int>(in.number(L"workspace", L"validityMs", 60000));
    if (in.contains(L"workspace", L"shareOverlayInCapture")) w.shareOverlayInCapture = in.number(L"workspace", L"shareOverlayInCapture", 1) != 0;
    w.activeHudId = in.text(L"workspace", L"activeHudId"); w.activeSetId = in.text(L"workspace", L"activeSetId");
    const auto hudCount = in.number(L"workspace", L"hudCount", static_cast<unsigned>(entityLimit));
    const auto statusCount = in.number(L"workspace", L"statusCount", static_cast<unsigned>(entityLimit));
    const auto setCount = in.number(L"workspace", L"setCount", static_cast<unsigned>(entityLimit));
    for (unsigned i = 0; i < hudCount; ++i) {
        const auto section = indexed(L"hud", i);
        HudLayout h;
        h.id = in.text(section, L"id"); h.name = in.text(section, L"name");
        h.clientWidth = static_cast<int>(in.number(section, L"clientWidth", 32768));
        h.clientHeight = static_cast<int>(in.number(section, L"clientHeight", 32768));
        h.monitorDpi = in.number(section, L"monitorDpi", 10000); h.monitorDevice = in.text(section, L"monitorDevice");
        const auto count = in.number(section, L"areaCount", static_cast<unsigned>(childLimit));
        for (unsigned j = 0; j < count; ++j) {
            const auto child = indexed(section + L".area", j);
            HudArea a;
            a.name = in.text(child, L"name");
            a.region.x = static_cast<int>(in.number(child, L"x", 32768)); a.region.y = static_cast<int>(in.number(child, L"y", 32768));
            a.region.width = static_cast<int>(in.number(child, L"width", 32768)); a.region.height = static_cast<int>(in.number(child, L"height", 32768));
            if (in.contains(child, L"shape")) a.region.shape = static_cast<RegionShape>(in.number(child, L"shape", 1));
            a.iconSize = static_cast<int>(in.number(child, L"iconSize", 256)); a.iconCalibrated = in.number(child, L"iconCalibrated", 1) != 0;
            if (schema >= 4 && in.number(child, L"healthCalibrated", 1) != 0) {
                a.healthCalibration.x = static_cast<int>(in.number(child, L"healthX", 32768));
                a.healthCalibration.y = static_cast<int>(in.number(child, L"healthY", 32768));
                a.healthCalibration.width = static_cast<int>(in.number(child, L"healthWidth", 32768));
                a.healthCalibration.height = static_cast<int>(in.number(child, L"healthHeight", 32768));
                a.healthCalibration.red = static_cast<std::uint8_t>(in.number(child, L"healthRed", 255));
                a.healthCalibration.green = static_cast<std::uint8_t>(in.number(child, L"healthGreen", 255));
                a.healthCalibration.blue = static_cast<std::uint8_t>(in.number(child, L"healthBlue", 255));
            }
            if (schema >= 5) a.readyReferencePath = in.text(child, L"readyReferencePath");
            if (schema >= 5 && in.contains(child, L"readyConfirmed")) a.readyConfirmed = in.number(child, L"readyConfirmed", 1) != 0;
            h.areas.push_back(std::move(a));
        }
        w.huds.push_back(std::move(h));
    }
    for (unsigned i = 0; i < statusCount; ++i) {
        const auto section = indexed(L"status", i);
        StatusDefinition s;
        s.id = in.text(section, L"id"); s.name = in.text(section, L"name");
        if(schema==1||schema>=6)s.debuff = in.number(section, L"debuff", 1) != 0;
        s.builtinAssassin = in.number(section, L"builtinAssassin", 1) != 0;
        s.referencePath = in.text(section, L"referencePath");
        if(schema==1||schema>=6) {
            if (schema>=6 || in.contains(section, L"clockReferencePath")) s.clockReferencePath = in.text(section, L"clockReferencePath");
            const auto count = in.number(section, L"stackCount", 99);
            for (unsigned j = 0; j < count; ++j) {
                const auto child = indexed(section + L".stack", j);
                s.stacks.push_back({in.number(child, L"value", 99), in.text(child, L"path")});
            }
        }
        w.statuses.push_back(std::move(s));
    }
    const auto readRule = [&](const std::wstring& child) {
            StatusRule r;
            r.id = in.text(child, L"id"); r.statusId = in.text(child, L"statusId");
            r.sourceArea = in.text(child, L"sourceArea"); r.targetArea = in.text(child, L"targetArea");
            r.condition.name = in.text(child, L"name"); r.condition.profile = in.text(child, L"profile");
            r.condition.enabled = in.number(child, L"enabled", 1) != 0;
            r.condition.condition = static_cast<Condition>(in.number(child, L"condition", 2));
            r.condition.stacks = in.number(child, L"stacks", 99); r.condition.color = in.number(child, L"color", 0xFFFFFF);
            const bool legacyGlow = in.number(child, L"glow", 1) != 0;
            r.effect = in.contains(child, L"effect") ? static_cast<OverlayEffect>(in.number(child, L"effect", 4)) :
                (legacyGlow ? OverlayEffect::Glow : OverlayEffect::Border);
            if (in.contains(child, L"followClock")) r.followClock = in.number(child, L"followClock", 1) != 0;
            if (schema >= 5) r.onlyWhenReady = in.number(child, L"onlyWhenReady", 1) != 0;
            if(schema>=2) {
                r.clockReferencePath=in.text(child,L"clockReferencePath");
                const auto samples=in.number(child,L"stackCount",99);
                for(unsigned k=0;k<samples;++k) {
                    const auto sample=indexed(child+L".stack",k);
                    r.stackSamples.push_back({in.number(sample,L"value",99),in.text(sample,L"path")});
                }
            }
            if(schema>=3) {
                r.action=r.condition; const auto triggerCount=in.number(child,L"triggerCount",static_cast<unsigned>(childLimit));
                for(unsigned k=0;k<triggerCount;++k){const auto trigger=indexed(child+L".trigger",k);RuleTrigger t;
                    t.id=in.text(trigger,L"id");t.statusId=in.text(trigger,L"statusId");t.sourceArea=in.text(trigger,L"sourceArea");
                    t.condition.condition=static_cast<Condition>(in.number(trigger,L"condition",2));t.condition.stacks=in.number(trigger,L"stacks",99);
                    t.clockReferencePath=in.text(trigger,L"clockReferencePath");const auto sampleCount=in.number(trigger,L"stackCount",99);
                    for(unsigned n=0;n<sampleCount;++n){const auto sample=indexed(trigger+L".stack",n);t.stackSamples.push_back({in.number(sample,L"value",99),in.text(sample,L"path")});}
                    if (schema >= 4) { t.kind=static_cast<TriggerKind>(in.number(trigger,L"kind",1)); t.healthArea=in.text(trigger,L"healthArea");
                        t.healthComparison=static_cast<HealthComparison>(in.number(trigger,L"healthComparison",1));t.healthPercent=in.number(trigger,L"healthPercent",100); }
                    r.triggers.push_back(std::move(t));}
            }
            if(schema==1) {
                const auto status=std::find_if(w.statuses.begin(),w.statuses.end(),[&](const auto& value){return value.id==r.statusId;});
                if(status!=w.statuses.end()) {
                    if(r.condition.condition==Condition::StacksEqual)
                        for(const auto& sample:status->stacks)if(sample.value==r.condition.stacks)r.stackSamples.push_back(sample);
                    if(r.followClock)r.clockReferencePath=status->clockReferencePath;
                }
            }
            if(schema<3) {
                r.action=r.condition;
                r.triggers={{r.id+L"-trigger",r.statusId,r.sourceArea,r.condition,r.stackSamples,r.clockReferencePath}};
            }
            return r;
    };
    const auto ruleCount = schema >= 6 ? in.number(L"workspace",L"ruleCount",static_cast<unsigned>(entityLimit*childLimit)) : 0;
    if (schema >= 6) for (unsigned i=0;i<ruleCount;++i) w.rules.push_back(readRule(indexed(L"rule",i)));
    for (unsigned i = 0; i < setCount; ++i) {
        const auto section = indexed(L"set", i);
        SetProfile s;
        s.id = in.text(section, L"id"); s.name = in.text(section, L"name");
        const auto count = in.number(section, L"ruleCount", static_cast<unsigned>(childLimit));
        for (unsigned j = 0; j < count; ++j) {
            const auto child = indexed(section + L".rule", j);
            if (schema >= 6) s.rules.push_back({in.text(child,L"ruleId"),in.number(child,L"enabled",1)!=0});
            else {
                auto r=readRule(child);
                const bool enabled=r.action.enabled;
                auto name=r.condition.name;
                for(unsigned suffix=2;std::any_of(w.rules.begin(),w.rules.end(),[&](const auto& other){return sameName(other.condition.name,r.condition.name);});++suffix) {
                    const auto tail=L" ("+std::to_wstring(suffix)+L")";
                    r.condition.name=boundedName(name,nameLimit-tail.size())+tail;
                    r.action.name=r.condition.name;
                }
                s.rules.push_back({r.id,enabled}); w.rules.push_back(std::move(r));
            }
        }
        w.sets.push_back(std::move(s));
    }
    in.finish(); validate(w);
    if(storedSchema)*storedSchema=schema;
    return w;
}
std::wstring boundedName(const std::wstring& value, std::size_t limit) {
    auto result = value.substr(0, limit);
    if (!result.empty() && result.back() >= 0xD800 && result.back() <= 0xDBFF) result.pop_back();
    while (!result.empty() && space(result.back())) result.pop_back();
    return result;
}
std::wstring legacyName(const std::wstring& value, const std::wstring& fallback) {
    auto result = value;
    for (auto& c : result) if (c < 32 || c == 127) c = L' ';
    const auto first = std::find_if_not(result.begin(), result.end(), space);
    result.erase(result.begin(), first);
    result = boundedName(result, nameLimit);
    return result.empty() ? fallback : result;
}
template<class T> std::wstring uniqueName(const std::vector<T>& values, const std::wstring& name) {
    auto candidate = name;
    for (unsigned suffix = 2; std::any_of(values.begin(), values.end(), [&](const auto& value) { return sameName(value.name, candidate); }); ++suffix)
    {
        const auto tail = L" (" + std::to_wstring(suffix) + L")";
        candidate = boundedName(name, nameLimit - tail.size()) + tail;
    }
    nameValid(candidate); return candidate;
}
bool sameRegion(const Region& a, const Region& b) { return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height && a.shape == b.shape; }
bool sameHud(const HudLayout& a, const HudLayout& b) {
    if (!sameName(a.name, b.name) || a.clientWidth != b.clientWidth || a.clientHeight != b.clientHeight ||
        a.monitorDpi != b.monitorDpi || a.monitorDevice != b.monitorDevice || a.areas.size() != b.areas.size()) return false;
    for (std::size_t i = 0; i < a.areas.size(); ++i) {
        const auto& x = a.areas[i]; const auto& y = b.areas[i];
        if (!sameName(x.name, y.name) || !sameRegion(x.region, y.region) || x.iconSize != y.iconSize || x.iconCalibrated != y.iconCalibrated) return false;
    }
    return true;
}
bool sameRule(const Rule& a, const Rule& b) {
    return a.name == b.name && a.profile == b.profile && a.enabled == b.enabled && a.condition == b.condition && a.stacks == b.stacks && a.color == b.color;
}
void importLegacy(Workspace& w, const Settings& old, const std::filesystem::path& origin, bool active,
                  const std::vector<StackSample>& legacyStacks) {
    HudLayout hud;
    hud.name = legacyName(old.hudName, L"HUD importada");
    hud.clientWidth = old.clientWidth; hud.clientHeight = old.clientHeight;
    hud.monitorDpi = old.monitorDpi; hud.monitorDevice = old.monitorDevice;
    hud.areas = {{L"Buffs", old.buffs, old.iconSize, old.iconCalibrated}, {L"Destaque", old.highlight, old.iconSize, false}};
    const auto foundHud = std::find_if(w.huds.begin(), w.huds.end(), [&](const auto& h) { return sameHud(h, hud); });
    std::wstring hudId;
    if (foundHud != w.huds.end()) hudId = foundHud->id;
    else {
        hud.id = newId(w); hud.name = uniqueName(w.huds, hud.name); hudId = hud.id;
        w.huds.push_back(std::move(hud));
    }
    auto reference = old.referencePath;
    if (!reference.empty() && std::filesystem::path(reference).is_relative())
        reference = std::filesystem::absolute(origin.parent_path() / reference).wstring();
    const auto foundStatus = std::find_if(w.statuses.begin(), w.statuses.end(), [&](const auto& s) {
        return s.builtinAssassin == reference.empty() && sameName(s.referencePath, reference);
    });
    std::wstring statusId;
    if (foundStatus != w.statuses.end()) statusId = foundStatus->id;
    else {
        StatusDefinition status;
        status.id = newId(w); status.name = uniqueName(w.statuses, L"Espírito Assassino");
        status.builtinAssassin = reference.empty(); status.referencePath = reference; statusId = status.id;
        w.statuses.push_back(std::move(status));
    }
    auto importedRule = old.rule;
    importedRule.name = legacyName(old.rule.name, L"Regra importada");
    importedRule.profile = legacyName(old.rule.profile, L"Set importado");
    // Zero invalida contagem; presença/ausência não usam esse campo legado.
    if (importedRule.stacks == 0) {
        importedRule.stacks = 1;
        if (importedRule.condition == Condition::StacksEqual) importedRule.enabled = false;
    }
    const auto foundSet = std::find_if(w.sets.begin(), w.sets.end(), [&](const auto& s) {
        if (s.rules.size() != 1) return false;
        const auto* rule=byId(w.rules,s.rules[0].ruleId);
        return rule && rule->statusId == statusId && sameRule(rule->condition, importedRule);
    });
    std::wstring setId;
    if (foundSet != w.sets.end()) setId = foundSet->id;
    else {
        SetProfile set;
        set.id = newId(w); set.name = uniqueName(w.sets, importedRule.profile);
        StatusRule rule;
        rule.id = newId(w); rule.statusId = statusId; rule.sourceArea = L"Buffs"; rule.targetArea = L"Destaque"; rule.condition = importedRule;
        rule.stackSamples=legacyStacks;
        for (unsigned suffix=2;std::any_of(w.rules.begin(),w.rules.end(),[&](const auto& other){return sameName(other.condition.name,rule.condition.name);});++suffix) {
            const auto tail=L" ("+std::to_wstring(suffix)+L")";
            rule.condition.name=boundedName(importedRule.name,nameLimit-tail.size())+tail;
        }
        rule.action=rule.condition;
        rule.triggers={{rule.id+L"-trigger",rule.statusId,rule.sourceArea,rule.condition,rule.stackSamples,rule.clockReferencePath}};
        set.rules.push_back({rule.id,rule.action.enabled}); w.rules.push_back(std::move(rule));
        setId = set.id; w.sets.push_back(std::move(set));
    }
    if (active) { w.activeHudId = hudId; w.activeSetId = setId; w.validityMs = old.validityMs; }
}
}

bool sameName(const std::wstring& a, const std::wstring& b) {
    if (a.empty() || b.empty()) return a.empty() && b.empty();
    return a.size() <= static_cast<std::size_t>(std::numeric_limits<int>::max()) &&
        b.size() <= static_cast<std::size_t>(std::numeric_limits<int>::max()) &&
        CompareStringOrdinal(a.c_str(), static_cast<int>(a.size()), b.c_str(), static_cast<int>(b.size()), TRUE) == CSTR_EQUAL;
}
std::wstring newId(Workspace& w) {
    Names ids;
    for (const auto& h : w.huds) ids.insert(h.id);
    for (const auto& s : w.statuses) ids.insert(s.id);
    for (const auto& s : w.sets) ids.insert(s.id);
    for (const auto& r : w.rules) ids.insert(r.id);
    while (w.nextId > 0 && w.nextId < std::numeric_limits<unsigned>::max()) {
        const auto id = L"id-" + std::to_wstring(w.nextId++);
        if (!ids.contains(id)) return id;
    }
    throw std::runtime_error("Nao ha IDs disponiveis no workspace.");
}
Workspace loadWorkspace(const std::filesystem::path& file, const std::filesystem::path& legacySettings,
                        const std::vector<StackSample>& legacyStacks) {
    require(!file.empty(), "Caminho de workspace vazio.");
    if (std::filesystem::exists(file)) return readWorkspace(file);
    Workspace w;
    if (!legacySettings.empty()) {
        if (std::filesystem::exists(legacySettings)) importLegacy(w, loadSettings(legacySettings.wstring()), legacySettings, true, legacyStacks);
        const auto folder = std::filesystem::absolute(legacySettings).parent_path() / L"hud-profiles";
        std::vector<std::filesystem::path> profiles;
        if (std::filesystem::is_directory(folder)) {
            for (const auto& entry : std::filesystem::directory_iterator(folder))
                if (entry.is_regular_file() && sameName(entry.path().extension().wstring(), L".ini") && hasExplicitHudName(entry.path()))
                    profiles.push_back(entry.path());
        }
        std::sort(profiles.begin(), profiles.end());
        for (const auto& profile : profiles)
            importLegacy(w, loadSettings(profile.wstring()), profile, w.activeHudId.empty(), legacyStacks);
    }
    saveWorkspace(file, w); return w;
}

void saveWorkspace(const std::filesystem::path& file, const Workspace& w) {
    require(!file.empty(), "Caminho de workspace vazio."); validate(w);
    const auto full = std::filesystem::absolute(file);
    try{std::filesystem::create_directories(full.parent_path());}
    catch(const std::filesystem::filesystem_error& error){
        diagnostic_log::filesystem("workspace.create_directories",error.code().value());throw;
    }
    const auto temporary = full.wstring() + L".tmp-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64());
    const auto handle = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) storageFailure("workspace.CreateFileW", "Nao foi possivel criar o workspace temporario");
    const unsigned char bom[] = {0xFF, 0xFE}; DWORD written = 0;
    const bool wrote = WriteFile(handle, bom, sizeof(bom), &written, nullptr) != FALSE;
    const auto writeError=wrote?ERROR_WRITE_FAULT:GetLastError();
    const bool created=wrote&&written==sizeof(bom);
    CloseHandle(handle);
    try {
        if (!created){SetLastError(writeError);storageFailure("workspace.WriteFile", "Nao foi possivel iniciar o workspace Unicode");}
        const auto write = [&](const std::wstring& section, const wchar_t* key, const std::wstring& value) {
            if (!WritePrivateProfileStringW(section.c_str(), key, value.c_str(), temporary.c_str()))
                storageFailure("workspace.WritePrivateProfileStringW", "Nao foi possivel gravar o workspace");
        };
        const auto text = [&](const std::wstring& section, const wchar_t* key, const std::wstring& value) { write(section, key, L"\"" + value + L"\""); };
        const auto number = [&](const std::wstring& section, const wchar_t* key, auto value) { write(section, key, std::to_wstring(value)); };
        const bool health=true,ready=true,triggerSchema=true;
        number(L"workspace", L"schema", 6); number(L"workspace", L"nextId", w.nextId);
        number(L"workspace", L"validityMs", w.validityMs); number(L"workspace", L"shareOverlayInCapture", w.shareOverlayInCapture ? 1 : 0); text(L"workspace", L"activeHudId", w.activeHudId); text(L"workspace", L"activeSetId", w.activeSetId);
        number(L"workspace", L"hudCount", w.huds.size()); number(L"workspace", L"statusCount", w.statuses.size()); number(L"workspace", L"setCount", w.sets.size()); number(L"workspace", L"ruleCount", w.rules.size());
        for (std::size_t i = 0; i < w.huds.size(); ++i) {
            const auto section = indexed(L"hud", i); const auto& h = w.huds[i];
            text(section, L"id", h.id); text(section, L"name", h.name);
            number(section, L"clientWidth", h.clientWidth); number(section, L"clientHeight", h.clientHeight);
            number(section, L"monitorDpi", h.monitorDpi); text(section, L"monitorDevice", h.monitorDevice); number(section, L"areaCount", h.areas.size());
            for (std::size_t j = 0; j < h.areas.size(); ++j) {
                const auto child = indexed(section + L".area", j); const auto& a = h.areas[j];
                text(child, L"name", a.name); number(child, L"x", a.region.x); number(child, L"y", a.region.y);
                number(child, L"width", a.region.width); number(child, L"height", a.region.height);
                number(child, L"shape", static_cast<int>(a.region.shape));
                number(child, L"iconSize", a.iconSize); number(child, L"iconCalibrated", a.iconCalibrated ? 1 : 0);
                if (ready) text(child, L"readyReferencePath", a.readyReferencePath);
                if (ready) number(child, L"readyConfirmed", a.readyConfirmed ? 1 : 0);
                if (health||ready) { number(child,L"healthCalibrated",a.healthCalibration.valid()?1:0); if(a.healthCalibration.valid()) {
                    number(child,L"healthX",a.healthCalibration.x);number(child,L"healthY",a.healthCalibration.y);
                    number(child,L"healthWidth",a.healthCalibration.width);number(child,L"healthHeight",a.healthCalibration.height);
                    number(child,L"healthRed",a.healthCalibration.red);number(child,L"healthGreen",a.healthCalibration.green);number(child,L"healthBlue",a.healthCalibration.blue); } }
            }
        }
        for (std::size_t i = 0; i < w.statuses.size(); ++i) {
            const auto section = indexed(L"status", i); const auto& s = w.statuses[i];
            text(section, L"id", s.id); text(section, L"name", s.name);
            number(section,L"debuff",s.debuff?1:0);
            number(section, L"builtinAssassin", s.builtinAssassin ? 1 : 0); text(section, L"referencePath", s.referencePath);
            text(section,L"clockReferencePath",s.clockReferencePath);number(section,L"stackCount",s.stacks.size());
            for(std::size_t j=0;j<s.stacks.size();++j){const auto child=indexed(section+L".stack",j);
                number(child,L"value",s.stacks[j].value);text(child,L"path",s.stacks[j].path);}
        }
        for (std::size_t j = 0; j < w.rules.size(); ++j) {
                const auto child = indexed(L"rule", j); const auto& r = w.rules[j];
                text(child, L"id", r.id); text(child, L"statusId", r.statusId); text(child, L"sourceArea", r.sourceArea); text(child, L"targetArea", r.targetArea);
                text(child, L"name", r.condition.name); text(child, L"profile", r.condition.profile);
                number(child, L"enabled", r.condition.enabled ? 1 : 0); number(child, L"condition", static_cast<int>(r.condition.condition));
                number(child, L"stacks", r.condition.stacks); number(child, L"color", r.condition.color);
                number(child, L"glow", r.effect == OverlayEffect::Border ? 0 : 1);
                number(child, L"effect", static_cast<int>(r.effect));
                number(child, L"followClock", r.followClock ? 1 : 0);
                if (ready) number(child, L"onlyWhenReady", r.onlyWhenReady ? 1 : 0);
                text(child,L"clockReferencePath",r.clockReferencePath);number(child,L"stackCount",r.stackSamples.size());
                for(std::size_t k=0;k<r.stackSamples.size();++k){
                    const auto sample=indexed(child+L".stack",k);
                    number(sample,L"value",r.stackSamples[k].value);text(sample,L"path",r.stackSamples[k].path);
                }
                if(triggerSchema) {
                    const std::vector<RuleTrigger> fallback={{r.id+L"-trigger",r.statusId,r.sourceArea,r.condition,r.stackSamples,r.clockReferencePath}};
                    const auto& triggers=r.triggers.empty()?fallback:r.triggers; number(child,L"triggerCount",triggers.size());
                    for(std::size_t k=0;k<triggers.size();++k){const auto triggerSection=indexed(child+L".trigger",k);const auto& t=triggers[k];
                        text(triggerSection,L"id",t.id);text(triggerSection,L"statusId",t.statusId);text(triggerSection,L"sourceArea",t.sourceArea);number(triggerSection,L"condition",static_cast<int>(t.condition.condition));number(triggerSection,L"stacks",t.condition.stacks);text(triggerSection,L"clockReferencePath",t.clockReferencePath);number(triggerSection,L"stackCount",t.stackSamples.size());
                        for(std::size_t n=0;n<t.stackSamples.size();++n){const auto sample=indexed(triggerSection+L".stack",n);number(sample,L"value",t.stackSamples[n].value);text(sample,L"path",t.stackSamples[n].path);}
                         if(health||ready){number(triggerSection,L"kind",static_cast<int>(t.kind));text(triggerSection,L"healthArea",t.healthArea);number(triggerSection,L"healthComparison",static_cast<int>(t.healthComparison));number(triggerSection,L"healthPercent",t.healthPercent);}}
                }
        }
        for (std::size_t i = 0; i < w.sets.size(); ++i) {
            const auto section = indexed(L"set", i); const auto& s = w.sets[i];
            text(section,L"id",s.id);text(section,L"name",s.name);number(section,L"ruleCount",s.rules.size());
            for (std::size_t j=0;j<s.rules.size();++j) {
                const auto child=indexed(section+L".rule",j);
                text(child,L"ruleId",s.rules[j].ruleId);number(child,L"enabled",s.rules[j].enabled?1:0);
            }
        }
        // A chamada de flush retorna zero tambem quando tem sucesso; verificar disco abaixo.
        WritePrivateProfileStringW(nullptr, nullptr, nullptr, temporary.c_str());
        (void)readWorkspace(temporary);
        const auto flush = CreateFileW(temporary.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (flush == INVALID_HANDLE_VALUE) storageFailure("workspace.CreateFileW", "Nao foi possivel verificar a gravacao do workspace");
        const bool flushed = FlushFileBuffers(flush) != FALSE;
        const auto flushError=flushed?ERROR_SUCCESS:GetLastError();CloseHandle(flush);
        if (!flushed){SetLastError(flushError);storageFailure("workspace.FlushFileBuffers", "Nao foi possivel concluir a gravacao do workspace");}
        // O EXE anterior nao abre o schema 6. Antes da primeira conversao, guardar
        // os bytes originais para permitir voltar ao EXE anterior sem refazer HUDs.
        if (std::filesystem::exists(full)) {
            unsigned previousSchema=0;
            // Se o arquivo foi alterado/corrompido desde a abertura, nao o
            // substituir. O mesmo parser decide a versao e valida todo o arquivo,
            // inclusive valores numericos com zeros a esquerda.
            (void)readWorkspace(full,&previousSchema);
            if(previousSchema<=5) {
                const auto prefix=full.wstring()+L".before-schema6";
                for(unsigned suffix=0;;++suffix) {
                    const auto backup=prefix+(suffix?L"-"+std::to_wstring(suffix):L"")+L".ini";
                    if(!CopyFileW(full.c_str(),backup.c_str(),TRUE)) {
                        const auto reason=GetLastError();
                        if((reason==ERROR_FILE_EXISTS||reason==ERROR_ALREADY_EXISTS)&&suffix<9999)continue;
                        SetLastError(reason);storageFailure("workspace.CopyFileW", "Nao foi possivel guardar uma copia do workspace antigo; o original foi mantido");
                    }
                    // A copia so habilita a substituicao depois de ser legivel e duravel.
                    (void)readWorkspace(backup);
                    const auto stored=CreateFileW(backup.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
                    if(stored==INVALID_HANDLE_VALUE)storageFailure("workspace.CreateFileW", "Nao foi possivel verificar a copia do workspace antigo");
                    const bool safe=FlushFileBuffers(stored)!=FALSE;const auto safeError=safe?ERROR_SUCCESS:GetLastError();CloseHandle(stored);
                    if(!safe){SetLastError(safeError);storageFailure("workspace.FlushFileBuffers", "Nao foi possivel concluir a copia do workspace antigo");}
                    break;
                }
            }
        }
        if (!MoveFileExW(temporary.c_str(), full.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            storageFailure("workspace.MoveFileExW", "Nao foi possivel substituir o workspace");
        WritePrivateProfileStringW(nullptr, nullptr, nullptr, full.c_str());
    } catch (...) { DeleteFileW(temporary.c_str()); throw; }
}
void eraseHud(Workspace& w, const std::wstring& id) {
    const auto copy = id;
    std::erase_if(w.huds, [&](const auto& h) { return h.id == copy; });
    if (w.activeHudId == copy) w.activeHudId.clear();
}
void eraseSet(Workspace& w, const std::wstring& id) {
    const auto copy = id;
    std::erase_if(w.sets, [&](const auto& s) { return s.id == copy; });
    if (w.activeSetId == copy) w.activeSetId.clear();
}
void eraseStatus(Workspace& w, const std::wstring& id) {
    const auto copy = id;
    for (const auto& rule : w.rules) {
            require(rule.statusId != copy, "Status usado por uma regra; remova a dependencia antes de excluir.");
            for (const auto& trigger : rule.triggers) require(trigger.statusId != copy, "Status usado por uma regra; remova a dependencia antes de excluir.");
    }
    std::erase_if(w.statuses, [&](const auto& s) { return s.id == copy; });
}
const StatusRule* findRule(const Workspace& w,const std::wstring& id){return byId(w.rules,id);}
void eraseRule(Workspace& w,const std::wstring& id) {
    std::wstring owners;
    for(const auto& set:w.sets)for(const auto& link:set.rules)if(link.ruleId==id){
        if(!owners.empty())owners+=L", ";owners+=set.name;break;
    }
    if(!owners.empty()){
        throw std::invalid_argument("Remova a regra dos perfis antes de exclui-la: "+utf8Description(owners));
    }
    std::erase_if(w.rules,[&](const auto& rule){return rule.id==id;});
}
std::vector<unsigned> stackValues(const StatusDefinition& status) {
    std::set<unsigned> values;
    if (status.builtinAssassin) { values.insert(2); values.insert(3); }
    for (const auto& sample : status.stacks) if (sample.value >= 1 && sample.value <= 99) values.insert(sample.value);
    return {values.begin(), values.end()};
}
std::vector<std::wstring> readinessIssues(const Workspace& w) {
    std::vector<std::wstring> issues;
    const auto* hud=byId(w.huds,w.activeHudId);const auto* set=byId(w.sets,w.activeSetId);
    if(!hud)issues.push_back(L"Selecione uma HUD.");if(!set)issues.push_back(L"Selecione um perfil.");if(!set)return issues;
    if(hud&&(hud->clientWidth<=0||hud->clientHeight<=0))issues.push_back(L"A HUD precisa de calibracao para o tamanho da janela.");
    if(w.validityMs<1||w.validityMs>60000)issues.push_back(L"Validade das observacoes invalida.");
    const auto exists=[](const std::wstring& path){std::error_code error;return !path.empty()&&std::filesystem::is_regular_file(path,error);};
    bool enabled=false;
    for(std::size_t i=0;i<set->rules.size();++i){
        const auto& link=set->rules[i];if(!link.enabled)continue;
        const auto* found=byId(w.rules,link.ruleId);
        if(!found){issues.push_back(L"Perfil contem regra inexistente: "+link.ruleId);continue;}
        const auto& rule=*found;const auto& action=rule.triggers.empty()?rule.condition:rule.action;enabled=true;
        const auto prefix=L"Regra "+std::to_wstring(i+1)+L" ("+action.name+L"): ";const auto issue=[&](const std::wstring& value){issues.push_back(prefix+value);};
        const auto area=[&](const std::wstring& name)->const HudArea*{if(!hud)return nullptr;const auto found=std::find_if(hud->areas.begin(),hud->areas.end(),[&](const auto& a){return sameName(a.name,name);});return found==hud->areas.end()?nullptr:&*found;};
        const auto* target=area(rule.targetArea);if(!target)issue(L"regiao de destino inexistente: "+rule.targetArea);else if(!inBounds(*hud,target->region))issue(L"regiao de destino nao esta calibrada dentro da HUD.");
        if(rule.onlyWhenReady&&target&&(!target->readyConfirmed||!exists(target->readyReferencePath)||target->region.width<24||target->region.height<24||
            target->region.width>256||target->region.height>256))
            issue(L"capture a habilidade pronta na area de destino \""+target->name+L"\" desta HUD.");
        std::vector<RuleTrigger> fallback;if(rule.triggers.empty())fallback={{L"legacy-"+rule.id,rule.statusId,rule.sourceArea,rule.condition,rule.stackSamples,rule.clockReferencePath}};
        const auto& triggers=rule.triggers.empty()?fallback:rule.triggers;
        for(std::size_t k=0;k<triggers.size();++k){
            const auto& trigger=triggers[k];const auto conditionPrefix=triggers.size()>1?L"Condicao "+std::to_wstring(k+1)+L": ":L"";
            if(trigger.kind==TriggerKind::Health) {
                const auto* source=area(trigger.healthArea);
                if(!source)issue(conditionPrefix+L"selecione a area da barra de vida.");
                else if(!inBounds(*hud,source->region))issue(conditionPrefix+L"a area de vida nao esta calibrada dentro da HUD.");
                else if(!source->healthCalibration.valid())issue(conditionPrefix+L"na HUD \""+hud->name+L"\", selecione a barra cheia em \""+source->name+L"\" e clique em \"Calibrar vida cheia\".");
                if(trigger.healthPercent<1||trigger.healthPercent>100)issue(conditionPrefix+L"percentual de vida invalido.");
                continue;
            }
            const auto* status=byId(w.statuses,trigger.statusId);if(!status)issue(conditionPrefix+L"selecione um status existente.");
            else {
                if(status->referencePath.empty()){if(!status->builtinAssassin)issue(conditionPrefix+L"o status nao possui referencia visual.");}
                else if(!exists(status->referencePath))issue(conditionPrefix+L"referencia visual ausente: "+status->referencePath);
                if(trigger.condition.condition==Condition::StacksEqual){
                    const auto& samples=trigger.stackSamples.empty()?status->stacks:trigger.stackSamples;std::set<unsigned> values;if(status->builtinAssassin){values.insert(2);values.insert(3);}for(const auto& sample:samples){if(!exists(sample.path))issue(conditionPrefix+L"amostra de stacks "+std::to_wstring(sample.value)+L" ausente: "+sample.path);values.insert(sample.value);}if(!values.contains(trigger.condition.stacks))issue(conditionPrefix+L"stacks "+std::to_wstring(trigger.condition.stacks)+L" nao cadastrados para este status.");
                }
            }
            if(trigger.condition.condition!=Condition::StacksEqual&&trigger.condition.condition!=Condition::Present&&trigger.condition.condition!=Condition::Absent)issue(conditionPrefix+L"condicao invalida.");
            const auto* source=area(trigger.sourceArea);if(!source)issue(conditionPrefix+L"regiao de origem inexistente: "+trigger.sourceArea);else {if(!inBounds(*hud,source->region))issue(conditionPrefix+L"regiao de origem nao esta calibrada dentro da HUD.");if(!source->iconCalibrated||source->iconSize<24||source->iconSize>256||source->iconSize>source->region.width||source->iconSize>source->region.height)issue(conditionPrefix+L"na HUD \""+hud->name+L"\", selecione a area \""+source->name+L"\" e clique em \"Medir \u00edcone de status\".");}
        }
    }
    if(!enabled)issues.push_back(L"O perfil precisa de pelo menos uma regra ativada.");return issues;
}
}
