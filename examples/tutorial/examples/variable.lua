mewa = require "mewa"
typedb = mewa.typedb()

-- [1] Define some helper functions
-- Map template for LLVM IR Code synthesis
--   substitute identifiers in curly brackets '{' '}' with the values in a table
function constructor_format( fmt, argtable)
	return (fmt:gsub("[{]([_%d%w]*)[}]",
		function( match) return argtable[ match] or "" end))
end

-- A register allocator as a generator function counting from 1,
-- returning the LLVM register identifiers:
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
		if (type(constructor) == "function") then
			return constructor( this, arg)
		else
			return constructor
		end
	else
		return this
	end
end

-- [2] Define the LLVM format strings
local fmt_def_local = "{out} = alloca i32, align 4 ; variable allocation\n"
local fmt_load = "{out} = load i32, i32* {this} ; reduction int <- int&\n"
local fmt_assign = "store i32 {arg1}, i32* {this} ; variable assignment\n"

-- [3] Define an integer type with assignment operator
do
local register = register_allocator()
local intValType = typedb:def_type( 0, "int")	-- integer value type
local intRefType = typedb:def_type( 0, "int&")	-- integer reference type
typedb:def_reduction( intValType, intRefType,	-- get value from reference type
	function(this)
		local out = register()
		local code = constructor_format( fmt_load, {out=out,this=this.out})
		return {out=out, code=code}
	end, 1)
typedb:def_type( intRefType, "=",		-- assignment operator
	function( this, arg)
		local code = (this.code or "") .. (arg[1].code or "")
			.. constructor_format( fmt_assign, {this=this.out,arg1=arg[1].out})
		return {code=code, out=this.out}
	end, {intValType})

-- [4] Define a variable "a" initialized with 1:
local register_a = register()
local variable_a = typedb:def_type( 0, "a", {out=register_a})
typedb:def_reduction( intRefType, variable_a, nil, 1)
io.stdout:write( "; SOURCE int a = 1;\n")
io.stdout:write( constructor_format( fmt_def_local, {out=register_a}))
io.stdout:write( constructor_format( fmt_assign, {this=register_a,arg1=1}))

-- [5] Define a variable b and assign the value of a to it:
io.stdout:write( "; SOURCE int b = a;\n")

-- [5.1] Define a variable "b":
local register_b = register()
local variable_b = typedb:def_type( 0, "b", {out=register_b})
typedb:def_reduction( intRefType, variable_b, nil, 1)

io.stdout:write( constructor_format( fmt_def_local, {out=register_b}))

-- [5.2] Assign the value of "a" to "b":
-- [5.2.1] Resolve the operator "b = .."
local resolveTypeId, reductions, items = typedb:resolve_type( variable_b, "=")
if not resolveTypeId then
	error( "Not found")
elseif type( resolveTypeId) == "table" then
	error( "Ambiguous")
end

-- [5.2.2] For all candidates of "b = ..", get the first one with one parameter
--   and match the argument to this parameter
local constructor = typedb:type_constructor( variable_b)
for _,redu in ipairs(reductions or {}) do
	constructor = applyConstructor( redu.constructor, constructor)
end
for _,item in ipairs(items) do
	local parameters = typedb:type_parameters( item)
	if #parameters == 1 then
		local reductions,weight,altpath
			= typedb:derive_type( parameters[1].type, variable_a)
		if altpath then error( "Ambiguous parameter") end
		if weight then
			-- [5.3] Synthesize the code for "b = a;"
			local param_constructor = typedb:type_constructor( variable_a)
			for _,redu in ipairs(reductions or {}) do
				param_constructor = applyConstructor( redu.constructor, param_constructor)
			end
			constructor = typedb:type_constructor(item)( constructor, {param_constructor})
			break
		end
	end
end
-- [5.3] Print the code of "b = a;"
io.stdout:write( constructor.code)

end
