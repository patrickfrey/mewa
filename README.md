# Mewa
_Mewa_ is a **compiler compiler for prototyping** of compiler front-ends. You write the grammar in a custom language, a sort of _Bison/Yacc_-style BNF.
Instead of actions consisting of C statements surrounded by braces in _Bison/Yacc_, you specify a _Lua_ function call. This function call is attached to the [AST](doc/ast.md) node created with it. The [parser generator program mewa](doc/program_mewa.pdf) generates a _Lua_ module implementing the parser you described in the grammar.
A _Lua_ module written in C++ (see [typedb API](doc/typedb.md)) provides some assistance to define your own type system and to generate code.

# Design Philosophy and Limitations
 - _Mewa_ is **not a framework**. It is not instrumented with configuration or plug-ins. The program logic is entirely implemented by the compiler front-end author in _Lua_. In other words: You write the compiler front-end with _Lua_ using _Mewa_ and not with _Mewa_ using _Lua_ as binding language.
 - _Mewa_ is **not optimised for collaborative work** unlike many other compiler front-ends.
 - _Mewa_ provides no support for evaluation of different paths of code generation. The idea is to do a one to one mapping of program structures to code and to **leave all analytical optimization steps to the backend**.

# LLVM IR
The examples provided here use the [intermediate representation (IR) of LLVM](https://llvm.org/docs/LangRef.html) for code generation. 
For reading more see the [collected links](doc/links.md).

# Status
I consider the software as ready for use in projects.

# Portability

## Build
Currently there is only GNU Makefile support. CMake support is not planned for this project because the project is too small to justify it and it has not many dependencies. I would rather like to have Makefiles for platforms where GNU Makefile support is not available (e.g. Windows). 

## Hardware
The tests currently use a LLVM IR target template file for the following platforms:

 * [Linux x86_64 (Intel 64 bit)](examples/target/x86_64-pc-linux-gnu.tpl)
 * [Linux i686 (Intel 32 bit)](examples/target/i686-pc-linux-gnu.tpl)

The target template files were created with the command 
```sh
echo "int main(){}" | clang -x "c" -S -emit-llvm -o - -
```
And some editing, removing the ```datalayout```, minimizing processor capabilities declared (```target-features``` and replacing the main program declaration with the middle section of the existing target files with the variables to substitute when generating the compiler output.
Contributions for other platforms are appreciated and welcome. 

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

# Mailing List
* mewa at freelists

# Website
* [mewa.cc](http://mewa.cc)

# Contact (E-Mail)
* mail at mewa cc

