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

### Typesystem
A set of term rewrite rules to reduce structures of types to types and in this process construct a program that represents the structure. Further reading [Wikipedia: Type theory](https://en.wikipedia.org/wiki/Type_theory).


## Glossary of Formal Languages
### Terminal
A basic item of the language. Further reading in [Wikipedia: Context Free Grammar](https://en.wikipedia.org/wiki/Context-free_grammar).

### Nonterminal
A named structure of the language. Further reading in [Wikipedia: Context Free Grammar](https://en.wikipedia.org/wiki/Context-free_grammar).

### Context Free Grammar
Type of grammar with rewrite rules that substitute one nonterminal by a (possibly empty) sequence of terminals or nonterminals independent of the context. Also referred to as Chomsky Type 2 grammar. and Further reading in [Wikipedia: Context Free Grammar](https://en.wikipedia.org/wiki/Context-free_grammar).

### LALR(1)
The class of language covered by grammars that can be implemented with _mewa_. Further reading in [Wikipedia: Context Free Grammar](https://en.wikipedia.org/wiki/LALR_parser).

### EBNF
Language to describe a context free grammar. Further reading in [Wikipedia: Extended Backus-Naur form](https://en.wikipedia.org/wiki/Extended_Backus%E2%80%93Naur_form).

#### The _Mewa_ Grammar Description Language ###
_Mewa_ uses a language to describe a grammar derived from EBNF. It does not use commas to separate elements on the right hand of a production and it uses rules with regular expressions to describe lexems. It also restricts itself to classes of languages where space has no meaning. It adds callbacks to the typesystem and some commands to define scope in the language. See [The _Mewa_ Grammar](grammar.md).



