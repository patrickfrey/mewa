--- Utility functions for the typesystem module

function char_to_hex( c)
	return string.format("_%02X", string.byte(c))
end

function hex_to_char( x)
	return string.char( tonumber( x, 16))
end

-- Module object with all functions exported
local M = {}

-- Name mangling/demangling:
-- [1] Mangle a name
function M.mangleName( name)
	return name:gsub("([^%w _])", char_to_hex)
end

-- [2] Demangle a name
function M.demangleName( name)
	return name:gsub("_(%x%x)", hex_to_char)
end

-- [3] Mangle a variable name
function M.mangleVariableName( name)
	return M.mangleName( "=" .. name)
end

function M.positional_format( fmt, argtable, register, outindex)
	local valtable = {}
	local subst = function( match)
		local index = tonumber( match)
		if index <= #argtable then
			return argtable[ index]
		elseif valtable[ index] then
			return valtable[ index]
		else
			local rr = register()
			valtable[ index] = rr
			return rr
		end
	end
	local result = fmt:gsub("[{]([%d]*)[}]", subst)
	local outsubst = nil
	if outindex <= #argtable then
		outsubst = argtable[ index]
	else
		outsubst = valtable[ index]
	end
	return result,outsubst
end

-- Tree traversal:
-- [1] Tree traversal subnode call implementation
function M.traverseCall( node, context)
	if node.arg then
		local rt = {}
		for ii, vv in ipairs( node.arg) do
			local subnode = node.arg[ ii]
			if subnode.call then
				if subnode.call.obj then
					rt[ ii] = subnode.call.proc( subnode, subnode.call.obj, context)
				else
					rt[ ii] = subnode.call.proc( subnode, context)
				end
			else
				rt[ ii] = subnode
			end
		end
		return rt
	else
		return node.value
	end
end

-- [2] Tree traversal implementation, define scope/step and do the traversal call 
function M.traverse( typedb, node, context)
	local rt = nil
	if (node.scope) then
		local parent_scope = typedb:scope( node.scope)
		rt = M.traverseCall( node)
		typedb:scope( parent_scope)
	elseif (node.step) then
		local prev_step = typedb:step( node.step)
		rt = M.traverseCall( node)
		typedb:step( prev_step)
	else
		rt = M.traverseCall( node)
	end
	return rt
end

-- [3] Default node visitor doing a traversal and returning the tree structure with the name and scope of the node
function M.visit( typedb, node)
	return {name=node.call.name, scope=node.scope, arg=M.traverse( typedb, node)}
end


-- Error reporting:
-- [1] General error
function M.errorMessage( line, fmt, ...)
	error( "Error on line " .. line .. ": " .. string.format( fmt, unpack(arg)))
end

function typeString( typedb, typeId)
	if typeId == 0 then
		return "<unbound>"
	else
		return typedb:type_string( typeId)
	end
end

-- [2] Exit with error for no result or an ambiguous result returned by typedb:resolve_type
function M.errorResolveType( typedb, line, typeId, contextType, typeName)
	local resolveTypeString
	if type(contextType) == "table" then
		resolveTypeString = "{ " .. typeString( typedb, contextType[1])
		for ii = 2,#contextType,1 do
			resolveTypeString = ", " .. typeString( typedb, contextType[1])
		end
		resolveTypeString = resolveTypeString .. " } => " .. typeName
	else
		resolveTypeString = typeString( typedb, contextType) .. " => " .. typeName
	end
	if not typeId then
		errorMessage( line, "Failed to resolve type %s", resolveTypeString)
	elseif type(typeId) == "table" then
		local t1 = typedb:type_string( typeId[1])
		local t2 = typedb:type_string( typeId[2])
		errorMessage( line, "Ambiguous reference resolving type %s: %s, %s", resolveTypeString, t1, t2)
	end
end


return M

