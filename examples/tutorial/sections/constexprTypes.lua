constexprIntegerType = typedb:def_type( 0, "constexpr int")		-- signed integer type constant value, represented as Lua number
constexprFloatType = typedb:def_type( 0, "constexpr float")		-- single precision floating point number constant, represented as Lua number
constexprBoolType = typedb:def_type( 0, "constexpr bool")		-- boolean constants
constexprStructureType = typedb:def_type( 0, "constexpr struct")	-- structure initializer list

typeDescriptionMap[ constexprIntegerType] = llvmir.constexprIntegerDescr;
typeDescriptionMap[ constexprFloatType] = llvmir.constexprFloatDescr;
typeDescriptionMap[ constexprBoolType] = llvmir.constexprBooleanDescr;
typeDescriptionMap[ stringType] = llvmir.constexprStringDescr;
typeDescriptionMap[ constexprStructureType] = llvmir.constexprStructDescr;

scalarTypeMap["constexpr int"] = constexprIntegerType
scalarTypeMap["constexpr float"] = constexprFloatType
scalarTypeMap["constexpr bool"] = constexprBoolType
scalarTypeMap["constexpr struct"] = constexprStructureType

-- Define arithmetics of constant expressions
function defineConstExprArithmetics()
	defineCall( constexprIntegerType, constexprIntegerType, "-", {}, function(this) return -this end)
	defineCall( constexprIntegerType, constexprIntegerType, "+", {constexprIntegerType}, function(this,args) return this+args[0] end)
	defineCall( constexprIntegerType, constexprIntegerType, "-", {constexprIntegerType}, function(this,args) return this-args[0] end)
	defineCall( constexprIntegerType, constexprIntegerType, "/", {constexprIntegerType}, function(this,args) return tointeger(this/args[0]) end)
	defineCall( constexprIntegerType, constexprIntegerType, "*", {constexprIntegerType}, function(this,args) return tointeger(this*args[0]) end)
	defineCall( constexprFloatType, constexprFloatType, "-", {}, function(this) return -this end)
	defineCall( constexprFloatType, constexprFloatType, "+", {constexprFloatType}, function(this,args) return this+args[0] end)
	defineCall( constexprFloatType, constexprFloatType, "-", {constexprFloatType}, function(this,args) return this-args[0] end)
	defineCall( constexprFloatType, constexprFloatType, "/", {constexprFloatType}, function(this,args) return this/args[0] end)
	defineCall( constexprFloatType, constexprFloatType, "*", {constexprFloatType}, function(this,args) return this*args[0] end)

	-- Define arithmetic operators of integers with a float as promotion of the left hand integer to a float and an operator of a float with a float
	definePromoteCall( constexprFloatType, constexprIntegerType, constexprFloatType, "+", {constexprFloatType},function(this) return this end)
	definePromoteCall( constexprFloatType, constexprIntegerType, constexprFloatType, "-", {constexprFloatType},function(this) return this end)
	definePromoteCall( constexprFloatType, constexprIntegerType, constexprFloatType, "/", {constexprFloatType},function(this) return this end)
	definePromoteCall( constexprFloatType, constexprIntegerType, constexprFloatType, "*", {constexprFloatType},function(this) return this end)
end
-- Define the type conversions of const expressions to other const expression types
function defineConstExprTypeConversions()
	typedb:def_reduction( constexprBooleanType, constexprIntegerType, function( value) return value ~= "0" end, tag_typeConversion, rdw_bool)
	typedb:def_reduction( constexprBooleanType, constexprFloatType, function( value) return math.abs(value) < math.abs(epsilon) end, tag_typeConversion, rdw_bool)
	typedb:def_reduction( constexprFloatType, constexprIntegerType, function( value) return value:tonumber() end, tag_typeConversion, rdw_float)
	typedb:def_reduction( constexprFloatType, constexprUIntegerType, function( value) return value:tonumber() end, tag_typeConversion, rdw_float)
	typedb:def_reduction( constexprIntegerType, constexprFloatType, function( value) return bcd.int( tostring(value)) end, tag_typeConversion, rdw_float)
	typedb:def_reduction( constexprIntegerType, constexprUIntegerType, function( value) return value end, tag_typeConversion, rdw_sign)
	typedb:def_reduction( constexprUIntegerType, constexprIntegerType, function( value) return value end, tag_typeConversion, rdw_sign)

	local function bool2bcd( value) if value then return bcd.int("1") else return bcd.int("0") end end
	typedb:def_reduction( constexprIntegerType, constexprBooleanType, bool2bcd, tag_typeConversion, rdw_bool)
end

