#!%luabin%

typesystem = require( "%typesystem%")
mewa = require("mewa")

compilerdef = %automaton%

compiler = mewa.compiler( compilerdef)
print( compiler)
