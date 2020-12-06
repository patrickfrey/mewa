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
    * [How to implement return in a function?](#functionReturn)
    * [How to implement hierarchical data structures?](#hierarchicalDataStructures)
    * [How to implement the Pascal "WITH", the C++ "using", etc.?](#withAndUsing)
    * [How to implement object oriented polymorphism?](#virtualMethodTables)
    * [How to implement visibility rules, e.g. private,public,protected?](#visibilityRules)
    * [How to implement exception handling?](#exceptions)
    * [How to implement generic programming with templates?](#templates)
    * [How to implement call of C-Library functions?](#cLibraryCalls)


<a name="general"/>

## General
This section of the FAQ answers some broader questions about what one can do with _Mewa_.

<a name="WTF"/>

### What is _Mewa_?
_Mewa_ is a tool for the rapid prototyping of compiler frontends for strongly typed languages.

<a name="targetAudience"/>

### What audience is targeted by _Mewa_?
**_Mewa_ is for people with some base knowledge about how parser generators work (*).**
It starts where many compiler compiler projects stop. It tries to assist you implementing a typesystem for your compiler project. 

**(*)** There has not yet been much effort done to assist users in case of conflicts in a grammar. The conflicts are detected and reported and the building of the parser fails in consequence, but there is no mechanism implemented for autocorrection and there is no deeper analysis made by the _Mewa_ parser generator program.

<a name="coveredLanguageClasses"/>

### What classes of languages are covered by _Mewa_?
**_Mewa_ was been designed to be capable to implement general purpose programming languages with the power of C++ (*) or Rust having a LALR(1) grammar definition.**
It is not recommended to use _Mewa_ for other than compilable, stronly typed programming languages, because it was not designed for other language classes.
To define a typesystem as graph of types and reductions within the hierarchical concept of scope in this form makes no sense for other language classes.

**(*)** Unfortunately C++ (and C) can't be implemented with _Mewa_ as there is no clear separation of syntax and semantic analysis possible in C/C++.
To name one example: There is no way to decide if the statement ```A * B;``` is an expression or a type declaration without having semantical information about A.


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

<a name="exceptions"/>

### How to implement exception handling?

<a name="templates"/>

### How to implement generic programming with templates?

<a name="cLibraryCalls"/>

### How to implement call of C-Library functions?

