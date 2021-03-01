#!/bin/sh
# Some tests of the mewa Lua module
#
set -e 

. tests/luaenv.sh
LUABIN=`which $1`
TARGET=$2
TESTID=`echo $3 | tr a-z A-Z`

if [ "x$TARGET" = "x" ]; then
	TARGET=`cat Platform.inc  | sed 's/TARGET//' | sed 's/:=//' | sed -E 's/\s//g'`
fi

>&2 echo "Target tripple defined as $TARGET"
>&2 echo "Using Lua binary $LUABIN"

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
echo "Building tables for dump of example \"language1\" ..."
/usr/bin/time -f "Running for %e seconds"\
    build/mewa -b "$LUABIN" -d build/language1.debug.out -g -o build/language1.dump.lua -t tests/dumpAutomaton.tpl examples/language1/grammar.g
chmod +x build/language1.dump.lua
echo "Dump tables of compiler for example \"language1\" ..."
build/language1.dump.lua > build/language1.dump.lua.out
verify_test_result "Check dump automaton read by Lua script"  build/language1.dump.lua.out tests/language1.dump.lua.exp
verify_test_result "Check dump states dumped from language1 grammar"  build/language1.debug.out tests/language1.debug.exp
fi

if [ "x$TESTID" = "x" ] || [ "x$TESTID" = "xLANG1" ] ; then
echo "Building tables and script implementing the compiler for example \"language1\" ..."
/usr/bin/time -f "Running for %e seconds"\
    build/mewa -b "$LUABIN" -g -o build/language1.compiler.lua examples/language1/grammar.g
chmod +x build/language1.compiler.lua
fi

for tst in fibo tree class array pointer generic complex
do
	TST=`echo $tst | tr a-z A-Z`
	if [ "x$TESTID" = "x" ] || [ "x$TESTID" = "x$TST" ] ; then
		echo "* Lua test '$tst': "`head -n1 examples/language1/sources/$tst.prg | sed s@//@@`
		echo "Compile and run program examples/language1/sources/$tst.prg"
		build/language1.compiler.lua -t $TARGET -d build/language1.debug.$tst.out -o build/language1.compiler.$tst.llr examples/language1/sources/$tst.prg
		LN=`grep -n 'attributes #0' build/language1.compiler.$tst.llr | awk -F: '{print $1}'`
		head -n `expr $LN - 1` build/language1.compiler.$tst.llr | tail -n `expr $LN - 5` > build/language1.compiler.$tst.out
		verify_test_result "Lua test ($tst debug) compiling example program with language1 compiler" \
						build/language1.debug.$tst.out tests/language1.debug.$tst.exp
		verify_test_result "Lua test ($tst output) compiling example program with language1 compiler" \
						build/language1.compiler.$tst.out tests/language1.compiler.$tst.exp

		lli build/language1.compiler.$tst.llr > build/language1.run.$tst.out
		verify_test_result "Lua test ($tst run) run program translated with language1 compiler"  build/language1.run.$tst.out tests/language1.run.$tst.exp
	fi
done


