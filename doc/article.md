# Writing Compiler Front-Ends for LLVM with Lua using Mewa

## Lead
LLVM IR text representation makes it possible to write a compiler front-end without being bound to an API. We can map the source text to the text of LLVM IR and use the tools of the clang compiler for further compilation steps. This opens the doors to define compiler front-ends in different ways. Mewa tries to optimize the amount of code written. It targets single authors that would like to write a prototype of a non-trivial programming language fast, but at the expense of a structure supporting collaborative work. This makes it rather a tool for experiment than for building a product.

## Introduction

The Mewa compiler-compiler is a tool to script compiler front-ends in Lua. A compiler written with Mewa takes a source file as input and prints an intermediate representation as input for the next compilation step. The example created as a proof of concept uses the text format of LLVM IR as output.

Mewa provides no support for the evaluation of different paths of code generation. The idea is to do a one-to-one mapping of program structures to code and to leave all analytical optimization steps to the backend (*).

##### (*)
One optimization step that comes into my mind as assigned to the front-end is the mapping of virtual method calls to direct calls.
In the first example language of Mewa, I circumvented this need by only implementing method call indirection on interfaces.

### Goals

This article first explains the typedb library used to define the type system of a language.
The typedb library is written in C++, but I will explain it without referring explicitly to the C++ code.  If you think that Lua is not the best choice and you want to port Mewa to another language, say OCaml or Haskell, then you will find an entry point here.

Then a tutorial will guide you through the definition of an example language.
During the development of the tutorial, I will also point to elements of the larger example language I implemented as a proof of concept.
The goal of this article is to show you how to write your own compiler front-end with Lua using Mewa.

### Target Audience

To understand this article some knowledge about formal languages and parser generators is helpful. To understand the examples you should be familiar with some scripting languages that have a concept of closures similar to Lua. If you have read a tutorial about LLVM IR, you will get grip on the code generation in the examples.

### Deeper Digging

For a deeper digging you have to look at the Mewa project itself and the implementation of the main example language, a strongly typed multiparadigm programming language with structures, classes, interfaces, free functions, generics, and exceptions. There exists also an FAQ that tries to answer problem-solving questions.


### How Mewa Works

For implementing a compiler with Mewa, you define a grammar attributed with Lua function calls.
The program ```mewa`` will generate a Lua script that will transform any source passing the grammar specified into an AST with the specified Lua function calls attached to the nodes and then call the functions of the top level node(s) of the AST.

The top-level AST node functions called by the compiler take the control of the further processing of the compiler front-end. The idea is that the functions called with their AST node as argument calls the functions of their subnodes. During this tree traversal, the types and data structures of the program are built and the output is printed in the process. For some nodes, the traversal can have several passes. It is up to the function attached to the node, how and how many times it calls its subnodes.

The typedb library of Mewa helps you to build up the types and data structures of your program. We will discuss the library in the following section.


## The Type Database (typedb) API

### The API

The typedb library implements an in-memory map for 3 types of data that are associated with a scope:

 * Data of any kind, used to store contextual data like the return type of a function identified by its definition scope
 * Types, all items of the language like variables, data types, generic instance arguments, functions are stored as types 
 * Reductions, all type deduction relations like the relation of a variable to its type and the return type of a function are stored as reductions

The scope inclusion relation may not describe all we need to describe visibility in all cases. But it is always a valid restriction.
Further restrictions can be implemented on top of that.

## Tutorial

The tutorial walks to the implementation of a simple example programming language without any claim to completeness.


## Types and Constructors

In Mewa you define a typesystem by defining types and a structure describing the construction of this type called a **constructor**. The term **constructor** here is not related to the constructor function in programming languages. It represents a pice of the program and connectors to wire it together with other constructors to form a bigger unit. An item in the program is represented by a type/constructor pair. Depending on the type there exist different classes of constructors.

In the example "language1" I define the following classes of constructors as structures:
 *  **Constant Expressions**: The constructor is the value of the constant, depending on the data type represented by a specific value type. For example for integers I use arbitrary precision BCD numbers, for floating point numbers I use Lua numbers. In the future I will use other representations. I just did not find any Lua module, I could install, that was able to process arbitrary precision arithmetics. The precision of the numbers representable by the compiler should not be dependend on the precision of the scalar Lua types.
 *  **Code with Output as Variable**: The constructor is a structure with two fields: **code** and **out**. The code field contains the LLVM code needed to construct the item. The out field contains the output, an address or a virtual register in LLVM.
 *  **Control Boolean**: This constructor is also a structure with two fields: **code** and **out**. The code field contains the LLVM code needed to construct the item. The out field contains the unbound label where the code jumps in the exit case. This type has two instances, the **control true type** where the exit case is **false** and the **control true type** where the exit case is **true**. **Control Boolean** types are used when evaluating boolean expressions and conditions in control structures. Most programming languages require that the evaluation of a boolean expression stops when its outcome is determined. An expression like ```if ptr && ptr->value()``` should not crash.
 *  **RValue Reference**: This constructor is also a structure with two fields: **code** and **out**. The code field contains a template of the LLVM code needed to construct the item. The out field contains the variable to substitute in the code template with the target of the output. **rvalue reference** types are used to implement copy elision and for complex return values passed as pointer argument to the called function. 







