#ifndef CGSS2_ORDERED_OBJECTIVES_H
#define CGSS2_ORDERED_OBJECTIVES_H

#include <algorithm>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace cgss2 {

class OrderedImpGraph {
public:
  void clear();
  void reserve_vars(int nof_vars);
  void reserve_lit(int lit);

  void add_edge(int from, int to);
  void finalize();

  bool implies(int from, int to) const;

  void minimize_core(std::vector<int>& core) const;

  const std::vector<int>& successors(int lit) const;
  const std::vector<int>& predecessors(int lit) const;

  std::size_t size() const { return out_.size(); }

private:
  static const std::vector<int>& empty_vec();

  std::vector<std::vector<int> > out_;
  std::vector<std::vector<int> > in_;

  bool dirty_ = false;

  mutable std::vector<int> seen_;
  mutable int stamp_ = 1;
};


class OrderedObjectiveAnalyzer {
public:
  struct Result {
    bool full_order = false;
    bool too_large = false;

    std::vector<int> component_order;

    std::vector<std::vector<int> > component_members;

    std::vector<int> lit_component;

    std::size_t active_lits = 0;
    std::size_t components = 0;
  };

  explicit OrderedObjectiveAnalyzer(std::size_t max_exact_components = 4096)
      : max_exact_components_(max_exact_components) {}

  Result analyze(
      const OrderedImpGraph& graph,
      const std::vector<int>& alit_order,
      const std::vector<uint64_t>& ws,
      const std::function<bool(int)>& is_base_assumption
  ) const;

private:
  std::size_t max_exact_components_;
};


class OrderedStratification {
public:
  void disable();

  void configure_full_order(const OrderedObjectiveAnalyzer::Result& result);

  bool active() const { return active_; }
  bool exhausted() const { return exhausted_; }

  bool next_level();

  bool is_unlocked(int lit) const;
  int unlock_level(int lit) const;

  int level() const { return level_; }
  int levels() const { return (int)level_lits_.size(); }

  const std::vector<int>& current_level_lits() const;

private:
  static const std::vector<int>& empty_vec();

  bool active_ = false;
  bool exhausted_ = false;

  int level_ = -1;

  std::vector<int> lit_unlock_level_;

  std::vector<std::vector<int> > level_lits_;
};

} // namespace cgss2

#endif