local utils = require "typesystem_utils"

function typesystem.vardef( node, context)
	local datatype,varnam,initval = table.unpack( utils.traverse( typedb, node, context))
	return defineVariable( node, context, datatype, varnam, initval)
end
function typesystem.variable( node)
	local varname = node.arg[ 1].value
	return getVariable( node, varname)
end
