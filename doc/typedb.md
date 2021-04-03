# The mewa.typedb API

The type database API referred to as _mewa.typedb_ offers you the functions needed to build up the typesystem of your language. 

## Table of Contents
1. [Create a New Type Database](#createTypeDb)
1. [Implementing the Concept of Scope](#scopeImpl)
    * [typedb:scope](#scope)
    * [typedb:step](#step)
1. [User Defined Objects Assigned to Scopes](#instance)
    * [typedb:set_instance](#set_instance)
    * [typedb:get_instance](#get_instance)
    * [typedb:this_instance](#this_instance)
1. [Definition and Lookup of Types](#types)
    * [typedb:def_type](#def_type)
    * [typedb:def_type_as](#def_type_as)
    * [typedb:get_type](#get_type)
    * [typedb:get_types](#get_types)
1. [Reductions](#reductions)
    * [typedb:def_reduction](#def_reduction)
    * [typedb:reduction_tagmask](#reduction_tagmask)
    * [typedb:get_reduction](#get_reduction)
    * [typedb:get_reductions](#get_reductions)
1. [Derive and Resolve Types](#deriveAndResolveTypes)
    * [typedb:derive_type](#derive_type)
    * [typedb:resolve_type](#resolve_type)
1. [Type Attributes](#typeAttributes)
    * [typedb:type_name](#type_name)
    * [typedb:type_context](#type_context)
    * [typedb:type_string](#type_string)
    * [typedb:type_parameters](#type_parameters)
    * [typedb:type_nof_parameters](#type_nof_parameters)
    * [typedb:type_constructor](#type_constructor)
    * [typedb:type_scope](#type_scope)
1. [Instrospection for Debugging](#introspection)
    * [typedb:instance_tree](#instance_tree)
    * [typedb:type_tree](#type_tree)
    * [typedb:reduction_tree](#reduction_tree)


<a name="createTypeDb"/>

## Create a New Type Database
```lua
mewa = require("mewa")
typedb = mewa.typedb()
```
The variable typedb holds now the type database created.


<a name="#scopeImpl"/>

## Implementing the Concept of Scope

<a name="scope"/>

### typedb:scope
Get or/and set the current scope. All methods defining objects dependent on a scope like

 - [typedb:set_instance](#set_instance)
 - [typedb:this_instance](#this_instance)
 - [typedb:def_type](#def_type)
 - [typedb:def_type_as](#def_type_as)
 - [typedb:get_type](#get_type)
 - [typedb:get_types](#get_types)
 - [typedb:def_reduction](#def_reduction)

are referring to the current scope. Without parameter the function just returns the current scope.
With a parameter the function sets the current scope to the parameter value and returns the previously defined value for the current scope.

#### Note
This function was designed to support the mapping of AST nodes to scopes in the tree traversal: You set the current scope when you enter the node and restore the old current scope value when leaving the node.

#### Parameter
| #          | Name     | Type              | Description                                                                                                |
| :--------- | :------- | :---------------- | :--------------------------------------------------------------------------------------------------------- |
| 1st        | scope    | table             | (optional) pair of integers describing the scope to set as the current one                                 |
| 2nd        | step     | integer           | (optional) integer describing the scope step to set as the current one, set to scope[2]-1 if not specified |
| Return 1st |          | table             | pair of integers describing the (previously defined) current scope                                         |
| Return 2nd |          | integer           | integer describing the (previously defined) current scope step                                             |


<a name="step"/>

### typedb:step
Get or/and set the current scope step. All methods retrieving objects dependent on a scope like

 - [typedb:get_instance](#get_instance)
 - [typedb:derive_type](#derive_type)
 - [typedb:resolve_type](#resolve_type)
 - [typedb:get_reduction](#get_reduction)
 - [typedb:get_reductions](#get_reductions)

are referring to the current scope step. Without parameter the function just returns the current scope step.
With a parameter the function sets the current scope step to the parameter value and returns the previously defined value for the current scope step.

#### Note
This function was designed to support the mapping of AST nodes to scope steps in the tree traversal in the same way as [typedb:scope](#scope).

#### Parameter
| #      | Name     | Type              | Description                                                    |
| :----- | :------- | :---------------- | :------------------------------------------------------------- |
| 1st    | step     | integer           | (optional) integer describing the scope step                   |
| Return |          | integer           | integer describing the (previously defined) current scope step |

#### Remark
The call of [typedb:scope](#scope) with parameter sets the scope step implicitly to the start of the scope.
_If you are not relying on a scope step different from that, you don't have to call scope step for every scope step value provided by the grammar description_.


<a name="instance"/>

## User Defined Object Instances in Different Scopes

<a name="set_instance"/>

### typedb:set_instance
Set the instance for the object with name _name_ to be _object_ for the current scope (set with [typedb::scope](#scope)).

#### Parameter
| #   | Name     | Type              | Description                                            |
| :-- | :------- | :---------------- | :----------------------------------------------------- |
| 1st | name     | string            | Name of the object we declare an instance of           |
| 2nd | instance | any type not nil  | The instance of the object                             |

#### Remark
The object handles are currently not recycled. This means that every object update with ```set_instance``` creates a new handle and stores the object passed with this handle as key in a Lua table. As consequence, old instances are not freed when overwritten with ```set_instance```. In the example _language1_ I use ```this_instance``` to get the current object for update and ```set_instance``` only if there is no object declared for the current scope and a new one has to be created.

<a name="get_instance"/>

### typedb:get_instance
Get the instance for the object with name _name_ for the innermost scope including the current scope step (set with [typedb::scope](#scope) or [typedb::step](#step)).

#### Parameter
| #      | Name     | Type     | Description                                                                                                      |
| :----- | :------- | :------- | :--------------------------------------------------------------------------------------------------------------- |
| 1st    | name     | string   | Name of the object we are addressing                                                                             |
| Return |          | *        | The instance of the object we addressed or *nil* if not defined in an enclosing scope of the current scope step. |

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
                return "%R" .. i
        end
end

-- Create a register allocator for the scope [0..1000]:
typedb:scope( {0,1000}); typedb:set_instance( "register", register_allocator())
-- Create a register allocator for the scope [2..300]:
typedb:scope( {2,300}); typedb:set_instance( "register", register_allocator())

-- Allocate a register in the scope step 45, that is the first register in [2..300]:
typedb:step( 45);  print( typedb:get_instance( "register")());
-- Allocate a register in the scope step 500, that is the first register in [0..1000]:
typedb:step( 500);  print( typedb:get_instance( "register")());
-- Allocate a register in the scope step 49, that is the second register in [2..300]:
typedb:step( 49);  print( typedb:get_instance( "register")());

```
#### Output
```
%R1
%R1
%R2

```

<a name="this_instance"/>

### typedb:this_instance
Get the instance for the object with name _name_ assigned to the current scope (set with [typedb::scope](#scope) or [typedb::step](#step)).

#### Parameter
| #      | Name     | Type     | Description                                                                           |
| :----- | :------- | :------- | :------------------------------------------------------------------------------------ |
| 1st    | name     | string   | Name of the object we are addressing                                                  |
| Return |          | *        | The instance of the object we addressed or *nil* if not defined in the current scope. |


<a name="types"/>

## Definition and Lookup of Types

<a name="def_type"/>

### typedb:def_type
Define a type and describe how the type is built by assigning a constructor to it. 
The scope of the newly defined type has been set with the last call of the setter [typedb::scope](#scope). 

#### Parameter
| #      | Name         | Type             | Description                                                                                                   |
| :----- | :----------- | :--------------- | :------------------------------------------------------------------------------------------------------------ |
| 1st    | context-type | integer          | Type referring to the context of the type or 0 if the type is not a member of some other structure            |
| 2nd    | name         | string           | Name of the type defined                                                                                      |
| 3rd    | constructor  | any type         | (optional) Constructor describing how the type is built (*nil* if undefined)                                  |
| 4th    | parameter    | table            | (optional) Array of type/constructor pair or type handles without constructor. (*)                            |
| Return |              | integer          | identifier assigned to the type or -1 if the definition is a duplicate                                        |

##### Remark (*)
To put no parameters on the same level as an empty parameter list is as declaring *null* and *empty* as equivalent as we do here is dangerous in language design.
Be aware of the pitfalls and try to separate the types that can have parameters and the ones that don't by an appropriate naming scheme.


<a name="def_type_as"/>

### typedb:def_type_as
Define a new type representing another type already defined.
The newly defined type name can be used as a kind of synonym for the specified type in the current scope.
The scope of the newly defined type has been set with the last call of the setter [typedb::scope](#scope). 

#### Parameter
| #      | Name         | Type             | Description                                                                                                   |
| :----- | :----------- | :--------------- | :------------------------------------------------------------------------------------------------------------ |
| 1st    | context-type | integer          | Type referring to the context of the type or 0 if the type is not a member of some other structure            |
| 2nd    | name         | string           | Name of the type defined                                                                                      |
| 3rd    | type         | any type         | Type handle of the type this type definition is a synonym for in the current scope                            |
| Return |              | integer          | type handle passed as third argument or -1 if the definition is a duplicate                                   |

#### Remark

The type defined like this has the same constructor, therefore it cannot be used as alias for a type with a built constructor.


<a name="get_type"/>

### typedb:get_type
Get a type definition defined in the current scope, without lookup in enclosing scopes and without any reductions of the context type.
The current scope has been set with the last call of the setter [typedb::scope](#scope).
Same as [typedb:get_types](#get_types) but filtering the one instance with all parameters matching.

#### Parameter
| #      | Name         | Type             | Description                                                                                                 |
| :----- | :----------- | :--------------- | :---------------------------------------------------------------------------------------------------------- |
| 1st    | context-type | integer          | Type referring to the context of the type or 0 if the type is not a member of some other structure          |
| 2nd    | name         | string           | Name of the type defined                                                                                    |
| 3rd    | parameter    | table            | (optional) Array of type handles (integers) or *nil* if no parameters defined                               |
| Return |              | integer          | identifier assigned to the type or *nil* if not found                                                           |


<a name="get_types"/>

### typedb:get_types
Get all type definitions without matching the parameters defined in the current scope, without lookup in enclosing scopes and without any reductions of the context type.
The current scope has been set with the last call of the setter [typedb::scope](#scope). 
Same as [typedb:get_type](#get_type) but retrieving all instances differing in the parameters attached.

#### Parameter
| #      | Name         | Type             | Description                                                                                         |
| :----- | :----------- | :--------------- | :-------------------------------------------------------------------------------------------------- |
| 1st    | context-type | integer          | Type referring to the context of the type or 0 if the type is not a member of some other structure  |
| 2nd    | name         | string           | Name of the type defined                                                                            |
| Return |              | table            | List of types                                                                                       |


<a name="reductions"/>

## Define Reductions

<a name="def_reduction"/>

### typedb:def_reduction
Define a reduction from a type resulting in another type with a tag to classify it.
The scope of the newly defined reduction has been set with the last call of the setter [typedb::scope](#scope). 

#### Parameter
| #      | Name         | Type             | Description                                                                                                       |
| :----- | :----------- | :--------------- | :---------------------------------------------------------------------------------------------------------------- |
| 1st    | dest-type    | integer          | Resulting type of the reduction                                                                                   |
| 2nd    | src-type     | integer          | Origin type of the reduction                                                                                      |
| 3rd    | constructor  | any type         | Constructor describing how the type reduction is implemented (*nil* if undefined ~ identity).                     |
| 4th    | tag          | integer 1..32    | Tag assigned to the reduction, used to restrict a type search or derivation to selected classes of reductions.    |
| 5th    | weight       | number           | (optional) Weight of the reduction in the calculation of the shortest path when resolving types. Default is 0.0   |


<a name="reduction_tagmask"/>

### typedb:reduction_tagmask
Create a set of tags for selecting a set of reductions valid in the context of a type search.

#### Parameter
| #      | Name         | Type    | Description                                 |
| :----- | :----------- | :------ | :------------------------------------------ |
| ....   | tag          | integer | Element of a list of tags to add to the set |


<a name="get_reduction"/>

### typedb:get_reduction
Get the constructor of a reduction from a type to another if it exists.
The scope step of the search that defines the valid reduction candidates has been set with the last call of the setter [typedb::step](#step) or [typedb::scope](#scope). 

#### Parameter
| #          | Name         | Type     | Description                                                                                                         |
| :--------- | :----------- | :------- | :------------------------------------------------------------------------------------------------------------------ |
| 1st        | dest-type    | integer  | Resulting type to of the reduction.                                                                                 |
| 2nd        | src-type     | integer  | Start type of the reduction.                                                                                        |
| 3rd        | tagmask      | integer  | Set (bit-set) of tags (*) that selects the reduction classes to consider.                                           |
| Return 1st |              | number   | Weight of the reduction if it exists in a scope and the result is valid or *nil* if it does not exist.              |
| Return 2nd |              | any type | Constructor of the reduction if it exists or *nil* if it is not defined by a scope covering the current scope step. |

#### Remark (*)
Built with [typedb:reduction_tagmask](#reduction_tagmask).


<a name="get_reductions"/>

### typedb:get_reductions
Get the list of reductions defined for a type from a list of selected classes defined by tag.
The scope step of the search that defines the valid reduction candidates has been set with the last call of the setter [typedb::step](#step) or [typedb::scope](#scope). 

#### Parameter
| #      | Name         | Type    | Description                                                                                                    |
| :----- | :----------- | :------ | :------------------------------------------------------------------------------------------------------------- |
| 1st    | type         | integer | Start type of the reductions to inspect.                                                                       |
| 2nd    | tagmask      | integer | (optional) Set (bit-set) of tags (*) that selects the reduction classes to consider (select all if undefined). |
| 3rd    | tagmask      | integer | (optional) Set (bit-set) of tags (*) that trigger a flag 'count' to be set in a result if it matches.          |
| Return |              | table   | List of type/constructor/weight/count as tables with named members.                                            | 

#### Remark (*)
Built with [typedb:reduction_tagmask](#reduction_tagmask).


<a name="deriveAndResolveTypes"/>

## Derive and Resolve Types

<a name="derive_type"/>

### typedb:derive_type
Finds the path of reductions selected by the _tagmask_ parameter with the minimum cost (sum of reduction weights). Throws an error if the results are ambiguous.
The scope step of the search that defines the valid reduction candidates has been set with the last call of the setter [typedb::step](#step) or [typedb::scope](#scope). 

#### Parameter
| #          | Name            | Type              | Description                                                                                                      |
| :--------- | :-------------- | :------- | :------------------------------------------------------------------------------------------------------------------------ |
| 1st        | dest-type       | integer  | Resulting type to derive.                                                                                                 |
| 2nd        | src-type        | integer  | Start type of the reduction path leading to the result type.                                                              |
| 3rd        | tagmask         | integer  | (optional) Set (bit-set) of tags (*) that selects the reductions to use (select all if undefined).                        |
| 3rd        | tagmask_pathlen | integer  | (optional) Set (bit-set) of tags (*) that selects the reductions contributing to the path length count of a result. (**)  |
| 4th        | max_pathlen     | integer  | (optional) maximum length count (number of reductions selected by tagmask_pathlen) of a path accepted as a result. (***)  |
| Return 1st |                 | table    | List of type/constructor pairs as tables with named members or *nil* if no result path found.                             |
| Return 2nd |                 | number   | Weight sum of best path found                                                                                             |
| Return 3rd |                 | table    | Alternative path with same weight found. There is an ambiguus reference if this value is not *nil*.                       |

#### Remark (*)
Built with [typedb:reduction_tagmask](#reduction_tagmask).
#### Remark (**)
Select none if undefined
#### Remark (***)
The tagmask_pathlen, max_pathlen parameters allow to define restrictions on the number of reductions that are implicit value conversions. 
Most programming languages do have such restrictions to avoid surprises in the parameter matching of functions. 
Usually the max_pathlen value is 1 if implicit type conversions are allowed at all. The default if not specified is also 1.


<a name="resolve_type"/>

### typedb:resolve_type
Finds the matching type with the searched name and a context-type derivable from the searched context-type, that has the shortest path (sum of reduction weights) of reductions of the classes selected by the _tagmask_ parameter. 
The returned list of candidates (2nd return value) has to be inspected by the client to find the best match.
The scope step of the search that defines the valid reduction candidates has been set with the last call of the setter [typedb::step](#step) or [typedb::scope](#scope). 

#### Parameter
| #          | Name            | Type              | Description                                                                                                        |
| :--------- | :-------------- | :---------------- | :----------------------------------------------------------------------------------------------------------------- |
| 1st        | context-type(s) | integer/table     | Single type or array of types or type/constructor pairs referring to the context of the type (*)                   |
| 2nd        | name            | string            | Name of the type searched                                                                                          |
| 3rd        | tagmask         | integer           | (optional) Set (bit-set) of tags (**) selecting the reduction classes used. No used reductions if not specified.   |
| Return 1st |                 | integer           | Derived context-type of the result, *nil* if not found, array with two types in case of ambiguous results.         |
| Return 2nd |                 | table             | List of context-type reductions, type/constructor pairs as tables with named members.                              |
| Return 3rd |                 | table             | List of candidates types found, differing in the parameters. The list has to be filtered by the caller (***).      |

#### Remark (*)
The context-type 0 is reserved for types that are not a member of some other structure. 
In case of a type/constructor pair selected as root type of the resolved type, this pair is returned as first element if the list of reductions (2nd return value).
#### Remark (**)
Built with [typedb:reduction_tagmask](#reduction_tagmask).
#### Remark (***)
The reason why parameter matching of the results is left to the caller is because there are different ways how to do this depending on the language. 
In fact, how parameters are matched is a core characteristic of the language and should therefore be implemented in the lua part of the type system.

#### Note
To inspect the result you first have to look if the 1st return value is *nil* (meaning that the type could not be resolved).
Then you have to check if the first return value is a table (meaning that there are ambiguous results). The table contains two of the conflicting context types.
Otherwise the first return value is the context-type of the resuls types and the 2nd and 3rd return value specify how the resolved types are built.


<a name="typeAttributes"/>

## Inspect Type Attributes

<a name="type_name"/>

### typedb:type_name
Get the name of the type as it was specified as name argument (second) of 'typedb:def_type'.

#### Parameter
| #          | Name   | Type              | Description                      |
| :--------- | :----- | :---------------- | :------------------------------- |
| 1st        | type   | integer           | Type identifier                  |
| Return     |        | string            | Name of the type without context |

<a name="type_context"/>

### typedb:type_context
Get the type handle of the type as specified as context-type argument (first) of 'typedb:def_type'.

#### Parameter
| #          | Name   | Type              | Description                                                         |
| :--------- | :----- | :---------------- | :------------------------------------------------------------------ |
| 1st        | type   | integer           | Type identifier                                                     |
| Return     |        | string            | Handle of the context type of this type or 0 for the global context |


<a name="type_string"/>

### typedb:type_string
Get the full signature of the type as string. This is the full name of the context type, the name and the full name of all parameters defined.

#### Parameter
| #          | Name | Type              | Description                                                                                    |
| :--------- | :--- | :---------------- | :--------------------------------------------------------------------------------------------- |
| 1st        | type | integer           | Type identifier                                                                                |
| 2nd        | type | string            | (optional) Separator for multipart type name, default is space ' '                             |
| Return     |      | string            | Full name of the type with context separated by spaces and parameters in oval brackets '(' ')' |


<a name="type_parameters"/>

### typedb:type_parameters
Get the list of parameters defined for a type.

#### Parameter
| #          | Name | Type     | Description                                                                          |
| :--------- | :--- | :------- | :----------------------------------------------------------------------------------- |
| 1st        | type | integer  | Type identifier                                                                      |
| Return     |      | table    | List of type/constructor pairs as tables with named members.                         |


<a name="type_nof_parameters"/>

### typedb:type_nof_parameters
Get the number of parameters defined for a type.

#### Parameter
| #          | Name | Type     | Description                                     |
| :--------- | :--- | :------- | :---------------------------------------------- |
| 1st        | type | integer  | Type identifier                                 |
| Return     |      | integer  | Number of parameters defined for this function. |


<a name="type_constructor"/>

### typedb:type_constructor
Get the constructor of a type.

#### Parameter
| #          | Name | Type              | Description                                    |
| :--------- | :--- | :---------------- | :--------------------------------------------- |
| 1st        | type | integer           | Type identifier                                |
| Return     |      | any type          | Constructor of the type or *nil* if undefined. |

#### Remark
Exits with error if the type passed is not valid. Returns *nil* if type is 0 or if not constructor is defined for that type.


<a name="type_scope"/>

### typedb:type_scope
Get the scope where a type has been defined.

#### Parameter
| #          | Name | Type              | Description                                                            |
| :--------- | :--- | :---------------- | :--------------------------------------------------------------------- |
| 1st        | type | integer           | Type identifier to query the scope                                     |
| Return     |      | integer           | the scope where the type has been defined (global scope for typeid 0). |

#### Remark
Exits with error if the type passed is not valid.
#### Note
Used to enforce some stricter rules for the visibility of certain objects, e.g. restrict access of variables in locally declared functions to a declared capture, the own stack frame and global variables. 
Also used for binding implicitly defined types to the scope of the type they are bound to.


<a name="introspection"/>

## Instrospection for Debugging:

<a name="instance_tree"/>

### typedb:instance_tree
Get the tree of scopes with the instances defined. 

#### Parameter
| #      | Name     | Type      | Description                                           |
| :----- | :------- | :-------- | :---------------------------------------------------- |
| 1st    | name     | string    | Name of the object we are addressing                  |
| Return |          | table     | Tree structure with a node for each definition scope  |

#### Remark
This is a costly operation and not intended to be used as data structure for the compiler itself. 
It is thought as help during development.

#### Fields of the Returned Tree Structure
| Name     | Type      | Description                                                                             |
| :------- | :-------- | :-------------------------------------------------------------------------------------- |
| chld     | function  | Iterator function to iterate on the child nodes (also tree structures like this node).  |
| scope    | function  | Function returning an array (pair of integers) describing the scope of this node.       |
| instance | function  | Function returning the instance defined for this node.                                  |


<a name="type_tree"/>

### typedb:type_tree
Get the tree of scopes with the list of types defined there. 

#### Parameter
| #      | Name     | Type      | Description                                           |
| :----- | :------- | :-------- | :---------------------------------------------------- |
| Return |          | table     | Tree structure with a node for each definition scope  |

#### Fields of the Returned Tree Structure
| Name     | Type      | Description                                                                             |
| :------- | :-------- | :-------------------------------------------------------------------------------------- |
| chld     | function  | Iterator function to iterate on the child nodes (also tree structures like this node)   |
| scope    | function  | Function returning an array (pair of integers) describing the scope of this node.       |
| list     | function  | Function returning the list of types (integers) defined in the scope of this node       |

#### Remark
This is a costly operation and not intended to be used as data structure for the compiler itself. 
It is thought as help during development.


<a name="reduction_tree"/>

### typedb:reduction_tree
Get the tree of scopes with the list of reductions defined there. 

#### Parameter
| #      | Name     | Type      | Description                                           |
| :----- | :------- | :-------- | :---------------------------------------------------- |
| Return |          | table     | Tree structure with a node for each definition scope  |

#### Fields of the Returned Tree Structure
| Name     | Type      | Description                                                                               |
| :------- | :-------- | :---------------------------------------------------------------------------------------- |
| chld     | function  | Iterator function to iterate on the child nodes (also tree structures like this node)     |
| scope    | function  | Function returning an array (pair of integers) describing the scope of this node          |
| list     | function  | Function returning the list of reductions (structures) defined in the scope of this node  |

#### Members of the Tree Structure List Elements
| Name          | Type             | Description                                                     |
| :------------ | :--------------- | :-------------------------------------------------------------- |
| to            | integer          | Destination type of the reduction                               |
| from          | integer          | Origin type of the reduction                                    |
| constructor   | any type not nil | Constructor describing how the type reduction is implemented.   |
| tag           | integer 1..32    | Tag assigned to the reduction.                                  |
| weight        | number           | Weight assigned to the reduction.                               |

#### Remark
This is a costly operation and not intended to be used as data structure for the compiler itself. 
It is thought as help during development.




