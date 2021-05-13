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
dofile( "examples/tutorial/sections/callableTypes.lua")			-- All type declarations for callables (functions,methods,etc.)
dofile( "examples/tutorial/sections/resolveTypes.lua")			-- Methods to resolve types
dofile( "examples/tutorial/sections/controlBooleanTypes.lua")		-- Implementation of control boolean types
dofile( "examples/tutorial/sections/variables.lua")			-- Define variables (globals, locals, members)

-- AST Callbacks:
local typesystem = {}
function typesystem.program( node)
	defineConstExprArithmetics()
	initBuiltInTypes()
	initControlBooleanTypes()
	local context = {domain="global"}
	utils.traverse( typedb, node, context)
end
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
function typesystem.funcdef( node, context, pass)
	local typnam = node.arg[1].value
	if not pass or pass == 1 then
		local rtype = table.unpack( utils.traverseRange( typedb, node, {2,2}, context))
		if rtype == voidType then rtype = nil end -- void type as return value means that we are in a procedure without return value
		local symbolname = (context.domain == "member") and (typedb:type_name(context.decltype) .. "__" .. typnam) or typnam
		local rtllvmtype = rtype and typeDescriptionMap[ rtype].llvmtype or "void"
		local descr = {lnk="internal", name=typnam, symbolname=symbolname, func='@'..symbolname, ret=rtype, rtllvmtype=rtllvmtype, attr="#0"}
		utils.traverseRange( typedb, node, {3,3}, context, descr, 1)	-- 1st pass: function declaration
		utils.allocNodeData( node, localDefinitionContext, descr)
		io.stderr:write("DECLARE " .. context.domain .. " function " .. descr.symbolname
				.. " (" .. utils.typeListString(typedb,descr.param) .. ")"
				.. " -> " .. (rtype and utils.typeString(typedb,rtype) or "void") .. "\n")
		defineFunctionCall( node, descr, context)
	end
	if not pass or pass == 2 then
		local descr = utils.getNodeData( node, localDefinitionContext)
		utils.traverseRange( typedb, node, {3,3}, context, descr, 2)	-- 2nd pass: function implementation
		io.stderr:write("IMPLEMENTATION function " .. descr.name .. "\n")
		if descr.ret then
			local rtdescr = typeDescriptionMap[descr.ret]
			descr.body = descr.body .. utils.constructor_format( llvmir.control.returnStatement, {type=rtdescr.llvmtype,this=rtdescr.default})
		else
			descr.body = descr.body .. utils.constructor_format( llvmir.control.returnVoidStatement)
		end
		print( utils.constructor_format( llvmir.control.functionDeclaration, descr))
	end
end
function typesystem.callablebody( node, context, descr, selectid)
	local rt
	local subcontext = {domain="local"}
	if selectid == 1 then -- parameter declarations
		descr.env = defineCallableEnvironment( node, "body " .. descr.name, descr.ret)
		io.stderr:write("PARAMDECL function " .. descr.name .. "\n")
		descr.param = table.unpack( utils.traverseRange( typedb, node, {1,1}, subcontext))
		descr.paramstr = getDeclarationLlvmTypeRegParameterString( descr, context)
	elseif selectid == 2 then -- statements in body
		if context.domain == "member" then expandMethodEnvironment( node, context, descr, descr.env) end
		io.stderr:write("STATEMENTS function " .. descr.name .. "\n")
		local statementlist = utils.traverseRange( typedb, node, {2,#node.arg}, subcontext)
		local code = ""
		for _,statement in ipairs(statementlist) do code = code .. statement.code end
		descr.body = code
	end
	return rt
end
function typesystem.main_procdef( node)
	local env = defineCallableEnvironment( node, "main ", scalarIntegerType)
	local block = table.unpack( utils.traverse( typedb, node, {domain="local"}))
	local body = block.code .. utils.constructor_format( llvmir.control.returnStatement, {type="i32",this="0"})
	print( "\n" .. utils.constructor_format( llvmir.control.mainDeclaration, {body=body}))
end
function typesystem.paramdef( node, param)
	local datatype,varname = table.unpack( utils.traverse( typedb, node, param))
	table.insert( param, defineParameter( node, datatype, varname))
end
function typesystem.paramdeflist( node, param)
	local param = {}
	utils.traverse( typedb, node, param)
	return param
end
function typesystem.codeblock( node)
	local stmlist = utils.traverse( typedb, node)
	local code = ""
	for _,stm in ipairs(stmlist) do code = code .. stm.code end
	return {code=code}
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
	local env = getCallableEnvironment()
	local rtype = env.returntype
	local descr = typeDescriptionMap[ rtype]
	expectValueType( node, operand)
	if rtype == 0 then utils.errorMessage( node.line, "Can't return value from procedure") end
	local this,code = constructorParts( getRequiredTypeConstructor( node, rtype, operand, tagmask_matchParameter, tagmask_typeConversion))
	return {code = code .. utils.constructor_format( llvmir.control.returnStatement, {this=this, type=descr.llvmtype})}
end
function typesystem.return_void( node)
	local env = getCallableEnvironment()
	if env.returntype ~= 0 then utils.errorMessage( node.line, "Can't return without value from function") end
	return {code = utils.constructor_format( llvmir.control.returnVoidStatement, {})}
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
	return defineVariable( node, context, datatype, varnam, initval)
end
function typesystem.structure( node)
	local args = utils.traverse( typedb, node)
	return {type=constexprStructureType, constructor={list=args}}
end
function typesystem.variable( node)
	local varname = node.arg[ 1].value
	return getVariable( node, varname)
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
