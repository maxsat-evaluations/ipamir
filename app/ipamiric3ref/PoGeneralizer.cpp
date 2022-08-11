/*
 * PoGeneralizer.cpp
 *
 *  Created on: Jan 1, 2021
 *      Author: seufert
 */

#include "PoGeneralizer.h"

namespace IC3
{

PoGeneralizer::PoGeneralizer(GenType _genType, Model &_model, bool _revpdr,
		GeneralOpt &_genOpt, SlimLitOrder *_order, LitVec *_inputs,
		LitVec *_pred, LitVec *_succ, vector<LitVec> *_frCubes,
		LitVec *_fullAssignment, MyMinisat::Solver *_modelSlv,
		bool _deletePointers) :
		genOpt(_genOpt),
		revpdr(_revpdr),
		modelSlv(_modelSlv),
		genType(_genType),
		inputs(_inputs),
		pred(_pred),
		succ(_succ),
		frCubes(_frCubes),
		model(_model),
		fullAssignment(_fullAssignment),
		maxSatSlv(NULL),
		greedyQbfSlv(NULL),
		ternSatSlv(NULL),
		igbgSatSlvSucc(NULL),
		igbgSatSlvBad(NULL),
		coverSatSlv(NULL),
		liftingSatSlv(NULL),
		GeNTRSlv(NULL),
		maxQbfSlv(NULL),
		maxSatSlvIpamir(NULL),
		deletePointers(_deletePointers),
		freeMode(false),
		ic3refLitOrder(_order)
{
	switch (genType)
	{
	case NONE:
		break;
	case LIFTING:
	case LIFTING_LITDROP:
		initLifting();
		break;
	case S01X_FIX:
		initS01X();
		break;
	case S01X_FREE:
		initS01X();
		freeMode = true;
		break;
	case MS01X_FIX:
		initMS01X();
		break;
	case MS01X_FREE:
		initMS01X();
		freeMode = true;
		break;
	case MS01X_INCREMENTAL:
		initMS01XIncremental();
		break;
	case IGBG:
	case IGBG_LITDROP:
		initIGBG();
		break;
	case GENTR:
	case GENTR_LITDROP:
		initGeNTR();
		break;
	case SAT_COVER_FIX:
		initSATCover();
		break;
	case SAT_COVER_FREE:
		initSATCover();
		freeMode = true;
		break;
	case GREEDY_QBF_FIX:
		initGreedyQBF();
		break;
	case GREEDY_QBF_FREE:
		initGreedyQBF();
		freeMode = true;
		break;
	case MAXQBF_FIX:
		initMaxQBF();
		break;
	case MAXQBF_FREE:
		initMaxQBF();
		freeMode = true;
		break;
	case ILP_COVER:
		initILP();
		break;
	case ILP_COVER_FREE:
		initILP();
		freeMode = true;
		break;
	case MS_HEURISTICS:
		initMS01X();
		initIGBG();
		break;
	case IGBG_LIFTING_HEURISTICS:
		initLifting();
		initIGBG();
		break;
	case JUSTIFICATION:
		initJust();
		break;
	case TERNSIM:
		//nothing to initialize ...
		break;
	case REV_STRUCT:
		initRevStruct();
		break;
	default:
		throw std::runtime_error(
				"PO-Generalization: Inadmissible mode of operation.");
		break;
	}

}

PoGeneralizer::~PoGeneralizer()
{
	if (maxSatSlv)
		deinitMaxSAT();
	if (maxSatSlvIpamir)
		deinitMaxSATIncremental();
	if (greedyQbfSlv)
		qdpll_delete( greedyQbfSlv );
	if (ternSatSlv)
		delete ternSatSlv;
	if (igbgSatSlvSucc && igbgSatSlvBad)
	{
		delete igbgSatSlvSucc;
		delete igbgSatSlvBad;
	}
	if (coverSatSlv)
		delete coverSatSlv;
	if (liftingSatSlv)
		delete liftingSatSlv;
	if (GeNTRSlv)
		delete GeNTRSlv;
	if (maxQbfSlv)
		delete maxQbfSlv;

	if (deletePointers)
	{
		delete inputs;
		delete pred;
		delete succ;
		delete frCubes;
		delete fullAssignment;
		assert(!modelSlv);
	}
}

void PoGeneralizer::initLifting()
{
	liftingSatSlv = model.newSolver();
	model.loadTransitionRelationLifting(*liftingSatSlv);
	notInvConstraints = MyMinisat::mkLit(liftingSatSlv->newVar());
	MyMinisat::vec<MyMinisat::Lit> cls;
	cls.push(~notInvConstraints);
	for (LitVec::const_iterator i = model.invariantConstraints().begin();
			i != model.invariantConstraints().end(); ++i)
		cls.push(model.primeLit(~*i));
	liftingSatSlv->addClause_(cls);
	if(genOpt.lift.extCall) //extended call to enable lifting with invariant constraints
	{
		cls.clear();
		notUnprimedInvConstraints = MyMinisat::mkLit(liftingSatSlv->newVar());
		cls.push(~notUnprimedInvConstraints);
		for (LitVec::const_iterator i = model.invariantConstraints().begin();
				i != model.invariantConstraints().end(); ++i)
			cls.push(~*i);
		liftingSatSlv->addClause_(cls);
	}
}

void PoGeneralizer::initS01X()
{
	ternSatSlv = model.newTernSatSlv();
	model.loadTransitionRelation01XElim(*ternSatSlv);
}

void PoGeneralizer::initMS01X()
{
	if (dontCares.size() > 0)
		maxSatSlv = model.newPacose(&dontCares);
	else
		maxSatSlv = model.newPacose();

	model.loadTransitionRelation01XPacoseElim(*maxSatSlv);
}


void PoGeneralizer::initMS01XIncremental()
{
	maxSatSlvIpamir = model.newIpamirSolver();

	model.loadTransitionRelation01XIpamir(maxSatSlvIpamir);
}

void PoGeneralizer::initIGBG()
{
	//set options TODO

	igbgSatSlvSucc = model.newSolver();
	igbgSatSlvBad = model.newSolver();

	model.loadTransitionRelationImplgraphSucc(*igbgSatSlvSucc, genOpt.igbg.respectProperty);
	model.loadTransitionRelationImplgraphBad(*igbgSatSlvBad);
	//model.loadTransitionRelationImplgraph(*igbgSatSlv);
	for (int var = 0; var < igbgSatSlvSucc->nVars(); ++var)
	{
		igbgSatSlvSucc->setDecisionVar(var, false); //no decisions
		igbgSatSlvBad->setDecisionVar(var, false); //no decisions
	}
}

void PoGeneralizer::initGeNTR()
{
	GeNTRSlv = model.newSolver();
	model.loadNegTrans(*GeNTRSlv);
}

void PoGeneralizer::initSATCover()
{
	coverSatSlv = model.newTernSatSlv();
	model.loadTransitionRelationCover01XElim(*coverSatSlv);
}

void PoGeneralizer::initGreedyQBF()
{
	greedyQbfSlv = model.newDepQBF(muxAuxExist, muxAuxForall, muxSelect,
			revpdr);
	model.loadTransitionRelationDepQBF(greedyQbfSlv);
}

void PoGeneralizer::initMaxQBF()
{
	maxQbfSlv = new quantom::Quantom();
	maxQbfSlv->setVerbosity(0);
	/*maxQbfSlv->setPreprocessor(false);
	 maxQbfSlv->setVarElim(false);
	 maxQbfSlv->setInprocessor(false);
	 maxQbfSlv->setQuantMerge(false);
	 maxQbfSlv->setSimplifyLBD(false);
	 qSlv->setUPLA(false);*/
	assert(muxSelect.empty());
	assert(muxAuxExist.empty());
	int nclauses = 0;

	//TODO: align variables!!! correct levels!!
	MyMinisat::SimpSolver *transSlv = model.getTransSslv();
	assert(transSlv);

	for (int i = 0; i < transSlv->nVars(); ++i)
	{
		unsigned int nv;
		nv = maxQbfSlv->newVariable(3);
		maxQbfSlv->setDontTouch(nv);
	}
	model.loadTransitionRelationQuantom(*maxQbfSlv);

	assert(model.invariantConstraints().empty());

	//variables which aren't quantified are implicitly existentially quantified
	//in the outermost quantifier level (*QDIMACS standard)

	//introduce multiplexer vars (f_1 ... f_n) and forall
	//quantified variables (r_1 ... r_n) as well as clauses for
	// f_1 -> (r_1 <-> s'_1) i.e. if f_1 is added as soft clause,
	//maximizing the number of satisfied f_i maximizes the number
	//of forall quantified next-state variables.
	for (VarVec::const_iterator i = model.beginLatches();
			i != model.endLatches(); ++i)
	{
		assert(model.primeVar(*i).var() > 1);
		int f_i = maxQbfSlv->newVariable(1); //existentially quantified (level 1)
		maxQbfSlv->setDontTouch(f_i);
		muxSelect.push_back(f_i);
		int r_i = maxQbfSlv->newVariable(2); //universally quantified
		maxQbfSlv->setDontTouch(r_i);
		int s_i = maxQbfSlv->newVariable(1); //existentially quantified (level 3)
		maxQbfSlv->setDontTouch(s_i);
		muxAuxExist.push_back(s_i);

		std::vector<unsigned int> quantomClause;
		//(~f_i + ~s'_i + r_i) * (~f_i + s'_i + ~r_i)
		quantomClause.push_back((f_i << 1) + 1);
		if (revpdr)
			quantomClause.push_back((model.primeVar(*i).var() << 1) + 1);
		else
			quantomClause.push_back((i->var() << 1) + 1);
		quantomClause.push_back(r_i << 1);
		maxQbfSlv->addClause(quantomClause);
		nclauses++;
		quantomClause.clear();

		quantomClause.push_back((f_i << 1) + 1);
		if (revpdr)
			quantomClause.push_back(model.primeVar(*i).var() << 1);
		else
			quantomClause.push_back(i->var() << 1);
		quantomClause.push_back((r_i << 1) + 1);
		maxQbfSlv->addClause(quantomClause);
		nclauses++;
		quantomClause.clear();

		//(f_i + ~s'_i + s_i) * (f_i + s'_i + ~s_i)
		quantomClause.push_back(f_i << 1);
		if (revpdr)
			quantomClause.push_back((model.primeVar(*i).var() << 1) + 1);
		else
			quantomClause.push_back((i->var() << 1) + 1);
		quantomClause.push_back(s_i << 1);
		maxQbfSlv->addClause(quantomClause);
		nclauses++;
		quantomClause.clear();

		quantomClause.push_back(f_i << 1);
		if (revpdr)
			quantomClause.push_back(model.primeVar(*i).var() << 1);
		else
			quantomClause.push_back(i->var() << 1);
		quantomClause.push_back((s_i << 1) + 1);
		maxQbfSlv->addClause(quantomClause);
		nclauses++;
	}

	unsigned int idx = 0;
	for (auto &f_i : muxSelect)
	{
		std::vector<unsigned int> quantomSoftClause;
		quantomSoftClause.push_back(f_i << 1);
		maxQbfSlv->addSoftClause(quantomSoftClause);
		nclauses++;

		++idx;
	}
	//assert(maxQbfSlv->getMaxQLevel() == 3);
}

void PoGeneralizer::initILP()
{
	assert(!model.getTransSslvILP());
	model.loadTransitionRelationILP();
}

void PoGeneralizer::initJust()
{
	//TODO
}

void PoGeneralizer::initRevStruct()
{
	//simulate random patterns -> no change? assume constant
	LitVec presentState;
	std::vector<Var> possPrunableNextStateVars;
	PatternFix res = model.simFixRndPatterns(presentState);

	stateVarsWithDisjSupp.clear();
	for (VarVec::const_iterator i = model.beginLatches();
			i != model.endLatches(); ++i)
	{
		model.getTransitionSupport(*i);

		bool notConst = false;
		for (size_t k = 1;
				k < res.at(i->var() - model.beginLatches()->var()).size();
				++k)
		{
			if (res[i->var() - model.beginLatches()->var()][k]
					!= res[i->var() - model.beginLatches()->var()][0])
				notConst = true;
		}

		if (notConst)
			possPrunableNextStateVars.push_back(*i);
	}

	for (std::vector<Var>::const_iterator i =
			possPrunableNextStateVars.begin();
			i != possPrunableNextStateVars.end(); ++i)
	{
		if (model.nextStateFn(*i).x > 1) //no constant functions
		{
			std::vector<unsigned int> suppI = model.getTransitionSupport(
					*i);
			//prune next-states by structural checks
			bool intersect = false;
			for (auto &suppVar : suppI)
			{
				//is there at least one support variable with out degree > 1?
				if (model.outDegrSuppVars[suppVar] > 1)
				{
					intersect = true;
					break;
				}
			}
			//add !variable! to log disjoint next-state coi
			if (!intersect)
				stateVarsWithDisjSupp.push_back(i->var());
		}
	}

	//store support variables ordered by out degree
	for (unsigned int i = 0; i < model.outDegrSuppVars.size(); ++i)
		orderedSuppDegr.insert(SupportVar(i, model.outDegrSuppVars[i]));
}

LitVec PoGeneralizer::stateOf(MyMinisat::Solver *_slv, LitVec *_succ)
{
	//from within IC3
	if (_slv)
		modelSlv = _slv;
	if (_succ)
	{
		assert(!succ);
		succ = _succ;
	}
	assert(succ || modelSlv); //it could be, that BAD is the successor -> succ is empty
	//However, this is only possible within IC3 and thus modelSlv is available

	if (!inputs)
	{
		assert(modelSlv);
		inputs = new LitVec();
		for (VarVec::const_iterator i = model.beginInputs();
				i != model.endInputs(); ++i)
		{
			MyMinisat::lbool val = modelSlv->modelValue(i->var());
			if (val != MyMinisat::l_Undef)
			{
				MyMinisat::Lit pi = i->lit(val == MyMinisat::l_False);
				inputs->push_back(pi);
			}
		}
	}
	if (!pred)
	{
		assert(modelSlv);
		pred = new LitVec();
		if (!revpdr)
		{
			for (VarVec::const_iterator i = model.beginLatches();
					i != model.endLatches(); ++i)
			{
				MyMinisat::lbool val = modelSlv->modelValue(i->var());
				if (val != MyMinisat::l_Undef)
				{
					MyMinisat::Lit la = i->lit(val == MyMinisat::l_False);
					pred->push_back(la);
				}
			}
		}
		else
		{
			for (VarVec::const_iterator i = model.beginLatches();
					i != model.endLatches(); ++i)
			{
				MyMinisat::lbool val = modelSlv->modelValue(
						model.primeVar(*i).var());
				if (val != MyMinisat::l_Undef)
				{
					MyMinisat::Lit la = i->lit(val == MyMinisat::l_False);
					pred->push_back(la);
				}
			}

		}
	}
	assert(pred);
	if (genOpt.sortAssumpsByIc3refActivity)
		sort(pred->begin(), pred->end(), *ic3refLitOrder);
	if (genOpt.reverse)
		reverse(pred->begin(), pred->end());
	//---------------

	LitVec ret;
	switch (genType)
	{
	case NONE:
		ret = *pred;
		break;
	case LIFTING:
		ret = stateOfLifting();
		break;
	case LIFTING_LITDROP:
		ret = stateOfLifting(true);
		break;
	case TERNSIM:
		ret = stateOfTernSim();
		break;
	case S01X_FIX:
		ret = stateOfS01XFix();
		break;
	case S01X_FREE:
		ret = stateOfS01XFree();
		break;
	case MS01X_FIX:
		ret = stateOfMS01XFix();
		break;
	case MS01X_FREE:
		ret = stateOfMS01XFree();
		break;
	case MS01X_INCREMENTAL:
		ret = stateOfMS01XIncremental();
		break;
	case IGBG:
		ret = stateOfIGBG();
		break;
	case IGBG_LITDROP:
		ret = stateOfIGBG(true);
		break;
	case GENTR:
		ret = stateOfGeNTR();
		break;
	case GENTR_LITDROP:
		ret = stateOfGeNTR(true);
		break;
	case GREEDY_COVER:
		ret = stateOfGreedyCover();
		break;
#ifdef COMPILE_WITH_GUROBI
	case ILP_COVER:
		ret = stateOfILPCover();
		break;
	case ILP_COVER_FREE:
		ret = stateOfILPCoverFree();
		break;
#endif
	case SAT_COVER_FIX:
		ret = stateOfSATCoverFix();
		break;
	case SAT_COVER_FREE:
		ret = stateOfSATCoverFree();
		break;
	case GREEDY_QBF_FIX:
		ret = stateOfGreedyQBFFix();
		break;
	case GREEDY_QBF_FREE:
		ret = stateOfGreedyQBFFree();
		break;
	case MAXQBF_FIX:
		ret = stateOfMaxQBFFix();
		break;
	case MAXQBF_FREE:
		ret = stateOfMaxQBFFree();
		break;
	case REV_STRUCT:
		ret = stateOfStructRev();
		break;
	case IGBG_LIFTING_HEURISTICS:
		ret = stateOfLiftIGBG();
		break;
	case MS_HEURISTICS:
		ret = stateOfMSHeuristics();
		break;
	case JUSTIFICATION:
		ret = stateOfJust();
		break;
	default:
		break;
	}

	//IMPORTANT!
	resetStates();
	//----------

	return ret;
}

LitVec PoGeneralizer::stateOfLifting(bool litdrop)
{
	MSLitVec assumps;
	LitVec ret;
	assert(!revpdr);
	if (succ)
		assumps.capacity(1 + inputs->size() + pred->size());
	else
		assumps.capacity(1 + 2 * inputs->size() + pred->size());

	MyMinisat::Lit act = MyMinisat::mkLit(liftingSatSlv->newVar()); // activation literal
	assumps.push(act);
	MyMinisat::vec<MyMinisat::Lit> cls;
	cls.push(~act);
	if (succ)
	{
		for (auto &lit : *succ)
		{
			cls.push(model.primeLit(~lit));
		}
	}
	else
	{
		cls.push(~model.primedError());
		cls.push(notInvConstraints); // successor must satisfy inv. constraint

	}
	if(genOpt.lift.extCall)
		cls.push(notUnprimedInvConstraints); // predecessor must satisfy inv. constraint

	liftingSatSlv->addClause_(cls);
	// extract and assert primary inputs
	for (auto &pi : *inputs)
		assumps.push(pi);

	if (!succ)
	{
		// some properties include inputs, so assert primed inputs after (if BAD is successor)
		for (VarVec::const_iterator i = model.beginInputs();
				i != model.endInputs(); ++i)
		{
			assumps.push(getAssignment(model.primeVar(*i).var()));
		}
	}

	MyMinisat::Lit lastElem = assumps.last();
	int sz = assumps.size();
	//assert present state
	for (auto &lat : *pred)
		assumps.push(lat);

	// State s, inputs i, transition relation T, successor t:
	//   s & i & T & ~t' is unsat
	// Core assumptions reveal a lifting of s.

	bool rv;
	if (genOpt.lift.useLiteralRotation)
		rv = liftingSatSlv->solveWithLiteralRotation(assumps, genOpt.lift.litRotMaxFails, genOpt.lift.litRotMaxTries);
	else
		rv = liftingSatSlv->solve(assumps);
	assert(!rv);
	// obtain lifted latch set from unsat core
	for (auto &lat : *pred)
	{
		if (liftingSatSlv->conflict.has(~lat))
			ret.push_back(lat);  // record lifted latches
	}

	if (litdrop)
	{
		size_t dropIdx = 0;
		int tries = 0;
		int fails = 0;
		bool sortingRequired = false;
		while (dropIdx < ret.size() && ret.size() > 1
				&& fails < genOpt.lift.litDropMaxFails && tries < genOpt.lift.litDropMaxTries)
		{
			size_t oldsz = ret.size();
			LitVec newLatches;
			MyMinisat::Lit droppedLit = ret.at(dropIdx);
			if (dropIdx < (ret.size() - 1))
				ret.at(dropIdx) = ret.back();
			ret.pop_back(); //switch with last, pop last
			assumps.shrink(assumps.size() - sz); //drop old assumptions

			assert(assumps.last() == lastElem);

			if (dropIdx == 0 && genOpt.lift.shuffle) // like in TIP
				std::shuffle(ret.begin(), ret.end(),
						std::default_random_engine(12345678));

			for (auto &lit : ret)
			{
				assumps.push(lit);
				newLatches.push_back(lit);
			}

			if (genOpt.lift.useLiteralRotation)
				rv = liftingSatSlv->solveWithLiteralRotation(assumps, genOpt.lift.litRotMaxFails, genOpt.lift.litRotMaxTries);
			else
			{
				rv = liftingSatSlv->solve(assumps, genOpt.lift.useApproxSat); // test: approximate SAT, assume satisfiable after 100 (or something) decisions
			}

			// obtain lifted latch set from unsat core
			if (!rv)
			{
				//literal dropping successful
				ret.clear();
				for (LitVec::const_iterator i = newLatches.begin();
						i != newLatches.end(); ++i)
					if (liftingSatSlv->conflict.has(~*i))
						ret.push_back(*i);
				dropIdx = 0;
				sortingRequired = true;

				assert(oldsz > ret.size());
			}
			else
			{
				if (dropIdx < (ret.size()))
				{
					ret.push_back(ret.at(dropIdx)); // swap back
					ret.at(dropIdx) = droppedLit;
				}
				else
					ret.push_back(droppedLit);
				dropIdx++; //probe next
				fails++;
			}
			tries++;
		}
		if (sortingRequired)
			std::sort(ret.begin(), ret.end()); //sort lexicographically
	}

	// deactivate negation of successor
	liftingSatSlv->releaseVar(~act);
	return ret;
}

LitVec PoGeneralizer::stateOfS01XFix()
{
	assert(!revpdr);
	LitVec ret;
	MSLitVec assumps, cls;

	if (succ)
	{
		for (auto &lit : *succ)
		{
			model.add01XUnitToMSAssumps(assumps, model.primeLit(lit));
		}
	}
	else //error successor
	{
		model.add01XUnitToMSAssumps(assumps, model.primedError());
	}

	//either take the original value or X
	//collect present state latches
	LitVec latches;
	for (auto &la : *pred)
	{
		if (MyMinisat::sign(la))
		{
			assumps.push(la); //v^t = 0
		}
		else
		{
			assumps.push(
					MyMinisat::mkLit(model.get01XAuxVar(MyMinisat::var(la)), true)); //v^f = 0
		}
	}

	for (auto &pi : *inputs)
	{
		if (MyMinisat::sign(pi))
		{
			assumps.push(pi); //v^t = 0
		}
		else
		{
			assumps.push(
					MyMinisat::mkLit(model.get01XAuxVar(MyMinisat::var(pi)), true)); //v^f = 0
		}
	}
	//=> original value or X (1,1 forbidden by design)

	// (pred | (XX..X)) & T & succ'
	bool rv = ternSatSlv->solve(assumps);
	assert(rv);

	for (VarVec::const_iterator i = model.beginLatches();
			i != model.endLatches(); ++i)
	{
		MyMinisat::lbool val = ternSatSlv->modelValue(i->var());
		MyMinisat::lbool auxVal = ternSatSlv->modelValue(
				model.get01XAuxVar(i->var()));
		if (val != MyMinisat::l_Undef)
		{
			if (val != MyMinisat::l_False || auxVal != MyMinisat::l_False) // (0,0) don't care
			{
				MyMinisat::Lit la = i->lit(val == MyMinisat::l_False);
				ret.push_back(la);
			}
			else
				assert(val == MyMinisat::l_False && auxVal == MyMinisat::l_False);
		}
	}

	return ret;
}

LitVec PoGeneralizer::stateOfS01XFree()
{
	assert(!revpdr);
	LitVec ret;
	MSLitVec assumps, cls;

	for (auto &lit : *succ)
	{
		model.add01XUnitToMSAssumps(assumps, model.primeLit(lit));
	}

	//add frame clauses
	for (auto &cube : *frCubes)
	{
		model.add01XCube(*ternSatSlv, cube);
	}

	// F_fi & ~succ & T & succ'
	bool rv = ternSatSlv->solve(assumps);
	assert(rv);

	for (VarVec::const_iterator i = model.beginLatches();
			i != model.endLatches(); ++i)
	{
		MyMinisat::lbool val = ternSatSlv->modelValue(i->var());
		MyMinisat::lbool auxVal = ternSatSlv->modelValue(
				model.get01XAuxVar(i->var()));

		if (val != MyMinisat::l_Undef)
		{
			if (val != MyMinisat::l_False || auxVal != MyMinisat::l_False) // (0,0) don't care
			{
				MyMinisat::Lit la = i->lit(val == MyMinisat::l_False);
				ret.push_back(la);
			}
			else
				assert(val == MyMinisat::l_False && auxVal == MyMinisat::l_False);
		}
	}
	return ret;
}

LitVec PoGeneralizer::stateOfMS01XFree()
{
	assert(maxSatSlv);
	LitVec latches, ret;

	//LitVec cubeNotSWithAct;
	//unsigned int act = maxSatSlv->NewVariable();
	//cubeNotSWithAct.push_back(MyMinisat::mkLit(act, false)); // interpreted as cube -- gets negated
	//model.add01XcubeToPacoseSlv(*maxSatSlv, cubeNotSWithAct);
	for (auto &lit : *succ)
	{
		model.add01XUnitPacose(*maxSatSlv, model.primeLit(lit));
	}
	//maxSatSlv->AddAssumption(2 * act);

	//load frame clauses
	for (auto &cube : *frCubes)
		model.add01XcubeToPacoseSlv(*maxSatSlv, cube);

	// ~succ & R_i & T & succ' -> max sat yields optimal lifting
	//suppress cout of Pacose
	std::cout.setstate(std::ios_base::failbit);
	uint32_t rv = maxSatSlv->SolveProcedure();
	std::cout.clear();
	maxSatSlv->ClearAssumptions();

	assert(rv == 10);
	unsigned int idx = 0;

	for (VarVec::const_iterator i = model.beginLatches();
			i != model.endLatches(); ++i)
	{
		uint32_t softClLit = maxSatSlv->GetModel(model.softClActVars.at(idx));
		if ((softClLit % 2) != 0) //use LSB?
		{ //soft clause literal is false -> cannot lift latch literal
			uint32_t latchModelVal = maxSatSlv->GetModel(i->var());
			assert(
					(latchModelVal % 2)
							!= (maxSatSlv->GetModel(
									model.get01XAuxVar(i->var())) % 2)); //no don't care assignment
			MyMinisat::Lit latchLit = MyMinisat::mkLit(i->var(),
					(latchModelVal % 2));
			ret.push_back(latchLit); // record lifted latches
		}
		else
		{
			uint32_t latchModelVal = maxSatSlv->GetModel(i->var());
			assert((latchModelVal % 2) == 1);
			assert(
					(latchModelVal % 2)
							== (maxSatSlv->GetModel(
									model.get01XAuxVar(i->var())) % 2)); //don't care assignment
		}
		idx++;
	}

	//deactivate !succ for good
	//vector<unsigned int> deact;
	//deact.push_back(2 * act + 1);
	//maxSatSlv->AddClause(deact);

	return ret;
}

LitVec PoGeneralizer::stateOfMS01XFix()
{
	//TODO: incremental MaxSAT
	//atm: delete and reinit Pacose
	if (!maxSatSlv)
		initMS01X();

	assert(maxSatSlv);

	LitVec ret;
	std::vector<uint32_t> clStatebitToGen;

	for (auto &la : *pred)
	{
		// assert (assignment or don't care)
		if (MyMinisat::sign(la))
		{
			clStatebitToGen.push_back(la.x); //v^t = 0
			maxSatSlv->AddClause(clStatebitToGen);
			clStatebitToGen.clear();
		}
		else
		{
			clStatebitToGen.push_back(
					MyMinisat::mkLit(model.get01XAuxVar(MyMinisat::var(la)), true).x); //v^f = 0
			maxSatSlv->AddClause(clStatebitToGen);
			clStatebitToGen.clear();
		}
	}

	if (succ)
	{
		for (auto &lit : *succ)
		{
			model.add01XUnitPacose(*maxSatSlv, model.primeLit(lit));
		}
	}
	else //error successor
	{
		model.add01XUnitPacose(*maxSatSlv, model.primedError());
	}

	// s & T & succ -> max sat yields 01X-optimal lifting
	//suppress cout of Pacose
	std::cout.setstate(std::ios_base::failbit);
	uint32_t rv = maxSatSlv->SolveProcedure();
	std::cout.clear();
	//when using a heuristics, a UNSAT result only means,
	//that we can only achieve a worse result than by using
	//the (for instance) standard lifting procedure
	if (rv != 10)
	{
		//just output the complete latches
		ret = *pred;

		deinitMaxSAT();
		return ret;
	}

	for (VarVec::const_iterator i = model.beginLatches();
			i != model.endLatches(); ++i)
	{
		uint32_t val = maxSatSlv->GetModel(i->var());
		uint32_t auxVal = maxSatSlv->GetModel(model.get01XAuxVar(i->var()));

		if (!(val & 1) || !(auxVal & 1)) // (0,0) don't care
		{
			MyMinisat::Lit la = i->lit(val % 2);
			ret.push_back(la);
		}
		else
			assert((val & 1) && (auxVal & 1));
	}
	//TODO: incremental MaxSAT
	//atm: delete and reinit Pacose
	deinitMaxSAT();

	return ret;
}

LitVec PoGeneralizer::stateOfMS01XIncremental()
{
	assert(maxSatSlvIpamir);

	LitVec ret;
	std::vector<uint32_t> clStatebitToGen;

	for (auto &la : *pred)
	{
		// assert (assignment or don't care)
		// (0,0) = X
		// (0,1) = 0
		// (1,0) = 1
		// (latch, auxVar(latch))
		if (MyMinisat::sign(la))
		{
			model.ipamirAssume(maxSatSlvIpamir, la);
		}
		else
		{
			model.ipamirAssume(maxSatSlvIpamir, MyMinisat::mkLit(model.get01XAuxVar(MyMinisat::var(la)), true));
		}
	}

	if (succ)
	{
		for (auto &lit : *succ)
		{
			model.ipamirAssume01XUnit(maxSatSlvIpamir, model.primeLit(lit));
		}
	}
	else //error successor
	{
		model.ipamirAssume01XUnit(maxSatSlvIpamir, model.primedError());
	}

	// s & T & succ -> max sat yields 01X-optimal lifting
	// cout << "start solving... " << endl;
	int rv = ipamir_solve(maxSatSlvIpamir);
	// cout << "... solved" << endl;
	assert(rv == 10 || rv == 30);

	for (VarVec::const_iterator i = model.beginLatches();
			i != model.endLatches(); ++i)
	{
		int val = ipamir_val_lit(maxSatSlvIpamir, i->var());//returns pos/neg value
		int auxVal = ipamir_val_lit(maxSatSlvIpamir, model.get01XAuxVar(i->var()));

		if (val > 0 || auxVal > 0)
		{
			MyMinisat::Lit la = i->lit(val < 0);
			ret.push_back(la); // required
		}
		else// (0,0) don't care
			assert((val < 0) && (auxVal < 0));
	}

	return ret;
}

void PoGeneralizer::rotateForIGBG(vector<MyMinisat::Lit>& ret, vector<int>& reason_frequ)
{
	struct cmp_frequ
	{
		Model &m;
		vector<int>& reason_frequ;
		cmp_frequ(Model &_m, vector<int>& _reason_frequ) :
			m(_m), reason_frequ(_reason_frequ)
		{
		}
		bool operator()(MyMinisat::Lit a, MyMinisat::Lit b)
		{
			return (reason_frequ[MyMinisat::var(a) - m.beginLatches()->var()] >
				reason_frequ[MyMinisat::var(b) - m.beginLatches()->var()]); //most frequently occuring variable (in reasons)
		}
	};
	cmp_frequ cmp_reason(model, reason_frequ);

	sort(ret.begin(), ret.end(), cmp_reason);
}

void PoGeneralizer::generalizeByIGBG(LitVec &ret)
{

	//only BCP should have been used here
	LitSet reasons;
	std::vector<bool> processed(igbgSatSlvSucc->nVars() + 1, false);
	std::vector<int> reason_count(model.endLatches()-model.beginLatches(), 0); //number of occurences in reasons set
	set<MyMinisat::Var, ReverseVarComp> pending;

	if (!model.invariantConstraints().empty()) //AIGER 1.9
	{
		for (LitVec::const_iterator c = model.invariantConstraints().begin();
				c != model.invariantConstraints().end(); ++c)
		{
			pending.insert(MyMinisat::var(*c));
			if (!succ) //only required for "target enlargement"
				pending.insert(MyMinisat::var(model.primeLit(*c)));
		}
	}
	if (succ)
	{
		for (auto &lit : *succ)
		{
			pending.insert(MyMinisat::var(model.primeLit(lit)));
		}
	}
	else
	{
		pending.insert(MyMinisat::var(model.primedError()));
	}
	if(genOpt.igbg.respectProperty)
		pending.insert(MyMinisat::var(model.error()));

	while (pending.size() > 0)
	{
		MyMinisat::Var var = *pending.begin();
		MyMinisat::CRef clRef =
				(!succ) ?
						igbgSatSlvBad->reason(var) :
						igbgSatSlvSucc->reason(var);
		if (!processed[var])
		{
			processed[var] = true;
			if (clRef != MyMinisat::CRef_Undef)
			{
				const MyMinisat::Clause &cl =
						(!succ) ?
								igbgSatSlvBad->ca[clRef] :
								igbgSatSlvSucc->ca[clRef];
				pending.erase(pending.begin());
				for (int j = 0; j < cl.size(); ++j)
				{
					if (MyMinisat::var(cl[j]) >= model.endLatches()->var()
							&& MyMinisat::var(cl[j]) != var
							&& (MyMinisat::var(cl[j])
									>= model.primeVar(*model.endInputs()).var()
									|| MyMinisat::var(cl[j])
											< model.primeVar(
													*model.beginInputs()).var()) //ignore itself
							&& !processed[MyMinisat::var(cl[j])])
						pending.insert(MyMinisat::var(cl[j]));
					if (MyMinisat::var(cl[j]) >= model.beginLatches()->var()
							&& MyMinisat::var(cl[j]) < model.endLatches()->var())
					{
						reasons.insert(cl[j]);
						reason_count[MyMinisat::var(cl[j]) - model.beginLatches()->var()]++;
					}
				}
			}
			else
				pending.erase(pending.begin());
		}
		else
			pending.erase(pending.begin());
	}

	dontCares.clear();
	if (!ret.empty())
	{
		LitVec cands;
		ret.swap(cands);
		ret.clear();
		for (auto &lit : cands)
		{
			if (reasons.find(lit) != reasons.end()
					|| reasons.find(~lit) != reasons.end())
				ret.push_back(lit);
			else
			{
				//only necessary for using IGBG with MS01X heuristically
				dontCares.push_back(MyMinisat::var(lit)); //sorted implicitly?!
			}
		}
	}
	else
	{
		for (auto &lit : *pred)
		{
			if (reasons.find(lit) != reasons.end()
					|| reasons.find(~lit) != reasons.end())
				ret.push_back(lit);
			else
			{
				//only necessary for using IGBG with MS01X heuristically
				dontCares.push_back(MyMinisat::var(lit)); //sorted implicitly?!
			}
		}
	}

	/*IGBG-style literal rotation?*/
	if (genOpt.igbg.useLiteralRotation)
	{
		rotateForIGBG(ret, reason_count);
	}
	/*******/
}

LitVec PoGeneralizer::stateOfIGBG(bool litdrop)
{
	LitVec ret;
	MSLitVec assumps;
	if (succ)
		assumps.capacity(inputs->size() + pred->size());
	else
		assumps.capacity(inputs->size() * 2 + pred->size());
	assert(!revpdr);
	assert(igbgSatSlvSucc && igbgSatSlvBad);

	// assert primary inputs
	for (auto &lit : *inputs)
		assumps.push(lit);

	if (!succ)
	{
		// some properties include inputs, so assert primed inputs after (if BAD is successor)
		for (VarVec::const_iterator i = model.beginInputs();
				i != model.endInputs(); ++i)
		{
			assumps.push(getAssignment(model.primeVar(*i).var()));
		}
	}

	//EXPERIMENT WITH VAR SORT FROM SOLVER ACTIVITY HEURISTICS
	if (genOpt.igbg.sortAssumpsBySolverActivity
			&& !genOpt.sortAssumpsByIc3refActivity)
	{
		struct cmp
		{
			MyMinisat::Solver *lslv;
			bool operator()(MyMinisat::Lit a, MyMinisat::Lit b)
			{
				return lslv->cmpLit(a, b);
			}
			cmp(MyMinisat::Solver *_lslv) :
					lslv(_lslv)
			{
			}
		};
		cmp cmp1(modelSlv);

		struct cmp_occurrences
		{
			Model &m;
			cmp_occurrences(Model &_m) :
					m(_m)
			{
			}
			bool operator()(MyMinisat::Lit a, MyMinisat::Lit b)
			{
				//return (m.getLitOcc(~a) < m.getLitOcc(~b)); //literal of which its negation is least frequent
				return (m.getLitOcc(a) > m.getLitOcc(b)); //most frequent literal
			}
		};
		cmp_occurrences cmp2(model);
		//sort(pred->begin(), pred->end(), cmp1);
		sort(pred->begin(), pred->end(), cmp2);
	}
	//--------------------------------------------------------

	MyMinisat::Lit lastElem = assumps.last();
	int sz = assumps.size();
	// assert latches
	for (auto &lit : *pred)
		assumps.push(lit);

	bool rv =
			(!succ) ?
					igbgSatSlvBad->solve(assumps) :
					igbgSatSlvSucc->solve(assumps); //cancelUntil(0) by hand?

	assert(rv);

	generalizeByIGBG(ret);
	if(genOpt.igbg.useLiteralRotation)
	{
		while (true)
		{
			size_t oldsz = ret.size();
			assumps.shrink(assumps.size() - sz); //drop old assumptions
			for (auto &lit : ret)
			{
				assumps.push(lit);
			}
			bool rv = false;
			if (!succ)
			{
				for (int var = 0; var < igbgSatSlvBad->nVars(); ++var)
					igbgSatSlvBad->setDecisionVar(var, false); //no decisions
				rv = igbgSatSlvBad->solve(assumps);
			}
			else
			{
				for (int var = 0; var < igbgSatSlvSucc->nVars(); ++var)
					igbgSatSlvSucc->setDecisionVar(var, false); //no decisions
				rv = igbgSatSlvSucc->solve(assumps);
			}
			assert(rv);
			generalizeByIGBG(ret);
			if(ret.size() >= oldsz) break;
		}

		std::sort(ret.begin(), ret.end()); //sort lexicographically
	}

	/****TEST***/

	if (litdrop)
	{
		bool retryIGBG = true; //TODO: option
		size_t dropIdx = 0;
		int tries = 0;
		int fails = 0;
		bool sortingRequired = false;
		while (dropIdx < ret.size() && ret.size() > 1
				&& fails < genOpt.igbg.maxFails && tries < genOpt.igbg.maxTries)
		{
			size_t oldsz = ret.size();
			MyMinisat::Lit droppedLit = ret.at(dropIdx);
			if (dropIdx < (ret.size() - 1))
				ret.at(dropIdx) = ret.back();
			ret.pop_back(); //switch with last, pop last
			assumps.shrink(assumps.size() - sz); //drop old assumptions

			assert(assumps.last() == lastElem);

			if (dropIdx == 0 && genOpt.igbg.shuffle) // like in TIP
				std::shuffle(ret.begin(), ret.end(),
						std::default_random_engine(12345678));

			for (auto &lit : ret)
			{
				assumps.push(lit);
			}
			bool rv = false;
			if (!succ)
			{
				for (int var = 0; var < igbgSatSlvBad->nVars(); ++var)
					igbgSatSlvBad->setDecisionVar(var, false); //no decisions
				rv = igbgSatSlvBad->solve(assumps);
			}
			else
			{
				for (int var = 0; var < igbgSatSlvSucc->nVars(); ++var)
					igbgSatSlvSucc->setDecisionVar(var, false); //no decisions
				rv = igbgSatSlvSucc->solve(assumps);
			}
			assert(rv);
			bool droppable = true;
			//assert, that all relevant next states are not X -> droppable!

			if (succ)
			{
				if (!model.invariantConstraints().empty())
				{
					for (LitVec::const_iterator c =
							model.invariantConstraints().begin();
							c != model.invariantConstraints().end(); ++c)
						droppable = droppable
								&& (igbgSatSlvSucc->modelValue(MyMinisat::var(*c))
										!= MyMinisat::l_Undef);
				}
				for (auto &nextstate : *succ)
					droppable = droppable
							&& (igbgSatSlvSucc->modelValue(
									MyMinisat::var(model.primeLit(nextstate)))
									!= MyMinisat::l_Undef);
				if(genOpt.igbg.respectProperty)
					droppable = droppable
							&& (igbgSatSlvSucc->modelValue(
									MyMinisat::var(model.error()))
									!= MyMinisat::l_Undef);
			}
			else
			{
				if (!model.invariantConstraints().empty())
				{
					for (LitVec::const_iterator c =
							model.invariantConstraints().begin();
							c != model.invariantConstraints().end(); ++c)
						droppable = droppable
								&& (igbgSatSlvBad->modelValue(MyMinisat::var(*c))
										!= MyMinisat::l_Undef);
				}
				//TODO
				droppable = droppable
						&& (igbgSatSlvBad->modelValue(
						MyMinisat::var(model.primedError())) != MyMinisat::l_Undef);
				if(genOpt.igbg.respectProperty)
					droppable = droppable
							&& (igbgSatSlvBad->modelValue(
									MyMinisat::var(model.error()))
									!= MyMinisat::l_Undef);
			}


			if (droppable)
			{

				//IGBG again?
				if(retryIGBG)
					generalizeByIGBG(ret);
				//literal dropping successful
				dropIdx = 0;
				sortingRequired = true;

				assert(oldsz > ret.size());
			}
			else
			{
				if (dropIdx < (ret.size()))
				{
					ret.push_back(ret.at(dropIdx)); // swap back
					ret.at(dropIdx) = droppedLit;
				}
				else
					ret.push_back(droppedLit);
				dropIdx++; //probe next
				fails++;
			}
			tries++;
		}
		if (sortingRequired)
			std::sort(ret.begin(), ret.end()); //sort lexicographically
	}

	/***********/

	return ret;
}

LitVec PoGeneralizer::stateOfGreedyQBFFix()
{
	assert(greedyQbfSlv);

	qdpll_push(greedyQbfSlv);

	vector<bool> droppedVars(muxSelect.size(), false);

	LitVec ret;

	/*
	 * Consecution call SAT?[R_i * T * succ'] was SAT (assignment s).
	 * Now we want to lift using greedyQBF.
	 *
	 * We need to assume the following:
	 * 	- predecessor/successor state succ'
	 * 	- assignment s aligned with the corresponding exist variables of the multiplexer
	 *	- muxSelect vars
	 */
	if (revpdr)
	{
		if (succ)
		{
			for (auto &lit : *succ)
			{
				qdpll_add(greedyQbfSlv,
						MyMinisat::var(lit)
								* ((int) pow(-1, MyMinisat::sign(lit))));
				qdpll_add(greedyQbfSlv, 0);
			}
		}
		else //init predecessor
		{
			LitVec init = model.getInitLits();
			for (auto &lit : init)
			{
				qdpll_add(greedyQbfSlv,
						MyMinisat::var(lit)
								* ((int) pow(-1, MyMinisat::sign(lit))));
				qdpll_add(greedyQbfSlv, 0);
			}
		}

		/* assignment s' aligned with the corresponding exist variables of the multiplexer */
		unsigned int muxAuxExistIdx = 0;
		for (auto &lit : *pred)
		{
			MyMinisat::Lit pLit = model.primeLit(lit);

			bool sign = MyMinisat::sign(pLit);
			qdpll_add(greedyQbfSlv,
					muxAuxExist[muxAuxExistIdx] * ((int) pow(-1, sign)));
			qdpll_add(greedyQbfSlv, 0);
			muxAuxExistIdx++;
		}
	}
	else
	{
		/* successor state succ' */
		if (succ)
		{
			for (auto &lit : *succ)
			{
				qdpll_add(greedyQbfSlv,
						MyMinisat::var(model.primeLit(lit))
								* ((int) pow(-1,
										MyMinisat::sign(model.primeLit(lit)))));
				qdpll_add(greedyQbfSlv, 0);
			}
		}
		else //error successor
		{
			qdpll_add(greedyQbfSlv,
					MyMinisat::var(model.primedError())
							* ((int) pow(-1, MyMinisat::sign(model.primedError()))));
			qdpll_add(greedyQbfSlv, 0);
		}

		/* assignment s aligned with the corresponding exist variables of the multiplexer */
		unsigned int muxAuxExistIdx = 0;
		for (auto &lit : *pred)
		{
			bool sign = MyMinisat::sign(lit);
			qdpll_add(greedyQbfSlv,
					muxAuxExist[muxAuxExistIdx] * ((int) pow(-1, sign)));
			qdpll_add(greedyQbfSlv, 0);
			muxAuxExistIdx++;
		}
	}

	assert(muxSelect.size() == muxAuxExist.size());
	assert((model.endLatches() - model.beginLatches()) >= 0);
	assert(
			muxSelect.size()
					== (unsigned long )(model.endLatches()
							- model.beginLatches()));
	assert(muxSelect.size() == droppedVars.size());

	for (size_t i = 0; i < droppedVars.size(); ++i)
	{
		droppedVars[i] = true;
		for (size_t j = 0; j < muxSelect.size(); ++j)
		{
			if (droppedVars[j])
			{
				qdpll_assume(greedyQbfSlv, muxSelect[j]);
			}
			else
			{
				qdpll_assume(greedyQbfSlv, -muxSelect[j]);
			}
		}
		unsigned int rv = qdpll_sat(greedyQbfSlv);
		if (rv != QDPLL_RESULT_SAT) //dropping the literal is allowed
		{
			droppedVars[i] = false;
		}
		qdpll_reset(greedyQbfSlv);
	}

	qdpll_pop(greedyQbfSlv);

	unsigned int idx = 0;

	for (auto &lit : *pred) //pred should be aligned with latches
	{
		if (!droppedVars[idx])
		{
			MyMinisat::Lit addLit;
			if (revpdr)
			{
				addLit = model.primeLit(lit);
			}
			else
			{
				addLit = lit;
			}
			ret.push_back(addLit);
		}
		idx++;
	}

	return ret;
}

LitVec PoGeneralizer::stateOfGreedyQBFFree()
{
	assert(greedyQbfSlv);

	qdpll_push(greedyQbfSlv);

	vector<bool> droppedVars(muxSelect.size(), false);

	LitVec ret;

	/*
	 * Consecution call SAT?[R_i * T * succ'] was SAT (assignment s).
	 * Now we want to lift using greedyQBF.
	 *
	 * We need to assume the following:
	 * 	- predecessor/successor state succ'
	 * 	- assignment s aligned with the corresponding exist variables of the multiplexer
	 *	- muxSelect vars
	 */
	if (revpdr)
	{
		for (auto &lit : *succ)
		{
			qdpll_add(greedyQbfSlv,
					MyMinisat::var(lit) * ((int) pow(-1, MyMinisat::sign(lit))));
			qdpll_add(greedyQbfSlv, 0);
		}

		/* assignment s' aligned with the corresponding exist variables of the multiplexer
		 unsigned int muxAuxExistIdx = 0;
		 for (auto &lit : *pred)
		 {
		 MyMinisat::Lit pLit = model.primeLit(lit);

		 bool sign = MyMinisat::sign(pLit);
		 qdpll_add(greedyQbfSlv,
		 muxAuxExist[muxAuxExistIdx] * ((int) pow(-1, sign)));
		 qdpll_add(greedyQbfSlv, 0);
		 muxAuxExistIdx++;
		 } */
	}
	else
	{
		/* successor state succ' */
		for (auto &lit : *succ)
		{
			qdpll_add(greedyQbfSlv,
					MyMinisat::var(model.primeLit(lit))
							* ((int) pow(-1, MyMinisat::sign(model.primeLit(lit)))));
			qdpll_add(greedyQbfSlv, 0);
		}

		/* assignment s aligned with the corresponding exist variables of the multiplexer
		 unsigned int muxAuxExistIdx = 0;
		 for (auto &lit : pred)
		 {
		 bool sign = MyMinisat::sign(lit);
		 qdpll_add(greedyQbfSlv,
		 muxAuxExist[muxAuxExistIdx] * ((int) pow(-1, sign)));
		 qdpll_add(greedyQbfSlv, 0);
		 muxAuxExistIdx++;
		 }*/
	}

	//add R_i cubes
	for (auto &cube : *frCubes)
	{
		for (auto &lit : cube)
		{
			assert(lit.x > 1);
			qdpll_add(greedyQbfSlv,
					MyMinisat::var(lit) * ((int) pow(-1, MyMinisat::sign(~lit)))); //negated
		}
		qdpll_add(greedyQbfSlv, 0);
	}

	assert(muxSelect.size() == muxAuxExist.size());
	assert((model.endLatches() - model.beginLatches()) >= 0);
	assert(
			muxSelect.size()
					== (unsigned long )(model.endLatches()
							- model.beginLatches()));
	assert(muxSelect.size() == droppedVars.size());

	for (size_t i = 0; i < droppedVars.size(); ++i)
	{
		droppedVars[i] = true;
		for (size_t j = 0; j < muxSelect.size(); ++j)
		{
			if (droppedVars[j])
			{
				qdpll_assume(greedyQbfSlv, muxSelect[j]);
			}
			else
			{
				qdpll_assume(greedyQbfSlv, -muxSelect[j]);
			}
		}
		unsigned int rv = qdpll_sat(greedyQbfSlv);
		if (rv != QDPLL_RESULT_SAT) //dropping the literal is allowed
		{
			droppedVars[i] = false;
		}
		qdpll_reset(greedyQbfSlv);
	}

	qdpll_pop(greedyQbfSlv);

	unsigned int idx = 0;

	for (auto &lit : *pred) //pred should be aligned with latches
	{
		if (!droppedVars[idx])
		{
			MyMinisat::Lit addLit;
			if (revpdr)
			{
				addLit = model.primeLit(lit);
			}
			else
			{
				addLit = lit;
			}
			ret.push_back(addLit);
		}
		idx++;
	}

	return ret;
}

LitVec PoGeneralizer::stateOfMaxQBFFix()
{
	//TODO: incremental MaxSAT
	//atm: delete and reinit Quantom
	if (!maxQbfSlv)
		initMaxQBF();

	LitVec ret;
	std::vector<unsigned int> assumps;

	if (succ)
	{
		for (auto &lit : *succ)
		{
			//assumptions.push_back(lit.x);
			if (revpdr)
			{
				maxQbfSlv->addUnit(lit.x);
			}
			else
			{
				maxQbfSlv->addUnit((model.primeLit(lit)).x);
			}
		}
	}
	else //error successor / init predecessor
	{
		if (revpdr)
		{
			LitVec init = model.getInitLits();
			for (auto &lit : init)
			{
				//assumptions.push_back(lit.x);
				maxQbfSlv->addUnit(lit.x);
			}
		}
		else
		{
			maxQbfSlv->addUnit(model.primedError().x);
		}
	}

	//instead of existentially quantifying the alternative
	//to the universally quantified variables,
	//we assign them the original POs assignment
	assert(pred->size() == muxAuxExist.size());
	for (size_t i = 0; i < muxAuxExist.size(); ++i)
	{
		maxQbfSlv->addUnit((muxAuxExist[i] * 2) + MyMinisat::sign((*pred)[i]));
	}

	int optimum = -1; //standard maximization
	unsigned int rv = maxQbfSlv->maxSolve(optimum, assumps, 1); //mode 0: unsat based (?)
	assert(rv == 10);		//possible!
	int idx = 0;
	for (VarVec::const_iterator v = model.beginLatches();
			v != model.endLatches(); ++v)
	{
		unsigned int softClVar = muxSelect.at(idx);
		unsigned int auxExistVar = muxAuxExist.at(idx);
		if ((maxQbfSlv->model().at(softClVar) % 2) == 1)//unsatisfied soft clause
		{
			//we have to keep the literal
			unsigned int auxExistLit = maxQbfSlv->model().at(auxExistVar);
			MyMinisat::Lit minisatLatchLit = MyMinisat::mkLit(v->var(),
					(auxExistLit % 2));
			ret.push_back(minisatLatchLit);
		}
		idx++;
	}

	//TODO: incremental MaxSAT
	//atm: delete and reinit Quantom
	deinitMaxQBF();

	return ret;
}

LitVec PoGeneralizer::stateOfMaxQBFFree()
{
	LitVec ret;
	std::vector<unsigned int> assumps;

	//add learnt clauses
	//add blocked cubes / learnt clauses to maxSAT Solver
	for (auto &cube : *frCubes)
	{
		std::vector<unsigned int> quantomClause;
		for (auto &lit : cube)
		{
			if (revpdr)
				quantomClause.push_back((~model.primeLit(lit)).x);
			else
				quantomClause.push_back((~lit).x);
		}
		maxQbfSlv->addClause(quantomClause);

	}

	//predecessor / successor
	//std::vector<unsigned int> notS;
	for (auto &lit : *succ)
	{
		if (revpdr)
		{
			maxQbfSlv->addUnit(lit.x);
		}
		else
		{
			maxQbfSlv->addUnit((model.primeLit(lit)).x);
			//notS.push_back((~lit).x);
		}
	}
	//add ~s to the query (only in !revpdr case)
	//because down() is only used in original PDR case
	//if (!revpdr)
	{
		//	maxQbfSlv->addClause(notS);
	}

	int optimum = -1; //standard maximization
	unsigned int rv = maxQbfSlv->maxSolve(optimum, assumps, 1); //mode 0: unsat based (?)
	assert(rv == 10);		//possible!
	int idx = 0;
	for (VarVec::const_iterator v = model.beginLatches();
			v != model.endLatches(); ++v)
	{
		unsigned int softClVar = muxSelect.at(idx);
		unsigned int auxExistVar = muxAuxExist.at(idx);
		if ((maxQbfSlv->model().at(softClVar) % 2) == 1)//unsatisfied soft clause
		{
			//we have to keep the literal
			unsigned int auxExistLit = maxQbfSlv->model().at(auxExistVar);
			MyMinisat::Lit minisatLatchLit = MyMinisat::mkLit(v->var(),
					(auxExistLit % 2));
			ret.push_back(minisatLatchLit);
		}
		idx++;
	}

	return ret;
}

LitVec PoGeneralizer::stateOfGreedyCover()
{
	std::vector<LitVec> clausesToScan;
	LitVec ret;
	MSLitVec assumps;
	assumps.capacity(
			(model.endInputs() - model.beginInputs())
					+ 2 * (model.endLatches() - model.beginLatches()));

	// extract primary inputs
	for (auto &pi : *inputs)
	{
		assumps.push(pi);
	}
	// extract latches
	LitVec latches;
	for (auto &la : *pred)
	{
		latches.push_back(la); //sorted
		assumps.push(la);
	}

	for (auto &lit : *succ)
	{
		assumps.push(model.primeLit(lit));
	}

	MyMinisat::SimpSolver *slv = model.getTransSslv();
	//clauses to scan remain the same: just T
	assert(slv);
	bool rv = slv->solve(assumps, false, true);
	assert(rv);
	for (MyMinisat::ClauseIterator c = slv->clausesBegin();
			c != slv->clausesEnd(); ++c)
	{
		const MyMinisat::Clause &cl = *c;
		LitVec cl_;
		for (int i = 0; i < cl.size(); ++i)
			cl_.push_back(cl[i]);
		clausesToScan.push_back(cl_);
	}

	for (MyMinisat::TrailIterator c = slv->trailBegin(); c != slv->trailEnd();
			++c)
	{
		MyMinisat::Lit l = *c;
		LitVec cl_;
		cl_.push_back(l);
		clausesToScan.push_back(cl_);
	}

	/*
	 * reduce clause set by clauses which are already satisfied through
	 * assignments to non-po variables
	 */
	for (size_t c = 0; c < clausesToScan.size();)
	{
		bool sat = false;
		LitVec cl = clausesToScan[c];
		for (size_t i = 0; i < cl.size(); ++i)
		{
			if (MyMinisat::var(cl[i]) < model.beginLatches()->var()
					|| MyMinisat::var(cl[i]) >= model.endLatches()->var())
			{
				if (slv->modelValue(MyMinisat::var(cl[i])) != MyMinisat::l_Undef)
				{
					if (MyMinisat::sign(cl[i])
							== (slv->modelValue(MyMinisat::var(cl[i]))
									== MyMinisat::l_False))
					{
						/*
						 * literal in clause is satisfied, clause can be removed
						 * from unsatisfied clauses...
						 */
						sat = true;
						break;
					}
				}
			}
		}
		//printf("\n");
		if (sat)
		{
			/*
			 * clause satisfied, remove
			 */
			if (c < (clausesToScan.size() - 1))
				clausesToScan[c] = clausesToScan.back();
			clausesToScan.pop_back();
		}
		else
			++c;
	}

	auto cmp = [](std::pair<MyMinisat::Lit, unsigned int> left,
			std::pair<MyMinisat::Lit, unsigned int> right)
			{	return (left.second) <= (right.second);};
	std::priority_queue<std::pair<MyMinisat::Lit, unsigned int>,
			std::vector<std::pair<MyMinisat::Lit, unsigned int>>, decltype(cmp)> litFrequencies(
			cmp);
	std::vector<std::pair<unsigned int, std::vector<size_t>>> occVec(
			(model.endLatches() - model.beginLatches()));
	//compute current frequencies
	for (size_t c = 0; c < clausesToScan.size(); ++c)
	{
		bool found = false;
		LitVec cl = clausesToScan[c];
		for (size_t i = 0; i < cl.size(); ++i)
		{
			if (MyMinisat::var(cl[i]) >= model.beginLatches()->var()
					&& MyMinisat::var(cl[i]) < model.endLatches()->var())
			{
				//only update the occurence list for obligation-literals
				if (latches[MyMinisat::var(cl[i]) - model.beginLatches()->var()]
						== cl[i])
				{
					found = true;
					//increment frequency
					occVec[MyMinisat::var(cl[i]) - model.beginLatches()->var()].first++;
					//add index to occurence list
					occVec[MyMinisat::var(cl[i]) - model.beginLatches()->var()].second.push_back(
							c);
				}
			}
		}
		assert(found);
	}
	for (size_t i = 0; i < occVec.size(); ++i)
	{
		//literals from oblCube ordered by frequency
		litFrequencies.push(
				std::pair<MyMinisat::Lit, unsigned int>(latches[i],
						occVec[i].first));
	}

	unsigned int satCl = 0; //count satisfied clauses
	std::vector<bool> flagsSatClauses(clausesToScan.size(), false);
	while (satCl < clausesToScan.size())
	{
		//printf("occ: %d\n",litFrequencies.top().second);
		MyMinisat::Lit currLit = litFrequencies.top().first;
		for (auto &index : occVec[MyMinisat::var(currLit)
				- model.beginLatches()->var()].second)
		{
			if (!flagsSatClauses[index])
			{
				flagsSatClauses[index] = true;
				satCl++;
			}
		}
		ret.push_back(currLit);
		litFrequencies.pop();
	}
	/*printf("satCl: %d\n", satCl);
	 printf("clausesToScan.size(): %d\n", clausesToScan.size());
	 printf("remaining literals: %d\n", litFrequencies.size());*/
	for (bool flag : flagsSatClauses)
		assert(flag);

	//std::cout << "hitting set gen from " << latches.size() << " to "
	//		<< state(st).latches.size() << std::endl;
	return ret;
}

#ifdef COMPILE_WITH_GUROBI
LitVec PoGeneralizer::stateOfILPCover()
{
	LitVec ret;

	try
	{
		// Create an environment
		GRBEnv env = GRBEnv(true);
		//env.set("LogFile", "mip1.log");
		env.set("OutputFlag", "0");
		env.start();

		// Create an empty model
		GRBModel grbmodel = GRBModel(env);
		grbmodel.set("Threads", "1");
		std::vector<GRBVar> fixGrbVars(model.getMaxVar() + 1);

		// Create variables
		for (size_t v = 0; v <= model.getMaxVar(); ++v)
		{
			//fix present state variables
			fixGrbVars.at(v) = grbmodel.addVar(0.0, 1.0, 0.0,
			GRB_BINARY, std::to_string(v).append("_fix"));
		}

		//extract present state valuation
		GRBLinExpr optConstr = 0;
		for (VarVec::const_iterator i = model.beginLatches();
				i != model.endLatches(); ++i)
		{
			//objective: minimize sum of present state variables
			optConstr += fixGrbVars[i->var()];
		}
		grbmodel.setObjective(optConstr, GRB_MINIMIZE);

		MyMinisat::SimpSolver *transSlv = model.getTransSslv();//model.getTransSslvILP();
		assert(transSlv);
		int clauseIdx = 0;
		for (MyMinisat::ClauseIterator c = transSlv->clausesBegin();
				c != transSlv->clausesEnd(); ++c)
		{
			const MyMinisat::Clause &cls = *c;
			GRBLinExpr constr = 0;
			assert(cls.size() > 0);
			for (int i = 0; i < cls.size(); ++i)
			{
				assert(cls[i].x > 1);

				//check polarity of literal
				//remove if it is unsatisfied by the assignment to the fixed
				//literals.
				if (MyMinisat::sign(cls[i])
						== MyMinisat::sign(
								getAssignment(MyMinisat::var(cls[i]))))
				{
					constr += fixGrbVars[MyMinisat::var(cls[i])];
				}
			}
			ostringstream cname;
			assert(constr.size() > 0);
			grbmodel.addConstr(constr >= 1.0, cname.str());
			clauseIdx++;
		}
		for (MyMinisat::TrailIterator c = transSlv->trailBegin();
				c != transSlv->trailEnd(); ++c)
		{
			if ((*c).x > 1)
			{
				GRBLinExpr constr = 0;
				//check polarity of literal
				//remove if it is unsatisfied by the assignment to the fixed
				//literals.
				if (MyMinisat::sign(*c)
						== MyMinisat::sign(getAssignment(MyMinisat::var(*c))))
				{
					constr += fixGrbVars[MyMinisat::var(*c)];
				}
				ostringstream cname;
				cname << "cl_" << clauseIdx;
				assert(constr.size() > 0);
				grbmodel.addConstr(constr >= 1.0, cname.str());
				clauseIdx++;
			}
		}

		// Optimize model
		grbmodel.optimize();

		for (auto &lit : *pred)
		{
			//extract result:
			//unused present state variables have value 0
			if (fixGrbVars[MyMinisat::var(lit)].get(GRB_DoubleAttr_X) != 0)
			{
				ret.push_back(lit);
			}
		}

	} catch (GRBException &e)
	{
		cout << "Error code = " << e.getErrorCode() << endl;
		cout << e.getMessage() << endl;
	} catch (...)
	{
		cout << "Exception during optimization" << endl;
	}
	return ret;
}

LitVec PoGeneralizer::stateOfILPCoverFree()
{
	LitVec ret;

	try
	{
		// Create an environment
		GRBEnv env = GRBEnv(true);
		//env.set("LogFile", "mip1.log");
		env.set("OutputFlag", "0");
		env.start();

		// Create an empty model
		GRBModel grbmodel = GRBModel(env);
		grbmodel.set("Threads", "1");
		std::vector<GRBVar> openGrbVars(2 * (model.getMaxVar() + 1));
		std::vector<GRBVar> fixGrbVars(model.getMaxVar() + 1);
		std::vector<bool> isFix(model.getMaxVar() + 1, false);

		// Create variables
		for (size_t v = 0; v <= model.getMaxVar(); ++v)
		{
			if (v >= model.endLatches()->var()
					&& model.unprimeVar(v) < model.endLatches()->var()
					&& model.unprimeVar(v) >= model.beginLatches()->var())
			{
				//fix variables if contained in s'
				if (std::binary_search(succ->begin(),
						succ->end(),
						MyMinisat::mkLit(model.unprimeVar(v), false))
						|| std::binary_search(
								succ->begin(),
								succ->end(),
								MyMinisat::mkLit(model.unprimeVar(v),
										true)))
				{
					//fix next state variables
					fixGrbVars.at(v) = grbmodel.addVar(0.0, 1.0,
							0.0, GRB_BINARY,
							std::to_string(v).append("_fix"));
					isFix.at(v) = true;
				}
				else
				{
					//open variables
					openGrbVars.at(2 * v) = grbmodel.addVar(0.0,
							1.0, 0.0, GRB_BINARY,
							std::to_string(v).append("_pos"));
					openGrbVars.at(2 * v + 1) = grbmodel.addVar(
							0.0, 1.0, 0.0, GRB_BINARY,
							std::to_string(v).append("_neg"));
				}
			}
			else
			{
				//open variables
				openGrbVars.at(2 * v) = grbmodel.addVar(0.0, 1.0,
						0.0, GRB_BINARY,
						std::to_string(v).append("_pos"));
				openGrbVars.at(2 * v + 1) = grbmodel.addVar(0.0,
						1.0, 0.0, GRB_BINARY,
						std::to_string(v).append("_neg"));
			}
		}

		//extract present state valuation
		GRBLinExpr optConstr = 0;
		for (VarVec::const_iterator i = model.beginLatches();
				i != model.endLatches(); ++i)
		{
			//objective: minimize sum of present state variables
			optConstr += openGrbVars[2 * i->var()];
			optConstr += openGrbVars[2 * i->var() + 1];
		}
		grbmodel.setObjective(optConstr, GRB_MINIMIZE);

		MyMinisat::SimpSolver *transSlv = model.getTransSslv();
		assert(transSlv);
		int clauseIdx = 0;
		for (MyMinisat::ClauseIterator c = transSlv->clausesBegin();
				c != transSlv->clausesEnd(); ++c)
		{
			const MyMinisat::Clause &cls = *c;
			GRBLinExpr constr = 0;
			assert(cls.size() > 0);
			for (int i = 0; i < cls.size(); ++i)
			{
				assert(cls[i].x > 1);

				if (isFix[MyMinisat::var(cls[i])])
				{
					//check polarity of literal
					//remove if it is unsatisfied by the assignment to the fixed
					//literals.
					if (MyMinisat::sign(cls[i])
							== MyMinisat::sign(
									getAssignment(MyMinisat::var(cls[i]))))
					{
						constr += fixGrbVars[MyMinisat::var(cls[i])];
					}
				}
				else
				{
					constr += openGrbVars[2 * MyMinisat::var(cls[i])
							+ MyMinisat::sign(cls[i])];
				}
			}
			ostringstream cname;
			assert(constr.size() > 0);
			grbmodel.addConstr(constr >= 1.0, cname.str());
			clauseIdx++;
		}
		for (MyMinisat::TrailIterator c = transSlv->trailBegin();
				c != transSlv->trailEnd(); ++c)
		{
			if ((*c).x > 1)
			{
				GRBLinExpr constr = 0;
				if (isFix[MyMinisat::var(*c)])
				{
					//check polarity of literal
					//remove if it is unsatisfied by the assignment to the fixed
					//literals.
					if (MyMinisat::sign(*c)
							== MyMinisat::sign(getAssignment(MyMinisat::var(*c))))
					{
						constr += fixGrbVars[MyMinisat::var(*c)];
					}
				}
				else
				{
					constr += openGrbVars[2 * MyMinisat::var(*c)
							+ MyMinisat::sign(*c)];
				}
				ostringstream cname;
				cname << "cl_" << clauseIdx;
				assert(constr.size() > 0);
				grbmodel.addConstr(constr >= 1.0, cname.str());
				clauseIdx++;
			}
		}

		//add frame clauses to constraints
		for(auto& cube: *frCubes)
		{
			GRBLinExpr constr = 0;
			for(auto cblit: cube)
			{
				const MyMinisat::Lit lit = ~cblit;
				constr += openGrbVars[2 * MyMinisat::var(lit)
						+ MyMinisat::sign(lit)];
			}
			ostringstream cname;
			cname << "fr_cl_" << clauseIdx;
			assert(constr.size() > 0);
			grbmodel.addConstr(constr >= 1.0, cname.str());
			clauseIdx++;
		}

		//variable constraints v_pos + v_neg <= 1
		for (size_t v = 0; v <= model.getMaxVar(); ++v)
		{
			if(!isFix[v])
			{
				GRBLinExpr constr = 0;
				constr += openGrbVars[2 * v];
				constr += openGrbVars[2 * v + 1];
				ostringstream cname;
				cname << "pos_neg_" << v;
				grbmodel.addConstr(constr <= 1.0, cname.str());
			}
		}

		// Optimize model
		grbmodel.optimize();

		for (auto &lit : *pred)
		{
			//extract result:
			//unused present state variables have value 0
			//if (fixGrbVars[MyMinisat::var(lit)].get(GRB_DoubleAttr_X) != 0)
			if ((openGrbVars[2 * MyMinisat::var(lit)].get(GRB_DoubleAttr_X) != 0) ||
					(openGrbVars[2 * MyMinisat::var(lit) + 1].get(GRB_DoubleAttr_X) != 0))
			{
				ret.push_back(lit);
			}
		}

	} catch (GRBException &e)
	{
		cout << "Error code = " << e.getErrorCode() << endl;
		cout << e.getMessage() << endl;
	} catch (...)
	{
		cout << "Exception during optimization" << endl;
	}
	return ret;
}
#endif

LitVec PoGeneralizer::stateOfGeNTR(bool litdrop)
{
	LitVec ret;
	MSLitVec assumps;
	assert(!revpdr); //provably no effect for RevPDR, ergo not implemented accordingly
	assert(inputs->size() == (unsigned)(model.endInputs() - model.beginInputs()));
	for (auto &lit : *inputs)
	{
		assumps.push(lit);
	}
	assert(pred->size() == (unsigned)(model.endLatches() - model.beginLatches()));
	for (auto &lit : *pred)
	{
		assumps.push(lit);
	}
	for (VarVec::const_iterator it = model.beginLatches();
			it < model.endLatches(); ++it)
	{
		const MyMinisat::Lit pLit = getAssignment(model.primeVar(*it).var());
		assumps.push(pLit);
	}

	assert(GeNTRSlv->okay());
	bool rv = model.getTransSslvRed()->solve(assumps, false, true);
	assert(rv);
	assumps.clear();
	for (int m = 0; m < model.getTransSslvRed()->model.size(); ++m)
	{
		const MyMinisat::Lit mLit = MyMinisat::mkLit(m,
				(model.getTransSslvRed()->model[m] == MyMinisat::l_False));
		assumps.push(mLit);
	}

	rv = GeNTRSlv->solve(assumps);
	assert(!rv);
	// obtain lifted latch set from unsat core
	for (auto &lit : *pred)
		if (GeNTRSlv->conflict.has(~lit))
			ret.push_back(lit);  // record lifted latches

	if (litdrop)
	{
		size_t dropIdx = 0;
		bool sortingRequired = false;
		while (dropIdx < ret.size() && ret.size() > 1)
		{
			size_t oldsz = ret.size();
			LitVec newLatches;
			MyMinisat::Lit droppedLit = ret.at(dropIdx);
			if (dropIdx < (ret.size() - 1))
				ret.at(dropIdx) = ret.back();
			ret.pop_back(); //switch with last, pop last
			assumps.clear();
			//everything before the latches
			for (int m = 0; m < model.beginLatches()->var(); ++m)
			{
				const MyMinisat::Lit mLit =
						MyMinisat::mkLit(m,
								(model.getTransSslvRed()->model[m]
										== MyMinisat::l_False));
				assumps.push(mLit);
			}

			for (auto &lit : ret)
			{
				assumps.push(lit);
				newLatches.push_back(lit);
			}

			//everything after the latches
			for (int m = model.endLatches()->var();
					m < model.getTransSslvRed()->model.size(); ++m)
			{
				const MyMinisat::Lit mLit =
						MyMinisat::mkLit(m,
								(model.getTransSslvRed()->model[m]
										== MyMinisat::l_False));
				assumps.push(mLit);
			}

			rv = GeNTRSlv->solve(assumps);
			// obtain lifted latch set from unsat core
			if (!rv)
			{
				//literal dropping successful
				ret.clear();
				for (LitVec::const_iterator i = newLatches.begin();
						i != newLatches.end(); ++i)
					if (GeNTRSlv->conflict.has(~*i))
						ret.push_back(*i);
				dropIdx = 0;
				sortingRequired = true;

				assert(oldsz > ret.size());
			}
			else
			{
				if (dropIdx < (ret.size()))
				{
					ret.push_back(ret.at(dropIdx)); // swap back
					ret.at(dropIdx) = droppedLit;
				}
				else
					ret.push_back(droppedLit);
				dropIdx++; //probe next
			}
		}
		if (sortingRequired)
			std::sort(ret.begin(), ret.end()); //sort lexicographically
	}

	return ret;
}

LitVec PoGeneralizer::stateOfSATCoverFix()
{
	LitVec ret;
	MSLitVec assumps;

	if (succ)
	{
		for (auto &lit : *succ)
			assumps.push(model.primeLit(lit));
	}
	else // error successor
	{
		assumps.push(model.primedError());
	}

	for (auto &la : *pred)
	{
		if (MyMinisat::sign(la))
		{
			assumps.push(la); //v^t = 0
		}
		else
		{
			assumps.push(
					MyMinisat::mkLit(model.get01XAuxVar(MyMinisat::var(la)), true)); //v^f = 0
		}
	}

	bool rv = coverSatSlv->solve(assumps);
	assert(rv);

	for (VarVec::const_iterator i = model.beginLatches();
			i != model.endLatches(); ++i)
	{
		MyMinisat::lbool val = coverSatSlv->modelValue(i->var());
		MyMinisat::lbool auxVal = coverSatSlv->modelValue(
				model.get01XAuxVar(i->var()));
		if (val != MyMinisat::l_Undef)
		{
			if (val != MyMinisat::l_False || auxVal != MyMinisat::l_False) // (0,0) don't care
			{
				MyMinisat::Lit la = i->lit(val == MyMinisat::l_False);
				ret.push_back(la);
			}
			else
				assert(val == MyMinisat::l_False && auxVal == MyMinisat::l_False);
		}
	}

	return ret;
}

LitVec PoGeneralizer::stateOfTernSim()
{
	assert(!revpdr);
	/*
	 * apply 01X-simulation (bit-parallel) for generalization
	 */

	if (succ)
	{
		generalizeByTernSim();
	}
	else
	{
		LitVec primedInputs;
		// some properties include inputs, so assert primed inputs after (if BAD is successor)
		for (VarVec::const_iterator i = model.beginInputs();
				i != model.endInputs(); ++i)
		{
			primedInputs.push_back(getAssignment(model.primeVar(*i).var()));
		}
		generalizeBadByTernSim(primedInputs);
	}

	return *pred;
}

void PoGeneralizer::generalizeByTernSim()
{
	assert(!revpdr);
	/*
	 * apply 01X-simulation (bit-parallel) for generalization
	 */

	std::pair<uint64_t, uint64_t> initVal;
	MyMinisat::Var maxNextStateVar = 0;
	initVal.first = 0;
	initVal.second = 0;
	size_t firstTryIdx = 0;
	bool finished = false;
	/*
	 * always try ternsimBitWidth portions of
	 * X-ed present state bits in parallel
	 */
	Model::ternsimVec valuation(model.getMaxVar() + 1, initVal);
	//constant zero:
	valuation[0].first = 0;
	valuation[0].second = UINT64_MAX;

	for (auto &lit : *inputs)
	{
		valuation[MyMinisat::var(lit)].first = (!sign(lit)) * UINT64_MAX;
		valuation[MyMinisat::var(lit)].second = sign(lit) * UINT64_MAX;
	}

	if (!succ)
	{
		// some properties include inputs, so assert primed inputs after (if BAD is successor)
		for (VarVec::const_iterator i = model.beginInputs();
				i != model.endInputs(); ++i)
		{
			MyMinisat::Lit lit = getAssignment(model.primeVar(*i).var());
			valuation[MyMinisat::var(lit)].first = (!sign(lit)) * UINT64_MAX;
			valuation[MyMinisat::var(lit)].second = sign(lit) * UINT64_MAX;
		}
	}

	//new present state valuation
	for (auto &lit : *pred)
	{
		valuation[MyMinisat::var(lit)].first = (!sign(lit)) * UINT64_MAX;
		valuation[MyMinisat::var(lit)].second = sign(lit) * UINT64_MAX;
	}
	//next state valuation
	for (auto &lit : *succ)
	{
		MyMinisat::Var nextstatevar = MyMinisat::var(
				model.nextStateFn(model.varOfLit(lit)));
		if (nextstatevar > maxNextStateVar)
			maxNextStateVar = nextstatevar;
		bool truthval = (sign(lit)
				== sign(model.nextStateFn(model.varOfLit(lit))));
		valuation[nextstatevar].first = truthval * UINT64_MAX;
		valuation[nextstatevar].second = (!truthval) * UINT64_MAX;
	}
	for (auto &lit : model.invariantConstraints())
	{
		valuation[MyMinisat::var(lit)].first = (!sign(lit)) * UINT64_MAX;
		valuation[MyMinisat::var(lit)].second = sign(lit) * UINT64_MAX;
	}

	while (!finished)
	{
		//new present state valuation
		for (auto &lit : *pred)
		{
			valuation[MyMinisat::var(lit)].first = (!sign(lit)) * UINT64_MAX;
			valuation[MyMinisat::var(lit)].second = (sign(lit)) * UINT64_MAX;
		}

		size_t limit = (
				(pred->size() <= (ternsimBitWidth + firstTryIdx)) ?
						pred->size() : ternsimBitWidth + firstTryIdx);
		for (size_t i = firstTryIdx; i < limit; ++i)
		{
			MyMinisat::Lit lit = (*pred)[i];
			uint64_t mask = 1;
			mask = ~(mask << (63 - (i - firstTryIdx))); // 11..11011..11
			valuation[MyMinisat::var(lit)].first &= mask;
			valuation[MyMinisat::var(lit)].second &= mask;
		}

		MyMinisat::Var maxIdx;
		if (!model.invariantConstraints().empty())
		{
			size_t numConstr = model.invariantConstraints().size();
			MyMinisat::Var maxConstr = MyMinisat::var(
					model.invariantConstraints()[numConstr - 1]);
			maxIdx = std::max(maxNextStateVar, maxConstr);
		}
		else
			maxIdx = maxNextStateVar;
		/* apply simulation vector */
		model.ternSim(valuation, maxIdx);

		//determine which X is allowed and which not
		//by whether it has been propagated to the next state
		//or not.
		uint64_t xIndicator = UINT64_MAX;
		for (auto &lit : *succ)
		{
			MyMinisat::Var nextstatevar = MyMinisat::var(
					model.nextStateFn(model.varOfLit(lit)));
			//OR is zero if there is an X at any place -> AND is zero in the indicator
			//at exactly the index of the in-admissible X
			xIndicator &= (valuation[nextstatevar].first
					| valuation[nextstatevar].second);
		}
		//do not propagate X to invariant constraints!
		for (auto &constr : model.invariantConstraints())
		{
			xIndicator &= (valuation[MyMinisat::var(constr)].first
					| valuation[MyMinisat::var(constr)].second);
		}
		int dropIndex = -1;
		//only check as much X's as we have applied
		//state size could be less than bit-width
		int kLimit = 0;
		//the last portion could be less than the kLimit span
		//(portions only possible if the state-size is greater than the bit-width)
		if (pred->size() > ternsimBitWidth)
		{
			assert(firstTryIdx <= pred->size());
			size_t d = (pred->size() - firstTryIdx);
			kLimit = (d < ternsimBitWidth) ? (ternsimBitWidth - d) : 0;
		}
		else
		{
			kLimit = ternsimBitWidth - pred->size();
		}
		//assert(presentState.size() > 0);
		for (int k = (int) ternsimBitWidth - 1; k >= kLimit; --k)
		{
			int xAtIdx = (xIndicator >> k) % 2; // 1 -> no X, 0 -> X
			if (xAtIdx)
			{
				// no X propagated with this index at all? X is admissible!
				dropIndex = firstTryIdx + (ternsimBitWidth - 1 - k);
				break;
			}
		}

		if (dropIndex > -1)
		{
			assert(dropIndex < (int)pred->size());
			valuation[MyMinisat::var((*pred)[dropIndex])].first = 0;
			valuation[MyMinisat::var((*pred)[dropIndex])].second = 0;
			//remove literal from presentState
			(*pred)[dropIndex] = pred->back();
			pred->pop_back();
			//dropIndices.push_back(dropIndex);
			firstTryIdx = 0;
		}
		else
		{
			//nothing found which can be dropped
			firstTryIdx += ternsimBitWidth;
			//will we still try?
			if (firstTryIdx >= pred->size())
				finished = true; //no!
		}
	}

	//exit(0);
	std::sort(pred->begin(), pred->end());
}

void PoGeneralizer::generalizeBadByTernSim(LitVec &primedInputs)
{
	//try the first "ternsimBitWidth" state vars as X
	std::pair<uint64_t, uint64_t> initVal(0, 0);
	MyMinisat::Var maxNextStateVar = 0;
	size_t firstTryIdx = 0;
	bool finished = false;
	Model::ternsimVec valuation(model.getMaxVar() + 1, initVal);
	Model::ternsimVec valuationTargEnl(model.getMaxVar() + 1, initVal);
	valuation[0].first = 0;
	valuation[0].second = UINT64_MAX;
	/*
	 * always try ternsimBitWidth portions of
	 * X-ed present state bits in parallel
	 */
	for (auto &lit : primedInputs)
	{
		valuationTargEnl[MyMinisat::var(lit)].first = (!sign(lit)) * UINT64_MAX;
		valuationTargEnl[MyMinisat::var(lit)].second = sign(lit) * UINT64_MAX;
	}
	for (auto &lit : *inputs)
	{
		valuation[MyMinisat::var(lit)].first = (!sign(lit)) * UINT64_MAX;
		valuation[MyMinisat::var(lit)].second = sign(lit) * UINT64_MAX;
	}
	//present state valuation
	for (auto &lit : *pred)
	{
		/*valuation[MyMinisat::var(lit)].first = (!sign(lit)) * UINT64_MAX;
		 valuation[MyMinisat::var(lit)].second = sign(lit) * UINT64_MAX;*/
		if (MyMinisat::var(model.nextStateFn(model.varOfLit(lit)))
				> maxNextStateVar)
			maxNextStateVar = MyMinisat::var(
					model.nextStateFn(model.varOfLit(lit)));
	}
	assert(pred->size() == (unsigned)(model.endLatches() - model.beginLatches()));

	while (!finished)
	{
		/*** valuation ***/
		//constant zero:
		//new present state valuation
		for (auto &lit : *pred)
		{
			valuation[MyMinisat::var(lit)].first = (!sign(lit)) * UINT64_MAX;
			valuation[MyMinisat::var(lit)].second = (sign(lit)) * UINT64_MAX;
		}

		size_t limit = (
				(pred->size() <= (ternsimBitWidth + firstTryIdx)) ?
						pred->size() : ternsimBitWidth + firstTryIdx);
		for (size_t i = firstTryIdx; i < limit; ++i)
		{
			MyMinisat::Lit lit = pred->at(i);
			uint64_t mask = 1;
			mask = ~(mask << (63 - (i - firstTryIdx))); // 11..11011..11
			valuation[MyMinisat::var(lit)].first &= mask;
			valuation[MyMinisat::var(lit)].second &= mask;
		}
		MyMinisat::Var maxIdx = 0;
		if (!model.invariantConstraints().empty())
		{
			size_t numConstr = model.invariantConstraints().size();
			MyMinisat::Var maxConstr = MyMinisat::var(
					model.invariantConstraints()[numConstr - 1]);
			maxIdx = std::max(MyMinisat::var(model.error()), maxConstr);
		}
		else
			maxIdx = MyMinisat::var(model.error());
		maxIdx = std::max(maxNextStateVar, maxIdx);
		/* apply simulation vector */
		model.ternSim(valuation, maxIdx);

		//determine which X is allowed and which not
		//by whether it has been propagated to the error
		//or not.
		uint64_t xIndicator = UINT64_MAX;
		for (auto &constr : model.invariantConstraints())
		{
			xIndicator &= (valuation[MyMinisat::var(constr)].first
					| valuation[MyMinisat::var(constr)].second);
		}

		//new present state after one round of simulation
		for (VarVec::const_iterator i = model.beginLatches();
				i != model.endLatches(); ++i)
		{
			MyMinisat::Var nextstatevar = MyMinisat::var(model.nextStateFn(*i));
			//Just propagate the X for the checker circuit (1-step target enlargement)
			if (MyMinisat::sign(model.nextStateFn(*i))) //negated next state functions
			{
				//negation: swap 0- and 1-vector
				valuationTargEnl[i->var()].first =
						valuation[nextstatevar].second;
				valuationTargEnl[i->var()].second =
						valuation[nextstatevar].first;
			}
			else
				valuationTargEnl[i->var()] = valuation[nextstatevar];
		}
		//primed inputs for checker circuit

		//error and constraint valuation
		assert((unsigned)maxIdx <= model.getMaxVar());
		model.ternSim(valuationTargEnl, maxIdx);
		//OR is zero if there is an X at any place -> AND is zero in the indicator
		//at exactly the index of the in-admissible X
		xIndicator &= (valuationTargEnl[MyMinisat::var(model.error())].first
				| valuationTargEnl[MyMinisat::var(model.error())].second);
		for (auto &constr : model.invariantConstraints())
		{
			xIndicator &= (valuationTargEnl[MyMinisat::var(constr)].first
					| valuationTargEnl[MyMinisat::var(constr)].second);
		}

		int dropIndex = -1;
		//only check as much X's as we have applied
		//state size could be less than bit-width
		int kLimit = 0;
		//the last portion could be less than the kLimit span
		//(portions only possible if the state-size is greater than the bit-width)
		if (pred->size() > ternsimBitWidth)
		{
			assert(firstTryIdx <= pred->size());
			size_t d = (pred->size() - firstTryIdx);
			kLimit = (d < ternsimBitWidth) ? (ternsimBitWidth - d) : 0;
		}
		else
		{
			kLimit = ternsimBitWidth - pred->size();
		}
		//assert(presentState.size() > 0);
		for (int k = (int) ternsimBitWidth - 1; k >= kLimit; --k)
		{
			int xAtIdx = (xIndicator >> k) % 2; // 1 -> no X, 0 -> X
			if (xAtIdx)
			{
				// no X propagated with this index at all? X is admissible!
				dropIndex = firstTryIdx + (ternsimBitWidth - 1 - k);
				break;
			}
		}

		if (dropIndex > -1)
		{
			assert(dropIndex < (int)pred->size());
			valuation[MyMinisat::var((*pred)[dropIndex])].first = 0;
			valuation[MyMinisat::var((*pred)[dropIndex])].second = 0;
			//remove literal from presentState
			(*pred)[dropIndex] = pred->back();
			pred->pop_back();
			//dropIndices.push_back(dropIndex);
			firstTryIdx = 0;
		}
		else
		{
			//nothing found which can be dropped
			firstTryIdx += ternsimBitWidth;
			//will we still try?
			if (firstTryIdx >= pred->size())
				finished = true; //no!
		}
	}

	std::sort(pred->begin(), pred->end());
}

LitVec PoGeneralizer::stateOfJust()
{
	LitVec ret;
	LitVec primedErrorJust; //justification for primed error, later used as successor (only for target enlargement case)
	bool targetEnl = !succ; //if target enlargement -> traverse present and primed variant
	//find prime state justification j' for primed error -> find present state justification for j'
	for (int rounds = 0; rounds < (targetEnl ? 2 : 1); ++rounds)
	{
		cout << "starting just" << endl;
		AigVec justPath;
		set<MyMinisat::Var> justLatches; //latches at least required for justification
		LitVec values(model.getMaxVar());

		if (rounds == 1) //target enlargement, justification for primed error already found
		{
			assert(!succ);
			succ = &primedErrorJust;
		}

		//propagate values from CIs - can not use Solver-Model since we did variable elimination on the TR
		if(modelSlv)
			model.assignValues(modelSlv, values, (rounds == 0 && targetEnl));
		else
			model.assignValues(fullAssignment, values, (rounds == 0 && targetEnl)); //POGP-mode

		//Find justification path
		model.findJustPath(justPath, justLatches, succ, values); //traverse backward
		//at this point, justLatches also contains PIs

		//introduce priorities
		vector<int> prio(model.getMaxVar(), 0);
		prio[0] = INT_MAX; //constant 0
		for (VarVec::const_iterator i = model.beginInputs();
				i != model.endInputs(); ++i)
		{
			prio[i->var()] = INT_MAX; // max Prio for PI
		}
		assert(pred->size() == (unsigned)(model.endLatches() - model.beginLatches()));
		for (size_t k = 0; k < pred->size(); ++k) //assign priority as the predecessor's order
		{
			prio[MyMinisat::var((*pred)[k])] = (k + 1); //start with 1
		}
		for (VarVec::const_iterator i = model.beginLatches();
				i != model.endLatches(); ++i)
		{
			// only used for latches from the justification path - assign all anyway
			if (justLatches.find(i->var()) == justLatches.end())
				prio[i->var()] = INT_MAX; // max Prio for latches which are unneccesary for justification
		}
		while (true)
		{
			int latchId = model.propagateJustPrio(justPath, justLatches, succ,
					values, prio);
			if (latchId == -1)
				break;
			assert(model.isLatch(MyMinisat::var((*pred)[latchId - 1]))); // index -1 because we started with 1
			prio[MyMinisat::var((*pred)[latchId - 1])] = INT_MAX;
			MyMinisat::Lit litForJust = values[MyMinisat::var((*pred)[latchId - 1])]; //WATCH OUT: other priorities change behaviour here
			if (targetEnl && rounds == 0) //first iteration -> fill successor
			{
				primedErrorJust.push_back(litForJust);
			}
			else
				ret.push_back(litForJust);
		}
	}
	//cout << "justification done, reduced from: " << pred->size() << " to "
	//		<< ret.size() << endl;
	sort(ret.begin(), ret.end());

	return ret;
}

LitVec PoGeneralizer::stateOfSATCoverFree()
{
	LitVec ret;
	MSLitVec assumps, notS;

	/*for (auto &lit : *succ)
	 {
	 assumps.push(model.primeLit(lit));
	 MyMinisat::Lit litpos = MyMinisat::mkLit(MyMinisat::var(lit), false); // v^t = 1
	 MyMinisat::Lit auxlit = MyMinisat::mkLit(
	 model.get01XAuxVar(MyMinisat::var(lit)), false); // v^f = 1
	 if (!MyMinisat::sign(lit)) //~succ is negated
	 notS.push(auxlit);
	 else
	 notS.push(litpos);
	 }*/

	for (auto &lit : *succ)
		assumps.push(model.primeLit(lit));

	//instead of assignment, add clauses
	for (auto &cube : *frCubes) //NEGATE!
	{
		MSLitVec cl;
		for (auto &lit : cube)
		{
			//differentiate for state variables
			if (MyMinisat::var(lit) >= model.beginLatches()->var()
					&& MyMinisat::var(lit) < model.endLatches()->var())
			{
				MyMinisat::Lit litpos = MyMinisat::mkLit(MyMinisat::var(lit), false); // v^t = 1
				MyMinisat::Lit auxlit = MyMinisat::mkLit(
						model.get01XAuxVar(MyMinisat::var(lit)), false); // v^f = 1
				if (!MyMinisat::sign(lit))
					cl.push(auxlit);
				else
					cl.push(litpos);
			}
			else
				cl.push(~lit);
		}
		coverSatSlv->addClause(cl);
	}

	bool rv = coverSatSlv->solve(assumps);
	assert(rv);

	for (VarVec::const_iterator i = model.beginLatches();
			i != model.endLatches(); ++i)
	{
		MyMinisat::lbool val = coverSatSlv->modelValue(i->var());
		MyMinisat::lbool auxVal = coverSatSlv->modelValue(
				model.get01XAuxVar(i->var()));
		if (val != MyMinisat::l_Undef)
		{
			if (val != MyMinisat::l_False || auxVal != MyMinisat::l_False) // (0,0) don't care
			{
				MyMinisat::Lit la = i->lit(val == MyMinisat::l_False);
				ret.push_back(la);
			}
			else
				assert(val == MyMinisat::l_False && auxVal == MyMinisat::l_False);
		}
	}

	return ret;
}

LitVec PoGeneralizer::stateOfStructRev()
{
	//technically, succ is a predecessor in Reverse PDR
	// -> present state = succ, next state = pred

	assert(revpdr);

	std::vector<unsigned int> arbitraryPresentStateBits;
	std::vector<int> prunableNextStateVars;
	std::vector<int> possPrunableNextStateVars;

	ternBool l_T((uint8_t) 1);
	ternBool l_F((uint8_t) 0);
	ternBool l_U((uint8_t) 2);

	bool sortps = true;

	//alter present state - add some literals with high out degree
	uint8_t found = 0;

	/****************/
	//"left hand side approach"
	unsigned int nLatches = (model.endLatches() - model.beginLatches());

	LitSet presentStateSet;
	if (succ)
	{
		for (auto &lit : *succ)
			presentStateSet.insert(lit);
		succ->clear();
	}
	else //init predecessor
	{
		LitVec init = model.getInitLits();
		for (auto &lit : init)
		{
			presentStateSet.insert(lit);
		}
		succ = new LitVec();
	}

	for (SvPrioQueue::const_iterator i = orderedSuppDegr.begin();
			i != orderedSuppDegr.end(); ++i)
	{
		MyMinisat::Lit la = getAssignment(i->index);
		if (presentStateSet.find(la) == presentStateSet.end() && i->index > 0
				&& i->outDegr > 1)
		{
			if (i->outDegr < (nLatches / 2))
				break;	//heuristics, stop when outdegree seems not worth it
			found++;
			presentStateSet.insert(la);
		}
	}

	for (auto &lit : presentStateSet)
		succ->push_back(lit);
	sortps = false;
	/***************/

	if (sortps)
		std::sort(succ->begin(), succ->end());

	int posVar = model.beginInputs()->var();

	//get present-state variables not contained in present-state cube to block
	std::vector<ternBool> valuation(model.getMaxVar() + 1, l_U);
	for (auto &lit : *succ)
	{
		valuation[lit.x >> 1] = !MyMinisat::sign(lit);
		while (MyMinisat::var(lit) > posVar)
		{
			arbitraryPresentStateBits.push_back(posVar);
			posVar++;
		}
		if (MyMinisat::var(lit) == posVar)
		{
			posVar++;
		}
	}
	posVar = MyMinisat::var(succ->back()) + 1;
	while (posVar < model.endLatches()->var())
	{
		arbitraryPresentStateBits.push_back(posVar);
		posVar++;
	}

	//std::cout << "circuit propagation and traversation ..."; std::cout.flush();
	//propagate constants - find irrelevant unassigned support parts
	//could already find some (non-trivial) constant functions
	std::vector<unsigned int> outDegreesUnderAssignment = model.outDegrSuppVars;
	std::set<unsigned int> constants;
	model.circuitPropagation(valuation, constants);
	for (VarVec::const_iterator i = model.beginLatches();
			i != model.endLatches(); ++i)
	{
		if (model.nextStateFn(*i).x <= 1)
		{
			constants.insert(i->var());
			continue;		//no trivially constant functions
		}

		std::set<unsigned int> irrSuppVars = model.circuitTrav(valuation,
				i->var());
		for (std::set<unsigned int>::const_iterator i = irrSuppVars.begin();
				i != irrSuppVars.end(); ++i)
			outDegreesUnderAssignment[*i]--;
	}

	//check for common inputs/arbitrary state variables
	for (VarVec::const_iterator i = model.beginLatches();
			i != model.endLatches(); ++i)
	{
		if (model.nextStateFn(*i).x <= 1)
			continue;		//no trivially constant functions

		std::vector<unsigned int> suppI = model.getTransitionSupport(*i);
		//prune next-states by structural checks
		bool intersect = false;
		for (auto &suppVar : suppI)
		{
			//only check for inputs and arbitrary present state variables
			if (std::binary_search(arbitraryPresentStateBits.begin(),
					arbitraryPresentStateBits.end(), suppVar))
			{
				//is there at least one support variable with out degree > 1?
				if (outDegreesUnderAssignment[suppVar] > 1)
				{
					intersect = true;
					break;
				}
			}
		}
		//add !variable! to log disjoint next-state coi, non-constant
		if (!intersect && constants.find(i->var()) == constants.end())
			possPrunableNextStateVars.push_back(i->var());
	}

	//simulate random patterns -> no change? assume constant
	PatternFix res = model.simFixRndPatterns(*succ);
	for (auto &possPrunableVar : possPrunableNextStateVars)
	{
		if (!res[possPrunableVar - model.beginLatches()->var()].all()
				&& !res[possPrunableVar - model.beginLatches()->var()].none())
			prunableNextStateVars.push_back(possPrunableVar);
	}

	//prune possible next-state vars
	//exploit the fact that latch vars are enumerated
	std::sort(prunableNextStateVars.begin(), prunableNextStateVars.end());
	posVar = model.beginLatches()->var();

	for (size_t i = 0; i < pred->size();)
	{
		if (std::binary_search(prunableNextStateVars.begin(),
				prunableNextStateVars.end(), MyMinisat::var((*pred)[i])))
		{
			(*pred)[i] = pred->back();
			pred->pop_back();
		}
		else
			++i;
	}

	std::sort(pred->begin(), pred->end());

	assert(pred->size() > 0);

	return *pred;
}

LitVec PoGeneralizer::stateOfMSHeuristics()
{
	LitVec newState;

	if (!pacoseOrLifting)	// && succ != 0)
	{
		clock_t clkBeforeLifting = clock();
		//size_t newStateLift = stateOf(fr, succ);
		//size_t newStateLift = stateOfTernSim(fr, succ);
		LitVec newStateLift = stateOfIGBG();
		clock_t clkLiftDur = clock() - clkBeforeLifting;

		clock_t clkBeforeMaxSat = clock();
		LitVec newStatePac;
		if (model.endLatches() - model.beginLatches() < 3000) //avoid unsolvable Pacose-MAXSAT problems
		//TODO: much rather use a timeout on Pacose ...
		{
			newStatePac = stateOfMS01XFix();
		}
		else
		{
			pacoseOrLifting = 2;
			cout << "only igbg" << endl;
			return newStateLift;
		}
		clock_t clkMaxSatDur = clock() - clkBeforeMaxSat;

		//determine which method to use for further calls
		//HEURISTICS!
		if (newStateLift.size() > newStatePac.size())
		{
			int diff = newStateLift.size() - newStatePac.size();
			//pacose could be helpful since it produces more general pos
			int factor = diff * 200; //200; //should be a function of the lifting difference
			int lesserFactor = diff * 2; // * 4;// 3;
			if (clkMaxSatDur > factor * clkLiftDur || clkMaxSatDur > 10000000)
			{
				//refrain from using pacose, because it just takes too long
				pacoseOrLifting = 2;
				cout << "too slow: only igbg" << endl;
			}
			else
			{
				if (clkMaxSatDur > lesserFactor * clkLiftDur)
				{
					//use both
					pacoseOrLifting = 3;
					cout << "combination" << endl;
				}
				else
				{
					//pacose is fast and performs well -> use Pacose
					pacoseOrLifting = 1;
					cout << "only maxsat" << endl;
				}
			}
			newState = newStatePac;
		}
		else
		{
			//refrain from using pacose
			pacoseOrLifting = 2;
			newState = newStateLift;
			cout << "too bad: only igbg" << endl;
		}
	}

	switch (pacoseOrLifting)
	{
	case 1:
		newState = stateOfMS01XFix();
		break;
	case 2:
		newState = stateOfIGBG();
		break;
	case 3:
		LitVec newStateLift = stateOfIGBG();
		if (newStateLift.size() < 5000)
		{
			LitVec newStatePac = stateOfMS01XFix();
			if (newStatePac.size() < newStateLift.size())
			{
				newState = newStatePac;
				//cout << "pacose contributed a more general po" << endl;
				runsWithoutContribution = 0;
			}
			else
			{
				newState = newStateLift;
				//cout << "pacose did not contribute a more general po" << endl;
				runsWithoutContribution++; //*consecutive* runs without any helpful contribution of Pacose
				if (runsWithoutContribution > contributionThresh)
					pacoseOrLifting = 2; //switch back to only lifting/ternsim
			}
		}
		else
			newState = newStateLift;
		dontCares.clear();
		break;
	}
	return newState;
}

LitVec PoGeneralizer::stateOfLiftIGBG()
{
	LitVec ret = stateOfIGBG();
	*pred = ret;
	//cout << "ret.size() after IGBG: " << ret.size() << endl;
	ret = stateOfLifting();
	//cout << "ret.size() after Lifting: " << ret.size() << endl;
	return ret;
}

void PoGeneralizer::resetStates()
{
	if (!deletePointers)
	{
		//managed by PoGeneralizer
		delete pred;
		delete inputs;
		//////
		pred = NULL;
		succ = NULL;
		inputs = NULL;
		frCubes = NULL;
		fullAssignment = NULL;
	}
	modelSlv = NULL;
}

//get assignment of variable - from modelSolver or fullAssignment
MyMinisat::Lit PoGeneralizer::getAssignment(MyMinisat::Var v)
{
	MyMinisat::Lit retLit;
	if (modelSlv)
	{
		MyMinisat::lbool val = modelSlv->modelValue(v);
		assert(val != MyMinisat::l_Undef);
		retLit = MyMinisat::mkLit(v, (val == MyMinisat::l_False));
	}
	else
	{
		assert(fullAssignment);
		assert(v < (int)fullAssignment->size());
		retLit = (*fullAssignment)[v];
	}
	return retLit;
}

//TODO: free modes for IC3 operation
//extend frames by one: only in "free" mode
void PoGeneralizer::newFrame(size_t k)
{
	if (!freeMode)
		return;

	assert(k >= 0 && k == frameSolvers.size());
	MyMinisat::Solver *tsSlv = model.newTernSatSlv();

	switch (genType)
	{
	case S01X_FREE:
		model.loadTransitionRelation01XElim(*tsSlv);
		frameSolvers.push_back(tsSlv);
		break;
	case MS01X_FREE:
		break;
	case SAT_COVER_FREE:
		break;
	case GREEDY_QBF_FREE:
		break;
	case MAXQBF_FREE:
		break;
	case ILP_COVER_FREE:
		freeMode = true;
		break;
	default:
		throw std::runtime_error(
				"Wrong operation mode (free-mode and no free admissible method specified)!");
		break;
	}

	if (k == 0)
	{
		//TODO load initial conditions
		//	model.loadInitialCondition01X(*tsSlv);
	}

}

void PoGeneralizer::simplifySolver()
{
	switch (genType)
	{
	case NONE:
		break;
	case LIFTING:
	case LIFTING_LITDROP:
		liftingSatSlv->simplify();
		break;
	case S01X_FIX:
	case S01X_FREE:
		ternSatSlv->simplify();
		break;
	case IGBG:
	case IGBG_LITDROP:
		igbgSatSlvSucc->simplify();
		igbgSatSlvBad->simplify();
		break;
	case GENTR:
	case GENTR_LITDROP:
		GeNTRSlv->simplify();
		break;
	case SAT_COVER_FIX:
	case SAT_COVER_FREE:
		coverSatSlv->simplify();
		break;
	case IGBG_LIFTING_HEURISTICS:
		break; //TODO: think about
	case MS01X_FIX:
	case MS01X_FREE:
	case MS01X_INCREMENTAL:
	case GREEDY_QBF_FIX:
	case GREEDY_QBF_FREE:
	case MAXQBF_FIX:
	case MAXQBF_FREE:
	case ILP_COVER:
	case ILP_COVER_FREE:
	case MS_HEURISTICS:
	case JUSTIFICATION:
	case TERNSIM:
	case REV_STRUCT:
		break;
	default:
		throw std::runtime_error(
				"PO-Generalization simplify(): Inadmissible mode of operation.");
		break;
	}
}

void PoGeneralizer::deinitMaxSAT()
{
	delete maxSatSlv;
	maxSatSlv = NULL;
}

void PoGeneralizer::deinitMaxSATIncremental()
{
	ipamir_release(maxSatSlvIpamir);
	maxSatSlvIpamir = NULL;
}

void PoGeneralizer::deinitMaxQBF()
{
	delete maxQbfSlv;
	maxQbfSlv = NULL;

	muxAuxExist.clear();
	muxAuxForall.clear();
	muxSelect.clear();
}

} /* namespace IC3 */
