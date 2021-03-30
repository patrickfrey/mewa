# TODO List

 * Report error on statements after return and similar cases, mark statements without follow statement allowed
 * Try to get into https://llvm.org/ProjectsWithLLVM
 * Solve problem with destructor of variable with destructor assigned where the assignment fails, after the variable has been destroyed. Add an own destructor chain with the destroyed type as part of the key, that leaves out the destruction of the variable.
 * Pointer relations of inherited class to derived class
 * Migrate Mewa repo to own github organization
 * Use more 'next' instead of array access in Lua, replace ipairs with pairs or next where possible
 * Implement generic var for global variables depending on a type (instead of static declarations)
 * Implement lambdas ```lambda( a, b, c){ a = b + c }```, lambda argument type as transfer type with a constructor defining the reduction of the lambda argument name to the type transferred.
 * Implement free operators: ```operator Complex[double]( const double, const Complex[double] o)```for promote calls
 * Implement coroutines
 * Test with Lua 5.1 (LuaJIT)

# Open issues
* Arbitrary precision arithmetics library for both integers and floating points used for modelling const expressions in Lua.
* Native build for non GNU platforms. CMake is not considered.
* Build with LuaJIT
* Packaging

