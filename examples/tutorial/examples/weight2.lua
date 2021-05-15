mewa = require "mewa"
typedb = mewa.typedb()

-- Centralized list of the ordering of the reduction rules determined by their weights:
local weight_conv=	1.0		-- weight of a conversion
local weight_const_1=	0.5 / (1*1)	-- make const of a type having one qualifier
local weight_const_2=	0.5 / (2*2)	-- make const of a type having two qualifiers
local weight_const_3=	0.5 / (3*3)	-- make const of a type having three qualifiers
local weight_ref_1=	0.25 / (1*1)	-- strip reference of a type having one qualifier
local weight_ref_2=	0.25 / (2*2)	-- strip reference of a type having two qualifiers
local weight_ref_3=	0.25 / (3*3)	-- strip reference of a type having three qualifiers

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
	typedb:def_reduction( valTypeId, refTypeId, function( arg) return "load ( " .. arg .. ")"  end, 1, weight_ref_1)
	typedb:def_reduction( c_valTypeId, c_refTypeId, function( arg) return "load ( " .. arg .. ")"  end, 1, weight_ref_2)
	typedb:def_reduction( c_valTypeId, valTypeId, function( arg) return "make const ( " .. arg .. ")"  end, 1, weight_const_1)
	typedb:def_reduction( c_refTypeId, refTypeId, function( arg) return "make const ( " .. arg .. ")"  end, 1, weight_const_2)
	return {val=valTypeId, ref=refTypeId, c_val=c_valTypeId, c_ref=c_refTypeId}
end

local qualitype_int = defineType( "int")
local qualitype_long = defineType( "long")

typedb:def_reduction( qualitype_int.val, qualitype_long.val, function( arg) return "convert int ( " .. arg .. ")"  end, 1, weight_conv)
typedb:def_reduction( qualitype_int.c_val, qualitype_long.c_val, function( arg) return "convert const int ( " .. arg .. ")"  end, 1, weight_conv)
typedb:def_reduction( qualitype_long.val, qualitype_int.val, function( arg) return "convert to long ( " .. arg .. ")"  end, 1, weight_conv)
typedb:def_reduction( qualitype_long.c_val, qualitype_int.c_val, function( arg) return "convert to const long ( " .. arg .. ")"  end, 1, weight_conv)

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

