local utils = require "typesystem_utils"

-- Build a conditional if/elseif block
function conditionalIfElseBlock( node, condition, matchblk, elseblk, exitLabel)
    local cond_constructor
        = getRequiredTypeConstructor( node, controlTrueType, condition,
                        tagmask_matchParameter, tagmask_typeConversion)
    if not cond_constructor then
        local declstr = typedb:type_string(condition.type)
        utils.errorMessage( node.line, "Can't use type '%s' as condition", declstr)
    end
    local code = cond_constructor.code .. matchblk.code
    if exitLabel then
        local subst = {inp=cond_constructor.out, out=exitLabel}
        local fmt = llvmir.control.invertedControlType
        code = code .. utils.template_format( fmt, subst)
    else
        local subst = {inp=cond_constructor.out}
        local fmt = llvmir.control.label
        code = code .. utils.template_format( fmt, subst)
    end
    local out
    if elseblk then
        code = code .. elseblk.code
        out = elseblk.out
    else
        out = cond_constructor.out
    end
    return {code = code, out = out}
end
