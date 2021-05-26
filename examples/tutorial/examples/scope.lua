mewa = require "mewa"
typedb = mewa.typedb()

function defineVariable( name)
	local scopestr = mewa.tostring( (typedb:scope()))
	typedb:def_type( 0, name, string.format( "instance of scope %s", scopestr))
end
function findVariable( name)
	local res,reductions,items = typedb:resolve_type( 0, name)
	if not res then return "!NOTFOUND" end
	if type(res) == "table" then return "!AMBIGUOUS" end
	for _,item in ipairs(items) do
		if typedb:type_nof_parameters(item) == 0 then
			return typedb:type_constructor(item)
		end
	end
	return "!NO MATCH"
end

typedb:scope( {1,100} )
defineVariable( "X")
typedb:scope( {10,50} )
defineVariable( "X")
typedb:scope( {20,30} )
defineVariable( "X")
typedb:scope( {70,90} )
defineVariable( "X")

typedb:step( 25)
print( "Retrieve X " .. findVariable("X"))
typedb:step( 45)
print( "Retrieve X " .. findVariable("X"))
typedb:step( 95)
print( "Retrieve X " .. findVariable("X"))
typedb:step( 100)
print( "Retrieve X " .. findVariable("X"))
