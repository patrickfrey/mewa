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

local arrayTemplate = {
	symbol = "{size}__{elementsymbol}",
	def_local = "{out} = alloca [{size} x {element}], align 8\n",
	def_global = "{out} = internal global [{size} x {element}] zeroinitializer, align 8\n",
	llvmtype = "[{size} x {element}]",
	class = "array",
	assign = "store [{size} x {element}] {arg1}, [{size} x {element}]* {this}\n",
	scalar = false,
	ctorproc_init = "define private dso_local void @__ctor_init_{size}__{elementsymbol}( [{size} x {element}]* %ths_ar, i32 %start){attributes} {\n"
		.. "enter:\n"
		.. "%ths_base = getelementptr inbounds [{size} x {element}], [{size} x {element}]* %ths_ar, i32 0, i32 %start\n"
		.. "%ths_top = getelementptr inbounds [{size} x {element}], [{size} x {element}]* %ths_ar, i32 0, i32 {size}\n"
		.. "br label %loop\nloop:\n"
		.. "%ths = phi {element}* [%ths_base, %enter], [%A2, %cond]\n"
		.. "{ctors}br label %cond\ncond:\n"
		.. "%A2 = getelementptr inbounds {element}, {element}* %ths, i64 1\n"
		.. "%A3 = icmp eq {element}* %A2, %ths_top\n"
		.. "br i1 %A3, label %end, label %loop\n"
		.. "end:\nret void\n}\n",
	ctorproc_copy = "define private dso_local void @__ctor_copy_{size}__{elementsymbol}( [{size} x {element}]* %ths_ar, [{size} x {element}]* %oth_ar){attributes} {\n"
		.. "enter:\n"
		.. "%ths_base = getelementptr inbounds [{size} x {element}], [{size} x {element}]* %ths_ar, i32 0, i32 0\n"
		.. "%ths_top = getelementptr inbounds [{size} x {element}], [{size} x {element}]* %ths_ar, i32 0, i32 {size}\n"
		.. "%oth_base = getelementptr inbounds [{size} x {element}], [{size} x {element}]* %oth_ar, i32 0, i32 0\n"
		.. "br label %loop\nloop:\n"
		.. "%ths = phi {element}* [%ths_base, %enter], [%A2, %cond]\n"
		.. "%oth = phi {element}* [%oth_base, %enter], [%A3, %cond]\n"
		.. "{ctors}br label %cond\ncond:\n"
		.. "%A2 = getelementptr inbounds {element}, {element}* %ths, i64 1\n%A3 = getelementptr inbounds {element}, {element}* %oth, i64 1\n"
		.. "%A4 = icmp eq {element}* %A2, %ths_top\n"
		.. "br i1 %A4, label %end, label %loop\n"
		.. "end:\nret void\n}\n",
	memberwise_index = "{out} = getelementptr inbounds [{size} x {element}], [{size} x {element}]* {this}, i32 0, i32 {index}\n",
	ctor_init = "call void @__ctor_init_{size}__{elementsymbol}( [{size} x {element}]* {this}, i32 0)\n",
	ctor_rest = "call void @__ctor_init_{size}__{elementsymbol}( [{size} x {element}]* {this}, i32 {index})\n",
	ctor_copy = "call void @__ctor_copy_{size}__{elementsymbol}( [{size} x {element}]* {this}, [{size} x {element}]* {arg1})\n",

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
	ctorproc_init = "define private dso_local void @__ctor_init_{symbol}( %{symbol}* %ths){attributes} {\n"
		.. "enter:\n{entercode}{ctors}br label %end\n{rewind}end:\nret void\n}\n",
	ctorproc_copy = "define private dso_local void @__ctor_copy_{symbol}( %{symbol}* %ths, %{symbol}* %oth){attributes} {\n"
		.. "enter:\n{entercode}{ctors}br label %end\n{rewind}end:\nret void\n}\n",
	ctorproc_elements = "define private dso_local void @__ctor_elements_{symbol}( %{symbol}* %ths{paramstr}){attributes} {\n"
		.. "enter:\n{entercode}{ctors}br label %end\n{rewind}end:\nret void\n}\n",
	ctor_init = "call void @__ctor_init_{symbol}( %{symbol}* {this})\n",
	ctor_copy = "call void @__ctor_copy_{symbol}( %{symbol}* {this}, %{symbol}* {arg1})\n",
	ctor_elements = "call void @__ctor_elements_{symbol}( %{symbol}* {this}{args})\n",
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

return llvmir


