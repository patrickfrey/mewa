mewa = require "mewa"
typedb = mewa.typedb()

-- A register allocator as a generator function counting from 1, returning the LLVM register identifiers:
function register_allocator()
	local i = 0
	return function ()
		i = i + 1
		return "%R" .. i
	end
end

-- Attach a data structure for a callable to its scope
function defineCallableEnvironment( name)
	local env = {name=name, scope=(typedb:scope()), register=register_allocator()}
	if typedb:this_instance( "callenv") then error( "Callable environment defined twice") end
	typedb:set_instance( "callenv", env)
	return env
end
-- Get the active callable instance
function getCallableEnvironment()
	return typedb:get_instance( "callenv")
end

typedb:scope( {1,100} )
defineCallableEnvironment( "function X")
typedb:scope( {20,30} )
defineCallableEnvironment( "function Y")
typedb:scope( {70,90} )
defineCallableEnvironment( "function Z")

typedb:step( 10)
print( "We are in " .. typedb:get_instance( "callenv").name)
typedb:step( 25)
print( "We are in " .. typedb:get_instance( "callenv").name)
typedb:step( 85)
print( "We are in " .. typedb:get_instance( "callenv").name)

