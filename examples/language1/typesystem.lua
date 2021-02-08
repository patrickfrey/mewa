local mewa = require "mewa"
local llvmir = require "language1/llvmir"
local utils = require "typesystem_utils"
local bcd = require "bcd"

local typedb = mewa.typedb()
local typesystem = {}

local tag_typeDeduction = 1
local tag_typeDeclaration = 2
local tag_TypeConversion = 3
local tag_typeNamespace = 4
local tag_pointerReinterpretCast = 5
local tagmask_declaration = typedb.reduction_tagmask( tag_typeDeclaration)
local tagmask_resolveType = typedb.reduction_tagmask( tag_typeDeduction, tag_typeDeclaration)
local tagmask_matchParameter = typedb.reduction_tagmask( tag_typeDeduction, tag_typeDeclaration, tag_TypeConversion)
local tagmask_typeConversion = typedb.reduction_tagmask( tag_TypeConversion)
local tagmask_typeNameSpace = typedb.reduction_tagmask( tag_typeNamespace)
local tagmask_typeCast = typedb.reduction_tagmask( tag_typeDeduction, tag_typeDeclaration, tag_TypeConversion, tag_pointerReinterpretCast)

local weightEpsilon = 1E-5		-- epsilon used for comparing weights for equality
local scalarQualiTypeMap = {}		-- maps scalar type names without qualifiers to the table of type ids for all qualifiers possible
local scalarIndexTypeMap = {}		-- maps scalar type names usable as index without qualifiers to the const type id used as index for [] or pointer arithmetics
local scalarBooleanType = nil		-- type id of the boolean type, result of cmpop binary operators
local scalarIntegerType = nil		-- type id of the main integer type, result of main function
local stringPointerType = nil		-- type id of the string constant type used for free string litterals
local memPointerType = nil		-- type id of the type used for result of allocmem and argument of freemem
local stringAddressType = nil		-- type id of the string constant type used string constants outsize a function scope
local anyClassPointerType = nil		-- type id of the "class^" type
local anyConstClassPointerType = nil 	-- type id of the "class^" type
local anyStructPointerType = nil	-- type id of the "struct^" type
local anyConstStructPointerType = nil	-- type id of the "struct^" type
local controlTrueType = nil		-- type implementing a boolean not represented as value but as peace of code (in the constructor) with a falseExit label
local controlFalseType = nil		-- type implementing a boolean not represented as value but as peace of code (in the constructor) with a trueExit label
local qualiTypeMap = {}			-- maps any defined type without qualifier to the table of type ids for all qualifiers possible
local referenceTypeMap = {}		-- maps non reference types to their reference types
local dereferenceTypeMap = {}		-- maps reference types to their type with the reference qualifier stripped away
local constTypeMap = {}			-- maps non const types to their const type
local privateTypeMap = {}		-- maps non private types to their private type
local pointerTypeMap = {}		-- maps non pointer types to their pointer type
local rvalueTypeMap = {}		-- maps value types with an rvalue reference type defined to their rvalue reference type
local typeQualiSepMap = {}		-- maps any defined type id to a separation pair (lval,qualifier) = lval type (strips away qualifiers), qualifier string
local typeDescriptionMap = {}		-- maps any defined type id to its llvmir template structure
local stringConstantMap = {}    	-- maps string constant values to a structure with its attributes {fmt,name,size}
local arrayTypeMap = {}			-- maps the pair lval,size to the array type lval for an array size 
local varargFuncMap = {}		-- maps types to true for vararg functions/procedures
local varargParameterMap = {}		-- maps any defined type id to the structure {llvmtype,type} where to map additional vararg types to in a function call
local instantCallableContext = nil		-- callable structure temporarily assigned for implicitely generated code (constructors,destructors,assignments,etc.)

-- Create the data structure with attributes attached to a context (referenced in body) of some function/procedure or a callable in general terms
function createCallableContext( scope, rtype)
	return {scope=scope, register=utils.register_allocator(), label=utils.label_allocator(), returntype=rtype}
end
-- Attach a newly created data structure for a callable to its scope
function defineCallableContext( scope, rtype)
	typedb:set_instance( "callable", createCallableContext( scope, rtype))
end
-- Get the active callable instance
function getCallableContext()
	return instantCallableContext or typedb:get_instance( "callable")
end
-- Get the two parts of a constructor as tuple
function constructorParts( constructor)
	if type(constructor) == "table" then return constructor.out,(constructor.code or ""),(constructor.alloc or "") else return tostring(constructor),"","" end
end
function constructorStruct( out, code, alloc)
	return {out=out, code=code, alloc=alloc}
end
function constructorStructEmpty()
	return {code=""}
end
function typeConstructorStruct( type, out, code, alloc)
	return {type=type, constructor=constructorStruct( out, code, alloc)}
end
-- Constructor of a single constant value without code
function constConstructor( val)
	return function() return {out=val,code=""} end
end
-- Constructor implementing some sort of manipulation of an object without output
function manipConstructor( fmt)
	return function( this)
		local inp,code = constructorParts( this)
		return constructorStruct( inp, code .. utils.constructor_format( fmt, {this=inp}))
	end
end
-- Constructor implementing an assignment of a single argument to an object
function assignConstructor( fmt)
	return function( this, arg)
		local callable = getCallableContext()
		local code = ""
		local this_inp,this_code = constructorParts( this)
		local arg_inp,arg_code = constructorParts( arg[1])
		local subst = {arg1 = arg_inp, this = this_inp}
		code = code .. this_code .. arg_code
		return constructorStruct( this_inp, code .. utils.constructor_format( fmt, subst, callable.register))
	end
end
-- Constructor imlementing a conversion of a data item to another type using a format string that describes the conversion operation
-- param[in] fmt format string, param[in] outRValueRef output variable in case of an rvalue reference constructor
function convConstructor( fmt, outRValueRef)
	return function( constructor)
		local callable = getCallableContext()
		local out,outsubst; if outRValueRef then out,outsubst = outRValueRef,("{"..outRValueRef.."}") else out = callable.register(); outsubst = out end
		local inp,code = constructorParts( constructor)
		local convCode = utils.constructor_format( fmt, {this = inp, out = outsubst}, callable.register)
		return constructorStruct( out, code .. convCode)
	end
end
-- Constructor implementing some operation with an arbitrary number of arguments selectively addressed without LLVM typeinfo attributes attached
-- param[in] fmt format string, param[in] outRValueRef output variable in case of an rvalue reference constructor
function callConstructor( fmt, outRValueRef)
	local function buildArguments( out, this_inp, args)
		local code = ""
		local subst = {out = out, this = this_inp}
		if args then
			for ii=1,#args do
				local arg_inp,arg_code = constructorParts( args[ ii])
				code = code .. arg_code
				subst[ "arg" .. ii] = arg_inp
			end
		end
		return code,subst
	end
	return function( this, args)
		local callable = getCallableContext()
		local out,outsubst; if outRValueRef then out,outsubst = outRValueRef,("{"..outRValueRef.."}") else out = callable.register(); outsubst = out end
		local this_inp,this_code = constructorParts( this)
		local code,subst = buildArguments( outsubst, this_inp, args)
		return constructorStruct( out, this_code .. code .. utils.constructor_format( fmt, subst, callable.register))
	end
end
-- Move constructor of a reference type from an RValue reference type
function rvalueReferenceMoveConstructor( this, args)
	local this_inp,this_code = constructorParts( this)
	local arg_inp,arg_code = constructorParts( args[1])
	return {code = this_code .. utils.template_format( arg_code, {[arg_inp] = this_inp}), out=this_inp}
end
-- Constructor of a contant reference value from a RValue reference type, creating a temporary on the stack for it
function constReferenceFromRvalueReferenceConstructor( descr)
	return function( constructor)
		local callable = getCallableContext()
		local out = callable.register()
		local inp,code = constructorParts( constructor)
		code = utils.constructor_format( descr.def_local, {out = out}, callable.register) .. utils.template_format( code, {[inp] = out} )
		return {code = code, out = out}
	end
end
-- Constructor implementing a call of a function with an arbitrary number of arguments built as one string with LLVM typeinfo attributes as needed for function calls
function functionCallConstructor( fmt, thisTypeId, sep, argvar, rtype)
	local function buildArguments( this_code, this_inp, args, llvmtypes)
		local code = this_code
		local argstr; if thisTypeId ~= 0 then argstr = typeDescriptionMap[ thisTypeId].llvmtype .. " " .. this_inp else argstr = "" end
		for ii=1,#args do
			local arg = args[ ii]
			local llvmtype = llvmtypes[ ii]
			local arg_inp,arg_code = constructorParts( arg)
			code = code .. arg_code
			if llvmtype then
				if argstr == "" then argstr = llvmtype .. " " .. arg_inp else argstr = argstr .. sep .. llvmtype .. " " .. arg_inp end
			else
				if argstr == "" then argstr = "i32 0" else argstr = argstr .. sep .. "i32 0" end
			end
		end
		return code,argstr
	end
	return function( this, args, llvmtypes)
		local callable = getCallableContext()
		local this_inp,this_code = constructorParts( this)
		local code,argstr = buildArguments( this_code, this_inp, args, llvmtypes)
		local out,subst
		if not rtype then
			subst = {[argvar] = argstr}
		elseif rvalueTypeMap[ rtype] then
			out = "RVAL"
			subst = {[argvar] = argstr, rvalref = "{RVAL}"}
		else
			out = callable.register()
			subst = {out = out, [argvar] = argstr}
		end
		return constructorStruct( out, code .. utils.constructor_format( fmt, subst, callable.register))
	end
end
-- Constructor for a recursive memberwise assignment of a tree structure (initializing a "struct")
function memberwiseAssignStructureConstructor( node, thisTypeId, members)
	return function( this, args)
		args = args[1] -- args contains one list node
		if #args ~= #members then 
			utils.errorMessage( node.line, "Number of elements %d in structure do not match for '%s' [%d]", #args, typedb:type_string( thisTypeId), #members)
		end
		local qualitype = qualiTypeMap[ thisTypeId]
		local descr = typeDescriptionMap[ thisTypeId]
		local callable = getCallableContext()
		local this_inp,this_code = constructorParts( this)
		local rt = constructorStruct( this_inp, this_code)
		for mi,member in ipairs( members) do
			local out = callable.register()
			local code = utils.constructor_format( member.loadref, {out = out, this = this_inp, index=mi-1}, callable.register)
			local reftype = referenceTypeMap[ member.type]
			local maparg = applyCallable( node, typeConstructorStruct( reftype, out, code), ":=", {args[mi]})
			rt.code = rt.code .. maparg.constructor.code
		end
		return rt
	end
end
-- Map an argument to a requested parameter type, creating a local variable if not possible otherwise
function tryCreateParameter( node, callable, typeId, arg)
	local constructor,llvmtype,weight = tryGetParameterConstructor( node, typeId, arg)
	if weight then
		return {type=typeId,constructor=constructor}
	else
		local derefTypeId = dereferenceTypeMap[ typeId] or typeId
		local refTypeId = referenceTypeMap[ typeId] or typeId
		local descr = typeDescriptionMap[ derefTypeId]
		local out = callable.register()
		local code = utils.constructor_format( descr.def_local, {out=out}, callable.register)
		return tryApplyCallable( node, typeConstructorStruct( refTypeId, out, code), ":=", {arg})
	end
end
-- Constructor for a recursive application of an operator with a tree structure as argument (initializing a "class" or a multiargument operator application)
function resolveOperatorConstructor( node, thisTypeId, opr)
	return function( this, args)
		local rt = {}
		if #args == 1 and args[1].type == constexprStructureType then args = args[1] end
		local callable = getCallableContext()
		local descr = typeDescriptionMap[ thisTypeId]
		local this_inp,this_code = constructorParts( this)
		local contextTypeId,reductions,candidateFunctions = typedb:resolve_type( thisTypeId, opr)
		if not contextTypeId then utils.errorMessage( node.line, "No operator '%s' found for '%s'", opr, typedb:type_string( thisTypeId)) end
		for ci,item in ipairs( candidateFunctions) do
			if typedb:type_nof_parameters( item) == #args then
				local param_ar = {}
				if #args > 0 then
					local parameters = typedb:type_parameters( item)
					for ai,arg in ipairs(args) do
						local maparg = tryCreateParameter( node, callable, parameters[ ai].type, args[ai])
						if not maparg then break end
						table.insert( param_ar, maparg)
					end
				end
				if #param_ar == #args then
					table.insert( rt, applyCallable( node, typeConstructorStruct( thisTypeId, this_inp, this_code), ":=", param_ar))
				end
			end
		end
		if #rt == 0 then
			local argstr = utils.typeListString( typedb, args, ", ")
			utils.errorMessage( node.line, "No operator '%s' for arguments {%s} found for '%s'", opr, argstr, typedb:type_string( thisTypeId))
		elseif #rt > 1 then
			utils.errorMessage( node.line, "Ambiguus reference for constructor of '%s'", typedb:type_string( thisTypeId))
		else
			return rt[1].constructor
		end
	end
end
-- Constructor for a promote call (implementing int + float by promoting the first argument int to float and do a float + float to get the result)
function promoteCallConstructor( call_constructor, promote_constructor)
	return function( this, arg)
		return call_constructor( promote_constructor( this), arg)
	end
end
-- Define an operation with involving the promotion of the left hand argument to another type and executing the operation as defined for the type promoted to.
function definePromoteCall( returnType, thisType, promoteType, opr, argTypes, promote_constructor)
	local call_constructor = typedb:type_constructor( typedb:get_type( promoteType, opr, argTypes))
	local callType = typedb:def_type( thisType, opr, promoteCallConstructor( call_constructor, promote_constructor), argTypes)
	if returnType then typedb:def_reduction( returnType, callType, nil, tag_typeDeclaration) end
end
-- Define an operation
function defineCall( returnType, thisType, opr, argTypes, constructor, priority)
	local callType = typedb:def_type( thisType, opr, constructor, argTypes, priority)
	if callType == -1 then utils.errorMessage( 0, "Duplicate definition of type '%s %s'", typedb:type_string(thisType), opr) end
	if returnType then typedb:def_reduction( returnType, callType, nil, tag_typeDeclaration) end
	return callType
end

local constexprIntegerType = typedb:def_type( 0, "constexpr int")		-- const expression integers implemented as arbitrary precision BCD numbers
local constexprUIntegerType = typedb:def_type( 0, "constexpr uint")		-- const expression cardinals implemented as arbitrary precision BCD numbers
local constexprFloatType = typedb:def_type( 0, "constexpr float")		-- const expression floating point numbers implemented as 'double'
local constexprBooleanType = typedb:def_type( 0, "constexpr bool")		-- const expression boolean implemented as Lua boolean
local constexprNullType = typedb:def_type( 0, "constexpr null")			-- const expression null value implemented as nil
local constexprStructureType = typedb:def_type( 0, "constexpr struct")		-- const expression tree structure implemented as a list of type/constructor pairs

varargParameterMap[ constexprIntegerType] = {llvmtype="i64",type=constexprIntegerType}
varargParameterMap[ constexprUIntegerType] = {llvmtype="i64",type=constexprUIntegerType}
varargParameterMap[ constexprFloatType] = {llvmtype="double",type=constexprFloatType}
varargParameterMap[ constexprBooleanType] = {llvmtype="i1",type=constexprBooleanType}

local typeClassToConstExprTypesMap = {
	fp = {constexprFloatType,constexprIntegerType,constexprUIntegerType}, 
	bool = {constexprBooleanType},
	signed = {constexprIntegerType,constexprUIntegerType},
	unsigned = {constexprUIntegerType}
}
local bits64 = bcd.bits( 64)
local constexprOperatorMap = {
	["+"] = function( this, arg) return this + arg[1] end,
	["-"] = function( this, arg) return this - arg[1] end,
	["*"] = function( this, arg) return this * arg[1] end,
	["/"] = function( this, arg) return this / arg[1] end,
	["%"] = function( this, arg) return this % arg[1] end,
	["&"] = function( this, arg) return this.bit_and( arg[1], bits64) end,
	["|"] = function( this, arg) return this.bit_or( arg[1], bits64) end,
	["^"] = function( this, arg) return this.bit_xor( arg[1], bits64) end,
	["~"] = function( this, arg) return this.bit_not( bits64) end,
	["&&"] = function( this, arg) return this == true and arg[1] == true end,
	["||"] = function( this, arg) return this == true or arg[1] == true end,
	["!"] = function( this, arg) return this ~= true end
}
local constexprTypeOperatorMap = {
	[constexprIntegerType]  = {"+","-","*","/","%"},
	[constexprUIntegerType] = {"&","|","^","~"},
	[constexprFloatType]    = {"+","-","*","/","%"},
	[constexprBooleanType]  = {"&&","||","!"}
}
local unaryOperatorMap = {["~"]=1,["!"]=1,["-"]=2}

-- Create a constexpr node from a lexem in the AST
function createConstExpr( node, constexpr_type, lexemvalue)
	if constexpr_type == constexprIntegerType then return bcd.int(lexemvalue)
	elseif constexpr_type == constexprUIntegerType then return bcd.int(lexemvalue)
	elseif constexpr_type == constexprFloatType then return tonumber(lexemvalue)
	elseif constexpr_type == constexprBooleanType then if lexemvalue == "true" then return true else return false end
	end
end
-- Define the exaluation of expressions with only const expression arguments
function defineConstExprArithmetics()
	for constexpr_type,oprlist in pairs(constexprTypeOperatorMap) do
		for oi,opr in ipairs(oprlist) do
			if unaryOperatorMap[ opr] then
				defineCall( constexpr_type, constexpr_type, opr, {}, constexprOperatorMap[ opr])
			end
			if not unaryOperatorMap[ opr] or unaryOperatorMap[ opr] == 2 then
				defineCall( constexpr_type, constexpr_type, opr, {constexpr_type}, constexprOperatorMap[ opr])
			end
		end
	end
	local oprlist = constexprTypeOperatorMap[ constexprFloatType]
	for oi,opr in ipairs(oprlist) do
		definePromoteCall( constexprFloatType, constexprIntegerType, constexprFloatType, opr, {constexprFloatType}, function(this) return this:tonumber() end)
	end
	local oprlist = constexprTypeOperatorMap[ constexprUIntegerType]
	for oi,opr in ipairs(oprlist) do
		if not unaryOperatorMap[ opr] then
			definePromoteCall( constexprUIntegerType, constexprIntegerType, constexprUIntegerType, opr, {constexprUIntegerType}, function(this) return this end)
		end
	end
	typedb:def_reduction( constexprBooleanType, constexprIntegerType, function( value) return value ~= "0" end, tag_TypeConversion, 0.25)
	typedb:def_reduction( constexprBooleanType, constexprFloatType, function( value) return math.abs(value) < math.abs(epsilon) end, tag_TypeConversion, 0.25)
	typedb:def_reduction( constexprFloatType, constexprIntegerType, function( value) return value:tonumber() end, tag_TypeConversion, 0.25)
	typedb:def_reduction( constexprIntegerType, constexprFloatType, function( value) return bcd.int( value) end, tag_TypeConversion, 0.25)
	typedb:def_reduction( constexprIntegerType, constexprUIntegerType, function( value) return value end, tag_TypeConversion, 0.125)
	typedb:def_reduction( constexprUIntegerType, constexprIntegerType, function( value) return value end, tag_TypeConversion, 0.125)

	local function bool2bcd( value) if value then return bcd.int("1") else return bcd.int("0") end end
	typedb:def_reduction( constexprIntegerType, constexprBooleanType, bool2bcd, tag_TypeConversion, 0.25)
end
-- Define all basic types associated with a type name
function defineQualifiedTypes( contextTypeId, typnam, typeDescription)
	local pointerTypeDescription = llvmir.pointerDescr( typeDescription)
	local pointerPointerTypeDescription = llvmir.pointerDescr( pointerTypeDescription)

	local lval = typedb:def_type( contextTypeId, typnam)				-- L-value | plain typedef
	local c_lval = typedb:def_type( contextTypeId, "const " .. typnam)		-- const L-value
	local rval = typedb:def_type( contextTypeId, "&" .. typnam)			-- reference
	local c_rval = typedb:def_type( contextTypeId, "const&" .. typnam)		-- const reference
	local pval = typedb:def_type( contextTypeId, "^" .. typnam)			-- pointer
	local c_pval = typedb:def_type( contextTypeId, "const^" .. typnam)		-- const pointer
	local rpval = typedb:def_type( contextTypeId, "^&" .. typnam)			-- pointer reference
	local c_rpval = typedb:def_type( contextTypeId, "const^&" .. typnam)		-- const pointer reference

	varargParameterMap[ lval]    = {llvmtype=typeDescription.llvmtype,type=c_lval}
	varargParameterMap[ c_lval]  = {llvmtype=typeDescription.llvmtype,type=c_lval}
	varargParameterMap[ rval]    = {llvmtype=typeDescription.llvmtype,type=c_lval}
	varargParameterMap[ c_rval]  = {llvmtype=typeDescription.llvmtype,type=c_lval}
	varargParameterMap[ pval]    = {llvmtype=pointerTypeDescription.llvmtype,type=c_pval}
	varargParameterMap[ c_pval]  = {llvmtype=pointerTypeDescription.llvmtype,type=c_pval}
	varargParameterMap[ rpval]   = {llvmtype=pointerTypeDescription.llvmtype,type=c_pval}
	varargParameterMap[ c_rpval] = {llvmtype=pointerTypeDescription.llvmtype,type=c_pval}

	typeDescriptionMap[ lval]    = typeDescription
	typeDescriptionMap[ c_lval]  = typeDescription
	typeDescriptionMap[ rval]    = pointerTypeDescription
	typeDescriptionMap[ c_rval]  = pointerTypeDescription
	typeDescriptionMap[ pval]    = pointerTypeDescription
	typeDescriptionMap[ c_pval]  = pointerTypeDescription
	typeDescriptionMap[ rpval]   = pointerPointerTypeDescription
	typeDescriptionMap[ c_rpval] = pointerPointerTypeDescription

	referenceTypeMap[ lval]   = rval
	referenceTypeMap[ c_lval] = c_rval
	referenceTypeMap[ pval]   = rpval
	referenceTypeMap[ c_pval] = c_rpval

	dereferenceTypeMap[ rval]    = lval
	dereferenceTypeMap[ c_rval]  = c_lval
	dereferenceTypeMap[ rpval]   = pval
	dereferenceTypeMap[ c_rpval] = c_pval

	constTypeMap[ lval]  = c_lval
	constTypeMap[ rval]  = c_rval
	constTypeMap[ pval]  = c_pval
	constTypeMap[ rpval] = c_rpval

	pointerTypeMap[ lval]   = pval
	pointerTypeMap[ c_lval] = c_pval
	pointerTypeMap[ rval]   = pval
	pointerTypeMap[ c_rval] = c_pval

	typeQualiSepMap[ lval]    = {lval=lval,qualifier=""}
	typeQualiSepMap[ c_lval]  = {lval=lval,qualifier="const "}
	typeQualiSepMap[ rval]    = {lval=lval,qualifier="&"}
	typeQualiSepMap[ c_rval]  = {lval=lval,qualifier="const&"}
	typeQualiSepMap[ pval]    = {lval=lval,qualifier="^"}
	typeQualiSepMap[ c_pval]  = {lval=lval,qualifier="const^"}
	typeQualiSepMap[ rpval]   = {lval=lval,qualifier="^&"}
	typeQualiSepMap[ c_rpval] = {lval=lval,qualifier="const^&"}

	typedb:def_reduction( c_pval, constexprNullType, constConstructor("null"), tag_TypeConversion, 0.03125)
 	typedb:def_reduction( lval, c_rval, convConstructor( typeDescription.load), tag_typeDeduction, 0.25)
	typedb:def_reduction( pval, rpval, convConstructor( pointerTypeDescription.load), tag_typeDeduction, 0.125)
	typedb:def_reduction( c_pval, c_rpval, convConstructor( pointerTypeDescription.load), tag_typeDeduction, 0.0625)

	typedb:def_reduction( c_lval, lval, nil, tag_typeDeduction, 0.125)
	typedb:def_reduction( c_rval, rval, nil, tag_typeDeduction, 0.125)
	typedb:def_reduction( c_pval, pval, nil, tag_typeDeduction, 0.125)
	typedb:def_reduction( c_rpval, rpval, nil, tag_typeDeduction, 0.125)

	defineCall( pval, rval, "&", {}, function(this) return this end)
	defineCall( rval, pval, "->", {}, function(this) return this end)
	defineCall( c_rval, c_pval, "->", {}, function(this) return this end)

	qualiTypeMap[ lval] = {lval=lval, c_lval=c_lval, rval=rval, c_rval=c_rval, pval=pval, c_pval=c_pval, rpval=rpval, c_rpval=c_rpval}
	return qualiTypeMap[ lval]
end
-- Define all R-Value reference types for type definitions representable by R-Value reference types (custom defined complex types)
function defineRValueReferenceTypes( contextTypeId, typnam, typeDescription, qualitype)
	local pointerTypeDescription = llvmir.pointerDescr( typeDescription)

	local rval_ref = typedb:def_type( contextTypeId, "&&" .. typnam)		-- R-value reference
	local c_rval_ref = typedb:def_type( contextTypeId, "const&&" .. typnam)		-- constant R-value reference

	qualitype.rval_ref = rval_ref
	qualitype.c_rval_ref = c_rval_ref

	varargParameterMap[ rval_ref]   = {llvmtype=typeDescription.llvmtype,type=c_lval}
	varargParameterMap[ c_rval_ref] = {llvmtype=typeDescription.llvmtype,type=c_lval}

	typeDescriptionMap[ rval_ref]   = pointerTypeDescription
	typeDescriptionMap[ c_rval_ref] = pointerTypeDescription

	constTypeMap[ rval_ref] = c_rval_ref

	typeQualiSepMap[ rval_ref]   = {lval=lval,qualifier="&&"}
	typeQualiSepMap[ c_rval_ref] = {lval=lval,qualifier="const&&"}

	rvalueTypeMap[ qualitype.lval] = rval_ref;
	rvalueTypeMap[ qualitype.c_lval] = c_rval_ref;

	typedb:def_reduction( c_rval_ref, rval_ref, nil, tag_typeDeduction, 0.125/4)
	typedb:def_reduction( qualitype.c_rval, c_rval_ref, constReferenceFromRvalueReferenceConstructor( typeDescription), tag_typeDeduction, 0.125/4)
	typedb:def_reduction( qualitype.c_rval, rval_ref, constReferenceFromRvalueReferenceConstructor( typeDescription), tag_typeDeduction, 0.125/4)

	defineCall( qualitype.rval, qualitype.rval, ":=", {rval_ref}, rvalueReferenceMoveConstructor)
	defineCall( qualitype.c_rval, qualitype.c_rval, ":=", {c_rval_ref}, rvalueReferenceMoveConstructor)
end
-- Define public/private types for implementing visibility/accessibility in different contexts
function definePublicPrivate( contextTypeId, typnam, typeDescription, qualitype)
	local pointerTypeDescription = llvmir.pointerDescr( typeDescription)

	local priv_rval = typedb:def_type( contextTypeId, "private &" .. typnam)
	local priv_c_rval = typedb:def_type( contextTypeId, "private const&" .. typnam)
	local priv_pval = typedb:def_type( contextTypeId, "private ^" .. typnam)
	local priv_c_pval = typedb:def_type( contextTypeId, "private const^" .. typnam)

	qualitype.priv_rval = priv_rval
	qualitype.priv_c_rval = priv_c_rval
	qualitype.priv_pval = priv_pval
	qualitype.priv_c_pval = priv_c_pval

	varargParameterMap[ priv_rval] = {llvmtype=typeDescription.llvmtype,type=qualitype.c_lval}
	varargParameterMap[ priv_c_rval] = {llvmtype=typeDescription.llvmtype,type=qualitype.c_lval}
	varargParameterMap[ priv_pval] = {llvmtype=pointerTypeDescription.llvmtype,type=qualitype.c_pval}
	varargParameterMap[ priv_c_pval] = {llvmtype=pointerTypeDescription.llvmtype,type=qualitype.c_pval}

	typeDescriptionMap[ priv_rval] = pointerTypeDescription
	typeDescriptionMap[ priv_c_rval] = pointerTypeDescription
	typeDescriptionMap[ priv_pval] = pointerTypeDescription
	typeDescriptionMap[ priv_c_pval] = pointerTypeDescription

	referenceTypeMap[ priv_pval] = qualitype.rpval
	referenceTypeMap[ priv_c_pval] = qualitype.c_rpval

	constTypeMap[ priv_rval] = priv_c_rval
	constTypeMap[ priv_pval] = priv_c_pval

	privateTypeMap[ qualitype.rval] = priv_rval
	privateTypeMap[ qualitype.c_rval] = priv_c_rval
	privateTypeMap[ qualitype.pval] = priv_pval
	privateTypeMap[ qualitype.c_pval] = priv_c_pval

	pointerTypeMap[ priv_rval] = priv_pval
	pointerTypeMap[ priv_c_rval] = priv_c_pval

	typeQualiSepMap[ priv_rval] = {lval=qualitype.lval,qualifier="private &"}
	typeQualiSepMap[ priv_c_rval] = {lval=qualitype.lval,qualifier="private const&"}
	typeQualiSepMap[ priv_pval] = {lval=qualitype.lval,qualifier="private ^"}
	typeQualiSepMap[ priv_c_pval] = {lval=qualitype.lval,qualifier="private const^"}

	typedb:def_reduction( qualitype.rval, priv_rval, nil, tag_typeDeduction, 0.125/16)
	typedb:def_reduction( qualitype.c_rval, priv_c_rval, nil, tag_typeDeduction, 0.125/16)
	typedb:def_reduction( qualitype.pval, priv_pval, nil, tag_typeDeduction, 0.125/16)
	typedb:def_reduction( qualitype.c_pval, priv_c_pval, nil, tag_typeDeduction, 0.125/16)

	defineCall( priv_pval, priv_rval, "&", {}, function(this) return this end)
	defineCall( priv_rval, priv_pval, "->", {}, function(this) return this end)
	defineCall( priv_c_rval, priv_c_pval, "->", {}, function(this) return this end)
end
-- Define constructurs and assignment operations for pointer types
function definePointerConstructors( qualitype, descr)
	local pointer_descr = llvmir.pointerDescr( descr)
	defineCall( qualitype.rpval, qualitype.rpval, "=",  {qualitype.c_pval}, assignConstructor( pointer_descr.assign))
	defineCall( qualitype.rpval, qualitype.rpval, ":=", {qualitype.c_pval}, assignConstructor( pointer_descr.assign))
	defineCall( qualitype.rpval, qualitype.rpval, ":=", {qualitype.c_rpval}, assignConstructor( pointer_descr.ctor_copy))
	defineCall( qualitype.rpval, qualitype.rpval, ":=", {}, manipConstructor( pointer_descr.ctor))
	defineCall( 0, qualitype.rpval, ":~", {}, constructorStructEmpty)
end
function defineScalarDestructors( qualitype, descr)
	defineCall( 0, qualitype.pval, " delete", {}, constructorStructEmpty)
	defineCall( 0, qualitype.rval, ":~", {}, constructorStructEmpty)
end
function defineScalarConstructors( qualitype, descr)
	defineCall( qualitype.rval, qualitype.rval, "=",  {qualitype.c_lval}, assignConstructor( descr.assign))
	defineCall( qualitype.rval, qualitype.rval, ":=", {qualitype.c_lval}, assignConstructor( descr.assign))	
	defineCall( qualitype.rval, qualitype.rval, ":=", {qualitype.c_rval}, assignConstructor( descr.ctor_copy))
	defineCall( qualitype.rval, qualitype.rval, ":=", {}, manipConstructor( descr.ctor))
	definePointerConstructors( qualitype, descr)
	defineScalarDestructors( qualitype, descr)
end
-- Define constructors for implicitely defined array types (when declaring a variable int a[30], then a type int[30] is implicitely declared) 
function defineArrayConstructors( node, qualitype, descr, memberTypeId)
	definePointerConstructors( qualitype, descr)

	instantCallableContext = createCallableContext( node.scope, nil)
	local c_memberTypeId = constTypeMap[memberTypeId] or memberTypeId
	local ths = {type=memberTypeId, constructor={out="%ths"}}
	local oth = {type=c_memberTypeId, constructor={out="%oth"}}
	local init = tryApplyCallable( node, ths, ":=", {} )
	local initcopy = tryApplyCallable( node, ths, ":=", {oth} )
	local assign = tryApplyCallable( node, ths, "=", {oth} )
	local destroy = tryApplyCallable( node, ths, ":~", {} )
	if init then
		print_section( "Auto", utils.template_format( descr.ctorproc, {ctors=init.constructor.code}))
		defineCall( qualitype.rval, qualitype.rval, ":=", {}, manipConstructor( descr.ctor))
	end
	if initcopy then
		print_section( "Auto", utils.template_format( descr.ctorproc_copy, {procname="copy",ctors=initcopy.constructor.code}))
		defineCall( qualitype.rval, qualitype.rval, ":=", {qualitype.c_rval}, assignConstructor( utils.template_format( descr.ctor_copy, {procname="copy"})))
	end
	if assign then
		print_section( "Auto", utils.template_format( descr.ctorproc_copy, {procname="assign",ctors=assign.constructor.code}))
		defineCall( qualitype.rval, qualitype.rval, "=", {qualitype.c_rval}, assignConstructor( utils.template_format( descr.ctor_copy, {procname="assign"})))
	end
	if destroy then
		print_section( "Auto", utils.template_format( descr.dtorproc, {dtors=destroy.constructor.code}))
		defineCall( 0, qualitype.rval, ":~", {}, manipConstructor( descr.dtor))
		defineCall( 0, qualitype.pval, " delete", {}, manipConstructor( descr.dtor))
	end
	instantCallableContext = nil
end
-- Define constructors for 'struct' types
function defineStructConstructors( node, qualitype, descr, context)
	definePointerConstructors( qualitype, descr)

	instantCallableContext = createCallableContext( node.scope, nil)
	local ctors,dtors,ctors_copy,ctors_assign,ctors_elements,paramstr,argstr,elements = "","","","","","","",{}

	for mi,member in ipairs(context.members) do
		table.insert( elements, member.type)
		paramstr = paramstr .. ", " .. typeDescriptionMap[ member.type].llvmtype .. " %p" .. mi
		argstr = argstr .. ", " .. typeDescriptionMap[ member.type].llvmtype .. " {arg" .. mi .. "}"

		local out = instantCallableContext.register()
		local inp = instantCallableContext.register()
		local llvmtype = member.descr.llvmtype
		local c_member_type = constTypeMap[ member.type] or member.type
		local member_reftype = referenceTypeMap[ member.type]
		local c_member_reftype = constTypeMap[ member_reftype] or member_reftype
		local loadref = context.descr.loadelemref
		local ths = {type=member_reftype,constructor={code=utils.constructor_format(loadref,{out=out,this="%ptr",index=mi-1, type=llvmtype}),out=out}}
		local oth = {type=c_member_reftype,constructor={code=utils.constructor_format(loadref,{out=inp,this="%oth",index=mi-1, type=llvmtype}),out=inp}}

		local member_init = tryApplyCallable( node, ths, ":=", {} )
		local member_initcopy = tryApplyCallable( node, ths, ":=", {oth} )
		local member_assign = tryApplyCallable( node, ths, "=", {oth} )
		local member_element = tryApplyCallable( node, ths, ":=", {type=c_member_type,constructor={out="%p" .. mi}} )
		local member_destroy = tryApplyCallable( node, ths, ":~", {} )

		if member_init and ctors then ctors = ctors .. member_init.constructor.code else ctors = nil end
		if member_initcopy and ctors_copy then ctors_copy = ctors_copy .. member_initcopy.constructor.code else ctors_copy = nil end
		if member_assign and ctors_assign then ctors_assign = ctors_assign .. member_assign.constructor.code else ctors_assign = nil end
		if member_element and ctors_elements then ctors_elements = ctors_elements .. member_element.constructor.code else ctors_elements = nil end
		if member_destroy and dtors then dtors = dtors .. member_destroy.constructor.code else dtors = nil end
	end
	if ctors then
		print_section( "Auto", utils.template_format( descr.ctorproc, {ctors=ctors}))
		defineCall( qualitype.rval, qualitype.rval, ":=", {}, manipConstructor( descr.ctor))
	end
	if ctors_copy then
		print_section( "Auto", utils.template_format( descr.ctorproc_copy, {procname="copy",ctors=ctors_copy}))
		defineCall( qualitype.rval, qualitype.rval, ":=", {qualitype.rval}, assignConstructor( utils.template_format( descr.ctor_copy, {procname="copy"})))
	end
	if ctors_assign then
		print_section( "Auto", utils.template_format( descr.ctorproc_copy, {procname="assign",ctors=ctors_assign}))
		defineCall( qualitype.rval, qualitype.rval, "=", {qualitype.rval}, assignConstructor( utils.template_format( descr.ctor_copy, {procname="assign"})))
	end
	if ctors_elements then
		print_section( "Auto", utils.template_format( descr.ctorproc_elements, {procname="assign",ctors=ctors_elements,paramstr=paramstr}))
		defineCall( qualitype.rval, qualitype.rval, ":=", elements, callConstructor( utils.template_format( descr.ctor_elements, {args=argstr})))
	end
	if dtors then
		print_section( "Auto", utils.template_format( descr.dtorproc, {dtors=dtors}))
		defineCall( 0, qualitype.rval, ":~", {}, manipConstructor( descr.dtor))
		defineCall( 0, qualitype.pval, " delete", {}, manipConstructor( descr.dtor))
	end
	defineCall( qualitype.rval, qualitype.rval, ":=", {constexprStructureType}, memberwiseAssignStructureConstructor( node, qualitype.lval, context.members))
	typedb:def_reduction( anyStructPointerType, qualitype.pval, nil, tag_pointerReinterpretCast)
	typedb:def_reduction( anyConstStructPointerType, qualitype.c_pval, nil, tag_pointerReinterpretCast)
	typedb:def_reduction( qualitype.pval, anyStructPointerType, nil, tag_pointerReinterpretCast)
	typedb:def_reduction( qualitype.c_pval, anyConstStructPointerType, nil, tag_pointerReinterpretCast)
	instantCallableContext = nil
end
-- Define constructors for 'interface' types
function defineInterfaceConstructors( node, qualitype, descr, context)
	definePointerConstructors( qualitype, descr)
	defineCall( qualitype.rval, qualitype.rval, ":=", {}, manipConstructor( descr.ctor))
	defineCall( qualitype.rval, qualitype.rval, ":=", {qualitype.rval}, assignConstructor( descr.ctor_copy))
	defineCall( qualitype.rval, qualitype.rval, ":=", {qualitype.c_lval}, assignConstructor( descr.assign))
	defineCall( qualitype.rval, qualitype.rval, "=", {qualitype.rval}, assignConstructor( descr.ctor_copy))
end
-- Define constructors for 'class' types
function defineClassConstructors( node, qualitype, descr, context)
	definePointerConstructors( qualitype, descr)

	instantCallableContext = createCallableContext( node.scope, nil)
	local dtors = ""
	for mi,member in ipairs(context.members) do
		local out = instantCallableContext.register()
		local llvmtype = member.descr.llvmtype
		local member_reftype = referenceTypeMap[ member.type]
		local loadref = context.descr.loadelemref
		local ths = {type=member_reftype,constructor={code=utils.constructor_format(loadref,{out=out,this="%ptr",index=mi-1, type=llvmtype}),out=out}}

		local member_destroy = tryApplyCallable( node, ths, ":~", {} )

		if member_destroy and dtors then dtors = dtors .. member_destroy.constructor.code else dtors = nil end
	end
	if dtors then
		print_section( "Auto", utils.template_format( descr.dtorproc, {dtors=dtors}))
		defineCall( 0, qualitype.rval, ":~", {}, manipConstructor( descr.dtor), -1)
		defineCall( 0, qualitype.pval, " delete", {}, manipConstructor( descr.dtor), -1)
	end
	typedb:def_reduction( anyClassPointerType, qualitype.pval, nil, tag_pointerReinterpretCast)
	typedb:def_reduction( anyConstClassPointerType, qualitype.c_pval, nil, tag_pointerReinterpretCast)
	typedb:def_reduction( qualitype.pval, anyClassPointerType, nil, tag_pointerReinterpretCast)
	typedb:def_reduction( qualitype.c_pval, anyConstClassPointerType, nil, tag_pointerReinterpretCast)
	instantCallableContext = nil
end
-- Iterate through all interfaces defined and build the VMT tables of these interfaces related to this class defined
function defineInheritedInterfaces( node, context, classTypeId)
	for ii,iTypeId in ipairs(context.interfaces) do
		local idescr = typeDescriptionMap[ iTypeId]
		local cdescr = context.descr
		local vmtstr = ""
		local sep = "\n\t"
		for mi,imethod in ipairs(idescr.methods) do
			local mk = context.methodmap[ imethod.methodid]
			if mk then
				cmethod = context.methods[ mk]
				vmtstr = vmtstr .. sep
					.. utils.template_format( llvmir.control.vtableElement,
								{interface_signature=imethod.llvmtype, class_signature=cmethod.llvmtype, symbolname=cmethod.symbol})
				sep = ",\n\t"
			else
				utils.errorMessage( node.line, "Method '%s' of class '%s' not implemented as required by interface '%s'",
							imethod.methodname, typedb:type_name(classTypeId), typedb:type_name(iTypeId))
			end
		end
		print_section( "Auto", utils.template_format( llvmir.control.vtable,
					{classname=cdescr.classname, interfacename=idescr.interfacename, llvmtype=vmtstr}))
	end
end
-- Get the type assigned to the variable 'this' or implicitely added to the context of a method in its body
function getFunctionThisType( private, const, thisType)
	if private == true then thisType = privateTypeMap[ thisType] end
	if const == true then thisType = constTypeMap[ thisType] end
	return thisType
end
-- Define the attributes assigned to an operator, collected for the decision if to implement it with a recursive structure as argument (e.g. a + {3,24, 5.13})
function defineOperatorAttributes( context, descr)
	local contextType = getFunctionThisType( descr.private, descr.const, context.qualitype.rval)
	local def = context.operators[ descr.name]
	if def then
		if #descr.param > def.maxNofArguments then def.maxNofArguments = #descr.param end
		if def.contextType ~= contextType or def.ret ~= descr.ret then def.hasStructArgument = false end
	else
		context.operators[ descr.name] = {contextType = contextType, ret = descr.ret, hasStructArgument = true, maxNofArguments = #descr.param}
	end
end
-- Iterate through operator declarations and implement them with a recursive structure as argument (e.g. a + {3,24, 5.13}) if possible
function defineOperatorsWithStructArgument( node, context)
	for opr,def in pairs( context.operators) do
		if def.hasStructArgument == true then
			defineCall( def.returnType, def.contextType, opr, {constexprStructureType}, resolveOperatorConstructor( node, def.contextType, opr))
		elseif def.maxNofArguments > 1 then
			utils.errorMessage( node.line, "Operator '%s' defined different instances with more than one argument, but with varying signature", opr)
		end
	end
end
-- Define index operators for pointers to first class citizen scalar types
function defineIndexOperators( element_qualitype, pointer_descr)
	for index_typenam, index_type in pairs(scalarIndexTypeMap) do
		defineCall( element_qualitype.rval, element_qualitype.pval, "[]", {index_type}, callConstructor( pointer_descr.index[ index_typnam]))
		defineCall( element_qualitype.c_rval, element_qualitype.c_pval, "[]", {index_type}, callConstructor( pointer_descr.index[ index_typnam]))
	end
end
-- Define pointer comparison operators if defined in the type description
function definePointerOperators( priv_c_pval, c_pval, pointer_descr)
	for operator,operator_fmt in pairs( pointer_descr.cmpop) do
		defineCall( scalarBooleanType, c_pval, operator, {c_pval}, callConstructor( operator_fmt))
		if priv_c_pval then defineCall( scalarBooleanType, priv_c_pval, operator, {c_pval}, callConstructor( operator_fmt)) end
	end
end
-- Define built-in type conversions for first class citizen scalar types
function defineBuiltInTypeConversions( typnam, descr)
	local qualitype = scalarQualiTypeMap[ typnam]
	if descr.conv then
		for oth_typenam,conv in pairs( descr.conv) do
			local oth_qualitype = scalarQualiTypeMap[ oth_typenam]
			local conv_constructor; if conv.fmt then conv_constructor = convConstructor( conv.fmt) else conv_constructor = nil end
			typedb:def_reduction( qualitype.c_lval, oth_qualitype.c_lval, conv_constructor, tag_TypeConversion, conv.weight)
		end
	end
end
-- Define built-in promote calls for first class citizen scalar types
function defineBuiltInTypePromoteCalls( typnam, descr)
	local qualitype = scalarQualiTypeMap[ typnam]
	for i,promote_typnam in ipairs( descr.promote) do
		local promote_qualitype = scalarQualiTypeMap[ promote_typnam]
		local promote_descr = typeDescriptionMap[ promote_qualitype.lval]
		local promote_conv_fmt = promote_descr.conv[ typnam].fmt
		local promote_conv; if promote_conv_fmt then promote_conv = convConstructor( promote_conv_fmt) else promote_conv = nil end
		if promote_descr.binop then
			for operator,operator_fmt in pairs( promote_descr.binop) do
				definePromoteCall( promote_qualitype.lval, qualitype.c_lval, promote_qualitype.c_lval, operator, {promote_qualitype.c_lval}, promote_conv)
			end
		end
		if promote_descr.cmpop then
			for operator,operator_fmt in pairs( promote_descr.cmpop) do
				definePromoteCall( scalarBooleanType, qualitype.c_lval, promote_qualitype.c_lval, operator, {promote_qualitype.c_lval}, promote_conv)
			end
		end
	end
end
-- Define built-in unary,binary operators for first class citizen scalar types
function defineBuiltInTypeOperators( typnam, descr)
	local qualitype = scalarQualiTypeMap[ typnam]
	local constexprTypes = typeClassToConstExprTypesMap[ descr.class]
	if descr.unop then
		for operator,operator_fmt in pairs( descr.unop) do
			defineCall( qualitype.lval, qualitype.c_lval, operator, {}, callConstructor( operator_fmt))
		end
	end
	if descr.binop then
		for operator,operator_fmt in pairs( descr.binop) do
			defineCall( qualitype.lval, qualitype.c_lval, operator, {qualitype.c_lval}, callConstructor( operator_fmt))
			for i,constexprType in ipairs(constexprTypes) do
				defineCall( qualitype.lval, qualitype.c_lval, operator, {constexprType}, callConstructor( operator_fmt))
			end
		end
	end
	if descr.cmpop then
		for operator,operator_fmt in pairs( descr.cmpop) do
			defineCall( scalarBooleanType, qualitype.c_lval, operator, {qualitype.c_lval}, callConstructor( operator_fmt))
			for i,constexprType in ipairs(constexprTypes) do
				defineCall( scalarBooleanType, qualitype.c_lval, operator, {constexprType}, callConstructor( operator_fmt))
			end
		end
	end
end
-- Get the list of context types associated with the current scope used for resolving types
function getContextTypes()
	local rt = typedb:get_instance( "context")
	if rt then return rt else return {0} end
end
-- Extend the list of context types associated with the current scope used for resolving types by one type/constructor pair structure
function addContextTypeConstructorPair( val)
	local defcontext = typedb:this_instance( "context")
	if defcontext then
		table.insert( defcontext, val)
	else
		defcontext = typedb:get_instance( "context")
		if defcontext then
			-- inherit context from enclosing scope and add new element
			local defcontext_copy = {}
			for di,ctx in ipairs(defcontext) do
				table.insert( defcontext_copy, ctx)
			end
			table.insert( defcontext_copy, val)
			typedb:set_instance( "context", defcontext_copy)
		else
			-- create empty context and add new element
			defcontext = {0,val}
			typedb:set_instance( "context", defcontext)
		end
	end
end
-- This function checks if an array type defined by element type and size already exists and creates it implicitely if not, return the array type handle 
function implicitDefineArrayType( node, elementTypeId, arsize)
	local element_sep = typeQualiSepMap[ elementTypeId]
	local descr = llvmir.arrayDescr( typeDescriptionMap[ elementTypeId], arsize)
	local typenam = "[" .. arsize .. "]"
	local qualitypenam = element_sep.qualifier .. typenam
	local typekey = element_sep.lval .. typenam
	local lval = arrayTypeMap[ typekey]
	local rt
	if lval then
		local scopebk = typedb:scope( typedb:type_scope( element_sep.lval))
		rt = typedb:get_type( element_sep.lval, qualitypenam)
		typedb:scope( scopebk)
	else
		local scopebk = typedb:scope( typedb:type_scope( element_sep.lval))
		local qualitype = defineQualifiedTypes( element_sep.lval, typnam, descr)
		defineRValueReferenceTypes( element_sep.lval, typnam, descr, qualitype)
		defineArrayConstructors( node, qualitype, descr, elementTypeId)
		rt = typedb:get_type( element_sep.lval, qualitypenam)
		typedb:scope( scopebk)
		arrayTypeMap[ typekey] = qualitype.lval
		definePointerOperators( qualitype.priv_c_pval, qualitype.c_pval, typeDescriptionMap[ qualitype.c_pval])
	end
	return rt
end
function getFrame()
	local rt = typedb:this_instance( "frame")
	if not rt then	
		local label = utils.label_allocator()
		rt = {entry=label(), register=utils.register_allocator(), label=label}
		typedb:set_instance( "frame", rt)
	end
	return rt
end
function setInitCode( descr, this, initVal)
	local frame = getFrame()
	if initVal then
		frame.ctor = (frame.ctor or "") .. utils.constructor_format( descr.ctor_copy, {this=this,arg1=initVal})
	else
		frame.ctor = (frame.ctor or "") .. utils.constructor_format( descr.ctor, {this=this})
	end
end
function setCleanupCode( descr, this)
	local frame = getFrame()
	if not frame.dtor then frame.dtor = utils.template_format( llvmir.control.label,{inp=frame.entry}) end
	frame.dtor = frame.dtor .. utils.constructor_format( descr.dtor, {this=this})
end
function getFrameCodeBlock( code)
	local frame = typedb:this_instance( "frame")
	if frame then
		local rt = ""
		if frame.ctor then rt = rt .. frame.ctor end
		rt = rt .. code
		if frame.dtor then rt = rt .. frame.dtor end
		return rt
	else
		return code
	end
end
-- Hardcoded variable definition (variable not declared in source, but implicitely declared, for example the 'this' pointer in a method body context)
function defineVariableHardcoded( typeId, name, reg)
	local var = typedb:def_type( 0, name, reg)
	typedb:def_reduction( typeId, var, nil, tag_typeDeclaration)
	return var
end
-- Define a free variable or a member variable (depending on the context)
function defineVariable( node, context, typeId, name, initVal)
	local descr = typeDescriptionMap[ typeId]
	local callable = getCallableContext()
	local refTypeId = referenceTypeMap[ typeId]
	if not refTypeId then utils.errorMessage( node.line, "References not allowed in variable declarations, use pointer instead") end
	if not context then
		local out = callable.register()
		local code = utils.constructor_format( descr.def_local, {out = out}, callable.register)
		local var = typedb:def_type( 0, name, out)
		typedb:def_reduction( refTypeId, var, nil, tag_typeDeclaration)
		local rt = {type=var, constructor={code=code,out=out}}
		if initVal then
			rt = applyCallable( node, rt, ":=", {initVal})
		else
			rt = applyCallable( node, rt, ":=", {})
		end
		if descr.dtor then setCleanupCode( descr, rt.constructor.out) end
		return rt
	elseif not context.qualitype then
		if type(initVal) == "table" then utils.errorMessage( node.line, "Only constexpr allowed to assign in global variable initialization") end
		out = "@" .. name
		if descr.scalar == true and initVal then
			print( utils.constructor_format( descr.def_global_val, {out = out, val = initVal}))
		else
			print( utils.constructor_format( descr.def_global, {out = out}))
			if initVal then
				if descr.ctor_copy then setInitCode( descr, out, initVal) end
			else
				if descr.ctor then setInitCode( descr, out) end
			end
			if descr.dtor then setCleanupCode( descr, out) end
		end
		local var = typedb:def_type( 0, name, out)
		typedb:def_reduction( refTypeId, var, nil, tag_typeDeclaration)
	else
		if initVal then utils.errorMessage( node.line, "No initialization value in definition of member variable allowed") end
		defineVariableMember( node, context, typeId, name, context.private)
	end
end
function expandContextLlvmMember( descr, context)
	if context.llvmtype == "" then context.llvmtype = descr.llvmtype else context.llvmtype = context.llvmtype  .. ", " .. descr.llvmtype end
end
-- Define a member variable of a class or a structure
function defineVariableMember( node, context, typeId, name, private)
	local descr = typeDescriptionMap[ typeId]
	local qualisep = typeQualiSepMap[ typeId]
	local memberpos = context.structsize
	local load_ref = utils.template_format( context.descr.loadelemref, {index=#context.members, type=descr.llvmtype})
	local load_val = utils.template_format( context.descr.loadelem, {index=#context.members, type=descr.llvmtype})

	while math.fmod( memberpos, descr.align) ~= 0 do memberpos = memberpos + 1 end
	context.structsize = memberpos + descr.size
	local member = {
		type = typeId,
		qualitype = qualiTypeMap[ qualisep.lval],
		qualifier = qualisep.qualifier,
		descr = descr,
		bytepos = memberpos,
		loadref = load_ref,
		load = load_val
	}
	table.insert( context.members, member)
	expandContextLlvmMember( descr, context)
	local r_typeId = referenceTypeMap[ typeId]
	local c_r_typeId = constTypeMap[ r_typeId]
	if private == true then
		defineCall( r_typeId, context.qualitype.priv_rval, name, {}, callConstructor( load_ref))
		defineCall( c_r_typeId, context.qualitype.priv_c_rval, name, {}, callConstructor( load_ref))
	else
		defineCall( r_typeId, context.qualitype.rval, name, {}, callConstructor( load_ref))
		defineCall( c_r_typeId, context.qualitype.c_rval, name, {}, callConstructor( load_ref))
		defineCall( typeId, context.qualitype.c_lval, name, {}, callConstructor( load_val))
	end
end
function defineInterfaceMember( node, context, typeId, typnam, private)
	local descr = typeDescriptionMap[ typeId]
	table.insert( context.interfaces, typeId)
	local fmt = utils.template_format( descr.getClassInterface, {classname=context.descr.classname})
	local thisType = getFunctionThisType( false, descr.const == true, context.qualitype.rval)
	defineCall( rvalueTypeMap[ typeId], thisType, typnam, {}, convConstructor( fmt, "RVAL"))
end
-- Define a reduction to a member variable to implement class/interface inheritance
function defineReductionToMember( objTypeId, name)
	memberTypeId = typedb:get_type( objTypeId, name)
	typedb:def_reduction( memberTypeId, objTypeId, typedb:type_constructor( memberTypeId), tag_typeDeclaration, 0.125/32)
end
-- Define the reductions implementing class inheritance
function defineClassInheritanceReductions( context, name, private, const)
	local contextType = getFunctionThisType( private, const, context.qualitype.rval)
	defineReductionToMember( contextType, name)
	if private == false and const == true then defineReductionToMember( context.qualitype.c_lval, name) end
end
-- Define the reductions implementing interface inheritance
function defineInterfaceInheritanceReductions( context, name, private, const)
	local contextType = getFunctionThisType( private, const, context.qualitype.rval)
	defineReductionToMember( contextType, name)
end
-- Make a function/procedure/operator parameter addressable by name in the callable body
function defineParameter( node, type, name, callable)
	local descr = typeDescriptionMap[ type]
	local paramreg = callable.register()
	local var = typedb:def_type( 0, name, paramreg)
	if var < 0 then utils.errorMessage( node.line, "Duplicate definition of parameter '%s'", name) end
	typedb:def_reduction( type, var, nil, tag_typeDeclaration)
	return {type=type, llvmtype=descr.llvmtype, reg=paramreg}
end
-- Initialize control boolean types used for implementing control structures and '&&','||' operators on booleans
function initControlBooleanTypes()
	controlTrueType = typedb:def_type( 0, " controlTrueType")
	controlFalseType = typedb:def_type( 0, " controlFalseType")

	local function falseExitToBoolean( constructor)
		local callable = getCallableContext()
		local out = callable.register()
		return {code = constructor.code .. utils.constructor_format(llvmir.control.falseExitToBoolean,{falseExit=constructor.out,out=out},callable.label),out=out}
	end
	local function trueExitToBoolean( constructor)
		local callable = getCallableContext()
		local out = callable.register()
		return {code = constructor.code .. utils.constructor_format( llvmir.control.trueExitToBoolean,{trueExit=constructor.out,out=out},callable.label),out=out}
	end
	typedb:def_reduction( scalarBooleanType, controlTrueType, falseExitToBoolean, tag_typeDeduction, 0.1)
	typedb:def_reduction( scalarBooleanType, controlFalseType, trueExitToBoolean, tag_typeDeduction, 0.1)

	local function booleanToFalseExit( constructor)
		local callable = getCallableContext()
		local out = callable.label()
		return {code = constructor.code .. utils.constructor_format( llvmir.control.booleanToFalseExit, {inp=constructor.out, out=out}, callable.label),out=out}
	end
	local function booleanToTrueExit( constructor)
		local callable = getCallableContext()
		local out = callable.label()
		return {code = constructor.code .. utils.constructor_format( llvmir.control.booleanToTrueExit, {inp=constructor.out, out=out}, callable.label),out=out}
	end

	typedb:def_reduction( controlTrueType, scalarBooleanType, booleanToFalseExit, tag_typeDeduction, 0.1)
	typedb:def_reduction( controlFalseType, scalarBooleanType, booleanToTrueExit, tag_typeDeduction, 0.1)

	local function negateControlTrueType( this) return {type=controlFalseType, constructor=this.constructor} end
	local function negateControlFalseType( this) return {type=controlTrueType, constructor=this.constructor} end

	local function joinControlTrueTypeWithBool( this, arg)
		local out = this.out
		local code2 = utils.constructor_format( llvmir.control.booleanToFalseExit, {inp=arg[1].out, out=out}, getCallableContext().label())
		return {code=this.code .. arg[1].code .. code2, out=out}
	end
	local function joinControlFalseTypeWithBool( this, arg)
		local out = this.out
		local code2 = utils.constructor_format( llvmir.control.booleanToTrueExit, {inp=arg[1].out, out=out}, getCallableContext().label())
		return {code=this.code .. arg[1].code .. code2, out=out}
	end
	defineCall( controlTrueType, controlFalseType, "!", {}, nil)
	defineCall( controlFalseType, controlTrueType, "!", {}, nil)
	defineCall( controlTrueType, controlTrueType, "&&", {scalarBooleanType}, joinControlTrueTypeWithBool)
	defineCall( controlFalseType, controlFalseType, "||", {scalarBooleanType}, joinControlFalseTypeWithBool)

	local function joinControlFalseTypeWithConstexprBool( this, arg)
		if arg == false then
			return this
		else 
			local callable = getCallableContext()
			return {code= this.code .. utils.constructor_format( llvmir.control.terminateTrueExit,{out=this.out},callable.label), out=this.out}
		end
	end
	local function joinControlTrueTypeWithConstexprBool( this, arg)
		if arg == true then
			return this
		else 
			local callable = getCallableContext()
			return {code= this.code .. utils.constructor_format( llvmir.control.terminateFalseExit,{out=this.out},callable.label), out=this.out}
		end
	end
	defineCall( controlTrueType, controlTrueType, "&&", {constexprBooleanType}, joinControlTrueTypeWithConstexprBool)
	defineCall( controlFalseType, controlFalseType, "||", {constexprBooleanType}, joinControlFalseTypeWithConstexprBool)

	local function constexprBooleanToControlTrueType( value)
		local callable = getCallableContext()
		local out = label()
		local code; if value == true then code="" else code=utils.constructor_format( llvmir.control.terminateFalseExit, {out=out}, callable.label) end
		return {code=code, out=out}
	end
	local function constexprBooleanToControlFalseType( value)
		local callable = getCallableContext()
		local out = label()
		local code; if value == false then code="" else code=utils.constructor_format( llvmir.control.terminateFalseExit, {out=out}, callable.label) end
		return {code=code, out=out}
	end
	typedb:def_reduction( controlFalseType, constexprBooleanType, constexprBooleanToControlFalseType, tag_typeDeduction, 0.1)
	typedb:def_reduction( controlTrueType, constexprBooleanType, constexprBooleanToControlTrueType, tag_typeDeduction, 0.1)

	local function invertControlBooleanType( this)
		local out = getCallableContext().label()
		return {code = this.code .. utils.constructor_format( llvmir.control.invertedControlType, {inp=this.out, out=out}), out = out}
	end
	typedb:def_reduction( controlFalseType, controlTrueType, invertControlBooleanType, tag_typeDeduction, 0.1)
	typedb:def_reduction( controlTrueType, controlFalseType, invertControlBooleanType, tag_typeDeduction, 0.1)

	definePromoteCall( controlTrueType, constexprBooleanType, controlTrueType, "&&", {scalarBooleanType}, constexprBooleanToControlTrueType)
	definePromoteCall( controlFalseType, constexprBooleanType, controlFalseType, "||", {scalarBooleanType}, constexprBooleanToControlFalseType)
end
-- Initialize all built-in types
function initBuiltInTypes()
	stringAddressType = typedb:def_type( 0, " stringAddressType")
	anyClassPointerType = typedb:def_type( 0, "class^")
	anyConstClassPointerType = typedb:def_type( 0, "const class^")
	anyStructPointerType = typedb:def_type( 0, "struct^")
	anyConstStructPointerType = typedb:def_type( 0, "const struct^")

	for typnam, scalar_descr in pairs( llvmir.scalar) do
		local qualitype = defineQualifiedTypes( 0, typnam, scalar_descr)
		defineScalarConstructors( qualitype, scalar_descr)
		local c_lval = qualitype.c_lval
		scalarQualiTypeMap[ typnam] = qualitype
		if scalar_descr.class == "bool" then
			scalarBooleanType = c_lval
		elseif scalar_descr.class == "unsigned" then
			if scalar_descr.llvmtype == "i8" then
				stringPointerType = qualitype.c_pval
				memPointerType = qualitype.pval
			end
			scalarIndexTypeMap[ typnam] = c_lval
		elseif scalar_descr.class == "signed" then
			if scalar_descr.llvmtype == "i32" then scalarIntegerType = qualitype.c_lval end
			if scalar_descr.llvmtype == "i8" and stringPointerType == 0 then
				stringPointerType = qualitype.c_pval
				memPointerType = qualitype.pval
			end
			scalarIndexTypeMap[ typnam] = c_lval
		end
		for i,constexprType in ipairs( typeClassToConstExprTypesMap[ scalar_descr.class]) do
			local weight = 0.25*(1.0-scalar_descr.sizeweight)
			typedb:def_reduction( qualitype.lval, constexprType, function(arg) return {code="",out=tostring(arg)} end, tag_typeDeduction, weight)
		end
	end
	if not scalarBooleanType then utils.errorMessage( 0, "No boolean type defined in built-in scalar types") end
	if not scalarIntegerType then utils.errorMessage( 0, "No integer type mapping to i32 defined in built-in scalar types (return value of main)") end
	if not stringPointerType then utils.errorMessage( 0, "No i8 type defined suitable to be used for free string constants)") end

	for typnam, scalar_descr in pairs( llvmir.scalar) do
		defineIndexOperators( scalarQualiTypeMap[ typnam], llvmir.pointerDescr( scalar_descr))
		defineBuiltInTypeConversions( typnam, scalar_descr)
		defineBuiltInTypeOperators( typnam, scalar_descr)
	end
	for typnam, scalar_descr in pairs( llvmir.scalar) do
		defineBuiltInTypePromoteCalls( typnam, scalar_descr)
	end
	defineConstExprArithmetics()
	initControlBooleanTypes()
end
-- Application of a conversion constructor depending on its type and its argument type
function applyConversionConstructor( node, typeId, constructor, arg)	
	if constructor then
		if (type(constructor) == "function") then
			return constructor( arg)
		elseif arg then
			utils.errorMessage( node.line, "reduction constructor overwriting previous constructor for '%s'", typedb:type_string(typeId))
		else
			return constructor
		end
	else
		return arg
	end
end
-- Get the handle of a type expected to have no arguments (plain typedef type or a variable name)
function selectNoArgumentType( node, typeName, resolveContextTypeId, reductions, items)
	if not resolveContextTypeId or type(resolveContextTypeId) == "table" then
		utils.errorResolveType( typedb, node.line, resolveContextTypeId, getContextTypes(), typeName)
	end
	for ii,item in ipairs(items) do
		if typedb:type_nof_parameters( item) == 0 then
			local constructor = nil
			for ri,redu in ipairs(reductions) do
				constructor = applyConversionConstructor( node, redu.type, redu.constructor, constructor)
			end
			local item_constructor = typedb:type_constructor( item)
			constructor = applyConversionConstructor( node, item, item_constructor, constructor)
			if constructor then
				local out,code = constructorParts( constructor)
				return item,{code=code,out=out}
			else
				return item
			end
		end
	end
	utils.errorMessage( node.line, "Failed to resolve %s with no arguments", utils.resolveTypeString( typedb, getContextTypes(), typeName))
end
-- Get the handle of a type expected to have no arguments and no constructor (plain typedef type)
function selectNoConstructorNoArgumentType( node, typeName, resolveContextTypeId, reductions, items)
	local rt,constructor = selectNoArgumentType( node, typeName, resolveContextTypeId, reductions, items)
	if constructor then utils.errorMessage( node.line, "No constructor type expected: %s", typeName) end
	return rt
end
-- Call a function, meaning to apply operator "()" on a callable
function callFunction( node, contextTypes, name, args)
	local typeId,constructor = selectNoArgumentType( node, name, typedb:resolve_type( contextTypes, name, tagmask_resolveType))
	local this = {type=typeId, constructor=constructor}
	return applyCallable( node, this, "()", args)
end
-- Get the constructor of a reduction to a specified type or nil if not possible for the operand type
function getDeriveTypeReductionConstructor( redulist, weight, altpath, redu_constructor)
	if not redulist then
		return nil
	elseif altpath then
		utils.errorMessage( node.line, "Ambiguous derivation paths for '%s': %s | %s",
					typedb:type_string(operand.type), utils.typeListString(typedb,altpath," =>"), utils.typeListString(typedb,redulist," =>"))
	end
	for ri,redu in ipairs(redulist) do
		if redu.constructor then
			redu_constructor = redu.constructor( redu_constructor)
		end
	end
	return redu_constructor,weight
end
-- Try to get the constructor and weight of an explicitely specified parameter type
function tryGetParameterReductionConstructor( node, redu_type, operand)
	if redu_type ~= operand.type then
		local redulist,weight,altpath = typedb:derive_type( redu_type, operand.type, tagmask_matchParameter, tagmask_typeConversion, 1)
		return getDeriveTypeReductionConstructor( redulist, weight, altpath, operand.constructor)
	else
		return operand.constructor,0.0
	end
end
-- Try to get the constructor,llvm type and weight of an explicitely specified parameter type
function tryGetParameterConstructor( node, redu_type, operand)
	local constructor,weight = tryGetParameterReductionConstructor( node, redu_type, operand)
	if weight then
		local vp = typeDescriptionMap[ redu_type]
		if vp then return constructor,vp.llvmtype,weight else return constructor,nil,weight end
	end
end
-- Try to get the constructor,llvm type and weight of a vararg parameter type
function tryGetVarargParameterConstructor( node, operand)
	local vp = varargParameterMap[ operand.type]
	if not vp then
		local redulist = typedb:get_reductions( operand.type, tagmask_declaration)
		for ri,redu in ipairs( redulist) do
			vp = varargParameterMap[ redu.type]
			if vp then break end
		end
	end
	if vp then
		local constructor,weight = tryGetParameterReductionConstructor( node, vp.type, operand)
		return constructor,vp.llvmtype,weight
	end
end
-- Get the constructor of an implicitly required type with the deduction tagmask passed as argument
function getRequiredTypeConstructor( node, redu_type, operand, tagmask)
	if redu_type ~= operand.type then
		local redulist,weight,altpath = typedb:derive_type( redu_type, operand.type, tagmask or tagmask_matchParameter, tagmask_typeConversion, 1)
		local rt = getDeriveTypeReductionConstructor( redulist, weight, altpath, operand.constructor)
		if not rt then utils.errorMessage( node.line, "Type mismatch, required type '%s'", typedb:type_string(redu_type)) end
		return rt
	else
		return operand.constructor
	end
end
-- Get the best matching item from a list of items by weighting the matching of the arguments to the item parameter types
function selectItemsMatchParameters( node, items, args, this_constructor)
	local bestmatch = {}
	local bestweight = nil
	for ii,item in ipairs(items) do
		local weight = 0.0
		local nof_params = typedb:type_nof_parameters( item)
		if nof_params <= #args then
			local param_constructor_ar = {}
			local param_llvmtype_ar = {}
			if nof_params == #args then
				if #args > 0 then
					local parameters = typedb:type_parameters( item)
					for pi=1,#parameters do
						local param_constructor,param_llvmtype,param_weight = tryGetParameterConstructor( node, parameters[ pi].type, args[ pi])
						if not param_weight then break end
						weight = weight + param_weight
						table.insert( param_constructor_ar, param_constructor)
						table.insert( param_llvmtype_ar, param_llvmtype)
					end
				end
			elseif varargFuncMap[ item] then
				if #args > 0 then
					local parameters = typedb:type_parameters( item)
					for pi=1,#parameters do
						local param_constructor,param_llvmtype,param_weight = tryGetParameterConstructor( node, parameters[ pi].type, args[ pi])
						if not param_weight then break end
						weight = weight + param_weight
						table.insert( param_constructor_ar, param_constructor)
						table.insert( param_llvmtype_ar, param_llvmtype)
					end
					if #parameters == #param_constructor_ar then
						for ai=#parameters+1,#args do
							local param_constructor,param_llvmtype,param_weight = tryGetVarargParameterConstructor( node, args[ai])
							if not param_weight then break end
							weight = weight + param_weight
							table.insert( param_constructor_ar, param_constructor)
							table.insert( param_llvmtype_ar, param_llvmtype)
						end
					end			
				end
			end
			if #param_constructor_ar == #args then
				if not bestweight or weight < bestweight + weightEpsilon then
					local item_constructor = typedb:type_constructor( item)
					local callable_constructor = item_constructor( this_constructor, param_constructor_ar, param_llvmtype_ar)
					if bestweight and weight >= bestweight - weightEpsilon then
						table.insert( bestmatch, {type=item, constructor=callable_constructor})
					else
						bestweight = weight
						bestmatch = {{type=item, constructor=callable_constructor}}
					end
				end
			end
		end
	end
	return bestmatch,bestweight
end
-- Find a callable specified by name in the context of the 'this' operand
function findApplyCallable( node, this, callable, args)
	local resolveContextType,reductions,items = typedb:resolve_type( this.type, callable, tagmask_resolveType)
	if not resolveContextType or type(resolveContextType) == "table" then utils.errorResolveType( typedb, node.line, resolveContextType, this.type, callable) end

	local this_constructor = this.constructor
	for ri,redu in ipairs(reductions) do
		if redu.constructor then
			this_constructor = redu.constructor( this_constructor)
		end
	end
	local bestmatch,bestweight = selectItemsMatchParameters( node, items, args or {}, this_constructor)
	return bestmatch,bestweight
end
function getCallableBestMatch( node, bestmatch, bestweight)
	if #bestmatch == 1 then
		return bestmatch[1]
	else
		local altmatchstr = ""
		for ii,bm in ipairs(bestmatch) do
			if altmatchstr ~= "" then
				altmatchstr = altmatchstr .. ", "
			end
			altmatchstr = altmatchstr .. typedb:type_string(bm.type)
		end
		utils.errorMessage( node.line, "Ambiguous matches resolving callable with signature '%s', list of candidates: %s",
				utils.resolveTypeString( typedb, this.type, callable) .. "(" .. utils.typeListString( typedb, args, ", ") .. ")", altmatchstr)
	end
end
-- Retrieve and apply a callable in a specified context
function applyCallable( node, this, callable, args)
	local bestmatch,bestweight = findApplyCallable( node, this, callable, args)
	if not bestweight then
		utils.errorMessage( node.line, "Failed to find callable with signature '%s'",
	                    utils.resolveTypeString( typedb, this.type, callable) .. "(" .. utils.typeListString( typedb, args, ", ") .. ")")
	end
	return getCallableBestMatch( node, bestmatch, bestweight)
end
-- Try to retrieve a callable in a specified context, apply it and return its type/constructor pair if found, return nil if not
function tryApplyCallable( node, this, callable, args)
	local bestmatch,bestweight = findApplyCallable( node, this, callable, args)
	if bestweight then return getCallableBestMatch( node, bestmatch, bestweight) else return nil end
end
-- Get the symbol name for a function in the LLVM output
function getTargetFunctionIdentifierString( name, args, context)
	if not context then
		return utils.uniqueName( name .. "__")
	else
		local pstr; if context.symbol then pstr = "__C_" .. context.symbol .. "__" .. name else pstr = name end
		for ai,arg in ipairs(args) do pstr = pstr .. "__" .. arg.llvmtype end
		return utils.encodeName(  pstr)
	end
end
-- Get the identifier of a method for matching interface/implementation
function getInterfaceMethodIdentifierString( symbol, args, const)
	local pstr = symbol
	for ai,arg in ipairs(args) do pstr = pstr .. "__" .. arg.llvmtype end
	if const == true then return utils.encodeName(  pstr) .. " const" else return utils.encodeName(  pstr) end
end
-- Get the identifier of a method for displaying interface/implementation matching
function getInterfaceMethodNameString( name, args)
	local pstr = name
	for ai,arg in ipairs(args) do local sep; if ai == 1 then sep = "(" else sep = ", " end; pstr = pstr .. sep .. typedb:type_string(arg.type) end
	if const == true then return pstr .. ") const" else return pstr .. ")" end
end
function getDeclarationLlvmParameterString( args)
	local rt = ""
	for ai,arg in ipairs(args) do if rt == "" then rt = rt .. arg.llvmtype .. " " .. arg.reg else rt = rt .. ", " .. arg.llvmtype .. " " .. arg.reg end end
	return rt
end
function getDeclarationLlvmParameterTypeString( args)
	local rt = ""
	for ai,arg in ipairs(args) do if rt == "" then rt = rt .. arg.llvmtype else rt = rt .. ", " .. arg.llvmtype end end
	return rt
end
function getThisParameterString( context, args)
	local rt = ""
	if context and context.qualitype then
		rt = typeDescriptionMap[ context.qualitype.pval].llvmtype .. " %ths"
		if #args >= 1 then rt = rt .. ", " end
	end
	return rt
end
function getParameterTypeList( args)
	rt = {}
	for ai,arg in ipairs(args) do table.insert(rt,arg.type) end
	return rt
end
function getTypeDeclContextTypeId( context)
	if context.qualitype then return context.qualitype.lval else return 0 end
end
function getTypeDeclUniqueName( contextTypeId, typnam)
	if contextTypeId == 0 then 
		return utils.uniqueName( typnam .. "__")
	else		
		return utils.uniqueName( typedb:type_string(contextTypeId) .. "__" .. typnam .. "__")
	end
end
-- Function shared by all expansions of the structure used for mapping the LLVM template for a function call
function expandDescrCallTemplateParameter( descr, context)
	descr.thisstr = getThisParameterString( context, descr.param)
	descr.argstr = getDeclarationLlvmParameterTypeString( descr.param)
	descr.symbolname = getTargetFunctionIdentifierString( descr.symbol, descr.param, context)
	if descr.ret then descr.rtype = typeDescriptionMap[ descr.ret].llvmtype else descr.rtype = "void" end
end
function expandDescrMethod( descr, context)
	descr.methodid = getInterfaceMethodIdentifierString( descr.symbol, descr.param, descr.const)
	descr.methodname = getInterfaceMethodNameString( descr.name, descr.param, descr.const)
	local sep; if descr.argstr == "" then sep = "" else sep = ", " end
	descr.llvmtype = utils.template_format( context.descr.methodCallType, {rtype = descr.rtype, argstr = sep .. descr.argstr})
end
-- Expand the structure used for mapping the LLVM template for a class method call with keys derived from the call description
function expandDescrClassCallTemplateParameter( descr, context)
	expandDescrCallTemplateParameter( descr, context)
	descr.paramstr = getDeclarationLlvmParameterString( descr.param)
	expandDescrMethod( descr, context)
end
-- Expand the structure used for mapping the LLVM template for an interface method call with keys derived from the call description
function expandDescrInterfaceCallTemplateParameter( descr, context)
	expandDescrCallTemplateParameter( descr, context)
	expandDescrMethod( descr, context)
	if not descr.const then context.const = false end
end
-- Expand the structure used for mapping the LLVM template for an free function call with keys derived from the call description
function expandDescrFreeCallTemplateParameter( descr, context)
	expandDescrCallTemplateParameter( descr, context)
	descr.paramstr = getDeclarationLlvmParameterString( descr.param)
	descr.llvmtype = utils.template_format( llvmir.control.freeFunctionCallType, {rtype = descr.rtype, argstr = descr.argstr})
end
-- Expand the structure used for mapping the LLVM template for an extern function call with keys derived from the call description
function expandDescrExternCallTemplateParameter( descr, context)
	descr.argstr = getDeclarationLlvmParameterTypeString( descr.param)
	if descr.externtype == "C" then
		descr.symbolname = descr.name
	else
		utils.errorMessage( node.line, "Unknown extern call type \"%s\" (must be one of {\"C\"})", descr.externtype)
	end
	if descr.ret then descr.rtype = typeDescriptionMap[ descr.ret].llvmtype else descr.rtype = "void" end
	if descr.vararg then
		local sep; if descr.argstr == "" then sep = "" else sep = ", " end
		descr.signature = utils.template_format( llvmir.control.freeFunctionVarargSignature,  {argstr = descr.argstr .. sep})
	end
	descr.llvmtype = utils.template_format( llvmir.control.freeFunctionCallType, {rtype = descr.rtype, argstr = descr.argstr})
end
-- Add a descriptor to the list of methods that helps to link interfaces with classes
function expandContextMethodList( node, descr, context)
	table.insert( context.methods, {llvmtype=descr.llvmtype, methodid=descr.methodid, methodname=descr.methodname, symbol=descr.symbolname})
	if not descr.private then
		if context.methodmap[ descr.methodid] then utils.errorMessage( node.line, "Duplicate definition of method '%s'", descr.methodname) end
		context.methodmap[ descr.methodid] = #context.methods
	end
end
function printFunctionDeclaration( node, descr)
	print( "\n" .. utils.constructor_format( llvmir.control.functionDeclaration, descr))
end
function printExternFunctionDeclaration( node, descr)
	local declfmt; if descr.vararg then declfmt = llvmir.control.extern_functionDeclaration_vararg else declfmt = llvmir.control.extern_functionDeclaration end
	print( "\n" .. utils.constructor_format( declfmt, descr))
end
function defineFunctionCall( thisTypeId, contextTypeId, opr, descr)
	if descr.ret and not referenceTypeMap[ descr.ret] then utils.errorMessage( node.line, "Reference type not allowed as function return value") end
	local callfmt	
	callfmt = utils.template_format( descr.call, descr)
	local functype = defineCall( rvalueTypeMap[ descr.ret] or descr.ret, contextTypeId, opr, descr.param, 
					functionCallConstructor( callfmt, thisTypeId, ", ", "callargstr", descr.ret))
	if descr.vararg then varargFuncMap[ functype] = true end
end
function defineCallableType( node, descr, thisTypeId, context)
	local callableContextTypeId = typedb:get_type( thisTypeId, descr.name)
	if not callableContextTypeId then
		callableContextTypeId = typedb:def_type( thisTypeId, descr.name)
		typeDescriptionMap[ callableContextTypeId] = llvmir.callableDescr
	end
	defineFunctionCall( thisTypeId, callableContextTypeId, "()", descr)
end
-- Define a virtual object identified by name and an operator "()" with the function arguments (unifying the semantics of objects with "()" operator and function calls)
function defineCallable( node, descr, context)
	if context and context.qualitype then
		local thisTypeId = getFunctionThisType( descr.private, descr.const, context.qualitype.rval)
		expandDescrClassCallTemplateParameter( descr, context)
		expandContextMethodList( node, descr, context)
		defineCallableType( node, descr, thisTypeId, context)
	else
		expandDescrFreeCallTemplateParameter( descr, context)
		defineCallableType( node, descr, 0, context)
	end
end
function defineOperatorType( node, descr, context)
	local contextTypeId = 0; if context and context.qualitype then contextTypeId = getFunctionThisType( descr.private, descr.const, context.qualitype.rval) end
	defineFunctionCall( contextTypeId, contextTypeId, descr.name, descr)
end
-- Define a callable operator with its arguments
function defineOperator( node, descr, context)
	expandDescrClassCallTemplateParameter( descr, context)
	expandContextMethodList( node, descr, context)
	defineOperatorType( node, descr, context)
end
-- Define an extern function as callable with "()" operator similar to 'defineCallable'
function defineExternCallable( node, descr)
	if descr.ret and rvalueTypeMap[ descr.ret] then utils.errorMessage( node.line, "Non scalar or non pointer type not allowed as extern function return value") end
	expandDescrExternCallTemplateParameter( descr, context)
	defineCallableType( node, descr, 0, nil)
end
-- Define an interface method call as callable with "()" operator similar to 'defineCallable'
function defineInterfaceCallable( node, descr, context)
	expandDescrInterfaceCallTemplateParameter( descr, context)
	expandContextMethodList( node, descr, context)
	expandContextLlvmMember( descr, context)
end
-- Define the context of the body of any function/procedure/operator except the main function
function defineCallableBodyContext( node, context, descr)
	local prev_scope = typedb:scope( node.scope)
	if context and context.qualitype then
		local privateThisReferenceType = getFunctionThisType( true, descr.const, context.qualitype.rval)
		local privateThisPointerType = getFunctionThisType( true, descr.const, context.qualitype.pval)
		defineVariableHardcoded( privateThisPointerType, "this", "%ths")
		local classvar = defineVariableHardcoded( privateThisReferenceType, "*this", "%ths")
		addContextTypeConstructorPair( {type=classvar, constructor={out="%ths"}})
	end
	defineCallableContext( node.scope, descr.ret)
	typedb:scope( prev_scope)
end
-- Define the context of the body of the main function
function defineMainProcContext( node)
	local prev_scope = typedb:scope( node.scope)
	defineVariableHardcoded( stringPointerType, "argc", "%argc")
	defineVariableHardcoded( scalarIntegerType, "argv", "%argv")
	defineCallableContext( node.scope, scalarIntegerType)
	typedb:scope( prev_scope)
end
-- Create a string constant pointer type/constructor
function getStringConstant( value)
	if not stringConstantMap[ value] then
		local encval,enclen = utils.encodeLexemLlvm(value)
		local name = utils.uniqueName( "string")
		stringConstantMap[ value] = {fmt=utils.template_format( llvmir.control.stringConstConstructor, {name=name,size=enclen+1}),name=name,size=enclen+1}
		print( utils.constructor_format( llvmir.control.stringConstDeclaration, {out="@" .. name, size=enclen+1, value=encval}) .. "\n")
	end
	local callable = getCallableContext()
	if not callable then
		return {type=stringAddressType,constructor=stringConstantMap[ value]}
	else
		out = callable.register()
		return {type=stringPointerType,constructor={code=utils.constructor_format( stringConstantMap[ value].fmt, {out=out}), out=out}}
	end
end

-- AST Callbacks:
function typesystem.definition( node, pass, context, pass_selected)
	if not pass_selected or pass == pass_selected then
		local arg = utils.traverse( typedb, node, context)
		if arg[1] then return {code = arg[1].constructor.code} else return {code=""} end
	end
end
function typesystem.paramdef( node) 
	local arg = utils.traverse( typedb, node)
	return defineParameter( node, arg[1], arg[2], getCallableContext())
end

function typesystem.paramdeflist( node)
	return utils.traverse( typedb, node)
end

function typesystem.structure( node, context)
	local arg = utils.traverse( typedb, node, context)
	return {type=constexprStructureType, constructor=arg}
end
function typesystem.allocate( node, context)
	local callable = getCallableContext()
	local args = utils.traverse( typedb, node, context)
	local typeId = args[1]
	local pointerTypeId = pointerTypeMap[typeId]
	if not pointerTypeId then utils.errorMessage( node.line, "Only non pointer type allowed in %s", "new") end
	local refTypeId = referenceTypeMap[ typeId]
	if not refTypeId then utils.errorMessage( node.line, "Only non reference type allowed in %s", "new") end
	local descr = typeDescriptionMap[ typeId]
	local memblk = callFunction( node, {0}, "allocmem", {{type=constexprUIntegerType, constructor=bcd.int(descr.size)}})
	local ww,ptr_constructor = typedb:get_reduction( memPointerType, memblk.type, tagmask_resolveType)
	if not ww then utils.errorMessage( node.line, "Function '%s' not returning a pointer %s / %s", "allocmem", 
	                                                          typedb:type_string(memblk.type), typedb:type_string(memPointerType)) end
	if ptr_constructor then memblk.constructor = ptr_constructor( memblk.constructor) end
	local out = callable.register()
	local cast = utils.constructor_format( llvmir.control.memPointerCast, {llvmtype=descr.llvmtype, out=out,this=memblk.constructor.out}, callable.register)
	local rt = applyCallable( node, {type=refTypeId,constructor={out=out, code=memblk.constructor.code .. cast}}, ":=", {args[2]})
	rt.type = pointerTypeId
	return rt
end
function typesystem.typecast( node, context)
	local callable = getCallableContext()
	local args = utils.traverse( typedb, node, context)
	local typeId = args[1]
	local operand = args[2]
	local descr = typeDescriptionMap[ typeId]
	if operand then
		return getRequiredTypeConstructor( node, typeId, operand, tagmask_typeCast)
	else
		local out = callable.register()
		local code = utils.constructor_format( descr.def_local, {out=out}, callable.register)
		return tryApplyCallable( node, {type=typeId,constructor={out=out,code=code}}, ":=", {})
	end
end
function typesystem.delete( node, context)
	local callable = getCallableContext()
	local args = utils.traverse( typedb, node, context)
	local typeId = args[1].type
	local out,code = constructorParts( args[1].constructor)
	local res = applyCallable( node, {type=typeId,constructor={code=code,out=out}}, " delete")
	local pointerType = typedb:type_context( res.type)
	local lval = typeQualiSepMap[ pointerType].lval
	local descr = typeDescriptionMap[ lval]
	local cast_out = callable.register()
	local cast = utils.constructor_format( llvmir.control.bytePointerCast, {llvmtype=descr.llvmtype, out=cast_out, this=out}, callable.register)
	local memblk = callFunction( node, {0}, "freemem", {{type=memPointerType, constructor={code=res.constructor.code .. cast,out=cast_out}}})
	return {code=memblk.constructor.code}
end
function typesystem.vardef( node, context)
	local arg = utils.traverse( typedb, node, context)
	return defineVariable( node, context, arg[1], arg[2], nil)
end
function typesystem.vardef_assign( node, context)
	local arg = utils.traverse( typedb, node, context)
	return defineVariable( node, context, arg[1], arg[2], arg[3])
end
function typesystem.vardef_array( node, context)
	local arg = utils.traverse( typedb, node, context)
	if arg[3].type ~= constexprUIntegerType then utils.errorMessage( node.line, "Size of array is not an unsigned integer const expression") end
	local arsize = arg[3].constructor:tonumber()
	local arrayTypeId = implicitDefineArrayType( node, arg[1], arsize)
	return defineVariable( node, context, arrayTypeId, arg[2], nil)
end
function typesystem.vardef_array_assign( node, context)
	local arg = utils.traverse( typedb, node, context)
	if arg[3].type ~= constexprUIntegerType then utils.errorMessage( node.line, "Size of array is not an unsigned integer const expression") end
	local arsize = arg[3].constructor:tonumber()
	local arrayTypeId = implicitDefineArrayType( node, arg[1], arsize)
	return defineVariable( node, context, arrayTypeId, arg[2], arg[4])
end
function typesystem.typedef( node, context)
	local typnam = node.arg[2].value
	local arg = utils.traverse( typedb, node)
	local declContextTypeId = getTypeDeclContextTypeId( context)
	local descr = typeDescriptionMap[ arg[1]]
	local orig_qualitype = qualiTypeMap[ arg[1]]
	local this_qualitype = defineQualifiedTypes( declContextTypeId, typnam, descr)
	local convlist = {"lval","c_lval","c_rval"}
	for ci,conv in ipairs(convlist) do
		typedb:def_reduction( this_qualitype[conv], orig_qualitype[conv], nil, tag_TypeConversion, 0.125 / 32)
		typedb:def_reduction( orig_qualitype[conv], this_qualitype[conv], nil, tag_TypeConversion, 0.125 / 64)
	end
end
function typesystem.assign_operator( node, operator)
	local arg = utils.traverse( typedb, node)
	return applyCallable( node, arg[1], "=", {applyCallable( node, arg[1], operator, {arg[2]})})
end
function typesystem.operator( node, operator)
	local args = utils.traverse( typedb, node)
	local this = args[1]
	table.remove( args, 1)
	return applyCallable( node, this, operator, args)
end

function typesystem.free_expression( node)
	local arg = utils.traverse( typedb, node)
	if arg[1].type == controlTrueType or arg[1].type == controlFalseType then
		return {code=arg[1].constructor.code .. utils.constructor_format( llvmir.control.label, {inp=arg[1].constructor.out})}
	else
		return {code=arg[1].constructor.code}
	end
end
function typesystem.codeblock( node)
	local code = ""
	local arg = utils.traverse( typedb, node)
	for ai=1,#arg do if arg[ ai] then code = code .. arg[ ai].code end end
	return {code=getFrameCodeBlock( code)}
end
function typesystem.return_value( node)
	local arg = utils.traverse( typedb, node)
	local callable = getCallableContext()
	local rtype = callable.returntype
	if rtype == 0 then utils.errorMessage( node.line, "Can't return value from procedure") end
	local constructor = getRequiredTypeConstructor( node, rtype, arg[1], tagmask_matchParameter)
	if not constructor then utils.errorMessage( node.line, "Return value does not match declared return type '%s'", typedb:type_string(rtype)) end
	local code = utils.constructor_format( llvmir.control.returnStatement, {type=typeDescriptionMap[rtype].llvmtype, this=constructor.out})
	return {code = constructor.code .. code}
end
function typesystem.conditional_if( node)
	local arg = utils.traverse( typedb, node)
	local cond_constructor = getRequiredTypeConstructor( node, controlTrueType, arg[1], tagmask_matchParameter)
	if not cond_constructor then utils.errorMessage( node.line, "Can't use type '%s' as a condition", typedb:type_string(arg[1].type)) end
	return {code = cond_constructor.code .. arg[2].code .. utils.constructor_format( llvmir.control.label, {inp=cond_constructor.out})}
end
function typesystem.conditional_if_else( node)
	local arg = utils.traverse( typedb, node)
	local cond_constructor = getRequiredTypeConstructor( node, controlTrueType, arg[1], tagmask_matchParameter)
	if not cond_constructor then utils.errorMessage( node.line, "Can't use type '%s' as a condition", typedb:type_string(arg[1].type)) end
	local callable = getCallableContext()
	local exit = callable.label()
	local elsecode = utils.constructor_format( llvmir.control.invertedControlType, {inp=cond_constructor.out, out=exit})
	return {code = cond_constructor.code .. arg[2].code .. elsecode .. arg[3].code .. utils.constructor_format( llvmir.control.label, {inp=exit})}
end
function typesystem.conditional_while( node)
	local arg = utils.traverse( typedb, node)
	local cond_constructor = getRequiredTypeConstructor( node, controlTrueType, arg[1], tagmask_matchParameter)
	if not cond_constructor then utils.errorMessage( node.line, "Can't use type '%s' as a condition", typedb:type_string(arg[1].type)) end
	local callable = getCallableContext()
	local start = callable.label()
	local startcode = utils.constructor_format( llvmir.control.label, {inp=start})
	return {code = startcode .. cond_constructor.code .. arg[2].code 
			.. utils.constructor_format( llvmir.control.invertedControlType, {out=start,inp=cond_constructor.out})}
end
function typesystem.typespec( node, qualifier)
	local typeName = qualifier .. node.arg[ #node.arg].value
	local typeId
	if #node.arg == 1 then
		typeId = selectNoConstructorNoArgumentType( node, typeName, typedb:resolve_type( getContextTypes(), typeName, tagmask_typeNameSpace))
	else
		local ctxTypeName = node.arg[ 1].value
		local ctxTypeId = selectNoConstructorNoArgumentType( node, ctxTypeName, typedb:resolve_type( getContextTypes(), node.arg[1].value, tagmask_typeNameSpace))
		if #node.arg > 2 then
			for ii = 2, #node.arg-1 do
				ctxTypeName  = node.arg[ii].value
				ctxTypeId = selectNoConstructorNoArgumentType( node, ctxTypeName, typedb:resolve_type( ctxTypeId, ctxTypeName, tagmask_typeNameSpace))
			end
		end
		typeId = selectNoConstructorNoArgumentType( node, typeName, typedb:resolve_type( contextTypeId, typeName, tagmask_typeNameSpace))
	end
	return typeId
end
function typesystem.typespec_key( node, qualifier)
	return typedb:get_type( 0, qualifier)
end
function typesystem.constant( node, typeName)
	local typeId = selectNoConstructorNoArgumentType( node, typeName, typedb:resolve_type( 0, typeName))
	return {type=typeId, constructor=createConstExpr( node, typeId, node.arg[1].value)}
end
function typesystem.null( node)
	return {type=constexprNullType, constructor="null"}
end
function typesystem.string_constant( node)
	return getStringConstant( node.arg[1].value)
end
function typesystem.char_constant( node)
	local ua = utils.utf8to32( node, node.arg[1].value)
	if #ua == 0 then return {type=constexprUIntegerType, constructor=bcd.int(0)}
	elseif #ua == 1 then return {type=constexprUIntegerType, constructor=bcd.int(ua[1])}
	else utils.errorMessage( node.line, "Single quoted string '%s' length %d not containing a single unicode character", node.arg[1].value, #ua)
	end
end
function typesystem.variable( node)
	local typeName = node.arg[ 1].value
	local callable = getCallableContext()
	local typeId,constructor = selectNoArgumentType( node, typeName, typedb:resolve_type( getContextTypes(), typeName, tagmask_resolveType))
	local variableScope = typedb:type_scope( typeId)
	if variableScope[1] == 0 or callable.scope[1] <= variableScope[1] then
		return {type=typeId, constructor=constructor}
	else
		utils.errorMessage( node.line, "Not allowed to access variable '%s' that is not defined in local function or global scope", typedb:type_string(typeId))
	end
end

function typesystem.linkage( node, llvm_linkage)
	return llvm_linkage
end
function typesystem.funcdef( node, decl, context)
	local arg = utils.traverseRange( typedb, node, {1,3}, context)
	local descr = {lnk = arg[1].linkage, attr = llvmir.functionAttribute( arg[1].private), signature="",
			name = arg[2], symbol = arg[2], ret = arg[3], private=arg[1].private, const=decl.const }
	if rvalueTypeMap[ descr.ret] then descr.call = llvmir.control.sretFunctionCall else descr.call = llvmir.control.functionCall end
	descr.param = utils.traverseRange( typedb, node, {4,4}, context, descr, 1)[4]
	defineCallable( node, descr, context)
	descr.body  = utils.traverseRange( typedb, node, {4,4}, context, descr, 2)[4]
	printFunctionDeclaration( node, descr)
end
function typesystem.procdef( node, decl, context)
	local arg = utils.traverseRange( typedb, node, {1,2}, context)
	local descr = {call = llvmir.control.procedureCall, lnk = arg[1].linkage, attr = llvmir.functionAttribute( arg[1].private), signature="",
			name = arg[2], symbol = arg[2], ret = nil, private=arg[1].private, const=decl.const }
	descr.param = utils.traverseRange( typedb, node, {3,3}, context, descr, 1)[3]
	defineCallable( node, descr, context)
	descr.body  = utils.traverseRange( typedb, node, {3,3}, context, descr, 2)[3] .. "ret void\n"
	printFunctionDeclaration( node, descr)
end
function typesystem.operatordecl( node, opr)
	return opr
end
function typesystem.operator_funcdef( node, decl, context)
	local arg = utils.traverseRange( typedb, node, {1,3}, context)
	local descr = {lnk = arg[1].linkage, attr = llvmir.functionAttribute( arg[1].private), signature="",
			name = arg[2].name, symbol = "$" .. arg[2].symbol, ret = arg[3], private=arg[1].private, const=decl.const}
	if rvalueTypeMap[ descr.ret] then descr.call = llvmir.control.sretFunctionCall else descr.call = llvmir.control.functionCall end
	descr.param = utils.traverseRange( typedb, node, {4,4}, context, descr, 1)[4]
	defineOperator( node, descr, context)
	descr.body  = utils.traverseRange( typedb, node, {4,4}, context, descr, 2)[4]
	printFunctionDeclaration( node, descr)
	defineOperatorAttributes( context, descr)
end
function typesystem.operator_procdef( node, decl, context)
	local arg = utils.traverseRange( typedb, node, {1,2}, context)
	local descr = {call = llvmir.control.procedureCall, lnk = arg[1].linkage, attr = llvmir.functionAttribute( arg[1].private), signature="",
			name = arg[2].name, symbol = "$" .. arg[2].symbol, ret = nil, private=arg[1].private, const=decl.const}
	descr.param = utils.traverseRange( typedb, node, {3,3}, context, descr, 1)[3]
	defineOperator( node, descr, context)
	descr.body  = utils.traverseRange( typedb, node, {3,3}, context, descr, 2)[3] .. "ret void\n"
	printFunctionDeclaration( node, descr)
	defineOperatorAttributes( context, descr)
end
function typesystem.constructordef( node, context)
	local arg = utils.traverseRange( typedb, node, {1,1}, context)
	local descr = {call = llvmir.control.procedureCall, lnk = arg[1].linkage, attr = llvmir.functionAttribute( arg[1].private), signature="",
			name = ":=", symbol = "$ctor", ret = nil, private=arg[1].private, const=false}
	descr.param = utils.traverseRange( typedb, node, {2,2}, context, descr, 1)[2]
	defineOperator( node, descr, context)
	descr.body  = utils.traverseRange( typedb, node, {2,2}, context, descr, 2)[2] .. "ret void\n"
	printFunctionDeclaration( node, descr)
	defineOperatorAttributes( context, descr)
end
function typesystem.destructordef( node, context)
	local arg = utils.traverseRange( typedb, node, {1,1}, context)
	local descr = {call = llvmir.control.procedureCall, lnk = arg[1].linkage, attr = llvmir.functionAttribute( arg[1].private), signature="",
	               name = ":~", symbol = "$dtor", ret = nil, private=false, const=false }
	descr.param = utils.traverseRange( typedb, node, {2,2}, context, descr, 1)[2]
	defineOperator( node, descr, context)
	descr.body  = utils.traverseRange( typedb, node, {2,2}, context, descr, 2)[2] .. "ret void\n"
	printFunctionDeclaration( node, descr)
end
function typesystem.callablebody( node, context, descr, select)
	if select == 1 then
		defineCallableBodyContext( node, context, descr)
		local arg = utils.traverseRange( typedb, node, {1,1}, context)
		return arg[1]
	else
		local arg = utils.traverseRange( typedb, node, {2,2}, context)
		return arg[2].code
	end
end
function typesystem.extern_paramdef( node, args)
	table.insert( args, utils.traverseRange( typedb, node, {1,1})[1])
	utils.traverseRange( typedb, node, {#node.arg,#node.arg}, args)
end
function typesystem.extern_paramdeflist( node)
	local args = {}
	utils.traverse( typedb, node, args)
	local rt = {}; for ai,arg in ipairs(args) do
		local llvmtype = typeDescriptionMap[ arg].llvmtype
		table.insert( rt, {type=arg,llvmtype=llvmtype} )
	end
	return rt
end
function typesystem.extern_funcdef( node)
	local arg = utils.traverse( typedb, node)
	local descr = {call = llvmir.control.functionCall, externtype = arg[1], name = arg[2], symbol = arg[2], ret = arg[3], param = arg[4], vararg=false, signature=""}
	defineExternCallable( node, descr)
	printExternFunctionDeclaration( node, descr)
end
function typesystem.extern_procdef( node)
	local arg = utils.traverse( typedb, node)
	local descr = {call = llvmir.control.procedureCall, externtype = arg[1], name = arg[2], symbol = arg[2], param = arg[3], vararg=false, signature=""}
	defineExternCallable( node, descr)
	printExternFunctionDeclaration( node, descr)
end
function typesystem.extern_funcdef_vararg( node)
	local arg = utils.traverse( typedb, node)
	local descr = {call = llvmir.control.functionCall, externtype = arg[1], name = arg[2], symbol = arg[2], ret = arg[3], param = arg[4], vararg=true, signature=""}
	defineExternCallable( node, descr)
	printExternFunctionDeclaration( node, descr)
end
function typesystem.extern_procdef_vararg( node)
	local arg = utils.traverse( typedb, node)
	local descr = {call = llvmir.control.procedureCall, externtype = arg[1], name = arg[2], symbol = arg[2], param = arg[3], vararg=true, signature=""}
	defineExternCallable( node, descr)
	printExternFunctionDeclaration( node, descr)
end
function typesystem.interface_funcdef( node, decl, context)
	local arg = utils.traverse( typedb, node, context)
	local descr = {name = arg[1], symbol = arg[1], ret = arg[2], param = arg[3], signature="", const=decl.const, index=#context.methods}
	if rvalueTypeMap[ descr.ret] then descr.call = llvmir.control.sretInterfaceFunctionCall else descr.call = llvmir.control.interfaceFunctionCall end
	defineInterfaceCallable( node, descr, context)
end
function typesystem.interface_procdef( node, decl, context)
	local arg = utils.traverse( typedb, node, context)
	local descr = {call = llvmir.control.interfaceProcedureCall, name = arg[1], symbol = arg[1], ret = nil, param = arg[2], signature="", const=decl.const}
	defineInterfaceCallable( node, descr, context)
end
function typesystem.interface_operator_funcdef( node, decl, context)
	local arg = utils.traverse( typedb, node, context)
	local descr = {name = arg[1].name, symbol = "$" .. arg[1].symbol, ret = arg[2], param = arg[3], signature="", const=decl.const}
	if rvalueTypeMap[ descr.ret] then descr.call = llvmir.control.sretInterfaceFunctionCall else descr.call = llvmir.control.interfaceFunctionCall end
	defineInterfaceCallable( node, descr, context)
end
function typesystem.interface_operator_procdef( node, decl, context)
	local arg = utils.traverse( typedb, node, context)
	local descr = {call = llvmir.control.interfaceProcedureCall, name = arg[1].name, symbol = "$" .. arg[1].symbol, ret = nil, param = arg[2],
	               signature = "", const = decl.const}
	defineInterfaceCallable( node, descr, context)
end
function typesystem.structdef( node, context)
	local typnam = node.arg[1].value
	local declContextTypeId = getTypeDeclContextTypeId( context)
	local structname = getTypeDeclUniqueName( declContextTypeId, typnam)
	local descr = utils.template_format( llvmir.structTemplate, {structname=structname})
	local qualitype = defineQualifiedTypes( declContextTypeId, typnam, descr)
	defineRValueReferenceTypes( declContextTypeId, typnam, descr, qualitype)
	addContextTypeConstructorPair( qualitype.lval)
	local context = {qualitype=qualitype, descr=descr, index=0, private=false, members={}, structsize=0, llvmtype="", symbol=structname}
	local arg = utils.traverse( typedb, node, context)
	descr.size = context.structsize
	definePointerOperators( qualitype.priv_c_pval, qualitype.c_pval, typeDescriptionMap[ qualitype.c_pval])
	defineStructConstructors( node, qualitype, descr, context)
	print_section( "Typedefs", utils.template_format( descr.typedef, {llvmtype=context.llvmtype}))
end
function typesystem.interfacedef( node, context)
	local typnam = node.arg[1].value
	local declContextTypeId = getTypeDeclContextTypeId( context)
	local interfacename = getTypeDeclUniqueName( declContextTypeId, typnam)
	local descr = utils.template_format( llvmir.interfaceTemplate, {interfacename=interfacename})
	local qualitype = defineQualifiedTypes( declContextTypeId, typnam, descr)
	defineRValueReferenceTypes( declContextTypeId, typnam, descr, qualitype)
	local context = {qualitype=qualitype, descr=descr, methods={}, methodmap={}, llvmtype="", symbol=interfacename, const=true}
	local arg = utils.traverse( typedb, node, context)
	descr.methods = context.methods
	descr.const = context.const
	definePointerOperators( qualitype.priv_c_pval, qualitype.c_pval, typeDescriptionMap[ qualitype.c_pval])
	defineInterfaceConstructors( node, qualitype, descr, context)
	print_section( "Typedefs", utils.template_format( descr.vmtdef, {llvmtype=context.llvmtype}))
	print_section( "Typedefs", descr.typedef)
end
function typesystem.classdef( node, context)
	local typnam = node.arg[1].value
	local declContextTypeId = getTypeDeclContextTypeId( context)
	local classname = getTypeDeclUniqueName( declContextTypeId, typnam)
	local descr = utils.template_format( llvmir.classTemplate, {classname=classname})
	local qualitype = defineQualifiedTypes( declContextTypeId, typnam, descr)
	defineRValueReferenceTypes( declContextTypeId, typnam, descr, qualitype)
	definePublicPrivate( declContextTypeId, typnam, descr, qualitype)
	addContextTypeConstructorPair( qualitype.lval)
	local context = {qualitype=qualitype, descr=descr, index=0, private=true, members={}, operators={}, methods={}, methodmap={},
	                 structsize=0, llvmtype="", symbol=classname, interfaces={}}
	-- Traversal in two passes, first type and variable declarations, then method definitions
	utils.traverse( typedb, node, context, 1)
	utils.traverse( typedb, node, context, 2)
	descr.size = context.structsize
	definePointerOperators( qualitype.priv_c_pval, qualitype.c_pval, typeDescriptionMap[ qualitype.c_pval])
	defineClassConstructors( node, qualitype, descr, context)
	defineOperatorsWithStructArgument( node, context)
	defineInheritedInterfaces( node, context, qualitype.lval)
	print_section( "Typedefs", utils.template_format( descr.typedef, {llvmtype=context.llvmtype}))
end
function typesystem.inheritdef( node, pass, context, pass_selected)
	if not pass_selected or pass == pass_selected then
		local arg = utils.traverseRange( typedb, node, {1,1}, context)
		local typeId = arg[1]
		local descr = typeDescriptionMap[ typeId]
		local typnam = typedb:type_name(typeId)
		local private = false
		if descr.class == "class" then
			defineVariableMember( node, context, typeId, typnam, private)
			defineClassInheritanceReductions( context, typnam, private, false)
		elseif descr.class == "interface" then
			defineInterfaceMember( node, context, typeId, typnam, private)
			defineInterfaceInheritanceReductions( context, typnam, private, descr.const)
		else
			utils.errorMessage( node.line, "Inheritance only allowed from class or interface, not from '%s'", descr.class)
		end
		if #node.arg > 1 then utils.traverseRange( typedb, node, {2,2}, context, pass_selected) end
	end
end
function typesystem.main_procdef( node)
	defineMainProcContext( node)
	local arg = utils.traverse( typedb, node)
	local descr = {body = arg[1].code}
	print( "\n" .. utils.constructor_format( llvmir.control.mainDeclaration, descr))
end
function typesystem.program( node)
	initBuiltInTypes()
	utils.traverse( typedb, node, {})
	return node
end
function typesystem.count( node)
	if #node.arg == 0 then
		return 1
	else
		return utils.traverseCall( node)[ 1] + 1
	end
end
function typesystem.rep_operator( node)
	local arg = utils.traverse( typedb, node)
	local icount = arg[2]
	local expr = arg[1]
	for ii=1,icount do
		expr = applyCallable( node, expr, "->")
	end
	return applyCallable( node, expr, arg[3])
end
function typesystem.member( node)
	local arg = utils.traverse( typedb, node)
	return applyCallable( node, arg[1], arg[2])
end

return typesystem
