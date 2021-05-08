mewa = require "mewa"
local utils = require "typesystem_utils"
llvmir = require "llvmir"
typedb = mewa.typedb()

-- Global variables
localDefinitionContext = 0		-- Context type of local definitions
seekContextKey = "seekctx"		-- Key for context types defined for a scope
callableEnvKey = "env"			-- Key for current callable environment
typeDescriptionMap = {}			-- Map of type ids to their description
referenceTypeMap = {}			-- Map of value type ids to their reference type if it exists
dereferenceTypeMap = {}			-- Map of reference type ids to their value type if it exists
arrayTypeMap = {}			-- Map array keys to their array type
stringConstantMap = {}			-- Map of string constant values to a structure with the attributes {name,size}
scalarTypeMap = {}			-- Map of scalar type names to the correspoding value type
instantCallableEnvironment = nil	-- Define a callable environment for an implicitly generated function not bound to a scope
globalCallableEnvironment = nil		-- The environment used in implicitely generated code for the initialization of globals

dofile( "examples/tutorial/sections/reductionTagsAndTagmasks.lua")	-- Reductions are defined with a tag and selected with a tagmask when addressed for type retrieval
dofile( "examples/tutorial/sections/reductionWeights.lua")		-- Weights of reductions
dofile( "examples/tutorial/sections/constructorFunctions.lua")		-- Functions of constructors
dofile( "examples/tutorial/sections/applyConstructors.lua")		-- Build constructors by applying constructor functions on argument constructors
dofile( "examples/tutorial/sections/applyCallable.lua")			-- Build constructors of calls of callables (types with a "()" operator with parameters to match)
dofile( "examples/tutorial/sections/defineTypes.lua")			-- Functions to define types with or without arguments
dofile( "examples/tutorial/sections/callableEnvironment.lua")		-- All data bound to a function are stored in a structure called callable environment associated to a scope
dofile( "examples/tutorial/sections/firstClassScalarTypes.lua")		-- Functions to define types with or without arguments
dofile( "examples/tutorial/sections/constexprTypes.lua")		-- All constant expression types and arithmetics are defined here
dofile( "examples/tutorial/sections/contextTypes.lua")			-- All type declarations are bound to a context type and for retrieval there is a set of context types defined, associated to a scope
dofile( "examples/tutorial/sections/resolveTypes.lua")			-- Methods to resolve types
dofile( "examples/tutorial/sections/controlBooleanTypes.lua")		-- Implementation of control boolean types
dofile( "examples/tutorial/sections/variables.lua")			-- Define variables (globals, locals, members)

-- AST Callbacks:
local typesystem = {}
function typesystem.program( node)
	globalCallableEnvironment = createCallableEnvironment( node, "globals ", nil, "%ir", "IL")
	defineConstExprArithmetics()
	initBuiltInTypes()
	initControlBooleanTypes()
	local context = {domain="global"}
	utils.traverse( typedb, node, context)
end
function typesystem.extern_funcdef( node)
end
function typesystem.extern_funcdef_vararg( node)
end
function typesystem.extern_paramdef( node)
end
function typesystem.extern_paramdef_collect( node)
end
function typesystem.extern_paramdeflist( node)
end
function typesystem.definition( node, pass, context, pass_selected)
	if not pass_selected or pass == pass_selected then	-- if the pass matches the declaration in the grammar
		local arg = table.unpack( utils.traverse( typedb, node, context))
		if arg then return {code = arg.constructor.code} else return {code=""} end
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
function typesystem.typehdr( node, decl)
	return resolveTypeFromNamePath( node, utils.traverse( typedb, node))
end
function typesystem.arraytype( node)
	local dim = tonumber( node.arg[2].value)
	local elemType = table.unpack( utils.traverseRange( typedb, node, {1,1}))
	local arraykey = string.format( "%d[%d]", elemType, dim)
	if not arrayTypeMap[ arrayKey] then
		local scope_bk,step_bk = typedb:scope( typedb:type_scope( elemType)) -- define the implicit array type in the same scope as the element type
		local typnam = string.format( "[%d]", dim)
		arrayTypeMap[ arrayKey] = typedb:def_type( elemType, typnam)
		typedb:scope( scope_bk,step_bk)
	end
	return arrayTypeMap[ arrayKey]
end
function typesystem.typespec( node)
	return table.unpack( utils.traverse( typedb, node))
end
function typesystem.inheritdef( node, decl)
end
function typesystem.classdef( node, context)
	local typnam = node.arg[1].value
	local declContextTypeId = getDeclarationContextTypeId( context)
	local descr,typeId = defineStructureType( node, declContextTypeId, typnam, llvmir.structTemplate)
	local classContext = {domain="member", decltype=typeId, members={}, descr=descr}
	io.stderr:write("DECLARE " .. context.domain .. " class " .. descr.symbol .. "\n")
	utils.traverse( typedb, node, classContext, 1)	-- 1st pass: type definitions
	io.stderr:write("-- MEMBER VARIABLES class " .. descr.symbol .. "\n")
	utils.traverse( typedb, node, classContext, 2)	-- 2nd pass: member variables
	io.stderr:write("-- FUNCTION DECLARATIONS class " .. descr.symbol .. "\n")
	utils.traverse( typedb, node, classContext, 3)	-- 3rd pass: declarations
	io.stderr:write("-- FUNCTION IMPLEMENTATIONS class " .. descr.symbol .. "\n")
	utils.traverse( typedb, node, classContext, 4)	-- 4th pass: implementations
end
function typesystem.funcdef( node, context, pass)
	local typnam = node.arg[1].value
	if not pass or pass == 1 then
		local rtype = table.unpack( utils.traverseRange( typedb, node, {2,2}, context))
		local descr = {name=typnam,rtype=rtype}
		utils.traverseRange( typedb, node, {3,3}, context, descr, 1)	-- 1st pass: function declaration
		utils.allocNodeData( node, localDefinitionContext, descr)
		io.stderr:write("DECLARE " .. context.domain .. " function " .. descr.name
				.. " (" .. utils.typeListString(typedb,param) .. ") -> " .. utils.typeString(typedb,rtype) .. "\n")
	end
	if not pass or pass == 2 then
		local descr = utils.getNodeData( node, localDefinitionContext)
		utils.traverseRange( typedb, node, {3,3}, context, descr, 2)	-- 2nd pass: function implementation
		io.stderr:write("IMPLEMENTATION function " .. descr.name .. "\n")
	end
end
function typesystem.callablebody( node, context, descr, selectid)
	local rt
	local subcontext = {domain="local"}
	if selectid == 1 then -- parameter declarations
		descr.env = defineCallableEnvironment( node, "body " .. descr.name, descr.ret)
		io.stderr:write("PARAMDECL function " .. descr.name .. "\n")
		descr.param = utils.traverseRange( typedb, node, {1,1}, subcontext)
	elseif selectid == 2 then -- statements in body
		local env = getCallableEnvironment()
		descr.env = env
		io.stderr:write("STATEMENTS function " .. descr.name .. "\n")
		utils.traverseRange( typedb, node, {2,#node.arg}, subcontext)
	end
	return rt
end
function typesystem.main_procdef( node)
end
function typesystem.paramdeflist( node)
end
function typesystem.paramdef( node)
end
function typesystem.codeblock( node)
	local stmlist = utils.traverse( typedb, node)
end
function typesystem.conditional_elseif( node)
end
function typesystem.conditional_else( node)
end
function typesystem.free_expression( node)
end
function typesystem.return_value( node)
end
function typesystem.return_void( node)
end
function typesystem.conditional_if( node)
end
function typesystem.conditional_while( node)
end
function typesystem.vardef( node, context)
	local datatype,varnam,initval = table.unpack( utils.traverse( typedb, node, context))
	io.stderr:write("DECLARE " .. context.domain .. " variable " .. varnam .. " " .. typedb:type_string(datatype) .. "\n")
	defineVariable( node, context, datatype, varnam, initval)
end
function typesystem.structure( node)
	local args = utils.traverse( typedb, node)
	return {type=constexprStructureType, constructor={list=args}}
end
function typesystem.variable( node)
	local typeName = node.arg[ 1].value
	local env = getCallableEnvironment()
	local seekctx = getSeekContextTypes()
	local resolveContextTypeId, reductions, items = typedb:resolve_type( seekctx, typeName, tagmask_resolveType)
	local typeId = selectNoArgumentType( node, seekctx, typeName, resolveContextTypeId, reductions, items)
	local variableScope = typedb:type_scope( typeId)
	if variableScope[1] == 0 or env.scope[1] <= variableScope[1] then
		return {type=typeId}
	else
		utils.errorMessage( node.line, "Not allowed to access variable '%s' that is not defined in local function or global scope", typedb:type_string(typeId))
	end
end
function typesystem.constant( node, decl)
	local typeId = scalarTypeMap[ decl]
	return {type=typeId,constructor=createConstexprValue( typeId, node.arg[1].value)}
end
function typesystem.binop( node, decl)
end
function typesystem.unop( node, decl)
end
function typesystem.member( node)
end
function typesystem.operator( node, decl)
end
function typesystem.operator_array( node, decl)
end

return typesystem

