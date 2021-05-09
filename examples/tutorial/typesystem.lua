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
		local arg = table.unpack( utils.traverse( typedb, node, context or {domain="local"}))
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
	io.stderr:write("-- DONE class " .. descr.symbol .. "\n")
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
function typesystem.conditional_else( node, exitLabel)
	return table.unpack( utils.traverse( typedb, node))
end
function typesystem.conditional_elseif( node, exitLabel)
	local env = getCallableEnvironment()
	local condition,yesblock,noblock = table.unpack( utils.traverse( typedb, node, exitLabel))
	return conditionalIfElseBlock( node, condition, yesblock, noblock, exitLabel)
end
function typesystem.free_expression( node)
	local expression = table.unpack( utils.traverse( typedb, node))
	if expression.type == controlTrueType or expression.type == controlFalseType then
		return {code=expression.constructor.code .. utils.constructor_format( llvmir.control.label, {inp=expression.constructor.out})}
	else
		return {code=expression.constructor.code}
	end
end
function typesystem.return_value( node)
	local operand = table.unpack( utils.traverse( typedb, node))
	expectValueType( node, operand)
	local env = getCallableEnvironment()
	local rtype = env.returntype
	if rtype == 0 then utils.errorMessage( node.line, "Can't return value from procedure") end
	if doReturnValueAsReferenceParameter( rtype) then
		local reftype = referenceTypeMap[ rtype]
		local rt
		if operand.constructor.out == "%rt" then
			rt = operand -- copy elision through in-place construction done in 'defineVariable(..)'
		else
			rt = applyCallable( node, typeConstructorPairStruct( reftype, "%rt", ""), ":=", {operand})
		end
		return {code = rt.constructor.code .. doReturnVoidStatement(), nofollow=true}
	else
		local constructor = getRequiredTypeConstructor( node, rtype, operand, tagmask_matchParameter, tagmask_typeConversion)
		local code = env.returnfunction and env.returnfunction( constructor) or doReturnTypeStatement( rtype, constructor)
		return {code = code, nofollow=true}
	end
end
function typesystem.return_void( node)
	local env = getCallableEnvironment()
	local rtype = env.returntype
	if rtype ~= 0 then utils.errorMessage( node.line, "Can't return without value from function") end
	local code = ""
	if env.initstate then code = code .. completeCtorInitializationCode( false) end
	return {code = code .. doReturnVoidStatement(), nofollow=true}
end
function typesystem.conditional_if( node)
	local env = getCallableEnvironment()
	local exitLabel = env.label()
	local condition,yesblock,noblock = table.unpack( utils.traverse( typedb, node, exitLabel))
	local rt = conditionalIfElseBlock( node, condition, yesblock, noblock, exitLabel)
	rt.code = rt.code .. utils.constructor_format( llvmir.control.label, {inp=exitLabel})
	return rt
end
function typesystem.conditional_while( node)
	local env = getCallableEnvironment()
	local condition,yesblock = table.unpack( utils.traverse( typedb, node))
	local cond_constructor = getRequiredTypeConstructor( node, controlTrueType, condition, tagmask_matchParameter, tagmask_typeConversion)
	if not cond_constructor then utils.errorMessage( node.line, "Can't use type '%s' as a condition", typedb:type_string(condition.type)) end
	local start = env.label()
	local startcode = utils.constructor_format( llvmir.control.label, {inp=start})
	return {code = startcode .. cond_constructor.code .. yesblock.code
			.. utils.constructor_format( llvmir.control.invertedControlType, {out=start,inp=cond_constructor.out})}
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
function typesystem.binop( node, operator)
	local this,operand = table.unpack( utils.traverse( typedb, node))
	expectValueType( node, this)
	expectValueType( node, operand)
	return applyCallable( node, this, operator, {operand})
end
function typesystem.unop( node, operator)
	local this = table.unpack( utils.traverse( typedb, node))
	expectValueType( node, operand)
	return applyCallable( node, this, operator, {})
end
function typesystem.member( node)
	local struct,name = table.unpack( utils.traverse( typedb, node))
	return applyCallable( node, struct, name)
end
function typesystem.operator( node, operator)
	local args = utils.traverse( typedb, node)
	local this = table.remove( args, 1)
	return applyCallable( node, this, operator, args)
end
function typesystem.operator_array( node, operator)
	local args = utils.traverse( typedb, node)
	local this = table.remove( args, 1)
	return applyCallable( node, this, operator, args)
end

return typesystem

