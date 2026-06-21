// govcheck/src/graph/cycle_detector.cpp
// Tarjan's cycle detection (GC-F-001) implementation.

#include "cycle_detector.h"
#include <unordered_map>
#include <unordered_set>
#include <stack>
#include <algorithm>

namespace egbos::govcheck {

struct TarjanState {
    const ClaimGraph& graph;
    std::unordered_map<std::string, int> dfn;
    std::unordered_map<std::string, int> low;
    std::unordered_set<std::string> in_stack;
    std::stack<std::string> stack;
    int index = 0;
    std::vector<ValidationFinding> findings;

    void dfs(const std::string& u) {
        dfn[u] = low[u] = ++index;
        stack.push(u);
        in_stack.insert(u);

        auto it = graph.find(u);
        if (it != graph.end()) {
            for (const auto& v : it->second) {
                if (dfn.find(v) == dfn.end()) {
                    dfs(v);
                    low[u] = std::min(low[u], low[v]);
                } else if (in_stack.count(v)) {
                    low[u] = std::min(low[u], dfn[v]);
                }
            }
        }

        if (dfn[u] == low[u]) {
            std::vector<std::string> scc;
            while (true) {
                std::string v = stack.top();
                stack.pop();
                in_stack.erase(v);
                scc.push_back(v);
                if (v == u) break;
            }

            bool is_cycle = false;
            if (scc.size() > 1) {
                is_cycle = true;
            } else if (scc.size() == 1) {
                auto it_self = graph.find(u);
                if (it_self != graph.end()) {
                    for (const auto& v : it_self->second) {
                        if (v == u) {
                            is_cycle = true;
                            break;
                        }
                    }
                }
            }

            if (is_cycle) {
                std::string cycle_path;
                for (auto it_path = scc.rbegin(); it_path != scc.rend(); ++it_path) {
                    if (!cycle_path.empty()) cycle_path += " -> ";
                    cycle_path += *it_path;
                }
                cycle_path += " -> " + scc.back();

                findings.push_back({
                    "GC-F-001",
                    Severity::Fatal,
                    u,
                    "Circular dependency in claim graph: " + cycle_path,
                    "",
                    0
                });
            }
        }
    }
};

std::vector<ValidationFinding> detect_cycles(const ClaimGraph& graph) {
    TarjanState state{graph};
    for (const auto& pair : graph) {
        if (state.dfn.find(pair.first) == state.dfn.end()) {
            state.dfs(pair.first);
        }
    }
    return state.findings;
}

} // namespace egbos::govcheck
