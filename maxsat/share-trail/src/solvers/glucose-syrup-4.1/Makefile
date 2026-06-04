CC = g++
CFLAGS = -std=c++17 -O2 -Wall -Wextra -Wshadow -Wfatal-errors -I. -DINCREMENTAL -DINCREMNENTAL -DGlucose=Glucose41CGSS2
OBJFILES = utils/System.o utils/Options.o

.PHONY: lib

lib: core/Solver.o $(OBJFILES)
	mkdir -p ../lib
	ar rvs ../lib/libglucose41.a core/Solver.o $(OBJFILES)

%.o: %.cc
	$(CC) $(CFLAGS) -MMD -c $< -o $@

clean:
	rm -f ../lib/libglucose41.a
	rm -f *.o *.d
	rm -f core/*.o core/*.d
	rm -f utils/*.o utils/*.d
