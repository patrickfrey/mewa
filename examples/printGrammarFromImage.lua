-- This module exports the function "printLanguageDef" that builds a mewa language description file (equivalent to the original ".g" file)
-- from a Lua table generated with the mewa option "--generate-language" or "-l"
--
-- This reverse process is in itself not very useful but for testing and as an example.
--
require "io"
require "string"
require "math"

local reserved = {"op","name","pattern","open","close","tabsize","nl","select","priority","left","right","scope","call","line"}
local image = {}

local function contains( tb, val)
   for i=1,#tb do
      if tb[i] == val then
	 return true
      end
   end
   return false
end

local function printDecoratorsAsComments( rule)
	for key, val in pairs(rule) do
		if not contains( reserved, key) and #val > 0 then
			print( "# @" .. key .. " " .. val[ 1])
			for ii=2,#val do
				print( "#\t" .. val[ ii])
			end
		end
	end
end

local function quoteString( str)
	if string.find( str, "\"") then
		return "\'" .. str .. "\'"
	else
		return "\"" .. str .. "\""
	end
end

local function productionElementListToString( right)
	local rt = nil
	for idx,elem in ipairs(right) do
		if elem.type == "name" then
			value = elem.value
		elseif elem.type == "symbol" then
			value = quoteString(elem.value)
		else
			error( "unknown production element type '" .. elem.type .. "'")
		end
		rt = not rt and value or rt .. " " .. value
	end
	return rt
end

function image.printLanguageDef( def)
	print( "% LANGUAGE " .. def.LANGUAGE .. ";")
	print( "% TYPESYSTEM \"" .. def.TYPESYSTEM .. "\";")
	print( "% CMDLINE \"" .. def.CMDLINE .. "\";")
	prev_prodname = nil
	rulestr = nil
	for idx,rule in ipairs( def.RULES ) do
		printDecoratorsAsComments( rule)
		if rule.op == "COMMENT" then
			if rule.close then
				print( "% COMMENT " .. quoteString( rule.open) .. " " .. quoteString( rule.close) .. ";")
			else
				print( "% COMMENT " .. quoteString( rule.open) .. ";")
			end
		elseif rule.op == "INDENTL" then
			print( "% INDENTL " .. quoteString( rule.open) .. " " .. quoteString( rule.close) .. " " .. quoteString( rule.nl) .. " " .. quoteString( rule.tabsize) .. ";")
		elseif rule.op == "BAD" then
			print( "% BAD " .. quoteString( rule.name) .. ";")
		elseif rule.op == "IGNORE" then
			print( "% IGNORE " .. quoteString( rule.pattern) .. ";")
		elseif rule.op == "TOKEN" then
			if rule.select then
				print( rule.name .. ": " .. quoteString( rule.pattern) .. " " .. rule.select .. ";")
			else
				print( rule.name .. ": " .. quoteString( rule.pattern) .. ";")
			end
		elseif rule.op == "PROD" then
			left = rule.priority and rule.left .. "/" .. rule.priority or rule.left
			if prev_prodname == left then
				indent = string.rep( "\t", 3)
				rulestr = rulestr .. "\n" .. indent .. "| "
			else
				indent = string.rep( "\t", math.max( 3 - math.floor(string.len( left) / 8), 0))
				if rulestr then
					print( rulestr .. ";")
				end
				rulestr = left .. indent .. "= "
				prev_prodname = left
			end
			if #rule.right > 0 then
				rulestr = rulestr .. productionElementListToString( rule.right)
			end
			if rule.call or rule.scope then
				rulestr = rulestr .. " (" .. (rule.scope or "") .. (rule.call or "") .. ")"
			end
		end
	end
	if rulestr then
		print( rulestr .. ";")
	end
end

return image
