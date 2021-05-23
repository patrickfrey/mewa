# The Grammar Definition Language
_Mewa_ uses a language to describe an **attributed grammar** similar to the language defined by [Yacc/Bison](https://www.cs.ccu.edu.tw/~naiwei/cs5605/YaccBison.html). If you are familiar with _Yacc/Bison_ you won't have problems adapting to _Mewa_.

## Parser Generator
_Mewa_ implements an LALR(1) parser generator for an attributed grammar. The grammar description is read from a source file and has 3 parts:

  * The **configuration declarations** marked with a prefix '%'
  * The **lexeme/token declarations** 
  * The **production declarations**

### Configuration Declarations
Configuration declarations start with a '**%**' followed by the command and its arguments separated by spaces.
The following commands are known:

* **%** **LANGUAGE** _name_ **;** Defines the name of the language of this grammar.
* **%** **TYPESYSTEM** _name_ **;** Defines the name of the _Lua_ module without extension _.lua_ implementing the typesystem. The generated _Lua_ module of the compiler will import the type system module with this name. The typesystem module will contain all _Lua_ AST node functions referred to in the grammar.
* **%** **CMDLINE** _name_ **;** Defines the name of the Lua module without extension _.lua_ implementing command-line parser of the compiler. If not specified _Mewa_ generates code that calls the compiler with the first command-line argument. The module defined by CMDLINE has to export a method _parse_ with 2 arguments: the name of the language and the array of command-line arguments. It is expected to return a structure with the following fields:

    * **input**: the input file name
    * **output**: the output file name
    * **debug**: the debug output file name
    * **options**: an option structure passed to the compiler
    * **target**: the template file used fot the LLVM IR output. 

  An example of a command-line parser can be found [here](../examples/cmdlinearg.lua). 

* **%** **COMMENT** _start_ _end_ **;** Defines the content between the patterns _start_ and _end_ defined as regular expression quoted in single or double quotes as a comment. Comments are ignored by the lexer and thus by the compiler.
* **%** **COMMENT** _start_ **;** Defines the content starting with the pattern _start_ defined as regular expression quoted in single or double quotes until the next end of a line as a comment. Comments are ignored by the lexer and thus by the compiler.
* **%** **IGNORE** _pattern_ **;** Defines a token matching this pattern defined as regular expression quoted in single or double quotes as invisible. It is ignored by the lexer and thus by the compiler. Hence it does not need a name.
* **%** **BAD** _name_ **;** Defines the name of the error token of the lexer. Has no implications but for the debugging output.

### Lexeme/Token Declarations
Lexeme declarations start with an identifier followed by a colon '**:**', a pattern matching the lexeme, and an optional selection index:

* _lexemename_ **:** _pattern_ **;**
* _lexemename_ **:** _pattern_ _select_ **;**

The name _lexemename_ defines the identifier this lexeme can be referred to as a token in the production declarations of the grammar.
The regular expression string _pattern_ quoted in single or double quotes defines the pattern that matches the _lexeme_.
You can have multiple declarations with the same name.
The optional integer number _select_ defines the index of the subexpression of the regular expression match to select as the value of the _lexeme_ recognized. 
If _0_ or nothing is specified the whole expression match is chosen as the _token_ value emitted.
If multiple patterns match at the same source position then the longest match is emitted as the _token_ value. If two matches have the same length, the first declaration is chosen. If one of the matches is a keyword or an operator, it is always the first choice.
Keywords and operators of the grammar are not declared in the lexer section but are referred to as strings in the production declarations of the grammar.

### Production Declarations
Production declarations start with an identifier optionally attributed with a priority specifier, followed by an assignment '**=**' and the right side of the production.
At the end of each production, we can declare the production attributes. Attributes are
   * A reference to a function calling the typesystem module written in _Lua_ with an optional argument.
   * One of the operators '**{}**' or '**>>**' attaching the _scope_ attributes **scope** or **step** to the nodes of the AST built during parsing.

Here are some declaration patterns described:

1. _name_ **=** _itemlist_ **;**

    * Simple grammar production definition with _name_ as left-hand _nonterminal_ and _itemlist_ as a space-separated list of identifiers (nonterminals or lexeme names) and strings (keywords and operators as implicitly defined lexemes).
2. _name_/_priority_ **=** _itemlist_ **;**

    * Production definition with a specifier for the rule priority in _SHIFT/REDUCE_ conflicts. The priority specifier is a character **L** or **R** immediately followed by a non-negative integer number. The **L** defines the rule to be left-handed, meaning that _REDUCE_ is preferred in self including rules **L** -> **L** **..**, whereas **R** defines the rule to be right-handed, meaning that _SHIFT_ is preferred in self including rules. The integer number in the priority specifier specifies the preference in _SHIFT/REDUCE_ involving different rules. The production with the higher of two numbers is preferred in _SHIFT/REDUCE_ conflicts. Priorities of _SHIFT_ actions must be consistent for a state.
3. _name_[ /_priority_ ] **=** _itemlist_ **(** _luafunction_ **)** **;**

    * Production definition with Lua function as a single identifier plus an optional argument (a Lua value or table). The function identifier refers to a member of the typesystem module written by the author and loaded by the compiler. The function and its optional argument are attached to the node of the **AST** defined by its production.

4. _name_[ /_priority_ ] **=** _itemlist_ **(** **>>** _luafunction_ **)** **;**

    * Production definition with an increment defined for the **scope-step**. The Lua function is optional here.

5. _name_[ /_priority_ ] **=** _itemlist_ **(** **{}** _luafunction_ **)** **;**

    * Production definition with a **scope** structure (start and end of the scope) defined for the result node. The Lua function is optional here.

### Conflict solving
The mechanism of conflict solving by attaching priorities to productions is comparable with declaring operator priorities in _Bison_/_Yacc_ though it is more error-prone and can lead to undetected conflicts. See bug report #1.

## Compilation Process
Matching lexemes declared as regular expressions in the 2nd part of the grammar create a new node on the stack. Attributes with a Lua call create a new node on the stack. Nodes created from Lua calls take all elements created on the stack by their production from the stack and define them as child nodes in the AST. 
The remaining nodes on the stack after syntax analysis are the root nodes of the AST built. Their Lua function attached is called by the compiler after the syntax analysis to produce the output.

## Differences to Yacc/Bison
Unlike in _Yacc/Bison_ where the lexer can be arbitrarily defined, the Lexer of _Mewa_ is restricted to classes of languages where spaces have no meaning and the _tokens_ can only be described as regular expression matches. The annotation of the rules with code in _Yacc/Bison_ is also different and more restricted in _Mewa_. Instead of arbitrary code to be generated and invoked when the annotated rule matches, _Mewa_ has one _Lua_ AST node function per production that is called on its match. The operator precedence in _Yacc/Bison_ has a counterpart with roughly the same expressiveness in _Mewa_. In _Mewa_ priorities with left/right-handed associativity are attached to productions. Unfortunately, the _Mewa_ approach is more error-prone.

### Meaning of Whitespaces
Whitespaces have no meaning in languages describable by _Mewa_.
The lexemes of the languages cannot start with whitespaces as the parser skips whitespaces after every lexeme matched and starts matching the next lexeme with the first non-whitespace character after the last match.

## Scope of AST Nodes and Definitions
The **scope** of a node in the resulting AST is defined as a pair of integer numbers **[** _start_, _end_ **]**.
The definitions of types in the typesystem will include the scope structure to select the valid definition amongst definitions with the same name.
A definition is valid if its **scope-step** is inside the scope checked:
* _step_ **>=** _start_
* _step_ **<** _end_.



