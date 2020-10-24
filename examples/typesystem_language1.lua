local mewa = require "mewa"
local fcc = require "fcc_language1"
local typedb = mewa.typedb()
local typesystem = {
	puretype = {},
	consttype = {},
	reftype = {},
	constreftype = {},
	ptrtype = {},
	constptrtype = {},
	ptrreftype = {},
	constptrreftype = {},
	movereftype = {},

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

function initFirstClassCitizens()
	for kk, vv in pairs( fcc.constructor) do
		typedb:def_type( {0,-1}, 0, kk, vv)
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

function typesystem.vardef( node) return {name=node.call.name, arg=traverse( node)} end
function typesystem.vardef_assign( node) return {name=node.call.name, arg=traverse( node)} end
function typesystem.vardef_array( node) return {name=node.call.name, arg=traverse( node)} end
function typesystem.vardef_array_assign( node) return {name=node.call.name, arg=traverse( node)} end
function typesystem.operator( node, opdescr) return {name=node.call.name, arg=traverse( node)} end
function typesystem.stm_expression( node) return {name=node.call.name, arg=traverse( node)} end
function typesystem.stm_return( node) return {name=node.call.name, arg=traverse( node)} end
function typesystem.conditional_if( node) return {name=node.call.name, arg=traverse( node)} end
function typesystem.conditional_while( node) return {name=node.call.name, arg=traverse( node)} end

function typesystem.namespaceref( node) return {name=node.call.name, arg=traverse( node)} end
function typesystem.typedef( node) return {name=node.call.name, arg=traverse( node)} end
function typesystem.typespec( node, typedescr) return {name=node.call.name, arg=traverse( node)} end
function typesystem.funcdef( node) return {name=node.call.name, arg=traverse( node)} end
function typesystem.procdef( node) return {name=node.call.name, arg=traverse( node)} end
function typesystem.paramdef( node) return {name=node.call.name, arg=traverse( node)} end

function typesystem.program( node)
	initFirstClassCitizens()
	return traverse( node)
end

return typesystem

