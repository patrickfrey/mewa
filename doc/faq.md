# FAQ

1. [General](#general)
    * [What is _Mewa_](#WTF)
    * [What audience is targeted by _Mewa_?](#targetAudience)
    * [What classes of languages are covered by _Mewa_?](#coveredLanguageClasses)
1. [Problem Solving HOWTO](#problemSolving)
    * [How do we process the AST structure?](#astStructure)
    * [How do we handle scopes?](#astTraversalAndScope)
    * [How do we implement complex expressions?](#expressionsTypesAndConstructors)
    * [How do we implement control structures?](#controlStructures)
    * [How do we implement callables like functions, procedures and methods?](#functionsProceduresAndMethods)
    * [How do we implement hierarchical data structures?](#dataStructures)
    * [How do we implement object oriented polymorphism?](#virtualMethodTables)
    * [How do we implement exception handling?](#exceptions)
    * [How do we implement generic programming with templates?](#templates)


<a name="general"/>

## General
This section of the FAQ answers some broader questions about what one can do with _Mewa_.

<a name="WTF"/>

### What is _Mewa_?
_Mewa_ is a tool for the rapid prototyping of compiler frontends for strongly typed languages.

<a name="targetAudience"/>

### What audience is targeted by _Mewa_?
_Mewa_ is probably not the right tool for people without any clue how copilers work as the assistance for beginners is quite poor.
There is for example no support given by _Mewa_ if your first grammar contains LALR(1) conflicts. You stand in front of a wall in this case.
The ideal start to get familiar with _Mewa_ is after you have made some steps with other parser generators and you know how a table driven parser generator works
and you start to ask yourself questions about how to implement a typesystem. Some background in theoretical computer science, especially some lectures about type theory helps too, as the glossary of _Mewa_ was designed to get people with such a background on board, but it is not necessary.

<a name="coveredLanguageClasses"/>

### What classes of languages are covered by _Mewa_?
_Mewa_ was written to be capable to implement general purpose programming languages of the power of C++ (*) or Rust with a LALR(1) grammar definition.
It is not recommended to use _Mewa_ for other than compilable, stronly typed programming languages, because it was not designed for other language classes.
The system to define a typesystem as graph of types and reductions within the temporal concept of scope makes no sense for other language classes.

#### (*)
Unfortunately C++ can't be implemented with _Mewa_ as there is no clear separation of syntax and semantic analysis possible in C++.
To name only one example: There is no way to decide if "the statement A * B;" is an expression or a type declaration without semantical information about A.


<a name="problemSolving"/>

## Problem Solving HOWTO
This section of the FAQ gives some recommendations on how to solve specific problems in compilers for strongly typed languages. The hints given here are not meant as instructions. There might be better solutions for the problems presented.

<a name="astStructure"/>

### How do we process the AST structure?

<a name="astTraversalAndScope"/>

### How do we handle scopes?

<a name="expressionsTypesAndConstructors"/>

### How do we implement complex expressions?

<a name="controlStructures"/>

### How do we implement control structures?

<a name="functionsProceduresAndMethods"/>

### How do we implement callables like functions, procedures and methods?

<a name="dataStructures"/>

### How do we implement hierarchical data structures?

<a name="virtualMethodTables"/>

### How do we implement object oriented polymorphism?

<a name="exceptions"/>

### How do we implement exception handling?

<a name="templates"/>

### How do we implement generic programming with templates?


