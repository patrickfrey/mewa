local utils = require "typesystem_utils"

function typesystem.typehdr( node, decl)
	return resolveTypeFromNamePath( node, utils.traverse( typedb, node))
end
function typesystem.arraytype( node)
	local dim = tonumber( node.arg[2].value)
	local elem = table.unpack( utils.traverseRange( typedb, node, {1,1}))
	return {type=getOrCreateArrayType( node, expectDataType( node, elem), dim)}
end
function typesystem.typespec( node)
	return expectDataType( node, table.unpack( utils.traverse( typedb, node)))
end
function typesystem.classdef( node, context)
	local typnam = node.arg[1].value
	local declContextTypeId = getDeclarationContextTypeId( context)
	local typeId,descr = defineStructureType( node, declContextTypeId, typnam, llvmir.structTemplate)
	local classContext = {domain="member", decltype=typeId, members={}, descr=descr}
	io.stderr:write("DECLARE " .. context.domain .. " class " .. descr.symbol .. "\n")
	utils.traverse( typedb, node, classContext, 1)	-- 1st pass: type definitions
	io.stderr:write("-- MEMBER VARIABLES class " .. descr.symbol .. "\n")
	utils.traverse( typedb, node, classContext, 2)	-- 2nd pass: member variables
	io.stderr:write("-- FUNCTION DECLARATIONS class " .. descr.symbol .. "\n")
	descr.size = classContext.structsize
	descr.members = classContext.members
	defineClassStructureAssignmentOperator( node, typeId)
	utils.traverse( typedb, node, classContext, 3)	-- 3rd pass: method declarations
	io.stderr:write("-- FUNCTION IMPLEMENTATIONS class " .. descr.symbol .. "\n")
	utils.traverse( typedb, node, classContext, 4)	-- 4th pass: method implementations
	print_section( "Typedefs", utils.constructor_format( descr.typedef, {llvmtype=classContext.llvmtype}))
	io.stderr:write("-- DONE class " .. descr.symbol .. "\n")
end

