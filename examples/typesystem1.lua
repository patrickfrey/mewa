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
	add = {},
	sub = {},
	minus = {},
	plus = {},
	mul = {},
	div = {},
	mod = {},

	operator = function( arg)
		print( "operator " .. arg)
	end
}

