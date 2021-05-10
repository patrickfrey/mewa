-- Define built-in promote calls for first class citizen scalar types
function defineBuiltInTypePromoteCalls( typnam, descr)
	local typeId = scalarTypeMap[ typnam]
	for i,promote_typnam in ipairs( descr.promote) do
		local promote_typeId = scalarTypeMap[ promote_typnam]
		local promote_descr = typeDescriptionMap[ promote_typeId]
		local promote_conv_fmt = promote_descr.conv[ typnam].fmt
		local promote_conv = promote_conv_fmt and callConstructor( promote_conv_fmt) or nil
		if promote_descr.binop then
			for operator,operator_fmt in pairs( promote_descr.binop) do
				definePromoteCall( promote_typeId, typeId, promote_typeId, operator, {promote_typeId}, promote_conv)
			end
		end
		if promote_descr.cmpop then
			for operator,operator_fmt in pairs( promote_descr.cmpop) do
				definePromoteCall( scalarBooleanType, typeId, promote_typeId, operator, {promote_typeId}, promote_conv)
			end
		end
	end
end
-- Helper functions to define binary operators of first class scalar types
function defineBuiltInBinaryOperators( typnam, descr, operators, resultTypeId)
	for opr,fmt in pairs(operators) do
		local typeId = scalarTypeMap[ typnam]
		defineCall( resultTypeId, typeId, opr, {typeId}, callConstructor( fmt, typeId))
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
		for typnam_conv,conv in pairs(scalar_descr.conv) do
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
	-- Define operators with promoting of the left side argument
	for typnam, scalar_descr in pairs( llvmir.scalar) do
		defineBuiltInTypePromoteCalls( typnam, scalar_descr)
	end
end

