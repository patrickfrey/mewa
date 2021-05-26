local utils = require "typesystem_utils"

function typesystem.funcdef( node, context, pass)
    local typnam = node.arg[1].value
    if not pass or pass == 1 then    -- 1st pass: function declaration
        local rtype = table.unpack(
                        utils.traverseRange( typedb, node, {2,2}, context))
        if rtype == voidType then rtype = nil end -- void return => procedure
        local symbolname = (context.domain == "member")
                    and (typedb:type_name(context.decltype) .. "__" .. typnam)
                    or typnam
        local rtllvmtype = rtype and typeDescriptionMap[ rtype].llvmtype or "void"
        local descr = {
                lnk = "internal",
                name = typnam,
                symbolname = symbolname,
                func = '@'..symbolname,
                ret = rtype,
                rtllvmtype = rtllvmtype,
                attr = "#0"}
        utils.traverseRange( typedb, node, {3,3}, context, descr, 1)
        utils.allocNodeData( node, localDefinitionContext, descr)
        defineFunctionCall( node, descr, context)
    end
    if not pass or pass == 2 then     -- 2nd pass: function implementation
        local descr = utils.getNodeData( node, localDefinitionContext)
        utils.traverseRange( typedb, node, {3,3}, context, descr, 2)
        if descr.ret then
            local rtdescr = typeDescriptionMap[descr.ret]
            local subst = {type=rtdescr.llvmtype,this=rtdescr.default}
            descr.body = descr.body
                .. utils.constructor_format( llvmir.control.returnStatement, subst)
        else
            descr.body = descr.body
                .. utils.constructor_format( llvmir.control.returnVoidStatement)
        end
        print( utils.constructor_format( llvmir.control.functionDeclaration, descr))
    end
end
function typesystem.callablebody( node, context, descr, selectid)
    local rt
    local context_ = {domain="local"}
    if selectid == 1 then -- parameter declarations
        local envname = "body " .. descr.name
        descr.env = defineCallableEnvironment( node, envname, descr.ret)
        descr.param = table.unpack(
                            utils.traverseRange( typedb, node, {1,1}, context_))
        descr.paramstr = getDeclarationLlvmTypeRegParameterString( descr, context)
    elseif selectid == 2 then -- statements in body
        if context.domain == "member" then
            expandMethodEnvironment( node, context, descr, descr.env)
        end
        local stmlist = utils.traverseRange( typedb, node, {2,#node.arg}, context_)
        local code = ""
        for _,statement in ipairs(stmlist) do code = code .. statement.code end
        descr.body = code
    end
    return rt
end
function typesystem.paramdef( node, param)
    local datatype,varname = table.unpack( utils.traverse( typedb, node, param))
    table.insert( param, defineParameter( node, datatype, varname))
end
function typesystem.paramdeflist( node, param)
    local param = {}
    utils.traverse( typedb, node, param)
    return param
end
