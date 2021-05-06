-- Tags attached to reduction definitions. When resolving a type or deriving a type, we select reductions by specifying a set of valid tags
tag_typeDeclaration = 1			-- Type declaration relation (e.g. variable to data type)
tag_typeConversion = 2			-- Type conversion of parameters
tag_typeInstantiation = 3		-- Type value construction from const expression
tag_namespace = 4			-- Type deduction for resolving namespaces
tag_pushVararg = 5			-- Type deduction for passing parameter as vararg argument
tag_transfer = 6			-- Transfer of information to build an object by a constructor, used in free function callable to pointer assignment

-- Sets of tags used for resolving a type or deriving a type, depending on the case
tagmask_resolveType = typedb.reduction_tagmask( tag_typeDeclaration)
tagmask_matchParameter = typedb.reduction_tagmask( tag_typeDeclaration, tag_typeConversion, tag_typeInstantiation, tag_transfer)
tagmask_typeConversion = typedb.reduction_tagmask( tag_typeConversion)
tagmask_namespace = typedb.reduction_tagmask( tag_namespace)
tagmask_pushVararg = typedb.reduction_tagmask( tag_typeDeclaration, tag_typeConversion, tag_typeInstantiation, tag_pushVararg)

