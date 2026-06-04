#include "glucose41_solver.h"
#include "../../global.h"

#include <algorithm>
#include <iostream>

using namespace std;

void Glucose41Solver::add_clause(const vector<int>& clause) {
  if (clause.size() == 1) {
    while ((clause[0] >> 1) > topv) { solver.newVar(); ++topv; }
    solver.addClause(Glucose::toLit(clause[0]));
  } else if (clause.size() == 2) {
    while ((clause[0] >> 1) > topv || (clause[1] >> 1) > topv) { solver.newVar(); ++topv; }
    solver.addClause(Glucose::toLit(clause[0]), Glucose::toLit(clause[1]));
  } else if (clause.size() == 3) {
    while ((clause[0] >> 1) > topv || (clause[1] >> 1) > topv || (clause[2] >> 1) > topv) { solver.newVar(); ++topv; }
    solver.addClause(Glucose::toLit(clause[0]), Glucose::toLit(clause[1]), Glucose::toLit(clause[2]));
  } else {
    for (unsigned i = 0; i < clause.size(); ++i) while ((clause[i] >> 1) > topv) { solver.newVar(); ++topv; }
    Glucose::vec<Glucose::Lit> clause_;
    intsToLits(clause, clause_);
    solver.addClause_(clause_);
  }
}

void Glucose41Solver::add_clause(int lit) {
  while ((lit >> 1) > topv) { solver.newVar(); ++topv; }
  solver.addClause(Glucose::toLit(lit));
}

void Glucose41Solver::add_clause(int lit1, int lit2) {
  while ((lit1 >> 1) > topv || (lit2 >> 1) > topv) { solver.newVar(); ++topv; }
  solver.addClause(Glucose::toLit(lit1), Glucose::toLit(lit2));
}

void Glucose41Solver::add_clause(int lit1, int lit2, int lit3) {
  while ((lit1 >> 1) > topv || (lit2 >> 1) > topv || (lit3 >> 1) > topv) { solver.newVar(); ++topv; }
  solver.addClause(Glucose::toLit(lit1), Glucose::toLit(lit2), Glucose::toLit(lit3));
}

bool Glucose41Solver::solve(vector<int>& assumptions) {
  Glucose::vec<Glucose::Lit> assumps;
  intsToLits(assumptions, assumps);
  return solver.solve(assumps);
}

bool Glucose41Solver::solve(int lit) {
  while ((lit >> 1) > topv) { solver.newVar(); ++topv; }
  return solver.solve(Glucose::toLit(lit));
}

bool Glucose41Solver::solve() {
  return solver.solve();
}

void Glucose41Solver::set_budget(int64_t max_conflicts) {
  solver.setConfBudget(max_conflicts);
}

void Glucose41Solver::set_budget_relative(double budget) {
  uint64_t max_conflicts = max((uint64_t)1, (uint64_t)(solver.conflicts * budget));
  set_budget(max_conflicts);
}

void Glucose41Solver::increase_budget(int64_t max_conflicts) {
  // Glucose 4.1 solver API has setConfBudget but no increaseConfBudget.
  set_budget(max_conflicts);
}

void Glucose41Solver::increase_budget_relative(double budget) {
  uint64_t max_conflicts = max((uint64_t)1, (uint64_t)(solver.conflicts * budget));
  increase_budget(max_conflicts);
}

int Glucose41Solver::solve_limited(vector<int>& assumptions) {
  Glucose::vec<Glucose::Lit> assumps;
  intsToLits(assumptions, assumps);

  Glucose::lbool srv = solver.solveLimited(assumps);
  if (srv == l_True) return 1;
  if (srv == l_False) return 0;
  if (srv == l_Undef) return -1;
  return -2;
}

int Glucose41Solver::core_size() {
  return solver.conflict.size();
}

void Glucose41Solver::get_core(vector<int>& retCore) {
  litsToInts(solver.conflict, retCore, true);
}

void Glucose41Solver::get_model(vector<bool>& retModel) {
  lboolsToBools(solver.model, retModel);
}

bool Glucose41Solver::propagate(vector<int>& assumptions, vector<int>& result) {
  Glucose::vec<Glucose::Lit> assumps;
  intsToLits(assumptions, assumps);
  Glucose::vec<Glucose::Lit> res;
  bool rv = solver.prop_check(assumps, res, 0);
  litsToInts(res, result);
  return rv;
}

bool Glucose41Solver::propagate(int lit, vector<int>& result) {
  while ((lit >> 1) > topv) { solver.newVar(); ++topv; }
  Glucose::vec<Glucose::Lit> assumps;
  assumps.push(Glucose::toLit(lit));
  Glucose::vec<Glucose::Lit> res;
  bool rv = solver.prop_check(assumps, res, 0);
  litsToInts(res, result);
  return rv;
}

bool Glucose41Solver::prop_check(vector<int>& assumptions) {
  Glucose::vec<Glucose::Lit> assumps;
  intsToLits(assumptions, assumps);
  Glucose::vec<Glucose::Lit> res;
  return solver.prop_check(assumps, res, 0);
}

bool Glucose41Solver::prop_check(int lit) {
  while ((lit >> 1) > topv) { solver.newVar(); ++topv; }
  Glucose::vec<Glucose::Lit> assumps;
  assumps.push(Glucose::toLit(lit));
  Glucose::vec<Glucose::Lit> res;
  return solver.prop_check(assumps, res, 0);
}

void Glucose41Solver::get_learnt_clauses(vector<vector<int> >& clauses) {
  (void)clauses;
}

void Glucose41Solver::stats(const std::string& b, std::ostream& out) {
  out << b << "solves: "       << solver.solves << "\n";
  out << b << "starts: "       << solver.starts << "\n";
  out << b << "decisions: "    << solver.decisions << "\n";
  out << b << "propagations: " << solver.propagations << "\n";
  out << b << "conflicts: "    << solver.conflicts << "\n";
  out << b << "num_clauses: "  << solver.nClauses() << "\n";
  out << b << "num_learnts: "  << solver.nLearnts() << "\n";
}

void Glucose41Solver::set_option(std::string&) {
}

std::string Glucose41Solver::version(int l) {
  if (l & 1) return "Glucose Syrup 4.1";
  return "4.1";
}
