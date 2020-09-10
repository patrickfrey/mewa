# Build flavor parameters:
ifeq ($(strip $(COMPILER)),clang)
CC=/usr/bin/clang
DEBUGFLAGS:=-fstandalone-debug
else ifeq ($(strip $(COMPILER)),gcc)
CC=/usr/bin/g++
else
CC=/usr/bin/clang
DEBUGFLAGS:=-fstandalone-debug
endif
AR=ar rcs
LNKSO=$(CC) -shared

ifeq ($(strip $(RELEASE)),)
DEBUGFLAGS:=-ggdb -g3 -O0 $(DEBUGFLAGS)
else
DEBUGFLAGS=-O3
endif

# Project settings:
BUILDDIR := build
MANPAGES := /usr/local/man
DESTINATION := /usr/local/bin
SRCDIR   := src
TESTDIR  := tests
STDFLAGS := -std=c++17
CXXFLAGS := -c $(STDFLAGS) -fPIC -Wall -Wshadow -pedantic -Wfatal-errors -fvisibility=hidden -pthread $(DEBUGFLAGS)
INCFLAGS := -I$(SRCDIR) -I/usr/include/lua5.2
LDFLAGS  := -g -pthread
LDLIBS   := -lm -lstdc++
LIBOBJS  := $(BUILDDIR)/lexer.o $(BUILDDIR)/automaton.o $(BUILDDIR)/fileio.o
MODOBJS  := $(BUILDDIR)/lualib_mewa.o
LIBRARY  := $(BUILDDIR)/libmewa.a
MODULE   := $(BUILDDIR)/lualib_mewa.so
TESTPRG  := $(BUILDDIR)/testLexer $(BUILDDIR)/testScope $(BUILDDIR)/testAutomaton
PROGRAM  := $(BUILDDIR)/mewa 

# Build targets:
all : build $(LIBRARY) $(PROGRAM) $(MODULE) $(TESTPRG)

clean: build
	rm $(BUILDDIR)/* .depend
build:
	mkdir -p $(BUILDDIR)

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

$(PROGRAM) $(TESTPRG): $(LIBRARY)

$(BUILDDIR)/%: $(BUILDDIR)/%.o
	$(CC) $(LDFLAGS) -o $@ $(LDLIBS) $< $(LIBRARY)

$(MODULE): $(LIBRARY) $(MODOBJS)
	$(LNKSO) -o $@ $(LIBRARY) $(MODOBJS)

test : all
	$(BUILDDIR)/testLexer
	$(BUILDDIR)/testScope
	$(BUILDDIR)/testAutomaton

install:
	cp $(PROGRAM) $(DESTINATION)
	cp MANPAGE $(MANPAGES)/man1/mewa.1
	gzip -f $(MANPAGES)/man1/mewa.1

