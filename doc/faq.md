# FAQ

1. [General](#general)
    * [What is _Mewa_](#WTF)
    * [What audience is targeted by _Mewa_?](#targetAudience)
    * [What types of grammars are covered by _Mewa_?](#coveredGrammarClasses)
    * [Why only an LALR(1) parser generator?](#onlyLALR1)
    * [What types of languages are covered by _Mewa_?](#coveredLanguageClasses)
    * [What does this silly name _Mewa_ stand for?](#nameMewa)
    * [What are the hacks in the implementation of the example language1?](#hacks)
1. [Decision Questions](#decisions)
    * [Why are type deduction paths weighted?](#weightedTypeDeduction)
    * [Why are reductions defined with scope?](#reductionScope)
1. [Problem Solving HOWTO](#problemSolving)
    * [How to process the AST structure?](#astStructure)
    * [How to handle lexical scopes?](#astTraversalAndScope)
    * [How to define a schema of reduction weights?](#reductionWeightSchema)
    * [How to compare matching function candidates with more than one parameter?](#functionWeighting)
    * [How to store and access scope-bound data?](#scopeInstanceAndAllocators)
    * [How to debug/trace the _Mewa_ functions?](#tracing)
    * [How to implement type qualifiers like const and reference?](#typeQualifiers)
    * [How to implement pointers and arrays?](#pointersAndArrays)
    * [How to implement namespaces?](#namespaces)
    * [How to implement the resolving of initializer lists?](#initializerLists)
    * [How to implement control structures?](#controlStructures)
    * [How to implement constants and constant expressions?](#constants)
    * [How to correctly implement (int * float)?](#promoteCall)
    * [How to implement callables like functions, procedures and methods?](#functionsProceduresAndMethods)
    * [How to implement return in a function?](#functionReturn)
    * [How to implement return of a non-scalar type from a function?](#functionReturnComplexData)
    * [How to implement the Pascal "WITH", the C++ "using", etc...?](#withAndUsing)
    * [How to implement class inheritance?](#inheritance)
    * [How to implement method call polymorphism?](#virtualMethodTables)
    * [How to implement visibility rules, e.g. private, public, protected?](#visibilityRules)
	* [How to report error on violation of visibility rules implemented as types?](#visibilityRuleErrors)
    * [How to implement access of types declared later in the source?](#orderOfDefinition)
    * [How to implement multipass AST traversal?](#multipassTraversal)
    * [How to handle dependencies between branches of a node in the AST?](#branchDependencies)
    * [How to implement capture rules of local function definitions?](#localFunctionCaptureRules)
    * [How to implement exception handling?](#exceptions)
    * [How to implement generics?](#generics)
       * [How to deduce generic arguments from a parameter list?](#genericsParameterDeduction)
    * [How to traverse AST nodes multiple times without the definitions of different traversals interfering?](#multipleTraversal)
    * [How to implement concepts like in C++?](#concepts)
    * [How to implement lambdas?](#lambdas)
    * [How to implement calls of C-Library functions?](#cLibraryCalls)
    * [How to implement coroutines?](#coroutines)
    * [How to implement copy elision?](#copyElision)
    * [How to implement reference counting on use as in Swift?](#referenceCounting)
    * [What about optimizations?](#optimizations)


<a name="general"/>

## General
This section of the FAQ answers some broader questions about what one can do with _Mewa_.

<a name="WTF"/>

### What is _Mewa_?
_Mewa_ is a tool for the prototyping of compiler frontends for statically typed languages.
It aims to support the mapping of a programming language to an intermediate representation in the simplest way leaving all analytical optimization steps to the backend.

<a name="targetAudience"/>

### What audience is targeted by _Mewa_?
**_Mewa_ is for people with some base knowledge about how parser generators work (*).**

**(*)** There have not yet been many efforts done to assist users in case of conflicts in a grammar. The conflicts are detected and reported and the building of the parser fails in consequence, but there is no mechanism implemented for autocorrection and there is no deeper analysis made by the _Mewa_ parser generator program.

<a name="coveredGrammarClasses"/>

### What types of grammars are covered by _Mewa_?
_Mewa_ is currently based on its own implementation of an **LALR(1)** parser generator.

<a name="onlyLALR1"/>

### Why only an LALR(1) parser generator?
_Mewa_ is not first and foremost about parser generators. The main focus is currently on other problems. For a tool for prototyping languages, it is not the most important thing to be able to cover many language classes. At least not yet. This is more of an issue when dealing with legacy languages. But here, the author of the grammar is most likely also the author of the compiler.
But I am open to alternatives or extensions to cover more classes of grammars. I don't say that it isn't important. It's just not a forefront issue yet.

<a name="coveredLanguageClasses"/>

### What types of languages are covered by _Mewa_?
**_Mewa_ was been designed to be capable to implement statically typed, general-purpose programming languages having a LALR(1) grammar definition.**
It is not recommended to use _Mewa_ for processing other than compilable, statically typed programming languages, because it was not designed for other language classes.
To define a type system as a graph of types and reductions within the hierarchical concept of scope in this form makes no sense for other language classes.

<a name="nameMewa"/>

### What does this silly name _Mewa_ stand for?
Name finding is difficult. I got stuck in Polish names of birds without non-ASCII characters. _Mewa_ is the Polish name for seagull.
I think the original idea was that seagulls are a sign of land nearby when you are lost in the sea. Compiler projects are usually a neverending thing (like the sea). With _Mewa_ you see land, meaning that you can at least prototype your compiler. :-)

<a name="hacks"/>

### What are the hacks in the implementation of the example language1?
Some may say that the whole example **language1** is a big hack because of the information entanglement without contracts all over the place. I cannot say much against that argument. _Mewa_ is not optimized for collaborative work. What I consider hacks here are violations or circumventions of the core ideas of _Mewa_. Here are bad hacks I have to talk about:
 1. *Stateful constructors*: Constructors have an initialization state that tells how many of the members have been initialized. One principle of _Mewa_ is that every piece of information is related to what is stored in the _typedb_ or in the _AST_ or somehow related to that, stored in a _Lua_ table indexed by a value in the _typedb_ or the _AST_. Having a state variable during the traversal of the _AST_ and the code generation is considered bad and a hack. Unfortunately, I don't have any idea to get around the problem differently.
 2. *Cleaning up of partially constructed objects*: This problem caught me on the wrong foot, especially the building of arrays from initializer lists. The solution is awkward and needs to be revisited.

<a name="decisions"/>

## Decision Questions

<a name="weightedTypeDeduction"/>

### Why are type deduction paths weighted?

 * It makes the selection of the deduction path deterministic.
 * It helps to detect real ambiguity by sorting out solutions with multiple equivalent paths. There must always exist one unique solution to a resolve type query. The request is considered to be ambiguous otherwise.


<a name="reductionScope"/>

### Why are reductions defined with scope?

At the first glance, there seems no need to exist for defining reductions with a scope, because the types are already bound to a scope.
But there are rare cases where reductions bound to a scope are useful. One that comes into my mind is private/public access restrictions imposed on the type and not on data. Private/public access restrictions on a type (meaning that in a method of a class you can access the private members of all instances of this class) can be implemented with a reduction from the class reference type to its private reference type in every method scope.

<a name="problemSolving"/>

## Problem Solving HOWTO
This section of the FAQ gives some recommendations on how to solve specific problems in compilers for statically typed languages. The hints given here are not meant as instructions. There might be better solutions for the problems presented. Some questions are still open and wait for a good and practical answer.

<a name="astStructure"/>

### How to process the AST structure?
The compiler builds an Abstract Syntax Tree ([AST](ast.md)) with the lexemes explicitly declared as leaves and _Lua_ calls bound to the productions as non-leaf nodes. Keywords of the language specified as strings in the productions are not appearing in the _AST_. The compiler calls the topmost nodes of the tree built this way. It is assumed that the tree is traversed by the _Lua_ functions called. The example language uses a function utils.traverse defined in [typesystem_utils.lua](../examples/typesystem_utils.lua) for the tree traversal of sub-nodes. The traversal function sets the current scope or scope-step if defined in the _AST_ and calls the function defined for the _AST_ node.

<a name="astTraversalAndScope"/>

### How to handle lexical scopes?

The _scope_ (referring to the lexical scope) is part of the type database and represented as pair of integer numbers, the first element specifying the start of the _scope_ and the second the first element after the last element of the _scope_. Every definition (type, reduction, instance) has a _scope_ definition attached. Depending on the current _scope-step_ (an integer number) only the definitions having a _scope_ covering it are visible when resolving or deriving a type. The current _scope_ and the current _scope-step_ are set by the tree traversal function before calling the function attached to a node. The current _scope_ is set to its previous value after calling the function attached to a node. The _scope_ is usually used to envelop code blocks. Substructures on the other hand are preferably implemented with a context-type attached to the definition. So class or structure member is defined as type with a name and the owner is attached as context-type to it. Every resolve type query can contain a set of context-type candidates.

<a name="reductionWeightSchema"/>

### How to define a schema of reduction weights?

If there is anything alien in _Mewa_, it's the assignment of weights to productions to memorize the preference of type deduction paths.
We talk about twenty lines of floating-point number assignments here. But weird ones. They empower _Mewa_ to use the shortest path search algorithm for resolving and deriving types, which is the key to the stunning performance of _Mewa_.

When thinking about the weight-sum of a path, I imagine it as the summation of its parts. Each part represents one type of action, like removing a qualifier, adding const, converting a value, etc... Now I describe my rules like the following:

 * If I have to add _const_ I want to do it as soon as possible
 * If I need to load a value from a reference type I do it as soon as possible, but after adding _const_
 * If I need to do a value conversion, I do it as late as possible
 * Value conversion somehow weight-in the amount of information they destroy

When we have written down our rules, then we think about memorizing them in the weight-sum. Let's take the first one, adding _const_. The "as early as" possible measure requires a quantization of the "when". A quantization could be the number of qualifiers attached to the source type. Because _const_ is always taken away (except in an error reduction reporting illegal use of a type), we count the non-const as a qualifier. The example language1 enumerates the number of qualifiers up to 3. For the reductions adding const we could define 3 weights:
 * rdw_const_1 = 0.5 * (1/(1*1))
 * rdw_const_2 = 0.5 * (1/(2*2))
 * rdw_const_3 = 0.5 * (1/(3*3))

The weight of adding const is here the smallest, when applied of a type with the maximum cardinality of qualifiers.
For the second case, load a value from a reference we do implement a similar schema. But the base weight is chosen smaller than 0.5. Let's take 0.3:
 * rdw_load_1 = 0.3 * (1/(1*1))
 * rdw_load_2 = 0.3 * (1/(2*2))
 * rdw_load_3 = 0.3 * (1/(3*3))

In a weight-sum of both, the adding const is applied before loading the value. But both are applied as soon as possible.
You will continue this process of assigning the weights yourself. It will take some time and will cost some nerves. But the result will be stable without surprises after a while.

In the utils helper module there are functions to trace the shortest path search of ```typedb:resolve_type (utils.getResolveTypeTrace)``` and ```typedb:derive_type (utils.getDeriveTypeTrace)``` to find bugs in your weighting schema.

<a name="functionWeighting"/>

### How to compare matching function candidates with more than one parameter?
To weigh matching function candidates against each other we have to find a way to accumulate weights resulting from the matching of the parameters.
To take the weight sum is a bad idea. Consider the example
```
func( 1, 2, 0.7)
```
Which one of the following is the better candidate:
```
fn func( integer A, integer B, integer C)
```
or
```
fn func( float A, float B, float C)
```
If we weight loss of information, we have to weight the conversion ```float -> integer``` higher than ```integer -> float```.
(Both conversions potentially lose information, but the first one is worse in this regard).

So the second function is our candidate. The problem is that with a weight sum as accumulator of weights and with a sufficient number of
parameters, the first choice can potentially overrun the second one.

In the example **language1**, I chose the maximum of all parameter matching weights as accumulating function.
A clash of two functions with the same maximum is ambiguous. You could also take another measure than the bare maximum.
But I think the maximum has to be the first measure. A sum of weights, if taken into account, should be capped with a normalization.


<a name="scopeInstanceAndAllocators"/>

### How to store and access scope-bound data?

_Scope_ bound data in form of a _Lua_ object can be stored with [typedb:set_instance](#set_instance) and retrieved exactly with [typedb:this_instance](#this_instance) and implying inheritance (enclosing _scopes_ inherit an object from the parent _scope_) with [typedb:get_instance](#get_instance).
Examples of scope bound data are
 * Structure for managing allocated variables (garbage collecting on scope exit). In the example **language1**, I  call this the **Allocation Frame**.
 * Data of compilation units like functions (evaluate the return type the calling return). In the example **language1**, I  call this the **Callable Environment**.
 * Node data of scopes to walk trough a definition scope (check the usage of a return variable to determine if [copy elision](#copyElision) is possible). In the example **language1**, I store every node with a scope as instance with the name "node".

<a name="tracing"/>

### How to debug/trace the _Mewa_ functions?

A developer of a compiler front-end with _Lua_ using _Mewa_ should not have to debug a program with a C/C++ debugger if something goes wrong.
Fortunately, the _Mewa_ code for important functions like typedb:resolve_type and typedb:derive_type is simple. I wrote parallel implementations in _Lua_ that do the same. The _typedb_ API has been extended with convenient functions that make such parallel implementations possible. The following functions are in examples/typesystem_utils.lua:

 * utils.getResolveTypeTrace( typedb, contextType, typeName, tagmask) is the equivalent of typedb:resolve_type
 * utils.getDeriveTypeTrace( typedb, destType, srcType, tagmask, tagmask_pathlen, max_pathlen) is the equivalent of typedb:derive_type

 Both functions are not returning a result though, but just printing a trace of the search and returning this trace as a dump.

Besides that, there are two functions in examples/typesystem_utils.lua that dump the scope tree of definitions:
 * function utils.getTypeTreeDump( typedb) dumps all reductions defined, arranges the definitions in a tree where the nodes correspond to scopes
 * function utils.getReductionTreeDump( typedb) dumps all types defined, arranges the definitions in a tree where the nodes correspond to scopes

I must admit to having rarely used the tree dump functions. They are tested though. I found that the trace functions for typedb:resolve_type and typedb:derive_type showed to be much more useful in practice.


<a name="typeQualifiers"/>

### How to implement type qualifiers like const and reference?

Qualifiers are not considered to be attached to the type but part of the type definition. A const reference to an integer is a type on its own, like an integer value type or an integer reference type.

<a name="pointersAndArrays"/>

### How to implement pointers and arrays?

In the example **language1**, pointers and arrays are types on their own. Both types are created on-demand. The array type is created when used in a declaration. A pointer is created when used in a declaration or as a result of an '&' address operator on a pointee reference type.
An array of size 40 has to be declared as a separate type that differs from the array of size 20. In this regard, arrays are similar to generics. But the analogies stop there.

<a name="namespaces"/>

### How to implement namespaces?

Namespaces are implemented as types. A namespace type can then be used as context-type in a [typedb:resolve_type](#resolve_type) call. Every resolve type call has a set of context-types or a single context-type as an argument.

<a name="initializerLists"/>

### How to implement resolving of initializer lists?

For initializer lists without explicit typing of the nodes (like for example ```{1,{2,3},{4,5,{6,7,8}}}```) some sort of recursive type resolution is needed within a constructor call. In the example **language1**, I defined a const expression type for unresolved structures represented as a list of type constructor pairs. The reduction of such a structure to a type that can be instantiated with structures using type resolution ([typedb:resolve_type](#resolve_type)) and type derivation ([typedb:derive_type](#derive_type)) to build the structures as a composition of the structure members. This mechanism can be defined recursively, meaning that the members can be defined as const expression type for unresolved structures themselves. The rule in example **language1** is that a constructor that returns *nil* has failed and leads to an error or a dropping of the case when using it in a function with the name prefix _try_.

<a name="controlStructures"/>

### How to implement control structures?

Control structure constructors are special because expression evaluation has to be interrupted as soon as the expression evaluation result is determined. Most known programming languages have this behavior in their specification. For example in an expression in C/C++ like
```C
if (expr != NULL && expr->data != NULL) {...}
```
the subexpression ```expr->data != NULL``` is not evaluated if ```expr != NULL``` evaluated to false.

For control structures two new types have been defined in the example **language1**:
1. Control-True Type that represents a constructor as a structure with 2 members:
    1. **code** containing the code that is executed in the case the expression evaluates to **true** and
    1. **out** the name of an unbound label (not defined in the code yet) where the evaluation jumps to in the case the expression evaluates to **false** and
1. Control-False Type
    1. **code** containing the code that is executed in the case the expression evaluates to **false** and
    1. **out** the name of an unbound label (not appearing in the code yet) where the evaluation jumps to in the case the expression evaluates to **true**.

An expression with the operator ```||``` is evaluated to a control-false type.
An expression with the operator ```&&``` is evaluated to a control-true type.
This is because the behavior has also to apply to expressions without control structures like the C99/C++ example
```C
bool hasData = (expr != NULL && expr->data != NULL);
```
Control true/false types are convertible into each other.
Depending on the control structure, either control-true type or control-false type is required.

<a name="constants"/>

### How to implement constants and constant expressions?
##### Propagation of Types
The constructors of constant values are implemented as _Lua_ userdata. Expressions are values calculated immediately by the constructor functions.
The propagation of types is a more complex topic:
```
byte b = 1
int a = b + 2304879
```
What type should the expression on the right side of the second assignment have?
 1. We can reduce ```2304879``` to the type of 'b' before the addition
 1. We can promote 'b' to a maximum size integer type and so ```2304879``` before the addition
 1. We can propagate the type ```int``` of the left side of the assignment to the expression ```b + 2304879``` as result type.

In the example **language1**, I decided on the solution (1) even though it lead to anomalies, but I will return to the problem later. So far all three possibilities can be implemented with _Mewa_.

##### What types to use for representing constant values in the compiler
The decision for data types used to implement constants is more complex than it looks at a first glance. To rely on the scalar types of the language of the compiler is wrong. The language of the compiler may have a different set of scalar types with a different set of rules.
Arbitrary precision arithmetics for integers and floating-point numbers are required at least. The first example language of _Mewa_ uses a module implementing BCD numbers for arbitrary precision integer arithmetics. This should be changed in the future.


<a name="promoteCall"/>

### How to correctly implement (int * float)?
In the example **language1**, operators are defined with the left operand as context type. Mewa encourages such a definition. Defining operators as free functions could not make much use of the type resolving mechanism provided by the _typedb_.
But (int * float) should be defined as the multiplication of two floating-point numbers. For this, the multiplication of an integer on the left side with a floating-point number on the right side has to be defined explicitly. In the example **language1** these definitions are named **promote calls**. The left side argument is promoted to the type on the right side before applying the operator defined for two floating-point number arguments.


<a name="functionsProceduresAndMethods"/>

### How to implement callables like functions, procedures, and methods?

Because modern programming languages treat callables as types, we need some sort of unification of all cases to call a function.
In the example **language1** a callable type is declared as a type with a "()" operator with the function arguments as argument types.
This way all forms of a call can be treated uniformly on application.

The implementation of callables has some issues around scope and visibility. There are two scopes involved the visibility of the function for callers
and the scope of the code block of the callable. Therefore I have to do some declaration of the current scope manually at some place.
A callable declaration declares the parameters similarly to variables in the scope of its body. But the parameters are also used as part of the
function declaration in the outer scope.

Furthermore, we have to do traverse the declaration and the implementation in two different passes. The function is declared in one pass,
the body is traversed and the code generated in another pass. This makes the function known before its body is traversed. Recursion becomes possible.

See the function ```typesystem.callablebody``` in ```typesystem.lua``` of the example **language1**.
See also [How to implement multipass AST traversal](#multipassTraversal).

There are some differences between methods and free functions and other function types, mainly in the context type and the
passing of the ```self``` parameter and maybe the LLVM IR code template. But the things shared overweigh the differences.
I would suggest using the same mapping for both.

<a name="functionReturn"/>

### How to implement return in a function?

For every function, a structure named ```env``` is defined and attached to the scope of the function. In this structure, we define the type of the returned value as member ```env.returntype```. A return reduces the argument to the type specified as a return value type and returns it.

<a name="functionReturnComplexData"/>

### How to implement the return of a non-scalar type from a function?

LLVM has the attribute ```sret``` for structure pointers passed to functions for storing the return value for non-scalar values.
In the example **language1**, I defined the types

 * unbound_reftype with the qualifier "&<-" representing a reference to an unbound variable.
 * c_unbound_reftype with the qualifiers "const " and "&<-" representing an unbound constant reference.

 The ```out``` member of the unbound reference constructor references a variable that can be substituted with the reference of the type this value is assigned to. This allows injecting the destination address later in the process after the constructor code has been built. This allows to implement copy elision and to instantiate the target of the constructor in the assignment of the unbound reference to a reference. Or to later allocate a local variable for the structure and to instantiate the target there, if the unbound reference is reduced to a value or a const reference instead.

 This mechanism allows treating return value references passed as parameters uniformly with scalar return values, just using different constructors representing these types.


<a name="withAndUsing"/>

### How to implement type contexts, e.g. the Pascal "WITH", the C++ "using", etc...?

In the example **language1**, a context list is an object bound to a scope. The enclosed scopes inherit the context list from the parent scope.
A "WITH" or "using" defines the context list for the current scope as a copy of the inherited context list if not defined yet. It then attaches the
new context member to it.

<a name="inheritance"/>

### How to implement class inheritance?

An inherited class is declared as a member. A reduction from the inheriting class to this member makes the class appearing as an instance of the inherited class.

##### Note
For implementing method call polymorphism see [How to implement method call polymorphism?](#virtualMethodTables).


<a name="virtualMethodTables"/>

### How to implement method call polymorphism?

In the example **language1**, I implemented interface inheritance only. For class inheritance with overriding virtual method declarations possible, a _VMT_ ([Virtual Method Table](https://en.wikipedia.org/wiki/Virtual_method_table)) has to be implemented. A virtual method call is a call via the _VMT_.

Polymorphism with virtual methods has some issues concerning the base pointer of the class. If a method implementation can be overridden, then the base pointer of a class has to be determined by the method itself. Each method implementation has to calculate its ```self``` pointer from the base pointer passed. This could be the base pointer of the first class implementing the method.


<a name="orderOfDefinition"/>

### How to implement access of types declared later in the source?
In C++ types can be referenced even if they are declared later in the source.
```
class A {
	B b;

	class B {
		...
	};
};
```
When relying on a single pass traversal of the AST to emit code, an order of definitions like the following is required:
```
class A {
	class B {
		...
	};
	B b;
};
```
For languages allowing a schema like C++, we need multi-pass AST traversal. See [How to implement multipass AST traversal](#multipassTraversal).


<a name="multipassTraversal"/>

### How to implement multi-pass AST traversal?

Sometimes some definitions have to be prioritized, e.g. member variable definitions have to be known before translating member functions.
_Mewa_ does not support multipass traversal by nature, but you can implement it by passing additional parameters to the traversal routine.
In the example grammar I attached a pass argument for the ```definition``` _Lua_ function call:
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
				| functiondefinition		(definition 1)
				| classdefinition           	(definition 1)
				| interfacedefinition		(definition 1)
				;
```
The Lua function gets 2 additional parameters, one from the grammar: ```pass``` and one from the traversal call: ```pass_selected```
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
The declaration of functions, operators, and methods of classes gets more complicated. In the example **language1**, I define the typesystem method ```definition_decl_impl_pass``` that is executed in two passes. The first pass declares the callable. The second pass creates its implementation. The reason for this is the possibility of the methods to call each other without being declared in an order of their dependency as this is not always possible and not intuitive.


<a name="branchDependencies"/>

### How to handle dependencies between AST branches of a node in the AST?

With the tree traversal in your hands, you can partially traverse the sub-nodes of an AST node. You can also combine a partial traversal with a multipass traversal to get some info out of the AST sub-nodes before doing the real job.

There are no limitations, but the model of _Mewa_ punishes partial traversal and multipass traversal with bad readability.
Use it selectively and only if there is no other possibility and try to document it well.

In the example **language1** of _Mewa_, I use partial traversal (utils.traverseRange) in callable definitions to make the declaration available in the body to allow self-reference. Furthermore, I use it in the declaration of inheritance.
Multipass traversal is used in **language1** to parse class and structure members, subclasses, and substructures before method declarations to make them accessible in the methods independent from the order or definition.

Sharing data between passes is encouraged by the possibility to attach data to the _AST_. _Mewa_ event reserves one preallocated entry per node for that. I would not attach complex structures though as this makes the _AST_ as output unreadable. In the example **language1**, I attach an integer as an id, to a node that has data shared by multiple passes and I use this id to identify the data attached.


<a name="visibilityRules"/>

### How to implement visibility rules, e.g. private, public, protected?

Visibility is also best expressed with type qualifiers of the structure types with members having privileged access. In the example **language1**, I define the types

 * ```priv_rval``` with the qualifier "private &" with the reduction to ```rval``` with the qualifier "&"
 * ```priv_c_rval``` with the qualifier "private const&" with the reduction to ```c_rval``` with the qualifier "const&"

for each class type.

Private methods are then defined in the context of ```priv_rval``` or ```priv_c_rval``` if they are const.
Public methods on the contrary are then defined in the context of ```rval``` or ```c_rval``` if they are const.

In the body of a method, the implicitly defined ```self``` reference is set to be reducible to its private reference type.
The ```self``` is also added as a private reference to the context used for resolving types there, so it does not have to be explicitly defined.
If defined like this, private members are accessible from the private context, in the body of methods. Outside, in the public context, private members are not accessible, because there exists no reduction from the public context-type to the private context-type.

The example **language1** implements private/public access restrictions on the type. This means that a method can access the private data of another instance of the same class like in C++. To make the private members of other instances besides the own ```self``` reference of a class accessible in a method, a reduction from the public reference type to the private reference type is declared in every method scope.


<a name="visibilityRuleErrors"/>

#### How to an report error on violation of visibility rules implemented as types?

For reporting implied but illegal type deductions like writing to a const variable, you can define reductions with a constructor
throwing an error if applied. High weight can ensure that the forbidden path is taken only in the absence of a legal one.

Another possibility is to attach an error attribute to such constructors indicating an error and forward them to referencing constructors.
The evaluation of the best matching candidate type instance can drop instances with an error in the construction.
In the example **language1**, the functions with the prefix **try** would drop candidates with an error attribute in the constructor.

Neither one nor the other method of improving error messages has been implemented in the example **language1**.


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
To prevent direct access of the variable ```a``` in the function ```b``` the scope inclusion rules are not enough. In the example **language1**, the access of local variables is filtered by checking if the variable is defined in the global scope or inside the scope of the function ```b```. This can be done with the one-liner:
```Lua
if variableScope[1] == 0 or env.scope[1] <= variableScope[1] then ...OK... else ...ERROR... end
```
where variableScope scope is the value of ```typedb:type_scope``` of the variable type and env.scope the procedure/function declaration scope.


<a name="exceptions"/>

### How to implement exception handling?

In the example **language1**, I implemented a very primitive exception handling. The only thing throwable is an error code plus optionally a string. Besides simplicity, this solution has also the advantage that exceptions could potentially be thrown across shared library borders without relying on a global object registry as in _Microsoft Windows_.

Every call that can potentially raise an exception needs a label to be jumped at in the case. The first instructions at this label are launching the exception handling and extracting the exception data, storing them into local variables reserved for that. In the following the code goes through a sequence of cleanup calls. After cleanup the exception structure is rebuilt and rethrown with the LLVM 'resume' instruction or the exception is processed. The instructions for launching the exception handling and ending it are different for the two cases. LLVM calls the case of rethrowing the exception with resume 'cleanup' and the case where the exception is processed 'catch'.

The templates I used for implementing it are defined in ```llvmir.exception``` in the _Lua_ module llvmir.lua. I will provide more documentation later.

The most difficult about exception handling is the cleanup. In the example **language1**, I pair every constructor call where a destructor exists with a call registering a cleanup call with the current _scope-step_ in the current _allocation frame_. The _scope-step_ is the identifier that helps to figure out the label to branch to. This label is the start of the chain of cleanup commands. Every cleanup chain of an _allocation frame_ ends with the jump to a label of the enclosing _allocation frame_. For every exit case (return with a specific value, throwing an exception, handling an exception) there exists an own chain of cleanup calls with entry labels for any possible location to come from.

To figure this out was one of the most complicated things in the example **language1**. Expect to spend some time there.

<a name="generics"/>

### How to implement generics?

Generics (e.g. C++ templates) are a form of lazy evaluation. This is supported by _Mewa_ as I can store a subtree of the AST and associate it with a type instead of directly evaluating it.
A generic can be implemented as type. The application of an operator with arguments triggers the creation of a template instance on demand in the scope of the generic.
A table is used to ensure generic type instances are created only once and reused if referenced multiple times.
The instantiation of a generic issues a traversal of the AST node of the generic with a context type declared for this one instance.
The template parameters are declared as types in this context.

In the following, I describe how generics are implemented in the example **language1**.
 1. First, a unique key is created from the template argument list.
    All generic instances are in a map associated with the type. We make a lookup there to see if the generic type instance has already been created. We are done, if yes.
 2. Then we create the generic instance type with the generic type as context type and the unique key as type name.
 3. For the local variables in the scope of the generic, we create a type in a similar way as the generic instance type.
    In the example **language1** a prefix "local " is used for the type name. This type is used as context type for local definitions instead of 0.
    This ensures that local definitions from different AST node traversals are separated from each other.
 4. For each template argument we create a type with the generic instance type as context type.
     * If the generic argument is a data type (has no constructor), we create a type as alias (typedb:def_type_as).
     * If the generic argument is a value type (with a constructor), we create a type (typedb:def_type) with this constructor and a reduction to the passed type.
    The scope of the template argument definition is the scope of the generic AST node.
 5. Finally we add the generic instance type to the list of context types and traverse the AST node of the generic.
    The code for the generic instance type is created in the process.


<a name="genericsParameterDeduction"/>

#### How to deduce generic arguments from a parameter list?
C++ can reference template functions without declaring the template arguments. The template arguments are deduced from the call arguments.
I did not implement this in the example **language1**. Template parameter deduction can be seen as preliminar step before the instantiation of the generic type.
The instantiation itself does not change having template parameter deduction.
For automatic template argument deduction, the generic parameter assignments have to be synthesized by matching the function parameters
against the function candidates. A complete set of generic parameter assignments is evaluated in the process. This list is used
as the generic argument list to refer to the generic instance.


<a name="multipleTraversal"/>

### How to traverse AST nodes multiple times without the definitions of different traversals interfering?

Generics use definition scopes multiple times with differing declarations inside the scope. These definitions interfere. To separate the definitions I suggest defining a type named 'local &lt;generic name&gt;' and define this type as a global variable to use as context-type for local variables inside this scope. In the example **language1** I use a stack for the context-types used for locals inside a generic scope, resp. during the expansion of a generic as these generic expansions can pile up (The expansion can lead to another generic expansion, etc...).


<a name="concepts"/>

### How to implement concepts like in C++?

I did not implement concepts in the example **language1**. But I would implement them similarly to generic classes or structures. The AST node of the concept is kept in the concept data structure and traversed with the type to check against the concept passed down as context-type. In the AST node functions of the concept definition, you can use typedb:resolve_type to retrieve properties as types to check as a sub condition of the concept.

In other words, the concept is implemented as generic with the type to check the concept against as an argument, with the difference that it doesn't produce code. It is just used to check if the concept structure can be successfully traversed with the argument type.

<a name="lambdas"/>

### How to implement lambdas?

In the example **language1**, lambdas are declared like generics.
The node with the code of the lambda is traversed when referenced after the assignment of the parameters.
Because the scope of the lambda is not a subscope of the caller, every parameter creates a type with the name of the lambda parameter and the constructor
of the type instance passed. A reduction is defined to the parameter type.

Because of the multiple uses of the same scope as in generics, lambdas have to define free variables with a context type created for each instance.


<a name="cLibraryCalls"/>

### How to implement calls of C-Library functions?

LLVM supports extern calls. In the specification of the example **language1**, I support calls of the C standard library.


<a name="coroutines"/>

### How to implement coroutines?

Coroutines are callables that store their local variables in a structure instead of allocating them on the stack. This makes them interruptable with a yield that is a return with a state that describes where the coroutine continues when it is called the next time. This should not be a big deal to implement. I will do this in the example **language1** soon.


<a name="copyElision"/>

### How to implement copy elision?

Copy elision, though it's making programs faster is not considered as an optimization, because it changes behavior. Copy elision has to be part of a language specification and thus be implemented in the compiler front-end. In the example **language1**, I implemented two 2 cases of copy elision:
  * Construction of a return value in the return slot (LLVM sret) provided by the caller
  * Construction of a return value in the return slot, when using only one variable for the return value in the variable declaration scope


<a name="referenceCounting"/>

### How to implement reference counting on use as in Swift?

Swift implements reference counting when required. This implies the adding of a traversal pass that notifies how a type (variable) is used.
This pass decides if a reference counting type used for the variable or not. There is no way to get around the traversal pass that notifies the usage.
I suggest to implement other things like copy elision decisions in this pass too if it is required anyway.
I need to think a little bit more about this issue as it is an aspect that is not covered at all in the example "language1".


<a name="optimizations"/>

### What about optimizations?

Other compiler-frontend models are better suited for optimizations. What you can do with _Mewa_ is to attach attributes to code that helps a back-end to optimize the code generated. That is the model LLVM IR encourages.


