## Abstract Syntax Tree (AST) Format of Mewa

The abstract syntax tree of _Mewa_ is a tree with two types of nodes. Either a node represents a [non keyword lexem nodes](#lexem_node) or a [callback node](#callback_node) bound to a call to the typesystem defined in Lua. The tree is built during the parsing of the program. After parsing the _Mewa_ compiler calls the topmost Lua callback functions defined. These toplevel functions are then responsible for the traversal of the sub nodes of the abstract syntax tree. 

### Note on Keyword Lexems
Keyword lexems (lexems defined as string constants in the rules of the grammar) are dismissed during the built of the AST. They are considered to only have a contextual meaning in the language selecting a processing path of the items declared.


<a name="lexem_node"/>

### Non-Keyword Lexem Node

#### Structure
| Name    | Type     | Description                                                             |
| :------ | :------- | :---------------------------------------------------------------------- |
| name    | string   | Name of the lexem assigned to it in lexer section of the grammar.       |
| value   | string   | Selected value of the pattern defined in lexer section of the grammar.  |
| line    | integer  | Number of the line in the source where the lexem was detected.          |


<a name="callback_node"/>

### Lua Callback Node

#### Structure
| Name    | Type     | Description                                                                                                    |
| :------ | :------- | :------------------------------------------------------------------------------------------------------------- |
| call    | table    | ([Structure](#callback_description) describing the function to call.                                           |
| scope   | table    | (optional) Scope (pair of integers) attached to the node with the operator **{}** in the grammar description.  |
| step    | integer  | (optional) Scope step (integer) attached to the node with the operator **>>** in the grammar description.      |
| arg     | table    | List of sub nodes, either call structures or [non keyword lexem nodes](#lexem_node).                           |
| line    | integer  | Number of the line in the source where the last lexem was detected.                                            |

##### Note
There is one additional field reserved in a callback node to store some data shared between passes in a multipass traversal.

<a name="callback_description"/>

### Lua Callback Description
The Lua callback function descriptions are passed as an array that is part of the compiler definition (member "call") to the compiler.
They are referenced by index in the LALR(1) action table.

#### Structure

| Name    | Type     | Description                                                      |
| :------ | :------- | :--------------------------------------------------------------- |
| name    | string   | Name of the function and the argument object as string.          |
| proc    | function | Lua function to call                                             |
| obj     | any type | 2nd argument (besides node) to pass to the Lua callback function.|

