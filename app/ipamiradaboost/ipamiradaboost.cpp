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

#include <cassert>
#include <fstream>
#include <iostream>
#include <vector>
#include <cmath>
#include <random>

using namespace std;

extern "C" {
#include "ipamir.h"
}

const int max_depth = 2;
const int adaboost = 20;

#include "DecisionTree.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        cout << "USAGE: ./ipamiradaboost <input_file_name>\n\n";
        cout << "where <input_file_name> is a dataset as a CSV file.\n\n";
        cout << "See ./inputs for example input files.\n";
        return 1;
    }

    Data full_data(argv[1]);
    Data training_data = full_data;

    /*std::default_random_engine generator;
    generator.seed(static_cast<int>(random_seed));
    std::uniform_real_distribution<double> distribution (0.0, 1.0);
    Data training_data, test_data;
    for (auto example : full_data) {
      if (distribution(generator) < ratio) {
        training_data.add_example(example);
      } else {
        test_data.add_example(example);
      }
    }*/

    DTEncoder encoder(training_data, 0, 0, max_depth, 0, true, false);
    encoder.generate_encoding();

    void * maxsat_solver = ipamir_init();
    for (size_t i = 0; i < encoder.formula.hard_clauses.size(); i++) {
        for (size_t j = 0; j < encoder.formula.hard_clauses[i].size(); j++) {
            ipamir_add_hard(maxsat_solver, encoder.formula.hard_clauses[i][j]);
        }
        ipamir_add_hard(maxsat_solver, 0);
    }
    for (size_t i = 0; i < encoder.formula.soft_literals.size(); i++) {
        ipamir_add_soft_lit(maxsat_solver, encoder.formula.soft_literals[i].first, 1);
    }

    vector<int> weights(encoder.formula.soft_literals.size(), 1);
    vector<double> alphas;
    vector<DecisionTree> trees;

    for (int i = 0; i <= adaboost; i++) {
        cout << "\n";
        cout << "c AdaBoost iteration " << i << "\n";

        int code = ipamir_solve(maxsat_solver);
        if (code != 30) {
            cout << "ERROR: ipamir_solve returned " << code << ". Terminating.\n";
            return code;
        } else {
            cout << "o " << ipamir_val_obj(maxsat_solver) << "\n";
        }

        vector<uint8_t> model(encoder.count+1, 0);
        for (int v = 1; v <= encoder.count; v++) {
            if (ipamir_val_lit(maxsat_solver, v) > 0) {
                model[v] = 1;
            }
        }

        DecisionTree tree(encoder, model);
        int training_classified_correctly = 0;
        vector<uint8_t> classified_correctly(weights.size(), 0);
        uint32_t q = 0;
        for (auto example : training_data) {
            if (tree.classify(example) == example.get_class()) {
                training_classified_correctly++;
                classified_correctly[q] = 1;
            }
            q++;
        }
        double training_accuracy = (double)training_classified_correctly/training_data.n_examples();
        cout << "c Training accuracy: " << training_classified_correctly << "/" << training_data.n_examples() << " = " << training_accuracy << "\n";

        double epsilon = 1.0 - training_accuracy;
        double alpha   = 0.5 * log((1.0-epsilon)/epsilon);
        if (epsilon > 0.5) {
            cout << "c Warning: worse than chance.\n";
        } else if (training_accuracy > 0.999) {
            cout << "c Warning: reached 100% accuracy.\n";
            alpha = 1.0;
            //i = adaboost;
        }
        alphas.push_back(alpha);
        trees.push_back(tree);

        if (i != adaboost) {
            vector<double> weights_hat(weights.size());
            double sum = 0.0;
            for (q = 0; q < weights.size(); q++) {
                if (classified_correctly[q]) {
                    weights_hat[q] = exp(-alpha) * weights[q];
                } else {
                    weights_hat[q] =  exp(alpha) * weights[q];
                }
                sum += weights_hat[q];
            }
            double smallest = weights_hat[0];
            for (q = 0; q < weights.size(); q++) {
                weights_hat[q] /= sum;
                if (weights_hat[q] < smallest) {
                    smallest = weights_hat[q];
                }
            }
            for (q = 0; q < weights.size(); q++) {
                weights[q] = (int)round(weights_hat[q] / smallest);
            }
            for (q = 0; q < weights.size(); q++) {
                ipamir_add_soft_lit(maxsat_solver, encoder.formula.soft_literals[q].first, weights[q]);
            }

        } else {
            training_classified_correctly = 0;
            for (auto example : training_data) {
                double aggregated_sum = 0.0;
                for (uint32_t j = 0; j < alphas.size(); j++) {
                    aggregated_sum += alphas[j] * (trees[j].classify(example) ? 1 : -1);
                }
                if (aggregated_sum > 0 && example.get_class()) {
                    training_classified_correctly++;
                } else if (aggregated_sum <= 0 && !example.get_class()) {
                    training_classified_correctly++;
                }
            }
            cout << "c Final training accuracy = " << training_classified_correctly << "/" << training_data.n_examples() << " = " << (double)training_classified_correctly/training_data.n_examples() << "\n";
        }
    }

    ipamir_release(maxsat_solver);
}
