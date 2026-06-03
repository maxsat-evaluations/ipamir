#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace std;

extern "C" {
#include "ipamir.h"
}

struct SoftClause {
    uint64_t weight = 0;
    int32_t clause_lit = 0;
};

struct Layer {
    int index = 0;
    size_t hard_clause_prefix = 0;
    vector<SoftClause> soft_clauses;
    vector<int32_t> assumptions;
};

static bool starts_with(const string & line, const string & prefix) {
    return line.rfind(prefix, 0) == 0;
}

static bool parse_int_after_prefix(const string & line, const string & prefix, int & value) {
    if (!starts_with(line, prefix)) return false;
    istringstream iss(line.substr(prefix.size()));
    iss >> value;
    return !iss.fail();
}

static bool parse_size_after_prefix(const string & line, const string & prefix, size_t & value) {
    if (!starts_with(line, prefix)) return false;
    istringstream iss(line.substr(prefix.size()));
    iss >> value;
    return !iss.fail();
}

static bool parse_trace(const string & filename,
                        vector<vector<int32_t>> & hard_clauses,
                        vector<Layer> & layers,
                        int32_t & max_var) {
    ifstream input(filename);
    if (!input) return false;

    string line;
    Layer * current_layer = nullptr;
    bool in_soft_section = false;
    bool in_assumption_section = false;

    while (getline(input, line)) {
        if (line.empty()) continue;

        if (line[0] != 'c') {
            istringstream iss(line);
            vector<int32_t> clause;
            int32_t lit = 0;
            while (iss >> lit) {
                if (lit == 0) break;
                clause.push_back(lit);
                max_var = max(max_var, static_cast<int32_t>(abs(lit)));
            }
            if (clause.empty()) return false;
            hard_clauses.push_back(clause);
            continue;
        }

        int layer_index = 0;
        if (parse_int_after_prefix(line, "c ==================== layer ", layer_index)) {
            layers.push_back(Layer{});
            current_layer = &layers.back();
            current_layer->index = layer_index;
            current_layer->hard_clause_prefix = hard_clauses.size();
            in_soft_section = false;
            in_assumption_section = false;
            continue;
        }

        if (current_layer == nullptr) continue;

        size_t hard_clause_prefix = 0;
        if (parse_size_after_prefix(line, "c hard_clause_prefix=", hard_clause_prefix)) {
            if (hard_clause_prefix != current_layer->hard_clause_prefix) {
                cerr << "c ERROR: layer " << current_layer->index
                     << " has inconsistent hard_clause_prefix (" << hard_clause_prefix
                     << " != " << current_layer->hard_clause_prefix << ")\n";
                return false;
            }
            continue;
        }

        if (line == "c SOFT_CLAUSES_BEGIN") {
            in_soft_section = true;
            in_assumption_section = false;
            continue;
        }
        if (line == "c SOFT_CLAUSES_END") {
            in_soft_section = false;
            continue;
        }
        if (line == "c ASSUMPTIONS_BEGIN") {
            in_assumption_section = true;
            in_soft_section = false;
            continue;
        }
        if (line == "c ASSUMPTIONS_END") {
            in_assumption_section = false;
            continue;
        }

        if (in_soft_section && starts_with(line, "c soft ")) {
            istringstream iss(line.substr(7));
            SoftClause soft_clause;
            int32_t zero = 1;
            iss >> soft_clause.weight >> soft_clause.clause_lit >> zero;
            if (iss.fail() || zero != 0) return false;
            current_layer->soft_clauses.push_back(soft_clause);
            max_var = max(max_var, static_cast<int32_t>(abs(soft_clause.clause_lit)));
            continue;
        }

        if (in_assumption_section && starts_with(line, "c assumption ")) {
            istringstream iss(line.substr(13));
            int32_t assumption = 0;
            iss >> assumption;
            if (iss.fail()) return false;
            current_layer->assumptions.push_back(assumption);
            max_var = max(max_var, static_cast<int32_t>(abs(assumption)));
        }
    }

    return !hard_clauses.empty() && !layers.empty();
}

static void add_hard_clause(void * solver, const vector<int32_t> & clause) {
    for (int32_t lit : clause) ipamir_add_hard(solver, lit);
    ipamir_add_hard(solver, 0);
}

static void set_soft_clauses(void * solver,
                             vector<int32_t> & active_soft_lits,
                             const vector<SoftClause> & soft_clauses) {
    for (int32_t lit : active_soft_lits) ipamir_add_soft_lit(solver, lit, 0);
    active_soft_lits.clear();

    for (const SoftClause & soft_clause : soft_clauses) {
        const int32_t soft_lit = -soft_clause.clause_lit;
        ipamir_add_soft_lit(solver, soft_lit, soft_clause.weight);
        active_soft_lits.push_back(soft_lit);
    }
}

static size_t get_clause_count(size_t hard_clause_count, const Layer & layer) {
    return hard_clause_count + layer.soft_clauses.size();
}

[[maybe_unused]] static void print_model(void * solver, int32_t max_var) {
    cout << "v ";
    for (int32_t var = 1; var <= max_var; ++var) {
        cout << ipamir_val_lit(solver, var) << " ";
    }
    cout << "\n";
}

int main(int argc, char ** argv) {
    if (argc < 2) {
        cout << "USAGE: ./ipamirsibyltrace <formula.wcnf>\n\n";
        cout << "where <formula.wcnf> is a SibylSat trace generated to find an optimal solution for a TOHTN planning problem.\n";
        return 1;
    }

    vector<vector<int32_t>> hard_clauses;
    vector<Layer> layers;
    int32_t max_var = 0;

    cout << "c parsing file " << argv[1] << "...\n";
    if (!parse_trace(argv[1], hard_clauses, layers, max_var)) {
        cerr << "c ERROR: could not parse SibylSat trace " << argv[1] << "\n";
        return 1;
    }

    cout << "c parsed " << hard_clauses.size() << " hard clauses and "
         << layers.size() << " layers\n";

    void * solver = ipamir_init();
    size_t loaded_hard_clauses = 0;
    vector<int32_t> active_soft_lits;

    for (const Layer & layer : layers) {
        if (layer.hard_clause_prefix > hard_clauses.size()) {
            cerr << "c ERROR: layer " << layer.index
                 << " refers to " << layer.hard_clause_prefix
                 << " hard clauses but only " << hard_clauses.size() << " were parsed\n";
            ipamir_release(solver);
            return 1;
        }

        while (loaded_hard_clauses < layer.hard_clause_prefix) {
            add_hard_clause(solver, hard_clauses[loaded_hard_clauses++]);
        }

        set_soft_clauses(solver, active_soft_lits, layer.soft_clauses);

        cout << "\nc layer " << layer.index << ": solve abstract MaxSAT call "
             << "(clauses=" << get_clause_count(loaded_hard_clauses, layer)
             << ", assumptions=0)\n";
        const int abstract_result = ipamir_solve(solver);
        if (abstract_result == 20) {
            cout << "s UNSATISFIABLE\n";
            cout << "c hard clauses are already infeasible at layer " << layer.index << "\n";
            ipamir_release(solver);
            return 0;
        }
        if (abstract_result != 30) {
            cerr << "c ERROR: abstract MaxSAT call returned " << abstract_result
                 << " on layer " << layer.index << "\n";
            ipamir_release(solver);
            return 1;
        }

        const uint64_t abstract_objective = ipamir_val_obj(solver);
        cout << "c layer " << layer.index << ": abstract objective = "
             << abstract_objective << "\n";


        // Solve the same MaxSAT instance but with the additional assumptions corresponding to the current layer. 
        // If there is a solution and the objective value of that solution is the same as the one found without assumptions, then we have proved that this solution
        // is optimal for the TOHTN problem. 
        for (int32_t assumption : layer.assumptions) ipamir_assume(solver, assumption);

        cout << "c layer " << layer.index << ": solve primitive MaxSAT call "
             << "(clauses=" << get_clause_count(loaded_hard_clauses, layer)
             << ", assumptions=" << layer.assumptions.size() << ")\n";
        const int primitive_result = ipamir_solve(solver);
        if (primitive_result == 20) {
            cout << "c layer " << layer.index
                 << ": primitive call is infeasible under assumptions\n";
            continue;
        }
        if (primitive_result != 30) {
            cerr << "c ERROR: primitive MaxSAT call returned " << primitive_result
                 << " on layer " << layer.index << "\n";
            ipamir_release(solver);
            return 1;
        }

        const uint64_t primitive_objective = ipamir_val_obj(solver);
        cout << "c layer " << layer.index << ": primitive objective = "
             << primitive_objective << "\n";

        if (primitive_objective == abstract_objective) {
            cout << "s OPTIMUM FOUND\n";
            cout << "o " << primitive_objective << "\n";
            cout << "c globally optimal primitive solution found at layer "
                 << layer.index << "\n";
            // print_model(solver, max_var);
            ipamir_release(solver);
            return 0;
        }
    }

    cout << "s UNKNOWN\n";
    cout << "c no layer in the trace proved global optimality\n";
    ipamir_release(solver);
    return 0;
}
