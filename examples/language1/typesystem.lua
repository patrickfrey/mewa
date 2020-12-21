local mewa = require "mewa"
local llvmir = require "language1/llvmir"
local utils = require "typesystem_utils"
local bcd = require "bcd"

local typedb = mewa.typedb()
local typesystem = {
	arrow = {},
	member = {},
	ptrderef = {},
	call = {},
	arrayaccess = {},
}

local tag_typeDeduction = 1
local tag_typeDeclaration = 2
local tag_TypeConversion = 3
local tag_typeNamespace = 4
local tagmask_resolveType = typedb.reduction_tagmask( tag_typeDeduction, tag_typeDeclaration)
local tagmask_matchParameter = typedb.reduction_tagmask( tag_typeDeduction, tag_typeDeclaration, tag_TypeConversion)
local tagmask_typeConversion = typedb.reduction_tagmask( tag_TypeConversion)
local tagmask_typeNameSpace = typedb.reduction_tagmask( tag_typeNamespace)

function constructorParts( constructor)
	if type(constructor) == "table" then return constructor.code,constructor.out else return "",tostring(constructor) end
end
function convConstructor( fmt)
	return function( constructor)
		local callable = typedb:get_instance( "callable")
		local out = callable.register()
		local code,inp = constructorParts( constructor)
		return {code = code .. utils.constructor_format( fmt, {inp = inp, out = out}, callable.register), out = out}
	end
end
function assignConstructor( fmt)
	return function( this, arg)
		local code = ""
		local this_code,this_inp = constructorParts( this)
		local arg_code,arg_inp = constructorParts( arg[1])
		local subst = {arg1 = arg_inp, this = this_inp}
		code = code .. this_code .. arg_code
		return {code = code .. utils.constructor_format( fmt, subst), out = this_inp}
	end
end
function callConstructor( fmt)
	return function( this, arg)
		local callable = typedb:get_instance( "callable")
		local out = callable.register()
		local code = ""
		local this_code,this_inp = constructorParts( this)
		local subst = {out = out, this = this_inp}
		code = code .. this_code
		for ii=1,#arg do
			local arg_code,arg_inp = constructorParts( arg[ ii])
			code = code .. arg_code
			subst[ "arg" .. ii] = arg_inp
		end
		return {code = code .. utils.constructor_format( fmt, subst, callable.register), out = out}
	end
end
function promoteCallConstructor( call_constructor, promote_constructor)
	return function( this, arg)
		return call_constructor( promote_constructor( this), arg)
	end
end

local weightEpsilon = 1E-5	-- epsilon used for comparing weights for equality
local scalarQualiTypeMap = {}	-- maps scalar type names without qualifiers to the table of type ids for all qualifiers possible
local scalarIndexTypeMap = {}	-- maps scalar type names usable as index without qualifiers to the const type id used as index for [] operators or pointer arithmetics
local scalarBooleanType = nil	-- type id of the boolean type, result of cmpop binary operators
local scalarIntegerType = nil	-- type id of the main integer type, result of main function
local stringPointerType = nil	-- type id of the string constant type used for free string litterals
local stringAddressType = nil	-- type id of the string constant type used string constants outsize a function scope
local controlTrueType = nil	-- type implementing a boolean not represented as value but as peace of code (in the constructor) with a falseExit label
local controlFalseType = nil	-- type implementing a boolean not represented as value but as peace of code (in the constructor) with a trueExit label
local qualiTypeMap = {}		-- maps any defined type id without qualifier to the table of type ids for all qualifiers possible
local referenceTypeMap = {}	-- maps any defined type id to its reference type
local typeQualiMap = {}		-- maps any defined type id to a pair (lval,qualifier) = lval type (strips away qualifiers), qualifier string
local typeDescriptionMap = {}	-- maps any defined type id to its llvmir template structure
local stringConstantMap = {}    -- maps string constant values to a structure with its attributes {fmt,name,size}
local arrayTypeMap = {}		-- maps the pair lval,size to the array type lval for an array size 

function definePromoteCall( returnType, thisType, promoteType, opr, argTypes, promote_constructor)
	local call_constructor = typedb:type_constructor( typedb:get_type( promoteType, opr, argTypes))
	local callType = typedb:def_type( thisType, opr, promoteCallConstructor( call_constructor, promote_constructor), argTypes)
	if returnType then typedb:def_reduction( returnType, callType, nil, tag_typeDeclaration) end
end
function defineCall( returnType, thisType, opr, argTypes, constructor)
	local callType = typedb:def_type( thisType, opr, constructor, argTypes)
	if returnType then typedb:def_reduction( returnType, callType, nil, tag_typeDeclaration) end
end

local constexprIntegerType = typedb:def_type( 0, "constexpr int")
local constexprUIntegerType = typedb:def_type( 0, "constexpr uint")
local constexprFloatType = typedb:def_type( 0, "constexpr float")
local constexprBooleanType = typedb:def_type( 0, "constexpr bool")
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

function createConstExpr( node, constexpr_type, lexemvalue)
	if constexpr_type == constexprIntegerType then return bcd.int(lexemvalue)
	elseif constexpr_type == constexprUIntegerType then return bcd.int(lexemvalue)
	elseif constexpr_type == constexprFloatType then return tonumber(lexemvalue)
	elseif constexpr_type == constexprBooleanType then if lexemvalue == "true" then return true else return false end
	end
end
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

	function bool2bcd( value) if value then return bcd.int("1") else return bcd.int("0") end end
	typedb:def_reduction( constexprIntegerType, constexprBooleanType, bool2bcd, tag_TypeConversion, 0.25)
end
function defineQualifiedTypes( contextTypeId, typnam, typeDescription)
	local pointerTypeDescription = llvmir.pointerDescr( typeDescription)
	local pointerPointerTypeDescription = llvmir.pointerDescr( pointerTypeDescription)

	local lval = typedb:def_type( contextTypeId, typnam)
	local c_lval = typedb:def_type( contextTypeId, "const " .. typnam)
	local rval = typedb:def_type( contextTypeId, "&" .. typnam)
	local c_rval = typedb:def_type( contextTypeId, "const&" .. typnam)
	local pval = typedb:def_type( contextTypeId, "^" .. typnam)
	local c_pval = typedb:def_type( contextTypeId, "const^" .. typnam)
	local rpval = typedb:def_type( contextTypeId, "^&" .. typnam)
	local c_rpval = typedb:def_type( contextTypeId, "const^&" .. typnam)

	typeDescriptionMap[ lval] = typeDescription
	typeDescriptionMap[ c_lval] = typeDescription
	typeDescriptionMap[ rval] = pointerTypeDescription
	typeDescriptionMap[ c_rval] = pointerTypeDescription
	typeDescriptionMap[ pval] = pointerTypeDescription
	typeDescriptionMap[ c_pval] = pointerTypeDescription
	typeDescriptionMap[ rpval] = pointerPointerTypeDescription
	typeDescriptionMap[ c_rpval] = pointerPointerTypeDescription

	referenceTypeMap[ lval] = rval
	referenceTypeMap[ c_lval] = c_rval
	referenceTypeMap[ rval] = rval
	referenceTypeMap[ c_rval] = c_rval
	referenceTypeMap[ pval] = rpval
	referenceTypeMap[ c_pval] = c_rpval
	referenceTypeMap[ rpval] = rpval
	referenceTypeMap[ c_rpval] = c_rpval

	typeQualiMap[ lval] = {lval=lval}
	typeQualiMap[ c_lval] = {lval=lval,qualifier="const "}
	typeQualiMap[ rval] = {lval=lval,qualifier="&"}
	typeQualiMap[ c_rval] = {lval=lval,qualifier="const&"}
	typeQualiMap[ pval] = {lval=lval,qualifier="^"}
	typeQualiMap[ c_pval] = {lval=lval,qualifier="const^"}
	typeQualiMap[ rpval] = {lval=lval,qualifier="^&"}
	typeQualiMap[ c_rpval] = {lval=lval,qualifier="const^&"}

	typedb:def_reduction( lval, c_rval, convConstructor( pointerTypeDescription.load), tag_typeDeduction)
	typedb:def_reduction( pval, rpval, convConstructor( pointerPointerTypeDescription.load), tag_typeDeduction)
	typedb:def_reduction( c_pval, c_rpval, convConstructor( pointerPointerTypeDescription.load), tag_typeDeduction)

	typedb:def_reduction( c_lval, lval, nil, tag_typeDeduction)
	typedb:def_reduction( c_rval, rval, nil, tag_typeDeduction)
	typedb:def_reduction( c_pval, pval, nil, tag_typeDeduction)
	typedb:def_reduction( c_rpval, rpval, nil, tag_typeDeduction)

	defineCall( pval, rval, "&", {}, nil)
	defineCall( rval, pval, "->", {}, nil)
	defineCall( c_rval, c_pval, "->", {}, nil)

	qualiTypeMap[ lval] = {lval=lval, c_lval=c_lval, rval=rval, c_rval=c_rval, pval=pval, c_pval=c_pval, rpval=rpval, c_rpval=c_rpval}
	return qualiTypeMap[ lval]
end
function defineAssignOperators( qualitype, descr)
	local pointer_descr = llvmir.pointerDescr( descr)
	if descr.assign then defineCall( qualitype.rval, qualitype.rval, "=", {qualitype.c_lval}, assignConstructor( descr.assign)) end
	defineCall( qualitype.rpval, qualitype.rpval, "=", {qualitype.c_pval}, assignConstructor( pointer_descr.assign))
end
function defineIndexOperators( element_qualitype, pointer_descr)
	for index_typenam, index_type in pairs(scalarIndexTypeMap) do
		defineCall( element_qualitype.rval, element_qualitype.pval, "[]", {index_type}, callConstructor( pointer_descr.index[ index_typnam]))
		defineCall( element_qualitype.c_rval, element_qualitype.c_pval, "[]", {index_type}, callConstructor( pointer_descr.index[ index_typnam]))
	end
end
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
function defineBuiltInTypePromoteCalls( typnam, descr)
	local qualitype = scalarQualiTypeMap[ typnam]
	for i,promote_typnam in ipairs( descr.promote) do
		local promote_qualitype = scalarQualiTypeMap[ promote_typnam]
		local promote_descr = typeDescriptionMap[ promote_qualitype.lval]
		local promote_conv = convConstructor( promote_descr.conv[ typnam].fmt)
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

function getContextTypes()
	local rt = typedb:get_instance( "context")
	if rt then return rt else return {0} end
end

function implicitDefineArrayType( node, elementTypeId, arsize)
	local element_sep = typeQualiMap[ elementTypeId]
	local element_descr = typeDescriptionMap[ elementTypeId]
	local descr = llvmir.arrayDescr( element_descr, arsize)
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
		defineAssignOperators( qualitype, descr)
		rt = typedb:get_type( element_sep.lval, qualitypenam)
		typedb:scope( scopebk)
		arrayTypeMap[ typekey] = qualitype.lval
	end
	return rt
end

function defineVariable( node, context, typeId, name, initVal)
	local descr = typeDescriptionMap[ typeId]
	local callable = typedb:get_instance( "callable")
	if not context then
		local out = callable.register()
		local code = utils.constructor_format( descr.def_local, {out = out}, callable.register)
		local var = typedb:def_type( 0, name, out)
		typedb:def_reduction( referenceTypeMap[ typeId], var, nil, tag_typeDeclaration)
		local rt = {type=var, constructor={code=code,out=out}}
		if initVal then rt = applyCallable( node, rt, "=", {initVal}) end
		return rt
	elseif not context.qualitype then
		local fmt; if initVal then fmt = descr.def_global_val else fmt = descr.def_global end
		if type(initVal) == "table" then utils.errorMessage( node.line, "Only constexpr allowed to assign in global variable initialization") end
		out = "@" .. name
		print( utils.constructor_format( fmt, {out = out, inp = initVal}))
		local var = typedb:def_type( 0, name, out)
		typedb:def_reduction( referenceTypeMap[ typeId], var, nil, tag_typeDeclaration)
	else
		if initVal then utils.errorMessage( node.line, "No initialization value in definition of member variable allowed") end
		local element_lval,element_quali = typeQualiMap[ typeId];
		local element_index = context.index; context.index = element_index + 1
		local element_qualitype = qualiTypeMap[ element_lval]
		local element_descr = typeDescriptionMap[ typeId]
		if context.llvmtype then
			context.llvmtype = context.llvmtype  .. ", " .. element_descr.llvmtype
		else
			context.llvmtype = element_descr.llvmtype
		end
		local out = context.register()
		local load_code = utils.constructor_format( context.qualitype.rval, {out=out,this="%ptr",index=element_index, type=element_descr.llvmtype} )
		if element_descr.ctor then
			context.ctors = (context.ctors or "") .. load_code .. utils.constructor_format( element_descr.ctor, {this=out}, context.register)
		end
		if element_descr.dtor then
			context.dtors = (context.dtors or "") .. load_code .. utils.constructor_format( element_descr.dtor, {this=out}, context.register)
		end
		local load_ref = callConstructor( utils.template_format( context.descr.loadref, {index=element_index, type=element_descr.llvmtype}))
		local load_val = callConstructor( utils.template_format( context.descr.load, {index=element_index, type=element_descr.llvmtype}))
		if element_quali == "" then
			defineCall( element_qualitype.rval, context.qualitype.rval, name, {}, load_ref)
			defineCall( element_qualitype.c_rval, context.qualitype.c_rval, name, {}, load_ref)
			defineCall( element_qualitype.c_lval, context.qualitype.c_lval, name, {}, load_val)
		elseif element_quali == "^" then
			defineCall( element_qualitype.rpval, context.qualitype.rval, name, {}, load_ref)
			defineCall( element_qualitype.c_rpval, context.qualitype.c_rval, name, {}, load_ref)
			defineCall( element_qualitype.c_pval, context.qualitype.c_lval, name, {}, load_val)
		elseif element_quali == "const " then
			defineCall( element_qualitype.c_rval, context.qualitype.rval, name, {}, load_ref)
			defineCall( element_qualitype.c_rval, context.qualitype.c_rval, name, {}, load_ref)
			defineCall( element_qualitype.c_lval, context.qualitype.c_lval, name, {}, load_val)
		elseif element_quali == "const^" then
			defineCall( element_qualitype.c_rpval, context.qualitype.rval, name, {}, load_ref)
			defineCall( element_qualitype.c_rpval, context.qualitype.c_rval, name, {}, load_ref)
			defineCall( element_qualitype.c_pval, context.qualitype.c_lval, name, {}, load_val)
		else
			utils.errorMessage( node.line, "Reference qualifier '&' not defined for member variables")
		end
	end
end

function defineParameter( node, type, name, callable)
	local descr = typeDescriptionMap[ type]
	local paramreg = callable.register()
	local var = typedb:def_type( 0, name, paramreg)
	typedb:def_reduction( type, var, nil, tag_typeDeclaration)
	return {type=type, llvmtype=descr.llvmtype, reg=paramreg}
end

function initControlTypes()
	controlTrueType = typedb:def_type( 0, " controlTrueType")
	controlFalseType = typedb:def_type( 0, " controlFalseType")

	function falseExitToBoolean( constructor)
		local callable = typedb:get_instance( "callable")
		local out = callable.register()
		return {code = constructor.code .. utils.constructor_format(llvmir.control.falseExitToBoolean,{falseExit=constructor.out,out=out},callable.label),out=out}
	end
	function trueExitToBoolean( constructor)
		local callable = typedb:get_instance( "callable")
		local out = callable.register()
		return {code = constructor.code .. utils.constructor_format( llvmir.control.trueExitToBoolean,{trueExit=constructor.out,out=out},callable.label),out=out}
	end
	typedb:def_reduction( scalarBooleanType, controlTrueType, falseExitToBoolean, tag_typeDeduction, 0.1)
	typedb:def_reduction( scalarBooleanType, controlFalseType, trueExitToBoolean, tag_typeDeduction, 0.1)

	function booleanToFalseExit( constructor)
		local callable = typedb:get_instance( "callable")
		local out = callable.label()
		return {code = constructor.code .. utils.constructor_format( llvmir.control.booleanToFalseExit, {inp=constructor.out, out=out}, callable.label),out=out}
	end
	function booleanToTrueExit( constructor)
		local callable = typedb:get_instance( "callable")
		local out = callable.label()
		return {code = constructor.code .. utils.constructor_format( llvmir.control.booleanToTrueExit, {inp=constructor.out, out=out}, callable.label),out=out}
	end

	typedb:def_reduction( controlTrueType, scalarBooleanType, booleanToFalseExit, tag_typeDeduction, 0.1)
	typedb:def_reduction( controlFalseType, scalarBooleanType, booleanToTrueExit, tag_typeDeduction, 0.1)

	function negateControlTrueType( this) return {type=controlFalseType, constructor=this.constructor} end
	function negateControlFalseType( this) return {type=controlTrueType, constructor=this.constructor} end

	function joinControlTrueTypeWithBool( this, arg)
		local out = this.out
		local code2 = utils.constructor_format( llvmir.control.booleanToFalseExit, {inp=arg[1].out, out=out}, typedb:get_instance( "callable").label())
		return {code=this.code .. arg[1].code .. code2, out=out}
	end
	function joinControlFalseTypeWithBool( this, arg)
		local out = this.out
		local code2 = utils.constructor_format( llvmir.control.booleanToTrueExit, {inp=arg[1].out, out=out}, typedb:get_instance( "callable").label())
		return {code=this.code .. arg[1].code .. code2, out=out}
	end
	defineCall( controlTrueType, controlFalseType, "!", {}, nil)
	defineCall( controlFalseType, controlTrueType, "!", {}, nil)
	defineCall( controlTrueType, controlTrueType, "&&", {scalarBooleanType}, joinControlTrueTypeWithBool)
	defineCall( controlFalseType, controlFalseType, "||", {scalarBooleanType}, joinControlFalseTypeWithBool)

	function joinControlFalseTypeWithConstexprBool( this, arg)
		if arg == false then
			return this
		else 
			local callable = typedb:get_instance( "callable")
			return {code= this.code .. utils.constructor_format( llvmir.control.terminateTrueExit,{out=this.out},callable.label), out=this.out}
		end
	end
	function joinControlTrueTypeWithConstexprBool( this, arg)
		if arg == true then
			return this
		else 
			local callable = typedb:get_instance( "callable")
			return {code= this.code .. utils.constructor_format( llvmir.control.terminateFalseExit,{out=this.out},callable.label), out=this.out}
		end
	end
	defineCall( controlTrueType, controlTrueType, "&&", {constexprBooleanType}, joinControlTrueTypeWithConstexprBool)
	defineCall( controlFalseType, controlFalseType, "||", {constexprBooleanType}, joinControlFalseTypeWithConstexprBool)

	function constexprBooleanToControlTrueType( value)
		local callable = typedb:get_instance( "callable")
		local out = label()
		local code; if value == true then code="" else code=utils.constructor_format( llvmir.control.terminateFalseExit, {out=out}, callable.label) end
		return {code=code, out=out}
	end
	function constexprBooleanToControlFalseType( value)
		local callable = typedb:get_instance( "callable")
		local out = label()
		local code; if value == false then code="" else code=utils.constructor_format( llvmir.control.terminateFalseExit, {out=out}, callable.label) end
		return {code=code, out=out}
	end
	typedb:def_reduction( controlFalseType, constexprBooleanType, constexprBooleanToControlFalseType, tag_typeDeduction, 0.1)
	typedb:def_reduction( controlTrueType, constexprBooleanType, constexprBooleanToControlTrueType, tag_typeDeduction, 0.1)

	function invertControlBooleanType( this)
		local out = typedb:get_instance( "callable").label()
		return {code = this.code .. utils.constructor_format( llvmir.control.invertedControlType, {inp=this.out, out=out}), out = out}
	end
	typedb:def_reduction( controlFalseType, controlTrueType, invertControlBooleanType, tag_typeDeduction, 0.1)
	typedb:def_reduction( controlTrueType, controlFalseType, invertControlBooleanType, tag_typeDeduction, 0.1)

	definePromoteCall( controlTrueType, constexprBooleanType, controlTrueType, "&&", {scalarBooleanType}, constexprBooleanToControlTrueType)
	definePromoteCall( controlFalseType, constexprBooleanType, controlFalseType, "||", {scalarBooleanType}, constexprBooleanToControlFalseType)
end

function initBuiltInTypes()
	stringAddressType = typedb:def_type( 0, " stringAddressType")
	for typnam, scalar_descr in pairs( llvmir.scalar) do
		local qualitype = defineQualifiedTypes( 0, typnam, scalar_descr)
		defineAssignOperators( qualitype, scalar_descr)
		local c_lval = qualitype.c_lval
		scalarQualiTypeMap[ typnam] = qualitype
		if scalar_descr.class == "bool" then
			scalarBooleanType = c_lval
		elseif scalar_descr.class == "unsigned" then
			if scalar_descr.llvmtype == "i8" then
				stringPointerType = qualitype.c_pval
			end
			scalarIndexTypeMap[ typnam] = c_lval
		elseif scalar_descr.class == "signed" then
			if scalar_descr.llvmtype == "i32" then scalarIntegerType = qualitype.c_lval end
			if scalar_descr.llvmtype == "i8" and stringPointerType == 0 then
				stringPointerType = qualitype.c_pval
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
	initControlTypes()
end

function selectNoArgumentType( node, typeName, resolveContextTypeId, reductions, items)
	if not resolveContextTypeId or type(resolveContextTypeId) == "table" then
		utils.errorResolveType( typedb, node.line, resolveContextTypeId, getContextTypes(), typeName)
	end
	for ii,item in ipairs(items) do
		if typedb:type_nof_parameters( item.type) == 0 then
			local constructor = nil
			if resolveContextTypeId ~= 0 then
				constructor = typedb:type_constructor( resolveContextTypeId) 
			end
			for ri,redu in ipairs(reductions) do
				if redu.constructor then
					constructor = redu.constructor( constructor)
				end
			end
			if item.constructor then
				if type( item.constructor) == "function" then
					constructor = item.constructor( constructor)
				else
					constructor = item.constructor
				end
			end
			if constructor then
				local code,out = constructorParts( constructor)
				return item.type,{code=code,out=out}
			else
				return item.type
			end
		end
	end
	utils.errorMessage( node.line, "Failed to resolve %s with no arguments", utils.resolveTypeString( typedb, getContextTypes(), typeName))
end
function selectNoConstructorNoArgumentType( node, typeName, resolveContextTypeId, reductions, items)
	local rt,constructor = selectNoArgumentType( node, typeName, resolveContextTypeId, reductions, items)
	if constructor then utils.errorMessage( node.line, "No constructor type expected: %s", typeName) end
	return rt
end
function getReductionConstructor( node, redu_type, operand)
	local redu_constructor,weight = operand.constructor,0.0
	if redu_type ~= operand.type then
		local redulist,altpath
		redulist,weight,altpath = typedb:derive_type( redu_type, operand.type, tagmask_matchParameter, tagmask_typeConversion, 1)
		if not redulist then
			return nil
		elseif altpath then
			utils.errorMessage( node.line, "Ambiguous derivation paths for '%s': %s | %s",
			                    typedb:type_string(operand.type), utils.typeListString(typedb,altpath," =>"), utils.typeListString(typedb,redulist," =>"))
		end
		for ri,redu in ipairs(redulist) do
			if redu.constructor then redu_constructor = redu.constructor( redu_constructor) end
		end
	end
	return redu_constructor,weight
end

function applyCallable( node, this, callable, args)
	local bestmatch = {}
	local bestweight = nil
	local resolveContextType,reductions,items = typedb:resolve_type( this.type, callable, tagmask_resolveType)
	if not resolveContextType or type(resolveContextType) == "table" then utils.errorResolveType( typedb, node.line, resolveContextType, this.type, callable) end

	local this_constructor = this.constructor
	for ri,redu in ipairs(reductions) do
		if redu.constructor then
			this_constructor = redu.constructor( this_constructor)
		end
	end
	if not args then args = {} end
	for ii,item in ipairs(items) do
		local weight = 0.0
		if typedb:type_nof_parameters( item.type) == #args then
			local param_constructor_ar = {}
			if #args > 0 then
				local parameters = typedb:type_parameters( item.type)
				for ai,arg in ipairs(args) do
					local param_constructor,param_weight = getReductionConstructor( node, parameters[ai].type, arg)
					if not param_weight then break end

					weight = weight + param_weight
					table.insert( param_constructor_ar, param_constructor)
				end
			end
			if #param_constructor_ar == #args then
				if not bestweight or weight < bestweight + weightEpsilon then
					local callable_constructor = item.constructor( this_constructor, param_constructor_ar)
					if bestweight and weight >= bestweight - weightEpsilon then
						table.insert( bestmatch, {type=item.type, constructor=callable_constructor})
					else
						bestweight = weight
						bestmatch = {{type=item.type, constructor=callable_constructor}}
					end
				end
			end
		end
	end
	if not bestweight then
		utils.errorMessage( node.line, "Failed to find callable with signature '%s'",
	                    utils.resolveTypeString( typedb, this.type, callable) .. "(" .. utils.typeListString( typedb, args, ", ") .. ")")
	end
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

function getSignatureString( name, args, context)
	if not context then
		return utils.uniqueName( name .. "__")
	else
		local pstr = ""
		if context.qualitype then
			pstr = ptr .. "__C_" .. typedb:type_string( context.qualitype.lval, "__") .. "__"
		end
		for ai,arg in ipairs(args) do if pstr == "" then pstr = pstr .. "__" .. arg.llvmtype else pstr = "_" .. arg.llvmtype end end
		return utils.encodeName( name .. pstr)
	end
end
function getParameterString( args)
	local rt = ""
	for ai,arg in ipairs(args) do if rt == "" then rt = rt .. arg.llvmtype .. " " .. arg.reg else rt = rt .. ", " .. arg.llvmtype .. " " .. arg.reg end end
	return rt
end
function getParameterLlvmTypeString( args)
	local rt = ""
	for ai,arg in ipairs(args) do if rt == "" then rt = rt .. arg.llvmtype else rt = rt .. ", " .. arg.llvmtype end end
	return rt
end
function getParameterTypeList( args)
	rt = {}
	for ai,arg in ipairs(args) do table.insert(rt,arg.type) end
	return rt
end
function getParameterListCallTemplate( param)
	if #param == 0 then return "" end
	local rt = param[1].llvmtype .. " {arg" .. 1 .. "}"
	for i=2,#param do rt = rt .. ", " .. param[i].llvmtype .. " {arg" .. i .. "}" end
	return rt
end
function getTypeDeclContextTypeId( context)
	if not context then return 0 elseif context.qualitype then return context.qualitype.lval else return 0 end
end
function defineCallableType( node, descr, contextTypeId)
	local callable = typedb:get_type( contextTypeId, descr.name)
	if not callable then callable = typedb:def_type( contextTypeId, descr.name) end
	if descr.ret then
		descr.rtype = typeDescriptionMap[ descr.ret].llvmtype
		local callfmt = utils.template_format( llvmir.control.functionCall, descr)
		defineCall( descr.ret, callable, "()", descr.param, callConstructor( callfmt))
	else
		descr.rtype = "void"
		local callfmt = utils.template_format( llvmir.control.procedureCall, descr)
		defineCall( nil, callable, "()", descr.param, callConstructor( callfmt))
	end
end
function defineCallable( node, descr, context)
	descr.paramstr = getParameterString( descr.param)
	descr.symbolname = getSignatureString( descr.name, descr.param, context)
	descr.callargstr = getParameterListCallTemplate( descr.param)
	defineCallableType( node, descr, getTypeDeclContextTypeId( context))
	print( "\n" .. utils.constructor_format( llvmir.control.functionDeclaration, descr))
end
function defineExternCallable( node, descr)
	descr.argstr = getParameterLlvmTypeString( descr.param)
	if descr.externtype == "C" then
		descr.symbolname = descr.name
	else
		utils.errorMessage( node.line, "Unknown extern call type \"%s\" (must be one of {\"C\"})", descr.externtype)
	end
	descr.callargstr = getParameterListCallTemplate( descr.param)
	defineCallableType( node, descr, 0)
	print( "\n" .. utils.constructor_format( llvmir.control.extern_functionDeclaration, descr))
end
function defineCallableBodyContext( scope, rtype)
	typedb:set_instance( "callable", {scope=scope, register=utils.register_allocator(), label=utils.label_allocator(), returntype=rtype})
end
function getStringConstant( value)
	if not stringConstantMap[ value] then
		local encval,enclen = utils.encodeLexemLlvm(value)
		local name = utils.uniqueName( "string")
		stringConstantMap[ value] = {fmt=utils.template_format( llvmir.control.stringConstConstructor, {name=name,size=enclen+1}),name=name,size=enclen+1}
		print( utils.constructor_format( llvmir.control.stringConstDeclaration, {out="@" .. name, size=enclen+1, value=encval}) .. "\n")
	end
	local callable = typedb:get_instance( "callable")
	if not callable then
		return {type=stringAddressType,constructor=stringConstantMap[ value]}
	else
		out = callable.register()
		return {type=stringPointerType,constructor={code=utils.constructor_format( stringConstantMap[ value].fmt, {out=out}), out=out}}
	end
end

-- AST Callbacks:
function typesystem.definition( node, context)
	local arg = utils.traverse( typedb, node, context)
	if arg[1] then return {code = arg[1].constructor.code} else return {code=""} end
end
function typesystem.paramdef( node) 
	local arg = utils.traverse( typedb, node)
	return defineParameter( node, arg[1], arg[2], typedb:get_instance( "callable"))
end

function typesystem.paramdeflist( node)
	return utils.traverse( typedb, node)
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
	local arg = utils.traverse( typedb, node)
	if not context or context.typeId == 0 then
		local type = typedb:def_type( 0, arg[2].value, typedb:type_constructor( arg[1]))
		typedb:def_reduction( arg[1], type, nil, tag_typeDeclaration)
	else
		utils.errorMessage( node.line, "Member variables not implemented yet")
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
function typesystem.statement( node)
	local code = ""
	local arg = utils.traverse( typedb, node)
	for ai=1,#arg do
		if arg[ ai] then
			code = code .. arg[ ai].code
		end
	end
	return {code=code}
end
function typesystem.return_value( node)
	local arg = utils.traverse( typedb, node)
	local callable = typedb:get_instance( "callable")
	local rtype = callable.returntype
	if rtype == 0 then utils.errorMessage( node.line, "Can't return value from procedure") end
	local constructor = getReductionConstructor( node, rtype, arg[1])
	if not constructor then utils.errorMessage( node.line, "Return value does not match declared return type '%s'", typedb:type_string(rtype)) end
	local code = utils.constructor_format( llvmir.control.returnStatement, {type=typeDescriptionMap[rtype].llvmtype, inp=constructor.out})
	return {code = constructor.code .. code}
end
function typesystem.conditional_if( node)
	local arg = utils.traverse( typedb, node)
	local cond_constructor = getReductionConstructor( node, controlTrueType, arg[1])
	if not cond_constructor then utils.errorMessage( node.line, "Can't use type '%s' as a condition", typedb:type_string(arg[1].type)) end
	return {code = cond_constructor.code .. arg[2].code .. utils.constructor_format( llvmir.control.label, {inp=cond_constructor.out})}
end
function typesystem.conditional_if_else( node)
	local arg = utils.traverse( typedb, node)
	local cond_constructor = getReductionConstructor( node, controlTrueType, arg[1])
	if not cond_constructor then utils.errorMessage( node.line, "Can't use type '%s' as a condition", typedb:type_string(arg[1].type)) end
	local callable = typedb:get_instance( "callable")
	local exit = callable.label()
	local elsecode = utils.constructor_format( llvmir.control.invertedControlType, {inp=cond_constructor.out, out=exit})
	return {code = cond_constructor.code .. arg[2].code .. elsecode .. arg[3].code .. utils.constructor_format( llvmir.control.label, {inp=exit})}
end
function typesystem.conditional_while( node)
	local arg = utils.traverse( typedb, node)
	local cond_constructor = getReductionConstructor( node, controlTrueType, arg[1])
	if not cond_constructor then utils.errorMessage( node.line, "Can't use type '%s' as a condition", typedb:type_string(arg[1].type)) end
	local callable = typedb:get_instance( "callable")
	local start = callable.label()
	local startcode = utils.constructor_format( llvmir.control.label, {inp=start})
	return {code = startcode .. cond_constructor.code .. arg[2].code 
			.. utils.constructor_format( llvmir.control.invertedControlType, {out=start,inp=cond_constructor.out})}
end

function typesystem.typespec( node, qualifier)
	local typeName = qualifier .. node.arg[ #node.arg].value;
	local typeId
	if #node.arg == 1 then
		typeId = selectNoConstructorNoArgumentType( node, typeName, typedb:resolve_type( getContextTypes(), typeName, tagmask_typeNameSpace))
	else
		local res = typedb:resolve_type( getContextTypes(), node.arg[ 1].value, tagmask_typeNameSpace)
		local contextTypeId = selectNoConstructorNoArgumentType( node, typeName, res)
		if #node.arg > 2 then
			for ii = 2, #node.arg-1 do
				local res = typedb:resolve_type( contextTypeId, node.arg[ii].value, tagmask_typeNameSpace)
				contextTypeId = selectNoConstructorNoArgumentType( node, typeName, res)
			end
		end
		if not constructor then
			typeId = selectNoConstructorNoArgumentType( node, typeName, typedb:resolve_type( contextTypeId, typeName, tagmask_typeNameSpace))
		end
	end
	return typeId
end
function typesystem.constant( node, typeName)
	local typeId = selectNoConstructorNoArgumentType( node, typeName, typedb:resolve_type( 0, typeName))
	return {type=typeId, constructor=createConstExpr( node, typeId, node.arg[1].value)}
end
function typesystem.string_constant( node)
	return getStringConstant( node.arg[1].value)
end
function typesystem.char_constant( node)
	local ua = utils.utf8to32( node.arg[1].value)
	if #ua == 0 then return {type=constexprUIntegerType, constructor=bcd.int(0)}
	elseif #ua == 1 then return {type=constexprUIntegerType, constructor=bcd.int(ua[1])}
	else utils.errorMessage( node.line, "Single quoted string '%s' not containing a single unicode character", node.arg[1].value)
	end
end
function typesystem.variable( node)
	local typeName = node.arg[ 1].value
	local callable = typedb:get_instance( "callable")
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
function typesystem.funcdef( node, context)
	local arg = utils.traverseRange( typedb, node, {1,#node.arg-1}, context)
	arg[#node.arg] = utils.traverseRange( typedb, node, {#node.arg-1,#node.arg}, context, arg[2])[#node.arg]
	local descr = {lnk = arg[1].linkage, attr = arg[1].attributes, ret = arg[2], name = arg[3], param = arg[4].param, body = arg[4].code}
	defineCallable( node, descr, context)
end
function typesystem.procdef( node, context)
	local arg = utils.traverse( typedb, node, context, 0)
	local descr = {lnk = arg[1].linkage, attr = arg[1].attributes, name = arg[2], param = arg[3].param, body = arg[3].code .. "ret void\n" }
	defineCallable( node, descr, context)
end
function typesystem.callablebody( node, contextTypeId, rtype) 
	defineCallableBodyContext( node.scope, rtype)
	local arg = utils.traverse( typedb, node, contextTypeId)
	return {param = arg[1], code = arg[2].code}
end
function typesystem.extern_paramdeflist( node)
	local args = utils.traverse( typedb, node)
	local rt = {}; for ai,arg in ipairs(args) do
		local llvmtype = typeDescriptionMap[ arg].llvmtype
		table.insert( rt, {type=arg,llvmtype=llvmtype} )
	end
	return rt
end
function typesystem.extern_funcdef( node)
	local arg = utils.traverse( typedb, node)
	local descr = {externtype = arg[1], ret = arg[2], name = arg[3], param = arg[4]}
	defineExternCallable( node, descr)
end
function typesystem.extern_procdef( node)
	local arg = utils.traverse( typedb, node)
	local descr = {externtype = arg[1], name = arg[2], param = arg[3]}
	defineExternCallable( node, descr)
end
function typesystem.interface_funcdef( node)
end
function typesystem.interface_procdef( node)
end
function typesystem.structdef( node, context)
	local typnam = node.arg[1].value
	local structname = utils.uniqueName( typnam)
	local descr = utils.template_format( llvmir.structTemplate, {structname=structname})
	local qualitype = defineQualifiedTypes( getTypeDeclContextTypeId( context), typnam, descr)
	local context = {qualitype=qualitype,descr=descr,index=0,register=utils.register_allocator()}
	local arg = utils.traverse( typedb, node, context)
	print( "Auto", utils.template_format( llvmir.structTemplate.ctorproc, {ctors=context.ctors or ""}))
	print( "Auto", utils.template_format( llvmir.structTemplate.dtorproc, {ctors=context.dtors or ""}))
	print( "Typedefs", utils.template_format( llvmir.control.structdef, {structname=structname,llvmtype=context.llvmtype}))
end
function typesystem.interfacedef( node, context)
end
function typesystem.classdef( node, context)
end
function typesystem.main_procdef( node)
	defineCallableBodyContext( node.scope, scalarIntegerType)
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
	return applyCallable( node, expr, node.arg[2].value)
end

return typesystem
