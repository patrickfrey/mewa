# The mewa.typedb API

The type database API referred to as _mewa.typedb_ offers you the functions needed to build up the typesystem of your language. 

## Create a New Type Database
```lua
mewa = require("mewa")
typedb = mewa.typedb()
```
The variable typedb holds now the type database created.

## User Defined Objects in Different Scopes

### typedb:set_instance
Set the instance for the object with name _name_ to be _object_ for the scope _scope_.

#### Parameter
| #   | Name     | Type              | Description                                           |
| --- | -------- | ----------------- | ----------------------------------------------------- |
| 1st | name     | string            | Name of the object we declare an instance of          |
| 2nd | scope    | pair of integers  | The scope if the instance                             |
| 3rd | instance | any type not nil  | The instance of the object                            |


### typedb:get_instance
Get the instance for the object with name _name_ for the innermost scope including the scope step _step_.

#### Parameter
| #      | Name     | Type     | Description                                           |
| ------ | -------- | -------- | ----------------------------------------------------- |
| 1st    | name     | string   | Name of the object we are addressing                  |
| 2nd    | step     | integer  | Scope step to locate the instance we are referring to |
| Return |          | *        | The instance of the object we addressed               |

### Example typedb:set_instance / typedb:get_instance
Implementation of a register allocator for LLVM IR.

#### Source
```lua
mewa = require("mewa")
typedb = mewa.typedb()

-- A register allocator as a function counting from 1, returning the LLVM register identifiers:
function register_allocator()
        local i = 0
        return function ()
                i = i + 1
                return "%" .. i
        end
end

-- Create a register allocator for the scope [0..1000]:
typedb:set_instance( "register", {0,1000}, register_allocator())
-- Create a register allocator for the scope [2..300]:
typedb:set_instance( "register", {2,300}, register_allocator())

-- Allocate a register in the scope step 45, that is the first register in [2..300]:
print( typedb:get_instance( "register", 45)());
-- Allocate a register in the scope step 500, that is the first register in [0..1000]:
print( typedb:get_instance( "register", 500)());
-- Allocate a register in the scope step 49, that is the second register in [2..300]:
print( typedb:get_instance( "register", 49)());

```
#### Output
```
%1
%1
%2

```
### typedb:instance_tree
Get the tree of scopes with the instances defined. 

#### Parameter
| #      | Name     | Type      | Description                                           |
| ------ | -------- | --------- | ----------------------------------------------------- |
| 1st    | name     | string    | Name of the object we are addressing                  |
| Return |          | structure | Tree structure with a node for each definition scope  |

#### Remark
This is a costly operation and not intended to be used as data structure for the compiler itself. 
It is thought as help during development.

#### Fields of the Returned Tree Structure
| Name     | Type      | Description                                                                             |
| -------- | --------- | --------------------------------------------------------------------------------------- |
| chld     | function  | Iterator function to iterate on the child nodes (also tree structures like this node).  |
| scope    | function  | Function returning a pair of integers describing the scope of this node.                |
| instance | function  | Function returning the instance defined for this node.                                  |

## Define Types
### typedb:def_type
Define a type. 

#### Parameter
| #      | Name         | Type             | Description                                                                                        |
| ------ | ------------ | ---------------- | -------------------------------------------------------------------------------------------------- |
| 1st    | scope        | pair of integers | Scope of the type defined                                                                          |
| 2nd    | context-type | integer          | Type referring to the context of the type or 0 if the type is not a member of some other structure |
| 3rd    | name         | string           | Name of the type defined                                                                           |
| 4th    | constructor  | any type not nil | Constructor describing how the type is built                                                       |
| 5th    | parameter    | array of types   | Array of type/constructor pairs                                                                    |
| 6nd    | priority     | integer          | Priority of the definition, Higher priority overwrites lower priority definition.                  |
| Return |              | integer          | Handle assigned as identifier of the type                                                          |

## Define Reductions
### typedb:def_reduction
Define a reduction from a type resulting in another type with a tag to classify it.

#### Parameter
| #      | Name         | Type             | Description                                                                                                    |
| ------ | ------------ | ---------------- | -------------------------------------------------------------------------------------------------------------- |
| 1st    | scope        | pair of integers | Scope of the reduction defined                                                                                 |
| 2nd    | dest-type    | integer          | Resulting type of the reduction                                                                                |
| 3rd    | src-type     | integer          | Origin type of the reduction                                                                                   |
| 4th    | constructor  | any type not nil | Constructor describing how the type reduction is implemented.                                                  |
| 5th    | tag          | integer 1..32    | Tag assigned to the reduction, used to restrict a type search or derivation to selected classes of reductions. |
| 6nd    | weight       | number           | Weight assigned to the reduction, used to calculate the shortest path of reductions for resolving types.       |

## Type Searches or Derivations

### typedb:reduction_tagmask
Create a set of tags for type searches (typedb:resolve_type) or derivations (typedb:derive_type).

#### Parameter
| #      | Name         | Type    | Description                                 |
| ------ | ------------ | ------- | ------------------------------------------- |
| ....   | tag          | integer | Element of a list of tags to add to the set |


### typedb:derive_type
Finds the shortest path (sum of reduction weights) of reductions of the classes selected by the _tagmask_ parameter. Throws an error if the result is ambiguous.

#### Parameter
| #      | Name         | Type              | Description                                                                                  |
| ------ | ------------ | ----------------- | -------------------------------------------------------------------------------------------- |
| 1st    | step         | integer           | Scope step covered by the scopes of the result candidates.                                   |
| 2nd    | dest-type    | integer           | Resulting type to derive.                                                                    |
| 3rd    | src-type     | integer           | Start type of the reduction path leading to the result type.                                 |
| 4th    | tagmask      | bit set (integer) | Set of tags (built with typedb:reduction_tagmask) that selects the reduction classes to use. |
| Return |              | array             | List of type/constructor pairs as structures with "type","constructor" member names.         |

### typedb:resolve_type
Finds the matching type with the searched name and a context-type derivable from the searched context-type, that has the shortest path (sum of reduction weights) of reductions of the classes selected by the _tagmask_ parameter. Throws an error if the the reduction path of the context-type is ambiguous.
The returned list of candidates (2nd return value) has to be inspected by the client to find the best match.

#### Parameter
| #          | Name         | Type              | Description                                                                                                       |
| ---------- | ------------ | ----------------- | ----------------------------------------------------------------------------------------------------------------- |
| 1st        | step         | integer           | Scope step covered by the scopes of the result candidates.                                                        |
| 2nd        | context-type | integer           | Type referring to the context of the type or 0 if the type is not a member of some other structure                |
| 3rd        | name         | string            | Name of the type searched                                                                                         |
| 4th        | tagmask      | bit set (integer) | Set of tags (built with typedb:reduction_tagmask) that selects the reduction classes to use.                      |
| Return 1st |              | array             | List of context-type reductions, type/constructor pairs as structures with "type","constructor" member names.     |
| Return 2nd |              | array             | List of candidates found, differing in the parameters. The final result has to be client matching the parameters. |


## Inspect Type Attributes
### typedb:type_name
Get the name of the type as it was specified as argument of 'typedb:def_type'.

#### Parameter
| #          | Name | Type              | Description             |
| ---------- | ---- | ----------------- | ----------------------- |
| 1st        | type | integer           | Type identifier         |


### typedb:type_string
Get the full signature of the type as string. This is the full name of the context type, the name and the full name of all parameters defined.

#### Parameter
| #          | Name | Type              | Description             |
| ---------- | ---- | ----------------- | ----------------------- |
| 1st        | type | integer           | Type identifier         |


### typedb:type_constructor
Get the constructor of a type.

#### Parameter
| #          | Name | Type              | Description             |
| ---------- | ---- | ----------------- | ----------------------- |
| 1st        | type | integer           | Type identifier         |


### typedb:type_reduction
Get the constructor of a type.

#### Parameter
| #          | Name | Type              | Description             |
| ---------- | ---- | ----------------- | ----------------------- |
| 1st        | type | integer           | Type identifier         |


### typedb:type_reduction
Get the constructor of a reduction from a type to another if it exists.

#### Parameter
| #      | Name         | Type              | Description                                                                                                        |
| ------ | ------------ | ----------------- | ------------------------------------------------------------------------------------------------------------------ |
| 1st    | step         | integer           | Scope step covered by the scopes of the reductions considered as result.                                           |
| 2nd    | dest-type    | integer           | Resulting type to of the reduction.                                                                                |
| 3rd    | src-type     | integer           | Start type of the reduction.                                                                                       |
| 4th    | tagmask      | bit set (integer) | Set of tags (built with typedb:reduction_tagmask) that selects the reduction classes to consider.                  |
| Return |              | any type          | Constructor of the reduction if it exists or nil if it is not defined by a scope covering the argument scope step. |


### typedb:type_reductions
Get the list of reductions defined for a type.

#### Parameter
| #      | Name         | Type              | Description                                                                                        |
| ------ | ------------ | ----------------- | -------------------------------------------------------------------------------------------------- |
| 1st    | step         | integer           | Scope step covered by the scopes of the reductions considered as result.                           |
| 2nd    | type         | integer           | Start type of the reductions to inspect.                                                           |
| 3rd    | tagmask      | bit set (integer) | Set of tags (built with typedb:reduction_tagmask) that selects the reduction classes to consider.  |
| Return |              | array             | List of type/constructor pairs as structures with "type","constructor" member names.               | 



## Type and Reduction Instrospection for Debugging:

### typedb:type_tree
Get the tree of scopes with the list of types defined there. 

#### Parameter
| #      | Name     | Type      | Description                                           |
| ------ | -------- | --------- | ----------------------------------------------------- |
| Return |          | structure | Tree structure with a node for each definition scope  |

#### Fields of the Returned Tree Structure
| Name     | Type      | Description                                                                             |
| -------- | --------- | --------------------------------------------------------------------------------------- |
| chld     | function  | Iterator function to iterate on the child nodes (also tree structures like this node)   |
| scope    | function  | Function returning a pair of integers describing the scope of this node                 |
| list     | function  | Function returning the list of types (integers) defined in the scope of this node       |

#### Remark
This is a costly operation and not intended to be used as data structure for the compiler itself. 
It is thought as help during development.


### typedb:reduction_tree
Get the tree of scopes with the list of reductions defined there. 

#### Parameter
| #      | Name     | Type      | Description                                           |
| ------ | -------- | --------- | ----------------------------------------------------- |
| Return |          | structure | Tree structure with a node for each definition scope  |

#### Fields of the Returned Tree Structure
| Name     | Type      | Description                                                                               |
| -------- | --------- | ----------------------------------------------------------------------------------------- |
| chld     | function  | Iterator function to iterate on the child nodes (also tree structures like this node)     |
| scope    | function  | Function returning a pair of integers describing the scope of this node                   |
| list     | function  | Function returning the list of reductions (structures) defined in the scope of this node  |

#### Members of the Tree Structure List Elements
| Name          | Type             | Description                                                     |
| ------------- | ---------------- | --------------------------------------------------------------- |
| to            | integer          | Destination type of the reduction                               |
| from          | integer          | Origin type of the reduction                                    |
| constructor   | any type not nil | Constructor describing how the type reduction is implemented.   |
| tag           | integer 1..32    | Tag assigned to the reduction.                                  |
| weight        | number           | Weight assigned to the reduction.                               |

#### Remark
This is a costly operation and not intended to be used as data structure for the compiler itself. 
It is thought as help during development.




