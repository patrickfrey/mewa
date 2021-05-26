local utils = require "typesystem_utils"

-- Constructor for a promote call (implementing int + double by promoting
--   the first argument int to double and do a double + double to get the result)
function promoteCallConstructor( call_constructor, promote)
	return function( this, arg)
		return call_constructor( promote( this), arg)
	end
end
-- Define an operation with involving the promotion of the left-hand argument to
--   another type and executing the operation as defined for the type promoted to.
function definePromoteCall( rType, thisType, proType, opr, argTypes, promote)
	local call_type = typedb:get_type( proType, opr, argTypes)
	local call_constructor = typedb:type_constructor( call_type)
	local constructor = promoteCallConstructor( call_constructor, promote)
	local callType = typedb:def_type( thisType, opr, constructor, argTypes)
	if callType == -1 then
		utils.errorMessage( node.line, "Duplicate definition '%s'",
				typeDeclarationString( thisType, opr, argTypes))
	end
	if rType then
		typedb:def_reduction( rType, callType, nil, tag_typeDeclaration)
	end
end
-- Define an operation generalized
function defineCall( rType, thisType, opr, argTypes, constructor)
	local callType = typedb:def_type( thisType, opr, constructor, argTypes)
	if callType == -1 then
		local declstr = typeDeclarationString( thisType, opr, argTypes)
		utils.errorMessage( 0, "Duplicate definition of call '%s'", declstr)
	end
	if rType then
		typedb:def_reduction( rType, callType, nil, tag_typeDeclaration)
	end
	return callType
end
