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

ifeq ($(subst 1,Y,$(subst YES,Y,$(strip $(TIME)))),Y)
TIMECMD=/usr/bin/time -f "%C running %e seconds"
else
TIMECMD=
endif

ifeq ($(subst 1,Y,$(subst YES,Y,$(strip $(RELEASE)))),Y)
DEBUGFLAGS=-O3
else
DEBUGFLAGS:=-ggdb -g3 -O0 $(DEBUGFLAGS)
endif

ifeq ($(subst 1,Y,$(subst YES,Y,$(strip $(VERBOSE)))),Y)
CXXVBFLAGS 	:=-v
TSTVBFLAGS 	:=-V
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
LIBDEST  := $(PREFIX)/lib
INCDEST  := $(PREFIX)/include
MAKEDEP  := Lua.inc configure
SRCDIR   := src
INCDIR   := include
TESTDIR  := tests
DOCDIR   := doc
STDFLAGS := -std=c++17
CXXFLAGS := -c $(STDFLAGS) $(CXXVBFLAGS) $(DEBUGFLAGS) -fPIC -Wall -Wshadow -pedantic -Wfatal-errors -fvisibility=hidden -pthread
INCFLAGS := -I$(SRCDIR) -I$(LUAINC) -I$(INCDIR)
LDFLAGS  := -g -pthread
LDLIBS   := -lm -lstdc++
LIBOBJS  := $(BUILDDIR)/lexer.o \
		$(BUILDDIR)/automaton.o $(BUILDDIR)/automaton_tostring.o $(BUILDDIR)/automaton_parser.o \
		$(BUILDDIR)/typedb.o \
                $(BUILDDIR)/fileio.o $(BUILDDIR)/strings.o $(BUILDDIR)/error.o
MODOBJS  := $(BUILDDIR)/lualib_mewa.o \
		$(BUILDDIR)/lua_load_automaton.o $(BUILDDIR)/lua_run_compiler.o \
		$(BUILDDIR)/lua_serialize.o $(BUILDDIR)/lua_parameter.o
LIBRARY  := $(BUILDDIR)/libmewa.a
MODULE   := $(BUILDDIR)/mewa.so
SONAME	 := libmewa.so.0
SHLIBRARY:= $(BUILDDIR)/libmewa.so.0.1.0
SHLIBOBJ := $(BUILDDIR)/libmewa.o
TESTPRG  := $(BUILDDIR)/testError $(BUILDDIR)/testLexer $(BUILDDIR)/testScope $(BUILDDIR)/testRandomScope \
		$(BUILDDIR)/testRandomIdentMap $(BUILDDIR)/testAutomaton \
		$(BUILDDIR)/testTypeDb $(BUILDDIR)/testRandomTypeDb
PROGRAM  := $(BUILDDIR)/mewa

# Build targets:
all : build $(LIBRARY) $(SHLIBRARY) $(PROGRAM) $(MODULE) $(TESTPRG) $(MAKEDEP)

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
$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp $(MAKEDEP)
	$(CC) $(CXXFLAGS) $(INCFLAGS) -c $< -o $@

$(BUILDDIR)/%.o: $(TESTDIR)/%.cpp $(MAKEDEP)
	$(CC) $(CXXFLAGS) $(INCFLAGS) -c $< -o $@

$(LIBRARY): $(LIBOBJS)
	$(AR) $(LIBRARY) $(LIBOBJS)

$(PROGRAM) $(TESTPRG): $(LIBRARY)

$(BUILDDIR)/%: $(BUILDDIR)/%.o
	$(CC) $(LDFLAGS) $(LDLIBS) -o $@ $< $(LIBRARY)

$(MODULE): $(LIBRARY) $(MODOBJS)
	$(LNKSO) $(LUALIBS) $(LDLIBS) -o $@ $(MODOBJS) $(LIBRARY)

$(SHLIBRARY): $(LIBRARY) $(SHLIBOBJ)
	$(LNKSO) -Wl,-soname,$(SONAME) $(LDLIBS) -o $(SHLIBRARY) $(SHLIBOBJ) $(LIBRARY)

test : all
	$(TIMECMD) $(BUILDDIR)/testError $(TSTVBFLAGS)
	$(TIMECMD) $(BUILDDIR)/testLexer $(TSTVBFLAGS)
	$(TIMECMD) $(BUILDDIR)/testScope $(TSTVBFLAGS)
	$(TIMECMD) $(BUILDDIR)/testRandomScope $(TSTVBFLAGS)
	$(TIMECMD) $(BUILDDIR)/testRandomIdentMap $(TSTVBFLAGS)
	$(TIMECMD) $(BUILDDIR)/testAutomaton $(TSTVBFLAGS)
	$(TIMECMD) $(BUILDDIR)/testTypeDb $(TSTVBFLAGS)
	$(TIMECMD) $(BUILDDIR)/testRandomTypeDb $(TSTVBFLAGS)
	tests/luatest.sh "$(LUABIN)"
check: test

install: all
	cp $(PROGRAM) $(DESTINATION)
	cp $(SHLIBRARY) $(LIBDEST)
	mkdir -p $(INCDEST)/mewa
	cp include/mewa/*.hpp $(INCDEST)/mewa/
	mkdir -p $(MANPAGES)/man1
	cp MANPAGE $(MANPAGES)/man1/mewa.1
	gzip -f $(MANPAGES)/man1/mewa.1
	cp $(MODULE) $(CMODPATH)

uninstall:
	rm $(CMODPATH)/mewa.so
	rm $(MANPAGES)/man1/mewa.1.gz
	rm $(INCDEST)/mewa/*.hpp
	rm $(LIBDEST)/libmewa.so*
	rm $(DESTINATION)/mewa

# ONLY FOR DEVELOPPERS (All generated files of make doc are checked in):
DOCSRC := $(wildcard $(DOCDIR)/*.md.in)
doc: all $(DOCSRC)
	@echo "Create documentation of program mewa from its man page ..."
	groff -mandoc -T pdf MANPAGE > $(DOCDIR)/program_mewa.pdf
	@echo "Substitute some .in files in $(DOCDIR) with verified examples. Needs perl installed ..."
	perl doc/gen/map_md_in.pl $(TSTVBFLAGS)

