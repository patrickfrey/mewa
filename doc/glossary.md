# Glossary

## Name _Mewa_
The Polish name for seagull. It's difficult to find short memorizable names. I got stuck to names of birds in the polish language without non-ASCII characters.

## LLVM IR ##
The intermediate representation of LLVM (https://llvm.org/). The LLVM IR has a textual representation that is used as the intermediate format in the examples of _Mewa_. The tests also use the programs *lli* (interpreter) and *llc* (compiler) that can take this textual representation of LLVM IR as input and run it (*lli*) or translate it into a binary object file (*llc*).

## Type System
A set of term rewrite rules to build structures of types and to reduce them to types and in this process construct a program that represents the entire structure defined in the program source.

## Type
Any item addressable in a program is a **type**. In _Mewa_ types are represented by an unsigned integer number. Types are also used to describe the context of definitions inside a structure. Every type is defined with a context type. The reserved value of 0 is used as the context type for free definitions.

## Reduction
### In the context of the type system
A rule to derive a **type** from another with a description of how to construct the target type from the source type attached.
### In the context of the parser
A state transition occurring after the last item of a production has been parsed, replacing the right side of the production with the left side on the parser stack.

## Scope
Pair of integer numbers that address a subtree of the AST. The first number defines the start of the scope and the second number defines one number after the last step that belongs to the scope. The scope defines the validity of a definition in the language defined. A definition is valid if the scope includes the scope-step of the instruction that queries the definition.

### Example
1. Item 'ABC' defined in scope [1,123]
2. Item 'ABC' defined in scope [5,78]
3. Item 'ABC' defined in scope [23,77]
4. Item 'ABC' defined in scope [81,99]

The Query for a type 'ABC' in an instruction with scope-step 56 assigned, returns the 3rd definition.

### Scope and Structures
Some other compiler models represent hierarchies of data structures by lexical scoping. In _Mewa_ best practice is considered to represent visibility in hierarchies of data structures with context types and not by scope.

## Scope-step
Counter that is incremented for every production in the grammar marked with the operators **>>** or **{}**. The **scope-step** defines the start and the end of the **scope** assigned to productions by the scope operator **{}**. A **scope** starts with the scope-step counter value when first entering a state with a production marked as **{}** and ends with one step after the value of the **scope-step** after exit (**reduction** in the context of the parser).

## Constructor
A constructor implements the construction of an object representing a type. It is either a structure describing the initial construction of the object or a function describing the derivation of an object from the constructor of the derived type. 

### Ctor/Dtor
To prevent a mess in the glossary we refer to a constructor of an object in the programming language our compiler translates as *ctor*. The corresponding destructor is called *dtor".

## Glossary of Formal Languages
### Terminal
A basic item of the language. Further reading in [Wikipedia: Context Free Grammar](https://en.wikipedia.org/wiki/Context-free_grammar).

### Nonterminal
A named structure of the language. Further reading in [Wikipedia: Context Free Grammar](https://en.wikipedia.org/wiki/Context-free_grammar).

### BNF / EBNF / Yacc/Bison
Languages to describe a context free grammar. There exist many dialects for a formal description of a context free language grammar based on BNF. The most similar to [the grammar of _Mewa_](grammar.md) is the language used in [Yacc/Bison](https://www.cs.ccu.edu.tw/~naiwei/cs5605/YaccBison.html). Further reading in [Wikipedia: Backus-Naur form](https://en.wikipedia.org/wiki/Backus%E2%80%93Naur_form) and [Wikipedia: Extended Backus-Naur form](https://en.wikipedia.org/wiki/Extended_Backus%E2%80%93Naur_form). 

### LALR(1)
The class of language covered by _Mewa_. Further reading in [Wikipedia: Context Free Grammar](https://en.wikipedia.org/wiki/LALR_parser).

### Lexer
Component that scans the input and produces a stream of items called _tokens_ that are the atoms the grammar of the language is based on. In _Mewa_ the _lexer_ is defined as a set of named patterns. The patterns are regular expressions that are matched on the first/next non-whitespace character in the input. Contradicting _lexeme_ matches are resolved in _Mewa_ by selecting the longest match or the first definition comparing matches of the same length as the _token_ value emitted.

### AST (Abstract Syntax Tree)
The intermediate representation of the program. The output of the program parser. The AST in _Mewa_ is described [here](ast.md).


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
These types are used as context type for accessing private members (in class methods).

#### Initialization Type (init_reftype)
This type is used as a *self* reference in constructors. It redirects assignment calls to constructor calls during the initialization phase of the constructor. During this phase, the elements of a class are initialized in the order of their definition as members of the class. The initialization phase is completed when the first method is called or the first member is accessed otherwise than through the assignment operator redirected to a constructor call. The initialization of members not explicitly initialized is implicitly completed in the constructor.

#### Control True/False Types / Control Boolean Types
Most language specifications require the evaluation of a boolean expression to terminate as early as the result is guaranteed, even when some branches of the expressions are undefined. Thus ```if (a && a->x)``` should evaluate to false if a is _NULL_ without trying to evaluate the undefined branch ```a->x``` that would lead to a segmentation fault on most platforms. For representing boolean expressions we define the types *controlTrueType* that contains the code that is executed for the expression remaining *true* and contains an unbound label in the out variable where the code jumps to when the expression evaluates to *false*. The mirror type of the *controlTrueType* is the type *controlFalseType* that contains the code that is executed for the expression remaining *false* and in contains an unbound label in the out variable where the code jumps to when the expression evaluates to *true*.
As a class of types, they are also referred to as **Control Boolean Types**.

#### Pointer types
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
The frame object defines the context for implicit cleanup of resources after the exit of the scope the allocation frame is associated with. Every form of exit has a chain of commands executed before the final exit code is executed. The allocation frame provides a label to jump to depending on the current scope-step and the exit code. With the jump to this label, the cleanup followed by the exit from the allocation frame is initiated.

### Callable Environment
The callable environment holds the data associated with a callable during the processing of its body. Such data are for example the generators of registers and labels, the list of allocation frames holding the code executed in case of exceptions, the return type in case of a function, the initialization state in case of a constructor, some flags that indicate some events needed for printing the function declaration, etc...


