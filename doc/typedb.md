# The mewa.typedb API

The type database API referred to as _mewa.typedb_ offers you the functions needed to build up the typesystem of your language. 

## Create a new type database
```lua
mewa = require("mewa")
typedb = mewa.typedb()
```
The variable typedb holds now the type database created.

## User defined objects in different scopes:
### typedb:set_instance
#### Parameter
| Name     | Type              | Description                                           |
| -------- | ----------------- | ----------------------------------------------------- |
| name     | string            | Name of the object we declare an instance of          |
| -------- | ----------------- | ----------------------------------------------------- |
| scope    | pair of integers  | The scope if the instance                             |
| -------- | ----------------- | ----------------------------------------------------- |
| instance | anything not nil  | The instance of the object                            |
| -------- | ----------------- | ----------------------------------------------------- |

#### Description
Defines the instance for the object with name _name_ to be _object_ for the scope _scope_.

### typedb:get_instance
#### Parameter
| Name | Type     | Description                                           |
| ---- | -------- | ----------------------------------------------------- |
| name | string   | Name of the object we declare an instance of          |
| ---- | -------- | ----------------------------------------------------- |
| step | integer  | Scope step to locate the instance we are referring to |
| ---- | -------- | ----------------------------------------------------- |
#### Description
Returns the instance for the object with name _name_ for the innermost scope including the scope step _step_.

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



