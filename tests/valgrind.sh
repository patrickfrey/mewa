#!/bin/sh
# Some valgrind runs
#
set -e

. tests/luaenv.sh

VG="valgrind --leak-check=full --show-leak-kinds=all --leak-resolution=high --show-reachable=yes --undef-value-errors=yes --track-origins=yes"

$VG build/testError
$VG build/testLexer
$VG build/testScope
$VG build/testRandomScope
$VG build/testAutomaton
$VG build/testRandomIdentMap
$VG build/testTypeDb
$VG build/testRandomTypeDb
$VG lua tests/typedb.lua
$VG build/mewa -d build/language1.debug.out -g -o build/language1.dump.lua -t tests/dumpAutomaton.tpl examples/language1/grammar.g
$VG build/mewa -g -o build/language1.compiler.lua examples/language1/grammar.g
for ff in fibo tree class array pointer generic complex matrix exception
do
	$VG lua build/language1.compiler.lua -d build/language1.debug.$ff.out -o build/language1.compiler.$ff.llr examples/language1/sources/$ff.prg
done
