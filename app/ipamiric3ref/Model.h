/*********************************************************************
 Copyright (c) 2013, Aaron Bradley

 Permission is hereby granted, free of charge, to any person obtaining
 a copy of this software and associated documentation files (the
 "Software"), to deal in the Software without restriction, including
 without limitation the rights to use, copy, modify, merge, publish,
 distribute, sublicense, and/or sell copies of the Software, and to
 permit persons to whom the Software is furnished to do so, subject to
 the following conditions:

 The above copyright notice and this permission notice shall be
 included in all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *********************************************************************/

#ifndef MODEL_H_INCLUDED
#define MODEL_H_INCLUDED

#include <algorithm>
#include <set>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <bitset>
#include <queue>
#include <fstream>

extern "C"
{
#include "aiger.h"
#include "qdpll.h"
}
#include "config.h"
#include "InterfaceCommandLine.hh" //AIGSolve MaxQBF
#include "Solver.h"
#include "SimpSolver.h"
#include "quantom.hpp"
#include "Pacose.h"
#include "Settings.h"
#ifdef COMPILE_WITH_GUROBI
#include "gurobi_c++.h"
#endif

//incremental MaxSAT
#include "ipamir.h"

// Read it and weep: yes, it's in a header file; no, I don't want to
// have std:: all over the place.
using namespace std;

// A row of the AIGER spec: lhs = rhs0 & rhs1.
struct AigRow
{
	AigRow(MyMinisat::Lit _lhs, MyMinisat::Lit _rhs0, MyMinisat::Lit _rhs1) :
			lhs(_lhs), rhs0(_rhs0), rhs1(_rhs1)
	{
	}
	MyMinisat::Lit lhs, rhs0, rhs1;
};
// Intended to hold the AND section of an AIGER spec.
typedef vector<AigRow> AigVec;
typedef vector<MyMinisat::Lit> LitVec;

typedef MyMinisat::vec<MyMinisat::Lit> MSLitVec;

// A lightweight wrapper around MyMinisat::Var that includes a name.
class Var
{
public:
	Var(const string name, MyMinisat::Var &gvi)
	{
		_var = gvi++;
		_name = name;
	}
	size_t index() const
	{
		return (size_t) _var;
	}
	MyMinisat::Var var() const
	{
		return _var;
	}
	MyMinisat::Lit lit(bool neg) const
	{
		return MyMinisat::mkLit(_var, neg);
	}
	string name() const
	{
		return _name;
	}
	//static MyMinisat::Var gvi;  // aligned with solvers
private:
	MyMinisat::Var _var;        // corresponding MyMinisat::Var in *any* solver
	string _name;
};

typedef vector<Var> VarVec;

class VarComp
{
public:
	bool operator()(const Var &v1, const Var &v2)
	{
		return v1.index() < v2.index();
	}
};
typedef set<Var, VarComp> VarSet;
typedef set<MyMinisat::Lit> LitSet;

/*** ternary bool ***/
//lookup table for 0/1/x
const std::vector<std::vector<uint8_t>> andTab
{
{ 0, 0, 0 },
{ 0, 1, 2 },
{ 0, 2, 2 } };
const std::vector<std::vector<uint8_t>> orTab
{
{ 0, 1, 2 },
{ 1, 1, 1 },
{ 2, 1, 2 } };
const std::vector<uint8_t> notTab
{ 1, 0, 2 };
//ternary bool {0=false,1=true,2=dont care}
struct ternBool
{
	uint8_t value;

	ternBool()
	{
	}
	ternBool(uint8_t value) :
			value(value)
	{
		assert(value <= 2);
	}

	bool operator ==(ternBool b) const
	{
		return (b.value == value);
	}
	bool operator !=(ternBool b) const
	{
		return (b.value != value);
	}

	ternBool operator &&(ternBool b)
	{
		return ternBool(andTab[this->value][b.value]);
	}

	ternBool operator ||(ternBool b)
	{
		return ternBool(orTab[this->value][b.value]);
	}

	ternBool operator !()
	{
		return ternBool(notTab[this->value]);
	}
};

struct SupportVar
{
	unsigned int index;
	unsigned int outDegr;

	SupportVar(const SupportVar &sv) :
			index(sv.index), outDegr(sv.outDegr)
	{
	} //copy constructor
	SupportVar(SupportVar &&sv) :
			index(sv.index), outDegr(sv.outDegr) //move constructor
	{
		sv.index = 0;
		sv.outDegr = 0;
	}

	//copy assignment
	SupportVar& operator=(const SupportVar &sv)
	{
		this->index = sv.index;
		this->outDegr = sv.outDegr;
		return *this;
	}

	SupportVar()
	{
	}
	SupportVar(unsigned int i, unsigned int o) :
			index(i), outDegr(o)
	{
	}

	bool operator ==(SupportVar sv) const
	{
		return (index == sv.index);
	}
	bool operator !=(SupportVar sv) const
	{
		return (index != sv.index);
	}
};

class SuppVarComp
{
public:
	bool operator()(const SupportVar &v1, const SupportVar &v2)
	{
		if (v1.outDegr > v2.outDegr)
			return true;  // prefer higher out degree
		if (v1.outDegr < v2.outDegr)
			return false;
		if (v1.index < v2.index)
			return true;  // canonical final decider
		return false;
	}
};
typedef std::set<SupportVar, SuppVarComp> SvPrioQueue;

/*** ternary bool ***/
const size_t ternsimBitWidth = 64;
const int gBitwidth = 20;
typedef std::vector<std::bitset<gBitwidth>> PatternFix;

// Structure and methods for imposing priorities on literals
// through ordering the dropping of literals in mic (drop leftmost
// literal first) and assumptions to Minisat.  The implemented
// ordering prefers to keep literals that appear frequently in
// addCube() calls.
struct HeuristicLitOrder
{
	HeuristicLitOrder() :
			_mini(1 << 20)
	{
	}
	vector<float> counts;
	size_t _mini;
	void count(const LitVec &cube)
	{
		assert(!cube.empty());
		// assumes cube is ordered
		size_t sz = (size_t) MyMinisat::toInt(MyMinisat::var(cube.back()));
		if (sz >= counts.size())
			counts.resize(sz + 1);
		_mini = (size_t) MyMinisat::toInt(MyMinisat::var(cube[0]));
		for (LitVec::const_iterator i = cube.begin(); i != cube.end(); ++i)
			counts[(size_t) MyMinisat::toInt(MyMinisat::var(*i))] += 1;
	}
	void decay()
	{
		for (size_t i = _mini; i < counts.size(); ++i)
			counts[i] *= 0.99;
	}
};

struct SlimLitOrder
{
	HeuristicLitOrder *heuristicLitOrder;

	SlimLitOrder()
	{
	}

	bool operator()(const MyMinisat::Lit &l1, const MyMinisat::Lit &l2) const
	{
		// l1, l2 must be unprimed
		size_t i2 = (size_t) MyMinisat::toInt(MyMinisat::var(l2));
		if (i2 >= heuristicLitOrder->counts.size())
			return false;
		size_t i1 = (size_t) MyMinisat::toInt(MyMinisat::var(l1));
		if (i1 >= heuristicLitOrder->counts.size())
			return true;
		return (heuristicLitOrder->counts[i1]
				< heuristicLitOrder->counts[i2]);
	}
};

// A simple wrapper around an AIGER-specified invariance benchmark.
// It specifically disallows primed variables beyond those required to
// express the (property-constrained) transition relation and the
// primed error constraint.  Variables are kept aligned with the
// variables of any solver created through newSolver().
class Model
{
public:
	// Construct a model from a vector of variables, indices indicating
	// divisions between variable types, constraints, next-state
	// functions, the error, and the AND table, closely reflecting the
	// AIGER format.  Easier to use "modelFromAiger()", below.
	Model(vector<Var> _vars, size_t _inputs, size_t _latches, size_t _reps,
			LitVec _init, LitVec _constraints, LitVec _nextStateFns,
			MyMinisat::Lit _err, AigVec _aig, MyMinisat::Lit _nand,
			MyMinisat::Var _gvi) :
			vars(_vars), inputs(_inputs), latches(_latches), reps(_reps), primes(
					_vars.size()), primesUnlocked(true), aig(_aig), init(_init), constraints(
					_constraints), nextStateFns(_nextStateFns), _error(_err), inits(
			NULL), sslv(NULL), sslvError(NULL), sslvReduced(NULL), sslv01X(
			NULL), sslvImplgraph(NULL), sslvImplgraphSucc(NULL), sslvImplgraphBad(
					NULL), sslvLifting(NULL), _nand(_nand), gvi(_gvi), sslvQBF(
					NULL), sslvILP(NULL)
	{
		// create primed inputs and latches in known region of vars
		for (size_t i = inputs; i < reps; ++i)
		{
			stringstream ss;
			ss << vars[i].name() << "'";
			vars.push_back(Var(ss.str(), gvi));
		}
		// same with primed error
		_primedError = primeLit(_error);
		// same with primed constraints
		for (LitVec::const_iterator i = constraints.begin();
				i != constraints.end(); ++i)
			primeLit(*i);

		/* revPDR po gen */
		transitionSupport.resize(endLatches()->var() - beginLatches()->var());
		circuitNodeSuccessors.resize(getMaxVar() + 1);
		outDegrSuppVars.resize(endLatches()->var(), 0);
	}
	~Model();

	// Returns the Var of the given MyMinisat::Lit.
	const Var& varOfLit(MyMinisat::Lit lit) const
	{
		MyMinisat::Var v = MyMinisat::var(lit);
		assert((unsigned ) v < vars.size());
		return vars[v];
	}

	// Returns the name of the MyMinisat::Lit.
	string stringOfLit(MyMinisat::Lit lit) const
	{
		stringstream ss;
		if (MyMinisat::sign(lit))
			ss << "~";
		ss << varOfLit(lit).name();
		return ss.str();
	}

	// Returns the primed Var/MyMinisat::Lit for the given
	// Var/MyMinisat::Lit.  Once lockPrimes() is called, primeVar() fails
	// (with an assertion violation) if it is asked to create a new
	// variable.
	const Var& primeVar(const Var &v, MyMinisat::SimpSolver *slv = NULL);
	MyMinisat::Lit primeLit(MyMinisat::Lit lit, MyMinisat::SimpSolver *slv = NULL)
	{
		const Var &pv = primeVar(varOfLit(lit), slv);
		return pv.lit(MyMinisat::sign(lit));
	}
	MyMinisat::Var unprimeVar(MyMinisat::Var var)
	{
		size_t i = (size_t) var;
		if (i >= primes && i < primes + reps - inputs)
			return (MyMinisat::Var) (i - primes + inputs);
		else
			return var;
	}
	MyMinisat::Lit unprimeLit(MyMinisat::Lit lit)
	{
		size_t i = (size_t) var(lit);
		if (i >= primes && i < primes + reps - inputs)
			return MyMinisat::mkLit((MyMinisat::Var) (i - primes + inputs),
					sign(lit));
		else
			return lit;
	}

	// Once all primed variables have been created, it locks the Model
	// from creating any further ones.  Then Solver::newVar() may be
	// called safely.
	//
	// WARNING: Do not call Solver::newVar() until lockPrimes() has been
	// called.
	void lockPrimes()
	{
		primesUnlocked = false;
	}

	// MyMinisat::Lits corresponding to true/false.
	MyMinisat::Lit btrue() const
	{
		return MyMinisat::mkLit(vars[0].var(), true);
	}
	MyMinisat::Lit bfalse() const
	{
		return MyMinisat::mkLit(vars[0].var(), false);
	}

	// Primary inputs.
	VarVec::const_iterator beginInputs() const
	{
		return vars.begin() + inputs;
	}
	VarVec::const_iterator endInputs() const
	{
		return vars.begin() + latches;
	}

	// Latches.
	VarVec::const_iterator beginLatches() const
	{
		return vars.begin() + latches;
	}
	VarVec::const_iterator endLatches() const
	{
		return vars.begin() + reps;
	}

	// Next-state function for given latch.
	MyMinisat::Lit nextStateFn(const Var &latch) const
	{
		assert(latch.index() >= latches && latch.index() < reps);
		return nextStateFns[latch.index() - latches];
	}

	// Error and its primed form.
	MyMinisat::Lit error() const
	{
		return _error;
	}
	MyMinisat::Lit primedError() const
	{
		return _primedError;
	}

	// Invariant constraints
	const LitVec& invariantConstraints()
	{
		return constraints;
	}

	MyMinisat::SimpSolver* getTransSslv() const
	{
		return sslv;
	}
	MyMinisat::SimpSolver* getErrorSslv() const
	{
		return sslvError;
	}
	MyMinisat::SimpSolver* getTransSslvRed()
	{
		return sslvReduced;
	}
	MyMinisat::SimpSolver* getTransSslv01X()
	{
		return sslv01X;
	}
	MyMinisat::SimpSolver* getTransSslvILP()
	{
		return sslvILP;
	}
	// Creates a Solver and initializes its variables to maintain
	// alignment with the Model's variables.
	MyMinisat::Solver* newSolver() const;
	/*
	 * new MAX SAT antom solver:
	 * this call already initializes the antom solver with
	 * a 01X encoded set of soft clauses (latches).
	 * Aligns auxiliary variable vector -> not constant
	 */
#ifdef maxsatpogen
	antom::Antom* newAntom();
#endif /* maxsatpogen */
	QDPLL* newDepQBF(vector<VarID> &muxAuxExist, vector<VarID> &muxAuxForall,
			vector<VarID> &muxSelect, bool revpdr);
	Pacose* newPacose(vector<unsigned int> *dontCares = NULL);
	void* newIpamirSolver();
	MyMinisat::Solver* newTernSatSlv();

	/*
	 * 01X MAX SAT encoding - helpers
	 */
	void add01XAnd(MyMinisat::Solver &slv, MyMinisat::Lit lhs, MyMinisat::Lit rhs0,
			MyMinisat::Lit rhs1);
	void add01XAndPacose(Pacose &slv, MyMinisat::Lit lhs, MyMinisat::Lit rhs0,
			MyMinisat::Lit rhs1);
	void add01XAndPacoseElim(MyMinisat::SimpSolver &slv, MyMinisat::Lit lhs,
			MyMinisat::Lit rhs0, MyMinisat::Lit rhs1);
#ifdef maxsatpogen
	void add01XEquiv(antom::Antom & slv, MyMinisat::Lit lhs, MyMinisat::Lit rhs);
#endif /* maxsatpogen */
	void add01XEquivPacose(Pacose &slv, MyMinisat::Lit lhs, MyMinisat::Lit rhs);
	void add01XEquivPacoseElim(MyMinisat::SimpSolver &slv, MyMinisat::Lit lhs,
			MyMinisat::Lit rhs);
	void add01XcubeToPacoseSlv(Pacose &slv, const LitVec &c);
	void add01XCube(MyMinisat::Solver &slv, const LitVec &c);
	void addDualRailCube(MyMinisat::Solver &slv, const LitVec &c);

	void add01XUnit(MyMinisat::Solver &slv, MyMinisat::Lit l) const
	{
		slv.addClause(l);
		//auxiliary counterpart
		slv.addClause(
				MyMinisat::mkLit(antomAuxiliaryVars[MyMinisat::var(l)],
						(!MyMinisat::sign(l))));
		assert(
				((int)(2 * antomAuxiliaryVars[MyMinisat::var(l)]
						+ (!MyMinisat::sign(l)) % 2)) != (l.x % 2));
	}
	//add literal 0 or 1 to assumption vector in 01X encoding (no X!)
	void add01XUnitToMSAssumps(MSLitVec &assumps, MyMinisat::Lit l) const
	{
		assumps.push(l);
		//auxiliary counterpart
		assumps.push(
				MyMinisat::mkLit(antomAuxiliaryVars[MyMinisat::var(l)],
						(!MyMinisat::sign(l))));
		assert(
				((int)(2 * antomAuxiliaryVars[MyMinisat::var(l)]
						+ (!MyMinisat::sign(l)) % 2)) != (l.x % 2));
	}
	void add01XUnitPacose(Pacose &slv, MyMinisat::Lit l) const
	{
		std::vector<unsigned int> unit;
		unit.push_back(l.x);
		slv.AddClause(unit);
		unit.clear();
		//auxiliary counterpart
		unit.push_back(
				2 * antomAuxiliaryVars[MyMinisat::var(l)] + (!MyMinisat::sign(l)));
		slv.AddClause(unit);
		assert(
				((int)(2 * antomAuxiliaryVars[MyMinisat::var(l)]
						+ (!MyMinisat::sign(l)) % 2)) != (l.x % 2));
	}
	//add literal 0 or 1 to assumption vector in 01X encoding (no X!)
	void add01XUnitToAssumps(std::vector<uint32_t> &assumps,
			MyMinisat::Lit l) const
	{
		assumps.push_back(l.x);
		//auxiliary counterpart
		assumps.push_back(
				2 * antomAuxiliaryVars[MyMinisat::var(l)] + (!MyMinisat::sign(l)));
		assert(
				(int)(2 * antomAuxiliaryVars[MyMinisat::var(l)]
						+ (!MyMinisat::sign(l)) % 2) != (l.x % 2));
	}
	//push literal 0 or 1 to clause vector in 01X encoding (no X!)
	void add01XLitToClause(std::vector<uint32_t> &cl, MyMinisat::Lit l) const
	{
		cl.push_back(l.x);
		//auxiliary counterpart
		cl.push_back(
				2 * antomAuxiliaryVars[MyMinisat::var(l)] + (!MyMinisat::sign(l)));
		assert(
				(int)(2 * antomAuxiliaryVars[MyMinisat::var(l)]
						+ (!MyMinisat::sign(l)) % 2) != (l.x % 2));
	}
	uint32_t get01XAuxVar(MyMinisat::Var v)
	{
		assert((unsigned)v < antomAuxiliaryVars.size());
		uint32_t vret = antomAuxiliaryVars[v];
		assert(vret > reps);
		return (vret);
	}
	void addCubeToDepQBF(QDPLL &slv, const LitVec &c);
#ifdef maxsatpogen
	void addSoftClauses(antom::Antom& slv);
#endif /* maxsatpogen */
	void addSoftClausesPacose(Pacose &slv, vector<unsigned int> *dontCares =
	NULL);
	void addSoftClausesIpamir(void* solver);

	//is latch or input?
	bool isLatch(const MyMinisat::Var v) const
	{
		return ((v < endLatches()->var()) && (v >= beginLatches()->var()));
	}
	bool isInput(const MyMinisat::Var v) const
	{
		return ((v < endInputs()->var()) && (v >= beginInputs()->var()));
	}


	//IPAMIR interface functions:
	void ipamirAddClause(void * solver, const vector<MyMinisat::Lit> cls_) const;
	void ipamirAddSoftLit(void * solver, const MyMinisat::Lit l) const;
	void ipamirAssume(void * solver, const MyMinisat::Lit l) const;
	void ipamirAssume01XUnit(void * solver, const MyMinisat::Lit l) const
	{
		ipamirAssume(solver, l);
		//auxiliary counterpart
		ipamirAssume(solver, MyMinisat::mkLit(antomAuxiliaryVars[MyMinisat::var(l)], !MyMinisat::sign(l)));
		assert(
				((int)(2 * antomAuxiliaryVars[MyMinisat::var(l)]
						+ (!MyMinisat::sign(l)) % 2)) != (l.x % 2));
	}

	// Loads the TR into the solver.  Also loads the primed error
	// definition such that Model::primedError() need only be asserted
	// to activate it.  Invariant constraints (AIGER 1.9) and the
	// negation of the error are always added --- except that the primed
	// form of the invariant constraints are not asserted if
	// !primeConstraints.
	void loadTransitionRelation(MyMinisat::Solver &slv, bool primeConstraints =
			true, bool selfLoops = false, bool noConstraints = false,
			bool noError = false);
	void loadTransitionRelationImplgraph(MyMinisat::Solver &slv);
	void loadTransitionRelationImplgraphBad(MyMinisat::Solver &slv);
	void loadTransitionRelationImplgraphSucc(MyMinisat::Solver &slv, bool respectProperty);
	void loadTransitionRelationLifting(MyMinisat::Solver &slv);
	void loadTransitionRelationDepQBF(QDPLL *slv, bool primeConstraints = true);
	void loadTransitionRelationQuantom(quantom::Quantom &qSlv);
	void loadTransitionRelationILP();
#ifdef maxsatpogen
	//load the transition relation using a 01X encoding
	void loadTransitionRelation01X(antom::Antom & slv, bool primeConstraints =
			true);
#endif /* maxsatpogen */
	void loadTransitionRelation01XPacose(Pacose &slv, bool primeConstraints =
			true);
	void loadTransitionRelation01XPacoseElim(Pacose &slv,
			bool primeConstraints = true);
	void loadTransitionRelation01XIpamir(void* solver);
	void loadTransitionRelation01XElim(MyMinisat::Solver &slv);
	void loadTransitionRelationCover01XElim(MyMinisat::Solver &slv);
	// Loads the initial condition into the solver.
	void loadInitialCondition(MyMinisat::Solver &slv) const;
	// Loads the initial condition into the QBF solver.
	void loadInitialConditionDepQBF(QDPLL &slv) const;
	// Loads the initial condition using a 01X encoding
	void loadInitialCondition01X(MyMinisat::Solver &slv);
	void loadInitialCondition01XPacose(Pacose &slv);
	// Loads the error into the solver, which is only necessary for the
	// 0-step base case of IC3.
	void loadError(MyMinisat::Solver &slv, bool noConstraints = false);
	void loadErrorBasic(MyMinisat::Solver &slv) const;

	// Use this method to allow the Model to decide how best to decide
	// if a cube has an initial state.
	bool isInitial(const LitVec &latches, bool reverse);
	// ... repair if cube is generalized too much
	void ungenInitCube(LitVec &genCube, const LitVec &originalCube,
			bool trivial = false);

	//check whether invariant constraints (AIGER 1.9) are a function of the inputs
	bool checkConstraintsForInputFanin();

	LitVec getInitLits();
	size_t getMaxVar();
	int getLitOcc(const MyMinisat::Lit l);
	/*Rev PDR po generalization */
	std::vector<unsigned int>& getTransitionSupport(Var v);
	//only propagation, no correct constant checking
	std::set<unsigned int> circuitTrav(std::vector<ternBool> &valuation,
			unsigned int stateVar);
	void circuitPropagation(std::vector<ternBool> &valuation,
			std::set<unsigned int> &constants);
	//plain simulation of circuit with random patterns
	//returns bit (vector) for each next state function
	PatternFix simFixRndPatterns(LitVec &presentState); //uses std::bitset with constant bitwidth -> more efficient

	//01X-Simulation (ternary)
	/*typedef std::vector<
	 std::pair<std::bitset<ternsimBitWidth>, std::bitset<ternsimBitWidth>>> ternsimVec;*/
	typedef std::vector<std::pair<uint64_t, uint64_t>> ternsimVec;
	void ternSim(ternsimVec &valuation, MyMinisat::Var maxVar);

	//load ~T into solver
	void loadNegTrans(MyMinisat::Solver &negSlv);

	//helper for Justification
	void assignValues(const MyMinisat::Solver *modelSlv, LitVec &model, bool primeRun = false);
	void assignValues(const LitVec *assignment, LitVec &model, bool primeRun = false);
	void findJustPath(AigVec &justPath, set<MyMinisat::Var> &justCI,
			const LitVec *succ, const LitVec &model) const;
	int propagateJustPrio(AigVec &justPath, set<MyMinisat::Var> &justCI,
			const LitVec *succ, const LitVec &model,
			vector<int> &prio) const;

	//stores out degree of support nodes (inputs, latches)
	//out degree of i -> node supports i transition functions
	std::vector<unsigned int> outDegrSuppVars;
	//stores support variables ordered by their number of occurrence in transition functions
	std::vector<unsigned int> suppByNumOfOcc;

	//01X MAX SAT optimization, soft clause activation vars
	std::vector<uint32_t> softClActVars;
private:

	VarVec vars;
	const size_t inputs, latches, reps, primes;

	bool primesUnlocked;
	typedef unordered_map<size_t, size_t> IndexMap;
	IndexMap primedAnds;

	const AigVec aig;
	const LitVec init, constraints, nextStateFns;
	const MyMinisat::Lit _error;
	MyMinisat::Lit _primedError;

	typedef size_t TRMapKey;
	typedef unordered_map<TRMapKey, MyMinisat::SimpSolver*> TRMap;
	TRMap trmap;

	MyMinisat::Solver *inits;
	LitSet initLits;

	MyMinisat::SimpSolver *sslv;
	MyMinisat::SimpSolver *sslvError;
	MyMinisat::SimpSolver *sslvReduced;
	MyMinisat::SimpSolver *sslv01X;
	MyMinisat::SimpSolver *sslvImplgraph;
	MyMinisat::SimpSolver *sslvImplgraphSucc;
	MyMinisat::SimpSolver *sslvImplgraphBad;
	MyMinisat::SimpSolver *sslvLifting;
	const MyMinisat::Lit _nand;
	MyMinisat::Var gvi = 0;
	MyMinisat::SimpSolver *sslvQBF;
	MyMinisat::SimpSolver *sslvILP;

	//stores for each variable (node/latch/input) its successors in latch transition function
	std::vector<std::set<unsigned int>> circuitNodeSuccessors;
	bool circuitNodeSuccessorsActive = false;
	//stores support (input/latch variables) of transition function for each latch
	std::vector<std::vector<unsigned int>> transitionSupport;

	//01X encoding auxiliary variables
	std::vector<uint32_t> antomAuxiliaryVars;
	bool antomAuxiliaryVarsInitialized = false;
	bool softClActVarsInitialized = false;

	//number of occurence of literals in TR
	vector<int> litOccurence;

	//ipamir variable with maximum index
	unsigned ipamirMaxVar = 0;
};

// The easiest way to create a model.
Model* modelFromAiger(aiger *aig, unsigned int propertyIndex);
void introduceSelfLoops(aiger *aig);

#endif
