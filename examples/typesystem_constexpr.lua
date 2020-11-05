--- Utility functions for constexpr types
bcd = require("bcd")

-- Module object with all functions exported
local M = {}

function M.tonumber( val)
	if val.type == "bigint" then
		val.type = "number"
		val.value = tonumber( tostring( val.value))
	end
	return val
end

function M.add( v1, v2)
	
	if v1.type == "bigint" and v2.type == "bigint" then
		return { type = "bigint", value = v1.value + v2.value }
	else
		return { type = "number", value = (M.tonumber( v1).value + M.tonumber( v2).value) }
	end
end

function M.sub( v1, v2)
	if v1.type == "bigint" and v2.type == "bigint" then
		return { type = "bigint", value = v1.value - v2.value }
	else
		return { type = "number", value = (M.tonumber( v1).value - M.tonumber( v2).value) }
	end
end

function M.mul( v1, v2)
	if v1.type == "bigint" and v2.type == "bigint" then
		return { type = "bigint", value = v1.value * v2.value }
	else
		return { type = "number", value = (M.tonumber( v1).value * M.tonumber( v2).value) }
	end
end

function M.div( v1, v2)
	if v1.type == "bigint" and v2.type == "bigint" then
		return { type = "bigint", value = v1.value / v2.value }
	else
		if M.tonumber( v2).value == 0.0 then
			error( "Division by zero in constexpr")
		end
		return { type = "number", value = (M.tonumber( v1).value / M.tonumber( v2).value) }
	end
end

function M.mod( v1, v2)
	if v1.type == "bigint" and v2.type == "bigint" then
		return { type = "bigint", value = v1.value % v2.value }
	else
		if M.tonumber( v2).value == 0.0 then
			error( "Division by zero in constexpr")
		end
		return { type = "number", value = (M.tonumber( v1).value % M.tonumber( v2).value) }
	end
end

function M.unm( val)
	if val.type == "bigint" then
		return { type = "bigint", value = -val.value }
	else
		return { type = "number", value = -M.tonumber( val).value }
	end
end

function M.pow( v1, v2)
	if v1.type == "bigint" and v2.type == "bigint" then
		return { type = "bigint", value = v1.value ^ v2.value }
	else
		return { type = "number", value = (M.tonumber( v1).value ^ M.tonumber( v2).value) }
	end
end

local Metatable = 
{
	__add = M.add,
	__mul = M.mul, 
	__sub = M.sub,
	__div = M.div,
	__mod = M.mod,
	__unm = M.unm,
	__pow = M.pow,
}

function M.bigint( val)
	local rt = {
		type = "bigint";
		value = bcd.int( val);
	}
	setmetatable( rt, Metatable)
	return rt;
end

function M.number( val)
	local rt = {
		type = "number";
		value = tonumber( val);
	}
	setmetatable( rt, Metatable)
	return rt;
end

return M

