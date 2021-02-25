local mewa = require "mewa"
local llvmir = require "language1/llvmir"
local utils = require "typesystem_utils"
local bcd = require "bcd"

local typedb = mewa.typedb()
local typesystem = {}

local tag_typeAlias = 1			-- Type that describes a substitution (as used for generic parameters)
local tag_typeDeduction = 2		-- Inheritance relation
local tag_typeDeclaration = 3		-- Type declaration relation (e.g. variable to data type)
local tag_typeConversion = 4		-- Type conversion of parameters
local tag_namespace = 5			-- Type deduction for resolving namespaces
local tag_pointerReinterpretCast = 6	-- Type deduction only allowed in an explicit "cast" operation
local tag_pushVararg = 7		-- Type deduction for passing parameter as vararg argument
local tag_transfer = 8			-- Transfer of information to build an object by a constructor, used in free function callable to pointer assignment
local tagmask_declaration = typedb.reduction_tagmask( tag_typeAlias, tag_typeDeclaration)
local tagmask_resolveType = typedb.reduction_tagmask( tag_typeAlias, tag_typeDeduction, tag_typeDeclaration)
local tagmask_matchParameter = typedb.reduction_tagmask( tag_typeAlias, tag_typeDeduction, tag_typeDeclaration, tag_typeConversion, tag_transfer)
local tagmask_typeConversion = typedb.reduction_tagmask( tag_typeConversion)
local tagmask_namespace = typedb.reduction_tagmask( tag_typeAlias, tag_namespace)
local tagmask_typeCast = typedb.reduction_tagmask( tag_typeAlias, tag_typeDeduction, tag_typeDeclaration, tag_typeConversion, tag_pointerReinterpretCast)
local tagmask_typeAlias = typedb.reduction_tagmask( tag_typeAlias)
local tagmask_pushVararg = typedb.reduction_tagmask( tag_typeAlias, tag_typeDeduction, tag_typeDeclaration, tag_typeConversion, tag_pushVararg)

-- Centralized list of reduction preference rules:
local rdw_conv = 1.0			-- Reduction weight of conversion
local rdw_strip_rvref = 0.75		-- Reduction weight of instantiating rvalue reference 
local rdw_strip_v_1 = 0.5 / (1)		-- Reduction weight of stripping private/public from type with 1 qualifier 
local rdw_strip_v_2 = 0.5 / (2*2)	-- Reduction weight of stripping private/public from type with 2 qualifiers
local rdw_strip_v_3 = 0.5 / (3*3)	-- Reduction weight of stripping private/public from type with 3 qualifiers
local rdw_swap_r_p = 0.375 / (1)	-- Reduction weight of swapping reference with pointer on a type with 1 qualifier
local rdw_swap_r_p = 0.375 / (2*2)	-- Reduction weight of swapping reference with pointer on a type with 2 qualifiers
local rdw_swap_r_p = 0.375 / (3*3)	-- Reduction weight of swapping reference with pointer on a type with 3 qualifiers
local rdw_strip_r_1 = 0.25 / (1)	-- Reduction weight of stripping reference (fetch value) from type with 1 qualifier 
local rdw_strip_r_2 = 0.25 / (2*2)	-- Reduction weight of stripping reference (fetch value) from type with 2 qualifiers
local rdw_strip_r_3 = 0.25 / (3*3)	-- Reduction weight of stripping reference (fetch value) from type with 3 qualifiers
local rdw_strip_c_1 = 0.125 / (1)	-- Reduction weight of changing constant qualifier in type with 1 qualifier
local rdw_strip_c_2 = 0.125 / (2*2)	-- Reduction weight of changing constant qualifier in type with 2 qualifiers
local rdw_strip_c_3 = 0.125 / (3*3)	-- Reduction weight of changing constant qualifier in type with 3 qualifiers
local rwd_inheritance = 0.125 / 16	-- Reduction weight of class inheritance
local rwd_boolean = 0.125 / 16		-- Reduction weight of boolean type (control/value type) conversions
function combineWeight( w1, w2) return w1 + w2 * 0.9921875 end -- Combining two reductions slightly less weight compared with applying both singularily
function scalarDeductionWeight( sizeweight) return 0.25*(1.0-sizeweight) end -- Deduction weight of this element for scalar operators

local weightEpsilon = 1E-10		-- epsilon used for comparing weights for equality
local scalarQualiTypeMap = {}		-- maps scalar type names without qualifiers to the table of type ids for all qualifiers possible
local scalarIndexTypeMap = {}		-- maps scalar type names usable as index without qualifiers to the const type id used as index for [] or pointer arithmetics
local scalarBooleanType = nil		-- type id of the boolean type, result of cmpop binary operators
local scalarIntegerType = nil		-- type id of the main integer type, result of main function
local scalarLongType = nil		-- type id of the main long integer type (64 bits), used for long integer vararg parameters
local scalarFloatType = nil		-- type id of the main floating point number type (64 bits), used for floating point vararg parameters
local stringPointerType = nil		-- type id of the string constant type used for free string litterals
local memPointerType = nil		-- type id of the type used for result of allocmem and argument of freemem
local stringAddressType = nil		-- type id of the string constant type used string constants outsize a function scope
local anyClassPointerType = nil		-- type id of the "class^" type
local anyConstClassPointerType = nil 	-- type id of the "class^" type
local anyStructPointerType = nil	-- type id of the "struct^" type
local anyConstStructPointerType = nil	-- type id of the "struct^" type
local anyFreeFunctionType = nil		-- type id of any free function/procedure callable
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
local instantCallableEnvironment = nil	-- callable environment created for implicitly generated code (constructors,destructors,assignments,etc.)
local mainCallableEnvironment = nil	-- callable environment for constructors/destructors of globals
local hardcodedTypeMap = {}		-- maps some hardcoded type names to their id

-- Create the data structure with attributes attached to a context (referenced in body) of some function/procedure or a callable in general terms
function createCallableEnvironment( scope, rtype, rprefix, lprefix)
	return {scope=scope, register=utils.register_allocator(rprefix), label=utils.label_allocator(lprefix), returntype=rtype}
end
-- Attach a newly created data structure for a callable to its scope
function defineCallableEnvironment( scope, rtype)
	typedb:set_instance( "callable", createCallableEnvironment( scope, rtype))
end
-- Get the active callable instance
function getCallableEnvironment()
	return instantCallableEnvironment or typedb:get_instance( "callable")
end
-- Get the two parts of a constructor as tuple
function constructorParts( constructor)
	if type(constructor) == "table" then return constructor.out,(constructor.code or ""),(constructor.alloc or "") else return constructor,"","" end
end
function constructorStruct( out, code)
	return {out=out, code=code or ""}
end
function constructorStructEmpty()
	return {code=""}
end
function typeConstructorStruct( type, out, code)
	return {type=type, constructor=constructorStruct( out, code)}
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
		local callable = getCallableEnvironment()
		local code = ""
		local this_inp,this_code = constructorParts( this)
		local arg_inp,arg_code = constructorParts( arg[1])
		local subst = {arg1 = tostring(arg_inp), this = this_inp}
		code = code .. this_code .. arg_code
 		return constructorStruct( this_inp, code .. utils.constructor_format( fmt, subst, callable.register))
	end
end
-- Constructor implementing an assignment of a free function callable to a function/procedure pointer
function assignFunctionPointerConstructor( descr)
	local assign = assignConstructor( descr.assign)
	return function( this, arg)
		local scope_bk = typedb:scope( typedb:type_scope( arg[1].type))
		local functype = typedb:get_type( arg[1].type, "()", descr.param)
		typedb:scope( scope_bk)
		if functype then return assign( this, {{out="@" .. typeDescriptionMap[ functype].symbolname}}) end
	end
end
-- Binary operator with a conversion of the argument. Needed for conversion of floating point numbers into a hexadecimal code required by LLVM IR. 
function binopArgConversionConstructor( fmt, argconv)
	return function( this, arg)
		local callable = getCallableEnvironment()
		local out = callable.register()
		local code = ""
		local this_inp,this_code = constructorParts( this)
		local arg_inp,arg_code = constructorParts( arg[1])
		local subst = {out = out, this = this_inp, arg1 = argconv(arg_inp)}
		code = code .. this_code .. arg_code
		return constructorStruct( out, code .. utils.constructor_format( fmt, subst, callable.register))
	end
end
-- Constructor imlementing a conversion of a data item to another type using a format string that describes the conversion operation
-- param[in] fmt format string, param[in] outRValueRef output variable in case of an rvalue reference constructor
function convConstructor( fmt, outRValueRef)
	return function( constructor)
		local callable = getCallableEnvironment()
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
		local callable = getCallableEnvironment()
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
		local callable = getCallableEnvironment()
		local out = callable.register()
		local inp,code = constructorParts( constructor)
		code = utils.constructor_format( descr.def_local, {out = out}, callable.register) .. utils.template_format( code, {[inp] = out} )
		return {code = code, out = out}
	end
end
-- Function that decides wheter a function (in the LLVM code output) should return the return value as value or via an 'sret' pointer passed as argument
function doReturnValueAsReferenceParameter( typeId)
	if typeId and rvalueTypeMap[ typeId] then return true else return false end
end
-- Builds the argument string and the argument build-up code for a function call or interface method call constructor
function buildFunctionCallArguments( code, argstr, args, llvmtypes)
	local rt = argstr; if argstr ~= "" then rt = rt .. ", " end
	for ii=1,#args do
		local arg = args[ ii]
		local llvmtype = llvmtypes[ ii]
		local arg_inp,arg_code = constructorParts( arg)
		code = code .. arg_code
		if llvmtype then rt = rt .. llvmtype .. " " .. tostring(arg_inp) .. ", " else rt = rt .. "i32 0, " end
	end
	if #args > 0 or argstr ~= "" then rt = rt:sub(1, -3) end
	return code,rt
end
-- Builds the table with the variables to substitute in a function call template
function buildFunctionCallSubst( callable, rtype, argstr)
	if not rtype then
		return nil,{callargstr = argstr}
	elseif doReturnValueAsReferenceParameter( rtype) then
		local out = "RVAL"
		local rvalsubst; if argstr == "" then rvalsubst = "{RVAL}" else rvalsubst = "{RVAL}, " end
		return out,{callargstr = argstr, rvalref = rvalsubst}
	else
		local out = callable.register()
		subst = {out = out, callargstr = argstr}
		return out,subst
	end
end
-- Constructor implementing a call of a function with an arbitrary number of arguments built as one string with LLVM typeinfo attributes as needed for function calls
function functionCallConstructor( fmt, thisTypeId, rtype)
	return function( this, args, llvmtypes)
		local callable = getCallableEnvironment()
		local this_inp,this_code = constructorParts( this)
		local this_argstr; if thisTypeId ~= 0 then this_argstr = typeDescriptionMap[ thisTypeId].llvmtype .. " " .. this_inp else this_argstr = "" end
		local code,argstr = buildFunctionCallArguments( this_code, this_argstr, args, llvmtypes)
		local out,subst = buildFunctionCallSubst( callable, rtype, argstr)
		return constructorStruct( out, code .. utils.constructor_format( fmt, subst, callable.register))
	end
end
-- Constructor implementing a call of a method of an interface
function interfaceMethodCallConstructor( loadfmt, callfmt, rtype)
	return function( this, args, llvmtypes)
		local callable = getCallableEnvironment()
		local this_inp,this_code = constructorParts( this)
		local intr_func = callable.register()
		local intr_this = callable.register()
		this_code = this_code .. utils.constructor_format( loadfmt, {this=this_inp, out_func=intr_func, out_this=intr_this}, callable.register)
		local this_argstr = "i8* " .. intr_this
		local code,argstr = buildFunctionCallArguments( this_code, this_argstr, args, llvmtypes)
		local out,subst = buildFunctionCallSubst( callable, rtype, argstr)
		subst.func = intr_func
		return constructorStruct( out, code .. utils.constructor_format( callfmt, subst, callable.register))
	end
end
-- Constructor for a memberwise assignment of a tree structure (initializing a "struct")
function memberwiseInitStructConstructor( node, thisTypeId, members)
	return function( this, args)
		args = args[1] -- args contains one list node
		if #args ~= #members then 
			utils.errorMessage( node.line, "Number of elements %d in init do not match for '%s' [%d]", #args, typedb:type_string(thisTypeId), #members)
		end
		local qualitype = qualiTypeMap[ thisTypeId]
		local descr = typeDescriptionMap[ thisTypeId]
		local callable = getCallableEnvironment()
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
-- Constructor for a memberwise assignment of a tree structure (initializing an "array")
function memberwiseInitArrayConstructor( node, thisTypeId, elementTypeId, nofElements)
	return function( this, args)
		args = args[1] -- args contains one list node
		if #args > nofElements then
			utils.errorMessage( node.line, "Number of elements %d in init is too big for '%s' [%d]", #args, typedb:type_string( thisTypeId), nofElements)
		end
		local qualitype = qualiTypeMap[ thisTypeId]
		local descr = typeDescriptionMap[ thisTypeId]
		local descr_element = typeDescriptionMap[ elementTypeId]
		local callable = getCallableEnvironment()
		local this_inp,this_code = constructorParts( this)
		local rt = constructorStruct( this_inp, this_code)
		for ai=1,#args do
			local out = callable.register()
			local code = utils.constructor_format( descr.loadelemref, {out = out, this = this_inp, index=ai-1}, callable.register)
			local reftype = referenceTypeMap[ elementTypeId] or elementTypeId
			local maparg = applyCallable( node, typeConstructorStruct( reftype, out, code), ":=", {args[ai]})
			rt.code = rt.code .. maparg.constructor.code
		end
		if #args < nofElements then
			local fmtdescr = {this=this_inp, element=descr_element.llvmtype,
			                  enter=callable.label(), begin=callable.label(), ["end"]=callable.label(), index=#args}
			rt.code = rt.code .. utils.constructor_format( descr.ctor_rest, fmtdescr, callable.register)
		end
		return rt
	end
end
-- Map an argument to a requested parameter type, creating a local variable if not possible otherwise
function tryCreateParameter( node, callable, typeId, arg)
	local redulist,weight = tryGetWeightedParameterReductionList( node, typeId, arg)
	if redulist then
		local constructor = applyReductionList( node, redulist, arg.constructor)
		if not constructor then return nil end
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
-- Constructor for an application of an operator with a tree structure as argument (initializing a "class" or a applying a multiargument operator)
function resolveOperatorConstructor( node, thisTypeId, opr)
	return function( this, args)
		local rt = {}
		if #args == 1 and args[1].type == constexprStructureType then args = args[1] end
		local callable = getCallableEnvironment()
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
					table.insert( rt, applyCallable( node, typeConstructorStruct( thisTypeId, this_inp, this_code), opr, param_ar))
				end
			end
		end
		if #rt == 0 then
			return nil
		elseif #rt > 1 then
			utils.errorMessage( node.line, "Ambiguus reference for constructor of '%s'", typedb:type_string( thisTypeId))
		else
			return rt[1].constructor
		end
	end
end
-- Constructor for a promote call (implementing int + float by promoting the first argument int to float and do a float + float to get the result)
function promoteCallConstructor( call_constructor, promote_constructor)
	return function( this, arg) return call_constructor( promote_constructor( this), arg) end
end
-- Define an operation with involving the promotion of the left hand argument to another type and executing the operation as defined for the type promoted to.
function definePromoteCall( returnType, thisType, promoteType, opr, argTypes, promote_constructor)
	local call_constructor = typedb:type_constructor( typedb:get_type( promoteType, opr, argTypes))
	local callType = typedb:def_type( thisType, opr, promoteCallConstructor( call_constructor, promote_constructor), argTypes)
	if returnType then typedb:def_reduction( returnType, callType, nil, tag_typeDeclaration) end
end
-- Define an operation
function defineCall( returnType, thisType, opr, argTypes, constructor)
	local callType = typedb:def_type( thisType, opr, constructor, argTypes)
	if callType == -1 then utils.errorMessage( 0, "Duplicate definition of type '%s %s'", typedb:type_string(thisType), opr) end
	if returnType then typedb:def_reduction( returnType, callType, nil, tag_typeDeclaration) end
	return callType
end

-- Conversion of constexpr constant to representation in LLVM IR
function constexprLlvmConversion( typeid)
	if typeDescriptionMap[ typeid].class == "fp" then
		if typeDescriptionMap[ typeid].llvmtype == "float" then
			return function( val) return "0x" .. mewa.llvm_float_tohex( val) end
		elseif typeDescriptionMap[ typeid].llvmtype == "double" then
			return function( val) return "0x" .. mewa.llvm_double_tohex( val) end
		end
	end
	return function( val) return tostring( val) end
end
-- Constant expression/value types
local constexprIntegerType = typedb:def_type( 0, "constexpr int")	-- const expression integers implemented as arbitrary precision BCD numbers
local constexprUIntegerType = typedb:def_type( 0, "constexpr uint")	-- const expression unsigned integer implemented as arbitrary precision BCD numbers
local constexprFloatType = typedb:def_type( 0, "constexpr float")	-- const expression floating point numbers implemented as 'double'
local constexprBooleanType = typedb:def_type( 0, "constexpr bool")	-- const expression boolean implemented as Lua boolean
local constexprNullType = typedb:def_type( 0, "constexpr null")		-- const expression null value implemented as nil
local constexprStructureType = typedb:def_type( 0, "constexpr struct")	-- const expression tree structure implemented as a list of type/constructor pairs
-- Mapping of some constant expression type naming
hardcodedTypeMap[ "constexpr bool"] = constexprBooleanType
hardcodedTypeMap[ "constexpr uint"] = constexprUIntegerType
hardcodedTypeMap[ "constexpr int"] = constexprIntegerType
hardcodedTypeMap[ "constexpr float"] = constexprFloatType
-- Description records of constsexpr types
typeDescriptionMap[ constexprIntegerType] = llvmir.constexprIntegerDescr
typeDescriptionMap[ constexprUIntegerType] = llvmir.constexprUIntegerDescr
typeDescriptionMap[ constexprFloatType] = llvmir.constexprFloatDescr
typeDescriptionMap[ constexprBooleanType] = llvmir.constexprBooleanDescr
typeDescriptionMap[ constexprNullType] = llvmir.constexprNullDescr
typeDescriptionMap[ constexprStructureType] = llvmir.constexprStructDescr
-- Table listing the accepted constant expression/value types for each scalar type class 
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
-- Get the value of a const expression or the value of an alias of a const expression
function getConstexprValue( constexpr_type, type, value)
	if type == constexpr_type then
		return tostring(value)
	else
		local weight,redu_constructor = typedb:get_reduction( constexpr_type, type, tagmask_typeAlias)
		if weight then
			if not redu_constructor then
				return typedb:type_constructor( type)
			elseif type(redu_constructor) == "function" then 
				return redu_constructor( typedb:type_constructor( type))
			else 
				return redu_constructor 
			end
		end
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
	typedb:def_reduction( constexprBooleanType, constexprIntegerType, function( value) return value ~= "0" end, tag_typeConversion, rdw_conv)
	typedb:def_reduction( constexprBooleanType, constexprFloatType, function( value) return math.abs(value) < math.abs(epsilon) end, tag_typeConversion, rdw_conv)
	typedb:def_reduction( constexprFloatType, constexprIntegerType, function( value) return value:tonumber() end, tag_typeConversion, rdw_conv)
	typedb:def_reduction( constexprIntegerType, constexprFloatType, function( value) return bcd.int( tostring(value)) end, tag_typeConversion, rdw_conv)
	typedb:def_reduction( constexprIntegerType, constexprUIntegerType, function( value) return value end, tag_typeConversion, rdw_conv)
	typedb:def_reduction( constexprUIntegerType, constexprIntegerType, function( value) return value end, tag_typeConversion, rdw_conv)

	local function bool2bcd( value) if value then return bcd.int("1") else return bcd.int("0") end end
	typedb:def_reduction( constexprIntegerType, constexprBooleanType, bool2bcd, tag_typeConversion, rdw_conv)
end
function isPointerType( typeId)
	return not pointerTypeMap[ typeId]
end
-- Get the declaration type of a variable or data member of a structure or class
function getDeclarationType( typeId)
	local redulist = typedb:get_reductions( typeId, tagmask_declaration)
	for ri,redu in ipairs( redulist) do return redu.type end
end
-- Return the raw type of a vararg argument to use for parameter passing
function stripVarargType( typeId)
	local qualitype = qualiTypeMap[ typeQualiSepMap[ typeId].lval]
	if isPointerType(typeId) then return qualitype.c_pval else return qualitype.c_lval end
end
-- Map a type used as variable argument parameter (where no explicit parameter type defined for the callee) to its type used for parameter passing
function getVarargArgumentType( typeId)
	local descr = typeDescriptionMap[ typeId]
	if not descr then
		typeId = getDeclarationType( typeId)
		if typeId then descr = typeDescriptionMap[ stripVarargType( typeId)] end
	end
	if descr then
		if descr.class == "constexpr" then
			return typeId
		elseif descr.class == "pointer" then
			return memPointerType
		elseif descr.class == "signed" or descr.class == "unsigned" or descr.class == "bool" then
			if descr.llvmtype == "i64" then return scalarLongType else return scalarIntegerType end
		elseif descr.class == "fp" then
			return scalarFloatType
		end
	end
end
-- Define all variations of a type alias that can explicitly  specified
function defineTypeAlias( contextTypeId, typnam, aliasTypeId)
	local qualitype = qualiTypeMap[ aliasTypeId]
	typedb:def_type_as( contextTypeId, typnam, qualitype.lval)			-- L-value | plain typedef
	typedb:def_type_as( contextTypeId, "const " .. typnam, qualitype.c_lval)	-- const L-value
	typedb:def_type_as( contextTypeId, "&" .. typnam, qualitype.rval)		-- reference
	typedb:def_type_as( contextTypeId, "const&" .. typnam, qualitype.c_rval)	-- const reference
	typedb:def_type_as( contextTypeId, "^" .. typnam, qualitype.pval)		-- pointer
	typedb:def_type_as( contextTypeId, "const^" .. typnam, qualitype.c_pval)	-- const pointer
	typedb:def_type_as( contextTypeId, "^&" .. typnam, qualitype.rpval)		-- pointer reference
	typedb:def_type_as( contextTypeId, "const^&" .. typnam, qualitype.c_rpval)	-- const pointer reference
end
-- Define all basic types associated with a type name
function defineQualifiedTypes( node, contextTypeId, typnam, typeDescription)
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

	if lval == -1 then utils.errorMessage( node.line, "Duplicate definition of type '%s %s'", typedb:type_string(contextTypeId), typnam) end

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

	typedb:def_reduction( c_pval, constexprNullType, constConstructor("null"), tag_typeConversion, rdw_conv)
 	typedb:def_reduction( lval, c_rval, convConstructor( typeDescription.load), tag_typeDeduction, combineWeight( rdw_strip_r_2,rdw_strip_c_1))
	typedb:def_reduction( pval, rpval, convConstructor( pointerTypeDescription.load), tag_typeDeduction, rdw_strip_r_2)
	typedb:def_reduction( c_pval, c_rpval, convConstructor( pointerTypeDescription.load), tag_typeDeduction, rdw_strip_r_3)

	typedb:def_reduction( c_lval, lval, nil, tag_typeDeduction, rdw_strip_c_1)
	typedb:def_reduction( c_rval, rval, nil, tag_typeDeduction, rdw_strip_c_1)
	typedb:def_reduction( c_pval, pval, nil, tag_typeDeduction, rdw_strip_c_1)
	typedb:def_reduction( c_rpval, rpval, nil, tag_typeDeduction, rdw_strip_c_2)

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

	typeDescriptionMap[ rval_ref]   = pointerTypeDescription
	typeDescriptionMap[ c_rval_ref] = pointerTypeDescription

	constTypeMap[ rval_ref] = c_rval_ref

	typeQualiSepMap[ rval_ref]   = {lval=lval,qualifier="&&"}
	typeQualiSepMap[ c_rval_ref] = {lval=lval,qualifier="const&&"}

	rvalueTypeMap[ qualitype.lval] = rval_ref
	rvalueTypeMap[ qualitype.c_lval] = c_rval_ref

	typedb:def_reduction( c_rval_ref, rval_ref, nil, tag_typeDeduction, rdw_strip_c_3)
	typedb:def_reduction( qualitype.c_rval, c_rval_ref, constReferenceFromRvalueReferenceConstructor( typeDescription), tag_typeDeduction, rdw_strip_rvref)
	typedb:def_reduction( qualitype.c_rval, rval_ref, constReferenceFromRvalueReferenceConstructor( typeDescription), tag_typeDeduction, rdw_strip_rvref)

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

	typedb:def_reduction( qualitype.rval, priv_rval, nil, tag_typeDeduction, rdw_strip_v_2)
	typedb:def_reduction( qualitype.c_rval, priv_c_rval, nil, tag_typeDeduction, rdw_strip_v_3)
	typedb:def_reduction( qualitype.pval, priv_pval, nil, tag_typeDeduction, rdw_strip_v_2)
	typedb:def_reduction( qualitype.c_pval, priv_c_pval, nil, tag_typeDeduction, rdw_strip_v_3)

	defineCall( priv_pval, priv_rval, "&", {}, function(this) return this end)
	defineCall( priv_rval, priv_pval, "->", {}, function(this) return this end)
	defineCall( priv_c_rval, priv_c_pval, "->", {}, function(this) return this end)
end
-- Define constructurs and assignment operations for pointer types and some type reductions bound to pointers only
function definePointerOperations( qualitype, descr)
	local pointer_descr = llvmir.pointerDescr( descr)
	defineCall( qualitype.rpval, qualitype.rpval, "=",  {qualitype.c_pval}, assignConstructor( pointer_descr.assign))
	defineCall( qualitype.rpval, qualitype.rpval, ":=", {qualitype.c_pval}, assignConstructor( pointer_descr.assign))
	defineCall( qualitype.rpval, qualitype.rpval, ":=", {qualitype.c_rpval}, assignConstructor( pointer_descr.ctor_copy))
	defineCall( qualitype.rpval, qualitype.rpval, ":=", {}, manipConstructor( pointer_descr.ctor))
	defineCall( qualitype.c_rpval, qualitype.c_rpval, ":=", {qualitype.c_pval}, assignConstructor( pointer_descr.assign))
	defineCall( qualitype.c_rpval, qualitype.c_rpval, ":=", {qualitype.c_rpval}, assignConstructor( pointer_descr.ctor_copy))
	defineCall( qualitype.c_rpval, qualitype.c_rpval, ":=", {}, manipConstructor( pointer_descr.ctor))
	defineCall( 0, qualitype.rpval, ":~", {}, constructorStructEmpty)
	local voidptr_cast_fmt = utils.template_format( llvmir.control.bytePointerCast, {llvmtype=pointer_descr.llvmtype})
	typedb:def_reduction( memPointerType, qualitype.c_pval, convConstructor(voidptr_cast_fmt), tag_pushVararg)
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
	defineCall( qualitype.c_rval, qualitype.c_rval, ":=", {qualitype.c_lval}, assignConstructor( descr.assign))
	defineCall( qualitype.c_rval, qualitype.c_rval, ":=", {qualitype.c_rval}, assignConstructor( descr.ctor_copy))
	defineCall( qualitype.c_rval, qualitype.c_rval, ":=", {}, manipConstructor( descr.ctor))
	definePointerOperations( qualitype, descr)
	defineScalarDestructors( qualitype, descr)
end
-- Define a conversion reduction as implicit const reference object allocation and reduction to it as initialization with the specified ":=" constructor
function defineConstructionReduction( destType, sourceType, allocfmt, initconstructor)
	function memberwiseInitConvConstructor( constructor)
		local callable = getCallableEnvironment()
		out = callable.register()
		local alloc = utils.constructor_format( allocfmt, {out=out}, callable.register)
		local this = {code = alloc, out = out}
		local args = {constructor}
		return initconstructor( this, args)
	end
	typedb:def_reduction( destType, sourceType, memberwiseInitConvConstructor, tag_typeConversion, rdw_conv)
end
-- Define constructors for implicitly defined array types (when declaring a variable int a[30], then a type int[30] is implicitly declared) 
function defineArrayConstructors( node, qualitype, descr, elementTypeId, arsize)
	definePointerOperations( qualitype, descr)

	instantCallableEnvironment = createCallableEnvironment( node.scope, nil)
	local r_elementTypeId = referenceTypeMap[ elementTypeId]
	local c_elementTypeId = constTypeMap[ elementTypeId]
	if not r_elementTypeId then utils.errorMessage( node.line, "References not allowed in array declarations, use pointer instead") end
	local cr_elementTypeId = constTypeMap[ r_elementTypeId] or r_elementTypeId
	local ths = {type=r_elementTypeId, constructor={out="%ths"}}
	local oth = {type=cr_elementTypeId, constructor={out="%oth"}}
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
		defineCall( qualitype.c_rval, qualitype.c_rval, ":=", {qualitype.c_rval}, assignConstructor( utils.template_format( descr.ctor_copy, {procname="copy"})))
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
	if init and initcopy then
		local initconstructor = memberwiseInitArrayConstructor( node, qualitype.lval, elementTypeId, arsize)
		defineCall( qualitype.rval, qualitype.rval, ":=", {constexprStructureType}, initconstructor)
		defineCall( qualitype.c_rval, qualitype.c_rval, ":=", {constexprStructureType}, initconstructor)
		defineConstructionReduction( qualitype.c_rval, constexprStructureType, descr.def_local, initconstructor)
	end
	instantCallableEnvironment = nil
end
-- Define constructors for 'struct' types
function defineStructConstructors( node, qualitype, descr, context)
	definePointerOperations( qualitype, descr)

	instantCallableEnvironment = createCallableEnvironment( node.scope, nil)
	local ctors,dtors,ctors_copy,ctors_assign,ctors_elements,paramstr,argstr,elements = "","","","","","","",{}

	for mi,member in ipairs(context.members) do
		table.insert( elements, member.type)
		paramstr = paramstr .. ", " .. typeDescriptionMap[ member.type].llvmtype .. " %p" .. mi
		argstr = argstr .. ", " .. typeDescriptionMap[ member.type].llvmtype .. " {arg" .. mi .. "}"

		local out = instantCallableEnvironment.register()
		local inp = instantCallableEnvironment.register()
		local llvmtype = member.descr.llvmtype
		local c_member_type = constTypeMap[ member.type] or member.type
		local member_reftype = referenceTypeMap[ member.type]
		local c_member_reftype = constTypeMap[ member_reftype] or member_reftype
		local loadref = context.descr.loadelemref
		local ths = {type=member_reftype,constructor={code=utils.constructor_format(loadref,{out=out,this="%ths",index=mi-1, type=llvmtype}),out=out}}
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
		if member_destroy and dtors then dtors = member_destroy.constructor.code .. dtors else dtors = nil end
	end
	if ctors then
		print_section( "Auto", utils.template_format( descr.ctorproc, {ctors=ctors}))
		defineCall( qualitype.rval, qualitype.rval, ":=", {}, manipConstructor( descr.ctor))
		defineCall( qualitype.c_rval, qualitype.c_rval, ":=", {}, manipConstructor( descr.ctor))
	end
	if ctors_copy then
		print_section( "Auto", utils.template_format( descr.ctorproc_copy, {procname="copy",ctors=ctors_copy}))
		defineCall( qualitype.rval, qualitype.rval, ":=", {qualitype.rval}, assignConstructor( utils.template_format( descr.ctor_copy, {procname="copy"})))
		defineCall( qualitype.c_rval, qualitype.c_rval, ":=", {qualitype.rval}, assignConstructor( utils.template_format( descr.ctor_copy, {procname="copy"})))
	end
	if ctors_assign then
		print_section( "Auto", utils.template_format( descr.ctorproc_copy, {procname="assign",ctors=ctors_assign}))
		defineCall( qualitype.rval, qualitype.rval, "=", {qualitype.rval}, assignConstructor( utils.template_format( descr.ctor_copy, {procname="assign"})))
	end
	if ctors_elements then
		print_section( "Auto", utils.template_format( descr.ctorproc_elements, {procname="assign",ctors=ctors_elements,paramstr=paramstr}))
		defineCall( qualitype.rval, qualitype.rval, ":=", elements, callConstructor( utils.template_format( descr.ctor_elements, {args=argstr})))
		defineCall( qualitype.c_rval, qualitype.c_rval, ":=", elements, callConstructor( utils.template_format( descr.ctor_elements, {args=argstr})))
	end
	if dtors then
		print_section( "Auto", utils.template_format( descr.dtorproc, {dtors=dtors}))
		defineCall( 0, qualitype.rval, ":~", {}, manipConstructor( descr.dtor))
		defineCall( 0, qualitype.pval, " delete", {}, manipConstructor( descr.dtor))
	end
	local initconstructor = memberwiseInitStructConstructor( node, qualitype.lval, context.members)
	defineCall( qualitype.rval, qualitype.rval, ":=", {constexprStructureType}, initconstructor)
	defineCall( qualitype.c_rval, qualitype.c_rval, ":=", {constexprStructureType}, initconstructor)
	defineConstructionReduction( qualitype.c_rval, constexprStructureType, descr.def_local, initconstructor)
	typedb:def_reduction( anyStructPointerType, qualitype.pval, nil, tag_pointerReinterpretCast)
	typedb:def_reduction( anyConstStructPointerType, qualitype.c_pval, nil, tag_pointerReinterpretCast)
	typedb:def_reduction( qualitype.pval, anyStructPointerType, nil, tag_pointerReinterpretCast)
	typedb:def_reduction( qualitype.c_pval, anyConstStructPointerType, nil, tag_pointerReinterpretCast)
	instantCallableEnvironment = nil
end
-- Define constructors for 'interface' types
function defineInterfaceConstructors( node, qualitype, descr, context)
	definePointerOperations( qualitype, descr)
	defineCall( qualitype.c_rval, qualitype.c_rval, ":=", {}, manipConstructor( descr.ctor))
	defineCall( qualitype.rval, qualitype.rval, ":=", {}, manipConstructor( descr.ctor))
	defineCall( qualitype.c_rval, qualitype.c_rval, ":=", {qualitype.rval}, assignConstructor( descr.ctor_copy))
	defineCall( qualitype.rval, qualitype.rval, ":=", {qualitype.rval}, assignConstructor( descr.ctor_copy))
	defineCall( qualitype.c_rval, qualitype.c_rval, ":=", {qualitype.c_lval}, assignConstructor( descr.assign))
	defineCall( qualitype.rval, qualitype.rval, ":=", {qualitype.c_lval}, assignConstructor( descr.assign))
	defineCall( qualitype.rval, qualitype.rval, "=", {qualitype.rval}, assignConstructor( descr.ctor_copy))
end
-- Define constructors for 'class' types
function defineClassConstructors( node, qualitype, descr, context)
	definePointerOperations( qualitype, descr)

	instantCallableEnvironment = createCallableEnvironment( node.scope, nil)
	local dtors = ""
	for mi,member in ipairs(context.members) do
		local out = instantCallableEnvironment.register()
		local llvmtype = member.descr.llvmtype
		local member_reftype = referenceTypeMap[ member.type]
		local loadref = context.descr.loadelemref
		local ths = {type=member_reftype,constructor={code=utils.constructor_format(loadref,{out=out,this="%ths",index=mi-1, type=llvmtype}),out=out}}

		local member_destroy = tryApplyCallable( node, ths, ":~", {} )

		if member_destroy and dtors then dtors = member_destroy.constructor.code .. dtors else dtors = nil end
	end
	if dtors and not context.properties.destructor then
		print_section( "Auto", utils.template_format( descr.dtorproc, {dtors=dtors}))
		defineCall( 0, qualitype.rval, ":~", {}, manipConstructor( descr.dtor))
		defineCall( 0, qualitype.pval, " delete", {}, manipConstructor( descr.dtor))
	end
	typedb:def_reduction( anyClassPointerType, qualitype.pval, nil, tag_pointerReinterpretCast)
	typedb:def_reduction( anyConstClassPointerType, qualitype.c_pval, nil, tag_pointerReinterpretCast)
	typedb:def_reduction( qualitype.pval, anyClassPointerType, nil, tag_pointerReinterpretCast)
	typedb:def_reduction( qualitype.c_pval, anyConstClassPointerType, nil, tag_pointerReinterpretCast)
	instantCallableEnvironment = nil
end
-- Tell if a method identifier by id implements an inherited interface method, thus has to be noinline
function isInterfaceMethod( context, methodid)
	for ii,iTypeId in ipairs(context.interfaces) do
		local idescr = typeDescriptionMap[ iTypeId]
		if idescr.methodmap[ methodid] then return true end
	end
	return false
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
					{classname=cdescr.symbol, interfacename=idescr.symbol, llvmtype=vmtstr}))
	end
end
-- Get the type assigned to the variable 'this' or implicitly added to the context of a method in its body
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
function defineArrayIndexOperators( resTypeId, arTypeId, descr)
	for index_typnam, index_type in pairs(scalarIndexTypeMap) do
		defineCall( resTypeId, arTypeId, "[]", {index_type}, callConstructor( descr.index[ index_typnam]))
	end
end
-- Define pointer comparison operators if defined in the type description
function definePointerCompareOperators( qualitype, pointer_descr)
	for operator,operator_fmt in pairs( pointer_descr.cmpop) do
		defineCall( scalarBooleanType, qualitype.c_pval, operator, {qualitype.c_pval}, callConstructor( operator_fmt))
		if qualitype.priv_c_pval then defineCall( scalarBooleanType, qualitype.priv_c_pval, operator, {qualitype.c_pval}, callConstructor( operator_fmt)) end
	end
end
-- Define built-in type conversions for first class citizen scalar types
function defineBuiltInTypeConversions( typnam, descr)
	local qualitype = scalarQualiTypeMap[ typnam]
	if descr.conv then
		for oth_typnam,conv in pairs( descr.conv) do
			local oth_qualitype = scalarQualiTypeMap[ oth_typnam]
			local conv_constructor; if conv.fmt then conv_constructor = convConstructor( conv.fmt) else conv_constructor = nil end
			typedb:def_reduction( qualitype.lval, oth_qualitype.c_lval, conv_constructor, tag_typeConversion, rdw_conv)
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
	local valueconv = constexprLlvmConversion( qualitype.lval)
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
				defineCall( qualitype.lval, qualitype.c_lval, operator, {constexprType}, binopArgConversionConstructor( operator_fmt, valueconv))
			end
		end
	end
	if descr.cmpop then
		for operator,operator_fmt in pairs( descr.cmpop) do
			defineCall( scalarBooleanType, qualitype.c_lval, operator, {qualitype.c_lval}, callConstructor( operator_fmt))
			for i,constexprType in ipairs(constexprTypes) do
				defineCall( scalarBooleanType, qualitype.c_lval, operator, {constexprType}, binopArgConversionConstructor( operator_fmt, valueconv))
			end
		end
	end
end
-- Get the list of context types associated with the current scope used for resolving types
function getContextTypes()
	local rt = typedb:get_instance( "defcontext")
	if rt then return rt else return {0} end
end
-- Extend the list of context types associated with the current scope used for resolving types by one type/constructor pair structure
function pushDefContextTypeConstructorPair( val)
	local defcontext = typedb:this_instance( "defcontext")
	if defcontext then
		table.insert( defcontext, val)
	else
		defcontext = typedb:get_instance( "defcontext")
		if defcontext then
			-- inherit context from enclosing scope and add new element
			local defcontext_copy = {}
			for di,ctx in ipairs(defcontext) do
				table.insert( defcontext_copy, ctx)
			end
			table.insert( defcontext_copy, val)
			typedb:set_instance( "defcontext", defcontext_copy)
		else
			-- create empty context and add new element
			defcontext = {0,val}
			typedb:set_instance( "defcontext", defcontext)
		end
	end
end
-- Remove the last element of the the list of context types associated with the current scope used for resolving types by one type/constructor pair structure
function popDefContextTypeConstructorPair( val)
	local defcontext = typedb:this_instance( "defcontext")
	if not defcontext or defcontext[ #defcontext] ~= val then utils.errorMessage( {line=0}, "Internal: corrupt definition context stack") end
	table.remove( defcontext, #defcontext)
end
-- This function checks if an array type defined by element type and size already exists and creates it implicitly if not, return the array type handle 
function implicitDefineArrayType( node, elementTypeId, arsize)
	local descr = llvmir.arrayDescr( typeDescriptionMap[ elementTypeId], arsize)
	local typnam = "[" .. arsize .. "]"
	local typkey = elementTypeId .. typnam
	local arrayTypeId = arrayTypeMap[ typkey]
	if arrayTypeId then
		return arrayTypeId
	else
		local scope_bk = typedb:scope( typedb:type_scope( elementTypeId))
		local qualitype = defineQualifiedTypes( node, elementTypeId, typnam, descr)
		local qualitype_element = qualiTypeMap[ typeQualiSepMap[ elementTypeId].lval]
		arrayTypeId = qualitype.lval
		defineRValueReferenceTypes( elementTypeId, typnam, descr, qualitype)
		defineArrayConstructors( node, qualitype, descr, elementTypeId, arsize)
		arrayTypeMap[ typkey] = arrayTypeId
		definePointerCompareOperators( qualitype, typeDescriptionMap[ qualitype.c_pval])
		defineArrayIndexOperators( qualitype_element.rval, qualitype.rval, descr)
		defineArrayIndexOperators( qualitype_element.c_rval, qualitype.c_rval, descr)
		typedb:scope( scope_bk)
		return arrayTypeId
	end
end
-- The frame object defines constructors/destructors called implicitly at start/end of their scope
function getAllocationScopeFrame()
	local rt = typedb:this_instance( "frame")
	if not rt then
		local callable = getCallableEnvironment()
		rt = {entry=callable.label(), register=callable.register, label=callable.label, ctor="", dtor="", entry=nil}
		typedb:set_instance( "frame", rt)
	end
	return rt
end
function setInitCode( descr, code)
	local frame = getAllocationScopeFrame()
	frame.ctor = frame.ctor .. code
end
function setCleanupCode( descr, code)
	if code ~= "" then
		local frame = getAllocationScopeFrame()
		frame.entry = frame.label()
		frame.dtor = utils.constructor_format( llvmir.control.label, {inp=frame.entry}) .. code .. frame.dtor
	end
end
function getAllocationScopeCodeBlock( code)
	local frame = typedb:this_instance( "frame")
	if frame then return frame.ctor .. code .. frame.dtor else return code end
end
-- Hardcoded variable definition (variable not declared in source, but implicitly declared, for example the 'this' pointer in a method body context)
function defineVariableHardcoded( typeId, name, reg)
	local var = typedb:def_type( 0, name, reg)
	typedb:def_reduction( typeId, var, nil, tag_typeDeclaration)
	return var
end
-- Define a free variable or a member variable (depending on the context)
function defineVariable( node, context, typeId, name, initVal)
	local descr = typeDescriptionMap[ typeId]
	local refTypeId = referenceTypeMap[ typeId]
	if not refTypeId then utils.errorMessage( node.line, "References not allowed in variable declarations, use pointer instead") end
	if not context then
		local callable = getCallableEnvironment()
		local out = callable.register()
		local code = utils.constructor_format( descr.def_local, {out = out}, callable.register)
		local var = typedb:def_type( 0, name, out)
		typedb:def_reduction( refTypeId, var, nil, tag_typeDeclaration)
		local decl = {type=var, constructor={code=code,out=out}}
		if initVal then rt = applyCallable( node, decl, ":=", {initVal}) else rt = applyCallable( node, decl, ":=", {}) end
		local cleanup = tryApplyCallable( node, {type=var,constructor={out=out}}, ":~", {})
		if cleanup and cleanup.constructor then setCleanupCode( descr, cleanup.constructor.code) end
		return rt
	elseif not context.qualitype then
		instantCallableEnvironment = mainCallableEnvironment
		out = "@" .. name
		local var = typedb:def_type( 0, name, out)
		typedb:def_reduction( refTypeId, var, nil, tag_typeDeclaration)

		if descr.scalar == true and initVal and type(initVal) ~= "table" then
			valueconv = constexprLlvmConversion( typeId)
			print( utils.constructor_format( descr.def_global_val, {out = out, val = valueconv(initVal)})) -- print global data declaration
		else
			print( utils.constructor_format( descr.def_global, {out = out})) -- print global data declaration
			local decl = {type=var, constructor={out=out}}
			local init; if initVal then init = applyCallable( node, decl, ":=", {initVal}) else init = applyCallable( node, decl, ":=", {}) end
			local cleanup = tryApplyCallable( node, decl, ":~", {})
			if init.constructor then setInitCode( descr, init.constructor.code) end
			if cleanup and cleanup.constructor then setCleanupCode( descr, cleanup.constructor.code) end
		end
		instantCallableEnvironment = nil
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
	local fmt = utils.template_format( descr.getClassInterface, {classname=context.descr.symbol})
	local thisType = getFunctionThisType( descr.private, descr.const, context.qualitype.rval)
	defineCall( rvalueTypeMap[ typeId], thisType, typnam, {}, convConstructor( fmt, "RVAL"))
end
-- Define a reduction to a member variable to implement class/interface inheritance
function defineReductionToMember( objTypeId, name)
	memberTypeId = typedb:get_type( objTypeId, name)
	typedb:def_reduction( memberTypeId, objTypeId, typedb:type_constructor( memberTypeId), tag_typeDeclaration, rwd_inheritance)
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
		local callable = getCallableEnvironment()
		local out = callable.register()
		return {code = constructor.code .. utils.constructor_format(llvmir.control.falseExitToBoolean,{falseExit=constructor.out,out=out},callable.label),out=out}
	end
	local function trueExitToBoolean( constructor)
		local callable = getCallableEnvironment()
		local out = callable.register()
		return {code = constructor.code .. utils.constructor_format( llvmir.control.trueExitToBoolean,{trueExit=constructor.out,out=out},callable.label),out=out}
	end
	typedb:def_reduction( scalarBooleanType, controlTrueType, falseExitToBoolean, tag_typeDeduction, rwd_boolean)
	typedb:def_reduction( scalarBooleanType, controlFalseType, trueExitToBoolean, tag_typeDeduction, rwd_boolean)

	local function booleanToFalseExit( constructor)
		local callable = getCallableEnvironment()
		local out = callable.label()
		return {code = constructor.code .. utils.constructor_format( llvmir.control.booleanToFalseExit, {inp=constructor.out, out=out}, callable.label),out=out}
	end
	local function booleanToTrueExit( constructor)
		local callable = getCallableEnvironment()
		local out = callable.label()
		return {code = constructor.code .. utils.constructor_format( llvmir.control.booleanToTrueExit, {inp=constructor.out, out=out}, callable.label),out=out}
	end

	typedb:def_reduction( controlTrueType, scalarBooleanType, booleanToFalseExit, tag_typeDeduction, rwd_boolean)
	typedb:def_reduction( controlFalseType, scalarBooleanType, booleanToTrueExit, tag_typeDeduction, rwd_boolean)

	local function negateControlTrueType( this) return {type=controlFalseType, constructor=this.constructor} end
	local function negateControlFalseType( this) return {type=controlTrueType, constructor=this.constructor} end

	local function joinControlTrueTypeWithBool( this, arg)
		local out = this.out
		local code2 = utils.constructor_format( llvmir.control.booleanToFalseExit, {inp=arg[1].out, out=out}, getCallableEnvironment().label())
		return {code=this.code .. arg[1].code .. code2, out=out}
	end
	local function joinControlFalseTypeWithBool( this, arg)
		local out = this.out
		local code2 = utils.constructor_format( llvmir.control.booleanToTrueExit, {inp=arg[1].out, out=out}, getCallableEnvironment().label())
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
			local callable = getCallableEnvironment()
			return {code= this.code .. utils.constructor_format( llvmir.control.terminateTrueExit,{out=this.out},callable.label), out=this.out}
		end
	end
	local function joinControlTrueTypeWithConstexprBool( this, arg)
		if arg == true then
			return this
		else 
			local callable = getCallableEnvironment()
			return {code= this.code .. utils.constructor_format( llvmir.control.terminateFalseExit,{out=this.out},callable.label), out=this.out}
		end
	end
	defineCall( controlTrueType, controlTrueType, "&&", {constexprBooleanType}, joinControlTrueTypeWithConstexprBool)
	defineCall( controlFalseType, controlFalseType, "||", {constexprBooleanType}, joinControlFalseTypeWithConstexprBool)

	local function constexprBooleanToControlTrueType( value)
		local callable = getCallableEnvironment()
		local out = label()
		local code; if value == true then code="" else code=utils.constructor_format( llvmir.control.terminateFalseExit, {out=out}, callable.label) end
		return {code=code, out=out}
	end
	local function constexprBooleanToControlFalseType( value)
		local callable = getCallableEnvironment()
		local out = label()
		local code; if value == false then code="" else code=utils.constructor_format( llvmir.control.terminateFalseExit, {out=out}, callable.label) end
		return {code=code, out=out}
	end
	typedb:def_reduction( controlFalseType, constexprBooleanType, constexprBooleanToControlFalseType, tag_typeDeduction, rwd_boolean)
	typedb:def_reduction( controlTrueType, constexprBooleanType, constexprBooleanToControlTrueType, tag_typeDeduction, rwd_boolean)

	local function invertControlBooleanType( this)
		local out = getCallableEnvironment().label()
		return {code = this.code .. utils.constructor_format( llvmir.control.invertedControlType, {inp=this.out, out=out}), out = out}
	end
	typedb:def_reduction( controlFalseType, controlTrueType, invertControlBooleanType, tag_typeDeduction, rwd_boolean)
	typedb:def_reduction( controlTrueType, controlFalseType, invertControlBooleanType, tag_typeDeduction, rwd_boolean)

	definePromoteCall( controlTrueType, constexprBooleanType, controlTrueType, "&&", {scalarBooleanType}, constexprBooleanToControlTrueType)
	definePromoteCall( controlFalseType, constexprBooleanType, controlFalseType, "||", {scalarBooleanType}, constexprBooleanToControlFalseType)
end
-- Initialize all built-in types
function initBuiltInTypes()
	stringAddressType = typedb:def_type( 0, " stringAddressType")

	for typnam, scalar_descr in pairs( llvmir.scalar) do
		local qualitype = defineQualifiedTypes( {line=0}, 0, typnam, scalar_descr)
		local c_lval = qualitype.c_lval
		scalarQualiTypeMap[ typnam] = qualitype
		if scalar_descr.class == "fp" then
			if scalar_descr.llvmtype == "double" then scalarFloatType = c_lval end -- LLVM needs a double to be passed as vararg argument
		elseif scalar_descr.class == "bool" then
			scalarBooleanType = c_lval
		elseif scalar_descr.class == "unsigned" then
			if scalar_descr.llvmtype == "i32" and not scalarIntegerType then scalarIntegerType = qualitype.c_lval end
			if scalar_descr.llvmtype == "i64" and not scalarLongType then scalarLongType = qualitype.c_lval end
			if scalar_descr.llvmtype == "i8" then
				stringPointerType = qualitype.c_pval
				memPointerType = qualitype.pval
			end
			scalarIndexTypeMap[ typnam] = c_lval
		elseif scalar_descr.class == "signed" then
			if scalar_descr.llvmtype == "i32" then scalarIntegerType = qualitype.c_lval end
			if scalar_descr.llvmtype == "i64" then scalarLongType = qualitype.c_lval end
					local scalarFloatType = nil

			if scalar_descr.llvmtype == "i8" and stringPointerType == 0 then
				stringPointerType = qualitype.c_pval
				memPointerType = qualitype.pval
			end
			scalarIndexTypeMap[ typnam] = c_lval
		end
		for i,constexprType in ipairs( typeClassToConstExprTypesMap[ scalar_descr.class]) do
			local weight = scalarDeductionWeight( scalar_descr.sizeweight)
			local valueconv = constexprLlvmConversion( qualitype.c_lval)
			typedb:def_reduction( qualitype.lval, constexprType, function(arg) return constructorStruct( valueconv(arg)) end, tag_typeDeduction, weight)
		end
	end
	if not scalarBooleanType then utils.errorMessage( 0, "No boolean type defined in built-in scalar types") end
	if not scalarIntegerType then utils.errorMessage( 0, "No integer type mapping to i32 defined in built-in scalar types (return value of main)") end
	if not stringPointerType then utils.errorMessage( 0, "No i8 type defined suitable to be used for free string constants)") end

	local qualitype_anyclass = defineQualifiedTypes( {line=0}, 0, "any class", llvmir.anyClassDescr)
	anyClassPointerType = qualitype_anyclass.pval; hardcodedTypeMap[ "any class^"] = anyClassPointerType
	anyConstClassPointerType = qualitype_anyclass.c_pval; hardcodedTypeMap[ "any const class^"] = anyConstClassPointerType
	local qualitype_anystruct = defineQualifiedTypes( {line=0}, 0, "any struct", llvmir.anyStructDescr)
	anyStructPointerType = qualitype_anystruct.pval; hardcodedTypeMap[ "any struct^"] = anyStructPointerType
	anyConstStructPointerType = qualitype_anystruct.c_pval; hardcodedTypeMap[ "any const struct^"] = anyConstStructPointerType

	anyFreeFunctionType = typedb:def_type( 0, "any function"); typeDescriptionMap[ anyFreeFunctionType] = llvmir.anyFunctionDescr

	for typnam, scalar_descr in pairs( llvmir.scalar) do
		local scalar_pointerdescr = llvmir.pointerDescr( scalar_descr)
		local qualitype = scalarQualiTypeMap[ typnam]
		defineScalarConstructors( qualitype, scalar_descr)
		defineArrayIndexOperators( qualitype.rval, qualitype.pval, scalar_pointerdescr)
		defineArrayIndexOperators( qualitype.c_rval, qualitype.c_pval, scalar_pointerdescr)
		defineBuiltInTypeConversions( typnam, scalar_descr)
		defineBuiltInTypeOperators( typnam, scalar_descr)
		definePointerCompareOperators( qualitype, scalar_pointerdescr)
	end
	for typnam, scalar_descr in pairs( llvmir.scalar) do
		defineBuiltInTypePromoteCalls( typnam, scalar_descr)
	end
	defineConstExprArithmetics()
	initControlBooleanTypes()
end
-- Application of a conversion constructor depending on its type and its argument type
function applyConstructor( node, typeId, constructor, arg)	
	if constructor then
		if (type(constructor) == "function") then
			local rt = constructor( arg)
			if not rt then utils.errorMessage( node.line, "reduction constructor failed for '%s'", typedb:type_string(typeId)) end
			return rt
		elseif arg then
			utils.errorMessage( node.line, "reduction constructor overwriting previous constructor for '%s'", typedb:type_string(typeId))
		else
			return constructor
		end
	else
		return arg
	end
end
-- Create a apply a list of reductions on a constructor
function applyReductionList( node, redulist, redu_constructor)
	for _,redu in ipairs(redulist) do redu_constructor = applyConstructor( node, redu.type, redu.constructor, redu_constructor) end
	return redu_constructor
end
-- Get the handle of a type expected to have no arguments (plain typedef type or a variable name)
function selectNoArgumentType( node, typeName, resolveContextTypeId, reductions, items)
	if not resolveContextTypeId or type(resolveContextTypeId) == "table" then -- not found or ambiguous
		utils.errorResolveType( typedb, node.line, resolveContextTypeId, getContextTypes(), typeName)
	end
	for ii,item in ipairs(items) do
		if typedb:type_nof_parameters( item) == 0 then
			constructor = applyReductionList( node, reductions, nil)
			local item_constructor = typedb:type_constructor( item)
			constructor = applyConstructor( node, item, item_constructor, constructor)
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
function resolveTypeFromNamePath( node, qualifier, arg)
	local typeName = qualifier .. arg[ #arg]
	local rt
	if #arg == 1 then
		rt = selectNoConstructorNoArgumentType( node, typeName, typedb:resolve_type( getContextTypes(), typeName, tagmask_namespace))
	else
		local ctxTypeName = arg[ 1]
		local ctxTypeId = selectNoConstructorNoArgumentType( node, ctxTypeName, typedb:resolve_type( getContextTypes(), ctxTypeName, tagmask_namespace))
		if #arg > 2 then
			for ii = 2, #arg-1 do
				ctxTypeName  = arg[ii]
				ctxTypeId = selectNoConstructorNoArgumentType( node, ctxTypeName, typedb:resolve_type( ctxTypeId, ctxTypeName, tagmask_namespace))
			end
		end
		rt = selectNoConstructorNoArgumentType( node, typeName, typedb:resolve_type( contextTypeId, typeName, tagmask_namespace))
	end
	return rt
end
-- Get the list of generic arguments filled with the default parameters for the ones not specified explicitly
function matchGenericParameter( node, genericType, param, args)
	local rt = {}
	if #param < #args then utils.errorMessage( node.line, "Too many arguments (%d) passed to instantiate generic '%s'", #args, typedb:type_string(genericType)) end
	for pi=1,#param do
		local arg = args[pi] or param[pi] -- argument or default parameter
		if not arg then utils.errorMessage( node.line, "Missing parameter '%s' of generic '%s'", param[ pi].name, typedb:type_string(genericType)) end
		table.insert( rt, arg)
	end
	return rt
end
-- For each generic argument, create an alias named as the parameter name as substitute for the generic argument specified
function defineGenericParameterAliases( instanceType, generic_param, generic_arg)
	for ii=1,#generic_arg do
		if generic_arg[ ii].value then
			local alias = typedb:def_type( instanceType, generic_param[ ii].name, generic_arg[ ii].value)
			typedb:def_reduction( generic_arg[ ii].type, alias, nil, tag_typeAlias)
		else
			defineTypeAlias( instanceType, generic_param[ ii].name, generic_arg[ ii].type)
		end
	end
end
-- Call a function, meaning to apply operator "()" on a callable
function callFunction( node, contextTypes, name, args)
	local typeId,constructor = selectNoArgumentType( node, name, typedb:resolve_type( contextTypes, name, tagmask_resolveType))
	local this = {type=typeId, constructor=constructor}
	return applyCallable( node, this, "()", args)
end
-- Try to get the constructor and weight of a parameter passed with the deduction tagmask optionally passed as an argument
function tryGetWeightedParameterReductionList( node, redutype, operand, tagmask)
	if redutype ~= operand.type then
		local redulist,weight,altpath = typedb:derive_type( redutype, operand.type, tagmask or tagmask_matchParameter, tagmask_typeConversion, 1)
		if altpath then
			utils.errorMessage( node.line, "Ambiguous derivation paths for '%s': %s | %s",
						typedb:type_string(operand.type), utils.typeListString(typedb,altpath," =>"), utils.typeListString(typedb,redulist," =>"))
		end
		return redulist,weight
	else
		return {},0.0
	end
end
-- Get the constructor of an implicitly required type with the deduction tagmask optionally passed as an argument
function getRequiredTypeConstructor( node, redutype, operand, tagmask)
	if redutype ~= operand.type then
		local redulist,weight,altpath = typedb:derive_type( redutype, operand.type, tagmask or tagmask_matchParameter, tagmask_typeConversion, 1)
		if not redulist then
			utils.errorMessage( node.line, "Type mismatch, required type '%s'", typedb:type_string(redutype))
		elseif altpath then
			utils.errorMessage( node.line, "Ambiguous derivation paths for '%s': %s | %s",
						typedb:type_string(operand.type), utils.typeListString(typedb,altpath," =>"), utils.typeListString(typedb,redulist," =>"))
		end
		local rt = applyReductionList( node, redulist, operand.constructor)
		if not rt then utils.errorMessage( node.line, "Construction of '%s' <- '%s' failed", typedb:type_string(redutype), typedb:type_string(operand.type)) end
		return rt
	else
		return operand.constructor
	end
end
-- For a callable item, create for each argument the lists of reductions needed to pass the arguments to it, with summation of the reduction weights
function collectItemParameter( node, item, args, parameters)
	local rt = {redulist={},llvmtypes={},weight=0.0}
	for pi=1,#args do
		local redutype,redulist,weight
		if pi <= #parameters then
			redutype = parameters[ pi].type
			redulist,weight = tryGetWeightedParameterReductionList( node, redutype, args[ pi])
		else
			redutype = getVarargArgumentType( args[ pi].type)
			if redutype then redulist,weight = tryGetWeightedParameterReductionList( node, redutype, args[ pi], tagmask_pushVararg) end
		end
		if not weight then return nil end
		if rt.weight < weight then rt.weight = weight end -- use max(a,b) as weight accumulation function
		table.insert( rt.redulist, redulist)
		local descr = typeDescriptionMap[ redutype]
		table.insert( rt.llvmtypes, descr.llvmtype)
	end
	return rt
end
-- Get the best matching item from a list of items by weighting the matching of the arguments to the item parameter types
function selectItemsMatchParameters( node, items, args, this_constructor)
	function selectCandidateItemsBestWeight( items, item_parameter_map, maxweight)
		local candidates,bestweight = {},nil
		for ii,item in ipairs(items) do
			if item_parameter_map[ ii] then -- we have a match
				local weight = item_parameter_map[ ii].weight
				if not maxweight or maxweight > weight + weightEpsilon then -- we have a candidate not looked at yet
					if not bestweight then
						candidates = {ii}
						bestweight = weight
					elseif weight < bestweight + weightEpsilon then
						if weight >= bestweight - weightEpsilon then -- they are equal
							table.insert( candidates, ii)
						else -- the new candidate is the single best match
							candidates = {ii}
							bestweight = weight
						end
					end
				end
			end
		end
		return candidates,bestweight
	end
	local item_parameter_map = {}
	local bestmatch = {}
	local candidates = {}
	local bestweight = nil
	local bestgroup = 0
	for ii,item in ipairs(items) do
		local nof_params = typedb:type_nof_parameters( item)
		if nof_params == #args or (nof_params < #args and varargFuncMap[ item]) then
			item_parameter_map[ ii] = collectItemParameter( node, item, args, typedb:type_parameters( item))
		end
	end
	while next(item_parameter_map) ~= nil do -- until no candidate groups left
		candidates,bestweight = selectCandidateItemsBestWeight( items, item_parameter_map, bestweight)
		for _,ii in ipairs(candidates) do -- create the items from the item constructors passing
			local item_constructor = typedb:type_constructor( items[ ii])
			local call_constructor
			if not item_constructor and #args == 0 then
				call_constructor = this_constructor
			else
				local arg_constructors = {}
				for ai=1,#args do
					local ac = applyReductionList( node, item_parameter_map[ ii].redulist[ ai], args[ ai].constructor)
					if not ac then utils.errorMessage( node.line, "Construction of arguments of '%s' failed", typedb:type_string(items[ii])) end
					table.insert( arg_constructors, ac)
				end
				call_constructor = item_constructor( this_constructor, arg_constructors, item_parameter_map[ ii].llvmtypes)
			end
			if call_constructor then table.insert( bestmatch, {type=items[ ii], constructor=call_constructor}) end
			item_parameter_map[ ii] = nil
		end
		if #bestmatch ~= 0 then return bestmatch,bestweight end
	end
end
-- Find a callable identified by name and its arguments (parameter matching) in the context of the 'this' operand
function findApplyCallable( node, this, callable, args)
	local resolveContextType,reductions,items = typedb:resolve_type( this.type, callable, tagmask_resolveType)
	if not resolveContextType then return nil end
	if type(resolveContextType) == "table" then utils.errorResolveType( typedb, node.line, resolveContextType, this.type, callable) end
	local this_constructor = applyReductionList( node, reductions, this.constructor)
	local bestmatch,bestweight = selectItemsMatchParameters( node, items, args or {}, this_constructor)
	return bestmatch,bestweight
end
-- Filter best result and report error on ambiguity
function getCallableBestMatch( node, bestmatch, bestweight, this, callable, args)
	if #bestmatch == 1 then
		return bestmatch[1]
	elseif #bestmatch == 0 then
		return nil
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
	local rt = getCallableBestMatch( node, bestmatch, bestweight, this, callable, args)
	if not rt then 
		utils.errorMessage( node.line, "Failed to match parameters to callable with signature '%s'",
	                    utils.resolveTypeString( typedb, this.type, callable) .. "(" .. utils.typeListString( typedb, args, ", ") .. ")")
	end
	return rt
end
-- Try to retrieve a callable in a specified context, apply it and return its type/constructor pair if found, return nil if not
function tryApplyCallable( node, this, callable, args)
	local bestmatch,bestweight = findApplyCallable( node, this, callable, args)
	if bestweight then return getCallableBestMatch( node, bestmatch, bestweight) end
end
-- Get the symbol name for a function in the LLVM output
function getTargetFunctionIdentifierString( name, args, const, context)
	if not context then
		return utils.uniqueName( name .. "__")
	else
		local pstr; if context.symbol then pstr = "__C_" .. context.symbol .. "__" .. name else pstr = name end
		for ai,arg in ipairs(args) do pstr = pstr .. "__" .. arg.llvmtype end
		if const == true then pstr = pstr .. "__const" end
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
-- Get the parameter string of a function declaration
function getDeclarationLlvmTypeRegParameterString( descr, context)
	local rt = ""
	if doReturnValueAsReferenceParameter( descr.ret) then rt = descr.rtllvmtype .. "* sret %rt, " end
	if context and context.qualitype then rt = rt .. typeDescriptionMap[ context.qualitype.pval].llvmtype .. " %ths, " end
	for ai,arg in ipairs(descr.param) do rt = rt .. arg.llvmtype .. " " .. arg.reg .. ", " end
	if rt ~= "" then rt = rt:sub(1, -3) end
	return rt
end
-- Get the parameter string of a function typedef
function getDeclarationLlvmTypedefParameterString( descr, context)
	local rt = ""
	if doReturnValueAsReferenceParameter( descr.ret) then rt = descr.rtllvmtype .. "* sret, " end
	if context and context.qualitype then rt = rt .. (descr.llvmthis or context.descr.llvmtype) .. "*, " end
	for ai,arg in ipairs(descr.param) do rt = rt .. arg.llvmtype .. ", " end
	if rt ~= "" then rt = rt:sub(1, -3) end
	return rt
end
function getTypeDeclContextTypeId( context)
	if context then if context.qualitype then return context.qualitype.lval else return context.namespace or 0 end else return 0 end
end
-- Symbol name for type in target LLVM output
function getGenericParameterIdString( param)
	local rt = ""
	for pi,arg in ipairs(param) do if arg.value then rt = rt .. "#" .. arg.value .. "," else rt = rt .. tostring(arg.type) .. "," end end
	return rt
end
-- Synthezized typename for generic
function getGenericTypeName( typeId, param)
	local rt = typedb:type_string( typeId, "__")
	for pi,arg in ipairs(param) do if arg.value then rt = rt .. arg.value else rt = rt .. "__" .. typedb:type_string(arg.type, "__") end end
	return rt
end
-- Symbol name for type in target LLVM output
function getTypeDeclUniqueName( contextTypeId, typnam)
	if contextTypeId == 0 then 
		return utils.uniqueName( typnam .. "__")
	else		
		return utils.uniqueName( typedb:type_string(contextTypeId, "__") .. "__" .. typnam .. "__")
	end
end
-- Function shared by all expansions of the structure used for mapping the LLVM template for a function call
function expandDescrCallTemplateParameter( descr, context)
	if descr.ret then descr.rtllvmtype = typeDescriptionMap[ descr.ret].llvmtype else descr.rtllvmtype = "void" end
	descr.argstr = getDeclarationLlvmTypedefParameterString( descr, context)
	descr.symbolname = getTargetFunctionIdentifierString( descr.symbol, descr.param, descr.const, context)
end
function expandDescrMethod( descr, context)
	descr.methodid = getInterfaceMethodIdentifierString( descr.symbol, descr.param, descr.const)
	descr.methodname = getInterfaceMethodNameString( descr.name, descr.param, descr.const)
	descr.llvmtype = utils.template_format( llvmir.control.functionCallType, descr)
end
-- Expand the structure used for mapping the LLVM template for a class method call with keys derived from the call description
function expandDescrClassCallTemplateParameter( descr, context)
	expandDescrCallTemplateParameter( descr, context)
	descr.paramstr = getDeclarationLlvmTypeRegParameterString( descr, context)
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
	descr.paramstr = getDeclarationLlvmTypeRegParameterString( descr, context)
	descr.llvmtype = utils.template_format( llvmir.control.functionCallType, descr)
end
-- Expand the structure used for mapping the LLVM template for an function/procedure variable type
function expandDescrFunctionReferenceTemplateParameter( descr, context)
	if descr.ret then descr.rtllvmtype = typeDescriptionMap[ descr.ret].llvmtype else descr.rtllvmtype = "void" end
	descr.argstr = getDeclarationLlvmTypedefParameterString( descr, context)
	descr.signature = "(" .. descr.argstr .. ")"
end
-- Expand the structure used for mapping the LLVM template for an extern function call with keys derived from the call description
function expandDescrExternCallTemplateParameter( descr, context)
	descr.argstr = getDeclarationLlvmTypedefParameterString( descr, context)
	if descr.externtype == "C" then
		descr.symbolname = descr.name
	else
		utils.errorMessage( node.line, "Unknown extern call type \"%s\" (must be one of {\"C\"})", descr.externtype)
	end
	if descr.ret then descr.rtllvmtype = typeDescriptionMap[ descr.ret].llvmtype else descr.rtllvmtype = "void" end
	if descr.vararg then
		local sep; if descr.argstr == "" then sep = "" else sep = ", " end
		descr.signature = utils.template_format( llvmir.control.functionVarargSignature,  {argstr = descr.argstr .. sep})
	end
	descr.llvmtype = utils.template_format( llvmir.control.functionCallType, descr)
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
	local fmt; if doReturnValueAsReferenceParameter( descr.ret) then fmt = llvmir.control.sretFunctionDeclaration else fmt = llvmir.control.functionDeclaration end
	print( "\n" .. utils.constructor_format( fmt, descr))
end
function printExternFunctionDeclaration( node, descr)
	local fmt; if descr.vararg then fmt = llvmir.control.extern_functionDeclaration_vararg else fmt = llvmir.control.extern_functionDeclaration end
	print( "\n" .. utils.constructor_format( fmt, descr))
end
function defineFunctionCall( thisTypeId, contextTypeId, opr, descr)
	local callfmt = utils.template_format( descr.call, descr)
	local rtype; if doReturnValueAsReferenceParameter( descr.ret) then rtype = rvalueTypeMap[ descr.ret] else rtype = descr.ret end
	local functype = defineCall( rtype, contextTypeId, opr, descr.param, functionCallConstructor( callfmt, thisTypeId, descr.ret))
	if descr.vararg then varargFuncMap[ functype] = true end
	return functype
end
function defineInterfaceMethodCall( contextTypeId, opr, descr)
	local loadfmt = utils.template_format( descr.load, descr)
	local callfmt = utils.template_format( descr.call, descr)
	local rtype; if doReturnValueAsReferenceParameter( descr.ret) then rtype = rvalueTypeMap[ descr.ret] else rtype = descr.ret end
	local functype = defineCall( rtype, contextTypeId, opr, descr.param, interfaceMethodCallConstructor( loadfmt, callfmt, descr.ret))
	if descr.vararg then varargFuncMap[ functype] = true end
end
function getOrCreateCallableContextTypeId( contextTypeId, name, descr)
	local rt = typedb:get_type( contextTypeId, name)
	if not rt then
		rt = typedb:def_type( contextTypeId, name)
		typeDescriptionMap[ rt] = descr
		return rt,true -- type is new
	end
	return rt,false
end
-- Define a procedure pointer type as callable object with "()" operator implementing the call
function defineProcedureVariableType( node, descr, context)
	local declContextTypeId = getTypeDeclContextTypeId( context)
	expandDescrFunctionReferenceTemplateParameter( descr, context)
	local descr = llvmir.procedureVariableDescr( descr, descr.signature)
	local qualitype = defineQualifiedTypes( node, declContextTypeId, descr.name, descr)
	defineFunctionCall( qualitype.c_lval, declContextTypeId, "()", descr)
	defineCall( qualitype.rval, qualitype.rval, ":=", {anyFreeFunctionType}, assignFunctionPointerConstructor( descr))
	defineCall( qualitype.c_rval, qualitype.c_rval, ":=", {anyFreeFunctionType}, assignFunctionPointerConstructor( descr))
end
-- Define a function pointer type as callable object with "()" operator implementing the call
function defineFunctionVariableType( node, descr, context)
	local declContextTypeId = getTypeDeclContextTypeId( context)
	expandDescrFunctionReferenceTemplateParameter( descr, context)
	local descr = llvmir.functionVariableDescr( descr, descr.rtllvmtype, descr.signature)
	local qualitype = defineQualifiedTypes( node, declContextTypeId, descr.name, descr)
	defineFunctionCall( qualitype.c_lval, declContextTypeId, "()", descr)
	defineCall( qualitype.rval, qualitype.rval, ":=", {anyFreeFunctionType}, assignFunctionPointerConstructor( descr))
	defineCall( qualitype.c_rval, qualitype.c_rval, ":=", {anyFreeFunctionType}, assignFunctionPointerConstructor( descr))
end
-- Define a free function as callable object with "()" operator implementing the call
function defineFreeFunction( node, descr, context)
	expandDescrFreeCallTemplateParameter( descr, context)
	local callablectx,newName = getOrCreateCallableContextTypeId( 0, descr.name, llvmir.callableDescr)
	local functype = defineFunctionCall( 0, callablectx, "()", descr)
	if newName then typedb:def_reduction( anyFreeFunctionType, callablectx, function(a) return {type = callablectx} end, tag_transfer, rdw_conv) end
	return functype
end
-- Define a class method as callable object with "()" operator implementing the call
function defineClassMethod( node, descr, context)
	local thisTypeId = getFunctionThisType( descr.private, descr.const, context.qualitype.rval)
	expandDescrClassCallTemplateParameter( descr, context)
	expandContextMethodList( node, descr, context)
	local callablectx = getOrCreateCallableContextTypeId( thisTypeId, descr.name, llvmir.callableDescr)
	defineFunctionCall( thisTypeId, callablectx, "()", descr)
end
-- Define an operator on the class with its arguments
function defineClassOperator( node, descr, context)
	expandDescrClassCallTemplateParameter( descr, context)
	expandContextMethodList( node, descr, context)
	contextTypeId = getFunctionThisType( descr.private, descr.const, context.qualitype.rval)
	defineFunctionCall( contextTypeId, contextTypeId, descr.name, descr)
	defineOperatorAttributes( context, descr)
end
-- Define an constructor/destructor
function defineConstructorDestructor( node, descr, context)
	expandDescrClassCallTemplateParameter( descr, context)
	contextTypeId = getFunctionThisType( descr.private, descr.const, context.qualitype.rval)
	defineFunctionCall( contextTypeId, contextTypeId, descr.name, descr)
	if constTypeMap[ contextTypeId] then defineFunctionCall( contextTypeId, constTypeMap[ contextTypeId], descr.name, descr) end
	defineOperatorAttributes( context, descr)
end
-- Define an extern function as callable object with "()" operator implementing the call
function defineExternFunction( node, descr)
	if doReturnValueAsReferenceParameter( descr.ret) then utils.errorMessage( node.line, "Type not allowed as extern function return value") end
	expandDescrExternCallTemplateParameter( descr, context)
	local callablectx = getOrCreateCallableContextTypeId( 0, descr.name, llvmir.callableDescr)
	defineFunctionCall( 0, callablectx, "()", descr)
end
-- Define an interface method call as callable object with "()" operator implementing the call
function defineInterfaceMethod( node, descr, context)
	local thisTypeId = getFunctionThisType( descr.private, descr.const, context.qualitype.rval)
	expandDescrInterfaceCallTemplateParameter( descr, context)
	expandContextMethodList( node, descr, context)
	expandContextLlvmMember( descr, context)
	local callablectx = getOrCreateCallableContextTypeId( thisTypeId, descr.name, llvmir.callableDescr)
	defineInterfaceMethodCall( callablectx, "()", descr)
end
-- Define an operator on the interface with its arguments
function defineInterfaceOperator( node, descr, context)
	expandDescrInterfaceCallTemplateParameter( descr, context)
	expandContextMethodList( node, descr, context)
	expandContextLlvmMember( descr, context)
	contextTypeId = getFunctionThisType( descr.private, descr.const, context.qualitype.rval)
	defineInterfaceMethodCall( contextTypeId, descr.name, descr)
	defineOperatorAttributes( context, descr)
end
-- Define the execution context of the body of any function/procedure/operator except the main function
function defineCallableBodyContext( node, context, descr)
	local prev_scope = typedb:scope( node.scope)
	if context and context.qualitype then
		local privateThisReferenceType = getFunctionThisType( true, descr.const, context.qualitype.rval)
		local privateThisPointerType = getFunctionThisType( true, descr.const, context.qualitype.pval)
		defineVariableHardcoded( privateThisPointerType, "this", "%ths")
		local classvar = defineVariableHardcoded( privateThisReferenceType, "*this", "%ths")
		pushDefContextTypeConstructorPair( {type=classvar, constructor={out="%ths"}})
	end
	defineCallableEnvironment( node.scope, descr.ret)
	typedb:scope( prev_scope)
end
-- Define the execution context of the body of the main function
function defineMainProcContext( node)
	local prev_scope = typedb:scope( node.scope)
	defineVariableHardcoded( stringPointerType, "argc", "%argc")
	defineVariableHardcoded( scalarIntegerType, "argv", "%argv")
	defineCallableEnvironment( node.scope, scalarIntegerType)
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
	local callable = getCallableEnvironment()
	if not callable then
		return {type=stringAddressType,constructor=stringConstantMap[ value]}
	else
		out = callable.register()
		return {type=stringPointerType,constructor={code=utils.constructor_format( stringConstantMap[ value].fmt, {out=out}), out=out}}
	end
end
-- Basic structure type definition for class,struct,interface
function defineStructureType( node, declContextTypeId, typnam, fmt)
	local symbol = getTypeDeclUniqueName( declContextTypeId, typnam)
	local descr = utils.template_format( fmt, {symbol=symbol})
	local qualitype = defineQualifiedTypes( node, declContextTypeId, typnam, descr)
	return descr,qualitype
end
-- Basic type definition for generic 
function defineGenericType( node, declContextTypeId, typnam, fmt, generic)
	local descr = utils.template_format( fmt, {})
	descr.node = node
	descr.generic = generic
	descr.instancemap = {}
	local lval = typedb:def_type( declContextTypeId, typnam)
	if lval == -1 then utils.errorMessage( node.line, "Duplicate definition of type '%s %s'", typedb:type_string(declContextTypeId), typnam) end
	typeDescriptionMap[ lval] = descr
end
-- Add a generic parameter to the generic parameter list
function addGenericParameter( node, generic, name, typeId)
	if generic.namemap[ name] then utils.errorMessage( node.line, "Duplicate definition of generic parameter '%s'", name) end
	generic.namemap[ name] = #generic.param+1
	table.insert( generic.param, {name=name, type=typeId} )
end
function traverseAstInterfaceDef( node, declContextTypeId, typnam, descr, qualitype, nodeidx)
	pushDefContextTypeConstructorPair( qualitype.lval)
	defineRValueReferenceTypes( declContextTypeId, typnam, descr, qualitype)
	local context = {qualitype=qualitype, descr=descr, operators={}, methods={}, methodmap={}, llvmtype="", symbol=descr.symbol, const=true}
	utils.traverseRange( typedb, node, {nodeidx,#node.arg}, context)
	descr.methods = context.methods
	descr.methodmap = context.methodmap
	descr.const = context.const
	definePointerCompareOperators( qualitype, typeDescriptionMap[ qualitype.c_pval])
	defineInterfaceConstructors( node, qualitype, descr, context)
	defineOperatorsWithStructArgument( node, context)
	print_section( "Typedefs", utils.template_format( descr.vmtdef, {llvmtype=context.llvmtype}))
	print_section( "Typedefs", descr.typedef)
	popDefContextTypeConstructorPair( qualitype.lval)
end
function traverseAstClassDef( node, declContextTypeId, typnam, descr, qualitype, nodeidx)
	pushDefContextTypeConstructorPair( qualitype.lval)
	defineRValueReferenceTypes( declContextTypeId, typnam, descr, qualitype)
	definePublicPrivate( declContextTypeId, typnam, descr, qualitype)
	local context = {qualitype=qualitype, descr=descr, index=0, private=true, members={}, operators={}, methods={}, methodmap={},
	                 structsize=0, llvmtype="", symbol=descr.symbol, interfaces={}, properties={}}
	-- Traversal in two passes, first type and variable declarations, then method definitions
	utils.traverseRange( typedb, node, {nodeidx,#node.arg}, context, 1)
	utils.traverseRange( typedb, node, {nodeidx,#node.arg}, context, 2)
	descr.size = context.structsize
	definePointerCompareOperators( qualitype, typeDescriptionMap[ qualitype.c_pval])
	defineClassConstructors( node, qualitype, descr, context)
	defineOperatorsWithStructArgument( node, context)
	defineInheritedInterfaces( node, context, qualitype.lval)
	print_section( "Typedefs", utils.template_format( descr.typedef, {llvmtype=context.llvmtype}))
	popDefContextTypeConstructorPair( qualitype.lval)
end
function traverseAstStructDef( node, declContextTypeId, typnam, descr, qualitype, nodeidx)
	pushDefContextTypeConstructorPair( qualitype.lval)
	defineRValueReferenceTypes( declContextTypeId, typnam, descr, qualitype)
	local context = {qualitype=qualitype, descr=descr, index=0, private=false, members={}, structsize=0, llvmtype="", symbol=descr.symbol}
	utils.traverseRange( typedb, node, {nodeidx,#node.arg}, context)
	descr.size = context.structsize
	definePointerCompareOperators( qualitype, typeDescriptionMap[ qualitype.c_pval])
	defineStructConstructors( node, qualitype, descr, context)
	print_section( "Typedefs", utils.template_format( descr.typedef, {llvmtype=context.llvmtype}))
	popDefContextTypeConstructorPair( qualitype.lval)
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
	return defineParameter( node, arg[1], arg[2], getCallableEnvironment())
end

function typesystem.paramdeflist( node)
	return utils.traverse( typedb, node)
end

function typesystem.structure( node, context)
	local arg = utils.traverse( typedb, node, context)
	return {type=constexprStructureType, constructor=arg}
end
function typesystem.allocate( node, context)
	local callable = getCallableEnvironment()
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
	local callable = getCallableEnvironment()
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
	local callable = getCallableEnvironment()
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
function typesystem.typedef( node, context)
	local typnam = node.arg[2].value
	local arg = utils.traverse( typedb, node)
	local declContextTypeId = getTypeDeclContextTypeId( context)
	local descr = typeDescriptionMap[ arg[1]]
	local orig_qualitype = qualiTypeMap[ arg[1]]
	local this_qualitype = defineQualifiedTypes( node, declContextTypeId, typnam, descr)
	local convlist = {"lval","c_lval","c_rval"}
	for ci,conv in ipairs(convlist) do
		typedb:def_reduction( this_qualitype[conv], orig_qualitype[conv], nil, tag_typeConversion, rdw_conv)
		typedb:def_reduction( orig_qualitype[conv], this_qualitype[conv], nil, tag_typeConversion, rdw_conv)
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
	return {code=getAllocationScopeCodeBlock( code)}
end
function typesystem.return_value( node)
	local arg = utils.traverse( typedb, node)
	local callable = getCallableEnvironment()
	local rtype = callable.returntype
	if rtype == 0 then utils.errorMessage( node.line, "Can't return value from procedure") end
	if doReturnValueAsReferenceParameter( rtype) then
		local reftype = referenceTypeMap[ rtype]
		local rt = applyCallable( node, typeConstructorStruct( reftype, "%rt", ""), ":=", {arg[1]})
		return {code = rt.constructor.code .. "ret void\n", nofollow=true}
	else
		local constructor = getRequiredTypeConstructor( node, rtype, arg[1], tagmask_matchParameter)
		if not constructor then utils.errorMessage( node.line, "Return value does not match declared return type '%s'", typedb:type_string(rtype)) end
		local code = utils.constructor_format( llvmir.control.returnStatement, {type=typeDescriptionMap[rtype].llvmtype, this=constructor.out})
		return {code = constructor.code .. code, nofollow=true}
	end
end
function typesystem.return_void( node)
	local callable = getCallableEnvironment()
	local rtype = callable.returntype
	if rtype ~= 0 then utils.errorMessage( node.line, "Can't return without value from function") end
	return {code = llvmir.control.returnFromProcedure, nofollow=true}
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
	local callable = getCallableEnvironment()
	local exit = callable.label()
	local elsecode = utils.constructor_format( llvmir.control.invertedControlType, {inp=cond_constructor.out, out=exit})
	return {code = cond_constructor.code .. arg[2].code .. elsecode .. arg[3].code .. utils.constructor_format( llvmir.control.label, {inp=exit})}
end
function typesystem.conditional_while( node)
	local arg = utils.traverse( typedb, node)
	local cond_constructor = getRequiredTypeConstructor( node, controlTrueType, arg[1], tagmask_matchParameter)
	if not cond_constructor then utils.errorMessage( node.line, "Can't use type '%s' as a condition", typedb:type_string(arg[1].type)) end
	local callable = getCallableEnvironment()
	local start = callable.label()
	local startcode = utils.constructor_format( llvmir.control.label, {inp=start})
	return {code = startcode .. cond_constructor.code .. arg[2].code 
			.. utils.constructor_format( llvmir.control.invertedControlType, {out=start,inp=cond_constructor.out})}
end
function typesystem.with_do( node, context)
	local arg = utils.traverseRange( typedb, node, {1,1}, context)
	pushDefContextTypeConstructorPair( arg[1])
	return utils.traverseRange( typedb, node, {2,2}, context)[2]
end
function typesystem.typehdr( node, qualifier)
	return resolveTypeFromNamePath( node, qualifier, utils.traverse( typedb, node))
end
function typesystem.typehdr_any( node, typeName)
	return hardcodedTypeMap[ typeName]
end
function typesystem.typehdr_generic( node, qualifier, context)
	local arg = utils.traverse( typedb, node, context)
	local inst_arg = arg[ #arg]
	table.remove( arg, #arg)
	local genericType = resolveTypeFromNamePath( node, "", arg)
	local genericDescr = typeDescriptionMap[ genericType]
	if string.sub(genericDescr.class,1,8) == "generic_" then
		local scope_bk = typedb:scope( typedb:type_scope( genericType))
		local generic_arg = matchGenericParameter( node, genericType, genericDescr.generic.param, inst_arg)
		local instanceid = getGenericParameterIdString( generic_arg)
		local instanceType = genericDescr.instancemap[ instanceid]
		if not instanceType then
			local declContextTypeId = typedb:type_context( genericType)
			local typnam = getGenericTypeName( genericType, generic_arg)
			local fmt; if genericDescr.class == "generic_class" then
				fmt = llvmir.classTemplate
			elseif genericDescr.class == "generic_struct" then
				fmt = llvmir.structTemplate
			end
			local descr,qualitype = defineStructureType( genericDescr.node, declContextTypeId, typnam, fmt)
			instanceType = qualitype.lval
			defineGenericParameterAliases( instanceType, genericDescr.generic.param, generic_arg)
			if genericDescr.class == "generic_class" then
				traverseAstClassDef( genericDescr.node, declContextTypeId, typnam, descr, qualitype, 3)
			elseif genericDescr.class == "generic_struct" then
				traverseAstStructDef( genericDescr.node, declContextTypeId, typnam, descr, qualitype, 3)
			else
				utils.errorMessage( node.line, "Using generic parameter in '<' '>' brackets for unknown generic class '%s'", genericDescr.class)
			end
		end
		typedb:scope( scope_bk)
		return instanceType
	else
		utils.errorMessage( node.line, "Using generic parameter in '<' '>' brackets for non generic type '%s'", typedb:type_string(genericType))
	end
end
function typesystem.typedim_array( node, qualifier)
	local arg = utils.traverse( typedb, node, context)
	local dim = getConstexprValue( constexprUIntegerType, arg[2].type, arg[2].constructor)
	if not dim then utils.errorMessage( node.line, "Size of array is not an unsigned integer constant expression") end
	local arsize = tonumber(dim)
	if arsize <= 0 then utils.errorMessage( node.line, "Size of array is not a positive integer number") end
	local elementTypeId = arg[1]
	local qs = typeQualiSepMap[ elementTypeId]
	local strippedElementTypeId; if isPointerType(elementTypeId) then strippedElementTypeId = qs.pval else strippedElementTypeId = qs.lval end
	local arrayTypeId = implicitDefineArrayType( node, strippedElementTypeId, arsize)
	if constTypeMap[ elementTypeId] then return arrayTypeId else return constTypeMap[ arrayTypeId] end
		-- const qualifier stripped from element and transfered to array type
end
function typesystem.typespec_ref( node, qualifier)
	local arg = utils.traverse( typedb, node)
	return referenceTypeMap[ arg[1]] or utils.errorMessage( node.line, "Type '%s' has no reference", typedb:type_string(arg[1]))
end
function typesystem.constant( node, typeName)
	local typeId = hardcodedTypeMap[ typeName]
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
	local callable = getCallableEnvironment()
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
			name = arg[2], symbol = arg[2], ret = arg[3], private=arg[1].private, const=decl.const, interface=false}
	if doReturnValueAsReferenceParameter( descr.ret) then descr.call = llvmir.control.sretFunctionCall else descr.call = llvmir.control.functionCall end
	descr.param = utils.traverseRange( typedb, node, {4,4}, context, descr, 1)[4]
	local isInterface = false
	if context and context.qualitype then 
		defineClassMethod( node, descr, context)
		descr.interface = isInterfaceMethod( context, descr.methodid)
		descr.attr = llvmir.functionAttribute( descr.interface)
	else
		local functype = defineFreeFunction( node, descr, context)
		typeDescriptionMap[ functype] = llvmir.functionReferenceDescr( descr.ret, descr.symbolname)
	end
	descr.body  = utils.traverseRange( typedb, node, {4,4}, context, descr, 2)[4]
	printFunctionDeclaration( node, descr)
end
function typesystem.procdef( node, decl, context)
	local arg = utils.traverseRange( typedb, node, {1,2}, context)
	local descr = {call = llvmir.control.procedureCall, lnk = arg[1].linkage, attr = llvmir.functionAttribute( arg[1].private), signature="",
			name = arg[2], symbol = arg[2], ret = nil, private=arg[1].private, const=decl.const, interface=false}
	descr.param = utils.traverseRange( typedb, node, {3,3}, context, descr, 1)[3]
	if context and context.qualitype then
		defineClassMethod( node, descr, context)
		descr.interface = isInterfaceMethod( context, descr.methodid)
		descr.attr = llvmir.functionAttribute( descr.interface)
	else
		local proctype = defineFreeFunction( node, descr, context)
		typeDescriptionMap[ proctype] = llvmir.procedureReferenceDescr( descr.symbolname)
	end
	descr.body  = utils.traverseRange( typedb, node, {3,3}, context, descr, 2)[3] .. "ret void\n"
	printFunctionDeclaration( node, descr)
end
function typesystem.operatordecl( node, opr)
	return opr
end
function typesystem.operator_funcdef( node, decl, context)
	local arg = utils.traverseRange( typedb, node, {1,3}, context)
	local descr = {lnk = arg[1].linkage, attr = llvmir.functionAttribute( arg[1].private), signature="",
			name = arg[2].name, symbol = "$" .. arg[2].symbol, ret = arg[3], private=arg[1].private, const=decl.const, interface=false}
	if doReturnValueAsReferenceParameter( descr.ret) then descr.call = llvmir.control.sretFunctionCall else descr.call = llvmir.control.functionCall end
	descr.param = utils.traverseRange( typedb, node, {4,4}, context, descr, 1)[4]
	descr.interface = isInterfaceMethod( context, descr.methodid)
	descr.attr = llvmir.functionAttribute( descr.interface)
	defineClassOperator( node, descr, context)
	descr.body  = utils.traverseRange( typedb, node, {4,4}, context, descr, 2)[4]
	printFunctionDeclaration( node, descr)
end
function typesystem.operator_procdef( node, decl, context)
	local arg = utils.traverseRange( typedb, node, {1,2}, context)
	local descr = {call = llvmir.control.procedureCall, lnk = arg[1].linkage, attr = llvmir.functionAttribute( arg[1].private), signature="",
			name = arg[2].name, symbol = "$" .. arg[2].symbol, ret = nil, private=arg[1].private, const=decl.const, interface=false}
	descr.param = utils.traverseRange( typedb, node, {3,3}, context, descr, 1)[3]
	descr.interface = isInterfaceMethod( context, descr.methodid)
	descr.attr = llvmir.functionAttribute( descr.interface)
	defineClassOperator( node, descr, context)
	descr.body  = utils.traverseRange( typedb, node, {3,3}, context, descr, 2)[3] .. "ret void\n"
	printFunctionDeclaration( node, descr)
end
function typesystem.constructordef( node, context)
	local arg = utils.traverseRange( typedb, node, {1,1}, context)
	local descr = {call = llvmir.control.procedureCall, lnk = arg[1].linkage, attr = llvmir.functionAttribute( arg[1].private), signature="",
			name = ":=", symbol = "$ctor", ret = nil, private=arg[1].private, const=false, interface=false}
	descr.param = utils.traverseRange( typedb, node, {2,2}, context, descr, 1)[2]
	context.properties.constructor = true
	defineConstructorDestructor( node, descr, context)
	descr.body  = utils.traverseRange( typedb, node, {2,2}, context, descr, 2)[2] .. "ret void\n"
	printFunctionDeclaration( node, descr)
end
function typesystem.destructordef( node, context)
	local arg = utils.traverseRange( typedb, node, {1,1}, context)
	local descr = {call = llvmir.control.procedureCall, lnk = arg[1].linkage, attr = llvmir.functionAttribute( arg[1].private), signature="",
	               name = ":~", symbol = "$dtor", ret = nil, private=false, const=false, interface=false}
	context.properties.destructor = true
	descr.param = utils.traverseRange( typedb, node, {2,2}, context, descr, 1)[2]
	defineConstructorDestructor( node, descr, context)
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
	local descr = {call = llvmir.control.functionCall, externtype = arg[1], name = arg[2], symbol = arg[2], ret = arg[3], param = arg[4],
	               vararg=false, signature=""}
	defineExternFunction( node, descr)
	printExternFunctionDeclaration( node, descr)
end
function typesystem.extern_procdef( node)
	local arg = utils.traverse( typedb, node)
	local descr = {call = llvmir.control.procedureCall, externtype = arg[1], name = arg[2], symbol = arg[2], param = arg[3],
	               vararg=false, signature=""}
	defineExternFunction( node, descr)
	printExternFunctionDeclaration( node, descr)
end
function typesystem.extern_funcdef_vararg( node)
	local arg = utils.traverse( typedb, node)
	local descr = {call = llvmir.control.functionCall, externtype = arg[1], name = arg[2], symbol = arg[2], ret = arg[3], param = arg[4],
	               vararg=true, signature=""}
	defineExternFunction( node, descr)
	printExternFunctionDeclaration( node, descr)
end
function typesystem.extern_procdef_vararg( node)
	local arg = utils.traverse( typedb, node)
	local descr = {call = llvmir.control.procedureCall, externtype = arg[1], name = arg[2], symbol = arg[2], param = arg[3],
	               vararg=true, signature=""}
	defineExternFunction( node, descr)
	printExternFunctionDeclaration( node, descr)
end
function typesystem.typedef_functype( node, context)
	local arg = utils.traverse( typedb, node)
	local descr = {ret = arg[2], param = arg[3], name=arg[1], signature=""}
	defineFunctionVariableType( node, descr, context)
end
function typesystem.typedef_proctype( node, context)
	local arg = utils.traverse( typedb, node)
	local descr = {ret = nil, param = arg[2], name=arg[1]}
	defineProcedureVariableType( node, descr, context)
end
function typesystem.interface_funcdef( node, decl, context)
	local arg = utils.traverse( typedb, node, context)
	local descr = {name = arg[1], symbol = arg[1], ret = arg[2], param = arg[3], signature="", private=false, const=decl.const,
	               index=#context.methods, llvmthis="i8", load = context.descr.loadVmtMethod}
	if doReturnValueAsReferenceParameter( descr.ret) then descr.call=context.descr.sretFunctionCall else descr.call=context.descr.functionCall end
	defineInterfaceMethod( node, descr, context)
end
function typesystem.interface_procdef( node, decl, context)
	local arg = utils.traverse( typedb, node, context)
	local descr = {name = arg[1], symbol = arg[1], ret = nil, param = arg[2], signature="", private=false, const=decl.const,
	               index=#context.methods, llvmthis="i8", load = context.descr.loadVmtMethod, call = context.descr.procedureCall}
	defineInterfaceMethod( node, descr, context)
end
function typesystem.interface_operator_funcdef( node, decl, context)
	local arg = utils.traverse( typedb, node, context)
	local descr = {name = arg[1].name, symbol = "$" .. arg[1].symbol, ret = arg[2], param = arg[3], signature="", private=false, const=decl.const,
			index=#context.methods, llvmthis="i8", load = context.descr.loadVmtMethod}
	if doReturnValueAsReferenceParameter( descr.ret) then descr.call=context.descr.sretFunctionCall else descr.call=context.descr.functionCall end
	defineInterfaceOperator( node, descr, context)
end
function typesystem.interface_operator_procdef( node, decl, context)
	local arg = utils.traverse( typedb, node, context)
	local descr = {name = arg[1].name, symbol = "$" .. arg[1].symbol, ret = nil, param = arg[2], signature = "", private=false, const=decl.const,
	               index=#context.methods, llvmthis="i8", load = context.descr.loadVmtMethod, call = context.descr.procedureCall}
	defineInterfaceOperator( node, descr, context)
end
function typesystem.namespacedef( node, context)
	local typnam = node.arg[1].value
	local declContextTypeId = getTypeDeclContextTypeId( context)
	local scope_bk = typedb:scope( {} ) -- set global scope
	local namespace = typedb:get_type( declContextTypeId, typnam) or typedb:def_type( declContextTypeId, typnam)
	typedb:scope( scope_bk)
	utils.traverseRange( typedb, node, {2,2}, {namespace = namespace} )
end
function typesystem.structdef( node, context)
	local typnam = node.arg[1].value
	local declContextTypeId = getTypeDeclContextTypeId( context)
	local descr,qualitype = defineStructureType( node, declContextTypeId, typnam, llvmir.structTemplate)
	traverseAstStructDef( node, declContextTypeId, typnam, descr, qualitype, 2)
end
function typesystem.generic_structdef( node, context)
	local typnam = node.arg[1].value
	local declContextTypeId = getTypeDeclContextTypeId( context)
	local param = utils.traverseRange( typedb, node, {2,2}, context)[2]
	defineGenericType( node, declContextTypeId, typnam, llvmir.llvmir.genericStructDescr, param)
end
function typesystem.interfacedef( node, context)
	local typnam = node.arg[1].value
	local declContextTypeId = getTypeDeclContextTypeId( context)
	local descr,qualitype = defineStructureType( node, declContextTypeId, typnam, llvmir.interfaceTemplate)
	traverseAstInterfaceDef( node, declContextTypeId, typnam, descr, qualitype, 2)
end
function typesystem.generic_instance_type( node, context, generic_instancelist)
	table.insert( generic_instancelist, {type=utils.traverseRange( typedb, node, {1,1}, context)[1]} )
	if #node.arg > 1 then utils.traverseRange( typedb, node, {2,2}, context, generic_instancelist) end
end
function typesystem.generic_instance_dimension( node, context, generic_instancelist)
	table.insert( generic_instancelist, {type=constexprUIntegerType, value=node.arg[1].value} )
	if #node.arg > 1 then utils.traverseRange( typedb, node, {2,2}, context, generic_instancelist) end
end
function typesystem.generic_instance( node, context)
	local rt = {}
	utils.traverse( typedb, node, context, rt)
	return rt
end
function typesystem.generic_header_ident_type( node, context, generic)
	local arg = utils.traverseRange( typedb, node, {1,2}, context)
	addGenericParameter( node, generic, arg[1], arg[2])
	if #arg > 2 then utils.traverseRange( typedb, node, {3,3}, context, generic) end
end
function typesystem.generic_header_ident( node, context, generic)
	addGenericParameter( node, generic, node.arg[1].value)
	if #node.arg > 1 then utils.traverseRange( typedb, node, {2,2}, context, generic) end
end
function typesystem.generic_header( node, context)
	local generic = {param={}, namemap={}}
	utils.traverse( typedb, node, context, generic)
	return generic
end
function typesystem.classdef( node, context)
	local typnam = node.arg[1].value
	local declContextTypeId = getTypeDeclContextTypeId( context)
	local descr,qualitype = defineStructureType( node, declContextTypeId, typnam, llvmir.classTemplate)
	traverseAstClassDef( node, declContextTypeId, typnam, descr, qualitype, 2)
end
function typesystem.generic_classdef( node, context)
	local typnam = node.arg[1].value
	local declContextTypeId = getTypeDeclContextTypeId( context)
	local generic = utils.traverseRange( typedb, node, {2,2}, context)[2]
	defineGenericType( node, declContextTypeId, typnam, llvmir.genericClassDescr, generic)
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
	mainCallableEnvironment = createCallableEnvironment( typedb:scope(), nil, "%ir", "IL")
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
