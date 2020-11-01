local mewa = require "mewa"
local fcc = require "fcc_language1"
local utils = require "typesystem_utils"

local typedb = mewa.typedb()
local typesystem = {
	assign = {},
	assign_add = {},
	assign_sub = {},
	assign_mul = {},
	assign_div = {},
	assign_mod = {},
	logical_or = {},
	logical_and = {},
	bitwise_or = {},
	bitwise_and = {},
	bitwise_xor = {},
	add = {},
	sub = {},
	minus = {},
	plus = {},
	mul = {},
	div = {},
	mod = {},

	arrow = {},
	member = {},
	ptrderef = {},
	call = {},
	arrayaccess = {},

	cmpeq = {},
	cmpne = {},
	cmple = {},
	cmplt = {},
	cmpge = {},
	cmpgt = {}
}

local tag_typeDeduction = 1
local tagmask_resolveType = typedb.reduction_tagmask( tag_typeDeduction)
local tagmask_typeNameSpace = typedb.reduction_tagmask( tag_typeDeduction)

function ident( arg)
	return arg
end

local variableConstructors = {}

function initFirstClassCitizens()
	function getVariableConstructors( type)
		return {
			def_local = function( name)
				local constructor,result = utils.positional_format( vv.def_local, {}, reg, 1)
				local var = typedb:def_type( 0, name, result)
				typedb:def_reduction( var, type, ident, tag_typeDeduction)
				return constructor,result
			end,
			def_local_val = function( name, initval)
				local constructor,result = utils.positional_format( vv.def_local_val, {[2] = initval}, reg, 1)
				local var = typedb:def_type( 0, name, result)
				typedb:def_reduction( var, type, ident, tag_typeDeduction)
				return constructor,result
			end
			def_global = function( name)
				local constructor,result = utils.positional_format( vv.def_global, {[1] = utils.mangleVariableName(name)}, reg, 1)
				local var = typedb:def_type( 0, name, result)
				typedb:def_reduction( var, type, ident, tag_typeDeduction)
				return constructor,result
			end,
			def_global_val = function( name, initval)
				local constructor,result = utils.positional_format( vv.def_global_val, {[1] = utils.mangleVariableName(name), [2] = initval}, reg, 1)
				local var = typedb:def_type( 0, name, result)
				typedb:def_reduction( var, type, ident, tag_typeDeduction)
				return constructor,result
			end
		}
	end
	for kk, vv in pairs( fcc) do
		local lvalue = typedb:def_type( 0, kk, vv)
		local const_lvalue = typedb:def_type( 0, "const " .. kk, vv)
		local reg = typedb:get_instance( "register")
		variableConstructors[ lvalue] = vc
		variableConstructors[ const_lvalue] = vc
		typedb:def_reduction( const_lvalue, lvalue, ident, tag_typeDeduction)
	end
end

local globals = {}

function mapConstructorTemplate( template, struct)
	local rt
	for elem in ipairs(template) do
		rt = rt .. (struct[ elem] or elem)
	end
	return rt;
end

function getDefContextType()
	local rt = typedb:get_instance( "defcontext")
	if rt then return rt else return 0 end
end

function getContextTypes()
	local rt = typedb:get_instance( "context")
	if rt then return rt else return {0} end
end

function isGlobalContext()
	return typedb:get_instance( "context") == nil
end

function defineVariable( line, typeId, varName, initval)
	local register = typedb:get_instance( "register")
	local defcontext = typedb:get_instance( "defcontext")
	if register then
	elseif defcontext then
		error( "Error on line " .. line .. ": Substructures not implemented yet")
	else
		adr = "@" .. utils.mangleVariableName( varName)
			defaultval = typedb:type_constructor( typeid)[ "default"]
			constructor = mapConstructorTemplate( typedb:type_constructor( typeid)[ "def_global"], { ["/ADR"] = adr, ["/VAL"] = initval or defaultval })
	end
	local tp = typedb:def_type( getDefContextType(), type_name, vv)
end

function typesystem.vardef( node)
	local typeId = utils.traverse( node.arg[1])
	defineVariable( utils.traverse( node.arg[1]), node.arg[2].value)
end

function typesystem.vardef_assign( node)
	local typeId = utils.traverse( node.arg[1])
	defineVariable( type_name, node.arg[2].value, utils.traverse(node.arg[2]))
end

function typesystem.vardef_array( node) return utils.visit( node) end
function typesystem.vardef_array_assign( node) return utils.visit( node) end
function typesystem.operator( node, opdescr) return utils.visit( node) end
function typesystem.stm_expression( node) return utils.visit( node) end
function typesystem.stm_return( node) return utils.visit( node) end
function typesystem.conditional_if( node) return utils.visit( node) end
function typesystem.conditional_while( node) return utils.visit( node) end

function typesystem.typedef( node) return utils.visit( node) end
function typesystem.typespec( node, qualifier)
	typeName = qualifier .. node.arg[ #node.arg].value;
	local typeId
	if #node.arg == 1 then
		typeId = typedb:resolve_type( getContextTypes(), typeName, tagmask_typeNameSpace)
		if not typeId or type(typeId) == "table" then errorResolveType( node.line, typeId, getContextTypes(), typeName)
	else
		contextTypeId = typedb:resolve_type( getContextTypes(), node.arg[ 1].value, tagmask_typeNameSpace)
		if not contextTypeId or type(contextTypeId) == "table" then errorResolveType( node.line, contextTypeId, getContextTypes(), typeName)
		if #node.arg > 2 then
			for ii = 2, #node.arg-1, +1 do
				typeId = typedb:resolve_type( contextTypeId, node.arg[ ii].value, tagmask_typeNameSpace)
				if not typeId or type(typeId) == "table" then errorResolveType( node.line, typeId, contextTypeId, typeName)
				contextTypeId = typeId
			end
		end
		typeId = typedb:resolve_type( contextTypeId, typeName, tagmask_typeNameSpace)
		if not typeId or type(typeId) == "table" then errorResolveType( node.line, typeId, contextTypeId, typeName)
	end
	return typeId
end
function typesystem.funcdef( node) return utils.visit( node) end
function typesystem.procdef( node) return utils.visit( node) end
function typesystem.paramdef( node) return utils.visit( node) end

function typesystem.program( node)
	initFirstClassCitizens()
	return utils.traverse( node)
end

return typesystem

