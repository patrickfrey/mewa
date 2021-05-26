local utils = require "typesystem_utils"

-- Define a data type with all its qualifiers
function defineDataType( node, contextTypeId, typnam, descr)
    local typeId = typedb:def_type( contextTypeId, typnam)
    local refTypeId = typedb:def_type( contextTypeId, typnam .. "&")
    if typeId <= 0 or refTypeId <= 0 then
        utils.errorMessage( node.line, "Duplicate definition of '%s'", typnam)
    end
    referenceTypeMap[ typeId] = refTypeId
    dereferenceTypeMap[ refTypeId] = typeId
    typeDescriptionMap[ typeId] = descr
    typeDescriptionMap[ refTypeId] = llvmir.pointerDescr(descr)
    typedb:def_reduction( typeId, refTypeId, callConstructor( descr.load),
                    tag_typeDeduction, rdw_load)
    return typeId
end
-- Structure type definition for a class
function defineStructureType( node, declContextTypeId, typnam, fmt)
    local descr = utils.template_format( fmt, {symbol=typnam})
    local typeId = defineDataType( node, declContextTypeId, typnam, descr)
    return typeId,descr
end
-- Define the assignment operator of a class as memberwise assignment
function defineClassStructureAssignmentOperator( node, typeId)
    local descr = typeDescriptionMap[ typeId]
    local function assignElementsConstructor( this, args)
        local env = getCallableEnvironment()
        if args and #args ~= 0 and #args ~= #descr.members then
            utils.errorMessage( node.line, "Nof elements %d != members %d in '%s'",
                        #args, #descr.members, typedb:type_string( typeId))
        end
        local this_inp,code = constructorParts( this)
        for mi,member in ipairs(descr.members) do
            local out = env.register()
            local loadref = descr.loadelemref
            local llvmtype = member.descr.llvmtype
            local member_reftype = referenceTypeMap[ member.type]
            local ths = {type=member_reftype,constructor=constructorStruct(out)}
            local member_element
                       = applyCallable( node, ths, "=", {args and args[mi]})
            local subst = {out=out,this=this_inp,index=mi-1, type=llvmtype}
            code = code
                .. utils.constructor_format(loadref,subst)
                .. member_element.constructor.code
        end
        return {code=code}
    end
    local function assignStructTypeConstructor( this, args)
        return assignElementsConstructor( this, args[1].list)
    end
    defineCall( nil, referenceTypeMap[ typeId], "=", {constexprStructureType},
                assignStructTypeConstructor)
    defineCall( nil, referenceTypeMap[ typeId], "=", {},
                assignElementsConstructor)
end
-- Define the index access operator for arrays
function defineArrayIndexOperator( elemTypeId, arTypeId, arDescr)
    defineCall( referenceTypeMap[elemTypeId], referenceTypeMap[arTypeId], "[]",
                {scalarIntegerType}, callConstructor( arDescr.index[ "int"]))
end
-- Constructor for a memberwise assignment of an array from an initializer-list
function memberwiseInitArrayConstructor(
            node, thisType, elementType, nofElements)
    return function( this, args)
        if #args > nofElements then
            utils.errorMessage( node.line, "Nof elements %d > %d for init '%s'",
                        #args, nofElements, typedb:type_string( thisType))
        end
        local descr = typeDescriptionMap[ thisType]
        local descr_element = typeDescriptionMap[ elementType]
        local elementRefTypeId = referenceTypeMap[ elementType] or elementType
        local env = getCallableEnvironment()
        local this_inp,code = constructorParts( this)
        for ai=1,nofElements do
            local arg = (ai <= nofElements) and args[ai] or nil
            local elemreg = env.register()
            local elem = typeConstructorPairStruct( elementRefTypeId, elemreg)
            local init = tryApplyCallable( node, elem, "=", {arg})
            if not init then
                utils.errorMessage( node.line, "Failed to find ctor '%s'",
                            typeDeclarationString( elem, "=", {arg}))
            end
            local subst = {index=ai-1,this=this_inp,out=elemreg}
            local fmt = descr.memberwise_index
            local memberwise_next
                    = utils.constructor_format( fmt, subst, env.register)
            code = code .. memberwise_next .. init.constructor.code
        end
        return {out=this_inp, code=code}
    end
end
-- Constructor for an assignment of a structure (initializer list) to an array
function arrayStructureAssignmentConstructor(
        node, thisType, elementType, nofElements)
    local initfunction
        = memberwiseInitArrayConstructor( node, thisType, elementType, nofElements)
    return function( this, args)
        return initfunction( this, args[1].list)
    end
end
-- Implicit on demand type definition for array
function getOrCreateArrayType( node, elementType, arraySize)
    local arrayKey = string.format( "%d[%d]", elementType, arraySize)
    if not arrayTypeMap[ arrayKey] then
        local scope_bk,step_bk = typedb:scope( typedb:type_scope( elementType))
        local typnam = string.format( "[%d]", arraySize)
        local elementDescr = typeDescriptionMap[ elementType]
        local arrayDescr = llvmir.arrayDescr( elementDescr, arraySize)
        local arrayType = defineDataType( node, elementType, typnam, arrayDescr)
        local arrayRefType = referenceTypeMap[ arrayType]
        arrayTypeMap[ arrayKey] = arrayType
        defineArrayIndexOperator( elementType, arrayType, arrayDescr)
        local constructor = arrayStructureAssignmentConstructor(
                        node, arrayType, elementType, arraySize)
        defineCall( voidType, arrayRefType, "=",
                        {constexprStructureType}, constructor)
        typedb:scope( scope_bk,step_bk)
    end
    return arrayTypeMap[ arrayKey]
end
