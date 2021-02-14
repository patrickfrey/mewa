#!/bin/sh
# Some tests of the mewa Lua module
#
set -e 

. tests/luaenv.sh
LUABIN=`which $1`
TESTID=$2

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

if [ "x$TESTID" = "x" ] || [ "x$TESTID" = "xTYPEDB" ] ; then
$LUABIN tests/typedb.lua
fi

if [ "x$TESTID" = "x" ] || [ "x$TESTID" = "xATM" ] ; then
/usr/bin/time -f "Time build language 1 tables for dump running %e seconds"\
    build/mewa -b "$LUABIN" -d build/language1.debug.out -g -o build/language1.dump.lua -t tests/dumpAutomaton.tpl examples/language1/grammar.g
chmod +x build/language1.dump.lua
build/language1.dump.lua > build/language1.dump.lua.out
verify_test_result "Check dump automaton read by Lua script"  build/language1.dump.lua.out tests/language1.dump.lua.exp
verify_test_result "Check dump states dumped from language1 grammar"  build/language1.debug.out tests/language1.debug.exp
fi

if [ "x$TESTID" = "x" ] || [ "x$TESTID" = "xLANG1" ] ; then
/usr/bin/time -f "Time build language 1 compiler running %e seconds"\
    build/mewa -b "$LUABIN" -g -o build/language1.compiler.lua examples/language1/grammar.g
chmod +x build/language1.compiler.lua
fi

if [ "x$TESTID" = "x" ] || [ "x$TESTID" = "xFIBO" ] ; then
echo "Lua test (fibo) Compile and run program examples/language1/sources/fibo.prg"
build/language1.compiler.lua -d build/language1.debug.fibo.out -o build/language1.compiler.fibo.out examples/language1/sources/fibo.prg
verify_test_result "Lua test (fibo debug) compiling example program with language1 compiler"  build/language1.debug.fibo.out tests/language1.debug.fibo.exp
verify_test_result "Lua test (fibo output) compiling example program with language1 compiler"  build/language1.compiler.fibo.out tests/language1.compiler.fibo.exp

lli build/language1.compiler.fibo.out > build/language1.run.fibo.out
verify_test_result "Lua test (fibo run) run program translated with language1 compiler"  build/language1.run.fibo.out tests/language1.run.fibo.exp
fi

if [ "x$TESTID" = "x" ] || [ "x$TESTID" = "xTREE" ] ; then
echo "Lua test (tree) Compile and run program examples/language1/sources/tree.prg"
build/language1.compiler.lua -d build/language1.debug.tree.out -o build/language1.compiler.tree.out examples/language1/sources/tree.prg
verify_test_result "Lua test (tree debug) compiling example program with language1 compiler"  build/language1.debug.tree.out tests/language1.debug.tree.exp
verify_test_result "Lua test (tree output) compiling example program with language1 compiler"  build/language1.compiler.tree.out tests/language1.compiler.tree.exp

lli build/language1.compiler.tree.out > build/language1.run.tree.out
verify_test_result "Lua test (tree run) output running program translated with language1 compiler"  build/language1.run.tree.out tests/language1.run.tree.exp
fi

if [ "x$TESTID" = "x" ] || [ "x$TESTID" = "xCLASS" ] ; then
echo "Lua test (class) Compile and run program examples/language1/sources/class.prg"
build/language1.compiler.lua -d build/language1.debug.class.out -o build/language1.compiler.class.out examples/language1/sources/class.prg
verify_test_result "Lua test (class debug) compiling example program with language1 compiler" build/language1.debug.class.out tests/language1.debug.class.exp
verify_test_result "Lua test (class output) compiling example program with language1 compiler"  build/language1.compiler.class.out tests/language1.compiler.class.exp

lli build/language1.compiler.class.out > build/language1.run.class.out
verify_test_result "Lua test (class run) output running program translated with language1 compiler"  build/language1.run.class.out tests/language1.run.class.exp
fi

if [ "x$TESTID" = "x" ] || [ "x$TESTID" = "xGENERIC" ] ; then
echo "Lua test (generic) Compile and run program examples/language1/sources/generic.prg"
build/language1.compiler.lua -d build/language1.debug.generic.out -o build/language1.compiler.generic.out examples/language1/sources/generic.prg
verify_test_result "Lua test (generic debug) compiling example program with language1 compiler" build/language1.debug.generic.out tests/language1.debug.generic.exp
verify_test_result "Lua test (generic output) compiling example program with language1 compiler"  build/language1.compiler.generic.out tests/language1.compiler.generic.exp

lli build/language1.compiler.generic.out > build/language1.run.generic.out
verify_test_result "Lua test (generic run) output running program translated with language1 compiler"  build/language1.run.generic.out tests/language1.run.generic.exp
fi


