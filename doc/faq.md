# FAQ

1. [General](#general)
    * [What is _Mewa_](#WTF)
    * [What audience is targeted by _Mewa_?](#targetAudience)
    * [What classes of languages are covered by _Mewa_?](#coveredLanguageClasses)
    * [What does this silly name _Mewa_ stand for?](#nameMewa)
1. [Decision Questions](#decisions)
    * [Why are type deduction paths weighted?](#weightedTypeDeduction)
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
    * [How to implement return of a non scalar type from a function?](#functionReturnComplexData)
    * [How to implement hierarchical data structures?](#hierarchicalDataStructures)
    * [How to implement the Pascal "WITH", the C++ "using", etc.?](#withAndUsing)
    * [How to implement object oriented polymorphism?](#virtualMethodTables)
    * [How to implement visibility rules, e.g. private,public,protected?](#visibilityRules)
        * [How to report error on violation of visibility rules implemented as types?](#visibilityRuleErrors)
    * [How to implement multi pass traversal?](#multipassTraversal)
    * [How to handle dependencies between branches of a node in the AST?](#branchDpendencies)
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
_Mewa_ is a tool for the prototyping of compiler frontends for strongly typed languages.
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

<a name="decisions"/>

## Decision Questions

<a name="weightedTypeDeduction"/>

### Why are type deduction paths weighted?

Type deduction rules are not equivalent. Some rules may lose more information than others. Some may be preferred over others. There may exist equivalent paths for a deduction of a type to another but you might prefer one because allowing equivalence and choosing one path randomly by just picking the first may lead to bugs not discovered. This is because you then may have to ignore any alternative solution path appearing, thus forgetting hints to bugs. You may also encounter suddenly changing behaviour with a changed pick. The weighting of deduction rules implements some sort of memory, like a neural network implemented by hand. The deduction path with the mininal weight sum is memorized as the minimal cost path in the network of deduction rules. The choice of a type reduction path may have any rationale, it is memorized by the assignment of weights to its edges.

<a name="problemSolving"/>

## Problem Solving HOWTO
This section of the FAQ gives some recommendations on how to solve specific problems in compilers for strongly typed languages. The hints given here are not meant as instructions. There might be better solutions for the problems presented. Some questions are still open and wait for a good and practical answer.

<a name="astStructure"/>

### How to process the AST structure?
The compiler builds an Abstract Syntax Tree (AST) with the lexems explicitely declared as leaves and Lua calls bound to the productions as non-leaf nodes. Keywords of the language specified as strings in the productions are not appearing in the AST. The compiler calls the topmost nodes of the tree built this way. It is assumed that the tree is traversed by the Lua functions called. The example language uses a function utils.traverse defined in [typesystem_utils.lua](examples/typesystem_utils.lua) for the tree traversal of sub nodes. The traversal function sets the current scope or scope step if defined in the AST and calls the function defined for the AST node.

Unlike in most other compiler frontend frameworks the tree is meant to be traversed once. This means that at specific places it gets ugly. But the hacks needed are describable. It probably depends on the environment if this is justifiable or not.

<a name="astTraversalAndScope"/>

### How to handle scopes?

Scope is part of the type database and represented as pair of cardinal numbers, the first element specifying the start of the scope and the second the first element after the last element of the scope. Every definition has such a scope definition attached. Depending on the current scope step (a cardinal number) only the subset of definitions having a scope covering it are visible when resolving a type. The current scope and the current scope step are set by the tree traversal function before calling the function attached to a node. The current scope is set to its previous value after calling the function attached to a node.

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

This is tricky because of issues around scope and visibility. There are two scopes involved the visibility of the function for callers and the scope of the codeblock that executed the callable. Furthermore we have to do some tricks to do the declaration in a predecessing pass of the code block traversal for enabling recursion (declare the function before its code, so it can be used in the code).
That is why the whole thing is quirky here. See function typesystem.callablebody in typesystem.lua in example "language1".

<a name="functionReturn"/>

### How to implement return in a function?

For every function a structure named ```callable``` is defined an attached to the scope of the function. In this structure we define the type of the returned value as member ```callable.returntype```. A return reduces the argument to the type specified as return value type and returns it.

<a name="functionReturnComplexData"/>

### How to implement return of a non scalar type from a function?

LLVM has the attribute ```sret``` for structure pointers passed to functions for storing the return value for non scalar values.
In the example language I defined the types

 * rval_ref with the qualifier "&&" representing an R-value reference.
 * c_rval_ref with the qualifier "const&&" representing a constant R-value reference.
 
 and an own kind of constructors for these types. The ```out``` member of the constructor references a variable that can be substituted with the reference of the type this value is assigned to. This allowes to inject the destination address later in the process, after the constructor code has been built. It allows to implement copy elision and to instantiate the target of the constructor in the assignment of the R-value reference to a reference. Or to later allocate a local variable for the structure and to instantiate the target there, if the R-value reference is reduced to a value reference instead.
 
 This mechanism allows to treat return value references passed as parameters uniformly with scalar return values, just using different constructors representing these types.
 
<a name="hierarchicalDataStructures"/>

### How to implement hierarchical data structures?

Hierarchical data structures need recursive resolving of types probably with SFINAE. For SFINAE use the Lua pcall function.
For representing hierarchical data structures we can define a new type representing a node with members that can also the hierarchical data structures.
For mapping them we define a type reduction involving recursive type resolution that maps the members of the structure to the members of a candidate data structure.
In the example "language1" I defined the type ```constexprStructureType``` to represent a node with some members in curly brackets ```{ ... }```, separated by commas.

<a name="withAndUsing"/>

### How to implement type contexts, e.g. the Pascal "WITH", the C++ "using" and friends?

In the example "language1" I define a context list as object bound to a scope. Enclosed scopes inherit the context list from the parent scope.
A "WITH" or "using" defines the context list for the current scope as copy of the inherited context list if not defined yet. It then attaches the
new context member to it.

<a name="virtualMethodTables"/>

### How to implement object oriented polymorphism?

In the example "language1" I implemented interface inheritance only. You have to implement a VMT and implement the method call as VMT call. 

<a name="multipassTraversal"/>

### How to implement multi pass traversal?

Sometimes some definitions have to be prioritized, e.g. member variable definitions have to be known before translating member functions.
_Mewa_ does not support multipass traversal by nature, but you can implement it by passing additional parameters to the traversal routine.
In the example grammar I attached a pass argument for the ```definition``` Lua callback:
```
class_definition	= typedefinition ";"			(definition 1)
			| variabledefinition ";"		(definition 1)
			| structdefinition			(definition 1)
			| classdefinition 			(definition 1)
			| interfacedefinition			(definition 1)
			| constructordefinition			(definition 2)
			| functiondefinition			(definition 2)
			;
free_definition		= struct_definition
			| functiondefinition			(definition 1)
			| classdefinition 			(definition 1)
			| interfacedefinition			(definition 1)
			;
```
The callback then gets 2 additional parameters, one from the grammar: ```pass``` and one from the traversal call: ```pass_selected```
```Lua
function typesystem.definition( node, pass, context, pass_selected)
	if not pass_selected or pass == pass_selected then
		local arg = utils.traverse( typedb, node, context)
		if arg[1] then return {code = arg[1].constructor.code} else return {code=""} end
	end
end
```
The traveral call looks as follows:
```Lua
function typesystem.classdef( node, context)
	...
	-- Traversal in two passes, first type and variable declarations, then method definitions
	utils.traverse( typedb, node, context, 1)
	utils.traverse( typedb, node, context, 2)
	...
end
```

<a name="branchDpendencies"/>

### How to handle dependencies between AST branches of a node in the AST?

As you have the tree traversal in your hands (a nice saying of having to do it on your own), you can partially traverse the subnodes of an AST node. You can also combine a partial traversal with a multipass traversal to get some info out of the AST subnodes before doing the real job. 

There are no limitations, but the model of _Mewa_ punishes partial traversal and multipass traversal with bad readability. So you should avoid to use it extensively. 
Use it selectively and only if there is no other possibility and try to document it well.

In the example **language1** of _Mewa_ I use partial traversal (utils.traverseRange) in callable definitions to make the declaration available in the body to allow self reference. Furthermore I use it in the declaration of inheritance. 
Multipass traversal is used in **language1** to parse class and structure members, subclasses and substructures before method declarations in order to make them accessible in the methods independent from the order or definition.

<a name="visibilityRules"/>

### How to implement visibility rules, e.g. private,public,protected?

Visibility is also best expressed with type qualifiers of the structure types with members having privileged access. In the example **language1** I define the types 

 * ```priv_rval``` with the qualifier "private &" with the reduction to ```rval``` with the qualifier "&"
 * ```priv_c_rval``` with the qualifier "private const&" with the reduction to ```c_rval``` with the qualifier "const&"
 * ```priv_pval``` with the qualifier "private ^" with the reduction to ```pval``` with the qualifier "^"
 * ```priv_c_pval``` with the qualifier  "private const^" with the reduction to ```c_pval``` with the qualifier "const^"

for each class type.

Private methods are then defined in the context of ```priv_rval``` or ```priv_c_rval``` if they are const. 
Public methods in the contrary are then defined in the context of ```rval``` or ```c_rval``` if they are const. 

In the body of a method the implicitely defined ```this``` pointer is set to be reducible to its private pointer type.
So is the implicit reference ```*this``` added as private reference to the context used for resolving types there.
If defined like this then private members are accessible from the private context, in the body of methods. Outside, in the public context, private members are not accessible, because there exist no reduction from the public context type to the private context type.

<a name="visibilityRuleErrors"/>

#### How to report error on violation of visibility rules implemented as types?

If visibility rules are implemented with privilege levels represented as types with reductions from the inner (private) layer to the next outer layer, then you can also define the reductions in the other direction, from the outer to the inner layer. The constructors of there reductions could be implemented as functions throwing an error if applied. Define an own tag for these kind of reductions and use these tags as part of the mask when resolving types.

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

Templates are a form of lazy evaluation. This is supported by _Mewa_ as I can store a subtree of the AST and assoziate it with a type instead of directly evaluating it.
If a template type ```TPL<A1,A2,...>``` is referenced we define a type ```T = TPL<t1,t2,...>``` with ```A1,A2,...``` being substitutes for ```t1,t2,...```.
For each template argument Ai we make a [typedb:def_type_as](#def_type_as) definition to define ```(context=T,name=Ai)``` as type ```ti```. With ```T``` as context type we then call the traversal of the template class or structure.
For functions we do not have a clear idea yet. Especially for automatic template type deduction. But as we can declare a type with the name of the template function we can do the deduction in the procedure matching the arguments to the parameters.


<a name="cLibraryCalls"/>

### How to implement the call of a C-Library function?

LLVM supports extern calls. In the specification of the example "language1" I support calls of the C standard library.


