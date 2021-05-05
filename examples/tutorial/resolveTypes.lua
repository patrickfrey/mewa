local utils = require "typesystem_utils"

-- Get the handle of a type expected to have no arguments (plain typedef type or a variable name)
function selectNoArgumentType( node, typeName, resolveContextTypeId, reductions, items)
	if not resolveContextTypeId or type(resolveContextTypeId) == "table" then -- not found or ambiguous
		utils.errorResolveType( typedb, node.line, resolveContextTypeId, seekContext, typeName)
	end
	for _,item in ipairs(items) do if typedb:type_nof_parameters( item) == 0 then return item end end
	utils.errorMessage( node, string.format( "Failed to find type '%s %s' without parameter", typedb:type_string(resolveContextTypeId), typeName))
end
-- Get the type handle of a type defined as a path
function resolveTypeFromNamePath( node, arg, argidx)
	if not argidx then argidx = #arg end
	local typeName = arg[ argidx]
	local typeId,constructor
	local seekContext
	if argidx > 1 then seekContext = resolveTypeFromNamePath( node, arg, argidx-1) else seekContext = getSeekContextTypes() end
	local resolveContextTypeId, reductions, items = typedb:resolve_type( seekContext, typeName, tagmask_namespace)
	return selectNoArgumentType( node, typeName, resolveContextTypeId, reductions, items)
end
