# FAQ

1. [General](#general)
    * [What is _Mewa_](#WTF)
    * [What audience is targeted by _Mewa_?](#targetAudience)
    * [What classes of languages are covered by _Mewa_?](#coveredLanguageClasses)
1. [Problem Solving HOWTO](#problemSolving)
    * [How to process the AST structure?](#astStructure)
    * [How to handle scopes?](#astTraversalAndScope)
    * [How to store and access scope related data?](#scopeInstanceAndAllocators)
    * [How to implement type qualifiers like const and reference?](#typeQualifiers)
    * [How to implement pointers and arrays?](#pointersAndArrays)
    * [How to implement namespaces?](#namespaces)
    * [How to implement complex expressions?](#complexExpressions)
    * [How to implement control structures?](#controlStructures)
    * [How to implement callables like functions, procedures and methods?](#functionsProceduresAndMethods)
    * [How to implement hierarchical data structures?](#hierarchicalDataStructures)
    * [How to implement the Pascal "WITH", the C++ "using", etc.?](#withAndUsing)
    * [How to implement object oriented polymorphism?](#virtualMethodTables)
    * [How to implement exception handling?](#exceptions)
    * [How to implement generic programming with templates?](#templates)


<a name="general"/>

## General
This section of the FAQ answers some broader questions about what one can do with _Mewa_.

<a name="WTF"/>

### What is _Mewa_?
_Mewa_ is a tool for the rapid prototyping of compiler frontends for strongly typed languages.

<a name="targetAudience"/>

### What audience is targeted by _Mewa_?
_Mewa_ is probably not the right tool for people without any clue how parser generators work as the assistance for beginners is quite poor.
There is for example no support given by _Mewa_ if your grammar contains conflicts.
The ideal start to get familiar with _Mewa_ is after you have made some steps with other parser generators and you know how a table driven parser generator works.
This is when you ask yourself questions about how to implement a typesystem.

<a name="coveredLanguageClasses"/>

### What classes of languages are covered by _Mewa_?
_Mewa_ was written to be capable to implement general purpose programming languages of the power of C++ (*) or Rust with a LALR(1) grammar definition.
It is not recommended to use _Mewa_ for other than compilable, stronly typed programming languages, because it was not designed for other language classes.
The system to define a typesystem as graph of types and reductions within the temporal concept of scope makes no sense for other language classes.

**(*)** Unfortunately C++ can't be implemented with _Mewa_ as there is no clear separation of syntax and semantic analysis possible in C++.
To name only one example: There is no way to decide if "the statement A * B;" is an expression or a type declaration without semantical information about A.


<a name="problemSolving"/>

## Problem Solving HOWTO
This section of the FAQ gives some recommendations on how to solve specific problems in compilers for strongly typed languages. The hints given here are not meant as instructions. There might be better solutions for the problems presented.

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

<a name="functionsProceduresAndMethods"/>

### How to implement callables like functions, procedures and methods?

<a name="hierarchicalDataStructures"/>

### How to implement hierarchical data structures?

#### Why not with scope for inner data structures?

<a name="withAndUsing"/>

### How to implement the Pascal "WITH", the C++ "using" and friends?

<a name="virtualMethodTables"/>

### How to implement object oriented polymorphism?

<a name="exceptions"/>

### How to implement exception handling?

<a name="templates"/>

### How to implement generic programming with templates?


