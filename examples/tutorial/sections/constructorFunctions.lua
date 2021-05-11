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
	local this_inp,code = constructorParts(thisConstructor or "%UNDEFINED")
	local argstr = ""
	if thisTypeId ~= 0 then argstr = typeDescriptionMap[ thisTypeId].llvmtype .. " " .. this_inp .. ", " end
	if args then
		for ii=1,#args do
			local arg = args[ ii]
			local arg_inp,arg_code = constructorParts( arg)
			subst[ "arg" .. ii] = arg_inp
			if types then
				local llvmtype = typeDescriptionMap[ types[ ii]].llvmtype
				if llvmtype then argstr = argstr .. llvmtype .. " " .. tostring(arg_inp) .. ", " else argstr = argstr .. "i32 0, " end
			end
			code = code .. arg_code
		end
	end
	if types and thisTypeId ~= 0 then argstr = argstr:sub(1, -3) end
	subst.argstr = argstr
	subst.this = this_inp
	return code
end
-- Constructor of a call with an self argument and some additional arguments
function callConstructor( fmt, thisTypeId, argTypeIds, vartable)
	return function( this_constructor, arg_constructors)
		env = getCallableEnvironment()
		local out = env.register()
		local subst = utils.deepCopyStruct( vartable) or {}
		subst.out = out
		local code = buildCallArguments( subst, thisTypeId or 0, this_constructor, arg_constructors, argTypeIds)
		code = code .. utils.constructor_format( fmt, subst, env.register)
		return {code=code,out=out}
	end
end

