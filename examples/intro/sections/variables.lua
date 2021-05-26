local utils = require "typesystem_utils"

-- Define a member variable of a class or a structure
function defineMemberVariable(
            node, descr, context, typeId, refTypeId, name)
    local memberpos = context.structsize or 0
    local index = #context.members
    local subst = {index=index, type=descr.llvmtype}
    local load_ref = utils.template_format( context.descr.loadelemref, subst)
    local load_val = utils.template_format( context.descr.loadelem, subst)

    while math.fmod( memberpos, descr.align) ~= 0 do
        memberpos = memberpos + 1
    end
    context.structsize = memberpos + descr.size
    local member = { type=typeId, name=name, descr=descr, bytepos=memberpos }
    table.insert( context.members, member)
    if not context.llvmtype then
        context.llvmtype = descr.llvmtype
    else
        context.llvmtype = context.llvmtype  .. ", " .. descr.llvmtype
    end
    defineCall( refTypeId, referenceTypeMap[ context.decltype], name, {},
            callConstructor( load_ref))
    defineCall( typeId, context.decltype, name, {},
            callConstructor( load_val))
end
-- Define a free global variable
function defineGlobalVariable(
            node, descr, context, typeId, refTypeId, name, initVal)
    local out = "@" .. name
    local var = typedb:def_type( 0, name, out)
    if var == -1 then
        utils.errorMessage( node.line, "Duplicate definition of '%s'", name)
    end
    typedb:def_reduction( refTypeId, var, nil, tag_typeDeclaration)
    if initVal
    and descr.scalar == true
    and isScalarConstExprValueType( initVal.type) then
        local initScalarConst
            = constructorParts(
                getRequiredTypeConstructor(
                    node, typeId, initVal,
                    tagmask_matchParameter, tagmask_typeConversion))
        local subst = {out = out, val = initScalarConst}
        print( utils.constructor_format( descr.def_global_val, subst))
    else
        utils.errorMessage( node.line, "Only const scalars as init of globals")
    end
end
-- Define a free local variable
function defineLocalVariable(
            node, descr, context, typeId, refTypeId, name, initVal)
    local env = getCallableEnvironment()
    local out = env.register()
    local code = utils.constructor_format( descr.def_local, {out=out}, env.register)
    local var = typedb:def_type( localDefinitionContext,name,constructorStruct(out))
    if var == -1 then
        utils.errorMessage( node.line, "Duplicate definition of '%s'", name)
    end
    typedb:def_reduction( refTypeId, var, nil, tag_typeDeclaration)
    local decl = {type=var, constructor={code=code,out=out}}
    if type(initVal) == "function" then initVal = initVal() end
    return applyCallable( node, decl, "=", {initVal})
end
-- Define a variable (what kind is depending on the context)
function defineVariable( node, context, typeId, name, initVal)
    local descr = typeDescriptionMap[ typeId]
    local refTypeId = referenceTypeMap[ typeId]
    if not refTypeId then
        utils.errorMessage( node.line, "Reference in variable declarations,: %s",
                           typedb:type_string(typeId))
    end
    if context.domain == "local" then
        return defineLocalVariable( node, descr, context,
                        typeId, refTypeId, name, initVal)
    elseif context.domain == "global" then
        return defineGlobalVariable( node, descr, context,
                        typeId, refTypeId, name, initVal)
    elseif context.domain == "member" then
        if initVal then
            utils.errorMessage( node.line, "No init value for member allowed")
        end
        defineMemberVariable( node, descr, context, typeId, refTypeId, name)
    else
        utils.errorMessage( node.line, "Context domain undefined (%s)",
                            mewa.tostring(context))
    end
end
-- Declare a variable implicitly that does not appear as definition in source
function defineImplicitVariable( node, typeId, name, reg)
    local var = typedb:def_type( localDefinitionContext, name, reg)
    if var == -1 then
        local declstr = typeDeclarationString( typeId, name)
        utils.errorMessage( node.line, "Duplicate type '%s'", declstr)
    end
    typedb:def_reduction( typeId, var, nil, tag_typeDeclaration)
    return var
end
-- Make a function parameter addressable by name in the callable body
function defineParameter( node, typeId, name)
    local env = getCallableEnvironment()
    local paramreg = env.register()
    local var = typedb:def_type( localDefinitionContext, name, paramreg)
    if var == -1 then
        utils.errorMessage( node.line, "Duplicate definition of parameter '%s'",
                typeDeclarationString( localDefinitionContext, name))
    end
    local descr = typeDescriptionMap[ typeId]
    local ptype = (descr.scalar or descr.class == "pointer")
                and typeId
                or referenceTypeMap[ typeId]
    if not ptype then
        utils.errorMessage( node.line, "Cannot use type '%s' as parameter type",
                    typedb:type_string(typeId))
    end
    typedb:def_reduction( ptype, var, nil, tag_typeDeclaration)
    return {type=ptype, llvmtype=typeDescriptionMap[ ptype].llvmtype, reg=paramreg}
end
-- Resolve a variable by name and return its type/constructor structure
function getVariable( node, varname)
    local env = getCallableEnvironment()
    local seekctx = getSeekContextTypes()
    local resolveContextTypeId, reductions, items
            = typedb:resolve_type( seekctx, varname, tagmask_resolveType)
    local typeId,constructor = selectNoArgumentType( node, seekctx, varname,
                    tagmask_resolveType, resolveContextTypeId, reductions, items)
    local variableScope = typedb:type_scope( typeId)
    if variableScope[1] == 0 or env.scope[1] <= variableScope[1] then
        -- the local variable is not belonging to another function
        return {type=typeId, constructor=constructor}
    else
        utils.errorMessage( node.line, "Not allowed to access variable '%s' not "
                        .. "defined in current function or global scope",
                        typedb:type_string(typeId))
    end
end

