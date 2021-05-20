local utils = require "typesystem_utils"

function typesystem.funcdef( node, context, pass)
	local typnam = node.arg[1].value
	if not pass or pass == 1 then
		local rtype = table.unpack( utils.traverseRange( typedb, node, {2,2}, context))
		if rtype == voidType then rtype = nil end -- void type as return value means that we are in a procedure without return value
		local symbolname = (context.domain == "member") and (typedb:type_name(context.decltype) .. "__" .. typnam) or typnam
		local rtllvmtype = rtype and typeDescriptionMap[ rtype].llvmtype or "void"
		local descr = {lnk="internal", name=typnam, symbolname=symbolname, func='@'..symbolname, ret=rtype, rtllvmtype=rtllvmtype, attr="#0"}
		utils.traverseRange( typedb, node, {3,3}, context, descr, 1)	-- 1st pass: function declaration
		utils.allocNodeData( node, localDefinitionContext, descr)
		defineFunctionCall( node, descr, context)
	end
	if not pass or pass == 2 then
		local descr = utils.getNodeData( node, localDefinitionContext)
		utils.traverseRange( typedb, node, {3,3}, context, descr, 2)	-- 2nd pass: function implementation
		if descr.ret then
			local rtdescr = typeDescriptionMap[descr.ret]
			descr.body = descr.body .. utils.constructor_format( llvmir.control.returnStatement, {type=rtdescr.llvmtype,this=rtdescr.default})
		else
			descr.body = descr.body .. utils.constructor_format( llvmir.control.returnVoidStatement)
		end
		print( utils.constructor_format( llvmir.control.functionDeclaration, descr))
	end
end
function typesystem.callablebody( node, context, descr, selectid)
	local rt
	local subcontext = {domain="local"}
	if selectid == 1 then -- parameter declarations
		descr.env = defineCallableEnvironment( node, "body " .. descr.name, descr.ret)
		descr.param = table.unpack( utils.traverseRange( typedb, node, {1,1}, subcontext))
		descr.paramstr = getDeclarationLlvmTypeRegParameterString( descr, context)
	elseif selectid == 2 then -- statements in body
		if context.domain == "member" then expandMethodEnvironment( node, context, descr, descr.env) end
		local statementlist = utils.traverseRange( typedb, node, {2,#node.arg}, subcontext)
		local code = ""
		for _,statement in ipairs(statementlist) do code = code .. statement.code end
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
