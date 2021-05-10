local utils = require "typesystem_utils"

-- Constructor for a promote call (implementing int + float by promoting the first argument int to float and do a float + float to get the result)
function promoteCallConstructor( call_constructor, promote_constructor)
	return function( this, arg) return call_constructor( promote_constructor( this), arg) end
end
-- Define an operation with involving the promotion of the left hand argument to another type and executing the operation as defined for the type promoted to.
function definePromoteCall( returnType, thisType, promoteType, opr, argTypes, promote_constructor)
	local call_constructor = typedb:type_constructor( typedb:get_type( promoteType, opr, argTypes))
	local callType = typedb:def_type( thisType, opr, promoteCallConstructor( call_constructor, promote_constructor), argTypes)
	if callType == -1 then utils.errorMessage( node.line, "Duplicate definition '%s'", typeDeclarationString( thisType, opr, argTypes)) end
	if returnType then typedb:def_reduction( returnType, callType, nil, tag_typeDeclaration) end
end
-- Define an operation generalized
function defineCall( returnType, thisType, opr, argTypes, constructor)
	local callType = typedb:def_type( thisType, opr, constructor, argTypes)
	if callType == -1 then utils.errorMessage( 0, "Duplicate definition of call '%s'", typeDeclarationString( thisType, opr, argTypes)) end
	if returnType then typedb:def_reduction( returnType, callType, nil, tag_typeDeclaration) end
	return callType
end
-- Define an operation generalized
function defineDataType( node, contextTypeId, typnam, descr)
	local typeId = typedb:def_type( contextTypeId, typnam)
	local refTypeId = typedb:def_type( contextTypeId, typnam .. "&")
	if typeId <= 0 or refTypeId <= 0 then utils.errorMessage( node.line, "Duplicate definition of data type '%s'", typnam) end
	referenceTypeMap[ typeId] = refTypeId
	dereferenceTypeMap[ refTypeId] = typeId
	typeDescriptionMap[ typeId] = descr
	typeDescriptionMap[ refTypeId] = llvmir.pointerDescr(descr)
	typedb:def_reduction( typeId, refTypeId, callConstructor( descr.load), tag_typeDeduction, rdw_load)
	return typeId
end
-- Structure type definition for class
function defineStructureType( node, declContextTypeId, typnam, fmt)
	local descr = utils.template_format( fmt, {symbol=typnam})
	local typeId = defineDataType( node, declContextTypeId, typnam, descr)
	return typeId,descr
end
-- Define index operator for arrays
function defineArrayIndexOperator( elemTypeId, arTypeId, arDescr)
	defineCall( referenceTypeMap[elemTypeId], referenceTypeMap[arTypeId], "[]", {scalarIntegerType}, callConstructor( arDescr.index[ "int"]))
end
-- Structure type definition for array
function getOrCreateArrayType( node, elementType, arraySize)
	local arrayKey = string.format( "%d[%d]", elementType, arraySize)
	if not arrayTypeMap[ arrayKey] then
		local scope_bk,step_bk = typedb:scope( typedb:type_scope( elementType)) -- define the implicit array type in the same scope as the element type
		local typnam = string.format( "[%d]", arraySize)
		local arrayDescr = llvmir.arrayDescr( typeDescriptionMap[ elementType], arraySize)
		local arrayType = defineDataType( node, elementType, typnam, arrayDescr)
		arrayTypeMap[ arrayKey] = arrayType
		defineArrayIndexOperator( elementType, arrayType, arrayDescr)
		typedb:scope( scope_bk,step_bk)
	end
	return arrayTypeMap[ arrayKey]
end
