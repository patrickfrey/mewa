local utils = require "typesystem_utils"

function typesystem.binop( node, operator)
    local this,operand = table.unpack( utils.traverse( typedb, node))
    expectValueType( node, this)
    expectValueType( node, operand)
    return applyCallable( node, this, operator, {operand})
end
function typesystem.unop( node, operator)
    local this = table.unpack( utils.traverse( typedb, node))
    expectValueType( node, operand)
    return applyCallable( node, this, operator, {})
end
function typesystem.member( node)
    local struct,name = table.unpack( utils.traverse( typedb, node))
    return applyCallable( node, struct, name)
end
function typesystem.operator( node, operator)
    local args = utils.traverse( typedb, node)
    local this = table.remove( args, 1)
    return applyCallable( node, this, operator, args)
end
function typesystem.operator_array( node, operator)
    local args = utils.traverse( typedb, node)
    local this = table.remove( args, 1)
    return applyCallable( node, this, operator, args)
end
