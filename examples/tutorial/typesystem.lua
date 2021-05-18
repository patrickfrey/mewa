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

-- Define the tags of reductions and the tag masks for resolve/derive type:
dofile( "examples/tutorial/sections/reductionTagsAndTagmasks.lua")
-- Define the weights of reductions:
dofile( "examples/tutorial/sections/reductionWeights.lua")
-- Type representation as string:
dofile( "examples/tutorial/sections/declarationString.lua")
-- Define the functions used to implement most of the constructors:
dofile( "examples/tutorial/sections/constructorFunctions.lua")
-- Define the functions to apply a constructor or a chain of constructors:
dofile( "examples/tutorial/sections/applyConstructors.lua")
-- Functions to find the best match of a callable with arguments and to build its constructor:
dofile( "examples/tutorial/sections/applyCallable.lua")
-- Define types named calls having arguments, for example operators:
dofile( "examples/tutorial/sections/defineCalls.lua")
-- Define data types, basic up to complex structures like arrays or classes:
dofile( "examples/tutorial/sections/defineDataTypes.lua")
-- Declare all data of a function in a structure named callable environment that is bound to a scope:
dofile( "examples/tutorial/sections/callableEnvironment.lua")
-- Declare the first class scalar types:
dofile( "examples/tutorial/sections/firstClassScalarTypes.lua")
-- Declare constant expression types and arithmetics:
dofile( "examples/tutorial/sections/constexprTypes.lua")
-- Define how the list of context types used to retrieve types is bound to the current scope:
dofile( "examples/tutorial/sections/contextTypes.lua")
-- Define all functions need to declare functions and methods,etc.:
dofile( "examples/tutorial/sections/callableTypes.lua")
-- Define the functions needed to resolve types and matching types:
dofile( "examples/tutorial/sections/resolveTypes.lua")
-- Implementation of control boolean types for control structures and boolean operators:
dofile( "examples/tutorial/sections/controlBooleanTypes.lua")
-- Define variables (globals, locals, members):
dofile( "examples/tutorial/sections/variables.lua")

-- AST Callbacks:
typesystem = {}
-- Extern function declarations (typesystem.extern_funcdef typesystem.extern_paramdef typesystem.extern_paramdeflist)
dofile( "examples/tutorial/typesystem/extern.lua")
-- Function declarations (typesystem.funcdef typesystem.callablebody typesystem.paramdef typesystem.paramdeflist)
dofile( "examples/tutorial/typesystem/functions.lua")
-- Operator declarations (typesystem.binop typesystem.unop typesystem.member typesystem.operator typesystem.operator_array)
dofile( "examples/tutorial/typesystem/operators.lua")
-- Control structures (typesystem.conditional_if typesystem.conditional_else typesystem.conditional_elseif
--   typesystem.conditional_while typesystem.return_value typesystem.return_void)
dofile( "examples/tutorial/typesystem/controls.lua")
-- Type definitions (typesystem.typehdr typesystem.arraytype typesystem.typespec typesystem.classdef)
dofile( "examples/tutorial/typesystem/types.lua")
-- Values, constants and constant structures (typesystem.vardef typesystem.variable typesystem.constant typesystem.structure)
dofile( "examples/tutorial/typesystem/values.lua")
-- Program building blocks besides functions, classes and types ()
dofile( "examples/tutorial/typesystem/blocks.lua")

return typesystem
