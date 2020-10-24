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

function typesystem.vardef( node) end
function typesystem.vardef_assign( node) end
function typesystem.vardef_array( node) end
function typesystem.vardef_array_assign( node) end
function typesystem.operator( node, opdescr) end
function typesystem.stm_expression( node) end
function typesystem.stm_return( node) end
function typesystem.conditional_if( node) end
function typesystem.conditional_while( node) end

function typesystem.namespaceref( node) end
function typesystem.typedef( node) end
function typesystem.typespec( node, typedescr) end
function typesystem.funcdef( node) end
function typesystem.procdef( node) end
function typesystem.paramdef( node) end

function typesystem.program( node)
	initFirstClassCitizens()
	-- return mewa.traverse( arg)
	return node.arg
end

return typesystem

