mewa = require "mewa"
local utils = require "typesystem_utils"
llvmir = require "llvmir"
typedb = mewa.typedb()

-- Global variables
localDefinitionContext = 0	-- Context type of local definitions
seekContextKey = "seekctx"	-- Key for seek context types defined for a scope
callableEnvKey = "env"		-- Key for current callable environment
typeDescriptionMap = {}		-- Map of type ids to their description
referenceTypeMap = {}		-- Map of value type to reference type
dereferenceTypeMap = {}		-- Map of reference type to value type
arrayTypeMap = {}		-- Map array key to its type
stringConstantMap = {}		-- Map of string constant value to its attributes
scalarTypeMap = {}		-- Map of scalar type name to its value type

dofile( "examples/tutorial/sections.inc")

-- AST Callbacks:
typesystem = {}
dofile( "examples/tutorial/ast.inc")
return typesystem
