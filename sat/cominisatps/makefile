#-----------------------------------------------------------------------#
#- GLOBAL DEFS ---------------------------------------------------------#
#-----------------------------------------------------------------------#

# Keep this as generic as possible.

NAME=cominisatps

# Avoid hardcoding the version number in this file.  This allows us to just
# drop a new distribution package into this directory without changing
# anything else. For other more complicated set-ups / solvers something like
# 'VERSION=960' should also work, if you have the appropriate tar file
# available, e.g., 'picosat-960.tar.gz', which extracts into 'picosat-960'.

#-----------------------------------------------------------------------#
# Solver signatures have to be both valid file names and C symbols.
# Since Picosat uses a dash '-' for the package name, we have to
# differentiate between with (directory / package name) and without
# dash (the signature).

SIG=$(NAME)$(VERSION)
DIR=$(NAME)$(VERSION)
TARGET=libipasir$(SIG).a

#-----------------------------------------------------------------------#

CXX=g++
CXXFLAGS=-Wall -DNDEBUG -O3

#-----------------------------------------------------------------------#
#- REQUIRED TOP RULES --------------------------------------------------#
#-----------------------------------------------------------------------#

all: $(TARGET)

clean:
	rm -rf $(DIR)
	rm -f *.o *.a

#-----------------------------------------------------------------------#
#- INVISIBLE INTERNAL SUB RULES ----------------------------------------#
#-----------------------------------------------------------------------#
cominisatps:
	@#
	@# extract library
	@#
	unzip COMiniSatPSChandrasekharDRUP.zip
	mv 'COMiniSatPS Chandrasekhar DRUP/cominisatps' $(DIR)
	rm -rf 'COMiniSatPS Chandrasekhar DRUP'
	@#
	@# apply patch 
	@#
	cd $(DIR); \
	patch -p1 <../$(NAME).patch
	find $(DIR) -name "*.h" -exec sed -i -e 's/namespace Minisat/namespace COMinisatPS/' -e 's/Minisat::/COMinisatPS::/g' {} \;
	find $(DIR) -name "*.cc" -exec sed -i -e 's/namespace Minisat/namespace COMinisatPS/' -e 's/Minisat::/COMinisatPS::/g' {} \;

libipasir$(SIG).a: cominisatps
	@#
	@# configure and build library
	@#
	MROOT=.. make -C $(DIR)/simp libr
	mkdir $(DIR)/minisat
	ln -s ../simp ../core ../utils ../mtl $(DIR)/minisat
	cp $(DIR)/simp/lib_release.a $(TARGET)

#-----------------------------------------------------------------------#

.FORCE:
.PHONY: all clean
