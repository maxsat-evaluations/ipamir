//  Copyright (c) 2022 Andreas Niskanen, University of Helsinki
//
//  Permission is hereby granted, free of charge, to any person obtaining a 
//  copy of this software and associated documentation files (the "Software"), 
//  to deal in the Software without restriction, including without limitation 
//  the rights to use, copy, modify, merge, publish, distribute, sublicense, 
//  and/or sell copies of the Software, and to permit persons to whom the 
//  Software is furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in 
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS 
//  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
//  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
//  DEALINGS IN THE SOFTWARE.

#ifndef DT_ENCODING_H
#define DT_ENCODING_H

#include <vector>
#include <unordered_map>

#include "Instance.h"

template <class T>
inline void hash_combine(std::size_t & seed, const T & v)
{
  std::hash<T> hasher;
  seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

namespace std
{
  template<typename S, typename T> struct hash<pair<S, T>>
  {
	inline size_t operator()(const pair<S, T> & v) const
	{
	  size_t seed = 0;
	  ::hash_combine(seed, v.first);
	  ::hash_combine(seed, v.second);
	  return seed;
	}
  };
}

struct MaxSATFormula {

    std::vector<std::vector<int>> hard_clauses;
    std::vector<std::pair<int, uint64_t>> soft_literals;

    void addHardClause(int x) { hard_clauses.push_back( {x} ); }
    void addHardClause(int x, int y) { hard_clauses.push_back( {x, y} ); }
    void addHardClause(int x, int y, int z) { hard_clauses.push_back( {x, y, z} ); }
    void addHardClause(int x, int y, int z, int w) { hard_clauses.push_back( {x, y, z, w} ); }
    void addHardClause(std::vector<int> & clause) { hard_clauses.push_back(clause); }
    void addSoftLiteral(int l, uint64_t w) { soft_literals.push_back(std::make_pair(l, w)); }

};

class DTEncoder {

public:
	DTEncoder(Data & data, uint32_t nodes = 0, uint32_t max_nodes = 0, uint32_t max_depth = 0, uint32_t exact_depth = 0,
		bool inference_constraints = false, bool reduced = true);
	uint32_t n_vars() { return count; }
	void generate_encoding();
	MaxSATFormula formula;
    int count; // count number of variables

	int tseitin_and(int first, int second);
	void exactly_one(std::vector<int> & lits);
	void conditional_exactly_one(int condition_var, std::vector<int> & lits);

	Data data; // binarized data
	uint32_t N; // number of nodes
	uint32_t M; // number of examples
	uint32_t K; // number of features
	uint32_t U; // max depth
	bool ub;    // N is ub?
	bool exact; // exact depth?
	bool lambda_and_tau;
	bool less_vars;
	std::vector<uint32_t> LR(uint32_t i);
	std::vector<uint32_t> RR(uint32_t i);

	// Narodytska et al. IJCAI'18
	std::unordered_map<uint32_t,int> is_leaf_node;                       // v_i: true iff ith node is a leaf node
	std::unordered_map<std::pair<uint32_t,uint32_t>,int> is_left_child;  // l_ij: true iff second is left child of first
	std::unordered_map<std::pair<uint32_t,uint32_t>,int> is_right_child; // r_ij: true iff second is right child of first
	std::unordered_map<std::pair<uint32_t,uint32_t>,int> is_parent;      // p_ji: true iff second is parent of first
	std::unordered_map<std::pair<uint32_t,uint32_t>,int> feature_assigned_to_node; // a_rj: true if feature (first) assigned to node (second)
	std::unordered_map<std::pair<uint32_t,uint32_t>,int> feature_discriminated_by_node; // u_rj: true if feature (first) discriminated by node (second)
	std::unordered_map<std::pair<uint32_t,uint32_t>,int> feature_discriminated_positively; // d_rj^0: true iff feature (first) discrimated positively
	                                                                                       // along path from root to node (second)
	std::unordered_map<std::pair<uint32_t,uint32_t>,int> feature_discriminated_negatively; // d_rj^1: true iff feature (first) discrimated negatively
	                                                                                       // along path from root to node (second)
	std::unordered_map<uint32_t,int> class_is_true;                                        // c_j: true iff class of leaf node is true
	std::unordered_map<std::pair<uint32_t,uint32_t>,int> lambda;
	std::unordered_map<std::pair<uint32_t,uint32_t>,int> tau;

	// Hu et al. IJCAI'20
	std::unordered_map<uint32_t,int> example_classified_correctly;
	std::unordered_map<std::pair<uint32_t,uint32_t>,int> depth; // true iff node (first) is on depth (second)
	std::unordered_map<uint32_t,int> nodes_used;

};

#endif
