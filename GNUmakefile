ifeq ($(strip $(COMPILER)),clang)
CC=/usr/bin/clang++-4.0
else
CC=/usr/bin/g++-9
endif

ifeq ($(strip $(RELEASE)),)
DEBUGFLAGS=-ggdb -g3 -O0
else
DEBUGFLAGS=-O3
endif

BUILDDIR=build
SRCDIR=src
TESTDIR=tests
CXXFLAGS=-c -fPIC -std=c++17 -fPIC -Wall -Wshadow -pedantic -Wfatal-errors -fvisibility=hidden -pthread $(DEBUGFLAGS)
INCFLAGS="-I$(SRCDIR)"
LDFLAGS=-g
LDLIBS=-lm -lstdc++
LIBOBJS=$(BUILDDIR)/lexer.o
LIBRARY=$(BUILDDIR)/libmewa.so

all : $(LIBRARY)
clean:
	rm $(BUILDDIR)/*

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp
	$(CC) $(CXXFLAGS) $(INCFLAGS) -c $< -o $@

$(BUILDDIR)/%.o: $(TESTDIR)/%.cpp
	$(CC) $(CXXFLAGS) $(INCFLAGS) -c $< -o $@

$(LIBRARY): $(LIBOBJS)
	$(CC) -shared $(LDFLAGS) $(LIBOBJS) $(LDLIBS) -o $(LIBRARY)

$(BUILDDIR)/%: $(BUILDDIR)/%.o
	$(CC) $(LDFLAGS) -o $@ $(LDLIBS) $< $(LIBRARY)

test : $(BUILDDIR)/testLexer $(BUILDDIR)/testScope
	$(BUILDDIR)/testLexer
	$(BUILDDIR)/testScope


