# Mewa Error Codes
Mewa uses error codes. Here is a list of all error codes and their description.

## System Errors
+ **#1** .. **#200** _Error codes reserved for system errors (errno)_.
A list for Linux can be found [here](https://nuetzlich.net/errno.html).

## Memory Allocation Errors, Buffer Overflow, Runtime and Logic Errors
+ **#400**   _memory allocation error_
+ **#401**   _internal buffer overflow error (some string is not fitting into a local buffer of fixed maximum size)_
+ **#402**   _Logic error_
+ **#403**   _Runtime error exception_
+ **#404**   _Unexpected exception_
+ **#405**   _Error serializing a lua data structure_

## Lua Function Call Argument Check Failures
+ **#407**   _Expected string as argument_
+ **#408**   _Expected integer as argument_
+ **#409**   _Expected non negative integer as argument_
+ **#410**   _Expected positive integer as argument_
+ **#411**   _Expected boolean value as argument_
+ **#412**   _Expected floating point number as argument_
+ **#413**   _Expected table as argument_
+ **#414**   _Expected argument to be a structure (pair of non negative integers, unsigned integer range)_
+ **#415**   _Expected argument to be a list of type,constructor pairs (named or positional)_
+ **#416**   _Expected argument to be a list of strings_
+ **#417**   _Expected argument to be a list of types (integers)_
+ **#418**   _Expected argument to be a list of types (integers) or type,constructor pairs_
+ **#419**   _Expected argument to be not nil_
+ **#420**   _Too few arguments_
+ **#421**   _Too many arguments_
+ **#422**	_Compile error_

## Errors in Lexer
+ **#431**   _Bad character in a regular expression passed to the lexer_
+ **#432**   _Syntax error in the lexer definition_
+ **#433**   _Logic error (array bound read) in the lexer definition_
+ **#434**   _Bad regular expression definition for the lexer_
+ **#435**   _Keyword defined twice for the lexer_

## Compiler Complexity Boundaries
+ **#436**   _Too many instances created (internal counter overflow)_
+ **#437**   _Too complex source file (counter overflow)_

## Mewa Version Mismatch Errors
+ **#447**   _Bad Mewa version_
+ **#448**   _Missing Mewa version_
+ **#449**   _Incompatible Mewa major version. You need a higher version of the Mewa Lua module_

## Typedb Errors
+ **#450**   _Too many arguments in type definition_
+ **#451**   _Priority value out of range_
+ **#452**   _Duplicate definition_
+ **#453**   _Bad weight <= 0.0 given to relation_
+ **#454**   _Bad value tor tag attached to relation, must be an unsigned integer number in {1..32}_
+ **#455**   _Invalid handle (constructor,type,object) assigned. Expected to be a positive/non negative integer number_
+ **#456**   _Invalid processing boundary value. Expected to be a non negative number_

+ **#462**   _Ambiguous definitions of reductions_
+ **#463**   _Error in scope hierarchy: Defined overlapping scopes without one including the other_
+ **#464**   _Invalid reduction definition_

## Grammar Definition Errors
+ **#501**   _Bad character in the grammar definition_
+ **#502**   _Value out of range in the grammar definition_
+ **#503**   _Unexpected EOF in the grammar definition_
+ **#504**   _Unexpected token in the grammar definition_
+ **#505**   _More than one scope marker '{}' of '>>' not allowed in a call definition_
+ **#506**   _Nested structures as AST node function arguments are not allowed_
+ **#521**   _Priority definition for lexems not implemented_
+ **#531**   _Unexpected end of rule in the grammar definition_
+ **#532**   _Some internal pattern string mapping error in the grammar definition_
+ **#541**   _Wrong number of arguments for command (followed by '%') in the grammar definition_
+ **#542**   _Unknown command (followed by '%') in the grammar definition_
+ **#543**   _Mandatory command is missing in grammar definition_

+ **#551**   _Identifier defined as nonterminal and as lexem in the grammar definition not allowed_
+ **#552**   _Unresolved identifier in the grammar definition_
+ **#553**   _Unreachable nonteminal in the grammar definition_. All non-terminals except the start symbol (the left side of the first production) must be referenced. Do comment them out if you do not use them yet.
+ **#554**   _Start symbol referenced on the right side of a rule in the grammar definition_. The reason could be that the start-production is not the first production of the grammar.
+ **#555**   _Start symbol defined on the left side of more than one rule of the grammar definition_
+ **#556**   _The grammar definition is empty_

## Conflicts in Grammar Definition
+ **#557**   _Priority definition conflict in the grammar definition_
+ **#558**   _No accept states in the grammar definition_
+ **#561**   _SHIFT/REDUCE conflict in the grammar definition_
+ **#562**   _REDUCE/REDUCE conflict in the grammar definition_
+ **#563**   _SHIFT/SHIFT conflict in the grammar definition_
+ **#564**   _Conflicts detected in the grammar definition. No output written._

## Automaton Definition Complexity Boundaries
+ **#571**   _To many states in the resulting tables of the grammar_
+ **#572**   _Too many productions defined in the grammar_
+ **#573**   _To many productions in the resulting tables of the grammar_
+ **#574**   _Priority value assigned to production exceeds maximum value allowed_
+ **#575**   _To many nonterminals in the resulting tables of the grammar_
+ **#576**   _To many terminals (lexems) in the resulting tables of the grammar_
+ **#577**   _To many distinct FOLLOW sets of terminals (lexems) in the resulting tables of the grammar_

## Mewa Lua Library Errors
+ **#581**   _Bad key encountered in table generated by Mewa grammar compiler_
+ **#582**   _Bad value encountered in table generated by Mewa grammar compiler_
+ **#583**   _Function defined in Lua call table is undefined_
+ **#584**   _Function context argument defined in Lua call table is undefined_
+ **#585**   _Bad token on the Lua stack of the compiler_
+ **#586**   _Item found with no Lua function defined for collecting it_
+ **#591**   _Lua runtime error (ERRRUN)_
+ **#592**   _Lua memory allocation error (ERRMEM)_
+ **#593**   _Lua error handler error (ERRERR)_
+ **#594**   _Lua error handler error (unknown error)_
+ **#595**   _Lua stack out of memory_
+ **#596**   _Userdata argument of this call is invalid_

## Errors Reported by the Generated Parser
+ **#601**   _Syntax error, unexpected token (expected one of {...})_

## Language Automaton Errors
+ **#602**   _Logic error, the automaton for parsing the language is corrupt_
+ **#603**   _Logic error, the automaton has no goto defined for a follow symbol_
+ **#604**   _Logic error, got into an unexpected accept state_

