local utils = require "typesystem_utils"

-- Build a conditional if/elseif block
function conditionalIfElseBlock( node, condition, matchblk, elseblk, exitLabel)
	local cond_constructor = getRequiredTypeConstructor( node, controlTrueType, condition, tagmask_matchParameter, tagmask_typeConversion)
	if not cond_constructor then utils.errorMessage( node.line, "Can't use type '%s' as a condition", typedb:type_string(condition.type)) end
	local code = cond_constructor.code .. matchblk.code
	if exitLabel then
		code = code .. utils.template_format( llvmir.control.invertedControlType, {inp=cond_constructor.out, out=exitLabel})
	else
		code = code .. utils.template_format( llvmir.control.label, {inp=cond_constructor.out})
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

