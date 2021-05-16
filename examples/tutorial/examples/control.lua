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


