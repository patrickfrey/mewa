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
	ctor = "store {pointee}* null, {pointee}** {this}\n",
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
		["<="] = "{out} = icmp sle {pointee}* {this}, {arg1}\n",
		["<"] = "{out} = icmp slt {pointee}* {this}, {arg1}\n",
		[">="] = "{out} = icmp sge {pointee}* {this}, {arg1}\n",
		[">"] = "{out} = icmp sgt {pointee}* {this}, {arg1}\n"}
}

local arrayTemplate = {
	def_local = "{out} = alloca [{size} x {element}], align 8\n",
	def_global = "{out} = internal global [{size} x {element}] zeroinitializer, align 8\n",
	llvmtype = "[{size} x {element}]",
	class = "array",
	assign = "store [{size} x {element}] {arg1}, [{size} x {element}]* {this}\n",
	scalar = false,
	ctorproc = "define private dso_local void @__ctor_{size}__{element}( {element}* %ar) alwaysinline {\n"
		.. "enter:\nbr label %loop\nloop:\n"
		.. "%ptr = phi {element}* [getelementptr inbounds ([{size} x {element}], [{size} x {element}]* %ar, i32 0, i32 0), %enter], [%r2, %loop]\n"
		.. "{ctors}%r2 = getelementptr inbounds {element}, {element}* %ptr, i64 1\n"
		.. "%r3 = icmp eq {element}* %r2, "
			.. "getelementptr inbounds ({element}, {element}*"
			.. " getelementptr inbounds ([{size} x {element}], [{size} x {element}]* %ar, i32 0, i32 0), i64 {size})\n"
		.. "br i1 %r3, label %end, label %loop\n"
		.. "end:\nret void\n}\n",
	ctorproc_copy = "define private dso_local void @__ctor_{procname}_{size}__{element}( {element}* %ths_ar, {element}* %oth_ar) alwaysinline {\n"
		.. "enter:\nbr label %loop\nloop:\n"
		.. "%ths = phi {element}* [getelementptr inbounds ([{size} x {element}], [{size} x {element}]* %ths_ar, i32 0, i32 0), %enter], [%r2, %loop]\n"
		.. "%oth = phi {element}* [getelementptr inbounds ([{size} x {element}], [{size} x {element}]* %oth_ar, i32 0, i32 0), %enter], [%r3, %loop]\n"
		.. "{ctors}%r2 = getelementptr inbounds {element}, {element}* %ths, i64 1\n%r3 = getelementptr inbounds {element}, {element}* %oth, i64 1\n"
		.. "%r4 = icmp eq {element}* %r2, "
			.. "getelementptr inbounds ({element}, {element}*"
			.. " getelementptr inbounds ([{size} x {element}], [{size} x {element}]* %ths_ar, i32 0, i32 0), i64 {size})\n"
		.. "br i1 %r4, label %end, label %loop\n"
		.. "end:\nret void\n}\n",
	dtorproc = "define private dso_local void @__dtor_{size}__{element}( {element}* %ar) alwaysinline {\n"
		.. "enter:\nbr label %loop\nloop:\n"
		.. "%ptr = phi {element}* [ getelementptr inbounds ({element}, {element}*"
		.. " getelementptr inbounds ([{size} x {element}], [{size} x {element}]* %ar, i32 0, i32 0), i64 {size}), %enter], [%r2, %loop]\n"
		.. "{dtors}%r2 = getelementptr inbounds {element}, {element}* %ptr, i64 -1\n"
		.. "%r3 = icmp eq {element}* %2, getelementptr inbounds ([{size} x {element}], [{size} x {element}]* %ar, i32 0, i32 0)\n"
		.. "br i1 %r3, label %end, label %loop\n"
		.. "end:\nret void\n}\n",
	ctor = "call void @__ctor_{size}__{element}( {element}* {this})\n",
	ctor_copy = "call void @__ctor_{procname}_{size}__{element}( {element}* {this}, {element}* {arg1})\n",
	dtor = "call void @__dtor_{size}__{element}( {element}* {this})\n",
	load = "{out} = load [{size} x {element}], [{size} x {element}]* {this}\n",
	index = {
		["long"] = "{out} = getelementptr inbounds [{size} x {element}], [{size} x {element}]* {this}, i64 0, i64 {arg1}\n",
		["ulong"] = "{out} = getelementptr inbounds [{size} x {element}], [{size} x {element}]* {this}, i64 0, i64 {arg1}\n",
		["int"] = "{out} = getelementptr inbounds [{size} x {element}], [{size} x {element}]* {this}, i32 0, i32 {arg1}\n",
		["uint"] = "{1} = zext i32 {arg1} to i64\n{out} = inbounds getelementptr [{size} x {element}], [{size} x {element}]* {this}, i64 0, i64 {1}\n",
		["short"] = "{out} = getelementptr inbounds [{size} x {element}], [{size} x {element}]* {this}, i16 0, i16 {arg1}\n",
		["ushort"] = "{1} = zext i16 {arg1} to i64\n{out} = getelementptr inbounds [{size} x {element}], [{size} x {element}]* {this}, i64 0, i64 {1}\n",
		["byte"] = "{1} = zext i8 {arg1} to i64\n{out} = getelementptr inbounds [{size} x {element}], [{size} x {element}]* {this}, i64 0, i64 {1}\n"
	}
}

llvmir.structTemplate = {
	structname = "{structname}",
	def_local = "{out} = alloca %{structname}, align 8\n",
	def_global = "{out} = internal global %{structname} zeroinitializer, align 8\n",
	llvmtype = "%{structname}",
	scalar = false,
	class = "struct",
	align = 8,
	assign = "store %{structname} {arg1}, %{structname}* {this}\n",
	ctorproc = "define private dso_local void @__ctor_{structname}( %{structname}* %ptr) alwaysinline {\n"
		.. "enter:\n{ctors}br label %end\nend:\nret void\n}\n",
	ctorproc_copy = "define private dso_local void @__ctor_{procname}_{structname}( %{structname}* %ptr, %{structname}* %oth) alwaysinline {\n"
		.. "enter:\n{ctors}br label %end\nend:\nret void\n}\n",
	ctorproc_elements = "define private dso_local void @__ctor_elements_{structname}( %{structname}* %ptr{paramstr}) alwaysinline {\n"
		.. "enter:\n{ctors}br label %end\nend:\nret void\n}\n",
	dtorproc = "define private dso_local void @__dtor_{structname}( %{structname}* %ptr) alwaysinline {\n"
		.. "enter:\n{dtors}br label %end\nend:\nret void\n}\n",
	ctor = "call void @__ctor_{structname}( %{structname}* {this})\n",
	ctor_copy = "call void @__ctor_{procname}_{structname}( %{structname}* {this}, %{structname}* {arg1})\n",
	ctor_elements = "call void @__ctor_elements_{structname}( %{structname}* {this}{args})\n",
	dtor = "call void @__dtor_{structname}( %{structname}* {this})\n",
	load = "{out} = load %{classname}, %{structname}* {this}\n",
	loadelemref = "{out} = getelementptr inbounds %{structname}, %{structname}* {this}, i32 0, i32 {index}\n",
	loadelem = "{1} = getelementptr inbounds %{structname}, %{structname}* {this}, i32 0, i32 {index}\n{out} = load {type}, {type}* {1}\n",
	typedef = "%{structname} = type { {llvmtype} }\n"
}

llvmir.classTemplate = {
	classname = "{classname}",
	def_local = "{out} = alloca %{classname}, align 8\n",
	def_global = "{out} = internal global %{classname} zeroinitializer, align 8\n",
	llvmtype = "%{classname}",
	scalar = false,
	class = "class",
	align = 8,
	assign = "store %{classname} {arg1}, %{classname}* {this}\n",
	ctorproc = "define private dso_local void @__ctor_{classname}( %{classname}* %ptr) alwaysinline {\n"
		.. "enter:\n{ctors}br label %end\nend:\nret void\n}\n",
	dtorproc = "define private dso_local void @__dtor_{classname}( %{classname}* %ptr) alwaysinline {\n"
		.. "enter:\n{dtors}br label %end\nend:\nret void\n}\n",
	ctor = "call void @__ctor_{classname}( %{classname}* {this})\n",
	dtor = "call void @__dtor_{classname}( %{classname}* {this})\n",
	load = "{out} = load %{classname}, %{classname}* {this}\n",
	loadelemref = "{out} = getelementptr inbounds %{classname}, %{classname}* {this}, i32 0, i32 {index}\n",
	loadelem = "{1} = getelementptr inbounds %{classname}, %{classname}* {this}, i32 0, i32 {index}\n{out} = load {type}, {type}* {1}\n",
	typedef = "%{classname} = type { {llvmtype} }\n",
	methodCallType = "{rtype} (%{classname}*{argstr})*",
	sretMethodCallType = "void ({rtype}* sret, %{classname}*{argstr})*"
}

llvmir.interfaceTemplate = {
	interfacename = "{interfacename}",
	vmtname = "%{interfacename}__VMT",
	def_local = "{out} = alloca %{interfacename}, align 8\n",
	def_global = "{out} = internal global %{interfacename} zeroinitializer, align 8\n",
	llvmtype = "%{interfacename}",
	scalar = false,
	class = "interface",
	align = 8,
	assign = "store %{interfacename} {arg1}, %{interfacename}* {this}\n",
	ctor = "{2} = getelementptr inbounds %{interfacename}, %{interfacename}* {this}, i32 0, i32 0\n"
			.. "store i8* null, i8** {2}, align 8\n"
			.. "{3} = getelementptr inbounds %{interfacename}, %{interfacename}* {this}, i32 0, i32 1\n"
			.. "store %{interfacename}__VMT* null, %{interfacename}__VMT** {3}, align 8\n",
	ctor_copy = "{1} = load %{interfacename}, %{interfacename}* {arg1}\nstore %{interfacename} {1}, %{interfacename}* {this}\n",
	load = "{out} = load %{interfacename}, %{interfacename}* {this}\n",
	loadelemref = "{out} = getelementptr inbounds %{interfacename}, %{interfacename}* {this}, i32 0, i32 {index}\n",
	loadelem = "{1} = getelementptr inbounds %{interfacename}, %{interfacename}* {this}, i32 0, i32 {index}\n{out} = load {type}, {type}* {1}\n",
	vmtdef = "%{interfacename}__VMT = type { {llvmtype} }\n",
	typedef = "%{interfacename} = type {i8*, %{interfacename}__VMT* }\n",
	methodCallType = "{rtype} (i8*{argstr})*",
	sretMethodCallType = "void ({rtype}* sret, i8*{argstr})*",
	getClassInterface = "{1} = bitcast %{classname}* {this} to i8*\n"
			..  "{2} = getelementptr inbounds %{interfacename}, %{interfacename}* {out}, i32 0, i32 0\n"
			..  "store i8* {1}, i8** {2}, align 8\n"
			..  "{3} = getelementptr inbounds %{interfacename}, %{interfacename}* {out}, i32 0, i32 1\n"
			..  "store %{interfacename}__VMT* @{classname}__VMT__{interfacename}, %{interfacename}__VMT** {3}, align 8\n"
}

llvmir.callableDescr = {
	class = "callable"
}

llvmir.control = {
	falseExitToBoolean =  "{1}:\nbr label %{2}\n{falseExit}:\nbr label %{2}\n{2}:\n{out} = phi i1 [ 1, %{1} ], [0, %{falseExit}]\n",
	trueExitToBoolean =  "{1}:\nbr label %{2}\n{trueExit}:\nbr label %{2}\n{2}:\n{out} = phi i1 [ 1, %{trueExit} ], [0, %{1}]\n",
	booleanToFalseExit = "br i1 {inp}, label %{1}, label %{out}\n{1}:\n",
	booleanToTrueExit = "br i1 {inp}, label %{out}, label %{1}\n{1}:\n",
	invertedControlType = "br label %{out}\n{inp}:\n",
	terminateFalseExit = "br label %{out}\n{1}:\n",
	terminateTrueExit = "br label %{out}\n{1}:\n",
	label = "br label %{inp}\n{inp}:\n",
	returnStatement = "ret {type} {this}\n",
	functionDeclaration = "define {lnk} {rtype} @{symbolname}( {thisstr}{paramstr} ) {attr} {\nentry:\n{body}}\n",
	sretFunctionDeclaration = "define {lnk} void @{symbolname}( {rtparamstr}{thisstr}{paramstr} ) {attr} {\nentry:\n{body}}\n",
	functionCall = "{out} = call {rtype}{signature} @{symbolname}( {callargstr})\n",
	procedureCall = "call void{signature} @{symbolname}( {callargstr})\n",
	sretFunctionCall = "call void{signature} @{symbolname}( {rtype}* sret {rvalref}{callargstr})\n",
	interfaceFunctionCall = "{1} = getelementptr inbounds %{interfacename}, %{interfacename}* @{symbolname}, i64 0, i64 1\n" 
				.. "{2} = getelementptr inbounds %{interfacename}__VMT, %{interfacename}__VMT* {1}, i64 0, i64 {index}\n"
				.. "{3} = getelementptr inbounds {llvmtype}, {llvmtype}* {2}, i64 0, i64 0\n"
				.. "{out} = call {llvmtype} {3}( {callargstr})\n",
	sretInterfaceFunctionCall = "{1} = getelementptr inbounds %{interfacename}, %{interfacename}* @{symbolname}, i64 0, i64 1\n" 
				.. "{2} = getelementptr inbounds %{interfacename}__VMT, %{interfacename}__VMT* {1}, i64 0, i64 {index}\n"
				.. "{3} = getelementptr inbounds {llvmtype}, {llvmtype}* {2}, i64 0, i64 0\n"
				.. "call {llvmtype} {3}( {rtype}* sret {rvalref}{callargstr})\n",
	interfaceProcedureCall = "{1} = getelementptr inbounds %{interfacename}, %{interfacename}* @{symbolname}, i64 0, i64 1\n" 
				.. "{2} = getelementptr inbounds %{interfacename}__VMT, %{interfacename}__VMT* {1}, i64 0, i64 {index}\n"
				.. "{3} = getelementptr inbounds {llvmtype}, {llvmtype}* {2}, i64 0, i64 0\n"
				.. "call {rtype} {3}( {callargstr})\n",
	extern_functionDeclaration = "declare external {rtype} @{symbolname}( {argstr} ) #1 nounwind\n",
	extern_functionDeclaration_vararg = "declare external {rtype} @{symbolname}( {argstr}, ... ) #1 nounwind\n",
	freeFunctionCallType = "{rtype} ({argstr})*",
	freeFunctionVarargSignature = "({argstr} ...)",
	stringConstDeclaration = "{out} = private unnamed_addr constant [{size} x i8] c\"{value}\\00\"",
	stringConstConstructor = "{out} = getelementptr inbounds [{size} x i8], [{size} x i8]* @{name}, i64 0, i64 0\n",
	mainDeclaration = "declare dso_local i32 @__gxx_personality_v0(...)\n" 
				.. "define dso_local i32 @main(i32 %argc, i8** %argv) #0 personality i8* bitcast (i32 (...)* @__gxx_personality_v0 to i8*)\n"
				.. "{\nentry:\n{body}br label %exit\nexit:\nret i32 0\n}\n",
	vtableElement = "{interface_signature} bitcast ({class_signature} @{symbolname} to {interface_signature})",
	vtable = "${classname}__VMT__{interfacename} = comdat any\n"
		.. "@{classname}__VMT__{interfacename} = linkonce_odr dso_local unnamed_addr constant %{interfacename}__VMT { {llvmtype} }, comdat, align 8\n",
	memPointerCast = "{out} = bitcast i8* {this} to {llvmtype}*\n",
	bytePointerCast = "{out} = bitcast {llvmtype}* {this} to i8*\n"
}

function llvmir.functionAttribute( isPrivate)
	if isPrivate == true then return "#0 nounwind" else return "#0 noinline nounwind" end
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

local arrayDescrMap = {}
function llvmir.arrayDescr( elementDescr, arraySize)
	local llvmElementType = elementDescr.llvmtype
	local arrayDescrKey = llvmElementType .. "#" .. arraySize
	local arrayDescr = arrayDescrMap[ arrayDescrKey]
	if not arrayDescr then
		local dtorElement = utils.template_format( elementDescr.dtor or "", {this="%ptr"})
		arrayDescr = utils.template_format( arrayTemplate, {element=llvmElementType,size=arraySize,dtors=dtorElement})
		arrayDescr.size = elementDescr.size * arraySize
		arrayDescr.align = elementDescr.align
		arrayDescrMap[ arrayDescrKey] = arrayDescr;
		if elementDescr.ctor then print_section( "Auto", arrayDescr.ctorproc) else arrayDescr.ctor = nil end
		if elementDescr.dtor then print_section( "Auto", arrayDescr.dtorproc) else arrayDescr.dtor = nil end
	end
	return arrayDescr
end

return llvmir

