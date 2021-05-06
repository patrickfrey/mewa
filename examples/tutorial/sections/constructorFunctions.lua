local utils = require "typesystem_utils"

function constructorStruct( out, code)
	return {out=out, code=code or ""}
end
function convConstructor( fmt, valueConv)
	return function( value)
		env = getCallableEnvironment()
		local out = env.register()
		if valueConv then value = valueConv(value) end
		local code = utils.constructor_format( fmt, {out=out, this=value}, env.register)
		return {code=code,out=out}
	end
end
