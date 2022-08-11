#ifndef QUANTOM_HPP
#define QUANTOM_HPP

/********************************************************************************************
quantom.hpp -- Copyright (c) 2014, Tobias Schubert, Sven Reimer

Permission is hereby granted, free of charge, to any person obtaining a copy of this 
software and associated documentation files (the "Software"), to deal in the Software 
without restriction, including without limitation the rights to use, copy, modify, merge, 
publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons 
to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or
substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING 
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
********************************************************************************************/

// Include standard headers.
#include <iostream>
#include <vector>
#include <set>
#include <map>
#include <deque>
#include "configure.hpp"

// Some definitions.
#define QUANTOM_UNKNOWN                  0
#define QUANTOM_SAT                     10
#define QUANTOM_UNSAT                   20
#define QUANTOM_UNSAT_WITH_ASSUMPTION   30

#define UNIFLAG  0x80000000
#define AUNIFLAG 0x7FFFFFFF

struct multiplexervars
{
  multiplexervars ( const std::vector< unsigned int >& muxinputs, const std::vector< unsigned int >& selectinputs, const std::vector< unsigned int >& levels, const std::vector< unsigned int >& weights, unsigned int var):
	muxinput(muxinputs),
	selectinput(selectinputs),
	level(levels),
	weight(weights),
	outputvar(var)
  {
	assert(muxinput.size() == selectinput.size());
	assert(muxinput.size() == levels.size());
	assert(muxinput.size() == weight.size());
	assert(var != 0);
  }

  multiplexervars ( void ):
	muxinput(),
	selectinput(),
	level(),
	weight(),
	outputvar(0)
  {}

  multiplexervars( const multiplexervars& other ) :
     muxinput( other.muxinput ), 
	 selectinput( other.selectinput ),
	 level( other.level ),
	 weight( other.weight ),
	 outputvar( other.outputvar )
  {}

  multiplexervars& operator=( const multiplexervars& other ) {
      muxinput = other.muxinput;
      selectinput = other.selectinput;
      level = other.level;
	  weight = other.weight;
	  outputvar = other.outputvar;
      return *this;
  }
	  
  ~multiplexervars() {};

public:
  std::vector< unsigned int > muxinput;
  std::vector< unsigned int > selectinput;
  std::vector< unsigned int > level;
  std::vector< unsigned int > weight;
  unsigned int outputvar;
};

namespace quantom
{

  // Some forward declarations.
  class Core;
  class Control;
  //class configureset;

  // The "Quantom" class.
  class Quantom
  {

  public:

	// Constructor.
    Quantom ( void );
   
    // Destructor.
    ~Quantom (void);

    // Returns the number of decisions made so far.
    unsigned int decisions (unsigned int id = 0) const;

    // Returns the number of BCP operations performed so far.
    unsigned int bcps (unsigned int id = 0) const;

    // Returns the number of conflicts encountered so far.
    unsigned int conflicts (unsigned int id = 0) const;

	// Returns the number of solutions encountered so far.
    unsigned int solutions (unsigned int id = 0) const;

	// Returns the number of pure literals detected so far.
	unsigned int purelits (unsigned int id = 0) const;

	// Returns the number of don't care literals detected so far.
	unsigned int dontcare (unsigned int id = 0) const;

    // Returns the number of restarts performed so far.
    unsigned int restarts (unsigned int id = 0) const;

    // Returns the number of restart attempts performed so far.
    unsigned int restartAttempts (unsigned int id = 0) const;

    // The number of database simplifications performed so far.
    unsigned int clauseSimplifications (unsigned int id = 0) const;
    unsigned int cubeSimplifications (unsigned int id = 0) const;

	/* Some literal/clause statistics */
	unsigned int clauses(unsigned int id = 0) const;

	unsigned int learnedclauses(unsigned int id = 0) const;
	
	unsigned int remainingcubes(unsigned int id = 0) const;

	unsigned int literals(unsigned int id = 0) const;

	unsigned int lhbrclauses(unsigned int id = 0) const;

	unsigned int remainingliterals(unsigned int id = 0) const;

    unsigned int resolvedVar (unsigned int id = 0) const;

    unsigned int deletedLevels (unsigned int id = 0) const;

    unsigned int mergedLevels (unsigned int id = 0) const;

    unsigned int unitProp (unsigned int id = 0) const;

	unsigned int inprocessings (unsigned int id = 0) const;

	std::vector<unsigned int> bcpstat (unsigned int id = 0) const;

	unsigned int getExtendedResult(unsigned int id = 0) const;

    // Returns a pointer to the satisfying variable assignment (contains invalid data if the formula is unsatisfiable).
    const std::vector<unsigned int>& model (unsigned int id = 0) const;

	void trivialAssignment( unsigned int id = 0 ) const;

	const std::vector< std::vector< unsigned int > >& getEquivalences (unsigned int id = 0) const;

	void getLearnedClauses( std::vector< std::vector< unsigned int > >& learnedclauses, unsigned int id = 0 );

    // Sets the time limit to "t" seconds. There will be no limit at all if "t" is less or equal 0.
    void setCPULimit (double t);

	// Sets the Luby shift to "luby" (default: 12).
	void setLubyShift (unsigned int luby, unsigned int id = 0);

	// Sets the inner and outer loop values for picosat restart modes 
	// default: inner=1024, outer=32
	void setPicoLoops( unsigned int inner, unsigned int outer, unsigned int id = 0);

	// Sets the "Initial Variable Activity" to "val" (default: 1.0).
    void setIVA (unsigned int val, unsigned int id = 0);

	// Sets the "Variable Activity Multiplier" to "mult" (default: 1.05).
    void setActMultiplier (double mult, unsigned int id = 0);

    // Sets the "Initial Phase" mode to "val".
    // 0: FALSE,
    // 1: All flipped (Phase depends also of decision heuristic),
	// 2: Universal flipped (default),
	// 3: Random
    void setIP (unsigned int val, unsigned int id = 0);

    // set sort mode for simplify
    void setCCS( unsigned int val, unsigned int id = 0 );

	// Delete clauses/cubes with high lbd during "simplify"
	void setSimplifyLBD( unsigned int val, unsigned int id = 0 );

	// Restricted Cube Learning
	void setSkipLearning( bool val, unsigned int id = 0 );
    	
	// Switch cache value of variable assignment with restart
	void setSwitchVarSign (bool val, unsigned int id = 0);

	// Sets the Decision Heuristic to "val" (default:3)
	// 0: VSIDS
	// 1: DLIS
	// 2: DLIS+Implication
	// 3: Qube-Style
	void setDecHeu (unsigned int val, unsigned int id = 0);

	// Sets "Restart Heuristic" to "val" (default:0).
	// 0: no restarts
	// 1: with luby-shift
	// 2: picosat-style (inner and outer loops)
	// 3: glucose-style (average decision level)
	void setRestartHeu (unsigned int val, unsigned int id = 0);

	// Reset heuristics during solving
	void setResetHeu (bool val, unsigned int id = 0);

	// Swap cached value durint pos and neg occurence
	void setCacheSwap (bool val, unsigned int id = 0);

	// Use Lazy Hyper Binary Resolution?
   	void setLHBR (bool val, unsigned int id = 0);

	// Minimize cubes?
	void setMinCube (bool val, unsigned int id = 0);

	// Subsumption check with every "simplify()"?
	void setSubsumption (bool val, unsigned int id = 0);

	// Use Agility?
	void setAgility (bool val, unsigned int id = 0);


	// Set the heuristic for the order of the literals in a solution case
	// 000: variable index
	// 001: overall occurence (from lowest to highest)
	// 010: pos/neg occurence (from lowest to highest)
	// 011: cache last solution - prefer literals which are deleted last time
	// 1xx: prefer quantlevel with least remaining literals
	void setSolutionOrder ( unsigned int val, unsigned int id = 0 );

	void setSolutionType ( unsigned int val, unsigned int id = 0 );

	void setMaxIndex ( unsigned int max, unsigned int id = 0 );

	// Set initial polarity to variable.
	// Polarities overwrite every other initial setting
	// "polarity"=true : set inital polarity to "true"
	void setPolarity( unsigned int var, bool polarity, unsigned int id = 0 );

	void setActivity( unsigned int var, double activity, unsigned int id = 0 );

	// Returns the activity for each variable
    std::vector<double> getActivity (unsigned int id = 0) const;

	unsigned int getMaxQLevel ( void ) const;

	void setClauseSort( bool val, unsigned int id = 0 );

	// Returns quant level of "var"
	unsigned int getQuantLevel( unsigned int var ) const;

	// Set initial weight to variable.
	// double value >=0. The initial activity will be multiplied with this weight
	void setWeight( unsigned int var, double weight, unsigned int id = 0 );

	/* Preprocessor options */ 
	// use Preprocessor?
	void setPreprocessor( bool val, unsigned int id = 0 );

	// use Inprocessor?
	void setInprocessor( bool val, unsigned int id = 0 );

	// Perform UPLA?
	void setUPLA (bool val, unsigned int id = 0);

	// use variable elimination?
	void setVarElim ( bool val, unsigned int id = 0 );

	// perform quantifier merge?
	void setQuantMerge (bool val, unsigned int id = 0);

	// Should variable "var" be a "Don't Touch" variable?
	void setDontTouch( unsigned int var, bool dt = true );

	// Return true if "var" is a "Don't Touch" variable
	bool isDontTouch( unsigned int var );

	// solve instance?
	// if not the partial model obtains so far is stored, accessed by "model()"
	void setSolve( bool var, unsigned int id = 0 );

	void setLiftingMode( unsigned int var, unsigned int id = 0 );

	void setBacktracklevel( unsigned int val, unsigned int id = 0 );

	std::vector< unsigned int > getLastConflict( unsigned int id = 0 );

	// Adds a clause to the clause database. Returns FALSE if the CNF formula is unsatisfiable,
    // otherwise TRUE will be returned. Assumes that the solver is on decision level 0 and that 
    // "clause" is not empty. Furthermore, all literals have to be encoded as follows, having 
    // variable indices greater 0:
    //  x3 <--> (3 << 1)     = 6
    // -x3 <--> (3 << 1) + 1 = 7
    // All clauses inserted into the clause database using "addClause()" are assumed to belong to 
    // the original CNF formula. Note, that "clause" will be modified. 
    bool addClause (std::vector<unsigned int>& clause);

	// Adds unit clause
	bool addUnit( unsigned int lit, unsigned int id = 0);

	// Copy instance from core "id1" to core "id2" 
	// Possibly overwrites instance in core "id2"
	void copyInstance( unsigned int id1, unsigned int id2 );

	// Adds soft clause
    bool addSoftClause (std::vector<unsigned int>& clause, unsigned int weight = 1 );

	// Set a soft variable "var" over different levels "levels"
	multiplexervars setSoftVariable ( unsigned int var, const std::vector< unsigned int >& levels, const std::vector< unsigned int >& weight );


	// Sets a variable "to be be maximized"
	// This is the special case where the variable is swapped between two levels
	// Assumes that all variables are already added
	// Also assumes that level of "var" is set correctly (at least level 3)
	// Returns the variable indices of the new introduced variables
	multiplexervars setSoftVariable (unsigned int var, unsigned int nonpreferredinputlevel, unsigned int preferredinputlevel);

    // Simplifies the current CNF formula by performing some preprocessing steps.
    // Returns FALSE if the formula is unsatisfiable, otherwise TRUE will be returned.
    unsigned int preprocess ( unsigned int id = 0 );

    unsigned int inprocess ( unsigned int id = 0 );

	// Load Configuration setting for "id"
	void loadConfiguration( unsigned int id = 0 );

    // Solves the current CNF formula taking the specified assumptions into account.
    // The assumptions have to be encoded in the same way as the literals of a clause (see "addClause()").
    // The return values are as follows:
    //  0 -- time limit exceeded.
    // 10 -- satisfiable.
    // 20 -- unsatisfiable.
    unsigned int solve (std::vector<unsigned int>& assumptions, unsigned int id = 0 );

	// lifts a solution, given by "assumptions"
	// returns the lifted solution
	// liftingmodes:
	// 1 : conflict driven
	// 2 : assumptions driven
	// 3 : combine 1+2
    std::vector< unsigned int > solveLifting( std::vector<unsigned int>& assumptions, unsigned int liftingmode, unsigned int sort = 10, unsigned int id = 0  );
 
    // Solves the current CNF formula. The return values are as follows:
    //  0 -- time limit exceeded.
    // 10 -- satisfiable.
    // 20 -- unsatisfiable.
    unsigned int solve (unsigned int id = 0);

	// Just deduce assumptions, do not solve formula
	unsigned int deduceAssumptions (const std::vector<unsigned int>& assumptions, unsigned int id = 0 );

	/* MaxQBF methods */

	// For MaxQBF we store the soft clauses separately
	struct Softclause {

	  // Constructor
	  Softclause ( unsigned int trigger, const std::vector< unsigned int >& clause ) :
		s_triggerlit(trigger),
		s_clause(clause),
		s_lastAssignment(0),
		s_simultan(0),
		s_contra(0)
	  {};

	  // Destructor
	  ~Softclause ( void ) {}

	  //unsigned int s_trigger;
	  unsigned int s_triggerlit;
	  std::vector<unsigned int> s_clause;

	  int s_lastAssignment;

	  unsigned int s_simultan;
	  unsigned int s_contra;
	};

	/* set maximization mode */ 
	void setMaxMode( unsigned int val, unsigned int id = 0 );

	void setMaxInpro( bool val );

	void setIncrementalMode( bool val );

	void skipInitbyChance( bool val );

	/* preset bounds for maximization */
	void setMaxBounds( int low, int high );

	/* maximize formula */
	unsigned int maxSolve ( int& opt, std::vector< unsigned int >& externalAssumptions, unsigned int mode, unsigned int id = 0);

	unsigned int maxSolveIncremental ( int& optimum, unsigned int mode );

	// Returns optimized soft variable level
	std::vector< std::pair< unsigned int, unsigned int > > getAllSoftvarlevel( void );

	void setGridMode( unsigned int val );
	void setHorizontalWidth( unsigned int val );

	void setCSC( bool val ) { m_setCSC = val; }
	void setTrigger( bool val ) { m_setTrigger = val; }

	void setSplittedWidth( unsigned int width ) { assert( width > 0 ); m_splittedWidth = width; }
	void setMaxWidth( unsigned int width ) { m_maxWidth = width; }

	// Initialize and returns next free variable index
	unsigned int newVariable ( unsigned int level = 1 );

	// load "qlevel" for "lit"
	// return false if something went wrong
	bool setQVarLevel (unsigned int lit, unsigned int qlevel);

	void setupLevelActivities ( unsigned int qlevel);

	void setVerbosity( unsigned int val );

	//void setBlackMagicMode( unsigned int val );

	/* "Black magic mode" (try different configurations pseudo parallel */
	void setRandomBlackMagic( bool val );

	unsigned int MaximizeBlackMagic ( int& opt, std::vector< unsigned int >& assumption );

	unsigned int solveBlackMagic (std::vector<unsigned int>& assumptions);

	configure getBestConfiguration( void );

	static configureset quantom_configs;

	// Debugging
	void printdatabase (unsigned int id = 0) const;

	void dumpCNF( std::ostream& os = std::cout, unsigned int id = 0 ) const;

	void printMaximizeData( void ) const;

	void dumpMaxResult ( const std::vector< unsigned int >& resultmodel, const std::vector< std::pair < unsigned int, std::vector< unsigned int > > >& softclauses, std::ostream& os = std::cout );
 
  private:

	// Datestructure for managing soft clauses in MaxQBF
	struct ConstraintPart {

	  // Constructor
	  ConstraintPart ( unsigned int size ) :
		currenttrigger(),
		softclauses(),
		mincontra(size),
		hardmincontra(0),
		maxsimultan(0),
		hardmaxsimultan(0),
		optimum(size),
		depth(0),
		assumption(0),
		firstsolved(false)
	  {};

	  // current trigger to maximize
	  std::vector< unsigned int > currenttrigger;
	  // List of indices corresponding to soft clause trigger
	  std::vector< Softclause* > softclauses;

	  unsigned int mincontra;
	  unsigned int hardmincontra;
	  unsigned int maxsimultan;
	  unsigned int hardmaxsimultan;
  
	  // The local optimum
	  unsigned int optimum;
	  unsigned int depth;

	  unsigned int assumption;

	  bool firstsolved;

	  unsigned int size ( void ) const { return currenttrigger.size(); }
	};

    // Copy constructor.
    Quantom (const Quantom&);

    // Assignment operator.
    Quantom& operator = (const Quantom&);

	/* Maximizer functions */
	void setNewQLevel( unsigned int var, unsigned int level );

	void bitonicCompare ( ConstraintPart& part, unsigned int i, unsigned int j, bool dir );
	void bitonicSort ( ConstraintPart& part, unsigned int lo, unsigned int n, bool dir );
	void bitonicMerge ( ConstraintPart& part, unsigned int lo, unsigned int n, bool dir );
	void createBitonicMaximizer ( Quantom::ConstraintPart& part );
	void createPartialBitonicMaximizer( unsigned int part );
	void createNextPartialBitonicMaximizer( void );

	void setVerticalBypasses( Quantom::ConstraintPart& part );
	void setHorizontalBypasses( const std::vector< unsigned int >& inputA, const std::vector< unsigned int >& inputB, const ConstraintPart& part );

	unsigned int countSatisfiedSoftClauses( const std::vector< unsigned int >& model, const Quantom::ConstraintPart& part, bool unproceeded = false ) const;
	unsigned int updateTriggerOptimum( void );

	unsigned int calcFirstDepth( unsigned int& result, unsigned int& optimum, int& overalloptimum, std::vector< unsigned int>& externalAssumptions, unsigned int maxmode );
	void mergePartTrigger( ConstraintPart& part1, ConstraintPart& part2, unsigned int size );

	void checkAllConflictingSoftclauses( void );
	void findSorterCSC( ConstraintPart& part );
	void setSorterCSC( ConstraintPart& part );
	void setSoftTrigger( std::vector< unsigned int >& assumptions, ConstraintPart& part );
	void calcSplitWidth( void );

	void copyAndDeleteCore( unsigned int oldID, unsigned int newID, bool keepheuristics );

	// Data members

	// A pointer to the "Control" object.
    Control* m_control;

	// The QBF solving core(s).
	std::vector< Core* > m_cores;

	unsigned int m_blackmagic;
	
	double m_cpulimit;
	double m_starttime;

	// Database for original soft clauses

	// Contains the trigger variables for MaxQBF
	// A list of seperated trigger variables (for partial mode)
	// In complete mode all trigger are stored in "m_partialTriggerVars[0]"
	std::vector< ConstraintPart > m_Constraints;
	std::deque< ConstraintPart > m_pendingConstraints;

	// Storage of soft variables
	std::vector< multiplexervars > m_softvariables;

	std::vector< Softclause > m_softclauses;

	std::vector< unsigned int > m_triggerVars;

	int m_low;
	int m_high;

	bool m_preprocess;
	bool m_maxinprocess;

	bool m_skipinitbychance;

	unsigned int m_stacksize;
	// Variable index triggering all property clauses in incremental mode
	std::vector< unsigned int > m_globalPropertyTrigger;

	// Soft clauses stack for incremental push-pop mechanism
	std::vector< unsigned int > m_softstack;

	unsigned int m_dummyvar;
	unsigned int m_splittedWidth;
	unsigned int m_bypassGrid;
	unsigned int m_horizontalwidth;

	bool m_setCSC;
	bool m_setTrigger;
	unsigned int m_maxWidth;
	
	/* some static counters*/
	unsigned int m_skipped;
	unsigned int m_comparator;

	unsigned int m_verticalbypasses;
	unsigned int m_horizontalbypasses;
  };

}

#endif
