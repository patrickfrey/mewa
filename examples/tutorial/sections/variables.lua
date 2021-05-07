local utils = require "typesystem_utils"

-- Define a free variable or a member variable (depending on the context)
function defineVariable( node, context, typeId, name, initVal)
	local descr = typeDescriptionMap[ typeId]
	local refTypeId = referenceTypeMap[ typeId]
	if not refTypeId then utils.errorMessage( node.line, "References not allowed in variable declarations, use pointer instead: %s", typedb:type_string(typeId)) end
	if context.domain == "local" then
		local env = getCallableEnvironment()
		local out = env.register()
		local code = utils.constructor_format( descr.def_local, {out = out}, env.register)
		local var = typedb:def_type( localDefinitionContext, name, constructorStruct(out))
		if var == -1 then utils.errorMessage( node.line, "Duplicate definition of variable '%s'", name) end
		typedb:def_reduction( refTypeId, var, nil, tag_typeDeclaration)
		local decl = {type=var, constructor={code=code,out=out}}
		if type(initVal) == "function" then initVal = initVal() end
		local init = applyCallable( node, decl, "=", {initVal})
		return init
	elseif context.domain == "global" then
		out = "@" .. name
		local var = typedb:def_type( 0, name, out)
		if var == -1 then utils.errorMessage( node.line, "Duplicate definition of variable '%s'", name) end
		typedb:def_reduction( refTypeId, var, nil, tag_typeDeclaration)
		if initVal and descr.scalar == true and isScalarConstExprValueType( initVal.type) then
			local initScalarConst = constructorParts( getRequiredTypeConstructor( node, typeId, initVal, tagmask_matchParameter, tagmask_typeConversion))
			print( utils.constructor_format( descr.def_global_val, {out = out, val = initScalarConst})) -- print global data declaration
		else
			utils.errorMessage( node.line, "Only constant scalars allowed as initializer of global variables like '%s'", name)
		end
	elseif context.domain == "member" then
		if initVal then utils.errorMessage( node.line, "No initialization value in definition of member variable allowed") end
		defineVariableMember( node, descr, context, typeId, name)
	else
		utils.errorMessage( node.line, "Internal: Context domain undefined, context=%s", mewa.tostring(context))
	end
end

-- Define a member variable of a class or a structure
function defineVariableMember( node, descr, context, typeId, name)
	local memberpos = context.structsize or 0
	local index = #context.members
	local load_ref = utils.template_format( context.descr.loadelemref, {index=index, type=descr.llvmtype})
	local load_val = utils.template_format( context.descr.loadelem, {index=index, type=descr.llvmtype})

	while math.fmod( memberpos, descr.align) ~= 0 do memberpos = memberpos + 1 end
	context.structsize = memberpos + descr.size
	table.insert( context.members, { type = typeId, name = name, descr = descr, bytepos = memberpos })
	if not context.llvmtype then context.llvmtype = descr.llvmtype else context.llvmtype = context.llvmtype  .. ", " .. descr.llvmtype end
	defineCall( referenceTypeMap[ typeId], referenceTypeMap[ context.decltype], name, {}, callConstructor( load_ref))
	defineCall( typeId, context.decltype, name, {}, callConstructor( load_val))
end

