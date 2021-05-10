local utils = require "typesystem_utils"

-- Get the parameter string of a function declaration
function getDeclarationLlvmTypeRegParameterString( descr, context)
	local rt = ""
	if context.domain == "member" then rt = rt .. typeDescriptionMap[ context.decltype].llvmtype .. "* %ths, " end
	for ai,arg in ipairs(descr.param or {}) do rt = rt .. arg.llvmtype .. " " .. arg.reg .. ", " end
	if rt ~= "" then rt = rt:sub(1, -3) end
	return rt
end
