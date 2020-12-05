# Mewa API

The _Mewa_ module imported with ```require "mewa"``` has the following functions. 

## Table of Contents
1. [mewa.compiler ~ Create a Compiler Object](#compiler)
1. [mewa.typedb ~ Create a Type Database Object](#typedb)
1. [mewa.tostring ~ Serialization for Debugging Purposes](#tostring)
1. [mewa.stacktrace ~ An Alternative to debug.traceback](#stacktrace)


<a name="compiler"/>

## Create a compiler object

```Lua
mewa = require("mewa")
compiler = mewa.compiler( compilerdef)

```
```compilerdef``` is the structure created from the grammar definition by the _mewa_ program.

### Run the Compiler
The compiler has one method ```run``` that takes 3 arguments from which two are optional:

```Lua
compiler:run( targetfile, inputfile, outputfile, dbgout)

```
```targetfile``` specifies the path to the file that is the template used for a specific target where {Code} and {Source} are substituted by the output printed and the name of the source file.
```inputfile``` specifies the path to the file to compile.
```outputfile``` specifies the path to the file to write the compiler output to. If not specified the output is written to stdandard output.
```dbgout``` specifies the path to the file to write the compiler debug output to. The debug output logs the actions of the compiler and may be helpful during the early stages of development.

### Note
The compiler that is called from the script generated from the grammar description by the _mewa_ program.


<a name="typedb"/>

## Create a Type Database Object
```Lua
mewa = require("mewa")
typedb = mewa.typedb()

```

## The Type Database API
A type database object provides some functions to build the type system of your compiler. 
A documentation can be found [here](typedb.md).


<a name="tostring"/>

## Serialization for debugging purposes

### mewa.tostring

#### Parameter
| #      | Name     | Type              | Description                                                             |
| :----- | :------- | :---------------- | :---------------------------------------------------------------------- |
| 1st    | obj      | any type          | object to dump the contents from                                        |
| 2nd    | indent   | boolean           | (optional) true if to use indentiation, false if not, Default is false. |
| Return |          | string            | The serialized contents of the object passed as string                  |

#### Remark
The serialization is not complete. Serialization is done only for raw tables, numbers and strings. It just does the job suitable for _Mewa_. 


<a name="stacktrace"/>

## An Alternative to debug.traceback

### mewa.stacktrace

#### Parameter
| #      | Name     | Type              | Description                                                               |
| :----- | :------- | :---------------- | :------------------------------------------------------------------------ |
| 1st    | depth    | cardinal          | (optional) number of steps to trace back. Default is no limit.            |
| 2nd    | ignore   | array of strings  | (optional) List of substrings of functions ignored (*), Default is empty. |
| Return |          | table             | Array of stack trace elements                                             |

#### Stack Trace Element Structure
A stack trace element of the list returned is a table containing the following elements:

| Name          | Type             | Description                                                     |
| :------------ | :--------------- | :-------------------------------------------------------------- |
| line          | integer          | Line number of the function header in the Lua script source     |
| file          | string           | Path of the Lua script                                          |
| function      | string           | Name of the function                                            |

#### Remark (*)
If the ignore list contains for example the elements ```{"traverse", "error"}``` then all stack trace elements referring to functions with names either containing a substring "traverse" or are containing a substring "error" are ignored and not part of the result. Remind that substrings are not regular expression matches but simple substring search matches.

#### Remark
The stack trace function has been written for compilers written with _Mewa_. It has its limitations that makes it not suitable for other purposes.


