require( 'pl')
lapp = require( 'pl.lapp')

function nilIfEmpty( arg)
	if arg == "" then return nil else return arg end
end

return {
	parseArguments = function( arg)
	local args = lapp [[
		Compiler for language1
			-d,--debug  (default "") Optional file path (or stderr, resp. stdout) to write the debug output to
			-o,--output (default "") Optional file path to write the output to (stdout if not specified)
			<input> (string) File with source to compile
		]]
		return args.input,nilIfEmpty( args.output),nilIfEmpty( args.debug)
	end,

	assign = {},
	assign_add = {},
	assign_sub = {},
	assign_mul = {},
	assign_div = {},
	assign_mod = {},
	logical_or = {},
	logical_and = {},
	bitwise_or = {},
	bitwise_xor = {},
	bitwise_and = {},
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
	cmpgt = {},

	puretype = {},
	consttype = {},
	reftype = {},
	constreftype = {},
	ptrtype = {},
	constptrtype = {},
	ptrreftype = {},
	constptrreftype = {},
	movereftype = {},

	vardef = function( arg) end,
	vardef_assign = function( arg) end,
	vardef_array = function( arg) end,
	vardef_array_assign = function( arg) end,
	operator = function( arg) end,
	stm_expression = function( arg) end,
	stm_return = function( arg) end,
	conditional_if = function( arg) end,
	conditional_while = function( arg) end,

	namespaceref = function( arg) end,
	typedef = function( arg) end,
	typespec = function( arg) end,
	funcdef = function( arg) end,
	procdef = function( arg) end,
	program = function( arg) return arg end
}

