mewa = require("mewa")
typedb = mewa.typedb()

-- A register allocator as a function counting from 1, returning the LLVM register identifiers:
function register_allocator()
        local i = 0
        return function ()
                i = i + 1
                return "%" .. i
        end
end

-- Create a register allocator for the scope [0..1000]:
typedb:set_instance( "register", {0,1000}, register_allocator())
-- Create a register allocator for the scope [2..300]:
typedb:set_instance( "register", {2,300}, register_allocator())

-- Allocate a register in the scope step 45, that is the first register in [2..300]:
print( typedb:get_instance( "register", 45)());
-- Allocate a register in the scope step 500, that is the first register in [0..1000]:
print( typedb:get_instance( "register", 500)());
-- Allocate a register in the scope step 49, that is the second register in [2..300]:
print( typedb:get_instance( "register", 49)());
