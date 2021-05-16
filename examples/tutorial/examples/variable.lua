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

