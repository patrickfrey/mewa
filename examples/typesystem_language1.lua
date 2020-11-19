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
		for ii=1,#arg,1 do
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
local qualiTypeMap = {}		-- maps any defined type id without qualifier to the table of type ids for all qualifiers possible
local referenceTypeMap = {}	-- maps any defined type id to its reference type
local typeDescriptionMap = {}	-- maps any defined type id without qualifier to its llvmir template structure
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

function printSectionCodeLine( section, codeln)
	if codeln then
		if codeMap[ codeKey] then
			codeMap[ codeKey] = codeMap[ codeKey] .. codeln .. "\n"
		else
			codeMap[ codeKey] = codeln .. "\n"
		end
	end
end

function printCodeLine( codeln)
	printSectionCodeLine( codeKey, codeln)
end

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

local constexpr_integer_type = typedb:def_type( 0, "constexpr int")
local constexpr_float_type = typedb:def_type( 0, "constexpr float")
local constexpr_bool_type = typedb:def_type( 0, "constexpr bool")
local constexpr_dqstring_type = typedb:def_type( 0, "constexpr dqstring")
local constexpr_sqstring_type = typedb:def_type( 0, "constexpr sqstring")
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
	defineCall( constexpr_type, constexpr_type, "~", {}, function( this, arg) return this ~= true end)
end

function defineConstExprOperators()
	defineConstExprBasicArithmetics( constexpr_float_type)
	defineConstExprBasicArithmetics( constexpr_integer_type)
	defineConstExprBasicArithmeticsPromoted( constexpr_integer_type, constexpr_float_type, function(this) return this:tonumber() end)
	defineConstExprBitArithmetics( constexpr_integer_type)
	defineConstExprBooleanArithmetics( constexpr_bool_type)

	typedb:def_reduction( constexpr_bool_type, constexpr_integer_type, function( value) return value ~= "0" end, tag_typeDeduction)
	typedb:def_reduction( constexpr_bool_type, constexpr_float_type, function( value) return math.abs(value) < math.abs(epsilon) end, tag_typeDeduction)
	typedb:def_reduction( constexpr_float_type, constexpr_integer_type, function( value) return value:tonumber() end, tag_typeDeduction)
	typedb:def_reduction( constexpr_integer_type, constexpr_float_type, function( value) return bcd:int( value) end, tag_typeDeduction)

	function bool2bcd( value) if value then return bcd:int("1") else return bcd:int("0") end end
	typedb:def_reduction( constexpr_integer_type, constexpr_bool_type, bool2bcd, tag_typeDeduction)

	local int_arg_typenam = "long"
	local int_arg_type = typedb:get_type( 0, "const " .. int_arg_typenam)
	definePromoteCall( int_arg_type, constexpr_integer_type, "+", {int_arg_type}, nil)
	definePromoteCall( int_arg_type, constexpr_integer_type, "-", {int_arg_type}, nil)
	definePromoteCall( int_arg_type, constexpr_integer_type, "/", {int_arg_type}, nil)
	definePromoteCall( int_arg_type, constexpr_integer_type, "%", {int_arg_type}, nil)
	local uint_arg_typenam = "ulong"
	local uint_arg_type = typedb:get_type( 0, "const " .. uint_arg_typenam)
	definePromoteCall( uint_arg_type, constexpr_integer_type, "&", {uint_arg_type}, nil)
	definePromoteCall( uint_arg_type, constexpr_integer_type, "|", {uint_arg_type}, nil)
	definePromoteCall( uint_arg_type, constexpr_integer_type, "^", {uint_arg_type}, nil)
	definePromoteCall( uint_arg_type, constexpr_integer_type, "~", {}, nil)

	local float_arg_typenam = "double"
	local float_arg_type = typedb:get_type( 0, "const " .. float_arg_typenam)
	definePromoteCall( float_arg_type, constexpr_float_type, "+", {float_arg_type}, nil)
	definePromoteCall( float_arg_type, constexpr_float_type, "-", {float_arg_type}, nil)
	definePromoteCall( float_arg_type, constexpr_float_type, "/", {float_arg_type}, nil)
	definePromoteCall( float_arg_type, constexpr_float_type, "%", {float_arg_type}, nil)

	local bool_arg_typenam = "bool"
	local bool_arg_type = typedb:get_type( 0, "const " .. bool_arg_typenam)
	definePromoteCall( bool_arg_type, constexpr_bool_type, "&&", {bool_arg_type}, nil)
	definePromoteCall( bool_arg_type, constexpr_bool_type, "||", {bool_arg_type}, nil)
	definePromoteCall( bool_arg_type, constexpr_bool_type, "~", {}, nil)	
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
	printCodeLine( utils.constructor_format( fmt, {out = out}, register))
	local var = typedb:def_type( 0, name, out)
	typedb:def_reduction( referenceTypeMap[ type], var, nil, tag_typeDeclaration)
	if initval then applyOperator( node, "=", {initval}) end
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
	local out = register()
	printCodeLine( utils.constructor_format( fmt, {out = out, inp = initval}))
	local var = typedb:def_type( 0, name, out)
	typedb:def_reduction( referenceTypeMap[ type], var, nil, tag_typeDeclaration)
end

function defineVariable( node, typeId, varName, initVal)
	local register = typedb:get_instance( "register")
	local defcontext = typedb:get_instance( "defcontext")
	if register then
		defineLocalVariable( node, typeId, varName, initval, register)
	elseif defcontext then
		utils.errorMessage( line, "Member variables not implemented yet")
	else
		defineGlobalVariable( node, typeId, varName, initval)
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
			local this_constructor = arg[1].constructor
			for ri,redu in ipairs(reductions) do
				if redu.constructor then
					this_constructor = redu.constructor( this_constructor)
				end
			end
			local param_constructor_ar = {}
			local parameters = typedb:type_parameters( item.type)
			for ii=2,#arg do
				if parameters[ii-1].type == arg[ii].type then
				else
					local param_type,param_constructor_func = typedb:get_reduction( parameters[ii-1].type, arg[ii].type, tagmask_resolveType)
					local param_constructor = arg[ii].constructor
					if param_type then
						if param_constructor_func then
							param_constructor = param_constructor_func( param_constructor)
						end
						table.insert( param_constructor_ar, param_constructor)
					else
						break
					end
				end
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

function getInstructionList( node, arg)
	return ""
end

function defineFunction( node, arg)
	local linkage = node.arg[1].linkage
	local attributes = node.arg[1].attributes
	local returnTypeName = llvmTypes[ arg[2]]
	if not returnTypeName then
	end
	local functionName = arg[3]
	local args = table.concat( arg[4], ", ")
	local body = getInstructionList( node.arg[5])
	printCodeLine( utils.code_format_varg( "define {1} {2} @{3}( {4} ) {5} {\n{6}}", linkage, returnTypeName, functionName, args, attributes, body))
end

function defineProcedure( node, arg)
	local linkage = node.arg[1].linkage
	local attributes = node.arg[1].attributes
	local functionName = arg[2]
	local args = table.concat( arg[3], ", ")
	local body = getInstructionList( node.arg[4])
	printCodeLine( utils.code_format_varg( "define {1} void @{2}( {3} ) {4} {\n{5}}", linkage, functionName, args, attributes, body))
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
	defineVariable( node, subnode[1], subnode[2], nil)
end

function typesystem.vardef_assign( node)
	local subnode = utils.traverse( typedb, node)
	defineVariable( node, subnode[1], subnode[2], subnode[3])
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

function typesystem.program( node)
	initFirstClassCitizens()
	openCode( "program")
	local rt = utils.visit( typedb, node)
	print( closeCode( cd))
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
	for ii=1,icount,1 do
		expr = applyOperator( node, "->", {expr})
	end
	return applyOperator( node, node.arg[2].value, {expr})
end

return typesystem

