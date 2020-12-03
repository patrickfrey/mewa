--- Utility functions for the typesystem module

-- Module object with all functions exported
local M = {}

-- Encode a name
function M.encodeName( name)
	return name:gsub("([*])", "$")
end

-- Create a unique name
local uniqueNameIdxMap = {}
function M.uniqueName( prefix)
	if uniqueNameIdxMap[ prefix] then uniqueNameIdxMap[ prefix] = uniqueNameIdxMap[ prefix] + 1 else uniqueNameIdxMap[ prefix] = 1 end
	return prefix .. uniqueNameIdxMap[ prefix]
end

-- Map template for LLVM Code synthesis
function M.constructor_format( fmt, argtable, allocator)
	local valtable = {}
	local subst = function( match)
		local index = tonumber( match)
		if index then
			if valtable[ index] then
				return valtable[ index]
			elseif allocator then
				local rr = allocator()
				valtable[ index] = rr
				return rr
			else
				M.errorMessage( 0, "Can't build constructor for '%s', having unbound enumerated variable '%d' and no allocator defined", fmt, index)
			end
		elseif argtable[ match] then
			return argtable[ match]
		else
			M.errorMessage( 0, "Can't build constructor for '%s', having unbound variable '%s' (argument table %s)", fmt, match, mewa.tostring(argtable,false))
		end
	end
	return fmt:gsub("[{]([_%d%w]*)[}]", subst)
end

-- Map a LLVM Code synthesis template to a template substituting only the defined arguments and leaving the rest of the substitutions occurring untouched
function M.template_format( fmt, arg )
	if type( fmt) == "table" then
		local rt = {}
		for kk,vv in pairs( fmt) do
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

-- Name allocators for LLVM
function M.name_allocator( prefix)
        local i = 0
        return function ()
                i = i + 1
                return prefix .. i
        end
end

function M.label_allocator()
	return M.name_allocator( "L")
end

function M.register_allocator()
	return M.name_allocator( "%r")
end

-- Tree traversal:
-- Call tree node callback function for subnodes
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

-- Tree traversal, define scope/step and do the traversal call 
function M.traverse( typedb, node, context)
	local rt = nil
	if (node.scope) then
		local parent_scope = typedb:scope( node.scope)
		rt = M.traverseCall( node, context)
		typedb:scope( parent_scope)
	elseif (node.step) then
		local prev_step = typedb:step( node.step)
		rt = M.traverseCall( node, context)
		typedb:step( prev_step)
	else
		rt = M.traverseCall( node, context)
	end
	return rt
end

-- [3] Node visitor doing a traversal and returning the tree structure with the name and scope of the node
function M.abstractSyntaxTree( typedb, node)
	return {name=node.call.name, scope=node.scope, arg=M.traverse( typedb, node)}
end

-- Types in readable form for messages
-- Type as string
function M.typeString( typedb, typeId)
	if typeId == 0 then
		return "<>"
	elseif type(typeId) == "table" then
		return typedb:type_string( typeId.type)
	else
		return typedb:type_string( typeId)
	end
end

-- Type list as string
function M.typeListString( typedb, typeList, separator)
	if not typeList or #typeList == 0 then return "" end
	local rt = M.typeString( typedb, typeList[ 1])
	for ii=2,#typeList do
		rt = rt .. separator .. M.typeString( typedb, typeList[ ii])
	end
	return rt
end

-- Type to resolve as string for errir messages
function M.resolveTypeString( typedb, contextType, typeName)
	local rt
	if type(contextType) == "table" then
		if #contextType == 0 then
			rt = "{} " .. typeName
		else
			rt = "{ " .. M.typeString( typedb, contextType[1])
			for ii = 2,#contextType,1 do
				rt = ", " .. M.typeString( typedb, contextType[ ii])
			end
			rt = rt .. " } " .. typeName
		end
	else
		rt = M.typeString( typedb, contextType) .. " " .. typeName
	end
	return rt;
end

-- Error reporting:
-- Exit with error message and line info
function M.errorMessage( line, fmt, ...)
	local arg = {...}
	if line ~= 0 then
		error( "Error on line " .. line .. ": " .. string.format( fmt, unpack(arg)))
	else
		error( string.format( fmt, unpack(arg)))
	end
end

-- Exit with error for no result or an ambiguous result returned by typedb:resolve_type
function M.errorResolveType( typedb, line, resultContextTypeId, contextType, typeName)
	local resolveTypeString = M.resolveTypeString( typedb, contextType, typeName)
	if not resultContextTypeId then
		M.errorMessage( line, "Failed to resolve '%s'", resolveTypeString)
	elseif type(typeId) == "table" then
		local t1 = typedb:type_string( resultContextTypeId[1])
		local t2 = typedb:type_string( resultContextTypeId[2])
		M.errorMessage( line, "Ambiguous reference resolving '%s': %s, %s", resolveTypeString, t1, t2)
	end
end

-- Unicode support, function taken from http://lua-users.org/wiki/LuaUnicode
function M.utf8to32( utf8str)
	local res, seq, val = {}, 0, nil
	for i = 1, #utf8str do
		local c = string.byte(utf8str, i)
		if seq == 0 then
			table.insert(res, val)
			seq = c < 0x80 and 1 or c < 0xE0 and 2 or c < 0xF0 and 3 or
			      c < 0xF8 and 4 or --c < 0xFC and 5 or c < 0xFE and 6 or
				  error("invalid UTF-8 character sequence")
			val = bit32.band(c, 2^(8-seq) - 1)
		else
			val = bit32.bor(bit32.lshift(val, 6), bit32.band(c, 0x3F))
		end
		seq = seq - 1
	end
	table.insert(res, val)
	table.insert(res, 0)
	return res
end

-- Debug function that returns the tree with all resolve type paths
function M.getResolveTypeTreeDump( typedb, contextType, typeName, parameters, tagmask)
	function getResolveTypeTree_rec( typedb, contextType, typeName, parameters, tagmask, indentstr, visited)
		local typeid = typedb:get_type( contextType, typeName, parameters)
		if typeid then
			return " " .. typeName .. " MATCH"
		else
			local rt = ""; if indentstr ~= "" then rt = rt .. "\n" end
			table.insert( visited, typeid)
			local rdlist = typedb:get_reductions( contextType, tagmask)
			for ri,rd in ipairs(rdlist) do
				local in_visited = nil; for vi,ve in ipairs(visited) do if ve == rd.type then in_visited = true; break end end
				if not in_visited then
					rt = rt .. indentstr .. rd.weight .. ": " .. typedb:type_string( rd.type)
					rt = rt .. getResolveTypeTree_rec( typedb, rd.type, typeName, parameters, tagmask, indentstr .. "  ", visited)
				end
			end
			table.remove( visited, #visited)
			return rt
		end
	end
	return getResolveTypeTree_rec( typedb, contextType, typeName, parameters, tagmask, "", {})
end

function treeToString( typedb, node, indentstr, node_tostring)
	rt = ""
	local scope = node:scope()
	rt = rt .. string.format( "%s[%d,%d]:", indentstr, scope[1], scope[2]) .. "\n"
	for elem in node:list() do
		rt = rt .. string.format( "%s  - %s", indentstr, node_tostring(elem)) .. "\n"
	end
	for chld in node:chld() do
		rt = rt .. treeToString( typedb, chld, indentstr .. "  ", node_tostring)
	end
	return rt
end

function M.getTypeTreeDump( typedb)
	function node_tostring(nd)
		return string.format( "%s : %s", typedb:type_string(nd), mewa.tostring(typedb:type_constructor(nd),false))
	end
	return treeToString( typedb, typedb:type_tree(), "", node_tostring)
end
function M.getReductionTreeDump( typedb)
	function node_tostring(nd)
		return string.format( "%s <- %s [%d]/%.3f : %s",
					typedb:type_string(nd.to), typedb:type_string(nd.from), nd.tag, nd.weight,
					mewa.tostring(nd.constructor,false))
	end
	return treeToString( typedb, typedb:reduction_tree(), "", node_tostring)
end

return M

