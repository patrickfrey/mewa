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

