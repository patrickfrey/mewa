#!/usr/bin/sh
# [1] Edit your grammar:
# ...

# [2] Build a typesystem module template
# Be careful, run mewa -o only once. It overwrites later edits!
mewa -o examples/tutorial/output/typesystem_template.lua -s examples/tutorial/grammar.g

# [3] Copy examples/tutorial/output/typesystem_template.lua to examples/tutorial/typesystem.lua and edit the typesystem.lua
# ...

