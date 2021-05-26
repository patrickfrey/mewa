local utils = require "typesystem_utils"
-- Constant expression scalar types and string type represented as Lua values
constexprIntegerType = typedb:def_type( 0, "constexpr int")
constexprDoubleType = typedb:def_type( 0, "constexpr double")
constexprBooleanType = typedb:def_type( 0, "constexpr bool")
constexprStringType = typedb:def_type( 0, "constexpr string")

-- structure initializer list type for structure declarations in the source
constexprStructureType = typedb:def_type( 0, "constexpr struct")

-- Void type handled as no type:
voidType = typedb:def_type( 0, "void")
-- String constant, this example language knows strings only as constants:
stringType = defineDataType( {line=0}, 0, "string", llvmir.string)

scalarTypeMap.void = voidType
scalarTypeMap.string = stringType

typeDescriptionMap[ constexprIntegerType] = llvmir.constexprIntegerDescr
typeDescriptionMap[ constexprDoubleType] = llvmir.constexprDoubleDescr
typeDescriptionMap[ constexprBooleanType] = llvmir.constexprBooleanDescr
typeDescriptionMap[ constexprStructureType] = llvmir.constexprStructDescr

function isScalarConstExprValueType( initType)
	return initType >= constexprIntegerType and initType <= stringType
end

scalarTypeMap["constexpr int"] = constexprIntegerType
scalarTypeMap["constexpr double"] = constexprDoubleType
scalarTypeMap["constexpr bool"] = constexprBooleanType
scalarTypeMap["constexpr struct"] = constexprStructureType
scalarTypeMap["constexpr string"] = constexprStringType

-- Create a constexpr node from a lexem in the AST
function createConstexprValue( typeId, value)
	if typeId == constexprBooleanType then
		if value == "true" then return true else return false end
	elseif typeId == constexprIntegerType or typeId == constexprDoubleType then
		return tonumber(value)
	elseif typeId == constexprStringType then
		if not stringConstantMap[ value] then
			local encval,enclen = utils.encodeLexemLlvm(value)
			local name = utils.uniqueName( "string")
			stringConstantMap[ value] = {size=enclen+1,name=name}
			local subst = {name=name, size=enclen+1, value=encval}
			local fmt = llvmir.control.stringConstDeclaration
			print_section( "Constants", utils.constructor_format( fmt, subst) .. "\n")
		end
		return stringConstantMap[ value]
	end
end
-- List of value constructors from constexpr constructors
function constexprDoubleToDoubleConstructor( val)
	return constructorStruct( "0x" .. mewa.llvm_double_tohex( val))
end
function constexprDoubleToIntegerConstructor( val)
	return constructorStruct( tostring(tointeger(val)))
end
function constexprDoubleToBooleanConstructor( val)
	return constructorStruct( math.abs(val) < epsilon and "0" or "1")
end
function constexprIntegerToDoubleConstructor( val)
	return constructorStruct( "0x" .. mewa.llvm_double_tohex( val))
end
function constexprIntegerToIntegerConstructor( val)
	return constructorStruct( tostring(val))
end
function constexprIntegerToBooleanConstructor( val)
	return constructorStruct( val == "0" and "0" or "1")
end
function constexprBooleanToScalarConstructor( val)
	return constructorStruct( val == true and "1" or "0")
end

function defineBinopConstexpr( typ, opr, constructor)
	defineCall( typ, typ, opr, {typ}, constructor)
end
function defineBinopConstexprPromote( typ, promotetyp, opr, promote)
	definePromoteCall( promotetyp, typ, promotetyp, opr, {promotetyp}, promote)
end
-- Define arithmetics of constant expressions
function defineConstExprArithmetics()
	defineCall( constexprIntegerType, constexprIntegerType, "-", {},
				function(this) return -this end)
	defineCall( constexprDoubleType, constexprDoubleType, "-", {},
				function(this) return -this end)
	defineBinopConstexpr( constexprIntegerType, "+", function(t,a) return t+a[1] end)
	defineBinopConstexpr( constexprIntegerType, "-", function(t,a) return t-a[1] end)
	defineBinopConstexpr( constexprIntegerType, "*",
				function(t,a) return tointeger(t*a[1]) end)
	defineBinopConstexpr( constexprIntegerType, "/",
				function(t,a) return tointeger(t/a[1]) end)
	defineBinopConstexpr( constexprDoubleType, "+", function(t,a) return t+a[1] end)
	defineBinopConstexpr( constexprDoubleType, "-", function(t,a) return t-a[1] end)
	defineBinopConstexpr( constexprDoubleType, "*", function(t,a) return t*a[1] end)
	defineBinopConstexpr( constexprDoubleType, "/", function(t,a) return t/a[1] end)

	-- Define arithmetic operators of integers with a double as promotion of the
	--   left-hand integer to a double and an operator of a double with a double:
	for _,opr in ipairs({"+","-","*","/"}) do
		defineBinopConstexprPromote( constexprIntegerType, constexprDoubleType, opr,
					function(this) return this end)
	end
end
-- Define the type conversions of const expressions to other const expression types
function defineConstExprTypeConversions()
	typedb:def_reduction( constexprBooleanType, constexprIntegerType,
			function( value) return math.abs(value) > epsilon end,
			tag_typeConversion, rdw_constexpr_bool)
	typedb:def_reduction( constexprBooleanType, constexprDoubleType,
			function( value) return math.abs(value) > epsilon end,
			tag_typeConversion, rdw_constexpr_bool)
	typedb:def_reduction( constexprDoubleType, constexprIntegerType,
			function( value) return value end,
			tag_typeConversion, rdw_constexpr_float)
	typedb:def_reduction( constexprIntegerType, constexprDoubleType,
			function( value) return math.floor(value) end,
			tag_typeConversion, rdw_constexpr_float)
	typedb:def_reduction( constexprIntegerType, constexprBooleanType,
			function( value) return value and 1 or 0 end,
			tag_typeConversion, rdw_constexpr_bool)
end
