# Mewa
_Mewa_ is a **compiler compiler for rapid prototyping** of compiler front-ends. You write the grammar in a custom language, a sort of _Bison/Yacc_-style BNF.
Instead of actions consisting of C statements surrounded by braces in _Bison/Yacc_, you specify a _Lua_ function call. This function call is attached to the [AST](doc/ast.md) node created with it. The [parser generator program mewa](doc/program_mewa.pdf) generates a _Lua_ module implementing the parser you described in the grammar.
A _Lua_ module written in C++ (see [typedb API](doc/typedb.md)) provides some assistance to define your own type system and to generate code.

# Design Philosophy and Limitations
 - _Mewa_'s approach is **naive**. It tries to get as far as possible with a minimalistic but clearly defined model of its world.
 - _Mewa_ is **not optimised for collaborative work** unlike many other compiler front-ends.
 - _Mewa_ provides no support for evaluation of different paths of code generation. The idea is to do a one to one mapping of program structures to code and to **leave all analytical optimization steps to the backend**.
 - _Mewa_ is **not a framework**. It is not instrumented with configuration or plug-ins. It provides a data structure, the [AST](doc/ast.md) and an in-memory map to store and retrieve scope bound data. The program logic is entirely implemented by the compiler front-end author in _Lua_.

# LLVM IR
The examples provided here use the [intermediate representation (IR) of LLVM](https://llvm.org/docs/LangRef.html) for code generation. 
For reading more see the [collected links](doc/links.md).

# Status
The project development is currently ongoing. First release will be in March.

# Portability
## Build
Currently there is only GNU Makefile support. CMake support is not planned for this project because the project is too small to justify it and it has not many dependencies. I would rather like to have Makefiles for platforms where GNU Makefile support is not available (e.g. Windows). 
## Hardware
The tests currently use a LLVM IR target template file for x86_64. I was told that the LLVM interpreter ```lli``` segfaulted running it on a x86_32 platform. I will take a look at that. Contributions in for of target template files like [examples/target/x86_64-pc-linux-gnu.tpl](examples/target/x86_64-pc-linux-gnu.tpl) for other platforms are appreciated.

# Documentation
* [Examples](doc/example_compiler.md)
* [Installation](INSTALL.Ubuntu.md)
* [Glossary](doc/glossary.md)
* [Grammar Description Language](doc/grammar.md)
* [Parser Generator](doc/program_mewa.pdf)
* [API](doc/libmewa.md)
* [AST (Abstract Syntax Tree) Format](doc/ast.md)
* [Error Codes](doc/errorcodes.md)
* [FAQ](doc/faq.md)
* [Links](doc/links.md)
* [Wishlist for LLVM IR](doc/wishlist_llvmir.md)
* [TODO](doc/todo.md)

# Website
* [mewa.cc](mewa.cc)

# Contact (E-Mail)
mail (at) mewa .cc

