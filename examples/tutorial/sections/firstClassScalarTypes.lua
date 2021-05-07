-- Helper functions to define binary operators of first class scalar types
function defineBuiltInBinaryOperators( typnam, descr, operators, resultTypeId)
	for opr,fmt in ipairs(operators) do
		local typeId = scalarTypeMap[ typnam]
		defineCall( resultTypeId, typeId, opr, {typeId}, callConstructor( fmt, typeId))
		for _,promote in descr.promote do
			local typeId_promote = scalarTypeMap[ promote]
			local promoteConstructor = callConstructor( llvmir.scalar[ promote].conv[ typnam])
			local promoteResultTypeId; if resultTypeId == typeId then promoteResultTypeId = typeId_promote else promoteResultTypeId = resultTypeId end
			definePromoteCall( promoteResultTypeId, typeId, typeId_promote, opr, {typeId_promote}, promoteConstructor)
		end
	end
end
-- Helper functions to define binary operators of first class scalar types
function defineBuiltInUnaryOperators( typnam, descr, operators, resultTypeId)
	for opr,fmt in ipairs(operators) do
		local typeId = scalarTypeMap[ typnam]
		defineCall( resultTypeId, typeId, opr, {}, callConstructor( fmt, typeId))
	end
end
-- Initialize all built-in types
function initBuiltInTypes()
	-- Define the first class scalar types
	for typnam, scalar_descr in pairs( llvmir.scalar) do
		local typeId = defineDataType( {line=0}, 0, typnam, scalar_descr)
		if typnam == "int" then
			scalarIntegerType = typeId
			typedb:def_reduction( typeId, constexprIntegerType, constexprIntegerToIntegerConstructor, tag_typeInstantiation)
		end
		if typnam == "float" then
			scalarFloatType = typeId
			typedb:def_reduction( typeId, constexprFloatType, constexprFloatToFloatConstructor, tag_typeInstantiation)
		end
		if typnam == "bool" then
			scalarBooleanType = typeId
			typedb:def_reduction( typeId, constexprBooleanType, constexprBooleanToScalarConstructor, tag_typeInstantiation)
		end
		scalarTypeMap[ typnam] = typeId
	end
	-- Define the conversions between built-in types
	for typnam, scalar_descr in pairs( llvmir.scalar) do
		local typeId = scalarTypeMap[ typnam]
		for typnam_conv,conv in ipairs(scalar_descr.conv) do
			local typeId_conv = scalarTypeMap[ typnam_conv]
			typedb:def_reduction( typeId, typeId_conv, callConstructor( conv.fmt, typeId), tag_typeConversion, conv.weight)
		end
	end
	-- Define operators
	for typnam, scalar_descr in pairs( llvmir.scalar) do
		local typeId = scalarTypeMap[ typnam]
		defineBuiltInBinaryOperators( typnam, scalar_descr, scalar_descr.binop, typeId)
		defineBuiltInBinaryOperators( typnam, scalar_descr, scalar_descr.cmpop, scalarBooleanType)
		defineBuiltInUnaryOperators( typnam, scalar_descr, scalar_descr.unop, typeId)
		defineCall( voidType, referenceTypeMap[ typeId], "=", {typeId}, callConstructor( scalar_descr.assign, typeId))
	end
end

