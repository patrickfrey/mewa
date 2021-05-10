local utils = require "typesystem_utils"

-- Create a callable environent object
function createCallableEnvironment( node, name, rtype, rprefix, lprefix)
	if rtype then
		local descr = typeDescriptionMap[ rtype]
		if not descr.scalar and not descr.class == "pointer" then
			utils.errorMessage( node.line, "Only scalar types can be return values")
		end
	end
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
-- Set some variables needed in a class method implementation body
function expandMethodEnvironment( node, context, descr, env)
	local selfTypeId = referenceTypeMap[ context.decltype]
	local classvar = defineImplicitVariable( node, selfTypeId, "self", "%ths")
	pushSeekContextType( {type=classvar, constructor={out="%ths"}})
end
