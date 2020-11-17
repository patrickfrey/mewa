local utils = require "typesystem_utils"

local M = {}

M.fcc = require "fcc_language1"

local pointerTemplate = {
	def_local = "{out} = alloca {pointee}\n",
	def_local_val = "{out} = alloca {pointee}\nstore {pointee} {inp}, {pointee}* {out}",
	def_global = "{out} = internal global {pointee} null\n",
	def_global_val = "{out} = internal global {pointee} {inp}\n",
	default = "null",
	llvmtype = "{pointee}*",
	class = "pointer",
	store = "store {pointee} {inp}, {pointee}* {adr}",
	load = "{out} = load {pointee}, {pointee}* {adr}\n",
	index = {
		["long"] = "{out} = getelementptr {pointee}, {pointee}* {arg1}, i64 {arg2}\n",
		["ulong"] = "{out} = getelementptr {pointee}, {pointee}* {arg1}, i64 {arg2}\n",
		["int"] = "{out} = getelementptr {pointee}, {pointee}* {arg1}, i32 {arg2}\n",
		["uint"] = "{1} = zext i32 {arg2} to i64\n{out} = getelementptr {pointee}, {pointee}* {arg1}, i64 {1}\n",
		["short"] = "{out} = getelementptr {pointee}, {pointee}* {arg1}, i16 {arg2}\n",
		["ushort"] = "{1} = zext i16 {arg2} to i64\n{out} = getelementptr {pointee}, {pointee}* {arg1}, i64 {1}\n",
		["byte"] = "{1} = zext i8 {arg2} to i64\n{out} = getelementptr {pointee}, {pointee}* {arg1}, i64 {1}\n"
	}
}

function M.pointerType( llvmPointeeType)
	return utils.template_format( pointerTemplate, {pointee=llvmPointeeType} )
end

return M

