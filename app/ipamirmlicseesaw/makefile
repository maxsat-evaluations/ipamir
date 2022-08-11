#--------------------------------------------------------------------------#
# The target name should be the name of this app, which actually should be
# the same as the name of this directory.
#--------------------------------------------------------------------------#

TARGET=$(shell basename "`pwd`")

#--------------------------------------------------------------------------#
# The environment variable 'IPAMIRSOLVER' is used to locate the MaxSAT solver
# library from ../../maxsat/$(IPAMIRSOLVER). For testing purposes we set it to
# the non-incremental MaxSAT solver 'solver2022' which is compiled using
# 'minisat220' as the SAT solver.
#--------------------------------------------------------------------------#

IPAMIRSOLVER ?= maxhs

#--------------------------------------------------------------------------#
# Compiler flags and library dependencies.
#--------------------------------------------------------------------------#

CPLEXLIBDIR ?= /proj/andreasn/opt/ibm/ILOG/CPLEX_Studio221/cplex/lib/x86-64_linux/static_pic
CPLEXINCDIR ?= /proj/andreasn/opt/ibm/ILOG/CPLEX_Studio221/cplex/include

CC = g++
CFLAGS ?= -Wall -DNDEBUG -O3 -std=c++17
CFLAGS += -I$(CPLEXINCDIR)

DEPS = ../../maxsat/$(IPAMIRSOLVER)/libipamir$(IPAMIRSOLVER).a dataset.o cplex.o
LIBS = -L../../maxsat/$(IPAMIRSOLVER)/ -lipamir$(IPAMIRSOLVER)
LIBS += -L$(CPLEXLIBDIR) -lcplex -lpthread -ldl
LIBS += $(shell cat ../../maxsat/$(IPAMIRSOLVER)/LIBS 2>/dev/null)
LIBS += $(shell cat ../../sat/$(IPASIRSOLVER)/LIBS 2>/dev/null)
OBJS = dataset.o cplex.o
LINK=$(CC)

#--------------------------------------------------------------------------#
# Targets 'all' and 'clean'.
#--------------------------------------------------------------------------#

all: $(TARGET)

clean:
	rm -f $(TARGET) *.o
	rm -rf pblib

$(TARGET): $(TARGET).o $(DEPS)
	$(LINK) -o $@ $(TARGET).o $(OBJS) $(LIBS)

#--------------------------------------------------------------------------#
# Local app specific rules.
#--------------------------------------------------------------------------#

$(TARGET).o: $(TARGET).cpp dataset.hpp makefile
	$(CC) $(CFLAGS) -c $(TARGET).cpp

dataset.o: dataset.cpp dataset.hpp makefile
	$(CC) $(CFLAGS) -c dataset.cpp

cplex.o: cplex.cpp cplex.hpp makefile
	$(CC) $(CFLAGS) -c cplex.cpp

../../maxsat/$(IPAMIRSOLVER)/libipamir$(IPAMIRSOLVER).a:
	make -C ../../maxsat/$(IPAMIRSOLVER)
