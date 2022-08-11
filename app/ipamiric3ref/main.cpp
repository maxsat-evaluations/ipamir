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
#include <string>
#include <time.h>

extern "C"
{
#include "aiger.h"
}
#include "IC3.h"
#include "Model.h"


void printHelp()
{
	cout << "This is ic3ref with additional support for *lots* of proof obligation generalization techniques." << endl;
	cout << "Usage: ./IC3 [options] < problem.aig" << endl;
	cout << "[options]:" << endl;
	cout << "-x: Reverse PDR" << endl;
	cout << "-v: verbose" << endl;
	cout << "-s: print statistics" << endl;
	cout << "-r: randomize SAT solver decision heuristics" << endl;
	cout << "-b: use basic UNSAT generalization (irrelevant for proof obligations)" << endl;
	cout << "--use-literal-rotation: Use literal rotation in MiniSAT (for clause generalization- not proof obligations!)" << endl;
	cout << "-p [path]: run on single POGP instance. Remark: Also specify according AIGER instance in stdin." << endl;
	cout << "--pogen-type [type]: specify type of PO generalization" << endl;
			cout << "\t" << "[type]: (NOTE: not all types are admissible for all modes of operation)" << endl;
			cout << "\t" << "none" << endl;
			cout << "\t" << "lifting" << endl;
			cout << "\t" << "lifting_literal_dropping" << endl;
			cout << "\t" << "igbg" << endl;
			cout << "\t" << "igbg_literal_dropping" << endl;
			cout << "\t" << "justification" << endl;
			cout << "\t" << "s01x" << endl;
			cout << "\t" << "s01x_free (only in POGP mode)" << endl;
			cout << "\t" << "01x_simulation" << endl;
			cout << "\t" << "ms01x" << endl;
			cout << "\t" << "ms01x_free (only in POGP mode)" << endl;
			cout << "\t" << "ms01x_incremental (IPAMIR API)" << endl;
			cout << "\t" << "ms01x_igbg_heuristics" << endl;
			cout << "\t" << "igbg_lifting_heuristics" << endl;
			cout << "\t" << "greedy_qbf" << endl;
			cout << "\t" << "greedy_qbf_free (only in POGP mode)" << endl;
			cout << "\t" << "sat_cover" << endl;
			cout << "\t" << "sat_cover_free (only in POGP mode)" << endl;
			cout << "\t" << "greedy_cover" << endl;
			cout << "\t" << "gentr" << endl;
			cout << "\t" << "ilp_cover" << endl;
			cout << "\t" << "ilp_cover_free (only in POGP mode)" << endl;
			cout << "\t" << "maxqbf" << endl;
			cout << "\t" << "maxqbf_free (only in POGP mode)" << endl;
			cout << "\t" << "rev_struct" << endl;

	cout << "Additional options for different proof obligation generalization modes:" << endl;
		cout << "\t" << "*general (if it makes sense): " << endl;
			cout << "\t\t" << "--pogen-sort-by-block-activity: \t Sort assumptions by Bradley's activity heuristics." << endl;
			cout << "\t\t" << "--pogen-sort-reverse: \t Reverse the order of the assumptions." << endl;
		cout << "\t" << "*lifting (incl. lifting w/ literal dropping): " << endl;
			cout << "\t\t" << "** regarding invariant constraints (only for standard lifting) **" << endl;
			cout << "\t\t" << "--lifting-self-loops: \t Introduce self loops to restore left totality of T." << endl;
			cout << "\t\t" << "--lifting-extended-call: \t Use the extended lifting call to cope with invariant constraints." << endl;
			cout << "\t\t" << "*****************************************************************" << endl;
			cout << "\t\t" << "--lifting-literal-rotation: \t Apply literal rotation for better generalization." << endl;
			cout << "\t\t" << "--lifting-shuffle: \t Literal dropping: shuffle before each lifting call (except for the first)." << endl;
			cout << "\t\t" << "--lifting-literal-dropping-max-fails [val]: \t Literal dropping: abort after [val] failed tries." << endl;
			cout << "\t\t" << "--lifting-literal-dropping-max-tries [val]: \t Literal dropping: abort after [val] tries." << endl;
			cout << "\t\t" << "--lifting-literal-rotation-max-fails [val]: \t Literal rotation: abort after [val] failed tries." << endl;
			cout << "\t\t" << "--lifting-literal-rotation-max-tries [val]: \t Literal rotation: abort after [val] tries." << endl;
			cout << "\t\t" << "--lifting-approx-sat: \t Literal dropping: use approximate SAT, consider call as SAT after some decisions." << endl;
		cout << "\t" << "*igbg: " << endl;
			cout << "\t\t" << "--igbg-literal-rotation: \t Apply literal rotation for better generalization." << endl;
			cout << "\t\t" << "--igbg-respect-property: \t respect the property during generalization." << endl;
			cout << "\t\t" << "--igbg-sort-by-solver-activity: \t Use the activity heuristic of MiniSat for sorting." << endl;
			cout << "\t\t" << "--igbg-shuffle: \t Literal dropping: shuffle before each lifting call (except for the first)." << endl;
			cout << "\t\t" << "--igbg-max-fails [val]: \t Literal dropping: abort after [val] failed tries." << endl;
			cout << "\t\t" << "--igbg-max-tries [val]: \t Literal dropping: abort after [val] tries." << endl;

	exit(0);
}

int main(int argc, char **argv)
{
	unsigned int propertyIndex = 0;
	bool basic = false, random = false, revpdr = false, litrot = false;
	string poGenPath = "";
	string poGenType = "none";
	int verbose = 0;
	IC3::GeneralOpt genOpt;
	const char* aigPath = "";
	for (int i = 1; i < argc; ++i)
	{
		if (string(argv[i]) == "-v")
			// option: verbosity
			verbose = 2;
		else if (string(argv[i]) == "-h" || string(argv[i]) == "--help")
			printHelp();
		else if (string(argv[i]) == "-s")
			// option: print statistics
			verbose = max(1, verbose);
		else if (string(argv[i]) == "-r")
		{
			// option: randomize the run, which is useful in performance
			// testing; default behavior is deterministic
			srand(time(NULL));
			random = true;
		}
		else if (string(argv[i]) == "-b")
			// option: use basic generalization
			basic = true;
		else if (string(argv[i]) == "-x")
			// option: reversePDR
			revpdr = true;
		else if (string(argv[i]).substr(0,2) == "-p")
		{
			// option: path for Po-Generalization problem instance
			if(argc < i + 2)
			{
				cout << "-p recognized: specify path!" << endl;
				exit(0);
			}
			poGenPath = string(argv[i+1]);
			i++; //skip
		}
		else if (string(argv[i]) == "--use-literal-rotation")
		{
			// option: literal rotation
			litrot = true;
		}
		else if (string(argv[i]).substr(0,12) == "--pogen-type")
		{
			if(argc < i + 2)
			{
				cout << "--pogen-type recognized: specify type of PO generalization!" << endl;
				exit(0);
			}
			poGenType = string(argv[i+1]);
			i++; //skip
		}
		else if (string(argv[i]) == "--pogen-sort-by-block-activity")
		{
			genOpt.sortAssumpsByIc3refActivity = true;
		}
		else if (string(argv[i]) == "--pogen-sort-reverse")
		{
			genOpt.reverse = true;
		}
		else if (string(argv[i]) == "--igbg-sort-by-solver-activity")
		{
			if(genOpt.sortAssumpsByIc3refActivity) throw std::runtime_error("Please specify only one sorting criterion!");
			genOpt.igbg.sortAssumpsBySolverActivity = true;
		}
		else if (string(argv[i]) == "--lifting-literal-rotation")
		{
			genOpt.lift.useLiteralRotation = true;
		}
		else if (string(argv[i]) == "--lifting-shuffle")
		{
			genOpt.lift.shuffle = true;
		}
		else if (string(argv[i]) == "--lifting-literal-dropping-max-tries")
		{
			assert(atoi(argv[i + 1]) != 0);
			genOpt.lift.litDropMaxTries = atoi(argv[i + 1]);
			i++; //skip
		}
		else if (string(argv[i]) == "--lifting-literal-dropping-max-fails")
		{
			assert(atoi(argv[i + 1]) != 0);
			genOpt.lift.litDropMaxFails = atoi(argv[i + 1]);
			i++; //skip
		}
		else if (string(argv[i]) == "--lifting-literal-rotation-max-tries")
		{
			assert(atoi(argv[i + 1]) != 0);
			genOpt.lift.litRotMaxTries = atoi(argv[i + 1]);
			i++; //skip
		}
		else if (string(argv[i]) == "--lifting-literal-rotation-max-fails")
		{
			assert(atoi(argv[i + 1]) != 0);
			genOpt.lift.litRotMaxFails = atoi(argv[i + 1]);
			i++; //skip
		}
		else if (string(argv[i]) == "--lifting-approx-sat")
		{
			genOpt.lift.useApproxSat = true;
		}
		else if (string(argv[i]) == "--lifting-self-loops")
		{
			genOpt.lift.selfLoops = true;
		}
		else if (string(argv[i]) == "--lifting-extended-call")
		{
			genOpt.lift.extCall = true;
		}
		else if (string(argv[i]) == "--igbg-max-fails")
		{
			assert(atoi(argv[i + 1]) != 0);
			genOpt.igbg.maxFails = atoi(argv[i + 1]);
			i++; //skip
		}
		else if (string(argv[i]) == "--igbg-shuffle")
		{
			genOpt.igbg.shuffle = true;
		}
		else if (string(argv[i]) == "--igbg-max-tries")
		{
			assert(atoi(argv[i + 1]) != 0);
			genOpt.igbg.maxTries = atoi(argv[i + 1]);
			i++; //skip
		}
		else if (string(argv[i]) == "--igbg-respect-property")
		{
			genOpt.igbg.respectProperty = true;
		}
		else if (string(argv[i]) == "--igbg-literal-rotation")
		{
			genOpt.igbg.useLiteralRotation = true;
		}
		//fail!
		else if (!atoi(argv[i]) && string(argv[i]) != "0")
		{
			//distinguish genType
			//HACK no option but only path as argument
			poGenType = "ms01x_incremental";
			aigPath = argv[i];
			//cout << "ERROR: unrecognized option: " << string(argv[i]) << endl;
			//exit(0);

		}
		else
			// optional argument: set property index
			propertyIndex = (unsigned) atoi(argv[i]);
	}

	// read AIGER model
	aiger *aig = aiger_init();
	const char *msg;
	if(strlen(aigPath) > 0)
	{
		FILE *fptr;

	   // use appropriate location if you are using MacOS or Linux
		fptr = fopen(aigPath,"r");

		if(fptr == NULL)
		{
		  printf("Error: no aig problem file found!");   
		  exit(1);             
		}

		msg = aiger_read_from_file(aig, fptr);
		fclose(fptr);
	}
	else
	{
		msg = aiger_read_from_file(aig, stdin);
	}
	if (msg)
	{
		cout << msg << endl;
		return 0;
	}

	// create the Model from the obtained aig
	Model *model = modelFromAiger(aig, propertyIndex);
	//TEST: introduce self loops for lifting
	Model *modelWithSelfLoops = NULL;
#ifdef lifting
	if(aig->num_constraints > 0)
	{
		introduceSelfLoops(aig);
		// create the Model from the obtained aig
		modelWithSelfLoops = modelFromAiger(aig, propertyIndex);
	}
#endif
	aiger_reset(aig);
	if (!model)
		return 0;

	// model check it
	bool rv = IC3::check(*model, *modelWithSelfLoops, poGenPath, poGenType, genOpt, verbose, basic, random, revpdr, litrot);
	// print 0/1 according to AIGER standard
	cout << !rv << endl;

	delete model;
	if(modelWithSelfLoops)
		delete modelWithSelfLoops;

	return 1;
}
