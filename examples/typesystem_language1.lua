local mewa = require "mewa"
local fcc = require "fcc_language1"
local utils = require "typesystem_utils"
local bcd = require "bcd"

local typedb = mewa.typedb()
local typesystem = {
	assign = {},
	assign_add = {},
	assign_sub = {},
	assign_mul = {},
	assign_div = {},
	assign_mod = {},
	logical_or = {},
	logical_and = {},
	bitwise_or = {},
	bitwise_and = {},
	bitwise_xor = {},
	add = {},
	sub = {},
	minus = {},
	plus = {},
	mul = {},
	div = {},
	mod = {},

	arrow = {},
	member = {},
	ptrderef = {},
	call = {},
	arrayaccess = {},

	cmpeq = {},
	cmpne = {},
	cmple = {},
	cmplt = {},
	cmpge = {},
	cmpgt = {}
}

local tag_typeDeduction = 1
local tag_typeDeclaration = 2
local tag_typeNamespace = 3
local tagmask_resolveType = typedb.reduction_tagmask( tag_typeDeduction, tag_typeDeclaration)
local tagmask_typeNameSpace = typedb.reduction_tagmask( tag_typeNamespace)

function convConstructor( fmt)
	return function( constructor)
		local code,result = utils.positional_format( fmt, {[1] = constructor.out}, typedb:get_instance( "register"), 2)
		return {code = constructor.code .. code, out = result}
	end
end
function loadConstructor( fmt)
	return function( value)
		local code,result = utils.positional_format( fmt, {[1] = value}, typedb:get_instance( "register"), 2)
		return {code = code, out = result}
	end
end
function storeConstructor( fmt, adr)
	return function( constructor)
		local code,result = utils.positional_format( fmt, {[1] = adr, [2] = constructor.out}, typedb:get_instance( "register"), 2)
		return {code = constructor.code .. code_load, out = result}
	end
end
function unaryOpConstructor( fmt)
	return function( arg)
		local code,result = utils.positional_format( fmt, {[1] = arg[1].out}, typedb:get_instance( "register"), 2)
		return {code = arg[1].code .. code, out = result}
	end
end
function binaryOpConstructor( fmt)
	return function( arg)
		local code,result = utils.positional_format( fmt, {[1] = arg[1].out, [2] = arg[2].out}, typedb:get_instance( "register"), 3)
		return {code = arg[1].code .. arg[2].code .. code, out = result}
	end
end

local variableConstructors = {}
local codeMap = {}
local codeKey = nil
local codeKeyCnt = 0

function allocCodeKey()
	codeKeyCnt = codeKeyCnt + 1
	return codeKeyCnt
end

function openCode( key)
	local rt = codeKey
	codeKey = key or allocCodeKey()
	return rt
end

function closeCode( oldKey)
	local rt = codeMap[ codeKey]
	codeMap[ codeKey] = nil
	codeKey = oldKey
	return rt
end

function printCodeLine( codeln)
	if codeln then
		if codeMap[ codeKey] then
			codeMap[ codeKey] = codeMap[ codeKey] .. codeln .. "\n"
		else
			codeMap[ codeKey] = codeln .. "\n"
		end
	end
end

local constexpr_integer_type = typedb:def_type( 0, "constexpr int")
local constexpr_float_type = typedb:def_type( 0, "constexpr float")
local constexpr_bool_type = typedb:def_type( 0, "constexpr bool")
local bits64 = bcd.bits( 64)

function defineConstExprOperators()
	typedb:def_reduction( constexpr_float_type,
		typedb:def_type( constexpr_float_type, "+", function( arg) return arg[1] + arg[2] end, {constexpr_float_type} ), nil, tag_typeDeduction)
	typedb:def_reduction( constexpr_float_type,
		typedb:def_type( constexpr_float_type, "-", function( arg) return arg[1] - arg[2] end, {constexpr_float_type} ), nil, tag_typeDeduction)
	typedb:def_reduction( constexpr_float_type,
		typedb:def_type( constexpr_float_type, "/", function( arg) return arg[1] / arg[2] end, {constexpr_float_type} ), nil, tag_typeDeduction)
	typedb:def_reduction( constexpr_float_type,
		typedb:def_type( constexpr_float_type, "%", function( arg) return arg[1] % arg[2] end, {constexpr_float_type} ), nil, tag_typeDeduction)

	typedb:def_reduction( constexpr_integer_type,
		typedb:def_type( constexpr_integer_type, "+", function( arg) return arg[1] + arg[2] end, {constexpr_integer_type} ), nil, tag_typeDeduction)
	typedb:def_reduction( constexpr_integer_type,
		typedb:def_type( constexpr_integer_type, "-", function( arg) return arg[1] - arg[2] end, {constexpr_integer_type} ), nil, tag_typeDeduction)
	typedb:def_reduction( constexpr_integer_type,
		typedb:def_type( constexpr_integer_type, "/", function( arg) return arg[1] / arg[2] end, {constexpr_integer_type} ), nil, tag_typeDeduction)
	typedb:def_reduction( constexpr_integer_type,
		typedb:def_type( constexpr_integer_type, "%", function( arg) return arg[1] % arg[2] end, {constexpr_integer_type} ), nil, tag_typeDeduction)

	typedb:def_reduction( constexpr_integer_type,
		typedb:def_type( constexpr_integer_type, "+", function( arg) return arg[1]:tonumber() + arg[2] end, {constexpr_float_type} ), nil, tag_typeDeduction)
	typedb:def_reduction( constexpr_integer_type,
		typedb:def_type( constexpr_integer_type, "-", function( arg) return arg[1]:tonumber() - arg[2] end, {constexpr_float_type} ), nil, tag_typeDeduction)
	typedb:def_reduction( constexpr_integer_type,
		typedb:def_type( constexpr_integer_type, "/", function( arg) return arg[1]:tonumber() / arg[2] end, {constexpr_float_type} ), nil, tag_typeDeduction)
	typedb:def_reduction( constexpr_integer_type,
		typedb:def_type( constexpr_integer_type, "%", function( arg) return arg[1]:tonumber() % arg[2] end, {constexpr_float_type} ), nil, tag_typeDeduction)

	typedb:def_reduction( constexpr_integer_type,
		typedb:def_type( constexpr_integer_type, "&", function( arg) return arg[1].bit_and( arg[2], bits64) end, {constexpr_integer_type} ), nil, tag_typeDeduction)
	typedb:def_reduction( constexpr_integer_type,
		typedb:def_type( constexpr_integer_type, "|", function( arg) return arg[1].bit_or( arg[2], bits64) end, {constexpr_integer_type} ), nil, tag_typeDeduction)
	typedb:def_reduction( constexpr_integer_type,
		typedb:def_type( constexpr_integer_type, "^", function( arg) return arg[1].bit_xor( arg[2], bits64) end, {constexpr_integer_type} ), nil, tag_typeDeduction)
	typedb:def_reduction( constexpr_integer_type,
		typedb:def_type( constexpr_integer_type, "~", function( arg) return arg[1].bit_not( bits64) end, {} ), nil, tag_typeDeduction)

	typedb:def_reduction( constexpr_bool_type,
		typedb:def_type( constexpr_bool_type, "&&", function( arg) return arg[1] and arg[2] end, {constexpr_bool_type} ), nil, tag_typeDeduction)
	typedb:def_reduction( constexpr_bool_type,
		typedb:def_type( constexpr_bool_type, "||", function( arg) return arg[1] or arg[2] end, {constexpr_bool_type} ), nil, tag_typeDeduction)
	typedb:def_reduction( constexpr_bool_type,
		typedb:def_type( constexpr_bool_type, "!", function( arg) return not arg[1] end, {} ), nil, tag_typeDeduction)

	typedb:def_reduction( constexpr_bool_type,
		constexpr_integer_type, function( value) return value ~= "0" end, tag_typeDeduction)
	typedb:def_reduction( constexpr_bool_type,
		constexpr_float_type, function( value) return math.abs(value) < math.abs(epsilon) end, tag_typeDeduction)

	typedb:def_reduction( constexpr_float_type,
		constexpr_integer_type, function( value) return value:tonumber() end, tag_typeDeduction)
	typedb:def_reduction( constexpr_integer_type,
		constexpr_float_type, function( value) return bcd:int( value) end, tag_typeDeduction)

	typedb:def_reduction( constexpr_integer_type,
		constexpr_bool_type, function( value) if value then return bcd:int("1") else return bcd:int("0") end end, tag_typeDeduction)
end

function defineConstExprValueOperator( constExprType, valueType, fcc_descr, opr)
	typedb:def_reduction( valueType,
		typedb:def_type( constExprType, opr,
			function( arg) return typedb:type_constructor( typedb:get_type( valueType, opr, {intType}))( { loadConstructor(fcc_descr.load)(arg[1]), arg[2] } ) end,
			{ valueType } ), 
		nil, tag_typeDeduction)
end
function defineConstExprValueOperatorsInt( intType, fcc_descr)
	defineConstExprValueOperator( constexpr_integer_type, intType, fcc_descr, "+")
	defineConstExprValueOperator( constexpr_integer_type, intType, fcc_descr, "-")
	defineConstExprValueOperator( constexpr_integer_type, intType, fcc_descr, "/")
	defineConstExprValueOperator( constexpr_integer_type, intType, fcc_descr, "%")
	defineConstExprValueOperator( constexpr_integer_type, intType, fcc_descr, "&")
	defineConstExprValueOperator( constexpr_integer_type, intType, fcc_descr, "|")
	defineConstExprValueOperator( constexpr_integer_type, intType, fcc_descr, "^")
end
function defineConstExprValueOperatorsFloat( floatType, fcc_descr)
	defineConstExprValueOperator( constexpr_float_type, floatType, fcc_descr, "+")
	defineConstExprValueOperator( constexpr_float_type, floatType, fcc_descr, "-")
	defineConstExprValueOperator( constexpr_float_type, floatType, fcc_descr, "/")
	defineConstExprValueOperator( constexpr_float_type, floatType, fcc_descr, "%")
end
function defineConstExprValueOperatorsBool( boolType, fcc_descr)
	defineConstExprValueOperator( constexpr_bool_type, boolType, fcc_descr, "&&")
	defineConstExprValueOperator( constexpr_bool_type, boolType, fcc_descr, "||")
end
function defineAssignmentOperator( rvalue, const_lvalue, operator, fmt)
	typedb:def_reduction( rvalue,
		typedb:def_type( rvalue, operator,
				function( arg) local rt = storeConstructor( fmt, arg[1])( arg[2]); rt.adr = arg[1]; return rt end,
				{const_lvalue} ),
			function( constructor) return constructor.adr end, tag_typeDeduction)
end

function initFirstClassCitizens()
	defineConstExprOperators()

	function getVariableConstructors( type, reftype, descr)
		return {
			def_local = function( name, initval, register)
				local fmt; if initval then fmt = descr.def_local_val else fmt = descr.def_local end
				local code,result = utils.positional_format( fmt, {[2] = initval}, register, 1)
				local var = typedb:def_type( 0, name, result)
				typedb:def_reduction( type, var, function( adr) return {code = "", out = adr} end, tag_typeDeclaration)
				typedb:def_reduction( reftype, var, nil, tag_typeDeduction)
				return code
			end,
			def_global = function( name, initval)
				local fmt; if initval then fmt = descr.def_global_val else fmt = descr.def_global end
				local adr = "@" .. utils.mangleVariableName(name)
				local code,result = utils.positional_format( fmt, {[1] = adr, [2] = initval}, nil, 1)
				local var = typedb:def_type( 0, name, result)
				typedb:def_reduction( type, var, loadConstructor( descr.load), tag_typeDeclaration)
				typedb:def_reduction( reftype, var, nil, tag_typeDeduction)
				return code
			end
		}
	end
	for typnam, fcc_descr in pairs( fcc) do
		local lvalue = typedb:def_type( 0, typnam)
		local const_lvalue = typedb:def_type( 0, "const " .. typnam)
		local rvalue = typedb:def_type( 0, "&" .. typnam)
		local const_rvalue = typedb:def_type( 0, "const&" .. typnam)

		local constexprType
		if fcc_descr.class == "fp" then
			defineConstExprValueOperatorsFloat( lvalue, fcc_descr)
			constexprType = constexpr_float_type
		elseif fcc_descr.class == "bool" then
			defineConstExprValueOperatorsBool( lvalue, fcc_descr)
			constexprType = constexpr_bool_type
		else
			defineConstExprValueOperatorsInt( lvalue, fcc_descr)
			constexprType = constexpr_integer_type
		end
		typedb:def_reduction( const_lvalue, constexprType, loadConstructor( fcc_descr.load), tag_typeDeduction)
		typedb:def_reduction( const_lvalue, lvalue, nil, tag_typeDeduction)

		variableConstructors[ lvalue] = getVariableConstructors( lvalue, rvalue, fcc_descr)
		variableConstructors[ const_lvalue] = getVariableConstructors( const_lvalue, const_rvalue, fcc_descr)

		typedb:def_reduction( lvalue, const_lvalue, loadConstructor( fcc_descr.load), tag_typeDeduction)
		defineAssignmentOperator( rvalue, const_lvalue, "=", fcc_descr.store)

		for operator,operator_fmt in pairs( fcc_descr.unop) do
			typedb:def_reduction(
				const_lvalue, typedb:def_type( const_lvalue, operator, unaryOpConstructor( operator_fmt)),
				nil, tag_typeDeduction)
		end
		for operator,operator_fmt in pairs( fcc_descr.binop) do
			typedb:def_reduction(
				const_lvalue, typedb:def_type( const_lvalue, operator, binaryOpConstructor( operator_fmt), {const_lvalue}),
				nil, tag_typeDeduction)
		end
	end
	for typnam, fcc_descr in pairs( fcc) do
		local const_lvalue = typedb:get_type( 0, "const " .. typnam)
		for from_typenam,conv_fmt in pairs( fcc_descr.conv) do
			from_type = typedb:get_type( 0, "const " .. from_typenam)
			if from_type == 0 then
				error( "Type undefined: 'const " .. from_typenam .. "'")
			end
			typedb:def_reduction( const_lvalue, from_type, convConstructor( conv_fmt), tag_typeDeduction)
		end
	end
end

local globals = {}

function mapConstructorTemplate( template, struct)
	local rt
	for elem in ipairs(template) do
		rt = rt .. (struct[ elem] or elem)
	end
	return rt;
end

function getDefContextType()
	local rt = typedb:get_instance( "defcontext")
	if rt then return rt else return 0 end
end

function getContextTypes()
	local rt = typedb:get_instance( "context")
	if rt then return rt else return {0} end
end

function defineVariable( line, typeId, varName, initVal)
	vc = variableConstructors[ typeId]
	if not vc then utils.errorMessage( line, "Can't define variable of type '%s'", typedb:type_string(typeId)) end

	local register = typedb:get_instance( "register")
	local defcontext = typedb:get_instance( "defcontext")
	if register then
		if not vc.def_local then utils.errorMessage( line, "Can't define variable of type '%s'", typedb:type_string(typeId)) end
		printCodeLine( vc.def_local( varName, initVal, register))
	elseif defcontext then
		utils.errorMessage( line, "Substructures not implemented yet")
	else
		if not vc.def_global then utils.errorMessage( line, "Can't define variable of type '%s'", typedb:type_string(typeId)) end
		printCodeLine( vc.def_global( varName, initVal))
	end
end

function selectVariableType( node, typeName, resultContextTypeId, reductions, items)
	if not resultContextTypeId or type(resultContextTypeId) == "table" then errorResolveType( typedb, node.line, resultContextTypeId, getContextTypes(), typeName) end
	for ii,item in ipairs(items) do
		if typedb:type_nof_parameters( item.type) == 0 then return item.type end
	end
	errorResolveType( typedb, node.line, resultContextTypeId, getContextTypes(), typeName)
end

function typesystem.vardef( node)
	local subnode = utils.traverse( typedb, node)
	defineVariable( node.line, subnode[1], subnode[2], nil)
end

function typesystem.vardef_assign( node)
	local subnode = utils.traverse( typedb, node)
	defineVariable( node.line, subnode[1], subnode[2], subnode[3])
end

function typesystem.vardef_array( node) return utils.visit( typedb, node) end
function typesystem.vardef_array_assign( node) return utils.visit( typedb, node) end
function typesystem.operator( node, opdescr) return utils.visit( typedb, node) end
function typesystem.stm_expression( node) return utils.visit( typedb, node) end
function typesystem.stm_return( node) return utils.visit( typedb, node) end
function typesystem.conditional_if( node) return utils.visit( typedb, node) end
function typesystem.conditional_while( node) return utils.visit( typedb, node) end
function typesystem.typedef( node) return utils.visit( typedb, node) end

function typesystem.typespec( node, qualifier)
	typeName = qualifier .. node.arg[ #node.arg].value;
	local typeId
	if #node.arg == 1 then
		typeId = selectVariableType( node, typeName, typedb:resolve_type( getContextTypes(), typeName, tagmask_typeNameSpace))
	else
		typeId = selectVariableType( node, typeName, typedb:resolve_type( getContextTypes(), node.arg[ 1].value, tagmask_typeNameSpace))
		if #node.arg > 2 then
			for ii = 2, #node.arg-1, 1 do
				typeId = selectVariableType( node, typeName, typedb:resolve_type( contextTypeId, node.arg[ ii].value, tagmask_typeNameSpace))
				contextTypeId = typeId
			end
		end
		typeId = selectVariableType( node, typeName, typedb:resolve_type( contextTypeId, typeName, tagmask_typeNameSpace))
	end
	return typeId
end
function typesystem.funcdef( node) local cd = openCode(); local rt = utils.visit( typedb, node); printCodeLine( closeCode( cd)); return rt; end
function typesystem.procdef( node) local cd = openCode(); local rt = utils.visit( typedb, node); printCodeLine( closeCode( cd)); return rt; end
function typesystem.paramdef( node) return utils.visit( typedb, node) end

function typesystem.program( node)
	initFirstClassCitizens()
	openCode( "program")
	local rt = utils.visit( typedb, node)
	print( closeCode( cd))
	return rt;
end

return typesystem

