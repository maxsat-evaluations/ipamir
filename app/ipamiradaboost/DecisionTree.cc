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

#include "DecisionTree.h"

using namespace std;

DecisionTree::DecisionTree(DTEncoder & encoder, vector<uint8_t> & model)
{
	if (!encoder.ub) {
		nodes = encoder.N;
	} else {
		for (uint32_t i = 1; i <= encoder.N; i += 2) {
			if (model[encoder.nodes_used[i]] == 1) {
				nodes = i;
			}
		}
	}
	classifications.resize(nodes+1, -1);
	features.resize(nodes+1, -1);
	left_children.resize(nodes+1, -1);
	right_children.resize(nodes+1, -1);
	for (uint32_t i = 1; i <= nodes; i++) {
		if (model[encoder.is_leaf_node[i]] == 1) {
			classifications[i] = model[encoder.class_is_true[i]];
			assert(classifications[i] == 0 || classifications[i] == 1);
			//classifications[i] = 1-classifications[i];
		} else {
			for (uint32_t j : encoder.LR(i)) {
				if (j > nodes) break;
				if (model[encoder.is_left_child[make_pair(i,j)]] == 1) {
					assert(left_children[i] == -1);
					left_children[i] = j;
				}
			}
			for (uint32_t j : encoder.RR(i)) {
				if (j > nodes) break;
				if (model[encoder.is_right_child[make_pair(i,j)]] == 1) {
					assert(right_children[i] == -1);
					right_children[i] = j;
				}
			}
			for (uint32_t r = 1; r <= encoder.K; r++) {
				if (model[encoder.feature_assigned_to_node[make_pair(r,i)]] == 1) {
					assert(features[i] == -1);
					features[i] = r;
				}
			}
		}
	}
}

bool DecisionTree::classify(Example & example)
{
	int node = 1;
	while (classifications[node] == -1) {
		bool feature = example.get_feature(features[node]-1);
		if (!feature) {
			node = left_children[node];
		} else {
			node = right_children[node];
		}
	}
	return static_cast<bool>(classifications[node]);
}
