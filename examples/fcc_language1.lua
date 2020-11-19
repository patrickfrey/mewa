return {
	double = {
		def_local = "{out} = alloca double, align 8\n",
		def_global = "{out} = internal global double 0.00000, align 8\n",
		def_global_val = "{out} = internal global double {inp}, align 8\n",
		default = "0.00000",
		llvmtype = "double",
		class = "fp",
		maxvalue = "1.8e+308",
		assign = "store double {arg1}, double* {this}, align 8",
		load = "{out} = load double, double* {inp}, align 8\n",
		advance = {"ulong", "long"},
		conv = {
			["float"] = {fmt="{out} = fpext float {inp} to double\n", weight=0},
			["ulong"] = {fmt="{out} = uitofp i64 {inp} to double\n", weight=0},
			["long"] = {fmt="{out} = sitofp i64 {inp} to double\n", weight=0},
			["uint"] = {fmt="{out} = uitofp i32 {inp} to double\n", weight=0},
			["int"] = {fmt="{out} = sitofp i32 {inp} to double\n", weight=0},
			["ushort"] = {fmt="{out} = uitofp i16 {inp} to double\n", weight=0},
			["short"] = {fmt="{out} = sitofp i16 {inp} to double\n", weight=0},
			["byte"] = {fmt="{out} = uitofp i8 {inp} to double\n", weight=0},
			["bool"] = {fmt="{1} = trunc i8 {inp} to i1\n{2} = zext i1 {1} to i32\n{out} = sitofp i32 {2} to double\n", weight=0}},
		unop = {
			["-"] = "{out} = fneg double {inp}\n"},
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
	float = {
		def_local = "{out} = alloca float, align 4\n",
		def_global = "{out} = internal global float 0.00000, align 4\n",
		def_global_val = "{out} = internal global float {inp}, align 4\n",
		default = "0.00000",
		llvmtype = "float",
		class = "fp",
		maxvalue = "3.402823e+38",
		assign = "store float {arg1}, float* {this}, align 4",
		load = "{out} = load float, float* {inp}, align 4\n",
		advance = {"double", "ulong", "long", "uint", "int"},
		conv = {
			["double"] = {fmt="{out} = fptrunc double {inp} to float\n", weight=0.1},
			["ulong"] = {fmt="{out} = uitofp i64 {inp} to float\n", weight=0.1},
			["long"] = {fmt="{out} = sitofp i64 {inp} to float\n", weight=0.1},
			["uint"] = {fmt="{out} = uitofp i32 {inp} to float\n", weight=0},
			["int"] = {fmt="{out} = sitofp i32 {inp} to float\n", weight=0},
			["ushort"] = {fmt="{out} = uitofp i16 {inp} to float\n", weight=0},
			["short"] = {fmt="{out} = sitofp i16 {inp} to float\n", weight=0},
			["byte"] = {fmt="{out} = uitofp i8 {inp} to float\n", weight=0},
			["bool"] = {fmt="{1} = trunc i8 {inp} to i1\n{2} = zext i1 {1} to i32\n{out} = sitofp i32 {2} to float\n", weight=0}},
		unop = {
			["-"] = "{out} = fneg float {inp}\n"},
		binop = {
			["+"] = "{out} = fadd float {this}, {arg1}\n",
			["-"] = "{out} = fsub float {this}, {arg1}\n",
			["*"] = "{out} = fmul float {this}, {arg1}\n",
			["/"] = "{out} = fdiv float {this}, {arg1}\n",
			["%"] = "{out} = frem float {this}, {arg1}\n"},
		cmpop = {
			["=="] = "{out} = fcmp oeq float {this}, {arg1}\n",
			["!="] = "{out} = fcmp one float {this}, {arg1}\n",
			["<="] = "{out} = fcmp ole float {this}, {arg1}\n",
			["<"] = "{out} = fcmp olt float {this}, {arg1}\n",
			[">="] = "{out} = fcmp oge float {this}, {arg1}\n",
			[">"] = "{out} = fcmp ogt float {this}, {arg1}\n"}
	},
	ulong = {
		def_local = "{out} = alloca i64, align 8\n",
		def_global = "{out} = internal global i64 0, align 8\n",
		def_global_val = "{out} = internal global i64 {inp}, align 8\n",
		default = "0",
		llvmtype = "i64",
		class = "unsigned",
		maxvalue = "18446744073709551616",
		assign = "store i64 {arg1}, i64* {this}, align 8",
		load = "{out} = load i64, i64* {inp}, align 8\n",
		advance = {"double", "long"},
		conv = {
			["double"] = {fmt="{out} = fptoui double {inp} to i64\n", weight=0},
			["float"] = {fmt="{out} = fptoui float {inp} to i64\n", weight=0},
			["long"] = {fmt="{out} = sext i64 {inp} to i64\n", weight=0.1},
			["uint"] = {fmt="{out} = zext i32 {inp} to i64\n", weight=0},
			["int"] = {fmt="{out} = sext i32 {inp} to i64\n", weight=0.1},
			["ushort"] = {fmt="{out} = zext i16 {inp} to i64\n", weight=0},
			["short"] = {fmt="{out} = sext i16 {inp} to i64\n", weight=0.1},
			["byte"] = {fmt="{out} = zext i8 {inp} to i64\n", weight=0},
			["bool"] = {fmt="{out} = zext i1 {inp} to i64\n", weight=0}},
		unop = {
			["!"] = "{out} = xor i64 {inp}, -1\n"},
		binop = {
			["+"] = "{out} = add nuw i64 {this}, {arg1}\n",
			["-"] = "{out} = add nsw i64 {this}, {arg1}\n",
			["*"] = "{out} = mul nsw i64 {this}, {arg1}\n",
			["/"] = "{out} = udiv i64 {this}, {arg1}\n",
			["%"] = "{out} = urem i64 {this}, {arg1}\n",
			["<<"] = "{out} = shl i64 {this}, {arg1}\n",
			[">>"] = "{out} = lshr i64 {this}, {arg1}\n",
			["&"] = "{out} = and i64 {this}, {arg1}\n",
			["|"] = "{out} = or i64 {this}, {arg1}\n",
			["^"] = "{out} = xor i64 {this}, {arg1}\n"},
		cmpop = {
			["=="] = "{out} = icmp eq i64 {this}, {arg1}\n",
			["!="] = "{out} = icmp ne i64 {this}, {arg1}\n",
			["<="] = "{out} = icmp ule i64 {this}, {arg1}\n",
			["<"] = "{out} = icmp ult i64 {this}, {arg1}\n",
			[">="] = "{out} = icmp uge i64 {this}, {arg1}\n",
			[">"] = "{out} = icmp ugt i64 {this}, {arg1}\n"}
	},
	long = {
		def_local = "{out} = alloca i64, align 8\n",
		def_global = "{out} = internal global i64 0, align 8\n",
		def_global_val = "{out} = internal global i64 {inp}, align 8\n",
		default = "0",
		llvmtype = "i64",
		class = "signed",
		maxvalue = "9223372036854775808",
		assign = "store i64 {arg1}, i64* {this}, align 8",
		load = "{out} = load i64, i64* {inp}, align 8\n",
		advance = {"double", "ulong"},
		conv = {
			["double"] = {fmt="{out} = fptosi double {inp} to i64\n", weight=0},
			["float"] = {fmt="{out} = fptosi float {inp} to i64\n", weight=0},
			["ulong"] = {fmt="{out} = zext i64 {inp} to i64\n", weight=0.1},
			["uint"] = {fmt="{out} = zext i32 {inp} to i64\n", weight=0.1},
			["int"] = {fmt="{out} = sext i32 {inp} to i64\n", weight=0},
			["ushort"] = {fmt="{out} = zext i16 {inp} to i64\n", weight=0.1},
			["short"] = {fmt="{out} = sext i16 {inp} to i64\n", weight=0},
			["byte"] = {fmt="{out} = zext i8 {inp} to i64\n", weight=0.1},
			["bool"] = {fmt="{out} = zext i1 {inp} to i64\n", weight=0}},
		unop = {
			["-"] = "{out} = sub i64 0, {inp}\n"},
		binop = {
			["+"] = "{out} = add nsw i64 {this}, {arg1}\n",
			["-"] = "{out} = sub nsw i64 {this}, {arg1}\n",
			["*"] = "{out} = mul nsw i64 {this}, {arg1}\n",
			["/"] = "{out} = sdiv i64 {this}, {arg1}\n",
			["%"] = "{out} = srem i64 {this}, {arg1}\n",
			["<<"] = "{out} = shl nsw i64 {this}, {arg1}\n",
			[">>"] = "{out} = ashr i64 {this}, {arg1}\n"},
		cmpop = {
			["=="] = "{out} = icmp eq i64 {this}, {arg1}\n",
			["!="] = "{out} = icmp ne i64 {this}, {arg1}\n",
			["<="] = "{out} = icmp sle i64 {this}, {arg1}\n",
			["<"] = "{out} = icmp slt i64 {this}, {arg1}\n",
			[">="] = "{out} = icmp sge i64 {this}, {arg1}\n",
			[">"] = "{out} = icmp sgt i64 {this}, {arg1}\n"}
	},
	uint = {
		def_local = "{out} = alloca i32, align 4\n",
		def_global = "{out} = internal global i32 0, align 4\n",
		def_global_val = "{out} = internal global i32 {inp}, align 4\n",
		default = "0",
		llvmtype = "i32",
		class = "unsigned",
		maxvalue = "4294967296",
		assign = "store i32 {arg1}, i32* {this}, align 4",
		load = "{out} = load i32, i32* {inp}, align 4\n",
		advance = {"double", "float", "long", "int"},
		conv = {
			["double"] = {fmt="{out} = fptoui double {inp} to i32\n", weight=0.1},
			["float"] = {fmt="{out} = fptoui float {inp} to i32\n", weight=0},
			["ulong"] = {fmt="{out} = trunc i64 {inp} to i32\n", weight=0.1},
			["long"] = {fmt="{out} = trunc i64 {inp} to i32\n", weight=0.2},
			["int"] = {fmt="{out} = sext i32 {inp} to i32\n", weight=0.1},
			["ushort"] = {fmt="{out} = zext i16 {inp} to i32\n", weight=0},
			["short"] = {fmt="{out} = sext i16 {inp} to i32\n", weight=0.1},
			["byte"] = {fmt="{out} = zext i8 {inp} to i32\n", weight=0},
			["bool"] = {fmt="{out} = zext i1 {inp} to i32\n", weight=0}},
		unop = {
			["!"] = "{out} = xor i32 {inp}, -1\n"},
		binop = {
			["+"] = "{out} = add nuw i32 {this}, {arg1}\n",
			["-"] = "{out} = add nsw i32 {this}, {arg1}\n",
			["*"] = "{out} = mul nsw i32 {this}, {arg1}\n",
			["/"] = "{out} = udiv i32 {this}, {arg1}\n",
			["%"] = "{out} = urem i32 {this}, {arg1}\n",
			["<<"] = "{out} = shl i32 {this}, {arg1}\n",
			[">>"] = "{out} = lshr i32 {this}, {arg1}\n",
			["&"] = "{out} = and i32 {this}, {arg1}\n",
			["|"] = "{out} = or i32 {this}, {arg1}\n",
			["^"] = "{out} = xor i32 {this}, {arg1}\n"},
		cmpop = {
			["=="] = "{out} = icmp eq i32 {this}, {arg1}\n",
			["!="] = "{out} = icmp ne i32 {this}, {arg1}\n",
			["<="] = "{out} = icmp ule i32 {this}, {arg1}\n",
			["<"] = "{out} = icmp ult i32 {this}, {arg1}\n",
			[">="] = "{out} = icmp uge i32 {this}, {arg1}\n",
			[">"] = "{out} = icmp ugt i32 {this}, {arg1}\n"}
	},
	int = {
		def_local = "{out} = alloca i32, align 4\n",
		def_global = "{out} = internal global i32 0, align 4\n",
		def_global_val = "{out} = internal global i32 {inp}, align 4\n",
		default = "0",
		llvmtype = "i32",
		class = "signed",
		maxvalue = "2147483648",
		assign = "store i32 {arg1}, i32* {this}, align 4",
		load = "{out} = load i32, i32* {inp}, align 4\n",
		advance = {"double", "float", "ulong", "uint"},
		conv = {
			["double"] = {fmt="{out} = fptosi double {inp} to i32\n", weight=0.1},
			["float"] = {fmt="{out} = fptosi float {inp} to i32\n", weight=0},
			["ulong"] = {fmt="{out} = trunc i64 {inp} to i32\n", weight=0.2},
			["long"] = {fmt="{out} = trunc i64 {inp} to i32\n", weight=0.1},
			["uint"] = {fmt="{out} = zext i32 {inp} to i32\n", weight=0.1},
			["ushort"] = {fmt="{out} = zext i16 {inp} to i32\n", weight=0.1},
			["short"] = {fmt="{out} = sext i16 {inp} to i32\n", weight=0},
			["byte"] = {fmt="{out} = zext i8 {inp} to i32\n", weight=0.1},
			["bool"] = {fmt="{out} = zext i1 {inp} to i32\n", weight=0}},
		unop = {
			["-"] = "{out} = sub i32 0, {inp}\n"},
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
	ushort = {
		def_local = "{out} = alloca i16, align 2\n",
		def_global = "{out} = internal global i16 0, align 2\n",
		def_global_val = "{out} = internal global i16 {inp}, align 2\n",
		default = "0",
		llvmtype = "i16",
		class = "unsigned",
		maxvalue = "65536",
		assign = "store i16 {arg1}, i16* {this}, align 2",
		load = "{out} = load i16, i16* {inp}, align 2\n",
		advance = {"double", "float", "long", "int", "short"},
		conv = {
			["double"] = {fmt="{out} = fptoui double {inp} to i16\n", weight=0.1},
			["float"] = {fmt="{out} = fptoui float {inp} to i16\n", weight=0.1},
			["ulong"] = {fmt="{out} = trunc i64 {inp} to i16\n", weight=0.1},
			["long"] = {fmt="{out} = trunc i64 {inp} to i16\n", weight=0.2},
			["uint"] = {fmt="{out} = trunc i32 {inp} to i16\n", weight=0.1},
			["int"] = {fmt="{out} = trunc i32 {inp} to i16\n", weight=0.2},
			["short"] = {fmt="{out} = sext i16 {inp} to i16\n", weight=0.1},
			["byte"] = {fmt="{out} = zext i8 {inp} to i16\n", weight=0},
			["bool"] = {fmt="{out} = zext i1 {inp} to i16\n", weight=0}},
		unop = {
			["!"] = "{out} = xor i16 {inp}, -1\n"},
		binop = {
			["+"] = "{out} = add nuw i16 {this}, {arg1}\n",
			["-"] = "{out} = add nsw i16 {this}, {arg1}\n",
			["*"] = "{out} = mul nsw i16 {this}, {arg1}\n",
			["/"] = "{out} = udiv i16 {this}, {arg1}\n",
			["%"] = "{out} = urem i16 {this}, {arg1}\n",
			["<<"] = "{out} = shl i16 {this}, {arg1}\n",
			[">>"] = "{out} = lshr i16 {this}, {arg1}\n",
			["&"] = "{out} = and i16 {this}, {arg1}\n",
			["|"] = "{out} = or i16 {this}, {arg1}\n",
			["^"] = "{out} = xor i16 {this}, {arg1}\n"},
		cmpop = {
			["=="] = "{out} = icmp eq i16 {this}, {arg1}\n",
			["!="] = "{out} = icmp ne i16 {this}, {arg1}\n",
			["<="] = "{out} = icmp ule i16 {this}, {arg1}\n",
			["<"] = "{out} = icmp ult i16 {this}, {arg1}\n",
			[">="] = "{out} = icmp uge i16 {this}, {arg1}\n",
			[">"] = "{out} = icmp ugt i16 {this}, {arg1}\n"}
	},
	short = {
		def_local = "{out} = alloca i16, align 2\n",
		def_global = "{out} = internal global i16 0, align 2\n",
		def_global_val = "{out} = internal global i16 {inp}, align 2\n",
		default = "0",
		llvmtype = "i16",
		class = "signed",
		maxvalue = "32768",
		assign = "store i16 {arg1}, i16* {this}, align 2",
		load = "{out} = load i16, i16* {inp}, align 2\n",
		advance = {"double", "float", "ulong", "uint", "ushort"},
		conv = {
			["double"] = {fmt="{out} = fptosi double {inp} to i16\n", weight=0.1},
			["float"] = {fmt="{out} = fptosi float {inp} to i16\n", weight=0.1},
			["ulong"] = {fmt="{out} = trunc i64 {inp} to i16\n", weight=0.2},
			["long"] = {fmt="{out} = trunc i64 {inp} to i16\n", weight=0.1},
			["uint"] = {fmt="{out} = trunc i32 {inp} to i16\n", weight=0.2},
			["int"] = {fmt="{out} = trunc i32 {inp} to i16\n", weight=0.1},
			["ushort"] = {fmt="{out} = zext i16 {inp} to i16\n", weight=0.1},
			["byte"] = {fmt="{out} = zext i8 {inp} to i16\n", weight=0.1},
			["bool"] = {fmt="{out} = zext i1 {inp} to i16\n", weight=0}},
		unop = {
			["-"] = "{out} = sub i16 0, {inp}\n"},
		binop = {
			["+"] = "{out} = add nsw i16 {this}, {arg1}\n",
			["-"] = "{out} = sub nsw i16 {this}, {arg1}\n",
			["*"] = "{out} = mul nsw i16 {this}, {arg1}\n",
			["/"] = "{out} = sdiv i16 {this}, {arg1}\n",
			["%"] = "{out} = srem i16 {this}, {arg1}\n",
			["<<"] = "{out} = shl nsw i16 {this}, {arg1}\n",
			[">>"] = "{out} = ashr i16 {this}, {arg1}\n"},
		cmpop = {
			["=="] = "{out} = icmp eq i16 {this}, {arg1}\n",
			["!="] = "{out} = icmp ne i16 {this}, {arg1}\n",
			["<="] = "{out} = icmp sle i16 {this}, {arg1}\n",
			["<"] = "{out} = icmp slt i16 {this}, {arg1}\n",
			[">="] = "{out} = icmp sge i16 {this}, {arg1}\n",
			[">"] = "{out} = icmp sgt i16 {this}, {arg1}\n"}
	},
	byte = {
		def_local = "{out} = alloca i8, align 1\n",
		def_global = "{out} = internal global i8 0, align 1\n",
		def_global_val = "{out} = internal global i8 {inp}, align 1\n",
		default = "0",
		llvmtype = "i8",
		class = "unsigned",
		maxvalue = "256",
		assign = "store i8 {arg1}, i8* {this}, align 1",
		load = "{out} = load i8, i8* {inp}, align 1\n",
		advance = {"double", "float", "long", "int", "short", "bool"},
		conv = {
			["double"] = {fmt="{out} = fptoui double {inp} to i8\n", weight=0.1},
			["float"] = {fmt="{out} = fptoui float {inp} to i8\n", weight=0.1},
			["ulong"] = {fmt="{out} = trunc i64 {inp} to i8\n", weight=0.1},
			["long"] = {fmt="{out} = trunc i64 {inp} to i8\n", weight=0.2},
			["uint"] = {fmt="{out} = trunc i32 {inp} to i8\n", weight=0.1},
			["int"] = {fmt="{out} = trunc i32 {inp} to i8\n", weight=0.2},
			["ushort"] = {fmt="{out} = trunc i16 {inp} to i8\n", weight=0.1},
			["short"] = {fmt="{out} = trunc i16 {inp} to i8\n", weight=0.2},
			["bool"] = {fmt="{out} = zext i1 {inp} to i8\n", weight=0}},
		unop = {
			["!"] = "{out} = xor i8 {inp}, -1\n"},
		binop = {
			["+"] = "{out} = add nuw i8 {this}, {arg1}\n",
			["-"] = "{out} = add nsw i8 {this}, {arg1}\n",
			["*"] = "{out} = mul nsw i8 {this}, {arg1}\n",
			["/"] = "{out} = udiv i8 {this}, {arg1}\n",
			["%"] = "{out} = urem i8 {this}, {arg1}\n",
			["<<"] = "{out} = shl i8 {this}, {arg1}\n",
			[">>"] = "{out} = lshr i8 {this}, {arg1}\n",
			["&"] = "{out} = and i8 {this}, {arg1}\n",
			["|"] = "{out} = or i8 {this}, {arg1}\n",
			["^"] = "{out} = xor i8 {this}, {arg1}\n"},
		cmpop = {
			["=="] = "{out} = icmp eq i8 {this}, {arg1}\n",
			["!="] = "{out} = icmp ne i8 {this}, {arg1}\n",
			["<="] = "{out} = icmp ule i8 {this}, {arg1}\n",
			["<"] = "{out} = icmp ult i8 {this}, {arg1}\n",
			[">="] = "{out} = icmp uge i8 {this}, {arg1}\n",
			[">"] = "{out} = icmp ugt i8 {this}, {arg1}\n"}
	},
	bool = {
		def_local = "{out} = alloca i8, align 1\n",
		def_global = "{out} = internal global i8 false, align 1\n",
		def_global_val = "{out} = internal global i8 {inp}, align 1\n",
		default = "false",
		llvmtype = "i1",
		class = "bool",
		maxvalue = "1",
		assign = "{1} = zext i1 {arg1} to i8\nstore i8 {1}, i8* {this}, align 1\n",
		load = "{1} = load i8, i8* {inp}, align 1\n{out} = trunc i8 {1} to i1\n",
		advance = {},
		conv = {
			["double"] = {fmt="{out} = fcmp une double {inp}, 0.000000e+00\n", weight=0.1},
			["float"] = {fmt="{out} = fcmp une float {inp}, 0.000000e+00\n", weight=0.1},
			["ulong"] = {fmt="{out} = cmp ne i64 {inp}, 0\n", weight=0.1},
			["long"] = {fmt="{out} = cmp ne i64 {inp}, 0\n", weight=0.1},
			["uint"] = {fmt="{out} = cmp ne i32 {inp}, 0\n", weight=0.1},
			["int"] = {fmt="{out} = cmp ne i32 {inp}, 0\n", weight=0.1},
			["ushort"] = {fmt="{out} = cmp ne i16 {inp}, 0\n", weight=0.1},
			["short"] = {fmt="{out} = cmp ne i16 {inp}, 0\n", weight=0.1},
			["byte"] = {fmt="{out} = cmp ne i8 {inp}, 0\n", weight=0}},
		unop = {
			["!"] = "{out} = xor i1 {inp}, true\n"},
		binop = {
			["&&"] = "{out} = and i1 {this}, {arg1}\n",
			["||"] = "{out} = or i1 {this}, {arg1}\n"},
		cmpop = {
			["=="] = "{out} = icmp eq i1 {this}, {arg1}\n",
			["!="] = "{out} = icmp ne i1 {this}, {arg1}\n",
			["<="] = "{out} = icmp ule i1 {this}, {arg1}\n",
			["<"] = "{out} = icmp ult i1 {this}, {arg1}\n",
			[">="] = "{out} = icmp uge i1 {this}, {arg1}\n",
			[">"] = "{out} = icmp ugt i1 {this}, {arg1}\n"}
	}
}
