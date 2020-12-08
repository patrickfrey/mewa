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
		local register = typedb:get_instance( "register")
		local out = register()
		local code,inp = constructorParts( constructor)
		return {code = code .. utils.constructor_format( fmt, {inp = inp, out = out}, register), out = out}
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
		local register = typedb:get_instance( "register")
		local out = register()
		local code = ""
		local this_code,this_inp = constructorParts( this)
		local subst = {out = out, this = this_inp}
		code = code .. this_code
		for ii=1,#arg do
			local arg_code,arg_inp = constructorParts( arg[ ii])
			code = code .. arg_code
			subst[ "arg" .. ii] = arg_inp
		end
		return {code = code .. utils.constructor_format( fmt, subst, register), out = out}
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
local valueTypeMap = {}		-- maps any defined type id to its value type (strips away reference qualifiers)
local typeDescriptionMap = {}	-- maps any defined type id to its llvmir template structure
local stringConstantMap = {}    -- maps string constant values to a structure with its attributes {fmt,name,size}

function definePromoteCall( returnType, thisType, opr, argTypes, promote_constructor)
	local call_constructor = typedb:type_constructor( typedb:get_type( argTypes[1], opr, argTypes ))
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
		definePromoteCall( constexprFloatType, constexprIntegerType, opr, {constexprFloatType}, function(this) return this:tonumber() end)
	end
	local oprlist = constexprTypeOperatorMap[ constexprUIntegerType]
	for oi,opr in ipairs(oprlist) do
		if not unaryOperatorMap[ opr] then
			definePromoteCall( constexprUIntegerType, constexprIntegerType, opr, {constexprUIntegerType}, function(this) return this end)
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
function defineQualifiedTypes( typnam, typeDescription)
	local pointerTypeDescription = llvmir.pointerType( typeDescription.llvmtype)
	local pointerPointerTypeDescription = llvmir.pointerType( pointerTypeDescription.llvmtype)

	local lval = typedb:def_type( 0, typnam)
	local c_lval = typedb:def_type( 0, "const " .. typnam)
	local rval = typedb:def_type( 0, "&" .. typnam)
	local c_rval = typedb:def_type( 0, "const&" .. typnam)
	local pval = typedb:def_type( 0, "^" .. typnam)
	local c_pval = typedb:def_type( 0, "const^" .. typnam)
	local rpval = typedb:def_type( 0, "^&" .. typnam)
	local c_rpval = typedb:def_type( 0, "const^&" .. typnam)

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

	valueTypeMap[ lval] = lval
	valueTypeMap[ c_lval] = c_lval
	valueTypeMap[ rval] = lval
	valueTypeMap[ c_rval] = c_lval
	valueTypeMap[ pval] = pval
	valueTypeMap[ c_pval] = c_pval
	valueTypeMap[ rpval] = pval
	valueTypeMap[ c_rpval] = c_pval

	typedb:def_reduction( lval, c_rval, convConstructor( pointerTypeDescription.load), tag_typeDeduction)
	typedb:def_reduction( pval, rpval, convConstructor( pointerPointerTypeDescription.load), tag_typeDeduction)
	typedb:def_reduction( c_pval, c_rpval, convConstructor( pointerPointerTypeDescription.load), tag_typeDeduction)

	typedb:def_reduction( c_lval, lval, nil, tag_typeDeduction)
	typedb:def_reduction( c_rval, rval, nil, tag_typeDeduction)
	typedb:def_reduction( c_pval, pval, nil, tag_typeDeduction)
	typedb:def_reduction( c_rpval, rpval, nil, tag_typeDeduction)

	defineCall( rval, pval, "->", {}, nil)
	defineCall( c_rval, c_pval, "->", {}, nil)

	defineCall( rval, rval, "=", {c_lval}, assignConstructor( typeDescription.assign))
	defineCall( rpval, rpval, "=", {c_pval}, assignConstructor( pointerTypeDescription.assign))

	qualiTypeMap[ lval] = {lval=lval, c_lval=c_lval, rval=rval, c_rval=c_rval, pval=pval, c_pval=c_pval, rpval=rpval, c_rpval=c_rpval}
	return lval
end
function defineIndexOperators( typnam, qualitype, descr)
	local pointerTypeDescription = llvmir.pointerType( descr.llvmtype)
	for index_typenam, index_type in pairs(scalarIndexTypeMap) do
		defineCall( qualitype.rval, qualitype.pval, "[]", {index_type}, convConstructor( pointerTypeDescription.index[ index_typnam]))
		defineCall( qualitype.c_rval, qualitype.c_pval, "[]", {index_type}, convConstructor( pointerTypeDescription.index[ index_typnam]))
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
				definePromoteCall( promote_qualitype.lval, qualitype.c_lval, operator, {promote_qualitype.c_lval}, promote_conv)
			end
		end
		if promote_descr.cmpop then
			for operator,operator_fmt in pairs( promote_descr.cmpop) do
				definePromoteCall( scalarBooleanType, qualitype.c_lval, operator, {promote_qualitype.c_lval}, promote_conv)
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

function defineVariable( node, contextTypeId, typeId, name, initVal)
	local descr = typeDescriptionMap[ typeId]
	local register = typedb:get_instance( "register")
	if not contextTypeId then
		local out = register()
		local code = utils.constructor_format( descr.def_local, {out = out}, register)
		local var = typedb:def_type( 0, name, out)
		typedb:def_reduction( referenceTypeMap[ typeId], var, nil, tag_typeDeclaration)
		local rt = {type=var, constructor={code=code,out=out}}
		if initVal then rt = applyCallable( node, rt, "=", {initVal}) end
		return rt
	elseif contextTypeId == 0 then
		local fmt; if initVal then fmt = descr.def_global_val else fmt = descr.def_global end
		if type(initVal) == "table" then utils.errorMessage( node.line, "Only constexpr allowed to assign in global variable initialization") end
		out = "@" .. name
		print( utils.constructor_format( fmt, {out = out, inp = initVal}))
		local var = typedb:def_type( contextTypeId, name, out)
		typedb:def_reduction( referenceTypeMap[ typeId], var, nil, tag_typeDeclaration)
	else
		utils.errorMessage( node.line, "Member variables not implemented yet")
	end
end

function defineParameter( node, type, name, register)
	local descr = typeDescriptionMap[ type]
	local paramreg = register()
	local var = typedb:def_type( 0, name, paramreg)
	typedb:def_reduction( type, var, nil, tag_typeDeclaration)
	return {type=type, llvmtype=descr.llvmtype, reg=paramreg}
end

function defineType( node, typeName, typeDescription)
	local lval = defineQualifiedTypes( typnam, typeDescription)
	defineIndexOperators( typnam, qualiTypeMap[ lval], typeDescription)
end

function initControlTypes()
	controlTrueType = typedb:def_type( 0, " controlTrueType")
	controlFalseType = typedb:def_type( 0, " controlFalseType")

	function falseExitToBoolean( constructor)
		local register,label = typedb:get_instance( "register"),typedb:get_instance( "label")
		local out = register()
		return {code = constructor.code .. utils.constructor_format( llvmir.control.falseExitToBoolean, {falseExit=constructor.out, out=out}, label),out=out}
	end
	function trueExitToBoolean( constructor)
		local register,label = typedb:get_instance( "register"),typedb:get_instance( "label")
		local out = register()
		return {code = constructor.code .. utils.constructor_format( llvmir.control.trueExitToBoolean, {trueExit=constructor.out, out=out}, label),out=out}
	end
	typedb:def_reduction( scalarBooleanType, controlTrueType, falseExitToBoolean, tag_typeDeduction)
	typedb:def_reduction( scalarBooleanType, controlFalseType, trueExitToBoolean, tag_typeDeduction)

	function booleanToFalseExit( constructor)
		local label = typedb:get_instance( "label")
		local out = label()
		return {code = constructor.code .. utils.constructor_format( llvmir.control.booleanToFalseExit, {inp=constructor.out, out=out}, label),out=out}
	end
	function booleanToTrueExit( constructor)
		local label = typedb:get_instance( "label")
		local out = label()
		return {code = constructor.code .. utils.constructor_format( llvmir.control.booleanToTrueExit, {inp=constructor.out, out=out}, label),out=out}
	end

	typedb:def_reduction( controlTrueType, scalarBooleanType, booleanToFalseExit, tag_typeDeduction)
	typedb:def_reduction( controlFalseType, scalarBooleanType, booleanToTrueExit, tag_typeDeduction)

	function negateControlTrueType( this) return {type=controlFalseType, constructor=this.constructor} end
	function negateControlFalseType( this) return {type=controlTrueType, constructor=this.constructor} end

	function joinControlTrueTypeWithBool( this, arg)
		local out = this.out
		local code2 = utils.constructor_format( llvmir.control.booleanToFalseExit, {inp=arg[1].out, out=out}, typedb:get_instance( "label"))
		return {code=this.code .. arg[1].code .. code2, out=out}
	end
	function joinControlFalseTypeWithBool( this, arg)
		local out = this.out
		local code2 = utils.constructor_format( llvmir.control.booleanToTrueExit, {inp=arg[1].out, out=out}, typedb:get_instance( "label"))
		return {code=this.code .. arg[1].code .. code2, out=out}
	end
	function invertControlBooleanType( this)
		local code = this.code .. utils.constructor_format( llvmir.control.invertedControlType, {inp=this.out, out=out})
	end
	defineCall( controlTrueType, controlFalseType, "!", {}, nil)
	defineCall( controlFalseType, controlTrueType, "!", {}, nil)
	defineCall( controlTrueType, controlTrueType, "&&", {scalarBooleanType}, joinControlTrueTypeWithBool)
	defineCall( controlFalseType, controlTrueType, "||", {scalarBooleanType}, joinControlFalseTypeWithBool)

	function joinControlFalseTypeWithConstexprBool( this, arg)
		if arg == false then
			return this
		else 
			return {code= this.code .. utils.constructor_format( llvmir.control.terminateFalseExit,{out=this.out},typedb:get_instance( "label")), out=this.out}
		end
	end
	function joinControlTrueTypeWithConstexprBool( this, arg)
		if arg == true then
			return this
		else 
			return {code= this.code .. utils.constructor_format( llvmir.control.terminateFalseExit,{out=this.out},typedb:get_instance( "label")), out=this.out}
		end
	end
	defineCall( controlTrueType, controlTrueType, "&&", {constexprBooleanType}, joinControlTrueTypeWithConstexprBool)
	defineCall( controlFalseType, controlFalseType, "||", {constexprBooleanType}, joinControlFalseTypeWithConstexprBool)

	-- typedb:def_reduction( controlFalseType, constexprBooleanType, function(this) return {code="",out=, tag_typeDeduction)
	-- typedb:def_reduction( controlTrueType, constexprBooleanType, invertControlBooleanType, tag_typeDeduction)

	typedb:def_reduction( controlFalseType, controlTrueType, invertControlBooleanType, tag_typeDeduction)
	typedb:def_reduction( controlTrueType, controlFalseType, invertControlBooleanType, tag_typeDeduction)
end

function initBuiltInTypes()
	stringAddressType = typedb:def_type( 0, " stringAddressType")
	for typnam, scalar_descr in pairs( llvmir.scalar) do
		local lval = defineQualifiedTypes( typnam, scalar_descr)
		local c_lval = qualiTypeMap[ lval].c_lval
		scalarQualiTypeMap[ typnam] = qualiTypeMap[ lval]
		if scalar_descr.class == "bool" then
			scalarBooleanType = c_lval
		elseif scalar_descr.class == "unsigned" then
			if scalar_descr.llvmtype == "i8" then
				stringPointerType = qualiTypeMap[ lval].c_pval
			end
			scalarIndexTypeMap[ typnam] = c_lval
		elseif scalar_descr.class == "signed" then
			if scalar_descr.llvmtype == "i32" then scalarIntegerType = qualiTypeMap[ lval].c_lval end
			if scalar_descr.llvmtype == "i8" and stringPointerType == 0 then
				stringPointerType = qualiTypeMap[ lval].c_pval
			end
			scalarIndexTypeMap[ typnam] = c_lval
		end
		for i,constexprType in ipairs( typeClassToConstExprTypesMap[ scalar_descr.class]) do
			local weight = 0.25*(1.0-scalar_descr.sizeweight)
			typedb:def_reduction( lval, constexprType, function(arg) return {code="",out=tostring(arg)} end, tag_typeDeduction, weight)
		end
	end
	if not scalarBooleanType then utils.errorMessage( 0, "No boolean type defined in built-in scalar types") end
	if not scalarIntegerType then utils.errorMessage( 0, "No integer type mapping to i32 defined in built-in scalar types (return value of main)") end
	if not stringPointerType then utils.errorMessage( 0, "No i8 type defined suitable to be used for free string constants)") end

	for typnam, scalar_descr in pairs( llvmir.scalar) do
		defineIndexOperators( typnam, scalarQualiTypeMap[ typnam], scalar_descr)
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
					if not param_constructor then break end

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
		utils.errorMessage( node.line, "Ambiguous matches resolving callable with signature %s, list of candidates: %s",
				utils.resolveTypeString( typedb, this.type, callable) .. "(" .. utils.typeListString( typedb, args, ", ") .. ")", altmatchstr)
	end
end

function convertToBooleanType( node, operand)
	if type(operand.constructor) == "table" then
		local bool_constructor = getReductionConstructor( node, scalarBooleanType, operand)
		if not bool_constructor then
			utils.errorMessage( node.line, "Operand type '%s' is not reducible to boolean", typedb:type_string(operand.type))
		end
		return {type=scalarBooleanType, constructor=bool_constructor}
	else
		local bool_constructor = getReductionConstructor( node, constexprBooleanType, operand)
		if not bool_constructor then
			utils.errorMessage( node.line, "Operand type '%s' is not reducible to boolean const expression", typedb:type_string(operand.type))
		end
		return {type=constexprBooleanType, constructor=bool_constructor}
	end
end

function convertToControlBooleanType( node, controlBooleanType, operand)
	if operand.type == controlBooleanType then
		return operand
	elseif operand.type == controlFalseType or operand.type == controlTrueType then
		local out = typedb:get_instance( "label")()
		return {type = controlBooleanType,
			constructor = operand.constructor .. utils.constructor_format( llvmir.control.invertedControlType, {inp=constructor.out, out=out}) }
	else
		local boolOperand = convertToBooleanType( node, operand)
		if not typedb:get_instance( "label") then return boolOperand end
		local control_constructor = getReductionConstructor( node, controlBooleanType, {type=scalarBooleanType,constructor=boolOperand.constructor})
		return {type=controlBooleanType, constructor=control_constructor}
	end
end
function negateControlBooleanType( node, this)
	if this.type == scalarBooleanType or this.type == constexprBooleanType then
		return applyCallable( node, this, "!")
	else
		if this.type == controlFalseType then
			return {type=controlTrueType, constructor=this.constructor}
		else
			return {type=controlFalseType, constructor=this.constructor}
		end
	end
end
function applyControlBooleanJoin( node, this, operand, controlBooleanType)
	local operand1 = convertToControlBooleanType( node, controlBooleanType, this)
	local operand2 = convertToBooleanType( node, operand)
	if operand1.operand.type == constexprBooleanType then
		if operand1.operand.constructor then return operand2 else return {type=constexprBooleanType,constructor=false} end
	elseif operand2.operand.type == constexprBooleanType then 
		if operand2.operand.constructor then return operand1 else return {type=constexprBooleanType,constructor=false} end
	elseif operand1.operand.type == scalarBooleanType and operand2.operand.type == scalarBooleanType then
		return applyCallable( node, this, "&&", {operand})
	elseif operand1.operand.type == controlFalseType and operand2.operand.type == scalarBooleanType then
		local out = operand1.constructor.out
		local fmt; if controlBooleanType == controlTrueType then fmt = llvmir.control.booleanToFalseExit else fmt = llvmir.control.booleanToTrueExit end
		local code2 = utils.constructor_format( fmt, {inp=operand2.constructor.out, out=out}, typedb:get_instance( "label"))
		return {code=operand1.constructor.code .. code2, out=out}
	else
		utils.errorMessage( node.line, "Boolean expression cannot be evaluated")
	end
end

function getSignatureString( name, args, contextTypeId)
	if not contextTypeId then
		return utils.uniqueName( name .. "__")
	else
		local pstr = ""
		if contextTypeId ~= 0 then
			pstr = ptr .. "__C_" .. typedb:type_string( contextTypeId, "__") .. "__"
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
function defineCallable( node, descr, contextTypeId)
	descr.paramstr = getParameterString( descr.param)
	descr.symbolname = getSignatureString( descr.name, descr.param, contextTypeId)
	descr.callargstr = getParameterListCallTemplate( descr.param)
	defineCallableType( node, descr, contextTypeId)
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
function defineCallableBodyContext( rtype)
	typedb:set_instance( "register", utils.register_allocator())
	typedb:set_instance( "label", utils.label_allocator())
	typedb:set_instance( "return", rtype)
end
function getStringConstant( value)
	if not stringConstantMap[ value] then
		local encval,enclen = utils.encodeCString(value)
		local name = utils.uniqueName( "string")
		stringConstantMap[ value] = {fmt=utils.template_format( llvmir.control.stringConstConstructor, {name=name,size=enclen+1}),name=name,size=enclen+1}
		print( utils.constructor_format( llvmir.control.stringConstDeclaration, {out="@" .. name, size=enclen+1, value=encval}) .. "\n")
	end
	local register = typedb:get_instance( "register")
	if not register then
		return {type=stringAddressType,constructor=stringConstantMap[ value]}
	else
		out = register()
		return {type=stringPointerType,constructor={code=utils.constructor_format( stringConstantMap[ value].fmt, {out=out}), out=out}}
	end
end

-- AST Callbacks:
function typesystem.definition( node, contextTypeId)
	local arg = utils.traverse( typedb, node, contextTypeId)
	if arg[1] then return {code = arg[1].constructor.code} else return {code=""} end
end
function typesystem.paramdef( node) 
	local arg = utils.traverse( typedb, node)
	return defineParameter( node, arg[1], arg[2], typedb:get_instance( "register"))
end

function typesystem.paramdeflist( node)
	return utils.traverse( typedb, node)
end

function typesystem.vardef( node, contextTypeId)
	local arg = utils.traverse( typedb, node, contextTypeId)
	return defineVariable( node, contextTypeId, arg[1], arg[2], nil)
end
function typesystem.vardef_assign( node, contextTypeId)
	local arg = utils.traverse( typedb, node, contextTypeId)
	return defineVariable( node, contextTypeId, arg[1], arg[2], arg[3])
end
function typesystem.vardef_array( node, contextTypeId)
	local arg = utils.traverse( typedb, node, contextTypeId)
	utils.errorMessage( node.line, "Array variables not implemented yet")
end
function typesystem.vardef_array_assign( node, contextTypeId)
	local arg = utils.traverse( typedb, node, contextTypeId)
	utils.errorMessage( node.line, "Array variables not implemented yet")
end
function typesystem.typedef( node, contextTypeId)
	local arg = utils.traverse( typedb, node)
	if not contextTypeId or contextTypeId == 0 then
		local type = typedb:def_type( 0, arg[2].value, typedb:type_constructor( arg[1]))
		typedb:def_reduction( arg[1], type, nil, tag_typeDeclaration)
	else
		utils.errorMessage( node.line, "Member variables not implemented yet")
	end
end

function typesystem.assign_operator( node, operator)
	local arg = utils.traverse( typedb, node)
	return applyCallable( node, "=", {arg[1], applyCallable( node, operator, arg)})
end
function typesystem.operator( node, operator)
	local args = utils.traverse( typedb, node)
	local this = args[1]
	table.remove( args, 1)
	return applyCallable( node, this, operator, args)
end

function typesystem.logic_operator_not( node, operator)
	local arg = utils.traverse( typedb, node)
	return negateControlBooleanType( node, convertToControlBooleanType( node, controlFalseType, arg[1]))
end
function typesystem.logic_operator_and( node, operator)
	local arg = utils.traverse( typedb, node)
	return applyControlBooleanJoin( node, arg, controlTrueType)
end
function typesystem.logic_operator_or( node, operator)
	local arg = utils.traverse( typedb, node)
	return applyControlBooleanJoin( node, arg, controlFalseType)
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
	local type = typedb:get_instance( "return")
	if type == 0 then utils.errorMessage( node.line, "Procedure can't return value") end
	local constructor = getReductionConstructor( node, type, arg[1])
	if not constructor then utils.errorMessage( node.line, "Return value does not match declared return type", typedb:type_string(type)) end
	local code = utils.constructor_format( llvmir.control.returnStatement, {type=typeDescriptionMap[type].llvmtype, inp=constructor.out})
	return {code = constructor.code .. code}
end
function typesystem.conditional_if( node)
	local arg = utils.traverse( typedb, node)
	local cond = convertToControlBooleanType( node, controlTrueType, arg[1])
	local ccode,cout = cond.constructor.code, cond.constructor.out
	return {code = ccode .. arg[2].code .. utils.constructor_format( llvmir.control.label, {inp=cout})}
end
function typesystem.conditional_if_else( node)
	local arg = utils.traverse( typedb, node)
	local cond = convertToControlBooleanType( node, controlTrueType, arg[1])
	local ccode,cout = cond.constructor.code, cond.constructor.out
	local exit = typedb:get_instance( "label")()
	local elsecode = utils.constructor_format( llvmir.control.invertedControlType, {inp=cout, out=exit})
	return {code = ccode .. arg[2].code .. elsecode .. arg[3].code .. utils.constructor_format( llvmir.control.label, {inp=exit})}
end
function typesystem.conditional_while( node)
	local arg = utils.traverse( typedb, node)
	local cond = convertToControlBooleanType( node, controlTrueType, arg[1])
	local ccode,cout = cond.constructor.code, cond.constructor.out
	local start = typedb:get_instance( "label")()
	local startcode = utils.constructor_format( llvmir.control.label, {inp=start})
	return {code = startcode .. ccode .. arg[2].code .. utils.constructor_format( llvmir.control.invertedControlType, {out=start,inp=cout})}
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
	local typeId,constructor = selectNoArgumentType( node, typeName, typedb:resolve_type( getContextTypes(), typeName, tagmask_resolveType))
	return {type=typeId, constructor=constructor}
end

function typesystem.linkage( node, llvm_linkage)
	return llvm_linkage
end
function typesystem.funcdef( node, contextTypeId)
	local arg = utils.traverseRange( typedb, node, {1,#node.arg-1}, contextTypeId)
	arg[#node.arg] = utils.traverseRange( typedb, node, {#node.arg-1,#node.arg}, contextTypeId, arg[2])[#node.arg]
	local descr = {lnk = arg[1].linkage, attr = arg[1].attributes, ret = arg[2], name = arg[3], param = arg[4].param, body = arg[4].code}
	defineCallable( node, descr, contextTypeId)
end
function typesystem.procdef( node, contextTypeId)
	local arg = utils.traverse( typedb, node, contextTypeId, 0)
	local descr = {lnk = arg[1].linkage, attr = arg[1].attributes, name = arg[2], param = arg[3].param, body = arg[3].code .. "ret void\n" }
	defineCallable( node, descr, contextTypeId)
end
function typesystem.callablebody( node, contextTypeId, rtype) 
	defineCallableBodyContext( rtype)
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
function typesystem.main_procdef( node)
	defineCallableBodyContext( scalarIntegerType)
	local arg = utils.traverse( typedb, node)
	local descr = {body = arg[1].code}
	print( "\n" .. utils.constructor_format( llvmir.control.mainDeclaration, descr))
end
function typesystem.program( node)
	initBuiltInTypes()
	utils.traverse( typedb, node)
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
