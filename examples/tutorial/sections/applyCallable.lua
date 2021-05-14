local utils = require "typesystem_utils"

-- For a callable item, create for each argument the lists of reductions needed to pass the arguments to it, with accumulation of the reduction weights
function collectItemParameter( node, item, args, parameters)
	local rt = {redulist={},llvmtypes={},weight=0.0}
	for pi=1,#args do
		local redutype,redulist,weight
		if pi <= #parameters then
			redutype = parameters[ pi].type
			redulist,weight = tryGetWeightedParameterReductionList( node, redutype, args[ pi], tagmask_matchParameter, tagmask_typeConversion)
		else
			redutype = getVarargArgumentType( args[ pi].type)
			if redutype then redulist,weight = tryGetWeightedParameterReductionList( node, redutype, args[ pi], tagmask_pushVararg, tagmask_typeConversion) end
		end
		if not weight then return nil end
		if rt.weight < weight then rt.weight = weight end -- use max(a,b) as weight accumulation function
		table.insert( rt.redulist, redulist)
		local descr = typeDescriptionMap[ redutype]
		table.insert( rt.llvmtypes, descr.llvmtype)
	end
	return rt
end
-- Select the candidate items with the highest weight not exceeding maxweight
function selectCandidateItemsBestWeight( items, item_parameter_map, maxweight)
	local candidates,bestweight = {},nil
	for ii,item in ipairs(items) do
		if item_parameter_map[ ii] then -- we have a match
			local weight = item_parameter_map[ ii].weight
			if not maxweight or maxweight > weight + weightEpsilon then -- we have a candidate not looked at yet
				if not bestweight then
					candidates = {ii}
					bestweight = weight
				elseif weight < bestweight + weightEpsilon then
					if weight >= bestweight - weightEpsilon then -- they are equal
						table.insert( candidates, ii)
					else -- the new candidate is the single best match
						candidates = {ii}
						bestweight = weight
					end
				end
			end
		end
	end
	return candidates,bestweight
end
-- Get the best matching item from a list of items by weighting the matching of the arguments to the item parameter types
function selectItemsMatchParameters( node, items, args, this_constructor)
	local item_parameter_map = {}
	local bestmatch = {}
	local candidates = {}
	local bestweight = nil
	local bestgroup = 0
	for ii,item in ipairs(items) do
		local nof_params = typedb:type_nof_parameters( item)
		if nof_params == #args then
			item_parameter_map[ ii] = collectItemParameter( node, item, args, typedb:type_parameters( item))
		end
	end
	while next(item_parameter_map) ~= nil do -- until no candidate groups left
		candidates,bestweight = selectCandidateItemsBestWeight( items, item_parameter_map, bestweight)
		for _,ii in ipairs(candidates) do -- create the items from the item constructors passing
			local item_constructor = typedb:type_constructor( items[ ii])
			local call_constructor
			if not item_constructor and #args == 0 then
				call_constructor = this_constructor
			else
				local ac,success = nil,true
				local arg_constructors = {}
				for ai=1,#args do
					ac,success = tryApplyReductionList( node, item_parameter_map[ ii].redulist[ ai], args[ ai].constructor)
					if not success then break end
					table.insert( arg_constructors, ac)
				end
				if success then call_constructor = item_constructor( this_constructor, arg_constructors, item_parameter_map[ ii].llvmtypes) end
			end
			if call_constructor then table.insert( bestmatch, {type=items[ ii], constructor=call_constructor}) end
			item_parameter_map[ ii] = nil
		end
		if #bestmatch ~= 0 then return bestmatch,bestweight end
	end
end
-- Find a callable identified by name and its arguments (parameter matching) in the context of a type (this)
function findApplyCallable( node, this, callable, args)
	local resolveContextType,reductions,items = typedb:resolve_type( this.type, callable, tagmask_resolveType)
	if not resolveContextType then return nil end
	if type(resolveContextType) == "table" then
		io.stderr:write( "TRACE typedb:resolve_type\n" .. utils.getResolveTypeTrace( typedb, this.type, callable, tagmask_resolveType) .. "\n")
		utils.errorResolveType( typedb, node.line, resolveContextType, this.type, callable)
	end
	local this_constructor = applyReductionList( node, reductions, this.constructor)
	local bestmatch,bestweight = selectItemsMatchParameters( node, items, args or {}, this_constructor)
	return bestmatch,bestweight
end
-- Filter best result and report error on ambiguity
function getCallableBestMatch( node, bestmatch, bestweight, this, callable, args)
	if #bestmatch == 1 then
		return bestmatch[1]
	elseif #bestmatch == 0 then
		return nil
	else
		local altmatchstr = ""
		for ii,bm in ipairs(bestmatch) do
			if altmatchstr ~= "" then
				altmatchstr = altmatchstr .. ", "
			end
			altmatchstr = altmatchstr .. typedb:type_string(bm.type)
		end
		utils.errorMessage( node.line, "Ambiguous matches resolving callable with signature '%s', list of candidates: %s", typeDeclarationString( this, callable, args), altmatchstr)
	end
end
-- Retrieve and apply a callable in a specified context
function applyCallable( node, this, callable, args)
	local bestmatch,bestweight = findApplyCallable( node, this, callable, args)
	if not bestweight then
		io.stderr:write( "TRACE typedb:resolve_type\n" .. utils.getResolveTypeTrace( typedb, this.type, callable, tagmask_resolveType) .. "\n")
		utils.errorMessage( node.line, "Failed to find callable with signature '%s'", typeDeclarationString( this, callable, args))
	end
	local rt = getCallableBestMatch( node, bestmatch, bestweight, this, callable, args)
	if not rt then  utils.errorMessage( node.line, "Failed to match parameters to callable with signature '%s'", typeDeclarationString( this, callable, args)) end
	return rt
end
-- Try to retrieve a callable in a specified context, apply it and return its type/constructor pair if found, return nil if not
function tryApplyCallable( node, this, callable, args)
	local bestmatch,bestweight = findApplyCallable( node, this, callable, args)
	if bestweight then return getCallableBestMatch( node, bestmatch, bestweight) end
end
