# The Mewa Lua Module API
The Lua module provided by _Mewa_ has two parts:
* The compiler that is called from the script generated from the grammar description by the _mewa_ program.
* The type database that is created and used by the typesystem Lua script with the callback hooks called on actions from the compiler.

## The Compiler API
### Create a Compiler
```Lua
mewa = require("mewa")
compiler = mewa.compiler( compilerdef)

```
```compilerdev``` is the structure created from the grammar definition by the _mewa_ program.

### Run the Compiler
The compiler has one method ```run``` that takes 3 arguments from which two are optional:

```Lua
compiler:run( inputfile, outputfile, dbgout)

```
```inputfile``` specifies the path to the file to compile.
```outputfile``` specifies the path to the file to write the compiler output to. If not specified the output is written to stdandard output.
```dbgout``` specifies the path to the file to write the compiler debug output to. The debug output logs the actions of the compiler and may be helpful during the early stages of development.


## The Type Database API
The type database API provides some functions to build the type system of your compiler. 
A documentation can be found [here](doc/typedb.md).



