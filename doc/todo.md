# TODO List

 * Pointer relations of inherited class to derived class
 * Migrate Mewa repo to own github organization
 * Implement generic var for global variables depending on a type (instead of static declarations)
 * Implement lambdas ```lambda( a, b, c){ a = b + c }```, lambda argument type as transfer type with a constructor defining the reduction of the lambda argument name to the type transferred.
 * Implement free operators: ```operator Complex[double]( const double, const Complex[double] o)```for promote calls
 * Do not implement NOP destructors (transitive)

# Open, postponed issues
 * Implement coroutines (more investigation needed, I want more than just implement generators and asio, arbitralily connect state machines)

# Open issues where help is welcome
 * Arbitrary precision arithmetics library for both integers and floating points used for modelling const expressions in Lua.
 * Native build for non GNU platforms. CMake is not considered.
 * Test with Lua 5.1, build with LuaJIT
 * Packaging
 * Try to get into https://llvm.org/ProjectsWithLLVM

