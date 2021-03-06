-- Tags attached to reduction definitions
tag_typeDeclaration = 1   -- Type declaration relation (e.g. variable to data type)
tag_typeDeduction = 2     -- Type deduction (e.g. inheritance)
tag_typeConversion = 3    -- Type value conversion
tag_typeInstantiation = 4 -- Type value construction from const expression

-- Sets of tags used for resolving a type or deriving a type, depending on the case
tagmask_declaration = typedb.reduction_tagmask( tag_typeDeclaration)
tagmask_resolveType = typedb.reduction_tagmask(
                tag_typeDeduction, tag_typeDeclaration)
tagmask_matchParameter = typedb.reduction_tagmask(
                tag_typeDeduction, tag_typeDeclaration,
                tag_typeConversion, tag_typeInstantiation)
tagmask_typeConversion = typedb.reduction_tagmask( tag_typeConversion)
