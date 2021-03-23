# Writing Compiler Front-Ends for LLVM with Lua using Mewa

## Introduction

Writing a Compiler is hard, but LLVM IR brought it within reach. You only have to care about the front-end now.
The Mewa Compiler Compiler is a tool to script compiler front-ends in Lua.
Compared with other frameworks it gives you a smaller code footprint but it is not optimised for collaborative work.
This makes is suitable for language prototyping by a single person but less suitable as platform for a larger developer group.

For implementing a compiler in Mewa, you define a grammar attributed with Lua function calls.
The program ```mewa`` will generate a Lua script that will transform any source passing the grammar specified into an AST with the specified Lua function calls attached to the nodes and then call the functions of the top level node(s) of the AST.

## Goals

In this article I will show by example how Mewa processes a grammar. Then I will describe the main data structure used in the **typedb** library that assists you defining a type system. The library is written in C++, but I will explain it without referring explicitly to the C++ code.
Subsequently there will be some problem solving discussed by example.
For a deeper digging you have to look at the Mewa project itself and the implementation of the main example language, a strongly typed multiparadigm programming language with structures, classes, interfaces, free functions, generics and exceptions.

## Target Audience of this Article

To understand this article some knowledge about formal language grammars and parser generators is helpful. To understand the examples you should be familiar to some scripting languages that have a concept of closures similar to Lua. If you have read a tutorial about LLVM IR, you will get grip on the code generation in the examples.


## The Type Database (typedb) API

### Scope Bound Map

The Type Database API of _Mewa_ implements an in memory map for 3 types of data:

 * Data of any kind
 * Types 
 * Type Reduction Rules

All the data in the **typedb** are bound to a scope. A **scope** is defined as a pair of integer numbers, the start of the scope and the first element after the scope, and a relation that tells if a scope covers another scope:
```
scope_A  covers  scope_B  <=>  scope_A[1] >= scope_B[1] and scope_A[2] <= scope_B[2]
```
One **scope step** is a single integer number. It is incremented for every production in the grammar that is marked as **scope step**. The scope step determines the values of the scope structures assigned to the **AST** nodes. We can tell of a **scope step** if its inside a **scope** or not:
```
step_I  inside scope_A  <=>  step_I >= scope_A[1] and step_I < scope_A[2]
```
All types of data that are stored temporarily in the **typedb** have a scope and a query for data includes a **scope step**. The **typedb** result is then filtered so that the result includes only items defined with a scope attached covering the **scope step** of the query. The scope inclusion relation may not describe all we need to describe visibility in all cases. But it is allways a valid restriction. Further restrictions can be implemented up on that.

The scope bound maps are derived from the original map
```K -> V```
as
```<K,E> -> ordered list of <V,(S,E)>```

E is the end of the scope of the relation K -> V.
S is the start of the scope of the relation K -> V.
The list <K,E> points to is ordered by the scope cover relation, the first element is covered by the second and so forth. 
The image illustrates that. If we search in this kind of map, we do an upperbound seek for say <K,31> and we get to the key <K,35>.
Then we follow the uplinks attached to it to find the elements to retrieve. 

An update is a little bit more complicated as we have to rewire the uplinks of some predecessors and of one successor. 
But in practice this problem is not so hard. You have to willingly construct degrading examples.
For the audience interested in the C++ code, all scope bound data structures of the **typedb** are implemented in src/scope.hpp of the Mewa project.


## Types and Constructors

In Mewa you define a typesystem by defining types and a structure describing the construction of this type called a **constructor**. The term **constructor** here is not related to the constructor function in programming languages. It represents a pice of the program and connectors to wire it together with other constructors to form a bigger unit. An item in the program is represented by a type/constructor pair. Depending on the type there exist different classes of constructors.

In the example "language1" I define the following classes of constructors as structures:
 *  **Constant Expressions**: The constructor is the value of the constant, depending on the data type represented by a specific value type. For example for integers I use arbitrary precision BCD numbers, for floating point numbers I use Lua numbers. In the future I will use other representations. I just did not find any Lua module, I could install, that was able to process arbitrary precision arithmetics. The precision of the numbers representable by the compiler should not be dependend on the precision of the scalar Lua types.
 *  **Code with Output as Variable**: The constructor is a structure with two fields: **code** and **out**. The code field contains the LLVM code needed to construct the item. The out field contains the output, an address or a virtual register in LLVM.
 *  **Control Boolean**: This constructor is also a structure with two fields: **code** and **out**. The code field contains the LLVM code needed to construct the item. The out field contains the unbound label where the code jumps in the exit case. This type has two instances, the **control true type** where the exit case is **false** and the **control true type** where the exit case is **true**. **Control Boolean** types are used when evaluating boolean expressions and conditions in control structures. Most programming languages require that the evaluation of a boolean expression stops when its outcome is determined. An expression like ```if ptr && ptr->value()``` should not crash.
 *  **RValue Reference**: This constructor is also a structure with two fields: **code** and **out**. The code field contains a template of the LLVM code needed to construct the item. The out field contains the variable to substitute in the code template with the target of the output. **rvalue reference** types are used to implement copy elision and for complex return values passed as pointer argument to the called function. 







