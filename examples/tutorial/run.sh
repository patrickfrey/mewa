#!/usr/bin/sh
. examples/tutorial/luaenv.sh
export LUABIN=/usr/bin/lua

# [0] Generate scalar types for LLVM IR
echo "examples/gen/generateScalarLlvmir.pl -t examples/tutorial/scalar_types.txt > examples/tutorial/llvmir_scalar.lua"

# [1] Edit your grammar:
# ...

# [2] Build a typesystem module template
# Be careful, run mewa -o only once. It overwrites later edits!
build/mewa -o examples/tutorial/output/typesystem_template.lua -s examples/tutorial/grammar.g

# [3] Copy examples/tutorial/output/typesystem_template.lua to examples/tutorial/typesystem.lua and edit the typesystem.lua
# ...

# [4] Build the compiler
build/mewa -b "$LUABIN" -g -o build/tutorial.compiler.lua examples/tutorial/grammar.g
chmod +x build/tutorial.compiler.lua

build/tutorial.compiler.lua -o build/tutorial.program.llr examples/tutorial/program.prg


