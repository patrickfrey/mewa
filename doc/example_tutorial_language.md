# Example "tutorial"

## Description
This is a very small subset of language1, only static structures and first class scalar types.

 * Classes (no constructors, no destructors, no inheritance, only methods)
 * Free procedures and functions
 * Backend is LLVM IR

## Project Specific Files

The project specific files of the tutorial language are located in [examples/tutorial](../examples/tutorial)

 - [grammar.g](../examples/tutorial/grammar.g) Grammar description in a kind of BNF with callback hooks to the typesystem module writen in Lua.
 - [llvmir.lua](../examples/tutorial/llvmir.lua) Format templates organized in structures for producing LLVM output.
 - [llvmir_scalar.lua](../examples/tutorial/llvmir_scalar.lua) GENERATED FILE. Format templates organized in structures for the built-in basic first class scalar types. 
 - [scalar_types.txt](../examples/tutorial/scalar_types.txt) Descriptions of the built-in basic first class scalar types.
 - [typesystem.lua](../examples/tutorial/typesystem.lua) Implements the Lua callbacks referenced in the grammar. For better readability this module is includes its subparts in [examples/tutorial/sections/](../examples/tutorial/sections/).

## Example Source Files

 - [program.prg](../examples/tutorial/program.prg) Demo program compiled in the tutorial.
 
