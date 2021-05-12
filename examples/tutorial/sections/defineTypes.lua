local utils = require "typesystem_utils"

-- Type string of a type declaration built from its parts for error messages
function typeDeclarationString( this, typnam, args)
	local rt = (this == 0 or not this) and typnam or (typedb:type_string(type(this) == "table" and this.type or this) .. " " .. typnam)
	if (args) then rt = rt .. "(" .. utils.typeListString( typedb, args) .. ")" end
	return rt
end
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
-- Define the assignment operator of a class
function defineClassStructureAssignmentOperator( node, typeId)
	local descr = typeDescriptionMap[ typeId]
	local function assignElementsConstructor( this, args)
		local env = getCallableEnvironment()
		if args and #args ~= 0 and #args ~= #descr.members then
			utils.errorMessage( node.line, "Number of elements %d in init does not match number of members %d in class '%s'", #args, #descr.members, typedb:type_string( typeId))
		end
		local this_inp,code = constructorParts( this.constructor)
		for mi,member in ipairs(descr.members) do
			local out = env.register()
			local loadref = descr.loadelemref
			local llvmtype = member.descr.llvmtype
			local member_reftype = referenceTypeMap[ member.type]
			local ths = {type=member_reftype,constructor=constructorStruct(out)}
			local member_element = applyCallable( node, ths, "=", {args and args[mi] or nil})
			code = code .. utils.constructor_format(loadref,{out=out,this=this_inp,index=mi-1, type=llvmtype}) .. member_element.constructor.code
		end
		return {code=code}
	end
	local function assignStructTypeConstructor( this, args)
		return assignElementsConstructor( this, args[1].list)
	end
	defineCall( nil, referenceTypeMap[ typeId], "=", {constexprStructureType}, assignStructTypeConstructor)
	defineCall( nil, referenceTypeMap[ typeId], "=", {}, assignElementsConstructor)
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
		for ai=1,nofElements do
			local arg = (ai <= nofElements) and args[ai] or nil
			local elemreg = env.register()
			local elem = typeConstructorPairStruct( elementRefTypeId, elemreg)
			local init = tryApplyCallable( node, elem, "=", {arg})
			if not init then utils.errorMessage( node.line, "Failed to find ctor with signature '%s'", typeDeclarationString( elem, "=", {arg})) end
			local memberwise_next = utils.constructor_format( descr.memberwise_index, {index=ai-1,this=this_inp,out=elemreg}, env.register)
			code = code .. memberwise_next .. init.constructor.code
		end
		return {out=this_inp, code=code}
	end
end
-- Constructor for an assignment of a structure (initializer list) to an array
function arrayStructureAssignmentConstructor( node, thisTypeId, elementTypeId, nofElements)
	local initfunction = memberwiseInitArrayConstructor( node, thisTypeId, elementTypeId, nofElements)
	return function( this, args)
		return initfunction( this, args[1].list)
	end
end
-- Implicit on demand type definition for array
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
		defineCall( voidType, arrayType, "=", {constexprStructureType}, arrayStructureAssignmentConstructor( node, arrayType, elementType, arraySize))
		typedb:scope( scope_bk,step_bk)
	end
	return arrayTypeMap[ arrayKey]
end
