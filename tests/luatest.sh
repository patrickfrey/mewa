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
		exit 0
	else
		echo $TNAM ERR
		exit 1
	fi
}

# Test 1: Build tables from example grammar dump them and compare them to the source
build/mewa -g -o build/language1.lua -t tests/dumpAutomaton.tpl examples/language1.g
chmod +x build/language1.lua
build/language1.lua > build/test1.out
verify_test_result "test dump automaton read by Lua script"  build/test1.out tests/test1.exp


