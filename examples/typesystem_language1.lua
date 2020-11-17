local mewa = require "mewa"
local llvmir_fmt = require "llvmir_fmt_language1"
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

function convConstructor( fmt)
	return function( constructor)
		local register = typedb:get_instance( "register")
		local out = register()
		local code = utils.constructor_format( fmt, {inp = constructor.out, out = out}, register)
		return {code = constructor.code .. code, out = out}
	end
end
function loadConstructor( fmt)
	return function( value)
		local register = typedb:get_instance( "register")
		local out = register()
		local code = utils.constructor_format( fmt, {adr = value, out = out}, register)
		return {code = code, out = out}
	end
end
function storeConstructor( fmt, adr)
	return function( constructor)
		local register = typedb:get_instance( "register")
		local code = utils.constructor_format( fmt, {inp = constructor.out, adr = adr}, register)
		return {code = constructor.code .. code_load, out = constructor.out}
	end
end
function unaryOpConstructor( fmt)
	return function( arg)
		local register = typedb:get_instance( "register")
		local out = register()
		local code = utils.constructor_format( fmt, {inp = arg[1].out, out = out}, register)
		return {code = arg[1].code .. code, out = out}
	end
end
function binaryOpConstructor( fmt)
	return function( arg)
		local register = typedb:get_instance( "register")
		local out = register()
		local code = utils.constructor_format( fmt, {arg1 = arg[1].out, arg2 = arg[2].out, out = out}, register)
		return {code = arg[1].code .. arg[2].code .. code, out = out}
	end
end

local typeAttributes = {}
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
local constexpr_dqstring_type = typedb:def_type( 0, "constexpr dqstring")
local constexpr_sqstring_type = typedb:def_type( 0, "constexpr sqstring")
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
			function( arg)
	                       return typedb:type_constructor( typedb:get_type( valueType, opr, {valueType}))( { loadConstructor(fcc_descr.load)(arg[1]), arg[2] } )
			end,
			{ valueType } ), 
		nil, tag_typeDeduction)
end
function defineLValueAdvanceBinaryOperator( valueType, operandType, opr, fmt_conv)
	typedb:def_reduction( operandType,
		typedb:def_type( valueType, opr,
			function( arg)
	                       return typedb:type_constructor( typedb:get_type( operandType, opr, {operandType}))( { convConstructor(fmt_conv)(arg[1]), arg[2] } )
			end,
			{operandType} ),
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
				local register = typedb:get_instance( "register")
				local out = register()
				local code = utils.constructor_format( fmt, {val = initval, out = out}, register)
				local var = typedb:def_type( 0, name, result)
				typedb:def_reduction( type, var, function( adr) return {code = "", out = adr} end, tag_typeDeclaration)
				typedb:def_reduction( reftype, var, nil, tag_typeDeduction)
				return code
			end,
			def_global = function( name, initval)
				local fmt; if initval then fmt = descr.def_global_val else fmt = descr.def_global end
				local adr = "@" .. utils.mangleVariableName(name)
				local code = utils.constructor_format( fmt, {val = initval, out = adr}, nil)
				local var = typedb:def_type( 0, name, result)
				typedb:def_reduction( type, var, loadConstructor( descr.load), tag_typeDeclaration)
				typedb:def_reduction( reftype, var, nil, tag_typeDeduction)
				return code
			end
		}
	end
	for typnam, fcc_descr in pairs( llvmir_fmt.fcc) do
		local avalue = typedb:def_type( 0, "$" .. typnam)
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
		typedb:def_reduction( lvalue, avalue, nil, tag_typeDeduction)

		typedb:def_reduction( const_lvalue, constexprType, loadConstructor( fcc_descr.load), tag_typeDeduction)
		typedb:def_reduction( const_lvalue, lvalue, nil, tag_typeDeduction)

		typeAttributes[ lvalue] = getVariableConstructors( lvalue, rvalue, fcc_descr)
		typeAttributes[ lvalue].llvmtype = fcc_descr.llvmType
		typeAttributes[ const_lvalue] = getVariableConstructors( const_lvalue, const_rvalue, fcc_descr)
		typeAttributes[ const_lvalue].llvmtype = fcc_descr.llvmType

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
	for typnam, fcc_descr in pairs( llvmir_fmt.fcc) do
		local const_lvalue = typedb:get_type( 0, "const " .. typnam)
		for from_typenam,conv in pairs( fcc_descr.conv) do
			from_type = typedb:get_type( 0, "const " .. from_typenam)
			typedb:def_reduction( const_lvalue, from_type, convConstructor( conv.fmt), tag_typeDeduction, conv.weight)
		end
		for operator,operator_fmt in pairs( fcc_descr.binop) do
			for ii,advance_typenam in ipairs( fcc_descr.advance) do
				local advance_const_lvalue = typedb:get_type( 0, "const " .. advance_typenam)
				defineLValueAdvanceBinaryOperator( const_lvalue, advance_const_lvalue, operator, fcc_descr.conv[ advance_typenam].fmt)
			end
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

function applyOperator( node, operator, arg)
	local resolveContextType,reductions,items = typedb:resolve_type( arg[1].type, operator, tagmask_resolveType)
	if not resultContextType or type(resultContextType) == "table" then utils.errorResolveType( typedb, node.line, resultContextType, arg[1].type, typeName) end
	for ii,item in ipairs(items) do
		if typedb:type_nof_parameters( item.type) == #arg-1 then
			local constructor = arg[1].constructor
			for ri,redu in ipairs(reductions) do
				if redu.constructor then
					constructor = redu.constructor( constructor)
				end
			end
			local constructor_ar = {constructor}
			local parameters = typedb:type_parameters( item.type)
			for ii=2,#arg do
				local param_type,param_constructor = typedb:get_reduction( parameters[ii-1].type, arg[ii].type, tagmask_resolveType)
				if param_type then
					table.insert( constructor_ar, arg[ii] )
				else
					break
				end
			end
			if #constructor_ar == #arg then
				local operator_constructor = item.constructor( constructor_ar)
				return {type=item.type, constructor=operator_constructor}
			end
		end
	end
	utils.errorMessage( node.line, "failed to resolve %s",
	                    utils.resolveTypeString( typedb, getContextTypes(), operator) .. "(" .. utils.typeListString( typedb, arg) .. ")")
end

function getArgumentListString( node, arg)
	return ""
end
function getInstructionList( node, arg)
	return ""
end
function getLlvmTypeName( node, typeId)
	return ""
end

function defineFunction( node, arg)
	local linkage = node.arg[1].linkage
	local returnTypeName = llvmTypes[ arg[2]]
	if not returnTypeName then
	end
	local functionName = arg[3]
	local args = getArgumentListString( node.arg[4], arg[4])
	local body = getInstructionList( node.arg[5])
	printCodeLine( utils.code_format_varg( "define {1} {2} @{3}( {4} ) {\n{5}}", linkage, returnTypeName, functionName, args, body))
end

function defineProcedure( node, arg)
	local linkage = node.arg[1].linkage
	local functionName = arg[2]
	local args = getArgumentListString( node.arg[3])
	local body = getInstructionList( node.arg[4])
	printCodeLine( utils.code_format_varg( "define {1} void @{2}( {3} ) {\n{4}}", linkage, functionName, args, body))
end


-- AST Callbacks:
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

function typesystem.assign_operator( node, operator, context)
	local arg = utils.traverse( typedb, node, context)
	return applyOperator( node, "=", {arg[1], applyOperator( node, operator, arg)})
end
function typesystem.nary_operator( node, operator)
	local arg = utils.traverse( typedb, node, context)
	return applyOperator( node, operator, arg)
end
function typesystem.binary_operator( node, operator)
	local arg = utils.traverse( typedb, node, context)
	return applyOperator( node, operator, arg)
end
function typesystem.unary_operator( node, operator)
	local arg = utils.traverse( typedb, node, context)
	return applyOperator( node, operator, arg)
end

function typesystem.stm_expression( node) return utils.visit( typedb, node) end
function typesystem.stm_return( node) return utils.visit( typedb, node) end
function typesystem.conditional_if( node) return utils.visit( typedb, node) end
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
			for ii = 2, #node.arg-1, 1 do
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
	typedb:set_instance( "register", utils.register_allocator())
	local cd = openCode(); 
	defineFunction( node, utils.traverse( typedb, node))
	printCodeLine( closeCode( cd)); 
	return rt
end

function typesystem.procdef( node) 
	typedb:set_instance( "register", utils.register_allocator())
	local cd = openCode(); 
	defineProcedure( node, utils.traverse( typedb, node))
	printCodeLine( closeCode( cd)); 
	return rt
end

function typesystem.paramdef( node) return utils.visit( typedb, node) end
function typesystem.paramdeflist( node) return utils.visit( typedb, node) end

function typesystem.program( node)
	initFirstClassCitizens()
	openCode( "program")
	local rt = utils.visit( typedb, node)
	print( closeCode( cd))
	return rt;
end

return typesystem

