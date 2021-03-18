# Mewa
_Mewa_ is a **compiler compiler for prototyping** of compiler front-ends. 
You write the grammar in a custom language, a sort of _Bison/Yacc_-style BNF.
Instead of actions consisting of C statements surrounded by braces in _Bison/Yacc_, you specify a _Lua_ function call. 
The [parser generator program mewa](doc/program_mewa.pdf) generates a _Lua_ module implementing the parser you described in the grammar.
A _Lua_ module written in C++ (see [typedb API](doc/typedb.md)) provides some assistance to define your own type system.
An example implementation of a non trivial compiler and an FAQ exists to help you in problem solving.

# Design Philosophy and Limitations
 - _Mewa_'s approach is **naive**. It tries to get as far as possible with a minimalistic but clearly defined model of its world.
 - _Mewa_ is **not optimised for collaborative work** unlike most other compiler front-ends.
 - _Mewa_ provides no support for evaluation of different paths of code generation. The idea is to do a one to one mapping of program structures to code and to **leave all analytical optimization steps to the backend**.
 - _Mewa_ is **not a framework**. It is not instrumented with configuration or plug-ins. It provides a data structure, the [AST](doc/ast.md) and an in-memory map called _typedb_ to store and retrieve scope bound data. The program logic is entirely implemented by the compiler front-end author in _Lua_. In other words: You write the compiler front-end with _Lua_ using _Mewa_ and not with _Mewa_ using _Lua_ as binding language.

# Links
- Github Project
- Codeproject Article

# Background Story
In spring 2020 I was diagnosed with pancreatic cancer. After the surgery and rehabilitation, I had to undergo a chemotherapy. For this time I decided to make a project that was not bound to anything I was doing at the time. I just wanted to keep my brain spinning. 
It had to be something thrilling that kept me thinking about even in endless fatigue. It had to be a about topic I was familiar with. It should have a foreseeable end, so that I could close it after full recovery. 
I decided to bring the prototyping of compiler front-ends into the Lua world, as I didn't see any similar project alive. Lua seemed a perfect choice because of its implementation of closures and because of its of its capabilities for C/C++ integration. I digged out some ideas I had in prevous related attempts.
Finally I had to implement an example compiler that had a complexity big enough to hold the claim. This was the most complicated thing and I have to admit that I underestimated it a little bit. But now I got through.

