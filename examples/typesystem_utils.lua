--- Utility functions for the typesystem module

-- Module object with all functions exported
local M = {}

-- Name mangling/demangling and unique name:
-- Helpers for encoding/decoding:
function char_to_hex( c)
	return string.format("_%02X", string.byte(c))
end

function hex_to_char( x)
	return string.char( tonumber( x, 16))
end

-- [1] Mangle a name
function M.mangleName( name)
	return name:gsub("([^_%w%d])", char_to_hex)
end

-- [2] Demangle a name
function M.demangleName( name)
	return name:gsub("_(%x%x)", hex_to_char)
end

-- [3] Mangle a variable name
function M.mangleVariableName( name)
	return M.mangleName( "$" .. name)
end

-- Template for LLVM Code synthesis with substitution of positional arguments in curly brackets {1},{2},{3}, ...
function M.constructor_format( fmt, argtable, register)
	local valtable = {}
	local subst = function( match)
		local index = tonumber( match)
		if index then
			if valtable[ index] then
				return valtable[ index]
			elseif register then
				local rr = register()
				valtable[ index] = rr
				return rr
			else
				M.errorMessage( 0, "Can't build constructor for %s, having unbound register variables and no register allocator defined", fmt)
			end
		elseif argtable[ match] then
			return argtable[ match]
		else
			M.errorMessage( 0, "Can't build constructor for %s, having unbound variables", fmt)
		end
	end
	return fmt:gsub("[{]([_%d%w]*)[}]", subst)
end

-- Template for LLVM Code synthesis of control structures
function M.code_format_varg( fmt, ... )
	local arg = {...}
	local subst = function( match)
		local index = tonumber( match)
		if arg[ index] then
			return arg[ index]
		else
			return ""
		end
	end
	return fmt:gsub("[{]([%d]*)[}]", subst)
end

function M.template_format( fmt, arg )
	if type( fmt) == "table" then
		rt = {}
		for kk,vv in pairs( tmt) do
			rt[ kk] = M.template_format( vv, arg)
		end
		return rt;
	else
		local subst = function( match)
			if arg[ match] then
				return arg[ match]
			else
				return "{" .. match .. "}"
			end
		end
		return fmt:gsub("[{]([_%w%d]*)[}]", subst)
	end
end

-- Register allocator for LLVM
function M.register_allocator()
        local i = 0
        return function ()
                i = i + 1
                return "%R" .. i
        end
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
				rt[ ii] = subnode.value
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

-- Types in readable form for messages
-- [1] Type as string
function typeString( typedb, typeId)
	if typeId == 0 then
		return "<>"
	elseif type(typeId) == "table" then
		return typedb:type_string( typeId.type)
	else
		return typedb:type_string( typeId)
	end
end

-- [2] Type list as string
function typeListString( typedb, typeList)
	if not typeList or #typeList == 0 then return "" end
	local rt = typeString( typedb, typeList[ 1])
	for ii=2,#typeList do
		rt = rt .. ", " .. typeString( typedb, typeList[ ii])
	end
	return rt
end

-- [2] Type to resolve as string
function M.resolveTypeString( typedb, contextType, typeName)
	local rt
	if type(contextType) == "table" then
		if #contextType == 0 then
			rt = "{} => " .. typeName
		else
			rt = "{ " .. typeString( typedb, contextType[1])
			for ii = 2,#contextType,1 do
				rt = ", " .. typeString( typedb, contextType[ ii])
			end
			rt = rt .. " } => " .. typeName
		end
	else
		rt = typeString( typedb, contextType) .. " => " .. typeName
	end
	return rt;
end

-- Error reporting:
-- [1] General error
function M.errorMessage( line, fmt, ...)
	local arg = {...}
	error( "Error on line " .. line .. ": " .. string.format( fmt, unpack(arg)))
end

-- [2] Exit with error for no result or an ambiguous result returned by typedb:resolve_type
function M.errorResolveType( typedb, line, resultContextTypeId, contextType, typeName)
	local resolveTypeString = M.resolveTypeString( typedb, contextType, typeName)
	if not resultContextTypeId then
		M.errorMessage( line, "Failed to resolve %s", resolveTypeString)
	elseif type(typeId) == "table" then
		local t1 = typedb:type_string( resultContextTypeId[1])
		local t2 = typedb:type_string( resultContextTypeId[2])
		M.errorMessage( line, "Ambiguous reference resolving %s: %s, %s", resolveTypeString, t1, t2)
	end
end


return M

