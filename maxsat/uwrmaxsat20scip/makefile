#-----------------------------------------------------------------------#
#- GLOBAL DEFS ---------------------------------------------------------#
#-----------------------------------------------------------------------#

# Keep this as generic as possible.

NAME=uwrmaxsat
SCIP=scipoptsuite-10.0.2
SATSOLVER=cadical

# NOTE: requires GNU make extension '$(shell ...)'.

VERSION=20

#-----------------------------------------------------------------------#
# Solver signatures have to be both valid file names and C symbols.

SIG=$(NAME)$(VERSION)
DIR=$(NAME)-$(VERSION)
TARGET=libipamir$(SIG)scip.a

#-----------------------------------------------------------------------#

CC=gcc
CFLAGS=-Wall -DNDEBUG -O3 -std=c++14

#-----------------------------------------------------------------------#
#- REQUIRED TOP RULES --------------------------------------------------#
#-----------------------------------------------------------------------#

all: $(TARGET)

clean:
	rm -rf cadical/build
	make -C $(NAME) clean
	rm -rf $(SCIP)/build
	rm -f *.o *.a

#-----------------------------------------------------------------------#
#- INVISIBLE INTERNAL SUB RULES ----------------------------------------#
#-----------------------------------------------------------------------#

cominisatps/core:
	cd cominisatps ; \
        rm core simp mtl utils 2>/dev/null ; \
        ln -s minisat/core minisat/simp minisat/mtl minisat/utils .


$(SCIP):
	tar zxf $(SCIP).tgz

$(SCIP)/build/lib/libscip.a: $(SCIP)
	cd $(SCIP); \
	mkdir build; cd build; \
	cmake -DSYM=snauty -DSHARED=off -DNO_EXTERNAL_CODE=on -DSOPLEX=on -DGMP=on -DMPFR=on -DBOOST=on -DTPI=tny .. ; \
	cmake --build . --config Release --target libscip libsoplex-pic

$(SATSOLVER)/build/libcadical.a:
	cd $(SATSOLVER) && ./configure --quiet --no-contracts --no-tracing
	make -C $(SATSOLVER) cadical

$(TARGET): $(SCIP)/build/lib/libscip.a $(SATSOLVER)/build/libcadical.a $(NAME) cominisatps/core
	make -C $(NAME) distclean
	make -C $(NAME) config
	MAXPRE= make -C $(NAME) lr
	cp -p $(NAME)/build/release/lib/lib$(NAME).a $(TARGET)

#-----------------------------------------------------------------------#

.FORCE:
.PHONY: all clean

