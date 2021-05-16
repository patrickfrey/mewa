mewa = require "mewa"
typedb = mewa.typedb()

-- Attach a data structure for a callable to its scope
function defineCallableEnvironment( name)
	local env = {name=name, scope=(typedb:scope())}
	if typedb:this_instance( "callenv") then error( "Callable environment defined twice") end
	typedb:set_instance( "callenv", env)
	return env
end
-- Get the active callable name
function getFunctionName()
	return typedb:get_instance( "callenv").name
end

typedb:scope( {1,100} )
defineCallableEnvironment( "function X")
typedb:scope( {20,30} )
defineCallableEnvironment( "function Y")
typedb:scope( {70,90} )
defineCallableEnvironment( "function Z")

typedb:step( 10)
print( "We are in " .. getFunctionName())
typedb:step( 25)
print( "We are in " .. getFunctionName())
typedb:step( 85)
print( "We are in " .. getFunctionName())

