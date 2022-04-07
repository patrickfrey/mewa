# Bridges to Other Tools (for Analysis, Documentation)

## Introduction
The mewa program option --generate-language or -l prints a Lua table as output, that contains all elements of [the grammar of _Mewa_](grammar.md).

### Enriching the Source for the Foreign Tool
For interfacing with a tool that requires elements that are not part the mewa language description, you have the possibility
to add some information in the comments of the _grammar definition_ that can be incorporated into the the source for the foreign tool.
The elements are declared in a JavaDoc-like manner with a **@** followed by an identifier that names the attribute followed by the rest of the line.
Lines not prefixed by an attribute are attached to the previous attribute if specified. The attributes declared this way are attached to the
declaration following the comment.

### Example
The following excerpt of a mewa language description defines the lexeme of an unsigned integer and attaches two attributes 'rule' and 'descr' to the table
generated.

#### Input
```
# @rule DIGIT ::= ("0"|"1"|"2"|"3"|"4"|"5"|"6"|"7"|"8"|"9")
#	UINTEGER ::= DIGIT*
# @descr Sequence of digits
UINTEGER: '[0123456789]+';
```

#### Output (beautified)
```
{
	op="TOKEN",
	name="UINTEGER",
	pattern="[0123456789]+",
	rule= {'DIGIT ::= ("0"|"1"|"2"|"3"|"4"|"5"|"6"|"7"|"8"|"9")',"UINTEGER ::= DIGIT*"},
	descr= {"Sequence of digits"},
	line=26},
```

#### Explanation
The attributes '@rule' and '@decr' with the following content are detected in the comments, extracted and attached to the next declaration,
the lexeme "UINTEGER" in this case. Attributes can be attached to all declarations in the file except "%LANGUAGE", "%TYPESYSTEM" and "%CMDLINE".

### Reserved attribute names
Some attributes names cannot be used for enriching the Lua table of the _grammar definition_. The following list describes them:

 * **op** Name of the operation, name of a '%' command in the _grammar definition_ or "TOKEN" or "PROD" for the rules
 * **name** Name of a token issued by the lexer
 * **pattern** Regular expression used for detecting a token in the source
 * **select** Selection of a matching regular sub-expression, the token issued by the lexer.
 * **open** The name of the terminal issued when indentiation is increased (%INDENTL command), the string starting a comment (%COMMENT command).
 * **close** The name of the terminal issued when indentiation is decreased (%INDENTL command), the string terminating a comment (%COMMENT command).
 * **nl** Non-terminal issued as new-line in **off-side rule** languages (%INDENTL command).
 * **tabsize** The number of spaces used as substitute for ```TAB``` (%INDENTL command).
 * **priority** The priority of a production
 * **left** Left hand of a production, name of the production
 * **right** Right hand of a production, a list of terminals or nonterminals.
 * **scope** Scope operation '>>' or '{}' attached to a production.
 * **call** Lua call attached to a production.
 * **line** Line number in the original _grammar definition_.

## Lua table structure
The following tree describes the Lua table attributes of the generated table.

 * LANGUAGE <string>     : Name of the language declared with '%LANGUAGE' in the _grammar definition language_.
 * TYPESYSTEM <string>   : Typesystem module to include.
 * CMDLINE <string>      : Command-line parser module of the compiler.
 * RULES <table>         : List of lexeme, comment, production declarations
     * op <string>       : Type of the element, one of 'TOKEN' (token defined by a regular expression), 'PROD' (production), or a command of [the grammar of _Mewa_](grammar.md) prefixed with '%' (without the ones already mentioned)
     * name <string      : in case of op='TOKEN', name of the token issued by the lexer
     * pattern <string>  : in case of op='TOKEN', regular expression used for detecting a token in the source
     * select <integer>  : in case of op='TOKEN', selection of a matching regular sub-expression, the token issued by the lexer.
     * open <string>     : in case of op='INDENTL', the name of the terminal issued when indentiation is increased, in case of op='COMMENT', the string starting a  comment
     * close <string>    : in case of op='INDENTL', the name of the terminal issued when indentiation is decreased, in case of op='COMMENT', the string terminating a comment
     * nl <string>       : in case of op='INDENTL', non-terminal issued as new-line
     * tabsize <integer> : in case of op='INDENTL', the number of spaces used as substitute for ```TAB```
     * priority <string> : in case of op='PROD', the priority of the production, one of {'L1',L2',...,'R1','R2',...}
     * left <string>     : in case of op='PROD', the left hand of the production, name of the production
     * right <table>     : in case of op='PROD', the right hand of a production, a list of terminals or nonterminals.
	 * type <string> : "symbol" if the list element represents a keword or an operator, "name" if the list element represents a non-terminal (declared as "PROD") or a named lexeme (declared as "TOKEN").
	 * value <string>: name or string value of the production element
      * scope <string>   : in case of op='PROD', the scope operator '>>' or '{}' attached to a production.
      * call <string>    : in case of op='PROD', the Lua call attached to a production with a constant first argument if specified.
      * line <integer>   : line number of the command or rule in the original _grammar definition_.

## Example Output
The example language1 creates [this table structure](../tests/language1_grammar.lua.exp).

## Example Lua Module
The example Lua module [printGrammarFromImage.lua](../examples/printGrammarFromImage.lua) creates a valid mewa grammar file from the table structure described here.


