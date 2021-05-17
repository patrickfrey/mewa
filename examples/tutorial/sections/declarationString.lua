local utils = require "typesystem_utils"

-- Type string of a type declaration built from its parts for error messages
function typeDeclarationString( this, typnam, args)
	local rt = (this == 0 or not this) and typnam or (typedb:type_string(type(this) == "table" and this.type or this) .. " " .. typnam)
	if (args) then rt = rt .. "(" .. utils.typeListString( typedb, args) .. ")" end
	return rt
end


