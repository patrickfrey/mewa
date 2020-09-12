#!/bin/sh
# Some tests of the mewa Lua module
#
set -e 

export LUA_CPATH="build/?.so"
export LUA_PATH="examples/?.lua"

verify_test_result() {
	TNAM=$1
	OUTF=$2
	EXPF=$3
	DIFF=`diff $OUTF $EXPF | wc -l`
	if [ "$DIFF" = "0" ]
	then
		echo $TNAM OK
	else
		echo $TNAM ERR
	fi
}

# Test 1: Build tables from example grammar dump them and compare them to the source
build/mewa -g -o build/language1-dump.lua -t tests/dumpAutomaton.tpl examples/language1.g
chmod +x build/language1-dump.lua
build/language1-dump.lua > build/test1.out
verify_test_result "test dump automaton read by Lua script"  build/test1.out tests/test1.exp

build/mewa -d build/test2.out examples/language1.g
verify_test_result "test dump states dumped from language1 grammar"  build/test2.out tests/test2.exp

build/mewa -g -o build/language1.lua examples/language1.g
chmod +x build/language1.lua
build/language1.lua examples/language1.prg > build/test3.out
verify_test_result "test compile example program with language1 compiler"  build/test3.out tests/test3.exp

