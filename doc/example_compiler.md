## Example Compiler Projects

Here is a description of the example language **language1** also used in the tests.
The language is a multiparadigm general purpose strongly typed programming language with 
 * Structures
 * Classes
 * Polymorphism through interface inheritance
 * Generic programming through templates (__planned__)
 * Exception handling (__planned__)
 * Backend LLVM-IR
 * Modules (__planned__)

### Language Independent Files
The language independent files for the test languages (currently only one) are located in [examples](../examples/). These are the following:

 - [typesystem_utils.lua](../examples/typesystem_utils.lua) 	Utility functions for AST traversal, error reporting, symbol encoding, UTF-8 parsing, etc.
 - [cmdlinearg.lua](../examples/cmdlinearg.lua) 	Command line parser for the test language compilers.
 
### Language Independent Generator Script
There is a Perl script for generating the format templates organized in structures for the basic first class scalar types like integers and floating point numbers out of a description. The Perl script has nearly the size of the generated code, but generating code helps here to avoid singular bugs (typos etc.) The script is only called when the description changes. The generated ouput is checked-in as part of the project.

 - [generateScalarLlvmir.pl] examples/gen/generateScalarLlvmir.pl

### Project "language1"
The project specific files of the test language **language1** are located in [examples/language1](../examples/language1)

 - [grammar.g](../examples/language1/grammar.g) Grammar description in a kind of BNF with callback hooks to the typesystem module writen in Lua.
 - [llvmir.lua](../examples/language1/llvmir.lua) Format templates organized in structures for producing LLVM output.
 - [llvmir_scalar.lua](../examples/language1/llvmir_scalar.lua) GENERATED FILE. Format templates organized in structures for the built-in basic first class scalar types. 
 - [scalar_types.txt](../examples/language1/scalar_types.txt) Descriptions of the built-in basic first class scalar types.
 - [typesystem.lua](../examples/language1/typesystem.lua) Implements the Lua callbacks referenced in the grammar.

#### Example Source Files
 - [fibo.prg](../examples/language1/sources/fibo.prg) Calculate the n-th (hardcoded) fibonacci number. Example using functions, procedures, constant string literal, and output via a C Library ```printf``` call.
 - [tree.prg](../examples/language1/sources/tree.prg) Tree structure. Build a tree as structure.
 - [class.prg](../examples/language1/sources/class.prg) Classes. Class and interface inheritance.
 - [generic.prg](../examples/language1/sources/generic.prg) Generic programming with templates.
 - [matrix.prg](../examples/language1/sources/matrix.prg) Some non trivial calculations using a bunch of features.
 

 
