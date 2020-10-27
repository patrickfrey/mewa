local mewa = require "mewa"
local fcc = require "fcc_language1"
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
		return node
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

function typesystem.vardef( node)
	-- local tp_name = typedescr(node.arg[1].value)
	-- local var_name = node.arg[2].value
	-- local tp = typedb:def_type( 0, tp_name, vv)

	return visit( node)
end
function typesystem.vardef_assign( node) return visit( node) end
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
	local type = typedb:resolve_type( 0, typedescr(node.arg[1].value), tagmask_resolveType)
	-- return typedescr(node.arg[1].value)
	-- return visit( node)
	return {name=node.call.name, step=node.step, arg=traverse( node), type=type}
end
function typesystem.funcdef( node) return visit( node) end
function typesystem.procdef( node) return visit( node) end
function typesystem.paramdef( node) return visit( node) end

function typesystem.program( node)
	initFirstClassCitizens()
	return traverse( node)
end

return typesystem

