local mewa = require "mewa"
local llvmir = require "llvmir_language1"
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
local tag_typeNamespace = 3
local tagmask_resolveType = typedb.reduction_tagmask( tag_typeDeduction, tag_typeDeclaration)
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
			subst[ arg .. ii] = arg_inp
		end
		return {code = code .. utils.constructor_format( fmt, subst, register), out = out}
	end
end

local fccQualiTypeMap = {}	-- maps fcc type names without qualifiers to the table of type ids for all qualifiers possible
local fccIndexTypeMap = {}	-- maps fcc type names usable as index without qualifiers to the const type id used as index for [] operators or pointer arithmetics
local fccBooleanType = 0	-- type id of the boolean type, result of cmpop binary operators
local controlTrueType = 0	-- type implementing a boolean not represented as value but as peace of code (in the constructor) with a falseExit label
local controlFalseType = 0	-- type implementing a boolean not represented as value but as peace of code (in the constructor) with a trueExit label
local qualiTypeMap = {}		-- maps any defined type id without qualifier to the table of type ids for all qualifiers possible
local referenceTypeMap = {}	-- maps any defined type id to its reference type
local typeDescriptionMap = {}	-- maps any defined type id to its llvmir template structure


function definePromoteCall( returnType, thisType, opr, argTypes, promote_constructor)
	local constructor = typedb:type_constructor( typedb:get_type( argTypes[1], opr, argTypes ))
	local constructor_promoted_this = function( this, arg) return constructor( promote_constructor(this), arg[1]) end
	local callType = typedb:def_type( thisType, opr, constructor_promoted_this, argTypes)
	if returnType then typedb:def_reduction( returnType, callType, nil, tag_typeDeduction) end
end
function defineCall( returnType, thisType, opr, argTypes, constructor)
	local callType = typedb:def_type( thisType, opr, constructor, argTypes)
	if returnType then typedb:def_reduction( returnType, callType, nil, tag_typeDeduction) end
end

local constexprIntegerType = typedb:def_type( 0, "constexpr int")
local constexprFloatType = typedb:def_type( 0, "constexpr float")
local constexprBooleanType = typedb:def_type( 0, "constexpr bool")
local constexprDqStringType = typedb:def_type( 0, "constexpr dqstring")
local constexprSqStringType = typedb:def_type( 0, "constexpr sqstring")
local bits64 = bcd.bits( 64)

function defineConstExprBasicArithmetics( constexpr_type)
	defineCall( constexpr_type, constexpr_type, "+", {constexpr_type}, function( this, arg) return this + arg[1] end)
	defineCall( constexpr_type, constexpr_type, "-", {constexpr_type}, function( this, arg) return this - arg[1] end)
	defineCall( constexpr_type, constexpr_type, "/", {constexpr_type}, function( this, arg) return this / arg[1] end)
	defineCall( constexpr_type, constexpr_type, "%", {constexpr_type}, function( this, arg) return this % arg[1] end)
end

function defineConstExprBasicArithmeticsPromoted( constexpr_type, arg_type, promote_this)
	defineCall( constexpr_type, constexpr_type, "+", {arg_type}, function( this, arg) return promote_this( this) + arg[1] end)
	defineCall( constexpr_type, constexpr_type, "-", {arg_type}, function( this, arg) return promote_this( this) - arg[1] end)
	defineCall( constexpr_type, constexpr_type, "/", {arg_type}, function( this, arg) return promote_this( this) / arg[1] end)
	defineCall( constexpr_type, constexpr_type, "%", {arg_type}, function( this, arg) return promote_this( this) % arg[1] end)
end

function defineConstExprBitArithmetics( constexpr_type)
	defineCall( constexpr_type, constexpr_type, "&", {constexpr_type}, function( this, arg) return this.bit_and( arg[1], bits64) end)
	defineCall( constexpr_type, constexpr_type, "|", {constexpr_type}, function( this, arg) return this.bit_or( arg[1], bits64) end)
	defineCall( constexpr_type, constexpr_type, "^", {constexpr_type}, function( this, arg) return this.bit_xor( arg[1], bits64) end)
	defineCall( constexpr_type, constexpr_type, "~", {constexpr_type}, function( this, arg) return this.bit_not( bits64) end)
end

function defineConstExprBooleanArithmetics( constexpr_type)
	defineCall( constexpr_type, constexpr_type, "&&", {constexpr_type}, function( this, arg) return this == true and arg[1] == true end)
	defineCall( constexpr_type, constexpr_type, "||", {constexpr_type}, function( this, arg) return this == true or arg[1] == true end)
	defineCall( constexpr_type, constexpr_type, "!", {}, function( this, arg) return this ~= true end)
end

function defineConstExprOperators()
	defineConstExprBasicArithmetics( constexprFloatType)
	defineConstExprBasicArithmetics( constexprIntegerType)
	defineConstExprBasicArithmeticsPromoted( constexprIntegerType, constexprFloatType, function(this) return this:tonumber() end)
	defineConstExprBitArithmetics( constexprIntegerType)
	defineConstExprBooleanArithmetics( constexprBooleanType)

	typedb:def_reduction( constexprBooleanType, constexprIntegerType, function( value) return value ~= "0" end, tag_typeDeduction)
	typedb:def_reduction( constexprBooleanType, constexprFloatType, function( value) return math.abs(value) < math.abs(epsilon) end, tag_typeDeduction)
	typedb:def_reduction( constexprFloatType, constexprIntegerType, function( value) return value:tonumber() end, tag_typeDeduction)
	typedb:def_reduction( constexprIntegerType, constexprFloatType, function( value) return bcd:int( value) end, tag_typeDeduction)

	function bool2bcd( value) if value then return bcd:int("1") else return bcd:int("0") end end
	typedb:def_reduction( constexprIntegerType, constexprBooleanType, bool2bcd, tag_typeDeduction)

	local int_arg_typenam = "long"
	local int_arg_type = typedb:get_type( 0, "const " .. int_arg_typenam)
	definePromoteCall( int_arg_type, constexprIntegerType, "+", {int_arg_type}, nil)
	definePromoteCall( int_arg_type, constexprIntegerType, "-", {int_arg_type}, nil)
	definePromoteCall( int_arg_type, constexprIntegerType, "/", {int_arg_type}, nil)
	definePromoteCall( int_arg_type, constexprIntegerType, "%", {int_arg_type}, nil)
	local uint_arg_typenam = "ulong"
	local uint_arg_type = typedb:get_type( 0, "const " .. uint_arg_typenam)
	definePromoteCall( uint_arg_type, constexprIntegerType, "&", {uint_arg_type}, nil)
	definePromoteCall( uint_arg_type, constexprIntegerType, "|", {uint_arg_type}, nil)
	definePromoteCall( uint_arg_type, constexprIntegerType, "^", {uint_arg_type}, nil)
	definePromoteCall( uint_arg_type, constexprIntegerType, "~", {}, nil)

	local float_arg_typenam = "double"
	local float_arg_type = typedb:get_type( 0, "const " .. float_arg_typenam)
	definePromoteCall( float_arg_type, constexprFloatType, "+", {float_arg_type}, nil)
	definePromoteCall( float_arg_type, constexprFloatType, "-", {float_arg_type}, nil)
	definePromoteCall( float_arg_type, constexprFloatType, "/", {float_arg_type}, nil)
	definePromoteCall( float_arg_type, constexprFloatType, "%", {float_arg_type}, nil)

	local bool_arg_typenam = "bool"
	local bool_arg_type = typedb:get_type( 0, "const " .. bool_arg_typenam)
	definePromoteCall( bool_arg_type, constexprBooleanType, "&&", {bool_arg_type}, nil)
	definePromoteCall( bool_arg_type, constexprBooleanType, "||", {bool_arg_type}, nil)
	definePromoteCall( bool_arg_type, constexprBooleanType, "!", {}, nil)	
end

function defineOperators( lval, c_lval, typeDescription)
	if typeDescription.unop then
		for operator,operator_fmt in pairs( typeDescription.unop) do
			defineCall( lval, c_lval, operator, {}, callConstructor( operator_fmt))
		end
	end
	if typeDescription.binop then
		for operator,operator_fmt in pairs( typeDescription.binop) do
			defineCall( lval, c_lval, operator, {c_lval}, callConstructor( operator_fmt))
		end
	end
	if typeDescription.cmpop then
		for operator,operator_fmt in pairs( typeDescription.cmpop) do
			defineCall( fccBooleanType, c_lval, operator, {c_lval}, callConstructor( operator_fmt))
		end
	end
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

	typedb:def_reduction( lval, rval, convConstructor( pointerTypeDescription.load), tag_typeDeduction)
	typedb:def_reduction( c_lval, c_rval, convConstructor( pointerTypeDescription.load), tag_typeDeduction)

	typedb:def_reduction( pval, rpval, convConstructor( pointerPointerTypeDescription.load), tag_typeDeduction)
	typedb:def_reduction( c_pval, c_rpval, convConstructor( pointerPointerTypeDescription.load), tag_typeDeduction)

	typedb:def_reduction( c_lval, lval, nil, tag_typeDeduction)
	typedb:def_reduction( c_rval, rval, nil, tag_typeDeduction)
	typedb:def_reduction( c_pval, pval, nil, tag_typeDeduction)
	typedb:def_reduction( c_rpval, rpval, nil, tag_typeDeduction)

	defineCall( rval, pval, "->", {}, nil)
	defineCall( c_rval, c_pval, "->", {}, nil)

	defineCall( rval, rval, "=", {c_lval}, callConstructor( typeDescription.assign))
	defineCall( rpval, rpval, "=", {c_pval}, callConstructor( pointerTypeDescription.assign))

	defineOperators( lval, c_lval, typeDescription)
	defineOperators( pval, c_pval, pointerTypeDescription)

	qualiTypeMap[ lval] = {lval=lval, c_lval=c_lval, rval=rval, c_rval=c_rval, pval=pval, c_pval=c_pval, rpval=rpval, c_rpval=c_rpval}
	return lval
end

function defineIndexOperators( typnam, qualitype, typeDescription)
	local pointerTypeDescription = llvmir.pointerType( typeDescription.llvmtype)
	for index_typenam, index_type in pairs(fccIndexTypeMap) do
		defineCall( qualitype.rval, qualitype.pval, "[]", {index_type}, convConstructor( pointerTypeDescription.index[ index_typnam]))
		defineCall( qualitype.c_rval, qualitype.c_pval, "[]", {index_type}, convConstructor( pointerTypeDescription.index[ index_typnam]))
	end
end

function getDefContextType()
	local rt = typedb:get_instance( "defcontext")
	if rt then return rt else return 0 end
end

function getContextTypes()
	local rt = typedb:get_instance( "context")
	if rt then return rt else return {0} end
end

function defineLocalVariable( node, type, name, initval, register)
	local descr = typeDescriptionMap[ type]
	local fmt = descr.def_local
	local out = register()
	local code = utils.constructor_format( fmt, {out = out}, register)
	local var = typedb:def_type( 0, name, out)
	typedb:def_reduction( referenceTypeMap[ type], var, nil, tag_typeDeclaration)
	if initval then applyOperator( node, "=", {initval}) end
	return {code=code}
end

function defineGlobalVariable( node, type, name, initval)
	local descr = typeDescriptionMap[ type]
	local fmt
	if initval then
		fmt = descr.def_global_val
		if type(initval) == "table" then utils.errorMessage( node.line, "only constexpr allowed to assign in global variable initialization") end
	else
		fmt = descr.def_global
	end
	local out = "@" .. mangleName( name)
	print( utils.constructor_format( fmt, {out = out, inp = initval}))
	local var = typedb:def_type( 0, name, out)
	typedb:def_reduction( referenceTypeMap[ type], var, nil, tag_typeDeclaration)
end

function defineVariable( node, typeId, varName, initVal)
	local register = typedb:get_instance( "register")
	local defcontext = typedb:get_instance( "defcontext")
	if register then
		return defineLocalVariable( node, typeId, varName, initval, register)
	elseif defcontext then
		utils.errorMessage( line, "Member variables not implemented yet")
	else
		return defineGlobalVariable( node, typeId, varName, initval)
	end
end

function defineParameter( node, type, varName, register)
	local descr = typeDescriptionMap[ type]
	local paramreg = register()
	local var = typedb:def_type( 0, name, paramreg)
	typedb:def_reduction( referenceTypeMap[ type], var, nil, tag_typeDeclaration)
	return desc.llvmtype .. " " .. paramreg
end

function defineType( node, typeName, typeDescription)
	local lval = defineQualifiedTypes( typnam, fcc_descr)
	defineIndexOperators( typnam, qualiTypeMap[ lval], typeDescription)
end

function initControlTypes()
	controlTrueType = typedb:def_type( 0, " controlTrueType")
	controlFalseType = typedb:def_type( 0, " controlFalseType")

	function falseExitToBoolean( constructor)
		local register,label = typedb:get_instance( "register"),typedb:get_instance( "label")
		out = register()
		return {code=utils.constructor_format( llvmir.control.falseExitToBoolean, {falseExit=constructor.out, out=out}, label),out=out}
	end
	function trueExitToBoolean( constructor)
		local register,label = typedb:get_instance( "register"),typedb:get_instance( "label")
		out = register()
		return {code=utils.constructor_format( llvmir.control.trueExitToBoolean, {trueExit=constructor.out, out=out}, label),out=out}
	end
	typedb:def_reduction( fccBooleanType, controlTrueType, falseExitToBoolean, tag_typeDeduction)
	typedb:def_reduction( fccBooleanType, controlFalseType, trueExitToBoolean, tag_typeDeduction)

	function BooleanToFalseExit( constructor)
		local label = typedb:get_instance( "label")
		out = label()
		return {code=utils.constructor_format( llvmir.control.booleanToFalseExit, {inp=constructor.out, out=out}, label),out=out}
	end
	function BooleanToTrueExit( constructor)
		local label = typedb:get_instance( "label")
		out = label()
		return {code=utils.constructor_format( llvmir.control.booleanToTrueExit, {inp=constructor.out, out=out}, label),out=out}
	end

	typedb:def_reduction( controlTrueType, fccBooleanType, BooleanToFalseExit, tag_typeDeduction)
	typedb:def_reduction( controlFalseType, fccBooleanType, BooleanToTrueExit, tag_typeDeduction)
end

function initFirstClassCitizens()
	for typnam, fcc_descr in pairs( llvmir.fcc) do
		local lval = defineQualifiedTypes( typnam, fcc_descr)
		fccQualiTypeMap[ typnam] = qualiTypeMap[ lval]
		if fcc_descr.class == "bool" then
			fccBooleanType = qualiTypeMap[ lval].c_lval
		elseif fcc_descr.class == "unsigned" or fcc_descr.class == "signed" then
			fccIndexTypeMap[ typnam] = qualiTypeMap[ lval].c_lval
		end
	end
	for typnam, fcc_descr in pairs( llvmir.fcc) do
		defineIndexOperators( typnam, fccQualiTypeMap[ typnam], fcc_descr)
	end
	defineConstExprOperators()
	if fccBooleanType then initControlTypes() end
end

function selectNoArgumentType( node, typeName, resultContextTypeId, reductions, items)
	if not resultContextTypeId or type(resultContextTypeId) == "table" then
		utils.errorResolveType( typedb, node.line, resultContextTypeId, getContextTypes(), typeName)
	end
	for ii,item in ipairs(items) do
		if typedb:type_nof_parameters( item.type) == 0 then return item.type end
	end
	utils.errorMessage( node.line, "failed to resolve %s",
	                    utils.resolveTypeString( typedb, getContextTypes(), typeName))
end

function getReductionConstructor( node, redu_type, operand)
	local redu_constructor = operand.constructor
	if redu_type ~= operand.type then
		local param_type,param_constructor_func = typedb:get_reduction( redu_type, operand.type, tagmask_resolveType)
		if not param_type then return nil end
		if param_constructor_func then
			redu_constructor = param_constructor_func( redu_constructor)
		end
	end
	return redu_constructor
end

function applyOperator( node, operator, arg)
	local resolveContextType,reductions,items = typedb:resolve_type( arg[1].type, operator, tagmask_resolveType)
	if not resultContextType or type(resultContextType) == "table" then utils.errorResolveType( typedb, node.line, resultContextType, arg[1].type, typeName) end
	for ii,item in ipairs(items) do
		if typedb:type_nof_parameters( item.type) == #arg-1 then
			local this_constructor = arg[1].constructor
			for ri,redu in ipairs(reductions) do
				if redu.constructor then
					this_constructor = redu.constructor( this_constructor)
				end
			end
			local param_constructor_ar = {}
			local parameters = typedb:type_parameters( item.type)
			for ii=2,#arg do
				local param_constructor = getReductionConstructor( node, parameters[ii-1].type, arg[ii])
				if not param_constructor then break end
				table.insert( param_constructor_ar, param_constructor)
			end
			if #param_constructor_ar+1 == #arg then
				local operator_constructor = item.constructor( this_constructor, param_constructor_ar)
				return {type=item.type, constructor=operator_constructor}
			end
		end
	end
	utils.errorMessage( node.line, "failed to resolve %s",
	                    utils.resolveTypeString( typedb, getContextTypes(), operator) .. "(" .. utils.typeListString( typedb, arg) .. ")")
end

function convertToBooleanType( node, operand)
	if type(operand.constructor) == "table" then
		local bool_constructor = getReductionConstructor( node, fccBooleanType, operand)
		if not bool_constructor then utils.errorMessage( node.line, "Argument is not reducible to boolean") end
		return {type=fccBooleanType, constructor=bool_constructor}
	else
		local bool_constructor = getReductionConstructor( node, constexprBooleanType, operand)
		if not bool_constructor then utils.errorMessage( node.line, "Argument is not reducible to boolean const expression") end
		return {type=constexprBooleanType, constructor=bool_constructor}
	end
end

function convertToControlBooleanType( node, controlBooleanType, operand)
	if operand.type == controlFalseType or operand.type == controlTrueType then
		if operand.type ~= controlBooleanType then
			local out = typedb:get_instance( "label")()
			return {type = controlBooleanType,
				constructor = operand.constructor .. utils.constructor_format( llvmir.invertedControlType, {inp=constructor.out, out=out}) }
		else
			return operand
		end
	else
		local boolOperand = convertToBooleanType( node, operand)
		if not typedb:get_instance( "label") then return boolOperand end
		local control_constructor = getReductionConstructor( node, controlBooleanType, {type=fccBooleanType,constructor=boolOperand.constructor})
		return {type=controlBooleanType, constructor=control_constructor}
	end
end
function negateControlBooleanType( node, operand)
	if operand.type == fccBooleanType or operand.type == constexprBooleanType then
		return applyOperator( node, "!", arg)
	else
		if operand.type == controlFalseType then
			return {type=controlTrueType, constructor=operand.constructor}
		else
			return {type=controlFalseType, constructor=operand.constructor}
		end
	end
end
function applyControlBooleanJoin( node, arg, controlBooleanType)
	local operand1 = convertToControlBooleanType( node, controlBooleanType, arg[1])
	local operand2 = convertToBooleanType( node, arg[2])
	if operand1.operand.type == constexprBooleanType then
		if operand1.operand.constructor then return operand2 else return {type=constexprBooleanType,constructor=false} end
	elseif operand2.operand.type == constexprBooleanType then 
		if operand2.operand.constructor then return operand1 else return {type=constexprBooleanType,constructor=false} end
	elseif operand1.operand.type == fccBooleanType and operand2.operand.type == fccBooleanType then
		return applyOperator( node, "&&", arg)
	elseif operand1.operand.type == controlFalseType and operand2.operand.type == fccBooleanType then
		local out = operand1.constructor.out
		local fmt; if controlBooleanType == controlTrueType then fmt = llvmir.control.booleanToFalseExit else fmt = llvmir.control.booleanToTrueExit end
		local code2 = utils.constructor_format( fmt, {inp=operand2.constructor.out, out=out}, typedb:get_instance( "label"))
		return {code=operand1.constructor.code .. code2, out=out}
	else
		utils.errorMessage( node.line, "Boolean expression cannot be evaluated")
	end
end

function getInstructionList( node, arg)
	return ""
end

function getSignatureString( name, args, is_global_def)
	if not is_global_def then
		return utils.uniqueName( name .. "__")
	else
		local pstr = ""
		for aa in args do for pp in aa:gmatch("%S+") do if pstr ~= "" then pstr = pstr .. "_" .. pp else pstr = "__" .. pp end break end end
		return name .. pstr
	end
end

function defineFunction( node, arg, is_global_def)
	local linkage = node.arg[1].linkage
	local attributes = node.arg[1].attributes
	local returnTypeName = typeDescriptionMap[ arg[2]].llvmtype
	local functionName = getSignatureString( arg[3], arg[4], is_global_def)
	local args = table.concat( arg[4], ", ")
	local body = getInstructionList( node.arg[5])
	print( utils.code_format_varg( "\ndefine {1} {2} @{3}( {4} ) {5} {\n{6}}", linkage, returnTypeName, functionName, args, attributes, body))
end

function defineProcedure( node, arg, is_global_def)
	local linkage = node.arg[1].linkage
	local attributes = node.arg[1].attributes
	local functionName = getSignatureString( arg[2], arg[3], is_global_def)
	local argstr = table.concat( arg[3], ", ")
	local body = getInstructionList( node.arg[4])
	print( utils.code_format_varg( "\ndefine {1} void @{2}( {3} ) {4} {\n{5}}", linkage, functionName, argstr, attributes, body))
end

-- AST Callbacks:
function typesystem.paramdef( node) 
	local subnode = utils.traverse( typedb, node)
	return defineParameter( node, subnode[1], subnode[2], typedb:get_instance( "register"))
end

function typesystem.paramdeflist( node)
	return utils.traverse( typedb, node)
end

function typesystem.vardef( node)
	local subnode = utils.traverse( typedb, node)
	return defineVariable( node, subnode[1], subnode[2], nil)
end

function typesystem.vardef_assign( node)
	local subnode = utils.traverse( typedb, node)
	return defineVariable( node, subnode[1], subnode[2], subnode[3])
end

function typesystem.vardef_array( node) return utils.visit( typedb, node) end
function typesystem.vardef_array_assign( node) return utils.visit( typedb, node) end

function typesystem.assign_operator( node, operator)
	local arg = utils.traverse( typedb, node)
	return applyOperator( node, "=", {arg[1], applyOperator( node, operator, arg)})
end
function typesystem.nary_operator( node, operator)
	local arg = utils.traverse( typedb, node)
	return applyOperator( node, operator, arg)
end
function typesystem.binary_operator( node, operator)
	local arg = utils.traverse( typedb, node)
	return applyOperator( node, operator, arg)
end
function typesystem.unary_operator( node, operator)
	local arg = utils.traverse( typedb, node)
	return applyOperator( node, operator, arg)
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

function typesystem.statement( node)
	local code = nil
	local arg = utils.traverse( typedb, node)
	for ai=1,#arg do
		if type(arg[ai]) == "table" then
			if code then code = code .. arg[ ai].code else code = arg[ ai].code end
		end
	end
	return {code=code}
end

function typesystem.return_value( node) return utils.visit( typedb, node) end
function typesystem.conditional_if( node)
	local arg = utils.traverse( typedb, node)
	convertToControlBooleanType( node, controlBooleanType, operand)
end
function typesystem.conditional_while( node) return utils.visit( typedb, node) end
function typesystem.typedef( node) return utils.visit( typedb, node) end

function typesystem.typespec( node, qualifier)
	typeName = qualifier .. node.arg[ #node.arg].value;
	local typeId
	if #node.arg == 1 then
		typeId = selectNoArgumentType( node, typeName, typedb:resolve_type( getContextTypes(), typeName, tagmask_typeNameSpace))
	else
		typeId = selectNoArgumentType( node, typeName, typedb:resolve_type( getContextTypes(), node.arg[ 1].value, tagmask_typeNameSpace))
		if #node.arg > 2 then
			for ii = 2, #node.arg-1 do
				typeId = selectNoArgumentType( node, typeName, typedb:resolve_type( contextTypeId, node.arg[ ii].value, tagmask_typeNameSpace))
				contextTypeId = typeId
			end
		end
		typeId = selectNoArgumentType( node, typeName, typedb:resolve_type( contextTypeId, typeName, tagmask_typeNameSpace))
	end
	return typeId
end

function typesystem.constant( node, typeName)
	local typeId = selectNoArgumentType( node, typeName, typedb:resolve_type( 0, typeName))
	return {type=typeId, constructor=node.arg[1].value}
end

function typesystem.linkage( node, llvm_linkage)
	return llvm_linkage
end

function typesystem.funcdef( node)
	local is_global_def = typedb:get_instance( "register") == nil
	typedb:set_instance( "register", utils.register_allocator())
	typedb:set_instance( "label", utils.label_allocator())
	defineFunction( node, utils.traverse( typedb, node), is_global_def)
	return rt
end

function typesystem.procdef( node) 
	local is_global_def = typedb:get_instance( "register") == nil
	typedb:set_instance( "register", utils.register_allocator())
	typedb:set_instance( "label", utils.label_allocator())
	defineProcedure( node, utils.traverse( typedb, node, is_global_def))
	return rt
end

function typesystem.program( node)
	initFirstClassCitizens()
	local rt = utils.visit( typedb, node)
	return rt;
end

function typesystem.count( node)
	if #node.arg == 0 then
		return 1
	else
		return utils.traverseCall( node)[ 1] + 1
	end
end

function typesystem.rep_unary_operator( node)
	local arg = utils.traverse( typedb, node)
	local icount = arg[2]
	local expr = arg[1]
	for ii=1,icount do
		expr = applyOperator( node, "->", {expr})
	end
	return applyOperator( node, node.arg[2].value, {expr})
end

return typesystem

