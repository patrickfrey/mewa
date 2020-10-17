#!/usr/bin/lua

mewa = require("mewa")
require( 'pl')
require( 'string')
lapp = require( 'pl.lapp')

local args = lapp( [[
Test program for mewa.typedb
	-h,--help     Print usage
	-V,--verbose  Verbose output
]])
if args.help then
	print( "Usage: typedb.lua [-h][-V]")
	exit( 0)
end

local verbose = args.verbose
local errorCount = 0

function checkTestResult( testname, result, expected)
	if result ~= expected then
		if (verbose) then
			print( "RUN " .. testname .. "()\n" .. result .. "\nERR")
		else
			print( "RUN " .. testname .. "() ERR")
		end
		errorCount = errorCount + 1
	end
	if (verbose) then
		print( "RUN " .. testname .. "()\n" .. result .. "\nOK")
	else
		print( "RUN " .. testname .. "() OK")
	end
end


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
	typedb:set_instance( "register", {0,1000}, register_allocator())
	typedb:set_instance( "register", {2,300}, register_allocator())
	typedb:set_instance( "register", {6,50}, register_allocator())
	typedb:set_instance( "register", {23,25}, register_allocator())

	-- Allocate registers, the second parameter of typedb:get_instance is the scope step, a sort of an instruction counter referring to 
	-- a specific scope and thus to a specific register allocator defined:
	local registers = {
		typedb:get_instance( "register", 45)(),	-- Get a register in the scope step 45, allocate it with the allocator defined for scope {6,50}
		typedb:get_instance( "register", 199)(),	-- Get a register in the scope step 199, allocate it with the allocator defined for scope {2,300}
		typedb:get_instance( "register", 49)(),	-- Get a register in the scope step 49, allocate it with the allocator defined for scope {6,50}
		typedb:get_instance( "register", 49)(),	-- Get a register in the scope step 49, allocate it with the allocator defined for scope {6,50}
		typedb:get_instance( "register", 23)(),	-- Get a register in the scope step 23, allocate it with the allocator defined for scope {23,25}
		typedb:get_instance( "register", 24)(),	-- Get a register in the scope step 24, allocate it with the allocator defined for scope {23,25}
		typedb:get_instance( "register", 24)(),	-- Get a register in the scope step 24, allocate it with the allocator defined for scope {23,25}
		typedb:get_instance( "register", 278)(),	-- Get a register in the scope step 278, allocate it with the allocator defined for scope {2,300}
		typedb:get_instance( "register", 289)(),	-- Get a register in the scope step 289, allocate it with the allocator defined for scope {2,300}
		typedb:get_instance( "register", 291)(),	-- Get a register in the scope step 291, allocate it with the allocator defined for scope {2,300}
		typedb:get_instance( "register", 300)()	-- Get a register in the scope step 300, allocate it with the allocator defined for scope {0,1000}
	}
	local result = ""
	for i,register in ipairs( registers) do
		result = result .. " " .. register
	end
	local expected = " %1 %1 %2 %3 %1 %2 %3 %2 %3 %4 %1"
	checkTestResult( "testRegisterAllocator", result, expected)
end

-- Function that tests the typedb define/resolve type 
function testDefineResolveType()
	typedb = mewa.typedb()

	function pushParameter( constructor)
		return "param " .. constructor.type
	end
	function typeReduction( stu_)
		local stu = stu_
		return function ( type)
			return "#" .. stu .. "(" .. type .. ")"
		end
	end

	function reduceType( constructor)
		return "param " .. constructor.type
	end
	local any_type = typedb:def_type( {0,100}, 0, "any", {type="any",code="#any"})

	local short_type = typedb:def_type( {0,100}, 0, "short", {type="short",code="#short"})
	local int_type = typedb:def_type( {0,100}, 0, "int", {type="int",code="#int"})
	local float_type = typedb:def_type( {0,100}, 0, "float", {type="float",code="#float"})

	local add_short = typedb:def_type( {0,100}, short_type, "+", {op="add",type="short",code="short+short"}, {{short_type, pushParameter}})
	local add_int = typedb:def_type( {0,100}, int_type, "+", {op="add",type="int",code="int+int"}, {{int_type, pushParameter}})
	local add_float = typedb:def_type( {0,100}, float_type, "+", {op="add",type="float",code="float+float"}, {{float_type, pushParameter}})

	typedb:def_reduction( {0,100}, int_type, float_type, typeReduction( "int"), 0.5)
	typedb:def_reduction( {0,100}, float_type, int_type, typeReduction( "float"), 1.0)
	typedb:def_reduction( {0,100}, short_type, float_type, typeReduction( "short"), 0.5)
	typedb:def_reduction( {0,100}, float_type, short_type, typeReduction( "float"), 1.0)
	typedb:def_reduction( {0,100}, short_type, int_type, typeReduction( "short"), 0.5)
	typedb:def_reduction( {0,100}, int_type, short_type, typeReduction( "int"), 1.0)

	typedb:def_reduction( {0,100}, float_type, any_type, typeReduction( "any"), 1.0)

	local result = ""
	local types = {short_type, int_type, float_type}
	for i,type in ipairs( types) do
		for i,reduction in ipairs( typedb:type_reductions( 34, type)) do
			result = result .. "\nREDU " .. typedb:type_string( type) .. " -> " .. typedb:type_string( reduction.type)
					.. " : " .. reduction.constructor( typedb:type_string( type))
		end
	end
	local reduction_queries = {
			{short_type,int_type},{short_type,float_type},{int_type,float_type},
			{int_type,short_type},{float_type,short_type},{float_type,int_type}}
	for i,redu in ipairs( reduction_queries) do
		local constructor = typedb:type_reduction( 99, redu[1], redu[2])
		result = result .. "\nSELECT REDU " .. typedb:type_string( redu[2]) .. " -> " .. typedb:type_string( redu[1])
					.. " : " .. constructor( typedb:type_string( redu[2]))
	end
	local resolve_queries = {{short_type, "+"},{int_type, "+"},{float_type, "+"}}
	for i,qry in ipairs( resolve_queries) do
		local reductions,items = typedb:resolve_type( 12, any_type, "+")
		print ("+++REDUS:" .. mewa.tostring( reductions) .. "+++")
		print ("+++ITEMS:" .. mewa.tostring( items) .. "+++")
		prev_type = any_type
		for i,reduction in ipairs( reductions) do
			result = result .. "\nRESOLVE REDU " .. typedb:type_string( prev_type) .. "<-" .. typedb:type_string( reduction.type)
					.. " : " reduction.constructor( prev_type)
			prev_type = reduction.type
		end
		for i,item in ipairs( items) do
			print ("+++ITEM:" .. mewa.tostring( item) .. "+++")
			result = result .. "\nRESOLVE ITEM " .. typedb:type_string( item.type) .. " : " .. item.constructor
		end
	end
	local expected = [[

REDU short -> float : #float(short)
REDU short -> int : #int(short)
REDU int -> float : #float(int)
REDU int -> short : #short(int)
REDU float -> int : #int(float)
REDU float -> short : #short(float)
SELECT REDU int -> short : #short(int)
SELECT REDU float -> short : #short(float)
SELECT REDU float -> int : #int(float)
SELECT REDU short -> int : #int(short)
SELECT REDU short -> float : #float(short)
SELECT REDU int -> float : #float(int)]]
	checkTestResult( "testDefineResolveType", result, expected)
end

testRegisterAllocator()
testDefineResolveType()

if errorCount > 0 then
	error( "result of " .. errorCount .. " tests not as expected!")
end

