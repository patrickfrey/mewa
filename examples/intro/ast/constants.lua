local utils = require "typesystem_utils"

function typesystem.constant( node, decl)
    local typeId = scalarTypeMap[ decl]
    local constructor = createConstexprValue( typeId, node.arg[1].value)
    return {type=typeId,constructor=constructor}
end
function typesystem.structure( node)
    local args = utils.traverse( typedb, node)
    return {type=constexprStructureType, constructor={list=args}}
end
