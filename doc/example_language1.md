# Example "language1"

## Description
This is a multiparadigm general purpose strongly typed programming language with the following features:

 * Structures (plain data stuctures without methods, implicitly defined assignment operator)
 * Interfaces
 * Classes (custom defined constructors, destructors, operators, methods, inheritance of classes and interfaces)
 * Free procedures and functions
 * Polymorphism through interface inheritance
 * Generic programming with templates (__planned__)
 * Exception handling (__planned__)
 * Backend is LLVM-IR
 * Modules (__planned__)

## Project Specific Files

The project specific files of the test language **language1** are located in [examples/language1](../examples/language1)

 - [grammar.g](../examples/language1/grammar.g) Grammar description in a kind of BNF with callback hooks to the typesystem module writen in Lua.
 - [llvmir.lua](../examples/language1/llvmir.lua) Format templates organized in structures for producing LLVM output.
 - [llvmir_scalar.lua](../examples/language1/llvmir_scalar.lua) GENERATED FILE. Format templates organized in structures for the built-in basic first class scalar types. 
 - [scalar_types.txt](../examples/language1/scalar_types.txt) Descriptions of the built-in basic first class scalar types.
 - [typesystem.lua](../examples/language1/typesystem.lua) Implements the Lua callbacks referenced in the grammar.

## Example Source Files

 - [fibo.prg](../examples/language1/sources/fibo.prg) Calculate the n-th (hardcoded) fibonacci number. Example using functions, procedures, constant string literal, and output via a C Library ```printf``` call.
 - [tree.prg](../examples/language1/sources/tree.prg) Tree structure. Build a tree as structure.
 - [class.prg](../examples/language1/sources/class.prg) Classes. Class and interface inheritance.
 - [generic.prg](../examples/language1/sources/generic.prg) Generic programming with templates.
 - [matrix.prg](../examples/language1/sources/matrix.prg) Some non trivial calculations using a bunch of features.

 
