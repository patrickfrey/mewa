# Mewa
Compiler compiler for rapid prototyping of compilers. You write the grammar in a custom language, a sort of _Bison/Yacc_-style BNF with callbacks to a module written in Lua.
The [parser generator program mewa](doc/program_mewa.pdf) generates a Lua module implementing the parser you described in the grammar.
A Lua module written in C++ (see [typedb API](doc/typedb.md)) provides you some assistance to define your own type system and to generate code.

# LLVM
The examples provided here use the [intermediate representation (IR) of LLVM](https://llvm.org/docs/LangRef.html) for code generation. 
For reading more see the [collected links](doc/links.md).

# Status
The project development is currently ongoing and only what you find in the tests really works.

# Documentation
* [Example Compiler Frontend Implementation](doc/example_compiler.md)
* [Installation](INSTALL.Ubuntu.md)
* [Glossary](doc/glossary.md)
* [Grammar Description Language](doc/grammar.md)
* [Parser Generator](doc/program_mewa.pdf)
* [API](doc/libmewa.md)
* [AST (Abstract Syntax Tree) Format](doc/ast.md)
* [Error Codes](doc/errorcodes.md)
* [FAQ](doc/faq.md)
* [Links](doc/links.md)

