## Abstract Syntax Tree (AST) Format of Mewa

The AST (Abstract Syntax Tree) of _Mewa_ is a tree with two types of nodes. Leaf nodes represent a [non-keyword lexeme nodes](#lexeme_node).
Non-leaf node represent [function nodes](#nodefunc_node). AST function nodes are bound to a call to the Lua typesystem module.
The tree is built during the parsing of the program. After parsing the _Mewa_ compiler calls the topmost AST node functions defined.
Each AST node function is responsible for the traversal of its sub-nodes of the AST.

### Note on Keyword Lexemes
Keyword lexemes (lexemes defined as string constants in the rules of the grammar) are dismissed during the built of the AST.
They are considered to only have a contextual meaning in the language selecting a processing path of the items declared.
You can re-add information dismissed this way with the first argument of the AST functions.

<a name="lexeme_node"/>

### Non-Keyword Lexeme Node

#### Structure
| Name    | Type     | Description                                                             |
| :------ | :------- | :---------------------------------------------------------------------- |
| name    | string   | Name of the lexeme assigned to it in lexer section of the grammar.      |
| value   | string   | Selected value of the pattern defined in lexer section of the grammar.  |
| line    | integer  | Number of the line in the source where the lexeme was detected.         |


<a name="nodefunc_node"/>

### AST Function Node

#### Structure
| Name    | Type     | Description                                                                                                    |
| :------ | :------- | :------------------------------------------------------------------------------------------------------------- |
| call    | table    | ([Structure](#nodefunc_description) describing the function to call.                                           |
| scope   | table    | (optional) Scope (pair of integers) attached to the node with the operator **{}** in the grammar description.  |
| step    | integer  | (optional) Scope step (integer) attached to the node with the operator **>>** in the grammar description.      |
| arg     | table    | List of sub nodes, either call structures or [non-keyword lexeme nodes](#lexeme_node).                         |
| line    | integer  | Number of the line in the source where the last lexeme was detected.                                           |

##### Note
There is one additional field reserved in an AST function node to store some data shared between passes in a multi-pass traversal.

<a name="nodefunc_description"/>

### AST Node Function Description
The AST node function descriptions are passed as an array that is part of the compiler definition (member "call") to the compiler.
They are referenced by index in the LALR(1) action table.

#### Structure

| Name    | Type     | Description                                                      |
| :------ | :------- | :--------------------------------------------------------------- |
| name    | string   | Name of the function and the argument object as string.          |
| proc    | function | Lua function to call                                             |
| obj     | any type | 2nd argument (besides node) to pass to the Lua AST node function.|

