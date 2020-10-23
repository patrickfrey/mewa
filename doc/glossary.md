# Glossary

# Name _Mewa_
Polish name for seagull. It's difficult to find short memorizable names. I stick to names of birds in the polish language without non ascii characters.

## Type System
A set of term rewrite rules to reduce structures of types to types and in this process construct a program that represents the structure. 

## Scope
Pair of cardinal numbers (represented as integers) that define an area the first number defines the start of the scope and the second number defines one number after the last step that belongs to the scope. The scope defines the validity of a definition in the language defined. A definition is valid if the scope includes the scope step of the instruction that queries the definition.

### Example
1. Item 'ABC' defined in scope [1,123]
2. Item 'ABC' defined in scope [5,78]
3. Item 'ABC' defined in scope [23,77]
4. Item 'ABC' defined in scope [81,99]

The Query for 'ABC' in an instruction with scope step 56 assigned, returns the 3rd definition.

## Scope Step
Counter that is incremented for every production in the grammar marked with the operators '>>' or '{}'. The scope step defines the start and the end of the scope assigned to productions by the scope operator '{}'. A scope starts with the scope step counter value when first entering a state with a production marked as '{}' and ends with one step after the value of the scope step when replacing the right side of the production with the left side on the parser stack (reduction).


## Glossary of Formal Languages
### Terminal
A basic item of the language. Further reading in [Wikipedia: Context Free Grammar](https://en.wikipedia.org/wiki/Context-free_grammar).

### Nonterminal
A named structure of the language. Further reading in [Wikipedia: Context Free Grammar](https://en.wikipedia.org/wiki/Context-free_grammar).

### BNF / EBNF / Yacc/Bison
Languages to describe a context free grammar. There exist many dialects for a formal description of a context free language grammar based on BNF. The most silimar to [the grammar of _Mewa_](grammar.md) is the language used in [Yacc/Bison](https://www.cs.ccu.edu.tw/~naiwei/cs5605/YaccBison.html). Further reading in [Wikipedia: Backus-Naur form](https://en.wikipedia.org/wiki/Backus%E2%80%93Naur_form) and [Wikipedia: Extended Backus-Naur form](https://en.wikipedia.org/wiki/Extended_Backus%E2%80%93Naur_form). 

### LALR(1)
The class of language covered by grammars that can be implemented with _Mewa_. Further reading in [Wikipedia: Context Free Grammar](https://en.wikipedia.org/wiki/LALR_parser).

### Lexer
Component that scans the input and produces a stream of items called lexems that are the atoms the grammar of the language is based on. It provides a sequence of items like identifiers, cardinal or floating point numbers, strings or operators instead of characters. In _Mewa_ the _lexer_ is defined as a set of named patterns. The patterns are regular expressions that are matched on the first/next non whitespace character in the input. Contradicting _lexem_ definitions are resoved in _Mewa_ by selecting the longest match or the first definition for matches of the same length.


