#-----------------------------------------------------------------------#
#- GLOBAL DEFS ---------------------------------------------------------#
#-----------------------------------------------------------------------#

# Keep this as generic as possible.

NAME=uwrmaxsat

# NOTE: requires GNU make extension '$(shell ...)'.

VERSION=14

#-----------------------------------------------------------------------#
# Solver signatures have to be both valid file names and C symbols.

SIG=$(NAME)$(VERSION)
DIR=$(NAME)-$(VERSION)
TARGET=libipamir$(SIG).a

#-----------------------------------------------------------------------#

CC=g++
CFLAGS=-Wall -DNDEBUG -O3 -std=c++11

#-----------------------------------------------------------------------#
#- REQUIRED TOP RULES --------------------------------------------------#
#-----------------------------------------------------------------------#

all: $(TARGET)

clean:
	make -C ../../sat/cominisatps clean
	rm -f *.o *.a

#-----------------------------------------------------------------------#
#- INVISIBLE INTERNAL SUB RULES ----------------------------------------#
#-----------------------------------------------------------------------#

#IPASIRSOLVER	?= cominisatps

../../sat/cominisatps/libipasircominisatps.a:
	make -C ../../sat/cominisatps

$(TARGET): ../../sat/cominisatps/libipasircominisatps.a
	make -C $(NAME) distclean
	make -C $(NAME) config
	MINISAT_INCLUDE=-I../../../sat/cominisatps/cominisatps USESCIP= MAXPRE= make -C $(NAME) lr
	cp -p $(NAME)/build/release/lib/lib$(NAME).a $(TARGET)

#-----------------------------------------------------------------------#

.FORCE:
.PHONY: all clean

