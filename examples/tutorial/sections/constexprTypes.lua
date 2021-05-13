local utils = require "typesystem_utils"

constexprIntegerType = typedb:def_type( 0, "constexpr int")		-- signed integer type constant value, represented as Lua number
constexprFloatType = typedb:def_type( 0, "constexpr float")		-- single precision floating point number constant, represented as Lua number
constexprBooleanType = typedb:def_type( 0, "constexpr bool")		-- boolean constants
constexprStructureType = typedb:def_type( 0, "constexpr struct")	-- structure initializer list
constexprStringType = typedb:def_type( 0, "constexpr string")		-- constant string in source, constructor is '@' operator plus name of global
voidType = typedb:def_type( 0, "void")					-- void type handled as no type
stringType = defineDataType( {line=0}, 0, "string", llvmir.string)	-- string constant, this example language knows strings only as constants
scalarTypeMap.void = voidType
scalarTypeMap.string = stringType

typeDescriptionMap[ constexprIntegerType] = llvmir.constexprIntegerDescr
typeDescriptionMap[ constexprFloatType] = llvmir.constexprFloatDescr
typeDescriptionMap[ constexprBooleanType] = llvmir.constexprBooleanDescr
typeDescriptionMap[ constexprStructureType] = llvmir.constexprStructDescr

function isScalarConstExprValueType( initType) return initType >= constexprIntegerType and initType <= stringType end

scalarTypeMap["constexpr int"] = constexprIntegerType
scalarTypeMap["constexpr float"] = constexprFloatType
scalarTypeMap["constexpr bool"] = constexprBooleanType
scalarTypeMap["constexpr struct"] = constexprStructureType
scalarTypeMap["constexpr string"] = constexprStringType

-- Create a constexpr node from a lexem in the AST
function createConstexprValue( typeId, value)
	if typeId == constexprBooleanType then
		if value == "true" then return true else return false end
	elseif typeId == constexprIntegerType or typeId == constexprFloatType then
		return tonumber(value)
	elseif typeId == constexprStringType then
		if not stringConstantMap[ value] then
			local encval,enclen = utils.encodeLexemLlvm(value)
			local name = utils.uniqueName( "string")
			stringConstantMap[ value] = {size=enclen+1,name=name}
			print_section( "Constants", utils.constructor_format( llvmir.control.stringConstDeclaration, {name=name, size=enclen+1, value=encval}) .. "\n")
		end
		return stringConstantMap[ value]
	end
end
-- List of value constructors from constexpr constructors
function constexprFloatToFloatConstructor( val) return constructorStruct( "0x" .. mewa.llvm_float_tohex( val)) end
function constexprFloatToIntegerConstructor( val) return constructorStruct( tostring(tointeger(val))) end
function constexprFloatToBooleanConstructor( val) if math.abs(val) < epsilonthen then return constructorStruct( "0") else constructorStruct( "1") end end
function constexprIntegerToFloatConstructor( val) return constructorStruct( "0x" .. mewa.llvm_float_tohex( val:tonumber())) end
function constexprIntegerToIntegerConstructor( val) return constructorStruct( tostring(val)) end
function constexprIntegerToBooleanConstructor( val) if val == "0" then return constructorStruct( "0") else return constructorStruct( "1") end end
function constexprBooleanToScalarConstructor( val) if val == true then return constructorStruct( "1") else return constructorStruct( "0") end end

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

