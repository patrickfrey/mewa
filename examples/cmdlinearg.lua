require( 'pl')
require( 'string')
lapp = require( 'pl.lapp')

function nilIfEmpty( arg)
        if arg == "" then return nil else return arg end
end

return {
        parse = function( langname, typesystem, arg)
                local args = lapp( string.format( [[
Compiler for language %s
	-d,--debug  (default "") Optional file path (or stderr, resp. stdout) to write the debug output to
        -o,--output (default "") Optional file path to write the output to (stdout if not specified)
	-t,--target (default "x86_64-pc-linux-gnu") Optional compile target
	<input> (string) File with source to compile
]], langname))
                return {input=args.input, output=nilIfEmpty( args.output), debug=nilIfEmpty( args.debug), target=args.target}
        end
}

