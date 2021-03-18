# FAQ

1. [General](#general)
    * [What is _Mewa_](#WTF)
    * [What audience is targeted by _Mewa_?](#targetAudience)
    * [What classes of languages are covered by _Mewa_?](#coveredLanguageClasses)
    * [What does this silly name _Mewa_ stand for?](#nameMewa)
1. [Decision Questions](#decisions)
    * [Why are type deduction paths weighted?](#weightedTypeDeduction)
    * [Why are reductions defined with scope?](#reductionScope)
1. [Problem Solving HOWTO](#problemSolving)
    * [How to process the AST structure?](#astStructure)
    * [How to handle scopes?](#astTraversalAndScope)
    * [How to store and access scope bound data?](#scopeInstanceAndAllocators)
    * [How to implement type qualifiers like const and reference?](#typeQualifiers)
    * [How to implement pointers and arrays?](#pointersAndArrays)
    * [How to implement namespaces?](#namespaces)
    * [How to implement the resolving of initializer lists?](#initializerLists)
    * [How to implement control structures?](#controlStructures)
    * [How to implement constants and constant expressions?](#constants)
    * [How to implement callables like functions, procedures and methods?](#functionsProceduresAndMethods)
    * [How to implement return in a function?](#functionReturn)
    * [How to implement return of a non scalar type from a function?](#functionReturnComplexData)
    * [How to implement the Pascal "WITH", the C++ "using", etc.?](#withAndUsing)
    * [How to implement class inheritance?](#inheritance)
    * [How to implement object oriented polymorphism?](#virtualMethodTables)
    * [How to implement visibility rules, e.g. private,public,protected?](#visibilityRules)
        * [How to report error on violation of visibility rules implemented as types?](#visibilityRuleErrors)
    * [How to implement access of types declared later in the source?](#orderOfDefinition)
    * [How to implement multipass traversal?](#multipassTraversal)
    * [How to handle dependencies between branches of a node in the AST?](#branchDpendencies)
    * [How to implement capture rules of local function definitions?](#localFunctionCaptureRules)
    * [How to implement exception handling?](#exceptions)
    * [How to implement generic programming?](#generics)
    * [How to traverse AST nodes multiple times without the definitions of different traversals interfering?](#multipleTraversal)
    * [How to implement concepts like in C++?](#concepts)
    * [How to implement lambdas?](#lambdas)
    * [How to implement call of C-Library functions?](#cLibraryCalls)


<a name="general"/>

## General
This section of the FAQ answers some broader questions about what one can do with _Mewa_.

<a name="WTF"/>

### What is _Mewa_?
_Mewa_ is a tool for the prototyping of compiler frontends for strongly typed languages.
It aims to support the mapping of a programming language to an intermediate representation in the simplest way leaving all analytical optimization steps to the backend. 

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

**(*)** Unfortunately C++ (and C) can't be implemented with _Mewa_ as there is no separation of syntax and semantic analysis possible in C/C++.
To name one example: There is no way to decide if the statement ```A * B;``` is an expression or a type declaration without having semantical information about A.


<a name="nameMewa"/>

### What does this silly name _Mewa_ stand for?
Name finding is difficult. I got stuck in Polish names of birds without non ASCII characters. _Mewa_ is the Polish name for seagull. 
I think the original idea was that seagulls are a sign of land nearby when you are lost in the sea. Compiler projects are usually a neverending thing (like the sea). With _Mewa_ you see land, meaning that you can at least prototype your compiler. :-)

<a name="decisions"/>

## Decision Questions

<a name="weightedTypeDeduction"/>

### Why are type deduction paths weighted?

 * It helps the type deduction to terminate in reasonable time. 
 * It makes the selection of the deduction path deterministic. 
 * It helps to detect real ambiguity by sorting out solutions with multiple equivalent paths. There must always exist one unique solution to a resolve type query. Otherwise the request is considered to be ambiguous.


<a name="reductionScope"/>

### Why are reductions defined with scope?

At the first glance there is no need for defining reductions with a scope, because the types are already bound to a scope.
But there are rare cases where reductions bound to a scope are useful. One that comes into my mind is private/public access restrictions imposed on the type and not on data. If you want to have private/public access restrictions on type, meaning that in a method of a class you can access the private members of all instances of this class, you can do this by declaring a reduction from the class reference type to its private reference type in every method scope. If you want to have private/public access restrictions on data only, meaning that an object can only access its own data, you can do this by declaring a the ```self``` reference as private without declaring the reduction from the public to the private reference type. 

<a name="problemSolving"/>

## Problem Solving HOWTO
This section of the FAQ gives some recommendations on how to solve specific problems in compilers for strongly typed languages. The hints given here are not meant as instructions. There might be better solutions for the problems presented. Some questions are still open and wait for a good and practical answer.

<a name="astStructure"/>

### How to process the AST structure?
The compiler builds an Abstract Syntax Tree ([AST](ast.md)) with the lexems explicitly declared as leaves and Lua calls bound to the productions as non-leaf nodes. Keywords of the language specified as strings in the productions are not appearing in the _AST_. The compiler calls the topmost nodes of the tree built this way. It is assumed that the tree is traversed by the Lua functions called. The example language uses a function utils.traverse defined in [typesystem_utils.Lua](../examples/typesystem_utils.Lua) for the tree traversal of sub nodes. The traversal function sets the current scope or scope step if defined in the _AST_ and calls the function defined for the _AST_ node.

<a name="astTraversalAndScope"/>

### How to handle lexical scopes?

The _Scope_ (referring to the lexical scope) is part of the type database and represented as pair of integer numbers, the first element specifying the start of the _scope_ and the second the first element after the last element of the _scope_. Every definition (type,reduction,instance) has a _scope_ definition attached. Depending on the current _scope step_ (an integer number) only the subset of definitions having a _scope_ covering it are visible when resolving a type. The current _scope_ and the current _scope step_ are set by the tree traversal function before calling the function attached to a node. The current _scope_ is set to its previous value after calling the function attached to a node. _Scope_ is usually used to envelop code blocks. Substructures on the other hand are preferrably implemented with a context type attached to the definition. So class or structure member is defined as type with a name and the owner is attached as context type to it. Every resolve type query can contain a set of context type candidates.

<a name="scopeInstanceAndAllocators"/>

### How to store and access scope bound data?

_Scope_ bound data in form of a Lua object can be stored with [typedb:set_instance](#set_instance) and retrieved exactly with [typedb:this_instance](#this_instance) and with inheritance (enclosing _scopes_ inherit an object from the parent _scope_) with [typedb:get_instance](#get_instance).

<a name="typeQualifiers"/>

### How to implement type qualifiers like const and reference?

Qualifiers are not considered to be attached to the type but part of the type definition. So a const reference to an integer is a type on its its own, like an integer value type or a pointer to an integer. There are no attributes of types in the typedb, but you can associate attributes with types in Lua tables.

<a name="pointersAndArrays"/>

### How to implement pointers and arrays?

Pointers and arrays are types on its own. Because there are no attributes attached to types in the typedb, you have to declare an array of size 40 as own type that differs from the array of size 20. For arrays you need a mechanism for definition on demand. Such a mechanism you need for generics too.

<a name="namespaces"/>

### How to implement namespaces?

Namespaces are implementd as types. A namespace type can then be used as context type in a [typedb:resolve_type](#resolve_type) call. Every resolve type call has a set of context types or a single context type as argument. 

<a name="initializerLists"/>

### How to implement resolving of initializer lists?

For initializer lists without explicit typeing of the nodes (like for example ```{1,{2,3},{4,5,{6,7,8}}}```) you need some sort of recursive type resolution within a constructor call. In the example _language1_ I defined a const expression type for unresolved structures represented as list of type constructor pairs. The reduction of such a structure to a type that can bew instantiated with structures using type resolution ([typedb:resolve_type](#resolve_type)) and type derivation ([typedb:derive_type](#derive_type)) to build the structures as composition of the structure members. This mechanism can be defined recursively, meaning that the members can be defined as const expression type for unresolved structures themselves. The rule in example _language1_ is that a constructor that returns *nil* has failed and leads to an error or a dropping of the case when using it in a function with the name prefix _try_.

<a name="controlStructures"/>

### How to implement control structures?

Control structure constructors are special because you need to interrupt and terminate expression evaluation as soon as the expression evaluation result is determined. Most known programming languages have this behaviour in their specification. For example in an expression in C/C++ like 
```C
if (expr != NULL && expr->data != NULL) {...}
```
the subexpression ```expr->data != NULL``` is not evaluated if ```expr != NULL``` evaluated to false.

For control structures two new types have been defined in the example _language1_:
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

In the example _language1_ I decided for solution (1) even though it lead to anomalies, but I will return to the problem later. So far all three possibilities can be implemented with _Mewa_.

##### What types to use for representing constant values in the compiler
The decision for data types used to implement constants is more complex as it actually looks like at the first glance. But for implementing constant values you can't rely on the scalar types of the language your implementing a compiler with because the language you design may have a different set of scalar types with a different set of rules. You need arbitrary precision arithmetics for integers and floating point numbers at least. The first example language of _Mewa_ uses a module implementing BCD numbers for arbitrary precision integer arithmetics. A silly decision that should be revised. I just did not find a module for arbitrary precision integer arithmetics on _LuaRocks_  with a working installation at the time. So I decided to activate some older project (implementing arbitrary BCD arithmetics for Lua) as a _LuaRocks_ module on my own. This has to be replaced in the future. But I do not plan to invest a lot of time there. I hope for some contribution or an idea that solves the problem.

<a name="functionsProceduresAndMethods"/>

### How to implement callables like functions, procedures and methods?

This is tricky because of issues around scope and visibility. There are two scopes involved the visibility of the function for callers and the scope of the codeblock that executed the callable. Furthermore we have to do some tricks to do the declaration in a predecessing pass of the code block traversal for enabling recursion (declare the function before its code, so it can be used in the code).
That is why the whole thing is quirky here. See function typesystem.callablebody in typesystem.lua of the example _language1_.

<a name="functionReturn"/>

### How to implement return in a function?

For every function a structure named ```env``` is defined an attached to the scope of the function. In this structure we define the type of the returned value as member ```env.returntype```. A return reduces the argument to the type specified as return value type and returns it.

<a name="functionReturnComplexData"/>

### How to implement return of a non scalar type from a function?

LLVM has the attribute ```sret``` for structure pointers passed to functions for storing the return value for non scalar values.
In the example _language1_ I defined the types

 * rval_ref with the qualifier "&&" representing an R-value reference.
 * c_rval_ref with the qualifier "const&&" representing a constant R-value reference.
 
 and an own kind of constructors for these types. The ```out``` member of the constructor references a variable that can be substituted with the reference of the type this value is assigned to. This allowes to inject the destination address later in the process, after the constructor code has been built. It allows to implement copy elision and to instantiate the target of the constructor in the assignment of the R-value reference to a reference. Or to later allocate a local variable for the structure and to instantiate the target there, if the R-value reference is reduced to a value reference instead.
 
 This mechanism allows to treat return value references passed as parameters uniformly with scalar return values, just using different constructors representing these types.
 

<a name="withAndUsing"/>

### How to implement type contexts, e.g. the Pascal "WITH", the C++ "using" and friends?

In the example _language1_ I define a context list as object bound to a scope. Enclosed scopes inherit the context list from the parent scope.
A "WITH" or "using" defines the context list for the current scope as copy of the inherited context list if not defined yet. It then attaches the
new context member to it.

<a name="inheritance"/>

### How to implement class inheritance?

You define the inherited class as member and define a reduction from the inheriting class to the inherited class with the loading of this member as constructor.


<a name="virtualMethodTables"/>

### How to implement object oriented polymorphism?

In the example _language1_ I implemented interface inheritance only. You have to implement a VMT ([Virtual Method Table](https://en.wikipedia.org/wiki/Virtual_method_table)) and implement the method call as _VMT_ call. For this it is appropriate to parse all methods and put them into lists before processing them and creating the code. Depending on the members of the method lists yout can them decide wheter a _VMT_ has to be created at all.

Implementing polymorphism with virtual methods and class inheritance is difficult. The problems start with multiple inheritance. The ```self``` pointer to be passed to the implementation of a virtual method has to be the one of the first class defining the method as virtual and thus defining the VMT where the method is declared in. When encountering inheritance of two base classes, then the 3 base pointers can't be aligned. Therefore each method implementation has to calculate its ```self``` pointer from the base pointer passed, that is by contract the base pointer of the first class implementing a method. This is not the only problem. I will consider to think about implementing class polymorphism besides interface polymorphism implemented in the example _language1_, if someone succeeds to explain me why it is useful.


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
To implement multipass traversal where we declare the sub classes in the first pass and the member variables in the second pass, we pass the index of the pass as additional traversal argument and check it in the sub node functions. In the example _language1_ I implemented multipass traversal for class definitions.


<a name="multipassTraversal"/>

### How to implement multi pass traversal?

Sometimes some definitions have to be prioritized, e.g. member variable definitions have to be known before translating member functions.
_Mewa_ does not support multipass traversal by nature, but you can implement it by passing additional parameters to the traversal routine.
In the example grammar I attached a pass argument for the ```definition``` Lua callback:
```
inclass_definition	= typedefinition ";"		    (definition 1)
	        		| variabledefinition ";"        (definition 2)
			        | structdefinition              (definition 1)
			        | classdefinition               (definition 1)
			        | interfacedefinition           (definition 1)
			        | constructordefinition         (definition_decl_impl_pass 3)
			        | operatorordefinition          (definition_decl_impl_pass 3)
			        | functiondefinition            (definition_decl_impl_pass 3)
			        ;
free_definition		= struct_definition
        			| functiondefinition			(definition 1)
		        	| classdefinition           	(definition 1)
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
The declaration of functions, operators and methods of classes gets even more complicated. In the example _language1_ we define the typesystem method ```definition_decl_impl_pass``` that is executed in two passes. The first pass declares the callable. The second pass creates its implementation. The reason for this is the possibility of the methods to call each other without beeing declared in an order of their dependency as this is not always possible and not intuitive.

<a name="branchDpendencies"/>

### How to handle dependencies between AST branches of a node in the AST?

As you have the tree traversal in your hands (a nice saying of having to do it on your own), you can partially traverse the subnodes of an AST node. You can also combine a partial traversal with a multipass traversal to get some info out of the AST subnodes before doing the real job. 

There are no limitations, but the model of _Mewa_ punishes partial traversal and multipass traversal with bad readability. So you should avoid to use it extensively. 
Use it selectively and only if there is no other possibility and try to document it well.

In the example **language1** of _Mewa_ I use partial traversal (utils.traverseRange) in callable definitions to make the declaration available in the body to allow self reference. Furthermore I use it in the declaration of inheritance. 
Multipass traversal is used in **language1** to parse class and structure members, subclasses and substructures before method declarations in order to make them accessible in the methods independent from the order or definition.

Sharing data between passes is encouraged by the possibility to attach data to the _AST_. _Mewa_ event reserves one preallocated entry per node for that. I would not attach complex structures though as this makes the _AST_ as output unreadable. In the example _language1_ I attach an id as integer to a node that has data shared by multiple passes and I use this id to identify the data attached.

<a name="visibilityRules"/>

### How to implement visibility rules, e.g. private,public,protected?

Visibility is also best expressed with type qualifiers of the structure types with members having privileged access. In the example **language1** I define the types 

 * ```priv_rval``` with the qualifier "private &" with the reduction to ```rval``` with the qualifier "&"
 * ```priv_c_rval``` with the qualifier "private const&" with the reduction to ```c_rval``` with the qualifier "const&"

for each class type.

Private methods are then defined in the context of ```priv_rval``` or ```priv_c_rval``` if they are const. 
Public methods in the contrary are then defined in the context of ```rval``` or ```c_rval``` if they are const. 

In the body of a method the implicitly defined ```self``` reference is set to be reducible to its private reference type.
The ```self``` is also added as private reference to the context used for resolving types there, so it does not have to be explicitely defined.
If defined like this, private members are accessible from the private context, in the body of methods. Outside, in the public context, private members are not accessible, because there exist no reduction from the public context type to the private context type.

The example _language1_ implements private/public access restrictions on type. This means that a method can access the private data of another instance of the same class like for example in C++.


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
To prevent a direct access of the variable ```a``` in the function ```b``` the scope inclusion rules are not enough. In the example language the access of local variables is filtered by checking if the variable is defined in the global scope or inside the scope of the function ```b```. This can be done with the one liner:
```Lua
if variableScope[1] == 0 or env.scope[1] <= variableScope[1] then ...OK... else ...ERROR... end
```
where variableScope scope is the value of ```typedb:type_scope``` of the variable type and env.scope the procedure/function declaration scope.


<a name="exceptions"/>

### How to implement exception handling?

Exception handling and especially zero-cost exception handling is a subject of further investigation in _Mewa_. As I broadly understood it you define a block with destructors to be executed in case of a thrown exception and LLVM builds the tables needed. This can be be implemented without any support needed from _typedb_.


<a name="templates"/>

### How to implement generic programming?

Generics (e.g. C++ templates) are a form of lazy evaluation. This is supported by _Mewa_ as I can store a subtree of the AST and associate it with a type instead of directly evaluating it.

If a template type ```TPL<A1,A2,...>``` is referenced we define a type ```T = TPL<t1,t2,...>``` with ```A1,A2,...``` being substitutes for ```t1,t2,...```.
For each template argument ```Ai``` without a value constructor attached, we make a [typedb:def_type_as](#def_type_as) definition to define ```(context=T,name=Ai)``` as type ```ti```. For types with an own constructor representing the value, we define a type ```Ai``` with the constructor and a reduction from this type to the generic argument type. With ```T``` as context type we then call the traversal of the generic class, structure or function. 

The lazy evaluation used in generics requires multiple traversal of the same AST node. The types defined in each traversal may differ and definitions with context type 0 (free locals, globals) may interfere. How to handle this is answered [here](#multipleTraversal).

How to do automatic template argument deduction for generic functions is an open question for me. I did not implement it in the example _language1_.
But I think for automatic template argument deduction you have to synthesize the generic parameter assignments somehow by matching the function parameters against the function candidates. If you got a complete set of generic parameter assignments, you can do the kind of expansion described above.


<a name="multipleTraversal"/>

### How to traverse AST nodes multiple times without the definitions of different traversals interfering?

Generics use definition scopes multiple times with differing declarations inside the scope. These definitions interfere. To separate the definitions I suggest to define a type named 'local &lt;generic name&gt;' and define this type as global variable to use as context type for local variables inside this scope. In the example language I use a stack for the context types used for locals inside a generic scope, resp. during the expansion of a generic as these generic expansions can pile up (The expansion can lead to another generic expansion, etc.).


<a name="concepts"/>

### How to implement concepts like in C++?

I did not implement concepts in the example _langauge1_. But I would implement them similarly to generic classes or structures. The AST node of the concept is kept in the concept data structure and traversed with the type to check against the concept passed down as context type. In the concept definition AST node functions you can use typedb:resolve_type to retrieve properties as types to check as sub condition of the concept.

In other words, the concept is implemented as generic with a the type to check the concept against as argument, with the difference that it doesn't produce code. It is just used to check if the concept structure can be succesfully traversed with the argument type.

<a name="lambdas"/>

### How to implement lambdas?

This is an open issue. One idea would be to pass the lambda as type that refers to a AST node that is traversed when referenced. It's related to generics, it is a form of evaluation on demand too, but not exactly the same thing.

<a name="cLibraryCalls"/>

### How to implement the call of a C-Library function?

LLVM supports extern calls. In the specification of the example _language1_ I support calls of the C standard library.


