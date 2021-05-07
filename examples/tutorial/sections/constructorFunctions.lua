local utils = require "typesystem_utils"

-- Get the two parts of a constructor as tuple
function constructorParts( constructor)
	if type(constructor) == "table" then return constructor.out,(constructor.code or "") else return tostring(constructor),"" end
end
-- Get a constructor structure
function constructorStruct( out, code)
	return {out=out, code=code or ""}
end

-- Builds the argument string and the argument build-up code for a function call or interface method call constructors
function buildCallArguments( subst, thisTypeId, thisConstructor, args, types)
	local this_inp,this_code = constructorParts(thisConstructor or "%UNDEFINED")
	local argstr = ""
	if thisTypeId ~= 0 then argstr = typeDescriptionMap[ thisTypeId].llvmtype .. " " .. this_inp .. ", " end
	code = code .. this_code
	for ii=1,#args do
		local arg = args[ ii]
		subst[ "arg" .. ii] = arg
		if types then
			local llvmtype = typeDescriptionMap[ types[ ii]].llvmtype
			local arg_inp,arg_code = constructorParts( arg)
			code = code .. arg_code
			if llvmtype then argstr = argstr .. llvmtype .. " " .. tostring(arg_inp) .. ", " else argstr = argstr .. "i32 0, " end
		end
	end
	if types and (#args > 0 or thisTypeId ~= 0) then argstr = argstr:sub(1, -3) end
	subst.argstr = argstr
	return code
end
-- Constructor of a call with an self argument and some additional arguments
function callConstructorFunction( fmt, thisTypeId, argTypeIds)
	return function( this_constructor, arg_constructors)
		env = getCallableEnvironment()
		local out = env.register()
		local subst = {out=out, this=this_constructor}
		local code = buildCallArguments( subst, thisTypeId, this_constructor, arg_constructors, argTypeIds)
		code = code .. utils.constructor_format( fmt, {out=out, this=value}, env.register)
		return {code=code,out=out}
	end
end

