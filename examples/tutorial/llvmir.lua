local utils = require "typesystem_utils"
local llvmir = {}
llvmir.scalar = require "tutorial/llvmir_scalar"

local pointerTemplate = {
	def_local = "{out} = alloca {pointee}*\n",
	def_global = "{out} = internal global {pointee}* null\n",
	def_global_val = "{out} = internal global {pointee}* {val}, align 8\n",
	default = "null",
	llvmtype = "{pointee}*",
	class = "pointer",
	align = 8,
	size = 8,
	assign = "store {pointee}* {arg1}, {pointee}** {this}\n",
	ctor_copy = "{1} = load {pointee}*, {pointee}** {arg1}\nstore {pointee}* {1}, {pointee}** {this}\n",
	load = "{out} = load {pointee}*, {pointee}** {this}\n",
	ctor_init = "store {pointee}* null, {pointee}** {this}\n",
	scalar = true,

	cmpop = {
		["=="] = "{out} = icmp eq {pointee}* {this}, {arg1}\n",
		["!="] = "{out} = icmp ne {pointee}* {this}, {arg1}\n",
		["<="] = "{out} = icmp ule {pointee}* {this}, {arg1}\n",
		["<"] = "{out} = icmp ult {pointee}* {this}, {arg1}\n",
		[">="] = "{out} = icmp uge {pointee}* {this}, {arg1}\n",
		[">"] = "{out} = icmp ugt {pointee}* {this}, {arg1}\n"}
}

local arrayTemplate = {
	symbol = "{size}__{elementsymbol}",
	def_local = "{out} = alloca [{size} x {element}], align 8\n",
	def_global = "{out} = internal global [{size} x {element}] zeroinitializer, align 8\n",
	llvmtype = "[{size} x {element}]",
	class = "array",
	assign = "store [{size} x {element}] {arg1}, [{size} x {element}]* {this}\n",
	scalar = false,
	memberwise_index = "{out} = getelementptr inbounds [{size} x {element}], [{size} x {element}]* {this}, i32 0, i32 {index}\n",
	load = "{out} = load [{size} x {element}], [{size} x {element}]* {this}\n",
	loadelemref = "{out} = getelementptr inbounds [{size} x {element}], [{size} x {element}]* {this}, i32 0, i32 {index}\n",
	loadelem = "{1} = getelementptr inbounds [{size} x {element}], [{size} x {element}]* {this}, i32 0, i32 {index}\n{out} = load {type}, {type}* {1}\n",
	index = {
		["int"] = "{out} = getelementptr inbounds [{size} x {element}], [{size} x {element}]* {this}, i32 0, i32 {arg1}\n",
	}
}

llvmir.structTemplate = {
	symbol = "{symbol}",
	def_local = "{out} = alloca %{symbol}, align 8\n",
	def_global = "{out} = internal global %{symbol} zeroinitializer, align 8\n",
	llvmtype = "%{symbol}",
	scalar = false,
	class = "struct",
	align = 8,
	assign = "store %{symbol} {arg1}, %{symbol}* {this}\n",
	load = "{out} = load %{symbol}, %{symbol}* {this}\n",
	loadelemref = "{out} = getelementptr inbounds %{symbol}, %{symbol}* {this}, i32 0, i32 {index}\n",
	loadelem = "{1} = getelementptr inbounds %{symbol}, %{symbol}* {this}, i32 0, i32 {index}\n{out} = load {type}, {type}* {1}\n",
	typedef = "%{symbol} = type { {llvmtype} }\n"
}

llvmir.string = utils.template_format( pointerTemplate, {pointee="i8"})

llvmir.callableDescr = {
	class = "callable"
}
llvmir.constexprIntegerDescr = {
	llvmtype = "i64",
	scalar = true,
	class = "constexpr"
}
llvmir.constexprFloatDescr = {
	llvmtype = "double",
	scalar = true,
	class = "constexpr"
}
llvmir.constexprBooleanDescr = {
	llvmtype = "i1",
	scalar = true,
	class = "constexpr"
}
llvmir.constexprStructDescr = {
	llvmtype = "void",
	scalar = false,
	class = "constexpr"
}

-- Format strings for code control structures
llvmir.control = {
	controlTrueTypeToBoolean =  "{1}:\nbr label %{2}\n{falseExit}:\nbr label %{2}\n{2}:\n{out} = phi i1 [ 1, %{1} ], [0, %{falseExit}]\n",
	controlFalseTypeToBoolean =  "{1}:\nbr label %{2}\n{trueExit}:\nbr label %{2}\n{2}:\n{out} = phi i1 [ 1, %{trueExit} ], [0, %{1}]\n",
	booleanToControlTrueType = "br i1 {inp}, label %{1}, label %{out}\n{1}:\n",
	booleanToControlFalseType = "br i1 {inp}, label %{out}, label %{1}\n{1}:\n",
	invertedControlType = "br label %{out}\n{inp}:\n",
	terminateFalseExit = "br label %{out}\n{1}:\n",
	terminateTrueExit = "br label %{out}\n{1}:\n",
	gotoStatementPrefix = "br label ",
	label = "br label %{inp}\n{inp}:\n",
	gotoStatement = "br label %{inp}\n",
	plainLabel = "{inp}:\n",
	returnStatement = "ret {type} {this}\n",
	returnVoidStatement = "ret void\n",
	functionDeclaration = "define {lnk} {rtllvmtype} @{symbolname}( {paramstr} ) {attr} {\nentry:\n{body}}\n",
	functionCall = "{out} = call {rtllvmtype} {func}( {callargstr})\n",
	procedureCall = "call {rtllvmtype} {func}( {callargstr})\n",
	extern_functionDeclaration = "declare external {rtllvmtype} @{symbolname}( {argstr} ) nounwind\n",
	functionCallType = "{rtllvmtype} ({argstr})*",
	stringConstDeclaration = "@{name} = private unnamed_addr constant [{size} x i8] c\"{value}\\00\"",
	stringConstConstructor = "{out} = getelementptr inbounds [{size} x i8], [{size} x i8]* @{name}, i64 0, i64 0\n",
	mainDeclaration = "define dso_local i32 @main(i32 %argc, i8** %argv) #0\n{\nentry:\n{body}}\n",
}

local pointerDescrMap = {}
function llvmir.pointerDescr( pointeeDescr)
	local llvmPointerType = pointeeDescr.llvmtype
	local pointerDescr = pointerDescrMap[ llvmPointerType]
	if not pointerDescr then
		pointerDescr = utils.template_format( pointerTemplate, {pointee=llvmPointerType} )
		pointerDescrMap[ llvmPointerType] = pointerDescr
	end
	return pointerDescr
end

local arrayDescrMap = {}
function llvmir.arrayDescr( elementDescr, arraySize)
	local llvmElementType = elementDescr.llvmtype
	local arrayDescrKey = llvmElementType .. "#" .. arraySize
	local arrayDescr = arrayDescrMap[ arrayDescrKey]
	if not arrayDescr then
		arrayDescr = utils.template_format( arrayTemplate, {element=llvmElementType, elementsymbol=elementDescr.symbol or llvmElementType, size=arraySize})
		arrayDescr.size = elementDescr.size * arraySize
		arrayDescr.align = elementDescr.align
		arrayDescrMap[ arrayDescrKey] = arrayDescr;
	end
	return arrayDescr
end

return llvmir


