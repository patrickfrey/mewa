# FAQ

1. [General](#general)
    * [What is _Mewa_](#WTF)
    * [What audience is targeted by _Mewa_?](#targetAudience)
    * [What types of grammars are covered by _Mewa_?](#coveredGrammarClasses)
    * [Why only an LALR(1) parser generator?](#onlyLALR1)
    * [What types of languages are covered by _Mewa_?](#coveredLanguageClasses)
    * [What does _Mewa_ do differently from the others?](#differenceMewa)
    * [What are the known limitations of _Mewa_ so far?](#limitations)
    * [What does this silly name _Mewa_ stand for?](#nameMewa)
1. [Problem Solving HOWTO](#problemSolving)
    * [How to process the AST structure?](#astStructure)
    * [How to parse off-side rule languages?](#offSideRuleLanguages)
    * [How to handle lexical scopes?](#astTraversalAndScope)
    * [How to define a schema of reduction weights?](#reductionWeightSchema)
    * [How to compare matching function candidates with more than one parameter?](#functionWeighting)
    * [How to store and access scope-bound data?](#scopeInstanceAndAllocators)
    * [How to debug/trace the _Mewa_ functions?](#tracing)
    * [How to implement type qualifiers like const and reference?](#typeQualifiers)
    * [How to implement free variables?](#variables)
    * [How to implement structures?](#structures)
    * [How to implement pointers and arrays?](#pointersAndArrays)
    * [How to implement namespaces?](#namespaces)
    * [How to implement the resolving of initializer lists?](#initializerLists)
    * [How to implement control structures?](#controlStructures)
    * [How to implement constants and constant expressions?](#constants)
    * [How to correctly implement (int * float)?](#promoteCall)
    * [How to implement callables like functions, procedures and methods?](#functionsProceduresAndMethods)
    * [How to implement return from a function?](#functionReturn)
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
    * [How to report shadowing declarations?](#shadowingDeclarations)
    * [How to implement exception handling?](#exceptions)
    * [How to automate the cleanup of data?](#cleanupData)
    * [How to implement generics?](#generics)
       * [How to deduce generic arguments from a parameter list?](#genericsParameterDeduction)
    * [How to traverse AST nodes multiple times without the definitions of different traversals interfering?](#multipleTraversal)
    * [How to implement concepts like in C++?](#concepts)
    * [How to implement lambdas?](#lambdas)
    * [How to implement calls of C-Library functions?](#cLibraryCalls)
    * [How to implement calls with a variable number of arguments?](#varargCalls)
    * [How to implement coroutines?](#coroutines)
    * [How to implement copy elision?](#copyElision)
    * [How to implement reference counting on use as in Swift?](#referenceCountingSwift)
    * [How to implement reference lifetime checking as in Rust?](#referenceLifetimeRust)
    * [What about optimizations?](#optimizations)
1. [Decision Questions](#decisions)
    * [Why are type deduction paths weighted?](#weightedTypeDeduction)
    * [Why are reductions defined with scope?](#reductionScope)
    * [What are the hacks in the implementation of the example language1?](#hacks)


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
You can also [parse off-side rule languages](#offSideRuleLanguages) by converting indentiation into lexems.
It is not recommended to use _Mewa_ for processing other than compilable, statically typed programming languages, because it was not designed for other language classes.
To define a type system as a graph of types and reductions within the hierarchical concept of scope in this form makes no sense for other language classes.


<a name="differenceMewa"/>

### What does _Mewa_ do differently from the others?

Industrial compiler front-ends are building an _AST_ from the source and are processing it in several well-defined passes doing jobs
like type checking, resolving, etc. The _AST_ is enriched and transformed in this process.
Finally, the _AST_ is traversed and the intermediate representation code is generated.
This model has the advantage to scale well with the number of developers involved. It is optimized for collaborative work.
It is also not limited by other constraints than what can be expressed with the structure of the _AST_.

_Mewa_, on the other hand, builds an _AST_ deduced from the grammar. The functions attached to the nodes implement the tree traversal.
Instead of enriching the _AST_, a flat set of structures representing the type system is built. The resulting code is built according to
construction plans derived from this flat set of structures. This imposes a lot more constraints on what you can do and what not.
On the other hand, it can be interesting to think about language constructs in such a minimalistic way.
This makes _Mewa_ a tool suitable for prototyping compiler front-ends.


<a name="limitations"/>

### What are the known limitations of _Mewa_ so far?

* The notion of _scope_ imposes some restrictions on lazy evaluation in the _AST_ traversal (generics, lambdas). Because parameter types do have their scope and so do the relations defined on them, the _scope_ of a parameter type has to cover the _scope_ of the generic or lambda. That is a restriction that applies to _C++_ too. A possible solution could be to define type properties like class methods, structure member accesses, and reductions in the global scope. Because they would be related to the context type anyways, there would be no interference with other types. But I am not sure about the implications.


<a name="nameMewa"/>

### What does this silly name _Mewa_ stand for?
Name finding is difficult. I got stuck in Polish names of birds without non-ASCII characters. _Mewa_ is the Polish name for seagull.
I think the original idea was that seagulls are a sign of land nearby when you are lost in the sea. Compiler projects are usually a neverending thing (like the sea). With _Mewa_ you see land, meaning that you can at least prototype your compiler. :-)


<a name="problemSolving"/>

## Problem Solving HOWTO
This section of the FAQ gives some recommendations on how to solve specific problems in compilers for statically typed languages. The hints given here are not meant as instructions. There might be better solutions for the problems presented. Some questions are still open and wait for a good and practical answer.

<a name="astStructure"/>

### How to process the AST structure?
The compiler builds an Abstract Syntax Tree ([AST](ast.md)) with the lexemes explicitly declared as leaves and _Lua_ calls bound to the productions as non-leaf nodes. Keywords of the language specified as strings in the productions are not appearing in the _AST_. The compiler calls the topmost nodes of the tree built this way. It is assumed that the tree is traversed by the _Lua_ functions called. The example language uses a function utils.traverse defined in [typesystem_utils.lua](../examples/typesystem_utils.lua) for the tree traversal of sub-nodes. The traversal function sets the current scope or _scope-step_ if defined in the _AST_ and calls the function defined for the _AST_ node.

<a name="offSideRuleLanguages">

### How to parse off-side rule languages?

The command **INDENTL** in the [definition of the grammar](grammar.md) allows to define lexemes issued for indentiation and new line.
These lexemes can be referred to in the productions of the grammar.
By converting new line and indentiation signals into end of command and brackets, you can implement off-side rule languages as if they were
languages with operators marking the end of a command and open/close brackets marking blocks defined by indentiation in the original source.

<a name="astTraversalAndScope"/>

### How to handle lexical scopes?

The _scope_ (referring to the lexical scope) is part of the type database and represented as pair of integer numbers, the first element specifying the start of the _scope_ and the second the first element after the last element of the _scope_. Every definition (type, reduction, instance) has a _scope_ definition attached. Depending on the current _scope-step_ (an integer number) only the definitions having a _scope_ covering it are visible when resolving or deriving a type. The current _scope_ and the current _scope-step_ are set by the tree traversal function before calling the function attached to a node. The current _scope_ is set to its previous value after calling the function attached to a node. The _scope_ is usually used to envelop code blocks. Substructures on the other hand are preferably implemented with a context-type attached to the definition. So class and structure members are defined as types with a name and the owner is defined as its context-type. Every resolve type query can contain a set of context-type candidates.

<a name="reductionWeightSchema"/>

### How to define a schema of reduction weights?

If there is anything alien in _Mewa_, it's the assignment of weights to reductions to memorize the preference of type deduction paths.
We talk about twenty lines of floating-point number assignments here. But weird ones. They empower _Mewa_ to rely only on the shortest path search algorithm (_Dijkstra_) for resolving and deriving types.
This is the key to the stunning performance of _Mewa_.

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
Relations between types like const and non-const are expressed by reductions. A non-const type is reducible to const. The reducibility allowes non-const
types to be passed to functions that ask for a const parameter.


<a name="variables"/>

### How to implement free variables?

Free variables are types declared with their name as name and 0 as context type. In the scope of generics a dedicated type is used as context type instead of 0 to separate the definition realms for unrelated type definitions not to interfere.
The data type of a variable is delared with a reduction of the variable type to its data type.
With this reduction declared, we can use the variable in expressions, relying on its implicit deduction and construction as an instance of its associated data type.


Member variables are declared similarly, with the owning structure type as context type. See [structures](#structures).


<a name="structures"/>

### How to implement structures?

Structures are implemented as types. A member access of a structure is implemented as type built with the reference type of the structure as
context type and the member name as type name. The structure declaration starts with the declaration of the reference type of the structure.
During the traversal of the _AST_ nodes of the structure members, the member access types are built.

Substructure types are declared with the structure type as context type.


<a name="pointersAndArrays"/>

### How to implement pointers and arrays?

In the example **language1**, pointers and arrays are types on their own. Both types are created on-demand. The array type is created when used in a declaration. A pointer is created when used in a declaration or as a result of an '&' address operator on a pointee reference type.
An array of size 40 has to be declared as a separate type that differs from the array of size 20. In this regard, arrays are similar to generics. But the analogies stop there.


<a name="namespaces"/>

### How to implement namespaces?

Namespaces are implemented as types. A namespace type can then be used as context-type in a [typedb:resolve_type](#resolve_type) call.
Every resolve type call has a set of context-types or a single context-type as an argument.

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
The negation of a control true/false type is created by flipping the type to its counterpart without changing the constructor.
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
and the scope of the code block of the callable. Therefore, we have to do some declarations of the current scope manually at someplace.
A callable declaration declares the parameters similarly to variables in the scope of its body. But the parameters are also used as part of the
function declaration in the outer scope.

Furthermore, we have to traverse the declaration and the implementation in two different passes. The function is declared in one pass,
the body is traversed and the code is generated in another pass. This makes the function known before its body is traversed. Recursion becomes possible.

See the function ```typesystem.callablebody``` in ```typesystem.lua``` of the example **language1**.
See also [How to implement multipass AST traversal](#multipassTraversal).

There are some differences between methods and free functions and other callable types, mainly in the context type and the
passing of the ```self``` parameter and maybe the LLVM IR code template. But the things shared overweigh the differences.
I would suggest using the same mapping for both.

<a name="functionReturn"/>

### How to implement return from a function?

For every function, a structure named ```env``` is defined and attached to the scope of the function. In this structure, we define the type of the returned value as member ```env.returntype```. A return reduces the argument to the type specified as a return value type and returns it.

In the case of having destructor chains of allocated objects to be called first, the situation gets a little bit more complicated.
In this case the return of a value is one scenario for exit from the current allocation frame.
To get deeper into exit scenarios, see [How to automate the cleanup of data?](#cleanupData).


<a name="functionReturnComplexData"/>

### How to implement the return of a non-scalar type from a function?

LLVM has the attribute ```sret``` for structure pointers passed to functions for storing the return value for non-scalar values.
In the example **language1**, I defined the types

 * unbound_reftype with the qualifier "&<-" representing an unbound reference.
 * c_unbound_reftype with the qualifiers "const " and "&<-" representing an unbound constant reference.

The ```out``` member of the unbound reference constructor references a variable that can be substituted with the reference of the type this
value is assigned to. This allows injecting the address specifier of the reference after the constructor code has been built.

Returning values by constructing them in place at a memory address passed as a parameter by the caller can be implemented with unbound reference types
within the model of expressions represented by type/constructor pairs. In this case, the type returned is an unbound reference type.
The caller then injects the destination address when assigning the return value. This by the way implements copy elision with in-place construction.

Unbound reference types are defined as reducible to a reference type with a constructor creating the variable binding the reference as a local.


<a name="withAndUsing"/>

### How to implement type contexts, e.g. the Pascal "WITH", the C++ "using", etc...?

In the example **language1**, the context list used for resolving types of free identifiers is bound to the current scope.
A "WITH" or "using" defines the context list for the current scope as a copy of the context list of the innermost enclosing scope
and adds the type created from the argument of the "WITH" or "using" command to it.

<a name="inheritance"/>

### How to implement class inheritance?

An inherited class is declared as a member. A reduction from the inheriting class to this member makes the class appearing as an instance of the inherited class.

##### Note
For implementing method call polymorphism see [How to implement method call polymorphism?](#virtualMethodTables).


<a name="virtualMethodTables"/>

### How to implement method call polymorphism?

Method call polymorphism allows handling different implementations of an interface uniformly. Method calls are dispatched indirectly over a
_VMT_ ([Virtual Method Table](https://en.wikipedia.org/wiki/Virtual_method_table)).
In the example **language1**, I implemented interface inheritance only. An interface is a _VMT_ with a base pointer to the data of the object.

Class inheritance with overriding virtual method declarations possible on the other hand has to define a _VMT_  for every class having virtual methods.
Virtual methods are method calls dispatched indirectly over the _VMT_. The difference to interface inheritance is that any class inheriting from
a base class with virtual methods that are not marked as final can override them.

This raises some issues concerning the base pointer of the class. If a method implementation can be overridden, then the base pointer of a class
has to be determined by the method itself. Each method implementation has to calculate its ```self``` pointer from the base pointer passed.
This could be the base pointer of the first class implementing the method.

Because of these complications and the lack of insight about the usefulness of overriding methods, I did not implement it in the example **language1**.
Another reason is the need for optimizations in the compiler front-end to eliminate virtual method calls. As _Mewa_ follows the paradigm of mapping the input to the intermediate language without or only few optimizations,
language models with complicated optimizations needed in the front-end are not considered practical for _Mewa_.


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
When relying on a single pass traversal of the _AST_ to emit code, an order of definitions like the following is required:
```
class A {
	class B {
		...
	};
	B b;
};
```
For languages allowing a schema like C++, we need multi-pass _AST_ traversal. See [How to implement multipass AST traversal](#multipassTraversal).


<a name="multipassTraversal"/>

### How to implement multi-pass AST traversal?

Sometimes some definitions have to be prioritized, e.g. member variable definitions have to be known before translating member functions.
_Mewa_ does not support multipass traversal by nature, but you can implement it by passing an additional parameters to the traversal routine.
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
The declaration of functions, operators, and methods of classes gets more complicated. In the example **language1**, I define the type system method ```definition_decl_impl_pass``` that is executed in two passes. The first pass declares the callable. The second pass creates its implementation. The reason for this is the possibility of the methods to call each other without being declared in an order of their dependency as this is not always possible and not intuitive.


<a name="branchDependencies"/>

### How to handle dependencies between AST branches of a node in the AST?

With the tree traversal in your hands, you can partially traverse the sub-nodes of an _AST_ node. You can also combine a partial traversal with a multipass traversal to get some info out of the _AST_ sub-nodes before doing the real job.

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


<a name="shadowingDeclarations"/>

### How to report shadowing declarations?

In the example **language1** there is no measure taken to report shadowing declarations.
The symbol resolving algorithm itself gives no support on its own.
Shadowing declarations are preferred because the shortest path search prefers them.

I don't consider it reasonable to add support in the typedb API because it would be complicated and because it would not generate good error messages for
shadowing declaration conflicts. I suggest making a list of all cases and to handle them one by one.

For example:
 * When declaring a class member variable, check the accessibility of an inherited definition.
 * When declaring a local variable, check the accessibility of a variable in an enclosing scope.
 * Split the context type list in class method bodies in a global and a members part and do the type resolving on both sides. Report a shadowing conflict, if the symbol was found with both context-type lists.


<a name="exceptions"/>

### How to implement exception handling?

In the example **language1**, I implemented a very primitive exception handling. The only thing throwable is an error code plus optionally a string.
Besides simplicity, this solution has also the advantage that exceptions could potentially be thrown across shared library borders without relying
on a global object registry as in _Microsoft Windows_.

The LLVM IR templates used for implementing the exception handling for example **language1** can be found in ```llvmir.exception```
in the _Lua_ module llvmir.lua. There has been some trial and error involved in the implementation.
But it works now and has been tested well. LLVM IR defines two variants of calling a function:

 * One that might throw (LLVM IR instruction ```invoke```)
 * and one that never throws (LLVM IR instruction ```call```)

The code generator has to figure out which variant is the correct one, either by declaration or by instrospection.
Choosing the wrong one will lead to unrecoverable errors.

The ```invoke``` is declared with two labels, one for the success case and one for the exception case.
The exception case requires to enter a code block with a ```landingpad``` instruction. It has also two variants

 * One that will process the exception (catch it)
 * and one that will rethrow it (LLVM IR instruction ```resume```)

Exceptions are thrown with a LLVM library function call ```__cxa_throw```.
The throwing function has to be declared with function attributes telling that it is throwing. This is labeled as ```personality``` in LLVM IR.
If you don't label a throwing function as throwing, your translated program will crash.
Have a look at ```llvmir.functionAttribute``` in the _Lua_ module llvmir.lua as an example.

Every exception triggers some cleanup.
In the example **language1** I implemented the different cases of exception handling as exit scenarios.

Exit scenarios are commands that need some cleanup before they are executed.
To see how this is handled, continue with [How to automate the cleanup of data?](#cleanupData).


<a name="cleanupData"/>

### How to automate the cleanup of data?

Automated deallocation of allocated resources on the exit of scope is a complicated issue. Allocations have to be tracked.
At any state with a  potential exit, a withdrawal scenario has to be implemented.
In a withdrawal, all allocations have to be revoked in the reverse order they were done.

To implement this, you need some definition of a state. Some counter that is incremented on every resource allocation.
In the example **language1**, the _scope-step_ is used for this. Allocations are bound to the _scope-step_ of the _AST_
node from which the allocation originates. With originating, I mean the _AST_ node representing the expression.
The allocations themselves might queue up in functions creating the constructor of the expression. Using the the _scope-step_ of
the _AST_ node of this function would lead to allocations sharing the same _scope-step_ as state identifier.
In the example **language1**, the origin _scope-step_ of the _AST_ node is attached as an element to the constructor.
The resource allocation registers the snippet of its cleanup code with the origin _scope-step_ as key in the current _allocation frame_.

The _allocation frame_ is a structure bound to a scope that has some cleanup to do.
Every _allocation frame_ has a link to its next covering _allocation frame_ if such exists.
All exits of an allocation frame are acquired by the _scope-step_ where it happened and the key that classifies the exit.

Possible exit keys are
 * Exceptions thrown, the exceptions are store in local variables (simple, as we have only one structure for an exception in the example **language1**)
 * Exceptions forwarded (exception handling of functions called)
 * Return of a specific value or variable

Every allocated exit gets a label that is called to start the exit. For example a "return 0;" is implemented as branching to the label allocated for this.
The code at this label will start the cleanup call chain. At the end of this chain will be the "ret i32 0" instruction (in the case of a 32 bit integer).
The code generated for the allocation frame will contain the code to implement the exit chain. It will handle all allocations of exit requests in the
_allocation frame_ and it will join all registered deallocations for this _allocation frame_ with the labels of the allocated exit requests.
At the end of an exit chain of an _allocation frame_, the code either adds the exit code or branches to the enclosing _allocation frame_ with the
label associated with the _scope-step_ and the exit case in this _allocation frame_.

A special case is implemented with exception handling, where the branching to the enclosing _allocation frame_ is done only until the catch block of the exception.


<a name="generics"/>

### How to implement generics?

Generics (e.g. C++ templates) are a form of lazy evaluation. This is supported by _Mewa_. You can store a subtree of the _AST_ and associate it with a type instead of directly evaluating it.
A generic can be implemented as a type. The application of an operator with arguments triggers the creation of a template instance on-demand in the scope of the generic.
A table is used to ensure generic type instances are created only once and reused if referenced multiple times.
The instantiation of a generic invokes a traversal of the _AST_ node of the generic with a context type declared for this one instance.
The template parameters are declared as types in this context.

In the following, I describe how generics are implemented in the example **language1**.
 1. First, a unique key is created from the template argument list.
    All generic instances are in a map associated with the type. We make a lookup there to see if the generic type instance has already been created. We are done in this case.
 2. Otherwise, we create the generic instance type with the generic type as context type and the unique key as type name.
 3. For the local variables in the scope of the generic, we similarly create a type as the generic instance type.
    In the example **language1** a prefix "local " is used for the type name. This type is used as a context type for local definitions instead of 0.
    This ensures that local definitions from different _AST_ node traversals are separated from each other.
 4. For each generic argument, we create a type with the generic instance type as context type.
     * If the generic argument is a data type (has no constructor), we create a type as an alias (typedb:def_type_as).
     * If the generic argument is a value type (with a constructor), we create a type (typedb:def_type) with this constructor and a reduction to the passed type.
    The scope of the template argument definition is the scope of the generic _AST_ node.
 5. Finally, we add the generic instance type to the list of context types and traverse the _AST_ node of the generic.
    The code for the generic instance type is created in the process.

Because the instantiation of generics may trigger the instantiation of other generics, we have to use a stack holding the state variables used during this procedure.
The state variables include also all keys used for objects attached to a scope like callable environment, allocation frames etc.
In the example **language1**, the function ```pushGenericLocal``` creates a unique suffix for all keys used for ```typedb:set_instance```,```typedb:get_instance```, and ```typedb:this_instance```.
The function ```popGenericLocal``` restores the previous state.


<a name="genericsParameterDeduction"/>

#### How to deduce generic arguments from a parameter list?
C++ can reference template functions without declaring the template arguments. The template arguments are deduced from the call arguments.
I did not implement this in the example **language1**.

For automatic template argument deduction, the generic parameter assignments have to be synthesized by matching the function parameters
against the function candidates. As result, the complete list of generic parameter assignments has to be evaluated.
This list is used as the generic argument list for the instantiation.

The generic argument deduction is therefore decoupled from the instantiation of the generic.
It is a preliminar step before the instantiation of the generic type.


<a name="multipleTraversal"/>

### How to traverse AST nodes multiple times without the definitions of different traversals interfering?

Generics use definition scopes multiple times with differing declarations inside the scope. These definitions interfere. To separate the definitions I suggest defining a type named 'local &lt;generic name&gt;' and define this type as a global variable to use as context-type for local variables inside this scope. In the example **language1** I use a stack for the context-types used for locals inside a generic scope, resp. during the expansion of a generic as these generic expansions can pile up (The expansion can lead to another generic expansion, etc...).


<a name="concepts"/>

### How to implement concepts like in C++?

I did not implement concepts in the example **language1**. But I would implement them similarly to generic classes or structures. The _AST_ node of the concept is kept in the concept data structure and traversed with the type to check against the concept passed down as context-type. In the _AST_ node functions of the concept definition, you can use typedb:resolve_type to retrieve properties as types to check as a sub condition of the concept.

In other words, the concept is implemented as generic with the type to check the concept against as an argument, with the difference that it doesn't produce code. It is just used to check if the concept structure can be successfully traversed with the argument type.

<a name="lambdas"/>

### How to implement lambdas?

In the example **language1**, lambdas are declared as types with an _AST_ node and a list of parameter names attached.
Such a lambda type can be passed as argument when instantiating a generic.
The following happens when a lambda is applied:

 1. For each instance of the lambda a context-type is created and the parameters with their name and this context-type.
    * Each parameter with a constructor is defined with ```typedb:def_type``` with this constructor. A reduction is defined from this type to the passed parameter type.
    * Each parameter without a constructor is defined with ```typedb:def_type_as``` as alias of the passed parameter type.
    The parameter types are defined in the scope of the _AST_ node of the lambda.
 2. The _AST_ node of the lambda is traversed with the context-type of the lambda in the set of context-types.
 3. The resulting constructor is returned as result of the traversal of the the lambda instance

Because of the multiple uses of the same scope as in generics, lambdas have to define free variables with a context-type created for each instance.
As for generics, a context-type with the name prefix "local " is defined and used instead of 0 for local type definitions.
This ensures that local definitions from different _AST_ node traversals are separated from each other.

<a name="cLibraryCalls"/>

### How to implement calls of C-Library functions?

LLVM supports extern calls. In the specification of the example **language1**, I support calls of the C standard library.


<a name="varargCalls"/>

### How to implement calls with a variable number of arguments?

#### Extern functions

In the example **language1** I define a Lua table (```constVarargTypeMap```) mapping first-class types and const expression types to the type used to pass a value as an additional non-specified parameter (one of ```...```).
For every function that has a ```...``` at the end of its parameter list, that type is used as a parameter type.
About the implementation, I do not have to care, except that C-library functions cannot handle certain types as ```...``` arguments.
For example, ```float``` has to be mapped to a ```double``` if passed as an additional parameter.

#### Custom Functions

For implementing functions with a variable number of arguments in your language, you have to care about addressing them.
Clang has of course support for variable argument functions. Unfortunately, I did not dig into it yet.
The LLVM IR commands to handle additional variable arguments are similar to the C-Library functions: ```llvm.va_start, llvm.va_copy, llvm.va_end```.

#### Generics

For implementing generics with a variable number of arguments I suggest implementing the possibility to define the last argument of a generic as an array.
A dedicated marker like ```...``` could signal that. I did not implement this in the example **language1** yet. But I do not see any obstacle here
defining a template argument instantiation as a type with an access operator for its sub-types, the first element, the tail, an element addressed by a compile-time index, or an iterator.


<a name="coroutines"/>

### How to implement coroutines?

Coroutines are callables that store their local variables in a structure instead of allocating them on the stack.
This makes them interruptable with a yield that is a return with a state that describes where the coroutine continues when it is called the next time.
This should not be a big deal to implement. I will do this in the example **language1**.


<a name="copyElision"/>

### How to implement copy elision?

Copy elision, though it's making programs faster is not considered as an optimization, because it changes behavior. Copy elision has to be part of a language specification and thus be implemented in the compiler front-end. In the example **language1**, I implemented two 2 cases of copy elision:
  * Construction of a return value in the return slot (LLVM sret) provided by the caller
  * Construction of a return value in the return slot, when using only one variable for the return value in the variable declaration scope
Copy elision is implemented with the help of the **Unbound Reference Type**.

<a name="referenceCountingSwift"/>

### How to implement reference counting on use as in _Swift_?

_Swift_ implements reference counting when required. This implies the adding of a traversal pass that notifies how a type (variable) is used.
This pass decides if a reference counting type used for the variable or not. There is no way to get around the traversal pass that notifies the usage.
I suggest to implement other things like copy elision decisions in this pass too if it is required anyway.
I need to think a little bit more about this issue as it is an aspect that is not covered at all in the example **language1**.


<a name="referenceLifetimeRust"/>

### How to implement reference lifetime checking as in Rust?

_Rust_ implements reference lifetime checking by mandatory attributes for references passed to and returned from functions and for structure members.
These attributes allow assigning a lifetime to all references in the program as they describe a transitive forwarding of lifetime assignments to references.
Values returned from a function get a lifetime attribute from their parameters.
Structures get the lifetime of their shortest living member.
The attributes are identifiers that express identity if they are equal.
The following function header declares the return value lifetime being the same as the parameters lifetime.
```Rust
fn first_word<'a>(s: &'a str) -> &'a str
```
I did not yet get deep into _Rust_. But the lifetime attribute looks for me as perfectly representable by the _scope_ structure in _Mewa_.
I suggest adding the identifier (```a``` in the example above) as an attribute to the parameter constructor.
A function call constructor checks the _scope_ assignments (two values with the same attribute require the same _scope_) and synthesizes the _scope_ attribute of the return value (forwarding the _scope_ related to its lifetime attribute).
I do not know yet how to pass the lifetime attribute from the structure members to the structure.
But I guess the mechanisms are best modeled similarly to functions. It is about the transitive forwarding of lifetime attributes.


<a name="optimizations"/>

### What about optimizations?

Other compiler-frontend models are better suited for optimizations. What you can do with _Mewa_ is to attach attributes to code that helps a back-end to optimize the code generated. That is the model LLVM IR encourages.






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

<a name="hacks"/>

### What are the hacks in the implementation of the example language1?
Some may say that the whole example **language1** is a big hack because of the information entanglement without contracts all over the place. I cannot say much against that argument. _Mewa_ is not optimized for collaborative work. What I consider hacks here are violations or circumventions of the core ideas of _Mewa_. Here are bad hacks I have to talk about:
 1. *Stateful constructors*: Constructors have an initialization state that tells how many of the members have been initialized. One principle of _Mewa_ is that every piece of information is related to what is stored in the _typedb_ or in the _AST_ or somehow related to that, stored in a _Lua_ table indexed by a value in the _typedb_ or the _AST_. Having a state variable during the traversal of the _AST_ and the code generation is considered bad and a hack. Unfortunately, I don't have any idea to get around the problem differently.
 2. *Cleaning up of partially constructed objects*: This problem caught me on the wrong foot, especially the building of arrays from initializer lists. The solution is awkward and needs to be revisited.

