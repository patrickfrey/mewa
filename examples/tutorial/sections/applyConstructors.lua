local utils = require "typesystem_utils"

-- Application of a constructor depending on its type and its argument type, return false as 2nd result on failure, true on success
function tryApplyConstructor( node, typeId, constructor, arg)
	if constructor then
		if (type(constructor) == "function") then
			local rt = constructor( arg)
			return rt, rt ~= nil
		elseif arg then
			utils.errorMessage( node.line, "Reduction constructor overwriting previous constructor for '%s'", typedb:type_string(typeId))
		else
			return constructor, true
		end
	else
		return arg, true
	end
end
-- Application of a constructor depending on its type and its argument type, throw error on failure
function applyConstructor( node, typeId, constructor, arg)
	local result_constructor,success = tryApplyConstructor( node, typeId, constructor, arg)
	if not success then utils.errorMessage( node.line, "Failed to create type '%s'", typedb:type_string(typeId)) end
	return result_constructor
end
-- Try to apply a list of reductions on a constructor, return false as 2nd result on failure, true on success
function tryApplyReductionList( node, redulist, redu_constructor)
	local success = true
	for _,redu in ipairs(redulist) do
		redu_constructor,success = tryApplyConstructor( node, redu.type, redu.constructor, redu_constructor)
		if not success then return nil end
	end
	return redu_constructor, true
end
-- Apply a list of reductions on a constructor, throw error on failure
function applyReductionList( node, redulist, redu_constructor)
	local success = true
	for _,redu in ipairs(redulist) do
		redu_constructor,success = tryApplyConstructor( node, redu.type, redu.constructor, redu_constructor)
		if not success then utils.errorMessage( node.line, "Reduction constructor failed for '%s'", typedb:type_string(redu.type)) end
	end
	return redu_constructor
end

