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

#include <algorithm>
#include <iostream>
#include <fstream>
#include <set>
#include <sys/times.h>
// for fork
#include <sys/types.h>
#include <sys/wait.h>
#include <streambuf>
#include <string>
#include <iostream>
#include <array>
#include <memory>
#include <stdexcept>
#include <string>

#include "IC3.h"
#include "Solver.h"
#include "Vec.h"
#include "popen2.h"

unsigned int filecounter = 0;

namespace IC3
{

class IC3
{
public:
	IC3(
		Model &_model,
		Model &_modelWithSelfLoops,
		string &_poGenPath,
		GeneralOpt &_genOpt,
		int _verbose,
		bool _basic,
		bool _random,
		bool _revpdr,
		bool _litrot,
		string &_poGenType) :
			model(_model), // argument
			modelWithSelfLoops(_modelWithSelfLoops), // argument
			poGenPath(_poGenPath), // argument
			poGenType(determineGenType(_poGenType)), //argument
			genOpt(_genOpt), //argument
			verbose(_verbose), // argument
			// _basic is used directly in the constructor; there is no internal field
			random(_random), // argument
			revpdr(_revpdr), // argument
			literalRotation(_litrot), // argument
			MaxSATTime(0.0), // paxiant
			MaxQBFTime(0.0), // paxiant
			k(1),
			constraintsAreFnOfInputs(false),
			poGen(NULL),
			nextState(0),
			lifts(NULL),
			litOrder(),
			numLits(0),
			numUpdates(0),
			slimLitOrder(),
			maxDepth(1),
			maxCTGs(3),
			maxJoins(1 << 20),
			micAttempts(3),
			cexState(0),
			nQuery(0),
			nCTI(0),
			nCTG(0),
			nmic(0),
			poReduction(0),
			nPoReductions(0),
			nSuccessPoRed(0),
			satTime(0),
			poRedTime(0),
			nCoreReduced(0),
			nAbortJoin(0),
			nAbortMic(0)
#ifdef pacosefixs
			, maxSatSolver(NULL)
#endif
	{
		if (_basic)
		{
			maxDepth = 0;
			maxJoins = 0;
			maxCTGs = 0;
		}

		slimLitOrder.heuristicLitOrder = &litOrder;

		//check whether the invariant constraints are a function of the inputs -> no lifting!
		if(!model.invariantConstraints().empty() && poGenType == PoGeneralizer::LIFTING)
		{
			if(genOpt.lift.selfLoops)
				constraintsAreFnOfInputs = model.checkConstraintsForInputFanin();
			else
				constraintsAreFnOfInputs = false;
		}

		//cout << (constraintsAreFnOfInputs ? "constraints are a function of the inputs" : "constraints: no problem, lifting can be done") << endl;
//__________________ TEST

		if(_genOpt.lift.selfLoops && constraintsAreFnOfInputs)
			poGen = new PoGeneralizer(poGenType, _modelWithSelfLoops, revpdr, genOpt, &slimLitOrder);
		else
			poGen = new PoGeneralizer(poGenType, model, revpdr, genOpt, &slimLitOrder);

		// construct lifting solver
		if (!revpdr)
		{

#ifdef lifting
			if(constraintsAreFnOfInputs && poGenType == PoGeneralizer::LIFTING))
			{
				lifts = modelWithSelfLoops.newSolver();
			}
			else
#endif
			{
				lifts = model.newSolver();
			}
			// don't assert primed invariant constraints
			// assert notInvConstraints (in stateOf) when lifting
#ifdef lifting
#ifdef lifting_selfloops
			if(constraintsAreFnOfInputs)
			{
				modelWithSelfLoops.loadTransitionRelation(*lifts, false, true, true);
				modelWithSelfLoops.lockPrimes();
				notInvConstraints = MyMinisat::mkLit(lifts->newVar());
				MyMinisat::vec<MyMinisat::Lit> cls;
				cls.push(~notInvConstraints);
				for (LitVec::const_iterator i =
						modelWithSelfLoops.invariantConstraints().begin();
						i != modelWithSelfLoops.invariantConstraints().end(); ++i)
					cls.push(modelWithSelfLoops.primeLit(~*i));
				lifts->addClause_(cls);
			}
			else
#endif
#endif
			{
				model.loadTransitionRelationLifting(*lifts);
				notInvConstraints = MyMinisat::mkLit(lifts->newVar());
				MyMinisat::vec<MyMinisat::Lit> cls;
				cls.push(~notInvConstraints);
				for (LitVec::const_iterator i =
						model.invariantConstraints().begin();
						i != model.invariantConstraints().end(); ++i)
					cls.push(model.primeLit(~*i));
				lifts->addClause_(cls);
#ifdef lifting_extcall
				cls.clear();
				notUnprimedInvConstraints = MyMinisat::mkLit(lifts->newVar());
				cls.push(~notUnprimedInvConstraints);
				for (LitVec::const_iterator i =
						model.invariantConstraints().begin();
						i != model.invariantConstraints().end(); ++i)
					cls.push(~*i);
				lifts->addClause_(cls);
#endif
			}


#ifdef pacosepogen
#ifdef pacoseincremental
			//pacose incremental use
			maxSatSolver = model.newPacose();
			model.loadTransitionRelation01XPacoseElim(*maxSatSolver);
			maxSatSolver->AddUpcomingClausesAsAssumptions();
#endif
#endif
		}


	}
	~IC3()
	{
		for (vector<Frame>::const_iterator i = frames.begin();
				i != frames.end(); ++i)
		{
			if (i->consecution)
				delete i->consecution;
		}
		if (!revpdr)
			delete lifts;

		if(poGen)
			delete poGen;
	}

	// The main loop.
	bool check()
	{
		startTime = time();  // stats

		//Run a single POGP instance? (Option -p)
        if(poGenPath.size() > 1)
        {
			cout << "poGenPath: " << poGenPath << endl;
			PoGeneralizer *poGen = extractPoGenInstanceFromFile(poGenPath, poGenType,
					model, revpdr, genOpt);

			//order according to Bradley's activity heuristics
			if(genOpt.sortAssumpsByIc3refActivity)
				orderCube(*poGen->getPred());

			//solve POGP
			clock_t clkBefore = clock();
			LitVec ret = poGen->stateOf();
			delete poGen;

			//stats
			int numLatches = (model.endLatches() - model.beginLatches());
			float poRedRate = ((float) numLatches - (float) ret.size())
					/ (float) numLatches;
			cout << "red rate: " << poRedRate << endl;
			cout << "time required: "
					<< (((float) (clock() - clkBefore)) / CLOCKS_PER_SEC) << endl;
			cout << "total latches: " << numLatches << endl;
			cout << "removed literals: " << (numLatches - ret.size()) << endl;
			cout << "remaining literals: ";
			for (auto &lit : ret)
				cout << lit.x << " ";
			cout << endl;
			exit(0);
        }


		while (true)
		{
			if (verbose > 1)
				cout << "Level " << k << endl;
			extend();                         // push frontier frame
			if (!strengthen())
				return false;  // strengthen to remove bad successors
			if (propagate())
				return true;     // propagate clauses; check for proof
			printStats();
			++k;                              // increment frontier
		}

	}

	// Follows and prints chain of states from cexState forward.
	void printWitness()
	{
		if (cexState != 0)
		{
			size_t curr = cexState;
			while (curr)
			{
				cout << stringOfLitVec(state(curr).inputs)
						<< stringOfLitVec(state(curr).latches) << endl;
				curr = state(curr).successor;
			}
		}
	}

private:

	Model &model;
	Model &modelWithSelfLoops;
	string poGenPath;
	PoGeneralizer::GenType poGenType;
	GeneralOpt genOpt; //options for po generalizations
	int verbose; // 0: silent, 1: stats, 2: all
	bool random;
	bool revpdr; //true: use Reverse PDR, false: standard
	bool literalRotation; // true: use literal rotation for generating final conflict clause; false: standard

	//paxiant
	double MaxSATTime;
	double MaxQBFTime;

	size_t k;
	bool constraintsAreFnOfInputs;

	PoGeneralizer *poGen;

	class ReverseVarComp
	{
	public:
		bool operator()(const MyMinisat::Var &v1, const MyMinisat::Var &v2)
		{
			if (v1 > v2)
				return true;
			if (v1 <= v2)
				return false;
			return false;
		}
	};

	string stringOfLitVec(const LitVec &vec)
	{
		stringstream ss;
		for (LitVec::const_iterator i = vec.begin(); i != vec.end(); ++i)
			ss << model.stringOfLit(*i) << " ";
		return ss.str();
	}

	// The State structures are for tracking trees of (lifted) CTIs.
	// Because States are created frequently, I want to avoid dynamic
	// memory management; instead their (de)allocation is handled via
	// a vector-based pool.
	struct State
	{
		size_t successor;  // successor State
		LitVec latches;
		LitVec inputs;
		size_t index;      // for pool
		bool used;         // for pool
	};
	vector<State> states;
	size_t nextState;
	// WARNING: do not keep reference across newState() calls
	State& state(size_t sti)
	{
		return states[sti - 1];
	}
	size_t newState()
	{
		if (nextState >= states.size())
		{
			states.resize(states.size() + 1);
			states.back().index = states.size();
			states.back().used = false;
		}
		size_t ns = nextState;
		assert(!states[ns].used);
		states[ns].used = true;
		while (nextState < states.size() && states[nextState].used)
			nextState++;
		return ns + 1;
	}
	void delState(size_t sti)
	{
		State &st = state(sti);
		st.used = false;
		st.latches.clear();
		st.inputs.clear();
		if (nextState > st.index - 1)
			nextState = st.index - 1;
	}
	void resetStates()
	{
		for (vector<State>::iterator i = states.begin(); i != states.end(); ++i)
		{
			i->used = false;
			i->latches.clear();
			i->inputs.clear();
		}
		nextState = 0;
	}

	// A CubeSet is a set of ordered (by integer value) vectors of
	// MyMinisat::Lits.
	static bool _LitVecComp(const LitVec &v1, const LitVec &v2)
	{
		if (v1.size() < v2.size())
			return true;
		if (v1.size() > v2.size())
			return false;
		for (size_t i = 0; i < v1.size(); ++i)
		{
			if (v1[i] < v2[i])
				return true;
			if (v2[i] < v1[i])
				return false;
		}
		return false;
	}
	static bool _LitVecEq(const LitVec &v1, const LitVec &v2)
	{
		if (v1.size() != v2.size())
			return false;
		for (size_t i = 0; i < v1.size(); ++i)
			if (v1[i] != v2[i])
				return false;
		return true;
	}
	class LitVecComp
	{
	public:
		bool operator()(const LitVec &v1, const LitVec &v2) const
		{
			return _LitVecComp(v1, v2);
		}
	};
	typedef set<LitVec, LitVecComp> CubeSet;

	// A proof obligation.
	struct Obligation
	{
		Obligation(size_t st, size_t l, size_t d) :
				state(st), level(l), depth(d)
		{
		}
		size_t state;  // Generalize this state...
		size_t level;  // ... relative to this level.
		size_t depth;  // Length of CTI suffix to error.
	};
	class ObligationComp
	{
	public:
		bool operator()(const Obligation &o1, const Obligation &o2)
		{
			if (o1.level < o2.level)
				return true;  // prefer lower levels (required)
			if (o1.level > o2.level)
				return false;
			if (o1.depth < o2.depth)
				return true;  // prefer shallower (heuristic)
			if (o1.depth > o2.depth)
				return false;
			if (o1.state < o2.state)
				return true;  // canonical final decider
			return false;
		}
	};
	typedef set<Obligation, ObligationComp> PriorityQueue;

	// For IC3's overall frame structure.
	struct Frame
	{
		size_t k;             // steps from initial state
		CubeSet borderCubes;  // additional cubes in this and previous frames
		MyMinisat::Solver *consecution;
	};
	vector<Frame> frames;

	MyMinisat::Solver *lifts;
	MyMinisat::Lit notInvConstraints;
	MyMinisat::Lit notUnprimedInvConstraints;
#ifdef pacosepogen
	Pacose *maxSatSolver;
#endif

	// Push a new Frame.
	void extend()
	{
		while (frames.size() < k + 2)
		{
			frames.resize(frames.size() + 1);
			Frame &fr = frames.back();
			fr.k = frames.size() - 1;
			fr.consecution = model.newSolver();

			if (random)
			{
				fr.consecution->random_seed = rand();
				fr.consecution->rnd_init_act = true;
			}
			if (fr.k == 0)
			{
				if (!revpdr)
				{
					model.loadInitialCondition(*fr.consecution);
				}
				else
					fr.consecution->addClause(model.primedError()); //reverse PDR
			}
			model.loadTransitionRelation(*fr.consecution, true, false, false, revpdr);//don't assert P in rPDR
		}
	}


#ifdef pacosepogen
	void initPacoseMaxSatSolver(Frame &fr, vector<unsigned int>* dontCares = NULL)
	{
		maxSatSolver = model.newPacose(dontCares);
		//model.loadTransitionRelation01XPacose(*maxSatSolver);
		model.loadTransitionRelation01XPacoseElim(*maxSatSolver);
		//etc.
	}
#endif

#ifdef pacosewritewcnf
	void writePacoseMaxSatProblem(Frame &fr, size_t succ = 0)
	{
		MyMinisat::SimpSolver* transSlv = model.getTransSslv01X(); //T
		assert(transSlv);//only possible if the solver is already instantiated

		std::string clauses = "";
		std::string softClauses = "";
		std::string header("p wcnf ");
		int nClauses = 0;
		int weightTop = model.softClActVars.size() + 1;//#soft clause weights + 1 (soft clauses unweighted -> weight 1)
		int weightSoftCl = 1;
		uint32_t nVars = model.softClActVars.back();//last softclause var has highest index

		//print T (after variable elimination!)
		for (MyMinisat::ClauseIterator c = transSlv->clausesBegin();
				c != transSlv->clausesEnd(); ++c)
		{
			const MyMinisat::Clause &cls = *c;

			clauses.append(std::to_string(weightTop));
			clauses.append(" ");
			for (int i = 0; i < cls.size(); ++i)
			{
				assert(cls[i].x > 1);
				clauses.append(std::to_string(MyMinisat::var(cls[i]) * ((int) std::pow(-1, MyMinisat::sign(cls[i])))));
				clauses.append(" ");
			}
			clauses.append("0\n");
			nClauses++;
		}
		for (MyMinisat::TrailIterator c = transSlv->trailBegin(); c != transSlv->trailEnd();
				++c)
		{
			if ((*c).x > 1)
			{
				clauses.append(std::to_string(weightTop));
				clauses.append(" ");
				clauses.append(std::to_string(MyMinisat::var(*c) * ((int) std::pow(-1, MyMinisat::sign(*c)))));
				clauses.append(" 0\n");
				nClauses++;
			}
		} //printed T

		for (VarVec::const_iterator i = model.beginLatches(); i != model.endLatches(); ++i)
		{
			uint32_t softVar = model.softClActVars.at(i->var() - model.beginLatches()->var());

			//print soft clauses implications (hard clauses!)
			clauses.append(std::to_string(weightTop));
			clauses.append(" ");
			clauses.append(std::to_string(-((int)softVar)));//~t
			clauses.append(" ");
			clauses.append(std::to_string(-(i->var())));//~s_0
			clauses.append(" 0\n");
			nClauses++;
			clauses.append(std::to_string(weightTop));
			clauses.append(" ");
			clauses.append(std::to_string(-((int)softVar)));//~t
			clauses.append(" ");
			clauses.append(std::to_string(-((int)model.get01XAuxVar(i->var()))));//~s_1
			clauses.append(" 0\n");
			nClauses++;

			//print soft clauses
			softClauses.append(std::to_string(weightSoftCl));
			softClauses.append(" ");
			softClauses.append(std::to_string(softVar));
			softClauses.append(" 0\n");
			nClauses++;
		}

#ifdef pacosefixs
		//if fix s, add clauses which assert don't care or original value
		for (VarVec::const_iterator i = model.beginLatches();
				i != model.endLatches(); ++i)
		{
			// assert (assignment or don't care)
			MyMinisat::lbool val = fr.consecution->modelValue(i->var());
			if (val != MyMinisat::l_Undef)
			{
				MyMinisat::Lit la = i->lit(val == MyMinisat::l_False);
				if (MyMinisat::sign(la)) // s_0 = 0
				{
					clauses.append(std::to_string(weightTop));
					clauses.append(" ");
					clauses.append(std::to_string(-MyMinisat::var(la))); //~s_0
					clauses.append(" 0\n");
					nClauses++;
					clauses.append(std::to_string(weightTop));
					clauses.append(" ");
					clauses.append(std::to_string(-MyMinisat::var(la)));//~s_0
					clauses.append(" ");
					clauses.append(std::to_string(
									-((int)model.get01XAuxVar(MyMinisat::var(la)))));//~s_1
					clauses.append(" 0\n");
					nClauses++;
					clauses.append(std::to_string(weightTop));
					clauses.append(" ");
					clauses.append(std::to_string(-MyMinisat::var(la)));//~s_0
					clauses.append(" ");
					clauses.append(std::to_string(
									model.get01XAuxVar(MyMinisat::var(la))));//s_1
					clauses.append(" 0\n");
					nClauses++;
				}
				else // s_0 = 1
				{
					clauses.append(std::to_string(weightTop));
					clauses.append(" ");
					clauses.append(std::to_string(
									-((int)model.get01XAuxVar(MyMinisat::var(la))))); //~s_1
					clauses.append(" 0\n");
					nClauses++;
					clauses.append(std::to_string(weightTop));
					clauses.append(" ");
					clauses.append(std::to_string(MyMinisat::var(la)));//s_0
					clauses.append(" ");
					clauses.append(std::to_string(
									-((int)model.get01XAuxVar(MyMinisat::var(la)))));//~s_1
					clauses.append(" 0\n");
					nClauses++;
					clauses.append(std::to_string(weightTop));
					clauses.append(" ");
					clauses.append(std::to_string(-MyMinisat::var(la)));//~s_0
					clauses.append(" ");
					clauses.append(std::to_string(
									-((int)model.get01XAuxVar(MyMinisat::var(la)))));//~s_1
					clauses.append(" 0\n");
					nClauses++;
				}
			}
		}
#endif

		if (succ == 0)
		{
			clauses.append(std::to_string(weightTop));
			clauses.append(" ");
			clauses.append(std::to_string(MyMinisat::var(model.primedError()) * ((int) std::pow(-1, MyMinisat::sign(model.primedError())))));
			clauses.append(" 0\n");
			nClauses++;
			clauses.append(std::to_string(weightTop));
			clauses.append(" ");
			clauses.append(std::to_string((int)model.get01XAuxVar(MyMinisat::var(model.primedError())) * ((int) std::pow(-1, !MyMinisat::sign(model.primedError())))));
			clauses.append(" 0\n");
			nClauses++;
		}
		else
		{
			for (LitVec::const_iterator i = state(succ).latches.begin();
					i != state(succ).latches.end(); ++i)
			{
				clauses.append(std::to_string(weightTop));
				clauses.append(" ");
				clauses.append(std::to_string(MyMinisat::var(model.primeLit(*i)) * ((int) std::pow(-1, MyMinisat::sign(model.primeLit(*i))))));
				clauses.append(" 0\n");
				nClauses++;
				clauses.append(std::to_string(weightTop));
				clauses.append(" ");
				clauses.append(std::to_string((int)model.get01XAuxVar(MyMinisat::var(model.primeLit(*i))) * ((int) std::pow(-1, !MyMinisat::sign(model.primeLit(*i))))));
				clauses.append(" 0\n");
				nClauses++;
			}
		}

		//header line
		header.append(std::to_string(nVars));
		header.append(" ");
		header.append(std::to_string(nClauses));
		header.append(" ");
		header.append(std::to_string(weightTop));
		header.append("\n");

		//end of file
		std::string wcnf(header);
		wcnf.append(clauses);
		wcnf.append(softClauses);

		//output
		std::ofstream wcnffile;
		std::string pathstr = "/home/seufert/git/lifting-in-pdr/IC3ref-master/maxsatproblem.wcnf";
		const char *path = pathstr.c_str();
		wcnffile.open(path);
		wcnffile << wcnf;
		wcnf.clear();
		wcnffile.close();
	}
#endif


	HeuristicLitOrder litOrder;
	float numLits, numUpdates;
	void updateLitOrder(const LitVec &cube, size_t level)
	{
		litOrder.decay();
		numUpdates += 1;
		numLits += cube.size();
		litOrder.count(cube);
	}
	SlimLitOrder slimLitOrder;
	// order according to preference
	void orderCube(LitVec &cube)
	{
		stable_sort(cube.begin(), cube.end(), slimLitOrder);
	}

	// Orders assumptions for Minisat.
	void orderAssumps(MSLitVec &cube, bool rev, int start = 0)
	{
		stable_sort(cube + start, cube + cube.size(), slimLitOrder);
		if (rev)
			reverse(cube + start, cube + cube.size());
	}

	// Assumes that last call to fr.consecution->solve() was
	// satisfiable.  Extracts state(s) cube from satisfying
	// assignment.
	size_t stateOfPOGP(Frame &fr, size_t succ = 0,
			vector<unsigned int> *dontCares = NULL)
	{
		size_t st = newState();
		state(st).successor = succ;
		if(succ != 0)
		{
			state(st).latches = poGen->stateOf(fr.consecution, &(state(succ).latches));
		}
		else
		{
			state(st).latches = poGen->stateOf(fr.consecution, NULL);
		}
		return st;
	}

	// Assumes that last call to fr.consecution->solve() was
	// satisfiable.  Extracts state(s) cube from satisfying
	// assignment.
	size_t stateOf(Model &m, Frame &fr, size_t succ = 0, bool ctg = false,
			vector<unsigned int> *dontCares =
			NULL)
	{
		// create state
		size_t st = newState();
		state(st).successor = succ;
		MSLitVec assumps;
		assumps.capacity(
				1 + 2 * (m.endInputs() - m.beginInputs())
						+ (m.endLatches() - m.beginLatches()));
		MyMinisat::Lit act = MyMinisat::mkLit(lifts->newVar()); // activation literal
		assumps.push(act);
		MyMinisat::vec<MyMinisat::Lit> cls;
		cls.push(~act);
		if (succ == 0)
		{
			cls.push(~m.primedError());
			cls.push(notInvConstraints); // successor must satisfy inv. constraint
#ifdef lifting_extcall
			if(!ctg)
				cls.push(notUnprimedInvConstraints);  // predecessor must satisfy inv. constraint
#endif
		}
		else
		{
			for (LitVec::const_iterator i = state(succ).latches.begin();
					i != state(succ).latches.end(); ++i)
				cls.push(m.primeLit(~*i));
#ifdef lifting_extcall
			if(!ctg)
				cls.push(notUnprimedInvConstraints);  // predecessor must satisfy inv. constraint
#endif
		}
		lifts->addClause_(cls);

		// extract and assert primary inputs
		for (VarVec::const_iterator i = m.beginInputs(); i != m.endInputs();
				++i)
		{
			MyMinisat::lbool val = fr.consecution->modelValue(i->var());
			if (val != MyMinisat::l_Undef)
			{
				MyMinisat::Lit pi = i->lit(val == MyMinisat::l_False);
				state(st).inputs.push_back(pi);  // record full inputs
				assumps.push(pi);
			}
		}
		// some properties include inputs, so assert primed inputs after
		for (VarVec::const_iterator i = m.beginInputs(); i != m.endInputs();
				++i)
		{
			MyMinisat::lbool pval = fr.consecution->modelValue(
					model.primeVar(*i).var()); //always original model primes
			if (pval != MyMinisat::l_Undef)
				assumps.push(m.primeLit(i->lit(pval == MyMinisat::l_False)));
		}

		int sz = assumps.size();
		// extract and assert latches
		LitVec latches;
		for (VarVec::const_iterator i = m.beginLatches(); i != m.endLatches();
				++i)
		{
			MyMinisat::lbool val = fr.consecution->modelValue(i->var());
			if (val != MyMinisat::l_Undef)
			{
				MyMinisat::Lit la = i->lit(val == MyMinisat::l_False);
				latches.push_back(la);
				assumps.push(la);
			}
		}

#ifdef nolifting
		if(!ctg)
		{
			state(st).latches.swap(latches);
			return st;
		}
#endif

		//standard behaviour only for ctg
		//if(ctg)
		orderAssumps(assumps, false, sz); // empirically found to be best choice
		// State s, inputs i, transition relation T, successor t:
		//   s & i & T & ~t' is unsat
		// Core assumptions reveal a lifting of s.
		++nQuery;
		startTimer();  // stats
		bool rv = lifts->solve(assumps);
		endTimer(satTime);
		assert(!rv);
		// obtain lifted latch set from unsat core
		for (LitVec::const_iterator i = latches.begin(); i != latches.end();
				++i)
		{
			if (lifts->conflict.has(~*i))
				state(st).latches.push_back(*i);  // record lifted latches
		}

		// deactivate negation of successor
		lifts->releaseVar(~act);
		//std::cout << "lifting gen from " << oldsz << " to "
		//		<< state(st).latches.size() << std::endl;
		return st;
	}

	// Assumes that last call to fr.consecution->solve() was
	// satisfiable.  Extracts state(s) cube via an additional
	// MAX SAT call.
#ifdef pacosepogen
	vector<long long int> oValues;
	size_t stateOfMaxSatPacose(Frame &fr, size_t succ = 0, vector<unsigned int>* dontCares = NULL)
	{
		//std::cout << "max sat call" << std::endl;
#ifndef pacoseincremental
		initPacoseMaxSatSolver(fr, dontCares);
#endif
		assert(maxSatSolver);

#ifdef pacosewritewcnf
		writePacoseMaxSatProblem(fr, succ);
		exit(0);
#endif

		LitVec latches;
#ifdef pacosefixs
		std::vector<uint32_t> clStatebitToGen;

		for (VarVec::const_iterator i = model.beginLatches();
				i != model.endLatches(); ++i)
		{
			// assert (assignment or don't care)
			MyMinisat::lbool val = fr.consecution->modelValue(i->var());
			if (val != MyMinisat::l_Undef)
			{
#ifdef pacoseheuristics
				if(dontCares && std::binary_search(dontCares->begin(), dontCares->end(), i->var()))
				{ //variable has been found as don't care by lifting -> assume as don't care
					//in MaxSAT in order to find a better solution with less effort
					//-> add X as unit clause (0,0)
					std::vector<unsigned int> unit;
					unit.push_back((i->var() << 1) + 1);
					//slv._satSolver->AddClause(unit);
					maxSatSolver->AddClause(unit);
					unit.clear();
					//auxiliary counterpart
					unit.push_back((model.get01XAuxVar(i->var()) << 1) + 1);
					maxSatSolver->AddClause(unit);
				}
				else
#endif
				{
					MyMinisat::Lit la = i->lit(val == MyMinisat::l_False);
#ifdef pacoseheuristics
					latches.push_back(la);
#endif
					if (MyMinisat::sign(la)) // s_0 = 0
					{
						clStatebitToGen.push_back(la.x); //~s_0
						maxSatSolver->AddClause(clStatebitToGen);
						clStatebitToGen.clear();
						clStatebitToGen.push_back(la.x);//~s_0
						clStatebitToGen.push_back(
								(model.get01XAuxVar(MyMinisat::var(la)) << 1) + 1);//~s_1
						maxSatSolver->AddClause(clStatebitToGen);
						clStatebitToGen.clear();
						clStatebitToGen.push_back(la.x);//~s_0
						clStatebitToGen.push_back(
								(model.get01XAuxVar(MyMinisat::var(la)) << 1));//s_1
						maxSatSolver->AddClause(clStatebitToGen);
						clStatebitToGen.clear();
					}
					else // s_0 = 1
					{
						clStatebitToGen.push_back(
								(model.get01XAuxVar(MyMinisat::var(la)) << 1) + 1); //~s_1
						maxSatSolver->AddClause(clStatebitToGen);
						clStatebitToGen.clear();
						clStatebitToGen.push_back(la.x);//s_0
						clStatebitToGen.push_back(
								(model.get01XAuxVar(MyMinisat::var(la)) << 1) + 1);//~s_1
						maxSatSolver->AddClause(clStatebitToGen);
						clStatebitToGen.clear();
						clStatebitToGen.push_back((~la).x);//~s_0
						clStatebitToGen.push_back(
								(model.get01XAuxVar(MyMinisat::var(la)) << 1) + 1);//~s_1
						maxSatSolver->AddClause(clStatebitToGen);
						clStatebitToGen.clear();
					}
					// if (MyMinisat::sign(la))
					// {
					// 	clStatebitToGen.push_back(la.x); //v^t = 0
					// 	maxSatSolver->AddClause(clStatebitToGen);
					// 	clStatebitToGen.clear();
					// }
					// else
					// {
					// 	clStatebitToGen.push_back(
					// 			MyMinisat::mkLit(model.get01XAuxVar(MyMinisat::var(la)), true).x); //v^f = 0
					// 	maxSatSolver->AddClause(clStatebitToGen);
					// 	clStatebitToGen.clear();
					// }
				}
			}
		}
#endif /*pacosefixs*/

		Pacose *slv = maxSatSolver;

		// create state, PACOSE: be careful with assumptions ...
		size_t st = newState();

		//assert inputs:
#ifdef pacosefixinputs
		// extract and assert primary inputs
		for (VarVec::const_iterator i = model.beginInputs();
				i != model.endInputs(); ++i)
		{
			MyMinisat::lbool val = fr.consecution->modelValue(i->var());
			if (val != MyMinisat::l_Undef)
			{
				MyMinisat::Lit pi = i->lit(val == MyMinisat::l_False);
				state(st).inputs.push_back(pi);  // record full inputs
				model.add01XUnitPacose(*slv, pi);
			}
		}
#endif

		state(st).successor = succ;
		if (succ == 0)
		{
			model.add01XUnitPacose(*slv, model.primedError());
		}
		else
		{
			for (LitVec::const_iterator i = state(succ).latches.begin();
					i != state(succ).latches.end(); ++i)
			{
				model.add01XUnitPacose(*slv, model.primeLit(*i));
			}
		}

		// s & T & succ -> max sat yields optimal lifting
		//suppress cout of Pacose
		// std::cout.setstate(std::ios_base::failbit);
		slv->_settings.verbosity = 0;
		// slv->DumpWCNF();
		uint32_t rv = slv->SolveProcedure();
		// std::cout.clear();

		//read o values. Statistical purposes
		oValues.push_back(slv->GetOValue());

#ifdef pacoseincremental
		slv->DeactivateLastCNF();
#endif

#ifndef pacoseheuristics
		assert(rv==10);
#else
		//when using the heuristics, a UNSAT result only means,
		//that we can only achieve a worse result than by using
		//the (for instance) standard lifting procedure
		if(rv != 10)
		{
			//just output the complete latches
			state(st).latches.swap(latches);

			delete maxSatSolver;
			maxSatSolver = NULL;
			return st;
		}
#endif
		unsigned int idx = 0;

		for (VarVec::const_iterator i = model.beginLatches();
				i != model.endLatches(); ++i)
		{
			uint32_t softClLit = slv->GetModel(model.softClActVars.at(idx));
			if ((softClLit % 2) != 0) //use LSB?
			{ //soft clause literal is false -> cannot lift latch literal
				uint32_t latchModelVal = slv->GetModel(i->var());
				assert(
						(latchModelVal % 2)
						!= (slv->GetModel(
										model.get01XAuxVar(i->var())) % 2));//no don't care assignment
				MyMinisat::Lit latchLit = MyMinisat::mkLit(i->var(),
						(latchModelVal % 2));
				state(st).latches.push_back(latchLit);// record lifted latches
			}
			else
			{
				uint32_t latchModelVal = slv->GetModel(i->var());
				assert((latchModelVal % 2) == 1);
				assert(
						(latchModelVal % 2)
						== (slv->GetModel(
										model.get01XAuxVar(i->var())) % 2)); //don't care assignment
			}
			idx++;
		}
		/*std::cout << "max sat reduction from size "
		 << (model.endLatches() - model.beginLatches()) << " to "
		 << state(st).latches.size() << std::endl;*/
#ifndef pacoseincremental
		delete maxSatSolver;
		//std::cout << "delete done" << std::endl;
		//TODO: there is a lot more to tidy up
		maxSatSolver = NULL;
#endif

		return st;
	}
#endif


#ifdef maxqbfaigsolvepogen
	// Writes a maxQBF problem to file.
	// Solving this problem yields an optimal lifting for
	// Reverse PDR (multiplexer trick)
	void writeMaxQBFAigsolveProblem(Frame &fr, size_t predOrSucc,
			vector<int> &fVec, LitVec *latches = NULL, std::string *pipeStr =
					NULL)
	{
		//std::cout << __PRETTY_FUNCTION__ << std::endl;
		std::string matrix("c --- START consecution CNF: R_fr * T * s\n");
		unsigned int clcounter = 0;

		int weightHardClauses = model.endLatches() - model.beginLatches() + 1;
		std::string prefix(std::to_string(weightHardClauses).append(" "));
		std::string prefixNoSpace(std::to_string(weightHardClauses));
		std::string prefixSoftCl("1 ");
		MyMinisat::SimpSolver *transSlv = model.getTransSslv();
		assert(transSlv);

		/* load transition relation (from simplified context) to solver */
		for (MyMinisat::ClauseIterator c = transSlv->clausesBegin();
				c != transSlv->clausesEnd(); ++c)
		{
			const MyMinisat::Clause &cls = *c;
			assert(cls.size() > 0);
			matrix.append(prefix);
			for (int i = 0; i < cls.size(); ++i)
			{
				assert(cls[i].x > 0);

				assert(MyMinisat::var(cls[i]) > 0);
				matrix.append(
						std::to_string(
								MyMinisat::var(cls[i])
										* ((int) std::pow(-1,
												MyMinisat::sign(cls[i])))));
				matrix.append(" ");
			}
			clcounter++;
			matrix.append("0\n");
		}

		for (MyMinisat::TrailIterator c = transSlv->trailBegin();
				c != transSlv->trailEnd(); ++c)
		{
			if ((*c).x <= 1)
				continue;
			matrix.append(prefix);

			matrix.append(
					std::to_string(
							MyMinisat::var(*c)
									* ((int) std::pow(-1, MyMinisat::sign(*c)))));
			matrix.append(" ");
			clcounter++;
			matrix.append("0\n");
		}

		//with a fix a, we only require T

		//predecessor
		LitSet predLatches;
		if (predOrSucc == 0)	//get bad cube...
		{
			if (revpdr)
			{
				LitVec init = model.getInitLits();
				for (auto &lit : init)
				{
					matrix.append(prefix);
					matrix.append(
							std::to_string(
									MyMinisat::var(lit)
											* ((int) std::pow(-1,
													MyMinisat::sign(lit)))));
					matrix.append(" ");
					matrix.append("0\n");
					clcounter++;
					predLatches.insert(lit);
				}
			}
			else
			{
				matrix.append(prefix);
				matrix.append(
						std::to_string(
								MyMinisat::var(model.primedError())
										* ((int) std::pow(-1,
												MyMinisat::sign(
														model.primedError())))));
				matrix.append(" ");
				matrix.append("0\n");
				clcounter++;
				predLatches.insert(model.primedError());
			}
		}
		else //consecution
		{
			if (revpdr)
			{
				for (auto &lit : state(predOrSucc).latches)
				{
					matrix.append(prefix);
					matrix.append(
							std::to_string(
									MyMinisat::var(lit)
											* ((int) std::pow(-1,
													MyMinisat::sign(lit)))));
					matrix.append(" ");
					matrix.append("0\n");
					clcounter++;
					predLatches.insert(lit);
				}
			}
			else
			{
				for (auto &lit : state(predOrSucc).latches)
				{
					matrix.append(prefix);
					matrix.append(
							std::to_string(
									MyMinisat::var(model.primeLit(lit))
											* ((int) std::pow(-1,
													MyMinisat::sign(
															model.primeLit(
																	lit))))));
					matrix.append(" ");
					matrix.append("0\n");
					clcounter++;
					predLatches.insert(lit);
				}
			}
		}

		//variables which aren't quantified are implicitly existentially quantified
		//in the outermost quantifier level (*QDIMACS standard)

		int currVarIdx = transSlv->nVars() + 1;
		//introduce multiplexer vars (f_1 ... f_n) and forall
		//quantified variables (r_1 ... r_n) as well as clauses for
		// f_1 -> (r_1 <-> s'_1) i.e. if f_1 is added as soft clause,
		//maximizing the number of satisfied f_i maximizes the number
		//of forall quantified next-state variables.
		std::vector<int> rVec;
		//std::vector<int> fVec;
		std::vector<int> sVec;
		for (VarVec::const_iterator i = model.beginLatches();
				i != model.endLatches(); ++i)
		{
			int l_i;

			assert(model.primeVar(*i).var() > 1);
			int f_i = currVarIdx;
			fVec.push_back(f_i);
			int s_i = currVarIdx + 1;
			sVec.push_back(s_i);
			int r_i = currVarIdx + 2;
			rVec.push_back(r_i);

			if (revpdr)
			{
				l_i = model.primeVar(*i).var();
				//l_i = model.primeVar(*(model.beginLatches() + i)).var();
			}
			else
			{
				l_i = (*i).var();
				//l_i = (model.beginLatches() + i)->var();
			}

			//(~f_i + ~l_i + r_i) * (~f_i + l_i + ~r_i)
			matrix.append(prefix);
			matrix.append(std::to_string(-f_i));
			matrix.append(" ");
			matrix.append(std::to_string(-l_i));
			matrix.append(" ");
			matrix.append(std::to_string(r_i));
			matrix.append(" ");
			clcounter++;
			matrix.append("0\n");

			matrix.append(prefix);
			matrix.append(std::to_string(-f_i));
			matrix.append(" ");
			matrix.append(std::to_string(l_i));
			matrix.append(" ");
			matrix.append(std::to_string(-r_i));
			matrix.append(" ");
			clcounter++;
			matrix.append("0\n");

			/* (f_i + ~l_i + s_i) * (f_i + l_i + ~s_i) */
			matrix.append(prefix);
			matrix.append(std::to_string(f_i));
			matrix.append(" ");
			matrix.append(std::to_string(-l_i));
			matrix.append(" ");
			matrix.append(std::to_string(s_i));
			matrix.append(" ");
			clcounter++;
			matrix.append("0\n");

			matrix.append(prefix);
			matrix.append(std::to_string(f_i));
			matrix.append(" ");
			matrix.append(std::to_string(l_i));
			matrix.append(" ");
			matrix.append(std::to_string(-s_i));
			matrix.append(" ");
			clcounter++;
			matrix.append("0\n");

			currVarIdx += 3;
		}
		assert(fVec.size() == rVec.size());
		assert(fVec.size() == sVec.size());

		//fix s
		if (latches != NULL)
		{
			assert(latches->size() == sVec.size());
			for (size_t i = 0; i < sVec.size(); ++i)
			{
				matrix.append(prefix);
				matrix.append(
						std::to_string(
								sVec[i]
										* ((int) pow(-1,
												MyMinisat::sign((*latches)[i])))));
				matrix.append(" 0\n");
				clcounter++;
			}
		}

		//Quantifier
		//Forall
		std::string forallSection("");
		forallSection.append("a ");
		for (auto &r_i : rVec)
		{
			forallSection.append(std::to_string(r_i));
			forallSection.append(" ");
		}
		forallSection.append("0\n");
		//Exist (below forall level)
		//contains: Tseitin variables, unassigned state variables, TODO
		//and all soft variables
		std::string outerExistSection("");
		std::string innerExistSection("");
		outerExistSection.append("e ");
		innerExistSection.append("e ");
		for (auto &f_i : fVec)
		{
			outerExistSection.append(std::to_string(f_i));
			outerExistSection.append(" ");
		}
		for (auto &s_i : sVec)
		{
			outerExistSection.append(std::to_string(s_i));
			outerExistSection.append(" ");
		}
		for (MyMinisat::Var v = 1; v <= transSlv->nVars(); ++v)
		{
			//existentially quantify all variables except for present state s
			/*if (predLatches.find(MyMinisat::mkLit(v, false)) == predLatches.end()
			 && predLatches.find(MyMinisat::mkLit(v, true))
			 == predLatches.end())*/
			{
				innerExistSection.append(std::to_string(v));
				innerExistSection.append(" ");
			}
		}
		innerExistSection.append("0\n");
		outerExistSection.append("0\n");

		//soft clauses: Just the f_i
		std::string softClauseSection(
				"c --- START Optimization constraints as soft clauses\n");
		for (auto &f_i : fVec)
		{
			softClauseSection.append(prefixSoftCl);
			softClauseSection.append(std::to_string(f_i));
			softClauseSection.append(" 0\n");
			clcounter++;
		}

		//header line
		std::string header("p cnf ");
		unsigned int maxIdx = rVec.back();
		header.append(std::to_string(maxIdx));
		header.append(" ");
		header.append(std::to_string(clcounter));
		header.append(" ");
		header.append(prefixNoSpace);
		header.append("\n");

		//end of file
		std::string qcnf(header);
		qcnf.append(outerExistSection);
		qcnf.append(forallSection);
		qcnf.append(innerExistSection);
		qcnf.append(matrix);
		qcnf.append(softClauseSection);

		//output
		if (!pipeStr)
		{
			std::ofstream qcnffile;
			std::string pathstr = "./maxqbfproblem";
			pathstr.append(std::to_string(filecounter));
			pathstr.append(".qcnf");
			filecounter++;
			const char *path = pathstr.c_str();
			qcnffile.open(path);
			qcnffile << qcnf;
			qcnf.clear();
			qcnffile.close();
		}
		else
		{
			*pipeStr = "";
			pipeStr->append(qcnf);
		}
	}

	// Writes a maxQBF problem into a quantom instance.
	// Solving this problem yields an optimal lifting for
	// Reverse PDR (multiplexer trick)
	// softClVec will eventually contain the variables from all unit soft clauses
	void loadMaxQBFAigsolveProblem(InterfaceCommandLine &aigslv, Frame &fr,
			size_t predOrSucc, std::vector<int> &auxExists,
			std::vector<int> &softClVec)
	{
		assert(softClVec.empty());
		assert(auxExists.empty());

		MyMinisat::SimpSolver *transSlv = model.getTransSslv();
		assert(transSlv);

		int existsQuantifiedLvl1_size = (2
				* (distance(model.beginLatches(), model.endLatches())));
		vector<int> existsQuantifiedLvl1(existsQuantifiedLvl1_size);
		int existsQuantifiedLvl1_idx = 0;
		int forallQuantifiedLvl2_size = (distance(model.beginLatches(),
				model.endLatches()));
		vector<int> forallQuantifiedLvl2(forallQuantifiedLvl2_size);
		int forallQuantifiedLvl2_idx = 0;
		int existsQuantifiedLvl3_size = transSlv->nVars();
		vector<int> existsQuantifiedLvl3(existsQuantifiedLvl3_size);
		int existsQuantifiedLvl3_idx = 0;
		int varCounter = 1;
		vector<int> auxForall;

		//variables which aren't quantified are implicitly existentially quantified
		//in the outermost quantifier level (*QDIMACS standard)

		//introduce multiplexer vars (f_1 ... f_n) and forall
		//quantified variables (r_1 ... r_n) as well as clauses for
		// f_1 -> (r_1 <-> s'_1) i.e. if f_1 is added as soft clause,
		//maximizing the number of satisfied f_i maximizes the number
		//of forall quantified next-state variables.

		/*
		 * load all variables variables of the problem instance to maintain alignment
		 * to nesting level 3
		 */
		//std::cout << "transSlv->nVars(): " << transSlv->nVars() << std::endl;
		for (int i = 0; i < transSlv->nVars(); ++i)
		{
			assert(varCounter == i + 1);
			assert(existsQuantifiedLvl3_idx < existsQuantifiedLvl3_size);
			existsQuantifiedLvl3[existsQuantifiedLvl3_idx] = varCounter;
			existsQuantifiedLvl3_idx++;
			varCounter++;
		}

		/* load variables for multiplexer structure */
		for (VarVec::const_iterator i = model.beginLatches();
				i != model.endLatches(); ++i)
		{
			assert(model.primeVar(*i).var() > 1);
			int f_i = varCounter; //existentially quantified (level 1)
			//std::cout << "about to push " << f_i << " as softclvar" << std::endl;
			softClVec.push_back(f_i);
			assert(existsQuantifiedLvl1_idx < existsQuantifiedLvl1_size);
			existsQuantifiedLvl1[existsQuantifiedLvl1_idx] = f_i;
			existsQuantifiedLvl1_idx++;
			varCounter++;

			int s_i = varCounter; //existentially quantified (level 1)
			auxExists.push_back(s_i);
			assert(existsQuantifiedLvl1_idx < existsQuantifiedLvl1_size);
			existsQuantifiedLvl1[existsQuantifiedLvl1_idx] = s_i;
			existsQuantifiedLvl1_idx++;
			varCounter++;

			int r_i = varCounter; //universally quantified (level 2)
			auxForall.push_back(r_i);
			assert(forallQuantifiedLvl2_idx < forallQuantifiedLvl2_size);
			forallQuantifiedLvl2[forallQuantifiedLvl2_idx] = r_i;
			forallQuantifiedLvl2_idx++;
			varCounter++;
		}

		/* add variables to aigsolve */
		aigslv.CreateNewVariables(varCounter - 1);
		aigslv.SetNumberOfSoftClauses(softClVec.size());

		/* add quantifier blocks to aigsolve */
		aigslv.AddQuantifierBlock(existsQuantifiedLvl1, 0); //exist
		aigslv.AddQuantifierBlock(forallQuantifiedLvl2, 1); //forall
		aigslv.AddQuantifierBlock(existsQuantifiedLvl3, 0); //exist

		assert(softClVec.size() == auxExists.size());
		assert(softClVec.size() == auxForall.size());

		/* load multiplexer clauses */
		for (size_t i = 0; i < softClVec.size(); i++)
		{
			int f_i = 2 * softClVec[i];
			//std::cout << "about to use " << f_i << " as softclvar" << std::endl;
			int r_i = 2 * auxForall[i];
			int s_i = 2 * auxExists[i];
			int l_i;
			vector<int> cls(3);

			if (revpdr)
			{
				l_i = 2 * model.primeVar(*(model.beginLatches() + i)).var();
			}
			else
			{
				l_i = 2 * (model.beginLatches() + i)->var();
			}

			/* (~f_i + ~l_i + r_i) * (~f_i + l_i + ~r_i) */
			cls[0] = f_i + 1; //~f_i
			cls[1] = l_i + 1; //~l_i
			cls[2] = r_i; //r_i
			aigslv.AddClause(cls);

			cls[0] = f_i + 1; //~f_i
			cls[1] = l_i; //l_i
			cls[2] = r_i + 1; //~r_i
			aigslv.AddClause(cls);

			/* (f_i + ~l_i + s_i) * (f_i + l_i + ~s_i) */
			cls[0] = f_i; //f_i
			cls[1] = l_i + 1; //~l_i
			cls[2] = s_i; //s_i
			aigslv.AddClause(cls);

			cls[0] = f_i; //f_i
			cls[1] = l_i; //l_i
			cls[2] = s_i + 1; //~s_i
			aigslv.AddClause(cls);
		}

		/* load transition relation (from simplified context) to solver */
		for (MyMinisat::ClauseIterator c = transSlv->clausesBegin();
				c != transSlv->clausesEnd(); ++c)
		{
			const MyMinisat::Clause &cls = *c;
			vector<int> clsAig(cls.size());
			for (int i = 0; i < cls.size(); ++i)
			{
				assert(cls[i].x > 1);
				clsAig[i] = MyMinisat::toInt(cls[i]);
			}
			aigslv.AddClause(clsAig);
		}

		for (MyMinisat::TrailIterator c = transSlv->trailBegin();
				c != transSlv->trailEnd(); ++c)
		{
			if ((*c).x > 1)
			{
				vector<int> cls(1);
				cls[0] = MyMinisat::toInt(*c);
				aigslv.AddClause(cls);
			}
		}

		assert(model.invariantConstraints().empty());

// with a fix a, we only require T (odd even encoding)
#ifndef maxqbfaigsolvefixs
		//add learnt clauses
		if (fr.k > 0)
		{
			//add blocked cubes / learnt clauses to maxQBF Solver
			for (size_t i = fr.k; i < frames.size(); ++i)
			{
				Frame &fr = frames[i];
				for (CubeSet::iterator j = fr.borderCubes.begin();
						j != fr.borderCubes.end(); ++j)
				{
					std::vector<int> cls;
					for (auto &lit : *j)
					{
						if (revpdr)
						cls.push_back((~model.primeLit(lit)).x);
						else
						cls.push_back((~lit).x);
					}
					aigslv.AddClause(cls);
				}
			}
		}
		else
		{
			//special case frame 0
			if (revpdr)
			{
				std::vector<int> cls;
				cls.push_back(model.primedError().x);
				aigslv.AddClause(cls);
			}
			else
			{
				LitVec init = model.getInitLits();
				for (auto &lit : init)
				{
					std::vector<int> cls;
					cls.push_back(lit.x);
					aigslv.AddClause(cls);
				}
			}
		}
#endif

		//predecessor
		if (predOrSucc == 0) //get bad cube...
		{
			if (revpdr)
			{
				LitVec init = model.getInitLits();
				for (auto &lit : init)
				{
					vector<int> cls(1);
					cls[0] = MyMinisat::toInt(lit);
					aigslv.AddClause(cls);
				}
			}
			else
			{
				vector<int> cls(1);
				cls[0] = MyMinisat::toInt(model.primedError());
				aigslv.AddClause(cls);
			}
		}
		else //consecution
		{
			if (revpdr)
			{
				for (auto &lit : state(predOrSucc).latches)
				{
					vector<int> cls(1);
					cls[0] = MyMinisat::toInt(lit);
					aigslv.AddClause(cls);
				}
			}
			else
			{
				for (auto &lit : state(predOrSucc).latches)
				{
					vector<int> cls(1);
					cls[0] = MyMinisat::toInt(model.primeLit(lit));
					aigslv.AddClause(cls);
				}
			}
		}
	}

	// Assumes that last call to fr.consecution->solve() was
	// satisfiable.  Extracts state(s) cube from satisfying
	// assignment. Reverse: No Lifting
	// Reverse: succ is pred (to be precise)
	size_t stateOfMaxQBFAigqbf(Frame &fr, size_t predOrSucc = 0)
	{
		//cout << "Begin MaxABFAigqbf-Lifting" << endl;
		// create state
		size_t st = newState();
		state(st).successor = predOrSucc;
		// extract and assert primary inputs
		for (VarVec::const_iterator i = model.beginInputs();
				i != model.endInputs(); ++i)
		{
			MyMinisat::lbool val = fr.consecution->modelValue(i->var());
			if (val != MyMinisat::l_Undef)
			{
				MyMinisat::Lit pi = i->lit(val == MyMinisat::l_False);
				state(st).inputs.push_back(pi);  // record full inputs
			}
		}

#ifdef maxqbfaigsolvefixs
		LitVec latches;
		for (VarVec::const_iterator i = model.beginLatches();
				i != model.endLatches(); ++i)
		{
			MyMinisat::lbool val;
			if (revpdr)
				val = fr.consecution->modelValue(model.primeVar(*i).var());
			else
				val = fr.consecution->modelValue(i->var());
			if (val != MyMinisat::l_Undef)
			{
				MyMinisat::Lit la;
				if (revpdr)
				{
					la = model.primeVar(*i).lit(val == MyMinisat::l_False);
				}
				else
				{
					la = i->lit(val == MyMinisat::l_False);
				}
				latches.push_back(la);
			}
		}
#endif

		//TODO: Problem: Inputs are wrong ... they are existentially quantified,
		//i.e. there may be different inputs for different points within the
		//new proof-obligation -> CEX resimulation more complicated

#ifdef maxqbfaigsolvefixs
		std::string problemStr = "";
		vector<int> fVec;
		writeMaxQBFAigsolveProblem(fr, predOrSucc, fVec, &latches, &problemStr); //NULL as last argument if it shall write into a file
#else
		// writeMaxQBFAigsolveProblem(fr, predOrSucc);
#endif /* maxqbfaigsolvefixs */
		std::string cmdStr("./aigqbf/aigsolve --nopre --maxqbf --pacose");
		const char *cmd = cmdStr.c_str();
		std::array<char, 128> buffer;
		std::string result;
		int childPID;
		FILE *pipe_child_in = NULL;
		FILE *pipe_child_out = NULL;
		//bidirectional communication using two pipes
		popen_bidirectional(cmd, childPID, &pipe_child_in, &pipe_child_out);
		if (!pipe_child_in && !pipe_child_out)
		{
			throw std::runtime_error("popen() failed!");
		}
		problemStr.append("\n");
		//Pipe problem file to child (aigsolve)
		int ind = fputs(problemStr.c_str(), pipe_child_in);
		if(ind == EOF)
		{
			throw std::runtime_error("pipe to child failed!");
		}
		pclose2(pipe_child_in, childPID);
		//read result from Pipe
		while (fgets(buffer.data(), buffer.size(), pipe_child_out) != nullptr)
		{
			result += buffer.data();
		}
		pclose2(pipe_child_out, childPID);

		int idx = 0;
		while (idx < fVec.size()) //iterate soft variables
		{
			std::string delimiter = " ";
			int &softVar = fVec[idx];

			size_t pos = 0;
			std::string var;
			while ((pos = result.find(delimiter)) != std::string::npos)
			{
				var = result.substr(0, pos);
				if (var.compare(std::to_string(-softVar)) == 0)
				{
					state(st).latches.push_back(model.unprimeLit(latches[idx]));
					idx++;
					break;
				}
				else
				{
					if (var.compare(std::to_string(softVar)) == 0)
					{
						idx++;
						break;
					}
				}
				result.erase(0, pos + delimiter.length());
			}

		}
		std::cout << "max qbf reduction from "
				<< (model.endLatches() - model.beginLatches()) << " to "
				<< state(st).latches.size() << std::endl;


		return st;
	}
#endif


	// Checks if cube contains any initial states.
	bool initiation(const LitVec &latches)
	{
		//if error is trvially unsat, revPDR may deduce empty cubes
		if (latches.size() == 0)
			return false;

		return !model.isInitial(latches, revpdr);
	}

	bool solveWithMinisat(MyMinisat::Solver *slv, MSLitVec &assumps)
	{
		bool rv;
		assert(slv != NULL);

		if (literalRotation)
		{
			/*
			 * This is the consecution call which uses literal rotation
			 * for UNSAT Generalization. Therefore, do not use the maxFails and
			 * maxTries limits of the PO generalization here. But separate limits
			 * could be implemented...
			 */
			rv = slv->solveWithLiteralRotation(assumps);
		}
		else
		{
			rv = slv->solve(assumps);
		}

		return (rv);
	}


	int nPOGPs = 0;

	// Check if ~latches is inductive relative to frame fi.  If it's
	// inductive and core is provided, extracts the unsat core.  If
	// it's not inductive and pred is provided, extracts
	// predecessor(s).
	bool consecution(size_t fi, const LitVec &latches, size_t succ = 0,
			LitVec *core = NULL, size_t *pred = NULL, bool orderedCore = false,
			bool ctgCheck = false)
	{
		Frame &fr = frames[fi];
		MSLitVec assumps, cls;
		assumps.capacity(1 + latches.size());
		cls.capacity(1 + latches.size());
		MyMinisat::Lit act = MyMinisat::mkLit(fr.consecution->newVar());
		assumps.push(act);
		cls.push(~act);
		for (LitVec::const_iterator i = latches.begin(); i != latches.end();
				++i)
		{
			if (!revpdr)
				cls.push(~*i);
			else
				cls.push(~model.primeLit(*i)); //reverse PDR: primed
			assumps.push(*i);  // push unprimed...
		}
		// ... order... (empirically found to best choice)
		if (pred)
			orderAssumps(assumps, false, 1);
		else
			orderAssumps(assumps, orderedCore, 1);
		// ... now prime (don't prime for reverse PDR)
		if (!revpdr)
		{
			for (int i = 1; i < assumps.size(); ++i)
				assumps[i] = model.primeLit(assumps[i]);
		}
		fr.consecution->addClause_(cls);
		// F_fi & ~latches & T & latches'
		++nQuery;
		startTimer();  // stats
		bool rv = solveWithMinisat(fr.consecution, assumps);
		//bool rv = fr.consecution->solve(assumps);
		endTimer(satTime);
		if (rv)
		{
			// fails: extract predecessor(s)
			if (pred)
			{
#ifdef DUMP_POGEN_INSTANCE
				if(succ != 0 && !ctgCheck && nPOGPs < 100)
				{
					string path = "/home/seufert/POGP";
					path.append(poGenPath.substr(poGenPath.rfind("/")));
					while (path.substr(path.size()-1).compare(".") != 0)
						path.pop_back();
					path.append(std::to_string(nPOGPs));
					printLiftingInstance(fr, state(succ).latches, path); //TODO path
					nPOGPs++;
				}
#endif
				if(!ctgCheck)
					startTimer();
				if (!revpdr)
				{
					if(ctgCheck)
						*pred = stateOf(model, fr, succ, ctgCheck);
					else
						*pred = stateOfPOGP(fr,succ);

				}
				else
					//*pred = stateOfReverse(fr, succ);
					*pred = stateOfPOGP(fr, succ);

				//statistics
				if (!ctgCheck)
				{
					nPoReductions++;
					float red = 1.0
							- (((float) state(*pred).latches.size())
									/ ((float) (model.endLatches()
											- model.beginLatches())));
					if (red > 0)
						nSuccessPoRed++;
					poReduction += red;
					endTimer(poRedTime);
				}
			}
			fr.consecution->releaseVar(~act);
			return false;
		}
		// succeeds
		if (core)
		{
			if (pred && orderedCore)
			{
				// redo with correctly ordered assumps
				reverse(assumps + 1, assumps + assumps.size());
				++nQuery;
				startTimer();  // stats
				bool rv = solveWithMinisat(fr.consecution, assumps);
				assert(!rv);
				endTimer(satTime);
			}
			for (LitVec::const_iterator i = latches.begin(); i != latches.end();
					++i)
			{
				if (!revpdr)
				{
					if (fr.consecution->conflict.has(~model.primeLit(*i)))
						core->push_back(*i);
				}
				else
				{   //reverse PDR case (no successor)
					if (fr.consecution->conflict.has(~*i))
						core->push_back(*i);
				}
			}
			if (!initiation(*core))
			{
				//repair cube with init intersection
				if (model.invariantConstraints().empty())
				{
					if (revpdr)
						model.ungenInitCube(*core, latches);
					else
						model.ungenInitCube(*core, latches, true); //trivial, pre AIGER 1.9, original PDR
				}
				else
					*core = latches;
			}
		}
		fr.consecution->releaseVar(~act);
		return true;
	}

	size_t maxDepth, maxCTGs, maxJoins, micAttempts;

// Based on
//
//   Zyad Hassan, Aaron R. Bradley, and Fabio Somenzi, "Better
//   Generalization in IC3," (submitted May 2013)
//
// Improves upon "down" from the original paper (and the FMCAD'07
// paper) by handling CTGs.
	bool ctgDown(size_t level, LitVec &cube, size_t keepTo, size_t recDepth)
	{
		size_t ctgs = 0, joins = 0;
		while (true)
		{
			// induction check
			if (!initiation(cube))
				return false;
			if (revpdr) //no ctg analysis with reverse PDR (empirically found better)
				recDepth = maxDepth + 1;
			if (recDepth > maxDepth)
			{
				// quick check if recursion depth is exceeded
				LitVec core;
				bool rv = consecution(level, cube, 0, &core, NULL, true, true);
				if (rv && core.size() < cube.size())
				{
					++nCoreReduced;  // stats
					cube = core;
				}
				return rv;
			}
			// prepare to obtain CTG
			size_t cubeState = newState();
			state(cubeState).successor = 0;
			state(cubeState).latches = cube;
			size_t ctg;
			LitVec core;
			if (consecution(level, cube, cubeState, &core, &ctg, true, true))
			{
				if (core.size() < cube.size())
				{
					++nCoreReduced;  // stats
					cube = core;
				}
				// inductive, so clean up
				delState(cubeState);
				return true;
			}
			// not inductive, address interfering CTG
			LitVec ctgCore;
			bool ret = false;
			if (ctgs < maxCTGs && level > 1 && initiation(state(ctg).latches)
					&& consecution(level - 1, state(ctg).latches, cubeState,
							&ctgCore, NULL, false, true))
			{
				// CTG is inductive relative to level-1; push forward and generalize
				++nCTG;  // stats
				++ctgs;
				size_t j = level;
				// QUERY: generalize then push or vice versa?
				while (j <= k && consecution(j, ctgCore))
					++j;
				mic(j - 1, ctgCore, recDepth + 1);
				addCube(j, ctgCore);
			}
			else if (joins < maxJoins)
			{
				// ran out of CTG attempts, so join instead
				ctgs = 0;
				++joins;
				LitVec tmp;
				for (size_t i = 0; i < cube.size(); ++i)
					if (binary_search(state(ctg).latches.begin(),
							state(ctg).latches.end(), cube[i]))
						tmp.push_back(cube[i]);
					else if (i < keepTo)
					{
						// previously failed when this literal was dropped
						++nAbortJoin;  // stats
						ret = true;
						break;
					}
				cube = tmp;  // enlarged cube
			}
			else
				ret = true;
			// clean up
			delState(cubeState);
			delState(ctg);
			if (ret)
				return false;
		}
	}

// Extracts minimal inductive (relative to level) subclause from
// ~cube --- at least that's where the name comes from.  With
// ctgDown, it's not quite a MIC anymore, but what's returned is
// inductive relative to the possibly modifed level.
	void mic(size_t level, LitVec &cube, size_t recDepth)
	{
		++nmic;  // stats
		// try dropping each literal in turn
		size_t attempts = micAttempts;
		orderCube(cube);
		for (size_t i = 0; i < cube.size();)
		{
			LitVec cp(cube.begin(), cube.begin() + i);
			cp.insert(cp.end(), cube.begin() + i + 1, cube.end());
			if (ctgDown(level, cp, i, recDepth))
			{
				// maintain original order
				LitSet lits(cp.begin(), cp.end());
				LitVec tmp;
				for (LitVec::const_iterator j = cube.begin(); j != cube.end();
						++j)
					if (lits.find(*j) != lits.end())
						tmp.push_back(*j);
				cube.swap(tmp);
				// reset attempts
				attempts = micAttempts;
			}
			else
			{
				if (!--attempts)
				{
					// Limit number of attempts: if micAttempts literals in a
					// row cannot be dropped, conclude that the cube is just
					// about minimal.  Definitely improves mics/second to use
					// a low micAttempts, but does it improve overall
					// performance?
					++nAbortMic;  // stats
					return;
				}
				++i;
			}
		}
	}

// wrapper to start inductive generalization
	void mic(size_t level, LitVec &cube)
	{
		mic(level, cube, 1);
	}

	size_t earliest;  // track earliest modified level in a major iteration

// Adds cube to frames at and below level, unless !toAll, in which
// case only to level.
	void addCube(size_t level, LitVec &cube, bool toAll = true, bool silent =
			false)
	{
		if (!revpdr)
		{
			assert(!cube.empty());
		}
		else
		{
			//reverse PDR may proof the empty cube to be unable
			//to reach the unsafe states -> design is safe
			if (cube.empty())
			{
				for (size_t i = toAll ? 1 : level; i <= level; ++i)
				{
					frames[i].consecution->addEmptyClause();
				}
				return;
			}
		}
		sort(cube.begin(), cube.end());
		pair<CubeSet::iterator, bool> rv = frames[level].borderCubes.insert(
				cube);
		if (!rv.second)
			return;
		if (!silent && verbose > 1)
			cout << level << ": " << stringOfLitVec(cube) << endl;
		earliest = min(earliest, level);
		MSLitVec cls;
		cls.capacity(cube.size());
		if (!revpdr)
		{
			for (LitVec::const_iterator i = cube.begin(); i != cube.end(); ++i)
				cls.push(~*i);
		}
		else
		{
			//reverse: frames hold primed clauses
			for (LitVec::const_iterator i = cube.begin(); i != cube.end(); ++i)
				cls.push(~model.primeLit(*i));
		}
		for (size_t i = toAll ? 1 : level; i <= level; ++i)
		{
			frames[i].consecution->addClause(cls);
		}
		if (toAll && !silent)
			updateLitOrder(cube, level);
	}

// ~cube was found to be inductive relative to level; now see if
// we can do better.
	size_t generalize(size_t level, LitVec cube)
	{
		// generalize
		mic(level, cube);
		// push
		do
		{
			++level;
		} while (level <= k && consecution(level, cube));
		addCube(level, cube);
		return level;
	}

	size_t cexState;  // beginning of counterexample trace

// Process obligations according to priority.
	bool handleObligations(PriorityQueue obls)
	{
		while (!obls.empty())
		{
			PriorityQueue::iterator obli = obls.begin();
			Obligation obl = *obli;
			LitVec core;
			size_t predi;
			// Is the obligation fulfilled?
			if (consecution(obl.level, state(obl.state).latches, obl.state,
					&core, &predi))
			{
				// Yes, so generalize and possibly produce a new obligation
				// at a higher level.
				obls.erase(obli);
				size_t n = generalize(obl.level, core);
				if (n <= k)
					obls.insert(Obligation(obl.state, n, obl.depth));
			}
			else if (obl.level == 0)
			{
				// No, in fact an initial state is a predecessor.
				cout << "CEX depth: " << obl.depth << endl;
				cexState = predi;
				return false;
			}
			else
			{
				++nCTI;  // stats
				// No, so focus on predecessor.
				obls.insert(Obligation(predi, obl.level - 1, obl.depth + 1));
			}
		}
		return true;
	}

	bool trivial;  // indicates whether strengthening was required
// during major iteration

// Strengthens frontier to remove error successors.
	bool strengthen()
	{
		Frame &frontier = frames[k];
		trivial = true;  // whether any cubes are generated
		earliest = k + 1;  // earliest frame with enlarged borderCubes
		while (true)
		{
			++nQuery;
			startTimer();  // stats
			bool rv;
			if (!revpdr)
			{
				rv = frontier.consecution->solve(model.primedError());
			}
			else
			{
				MyMinisat::vec<MyMinisat::Lit> assumps;
				LitVec init = model.getInitLits();
				assumps.capacity(init.size());
				for (auto &lit : init)
				{
					assumps.push(lit);
				}
				rv = frontier.consecution->solve(assumps);
			}
			endTimer(satTime);
			if (!rv)
				return true;
			// handle CTI with error successor
			++nCTI;  // stats
			trivial = false;
			PriorityQueue pq;
			// enqueue main obligation and handle
			startTimer();

			pq.insert(Obligation(stateOfPOGP(frontier), k-1, 1));

			/*stats*/
			nPoReductions++;
			float red = 1.0
					- (((float) state(pq.begin()->state).latches.size())
							/ ((float) (model.endLatches()
									- model.beginLatches())));
			poReduction += red;
			if (red > 0)
				nSuccessPoRed++;
			endTimer(poRedTime);
			/*******/

			if (!handleObligations(pq))
				return false;
			// finished with States for this iteration, so clean up
			resetStates();
		}
	}

// Propagates clauses forward using induction.  If any frame has
// all of its clauses propagated forward, then two frames' clause
// sets agree; hence those clause sets are inductive
// strengthenings of the property.  See the four invariants of IC3
// in the original paper.
	bool propagate()
	{
		if (verbose > 1)
			cout << "propagate" << endl;
		// 1. clean up: remove c in frame i if c appears in frame j when i < j
		CubeSet all;
		for (size_t i = k + 1; i >= earliest; --i)
		{
			Frame &fr = frames[i];
			CubeSet rem, nall;
			set_difference(fr.borderCubes.begin(), fr.borderCubes.end(),
					all.begin(), all.end(), inserter(rem, rem.end()),
					LitVecComp());
			if (verbose > 1)
				cout << i << " " << fr.borderCubes.size() << " " << rem.size()
						<< " ";
			fr.borderCubes.swap(rem);
			set_union(rem.begin(), rem.end(), all.begin(), all.end(),
					inserter(nall, nall.end()), LitVecComp());
			all.swap(nall);
			for (CubeSet::const_iterator i = fr.borderCubes.begin();
					i != fr.borderCubes.end(); ++i)
				assert(all.find(*i) != all.end());
			if (verbose > 1)
				cout << all.size() << endl;
		}
		// 2. check if each c in frame i can be pushed to frame j
		for (size_t i = trivial ? k : 1; i <= k; ++i)
		{
			int ckeep = 0, cprop = 0, cdrop = 0;
			Frame &fr = frames[i];
			for (CubeSet::iterator j = fr.borderCubes.begin();
					j != fr.borderCubes.end();)
			{
				LitVec core;
				if (consecution(i, *j, 0, &core))
				{
					++cprop;
					// only add to frame i+1 unless the core is reduced
					addCube(i + 1, core, core.size() < j->size(), true);
					CubeSet::iterator tmp = j;
					++j;
					fr.borderCubes.erase(tmp);
				}
				else
				{
					++ckeep;
					++j;
				}
			}
			if (verbose > 1)
				cout << i << " " << ckeep << " " << cprop << " " << cdrop
						<< endl;
			if (fr.borderCubes.empty())
				return true;
		}
		// 3. simplify frames
		for (size_t i = trivial ? k : 1; i <= k + 1; ++i)
			frames[i].consecution->simplify();

		//____________________TEST__________
		//lifts->simplify();
		poGen->simplifySolver();
		//__________________________________

		return false;
	}

	void printBitVector(uint64_t vec)
	{
		std::vector<int> v;
		for (size_t i = 0; i < 64; ++i)
		{
			v.push_back((vec >> i) % 2);
		}
		for (vector<int>::const_reverse_iterator it = v.rbegin();
				it != v.rend(); ++it)
			cout << *it;

		std::cout << std::endl;
	}


	int nQuery, nCTI, nCTG, nmic;
	float poReduction, nPoReductions, nSuccessPoRed;
	clock_t startTime, satTime, poRedTime;
	int nCoreReduced, nAbortJoin, nAbortMic;
	clock_t time()
	{
		struct tms t;
		times(&t);
		return t.tms_utime;
	}
	clock_t timer;
	void startTimer()
	{
		timer = time();
	}
	void endTimer(clock_t &t)
	{
		t += (time() - timer);
	}
	void printOValues()
	{
#ifdef pacosepogen
#ifdef pacoseprintovalues
		if(!oValues.empty())
		{

			cout << left <<  ". o Values of Pacose: " << endl;
			for(auto oValue: oValues)
				cout << oValue << endl;
		}
		cout << endl;
#endif
#endif
	}
	void printStats()
	{
		if (!verbose)
			return;
		clock_t etime = time();
		cout << ". Elapsed time: " << ((double) etime / sysconf(_SC_CLK_TCK))
				<< endl;
		etime -= startTime;
		if (!etime)
			etime = 1;
		cout << ". % SAT:        "
				<< (int) (100 * (((double) satTime) / ((double) etime)))
				<< endl;
		cout << ". K:            " << k << endl;
		cout << ". # Queries:    " << nQuery << endl;
		cout << ". # CTIs:       " << nCTI << endl;
		cout << ". # CTGs:       " << nCTG << endl;
		cout << ". # mic calls:  " << nmic << endl;
		cout << ". Queries/sec:  "
				<< (int) (((double) nQuery) / ((double) etime)
						* sysconf(_SC_CLK_TCK)) << endl;
		cout << ". Mics/sec:     "
				<< (int) (((double) nmic) / ((double) etime)
						* sysconf(_SC_CLK_TCK)) << endl;
		cout << ". # Red. cores: " << nCoreReduced << endl;
		cout << ". # Int. joins: " << nAbortJoin << endl;
		cout << ". # Int. mics:  " << nAbortMic << endl;
		if (nPoReductions > 0)
		{
			cout << ". Avg. red. rate PO: " << poReduction / nPoReductions
					<< endl;
		}
		if (nSuccessPoRed > 0)
		{
			cout << ". Avg. red. rate PO (successful): "
					<< poReduction / nSuccessPoRed << endl;
		}
		cout << ". # Red. PO: " << nPoReductions << endl;
		cout << ". # Red. PO (successful): " << nSuccessPoRed << endl;
		cout << ". Avg time required for PO red.: "
				<< ((double) poRedTime / sysconf(_SC_CLK_TCK))
						/ (float) nPoReductions << endl;
		cout << ". Total time required for PO red.: "
				<< ((double) poRedTime / sysconf(_SC_CLK_TCK)) << endl;
		if (numUpdates)
			cout << ". Avg lits/cls: " << numLits / numUpdates << endl;
		if (MaxSATTime > 0)
			cout << ". MaxSAT time (overall): " << MaxSATTime << endl;
		if (MaxQBFTime > 0)
			cout << ". MaxQBF time (overall): " << MaxQBFTime << endl;
	}

	void appendLitVecToString(LitVec &lv, std::string &str)
	{
		for (auto &lit : lv)
		{
			str.append(std::to_string(MyMinisat::toInt(lit)));
			str.append(" ");
		}
		str.append("\n");
	}

	void printLiftingInstance(Frame &fr, LitVec &succ, std::string &pathstr)
	{
		LitVec inputs, pred, fullAssignment;
		//record inputs
		for (VarVec::const_iterator i = model.beginInputs();
				i != model.endInputs(); ++i)
		{
			MyMinisat::lbool val = fr.consecution->modelValue(i->var());
			if (val != MyMinisat::l_Undef)
			{
				MyMinisat::Lit pi = i->lit(val == MyMinisat::l_False);
				inputs.push_back(pi);
			}
		}
		// record pred latches
		LitVec latches;
		for (VarVec::const_iterator i = model.beginLatches();
				i != model.endLatches(); ++i)
		{
			MyMinisat::lbool val;
			if (revpdr)
				val = fr.consecution->modelValue(model.primeVar(*i).var());
			else
				val = fr.consecution->modelValue(i->var());
			if (val != MyMinisat::l_Undef)
			{
				MyMinisat::Lit la = i->lit(val == MyMinisat::l_False);
				pred.push_back(la);
			}
		}
		for (int var = 0; var < fr.consecution->nVars(); ++var)
		{
			MyMinisat::lbool val = fr.consecution->modelValue(var);
			if (val != MyMinisat::l_Undef)
			{
				MyMinisat::Lit a = MyMinisat::mkLit(var, (val == MyMinisat::l_False));
				fullAssignment.push_back(a);
			}
		}

		std::string inputstr = "";
		appendLitVecToString(inputs, inputstr);
		std::string predstr = "pred:\n";
		appendLitVecToString(pred, predstr);
		std::string succstr = "succ:\n";
		appendLitVecToString(succ, succstr);
		std::string fAssstr = "fullAssignment:\n";
		appendLitVecToString(fullAssignment, fAssstr);
		std::string cnf = "cnf:\n";

		for (size_t i = fr.k; i < frames.size(); ++i)
		{
			Frame &_fr = frames[i];
			for (CubeSet::iterator j = _fr.borderCubes.begin();
					j != _fr.borderCubes.end(); ++j)
			{
				//collect cubes -> non-negated
				for (auto &lit : *j)
				{
					cnf.append(std::to_string(MyMinisat::toInt(lit)));
					cnf.append(" ");
				}
				cnf.append("\n");
			}
		}
		std::ofstream cnfFile;
		std::string fileStr = "";
		fileStr.append(inputstr).append(predstr).append(succstr).append(fAssstr).append(
				cnf);

		const char *path = pathstr.c_str();
		cnfFile.open(path);
		cnfFile << fileStr;
		cnfFile.close();
	}

	friend bool check(Model&, Model&, string&, string&, GeneralOpt&, int, bool, bool, bool, bool);

};

// IC3 does not check for 0-step and 1-step reachability, so do it
// separately.
bool baseCases(Model &model)
{
	MyMinisat::Solver *base0 = model.newSolver();
	model.loadInitialCondition(*base0);
	model.loadError(*base0);
	bool rv = base0->solve(model.error());
	delete base0;
	if (rv)
		return false;

	MyMinisat::Solver *base1 = model.newSolver();
	model.loadInitialCondition(*base1);
	model.loadTransitionRelation(*base1);
	rv = base1->solve(model.primedError());
	delete base1;
	if (rv)
		return false;

	model.lockPrimes();

	return true;
}

// External function to make the magic happen.
bool check(Model &model, Model &modelWithSelfLoops, string &poGenPath,
		string &poGenType, GeneralOpt &genOpt,
		int verbose, bool basic, bool random, bool revpdr, bool litrot)
{
	if (!baseCases(model))
		return false;
	IC3 ic3(model, modelWithSelfLoops, poGenPath, genOpt, verbose, basic, random, revpdr, litrot, poGenType);
	bool rv = ic3.check();
	if (!rv && verbose > 1)
		ic3.printWitness();
	if (verbose)
		ic3.printStats();
	ic3.printOValues();
	return rv;
}


}
