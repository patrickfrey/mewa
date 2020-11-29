# The Grammar Definition Language
_Mewa_ uses a language to describe a grammar similar to the language defined by [Yacc/Bison](https://www.cs.ccu.edu.tw/~naiwei/cs5605/YaccBison.html). If you are familiar with _Yacc/Bison_ you won't have problems to adapt to _Mewa_.

## Differences to Yacc/Bison
Unlike in _Yacc/Bison_ where the lexer can be arbitrarily defined, the Lexer of _Mewa_ is restricted to classes of languages where space has no meaning and the lexems can only be described as regular expression matches. The annotation of the rules with code in _Yacc/Bison_ is also different and more restricted in _Mewa_. Instead of arbitrary code to be generated and invoked when the annotated rule matches, _Mewa_ has one Lua callback function per rule that is called on a match. The operator precedence in _Yacc/Bison_ has a counterpart with roughly the same expressiveness in _Mewa_. In _Mewa_ priorities with left/right handed assoziativity are attached to productions and not to terminals (operators). Attaching the precedence info to rules (_Mewa_) and not to the terminals (_Yacc/Bison_) can theoretically lead to more reported conflicts in the grammar definition. In practice both ways are equivalent. While the _Yacc/Bison_ is a little bit more intuitive (it's natural to talk about operator precedence), with _Mewa_ you have one class of rules less to write.

## Language Attribute Declarations
Language attribute declarations start with a '**%**' followed by the command and its arguments separated by spaces.
The following commands are known:

* **%** **LANGUAGE** _name_ **;** Defines the name of the language of this grammar as _name_
* **%** **TYPESYSTEM** _name_ **;** Defines the name of the _Lua_ module without extension _.lua_ implementing the typesystem as _name_. that is implicitely added. The generated lua module of the compiler will include the type system module with this name. The typesystem module will contain all Lua callback definitions referred to in the grammar.
* **%** **CMDLINE** _name_ **;** Defines the name of the Lua module without extension _.lua_ implementing command line parser of the compiler as _name_. If not specified _Mewa_ generates code that calls the compiler with the first command line argument. A module defined for the command line can implement some argument checking and initialize some compiler behaviour specified by command line options. The module defined by CMDLINE has to export a method _parse_ with 3 arguments: the name of the language, the typesystem structure and the array of command line arguments. It is expected to return 3 values: the input file name, the output file name, the debug output file name. Only the first return value is mandatory. And example of a command line parser can be found [here](../examples/cmdlinearg.lua). 
* **%** **COMMENT** _start_ _end_ **;** Defines the content between the patterns _start_ and _end_ defined as regular expression quoted in single or double quotes as comment. Comments are ignored by the lexer and thus by the compiler.
* **%** **COMMENT** _start_ **;** Defines the content starting with the pattern _start_ defined as regular expression quoted in single or double quotes until the next end of line as comment. Comments are ignored by the lexer and thus by the compiler.
* **%** **IGNORE** _pattern_ **;** Defines a token matching this pattern defined as regular expression quoted in single or double quotes as invisible. It is ignored by the lexer and thus by the compiler. Hence it does not need a name.
* **%** **BAD** _name_ **;** Defines the name of the error token of the lexer. Has no implications but for the debugging output.

## Lexem Declarations
Lexem declarations start with an identifier followed by a colon '**:**', a pattern and an optional selection index:

* _lexemname_ **:** _pattern_ **;**
* _lexemname_ **:** _pattern_ _select_ **;**

The name _lexemname_ defines the identifier this lexem can be referred to in the grammar.
The regular expression string _pattern_ quoted in single or double quotes defines the pattern that matches the lexem.
You can have multiple declarations with the same name _lexemname_.
The optional cardinal number _select_ defines the index of the subexpression of the regular expression match to select as the value of the lexem recognized. If _0_ or nothing is specified the whole expression match is taken as lexem value.
If multiple patterns match at the same source position then the longest match is taken. If two matches have the same length, the first declaration is chosen.
Keywords and operators of the the grammar do not have to be declared in the lexer section but can be directly referred to as strings in the rule section of the grammar.

### Meaning of Whitespaces
Whitespaces have no meaning in languages describable by _Mewa_.
The lexems of the languages cannot start with whitespaces as the parser skips whitespaces after every lexem matched and starts matching the next lexem with the first non whitespace character after the last match.

## Rule Delarations
Rule declarations start with an identifier followed by an assignment '**=**' and the right side of the production.
At the end of each production we can declare one optional call to the typesystem module written in Lua.
Additionally _Mewa_ provides the operators '**>>**' and '**{}**' to assign **scope** info to the nodes of the AST initially built.

1. _name_ **=** _itemlist_ **;**
    * Simple grammar rule definition with _name_ as left hand _nonterminal_ and _itemlist_ as space separated list of identifiers (nonterminals or lexem names) and strings (keywords and operators as implicitely defiend lexems).
2. _name_/_priority_ **=** _itemlist_ **;**
    * Rule definition as 1. but with a specifier for the rule priority in _SHIFT/REDUCE_ conflicts. The priority specifier is a character **L** or **R** immediately followed by a cardinal number. The **L** defines the rule to be left handed, meaning that _REDUCE_ is prefered in self including rules **L** -> **L** **..**, whereas **R** defines the rule to be right handed, meaning that _SHIFT_ is prefered in self including rules. The cardinal number in the priority specifier specifies the preference in _SHIFT/REDUCE_ involving different rules. The production with the higher of two numbers is preferred in _SHIFT/REDUCE_ conflicts. Priorities of SHIFT actions must be consistent for a state.
3. _name_[ /_priority_ ] **=** _itemlist_ **(** _luafunction_ **)** **;**
    * Rule definition as in (1. or 2.) but with _luafunction_ as a single identifier or an pair of identifiers referring to a function and an optional context argument defined in the typesystem Lua module. A rule defined with a function form a node in the resulting AST (abstract syntax tree). The result node has all non keyword/operator lexems or other nodes defined inside its right hand part of the rule that are not bound by other nodes as subnodes.
4. _name_[ /_priority_ ] **=** _itemlist_ **(** **>>** _luafunction_ **)** **;**
    * Rule definition as in (1. or 2.) but an with increment defined for the **scope step**.
5. _name_[ /_priority_ ] **=** _itemlist_ **(** **{}** _luafunction_ **)** **;**
    * Rule definition as in (1. or 2.) but with a **scope** structure (start and end of the scope) defined for the result node

## Scope of AST Nodes and Definitions
The **scope** of a node in the resulting AST is defined as a pair of cardinal numbers **[** _start_ , _end_ **]**.
The definitions of types in the typesystem will include the scope structure to select the valid definition amongst definitions with the same name.
A definition is valid if its **scope step** is inside the scope checked:
* _step_ **>=** _start_
* _step_ **<** _end_.











