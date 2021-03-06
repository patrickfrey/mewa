-- WARNING !!! GENERATED FILE !!! DO NOT EDIT
-- ---------------------------------------------------------------------------------------------------------------------------------
-- This file has been generated by the perl script call "examples/gen/generateScalarLlvmir.pl examples/language1/scalar_types.txt"
-- ---------------------------------------------------------------------------------------------------------------------------------
return {
	double = {
		def_local = "{out} = alloca double, align 8\n",
		def_global = "{out} = internal global double 0.00000, align 8\n",
		def_global_val = "{out} = internal global double {val}, align 8\n",
		default = "0.00000",
		llvmtype = "double",
		scalar = true,
		class = "fp",
		size = 8,
		align = 8,
		sizeweight = 0.4,
		maxvalue = "1.8e+308",
		assign = "store double {arg1}, double* {this}\n",
		ctor_init = "store double 0.00000, double* {this}\n",
		ctor_copy = "{1} = load double, double* {arg1}\nstore double {1}, double* {this}\n",
		load = "{out} = load double, double* {this}\n",
		promote = {},
		conv = {
			["int"] = {fmt="{out} = sitofp i32 {this} to double\n", weight=0.25},
			["bool"] = {fmt="{1} = trunc i8 {this} to i1\n{2} = zext i1 {1} to i32\n{out} = sitofp i32 {2} to double\n", weight=0.25}},
		unop = {
			["-"] = "{out} = fneg double {this}\n"},
		binop = {
			["+"] = "{out} = fadd double {this}, {arg1}\n",
			["-"] = "{out} = fsub double {this}, {arg1}\n",
			["*"] = "{out} = fmul double {this}, {arg1}\n",
			["/"] = "{out} = fdiv double {this}, {arg1}\n",
			["%"] = "{out} = frem double {this}, {arg1}\n"},
		cmpop = {
			["=="] = "{out} = fcmp oeq double {this}, {arg1}\n",
			["!="] = "{out} = fcmp one double {this}, {arg1}\n",
			["<="] = "{out} = fcmp ole double {this}, {arg1}\n",
			["<"] = "{out} = fcmp olt double {this}, {arg1}\n",
			[">="] = "{out} = fcmp oge double {this}, {arg1}\n",
			[">"] = "{out} = fcmp ogt double {this}, {arg1}\n"}
	},
	int = {
		def_local = "{out} = alloca i32, align 4\n",
		def_global = "{out} = internal global i32 0, align 4\n",
		def_global_val = "{out} = internal global i32 {val}, align 4\n",
		default = "0",
		llvmtype = "i32",
		scalar = true,
		class = "signed",
		size = 4,
		align = 4,
		sizeweight = 0.25,
		maxvalue = "2147483648",
		assign = "store i32 {arg1}, i32* {this}\n",
		ctor_init = "store i32 0, i32* {this}\n",
		ctor_copy = "{1} = load i32, i32* {arg1}\nstore i32 {1}, i32* {this}\n",
		load = "{out} = load i32, i32* {this}\n",
		promote = {"double"},
		conv = {
			["double"] = {fmt="{out} = fptosi double {this} to i32\n", weight=0.375},
			["bool"] = {fmt="{out} = zext i1 {this} to i32\n", weight=0.25}},
		unop = {
			["-"] = "{out} = sub i32 0, {this}\n"},
		binop = {
			["+"] = "{out} = add nsw i32 {this}, {arg1}\n",
			["-"] = "{out} = sub nsw i32 {this}, {arg1}\n",
			["*"] = "{out} = mul nsw i32 {this}, {arg1}\n",
			["/"] = "{out} = sdiv i32 {this}, {arg1}\n",
			["%"] = "{out} = srem i32 {this}, {arg1}\n",
			["<<"] = "{out} = shl nsw i32 {this}, {arg1}\n",
			[">>"] = "{out} = ashr i32 {this}, {arg1}\n"},
		cmpop = {
			["=="] = "{out} = icmp eq i32 {this}, {arg1}\n",
			["!="] = "{out} = icmp ne i32 {this}, {arg1}\n",
			["<="] = "{out} = icmp sle i32 {this}, {arg1}\n",
			["<"] = "{out} = icmp slt i32 {this}, {arg1}\n",
			[">="] = "{out} = icmp sge i32 {this}, {arg1}\n",
			[">"] = "{out} = icmp sgt i32 {this}, {arg1}\n"}
	},
	bool = {
		def_local = "{out} = alloca i8, align 1\n",
		def_global = "{out} = internal global i8 false, align 1\n",
		def_global_val = "{out} = internal global i8 {val}, align 1\n",
		default = "false",
		llvmtype = "i1",
		scalar = true,
		class = "bool",
		size = 1,
		align = 1,
		sizeweight = 0.125,
		maxvalue = "1",
		assign = "{1} = zext i1 {arg1} to i8\nstore i8 {1}, i8* {this}\n",
		ctor_init = "store i1 false, i1* {this}\n",
		ctor_copy = "{1} = load i8, i8* {arg1}\nstore i8 {1}, i8* {this}\n",
		load = "{1} = load i8, i8* {this}\n{out} = trunc i8 {1} to i1\n",
		promote = {"double", "int"},
		conv = {
			["double"] = {fmt="{out} = fcmp une double {this}, 0.00000\n", weight=0.375},
			["int"] = {fmt="{out} = icmp ne i32 {this}, 0\n", weight=0.375}},
		unop = {
			["!"] = "{out} = xor i1 {this}, true\n"},
		binop = {},
		cmpop = {
			["=="] = "{out} = icmp eq i1 {this}, {arg1}\n",
			["!="] = "{out} = icmp ne i1 {this}, {arg1}\n",
			["<="] = "{out} = icmp ule i1 {this}, {arg1}\n",
			["<"] = "{out} = icmp ult i1 {this}, {arg1}\n",
			[">="] = "{out} = icmp uge i1 {this}, {arg1}\n",
			[">"] = "{out} = icmp ugt i1 {this}, {arg1}\n"}
	}
}
