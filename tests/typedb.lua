#!/usr/bin/lua

mewa = require("mewa")

-- Function that tests the typedb getter/setter for objects in a scoped context
-- The example usage of the concept is a register allocator for LLVM that is a different one for every function scope
function testRegisterAllocator()
	typedb = mewa.typedb()

	-- A register allocator as a function counting from 1, returning the LLVM register identifiers:
	function register_allocator()
		local i = 0
		return function ()
			i = i + 1
			return "%" .. i
		end
	end

	-- Define a register allocator function as an object with name "register" for 4 scopes defined as {start,end}
	typedb:set( "register", {0,1000}, register_allocator())
	typedb:set( "register", {2,300}, register_allocator())
	typedb:set( "register", {6,50}, register_allocator())
	typedb:set( "register", {23,25}, register_allocator())

	-- Allocate registers, the second parameter of typedb:get is the scope step, a sort of an instruction counter referring to 
	-- a specific scope and thus to a specific register allocator defined:
	local registers = {
		typedb:get( "register", 45)(),	-- Get a register in the scope step 45, allocate it with the allocator defined for scope {6,50}
		typedb:get( "register", 199)(),	-- Get a register in the scope step 199, allocate it with the allocator defined for scope {2,300}
		typedb:get( "register", 49)(),	-- Get a register in the scope step 49, allocate it with the allocator defined for scope {6,50}
		typedb:get( "register", 49)(),	-- Get a register in the scope step 49, allocate it with the allocator defined for scope {6,50}
		typedb:get( "register", 23)(),	-- Get a register in the scope step 23, allocate it with the allocator defined for scope {23,25}
		typedb:get( "register", 24)(),	-- Get a register in the scope step 24, allocate it with the allocator defined for scope {23,25}
		typedb:get( "register", 24)(),	-- Get a register in the scope step 24, allocate it with the allocator defined for scope {23,25}
		typedb:get( "register", 278)(),	-- Get a register in the scope step 278, allocate it with the allocator defined for scope {2,300}
		typedb:get( "register", 289)(),	-- Get a register in the scope step 289, allocate it with the allocator defined for scope {2,300}
		typedb:get( "register", 291)(),	-- Get a register in the scope step 291, allocate it with the allocator defined for scope {2,300}
		typedb:get( "register", 300)()	-- Get a register in the scope step 300, allocate it with the allocator defined for scope {0,1000}
	}
	local result = "RESULT";
	for i,register in ipairs( registers) do
		result = result .. " " .. register
	end
	local expected = "RESULT %1 %1 %2 %3 %1 %2 %3 %2 %3 %4 %1"
	if result ~= expected then
		error( "testRegisterAllocator() result not as expected!")
	end
	print( "Run testRegisterAllocator() => " .. result .. " OK")
end

testRegisterAllocator()

