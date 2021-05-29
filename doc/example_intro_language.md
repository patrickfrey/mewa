# Example "intro"

## Description
This is a very small subset of language1, only static structures and first class scalar types.

 * No memory allocation, only scalar types, constant strings and static structures (named classes) as data types
 * Classes (no constructors, no destructors, no inheritance, only methods)
 * Free procedures and functions
 * Backend is LLVM IR

### Note

This language has been designed to explain _Mewa_. It is not considered as a code-base for a larger project. You shouldn't use the examples presented here as a code-base anyway. The examples, together with the FAQ can help you planning your compiler project. With a layout, you can do a much better job than me. I was cutting my way through the jungle with a machete. You have the overview.

## Project Specific Files

The project specific files of the example language presented in the article [Writing Compiler Front-Ends for LLVM with _Lua_ using _Mewa_](article.md) are located in [examples/intro](../examples/intro)

 - [grammar.g](../examples/intro/grammar.g) Grammar description in a kind of BNF with Lua function calls to the typesystem module writen in Lua as attributes.
 - [llvmir.lua](../examples/intro/llvmir.lua) Format templates organized in structures for producing LLVM output.
 - [llvmir_scalar.lua](../examples/intro/llvmir_scalar.lua) GENERATED FILE. Format templates organized in structures for the built-in basic first class scalar types.
 - [scalar_types.txt](../examples/intro/scalar_types.txt) Descriptions of the built-in basic first class scalar types.
 - [typesystem.lua](../examples/intro/typesystem.lua) Implements the _Lua_ AST node functions, the functions defined as production attributes of the grammar. For better presentability in an article, this module is divided into snippets included with the _Lua_ ```dofile``` command. The snippets implementing AST node functions are located in the subdirectory [ast](../examples/intro/ast). The snippets implementing the functionality are located in the subdirectory [sections](../examples/intro/sections)

## Example Source Files

 - [program.prg](../examples/intro/program.prg) Demo program.
 
