TARGET=$(shell basename "`pwd`")

CC=gcc
CFLAGS=-std=c++0x -Wall -D __STDC_LIMIT_MACROS -D __STDC_FORMAT_MACROS -O3 -pthread -g #-DNDEBUG
CXX=g++

IPAMIRSOLVER ?= solver2022

INCLUDE=-Iminisat -Iminisat/minisat/core -Iminisat/minisat/mtl -Iminisat/minisat/simp -Iquantom -Iaiger -Ipacose -Idepqbf -Iaigqbf -I../../maxsat/$(IPAMIRSOLVER)

LIBS = -L../../maxsat/$(IPAMIRSOLVER)/ -lipamir$(IPAMIRSOLVER)
LIBS += $(shell cat ../../maxsat/$(IPAMIRSOLVER)/LIBS 2>/dev/null)
LIBS += $(shell cat ../../maxsat/$(IPASIRSOLVER)/LIBS 2>/dev/null)

all:	$(TARGET)

$(TARGET):	minisat/build/release/lib/libminisat.a aiger/aiger.o Model.o IC3.o PoGeneralizer.o main.o
	$(CXX) -o $(TARGET) \
		aiger.o Model.o IC3.o PoGeneralizer.o main.o \
		minisat/build/release/lib/libminisat.a \
		quantom/libquantom.a \
		depqbf/libqdpll.a \
		pacose/libPacose.a \
		aigqbf/libaigsolve-cpp-all.a \
		../../maxsat/$(IPAMIRSOLVER)/libipamir$(IPAMIRSOLVER).a $(LIBS)

minisat/build/release/lib/libminisat.a:
	make -C minisat lr

.c.o:
	$(CC) -g -O3 $(INCLUDE) $< -c

.cpp.o:
	$(CXX) $(CFLAGS) $(INCLUDE) $< -c

clean:
	rm -f *.o $(TARGET)

dist:
	cd ..; tar cf ic3ref/IC3ref.tar ic3ref/*.h ic3ref/*.cpp ic3ref/Makefile ic3ref/LICENSE ic3ref/README; gzip ic3ref/IC3ref.tar
