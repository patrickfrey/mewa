local utils = require "typesystem_utils"

function typesystem.program( node, options)
	defineConstExprArithmetics()
	initBuiltInTypes()
	initControlBooleanTypes()
	local context = {domain="global"}
	utils.traverse( typedb, node, context)
end
function typesystem.definition( node, pass, context, pass_selected)
	if not pass_selected or pass == pass_selected then	-- if the pass matches the declaration in the grammar
		local statement = table.unpack( utils.traverse( typedb, node, context or {domain="local"}))
		return statement and statement.constructor or nil
	end
end
function typesystem.definition_2pass( node, pass, context, pass_selected)
	if not pass_selected then
		return typesystem.definition( node, pass, context, pass_selected)
	elseif pass == pass_selected+1 then
		utils.traverse( typedb, node, context, 1) 	-- 3rd pass: declarations
	elseif pass == pass_selected then
		utils.traverse( typedb, node, context, 2)	-- 4th pass: implementations
	end
end
function typesystem.main_procdef( node)
	local env = defineCallableEnvironment( node, "main ", scalarIntegerType)
	local block = table.unpack( utils.traverse( typedb, node, {domain="local"}))
	local body = block.code .. utils.constructor_format( llvmir.control.returnStatement, {type="i32",this="0"})
	print( "\n" .. utils.constructor_format( llvmir.control.mainDeclaration, {body=body}))
end
function typesystem.codeblock( node)
	local stmlist = utils.traverse( typedb, node)
	local code = ""
	for _,stm in ipairs(stmlist) do code = code .. stm.code end
	return {code=code}
end
function typesystem.free_expression( node)
	local expression = table.unpack( utils.traverse( typedb, node))
	if expression.type == controlTrueType or expression.type == controlFalseType then
		return {code=expression.constructor.code .. utils.constructor_format( llvmir.control.label, {inp=expression.constructor.out})}
	else
		return {code=expression.constructor.code}
	end
end
