# FAQ

1. [General](#general)
    * [What is _Mewa_](#WTF)
    * [What audience is targeted by _Mewa_?](#targetAudience)
    * [What classes of languages are covered by _Mewa_?](#coveredLanguageClasses)
    * [What does this silly name _Mewa_ stand for?](#nameMewa)
1. [Problem Solving HOWTO](#problemSolving)
    * [How to process the AST structure?](#astStructure)
    * [How to handle scopes?](#astTraversalAndScope)
    * [How to store and access scope related data?](#scopeInstanceAndAllocators)
    * [How to implement type qualifiers like const and reference?](#typeQualifiers)
    * [How to implement pointers and arrays?](#pointersAndArrays)
    * [How to implement namespaces?](#namespaces)
    * [How to implement complex expressions?](#complexExpressions)
    * [How to implement control structures?](#controlStructures)
    * [How to implement constants and constant expressions?](#constants)
    * [How to implement callables like functions, procedures and methods?](#functionsProceduresAndMethods)
    * [How to implement return in a function?](#functionReturn)
    * [How to implement hierarchical data structures?](#hierarchicalDataStructures)
    * [How to implement the Pascal "WITH", the C++ "using", etc.?](#withAndUsing)
    * [How to implement object oriented polymorphism?](#virtualMethodTables)
    * [How to implement visibility rules, e.g. private,public,protected?](#visibilityRules)
    * [How to implement capture rules of local function definitions?](#localFunctionCaptureRules)
    * [How to implement access of types declared later in the source?](#orderOfDefinition)
    * [How to implement exception handling?](#exceptions)
    * [How to implement generic programming with templates?](#templates)
    * [How to implement call of C-Library functions?](#cLibraryCalls)


<a name="general"/>

## General
This section of the FAQ answers some broader questions about what one can do with _Mewa_.

<a name="WTF"/>

### What is _Mewa_?
_Mewa_ is a tool for the rapid prototyping of compiler frontends for strongly typed languages.
It aims to support the mapping of a programming language to a intermediate representation in the simplest way leaving all analytical optimization steps to the backend. 

<a name="targetAudience"/>

### What audience is targeted by _Mewa_?
**_Mewa_ is for people with some base knowledge about how parser generators work (*).**
Maybe some insights into other compiler frontend implementations is recommended as the *naive* approach of _Mewa_ as the only point of view could be misleading.

**(*)** There has not yet been much effort done to assist users in case of conflicts in a grammar. The conflicts are detected and reported and the building of the parser fails in consequence, but there is no mechanism implemented for autocorrection and there is no deeper analysis made by the _Mewa_ parser generator program.

<a name="coveredLanguageClasses"/>

### What classes of languages are covered by _Mewa_?
**_Mewa_ was been designed to be capable to implement general purpose programming languages with the power of C++ (*) or Rust having a LALR(1) grammar definition.**
It is not recommended to use _Mewa_ for other than compilable, stronly typed programming languages, because it was not designed for other language classes.
To define a type system as graph of types and reductions within the hierarchical concept of scope in this form makes no sense for other language classes.

**(*)** Unfortunately C++ (and C) can't be implemented with _Mewa_ as there is no clear separation of syntax and semantic analysis possible in C/C++.
To name one example: There is no way to decide if the statement ```A * B;``` is an expression or a type declaration without having semantical information about A.


<a name="nameMewa"/>

### What does this silly name _Mewa_ stand for?
Name finding is difficult. I got stuck in Polish names of birds without non ASCII characters. _Mewa_ is the Polish name for seagull. 
I think the original idea was that seagulls are a sign of land nearby when you are lost in the sea. Compiler projects are usually a neverending thing (like the sea). With _Mewa_ you see land, meaning that you can at least prototype your compiler. :-)

<a name="problemSolving"/>

## Problem Solving HOWTO
This section of the FAQ gives some recommendations on how to solve specific problems in compilers for strongly typed languages. The hints given here are not meant as instructions. There might be better solutions for the problems presented. Some questions are still open and wait for a good and practical answer.

<a name="astStructure"/>

### How to process the AST structure?
The compiler builds an Abstract Syntax Tree (AST) with the lexems explicitely declared as leaves and Lua calls bound to the productions as non-leaf nodes. Keywords of the language specified as strings in the productions are not appearing in the AST. The compiler calls the topmost nodes of the tree built this way. It is assumed that the tree is traversed by the Lua functions called. The example language uses a function utils.traverse defined in (typesystem_utils.lua)[examples/typesystem_utils.lua] for the tree traversal of sub nodes. The traversal function sets the current scope or scope step if defined in the AST and calls the function defined for the AST node.

Unlike in most other compiler frontend frameworks the tree is meant to be traversed once. This means that at specific places it gets ugly. But the hacks needed are describable. It probably depends on the environment if this is justifiable or not. In my opinion it is for rapid prototyping.

<a name="astTraversalAndScope"/>

### How to handle scopes?

Scopes are part of the type database. Every definition has a scope attached. Depending on the current scope step only the subset of definitions having a scope covering it are visible when resolving a type. The current scope and the current scope step are set by the tree traversal function before calling the function attached to a node and set to the previous value after calling the function attached to a node.

<a name="scopeInstanceAndAllocators"/>

### How to store and access scope related data?

Scope related data in form of a lua object can be stored with [typedb:set_instance](#set_instance) and retrieved exactly with [typedb:this_instance](#this_instance) and with inheritance (enclosing scopes inherit an object from the parent scope) with [typedb:get_instance](#get_instance).

<a name="typeQualifiers"/>

### How to implement type qualifiers like const and reference?

Qualifiers are not considered to be attached to the type but part of the type definition. So a const reference to an integer is a type on its its own, like an integer value type or a pointer to an integer. There are no attributes of types in the typedb, but you can associate attributes to types with lua tables. The implementation of the "language1" example does this quite extensively. In the typedb the only relations of types are implemented as reduction definitions.

<a name="pointersAndArrays"/>

### How to implement pointers and arrays?

Pointers and arrays are types on its own. Because there are no attributes attached to types in the typedb, you have to declare an array of size 40 as own type that differs from the array of size 20. For arrays you need a mechanism for definition on demand. Such a mechanism you need for templates too.

<a name="namespaces"/>

### How to implement namespaces?

Namespaces are as types. A namespace type can then be used as context type in a [typedb:resolve_type](#resolve_type) call. Every resolve type call has a set of context types or a single context type as argument. 

<a name="complexExpressions"/>

### How to implement complex expressions?

For complex expressions you may need some sort of recursive type resolution within a constructor call. In the "example language 1" I defined a const expression type for unresolved structures represented as list of type constructor pairs. The reduction of such a structure to a type that can bew instantiated with structures using type resolution ([typedb:resolve_type](#resolve_type)) and type derivation ([typedb:derive_type](#derive_type)) to build the structures as composition of the structure members. This mechanism can be defined recursively, meaning that the members can be defined as const expression type for unresolved structures themselves.

<a name="controlStructures"/>

### How to implement control structures?

Control structure constructors are special because you need to interrupt and terminate expression evaluation as soon as the expression evaluation result is determined. Most known programming languages have this behaviour in their specification. For example in an expression in C/C++ like 
```C
if (expr != NULL && expr->data != NULL) {...}
```
the subexpression ```expr->data != NULL``` is not evaluated if ```expr != NULL``` evaluated to false.

For control structures two new types have been defined in the example "language1":
1. Control True Type that represents a constructor as structure with 2 members: 
    1. **code** containing the code that is executed in the case the expression evaluates to **true** and 
    1. **out** the name of a free label (not defined in the code yet) where the evaluation jumps to in the case the expression evaluates to **false** and 
1. Control False Type
    1. **code** containing the code that is executed in the case the expression evaluates to **false** and 
    1. **out** the name of a free label (not appearing in the code yet) where the evaluation jumps to in the case the expression evaluates to **true** and 

An expression with the operator ```||``` is evaluated to a control false type.
An expression with the operator ```&&``` is evaluated to a control true type.
This is because the behaviour has also to apply to expressions without control structures like the C99/C++ example
```C
bool hasData = (expr != NULL && expr->data != NULL);
```
Control true/false types are convertible into each other.
Depending on the control structure one of control true type or control false type is required.

<a name="constants"/>

### How to implement constants and constant expressions?
##### Propagation of Types
The constructors of constant values are implemented as userdata. Expressions are values calculated immediately by the constructor functions. 
The propagation of types is a more complex topic:
```
byte b = 1
int a = b + 2304879
```
What type should the expression on the right side of the second assignment have? 
 1. We can reduce ```2304879``` to the type of 'b' before the addition
 1. We can promote 'b' to a maximum size integer type and so ```2304879``` before the addition
 1. We can propagate the type ```int``` of the left side of the assignment to the expression ```b + 2304879``` as result type.

In the example language I decided for solution (1) even though it lead to anomalies, but I will return to the problem later. So far all three possibilities can be implemented with _Mewa_.

##### What Types to Use
The decision for data types used to implement constants is more complex as it actually looks like at the first glance. But for implementing constant values you can't rely on the scalar types of the language your implementing a compiler with because the language you design may have a different set of scalar types with a different set of rules. You need arbitrary precision arithmetics for integers and floating point numbers at least. The first example language of _Mewa_ uses a module implementing BCD numbers for arbitrary precision integer arithmetics. A silly decision that should be revisited. I just did not find a module for arbitrary precision integer arithmetics on luarocks with a working installation at the time. So I decided to activate some older project (implementing arbitrary BCD arithmetics for Lua) as a luarocks module on my own. This will be replaced in the future.

<a name="functionsProceduresAndMethods"/>

### How to implement callables like functions, procedures and methods?

<a name="functionReturn"/>

### How to implement return in a function?

<a name="hierarchicalDataStructures"/>

### How to implement hierarchical data structures?

<a name="withAndUsing"/>

### How to implement type contexts, e.g. the Pascal "WITH", the C++ "using" and friends?

<a name="virtualMethodTables"/>

### How to implement object oriented polymorphism?

<a name="visibilityRules"/>

### How to implement visibility rules, e.g. private,public,protected?

<a name="localFunctionCaptureRules"/>

### How to implement capture rules of local function definitions?
The concept of scope is hierarchical and allows to access items of an enclosing scope by an enclosed scope. This raises some problems when dealing with local function definitions.
```
function y() {
	int a = 3;
	function b( int x) {
		return a*x;
	}
```
By default ```a``` is accessible in function ```b```. This can lead to anomalies without precaution. Especially if we use naively define a new register allocator for every function. One solution could be using a register allocator that uses a different prefix of the allocated items depending on the function hierarchy level. In this case it could be possible to define rules on the constructors issued for variables accessed.

_This is still an open question and I do not know the correct answer yet_.

<a name="orderOfDefinition"/>

### How to implement access of types declared later in the source?
In C++ you can reference types declared later in the source.
```
class A {
	B b;

	class B {
		...
	};
};
```
When relying on one traversal of the AST to emit code, we can only implement an access scheme following the order of definitions like this:
```
class A {
	class B {
		...
	};
	B b;
};
```
For the first example we need more than one tree traversal each of implementing only a subset of functions. 
I did not figure out yet how it would be the best to support multiple tree traversals. But I am sure that the problem does not blow up the framework of _Mewa_. You need two traversals, one restricted on type and variable declarations first and a second complete one as it is defined now.

_We leave this question as TODO_.

<a name="exceptions"/>

### How to implement exception handling?

Exception handling and especially zero-cost exception handling is a subject of further investigation in _Mewa_. As I broadly understood it you define a block with destructors to be executed in case of a thrown exception and LLVM builds the tables needed. This can be be implemented without any support needed from _typedb_.

<a name="templates"/>

### How to implement generic programming with templates?

Templates are a subject of further investigation in _Mewa_. I have the vague idea to define a type ```TPL<ARG1,...>``` and reductions ```TPL<ARG1,...> -> ARG1``` and for all template arguments ARGi respectively. The resulting type of the reduction is the template parameter passed. The sub with the template definition is then evaluated with these reductions in the template scope defined. Templates are a form of lazy evaluation. This is supported by _Mewa_ as I can store a subtree of the AST and assoziate it with a type instead of directly evaluating it.

<a name="cLibraryCalls"/>

### How to implement the call of a C-Library function?

LLVM supports extern calls. In the specification of the example "language1" I support calls of the C standard library.


