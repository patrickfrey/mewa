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

	index = {
		["long"] = "{out} = getelementptr {pointee}, {pointee}* {arg1}, i64 {arg2}\n",
		["ulong"] = "{out} = getelementptr {pointee}, {pointee}* {arg1}, i64 {arg2}\n",
		["int"] = "{out} = getelementptr {pointee}, {pointee}* {arg1}, i32 {arg2}\n",
		["uint"] = "{1} = zext i32 {arg2} to i64\n{out} = getelementptr {pointee}, {pointee}* {arg1}, i64 {1}\n",
		["short"] = "{out} = getelementptr {pointee}, {pointee}* {arg1}, i16 {arg2}\n",
		["ushort"] = "{1} = zext i16 {arg2} to i64\n{out} = getelementptr {pointee}, {pointee}* {arg1}, i64 {1}\n",
		["byte"] = "{1} = zext i8 {arg2} to i64\n{out} = getelementptr {pointee}, {pointee}* {arg1}, i64 {1}\n"
	},
	cmpop = {
		["=="] = "{1} = {out} = icmp eq {pointee}* {1}, {2}\n",
		["!="] = "{1} = {out} = icmp ne {pointee}* {1}, {2}\n",
		["<="] = "{1} = {out} = icmp sle {pointee}* {1}, {2}\n",
		["<"] = "{1} = {out} = icmp slt {pointee}* {1}, {2}\n",
		[">="] = "{1} = {out} = icmp sge {pointee}* {1}, {2}\n",
		[">"] = "{1} = {out} = icmp sgt {pointee}* {1}, {2}\n"}
}

llvmir.control = {
	falseExitToBoolean =  "{1}:\nbr label %{2}\n{falseExit}:\nbr label %{2}\n{2}:\n{out} = phi i1 [ 1, %{1} ], [0, %{falseExit}]\n",
	trueExitToBoolean =  "{1}:\nbr label %{2}\n{trueExit}:\nbr label %{2}\n{2}:\n{out} = phi i1 [ 1, %{trueExit} ], [0, %{1}]\n",
	booleanToFalseExit = "br i1 {inp}, label %{1}, label %{out}\n{1}:\n",
	booleanToTrueExit = "br i1 {inp}, label %{out}, label %{1}\n{1}:\n",
	invertedControlType = "br label %{out}\n{inp}:\n",
	label = "br label %{inp}\n{inp}:\n",
	returnStatement = "ret {type} {inp}\n",
	functionDeclaration = "define {lnk} {rtype} @{symbolname}( {paramstr} ) {attr} {\nentry:\n{body}}\n",
	functionCall = "{out} = call {rtype} @{symbolname}( {callargstr})\n",
	procedureCall = "call void @{symbolname}( {callargstr})\n",
	extern_functionDeclaration = "declare external {rtype} @{symbolname}( {argstr} ) #1 nounwind\n",
	stringConstDeclaration = "{out} = private unnamed_addr constant [{size} x i8] c\"{value}\\00\"",
	stringConstConstructor = "{out} = getelementptr inbounds [{size} x i8], [{size} x i8]* @{name}, i64 0, i64 0\n",
	mainDeclaration = "define external i32 @main() #0 noinline nounwind {\nentry:\n{body}ret i32 0\n}\n"
}

local pointerTypeMap = {}
function llvmir.pointerType( llvmPointeeType)
	local pointerType = pointerTypeMap[ llvmPointeeType]
	if not pointerType then
		pointerType = utils.template_format( pointerTemplate, {pointee=llvmPointeeType, size_t="i64"} )
		pointerTypeMap[ llvmPointeeType] = pointerType
	end
	return pointerType
end

return llvmir

