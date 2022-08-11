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

#include <iostream>

#include "Model.h"
#include "SimpSolver.h"
#include "Vec.h"

//MyMinisat::Var Var::gvi = 0;

Model::~Model()
{
	if (inits)
		delete inits;
	if (sslv)
		delete sslv;
	if (sslvImplgraph)
		delete sslvImplgraph;
	if (sslvLifting)
		delete sslvLifting;
	if (sslvReduced)
		delete sslvReduced;
	if (sslvError)
		delete sslvError;
	//TODO: some are missing!
}

const Var& Model::primeVar(const Var &v, MyMinisat::SimpSolver *slv)
{
	// var for false
	if (v.index() == 0)
		return v;
	// latch or PI
	if (v.index() < reps)
		return vars[primes + v.index() - inputs];
	// AND lit
	assert(v.index() >= reps && v.index() < primes);
	// created previously?
	IndexMap::const_iterator i = primedAnds.find(v.index());
	size_t index;
	if (i == primedAnds.end())
	{
		// no, so make sure the model hasn't been locked
		assert(primesUnlocked);
		// create a primed version
		stringstream ss;
		ss << v.name() << "'";
		index = vars.size();
		vars.push_back(Var(ss.str(), gvi));
		if (slv)
		{
			MyMinisat::Var _v = slv->newVar();
			assert(_v == vars.back().var());
		}
		assert(vars.back().index() == index);
		primedAnds.insert(IndexMap::value_type(v.index(), index));
	}
	else
		index = i->second;
	return vars[index];
}

MyMinisat::Solver* Model::newSolver() const
{
	MyMinisat::Solver *slv = new MyMinisat::Solver();
	// load all variables to maintain alignment
	for (size_t i = 0; i < vars.size(); ++i)
	{
		MyMinisat::Var nv = slv->newVar();
		assert(nv == vars[i].var());
	}
	return slv;
}

#ifdef maxsatpogen
antom::Antom* Model::newAntom()
{
	antom::Antom *slv = new antom::Antom();
	// load all variables to maintain alignment
	for (size_t i = 1; i < vars.size(); ++i)
	{
		uint32_t nv = slv->NewVariable();
		assert(nv == vars[i].var());
	}
	if (antomAuxiliaryVarsInitialized)
	{
		for (size_t i = 0; i < vars.size(); ++i)
		{
			uint32_t nv = slv->NewVariable();
			assert(nv == antomAuxiliaryVars.at(i));
		}
	}
	else
	{
		//create auxiliary variable vector for 01X encoding
		for (size_t i = 0; i < vars.size(); ++i)
		{
			uint32_t nv = slv->NewVariable();
			antomAuxiliaryVars.push_back(nv);
			assert(nv == antomAuxiliaryVars.at(i));
		}
	}
	antomAuxiliaryVarsInitialized = true;

	addSoftClauses(*slv); //depends on incremental mode (only 2 keeps soft clauses)
	//aux var and soft clause act vars alignment required only once

	return slv;
}
#endif /* maxsatpogen */

QDPLL* Model::newDepQBF(vector<VarID> &muxAuxExist, vector<VarID> &muxAuxForall,
		vector<VarID> &muxSelect, bool revpdr)
{
	assert(muxAuxExist.empty());
	assert(muxAuxForall.empty());
	assert(muxSelect.empty());

	/*
	 * sslv must exist
	 * <=> transition relation was loaded at least once
	 * <=> vars vector is already enlarged by primed inputs/latches and aux variables
	 */
	assert(sslv);

	QDPLL *slv = qdpll_create();
	qdpll_configure(slv, "--dep-man=simple");
	qdpll_configure(slv, "--incremental-use");

	/*
	 * DepQBF is used for lifting; the last consecution call SAT?[R_i * T * s'] was
	 * satisfiable and we got a new proof obligation ŝ which is a full assignment of
	 * the latch variables under a certain input assignment.
	 * Now we want to generalize ŝ (SAT generalization) by using QBF. This is done by
	 * greedily quantify one latch literal after another with the forall quantifier.
	 * If the QBF-call is SAT, the corresponding latch literal is not responsible for
	 * the transition T to start in ŝ and end in s and can be neglected. If the QBF-call
	 * is UNSAT we cannot drop the latch literal and quantify it as existential again.
	 *
	 * This quantifier alternations of the latch literals are done by using a multiplexer.
	 * We have a selector variable f which chooses between the latch literal l being
	 * existentially (s) or universally (r) quantified.
	 * These are the clauses:
	 * (~f + ~l + r)
	 * (~f + l + ~r)
	 * (f + ~l + s)
	 * (f + l + ~s)
	 * If f is 1, then l <=> r, else l <=> s
	 *
	 * To model this, we need 3 quantification layers:
	 *		(1) existential: exists f, s
	 *		(2) universal: forall r
	 *		(3) existential: exists [all other variables of the instance]
	 *
	 * In this function, we build those three layers and fill it with the variables.
	 * The transition relation (and init) will be included separately.
	 * ŝ and s' are only assumed.
	 */

	/* add and open new leftmost existential block at nesting level 1 */
	qdpll_new_scope_at_nesting(slv, QDPLL_QTYPE_EXISTS, 1);
	/* close open scope */
	qdpll_add(slv, 0);
	/* add and open new universal block at nesting level 2 */
	qdpll_new_scope_at_nesting(slv, QDPLL_QTYPE_FORALL, 2);
	/* close open scope */
	qdpll_add(slv, 0);
	/* add and open new existential block at nesting level 3 */
	qdpll_new_scope_at_nesting(slv, QDPLL_QTYPE_EXISTS, 3);
	/* close open scope */
	qdpll_add(slv, 0);
	assert(qdpll_get_max_scope_nesting(slv) == 3);

	/*
	 * load all variables variables of the problem instance to maintain alignment
	 * to nesting level 3
	 */
	for (size_t i = 1; i < vars.size(); ++i)
	{
		qdpll_add_var_to_scope(slv, vars[i].var(), 3);
		assert(qdpll_is_var_declared(slv, vars[i].var()));
		assert(qdpll_get_nesting_of_var(slv, vars[i].var()) == 3);
	}
	assert(qdpll_get_max_declared_var_id(slv) == vars.size() - 1);

	/* we start with a fresh variable */
	VarID currVarCounter = qdpll_get_max_declared_var_id(slv) + 1;

	/* load variables for multiplexer structure */
	for (VarVec::const_iterator i = beginLatches(); i != endLatches(); ++i)
	{
		assert(primeVar(*i).var() > 1);

		VarID f_i = currVarCounter;
		qdpll_add_var_to_scope(slv, f_i, 1); /* existentially quantified at level 1 */
		assert(qdpll_is_var_declared(slv, f_i));
		assert(qdpll_get_nesting_of_var(slv, f_i) == 1);
		muxSelect.push_back(f_i);
		currVarCounter++;

		VarID r_i = currVarCounter;
		qdpll_add_var_to_scope(slv, r_i, 2); /* universally quantified at level 2 */
		assert(qdpll_is_var_declared(slv, r_i));
		assert(qdpll_get_nesting_of_var(slv, r_i) == 2);
		muxAuxForall.push_back(r_i);
		currVarCounter++;

		VarID s_i = currVarCounter;
		qdpll_add_var_to_scope(slv, s_i, 1); /* existentially quantified at level 1 */
		assert(qdpll_is_var_declared(slv, s_i));
		assert(qdpll_get_nesting_of_var(slv, s_i) == 1);
		muxAuxExist.push_back(s_i);
		currVarCounter++;
	}

	qdpll_push(slv);

	/* load multiplexer clauses */
	for (size_t i = 0; i < muxSelect.size(); i++)
	{
		VarID f_i = muxSelect[i];
		VarID r_i = muxAuxForall[i];
		VarID s_i = muxAuxExist[i];
		VarID l_i;

		if (revpdr)
		{
			l_i = primeVar(*(beginLatches() + i)).var();
		}
		else
		{
			l_i = (beginLatches() + i)->var();
		}

		/* (~f_i + ~l_i + r_i) * (~f_i + l_i + ~r_i) */
		qdpll_add(slv, -f_i);
		qdpll_add(slv, -l_i);
		qdpll_add(slv, r_i);
		qdpll_add(slv, 0);

		qdpll_add(slv, -f_i);
		qdpll_add(slv, l_i);
		qdpll_add(slv, -r_i);
		qdpll_add(slv, 0);

		/* (f_i + ~l_i + s_i) * (f_i + l_i + ~s_i) */
		qdpll_add(slv, f_i);
		qdpll_add(slv, -l_i);
		qdpll_add(slv, s_i);
		qdpll_add(slv, 0);

		qdpll_add(slv, f_i);
		qdpll_add(slv, l_i);
		qdpll_add(slv, -s_i);
		qdpll_add(slv, 0);
	}

	return slv;
}

Pacose* Model::newPacose(vector<unsigned int> *dontCares)
{
	Pacose *slv = new Pacose();
#ifdef pacoseincremental
	slv->_settings.incremental = true;
	slv->_settings.testIfDividable = 0;
	slv->_settings._encoding = DGPW18;
	slv->_settings.reuseDGPW = true;
	slv->_settings.greedyPPFixSCs = 0;
#else
	slv->_settings.incremental = false;
	slv->_settings.testIfDividable = 0;
	slv->_settings._encoding = DGPW18;
	slv->_settings.greedyPPFixSCs = 0;
	/*slv->_settings._encoding = WARNERS;
	 slv->_settings.greedyPPFixSCs = 1;
	 slv->_settings.greedyMinSizeOfSet = 1;*/
#endif
	//slv->_settings.greedyMinSizeOfSet = 1;
	// load all variables to maintain alignment
	//slv->InitSatSolver(SATSolverType::GLUCOSE421); //currently: glucose421 (see SATSolverType enum)
	/*
	 * 0 == GLUCOSE421
	 * 1 == GLUCOSE3
	 * 2 == CADICAL
	 * 3 == CRYPTOMINISAT
	 */
	slv->InitSatSolver(0);
	for (size_t i = 1; i < vars.size(); ++i)
	{
		int nv = slv->NewVariable();
		assert(nv == vars[i].var());
	}
	if (antomAuxiliaryVarsInitialized)
	{
		for (size_t i = 0; i < vars.size(); ++i)
		{
			unsigned nv = slv->NewVariable();
			assert(nv == antomAuxiliaryVars.at(i));
		}
	}
	else
	{
		//create auxiliary variable vector for 01X encoding
		for (size_t i = 0; i < vars.size(); ++i)
		{
			unsigned nv = slv->NewVariable();
			antomAuxiliaryVars.push_back(nv);
			assert(nv == antomAuxiliaryVars.at(i));
		}
	}
	antomAuxiliaryVarsInitialized = true;
	addSoftClausesPacose(*slv, dontCares);
	//aux var and soft clause act vars alignment required only once
	return slv;
}

void* Model::newIpamirSolver()
{
	void *slv = ipamir_init();
	cout << ipamir_signature() << endl;

	ipamirMaxVar = getMaxVar() - 1;
	assert((int)ipamirMaxVar == vars.back().var());

	//create auxiliary variable vector for 01X encoding
	for (size_t i = 0; i < vars.size(); ++i)
	{
		unsigned nv = ++ipamirMaxVar;
		antomAuxiliaryVars.push_back(nv);
		assert(nv == antomAuxiliaryVars.at(i));
	}

	addSoftClausesIpamir(slv);
	//aux var and soft clause act vars alignment required only once
	return slv;
}

/*
 * We can set a preferred zero decision for only latches, if we tweak the
 * decision heuristics to decide latches first.
 * Otherwise we drastically reduce generalization power.
 */
MyMinisat::Solver* Model::newTernSatSlv()
{
	MyMinisat::Solver *slv = new MyMinisat::Solver();

	for (size_t i = 0; i < vars.size(); ++i)
	{

		int nv = slv->newVar(MyMinisat::l_True);
		assert(nv == vars[i].var());
	}
	if (antomAuxiliaryVarsInitialized)
	{
		for (size_t i = 0; i < vars.size(); ++i)
		{
			unsigned nv = slv->newVar(MyMinisat::l_True);	//MyMinisat::l_False);
			assert(nv == antomAuxiliaryVars.at(i));
		}
	}
	else
	{
		//create auxiliary variable vector for 01X encoding
		for (size_t i = 0; i < vars.size(); ++i)
		{

			unsigned nv = slv->newVar(MyMinisat::l_True);
			antomAuxiliaryVars.push_back(nv);
			assert(nv == antomAuxiliaryVars.at(i));
		}
	}
	antomAuxiliaryVarsInitialized = true;
	//aux var and soft clause act vars alignment required only once
//#define ternsat_heuristics_tweak
#ifdef ternsat_heuristics_tweak
	//set latches and aux latches
	slv->setOrderHeapLatches(
			beginLatches()->var(),
			(endLatches()-1)->var(),
			get01XAuxVar(beginLatches()->var()),
			get01XAuxVar((endLatches()-1)->var())
			);
#endif
	return slv;
}

#ifdef maxsatpogen
void Model::addSoftClauses(antom::Antom &slv)
{
	/* soft clauses: one for each latch variable, incremental mode 0
	 if(softClActVarsInitialized && softClActVars.at(0) < slv.Variables())
	 {
	 //just add soft clauses
	 for(auto& nv: softClActVars)
	 //soft clause t
	 {
	 std::vector<uint32_t> cl;
	 cl.push_back(2 * nv);

	 slv.AddSoftClause(cl);
	 assert(nv < slv.Variables());
	 }
	 return;
	 }*/

	for (VarVec::const_iterator i = beginLatches(); i != endLatches(); ++i)
	{
		//t -> (s_0 = 0 & s_1 = 0) <-> s=X
		//(~t | ~s_0) & (~t | ~s_1)
		uint32_t nv;
		if (softClActVarsInitialized)
		{
			//non-incremental
			nv = slv.NewVariable();// create new variable
			//incremental mode 0:
			/*if(softClActVars.at(i->var()-beginLatches()->var()) > slv.Variables())
			 nv = slv.NewVariable(); // create new variable
			 else
			 nv = softClActVars.at(i->var()-beginLatches()->var()); //re-use*/
		}
		else
		{
			nv = slv.NewVariable(); // create new variable
			softClActVars.push_back(nv);
		}
		assert(nv == softClActVars.at(i->var() - beginLatches()->var()));
		std::vector<uint32_t> cl;
		cl.clear();
		cl.push_back(2 * nv + 1); //~t
		cl.push_back(2 * (i->var()) + 1);//~s_0
		slv.AddClause(cl);
		cl.clear();
		cl.push_back(2 * nv + 1);//~t
		cl.push_back(2 * get01XAuxVar(i->var()) + 1);//~s_1
		slv.AddClause(cl);

		//soft clause t
		cl.clear();
		cl.push_back(2 * nv);

		slv.AddSoftClause(cl);
	}

	softClActVarsInitialized = true;
}
#endif /* maxsatpogen */

void Model::addSoftClausesPacose(Pacose &slv, vector<unsigned int> *dontCares)
{
	for (VarVec::const_iterator i = beginLatches(); i != endLatches(); ++i)
	{
		//t -> (s_0 = 0 & s_1 = 0) <-> s=X
		//(~t | ~s_0) & (~t | ~s_1)
		unsigned nv;
		if (softClActVarsInitialized)
		{
			//non-incremental
			nv = slv.NewVariable();
		}
		else
		{
			nv = slv.NewVariable();
			softClActVars.push_back(nv);
		}
		assert(nv == softClActVars.at(i->var() - beginLatches()->var()));
		std::vector<uint32_t> cl;
		cl.clear();
		cl.push_back(2 * nv + 1); //~t
		cl.push_back(2 * (i->var()) + 1); //~s_0
		slv.AddClause(cl);
		cl.clear();
		cl.push_back(2 * nv + 1); //~t
		cl.push_back(2 * get01XAuxVar(i->var()) + 1); //~s_1
		slv.AddClause(cl);

		//soft clause t
		cl.clear();

		//if(!dontCares)
		{
			//cl.push_back(2 * nv);

			/*
			 * pacose soft clause code... why the fuck does it work like this and
			 * not if I call the API-routine?
			 *
			 unsigned relaxLit = static_cast<unsigned>(slv._satSolver->NewVariable() << 1);
			 SoftClause *SC = new SoftClause(relaxLit, cl);
			 slv._originalSoftClauses.push_back(SC);

			 cl.push_back(relaxLit);

			 bool test = slv._satSolver->AddClause(cl);
			 assert(test);
			 */

			//slv.AddSoftClause(cl);
		}
	}
	//Problem: pacose introduces relaxation literals for each new soft clause,
	//thus the variable alignment doesn't fit in "heuristic" mode

	//for heuristics combining MaxSAT with other techniques
	//if a set of don't cares is known a priori
	int nSoftCl = 0;
	//if(dontCares)
	{
		for (VarVec::const_iterator i = beginLatches(); i != endLatches(); ++i)
		{
			std::vector<uint32_t> cl;
			//premise: dontCares is sorted!
			cl.push_back(
					2 * softClActVars.at(i->var() - beginLatches()->var()));
			if (dontCares)
			{
				if (binary_search(dontCares->begin(), dontCares->end(),
						i->var()))
				{
					slv.AddClause(cl);
					continue; // already don't care -> no soft clause, hard clause instead
				}
			}
			slv.AddSoftClause(cl);
			nSoftCl++;
		}
	}

	softClActVarsInitialized = true;
}

void Model::addSoftClausesIpamir(void * solver)
{
	for (VarVec::const_iterator i = beginLatches(); i != endLatches(); ++i)
	{
		//t -> (s_0 = 0 & s_1 = 0) <-> s=X
		//(~t | ~s_0) & (~t | ~s_1)
		unsigned nv = ++ipamirMaxVar;

		softClActVars.push_back(nv);

		assert(nv == softClActVars.at(i->var() - beginLatches()->var()));
		std::vector<MyMinisat::Lit> cl;
		cl.clear();
		cl.push_back(MyMinisat::mkLit(nv, true)); //~t
		cl.push_back(i->lit(true)); //~s_0
		ipamirAddClause(solver, cl);
		cl.clear();
		cl.push_back(MyMinisat::mkLit(nv, true)); //~t
		cl.push_back(MyMinisat::mkLit(get01XAuxVar(i->var()),true)); //~s_1
		ipamirAddClause(solver, cl);

		//soft clause t
		cl.clear();
	}

	// vars = <i1,...,in,l0,....lm,....>
	for (VarVec::const_iterator i = beginLatches(); i != endLatches(); ++i)
	{
		MyMinisat::Lit sEquX = MyMinisat::mkLit(softClActVars.at(i->var() - beginLatches()->var()), false);
		ipamirAddSoftLit(solver, sEquX);
	}

	cout << "soft clauses added" << endl;
}

void Model::loadTransitionRelation(MyMinisat::Solver &slv, bool primeConstraints,
		bool selfLoops, bool noConstraints, bool noError)
{
	bool firstRun = false;
	if (!sslv)
	{
		firstRun = true;
		// how often literals occur
		// create a simplified CNF version of (this slice of) the TR
		sslv = new MyMinisat::SimpSolver();
		// introduce all variables to maintain alignment
		for (size_t i = 0; i < vars.size(); ++i)
		{
			MyMinisat::Var nv = sslv->newVar();
			assert(nv == vars[i].var());
		}
		// freeze inputs, latches, and special nodes (and primed forms)
		for (VarVec::const_iterator i = beginInputs(); i != endInputs(); ++i)
		{
			sslv->setFrozen(i->var(), true);
			sslv->setFrozen(primeVar(*i).var(), true);
		}
		for (VarVec::const_iterator i = beginLatches(); i != endLatches(); ++i)
		{
			sslv->setFrozen(i->var(), true);
			sslv->setFrozen(primeVar(*i).var(), true);
		}
		sslv->setFrozen(varOfLit(error()).var(), true);
		sslv->setFrozen(varOfLit(primedError()).var(), true);
		for (LitVec::const_iterator i = constraints.begin();
				i != constraints.end(); ++i)
		{
			Var v = varOfLit(*i);
			sslv->setFrozen(v.var(), true);
			sslv->setFrozen(primeVar(v).var(), true);
		}
		// initialize with roots of required formulas
		LitSet require;  // unprimed formulas
		for (VarVec::const_iterator i = beginLatches(); i != endLatches(); ++i)
			require.insert(nextStateFn(*i));
		require.insert(_error);
		require.insert(constraints.begin(), constraints.end());
		LitSet prequire; // for primed formulas; always subset of require
		prequire.insert(_error);
		prequire.insert(constraints.begin(), constraints.end());

		if (selfLoops) //self-loops for correct lifting
		{
			require.insert(_nand);
			prequire.insert(_nand);
			sslv->setFrozen(MyMinisat::var(_nand), true);
			sslv->setFrozen(MyMinisat::var(_nand), true);
		}

		// traverse AIG backward
		for (AigVec::const_reverse_iterator i = aig.rbegin(); i != aig.rend();
				++i)
		{
			// skip if this row is not required
			if (require.find(i->lhs) == require.end()
					&& require.find(~i->lhs) == require.end())
				continue;
			// encode into CNF
			sslv->addClause(~i->lhs, i->rhs0);
			sslv->addClause(~i->lhs, i->rhs1);
			sslv->addClause(~i->rhs0, ~i->rhs1, i->lhs);
			// require arguments
			require.insert(i->rhs0);
			require.insert(i->rhs1);
			// primed: skip if not required
			if (prequire.find(i->lhs) == prequire.end()
					&& prequire.find(~i->lhs) == prequire.end())
				continue;
			// encode PRIMED form into CNF
			MyMinisat::Lit r0 = primeLit(i->lhs, sslv), r1 = primeLit(i->rhs0,
					sslv), r2 = primeLit(i->rhs1, sslv);
			sslv->addClause(~r0, r1);
			sslv->addClause(~r0, r2);
			sslv->addClause(~r1, ~r2, r0);
			// require arguments
			prequire.insert(i->rhs0);
			prequire.insert(i->rhs1);
		}
		sslv->addClause(btrue());
		if (!noError)
			sslv->addClause(~_error);
		// assert literal for true
		if (!noConstraints)
		{
			// assert ~error, constraints, and primed constraints
			if (!selfLoops)
			{
				for (LitVec::const_iterator i = constraints.begin();
						i != constraints.end(); ++i)
				{
					sslv->addClause(*i);
				}
			}
		}
		// assert l' = f for each latch l
		for (VarVec::const_iterator i = beginLatches(); i != endLatches(); ++i)
		{
			MyMinisat::Lit platch = primeLit(i->lit(false)), f = nextStateFn(*i);
			sslv->addClause(~platch, f);
			sslv->addClause(~f, platch);
		}
		sslv->eliminate(true);
	}
	// load the clauses from the simplified context
	assert((unsigned)sslv->nVars() == vars.size());
	while (slv.nVars() < sslv->nVars())
		slv.newVar();

	if(firstRun)
		litOccurence = vector<int>(2 * vars.size() + 1, 0);
	for (MyMinisat::ClauseIterator c = sslv->clausesBegin();
			c != sslv->clausesEnd(); ++c)
	{
		const MyMinisat::Clause &cls = *c;
		MyMinisat::vec<MyMinisat::Lit> cls_;
		for (int i = 0; i < cls.size(); ++i)
		{
			if(firstRun)
				litOccurence.at(MyMinisat::toInt(cls[i]))++;
			cls_.push(cls[i]);
		}
		slv.addClause_(cls_);
	}
	for (MyMinisat::TrailIterator c = sslv->trailBegin(); c != sslv->trailEnd();
			++c)
		slv.addClause(*c);
	if (primeConstraints && !noConstraints && !selfLoops)
		for (LitVec::const_iterator i = constraints.begin();
				i != constraints.end(); ++i)
			slv.addClause(primeLit(*i));
}

void Model::loadTransitionRelationImplgraph(MyMinisat::Solver &slv)
{
	if (!sslvImplgraph)
	{
		// create a simplified CNF version of (this slice of) the TR
		sslvImplgraph = new MyMinisat::SimpSolver();
		// introduce all variables to maintain alignment
		for (size_t i = 0; i < vars.size(); ++i)
		{
			MyMinisat::Var nv = sslvImplgraph->newVar();
			assert(nv == vars[i].var());
		}
		// freeze inputs, latches, and special nodes (and primed forms)
		for (VarVec::const_iterator i = beginInputs(); i != endInputs(); ++i)
		{
			sslvImplgraph->setFrozen(i->var(), true);
			sslvImplgraph->setFrozen(primeVar(*i).var(), true);
		}
		for (VarVec::const_iterator i = beginLatches(); i != endLatches(); ++i)
		{
			sslvImplgraph->setFrozen(i->var(), true);
			sslvImplgraph->setFrozen(primeVar(*i).var(), true);
		}
		sslvImplgraph->setFrozen(varOfLit(error()).var(), true);
		sslvImplgraph->setFrozen(varOfLit(primedError()).var(), true);
		for (LitVec::const_iterator i = constraints.begin();
				i != constraints.end(); ++i)
		{
			Var v = varOfLit(*i);
			sslvImplgraph->setFrozen(v.var(), true);
			sslvImplgraph->setFrozen(primeVar(v).var(), true);
		}
		// initialize with roots of required formulas
		LitSet require;  // unprimed formulas
		for (VarVec::const_iterator i = beginLatches(); i != endLatches(); ++i)
			require.insert(nextStateFn(*i));
		require.insert(_error);
		require.insert(constraints.begin(), constraints.end());
		LitSet prequire; // for primed formulas; always subset of require
		prequire.insert(_error);
		prequire.insert(constraints.begin(), constraints.end());

		// traverse AIG backward
		for (AigVec::const_reverse_iterator i = aig.rbegin(); i != aig.rend();
				++i)
		{
			// skip if this row is not required
			if (require.find(i->lhs) == require.end()
					&& require.find(~i->lhs) == require.end())
				continue;
			// encode into CNF
			sslvImplgraph->addClause(~i->lhs, i->rhs0);
			sslvImplgraph->addClause(~i->lhs, i->rhs1);
			sslvImplgraph->addClause(~i->rhs0, ~i->rhs1, i->lhs);
			// require arguments
			require.insert(i->rhs0);
			require.insert(i->rhs1);
			// primed: skip if not required
			if (prequire.find(i->lhs) == prequire.end()
					&& prequire.find(~i->lhs) == prequire.end())
				continue;
			// encode PRIMED form into CNF
			MyMinisat::Lit r0 = primeLit(i->lhs, sslvImplgraph), r1 = primeLit(
					i->rhs0, sslvImplgraph), r2 = primeLit(i->rhs1,
					sslvImplgraph);
			sslvImplgraph->addClause(~r0, r1);
			sslvImplgraph->addClause(~r0, r2);
			sslvImplgraph->addClause(~r1, ~r2, r0);
			// require arguments
			prequire.insert(i->rhs0);
			prequire.insert(i->rhs1);
		}
		sslvImplgraph->addClause(btrue());
		sslvImplgraph->addClause(~_error);
		// assert literal for true

		// assert l' = f for each latch l
		for (VarVec::const_iterator i = beginLatches(); i != endLatches(); ++i)
		{
			MyMinisat::Lit platch = primeLit(i->lit(false)), f = nextStateFn(*i);
			sslvImplgraph->addClause(~platch, f);
			sslvImplgraph->addClause(~f, platch);
		}
		sslvImplgraph->eliminate(true);
	}
	// load the clauses from the simplified context
	assert((unsigned)sslvImplgraph->nVars() == vars.size());
	while (slv.nVars() < sslvImplgraph->nVars())
		slv.newVar();
	for (MyMinisat::ClauseIterator c = sslvImplgraph->clausesBegin();
			c != sslvImplgraph->clausesEnd(); ++c)
	{
		const MyMinisat::Clause &cls = *c;
		MyMinisat::vec<MyMinisat::Lit> cls_;
		for (int i = 0; i < cls.size(); ++i)
			cls_.push(cls[i]);
		slv.addClause_(cls_);
	}
	for (MyMinisat::TrailIterator c = sslvImplgraph->trailBegin();
			c != sslvImplgraph->trailEnd(); ++c)
		slv.addClause(*c);
}

void Model::loadTransitionRelationImplgraphSucc(MyMinisat::Solver &slv, bool respectProperty)
{
	if (!sslvImplgraphSucc)
	{
		// create a simplified CNF version of (this slice of) the TR
		sslvImplgraphSucc = new MyMinisat::SimpSolver();
		// introduce all variables to maintain alignment
		for (size_t i = 0; i < vars.size(); ++i)
		{
			MyMinisat::Var nv = sslvImplgraphSucc->newVar();
			assert(nv == vars[i].var());
		}
		// freeze inputs, latches, and special nodes (and primed forms)
		for (VarVec::const_iterator i = beginInputs(); i != endInputs(); ++i)
		{
			sslvImplgraphSucc->setFrozen(i->var(), true);
			sslvImplgraphSucc->setFrozen(primeVar(*i).var(), true);
		}
		for (VarVec::const_iterator i = beginLatches(); i != endLatches(); ++i)
		{
			sslvImplgraphSucc->setFrozen(i->var(), true);
			sslvImplgraphSucc->setFrozen(primeVar(*i).var(), true);
		}
		sslvImplgraphSucc->setFrozen(varOfLit(error()).var(), true);
		sslvImplgraphSucc->setFrozen(varOfLit(primedError()).var(), true);
		for (LitVec::const_iterator i = constraints.begin();
				i != constraints.end(); ++i)
		{
			Var v = varOfLit(*i);
			sslvImplgraphSucc->setFrozen(v.var(), true);
			sslvImplgraphSucc->setFrozen(primeVar(v).var(), true);
		}
		// initialize with roots of required formulas
		LitSet require;  // unprimed formulas
		for (VarVec::const_iterator i = beginLatches(); i != endLatches(); ++i)
			require.insert(nextStateFn(*i));
		if(respectProperty)
			require.insert(_error);
		require.insert(constraints.begin(), constraints.end());
		LitSet prequire; // for primed formulas; always subset of require
		//prequire.insert(_error);
		//prequire.insert(constraints.begin(), constraints.end());

		// traverse AIG backward
		for (AigVec::const_reverse_iterator i = aig.rbegin(); i != aig.rend();
				++i)
		{
			// skip if this row is not required
			if (require.find(i->lhs) == require.end()
					&& require.find(~i->lhs) == require.end())
				continue;
			// encode into CNF
			sslvImplgraphSucc->addClause(~i->lhs, i->rhs0);
			sslvImplgraphSucc->addClause(~i->lhs, i->rhs1);
			sslvImplgraphSucc->addClause(~i->rhs0, ~i->rhs1, i->lhs);
			// require arguments
			require.insert(i->rhs0);
			require.insert(i->rhs1);
			// primed: skip if not required
			if (prequire.find(i->lhs) == prequire.end()
					&& prequire.find(~i->lhs) == prequire.end())
				continue;
			// encode PRIMED form into CNF
			/*MyMinisat::Lit r0 = primeLit(i->lhs, sslvImplgraphSucc), r1 =
			 primeLit(i->rhs0, sslvImplgraphSucc), r2 = primeLit(i->rhs1,
			 sslvImplgraphSucc);
			 sslvImplgraphSucc->addClause(~r0, r1);
			 sslvImplgraphSucc->addClause(~r0, r2);
			 sslvImplgraphSucc->addClause(~r1, ~r2, r0);
			 // require arguments
			 prequire.insert(i->rhs0);
			 prequire.insert(i->rhs1);*/
		}
		sslvImplgraphSucc->addClause(btrue());
		//sslvImplgraphSucc->addClause(~_error);
		// assert literal for true

		// assert l' = f for each latch l
		for (VarVec::const_iterator i = beginLatches(); i != endLatches(); ++i)
		{
			MyMinisat::Lit platch = primeLit(i->lit(false)), f = nextStateFn(*i);
			sslvImplgraphSucc->addClause(~platch, f);
			sslvImplgraphSucc->addClause(~f, platch);
		}
		sslvImplgraphSucc->eliminate(true);
	}
	// load the clauses from the simplified context
	assert((unsigned)sslvImplgraphSucc->nVars() == vars.size());
	while (slv.nVars() < sslvImplgraphSucc->nVars())
		slv.newVar();
	for (MyMinisat::ClauseIterator c = sslvImplgraphSucc->clausesBegin();
			c != sslvImplgraphSucc->clausesEnd(); ++c)
	{
		const MyMinisat::Clause &cls = *c;
		MyMinisat::vec<MyMinisat::Lit> cls_;
		for (int i = 0; i < cls.size(); ++i)
			cls_.push(cls[i]);
		slv.addClause_(cls_);
	}
	for (MyMinisat::TrailIterator c = sslvImplgraphSucc->trailBegin();
			c != sslvImplgraphSucc->trailEnd(); ++c)
		slv.addClause(*c);
}

void Model::loadTransitionRelationImplgraphBad(MyMinisat::Solver &slv)
{
	if (!sslvImplgraphBad)
	{
		// create a simplified CNF version of (this slice of) the TR
		sslvImplgraphBad = new MyMinisat::SimpSolver();
		// introduce all variables to maintain alignment
		for (size_t i = 0; i < vars.size(); ++i)
		{
			MyMinisat::Var nv = sslvImplgraphBad->newVar();
			assert(nv == vars[i].var());
		}
		// freeze inputs, latches, and special nodes (and primed forms)
		for (VarVec::const_iterator i = beginInputs(); i != endInputs(); ++i)
		{
			sslvImplgraphBad->setFrozen(i->var(), true);
			sslvImplgraphBad->setFrozen(primeVar(*i).var(), true);
		}
		for (VarVec::const_iterator i = beginLatches(); i != endLatches(); ++i)
		{
			sslvImplgraphBad->setFrozen(i->var(), true);
			sslvImplgraphBad->setFrozen(primeVar(*i).var(), true);
		}
		sslvImplgraphBad->setFrozen(varOfLit(error()).var(), true);
		sslvImplgraphBad->setFrozen(varOfLit(primedError()).var(), true);
		for (LitVec::const_iterator i = constraints.begin();
				i != constraints.end(); ++i)
		{
			Var v = varOfLit(*i);
			sslvImplgraphBad->setFrozen(v.var(), true);
			sslvImplgraphBad->setFrozen(primeVar(v).var(), true);
		}
		// initialize with roots of required formulas
		LitSet require;  // unprimed formulas
		//for (VarVec::const_iterator i = beginLatches(); i != endLatches(); ++i)
		//	require.insert(nextStateFn(*i));
		require.insert(_error);
		require.insert(constraints.begin(), constraints.end());
		LitSet prequire; // for primed formulas; always subset of require
		prequire.insert(_error);
		prequire.insert(constraints.begin(), constraints.end());

		LitSet reqNextStates;
		// traverse AIG backward
		if (MyMinisat::var(_error) >= beginLatches()->var()
				&& MyMinisat::var(_error) < endLatches()->var())
			reqNextStates.insert(nextStateFn(varOfLit(_error)));
		for (AigVec::const_reverse_iterator i = aig.rbegin(); i != aig.rend();
				++i)
		{
			// skip if this row is not required
			if (require.find(i->lhs) == require.end()
					&& require.find(~i->lhs) == require.end())
				continue;
			if (MyMinisat::var(i->rhs0) >= beginLatches()->var()
					&& MyMinisat::var(i->rhs0) < endLatches()->var())
				reqNextStates.insert(nextStateFn(varOfLit(i->rhs0)));
			if (MyMinisat::var(i->rhs1) >= beginLatches()->var()
					&& MyMinisat::var(i->rhs1) < endLatches()->var())
				reqNextStates.insert(nextStateFn(varOfLit(i->rhs1)));
			require.insert(i->rhs0);
			require.insert(i->rhs1);
		}
		require.clear();
		for (auto &lit : reqNextStates)
			require.insert(lit);
		require.insert(_error);
		require.insert(constraints.begin(), constraints.end());

		// traverse AIG backward
		for (AigVec::const_reverse_iterator i = aig.rbegin(); i != aig.rend();
				++i)
		{
			// skip if this row is not required
			if (require.find(i->lhs) == require.end()
					&& require.find(~i->lhs) == require.end())
				continue;
			// encode into CNF
			sslvImplgraphBad->addClause(~i->lhs, i->rhs0);
			sslvImplgraphBad->addClause(~i->lhs, i->rhs1);
			sslvImplgraphBad->addClause(~i->rhs0, ~i->rhs1, i->lhs);
			// require arguments
			require.insert(i->rhs0);
			require.insert(i->rhs1);
			// primed: skip if not required
			if (prequire.find(i->lhs) == prequire.end()
					&& prequire.find(~i->lhs) == prequire.end())
				continue;
			// encode PRIMED form into CNF
			MyMinisat::Lit r0 = primeLit(i->lhs, sslvImplgraphBad), r1 = primeLit(
					i->rhs0, sslvImplgraphBad), r2 = primeLit(i->rhs1,
					sslvImplgraphBad);
			sslvImplgraphBad->addClause(~r0, r1);
			sslvImplgraphBad->addClause(~r0, r2);
			sslvImplgraphBad->addClause(~r1, ~r2, r0);
			// require arguments
			prequire.insert(i->rhs0);
			prequire.insert(i->rhs1);
		}
		sslvImplgraphBad->addClause(btrue());
		//sslvImplgraphBad->addClause(~_error);
		// assert literal for true

		// assert l' = f for each latch l
		for (VarVec::const_iterator i = beginLatches(); i != endLatches(); ++i)
		{
			MyMinisat::Lit platch = primeLit(i->lit(false)), f = nextStateFn(*i);
			sslvImplgraphBad->addClause(~platch, f);
			sslvImplgraphBad->addClause(~f, platch);
		}
		sslvImplgraphBad->eliminate(true);
	}
	// load the clauses from the simplified context
	assert((unsigned)sslvImplgraphBad->nVars() == vars.size());
	while (slv.nVars() < sslvImplgraphBad->nVars())
		slv.newVar();
	for (MyMinisat::ClauseIterator c = sslvImplgraphBad->clausesBegin();
			c != sslvImplgraphBad->clausesEnd(); ++c)
	{
		const MyMinisat::Clause &cls = *c;
		MyMinisat::vec<MyMinisat::Lit> cls_;
		for (int i = 0; i < cls.size(); ++i)
			cls_.push(cls[i]);
		slv.addClause_(cls_);
	}
	for (MyMinisat::TrailIterator c = sslvImplgraphBad->trailBegin();
			c != sslvImplgraphBad->trailEnd(); ++c)
		slv.addClause(*c);
}

void Model::loadTransitionRelationLifting(MyMinisat::Solver &slv)
{
	if (!sslvLifting)
	{
		// create a simplified CNF version of (this slice of) the TR
		sslvLifting = new MyMinisat::SimpSolver();
		// introduce all variables to maintain alignment
		for (size_t i = 0; i < vars.size(); ++i)
		{
			MyMinisat::Var nv = sslvLifting->newVar();
			assert(nv == vars[i].var());
		}
		// freeze inputs, latches, and special nodes (and primed forms)
		for (VarVec::const_iterator i = beginInputs(); i != endInputs(); ++i)
		{
			sslvLifting->setFrozen(i->var(), true);
			sslvLifting->setFrozen(primeVar(*i).var(), true);
		}
		for (VarVec::const_iterator i = beginLatches(); i != endLatches(); ++i)
		{
			sslvLifting->setFrozen(i->var(), true);
			sslvLifting->setFrozen(primeVar(*i).var(), true);
		}
		sslvLifting->setFrozen(varOfLit(error()).var(), true);
		sslvLifting->setFrozen(varOfLit(primedError()).var(), true);
		for (LitVec::const_iterator i = constraints.begin();
				i != constraints.end(); ++i)
		{
			Var v = varOfLit(*i);
			sslvLifting->setFrozen(v.var(), true);
			sslvLifting->setFrozen(primeVar(v).var(), true);
		}
		// initialize with roots of required formulas
		LitSet require;  // unprimed formulas
		for (VarVec::const_iterator i = beginLatches(); i != endLatches(); ++i)
			require.insert(nextStateFn(*i));
		require.insert(_error);
		require.insert(constraints.begin(), constraints.end());
		LitSet prequire; // for primed formulas; always subset of require
		prequire.insert(_error);
		prequire.insert(constraints.begin(), constraints.end());

		// traverse AIG backward
		for (AigVec::const_reverse_iterator i = aig.rbegin(); i != aig.rend();
				++i)
		{
			// skip if this row is not required
			if (require.find(i->lhs) == require.end()
					&& require.find(~i->lhs) == require.end())
				continue;
			// encode into CNF
			sslvLifting->addClause(~i->lhs, i->rhs0);
			sslvLifting->addClause(~i->lhs, i->rhs1);
			sslvLifting->addClause(~i->rhs0, ~i->rhs1, i->lhs);
			// require arguments
			require.insert(i->rhs0);
			require.insert(i->rhs1);
			// primed: skip if not required
			if (prequire.find(i->lhs) == prequire.end()
					&& prequire.find(~i->lhs) == prequire.end())
				continue;
			// encode PRIMED form into CNF
			MyMinisat::Lit r0 = primeLit(i->lhs, sslvLifting), r1 = primeLit(
					i->rhs0, sslvLifting), r2 = primeLit(i->rhs1, sslvLifting);
			sslvLifting->addClause(~r0, r1);
			sslvLifting->addClause(~r0, r2);
			sslvLifting->addClause(~r1, ~r2, r0);
			// require arguments
			prequire.insert(i->rhs0);
			prequire.insert(i->rhs1);
		}
		sslvLifting->addClause(btrue());
		sslvLifting->addClause(~_error);
		// assert literal for true

		// assert l' = f for each latch l
		for (VarVec::const_iterator i = beginLatches(); i != endLatches(); ++i)
		{
			MyMinisat::Lit platch = primeLit(i->lit(false)), f = nextStateFn(*i);
			sslvLifting->addClause(~platch, f);
			sslvLifting->addClause(~f, platch);
		}
		sslvLifting->eliminate(true);
	}
	// load the clauses from the simplified context
	assert((unsigned)sslvLifting->nVars() == vars.size());
	while (slv.nVars() < sslvLifting->nVars())
		slv.newVar();
	for (MyMinisat::ClauseIterator c = sslvLifting->clausesBegin();
			c != sslvLifting->clausesEnd(); ++c)
	{
		const MyMinisat::Clause &cls = *c;
		MyMinisat::vec<MyMinisat::Lit> cls_;
		for (int i = 0; i < cls.size(); ++i)
			cls_.push(cls[i]);
		slv.addClause_(cls_);
	}
	for (MyMinisat::TrailIterator c = sslvLifting->trailBegin();
			c != sslvLifting->trailEnd(); ++c)
		slv.addClause(*c);
}

bool Model::checkConstraintsForInputFanin()
{
	LitSet require;
	require.insert(constraints.begin(), constraints.end());
	for (AigVec::const_reverse_iterator i = aig.rbegin(); i != aig.rend(); ++i)
	{
		// skip if this row is not required
		if (require.find(i->lhs) == require.end()
				&& require.find(~i->lhs) == require.end())
			continue;

		if ((MyMinisat::var(i->rhs0) < endInputs()->var()
				&& MyMinisat::var(i->rhs0) >= beginInputs()->var())
				|| (MyMinisat::var(i->rhs1) < endInputs()->var()
						&& MyMinisat::var(i->rhs1) >= beginInputs()->var()))
			return true;

		// require arguments
		require.insert(i->rhs0);
		require.insert(i->rhs1);

	}
	return false;
}

void Model::loadTransitionRelationDepQBF(QDPLL *slv, bool primeConstraints)
{
	/*
	 * sslv must exist
	 * <=> transition relation was loaded at least once
	 * <=> vars vector is already enlarged by primed inputs/latches and aux variables
	 */
	assert(sslv);

	assert((unsigned)sslv->nVars() == vars.size());
	if (!sslvQBF)
	{
		// create a simplified CNF version of (this slice of) the TR
		sslvQBF = new MyMinisat::SimpSolver();
		// introduce all variables to maintain alignment
		for (size_t i = 0; i < vars.size(); ++i)
		{
			MyMinisat::Var nv = sslvQBF->newVar();
			assert(nv == vars[i].var());
		}
		// freeze inputs, latches, and special nodes (and primed forms)
		for (VarVec::const_iterator i = beginInputs(); i != endInputs(); ++i)
		{
			sslvQBF->setFrozen(i->var(), true);
			sslvQBF->setFrozen(primeVar(*i).var(), true);
		}
		for (VarVec::const_iterator i = beginLatches(); i != endLatches(); ++i)
		{
			sslvQBF->setFrozen(i->var(), true);
			sslvQBF->setFrozen(primeVar(*i).var(), true);
		}
		sslvQBF->setFrozen(varOfLit(error()).var(), true);
		sslvQBF->setFrozen(varOfLit(primedError()).var(), true);
		for (LitVec::const_iterator i = constraints.begin();
				i != constraints.end(); ++i)
		{
			Var v = varOfLit(*i);
			sslvQBF->setFrozen(v.var(), true);
			sslvQBF->setFrozen(primeVar(v).var(), true);
		}
		// initialize with roots of required formulas
		LitSet require;  // unprimed formulas
		for (VarVec::const_iterator i = beginLatches(); i != endLatches(); ++i)
			require.insert(nextStateFn(*i));
		require.insert(_error);
		require.insert(constraints.begin(), constraints.end());
		LitSet prequire; // for primed formulas; always subset of require
		prequire.insert(_error);
		prequire.insert(constraints.begin(), constraints.end());

		// traverse AIG backward
		for (AigVec::const_reverse_iterator i = aig.rbegin(); i != aig.rend();
				++i)
		{
			// skip if this row is not required
			if (require.find(i->lhs) == require.end()
					&& require.find(~i->lhs) == require.end())
				continue;
			// encode into CNF
			sslvQBF->addClause(~i->lhs, i->rhs0);
			sslvQBF->addClause(~i->lhs, i->rhs1);
			sslvQBF->addClause(~i->rhs0, ~i->rhs1, i->lhs);
			// require arguments
			require.insert(i->rhs0);
			require.insert(i->rhs1);
			// primed: skip if not required
			if (prequire.find(i->lhs) == prequire.end()
					&& prequire.find(~i->lhs) == prequire.end())
				continue;
			// encode PRIMED form into CNF
			MyMinisat::Lit r0 = primeLit(i->lhs, sslvQBF), r1 = primeLit(i->rhs0,
					sslvQBF), r2 = primeLit(i->rhs1, sslvQBF);
			sslvQBF->addClause(~r0, r1);
			sslvQBF->addClause(~r0, r2);
			sslvQBF->addClause(~r1, ~r2, r0);
			// require arguments
			prequire.insert(i->rhs0);
			prequire.insert(i->rhs1);
		}
		sslvQBF->addClause(btrue());
		//sslvQBF->addClause(~_error);
		// assert literal for true

		// assert l' = f for each latch l
		for (VarVec::const_iterator i = beginLatches(); i != endLatches(); ++i)
		{
			MyMinisat::Lit platch = primeLit(i->lit(false)), f = nextStateFn(*i);
			sslvQBF->addClause(~platch, f);
			sslvQBF->addClause(~f, platch);
		}
		sslvQBF->eliminate(true);
	}

	qdpll_push(slv);

	/* load the clauses from the simplified context */
	for (MyMinisat::ClauseIterator c = sslvQBF->clausesBegin();
			c != sslvQBF->clausesEnd(); ++c)
	{
		const MyMinisat::Clause &cls = *c;
		MyMinisat::vec<MyMinisat::Lit> cls_;
		for (int i = 0; i < cls.size(); ++i)
		{
			assert(cls[i].x > 1);
			qdpll_add(slv,
					MyMinisat::var(cls[i])
							* ((int) pow(-1, MyMinisat::sign(cls[i]))));
		}
		qdpll_add(slv, 0);
	}

	for (MyMinisat::TrailIterator c = sslvQBF->trailBegin();
			c != sslvQBF->trailEnd(); ++c)
	{
		if ((*c).x > 1)
		{
			qdpll_add(slv,
					MyMinisat::var(*c) * ((int) pow(-1, MyMinisat::sign(*c))));
			qdpll_add(slv, 0);
		}
	}
}

void Model::loadTransitionRelationQuantom(quantom::Quantom &qSlv)
{
	/*
	 * sslv must exist
	 * <=> transition relation was loaded at least once
	 * <=> vars vector is already enlarged by primed inputs/latches and aux variables
	 */
	assert(sslv);

	assert((unsigned)sslv->nVars() == vars.size());
	if (!sslvQBF)
	{
		// create a simplified CNF version of (this slice of) the TR
		sslvQBF = new MyMinisat::SimpSolver();
		// introduce all variables to maintain alignment
		for (size_t i = 0; i < vars.size(); ++i)
		{
			MyMinisat::Var nv = sslvQBF->newVar();
			assert(nv == vars[i].var());
		}
		// freeze inputs, latches, and special nodes (and primed forms)
		for (VarVec::const_iterator i = beginInputs(); i != endInputs(); ++i)
		{
			sslvQBF->setFrozen(i->var(), true);
			sslvQBF->setFrozen(primeVar(*i).var(), true);
		}
		for (VarVec::const_iterator i = beginLatches(); i != endLatches(); ++i)
		{
			sslvQBF->setFrozen(i->var(), true);
			sslvQBF->setFrozen(primeVar(*i).var(), true);
		}
		sslvQBF->setFrozen(varOfLit(error()).var(), true);
		sslvQBF->setFrozen(varOfLit(primedError()).var(), true);
		for (LitVec::const_iterator i = constraints.begin();
				i != constraints.end(); ++i)
		{
			Var v = varOfLit(*i);
			sslvQBF->setFrozen(v.var(), true);
			sslvQBF->setFrozen(primeVar(v).var(), true);
		}
		// initialize with roots of required formulas
		LitSet require;  // unprimed formulas
		for (VarVec::const_iterator i = beginLatches(); i != endLatches(); ++i)
			require.insert(nextStateFn(*i));
		require.insert(_error);
		require.insert(constraints.begin(), constraints.end());
		LitSet prequire; // for primed formulas; always subset of require
		prequire.insert(_error);
		prequire.insert(constraints.begin(), constraints.end());

		// traverse AIG backward
		for (AigVec::const_reverse_iterator i = aig.rbegin(); i != aig.rend();
				++i)
		{
			// skip if this row is not required
			if (require.find(i->lhs) == require.end()
					&& require.find(~i->lhs) == require.end())
				continue;
			// encode into CNF
			sslvQBF->addClause(~i->lhs, i->rhs0);
			sslvQBF->addClause(~i->lhs, i->rhs1);
			sslvQBF->addClause(~i->rhs0, ~i->rhs1, i->lhs);
			// require arguments
			require.insert(i->rhs0);
			require.insert(i->rhs1);
			// primed: skip if not required
			if (prequire.find(i->lhs) == prequire.end()
					&& prequire.find(~i->lhs) == prequire.end())
				continue;
			// encode PRIMED form into CNF
			MyMinisat::Lit r0 = primeLit(i->lhs, sslvQBF), r1 = primeLit(i->rhs0,
					sslvQBF), r2 = primeLit(i->rhs1, sslvQBF);
			sslvQBF->addClause(~r0, r1);
			sslvQBF->addClause(~r0, r2);
			sslvQBF->addClause(~r1, ~r2, r0);
			// require arguments
			prequire.insert(i->rhs0);
			prequire.insert(i->rhs1);
		}
		sslvQBF->addClause(btrue());
		//sslvQBF->addClause(~_error);
		// assert literal for true

		// assert l' = f for each latch l
		for (VarVec::const_iterator i = beginLatches(); i != endLatches(); ++i)
		{
			MyMinisat::Lit platch = primeLit(i->lit(false)), f = nextStateFn(*i);
			sslvQBF->addClause(~platch, f);
			sslvQBF->addClause(~f, platch);
		}
		sslvQBF->eliminate(true);
	}

	for (MyMinisat::ClauseIterator c = sslvQBF->clausesBegin();
			c != sslvQBF->clausesEnd(); ++c)
	{
		const MyMinisat::Clause &cls = *c;
		assert(cls.size() > 0);
		std::vector<unsigned int> quantomClause;
		for (int i = 0; i < cls.size(); ++i)
		{
			assert(cls[i].x > 0);

			assert(MyMinisat::var(cls[i]) > 0);
			quantomClause.push_back(cls[i].x);
		}
		assert(qSlv.addClause(quantomClause));
	}
	//for (MyMinisat::TrailIterator c = fr.consecution->trailBegin();
	//		c != fr.consecution->trailEnd(); ++c)
	for (MyMinisat::TrailIterator c = sslvQBF->trailBegin();
			c != sslvQBF->trailEnd(); ++c)
	{
		if ((*c).x <= 1)
		{
			continue;
		}
		assert(qSlv.addUnit((*c).x));
	}
}

void Model::loadTransitionRelationILP()
{
	/*
	 * sslv must exist
	 * <=> transition relation was loaded at least once
	 * <=> vars vector is already enlarged by primed inputs/latches and aux variables
	 */
	assert(sslv);

	assert((unsigned)sslv->nVars() == vars.size());
	if (!sslvILP)
	{
		// create a simplified CNF version of (this slice of) the TR
		sslvILP = new MyMinisat::SimpSolver();
		// introduce all variables to maintain alignment
		for (size_t i = 0; i < vars.size(); ++i)
		{
			MyMinisat::Var nv = sslvILP->newVar();
			assert(nv == vars[i].var());
		}
		// freeze inputs, latches, and special nodes (and primed forms)
		for (VarVec::const_iterator i = beginInputs(); i != endInputs(); ++i)
		{
			sslvILP->setFrozen(i->var(), true);
			sslvILP->setFrozen(primeVar(*i).var(), true);
		}
		for (VarVec::const_iterator i = beginLatches(); i != endLatches(); ++i)
		{
			sslvILP->setFrozen(i->var(), true);
			sslvILP->setFrozen(primeVar(*i).var(), true);
		}
		sslvILP->setFrozen(varOfLit(error()).var(), true);
		sslvILP->setFrozen(varOfLit(primedError()).var(), true);
		for (LitVec::const_iterator i = constraints.begin();
				i != constraints.end(); ++i)
		{
			Var v = varOfLit(*i);
			sslvILP->setFrozen(v.var(), true);
			sslvILP->setFrozen(primeVar(v).var(), true);
		}
		// initialize with roots of required formulas
		LitSet require;  // unprimed formulas
		for (VarVec::const_iterator i = beginLatches(); i != endLatches(); ++i)
			require.insert(nextStateFn(*i));
		require.insert(_error);
		require.insert(constraints.begin(), constraints.end());
		LitSet prequire; // for primed formulas; always subset of require
		prequire.insert(_error);
		prequire.insert(constraints.begin(), constraints.end());

		// traverse AIG backward
		for (AigVec::const_reverse_iterator i = aig.rbegin(); i != aig.rend();
				++i)
		{
			// skip if this row is not required
			if (require.find(i->lhs) == require.end()
					&& require.find(~i->lhs) == require.end())
				continue;
			// encode into CNF
			sslvILP->addClause(~i->lhs, i->rhs0);
			sslvILP->addClause(~i->lhs, i->rhs1);
			sslvILP->addClause(~i->rhs0, ~i->rhs1, i->lhs);
			// require arguments
			require.insert(i->rhs0);
			require.insert(i->rhs1);
			// primed: skip if not required
			if (prequire.find(i->lhs) == prequire.end()
					&& prequire.find(~i->lhs) == prequire.end())
				continue;
			// encode PRIMED form into CNF
			MyMinisat::Lit r0 = primeLit(i->lhs, sslvILP), r1 = primeLit(i->rhs0,
					sslvILP), r2 = primeLit(i->rhs1, sslvILP);
			sslvILP->addClause(~r0, r1);
			sslvILP->addClause(~r0, r2);
			sslvILP->addClause(~r1, ~r2, r0);
			// require arguments
			prequire.insert(i->rhs0);
			prequire.insert(i->rhs1);
		}
		sslvILP->addClause(btrue());
		// assert literal for true

		// assert l' = f for each latch l
		for (VarVec::const_iterator i = beginLatches(); i != endLatches(); ++i)
		{
			MyMinisat::Lit platch = primeLit(i->lit(false)), f = nextStateFn(*i);
			sslvILP->addClause(~platch, f);
			sslvILP->addClause(~f, platch);
		}
		sslvILP->eliminate(true);
	}

}

void Model::addCubeToDepQBF(QDPLL &slv, const LitVec &c)
{
#if 0
	/*
	 (1,0) => 1
	 (0,1) => 0

	 AND ( (1,0) , (1,0) ) = (1,0)
	 AND ( (0,1) , (1,0) ) = (0,1)
	 AND ( (1,0) , (0,1) ) = (0,1)
	 AND ( (0,1) , (0,1) ) = (0,1)
	 */
	std::vector<uint32_t> antomClAnd;
	for (auto &lit : c)
	{
		//negation: 1 -> 0, 0 -> 1. Aux lit the opposite. No X possible.
		MyMinisat::Lit negLit = ~lit;
		antomClAnd.push_back(negLit.x);
		/*if(sign(lit))
		 antomClAnd.push_back(2*get01XAuxVar(var(lit))+1);
		 else
		 antomClAnd.push_back(2*get01XAuxVar(var(lit)));*/

		antomClAnd.push_back(2 * get01XAuxVar(var(lit)) + sign(lit));

	}
	assert(antomClAnd.size() == 2 * c.size());

	//add clause to antom-instance
	slv.AddClause(antomClAnd);
#endif
}

void Model::add01XAndPacose(Pacose &slv, MyMinisat::Lit lhs, MyMinisat::Lit rhs0,
		MyMinisat::Lit rhs1)
{
	/*
	 (1,0) => 1
	 (0,1) => 0

	 AND ( (1,0) , (1,0) ) = (1,0)
	 AND ( (0,1) , (1,0) ) = (0,1)
	 AND ( (1,0) , (0,1) ) = (0,1)
	 AND ( (0,1) , (0,1) ) = (0,1)
	 */
	MyMinisat::Lit orig_rhs1 = rhs1;
	MyMinisat::Lit orig_rhs0 = rhs0;

	MyMinisat::Lit lhs_1 = MyMinisat::mkLit(get01XAuxVar(MyMinisat::var(lhs)), false); //MyMinisat::sign(lhs));
	MyMinisat::Lit rhs0_1 = MyMinisat::mkLit(get01XAuxVar(MyMinisat::var(rhs0)),
			false); //,MyMinisat::sign(rhs0));
	MyMinisat::Lit rhs1_1 = MyMinisat::mkLit(get01XAuxVar(MyMinisat::var(rhs1)),
			false); //,MyMinisat::sign(rhs1));

	if (MyMinisat::sign(rhs0)) //negation: switch 0 and 1 bits
	{
		MyMinisat::Lit bufLit = MyMinisat::mkLit(MyMinisat::var(rhs0), false); //positive lit
		rhs0 = rhs0_1;
		rhs0_1 = bufLit;
	}
	if (MyMinisat::sign(rhs1)) //negation: switch 0 and 1 bits
	{
		MyMinisat::Lit bufLit = MyMinisat::mkLit(MyMinisat::var(rhs1), false); //positive lit
		rhs1 = rhs1_1;
		rhs1_1 = bufLit;
	}

	// lhs_0 <=> rhs0_0 & rhs1_0
	// special cases for constant functions
	std::vector<uint32_t> cl;
	if (orig_rhs0.x != 0 && orig_rhs1.x != 0) //would be trivially satisfied otherwise
	{
		if (orig_rhs0.x > 1)
			cl.push_back((~rhs0).x);
		if (orig_rhs1.x > 1)
			cl.push_back((~rhs1).x);
		cl.push_back(lhs.x);
		//slv._satSolver->AddClause(cl);
		slv.AddClause(cl);

		cl.clear();
		if (orig_rhs0.x > 1)
			cl.push_back(rhs0_1.x);
		if (orig_rhs1.x > 1)
			cl.push_back(rhs1_1.x);
		cl.push_back((~lhs_1).x);
		//slv._satSolver->AddClause(cl);
		slv.AddClause(cl);
	}

	if (orig_rhs0.x != 1) //would be trivially satisfied otherwise
	{
		cl.clear();
		if (orig_rhs0.x > 0)
			cl.push_back(rhs0.x);
		cl.push_back((~lhs).x);
		//slv._satSolver->AddClause(cl);
		slv.AddClause(cl);

		cl.clear();
		cl.push_back(lhs_1.x);
		if (orig_rhs0.x > 0)
			cl.push_back((~rhs0_1).x);
		//slv._satSolver->AddClause(cl);
		slv.AddClause(cl);
	}

	if (orig_rhs1.x != 1) //would be trivially satisfied otherwise
	{
		cl.clear();
		if (orig_rhs1.x > 0)
			cl.push_back(rhs1.x);
		cl.push_back((~lhs).x);
		//slv._satSolver->AddClause(cl);
		slv.AddClause(cl);

		cl.clear();
		cl.push_back(lhs_1.x);
		if (orig_rhs1.x > 0)
			cl.push_back((~rhs1_1).x);
		//slv._satSolver->AddClause(cl);
		slv.AddClause(cl);
	}
	//std::cout << rhs0.x << " * " << rhs1.x << " = " << lhs.x << std::endl;

	/*
	 * auxiliary variables for 01X encoding
	 */
	// lhs_1 <=> rhs0_1 | rhs1_1
	/*MyMinisat::Lit lhs_1 = MyMinisat::mkLit(get01XAuxVar(MyMinisat::var(lhs)),MyMinisat::sign(lhs));
	 MyMinisat::Lit rhs0_1 = MyMinisat::mkLit(get01XAuxVar(MyMinisat::var(rhs0)),MyMinisat::sign(rhs0));
	 MyMinisat::Lit rhs1_1 = MyMinisat::mkLit(get01XAuxVar(MyMinisat::var(rhs1)),MyMinisat::sign(rhs1));*/

	//std::cout << rhs0_1.x << " + " << rhs1_1.x << " = " << lhs_1.x << std::endl;
}

void Model::add01XAnd(MyMinisat::Solver &slv, MyMinisat::Lit lhs, MyMinisat::Lit rhs0,
		MyMinisat::Lit rhs1)
{
	/*
	 (1,0) => 1
	 (0,1) => 0

	 AND ( (1,0) , (1,0) ) = (1,0)
	 AND ( (0,1) , (1,0) ) = (0,1)
	 AND ( (1,0) , (0,1) ) = (0,1)
	 AND ( (0,1) , (0,1) ) = (0,1)
	 */
	MyMinisat::Lit orig_rhs1 = rhs1;
	MyMinisat::Lit orig_rhs0 = rhs0;

	MyMinisat::Lit lhs_1 = MyMinisat::mkLit(get01XAuxVar(MyMinisat::var(lhs)), false); //MyMinisat::sign(lhs));
	MyMinisat::Lit rhs0_1 = MyMinisat::mkLit(get01XAuxVar(MyMinisat::var(rhs0)),
			false); //,MyMinisat::sign(rhs0));
	MyMinisat::Lit rhs1_1 = MyMinisat::mkLit(get01XAuxVar(MyMinisat::var(rhs1)),
			false); //,MyMinisat::sign(rhs1));

	if (MyMinisat::sign(rhs0)) //negation: switch 0 and 1 bits
	{
		MyMinisat::Lit bufLit = MyMinisat::mkLit(MyMinisat::var(rhs0), false); //positive lit
		rhs0 = rhs0_1;
		rhs0_1 = bufLit;
	}
	if (MyMinisat::sign(rhs1)) //negation: switch 0 and 1 bits
	{
		MyMinisat::Lit bufLit = MyMinisat::mkLit(MyMinisat::var(rhs1), false); //positive lit
		rhs1 = rhs1_1;
		rhs1_1 = bufLit;
	}

	MSLitVec cl;
	if (orig_rhs0.x != 0 && orig_rhs1.x != 0) //would be trivially satisfied otherwise
	{
		if (orig_rhs0.x > 1)
			cl.push((~rhs0));
		if (orig_rhs1.x > 1)
			cl.push((~rhs1));
		cl.push(lhs);
		slv.addClause(cl);

		cl.clear();
		if (orig_rhs0.x > 1)
			cl.push(rhs0_1);
		if (orig_rhs1.x > 1)
			cl.push(rhs1_1);
		cl.push((~lhs_1));
		slv.addClause(cl);
	}

	if (orig_rhs0.x != 1) //would be trivially satisfied otherwise
	{
		cl.clear();
		if (orig_rhs0.x > 0)
			cl.push(rhs0);
		cl.push((~lhs));
		slv.addClause(cl);

		cl.clear();
		cl.push(lhs_1);
		if (orig_rhs0.x > 0)
			cl.push((~rhs0_1));
		slv.addClause(cl);
	}

	if (orig_rhs1.x != 1) //would be trivially satisfied otherwise
	{
		cl.clear();
		if (orig_rhs1.x > 0)
			cl.push(rhs1);
		cl.push((~lhs));
		slv.addClause(cl);

		cl.clear();
		cl.push(lhs_1);
		if (orig_rhs1.x > 0)
			cl.push((~rhs1_1));
		slv.addClause(cl);
	}
}

void Model::add01XAndPacoseElim(MyMinisat::SimpSolver &slv, MyMinisat::Lit lhs,
		MyMinisat::Lit rhs0, MyMinisat::Lit rhs1)
{
	/*
	 (1,0) => 1
	 (0,1) => 0

	 AND ( (1,0) , (1,0) ) = (1,0)
	 AND ( (0,1) , (1,0) ) = (0,1)
	 AND ( (1,0) , (0,1) ) = (0,1)
	 AND ( (0,1) , (0,1) ) = (0,1)
	 */
	MyMinisat::Lit orig_rhs1 = rhs1;
	MyMinisat::Lit orig_rhs0 = rhs0;

	MyMinisat::Lit lhs_1 = MyMinisat::mkLit(get01XAuxVar(MyMinisat::var(lhs)), false); //MyMinisat::sign(lhs));
	MyMinisat::Lit rhs0_1 = MyMinisat::mkLit(get01XAuxVar(MyMinisat::var(rhs0)),
			false); //,MyMinisat::sign(rhs0));
	MyMinisat::Lit rhs1_1 = MyMinisat::mkLit(get01XAuxVar(MyMinisat::var(rhs1)),
			false); //,MyMinisat::sign(rhs1));

	if (MyMinisat::sign(rhs0)) //negation: switch 0 and 1 bits
	{
		MyMinisat::Lit bufLit = MyMinisat::mkLit(MyMinisat::var(rhs0), false); //positive lit
		rhs0 = rhs0_1;
		rhs0_1 = bufLit;
	}
	if (MyMinisat::sign(rhs1)) //negation: switch 0 and 1 bits
	{
		MyMinisat::Lit bufLit = MyMinisat::mkLit(MyMinisat::var(rhs1), false); //positive lit
		rhs1 = rhs1_1;
		rhs1_1 = bufLit;
	}

	/*slv.addClause(~lhs, rhs0);
	 slv.addClause(~lhs, rhs1);
	 slv.addClause(~rhs0, ~rhs1, lhs);

	 slv.addClause(lhs_1, ~rhs0_1);
	 slv.addClause(lhs_1, ~rhs1_1);
	 slv.addClause(rhs0_1, rhs1_1, ~lhs_1);*/

	// lhs_0 <=> rhs0_0 & rhs1_0, lhs_1 <=> rhs0_1 | rhs1_1
	/*MyMinisat::vec<MyMinisat::Lit> cl;
	 cl.push((~rhs0));
	 cl.push((~rhs1));
	 cl.push(lhs);
	 slv.addClause(cl);

	 cl.clear();
	 cl.push(rhs0_1);
	 cl.push(rhs1_1);
	 cl.push((~lhs_1));
	 slv.addClause(cl);


	 cl.clear();
	 cl.push(rhs0);
	 cl.push((~lhs));
	 slv.addClause(cl);


	 cl.clear();
	 cl.push(lhs_1);
	 cl.push((~rhs0_1));
	 slv.addClause(cl);


	 cl.clear();
	 cl.push(rhs1);
	 cl.push((~lhs));
	 slv.addClause(cl);

	 cl.clear();
	 cl.push(lhs_1);
	 cl.push((~rhs1_1));
	 slv.addClause(cl);*/

	// lhs_0 <=> rhs0_0 & rhs1_0
	// special cases for constant functions
	MyMinisat::vec<MyMinisat::Lit> cl;
	if (orig_rhs0.x != 0 && orig_rhs1.x != 0) //would be trivially satisfied otherwise
	{
		if (orig_rhs0.x > 1)
			cl.push((~rhs0));
		if (orig_rhs1.x > 1)
			cl.push((~rhs1));
		cl.push(lhs);
		slv.addClause(cl);

		cl.clear();
		if (orig_rhs0.x > 1)
			cl.push(rhs0_1);
		if (orig_rhs1.x > 1)
			cl.push(rhs1_1);
		cl.push((~lhs_1));
		slv.addClause(cl);
	}

	if (orig_rhs0.x != 1) //would be trivially satisfied otherwise
	{
		cl.clear();
		if (orig_rhs0.x > 0)
			cl.push(rhs0);
		cl.push((~lhs));
		slv.addClause(cl);

		cl.clear();
		cl.push(lhs_1);
		if (orig_rhs0.x > 0)
			cl.push((~rhs0_1));
		slv.addClause(cl);
	}

	if (orig_rhs1.x != 1) //would be trivially satisfied otherwise
	{
		cl.clear();
		if (orig_rhs1.x > 0)
			cl.push(rhs1);
		cl.push((~lhs));
		slv.addClause(cl);

		cl.clear();
		cl.push(lhs_1);
		if (orig_rhs1.x > 0)
			cl.push((~rhs1_1));
		slv.addClause(cl);
	}
	//std::cout << rhs0.x << " * " << rhs1.x << " = " << lhs.x << std::endl;

	/*
	 * auxiliary variables for 01X encoding
	 */
	// lhs_1 <=> rhs0_1 | rhs1_1
	/*MyMinisat::Lit lhs_1 = MyMinisat::mkLit(get01XAuxVar(MyMinisat::var(lhs)),MyMinisat::sign(lhs));
	 MyMinisat::Lit rhs0_1 = MyMinisat::mkLit(get01XAuxVar(MyMinisat::var(rhs0)),MyMinisat::sign(rhs0));
	 MyMinisat::Lit rhs1_1 = MyMinisat::mkLit(get01XAuxVar(MyMinisat::var(rhs1)),MyMinisat::sign(rhs1));*/

	//std::cout << rhs0_1.x << " + " << rhs1_1.x << " = " << lhs_1.x << std::endl;
}

void Model::add01XEquivPacose(Pacose &slv, MyMinisat::Lit lhs, MyMinisat::Lit rhs)
{
	/*
	 (1,0) => 1
	 (0,1) => 0
	 */
	//early outs
	if (rhs.x == 1)
	{
		add01XUnitPacose(slv, lhs);
		return;
	}
	else if (rhs.x == 0)
	{
		add01XUnitPacose(slv, ~lhs);
		return;
	}

	MyMinisat::Lit lhs_aux = MyMinisat::mkLit(get01XAuxVar(MyMinisat::var(lhs)),
			false);
	MyMinisat::Lit rhs_aux = MyMinisat::mkLit(get01XAuxVar(MyMinisat::var(rhs)),
			false);

	//negation -> flip 0/1 variable
	if (MyMinisat::sign(lhs))
	{
		MyMinisat::Lit bufLit = MyMinisat::mkLit(MyMinisat::var(lhs), false);
		lhs = lhs_aux;
		lhs_aux = bufLit;
	}
	if (MyMinisat::sign(rhs))
	{
		MyMinisat::Lit bufLit = MyMinisat::mkLit(MyMinisat::var(rhs), false);
		rhs = rhs_aux;
		rhs_aux = bufLit;
	}

	assert(!sign(rhs) && !sign(lhs) && !sign(rhs_aux) && !sign(lhs_aux));

	std::vector<uint32_t> cl;
	cl.push_back((~lhs).x);
	cl.push_back(rhs.x);
	slv.AddClause(cl);
	cl.clear();
	cl.push_back((~rhs).x);
	cl.push_back(lhs.x);
	slv.AddClause(cl);
	//TODO: wie das?
	cl.clear();
	cl.push_back((~lhs_aux).x);
	cl.push_back(rhs_aux.x);
	slv.AddClause(cl);
	cl.clear();
	cl.push_back((~rhs_aux).x);
	cl.push_back(lhs_aux.x);
	slv.AddClause(cl);
}

void Model::add01XEquivPacoseElim(MyMinisat::SimpSolver &slv, MyMinisat::Lit lhs,
		MyMinisat::Lit rhs)
{
	/*
	 (1,0) => 1
	 (0,1) => 0
	 */
	//early outs
	if (rhs.x == 1)
	{
		slv.addClause(lhs);
		slv.addClause(
				MyMinisat::mkLit(get01XAuxVar(MyMinisat::var(lhs)),
						!MyMinisat::sign(lhs)));
		return;
	}
	else if (rhs.x == 0)
	{
		slv.addClause(~lhs);
		slv.addClause(
				MyMinisat::mkLit(get01XAuxVar(MyMinisat::var(lhs)),
						!MyMinisat::sign(~lhs)));
		return;
	}

	MyMinisat::Lit lhs_aux = MyMinisat::mkLit(get01XAuxVar(MyMinisat::var(lhs)),
			false);
	MyMinisat::Lit rhs_aux = MyMinisat::mkLit(get01XAuxVar(MyMinisat::var(rhs)),
			false);
	//negation -> flip 0/1 variable
	if (MyMinisat::sign(lhs))
	{
		MyMinisat::Lit bufLit = MyMinisat::mkLit(MyMinisat::var(lhs), false);
		lhs = lhs_aux;
		lhs_aux = bufLit;
	}
	if (MyMinisat::sign(rhs))
	{
		MyMinisat::Lit bufLit = MyMinisat::mkLit(MyMinisat::var(rhs), false);
		rhs = rhs_aux;
		rhs_aux = bufLit;
	}

	assert(!sign(rhs) && !sign(lhs) && !sign(rhs_aux) && !sign(lhs_aux));

	MyMinisat::vec<MyMinisat::Lit> cl;
	cl.push((~lhs));
	cl.push(rhs);
	slv.addClause(cl);
	cl.clear();
	cl.push((~rhs));
	cl.push(lhs);
	slv.addClause(cl);
	//TODO: wie das?
	cl.clear();
	cl.push((~lhs_aux));
	cl.push(rhs_aux);
	slv.addClause(cl);
	cl.clear();
	cl.push((~rhs_aux));
	cl.push(lhs_aux);
	slv.addClause(cl);
}

void Model::add01XcubeToPacoseSlv(Pacose &slv, const LitVec &c)
{
	/*
	 (1,0) => 1
	 (0,1) => 0

	 AND ( (1,0) , (1,0) ) = (1,0)
	 AND ( (0,1) , (1,0) ) = (0,1)
	 AND ( (1,0) , (0,1) ) = (0,1)
	 AND ( (0,1) , (0,1) ) = (0,1)
	 */
	std::vector<uint32_t> clAnd;
	for (auto &lit : c)
	{
		//negation: 1 -> 0, 0 -> 1. Aux lit the opposite. No X possible.
		if (MyMinisat::sign(lit))
			clAnd.push_back((~lit).x); //lit^(1) = true
		else
			clAnd.push_back(2 * get01XAuxVar(var(lit))); //lit^(0) = true;
			//MyMinisat::Lit negLit = ~lit;
			//clAnd.push_back(negLit.x);
		/*if(sign(lit))
		 antomClAnd.push_back(2*get01XAuxVar(var(lit))+1);
		 else
		 antomClAnd.push_back(2*get01XAuxVar(var(lit)));*/

			//clAnd.push_back(2 * get01XAuxVar(var(lit)) + sign(lit));
	}
	//assert(clAnd.size() == 2 * c.size());

	//add clause to antom-instance
	//slv._satSolver->AddClause(clAnd);
	slv.AddClause(clAnd);

}

void Model::add01XCube(MyMinisat::Solver &slv, const LitVec &c)
{
	/*
	 (1,0) => 1
	 (0,1) => 0

	 AND ( (1,0) , (1,0) ) = (1,0)
	 AND ( (0,1) , (1,0) ) = (0,1)
	 AND ( (1,0) , (0,1) ) = (0,1)
	 AND ( (0,1) , (0,1) ) = (0,1)
	 */
	MSLitVec clAnd;
	for (auto &lit : c)
	{
		//negation: 1 -> 0, 0 -> 1. Aux lit the opposite. No X possible.
		//MyMinisat::Lit negLit = ~lit;
		//clAnd.push(negLit);
		//clAnd.push(MyMinisat::mkLit(get01XAuxVar(var(lit)), sign(lit)));
		if (MyMinisat::sign(lit))
			clAnd.push(~lit); //lit^(1) = true
		else
			clAnd.push(MyMinisat::mkLit(get01XAuxVar(var(lit)), false)); //lit^(0) = true;

	}
	//assert(clAnd.size() == 2 * c.size());

	slv.addClause(clAnd);
}

void Model::addDualRailCube(MyMinisat::Solver &slv, const LitVec &c)
{
	//differentiate for state variables
	MyMinisat::vec<MyMinisat::Lit> cl;
	for (auto lit : c)
	{
		if (MyMinisat::var(lit) >= beginLatches()->var()
				&& MyMinisat::var(lit) < endLatches()->var())
		{
			MyMinisat::Lit litpos = MyMinisat::mkLit(MyMinisat::var(lit), false); // v^t = 1
			MyMinisat::Lit auxlit = MyMinisat::mkLit(
					get01XAuxVar(MyMinisat::var(lit)), false); // v^f = 1
			if (!MyMinisat::sign(lit))
				cl.push(auxlit);
			else
				cl.push(litpos);
		}
		else
			cl.push(~lit);
	}
	slv.addClause(cl);
}


void Model::ipamirAddClause(void * solver,  const vector<MyMinisat::Lit> cls_) const
{
	for(auto& lit: cls_)
	{
		//pos neg encoding for ipamir
		int posNegLit = MyMinisat::var(lit) * ((int) pow(-1, MyMinisat::sign(lit)));
		ipamir_add_hard (solver, posNegLit);
	}
	ipamir_add_hard (solver, 0); //close clause (DIMACS style)
}

void Model::ipamirAddSoftLit(void * solver,  const MyMinisat::Lit l)  const
{
	int posNegLit = MyMinisat::var(l) * ((int) pow(-1, !MyMinisat::sign(l)));
	ipamir_add_soft_lit(solver, posNegLit, 1); //no weight distinction
}

void Model::ipamirAssume(void * solver,  const MyMinisat::Lit l)  const
{
	int posNegLit = MyMinisat::var(l) * ((int) pow(-1, MyMinisat::sign(l)));
	ipamir_assume(solver, posNegLit);
}


void Model::loadTransitionRelation01XPacose(Pacose &slv, bool primeConstraints)
{
	//only pre 1.9 aiger
	//forbid (1,1) encoding
	for (auto &var : vars)
	{
		if (var.var() == 0)
			continue;
		std::vector<uint32_t> cl;
		cl.push_back(2 * var.var() + 1);
		cl.push_back(2 * (get01XAuxVar(var.var())) + 1);
		//slv._satSolver->AddClause(cl);
		slv.AddClause(cl);
	}
	// freeze inputs, latches, and special nodes (and primed forms)
	/*for (VarVec::const_iterator i = beginInputs(); i != endInputs(); ++i)
	 {
	 slv._satSolver->SetFrozen(i->var());
	 slv._satSolver->SetFrozen(get01XAuxVar(i->var()));
	 slv._satSolver->SetFrozen(primeVar(*i).var());
	 slv._satSolver->SetFrozen(get01XAuxVar(primeVar(*i).var()));
	 }
	 for (VarVec::const_iterator i = beginLatches(); i != endLatches(); ++i)
	 {
	 slv._satSolver->SetFrozen(i->var());
	 slv._satSolver->SetFrozen(get01XAuxVar(i->var()));
	 slv._satSolver->SetFrozen(primeVar(*i).var());
	 slv._satSolver->SetFrozen(get01XAuxVar(primeVar(*i).var()));
	 }
	 slv._satSolver->SetFrozen(varOfLit(error()).var());
	 slv._satSolver->SetFrozen(get01XAuxVar(varOfLit(error()).var()));
	 slv._satSolver->SetFrozen(varOfLit(primedError()).var());
	 slv._satSolver->SetFrozen(get01XAuxVar(varOfLit(primedError()).var()));*/

	// initialize with roots of required formulas
	LitSet require;  // unprimed formulas
	for (VarVec::const_iterator i = beginLatches(); i != endLatches(); ++i)
		require.insert(nextStateFn(*i));
	require.insert(_error);
	LitSet prequire; // for primed formulas; always subset of require
	prequire.insert(_error);
	// traverse AIG backward
	for (AigVec::const_reverse_iterator i = aig.rbegin(); i != aig.rend(); ++i)
	{
		// skip if this row is not required
		if (require.find(i->lhs) == require.end()
				&& require.find(~i->lhs) == require.end())
			continue;
		// encode into CNF
		add01XAndPacose(slv, i->lhs, i->rhs0, i->rhs1);
		// require arguments
		require.insert(i->rhs0);
		require.insert(i->rhs1);
		// primed: skip if not required
		if (prequire.find(i->lhs) == prequire.end()
				&& prequire.find(~i->lhs) == prequire.end())
			continue;
		// encode PRIMED form into CNF
		MyMinisat::Lit r0 = primeLit(i->lhs, sslv), r1 = primeLit(i->rhs0, sslv),
				r2 = primeLit(i->rhs1, sslv);
		add01XAndPacose(slv, r0, r1, r2);
		// require arguments
		prequire.insert(i->rhs0);
		prequire.insert(i->rhs1);
	}
	// assert literal for true
	//add01XUnitPacose(slv, btrue());
	// assert ~error, !!no constraints, and primed constraints!! (pre AIGER 1.9)
	//add01XUnitPacose(slv, (~_error));
	// assert l' = f for each latch l
	for (VarVec::const_iterator i = beginLatches(); i != endLatches(); ++i)
	{
		MyMinisat::Lit platch = primeLit(i->lit(false));
		MyMinisat::Lit f = nextStateFn(*i);
		add01XEquivPacose(slv, platch, f);
	}
	//slv.Simplify();
	/*slv.SetPreprocessing(antom::PREPROCESS);
	 slv.Preprocess(antom::PREPROCESS);
	 slv.SetPreprocessing(antom::NOPREPRO);*/
}

void Model::loadTransitionRelation01XPacoseElim(Pacose &slv,
		bool primeConstraints)
{
	assert(sslv); //prime etc. has been done already
	//forbid (1,1) encoding
	if (!sslv01X)
	{
		// create a simplified CNF version of (this slice of) the TR
		sslv01X = new MyMinisat::SimpSolver();
		// introduce all variables to maintain alignment (including 01X aux-vars)
		for (size_t i = 0; i < 2 * vars.size(); ++i)
		{
			MyMinisat::Var nv = sslv01X->newVar();
			if (i < vars.size())
				assert(nv == vars[i].var());
		}

		//forbid (1,1) encoding
		for (auto &v : vars)
		{
			MyMinisat::vec<MyMinisat::Lit> cl;
			cl.push(MyMinisat::mkLit(v.var(), true));
			cl.push(MyMinisat::mkLit(get01XAuxVar(v.var()), true));
			sslv01X->addClause(cl);
		}

		// freeze inputs, latches, and special nodes (and primed forms)
		for (VarVec::const_iterator i = beginInputs(); i != endInputs(); ++i)
		{
			sslv01X->setFrozen(i->var(), true);
			sslv01X->setFrozen(get01XAuxVar(i->var()), true);
			sslv01X->setFrozen(primeVar(*i).var(), true);
			sslv01X->setFrozen(get01XAuxVar(primeVar(*i).var()), true);
		}
		for (VarVec::const_iterator i = beginLatches(); i != endLatches(); ++i)
		{
			sslv01X->setFrozen(i->var(), true);
			sslv01X->setFrozen(get01XAuxVar(i->var()), true);
			sslv01X->setFrozen(primeVar(*i).var(), true);
			sslv01X->setFrozen(get01XAuxVar(primeVar(*i).var()), true);
		}
		sslv01X->setFrozen(varOfLit(error()).var(), true);
		sslv01X->setFrozen(get01XAuxVar(varOfLit(error()).var()), true);
		sslv01X->setFrozen(varOfLit(primedError()).var(), true);
		sslv01X->setFrozen(get01XAuxVar(varOfLit(primedError()).var()), true);
		for (LitVec::const_iterator i = constraints.begin();
				i != constraints.end(); ++i)
		{
			Var v = varOfLit(*i);
			sslv01X->setFrozen(v.var(), true);
			sslv01X->setFrozen(get01XAuxVar(v.var()), true);
			sslv01X->setFrozen(primeVar(v).var(), true);
			sslv01X->setFrozen(get01XAuxVar(primeVar(v).var()), true);
		}

		// initialize with roots of required formulas
		LitSet require;  // unprimed formulas
		for (VarVec::const_iterator i = beginLatches(); i != endLatches(); ++i)
			require.insert(nextStateFn(*i));
		require.insert(_error);
		require.insert(constraints.begin(), constraints.end());
		LitSet prequire; // for primed formulas; always subset of require
		prequire.insert(_error);
		prequire.insert(constraints.begin(), constraints.end());
		// traverse AIG backward
		for (AigVec::const_reverse_iterator i = aig.rbegin(); i != aig.rend();
				++i)
		{
			// skip if this row is not required
			if (require.find(i->lhs) == require.end()
					&& require.find(~i->lhs) == require.end())
				continue;
			// encode into CNF
			add01XAndPacoseElim(*sslv01X, i->lhs, i->rhs0, i->rhs1);
			// require arguments
			require.insert(i->rhs0);
			require.insert(i->rhs1);
			// primed: skip if not required
			if (prequire.find(i->lhs) == prequire.end()
					&& prequire.find(~i->lhs) == prequire.end())
				continue;
			// encode PRIMED form into CNF
			MyMinisat::Lit r0 = primeLit(i->lhs, sslv01X), r1 = primeLit(i->rhs0,
					sslv01X), r2 = primeLit(i->rhs1, sslv01X);
			add01XAndPacoseElim(*sslv01X, r0, r1, r2);
			// require arguments
			prequire.insert(i->rhs0);
			prequire.insert(i->rhs1);
		}
		for (LitVec::const_iterator i = constraints.begin();
				i != constraints.end(); ++i)
		{
			sslv01X->addClause(*i);
			sslv01X->addClause(
					MyMinisat::mkLit(get01XAuxVar(MyMinisat::var(*i)),
							!MyMinisat::sign(*i)));
		}
		// assert literal for true
		sslv01X->addClause(btrue());
		/*sslv01X->addClause(~_error);
		 sslv01X->addClause(
		 MyMinisat::mkLit(get01XAuxVar(MyMinisat::var(_error)),
		 !MyMinisat::sign(~_error)));*/
		// assert l' = f for each latch l
		for (VarVec::const_iterator i = beginLatches(); i != endLatches(); ++i)
		{
			MyMinisat::Lit platch = primeLit(i->lit(false));
			MyMinisat::Lit f = nextStateFn(*i);
			add01XEquivPacoseElim(*sslv01X, platch, f);
		}
		sslv01X->eliminate(true);
	}

	//-> everything aligned
	assert((unsigned)sslv01X->nVars() == 2 * vars.size());

	for (MyMinisat::ClauseIterator c = sslv01X->clausesBegin();
			c != sslv01X->clausesEnd(); ++c)
	{
		const MyMinisat::Clause &cls = *c;
		std::vector<unsigned> cls_;
		for (int i = 0; i < cls.size(); ++i)
		{
			assert(cls[i].x > 1);
			cls_.push_back(cls[i].x);
		}
		slv.AddClause(cls_);
	}
	for (MyMinisat::TrailIterator c = sslv01X->trailBegin();
			c != sslv01X->trailEnd(); ++c)
	{
		if ((*c).x > 1)
		{
			std::vector<unsigned> cls_;
			cls_.push_back((*c).x);
			slv.AddClause(cls_);
		}
	}
	if (primeConstraints)
	{
		for (LitVec::const_iterator i = constraints.begin();
				i != constraints.end(); ++i)
		{
			std::vector<unsigned> cls_;
			cls_.push_back(primeLit(*i).x);
			slv.AddClause(cls_);
			cls_.clear();
			cls_.push_back(
					2 * get01XAuxVar(MyMinisat::var(primeLit(*i)))
							+ !MyMinisat::sign(primeLit(*i)));
			slv.AddClause(cls_);
		}
	}
}

void Model::loadTransitionRelation01XIpamir(void* solver)
{
	assert(sslv); //prime etc. has been done already
	//forbid (1,1) encoding
	if (!sslv01X)
	{
		// create a simplified CNF version of (this slice of) the TR
		sslv01X = new MyMinisat::SimpSolver();
		// introduce all variables to maintain alignment (including 01X aux-vars)
		for (size_t i = 0; i < 2 * vars.size(); ++i)
		{
			MyMinisat::Var nv = sslv01X->newVar();
			if (i < vars.size())
				assert(nv == vars[i].var());
		}

		//forbid (1,1) encoding
		for (auto &v : vars)
		{
			MyMinisat::vec<MyMinisat::Lit> cl;
			cl.push(MyMinisat::mkLit(v.var(), true));
			cl.push(MyMinisat::mkLit(get01XAuxVar(v.var()), true));
			sslv01X->addClause(cl);
		}

		// freeze inputs, latches, and special nodes (and primed forms)
		for (VarVec::const_iterator i = beginInputs(); i != endInputs(); ++i)
		{
			sslv01X->setFrozen(i->var(), true);
			sslv01X->setFrozen(get01XAuxVar(i->var()), true);
			sslv01X->setFrozen(primeVar(*i).var(), true);
			sslv01X->setFrozen(get01XAuxVar(primeVar(*i).var()), true);
		}
		for (VarVec::const_iterator i = beginLatches(); i != endLatches(); ++i)
		{
			sslv01X->setFrozen(i->var(), true);
			sslv01X->setFrozen(get01XAuxVar(i->var()), true);
			sslv01X->setFrozen(primeVar(*i).var(), true);
			sslv01X->setFrozen(get01XAuxVar(primeVar(*i).var()), true);
		}
		sslv01X->setFrozen(varOfLit(error()).var(), true);
		sslv01X->setFrozen(get01XAuxVar(varOfLit(error()).var()), true);
		sslv01X->setFrozen(varOfLit(primedError()).var(), true);
		sslv01X->setFrozen(get01XAuxVar(varOfLit(primedError()).var()), true);
		for (LitVec::const_iterator i = constraints.begin();
				i != constraints.end(); ++i)
		{
			Var v = varOfLit(*i);
			sslv01X->setFrozen(v.var(), true);
			sslv01X->setFrozen(get01XAuxVar(v.var()), true);
			sslv01X->setFrozen(primeVar(v).var(), true);
			sslv01X->setFrozen(get01XAuxVar(primeVar(v).var()), true);
		}

		// initialize with roots of required formulas
		LitSet require;  // unprimed formulas
		for (VarVec::const_iterator i = beginLatches(); i != endLatches(); ++i)
			require.insert(nextStateFn(*i));
		require.insert(_error);
		require.insert(constraints.begin(), constraints.end());
		LitSet prequire; // for primed formulas; always subset of require
		prequire.insert(_error);
		prequire.insert(constraints.begin(), constraints.end());
		// traverse AIG backward
		for (AigVec::const_reverse_iterator i = aig.rbegin(); i != aig.rend();
				++i)
		{
			// skip if this row is not required
			if (require.find(i->lhs) == require.end()
					&& require.find(~i->lhs) == require.end())
				continue;
			// encode into CNF
			add01XAndPacoseElim(*sslv01X, i->lhs, i->rhs0, i->rhs1);
			// require arguments
			require.insert(i->rhs0);
			require.insert(i->rhs1);
			// primed: skip if not required
			if (prequire.find(i->lhs) == prequire.end()
					&& prequire.find(~i->lhs) == prequire.end())
				continue;
			// encode PRIMED form into CNF
			MyMinisat::Lit r0 = primeLit(i->lhs, sslv01X), r1 = primeLit(i->rhs0,
					sslv01X), r2 = primeLit(i->rhs1, sslv01X);
			add01XAndPacoseElim(*sslv01X, r0, r1, r2);
			// require arguments
			prequire.insert(i->rhs0);
			prequire.insert(i->rhs1);
		}
		for (LitVec::const_iterator i = constraints.begin();
				i != constraints.end(); ++i)
		{
			sslv01X->addClause(*i);
			sslv01X->addClause(
					MyMinisat::mkLit(get01XAuxVar(MyMinisat::var(*i)),
							!MyMinisat::sign(*i)));
		}
		// assert literal for true
		sslv01X->addClause(btrue());
		/*sslv01X->addClause(~_error);
		 sslv01X->addClause(
		 MyMinisat::mkLit(get01XAuxVar(MyMinisat::var(_error)),
		 !MyMinisat::sign(~_error)));*/
		// assert l' = f for each latch l
		for (VarVec::const_iterator i = beginLatches(); i != endLatches(); ++i)
		{
			MyMinisat::Lit platch = primeLit(i->lit(false));
			MyMinisat::Lit f = nextStateFn(*i);
			add01XEquivPacoseElim(*sslv01X, platch, f);
		}
		sslv01X->eliminate(true);
	}

	//-> everything aligned
	assert((unsigned)sslv01X->nVars() == 2 * vars.size());

	for (MyMinisat::ClauseIterator c = sslv01X->clausesBegin();
			c != sslv01X->clausesEnd(); ++c)
	{
		const MyMinisat::Clause &cls = *c;
		std::vector<MyMinisat::Lit> cls_;
		for (int i = 0; i < cls.size(); ++i)
		{
			assert(cls[i].x > 1);
			cls_.push_back(cls[i]);
		}
		ipamirAddClause(solver, cls_);
	}
	for (MyMinisat::TrailIterator c = sslv01X->trailBegin();
			c != sslv01X->trailEnd(); ++c)
	{
		if ((*c).x > 1)
		{
			std::vector<MyMinisat::Lit> cls_;
			cls_.push_back((*c));
			ipamirAddClause(solver, cls_);
		}
	}
}

void Model::loadTransitionRelation01XElim(MyMinisat::Solver &slv)
{
	assert(sslv); //prime etc. has been done already
	//forbid (1,1) encoding
	if (!sslv01X)
	{
		// create a simplified CNF version of (this slice of) the TR
		sslv01X = new MyMinisat::SimpSolver();
		// introduce all variables to maintain alignment (including 01X aux-vars)
		for (size_t i = 0; i < 2 * vars.size(); ++i)
		{
			MyMinisat::Var nv = sslv01X->newVar();
			if (i < vars.size())
				assert(nv == vars[i].var());
		}
		//forbid (1,1) encoding
		for (auto &v : vars)
		{
			MyMinisat::vec<MyMinisat::Lit> cl;
			cl.push(MyMinisat::mkLit(v.var(), true));
			cl.push(MyMinisat::mkLit(get01XAuxVar(v.var()), true));
			sslv01X->addClause(cl);
		}

		// freeze inputs, latches, and special nodes (and primed forms)
		for (VarVec::const_iterator i = beginInputs(); i != endInputs(); ++i)
		{
			sslv01X->setFrozen(i->var(), true);
			sslv01X->setFrozen(get01XAuxVar(i->var()), true);
			sslv01X->setFrozen(primeVar(*i).var(), true);
			sslv01X->setFrozen(get01XAuxVar(primeVar(*i).var()), true);
		}
		for (VarVec::const_iterator i = beginLatches(); i != endLatches(); ++i)
		{
			sslv01X->setFrozen(i->var(), true);
			sslv01X->setFrozen(get01XAuxVar(i->var()), true);
			sslv01X->setFrozen(primeVar(*i).var(), true);
			sslv01X->setFrozen(get01XAuxVar(primeVar(*i).var()), true);
		}
		sslv01X->setFrozen(varOfLit(error()).var(), true);
		sslv01X->setFrozen(get01XAuxVar(varOfLit(error()).var()), true);
		sslv01X->setFrozen(varOfLit(primedError()).var(), true);
		sslv01X->setFrozen(get01XAuxVar(varOfLit(primedError()).var()), true);
		for (LitVec::const_iterator i = constraints.begin();
				i != constraints.end(); ++i)
		{
			Var v = varOfLit(*i);
			sslv01X->setFrozen(v.var(), true);
			sslv01X->setFrozen(get01XAuxVar(v.var()), true);
			sslv01X->setFrozen(primeVar(v).var(), true);
			sslv01X->setFrozen(get01XAuxVar(primeVar(v).var()), true);
		}

		// initialize with roots of required formulas
		LitSet require;  // unprimed formulas
		for (VarVec::const_iterator i = beginLatches(); i != endLatches(); ++i)
			require.insert(nextStateFn(*i));
		require.insert(_error);
		require.insert(constraints.begin(), constraints.end());
		LitSet prequire; // for primed formulas; always subset of require
		prequire.insert(_error);
		prequire.insert(constraints.begin(), constraints.end());
		// traverse AIG backward
		for (AigVec::const_reverse_iterator i = aig.rbegin(); i != aig.rend();
				++i)
		{
			// skip if this row is not required
			if (require.find(i->lhs) == require.end()
					&& require.find(~i->lhs) == require.end())
				continue;
			// encode into CNF
			add01XAndPacoseElim(*sslv01X, i->lhs, i->rhs0, i->rhs1);
			// require arguments
			require.insert(i->rhs0);
			require.insert(i->rhs1);
			// primed: skip if not required
			if (prequire.find(i->lhs) == prequire.end()
					&& prequire.find(~i->lhs) == prequire.end())
				continue;
			// encode PRIMED form into CNF
			MyMinisat::Lit r0 = primeLit(i->lhs, sslv01X), r1 = primeLit(i->rhs0,
					sslv01X), r2 = primeLit(i->rhs1, sslv01X);
			add01XAndPacoseElim(*sslv01X, r0, r1, r2);
			// require arguments
			prequire.insert(i->rhs0);
			prequire.insert(i->rhs1);
		}
		for (LitVec::const_iterator i = constraints.begin();
				i != constraints.end(); ++i)
		{
			sslv01X->addClause(*i);
			sslv01X->addClause(
					MyMinisat::mkLit(get01XAuxVar(MyMinisat::var(*i)),
							!MyMinisat::sign(*i)));
		}
		// assert literal for true
		sslv01X->addClause(btrue());
		sslv01X->addClause(~_error);
		 sslv01X->addClause(
		 MyMinisat::mkLit(get01XAuxVar(MyMinisat::var(_error)),
		 !MyMinisat::sign(~_error)));
		// assert l' = f for each latch l
		for (VarVec::const_iterator i = beginLatches(); i != endLatches(); ++i)
		{
			MyMinisat::Lit platch = primeLit(i->lit(false));
			MyMinisat::Lit f = nextStateFn(*i);
			add01XEquivPacoseElim(*sslv01X, platch, f);
		}
		sslv01X->eliminate(true);
	}

	//-> everything aligned
	assert((unsigned)sslv01X->nVars() == 2 * vars.size());
	// load the clauses from the simplified context
	//while (slv.nVars() < sslv01X->nVars())
	//	slv.newVar(MyMinisat::l_True); //MyMinisat::l_False);

	//forbid (1,1) encoding
	for (auto &v : vars)
	{
		MyMinisat::vec<MyMinisat::Lit> cl;
		cl.push(MyMinisat::mkLit(v.var(), true));
		cl.push(MyMinisat::mkLit(get01XAuxVar(v.var()), true));
		slv.addClause(cl);
	}

	for (MyMinisat::ClauseIterator c = sslv01X->clausesBegin();
			c != sslv01X->clausesEnd(); ++c)
	{
		const MyMinisat::Clause &cls = *c;
		MyMinisat::vec<MyMinisat::Lit> cls_;
		for (int i = 0; i < cls.size(); ++i)
			cls_.push(cls[i]);
		slv.addClause_(cls_);
	}
	for (MyMinisat::TrailIterator c = sslv01X->trailBegin();
			c != sslv01X->trailEnd(); ++c)
		slv.addClause(*c);

	for (LitVec::const_iterator i = constraints.begin(); i != constraints.end();
			++i)
	{
		slv.addClause(primeLit(*i));
		slv.addClause(
				MyMinisat::mkLit(get01XAuxVar(MyMinisat::var(primeLit(*i))),
						!MyMinisat::sign(primeLit(*i))));
	}
}

void Model::loadTransitionRelationCover01XElim(MyMinisat::Solver &slv)
{
	assert(sslv); //prime etc. has been done already
	//forbid (1,1) encoding
	if (!sslv01X)
	{
		// create a simplified CNF version of (this slice of) the TR
		sslv01X = new MyMinisat::SimpSolver();
		// introduce all variables to maintain alignment (including 01X aux-vars)
		for (size_t i = 0; i < 2 * vars.size(); ++i)
		{
			MyMinisat::Var nv = sslv01X->newVar();
			if (i < vars.size())
				assert(nv == vars[i].var());
		}
		// freeze inputs, latches, and special nodes (and primed forms)
		for (VarVec::const_iterator i = beginInputs(); i != endInputs(); ++i)
		{
			sslv01X->setFrozen(i->var(), true);
			sslv01X->setFrozen(primeVar(*i).var(), true);
		}
		for (VarVec::const_iterator i = beginLatches(); i != endLatches(); ++i)
		{
			sslv01X->setFrozen(i->var(), true);
			sslv01X->setFrozen(get01XAuxVar(i->var()), true);
			sslv01X->setFrozen(primeVar(*i).var(), true);
			sslv01X->setFrozen(get01XAuxVar(primeVar(*i).var()), true);
		}
		sslv01X->setFrozen(varOfLit(error()).var(), true);
		sslv01X->setFrozen(varOfLit(primedError()).var(), true);
		for (LitVec::const_iterator i = constraints.begin();
				i != constraints.end(); ++i)
		{
			Var v = varOfLit(*i);
			sslv01X->setFrozen(v.var(), true);
			sslv01X->setFrozen(primeVar(v).var(), true);
		}

		// initialize with roots of required formulas
		LitSet require;  // unprimed formulas
		for (VarVec::const_iterator i = beginLatches(); i != endLatches(); ++i)
			require.insert(nextStateFn(*i));
		require.insert(_error);
		require.insert(constraints.begin(), constraints.end());
		LitSet prequire; // for primed formulas; always subset of require
		prequire.insert(_error);
		prequire.insert(constraints.begin(), constraints.end());
		// traverse AIG backward
		for (AigVec::const_reverse_iterator i = aig.rbegin(); i != aig.rend();
				++i)
		{
			// skip if this row is not required
			if (require.find(i->lhs) == require.end()
					&& require.find(~i->lhs) == require.end())
				continue;
			// encode into CNF
			sslv01X->addClause(~i->lhs, i->rhs0);
			sslv01X->addClause(~i->lhs, i->rhs1);
			sslv01X->addClause(~i->rhs0, ~i->rhs1, i->lhs);
			// require arguments
			require.insert(i->rhs0);
			require.insert(i->rhs1);
			// primed: skip if not required
			if (prequire.find(i->lhs) == prequire.end()
					&& prequire.find(~i->lhs) == prequire.end())
				continue;
			// encode PRIMED form into CNF
			MyMinisat::Lit r0 = primeLit(i->lhs, sslv01X), r1 = primeLit(i->rhs0,
					sslv01X), r2 = primeLit(i->rhs1, sslv01X);
			sslv01X->addClause(~r0, r1);
			sslv01X->addClause(~r0, r2);
			sslv01X->addClause(~r1, ~r2, r0);
			// require arguments
			prequire.insert(i->rhs0);
			prequire.insert(i->rhs1);
		}
		sslv01X->addClause(btrue());
		//sslv01X->addClause(~_error);
		// assert literal for true
		// assert ~error, constraints, and primed constraints
		for (LitVec::const_iterator i = constraints.begin();
				i != constraints.end(); ++i)
		{
			sslv01X->addClause(*i);
		}
		// assert l' = f for each latch l
		for (VarVec::const_iterator i = beginLatches(); i != endLatches(); ++i)
		{
			MyMinisat::Lit platch = primeLit(i->lit(false)), f = nextStateFn(*i);
			sslv01X->addClause(~platch, f);
			sslv01X->addClause(~f, platch);
		}
		sslv01X->eliminate(true);
	}

	//-> everything aligned
	// load the clauses from the simplified context
	while (slv.nVars() < sslv01X->nVars())
		slv.newVar();

	//forbid (1,1) encoding -- only for present states -> binate cover
	for (auto &var : vars)
	{
		if (var.var() == 0)
			continue;
		MyMinisat::vec<MyMinisat::Lit> cl;
		cl.push(MyMinisat::mkLit(var.var(), true));
		cl.push(MyMinisat::mkLit(get01XAuxVar(var.var()), true));
		slv.addClause(cl);
	}

	for (MyMinisat::ClauseIterator c = sslv01X->clausesBegin();
			c != sslv01X->clausesEnd(); ++c)
	{
		const MyMinisat::Clause &cls = *c;
		MyMinisat::vec<MyMinisat::Lit> cls_;
		for (int i = 0; i < cls.size(); ++i)
		{
			//differentiate for state variables
			if (MyMinisat::var(cls[i]) >= beginLatches()->var()
					&& MyMinisat::var(cls[i]) < endLatches()->var())
			{
				MyMinisat::Lit lit = MyMinisat::mkLit(MyMinisat::var(cls[i]), false); // v^t = 1
				MyMinisat::Lit auxlit = MyMinisat::mkLit(
						get01XAuxVar(MyMinisat::var(cls[i])), false); // v^f = 1
				if (MyMinisat::sign(cls[i]))
					cls_.push(auxlit);
				else
					cls_.push(lit);
			}
			else
				cls_.push(cls[i]);
		}
		slv.addClause_(cls_);
	}

	for (MyMinisat::TrailIterator c = sslv01X->trailBegin();
			c != sslv01X->trailEnd(); ++c)
	{
		assert(
				MyMinisat::var(*c) < beginLatches()->var()
						|| MyMinisat::var(*c) >= endLatches()->var());
		slv.addClause(*c);
	}

	for (LitVec::const_iterator i = constraints.begin(); i != constraints.end();
			++i)
		slv.addClause(primeLit(*i));
}

#ifdef maxsatpogen
void Model::loadInitialCondition01X(antom::Antom &slv) const
{
	add01XUnit(slv, btrue());
	for (LitVec::const_iterator i = init.begin(); i != init.end(); ++i)
	{
		add01XUnit(slv, (*i));
	}
	assert(constraints.empty());
	// don't impose invariant constraints on initial states (pre AIGER 1.9)
}
#endif /* maxsatpogen */

void Model::loadInitialCondition01XPacose(Pacose &slv)
{
	add01XUnitPacose(slv, btrue());
	for (LitVec::const_iterator i = init.begin(); i != init.end(); ++i)
	{
		add01XUnitPacose(slv, (*i));
	}
	if (constraints.empty())
		return;
	// impose invariant constraints on initial states (AIGER 1.9)
	LitSet require;
	require.insert(constraints.begin(), constraints.end());
	for (AigVec::const_reverse_iterator i = aig.rbegin(); i != aig.rend(); ++i)
	{
		//assert(i->rhs0.x > 1 && i->rhs1.x > 1);
		// skip if this (*i) is not required
		if (require.find(i->lhs) == require.end()
				&& require.find(~i->lhs) == require.end())
			continue;
		// encode into CNF
		add01XAndPacose(slv, i->lhs, i->rhs0, i->rhs1);
		// require arguments
		require.insert(i->rhs0);
		require.insert(i->rhs1);
	}
	for (LitVec::const_iterator i = constraints.begin(); i != constraints.end();
			++i)
		add01XUnitPacose(slv, *i);
	//slv.addClause(*i);
}

void Model::loadInitialCondition01X(MyMinisat::Solver &slv)
{
	add01XUnit(slv, btrue());
	for (LitVec::const_iterator i = init.begin(); i != init.end(); ++i)
	{
		add01XUnit(slv, (*i));
	}
	if (constraints.empty())
		return;
	// impose invariant constraints on initial states (AIGER 1.9)
	LitSet require;
	require.insert(constraints.begin(), constraints.end());
	for (AigVec::const_reverse_iterator i = aig.rbegin(); i != aig.rend(); ++i)
	{
		// skip if this (*i) is not required
		if (require.find(i->lhs) == require.end()
				&& require.find(~i->lhs) == require.end())
			continue;
		// encode into CNF
		add01XAnd(slv, i->lhs, i->rhs0, i->rhs1);
		// require arguments
		require.insert(i->rhs0);
		require.insert(i->rhs1);
	}
	for (LitVec::const_iterator i = constraints.begin(); i != constraints.end();
			++i)
		add01XUnit(slv, *i);
}

void Model::loadInitialCondition(MyMinisat::Solver &slv) const
{
	slv.addClause(btrue());
	for (LitVec::const_iterator i = init.begin(); i != init.end(); ++i)
	{
		slv.addClause(*i);
	}
	if (constraints.empty())
		return;
	// impose invariant constraints on initial states (AIGER 1.9)
	LitSet require;
	require.insert(constraints.begin(), constraints.end());
	for (AigVec::const_reverse_iterator i = aig.rbegin(); i != aig.rend(); ++i)
	{
		// skip if this (*i) is not required
		if (require.find(i->lhs) == require.end()
				&& require.find(~i->lhs) == require.end())
			continue;
		// encode into CNF
		slv.addClause(~i->lhs, i->rhs0);
		slv.addClause(~i->lhs, i->rhs1);
		slv.addClause(~i->rhs0, ~i->rhs1, i->lhs);
		// require arguments
		require.insert(i->rhs0);
		require.insert(i->rhs1);
	}
	for (LitVec::const_iterator i = constraints.begin(); i != constraints.end();
			++i)
		slv.addClause(*i);
}

void Model::loadInitialConditionDepQBF(QDPLL &slv) const
{
#if 0
	slv.addClause(btrue());
	for (LitVec::const_iterator i = init.begin(); i != init.end(); ++i)
	slv.addClause(*i);
	if (constraints.empty())
	return;
	// impose invariant constraints on initial states (AIGER 1.9)
	LitSet require;
	require.insert(constraints.begin(), constraints.end());
	for (AigVec::const_reverse_iterator i = aig.rbegin(); i != aig.rend(); ++i)
	{
		// skip if this (*i) is not required
		if (require.find(i->lhs) == require.end()
				&& require.find(~i->lhs) == require.end())
		continue;
		// encode into CNF
		slv.addClause(~i->lhs, i->rhs0);
		slv.addClause(~i->lhs, i->rhs1);
		slv.addClause(~i->rhs0, ~i->rhs1, i->lhs);
		// require arguments
		require.insert(i->rhs0);
		require.insert(i->rhs1);
	}
	for (LitVec::const_iterator i = constraints.begin(); i != constraints.end();
			++i)
	slv.addClause(*i);
#endif
}

void Model::loadError(MyMinisat::Solver &slv, bool noConstraints)
{
	if (!sslvError)
	{
		// create a simplified CNF version of (this slice of) the TR
		sslvError = new MyMinisat::SimpSolver();
		// introduce all variables to maintain alignment
		for (size_t i = 0; i < vars.size(); ++i)
		{
			MyMinisat::Var nv = sslvError->newVar();
			assert(nv == vars[i].var());
		}
		// freeze inputs, latches, and special nodes (and primed forms)
		for (VarVec::const_iterator i = beginInputs(); i != endInputs(); ++i)
		{
			sslvError->setFrozen(i->var(), true);
		}
		for (VarVec::const_iterator i = beginLatches(); i != endLatches(); ++i)
		{
			sslvError->setFrozen(i->var(), true);
		}
		sslvError->setFrozen(varOfLit(error()).var(), true);
		sslvError->setFrozen(varOfLit(primedError()).var(), true);
		if (!noConstraints)
		{
			for (LitVec::const_iterator i = constraints.begin();
					i != constraints.end(); ++i)
			{
				Var v = varOfLit(*i);
				sslvError->setFrozen(v.var(), true);
			}
		}
		// initialize with roots of required formulas
		LitSet require;  // unprimed formulas
		require.insert(_error);
		if (!noConstraints)
		{
			require.insert(constraints.begin(), constraints.end());
		}
		// traverse AIG backward
		for (AigVec::const_reverse_iterator i = aig.rbegin(); i != aig.rend();
				++i)
		{
			// skip if this row is not required
			if (require.find(i->lhs) == require.end()
					&& require.find(~i->lhs) == require.end())
				continue;
			// encode into CNF
			sslvError->addClause(~i->lhs, i->rhs0);
			sslvError->addClause(~i->lhs, i->rhs1);
			sslvError->addClause(~i->rhs0, ~i->rhs1, i->lhs);
			// require arguments
			require.insert(i->rhs0);
			require.insert(i->rhs1);
		}
		// assert literal for true
		sslvError->addClause(btrue());
		if (!noConstraints)
		{
			for (LitVec::const_iterator i = constraints.begin();
					i != constraints.end(); ++i)
			{
				sslvError->addClause(*i);
			}
		}
		sslvError->eliminate(true);
	}
	// load the clauses from the simplified context
	while (slv.nVars() < sslvError->nVars())
		slv.newVar();
	for (MyMinisat::ClauseIterator c = sslvError->clausesBegin();
			c != sslvError->clausesEnd(); ++c)
	{
		const MyMinisat::Clause &cls = *c;
		MyMinisat::vec<MyMinisat::Lit> cls_;
		for (int i = 0; i < cls.size(); ++i)
			cls_.push(cls[i]);
		slv.addClause_(cls_);
	}
	for (MyMinisat::TrailIterator c = sslvError->trailBegin();
			c != sslvError->trailEnd(); ++c)
		slv.addClause(*c);

	/*LitSet require;  // unprimed formulas
	 slv.addClause(btrue());
	 require.insert(_error);
	 // traverse AIG backward
	 for (AigVec::const_reverse_iterator i = aig.rbegin(); i != aig.rend(); ++i)
	 {
	 // skip if this row is not required
	 if (require.find(i->lhs) == require.end()
	 && require.find(~i->lhs) == require.end())
	 continue;
	 // encode into CNF
	 slv.addClause(~i->lhs, i->rhs0);
	 slv.addClause(~i->lhs, i->rhs1);
	 slv.addClause(~i->rhs0, ~i->rhs1, i->lhs);
	 // require arguments
	 require.insert(i->rhs0);
	 require.insert(i->rhs1);
	 }*/
}

void Model::loadErrorBasic(MyMinisat::Solver &slv) const
{
	LitSet require;  // unprimed formulas
	slv.addClause(btrue());
	require.insert(_error);
	// traverse AIG backward
	for (AigVec::const_reverse_iterator i = aig.rbegin(); i != aig.rend(); ++i)
	{
		// skip if this row is not required
		if (require.find(i->lhs) == require.end()
				&& require.find(~i->lhs) == require.end())
			continue;
		// encode into CNF
		slv.addClause(~i->lhs, i->rhs0);
		slv.addClause(~i->lhs, i->rhs1);
		slv.addClause(~i->rhs0, ~i->rhs1, i->lhs);
		// require arguments
		require.insert(i->rhs0);
		require.insert(i->rhs1);
	}
}

bool Model::isInitial(const LitVec &latches, bool rev)
{
	if (!rev)
	{
		if (constraints.empty())
		{
			// an intersection check (AIGER 1.9 w/o invariant constraints)
			if (initLits.empty())
				initLits.insert(init.begin(), init.end());
			for (LitVec::const_iterator i = latches.begin(); i != latches.end();
					++i)
				if (initLits.find(~*i) != initLits.end())
					return false;
			return true;
		}
		else
		{
			// a full SAT query
			if (!inits)
			{
				inits = newSolver();
				loadInitialCondition(*inits);
			}
			MyMinisat::vec<MyMinisat::Lit> assumps;
			assumps.capacity(latches.size());
			for (LitVec::const_iterator i = latches.begin(); i != latches.end();
					++i)
				assumps.push(*i);
			return inits->solve(assumps);
		}
	}
	else
	{
		// a full SAT query
		if (!inits)
		{
			inits = newSolver();
			inits->addClause(error());
			loadError(*inits);
		}

		MyMinisat::vec<MyMinisat::Lit> assumps;
		assumps.capacity(latches.size());
		for (LitVec::const_iterator i = latches.begin(); i != latches.end();
				++i)
			assumps.push(*i);
		return inits->solve(assumps);
	}

}

/*
 * Un-generalize an unprimed cube which has an intersection
 * with the initial states.
 */
void Model::ungenInitCube(LitVec &genCube, const LitVec &originalCube,
		bool trivial) //trivial is fw PDR ... name should be altered (TODO)
{
	assert(!originalCube.empty());

	if (trivial && invariantConstraints().empty()) //trivial early out
	{
		//only possible for pre 1.9 AIGER models and
		//original PDR
		if (initLits.empty())
			initLits.insert(init.begin(), init.end());
		for (auto &lit : originalCube)
		{
			if (initLits.find(~lit) != initLits.end())
			{
				genCube.push_back(lit);
				sort(genCube.begin(), genCube.end());
				return;
			}
		}
		assert(false);
	}

	MyMinisat::vec<MyMinisat::Lit> assumps;

	if (!trivial && !inits)
	{
		inits = newSolver();
		inits->addClause(error());
		loadError(*inits);
	}

	/*
	 * Check which literals of the original cube are responsible
	 * for disjointness from the initial states.
	 * In case they have been dropped re-push them into the
	 * generalized cube.
	 */
	for (auto &lit : originalCube)
	{
		assumps.push(lit);
	}
	assert(inits);
	bool intersect = inits->solve(assumps);
	assert(!intersect);

	size_t oldszGenCube = genCube.size();
	for (int i = 0; i < inits->conflict.size(); ++i)
	{
		if (!std::binary_search(genCube.begin(), genCube.end(),
				~inits->conflict[i]))
			genCube.push_back(~inits->conflict[i]);
	}
	if (oldszGenCube < genCube.size())
		std::sort(genCube.begin(), genCube.end());
}

//return init literal set
LitVec Model::getInitLits()
{
	return this->init;
}

size_t Model::getMaxVar()
{
	return vars.size();
}

int Model::getLitOcc(const MyMinisat::Lit l)
{
	return litOccurence.at(MyMinisat::toInt(l));
}

void Model::circuitPropagation(std::vector<ternBool> &valuation,
		std::set<unsigned int> &constants)
{
	ternBool l_T((uint8_t) 1);
	ternBool l_F((uint8_t) 0);
	ternBool l_U((uint8_t) 2);

	//calculate ANDs
	for (AigVec::const_iterator i = aig.begin(); i != aig.end(); ++i)
	{
		ternBool rhs0Valuation, rhs1Valuation;
		if (i->rhs0.x > 1)
		{
			if (!MyMinisat::sign(i->rhs0))
				rhs0Valuation = valuation[i->rhs0.x >> 1];
			else
				rhs0Valuation = !valuation[i->rhs0.x >> 1];

			//log this and node as successor of signal rhs0
			if (!circuitNodeSuccessorsActive)
				circuitNodeSuccessors[i->rhs0.x >> 1].insert(i->lhs.x >> 1);
		}
		else
		{
			rhs0Valuation = i->rhs0.x;
		}

		if (i->rhs1.x > 1)
		{
			if (!MyMinisat::sign(i->rhs1))
				rhs1Valuation = valuation[i->rhs1.x >> 1];
			else
				rhs1Valuation = !valuation[i->rhs1.x >> 1];

			//log this and node as successor of signal rhs1
			if (!circuitNodeSuccessorsActive)
				circuitNodeSuccessors[i->rhs1.x >> 1].insert(i->lhs.x >> 1);
		}
		else
		{
			rhs1Valuation = i->rhs1.x;
		}

		valuation[i->lhs.x >> 1] = (rhs0Valuation && rhs1Valuation);
	}

	//calculate latch inputs (next state)
	//negate if there is a last NOT-gate
	for (VarVec::const_iterator it = beginLatches(); it != endLatches(); ++it)
	{
		if (nextStateFn(*it).x > 1)
		{
			if (MyMinisat::sign(nextStateFn(*it)))
			{
				valuation[nextStateFn(*it).x >> 1] =
						!valuation[nextStateFn(*it).x >> 1];
			}
		}
		else
			valuation[nextStateFn(*it).x >> 1] = nextStateFn(*it).x;

		if (valuation[nextStateFn(*it).x >> 1] != l_U)
			constants.insert(it->var());
	}

	if (!circuitNodeSuccessorsActive)
		circuitNodeSuccessorsActive = true;
}

PatternFix Model::simFixRndPatterns(LitVec &presentState)
{
	PatternFix res(endLatches() - beginLatches());
	PatternFix valuation(getMaxVar() + 1);
	//create patterns for each input
	srand(1);
	for (VarVec::const_iterator i = beginInputs(); i != endLatches(); ++i)
	{
		std::bitset<gBitwidth> bitvec;
		for (size_t i = 0; i < gBitwidth; ++i)
			bitvec[i] = rand() % 2;
		valuation[i->var()] = bitvec;
	}
	for (auto &lit : presentState)
	{
		std::bitset<gBitwidth> bitvec;
		for (size_t k = 0; k < gBitwidth; ++k)
			bitvec[k] = !MyMinisat::sign(lit);
		valuation[lit.x >> 1] = bitvec;
	}

	//calculate ANDs
	for (AigVec::const_iterator i = aig.begin(); i != aig.end(); ++i)
	{
		std::bitset<gBitwidth> rhs0Valuation, rhs1Valuation;
		if (i->rhs0.x > 1)
		{
			if (!MyMinisat::sign(i->rhs0))
				rhs0Valuation = valuation[i->rhs0.x >> 1];
			else
				rhs0Valuation = ~(valuation[i->rhs0.x >> 1]);
		}
		else
		{
			//for(size_t k=0; k<gBitwidth;++k)
			//	rhs0Valuation[k] = i->rhs0.x;
			if (!i->rhs0.x)
				rhs0Valuation.reset();
			else
				rhs0Valuation.set();
		}

		if (i->rhs1.x > 1)
		{
			if (!MyMinisat::sign(i->rhs1))
				rhs1Valuation = valuation[i->rhs1.x >> 1];
			else
				rhs1Valuation = ~(valuation[i->rhs1.x >> 1]);
		}
		else
		{
			//for(size_t k=0; k<gBitwidth;++k)
			//	rhs1Valuation[k] = i->rhs1.x;
			if (!i->rhs1.x)
				rhs1Valuation.reset();
			else
				rhs1Valuation.set();
		}

		valuation[i->lhs.x >> 1] = rhs0Valuation & rhs1Valuation;
	}

	//calculate latch inputs (next state)
	//negate if there is a last NOT-gate
	for (VarVec::const_iterator it = beginLatches(); it != endLatches(); ++it)
	{
		if (nextStateFn(*it).x > 1)
		{
			if (MyMinisat::sign(nextStateFn(*it)))
				res[it->var() - beginLatches()->var()] =
						~(valuation[nextStateFn(*it).x >> 1]);
			else
				res[it->var() - beginLatches()->var()] = valuation[nextStateFn(
						*it).x >> 1];
		}
		else
		{
			//for(size_t k=0; k<gBitwidth;++k)
			//	res[it->var()-beginLatches()->var()][k]=nextStateFn(*it).x;
			if (!nextStateFn(*it).x)
				res[it->var() - beginLatches()->var()].reset();
			else
				res[it->var() - beginLatches()->var()].set();
		}
	}
	return res;
}

std::set<unsigned int> Model::circuitTrav(std::vector<ternBool> &valuation,
		unsigned int stateVar)
{
	ternBool l_T((uint8_t) 1);
	ternBool l_F((uint8_t) 0);
	ternBool l_U((uint8_t) 2);

	std::set<unsigned int> irrVarInFnSupp;

	MyMinisat::Lit nxt = nextStateFn(varOfLit(MyMinisat::mkLit(stateVar, false)));

	//if nxt is constant -> decrement all non-assigned variable outdegrees?
	if (valuation[nxt.x >> 1] != l_U)
	{
		std::vector<unsigned int> supp = getTransitionSupport(
				varOfLit(MyMinisat::mkLit(stateVar, false)));
		//assert(supp.size()>1);
		for (unsigned int i = 0; i < supp.size(); ++i)
		{
			//only unassigned
			if (valuation[supp[i]] != l_U)
				continue;
			else
			{
				//irrelevant support variable
				irrVarInFnSupp.insert(supp[i]);
			}
		}
		return irrVarInFnSupp;
	}

	//find state variables with only irrelevant "fanout"
	//calculate ANDs - start at next state of this latch
	std::vector<bool> irrelevantVars(this->getMaxVar() + 1, false);
	if (MyMinisat::var(nxt) >= endLatches()->var())
	{
		AigVec::const_reverse_iterator it = aig.rend()
				- (MyMinisat::var(nxt) - (aig.begin()->lhs.x >> 1) + 1);
		assert(MyMinisat::var(it->lhs) == MyMinisat::var(nxt));
		LitSet require;
		require.insert(nxt);
		for (AigVec::const_reverse_iterator i = it; i != aig.rend(); ++i)
		{
			if (require.find(i->lhs) == require.end()
					&& require.find(~i->lhs) == require.end())
			{
				continue;
			}

			//already "irrelevant"?
			if (valuation[i->lhs.x >> 1] != l_U
					|| irrelevantVars[i->lhs.x >> 1])
			{
				if (i->rhs0.x > 1 && (i->rhs0.x >> 1) >= endLatches()->var())
					require.insert(i->rhs0);
				if (i->rhs1.x > 1 && (i->rhs1.x >> 1) >= endLatches()->var())
					require.insert(i->rhs1);
				continue;
			}

			bool irrelevant = true;
			if (MyMinisat::var(i->lhs) == MyMinisat::var(nxt))
				irrelevant = false;
			else
			{
				for (auto &succ : circuitNodeSuccessors[i->lhs.x >> 1])
				{
					if (valuation[succ] == l_U && !irrelevantVars[succ])
					{
						if (require.find(MyMinisat::mkLit(succ, false))
								!= require.end()
								|| require.find(MyMinisat::mkLit(succ, true))
										!= require.end())
						{
							irrelevant = false;
							break;
						}
					}
				}
			}

			//if irrelevant -> use constant valuation as helper
			//(semantically incorrect but sufficient)
			if (irrelevant)
			{
				irrelevantVars[i->lhs.x >> 1] = true;
			}

			if (i->rhs0.x > 1 && (i->rhs0.x >> 1) >= endLatches()->var())
				require.insert(i->rhs0);
			if (i->rhs1.x > 1 && (i->rhs1.x >> 1) >= endLatches()->var())
				require.insert(i->rhs1);
		}

		//which (unassigned) present state variables are no longer in transition support?
		std::vector<unsigned int> supp = getTransitionSupport(
				varOfLit(MyMinisat::mkLit(stateVar, false)));
		//assert(supp.size()>1);
		for (unsigned int i = 0; i < supp.size(); ++i)
		{
			//only unassigned
			if (valuation[supp[i]] != l_U)
				continue;
			bool irrelevant = true;
			for (auto &succ : circuitNodeSuccessors[supp[i]])
			{
				if (valuation[succ] == l_U && !irrelevantVars[succ])
				{
					if (require.find(MyMinisat::mkLit(succ, false))
							!= require.end()
							|| require.find(MyMinisat::mkLit(succ, true))
									!= require.end())
					{

						irrelevant = false;
						break;
					}
				}
			}
			if (irrelevant)
			{
				//irrelevant support variable
				irrVarInFnSupp.insert(supp[i]);
			}
		}
	}
	valuation.clear();
	return irrVarInFnSupp;
}

std::vector<unsigned int>& Model::getTransitionSupport(Var v)
{

	if (transitionSupport[v.var() - beginLatches()->var()].size() > 0)
	{
		return transitionSupport[v.var() - beginLatches()->var()];
	}
	else
	{
		LitSet require;  // unprimed formulas
		std::vector<unsigned int> support;
		if ((nextStateFn(v).x >> 1) < endLatches()->var()
				&& nextStateFn(v).x > 1)
		{
			support.push_back(nextStateFn(v).x >> 1);
		}
		else
		{
			if (MyMinisat::var(nextStateFn(v)) >= endLatches()->var())
			{
				require.insert(nextStateFn(v));
				AigVec::const_reverse_iterator it = aig.rend()
						- (MyMinisat::var(nextStateFn(v))
								- (aig.begin()->lhs.x >> 1) + 1);
				assert(MyMinisat::var(it->lhs) == MyMinisat::var(nextStateFn(v)));
				// traverse AIG backward
				for (AigVec::const_reverse_iterator i = it; i != aig.rend();
						++i)
				{
					// skip if this row is not required
					if (require.find(i->lhs) == require.end()
							&& require.find(~i->lhs) == require.end())
						continue;

					if ((i->rhs0.x >> 1) < endLatches()->var() && i->rhs0.x > 1)
					{
						support.push_back(i->rhs0.x >> 1); //store vars in support set
					}
					else
						require.insert(i->rhs0);

					if ((i->rhs1.x >> 1) < endLatches()->var() && i->rhs1.x > 1)
					{
						support.push_back(i->rhs1.x >> 1); //store vars in support set
					}
					else
						require.insert(i->rhs1);
				}
				std::sort(support.begin(), support.end());
			}

		}

		//exploit that support is sorted -
		//avoid counting the same supp var twice
		for (unsigned int i = 0; i < support.size(); ++i)
		{
			if (i > 0)
			{
				if (support[i] != support[i - 1])
					outDegrSuppVars[support[i]]++;
			}
			else
				outDegrSuppVars[support[i]]++;
		}
		transitionSupport[v.var() - beginLatches()->var()] = support;
		return transitionSupport[v.var() - beginLatches()->var()];
	}
}

typedef std::pair<uint64_t, uint64_t> ternsimVal;
//pair.first: 1-vector, pair.second: 0-vector -> (1,0) = 1, (0,1) = 0, (0,0) = X
void Model::ternSim(ternsimVec &valuation, MyMinisat::Var maxVar)
{
	//calculate ANDs
	for (AigVec::const_iterator i = aig.begin(); i != aig.end(); ++i)
	{
		//only calculate relevant next state functions
		if (MyMinisat::var(i->lhs) > maxVar)
			break;

		ternsimVal rhs0Valuation, rhs1Valuation;
		//if (i->rhs0.x > 1)
		{
			if (!MyMinisat::sign(i->rhs0))
				rhs0Valuation = valuation[MyMinisat::var(i->rhs0)];
			else
			{
				//negation: swap 0- and 1-vector
				rhs0Valuation.first = (valuation[MyMinisat::var(i->rhs0)]).second;
				rhs0Valuation.second = (valuation[MyMinisat::var(i->rhs0)]).first;
			}
		}
		/*else
		 {
		 if (i->rhs0.x == 0)
		 {	//constant 0 ((00...0),(11...1))
		 rhs0Valuation.first = 0;//.reset();
		 rhs0Valuation.second = UINT64_MAX;//.set();
		 }
		 else
		 {	//constant 1 ((11...1),(00...0))
		 rhs0Valuation.first = UINT64_MAX;//.set();
		 rhs0Valuation.second = 0;//.reset();
		 }
		 }*/

		//if (i->rhs1.x > 1)
		{
			if (!MyMinisat::sign(i->rhs1))
				rhs1Valuation = valuation[MyMinisat::var(i->rhs1)];
			else
			{
				//negation: swap 0- and 1-vector
				rhs1Valuation.first = (valuation[MyMinisat::var(i->rhs1)]).second;
				rhs1Valuation.second = (valuation[MyMinisat::var(i->rhs1)]).first;
			}
		}
		/*else
		 {
		 if (i->rhs1.x == 0)
		 {	//constant 0 ((00...0),(11...1))
		 rhs1Valuation.first = 0;//.reset();
		 rhs1Valuation.second = UINT64_MAX;//.set();
		 }
		 else
		 {	//constant 1 ((11...1),(00...0))
		 rhs1Valuation.first = UINT64_MAX;//.set();
		 rhs1Valuation.second = 0;//.reset();
		 }
		 }*/

		//AND: 1-vector & 1-vector, 0-vector | 0-vector
		valuation[MyMinisat::var(i->lhs)].first = rhs0Valuation.first
				& rhs1Valuation.first;
		valuation[MyMinisat::var(i->lhs)].second = rhs0Valuation.second
				| rhs1Valuation.second;
	}
}

void Model::loadNegTrans(MyMinisat::Solver &negSlv)
{
	/*
	 * Precondition: Solver with aligned variables. Variable
	 * Elimination happens after the call of this function.
	 *
	 * This function negates the TR clauses via PG-Transformation
	 * it is not necessary to assert a top level tseitin-Variable
	 * since it is translated with one polarity only.
	 * ~((l_00 + ... + l_0i) * (l_10 + ... + l_1j) * ...)
	 *     auxCl										auxOppCl
	 * (aux_0 + ~l_00) * (aux_0 + ~l_01) * ... * (~aux_0 + l_00 + l_01 ...) *
	 * ... *
	 * (~aux_0 + ~aux_1 + ... )
	 * 		  clAuxLits
	 */

	if (!sslvReduced)
	{
		// create a simplified CNF version of (this slice of) the TR
		sslvReduced = new MyMinisat::SimpSolver();
		// introduce all variables to maintain alignment
		for (size_t i = 0; i < vars.size(); ++i)
		{
			MyMinisat::Var nv = sslvReduced->newVar();
			assert(nv == vars[i].var());
		}
		// freeze inputs, latches, and special nodes (and primed forms)
		for (VarVec::const_iterator i = beginInputs(); i != endInputs(); ++i)
		{
			sslvReduced->setFrozen(i->var(), true);
			sslvReduced->setFrozen(primeVar(*i).var(), true);
		}
		for (VarVec::const_iterator i = beginLatches(); i != endLatches(); ++i)
		{
			sslvReduced->setFrozen(i->var(), true);
			sslvReduced->setFrozen(primeVar(*i).var(), true);
			sslvReduced->setFrozen(MyMinisat::var(nextStateFn(*i)), true);
		}
		sslvReduced->setFrozen(varOfLit(error()).var(), true);
		sslvReduced->setFrozen(varOfLit(primedError()).var(), true);
		for (LitVec::const_iterator i = constraints.begin();
				i != constraints.end(); ++i)
		{
			Var v = varOfLit(*i);
			sslvReduced->setFrozen(v.var(), true);
			sslvReduced->setFrozen(primeVar(v).var(), true);
		}
		// initialize with roots of required formulas
		LitSet require;  // unprimed formulas
		for (VarVec::const_iterator i = beginLatches(); i != endLatches(); ++i)
			require.insert(nextStateFn(*i));
		//require.insert(_error);
		require.insert(constraints.begin(), constraints.end());
		/*LitSet prequire; // for primed formulas; always subset of require
		 prequire.insert(_error);
		 prequire.insert(constraints.begin(), constraints.end());*/

		// traverse AIG backward
		for (AigVec::const_reverse_iterator i = aig.rbegin(); i != aig.rend();
				++i)
		{
			// skip if this row is not required
			if (require.find(i->lhs) == require.end()
					&& require.find(~i->lhs) == require.end())
				continue;
			// encode into CNF
			sslvReduced->addClause(~i->lhs, i->rhs0);
			sslvReduced->addClause(~i->lhs, i->rhs1);
			sslvReduced->addClause(~i->rhs0, ~i->rhs1, i->lhs);
			// require arguments
			require.insert(i->rhs0);
			require.insert(i->rhs1);
			// primed: skip if not required
			/*if (prequire.find(i->lhs) == prequire.end()
			 && prequire.find(~i->lhs) == prequire.end())
			 continue;
			 // encode PRIMED form into CNF
			 MyMinisat::Lit r0 = primeLit(i->lhs, sslvReduced), r1 = primeLit(
			 i->rhs0, sslvReduced), r2 = primeLit(i->rhs1, sslvReduced);
			 sslvReduced->addClause(~r0, r1);
			 sslvReduced->addClause(~r0, r2);
			 sslvReduced->addClause(~r1, ~r2, r0);
			 // require arguments
			 prequire.insert(i->rhs0);
			 prequire.insert(i->rhs1);*/
		}
		//sslvReduced->addClause(btrue());
		//sslvReduced->addClause(~_error);
		// assert literal for true

		// assert l' = f for each latch l
		/*for (VarVec::const_iterator i = beginLatches(); i != endLatches(); ++i)
		 {
		 MyMinisat::Lit platch = primeLit(i->lit(false)), f = nextStateFn(*i);
		 sslvReduced->addClause(~platch, f);
		 sslvReduced->addClause(~f, platch);
		 }*/
		sslvReduced->eliminate(true);
	}

	while (negSlv.nVars() < sslvReduced->nVars())
	{
		negSlv.newVar();
	}

	for (VarVec::const_iterator i = beginLatches(); i != endLatches(); ++i)
	{
		MyMinisat::Lit platch = primeLit(i->lit(false)), f = nextStateFn(*i);
		negSlv.addClause(~platch, f);
		negSlv.addClause(~f, platch);
	}
	// assert literal for true
	negSlv.addClause(btrue());

	MyMinisat::vec<MyMinisat::Lit> clAuxLits;

	assert(sslvReduced);
	for (MyMinisat::ClauseIterator c = sslvReduced->clausesBegin();
			c != sslvReduced->clausesEnd(); ++c)
	{
		const MyMinisat::Clause &cl = *c;
		assert(cl.size() > 1);

		MyMinisat::Var clAuxVar = negSlv.newVar();
		MyMinisat::Lit clAuxLit = MyMinisat::mkLit(clAuxVar, false);
		clAuxLits.push(~clAuxLit);

		//MyMinisat::vec<MyMinisat::Lit> auxOppCl; // (~aux + lit_1 + lit_2 + ...)
		//MyMinisat::vec<MyMinisat::Lit> fullcl;
		//auxOppCl.push(~clAuxLit);
		for (int j = 0; j < cl.size(); ++j)
		{
			const MyMinisat::Lit lit = cl[j];
			//fullcl.push(lit);
			MyMinisat::vec<MyMinisat::Lit> auxCl; // (aux + ~lit_i)
			auxCl.push(~lit);
			auxCl.push(clAuxLit);
			negSlv.addClause(auxCl);

			//auxOppCl.push(lit);
		}
		//negSlv.addClause(fullcl); //preserve circuit implications
		//negSlv.addClause(auxOppCl);
	}

	for (MyMinisat::TrailIterator c = sslvReduced->trailBegin();
			c != sslvReduced->trailEnd(); ++c)
	{
		clAuxLits.push(~(*c));
	}

	negSlv.addClause(clAuxLits); // (~aux_1 + ~aux_2 + ... + ~units )*/
}

// Creates a named variable.
Var var(const aiger_symbol *syms, size_t i, const char prefix,
		MyMinisat::Var &gvi, bool prime = false)
{
	const aiger_symbol &sym = syms[i];
	stringstream ss;
	if (sym.name)
		ss << sym.name;
	else
		ss << prefix << i;
	if (prime)
		ss << "'";
	return Var(ss.str(), gvi);
}

MyMinisat::Lit lit(const VarVec &vars, unsigned int l)
{
	return vars[l >> 1].lit(aiger_sign(l));
}

Model* modelFromAiger(aiger *aig, unsigned int propertyIndex)
{
	MyMinisat::Var gvi = 0;
	VarVec vars(1, Var("false", gvi));
	LitVec init, constraints, nextStateFns;

	// declare primary inputs and latches
	for (size_t i = 0; i < aig->num_inputs; ++i)
		vars.push_back(var(aig->inputs, i, 'i', gvi));
	for (size_t i = 0; i < aig->num_latches; ++i)
		vars.push_back(var(aig->latches, i, 'l', gvi));

	// the AND section
	AigVec aigv;
	for (size_t i = 0; i < aig->num_ands; ++i)
	{
		// 1. create a representative
		stringstream ss;
		ss << 'r' << i;
		vars.push_back(Var(ss.str(), gvi));
		const Var &rep = vars.back();
		// 2. obtain arguments of AND as lits
		MyMinisat::Lit l0 = lit(vars, aig->ands[i].rhs0);
		MyMinisat::Lit l1 = lit(vars, aig->ands[i].rhs1);
		// 3. add AIG row
		aigv.push_back(AigRow(rep.lit(false), l0, l1));
	}

	// acquire latches' initial states and next-state functions
	for (size_t i = 0; i < aig->num_latches; ++i)
	{
		const Var &latch = vars[1 + aig->num_inputs + i];
		// initial condition
		unsigned int r = aig->latches[i].reset;
		if (r < 2)
			init.push_back(latch.lit(r == 0));
		// next-state function
		nextStateFns.push_back(lit(vars, aig->latches[i].next));
	}

	// invariant constraints
	for (size_t i = 0; i < aig->num_constraints; ++i)
		constraints.push_back(lit(vars, aig->constraints[i].lit));

	// acquire error from given propertyIndex
	if ((aig->num_bad > 0 && aig->num_bad <= propertyIndex)
			|| (aig->num_outputs > 0 && aig->num_outputs <= propertyIndex))
	{
		cout << "Bad property index specified." << endl;
		return 0;
	}
	MyMinisat::Lit err =
			aig->num_bad > 0 ?
					lit(vars, aig->bad[propertyIndex].lit) :
					lit(vars, aig->outputs[propertyIndex].lit);

	//if there is the necessity for invariant constraint self-loops
	MyMinisat::Lit nand;
	if (aig->num_outputs > 0 && aig->num_constraints > 0)
	{
		nand = lit(vars, aig->outputs[aig->num_outputs - 1].lit);
	}

	size_t offset = 0;
	return new Model(vars, offset += 1, offset += aig->num_inputs,
			offset + aig->num_latches, init, constraints, nextStateFns, err,
			aigv, nand, gvi);
}

void introduceSelfLoops(aiger *aig)
{
	//big NAND over all constraints
	unsigned rhs1 = aig->constraints[0].lit;
	unsigned lhs = 2 * (aig->maxvar + 1);
	if (aig->num_constraints > 1)
	{
		for (size_t i = 1; i < (aig->num_constraints); ++i)
		{
			lhs = 2 * (aig->maxvar + 1);
			aiger_add_and(aig, lhs, aig->constraints[i].lit, rhs1);
			rhs1 = lhs;
		}
		aiger_add_output(aig, lhs + 1, "NAND constraints"); //this is supposed to be the last output (highest index)
	}
	else
	{
		assert(aig->num_constraints == 1);
		//negate constraint c if there is only one (lhs = c & c)
		lhs = 2 * (aig->maxvar + 1);
		aiger_add_and(aig, lhs, rhs1, rhs1);
		aiger_add_output(aig, lhs + 1, "NAND constraints"); //this is supposed to be the last output (highest index)
	}
	assert(aig->outputs[aig->num_outputs - 1].lit == (lhs + 1));
	//cout << "NAND lit: " << aig->outputs[aig->num_outputs-1].lit << endl;
	//multiplexers for self loops
	//if constraints are false, i.e. NAND is true (output uneven)
	//then select original latch value
	//else select next state function
	for (size_t i = 0; i < aig->num_latches; ++i)
	{
		unsigned lhs1 = 2 * (aig->maxvar + 1);
		aiger_add_and(aig, lhs1, aig->latches[i].lit,
				aig->outputs[aig->num_outputs - 1].lit);
		unsigned lhs2 = 2 * (aig->maxvar + 1);
		aiger_add_and(aig, lhs2, aig->latches[i].next,
				aig->outputs[aig->num_outputs - 1].lit - 1);
		lhs = 2 * (aig->maxvar + 1);
		aiger_add_and(aig, lhs, lhs1 + 1, lhs2 + 1);

		//negate output!
		aig->latches[i].next = lhs + 1;
	}

	//reencode for correct topological order
	aiger_reencode(aig);
}

void Model::assignValues(const MyMinisat::Solver *modelSlv, LitVec &model, bool primeRun)
{
	//get latch values
	assert(model.size() == vars.size());
	model[0] = MyMinisat::mkLit(0,true);
	//collect CIs from solver - model
	if(primeRun)
	{
		for(VarVec::const_iterator v = beginInputs(); v != endLatches(); ++v)
		{
			model[v->var()] = v->lit(modelSlv->modelValue(primeVar(*v).var()) == MyMinisat::l_False);
		}
	}
	else
	{
		for(VarVec::const_iterator v = beginInputs(); v != endLatches(); ++v)
		{
			model[v->var()] = v->lit(modelSlv->modelValue(v->var()) == MyMinisat::l_False);
		}
	}

	// traverse AIG forward
	for (AigVec::const_iterator i = aig.begin(); i != aig.end(); ++i)
	{
		MyMinisat::Var lhsVar = MyMinisat::var(i->lhs);
		MyMinisat::Var rhs0Var = MyMinisat::var(i->rhs0);
		MyMinisat::Var rhs1Var = MyMinisat::var(i->rhs1);

		bool value0 = !MyMinisat::sign(model[rhs0Var]) != MyMinisat::sign(i->rhs0);
		bool value1 = !MyMinisat::sign(model[rhs1Var]) != MyMinisat::sign(i->rhs1);

		bool valueLhs = value1 && value0;

		model[lhsVar] = MyMinisat::mkLit(lhsVar, !valueLhs);

		//cout << model[lhsVar].x << " = " << (MyMinisat::sign(i->rhs0) ? "~" : "") << model[rhs0Var].x << " & " << (MyMinisat::sign(i->rhs1) ? "~" : "") << model[rhs1Var].x << endl;
	}

}

void Model::assignValues(const LitVec *assignment, LitVec &model, bool primeRun)
{
	//get latch values
	assert(model.size() == vars.size());
	model[0] = MyMinisat::mkLit(0,true);
	//collect CIs from solver - model
	if(primeRun)
	{
		for(VarVec::const_iterator v = beginInputs(); v != endLatches(); ++v)
		{
			model[v->var()] = assignment->at(primeVar(*v).var());
		}
	}
	else
	{
		for(VarVec::const_iterator v = beginInputs(); v != endLatches(); ++v)
		{
			model[v->var()] = assignment->at(v->var());
		}
	}

	// traverse AIG forward
	for (AigVec::const_iterator i = aig.begin(); i != aig.end(); ++i)
	{
		MyMinisat::Var lhsVar = MyMinisat::var(i->lhs);
		MyMinisat::Var rhs0Var = MyMinisat::var(i->rhs0);
		MyMinisat::Var rhs1Var = MyMinisat::var(i->rhs1);

		bool value0 = !MyMinisat::sign(model[rhs0Var]) != MyMinisat::sign(i->rhs0);
		bool value1 = !MyMinisat::sign(model[rhs1Var]) != MyMinisat::sign(i->rhs1);

		bool valueLhs = value1 && value0;

		model[lhsVar] = MyMinisat::mkLit(lhsVar, !valueLhs);

		//cout << model[lhsVar].x << " = " << (MyMinisat::sign(i->rhs0) ? "~" : "") << model[rhs0Var].x << " & " << (MyMinisat::sign(i->rhs1) ? "~" : "") << model[rhs1Var].x << endl;
	}

}

void Model::findJustPath(AigVec &justPath, set<MyMinisat::Var> &justCI,
		const LitVec *succ, const LitVec &model) const
{
	LitSet require;
	if(succ)
	{
		for (auto &lit : *succ)
		{
			require.insert(nextStateFn(varOfLit(lit)));
			//special case: next state function is direct connection to other latch ...
			if(isLatch(MyMinisat::var(nextStateFn(varOfLit(lit)))))
				justCI.insert(MyMinisat::var(nextStateFn(varOfLit(lit))));
		}
	}
	else
	{
		require.insert(error());
		//special case: error is latch...
		if(isLatch(MyMinisat::var(error())))
			justCI.insert(MyMinisat::var(error()));
	}

	// traverse AIG backward
	for (AigVec::const_reverse_iterator i = aig.rbegin(); i != aig.rend(); ++i)
	{
		// skip if this row is not required
		if (require.find(i->lhs) == require.end()
				&& require.find(~i->lhs) == require.end())
			continue;

		justPath.push_back(*i); //insert AIG line

		MyMinisat::Var rhs0var = MyMinisat::var(i->rhs0);
		MyMinisat::Var rhs1var = MyMinisat::var(i->rhs1);

		bool value = !MyMinisat::sign(model[MyMinisat::var(i->lhs)]);

		if (value) //there is a 1 at the AND-output
		{
			if (isLatch(rhs0var))
				justCI.insert(rhs0var);
			else
				require.insert(i->rhs0);

			if (isLatch(rhs1var))
				justCI.insert(rhs1var);
			else
				require.insert(i->rhs1); //we require both inputs to justify the 1
		}
		else //there is a 0 at the AND output
		{
			//we require at least one 0 input
			bool rhs0_model_val = !MyMinisat::sign(model[rhs0var]);
			bool rhs1_model_val = !MyMinisat::sign(model[rhs1var]);
			bool value0 = (MyMinisat::sign(i->rhs0) != rhs0_model_val);
			bool value1 = (MyMinisat::sign(i->rhs1) != rhs1_model_val);
			assert(!value0 || !value1);
			if (value0)
			{
				if (isLatch(rhs1var))
					justCI.insert(rhs1var);
				else
					require.insert(i->rhs1); //only rhs1 input is 0 -> the only one required for justification
			}
			else if (value1)
			{
				if (isLatch(rhs0var))
					justCI.insert(rhs0var);
				else
					require.insert(i->rhs0); //only rhs0 input is 0
			}
			else
			{
				//both are 0
				assert(!value0 && !value1);
				if (isLatch(rhs0var))
					justCI.insert(rhs0var);
				else
					require.insert(i->rhs0);

				if (isLatch(rhs1var))
					justCI.insert(rhs1var);
				else
					require.insert(i->rhs1);
			}
		}
	}
}

int Model::propagateJustPrio(AigVec &justPath, set<MyMinisat::Var> &justCI,
		const LitVec *succ, const LitVec &model,
		vector<int> &prio) const
{
	//prioritize paths that do not originate in latches
	set<MyMinisat::Var> nextStFns;
	int minPrio = -1;

	if(succ)
	{
		for(auto& lit: *succ)
		{
			MyMinisat::Var nf = MyMinisat::var(nextStateFn(varOfLit(lit)));
			nextStFns.insert(nf);
			if(isLatch(nf)) //special case, next state function is direct connection to latch
			{
				if(prio[nf] != INT_MAX)
				{
					if(minPrio == -1 || prio[nf] < minPrio)
						minPrio = prio[nf];
				}
			}
		}
	}
	else
	{
		MyMinisat::Var nf = MyMinisat::var(error());
		nextStFns.insert(nf);
		if(isLatch(nf)) //special case, next state function is direct connection to latch
		{
			if(prio[nf] != INT_MAX)
			{
				if(minPrio == -1 || prio[nf] < minPrio)
					minPrio = prio[nf];
			}
		}
	}

	//"reverse" is a forward pass in this case, since we built it in a reverted direction
	for (AigVec::const_reverse_iterator i = justPath.rbegin();
			i != justPath.rend(); ++i)
	{
		bool value = !MyMinisat::sign(model[MyMinisat::var(i->lhs)]);

		MyMinisat::Var rhs0var = MyMinisat::var(i->rhs0);
		MyMinisat::Var rhs1var = MyMinisat::var(i->rhs1);


		if (value) //there is a 1 at the AND-output
		//choose minimum of incoming priorities
		{
			if (prio[rhs0var] < prio[rhs1var])
				//both come from a necessary latch ... take minimum
				prio[MyMinisat::var(i->lhs)] = prio[rhs0var];
			else
				prio[MyMinisat::var(i->lhs)] = prio[rhs1var];
		}
		else
		{
			//there is a 0 at the AND-output
			//both 0: choose maximum of incoming priorities
			//we require at least one 0 input -> differentiate again
			bool rhs0_model_val = !MyMinisat::sign(model[rhs0var]);
			bool rhs1_model_val = !MyMinisat::sign(model[rhs1var]);
			bool value0 = (MyMinisat::sign(i->rhs0) != rhs0_model_val);
			bool value1 = (MyMinisat::sign(i->rhs1) != rhs1_model_val);
			assert(!value0 || !value1);
			if (value0) //rhs0 is one -> rhs1 input is zero - take its priority
				prio[MyMinisat::var(i->lhs)] = prio[rhs1var];
			else if (value1) //rhs1 is one -> rhs0 input is zero ...
				prio[MyMinisat::var(i->lhs)] = prio[rhs0var];
			else if (prio[rhs0var] >= prio[rhs1var])
				//both zero - take maximum prio - hopefully INT_MAX ...
				prio[MyMinisat::var(i->lhs)] = prio[rhs0var];
			else
				prio[MyMinisat::var(i->lhs)] = prio[rhs1var];
		}

		if(nextStFns.find(MyMinisat::var(i->lhs))!=nextStFns.end()) //if CO, check out prio
		{
			assert(MyMinisat::var(i->lhs) > 0);
			if(prio[MyMinisat::var(i->lhs)] != INT_MAX)
			{
				if(minPrio == -1 || prio[MyMinisat::var(i->lhs)] < minPrio)
					minPrio = prio[MyMinisat::var(i->lhs)];
			}
		}

		assert(prio[MyMinisat::var(i->lhs)] > 0);
	}
	return minPrio;

}

