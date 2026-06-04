#include "ordered_objectives.h"

#include <queue>
#include <stack>

namespace cgss2 {

const std::vector<int>& OrderedImpGraph::empty_vec() {
  static const std::vector<int> v;
  return v;
}

void OrderedImpGraph::clear() {
  out_.clear();
  in_.clear();
  dirty_ = false;
  seen_.clear();
  stamp_ = 1;
}

void OrderedImpGraph::reserve_vars(int nof_vars) {
  if (nof_vars < 0) return;

  std::size_t n = (std::size_t)2 * ((std::size_t)nof_vars + 1);
  if (out_.size() >= n) return;

  out_.resize(n);
  in_.resize(n);
}

void OrderedImpGraph::reserve_lit(int lit) {
  if (lit < 0) return;

  std::size_t n = (std::size_t)lit + 1;
  if (out_.size() >= n) return;

  out_.resize(n);
  in_.resize(n);
}

void OrderedImpGraph::add_edge(int from, int to) {
  if (from < 0 || to < 0) return;
  if (from == to) return;

  reserve_lit(from);
  reserve_lit(to);

  out_[from].push_back(to);
  in_[to].push_back(from);
  dirty_ = true;
}

void OrderedImpGraph::finalize() {
  if (!dirty_) return;

  for (std::vector<int>& v : out_) {
    std::sort(v.begin(), v.end());
    v.erase(std::unique(v.begin(), v.end()), v.end());
  }

  for (std::vector<int>& v : in_) {
    std::sort(v.begin(), v.end());
    v.erase(std::unique(v.begin(), v.end()), v.end());
  }

  dirty_ = false;
}

const std::vector<int>& OrderedImpGraph::successors(int lit) const {
  if (lit < 0) return empty_vec();
  if ((std::size_t)lit >= out_.size()) return empty_vec();
  return out_[lit];
}

const std::vector<int>& OrderedImpGraph::predecessors(int lit) const {
  if (lit < 0) return empty_vec();
  if ((std::size_t)lit >= in_.size()) return empty_vec();
  return in_[lit];
}

bool OrderedImpGraph::implies(int from, int to) const {
  if (from == to) return true;
  if (from < 0 || to < 0) return false;
  if ((std::size_t)from >= out_.size()) return false;
  if ((std::size_t)to >= out_.size()) return false;

  if (seen_.size() < out_.size()) {
    seen_.resize(out_.size(), 0);
  }

  ++stamp_;
  if (stamp_ == 0x3fffffff) {
    std::fill(seen_.begin(), seen_.end(), 0);
    stamp_ = 1;
  }

  std::vector<int> st;
  st.push_back(from);
  seen_[from] = stamp_;

  while (!st.empty()) {
    int v = st.back();
    st.pop_back();

    for (int u : successors(v)) {
      if (u == to) return true;
      if (u < 0) continue;
      if ((std::size_t)u >= out_.size()) continue;
      if (seen_[u] == stamp_) continue;

      seen_[u] = stamp_;
      st.push_back(u);
    }
  }

  return false;
}

void OrderedImpGraph::minimize_core(std::vector<int>& core) const {
  if (core.size() <= 1) return;

  std::vector<int> lits;
  lits.reserve(core.size());

  std::unordered_set<int> seen;
  seen.reserve(core.size() * 2 + 1);

  for (int l : core) {
    if (seen.insert(l).second) {
      lits.push_back(l);
    }
  }

  if (lits.size() <= 1) {
    core.swap(lits);
    return;
  }

  std::vector<unsigned char> drop(lits.size(), 0);

  for (std::size_t i = 0; i < lits.size(); ++i) {
    if (drop[i]) continue;

    for (std::size_t j = 0; j < lits.size(); ++j) {
      if (i == j) continue;
      if (drop[j]) continue;

      if (!implies(lits[i], lits[j])) continue;

      if (implies(lits[j], lits[i])) {
        // Equivalent under the discovered graph. Keep one representative.
        if (lits[i] <= lits[j]) {
          drop[j] = 1;
        } else {
          drop[i] = 1;
          break;
        }
      } else {
        // lits[i] is stronger. Assuming lits[i] already assumes lits[j].
        drop[j] = 1;
      }
    }
  }

  std::vector<int> reduced;
  reduced.reserve(lits.size());

  for (std::size_t i = 0; i < lits.size(); ++i) {
    if (!drop[i]) reduced.push_back(lits[i]);
  }

  if (!reduced.empty()) {
    core.swap(reduced);
  }
}


OrderedObjectiveAnalyzer::Result OrderedObjectiveAnalyzer::analyze(
    const OrderedImpGraph& graph,
    const std::vector<int>& alit_order,
    const std::vector<uint64_t>& ws,
    const std::function<bool(int)>& is_base_assumption
) const {
  Result result;

  std::vector<int> active;
  active.reserve(alit_order.size());

  std::unordered_map<int, int> active_index;
  active_index.reserve(alit_order.size() * 2 + 1);

  int max_lit = -1;

  for (int l : alit_order) {
    if (l < 0) continue;
    if ((std::size_t)l >= ws.size()) continue;
    if (!ws[l]) continue;
    if (is_base_assumption(l)) continue;

    if (active_index.find(l) != active_index.end()) continue;

    int ix = (int)active.size();
    active_index[l] = ix;
    active.push_back(l);

    if (l > max_lit) max_lit = l;
  }

  result.active_lits = active.size();

  if (active.size() < 2) {
    return result;
  }

  const int n = (int)active.size();

  std::vector<int> index(n, -1);
  std::vector<int> low(n, 0);
  std::vector<unsigned char> on_stack(n, 0);
  std::vector<int> stack;

  int next_index = 0;
  int comp_count = 0;
  std::vector<int> lit_comp_ix(n, -1);

  std::function<void(int)> dfs = [&](int v) {
    index[v] = low[v] = next_index++;
    stack.push_back(v);
    on_stack[v] = 1;

    int lit = active[v];

    for (int to_lit : graph.successors(lit)) {
      auto it = active_index.find(to_lit);
      if (it == active_index.end()) continue;

      int w = it->second;

      if (index[w] == -1) {
        dfs(w);
        low[v] = std::min(low[v], low[w]);
      } else if (on_stack[w]) {
        low[v] = std::min(low[v], index[w]);
      }
    }

    if (low[v] == index[v]) {
      while (true) {
        int w = stack.back();
        stack.pop_back();
        on_stack[w] = 0;
        lit_comp_ix[w] = comp_count;

        if (w == v) break;
      }

      ++comp_count;
    }
  };

  for (int v = 0; v < n; ++v) {
    if (index[v] == -1) dfs(v);
  }

  result.components = comp_count;
  result.component_members.resize(comp_count);

  if (max_lit >= 0) {
    result.lit_component.assign((std::size_t)max_lit + 1, -1);
  }

  for (int i = 0; i < n; ++i) {
    int c = lit_comp_ix[i];
    int lit = active[i];

    result.component_members[c].push_back(lit);

    if (lit >= 0 && (std::size_t)lit < result.lit_component.size()) {
      result.lit_component[lit] = c;
    }
  }

  if (comp_count == 1) {
    result.full_order = true;
    result.component_order.push_back(0);
    return result;
  }

  if ((std::size_t)comp_count > max_exact_components_) {
    result.too_large = true;
    return result;
  }

  std::vector<std::vector<int> > dag(comp_count);

  for (int i = 0; i < n; ++i) {
    int from_comp = lit_comp_ix[i];
    int lit = active[i];

    for (int to_lit : graph.successors(lit)) {
      auto it = active_index.find(to_lit);
      if (it == active_index.end()) continue;

      int to_comp = lit_comp_ix[it->second];
      if (from_comp == to_comp) continue;

      dag[from_comp].push_back(to_comp);
    }
  }

  for (std::vector<int>& v : dag) {
    std::sort(v.begin(), v.end());
    v.erase(std::unique(v.begin(), v.end()), v.end());
  }

  std::vector<std::vector<unsigned char> > reach(
      comp_count,
      std::vector<unsigned char>(comp_count, 0)
  );

  for (int s = 0; s < comp_count; ++s) {
    std::queue<int> q;
    reach[s][s] = 1;
    q.push(s);

    while (!q.empty()) {
      int v = q.front();
      q.pop();

      for (int u : dag[v]) {
        if (reach[s][u]) continue;
        reach[s][u] = 1;
        q.push(u);
      }
    }
  }

  for (int a = 0; a < comp_count; ++a) {
    for (int b = a + 1; b < comp_count; ++b) {
      if (!reach[a][b] && !reach[b][a]) {
        return result;
      }
    }
  }

  std::vector<int> order;
  order.reserve(comp_count);

  for (int c = 0; c < comp_count; ++c) {
    order.push_back(c);
  }

  std::sort(order.begin(), order.end(), [&](int a, int b) {
    if (reach[a][b] && !reach[b][a]) return true;
    if (reach[b][a] && !reach[a][b]) return false;
    return a < b;
  });

  result.full_order = true;
  result.component_order.swap(order);

  return result;
}


const std::vector<int>& OrderedStratification::empty_vec() {
  static const std::vector<int> v;
  return v;
}

void OrderedStratification::disable() {
  active_ = false;
  exhausted_ = false;
  level_ = -1;
  lit_unlock_level_.clear();
  level_lits_.clear();
}

void OrderedStratification::configure_full_order(
    const OrderedObjectiveAnalyzer::Result& result
) {
  disable();

  if (!result.full_order) return;
  if (result.component_order.empty()) return;

  int max_lit = -1;

  for (const std::vector<int>& members : result.component_members) {
    for (int l : members) {
      if (l > max_lit) max_lit = l;
    }
  }

  if (max_lit < 0) return;

  lit_unlock_level_.assign((std::size_t)max_lit + 1, -1);

  // component_order is source-to-sink:
  // c0 -> c1 -> ... -> ck.
  //
  // SAT-to-UNSAT unlocking wants sink first:
  // ck, then c{k-1}, ...
  for (int oi = (int)result.component_order.size() - 1; oi >= 0; --oi) {
    int comp = result.component_order[oi];

    if (comp < 0) continue;
    if ((std::size_t)comp >= result.component_members.size()) continue;

    std::vector<int> lits;

    for (int l : result.component_members[comp]) {
      if (l < 0) continue;
      if ((std::size_t)l >= lit_unlock_level_.size()) continue;

      lit_unlock_level_[l] = (int)level_lits_.size();
      lits.push_back(l);
    }

    if (!lits.empty()) {
      std::sort(lits.begin(), lits.end());
      lits.erase(std::unique(lits.begin(), lits.end()), lits.end());
      level_lits_.push_back(lits);
    }
  }

  if (level_lits_.empty()) return;

  active_ = true;
  exhausted_ = false;
  level_ = 0;
}

bool OrderedStratification::next_level() {
  if (!active_) return false;

  if (level_ + 1 < (int)level_lits_.size()) {
    ++level_;
    return true;
  }

  exhausted_ = true;
  return false;
}

bool OrderedStratification::is_unlocked(int lit) const {
  if (!active_) return false;
  if (lit < 0) return false;
  if ((std::size_t)lit >= lit_unlock_level_.size()) return false;

  int l = lit_unlock_level_[lit];
  return l >= 0 && l <= level_;
}

int OrderedStratification::unlock_level(int lit) const {
  if (lit < 0) return -1;
  if ((std::size_t)lit >= lit_unlock_level_.size()) return -1;
  return lit_unlock_level_[lit];
}

const std::vector<int>& OrderedStratification::current_level_lits() const {
  if (!active_) return empty_vec();
  if (level_ < 0) return empty_vec();
  if ((std::size_t)level_ >= level_lits_.size()) return empty_vec();
  return level_lits_[level_];
}

} // namespace cgss2