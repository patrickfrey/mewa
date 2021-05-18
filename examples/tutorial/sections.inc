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
