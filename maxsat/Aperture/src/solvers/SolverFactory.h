#pragma once

#include "../ATypes.h"
#include "SatSolver.h"
#include "cadical/Solver.h"
#include "glucose/Solver.h"
#include "kissat/Solver.h"
#include "topor/Solver.h"

namespace Aperture {
template <ValidLiteral TLit = int32_t>
class SolverFactory {
 public:
  static std::unique_ptr<SatSolver<TLit>> CreateSolver(
      SolverType solver_type,
      const std::unordered_map<std::string, std::string>& params = {}) {
    switch (solver_type) {
      case SolverType::TOPOR:
        return std::make_unique<ToporSatSolver<TLit>>(params);
      case SolverType::CADICAL:
        return std::make_unique<CadicalSatSolver<TLit>>(params);
      case SolverType::GLUCOSE:
        return std::make_unique<GlucoseSatSolver<TLit>>(params);
      case SolverType::KISSAT:
        return std::make_unique<KissatSatSolver<TLit>>(params);
      default:
        throw std::invalid_argument("Unknown solver type.");
    }
  }
};
};  // namespace Aperture