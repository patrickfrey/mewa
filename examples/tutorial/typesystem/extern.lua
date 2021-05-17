local utils = require "typesystem_utils"

function typesystem.extern_funcdef( node)
	local context = {domain="global"}
	local name,ret,param = table.unpack( utils.traverse( typedb, node, context))
	if ret == voidType then ret = nil end -- void type as return value means that we are in a procedure without return value
	local descr = {externtype="C", name=name, symbolname=name, func='@' .. name, ret=ret, param=param, signature=""}
	descr.rtllvmtype = ret and typeDescriptionMap[ ret].llvmtype or "void"
	defineFunctionCall( node, descr, context)
	print_section( "Typedefs", utils.constructor_format( llvmir.control.extern_functionDeclaration, descr))
end
function typesystem.extern_paramdef( node, param)
	local typeId = table.unpack( utils.traverseRange( typedb, node, {1,1}, context))
	local descr = typeDescriptionMap[ typeId]
	if not descr then utils.errorMessage( node.line, "Type '%s' not allowed as parameter of extern function", typedb:type_string( typeId)) end
	table.insert( param, {type=typeId,llvmtype=descr.llvmtype})
end
function typesystem.extern_paramdeflist( node)
	local param = {}
	utils.traverse( typedb, node, param)
	return param
end
