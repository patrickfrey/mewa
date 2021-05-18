local utils = require "typesystem_utils"

function typesystem.vardef( node, context)
	local datatype,varnam,initval = table.unpack( utils.traverse( typedb, node, context))
	io.stderr:write("DECLARE " .. context.domain .. " variable " .. varnam .. " " .. typedb:type_string(datatype) .. "\n")
	return defineVariable( node, context, datatype, varnam, initval)
end
function typesystem.variable( node)
	local varname = node.arg[ 1].value
	return getVariable( node, varname)
end
function typesystem.constant( node, decl)
	local typeId = scalarTypeMap[ decl]
	return {type=typeId,constructor=createConstexprValue( typeId, node.arg[1].value)}
end
function typesystem.structure( node)
	local args = utils.traverse( typedb, node)
	return {type=constexprStructureType, constructor={list=args}}
end
