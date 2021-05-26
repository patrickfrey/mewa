local utils = require "typesystem_utils"

-- Get the handle of a type expected to have no arguments
function selectNoArgumentType(
        node, seekctx, typeName, tagmask, resolveContextType, reductions, items)
    if not resolveContextType or type(resolveContextType) == "table" then
        io.stderr:write( "TRACE typedb:resolve_type\n"
                .. utils.getResolveTypeTrace( typedb, seekctx, typeName, tagmask)
                .. "\n")
        utils.errorResolveType( typedb, node.line,
                      resolveContextType, seekctx, typeName)
    end
    for ii,item in ipairs(items) do
        if typedb:type_nof_parameters( item) == 0 then
            local constructor = applyReductionList( node, reductions, nil)
            local item_constr = typedb:type_constructor( item)
            constructor = applyConstructor( node, item, item_constr, constructor)
            return item,constructor
        end
    end
    utils.errorMessage( node.line, "Failed to resolve %s with no arguments",
                  utils.resolveTypeString( typedb, seekctx, typeName))
end
-- Get the type handle of a type defined as a path (elements of the
--    path are namespaces and parent structures followed by the type name resolved)
function resolveTypeFromNamePath( node, arg, argidx)
    if not argidx then argidx = #arg end
    local typeName = arg[ argidx]
    local seekContextTypes
    if argidx > 1 then
        seekContextTypes = resolveTypeFromNamePath( node, arg, argidx-1)
        expectDataType( node, seekContextTypes)
    else
        seekContextTypes = getSeekContextTypes()
    end
    local resolveContextType, reductions, items
            = typedb:resolve_type( seekContextTypes, typeName, tagmask_namespace)
    local typeId,constructor
            = selectNoArgumentType( node, seekContextTypes, typeName,
                    tagmask_namespace, resolveContextType, reductions, items)
    return {type=typeId, constructor=constructor}
end
-- Try to get the constructor and weight of a parameter passed with the
--    deduction tagmask optionally passed as an argument
function tryGetWeightedParameterReductionList(
        node, redutype, operand, tagmask_decl, tagmask_conv)
    if redutype ~= operand.type then
        local redulist,weight,altpath
            = typedb:derive_type( redutype, operand.type,
                                        tagmask_decl, tagmask_conv)
        if altpath then
            utils.errorMessage( node.line, "Ambiguous derivation for '%s': %s | %s",
                        typedb:type_string(operand.type),
                        utils.typeListString(typedb,altpath," =>"),
                        utils.typeListString(typedb,redulist," =>"))
        end
        return redulist,weight
    else
        return {},0.0
    end
end
-- Get the constructor of a type required. The deduction tagmasks are arguments
function getRequiredTypeConstructor(
        node, redutype, operand, tagmask_decl, tagmask_conv)
    if redutype ~= operand.type then
        local redulist,weight,altpath
            = typedb:derive_type( redutype, operand.type,
                                        tagmask_decl, tagmask_conv)
        if not redulist or altpath then
            io.stderr:write( "TRACE typedb:derive_type "
                    .. typedb:type_string(redutype)
                    .. " <- " .. typedb:type_string(operand.type) .. "\n"
                    .. utils.getDeriveTypeTrace( typedb, redutype, operand.type,
                                    tagmask_decl, tagmask_conv)
                    .. "\n")
        end
        if not redulist then
            utils.errorMessage( node.line, "Type mismatch, required type '%s'",
                        typedb:type_string(redutype))
        elseif altpath then
            utils.errorMessage( node.line, "Ambiguous derivation for '%s': %s | %s",
                        typedb:type_string(operand.type),
                        utils.typeListString(typedb,altpath," =>"),
                        utils.typeListString(typedb,redulist," =>"))
        end
        local rt = applyReductionList( node, redulist, operand.constructor)
        if not rt then
            utils.errorMessage( node.line, "Construction of '%s' <- '%s' failed",
                        typedb:type_string(redutype),
                        typedb:type_string(operand.type))
        end
        return rt
    else
        return operand.constructor
    end
end
-- Issue an error if the argument does not refer to a value type
function expectValueType( node, item)
    if not item.constructor then
        utils.errorMessage( node.line, "'%s' does not refer to a value",
                    typedb:type_string(item.type))
    end
end
-- Issue an error if the argument does not refer to a data type
function expectDataType( node, item)
    if item.constructor then
        utils.errorMessage( node.line, "'%s' does not refer to a data type",
                    typedb:type_string(item.type))
    end
    return item.type
end
