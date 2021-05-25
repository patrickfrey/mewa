mewa = require "mewa"
typedb = mewa.typedb()

-- Define some helper functions, that we discussed already before

function constructor_format( fmt, argtable)
	return (fmt:gsub("[{]([_%d%w]*)[}]",
		function( match) return argtable[ match] or "" end))
end

function register_allocator()
	local i = 0
	return function ()
		i = i + 1
		return "%R" .. i
	end
end

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

-- Define some format strings for the LLVM code
local fmt_load = "{out} = load %{name}, %{name}* {this}\n"
local fmt_member = "{out} = getelementptr inbounds %{name}, "
			.. "%{name}* {this}, i32 0, i32 {index}\n"
local fmt_init = "call void @__ctor_init_{name}( %{name}* {this})\n"

-- Define a type with variations having different qualifiers
function defineType( name)
	local valTypeId = typedb:def_type( 0, name)
	local refTypeId = typedb:def_type( 0, name .. "&")
	typedb:def_reduction( valTypeId, refTypeId,
		function(this)
			local out = register()
			local code = constructor_format( fmt_load, {name=name,out=out,this=this.out})
			return {out=out, code=code}
		end, 1)
	return {val=valTypeId, ref=refTypeId}
end

local register = register_allocator()

-- Constructor for loading a member reference
function loadMemberConstructor( classname, index)
	return function( this)
		local out = register()
		local subst = {out=out,this=this.out,name=classname,index=index}
		local code = (this.code or "") .. constructor_format( fmt_member, subst)
		return {code=code, out=out}
	end
end
-- Constructor for calling the default ctor, assuming that it exists
function callDefaultCtorConstructor( classname)
	return function(this)
		local code = (this.code or "") .. (arg[1].code or "")
			.. constructor_format( fmt_init, {this=this.out,name=classname})
		return {code=code, out=out}
	end
end

local t_baseclass = defineType("baseclass")
local t_class = defineType("class")

-- Define the default ctor of baseclass
local ctor = callDefaultCtorConstructor("baseclass")
local constructor_baseclass = typedb:def_type( t_baseclass.ref, "constructor", ctor)

-- Define the inheritance of class from baseclass
local load_baseclass = loadMemberConstructor( "class", 1)
typedb:def_reduction( t_baseclass.ref, t_class.ref, load_baseclass, 1)

-- Search for the constructor of class (that does not exist)
local resolveTypeId, reductions, items
	= typedb:resolve_type( t_class.ref, "constructor")
if not resolveTypeId then
	print( "Not found")
elseif type( resolveTypeId) == "table" then
	print( "Ambiguous")
end

for _,item in ipairs(items or {}) do
	print( "Found " .. typedb:type_string( item) .. "\n")
end

