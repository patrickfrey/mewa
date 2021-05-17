# TODO List

 * Implement generic var for global variables depending on a type (instead of static declarations)
 * Implement free operators: ```operator Complex[double]( const double, const Complex[double] o)```for promote calls
 * Do not implement NOP constructors/destructors (transitive)
 * Check if L/R is used only in conflict resolution of a production with itself as written in the tutorial.

# Open, postponed issues
 * Implement coroutines (more investigation needed, I want more than just implement generators and asio, arbitralily connect state machines)

# Open issues where help is welcome
 * Arbitrary precision arithmetics library for both integers and floating points used for modelling const expressions in Lua.
 * Native build for non GNU platforms. CMake is not considered.
 * Packaging
 * Try to get into https://llvm.org/ProjectsWithLLVM

