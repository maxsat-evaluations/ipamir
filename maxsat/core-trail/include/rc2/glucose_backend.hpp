#pragma once

#include <cstdint>
#include <utility>
#include <vector>

namespace rc2 {

enum class SatResult : int {
    SAT = 1,
    UNSAT = 0,
    UNKNOWN = -1,
};

class GlucoseBackend {
public:
    GlucoseBackend();
    ~GlucoseBackend();

    void ensure_vars(int max_var);
    bool add_clause(const std::vector<int>& clause);
    bool solve(const std::vector<int>& assumptions);
    std::pair<bool, std::vector<int>> prop_check(const std::vector<int>& assumptions, int phase_saving = 0);
    SatResult solve_limited(const std::vector<int>& assumptions);
    void set_conf_budget(std::int64_t conflicts);
    void budget_off();
    std::vector<int> get_model() const;
    std::vector<int> get_core() const;
    SatResult get_last_status() const;
    void interrupt();
    void clear_interrupt();
    void reset();

private:
    struct Impl;
    Impl* p_;
};

}  // namespace rc2
