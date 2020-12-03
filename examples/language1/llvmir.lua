local utils = require "typesystem_utils"

local M = {}

M.scalar = require "language1/llvmir_scalar"

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
		["=="] = "{1} = ptrtoint {pointee}* {this} to {size_t}\n{2} = ptrtoint {pointee}* {arg1} to {size_t}\n{out} = icmp eq {size_t} {1}, {2}\n",
		["!="] = "{1} = ptrtoint {pointee}* {this} to {size_t}\n{2} = ptrtoint {pointee}* {arg1} to {size_t}\n{out} = icmp ne {size_t} {1}, {2}\n",
		["<="] = "{1} = ptrtoint {pointee}* {this} to {size_t}\n{2} = ptrtoint {pointee}* {arg1} to {size_t}\n{out} = icmp sle {size_t} {1}, {2}\n",
		["<"] = "{1} = ptrtoint {pointee}* {this} to {size_t}\n{2} = ptrtoint {pointee}* {arg1} to {size_t}\n{out} = icmp slt {size_t} {1}, {2}\n",
		[">="] = "{1} = ptrtoint {pointee}* {this} to {size_t}\n{2} = ptrtoint {pointee}* {arg1} to {size_t}\n{out} = icmp sge {size_t} {1}, {2}\n",
		[">"] = "{1} = ptrtoint {pointee}* {this} to {size_t}\n{2} = ptrtoint {pointee}* {arg1} to {size_t}\n{out} = icmp sgt {size_t} {1}, {2}\n"}
}

M.control = {
	falseExitToBoolean =  "{1}:\nbr label %{2}\n{falseExit}:\nbr label %{2}\n{2}:\n{out} = phi i1 [ 1, %{1} ], [0, %{falseExit}]\n",
	trueExitToBoolean =  "{1}:\nbr label %{2}\n{trueExit}:\nbr label %{2}\n{2}:\n{out} = phi i1 [ 1, %{trueExit} ], [0, %{1}]\n",
	booleanToFalseExit = "br i1 {inp}, label %{1}, label %{out}\n{1}:\n",
	booleanToTrueExit = "br i1 {inp}, label %{out}, label %{1}\n{1}:\n",
	invertedControlType = "br %{out}\n{inp}:\n",
	label = "{inp}:\n",
	returnStatement = "ret {type} {inp}\n",
	functionDeclaration = "define {lnk} {rtype} @{symbolname}( {paramstr} ) {attr} {\nentry:\n{body}}\n",
	functionCall = "{out} = call {rtype} @{symbolname}( {callargstr})\n",
	procedureCall = "call void @{symbolname}( {callargstr})\n"
}

local pointerTypeMap = {}
function M.pointerType( llvmPointeeType)
	local pointerType = pointerTypeMap[ llvmPointeeType]
	if not pointerType then
		pointerType = utils.template_format( pointerTemplate, {pointee=llvmPointeeType, size_t="i64"} )
		pointerTypeMap[ llvmPointeeType] = pointerType
	end
	return pointerType
end

return M

