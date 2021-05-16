# Writing Compiler Front-Ends for LLVM with Lua using Mewa

## Lead Text
LLVM IR text representation makes it possible to write a compiler front-end without being bound to an API. We can map the source text to the text of LLVM IR and use the tools of the clang compiler for further compilation steps. This opens the doors to implement compiler front-ends in different ways. Mewa tries to optimize the amount of code written. It targets single authors that would like to write a prototype of a non-trivial programming language fast, but at the expense of a structure supporting collaborative work. This makes it rather a tool for experiment than for building a product.

## Introduction

The Mewa compiler-compiler is a tool to script compiler front-ends in Lua. A compiler written with Mewa takes a source file as input and prints an intermediate representation as input for the next compilation step. The example in this tutorial uses the text format of LLVM IR as output. An executable is built using the ```llc``` command of _Clang_.

Mewa provides no support for the evaluation of different paths of code generation. The idea is to do a one-to-one mapping of program structures to code and to leave all analytical optimization steps to the backend.

For implementing a compiler with Mewa, you define a grammar attributed with Lua function calls.
The program ```mewa``` will generate a Lua script that will transform any source passing the grammar specified into an AST with the Lua function calls attached to the nodes. The compiler will call the functions of the top level nodes of the AST that take the control of the further processing of the compiler front-end. The functions called with their AST node as argument invoke the further tree traversal. The types and data structures of the program are built and the output is printed in the process.


## Goals

This tutorial starts with some self-contained examples of using the Lua library of Mewa to build the type system of your programming language. Self-contained means that nothing is used but the library. The examples are also not dependent on each other. This allows you to continue reading and return to the snippets you did not
understand completely later.

After this, the tutorial will guide you through the implementation of a compiler for a reduced and incomplete programming language.
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
We will compile this program with the tutorial compiler and run it in a shell. We will also look at different parts including the grammar of the language.
Finally, I will talk about the features missing in the language to give you some outlook on how to implement a compiler of a complete programming language.

## Target Audience

To understand this article some knowledge about formal languages and parser generators is helpful. To understand the examples you should be familiar with some scripting languages that have a concept of closures similar to Lua. If you have read a tutorial about LLVM IR, you will get grip on the code generation in the examples.

## Deeper Digging

For a deeper digging you have to look at the Mewa project itself and the implementation of the main example language, a strongly typed multiparadigm programming language with structures, classes, interfaces, free functions, generics, and exceptions. There exists an [FAQ](faq.md) that also tries to answer problem-solving questions.


## Tutorial, Part 1 

### Declaring a variable

Let's start with an example that is complicated but a substantial step forward. 
We print the LLVM code needed to assign a variable value to another variable. 
For this, we need to introduce **types** and **reductions**.

#### Introduction
##### Types
Types are items represented as integers. They are declared and retrieved by a string, the name of the type, and a context type.
The context type is a type declared before or 0.
Global free variables have for example no associated context type and are declared with 0 as context type. 
Types are associated with a constructor. A constructor is a value, structure or a function that describes the construction of the type.
Optionally, types are associated with arguments. Arguments are list of type/constructor pairs. Types are declared with 
```Lua
	typeid = typedb:def_type( contextType, name, constructor, parameter)
```
The ```typeid``` returned is the integer that represents the type for the typedb.
##### Reductions
Reductions are paths to derive a type from another. You can imagine the typesystem as a directed graph of vertices (types) and edges (reductions).
We will introduce some concepts that allow a partial view on this graph later. For now, imagine it as a graph.
Reductions have also an associated constructor. The constructor describes the construction of the type in the direction of the reduction from its source.
Here is an example:
```Lua
	typedb:def_reduction( destType, srcType, constructor, 1)
```
The 1 as parameter is an integer value we will explain later.
##### Resolve Type
Types can be resolved by their name and a context type having a path of reductions to the context type declaration.
##### Derive type
Types can be constructed by querying a reduction path from one type to another and constructing the type from the source type constructor
by applying the list of constructors on this path.

Let's have a look at the example:

#### Source
File examples/variable.lua
```lua
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
The first match of the operator was our candidate match chosen as result. But selecting a match by other criteria is imaginable.
We declared a variable with a name, a concept of scope is missing here. We will look at scopes in the next example.


### The Concept of Scope

Now let's see how scope is represented in the typedb.

#### Introduction
Scope in Mewa is represented as a pair of non negative integer numbers. The first number is the start, the first scope step that belongs to the scope, the second number the end, the first scope step that is not part of the scope. Scope steps are generated by a counter with increments declared in the grammar of your language parser.
All declarations in the typedb are bound to a scope, all queryies of the typedb are bound to the current scope step. A declaration is considered to contribute to the result if its scope is covering the scope step of the query.

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
```lua
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
The function used for the AST tree traversal sets the current scope step and scope. This works for most of the cases. Sometimes you have to set the scope manually in nodes that implement declarations of different scopes, like for example function declarations with a function body in an own scope.


### The Weight of Reductions

Now we have to look again at something that is a little bit puzzling. We have to assign weights to reductions. The problem is that even trivial examples of type queryies lead to ambiguity if we do not introduce a weighting scheme that memorizes a preference. Real ambiguity is something we want to detect as an error.
I came to the conclusion that it is the best to declare all reduction weights at one central place in the source and to document it well.

Let's have a look at an example without weighting of reductions that will lead to an ambiguity. 

#### Failing example
File examples/weight1.lua
```lua
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


### Tags of Reductions

Unfortunately, the display of the typesystem as one graph is not enough for all cases. There are type derivation paths that are fitting in one case and undesirable in other cases. The following example declares an object of a class derived from a base class that calls a constructor function with no arguments. Unfortunately the constructor function is only declared for the base class. But when calling an object constructor an error should be reported if it does not exist for the class. The same behaviour as for a method call is bad in this case.

Let's first look at an example failing:

#### Failing example
File examples/tags1.lua
```lua
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
We explained now the 4th parameter of the typedb:def_reduction defined as ```1``` in the first examples. It is the mandatory tag assigned to the reduction.
The command ```typedb.reduction_tagmask``` is used to declare sets of tags selected for resolving and deriving types.


### Objects with Scope

In a compiler we have building blocks that are bound to a scope. For example functions. These building blocks are best represented as objects. If we are in the depht of an AST tree traversal we would like to have a way to address these objects without complicated structures passed down as parameters. This would be very error prone. Especially in a non stricly typed language as Lua. 

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
```lua
mewa = require "mewa"
typedb = mewa.typedb()

-- A register allocator as a generator function counting from 1, returning the LLVM register identifiers:
function register_allocator()
	local i = 0
	return function ()
		i = i + 1
		return "%R" .. i
	end
end

-- Attach a data structure for a callable to its scope
function defineCallableEnvironment( name)
	local env = {name=name, scope=(typedb:scope()), register=register_allocator()}
	if typedb:this_instance( "callenv") then error( "Callable environment defined twice") end
	typedb:set_instance( "callenv", env)
	return env
end
-- Get the active callable instance
function getCallableEnvironment()
	return typedb:get_instance( "callenv")
end

typedb:scope( {1,100} )
defineCallableEnvironment( "function X")
typedb:scope( {20,30} )
defineCallableEnvironment( "function Y")
typedb:scope( {70,90} )
defineCallableEnvironment( "function Z")

typedb:step( 10)
print( "We are in " .. typedb:get_instance( "callenv").name)
typedb:step( 25)
print( "We are in " .. typedb:get_instance( "callenv").name)
typedb:step( 85)
print( "We are in " .. typedb:get_instance( "callenv").name)


```
#### Output
```
We are in function X
We are in function Y
We are in function Z

```
#### Conclusion
The possibility of attaching named objects to a scope is a big deal for the readability, an important property for prototyping.
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
The IF takes the condition argument and transforms it into a control true type. The code of the resulting constructor is joined with the constructor of the code block to evaluate in the case the condition is true. At the end the unbound label is declared at the end of the code.

#### Source
```lua
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
Control structures are implemented by constructing the control boolean type required. Boolean operators as the logical and or the logical or are constructed by wiring control boolean types together. This has not been done in this example, but it is imaginable after constructing an IF. The construction of types with reduction rules does not stop here. Control structures are not a entirely different animal.

I think we are now ready to look at our example compiler as a whole as we have introduced the whole variety of functions of the typedb library.





