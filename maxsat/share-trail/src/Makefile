GITV = $(shell git describe --always --dirty)
CC = g++ 
ARFLAGS ?= rcs
ENABLE_LTO ?= 0
ENABLE_MARCH_NATIVE ?= 1
PROFILE_BUILD ?= 0
JEMALLOC_ROOT ?= ../third_party/jemalloc
JEMALLOC_LIB ?= $(JEMALLOC_ROOT)/lib/libjemalloc.a

BASE_CXXFLAGS = -O3 -DNDEBUG -std=c++17 -fvisibility=hidden -ffunction-sections -fdata-sections
PERF_CXXFLAGS = -fno-semantic-interposition -fomit-frame-pointer -ffp-contract=fast

CFLAGS += $(BASE_CXXFLAGS) $(PERF_CXXFLAGS) -Wall -Wextra -Wshadow -Wfatal-errors -Lsolvers/lib/ -DGIT_IDENTIFIER=\"$(GITV)\" -fPIC -I$(JEMALLOC_ROOT)/include

ifeq ($(PROFILE_BUILD),1)
  # Better perf profiling / flamegraphs: preserve frame pointers and symbols.
  CFLAGS += -g3 -fno-omit-frame-pointer
  CFLAGS := $(filter-out -fomit-frame-pointer,$(CFLAGS))
endif

ifeq ($(ENABLE_LTO),1)
  CFLAGS += -flto=auto
endif

ifeq ($(ENABLE_MARCH_NATIVE),1)
  CFLAGS += -march=native
endif

JEMALLOC_LINK_ARGS = $(JEMALLOC_LIB) -lpthread -ldl
OBJFILES = cgss2.o options.o ss_encoder.o satsolver.o clausebuffer.o optimizer.o ordered_objectives.o
LIBS =
STATICLIBS = -static
LIBFILES =
LIBOBJFILES =


with_zlib = 'false'
with_gurobi = 'false'
with_cplex = 'false'
with_scip = 'false'
with_cadical = 'false'
with_glucose3 = 'false'
with_glucose3x = 'false'
with_glucose41 = 'true'
with_maxpre2 = 'false'

ifeq ($(with_zlib),'true')
CFLAGS += -DZLIB
LIBS +=  -lboost_iostreams
STATICLIBS +=  -l:libboost_iostreams.a -l:libz.a -l:liblzma.a
endif

ifeq ($(with_gurobi),'true')
CFLAGS += -DWITH_GUROBI
OBJFILES += optimizers/gurobi/gurobi_optimizer.o
LIBFILES +=
LIBS += -lgurobi_c++ -lgurobi100
STATICLIBS += -l:libgurobi_c++.a -l:libgurobi100.a
#LIBS += -lgurobi_c++ -lgurobi95
solver: gurobi
ipamir: gurobi
lib: gurobi
clean-all: clean-gurobi
endif

# cplex
ifeq ($(with_cplex),'true')
CFLAGS += -DWITH_CPLEX -DIL_STD
OBJFILES += optimizers/cplex/cplex_optimizer.o
LIBFILES +=
LIBS +=  -lconcert -lilocplex -lcplex -lpthread -ldl
STATICLIBS +=  -l:libconcert.a -l:libilocplex.a -l:libcplex.a -l:libpthread.a -l:libdl.a
solver: cplex
ipamir: cplex
lib: cplex
clean-all: clean-cplex
endif

ifeq ($(with_scip),'true')
CFLAGS += -DWITH_SCIP 
OBJFILES += optimizers/scip/scip_optimizer.o
LIBFILES +=
LIBS += -lscipbase -llpispx2 -ltpinone -lsoplex
STATICLIBS += -l:libscipbase.a -l:liblpispx2.a -l:libtpinone.a -l:libsoplex.a
solver: scip
ipamir: scip
lib: scip
clean-all: clean-scip
endif


# cadical
ifeq ($(with_cadical),'true')
CFLAGS += -DCADICAL
OBJFILES += solvers/cadical/cadical_solver.o
LIBFILES +=  libcadical.a
LIBOBJFILES += ../cadical/build/*.o
LIBS += -lcadical
STATICLIBS += -l:libcadical.a
solver: cadical
ipamir: cadical
lib: cadical
clean-all: clean-cadical
endif

# glucose
ifeq ($(with_glucose3),'true')
CFLAGS += -DGLUCOSE3
OBJFILES += solvers/glucose3/glucose3_solver.o
LIBFILES += libglucose3.a
LIBOBJFILES += solvers/glucose3/core/Solver.o  solvers/glucose3/utils/System.o
LIBS += -lglucose3
STATICLIBS += -l:libglucose3.a
solver: glucose3
ipamir: glucose3
lib: glucose3
clean-all: clean-glucose3
endif

# glucose 3x
ifeq ($(with_glucose3x),'true')
CFLAGS += -DGLUCOSE3X
OBJFILES += solvers/glucose3x/glucose3x_solver.o
LIBFILES += libglucose3x.a
LIBOBJFILES += solvers/glucose3x/core/Solver.o  solvers/glucose3x/utils/System.o
LIBS += -lglucose3x
STATICLIBS += -l:libglucose3x.a
solver: glucose3x
ipamir: glucose3x
lib: glucose3x
clean-all: clean-glucose3x
endif

# glucose syrup 4.1
ifeq ($(with_glucose41),'true')
CFLAGS += -DGLUCOSE41 -DGlucose=Glucose41CGSS2 -Isolvers/glucose-syrup-4.1
OBJFILES += solvers/glucose-syrup-4.1/glucose41_solver.o
LIBFILES += libglucose41.a
LIBOBJFILES += solvers/glucose-syrup-4.1/core/Solver.o solvers/glucose-syrup-4.1/utils/System.o solvers/glucose-syrup-4.1/utils/Options.o
LIBS += -lglucose41
STATICLIBS += -l:libglucose41.a
solver: glucose41
ipamir: glucose41
lib: glucose41
clean-all: clean-glucose41
endif


# MAXPRE2
ifeq ($(with_maxpre2),'true')
CFLAGS += -DMAXPRE  -L../maxpre2/src/lib/ -I../maxpre2/src
LIBFILES += libmaxpre.a
LIBS += -lmaxpre
STATICLIBS += -l:libmaxpre.a 
solver: maxpre
lib: maxpre
clean-all: clean-maxpre
endif

all: solver

$(JEMALLOC_LIB):
	@test -d "$(JEMALLOC_ROOT)" || (echo "Missing vendored jemalloc at $(JEMALLOC_ROOT)" >&2; exit 2)
	@test -x "$(JEMALLOC_ROOT)/configure" || (echo "Missing $(JEMALLOC_ROOT)/configure (git clone lacks generated configure). Use jemalloc release tarball or install autoconf and run ./autogen.sh once." >&2; exit 2)
	@cd "$(JEMALLOC_ROOT)" && \
		{ test -f Makefile || ./configure --disable-shared --enable-static; } && \
		$(MAKE) -j4
	@test -f "$(JEMALLOC_LIB)" || (echo "jemalloc build failed: $(JEMALLOC_LIB) not found" >&2; exit 2)

gurobi:

cplex:

scip:

solver: $(JEMALLOC_LIB) main.o  interface.o dimacs.o $(OBJFILES)
	$(CC) $(CFLAGS) main.o interface.o dimacs.o $(OBJFILES) $(LIBS) $(JEMALLOC_LINK_ARGS) -o cgss2

static: $(JEMALLOC_LIB) main.o  interface.o dimacs.o $(OBJFILES)
	$(CC) $(CFLAGS) main.o interface.o dimacs.o $(OBJFILES) $(STATICLIBS) $(JEMALLOC_LINK_ARGS) -o cgss2

.PHONY: lib

lib: interface.o dimacs.o $(OBJFILES)
	rm -f lib/libcgss2.a
	ar $(ARFLAGS) lib/libcgss2.a interface.o dimacs.o $(OBJFILES) $(LIBOBJFILES)

sipamir: ipamir
	ar

ipamir: ipamir.o $(OBJFILES)
	mkdir -p lib
	rm -f lib/libipamircgss2.a
	ar $(ARFLAGS) lib/libipamircgss2.a ipamir.o $(OBJFILES) $(LIBOBJFILES)

glucose3:
	$(MAKE) -C solvers/glucose3 lib
	cp solvers/lib/libglucose3.a lib/libglucose3.a

cadical:
	$(MAKE) -C ../cadical
	cp ../cadical/build/libcadical.a solvers/lib/libcadical.a
	cp ../cadical/build/libcadical.a lib/libcadical.a

glucose3x:
	$(MAKE) -C solvers/glucose3x lib
	cp solvers/lib/libglucose3x.a lib/libglucose3x.a

glucose41:
	$(MAKE) -C solvers/glucose-syrup-4.1 lib
	mkdir -p lib
	cp solvers/lib/libglucose41.a lib/libglucose41.a

maxpre:
	$(MAKE) -C ../maxpre2 lib
	mkdir -p lib
	cp ../maxpre2/src/lib/libmaxpre.a lib/libmaxpre.a


%.o: %.cpp
	$(CC) $(CFLAGS) -MMD -c $< -o $@

-include *.d


clean-all: clean

clean: clean-cgss2

clean-cgss2:
	rm -f lib/*.a
	rm -f *.o *.d
	rm -f solvers/glucose-syrup-4.1/*.o solvers/glucose-syrup-4.1/*.d
	rm -f cgss2
	rm -f ../cgss2
	
clean-gurobi:
	rm -f optimizers/gurobi/*.o
	rm -f optimizers/gurobi/*.d

clean-cplex:
	rm -f optimizers/cplex/*.o
	rm -f optimizers/cplex/*.d
	
clean-scip:
	rm -f optimizers/cplex/*.o
	rm -f optimizers/cplex/*.d

clean-glucose3:
	$(MAKE) -C solvers/glucose3 clean

clean-glucose3x:
	$(MAKE) -C solvers/glucose3x clean

clean-glucose41:
	$(MAKE) -C solvers/glucose-syrup-4.1 clean

clean-cadical:
	$(MAKE) -C ../cadical clean
	cd ../cadical && ./configure

clean-maxpre:
	$(MAKE) -C ../maxpre2 clean
