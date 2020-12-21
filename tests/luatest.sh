#!/bin/sh
# Some tests of the mewa Lua module
#
set -e 

. tests/luaenv.sh
LUABIN=$1

verify_test_result() {
	TNAM=$1
	OUTF=$2
	EXPF=$3
	DIFF=`diff $OUTF $EXPF | wc -l`
	if [ "$DIFF" = "0" ]
	then
		echo $TNAM OK
	else
                echo "diff $OUTF $EXPF"
		echo $TNAM ERR
	fi
}

$LUABIN tests/typedb.lua

/usr/bin/time -f "Time Lua test 1/2 running %e seconds"\
    build/mewa -b "$LUABIN" -d build/language1.debug.out -g -o build/language1.dump.lua -t tests/dumpAutomaton.tpl examples/language1/grammar.g
chmod +x build/language1.dump.lua
build/language1.dump.lua > build/language1.dump.lua.out
verify_test_result "Lua test (1) dump automaton read by Lua script"  build/language1.dump.lua.out tests/language1.dump.lua.exp
verify_test_result "Lua test (2) dump states dumped from language1 grammar"  build/language1.debug.out tests/language1.debug.exp

/usr/bin/time -f "Time Lua test 3/4 running %e seconds"\
    build/mewa -b "$LUABIN" -g -o build/language1.compiler.lua examples/language1/grammar.g
chmod +x build/language1.compiler.lua
build/language1.compiler.lua -d build/language1.compiler.debug.1.out -o build/language1.compiler.1.out examples/language1/sources/fibo.prg
verify_test_result "Lua test (3) debug output compiling example program with language1 compiler"  build/language1.compiler.debug.1.out tests/language1.compiler.debug.1.exp
verify_test_result "Lua test (4) output compiling example program with language1 compiler"  build/language1.compiler.1.out tests/language1.compiler.1.exp

lli build/language1.compiler.1.out > build/language1.run.1.out
verify_test_result "Lua test (5) output running program translated with language1 compiler"  build/language1.run.1.out tests/language1.compiler.run.1.exp

