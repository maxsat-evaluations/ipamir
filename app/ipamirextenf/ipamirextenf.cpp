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
#include <algorithm>

using namespace std;

extern "C" {
#include "ipamir.h"
#include "ipasir.h"
}

#include "Instance.h"
#include "Encoding.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        cout << "USAGE: ./ipamirextenf <input_file_name>\n\n";
        cout << "where <input_file_name> is an AF in APX format.\n\n";
        cout << "See ./inputs for example input files.\n";
        return 1;
    }

    ifstream input;
    input.open(argv[1]);

    if (!input.good()) {
        cout << "ERROR: Input file is not good.\n";
        return 1;
    }

    AF af;
    string line, arg, source, target;

    while (!input.eof()) {
        getline(input, line);
        line.erase(remove_if(line.begin(), line.end(), ::isspace), line.end());
        if (line.length() == 0 || line[0] == '/' || line[0] == '%') continue;
        if (line.length() < 6) cout << "WARNING: Cannot parse line: " << line << "\n";
        string op = line.substr(0,3);
        if (op == "arg") {
            if (line[3] == '(' && line.find(')') != string::npos) {
                arg = line.substr(4,line.find(')')-4);
                af.addArgument(arg);
            } else {
                cout << "WARNING: Cannot parse line: " << line << "\n";
            }
        } else if (op == "att") {
            if (line[3] == '(' && line.find(',') != string::npos && line.find(')') != string::npos) {
                source = line.substr(4,line.find(',')-4);
                target = line.substr(line.find(',')+1,line.find(')')-line.find(',')-1);
                af.addAttack(make_pair(source, target));
            } else {
                cout << "WARNING: Cannot parse line: " << line << "\n";
            }
        } else if (op == "enf" && line.find(')') != string::npos) {
            if (line[3] == '(') {
                arg = line.substr(4,line.find(')')-4);
                af.addEnforcement(arg);
            } else {
                cout << "WARNING: Cannot parse line: " << line << "\n";
            }
        }
    }

    cout << "c Number of arguments: " << af.args.size() << "\n";
    cout << "c Number of attacks:   " << af.atts.size() << "\n";
    cout << "c Number of targets:   " << af.enfs.size() << "\n";
    cout << "c Number of conflicts: " << af.numberOfConflicts() << "\n";

    DynamicAFEncoder encoder(af);
    encoder.generate_encoding();

    void * maxsat_solver = ipamir_init();
    for (size_t i = 0; i < encoder.formula.hard_clauses.size(); i++) {
        //cout << "ipamir_add_hard\t";
        for (size_t j = 0; j < encoder.formula.hard_clauses[i].size(); j++) {
            //cout << encoder.formula.hard_clauses[i][j] << " ";
            ipamir_add_hard(maxsat_solver, encoder.formula.hard_clauses[i][j]);
        }
        //cout << "0\n";
        ipamir_add_hard(maxsat_solver, 0);
    }
    for (size_t i = 0; i < encoder.formula.soft_literals.size(); i++) {
        //cout << "ipamir_add_soft_lit\t" << encoder.formula.soft_literals[i].first << " 0\n";
        ipamir_add_soft_lit(maxsat_solver, encoder.formula.soft_literals[i].first, 1);
    }

    while (true) {
        //cout << "ipamir_solve\n";
        int code = ipamir_solve(maxsat_solver);
        if (code != 30) {
            cout << "ERROR: ipamir_solve returned " << code << ". Terminating.\n";
            return code;
        }

        AF candidate;
        for (int i = 0; i < af.args.size(); i++) {
            candidate.addArgument(af.args[i]);
        }
        for (int i = 0; i < af.args.size(); i++) {
            for (int j = 0; j < af.args.size(); j++) {
                if (!af.enforce[i] || !af.enforce[j]) {
                    //cout << "ipamir_val_lit\t" << encoder.att_var[make_pair(i,j)] << "\n";
                    if (ipamir_val_lit(maxsat_solver, encoder.att_var[make_pair(i,j)]) > 0) {
                        candidate.addAttack(make_pair(af.args[i], af.args[j]));
                    }
                }
            }
        }
        //candidate.print();

        StaticAFEncoder sat_encoder(candidate);
        sat_encoder.generate_encoding();

        void * sat_solver = ipasir_init();
        for (size_t i = 0; i < sat_encoder.formula.hard_clauses.size(); i++) {
            for (size_t j = 0; j < sat_encoder.formula.hard_clauses[i].size(); j++) {
                ipasir_add(sat_solver, sat_encoder.formula.hard_clauses[i][j]);
            }
            ipasir_add(sat_solver, 0);
        }

        for (int i = 0; i < af.args.size(); i++) {
            if (af.enforce[i]) {
                ipasir_add(sat_solver, sat_encoder.arg_accepted_var[i]);
                ipasir_add(sat_solver, 0); 
            }
        }
        vector<int> clause;
        for (int i = 0; i < af.args.size(); i++) {
            if (!af.enforce[i]) {
                ipasir_add(sat_solver, sat_encoder.arg_accepted_var[i]);
            }
        }
        ipasir_add(sat_solver, 0);

        code = ipasir_solve(sat_solver);
        if (code == 10) {
            vector<int> labeling(af.args.size(), 0);
            for (int i = 0; i < af.args.size(); i++) {
                if (ipasir_val(sat_solver, sat_encoder.arg_accepted_var[i]) > 0) {
                    labeling[i] = 1;
                } else if (ipasir_val(sat_solver, sat_encoder.arg_rejected_var[i]) > 0) {
                    labeling[i] = -1;
                }
            }
            //cout << "ipamir_add_hard\t";
            for (int i = 0; i < af.args.size(); i++) {
                for (int j = 0; j < af.args.size(); j++) {
                    if (candidate.att_exists[make_pair(i,j)]) {
                        if (labeling[i] == 1 && labeling[j] == -1) {
                            //cout << -encoder.att_var[make_pair(i,j)] << " ";
                            ipamir_add_hard(maxsat_solver, -encoder.att_var[make_pair(i,j)]);
                        }
                    } else {
                        if ((labeling[i] == 1 && labeling[j] == 1) || (labeling[i] == 0 && labeling[j] == 1)) {
                            if (!af.enforce[i] || !af.enforce[j]) {
                                //cout << encoder.att_var[make_pair(i,j)] << " ";
                                ipamir_add_hard(maxsat_solver, encoder.att_var[make_pair(i,j)]);
                            }
                        }
                    }
                }
            }
            //cout << " 0\n";
            ipamir_add_hard(maxsat_solver, 0);
        } else if (code == 20) {
            cout << "s OPTIMUM FOUND\n";
            cout << "o " << ipamir_val_obj(maxsat_solver) << "\n";
            candidate.print();
            return 0;
        } else {
            cout << "ERROR: ipasir_solve returned " << code << ". Terminating.\n";
            return code;
        }
    }

}
