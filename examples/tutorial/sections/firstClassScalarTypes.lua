-- Initialize all built-in types
function initBuiltInTypes()
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
			scalarBoolType = typeId
			typedb:def_reduction( typeId, constexprBooleanType, constexprBooleanToScalarConstructor, tag_typeInstantiation)
		end
		scalarTypeMap[ typnam] = typeId
	end
	-- Define the conversions between built-in types
	for typnam, scalar_descr in pairs( llvmir.scalar) do
		local typeId = scalarTypeMap[ typnam]
		for typnam_conv,conv in ipairs(scalar_descr.conv) do
			local typeId_conv = scalarTypeMap[ typnam_conv]
			typedb:def_reduction( typeId, typeId_conv, callFunctionConstructor( conv.fmt, typeId), tag_typeConversion, conv.weight)
		end
	end
end

