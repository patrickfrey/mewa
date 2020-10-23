# Mewa
Compiler compiler for rapid prototyping of compilers. You write the grammar in a custom language, a sort of _Bison/Yacc_-style BNF with callbacks to a module written in Lua.
The [parser generator program mewa](doc/program_mewa.html) generates a Lua module implementing the parser you described in the grammar.
A Lua module written in C++ provides you some assistance to define your own type system and to generate code. You write a _Lua_ script implementing the callbacks called from the generated parser.
The examples in the documentation use the [intermediate representation (IR) of LLVM](https://llvm.org/devmtg/2017-06/1-Davis-Chisnall-LLVM-2017.pdf) for code generation.

# Status
The project development is currently ongoing and only what you find in the tests really works.

# Documentation
* [Glossary](doc/glossary.md)
* [The Mewa Grammar Description Language](doc/grammar.md)
* [An Example Grammar](examples/language1.g)
* [The Mewa Lua Module API](doc/libmewa.md)
* [The Mewa Error Codes](doc/errorcodes.md)
* [Collected Links](doc/links.md)

