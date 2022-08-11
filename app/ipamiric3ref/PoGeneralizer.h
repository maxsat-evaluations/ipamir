/*
 * PoGeneralizer.h
 *
 *  Created on: Jan 1, 2021
 *      Author: seufert
 */
#include "Model.h"

#include<algorithm>
#include<random>

#ifndef SRC_POGENERALIZER_H_
#define SRC_POGENERALIZER_H_


//#define IGBG_WITH_DECISIONS

namespace IC3
{

struct IgbgOpt
{
	bool useLiteralRotation;
	bool sortAssumpsBySolverActivity;
	int maxFails;
	int maxTries;
	bool shuffle;
	bool respectProperty;

	IgbgOpt()
	{
		useLiteralRotation = false;
		sortAssumpsBySolverActivity = false;
		maxFails = INT_MAX;
		maxTries = INT_MAX;
		shuffle = false;
		respectProperty = false;
	}

	IgbgOpt(bool _solvAct, int _maxFails, int _maxTries, bool _shuffle, bool _respectProperty, bool _litrot):
		useLiteralRotation(_litrot),
		sortAssumpsBySolverActivity(_solvAct),
		maxFails(_maxFails),
		maxTries(_maxTries),
		shuffle(_shuffle),
		respectProperty(_respectProperty)
	{}
};

struct LiftingOpt
{
	bool useLiteralRotation;
	int litDropMaxFails;
	int litDropMaxTries;
	int litRotMaxFails;
	int litRotMaxTries;
	bool shuffle;
	bool useApproxSat;
	bool selfLoops;
	bool extCall;

	LiftingOpt()
	{
		useLiteralRotation = false;
		litDropMaxFails = INT_MAX;
		litDropMaxTries = INT_MAX;
		litRotMaxFails = INT_MAX;
		litRotMaxTries = INT_MAX;
		shuffle = false;
		useApproxSat = false;
		selfLoops = false;
		extCall = false;
	}

	LiftingOpt(bool _litrot, int _litDropMaxFails, int _litDropMaxTries, int _litRotMaxFails, int _litRotMaxTries, bool _shuffle, bool _approxsat, bool _selfLoops, bool _extCall):
		useLiteralRotation(_litrot),
		litDropMaxFails(_litDropMaxFails),
		litDropMaxTries(_litDropMaxTries),
		litRotMaxFails(_litRotMaxFails),
		litRotMaxTries(_litRotMaxTries),
		shuffle(_shuffle),
		useApproxSat(_approxsat),
		selfLoops(_selfLoops),
		extCall(_extCall)
	{}
};

struct GeneralOpt
{
	bool sortAssumpsByIc3refActivity;
	bool reverse;

	IgbgOpt igbg;
	LiftingOpt lift;

	GeneralOpt()
	{
		sortAssumpsByIc3refActivity = false;
		reverse = false;
	}

	GeneralOpt(bool _ic3refAct, bool _rev, bool _cubLitRot, IgbgOpt _igbg, LiftingOpt _lift):
		sortAssumpsByIc3refActivity(_ic3refAct),
		reverse(_rev),
		igbg(_igbg),
		lift(_lift)
	{}
};


class PoGeneralizer
{
public:

	enum GenType
	{
		NONE,
		LIFTING,
		LIFTING_LITDROP,
		TERNSIM,
		S01X_FIX,
		S01X_FREE,
		MS01X_FIX,
		MS01X_FREE,
		MS01X_INCREMENTAL,
		IGBG,
		IGBG_LITDROP,
		GREEDY_COVER,
		ILP_COVER,
		ILP_COVER_FREE,
		SAT_COVER_FIX,
		SAT_COVER_FREE,
		GENTR,
		GENTR_LITDROP,
		GREEDY_QBF_FIX,
		GREEDY_QBF_FREE,
		MAXQBF_FIX,
		MAXQBF_FREE,
		REV_STRUCT,
		MS_HEURISTICS,
		IGBG_LIFTING_HEURISTICS,
		JUSTIFICATION
	};

	PoGeneralizer(GenType _genType, Model &_model, bool _revpdr,
			GeneralOpt& _genOpt,
			SlimLitOrder *_order = NULL, LitVec *_inputs = NULL,
			LitVec *_pred = NULL, LitVec *_succ = NULL, vector<LitVec> *_frCubes = NULL,
			LitVec *_fullAssignment = NULL, MyMinisat::Solver *modelSlv = NULL,
			bool _deletePointers = false);
	virtual ~PoGeneralizer();

	LitVec stateOf(MyMinisat::Solver *slv = NULL, LitVec* succ = NULL);

	void newFrame(size_t k);

	LitVec* getPred() { return pred; }
	LitVec* getSucc() { return succ; }

	void simplifySolver();

private:

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

	GeneralOpt genOpt;

	bool revpdr;

	//Some approaches require primed inputs or full assignments
	//When we apply generalization to POGP within IC3, we
	//have a consecution solver available
	MyMinisat::Solver *modelSlv;

	GenType genType;
	LitVec *inputs;
	LitVec *pred;
	LitVec *succ;
	vector<LitVec> *frCubes;

	Model &model;

	//Unate cover: full assignment
	LitVec *fullAssignment;

	//depQBF greedy QBF / maxQBF
	vector<VarID> muxAuxExist;
	vector<VarID> muxAuxForall;
	vector<VarID> muxSelect;

	Pacose *maxSatSlv;
	QDPLL *greedyQbfSlv;
	MyMinisat::Solver *ternSatSlv;
	MyMinisat::Solver *igbgSatSlvSucc;
	MyMinisat::Solver *igbgSatSlvBad;
	MyMinisat::Solver *coverSatSlv;
	MyMinisat::Solver *liftingSatSlv;
	MyMinisat::Solver *GeNTRSlv;
	quantom::Quantom *maxQbfSlv;
	void *maxSatSlvIpamir;

	//vector of frame solvers -> free mode
	vector<void*> frameSolvers;

	//Lifting with invariants
	MyMinisat::Lit notInvConstraints;
	MyMinisat::Lit notUnprimedInvConstraints;

	//Rev PDR struct. approach
	//state variables with disjoint support sets
	std::vector<unsigned int> stateVarsWithDisjSupp;
	//ordered out degrees of support variables (of transition functions)
	SvPrioQueue orderedSuppDegr;
	//stores for each latch: exclusive variables / (exclusive variables + shared variables)
	std::vector<float> exclSharedQuotient;

	//MSHeuristics fields
	uint8_t pacoseOrLifting = 0; //0:neither, 1: pacose, 2: lifting, 3: combination
	unsigned int runsWithoutContribution = 0;
	const unsigned int contributionThresh = 10;
	vector<unsigned int> dontCares; // don't care vector - literals which are currently set to X

	//are the pointers managed by this? If yes, delete objects in destructor
	bool deletePointers;

	//are we running in "free" mode? (no present state valuation fixed, only IC3 frame constraints)
	bool freeMode;

	//variable order inherited from ic3ref / managed by ic3ref
	SlimLitOrder *ic3refLitOrder;

	void initLifting();
	void initS01X();
	void initMS01X();
	void initMS01XIncremental();
	void initIGBG();
	void initGeNTR();
	void initSATCover();
	void initGreedyQBF();
	void initMaxQBF();
	void initILP();
	void initJust();
	void initRevStruct();

	/*
	 * Some solvers do not allow for incremental use
	 */
	void deinitMaxSAT();
	void deinitMaxSATIncremental();
	void deinitMaxQBF();


	LitVec stateOfLifting(bool litdrop = false);

	LitVec stateOfTernSim();
	//- tern sim helper -//
	void generalizeByTernSim();
	void generalizeBadByTernSim(LitVec& primedInputs);
	//-------------------//


	LitVec stateOfS01XFix();
	LitVec stateOfS01XFree();
	LitVec stateOfMS01XFix();
	LitVec stateOfMS01XFree();
	LitVec stateOfMS01XIncremental();
	//IGBG helper
	void rotateForIGBG(vector<MyMinisat::Lit>& ret, vector<int>& reason_frequ);
	void generalizeByIGBG(LitVec &ret);
	LitVec stateOfIGBG(bool litdrop = false);
	LitVec stateOfJust();
	// - Justification helper -//
	void assignValues(LitVec &model, bool primeRun);

	//MS01X / IGBG Heuristics
	LitVec stateOfMSHeuristics();

	//Lifting / IGBG Heuristics
	LitVec stateOfLiftIGBG();

	LitVec stateOfGreedyCover();
#ifdef COMPILE_WITH_GUROBI
	LitVec stateOfILPCover(); //unate !! TODO: binate or not?
	LitVec stateOfILPCoverFree();
#endif
	LitVec stateOfSATCoverFix();
	LitVec stateOfSATCoverFree();
	LitVec stateOfGeNTR(bool litdrop = false);

	LitVec stateOfGreedyQBFFix();
	LitVec stateOfGreedyQBFFree();
	LitVec stateOfMaxQBFFix();
	LitVec stateOfMaxQBFFree();

	LitVec stateOfStructRev();

	void resetStates();
	MyMinisat::Lit getAssignment(MyMinisat::Var v);

};

static PoGeneralizer* extractPoGenInstanceFromFile(string& path,
		PoGeneralizer::GenType genType, Model &model, bool revpdr,
		GeneralOpt& genOpt)
{
	string line;
	ifstream dimacsFile(path);
	vector<LitVec> *frCubes;
	LitVec *inputs, *pred, *succ, *fullAssignment;
	enum readMode
	{
		INPUTS, PRED, SUCC, FULLASSIGNMENT, FRCUBES
	};
	readMode rdMode;

	inputs = new LitVec();
	pred = new LitVec();
	succ = new LitVec();
	fullAssignment = new LitVec();
	frCubes = new vector<LitVec>();



	if (dimacsFile.is_open())
	{
		rdMode = INPUTS;
		int lineidx = 0;
		while (getline(dimacsFile, line))
		{
			if (line.find("pred:") != string::npos)
			{
				rdMode = PRED;
				continue;
			}
			if (line.find("succ:") != string::npos)
			{
				rdMode = SUCC;
				continue;
			}
			if (line.find("fullAssignment:") != string::npos)
			{
				rdMode = FULLASSIGNMENT;
				continue;
			}
			if (line.find("cnf:") != string::npos)
			{
				rdMode = FRCUBES;
				continue;
			}

			LitVec* c;
			LitVec frCube;
			switch (rdMode)
			{
			case FRCUBES:
				c = &frCube;
				break;
			case INPUTS:
				c = inputs;
				break;
			case PRED:
				c = pred;
				break;
			case SUCC:
				c = succ;
				break;
			case FULLASSIGNMENT:
				c = fullAssignment;
			}
			string substring = line;
			while (substring.find(" ") != string::npos)
			{
				string lit = substring.substr(0, substring.find(" "));
				substring = substring.substr(substring.find(" ") + 1);
				MyMinisat::Lit msLit = MyMinisat::toLit(std::stoi(lit));
				c->push_back(msLit);
			}

			if (!frCube.empty() && rdMode == FRCUBES)
			{
				std::sort(frCube.begin(), frCube.end());
				frCubes->push_back(frCube);
			}

			lineidx++;
		}
		dimacsFile.close();
	}

	//create Generalizer instance
	PoGeneralizer *poGen = new PoGeneralizer(genType, model, revpdr, genOpt, NULL, inputs, pred, succ, frCubes, fullAssignment, NULL, true);

	return poGen;
}


static PoGeneralizer::GenType determineGenType(string &str)
{
	PoGeneralizer::GenType ret;
	if(str == "none")
		ret = PoGeneralizer::NONE;
	else if(str == "lifting")
		ret = PoGeneralizer::LIFTING;
	else if(str == "lifting_literal_dropping")
		ret = PoGeneralizer::LIFTING_LITDROP;
	else if(str == "igbg")
		ret = PoGeneralizer::IGBG;
	else if(str == "igbg_literal_dropping")
		ret = PoGeneralizer::IGBG_LITDROP;
	else if( str == "justification" )
		ret = PoGeneralizer::JUSTIFICATION;
	else if( str == "s01x" )
		ret = PoGeneralizer::S01X_FIX;
	else if( str == "01x_simulation" )
		ret = PoGeneralizer::TERNSIM;
	else if( str == "ms01x" )
		ret = PoGeneralizer::MS01X_FIX;
	else if( str == "ms01x_incremental" )
		ret = PoGeneralizer::MS01X_INCREMENTAL;
	else if( str == "ms01x_igbg_heuristics")
		ret = PoGeneralizer::MS_HEURISTICS;
	else if( str == "igbg_lifting_heuristics" )
		ret = PoGeneralizer::IGBG_LIFTING_HEURISTICS;
	else if( str == "ms01x_free" )
		ret = PoGeneralizer::MS01X_FREE;
	else if( str == "s01x_free" )
		ret = PoGeneralizer::S01X_FREE;
	else if( str == "greedy_qbf" )
		ret = PoGeneralizer::GREEDY_QBF_FIX;
	else if( str == "greedy_qbf_free" )
		ret = PoGeneralizer::GREEDY_QBF_FREE;
	else if( str == "sat_cover" )
		ret = PoGeneralizer::SAT_COVER_FIX;
	else if( str == "sat_cover_free" )
		ret = PoGeneralizer::SAT_COVER_FREE;
	else if( str == "gentr" )
		ret = PoGeneralizer::GENTR;
	else if( str == "ilp_cover" )
		ret = PoGeneralizer::ILP_COVER;
	else if( str == "ilp_cover_free" )
		ret = PoGeneralizer::ILP_COVER_FREE;
	else if( str == "maxqbf" )
		ret = PoGeneralizer::MAXQBF_FIX;
	else if( str == "maxqbf_free" )
		ret = PoGeneralizer::MAXQBF_FREE;
	else if( str == "greedy_cover" )
		ret = PoGeneralizer::GREEDY_COVER;
	else if( str == "rev_struct" )
		ret = PoGeneralizer::REV_STRUCT;
	else
		throw std::runtime_error("Specified inadmissible type for PO-generalization. Abort.");

	return ret;
}

} /* namespace IC3 */

#endif /* SRC_POGENERALIZER_H_ */
