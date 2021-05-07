local utils = require "typesystem_utils"

-- Create a callable environent object
function createCallableEnvironment( node, name, rtype, rprefix, lprefix)
	return {name=name, line=node.line, scope=typedb:scope(),
		register=utils.register_allocator(rprefix), label=utils.label_allocator(lprefix), returntype=rtype}
end
-- Attach a newly created data structure for a callable to its scope
function defineCallableEnvironment( node, name, rtype)
	local env = createCallableEnvironment( node, name, rtype)
	if typedb:this_instance( callableEnvKey) then utils.errorMessage( node.line, "Internal: Callable environment defined twice: %s %s", name, mewa.tostring({(typedb:scope())})) end
	typedb:set_instance( callableEnvKey, env)
	return env
end
-- Get the active callable instance
function getCallableEnvironment()
	return instantCallableEnvironment or typedb:get_instance( callableEnvKey)
end
