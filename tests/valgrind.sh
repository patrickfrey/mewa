#!/bin/sh
# Some valgrind runs
#
set -e

. tests/luaenv.sh

VG="valgrind --leak-check=full --show-leak-kinds=all"

$VG build/testLexer
$VG build/testScope
$VG build/testAutomaton
$VG build/mewa -d build/language1.debug.out -g -o build/language1.dump.lua -t tests/dumpAutomaton.tpl examples/language1.g
$VG build/mewa -g -o build/language1.compiler.lua examples/language1.g
$VG lua build/language1.compiler.lua -d build/language1.compiler.dedug.out -o build/language1.compiler.out examples/language1.prg
