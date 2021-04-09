# TODO List

 * Solve problem with destructor of variable with destructor assigned where the assignment fails, after the variable has been destroyed. 
	Add an destructor chain with the destroyed type as part of the key, that leaves out the destruction of the variable.
 * Pointer relations of inherited class to derived class
 * Migrate Mewa repo to own github organization
 * Implement generic var for global variables depending on a type (instead of static declarations)
 * Implement lambdas ```lambda( a, b, c){ a = b + c }```, lambda argument type as transfer type with a constructor defining the reduction of the lambda argument name to the type transferred.
 * Implement free operators: ```operator Complex[double]( const double, const Complex[double] o)```for promote calls
 * Implement coroutines
 * Implement copy elision of single return variable
 * Implement a derive type dump with queue in typesystem_utils.lua for debugging

# Open issues
 * Arbitrary precision arithmetics library for both integers and floating points used for modelling const expressions in Lua.
 * Native build for non GNU platforms. CMake is not considered.
 * Test with Lua 5.1, build with LuaJIT
 * Packaging
 * Try to get into https://llvm.org/ProjectsWithLLVM

