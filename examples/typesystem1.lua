
return {
	options = {},

	parseArguments = function( arg)
		files = {}
		for ai = 1, #arg do
			if string.sub( arg[ai], 1, 1) == "-" then
				options[ string.sub( arg[ai], 2, 1)] = string.sub( arg[ai], 3)
			else
				table.insert( files, arg[ai])
			end
		end
		return files
	end,

	assign = {},
	add = {},
	sub = {},
	minus = {},
	plus = {},
	mul = {},
	div = {},
	mod = {},

	operator = function( arg)
		print( "operator " .. arg)
	end
}

