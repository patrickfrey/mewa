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
