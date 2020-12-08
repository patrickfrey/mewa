# Mewa
_Mewa_ is a **compiler compiler for rapid prototyping**. You write the grammar in a custom language, a sort of _Bison/Yacc_-style BNF with callbacks to a module written in Lua. The [parser generator program mewa](doc/program_mewa.pdf) generates a Lua module implementing the parser you described in the grammar.
A Lua module written in C++ (see [typedb API](doc/typedb.md)) provides you some assistance to define your own type system and to generate code.

# Design Philosophy and Limitations
 - _Mewa_'s approach is **naive**. It tries to get as far as possible with a **minimal** set of tools.
It is **not complete**. This means that chances are high that you will come up against limits of things you can't implement. But there exists an [FAQ](doc/faq.md) with a list of questions that help you to figure out in advance, if your compiler project is duable with _Mewa_.
Furthermore, _Mewa_ is **not optimised for collaborative work** unlike other compiler frontends.

 - _Mewa_ provides no support for evaluation of different paths of code generation.
The idea is to do one to one mapping of program structures to code and to **leave all analytical optimization steps to the backend**.

 - _Mewa_ is just a tool **to experiment, to discover**.

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

