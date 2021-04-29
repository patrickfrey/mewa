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
			print( "RUN " .. testname .. "()\n[[" .. result .. "]]\nERR\nEXPECTED:[[" .. expected .. "]]\n")
		else
			print( "RUN " .. testname .. "() ERR")
		end
		errorCount = errorCount + 1
	else
		if (verbose) then
			print( "RUN " .. testname .. "()\n" .. result .. "\nOK")
		else
			print( "RUN " .. testname .. "() OK")
		end
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
	typedb:scope( {0,1000}); typedb:set_instance( "register", register_allocator())
	typedb:scope( {2,300}); typedb:set_instance( "register", register_allocator())
	typedb:scope( {6,50}); typedb:set_instance( "register", register_allocator())
	typedb:scope( {23,25}); typedb:set_instance( "register", register_allocator())

	-- Allocate registers:
	local steps = {45, 199, 49, 49, 23, 24, 24,278,289, 291, 300}
	local registers = {}
	for ii,step in ipairs( steps) do
		typedb:step( step);
		table.insert( registers, typedb:get_instance( "register")())
	end
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

	local function pushParameter( constructor)
		return "param " .. constructor.type
	end
	local function typeReduction( stu_)
		local stu = stu_
		return function ( type)
			return "#" .. stu .. "(" .. type .. ")"
		end
	end

	local function reduceType( constructor)
		return "param " .. constructor.type
	end
	local function typeTreeToString( node, indent)
		rt = ""
		for chld in node:chld() do
			local scope = chld:scope()
			local indentstr = string.rep("  ", indent)
			rt = rt .. string.format( "%s[%d,%d]:", indentstr, scope[1], scope[2]) .. "\n"
			for type in chld:list() do
				local constructor = typedb:type_constructor( type)
				rt = rt .. string.format( "%s  - %s : %s", indentstr, typedb:type_string( type), mewa.tostring( constructor, 0, false)) .. "\n"
			end
			rt = rt .. typeTreeToString( chld, indent+1)
		end
		return rt
	end

	local tag_scalar_conv = 1
	local mask_resolve = typedb.reduction_tagmask( tag_scalar_conv)

	local scope_bk = typedb:scope( {0,100} )
	local any_type = typedb:def_type( 0, "any", {type="any",code="#any"})

	local short_type = typedb:def_type( 0, "short", {type="short",code="#short"})
	local int_type = typedb:def_type( 0, "int", {type="int",code="#int"})
	local float_type = typedb:def_type( 0, "float", {type="float",code="#float"})

	local add_short = typedb:def_type( short_type, "+", {op="add",type="short",code="short+short"}, {{short_type, pushParameter}})
	local add_int = typedb:def_type( int_type, "+", {op="add",type="int",code="int+int"}, {{int_type, pushParameter}})
	local add_float = typedb:def_type( float_type, "+", {op="add",type="float",code="float+float"}, {{float_type, pushParameter}})

	typedb:def_reduction( int_type, float_type, typeReduction( "int"), tag_scalar_conv, 0.5)
	typedb:def_reduction( float_type, int_type, typeReduction( "float"), tag_scalar_conv, 1.0)
	typedb:def_reduction( short_type, float_type, typeReduction( "short"), tag_scalar_conv, 0.5)
	typedb:def_reduction( float_type, short_type, typeReduction( "float"), tag_scalar_conv, 1.0)
	typedb:def_reduction( short_type, int_type, typeReduction( "short"), tag_scalar_conv, 0.5)
	typedb:def_reduction( int_type, short_type, typeReduction( "int"), tag_scalar_conv, 1.0)

	typedb:def_reduction( float_type, any_type, typeReduction( "float"), tag_scalar_conv, 1.0)

	local result = ""
	result = result .. "TYPE TREE\n" .. typeTreeToString( typedb:type_tree(), 0)

	typedb:scope( scope_bk)
	typedb:step( 34)
	local types = {short_type, int_type, float_type}
	for i,typeid in ipairs( types) do
		for i,reduction in ipairs( typedb:get_reductions( typeid, mask_resolve)) do
			result = result .. "\nREDU " .. typedb:type_string( typeid) .. " -> " .. typedb:type_string( reduction.type)
					.. " : " .. reduction.constructor( typedb:type_string( typeid))
		end
	end
	typedb:step( 99)
	local reduction_queries = {
			{short_type,int_type},{short_type,float_type},{int_type,float_type},
			{int_type,short_type},{float_type,short_type},{float_type,int_type}}
	for i,redu in ipairs( reduction_queries) do
		local weight,constructor = typedb:get_reduction( redu[1], redu[2], mask_resolve)
		result = result .. "\nSELECT REDU " .. typedb:type_string( redu[2]) .. " -> " .. typedb:type_string( redu[1])
					.. string.format( " /%.4f", weight)
					.. " : " .. constructor( typedb:type_string( redu[2]))
	end
	local resolve_queries = {{short_type, "+"},{int_type, "+"},{float_type, "+"},{any_type, "+"}}
	for i,qry in ipairs( resolve_queries) do
		local ctx,reductions,items = typedb:resolve_type( qry[1], qry[2], mask_resolve)
		if ctx then
			if type(ctx) == "table" then error( "Ambiguous reference resolving type " .. typedb:type_string( qry[1]) .. qry[2]) end

			prev_type = any_type
			for ri,reduction in ipairs( reductions) do
				constructor = reduction.constructor( typedb:type_string( prev_type))
				result = result .. "\nRESOLVE REDU " .. typedb:type_string( reduction.type) .. "<-" .. typedb:type_string( prev_type)
						.. " : " .. constructor
				prev_type = reduction.type
			end
			for i,item in ipairs( items) do
				result = result .. "\nRESOLVE ITEM " .. typedb:type_string( item) .. " : " .. mewa.tostring( typedb:type_constructor( item), 0, false)
			end
		else
			error( "Failed to resolve type " .. typedb:type_string( qry[1]) .. qry[2])
		end
	end
	local expected = [[
TYPE TREE
[0,100]:
  - any : {code = "#any",type = "any"}
  - short : {code = "#short",type = "short"}
  - int : {code = "#int",type = "int"}
  - float : {code = "#float",type = "float"}
  - short +(short) : {code = "short+short",op = "add",type = "short"}
  - int +(int) : {code = "int+int",op = "add",type = "int"}
  - float +(float) : {code = "float+float",op = "add",type = "float"}

REDU short -> float : #float(short)
REDU short -> int : #int(short)
REDU int -> float : #float(int)
REDU int -> short : #short(int)
REDU float -> int : #int(float)
REDU float -> short : #short(float)
SELECT REDU int -> short /0.5000 : #short(int)
SELECT REDU float -> short /0.5000 : #short(float)
SELECT REDU float -> int /0.5000 : #int(float)
SELECT REDU short -> int /1.0000 : #int(short)
SELECT REDU short -> float /1.0000 : #float(short)
SELECT REDU int -> float /1.0000 : #float(int)
RESOLVE ITEM short +(short) : {code = "short+short",op = "add",type = "short"}
RESOLVE ITEM int +(int) : {code = "int+int",op = "add",type = "int"}
RESOLVE ITEM float +(float) : {code = "float+float",op = "add",type = "float"}
RESOLVE REDU float<-any : #float(any)
RESOLVE ITEM float +(float) : {code = "float+float",op = "add",type = "float"}]]
	checkTestResult( "testDefineResolveType", result, expected)
end

-- Function that tests the typedb resolve type with many context type with constructors
function testResolveTypeContext()
	typedb = mewa.typedb()
	local tag_conv = 1
	local mask_resolve = typedb.reduction_tagmask( tag_conv)

	local type1 = typedb:def_type( 0, "type1", "constructor type1")
	local funA = typedb:def_type( type1, "A", function(arg) return arg .. " # " .. " constructor fun A" end)
	local type2 = typedb:def_type( 0, "type2", "constructor type2")
	local ext_type2 = typedb:def_type( 0, "type2 ext", "constructor ext type2")
	typedb:def_reduction( type2, ext_type2, function(arg) return arg .. " [22->11]" end, tag_conv)
	local funB = typedb:def_type( type2, "B", function(arg) return arg .. " # " .. " constructor fun B" end)
	local type3 = typedb:def_type( 0, "type3", "constructor type3")
	local ext_type3 = typedb:def_type( 0, "type3 ext", "constructor ext type3")
	local ext_ext_type3 = typedb:def_type( 0, "type3 XXext", "constructor ext XX type3")
	typedb:def_reduction( type3, ext_type3, function(arg) return arg .. " [2->1]" end, tag_conv)
	typedb:def_reduction( ext_type3, ext_ext_type3, function(arg) return arg .. " [3->2]" end, tag_conv)
	local funC = typedb:def_type( type3, "C", function(arg) return arg .. " # " .. " constructor fun C" end)
	local type4 = typedb:def_type( 0, "type4", "constructor type4")
	local funD = typedb:def_type( type4, "D", function(arg) return arg .. " # " .. " constructor fun D" end)
	local contextList =  {
		type1,
		{constructor = "contextual constructor 2", type = ext_type2},
		{constructor = "contextual constructor 3", type = ext_ext_type3},
		{constructor = "contextual constructor 4", type = type4}
	}
	function getResolveResultString( fun)
		local rt = ""
		local ctx,reductions,items = typedb:resolve_type( contextList, fun, mask_resolve)
		if not ctx or type(ctx) == "table" then error( "Failed to resolve function " .. fun) end
		rt = rt .. "CTX " .. typedb:type_string(ctx) .. "\n"
		local redures = nil
		for ri,redu in ipairs(reductions) do
			if type(redu.constructor) == "function" then redures = redu.constructor(redures or "ORIG") else redures = redu.constructor end
		end
		rt = rt .. "REDU " .. (redures or "ORIG") .. "\n"
		for ii,item in ipairs(items) do
			local itemstr
			local item_constructor = typedb:type_constructor( item)
			if type(item_constructor) == "function" then itemstr = item_constructor(redures or "ORIG") else itemstr = item_constructor end
			rt = rt .. "ITEM " .. itemstr .. "\n"
		end
		return rt
	end
	local result = 	getResolveResultString( "A") .. getResolveResultString( "B") .. getResolveResultString( "C") .. getResolveResultString( "D")
	local expected = [[
CTX type1
REDU ORIG
ITEM ORIG #  constructor fun A
CTX type2
REDU contextual constructor 2 [22->11]
ITEM contextual constructor 2 [22->11] #  constructor fun B
CTX type3
REDU contextual constructor 3 [3->2] [2->1]
ITEM contextual constructor 3 [3->2] [2->1] #  constructor fun C
CTX type4
REDU contextual constructor 4
ITEM contextual constructor 4 #  constructor fun D
]]
	checkTestResult( "testResolveTypeContext", result, expected)
end

-- Function that tests the typedb def_type_as
function testDefineTypeAs()
	typedb = mewa.typedb()
	local tag_conv = 1
	local mask_resolve = typedb.reduction_tagmask( tag_conv)
	local ctx1 = typedb:def_type( 0, "ctx1", "constructor ctx1")
	local type1 = typedb:def_type( ctx1, "type1", "constructor type1")
	local ctx2 = 0
	local type2 = typedb:def_type( ctx2, "type2", "constructor type2")
	local syn1 = typedb:def_type_as( ctx1, "syn1", type2)
	local syn2 = typedb:def_type_as( ctx2, "syn2", type1)
	
	local rctx1,reductions1,items1 = typedb:resolve_type( ctx1, "syn1", mask_resolve)
	local rctx2,reductions2,items2 = typedb:resolve_type( ctx2, "syn2", mask_resolve)
	if rctx1 then
		if type(rctx1) == "table" then error( "Ambiguous reference resolving type " .. typedb:type_string( ctx1) .. "syn1") end
	else
		error( "Failed to resolve type " .. typedb:type_string( ctx1) .. "syn1")
	end
	if rctx2 then
		if type(rctx2) == "table" then error( "Ambiguous reference resolving type " .. typedb:type_string( ctx2) .. "syn2") end
	else
		error( "Failed to resolve type " .. typedb:type_string( ctx2) .. "syn2")
	end
	if #items1 ~= 1 then
		error( "Expected only one resolved type for " .. typedb:type_string( ctx1) .. "syn1")
	end
	if #items2 ~= 1 then
		error( "Expected only one resolved type for " .. typedb:type_string( ctx2) .. "syn2")
	end
	if items1[1] ~= type2 then
		error( "Expected syn1 to be a synonym of type2")
	end
	if items2[1] ~= type1 then
		error( "Expected syn2 to be a synonym of type1")
	end
	checkTestResult( "testDefineTypeAs", "", "")
end

function testLlvmFloatAndDoubleToHex()
	local result = "";
	local input_float = {4.21, 3.01, 9.012e-7, 1.11711e+10, 5.9125e-4, 0.1111111111, 4.25}
	local input_double = {4.212121212121, 3.010101010101, 9.01212121212e-7, 1.117111111e+34, 5.9125555e-41, 0.1111111111, 4.2555555555}
	for ii,val in ipairs(input_float) do
		result = result .. "0x" .. mewa.llvm_float_tohex( val) .. "\n"
	end
	for ii,val in ipairs(input_double) do
		result = result .. "0x" .. mewa.llvm_double_tohex( val) .. "\n"
	end
	local expected = [[0x4010D70A40000000
0x4008147AE0000000
0x3EAE3D3FC0000000
0x4204CECBA0000000
0x3F435FC3C0000000
0x3FBC71C720000000
0x4011000000000000
0x4010D9364D9363EA
0x400814AFD6A052A9
0x3EAE3D5A69A8B55A
0x47013639F616F11E
0x37949A2CFDC32C37
0x3FBC71C71C658F9D
0x401105B05B04BC05
]]
	checkTestResult( "testLlvmFloatAndDoubleToHex", result, expected)
end

testRegisterAllocator()
testDefineResolveType()
testResolveTypeContext()
testDefineTypeAs()
testLlvmFloatAndDoubleToHex()

if errorCount > 0 then
	error( "result of " .. errorCount .. " tests not as expected!")
end


