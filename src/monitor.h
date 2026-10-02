#pragma once
#include "workspace.h"
namespace aa {
enum class MonitorReaderKind { Status, Health };
struct MonitorReader { StatusDefinition status; HudArea area; bool needsStacks=false; bool needsClock=false; MonitorReaderKind kind=MonitorReaderKind::Status; };
struct MonitorAction { StatusRule rule; Region target; std::vector<std::size_t> readers; std::size_t reader = 0; mutable std::vector<bool> healthLatches; };
struct MonitorPlan {
    Region captureArea;
    std::vector<MonitorReader> readers;
    std::vector<MonitorAction> actions;
};
MonitorPlan makeMonitorPlan(const Workspace& workspace);
// Comparacoes de habilidade pronta iguais usam o resultado da primeira acao no mesmo frame.
std::vector<std::size_t> readyComparisonSources(const MonitorPlan& plan);
std::vector<bool> evaluateMonitor(const MonitorPlan& plan, const std::vector<Observation>& observations,
                                  std::int64_t nowMs, int validityMs, std::uint64_t source,
                                  std::vector<std::optional<float>>* remainingFractions=nullptr,
                                  const std::vector<bool>* ready=nullptr);
}
