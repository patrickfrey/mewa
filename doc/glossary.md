# Glossary

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

## Terminal
A basic item of the language. Further reading in [Wikipedia: Context Free Grammar](https://en.wikipedia.org/wiki/Context-free_grammar).

## Nonterminal
A named structure of the language. Further reading in [Wikipedia: Context Free Grammar](https://en.wikipedia.org/wiki/Context-free_grammar).

## LALR(1)
The class of language covered by grammars that can be implemented with _mewa_. Further reading in [Wikipedia: Context Free Grammar](https://en.wikipedia.org/wiki/LALR_parser).

## Typesystem
A set of term rewrite rules to reduce structures of types to types and in this process construct a program that represents the structure. Further reading [Wikipedia: Type theory](https://en.wikipedia.org/wiki/Type_theory).



