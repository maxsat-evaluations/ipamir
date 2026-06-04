#ifndef GLUCOSE41_SOLVER_H
#define GLUCOSE41_SOLVER_H

#include "../../satsolver.h"
#include "core/Solver.h"

#include <vector>

class Glucose41Solver : cgss2::SATsolver {
  Glucose::Solver solver;
  int topv;

  void intsToLits(const std::vector<int>& lits, Glucose::vec<Glucose::Lit>& rLits) {
    rLits.capacity(lits.size());
    for (unsigned i = 0; i < lits.size(); ++i) {
      while ((lits[i] >> 1) > topv) { solver.newVar(); ++topv; }
      rLits.push_(Glucose::toLit(lits[i]));
    }
  }

  void litsToInts(Glucose::vec<Glucose::Lit>& lits, std::vector<int>& rLits, bool neg = false) {
    rLits.resize(lits.size());
    if (!neg) {
      for (int i = 0; i < lits.size(); ++i) rLits[i] = Glucose::toInt(lits[i]);
    } else {
      for (int i = 0; i < lits.size(); ++i) rLits[i] = Glucose::toInt(~lits[i]);
    }
  }

  void lboolsToBools(Glucose::vec<Glucose::lbool>& lbools, std::vector<bool>& bools) {
    bools.resize(lbools.size());
    for (int i = 0; i < lbools.size(); ++i) bools[i] = (lbools[i] == l_True ? 1 : 0);
  }

public:
  void add_clause(const std::vector<int>& clause);
  void add_clause(int lit);
  void add_clause(int lit1, int lit2);
  void add_clause(int lit1, int lit2, int lit3);

  bool solve(std::vector<int>& assumptions);
  bool solve(int lit);
  bool solve();

  void set_budget(int64_t max_conflicts);
  void set_budget_relative(double budget);
  void increase_budget(int64_t max_conflicts);
  void increase_budget_relative(double budget);

  int solve_limited(std::vector<int>& assumptions);
  int core_size();
  void get_core(std::vector<int>& retCore);
  void get_model(std::vector<bool>& retModel);

  bool propagate(std::vector<int>& assumptions, std::vector<int>& result);
  bool propagate(int lit, std::vector<int>& result);
  bool prop_check(std::vector<int>& assumptions);
  bool prop_check(int lit);

  void get_learnt_clauses(std::vector<std::vector<int> >& clauses);
  void stats(const std::string& b, std::ostream& out);
  void set_option(std::string& opt);

  static std::string version(int l = 0);

  Glucose41Solver() : topv(-1) {
    solver.setIncrementalMode();
  }
};

#endif
