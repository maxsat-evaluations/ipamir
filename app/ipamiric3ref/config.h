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




#ifndef CONFIG_H_INCLUDED
#define CONFIG_H_INCLUDED




/*******************************************************************************
 * Lifting Modes (activate exactly only one at a time) *************************
 *******************************************************************************/

/* Lifting Modes for Forward PDR (original) */
// #define IC3_POGEN_NONE
// #define IC3_POGEN_TERNARY_SIMULATION_01X
//#define IC3_POGEN_LIFTING_DEFAULT
// #define IC3_POGEN_LIFTING_WITH_SELFLOOPS 				/* can handle invariant constraints */
// #define IC3_POGEN_LIFTING_EXTENDED_CALL 				/* can handle invariant constraints */
// #define IC3_POGEN_LIFTING_WITH_ADDITIONAL_LITERAL_DROPPING
// #define IC3_POGEN_IGBG 							/* Implication Graph Based Generalization */
// #define IC3_POGEN_MAXSAT_01X_FIX					/* MaxSAT Solver: pacose */
// #define IC3_POGEN_MAXSAT_01X_FREE					/* MaxSAT Solver: pacose */
// #define IC3_POGEN_MAXSAT_01X_INCREMENTAL_FIX				/* MaxSAT Solver: pacose */
// #define IC3_POGEN_MAXSAT_01X_INCREMENTAL_FREE				/* MaxSAT Solver: pacose */
// #define IC3_POGEN_SAT_01X_FIX
// #define IC3_POGEN_SAT_01X_FREE
// #define IC3_POGEN_GENTR							/* former karstenlifting */
// #define IC3_POGEN_GREEDY_COVER
// #define IC3_POGEN_ILP_COVER_UNATE
// #define IC3_POGEN_SAT_COVER_FIX
// #define IC3_POGEN_SAT_COVER_FREE
// #define IC3_POGEN_HEURISTIC_IGBG_MAXSAT_01X_FIX

/* Lifting Modes for Reverse PDR */
// #define IC3_POGEN_REV_STRUCTURAL

/* Lifting Modes for both, Forward and Reverse PDR */
// #define IC3_POGEN_GREEDY_QBF_FIX
// #define IC3_POGEN_GREEDY_QBF_FREE
// #define IC3_POGEN_MAXQBF_QUANTOM_FIX
// #define IC3_POGEN_MAXQBF_QUANTOM_FREE
// #define IC3_POGEN_MAXQBF_AIGSOLVE_FIX
// #define IC3_POGEN_MAXQBF_AIGSOLVE_FREE
// #define IC3_POGEN_HEURISTIC_STRUCTURAL_MAXQBF

/*******************************************************************************/




/*******************************************************************************
 * Translation of Lifting Modes to interal structure (do not touch) ************
 *******************************************************************************/

#ifdef IC3_POGEN_NONE
#define nolifting
#endif /* IC3_POGEN_NONE */

#ifdef IC3_POGEN_TERNARY_SIMULATION_01X
#define ternsim
#endif /* IC3_POGEN_TERNARY_SIMULATION_01X */

#ifdef IC3_POGEN_LIFTING_DEFAULT
#define lifting
#endif /* IC3_POGEN_LIFTING_DEFAULT */

#ifdef IC3_POGEN_LIFTING_WITH_SELFLOOPS
#define lifting
#define lifting_selfloops
#endif /* IC3_POGEN_LIFTING_WITH_SELFLOOPS */

#ifdef IC3_POGEN_LIFTING_EXTENDED_CALL
#define lifting
#define lifting_extcall
#endif /* IC3_POGEN_LIFTING_EXTENDED_CALL */

#ifdef IC3_POGEN_LIFTING_WITH_ADDITIONAL_LITERAL_DROPPING
#define lifting
#define lifting_literaldropping
#endif /* IC3_POGEN_LIFTING_WITH_ADDITIONAL_LITERAL_DROPPING */

#ifdef IC3_POGEN_IGBG
#define implgraph
#endif /* IC3_POGEN_IGBG */

#ifdef IC3_POGEN_MAXSAT_01X_FIX
#define pacosepogen
#define pacosefixs
#endif /* IC3_POGEN_MAXSAT_01X_FIX */

#ifdef IC3_POGEN_MAXSAT_01X_FREE
#define pacosepogen
#endif /* IC3_POGEN_MAXSAT_01X_FREE */

#ifdef IC3_POGEN_MAXSAT_01X_INCREMENTAL_FIX
#define pacosepogen
#define pacosefixs
#define pacoseincremental
#endif /* IC3_POGEN_MAXSAT_01X_INCREMENTAL_FIX */

#ifdef IC3_POGEN_MAXSAT_01X_INCREMENTAL_FREE
#define pacosepogen
#define pacoseincremental
#endif /* IC3_POGEN_MAXSAT_01X_INCREMENTAL_FREE */

#ifdef IC3_POGEN_SAT_01X_FIX
#define ternsat
#define ternsatfixs
#define ternsat_heuristics_tweak
#endif /* IC3_POGEN_SAT_01X_FIX */

#ifdef IC3_POGEN_SAT_01X_FREE
#define ternsat
#define ternsatopens
#endif /* IC3_POGEN_SAT_01X_FREE */

#ifdef IC3_POGEN_GENTR
#define karstenlifting
#endif /* IC3_POGEN_GENTR */

#ifdef IC3_POGEN_GREEDY_COVER
#define hittingset
#endif /* IC3_POGEN_GREEDY_COVER */

#ifdef IC3_POGEN_ILP_COVER_UNATE
#define ilpgen
#define ilpgensat
#define COMPILE_WITH_GUROBI
#endif /* IC3_POGEN_ILP_COVER_UNATE */

#ifdef IC3_POGEN_SAT_COVER_FIX
#define satcover
#define satcoverfixs
#endif /* IC3_POGEN_SAT_COVER_FIX */

#ifdef IC3_POGEN_SAT_COVER_FREE
#define satcover
#define satcoveropens
#endif /* IC3_POGEN_SAT_COVER_FREE */

#ifdef IC3_POGEN_HEURISTIC_IGBG_MAXSAT_01X_FIX
#define pacosepogen
#define pacosefixs
#define pacoseheuristics
#endif /* IC3_POGEN_HEURISTIC_IGBG_MAXSAT_01X_FIX */




#ifdef IC3_POGEN_REV_STRUCTURAL
#define revpogen
#endif /* IC3_POGEN_REV_STRUCTURAL */




#ifdef IC3_POGEN_GREEDY_QBF_FIX
#define greedyqbfpogen
#define greedyqbffixs
#endif /* IC3_POGEN_GREEDY_QBF_FIX */

#ifdef IC3_POGEN_GREEDY_QBF_FREE
#define greedyqbfpogen
#endif /* IC3_POGEN_GREEDY_QBF_FREE */

#ifdef IC3_POGEN_MAXQBF_QUANTOM_FIX
#define maxqbfpogen
#define maxqbffixs
#endif /* IC3_POGEN_MAXQBF_QUANTOM_FIX */

#ifdef IC3_POGEN_MAXQBF_QUANTOM_FREE
#define maxqbfpogen
#endif /* IC3_POGEN_MAXQBF_QUANTOM_FREE */

#ifdef IC3_POGEN_MAXQBF_AIGSOLVE_FIX
#define maxqbfaigsolvepogen
#define maxqbfaigsolvefixs
#endif /* IC3_POGEN_MAXQBF_AIGSOLVE_FIX */

#ifdef IC3_POGEN_MAXQBF_AIGSOLVE_FREE
#define maxqbfaigsolvepogen
#endif /* IC3_POGEN_MAXQBF_AIGSOLVE_FREE */

#ifdef IC3_POGEN_HEURISTIC_STRUCTURAL_MAXQBF
#define revpogen
#define maxqbfpogen
#define maxqbffixs
#define maxqbfheuristics
#endif /* IC3_POGEN_HEURISTIC_STRUCTURAL_MAXQBF */

/*******************************************************************************/




/*******************************************************************************
 * Remaining Debug Options *****************************************************
 *******************************************************************************/

/* Forward PDR */
// #define pacosefixinputs
// #define pacosewritewcnf
// #define pacoseprintovalues
// #define ilpgenunsat

/* both */
// #define greedyqbfusesomewhatunsatcore

/* POGP instances */
// #define DUMP_POGEN_INSTANCE
// #define RUN_SINGLE_POGEN_INSTANCE
// #define COMPILE_WITH_GUROBI

/*******************************************************************************/




#endif /* CONFIG_H_INCLUDED */
