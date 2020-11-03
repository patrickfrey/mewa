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
local tag_typeDeclaration = 2
local tag_typeNamespace = 3
local tagmask_resolveType = typedb.reduction_tagmask( tag_typeDeduction, tag_typeDeclaration)
local tagmask_typeNameSpace = typedb.reduction_tagmask( tag_typeNamespace)

function ident( arg)
	return arg
end

function loadConstructor( loadfmt)
	return function( adr) 
		local code,result = utils.positional_format( loadfmt, {[1] = adr}, typedb:get_instance( "register"), 2)
		return {code = code_load, out = result_load}
	end
end

local variableConstructors = {}
local codeMap = {}
local codeKey = nil
local codeKeyCnt = 0

function allocCodeKey()
	codeKeyCnt = codeKeyCnt + 1
	return codeKeyCnt
end

function openCode( key)
	local rt = codeKey
	codeKey = key or allocCodeKey()
	return rt
end

function closeCode( oldKey)
	local rt = codeMap[ codeKey]
	codeMap[ codeKey] = nil
	codeKey = oldKey
end

function printCodeLine( codeln)
	codeMap[ codeKey] = codeMap[ codeKey] .. codeln .. "\n"
end


function initFirstClassCitizens()
	function getVariableConstructors( type, reftype, descr)
		return {
			def_local = function( name, initval, register)
				local fmt; if initval then fmt = descr.def_local_val else fmt = descr.def_local end
				local code,result = utils.positional_format( fmt, {[2] = initval}, register, 1)
				local var = typedb:def_type( 0, name, result)
				typedb:def_reduction( type, var, {out = result}, tag_typeDeclaration)
				typedb:def_reduction( reftype, var, {out = result}, tag_typeDeduction)
				return code
			end,
			def_global = function( name, initval)
				local fmt; if initval then fmt = descr.def_global_val else fmt = descr.def_global end
				local adr = "@" .. utils.mangleVariableName(name)
				local code,result = utils.positional_format( fmt, {[1] = adr, [2] = initval}, 1)
				local var = typedb:def_type( 0, name, result)
				typedb:def_reduction( type, var, loadConstructor( descr.load), tag_typeDeclaration)
				typedb:def_reduction( reftype, var, {out = adr}, tag_typeDeduction)
				return code
			end
		}
	end
	for kk, vv in pairs( fcc) do
		local lvalue = typedb:def_type( 0, kk, vv)
		local const_lvalue = typedb:def_type( 0, "const " .. kk, vv)
		local rvalue = typedb:def_type( 0, "&" .. kk, vv)
		local const_rvalue = typedb:def_type( 0, "const&" .. kk, vv)
		variableConstructors[ lvalue] = getVariableConstructors( lvalue, rvalue, vv)
		variableConstructors[ const_lvalue] = getVariableConstructors( const_lvalue, const_rvalue, vv)
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

function defineVariable( line, typeId, varName, initVal)
	vc = variableConstructors[ typeId]
	if not vc then errorMessage( line, "Can't define variable of type '%s'", typedb:type_string(typeId)) end

	local register = typedb:get_instance( "register")
	local defcontext = typedb:get_instance( "defcontext")
	if register then
		if not v.def_local then errorMessage( line, "Can't define variable of type '%s'", typedb:type_string(typeId)) end
		printCodeLine( v.def_local( varName, initVal, register))
	elseif defcontext then
		errorMessage( line, "Substructures not implemented yet")
	else
		if not v.def_global then errorMessage( line, "Can't define variable of type '%s'", typedb:type_string(typeId)) end
		printCodeLine( v.def_global( varName, initVal))
	end
end

function typesystem.vardef( node)
	local typeId = utils.traverse( typedb, node.arg[1])
	defineVariable( node.line, typeId, node.arg[2].value)
end

function typesystem.vardef_assign( node)
	local typeId = utils.traverse( typedb, node.arg[1])
	defineVariable( node.line, typeId, node.arg[2].value, utils.traverse( typedb, node.arg[2]))
end

function typesystem.vardef_array( node) return utils.visit( typedb, node) end
function typesystem.vardef_array_assign( node) return utils.visit( typedb, node) end
function typesystem.operator( node, opdescr) return utils.visit( typedb, node) end
function typesystem.stm_expression( node) return utils.visit( typedb, node) end
function typesystem.stm_return( node) return utils.visit( typedb, node) end
function typesystem.conditional_if( node) return utils.visit( typedb, node) end
function typesystem.conditional_while( node) return utils.visit( typedb, node) end
function typesystem.typedef( node) return utils.visit( typedb, node) end

function typesystem.typespec( node, qualifier)
	typeName = qualifier .. node.arg[ #node.arg].value;
	local typeId
	if #node.arg == 1 then
		typeId = typedb:resolve_type( getContextTypes(), typeName, tagmask_typeNameSpace)
		if not typeId or type(typeId) == "table" then errorResolveType( typedb, node.line, typeId, getContextTypes(), typeName) end
	else
		contextTypeId = typedb:resolve_type( getContextTypes(), node.arg[ 1].value, tagmask_typeNameSpace)
		if not contextTypeId or type(contextTypeId) == "table" then errorResolveType( typedb, node.line, contextTypeId, getContextTypes(), typeName) end
		if #node.arg > 2 then
			for ii = 2, #node.arg-1, 1 do
				typeId = typedb:resolve_type( contextTypeId, node.arg[ ii].value, tagmask_typeNameSpace)
				if not typeId or type(typeId) == "table" then errorResolveType( typedb, node.line, typeId, contextTypeId, typeName) end
				contextTypeId = typeId
			end
		end
		typeId = typedb:resolve_type( contextTypeId, typeName, tagmask_typeNameSpace)
		if not typeId or type(typeId) == "table" then errorResolveType( typedb, node.line, typeId, contextTypeId, typeName) end
	end
	return typeId
end
function typesystem.funcdef( node) local cd = openCode(); local rt = utils.visit( typedb, node); printCodeLine( closeCode( cd)); return rt; end
function typesystem.procdef( node) local cd = openCode(); local rt = utils.visit( typedb, node); printCodeLine( closeCode( cd)); return rt; end
function typesystem.paramdef( node) return utils.visit( typedb, node) end

function typesystem.program( node)
	initFirstClassCitizens()
	openCode( "program")
	local rt = utils.visit( typedb, node)
	print( closeCode( cd))
	return rt;
end

return typesystem

