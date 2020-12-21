local utils = require "typesystem_utils"

local llvmir = {}

llvmir.scalar = require "language1/llvmir_scalar"

local pointerTemplate = {
	def_local = "{out} = alloca {pointee}\n",
	def_global = "{out} = internal global {pointee} null\n",
	def_global_val = "{out} = internal global {pointee} {inp}\n",
	default = "null",
	llvmtype = "{pointee}*",
	class = "pointer",
	assign = "store {pointee} {inp}, {pointee}* {this}\n",
	load = "{out} = load {pointee}, {pointee}* {inp}\n",
	ctor = "store {pointee}* null, {pointee}** {this}\n",

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
		["=="] = "{1} = {out} = icmp eq {pointee}* {1}, {2}\n",
		["!="] = "{1} = {out} = icmp ne {pointee}* {1}, {2}\n",
		["<="] = "{1} = {out} = icmp sle {pointee}* {1}, {2}\n",
		["<"] = "{1} = {out} = icmp slt {pointee}* {1}, {2}\n",
		[">="] = "{1} = {out} = icmp sge {pointee}* {1}, {2}\n",
		[">"] = "{1} = {out} = icmp sgt {pointee}* {1}, {2}\n"}
}

local arrayTemplate = {
	def_local = "{out} = alloca [{size} x {element}], align 16\n",
	def_global = "{out} = internal global [{size} x {element}] zeroinitializer, align 16\n",
	llvmtype = "[{size} x {element}]",
	class = "array",
	ctorproc = "define private dso_local hidden void @__ctor_{size}__{element}( {element}* %ar) alwaysinline {\n"
		.. "enter:\nbr label %loop\nloop:\n"
		.. "%ptr = phi {element}* [getelementptr inbounds ([{size} x {element}], [{size} x {element}]* %ar, i32 0, i32 0), %enter], [%r2, %loop]\n"
		.. "{ctorelement}%r2 = getelementptr inbounds {element}, {element}* %ptr, i64 1\n"
		.. "%r3 = icmp eq {element}* %r2, "
			.. "getelementptr inbounds ({element}, {element}*"
			.. " getelementptr inbounds ([{size} x {element}], [{size} x {element}]* %ar, i32 0, i32 0), i64 {size})\n"
		.. "br i1 %r3, label %end, label %loop\n"
		.. "end:\nreturn void\n}\n",
	dtorproc = "define private dso_local hidden void @__dtor_{size}__{element}( {element}* %ar) alwaysinline {\n"
		.. "enter:\nbr label %loop\nloop:\n"
		.. "%ptr = phi {element}* [ getelementptr inbounds ({element}, {element}*"
		.. " getelementptr inbounds ([{size} x {element}], [{size} x {element}]* %ar, i32 0, i32 0), i64 {size}), %enter], [%r2, %loop]\n"
		.. "{dtorelement}%r2 = getelementptr inbounds {element}, {element}* %ptr, i64 -1\n"
		.. "%r3 = icmp eq {element}* %2, getelementptr inbounds ([{size} x {element}], [{size} x {element}]* %ar, i32 0, i32 0)\n"
		.. "br i1 %r3, label %end, label %loop\n"
		.. "end:\nreturn void\n}\n",
	ctor = "call void @__ctor_{size}__{element}( {element}* {this})\n",
	dtor = "call void @__dtor_{size}__{element}( {element}* {this})\n",
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
	def_local = "{out} = alloca %{structname}, align 16\n",
	def_global = "{out} = internal global %{structname} zeroinitializer, align 16\n",
	llvmtype = "%{structname}",
	class = "array",
	ctorproc = "define private dso_local hidden void @__ctor_{structname}( %{structname}* %ptr) alwaysinline {\n"
		.. "enter:\n{ctors}end:\nreturn void\n}\n",
	dtorproc = "define private dso_local hidden void @__dtor_{structname}( %{structname}* %ptr) alwaysinline {\n"
		.. "enter:\n{dtors}end:\nreturn void\n}\n",
	ctor = "call void @__ctor_{structname}( %{structname}* {this})\n",
	dtor = "call void @__dtor_{structname}( %{structname}* {this})\n",
	load = "{out} = load {type}, getelementptr inbounds {type}*, %{structname}* {this}, i64 {index}\n",
	loadref = "{out} = getelementptr inbounds {type}*, %{structname}* {this}, i64 {index}\n"
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
	returnStatement = "ret {type} {inp}\n",
	functionDeclaration = "define {lnk} {rtype} @{symbolname}( {paramstr} ) {attr} {\nentry:\n{body}}\n",
	functionCall = "{out} = call {rtype} @{symbolname}( {callargstr})\n",
	procedureCall = "call void @{symbolname}( {callargstr})\n",
	extern_functionDeclaration = "declare external {rtype} @{symbolname}( {argstr} ) #1 nounwind\n",
	stringConstDeclaration = "{out} = private unnamed_addr constant [{size} x i8] c\"{value}\\00\"",
	stringConstConstructor = "{out} = getelementptr inbounds [{size} x i8], [{size} x i8]* @{name}, i64 0, i64 0\n",
	mainDeclaration = "define external i32 @main() #0 noinline nounwind {\nentry:\n{body}ret i32 0\n}\n",
	structdef = "%{structname} = type { {llvmtype} }\n"
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
		local ctorElement = utils.template_format( elementDescr.ctor or "", {this="%ptr"})
		local dtorElement = utils.template_format( elementDescr.dtor or "", {this="%ptr"})
		arrayDescr = utils.template_format( arrayTemplate, {element=llvmElementType, size=arraySize, ctorelement=ctorElement, dtorelement=dtorElement})
		arrayDescrMap[ arrayDescrKey] = arrayDescr;
		if elementDescr.ctor then print_section( "Auto", arrayDescr.ctorproc) else arrayDescr.ctor = nil end
		if elementDescr.dtor then print_section( "Auto", arrayDescr.dtorproc) else arrayDescr.dtor = nil end
	end
	return arrayDescr
end

return llvmir

