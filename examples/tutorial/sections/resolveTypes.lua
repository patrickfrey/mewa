local utils = require "typesystem_utils"

-- Get the handle of a type expected to have no arguments (plain typedef type or a variable name)
function selectNoArgumentType( node, seekctx, typeName, resolveContextTypeId, reductions, items)
	if not resolveContextTypeId or type(resolveContextTypeId) == "table" then -- not found or ambiguous
		utils.errorResolveType( typedb, node.line, resolveContextTypeId, seekctx, typeName)
	end
	for _,item in ipairs(items) do if typedb:type_nof_parameters( item) == 0 then return item end end
	utils.errorMessage( node, string.format( "Failed to find type '%s %s' without parameter", utils.resolveTypeString( typedb, resolveContextTypeId, typeName)))
end
-- Get the type handle of a type defined as a path
function resolveTypeFromNamePath( node, arg, argidx)
	if not argidx then argidx = #arg end
	local typeName = arg[ argidx]
	local typeId,constructor
	local seekContext
	if argidx > 1 then seekContext = resolveTypeFromNamePath( node, arg, argidx-1) else seekContext = getSeekContextTypes() end
	local resolveContextTypeId, reductions, items = typedb:resolve_type( seekContext, typeName, tagmask_namespace)
	return selectNoArgumentType( node, seekContext, typeName, resolveContextTypeId, reductions, items)
end
-- Try to get the constructor and weight of a parameter passed with the deduction tagmask optionally passed as an argument
function tryGetWeightedParameterReductionList( node, redutype, operand, tagmask_decl, tagmask_conv)
	if redutype ~= operand.type then
		local redulist,weight,altpath = typedb:derive_type( redutype, operand.type, tagmask_decl, tagmask_conv)
		if altpath then
			utils.errorMessage( node.line, "Ambiguous derivation paths for '%s': %s | %s",
						typedb:type_string(operand.type), utils.typeListString(typedb,altpath," =>"), utils.typeListString(typedb,redulist," =>"))
		end
		return redulist,weight
	else
		return {},0.0
	end
end
-- Get the constructor of a type required. The deduction tagmasks are passed as an arguments
function getRequiredTypeConstructor( node, redutype, operand, tagmask_decl, tagmask_conv)
	if redutype ~= operand.type then
		local redulist,weight,altpath = typedb:derive_type( redutype, operand.type, tagmask_decl, tagmask_conv)
		if not redulist then
			utils.errorMessage( node.line, "Type mismatch, required type '%s'", typedb:type_string(redutype))
		elseif altpath then
			utils.errorMessage( node.line, "Ambiguous derivation paths for '%s': %s | %s",
						typedb:type_string(operand.type), utils.typeListString(typedb,altpath," =>"), utils.typeListString(typedb,redulist," =>"))
		end
		local rt = applyReductionList( node, redulist, operand.constructor)
		if not rt then utils.errorMessage( node.line, "Construction of '%s' <- '%s' failed", typedb:type_string(redutype), typedb:type_string(operand.type)) end
		return rt
	else
		return operand.constructor
	end
end
-- Issue an error if the argument does not refer to a value type
function expectValueType( node, item)
	if not item.constructor then utils.errorMessage( node.line, "'%s' does not refer to a value", typedb:type_string(item.type)) end
end
-- Issue an error if the argument does not refer to a data type
function expectDataType( node, item)
	if item.constructor then utils.errorMessage( node.line, "'%s' does not refer to a data type", typedb:type_string(item.type)) end
	return item.type
end
