# The EBNF Definition Language
The language to define the EBNF has 3 types of declarations.
All declarations are terminated with a semicolon ';'.
Here is the list of declaration types explained.

## Language Attribute Declarations
Language attribute declarations start with a '%' followed by the command and its arguments separated by spaces.
The following commands are known:

* **%** **LANGUAGE** _name_ **;** Defines the name of the language of this grammar as _name_
* **%** **TYPESYSTEM** _name_ **;** Defines the name of the module implementing the typesystem as _name_ without the extension .lua that is implicitely added. The generated lua module of the compiler will include the type system module with this name.
* **%** **IGNORE** _pattern_ **;** Defines a token matching this pattern defined as regular expression quoted in single or double quotes as invisible. It is ignored by the lexer and thus by the compiler. Hence it does not need a name.
* **%** **BAD** _name_ **;** Defines the name of the error token of the lexer. Has no implications but for the debugging output.
* **%** **COMMENT** _start_ _end_ **;** Defines the content between the patterns _start_ and _end_ defined as regular expression quoted in single or double quotes as comment. Comments are ignored by the lexer and thus by the compiler.
* **%** **COMMENT** _start_ **;** Defines the content starting with the pattern _start_ defined as regular expression quoted in single or double quotes until the next end of line as comment. Comments are ignored by the lexer and thus by the compiler.

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
Whitespaces have no meaning in languages describable by _mewa_.
The lexems of the languages cannot start with whitespaces as the parser skips whitespaces after every lexem matched and starts matching the next lexem with the first non whitespace character after the last match.

## Rule Delarations
Rule declarations start with an identifier followed by an assignment '**=**' and the right side of the production.
At the end of each production we can declare one optional call to the typesystem module written in Lua.
Additionally _mewa_ provides the operators '**>>**' and '**{}**' to assign **scope** info to the nodes of the AST initially built.

1. _name_ **=** _itemlist_ **;**
    * Simple EBNF rule definition with _name_ as left hand _nonterminal_ and _itemlist_ as space separated list of identifiers (nonterminals or lexem names) and strings (keywords and operators as implicitely defiend lexems).
2. _name_ **=** _itemlist_ **(** _luafunction_ **)** **;**
    * EBNF rule definition as in (1.) but with _luafunction_ as a single identifier or an pair of identifiers referring to a function and an optional context argument defined in the typesystem Lua module. A rule defined with a function form a node in the resulting AST (abstract syntax tree). The result node has all non keyword/operator lexems or other nodes defined inside its right hand part of the rule that are not bound by other nodes as subnodes.
3. _name_ **=** _itemlist_ **(** **>>** _luafunction_ **)** **;**
    * EBNF rule definition as in (2.) but an with increment defined for the **scope** counter.
4. _name_ **=** _itemlist_ **(** **{}** _luafunction_ **)** **;**
    * EBNF rule definition as in (2.) but with a **scope** structure (start and end of the scope) defined for the result node

## Scope of AST Nodes and Definitions
The scope of a node in the resulting AST is defined as a pair of cardinal numbers **[** _start_ , _end_ **]**.
The definitions of types in the typesystem will include the scope info to select the valid definition amongst definitions with the same name.
A definition is valid if its scope counter is inside the scope checked:
* _counter_ **>=** _start_
* _counter_ **<** _end_.











