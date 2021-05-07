-- Scalar first class type declarations
voidType = typedb:def_type( 0, "void")				-- the void type like in "C/C++"
stringType = defineDataType( 0, "string", llvmir.scalar.string)	-- string constant, this example language knows strings only as constants

scalarTypeMap.void = voidType
scalarTypeMap.string = stringType

-- Initialize all built-in types
function initBuiltInTypes()
	for typnam, scalar_descr in pairs( llvmir.scalar) do
		local typeId = defineDataType( 0, typnam, scalar_descr)
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
	--typedb:def_reduction( valtype, constexprFloatType, constexprFloatToFloatConstructor, tag_typeInstantiation, constexprInitWeight)
end

