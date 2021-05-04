local mewa = require "mewa"
local utils = require "typesystem_utils"
local typedb = mewa.typedb()

-- Global variables
local localDefinitionContext = 0	-- context type of local definitions
local seekContextKey = "seekctx"	-- key for context types defined for a scope
local typeDescriptionMap = {}		-- maps any type id to the description of the associated type
local arrayTypeMap = {}			-- maps an array key to the array type if defined

-- Print a log message for a visited node to stderr
function log_call( fname, ...)
	io.stderr:write("CALL " .. fname .. " " .. mewa.tostring({...}) .. "\n")
end

local voidType = typedb:def_type( 0, "void")
local integerType = typedb:def_type( 0, "int")
local floatType = typedb:def_type( 0, "float")
local boolType = typedb:def_type( 0, "bool")
local stringType = typedb:def_type( 0, "string")

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
-- Get the type handle of a type defined as a path
function resolveTypeFromNamePath( node, arg, argidx)
	if not argidx then argidx = #arg end
	local typeName = arg[ argidx]
	local typeId,constructor
	local seekContext
	if argidx > 1 then seekContext = resolveTypeFromNamePath( node, arg, argidx-1) else seekContext = getSeekContextTypes() end
	local resolveContextTypeId, reductions, items = typedb:resolve_type( seekContext, typeName)
	if not resolveContextTypeId or type(resolveContextTypeId) == "table" then -- not found or ambiguous
		utils.errorResolveType( typedb, node.line, resolveContextTypeId, seekContext, typeName)
	end
	for _,item in ipairs(items) do if typedb:type_nof_parameters( item) == 0 then return item end end
	utils.errorMessage( node, string.format( "Failed to find type '%s %s' without parameter", typedb:type_string(resolveContextTypeId), typeName))
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
		log_call( "typesystem.definition", context, {pass=pass,pass_selected=pass_selected})
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
	log_call( "typesystem.classdef", context, descr)
	utils.traverse( typedb, node, classContext, 1)	-- 1st pass: type definitions
	utils.traverse( typedb, node, classContext, 2)	-- 2nd pass: member variables
	utils.traverse( typedb, node, classContext, 3)	-- 3rd pass: declarations
	utils.traverse( typedb, node, classContext, 4)	-- 4th pass: implementations
end
function typesystem.funcdef( node, context, pass)
	local typnam = node.arg[1].value
	if not pass or pass == 1 then
		local rtype = table.unpack( utils.traverseRange( typedb, node, {2,2,1}, context))
		local param = utils.traverseRange( typedb, node, {3,3,1}, context, 1)	-- 1st pass: function declaration
		local descr = {name=typnam,rtype=rtype,param=param}
		utils.allocNodeData( node, localDefinitionContext, descr)
		log_call( "typesystem.funcdef.declaration", context, {name=typnam, rtype=utils.typeString(typedb,rtype), param=utils.typeListString(typedb,param)})
	end
	if not pass or pass == 2 then
		local descr = utils.getNodeData( node, localDefinitionContext)
		utils.traverseRange( typedb, node, {3,3,1}, context, 2)			-- 2nd pass: function implementation
		log_call( "typesystem.funcdef.implementation", context, {name=descr.name, rtype=utils.typeString(typedb,descr.rtype), param=utils.typeListString(typedb,descr.param)})
	end
end
function typesystem.callablebody( node)
end
function typesystem.main_procdef( node)
end
function typesystem.paramdeflist( node)
end
function typesystem.paramdef( node)
end
function typesystem.codeblock( node)
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
	log_call( "typesystem.vardef", context, {name=typedb:type_string(datatype),name=varnam,value=mewa.tostring(initval)})
end
function typesystem.structure( node)
end
function typesystem.variable( node)
end
function typesystem.constant( node, decl)
end
function typesystem.string_constant( node)
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

