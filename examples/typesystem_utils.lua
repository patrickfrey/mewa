--- Utility functions for the typesystem module
-- Use Lua5.2 bit manipulation library
local bit32 = require "bit32"
-- Module object with all functions exported
local utils = {}

-- Encode a name
local encodeNameSubstMap = {
 	["*"]  = "$",
	[" "]  = "",
	["["]  = "$",
	["]"]  = "",
	["%"]  = ""
}
function utils.encodeName( name)
	local subst = function( match)
		return encodeNameSubstMap[ match] or ""
	end
	return (name:gsub("([%*%[%] %%])", subst))
end

-- Map Lexem values to strings encoded for LLVM IR output
local lexemLlvmSubstMap = {
	["\\a"]  = "\\07",
	["\\b"]  = "\\08",
	["\\f"]  = "\\0C",
	["\\n"]  = "\\0A",
	["\\t"]  = "\\09",
	["\\v"]  = "\\0B",
	["\\\\"] = "\\5C",
	["'"]    = "\\27",
	['"']    = "\\22"
}
function utils.encodeLexemLlvm( name)
	local len = string.len(name)
	local subst = function( match)
		len = len - 1
		return lexemLlvmSubstMap[ match] or " "
	end
	return (name:gsub("([\\][abfnrtv\\'\"])", subst)), len
end

-- Encode C strings
local cStringSubstMap = {
	["\a"]  = "\\a",
	["\b"]  = "\\b",
	["\f"]  = "\\f",
	["\n"]  = "\\n",
	["\t"]  = "\\t",
	["\v"]  = "\\v",
	["\\\\"] = "\\\\",
}
function utils.encodeCString( str)
	local len = string.len( str)
	local subst = function( match)
		len = len - 1
		return cStringSubstMap[ match] or " "
	end
	return (str:gsub("([\a\b\f\n\r\t\v\\])", subst)), len
end

-- Create a unique name
local uniqueNameIdxMap = {}
function utils.uniqueName( prefix)
	if uniqueNameIdxMap[ prefix] then uniqueNameIdxMap[ prefix] = uniqueNameIdxMap[ prefix] + 1 else uniqueNameIdxMap[ prefix] = 1 end
	return prefix .. uniqueNameIdxMap[ prefix]
end

-- Map template for LLVM Code synthesis
function utils.constructor_format( fmt, argtable, allocator)
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
				utils.errorMessage( 0, "Can't build constructor for '%s', having unbound enumerated variable '%d' and no allocator defined", 
							utils.encodeCString( fmt), index)
			end
		elseif argtable[ match] then
			return argtable[ match]
		else
			utils.errorMessage( 0, "Can't build constructor for '%s', having unbound variable '%s' (argument table %s)", utils.encodeCString(fmt), match, mewa.tostring(argtable,false))
		end
	end
	return (fmt:gsub("[{]([_%d%w]*)[}]", subst))
end

-- Map a LLVM Code synthesis template to a template substituting only the defined arguments and leaving the rest of the substitutions occurring untouched
function utils.template_format( fmt, arg )
	if type( fmt) == "table" then
		local rt = {}
		for kk,vv in pairs( fmt) do
			rt[ kk] = utils.template_format( vv, arg)
		end
		return rt;
	elseif type( fmt) == "string" then
		local subst = function( match)
			if arg[ match] then
				return arg[ match]
			else
				return "{" .. match .. "}"
			end
		end
		return (fmt:gsub("[{]([_%w%d]*)[}]", subst))
	else
		return fmt
	end
end

-- Name allocators for LLVM
function utils.name_allocator( prefix)
        local i = 0
        return function ()
                i = i + 1
                return prefix .. i
        end
end

function utils.label_allocator( prefix)
	return utils.name_allocator( prefix or "L")
end

function utils.register_allocator( prefix)
	return utils.name_allocator( prefix or "%r")
end

-- Tree traversal:
function processSubnode( typedb, subnode, ...)
	local rt
	if subnode.call then
		if (subnode.scope) then
			local scope_bk = typedb:scope( subnode.scope)
			if subnode.call.obj then rt = subnode.call.proc( subnode, subnode.call.obj, ...) else rt = subnode.call.proc( subnode, ...) end
			typedb:scope( scope_bk)
		elseif (subnode.step) then
			typedb:step( subnode.step)
			if subnode.call.obj then rt = subnode.call.proc( subnode, subnode.call.obj, ...) else rt = subnode.call.proc( subnode, ...) end
		else
			if subnode.call.obj then rt = subnode.call.proc( subnode, subnode.call.obj, ...) else rt = subnode.call.proc( subnode, ...) end
		end
	else
		rt = subnode.value
	end
	return rt
end
-- Tree traversal or a subrange of argument nodes, define scope/step and do the traversal call
function utils.traverseRange( typedb, node, range, ...)
	if node.arg then
		local rt = {}
		local kk; if range[3] then kk = range[3] else kk = range[1] end
		for ii=range[1],range[2] do rt[ kk] = processSubnode( typedb, node.arg[ ii], ...); kk = kk + 1 end
		return rt
	else
		return node.value
	end
end

-- Tree traversal, define scope/step and do the traversal call
function utils.traverse( typedb, node, ...)
	if node.arg then
		local rt = {}
		for ii,subnode in ipairs(node.arg) do rt[ii] = processSubnode( typedb, subnode, ...) end
		return rt
	else
		return node.value
	end
end

-- [3] Node visitor doing a traversal and returning the tree structure with the name and scope of the node
function utils.abstractSyntaxTree( typedb, node)
	return {name=node.call.name, scope=node.scope, arg=utils.traverse( typedb, node)}
end

-- Types in readable form for messages
-- Type as string
function utils.typeString( typedb, typeId)
	if typeId == 0 then
		return "<>"
	elseif type(typeId) == "table" then
		return typedb:type_string( typeId.type)
	else
		return typedb:type_string( typeId)
	end
end

-- Type list as string
function utils.typeListString( typedb, typeList, separator)
	if not typeList or #typeList == 0 then return "" end
	local rt = utils.typeString( typedb, typeList[ 1])
	for ii=2,#typeList do
		rt = rt .. (separator or ", ") .. utils.typeString( typedb, typeList[ ii])
	end
	return rt
end

-- Type to resolve as string for errir messages
function utils.resolveTypeString( typedb, contextType, typeName)
	local rt
	if type(contextType) == "table" then
		if #contextType == 0 then
			rt = "{} " .. typeName
		else
			rt = "{ " .. utils.typeListString( typedb, contextType) .. " } " .. typeName
		end
	else
		rt = utils.typeString( typedb, contextType) .. " " .. typeName
	end
	return rt;
end

-- Stacktrace:
function utils.stack( msg, lv)
	io.stderr:write( msg .. mewa.tostring( mewa.stacktrace( lv or 7,{"utils.traverse","utils.stack","processSubnode"}), true) .. "\n")
end
-- Error reporting:
-- Exit with error message and line info
function utils.errorMessage( line, fmt, ...)
	local arg = {...}
	if line ~= 0 then
		error( "Error on line " .. line .. ": " .. string.format( fmt, unpack(arg)))
	else
		error( string.format( fmt, unpack(arg)))
	end
end

-- Exit with error for no result or an ambiguous result returned by typedb:resolve_type
function utils.errorResolveType( typedb, line, resultContextTypeId, contextType, typeName)
	local resolveTypeString = utils.resolveTypeString( typedb, contextType, typeName)
	if not resultContextTypeId then
		utils.errorMessage( line, "Failed to resolve '%s'", resolveTypeString)
	elseif type(resultContextTypeId) == "table" then
		local t1 = typedb:type_string( resultContextTypeId[1])
		local t2 = typedb:type_string( resultContextTypeId[2])
		utils.errorMessage( line, "Ambiguous reference resolving '%s': %s, %s", resolveTypeString, t1, t2)
	end
end

-- Unicode support, function inspired from http://lua-users.org/wiki/LuaUnicode
function utils.utf8to32( node, utf8str)
	local res = {}
	local seq = 0
	local val = nil
	for i = 1, #utf8str do
		local c = string.byte( utf8str, i)
		if seq == 0 then
			if val then table.insert( res, val) end
			seq = c < 0x80 and 1 or c < 0xE0 and 2 or c < 0xF0 and 3 or
			      c < 0xF8 and 4 or --c < 0xFC and 5 or c < 0xFE and 6 or
				  utils.errorMessage( node.line, "invalid UTF-8 character sequence at byte %d: %s", i-1, utf8str)
			val = bit32.band(c, 2^(8-seq) - 1)
		else
			val = bit32.bor(bit32.lshift(val, 6), bit32.band(c, 0x3F))
		end
		seq = seq - 1
	end
	if val then
		if seq ~= 0 then utils.errorMessage( node.line, "incomplete UTF-8 character sequence: %s", utf8str) end
		table.insert( res, val)
	end
	return res
end

-- Debug function that returns the tree with all resolve type paths
function utils.getResolveTypeTrace( typedb, contextType, typeName, tagmask)
	function getResolveTypeTrace_rec( typedb, contextType, typeName, indentstr, visited, weightsum)
		local typeid = typedb:resolve_type( contextType, typeName, tagmask)
		if typeid and typeid == contextType then
			local rt = ""; if indentstr ~= "" then rt = rt .. "\n" end
			rt = rt .. indentstr .. "MATCH " .. typedb:type_string(contextType) .. " " .. typeName .. "\n"
			return rt
		elseif contextType ~= 0 then
			local rt = ""; if indentstr ~= "" then rt = rt .. "\n" end
			table.insert( visited, contextType)
			local rdlist = typedb:get_reductions( contextType, tagmask)
			for ri,rd in ipairs(rdlist) do
				local in_visited = false; for vi,ve in ipairs(visited) do if ve == rd.type then in_visited = true; end end
				if not in_visited then
					rt = rt .. indentstr .. "[" .. weightsum+rd.weight .."] ".. typedb:type_string(contextType) .." -> ".. typedb:type_string(rd.type)
					rt = rt .. getResolveTypeTrace_rec( typedb, rd.type, typeName, indentstr .. "  ", visited, weightsum+rd.weight)
				end
			end
			table.remove( visited, #visited)
			return rt
		else
			return ""
		end
	end
	if type(contextType) == "table" then
		if contextType.type then
			return getResolveTypeTrace_rec( typedb, contextType.type, typeName, "", {}, 0.0)
		else
			local rt = ""
			for ci,ct in ipairs(contextType) do
				rt = rt .. utils.getResolveTypeTrace( typedb, ct, typeName, tagmask) .. "\n"
			end
			return rt
		end
	else
		return getResolveTypeTrace_rec( typedb, contextType, typeName, "", {}, 0.0)
	end
end

-- Debug function that returns the search trace of a derive type call
function utils.getDeriveTypeTrace( typedb, destType, srcType, tagmask, tagmask_pathlen, max_pathlen)
	local rt = ""
	if not max_pathlen then max_pathlen = 1 end
	local elementmap = {[srcType] = 1}
	local elementlist = {{type=srcType, weight=0.0, pathlen=0, pred=0}}
	local queue = {{weight=0.0, elementidx=#elementlist}}
	local weightEpsilon = 1E-8
	local function insertQueue( queue, weight, elementidx)
		for qi=1,#queue do
			if queue[ qi].weight > weight then
				table.insert( queue, qi, {weight=weight, elementidx=elementidx})
				return true
			end
		end
		table.insert( queue, {weight=weight, elementidx=elementidx})
	end
	local function fetchElement()
		local weight = queue[1].weight
		local index = queue[1].elementidx
		local element = elementlist[ index]
		table.remove( queue, 1)
		return element,weight,index
	end
	local function pushElement( typeId, weight, count, pred, constructor)
		local pathlen
		if count then incr = 1 else incr = 0 end
		if pred == 0 then
			pathlen = incr
		else
			pathlen = elementlist[ pred].pathlen +incr
			weight = weight + elementlist[ pred].weight
		end
		if pathlen <= max_pathlen then
			if not elementmap[ typeId] or weight <= elementlist[ elementmap[ typeId]].weight + weightEpsilon then
				table.insert( elementlist, {type=typeId, weight=weight, pathlen=pathlen, pred=pred, constructor=constructor})
				elementmap[ typeId] = #elementlist
				insertQueue( queue, weight, #elementlist)
			end
		end
	end
	local function elementString( element, elementidx)
		local rt = typedb:type_string( element.type)
		if element.pred > 0 then rt = rt .. " <- [" .. string.format( "%.4f", elementlist[elementidx].weight) .. "] " .. elementString( elementlist[element.pred],element.pred) end
		return rt
	end
	local result
	local resultweight
	local rt_bk
	while #queue > 0 do
		local element,weight,elementidx = fetchElement()
		if resultweight and weight > resultweight + weightEpsilon then break end
		rt = rt .. "CANDIDATE weight=" .. string.format( "%.4f", weight) .. " #" .. element.pathlen .. " = " .. elementString( element, elementidx) .. "\n"
		if element.type == destType then
			if resultweight then
				if resultweight < weight + weightEpsilon then 
					rt = rt .. "AMBIGUOUS\n"
					rt_bk = rt
				end
				break
			else
				rt = rt .. "MATCH " .. typedb:type_string(destType) .. "\n"
				rt_bk = rt
				resultweight = weight
			end
		end
		local redulist = typedb:get_reductions( element.type, tagmask, tagmask_pathlen)
		for _,redu in ipairs( redulist) do
			pushElement( redu.type, redu.weight, redu.count, elementidx, redu.constructor)
		end
	end
	if not resultweight then 
		rt = rt .. "NOTFOUND\n"
	end
	return rt
end

local function treeToString( typedb, node, indentstr, node_tostring)
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
function utils.getTypeTreeDump( typedb)
	function node_tostring(nd)
		return string.format( "%s : %s", typedb:type_string(nd), mewa.tostring(typedb:type_constructor(nd),false))
	end
	return treeToString( typedb, typedb:type_tree(), "", node_tostring)
end
function utils.getReductionTreeDump( typedb)
	function node_tostring(nd)
		return string.format( "%s <- %s [%d]/%.3f : %s",
					typedb:type_string(nd.to), typedb:type_string(nd.from), nd.tag, nd.weight,
					mewa.tostring(nd.constructor,false))
	end
	return treeToString( typedb, typedb:reduction_tree(), "", node_tostring)
end

return utils

