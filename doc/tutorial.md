# Writing Compiler Front-Ends for LLVM with _Lua_ using _Mewa_

## Lead Text
_LLVM IR_ text representation makes it possible to write a compiler front-end without being bound to an API. We can map the source text to the text of _LLVM IR_ and use the tools of the _Clang_ compiler for further compilation steps. This opens the doors to implement compiler front-ends in different ways. _Mewa_ tries to optimize the amount of code written. It targets single authors that would like to write a prototype of a non-trivial programming language fast, but at the expense of a structure supporting collaborative work. This makes it rather a tool for experiment than for building a product.

## Introduction

The _Mewa_ compiler-compiler is a tool to script compiler front-ends in _Lua_. A compiler written with _Mewa_ takes a source file as input and prints an intermediate representation as input for the next compilation step. The example in this article uses the text format of _LLVM IR_ as output. An executable is built using the ```llc``` command of _Clang_.

_Mewa_ provides no support for the evaluation of different paths of code generation. The idea is to do a one-to-one mapping of program structures to code and to leave all analytical optimization steps to the backend.

For implementing a compiler with _Mewa_, you define a grammar attributed with _Lua_ function calls.
The program ```mewa``` will generate a _Lua_ script that will transform any source passing the grammar specified into an _AST_ with the _Lua_ function calls attached to the nodes. The compiler will call the functions of the top-level nodes of the _AST_ that take the control of the further processing of the compiler front-end. The functions called with their _AST_ node as argument invoke the further tree traversal. The types and data structures of the program are built and the output is printed in the process.

## Goals

We start with a tutorial of the **typedb** library of _Mewa_ for _Lua_ with some self-contained examples. Self-contained means that nothing is used but the library. The examples are also not dependent on each other. This allows you to continue reading even if you did not understand it completely and to return later.

After this, this article will guide you through the implementation of a compiler for a simple programming language missing many features.
The example program we will compile is the following:
```
extern function printf void( string, double);
// ... printf is a variable argument list function, but this would go beyond the scope of this tutorial

class Employee
{
	string name;
	int age;
	double salary;

	function setSalary void( double salary_)
	{
		salary = salary_;
	}
}

function salarySum double( Employee[10] list)
{
	var int idx = 0;
	var double sum = 0.0;
	while (list[idx].age)
	{
		sum = sum + list[idx].salary;
		idx = idx + 1;
	}
	return sum;
}

function salaryRaise void( Employee[10] list, double factor)
{
	var int idx = 0;
	while (list[idx].age)
	{
		list[idx].setSalary( list[idx].salary * factor);
		idx = idx + 1;
	}
}

main
{
	var Employee[ 10] list = {
		{"John Doe", 36, 60000},
		{"Suzy Sample", 36, 63400},
		{"Doe Joe", 36, 64400},
		{"Sandra Last", 36, 67400}
	};
	salaryRaise( list, 1.10); // Give them all a 10% raise
	printf( "Salary sum: %.2f\n", salarySum( list)); // Expected result = 280720
}


```
We will compile this program with the our example compiler and run it in a shell. We will also look at different parts including the grammar of the language.
Finally, I will talk about the features missing in the language to give you some outlook on how to implement a compiler of a complete programming language.

## Target Audience

To understand this article some knowledge about formal languages and parser generators is helpful. To understand the examples you should be familiar with some scripting languages that have a concept of closures similar to _Lua_. If you have read an introduction to [LLVM IR](links.md), you will get grip on the code generation in the examples.

## Deeper Digging

For a deeper digging you have to look at the _Mewa_ project itself and the implementation of [the main example language](example_language1.md), a strongly typed multiparadigm programming language with structures, classes, interfaces, free functions, generics, lambdas and exceptions. There exists an [FAQ](faq.md) that also tries to answer problem-solving questions.

## Preparation

The _Mewa_ program and the _Lua_ module are written in C++. Follow the instructions to install it.
The installation description is written for _Ubuntu_. For other platforms you might need to lookup the corresponding package names.

### Installation

To install _Mewa_ you need a C++ compiler with C++17 support. _Mewa_ is tested with _clang_ and _gcc_. 

#### Required system packages
##### For Lua 5.2
```Bash
lua5.2 liblua5.2-dev luarocks llvm llvm-runtime
```
##### For Lua 5.1
```Bash
lua5.1 liblua5.1-0-dev luarocks llvm llvm-runtime
```
#### Required luarocks packages
```Bash
luarocks install bit32
luarocks install penlight 
luarocks install LuaBcd
```
#### Build LuaBcd from sources (if luarocks install LuaBcd fails)
If the build of _LuaBcd_ with _luarocks_ fails, you can fetch the sources from _github_ and build it:
```Bash
git clone https://github.com/patrickfrey/luabcd.git
cd LuaBcd
./configure
make
make install
```
#### Fetch sources of latest release version
```Bash
git clone https://github.com/patrickfrey/mewa
cd mewa
git checkout -b `cat VERSION`
```
#### Configure to find Lua includes and to write the file Lua.inc included by make
```Bash
./configure
```
#### Build with GNU C/C++
```Bash
make COMPILER=gcc RELEASE=YES
```
#### Build with Clang C/C++
```Bash
make COMPILER=clang RELEASE=YES
```
#### Run tests
```Bash
make test
```
#### Install
```Bash
make install
```
#### Lua Environment
Don't forget to set the _Lua_ environment (LUA_CPATH,LUA_PATH) correctly when running the _Lua_ scripts from command line. 
You find a luaenv.sh in the archive of this article. Load it with.
```Bash
. tutorial/luaenv.sh
```


## Tutorial

In this tutorial we will learn how to use the typedb library with some self-contained examples.

### Declaring a variable

Let's start with a complicated example that is a substantial step forward. 
We print the LLVM code needed to assign a variable value to another variable. 
For this, we need to introduce **types** and **reductions**.

#### Introduction
##### Types
Types are items represented as integers. They are declared and retrieved by the name of the type and a context type.
The context type is itself a type or 0.
Global free variables have for example no associated context type and are declared with 0 as context type. 
Types are associated with a constructor. A constructor is a value, structure, or a function that describes the construction of the type.
Optionally, types can have parameters attached. Parameters are a list of type/constructor pairs. Types are declared with 
```Lua
typeid = typedb:def_type( contextType, name, constructor, parameters)
```
The ```typeid``` returned is the integer that represents the type for the _typedb_.
##### Reductions
Reductions are paths to derive a type from another. You can imagine the typesystem as a directed graph of vertices (types) and edges (reductions).
We will introduce some concepts that allow a partial view of this graph later. For now, imagine it as a graph.
Reductions have also an associated constructor. The constructor describes the construction of the type in the direction of the reduction from its source.
Here is an example:
```Lua
typedb:def_reduction( destType, srcType, constructor, 1)
```
The 1 as parameter is an integer value we will explain later.
##### Resolve Type
Types can be resolved by their name and a single or a list of context types, one having a path of reductions to the context type of the resolved type.
##### Derive type
Types can be constructed by querying a reduction path from one type to another and constructing the type from the source type constructor
by applying the list of constructors on this path. 

Let's have a look at the example:

#### Source
File examples/variable.lua
```Lua
mewa = require "mewa"
typedb = mewa.typedb()

-- [1] Define some helper functions
-- Map template for LLVM IR Code synthesis
--   substitute identifiers in curly brackets '{' '}' with the values in a table
function constructor_format( fmt, argtable)
	return (fmt:gsub("[{]([_%d%w]*)[}]", function( match) return argtable[ match] or "" end))
end

-- A register allocator as a generator function counting from 1, returning the LLVM register identifiers:
function register_allocator()
	local i = 0
	return function ()
		i = i + 1
		return "%R" .. i
	end
end

-- Build a constructor by applying a constructor function on some arguments
--   or returning the first argument in case of a constructor function as nil
function applyConstructor( constructor, this, arg)
	if constructor then
		if (type(constructor) == "function") then return constructor( this, arg) else return constructor end
	else
		return this
	end
end

-- [2] Define an integer type with assignment operator
do
local register = register_allocator()
local intValType = typedb:def_type( 0, "int")	-- integer value type
local intRefType = typedb:def_type( 0, "int&")	-- integer reference type
typedb:def_reduction( intValType, intRefType,	-- get the value type from its reference type
	function(this)
		local out = register()
		local code = constructor_format( "{out} = load i32, i32* {this} ; reduction int <- int&\n", {out=out,this=this.out})
		return {out=out, code=code}
	end, 1)
typedb:def_type( intRefType, "=",		-- assignment operator
	function( this, arg)
		local code = (this.code or "") .. (arg[1].code or "")
			.. constructor_format( "store i32 {arg1}, i32* {this} ; assignment int& = int\n", {this=this.out,arg1=arg[1].out})
		return {code=code, out=this.out}
	end, {intValType})

-- [3] Define a variable "a" initialized with 1:
local register_a = register()
local variable_a = typedb:def_type( 0, "a", {out=register_a})
typedb:def_reduction( intRefType, variable_a, nil, 1)
io.stdout:write( "; SOURCE int a = 1;\n")
io.stdout:write( constructor_format( "{out} = alloca i32, align 4 ; allocation of 'a'\n", {out=register_a}))
io.stdout:write( constructor_format( "store i32 {arg1}, i32* {this} ; assignment int& = 1 \n", {this=register_a,arg1=1}))

-- [4] Define a variable b and assign the value of a to it:
io.stdout:write( "; SOURCE int b = a;\n")

-- [4.1] Define a variable "b":
local register_b = register()
local variable_b = typedb:def_type( 0, "b", {out=register_b})
typedb:def_reduction( intRefType, variable_b, nil, 1)

io.stdout:write( constructor_format( "{out} = alloca i32, align 4 ; allocation of 'b'\n", {out=register_b}))

-- [4.2] Assign the value to "a" to "b":
-- [4.2.1] Resolve the operator "b = .."
local resolveTypeId, reductions, items = typedb:resolve_type( variable_b, "=")
if not resolveTypeId then error( "Not found") elseif type( resolveTypeId) == "table" then error( "Ambiguous") end

-- [4.2.2] For all candidates of "b = ..", get the first one with one parameter and match the argument to this parameter
local constructor = typedb:type_constructor( variable_b)
for _,redu in ipairs(reductions or {}) do constructor = applyConstructor( redu.constructor, constructor) end
for _,item in ipairs(items) do
	local parameters = typedb:type_parameters( item)
	if #parameters == 1 then
		local reductions,weight,altpath = typedb:derive_type( parameters[1].type, variable_a)
		if altpath then error( "Ambiguous parameter") end
		if weight then
			-- [5.3] Synthesize the code for "b = a;"
			local parameter_constructor = typedb:type_constructor( variable_a)
			for _,redu in ipairs(reductions or {}) do
				parameter_constructor = applyConstructor( redu.constructor, parameter_constructor)
			end
			constructor = typedb:type_constructor(item)( constructor, {parameter_constructor})
			break
		end
	end
end
-- [4.3] Print the code of "b = a;"
io.stdout:write( constructor.code)

end


```
#### Output
```
; SOURCE int a = 1;
%R1 = alloca i32, align 4 ; allocation of 'a'
store i32 1, i32* %R1 ; assignment int& = 1 
; SOURCE int b = a;
%R2 = alloca i32, align 4 ; allocation of 'b'
%R3 = load i32, i32* %R1 ; reduction int <- int&
store i32 %R3, i32* %R2 ; assignment int& = int

```
#### Conclusion
If you got here you got already quite far. We saw the application of an operator ('=') with an argument. 
Applying a function with more than one argument is imaginable. 
The first match of the operator was our candidate match chosen as a result. But selecting a match by other criteria is imaginable.
We declared a variable with a name, a concept of scope is missing here. We will look at scopes in the next example.


### Scope

Now let's see how scope is represented in the _typedb_.

#### Introduction
Scope in _Mewa_ is represented as a pair of non-negative integer numbers. The first number is the start, the first scope step that belongs to the scope, the second number the end, the first scope step that is not part of the scope. Scope steps are generated by a counter with increments declared in the grammar of your language parser.
All declarations in the _typedb_ are bound to a scope, all queries of the _typedb_ are bound to the current scope step. A declaration is considered to contribute to the result if its scope is covering the scope step of the query.

##### Set the current scope
```Lua
scope_old = typedb:scope( scope_new )
```
##### Get the current scope
```Lua
current_scope = typedb:scope()
```

The example is fairly artificial, but it shows how it works:

#### Source
File examples/scope.lua
```Lua
mewa = require "mewa"
typedb = mewa.typedb()

function defineVariable( name)
	typedb:def_type( 0, name, string.format( "instance of scope %s", mewa.tostring( (typedb:scope()))))
end
function findVariable( name)
	local res,reductions,items = typedb:resolve_type( 0, name)
	if not res then return "!NOTFOUND" end
	if type(res) == "table" then return "!AMBIGUOUS" end
	for _,item in ipairs(items) do if typedb:type_nof_parameters(item) == 0 then return typedb:type_constructor(item) end end
	return "!NO MATCH"
end

typedb:scope( {1,100} )
defineVariable( "X")
typedb:scope( {10,50} )
defineVariable( "X")
typedb:scope( {20,30} )
defineVariable( "X")
typedb:scope( {70,90} )
defineVariable( "X")

typedb:step( 25)
print( "Retrieve X " .. findVariable("X"))
typedb:step( 45)
print( "Retrieve X " .. findVariable("X"))
typedb:step( 95)
print( "Retrieve X " .. findVariable("X"))
typedb:step( 100)
print( "Retrieve X " .. findVariable("X"))


```
#### Output
```
Retrieve X instance of scope {20,30}
Retrieve X instance of scope {10,50}
Retrieve X instance of scope {1,100}
Retrieve X !NOTFOUND

```
#### Conclusion
This was easy, wasn't it? What is missing now is how the current scope step and scope are set. I chose the variant of treating it as an own aspect. 
The function used for the _AST_ tree traversal sets the current scope step and scope. This works for most cases. Sometimes you have to set the scope manually in nodes that implement declarations of different scopes, like for example function declarations with a function body in an own scope.


### Reduction Weight

Now we have to look again at something a little bit puzzling. We have to assign a weight to reductions. The problem is that even trivial examples of type queries lead to ambiguity if we do not introduce a weighting scheme that memorizes a preference. The ```typedb:resolve_type``` and ```typedb:derive_type``` functions are implemented as shortest path search that selects the result and implements this preference. Real ambiguity is something we want to detect as an error. An ambiguity is indicated when two or more results are appearing having the same weight sum.
I concluded that it is best to declare all reduction weights at one central place in the source and to document it well.

Let's have a look at an example without weighting of reductions that will lead to ambiguity. 

#### Failing example
File examples/weight1.lua
```Lua
mewa = require "mewa"
typedb = mewa.typedb()

-- Build a constructor
function applyConstructor( constructor, this, arg)
	if constructor then
		if (type(constructor) == "function") then return constructor( this, arg) else return constructor end
	else
		return this
	end
end

-- Type list as string
function typeListString( typeList, separator)
	if not typeList or #typeList == 0 then return "" end
	local rt = typedb:type_string( type(typeList[ 1]) == "table" and typeList[ 1].type or typeList[ 1])
	for ii=2,#typeList do
		rt = rt .. separator .. typedb:type_string( type(typeList[ ii]) == "table" and typeList[ ii].type or typeList[ ii])
	end
	return rt
end

-- Define a type with all its variations having different qualifiers
function defineType( name)
	local valTypeId = typedb:def_type( 0, name)
	local refTypeId = typedb:def_type( 0, name .. "&")
	local c_valTypeId = typedb:def_type( 0, "const " .. name)
	local c_refTypeId = typedb:def_type( 0, "const " .. name .. "&")
	typedb:def_reduction( valTypeId, refTypeId, function( arg) return "load ( " .. arg .. ")"  end, 1)
	typedb:def_reduction( c_valTypeId, c_refTypeId, function( arg) return "load ( " .. arg .. ")"  end, 1)
	typedb:def_reduction( c_valTypeId, valTypeId, function( arg) return "make const ( " .. arg .. ")"  end, 1)
	typedb:def_reduction( c_refTypeId, refTypeId, function( arg) return "make const ( " .. arg .. ")"  end, 1)
	return {val=valTypeId, ref=refTypeId, c_val=c_valTypeId, c_ref=c_refTypeId}
end

local qualitype_int = defineType( "int")
local qualitype_long = defineType( "long")

typedb:def_reduction( qualitype_int.val, qualitype_long.val, function( arg) return "convert int ( " .. arg .. ")"  end, 1)
typedb:def_reduction( qualitype_int.c_val, qualitype_long.c_val, function( arg) return "convert const int ( " .. arg .. ")"  end, 1)
typedb:def_reduction( qualitype_long.val, qualitype_int.val, function( arg) return "convert long ( " .. arg .. ")"  end, 1)
typedb:def_reduction( qualitype_long.c_val, qualitype_int.c_val, function( arg) return "convert const long ( " .. arg .. ")" end, 1)

local reductions,weight,altpath = typedb:derive_type( qualitype_int.c_val, qualitype_long.ref)
if not weight then
	print( "Not found!")
elseif altpath then
	print( "Ambiguous: " .. typeListString( reductions, " -> ") .. " | " .. typeListString( altpath, " -> "))
else
	local constructor = typedb:type_name( qualitype_long.ref)
	for _,redu in ipairs(reductions or {}) do constructor = applyConstructor( redu.constructor, constructor) end

	print( constructor)
end

```
#### Output
```
Ambiguous: long -> const long -> const int | long -> int -> const int

```
#### Adding weights
File examples/weight2.lua

Here is a diff with the edits we have to make for fixing the problem:
```
3a4,12
> -- Centralized list of the ordering of the reduction rules determined by their weights:
> local weight_conv=	1.0		-- weight of a conversion
> local weight_const_1=	0.5 / (1*1)	-- make const of a type having one qualifier
> local weight_const_2=	0.5 / (2*2)	-- make const of a type having two qualifiers
> local weight_const_3=	0.5 / (3*3)	-- make const of a type having three qualifiers
> local weight_ref_1=	0.25 / (1*1)	-- strip reference of a type having one qualifier
> local weight_ref_2=	0.25 / (2*2)	-- strip reference of a type having two qualifiers
> local weight_ref_3=	0.25 / (3*3)	-- strip reference of a type having three qualifiers
> 
29,32c38,41
< 	typedb:def_reduction( valTypeId, refTypeId, function( arg) return "load ( " .. arg .. ")"  end, 1)
< 	typedb:def_reduction( c_valTypeId, c_refTypeId, function( arg) return "load ( " .. arg .. ")"  end, 1)
< 	typedb:def_reduction( c_valTypeId, valTypeId, function( arg) return "make const ( " .. arg .. ")"  end, 1)
< 	typedb:def_reduction( c_refTypeId, refTypeId, function( arg) return "make const ( " .. arg .. ")"  end, 1)
---
> 	typedb:def_reduction( valTypeId, refTypeId, function( arg) return "load ( " .. arg .. ")"  end, 1, weight_ref_1)
> 	typedb:def_reduction( c_valTypeId, c_refTypeId, function( arg) return "load ( " .. arg .. ")"  end, 1, weight_ref_2)
> 	typedb:def_reduction( c_valTypeId, valTypeId, function( arg) return "make const ( " .. arg .. ")"  end, 1, weight_const_1)
> 	typedb:def_reduction( c_refTypeId, refTypeId, function( arg) return "make const ( " .. arg .. ")"  end, 1, weight_const_2)
39,42c48,51
< typedb:def_reduction( qualitype_int.val, qualitype_long.val, function( arg) return "convert int ( " .. arg .. ")"  end, 1)
< typedb:def_reduction( qualitype_int.c_val, qualitype_long.c_val, function( arg) return "convert const int ( " .. arg .. ")"  end, 1)
< typedb:def_reduction( qualitype_long.val, qualitype_int.val, function( arg) return "convert long ( " .. arg .. ")"  end, 1)
< typedb:def_reduction( qualitype_long.c_val, qualitype_int.c_val, function( arg) return "convert const long ( " .. arg .. ")" end, 1)
---
> typedb:def_reduction( qualitype_int.val, qualitype_long.val, function( arg) return "convert int ( " .. arg .. ")"  end, 1, weight_conv)
> typedb:def_reduction( qualitype_int.c_val, qualitype_long.c_val, function( arg) return "convert const int ( " .. arg .. ")"  end, 1, weight_conv)
> typedb:def_reduction( qualitype_long.val, qualitype_int.val, function( arg) return "convert to long ( " .. arg .. ")"  end, 1, weight_conv)
> typedb:def_reduction( qualitype_long.c_val, qualitype_int.c_val, function( arg) return "convert to const long ( " .. arg .. ")"  end, 1, weight_conv)

```
#### Output with weights
```
convert const int ( load ( make const ( long&)))

```
#### Conclusion
We introduced a 5th parameter of the ```typedb:def_reduction``` command that is 0 if not specified. 
This new parameter is meant to be declared in a way that it memorizes a preference of solution paths. 
The weights should be referenced by constants we declare at a central place where the trains of thought that led to the weighting schema are documented.


### Reduction Tag

The display of the typesystem as one graph is not enough for all cases. There are type derivation paths that are fitting in one case and undesirable in other cases. The following example declares an object of a class derived from a base class that calls a constructor function with no arguments. The constructor function is only declared for the base class. But when calling an object constructor an error should be reported if it does not exist for the class. The same behavior as for a method call is bad in this case.

Let's first look at an example failing:

#### Failing example
File examples/tags1.lua
```Lua
mewa = require "mewa"
typedb = mewa.typedb()

-- [1] Define some helper functions
-- Map template for LLVM Code synthesis
function constructor_format( fmt, argtable)
	return (fmt:gsub("[{]([_%d%w]*)[}]", function( match) return argtable[ match] or "" end))
end

-- A register allocator as a generator function counting from 1, returning the LLVM register identifiers:
function register_allocator()
	local i = 0
	return function ()
		i = i + 1
		return "%R" .. i
	end
end

-- Build a constructor
function applyConstructor( constructor, this, arg)
	if constructor then
		if (type(constructor) == "function") then return constructor( this, arg) else return constructor end
	else
		return this
	end
end

-- Define a type with variations having different qualifiers
function defineType( name)
	local valTypeId = typedb:def_type( 0, name)
	local refTypeId = typedb:def_type( 0, name .. "&")
	typedb:def_reduction( valTypeId, refTypeId,
		function(this)
			local out = register()
			local fmt = "{out} = load %{symbol}, %{symbol}* {this}\n"
			local code = constructor_format( fmt, {symbol=name,out=out,this=this.out})
			return {out=out, code=code}
		end, 1)
	return {val=valTypeId, ref=refTypeId}
end

local register = register_allocator()

-- Constructor for loading a member reference
function loadMemberConstructor( classname, index)
	return function( this)
		local out = register()
		local fmt = "{out} = getelementptr inbounds %{symbol}, %{symbol}* {this}, i32 0, i32 {index}\n"
		local code = (this.code or "")
			.. constructor_format( fmt, {out=out,this=this.out,symbol=classname,index=index})
		return {code=code, out=out}
	end
end
-- Constructor for calling the default ctor, assuming that it exists
function callDefaultCtorConstructor( classname)
	return function(this)
		local fmt = "call void @__ctor_init_{symbol}( %{symbol}* {this})\n"
		local code = (this.code or "") .. (arg[1].code or "")
			.. constructor_format( fmt, {this=this.out,symbol=classname})
		return {code=code, out=out}
	end
end

local qualitype_baseclass = defineType("baseclass")
local qualitype_class = defineType("class")

-- Define the default ctor of baseclass
local constructor_baseclass = typedb:def_type( qualitype_baseclass.ref, "constructor", callDefaultCtorConstructor("baseclass"))

-- Define the inheritance of class from baseclass
typedb:def_reduction( qualitype_baseclass.ref, qualitype_class.ref, loadMemberConstructor( "class", 1), 1)

-- Search for the constructor of class (that does not exist)
local resolveTypeId, reductions, items = typedb:resolve_type( qualitype_class.ref, "constructor")
if not resolveTypeId then print( "Not found") elseif type( resolveTypeId) == "table" then print( "Ambiguous") end

for _,item in ipairs(items or {}) do print( "Found " .. typedb:type_string( item) .. "\n") end

```
#### Output
```
Found baseclass& constructor


```
#### Adding tags
File examples/tags2.lua

Here is a diff with the edits we have to make for fixing the problem:
```
3a4,13
> -- [1] Define all tags and tag masks
> -- Tags attached to reduction definitions. When resolving a type or deriving a type, we select reductions by specifying a set of valid tags
> tag_typeDeclaration = 1		-- Type declaration relation (e.g. variable to data type)
> tag_typeDeduction   = 2		-- Type deduction (e.g. inheritance)
> 
> -- Sets of tags used for resolving a type or deriving a type, depending on the case
> tagmask_declaration = typedb.reduction_tagmask( tag_typeDeclaration)
> tagmask_resolveType = typedb.reduction_tagmask( tag_typeDeduction, tag_typeDeclaration)
> 
> 
38c48
< 		end, 1)
---
> 		end, tag_typeDeduction)
71c81
< typedb:def_reduction( qualitype_baseclass.ref, qualitype_class.ref, loadMemberConstructor( "class", 1), 1)
---
> typedb:def_reduction( qualitype_baseclass.ref, qualitype_class.ref, loadMemberConstructor( "class", 1), tag_typeDeduction)
74c84
< local resolveTypeId, reductions, items = typedb:resolve_type( qualitype_class.ref, "constructor")
---
> local resolveTypeId, reductions, items = typedb:resolve_type( qualitype_class.ref, "constructor", tagmask_declaration)

```
#### Output with tags
```
Not found

```
#### Conclusion
We explained now the 4th parameter of the ```typedb:def_reduction``` defined as ```1``` in the first examples. It is the mandatory tag assigned to the reduction.
The command ```typedb.reduction_tagmask``` is used to declare named sets of tags selected for resolving and deriving types.

#### Remark
```typedb:derive_type``` has a second optional tag mask parameter that selects reductions to count and limit to a specified number, 1 by default.
The aim behind that is to allow restrictions on some classes of reductions. Most statically typed programming languages impose restrictions on the number of conversions of a parameter. The second tag mask helps you to implement such restrictions.  


### Objects with a Scope

In a compiler, we have building blocks that are bound to a scope. For example functions. These building blocks are best represented as objects. If we are in the depth of an _AST_ tree traversal we would like to have a way to address these objects without complicated structures passed down as parameters. This would be very error-prone. Especially in a non strictly typed language as _Lua_. 

#### Introduction
##### Define object instance in the current scope
```Lua
typedb:set_instance( name, obj)
```
##### Get the object instance of this or the nearest enclosing scope
```Lua
obj = typedb:get_instance( name)
```
##### Get the object instance declared of the current scope
```Lua
obj = typedb:this_instance( name)
```

#### Source
```Lua
mewa = require "mewa"
typedb = mewa.typedb()

-- Attach a data structure for a callable to its scope
function defineCallableEnvironment( name)
	local env = {name=name, scope=(typedb:scope())}
	if typedb:this_instance( "callenv") then error( "Callable environment defined twice") end
	typedb:set_instance( "callenv", env)
	return env
end
-- Get the active callable name
function getFunctionName()
	return typedb:get_instance( "callenv").name
end

typedb:scope( {1,100} )
defineCallableEnvironment( "function X")
typedb:scope( {20,30} )
defineCallableEnvironment( "function Y")
typedb:scope( {70,90} )
defineCallableEnvironment( "function Z")

typedb:step( 10)
print( "We are in " .. getFunctionName())
typedb:step( 25)
print( "We are in " .. getFunctionName())
typedb:step( 85)
print( "We are in " .. getFunctionName())


```
#### Output
```
We are in function X
We are in function Y
We are in function Z

```
#### Conclusion
The possibility of attaching named objects to a scope is a big deal for readability, an important property for prototyping.
It brings structure into a system in an interpreter context where we have few possibilities to ensure safety by contracts.


### Control Structures, Implementing an IF
The next step is implementing an IF and to introduce a new class of type. 
Most programming languages require that the evaluation of a boolean expression stops when its outcome is determined.
An expression like ```if ptr && ptr->value()``` should not crash in the case ptr is a NULL pointer.

#### Introduction
For evaluating boolean expressions and as condition type of conditionals we define two new types.
##### Control True Type
This type has a field ```code``` that holds the code executed as long as it evaluates to true and a field ```out``` that holds an unbound label where the control branches to in case of evaluating to false.
##### Control False Type
This type is the mirror type of the control true type. It has a field ```code``` that holds the code that is executed as long as it evaluates to false and a field ```out``` that holds an unbound label where the control branches to in case of evaluating to true.
##### The IF statement
The IF takes the condition argument and transforms it into a control true type. The code of the resulting constructor is joined with the constructor code of the statements to evaluate in the case the condition is true. At the end, the unbound label is bound and declared at the end of the code.

#### Source
```Lua
mewa = require "mewa"
typedb = mewa.typedb()

-- [1] Define some helper functions
-- Map template for LLVM Code synthesis
function constructor_format( fmt, argtable)
	return (fmt:gsub("[{]([_%d%w]*)[}]", function( match) return argtable[ match] or "" end))
end

-- A register allocator as a generator function counting from 1, returning the LLVM register identifiers:
function register_allocator()
	local i = 0
	return function ()
		i = i + 1
		return "%R" .. i
	end
end

-- A label allocator as a generator function counting from 1, returning the LLVM label identifiers:
function label_allocator()
	local i = 0
	return function ()
		i = i + 1
		return "L" .. i
	end
end

-- Build a constructor
function applyConstructor( constructor, this, arg)
	if constructor then
		if (type(constructor) == "function") then return constructor( this, arg) else return constructor end
	else
		return this
	end
end

local register = register_allocator()
local label = label_allocator()

local controlTrueType = typedb:def_type( 0, " controlTrueType")
local controlFalseType = typedb:def_type( 0, " controlFalseType")
local scalarBooleanType = typedb:def_type( 0, "bool")

-- Build a control true type from a boolean value
local function booleanToControlTrueType( constructor)
	local true_label = label()
	local false_label = label()
	return {code = constructor.code
			.. constructor_format( "br i1 {inp}, label %{on}, label %{out}\n{on}:\n",
				{inp=constructor.out, out=false_label, on=true_label}), out=false_label}
end
-- Build a control false type from a boolean value
local function booleanToControlFalseType( constructor)
	local true_label = label()
	local false_label = label()
	return {code = constructor.code
			.. constructor_format( "br i1 {inp}, label %{on}, label %{out}\n{on}:\n",
				{inp=constructor.out, out=true_label, on=false_label}), out=true_label}
end

typedb:def_reduction( controlTrueType, scalarBooleanType, booleanToControlTrueType, 1)
typedb:def_reduction( controlFalseType, scalarBooleanType, booleanToControlFalseType, 1)

-- Implementation of an 'if' statement
--   condition is a type/constructor pair, block a constructor, return value is a constructor
function if_statement( condition, block)
	local reductions,weight,altpath = typedb:derive_type( controlTrueType, condition.type)
	if not weight then error( "Type not usable as conditional") end
	if altpath then error("Ambiguous") end
	local constructor = condition.constructor
	for _,redu in ipairs(reductions or {}) do
		constructor = applyConstructor( redu.constructor, constructor)
	end
	local code = constructor.code .. block.code
			.. constructor_format( "br label {out}\n{out}:\n", {out=constructor.out})
	return {code=code}
end

local condition_in = register()
local condition_out = register()
local condition = {
	type=scalarBooleanType,
	constructor={code = constructor_format( "{out} = icmp ne i32 {this}, 0\n",
				{out=condition_out,this=condition_in}), out=condition_out}
}
local block = {code = "... this code is executed if the value in " .. condition_in .. " is not 0 ...\n"}

print( if_statement( condition, block).code)



```
#### Output
```
%R2 = icmp ne i32 %R1, 0
br i1 %R2, label %L1, label %L2
L1:
... this code is executed if the value in %R1 is not 0 ...
br label L2
L2:


```

#### Conclusion
Control structures are implemented by constructing the control boolean types required. Boolean operators as the logical **AND** or the logical **OR** are constructed by wiring control boolean types together. This has not been done in this example, but it is imaginable after constructing an **IF**. The construction of types with reduction rules does not stop here. Control structures are not entirely different animals.

We have seen the whole variety of functions of the _typedb_ library now. The remaining functions not explained yet are convenient functions to set and get attributes of types. There is nothing substantial left to explain about the API though there is a lot more to talk about best practices and how to use this API.

I think we are now ready to look at our example compiler as a whole.


## Example Language Grammar

Now we will take a look at the grammar of the example language. 

### Parser Generator
_Mewa_ implements an _LALR(1)_ parser generator. The source file of the attributed grammar has 3 parts. 

  * Some configuration marked with a prefix '**%**'
  * The named lexems as a regular expression that matches the lexem value as argument.
  * An attributed grammar with keywords as strings and lexem or production names as identifiers

### Production Attribute Operators
The operator **>>** in the production attributes in oval brackets on the right side is incrementing the scope-step.
The operator **{}** in the production attributes is defining a scope range.

### Production Head Attributes
The attributes **L1**,**L2**,... are defining the production to be left associative with a priority specified as a number. 
The attributes **R1**,**R2**,... are defining the production to be right associative with a priority specified as a number. 

### Source
```
% LANGUAGE tutolang;
% TYPESYSTEM "tutorial/typesystem";
% CMDLINE "cmdlinearg";
% COMMENT "/*" "*/";
% COMMENT "//";

BOOLEAN : '((true)|(false))';
IDENT	: '[a-zA-Z_]+[a-zA-Z_0-9]*';
DQSTRING: '["]((([^\\"\n]+)|([\\][^"\n]))*)["]' 1;
UINTEGER: '[0123456789]+';
FLOAT	: '[0123456789]*[.][0123456789]+';
FLOAT	: '[0123456789]*[.][0123456789]+[Ee][+-]{0,1}[0123456789]+';

program		   	= extern_definitionlist free_definitionlist main_procedure	(program)
			;
extern_definitionlist	= extern_definition extern_definitionlist
			|
			;
free_definitionlist	= free_definition free_definitionlist
			|
			;
inclass_definitionlist	= inclass_definition inclass_definitionlist
			|
			;
extern_definition	= "extern" "function" IDENT typespec "(" extern_paramlist ")" ";" (extern_funcdef)
			;
extern_paramdecl	= typespec IDENT						(extern_paramdef)
			| typespec							(extern_paramdef)
			;
extern_parameters 	= extern_paramdecl "," extern_parameters
			| extern_paramdecl
			;
extern_paramlist	= extern_parameters						(extern_paramdeflist)
			|								(extern_paramdeflist)
			;
inclass_definition	= variabledefinition ";"					(definition 2)
			| classdefinition 						(definition 1)
			| functiondefinition						(definition_2pass 4)
			;
free_definition		= variabledefinition ";"					(definition 1)
			| classdefinition 						(definition 1)
			| functiondefinition						(definition 1)
			;
typename/L1		= IDENT
			| IDENT "::" typename
			;
typehdr/L1		= typename							(typehdr)
			;
typegen/L1		= typehdr
			| typegen "[" UINTEGER "]"					(arraytype)
			;
typespec/L1		= typegen							(typespec)
			;
classdefinition		= "class" IDENT "{" inclass_definitionlist "}"			(>>classdef)
			;
functiondefinition	= "function" IDENT typespec callablebody			(funcdef)
			;
callablebody		= "(" impl_paramlist ")" "{" statementlist "}"			({}callablebody)
			;
main_procedure		= "main" codeblock						(main_procdef)
			|
			;
impl_paramlist		= impl_parameters						(paramdeflist)
			|								(paramdeflist)
			;
impl_parameters		= impl_paramdecl "," impl_parameters
			| impl_paramdecl
			;
impl_paramdecl		= typespec IDENT						(paramdef)
			;
codeblock/L1		= "{" statementlist "}"						({}codeblock)
			;
statementlist/L1	= statement statementlist					(>>)
			|
			;
elseblock/L1		= "elseif" "(" expression ")" codeblock	elseblock		(conditional_elseif)
			| "elseif" "(" expression ")" codeblock 			(conditional_elseif)
			| "else" codeblock 						(conditional_else)
			;
statement/L1		= classdefinition 						(definition 1)
			| functiondefinition						(definition 1)
			| "var" variabledefinition ";"					(>>definition 1)
			| expression ";"						(free_expression)
			| "return" expression ";"					(>>return_value)
			| "return" ";"							(>>return_void)
			| "if" "(" expression ")" codeblock elseblock			(conditional_if)
			| "if" "(" expression ")" codeblock 				(conditional_if)
			| "while" "(" expression ")" codeblock				(conditional_while)
			| codeblock
			;
variabledefinition	= typespec IDENT						(>>vardef)
			| typespec IDENT "=" expression					(>>vardef)
			;
expression/L1		= "{" expressionlist "}"					(>>structure)
			| "{" "}"							(>>structure)
			;
expression/L2		= IDENT								(variable)
			| BOOLEAN							(constant "constexpr bool")
			| UINTEGER							(constant "constexpr int")
			| FLOAT								(constant "constexpr double")
			| DQSTRING							(constant "constexpr string")
			;
expression/L3		= expression  "="  expression					(>>binop "=")
			;
expression/L4		= expression  "||"  expression					(>>binop "||")
			;
expression/L5		= expression  "&&"  expression					(>>binop "&&")
			;
expression/L8		= expression  "=="  expression					(>>binop "==")
			| expression  "!="  expression					(>>binop "!=")
			| expression  "<="  expression					(>>binop "<=")
			| expression  "<"  expression					(>>binop "<")
			| expression  ">="  expression					(>>binop ">=")
			| expression  ">"  expression					(>>binop ">")
			;
expression/L9		= expression  "+"  expression					(>>binop "+")
			| expression  "-"  expression					(>>binop "-")
			| "-"  expression						(>>unop "-")
			| "+"  expression						(>>unop "+")
			;
expression/L10		= expression  "*"  expression					(>>binop "*")
			| expression  "/"  expression					(>>binop "/")
			;
expression/L12		= expression "." IDENT						(member)
			;
expression/L13		= expression  "(" expressionlist ")"				(>>operator "()")
			| expression  "(" ")"						(>>operator "()")
			| expression  "[" expressionlist "]"				(>>operator_array "[]")
			;
expressionlist/L0	= expression "," expressionlist
			| expression
			;


```


## Typesystem
Now let's overview the implementation of the typesystem module that generates the code.
The code shown now will be more organised, more complete, but in contrary to the tutorial not self-contained anymore.

In contrast to the example [language1](example_language1.md) the typesystem module has been splitted in several parts of maximum 100 lines of code to make them digestible. The snippets implementing helper functions are in the directory ```tutorial/sections```. The snippets implementing the functions attached to the _AST_ nodes are in the directory ```tutorial/ast```.

Because the amount of code in this part, we will not inspect it so closely as in the tutorial anymore. 
But I hope that after the tutorial you will still get a grip on the code shown.

### Header
Let's first take a look at the header of typesystem.lua. 

#### Submodule llvmir
The submodule ```llvmir``` implements all templates for the LLVM IR code generation. We sah such templates like ```{out} = load i32, i32* {this}``` with substututes in curly brackets in the examples of the tutorial. In the example compiler these templates are all declared in the module ```llvmir``` and referred to by name. The module ```llvmir``` has a submodule ```llvmir_scalar``` that is generated from a description of the scalar types of our language.

#### Submodule utils
The submodule ```typesystem_utils``` implements functions that are language independent. For example the function ```constructor_format``` that instantiated the llvmir code templates in the tutorial. It is implemented there in a more powerful form. String encoding, mangling, and _AST_ tree traversal functions are other examples there.

#### Global variables
The approach of _Mewa_ is not pure. Things are stored in the _typedb_ if it helps. For everything else we use global variables. The API of the _typedb_ is pragmatic and kept as minimal as reasonable. The header of ```typesystem.lua ``` has about a dozen global variables declared. In the example **language1** there are around 50 variables defined. For example ```typeDescriptionMap```, a map that associates every type with a table with the code generation templates for this type as members.

#### Source
```
mewa = require "mewa"
local utils = require "typesystem_utils"
llvmir = require "llvmir"
typedb = mewa.typedb()

-- Global variables
localDefinitionContext = 0	-- Context type of local definitions
seekContextKey = "seekctx"	-- Key for seek context types defined for a scope
callableEnvKey = "env"		-- Key for current callable environment
typeDescriptionMap = {}		-- Map of type ids to their description
referenceTypeMap = {}		-- Map of value type ids to their reference type if it exists
dereferenceTypeMap = {}		-- Map of reference type ids to their value type if it exists
arrayTypeMap = {}		-- Map array keys to their array type
stringConstantMap = {}		-- Map of string constant values to a structure with the attributes {name,size}
scalarTypeMap = {}		-- Map of scalar type names to the correspoding value type

dofile( "examples/tutorial/sections.inc")

-- AST Callbacks:
typesystem = {}
dofile( "examples/tutorial/ast.inc")
return typesystem

```

### Output
The output of out compiler front-end is printed with the functions ```print``` and ```print_section``` defined by the compiler. The function ```print``` is redirected to a function appending to the ```Code``` section of the output. The sections are variables in the [target files](../examples/target). The function ```print_section``` is appending to an output section specified as first parameter.

### AST Traversal
All _AST_ node functions participate in the traversal as it is in their responsibility to call the subnodes of the _AST_. I already mentioned in the tutorial that the current scope is implemented as own aspect and set by the _AST_ traversal function. The traversal functions are implemented in the ```typesystem_utils``` module. There are two variants of the traversal function:

  * ```function traverse( typedb, node, ...)```
  * ```function traverseRange( typedb, node, range, ...)```

The ```range``` is a pair of _Lua_ array indices ```{first, last element of the range}``` referring to the subnodes to process for a partial traversal. Partial traversal is used for processing the subnodes selectively. You may for example traverse the function name and the parameters first, then declare the function, and then traverse the function body for enabling recursion.
The variable arguments **...** are forwarded to the _AST_ node functions called. This way you can pass parameters down to the subnodes. The examples of _Mewa_ use parameters extensively. For example to pass down the declaration context that decides, wheter a variable declaration is member of a structure or a local or a global variable. Parameters are also used to implement multipass traversal or the _AST_ to parse declarations of a subtree, e.g. a class in a specific order. You pass down the index of the pass you want to process in an _AST_ node function. But be aware that this way of communicating is error prone. You should restrict it to a bare minimum and use scope bound data (```typedb:set_instance```, ```typedb:get_instance```) or even global variables instead.

##### Source from typesystem_utils
```Lua
-- Tree traversal helper function setting the current scope/step and calling the function of one node, and returning its result:
local function processSubnode( typedb, subnode, ...)
	local rt
	if subnode.call then
		if (subnode.scope) then
			local scope_bk,step_bk = typedb:scope( subnode.scope)
			typedb:set_instance( "node", subnode)
			if subnode.call.obj then rt = subnode.call.proc( subnode, subnode.call.obj, ...) else rt = subnode.call.proc( subnode, ...) end
			typedb:scope( scope_bk,step_bk)
		elseif (subnode.step) then
			local step_bk = typedb:step( subnode.step)
			if subnode.call.obj then rt = subnode.call.proc( subnode, subnode.call.obj, ...) else rt = subnode.call.proc( subnode, ...) end
			typedb:step( step_bk)
		else
			if subnode.call.obj then rt = subnode.call.proc( subnode, subnode.call.obj, ...) else rt = subnode.call.proc( subnode, ...) end
		end
	else
		rt = subnode.value
	end
	return rt
end
-- Tree traversal or a subrange of argument nodes, define scope/step and do the traversal call
function utils.traverseRange( typedb, node, range, ...)
	if node.arg then
		local rt = {}
		local start,last = table.unpack(range)
		local lasti,ofs = last-start+1,start-1
		for ii=1,lasti do rt[ ii] = processSubnode( typedb, node.arg[ ofs+ii], ...) end
		return rt
	else
		return node.value
	end
end
-- Tree traversal, define scope/step and do the traversal call
function utils.traverse( typedb, node, ...)
	if node.arg then
		local rt = {}
		for ii,subnode in ipairs(node.arg) do rt[ii] = processSubnode( typedb, subnode, ...) end
		return rt
	else
		return node.value
	end
end
```

### AST Node functions
Now we will visit the functions attchached to the _AST_ nodes. I split them into snippets covering different aspects. 
Most of the code is just delegating to functions we will inspect in the following section. 

#### Constants
Define atomic and structure constants in the source.

##### Structures, e.g. Initializer Lists
A structure has a list of type/constructor pairs as constructor. This resembles the parameter list of a function and that's what it is. For recursive initialization of objects from initializer lists, we declare a reduction from the type constexprStructureType to any object beeing initializable by a contant structure. The constructor is using the typedb to find a matching constructor with this list matching as parameter list. If it fails the constructor returns *nil* to indicate that it failed and that the solution relying on this reduction should be dropped. This kind of enveloping helps us to map initializer lists recursively.
```Lua
function typesystem.constant( node, decl)
	local typeId = scalarTypeMap[ decl]
	return {type=typeId,constructor=createConstexprValue( typeId, node.arg[1].value)}
end
function typesystem.structure( node)
	local args = utils.traverse( typedb, node)
	return {type=constexprStructureType, constructor={list=args}}
end

```

#### Variables
In this section are the node functions to define and query variables. These _AST_ node functions are just delegating to functions we will revisit later.
```Lua
function typesystem.vardef( node, context)
	local datatype,varnam,initval = table.unpack( utils.traverse( typedb, node, context))
	return defineVariable( node, context, datatype, varnam, initval)
end
function typesystem.variable( node)
	local varname = node.arg[ 1].value
	return getVariable( node, varname)
end

```

#### Extern Function Declarations
Here are the extern function declarations with their parameters processed. The function ```typesystem.extern_funcdef``` calls the tree traversal to get the function name and the parameters. The collecting of the parameters is possible in different ways. Here a table is defined by the node that declares the parameter list, passed down as parameter to the recursive list declaration, and filled by the parameter declaration node ```typesystem.extern_paramdef```. The function call definition with ```defineFunctionCall``` is the same as for free functions and class methods.
```Lua
function typesystem.extern_funcdef( node)
	local context = {domain="global"}
	local name,ret,param = table.unpack( utils.traverse( typedb, node, context))
	if ret == voidType then ret = nil end -- void type as return value means that we are in a procedure without return value
	local descr = {externtype="C", name=name, symbolname=name, func='@' .. name, ret=ret, param=param, signature=""}
	descr.rtllvmtype = ret and typeDescriptionMap[ ret].llvmtype or "void"
	defineFunctionCall( node, descr, context)
	print_section( "Typedefs", utils.constructor_format( llvmir.control.extern_functionDeclaration, descr))
end
function typesystem.extern_paramdef( node, param)
	local typeId = table.unpack( utils.traverseRange( typedb, node, {1,1}, context))
	local descr = typeDescriptionMap[ typeId]
	if not descr then utils.errorMessage( node.line, "Type '%s' not allowed as parameter of extern function", typedb:type_string( typeId)) end
	table.insert( param, {type=typeId,llvmtype=descr.llvmtype})
end
function typesystem.extern_paramdeflist( node)
	local param = {}
	utils.traverse( typedb, node, param)
	return param
end

```

#### Data Types
This section defines classes, arrays and atomic data types. The function we have to discuss a little bit deeper here is ```typesystem.classdef```, the definition of a class:
 1. The context is changed. Whatever is passed to this _AST_ node function as context, we define a new context (```classContext```) to be passed down to the subnodes in the traversal. The context is a structure a field ```domain``` that tells if we are inside the definition of a class ("member"), if we are in the body of a function ("local"), or if we are not inside any structure ("global"). It has other fields too depending on the context. In classes it is also used to collect data (```structsize```,```members```) from its subnodes.
 2. The traversal of the subnodes has 4 passes. The index of the pass is passed down as parameter for the subnodes to decide what to do:
    * 1st Pass: Type Definitions
    * 2nd Pass: Member Variable Declarations
    * 3rd Pass: Method Declarations (the traversal calls 1st pass of function declarations)
    * 4th Pass: Method Implementation (the traversal calls the 2nd pass of function declarations)

The class node defines a multiple pass evaluation and the nodes ```typesystem.definition_2pass``` and ```typesystem.definition``` implement the gates that encapsulate the decision what is processed when.
 
```Lua
function typesystem.typehdr( node, decl)
	return resolveTypeFromNamePath( node, utils.traverse( typedb, node))
end
function typesystem.arraytype( node)
	local dim = tonumber( node.arg[2].value)
	local elem = table.unpack( utils.traverseRange( typedb, node, {1,1}))
	return {type=getOrCreateArrayType( node, expectDataType( node, elem), dim)}
end
function typesystem.typespec( node)
	return expectDataType( node, table.unpack( utils.traverse( typedb, node)))
end
function typesystem.classdef( node, context)
	local typnam = node.arg[1].value
	local declContextTypeId = getDeclarationContextTypeId( context)
	pushSeekContextType( declContextTypeId)
	local typeId,descr = defineStructureType( node, declContextTypeId, typnam, llvmir.structTemplate)
	local classContext = {domain="member", decltype=typeId, members={}, descr=descr}
	utils.traverse( typedb, node, classContext, 1)	-- 1st pass: type definitions
	utils.traverse( typedb, node, classContext, 2)	-- 2nd pass: member variables
	descr.size = classContext.structsize
	descr.members = classContext.members
	defineClassStructureAssignmentOperator( node, typeId)
	utils.traverse( typedb, node, classContext, 3)	-- 3rd pass: method declarations
	utils.traverse( typedb, node, classContext, 4)	-- 4th pass: method implementations
	print_section( "Typedefs", utils.constructor_format( descr.typedef, {llvmtype=classContext.llvmtype}))
	popSeekContextType( declContextTypeId)
end
function typesystem.definition( node, pass, context, pass_selected)
	if not pass_selected or pass == pass_selected then	-- if the pass matches the declaration in the grammar
		local statement = table.unpack( utils.traverse( typedb, node, context or {domain="local"}))
		return statement and statement.constructor or nil
	end
end
function typesystem.definition_2pass( node, pass, context, pass_selected)
	if not pass_selected then
		return typesystem.definition( node, pass, context, pass_selected)
	elseif pass == pass_selected+1 then
		utils.traverse( typedb, node, context, 1) 	-- 3rd pass: declarations
	elseif pass == pass_selected then
		utils.traverse( typedb, node, context, 2)	-- 4th pass: implementations
	end
end


```

#### Function Declarations
Now we have a look at functions with the parameters and the body. The traversal is split into two passes, the declaration and implementation as already mentioned in the data type section. A helper function ```utils.allocNodeData``` attaches the function description created in the first pass to the node. The function ```utils.getNodeData``` references it in the second pass. At the end of the 2nd pass the LLVM function declaration is printed to the output. The constructor of the function call and the function type with its parameters have already been declared in the 1st pass.

To mention is also the call of ```defineCallableEnvironment``` that creates a structure referred to as callable environment and binds it to the scope of the function body. We have seen in the tutorial how this works (Objects with a Scope). The callable environment structure contains all function related data like the return type, the register allocator, etc.
It is accessed with the function ```getCallableEnvironment```.

The _AST_ node function ```typesystem.callablebody``` collects the elements of the free function or class method description in the scope of the callable body. It collects the list of parameters in a similar way as the extern function declaration we have already seen.

```Lua
function typesystem.funcdef( node, context, pass)
	local typnam = node.arg[1].value
	if not pass or pass == 1 then
		local rtype = table.unpack( utils.traverseRange( typedb, node, {2,2}, context))
		if rtype == voidType then rtype = nil end -- void type as return value means that we are in a procedure without return value
		local symbolname = (context.domain == "member") and (typedb:type_name(context.decltype) .. "__" .. typnam) or typnam
		local rtllvmtype = rtype and typeDescriptionMap[ rtype].llvmtype or "void"
		local descr = {lnk="internal", name=typnam, symbolname=symbolname, func='@'..symbolname, ret=rtype, rtllvmtype=rtllvmtype, attr="#0"}
		utils.traverseRange( typedb, node, {3,3}, context, descr, 1)	-- 1st pass: function declaration
		utils.allocNodeData( node, localDefinitionContext, descr)
		defineFunctionCall( node, descr, context)
	end
	if not pass or pass == 2 then
		local descr = utils.getNodeData( node, localDefinitionContext)
		utils.traverseRange( typedb, node, {3,3}, context, descr, 2)	-- 2nd pass: function implementation
		if descr.ret then
			local rtdescr = typeDescriptionMap[descr.ret]
			descr.body = descr.body .. utils.constructor_format( llvmir.control.returnStatement, {type=rtdescr.llvmtype,this=rtdescr.default})
		else
			descr.body = descr.body .. utils.constructor_format( llvmir.control.returnVoidStatement)
		end
		print( utils.constructor_format( llvmir.control.functionDeclaration, descr))
	end
end
function typesystem.callablebody( node, context, descr, selectid)
	local rt
	local subcontext = {domain="local"}
	if selectid == 1 then -- parameter declarations
		descr.env = defineCallableEnvironment( node, "body " .. descr.name, descr.ret)
		descr.param = table.unpack( utils.traverseRange( typedb, node, {1,1}, subcontext))
		descr.paramstr = getDeclarationLlvmTypeRegParameterString( descr, context)
	elseif selectid == 2 then -- statements in body
		if context.domain == "member" then expandMethodEnvironment( node, context, descr, descr.env) end
		local statementlist = utils.traverseRange( typedb, node, {2,#node.arg}, subcontext)
		local code = ""
		for _,statement in ipairs(statementlist) do code = code .. statement.code end
		descr.body = code
	end
	return rt
end
function typesystem.paramdef( node, param)
	local datatype,varname = table.unpack( utils.traverse( typedb, node, param))
	table.insert( param, defineParameter( node, datatype, varname))
end
function typesystem.paramdeflist( node, param)
	local param = {}
	utils.traverse( typedb, node, param)
	return param
end

```

#### Operators
This section defines the the functions of _AST_ nodes implementing operators. The contruction of operators, member references, function calls are all redirected to the function ```applyCallable``` we will visit later. Function calls are implemented as operator "()" of a callable type.
The functions ```expectValueType``` and ```expectDataType``` used here are assertions.
```Lua
function typesystem.binop( node, operator)
	local this,operand = table.unpack( utils.traverse( typedb, node))
	expectValueType( node, this)
	expectValueType( node, operand)
	return applyCallable( node, this, operator, {operand})
end
function typesystem.unop( node, operator)
	local this = table.unpack( utils.traverse( typedb, node))
	expectValueType( node, operand)
	return applyCallable( node, this, operator, {})
end
function typesystem.member( node)
	local struct,name = table.unpack( utils.traverse( typedb, node))
	return applyCallable( node, struct, name)
end
function typesystem.operator( node, operator)
	local args = utils.traverse( typedb, node)
	local this = table.remove( args, 1)
	return applyCallable( node, this, operator, args)
end
function typesystem.operator_array( node, operator)
	local args = utils.traverse( typedb, node)
	local this = table.remove( args, 1)
	return applyCallable( node, this, operator, args)
end

```

#### Control Structures
As we learned in the turorial, conditionals are implemented by wiring a control boolean type together with some code blocks. The control boolean type derived from the condition type with ```getRequiredTypeConstructor```.

The return node functions also use ```getRequiredTypeConstructor``` to derive the value returned from their argument. The return type is accessible in the callable environment structure.

```Lua
function typesystem.conditional_if( node)
	local env = getCallableEnvironment()
	local exitLabel = env.label()
	local condition,yesblock,noblock = table.unpack( utils.traverse( typedb, node, exitLabel))
	local rt = conditionalIfElseBlock( node, condition, yesblock, noblock, exitLabel)
	rt.code = rt.code .. utils.constructor_format( llvmir.control.label, {inp=exitLabel})
	return rt
end
function typesystem.conditional_else( node, exitLabel)
	return table.unpack( utils.traverse( typedb, node))
end
function typesystem.conditional_elseif( node, exitLabel)
	local env = getCallableEnvironment()
	local condition,yesblock,noblock = table.unpack( utils.traverse( typedb, node, exitLabel))
	return conditionalIfElseBlock( node, condition, yesblock, noblock, exitLabel)
end
function typesystem.conditional_while( node)
	local env = getCallableEnvironment()
	local condition,yesblock = table.unpack( utils.traverse( typedb, node))
	local cond_constructor = getRequiredTypeConstructor( node, controlTrueType, condition, tagmask_matchParameter, tagmask_typeConversion)
	if not cond_constructor then utils.errorMessage( node.line, "Can't use type '%s' as a condition", typedb:type_string(condition.type)) end
	local start = env.label()
	local startcode = utils.constructor_format( llvmir.control.label, {inp=start})
	return {code = startcode .. cond_constructor.code .. yesblock.code
			.. utils.constructor_format( llvmir.control.invertedControlType, {out=start,inp=cond_constructor.out})}
end
function typesystem.return_value( node)
	local operand = table.unpack( utils.traverse( typedb, node))
	local env = getCallableEnvironment()
	local rtype = env.returntype
	local descr = typeDescriptionMap[ rtype]
	expectValueType( node, operand)
	if rtype == 0 then utils.errorMessage( node.line, "Can't return value from procedure") end
	local this,code = constructorParts( getRequiredTypeConstructor( node, rtype, operand, tagmask_matchParameter, tagmask_typeConversion))
	return {code = code .. utils.constructor_format( llvmir.control.returnStatement, {this=this, type=descr.llvmtype})}
end
function typesystem.return_void( node)
	local env = getCallableEnvironment()
	if env.returntype ~= 0 then utils.errorMessage( node.line, "Can't return without value from function") end
	return {code = utils.constructor_format( llvmir.control.returnVoidStatement, {})}
end

```

#### Blocks and the Rest
Finally we come to the section containing the blocks that do not fit somewhere else. 
  * The top level node, the program, that does some initializations of the language before further processing by delegation with the traversal of the subnodes.
  * The main program node, that does not do a lot here as there are no program arguments and no program return value implemented here. 
  * The node function ```typesystem.codeblock``` joins a list of statements to one constructor. 
  * The node function ```typesystem.free_expression``` makes a statement node from an expression node.

```Lua
function typesystem.program( node, options)
	defineConstExprArithmetics()
	initBuiltInTypes()
	initControlBooleanTypes()
	local context = {domain="global"}
	utils.traverse( typedb, node, context)
end
function typesystem.main_procdef( node)
	local env = defineCallableEnvironment( node, "main ", scalarIntegerType)
	local block = table.unpack( utils.traverse( typedb, node, {domain="local"}))
	local body = block.code .. utils.constructor_format( llvmir.control.returnStatement, {type="i32",this="0"})
	print( "\n" .. utils.constructor_format( llvmir.control.mainDeclaration, {body=body}))
end
function typesystem.codeblock( node)
	local stmlist = utils.traverse( typedb, node)
	local code = ""
	for _,stm in ipairs(stmlist) do code = code .. stm.code end
	return {code=code}
end
function typesystem.free_expression( node)
	local expression = table.unpack( utils.traverse( typedb, node))
	if expression.type == controlTrueType or expression.type == controlFalseType then
		return {code=expression.constructor.code .. utils.constructor_format( llvmir.control.label, {inp=expression.constructor.out})}
	else
		return {code=expression.constructor.code}
	end
end

```

### Typesystem Functions
This chapter will survey the functions used in the AST node functions we have now seen. They are also split into snippets covering different aspects.

#### Reduction Weights
Here are all reduction weights of the reductions defined. We have explained the need for weighting reductions in the tutorial.
```Lua
-- Centralized list of the ordering of the reduction rules determined by their weights, we force an order of reductions by defining the weight sums as polynomials:
rdw_conv = 1.0			-- Reduction weight of conversion
rdw_constexpr = 0.0675		-- Minimum weight of a reduction involving a constexpr value
rdw_load = 0.25			-- Reduction weight of loading a value
rdw_sign = 0.125		-- Conversion of integers changing sign
rdw_float = 0.25		-- Conversion switching floating point with integers
rdw_bool = 0.25			-- Conversion of numeric value to boolean
rdw_strip_r_1 = 0.25 / (1*1)	-- Reduction weight of stripping reference (fetch value) from type having 1 qualifier
rdw_strip_r_2 = 0.25 / (2*2)	-- Reduction weight of stripping reference (fetch value) from type having 2 qualifiers
rdw_strip_r_3 = 0.25 / (3*3)	-- Reduction weight of stripping reference (fetch value) from type having 3 qualifiers
rwd_inheritance = 0.125 / (4*4)	-- Reduction weight of class inheritance
rwd_control = 0.125 / (4*4)	-- Reduction weight of boolean type (control true/false type <-> boolean value) conversions

function preferWeight( flag, w1, w2) if flag then return w1 else return w2 end end -- Prefer one weight to the other
function factorWeight( fac, w1) return fac * w1	end
function combineWeight( w1, w2) return w1 + (w2 * 127.0 / 128.0) end -- Combining two reductions slightly less weight compared with applying both singularily
function scalarDeductionWeight( sizeweight) return 0.125*(1.0-sizeweight) end -- Deduction weight of this element for scalar operators
weightEpsilon = 1E-8		-- Epsilon used for comparing weights for equality


```

#### Reduction Tags and Tagmasks
This section defines all reduction tags and tagmasks. We have explained the need for tagging in the tutorial.
```Lua
-- Tags attached to reduction definitions. When resolving a type or deriving a type, we select reductions by specifying a set of valid tags
tag_typeDeclaration = 1			-- Type declaration relation (e.g. variable to data type)
tag_typeDeduction = 2			-- Type deduction (e.g. inheritance)
tag_typeConversion = 3			-- Type conversion of parameters
tag_typeInstantiation = 4		-- Type value construction from const expression
tag_transfer = 5			-- Transfer of information to build an object by a constructor, used in free function callable to pointer assignment

-- Sets of tags used for resolving a type or deriving a type, depending on the case
tagmask_declaration = typedb.reduction_tagmask( tag_typeDeclaration)
tagmask_resolveType = typedb.reduction_tagmask( tag_typeDeduction, tag_typeDeclaration)
tagmask_matchParameter = typedb.reduction_tagmask( tag_typeDeduction, tag_typeDeclaration, tag_typeConversion, tag_typeInstantiation, tag_transfer)
tagmask_typeConversion = typedb.reduction_tagmask( tag_typeConversion)


```

#### Type Declaration String
This function provides a signature string of the type including context type and parameter types.
```Lua
-- Type string of a type declaration built from its parts for error messages
function typeDeclarationString( this, typnam, args)
	local rt = (this == 0 or not this) and typnam or (typedb:type_string(type(this) == "table" and this.type or this) .. " " .. typnam)
	if (args) then rt = rt .. "(" .. utils.typeListString( typedb, args) .. ")" end
	return rt
end



```

#### Calls and Promote Calls
Here are the functions to define calls with parameters and a return value. For first class scalar types we often need to look also at the 2nd argument to determine the constructor function to call. Most statically typed programming languages specify a multiplication of an interger with a floating point number as a multiplication of floating point numbers. If we define the operator dependent on the first argument, we have to define the call int * float as conversion of the first operand to a float followed by a float multiplication. I call these calls promote calls (promoting the first argument to the type of the second argument) in the examples. The integer argument is promoted to a float and then the constructor function of the float multiplication is called.
```Lua
-- Constructor for a promote call (implementing int + double by promoting the first argument int to double and do a double + double to get the result)
function promoteCallConstructor( call_constructor, promote_constructor)
	return function( this, arg) return call_constructor( promote_constructor( this), arg) end
end
-- Define an operation with involving the promotion of the left hand argument to another type and executing the operation as defined for the type promoted to.
function definePromoteCall( returnType, thisType, promoteType, opr, argTypes, promote_constructor)
	local call_constructor = typedb:type_constructor( typedb:get_type( promoteType, opr, argTypes))
	local callType = typedb:def_type( thisType, opr, promoteCallConstructor( call_constructor, promote_constructor), argTypes)
	if callType == -1 then utils.errorMessage( node.line, "Duplicate definition '%s'", typeDeclarationString( thisType, opr, argTypes)) end
	if returnType then typedb:def_reduction( returnType, callType, nil, tag_typeDeclaration) end
end
-- Define an operation generalized
function defineCall( returnType, thisType, opr, argTypes, constructor)
	local callType = typedb:def_type( thisType, opr, constructor, argTypes)
	if callType == -1 then utils.errorMessage( 0, "Duplicate definition of call '%s'", typeDeclarationString( thisType, opr, argTypes)) end
	if returnType then typedb:def_reduction( returnType, callType, nil, tag_typeDeclaration) end
	return callType
end


```

#### Apply Constructors
This section defines the call of a constructor function. We got an impression of how this works in the tutorial (variable assignment). 
The functions ```tryApplyConstructor``` and ```applyConstructor``` are building a constructor by applying a constructor function on a list of arguments.
The functions ```tryApplyReductionList``` and ```applyReductionList``` are building a constructor from a chain of reductions applied on a base constructor.
The functions with a prefix ```try``` report a miss or an ambiguity with their return value instead of exiting with ```utils.errorMessage``` in case of an error.
```Lua
-- Application of a constructor depending on its type and its argument type, return false as 2nd result on failure, true on success
function tryApplyConstructor( node, typeId, constructor, arg)
	if constructor then
		if (type(constructor) == "function") then
			local rt = constructor( arg)
			return rt, rt ~= nil
		elseif arg then
			utils.errorMessage( node.line, "Reduction constructor overwriting previous constructor for '%s'", typedb:type_string(typeId))
		else
			return constructor, true
		end
	else
		return arg, true
	end
end
-- Application of a constructor depending on its type and its argument type, throw error on failure
function applyConstructor( node, typeId, constructor, arg)
	local result_constructor,success = tryApplyConstructor( node, typeId, constructor, arg)
	if not success then utils.errorMessage( node.line, "Failed to create type '%s'", typedb:type_string(typeId)) end
	return result_constructor
end
-- Try to apply a list of reductions on a constructor, return false as 2nd result on failure, true on success
function tryApplyReductionList( node, redulist, redu_constructor)
	local success = true
	for _,redu in ipairs(redulist) do
		redu_constructor,success = tryApplyConstructor( node, redu.type, redu.constructor, redu_constructor)
		if not success then return nil end
	end
	return redu_constructor, true
end
-- Apply a list of reductions on a constructor, throw error on failure
function applyReductionList( node, redulist, redu_constructor)
	local success = true
	for _,redu in ipairs(redulist) do
		redu_constructor,success = tryApplyConstructor( node, redu.type, redu.constructor, redu_constructor)
		if not success then utils.errorMessage( node.line, "Reduction constructor failed for '%s'", typedb:type_string(redu.type)) end
	end
	return redu_constructor
end


```

#### Resolve Types
Here are the functions for resolving types, mapping types, and asserting type properties.
  * The function ```selectNoArgumentType``` filters a resolve type match without parameters, a variable or a data type.
  * The function ```resolveTypeFromNamePath``` resolves a type from a namespace or structure name path.
  * The function ```tryGetWeightedParameterReductionList``` get the reduction of for a parameter matching and its weight. The weight can be used to accumulate the weight of a function match as a whole to determine the best match. In the example languages, the maximum is used as accumulation function for the weight of the function match.
  * The function ```getRequiredTypeConstructor``` transforms a type into another type.

```Lua
-- Get the handle of a type expected to have no arguments (plain typedef type or a variable name)
function selectNoArgumentType( node, seekctx, typeName, tagmask, resolveContextTypeId, reductions, items)
	if not resolveContextTypeId or type(resolveContextTypeId) == "table" then -- not found or ambiguous
		io.stderr:write( "TRACE typedb:resolve_type\n" .. utils.getResolveTypeTrace( typedb, seekctx, typeName, tagmask) .. "\n")
		utils.errorResolveType( typedb, node.line, resolveContextTypeId, seekctx, typeName)
	end
	for ii,item in ipairs(items) do
		if typedb:type_nof_parameters( item) == 0 then
			local constructor = applyReductionList( node, reductions, nil)
			local item_constructor = typedb:type_constructor( item)
			constructor = applyConstructor( node, item, item_constructor, constructor)
			return item,constructor
		end
	end
	utils.errorMessage( node.line, "Failed to resolve %s with no arguments", utils.resolveTypeString( typedb, seekctx, typeName))
end
-- Get the type handle of a type defined as a path (elements of the path are namespaces and parent structures followed by the type name resolved)
function resolveTypeFromNamePath( node, arg, argidx)
	if not argidx then argidx = #arg end
	local typeName = arg[ argidx]
	local seekContextTypes
	if argidx > 1 then
		seekContextTypes = expectDataType( node, resolveTypeFromNamePath( node, arg, argidx-1))
	else
		seekContextTypes = getSeekContextTypes()
	end
	local resolveContextTypeId, reductions, items = typedb:resolve_type( seekContextTypes, typeName, tagmask_namespace)
	local typeId,constructor = selectNoArgumentType( node, seekContextTypes, typeName, tagmask_namespace, resolveContextTypeId, reductions, items)
	return {type=typeId, constructor=constructor}
end
-- Try to get the constructor and weight of a parameter passed with the deduction tagmask optionally passed as an argument
function tryGetWeightedParameterReductionList( node, redutype, operand, tagmask_decl, tagmask_conv)
	if redutype ~= operand.type then
		local redulist,weight,altpath = typedb:derive_type( redutype, operand.type, tagmask_decl, tagmask_conv)
		if altpath then
			utils.errorMessage( node.line, "Ambiguous derivation paths for '%s': %s | %s",
						typedb:type_string(operand.type), utils.typeListString(typedb,altpath," =>"), utils.typeListString(typedb,redulist," =>"))
		end
		return redulist,weight
	else
		return {},0.0
	end
end
-- Get the constructor of a type required. The deduction tagmasks are passed as an arguments
function getRequiredTypeConstructor( node, redutype, operand, tagmask_decl, tagmask_conv)
	if redutype ~= operand.type then
		local redulist,weight,altpath = typedb:derive_type( redutype, operand.type, tagmask_decl, tagmask_conv)
		if not redulist or altpath then
			io.stderr:write( "TRACE typedb:derive_type " .. typedb:type_string(redutype) .. " <- " .. typedb:type_string(operand.type) .. "\n"
					.. utils.getDeriveTypeTrace( typedb, redutype, operand.type, tagmask_decl, tagmask_conv) .. "\n")
		end
		if not redulist then
			utils.errorMessage( node.line, "Type mismatch, required type '%s'", typedb:type_string(redutype))
		elseif altpath then
			utils.errorMessage( node.line, "Ambiguous derivation paths for '%s': %s | %s",
						typedb:type_string(operand.type), utils.typeListString(typedb,altpath," =>"), utils.typeListString(typedb,redulist," =>"))
		end
		local rt = applyReductionList( node, redulist, operand.constructor)
		if not rt then utils.errorMessage( node.line, "Construction of '%s' <- '%s' failed", typedb:type_string(redutype), typedb:type_string(operand.type)) end
		return rt
	else
		return operand.constructor
	end
end
-- Issue an error if the argument does not refer to a value type
function expectValueType( node, item)
	if not item.constructor then utils.errorMessage( node.line, "'%s' does not refer to a value", typedb:type_string(item.type)) end
end
-- Issue an error if the argument does not refer to a data type
function expectDataType( node, item)
	if item.constructor then utils.errorMessage( node.line, "'%s' does not refer to a data type", typedb:type_string(item.type)) end
	return item.type
end

```
##### Note
The function ```getSeekContextTypes()``` called here is important to resolve types. It provides the list of the candidate context type/constructor pairs used to resolve a type. Inside a structure for example, it is allowed to access elements of this structure by name without a full namespace path of the element. But we can also access global types. Therefore we need to maintain a list of current context types that is extended on the entry into a structure and shrinked on exit.


#### Apply Callable
This section is the most complicated of the whole example compiler. 
It shows the code for how to find the best match of a callable with parameters. The candidates are fetched from a priority queue ordered by weight.
Constructor functions of the top candidates are called and if they succeed to build the objects then the match is returned.
Candidates with a failing constructor are dropped to enable recursive initializer structures.

The functions ```tryApplyCallable``` and ```applyCallable``` are provided for resolving all types with arguments (operators, member access, function call, etc.).
```Lua
-- For a callable item, create for each argument the lists of reductions needed to pass the arguments to it, with accumulation of the reduction weights
function collectItemParameter( node, item, args, parameters)
	local rt = {redulist={},llvmtypes={},weight=0.0}
	for pi=1,#args do
		local redutype,redulist,weight
		if pi <= #parameters then
			redutype = parameters[ pi].type
			redulist,weight = tryGetWeightedParameterReductionList( node, redutype, args[ pi], tagmask_matchParameter, tagmask_typeConversion)
		else
			redutype = getVarargArgumentType( args[ pi].type)
			if redutype then redulist,weight = tryGetWeightedParameterReductionList( node, redutype, args[ pi], tagmask_pushVararg, tagmask_typeConversion) end
		end
		if not weight then return nil end
		if rt.weight < weight then rt.weight = weight end -- use max(a,b) as weight accumulation function
		table.insert( rt.redulist, redulist)
		local descr = typeDescriptionMap[ redutype]
		table.insert( rt.llvmtypes, descr.llvmtype)
	end
	return rt
end
-- Select the candidate items with the highest weight not exceeding maxweight
function selectCandidateItemsBestWeight( items, item_parameter_map, maxweight)
	local candidates,bestweight = {},nil
	for ii,item in ipairs(items) do
		if item_parameter_map[ ii] then -- we have a match
			local weight = item_parameter_map[ ii].weight
			if not maxweight or maxweight > weight + weightEpsilon then -- we have a candidate not looked at yet
				if not bestweight then
					candidates = {ii}
					bestweight = weight
				elseif weight < bestweight + weightEpsilon then
					if weight >= bestweight - weightEpsilon then -- they are equal
						table.insert( candidates, ii)
					else -- the new candidate is the single best match
						candidates = {ii}
						bestweight = weight
					end
				end
			end
		end
	end
	return candidates,bestweight
end
-- Get the best matching item from a list of items by weighting the matching of the arguments to the item parameter types
function selectItemsMatchParameters( node, items, args, this_constructor)
	local item_parameter_map = {}
	local bestmatch = {}
	local candidates = {}
	local bestweight = nil
	local bestgroup = 0
	for ii,item in ipairs(items) do
		local nof_params = typedb:type_nof_parameters( item)
		if nof_params == #args then
			item_parameter_map[ ii] = collectItemParameter( node, item, args, typedb:type_parameters( item))
		end
	end
	while next(item_parameter_map) ~= nil do -- until no candidate groups left
		candidates,bestweight = selectCandidateItemsBestWeight( items, item_parameter_map, bestweight)
		for _,ii in ipairs(candidates) do -- create the items from the item constructors passing
			local item_constructor = typedb:type_constructor( items[ ii])
			local call_constructor
			if not item_constructor and #args == 0 then
				call_constructor = this_constructor
			else
				local ac,success = nil,true
				local arg_constructors = {}
				for ai=1,#args do
					ac,success = tryApplyReductionList( node, item_parameter_map[ ii].redulist[ ai], args[ ai].constructor)
					if not success then break end
					table.insert( arg_constructors, ac)
				end
				if success then call_constructor = item_constructor( this_constructor, arg_constructors, item_parameter_map[ ii].llvmtypes) end
			end
			if call_constructor then table.insert( bestmatch, {type=items[ ii], constructor=call_constructor}) end
			item_parameter_map[ ii] = nil
		end
		if #bestmatch ~= 0 then return bestmatch,bestweight end
	end
end
-- Find a callable identified by name and its arguments (parameter matching) in the context of a type (this)
function findApplyCallable( node, this, callable, args)
	local resolveContextType,reductions,items = typedb:resolve_type( this.type, callable, tagmask_resolveType)
	if not resolveContextType then return nil end
	if type(resolveContextType) == "table" then
		io.stderr:write( "TRACE typedb:resolve_type\n" .. utils.getResolveTypeTrace( typedb, this.type, callable, tagmask_resolveType) .. "\n")
		utils.errorResolveType( typedb, node.line, resolveContextType, this.type, callable)
	end
	local this_constructor = applyReductionList( node, reductions, this.constructor)
	local bestmatch,bestweight = selectItemsMatchParameters( node, items, args or {}, this_constructor)
	return bestmatch,bestweight
end
-- Filter best result and report error on ambiguity
function getCallableBestMatch( node, bestmatch, bestweight, this, callable, args)
	if #bestmatch == 1 then
		return bestmatch[1]
	elseif #bestmatch == 0 then
		return nil
	else
		local altmatchstr = ""
		for ii,bm in ipairs(bestmatch) do
			if altmatchstr ~= "" then
				altmatchstr = altmatchstr .. ", "
			end
			altmatchstr = altmatchstr .. typedb:type_string(bm.type)
		end
		utils.errorMessage( node.line, "Ambiguous matches resolving callable with signature '%s', list of candidates: %s", typeDeclarationString( this, callable, args), altmatchstr)
	end
end
-- Retrieve and apply a callable in a specified context
function applyCallable( node, this, callable, args)
	local bestmatch,bestweight = findApplyCallable( node, this, callable, args)
	if not bestweight then
		io.stderr:write( "TRACE typedb:resolve_type\n" .. utils.getResolveTypeTrace( typedb, this.type, callable, tagmask_resolveType) .. "\n")
		utils.errorMessage( node.line, "Failed to find callable with signature '%s'", typeDeclarationString( this, callable, args))
	end
	local rt = getCallableBestMatch( node, bestmatch, bestweight, this, callable, args)
	if not rt then  utils.errorMessage( node.line, "Failed to match parameters to callable with signature '%s'", typeDeclarationString( this, callable, args)) end
	return rt
end
-- Try to retrieve a callable in a specified context, apply it and return its type/constructor pair if found, return nil if not
function tryApplyCallable( node, this, callable, args)
	local bestmatch,bestweight = findApplyCallable( node, this, callable, args)
	if bestweight then return getCallableBestMatch( node, bestmatch, bestweight) end
end

```

#### Constructor Functions
Here are the building blocks for defining constructors.
  * The functions ```constructorParts```, ```constructorStruct``` and ```typeConstructorPairStruct''' are helpers for constructor or type/constructor pair structures.
  * The function ```callConstructor``` builds a constructor function from a format string and additional attributes. Most of the constructor functions of this example compiler are defined with ```callConstructor```. It uses the helper function ```buildCallArguments``` to build some format string variables for mapping the arguments of a constructor call.
```Lua
-- Get the two parts of a constructor as tuple
function constructorParts( constructor)
	if type(constructor) == "table" then return constructor.out,(constructor.code or "") else return tostring(constructor),"" end
end
-- Get a constructor structure
function constructorStruct( out, code)
	return {out=out, code=code or ""}
end
-- Create a type/constructor pair as used by most functions constructing a type
function typeConstructorPairStruct( type, out, code)
	return {type=type, constructor=constructorStruct( out, code)}
end
-- Builds the argument string and the argument build-up code for a function call or interface method call constructors
function buildCallArguments( subst, thisTypeId, thisConstructor, args, types)
	local this_inp,code = constructorParts(thisConstructor or "%UNDEFINED")
	local callargstr = ""
	if types and thisTypeId and thisTypeId ~= 0 then callargstr = typeDescriptionMap[ thisTypeId].llvmtype .. " " .. this_inp .. ", " end
	if args then
		for ii=1,#args do
			local arg = args[ ii]
			local arg_inp,arg_code = constructorParts( arg)
			subst[ "arg" .. ii] = arg_inp
			if types then
				local llvmtype = types[ ii].llvmtype
				if not llvmtype then utils.errorMessage( 0, "Parameter has no LLVM type specified") end
				callargstr = callargstr .. llvmtype .. " " .. tostring(arg_inp) .. ", "
			end
			code = code .. arg_code
		end
	end
	if types or thisTypeId ~= 0 then callargstr = callargstr:sub(1, -3) end
	subst.callargstr = callargstr
	subst.this = this_inp
	return code
end
-- Constructor of a call with an self argument and some additional arguments
function callConstructor( fmt, thisTypeId, argTypeIds, vartable)
	return function( this_constructor, arg_constructors)
		env = getCallableEnvironment()
		local out = env.register()
		local subst = utils.deepCopyStruct( vartable) or {}
		subst.out = out
		local code = buildCallArguments( subst, thisTypeId or 0, this_constructor, arg_constructors, argTypeIds)
		code = code .. utils.constructor_format( fmt, subst, env.register)
		return {code=code,out=out}
	end
end


```

#### Define Function Call
The function ```defineFunctionCall``` introduced in this section is used to define a free function call or a method call as "()" operator with arguments on a callable.
A callable is an intermediate type created to unify all cases of a function call into one case. For example function variables may also be called with the arguments in oval brackets "(" ")". If we want to unify these cases, we cannot define a function as symbol with arguments, because the symbol binding is not available for a function variable. Therefore we define the function symbol to be of type callable with a "()" operator with the function parameter as arguments. The function variable can reference the callable as its type and also have a "()" operator with the function parameter as arguments.
```Lua
-- Get the parameter string of a function declaration
function getDeclarationLlvmTypeRegParameterString( descr, context)
	local rt = ""
	if context.domain == "member" then rt = rt .. typeDescriptionMap[ context.decltype].llvmtype .. "* %ths, " end
	for ai,arg in ipairs(descr.param or {}) do rt = rt .. arg.llvmtype .. " " .. arg.reg .. ", " end
	if rt ~= "" then rt = rt:sub(1, -3) end
	return rt
end

-- Get the parameter string of a function declaration
function getDeclarationLlvmTypedefParameterString( descr, context)
	local rt = ""
	if context.domain == "member" then rt = rt .. (descr.llvmthis or context.descr.llvmtype) .. "*, " end
	for ai,arg in ipairs(descr.param) do rt = rt .. arg.llvmtype .. ", " end
	if rt ~= "" then rt = rt:sub(1, -3) end
	return rt
end

-- Get (if already defined) or create the callable context type (function name) on which the "()" operator implements the function call
function getOrCreateCallableContextTypeId( contextTypeId, name, descr)
	local rt = typedb:get_type( contextTypeId, name)
	if not rt then
		rt = typedb:def_type( contextTypeId, name)
		typeDescriptionMap[ rt] = descr
	end
	return rt
end

-- Define a direct function call: class method call, free function call
function defineFunctionCall( node, descr, context)
	descr.argstr = getDeclarationLlvmTypedefParameterString( descr, context)
	descr.llvmtype = utils.template_format( llvmir.control.functionCallType, descr)
	local contextType = getDeclarationContextTypeId(context)
	local thisType = (contextType ~= 0) and referenceTypeMap[ contextType] or 0
	local callablectx = getOrCreateCallableContextTypeId( thisType, descr.name, llvmir.callableDescr)
	local constructor = callConstructor( descr.ret and llvmir.control.functionCall or llvmir.control.procedureCall, thisType, descr.param, descr)
	return defineCall( descr.ret, callablectx, "()", descr.param, constructor)
end


```

#### Const Expression Types
Constants in the source trigger the creation of const expression types. Const expression types have their own implementation of operators. 
The const expression operator constructors do not produce code, but return a value as result. 
The constructor of a const expression is the value of the data item with this type.

```Lua
constexprIntegerType = typedb:def_type( 0, "constexpr int")		-- signed integer type constant value, represented as Lua number
constexprDoubleType = typedb:def_type( 0, "constexpr double")		-- double precision floating point number constant, represented as Lua number
constexprBooleanType = typedb:def_type( 0, "constexpr bool")		-- boolean constants
constexprStructureType = typedb:def_type( 0, "constexpr struct")	-- structure initializer list
constexprStringType = typedb:def_type( 0, "constexpr string")		-- constant string in source, constructor is '@' operator plus name of global
voidType = typedb:def_type( 0, "void")					-- void type handled as no type
stringType = defineDataType( {line=0}, 0, "string", llvmir.string)	-- string constant, this example language knows strings only as constants
scalarTypeMap.void = voidType
scalarTypeMap.string = stringType

typeDescriptionMap[ constexprIntegerType] = llvmir.constexprIntegerDescr
typeDescriptionMap[ constexprDoubleType] = llvmir.constexprDoubleDescr
typeDescriptionMap[ constexprBooleanType] = llvmir.constexprBooleanDescr
typeDescriptionMap[ constexprStructureType] = llvmir.constexprStructDescr

function isScalarConstExprValueType( initType) return initType >= constexprIntegerType and initType <= stringType end

scalarTypeMap["constexpr int"] = constexprIntegerType
scalarTypeMap["constexpr double"] = constexprDoubleType
scalarTypeMap["constexpr bool"] = constexprBooleanType
scalarTypeMap["constexpr struct"] = constexprStructureType
scalarTypeMap["constexpr string"] = constexprStringType

-- Create a constexpr node from a lexem in the AST
function createConstexprValue( typeId, value)
	if typeId == constexprBooleanType then
		if value == "true" then return true else return false end
	elseif typeId == constexprIntegerType or typeId == constexprDoubleType then
		return tonumber(value)
	elseif typeId == constexprStringType then
		if not stringConstantMap[ value] then
			local encval,enclen = utils.encodeLexemLlvm(value)
			local name = utils.uniqueName( "string")
			stringConstantMap[ value] = {size=enclen+1,name=name}
			print_section( "Constants", utils.constructor_format( llvmir.control.stringConstDeclaration, {name=name, size=enclen+1, value=encval}) .. "\n")
		end
		return stringConstantMap[ value]
	end
end
-- List of value constructors from constexpr constructors
function constexprDoubleToDoubleConstructor( val) return constructorStruct( "0x" .. mewa.llvm_double_tohex( val)) end
function constexprDoubleToIntegerConstructor( val) return constructorStruct( tostring(tointeger(val))) end
function constexprDoubleToBooleanConstructor( val) if math.abs(val) < epsilonthen then return constructorStruct( "0") else constructorStruct( "1") end end
function constexprIntegerToDoubleConstructor( val) return constructorStruct( "0x" .. mewa.llvm_double_tohex( val:tonumber())) end
function constexprIntegerToIntegerConstructor( val) return constructorStruct( tostring(val)) end
function constexprIntegerToBooleanConstructor( val) if val == "0" then return constructorStruct( "0") else return constructorStruct( "1") end end
function constexprBooleanToScalarConstructor( val) if val == true then return constructorStruct( "1") else return constructorStruct( "0") end end

-- Define arithmetics of constant expressions
function defineConstExprArithmetics()
	defineCall( constexprIntegerType, constexprIntegerType, "-", {}, function(this) return -this end)
	defineCall( constexprIntegerType, constexprIntegerType, "+", {constexprIntegerType}, function(this,args) return this+args[0] end)
	defineCall( constexprIntegerType, constexprIntegerType, "-", {constexprIntegerType}, function(this,args) return this-args[0] end)
	defineCall( constexprIntegerType, constexprIntegerType, "/", {constexprIntegerType}, function(this,args) return tointeger(this/args[0]) end)
	defineCall( constexprIntegerType, constexprIntegerType, "*", {constexprIntegerType}, function(this,args) return tointeger(this*args[0]) end)
	defineCall( constexprDoubleType, constexprDoubleType, "-", {}, function(this) return -this end)
	defineCall( constexprDoubleType, constexprDoubleType, "+", {constexprDoubleType}, function(this,args) return this+args[0] end)
	defineCall( constexprDoubleType, constexprDoubleType, "-", {constexprDoubleType}, function(this,args) return this-args[0] end)
	defineCall( constexprDoubleType, constexprDoubleType, "/", {constexprDoubleType}, function(this,args) return this/args[0] end)
	defineCall( constexprDoubleType, constexprDoubleType, "*", {constexprDoubleType}, function(this,args) return this*args[0] end)

	-- Define arithmetic operators of integers with a double as promotion of the left hand integer to a double and an operator of a double with a double
	definePromoteCall( constexprDoubleType, constexprIntegerType, constexprDoubleType, "+", {constexprDoubleType},function(this) return this end)
	definePromoteCall( constexprDoubleType, constexprIntegerType, constexprDoubleType, "-", {constexprDoubleType},function(this) return this end)
	definePromoteCall( constexprDoubleType, constexprIntegerType, constexprDoubleType, "/", {constexprDoubleType},function(this) return this end)
	definePromoteCall( constexprDoubleType, constexprIntegerType, constexprDoubleType, "*", {constexprDoubleType},function(this) return this end)
end
-- Define the type conversions of const expressions to other const expression types
function defineConstExprTypeConversions()
	typedb:def_reduction( constexprBooleanType, constexprIntegerType, function( value) return value ~= "0" end, tag_typeConversion, rdw_bool)
	typedb:def_reduction( constexprBooleanType, constexprDoubleType, function( value) return math.abs(value) < math.abs(epsilon) end, tag_typeConversion, rdw_bool)
	typedb:def_reduction( constexprDoubleType, constexprIntegerType, function( value) return value:tonumber() end, tag_typeConversion, rdw_float)
	typedb:def_reduction( constexprDoubleType, constexprUIntegerType, function( value) return value:tonumber() end, tag_typeConversion, rdw_float)
	typedb:def_reduction( constexprIntegerType, constexprDoubleType, function( value) return bcd.int( tostring(value)) end, tag_typeConversion, rdw_float)
	typedb:def_reduction( constexprIntegerType, constexprUIntegerType, function( value) return value end, tag_typeConversion, rdw_sign)
	typedb:def_reduction( constexprUIntegerType, constexprIntegerType, function( value) return value end, tag_typeConversion, rdw_sign)

	local function bool2bcd( value) if value then return bcd.int("1") else return bcd.int("0") end end
	typedb:def_reduction( constexprIntegerType, constexprBooleanType, bool2bcd, tag_typeConversion, rdw_bool)
end


```
##### Remark
The functions ```mewa.llvm_double_tohex``` or ```mewa.llvm_float_tohex``` are functions provided by the _Mewa_ library to convert floating point number constants into a binary representation needed by LLVM IR for floating point numbers that have no exact binary representation of a value writen with decimals, e.g. ```0.1```. This is not an ideal situation. The _Mewa_ library should not have any API bindings to LLVM. 

#### First Class Scalar Types
This section implements the creation of the first class scalar types from their descriptions in the module llvmir_scalar.lua. The string type, or better called string pointer type in this example language, is also defined here. As for the const expression types, we also define promote calls here, like integer + float implemented as float + float after _promotion_ of the left argument.

```Lua
-- Define built-in promote calls for first class citizen scalar types
function defineBuiltInTypePromoteCalls( typnam, descr)
	local typeId = scalarTypeMap[ typnam]
	for i,promote_typnam in ipairs( descr.promote) do
		local promote_typeId = scalarTypeMap[ promote_typnam]
		local promote_descr = typeDescriptionMap[ promote_typeId]
		local promote_conv_fmt = promote_descr.conv[ typnam].fmt
		local promote_conv = promote_conv_fmt and callConstructor( promote_conv_fmt) or nil
		if promote_descr.binop then
			for operator,operator_fmt in pairs( promote_descr.binop) do
				definePromoteCall( promote_typeId, typeId, promote_typeId, operator, {promote_typeId}, promote_conv)
			end
		end
		if promote_descr.cmpop then
			for operator,operator_fmt in pairs( promote_descr.cmpop) do
				definePromoteCall( scalarBooleanType, typeId, promote_typeId, operator, {promote_typeId}, promote_conv)
			end
		end
	end
end
-- Helper functions to define binary operators of first class scalar types
function defineBuiltInBinaryOperators( typnam, descr, operators, resultTypeId)
	for opr,fmt in pairs(operators) do
		local typeId = scalarTypeMap[ typnam]
		defineCall( resultTypeId, typeId, opr, {typeId}, callConstructor( fmt))
	end
end
-- Helper functions to define binary operators of first class scalar types
function defineBuiltInUnaryOperators( typnam, descr, operators, resultTypeId)
	for opr,fmt in ipairs(operators) do
		local typeId = scalarTypeMap[ typnam]
		defineCall( resultTypeId, typeId, opr, {}, callConstructor( fmt))
	end
end
-- Constructor of a string pointer from a string definition
function stringPointerConstructor( stringdef)
	local env = getCallableEnvironment()
	local out = env.register()
	local code = utils.template_format( llvmir.control.stringConstConstructor,{size=stringdef.size,name=stringdef.name,out=out})
	return constructorStruct( out, code)
end
-- Initialize all built-in types
function initBuiltInTypes()
	-- Define the first class scalar types
	for typnam, scalar_descr in pairs( llvmir.scalar) do
		local typeId = defineDataType( {line=0}, 0, typnam, scalar_descr)
		if typnam == "int" then
			scalarIntegerType = typeId
			typedb:def_reduction( typeId, constexprIntegerType, constexprIntegerToIntegerConstructor, tag_typeInstantiation)
		elseif typnam == "double" then
			scalarDoubleType = typeId
			typedb:def_reduction( typeId, constexprDoubleType, constexprDoubleToDoubleConstructor, tag_typeInstantiation)
		elseif typnam == "bool" then
			scalarBooleanType = typeId
			typedb:def_reduction( typeId, constexprBooleanType, constexprBooleanToScalarConstructor, tag_typeInstantiation)
		end
		scalarTypeMap[ typnam] = typeId
	end
	-- Define the conversions between built-in types
	for typnam, scalar_descr in pairs( llvmir.scalar) do
		local typeId = scalarTypeMap[ typnam]
		for typnam_conv,conv in pairs(scalar_descr.conv) do
			local typeId_conv = scalarTypeMap[ typnam_conv]
			typedb:def_reduction( typeId, typeId_conv, callConstructor( conv.fmt), tag_typeConversion, conv.weight)
		end
	end
	-- Define operators
	for typnam, scalar_descr in pairs( llvmir.scalar) do
		local typeId = scalarTypeMap[ typnam]
		defineBuiltInBinaryOperators( typnam, scalar_descr, scalar_descr.binop, typeId)
		defineBuiltInBinaryOperators( typnam, scalar_descr, scalar_descr.cmpop, scalarBooleanType)
		defineBuiltInUnaryOperators( typnam, scalar_descr, scalar_descr.unop, typeId)
		defineCall( voidType, referenceTypeMap[ typeId], "=", {typeId}, callConstructor( scalar_descr.assign))
		defineCall( voidType, referenceTypeMap[ typeId], "=", {}, callConstructor( scalar_descr.assign, 0, nil, {arg1=scalar_descr.default}))
	end
	-- Define operators with promoting of the left side argument
	for typnam, scalar_descr in pairs( llvmir.scalar) do
		defineBuiltInTypePromoteCalls( typnam, scalar_descr)
	end
	-- String type
	local string_descr = typeDescriptionMap[ stringType]
	local string_refType = referenceTypeMap[ stringType]
	typedb:def_reduction( stringType, string_refType, callConstructor( string_descr.load), tag_typeInstantiation)
	typedb:def_reduction( stringType, constexprStringType, stringPointerConstructor, tag_typeInstantiation)
	defineCall( voidType, string_refType, "=", {stringType}, callConstructor( string_descr.assign))
	defineCall( voidType, string_refType, "=", {}, callConstructor( string_descr.assign, 0, nil, {arg1=string_descr.default}))
end


```

#### Variables
Here are the functions to declare variables of any kind depending on the context: local variables, global variables, member variables, function parameter, etc.
Most of the code is self explaining now.

One thing that is new is the use of the global variable ```localDefinitionContext``` as context type when defining local variables. Instead of the variable we could as well use 0. In a language implementing generics the value of localDefinitionContext can be set to an artificial type created for a generic instance because generic instances share scopes and the definition of different instances would otherwise interfere. 
```Lua
-- Define a member variable of a class or a structure
function defineMemberVariable( node, descr, context, typeId, refTypeId, name)
	local memberpos = context.structsize or 0
	local index = #context.members
	local load_ref = utils.template_format( context.descr.loadelemref, {index=index, type=descr.llvmtype})
	local load_val = utils.template_format( context.descr.loadelem, {index=index, type=descr.llvmtype})

	while math.fmod( memberpos, descr.align) ~= 0 do memberpos = memberpos + 1 end
	context.structsize = memberpos + descr.size
	table.insert( context.members, { type = typeId, name = name, descr = descr, bytepos = memberpos })
	if not context.llvmtype then context.llvmtype = descr.llvmtype else context.llvmtype = context.llvmtype  .. ", " .. descr.llvmtype end
	defineCall( refTypeId, referenceTypeMap[ context.decltype], name, {}, callConstructor( load_ref))
	defineCall( typeId, context.decltype, name, {}, callConstructor( load_val))
end
-- Define a free global variable
function defineGlobalVariable( node, descr, context, typeId, refTypeId, name, initVal)
	local out = "@" .. name
	local var = typedb:def_type( 0, name, out)
	if var == -1 then utils.errorMessage( node.line, "Duplicate definition of variable '%s'", name) end
	typedb:def_reduction( refTypeId, var, nil, tag_typeDeclaration)
	if initVal and descr.scalar == true and isScalarConstExprValueType( initVal.type) then
		local initScalarConst = constructorParts( getRequiredTypeConstructor( node, typeId, initVal, tagmask_matchParameter, tagmask_typeConversion))
		print( utils.constructor_format( descr.def_global_val, {out = out, val = initScalarConst})) -- print global data declaration
	else
		utils.errorMessage( node.line, "Only constant scalars allowed as initializer of global variables like '%s'", name)
	end
end
-- Define a free local variable
function defineLocalVariable( node, descr, context, typeId, refTypeId, name, initVal)
	local env = getCallableEnvironment()
	local out = env.register()
	local code = utils.constructor_format( descr.def_local, {out = out}, env.register)
	local var = typedb:def_type( localDefinitionContext, name, constructorStruct(out))
	if var == -1 then utils.errorMessage( node.line, "Duplicate definition of variable '%s'", name) end
	typedb:def_reduction( refTypeId, var, nil, tag_typeDeclaration)
	local decl = {type=var, constructor={code=code,out=out}}
	if type(initVal) == "function" then initVal = initVal() end
	return applyCallable( node, decl, "=", {initVal})
end
-- Define a variable (what kind is depending on the context)
function defineVariable( node, context, typeId, name, initVal)
	local descr = typeDescriptionMap[ typeId]
	local refTypeId = referenceTypeMap[ typeId]
	if not refTypeId then utils.errorMessage( node.line, "References not allowed in variable declarations, use pointer instead: %s", typedb:type_string(typeId)) end
	if context.domain == "local" then
		return defineLocalVariable( node, descr, context, typeId, refTypeId, name, initVal)
	elseif context.domain == "global" then
		return defineGlobalVariable( node, descr, context, typeId, refTypeId, name, initVal)
	elseif context.domain == "member" then
		if initVal then utils.errorMessage( node.line, "No initialization value in definition of member variable allowed") end
		defineMemberVariable( node, descr, context, typeId, refTypeId, name)
	else
		utils.errorMessage( node.line, "Internal: Context domain undefined, context=%s", mewa.tostring(context))
	end
end
-- Declare a variable implicitly that does not appear as definition in source (for example the 'self' reference in a method body).
function defineImplicitVariable( node, typeId, name, reg)
	local var = typedb:def_type( localDefinitionContext, name, reg)
	if var == -1 then utils.errorMessage( node.line, "Duplicate definition of variable '%s'", typeDeclarationString( typeId, name)) end
	typedb:def_reduction( typeId, var, nil, tag_typeDeclaration)
	return var
end
-- Make a function parameter addressable by name in the callable body
function defineParameter( node, typeId, name)
	local env = getCallableEnvironment()
	local paramreg = env.register()
	local var = typedb:def_type( localDefinitionContext, name, paramreg)
	if var == -1 then utils.errorMessage( node.line, "Duplicate definition of parameter '%s'", typeDeclarationString( localDefinitionContext, name)) end
	local descr = typeDescriptionMap[ typeId]
	local ptype = (descr.scalar or descr.class == "pointer") and typeId or referenceTypeMap[ typeId]
	if not ptype then utils.errorMessage( node.line, "Cannot use type '%s' as parameter data type", typedb:type_string(typeId)) end
	typedb:def_reduction( ptype, var, nil, tag_typeDeclaration)
	return {type=ptype, llvmtype=typeDescriptionMap[ ptype].llvmtype, reg=paramreg}
end
-- Resolve a variable by name and return its type/constructor structure
function getVariable( node, varname)
	local env = getCallableEnvironment()
	local seekctx = getSeekContextTypes()
	local resolveContextTypeId, reductions, items = typedb:resolve_type( seekctx, varname, tagmask_resolveType)
	local typeId,constructor = selectNoArgumentType( node, seekctx, varname, tagmask_resolveType, resolveContextTypeId, reductions, items)
	local variableScope = typedb:type_scope( typeId)
	if variableScope[1] == 0 or env.scope[1] <= variableScope[1] then -- the local variable is not belonging to another function
		return {type=typeId, constructor=constructor}
	else
		utils.errorMessage( node.line, "Not allowed to access variable '%s' that is not defined in local function or global scope", typedb:type_string(typeId))
	end
end


```

#### Define Data Types
This section provides the functions to declare the data types of the example language, including structures and arrays.
The data types of the example language are fairly simple because the missing need of cleanup in case of failure.

The elementwise initializer of the class type and the array type are using the function ```applyCallable``` for the member initialization. 
This provides the recursive assignments of substructures.

The ```getOrCreateArrayType``` function is a seldom case where the current scope is set manually. The scope of the array type definition is set to the scope of the element type. The current scope is set back to the original after the  function completed its job.

```Lua
-- Define a data type with all its qualifiers
function defineDataType( node, contextTypeId, typnam, descr)
	local typeId = typedb:def_type( contextTypeId, typnam)
	local refTypeId = typedb:def_type( contextTypeId, typnam .. "&")
	if typeId <= 0 or refTypeId <= 0 then utils.errorMessage( node.line, "Duplicate definition of data type '%s'", typnam) end
	referenceTypeMap[ typeId] = refTypeId
	dereferenceTypeMap[ refTypeId] = typeId
	typeDescriptionMap[ typeId] = descr
	typeDescriptionMap[ refTypeId] = llvmir.pointerDescr(descr)
	typedb:def_reduction( typeId, refTypeId, callConstructor( descr.load), tag_typeDeduction, rdw_load)
	return typeId
end
-- Structure type definition for a class
function defineStructureType( node, declContextTypeId, typnam, fmt)
	local descr = utils.template_format( fmt, {symbol=typnam})
	local typeId = defineDataType( node, declContextTypeId, typnam, descr)
	return typeId,descr
end
-- Define the assignment operator of a class as memberwise assignment of the member variables
function defineClassStructureAssignmentOperator( node, typeId)
	local descr = typeDescriptionMap[ typeId]
	local function assignElementsConstructor( this, args)
		local env = getCallableEnvironment()
		if args and #args ~= 0 and #args ~= #descr.members then
			utils.errorMessage( node.line, "Number of elements %d in init does not match number of members %d in class '%s'", #args, #descr.members, typedb:type_string( typeId))
		end
		local this_inp,code = constructorParts( this)
		for mi,member in ipairs(descr.members) do
			local out = env.register()
			local loadref = descr.loadelemref
			local llvmtype = member.descr.llvmtype
			local member_reftype = referenceTypeMap[ member.type]
			local ths = {type=member_reftype,constructor=constructorStruct(out)}
			local member_element = applyCallable( node, ths, "=", {args and args[mi] or nil})
			code = code .. utils.constructor_format(loadref,{out=out,this=this_inp,index=mi-1, type=llvmtype}) .. member_element.constructor.code
		end
		return {code=code}
	end
	local function assignStructTypeConstructor( this, args)
		return assignElementsConstructor( this, args[1].list)
	end
	defineCall( nil, referenceTypeMap[ typeId], "=", {constexprStructureType}, assignStructTypeConstructor)
	defineCall( nil, referenceTypeMap[ typeId], "=", {}, assignElementsConstructor)
end
-- Define the index access operator for arrays
function defineArrayIndexOperator( elemTypeId, arTypeId, arDescr)
	defineCall( referenceTypeMap[elemTypeId], referenceTypeMap[arTypeId], "[]", {scalarIntegerType}, callConstructor( arDescr.index[ "int"]))
end
-- Constructor for a memberwise assignment of a structure from an initializer-list (initializing an "array")
function memberwiseInitArrayConstructor( node, thisTypeId, elementTypeId, nofElements)
	return function( this, args)
		if #args > nofElements then
			utils.errorMessage( node.line, "Number of elements %d in init is too big for '%s' [%d]", #args, typedb:type_string( thisTypeId), nofElements)
		end
		local descr = typeDescriptionMap[ thisTypeId]
		local descr_element = typeDescriptionMap[ elementTypeId]
		local elementRefTypeId = referenceTypeMap[ elementTypeId] or elementTypeId
		local env = getCallableEnvironment()
		local this_inp,code = constructorParts( this)
		for ai=1,nofElements do
			local arg = (ai <= nofElements) and args[ai] or nil
			local elemreg = env.register()
			local elem = typeConstructorPairStruct( elementRefTypeId, elemreg)
			local init = tryApplyCallable( node, elem, "=", {arg})
			if not init then utils.errorMessage( node.line, "Failed to find ctor with signature '%s'", typeDeclarationString( elem, "=", {arg})) end
			local memberwise_next = utils.constructor_format( descr.memberwise_index, {index=ai-1,this=this_inp,out=elemreg}, env.register)
			code = code .. memberwise_next .. init.constructor.code
		end
		return {out=this_inp, code=code}
	end
end
-- Constructor for an assignment of a structure (initializer list) to an array
function arrayStructureAssignmentConstructor( node, thisTypeId, elementTypeId, nofElements)
	local initfunction = memberwiseInitArrayConstructor( node, thisTypeId, elementTypeId, nofElements)
	return function( this, args)
		return initfunction( this, args[1].list)
	end
end
-- Implicit on demand type definition for array
function getOrCreateArrayType( node, elementType, arraySize)
	local arrayKey = string.format( "%d[%d]", elementType, arraySize)
	if not arrayTypeMap[ arrayKey] then
		local scope_bk,step_bk = typedb:scope( typedb:type_scope( elementType)) -- define the implicit array type in the same scope as the element type
		local typnam = string.format( "[%d]", arraySize)
		local arrayDescr = llvmir.arrayDescr( typeDescriptionMap[ elementType], arraySize)
		local arrayType = defineDataType( node, elementType, typnam, arrayDescr)
		local arrayRefType = referenceTypeMap[ arrayType]
		arrayTypeMap[ arrayKey] = arrayType
		defineArrayIndexOperator( elementType, arrayType, arrayDescr)
		defineCall( voidType, arrayRefType, "=", {constexprStructureType}, arrayStructureAssignmentConstructor( node, arrayType, elementType, arraySize))
		typedb:scope( scope_bk,step_bk)
	end
	return arrayTypeMap[ arrayKey]
end

```

#### Context Types
The list of context types used for resolving types has already been explained. 
The function ```getDeclarationContextTypeId``` provides the access to the context type used for declarations depending on context.
The function ```getSeekContextTypes``` provides the access to the list context type used for ```typedb:resolve_type```.

The list of seek context types is also dependent on scope. When adding an element to a list not yet bound to the current scope then the list of the enclosing 
scope cloned calling ```thisInstanceTableClone( name, emptyInst)``` and the element is added to the clone. This comes into play when the ```self``` type is added to the list of context types. It is only visible in the methods body.

```Lua
-- Get the context type for type declarations
function getDeclarationContextTypeId( context)
	if context.domain == "local" then return localDefinitionContext
	elseif context.domain == "member" then return context.decltype
	elseif context.domain == "global" then return 0
	end
end
-- Get an object instance and clone it if it is not stored in the current scope, making it possible to add elements to an inherited instance in the current scope
function thisInstanceTableClone( name, emptyInst)
	local inst = typedb:this_instance( name)
	if not inst then
		inst = utils.deepCopyStruct( typedb:get_instance( name) or emptyInst)
		typedb:set_instance( name, inst)
	end
	return inst
end
-- Get the list of context types associated with the current scope used for resolving types
function getSeekContextTypes()
	return typedb:get_instance( seekContextKey) or {0}
end
-- Push an element to the current context type list used for resolving types
function pushSeekContextType( val)
	table.insert( thisInstanceTableClone( seekContextKey, {0}), val)
end
-- Remove the last element of the the list of context types associated with the current scope used for resolving types
function popSeekContextType( val)
	local seekctx = typedb:this_instance( seekContextKey)
	if not seekctx or seekctx[ #seekctx] ~= val then utils.errorMessage( 0, "Internal: corrupt definition context stack") end
	table.remove( seekctx, #seekctx)
end

```

#### Callable Environment
The callable environment bound to the scope of the function body has also been discussed.

To mention is the function ```expandMethodEnvironment``` that declares the ```self``` variable in a method body (explicit and implicit).
```Lua
-- Create a callable environent object
function createCallableEnvironment( node, name, rtype, rprefix, lprefix)
	if rtype then
		local descr = typeDescriptionMap[ rtype]
		if not descr.scalar and not descr.class == "pointer" then
			utils.errorMessage( node.line, "Only scalar types can be return values")
		end
	end
	return {name=name, line=node.line, scope=typedb:scope(),
		register=utils.register_allocator(rprefix), label=utils.label_allocator(lprefix), returntype=rtype}
end
-- Attach a newly created data structure for a callable to its scope
function defineCallableEnvironment( node, name, rtype)
	local env = createCallableEnvironment( node, name, rtype)
	if typedb:this_instance( callableEnvKey) then utils.errorMessage( node.line, "Internal: Callable environment defined twice: %s %s", name, mewa.tostring({(typedb:scope())})) end
	typedb:set_instance( callableEnvKey, env)
	return env
end
-- Get the active callable instance
function getCallableEnvironment()
	return typedb:get_instance( callableEnvKey)
end
-- Set some variables needed in a class method implementation body
function expandMethodEnvironment( node, context, descr, env)
	local selfTypeId = referenceTypeMap[ context.decltype]
	local classvar = defineImplicitVariable( node, selfTypeId, "self", "%ths")
	pushSeekContextType( {type=classvar, constructor={out="%ths"}})
end

```

#### Control Boolean Types
Here we have a complete definition of the control boolean types as introduced in the tutorial. The function definition has been taken one to one from the example **language1**. I think after our journey through the example, the code explains itself.

```Lua
-- Initialize control boolean types used for implementing control structures like 'if','while' and operators on booleans like '&&','||'
function initControlBooleanTypes()
	controlTrueType = typedb:def_type( 0, " controlTrueType")
	controlFalseType = typedb:def_type( 0, " controlFalseType")

	local function controlTrueTypeToBoolean( constructor)
		local env = getCallableEnvironment()
		local out = env.register()
		return {code = constructor.code .. utils.constructor_format(llvmir.control.controlTrueTypeToBoolean,{falseExit=constructor.out,out=out},env.label),out=out}
	end
	local function controlFalseTypeToBoolean( constructor)
		local env = getCallableEnvironment()
		local out = env.register()
		return {code = constructor.code .. utils.constructor_format( llvmir.control.controlFalseTypeToBoolean,{trueExit=constructor.out,out=out},env.label),out=out}
	end
	typedb:def_reduction( scalarBooleanType, controlTrueType, controlTrueTypeToBoolean, tag_typeDeduction, rwd_control)
	typedb:def_reduction( scalarBooleanType, controlFalseType, controlFalseTypeToBoolean, tag_typeDeduction, rwd_control)

	local function booleanToControlTrueType( constructor)
		local env = getCallableEnvironment()
		local out = env.label()
		return {code = constructor.code .. utils.constructor_format( llvmir.control.booleanToControlTrueType, {inp=constructor.out, out=out}, env.label),out=out}
	end
	local function booleanToControlFalseType( constructor)
		local env = getCallableEnvironment()
		local out = env.label()
		return {code = constructor.code .. utils.constructor_format( llvmir.control.booleanToControlFalseType, {inp=constructor.out, out=out}, env.label),out=out}
	end

	typedb:def_reduction( controlTrueType, scalarBooleanType, booleanToControlTrueType, tag_typeDeduction, rwd_control)
	typedb:def_reduction( controlFalseType, scalarBooleanType, booleanToControlFalseType, tag_typeDeduction, rwd_control)

	local function negateControlTrueType( this) return {type=controlFalseType, constructor=this.constructor} end
	local function negateControlFalseType( this) return {type=controlTrueType, constructor=this.constructor} end

	local function joinControlTrueTypeWithBool( this, arg)
		local out = this.out
		local code2 = utils.constructor_format( llvmir.control.booleanToControlTrueType, {inp=arg[1].out, out=out}, getCallableEnvironment().label)
		return {code=this.code .. arg[1].code .. code2, out=out}
	end
	local function joinControlFalseTypeWithBool( this, arg)
		local out = this.out
		local code2 = utils.constructor_format( llvmir.control.booleanToControlFalseType, {inp=arg[1].out, out=out}, getCallableEnvironment().label)
		return {code=this.code .. arg[1].code .. code2, out=out}
	end
	defineCall( controlTrueType, controlFalseType, "!", {}, nil)
	defineCall( controlFalseType, controlTrueType, "!", {}, nil)
	defineCall( controlTrueType, controlTrueType, "&&", {scalarBooleanType}, joinControlTrueTypeWithBool)
	defineCall( controlFalseType, controlFalseType, "||", {scalarBooleanType}, joinControlFalseTypeWithBool)

	local function joinControlFalseTypeWithConstexprBool( this, arg)
		if arg == false then
			return this
		else
			local env = getCallableEnvironment()
			return {code= this.code .. utils.constructor_format( llvmir.control.terminateTrueExit,{out=this.out},env.label), out=this.out}
		end
	end
	local function joinControlTrueTypeWithConstexprBool( this, arg)
		if arg == true then
			return this
		else
			local env = getCallableEnvironment()
			return {code= this.code .. utils.constructor_format( llvmir.control.terminateFalseExit,{out=this.out},env.label), out=this.out}
		end
	end
	defineCall( controlTrueType, controlTrueType, "&&", {constexprBooleanType}, joinControlTrueTypeWithConstexprBool)
	defineCall( controlFalseType, controlFalseType, "||", {constexprBooleanType}, joinControlFalseTypeWithConstexprBool)

	local function constexprBooleanToControlTrueType( value)
		local env = getCallableEnvironment()
		local out = label()
		local code = (value == true) and "" or utils.constructor_format( llvmir.control.terminateFalseExit, {out=out}, env.label)
		return {code=code, out=out}
	end
	local function constexprBooleanToControlFalseType( value)
		local env = getCallableEnvironment()
		local out = label()
		local code = (value == false) and "" or utils.constructor_format( llvmir.control.terminateFalseExit, {out=out}, env.label)
		return {code=code, out=out}
	end
	typedb:def_reduction( controlFalseType, constexprBooleanType, constexprBooleanToControlFalseType, tag_typeDeduction, rwd_control)
	typedb:def_reduction( controlTrueType, constexprBooleanType, constexprBooleanToControlTrueType, tag_typeDeduction, rwd_control)

	local function invertControlBooleanType( this)
		local out = getCallableEnvironment().label()
		return {code = this.code .. utils.constructor_format( llvmir.control.invertedControlType, {inp=this.out, out=out}), out = out}
	end
	typedb:def_reduction( controlFalseType, controlTrueType, invertControlBooleanType, tag_typeDeduction, rwd_control)
	typedb:def_reduction( controlTrueType, controlFalseType, invertControlBooleanType, tag_typeDeduction, rwd_control)

	definePromoteCall( controlTrueType, constexprBooleanType, controlTrueType, "&&", {scalarBooleanType}, constexprBooleanToControlTrueType)
	definePromoteCall( controlFalseType, constexprBooleanType, controlFalseType, "||", {scalarBooleanType}, constexprBooleanToControlFalseType)
end


```

#### Control Structures
Finally we visit the function ```conditionalIfElseBlock( node, condition, matchblk, elseblk, exitLabel)``` that is doing the work for the _AST_ node functions implementing the _if_ with or without the _else_. If we have an _else_ then we have to branch to the end and to bind the unbound label of the control true type. This is equivalent to turning the control true type to a control false type before adding the else block. 

```Lua
-- Build a conditional if/elseif block
function conditionalIfElseBlock( node, condition, matchblk, elseblk, exitLabel)
	local cond_constructor = getRequiredTypeConstructor( node, controlTrueType, condition, tagmask_matchParameter, tagmask_typeConversion)
	if not cond_constructor then utils.errorMessage( node.line, "Can't use type '%s' as a condition", typedb:type_string(condition.type)) end
	local code = cond_constructor.code .. matchblk.code
	if exitLabel then
		code = code .. utils.template_format( llvmir.control.invertedControlType, {inp=cond_constructor.out, out=exitLabel})
	else
		code = code .. utils.template_format( llvmir.control.label, {inp=cond_constructor.out})
	end
	local out
	if elseblk then
		code = code .. elseblk.code
		out = elseblk.out
	else
		out = cond_constructor.out
	end
	return {code = code, out = out}
end

```

### Build and Run the Compiler
Finally or maybe with a quick jump to the end we got here. Now we build and run our compiler on the example source presented at the beginning.

##### Translate the grammar and build the Lua script of the compiler
```bash
cd examples
mkdir build

LUABIN=`which $ARG`
. tutorial/luaenv.sh

mewa -b "$LUABIN" -g -o build/tutorial.compiler.lua tutorial/grammar.g
chmod +x build/tutorial.compiler.lua
```

##### Run the compiler front-end on a test program
```bash
build/tutorial.compiler.lua -o build/tutorial.program.llr tutorial/program.prg
```

##### Inspect the LLVM IR code
```bash
less build/tutorial.program.llr 
```

##### Create an object file
```bash
llc -relocation-model=pic -O=3 -filetype=obj build/tutorial.program.llr -o build/tutorial.program.o
```

##### Create an executable
```bash
clang -lm -lstdc++ -o build/tutorial.program build/tutorial.program.o
```

##### Run the executable file
```bash
build/tutorial.program
```

#### Output
```
Salary sum: 280720.00
```

### What is Missing
This article showed the implementation of a primitive language missing lots of features. For example:
  * Constructors implementing late binding, e.g. for implementing copy elision. In the example **language1** and in the [glossary](glossary.md) they are called unbound reference types.
  * More type qualifiers like const, private, etc.
  * Pointers
  * Dynamic memory allocation
  * Exceptions
  * Generics
  * Lambdas
  * Coroutines

And many more. You can visit the [FAQ](faq.md) to get an idea, how to implement a richer language.
You can dig into the main [example language1](example_language1.md) that implements many features missing here as example.

## Conclusion
We have seen how a very primitive compiler is written in _Lua_ using _Mewa_. 
We could compile and run a simple demo program in the shell.

I hope I could give you some overview about one way to write a compiler front-end.
Learning about another approach can be interesting, even if you go along another path.

_Mewa_ is based on very old ideas and offers nothing new. 
But it is a pragmatic approach that brings the implementation of a prototype of a non trivial compiler front-end for a single person within reach.





