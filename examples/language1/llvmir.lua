local utils = require "typesystem_utils"

local llvmir = {}

llvmir.scalar = require "language1/llvmir_scalar"

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

	index = {
		["long"] = "{out} = getelementptr {pointee}, {pointee}* {this}, i64 {arg1}\n",
		["ulong"] = "{out} = getelementptr {pointee}, {pointee}* {this}, i64 {arg1}\n",
		["int"] = "{out} = getelementptr {pointee}, {pointee}* {this}, i32 {arg1}\n",
		["uint"] = "{1} = zext i32 {arg1} to i64\n{out} = getelementptr {pointee}, {pointee}* {this}, i64 {1}\n",
		["short"] = "{out} = getelementptr {pointee}, {pointee}* {this}, i16 {arg1}\n",
		["ushort"] = "{1} = zext i16 {arg1} to i64\n{out} = getelementptr {pointee}, {pointee}* {this}, i64 {1}\n",
		["byte"] = "{1} = zext i8 {arg1} to i64\n{out} = getelementptr {pointee}, {pointee}* {this}, i64 {1}\n"
	},
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
	ctorproc_init = "define private dso_local void @__ctor_init_{size}__{elementsymbol}( [{size} x {element}]* %ar, i32 %start){attributes} {\n"
		.. "enter:\n{entercode}"
		.. "%ths_base = getelementptr inbounds [{size} x {element}], [{size} x {element}]* %ar, i32 0, i32 %start\n"
		.. "%ths_top = getelementptr inbounds [{size} x {element}], [{size} x {element}]* %ar, i32 0, i32 {size}\n"
		.. "br label %loop\nloop:\n"
		.. "%ths = phi {element}* [%ths_base, %enter], [%A2, %cond]\n"
		.. "{ctors}br label %cond\ncond:\n"
		.. "%A2 = getelementptr inbounds {element}, {element}* %ths, i64 1\n"
		.. "%A3 = icmp eq {element}* %A2, %ths_top\n"
		.. "br i1 %A3, label %end, label %loop\n{rewind}"
		.. "end:\nret void\n}\n",
	rewind_ctor = "%D1 = ptrtoint {element}* %ths to i64\n"
		.. "%D2 = ptrtoint {element}* %ths_base to i64\n"
		.. "%D3 = getelementptr inbounds {element}, {element}* %ths_base, i64 1\n"
		.. "%D4 = ptrtoint {element}* %D3 to i64\n"
		.. "%D5 = sub i64 %D4, %D2\n"
		.. "%D6 = sub i64 %D1, %D2\n"
		.. "%D7 = udiv exact i64 %D6, %D5\n"
		.. "%D8 = trunc i64 %D7 to i32\n"
		.. "call void @__dtor_{size}__{elementsymbol}( [{size} x {element}]* %ar, i32 %D8)\n",
	ctorproc_copy = "define private dso_local void @__ctor_copy_{size}__{elementsymbol}( [{size} x {element}]* %ths_ar, [{size} x {element}]* %oth_ar){attributes} {\n"
		.. "enter:\n{entercode}"
		.. "%ths_base = getelementptr inbounds [{size} x {element}], [{size} x {element}]* %ths_ar, i32 0, i32 0\n"
		.. "%ths_top = getelementptr inbounds [{size} x {element}], [{size} x {element}]* %ths_ar, i32 0, i32 {size}\n"
		.. "%oth_base = getelementptr inbounds [{size} x {element}], [{size} x {element}]* %oth_ar, i32 0, i32 0\n"
		.. "br label %loop\nloop:\n"
		.. "%ths = phi {element}* [%ths_base, %enter], [%A2, %cond]\n"
		.. "%oth = phi {element}* [%oth_base, %enter], [%A3, %cond]\n"
		.. "{ctors}br label %cond\ncond:\n"
		.. "%A2 = getelementptr inbounds {element}, {element}* %ths, i64 1\n%A3 = getelementptr inbounds {element}, {element}* %oth, i64 1\n"
		.. "%A4 = icmp eq {element}* %A2, %ths_top\n"
		.. "br i1 %A4, label %end, label %loop\n{rewind}"
		.. "end:\nret void\n}\n",
	memberwise_start = "{cnt} = alloca i32, align 4\nstore i32 -1, i32* {cnt}\n"
		.. "{base} = getelementptr inbounds [{size} x {element}], [{size} x {element}]* {this}, i32 0, i32 0\n",
	memberwise_next = "{1} = load i32, i32* {cnt}\n{2} = add nsw i32 {1}, 1\nstore i32 {2}, i32* {cnt}\n"
		.. "{out} = getelementptr inbounds {element}, {element}* {base}, i32 {2}\n",
	memberwise_index = "{out} = getelementptr inbounds [{size} x {element}], [{size} x {element}]* {this}, i32 0, i32 {index}\n",
	memberwise_cleanup = "{1} = load i32, i32* {cnt}\ncall void @__dtor_{size}__{elementsymbol}( [{size} x {element}]* {this}, i32 {1})\n",
	dtorproc = "define private dso_local void @__dtor_{size}__{elementsymbol}( [{size} x {element}]* %ar, i32 %arsize) alwaysinline {\n"
		.. "enter:\n"
		.. "%X1 = icmp eq i32 %arsize, 0\n"
		.. "br i1 %X1, label %end, label %start\nstart:\n"
		.. "%artop = sub nsw i32 %arsize, 1\n"
		.. "%base = getelementptr inbounds [{size} x {element}], [{size} x {element}]* %ar, i32 0, i32 0\n"
		.. "%top = getelementptr inbounds [{size} x {element}], [{size} x {element}]* %ar, i32 0, i32 %artop\n"
		.. "br label %loop\nloop:\n"
		.. "%ths = phi {element}* [%top, %start], [%A2, %loop]\n"
		.. "%A2 = getelementptr inbounds {element}, {element}* %ths, i64 -1\n{dtors}"
		.. "%A3 = icmp eq {element}* %ths, %base\n"
		.. "br i1 %A3, label %end, label %loop\n"
		.. "end:\nret void\n}\n",
	ctor_init = "call void @__ctor_init_{size}__{elementsymbol}( [{size} x {element}]* {this}, i32 0)\n",
	ctor_init_throwing = "invoke void @__ctor_init_{size}__{elementsymbol}( [{size} x {element}]* {this}, i32 0) to label %{success} unwind label %{cleanup}\n{success}:\n",
	ctor_rest = "call void @__ctor_init_{size}__{elementsymbol}( [{size} x {element}]* {this}, i32 {index})\n",
	ctor_rest_throwing = "invoke void @__ctor_init_{size}__{elementsymbol}( [{size} x {element}]* {this}, i32 {index}) to label %{success} unwind label %{cleanup}\n{success}:\n",
	ctor_copy = "call void @__ctor_copy_{size}__{elementsymbol}( [{size} x {element}]* {this}, [{size} x {element}]* {arg1})\n",
	ctor_copy_throwing = "invoke void @__ctor_copy_{size}__{elementsymbol}( [{size} x {element}]* {this}, [{size} x {element}]* {arg1}) to label %{success} unwind label %{cleanup}\n{success}:\n",

	dtor = "call void @__dtor_{size}__{elementsymbol}( [{size} x {element}]* {this}, i32 {size})\n",
	partial_dtor = "call void @__dtor_{size}__{elementsymbol}( [{size} x {element}]* {this}, i32 {num})\n",
	load = "{out} = load [{size} x {element}], [{size} x {element}]* {this}\n",
	loadelemref = "{out} = getelementptr inbounds [{size} x {element}], [{size} x {element}]* {this}, i32 0, i32 {index}\n",
	loadelem = "{1} = getelementptr inbounds [{size} x {element}], [{size} x {element}]* {this}, i32 0, i32 {index}\n{out} = load {type}, {type}* {1}\n",
	index = {
		["long"] = "{out} = getelementptr inbounds [{size} x {element}], [{size} x {element}]* {this}, i64 0, i64 {arg1}\n",
		["ulong"] = "{out} = getelementptr inbounds [{size} x {element}], [{size} x {element}]* {this}, i64 0, i64 {arg1}\n",
		["int"] = "{out} = getelementptr inbounds [{size} x {element}], [{size} x {element}]* {this}, i32 0, i32 {arg1}\n",
		["uint"] = "{1} = zext i32 {arg1} to i64\n{out} = getelementptr inbounds [{size} x {element}], [{size} x {element}]* {this}, i64 0, i64 {1}\n",
		["short"] = "{out} = getelementptr inbounds [{size} x {element}], [{size} x {element}]* {this}, i16 0, i16 {arg1}\n",
		["ushort"] = "{1} = zext i16 {arg1} to i64\n{out} = getelementptr inbounds [{size} x {element}], [{size} x {element}]* {this}, i64 0, i64 {1}\n",
		["byte"] = "{1} = zext i8 {arg1} to i64\n{out} = getelementptr inbounds [{size} x {element}], [{size} x {element}]* {this}, i64 0, i64 {1}\n"
	}
}

local structTemplate_vars = {
	dtorname = "__dtor_{symbol}"
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
	dtorproc = "define private dso_local void @" .. structTemplate_vars.dtorname .. "( %{symbol}* %ths) {\n"
		.. "enter:\n{dtors}br label %end\nend:\nret void\n}\n",
	ctor_init = "call void @__ctor_init_{symbol}( %{symbol}* {this})\n",
	ctor_init_throwing = "invoke void @__ctor_init_{symbol}( %{symbol}* {this}) to label %{success} unwind label %{cleanup}\n{success}:\n",
	ctor_copy = "call void @__ctor_copy_{symbol}( %{symbol}* {this}, %{symbol}* {arg1})\n",
	ctor_copy_throwing = "invoke void @__ctor_copy_{symbol}( %{symbol}* {this}, %{symbol}* {arg1}) to label %{success} unwind label %{cleanup}\n{success}:\n",
	ctor_elements = "call void @__ctor_elements_{symbol}( %{symbol}* {this}{args})\n",
	ctor_elements_throwing = "invoke void @__ctor_elements_{symbol}( %{symbol}* {this}{args}) to label %{success} unwind label %{cleanup}\n{success}:\n",
	dtorname = structTemplate_vars.dtorname,
	dtor = "call void @" .. structTemplate_vars.dtorname .. "( %{symbol}* {this})\n",
	load = "{out} = load %{symbol}, %{symbol}* {this}\n",
	loadelemref = "{out} = getelementptr inbounds %{symbol}, %{symbol}* {this}, i32 0, i32 {index}\n",
	loadelem = "{1} = getelementptr inbounds %{symbol}, %{symbol}* {this}, i32 0, i32 {index}\n{out} = load {type}, {type}* {1}\n",
	typedef = "%{symbol} = type { {llvmtype} }\n"
}

llvmir.classTemplate = {
	symbol = llvmir.structTemplate.symbol,
	def_local = llvmir.structTemplate.def_local,
	def_global = llvmir.structTemplate.def_global,
	llvmtype = llvmir.structTemplate.llvmtype,
	scalar = false,
	class = "class",
	align = 8,
	assign = llvmir.structTemplate.assign,
	ctorproc_init = llvmir.structTemplate.ctorproc_init,
	ctorproc_copy = llvmir.structTemplate.ctorproc_copy,
	ctorproc_elements = llvmir.structTemplate.ctorproc_elements,
	dtorproc = llvmir.structTemplate.dtorproc,
	partial_dtorproc = "define private dso_local void @__partial_dtor_{symbol}( %{symbol}* %ths, i32 %istate) {\n"
		.. "enter:\n{dtors}br label %end\nend:\nret void\n}\n",
	partial_dtorelem = "{creg} = icmp sle i32 %istate, {istate}\n"
		.. "br i1 {creg}, label %{1}, label %{2}\n"
		.. "{1}:\n{dtor}br label %{2}\n{2}:\n",
	partial_dtor = "{1} = load i32, i32* %initstate\n"
		.. "call void @__partial_dtor_{symbol}( %{symbol}* %ths, i32 {1})\n",
	ctor_init = llvmir.structTemplate.ctor_init,
	ctor_init_throwing = llvmir.structTemplate.ctor_init_throwing,
	ctor_copy = llvmir.structTemplate.ctor_copy,
	ctor_copy_throwing = llvmir.structTemplate.ctor_copy_throwing,
	ctor_elements = llvmir.structTemplate.ctor_elements,
	ctor_elements_throwing = llvmir.structTemplate.ctor_elements_throwing,
	dtorname = llvmir.structTemplate.dtorname,
	dtor = llvmir.structTemplate.dtor,
	load = llvmir.structTemplate.load,
	loadelemref = llvmir.structTemplate.loadelemref,
	loadelem = llvmir.structTemplate.loadelem,
	typedef = llvmir.structTemplate.typedef
}

llvmir.interfaceTemplate = {
	symbol = llvmir.structTemplate.symbol,
	vmtname = "%{symbol}__VMT",
	def_local = llvmir.structTemplate.def_local,
	def_global = llvmir.structTemplate.def_global,
	llvmtype = llvmir.structTemplate.llvmtype,
	scalar = false,
	class = "interface",
	align = 8,
	assign = llvmir.structTemplate.assign,
	ctor_init = "{2} = getelementptr inbounds %{symbol}, %{symbol}* {this}, i32 0, i32 0\n"
			.. "store i8* null, i8** {2}, align 8\n"
			.. "{3} = getelementptr inbounds %{symbol}, %{symbol}* {this}, i32 0, i32 1\n"
			.. "store %{symbol}__VMT* null, %{symbol}__VMT** {3}, align 8\n",
	ctor_copy = "{1} = load %{symbol}, %{symbol}* {arg1}\nstore %{symbol} {1}, %{symbol}* {this}\n",
	load = llvmir.structTemplate.load,
	loadelemref = llvmir.structTemplate.loadelemref,
	loadelem = "{1} = getelementptr inbounds %{symbol}, %{symbol}* {this}, i32 0, i32 {index}\n{out} = load {type}, {type}* {1}\n",
	vmtdef = "%{symbol}__VMT = type { {llvmtype} }\n",
	typedef = "%{symbol} = type {i8*, %{symbol}__VMT* }\n",
	getClassInterface = "{1} = bitcast %{classname}* {this} to i8*\n"
			.. "{2} = getelementptr inbounds %{symbol}, %{symbol}* {out}, i32 0, i32 0\n"
			.. "store i8* {1}, i8** {2}, align 8\n"
			.. "{3} = getelementptr inbounds %{symbol}, %{symbol}* {out}, i32 0, i32 1\n"
			.. "store %{symbol}__VMT* @{classname}__VMT__{symbol}, %{symbol}__VMT** {3}, align 8\n",
	loadVmtMethod = "{1} = getelementptr inbounds %{symbol}, %{symbol}* {this}, i32 0, i32 1\n"
			.. "{2} = load %{symbol}__VMT*, %{symbol}__VMT** {1}\n"
			.. "{3} = getelementptr inbounds %{symbol}__VMT, %{symbol}__VMT* {2}, i32 0, i32 {index}\n"
			.. "{out_func} = load {llvmtype}, {llvmtype}* {3}, align 8\n"
			.. "{5} = getelementptr inbounds %{symbol}, %{symbol}* {this}, i32 0, i32 0\n"
			.. "{out_this} = load i8*, i8** {5}\n"
}

local functionVariableTemplate = { -- inherits pointer template
	scalar = false,
	class = "function variable"
}
local procedureVariableTemplate = { -- inherits pointer template
	scalar = false,
	class = "procedure variable"
}

llvmir.anyClassPointerDescr = {
	llvmtype = "i8*",
	scalar = false,
	class = "any class pointer"
}
llvmir.anyStructPointerDescr = {
	llvmtype = "i8*",
	scalar = false,
	class = "any struct pointer"
}
llvmir.anyFunctionDescr = {
	llvmtype = "i8*",
	scalar = false,
	class = "transfer"
}
llvmir.genericClassDescr = {
	class = "generic_class"
}
llvmir.genericStructDescr = {
	class = "generic_struct"
}
llvmir.genericProcedureDescr = {
	class = "generic_procedure"
}
llvmir.genericFunctionDescr = {
	class = "generic_function"
}
llvmir.callableDescr = {
	class = "callable"
}

llvmir.constexprIntegerDescr = {
	llvmtype = "i64",
	scalar = true,
	class = "constexpr"	
}
llvmir.constexprUIntegerDescr = {
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
llvmir.constexprNullDescr = {
	llvmtype = "i8*",
	scalar = false,
	class = "constexpr"	
}
llvmir.constexprStructDescr = {
	llvmtype = "void",
	scalar = false,
	class = "constexpr"
}

llvmir.control = {
	falseExitToBoolean =  "{1}:\nbr label %{2}\n{falseExit}:\nbr label %{2}\n{2}:\n{out} = phi i1 [ 1, %{1} ], [0, %{falseExit}]\n",
	trueExitToBoolean =  "{1}:\nbr label %{2}\n{trueExit}:\nbr label %{2}\n{2}:\n{out} = phi i1 [ 1, %{trueExit} ], [0, %{1}]\n",
	booleanToFalseExit = "br i1 {inp}, label %{1}, label %{out}\n{1}:\n",
	booleanToTrueExit = "br i1 {inp}, label %{out}, label %{1}\n{1}:\n",
	invertedControlType = "br label %{out}\n{inp}:\n",
	terminateFalseExit = "br label %{out}\n{1}:\n",
	terminateTrueExit = "br label %{out}\n{1}:\n",
	gotoStatementPrefix = "br label ",
	label = "br label %{inp}\n{inp}:\n",
	gotoStatement = "br label %{inp}\n",
	plainLabel = "{inp}:\n",
	returnStatement = "ret {type} {this}\n",
	returnFromProcedure = "ret void\n",
	returnFromMain = "{1} = load i32, i32* {this}\nret i32 {1}\n",
	functionDeclaration = "define {lnk} {rtllvmtype} @{symbolname}( {paramstr} ) {attr} {\nentry:\n{body}}\n",
	sretFunctionDeclaration = "define {lnk} void @{symbolname}( {paramstr} ) {attr} {\nentry:\n{body}}\n",
	functionCall = "{out} = call {rtllvmtype}{signature} {func}( {callargstr})\n",
	procedureCall = "call void{signature} {func}( {callargstr})\n",
	sretFunctionCall = "call void{signature} {func}( {rtllvmtype}* sret {reftyperef}{callargstr})\n",
	functionCallThrowing = "{out} = invoke {rtllvmtype}{signature} {func}( {callargstr}) to label %{success} unwind label %{cleanup}\n{success}:\n",
	procedureCallThrowing = "invoke void{signature} {func}( {callargstr}) to label %{success} unwind label %{cleanup}\n{success}:\n",
	sretFunctionCallThrowing = "invoke void{signature} {func}( {rtllvmtype}* sret {reftyperef}{callargstr}) to label %{success} unwind label %{cleanup}\n{success}:\n",	
	extern_functionDeclaration = "declare external {rtllvmtype} @{symbolname}( {argstr} ) nounwind\n",
	functionCallType = "{rtllvmtype} ({argstr})*",
	functionVarargSignature = "({argstr} ...)",
	stringConstDeclaration = "@{name} = private unnamed_addr constant [{size} x i8] c\"{value}\\00\"",
	stringConstConstructor = "{out} = getelementptr inbounds [{size} x i8], [{size} x i8]* @{name}, i64 0, i64 0\n",
	mainDeclaration = "define dso_local i32 @main(i32 %argc, i8** %argv) #0 personality i8* bitcast (i32 (...)* @__gxx_personality_v0 to i8*)\n{\nentry:\n{body}}\n",
	vtableElement = "{interface_signature} bitcast ({class_signature} @{symbolname} to {interface_signature})",
	vtable = "${classname}__VMT__{interfacename} = comdat any\n"
		.. "@{classname}__VMT__{interfacename} = linkonce_odr dso_local unnamed_addr constant %{interfacename}__VMT { {llvmtype} }, comdat, align 8\n",
	memPointerCast = "{out} = bitcast i8* {this} to {llvmtype}*\n",
	bytePointerCast = "{out} = bitcast {llvmtype}* {this} to i8*\n"
}

llvmir.exception = {
	section = "%__L_Exception = type { i64, i8* }\n"
		.. "@__L_ExceptionSize = constant i32 ptrtoint(%__L_Exception* getelementptr(%__L_Exception, %__L_Exception* null, i32 1) to i32)\n"
		.. "declare external i8* @__cxa_allocate_exception( i32)\n"
		.. "declare external void @__cxa_throw( i8*, i8*, i8*) noreturn\n"
		.. "declare external i8* @__cxa_begin_catch( i8*)\n"
		.. "declare external void @__cxa_end_catch()\n"
		.. "define dso_local void @__L_init__Exception( %__L_Exception* %exception, i64 %code, i8* %msg) {\n"
		.. "%ObjCodeRef = getelementptr inbounds %__L_Exception, %__L_Exception* %exception, i32 0, i32 0\n"
		.. "store i64 %code, i64* %ObjCodeRef\n"
		.. "%ObjMsgRef = getelementptr inbounds %__L_Exception, %__L_Exception* %exception, i32 0, i32 1\n"
		.. "%IsNull = icmp ne i8* %msg, null\n"
		.. "br i1 %IsNull, label %L_COPY, label %L_NULL\n"
		.. "L_COPY:\n"
		.. "%MsgCopy = call i8* @strdup( i8* %msg) nounwind\n"
		.. "store i8* %MsgCopy, i8** %ObjMsgRef\n"
		.. "br label %L_CONT\n"
		.. "L_NULL:\n"
		.. "store i8* null, i8** %ObjMsgRef\n"
		.. "br label %L_CONT\n"
		.. "L_CONT:\n"
		.. "ret void\n"
		.. "}\n"
		.. "define dso_local void @__L_throw__Exception( %__L_Exception* %exception) {\n"
		.. "%SizeObObj = load i32, i32* @__L_ExceptionSize\n"
		.. "%Mem = call i8* @__cxa_allocate_exception( i32 %SizeObObj) nounwind\n"
		.. "%Obj = bitcast i8* %Mem to %__L_Exception*\n"
		.. "%exceptionVal = load %__L_Exception, %__L_Exception* %exception\n"
		.. "store %__L_Exception %exceptionVal, %__L_Exception* %Obj\n"
		.. "call void @__cxa_throw( i8* %Mem, i8* null, i8* null) noreturn\n"
		.. "unreachable\n"
		.. "}\n"
		.. "define dso_local void @__L_free__ExceptionMsg( i8* %msg) {\n"
		.. "%IsNull = icmp ne i8* %msg, null\n"
		.. "br i1 %IsNull, label %L_FREE, label %L_DONE\n"
		.. "L_FREE:\n"
		.. "call void @free( i8* %msg) nounwind\n"
		.. "br label %L_DONE\n"
		.. "L_DONE:\n"
		.. "ret void\n"
		.. "}\n",
	allocLandingpad = "%excptr = alloca i8*\n%excidx = alloca i32\n",
	allocExceptionLocal = "%exception = alloca %__L_Exception\n",
	allocInitstate = "%initstate = alloca i32\n",
	initExceptionLocal = "call void @__L_init__Exception( %__L_Exception* %exception, i64 {errcode}, i8* {errmsg})\n",
	throwExceptionLocal = "call void @__L_throw__Exception( %__L_Exception* %exception)\nunreachable\n",
	loadExceptionErrCode = "{1} = getelementptr inbounds %__L_Exception, %__L_Exception* %exception, i32 0, i32 0\n"
		.. "{out} = load i64, i64* {1}\n",
	loadExceptionErrMsg = "{1} = getelementptr inbounds %__L_Exception, %__L_Exception* %exception, i32 0, i32 1\n"
		.. "{out} = load i8*, i8** {1}\n",
	freeExceptionErrMsg = "call void @__L_free__ExceptionMsg( i8* {this})\n",
	catch = "{landingpad}:\n"
		.. "{1} = landingpad { i8*, i32 } catch i8* null\n"
		.. "{2} = extractvalue { i8*, i32 } {1}, 0\n"
		.. "{3} = call i8* @__cxa_begin_catch( i8* {2})\n"
		.. "{4} = bitcast i8* {3} to %__L_Exception*\n"
		.. "{5} = load %__L_Exception, %__L_Exception* {4}\n"
		.. "store %__L_Exception {5}, %__L_Exception* %exception\n"
		.. "call void @__cxa_end_catch()\n",
	storeErrCodeToRetval = "{1} = getelementptr inbounds %__L_Exception, %__L_Exception* %exception, i32 0, i32 0\n"
		.. "{2} = load i64, i64* {1}\n"
		.. "{3} = trunc i64 {2} to i32\n"
		.. "store i32 {3}, i32* {retval}\n",
	cleanup_start = "{landingpad}:\n"
		.. "{1} = landingpad { i8*, i32 } cleanup\n"
		.. "{2} = extractvalue { i8*, i32 } {1}, 0\n"
		.. "store i8* {2}, i8** %excptr\n"
		.. "{3} = extractvalue { i8*, i32 } {1}, 1\n"
		.. "store i32 {3}, i32* %excidx\n",
	cleanup_end = "{1} = load i8*, i8** %excptr\n"
		.. "{2} = load i32, i32* %excidx\n"
		.. "{3} = insertvalue { i8*, i32 } undef, i8* {1}, 0\n"
		.. "{4} = insertvalue { i8*, i32 } {3}, i32 {2}, 1\n"
		.. "resume { i8*, i32 } {4}\n",
	set_constructor_initstate = "store i32 {initstate}, i32* %initstate\n"
}
local externFunctionReferenceMap = {}
function llvmir.externFunctionDeclaration( lang, rtllvmtype, symbolname, argstr, vararg)
	if vararg then if argstr == "" then argstr = "..." else argstr = argstr .. ", ..." end end
	local key = lang .. rtllvmtype .. symbolname .. argstr
	local fmt
	if not externFunctionReferenceMap[ key] then
		externFunctionReferenceMap[ key] = true
		return utils.template_format( llvmir.control.extern_functionDeclaration, {lang=lang, rtllvmtype=rtllvmtype, symbolname=symbolname, argstr=argstr})
	else
		return ""
	end
end
function llvmir.functionAttribute( isInterface, throws)
	local behaviour_exception="";
	if throws then 
		behaviour_exception = " personality i8* bitcast (i32 (...)* @__gxx_personality_v0 to i8*)"
	else
		behaviour_exception = " nounwind"
	end
	local behaviour_inline=""; if isInterface then behaviour_inline = " noinline" end
	return "#0" .. behaviour_inline .. behaviour_exception
end
                                                                           
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
local callableDescrMap = {}
function callableDescr( descr, fmt, llvmtype, rtllvmtype, signature)
	local rt = callableDescrMap[ llvmtype]
	if not rt then
		rt = utils.template_format( pointerTemplate, {pointee=llvmtype} )
		funcDescr = utils.template_format( fmt, {rtllvmtype=rtllvmtype, signature=signature} )
		for key,val in pairs(funcDescr) do rt[key] = val end
		for key,val in pairs(descr) do rt[key] = val end
		callableDescrMap[ llvmtype] = rt
	end
	return rt
end
function llvmir.functionVariableDescr( descr, rtllvmtype, signature)
	return callableDescr( descr, functionVariableTemplate, rtllvmtype .. signature, rtllvmtype, signature)
end
function llvmir.procedureVariableDescr( descr, signature)
	return callableDescr( descr, procedureVariableTemplate, "void" .. signature, nil, signature)
end
function llvmir.callableReferenceDescr( symbolname, ret, throws)
	local class; if ret then class = "function" else class = "procedure" end
	return {class = class, scalar = false, symbolname = symbolname, ret = ret, throws = throws }
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
function llvmir.init()
	print_section( "Typedefs", llvmir.externFunctionDeclaration( "C", "i32", "__gxx_personality_v0", "", true))
end
function llvmir.comment( msg)
	return "; " .. msg .. "\n"
end
return llvmir

