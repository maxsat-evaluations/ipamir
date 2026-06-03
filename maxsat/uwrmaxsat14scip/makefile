#-----------------------------------------------------------------------#
#- GLOBAL DEFS ---------------------------------------------------------#
#-----------------------------------------------------------------------#

# Keep this as generic as possible.

NAME=uwrmaxsat
SCIP=scipoptsuite-8.0.0

# NOTE: requires GNU make extension '$(shell ...)'.

VERSION=14

#-----------------------------------------------------------------------#
# Solver signatures have to be both valid file names and C symbols.

SIG=$(NAME)$(VERSION)
DIR=$(NAME)-$(VERSION)
TARGET=libipamir$(SIG)scip.a

#-----------------------------------------------------------------------#

CC=gcc
CFLAGS=-Wall -DNDEBUG -O3 -std=c11
CXXFLAGE=-std=c++11

#-----------------------------------------------------------------------#
#- REQUIRED TOP RULES --------------------------------------------------#
#-----------------------------------------------------------------------#

all: $(TARGET)

clean:
	make -C ../../sat/cominisatps clean
	rm -rf $(SCIP)
	rm -f *.o *.a

#-----------------------------------------------------------------------#
#- INVISIBLE INTERNAL SUB RULES ----------------------------------------#
#-----------------------------------------------------------------------#

IPASIRSOLVER	?= cominisatps

$(SCIP):
	tar zxf $(SCIP).tgz

$(SCIP)/build/lib/libscip.a: $(SCIP)
	cd $(SCIP); \
	sed -i "s/add_library(libscip/add_library(libscip STATIC/g" scip/src/CMakeLists.txt; \
	sed -i -e "/primes = base.primes/d" -e "/primes(base.primes)/d" soplex/src/soplex/datahashtable.h; \
	mkdir build; cd build; \
	cmake -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DNO_EXTERNAL_CODE=on -DSOPLEX=on -DTPI=tny .. ; \
	make libscip

../../sat/cominisatps/libipasircominisatps.a:
	make -C ../../sat/cominisatps

$(TARGET): $(SCIP)/build/lib/libscip.a ../../sat/cominisatps/libipasircominisatps.a $(NAME)
	make -C $(NAME) distclean
	make -C $(NAME) config
	MINISAT_INCLUDE=-I../../../sat/cominisatps/cominisatps MAXPRE= make -C $(NAME) lr
	cp -p $(NAME)/build/release/lib/lib$(NAME).a $(TARGET)

#-----------------------------------------------------------------------#

.FORCE:
.PHONY: all clean

