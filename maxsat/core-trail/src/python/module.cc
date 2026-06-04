#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <exception>
#include <optional>
#include <atomic>
#include <csignal>
#include <fstream>
#include <thread>
#include <chrono>
#include <stdexcept>
#include <string>
#include <vector>
#include <exception>

#include "rc2/rc2_solver.hpp"

typedef struct {
    PyObject_HEAD
    rc2::RC2StratifiedSolver* solver;
} PyRC2Stratified;

namespace {
std::atomic<rc2::RC2StratifiedSolver*> g_active_solver{nullptr};
std::atomic<bool> g_sigint_seen{false};
struct sigaction g_prev_sigint{};
bool g_sigint_installed{false};

void rc2_sigint_handler(int signum) {
    (void)signum;
    g_sigint_seen.store(true, std::memory_order_relaxed);
    PyErr_SetInterrupt();
}

void install_temp_sigint_handler(rc2::RC2StratifiedSolver* s) {
    g_active_solver.store(s, std::memory_order_relaxed);
    g_sigint_seen.store(false, std::memory_order_relaxed);
    struct sigaction sa;
    sa.sa_handler = rc2_sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, &g_prev_sigint);
    g_sigint_installed = true;
}

void restore_sigint_handler() {
    g_active_solver.store(nullptr, std::memory_order_relaxed);
    if (g_sigint_installed) {
        sigaction(SIGINT, &g_prev_sigint, nullptr);
        g_sigint_installed = false;
    }
}
}  // namespace

static int py_to_int_vector(PyObject* seq, std::vector<int>* out) {
    PyObject* it = PyObject_GetIter(seq);
    if (it == nullptr) return -1;
    PyObject* x = nullptr;
    while ((x = PyIter_Next(it)) != nullptr) {
        long v = PyLong_AsLong(x);
        Py_DECREF(x);
        if (PyErr_Occurred()) {
            Py_DECREF(it);
            return -1;
        }
        out->push_back(static_cast<int>(v));
    }
    Py_DECREF(it);
    if (PyErr_Occurred()) return -1;
    return 0;
}

static int load_hard_from_wcnf_local(PyObject* formula, int* nv, std::vector<std::vector<int>>* hard) {
    PyObject* nv_obj = PyObject_GetAttrString(formula, "nv");
    if (nv_obj == nullptr) return -1;
    long v = PyLong_AsLong(nv_obj);
    Py_DECREF(nv_obj);
    if (PyErr_Occurred()) return -1;
    if (v <= 0) {
        PyErr_SetString(PyExc_ValueError, "formula.nv must be positive");
        return -1;
    }
    *nv = static_cast<int>(v);

    PyObject* hard_obj = PyObject_GetAttrString(formula, "hard");
    if (hard_obj == nullptr) return -1;
    Py_ssize_t hsz = PySequence_Size(hard_obj);
    if (hsz < 0) {
        Py_DECREF(hard_obj);
        return -1;
    }
    hard->clear();
    hard->reserve(static_cast<std::size_t>(hsz));
    for (Py_ssize_t i = 0; i < hsz; ++i) {
        PyObject* cl = PySequence_GetItem(hard_obj, i);
        if (cl == nullptr) {
            Py_DECREF(hard_obj);
            return -1;
        }
        std::vector<int> c;
        if (py_to_int_vector(cl, &c) < 0) {
            Py_DECREF(cl);
            Py_DECREF(hard_obj);
            return -1;
        }
        Py_DECREF(cl);
        hard->push_back(std::move(c));
    }
    Py_DECREF(hard_obj);
    return 0;
}

static int load_formula_from_wcnf(rc2::RC2StratifiedSolver* solver, PyObject* formula) {
    PyObject* nv_obj = PyObject_GetAttrString(formula, "nv");
    if (nv_obj == nullptr) return -1;
    long nv = PyLong_AsLong(nv_obj);
    Py_DECREF(nv_obj);
    if (PyErr_Occurred()) return -1;
    solver->initialize_external_vars(static_cast<int>(nv));

    PyObject* hard = PyObject_GetAttrString(formula, "hard");
    PyObject* soft = PyObject_GetAttrString(formula, "soft");
    PyObject* wght = PyObject_GetAttrString(formula, "wght");
    PyObject* atms = PyObject_GetAttrString(formula, "atms");
    if (hard == nullptr || soft == nullptr || wght == nullptr) {
        Py_XDECREF(hard); Py_XDECREF(soft); Py_XDECREF(wght); Py_XDECREF(atms);
        return -1;
    }
    if (atms != nullptr) {
        Py_ssize_t asz = PySequence_Size(atms);
        if (asz < 0) {
            Py_DECREF(hard); Py_DECREF(soft); Py_DECREF(wght); Py_DECREF(atms);
            return -1;
        }
        if (asz > 0) {
            Py_DECREF(hard); Py_DECREF(soft); Py_DECREF(wght); Py_DECREF(atms);
            PyErr_SetString(PyExc_NotImplementedError, "RC2_IX2 does not support native cardinality constraints.");
            return -1;
        }
        Py_DECREF(atms);
    } else {
        PyErr_Clear();
    }

    Py_ssize_t hsz = PySequence_Size(hard);
    if (hsz < 0) {
        Py_DECREF(hard); Py_DECREF(soft); Py_DECREF(wght);
        return -1;
    }
    for (Py_ssize_t i = 0; i < hsz; ++i) {
        PyObject* cl = PySequence_GetItem(hard, i);
        if (cl == nullptr) {
            Py_DECREF(hard); Py_DECREF(soft); Py_DECREF(wght);
            return -1;
        }
        std::vector<int> c;
        if (py_to_int_vector(cl, &c) < 0) {
            Py_DECREF(cl); Py_DECREF(hard); Py_DECREF(soft); Py_DECREF(wght);
            return -1;
        }
        Py_DECREF(cl);
        try {
            solver->add_hard_clause(c);
        } catch (const std::exception& e) {
            Py_DECREF(hard); Py_DECREF(soft); Py_DECREF(wght);
            PyErr_SetString(PyExc_ValueError, e.what());
            return -1;
        }
    }

    Py_ssize_t ssz = PySequence_Size(soft);
    if (ssz < 0) {
        Py_DECREF(hard); Py_DECREF(soft); Py_DECREF(wght);
        return -1;
    }
    for (Py_ssize_t i = 0; i < ssz; ++i) {
        PyObject* cl = PySequence_GetItem(soft, i);
        PyObject* ww = PySequence_GetItem(wght, i);
        if (cl == nullptr || ww == nullptr) {
            Py_XDECREF(cl); Py_XDECREF(ww);
            Py_DECREF(hard); Py_DECREF(soft); Py_DECREF(wght);
            return -1;
        }
        long w = PyLong_AsLong(ww);
        Py_DECREF(ww);
        if (PyErr_Occurred()) {
            Py_DECREF(cl); Py_DECREF(hard); Py_DECREF(soft); Py_DECREF(wght);
            return -1;
        }
        std::vector<int> c;
        if (py_to_int_vector(cl, &c) < 0) {
            Py_DECREF(cl); Py_DECREF(hard); Py_DECREF(soft); Py_DECREF(wght);
            return -1;
        }
        Py_DECREF(cl);
        try {
            solver->add_clause(c, w);
        } catch (const std::exception& e) {
            Py_DECREF(hard); Py_DECREF(soft); Py_DECREF(wght);
            PyErr_SetString(PyExc_ValueError, e.what());
            return -1;
        }
    }

    Py_DECREF(hard);
    Py_DECREF(soft);
    Py_DECREF(wght);
    return 0;
}

static int PyRC2_init(PyRC2Stratified* self, PyObject* args, PyObject* kwargs) {
    PyObject* formula = nullptr;
    if (!PyArg_ParseTuple(args, "O", &formula)) return -1;
    rc2::SolverOptions opts;
    opts.minz = true;
    opts.trim = 0;
    opts.exhaust = false;
    opts.core_memory = false;
    opts.core_replay = 10;
    opts.solver = "g4";
    opts.adapt = true;
    opts.full_stratified = true;
    opts.nohard = true;

    if (kwargs != nullptr) {
        PyObject* v = nullptr;

        v = PyDict_GetItemString(kwargs, "minz");
        if (v) {
            int b = PyObject_IsTrue(v);
            if (b < 0) return -1;
            opts.minz = (b != 0);
        }
        v = PyDict_GetItemString(kwargs, "trim");
        if (v) {
            long t = PyLong_AsLong(v);
            if (PyErr_Occurred()) return -1;
            opts.trim = static_cast<int>(t);
        }
        v = PyDict_GetItemString(kwargs, "exhaust");
        if (v) {
            int b = PyObject_IsTrue(v);
            if (b < 0) return -1;
            opts.exhaust = (b != 0);
        }
        v = PyDict_GetItemString(kwargs, "core_memory");
        if (v) {
            int b = PyObject_IsTrue(v);
            if (b < 0) return -1;
            opts.core_memory = (b != 0);
        }
        v = PyDict_GetItemString(kwargs, "core_replay");
        if (v) {
            long cr = PyLong_AsLong(v);
            if (PyErr_Occurred()) return -1;
            opts.core_replay = static_cast<int>(cr);
        }
        v = PyDict_GetItemString(kwargs, "solver");
        if (v) {
            const char* s = PyUnicode_AsUTF8(v);
            if (s == nullptr) return -1;
            opts.solver = s;
        }
        v = PyDict_GetItemString(kwargs, "adapt");
        if (v) {
            int b = PyObject_IsTrue(v);
            if (b < 0) return -1;
            opts.adapt = (b != 0);
        }
        v = PyDict_GetItemString(kwargs, "full_stratified");
        if (v) {
            int b = PyObject_IsTrue(v);
            if (b < 0) return -1;
            opts.full_stratified = (b != 0);
        }
        v = PyDict_GetItemString(kwargs, "exploit_overlap");
        if (v) {
            int b = PyObject_IsTrue(v);
            if (b < 0) return -1;
            opts.exploit_overlap = (b != 0);
        }
        v = PyDict_GetItemString(kwargs, "blo");
        if (v) {
            const char* s = PyUnicode_AsUTF8(v);
            if (s == nullptr) return -1;
            opts.blo = s;
            if (!(opts.blo == "none" || opts.blo == "basic" || opts.blo == "div" ||
                  opts.blo == "cluster" || opts.blo == "full")) {
                PyErr_SetString(PyExc_AssertionError, "Unknown BLO strategy");
                return -1;
            }
        }
        v = PyDict_GetItemString(kwargs, "incr");
        if (v) {
            int b = PyObject_IsTrue(v);
            if (b < 0) return -1;
            opts.incr = (b != 0);
        }
        v = PyDict_GetItemString(kwargs, "nohard");
        if (v) {
            int b = PyObject_IsTrue(v);
            if (b < 0) return -1;
            opts.nohard = (b != 0);
            if (!opts.nohard) {
                PyErr_SetString(PyExc_AssertionError, "Clause hardening not supported in Incremental MaxSAT");
                return -1;
            }
        }
        (void)PyDict_GetItemString(kwargs, "process");
        v = PyDict_GetItemString(kwargs, "verbose");
        if (v) {
            long vv = PyLong_AsLong(v);
            if (PyErr_Occurred()) return -1;
            opts.verbose = static_cast<int>(vv);
        }
    }
    try {
        self->solver = new rc2::RC2StratifiedSolver(opts);
        if (self->solver == nullptr) {
            PyErr_NoMemory();
            return -1;
        }
        if (load_formula_from_wcnf(self->solver, formula) < 0) {
            delete self->solver;
            self->solver = nullptr;
            return -1;
        }
        return 0;
    } catch (const std::invalid_argument& e) {
        PyErr_SetString(PyExc_ValueError, e.what());
        return -1;
    } catch (const std::exception& e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
        return -1;
    }
}

static void PyRC2_dealloc(PyRC2Stratified* self) {
    delete self->solver;
    self->solver = nullptr;
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static inline rc2::RC2StratifiedSolver* get_solver(PyRC2Stratified* self) {
    if (self->solver == nullptr) {
        PyErr_SetString(PyExc_RuntimeError, "solver not initialized");
        return nullptr;
    }
    return self->solver;
}

static PyObject* m_add_clause(PyRC2Stratified* self, PyObject* args, PyObject* kwargs) {
    rc2::RC2StratifiedSolver* s = get_solver(self);
    if (s == nullptr) return nullptr;
    PyObject* clause_obj = nullptr;
    PyObject* weight_obj = Py_None;
    if (!PyArg_ParseTuple(args, "O|O", &clause_obj, &weight_obj)) return nullptr;
    if (kwargs != nullptr) {
        PyObject* w = PyDict_GetItemString(kwargs, "weight");
        if (w != nullptr) weight_obj = w;
    }
    std::vector<int> clause;
    if (PyTuple_Check(clause_obj) && PyTuple_Size(clause_obj) >= 2) {
        PyObject* lits_obj = PyTuple_GetItem(clause_obj, 0);  // borrowed
        if (lits_obj == nullptr) return nullptr;
        if (py_to_int_vector(lits_obj, &clause) < 0) return nullptr;
        PyErr_SetString(PyExc_NotImplementedError, "Native cardinality constraints are unsupported in rc2_ren_soft_cpp");
        return nullptr;
    }
    if (py_to_int_vector(clause_obj, &clause) < 0) return nullptr;
    std::optional<long> w = std::nullopt;
    if (weight_obj != Py_None) {
        long wi = PyLong_AsLong(weight_obj);
        if (PyErr_Occurred()) return nullptr;
        w = wi;
    }
    try {
        s->add_clause(clause, w);
    } catch (const std::invalid_argument& e) {
        PyErr_SetString(PyExc_ValueError, e.what());
        return nullptr;
    } catch (const std::exception& e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
        return nullptr;
    }
    Py_RETURN_NONE;
}

static PyObject* m_set_soft(PyRC2Stratified* self, PyObject* args, PyObject* kwargs) {
    rc2::RC2StratifiedSolver* s = get_solver(self);
    if (s == nullptr) return nullptr;
    long lit = 0, weight = 0;
    if (!PyArg_ParseTuple(args, "ll", &lit, &weight)) return nullptr;
    if (kwargs != nullptr) {
        PyObject* l = PyDict_GetItemString(kwargs, "lit");
        PyObject* w = PyDict_GetItemString(kwargs, "weight");
        if (l) lit = PyLong_AsLong(l);
        if (w) weight = PyLong_AsLong(w);
        if (PyErr_Occurred()) return nullptr;
    }
    try {
        s->set_soft(static_cast<int>(lit), weight);
    } catch (const std::invalid_argument& e) {
        PyErr_SetString(PyExc_ValueError, e.what());
        return nullptr;
    } catch (const std::exception& e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
        return nullptr;
    }
    Py_RETURN_NONE;
}

static PyObject* m_add_soft_unit(PyRC2Stratified* self, PyObject* args, PyObject* kwargs) {
    rc2::RC2StratifiedSolver* s = get_solver(self);
    if (s == nullptr) return nullptr;
    long lit = 0, weight = 0;
    if (!PyArg_ParseTuple(args, "ll", &lit, &weight)) return nullptr;
    if (kwargs != nullptr) {
        PyObject* l = PyDict_GetItemString(kwargs, "lit");
        PyObject* w = PyDict_GetItemString(kwargs, "weight");
        if (l) lit = PyLong_AsLong(l);
        if (w) weight = PyLong_AsLong(w);
        if (PyErr_Occurred()) return nullptr;
    }
    try {
        s->add_soft_unit(static_cast<int>(lit), weight);
    } catch (const std::invalid_argument& e) {
        PyErr_SetString(PyExc_ValueError, e.what());
        return nullptr;
    } catch (const std::exception& e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
        return nullptr;
    }
    Py_RETURN_NONE;
}

static PyObject* m_solve(PyRC2Stratified* self, PyObject* args, PyObject* kwargs) {
    rc2::RC2StratifiedSolver* s = get_solver(self);
    if (s == nullptr) return nullptr;
    PyObject* assumptions_obj = Py_None;
    int raise_on_abnormal = 0;
    if (!PyArg_ParseTuple(args, "|Op", &assumptions_obj, &raise_on_abnormal)) return nullptr;
    if (kwargs != nullptr) {
        PyObject* a = PyDict_GetItemString(kwargs, "assumptions");
        PyObject* r = PyDict_GetItemString(kwargs, "raise_on_abnormal");
        if (a) assumptions_obj = a;
        if (r) raise_on_abnormal = PyObject_IsTrue(r);
    }
    std::vector<int> assumptions;
    if (assumptions_obj != Py_None && py_to_int_vector(assumptions_obj, &assumptions) < 0) return nullptr;
    std::atomic<bool> solve_done{false};
    std::thread stop_watcher;
    try {
        install_temp_sigint_handler(s);
        stop_watcher = std::thread([s, &solve_done]() {
            while (!solve_done.load(std::memory_order_relaxed)) {
                if (g_sigint_seen.load(std::memory_order_relaxed)) {
                    s->request_stop();
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
        bool sat = false;
        std::exception_ptr solve_eptr;
        Py_BEGIN_ALLOW_THREADS
        try {
            sat = s->solve(assumptions, raise_on_abnormal != 0);
        } catch (...) {
            solve_eptr = std::current_exception();
        }
        Py_END_ALLOW_THREADS
        if (solve_eptr) {
            std::rethrow_exception(solve_eptr);
        }
        solve_done.store(true, std::memory_order_relaxed);
        if (stop_watcher.joinable()) stop_watcher.join();
        restore_sigint_handler();
        if (g_sigint_seen.load(std::memory_order_relaxed) || s->interrupted_last_solve()) {
            delete self->solver;
            self->solver = nullptr;
            PyErr_SetString(PyExc_KeyboardInterrupt, "RC2 solve interrupted by Ctrl+C; solver state discarded");
            return nullptr;
        }
        if (sat) Py_RETURN_TRUE;
        Py_RETURN_FALSE;
    } catch (const std::runtime_error& e) {
        solve_done.store(true, std::memory_order_relaxed);
        if (stop_watcher.joinable()) stop_watcher.join();
        g_sigint_seen.store(false, std::memory_order_relaxed);
        restore_sigint_handler();
        PyErr_SetString(PyExc_RuntimeError, e.what());
        return nullptr;
    } catch (const std::exception& e) {
        solve_done.store(true, std::memory_order_relaxed);
        if (stop_watcher.joinable()) stop_watcher.join();
        g_sigint_seen.store(false, std::memory_order_relaxed);
        restore_sigint_handler();
        PyErr_SetString(PyExc_RuntimeError, e.what());
        return nullptr;
    }
}

static PyObject* m_get_status(PyRC2Stratified* self, PyObject* args) {
    (void)args;
    rc2::RC2StratifiedSolver* s = get_solver(self);
    if (s == nullptr) return nullptr;
    return PyLong_FromLong(static_cast<long>(s->get_status()));
}

static PyObject* m_request_stop(PyRC2Stratified* self, PyObject* args) {
    (void)args;
    rc2::RC2StratifiedSolver* s = get_solver(self);
    if (s == nullptr) return nullptr;
    s->request_stop();
    Py_RETURN_NONE;
}

static PyObject* m_clear_stop(PyRC2Stratified* self, PyObject* args) {
    (void)args;
    rc2::RC2StratifiedSolver* s = get_solver(self);
    if (s == nullptr) return nullptr;
    s->clear_stop_request();
    Py_RETURN_NONE;
}

static PyObject* m_get_cost(PyRC2Stratified* self, PyObject* args) {
    (void)args;
    rc2::RC2StratifiedSolver* s = get_solver(self);
    if (s == nullptr) return nullptr;
    try {
        return PyLong_FromLong(s->get_cost());
    } catch (const std::runtime_error& e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
        return nullptr;
    }
}

static PyObject* m_val(PyRC2Stratified* self, PyObject* args) {
    rc2::RC2StratifiedSolver* s = get_solver(self);
    if (s == nullptr) return nullptr;
    long lit = 0;
    if (!PyArg_ParseTuple(args, "l", &lit)) return nullptr;
    try {
        return PyLong_FromLong(s->val(static_cast<int>(lit)));
    } catch (const std::runtime_error& e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
        return nullptr;
    }
}

static PyObject* m_get_model(PyRC2Stratified* self, PyObject* args) {
    (void)args;
    rc2::RC2StratifiedSolver* s = get_solver(self);
    if (s == nullptr) return nullptr;
    try {
        std::vector<int> m = s->get_model();
        PyObject* out = PyList_New(0);
        if (out == nullptr) return nullptr;
        for (int l : m) {
            PyObject* o = PyLong_FromLong(l);
            if (o == nullptr || PyList_Append(out, o) < 0) {
                Py_XDECREF(o);
                Py_DECREF(out);
                return nullptr;
            }
            Py_DECREF(o);
        }
        return out;
    } catch (const std::runtime_error& e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
        return nullptr;
    }
}

static PyObject* m_signature(PyRC2Stratified* self, PyObject* args) {
    (void)args;
    rc2::RC2StratifiedSolver* s = get_solver(self);
    if (s == nullptr) return nullptr;
    return PyUnicode_FromString(s->signature().c_str());
}

static PyObject* m_close(PyRC2Stratified* self, PyObject* args) {
    (void)args;
    rc2::RC2StratifiedSolver* s = get_solver(self);
    if (s == nullptr) return nullptr;
    s->close();
    Py_RETURN_NONE;
}

static PyMethodDef RC2_methods[] = {
    {"add_clause", (PyCFunction)(void(*)(void))m_add_clause, METH_VARARGS | METH_KEYWORDS, "Add hard/soft clause."},
    {"set_soft", (PyCFunction)(void(*)(void))m_set_soft, METH_VARARGS | METH_KEYWORDS, "Set soft literal."},
    {"add_soft_unit", (PyCFunction)(void(*)(void))m_add_soft_unit, METH_VARARGS | METH_KEYWORDS, "Add soft unit."},
    {"solve", (PyCFunction)(void(*)(void))m_solve, METH_VARARGS | METH_KEYWORDS, "Solve incremental query."},
    {"request_stop", (PyCFunction)m_request_stop, METH_VARARGS, "Request solver stop/interruption."},
    {"clear_stop", (PyCFunction)m_clear_stop, METH_VARARGS, "Clear solver stop/interruption request."},
    {"get_status", (PyCFunction)m_get_status, METH_VARARGS, "Get status."},
    {"get_cost", (PyCFunction)m_get_cost, METH_VARARGS, "Get objective value."},
    {"val", (PyCFunction)m_val, METH_VARARGS, "Get literal value."},
    {"get_model", (PyCFunction)m_get_model, METH_VARARGS, "Get model."},
    {"signature", (PyCFunction)m_signature, METH_VARARGS, "Get signature."},
    {"close", (PyCFunction)m_close, METH_VARARGS, "Close solver."},
    {NULL, NULL, 0, NULL}
};

static PyTypeObject RC2Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
};

static struct PyModuleDef module_def = {
    PyModuleDef_HEAD_INIT,
    "rc2_ren_soft_cpp_native",
    "Pure C++ RC2Stratified module.",
    -1,
    NULL
};

PyMODINIT_FUNC PyInit_rc2_ren_soft_cpp_native(void) {
    RC2Type.tp_name = "rc2_ren_soft_cpp_native.RC2Stratified";
    RC2Type.tp_basicsize = sizeof(PyRC2Stratified);
    RC2Type.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE;
    RC2Type.tp_doc = "Pure C++ RC2Stratified";
    RC2Type.tp_methods = RC2_methods;
    RC2Type.tp_init = (initproc)PyRC2_init;
    RC2Type.tp_dealloc = (destructor)PyRC2_dealloc;
    RC2Type.tp_new = PyType_GenericNew;
    if (PyType_Ready(&RC2Type) < 0) return NULL;

    PyObject* m = PyModule_Create(&module_def);
    if (m == NULL) return NULL;
    Py_INCREF(&RC2Type);
    if (PyModule_AddObject(m, "RC2Stratified", (PyObject*)&RC2Type) < 0) {
        Py_DECREF(&RC2Type);
        Py_DECREF(m);
        return NULL;
    }
    return m;
}
