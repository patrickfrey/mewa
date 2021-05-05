local mewa = require "mewa"
local utils = require "typesystem_utils"
local typedb = mewa.typedb()

-- Tags attached to reduction definitions. When resolving a type or deriving a type, we select reductions by specifying a set of valid tags
local tag_typeDeclaration = 1		-- Type declaration relation (e.g. variable to data type)
local tag_typeConversion = 2		-- Type conversion of parameters
local tag_typeInstantiation = 3		-- Type value construction from const expression
local tag_namespace = 4			-- Type deduction for resolving namespaces
local tag_pushVararg = 5		-- Type deduction for passing parameter as vararg argument
local tag_transfer = 6			-- Transfer of information to build an object by a constructor, used in free function callable to pointer assignment

-- Sets of tags used for resolving a type or deriving a type, depending on the case
local tagmask_resolveType = typedb.reduction_tagmask( tag_typeDeclaration)
local tagmask_matchParameter = typedb.reduction_tagmask( tag_typeDeclaration, tag_typeConversion, tag_typeInstantiation, tag_transfer)
local tagmask_typeConversion = typedb.reduction_tagmask( tag_typeConversion)
local tagmask_namespace = typedb.reduction_tagmask( tag_namespace)
local tagmask_pushVararg = typedb.reduction_tagmask( tag_typeDeclaration, tag_typeConversion, tag_typeInstantiation, tag_pushVararg)

-- Global variables
local localDefinitionContext = 0	-- context type of local definitions
local seekContextKey = "seekctx"	-- key for context types defined for a scope
local callableEnvKey = "env"		-- key for current callable environment
local typeDescriptionMap = {}		-- maps any type id to the description of the associated type
local arrayTypeMap = {}			-- maps an array key to the array type if defined
local stringConstantMap = {}		-- map of string constant values to a structure with the attributes {name,size}

-- Scalar first class type declarations
local voidType = typedb:def_type( 0, "void")				-- the void type like in "C/C++"
local integerType = typedb:def_type( 0, "int")				-- signed integer type
local floatType = typedb:def_type( 0, "float")				-- single precision floating point number
local boolType = typedb:def_type( 0, "bool")				-- boolean value, either true or false
local stringType = typedb:def_type( 0, "string")			-- string constant, this example language knows strings only as constants
local constexprIntegerType = typedb:def_type( 0, "constexpr int")	-- signed integer type constant value, represented as Lua number
local constexprFloatType = typedb:def_type( 0, "constexpr float")	-- single precision floating point number constant, represented as Lua number
local constexprBoolType = typedb:def_type( 0, "constexpr bool")		-- boolean constants
local constexprStructureType = typedb:def_type( 0, "constexpr struct")	-- structure initializer list

local scalarTypeMap = {
	void=voidType, int=intType, float=floatType, bool=boolType, string=stringType,
	["constexpr int"]=constexprIntegerType, ["constexpr float"]=constexprFloatType,
	["constexpr bool"]=constexprBoolType, ["constexpr struct"]=constexprStructureType
}

-- Get the context type for type declarations
function getDeclarationContextTypeId( context)
	if context.domain == "local" then return localDefinitionContext
	elseif context.domain == "member" then return context.decltype
	elseif context.domain == "global" then return 0
	end
end
-- Get an object instance and clone it if it is not stored in the current scope, making it possible to add elements to an inherited instance in the current scope
function thisInstanceTableClone( name, emptyInst)
	local inst = typedb:this_instance( name)
	if not inst then
		inst = utils.deepCopyStruct( typedb:get_instance( name) or emptyInst)
		typedb:set_instance( name, inst)
	end
	return inst
end
-- Get the list of context types associated with the current scope used for resolving types
function getSeekContextTypes()
	return typedb:get_instance( seekContextKey) or {0}
end
-- Push an element to the current context type list used for resolving types
function pushSeekContextType( val)
	table.insert( thisInstanceTableClone( seekContextKey, {0}), val)
end
-- Get the handle of a type expected to have no arguments (plain typedef type or a variable name)
function selectNoArgumentType( node, typeName, resolveContextTypeId, reductions, items)
	if not resolveContextTypeId or type(resolveContextTypeId) == "table" then -- not found or ambiguous
		utils.errorResolveType( typedb, node.line, resolveContextTypeId, seekContext, typeName)
	end
	for _,item in ipairs(items) do if typedb:type_nof_parameters( item) == 0 then return item end end
	utils.errorMessage( node, string.format( "Failed to find type '%s %s' without parameter", typedb:type_string(resolveContextTypeId), typeName))
end
-- Get the type handle of a type defined as a path
function resolveTypeFromNamePath( node, arg, argidx)
	if not argidx then argidx = #arg end
	local typeName = arg[ argidx]
	local typeId,constructor
	local seekContext
	if argidx > 1 then seekContext = resolveTypeFromNamePath( node, arg, argidx-1) else seekContext = getSeekContextTypes() end
	local resolveContextTypeId, reductions, items = typedb:resolve_type( seekContext, typeName, tagmask_namespace)
	return selectNoArgumentType( node, typeName, resolveContextTypeId, reductions, items)
end
-- Create a callable environent object
function createCallableEnvironment( node, name, rtype, rprefix, lprefix)
	return {name=name, line=node.line, scope=typedb:scope(),
		register=utils.register_allocator(rprefix), label=utils.label_allocator(lprefix),
		returntype=rtype}
end
-- Attach a newly created data structure for a callable to its scope
function defineCallableEnvironment( node, name, rtype)
	local env = createCallableEnvironment( node, name, rtype)
	if typedb:this_instance( callableEnvKey) then
		utils.errorMessage( node.line, "Internal: Callable environment defined twice: %s %s", name, mewa.tostring({(typedb:scope())}))
	end
	typedb:set_instance( callableEnvKey, env)
	return env
end
-- Get the active callable instance
function getCallableEnvironment()
	return instantCallableEnvironment or typedb:get_instance( callableEnvKey)
end

-- AST Callbacks:
local typesystem = {}
function typesystem.program( node)
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
	local typeId = typedb:def_type( declContextTypeId, typnam)
	local descr = {name=typnam, type=typeId}
	typeDescriptionMap[ typeId] = descr
	local classContext = {domain="member", decltype=typeId}
	io.stderr:write("DECLARE " .. context.domain .. " class " .. descr.name .. "\n")
	utils.traverse( typedb, node, classContext, 1)	-- 1st pass: type definitions
	io.stderr:write("-- MEMBER VARIABLES class " .. descr.name .. "\n")
	utils.traverse( typedb, node, classContext, 2)	-- 2nd pass: member variables
	io.stderr:write("-- FUNCTION DECLARATIONS class " .. descr.name .. "\n")
	utils.traverse( typedb, node, classContext, 3)	-- 3rd pass: declarations
	io.stderr:write("-- FUNCTION IMPLEMENTATIONS class " .. descr.name .. "\n")
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
	io.stderr:write("DECLARE " .. context.domain .. " variable " .. varnam .. "\n")
end
function typesystem.structure( node)
	local args = utils.traverse( typedb, node)
	return {type=constexprStructureType, constructor={list=args}}
end
function typesystem.variable( node)
	local typeName = node.arg[ 1].value
	local env = getCallableEnvironment()
	local resolveContextTypeId, reductions, items = typedb:resolve_type( getSeekContextTypes(), typeName, tagmask_resolveType)
	local typeId = selectNoArgumentType( node, typeName, resolveContextTypeId, reductions, items)
	local variableScope = typedb:type_scope( typeId)
	if variableScope[1] == 0 or env.scope[1] <= variableScope[1] then
		return {type=typeId}
	else
		utils.errorMessage( node.line, "Not allowed to access variable '%s' that is not defined in local function or global scope", typedb:type_string(typeId))
	end
end
function typesystem.constant( node, decl)
	local typeId = scalarTypeMap[ decl]
	local value = node.arg[1].value
	if typeId == constexprBoolType then
		if value == "true" then value = true else value = false end
	elseif typeId == constexprIntegerType or typeId == constexprFloatType then
		value = tonumber(value)
	elseif typeId == stringType then
		if not stringConstantMap[ value] then
			local encval,enclen = utils.encodeLexemLlvm(value)
			local name = utils.uniqueName( "string")
			stringConstantMap[ value] = {size=enclen+1,name=name}
		end
		value = stringConstantMap[ value]
	end
	return {type=typeId,constructor=value}
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

