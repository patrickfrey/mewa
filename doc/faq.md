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
_Mewa_ is a tool for the rapid prototyping of compiler frontends for strongly typed languages without the claim to be complete.
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

<a name="problemSolving"/>

## Problem Solving HOWTO
This section of the FAQ gives some recommendations on how to solve specific problems in compilers for strongly typed languages. The hints given here are not meant as instructions. There might be better solutions for the problems presented. Some questions are still open and wait for a good and practical answer.

<a name="astStructure"/>

### How to process the AST structure?

<a name="astTraversalAndScope"/>

### How to handle scopes?

<a name="scopeInstanceAndAllocators"/>

### How to store and access scope related data?

<a name="typeQualifiers"/>

### How to implement type qualifiers like const and reference?

<a name="pointersAndArrays"/>

### How to implement pointers and arrays?

<a name="namespaces"/>

### How to implement namespaces?

<a name="complexExpressions"/>

### How to implement complex expressions?

<a name="controlStructures"/>

### How to implement control structures?

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

In the example language I decided for solution (1) even though it lead to anomalies, but I will return to the problem later.

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

<a name="templates"/>

### How to implement generic programming with templates?

<a name="cLibraryCalls"/>

### How to implement call of C-Library functions?

