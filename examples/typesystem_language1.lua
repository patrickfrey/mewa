local mewa = require "mewa"
local fcc = require "fcc_language1"
local nameMangling = require "nameMangling"

local typedb = mewa.typedb()
local typesystem = {
	pure_type = function( nm) return "@ " .. nm end,
	const_type = function( nm) return "const " .. nm end,
	ref_type = function( nm) return "& " .. nm end,
	const_ref_type = function( nm) return "const& " .. nm end,
	ptr_type = function( nm) return "^ " .. nm end,
	const_ptr_type = function( nm) return "const^ " .. nm end,
	ref_ptr_type = function( nm) return "^& " .. nm end,
	const_ref_ptr_type = function( nm) return "const^& " .. nm end,
	move_ref_type = function( nm) return "&& " .. nm end,

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

function ident( arg)
	return arg
end

function initFirstClassCitizens()
	for kk, vv in pairs( fcc.constructor) do
		local lvalue = typedb:def_type( 0, typesystem.pure_type(kk), vv)
		local const_lvalue = typedb:def_type( 0, typesystem.const_type(kk), vv)
		typedb:def_reduction( const_lvalue, lvalue, ident, tag_typeDeduction)
	end
end

function compileError( line, msg)
	error( "Error on line " .. line .. ": " .. msg)
end

function traverse( node, context)
	if node.arg then
		local rt = {}
		for ii, vv in ipairs( node.arg) do
			local subnode = node.arg[ ii]
			if subnode.call then
				if subnode.call.obj then
					rt[ ii] = subnode.call.proc( subnode, subnode.call.obj, context)
				else
					rt[ ii] = subnode.call.proc( subnode, context)
				end
			else
				rt[ ii] = subnode
			end
		end
		return rt
	else
		return node.value
	end
end

function visit( node)
	local rt = nil
	if (node.scope) then
		local parent_scope = typedb:scope( node.scope)
		rt = {name=node.call.name, scope=node.scope, arg=traverse( node)}
		typedb:scope( parent_scope)
	elseif (node.step) then
		local prev_step = typedb:step( node.step)
		rt = {name=node.call.name, step=node.step, arg=traverse( node)}
		typedb:step( prev_step)
	else
		rt = {name=node.call.name, step=node.step, arg=traverse( node)}
	end
	return rt
end

local globals = {}

function mapConstructorTemplate( template, struct)
	local rt
	for elem in ipairs(template) do
		rt = rt .. (struct[ elem] or elem)
	end
	return rt;
end

function mangledName( name)
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

function defineVariable( type_name, var_name, initval)
	local typeid,reductions,itemlist = typedb:resolve_type( getContextTypes(), var_name, tagmask_resolveType)
	if not typeid then
		compileError( node.line, "Failed to resolve type " .. type_name .. ", declaring variable " .. var_name)
	elseif type(typeid) == "table" then
		compileError( node.line, "Ambiguous reference to type " .. type_name .. ", found candidate match "
					.. typedb:type_string(typeid[1]) .. " and " .. typedb:type_string(typeid[2]))
	else
		local constructor = nil
		if isGlobalContext() then
			adr = "@" .. nameMangling.mangleName( "=" .. var_name)
			defaultval = typedb:type_constructor( typeid)[ "default"]
			constructor = mapConstructorTemplate( typedb:type_constructor( typeid)[ "def_global"], { ["/ADR"] = adr, ["/VAL"] = initval or defaultval })
		end
	end
	local tp = typedb:def_type( getDefContextType(), type_name, vv)
end

function typesystem.vardef( node)
	local type_name = visit( node.arg[1])
	defineVariable( traverse( node.arg[1]), node.arg[2].value)
end

function typesystem.vardef_assign( node)
	local type_name = visit( node.arg[1])
	defineVariable( traverse( node.arg[1]), node.arg[2].value, traverse(node.arg[2]))
end

function typesystem.vardef_array( node) return visit( node) end
function typesystem.vardef_array_assign( node) return visit( node) end
function typesystem.operator( node, opdescr) return visit( node) end
function typesystem.stm_expression( node) return visit( node) end
function typesystem.stm_return( node) return visit( node) end
function typesystem.conditional_if( node) return visit( node) end
function typesystem.conditional_while( node) return visit( node) end

function typesystem.namespaceref( node) return visit( node) end
function typesystem.typedef( node) return visit( node) end
function typesystem.typespec( node, typedescr)
	return typedescr( node.arg[1].value)
end
function typesystem.funcdef( node) return visit( node) end
function typesystem.procdef( node) return visit( node) end
function typesystem.paramdef( node) return visit( node) end

function typesystem.program( node)
	initFirstClassCitizens()
	return traverse( node)
end

return typesystem

