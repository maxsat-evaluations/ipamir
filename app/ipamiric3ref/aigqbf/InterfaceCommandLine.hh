#ifndef INTERFACECOMMANDLINE_HH
#define INTERFACECOMMANDLINE_HH

#include <vector>

#define DUMPQCNF

namespace qbf {
class Instance;
class Solution2;
}  // namespace qbf
struct CommandLineOptions;

class InterfaceCommandLine {
 public:
  InterfaceCommandLine();
  ~InterfaceCommandLine();

  /**
   * @brief CreateNewVariables - before solving
   *        ONLY CALL ONCE
   *        undefined behaviour if called multiple times
   * @param numVar - total number of variables
   */
  void CreateNewVariables(int numVar);
  /**
   * @brief SetNumberOfSoftClauses - before solving
   *        ONLY CALL ONCE
   *        undefined behaviour if called multiple times
   * @param numVar - total number of soft clauses
   */
  void SetNumberOfSoftClauses(int numVar);
  /**
   * @brief AddQuantifierBlock - before solving
   * @param variables
   * @param quantifier
   *              0 Existential
   *              1 Universal
   */
  void AddQuantifierBlock(std::vector<int>& variables, int quantifier);
  /**
   * @brief AddClause - before solving
   * @param literals - literals in ODD EVEN style
   */
  void AddClause(std::vector<int>& literals);
  /**
   * @brief AddSoftClause - before solving
   * @param literals - literals in ODD EVEN style
   */
  void AddSoftClause(std::vector<int>& literals);

  /**
   * @brief SolveMaxQBF starting the solve routine
   * @return the same status value as GetStatus
   *          10 satisfiable
   *          -1 unknown
   *          20 unsatisfiable
   *          ...
   */
  int SolveMaxQBF();

  /**
   * @brief SetMaxSATSolver [optional, pacose is already set!]
   * @param pacose - if true, then pacose; false then its antom
   */
  void SetMaxSATSolver(bool pacose = true);

  /**
   * @brief GetStatus
   * @return  10 satisfiable
   *          -1 unknown
   *          20 unsatisfiable
   *          ...
   */
  int GetStatus() { return _status; }
  /**
   * @brief GetOValue - after the SolveMaxQBF routine
   * @return value of unsatisfiable soft clauses
   */
  int GetOValue();
  /**
   * @brief GetWeightOfSoftClauses  - after the SolveMaxQBF routine
   * @return total weight of all soft clauses (currently same as number, as we
   * have unweighted)
   */
  int GetWeightOfSoftClauses();
  /**
   * @brief GetModel  - after the SolveMaxQBF routine
   * @param index - variable index
   * @return value of variable in POSITIVE NEGATIVE style
   */
  int GetModel(unsigned index);
  /**
   * @brief GetOverallTime  - after the SolveMaxQBF routine
   * @return overall solving time
   */
  double GetOverallTime() { return _overallTime; };
  /**
   * @brief GetMaxSATTime - after the SolveMaxQBF routine
   * @return time of MaxSAT solver
   */
  double GetMaxSATTime() { return _maxSATTime; };

  /**
   * @brief ActivatePreProcessor
   * @param value -- boolean value if preprocessor is going to be activated
   * @return returns _opt.preprocessor value
   */
  bool ActivatePreProcessor(bool value);

  /**
   * @brief SetQtreeLinear
   * @param value
   *          false = Linear (most solved but slower for critical instances)
   *          true  = Orig (standard setting - fastest but 4% less instances
   * solved)
   */
  void SetQtreeMode(bool value);

  /**
   * @brief DumpQCNF writes the qcnf file if DUMPQCNF is defined
   * @return true if written
   */
  bool DumpQCNF();

 private:
  bool _pacose;
  bool _numberOfClausesSetOnce;
  int _status;
  int _level;
  int _lastQuantor;
  int _variablesCount;
  int _clausesCount;
  int _softClausesCount;
  int _univVariablesCount;
  int _existVariablesCount;
  double _overallTime;
  double _maxSATTime;
  std::vector<int> _maxSolution;
  qbf::Instance& _inst;
  CommandLineOptions& _opt;
  qbf::Solution2* _solution;
  std::vector<std::pair<std::vector<int>, int>> _quantifierBlocks;
  std::vector<std::vector<int>> _CNF;
  std::vector<std::vector<int>> _SoftClauses;
};

#endif  // INTERFACECOMMANDLINE_HH
