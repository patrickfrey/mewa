## Example Compiler Projects

Here is a description of the example language **language1** also used in the tests.
The language is a multiparadigm general purpose strongly typed programming language with 
 * Structures
 * Classes
 * Polymorphism through interface inheritance (__planned__)
 * Generic programming through templates (__planned__)
 * Exception handling (__planned__)
 * Backend LLVM-IR
 * Modules (__maybe__)

### Language Independent Files
 - [Typesystem Utilities Independent of the Language](../examples/typesystem_utils.lua)
 - [Compiler Command Line Argument Parser](../examples/cmdlinearg.lua)

### Project "language1"
 - [Grammar Description](../examples/language1/grammar.g)
 - [LLVM Templates](../examples/language1/llvmir.lua) 
      - [Built-in Scalar Type Descriptions](../examples/language1/scalar_types.txt)
      - [Built-in Scalar Types (generated file!)](../examples/language1/llvmir_scalar.lua)
 - [Typesystem / Lua Callbacks Referenced in Grammar](../examples/language1/typesystem.lua)
 - Example Source Files
    - [Fibonacci](../examples/language1/sources/fibo.prg)
    - [Tree Structure](../examples/language1/sources/tree.prg)
    - [Classes](../examples/language1/sources/class.prg)
 
