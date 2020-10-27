-- This module was inspired by the URL encoder/decoder of Liukun (Github https://gist.github.com/liukun)
-- See https://gist.github.com/liukun/f9ce7d6d14fa45fe9b924a3eed5c3d99
-- Thanks to Liukun

local char_to_hex = function(c)
  return string.format("_%02X", string.byte(c))
end

local hex_to_char = function(x)
  return string.char( tonumber( x, 16))
end


local M = {}

function M.mangleName( name)
  return url:gsub("([^%w _])", char_to_hex)
end

function M.demangleName( name)
  return url:gsub("_(%x%x)", hex_to_char)
end

return M

