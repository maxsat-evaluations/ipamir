#include <algorithm>
#include <chrono>
#include <cctype>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Top-level build configuration
// ---------------------------------------------------------------------------
// Parser backend selector
//   1 -> C-style parser backend
//   2 -> C++ parser backend
#ifndef IPAMIRWCNFI_PARSER_BACKEND
#define IPAMIRWCNFI_PARSER_BACKEND 1
#endif

#define IPAMIRWCNFI_PARSER_BACKEND_C 1
#define IPAMIRWCNFI_PARSER_BACKEND_CPP 2

#if IPAMIRWCNFI_PARSER_BACKEND != IPAMIRWCNFI_PARSER_BACKEND_C && \
    IPAMIRWCNFI_PARSER_BACKEND != IPAMIRWCNFI_PARSER_BACKEND_CPP
#error "IPAMIRWCNFI_PARSER_BACKEND must be 1 (C) or 2 (C++)"
#endif

// Stack buffers used by the C-style parser backend.
#ifndef IPAMIRWCNFI_STACK_LINE_BYTES
#define IPAMIRWCNFI_STACK_LINE_BYTES 65536
#endif

#ifndef IPAMIRWCNFI_STACK_CLAUSE_LITS
#define IPAMIRWCNFI_STACK_CLAUSE_LITS 32768
#endif

extern "C" {
#include "ipamir.h"
}

using std::cerr;
using std::cout;
using std::ifstream;
using std::map;
using std::set;
using std::string;
using std::vector;

namespace {

using Clause = vector<int32_t>;

struct Options {
    enum class UnsolvedChangesMode { Silent, Warn, Error };

    bool allow_multi_soft = false;
    bool canonical_soft_clauses = false;
    bool print_cost_history = true;
    bool print_model_history = false;
    bool compressed_model = false;
    bool print_stats = false;
    UnsolvedChangesMode unsolved_changes_mode = UnsolvedChangesMode::Silent;
};

#if IPAMIRWCNFI_PARSER_BACKEND == IPAMIRWCNFI_PARSER_BACKEND_CPP
bool parse_clause(const vector<string> & tokens, Clause & clause_out) {
    clause_out.clear();
    if (tokens.empty()) {
        return false;
    }
    for (const auto & token : tokens) {
        std::size_t idx = 0;
        long long parsed = 0;
        try {
            parsed = std::stoll(token, &idx);
        } catch (...) {
            return false;
        }
        if (idx != token.size()) {
            return false;
        }
        if (parsed < std::numeric_limits<int32_t>::min() ||
            parsed > std::numeric_limits<int32_t>::max()) {
            return false;
        }
        const int32_t lit = static_cast<int32_t>(parsed);
        if (lit == std::numeric_limits<int32_t>::min()) {
            return false;
        }
        clause_out.push_back(lit);
    }
    if (clause_out.back() != 0) {
        return false;
    }
    clause_out.pop_back();
    return true;
}
#endif

#if IPAMIRWCNFI_PARSER_BACKEND == IPAMIRWCNFI_PARSER_BACKEND_CPP
void add_hard_clause(void * solver, const Clause & clause) {
    for (int32_t lit : clause) {
        ipamir_add_hard(solver, lit);
    }
    ipamir_add_hard(solver, 0);
}
#endif

string build_soft_key(const Clause & clause, bool canonicalize) {
    Clause tmp = clause;
    if (canonicalize) {
        std::sort(tmp.begin(), tmp.end());
    }
    string key;
    key.reserve(tmp.size() * 12);
    for (int32_t lit : tmp) {
        key.append(std::to_string(lit));
        key.push_back(',');
    }
    return key;
}

int normalize_exit_code(int raw_code) {
    // MSE/incremental-track compatible output codes.
    if (raw_code == 0 || raw_code == 10 || raw_code == 20 || raw_code == 30) {
        return raw_code;
    }
    // Unsupported/driver-internal solver states map to "unknown".
    return 0;
}

void print_model(void * solver, int32_t n_vars, bool compressed) {
    if (compressed) {
        cout << "v ";
        for (int32_t var = 1; var <= n_vars; ++var) {
            const int32_t v = ipamir_val_lit(solver, var);
            cout << ((v > 0) ? '1' : '0');
        }
        cout << "\n";
        return;
    }
    cout << "v ";
    for (int32_t var = 1; var <= n_vars; ++var) {
        cout << ipamir_val_lit(solver, var);
        if (var < n_vars) {
            cout << " ";
        }
    }
    cout << "\n";
}

const char * mse_status_for_code(int code) {
    if (code == 30) {
        return "s OPTIMUM FOUND";
    }
    if (code == 20) {
        return "s UNSATISFIABLE";
    }
    if (code == 10) {
        return "s SATISFIABLE";
    }
    return "s UNKNOWN";
}

#if IPAMIRWCNFI_PARSER_BACKEND == IPAMIRWCNFI_PARSER_BACKEND_CPP
int32_t max_abs_lit(const Clause & clause) {
    int32_t m = 0;
    for (int32_t lit : clause) {
        m = std::max(m, static_cast<int32_t>(std::abs(lit)));
    }
    return m;
}
#endif

#if IPAMIRWCNFI_PARSER_BACKEND == IPAMIRWCNFI_PARSER_BACKEND_CPP
int run_wcnfi_cpp(const string & path, const Options & options) {
    ifstream in(path);
    if (!in) {
        cerr << "ERROR: could not open file: " << path << "\n";
        return 1;
    }

    void * solver = ipamir_init();
    if (!solver) {
        cerr << "ERROR: ipamir_init failed\n";
        return 1;
    }

    bool all_sat = true;
    vector<double> query_times;
    map<string, int32_t> soft_clause_lits;
    int32_t max_var = 0;
    int final_exit_code = 0;
    bool dirty_since_last_query = false;

    string raw;
    std::size_t line_no = 0;
    std::size_t query_idx = 0;
    while (std::getline(in, raw)) {
        ++line_no;
        if (raw.empty()) {
            continue;
        }

        std::istringstream iss(raw);
        vector<string> parts;
        string tok;
        while (iss >> tok) {
            parts.push_back(tok);
        }
        if (parts.empty()) {
            continue;
        }

        if (parts[0][0] == 'c') {
            continue;
        }

        if (parts.size() < 2) {
            cerr << "ERROR: malformed line " << line_no << ": " << raw << "\n";
            ipamir_release(solver);
            return 1;
        }

        Clause clause;
        vector<string> clause_tokens(parts.begin() + 1, parts.end());
        if (!parse_clause(clause_tokens, clause)) {
            cerr << "ERROR: malformed clause on line " << line_no << ": " << raw << "\n";
            ipamir_release(solver);
            return 1;
        }

        max_var = std::max(max_var, max_abs_lit(clause));
        const string & head = parts[0];

        if (head == "h") {
            add_hard_clause(solver, clause);
            dirty_since_last_query = true;
            continue;
        }

        if (head == "r") {
            ++query_idx;
            for (int32_t lit : clause) {
                ipamir_assume(solver, lit);
            }
            const auto t0 = std::chrono::steady_clock::now();
            const int raw_code = ipamir_solve(solver);
            const auto t1 = std::chrono::steady_clock::now();
            const double dt = std::chrono::duration<double>(t1 - t0).count();
            query_times.push_back(dt);
            const int code = normalize_exit_code(raw_code);
            final_exit_code = code;
            dirty_since_last_query = false;

            if (!(code == 10 || code == 30)) {
                all_sat = false;
            }
            if (options.print_cost_history) {
                cout << "c query " << query_idx << "\n";
                if (code == 10 || code == 30) {
                    const uint64_t obj = ipamir_val_obj(solver);
                    cout << "o " << obj << "\n";
                }
                cout << mse_status_for_code(code) << "\n";
            }
            if (options.print_model_history && (code == 10 || code == 30)) {
                print_model(solver, max_var, options.compressed_model);
            }
            if (!(raw_code == 0 || raw_code == 10 || raw_code == 20 || raw_code == 30)) {
                cerr << "WARNING: ipamir_solve returned non-MSE code " << raw_code
                     << " on query at line " << line_no << ", mapped to 0\n";
            }
            continue;
        }

        std::size_t idx = 0;
        uint64_t weight = 0;
        try {
            weight = std::stoull(head, &idx);
        } catch (...) {
            cerr << "ERROR: malformed weight on line " << line_no << ": " << raw << "\n";
            ipamir_release(solver);
            return 1;
        }
        if (idx != head.size()) {
            cerr << "ERROR: malformed weight on line " << line_no << ": " << raw << "\n";
            ipamir_release(solver);
            return 1;
        }

        if (!options.allow_multi_soft && clause.size() > 1) {
            cerr << "ERROR: multi-literal soft clause on line " << line_no
                 << " requires --allow-multi-soft-clauses\n";
            ipamir_release(solver);
            return 1;
        }
        const string key = build_soft_key(clause, options.canonical_soft_clauses);
        auto it = soft_clause_lits.find(key);
        if (it == soft_clause_lits.end()) {
            int32_t soft_lit = 0;
            if (clause.size() == 1) {
                soft_lit = -clause[0];
            } else {
                ++max_var;
                soft_lit = max_var;
                ipamir_add_hard(solver, soft_lit);
                for (int32_t lit : clause) {
                    ipamir_add_hard(solver, lit);
                }
                ipamir_add_hard(solver, 0);
            }
            soft_clause_lits.emplace(key, soft_lit);
            ipamir_add_soft_lit(solver, soft_lit, weight);
        } else {
            ipamir_add_soft_lit(solver, it->second, weight);
        }
        dirty_since_last_query = true;
    }

    if (query_times.empty()) {
        cerr << "ERROR: no query lines ('r ... 0') found\n";
        ipamir_release(solver);
        return 1;
    }

    if (dirty_since_last_query) {
        if (options.unsolved_changes_mode == Options::UnsolvedChangesMode::Warn) {
            cerr << "WARNING: unsolved changes after last query were ignored\n";
        } else if (options.unsolved_changes_mode == Options::UnsolvedChangesMode::Error) {
            cerr << "ERROR: unsolved changes after last query\n";
            ipamir_release(solver);
            return 1;
        }
    }

    double total = 0.0;
    double max_t = 0.0;
    for (double t : query_times) {
        total += t;
        max_t = std::max(max_t, t);
    }
    const double avg = total / static_cast<double>(query_times.size());

    if (options.print_stats) {
        cout << std::setprecision(17);
        cout << "file=" << path << "\n";
        cout << "queries=" << query_times.size() << "\n";
        cout << "all_sat=" << (all_sat ? "True" : "False") << "\n";
        cout << "total_s=" << total << "\n";
        cout << "avg_s=" << avg << "\n";
        cout << "max_s=" << max_t << "\n";
    }

    ipamir_release(solver);
    return final_exit_code;
}
#endif

#if IPAMIRWCNFI_PARSER_BACKEND == IPAMIRWCNFI_PARSER_BACKEND_C
int run_wcnfi_c(const string & path, const Options & options) {
    static constexpr size_t kMaxLineBytes = IPAMIRWCNFI_STACK_LINE_BYTES;
    static constexpr size_t kMaxClauseLits = IPAMIRWCNFI_STACK_CLAUSE_LITS;

    FILE * in = std::fopen(path.c_str(), "r");
    if (!in) {
        cerr << "ERROR: could not open file: " << path << "\n";
        return 1;
    }

    void * solver = ipamir_init();
    if (!solver) {
        cerr << "ERROR: ipamir_init failed\n";
        std::fclose(in);
        return 1;
    }

    bool all_sat = true;
    vector<double> query_times;
    map<string, int32_t> soft_clause_lits;
    int32_t max_var = 0;
    int final_exit_code = 0;
    bool dirty_since_last_query = false;
    std::size_t line_no = 0;
    std::size_t query_idx = 0;
    char line[kMaxLineBytes];

    auto fail_and_close = [&](const string & msg) -> int {
        cerr << msg << "\n";
        ipamir_release(solver);
        std::fclose(in);
        return 1;
    };

    while (std::fgets(line, static_cast<int>(kMaxLineBytes), in)) {
        ++line_no;
        size_t len = std::strlen(line);
        if (len == 0) {
            continue;
        }
        if (line[len - 1] != '\n' && !std::feof(in)) {
            return fail_and_close("ERROR: line too long at line " + std::to_string(line_no));
        }
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }

        char * p = line;
        while (*p && std::isspace(static_cast<unsigned char>(*p))) {
            ++p;
        }
        if (*p == '\0') {
            continue;
        }
        if (*p == 'c' && (p[1] == '\0' || std::isspace(static_cast<unsigned char>(p[1])))) {
            continue;
        }

        // Parse head token.
        char head[64];
        size_t h = 0;
        while (*p && !std::isspace(static_cast<unsigned char>(*p))) {
            if (h + 1 >= sizeof(head)) {
                return fail_and_close("ERROR: malformed line " + std::to_string(line_no) + ": head too long");
            }
            head[h++] = *p++;
        }
        head[h] = '\0';
        while (*p && std::isspace(static_cast<unsigned char>(*p))) {
            ++p;
        }
        if (*p == '\0') {
            return fail_and_close("ERROR: malformed line " + std::to_string(line_no) + ": missing clause");
        }

        // Parse integer tail into stack array.
        int32_t lits[kMaxClauseLits];
        size_t lit_count = 0;
        while (*p) {
            errno = 0;
            char * end = nullptr;
            long long parsed = std::strtoll(p, &end, 10);
            if (end == p) {
                return fail_and_close("ERROR: malformed clause on line " + std::to_string(line_no));
            }
            if (errno == ERANGE || parsed < std::numeric_limits<int32_t>::min() ||
                parsed > std::numeric_limits<int32_t>::max()) {
                return fail_and_close("ERROR: malformed clause on line " + std::to_string(line_no));
            }
            if (lit_count >= kMaxClauseLits) {
                return fail_and_close("ERROR: clause too long on line " + std::to_string(line_no));
            }
            const int32_t lit = static_cast<int32_t>(parsed);
            if (lit != 0 && lit == std::numeric_limits<int32_t>::min()) {
                return fail_and_close("ERROR: malformed clause on line " + std::to_string(line_no));
            }
            lits[lit_count++] = lit;
            p = end;
            while (*p && std::isspace(static_cast<unsigned char>(*p))) {
                ++p;
            }
        }

        if (lit_count == 0 || lits[lit_count - 1] != 0) {
            return fail_and_close("ERROR: malformed clause on line " + std::to_string(line_no));
        }
        const size_t n_lits = lit_count - 1; // without trailing 0

        // Track max variable id from clause literals.
        for (size_t i = 0; i < n_lits; ++i) {
            const int32_t a = static_cast<int32_t>(std::abs(lits[i]));
            if (a > max_var) {
                max_var = a;
            }
        }

        if (std::strcmp(head, "h") == 0) {
            for (size_t i = 0; i < n_lits; ++i) {
                ipamir_add_hard(solver, lits[i]);
            }
            ipamir_add_hard(solver, 0);
            dirty_since_last_query = true;
            continue;
        }

        if (std::strcmp(head, "r") == 0) {
            ++query_idx;
            for (size_t i = 0; i < n_lits; ++i) {
                ipamir_assume(solver, lits[i]);
            }
            const auto t0 = std::chrono::steady_clock::now();
            const int raw_code = ipamir_solve(solver);
            const auto t1 = std::chrono::steady_clock::now();
            const double dt = std::chrono::duration<double>(t1 - t0).count();
            query_times.push_back(dt);
            const int code = normalize_exit_code(raw_code);
            final_exit_code = code;
            dirty_since_last_query = false;

            if (!(code == 10 || code == 30)) {
                all_sat = false;
            }
            if (options.print_cost_history) {
                cout << "c query " << query_idx << "\n";
                if (code == 10 || code == 30) {
                    const uint64_t obj = ipamir_val_obj(solver);
                    cout << "o " << obj << "\n";
                }
                cout << mse_status_for_code(code) << "\n";
            }
            if (options.print_model_history && (code == 10 || code == 30)) {
                print_model(solver, max_var, options.compressed_model);
            }
            if (!(raw_code == 0 || raw_code == 10 || raw_code == 20 || raw_code == 30)) {
                cerr << "WARNING: ipamir_solve returned non-MSE code " << raw_code
                     << " on query at line " << line_no << ", mapped to 0\n";
            }
            continue;
        }

        // Soft clause line: head is weight.
        errno = 0;
        char * endw = nullptr;
        const unsigned long long w = std::strtoull(head, &endw, 10);
        if (endw == head || *endw != '\0' || errno == ERANGE) {
            return fail_and_close("ERROR: malformed weight on line " + std::to_string(line_no));
        }
        const uint64_t weight = static_cast<uint64_t>(w);

        if (!options.allow_multi_soft && n_lits > 1) {
            return fail_and_close("ERROR: multi-literal soft clause on line " + std::to_string(line_no) +
                                  " requires --allow-multi-soft-clauses");
        }

        Clause clause;
        clause.reserve(n_lits);
        for (size_t i = 0; i < n_lits; ++i) {
            clause.push_back(lits[i]);
        }
        const string key = build_soft_key(clause, options.canonical_soft_clauses);
        auto it = soft_clause_lits.find(key);
        if (it == soft_clause_lits.end()) {
            int32_t soft_lit = 0;
            if (n_lits == 1) {
                soft_lit = -lits[0];
            } else {
                ++max_var;
                soft_lit = max_var;
                ipamir_add_hard(solver, soft_lit);
                for (size_t i = 0; i < n_lits; ++i) {
                    ipamir_add_hard(solver, lits[i]);
                }
                ipamir_add_hard(solver, 0);
            }
            soft_clause_lits.emplace(key, soft_lit);
            ipamir_add_soft_lit(solver, soft_lit, weight);
        } else {
            ipamir_add_soft_lit(solver, it->second, weight);
        }
        dirty_since_last_query = true;
    }

    if (query_times.empty()) {
        cerr << "ERROR: no query lines ('r ... 0') found\n";
        ipamir_release(solver);
        std::fclose(in);
        return 1;
    }

    if (dirty_since_last_query) {
        if (options.unsolved_changes_mode == Options::UnsolvedChangesMode::Warn) {
            cerr << "WARNING: unsolved changes after last query were ignored\n";
        } else if (options.unsolved_changes_mode == Options::UnsolvedChangesMode::Error) {
            cerr << "ERROR: unsolved changes after last query\n";
            ipamir_release(solver);
            std::fclose(in);
            return 1;
        }
    }

    double total = 0.0;
    double max_t = 0.0;
    for (double t : query_times) {
        total += t;
        max_t = std::max(max_t, t);
    }
    const double avg = total / static_cast<double>(query_times.size());

    if (options.print_stats) {
        cout << std::setprecision(17);
        cout << "file=" << path << "\n";
        cout << "queries=" << query_times.size() << "\n";
        cout << "all_sat=" << (all_sat ? "True" : "False") << "\n";
        cout << "total_s=" << total << "\n";
        cout << "avg_s=" << avg << "\n";
        cout << "max_s=" << max_t << "\n";
    }

    ipamir_release(solver);
    std::fclose(in);
    return final_exit_code;
}
#endif

int run_wcnfi(const string & path, const Options & options) {
#if IPAMIRWCNFI_PARSER_BACKEND == IPAMIRWCNFI_PARSER_BACKEND_CPP
    return run_wcnfi_cpp(path, options);
#else
    return run_wcnfi_c(path, options);
#endif
}

const char * parser_backend_name() {
#if IPAMIRWCNFI_PARSER_BACKEND == IPAMIRWCNFI_PARSER_BACKEND_CPP
    return "cpp";
#else
    return "c";
#endif
}

void print_help(const char * prog) {
    cout << "Usage: " << prog << " --file <input.wcnfi> [options]\n";
    cout << "   or: " << prog << " <input.wcnfi> [options]\n\n";
    cout << "Options:\n";
    cout << "  --file <path>         Path to a WCNFI input file.\n";
    cout << "  --allow-multi-soft-clauses\n";
    cout << "                        Allow non-unit soft clauses by IPAMIR reification (default: off).\n";
    cout << "  --canonical-soft-clauses\n";
    cout << "                        Canonicalize soft clause keys so permutations map to same clause (default: off).\n";
    cout << "  --no-cost-history     Disable per-query cost/status trace (default: trace enabled).\n";
    cout << "  --print-query-objs    Enable per-query trace.\n";
    cout << "                        Trace format is MSE-like and query-scoped:\n";
    cout << "                        c query <idx>, o <cost>, s <status>, optional v <model>.\n";
    cout << "  --print-model-history Print model after each SAT/OPT query (default: off).\n";
    cout << "  --compressed-model    Use 2022+ bitstring v-line format with --print-model-history.\n";
    cout << "                        Default model format is classic literal list.\n";
    cout << "  --unsolved-changes <silent|warn|error>\n";
    cout << "                        Behavior when file ends with changes not followed by query.\n";
    cout << "                        Default: silent.\n";
    cout << "  --print-stats         Print end-of-run summary stats (default: off):\n";
    cout << "                        file, queries, all_sat, total_s, avg_s, max_s.\n";
    cout << "  -h, --help            Show this help and exit.\n";
    cout << "\nCompiled parser backend: " << parser_backend_name() << "\n";
    cout << "Compile with -DIPAMIRWCNFI_PARSER_BACKEND=1 for C backend.\n";
    cout << "Compile with -DIPAMIRWCNFI_PARSER_BACKEND=2 for C++ backend.\n";
    cout << "C-backend stack sizes: line=" << IPAMIRWCNFI_STACK_LINE_BYTES
         << " bytes, clause_lits=" << IPAMIRWCNFI_STACK_CLAUSE_LITS << ".\n";
    cout << "\nExit Codes:\n";
    cout << "  0  unknown/interrupted or no proof\n";
    cout << " 10  feasible solution found (non-optimal/unknown optimality)\n";
    cout << " 20  hard clauses unsatisfiable\n";
    cout << " 30  optimal solution found\n";
    cout << "For multi-query instances, the process exits with the final query code.\n";
}

} // namespace

int main(int argc, char ** argv) {
    string file;
    bool have_file = false;
    Options options;
    for (int i = 1; i < argc; ++i) {
        const string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            print_help(argv[0]);
            return 0;
        } else if (arg == "--file") {
            if (i + 1 >= argc) {
                cerr << "ERROR: --file requires a value\n";
                print_help(argv[0]);
                return 1;
            }
            file = argv[++i];
            have_file = true;
        } else if (arg == "--allow-multi-soft-clauses") {
            options.allow_multi_soft = true;
        } else if (arg == "--canonical-soft-clauses") {
            options.canonical_soft_clauses = true;
        } else if (arg == "--no-cost-history") {
            options.print_cost_history = false;
        } else if (arg == "--print-query-objs") {
            options.print_cost_history = true;
        } else if (arg == "--print-model-history") {
            options.print_model_history = true;
        } else if (arg == "--compressed-model") {
            options.compressed_model = true;
        } else if (arg == "--unsolved-changes") {
            if (i + 1 >= argc) {
                cerr << "ERROR: --unsolved-changes requires a value\n";
                print_help(argv[0]);
                return 1;
            }
            const string mode = argv[++i];
            if (mode == "silent") {
                options.unsolved_changes_mode = Options::UnsolvedChangesMode::Silent;
            } else if (mode == "warn") {
                options.unsolved_changes_mode = Options::UnsolvedChangesMode::Warn;
            } else if (mode == "error") {
                options.unsolved_changes_mode = Options::UnsolvedChangesMode::Error;
            } else {
                cerr << "ERROR: invalid --unsolved-changes value: " << mode << "\n";
                print_help(argv[0]);
                return 1;
            }
        } else if (arg == "--print-stats") {
            options.print_stats = true;
        } else if (arg.rfind("--", 0) == 0) {
            cerr << "ERROR: unknown option: " << arg << "\n";
            print_help(argv[0]);
            return 1;
        } else {
            if (have_file) {
                cerr << "ERROR: multiple input files provided\n";
                print_help(argv[0]);
                return 1;
            }
            file = arg;
            have_file = true;
        }
    }
    if (!have_file || file.empty()) {
        cerr << "ERROR: missing input file\n";
        print_help(argv[0]);
        return 1;
    }
    return run_wcnfi(file, options);
}
