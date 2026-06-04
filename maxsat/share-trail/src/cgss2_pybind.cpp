#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "cgss2.h"
#include "options.h"

using namespace cgss2;

typedef struct {
  PyObject_HEAD
  CGSS2* solver;
} PyCGSS2Object;

static int pyseq_to_int_vector(PyObject* seq_obj, std::vector<int>& out) {
  PyObject* seq = PySequence_Fast(seq_obj, "Expected a sequence of integers");
  if (!seq) return -1;
  Py_ssize_t n = PySequence_Fast_GET_SIZE(seq);
  PyObject** items = PySequence_Fast_ITEMS(seq);
  out.clear();
  out.reserve(static_cast<size_t>(n));
  for (Py_ssize_t i = 0; i < n; ++i) {
    long v = PyLong_AsLong(items[i]);
    if (PyErr_Occurred()) {
      Py_DECREF(seq);
      PyErr_SetString(PyExc_TypeError, "Clause/assumption literals must be integers");
      return -1;
    }
    out.push_back(static_cast<int>(v));
  }
  Py_DECREF(seq);
  return 0;
}

static int get_kw_bool(PyObject* kwds, const char* key, bool* out, bool cur) {
  *out = cur;
  if (!kwds) return 0;
  PyObject* v = PyDict_GetItemString(kwds, key);
  if (!v) return 0;
  int b = PyObject_IsTrue(v);
  if (b < 0) return -1;
  *out = (b != 0);
  return 0;
}

static int get_kw_int(PyObject* kwds, const char* key, int* out, int cur) {
  *out = cur;
  if (!kwds) return 0;
  PyObject* v = PyDict_GetItemString(kwds, key);
  if (!v) return 0;
  long x = PyLong_AsLong(v);
  if (PyErr_Occurred()) {
    PyErr_Format(PyExc_TypeError, "Keyword '%s' must be an integer", key);
    return -1;
  }
  *out = static_cast<int>(x);
  return 0;
}

static int validate_kwargs(PyObject* kwds) {
  if (!kwds) return 0;
  static const std::set<std::string> valid_keys = {
      "verbose",       "am1s",       "strat",   "blo",      "wce",
      "fc",            "abst_cg",    "hardening", "minimize", "trim",
      "exhaust",       "greedy_costs", "use_ub", "relax_threshold"};

  PyObject* key = nullptr;
  PyObject* value = nullptr;
  Py_ssize_t pos = 0;
  while (PyDict_Next(kwds, &pos, &key, &value)) {
    if (!PyUnicode_Check(key)) {
      PyErr_SetString(PyExc_TypeError, "Keyword arguments must be strings");
      return -1;
    }
    const char* k = PyUnicode_AsUTF8(key);
    if (!k) return -1;
    if (valid_keys.find(k) == valid_keys.end()) {
      PyErr_Format(PyExc_ValueError, "Unknown keyword argument: %s", k);
      return -1;
    }
  }
  return 0;
}

static int PyCGSS2_init(PyCGSS2Object* self, PyObject* args, PyObject* kwds) {
  if (validate_kwargs(kwds) < 0) return -1;

  PyObject* wcnf = nullptr;
  if (!PyArg_ParseTuple(args, "O", &wcnf)) {
    PyErr_SetString(PyExc_TypeError, "CGSS2 expects one positional argument: wcnf");
    return -1;
  }

  try {
    self->solver = new CGSS2();
  } catch (...) {
    PyErr_SetString(PyExc_MemoryError, "Failed to allocate solver");
    return -1;
  }

  Options op;
  CGSS2::add_options(op);
  op.prepare(std::cerr);
  if (op.strs["satsolver"] == "") op.strs["satsolver"] = "glucose41";

  int verbose = 0;
  if (get_kw_int(kwds, "verbose", &verbose, 0) < 0) return -1;
  Log log(std::cerr, verbose);
  self->solver->set_log(log);

  self->solver->init(op);
  self->solver->make_incremental();

  bool btmp = false;
  int itmp = 0;

  if (get_kw_bool(kwds, "am1s", &btmp, self->solver->am1s) < 0) return -1;
  self->solver->am1s = btmp;
  if (get_kw_bool(kwds, "strat", &btmp, self->solver->strat) < 0) return -1;
  self->solver->strat = btmp;
  if (get_kw_bool(kwds, "blo", &btmp, self->solver->blo) < 0) return -1;
  self->solver->blo = btmp;
  if (get_kw_bool(kwds, "wce", &btmp, self->solver->WCE) < 0) return -1;
  self->solver->WCE = btmp;
  if (get_kw_bool(kwds, "fc", &btmp, self->solver->FC) < 0) return -1;
  self->solver->FC = btmp;
  if (get_kw_int(kwds, "abst_cg", &itmp, self->solver->AbstCG) < 0) return -1;
  self->solver->AbstCG = itmp;
  if (get_kw_int(kwds, "hardening", &itmp, self->solver->hardening) < 0) return -1;
  self->solver->hardening = itmp;
  if (get_kw_int(kwds, "minimize", &itmp, self->solver->minimize) < 0) return -1;
  self->solver->minimize = itmp;
  if (get_kw_int(kwds, "trim", &itmp, self->solver->trim) < 0) return -1;
  self->solver->trim = itmp;
  if (get_kw_bool(kwds, "exhaust", &btmp, self->solver->exhaust) < 0) return -1;
  self->solver->exhaust = btmp;
  if (get_kw_bool(kwds, "greedy_costs", &btmp, self->solver->greedy_costs) < 0) return -1;
  self->solver->greedy_costs = btmp;
  if (get_kw_bool(kwds, "use_ub", &btmp, self->solver->use_ub) < 0) return -1;
  self->solver->use_ub = btmp;
  if (get_kw_int(kwds, "relax_threshold", &itmp, self->solver->core_relax_threshold) < 0) return -1;
  self->solver->core_relax_threshold = itmp;

  PyObject* nv_obj = PyObject_GetAttrString(wcnf, "nv");
  if (!nv_obj) return -1;
  long nv = PyLong_AsLong(nv_obj);
  Py_DECREF(nv_obj);
  if (PyErr_Occurred() || nv < 0) {
    PyErr_SetString(PyExc_ValueError, "wcnf.nv must be a non-negative integer");
    return -1;
  }
  self->solver->set_nof_vars(static_cast<int>(nv));
  for (int v = 1; v <= nv; ++v) self->solver->lit_to_solver(posLit(v), false);

  PyObject* hard = PyObject_GetAttrString(wcnf, "hard");
  PyObject* soft = PyObject_GetAttrString(wcnf, "soft");
  PyObject* wght = PyObject_GetAttrString(wcnf, "wght");
  if (!hard || !soft || !wght) {
    Py_XDECREF(hard);
    Py_XDECREF(soft);
    Py_XDECREF(wght);
    PyErr_SetString(PyExc_AttributeError, "wcnf must expose attributes: hard, soft, wght");
    return -1;
  }

  PyObject* hard_seq = PySequence_Fast(hard, "wcnf.hard must be a sequence");
  PyObject* soft_seq = PySequence_Fast(soft, "wcnf.soft must be a sequence");
  PyObject* wght_seq = PySequence_Fast(wght, "wcnf.wght must be a sequence");
  Py_DECREF(hard);
  Py_DECREF(soft);
  Py_DECREF(wght);
  if (!hard_seq || !soft_seq || !wght_seq) {
    Py_XDECREF(hard_seq);
    Py_XDECREF(soft_seq);
    Py_XDECREF(wght_seq);
    return -1;
  }

  Py_ssize_t hsz = PySequence_Fast_GET_SIZE(hard_seq);
  Py_ssize_t ssz = PySequence_Fast_GET_SIZE(soft_seq);
  Py_ssize_t wsz = PySequence_Fast_GET_SIZE(wght_seq);
  if (ssz != wsz) {
    Py_DECREF(hard_seq);
    Py_DECREF(soft_seq);
    Py_DECREF(wght_seq);
    PyErr_SetString(PyExc_ValueError, "wcnf.soft and wcnf.wght must have equal length");
    return -1;
  }

  std::vector<int> clause;
  for (Py_ssize_t i = 0; i < hsz; ++i) {
    if (pyseq_to_int_vector(PySequence_Fast_GET_ITEM(hard_seq, i), clause) < 0) {
      Py_DECREF(hard_seq);
      Py_DECREF(soft_seq);
      Py_DECREF(wght_seq);
      return -1;
    }
    self->solver->add_clause(clause, HARDWEIGHT, true);
  }

  for (Py_ssize_t i = 0; i < ssz; ++i) {
    if (pyseq_to_int_vector(PySequence_Fast_GET_ITEM(soft_seq, i), clause) < 0) {
      Py_DECREF(hard_seq);
      Py_DECREF(soft_seq);
      Py_DECREF(wght_seq);
      return -1;
    }
    unsigned long long w = PyLong_AsUnsignedLongLong(PySequence_Fast_GET_ITEM(wght_seq, i));
    if (PyErr_Occurred()) {
      Py_DECREF(hard_seq);
      Py_DECREF(soft_seq);
      Py_DECREF(wght_seq);
      PyErr_SetString(PyExc_TypeError, "wcnf.wght must contain non-negative integers");
      return -1;
    }
    self->solver->add_clause(clause, static_cast<uint64_t>(w), true);
  }

  Py_DECREF(hard_seq);
  Py_DECREF(soft_seq);
  Py_DECREF(wght_seq);
  return 0;
}

static void PyCGSS2_dealloc(PyCGSS2Object* self) {
  delete self->solver;
  self->solver = nullptr;
  Py_TYPE(self)->tp_free(reinterpret_cast<PyObject*>(self));
}

static PyObject* PyCGSS2_solve(PyCGSS2Object* self, PyObject* args, PyObject* kwargs) {
  static const char* kwlist[] = {"assumptions", nullptr};
  PyObject* assumptions = Py_None;
  if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|O", const_cast<char**>(kwlist), &assumptions)) {
    return nullptr;
  }

  if (assumptions != Py_None) {
    PyObject* seq = PySequence_Fast(assumptions, "assumptions must be a sequence of integers or None");
    if (!seq) return nullptr;
    Py_ssize_t n = PySequence_Fast_GET_SIZE(seq);
    PyObject** items = PySequence_Fast_ITEMS(seq);
    for (Py_ssize_t i = 0; i < n; ++i) {
      long l = PyLong_AsLong(items[i]);
      if (PyErr_Occurred()) {
        Py_DECREF(seq);
        PyErr_SetString(PyExc_TypeError, "assumptions must contain integers");
        return nullptr;
      }
      self->solver->ipamir_assume(static_cast<int>(l));
    }
    Py_DECREF(seq);
  }

  int32_t status = self->solver->ipamir_solve();
  if (status == 30) Py_RETURN_TRUE;
  if (status == 20) Py_RETURN_FALSE;
  PyErr_SetString(PyExc_RuntimeError, "CGSS2 returned UNKNOWN/abnormal status");
  return nullptr;
}

static PyObject* PyCGSS2_get_cost(PyCGSS2Object* self, PyObject*) {
  uint64_t c = self->solver->ipamir_val_obj();
  return PyLong_FromUnsignedLongLong(static_cast<unsigned long long>(c));
}

static PyObject* PyCGSS2_get_model(PyCGSS2Object* self, PyObject*) {
  std::vector<int> model;
  self->solver->get_model(model, true);
  PyObject* out = PyList_New(static_cast<Py_ssize_t>(model.size()));
  if (!out) return nullptr;
  for (Py_ssize_t i = 0; i < static_cast<Py_ssize_t>(model.size()); ++i) {
    PyObject* v = PyLong_FromLong(model[static_cast<size_t>(i)]);
    if (!v) {
      Py_DECREF(out);
      return nullptr;
    }
    PyList_SET_ITEM(out, i, v);
  }
  return out;
}

static PyObject* PyCGSS2_set_soft(PyCGSS2Object* self, PyObject* args) {
  int lit = 0;
  unsigned long long weight = 0;
  if (!PyArg_ParseTuple(args, "iK", &lit, &weight)) return nullptr;
  self->solver->ipamir_add_soft_lit(-lit, static_cast<uint64_t>(weight));
  Py_RETURN_NONE;
}

static PyObject* PyCGSS2_add_clause(PyCGSS2Object* self, PyObject* args, PyObject* kwargs) {
  static const char* kwlist[] = {"clause", "weight", nullptr};
  PyObject* clause_obj = nullptr;
  PyObject* weight_obj = Py_None;
  if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|O", const_cast<char**>(kwlist), &clause_obj, &weight_obj)) {
    return nullptr;
  }

  std::vector<int> clause;
  if (pyseq_to_int_vector(clause_obj, clause) < 0) return nullptr;

  uint64_t w = HARDWEIGHT;
  if (weight_obj != Py_None) {
    unsigned long long x = PyLong_AsUnsignedLongLong(weight_obj);
    if (PyErr_Occurred()) {
      PyErr_SetString(PyExc_TypeError, "weight must be a non-negative integer or None");
      return nullptr;
    }
    w = static_cast<uint64_t>(x);
  }

  self->solver->add_clause(clause, w, true);
  Py_RETURN_NONE;
}

static PyMethodDef PyCGSS2_methods[] = {
    {"solve", reinterpret_cast<PyCFunction>(PyCGSS2_solve), METH_VARARGS | METH_KEYWORDS, "Solve with optional assumptions"},
    {"get_model", reinterpret_cast<PyCFunction>(PyCGSS2_get_model), METH_NOARGS, "Get model"},
    {"get_cost", reinterpret_cast<PyCFunction>(PyCGSS2_get_cost), METH_NOARGS, "Get objective cost"},
    {"set_soft", reinterpret_cast<PyCFunction>(PyCGSS2_set_soft), METH_VARARGS, "Set soft literal weight"},
    {"add_clause", reinterpret_cast<PyCFunction>(PyCGSS2_add_clause), METH_VARARGS | METH_KEYWORDS, "Add hard/soft clause"},
    {nullptr, nullptr, 0, nullptr}};

static PyTypeObject PyCGSS2Type = {
    PyVarObject_HEAD_INIT(nullptr, 0)
    .tp_name = "cgss2_native.CGSS2",
    .tp_basicsize = sizeof(PyCGSS2Object),
    .tp_itemsize = 0,
    .tp_dealloc = reinterpret_cast<destructor>(PyCGSS2_dealloc),
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "Raw CPython CGSS2 wrapper",
    .tp_methods = PyCGSS2_methods,
    .tp_init = reinterpret_cast<initproc>(PyCGSS2_init),
    .tp_new = PyType_GenericNew,
};

static PyModuleDef cgss2_native_module = {
    PyModuleDef_HEAD_INIT,
    "cgss2_native",
    "Raw CPython wrapper for CGSS2",
    -1,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr};

PyMODINIT_FUNC PyInit_cgss2_native(void) {
  if (PyType_Ready(&PyCGSS2Type) < 0) return nullptr;
  PyObject* m = PyModule_Create(&cgss2_native_module);
  if (!m) return nullptr;
  Py_INCREF(&PyCGSS2Type);
  if (PyModule_AddObject(m, "CGSS2", reinterpret_cast<PyObject*>(&PyCGSS2Type)) < 0) {
    Py_DECREF(&PyCGSS2Type);
    Py_DECREF(m);
    return nullptr;
  }
  return m;
}
