mewa = require "mewa"
local utils = require "typesystem_utils"
llvmir = require "llvmir"
typedb = mewa.typedb()

-- Global variables
localDefinitionContext = 0	-- Context type of local definitions
seekContextKey = "seekctx"	-- Key for context types defined for a scope
callableEnvKey = "env"		-- Key for current callable environment
typeDescriptionMap = {}		-- Map of type ids to their description
referenceTypeMap = {}		-- Map of value type ids to their reference type if it exists
dereferenceTypeMap = {}		-- Map of reference type ids to their value type if it exists
arrayTypeMap = {}		-- Map array keys to their array type
stringConstantMap = {}		-- Map of string constant values to a structure with the attributes {name,size}
scalarTypeMap = {}		-- Map of scalar type names to the correspoding value type

dofile( "examples/tutorial/sections.inc")

-- AST Callbacks:
typesystem = {}
dofile( "examples/tutorial/ast.inc")
return typesystem
