local mewa = require "mewa"
local llvmir = require "language1/llvmir"
local utils = require "typesystem_utils"
local bcd = require "bcd"
local typedb = mewa.typedb()

local tag_typeAlias = 1			-- Type that describes a substitution (as used for generic parameters)
local tag_typeDeduction = 2		-- Inheritance relation
local tag_typeDeclaration = 3		-- Type declaration relation (e.g. variable to data type)
local tag_typeConversion = 4		-- Type conversion of parameters
local tag_typeInstantiation = 5		-- Type value construction from const expression
local tag_namespace = 6			-- Type deduction for resolving namespaces
local tag_pointerReinterpretCast = 7	-- Type deduction only allowed in an explicit "cast" operation
local tag_pushVararg = 8		-- Type deduction for passing parameter as vararg argument
local tag_transfer = 9			-- Transfer of information to build an object by a constructor, used in free function callable to pointer assignment

local tagmask_declaration = typedb.reduction_tagmask( tag_typeAlias, tag_typeDeclaration)
local tagmask_resolveType = typedb.reduction_tagmask( tag_typeAlias, tag_typeDeduction, tag_typeDeclaration)
local tagmask_matchParameter = typedb.reduction_tagmask( tag_typeAlias, tag_typeDeduction, tag_typeDeclaration, tag_typeConversion, tag_typeInstantiation, tag_transfer)
local tagmask_typeConversion = typedb.reduction_tagmask( tag_typeConversion)
local tagmask_namespace = typedb.reduction_tagmask( tag_typeAlias, tag_namespace)
local tagmask_typeCast = typedb.reduction_tagmask( tag_typeAlias, tag_typeDeduction, tag_typeDeclaration, tag_typeConversion, tag_typeInstantiation, tag_pointerReinterpretCast)
local tagmask_typeAlias = typedb.reduction_tagmask( tag_typeAlias)
local tagmask_pushVararg = typedb.reduction_tagmask( tag_typeAlias, tag_typeDeduction, tag_typeDeclaration, tag_typeConversion, tag_typeInstantiation, tag_pushVararg)

-- Centralized list of the ordering of the reduction rules determined by their weights, we force an order of reductions by defining the weight sums as polynomials:
local rdw_conv = 1.0			-- Reduction weight of conversion
local rdw_constexpr = 0.0675		-- Minimum weight of a reduction involving a constexpr value
local rdw_sign = 0.125			-- Conversion of integers changing sign
local rdw_float = 0.25			-- Conversion switching floating point with integers
local rdw_bool = 0.25			-- Conversion of numeric value to boolean
local rdw_barrier = 2.0			-- Reduction weight of a reduction that invokes a state transition if used
local rdw_strip_u_1 = 0.75 / (1*1)	-- Reduction weight of stripping unbound reftype from type having 1 qualifier 
local rdw_strip_u_2 = 0.75 / (2*2)	-- Reduction weight of stripping unbound reftype from type having 2 qualifiers
local rdw_strip_u_3 = 0.75 / (3*3)	-- Reduction weight of stripping unbound reftype from type having 3 qualifiers
local rdw_strip_v_1 = 0.125 / (1*1)	-- Reduction weight of stripping private/public from type having 1 qualifier 
local rdw_strip_v_2 = 0.125 / (2*2)	-- Reduction weight of stripping private/public from type having 2 qualifiers
local rdw_strip_v_3 = 0.125 / (3*3)	-- Reduction weight of stripping private/public from type having 3 qualifiers
local rdw_strip_r_1 = 0.25 / (1*1)	-- Reduction weight of stripping reference (fetch value) from type having 1 qualifier 
local rdw_strip_r_2 = 0.25 / (2*2)	-- Reduction weight of stripping reference (fetch value) from type having 2 qualifiers
local rdw_strip_r_3 = 0.25 / (3*3)	-- Reduction weight of stripping reference (fetch value) from type having 3 qualifiers
local rdw_strip_c_1 = 0.5 / (1*1)	-- Reduction weight of changing constant qualifier in type having 1 qualifier
local rdw_strip_c_2 = 0.5 / (2*2)	-- Reduction weight of changing constant qualifier in type having 2 qualifiers
local rdw_strip_c_3 = 0.5 / (3*3)	-- Reduction weight of changing constant qualifier in type having 3 qualifiers
local rwd_inheritance = 0.125 / (4*4)	-- Reduction weight of class inheritance
local rwd_c_inheritance = 0.125/(5*5) 	-- Reduction weight of const class inheritance
local rwd_control = 0.125 / (4*4)	-- Reduction weight of boolean type (control true/false type <-> boolean value) conversions

function preferWeight( flag, w1, w2) if flag then return w1 else return w2 end end -- Prefer one weight to the other 
function factorWeight( fac, w1) return fac * w1	end
function combineWeight( w1, w2) return w1 + (w2 * 127.0 / 128.0) end -- Combining two reductions slightly less weight compared with applying both singularily
function combineWeight3( w1, w2, w3) return w1 + (w2 * 127.0 / 128.0) + (w3 * 255.0 / 256.0) end -- Combining three reductions slightly less weight compared with applying them singularily
function scalarDeductionWeight( sizeweight) return 0.125*(1.0-sizeweight) end -- Deduction weight of this element for scalar operators

local weightEpsilon = 1E-8		-- Epsilon used for comparing weights for equality
local scalarQualiTypeMap = {}		-- Map of scalar type names without qualifiers to the table of type ids for all qualifiers possible
local scalarIndexTypeMap = {}		-- Map of scalar type names usable as index without qualifiers to the const type id used as index for [] or pointer arithmetics
local scalarBooleanType = nil		-- Type id of the boolean type, result of cmpop binary operators
local scalarIntegerType = nil		-- Type id of the main integer type, result of main function
local scalarLongType = nil		-- Type id of the main long integer type (64 bits), used for long integer vararg parameters
local scalarFloatType = nil		-- Type id of the main floating point number type (64 bits), used for floating point vararg parameters
local stringPointerType = nil		-- Type id of the string constant type used for free string litterals
local memPointerType = nil		-- Type id of the type used for result of allocmem and argument of freemem
local stringAddressType = nil		-- Type id of the string constant type used string constants outsize a function scope
local anyClassPointerType = nil		-- Type id of the "class^" type
local anyConstClassPointerType = nil 	-- Type id of the "class^" type
local anyStructPointerType = nil	-- Type id of the "struct^" type
local anyConstStructPointerType = nil	-- Type id of the "struct^" type
local anyFreeFunctionType = nil		-- Type id of any free function/procedure callable
local controlTrueType = nil		-- Type implementing a boolean not represented as value but as peace of code (in the constructor) with a falseExit label
local controlFalseType = nil		-- Type implementing a boolean n§ot represented as value but as peace of code (in the constructor) with a trueExit label
local lambdaResultType = nil		-- Type of a result of a lambda expression, can only be used as free expression until now
local qualiTypeMap = {}			-- Map of value type without qualifier to the table of type ids for all qualifiers defined for this type
local referenceTypeMap = {}		-- Map of value types to their reference types
local dereferenceTypeMap = {}		-- Map of reference types to their value type with the reference qualifier (and the private flag) stripped away
local constTypeMap = {}			-- Map of non const types to their const type
local privateTypeMap = {}		-- Map of non private types to their private type
local pointerTypeMap = {}		-- Map of non pointer types to their pointer type
local pointeeTypeMap = {}		-- Map of pointer types to their pointee type
local unboundReferenceTypeMap = {}	-- Map of value types to their unbound reftype type if it exists
local ctorThrowingMap = {}		-- Map that tells if a constructor is throwing or not
local typeHasCtorThrowingMap = {}	-- Map that tells if a type has a throwing constructor (true) or not (false or nil)
local typeQualiSepMap = {}		-- Map of any defined type id to a separation pair (valtype,qualifier) = valtype type, qualifier as booleans
local typeDescriptionMap = {}		-- Map of any defined type id to its llvmir template structure
local constVarargTypeMap = {}		-- Map of const value types to their vararg type (type used if passed as variable size argument list parameter)
local stringConstantMap = {}		-- Map of string constant values to a structure with the attributes {name,size}
local arrayTypeMap = {}			-- Map of the pair valtype,size to the array type valtype for an array size
local genericInstanceTypeMap = {}	-- Map of the pair valtype,generic parameter to the generic instance type valtype for list of arguments
local varargFuncMap = {}		-- Map of types to true for vararg functions/procedures
local instantCallableEnvironment = nil	-- Callable environment created for implicitly generated code (constructors,destructors,assignments,etc.)
local globalCallableEnvironment = nil	-- Callable environment for constructors/destructors of globals
local instantAllocationFrame = nil	-- Allocation frame created for implicitly generated code
local globalAllocationFrame = nil	-- Allocation frame for global variables
local instantUnwindLabel = nil 		-- label for cleanup in case of an exception created for implicitly generated code (constructors,destructors,assignments,etc.)
local environmentStack = {}		-- Stack used for instantCallableEnvironment, instantAllocationFrame, instantUnwindLabel defining the environment for implicitly defined code
local globalInitCode = ""		-- Init code for global variables
local hardcodedTypeMap = {}		-- Map of hardcoded type names to their id
local genericLocalStack = {}		-- Stack of values for localDefinitionContext 
local localDefinitionContext = 0	-- The context type id used for local type definitions, to separate instances of generics using the same scope
local seekContextKey = "seekctx"	-- Current key for the seek context type realm for (name parameter for typedb:set_instance,typedb:this_instance,typedb:get_instance)
local callableEnvKey = "env"		-- Current key for the callable environment for (name parameter for typedb:set_instance,typedb:this_instance,typedb:get_instance)
local allocFrameKey = "frame"		-- Current key for the allocation frames (name parameter for typedb:set_instance,typedb:this_instance,typedb:get_instance)
local exceptionSectionPrinted = false	-- True if the code for throwing exceptions hat been attached to the code

---- Callable Environment Fields ----
--	name		: Name for debugging/loging/tracing
--      scope		: Scope of the callable
--      register	: LLVM register/slot allocator
--	label		: LLVM label allocator
--	returntype	: Return type in case of a function, used by the return statement to derive the value returned
--	returnfunction	: Function implementing the return statement, there are differences between regular functions and the main function
--	frames		: List of allocation frames allocated during this function, used for collecting the exception handling code when printing the function
--	features	: Set of flags indicating what features are used and what initialization code should be generated
--	initstate	: Initialization state counter in case of constructor, otherwise undefined
--	partial_dtor	: Call of partial dtor in case of an exception, used in exception handling code in the case of a constructor, otherwise undefined
--	initcontext	: Context of the class in case of a constructor, otherwise undefined
--	throws		: True if the function is declared as potentially throwing exceptions

-- Create a callable environent object
function createCallableEnvironment( node, name, rtype, throws, rprefix, lprefix)
	return {name=name, line=node.line, scope=typedb:scope(), throws=throws, register=utils.register_allocator(rprefix), label=utils.label_allocator(lprefix),
	        returntype=rtype, returnfunction=nil, frames={}, features=nil, initstate=nil, partial_dtor=nil, initcontext=nil}
end
-- Attach a newly created data structure for a callable to its scope
function defineCallableEnvironment( node, name, rtype, throws)
	local env = createCallableEnvironment( node, name, rtype, throws)
	if typedb:this_instance( callableEnvKey) then utils.errorMessage( node.line, "Internal: Callable environment defined twice: %s %s", name, mewa.tostring({(typedb:scope())})) end
	typedb:set_instance( callableEnvKey, env)
	return env
end
-- Get the active callable instance
function getCallableEnvironment()
	return instantCallableEnvironment or typedb:get_instance( callableEnvKey)
end
-- Type string of a type declaration built from its parts for error messages
function typeDeclarationString( this, typnam, args)
	local rt = (this == 0 or not this) and typnam or (typedb:type_string(type(this) == "table" and this.type or this) .. " " .. typnam)
	if (args) then rt = rt .. "(" .. utils.typeListString( typedb, args) .. ")" end
	return rt
end
---- Constructor Structure Fields ----
--	out		: Output of the constructor, type depends on the constructor class (register for value constructor, unbound label for boolean control type, substitution name for unbound reference type) 
--	code 		: LLVM code of the constructor
--	step 		: scope step of the node creating the type the constructor is associated with, inherited when resolving an unbound reference type or building a structure from a constexpr structure
--	throws		: true if the code of this constructor may throw an exception

-- Get the two parts of a constructor as tuple
function constructorParts( constructor)
	if type(constructor) == "table" then return constructor.out,(constructor.code or "") else return tostring(constructor),"" end
end
-- Get a constructor structure
function constructorStruct( out, code)
	return {out=out, code=code or ""}
end
function constructorStructCall( out, code, step, throws)
	return {out=out, code=code or "", step=step, throws=throws}
end
-- Get an empty constructor structure
function constructorStructEmpty()
	return {code=""}
end
-- Build a key out of a constructor, used for the unique exit path (dtor chain) generation for an exit scenario identified by its constructor (return value, throw exception, etc.)
function constructorKey( prefix, typeId, out)
	return prefix .. string.format(":%x:", typeId) .. out
end
-- Create a type/constructor pair as used by most functions constructing a type
function typeConstructorPairStruct( type, out, code)
	return {type=type, constructor=constructorStruct( out, code)}
end
-- Get the code part of a constructor of a type constructor pair (may be nil if undefined) as string or nil if not defined or empty
function typeConstructorPairCode( tc_pair)
	if tc_pair and tc_pair.constructor and tc_pair.constructor.code and tc_pair.constructor.code ~= "" then return tc_pair.constructor.code end
end
-- Constructor of a single constant value without code
function constConstructor( val)
	return function() return {out=val,code=""} end
end
-- Build the format substitution structure for a call constructor
function buildCallArguments( out, this_inp, args)
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
-- Constructor implementing some sort of manipulation of an object without arguments or output and without exception
function manipConstructor( fmt)
	return function( this)
		local inp,code = constructorParts( this)
		return constructorStruct( inp, code .. utils.constructor_format( fmt, {this=inp}))
	end
end
-- Constructor implementing a constructor of an object by a single argument without exception
function singleArgumentConstructor( fmt)
	return function( this, arg)
		local env = getCallableEnvironment()
		local code = ""
		local this_inp,this_code = constructorParts( this)
		local arg_inp,arg_code = constructorParts( arg[1])
		local subst = {arg1 = tostring(arg_inp), this = this_inp}
		code = code .. this_code .. arg_code .. utils.constructor_format( fmt, subst, env.register)
 		return constructorStruct( this_inp, code)
	end
end
-- Constructor of a string pointer from a string definition
function stringPointerConstructor( stringdef)
	local env = getCallableEnvironment()
	local out = env.register()
	local code = utils.template_format( llvmir.control.stringConstConstructor,{size=stringdef.size,name=stringdef.name,out=out})
	return constructorStruct( out, code)
end
-- Constructor implementing an assignment of a free function callable to a function/procedure pointer
function copyFunctionPointerConstructor( descr)
	local copyFunc = singleArgumentConstructor( descr.assign)
	return function( this, arg)
		local scope_bk,step_bk = typedb:scope( typedb:type_scope( arg[1].type))
		local functype = typedb:get_type( arg[1].type, "()", descr.param)
		local functypeDescr = typeDescriptionMap[ functype]
		typedb:scope( scope_bk,step_bk)
		if functype and descr.throws == functypeDescr.throws then return copyFunc( this, {{out="@" .. functypeDescr.symbolname}}) end
	end
end
-- Binary operator with a conversion of the argument before the call of the operator. Needed for conversion of floating point numbers into a hexadecimal code required by LLVM IR. 
function binopArgConversionConstructor( fmt, argconv)
	return function( this, arg)
		local env = getCallableEnvironment()
		local out = env.register()
		local code = ""
		local this_inp,this_code = constructorParts( this)
		local arg_inp,arg_code = constructorParts( arg[1])
		local subst = {out = out, this = this_inp, arg1 = argconv(arg_inp)}
		code = code .. this_code .. arg_code .. utils.constructor_format( fmt, subst, env.register)
		return constructorStruct( out, code)
	end
end
-- Constructor for commutative binary operation, swapping arguments
function binopSwapConstructor( constructor)
	return function( this, args) return constructor( args[1], {this}) end
end
-- Constructor imlementing a conversion of a data item to another type using a format string that describes the conversion operation
function convConstructor( fmt)
	return function( constructor)
		local env = getCallableEnvironment()
		local out = env.register()
		local inp,code = constructorParts( constructor)
		local convCode = utils.constructor_format( fmt, {this = inp, out = out}, env.register)
		return constructorStruct( out, code .. convCode)
	end
end
-- Constructor imlementing a conversion of a data item to another and unbound reference type using a format string that describes the conversion operation
function convConstructorUnboundReference( fmt)
	return function( constructor)
		local env = getCallableEnvironment()
		local inp,code = constructorParts( constructor)
		local convCode = utils.constructor_format( fmt, {this = inp, out = "{RVAL}"}, env.register)
		local rt = constructorStruct( "RVAL", code .. convCode)
		rt.step = typedb:step()
		return rt
	end
end
-- Constructor implementing some operation with an arbitrary number of arguments selectively addressed without LLVM typeinfo attributes attached
function callConstructor( fmt, has_result)
	return function( this, args)
		local env = getCallableEnvironment()
		local this_inp,this_code = constructorParts( this)
		local out,res
		if has_result then
			out = env.register()
			res = out
		else
			res = this_inp
		end
		local code,subst = buildCallArguments( out, this_inp, args)
		return constructorStructCall( res, this_code .. code .. utils.constructor_format( fmt, subst, env.register), nil, false)
	end
end
-- Call constructor that might throw an exception
function invokeConstructor( fmt, has_result)
	return function( this, args)
		local env = getCallableEnvironment()
		local this_inp,this_code = constructorParts( this)
		local out,res
		if has_result then
			out = env.register()
			res = out
		else
			res = this_inp
		end
		local code,subst = buildCallArguments( out, this_inp, args)
		subst.success = env.label()
		subst.cleanup = getInvokeUnwindLabel()
		return constructorStructCall( res, this_code .. code .. utils.constructor_format( fmt, subst, env.register), nil, true)
	end
end
-- Move constructor of a reference type from an unbound reference type
function unboundReferenceMoveConstructor( this, args)
	local operand = args[1]
	local this_inp,this_code = constructorParts( this)
	local arg_inp,arg_code = constructorParts( operand)
	bindPartialDtor( operand.step, arg_inp, this_inp)
	local code = this_code .. utils.template_format( arg_code, {[arg_inp]=this_inp})
	return {code=code, out=this_inp}
end
-- Constructor of a constant reference value from a unbound reference type, creating a temporary on the stack for it and registering the cleanup function
function constReferenceFromUnboundRefTypeConstructor( descr, refTypeId)
	return function( constructor)
		local env = getCallableEnvironment()
		local out = env.register()
		local inp,code = constructorParts( constructor)
		bindPartialDtor( constructor.step, inp, out)
		code = utils.constructor_format( descr.def_local, {out = out}, env.register) .. utils.template_format( code, {[inp]=out} )
		local cleanup = typeConstructorPairCode( tryApplyCallable( {line=env.line}, {type=refTypeId,constructor={out=out}}, ":~", {}))
		if cleanup then registerCleanupCode( "local " .. typedb:type_name(dereferenceTypeMap[ refTypeId]), cleanup, constructor.step) end
		return {code=code, out=out}
	end
end
-- Function that decides wheter a function (in the LLVM code output) should return the return value as value or via an 'sret' pointer passed as argument
function doReturnValueAsReferenceParameter( typeId)
	if typeId and unboundReferenceTypeMap[ typeId] then return true else return false end
end
-- Function that decides wheter a parameter of an implicitly generated function should be passed by value or by reference
function doPassValueAsReferenceParameter( typeId)
	return doReturnValueAsReferenceParameter( typeId)
end
-- Function that decides wheter an explicit cast to a type creates a reference type or a value type
function doCastToUnboundReferenceType( typeId)
	return doReturnValueAsReferenceParameter( typeId)
end
-- Builds the argument string and the argument build-up code for a function call or interface method call constructors
function buildFunctionCallArguments( code, argstr, args, llvmtypes)
	local rt = (argstr == "") and "" or (argstr .. ", ")
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
-- Constructor of a function/procedure call generalized. Shared part of functionDirectCallConstructor,functionIndirectCallConstructor,interfaceMethodCallConstructor
function functionCallConstructor( env, code, func, argstr, descr)
	local out,fmt,subst
	local doInvoke = descr.throws
	local step = nil
	if not env.throws and descr.throws then
		if hasCatchFrame() then
			env.throws = true
		else
			utils.errorMessage( env.line or 0, "Calling throwing function inside a callable declared as nothrow without a catch handling an exception: %s", func)
		end
	end
	if descr.ret then
		if doReturnValueAsReferenceParameter( descr.ret) then
			step = typedb:step()
			local reftypesubst = (argstr == "") and "{RVAL}" or "{RVAL}, "
			out = "RVAL"
			subst = {func = func, callargstr = argstr, reftyperef = reftypesubst, signature=descr.signature, rtllvmtype=descr.rtllvmtype}
			if descr.throws then
				subst.cleanup = getInvokeUnwindLabel()
				subst.success = env.label()
				fmt = llvmir.control.sretFunctionCallThrowing
			else
				fmt = llvmir.control.sretFunctionCall
			end
		else
			out = env.register()
			subst = {func = func, callargstr = argstr, out = out, signature=descr.signature, rtllvmtype=descr.rtllvmtype}
			if descr.throws then
				subst.cleanup = getInvokeUnwindLabel()
				subst.success = env.label()
				fmt = llvmir.control.functionCallThrowing
			else
				fmt = llvmir.control.functionCall
			end
		end
	else
		subst = {func = func, callargstr = argstr, signature=descr.signature, rtllvmtype=descr.rtllvmtype}
		if descr.throws then
			subst.cleanup = getInvokeUnwindLabel()
			subst.success = env.label()
			fmt = llvmir.control.procedureCallThrowing
		else
			fmt = llvmir.control.procedureCall
		end
	end
	return constructorStructCall( out, code .. utils.constructor_format( fmt, subst, env.register), step, descr.throws)
end
-- Constructor implementing a direct call of a function with an arbitrary number of arguments built as one string with LLVM typeinfo attributes as needed for function calls
function functionDirectCallConstructor( thisTypeId, descr)
	return function( this, args, llvmtypes)
		local env = getCallableEnvironment()
		local this_inp,this_code = constructorParts( this)
		local this_argstr = (thisTypeId ~= 0) and (typeDescriptionMap[ thisTypeId].llvmtype .. " " .. this_inp) or ""
		local code,argstr = buildFunctionCallArguments( this_code, this_argstr, args, llvmtypes)
		return functionCallConstructor( env, code, "@" .. descr.symbolname, argstr, descr)
	end
end
-- Constructor implementing an indirect call of a function through a function/procedure variable
function functionIndirectCallConstructor( thisTypeId, descr)
	return function( this, args, llvmtypes)
		local env = getCallableEnvironment()
		local this_inp,this_code = constructorParts( this)
		local code,argstr = buildFunctionCallArguments( "", "", args, llvmtypes)
		return functionCallConstructor( env, this_code .. code, this_inp, argstr, descr)
	end
end
-- Constructor implementing a call of a method of an interface
function interfaceMethodCallConstructor( descr)
	return function( this, args, llvmtypes)
		local env = getCallableEnvironment()
		local this_inp,this_code = constructorParts( this)
		local intr_func = env.register()
		local intr_this = env.register()
		this_code = this_code .. utils.constructor_format( descr.load, {this=this_inp, index=descr.index, llvmtype=descr.llvmtype, out_func=intr_func, out_this=intr_this}, env.register)
		local this_argstr = "i8* " .. intr_this
		local code,argstr = buildFunctionCallArguments( this_code, this_argstr, args, llvmtypes)
		return functionCallConstructor( env, code, intr_func, argstr, descr)
	end
end
-- Constructor for a memberwise assignment of a tree structure (initializing an "array")
function memberwiseInitArrayConstructor( node, thisTypeId, elementTypeId, nofElements)
	return function( this, args, parentStep)
		if #args > nofElements then
			utils.errorMessage( node.line, "Number of elements %d in init is too big for '%s' [%d]", #args, typedb:type_string( thisTypeId), nofElements)
		end
		local descr = typeDescriptionMap[ thisTypeId]
		local descr_element = typeDescriptionMap[ elementTypeId]
		local elementRefTypeId = referenceTypeMap[ elementTypeId] or elementTypeId
		local env = getCallableEnvironment()
		local this_inp,res_code = constructorParts( this)
		local cntreg,basereg = env.register(),env.register()
		local memberwise_start = utils.constructor_format( descr.memberwise_start, {cnt=cntreg,this=this_inp,base=basereg})
		local memberwise_cleanup = utils.template_format( descr.memberwise_cleanup, {cnt=cntreg,this=this_inp}, env.register)
		local memberwise_final = utils.constructor_format( descr.memberwise_final, {cnt=cntreg,index=#args}, env.register)
		local code = ""
		local element_throws = typeHasCtorThrowingMap[ elementRefTypeId]
		if env ~= globalCallableEnvironment then registerPartialDtor( "array", memberwise_cleanup, parentStep) end -- if the initialization of globals fail we do not go into partial cleanup
		for ai,arg in ipairs(args) do
			local elemreg = env.register()
			local elem = typeConstructorPairStruct( elementRefTypeId, elemreg)
			local init = tryApplyCallable( node, elem, ":=", {arg})
			if not init then utils.errorMessage( node.line, "Failed to find ctor with signature '%s'", typeDeclarationString( elem, ":=", {arg})) end
			local memberwise_next
			if element_throws then
				memberwise_next = utils.constructor_format( descr.memberwise_next, {cnt=cntreg,base=basereg,out=elemreg}, env.register)
			else
				memberwise_next = utils.constructor_format( descr.memberwise_index, {index=ai-1,this=this_inp,out=elemreg}, env.register)
			end
			code = code .. memberwise_next .. typeConstructorPairCode(init)
		end
		if element_throws then code = code .. memberwise_final end
		if #args < nofElements then
			local init = tryApplyCallable( node, typeConstructorPairStruct( elementRefTypeId, "%ths", ""), ":=", {})
			if not init then utils.errorMessage( node.line, "Failed to find ctor with signature '%s'", typeDeclarationString( elem, ":=", {})) end
			local fmtdescr = {element=descr_element.symbol or descr_element.llvmtype, enter=env.label(), begin=env.label(), ["end"]=env.label(), index=#args,
						this=this_inp, ctors=init.constructor.code}
			if init.constructor.throws then
				fmtdescr.success = env.label()
				fmtdescr.cleanup = getInvokeUnwindLabel()
				code = code .. utils.constructor_format( descr.ctor_rest_throwing, fmtdescr, env.register)
			else
				code = code .. utils.constructor_format( descr.ctor_rest, fmtdescr, env.register)
			end
		end
		if element_throws then code = memberwise_start .. code end
		return constructorStructCall( this_inp, code, nil, element_throws)
	end
end
-- Constructor of an array unbound reference type from a constexpr structure type as argument (used for a reduction of a constexpr structure to an unbound reference array type)
function tryConstexprStructureReductionConstructorArray( node, thisTypeId, elementTypeId, nofElements)
	local initconstructor = memberwiseInitArrayConstructor( node, thisTypeId, elementTypeId, nofElements)
	return function( constructor) -- constructor of a constexpr structure type passed is a list of type constructor pairs
		local step_bk = typedb:step( constructor.step)
		local rt = initconstructor( {out="{RVAL}"}, constructor.list, step_bk)
		typedb:step( step_bk)
		if rt then
			rt.out = "RVAL"
			rt.step = constructor.step
		end
		return rt
	end
end
-- Constructor of an unbound reference type from a constexpr structure type as argument (used for a reduction of a constexpr structure to an unbound reference type)
function tryConstexprStructureReductionConstructor( node, thisTypeId)
	local this = {type=referenceTypeMap[ thisTypeId], constructor={out="{RVAL}"}}
	return function( constructor) -- constructor of a constexpr structure type passed is a list of type constructor pairs
		local step_bk = typedb:step( constructor.step)
		local res = tryApplyCallable( node, this, ":=", constructor.list) -- constexpr structure constructor is a list o arguments with type
		typedb:step( step_bk)
		if res then
			res.constructor.out = "RVAL"
			res.constructor.step = constructor.step
			return res.constructor
		end
	end
end
-- Constructor of an operator on a constexpr structure type as argument
function tryConstexprStructureOperatorConstructor( node, thisTypeId, opr)
	return function( this, args)
		if args[1].type ~= constexprStructureType then utils.errorMessage( node.line, "Expected constexpr structure argument instead of: '%s'", typedb:type_string(args[1].type)) end
		local res = tryApplyCallable( node, {type=thisTypeId, constructor=this}, opr, args[1].list) -- constexpr structure constructor is a list o arguments with type
		if res then return res.constructor end
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
	if callType == -1 then utils.errorMessage( node.line, "Duplicate definition '%s'", typeDeclarationString( thisType, opr, argTypes)) end
	if returnType then typedb:def_reduction( returnType, callType, nil, tag_typeDeclaration) end
end
-- Define an operation generalized
function defineCall( returnType, thisType, opr, argTypes, constructor)
	local callType = typedb:def_type( thisType, opr, constructor, argTypes)
	if callType == -1 then utils.errorMessage( 0, "Duplicate definition of call '%s'", typeDeclarationString( thisType, opr, argTypes)) end
	if returnType then typedb:def_reduction( returnType, callType, nil, tag_typeDeclaration) end
	return callType
end
-- Constant expression/value types
local constexprIntegerType = typedb:def_type( 0, "constexpr int")	-- const expression integers implemented as arbitrary precision BCD numbers
local constexprUIntegerType = typedb:def_type( 0, "constexpr uint")	-- const expression unsigned integer implemented as arbitrary precision BCD numbers
local constexprFloatType = typedb:def_type( 0, "constexpr float")	-- const expression floating point numbers implemented as 'double'
local constexprBooleanType = typedb:def_type( 0, "constexpr bool")	-- const expression boolean implemented as Lua boolean
local constexprNullType = typedb:def_type( 0, "constexpr null")		-- const expression null value implemented as nil
local constexprStructureType = typedb:def_type( 0, "constexpr struct")	-- const expression tree structure implemented as a list of type/constructor pairs (envelop for structure recursively resolved)
function isScalarConstExprValueType( initType) return initType >= constexprIntegerType and initType <= constexprNullType end

-- Mapping of some constant expression type naming
hardcodedTypeMap[ "constexpr bool"] = constexprBooleanType
hardcodedTypeMap[ "constexpr uint"] = constexprUIntegerType
hardcodedTypeMap[ "constexpr int"] = constexprIntegerType
hardcodedTypeMap[ "constexpr float"] = constexprFloatType

-- Description records of constant expression types
typeDescriptionMap[ constexprIntegerType] = llvmir.constexprIntegerDescr
typeDescriptionMap[ constexprUIntegerType] = llvmir.constexprUIntegerDescr
typeDescriptionMap[ constexprFloatType] = llvmir.constexprFloatDescr
typeDescriptionMap[ constexprBooleanType] = llvmir.constexprBooleanDescr
typeDescriptionMap[ constexprNullType] = llvmir.constexprNullDescr
typeDescriptionMap[ constexprStructureType] = llvmir.constexprStructDescr
constVarargTypeMap[ constexprIntegerType] = constexprIntegerType
constVarargTypeMap[ constexprUIntegerType] = constexprIntegerType
constVarargTypeMap[ constexprFloatType] = constexprFloatType
constVarargTypeMap[ constexprBooleanType] = constexprIntegerType
constVarargTypeMap[ constexprNullType] = constexprNullType

local bits64 = bcd.bits( 64) -- The example language1 has a maximum integer bit width of 64

-- Create a constexpr node from a lexem in the AST
function createConstexprValue( constexpr_type, lexemvalue)
	if constexpr_type == constexprIntegerType then return bcd.int(lexemvalue)
	elseif constexpr_type == constexprUIntegerType then return bcd.int(lexemvalue)
	elseif constexpr_type == constexprFloatType then return tonumber(lexemvalue)
	elseif constexpr_type == constexprBooleanType then if lexemvalue == "true" then return true else return false end
	end
end
-- List of value constructors from constexpr constructors
function constexprFloatToFloatConstructor( val) return constructorStruct( "0x" .. mewa.llvm_float_tohex( val)) end
function constexprFloatToDoubleConstructor( val) return constructorStruct( "0x" .. mewa.llvm_double_tohex( val)) end
function constexprFloatToIntegerConstructor( val) return constructorStruct( tostring(tointeger(val))) end
function constexprFloatToBooleanConstructor( val) if math.abs(val) < epsilonthen then return constructorStruct( "0") else constructorStruct( "1") end end
function constexprIntegerToFloatConstructor( val) return constructorStruct( "0x" .. mewa.llvm_float_tohex( val:tonumber())) end
function constexprIntegerToDoubleConstructor( val) return constructorStruct( "0x" .. mewa.llvm_double_tohex( val:tonumber())) end
function constexprIntegerToIntegerConstructor( val) return constructorStruct( tostring(val)) end
function constexprIntegerToBooleanConstructor( val) if val == "0" then return constructorStruct( "0") else return constructorStruct( "1") end end
function constexprBooleanToScalarConstructor( val) if val == true then return constructorStruct( "1") else return constructorStruct( "0") end end

-- Define arithmetic operators for a constexpr type
function defineConstExprArithmetics( constExprType)
	defineCall( constExprType, constExprType, "-", {}, function( this, arg) return -this end)
	defineCall( constExprType, constExprType, "+", {constExprType}, function( this, arg) return this + arg[1] end)
	defineCall( constExprType, constExprType, "-", {constExprType}, function( this, arg) return this - arg[1] end)
	defineCall( constExprType, constExprType, "*", {constExprType}, function( this, arg) return this * arg[1] end)
	defineCall( constExprType, constExprType, "/", {constExprType}, function( this, arg) return this / arg[1] end)
	defineCall( constExprType, constExprType, "%", {constExprType}, function( this, arg) return this % arg[1] end)
end
-- Define bitwise operators for a constexpr type
function defineConstExprBitwiseOps( constExprType)
	defineCall( constExprType, constExprType, "~", {}, function( this, arg) return this.bit_not( bits64) end)
	defineCall( constExprType, constExprType, "&", {constExprType}, function( this, arg) return this.bit_and( arg[1], bits64) end)
	defineCall( constExprType, constExprType, "|", {constExprType}, function( this, arg) return this.bit_or( arg[1], bits64) end)
	defineCall( constExprType, constExprType, "^", {constExprType}, function( this, arg) return this.bit_xor( arg[1], bits64) end)
end
-- Define the boolean operators for the constexpr boolean type
function defineConstExprBooleanOps()
	defineCall( constexprBooleanType, constexprBooleanType, "!", {}, function( this, arg) return this ~= true end)
	defineCall( constexprBooleanType, constexprBooleanType, "&&", {constexprBooleanType}, function( this, arg) return this == true and arg[1] == true end)
	defineCall( constexprBooleanType, constexprBooleanType, "||", {constexprBooleanType}, function( this, arg) return this == true or arg[1] == true end)
end
-- Define the type conversions of const expressions to other const expression types
function defineConstExprTypeConversions()
	typedb:def_reduction( constexprBooleanType, constexprIntegerType, function( value) return value ~= "0" end, tag_typeConversion, rdw_bool)
	typedb:def_reduction( constexprBooleanType, constexprFloatType, function( value) return math.abs(value) < math.abs(epsilon) end, tag_typeConversion, rdw_bool)
	typedb:def_reduction( constexprFloatType, constexprIntegerType, function( value) return value:tonumber() end, tag_typeConversion, rdw_float)
	typedb:def_reduction( constexprFloatType, constexprUIntegerType, function( value) return value:tonumber() end, tag_typeConversion, rdw_float)
	typedb:def_reduction( constexprIntegerType, constexprFloatType, function( value) return bcd.int( tostring(value)) end, tag_typeConversion, rdw_float)
	typedb:def_reduction( constexprIntegerType, constexprUIntegerType, function( value) return value end, tag_typeConversion, rdw_sign)
	typedb:def_reduction( constexprUIntegerType, constexprIntegerType, function( value) return value end, tag_typeConversion, rdw_sign)

	local function bool2bcd( value) if value then return bcd.int("1") else return bcd.int("0") end end
	typedb:def_reduction( constexprIntegerType, constexprBooleanType, bool2bcd, tag_typeConversion, rdw_bool)
end
-- Define the operators for const expression arguments
function defineConstExprOperators()
	defineConstExprArithmetics( constexprIntegerType)
	defineConstExprArithmetics( constexprFloatType)
	defineConstExprBitwiseOps( constexprUIntegerType)
	defineCall( constexprIntegerType, constexprUIntegerType, "-", {}, function( this, arg) return -this end)
	defineConstExprBooleanOps()

	for _,opr in ipairs({"+","-","*","/","%"}) do
		definePromoteCall( constexprFloatType, constexprIntegerType, constexprFloatType, opr, {constexprFloatType}, function(this) return this:tonumber() end)
		definePromoteCall( constexprFloatType, constexprUIntegerType, constexprFloatType, opr, {constexprFloatType}, function(this) return this:tonumber() end)
	end
	for _,opr in ipairs({"&","|","^"}) do
		definePromoteCall( constexprUIntegerType, constexprIntegerType, constexprUIntegerType, opr, {constexprUIntegerType}, function(this) return this end)
	end
	defineCall( constexprBooleanType, constexprUIntegerType, "!", {}, function( this, arg) return this ~= "0" end)
	defineCall( constexprBooleanType, constexprIntegerType, "!", {}, function( this, arg) return this ~= "0" end)
end
-- Tells if the argument type is a pointer type (implicitely generated type marked with '^')
function isPointerType( typeId)
	return pointeeTypeMap[ typeId]
end
-- Get the declaration type of a variable or data member of a structure or class
function getDeclarationType( typeId)
	local redulist = typedb:get_reductions( typeId, tagmask_declaration)
	for ri,redu in ipairs( redulist) do if not redu.constructor then return redu.type end end
	return typeId
end
-- Map a type used as variable argument parameter (where no explicit parameter type defined for the callee) to its type used for parameter passing
function getVarargArgumentType( typeId)
	local decltype = getDeclarationType( typeId)
	local qs = typeQualiSepMap[ decltype]
	if qs then decltype = qualiTypeMap[ qs.valtype].c_valtype end
	return constVarargTypeMap[ decltype]
end
-- Get the name used for a non pointer type with qualifiers built into the type name
function getQualifierTypeName( qualifier, typnam, prefix)
	local rt = prefix and (prefix .. " " .. typnam) or typnam
	if qualifier.const == true then rt = "const " .. rt end
	if qualifier.reference == true then rt = rt .. "&" end
	return rt
end
-- Get the name used for a pointer type with qualifiers built into the type name
function getPointerQualifierTypeName( qualifier, typnam)
	local rt = typnam
	if qualifier.const == true then rt = rt .. " const^" else rt = rt .. "^" end
	if qualifier.reference == true then rt = rt .. "&" end
	return rt
end
-- Define all variations of a type alias that can explicitly specified
function defineTypeAlias( node, contextTypeId, typnam, aliasTypeId)
	local qualitype = qualiTypeMap[ aliasTypeId]
	local alias
	if qualitype then
		if isPointerType( qualitype.valtype) then
			alias = typedb:def_type_as( contextTypeId, getPointerQualifierTypeName( {}, typnam), qualitype.valtype)			-- value type | plain typedef
			typedb:def_type_as( contextTypeId, getPointerQualifierTypeName( {const=true}, typnam), qualitype.c_valtype)		-- const value type
			typedb:def_type_as( contextTypeId, getPointerQualifierTypeName( {reference=true}, typnam), qualitype.reftype)		-- reference type
			typedb:def_type_as( contextTypeId, getPointerQualifierTypeName( {const=true,reference=true}, typnam), qualitype.c_reftype) -- const reference
		else
			alias = typedb:def_type_as( contextTypeId, getQualifierTypeName( {}, typnam), qualitype.valtype)			-- value type | plain typedef
			typedb:def_type_as( contextTypeId, getQualifierTypeName( {const=true}, typnam), qualitype.c_valtype)			-- const value type
			typedb:def_type_as( contextTypeId, getQualifierTypeName( {reference=true}, typnam), qualitype.reftype)			-- reference type
			typedb:def_type_as( contextTypeId, getQualifierTypeName( {const=true,reference=true}, typnam), qualitype.c_reftype)	-- const reference type
		end
	else
		alias = typedb:def_type_as( contextTypeId, typnam, aliasTypeId)
	end
	if alias == -1 then utils.errorMessage( 0, "Duplicate definition of alias '%s'", typeDeclarationString( contextTypeId, typnam)) end
end
-- Define a type alias for a type/constructor pair (a value type)
function defineValueAlias( node, contextType, name, type, constructor)
	local alias = typedb:def_type( contextType, name, constructor)
	if alias == -1 then utils.errorMessage( node.line, "Duplicate definition of value type alias '%s' '%s'", typedb:type_string(contextType), name) end
	typedb:def_reduction( type, alias, nil, tag_typeAlias)
end
-- Define all basic types associated with a type name
function defineQualifiedTypeRelations( qualitype, typeDescription)
	local pointerTypeDescription = llvmir.pointerDescr( typeDescription)
	local valtype,c_valtype,reftype,c_reftype = qualitype.valtype,qualitype.c_valtype,qualitype.reftype,qualitype.c_reftype

	typeDescriptionMap[ valtype]    = typeDescription
	typeDescriptionMap[ c_valtype]  = typeDescription
	typeDescriptionMap[ reftype]    = pointerTypeDescription
	typeDescriptionMap[ c_reftype]  = pointerTypeDescription

	referenceTypeMap[ valtype]   = reftype
	referenceTypeMap[ c_valtype] = c_reftype

	dereferenceTypeMap[ reftype]    = valtype
	dereferenceTypeMap[ c_reftype]  = c_valtype

	constTypeMap[ valtype]  = c_valtype
	constTypeMap[ reftype]  = c_reftype

	typeQualiSepMap[ valtype]    = {valtype=valtype,qualifier={const=false,reference=false,private=false}}
	typeQualiSepMap[ c_valtype]  = {valtype=valtype,qualifier={const=true,reference=false,private=false}}
	typeQualiSepMap[ reftype]    = {valtype=valtype,qualifier={const=false,reference=true,private=false}}
	typeQualiSepMap[ c_reftype]  = {valtype=valtype,qualifier={const=true,reference=true,private=false}}

 	typedb:def_reduction( valtype, c_reftype, convConstructor( typeDescription.load), tag_typeDeduction, combineWeight( rdw_strip_r_2,rdw_strip_c_1))
	typedb:def_reduction( c_valtype, valtype, nil, tag_typeDeduction, rdw_strip_c_1)
	typedb:def_reduction( c_reftype, reftype, nil, tag_typeDeduction, rdw_strip_c_1)
end
-- Define the basic data types for non pointer types
function defineQualiTypes( node, contextTypeId, typnam, typeDescription)
	local valtype = typedb:def_type( contextTypeId, getQualifierTypeName( {}, typnam))				-- value type | plain typedef
	local c_valtype = typedb:def_type( contextTypeId, getQualifierTypeName( {const=true}, typnam))			-- const value type
	local reftype = typedb:def_type( contextTypeId, getQualifierTypeName( {reference=true}, typnam))		-- reference type
	local c_reftype = typedb:def_type( contextTypeId, getQualifierTypeName( {const=true,reference=true}, typnam))	-- const reference type
	if valtype == -1 then utils.errorMessage( node.line, "Duplicate definition of type '%s'", typeDeclarationString( contextTypeId, typnam)) end
	local qualitype = {valtype=valtype, c_valtype=c_valtype, reftype=reftype, c_reftype=c_reftype}
	qualiTypeMap[ valtype] = qualitype
	defineQualifiedTypeRelations( qualitype, typeDescription)
	return qualitype
end
-- Define all types related to a pointer to a type and defined all its relations and operations
function definePointerQualiTypes( node, typeId)
	local scope_bk,step_bk = typedb:scope( typedb:type_scope( typeId))
	local typeDescription = typeDescriptionMap[ typeId]
	local pointerTypeDescription = llvmir.pointerDescr( typeDescription)
	local qs = typeQualiSepMap[ typeId]
	if not qs then utils.errorMessage( node.line, "Cannot define pointer of type '%s'", typedb:type_string(typeId)) end
	local qualitype_pointee = qualiTypeMap[ qs.valtype]
	local pointeeTypeId = (qs.qualifier.const == true) and qualitype_pointee.c_valtype or qualitype_pointee.valtype
	local pointeeRefTypeId = referenceTypeMap[ pointeeTypeId]
	local typnam = typedb:type_name( pointeeTypeId)
	local contextTypeId = typedb:type_context( pointeeTypeId)
	local valtype = typedb:def_type( contextTypeId, getPointerQualifierTypeName( {}, typnam))				-- value type | plain typedef
	local c_valtype = typedb:def_type( contextTypeId, getPointerQualifierTypeName( {const=true}, typnam))			-- const value type
	local reftype = typedb:def_type( contextTypeId, getPointerQualifierTypeName( {reference=true}, typnam))			-- reference type
	local c_reftype = typedb:def_type( contextTypeId, getPointerQualifierTypeName( {const=true,reference=true}, typnam))	-- const reference type
	if valtype == -1 then utils.errorMessage( node.line, "Duplicate definition of pointer to type '%s'", typeDeclarationString( contextTypeId, typnam)) end
	local qualitype = {valtype=valtype, c_valtype=c_valtype, reftype=reftype, c_reftype=c_reftype}
	qualiTypeMap[ valtype] = qualitype
	defineQualifiedTypeRelations( qualitype, pointerTypeDescription)

	typedb:def_reduction( valtype, constexprNullType, constConstructor("null"), tag_typeConversion, rdw_conv)
	defineCall( valtype, pointeeRefTypeId, "&", {}, function(this) return this end)
	defineCall( pointeeRefTypeId, valtype, "->", {}, function(this) return this end)

	local dc = (typeDescription.scalar == false and typeDescription.dtor) and manipConstructor( typeDescription.dtor) or constructorStructEmpty
	defineCall( valtype, valtype, " delete", {}, dc)

	defineArrayIndexOperators( pointeeRefTypeId, c_valtype, pointerTypeDescription)
	defineArrayIndexOperators( pointeeRefTypeId, valtype, pointerTypeDescription)
	for operator,operator_fmt in pairs( pointerTypeDescription.cmpop) do
		defineCall( scalarBooleanType, qualitype.c_valtype, operator, {qualitype.c_valtype}, callConstructor( operator_fmt, true))
	end
	pointerTypeMap[ pointeeTypeId] = valtype
	pointeeTypeMap[ valtype] = pointeeTypeId
	pointeeTypeMap[ c_valtype] = pointeeTypeId

	if typeDescription.class == "struct" then -- define the special pointers to any instance
		typedb:def_reduction( anyStructPointerType, valtype, nil, tag_pointerReinterpretCast)
		typedb:def_reduction( anyConstStructPointerType, c_valtype, nil, tag_pointerReinterpretCast)
		typedb:def_reduction( valtype, anyStructPointerType, nil, tag_pointerReinterpretCast)
		typedb:def_reduction( c_valtype, anyConstStructPointerType, nil, tag_pointerReinterpretCast)
	elseif typeDescription.class == "class" then -- define the special pointers to any instance
		typedb:def_reduction( anyClassPointerType, valtype, nil, tag_pointerReinterpretCast)
		typedb:def_reduction( anyConstClassPointerType, c_valtype, nil, tag_pointerReinterpretCast)
		typedb:def_reduction( valtype, anyClassPointerType, nil, tag_pointerReinterpretCast)
		typedb:def_reduction( c_valtype, anyConstClassPointerType, nil, tag_pointerReinterpretCast)
	end
	defineScalarConstructorDestructors( qualitype, pointerTypeDescription)
	typedb:scope( scope_bk,step_bk)
	return qualitype
end
-- Define index operators for pointers and arrays
function defineArrayIndexOperators( resTypeId, arTypeId, arDescr)
	for index_typnam, index_type in pairs(scalarIndexTypeMap) do
		defineCall( resTypeId, arTypeId, "[]", {index_type}, callConstructor( arDescr.index[ index_typnam], true))
	end
	defineCall( resTypeId, arTypeId, "[]", {constexprUIntegerType}, callConstructor( arDescr.index[ scalarIntegerTypnam], true))
	defineCall( resTypeId, arTypeId, "[]", {constexprIntegerType}, callConstructor( arDescr.index[ scalarIntegerTypnam], true))
end
-- Define increment/decrement operations for a pointer type and an index type
function definePointerIncrementDecrement( ptrtype, index_typnam, index_type)
	local descr = typeDescriptionMap[ ptrtype]
	local index_constructor = callConstructor( descr.index[ index_typnam], true) -- Load the pointer of the element at the index given as argument
	defineCall( ptrtype, ptrtype, "+", {index_type}, index_constructor)
	local negop = typedb:get_type( index_type, "-", {})
	if negop then
		local negconstructor = typedb:type_constructor( negop)
		local function neg_index_constructor( this, arg) return index_constructor( this, {negconstructor( arg[1])}) end -- Load the pointer of the element at index given as negative value of the argument
		defineCall( ptrtype, ptrtype, "-", {index_type}, neg_index_constructor)
	end
end
-- Define pointer arithmetics
function definePointerArithmeticOperators( ptrtype)
	for index_typnam, index_type in pairs(scalarIndexTypeMap) do
		definePointerIncrementDecrement( ptrtype, index_typnam, index_type)
	end
	definePointerIncrementDecrement( ptrtype, scalarIntegerTypnam, constexprIntegerType)
	definePointerIncrementDecrement( ptrtype, scalarIntegerTypnam, constexprUIntegerType)
end
-- Define all types related to a pointer to a base type and its const type with all relations and operations (crossed too)
function definePointerQualiTypesCrossed( node, qualitype_pointee)
	local qualitype_valtype_pointer = definePointerQualiTypes( node, qualitype_pointee.valtype)
	local qualitype_cval_pointer = definePointerQualiTypes( node, qualitype_pointee.c_valtype)
	typedb:def_reduction( qualitype_cval_pointer.valtype, qualitype_valtype_pointer.valtype, nil, tag_typeDeduction, rdw_strip_c_3)
	typedb:def_reduction( qualitype_cval_pointer.c_valtype, qualitype_valtype_pointer.c_valtype, nil, tag_typeDeduction, rdw_strip_c_3)
	return qualitype_valtype_pointer,qualitype_cval_pointer
end
-- Define all unbound reference types for type definitions representable by unbound reference (custom defined structure types)
function defineUnboundReferenceTypes( contextTypeId, typnam, typeDescription, qualitype)
	local pointerTypeDescription = llvmir.pointerDescr( typeDescription)

	local unbound_reftype = typedb:def_type( contextTypeId, typnam .. "?&" )			-- unbound reference type
	local c_unbound_reftype = typedb:def_type( contextTypeId, "const " .. typnam .. "?&")		-- constant unbound reference type

	qualitype.unbound_reftype = unbound_reftype
	qualitype.c_unbound_reftype = c_unbound_reftype

	typeDescriptionMap[ unbound_reftype]   = pointerTypeDescription
	typeDescriptionMap[ c_unbound_reftype] = pointerTypeDescription

	constTypeMap[ unbound_reftype] = c_unbound_reftype

	typeQualiSepMap[ unbound_reftype]   = {valtype=valtype,qualifier={reftyperef=true,const=false}}
	typeQualiSepMap[ c_unbound_reftype] = {valtype=valtype,qualifier={reftyperef=true,const=true}}

	unboundReferenceTypeMap[ qualitype.valtype] = unbound_reftype
	unboundReferenceTypeMap[ qualitype.c_valtype] = c_unbound_reftype

	typedb:def_reduction( c_unbound_reftype, unbound_reftype, nil, tag_typeDeduction, rdw_strip_c_3)
	typedb:def_reduction( qualitype.c_reftype, c_unbound_reftype, constReferenceFromUnboundRefTypeConstructor( typeDescription, qualitype.c_reftype), tag_typeDeduction, rdw_strip_u_2)
	typedb:def_reduction( qualitype.c_reftype, unbound_reftype, constReferenceFromUnboundRefTypeConstructor( typeDescription, qualitype.c_reftype), tag_typeDeduction, rdw_strip_u_1)

	defineCall( qualitype.reftype, qualitype.reftype, ":=", {unbound_reftype}, unboundReferenceMoveConstructor)
	defineCall( qualitype.c_reftype, qualitype.c_reftype, ":=", {c_unbound_reftype}, unboundReferenceMoveConstructor)
end
-- Define public/private types for implementing visibility/accessibility
function definePublicPrivate( contextTypeId, typnam, typeDescription, qualitype)
	local pointerTypeDescription = llvmir.pointerDescr( typeDescription)
	local init_reftype = typedb:def_type( contextTypeId, getQualifierTypeName( {const=false,reference=true}, typnam, "init"))
	qualitype.init_reftype = init_reftype
	local priv_reftype = typedb:def_type( contextTypeId, getQualifierTypeName( {const=false,reference=true}, typnam, "private"))
	qualitype.priv_reftype = priv_reftype
	local priv_c_reftype = typedb:def_type( contextTypeId, getQualifierTypeName( {const=true,reference=true}, typnam, "private"))
	qualitype.priv_c_reftype = priv_c_reftype

	typeDescriptionMap[ priv_reftype] = pointerTypeDescription
	typeDescriptionMap[ priv_c_reftype] = pointerTypeDescription
	constTypeMap[ priv_reftype] = priv_c_reftype
	privateTypeMap[ qualitype.reftype] = priv_reftype
	privateTypeMap[ qualitype.c_reftype] = priv_c_reftype
	dereferenceTypeMap[ priv_reftype]    = valtype
	dereferenceTypeMap[ priv_c_reftype]  = c_valtype
	typeQualiSepMap[ init_reftype] = {valtype=qualitype.valtype,qualifier={reference=true,const=false,init=true}}
	typeQualiSepMap[ priv_reftype] = {valtype=qualitype.valtype,qualifier={reference=true,const=false,private=true}}
	typeQualiSepMap[ priv_c_reftype] = {valtype=qualitype.valtype,qualifier={reference=true,const=true,private=true}}

	typedb:def_reduction( priv_reftype, init_reftype, completeCtorInitializationAndReturnThis, tag_typeDeduction, rdw_barrier)
	typedb:def_reduction( priv_c_reftype, priv_reftype, nil, tag_typeDeduction, rdw_strip_c_3)
	typedb:def_reduction( qualitype.reftype, priv_reftype, nil, tag_typeDeduction, rdw_strip_v_2)
	typedb:def_reduction( qualitype.c_reftype, priv_c_reftype, nil, tag_typeDeduction, rdw_strip_v_3)
end
-- Define the constructors/destructors for built-in scalar types
function defineScalarConstructorDestructors( qualitype, descr)
	defineCall( qualitype.reftype, qualitype.reftype, "=",  {qualitype.c_valtype}, singleArgumentConstructor( descr.assign))
	defineCall( qualitype.reftype, qualitype.reftype, ":=", {qualitype.c_valtype}, singleArgumentConstructor( descr.assign))
	defineCall( qualitype.reftype, qualitype.reftype, ":=", {qualitype.c_reftype}, singleArgumentConstructor( descr.ctor_copy))
	defineCall( qualitype.reftype, qualitype.reftype, ":=", {}, manipConstructor( descr.ctor_init))
	defineCall( qualitype.c_reftype, qualitype.c_reftype, ":=", {qualitype.c_valtype}, singleArgumentConstructor( descr.assign))
	defineCall( qualitype.c_reftype, qualitype.c_reftype, ":=", {qualitype.c_reftype}, singleArgumentConstructor( descr.ctor_copy))
	defineCall( qualitype.c_reftype, qualitype.c_reftype, ":=", {}, manipConstructor( descr.ctor_init))
	defineCall( 0, qualitype.reftype, ":~", {}, constructorStructEmpty)
end
-- Define structure assignments as construction in a local buffer, a destructor call of the target followed by a move into the destination place
function getAssignmentsFromConstructors( node, qualitype, descr)
	function assignmentConstructorFromCtor( descr, ctorfunc, throws)
		if throws then
			return function( this, arg)
				local env = getCallableEnvironment()
				local out_copy = env.register()
				local copy_buf = constructorStruct( out_copy, utils.constructor_format( descr.def_local, {out=out_copy}, env.register))
				local copy_res = ctorfunc( copy_buf, args)
				local this_inp,this_code = constructorParts( this)
				local destroy = utils.constructor_format( descr.dtor, {this=this_inp}, env.register)
				local code = code .. this_code .. copy_res.code .. destroy .. utils.constructor_format( descr.assign, {this = this_inp, arg1 = out_copy}, env.register)
				return constructorStruct( this_inp, code)
			end
		else
			return function( this, arg)
				local this_inp,this_code = constructorParts( this)
				local destroy = utils.constructor_format( descr.dtor, {this=this_inp}, env.register)
				local copy_res = ctorfunc( this, args)
				local code = this_code .. destroy .. copy_res.code
				return constructorStructCall( this_inp, code, copy_res.step, copy_res.throws)
			end
		end
	end
	local scope_bk,step_bk = typedb:scope( typedb:type_scope( qualitype.reftype))
	local items = typedb:get_types( qualitype.reftype, ":=")
	for _,item in ipairs(items) do
		defineCall( qualitype.reftype, qualitype.reftype, "=", typedb:type_parameters(item), assignmentConstructorFromCtor( descr, typedb:type_constructor(item), ctorThrowingMap[ item]))
	end
	typedb:scope( scope_bk,step_bk)
end
-- Get the landingpad resume code of a constructor with a fixed list of defined cleanup functions
function getDefaultConstructorCleanupResumeCode( env, codelist, finishLabel)
	local partial_dtor = ""
	for ci=#codelist,1,-1 do 
		local label = codelist[ ci].label
		local code = codelist[ ci].code
		local dtor_label = label .. "_dtor"
		local dtor_next = finishLabel
		local ni = ci
		while ni > 1 do
			ni = ni-1
			cd = codelist[ni].code
			if cd and cd ~= "" then
				dtor_next = codelist[ ni].label .. "_dtor"
				break
			end
		end
		if code then
			partial_dtor = partial_dtor
				.. utils.constructor_format( llvmir.exception.cleanup_start, {landingpad=label}, env.register)
				.. utils.constructor_format( llvmir.control.label, {inp=dtor_label}) .. code
				.. utils.constructor_format( llvmir.control.gotoStatement, {inp=dtor_next})
		else
			partial_dtor = partial_dtor
				.. utils.constructor_format( llvmir.exception.cleanup_start, {landingpad=label}, env.register)
				.. utils.constructor_format( llvmir.control.gotoStatement, {inp=dtor_next})
		end
	end
	partial_dtor = partial_dtor
				.. utils.constructor_format( llvmir.control.plainLabel, {inp=finishLabel}, env.register)
				.. utils.constructor_format( llvmir.exception.cleanup_end, {}, env.register)
	return partial_dtor
end
-- Define ctor calls (const and non const)
function defineCtorCalls( qualitype, parameter, constructorFunc, throws)
	local calltype = defineCall( qualitype.reftype, qualitype.reftype, ":=", parameter, constructorFunc)
	local c_calltype = defineCall( qualitype.c_reftype, qualitype.c_reftype, ":=", parameter, constructorFunc)
	if throws == true then
		ctorThrowingMap[ calltype] = true
		ctorThrowingMap[ c_calltype] = true
		typeHasCtorThrowingMap[ qualitype.reftype] = true
		typeHasCtorThrowingMap[ qualitype.c_reftype] = true
	end
end
-- Define constructors/destructors for implicitly defined array types (when declaring a variable int a[30], then a type int[30] is implicitly declared) 
function defineArrayConstructorDestructors( node, qualitype, arrayDescr, elementTypeId, arraySize)
	local env = createCallableEnvironment( node, "ctor " .. arrayDescr.symbol, nil, true)
	pushEnvironment( env, nil, "cleanup")
	local r_elementTypeId = referenceTypeMap[ elementTypeId]
	local c_elementTypeId = constTypeMap[ elementTypeId]
	if not r_elementTypeId then utils.errorMessage( node.line, "References not allowed in array declarations, use pointer instead") end
	local cr_elementTypeId = constTypeMap[ r_elementTypeId] or r_elementTypeId
	local ths = {type=r_elementTypeId, constructor={out="%ths"}}
	local oth = {type=cr_elementTypeId, constructor={out="%oth"}}
	local init = tryApplyCallable( node, ths, ":=", {})
	local copy = tryApplyCallable( node, ths, ":=", {oth})
	local dtor_code = typeConstructorPairCode( tryApplyCallable( node, ths, ":~", {})) or ""
	if init then
		local attributes,entercode,rewind,constructorFunc
		if init.constructor.throws == true then
			attributes = llvmir.functionAttribute( false, true)
			entercode = llvmir.exception.allocLandingpad
			rewind = getDefaultConstructorCleanupResumeCode( env, {{label="cleanup", code=arrayDescr.rewind_ctor}}, "finish")
			constructorFunc = invokeConstructor( arrayDescr.ctor_init_throwing)
		else
			attributes = llvmir.functionAttribute( false, false)
			entercode,rewind = "",""
			constructorFunc = callConstructor( arrayDescr.ctor_init, false)
		end
		print_section( "Auto", utils.template_format( arrayDescr.ctorproc_init, {ctors=init.constructor.code, rewind=rewind, entercode=entercode, attributes=attributes}))
		defineCtorCalls( qualitype, {}, constructorFunc, init.constructor.throws)
	end
	if copy then
		local attributes,entercode,rewind,constructorFunc
		if copy.constructor.throws == true then
			attributes = llvmir.functionAttribute( false, true)
			entercode = llvmir.exception.allocLandingpad
			rewind = getDefaultConstructorCleanupResumeCode( env, {{label="cleanup", code=arrayDescr.rewind_ctor}}, "finish")
			constructorFunc = invokeConstructor( arrayDescr.ctor_copy_throwing)
		else
			attributes = llvmir.functionAttribute( false, false)
			entercode,rewind = "",""
			constructorFunc = callConstructor( arrayDescr.ctor_copy)
		end
		print_section( "Auto", utils.template_format( arrayDescr.ctorproc_copy, {ctors=copy.constructor.code, rewind=rewind, entercode=entercode, attributes=attributes}))
		defineCtorCalls( qualitype, {qualitype.c_reftype}, constructorFunc, copy.constructor.throws)
	end
	print_section( "Auto", utils.template_format( arrayDescr.dtorproc, {dtors=dtor_code}))
	defineCall( 0, qualitype.reftype, ":~", {}, manipConstructor( arrayDescr.dtor))

	local redu_constructor_l = tryConstexprStructureReductionConstructorArray( node, qualitype.valtype, elementTypeId, arraySize)
	local redu_constructor_c = tryConstexprStructureReductionConstructorArray( node, qualitype.c_valtype, elementTypeId, arraySize)
	if redu_constructor_l then typedb:def_reduction( unboundReferenceTypeMap[ qualitype.valtype], constexprStructureType, redu_constructor_l, tag_typeConversion, rdw_conv) end
	if redu_constructor_c then typedb:def_reduction( unboundReferenceTypeMap[ qualitype.c_valtype], constexprStructureType, redu_constructor_c, tag_typeConversion, rdw_conv) end
	popEnvironment()
end
-- Define initialization from an initializer structure
function defineInitializationFromStructure( node, qualitype)
	local redu_constructor_l = tryConstexprStructureReductionConstructor( node, qualitype.valtype)
	local redu_constructor_c = tryConstexprStructureReductionConstructor( node, qualitype.c_valtype)
	if redu_constructor_l then typedb:def_reduction( unboundReferenceTypeMap[ qualitype.valtype], constexprStructureType, redu_constructor_l, tag_typeConversion, rdw_conv) end
	if redu_constructor_c then typedb:def_reduction( unboundReferenceTypeMap[ qualitype.c_valtype], constexprStructureType, redu_constructor_c, tag_typeConversion, rdw_conv) end
end
-- Get the code of a class/struct destructor that calls the member destructors, for struct it is the complete constructor, for classes the final part
function getStructMemberDestructorCode( env, descr, members)
	local rt = ""
	for mi,member in ipairs(descr.members) do
		local out = env.register()
		local llvmtype = member.descr.llvmtype
		local member_reftype = referenceTypeMap[ member.type]
		local ths = {type=member_reftype,constructor={code=utils.constructor_format(descr.loadelemref,{out=out,this="%ths",index=mi-1, type=llvmtype}),out=out}}
		local member_destroy_code = member.descr.dtor and utils.constructor_format( member.descr.dtor, {this=out}, env.register) or ""
		if member_destroy_code ~= "" then rt = ths.constructor.code .. member_destroy_code .. rt end
	end
	return rt
end
-- Get a partial destructor used in class constructors for rewinding object construction in case of an exception
function getClassMemberPartialDestructorCode( env, descr, members)
	local rt = ""
	for mi,member in ipairs(descr.members) do
		local out = env.register()
		local llvmtype = member.descr.llvmtype
		local member_reftype = referenceTypeMap[ member.type]
		local ths = {type=member_reftype,constructor={code=utils.constructor_format(descr.loadelemref,{out=out,this="%ths",index=mi-1, type=llvmtype}),out=out}}
		if member.descr.dtor then
			local member_destroy_code = ths.constructor.code .. utils.constructor_format( member.descr.dtor, {this=out}, env.register)
			rt = utils.constructor_format( descr.partial_dtorelem, {creg=env.register(), istate=mi, dtor=member_destroy_code}, env.label) .. rt
		end
	end
	return rt
end
-- Define implicit destructors for 'struct'/'class' types
function defineStructDestructors( node, qualitype, descr)
	instantCallableEnvironment = createCallableEnvironment( node, "dtor " .. descr.symbol, nil, false)
	local dtors = getStructMemberDestructorCode( instantCallableEnvironment, descr, members)
	print_section( "Auto", utils.template_format( descr.dtorproc, {dtors=dtors}))
	defineCall( 0, qualitype.reftype, ":~", {}, manipConstructor( descr.dtor))
	defineCall( 0, qualitype.c_reftype, ":~", {}, manipConstructor( descr.dtor))
	instantCallableEnvironment = nil
end
-- Print the code of the partial destructor for a 'class' type
function definePartialClassDestructor( node, qualitype, descr)
	instantCallableEnvironment = createCallableEnvironment( node, "dtor " .. descr.symbol, nil, false)
	local partial_dtors = getClassMemberPartialDestructorCode( instantCallableEnvironment, descr, members)
	print_section( "Auto", utils.template_format( descr.partial_dtorproc, {dtors=partial_dtors}))
	instantCallableEnvironment = nil
end
-- Define implicit constructors for 'struct'/'class' types
function defineStructConstructors( node, qualitype, descr)
	local env = createCallableEnvironment( node, "ctor " .. descr.symbol, nil, true)
	local ctors_init,ctors_copy,ctors_elements = "","",""
	local paramstr,argstr,elements = "","",{}
	local rewindlist = {{label="cleanup_0"}}
	local init_nofThrows,copy_nofThrows,element_nofThrows = 0,0,0
	pushEnvironment( env, nil, "cleanup_0")

	for mi,member in ipairs(descr.members) do
		local out = env.register()
		local inp = env.register()
		local llvmtype = member.descr.llvmtype
		local c_member_type = constTypeMap[ member.type] or member.type
		local member_reftype = referenceTypeMap[ member.type]
		local c_member_reftype = constTypeMap[ member_reftype] or member_reftype
		local etype = doPassValueAsReferenceParameter( member.type) and c_member_reftype or c_member_type
		table.insert( elements, etype)
		local paramDescr = typeDescriptionMap[ etype]
		paramstr = paramstr .. ", " .. paramDescr.llvmtype .. " %p" .. mi
		argstr = argstr .. ", " .. paramDescr.llvmtype .. " {arg" .. mi .. "}"

		local loadref = descr.loadelemref
		local ths = {type=member_reftype,constructor={code=utils.constructor_format(loadref,{out=out,this="%ths",index=mi-1, type=llvmtype}),out=out}}
		local oth = {type=c_member_reftype,constructor={code=utils.constructor_format(loadref,{out=inp,this="%oth",index=mi-1, type=llvmtype}),out=inp}}

		local param = {type=etype,constructor={out="%p" .. mi}}
		local member_init = tryApplyCallable( node, ths, ":=", {})
		if member_init and member_init.constructor.throws then init_nofThrows = init_nofThrows + 1 end
		local member_copy = tryApplyCallable( node, ths, ":=", {oth})
		if member_copy and member_copy.constructor.throws then copy_nofThrows = copy_nofThrows + 1 end
		local member_element = tryApplyCallable( node, ths, ":=", {param})
		if member_element and member_element.constructor.throws then element_nofThrows = element_nofThrows + 1 end
		local member_destroy_code = member.descr.dtor and utils.constructor_format( member.descr.dtor, {this=out}, env.register) or ""

		instantUnwindLabel = "cleanup_" .. mi
		if member_destroy_code ~= "" and mi ~= #descr.members then table.insert( rewindlist,{label=instantUnwindLabel, code=member_destroy_code}) end
		if member_init and ctors_init then ctors_init = ctors_init .. member_init.constructor.code else ctors_init = nil end
		if member_copy and ctors_copy then ctors_copy = ctors_copy .. member_copy.constructor.code else ctors_copy = nil end 
		if member_element and ctors_elements then ctors_elements = ctors_elements .. member_element.constructor.code else ctors_elements = nil end
	end
	if ctors_init then
		local attributes,entercode,rewind,constructorFunc
		if init_nofThrows > 0 then
			attributes = llvmir.functionAttribute( false, true)
			entercode = llvmir.exception.allocLandingpad
			rewind = getDefaultConstructorCleanupResumeCode( env, rewindlist, "finish")
			constructorFunc = invokeConstructor( descr.ctor_init_throwing)
		else
			attributes = llvmir.functionAttribute( false, false)
			entercode,rewind = "",""
			constructorFunc = callConstructor( descr.ctor_init)
		end
		print_section( "Auto", utils.template_format( descr.ctorproc_init, {ctors=ctors_init, rewind=rewind, entercode=entercode, attributes=attributes}))
		defineCtorCalls( qualitype, {}, constructorFunc, init_nofThrows > 0)
	end
	if ctors_copy then
		local attributes,entercode,rewind,constructorFunc
		if copy_nofThrows > 0 then
			attributes = llvmir.functionAttribute( false, true)
			entercode = llvmir.exception.allocLandingpad
			rewind = getDefaultConstructorCleanupResumeCode( env, rewindlist, "finish")
			constructorFunc = invokeConstructor( descr.ctor_copy_throwing)
		else
			attributes = llvmir.functionAttribute( false, false)
			entercode,rewind = "",""
			constructorFunc = callConstructor( descr.ctor_copy)
		end
		print_section( "Auto", utils.template_format( descr.ctorproc_copy, {ctors=ctors_copy, rewind=rewind, entercode=entercode, attributes=attributes}))
		defineCtorCalls( qualitype, {qualitype.c_reftype}, constructorFunc, copy_nofThrows > 0)
	end
	if ctors_elements then
		local attributes,entercode,rewind,constructorFunc
		if element_nofThrows > 0 then
			attributes = llvmir.functionAttribute( false, true)
			entercode = llvmir.exception.allocLandingpad
			rewind = getDefaultConstructorCleanupResumeCode( env, rewindlist, "finish")
			constructorFunc = invokeConstructor( utils.template_format( descr.ctor_elements_throwing, {args=argstr}))
			
		else
			attributes = llvmir.functionAttribute( false, false)
			entercode,rewind = "",""
			constructorFunc = callConstructor( utils.template_format( descr.ctor_elements, {args=argstr}))
		end
		print_section( "Auto", utils.template_format( descr.ctorproc_elements, {ctors=ctors_elements, paramstr=paramstr, rewind=rewind, entercode=entercode, attributes=attributes}))
		defineCtorCalls( qualitype, elements, constructorFunc, element_nofThrows > 0)
	end
	popEnvironment()
end
-- Define constructors/destructors for 'interface' types
function defineInterfaceConstructorDestructors( node, qualitype, descr, context)
	defineCall( qualitype.c_reftype, qualitype.c_reftype, ":=", {}, manipConstructor( descr.ctor_init))
	defineCall( qualitype.reftype, qualitype.reftype, ":=", {}, manipConstructor( descr.ctor_init))
	defineCall( qualitype.c_reftype, qualitype.c_reftype, ":=", {qualitype.reftype}, singleArgumentConstructor( descr.ctor_copy))
	defineCall( qualitype.reftype, qualitype.reftype, ":=", {qualitype.reftype}, singleArgumentConstructor( descr.ctor_copy))
	defineCall( qualitype.c_reftype, qualitype.c_reftype, ":=", {qualitype.c_valtype}, singleArgumentConstructor( descr.assign))
	defineCall( qualitype.reftype, qualitype.reftype, ":=", {qualitype.c_valtype}, singleArgumentConstructor( descr.assign))
	defineCall( qualitype.reftype, qualitype.reftype, "=", {qualitype.reftype}, singleArgumentConstructor( descr.ctor_copy))
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
				if cmethod.throws ~= imethod.throws then
					utils.errorMessage( node.line, "Exception behaviour (attribute nothrow) of method '%s' of class '%s' differs from interface '%s': '%s'",
					                    cmethod.methodname, typedb:type_name(classTypeId),
					                    typedb:type_name(iTypeId), typedb:type_string( imethod.ret or 0))
				end
				if cmethod.ret ~= imethod.ret then
					utils.errorMessage( node.line, "Return type '%s' of method '%s' of class '%s' differs from interface '%s': '%s'",
					                    typedb:type_string( cmethod.ret or 0), cmethod.methodname, typedb:type_name(classTypeId),
					                    typedb:type_name(iTypeId), typedb:type_string( imethod.ret or 0))
				end
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
-- Get the type of the 'self' reference in the body of a method
function getFunctionThisType( private, const, thisType)
	if private == true then thisType = privateTypeMap[ thisType] end
	if const == true then thisType = constTypeMap[ thisType] end
	return thisType
end
-- Define the attributes assigned to an operator, collected for the decision if to implement it with a constexpr structure as argument (e.g. a + {3,24, 5.13})
function defineOperatorAttributes( context, descr)
	local thisType = getFunctionThisType( descr.private, descr.const, context.qualitype.reftype)
	local def = context.operators[ descr.name]
	if def then
		if #descr.param > def.maxNofArguments then def.maxNofArguments = #descr.param end
		if def.thisType ~= thisType or def.returnType ~= descr.ret then def.hasStructArgument = false end
	else
		context.operators[ descr.name] = {thisType = thisType, returnType = descr.ret, hasStructArgument = true, maxNofArguments = #descr.param}
	end
end
-- Iterate through operator declarations and implement them with a constexpr structure as argument (e.g. a + {3,24, 5.13}) if possible
function defineOperatorsWithStructArgument( node, context)
	for opr,def in pairs( context.operators) do
		if def.hasStructArgument == true then
			local constructor = tryConstexprStructureOperatorConstructor( node, def.thisType, opr)
			local rtval = doReturnValueAsReferenceParameter( def.returnType) and unboundReferenceTypeMap[ def.returnType] or def.returnType
			defineCall( rtval, def.thisType, opr, {constexprStructureType}, constructor)
		end
	end
end
-- Define built-in type conversions for first class citizen scalar types
function defineBuiltInTypeConversions( typnam, descr)
	local qualitype = scalarQualiTypeMap[ typnam]
	if descr.conv then
		for oth_typnam,conv in pairs( descr.conv) do
			local oth_qualitype = scalarQualiTypeMap[ oth_typnam]
			local conv_constructor = conv.fmt and convConstructor( conv.fmt) or nil
			typedb:def_reduction( qualitype.valtype, oth_qualitype.c_valtype, conv_constructor, tag_typeConversion, combineWeight( rdw_conv, conv.weight))
		end
	end
end
-- Define built-in promote calls for first class citizen scalar types
function defineBuiltInTypePromoteCalls( typnam, descr)
	local qualitype = scalarQualiTypeMap[ typnam]
	for i,promote_typnam in ipairs( descr.promote) do
		local promote_qualitype = scalarQualiTypeMap[ promote_typnam]
		local promote_descr = typeDescriptionMap[ promote_qualitype.valtype]
		local promote_conv_fmt = promote_descr.conv[ typnam].fmt
		local promote_conv = promote_conv_fmt and convConstructor( promote_conv_fmt) or nil
		if promote_descr.binop then
			for operator,operator_fmt in pairs( promote_descr.binop) do
				definePromoteCall( promote_qualitype.valtype, qualitype.c_valtype, promote_qualitype.c_valtype, operator, {promote_qualitype.c_valtype}, promote_conv)
			end
		end
		if promote_descr.cmpop then
			for operator,operator_fmt in pairs( promote_descr.cmpop) do
				definePromoteCall( scalarBooleanType, qualitype.c_valtype, promote_qualitype.c_valtype, operator, {promote_qualitype.c_valtype}, promote_conv)
			end
		end
	end
end
-- Define the promote calls of a const expression for a binary scalar operator
function defineBinopConstexprPromoteCalls( resultType, operator, argtype, descr)
	if descr.llvmtype == "double" then
		definePromoteCall( resultType, constexprFloatType, argtype, operator, {argtype}, constexprFloatToDoubleConstructor)
		definePromoteCall( resultType, constexprIntegerType, argtype, operator, {argtype}, constexprIntegerToDoubleConstructor)
		definePromoteCall( resultType, constexprUIntegerType, argtype, operator, {argtype}, constexprIntegerToDoubleConstructor)
		definePromoteCall( resultType, constexprBooleanType, argtype, operator, {argtype}, constexprBooleanToScalarConstructor)
	elseif descr.llvmtype == "float" then
		definePromoteCall( resultType, constexprFloatType, argtype, operator, {argtype}, constexprFloatToFloatConstructor)
		definePromoteCall( resultType, constexprIntegerType, argtype, operator, {argtype}, constexprIntegerToFloatConstructor)
		definePromoteCall( resultType, constexprUIntegerType, argtype, operator, {argtype}, constexprIntegerToFloatConstructor)
		definePromoteCall( resultType, constexprBooleanType, argtype, operator, {argtype}, constexprBooleanToScalarConstructor)
	elseif descr.class == "bool" then
		definePromoteCall( resultType, constexprBooleanType, argtype, operator, {argtype}, constexprBooleanToScalarConstructor)
	elseif descr.class == "signed" then
		definePromoteCall( resultType, constexprIntegerType, argtype, operator, {argtype}, constexprIntegerToIntegerConstructor)
		definePromoteCall( resultType, constexprUIntegerType, argtype, operator, {argtype}, constexprIntegerToIntegerConstructor)
		definePromoteCall( resultType, constexprBooleanType, argtype, operator, {argtype}, constexprBooleanToScalarConstructor)
	elseif descr.class == "unsigned" then
		definePromoteCall( resultType, constexprUIntegerType, argtype, operator, {argtype}, constexprIntegerToIntegerConstructor)
	end
end
function defineBinopConstexprArgumentConversionCalls( resultType, operator, argtype, descr, call_constructor)
	if descr.llvmtype == "double" then
		defineCall( resultType, argtype, operator, {constexprFloatType}, function(this,arg) return call_constructor(this,{constexprFloatToDoubleConstructor(arg[1])}) end)
		defineCall( resultType, argtype, operator, {constexprIntegerType}, function(this,arg) return call_constructor(this,{constexprIntegerToDoubleConstructor(arg[1])}) end)
		defineCall( resultType, argtype, operator, {constexprUIntegerType}, function(this,arg) return call_constructor(this,{constexprIntegerToDoubleConstructor(arg[1])}) end)
		defineCall( resultType, argtype, operator, {constexprBooleanType}, function(this,arg) return call_constructor(this,{constexprBooleanToScalarConstructor(arg[1])}) end)
	elseif descr.llvmtype == "float" then
		defineCall( resultType, argtype, operator, {constexprFloatType}, function(this,arg) return call_constructor(this,{constexprFloatToFloatConstructor(arg[1])}) end)
		defineCall( resultType, argtype, operator, {constexprIntegerType}, function(this,arg) return call_constructor(this,{constexprIntegerToFloatConstructor(arg[1])}) end)
		defineCall( resultType, argtype, operator, {constexprUIntegerType}, function(this,arg) return call_constructor(this,{constexprIntegerToFloatConstructor(arg[1])}) end)
		defineCall( resultType, argtype, operator, {constexprBooleanType}, function(this,arg) return call_constructor(this,{constexprBooleanToScalarConstructor(arg[1])}) end)
	elseif descr.class == "bool" then
		defineCall( resultType, argtype, operator, {constexprFloatType}, function(this,arg) return call_constructor(this,{constexprFloatToBooleanConstructor(arg[1])}) end)
		defineCall( resultType, argtype, operator, {constexprIntegerType}, function(this,arg) return call_constructor(this,{constexprIntegerToBooleanConstructor(arg[1])}) end)
		defineCall( resultType, argtype, operator, {constexprUIntegerType}, function(this,arg) return call_constructor(this,{constexprIntegerToBooleanConstructor(arg[1])}) end)
		defineCall( resultType, argtype, operator, {constexprBooleanType}, function(this,arg) return call_constructor(this,{constexprBooleanToScalarConstructor(arg[1])}) end)
	elseif descr.class == "signed" or descr.class == "unsigned" then
		defineCall( resultType, argtype, operator, {constexprFloatType}, function(this,arg) return call_constructor(this,{constexprFloatToIntegerConstructor(arg[1])}) end)
		defineCall( resultType, argtype, operator, {constexprIntegerType}, function(this,arg) return call_constructor(this,{constexprIntegerToIntegerConstructor(arg[1])}) end)
		defineCall( resultType, argtype, operator, {constexprUIntegerType}, function(this,arg) return call_constructor(this,{constexprIntegerToIntegerConstructor(arg[1])}) end)
		defineCall( resultType, argtype, operator, {constexprBooleanType}, function(this,arg) return call_constructor(this,{constexprBooleanToScalarConstructor(arg[1])}) end)
	end
end
-- Define built-in unary,binary operators for first class citizen scalar types
function defineBuiltInTypeOperators( typnam, descr)
	local qualitype = scalarQualiTypeMap[ typnam]
	local c_valtype = qualitype.c_valtype
	local valtype = qualitype.valtype
	if descr.unop then
		for operator,operator_fmt in pairs( descr.unop) do
			defineCall( valtype, c_valtype, operator, {}, callConstructor( operator_fmt, true))
		end
	end
	if descr.binop then
		for operator,operator_fmt in pairs( descr.binop) do
			local call_constructor = callConstructor( operator_fmt, true)
			defineCall( valtype, c_valtype, operator, {c_valtype}, call_constructor)
			defineBinopConstexprPromoteCalls( valtype, operator, c_valtype, descr)
			defineBinopConstexprArgumentConversionCalls( valtype, operator, c_valtype, descr, call_constructor)
		end
	end
	if descr.cmpop then
		for operator,operator_fmt in pairs( descr.cmpop) do
			local call_constructor = callConstructor( operator_fmt, true)
			defineCall( scalarBooleanType, c_valtype, operator, {c_valtype}, call_constructor)
			defineBinopConstexprPromoteCalls( scalarBooleanType, operator, c_valtype, descr)
			defineBinopConstexprArgumentConversionCalls( scalarBooleanType, operator, c_valtype, descr, call_constructor)
		end
	end
end
-- Get an object instance and clone it if it is not stored in the current scope, making it possible to add elements to an inherited instance in the current scope
function thisInstanceTableClone( name, emptyInst)
	local inst = typedb:this_instance( name)
	if not inst then
		inst = utils.deepCopyStruct( typedb:get_instance( name) or emptyInst)
		typedb:set_instance( name, inst)
	end
	return inst
end
-- Get the list of context types associated with the current scope used for resolving types
function getSeekContextTypes()
	return typedb:get_instance( seekContextKey) or {0}
end
-- Update the list of context types associated with the current scope used for resolving types
function setSeekContextTypes( types)
	typedb:set_instance( seekContextKey, utils.deepCopyStruct( types))
end
-- Push an element to the current context type list used for resolving types
function pushSeekContextType( val)
	table.insert( thisInstanceTableClone( seekContextKey, {0}), val)
end
-- Remove the last element of the the list of context types associated with the current scope used for resolving types by one type/constructor pair structure
function popSeekContextType( val)
	local seekctx = typedb:this_instance( seekContextKey)
	if not seekctx or seekctx[ #seekctx] ~= val then utils.errorMessage( 0, "Internal: corrupt definition context stack") end
	table.remove( seekctx, #seekctx)
end
-- Push an element to the generic local stack, allowing nested generic creation
function pushGenericLocal( genericlocal)
	table.insert( genericLocalStack, {localDefinitionContext,seekContextKey,callableEnvKey,allocFrameKey})
	localDefinitionContext = genericlocal
	local suffix = string.format(":%x", genericlocal)
	seekContextKey = "seekctx" .. suffix
	callableEnvKey = "env" .. suffix
	allocFrameKey = "frame" .. suffix
end
-- Pop the top element from the generic local stack, restoring the previous localDefinitionContext value
function popGenericLocal( genericlocal)
	if localDefinitionContext ~= genericlocal or #genericLocalStack == 0 then utils.errorMessage( 0, "Internal: corrupt generic local stack") end
	localDefinitionContext,seekContextKey,callableEnvKey,allocFrameKey = table.unpack(genericLocalStack[ #genericLocalStack])
	table.remove( genericLocalStack, #genericLocalStack)
end
-- Temporarily change environment for implicitely defined code
function pushEnvironment( env, frame, cleanup)
	table.insert( environmentStack, {instantCallableEnvironment,instantAllocationFrame,instantUnwindLabel} )
	instantCallableEnvironment = env
	instantAllocationFrame = frame
	instantUnwindLabel = cleanup
end
-- Restore temporarily changed environment for implicitely defined code
function popEnvironment()
	if #environmentStack == 0 then utils.errorMessage( 0, "Internal: corrupt environment stack") end
	local glb = table.remove( environmentStack, #environmentStack)
	instantCallableEnvironment = glb[1]
	instantAllocationFrame = glb[2]
	instantUnwindLabel = glb[3]	
end
---- Allocation Frame Fields ----
--	parent		: Parent allocation frame, the allocation frame in which this allocation frame has been created (in the same callable environment)
--	catch		: Allocation frame of the exception handler in the same callable environment that handles the exceptions thrown or forwarded by this allocation frame
--	env		: The callable environment this allocation frame belongs to
--	scope		: Scope of this allocation frame
-- 	partial_dtors	: Map scope step to a partial dtor for rewinding the construction of a partially build object. The partial dtor is called on a invoke unwind label. The partial dtor has a field start, the label where to jump for calling the partial dtor and a field continue, where the execution continues after the execution of the partial dtor. The partial dtor is registered with setPartialDtor.
--	dtors		: List of cleanup code entries, one for every object with a destructor, used to build the cleanup code sequences for different exit scenarios
--	exitmap		: Maps different named exit scenarios (exceptions thrown, return from a function with a specific value to a cleanup code sequence)
--	exitkeys	: List of all keys for the output in definition order (deterministic) of the exitmap entries
--	landingpad	: Landingpad table {code = code with all landingpads defined in this frame, map = maps cleanup labels to landingpad labels}
--	initstate	: Initialization state counter. This counter is used as second argument for the partial destructor called if a constructor fails.

-- Create the allocation frame for a specific environment and scope, used for createing the allocation frame for globals
function createAllocationFrame( env, scope)
	return {parent=nil, catch=nil, env=env, scope=scope, partial_dtors={}, dtors={}, exitmap={}, exitkeys={}, landingpad=nil, initstate=nil}
end
-- Create the allocation frame of this scope with the parent passed as argument
function createThisAllocationFrame( parent)
	local env = parent and parent.env or getCallableEnvironment()
	local catch,initstate
	if not parent or parent.env ~= env then
		parent = nil
		initstate = env.initstate
	else
		catch = parent.catch
		initstate = parent.initstate
	end
	local rt = {parent=parent, catch=catch, env=env, scope=typedb:scope(), partial_dtors={}, dtors={}, exitmap={}, exitkeys={}, landingpad=nil, initstate=initstate}
	table.insert( env.frames, rt)
	typedb:set_instance( allocFrameKey, rt)
	return rt
end
-- Get or create the allocation frame of this scope
function getOrCreateThisAllocationFrame()
	return instantAllocationFrame or typedb:this_instance( allocFrameKey) or createThisAllocationFrame( typedb:get_instance( allocFrameKey))
end
-- Get the allocation frame covering this scope
function getAllocationFrame()
	return instantAllocationFrame or typedb:get_instance( allocFrameKey)
end
-- Get the allocation frame of this scope if it exists
function thisAllocationFrame()
	return instantAllocationFrame or typedb:this_instance( allocFrameKey)
end
-- Get or create the cleanup slot for a scope step in a given allocation frame, must be called in ascending order, return the index of the dtor found or created
function getOrCreateAllocationFrameCleanupSlot( frame, step)
	local dtors = frame.dtors
	local di = #dtors
	while di > 0 and dtors[di].step > step do di = di - 1 end
	if di > 0 and dtors[di].step == step then
		return dtors[ di]
	else
		table.insert( dtors, di+1, {step=step})
		return dtors[ di+1]
	end
end
-- Get the cleanup slot for a scope step if it exists
function getAllocationFrameCleanupSlot( frame, step)
	local dtors = frame.dtors
	local di = #dtors
	while di > 0 and dtors[di].step > step do di = di - 1 end
	if di > 0 and dtors[di].step == step then return dtors[di] end
end
-- Register the cleanup code for the current scope step, must be called in ascending order
function registerCleanupCode( name, code, step)
	if not step then step = typedb:step() end
	local frame = getOrCreateThisAllocationFrame()
	local dtor = getOrCreateAllocationFrameCleanupSlot( frame, step)
	if dtor.cleanup then utils.errorMessage( frame.env.line, "Internal: More than one cleanup registered per scope step (%d): %s and %s", step, name, dtor.cleanup.name) end
	dtor.cleanup = {name=name,code=code}
end
-- Register the partial dtor for the current scope step, must be called in ascending order
function registerPartialDtor( name, code, parent_step)
	local step = typedb:step()
	local frame = getOrCreateThisAllocationFrame()
	local dtor = getOrCreateAllocationFrameCleanupSlot( frame, step)
	if dtor.partial then utils.errorMessage( frame.env.line, "Internal: More than one partial dtor registered per scope step (%d): %s and %s", step, name, dtor.partial.name) end
	dtor.partial = {name=name,code=code,parent_step=parent_step}
end
-- Substitute the target in the cleanup code of an unbound reference type if there is a partial cleanup registered
function bindPartialDtor( step, var, value)
	local frame = getAllocationFrame()
	if frame then
		local dtor = getAllocationFrameCleanupSlot( frame, step)
		if dtor then
			if not dtor.partial then utils.errorMessage( frame.env.line, "Internal: No partial dtor at step (%d)", step) end
			dtor.partial.code = utils.template_format( dtor.partial.code, {[var]=value})
		end
	end
end
-- Get a label to jump to for an exit at a specific step with a specific way of exit (return with a defined value,throw,etc.) specified with a key (exitkey)
function getAllocationFrameCleanupLabel( frame, exitkey, exitcode, exitlabel)
	local parent = frame.parent and not (frame.catch == frame and (exitkey == "catch" or exitkey == "throw")) and frame.parent or nil
	local exit = frame.exitmap[ exitkey]
	local env = frame.env
	if not exit then
		if parent then
			local parent_cleanup = getAllocationFrameCleanupLabel( parent, exitkey, exitcode, exitlabel)
			exit = {labels={}, labelsteps={}, exitcode=utils.constructor_format( llvmir.control.gotoStatement, {inp=parent_cleanup})}
		else
			if exitlabel then exitcode = (exitcode or "") .. utils.constructor_format( llvmir.control.gotoStatement, {inp=exitlabel}) end
			if not exitcode then utils.errorMessage( env.line, "Internal: Frame cleanup without label or code defined") end
			exit = {labels={}, labelsteps={}, exitcode=exitcode}
		end
		frame.exitmap[ exitkey] = exit
		table.insert( frame.exitkeys, exitkey)
	end
	local step = typedb:step()
	local rt = exit.labels[ step]
	if not rt then
		rt = env.label()
		exit.labels[ step] = rt
		local li = #exit.labelsteps
		while li >= 1 and exit.labelsteps[ li] >= step do li = li - 1 end
		table.insert( exit.labelsteps, li+1, step) -- insert in ascending order
	end
	return rt
end
function getCleanupLabel( exitkey, exitcode, exitlabel)
	return getAllocationFrameCleanupLabel( getOrCreateThisAllocationFrame(), exitkey, exitcode, exitlabel)
end
function getPartialDtorCode( env, dtors, di)
	local code = utils.constructor_format( dtors[ di].partial.code, {}, env.register)
	local step = dtors[ di].partial.parent_step
	while di < #dtors and dtors[ di].step < step do di = di + 1 end
	if di < #dtors and dtors[ di].step == step and dtors[ di].partial then return code .. getPartialDtorCode( env, dtors, di) else return code end
end
function addCodeLabel( code, label, nofollow)
	local fmt = nofollow and llvmir.control.plainLabel or llvmir.control.label
	return code .. utils.template_format( fmt, {inp=label}),false
end
function addCodeGoto( code, label, nofollow)
	if nofollow then return code,true else return code .. utils.template_format( llvmir.control.gotoStatement, {inp=label}),true end
end
-- Get the exception abort or the return statement cleanup code of an allocation frame
function getAllocationFrameAbortCleanupCode( frame)
	local code = frame.landingpad and frame.landingpad.code or ""
	local env = frame.env
	for _,ek in ipairs( frame.exitkeys) do
		exit = frame.exitmap[ ek]
		local labelsteps,labels,dtors = exit.labelsteps,exit.labels,frame.dtors
		local di,li = #dtors,#labelsteps
		local first = true
		local nofollow = true
		local nextDtorLabel = nil
		while di >= 1 and li >= 1 do -- merge the labels and the cleanup code, both bound to the scope step when they happened
			local dtor = dtors[ di]
			local lbstep = labelsteps[ li]
			if dtor.step == lbstep and dtor.partial then
				local label = labels[ lbstep]
				if not nextDtorLabel then nextDtorLabel = label .. "_nd" end
				code,nofollow = addCodeGoto( code, nextDtorLabel, nofollow)
				code = code .. utils.constructor_format( llvmir.control.plainLabel, {inp=label})
					.. getPartialDtorCode( env, dtors, di)
				li = li - 1
				first = false
				nofollow = false
				if dtor.cleanup then
					local followPartialDtorLabel = label .. "_fp"
					code,nofollow = addCodeGoto( code, followPartialDtorLabel, nofollow)
					code,nofollow = addCodeLabel( code, nextDtorLabel, nofollow)
					nextDtorLabel = nil
					code = code .. dtor.cleanup.code
					code,nofollow = addCodeLabel( code, followPartialDtorLabel, false)
				end
				di = di -1
			elseif dtor.step < lbstep then
				local label = labels[ lbstep]
				code,nofollow = addCodeLabel( code, label, nofollow)
				li = li - 1
				first = false
			else
				if not first and dtor.cleanup then
					if nextDtorLabel then
						code,nofollow = addCodeLabel( code, nextDtorLabel, nofollow)
						nextDtorLabel = nil
					end
					code = code .. dtor.cleanup.code
					nofollow = false
				end
				di = di -1
			end
		end
		if di >= 1 then -- last label printed, only code output
			while di >= 1 do
				local dtor = frame.dtors[ di]
				if not first and dtor.cleanup then
					if nextDtorLabel then
						code,nofollow = addCodeLabel( code, nextDtorLabel, nofollow)
						nextDtorLabel = nil
					end
					code = code .. dtor.cleanup.code
					nofollow = false
				end
				di = di -1
			end
		elseif li >= 1 then -- last code printed, print all labels left and then the exit code
			while li >= 1 do
				local label = labels[ labelsteps[ li]]
				code,nofollow = addCodeLabel( code, label, nofollow)
				li = li -1
				first = false
			end
		end
		if nextDtorLabel then
			code,nofollow = addCodeLabel( code, nextDtorLabel, nofollow)
			nextDtorLabel = nil
		end
		code = code .. exit.exitcode
	end
	return code
end
-- Get the normal exit (no explicit return or exception) cleanup code of an allocation frame
function getAllocationFrameRegularExitCleanupCode( frame)
	local code = ""
	if #frame.dtors > 0 then
		local label = frame.env.label()
		code = code .. utils.constructor_format( llvmir.control.label, {inp=label})
		for di=#frame.dtors,1,-1 do 
			local dtor = frame.dtors[ di]
			if dtor.cleanup then code = code .. dtor.cleanup.code end
		end
	end
	return code
end
-- For debugging: Print the contents of an allocation frame to string without following parents
function allocationFrameToString( frame)
	local rt = ""
	rt = rt .. mewa.tostring({env_name=frame.env.name,scope=frame.scope,dtors=frame.dtors}) .. "\n"
	for _,ek in ipairs( frame.exitkeys) do
		rt = rt .. ek .. " => " .. mewa.tostring({frame.exitmap[ ek]}) .. "\n"
	end
	return rt
end
-- Get the code of a jump to the start of a cleanup chain with an ending 'ret llvmtype value' statement
function doReturnTypeStatement( typeId, constructor)
	local out,code = constructorParts( constructor)
	local retcode = utils.template_format( llvmir.control.returnStatement, {this=out, type=typeDescriptionMap[ typeId].llvmtype})
	local label = getCleanupLabel( constructorKey( "return", typeId, out), retcode)
	return code .. utils.template_format( llvmir.control.gotoStatement, {inp=label})
end
-- Get the code of a jump to the start of a cleanup chain with an ending 'br label {exit}' statement
function doReturnFromMain( exitlabel)
	local label = getCleanupLabel( "main return", nil, exitlabel)
	return utils.constructor_format( llvmir.control.gotoStatement, {inp=label})
end
-- Get the code of a jump to the start of a cleanup chain with an ending 'ret void' statement
function doReturnVoidStatement()
	local label = getCleanupLabel( "return void", llvmir.control.returnFromProcedure)
	return utils.template_format( llvmir.control.gotoStatement, {inp=label})
end
-- Some initialization code of a callable environment defined by features used (usage declared in in env.features)
function callableFeaturesInitCode( env)
	local rt = ""
	if env.features then
		if env.features.exception then rt = rt .. llvmir.exception.allocExceptionLocal end
		if env.features.landingpad then rt = rt .. llvmir.exception.allocLandingpad end
		if env.features.initstate and env.initstate then rt = rt .. llvmir.exception.allocInitstate end
	end
	return rt
end
function enableFeatureException( env)
	if not env.features then
		env.features={exception=true, landingpad=false, initstate=true}
	else
		env.features.exception = true
		env.features.initstate = true
	end
end
function enableFeatureLandingpad( env)
	if not env.features then
		env.features={exception=false, landingpad=true, initstate=true}
	else
		env.features.landingpad = true
		env.features.initstate=true
	end
end
-- Initialize a frame that catches exceptions
function initTryBlock( catchlabel)
	local frame = getOrCreateThisAllocationFrame()
	frame.catch = frame
	enableFeatureException( frame.env)
	getAllocationFrameCleanupLabel( frame, "catch", nil, catchlabel)
end
-- Find out if there is a catch around the current scope step, determines if we can call a throwing function inside a non throwing function or not
function hasCatchFrame()
	local frame = getAllocationFrame()
	if frame and frame.catch then return true else return false end
end
-- Ensure that exceptions and their linkup code are defined, define them if not yet done
function requireExceptions()
	if not exceptionSectionPrinted then
		local exceptionSection = ""
		exceptionSection = llvmir.externFunctionDeclaration( "C", "i8*", "strdup", "i8*", false)
				.. llvmir.externFunctionDeclaration( "C", "void", "free", "i8*", false)
				.. llvmir.exception.section
		print_section( "Auto", exceptionSection)
		exceptionSectionPrinted = true
	end
end
-- Get the label to unwind a throwing call in case of an exception
function getInvokeUnwindLabel()
	if instantUnwindLabel then return instantUnwindLabel end
	local frame = getOrCreateThisAllocationFrame()
	local env = frame.env
	local cleanupLabel,landingpadLabel,landingpadCode
	requireExceptions()
	enableFeatureLandingpad( env)
	if not frame.landingpad then frame.landingpad = {code="",map={}} end
	if frame.catch then
		cleanupLabel = getAllocationFrameCleanupLabel( frame, "catch")
		if frame.landingpad.map[ cleanupLabel] then
			return frame.landingpad.map[ cleanupLabel]
		else
			landingpadLabel = env.label()
			landingpadCode = utils.constructor_format( llvmir.exception.catch, {landingpad=landingpadLabel}, env.register)
					.. utils.constructor_format( llvmir.control.gotoStatement, {inp=cleanupLabel})
		end
	else
		if frame.exitmap[ "catch"] then
			cleanupLabel = getAllocationFrameCleanupLabel( frame, "catch")
		else
			local exitcode = env.partial_dtor and utils.constructor_format( env.partial_dtor, {}, env.register) or ""
			exitcode = exitcode .. utils.constructor_format( llvmir.exception.cleanup_end, {}, env.register)
			cleanupLabel = getAllocationFrameCleanupLabel( frame, "catch", exitcode)
		end
		if frame.landingpad.map[ cleanupLabel] then
			return frame.landingpad.map[ cleanupLabel]
		else
			local cleanup_start_fmt = frame.initstate and llvmir.exception.cleanup_start_constructor or llvmir.exception.cleanup_start
			landingpadLabel = env.label()
			landingpadCode = utils.constructor_format( llvmir.exception.cleanup_start, {landingpad=landingpadLabel}, env.register)
			if frame.initstate then landingpadCode = landingpadCode .. utils.constructor_format( llvmir.exception.set_constructor_initstate, {initstate=frame.initstate}) end
			landingpadCode = landingpadCode .. utils.constructor_format( llvmir.control.gotoStatement, {inp=cleanupLabel})
		end
	end
	frame.landingpad.code = frame.landingpad.code .. landingpadCode
	frame.landingpad.map[ cleanupLabel] = landingpadLabel
	return landingpadLabel
end
-- Get the code that throws an exception
function getThrowExceptionCode( errcode, errmsg)
	local frame = getOrCreateThisAllocationFrame()
	local env = frame.env
	local rt = frame.env.label()
	local errcode_out,errcode_code = constructorParts( errcode)
	local errmsg_out,errmsg_code = constructorParts( errmsg)
	local cleanup
	requireExceptions()
	enableFeatureException( env)
	if frame.catch then
		cleanup = getAllocationFrameCleanupLabel( frame, "catch")
	else
		cleanup = getAllocationFrameCleanupLabel( frame, "throw", (env.partial_dtor or "") .. llvmir.exception.throwExceptionLocal)
	end
	local code = errcode_code .. errmsg_code
	if frame.initstate then code = code .. utils.template_format( llvmir.exception.set_constructor_initstate, {initstate=frame.initstate}) end
	return code .. utils.constructor_format( llvmir.exception.initExceptionLocal, {errcode=errcode_out, errmsg=errmsg_out}, env.register)
			.. utils.constructor_format( llvmir.control.gotoStatement, {inp=cleanup})
end
-- Function as env.returnfunction used for return from main
function returnFromMainFunction( rtype, exitlabel, retvaladr)
	local assign_int_fmt = typeDescriptionMap[ rtype].assign
	return function( constructor)
		local out,code = constructorParts( constructor)
		local assignretcode = code .. utils.constructor_format( assign_int_fmt, {this=retvaladr, arg1=out})
		return assignretcode .. doReturnFromMain( exitlabel)		
	end
end
-- Get the whole code block of a callable including init and cleanup code 
function getCallableEnvironmentCodeBlock( env, code)
	local rt = callableFeaturesInitCode( env) .. code
	for _,frame in ipairs(env.frames) do rt = rt .. getAllocationFrameAbortCleanupCode( frame) end
	return rt
end
-- Mark the scope passed as argument or the current scope as zone where initializations of member variables in a constructor are not allowed (for example inside a loop)
function disallowInitCalls( line, msg, scope)
	if scope then
		local scope_bk = typedb:scope()
		if not typedb:get_instance("noinit") then typedb:set_instance("noinit",{line,msg}) end
		typedb:scope(scope_bk)
	else
		if not typedb:get_instance("noinit") then typedb:set_instance("noinit",{line,msg}) end
	end
end
-- Log an initialization call of a member by the assignment operator '=' in a constructor. Increments the initialization state and checks the order of initialization.
function logInitCall( name, index)
	local line,msg = table.unpack(typedb:get_instance("noinit") or {})
	local code = ""
	if msg then utils.errorMessage( line, msg) end
	local frame = getOrCreateThisAllocationFrame()
	local env = frame.env
	if frame.initstate >= index+1 then utils.errorMessage( env.line, "Multiple initializations for member '%s' or initializations not in order of definition", name)
	else while frame.initstate ~= index do
		code = code .. getImplicitInitCode( node, env, frame.initstate, index)
		frame.initstate = index+1
	end end
	frame.initstate = index+1
	return code
end
-- Update the environment initialization state after an allocation frame is done
function resumeInitState( frame)
	if frame.parent then if frame.parent.initstate < frame.initstate then frame.parent.initstate = frame.initstate end else frame.env.initstate = frame.initstate end
end
-- Get the code of the default initialization constructor
function defaultInitConstructorCode( node, env, member)
	local seekctx = getSeekContextTypes()
	local resolveContextTypeId, reductions, items = typedb:resolve_type( seekctx, member.name, tagmask_resolveType)
	local membervar,memberconstructor = selectNoArgumentType( node, seekctx, member.name, resolveContextTypeId, reductions, items)
	local res = tryApplyCallable( node, {type=membervar,constructor=memberconstructor}, ":=", {})
	if not res then utils.errorMessage( env.line, "No default constructor available for member '%s', need explicit initialization for every case", member.name) end
	return res.constructor.code
end
-- Get the code of the default initialization constructors from state initstate to state initstateTo, needed top level up initialization states of different branches
function getImplicitInitCode( node, env, initstate, initstateTo)
	local rt = ""
	while initstate < initstateTo do
		initstate = initstate + 1
		rt = rt .. defaultInitConstructorCode( node, env, env.initcontext.members[ initstate])
	end
	return rt
end
-- Code needed for completion of the initialization of a class in its constructor
function completeCtorInitializationCode( nofollow)
	local frame = getOrCreateThisAllocationFrame()
	local env = frame.env
	if nofollow and frame.initstate ~= #env.initcontext.members then utils.errorMessage( node, "Internal: Can't complete initialization") end
	local rt = getImplicitInitCode( {line=env.line}, env, frame.initstate, #env.initcontext.members)
	frame.initstate = #env.initcontext.members
	return rt
end
-- Constructor that returns the input with implicit completion of the initialization of the class in a constructor. Used to ensure that a class initialization in a constructor is complete before any method is called or any member is read.
function completeCtorInitializationAndReturnThis( this)
	this.code = (this.code or "") .. completeCtorInitializationCode( this.nofollow)
	return this
end
-- Declare a variable implicitly that does not appear as definition in source (for example the 'self' reference in a method body).
function defineImplicitVariable( node, typeId, name, reg)
	local var = typedb:def_type( localDefinitionContext, name, reg)
	if var == -1 then utils.errorMessage( node.line, "Duplicate definition of variable '%s'", typeDeclarationString( typeId, name)) end
	typedb:def_reduction( typeId, var, nil, tag_typeDeclaration)
	return var
end
-- Tells if a variable is the only return value in its declaration scope
function variableIsOnlyReturnValueInScope( typeId, name)
	local declnode = typedb:get_instance( "node")
	if declnode then
		local decllist = utils.findNodes( declnode, typesystem.return_value)
		for _,decl in ipairs(decllist) do
			if not decl.arg[1].call or decl.arg[1].call.proc ~= typesystem.variable or decl.arg[1].arg[1].value ~= name then return false end
		end
		return #decllist > 0
	end
	return false
end
-- Define a free local variable
function defineLocalVariable( node, descr, context, typeId, refTypeId, name, initVal)
	local env = getCallableEnvironment()
	local out,code
	if env.returntype == typeId and doReturnValueAsReferenceParameter(typeId) and variableIsOnlyReturnValueInScope( typeId, name) then -- do try copy elision by in-place return value construction if the variable is the only instance returned in the declaration scope
		out = "%rt"
		code = ""
		local var2 = typedb:resolve_type( getSeekContextTypes(), name)
		if var2 then
			local scope2 = typedb:type_scope( var2)
			if scope2[1] ~= 0 and scope2[1] >= env.scope[1] then utils.errorMessage( node.line, "Redeclaration of variable '%s' that is subject to copy elision in nested scope", name) end
		end
	else
		out = env.register()
		code = utils.constructor_format( descr.def_local, {out = out}, env.register)
	end
	local var = typedb:def_type( localDefinitionContext, name, constructorStruct(out))
	if var == -1 then utils.errorMessage( node.line, "Duplicate definition of variable '%s'", name) end
	typedb:def_reduction( refTypeId, var, nil, tag_typeDeclaration)
	local decl = {type=var, constructor={code=code,out=out}}
	if type(initVal) == "function" then initVal = initVal() end
	local init = applyCallable( node, decl, ":=", {initVal})
	if out ~= "%rt" then -- no copy elision
		local cleanup = typeConstructorPairCode( tryApplyCallable( node, {type=refTypeId,constructor={out=out}}, ":~", {}))
		local step = (initVal and type(initVal.constructor) == "table") and initVal.constructor.step or nil
		if cleanup then registerCleanupCode( "local variable " .. name, cleanup, step) end
	end
	return init
end
-- Define a free global variable
function defineGlobalVariable( node, descr, context, typeId, refTypeId, name, initVal)
	pushEnvironment( globalCallableEnvironment, globalAllocationFrame, "abort")
	out = "@" .. name
	local var = typedb:def_type( 0, name, out)
	if var == -1 then utils.errorMessage( node.line, "Duplicate definition of variable '%s'", name) end
	typedb:def_reduction( refTypeId, var, nil, tag_typeDeclaration)
	if type(initVal) == "function" then initVal = initVal() end
	if initVal and descr.scalar == true and isScalarConstExprValueType( initVal.type) then
		local initScalarConst = constructorParts( getRequiredTypeConstructor( node, typeId, initVal, tagmask_matchParameter, tagmask_typeConversion))
		print( utils.constructor_format( descr.def_global_val, {out = out, val = initScalarConst})) -- print global data declaration
	else
		print( utils.constructor_format( descr.def_global, {out = out})) -- print global data declaration
		local decl = {type=var, constructor={code=code,out=out}}
		local init = tryApplyCallable( node, decl, ":=", {initVal})
		if init.constructor then
			globalInitCode = globalInitCode .. init.constructor.code
			if init.constructor.throws then globalCallableEnvironment.throws = true end
		end
	end
	local cleanup = typeConstructorPairCode( tryApplyCallable( node, {type=refTypeId,constructor={out=out}}, ":~", {}))
	local step = (initVal and type(initVal.constructor) == "table") and initVal.constructor.step or nil
	if cleanup then registerCleanupCode( "global variable " .. name, cleanup, step) end
	popEnvironment()
end
-- Define a free variable or a member variable (depending on the context)
function defineVariable( node, context, typeId, name, initVal)
	local descr = typeDescriptionMap[ typeId]
	local refTypeId = referenceTypeMap[ typeId]
	if not refTypeId then utils.errorMessage( node.line, "References not allowed in variable declarations, use pointer instead: %s", typedb:type_string(typeId)) end
	if context.domain == "local" then
		return defineLocalVariable( node, descr, context, typeId, refTypeId, name, initVal)
	elseif context.domain == "global" then
		defineGlobalVariable( node, descr, context, typeId, refTypeId, name, initVal)
	elseif context.domain == "member" then
		if initVal then utils.errorMessage( node.line, "No initialization value in definition of member variable allowed") end
		defineMemberVariable( node, descr, context, typeId, name, context.private)
	else
		utils.errorMessage( node.line, "Internal: Context domain undefined, context=%s", mewa.tostring(context))
	end
end
-- Incremental build up of the context LLVM type specification from its members (this function is adding one member)
function expandContextLlvmMember( descr, context)
	if context.llvmtype == "" then context.llvmtype = descr.llvmtype else context.llvmtype = context.llvmtype  .. ", " .. descr.llvmtype end
end
-- Define the assignment operators of the class members accessed via the init type of the class (self type in a constructor) as ctor instance
function defineCtorInitAssignments( refType, initType, name, index, load_ref)
	local scope_bk,step_bk = typedb:scope( typedb:type_scope( refType))
	local items = typedb:get_types( refType, ":=")
	typedb:scope( scope_bk,step_bk)
	local loadType = typedb:def_type( initType, name, callConstructor( load_ref, true), {})
	for _,item in ipairs(items) do
		local constructor = typedb:type_constructor(item)
		local function loggedConstructor( this, args, llvmtypes) -- increment the state after successful constructor call, that is the info for the partial cleanup, if the constructor throws
			local rt = constructor( this, args, llvmtypes)
			rt.code = logInitCall( name, index) .. rt.code
			return rt
		end
		local params = typedb:type_parameters(item)
		defineCall( loadType, loadType, "=", params, loggedConstructor)
		if #params == 0 then defineCall( loadType, loadType, ":=", {}, constructor) end -- declare default constructor
	end
	typedb:def_reduction( refType, loadType, completeCtorInitializationAndReturnThis, tag_typeDeduction, rdw_barrier) -- define phase change to completed initialization if used otherwise
end
-- Define a member variable of a class or a structure
function defineMemberVariable( node, descr, context, typeId, name, private)
	local qs = typeQualiSepMap[ typeId]
	local memberpos = context.structsize
	local index = #context.members
	local load_ref = utils.template_format( context.descr.loadelemref, {index=index, type=descr.llvmtype})
	local load_val = utils.template_format( context.descr.loadelem, {index=index, type=descr.llvmtype})

	while math.fmod( memberpos, descr.align) ~= 0 do memberpos = memberpos + 1 end
	context.structsize = memberpos + descr.size
	local member = {
		type = typeId,
		name = name,
		qualitype = qualiTypeMap[ qs.valtype],
		qualifier = qs.qualifier,
		descr = descr,
		bytepos = memberpos
	}
	table.insert( context.members, member)
	expandContextLlvmMember( descr, context)
	local r_typeId = referenceTypeMap[ typeId]
	local c_r_typeId = constTypeMap[ r_typeId]
	if private == true then
		defineCall( r_typeId, context.qualitype.priv_reftype, name, {}, callConstructor( load_ref, true))
		defineCall( c_r_typeId, context.qualitype.priv_c_reftype, name, {}, callConstructor( load_ref, true))
	else
		defineCall( r_typeId, context.qualitype.reftype, name, {}, callConstructor( load_ref, true))
		defineCall( c_r_typeId, context.qualitype.c_reftype, name, {}, callConstructor( load_ref, true))
		defineCall( typeId, context.qualitype.c_valtype, name, {}, callConstructor( load_val, true))
	end
	if context.qualitype.init_reftype then
		defineCtorInitAssignments( r_typeId, context.qualitype.init_reftype, name, index, load_ref)
	end
end
-- Define an inherited interface in a class as a member variable. The interface object is constructed in the destination, therefore we use the mechanism of unbound reference types, that are constructed when used.
function defineInterfaceMember( node, context, typeId, typnam, private)
	local descr = typeDescriptionMap[ typeId]
	table.insert( context.interfaces, typeId)
	local fmt = utils.template_format( descr.getClassInterface, {classname=context.descr.symbol})
	local thisType = getFunctionThisType( descr.private, descr.const, context.qualitype.reftype)
	defineCall( unboundReferenceTypeMap[ typeId], thisType, typnam, {}, convConstructorUnboundReference( fmt))
end
-- Define a reduction to a member variable to implement class/interface inheritance
function defineReductionToMember( objTypeId, name, const)
	local objRefTypeId = referenceTypeMap[ objTypeId]
	memberTypeId = typedb:get_type( objRefTypeId, name)
	typedb:def_reduction( memberTypeId, objRefTypeId, typedb:type_constructor( memberTypeId), tag_typeDeclaration, preferWeight( const, rwd_c_inheritance, rwd_inheritance))
end
-- Define a reduction from the pointer to the class to the pointer to the member variable to implement class/interface inheritance of class pointers
function definePointerReductionToMemberPointer( classTypeId, name, inheritTypeId, const)
	local classRefTypeId = referenceTypeMap[ classTypeId]
	memberConstructor = typedb:type_constructor( typedb:get_type( classRefTypeId, name))
	local inheritPointerTypeId = pointerTypeMap[ inheritTypeId]
	local classPointerTypeId = pointerTypeMap[ classTypeId]
	if not inheritPointerTypeId then inheritPointerTypeId = createPointerTypeInstance( node, {const=true}, inheritTypeId) end -- create pointer to member type (pointer is const)
	if not classPointerTypeId then classPointerTypeId = createPointerTypeInstance( node, {const=true}, classTypeId) end -- create pointer to class type (pointer is const)
	typedb:def_reduction( inheritPointerTypeId, classPointerTypeId, memberConstructor, tag_typeDeclaration, preferWeight( const, rwd_c_inheritance, rwd_inheritance))
end
-- Define the reductions implementing class inheritance
function defineClassInheritanceReductions( context, name, private, inheritTypeId)
	defineReductionToMember( context.qualitype.c_valtype, name, true)
	defineReductionToMember( context.qualitype.valtype, name, false)
	definePointerReductionToMemberPointer( context.qualitype.c_valtype, name, constTypeMap[ inheritTypeId], true)
	definePointerReductionToMemberPointer( context.qualitype.valtype, name, inheritTypeId, false)
end
-- Define the reductions implementing interface inheritance
function defineInterfaceInheritanceReductions( context, name, private, const)
	local contextType = getFunctionThisType( private, const, context.qualitype.valtype)
	defineReductionToMember( contextType, name, const)
end
-- Make a function/procedure/operator/constructor parameter addressable by name in the callable body
function defineParameter( node, context, typeId, name)
	local env = getCallableEnvironment()
	local paramreg = env.register()
	local var = typedb:def_type( localDefinitionContext, name, paramreg)
	if var == -1 then utils.errorMessage( node.line, "Duplicate definition of parameter '%s'", typeDeclarationString( localDefinitionContext, name)) end
	local ptype = typeId
	if doPassValueAsReferenceParameter( ptype) then
		local qs = typeQualiSepMap[ typeId]
		if qs.valtype == typeId then utils.errorMessage( node.line, "Passing structure parameter as value type not allowed as implicit copy is not implemented yet") end
		ptype = referenceTypeMap[ ptype]
	end
	typedb:def_reduction( ptype, var, nil, tag_typeDeclaration)
	return {type=ptype, llvmtype=typeDescriptionMap[ ptype].llvmtype, reg=paramreg}
end
-- Initialize control boolean types used for implementing control structures and '&&','||' operators on booleans
function initControlBooleanTypes()
	controlTrueType = typedb:def_type( 0, " controlTrueType")
	controlFalseType = typedb:def_type( 0, " controlFalseType")

	local function falseExitToBoolean( constructor)
		local env = getCallableEnvironment()
		local out = env.register()
		return {code = constructor.code .. utils.constructor_format(llvmir.control.falseExitToBoolean,{falseExit=constructor.out,out=out},env.label),out=out}
	end
	local function trueExitToBoolean( constructor)
		local env = getCallableEnvironment()
		local out = env.register()
		return {code = constructor.code .. utils.constructor_format( llvmir.control.trueExitToBoolean,{trueExit=constructor.out,out=out},env.label),out=out}
	end
	typedb:def_reduction( scalarBooleanType, controlTrueType, falseExitToBoolean, tag_typeDeduction, rwd_control)
	typedb:def_reduction( scalarBooleanType, controlFalseType, trueExitToBoolean, tag_typeDeduction, rwd_control)

	local function booleanToFalseExit( constructor)
		local env = getCallableEnvironment()
		local out = env.label()
		return {code = constructor.code .. utils.constructor_format( llvmir.control.booleanToFalseExit, {inp=constructor.out, out=out}, env.label),out=out}
	end
	local function booleanToTrueExit( constructor)
		local env = getCallableEnvironment()
		local out = env.label()
		return {code = constructor.code .. utils.constructor_format( llvmir.control.booleanToTrueExit, {inp=constructor.out, out=out}, env.label),out=out}
	end

	typedb:def_reduction( controlTrueType, scalarBooleanType, booleanToFalseExit, tag_typeDeduction, rwd_control)
	typedb:def_reduction( controlFalseType, scalarBooleanType, booleanToTrueExit, tag_typeDeduction, rwd_control)

	local function negateControlTrueType( this) return {type=controlFalseType, constructor=this.constructor} end
	local function negateControlFalseType( this) return {type=controlTrueType, constructor=this.constructor} end

	local function joinControlTrueTypeWithBool( this, arg)
		local out = this.out
		local code2 = utils.constructor_format( llvmir.control.booleanToFalseExit, {inp=arg[1].out, out=out}, getCallableEnvironment().label)
		return {code=this.code .. arg[1].code .. code2, out=out}
	end
	local function joinControlFalseTypeWithBool( this, arg)
		local out = this.out
		local code2 = utils.constructor_format( llvmir.control.booleanToTrueExit, {inp=arg[1].out, out=out}, getCallableEnvironment().label)
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
			local env = getCallableEnvironment()
			return {code= this.code .. utils.constructor_format( llvmir.control.terminateTrueExit,{out=this.out},env.label), out=this.out}
		end
	end
	local function joinControlTrueTypeWithConstexprBool( this, arg)
		if arg == true then
			return this
		else 
			local env = getCallableEnvironment()
			return {code= this.code .. utils.constructor_format( llvmir.control.terminateFalseExit,{out=this.out},env.label), out=this.out}
		end
	end
	defineCall( controlTrueType, controlTrueType, "&&", {constexprBooleanType}, joinControlTrueTypeWithConstexprBool)
	defineCall( controlFalseType, controlFalseType, "||", {constexprBooleanType}, joinControlFalseTypeWithConstexprBool)

	local function constexprBooleanToControlTrueType( value)
		local env = getCallableEnvironment()
		local out = label()
		local code = (value == true) and "" or utils.constructor_format( llvmir.control.terminateFalseExit, {out=out}, env.label)
		return {code=code, out=out}
	end
	local function constexprBooleanToControlFalseType( value)
		local env = getCallableEnvironment()
		local out = label()
		local code = (value == false) and "" or utils.constructor_format( llvmir.control.terminateFalseExit, {out=out}, env.label)
		return {code=code, out=out}
	end
	typedb:def_reduction( controlFalseType, constexprBooleanType, constexprBooleanToControlFalseType, tag_typeDeduction, rwd_control)
	typedb:def_reduction( controlTrueType, constexprBooleanType, constexprBooleanToControlTrueType, tag_typeDeduction, rwd_control)

	local function invertControlBooleanType( this)
		local out = getCallableEnvironment().label()
		return {code = this.code .. utils.constructor_format( llvmir.control.invertedControlType, {inp=this.out, out=out}), out = out}
	end
	typedb:def_reduction( controlFalseType, controlTrueType, invertControlBooleanType, tag_typeDeduction, rwd_control)
	typedb:def_reduction( controlTrueType, controlFalseType, invertControlBooleanType, tag_typeDeduction, rwd_control)

	definePromoteCall( controlTrueType, constexprBooleanType, controlTrueType, "&&", {scalarBooleanType}, constexprBooleanToControlTrueType)
	definePromoteCall( controlFalseType, constexprBooleanType, controlFalseType, "||", {scalarBooleanType}, constexprBooleanToControlFalseType)
end
-- Initialize all built-in types
function initBuiltInTypes()
	lambdaResultType = typedb:def_type( 0, "lambda result")
	stringAddressType = typedb:def_type( 0, "string address")
	local byteQualitype = nil
	for typnam, scalar_descr in pairs( llvmir.scalar) do
		local qualitype = defineQualiTypes( {line=0}, 0, typnam, scalar_descr)
		local valtype = qualitype.valtype
		local c_valtype = qualitype.c_valtype
		local constexprInitWeight = factorWeight( scalar_descr.sizeweight, rdw_constexpr)
		scalarQualiTypeMap[ typnam] = qualitype
		if scalar_descr.class == "fp" then
			if scalar_descr.llvmtype == "double" then
				scalarFloatType = c_valtype -- LLVM needs a double to be passed as vararg argument
				typedb:def_reduction( valtype, constexprFloatType, constexprFloatToDoubleConstructor, tag_typeInstantiation, constexprInitWeight)
			else
				typedb:def_reduction( valtype, constexprFloatType, constexprFloatToFloatConstructor, tag_typeInstantiation, constexprInitWeight)
			end
		elseif scalar_descr.class == "bool" then
			scalarBooleanType = c_valtype
			typedb:def_reduction( valtype, constexprBooleanType, constexprFloatToBooleanConstructor, tag_typeInstantiation, constexprInitWeight)
		elseif scalar_descr.class == "unsigned" then
			if scalar_descr.llvmtype == "i32" and not scalarIntegerType then
				scalarIntegerType = c_valtype
				scalarIntegerTypnam = typnam
			elseif scalar_descr.llvmtype == "i64" and not scalarLongType then
				scalarLongType = c_valtype
				scalarLongTypnam = typnam
			elseif scalar_descr.llvmtype == "i8" then
				byteQualitype = qualitype
			end
			scalarIndexTypeMap[ typnam] = c_valtype
			typedb:def_reduction( valtype, constexprUIntegerType, constexprIntegerToIntegerConstructor, tag_typeInstantiation, constexprInitWeight)
		elseif scalar_descr.class == "signed" then
			if scalar_descr.llvmtype == "i32" then
				scalarIntegerType = c_valtype
				scalarIntegerTypnam = typnam
			elseif scalar_descr.llvmtype == "i64" then
				scalarLongType = c_valtype
				scalarLongTypnam = typnam
			elseif scalar_descr.llvmtype == "i8" and not byteQualitype then
				byteQualitype = qualitype
			end
			scalarIndexTypeMap[ typnam] = c_valtype
			typedb:def_reduction( valtype, constexprIntegerType, constexprIntegerToIntegerConstructor, tag_typeInstantiation, constexprInitWeight)
		end
	end
	for typnam, scalar_descr in pairs( llvmir.scalar) do
		local qualitype = scalarQualiTypeMap[ typnam]
		local c_valtype = qualitype.c_valtype
		if scalar_descr.class == "fp" then
			constVarargTypeMap[ c_valtype] = scalarFloatType
		elseif scalar_descr.class == "bool" then
			constVarargTypeMap[ c_valtype] = scalarIntegerType
		elseif scalar_descr.class == "unsigned" then
			constVarargTypeMap[ c_valtype] = scalarIntegerType
		elseif scalar_descr.class == "signed" then
			constVarargTypeMap[ c_valtype] = scalarIntegerType			
		end
	end
	local bytePointerQualitype_valtype,bytePointerQualitype_cval
	if byteQualitype then
		bytePointerQualitype_valtype,bytePointerQualitype_cval = definePointerQualiTypesCrossed( {line=0}, byteQualitype)
		stringPointerType = bytePointerQualitype_cval.valtype
		memPointerType = bytePointerQualitype_valtype.valtype
		typedb:def_reduction( memPointerType, stringPointerType, nil, tag_pushVararg)
		constVarargTypeMap[ bytePointerQualitype_valtype.c_valtype] = bytePointerQualitype_valtype.c_valtype
		constVarargTypeMap[ bytePointerQualitype_cval.c_valtype] = bytePointerQualitype_cval.c_valtype
	end
	if not scalarBooleanType then utils.errorMessage( 0, "No boolean type defined in built-in scalar types") end
	if not scalarIntegerType then utils.errorMessage( 0, "No integer type mapping to i32 defined in built-in scalar types (return value of main)") end
	if not stringPointerType then utils.errorMessage( 0, "No i8 type defined suitable to be used for free string constants)") end

	local qualitype_anyclassptr = defineQualiTypes( {line=0}, 0, "any class^", llvmir.anyClassPointerDescr)
	local qualitype_anystructptr = defineQualiTypes( {line=0}, 0, "any struct^", llvmir.anyStructPointerDescr)
			                  
	anyClassPointerType = qualitype_anyclassptr.valtype
	hardcodedTypeMap[ "any class^"] = anyClassPointerType
	anyConstClassPointerType = qualitype_anyclassptr.c_valtype
	hardcodedTypeMap[ "any const class^"] = anyConstClassPointerType
	anyStructPointerType = qualitype_anystructptr.valtype
	hardcodedTypeMap[ "any struct^"] = anyStructPointerType
	anyConstStructPointerType = qualitype_anystructptr.c_valtype
	hardcodedTypeMap[ "any const struct^"] = anyConstStructPointerType
	anyFreeFunctionType = typedb:def_type( 0, "any function")
	typeDescriptionMap[ anyFreeFunctionType] = llvmir.anyFunctionDescr

	for typnam, scalar_descr in pairs( llvmir.scalar) do
		local qualitype = scalarQualiTypeMap[ typnam]
		defineScalarConstructorDestructors( qualitype, scalar_descr)
		defineBuiltInTypeConversions( typnam, scalar_descr)
		defineBuiltInTypeOperators( typnam, scalar_descr)
	end
	for typnam, scalar_descr in pairs( llvmir.scalar) do
		defineBuiltInTypePromoteCalls( typnam, scalar_descr)
	end
	defineConstExprOperators()
	defineConstExprTypeConversions()
	if byteQualitype then
		definePointerArithmeticOperators( bytePointerQualitype_valtype.valtype)
		definePointerArithmeticOperators( bytePointerQualitype_cval.valtype)
	end
	typedb:def_reduction( stringPointerType, stringAddressType, stringPointerConstructor, tag_typeInstantiation)
	constVarargTypeMap[ stringAddressType] = stringPointerType
	constVarargTypeMap[ stringPointerType] = stringPointerType	
	initControlBooleanTypes()
end
-- Application of a constructor depending on its type and its argument type, return false as 2nd result on failure, true on success
function tryApplyConstructor( node, typeId, constructor, arg)
	if constructor then
		if (type(constructor) == "function") then
			local rt = constructor( arg)
			return rt, rt ~= nil
		elseif arg then
			utils.errorMessage( node.line, "Reduction constructor overwriting previous constructor for '%s'", typedb:type_string(typeId))
		else
			return constructor, true
		end
	else
		return arg, true
	end
end
-- Application of a constructor depending on its type and its argument type, throw error on failure
function applyConstructor( node, typeId, constructor, arg)
	local result_constructor,success = tryApplyConstructor( node, typeId, constructor, arg)
	if not success then utils.errorMessage( node.line, "Failed to create type '%s'", typedb:type_string(typeId)) end
	return result_constructor
end
-- Try to apply a list of reductions on a constructor, return false as 2nd result on failure, true on success
function tryApplyReductionList( node, redulist, redu_constructor)
	local success = true
	for _,redu in ipairs(redulist) do
		redu_constructor,success = tryApplyConstructor( node, redu.type, redu.constructor, redu_constructor)
		if not success then return nil end
	end
	return redu_constructor, true
end
-- Apply a list of reductions on a constructor, throw error on failure
function applyReductionList( node, redulist, redu_constructor)
	local success = true
	for _,redu in ipairs(redulist) do
		redu_constructor,success = tryApplyConstructor( node, redu.type, redu.constructor, redu_constructor)
		if not success then utils.errorMessage( node.line, "Reduction constructor failed for '%s'", typedb:type_string(redu.type)) end
	end
	return redu_constructor
end
-- Get the handle of a type expected to have no arguments (plain typedef type or a variable name)
function selectNoArgumentType( node, seekctx, typeName, resolveContextTypeId, reductions, items)
	if not resolveContextTypeId or type(resolveContextTypeId) == "table" then -- not found or ambiguous
		utils.errorResolveType( typedb, node.line, resolveContextTypeId, seekctx, typeName)
	end
	for ii,item in ipairs(items) do
		if typedb:type_nof_parameters( item) == 0 then
			local constructor = applyReductionList( node, reductions, nil)
			local item_constructor = typedb:type_constructor( item)
			constructor = applyConstructor( node, item, item_constructor, constructor)
			return item,constructor
		end
	end
	utils.errorMessage( node.line, "Failed to resolve %s with no arguments", utils.resolveTypeString( typedb, seekctx, typeName))
end
-- Issue an error if the argument does not refer to a value
function expectValueType( node, item)
	if not item.constructor then utils.errorMessage( node.line, "'%s' does not refer to a value", typedb:type_string(item.type)) end
end
-- Issue an error if the argument does not refer to a data type
function expectDataType( node, item)
	if item.constructor then utils.errorMessage( node.line, "'%s' does not refer to a data type", typedb:type_string(item.type)) end
	return item.type
end
-- Get the type handle of a type defined as a path
function resolveTypeFromNamePath( node, qualifier, arg, argidx)
	if not argidx then argidx = #arg end
	local typeName = getQualifierTypeName( qualifier, arg[ argidx])
	local seekContextTypes
	if argidx > 1 then
		seekContextTypes = expectDataType( node, resolveTypeFromNamePath( node, "", arg, argidx-1))
	else
		seekContextTypes = getSeekContextTypes()
	end
	local resolveContextTypeId, reductions, items = typedb:resolve_type( seekContextTypes, typeName, tagmask_namespace)
	local typeId,constructor = selectNoArgumentType( node, seekContextTypes, typeName, resolveContextTypeId, reductions, items)
	return {type=typeId, constructor=constructor}
end
-- Get the list of generic arguments filled with the default parameters for the ones not specified explicitly
function matchGenericParameter( node, genericType, param, args)
	local rt = {}
	if #param < #args then utils.errorMessage( node.line, "Too many arguments (%d) passed to instantiate generic '%s'", #args, typedb:type_string(genericType)) end
	for pi=1,#param do
		local arg = args[pi] or param[pi] -- argument or default parameter
		if not arg.type then utils.errorMessage( node.line, "Missing parameter '%s' of generic '%s'", param[ pi].name, typedb:type_string(genericType)) end
		table.insert( rt, arg)
	end
	return rt
end
-- For each generic argument, create an alias named as the parameter name as substitute for the generic argument specified
function defineGenericParameterAliases( node, instanceType, param, arg)
	for ii=1,#arg do
		if arg[ ii].constructor then
			defineValueAlias( node, instanceType, param[ ii].name, arg[ ii].type, arg[ ii].constructor)
		else
			defineTypeAlias( node, instanceType, param[ ii].name, arg[ ii].type)
		end
	end
end
-- For each lambda argument, create an alias named as the parameter name as substitute for the generic argument specified
function defineLambdaParameterAliases( node, instanceType, param, arg)
	for ii=1,#arg do		
		if arg[ ii].constructor then
			defineValueAlias( node, instanceType, param[ ii], arg[ ii].type, arg[ ii].constructor)
		else
			defineTypeAlias( node, instanceType, param[ ii], arg[ ii].type)
		end
	end
end
-- Applying a lambda expression
function applyLambda( node, lambdaTypeId, lambdaDescr, lambdaArgs)
	for _,arg in ipairs(lambdaArgs) do arg.type = getDeclarationType( arg.type) end -- because the lambda is implemented in a different scope reductions of local variable declarations may not be defined, therefore we assign the declaration type to the lambda parameter
	local env = getCallableEnvironment()
	local frame = typedb:get_instance( allocFrameKey)
	local seekctx = getSeekContextTypes()
	local scope_bk,step_bk = typedb:scope(lambdaDescr.node.scope)
	local genericlocal = typedb:def_type( localDefinitionContext, "_X__" .. utils.uniqueName( lambdaDescr.name .. "__"))
	pushGenericLocal( genericlocal)
	setSeekContextTypes( seekctx)
	pushSeekContextType( genericlocal)
	typedb:set_instance( callableEnvKey, env)
	frame = createThisAllocationFrame( frame)
	if #lambdaArgs ~= #lambdaDescr.param then utils.errorMessage( node.line, "Number of lambda arguments do not match: argument %d <> %d expected", #lambdaArgs, #lambdaDescr.param) end
	defineLambdaParameterAliases( node, genericlocal, lambdaDescr.param, lambdaArgs)
	typedb:scope(scope_bk,step_bk)
	local rt = table.unpack( utils.traverse( typedb, lambdaDescr.node))
	popGenericLocal( genericlocal)
	return rt
end
-- Create an instance of a generic type with template arguments
function createGenericTypeInstance( node, genericType, genericArg, genericDescr, typeIdNotifyFunction)
	local declContextTypeId = typedb:type_context( genericType)
	local typnam = getGenericTypeName( genericType, genericArg)
	local genericlocal = typedb:def_type( declContextTypeId, "_X__" .. typnam)
	if genericlocal == -1 then utils.errorMessage( node.line, "Duplicate definition of generic '%s'", typnam) end
	pushGenericLocal( genericlocal)
	setSeekContextTypes( genericDescr.seekctx)
	pushSeekContextType( genericlocal)
	if genericDescr.class == "generic_class" or genericDescr.class == "generic_struct" then
		local fmt = (genericDescr.class == "generic_class") and llvmir.classTemplate or llvmir.structTemplate
		local descr,qualitype = defineStructureType( genericDescr.node, declContextTypeId, typnam, fmt)
		typeIdNotifyFunction( qualitype.valtype)
		defineGenericParameterAliases( genericDescr.node, qualitype.valtype, genericDescr.generic.param, genericArg)
		if genericDescr.class == "generic_class" then
			traverseAstClassDef( genericDescr.node, declContextTypeId, typnam, descr, qualitype, 3)
		else
			traverseAstStructDef( genericDescr.node, declContextTypeId, typnam, descr, qualitype, 3)
		end
	elseif genericDescr.class == "generic_procedure" or genericDescr.class == "generic_function" then
		local typeInstance = getOrCreateAnyCallableContextTypeId( genericDescr.context, typnam, genericDescr)
		typeIdNotifyFunction( typeInstance)
		defineGenericParameterAliases( genericDescr.node, typeInstance, genericDescr.generic.param, genericArg)
		local descr = {lnk="internal", signature="", name=typnam, symbol=typnam,
		               private=genericDescr.private, const=genericDescr.const, throws=genericDescr.throws, interface=false}
		pushSeekContextType( typeInstance)
		if genericDescr.class == "generic_function" then
			traverseAstFunctionDeclaration( genericDescr.node, genericDescr.context, descr, 4)
			traverseAstFunctionImplementation( genericDescr.node, genericDescr.context, descr, 4)
		else
			traverseAstProcedureDeclaration( genericDescr.node, genericDescr.context, descr, 4)
			traverseAstProcedureImplementation( genericDescr.node, genericDescr.context, descr, 4)
		end
	else
		utils.errorMessage( node.line, "Using generic parameter in '[' ']' brackets for unknown generic '%s'", genericDescr.class)
	end
	popGenericLocal( genericlocal)
end
-- Get an instance of a generic type if already defined or implicitely create it and return the created instance
function getOrCreateGenericType( node, genericType, genericDescr, instArg)
	local scope_bk,step_bk = typedb:scope( typedb:type_scope( genericType))
	local genericArg = matchGenericParameter( node, genericType, genericDescr.generic.param, instArg)
	local instanceId = getGenericParameterIdString( genericArg)
	local typkey = genericType .. "[" .. instanceId .. "]"
	local typeInstance = genericInstanceTypeMap[ typkey]
	if not typeInstance then
		createGenericTypeInstance( node, genericType, genericArg, genericDescr, function( ti) genericInstanceTypeMap[ typkey] = ti end)
		typeInstance = genericInstanceTypeMap[ typkey]
	end
	typedb:scope( scope_bk,step_bk)
	return typeInstance
end
-- Create an instance of an array type on demand with the size as argument
function createArrayTypeInstance( node, typnam, elementTypeId, elementDescr, arraySize)
	local arrayDescr = llvmir.arrayDescr( elementDescr, arraySize)
	local qualitype = defineQualiTypes( node, elementTypeId, typnam, arrayDescr)
	local arrayTypeId = qualitype.valtype
	defineUnboundReferenceTypes( elementTypeId, typnam, arrayDescr, qualitype)
	defineArrayConstructorDestructors( node, qualitype, arrayDescr, elementTypeId, arraySize)
	getAssignmentsFromConstructors( node, qualitype, arrayDescr)
	local qualitype_element = qualiTypeMap[ elementTypeId]
	defineArrayIndexOperators( qualitype_element.reftype, qualitype.reftype, arrayDescr)
	defineArrayIndexOperators( qualitype_element.c_reftype, qualitype.c_reftype, arrayDescr)
	return arrayTypeId
end
-- Get an instance of an array type if already defined or implicitely create it and return the created instance
function getOrCreateArrayType( node, elementTypeId, elementDescr, dimType)
	local scope_bk,step_bk = typedb:scope( typedb:type_scope( elementTypeId))
	local dim = getRequiredTypeConstructor( node, constexprUIntegerType, dimType, tagmask_typeAlias)
	local arraySize = tonumber( (constructorParts( dim)) )
	if arraySize <= 0 then utils.errorMessage( node.line, "Size of array is not a positive integer number") end
	local typnam = "[" .. arraySize .. "]"
	local typkey = elementTypeId .. typnam
	local rt = arrayTypeMap[ typkey]
	if not rt then
		rt = createArrayTypeInstance( node, typnam, elementTypeId, elementDescr, arraySize)
		arrayTypeMap[ typkey] = rt
	end
	typedb:scope( scope_bk,step_bk)
	return rt
end
-- Create an instance of a pointer type on demand
function createPointerTypeInstance( node, attr, typeId)
	local qs = typeQualiSepMap[ typeId]
	local qualitype_pointee = qualiTypeMap[ qs.valtype]
	local qualitype_valtype,qualitype_cval = definePointerQualiTypesCrossed( node, qualitype_pointee)
	definePointerArithmeticOperators( qualitype_valtype.valtype)
	definePointerArithmeticOperators( qualitype_cval.valtype)
	local qualitype
	if qs.qualifier.const == true then 
		qualitype = qualitype_cval
	else
		qualitype = qualitype_valtype
		local descr = typeDescriptionMap[ typeId]
		local voidptr_cast_fmt = utils.template_format( llvmir.control.bytePointerCast, {llvmtype=descr.llvmtype})
		typedb:def_reduction( memPointerType, qualitype.c_valtype, convConstructor(voidptr_cast_fmt), tag_pushVararg)
	end
	constVarargTypeMap[ qualitype.c_valtype] = memPointerType
	if attr.const == true then return qualitype.c_valtype else return qualitype.valtype end
end
-- Call a function, meaning to apply operator "()" on a callable
function callFunction( node, contextTypes, name, args)
	local resolveContextTypeId, reductions, items = typedb:resolve_type( contextTypes, name, tagmask_resolveType)
	local typeId,constructor = selectNoArgumentType( node, contextTypes, name, resolveContextTypeId, reductions, items)
	local this = {type=typeId, constructor=constructor}
	return applyCallable( node, this, "()", args)
end
-- Try to get the constructor and weight of a parameter passed with the deduction tagmask optionally passed as an argument
function tryGetWeightedParameterReductionList( node, redutype, operand, tagmask_decl, tagmask_conv)
	if redutype ~= operand.type then
		local redulist,weight,altpath = typedb:derive_type( redutype, operand.type, tagmask_decl, tagmask_conv)
		if altpath then
			utils.errorMessage( node.line, "Ambiguous derivation paths for '%s': %s | %s",
						typedb:type_string(operand.type), utils.typeListString(typedb,altpath," =>"), utils.typeListString(typedb,redulist," =>"))
		end
		return redulist,weight
	else
		return {},0.0
	end
end
-- Get the constructor of a type required. The deduction tagmasks are passed as an arguments
function getRequiredTypeConstructor( node, redutype, operand, tagmask_decl, tagmask_conv)
	if redutype ~= operand.type then
		local redulist,weight,altpath = typedb:derive_type( redutype, operand.type, tagmask_decl, tagmask_conv)
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
-- For a callable item, create for each argument the lists of reductions needed to pass the arguments to it, with accumulation of the reduction weights
function collectItemParameter( node, item, args, parameters)
	local rt = {redulist={},llvmtypes={},weight=0.0}
	for pi=1,#args do
		local redutype,redulist,weight
		if pi <= #parameters then
			redutype = parameters[ pi].type
			redulist,weight = tryGetWeightedParameterReductionList( node, redutype, args[ pi], tagmask_matchParameter, tagmask_typeConversion)
		else
			redutype = getVarargArgumentType( args[ pi].type)
			if redutype then redulist,weight = tryGetWeightedParameterReductionList( node, redutype, args[ pi], tagmask_pushVararg, tagmask_typeConversion) end
		end
		if not weight then return nil end
		if rt.weight < weight then rt.weight = weight end -- use max(a,b) as weight accumulation function
		table.insert( rt.redulist, redulist)
		local descr = typeDescriptionMap[ redutype]
		table.insert( rt.llvmtypes, descr.llvmtype)
	end
	return rt
end
-- Select the candidate items with the highest weight not exceeding maxweight
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
-- Get the best matching item from a list of items by weighting the matching of the arguments to the item parameter types
function selectItemsMatchParameters( node, items, args, this_constructor)
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
				local ac,success = nil,true
				local arg_constructors = {}
				for ai=1,#args do
					ac,success = tryApplyReductionList( node, item_parameter_map[ ii].redulist[ ai], args[ ai].constructor)
					if not success then break end
					table.insert( arg_constructors, ac)
				end
				if success then call_constructor = item_constructor( this_constructor, arg_constructors, item_parameter_map[ ii].llvmtypes) end
			end
			if call_constructor then table.insert( bestmatch, {type=items[ ii], constructor=call_constructor}) end
			item_parameter_map[ ii] = nil
		end
		if #bestmatch ~= 0 then return bestmatch,bestweight end
	end
end
-- Find a callable identified by name and its arguments (parameter matching) in the context of a type (this)
function findApplyCallable( node, this, callable, args)
	local mask = (callable == ":=" or callable == "=") and tagmask_declaration or tagmask_resolveType
	local resolveContextType,reductions,items = typedb:resolve_type( this.type, callable, mask)
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
		utils.errorMessage( node.line, "Ambiguous matches resolving callable with signature '%s', list of candidates: %s", typeDeclarationString( this, callable, args), altmatchstr)
	end
end
-- Retrieve and apply a callable in a specified context
function applyCallable( node, this, callable, args)
	local bestmatch,bestweight = findApplyCallable( node, this, callable, args)
	if not bestweight then utils.errorMessage( node.line, "Failed to find callable with signature '%s'", typeDeclarationString( this, callable, args)) end
	local rt = getCallableBestMatch( node, bestmatch, bestweight, this, callable, args)
	if not rt then  utils.errorMessage( node.line, "Failed to match parameters to callable with signature '%s'", typeDeclarationString( this, callable, args)) end
	return rt
end
-- Try to retrieve a callable in a specified context, apply it and return its type/constructor pair if found, return nil if not
function tryApplyCallable( node, this, callable, args)
	local bestmatch,bestweight = findApplyCallable( node, this, callable, args)
	if bestweight then return getCallableBestMatch( node, bestmatch, bestweight) end
end
-- Get the symbol name for a function in the LLVM output
function getTargetFunctionIdentifierString( descr, context)
	if context.domain == "local" then
		return utils.uniqueName( descr.symbol .. "__")
	elseif descr.name == ":~" then
		return utils.encodeName( context.descr.dtorname)
	else
		local pstr = context.symbol and ("__C_" .. context.symbol .. "__" .. descr.symbol) or descr.symbol
		for ai,arg in ipairs(descr.param) do pstr = pstr .. "__" .. (arg.symbol or arg.llvmtype) end
		if descr.const == true then pstr = pstr .. "__const" end
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
	for ai,arg in ipairs(args) do
		local sep = (ai == 1) and "(" or ", "
		pstr = pstr .. sep .. typedb:type_string(arg.type)
	end
	if const == true then return pstr .. ") const" else return pstr .. ")" end
end
-- Get the parameter string of a function declaration
function getDeclarationLlvmTypeRegParameterString( descr, context)
	local rt = ""
	if doReturnValueAsReferenceParameter( descr.ret) then rt = descr.rtllvmtype .. "* sret %rt, " end
	if context.domain == "member" then rt = rt .. typeDescriptionMap[ context.qualitype.valtype].llvmtype .. "* %ths, " end
	for ai,arg in ipairs(descr.param) do rt = rt .. arg.llvmtype .. " " .. arg.reg .. ", " end
	if rt ~= "" then rt = rt:sub(1, -3) end
	return rt
end
-- Get the parameter string of a function typedef
function getDeclarationLlvmTypedefParameterString( descr, context)
	local rt = ""
	if doReturnValueAsReferenceParameter( descr.ret) then rt = descr.rtllvmtype .. "* sret, " end
	if context.domain == "member" then rt = rt .. (descr.llvmthis or context.descr.llvmtype) .. "*, " end
	for ai,arg in ipairs(descr.param) do rt = rt .. arg.llvmtype .. ", " end
	if rt ~= "" then rt = rt:sub(1, -3) end
	return rt
end
-- Get the context for type declarations
function getDeclarationContextTypeId( context)
	if context.domain == "local" then return localDefinitionContext
	elseif context.domain == "member" then return context.qualitype.valtype
	elseif context.domain == "global" then return context.namespace or 0
	end
end
-- Part of identifier for generic signature, constructor (const expression, e.g. dimension) as string
function getGenericConstructorIdString( constructor)
	if type(constructor) == "table" then return constructor.out else return tostring(constructor) end
end
-- Identifier for generic signature
function getGenericParameterIdString( param)
	local rt = ""
	for pi,arg in ipairs(param) do
		if arg.constructor then rt = rt .. "#" .. getGenericConstructorIdString( arg.constructor) else rt = rt .. tostring(arg.type) .. "," end
	end
	return rt
end
-- Synthesized typename for generic
function getGenericTypeName( typeId, param)
	local rt = typedb:type_name( typeId)
	for pi,arg in ipairs(param) do
		rt = rt .. "__"
		if arg.constructor then rt = rt .. getGenericConstructorIdString(arg.constructor) else rt = rt .. typedb:type_string(arg.type, "__") end
	end
	return rt
end
-- Symbol name for type in target LLVM output
function getDeclarationUniqueName( contextTypeId, typnam)
	if contextTypeId == localDefinitionContext then nam = typnam else nam = typedb:type_string(contextTypeId, "__") .. "__" .. typnam end
	if typedb:scope()[1] == 0 then return nam else return utils.uniqueName( nam .. "__") end
end
-- Function shared by all expansions of the structure used for mapping the LLVM template for a function call
function expandDescrCallTemplateParameter( descr, context)
	if descr.ret then descr.rtllvmtype = typeDescriptionMap[ descr.ret].llvmtype else descr.rtllvmtype = "void" end
	descr.argstr = getDeclarationLlvmTypedefParameterString( descr, context)
	descr.symbolname = getTargetFunctionIdentifierString( descr, context)
end
-- Expand the structure used for relating class and interface methods and construct the LLVM type of the function representing the method 
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
		local sep = (descr.argstr == "") and "" or ", "
		descr.signature = utils.template_format( llvmir.control.functionVarargSignature,  {argstr = descr.argstr .. sep})
	end
	descr.llvmtype = utils.template_format( llvmir.control.functionCallType, descr)
end
-- Add a descriptor to the list of methods that helps to link interfaces with classes
function expandContextMethodList( node, descr, context)
	table.insert( context.methods, {llvmtype=descr.llvmtype, methodid=descr.methodid, methodname=descr.methodname, symbol=descr.symbolname, ret=descr.ret})
	if not descr.private then
		if context.methodmap[ descr.methodid] then utils.errorMessage( node.line, "Duplicate definition of method '%s'", descr.methodname) end
		context.methodmap[ descr.methodid] = #context.methods
	end
end
-- Output of the function implementation (header + body)
function printFunctionDeclaration( node, descr)
	local fmt = doReturnValueAsReferenceParameter( descr.ret) and llvmir.control.sretFunctionDeclaration or llvmir.control.functionDeclaration
	descr.attr = llvmir.functionAttribute( descr.interface, descr.env.throws)
	print( utils.constructor_format( fmt, descr))
end
-- Define a direct function call: class method call, free function call
function defineFunctionCall( thisTypeId, contextTypeId, opr, descr)
	local rtype = doReturnValueAsReferenceParameter( descr.ret) and unboundReferenceTypeMap[ descr.ret] or descr.ret
	local functype = defineCall( rtype, contextTypeId, opr, descr.param, functionDirectCallConstructor( thisTypeId, descr))
	if descr.vararg then varargFuncMap[ functype] = true end
	return functype
end
-- Define an indirect function call over a variable containing the function address
function defineFunctionVariableCall( thisTypeId, contextTypeId, opr, descr)
	local rtype = doReturnValueAsReferenceParameter( descr.ret) and unboundReferenceTypeMap[ descr.ret] or descr.ret
	return defineCall( rtype, contextTypeId, opr, descr.param, functionIndirectCallConstructor( thisTypeId, descr))
end
-- Define an indirect function call over an interface method table (VMT)
function defineInterfaceMethodCall( contextTypeId, opr, descr)
	local rtype = doReturnValueAsReferenceParameter( descr.ret) and unboundReferenceTypeMap[ descr.ret] or descr.ret
	local functype = defineCall( rtype, contextTypeId, opr, descr.param, interfaceMethodCallConstructor( descr))
	if descr.vararg then varargFuncMap[ functype] = true end
end
-- Get (if already defined) or create the callable context type (function name) on which the "()" operator implements the function call
function getOrCreateCallableContextTypeId( contextTypeId, name, descr)
	local rt = typedb:get_type( contextTypeId, name)
	if not rt then
		rt = typedb:def_type( contextTypeId, name)
		typeDescriptionMap[ rt] = descr
		return rt,true -- type is new
	end
	return rt,false
end
-- Get (if already defined) or create the callable context type where we have to examine the definition context of the callable first (when it is resolved as in case of a reference to a generic)
function getOrCreateAnyCallableContextTypeId( context, name, descr)
	if context.domain == "member" then
		local thisTypeId = getFunctionThisType( descr.private, descr.const, context.qualitype.reftype)
		return getOrCreateCallableContextTypeId( thisTypeId, name, llvmir.callableDescr)
	else
		return getOrCreateCallableContextTypeId( getDeclarationContextTypeId(context), name, llvmir.callableDescr)
	end
end
-- Common part of defineProcedureVariableType/defineFunctionVariableType
function defineCallableVariableType( node, descr, declContextTypeId)
	local qualitype = defineQualiTypes( node, declContextTypeId, descr.name, descr)
	defineFunctionVariableCall( qualitype.c_valtype, qualitype.c_valtype, "()", descr)
	defineCall( qualitype.reftype, qualitype.reftype, ":=", {anyFreeFunctionType}, copyFunctionPointerConstructor( descr, qualitype.c_valtype))
	defineCall( qualitype.c_reftype, qualitype.c_reftype, ":=", {anyFreeFunctionType}, copyFunctionPointerConstructor( descr, qualitype.c_valtype))
end
-- Define a procedure pointer type as callable object with "()" operator implementing the call
function defineProcedureVariableType( node, descr, context)
	local declContextTypeId = getDeclarationContextTypeId( context)
	expandDescrFunctionReferenceTemplateParameter( descr, context)
	defineCallableVariableType( node, llvmir.procedureVariableDescr( descr, "(" .. descr.argstr .. ")"), declContextTypeId)
end
-- Define a function pointer type as callable object with "()" operator implementing the call
function defineFunctionVariableType( node, descr, context)
	local declContextTypeId = getDeclarationContextTypeId( context)
	expandDescrFunctionReferenceTemplateParameter( descr, context)
	defineCallableVariableType( node, llvmir.functionVariableDescr( descr, descr.rtllvmtype, "(" .. descr.argstr .. ")"), declContextTypeId)
end
-- Define a free function as callable object with "()" operator implementing the call
function defineFreeFunction( node, descr, context)
	expandDescrFreeCallTemplateParameter( descr, context)
	local callablectx,newName = getOrCreateCallableContextTypeId( getDeclarationContextTypeId(context), descr.name, llvmir.callableDescr)
	local functype = defineFunctionCall( 0, callablectx, "()", descr)
	if newName then typedb:def_reduction( anyFreeFunctionType, callablectx, function(a) return {type = callablectx} end, tag_transfer, rdw_conv) end
	return functype
end
-- Define a class method as callable object with "()" operator implementing the call
function defineClassMethod( node, descr, context)
	local thisTypeId = getFunctionThisType( descr.private, descr.const, context.qualitype.reftype)
	local callablectx = getOrCreateCallableContextTypeId( thisTypeId, descr.name, llvmir.callableDescr)
	expandDescrClassCallTemplateParameter( descr, context)
	if context.methods then expandContextMethodList( node, descr, context) end
	defineFunctionCall( thisTypeId, callablectx, "()", descr)
end
-- Define an operator on the class with its arguments
function defineClassOperator( node, descr, context)
	expandDescrClassCallTemplateParameter( descr, context)
	if context.methods then expandContextMethodList( node, descr, context) end
	local contextTypeId = getFunctionThisType( descr.private, descr.const, context.qualitype.reftype)
	defineFunctionCall( contextTypeId, contextTypeId, descr.name, descr)
	defineOperatorAttributes( context, descr)
	if descr.name == "=" then context.properties.assignment = true end
end
-- Define a ctor procedure
function defineCtorProcedure( node, descr, context)
	expandDescrClassCallTemplateParameter( descr, context)
	local reftype = getFunctionThisType( descr.private, false, context.qualitype.reftype)
	local c_reftype = context.qualitype.c_reftype
	local calltype = defineFunctionCall( reftype, reftype, descr.name, descr)
	local c_calltype = defineFunctionCall( reftype, c_reftype, descr.name, descr)
	if descr.throws then
		ctorThrowingMap[ calltype] = true
		ctorThrowingMap[ c_calltype] = true
		typeHasCtorThrowingMap[ reftype] = true
		typeHasCtorThrowingMap[ c_reftype] = true
	end
end
-- Define a dtor procedure
function defineDtorProcedure( node, descr, context)
	expandDescrClassCallTemplateParameter( descr, context)
	local contextTypeId = getFunctionThisType( descr.private, false, context.qualitype.reftype)
	defineFunctionCall( contextTypeId, contextTypeId, descr.name, descr)
	defineFunctionCall( contextTypeId, constTypeMap[ contextTypeId], descr.name, descr)
end
-- Define an extern function as callable object with "()" operator implementing the call
function defineExternFunction( node, descr, context)
	if doReturnValueAsReferenceParameter( descr.ret) then utils.errorMessage( node.line, "Type not allowed as extern function return value") end
	expandDescrExternCallTemplateParameter( descr, context)
	local callablectx = getOrCreateCallableContextTypeId( 0, descr.name, llvmir.callableDescr)
	defineFunctionCall( 0, callablectx, "()", descr)
end
-- Define an interface method call as callable object with "()" operator implementing the call
function defineInterfaceMethod( node, descr, context)
	local thisTypeId = getFunctionThisType( descr.private, descr.const, context.qualitype.reftype)
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
	local contextTypeId = getFunctionThisType( descr.private, descr.const, context.qualitype.reftype)
	defineInterfaceMethodCall( contextTypeId, descr.name, descr)
	defineOperatorAttributes( context, descr)
end
-- Initialize some callable environment variables used in method calls and implicitly define the "self" variable for the scope of the method body
function expandMethodEnvironment( node, context, descr, env)
	local privateConstThisReferenceType = getFunctionThisType( true, true, context.qualitype.reftype)
	local privateThisReferenceType = getFunctionThisType( true, false, context.qualitype.reftype)
	local publicConstThisReferenceType = getFunctionThisType( false, true, context.qualitype.reftype)
	local publicThisReferenceType = getFunctionThisType( false, false, context.qualitype.reftype)
	local selfTypeId
	if descr.name == ":=" then
		selfTypeId = context.qualitype.init_reftype
		env.initstate = 0
		env.initcontext = context
		env.partial_dtor = utils.template_format( context.descr.partial_dtor, {symbol=context.descr.symbol,llvmtype="%" .. context.descr.symbol})
	elseif descr.const then
		selfTypeId = privateConstThisReferenceType
	else
		selfTypeId = privateThisReferenceType 
	end
	local classvar = defineImplicitVariable( node, selfTypeId, "self", "%ths")
	typedb:def_reduction( privateConstThisReferenceType, publicConstThisReferenceType, nil, tag_typeDeduction) -- make const private members of other instances of this class accessible
	typedb:def_reduction( privateThisReferenceType, publicThisReferenceType, nil, tag_typeDeduction) -- make private members of other instances of this class accessible
	pushSeekContextType( {type=classvar, constructor={out="%ths"}})
end
-- Define the execution context of the body of the main function
function defineMainProcVariables( node, context)
	defineImplicitVariable( node, stringPointerType, "argc", "%argc")
	defineImplicitVariable( node, scalarIntegerType, "argv", "%argv")
end
-- Create a string constant pointer type/constructor
function getStringConstant( value)
	if not stringConstantMap[ value] then
		local encval,enclen = utils.encodeLexemLlvm(value)
		local name = utils.uniqueName( "string")
		stringConstantMap[ value] = {size=enclen+1,name=name}
		print_section( "Constants", utils.constructor_format( llvmir.control.stringConstDeclaration, {name=name, size=enclen+1, value=encval}) .. "\n")
	end
	return {type=stringAddressType,constructor=stringConstantMap[value]}
end
-- Basic structure type definition for class,struct,interface
function defineStructureType( node, declContextTypeId, typnam, fmt)
	local symbol = getDeclarationUniqueName( declContextTypeId, typnam)
	local descr = utils.template_format( fmt, {symbol=symbol})
	local qualitype = defineQualiTypes( node, declContextTypeId, typnam, descr)
	return descr,qualitype
end
---- Generic (descr) Fields ----
--	node		: AST Node where the generic type was declared
-- 	generic 	: Generic parameter list of the generic
--	context		: Context structure set before the traversal when instantiating the generic (created from the declaration context of the generic)
--	seekctx		: Seek context types set before the traversal when instantiating the generic (copy of the seek context types of the declaration of the generic)
--	const		: const attribute for generic functions/procedures
--	private		: private attribute for generic functions/procedures
--	throws		: throws (!nothrow) attribute for generic functions/procedures
-- Create the type definition of a generic
function defineGenericType( node, context, typnam, fmt, generic_param)
	local declContextTypeId = getDeclarationContextTypeId( context)
	local descr = utils.template_format( fmt, {name=typnam})
	descr.node = node
	descr.generic = generic_param
	descr.context = context
	descr.seekctx = getSeekContextTypes()
	local valtype = typedb:def_type( declContextTypeId, getQualifierTypeName( {}, typnam))
	local c_valtype = typedb:def_type( declContextTypeId, getQualifierTypeName( {const=true}, typnam))
	if valtype == -1 or c_valtype == -1 then utils.errorMessage( node.line, "Duplicate definition of generic type '%s'", typnam) end
	constTypeMap[ valtype] = c_valtype
	typeQualiSepMap[ valtype]    = {valtype=valtype,qualifier={const=false,reference=false,private=false}}
	typeQualiSepMap[ c_valtype]  = {valtype=valtype,qualifier={const=true,reference=false,private=false}}
	if valtype == -1 then utils.errorMessage( node.line, "Duplicate definition of type '%s'", typeDeclarationString( declContextTypeId, typnam)) end
	typeDescriptionMap[ valtype] = descr
	typeDescriptionMap[ c_valtype] = descr
	return descr
end
---- Lambda (descr) Fields ----
--	node		: AST Node of the code block of the lambda (not the declaration node as in a generic)
-- 	param	 	: Generic parameter list of the lambda
--	name		: Artificial name of the lambda (synthesized from the generic parameter names and a counter) used for internal type definitions and debug display
-- Create the type definition of a lambda expression
function defineLambdaExpression( node, param)
	local descr = utils.template_format( llvmir.lambdaExpressionDescr, {})
	local pstr = ""
	if param then for _,pp in ipairs(param) do pstr = pstr .. "__" .. pp end end
	local name = "lambda" .. pstr
	local typnam = utils.uniqueName( name .. "__")
	descr.node = node
	descr.name = name
	descr.param = param
	local lambdatype = typedb:def_type( localDefinitionContext, getQualifierTypeName( {}, typnam))
	typeDescriptionMap[ lambdatype] = descr
	return {type=lambdatype}
end
-- Add a generic parameter to the generic parameter list
function addGenericParameter( node, generic, name, typeId)
	if generic.namemap[ name] then utils.errorMessage( node.line, "Duplicate definition of generic parameter '%s'", name) end
	generic.namemap[ name] = #generic.param+1
	table.insert( generic.param, {name=name, type=typeId} )
end

---- Structure Context (context) Fields ----
--	domain		: Describes the kind of the context that determines how declarations like variables and functions are handled
--   Attributes in case of domain "member":
--	qualitype	: Names all types declared for a structure
--	descr		: LLVM IR Template parameters for mapping the structure
--	operators	: Maps the operator name ('+','-',...) to a structure with some attributes that are used to create an instance with a structure argument if possible
--				thisType   		: Type used as self parameter of the operator
--				returnType 		: Return type 
--				hasStructArgument 	: Tells if it is possible to create an operator instance with a structure argument
--				maxNofArguments 	: Maximum number of elements in a structure argument
--	methods		: List of methods/operators declared as members of the class/interface
--	methodmap	: Map for associate class and interface declarations of the same method/operator
--	llvmtype	: LLVM Declaration type string of the structure definition
--	symbol		: LLVM identifier used when referencing the type
-- 	const		: Declares if an interface has only const methods and therefore can be an interface of a const instance
--	structsize	: Structure size in bytes for class and struct
--	index		: LLVM Member variable index (starting with 0) for class and struct
--	private		: True if member variables are private (class), false if they are public (struct)
--	properties	: Structure with the flags {constructor,destructor,assignment} telling what elements are declared in a class and do not need an implicit default declaration

-- Structure Description (descr) Fields:
-- The fields methods,methodmap,const are inherited from context
--	size 		: Size in bytes

-- Traversal of an "interface" definition node, for interfaces there are no generics possible
function traverseAstInterfaceDef( node, declContextTypeId, typnam, descr, qualitype, nodeidx)
	pushSeekContextType( qualitype.valtype)
	defineUnboundReferenceTypes( declContextTypeId, typnam, descr, qualitype)
	local context = {domain="member", qualitype=qualitype, descr=descr, operators={}, methods={}, methodmap={},
				llvmtype="", symbol=descr.symbol, const=true}
	utils.traverseRange( typedb, node, {nodeidx,#node.arg}, context)
	descr.methods = context.methods
	descr.methodmap = context.methodmap
	descr.const = context.const
	defineInterfaceConstructorDestructors( node, qualitype, descr, context)
	defineOperatorsWithStructArgument( node, context)
	print_section( "Typedefs", utils.template_format( descr.vmtdef, {llvmtype=context.llvmtype}))
	print_section( "Typedefs", descr.typedef)
	popSeekContextType( qualitype.valtype)
end
-- Traversal of a "class" definition node, either directly in case of an ordinary class or on demand in case of a generic class
function traverseAstClassDef( node, declContextTypeId, typnam, descr, qualitype, nodeidx)
	pushSeekContextType( qualitype.valtype)
	defineUnboundReferenceTypes( declContextTypeId, typnam, descr, qualitype)
	definePublicPrivate( declContextTypeId, typnam, descr, qualitype)
	local context = {domain="member", qualitype=qualitype, descr=descr,
				members={}, properties={}, operators={}, methods={}, methodmap={}, interfaces={}, 
	                	llvmtype="", symbol=descr.symbol, structsize=0, index=0, private=true}
	utils.traverseRange( typedb, node, {nodeidx,#node.arg}, context, 1) -- 1st pass: define types: typedefs,classes,structures
	utils.traverseRange( typedb, node, {nodeidx,#node.arg}, context, 2) -- 2nd pass: define member variables
	descr.members = context.members
	descr.size = context.structsize
	utils.traverseRange( typedb, node, {nodeidx,#node.arg}, context, 3) -- 3rd pass: define callable headers
	if not context.properties.constructor then defineStructConstructors( node, qualitype, descr) end
	if not context.properties.destructor then defineStructDestructors( node, qualitype, descr) end
	definePartialClassDestructor( node, qualitype, descr)
	defineInitializationFromStructure( node, qualitype)
	if not context.properties.assignment then getAssignmentsFromConstructors( node, qualitype, descr) end
	defineOperatorsWithStructArgument( node, context)
	utils.traverseRange( typedb, node, {nodeidx,#node.arg}, context, 4) -- 4th pass: define callable implementations
	defineInheritedInterfaces( node, context, qualitype.valtype)
	print_section( "Typedefs", utils.template_format( descr.typedef, {llvmtype=context.llvmtype}))
	popSeekContextType( qualitype.valtype)
end
-- Traversal of a "struct" definition node, either directly in case of an ordinary structure or on demand in case of a generic structure
function traverseAstStructDef( node, declContextTypeId, typnam, descr, qualitype, nodeidx)
	pushSeekContextType( qualitype.valtype)
	defineUnboundReferenceTypes( declContextTypeId, typnam, descr, qualitype)
	local context = {domain="member", qualitype=qualitype, descr=descr, members={}, properties={},
				llvmtype="", symbol=descr.symbol, structsize=0, index=0, private=false}
	utils.traverseRange( typedb, node, {nodeidx,#node.arg}, context, 1) -- 1st pass: define types: typedefs,structures
	utils.traverseRange( typedb, node, {nodeidx,#node.arg}, context, 2) -- 2nd pass: define member variables
	descr.members = context.members
	descr.size = context.structsize
	defineStructConstructors( node, qualitype, descr)
	defineStructDestructors( node, qualitype, descr)
	defineInitializationFromStructure( node, qualitype)
	getAssignmentsFromConstructors( node, qualitype, descr)
	print_section( "Typedefs", utils.template_format( descr.typedef, {llvmtype=context.llvmtype}))
	popSeekContextType( qualitype.valtype)
end
-- Common part of traverseAstFunctionDeclaration,traverseAstProcedureDeclaration
function instantiateCallableDef( node, context, descr)
	if context.domain == "member" then
		defineClassMethod( node, descr, context)
		descr.interface = isInterfaceMethod( context, descr.methodid)
	else
		local callabletype = defineFreeFunction( node, descr, context)
		typeDescriptionMap[ callabletype] = llvmir.callableReferenceDescr( descr.symbolname, descr.ret, descr.throws)
	end
end
-- Traversal of a function declaration node, either directly in case of an ordinary function or on demand in case of a generic function
function traverseAstFunctionDeclaration( node, context, descr, nodeidx)
	descr.ret = table.unpack( utils.traverseRange( typedb, node, {nodeidx,nodeidx}, context))
	utils.traverseRange( typedb, node, {nodeidx+1,nodeidx+1}, context, descr, 1)
	instantiateCallableDef( node, context, descr)
end
-- Traversal of a function implementation node, either directly in case of an ordinary function or on demand in case of a generic function
function traverseAstFunctionImplementation( node, context, descr, nodeidx)
	utils.traverseRange( typedb, node, {nodeidx+1,nodeidx+1}, context, descr, 2)
	printFunctionDeclaration( node, descr)
end
-- Traversal of a procedure declaration node, either directly in case of an ordinary procedure or on demand in case of a generic procedure
function traverseAstProcedureDeclaration( node, context, descr, nodeidx)
	utils.traverseRange( typedb, node, {nodeidx,nodeidx}, context, descr, 1)
	instantiateCallableDef( node, context, descr)
end
-- Traversal of a procedure implementation node, either directly in case of an ordinary function or on demand in case of a generic function
function traverseAstProcedureImplementation( node, context, descr, nodeidx)
	utils.traverseRange( typedb, node, {nodeidx,nodeidx}, context, descr, 2)
	printFunctionDeclaration( node, descr)
end
-- Build a conditional if/elseif block
function conditionalIfElseBlock( node, initstate, condition, matchblk, elseblk, exitLabel)
	local cond_constructor = getRequiredTypeConstructor( node, controlTrueType, condition, tagmask_matchParameter, tagmask_typeConversion)
	if not cond_constructor then utils.errorMessage( node.line, "Can't use type '%s' as a condition", typedb:type_string(condition.type)) end
	local code = cond_constructor.code .. matchblk.code
	if initstate then -- in a constructor
		if elseblk then -- we level up the initialization state by adding default init constructors for the members missing initialization
			if elseblk.initstate > matchblk.initstate then
				matchblk.code = matchblk.code .. getImplicitInitCode( node, env, matchblk.initstate, elseblk.initstate)
			elseif elseblk.initstate < matchblk.initstate then
				elseblk.code = elseblk.code .. getImplicitInitCode( node, env, elseblk.initstate, matchblk.initstate)
			end
		elseif initstate < matchblk.initstate then -- no else block available, but initializations in matchblk, then create an elseblk with initializations leveled up by adding default init constructors
			elseblk = {code = getImplicitInitCode( node, env, initstate, matchblk.initstate)}
		end
	end
	local exitLabelUsed
	if matchblk.nofollow == true then
		code = code .. utils.template_format( llvmir.control.plainLabel, {inp=cond_constructor.out})
		exitLabelUsed = false
	elseif not exitLabel then
		code = code .. utils.template_format( llvmir.control.label, {inp=cond_constructor.out})
		exitLabelUsed = false
	else
		code = code .. utils.template_format( llvmir.control.invertedControlType, {inp=cond_constructor.out, out=exitLabel})
		exitLabelUsed = true
	end
	local nofollow = matchblk.nofollow and elseblk and elseblk.nofollow or false
	local out
	if elseblk then
		code = code .. elseblk.code
		out = elseblk.out
	else
		out = cond_constructor.out
	end
	return {code = code, out = out, exitLabelUsed=exitLabelUsed, nofollow=nofollow}
end
-- Collect the code of a scope with the destructor code for no jump exit of the frame and resume the initialization state if we are in a constructor
function collectCode( node, args)
	local code = ""
	local nofollow = false
	for _,arg in ipairs(args) do
		if arg then
			if nofollow == true then utils.errorMessage( node.line, "Unreachable code inside code block") end
			nofollow = arg.nofollow
			code = code .. arg.code
		end
	end
	local frame = thisAllocationFrame()
	local initstate
	if frame then
		if frame.initstate then
			resumeInitState( frame)
			initstate = frame.initstate
		end
		if not nofollow and frame.parent then code = code .. getAllocationFrameRegularExitCleanupCode( frame) end
	end
	return {code=code, nofollow=nofollow, initstate=initstate}
end
-- AST Callbacks:
local typesystem = {}
function typesystem.definition( node, pass, context, pass_selected)
	if not pass_selected or pass == pass_selected then
		local arg = table.unpack( utils.traverse( typedb, node, context or {domain="local"}))
		if arg then return {code = arg.constructor.code} else return {code=""} end
	end
end
function typesystem.definition_2pass( node, pass, context, pass_selected)
	if not pass_selected then
		return typesystem.definition( node, pass, context, pass_selected)
	elseif pass == pass_selected+1 then
		utils.traverse( typedb, node, context, 1) 	-- pass-1 (3rd) pass: declarations
	elseif pass == pass_selected then
		utils.traverse( typedb, node, context, 2)	-- pass-0 (4th) pass: implementations
	end
end
function typesystem.paramdef( node, context) 
	local datatype,varname = table.unpack( utils.traverse( typedb, node, context))
	return defineParameter( node, context, datatype, varname)
end
function typesystem.paramdeflist( node, context)
	return utils.traverse( typedb, node, context)
end
function typesystem.funcattribute( node, decl)
	local rt = table.unpack( utils.traverse( typedb, node, context)) or {}
	for key,val in pairs(decl) do rt[key] = val end
	return rt
end
function typesystem.structure( node)
	local args = utils.traverse( typedb, node)
	return {type=constexprStructureType, constructor={list=args,step=node.step}}
end
function typesystem.allocate( node)
	local env = getCallableEnvironment()
	local typeId,expression = table.unpack( utils.traverse( typedb, node))
	local pointerTypeId = pointerTypeMap[typeId]
	local refTypeId = referenceTypeMap[ typeId]
	if not refTypeId then utils.errorMessage( node.line, "Only non reference type allowed in %s (%s)", "new", typedb:type_string(typeId or 0)) end
	local descr = typeDescriptionMap[ typeId]
	local memblk = callFunction( node, {0}, "allocmem", {{type=constexprUIntegerType, constructor=bcd.int(descr.size)}})
	local ww,ptr_constructor = typedb:get_reduction( memPointerType, memblk.type, tagmask_resolveType)
	if not ww then utils.errorMessage( node.line, "Function '%s' not returning a pointer %s / %s", "allocmem",  typedb:type_string(memblk.type), typedb:type_string(memPointerType)) end
	if ptr_constructor then memblk.constructor = ptr_constructor( memblk.constructor) end
	local out = env.register()
	local cast = utils.constructor_format( llvmir.control.memPointerCast, {llvmtype=descr.llvmtype, out=out,this=memblk.constructor.out}, env.register)
	local this = {type=refTypeId,constructor={out=out, code=memblk.constructor.code .. cast}}
	local rt = applyCallable( node, this, ":=", {expression})
	rt.type = pointerTypeId
	return rt
end
function typesystem.typecast( node)
	local rt
	local env = getCallableEnvironment()
	local typeId,operand = table.unpack( utils.traverse( typedb, node))
	expectValueType( node, operand)
	local refTypeId = referenceTypeMap[ typeId]
	if not refTypeId then utils.errorMessage( node.line, "Only non reference type allowed in %s", "cast") end
	local descr = typeDescriptionMap[ typeId]
	if doCastToUnboundReferenceType( typeId) then 
		local out = "{RVAL}"
		rt = tryApplyCallable( node, {type=refTypeId,constructor={out=out}}, ":=", {operand})
		if not rt then utils.errorMessage( node.line, "Failed to cast '%s' from '%s'", typedb:type_string(typeId), typedb:type_string(operand.type)) end
		rt.constructor.out = "RVAL"
		rt.type = unboundReferenceTypeMap[ constTypeMap[ typeId] or typeId]
		rt.constructor.step = typedb:step()
	else
		local out = env.register()
		local code = utils.constructor_format( descr.def_local, {out=out}, env.register)
		rt = tryApplyCallable( node, {type=refTypeId,constructor={out=out,code=code}}, ":=", {operand})
		if not rt then utils.errorMessage( node.line, "Failed to cast '%s' from '%s'", typedb:type_string(typeId), typedb:type_string(operand.type)) end
		local cleanup = typeConstructorPairCode( tryApplyCallable( node, {type=refTypeId,constructor={out=out}}, ":~", {}))
		if cleanup then registerCleanupCode( "cast " .. typedb:type_name(typeId), cleanup, operand.constructor.step) end
	end
	return rt
end
function typesystem.throw_exception( node)
	local errcode,errmsg = table.unpack( utils.traverse( typedb, node))
	expectValueType( node, errcode)
	expectValueType( node, errmsg)
	local errcode_constructor = getRequiredTypeConstructor( node, scalarLongType, errcode, tagmask_matchParameter, tagmask_typeConversion)
	local errmsg_constructor = getRequiredTypeConstructor( node, stringPointerType, errmsg, tagmask_matchParameter, tagmask_typeConversion)
	return {code=getThrowExceptionCode( errcode_constructor, errmsg_constructor), nofollow=true}
end
function typesystem.tryblock( node, context, catchlabel)
	initTryBlock( catchlabel)
	return table.unpack( utils.traverse( typedb, node, context))
end
function typesystem.catchblock( node, context, catchlabel)
	local errcodevar,errmsgvar = table.unpack( utils.traverseRange( typedb, node, {1,#node.arg-1}, context))
	local env = getCallableEnvironment()
	local errcode_constructor = constructorStruct( env.register())
	local errmsg_constructor = constructorStruct( env.register())
	local errcode_type = typedb:def_type( localDefinitionContext, errcodevar, errcode_constructor)
	local errmsg_type = typedb:def_type( localDefinitionContext, errmsgvar, errmsg_constructor)
	if errcode_type == -1 then utils.errorMessage( node.line, "Duplicate definition of variable '%s'", errcodevar) end
	if errmsg_type == -1 then utils.errorMessage( node.line, "Duplicate definition of variable '%s'", errmsgvar) end
	typedb:def_reduction( scalarLongType, errcode_type, nil, tag_typeDeclaration)
	typedb:def_reduction( stringPointerType, errmsg_type, nil, tag_typeDeclaration)
	local code = utils.constructor_format( llvmir.exception.loadExceptionErrCode, {out=errcode_constructor.out}, env.register)
			.. utils.constructor_format( llvmir.exception.loadExceptionErrMsg, {out=errmsg_constructor.out}, env.register)
	local cleanup = utils.constructor_format( llvmir.exception.freeExceptionErrMsg, {this=errmsg_constructor.out})
	local scope_bk = typedb:scope( node.arg[ #node.arg].scope)
	registerCleanupCode( "exception message", cleanup)
	typedb:scope( scope_bk)
	local rt = table.unpack( utils.traverseRange( typedb, node, {#node.arg,#node.arg}, context))
	rt.code = code .. rt.code 
	return rt
end
function typesystem.trycatch( node, context)
	local env = getCallableEnvironment()
	if env.initstate then disallowInitCalls( node.line, "Member initializations not allowed in try/catch block") end
	local catchlabel = env.label()
	local tryblock,catchblock = table.unpack( utils.traverse( typedb, node, context, catchlabel))
	local code
	if not tryblock.nofollow then 
		local followlabel = env.label()
		local gotoFollow = utils.constructor_format( llvmir.control.gotoStatement, {inp=followlabel}, env.register)
		local labelFollow = utils.constructor_format( llvmir.control.label, {inp=followlabel}, env.register)
		code = tryblock.code .. gotoFollow .. utils.constructor_format( llvmir.control.plainLabel, {inp=catchlabel}) .. catchblock.code .. labelFollow
	else
		code = tryblock.code .. utils.constructor_format( fmt, {inp=catchlabel}) .. catchblock.code
	end
	return {code=code, nofollow=catchblock.nofollow}
end
function typesystem.delete( node)
	local env = getCallableEnvironment()
	local operand = table.unpack( utils.traverse( typedb, node))
	local typeId = operand.type
	local out,code = constructorParts( operand.constructor)
	local res = applyCallable( node, {type=typeId,constructor={code=code,out=out}}, " delete")
	local valueTypeId = pointeeTypeMap[ typedb:type_context( res.type)]
	local descr = typeDescriptionMap[ valueTypeId]
	local cast_out = env.register()
	local cast = utils.constructor_format( llvmir.control.bytePointerCast, {llvmtype=descr.llvmtype, out=cast_out, this=out}, env.register)
	local memblk = callFunction( node, {0}, "freemem", {{type=memPointerType, constructor={code=res.constructor.code .. cast,out=cast_out}}})
	return {code=memblk.constructor.code}
end
function typesystem.vardef( node, context)
	local datatype,varnam = table.unpack( utils.traverseRange( typedb, node, {1,2}, context))
	local initval = (#node.arg >= 3) and function() return table.unpack( utils.traverseRange( typedb, node, {3,3}, context)) end or nil
	return defineVariable( node, context, datatype, varnam, initval)
end
function typesystem.typedef( node, context)
	local typ,alias = table.unpack( utils.traverse( typedb, node))
	local declContextTypeId = getDeclarationContextTypeId( context)
	if typ.constructor then utils.errorMessage( node.line, "Data type expected on the left hand of a typedef declaration instead of '%s'", typedb:type_string(typ.type)) end
	defineTypeAlias( node, declContextTypeId, alias, typ.type)
end
function typesystem.assign_operator( node, operator)
	local target,operand = table.unpack( utils.traverse( typedb, node))
	local assign_operand = applyCallable( node, target, operator, {operand})
	expectValueType( node, target)
	return applyCallable( node, typeConstructorPairStruct( target.type, (constructorParts( target.constructor))), "=", {assign_operand})
end
function typesystem.binop( node, operator)
	local this,operand = table.unpack( utils.traverse( typedb, node))
	expectValueType( node, this)
	expectValueType( node, operand)
	return applyCallable( node, this, operator, {operand})
end
function typesystem.unop( node, operator)
	local this = table.unpack( utils.traverse( typedb, node))
	expectValueType( node, this)
	return applyCallable( node, this, operator, {})
end
function typesystem.operator( node, operator)
	local args = utils.traverse( typedb, node)
	local this = table.remove( args, 1)
	return applyCallable( node, this, operator, args)
end
function typesystem.operator_address( node, operator)
	local this = table.unpack( utils.traverse( typedb, node))
	expectValueType( node, this)
	local declType = getDeclarationType( this.type)
	local qs = typeQualiSepMap[ declType]
	if not pointerTypeMap[ qs.valtype] then createPointerTypeInstance( node, {const=true}, qs.valtype) end -- the created pointer type is const
	return applyCallable( node, this, operator, {})
end
function typesystem.operator_array( node, operator)
	local args = utils.traverse( typedb, node)
	local this = table.remove( args, 1)
	local descr = typeDescriptionMap[ this.type]
	if descr then
		if descr.class == "generic_procedure" or descr.class == "generic_function" then
			local gentype = getOrCreateGenericType( node, this.type, descr, args)
			local gentypnam = typedb:type_name(gentype)
			local seekctx = getSeekContextTypes()
			local resolveContextTypeId, reductions, items = typedb:resolve_type( seekctx, gentypnam, tagmask_resolveType)
			local typeId,constructor = selectNoArgumentType( node, seekctx, gentypnam, resolveContextTypeId, reductions, items)
			return {type=typeId, constructor=constructor}
		elseif descr.class == "lambda" then
			local constructor = applyLambda( node, this.type, descr, args)
			return {type=lambdaResultType, constructor=constructor}
		end
	end
	return applyCallable( node, this, operator, args)
end
function typesystem.free_expression( node)
	local expression = table.unpack( utils.traverse( typedb, node))
	if expression.type == controlTrueType or expression.type == controlFalseType then
		return {code=expression.constructor.code .. utils.constructor_format( llvmir.control.label, {inp=expression.constructor.out})}
	else
		return {code=expression.constructor.code}
	end
end
function typesystem.codeblock( node)
	return collectCode( node, utils.traverse( typedb, node))
end
function typesystem.return_value( node)
	local operand = table.unpack( utils.traverse( typedb, node))
	expectValueType( node, operand)
	local env = getCallableEnvironment()
	local rtype = env.returntype
	if rtype == 0 then utils.errorMessage( node.line, "Can't return value from procedure") end
	if doReturnValueAsReferenceParameter( rtype) then
		local reftype = referenceTypeMap[ rtype]
		local rt
		if operand.constructor.out == "%rt" then
			rt = operand -- copy elision through in-place construction done in 'defineVariable(..)'
		else
			rt = applyCallable( node, typeConstructorPairStruct( reftype, "%rt", ""), ":=", {operand})
		end
		return {code= rt.constructor.code .. doReturnVoidStatement(), nofollow=true}
	else
		local constructor = getRequiredTypeConstructor( node, rtype, operand, tagmask_matchParameter, tagmask_typeConversion)
		local code = env.returnfunction and env.returnfunction( constructor) or doReturnTypeStatement( rtype, constructor)
		return {code=code, nofollow=true}
	end
end
function typesystem.return_void( node)
	local env = getCallableEnvironment()
	local rtype = env.returntype
	if rtype ~= 0 then utils.errorMessage( node.line, "Can't return without value from function") end
	local code = ""
	if env.initstate then code = code .. completeCtorInitializationCode( false) end
	return {code = code .. doReturnVoidStatement(), nofollow=true}
end
function typesystem.conditional_else( node, exitLabel)
	return table.unpack( utils.traverse( typedb, node))
end
function typesystem.conditional_elseif( node, exitLabel)
	local env = getCallableEnvironment()
	local initstate
	if env.initstate then -- we are in a constructor, so we calculate the init state to pass it to conditionalIfElseBlock
		local frame = getAllocationFrame()
		if frame then initstate = frame.initstate else initstate = env.initstate end
	end
	local condition,yesblock,noblock = table.unpack( utils.traverse( typedb, node, exitLabel))
	return conditionalIfElseBlock( node, initstate, condition, yesblock, noblock, exitLabel)
end
function typesystem.conditional_if( node)
	local env = getCallableEnvironment()
	local exitLabel = (#node.arg >= 3) and env.label() or nil
	local initstate
	if env.initstate then -- we are in a constructor, so we calculate the init state to pass it to conditionalIfElseBlock
		local frame = getAllocationFrame()
		if frame then initstate = frame.initstate else initstate = env.initstate end
	end
	local condition,yesblock,noblock = table.unpack( utils.traverse( typedb, node, exitLabel))
	local rt = conditionalIfElseBlock( node, initstate, condition, yesblock, noblock, exitLabel)
	if rt.exitLabelUsed == true then rt.code = rt.code .. utils.constructor_format( llvmir.control.label, {inp=exitLabel}) end
	return rt
end
function typesystem.conditional_while( node)
	local env = getCallableEnvironment()
	if env.initstate then disallowInitCalls( node.line, "Member initializations not allowed inside a loop", node.arg[2].scope) end
	local condition,yesblock = table.unpack( utils.traverse( typedb, node))
	local cond_constructor = getRequiredTypeConstructor( node, controlTrueType, condition, tagmask_matchParameter, tagmask_typeConversion)
	if not cond_constructor then utils.errorMessage( node.line, "Can't use type '%s' as a condition", typedb:type_string(condition.type)) end
	local start = env.label()
	local startcode = utils.constructor_format( llvmir.control.label, {inp=start})
	return {code = startcode .. cond_constructor.code .. yesblock.code 
			.. utils.constructor_format( llvmir.control.invertedControlType, {out=start,inp=cond_constructor.out})}
end
function typesystem.with_do( node)
	local operand = table.unpack( utils.traverseRange( typedb, node, {1,1}))
	pushSeekContextType( operand)
	if #node.arg >= 2 then return table.unpack( utils.traverseRange( typedb, node, {2,2})) end
end
function typesystem.typehdr( node, qualifier)
	return resolveTypeFromNamePath( node, qualifier, utils.traverse( typedb, node))
end
function typesystem.typehdr_any( node, typeName)
	return {type=hardcodedTypeMap[ typeName]}
end
function typesystem.typegen_generic( node, context)
	local typeInstance = nil
	local genericType,instArg = table.unpack( utils.traverse( typedb, node, context))
	if genericType.constructor then utils.errorMessage( node.line, "Data type expected on left hand of generic or array declaration instead of '%s'", typedb:type_string(genericType.type)) end
	local qs = typeQualiSepMap[ genericType.type]
	if not qs then utils.errorMessage( node.line, "Non generic/array type with instantiation operator '[ ]'", typedb:type_string(genericType.type)) end
	local genericDescr = typeDescriptionMap[ qs.valtype]
	if string.sub(genericDescr.class,1,8) == "generic_" then
		typeInstance = getOrCreateGenericType( node, qs.valtype, genericDescr, instArg)
	elseif #instArg == 1 then -- generic operator on non generic type creates an array
		typeInstance = getOrCreateArrayType( node, qs.valtype, genericDescr, instArg[1])
	else
		utils.errorMessage( node.line, "Operator '[' ']' with more than one argument for non generic %s", genericDescr.class)
	end
	if qs.qualifier.const == true then typeInstance = constTypeMap[ typeInstance] end -- const qualifier transfered to array type created
	return {type=typeInstance}
end
function typesystem.typegen_pointer( node, attr, context)
	local pointeeType = table.unpack( utils.traverse( typedb, node, context))
	local pointeeTypeId = pointeeType.type
	if pointeeType.constructor then utils.errorMessage( node.line, "Data type expected to create pointer from (operator '^') instead of '%s'", typedb:type_string(pointeeTypeId)) end
	local typeInstance = pointerTypeMap[ pointeeTypeId]
	if not typeInstance then
		local qs = typeQualiSepMap[ pointeeTypeId]
		if not qs then utils.errorMessage( node.line, "Cannot create pointer type for type '%s'", typedb:type_string(pointeeTypeId)) end
		typeInstance = createPointerTypeInstance( node, attr, pointeeTypeId)
	end
	return {type=typeInstance}
end
function typesystem.typespec( node)
	return expectDataType( node, table.unpack( utils.traverse( typedb, node)))
end
function typesystem.typespec_ref( node, qualifier)
	local datatype = table.unpack( utils.traverse( typedb, node))
	if datatype.constructor then utils.errorMessage( node.line, "Data type expected as data type specifier instead of '%s'", typedb:type_string(datatype.type)) end
	return referenceTypeMap[ datatype.type] or utils.errorMessage( node.line, "Type '%s' has no reference type", typedb:type_string(datatype.type))
end
function typesystem.constant( node, typeName)
	local typeId = hardcodedTypeMap[ typeName]
	return {type=typeId, constructor=createConstexprValue( typeId, node.arg[1].value)}
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
	local env = getCallableEnvironment()
	local seekctx = getSeekContextTypes()
	local resolveContextTypeId, reductions, items = typedb:resolve_type( seekctx, typeName, tagmask_resolveType)
	local typeId,constructor = selectNoArgumentType( node, seekctx, typeName, resolveContextTypeId, reductions, items)
	local variableScope = typedb:type_scope( typeId)
	if variableScope[1] == 0 or env.scope[1] <= variableScope[1] then
		return {type=typeId, constructor=constructor}
	else
		utils.errorMessage( node.line, "Not allowed to access variable '%s' that is not defined in local function or global scope", typedb:type_string(typeId))
	end
end
function typesystem.linkage( node, llvm_linkage, context)
	if context.domain == "local" and typedb:scope()[0] ~= 0 then -- local function
		if llvm_linkage.explicit == true and llvm_linkage.private == false then -- declared public
			utils.errorMessage( node.line, "public declaration not allowed in local context")
		end
		llvm_linkage.private = true
		llvm_linkage.linkage = "internal"
	end
	return llvm_linkage
end
---- Callable Description (descr) Fields ----
--	externtype	: Identifier for extern function defitions that determine some properties like the calling convention, currently only "C" known.
--	name		: Name of the callable that may contain non alphanumeric characters, not suitable for output, but comprehensible for tracing/debugging
--	symbol		: Symbol name of the callable, suitable for output
--	ret		: Return type in case of a function, otherwise undefined
--	param		: List of declared parameters of the callable
--	vararg		: Boolean flag, true in case of a function with a variable number of arguments, parameter list ending with "..."
--	private		: Boolean flag, true in case of a callable that is a private member of the class
--	const		: Boolean flag, true in case of a callable that can be called for a class declared as const 
--	throws		: Boolean flag, true in case of a callable that may throw an exception
--	interface	: Boolean flag, true in case of a method implementing a method that is defined by an interface
--	signature	: Signature string of the callable for casts and for calling functions with a variable number of arguments (vararg = true)
--	index		: Index of a method in the VMT of an interface, undefined for others than interfaces
--	llvmthis	: LLVM Type used for the this pointer in the parameter list of an interface method call, undefined for others than interfaces
--	load 		: Load format string for loading the method from the method table, undefined for others than interfaces
--	lnk		: Some attributes describing the visibility/compiling/linking of the callable
--	attr  		: LLVM function attribute string
--	node 		: Node that declared the generic callable, undefined for others than generics
--	generic 	: List of arguments of the generic, undefined for others than generics
--	context 	: Some subset of the context where the generic was declared, undefined for others than generics
--	seekctx 	: Seek context types at the time when the generic was declared, undefined for others than generics
--	env		: Callable environment of the function

function typesystem.funcdef( node, context, pass)
	if not pass or pass == 1 then
		local lnk,name = table.unpack( utils.traverseRange( typedb, node, {1,2}, context))
		local descr = {lnk = lnk.linkage, signature="", name=name, symbol=name, 
				private=lnk.private, const=false, throws=false, interface=false}
		utils.traverseRange( typedb, node, {4,4}, context, descr, 0) -- decl {const,throws}
		if descr.const == true and context.domain ~= "member" then utils.errorMessage( node.line, "Using 'const' attribute in free function declaration") end
		utils.allocNodeData( node, localDefinitionContext, descr)
		traverseAstFunctionDeclaration( node, context, descr, 3)
	end
	if not pass or pass == 2 then
		local descr = utils.getNodeData( node, localDefinitionContext)
		traverseAstFunctionImplementation( node, context, descr, 3)
	end
end
function typesystem.procdef( node, context, pass)
	if not pass or pass == 1 then
		local lnk,name = table.unpack( utils.traverseRange( typedb, node, {1,2}, context))
		local descr = {lnk = lnk.linkage, signature="", name = name, symbol = name, 
				private=lnk.private, const=false, throws=false, interface=false}
		utils.traverseRange( typedb, node, {3,3}, context, descr, 0) -- decl {const,throws}
		if descr.const == true and context.domain ~= "member" then utils.errorMessage( node.line, "Using 'const' attribute in free procedure declaration") end
		utils.allocNodeData( node, localDefinitionContext, descr)
		traverseAstProcedureDeclaration( node, context, descr, 3)
	end
	if not pass or pass == 2 then
		local descr = utils.getNodeData( node, localDefinitionContext)
		traverseAstProcedureImplementation( node, context, descr, 3)
	end
end
function typesystem.generic_funcdef( node, context, pass)
	if not pass or pass == 1 then
		local lnk,name,generic_arg = table.unpack( utils.traverseRange( typedb, node, {1,3}, context))
		local descr = defineGenericType( node, context, name, llvmir.genericFunctionDescr, generic_arg)
		utils.traverseRange( typedb, node, {5,5}, context, descr, 0) -- decl {const,throws}
		if descr.const == true and context.domain ~= "member" then utils.errorMessage( node.line, "Using 'const' attribute in generic free function declaration") end
		descr.private = lnk.linkage.private
	end
end
function typesystem.generic_procdef( node, context, pass)
	if not pass or pass == 1 then
		local lnk,name,generic_arg = table.unpack( utils.traverseRange( typedb, node, {1,3}, context))
		local descr = defineGenericType( node, context, name, llvmir.genericProcedureDescr, generic_arg)
		utils.traverseRange( typedb, node, {4,4}, context, descr, 0) -- decl {const,throws}
		if descr.const == true and context.domain ~= "member" then utils.errorMessage( node.line, "Using 'const' attribute in generic free procedure declaration") end
		descr.private = lnk.linkage.private
	end
end
function typesystem.operatordecl( node, opr)
	return opr
end
function typesystem.operator_funcdef( node, context, pass)
	if not pass or pass == 1 then
		local lnk, operatordecl, ret = table.unpack( utils.traverseRange( typedb, node, {1,3}, context))
		local descr = {lnk = lnk.linkage, signature="", name=operatordecl.name, symbol ="$"..operatordecl.symbol, 
				ret=ret, private=lnk.private, const=false, throws=false, interface=false}
		utils.traverseRange( typedb, node, {4,4}, context, descr, 0) -- decl {const,throws}
		utils.traverseRange( typedb, node, {4,4}, context, descr, 1) -- parameter
		descr.interface = isInterfaceMethod( context, descr.methodid)
		defineClassOperator( node, descr, context)
		utils.allocNodeData( node, localDefinitionContext, descr)
	end
	if not pass or pass == 2 then
		local descr = utils.getNodeData( node, localDefinitionContext)
		utils.traverseRange( typedb, node, {4,4}, context, descr, 2)
		printFunctionDeclaration( node, descr)
	end
end
function typesystem.operator_procdef( node, context, pass)
	if not pass or pass == 1 then
		local lnk, operatordecl = table.unpack( utils.traverseRange( typedb, node, {1,2}, context))
		local descr = {lnk = lnk.linkage, signature="", name = operatordecl.name, symbol = "$" .. operatordecl.symbol, 
				ret = nil, private=lnk.private, const=false, throws=false, interface=false}
		utils.traverseRange( typedb, node, {3,3}, context, descr, 0) -- decl {const,throws}
		utils.traverseRange( typedb, node, {3,3}, context, descr, 1) -- parameter
		descr.interface = isInterfaceMethod( context, descr.methodid)
		defineClassOperator( node, descr, context)
		utils.allocNodeData( node, localDefinitionContext, descr)
	end
	if not pass or pass == 2 then
		local descr = utils.getNodeData( node, localDefinitionContext)
		utils.traverseRange( typedb, node, {3,3}, context, descr, 2)
		printFunctionDeclaration( node, descr)
	end
end
function typesystem.constructordef( node, context, pass)
	if not pass or pass == 1 then
		local lnk = table.unpack( utils.traverseRange( typedb, node, {1,1}, context))
		local descr = {lnk = lnk.linkage, signature="", name = ":=", symbol = "$ctor", 
				ret = nil, private=lnk.private, const=false, throws=false, interface=false}
		utils.traverseRange( typedb, node, {2,2}, context, descr, 0) -- decl {const,throws}
		if descr.const == true then utils.errorMessage( node.line, "Using 'const' attribute in constructor declaration") end
		utils.traverseRange( typedb, node, {2,2}, context, descr, 1) -- parameter
		context.properties.constructor = true
		defineCtorProcedure( node, descr, context)
		utils.allocNodeData( node, localDefinitionContext, descr)
	end
	if not pass or pass == 2 then
		local descr = utils.getNodeData( node, localDefinitionContext)
		utils.traverseRange( typedb, node, {2,2}, context, descr, 2)
		printFunctionDeclaration( node, descr)
	end
end
function typesystem.destructordef( node, lnk, context, pass)
	local subcontext = {domain="local"}
	if not pass or pass == 1 then
		local descr = {lnk = lnk.linkage, signature="", name = ":~", symbol = "$dtor", 
		               ret = nil, param={}, private=false, const=false, throws=false, interface=false}
		context.properties.destructor = true
		defineDtorProcedure( node, descr, context)
		utils.allocNodeData( node, localDefinitionContext, descr)
	end
	if not pass or pass == 2 then
		local scope_bk = typedb:scope( node.arg[1].scope)
		local descr = utils.getNodeData( node, localDefinitionContext)
		local env = defineCallableEnvironment( node, "body " .. descr.symbol, descr.ret, descr.throws)
		if context.domain == "member" then expandMethodEnvironment( node, context, descr, env) end
		if descr.throws then getOrCreateThisAllocationFrame() end -- define the toplevel allocation frame if it throws
		local block = table.unpack( utils.traverse( typedb, node, subcontext))
		local code = block.code .. getStructMemberDestructorCode( env, context.descr, context.members)
		if not block.nofollow then code = code .. doReturnVoidStatement() end
		descr.body = getCallableEnvironmentCodeBlock( env, code)
		descr.env = env
		typedb:scope( scope_bk)
		printFunctionDeclaration( node, descr)
	end
end
function typesystem.callablebody( node, context, descr, selectid)
	local subcontext = {domain="local"}
	if selectid == 0 then -- function attributes {const,nothrow}
		local decl = table.unpack( utils.traverseRange( typedb, node, {2,2}, subcontext))
		descr.const = decl.const
		descr.throws = decl.throws
	elseif selectid == 1 then -- parameter declarations
		descr.env = defineCallableEnvironment( node, "body " .. descr.symbol, descr.ret, descr.throws)
		if context.domain == "member" then expandMethodEnvironment( node, context, descr, descr.env) end
		if descr.throws then getOrCreateThisAllocationFrame() end -- define the toplevel allocation frame if it throws
		descr.param = table.unpack( utils.traverseRange( typedb, node, {1,1}, subcontext))
	elseif selectid == 2 then -- statements in body
		local env = descr.env
		local args = utils.traverseRange( typedb, node, {3,#node.arg}, subcontext)
		local block = collectCode( node, args)
		local code = block.code
		if descr.ret then
			if not block.nofollow then utils.errorMessage( node.line, "Missing return value") end
		else
			if env.initstate then code = code .. completeCtorInitializationCode( block.nofollow) end
			if not block.nofollow then code = code .. doReturnVoidStatement() end
		end
		descr.body = getCallableEnvironmentCodeBlock( env, code)
	end
end
function typesystem.extern_paramdef( node, context, args)
	return table.unpack( utils.traverseRange( typedb, node, {1,1}, context))
end
function typesystem.extern_paramdef_collect( node, context, args)
	table.insert( args, table.unpack( utils.traverseRange( typedb, node, {1,1}, context)))
	if #node.arg > 1 then utils.traverseRange( typedb, node, {2,2}, context, args) end
end
function typesystem.extern_paramdeflist( node, context)
	local args = {}
	utils.traverse( typedb, node, context, args)
	local rt = {}
	for ai,arg in ipairs(args) do
		local llvmtype = typeDescriptionMap[ arg].llvmtype
		table.insert( rt, {type=arg,llvmtype=llvmtype} )
	end
	return rt
end
function typesystem.extern_funcdef( node)
	local context = {domain="global"}
	local lang,name,ret,param = table.unpack( utils.traverse( typedb, node, context))
	local descr = {externtype=lang, name=name, symbol=name, ret=ret, param=param, vararg=false, signature=""}
	defineExternFunction( node, descr, context)
	print_section( "Typedefs", llvmir.externFunctionDeclaration( lang, descr.rtllvmtype, name, descr.argstr, false))
end
function typesystem.extern_procdef( node)
	local context = {domain="global"}
	local lang,name,param = table.unpack( utils.traverse( typedb, node, context))
	local descr = {externtype=lang, name=name, symbol=name, param=param, vararg=false, signature=""}
	defineExternFunction( node, descr, context)
	print_section( "Typedefs", llvmir.externFunctionDeclaration( lang, "void", name, descr.argstr, false))
end
function typesystem.extern_funcdef_vararg( node)
	local context = {domain="global"}
	local lang,name,ret,param = table.unpack( utils.traverse( typedb, node, context))
	local descr = {externtype=lang, name=name, symbol=name, ret=ret, param=param, vararg=true, signature=""}
	defineExternFunction( node, descr, context)
	print( llvmir.externFunctionDeclaration( lang, descr.rtllvmtype, name, descr.argstr, true))
end
function typesystem.extern_procdef_vararg( node)
	local context = {domain="global"}
	local lang,name,param = table.unpack( utils.traverse( typedb, node, context))
	local descr = {externtype=lang, name=name, symbol=name, param=param, vararg=true, signature=""}
	defineExternFunction( node, descr, context)
	print( llvmir.externFunctionDeclaration( lang, "void", name, descr.argstr, true))
end
function typesystem.typedef_functype( node, decl, context)
	local name,ret,param = table.unpack( utils.traverse( typedb, node, context))
	local descr = {ret=ret, param=param, name=name, signature="", throws=decl.throws}
	defineFunctionVariableType( node, descr, context)
end
function typesystem.typedef_proctype( node, decl, context)
	local name,param = table.unpack( utils.traverse( typedb, node, context))
	local descr = {ret = nil, param=param, name=name, signature="", throws=decl.throws}
	defineProcedureVariableType( node, descr, context)
end
function typesystem.interface_funcdef( node, context)
	local name,ret,param,decl = table.unpack( utils.traverse( typedb, node, context))
	local descr = {name=name, symbol=name, ret=ret, param=param, signature="", private=false, const=decl.const, throws=decl.throws,
	               index=#context.methods, llvmthis="i8", load = context.descr.loadVmtMethod}
	defineInterfaceMethod( node, descr, context)
end
function typesystem.interface_procdef( node, context)
	local name,param,decl = table.unpack( utils.traverse( typedb, node, context))
	local descr = {name=name, symbol=name, ret=nil, param=param, signature="", private=false, const=decl.const, throws=decl.throws,
	               index=#context.methods, llvmthis="i8", load = context.descr.loadVmtMethod}
	defineInterfaceMethod( node, descr, context)
end
function typesystem.interface_operator_funcdef( node, context)
	local opr,ret,param,decl = table.unpack( utils.traverse( typedb, node, context))
	local descr = {name=opr.name, symbol = "$"..opr.symbol, ret=ret, param=param, signature="", private=false, const=decl.const, throws=decl.throws,
			index=#context.methods, llvmthis="i8", load = context.descr.loadVmtMethod}
	defineInterfaceOperator( node, descr, context)
end
function typesystem.interface_operator_procdef( node, context)
	local opr,param,decl = table.unpack( utils.traverse( typedb, node, context))
	local descr = {name=opr.name, symbol="$"..opr.symbol, ret=nil, param=param, signature="", private=false, const=decl.const, throws=decl.throws,
	               index=#context.methods, llvmthis="i8", load=context.descr.loadVmtMethod}
	defineInterfaceOperator( node, descr, context)
end
function typesystem.namespacedef( node, context)
	local typnam = node.arg[1].value
	local declContextTypeId = getDeclarationContextTypeId( context)
	local scope_bk,step_bk = typedb:scope( {} ) -- set global scope
	local namespace = typedb:get_type( declContextTypeId, typnam) or typedb:def_type( declContextTypeId, typnam)
	typedb:scope( scope_bk,step_bk)
	pushSeekContextType( namespace)
	utils.traverseRange( typedb, node, {2,2}, {domain="global", namespace = namespace})
	popSeekContextType( namespace)
end
function typesystem.structdef( node, context)
	local typnam = node.arg[1].value
	local declContextTypeId = getDeclarationContextTypeId( context)
	local descr,qualitype = defineStructureType( node, declContextTypeId, typnam, llvmir.structTemplate)
	traverseAstStructDef( node, declContextTypeId, typnam, descr, qualitype, 2)
end
function typesystem.generic_structdef( node, context)
	local typnam = node.arg[1].value
	local param = table.unpack( utils.traverseRange( typedb, node, {2,2}, context))
	defineGenericType( node, context, typnam, llvmir.genericStructDescr, param)
end
function typesystem.interfacedef( node, context)
	local typnam = node.arg[1].value
	local declContextTypeId = getDeclarationContextTypeId( context)
	local descr,qualitype = defineStructureType( node, declContextTypeId, typnam, llvmir.interfaceTemplate)
	traverseAstInterfaceDef( node, declContextTypeId, typnam, descr, qualitype, 2)
end
function typesystem.generic_instance_dimension( node, context)
	return {type=constexprUIntegerType, constructor=node.arg[1].value}
end
function typesystem.generic_instance_deflist( node, context, generic_instancelist)
	table.insert( generic_instancelist, table.unpack( utils.traverseRange( typedb, node, {1,1}, context)) )
	if #node.arg > 1 then utils.traverseRange( typedb, node, {2,2}, context, generic_instancelist) end
end
function typesystem.generic_instance( node, context)
	local rt = {}
	utils.traverse( typedb, node, context, rt)
	return rt
end
function typesystem.generic_header_ident_type( node, context, generic)
	local gentype,gendefault = table.unpack( utils.traverseRange( typedb, node, {1,2}, context))
	if gendefault.constructor then utils.errorMessage( node.line, "Data type expected instead of '%s' as default generic parameter", typedb:type_string(arg[2].type)) end
	addGenericParameter( node, generic, gentype, gendefault.type)
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
function typesystem.lambda_paramdeflist( node)
	return utils.traverse( typedb, node)
end
function typesystem.lambda_expression( node)
	local param = table.unpack( utils.traverseRange( typedb, node, {1,1}, context))
	return defineLambdaExpression( node.arg[2], param)
end
function typesystem.classdef( node, context)
	local typnam = node.arg[1].value
	local declContextTypeId = getDeclarationContextTypeId( context)
	local descr,qualitype = defineStructureType( node, declContextTypeId, typnam, llvmir.classTemplate)
	traverseAstClassDef( node, declContextTypeId, typnam, descr, qualitype, 2)
end
function typesystem.generic_classdef( node, context)
	local typnam = node.arg[1].value
	local generic = table.unpack( utils.traverseRange( typedb, node, {2,2}, context))
	defineGenericType( node, context, typnam, llvmir.genericClassDescr, generic)
end
function typesystem.inheritdef( node, pass, context, pass_selected)
	if not pass_selected or pass == pass_selected then
		local operand = table.unpack( utils.traverseRange( typedb, node, {1,1}, context))
		local typeId = operand.type
		if operand.constructor then utils.errorMessage( node.line, "Data type expected instead of '%s' to inherit from", typedb:type_string(typeId)) end
		local descr = typeDescriptionMap[ typeId]
		local typnam = typedb:type_name(typeId)
		local private = false
		if descr.class == "class" then
			defineMemberVariable( node, descr, context, typeId, typnam, private)
			defineClassInheritanceReductions( context, typnam, private, typeId)
		elseif descr.class == "interface" then
			defineInterfaceMember( node, context, typeId, typnam, private)
			defineInterfaceInheritanceReductions( context, typnam, private, descr.const)
		else
			utils.errorMessage( node.line, "Inheritance only allowed from class or interface, not from '%s'", descr.class)
		end
		if #node.arg > 1 then utils.traverseRange( typedb, node, {2,2}, context, pass_selected) end
	end
end
function typesystem.count( node)
 	if #node.arg == 0 then return 1 else return utils.traverseCall( node)[ 1] + 1 end
end
function typesystem.rep_operator( node)
	local expr,icount,name = table.unpack( utils.traverse( typedb, node))
	for ii=1,icount do expr = applyCallable( node, expr, "->") end
	return applyCallable( node, expr, name)
end
function typesystem.member( node)
	local struct,name = table.unpack( utils.traverse( typedb, node))
	return applyCallable( node, struct, name)
end
function typesystem.main_procdef( node)
	local context = {domain="local"}
	local scope_bk = typedb:scope( node.arg[1].scope)
	local env = defineCallableEnvironment( node, "main ", scalarIntegerType, true)
	local exitlabel = env.label()
	local retvaladr = env.register()
	local retvalinit = utils.constructor_format( typeDescriptionMap[ scalarIntegerType].def_local, {out=retvaladr}, env.register)
	env.returnfunction = returnFromMainFunction( scalarIntegerType, exitlabel, retvaladr)
	defineMainProcVariables( node, context)
	local block = table.unpack( utils.traverse( typedb, node, context))
	local code = globalInitCode .. retvalinit .. block.code
	if not block.nofollow then code = code .. env.returnfunction( constructorStruct("0")) end
	if globalCallableEnvironment.throws then enableFeatureException( env) end
	local body = getCallableEnvironmentCodeBlock( env, code)
		.. utils.template_format( llvmir.control.plainLabel, {inp=exitlabel}) 
		.. getAllocationFrameRegularExitCleanupCode( globalAllocationFrame)
		.. utils.constructor_format( llvmir.control.returnFromMain, {this=retvaladr}, env.register)
		.. getAllocationFrameAbortCleanupCode( globalAllocationFrame)
	if globalCallableEnvironment.throws then
		local errcode = env.register()
		body = body .. utils.constructor_format( llvmir.exception.catch, {landingpad="abort"}, env.register)
			.. utils.constructor_format( llvmir.exception.returnExceptionErrCode, {}, env.register)
	end
	typedb:scope( scope_bk)
	print( "\n" .. utils.constructor_format( llvmir.control.mainDeclaration, {body=body}))
end
function typesystem.program( node)
	llvmir.init()
	initBuiltInTypes()
	globalCallableEnvironment = createCallableEnvironment( node, "globals ", nil, false, "%ir", "IL")
	globalAllocationFrame = createAllocationFrame( globalCallableEnvironment, typedb:scope())
	globalAllocationFrame.catch = globalAllocationFrame -- there is not really a try/catch but we catch the exceptions of global initializations in the main (label "abort")
	utils.traverse( typedb, node, {domain="global"})
	return node
end

return typesystem
