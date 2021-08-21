# Glossary

## Name _Mewa_
The Polish name for seagull. It's difficult to find short memorizable names. I got stuck to names of birds in the Polish language without non-ASCII characters.

## LLVM IR ##
The intermediate representation of LLVM (https://llvm.org/). The LLVM IR has a textual representation that is used as the intermediate format in the examples of _Mewa_. The tests also use the programs *lli* (interpreter) and *llc* (compiler) that can take this textual representation of LLVM IR as input and run it (*lli*) or translate it into a binary object file (*llc*).

<a name="type"/>

## Type
Any item addressable in a program is a **type**. In _Mewa_ types are represented by an unsigned integer number that is created by the typedb library from a counter.
Every type definition has a tuple consisting of a ([context type](#contextType)) and a name as the key that identifies it plus some optional ([parameter](#parameter)) definitions attached.

<a name="reduction"/>

## Reduction

#### In the context of the type system
A rule to derive a [type](#type) from another with a description called [constructor](#constructor) that represents or implements the construction of an instance of the target type from the source type attached.

#### In the context of the parser
A state transition occurring after the last item of a [production](#production) has been parsed, replacing the right side of the _production_ with the left side on the parser stack.

<a name="scope"/>

## Scope
Pair of integer numbers that address a subtree of the [AST](#AST). The first number defines the start of the scope and the second number defines one number after the last step that belongs to the scope. The scope defines the validity of a definition in the language defined. A definition is valid if the scope includes the scope-step of the instruction that queries the definition.

### Example
1. Item 'ABC' defined in scope [1,123]
2. Item 'ABC' defined in scope [5,78]
3. Item 'ABC' defined in scope [23,77]
4. Item 'ABC' defined in scope [81,99]

The Query for a type 'ABC' in an instruction with scope-step 56 assigned, returns the 3rd definition.

### Scope and Structures
Some other compiler models represent hierarchies of data structures by lexical scoping. In _Mewa_ best practice is considered to represent visibility in hierarchies of data structures with [context types](#contextType) and not by scope.

<a name="step"/>

## Scope-step
Counter that is incremented for every [production](#production) in the grammar marked with the operators **>>** or **{}**. The **scope-step** defines the start and the end of the [scope](#scope) assigned to _productions_ by the scope operator **{}**.

#### Initialization of the Scope from the Scope-Step counter
A **scope** starts with the scope-step counter value when first entering the traversal of an [AST](#AST) node with a _production_ marked as **{}**.
It ends with the **scope-step** increment after exiting the traversal of the _AST_ node.

<a name="contextType"/>

## Context Type
The type used as first parameter in a type declaration is called the **context type** of the definition.
The _context type_ is either a type defined before or 0 representing the absence of a context type or the global context.
Context types are used to describe relations like membership. They are also used to express visibility rules.

<a name="constructor"/>

## Constructor
A constructor implements the building of an instance representing a type.
It is either a structure describing the initial construction of the instance or a function describing the derivation of an instance from the constructor of the derived type.

<a name="parameter"/>

## Type Parameters
Type parameters if not **nil** are represented as a list of ([type](#type)) / ([constructor](#constructor)) pairs.
Type parameters are treated as attributes and interpreted by the typedb library only to check for duplicate type definitions in the same [scope](#scope).
Besides that, they are also printed as part of the type in its representation as string.
Any other interpretation is up to the Lua part of the compiler.

<a name="ctor_dtor"/>

### Ctor/Dtor
To prevent a mess in the glossary we refer to a constructor of an object in the programming language our compiler translates as **ctor**.
The corresponding destructor is called **dtor**.

## Glossary of Formal Languages

<a name="production"/>

### Production
A rule the grammar describing the language. Further reading in [Wikipedia: Context Free Grammar](https://en.wikipedia.org/wiki/Context-free_grammar).

<a name="terminal"/>

### Terminal
A basic item of the language. Further reading in [Wikipedia: Context Free Grammar](https://en.wikipedia.org/wiki/Context-free_grammar).

<a name="nonterminal"/>

### Nonterminal
A named structure of the language. Further reading in [Wikipedia: Context Free Grammar](https://en.wikipedia.org/wiki/Context-free_grammar).

<a name="BNF"/>

### BNF / EBNF / Yacc/Bison
Languages to describe a context free grammar. There exist many dialects for a formal description of a context free language grammar based on _BNF_ (Backus-Naur form). The most similar to [the grammar of _Mewa_](grammar.md) is the language used in [Yacc/Bison](https://www.cs.ccu.edu.tw/~naiwei/cs5605/YaccBison.html). Further reading in [Wikipedia: Backus-Naur form](https://en.wikipedia.org/wiki/Backus%E2%80%93Naur_form) and [Wikipedia: Extended Backus-Naur form](https://en.wikipedia.org/wiki/Extended_Backus%E2%80%93Naur_form).

<a name="LALR"/>

### LALR(1)
The class of language covered by _Mewa_. Further reading in [Wikipedia: Context Free Grammar](https://en.wikipedia.org/wiki/LALR_parser).

<a name="Lexer"/>

### Lexer
A component that scans the input and produces a stream of items called _tokens_ that are the atoms the grammar of the language is based on. In _Mewa_ the _lexer_ is defined as a set of named patterns. The patterns are regular expressions that are matched on the first/next non-whitespace character in the input. Contradicting _lexeme_ matches are resolved in _Mewa_ by selecting the longest match or the first definition comparing matches of the same length as the _token_ value emitted.

<a name="AST"/>

### AST (Abstract Syntax Tree)
The intermediate representation of the program. The output of the program parser. The _AST_ in _Mewa_ is described [here](ast.md).

## Glossary of the Example language1

### Types
The types are split into the following categories, each category having a different constructor function interface.

#### Reference Type (reftype/c_reftype)
These types (const and non-const instance) are referring to an address of a variable. Similar to an _lvalue_ in C++.

#### Value Type (valtype/c_valtype)
These types (const and non-const instance) are referring to a value of a variable but are also used for the pure type (without qualifiers). Similar to an _rvalue_ in C++.

#### Unbound Reference Type (unbound_reftype/c_unbound_reftype)
These types (const and non-const instance) are an internal representation of a reference type whose address has not yet been determined. They are mainly used as the return type of a function where the return slot is provided by the caller. The address of this reference type is injected by an assignment constructor.

#### Private Reference Type (priv_reftype/priv_c_reftype)
These types are used as _context type_ for accessing private members (in class methods).

#### Initialization Type (init_reftype)
This type is used as a *self* reference in constructors. It redirects assignment calls to constructor calls during the initialization phase of the constructor. During this phase, the elements of a class are initialized in the order of their definition as members of the class. The initialization phase is completed when the first method is called or the first member is accessed otherwise than through the assignment operator redirected to a constructor call. The initialization of members not explicitly initialized is implicitly completed in the constructor.

#### Control True/False Types / Control Boolean Types
Most language specifications require the evaluation of a boolean expression to terminate as early as the result is guaranteed, even when some branches of the expressions are undefined. Thus ```if (a && a->x)``` should evaluate to false if a is _NULL_ without trying to evaluate the undefined branch ```a->x``` that would lead to a segmentation fault on most platforms. For representing boolean expressions we define the types *controlTrueType* that contains the code that is executed for the expression remaining *true* and contains an unbound label in the out variable where the code jumps to when the expression evaluates to *false*. The mirror type of the *controlTrueType* is the type *controlFalseType* that contains the code that is executed for the expression remaining *false* and it contains an unbound label in the out variable where the code jumps to when the expression evaluates to *true*.
As a class of types, they are also referred to as **Control Boolean Types**.

#### Pointer Types
Pointer types are implicitly created types. A pointer type is created when used the first time.

#### Constant Expression Types
Constants in the source and expressions built from constants are represented by the following types
  * **constexprIntegerType**	const expression integers implemented as arbitrary precision BCD numbers
  * **constexprUIntegerType** 	const expression unsigned integer implemented as arbitrary precision BCD numbers
  * **constexprFloatType** 	const expression floating-point numbers implemented as 'double'
  * **constexprBooleanType** 	const expression boolean implemented as boolean true/false
  * **constexprNullType** 	const expression null value
  * **constexprStructureType**  const expression tree structure implemented as a list of type/constructor pairs (envelop for structure recursively resolved)

### Allocation Frame
The frame object defines the context for implicit cleanup of resources after the exit of the [scope](#scope) the allocation frame is associated with. Every form of exit has a chain of commands executed before the final exit code is executed. The allocation frame provides a label to jump to depending on the current [scope-step](#step) and the exit code. With the jump to this label, the cleanup followed by the exit from the allocation frame is initiated.

### Callable Environment
The callable environment holds the data associated with a callable during the processing of its body. Such data are for example the generators of registers and labels, the list of allocation frames holding the code executed in case of exceptions, the return type in case of a function, the initialization state in case of a constructor, some flags that indicate some events needed for printing the function declaration, etc...

### Promote Calls
For first-class scalar types we also need to look at the 2nd argument to determine the constructor function to call.
The multiplication of an ```int``` with a ```double``` is defined as the conversion of the first operand to a ```double``` followed by a multiplication of two ```double```s.
This is an example of a **promote call**. It is "promoting" the first argument to the type of the second argument before applying the operator.

