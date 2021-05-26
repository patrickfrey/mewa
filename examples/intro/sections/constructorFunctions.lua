local utils = require "typesystem_utils"

-- Get the two parts of a constructor as tuple
function constructorParts( constructor)
	if type(constructor) == "table" then
		return constructor.out,(constructor.code or "")
	else
		return tostring(constructor),""
	end
end
-- Get a constructor structure
function constructorStruct( out, code)
	return {out=out, code=code or ""}
end
-- Create a type/constructor pair as used by most functions constructing a type
function typeConstructorPairStruct( type, out, code)
	return {type=type, constructor=constructorStruct( out, code)}
end
-- Builds the argument string and the argument build-up code for a function call
--   or interface method call constructors
function buildCallArguments( subst, thisTypeId, thisConstructor, args, types)
	local this_inp,code = constructorParts(thisConstructor or "%UNDEFINED")
	local callargstr = ""
	if types and thisTypeId and thisTypeId ~= 0 then
		callargstr = typeDescriptionMap[ thisTypeId].llvmtype .. " " .. this_inp .. ", "
	end
	if args then
		for ii=1,#args do
			local arg = args[ ii]
			local arg_inp,arg_code = constructorParts( arg)
			subst[ "arg" .. ii] = arg_inp
			if types then
				local llvmtype = types[ ii].llvmtype
				if not llvmtype then
					utils.errorMessage( 0, "Parameter has no LLVM type specified")
				end
				callargstr = callargstr .. llvmtype .. " " .. tostring(arg_inp) .. ", "
			end
			code = code .. arg_code
		end
	end
	if types or thisTypeId ~= 0 then callargstr = callargstr:sub(1, -3) end
	subst.callargstr = callargstr
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
		local code = buildCallArguments(
					subst, thisTypeId or 0,
					this_constructor, arg_constructors, argTypeIds)
		code = code .. utils.constructor_format( fmt, subst, env.register)
		return {code=code,out=out}
	end
end
