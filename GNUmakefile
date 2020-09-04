# Build flavor parameters:
ifeq ($(strip $(COMPILER)),clang)
CC=/usr/bin/clang
else
CC=/usr/bin/g++
endif
AR=ar rcs

ifeq ($(strip $(RELEASE)),)
DEBUGFLAGS=-ggdb -g3 -O0
else
DEBUGFLAGS=-O3
endif

# Project settings:
BUILDDIR := build
SRCDIR := src
TESTDIR := tests
STDFLAGS := -std=c++17
CXXFLAGS := -c $(STDFLAGS) -fPIC -Wall -Wshadow -pedantic -Wfatal-errors -fvisibility=hidden -pthread $(DEBUGFLAGS)
INCFLAGS := "-I$(SRCDIR)"
LDFLAGS := -g -pthread
LDLIBS := -lm -lstdc++
LIBOBJS := $(BUILDDIR)/lexer.o $(BUILDDIR)/grammar.o
LIBRARY := $(BUILDDIR)/libmewa.a
TESTPRG := $(BUILDDIR)/testLexer $(BUILDDIR)/testScope $(BUILDDIR)/testGrammar

# Build targets:
all : $(LIBRARY) $(TESTPRG)

clean:
	rm $(BUILDDIR)/* .depend

# Generate include dependencies:
SOURCES := $(wildcard $(SRCDIR)/*.cpp)
TESTSRC := $(wildcard $(TESTDIR)/*.cpp)
depend: .depend
.depend: $(SOURCES) $(TESTSRC)
	$(CC) $(STDFLAGS) $(INCFLAGS) -MM $^ | sed -E 's@^([^: ]*[:])@\$(BUILDDIR)/\1@' > .depend
include .depend

# Build rules:
$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp
	$(CC) $(CXXFLAGS) $(INCFLAGS) -c $< -o $@

$(BUILDDIR)/%.o: $(TESTDIR)/%.cpp
	$(CC) $(CXXFLAGS) $(INCFLAGS) -c $< -o $@

$(LIBRARY): $(LIBOBJS)
	$(AR) $(LIBRARY) $(LIBOBJS)

$(BUILDDIR)/%: $(BUILDDIR)/%.o
	$(CC) $(LDFLAGS) -o $@ $(LDLIBS) $< $(LIBRARY)

test : all
	$(BUILDDIR)/testLexer
	$(BUILDDIR)/testScope
	$(BUILDDIR)/testGrammar



