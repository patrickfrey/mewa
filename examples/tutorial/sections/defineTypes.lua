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
-- Constructor for a memberwise assignment of a tree structure (initializing an "array")
function memberwiseInitArrayConstructor( node, thisTypeId, elementTypeId, nofElements)
	return function( this, args)
		if #args > nofElements then
			utils.errorMessage( node.line, "Number of elements %d in init is too big for '%s' [%d]", #args, typedb:type_string( thisTypeId), nofElements)
		end
		local descr = typeDescriptionMap[ thisTypeId]
		local descr_element = typeDescriptionMap[ elementTypeId]
		local elementRefTypeId = referenceTypeMap[ elementTypeId] or elementTypeId
		local env = getCallableEnvironment()
		local this_inp,res_code = constructorParts( this)
		local code = ""
		for ai,arg in ipairs(args) do
			local elemreg = env.register()
			local elem = {type=elementRefTypeId,constructor={out=elemreg}}
			local init = tryApplyCallable( node, elem, "=", {arg})
			if not init then utils.errorMessage( node.line, "Failed to find ctor with signature '%s'", getMessageFunctionSignature( elem, "=", {arg})) end
			local memberwise_next = utils.constructor_format( descr.memberwise_index, {index=ai-1,this=this_inp,out=elemreg}, env.register)
			code = code .. memberwise_next .. typeConstructorPairCode(init)
		end
		if #args < nofElements then
			local init = tryApplyCallable( node, typeConstructorPairStruct( elementRefTypeId, "%ths", ""), "=", {})
			if not init then utils.errorMessage( node.line, "Failed to find ctor with signature '%s'", getMessageFunctionSignature( elem, "=", {})) end
			local fmtdescr = {element=descr_element.symbol or descr_element.llvmtype, enter=env.label(), begin=env.label(), ["end"]=env.label(), index=#args,
						this=this_inp, ctors=init.constructor.code}
			code = code .. utils.constructor_format( descr.ctor_rest, fmtdescr, env.register)
		end
		return {out=this_inp, code=code}
	end
end
-- Structure type definition for array
function getOrCreateArrayType( node, elementType, arraySize)
	local arrayKey = string.format( "%d[%d]", elementType, arraySize)
	if not arrayTypeMap[ arrayKey] then
		local scope_bk,step_bk = typedb:scope( typedb:type_scope( elementType)) -- define the implicit array type in the same scope as the element type
		local typnam = string.format( "[%d]", arraySize)
		local arrayDescr = llvmir.arrayDescr( typeDescriptionMap[ elementType], arraySize)
		local arrayType = defineDataType( node, elementType, typnam, arrayDescr)
		local arrayRefType = referenceTypeMap[ arrayType]
		arrayTypeMap[ arrayKey] = arrayType
		defineArrayIndexOperator( elementType, arrayType, arrayDescr)
		defineCall( voidType, arrayType, "=", {constexprStructureType}, memberwiseInitArrayConstructor( node, arrayRefType, elementType, arraySize))
		typedb:scope( scope_bk,step_bk)
	end
	return arrayTypeMap[ arrayKey]
end
