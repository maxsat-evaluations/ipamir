#-----------------------------------------------------------------------#
#- GLOBAL DEFS ---------------------------------------------------------#
#-----------------------------------------------------------------------#

# Keep this as generic as possible.

NAME=uwrmaxsat
SATSOLVER=cadical

# NOTE: requires GNU make extension '$(shell ...)'.

VERSION=20

#-----------------------------------------------------------------------#
# Solver signatures have to be both valid file names and C symbols.

SIG=$(NAME)$(VERSION)
DIR=$(NAME)-$(VERSION)
TARGET=libipamir$(SIG).a

#-----------------------------------------------------------------------#

CC=g++
CFLAGS=-Wall -DNDEBUG -O3 -std=c++14

#-----------------------------------------------------------------------#
#- REQUIRED TOP RULES --------------------------------------------------#
#-----------------------------------------------------------------------#

all: $(TARGET)

clean:
	rm -rf cadical/build
	make -C $(NAME) clean
	rm -f *.o *.a

#-----------------------------------------------------------------------#
#- INVISIBLE INTERNAL SUB RULES ----------------------------------------#
#-----------------------------------------------------------------------#

cominisatps/core:
	cd cominisatps ; \
        rm core simp mtl utils 2>/dev/null ; \
        ln -s minisat/core minisat/simp minisat/mtl minisat/utils .

$(SATSOLVER)/build/libcadical.a:
	(cd $(SATSOLVER) && ./configure --quiet --no-contracts --no-tracing)
	make -C $(SATSOLVER) cadical

$(TARGET): $(SATSOLVER)/build/libcadical.a $(NAME) cominisatps/core
	make -C $(NAME) distclean
	make -C $(NAME) config
	USESCIP= MAXPRE= make -C $(NAME) lr
	cp -p $(NAME)/build/release/lib/lib$(NAME).a $(TARGET)

#-----------------------------------------------------------------------#

.FORCE:
.PHONY: all clean

