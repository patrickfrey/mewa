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

-- Conversion of constexpr constant to representation in LLVM IR (floating point numbers need a conversion into an internal binary representation)
function constexprLlvmConversion( constexprType)
	if constexprType == constexprFloatType then
		return function( val) return "0x" .. mewa.llvm_float_tohex( constexprToFloat( val, constexprType)) end
	elseif constexprType == constexprIntegerType then
		return function( val) return string.format("%d", tointeger(val)) end
	else
		return function( val) return tostring( val) end
	end
end

