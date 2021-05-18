local utils = require "typesystem_utils"

-- Get the parameter string of a function declaration
function getDeclarationLlvmTypeRegParameterString( descr, context)
	local rt = ""
	if context.domain == "member" then rt = rt .. typeDescriptionMap[ context.decltype].llvmtype .. "* %ths, " end
	for ai,arg in ipairs(descr.param or {}) do rt = rt .. arg.llvmtype .. " " .. arg.reg .. ", " end
	if rt ~= "" then rt = rt:sub(1, -3) end
	return rt
end

-- Get the parameter string of a function typedef
function getDeclarationLlvmTypedefParameterString( descr, context)
	local rt = ""
	if context.domain == "member" then rt = rt .. (descr.llvmthis or context.descr.llvmtype) .. "*, " end
	for ai,arg in ipairs(descr.param) do rt = rt .. arg.llvmtype .. ", " end
	if rt ~= "" then rt = rt:sub(1, -3) end
	return rt
end

-- Get (if already defined) or create the callable context type (function name) on which the "()" operator implements the function call
function getOrCreateCallableContextTypeId( contextTypeId, name, descr)
	local rt = typedb:get_type( contextTypeId, name)
	if not rt then
		rt = typedb:def_type( contextTypeId, name)
		typeDescriptionMap[ rt] = descr
	end
	return rt
end

-- Define a direct function call: class method call, free function call
function defineFunctionCall( node, descr, context)
	descr.argstr = getDeclarationLlvmTypedefParameterString( descr, context)
	descr.llvmtype = utils.template_format( llvmir.control.functionCallType, descr)
	local contextType = getDeclarationContextTypeId(context)
	local thisType = (contextType ~= 0) and referenceTypeMap[ contextType] or 0
	local callablectx = getOrCreateCallableContextTypeId( thisType, descr.name, llvmir.callableDescr)
	local constructor = callConstructor( descr.ret and llvmir.control.functionCall or llvmir.control.procedureCall, thisType, descr.param, descr)
	return defineCall( descr.ret, callablectx, "()", descr.param, constructor)
end

