mewa = require "mewa"
typedb = mewa.typedb()

-- Centralized list of the ordering of the reduction rule weights:
local weight_conv=	1.0		-- weight of a conversion
local weight_const_1=	0.5 / (1*1)	-- make const of type with 1 qualifier
local weight_const_2=	0.5 / (2*2)	-- make const of type with 2 qualifiers
local weight_const_3=	0.5 / (3*3)	-- make const of type with 3 qualifiers
local weight_ref_1=	0.25 / (1*1)	-- strip reference of type with 1 qualifier
local weight_ref_2=	0.25 / (2*2)	-- strip reference of type with 2 qualifiers
local weight_ref_3=	0.25 / (3*3)	-- strip reference of type with 3 qualifiers

-- Build a constructor
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

-- Type list as string
function typeListString( tpList, separator)
	if not tpList or #tpList == 0 then return "" end
	local tp = type(tpList[ 1]) == "table" and tpList[ 1].type or tpList[ 1]
	local rt = typedb:type_string( tp)
	for ii=2,#tpList do
		local tp = type(tpList[ ii]) == "table" and tpList[ ii].type or tpList[ ii]
		rt = rt .. separator .. typedb:type_string( tp)
	end
	return rt
end

-- Define a type with all its variations having different qualifiers
function defineType( name)
	local valTypeId = typedb:def_type( 0, name)
	local refTypeId = typedb:def_type( 0, name .. "&")
	local c_valTypeId = typedb:def_type( 0, "const " .. name)
	local c_refTypeId = typedb:def_type( 0, "const " .. name .. "&")
	typedb:def_reduction( valTypeId, refTypeId,
		function( arg) return "load ( " .. arg .. ")"  end, 1, weight_ref_2)
	typedb:def_reduction( c_valTypeId, c_refTypeId,
		function( arg) return "load ( " .. arg .. ")"  end, 1, weight_ref_1)
	typedb:def_reduction( c_valTypeId, valTypeId,
		function( arg) return "make const ( " .. arg .. ")"  end, 1, weight_const_1)
	typedb:def_reduction( c_refTypeId, refTypeId,
		function( arg) return "make const ( " .. arg .. ")"  end, 1, weight_const_2)
	return {val=valTypeId, ref=refTypeId, c_val=c_valTypeId, c_ref=c_refTypeId}
end

local t_int = defineType( "int")
local t_long = defineType( "long")

typedb:def_reduction( t_int.val, t_long.val,
	function( arg) return "convert int ( " .. arg .. ")"  end, 1, weight_conv)
typedb:def_reduction( t_int.c_val, t_long.c_val,
	function( arg) return "convert const int ( " .. arg .. ")"  end, 1, weight_conv)
typedb:def_reduction( t_long.val, t_int.val,
	function( arg) return "convert long ( " .. arg .. ")"  end, 1, weight_conv)
typedb:def_reduction( t_long.c_val, t_int.c_val,
	function( arg) return "convert const long ( " .. arg .. ")" end, 1, weight_conv)

local reductions,weight,altpath = typedb:derive_type( t_int.c_val, t_long.ref)
if not weight then
	print( "Not found!")
elseif altpath then
	print( "Ambiguous: "
		.. typeListString( reductions, " -> ")
		.. " | " .. typeListString( altpath, " -> "))
else
	local constructor = typedb:type_name( t_long.ref)
	for _,redu in ipairs(reductions or {}) do
		constructor = applyConstructor( redu.constructor, constructor)
	end
	print( constructor)
end
