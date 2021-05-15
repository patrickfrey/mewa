mewa = require "mewa"
typedb = mewa.typedb()

-- [1] Define some helper functions
-- Map template for LLVM Code synthesis
function constructor_format( fmt, argtable)
	return (fmt:gsub("[{]([_%d%w]*)[}]", function( match) return argtable[ match] or "" end))
end

-- A register allocator as a function counting from 1, returning the LLVM register identifiers:
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

-- [2] Define an integer type with assignment
do
local register = register_allocator()
local intValType = typedb:def_type( 0, "int")
local intRefType = typedb:def_type( 0, "int&")
typedb:def_reduction( intValType, intRefType,
	function(this)
		local out = register()
		local code = constructor_format( "{out} = load i32, i32* {this} ; reduction int <- int&\n", {out=out,this=this.out})
		return {out=out, code=code}
	end, 1)
typedb:def_type( intRefType, "=",
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

-- [4] Define a variable "b" without initialization:

io.stdout:write( "; SOURCE int b = a;\n")
local register_b = register()
local variable_b = typedb:def_type( 0, "b", {out=register_b})
typedb:def_reduction( intRefType, variable_b, nil, 1)

io.stdout:write( constructor_format( "{out} = alloca i32, align 4 ; allocation of 'b'\n", {out=register_b}))

-- [5] Assign the value to "a" to "b":
-- [5.1] Resolve the operator "a = .."
local resolveTypeId, reductions, items = typedb:resolve_type( variable_b, "=")
if not resolveTypeId then error( "Not found") elseif type( resolveTypeId) == "table" then error( "Ambiguous") end

-- [5.2] For all candidates of "b = ..", get the first one with one parameter and match the argument to this parameter
local constructor = typedb:type_constructor( variable_b)
for _,redu in ipairs(reductions or {}) do constructor = applyConstructor( constructor) end
for _,item in ipairs(items) do
	local parameters = typedb:type_parameters( item)
	if #parameters == 1 then
		local reductions,weight,altpath = typedb:derive_type( parameters[1].type, variable_a)
		if not weight then error( "Failed to match parameter") elseif altpath then error( "Ambiguous parameter") end

		-- [5.3] Synthesize the code for "b = a;"
		local parameter_constructor = typedb:type_constructor( variable_a)
		for _,redu in ipairs(reductions or {}) do
			parameter_constructor = applyConstructor( redu.constructor, parameter_constructor)
		end
		constructor = typedb:type_constructor(item)( constructor, {parameter_constructor})
	end
end
-- [5.4] Print the code of "b = a;"
io.stdout:write( constructor.code)

end

