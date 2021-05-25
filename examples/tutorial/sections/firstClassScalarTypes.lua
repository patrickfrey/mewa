local utils = require "typesystem_utils"

-- Define built-in promote calls for first class citizen scalar types
function defineBuiltInTypePromoteCalls( typnam, descr)
	local typeId = scalarTypeMap[ typnam]
	for i,promote_typnam in ipairs( descr.promote) do
		local promote_typeId = scalarTypeMap[ promote_typnam]
		local promote_descr = typeDescriptionMap[ promote_typeId]
		local promote_conv_fmt = promote_descr.conv[ typnam].fmt
		local promote_conv = promote_conv_fmt and callConstructor( promote_conv_fmt)
		if promote_descr.binop then
			for operator,operator_fmt in pairs( promote_descr.binop) do
				definePromoteCall(
					promote_typeId, typeId, promote_typeId,
					operator, {promote_typeId}, promote_conv)
			end
		end
		if promote_descr.cmpop then
			for operator,operator_fmt in pairs( promote_descr.cmpop) do
				definePromoteCall(
					scalarBooleanType, typeId, promote_typeId,
					operator, {promote_typeId}, promote_conv)
			end
		end
	end
end
-- Define the promote calls of a const expression for a binary scalar operator
function defineBinopConstexprPromoteCalls( resultType, operator, argtype, descr)
	if descr.llvmtype == "double" then
		definePromoteCall( resultType, constexprDoubleType, argtype,
					operator, {argtype}, constexprDoubleToDoubleConstructor)
		definePromoteCall( resultType, constexprIntegerType, argtype,
					operator, {argtype}, constexprIntegerToDoubleConstructor)
		definePromoteCall( resultType, constexprBooleanType, argtype,
					operator, {argtype}, constexprBooleanToScalarConstructor)
	elseif descr.class == "bool" then
		definePromoteCall( resultType, constexprBooleanType, argtype,
					operator, {argtype}, constexprBooleanToScalarConstructor)
	elseif descr.class == "signed" then
		definePromoteCall( resultType, constexprIntegerType, argtype,
					operator, {argtype}, constexprIntegerToIntegerConstructor)
		definePromoteCall( resultType, constexprBooleanType, argtype,
					operator, {argtype}, constexprBooleanToScalarConstructor)
	end
end

-- Helper functions to define binary operators of first class scalar types
function defineBuiltInBinaryOperators( typnam, descr, operators, resultTypeId)
	for opr,fmt in pairs(operators) do
		local typeId = scalarTypeMap[ typnam]
		local descr = typeDescriptionMap[ typeId]
		defineCall( resultTypeId, typeId, opr, {typeId}, callConstructor( fmt))
		defineBinopConstexprPromoteCalls( resultTypeId, opr, typeId, descr)
	end
end
-- Helper functions to define binary operators of first class scalar types
function defineBuiltInUnaryOperators( typnam, descr, operators, resultTypeId)
	for opr,fmt in ipairs(operators) do
		local typeId = scalarTypeMap[ typnam]
		local descr = typeDescriptionMap[ typeId]
		defineCall( resultTypeId, typeId, opr, {}, callConstructor( fmt))
		defineBinopConstexprPromoteCalls( resultTypeId, opr, typeId, descr)
	end
end
-- Constructor of a string pointer from a string definition
function stringPointerConstructor( stringdef)
	local env = getCallableEnvironment()
	local out = env.register()
	local subst = {size=stringdef.size,name=stringdef.name,out=out}
	local code = utils.template_format( llvmir.control.stringConstConstructor, subst)
	return constructorStruct( out, code)
end
-- Initialize all built-in types
function initBuiltInTypes()
	-- Define the first class scalar types
	for typnam, descr in pairs( llvmir.scalar) do
		local typeId = defineDataType( {line=0}, 0, typnam, descr)
		if typnam == "int" then
			scalarIntegerType = typeId
			typedb:def_reduction( typeId, constexprIntegerType,
						constexprIntegerToIntegerConstructor, tag_typeInstantiation)
		elseif typnam == "double" then
			scalarDoubleType = typeId
			typedb:def_reduction( typeId, constexprDoubleType,
						constexprDoubleToDoubleConstructor, tag_typeInstantiation)
		elseif typnam == "bool" then
			scalarBooleanType = typeId
			typedb:def_reduction( typeId, constexprBooleanType,
						constexprBooleanToScalarConstructor, tag_typeInstantiation)
		end
		scalarTypeMap[ typnam] = typeId
	end
	-- Define the conversions between built-in types
	for typnam, descr in pairs( llvmir.scalar) do
		local typeId = scalarTypeMap[ typnam]
		for typnam_conv,conv in pairs(descr.conv) do
			local typeId_conv = scalarTypeMap[ typnam_conv]
			typedb:def_reduction( typeId, typeId_conv, callConstructor( conv.fmt),
						 tag_typeConversion, conv.weight)
		end
	end
	-- Define operators
	for typnam, descr in pairs( llvmir.scalar) do
		local typeId = scalarTypeMap[ typnam]
		defineBuiltInBinaryOperators( typnam, descr, descr.binop, typeId)
		defineBuiltInBinaryOperators( typnam, descr, descr.cmpop, scalarBooleanType)
		defineBuiltInUnaryOperators( typnam, descr, descr.unop, typeId)
		defineCall( voidType, referenceTypeMap[ typeId], "=", {typeId},
				callConstructor( descr.assign))
		defineCall( voidType, referenceTypeMap[ typeId], "=", {},
					callConstructor( descr.assign, 0, nil,
					{arg1=descr.default}))
	end
	-- Define operators with promoting of the left side argument
	for typnam, descr in pairs( llvmir.scalar) do
		defineBuiltInTypePromoteCalls( typnam, descr)
	end
	-- String type
	local string_descr = typeDescriptionMap[ stringType]
	local string_refType = referenceTypeMap[ stringType]
	typedb:def_reduction( stringType, string_refType,
					callConstructor( string_descr.load), tag_typeInstantiation)
	typedb:def_reduction( stringType, constexprStringType,
					stringPointerConstructor, tag_typeInstantiation)
	defineCall( voidType, string_refType, "=", {stringType},
					callConstructor( string_descr.assign))
	defineCall( voidType, string_refType, "=", {},
					callConstructor( string_descr.assign, 0, nil,
					{arg1=string_descr.default}))
end

