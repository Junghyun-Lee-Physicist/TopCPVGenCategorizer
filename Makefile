# =============================================================================
#  Makefile for TopCPVGenCategorizer (NanoAOD GenPar replacement)
#  Standalone build — no dependency on correctionlib or boost.
# =============================================================================

PROGRAM = TopCPVGenCategorizer

CC = g++
LD = g++

ROOTCFLAGS = $(shell root-config --cflags)
ROOTLIBS   = $(shell root-config --libs)
ROOTGLIBS  = $(shell root-config --glibs)

CXXFLAGS = -g -O2 -std=c++17 $(ROOTCFLAGS) -Wall -Wno-unused-variable
LDFLAGS  = $(ROOTLIBS)
GLIBS    = $(ROOTGLIBS)

OBJS = \
    TopCPVGenCategorizer.o \
    main_gencat.o

INPUTS = \
    ./interface/TopCPVGenCategorizer.h \
    ./src/TopCPVGenCategorizer.cpp \
    main_gencat.cpp

all: $(OBJS)
	$(CC) -o $(PROGRAM) -g $(OBJS) $(LDFLAGS)

main_gencat.o: main_gencat.cpp $(INPUTS)
	$(CC) $(CXXFLAGS) -c main_gencat.cpp

TopCPVGenCategorizer.o: ./interface/TopCPVGenCategorizer.h ./src/TopCPVGenCategorizer.cpp
	$(CC) $(CXXFLAGS) -c ./src/TopCPVGenCategorizer.cpp -o TopCPVGenCategorizer.o

clean:
	-rm -f *~ $(OBJS) core core.*
	-rm -f $(PROGRAM)

distclean: clean
	-rm -f *~ $(PROGRAM) $(OBJS)

run: $(PROGRAM)
	./$(PROGRAM)

