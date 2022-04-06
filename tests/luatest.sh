#!/bin/sh
# Some tests of the mewa Lua module
#
set -e

. tests/luaenv.sh

LUABIN=`which $1`
LLIBIN=lli
LLCBIN=llc
TARGET=$2
TESTID=`echo $3 | tr a-z A-Z`

if [ "x$TARGET" = "x" ]; then
	TARGET=`cat Platform.inc  | sed 's/TARGET//' | sed 's/:=//' | sed -E 's/\s//g'`
fi

>&2 echo "Target triple defined as $TARGET"
>&2 echo "Using Lua binary $LUABIN"

RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
ORANGE='\033[0;33m'
NOCOL='\033[0m'
ECHOBIN=`which echo`
ECHOCOL="$ECHOBIN -e"
FLAG_OK=${GREEN}OK${NOCOL}
FLAG_OK_ND=${BLUE}OK${NOCOL}
FLAG_ERR=${RED}ERR${NOCOL}

verify_test_result() {
	TNAM=$1
	OUTF=$2
	EXPF=$3
	ORDER=$4
	DIFF=`diff $OUTF $EXPF | wc -l`
	if [ "$DIFF" = "0" ]
	then
		$ECHOCOL $TNAM $FLAG_OK
	elif [ "x$ORDER" = "xSORT" ]
	then
		sort $OUTF > $OUTF.sort
		sort $EXPF > $OUTF.exp.sort
		DIFF=`diff $OUTF.sort $OUTF.exp.sort | wc -l`
		if [ "$DIFF" = "0" ]
		then
			$ECHOCOL $TNAM $FLAG_OK_ND
		else
			echo "diff $OUTF $EXPF"
			$ECHOCOL $TNAM $FLAG_ERR
		fi
	else
		echo "diff $OUTF $EXPF"
		$ECHOCOL $TNAM $FLAG_ERR
	fi
}

if [ "x$TESTID" = "x" ] || [ "x$TESTID" = "xTYPEDB" ] ; then
$LUABIN tests/typedb.lua
fi

if [ "x$TESTID" = "x" ] || [ "x$TESTID" = "xINDENTL" ] ; then
build/mewa -o build/indentl.lua -g tests/indentl.g
INDENTLRES=$?
if [ "$INDENTLRES" = "0" ]; then
   $LUABIN build/indentl.lua tests/indentl.prg
   INDENTLRES=$?
fi
if [ "$INDENTLRES" = "0" ]; then
   INDENTLFLAG=$FLAG_OK
else
   INDENTLFLAG=$FLAG_ERR
fi
echo "Test simple off-side rule language (%INDENTL command tests/indentl.g) $INDENTLFLAG"
fi

if [ "x$TESTID" = "x" ] || [ "x$TESTID" = "xATM" ] ; then
$ECHOCOL "${ORANGE}Building tables for dump of example \"language1\" ...${NOCOL}"
/usr/bin/time -f "Running for %e seconds"\
    build/mewa -b "$LUABIN" -d build/language1.debug.tmp -g -o build/language1.dump.lua -t tests/dumpAutomaton.tpl examples/language1/grammar.g
$ECHOCOL "${ORANGE}Building table with all the contents of the language parsed for \"language1\" ...${NOCOL}"
/usr/bin/time -f "Running for %e seconds"\
    build/mewa -b "$LUABIN" -l -o build/language1_grammar.lua examples/language1/grammar.g
echo "#!$LUABIN" > build/language1.image.lua
echo "image = require(\"printGrammarFromImage\")" >> build/language1.image.lua
echo "langdef = require(\"language1_grammar\")" >> build/language1.image.lua
echo "image.printLanguageDef( langdef)" >> build/language1.image.lua
chmod +x build/language1.image.lua
$ECHOCOL "${ORANGE}Create grammar for example \"language1\" from Lua table...${NOCOL}"
build/language1.image.lua > build/language1.image.tmp
cat build/language1_grammar.lua | grep -v "VERSION =" > build/language1_grammar.tmp
verify_test_result "Check 'mewa -l' output of parsed grammar as Lua table" build/language1_grammar.tmp tests/language1_grammar.lua.exp
chmod +x build/language1.dump.lua
$ECHOCOL "${ORANGE}Dump tables of compiler for example \"language1\" ...${NOCOL}"
build/language1.dump.lua > build/language1.dump.lua.tmp
cat build/language1.dump.lua.tmp\
	| sed -E 's@\[[0-9][0-9]*\] = [0-9][0-9]*@\[XX\] = YY@g' | uniq > build/language1.dump.lua.out
cat build/language1.debug.tmp\
	| sed -E 's@^\[[0-9][0-9]*\]@\[XX\]@g'\
	| sed -E 's@FOLLOW \[[0-9][0-9]*\]@FOLLOW \[XX\]@g'\
	| sed -E 's@\([0-9][0-9]*\)@\(XX\)@g'\
	| sed -E 's@GOTO [0-9][0-9]*@GOTO XX@g'\
	| sed -E 's@goto [0-9][0-9]*@goto XX@g'\
	| sed -E 's@=> [0-9][0-9]*@=> XX@g' > build/language1.debug.out
verify_test_result "Check dump automaton read by Lua script"  build/language1.dump.lua.out tests/language1.dump.lua.exp
verify_test_result "Check dump states dumped from language1 grammar"  build/language1.debug.out tests/language1.debug.exp "SORT"
fi

if [ "x$TESTID" = "x" ] || [ "x$TESTID" = "xLANG1" ] ; then
echo "Building tables and script implementing the compiler for example \"language1\" ..."
/usr/bin/time -f "Running for %e seconds"\
    build/mewa -b "$LUABIN" -g -o build/language1.compiler.lua examples/language1/grammar.g
chmod +x build/language1.compiler.lua
fi

for tst in fibo tree class array pointer generic complex matrix exception
do
	TST=`echo $tst | tr a-z A-Z`
	if [ "x$TESTID" = "x" ] || [ "x$TESTID" = "x$TST" ] ; then
		$ECHOCOL "${ORANGE}* Lua test '$tst': "`head -n1 examples/language1/sources/$tst.prg | sed s@//@@`"${NOCOL}"
		echo "Compile program examples/language1/sources/$tst.prg to LLVM IR"
		/usr/bin/time -f "Running for %e seconds"\
			build/language1.compiler.lua -t $TARGET -d build/language1.debug.$tst.tmp -o build/language1.compiler.$tst.llr examples/language1/sources/$tst.prg
		cat build/language1.debug.$tst.tmp\
			| sed -E 's/goto [0-9][0-9]*/goto XXX/g'\
			| sed -E 's/state [0-9][0-9]*/state XXX/g' > build/language1.debug.$tst.out
		LN=`grep -n 'attributes #0' build/language1.compiler.$tst.llr | awk -F: '{print $1}'`
		head -n `expr $LN - 1` build/language1.compiler.$tst.llr | tail -n `expr $LN - 5` > build/language1.compiler.$tst.out
		verify_test_result "Lua test ($tst debug) compiling example program with language1 compiler" \
						build/language1.debug.$tst.out tests/language1.debug.$tst.exp
		verify_test_result "Lua test ($tst output) compiling example program with language1 compiler" \
						build/language1.compiler.$tst.out tests/language1.compiler.$tst.exp
		echo "Run program examples/language1/sources/$tst.prg with LLVM interpreter (lli)"
		/usr/bin/time -f "Running for %e seconds"\
			$LLIBIN build/language1.compiler.$tst.llr > build/language1.run.$tst.out
		verify_test_result "Lua test ($tst run) interprete program translated with language1 compiler"  build/language1.run.$tst.out tests/language1.run.$tst.exp
		echo "Build object file build/language1.compiler.$tst.o"
		$LLCBIN -relocation-model=pic -O=3 -filetype=obj build/language1.compiler.$tst.llr -o build/language1.compiler.$tst.o
		$LLCBIN -relocation-model=pic -O=3 -filetype=asm build/language1.compiler.$tst.llr -o build/language1.compiler.$tst.asm
		echo "Build executable file build/language1.compiler.$tst"
		clang -lm -lstdc++ -o build/language1.compiler.$tst build/language1.compiler.$tst.o
		echo "Run executable file build/language1.compiler.$tst"
		build/language1.compiler.$tst > build/language1.run.$tst.bin.out
		verify_test_result "Lua test ($tst exe) native run program translated with language1 compiler"  build/language1.run.$tst.out tests/language1.run.$tst.exp
	fi
done


