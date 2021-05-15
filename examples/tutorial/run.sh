#!/usr/bin/sh
ARG=$1
if [ "x$ARG" = "x" ]; then
	ARG=lua
fi
LUABIN=`which $ARG`
. examples/tutorial/luaenv.sh

# [0] Generate scalar types for LLVM IR
echo "examples/gen/generateScalarLlvmir.pl -t examples/tutorial/scalar_types.txt > examples/tutorial/llvmir_scalar.lua"

echo "[1] Edit your grammar ... "

echo "[2] Build a typesystem module template ... "
# Be careful, run mewa -o only once. It overwrites later edits!
build/mewa -o examples/tutorial/output/typesystem_template.lua -s examples/tutorial/grammar.g

echo "[3] Copy examples/tutorial/output/typesystem_template.lua to examples/tutorial/typesystem.lua and edit the typesystem.lua ..."

echo "[4] Build the compiler ..."
build/mewa -b "$LUABIN" -g -o build/tutorial.compiler.lua examples/tutorial/grammar.g
chmod +x build/tutorial.compiler.lua

echo "[5] Run the compiler on a test program ..."
build/tutorial.compiler.lua -o build/tutorial.program.llr examples/tutorial/program.prg

echo "[6] Create an object file ..."
llc -relocation-model=pic -O=3 -filetype=obj build/tutorial.program.llr -o build/tutorial.program.o

echo "[7] Create an executable ..."
clang -lm -lstdc++ -o build/tutorial.program build/tutorial.program.o

echo "[8] Run the executable file build/tutorial.program"
build/tutorial.program > build/tutorial.result

echo "Salary sum: 280720.00" > build/tutorial.expect
RES=`diff build/tutorial.result build/tutorial.expect | wc -l`

if [ "x$RES" = "x0" ]; then
	echo "OK, result as expected"
else
	echo "FAILED, result not as expected, do diff build/tutorial.result build/tutorial.expect"
	exit 1
fi

echo ""
echo "[9] Run example variable assignment:"
$LUABIN examples/tutorial/examples/variable.lua

echo ""
echo "[10] Run example scope:"
$LUABIN examples/tutorial/examples/scope.lua



