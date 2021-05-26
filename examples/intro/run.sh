#!/usr/bin/sh
ARG=$1
if [ "x$ARG" = "x" ]; then
	ARG=lua
fi
LUABIN=`which $ARG`
. examples/intro/luaenv.sh

# [0] Generate scalar types for LLVM IR
echo "examples/gen/generateScalarLlvmir.pl -t examples/intro/scalar_types.txt > examples/intro/llvmir_scalar.lua"

echo "[1] Edit your grammar ... "

echo "[2] Build a typesystem module template ... "
# Be careful, run mewa -o only once. It overwrites later edits!
build/mewa -o examples/intro/output/typesystem_template.lua -s examples/intro/grammar.g

echo "[3] Copy examples/intro/output/typesystem_template.lua to examples/intro/typesystem.lua and edit the typesystem.lua ..."

echo "[4] Build the compiler ..."
build/mewa -b "$LUABIN" -g -o build/intro.compiler.lua examples/intro/grammar.g
chmod +x build/intro.compiler.lua

echo "[5] Run the compiler on a test program ..."
build/intro.compiler.lua -o build/intro.program.llr examples/intro/program.prg

echo "[6] Create an object file ..."
llc -relocation-model=pic -O=3 -filetype=obj build/intro.program.llr -o build/intro.program.o

echo "[7] Create an executable ..."
clang -lm -lstdc++ -o build/intro.program build/intro.program.o

echo "[8] Run the executable file build/intro.program"
build/intro.program > build/intro.result

echo "Salary sum: 280720.00" > build/intro.expect
RES=`diff build/intro.result build/intro.expect | wc -l`

if [ "x$RES" = "x0" ]; then
	echo "OK, result as expected"
else
	echo "FAILED, result not as expected, do diff build/intro.result build/intro.expect"
	exit 1
fi

echo ""
echo "[9] Run example variable assignment:"
$LUABIN examples/intro/tutorial/variable.lua

echo ""
echo "[10] Run example scope:"
$LUABIN examples/intro/tutorial/scope.lua

echo ""
echo "[11] Run example weight1:"
$LUABIN examples/intro/tutorial/weight1.lua

echo ""
echo "[12] Run example weight2:"
$LUABIN examples/intro/tutorial/weight2.lua

diff examples/intro/tutorial/weight1.lua examples/intro/tutorial/weight2.lua > examples/intro/tutorial/weight_diff.txt

echo ""
echo "[13] Run example tags1:"
$LUABIN examples/intro/tutorial/tags1.lua

echo ""
echo "[14] Run example tags2:"
$LUABIN examples/intro/tutorial/tags2.lua

diff examples/intro/tutorial/tags1.lua examples/intro/tutorial/tags2.lua > examples/intro/tutorial/tags_diff.txt

echo ""
echo "[15] Run example env:"
$LUABIN examples/intro/tutorial/env.lua

echo ""
echo "[16] Run example control:"
$LUABIN examples/intro/tutorial/control.lua



