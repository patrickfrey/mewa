# Mewa
_Mewa_ is a **compiler-compiler for prototyping** of compiler front-ends in [Lua](https://www.lua.org). You write an attributed grammar in a custom language, a sort of _Bison/Yacc_-style _BNF_. The parser creates an _AST_ (Abstract Syntax Tree) with a fixed schema. _Lua_ function calls attached as attributes are called on the _AST_ tree traversal triggered by the compiler after the syntax analysis.
A _Lua_ module written in C++ (see [typedb API](doc/typedb.md)) provides some assistance to define your type system and to generate code.

# Design Philosophy and Limitations
 - _Mewa_ is **not a framework**. It is not instrumented with configuration or plug-ins. The program logic is entirely implemented by the compiler front-end author in _Lua_. In other words: You write the compiler front-end with _Lua_ using _Mewa_ and not with _Mewa_ using _Lua_ as binding language.
 - _Mewa_ is **not optimized for collaborative work**.
 - _Mewa_ provides no support for the evaluation of different paths of code generation. **It leaves all analytical optimization steps to the backend**.
 - _Mewa_ does not aim to offer you the greatest variety of possibilities. There exist feature-richer compiler-compilers. _Mewa_ aims to **bring projects considered endless for a single person within reach**.

# LLVM IR
The examples provided here use the [intermediate representation (IR) of LLVM](https://llvm.org/docs/LangRef.html) for code generation. 
For reading more see the [collected links](doc/links.md).

# Status
I consider the software as ready for use in projects.

# Portability

## Build
Currently, there is only GNU Makefile support. CMake support is not planned for this project because the project is too small to justify it and it has not many dependencies. I would rather like to have Makefiles for platforms where GNU Makefile support is not available (e.g. Windows). 

## Target platforms
Issues around target platforms are discussed [here](doc/portability.md).


# Documentation
* [Introduction with Tutorial](doc/tutorial.md)
* [Examples](doc/example_compiler.md)
* [Installation](INSTALL.Ubuntu.md)
* [Glossary](doc/glossary.md)
* [Grammar Description Language](doc/grammar.md)
* [Parser Generator](doc/program_mewa.pdf)
* [API](doc/libmewa.md)
* [AST (Abstract Syntax Tree) Format](doc/ast.md)
* [Error Codes](doc/errorcodes.md)
* [FAQ](doc/faq.md)
* [Algorithms](doc/algorithms.md)
* [Links](doc/links.md)
* [Wishlist for LLVM IR](doc/wishlist_llvmir.md)
* [TODO](doc/todo.md)

# Mailing List
* mewa at freelists

# Website
* [mewa.cc](http://mewa.cc)

# Contact (E-Mail)
* mail at mewa cc

