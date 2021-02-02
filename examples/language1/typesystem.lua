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
local tagmask_declaration = typedb.reduction_tagmask( tag_typeDeclaration)
local tagmask_resolveType = typedb.reduction_tagmask( tag_typeDeduction, tag_typeDeclaration)
local tagmask_matchParameter = typedb.reduction_tagmask( tag_typeDeduction, tag_typeDeclaration, tag_TypeConversion)
local tagmask_typeConversion = typedb.reduction_tagmask( tag_TypeConversion)
local tagmask_typeNameSpace = typedb.reduction_tagmask( tag_typeNamespace)

local weightEpsilon = 1E-5	-- epsilon used for comparing weights for equality
local scalarQualiTypeMap = {}	-- maps scalar type names without qualifiers to the table of type ids for all qualifiers possible
local scalarIndexTypeMap = {}	-- maps scalar type names usable as index without qualifiers to the const type id used as index for [] operators or pointer arithmetics
local scalarBooleanType = nil	-- type id of the boolean type, result of cmpop binary operators
local scalarIntegerType = nil	-- type id of the main integer type, result of main function
local stringPointerType = nil	-- type id of the string constant type used for free string litterals
local memPointerType = nil	-- type id of the type used for result of allocmem and argument of freemem
local stringAddressType = nil	-- type id of the string constant type used string constants outsize a function scope
local controlTrueType = nil	-- type implementing a boolean not represented as value but as peace of code (in the constructor) with a falseExit label
local controlFalseType = nil	-- type implementing a boolean not represented as value but as peace of code (in the constructor) with a trueExit label
local qualiTypeMap = {}		-- maps any defined type id without qualifier to the table of type ids for all qualifiers possible
local referenceTypeMap = {}	-- maps any defined type id to its reference type
local dereferenceTypeMap = {}	-- maps any defined type id to its type with the reference qualifier stripped away
local constTypeMap = {}		-- maps any defined type id to its const type
local privateTypeMap = {}	-- maps any defined type id to its private type
local pointerTypeMap = {}	-- maps any defined type id to its pointer type
local typeQualiSepMap = {}	-- maps any defined type id to a separation pair (lval,qualifier) = lval type (strips away qualifiers), qualifier string
local typeDescriptionMap = {}	-- maps any defined type id to its llvmir template structure
local stringConstantMap = {}    -- maps string constant values to a structure with its attributes {fmt,name,size}
local arrayTypeMap = {}		-- maps the pair lval,size to the array type lval for an array size 
local varargFuncMap = {}	-- maps types to true for vararg functions/procedures
local varargParameterMap = {}	-- maps any defined type id to the structure {llvmtype,type} where to map additional vararg types to
local instantCallable = nil	-- callable structure for implicitely generated code (constructors,destructors,assignments,etc.)

function createCallableInstance( scope, rtype)
	return {scope=scope, register=utils.register_allocator(), label=utils.label_allocator(), returntype=rtype}
end
function defineCallableInstance( scope, rtype)
	typedb:set_instance( "callable", createCallableInstance( scope, rtype))
end
function getCallableInstance()
	return instantCallable or typedb:get_instance( "callable")
end

function constructorParts( constructor)
	if type(constructor) == "table" then return constructor.code or "",constructor.out else return "",tostring(constructor) end
end
function convConstructor( fmt)
	return function( constructor)
		local callable = getCallableInstance()
		local out = callable.register()
		local code,inp = constructorParts( constructor)
		return {code = code .. utils.constructor_format( fmt, {inp = inp, out = out}, callable.register), out = out}
	end
end
function manipConstructor( fmt)
	return function( this)
		local code,inp = constructorParts( this)
		return {code = code .. utils.constructor_format( fmt, {this=inp})}
	end
end
function assignConstructor( fmt)
	return function( this, arg)
		local callable = getCallableInstance()
		local code = ""
		local this_code,this_inp = constructorParts( this)
		local arg_code,arg_inp = constructorParts( arg[1])
		local subst = {arg1 = arg_inp, this = this_inp}
		code = code .. this_code .. arg_code
		return {code = code .. utils.constructor_format( fmt, subst, callable.register), out = this_inp}
	end
end
function callConstructor( fmt)
	return function( this, args)
		local callable = getCallableInstance()
		local out = callable.register()
		local this_code,this_inp = constructorParts( this)
		local subst = {out = out, this = this_inp}
		local code = this_code
		if args then
			for ii=1,#args do
				local arg_code,arg_inp = constructorParts( args[ ii])
				code = code .. arg_code
				subst[ "arg" .. ii] = arg_inp
			end
		end
		return {code = code .. utils.constructor_format( fmt, subst, callable.register), out = out}
	end
end
function callableCallConstructor( fmt, thisTypeId, sep, argvar)
	return function( this, args, llvmtypes)
		local callable = getCallableInstance()
		local out = callable.register()
		local this_code,this_inp = constructorParts( this)
		local code = this_code
		local argstr; if thisTypeId ~= 0 then argstr = typeDescriptionMap[ thisTypeId].llvmtype .. " " .. this_inp else argstr = ""
		end
		for ii=1,#args do
			local arg = args[ ii]
			local llvmtype = llvmtypes[ ii]
			local arg_code,arg_inp = constructorParts( arg)
			code = code .. arg_code
			if llvmtype then
				if argstr == "" then argstr = llvmtype .. " " .. arg_inp else argstr = argstr .. sep .. llvmtype .. " " .. arg_inp end
			else
				if argstr == "" then argstr = "i32 0" else argstr = argstr .. sep .. "i32 0" end
			end
		end
		local subst = {out = out, [argvar] = argstr}
		return {code = code .. utils.constructor_format( fmt, subst, callable.register), out = out}
	end
end
function assignStructureConstructor( node, thisTypeId, members)
	return function( this, args)
		args = args[1] -- args contains one list node
		if #args ~= #members then 
			utils.errorMessage( node.line, "Number of elements %d in structure do not match for '%s' [%d]", #args, typedb:type_string( thisTypeId), #members)
		end
		local qualitype = qualiTypeMap[ thisTypeId]
		local descr = typeDescriptionMap[ thisTypeId]
		local callable = getCallableInstance()
		local this_code,this_inp = constructorParts( this)
		local rt = {code=this_code,out=this_inp}
		for mi,member in ipairs( members) do
			local out = callable.register()
			local code = utils.constructor_format( member.loadref, {out = out, this = this_inp}, callable.register)
			local reftype = referenceTypeMap[ member.type]
			local maparg = applyCallable( node, {type=reftype,constructor={out=out,code=code}}, ":=", {args[mi]})
			rt.code = rt.code .. maparg.constructor.code
		end
		return rt
	end
end
function tryCreateParameter( node, callable, typeId, arg)
	local constructor,llvmtype,weight = getParameterConstructor( node, typeId, arg)
	if weight then
		return {type=typeId,constructor=constructor}
	else
		local derefTypeId = dereferenceTypeMap[ typeId] or typeId
		local refTypeId = referenceTypeMap[ typeId] or typeId
		local descr = typeDescriptionMap[ derefTypeId]
		local out = callable.register()
		local code = utils.constructor_format( descr.def_local, {out=out}, callable.register)
		return tryApplyCallable( node, {type=refTypeId,constructor={out=out,code=code}}, ":=", {arg})
	end
end
function recursiveResolveConstructor( node, thisTypeId, opr)
	return function( this, args)
		local rt = {}
		if #args == 1 and args[1].type == constexprStructureType then args = args[1] end
		local callable = getCallableInstance()
		local descr = typeDescriptionMap[ thisTypeId]
		local this_code,this_inp = constructorParts( this)
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
					table.insert( rt, applyCallable( node, {type=thisTypeId, constructor={code=this_code,out=this_inp}}, ":=", param_ar))
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
function assignClassConstructor( node, thisTypeId)
	return recursiveResolveConstructor( node, thisTypeId, ":=")
end
function promoteCallConstructor( call_constructor, promote_constructor)
	return function( this, arg)
		return call_constructor( promote_constructor( this), arg)
	end
end
function definePromoteCall( returnType, thisType, promoteType, opr, argTypes, promote_constructor)
	local call_constructor = typedb:type_constructor( typedb:get_type( promoteType, opr, argTypes))
	local callType = typedb:def_type( thisType, opr, promoteCallConstructor( call_constructor, promote_constructor), argTypes)
	if returnType then typedb:def_reduction( returnType, callType, nil, tag_typeDeclaration) end
end
function defineCall( returnType, thisType, opr, argTypes, constructor, priority)
	local callType = typedb:def_type( thisType, opr, constructor, argTypes, priority)
	if callType == -1 then utils.errorMessage( 0, "Duplicate definition of type '%s %s'", typedb:type_string(thisType), opr) end
	if returnType then typedb:def_reduction( returnType, callType, nil, tag_typeDeclaration) end
	return callType
end

local constexprIntegerType = typedb:def_type( 0, "constexpr int")
local constexprUIntegerType = typedb:def_type( 0, "constexpr uint")
local constexprFloatType = typedb:def_type( 0, "constexpr float")
local constexprBooleanType = typedb:def_type( 0, "constexpr bool")
local constexprNullType = typedb:def_type( 0, "constexpr null")
local constexprStructureType = typedb:def_type( 0, "constexpr struct")

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

	dereferenceTypeMap[ qualitype.rpval] = priv_pval
	dereferenceTypeMap[ qualitype.c_rpval] = priv_c_pval

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

	varargParameterMap[ lval] = {llvmtype=typeDescription.llvmtype,type=c_lval}
	varargParameterMap[ c_lval] = {llvmtype=typeDescription.llvmtype,type=c_lval}
	varargParameterMap[ rval] = {llvmtype=typeDescription.llvmtype,type=c_lval}
	varargParameterMap[ c_rval] = {llvmtype=typeDescription.llvmtype,type=c_lval}
	varargParameterMap[ pval] = {llvmtype=pointerTypeDescription.llvmtype,type=c_pval}
	varargParameterMap[ c_pval] = {llvmtype=pointerTypeDescription.llvmtype,type=c_pval}
	varargParameterMap[ rpval] = {llvmtype=pointerTypeDescription.llvmtype,type=c_pval}
	varargParameterMap[ c_rpval] = {llvmtype=pointerTypeDescription.llvmtype,type=c_pval}

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
	referenceTypeMap[ pval] = rpval
	referenceTypeMap[ c_pval] = c_rpval

	dereferenceTypeMap[ rval] = lval
	dereferenceTypeMap[ c_rval] = c_lval
	dereferenceTypeMap[ rpval] = pval
	dereferenceTypeMap[ c_rpval] = c_pval

	constTypeMap[ lval] = c_lval
	constTypeMap[ rval] = c_rval
	constTypeMap[ pval] = c_pval
	constTypeMap[ rpval] = c_rpval

	pointerTypeMap[ lval] = pval
	pointerTypeMap[ c_lval] = c_pval
	pointerTypeMap[ rval] = pval
	pointerTypeMap[ c_rval] = c_pval

	typeQualiSepMap[ lval] = {lval=lval,qualifier=""}
	typeQualiSepMap[ c_lval] = {lval=lval,qualifier="const "}
	typeQualiSepMap[ rval] = {lval=lval,qualifier="&"}
	typeQualiSepMap[ c_rval] = {lval=lval,qualifier="const&"}
	typeQualiSepMap[ pval] = {lval=lval,qualifier="^"}
	typeQualiSepMap[ c_pval] = {lval=lval,qualifier="const^"}
	typeQualiSepMap[ rpval] = {lval=lval,qualifier="^&"}
	typeQualiSepMap[ c_rpval] = {lval=lval,qualifier="const^&"}

	typedb:def_reduction( c_pval, constexprNullType, function() return {code="",out="null"} end, tag_TypeConversion, 0.03125)
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
function definePointerConstructors( qualitype, descr)
	local pointer_descr = llvmir.pointerDescr( descr)
	defineCall( qualitype.rpval, qualitype.rpval, "=",  {qualitype.c_pval}, assignConstructor( pointer_descr.assign))
	defineCall( qualitype.rpval, qualitype.rpval, ":=", {qualitype.c_pval}, assignConstructor( pointer_descr.assign))
	defineCall( qualitype.rpval, qualitype.rpval, ":=", {qualitype.c_rpval}, assignConstructor( pointer_descr.ctor_copy))
	defineCall( qualitype.rpval, qualitype.rpval, ":=", {}, manipConstructor( pointer_descr.ctor))
	defineCall( 0, qualitype.rpval, ":~", {}, function() return {code=""} end)
end
function defineScalarDestructors( qualitype, descr)
	defineCall( 0, qualitype.pval, " delete", {}, function() return {code=""} end)
	defineCall( 0, qualitype.rval, ":~", {}, function() return {code=""} end)
end
function defineScalarConstructors( qualitype, descr)
	defineCall( qualitype.rval, qualitype.rval, "=",  {qualitype.c_lval}, assignConstructor( descr.assign))
	defineCall( qualitype.rval, qualitype.rval, ":=", {qualitype.c_lval}, assignConstructor( descr.assign))	
	defineCall( qualitype.rval, qualitype.rval, ":=", {qualitype.c_rval}, assignConstructor( descr.ctor_copy))
	defineCall( qualitype.rval, qualitype.rval, ":=", {}, manipConstructor( descr.ctor))
	definePointerConstructors( qualitype, descr)
	defineScalarDestructors( qualitype, descr)
end
function defineArrayConstructors( node, qualitype, descr, memberTypeId)
	definePointerConstructors( qualitype, descr)

	instantCallable = createCallableInstance( node.scope, nil)
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
		defineCall( qualitype.rval, qualitype.rval, ":=", {qualitype.c_rval}, manipConstructor( utils.template_format( descr.ctor_copy, {procname="copy"})))
	end
	if assign then
		print_section( "Auto", utils.template_format( descr.ctorproc_copy, {procname="assign",ctors=assign.constructor.code}))
		defineCall( qualitype.rval, qualitype.rval, "=", {qualitype.c_rval}, manipConstructor( utils.template_format( descr.ctor_copy, {procname="assign"})))
	end
	if destroy then
		print_section( "Auto", utils.template_format( descr.dtorproc, {dtors=destroy.constructor.code}))
		defineCall( 0, qualitype.rval, ":~", {}, manipConstructor( descr.dtor))
		defineCall( 0, qualitype.pval, " delete", {}, manipConstructor( descr.dtor))
	end
	instantCallable = nil
end
function defineStructConstructors( node, qualitype, descr, context)
	definePointerConstructors( qualitype, descr)

	instantCallable = createCallableInstance( node.scope, nil)
	local ctors,dtors,ctors_copy,ctors_assign,ctors_elements,paramstr,argstr,elements = "","","","","","","",{}

	for mi,member in ipairs(context.members) do
		table.insert( elements, member.type)
		paramstr = paramstr .. ", " .. typeDescriptionMap[ member.type].llvmtype .. " %p" .. mi
		argstr = argstr .. ", " .. typeDescriptionMap[ member.type].llvmtype .. " {arg" .. mi .. "}"

		local out = instantCallable.register()
		local inp = instantCallable.register()
		local llvmtype = member.descr.llvmtype
		local c_member_type = constTypeMap[ member.type] or member.type
		local member_reftype = referenceTypeMap[ member.type]
		local c_member_reftype = constTypeMap[ member_reftype] or member_reftype
		local loadref = context.descr.loadref
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
		defineCall( qualitype.rval, qualitype.rval, ":=", {qualitype.rval}, manipConstructor( utils.template_format( descr.ctor_copy, {procname="copy"})))
	end
	if ctors_assign then
		print_section( "Auto", utils.template_format( descr.ctorproc_copy, {procname="assign",ctors=ctors_assign}))
		defineCall( qualitype.rval, qualitype.rval, "=", {qualitype.rval}, manipConstructor( utils.template_format( descr.ctor_copy, {procname="assign"})))
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
	defineCall( qualitype.rval, qualitype.rval, ":=", {constexprStructureType}, assignStructureConstructor( node, qualitype.lval, context.members))
	instantCallable = nil
end

function defineClassConstructors( node, qualitype, descr, context)
	definePointerConstructors( qualitype, descr)

	instantCallable = createCallableInstance( node.scope, nil)
	local dtors = ""
	for mi,member in ipairs(context.members) do
		local out = instantCallable.register()
		local llvmtype = member.descr.llvmtype
		local member_reftype = referenceTypeMap[ member.type]
		local ths = {type=member_reftype,constructor={code=utils.constructor_format(context.descr.loadref,{out=out,this="%ptr",index=mi-1, type=llvmtype}),out=out}}

		local member_destroy = tryApplyCallable( node, ths, ":~", {} )

		if member_destroy and dtors then dtors = dtors .. member_destroy.constructor.code else dtors = nil end
	end
	if dtors then
		print_section( "Auto", utils.template_format( descr.dtorproc, {dtors=dtors}))
		defineCall( 0, qualitype.rval, ":~", {}, manipConstructor( descr.dtor), -1)
		defineCall( 0, qualitype.pval, " delete", {}, manipConstructor( descr.dtor), -1)
	end
	instantCallable = nil
end

function getFunctionThisType( private, const, thisType)
	if private == true then thisType = privateTypeMap[ thisType] end
	if const == true then thisType = constTypeMap[ thisType] end
	return thisType
end
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
function defineOperatorsWithStructArgument( node, context)
	for opr,def in pairs( context.operators) do
		if def.hasStructArgument == true then
			defineCall( def.returnType, def.contextType, opr, {constexprStructureType}, recursiveResolveConstructor( node, def.contextType, opr))
		elseif def.maxNofArguments > 1 then
			utils.errorMessage( node.line, "Operator '%s' defined different instances with more than one argument, but with varying signature", opr)
		end
	end
end

function defineIndexOperators( element_qualitype, pointer_descr)
	for index_typenam, index_type in pairs(scalarIndexTypeMap) do
		defineCall( element_qualitype.rval, element_qualitype.pval, "[]", {index_type}, callConstructor( pointer_descr.index[ index_typnam]))
		defineCall( element_qualitype.c_rval, element_qualitype.c_pval, "[]", {index_type}, callConstructor( pointer_descr.index[ index_typnam]))
	end
end
function definePointerOperators( priv_c_pval, c_pval, pointer_descr)
	for operator,operator_fmt in pairs( pointer_descr.cmpop) do
		defineCall( scalarBooleanType, c_pval, operator, {c_pval}, callConstructor( operator_fmt))
		if priv_c_pval then defineCall( scalarBooleanType, priv_c_pval, operator, {c_pval}, callConstructor( operator_fmt)) end
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
function defineVariableHardcoded( typeId, name, reg)
	local var = typedb:def_type( 0, name, reg)
	typedb:def_reduction( typeId, var, nil, tag_typeDeclaration)
	return var
end
function defineVariable( node, context, typeId, name, initVal)
	local descr = typeDescriptionMap[ typeId]
	local callable = getCallableInstance()
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
			print( utils.constructor_format( descr.def_global_val, {out = out, inp = initVal}))
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
function defineVariableMember( node, context, typeId, name, private)
	local descr = typeDescriptionMap[ typeId]
	local qualisep = typeQualiSepMap[ typeId]
	local memberpos = context.structsize
	local load_ref = utils.template_format( context.descr.loadref, {index=#context.members, type=descr.llvmtype})
	local load_val = utils.template_format( context.descr.load, {index=#context.members, type=descr.llvmtype})

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
	if context.llvmtype ~= "" then context.llvmtype = context.llvmtype  .. ", " .. descr.llvmtype else context.llvmtype = descr.llvmtype end
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
function defineReductionToMember( objTypeId, name)
	memberTypeId = typedb:get_type( objTypeId, name)
	typedb:def_reduction( memberTypeId, objTypeId, typedb:type_constructor( memberTypeId), tag_typeDeclaration, 0.125/32)
end
function defineInheritance( context, name, private)
	if private == true then
		defineReductionToMember( context.qualitype.priv_rval, name)
		defineReductionToMember( context.qualitype.priv_c_rval, name)
	else
		defineReductionToMember( context.qualitype.rval, name)
		defineReductionToMember( context.qualitype.c_rval, name)
		defineReductionToMember( context.qualitype.c_lval, name)
	end
end

function defineParameter( node, type, name, callable)
	local descr = typeDescriptionMap[ type]
	local paramreg = callable.register()
	local var = typedb:def_type( 0, name, paramreg)
	if var < 0 then utils.errorMessage( node.line, "Duplicate definition of parameter '%s'", name) end
	typedb:def_reduction( type, var, nil, tag_typeDeclaration)
	return {type=type, llvmtype=descr.llvmtype, reg=paramreg}
end

function initControlBooleanTypes()
	controlTrueType = typedb:def_type( 0, " controlTrueType")
	controlFalseType = typedb:def_type( 0, " controlFalseType")

	function falseExitToBoolean( constructor)
		local callable = getCallableInstance()
		local out = callable.register()
		return {code = constructor.code .. utils.constructor_format(llvmir.control.falseExitToBoolean,{falseExit=constructor.out,out=out},callable.label),out=out}
	end
	function trueExitToBoolean( constructor)
		local callable = getCallableInstance()
		local out = callable.register()
		return {code = constructor.code .. utils.constructor_format( llvmir.control.trueExitToBoolean,{trueExit=constructor.out,out=out},callable.label),out=out}
	end
	typedb:def_reduction( scalarBooleanType, controlTrueType, falseExitToBoolean, tag_typeDeduction, 0.1)
	typedb:def_reduction( scalarBooleanType, controlFalseType, trueExitToBoolean, tag_typeDeduction, 0.1)

	function booleanToFalseExit( constructor)
		local callable = getCallableInstance()
		local out = callable.label()
		return {code = constructor.code .. utils.constructor_format( llvmir.control.booleanToFalseExit, {inp=constructor.out, out=out}, callable.label),out=out}
	end
	function booleanToTrueExit( constructor)
		local callable = getCallableInstance()
		local out = callable.label()
		return {code = constructor.code .. utils.constructor_format( llvmir.control.booleanToTrueExit, {inp=constructor.out, out=out}, callable.label),out=out}
	end

	typedb:def_reduction( controlTrueType, scalarBooleanType, booleanToFalseExit, tag_typeDeduction, 0.1)
	typedb:def_reduction( controlFalseType, scalarBooleanType, booleanToTrueExit, tag_typeDeduction, 0.1)

	function negateControlTrueType( this) return {type=controlFalseType, constructor=this.constructor} end
	function negateControlFalseType( this) return {type=controlTrueType, constructor=this.constructor} end

	function joinControlTrueTypeWithBool( this, arg)
		local out = this.out
		local code2 = utils.constructor_format( llvmir.control.booleanToFalseExit, {inp=arg[1].out, out=out}, getCallableInstance().label())
		return {code=this.code .. arg[1].code .. code2, out=out}
	end
	function joinControlFalseTypeWithBool( this, arg)
		local out = this.out
		local code2 = utils.constructor_format( llvmir.control.booleanToTrueExit, {inp=arg[1].out, out=out}, getCallableInstance().label())
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
			local callable = getCallableInstance()
			return {code= this.code .. utils.constructor_format( llvmir.control.terminateTrueExit,{out=this.out},callable.label), out=this.out}
		end
	end
	function joinControlTrueTypeWithConstexprBool( this, arg)
		if arg == true then
			return this
		else 
			local callable = getCallableInstance()
			return {code= this.code .. utils.constructor_format( llvmir.control.terminateFalseExit,{out=this.out},callable.label), out=this.out}
		end
	end
	defineCall( controlTrueType, controlTrueType, "&&", {constexprBooleanType}, joinControlTrueTypeWithConstexprBool)
	defineCall( controlFalseType, controlFalseType, "||", {constexprBooleanType}, joinControlFalseTypeWithConstexprBool)

	function constexprBooleanToControlTrueType( value)
		local callable = getCallableInstance()
		local out = label()
		local code; if value == true then code="" else code=utils.constructor_format( llvmir.control.terminateFalseExit, {out=out}, callable.label) end
		return {code=code, out=out}
	end
	function constexprBooleanToControlFalseType( value)
		local callable = getCallableInstance()
		local out = label()
		local code; if value == false then code="" else code=utils.constructor_format( llvmir.control.terminateFalseExit, {out=out}, callable.label) end
		return {code=code, out=out}
	end
	typedb:def_reduction( controlFalseType, constexprBooleanType, constexprBooleanToControlFalseType, tag_typeDeduction, 0.1)
	typedb:def_reduction( controlTrueType, constexprBooleanType, constexprBooleanToControlTrueType, tag_typeDeduction, 0.1)

	function invertControlBooleanType( this)
		local out = getCallableInstance().label()
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
				local code,out = constructorParts( constructor)
				return item,{code=code,out=out}
			else
				return item
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
function callFunction( node, contextTypes, name, args)
	local typeId,constructor = selectNoArgumentType( node, name, typedb:resolve_type( contextTypes, name, tagmask_resolveType))
	local this = {type=typeId, constructor=constructor}
	return applyCallable( node, this, "()", args)
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
			if redu.constructor then
				redu_constructor = redu.constructor( redu_constructor)
			end
		end
	end
	return redu_constructor,weight
end
function getParameterConstructor( node, redu_type, operand)
	local constructor,weight = getReductionConstructor( node, redu_type, operand)
	if weight then
		local vp = typeDescriptionMap[ redu_type]
		if vp then return constructor,vp.llvmtype,weight else return constructor,nil,weight end
	end
end
function getVarargConstructor( node, operand)
	local vp = varargParameterMap[ operand.type]
	if not vp then
		local redulist = typedb:get_reductions( operand.type, tagmask_declaration)
		for ri,redu in ipairs( redulist) do
			vp = varargParameterMap[ redu.type]
			if vp then break end
		end
	end
	if vp then
		local constructor,weight = getReductionConstructor( node, vp.type, operand)
		return constructor,vp.llvmtype,weight
	end
end
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
						local param_constructor,param_llvmtype,param_weight = getParameterConstructor( node, parameters[ pi].type, args[ pi])
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
						local param_constructor,param_llvmtype,param_weight = getParameterConstructor( node, parameters[ pi].type, args[ pi])
						if not param_weight then break end
						weight = weight + param_weight
						table.insert( param_constructor_ar, param_constructor)
						table.insert( param_llvmtype_ar, param_llvmtype)
					end
					if #parameters == #param_constructor_ar then
						for ai=#parameters+1,#args do
							local param_constructor,param_llvmtype,param_weight = getVarargConstructor( node, args[ai])
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
function applyCallable( node, this, callable, args)
	local bestmatch,bestweight = findApplyCallable( node, this, callable, args)
	if not bestweight then
		utils.errorMessage( node.line, "Failed to find callable with signature '%s'",
	                    utils.resolveTypeString( typedb, this.type, callable) .. "(" .. utils.typeListString( typedb, args, ", ") .. ")")
	end
	return getCallableBestMatch( node, bestmatch, bestweight)
end
function tryApplyCallable( node, this, callable, args)
	local bestmatch,bestweight = findApplyCallable( node, this, callable, args)
	if bestweight then return getCallableBestMatch( node, bestmatch, bestweight) else return nil end
end
function getSignatureString( name, args, context)
	if not context then
		return utils.uniqueName( name .. "__")
	else
		local pstr; if context.symbol then pstr = "__C_" .. context.symbol .. "__" .. name else pstr = name end
		for ai,arg in ipairs(args) do pstr = pstr .. "__" .. arg.llvmtype end
		return utils.encodeName(  pstr)
	end
end
function getParameterString( args)
	local rt = ""
	for ai,arg in ipairs(args) do if rt == "" then rt = rt .. arg.llvmtype .. " " .. arg.reg else rt = rt .. ", " .. arg.llvmtype .. " " .. arg.reg end end
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
function defineFunctionCall( thisTypeId, contextTypeId, opr, descr)
	local callfmt
	if descr.ret then descr.rtype = typeDescriptionMap[ descr.ret].llvmtype else descr.rtype = "void" end
	callfmt = utils.template_format( descr.call, descr)
	local functype = defineCall( descr.ret, contextTypeId, opr, descr.param, callableCallConstructor( callfmt, thisTypeId, ", ", "callargstr"))
	if descr.vararg then varargFuncMap[ functype] = true end
end
function defineCallableType( node, descr, context)
	local thisTypeId = 0; if context and context.qualitype then thisTypeId = getFunctionThisType( descr.private, descr.const, context.qualitype.rval) end
	local callableContextTypeId = typedb:get_type( thisTypeId, descr.name)
	if not callableContextTypeId then callableContextTypeId = typedb:def_type( thisTypeId, descr.name) end
	defineFunctionCall( thisTypeId, callableContextTypeId, "()", descr)
end
function defineCallable( node, descr, context)
	descr.thisstr = getThisParameterString( context, descr.param)
	descr.paramstr = getParameterString( descr.param)
	descr.symbolname = getSignatureString( descr.symbol, descr.param, context)
	defineCallableType( node, descr, context)
end
function defineOperatorType( node, descr, context)
	local contextTypeId = 0; if context and context.qualitype then contextTypeId = getFunctionThisType( descr.private, descr.const, context.qualitype.rval) end
	defineFunctionCall( contextTypeId, contextTypeId, descr.name, descr)
end
function defineOperator( node, descr, context)
	descr.thisstr = getThisParameterString( context, descr.param)
	descr.paramstr = getParameterString( descr.param)
	descr.symbolname = getSignatureString( descr.symbol, descr.param, context)
	defineOperatorType( node, descr, context)
end
function printFunction( node, descr, context)
	print( "\n" .. utils.constructor_format( llvmir.control.functionDeclaration, descr))
end
function defineExternCallable( node, descr)
	descr.argstr = getParameterLlvmTypeString( descr.param)
	if descr.externtype == "C" then
		descr.symbolname = descr.name
	else
		utils.errorMessage( node.line, "Unknown extern call type \"%s\" (must be one of {\"C\"})", descr.externtype)
	end
	if descr.vararg then
		descr.signature = "(" .. descr.argstr .. ", ...)"
	end
	defineCallableType( node, descr, nil)
	local declfmt; if descr.vararg then declfmt = llvmir.control.extern_functionDeclaration_vararg else declfmt = llvmir.control.extern_functionDeclaration end
	print( "\n" .. utils.constructor_format( declfmt, descr))
end
function defineInterfaceCallable( node, descr, context)
	descr.argstr = getParameterLlvmTypeString( descr.param)
	local sep; if descr.argstr ~= "" then sep = "" else sep = ", " end
	descr.signature = "(i8*" .. sep .. descr.argstr .. ")"
end
function defineCallableBodyContext( node, context, descr)
	local prev_scope = typedb:scope( node.scope)
	if context and context.qualitype then
		local privateThisReferenceType = getFunctionThisType( true, descr.const, context.qualitype.rval)
		local privateThisPointerType = getFunctionThisType( true, descr.const, context.qualitype.pval)
		defineVariableHardcoded( privateThisPointerType, "this", "%ths")
		local classvar = defineVariableHardcoded( privateThisReferenceType, "*this", "%ths")
		addContextTypeConstructorPair( {type=classvar, constructor={out="%ths"}})
	end
	defineCallableInstance( node.scope, descr.ret)
	typedb:scope( prev_scope)
end
function defineMainProcContext( node)
	local prev_scope = typedb:scope( node.scope)
	defineVariableHardcoded( stringPointerType, "argc", "%argc")
	defineVariableHardcoded( scalarIntegerType, "argv", "%argv")
	defineCallableInstance( node.scope, scalarIntegerType)
	typedb:scope( prev_scope)
end
function getStringConstant( value)
	if not stringConstantMap[ value] then
		local encval,enclen = utils.encodeLexemLlvm(value)
		local name = utils.uniqueName( "string")
		stringConstantMap[ value] = {fmt=utils.template_format( llvmir.control.stringConstConstructor, {name=name,size=enclen+1}),name=name,size=enclen+1}
		print( utils.constructor_format( llvmir.control.stringConstDeclaration, {out="@" .. name, size=enclen+1, value=encval}) .. "\n")
	end
	local callable = getCallableInstance()
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
	return defineParameter( node, arg[1], arg[2], getCallableInstance())
end

function typesystem.paramdeflist( node)
	return utils.traverse( typedb, node)
end

function typesystem.structure( node, context)
	local arg = utils.traverse( typedb, node, context)
	return {type=constexprStructureType, constructor=arg}
end
function typesystem.allocate( node, context)
	local callable = getCallableInstance()
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
	local cast = utils.constructor_format( llvmir.control.memPointerCast, {llvmtype=descr.llvmtype, out=out,inp=memblk.constructor.out}, callable.register)
	local rt = applyCallable( node, {type=refTypeId,constructor={out=out, code=memblk.constructor.code .. cast}}, ":=", {args[2]})
	rt.type = pointerTypeId
	return rt
end
function typesystem.typecast( node, context)
	local callable = getCallableInstance()
	local args = utils.traverse( typedb, node, context)
	local typeId = args[1]
	local operand = args[2]
	local descr = typeDescriptionMap[ typeId]
	if operand then
		return getReductionConstructor( node, typeId, operand)
	else
		local out = callable.register()
		local code = utils.constructor_format( descr.def_local, {out=out}, callable.register)
		return tryApplyCallable( node, {type=typeId,constructor={out=out,code=code}}, ":=", {})
	end
end
function typesystem.delete( node, context)
	local callable = getCallableInstance()
	local args = utils.traverse( typedb, node, context)
	local typeId = args[1].type
	local code,out = constructorParts( args[1].constructor)
	local res = applyCallable( node, {type=typeId,constructor={code=code,out=out}}, " delete")
	local pointerType = typedb:type_context( res.type)
	local lval = typeQualiSepMap[ pointerType].lval
	local descr = typeDescriptionMap[ lval]
	local cast_out = callable.register()
	local cast = utils.constructor_format( llvmir.control.bytePointerCast, {llvmtype=descr.llvmtype, out=cast_out, inp=out}, callable.register)
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
	local callable = getCallableInstance()
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
	local callable = getCallableInstance()
	local exit = callable.label()
	local elsecode = utils.constructor_format( llvmir.control.invertedControlType, {inp=cond_constructor.out, out=exit})
	return {code = cond_constructor.code .. arg[2].code .. elsecode .. arg[3].code .. utils.constructor_format( llvmir.control.label, {inp=exit})}
end
function typesystem.conditional_while( node)
	local arg = utils.traverse( typedb, node)
	local cond_constructor = getReductionConstructor( node, controlTrueType, arg[1])
	if not cond_constructor then utils.errorMessage( node.line, "Can't use type '%s' as a condition", typedb:type_string(arg[1].type)) end
	local callable = getCallableInstance()
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
	local callable = getCallableInstance()
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
	local descr = {call = llvmir.control.functionCall, lnk = arg[1].linkage, attr = llvmir.functionAttribute( arg[1].private), signature="",
			name = arg[2], symbol = arg[2], ret = arg[3], private=arg[1].private, const=decl.const }
	descr.param = utils.traverseRange( typedb, node, {4,4}, context, descr, 1)[4]
	defineCallable( node, descr, context)
	descr.body  = utils.traverseRange( typedb, node, {4,4}, context, descr, 2)[4]
	printFunction( node, descr, context)
end
function typesystem.procdef( node, decl, context)
	local arg = utils.traverseRange( typedb, node, {1,2}, context)
	local descr = {call = llvmir.control.procedureCall, lnk = arg[1].linkage, attr = llvmir.functionAttribute( arg[1].private), signature="",
			name = arg[2], symbol = arg[2], ret = nil, private=arg[1].private, const=decl.const }
	descr.param = utils.traverseRange( typedb, node, {3,3}, context, descr, 1)[3]
	defineCallable( node, descr, context)
	descr.body  = utils.traverseRange( typedb, node, {3,3}, context, descr, 2)[3] .. "ret void\n"
	printFunction( node, descr, context)
end
function typesystem.operatordecl( node, opr)
	return opr
end
function typesystem.operator_funcdef( node, decl, context)
	local arg = utils.traverseRange( typedb, node, {1,3}, context)
	local descr = {call = llvmir.control.functionCall, lnk = arg[1].linkage, attr = llvmir.functionAttribute( arg[1].private), signature="",
			name = arg[2].name, symbol = "$" .. arg[2].symbol, ret = arg[3], private=arg[1].private, const=decl.const}
	descr.param = utils.traverseRange( typedb, node, {4,4}, context, descr, 1)[4]
	defineOperator( node, descr, context)
	descr.body  = utils.traverseRange( typedb, node, {4,4}, context, descr, 2)[4]
	printFunction( node, descr, context)
	defineOperatorAttributes( context, descr)
end
function typesystem.operator_procdef( node, decl, context)
	local arg = utils.traverseRange( typedb, node, {1,2}, context)
	local descr = {call = llvmir.control.procedureCall, lnk = arg[1].linkage, attr = llvmir.functionAttribute( arg[1].private), signature="",
			name = arg[2].name, symbol = "$" .. arg[2].symbol, ret = nil, private=arg[1].private, const=decl.const}
	descr.param = utils.traverseRange( typedb, node, {3,3}, context, descr, 1)[3]
	defineOperator( node, descr, context)
	descr.body  = utils.traverseRange( typedb, node, {3,3}, context, descr, 2)[3] .. "ret void\n"
	printFunction( node, descr, context)
	defineOperatorAttributes( context, descr)
end
function typesystem.constructordef( node, context)
	local arg = utils.traverseRange( typedb, node, {1,1}, context)
	local descr = {call = llvmir.control.procedureCall, lnk = arg[1].linkage, attr = llvmir.functionAttribute( arg[1].private), signature="",
			name = ":=", symbol = "$ctor", ret = nil, private=arg[1].private, const=false}
	descr.param = utils.traverseRange( typedb, node, {2,2}, context, descr, 1)[2]
	defineOperator( node, descr, context)
	descr.body  = utils.traverseRange( typedb, node, {2,2}, context, descr, 2)[2] .. "ret void\n"
	printFunction( node, descr, context)
	defineOperatorAttributes( context, descr)
end
function typesystem.destructordef( node, context)
	local arg = utils.traverseRange( typedb, node, {1,1}, context)
	local descr = {lnk = arg[1].linkage, attr = llvmir.functionAttribute( arg[1].private), signature="",
	               name = ":~", symbol = "$dtor", ret = nil, private=false, const=false }
	descr.param = utils.traverseRange( typedb, node, {2,2}, context, descr, 1)[2]
	defineOperator( node, descr, context)
	descr.body  = utils.traverseRange( typedb, node, {2,2}, context, descr, 2)[2] .. "ret void\n"
	printFunction( node, descr, context)
end
function typesystem.callablebody( node, context, descr, select)
	-- HACK (PatrickFrey 1/20/2021): This function is called twice (select 1 or 2), once for parameter traversal, once for body traversal, to enable self reference
	if select == 1 then
		defineCallableBodyContext( node, context, descr)
		local arg = utils.traverseRange( typedb, node, {1,1}, context)
		return arg[1]
	else
		local arg = utils.traverseRange( typedb, node, {2,2}, context)
		return arg[2].code
	end
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
	local descr = {call = llvmir.control.functionCall, externtype = arg[1], name = arg[2], symbol = arg[2], ret = arg[3], param = arg[4], vararg=false, signature=""}
	defineExternCallable( node, descr)
end
function typesystem.extern_procdef( node)
	local arg = utils.traverse( typedb, node)
	local descr = {call = llvmir.control.procedureCall, externtype = arg[1], name = arg[2], symbol = arg[2], param = arg[3], vararg=false, signature=""}
	defineExternCallable( node, descr)
end
function typesystem.extern_funcdef_vararg( node)
	local arg = utils.traverse( typedb, node)
	local descr = {call = llvmir.control.functionCall, externtype = arg[1], name = arg[2], symbol = arg[2], ret = arg[3], param = arg[4], vararg=true, signature=""}
	defineExternCallable( node, descr)
end
function typesystem.extern_procdef_vararg( node)
	local arg = utils.traverse( typedb, node)
	local descr = {call = llvmir.control.procedureCall, externtype = arg[1], name = arg[2], symbol = arg[2], param = arg[3], vararg=true, signature=""}
	defineExternCallable( node, descr)
end
function typesystem.interface_funcdef( node, decl, context)
	local arg = utils.traverse( typedb, node)
	local descr = {call = llvmir.control.interfaceFunctionCall, name = arg[1], symbol = arg[1], ret = arg[2], param = arg[3], signature="", const=decl.const}
	defineInterfaceCallable( node, descr, context)
end
function typesystem.interface_procdef( node, decl, context)
	local arg = utils.traverse( typedb, node)
	local descr = {call = llvmir.control.interfaceProcedureCall, name = arg[1], symbol = arg[1], ret = nil, param = arg[2], signature="", const=decl.const}
	defineInterfaceCallable( node, descr, context)
end
function typesystem.interface_operator_funcdef( node, decl, context)
	local arg = utils.traverse( typedb, node)
	local descr = {call = llvmir.control.interfaceFunctionCall, name = arg[1].name, symbol = arg[1].symbol, ret = arg[2], param = arg[3], signature="", const=decl.const}
	defineInterfaceCallable( node, descr, context)
end
function typesystem.interface_operator_procdef( node, decl, context)
	local arg = utils.traverse( typedb, node)
	local descr = {call = llvmir.control.interfaceProcedureCall, name = arg[1].name, symbol = arg[1].symbol, ret = nil, param = arg[2], signature="", const=decl.const}
	defineInterfaceCallable( node, descr, context)
end
function typesystem.structdef( node, context)
	local typnam = node.arg[1].value
	local declContextTypeId = getTypeDeclContextTypeId( context)
	local structname = getTypeDeclUniqueName( declContextTypeId, typnam)
	local descr = utils.template_format( llvmir.structTemplate, {structname=structname})
	local qualitype = defineQualifiedTypes( declContextTypeId, typnam, descr)
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
	local context = {qualitype=qualitype, descr=descr, index=0, methods={}, llvmtype="", symbol=interfacename}
	local arg = utils.traverse( typedb, node, context)
	descr.size = context.index
	descr.methods = context.methods
	print_section( "Typedefs", utils.template_format( descr.vmtdef, {llvmtype=context.llvmtype}))
	print_section( "Typedefs", descr.typedef)
end
function typesystem.classdef( node, context)
	local typnam = node.arg[1].value
	local declContextTypeId = getTypeDeclContextTypeId( context)
	local classname = getTypeDeclUniqueName( declContextTypeId, typnam)
	local descr = utils.template_format( llvmir.classTemplate, {classname=classname})
	local qualitype = defineQualifiedTypes( declContextTypeId, typnam, descr)
	definePublicPrivate( declContextTypeId, typnam, descr, qualitype)
	addContextTypeConstructorPair( qualitype.lval)
	local context = {qualitype=qualitype, descr=descr, index=0, private=true, members={}, structsize=0, llvmtype="", operators={}, symbol=classname}
	-- Traversal in two passes, first type and variable declarations, then method definitions
	utils.traverse( typedb, node, context, 1)
	utils.traverse( typedb, node, context, 2)
	descr.size = context.structsize
	definePointerOperators( qualitype.priv_c_pval, qualitype.c_pval, typeDescriptionMap[ qualitype.c_pval])
	defineClassConstructors( node, qualitype, descr, context)
	defineOperatorsWithStructArgument( node, context)
	print_section( "Typedefs", utils.template_format( descr.typedef, {llvmtype=context.llvmtype}))
end
function typesystem.inheritdef( node, pass, context, pass_selected)
	if not pass_selected or pass == pass_selected then
		local arg = utils.traverseRange( typedb, node, {1,1}, context)
		local typeId = arg[1]
		local typnam = typedb:type_name(typeId)
		local private = false
		defineVariableMember( node, context, typeId, typnam, private)
		defineInheritance( context, typnam, private)
		if #node.arg > 1 then
			local next = utils.traverseRange( typedb, node, {2,2}, context)
			return {typeId, next[2]}
		else
			return {typeId}
		end
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
