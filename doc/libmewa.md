# Mewa API

The _Mewa_ module imported with ```require "mewa"``` has the following functions. 

## Table of Contents
1. [mewa.compiler](#compiler)  ~ Create a Compiler Object
1. [mewa.typedb](#typedb)  ~ Create a Type Database Object
1. [mewa.tostring](#tostring)  ~ Serialization for Debugging Purposes
1. [mewa.stacktrace](#stacktrace) ~ An Alternative to debug.traceback
1. [mewa.llvm_float_tohex](#llvm_float_tohex)  ~ Display of 32bit floating point numbers for output to LLVM IR
1. [mewa.llvm_double_tohex](#llvm_double_tohex)  ~ Display of 64bit floating point numbers (double) for output to LLVM IR


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
If the ignore list contains for example the elements ```{"traverse", "error"}``` then all stack trace elements referring to functions with names either containing a substring "traverse" or are containing a substring "error" are ignored and not part of the result. Remind that substrings are not regular expression matches but simple contains substring matches.

#### Remark
The stack trace function has been written for compilers written with _Mewa_. It has its limitations that makes it not suitable for other purposes.


<a name="llvm_float_tohex"/>

## Display of 32bit floating point numbers (float) for output to LLVM IR

This is a helper function to convert a floating point number to its hexadecimal representation accepted in all cases by LLVM.
The following code leads to an error:
```LLVM
	store float 4.21, float* %r1
```
while
```LLVM
	store float 4.25, float* %r1
```
is accepted. The representation as hexadecimal code is allways accepted by LLVM:
```LLVM
	store float 0x4010D70A40000000, float* %r1
```
To get the hexadecimal code for a 32 bit floating point number you have to call:
```Lua
	hexcode = "0x" .. llvm_float_tohex( 4.21)
```
and you get
```
	hexcode = "0x4010D70A40000000"
```

#### Note
This function does not belong here but should be part of a separate lua library for handling LLVM IR issues. IMHO: It should be possible in LLVM IR to configure how floating point numbers are rounded and as consequence it should be possible to use floating point constants in the LLVM IR output.


<a name="llvm_double_tohex"/>

## Display of 64bit floating point numbers (double) for output to LLVM IR

This does the same as [mewa.llvm_float_tohex](#llvm_float_tohex), but returns the representation of a 64 bit double precision floating point number.
```Lua
	hexcode = "0x" .. llvm_double_tohex( 4.212121212121)
```
and you get
```
	hexcode = "0x4010D9364D9363EA"
```

#### Note
This function does not belong here but should be part of a separate lua library for handling LLVM IR issues. IMHO: It should be possible in LLVM IR to configure how floating point numbers are rounded and as consequence it should be possible to use floating point constants in the LLVM IR output.






