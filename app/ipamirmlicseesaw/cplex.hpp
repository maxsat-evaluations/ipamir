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

#ifndef _cplex_hpp_INCLUDED
#define _cplex_hpp_INCLUDED

#include <cstdint>
#include <ilcplex/cplexx.h>
#include <limits>
#include <unordered_set>
#include <vector>

// CPLEX interface for minimal hitting set solving
class CplexHittingSetSolver {
protected:
  // API handles
  CPXENVptr env{};
  CPXLPptr mip{};

  const double absGap = 0.75;

  uint32_t nVars{};

  // Variable mappings
  const uint32_t UNDEF = std::numeric_limits<uint32_t>::max();
  std::vector<uint32_t> ex2inMap;
  std::vector<bool>
      exPositive; // Wether the external variable is positive or negative

  void ensureMapping(uint32_t ex, bool pos = true);
  void addNewVar(uint32_t ex);
  double getSolution(std::unordered_set<int32_t> &solution);

  // Stats
  bool solverValid = true;

  // Error processing
  void processError(int, bool, const char *);

public:
  CplexHittingSetSolver(){};
  ~CplexHittingSetSolver();
  void init();

  int32_t solve(std::unordered_set<int32_t> &solution);

  // Adding constraints
  void addCore(const std::unordered_set<int32_t> &core);

  // Stats getter
  inline bool isValid() const { return solverValid; }
  inline uint32_t getNVars() const { return nVars; }
};

#endif
