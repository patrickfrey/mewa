#!/bin/sh
# List of instructions (clang) to build mewa without make
#
set -x

COMP="/usr/bin/clang -c -std=c++17 -fPIC -Wall -Wshadow -pedantic -Wfatal-errors -fvisibility=hidden -pthread -O3"
LINK="/usr/bin/clang -pthread"
LINKSO="$LINK -shared"

$COMP -Isrc -c src/lexer.cpp -o build/lexer.o
$COMP -Isrc -c src/automaton.cpp -o build/automaton.o
$COMP -Isrc -c src/automaton_tostring.cpp -o build/automaton_tostring.o
$COMP -Isrc -c src/fileio.cpp -o build/fileio.o

ar rcs build/libmewa.a build/lexer.o build/automaton.o build/automaton_tostring.o build/fileio.o

$COMP -Isrc -I/usr/include/lua5.2 -c src/lualib_mewa.cpp -o build/lualib_mewa.o
$COMP -Isrc -c src/mewa.cpp -o build/mewa.o

$COMP -Isrc -c tests/testLexer.cpp -o build/testLexer.o
$COMP -Isrc -c tests/testScope.cpp -o build/testScope.o
$COMP -Isrc -c tests/testAutomaton.cpp -o build/testAutomaton.o

$LINK -o build/mewa -lm -lstdc++ build/mewa.o build/libmewa.a
$LINK -o build/testLexer -lm -lstdc++ build/testLexer.o build/libmewa.a
$LINK -o build/testScope -lm -lstdc++ build/testScope.o build/libmewa.a
$LINK -o build/testAutomaton -lm -lstdc++ build/testAutomaton.o build/libmewa.a
$LINKSO -o build/lualib_mewa.so build/libmewa.a build/lualib_mewa.o

build/testLexer
build/testScope
build/testAutomaton
