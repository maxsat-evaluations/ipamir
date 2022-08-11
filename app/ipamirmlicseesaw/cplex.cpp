/*
 * Author: Christoph Jabs - christoph.jabs@helsinki.fi
 *
 * Copyright © 2022 Christoph Jabs, University of Helsinki
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the “Software”), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include "cplex.hpp"

#include <cassert>
#include <cmath>
#include <iostream>

using std::cout;

void CplexHittingSetSolver::init() {
  int status;
  env = CPXXopenCPLEX(&status);
  if (env == nullptr)
    processError(status, true, "Could not open CPLEX environment");

  cout << "c Using IBM CPLEX version " << CPXXversion(env)
       << " under IBM's Academic Initiative licensing program\n";

  if (!(mip = CPXXcreateprob(env, &status, "cplex_prob")))
    processError(status, true, "Could not create CPLEX problem");
  if ((status = CPXXchgprobtype(env, mip, CPXPROB_MILP)))
    processError(status, false, "Could not change CPLEX problem to MIP");
  if ((status = CPXXsetdblparam(env, CPX_PARAM_EPAGAP, absGap)))
    processError(status, false, "Could not set CPLEX absolute gap");
  if ((status = CPXXsetdblparam(env, CPX_PARAM_EPGAP, 0.0)))
    processError(status, false, "Could not set CPLEX relative gap");
  if ((status = CPXXsetintparam(env, CPX_PARAM_CLOCKTYPE, 1)))
    processError(status, false, "Could not set CPLEX CLOCKTYPE");
  if ((status = CPXXsetintparam(env, CPX_PARAM_THREADS, 1)))
    processError(status, false, "Could not set CPLEX global threads");
  if ((status = CPXXsetintparam(env, CPX_PARAM_DATACHECK, true)))
    processError(status, false, "Could not set CPLEX Data Check");
  if ((status = CPXXsetintparam(env, CPX_PARAM_MIPEMPHASIS,
                                CPX_MIPEMPHASIS_OPTIMALITY)))
    processError(status, false, "Could not set CPLEX Optimality Emphasis");
  if ((status = CPXXsetintparam(env, CPX_PARAM_POPULATELIM, 256)))
    processError(status, false, "Could not set CPLEX Population limit");
  if ((status = CPXXsetintparam(env, CPX_PARAM_SOLNPOOLCAPACITY, 256)))
    processError(status, false, "Could not set CPLEX Solution Pool limit");
  if ((status = CPXXsetintparam(env, CPX_PARAM_SOLNPOOLINTENSITY, 2)))
    processError(status, false, "Could not set CPLEX Solution Pool limit");
}

CplexHittingSetSolver::~CplexHittingSetSolver() {
  int status;
  if (mip && (status = CPXXfreeprob(env, &mip)))
    processError(status, false, "Could not free the CPLEX model");
  if (env && (status = CPXXcloseCPLEX(&env)))
    processError(status, false, "Could not close the CPLEX environment");
}

void CplexHittingSetSolver::processError(int status, bool terminal,
                                         const char *msg) {
  char errmsg[CPXMESSAGEBUFSIZE];
  auto errstr = CPXXgeterrorstring(env, status, errmsg);
  cout << "c WARN: " << msg << "\n";
  if (errstr)
    cout << "c WARN: " << errmsg << "\n";
  else
    cout << "c WARN: "
         << "error code = " << status << "\n";
  if (terminal)
    solverValid = false;
}

void CplexHittingSetSolver::ensureMapping(uint32_t ex, bool pos) {
  // Create new CPLEX bool variable if one does not already exist
  if (ex >= ex2inMap.size()) {
    ex2inMap.resize(ex + 1, UNDEF);
    exPositive.resize(ex + 1);
  }
  if (ex2inMap[ex] == UNDEF) {
    uint32_t newCplexVar = CPXXgetnumcols(env, mip);
    ex2inMap[ex] = newCplexVar;
    exPositive[ex] = pos;
    addNewVar(ex);
  }
  assert(pos == exPositive[ex]);
}

void CplexHittingSetSolver::addNewVar(uint32_t ex) {
  // Add external variable to CPLEX as a new column
  double lb{0};
  double ub{1};
  char type{'B'};

  double weight{1};

  if (int status =
          CPXXnewcols(env, mip, 1, &weight, &lb, &ub, &type, nullptr)) {
    processError(status, false, "Could not create new CPLEX variable");
    cout << "c WARN: var = " << ex << ", wt = " << weight << "\n";
  }
  nVars++;
}

double
CplexHittingSetSolver::getSolution(std::unordered_set<int32_t> &solution) {
  double objval{};
  int status;
  if ((status = CPXXgetobjval(env, mip, &objval)))
    processError(status, false, "Problem getting MIP objective value");

  int nVars = getNVars();
  std::vector<double> vals(nVars, 0.0);

  if (nVars > 0) {
    status = CPXXgetx(env, mip, vals.data(), 0, nVars - 1);
    if (status)
      processError(status, false, "Problem getting MIP variable assignment");
  }

  solution.clear();
  for (size_t i = 0; i < ex2inMap.size(); i++) {
    uint32_t in = ex2inMap[i];
    if (in != UNDEF) {
      double val = vals[in];
      if (val > 0.99)
        solution.insert(exPositive[i] ? i : -i);
      else if (val >= 0.01) {
        // Found unset value
        solution.clear();
        return -1;
      }
    }
  }
  return objval;
}

int32_t CplexHittingSetSolver::solve(std::unordered_set<int32_t> &solution) {
  // Return a optimal solution over all variables in the solver
  // Return -1 as lower bound if error

  int status;

  if ((status = CPXXsetdblparam(env, CPX_PARAM_TILIM, 1e+75)))
    processError(status, false, "Could not set CPLEX time limit");

  status = CPXXmipopt(env, mip);

  if (status)
    processError(status, false, "CPLEX Failed to optimize MIP");

  status = CPXXgetstat(env, mip);

  if (status == CPXMIP_OPTIMAL || status == CPXMIP_OPTIMAL_TOL) {
    return round(getSolution(solution));
  } else {
    if (status == CPXMIP_TIME_LIM_FEAS || CPXMIP_TIME_LIM_INFEAS)
      return -1;
    char buf[CPXMESSAGEBUFSIZE];
    char *p = CPXXgetstatstring(env, status, buf);
    if (p)
      cout << "c WARN: CPLEX status = " << status << " " << buf << "\n";
    else
      cout << "c WARN: CPLEX status = " << status << "\n";
    return -1;
  }
}

void CplexHittingSetSolver::addCore(const std::unordered_set<int32_t> &core) {
  if (!solverValid)
    return;

  std::vector<int> cplexVars{};
  std::vector<double> cplexCoeff{};
  char sense{'G'};
  double rhs{1};
  CPXNNZ beg{0};

  for (int32_t l : core) {
    ensureMapping(l > 0 ? l : -l, (l >= 0));
    cplexVars.push_back(ex2inMap[l > 0 ? l : -l]);
    cplexCoeff.push_back(1.0);
  }

  if (int status =
          CPXXaddrows(env, mip, 0, 1, cplexVars.size(), &rhs, &sense, &beg,
                      cplexVars.data(), cplexCoeff.data(), nullptr, nullptr))
    processError(status, false, "Could not add core to CPLEX");
}