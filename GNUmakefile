# Build flavor parameters:
ifeq ($(strip $(COMPILER)),clang)
CC=clang
else ifeq ($(strip $(COMPILER)),gcc)
CC=g++
else
CC=clang
endif
ifeq ($(strip $(CC)),clang)
DEBUGFLAGS:=-fstandalone-debug
endif
AR=ar rcs
LNKSO=$(CC) -shared

ifeq ($(strip $(RELEASE)),)
DEBUGFLAGS:=-ggdb -g3 -O0 $(DEBUGFLAGS)
else
DEBUGFLAGS=-O3
endif

ifeq ($(strip $(PREFIX)),/usr)
MANPAGES := `man --path | awk  '1' RS=":" | grep /usr/share | awk 'NR==1{print $1}'`
else ifeq ($(strip $(PREFIX)),/usr/local)
MANPAGES := `man --path | awk  '1' RS=":" | grep /usr/local | awk 'NR==1{print $1}'`
else ifeq ($(strip $(PREFIX)),)
MANPAGES := `man --path | awk  '1' RS=":" | grep /usr/local | awk 'NR==1{print $1}'`
PREFIX   := /usr/local
else
MANPAGES := $(PREFIX)/man
endif

ifeq ($(wildcard Lua.inc),)
$(error File Lua.inc not found. Please run ./configure first!)
endif

include Lua.inc

# Project settings:
BUILDDIR := build
DESTINATION := $(PREFIX)/bin
SRCDIR   := src
TESTDIR  := tests
STDFLAGS := -std=c++17
CXXFLAGS := -c $(STDFLAGS) -fPIC -Wall -Wshadow -pedantic -Wfatal-errors -fvisibility=hidden -pthread $(DEBUGFLAGS)
INCFLAGS := -I$(SRCDIR) -I$(LUAINC)
LDFLAGS  := -g -pthread
LDLIBS   := -lm -lstdc++
LUALIBS  := -llua$(LUAVER) -L$(LUALIBDIR)
LIBOBJS  := $(BUILDDIR)/lexer.o \
		$(BUILDDIR)/automaton.o $(BUILDDIR)/automaton_tostring.o $(BUILDDIR)/automaton_parser.o \
		$(BUILDDIR)/typedb.o \
                $(BUILDDIR)/fileio.o $(BUILDDIR)/strings.o
MODOBJS  := $(BUILDDIR)/lualib_mewa.o $(BUILDDIR)/lua_load_automaton.o $(BUILDDIR)/lua_run_compiler.o
LIBRARY  := $(BUILDDIR)/libmewa.a
MODULE   := $(BUILDDIR)/mewa.so
TESTPRG  := $(BUILDDIR)/testLexer $(BUILDDIR)/testScope $(BUILDDIR)/testRandomScope $(BUILDDIR)/testIdentMap $(BUILDDIR)/testAutomaton
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
	$(CC) $(LDFLAGS) $(LDLIBS) -o $@ $< $(LIBRARY)

$(MODULE): $(LIBRARY) $(MODOBJS)
	$(LNKSO) $(LUALIBS) $(LDLIBS) -o $@ $(MODOBJS) $(LIBRARY)

test : all
	$(BUILDDIR)/testLexer
	$(BUILDDIR)/testScope
	$(BUILDDIR)/testRandomScope
	$(BUILDDIR)/testIdentMap
	$(BUILDDIR)/testAutomaton
	tests/luatest.sh
check: test

install: all
	cp $(PROGRAM) $(DESTINATION)
	mkdir -p $(MANPAGES)/man1
	cp MANPAGE $(MANPAGES)/man1/mewa.1
	gzip -f $(MANPAGES)/man1/mewa.1

