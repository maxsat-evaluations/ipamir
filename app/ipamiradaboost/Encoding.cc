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

#include <fstream>
#include <cmath>

#include "Encoding.h"

using namespace std;

vector<uint32_t> DTEncoder::LR(uint32_t i)
{
	vector<uint32_t> lr;
	for (uint32_t j = i+1; j <= min(2*i, N-1); j++) {
		if (j%2 == 0) lr.push_back(j);
	}
	return lr;
}

vector<uint32_t> DTEncoder::RR(uint32_t i)
{
	vector<uint32_t> rr;
	for (uint32_t j = i+2; j <= min(2*i+1, N); j++) {
		if (j%2 == 1) rr.push_back(j);
	}
	return rr;
}

DTEncoder::DTEncoder(Data & data, uint32_t nodes, uint32_t max_nodes, uint32_t max_depth, uint32_t exact_depth, bool inference_constraints, bool reduced)
	: data(data), lambda_and_tau(inference_constraints), less_vars(reduced), count(0)
{
	if (max_depth > 0 || exact_depth > 0) {
		U = max(max_depth, exact_depth);
		N = (1 << (U+1)) - 1;
		ub = true;
	} else if (max_nodes > 0) {
		U = 0;
		N = max_nodes;
		ub = true;
	} else {
		U = 0;
		N = nodes;
		ub = false;
	}
	if (exact_depth > 0) {
		exact = true;
	} else {
		exact = false;
	}
	if (N%2 == 0) {
		N--;
	}
	M = data.n_examples();
	K = data.get_example(0).n_features();
	for (uint32_t i = 1; i <= N; i++) {
		is_leaf_node[i] = ++count;
	}
	if (less_vars) {
		for (uint32_t i = 1; i <= N; i++) {
			for (uint32_t j : LR(i)) {
				is_left_child[make_pair(i,j)] = ++count;
			}
		}
		for (uint32_t i = 1; i <= N; i++) {
			for (uint32_t j : RR(i)) {
				is_right_child[make_pair(i,j)] = is_left_child[make_pair(i,j-1)];
			}
		}
		for (uint32_t j = 2; j <= N; j++) {
			for (uint32_t i = j/2; i <= j-1; i++) {
				if (j%2 == 0 && j >= i+1 && j <= min(2*i, N-1)) {
					is_parent[make_pair(j,i)] = is_left_child[make_pair(i,j)];
				} else if (j%2 == 1 && j >= i+2 && j <= min(2*i+1, N)) {
					is_parent[make_pair(j,i)] = is_right_child[make_pair(i,j)];
				}
			}
		}
	} else {
		for (uint32_t i = 1; i <= N; i++) {
			for (uint32_t j : LR(i)) {
				is_left_child[make_pair(i,j)] = ++count;
			}
		}
		for (uint32_t i = 1; i <= N; i++) {
			for (uint32_t j : RR(i)) {
				is_right_child[make_pair(i,j)] = ++count;
			}
		}
		for (uint32_t j = 2; j <= N; j++) {
			for (uint32_t i = j/2; i <= j-1; i++) {
				is_parent[make_pair(j,i)] = ++count;
			}
		}
	}
	for (uint32_t r = 1; r <= K; r++) {
		for (uint32_t j = 1; j <= N; j++) {
			feature_assigned_to_node[make_pair(r,j)] = ++count;
		}
	}
	for (uint32_t r = 1; r <= K; r++) {
		for (uint32_t j = 1; j <= N; j++) {
			feature_discriminated_by_node[make_pair(r,j)] = ++count;
		}
	}
	for (uint32_t r = 1; r <= K; r++) {
		for (uint32_t j = 1; j <= N; j++) {
			feature_discriminated_positively[make_pair(r,j)] = ++count;
		}
	}
	for (uint32_t r = 1; r <= K; r++) {
		for (uint32_t j = 1; j <= N; j++) {
			feature_discriminated_negatively[make_pair(r,j)] = ++count;
		}
	}
	for (uint32_t j = 1; j <= N; j++) {
		class_is_true[j] = ++count;
	}
	if (lambda_and_tau) {
		for (uint32_t i = 1; i <= N; i++) {
			for (uint32_t t = 0; t <= i/2; t++) {
				lambda[make_pair(t,i)] = ++count;
			}
		}
		for (uint32_t i = 1; i <= N; i++) {
			for (uint32_t t = 0; t <= i; t++) {
				tau[make_pair(t,i)] = ++count;
			}
		}
	}
	for (uint32_t q = 1; q <= M; q++) {
		example_classified_correctly[q] = ++count;
	}
	if (U > 0) {
		for (uint32_t i = 1; i <= N; i++) {
			for (uint32_t t = ceil(log2(i+1))-1; t <= ceil(static_cast<double>(i-1)/2); t++) {
				//cout<<"i="<<i<<",t="<<t<<"\n";
				depth[make_pair(i,t)] = ++count;
			}
		}
	}
	if (ub) {
		for (uint32_t i = 1; i <= N; i += 2) {
			nodes_used[i] = ++count;
		}
	}
}

void DTEncoder::exactly_one(vector<int> & lits)
{
	if (lits.size() == 0) {
		formula.addHardClause({});
		return;
	}
	if (lits.size() == 1) {
		formula.addHardClause(lits[0]);
		return;
	}
	if (lits.size() == 2) {
		formula.addHardClause(lits[0], lits[1]);
		formula.addHardClause(-lits[0], -lits[1]);
		return;
	}
	vector<int> counter_var(lits.size());
	for (uint32_t i = 0; i < lits.size(); i++) {
		counter_var[i] = (++count);
	}
	formula.addHardClause(-lits[0], counter_var[0]);
	formula.addHardClause(lits[0], -counter_var[0]);
	for (uint32_t i = 1; i < lits.size(); i++) {
		formula.addHardClause(-counter_var[i-1], counter_var[i]);
		formula.addHardClause(-lits[i], counter_var[i]);
		formula.addHardClause(-lits[i], -counter_var[i-1]);
		formula.addHardClause(lits[i], -counter_var[i], counter_var[i-1]);
	}
	formula.addHardClause(counter_var[lits.size()-1]);
}

void DTEncoder::conditional_exactly_one(int condition_var, vector<int> & lits)
{
	if (lits.size() == 0) {
		formula.addHardClause(-condition_var);
		return;
	}
	if (lits.size() == 1) {
		formula.addHardClause(-condition_var, lits[0]);
		return;
	}
	if (lits.size() == 2) {
		formula.addHardClause(-condition_var, lits[0], lits[1]);
		formula.addHardClause(-condition_var, -lits[0], -lits[1]);
		return;
	}
	vector<int> counter_var(lits.size());
	for (uint32_t i = 0; i < lits.size(); i++) {
		counter_var[i] = (++count);
	}
	formula.addHardClause(-condition_var, -lits[0], counter_var[0]);
	formula.addHardClause(-condition_var, lits[0], -counter_var[0]);
	for (uint32_t i = 1; i < lits.size(); i++) {
		formula.addHardClause(-condition_var, -counter_var[i-1], counter_var[i]);
		formula.addHardClause(-condition_var, -lits[i], counter_var[i]);
		formula.addHardClause(-condition_var, -lits[i], -counter_var[i-1]);
		formula.addHardClause(-condition_var, lits[i], -counter_var[i], counter_var[i-1]);
	}
	formula.addHardClause(-condition_var, counter_var[lits.size()-1]);
	for (uint32_t i = 0; i < lits.size(); i++) {
		formula.addHardClause(condition_var, -counter_var[i]);
	}
}

int DTEncoder::tseitin_and(int first, int second)
{
	int tseitin_var = (++count);
	formula.addHardClause(-tseitin_var, first);
	formula.addHardClause(-tseitin_var, second);
	formula.addHardClause(tseitin_var, -first, -second);
	return tseitin_var;
}

void DTEncoder::generate_encoding()
{
	// Narodytska et al. 2018
	//if (less_vars) formula.addHardClause(-(0));
	formula.addHardClause(-(is_leaf_node[1])); // (1): root is not a leaf
	for (uint32_t i = 1; i <= N; i++) { // (2): leaf node has no children
		for (uint32_t j : LR(i)) {
			if (ub) {
				formula.addHardClause(-(nodes_used[j+1]), -(is_leaf_node[i]), -(is_left_child[make_pair(i,j)]));
			} else {
				formula.addHardClause(-(is_leaf_node[i]), -(is_left_child[make_pair(i,j)]));
			}
		}
	}
	if (!less_vars) {
		for (uint32_t i = 1; i <= N; i++) { // (3): left and right numbered consecutively
			for (uint32_t j : LR(i)) {
				if (ub) {
					formula.addHardClause(-(nodes_used[j+1]), -(is_left_child[make_pair(i,j)]), (is_right_child[make_pair(i,j+1)]));
					formula.addHardClause(-(nodes_used[j+1]), (is_left_child[make_pair(i,j)]), -(is_right_child[make_pair(i,j+1)]));
				} else {
					formula.addHardClause(-(is_left_child[make_pair(i,j)]), (is_right_child[make_pair(i,j+1)]));
					formula.addHardClause((is_left_child[make_pair(i,j)]), -(is_right_child[make_pair(i,j+1)]));
				}
			}
		}
	}
	for (uint32_t i = 1; i <= N; i++) { // (4): non-leaf node must have a child
		vector<int> lits;
		if (ub) {
			for (uint32_t j : LR(i)) {
				int l_and_m = tseitin_and((is_left_child[make_pair(i,j)]), (nodes_used[j+1]));
				lits.push_back(l_and_m);
			}
			int not_v_and_m = tseitin_and(-(is_leaf_node[i]), i%2 ? (nodes_used[i]) : (nodes_used[i+1]));
			conditional_exactly_one(not_v_and_m, lits);
		} else {
			for (uint32_t j : LR(i)) {
				lits.push_back((is_left_child[make_pair(i,j)]));
			}
			conditional_exactly_one(-(is_leaf_node[i]), lits);
		}
	}
	if (!less_vars) {
		for (uint32_t i = 1; i <= N; i++) { // (5): parent must have a child
			if (ub) {
				for (uint32_t j : LR(i)) {
					formula.addHardClause(-(nodes_used[j+1]), -(is_parent[make_pair(j,i)]), (is_left_child[make_pair(i,j)]));
					formula.addHardClause(-(nodes_used[j+1]), (is_parent[make_pair(j,i)]), -(is_left_child[make_pair(i,j)]));
				}
				for (uint32_t j : RR(i)) {
					formula.addHardClause(-(nodes_used[j]), -(is_parent[make_pair(j,i)]), (is_right_child[make_pair(i,j)]));
					formula.addHardClause(-(nodes_used[j]), (is_parent[make_pair(j,i)]), -(is_right_child[make_pair(i,j)]));
				}
			} else {
				for (uint32_t j : LR(i)) {
					formula.addHardClause(-(is_parent[make_pair(j,i)]), (is_left_child[make_pair(i,j)]));
					formula.addHardClause((is_parent[make_pair(j,i)]), -(is_left_child[make_pair(i,j)]));
				}
				for (uint32_t j : RR(i)) {
					formula.addHardClause(-(is_parent[make_pair(j,i)]), (is_right_child[make_pair(i,j)]));
					formula.addHardClause((is_parent[make_pair(j,i)]), -(is_right_child[make_pair(i,j)]));
				}
			}
		}
	}
	for (uint32_t j = 2; j <= N; j++) { // (6): all nodes but the first must have a parent
		vector<int> lits;
		for (uint32_t i = j/2; i <= (j%2 ? j-2 : j-1); i++) {
			lits.push_back((is_parent[make_pair(j,i)]));
		}
		if (ub) {
			conditional_exactly_one(j%2 ? (nodes_used[j]) : (nodes_used[j+1]), lits);
		} else {
			exactly_one(lits);
		}
	}
	for (uint32_t r = 1; r <= K; r++) { // (7): discriminate feature for value 0 at node
		formula.addHardClause(-(feature_discriminated_negatively[make_pair(r,1)]));
		for (uint32_t j = 2; j <= N; j++) {
			if (ub) {
				vector<int> clause = { j%2 ? -(nodes_used[j]) : -(nodes_used[j+1]), -(feature_discriminated_negatively[make_pair(r,j)]) };
				for (uint32_t i = j/2; i <= (j%2 ? j-2 : j-1); i++) {
					int p_and_d0 = tseitin_and((is_parent[make_pair(j,i)]), (feature_discriminated_negatively[make_pair(r,i)]));
					formula.addHardClause(j%2 ? -(nodes_used[j]) : -(nodes_used[j+1]), (feature_discriminated_negatively[make_pair(r,j)]), -p_and_d0);
					clause.push_back(p_and_d0);
					if (j%2 != 1 || j < i+2 || j > min(2*i+1,N)) continue;
					int a_and_r = tseitin_and((feature_assigned_to_node[make_pair(r,i)]), (is_right_child[make_pair(i,j)]));
					formula.addHardClause(j%2 ? -(nodes_used[j]) : -(nodes_used[j+1]), (feature_discriminated_negatively[make_pair(r,j)]), -a_and_r);
					clause.push_back(a_and_r);
				}
				formula.addHardClause(clause);
			} else {
				vector<int> clause = { -(feature_discriminated_negatively[make_pair(r,j)]) };
				for (uint32_t i = j/2; i <= (j%2 ? j-2 : j-1); i++) {
					int p_and_d0 = tseitin_and((is_parent[make_pair(j,i)]), (feature_discriminated_negatively[make_pair(r,i)]));
					formula.addHardClause((feature_discriminated_negatively[make_pair(r,j)]), -p_and_d0);
					clause.push_back(p_and_d0);
					if (j%2 != 1 || j < i+2 || j > min(2*i+1,N)) continue;
					int a_and_r = tseitin_and((feature_assigned_to_node[make_pair(r,i)]), (is_right_child[make_pair(i,j)]));
					formula.addHardClause((feature_discriminated_negatively[make_pair(r,j)]), -a_and_r);
					clause.push_back(a_and_r);
				}
				formula.addHardClause(clause);
			}
		}
	}
	for (uint32_t r = 1; r <= K; r++) { // (8): discriminate feature for value 1 at node
		formula.addHardClause(-(feature_discriminated_positively[make_pair(r,1)]));
		for (uint32_t j = 2; j <= N; j++) {
			if (ub) {
				vector<int> clause = { j%2 ? -(nodes_used[j]) : -(nodes_used[j+1]), -(feature_discriminated_positively[make_pair(r,j)]) };
				for (uint32_t i = j/2; i <= (j%2 ? j-2 : j-1); i++) {
					int p_and_d1 = tseitin_and((is_parent[make_pair(j,i)]), (feature_discriminated_positively[make_pair(r,i)]));
					formula.addHardClause(j%2 ? -(nodes_used[j]) : -(nodes_used[j+1]), (feature_discriminated_positively[make_pair(r,j)]), -p_and_d1);
					clause.push_back(p_and_d1);
					if (j%2 != 0 || j < i+1 || j > min(2*i,N-1)) continue;
					int a_and_l = tseitin_and((feature_assigned_to_node[make_pair(r,i)]), (is_left_child[make_pair(i,j)]));
					formula.addHardClause(j%2 ? -(nodes_used[j]) : -(nodes_used[j+1]), (feature_discriminated_positively[make_pair(r,j)]), -a_and_l);
					clause.push_back(a_and_l);
				}
				formula.addHardClause(clause);
			} else {
				vector<int> clause = { -(feature_discriminated_positively[make_pair(r,j)]) };
				for (uint32_t i = j/2; i <= (j%2 ? j-2 : j-1); i++) {
					int p_and_d1 = tseitin_and((is_parent[make_pair(j,i)]), (feature_discriminated_positively[make_pair(r,i)]));
					formula.addHardClause((feature_discriminated_positively[make_pair(r,j)]), -p_and_d1);
					clause.push_back(p_and_d1);
					if (j%2 != 0 || j < i+1 || j > min(2*i,N-1)) continue;
					int a_and_l = tseitin_and((feature_assigned_to_node[make_pair(r,i)]), (is_left_child[make_pair(i,j)]));
					formula.addHardClause((feature_discriminated_positively[make_pair(r,j)]), -a_and_l);
					clause.push_back(a_and_l);
				}
				formula.addHardClause(clause);
			}
		}
	}
	for (uint32_t r = 1; r <= K; r++) { // (9): using feature at node
		for (uint32_t j = 1; j <= N; j++) {
			if (ub) {
				vector<int> clause = { j%2 ? -(nodes_used[j]) : -(nodes_used[j+1]), -(feature_discriminated_by_node[make_pair(r,j)]) };
				formula.addHardClause(j%2 ? -(nodes_used[j]) : -(nodes_used[j+1]), (feature_discriminated_by_node[make_pair(r,j)]), -(feature_assigned_to_node[make_pair(r,j)]));
				clause.push_back((feature_assigned_to_node[make_pair(r,j)]));
				if (j != 1) {
					for (uint32_t i = j/2; i <= (j%2 ? j-2 : j-1); i++) {
						int u_and_p = tseitin_and((feature_discriminated_by_node[make_pair(r,i)]), (is_parent[make_pair(j,i)]));
						formula.addHardClause(j%2 ? -(nodes_used[j]) : -(nodes_used[j+1]), -u_and_p, -(feature_assigned_to_node[make_pair(r,j)]));
						formula.addHardClause(j%2 ? -(nodes_used[j]) : -(nodes_used[j+1]), (feature_discriminated_by_node[make_pair(r,j)]), -u_and_p);
						clause.push_back(u_and_p);
					}
				}
				formula.addHardClause(clause);
			} else {
				vector<int> clause = { -(feature_discriminated_by_node[make_pair(r,j)]) };
				formula.addHardClause((feature_discriminated_by_node[make_pair(r,j)]), -(feature_assigned_to_node[make_pair(r,j)]));
				clause.push_back((feature_assigned_to_node[make_pair(r,j)]));
				if (j != 1) {
					for (uint32_t i = j/2; i <= (j%2 ? j-2 : j-1); i++) {
						int u_and_p = tseitin_and((feature_discriminated_by_node[make_pair(r,i)]), (is_parent[make_pair(j,i)]));
						formula.addHardClause(-u_and_p, -(feature_assigned_to_node[make_pair(r,j)]));
						formula.addHardClause((feature_discriminated_by_node[make_pair(r,j)]), -u_and_p);
						clause.push_back(u_and_p);
					}
				}
				formula.addHardClause(clause);
			}
		}
	}
	for (uint32_t j = 1; j <= N; j++) { // (10): for non-leaf node exactly one feature is used
		vector<int> lits;
		for (uint32_t r = 1; r <= K; r++) {
			lits.push_back((feature_assigned_to_node[make_pair(r,j)]));
		}
		if (ub) {
			int not_v_and_m = tseitin_and(-(is_leaf_node[j]), j%2 ? (nodes_used[j]) : (nodes_used[j+1]));
			conditional_exactly_one(not_v_and_m, lits);
		} else {
			conditional_exactly_one(-(is_leaf_node[j]), lits);
		}
	}
	for (uint32_t j = 1; j <= N; j++) { // (11): for leaf node no feature is used
		for (uint32_t r = 1; r <= K; r++) {
			if (ub) {
				formula.addHardClause(j%2 ? -(nodes_used[j]) : -(nodes_used[j+1]), -(is_leaf_node[j]), -(feature_assigned_to_node[make_pair(r,j)]));
			} else {
				formula.addHardClause(-(is_leaf_node[j]), -(feature_assigned_to_node[make_pair(r,j)]));
			}
		}
	}
	for (uint32_t q = 1; q <= M; q++) {
		Example example = data.get_example(q-1);
		if (example.get_class()) { // (12): positive example discriminated if leaf node associated with negative class
			for (uint32_t j = 1; j <= N; j++) {
				if (ub) {
					vector<int> clause = { j%2 ? -(nodes_used[j]) : -(nodes_used[j+1]), -(example_classified_correctly[q]), -(is_leaf_node[j]), (class_is_true[j]) };
					for (uint32_t r = 1; r <= K; r++) {
						int lit = example.get_feature(r-1) ? (feature_discriminated_positively[make_pair(r,j)]) : (feature_discriminated_negatively[make_pair(r,j)]);
						clause.push_back(lit);
					}
					formula.addHardClause(clause);
				} else {
					vector<int> clause = { -(example_classified_correctly[q]), -(is_leaf_node[j]), (class_is_true[j]) };
					for (uint32_t r = 1; r <= K; r++) {
						int lit = example.get_feature(r-1) ? (feature_discriminated_positively[make_pair(r,j)]) : (feature_discriminated_negatively[make_pair(r,j)]);
						clause.push_back(lit);
					}
					formula.addHardClause(clause);
				}
			}
		} else { // (13): negative example discriminated if leaf node associated with positive class
			for (uint32_t j = 1; j <= N; j++) {
				if (ub) {
					vector<int> clause = { j%2 ? -(nodes_used[j]) : -(nodes_used[j+1]), -(example_classified_correctly[q]), -(is_leaf_node[j]), -(class_is_true[j]) };
					for (uint32_t r = 1; r <= K; r++) {
						int lit = example.get_feature(r-1) ? (feature_discriminated_positively[make_pair(r,j)]) : (feature_discriminated_negatively[make_pair(r,j)]);
						clause.push_back(lit);
					}
					formula.addHardClause(clause);
				} else {
					vector<int> clause = { -(example_classified_correctly[q]), -(is_leaf_node[j]), -(class_is_true[j]) };
					for (uint32_t r = 1; r <= K; r++) {
						int lit = example.get_feature(r-1) ? (feature_discriminated_positively[make_pair(r,j)]) : (feature_discriminated_negatively[make_pair(r,j)]);
						clause.push_back(lit);
					}
					formula.addHardClause(clause);
				}
			}
		}
	}
	if (lambda_and_tau) {
		for (uint32_t i = 1; i <= N; i++) {
			if (ub) {
				formula.addHardClause(i%2 ? -(nodes_used[i]) : -(nodes_used[i+1]), (lambda[make_pair(0,i)]));
				for (uint32_t t = 1; t <= i/2; t++) {
					int lambda_and_v = tseitin_and((lambda[make_pair(t-1,i-1)]), (is_leaf_node[i]));
					if (t <= (i-1)/2) {
						formula.addHardClause(i%2 ? -(nodes_used[i]) : -(nodes_used[i+1]), -(lambda[make_pair(t,i)]), (lambda[make_pair(t,i-1)]), lambda_and_v);
						formula.addHardClause(i%2 ? -(nodes_used[i]) : -(nodes_used[i+1]), (lambda[make_pair(t,i)]), -(lambda[make_pair(t,i-1)]));
						formula.addHardClause(i%2 ? -(nodes_used[i]) : -(nodes_used[i+1]), (lambda[make_pair(t,i)]), -lambda_and_v);
					} else {
						formula.addHardClause(-(lambda[make_pair(t,i)]), lambda_and_v);
						formula.addHardClause((lambda[make_pair(t,i)]), -lambda_and_v);
					}
				}
			} else {
				formula.addHardClause((lambda[make_pair(0,i)]));
				for (uint32_t t = 1; t <= i/2; t++) {
					int lambda_and_v = tseitin_and((lambda[make_pair(t-1,i-1)]), (is_leaf_node[i]));
					if (t <= (i-1)/2) {
						formula.addHardClause(-(lambda[make_pair(t,i)]), (lambda[make_pair(t,i-1)]), lambda_and_v);
						formula.addHardClause((lambda[make_pair(t,i)]), -(lambda[make_pair(t,i-1)]));
						formula.addHardClause((lambda[make_pair(t,i)]), -lambda_and_v);
					} else {
						formula.addHardClause(-(lambda[make_pair(t,i)]), lambda_and_v);
						formula.addHardClause((lambda[make_pair(t,i)]), -lambda_and_v);
					}
				}
			}
		}
		for (uint32_t i = 1; i <= N; i++) {
			for (uint32_t t = 1; t <= i/2; t++) {
				uint32_t tval = 2*(i-t+1);
				if (tval >= i+1 && tval <= min(2*i,N-1)) {
					if (ub) {
						formula.addHardClause(i%2 ? -(nodes_used[i]) : -(nodes_used[i+1]), -(lambda[make_pair(t,i)]), -(is_left_child[make_pair(i,tval)]));
					} else {
						formula.addHardClause(-(lambda[make_pair(t,i)]), -(is_left_child[make_pair(i,tval)]));
					}
				}
				tval = 2*(i-t+1)+1;
				if (tval >= i+2 && tval <= min(2*i+1,N)) {
					if (ub) {
						formula.addHardClause(i%2 ? -(nodes_used[i]) : -(nodes_used[i+1]), -(lambda[make_pair(t,i)]), -(is_right_child[make_pair(i,tval)]));
					} else {
						formula.addHardClause(-(lambda[make_pair(t,i)]), -(is_right_child[make_pair(i,tval)]));
					}
				}
			}
		}
		for (uint32_t i = 1; i <= N; i++) {
			if (ub) {
				formula.addHardClause(i%2 ? -(nodes_used[i]) : -(nodes_used[i+1]), (tau[make_pair(0,i)]));
				for (uint32_t t = 1; t <= i; t++) {
					if (i == 1) {
						formula.addHardClause(i%2 ? -(nodes_used[i]) : -(nodes_used[i+1]), -(tau[make_pair(t,i)]), -(is_leaf_node[i]));
						formula.addHardClause(i%2 ? -(nodes_used[i]) : -(nodes_used[i+1]), (tau[make_pair(t,i)]), (is_leaf_node[i]));
					} else {
						int tau_and_not_v = tseitin_and((tau[make_pair(t-1,i-1)]), -(is_leaf_node[i]));
						if (t <= i-1) {
							formula.addHardClause(i%2 ? -(nodes_used[i]) : -(nodes_used[i+1]), -(tau[make_pair(t,i)]), (tau[make_pair(t,i-1)]), tau_and_not_v);
							formula.addHardClause(i%2 ? -(nodes_used[i]) : -(nodes_used[i+1]), (tau[make_pair(t,i)]), -(tau[make_pair(t,i-1)]));
							formula.addHardClause(i%2 ? -(nodes_used[i]) : -(nodes_used[i+1]), (tau[make_pair(t,i)]), -tau_and_not_v);
						} else {
							formula.addHardClause(i%2 ? -(nodes_used[i]) : -(nodes_used[i+1]), -(tau[make_pair(t,i)]), tau_and_not_v);
							formula.addHardClause(i%2 ? -(nodes_used[i]) : -(nodes_used[i+1]), (tau[make_pair(t,i)]), -tau_and_not_v);
						}
					}
				}
			} else {
				formula.addHardClause((tau[make_pair(0,i)]));
				for (uint32_t t = 1; t <= i; t++) {
					if (i == 1) {
						formula.addHardClause(-(tau[make_pair(t,i)]), -(is_leaf_node[i]));
						formula.addHardClause((tau[make_pair(t,i)]), (is_leaf_node[i]));
					} else {
						int tau_and_not_v = tseitin_and((tau[make_pair(t-1,i-1)]), -(is_leaf_node[i]));
						if (t <= i-1) {
							formula.addHardClause(-(tau[make_pair(t,i)]), (tau[make_pair(t,i-1)]), tau_and_not_v);
							formula.addHardClause((tau[make_pair(t,i)]), -(tau[make_pair(t,i-1)]));
							formula.addHardClause((tau[make_pair(t,i)]), -tau_and_not_v);
						} else {
							formula.addHardClause(-(tau[make_pair(t,i)]), tau_and_not_v);
							formula.addHardClause((tau[make_pair(t,i)]), -tau_and_not_v);
						}
					}
				}
			}
		}
		for (uint32_t i = 1; i <= N; i++) {
			for (uint32_t t = (i+1)/2+1; t <= i; t++) {
				uint32_t tval = 2*(i-1);
				if (tval >= i+1 && tval <= min(2*i,N-1)) {
					if (ub) {
						formula.addHardClause(i%2 ? -(nodes_used[i]) : -(nodes_used[i+1]), -(tau[make_pair(t,i)]), -(is_left_child[make_pair(i,tval)]));
					} else {
						formula.addHardClause(-(tau[make_pair(t,i)]), -(is_left_child[make_pair(i,tval)]));
					}
				}
				tval = 2*t-1;
				if (tval >= i+2 && tval <= min(2*i+1,N)) {
					if (ub) {
						formula.addHardClause(i%2 ? -(nodes_used[i]) : -(nodes_used[i+1]), -(tau[make_pair(t,i)]), -(is_right_child[make_pair(i,tval)]));
					} else {
						formula.addHardClause(-(tau[make_pair(t,i)]), -(is_right_child[make_pair(i,tval)]));
					}
				}
			}
		}
	}
	if (U > 0) {
		// Hu et al. 2020
		formula.addHardClause((depth[make_pair(1,0)]));
		for (uint32_t i = 1; i <= N; i++) { // (7)
			vector<int> lits;
			for (uint32_t t = ceil(log2(i+1))-1; t <= ceil(static_cast<double>(i-1)/2); t++) {
				lits.push_back((depth[make_pair(i,t)]));
			}
			conditional_exactly_one(i%2 ? (nodes_used[i]) : (nodes_used[i+1]), lits);
		}
		for (uint32_t i = 1; i <= N; i++) { // (8)
			for (uint32_t t = ceil(log2(i+1))-1; t <= ceil(static_cast<double>(i-1)/2); t++) {
				for (uint32_t j : LR(i)) {
					formula.addHardClause(-(nodes_used[j+1]), -(depth[make_pair(i,t)]), -(is_left_child[make_pair(i,j)]), (depth[make_pair(j,t+1)]));
				}
				for (uint32_t j : RR(i)) {
					formula.addHardClause(-(nodes_used[j]), -(depth[make_pair(i,t)]), -(is_right_child[make_pair(i,j)]), (depth[make_pair(j,t+1)]));
				}
			}
		}
		for (uint32_t i = 2*U; i <= N; i++) { // (9)
			formula.addHardClause(i%2 ? -(nodes_used[i]) : -(nodes_used[i+1]), -(depth[make_pair(i,U)]), (is_leaf_node[i]));
		}
		if (exact) {
			vector<int> clause;
			for (uint32_t i = 2*U; i <= N; i++) {
				clause.push_back((depth[make_pair(i,U)]));
			}
			formula.addHardClause(clause);
		}
	}
	if (ub) {
		formula.addHardClause((nodes_used[3]));
		for (uint32_t i = 1; i <= N-2; i += 2) {
			formula.addHardClause(-(nodes_used[i+2]), (nodes_used[i]));
		}
	}
	for (uint32_t q = 1; q <= M; q++) {
		formula.addSoftLiteral(-(example_classified_correctly[q]), 1);
	}

}
