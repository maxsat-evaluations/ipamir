#include <cstdint>
#include <exception>
#include <string>
#include <vector>
#include <jemalloc/jemalloc.h>

#include "../../ipamir.h"
#include "rc2/rc2_solver.hpp"

namespace {

struct SolverHandle {
    rc2::RC2StratifiedSolver solver;
    std::vector<int> hard_clause;
    std::vector<int> assumptions;

    void* term_state{nullptr};
    int (*term_cb)(void*){nullptr};

    explicit SolverHandle()
        : solver(rc2::SolverOptions{}) {
    }
};

static inline SolverHandle* as_handle(void* p) {
    return reinterpret_cast<SolverHandle*>(p);
}

}  // namespace

extern "C" {

const char* ipamir_signature() {
    return "core-trail";
}

void* ipamir_init() {
    try {
        uint64_t epoch = 1;
        size_t sz = sizeof(epoch);
        if (mallctl("epoch", &epoch, &sz, &epoch, sz) != 0) {
            return nullptr;
        }
        return new SolverHandle();
    } catch (...) {
        return nullptr;
    }
}

void ipamir_release(void* solver) {
    delete as_handle(solver);
}

void ipamir_add_hard(void* solver, int32_t lit_or_zero) {
    auto* h = as_handle(solver);
    if (!h) return;

    if (lit_or_zero == 0) {
        if (!h->hard_clause.empty()) {
            h->solver.add_hard_clause(h->hard_clause);
            h->hard_clause.clear();
        }
        return;
    }

    h->hard_clause.push_back((int)lit_or_zero);
}

void ipamir_add_soft_lit(void* solver, int32_t lit, uint64_t weight) {
    auto* h = as_handle(solver);
    if (!h) return;
    if (lit == 0) return;
    h->solver.set_soft((int)-lit, (long)weight);
}

void ipamir_assume(void* solver, int32_t lit) {
    auto* h = as_handle(solver);
    if (!h) return;
    if (lit == 0) return;

    h->assumptions.push_back((int)lit);
}

int ipamir_solve(void* solver) {
    auto* h = as_handle(solver);
    if (!h) return 40;

    try {
        if (h->term_cb && h->term_cb(h->term_state)) {
            h->assumptions.clear();
            return 0;
        }

        (void)h->solver.solve(h->assumptions, false);
        h->assumptions.clear();

        switch (h->solver.get_status()) {
            case rc2::SolveStatus::INTERRUPTED:
                return 0;
            case rc2::SolveStatus::INTERRUPTED_SAT:
                return 10;
            case rc2::SolveStatus::UNSAT:
                return 20;
            case rc2::SolveStatus::OPTIMUM:
                return 30;
            case rc2::SolveStatus::ERROR:
                return 40;
            case rc2::SolveStatus::UNKNOWN:
            default:
                return 0;
        }
    } catch (...) {
        h->assumptions.clear();
        return 40;
    }
}

uint64_t ipamir_val_obj(void* solver) {
    auto* h = as_handle(solver);
    if (!h) return 0;

    try {
        long c = h->solver.get_cost();
        return c < 0 ? 0 : (uint64_t)c;
    } catch (...) {
        return 0;
    }
}

int32_t ipamir_val_lit(void* solver, int32_t lit) {
    auto* h = as_handle(solver);
    if (!h || lit == 0) return 0;

    try {
        int v = h->solver.val((int)lit);
        if (v > 0) return lit;
        if (v < 0) return -lit;
        return 0;
    } catch (...) {
        return 0;
    }
}

void ipamir_set_terminate(void* solver, void* state, int (*terminate)(void*)) {
    auto* h = as_handle(solver);
    if (!h) return;
    h->term_state = state;
    h->term_cb = terminate;
}

}  // extern "C"
