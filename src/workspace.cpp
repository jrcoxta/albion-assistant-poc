#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "workspace.h"
#include "profiles.h"
#include <algorithm>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>

namespace aa {
namespace {
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
void validate(const Workspace& w) {
    require(w.nextId > 0 && w.validityMs >= 1 && w.validityMs <= 60000, "Parametros do workspace invalidos.");
    require(w.huds.size() <= entityLimit && w.statuses.size() <= entityLimit && w.sets.size() <= entityLimit,
            "Limite de 64 HUDs, status ou sets excedido.");
    Names ids, hudNames, statusNames, setNames;
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
            require(emptyRegion(area.region) || inBounds(hud, area.region), "Regiao fora das dimensoes da HUD.");
            require(area.iconSize >= 24 && area.iconSize <= 256, "Escala do icone invalida.");
        }
    }
    for (const auto& status : w.statuses) {
        entity(status, statusNames); textValid(status.referencePath);
        require(status.stacks.size() <= 99, "Limite de 99 amostras de stacks excedido.");
        std::set<unsigned> labels;
        for (const auto& sample : status.stacks) {
            require(sample.value >= 1 && sample.value <= 99 && labels.insert(sample.value).second,
                    "Rotulo de stacks invalido ou duplicado.");
            textValid(sample.path);
        }
    }
    for (const auto& set : w.sets) {
        entity(set, setNames);
        require(set.rules.size() <= childLimit, "Limite de 32 regras por set excedido.");
        Names ruleNames;
        for (const auto& rule : set.rules) {
            idValid(rule.id); require(ids.insert(rule.id).second, "ID de regra duplicado.");
            idValid(rule.statusId, true);
            require(rule.statusId.empty() || byId(w.statuses, rule.statusId), "Regra referencia status inexistente.");
            nameValid(rule.sourceArea, true); nameValid(rule.targetArea, true);
            nameValid(rule.condition.name); textValid(rule.condition.profile, nameLimit);
            require(ruleNames.insert(rule.condition.name).second, "Nome de regra duplicado no set.");
            require(rule.condition.condition == Condition::StacksEqual || rule.condition.condition == Condition::Present ||
                    rule.condition.condition == Condition::Absent, "Condicao de regra invalida.");
            require(rule.condition.stacks >= 1 && rule.condition.stacks <= 99 && rule.condition.color <= 0xFFFFFF,
                    "Stacks ou cor da regra invalidos.");
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
            require(visited.contains(section) && keys.empty(), "Campos ou contagens inesperados no workspace.");
        }
    }
};
std::wstring indexed(const std::wstring& prefix, std::size_t index) { return prefix + L"." + std::to_wstring(index); }

Workspace readWorkspace(const std::filesystem::path& file) {
    IniReader in(file);
    require(in.number(L"workspace", L"schema", 1) == 1, "Schema de workspace nao suportado.");
    Workspace w;
    w.nextId = in.number(L"workspace", L"nextId", std::numeric_limits<unsigned>::max());
    w.validityMs = static_cast<int>(in.number(L"workspace", L"validityMs", 60000));
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
            a.iconSize = static_cast<int>(in.number(child, L"iconSize", 256)); a.iconCalibrated = in.number(child, L"iconCalibrated", 1) != 0;
            h.areas.push_back(std::move(a));
        }
        w.huds.push_back(std::move(h));
    }
    for (unsigned i = 0; i < statusCount; ++i) {
        const auto section = indexed(L"status", i);
        StatusDefinition s;
        s.id = in.text(section, L"id"); s.name = in.text(section, L"name");
        s.debuff = in.number(section, L"debuff", 1) != 0; s.builtinAssassin = in.number(section, L"builtinAssassin", 1) != 0;
        s.referencePath = in.text(section, L"referencePath");
        const auto count = in.number(section, L"stackCount", 99);
        for (unsigned j = 0; j < count; ++j) {
            const auto child = indexed(section + L".stack", j);
            s.stacks.push_back({in.number(child, L"value", 99), in.text(child, L"path")});
        }
        w.statuses.push_back(std::move(s));
    }
    for (unsigned i = 0; i < setCount; ++i) {
        const auto section = indexed(L"set", i);
        SetProfile s;
        s.id = in.text(section, L"id"); s.name = in.text(section, L"name");
        const auto count = in.number(section, L"ruleCount", static_cast<unsigned>(childLimit));
        for (unsigned j = 0; j < count; ++j) {
            const auto child = indexed(section + L".rule", j);
            StatusRule r;
            r.id = in.text(child, L"id"); r.statusId = in.text(child, L"statusId");
            r.sourceArea = in.text(child, L"sourceArea"); r.targetArea = in.text(child, L"targetArea");
            r.condition.name = in.text(child, L"name"); r.condition.profile = in.text(child, L"profile");
            r.condition.enabled = in.number(child, L"enabled", 1) != 0;
            r.condition.condition = static_cast<Condition>(in.number(child, L"condition", 2));
            r.condition.stacks = in.number(child, L"stacks", 99); r.condition.color = in.number(child, L"color", 0xFFFFFF);
            r.glow = in.number(child, L"glow", 1) != 0; s.rules.push_back(std::move(r));
        }
        w.sets.push_back(std::move(s));
    }
    in.finish(); validate(w); return w;
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
bool sameRegion(const Region& a, const Region& b) { return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height; }
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
        if (!status.builtinAssassin) status.stacks = legacyStacks;
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
        return s.rules.size() == 1 && s.rules[0].statusId == statusId && sameRule(s.rules[0].condition, importedRule);
    });
    std::wstring setId;
    if (foundSet != w.sets.end()) setId = foundSet->id;
    else {
        SetProfile set;
        set.id = newId(w); set.name = uniqueName(w.sets, importedRule.profile);
        StatusRule rule;
        rule.id = newId(w); rule.statusId = statusId; rule.sourceArea = L"Buffs"; rule.targetArea = L"Destaque"; rule.condition = importedRule;
        set.rules.push_back(std::move(rule)); setId = set.id; w.sets.push_back(std::move(set));
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
    for (const auto& s : w.sets) { ids.insert(s.id); for (const auto& r : s.rules) ids.insert(r.id); }
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
    std::filesystem::create_directories(full.parent_path());
    const auto temporary = full.wstring() + L".tmp-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64());
    const auto handle = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) throw std::runtime_error("Nao foi possivel criar o workspace temporario.");
    const unsigned char bom[] = {0xFF, 0xFE}; DWORD written = 0;
    const bool created = WriteFile(handle, bom, sizeof(bom), &written, nullptr) && written == sizeof(bom);
    CloseHandle(handle);
    try {
        if (!created) throw std::runtime_error("Nao foi possivel iniciar o workspace Unicode.");
        const auto write = [&](const std::wstring& section, const wchar_t* key, const std::wstring& value) {
            if (!WritePrivateProfileStringW(section.c_str(), key, value.c_str(), temporary.c_str()))
                throw std::runtime_error("Nao foi possivel gravar o workspace.");
        };
        const auto text = [&](const std::wstring& section, const wchar_t* key, const std::wstring& value) { write(section, key, L"\"" + value + L"\""); };
        const auto number = [&](const std::wstring& section, const wchar_t* key, auto value) { write(section, key, std::to_wstring(value)); };
        number(L"workspace", L"schema", 1); number(L"workspace", L"nextId", w.nextId);
        number(L"workspace", L"validityMs", w.validityMs); text(L"workspace", L"activeHudId", w.activeHudId); text(L"workspace", L"activeSetId", w.activeSetId);
        number(L"workspace", L"hudCount", w.huds.size()); number(L"workspace", L"statusCount", w.statuses.size()); number(L"workspace", L"setCount", w.sets.size());
        for (std::size_t i = 0; i < w.huds.size(); ++i) {
            const auto section = indexed(L"hud", i); const auto& h = w.huds[i];
            text(section, L"id", h.id); text(section, L"name", h.name);
            number(section, L"clientWidth", h.clientWidth); number(section, L"clientHeight", h.clientHeight);
            number(section, L"monitorDpi", h.monitorDpi); text(section, L"monitorDevice", h.monitorDevice); number(section, L"areaCount", h.areas.size());
            for (std::size_t j = 0; j < h.areas.size(); ++j) {
                const auto child = indexed(section + L".area", j); const auto& a = h.areas[j];
                text(child, L"name", a.name); number(child, L"x", a.region.x); number(child, L"y", a.region.y);
                number(child, L"width", a.region.width); number(child, L"height", a.region.height);
                number(child, L"iconSize", a.iconSize); number(child, L"iconCalibrated", a.iconCalibrated ? 1 : 0);
            }
        }
        for (std::size_t i = 0; i < w.statuses.size(); ++i) {
            const auto section = indexed(L"status", i); const auto& s = w.statuses[i];
            text(section, L"id", s.id); text(section, L"name", s.name); number(section, L"debuff", s.debuff ? 1 : 0);
            number(section, L"builtinAssassin", s.builtinAssassin ? 1 : 0); text(section, L"referencePath", s.referencePath); number(section, L"stackCount", s.stacks.size());
            for (std::size_t j = 0; j < s.stacks.size(); ++j) {
                const auto child = indexed(section + L".stack", j);
                number(child, L"value", s.stacks[j].value); text(child, L"path", s.stacks[j].path);
            }
        }
        for (std::size_t i = 0; i < w.sets.size(); ++i) {
            const auto section = indexed(L"set", i); const auto& s = w.sets[i];
            text(section, L"id", s.id); text(section, L"name", s.name); number(section, L"ruleCount", s.rules.size());
            for (std::size_t j = 0; j < s.rules.size(); ++j) {
                const auto child = indexed(section + L".rule", j); const auto& r = s.rules[j];
                text(child, L"id", r.id); text(child, L"statusId", r.statusId); text(child, L"sourceArea", r.sourceArea); text(child, L"targetArea", r.targetArea);
                text(child, L"name", r.condition.name); text(child, L"profile", r.condition.profile);
                number(child, L"enabled", r.condition.enabled ? 1 : 0); number(child, L"condition", static_cast<int>(r.condition.condition));
                number(child, L"stacks", r.condition.stacks); number(child, L"color", r.condition.color); number(child, L"glow", r.glow ? 1 : 0);
            }
        }
        // A chamada de flush retorna zero tambem quando tem sucesso; verificar disco abaixo.
        WritePrivateProfileStringW(nullptr, nullptr, nullptr, temporary.c_str());
        (void)readWorkspace(temporary);
        const auto flush = CreateFileW(temporary.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (flush == INVALID_HANDLE_VALUE) throw std::runtime_error("Nao foi possivel verificar a gravacao do workspace.");
        const bool flushed = FlushFileBuffers(flush) != FALSE; CloseHandle(flush);
        if (!flushed || !MoveFileExW(temporary.c_str(), full.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Nao foi possivel substituir o workspace.");
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
    for (const auto& set : w.sets)
        for (const auto& rule : set.rules)
            require(rule.statusId != copy, "Status usado por uma regra; remova a dependencia antes de excluir.");
    std::erase_if(w.statuses, [&](const auto& s) { return s.id == copy; });
}
std::vector<unsigned> stackValues(const StatusDefinition& status) {
    std::set<unsigned> values;
    if (status.builtinAssassin) { values.insert(2); values.insert(3); }
    for (const auto& sample : status.stacks) if (sample.value >= 1 && sample.value <= 99) values.insert(sample.value);
    return {values.begin(), values.end()};
}
std::vector<std::wstring> readinessIssues(const Workspace& w) {
    std::vector<std::wstring> issues;
    const auto* hud = byId(w.huds, w.activeHudId); const auto* set = byId(w.sets, w.activeSetId);
    if (!hud) issues.push_back(L"Selecione uma HUD.");
    if (!set) issues.push_back(L"Selecione um set.");
    if (!set) return issues;
    if (hud && (hud->clientWidth <= 0 || hud->clientHeight <= 0)) issues.push_back(L"A HUD precisa de calibração para o tamanho da janela.");
    if (w.validityMs < 1 || w.validityMs > 60000) issues.push_back(L"Validade das observações inválida.");
    bool enabled = false;
    const auto fileExists = [](const std::wstring& path) { std::error_code error; return !path.empty() && std::filesystem::is_regular_file(path, error); };
    for (std::size_t i = 0; i < set->rules.size(); ++i) {
        const auto& rule = set->rules[i]; if (!rule.condition.enabled) continue;
        enabled = true;
        const auto prefix = L"Regra " + std::to_wstring(i + 1) + L" (" + rule.condition.name + L"): ";
        const auto issue = [&](const std::wstring& message) { issues.push_back(prefix + message); };
        const auto* status = byId(w.statuses, rule.statusId);
        if (!status) issue(L"selecione um status existente.");
        else {
            if (status->referencePath.empty()) { if (!status->builtinAssassin) issue(L"o status não possui referência visual."); }
            else if (!fileExists(status->referencePath)) issue(L"referência visual ausente: " + status->referencePath);
            if (rule.condition.condition == Condition::StacksEqual) {
                for (const auto& sample : status->stacks)
                    if (!fileExists(sample.path)) issue(L"amostra de stacks " + std::to_wstring(sample.value) + L" ausente: " + sample.path);
                const auto values = stackValues(*status);
                if (std::find(values.begin(), values.end(), rule.condition.stacks) == values.end())
                    issue(L"stacks " + std::to_wstring(rule.condition.stacks) + L" não cadastrados para este status.");
            }
        }
        if (rule.condition.condition != Condition::StacksEqual && rule.condition.condition != Condition::Present && rule.condition.condition != Condition::Absent)
            issue(L"condição inválida.");
        if (!hud) { issue(L"não há HUD ativa para resolver origem e destino."); continue; }
        const auto area = [&](const std::wstring& name) -> const HudArea* {
            const auto found = std::find_if(hud->areas.begin(), hud->areas.end(), [&](const auto& a) { return sameName(a.name, name); });
            return found == hud->areas.end() ? nullptr : &*found;
        };
        const auto* source = area(rule.sourceArea); const auto* target = area(rule.targetArea);
        if (!source) issue(L"região de origem inexistente: " + rule.sourceArea);
        else {
            if (!inBounds(*hud, source->region)) issue(L"região de origem não está calibrada dentro da HUD.");
            if (!source->iconCalibrated || source->iconSize < 24 || source->iconSize > 256 ||
                source->iconSize > source->region.width || source->iconSize > source->region.height)
                issue(L"escala do ícone da origem precisa de calibração.");
        }
        if (!target) issue(L"região de destino inexistente: " + rule.targetArea);
        else if (!inBounds(*hud, target->region)) issue(L"região de destino não está calibrada dentro da HUD.");
    }
    if (!enabled) issues.push_back(L"O set precisa de pelo menos uma regra habilitada.");
    return issues;
}
}
