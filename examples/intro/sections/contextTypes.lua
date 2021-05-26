local utils = require "typesystem_utils"

-- Get the context type for type declarations
function getDeclarationContextTypeId( context)
    if context.domain == "local" then return localDefinitionContext
    elseif context.domain == "member" then return context.decltype
    elseif context.domain == "global" then return 0
    end
end
-- Get an object instance and clone it if it is not stored in the current scope,
--   making it possible to add elements to an inherited instance
function thisInstanceTableClone( name, emptyInst)
    local inst = typedb:this_instance( name)
    if not inst then
        inst = utils.deepCopyStruct( typedb:get_instance( name) or emptyInst)
        typedb:set_instance( name, inst)
    end
    return inst
end
-- Get the list of seek context types associated with the current scope
function getSeekContextTypes()
    return typedb:get_instance( seekContextKey) or {0}
end
-- Push an element to the current seek context type list
function pushSeekContextType( val)
    table.insert( thisInstanceTableClone( seekContextKey, {0}), val)
end
-- Remove the last element of the the list of seek context types
function popSeekContextType( val)
    local seekctx = typedb:this_instance( seekContextKey)
    if not seekctx or seekctx[ #seekctx] ~= val then
        utils.errorMessage( 0, "Corrupt definition context stack")
    end
    table.remove( seekctx, #seekctx)
end
