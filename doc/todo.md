# TODO List

 * Report error on statements after return and similar cases, mark statements without follow statement allowed
 * Try to get into https://llvm.org/ProjectsWithLLVM
 * Make deep copies of values passed as lval and shallow copies of structures passed as const lval. Define these parameter variables as references to their copies. 
 * Migrate Mewa repo to own github project
 * Implement generic var for global variables depending on a type (instead of static declarations)
 * lambdas [ ]( a, b, c){ a = b + c }
 * swap tag, constructor arguments in def_reduction? Constructor often nil
 * lambda argument type as transfer type with a constructor defining the reduction of the lambda argument name to the type transferred.

# Open issues
* Arbitrary precision arithmetics library for both integers and floating points used for modelling const expressions in Lua.
* Native build for non GNU platforms. CMake is not considered.
* Build with LuaJIT
* Packaging

