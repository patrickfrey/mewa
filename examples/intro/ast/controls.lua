local utils = require "typesystem_utils"

function typesystem.conditional_if( node)
	local env = getCallableEnvironment()
	local exitLabel = env.label()
	local condition,yesblock,noblock
		= table.unpack( utils.traverse( typedb, node, exitLabel))
	local rt = conditionalIfElseBlock(
			node, condition, yesblock, noblock, exitLabel)
	rt.code = rt.code
		.. utils.constructor_format( llvmir.control.label, {inp=exitLabel})
	return rt
end
function typesystem.conditional_else( node, exitLabel)
	return table.unpack( utils.traverse( typedb, node))
end
function typesystem.conditional_elseif( node, exitLabel)
	local env = getCallableEnvironment()
	local condition,yesblock,noblock
		= table.unpack( utils.traverse( typedb, node, exitLabel))
	return conditionalIfElseBlock(
			node, condition, yesblock, noblock, exitLabel)
end
function typesystem.conditional_while( node)
	local env = getCallableEnvironment()
	local condition,yesblock = table.unpack( utils.traverse( typedb, node))
	local cond_constructor
		= getRequiredTypeConstructor(
			node, controlTrueType, condition,
			tagmask_matchParameter, tagmask_typeConversion)
	if not cond_constructor then
		utils.errorMessage( node.line, "Can't use type '%s' as a condition",
					typedb:type_string(condition.type))
	end
	local start = env.label()
	local startcode = utils.constructor_format( llvmir.control.label, {inp=start})
	local subst = {out=start,inp=cond_constructor.out}
	return {code = startcode .. cond_constructor.code .. yesblock.code
			.. utils.constructor_format( llvmir.control.invertedControlType, subst)}
end
function typesystem.return_value( node)
	local operand = table.unpack( utils.traverse( typedb, node))
	local env = getCallableEnvironment()
	local rtype = env.returntype
	local descr = typeDescriptionMap[ rtype]
	expectValueType( node, operand)
	if rtype == 0 then
		utils.errorMessage( node.line, "Can't return value from procedure")
	end
	local this,code = constructorParts(
				getRequiredTypeConstructor(
					node, rtype, operand,
					tagmask_matchParameter, tagmask_typeConversion))
	local subst = {this=this, type=descr.llvmtype}
	return {code = code
			.. utils.constructor_format( llvmir.control.returnStatement, subst)}
end
function typesystem.return_void( node)
	local env = getCallableEnvironment()
	if env.returntype ~= 0 then
		utils.errorMessage( node.line, "Can't return without value from function")
	end
	return {code = utils.constructor_format( llvmir.control.returnVoidStatement, {})}
end
