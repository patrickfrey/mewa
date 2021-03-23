-- Get a specific call constructor
function getCallConstructor( thisType, opr, argTypes)
	local scope_bk = typedb:scope( typedb:type_scope( thisType))
	local rt = typedb:type_constructor( typedb:get_type( promoteType, opr, argTypes))
	typedb:scope(scope_bk)
	return rt
end
