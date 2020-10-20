# Mewa
Compiler compiler for rapid prototyping of compilers. You write the grammar in a custom language, a sort of _Bison/Yacc_-style BNF with callbacks to a module written in Lua. A Lua module written in C++ provides you with some helper functions to define your type system and to generate code, for example code in the the intermediate representation (IR) of _llvm_.

# Status
The project development is currently ongoing and only what you find in the tests really works.

# Documentation
* [Glossary](doc/glossary.md)
* [Mewa Grammar Description](doc/grammar.md)
* [Example Grammar](examples/language1.g)
* [The mewa.typedb API](doc/typedb.md)
* [Mewa Error Codes](doc/errorcodes.md)
