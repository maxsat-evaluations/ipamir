#include "../Aperture.h"
#include "../constraints/Adder.h"

using namespace std;
using namespace Aperture;

template <ValidLiteral TLit, ValidWeight TWeight>
void Solver<TLit, TWeight>::LSU(Lits<TLit> assumps,
                                WLits<TLit, TWeight> targets,
                                EncoderType encoder_type,
                                const bool fix_model_value,
                                function<bool()> ShouldExitAfterSolutionFound) {
  logger_.Log("Starting LSU with encoder type {}.",
              static_cast<int>(encoder_type));

  vector<TLit> assumptions(assumps.begin(), assumps.end());
  bool problem_weighted = false;
  for (const auto &[weight, lit] : targets) {
    problem_weighted |= (weight != 1);
  }

  reference_wrapper<SatSolver<TLit>> complete_part_solver = *solver_;
  unique_ptr<SatSolver<TLit>> complete_solver_ptr;
  if (solver_options_.mrs_beaver_use_complete_part_solver) {
    logger_.Log(VerbosityLevel::VERBOSE,
                "Using a different complete part SAT solver for LSU.");

    complete_solver_ptr =
        BuildSecondarySolver(solver_options_.mrs_beaver_complete_part_solver,
                             targets, solver_options_.solve_optimistically,
                             solver_options_.use_target_bumping);
    complete_part_solver = *complete_solver_ptr;
  }

  auto CNewVar = [&]() {  // Create new variable in both solvers
    TLit v = NewVar();
    if (solver_options_.mrs_beaver_use_complete_part_solver) {
      complete_part_solver.get().NewVar();
    }
    return v;
  };
  auto CAddClause = [&](Lits<TLit> clause) {  // Add clause to both solvers
    bool res = AddClause(clause);
    if (solver_options_.mrs_beaver_use_complete_part_solver) {
      res = res && complete_part_solver.get().AddClause(clause);
    }
    return res;
  };

  switch (encoder_type) {
    case EncoderType::TOTALIZER: {
      Totalizer<TLit, TWeight> complete_part_totalizer(CNewVar, CAddClause);

      optional<TLit> selector = nullopt;
      if (!fix_model_value) {
        selector = CNewVar();
        assumptions.push_back(-selector.value());
      } else {
        latest_maxsat_fixed_model_value_ = true;
      }

      // Each totalizer bit corresponds to a possible upper bound on the cost
      vector<pair<TWeight, TLit>> tot;
      if (problem_weighted) {
        tot = move(complete_part_totalizer.EncodeGenTotalizer(
            targets, selector, latest_maxsat_value_ - 1));
      } else {
        vector<TLit> target_lits;
        target_lits.reserve(targets.size());
        for (const auto &[weight, lit] : targets) {
          target_lits.push_back(lit);
        }
        vector<TLit> totalizer = complete_part_totalizer.EncodeTotalizer(
            target_lits, selector, latest_maxsat_value_ - 1, true);
        tot.reserve(totalizer.size());
        for (size_t i = 0; i < totalizer.size(); i++) {
          tot.emplace_back(1, totalizer[i]);
        }
      }

      int64_t i = static_cast<int64_t>(tot.size()) - 1;
      bool exit_due_to_optimality = false;

      auto BoundWithTotalizer = [&]() {
        if (i >= 0) {
          if (fix_model_value) {
            TLit tot_bound_clause[] = {-tot[i].second};
            if (!CAddClause(tot_bound_clause)) {
              // If adding the clause itself causes UNSAT, then we are done
              latest_maxsat_optimal_ = true;
              return false;
            }
          } else {
            assumptions.push_back(-tot[i].second);
          }
        }
        return true;
      };

      while (i >= 0) {
        // Find the current best known upper bound and block just below it
        if (problem_weighted) {
          while (i >= 0 && tot[i].first >= latest_maxsat_value_) {
            if (!BoundWithTotalizer()) {
              exit_due_to_optimality = true;
              break;
            }
            i--;
          }
        } else {
          i = static_cast<int64_t>(latest_maxsat_value_) - 1;
          if (!BoundWithTotalizer()) exit_due_to_optimality = true;
        }

        if (exit_due_to_optimality) break;

        // Try to find a better solution under the new bound
        SolverStatus status = complete_part_solver.get().Solve(assumptions);
        assert(status == SolverStatus::SAT || status == SolverStatus::UNSAT);
        if (status == SolverStatus::SAT) {
          MaxSATSolutionFoundFrom(complete_part_solver.get(), targets);
          if (ShouldExitAfterSolutionFound()) {
            return;
          }
        } else if (status == SolverStatus::UNSAT) {
          latest_maxsat_optimal_ = true;
          break;
        } else {
          logger_.Log(VerbosityLevel::VERBOSE,
                      "LSU returned unexpected status {}, stopping.",
                      static_cast<int>(status));
          return;
        }
      }
      break;
    }
    case EncoderType::ADDER:
    default: {
      Adder<TLit, TWeight> complete_part_adder(CNewVar, CAddClause);

      AdderBits<TLit> adder_bound =
          complete_part_adder.LessThanOrEqualBits(targets);
      const vector<TLit> &adder_bound_lits = adder_bound.Bits();
      assumptions.insert(assumptions.end(), adder_bound_lits.begin(),
                         adder_bound_lits.end());
      size_t num_bound_bits = adder_bound.size();
      size_t num_assumptions_before_bound = assumptions.size() - num_bound_bits;

      while (latest_maxsat_value_ > 0) {
        Adder<TLit, TWeight>::UpdateLEQBound(adder_bound,
                                             latest_maxsat_value_ - 1);
        for (size_t i = 0; i < adder_bound.size(); i++) {
          assumptions[num_assumptions_before_bound + i] = adder_bound[i];
        }
        SolverStatus status = complete_part_solver.get().Solve(assumptions);
        assert(status == SolverStatus::SAT || status == SolverStatus::UNSAT);
        if (status == SolverStatus::SAT) {
          MaxSATSolutionFoundFrom(complete_part_solver.get(), targets);
          if (ShouldExitAfterSolutionFound()) {
            return;
          }
        } else if (status == SolverStatus::UNSAT) {
          latest_maxsat_optimal_ = true;
          break;
        } else {
          logger_.Log(VerbosityLevel::VERBOSE,
                      "LSU returned unexpected status {}, stopping.",
                      static_cast<int>(status));
          return;
        }
      }
      if (latest_maxsat_value_ == 0) {
        latest_maxsat_optimal_ = true;
      }
    }
  }
  logger_.Log("LSU finished with optimal value: {}.", latest_maxsat_value_);
}

template class Solver<int32_t, uint64_t>;