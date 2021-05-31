# Example "language1"

## Description
This is a general-purpose, statically typed programming language with the following features:

 * Structures (plain data stuctures without methods, implicitly defined assignment operator)
 * Interfaces
 * Classes (custom defined constructors, destructors, operators, methods, inheritance of classes and interfaces)
 * Free procedures and functions
 * Polymorphism through interface inheritance
 * Generic programming (generic classes and functions)
 * Lambdas as instantiation arguments of generic classes or functions
 * Exception handling with a fixed exception structure (an error code and an optional string)
 * Backend is LLVM IR

## Project Specific Files

The project specific files of the test language **language1** are located in [examples/language1](../examples/language1)

 - [grammar.g](../examples/language1/grammar.g) Grammar description in a kind of BNF with Lua function calls to the typesystem module writen in Lua as attributes.
 - [llvmir.lua](../examples/language1/llvmir.lua) Format templates organized in structures for producing LLVM output.
 - [llvmir_scalar.lua](../examples/language1/llvmir_scalar.lua) GENERATED FILE. Format templates organized in structures for the built-in basic first class scalar types. 
 - [scalar_types.txt](../examples/language1/scalar_types.txt) Descriptions of the built-in basic first class scalar types.
 - [typesystem.lua](../examples/language1/typesystem.lua) Implements the Lua function calls referenced in the grammar.


## Example Source Files

 - [fibo.prg](../examples/language1/sources/fibo.prg) using functions, procedures, constant string literal, and output via a C Library ```printf``` call.
 - [tree.prg](../examples/language1/sources/tree.prg) implementing a tree, demonstrating a recursively defined data structure.
 - [pointer.prg](../examples/language1/sources/pointer.prg) using pointers to data and pointers to functions.
 - [array.prg](../examples/language1/sources/array.prg) demonstrating the use of arrays.
 - [class.prg](../examples/language1/sources/class.prg) demonstrating class and interface inheritance.
 - [generic.prg](../examples/language1/sources/generic.prg) using some generic programming stuff.
 - [complex.prg](../examples/language1/sources/complex.prg) implementing some calculations with complex numbers.
 - [matrix.prg](../examples/language1/sources/matrix.prg) implementing a matrix class using generics and lambdas.
 - [exception.prg](../examples/language1/sources/exception.prg) testing exception handling, verified with [Valgrind](https://valgrind.org).
 
