#CC=/usr/bin/g++-9
CC=/usr/bin/clang++-4.0
BUILDDIR=build
SRCDIR=src
TESTDIR=tests
CXXFLAGS=-c -O0 -fPIC -std=c++17 -fPIC -Wall -Wshadow -pedantic -Wfatal-errors -fvisibility=hidden -pthread -ggdb -g3
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

$(BUILDDIR)/testLexer : $(LIBRARY) $(BUILDDIR)/testLexer.o
	$(CC) $(LDFLAGS) -o $@ $(LDLIBS) $(BUILDDIR)/testLexer.o $(LIBRARY)

test : $(BUILDDIR)/testLexer
	$(BUILDDIR)/testLexer


