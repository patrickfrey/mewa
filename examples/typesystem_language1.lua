local mewa = require "mewa"
local fcc = require "fcc_language1"
local utils = require "typesystem_utils"
local constexpr = require "typesystem_constexpr"

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

function unaryOpConstructor( fmt)
	return function( constructor)
		local code,result = utils.positional_format( fmt, {[1] = constructor.out}, typedb:get_instance( "register"), 2)
		return {code = constructor.code .. code_load, out = result}
	end
end
function storeConstructor( fmt, adr)
	return function( constructor)
		local code,result = utils.positional_format( fmt, {[1] = adr, [2] = constructor.out}, typedb:get_instance( "register"), 2)
		return {code = constructor.code .. code_load, out = result}
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
	io.stderr:write( string.format( "+++ OPEN %s\n", codeKey))
	return rt
end

function closeCode( oldKey)
	local rt = codeMap[ codeKey]
	codeMap[ codeKey] = nil
	io.stderr:write( string.format( "+++ CLOSE %s => %s\n", codeKey, oldKey or "nil"))
	codeKey = oldKey
	return rt
end

function printCodeLine( codeln)
	if codeln then
		io.stderr:write( string.format( "+++ CODE %s:\n%s\n", codeKey, codeln))

		if codeMap[ codeKey] then
			codeMap[ codeKey] = codeMap[ codeKey] .. codeln .. "\n"
		else
			codeMap[ codeKey] = codeln .. "\n"
		end
	end
end


function initFirstClassCitizens()
	function getVariableConstructors( type, reftype, descr)
		return {
			def_local = function( name, initval, register)
				local fmt; if initval then fmt = descr.def_local_val else fmt = descr.def_local end
				local code,result = utils.positional_format( fmt, {[2] = initval}, register, 1)
				local var = typedb:def_type( 0, name, result)
				typedb:def_reduction( type, var, {code = code, out = result}, tag_typeDeclaration)
				typedb:def_reduction( reftype, var, {code = code, out = result}, tag_typeDeduction)
				return code
			end,
			def_global = function( name, initval)
				local fmt; if initval then fmt = descr.def_global_val else fmt = descr.def_global end
				local adr = "@" .. utils.mangleVariableName(name)
				local code,result = utils.positional_format( fmt, {[1] = adr, [2] = initval}, nil, 1)
				local var = typedb:def_type( 0, name, {code = "", out = result} )
				typedb:def_reduction( type, var, loadConstructor( descr.load), tag_typeDeclaration)
				typedb:def_reduction( reftype, var, {code = "", out = adr}, tag_typeDeduction)
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

		local assign_type = typedb:def_type( rvalue, "_assign", storeConstructor( vv.store,  typedb:type_constructor(rvalue).out))
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

function defineVariable( line, typeId, varName, initVal)
	vc = variableConstructors[ typeId]
	if not vc then utils.errorMessage( line, "Can't define variable of type '%s'", typedb:type_string(typeId)) end

	local register = typedb:get_instance( "register")
	local defcontext = typedb:get_instance( "defcontext")
	if register then
		if not vc.def_local then utils.errorMessage( line, "Can't define variable of type '%s'", typedb:type_string(typeId)) end
		printCodeLine( vc.def_local( varName, initVal, register))
	elseif defcontext then
		utils.errorMessage( line, "Substructures not implemented yet")
	else
		if not vc.def_global then utils.errorMessage( line, "Can't define variable of type '%s'", typedb:type_string(typeId)) end
		printCodeLine( vc.def_global( varName, initVal))
	end
end

function selectVariableType( node, typeName, resultContextTypeId, reductions, items)
	if not resultContextTypeId or type(resultContextTypeId) == "table" then errorResolveType( typedb, node.line, resultContextTypeId, getContextTypes(), typeName) end
	for ii,item in ipairs(items) do
		if typedb:type_nof_parameters( item.type) == 0 then return item.type end
	end
	errorResolveType( typedb, node.line, resultContextTypeId, getContextTypes(), typeName)
end

function typesystem.vardef( node)
	local subnode = utils.traverse( typedb, node)
	defineVariable( node.line, subnode[1], subnode[2], nil)
end

function typesystem.vardef_assign( node)
	local subnode = utils.traverse( typedb, node)
	defineVariable( node.line, subnode[1], subnode[2], subnode[3])
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
		typeId = selectVariableType( node, typeName, typedb:resolve_type( getContextTypes(), typeName, tagmask_typeNameSpace))
	else
		typeId = selectVariableType( node, typeName, typedb:resolve_type( getContextTypes(), node.arg[ 1].value, tagmask_typeNameSpace))
		if #node.arg > 2 then
			for ii = 2, #node.arg-1, 1 do
				typeId = selectVariableType( node, typeName, typedb:resolve_type( contextTypeId, node.arg[ ii].value, tagmask_typeNameSpace))
				contextTypeId = typeId
			end
		end
		typeId = selectVariableType( node, typeName, typedb:resolve_type( contextTypeId, typeName, tagmask_typeNameSpace))
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

