# The Grammar Definition Language
_Mewa_ uses a language to describe an **attributed grammar** similar to the language defined by [Yacc/Bison](https://www.cs.ccu.edu.tw/~naiwei/cs5605/YaccBison.html).

## Parser Generator
_Mewa_ implements an LALR(1) parser generator for an attributed grammar. The grammar description is read from a source file and has 3 parts:

  * The **configuration declarations** marked with a prefix '%'
  * The **lexeme/token declarations**
  * The **production declarations**

### Configuration Declarations
Configuration declarations start with a '**%**' followed by the command and its arguments separated by spaces.
The following commands are known:

* **%** **LANGUAGE** _name_ **;** Defines the name of the language of this grammar.
* **%** **TYPESYSTEM** _name_ **;** Defines the name of the _Lua_ module without extension _.lua_ implementing the typesystem. The generated _Lua_ module of the compiler will import the type system module with this name. The typesystem module will contain all _Lua_ _AST_ node functions referred to in the grammar.
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
* **%** **INDENTL** _open_ _close_ _nl_ _tabsize_ **;** Defines lexems issued for indentiation. Used to implement **off-side rule** languages. The tokens issued can be referenced in the grammar by the names specified as command arguments.
     1. _open_ is the name of the terminal issued when indentiation is increased.
     2. _close_ is the name of the terminal issued when indentiation is decreased.
     3. _nl_ is the name of the terminal issued when a new line is started (issued after _open_).
     4. _tabsize_ is the number of spaces used as substitute for ```TAB```.
  Here is a code example and the indent events created if ```%INDENTL OPIND CLIND NLIND 4``` is configured:
  ```
  bla()
     if (a == b)
	return 1
     else
	return 2
  ```
  ```
  NLIND [bla()] OPIND NLIND [return 1] CLIND NLIND [else] OPIND NLIND [return 2] CLIND CLIND
  ```

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
   * One of the operators '**{}**' or '**>>**' attaching the _scope_ attributes **scope** or **step** to the nodes of the _AST_ built during parsing.

Here are some declaration patterns described:

1. _name_ **=** _itemlist_ **;**

    * Simple grammar production definition with _name_ as left-hand _nonterminal_ and _itemlist_ as a space-separated list of identifiers (nonterminals or lexeme names) and strings (keywords and operators as implicitly defined _tokens_).

2. _name_/_priority_ **=** _itemlist_ **;**

    * Production definition with a specifier for the rule priority in _SHIFT/REDUCE_ conflicts. The priority specifier is a character **L** or **R** immediately followed by a non-negative integer number. The **L** defines the rule to be left-handed, meaning that _REDUCE_ is preferred in self including rules **L** -> **L** **..**, whereas **R** defines the rule to be right-handed, meaning that _SHIFT_ is preferred in self including rules. The integer number in the priority specifier specifies the preference in _SHIFT/REDUCE_ involving different rules. The production with the higher of two numbers is preferred in _SHIFT/REDUCE_ conflicts. Priorities of _SHIFT_ actions must be consistent for a state.

3. _name_[ /_priority_ ] **=** _itemlist_ **(** _luafunction_ **)** **;**

    * Production definition with Lua function as a single identifier plus an optional argument (a Lua value or table). The function identifier refers to a member of the typesystem module written by the author and loaded by the compiler. The function and its optional argument are attached to the node of the _AST_ defined by its production.

4. _name_[ /_priority_ ] **=** _itemlist_ **(** **>>** **)** **;**

    * Production definition with an increment of the **scope-step**.

5. _name_[ /_priority_ ] **=** _itemlist_ **(** **>>** _luafunction_ **)** **;**

    * Production definition creating an _AST_ node with a Lua function to call and an increment of the **scope-step**.

6. _name_[ /_priority_ ] **=** _itemlist_ **(** **{}** _luafunction_ **)** **;**

    * Production definition creating an _AST_ node with a Lua function to call and a **scope** structure (start and end of the scope).

### Start-Symbol / Start-Production
The first production of the grammar has to be the start-production. The left-handed non-terminal of the start-production is called the start-symbol. The start-symbol must only appear once in the grammar, on the left side of the start production. Unlike most other parser generators, _Mewa_ does not generate a start production to impose these rules, avoiding restrictions on the grammar.

## How the AST is built
The _AST_ is built during syntax analysis with to a fixed schema according your grammar definition and the attribution of nodes. Here are the rules:

 * New _AST_ nodes are pushed on a stack when created. _AST_ nodes are taken from this stack when declared as children of another _AST_ node created.
 * Matching lexemes declared as regular expressions in the 2nd part of the grammar create a new node on the stack.
 * Attributes with a _Lua_ call create a new node on the stack.
 * Nodes created from Lua calls take all elements created on the stack during the matching of their production from the stack and define them as child nodes.
 * The nodes left on the stack after syntax analysis are the root nodes of the _AST_. Usually, it's one node. But there might exist many root nodes during development.

## Compilation Process

The compiler does the syntax analysis and builds the _AST_ in the process. After the syntax analysis, the root nodes of the _AST_ lay on the stack used for building the _AST_ . The _Lua_ functions attached to the root nodes are called by the compiler after the syntax analysis to produce the output. It's in the responsibility of the _Lua_ functions attached to nodes of the _AST_ to invoke the traversal of their sub-nodes.

## Differences to Yacc/Bison
 * _Mewa_ supports only LALR(1), while _Bison_ support a variety of different language classes.
 * Unlike in _Yacc/Bison_, where the lexer can be arbitrarily defined, the Lexer of _Mewa_ is restricted to classes of languages where spaces have no meaning and the _tokens_ can only be described as regular expression matches.
 * The annotation of the rules with code in _Yacc/Bison_ is also different. _Mewa_ has a fixed schema of the _AST_ construction. The actions described as attributes of the grammar are actions of the parser in _Bison/Yacc_, while is _Mewa_ they are actions performed during the tree traversal of the _AST_.
 * The operator precedence in _Yacc/Bison_ has a counterpart with roughly the same expressiveness in _Mewa_. In _Mewa_ priorities with left/right-handed associativity are attached to productions. Unfortunately, the _Mewa_ approach is more error-prone.

### Meaning of Whitespaces
Whitespaces have no meaning in languages describable by _Mewa_ except indentiation if configured with the command **%INDENTL**.
The lexemes of the languages cannot start with whitespaces as the parser skips whitespaces after every lexeme matched and starts matching the next lexeme with the first non-whitespace character after the last match.




