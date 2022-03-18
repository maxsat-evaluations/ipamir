#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>

using namespace std;

extern "C" {
#include "ipamir.h"
}

bool initialize_solver(void * solver, int32_t & n_vars,
                       vector<int32_t> & soft_lits, string filename) {
    ifstream wcnf(filename);
    string line;
    vector<vector<int32_t>> hard_clauses;
    vector<pair<uint64_t,vector<int32_t>>> soft_clauses;

    // parse wcnf
    while (getline(wcnf, line)) {
        if (line[0] == 'c') continue;
        istringstream iss(line);
        string hard; iss >> hard;
        vector<int32_t> clause;
        int32_t lit;
        while (iss >> lit)
            clause.push_back(lit);
        int32_t tmp = clause.back();
        if (tmp) return false;
        clause.pop_back();
        if (hard == "h")
            hard_clauses.push_back(clause);
        else {
            uint64_t weight = stoull(hard);
            soft_clauses.push_back(make_pair(weight, clause));
        }
    }

    n_vars = 0;
    for (auto const &clause : hard_clauses)
        for (auto lit : clause)
            n_vars = max(n_vars, abs(lit));
    for (auto const &[weight, clause] : soft_clauses)
        for (auto lit : clause)
            n_vars = max(n_vars, abs(lit));

    // add hard clauses via api
    for (auto const &clause : hard_clauses) {
        for (auto lit : clause)
            ipamir_add_hard(solver, lit);
        ipamir_add_hard(solver, 0);
    }

    // normalize soft clauses and add soft literals via api
    int32_t n_bvars = 0;
    for (auto const &[weight, clause] : soft_clauses) {
        if (clause.size() == 1) {
            ipamir_add_soft_lit(solver, -clause[0], weight);
            soft_lits.push_back(-clause[0]);
        } else {
            ++n_bvars;
            ipamir_add_hard(solver, n_vars + n_bvars);
            for (auto lit : clause)
                ipamir_add_hard(solver, lit);
            ipamir_add_hard(solver, 0);
            ipamir_add_soft_lit(solver, n_vars + n_bvars, weight);
            soft_lits.push_back(n_vars + n_bvars);
        }
    }
    return true;
}

int32_t solve_and_print_result(void * solver, int32_t n_vars)
{
    int32_t res = ipamir_solve(solver);
    if (res == 20)
        cout << "s UNSATISFIABLE\n";
    else if (res == 30) {
        cout << "s OPTIMUM FOUND\n";
        cout << "o " << ipamir_val_obj(solver) << "\n";
        cout << "v ";
        for (int32_t var = 1; var <= n_vars; var++)
            cout << ipamir_val_lit(solver, var) << " ";
        cout << "\n";
    } else cout << "c WARNING: ipamir_solve returned " << res << "\n";
    return res;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        cout << "USAGE: ./ipamirapp <input_file_name>\n\n";
        cout << "where <input_file_name> is a MaxSAT instance as specified in the MaxSAT evaluation 2022 rules:\n";
        cout << "https://maxsat-evaluations.github.io/2022/rules.html (Input and Output Requirements)\n\n";
        cout << "See ./inputs for example input files.\n";
        return 1;
    }

    void * solver = ipamir_init();
    srand(2022);
    int32_t n_vars = 0;
    vector<int32_t> soft_lits;

    if (!initialize_solver(solver, n_vars, soft_lits, argv[1])) {
        cout << "ERROR: Input file cannot be read.\n";
        return 0;
    }

    int32_t res = 0;
    res = solve_and_print_result(solver, n_vars);
    if (res == 20) {
        ipamir_release(solver);
        return 0;
    }

    // block two optimal solutions
    int32_t blocked_sols = 2;
    for (int32_t i = 0; i < blocked_sols; i++) {
        vector<int32_t> sol;
        for (int32_t var = 1; var <= n_vars; var++)
            sol.push_back(ipamir_val_lit(solver, var));
        for (int32_t val : sol)
            ipamir_add_hard(solver, -val);
        ipamir_add_hard(solver, 0);
        res = solve_and_print_result(solver, n_vars);
        if (res == 20) {
            ipamir_release(solver);
            return 0;
        }
    }

    // assume variables to be true or false with probability 0.25
    for (int32_t var = 1; var <= n_vars; var++) {
        if (rand()%2) {
            if (rand()%2)
                ipamir_assume(solver, var);
            else
                ipamir_assume(solver, -var);
        }
    }
    res = solve_and_print_result(solver, n_vars);

    // harden soft clauses with probability 0.5
    for (auto lit : soft_lits)
        if (rand()%2)
            ipamir_assume(solver, -lit);
    res = solve_and_print_result(solver, n_vars);

    // ignore soft clauses with probability 0.5
    for (auto lit : soft_lits)
        if (rand()%2)
            ipamir_assume(solver, lit);
    res = solve_and_print_result(solver, n_vars);

    // change weights of soft clauses to random integers between 1 and 10
    for (auto lit : soft_lits) {
        uint64_t new_weight = rand() % 10 + 1;
        ipamir_add_soft_lit(solver, lit, new_weight);
    }
    res = solve_and_print_result(solver, n_vars);

    ipamir_release(solver);
    return 0;
}
