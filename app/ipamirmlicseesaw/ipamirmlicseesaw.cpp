/*
 * Author: Christoph Jabs - christoph.jabs@helsinki.fi
 */

#include <iostream>
#include <limits>
#include <unordered_set>
#include <vector>

#include "cplex.hpp"
#include "dataset.hpp"

using namespace std;

extern "C" {
#include "ipamir.h"
}

#define NRULECLAUSES 2 // How many clauses there should be in the learned rule

template <typename T>
void setdiff(const std::unordered_set<T> &first,
             const std::unordered_set<T> &second, std::unordered_set<T> &out) {
  for (const auto &elem : first)
    if (second.find(elem) == second.end())
      out.insert(elem);
}

// An application for finding pareto-optimal solutions to the MLIC encoding
// of a binary dataset following the Seesaw framework
// Both, the Seesaw oracle and the cost function use the IPAMIR solver
bool initialize_solver(void *oracleSolver, unordered_set<int32_t> &universe,
                       string filename) {
  // Variable sets
  vector<int32_t> clauseContainsFeature{}; // b variables of the MLIC paper
  vector<int32_t>
      sampleNotMatchClause{};    // Tseitin variables (z) from the MLIC paper
  vector<int32_t> sampleNoisy{}; // The eta variables from the MLIC paper
  vector<int32_t> clausesEqualTill{}; // The e variables from our paper for
                                      // symmetry breaking

  // Load data
  DataSet data{filename};
  uint32_t nSamples = data.nSamples();
  uint32_t nFeatures = data.nFeatures();

  // Create variables
  int32_t nVars = 0;

  clauseContainsFeature.resize(NRULECLAUSES * nFeatures);
  for (uint32_t i = 0; i < NRULECLAUSES; i++)
    for (uint32_t j = 0; j < nFeatures; j++)
      clauseContainsFeature[i * nFeatures + j] = ++nVars;

  sampleNotMatchClause.resize(nSamples * NRULECLAUSES);
  for (uint32_t i = 0; i < nSamples; i++)
    for (uint32_t j = 0; j < NRULECLAUSES; j++)
      sampleNotMatchClause[i * NRULECLAUSES + j] = ++nVars;

  sampleNoisy.resize(nSamples);
  for (uint32_t i = 0; i < nSamples; i++)
    sampleNoisy[i] = ++nVars;

  // Encoding (added to oracle solver)
  // MLIC
  for (uint32_t i = 0; i < nSamples; i++) {
    // Encode constraint for every sample
    DataSample sample = data.getSample(i);
    if (sample.getClass())
      // Positive sample and CNF or negative sample and DNF
      // -eta -> (X v B)
      for (uint32_t l = 0; l < NRULECLAUSES; l++) {
        ipamir_add_hard(oracleSolver, sampleNoisy[i]);
        for (uint32_t j = 0; j < nFeatures; j++)
          if (sample.getFeature(j))
            ipamir_add_hard(oracleSolver,
                            clauseContainsFeature[l * nFeatures + j]);
        ipamir_add_hard(oracleSolver, 0);
      }
    else {
      // Negative sample and CNF or positive sample and DNF
      // -eta -> V z
      // z -> -(X v B)
      vector<int32_t> clause = {sampleNoisy[i]};
      for (uint32_t l = 0; l < NRULECLAUSES; l++) {
        clause.push_back(sampleNotMatchClause[i * NRULECLAUSES + l]);
        for (uint32_t j = 0; j < nFeatures; j++)
          if (sample.getFeature(j)) {
            ipamir_add_hard(oracleSolver,
                            -sampleNotMatchClause[i * NRULECLAUSES + l]);
            ipamir_add_hard(oracleSolver,
                            -clauseContainsFeature[l * nFeatures + j]);
            ipamir_add_hard(oracleSolver, 0);
          }
      }
      for (int32_t l : clause)
        ipamir_add_hard(oracleSolver, l);
      ipamir_add_hard(oracleSolver, 0);
    }
  }

  // Define universe
  universe.reserve(sampleNoisy.size());
  for (uint32_t i = 0; i < sampleNoisy.size(); i++)
    universe.insert(sampleNoisy[i]);

  for (uint32_t l = 0; l < NRULECLAUSES; l++)
    for (uint32_t j = 0; j < nFeatures; j++)
      ipamir_add_soft_lit(oracleSolver,
                          clauseContainsFeature[l * nFeatures + j], 1);

  return true;
}

bool coreExt(void *oracleSolver, const unordered_set<int32_t> &universe,
             unordered_set<int32_t> &hs, uint32_t f,
             unordered_set<int32_t> &core) {
  if (f == 0) {
    // If f == 0, terminate
    core.clear();
    return true;
  }

  // Seesaw improved optimization core extraction
  // (Only applicable because oracle is anti-monotonic)

  // Assume hitting set is sorted (which it is here)

  vector<int32_t> assumps{};
  for (const int32_t &e : universe)
    if (hs.find(e) == hs.end())
      assumps.push_back(-e);

  uint32_t keep = 0;
  int32_t res;
  while (keep < assumps.size()) {
    for (uint32_t i = 0; i < assumps.size(); i++)
      if (i != keep)
        ipamir_assume(oracleSolver, assumps[i]);

    res = ipamir_solve(oracleSolver);
    if (res != 20 && res != 30) {
      cout << "ERROR: IPAMIR calls should always return OPTIMAL or UNSAT\n";
      return false;
    } else if (res == 20) {
      // UNSAT
      hs.insert(-assumps[keep]);
      assumps.erase(assumps.begin() + keep);
    } else {
      // OPT
      uint64_t fNew = ipamir_val_obj(oracleSolver);
      if (fNew >= f) {
        hs.insert(-assumps[keep]);
        assumps.erase(assumps.begin() + keep);
      } else
        keep++;
    }
  }

  setdiff(universe, hs, core);

  return true;
}

bool seesaw(CplexHittingSetSolver &costSolver, void *oracleSolver,
            const unordered_set<int32_t> &universe) {
  int32_t res;
  unordered_set<int32_t> hs{};
  uint32_t g, f{};
  uint32_t bestG{numeric_limits<int32_t>::max()};
  uint32_t bestF{numeric_limits<uint32_t>::max()};

  while (true) {
    // Solve hitting set problem
    hs.clear();
    int32_t ret = costSolver.solve(hs);
    if (ret == -1) {
      cout << "ERROR: Solving hitting set problem with CPLEX returned an "
              "error\n";
      return false;
    }
    assert(ret >= 0);
    g = ret;
    // Assume hitting set in oracle solver
    for (const int32_t &l : universe)
      if (hs.find(l) == hs.end())
        ipamir_assume(oracleSolver, -l);

    // Check if pareto point found
    if (g > bestG) {
      cout << "c SOLUTION: pareto-optimal solution found for objective values "
              "inc="
           << bestG << ", and dec=" << bestF << "\n";
      bestG = numeric_limits<uint32_t>::max();

      if (bestF <= NRULECLAUSES) {
        cout << "c Terminating algorithm due to only trivial solutions left\n";
        break;
      }
    }

    // Check oracle
    res = ipamir_solve(oracleSolver);
    if (res != 20 && res != 30) {
      cout << "ERROR: IPAMIR calls should always return OPTIMAL or UNSAT\n";
      return false;
    } else if (res == 20)
      // UNSAT
      f = numeric_limits<uint32_t>::max();
    else
      // SAT
      f = ipamir_val_obj(oracleSolver);

    if (f < bestF) {
      bestF = f;
      bestG = g;
    }

    // Extract core
    unordered_set<int32_t> core{};
    if (!coreExt(oracleSolver, universe, hs, f, core))
      return false;
    if (core.size() == 0) {
      // Report last solution
      if (bestF < numeric_limits<uint32_t>::max())
        cout
            << "c SOLUTION: pareto-optimal solution found for objective values "
               "inc="
            << bestG << ", and dec=" << bestF << "\n";
      cout << "c Terminating algorithm due to infeasible hitting set problem\n";
      break;
    }
    costSolver.addCore(core);
  }

  return true;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    cout << "USAGE: ./ipamirmlicseesaw <input_file_name>\n\n";
    cout << "where <input_file_name> is a binary CSV file separated by ';' "
            "as specified in the benchmark description.\n\n";
    cout << "See ./inputs for example input files.\n";
    return 1;
  }

  unordered_set<int32_t> universe{};

  CplexHittingSetSolver costSolver;
  costSolver.init();
  void *oracleSolver = ipamir_init();

  cout << "c Solving with solver: " << ipamir_signature() << "\n";
  cout << "c (Hitting set extraction in CPLEX)\n";

  if (!initialize_solver(oracleSolver, universe, argv[1])) {
    cout << "c ERROR: error while initializing, terminating!\n";
    return 1;
  }

  if (!seesaw(costSolver, oracleSolver, universe)) {
    cout << "c ERROR: error while solving; terminating!\n";
    return 1;
  }

  ipamir_release(oracleSolver);
}