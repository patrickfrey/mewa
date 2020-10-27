return {
constructor = {
	-- /IN variable address (@name) or register (%id)
	-- /OUT output register (%id)
	-- /ADR variable address (@name) or register (%id)
	-- /VAL constant value of a matching LLVM type

	double = {
		def_local  = { { "/OUT", " = alloca double, align 8" }},
		def_global = { { "/ADR", " = internal global double ", "/VAL", ", align 8" }},
		default = "0.00000",
		store = { {"store double ", "/IN", ", double* ", "/ADR", ", align 8"}},
		load = { {"/OUT", " = load double, double* ", "/IN", ", align 8"}},
		conv = {
			_double = { {"/R1", " = load double, double* ", "/IN", ", align 8"}},
			_float = { {"/R1", " = load float, float* ", "/IN", ", align 4"}, {"/OUT", " = fpext float ", "/R1", " to double"}},
			_ulong = { {"/R1", " = load i64, i64* ", "/IN", ", align 8"}, {"/OUT", " = uitofp i64 ", "/R1", " to double"}},
			_long = { {"/R1", " = load i64, i64* ", "/IN", ", align 8"}, {"/OUT", " = sitofp i64 ", "/R1", " to double"}},
			_uint = { {"/R1", " = load i32, i32* ", "/IN", ", align 4"}, {"/OUT", " = uitofp i32 ", "/R1", " to double"}},
			_int = { {"/R1", " = load i32, i32* ", "/IN", ", align 4"}, {"/OUT", " = sitofp i32 ", "/R1", " to double"}},
			_ushort = { {"/R1", " = load i16, i16* ", "/IN", ", align 2"}, {"/OUT", " = uitofp i16 ", "/R1", " to double"}},
			_short = { {"/R1", " = load i16, i16* ", "/IN", ", align 2"}, {"/OUT", " = sitofp i16 ", "/R1", " to double"}},
			_byte = { {"/R1", " = load i8, i8* ", "/IN", ", align 1"}, {"/OUT", " = uitofp i8 ", "/R1", " to double"}},
			_bool = { {"/R1", " = load i8, i8* ", "/IN", ", align 1"}, {"/R1", " = trunc i8 ", "/R1", " to i1"}, {"/R2", " = zext i1 ", "/R1", " to i32"},{"/OUT", " = sitofp i32 ", "/R2", " to double"}}
		},
		unop = {
			_neg = { "/OUT", " = fneg double 0, ", "/IN" }
		},
		binop = {
			_add = { "/OUT", " = fadd double, ", "/IN" },
			_sub = { "/OUT", " = fsub double, ", "/IN" },
			_mul = { "/OUT", " = fmul double, ", "/IN" },
			_div = { "/OUT", " = fdiv double, ", "/IN" },
			_mod = { "/OUT", " = frem double, ", "/IN" }
		}
	},
	float = {
		def_local  = { { "/OUT", " = alloca float, align 4" }},
		def_global = { { "/ADR", " = internal global float ", "/VAL", ", align 4" }},
		default = "0.00000",
		store = { {"store float ", "/IN", ", float* ", "/ADR", ", align 4"}},
		load = { {"/OUT", " = load float, float* ", "/IN", ", align 4"}},
		conv = {
			_double = { {"/R1", " = load double, double* ", "/IN", ", align 8"}, {"/OUT", " = fptrunc double ", "/R1", " to float"}},
			_float = { {"/R1", " = load float, float* ", "/IN", ", align 4"}},
			_ulong = { {"/R1", " = load i64, i64* ", "/IN", ", align 8"}, {"/OUT", " = uitofp i64 ", "/R1", " to float"}},
			_long = { {"/R1", " = load i64, i64* ", "/IN", ", align 8"}, {"/OUT", " = sitofp i64 ", "/R1", " to float"}},
			_uint = { {"/R1", " = load i32, i32* ", "/IN", ", align 4"}, {"/OUT", " = uitofp i32 ", "/R1", " to float"}},
			_int = { {"/R1", " = load i32, i32* ", "/IN", ", align 4"}, {"/OUT", " = sitofp i32 ", "/R1", " to float"}},
			_ushort = { {"/R1", " = load i16, i16* ", "/IN", ", align 2"}, {"/OUT", " = uitofp i16 ", "/R1", " to float"}},
			_short = { {"/R1", " = load i16, i16* ", "/IN", ", align 2"}, {"/OUT", " = sitofp i16 ", "/R1", " to float"}},
			_byte = { {"/R1", " = load i8, i8* ", "/IN", ", align 1"}, {"/OUT", " = uitofp i8 ", "/R1", " to float"}},
			_bool = { {"/R1", " = load i8, i8* ", "/IN", ", align 1"}, {"/R1", " = trunc i8 ", "/R1", " to i1"}, {"/R2", " = zext i1 ", "/R1", " to i32"},{"/OUT", " = sitofp i32 ", "/R2", " to float"}}
		},
		unop = {
			_neg = { "/OUT", " = fneg float 0, ", "/IN" }
		},
		binop = {
			_add = { "/OUT", " = fadd float, ", "/IN" },
			_sub = { "/OUT", " = fsub float, ", "/IN" },
			_mul = { "/OUT", " = fmul float, ", "/IN" },
			_div = { "/OUT", " = fdiv float, ", "/IN" },
			_mod = { "/OUT", " = frem float, ", "/IN" }
		}
	},
	ulong = {
		def_local  = { { "/OUT", " = alloca i64, align 8" }},
		def_global = { { "/ADR", " = internal global i64 ", "/VAL", ", align 8" }},
		default = "0",
		store = { {"store i64 ", "/IN", ", i64* ", "/ADR", ", align 8"}},
		load = { {"/OUT", " = load i64, i64* ", "/IN", ", align 8"}},
		conv = {
			_double = { {"/R1", " = load double, double* ", "/IN", ", align 8"}, {"/OUT", " = fptoui double ", "/R1", " to i64"}},
			_float = { {"/R1", " = load float, float* ", "/IN", ", align 4"}, {"/OUT", " = fptoui float ", "/R1", " to i64"}},
			_ulong = { {"/R1", " = load i64, i64* ", "/IN", ", align 8"}},
			_long = { {"/R1", " = load i64, i64* ", "/IN", ", align 8"}},
			_uint = { {"/R1", " = load i32, i32* ", "/IN", ", align 4"}, {"/OUT", " = zext i32 ", "/R1", " to i64"}},
			_int = { {"/R1", " = load i32, i32* ", "/IN", ", align 4"}, {"/OUT", " = sext i32 ", "/R1", " to i64"}},
			_ushort = { {"/R1", " = load i16, i16* ", "/IN", ", align 2"}, {"/OUT", " = zext i16 ", "/R1", " to i64"}},
			_short = { {"/R1", " = load i16, i16* ", "/IN", ", align 2"}, {"/OUT", " = sext i16 ", "/R1", " to i64"}},
			_byte = { {"/R1", " = load i8, i8* ", "/IN", ", align 1"}, {"/OUT", " = zext i8 ", "/R1", " to i64"}},
			_bool = { {"/R1", " = load i8, i8* ", "/IN", ", align 1"}, {"/R1", " = trunc i8 ", "/R1", " to i1"}, {"/OUT", " = zext i1 ", "/R1", " to i64"}}
		},
		unop = {
			_not = { "/OUT", " = xor i64 ", "/IN", ", -1" }
		},
		binop = {
			_add = { "/OUT", " = add nuw i64, ", "/IN" },
			_sub = { "/OUT", " = add nsw i64, ", "/IN" },
			_mul = { "/OUT", " = mul nsw i64, ", "/IN" },
			_div = { "/OUT", " = udiv i64, ", "/IN" },
			_mod = { "/OUT", " = urem i64, ", "/IN" },
			_shl = { "/OUT", " = shl i64, ", "/IN" },
			_shr = { "/OUT", " = lshr i64, ", "/IN" },
			_and = { "/OUT", " = and i64, ", "/IN" },
			_or = { "/OUT", " = or i64, ", "/IN" },
			_xor = { "/OUT", " = xor i64, ", "/IN" }
		}
	},
	long = {
		def_local  = { { "/OUT", " = alloca i64, align 8" }},
		def_global = { { "/ADR", " = internal global i64 ", "/VAL", ", align 8" }},
		default = "0",
		store = { {"store i64 ", "/IN", ", i64* ", "/ADR", ", align 8"}},
		load = { {"/OUT", " = load i64, i64* ", "/IN", ", align 8"}},
		conv = {
			_double = { {"/R1", " = load double, double* ", "/IN", ", align 8"}, {"/OUT", " = fptosi double ", "/R1", " to i64"}},
			_float = { {"/R1", " = load float, float* ", "/IN", ", align 4"}, {"/OUT", " = fptosi float ", "/R1", " to i64"}},
			_ulong = { {"/R1", " = load i64, i64* ", "/IN", ", align 8"}},
			_long = { {"/R1", " = load i64, i64* ", "/IN", ", align 8"}},
			_uint = { {"/R1", " = load i32, i32* ", "/IN", ", align 4"}, {"/OUT", " = zext i32 ", "/R1", " to i64"}},
			_int = { {"/R1", " = load i32, i32* ", "/IN", ", align 4"}, {"/OUT", " = sext i32 ", "/R1", " to i64"}},
			_ushort = { {"/R1", " = load i16, i16* ", "/IN", ", align 2"}, {"/OUT", " = zext i16 ", "/R1", " to i64"}},
			_short = { {"/R1", " = load i16, i16* ", "/IN", ", align 2"}, {"/OUT", " = sext i16 ", "/R1", " to i64"}},
			_byte = { {"/R1", " = load i8, i8* ", "/IN", ", align 1"}, {"/OUT", " = zext i8 ", "/R1", " to i64"}},
			_bool = { {"/R1", " = load i8, i8* ", "/IN", ", align 1"}, {"/R1", " = trunc i8 ", "/R1", " to i1"}, {"/OUT", " = zext i1 ", "/R1", " to i64"}}
		},
		unop = {
			_neg = { "/OUT", " = sub i64 0, ", "/IN" }
		},
		binop = {
			_add = { "/OUT", " = add nsw i64, ", "/IN" },
			_sub = { "/OUT", " = sub nsw i64, ", "/IN" },
			_mul = { "/OUT", " = mul nsw i64, ", "/IN" },
			_div = { "/OUT", " = sdiv i64, ", "/IN" },
			_mod = { "/OUT", " = srem i64, ", "/IN" },
			_shl = { "/OUT", " = shl nsw i64, ", "/IN" },
			_shr = { "/OUT", " = ashr i64, ", "/IN" }
		}
	},
	uint = {
		def_local  = { { "/OUT", " = alloca i32, align 4" }},
		def_global = { { "/ADR", " = internal global i32 ", "/VAL", ", align 4" }},
		default = "0",
		store = { {"store i32 ", "/IN", ", i32* ", "/ADR", ", align 4"}},
		load = { {"/OUT", " = load i32, i32* ", "/IN", ", align 4"}},
		conv = {
			_double = { {"/R1", " = load double, double* ", "/IN", ", align 8"}, {"/OUT", " = fptoui double ", "/R1", " to i32"}},
			_float = { {"/R1", " = load float, float* ", "/IN", ", align 4"}, {"/OUT", " = fptoui float ", "/R1", " to i32"}},
			_ulong = { {"/R1", " = load i64, i64* ", "/IN", ", align 8"}, {"/OUT", " = trunc i64 ", "/R1", " to i32"}},
			_long = { {"/R1", " = load i64, i64* ", "/IN", ", align 8"}, {"/OUT", " = trunc i64 ", "/R1", " to i32"}},
			_uint = { {"/R1", " = load i32, i32* ", "/IN", ", align 4"}},
			_int = { {"/R1", " = load i32, i32* ", "/IN", ", align 4"}},
			_ushort = { {"/R1", " = load i16, i16* ", "/IN", ", align 2"}, {"/OUT", " = zext i16 ", "/R1", " to i32"}},
			_short = { {"/R1", " = load i16, i16* ", "/IN", ", align 2"}, {"/OUT", " = sext i16 ", "/R1", " to i32"}},
			_byte = { {"/R1", " = load i8, i8* ", "/IN", ", align 1"}, {"/OUT", " = zext i8 ", "/R1", " to i32"}},
			_bool = { {"/R1", " = load i8, i8* ", "/IN", ", align 1"}, {"/R1", " = trunc i8 ", "/R1", " to i1"}, {"/OUT", " = zext i1 ", "/R1", " to i32"}}
		},
		unop = {
			_not = { "/OUT", " = xor i32 ", "/IN", ", -1" }
		},
		binop = {
			_add = { "/OUT", " = add nuw i32, ", "/IN" },
			_sub = { "/OUT", " = add nsw i32, ", "/IN" },
			_mul = { "/OUT", " = mul nsw i32, ", "/IN" },
			_div = { "/OUT", " = udiv i32, ", "/IN" },
			_mod = { "/OUT", " = urem i32, ", "/IN" },
			_shl = { "/OUT", " = shl i32, ", "/IN" },
			_shr = { "/OUT", " = lshr i32, ", "/IN" },
			_and = { "/OUT", " = and i32, ", "/IN" },
			_or = { "/OUT", " = or i32, ", "/IN" },
			_xor = { "/OUT", " = xor i32, ", "/IN" }
		}
	},
	int = {
		def_local  = { { "/OUT", " = alloca i32, align 4" }},
		def_global = { { "/ADR", " = internal global i32 ", "/VAL", ", align 4" }},
		default = "0",
		store = { {"store i32 ", "/IN", ", i32* ", "/ADR", ", align 4"}},
		load = { {"/OUT", " = load i32, i32* ", "/IN", ", align 4"}},
		conv = {
			_double = { {"/R1", " = load double, double* ", "/IN", ", align 8"}, {"/OUT", " = fptosi double ", "/R1", " to i32"}},
			_float = { {"/R1", " = load float, float* ", "/IN", ", align 4"}, {"/OUT", " = fptosi float ", "/R1", " to i32"}},
			_ulong = { {"/R1", " = load i64, i64* ", "/IN", ", align 8"}, {"/OUT", " = trunc i64 ", "/R1", " to i32"}},
			_long = { {"/R1", " = load i64, i64* ", "/IN", ", align 8"}, {"/OUT", " = trunc i64 ", "/R1", " to i32"}},
			_uint = { {"/R1", " = load i32, i32* ", "/IN", ", align 4"}},
			_int = { {"/R1", " = load i32, i32* ", "/IN", ", align 4"}},
			_ushort = { {"/R1", " = load i16, i16* ", "/IN", ", align 2"}, {"/OUT", " = zext i16 ", "/R1", " to i32"}},
			_short = { {"/R1", " = load i16, i16* ", "/IN", ", align 2"}, {"/OUT", " = sext i16 ", "/R1", " to i32"}},
			_byte = { {"/R1", " = load i8, i8* ", "/IN", ", align 1"}, {"/OUT", " = zext i8 ", "/R1", " to i32"}},
			_bool = { {"/R1", " = load i8, i8* ", "/IN", ", align 1"}, {"/R1", " = trunc i8 ", "/R1", " to i1"}, {"/OUT", " = zext i1 ", "/R1", " to i32"}}
		},
		unop = {
			_neg = { "/OUT", " = sub i32 0, ", "/IN" }
		},
		binop = {
			_add = { "/OUT", " = add nsw i32, ", "/IN" },
			_sub = { "/OUT", " = sub nsw i32, ", "/IN" },
			_mul = { "/OUT", " = mul nsw i32, ", "/IN" },
			_div = { "/OUT", " = sdiv i32, ", "/IN" },
			_mod = { "/OUT", " = srem i32, ", "/IN" },
			_shl = { "/OUT", " = shl nsw i32, ", "/IN" },
			_shr = { "/OUT", " = ashr i32, ", "/IN" }
		}
	},
	ushort = {
		def_local  = { { "/OUT", " = alloca i16, align 2" }},
		def_global = { { "/ADR", " = internal global i16 ", "/VAL", ", align 2" }},
		default = "0",
		store = { {"store i16 ", "/IN", ", i16* ", "/ADR", ", align 2"}},
		load = { {"/OUT", " = load i16, i16* ", "/IN", ", align 2"}},
		conv = {
			_double = { {"/R1", " = load double, double* ", "/IN", ", align 8"}, {"/OUT", " = fptoui double ", "/R1", " to i16"}},
			_float = { {"/R1", " = load float, float* ", "/IN", ", align 4"}, {"/OUT", " = fptoui float ", "/R1", " to i16"}},
			_ulong = { {"/R1", " = load i64, i64* ", "/IN", ", align 8"}, {"/OUT", " = trunc i64 ", "/R1", " to i16"}},
			_long = { {"/R1", " = load i64, i64* ", "/IN", ", align 8"}, {"/OUT", " = trunc i64 ", "/R1", " to i16"}},
			_uint = { {"/R1", " = load i32, i32* ", "/IN", ", align 4"}, {"/OUT", " = trunc i32 ", "/R1", " to i16"}},
			_int = { {"/R1", " = load i32, i32* ", "/IN", ", align 4"}, {"/OUT", " = trunc i32 ", "/R1", " to i16"}},
			_ushort = { {"/R1", " = load i16, i16* ", "/IN", ", align 2"}},
			_short = { {"/R1", " = load i16, i16* ", "/IN", ", align 2"}},
			_byte = { {"/R1", " = load i8, i8* ", "/IN", ", align 1"}, {"/OUT", " = zext i8 ", "/R1", " to i16"}},
			_bool = { {"/R1", " = load i8, i8* ", "/IN", ", align 1"}, {"/R1", " = trunc i8 ", "/R1", " to i1"}, {"/OUT", " = zext i1 ", "/R1", " to i16"}}
		},
		unop = {
			_not = { "/OUT", " = xor i16 ", "/IN", ", -1" }
		},
		binop = {
			_add = { "/OUT", " = add nuw i16, ", "/IN" },
			_sub = { "/OUT", " = add nsw i16, ", "/IN" },
			_mul = { "/OUT", " = mul nsw i16, ", "/IN" },
			_div = { "/OUT", " = udiv i16, ", "/IN" },
			_mod = { "/OUT", " = urem i16, ", "/IN" },
			_shl = { "/OUT", " = shl i16, ", "/IN" },
			_shr = { "/OUT", " = lshr i16, ", "/IN" },
			_and = { "/OUT", " = and i16, ", "/IN" },
			_or = { "/OUT", " = or i16, ", "/IN" },
			_xor = { "/OUT", " = xor i16, ", "/IN" }
		}
	},
	short = {
		def_local  = { { "/OUT", " = alloca i16, align 2" }},
		def_global = { { "/ADR", " = internal global i16 ", "/VAL", ", align 2" }},
		default = "0",
		store = { {"store i16 ", "/IN", ", i16* ", "/ADR", ", align 2"}},
		load = { {"/OUT", " = load i16, i16* ", "/IN", ", align 2"}},
		conv = {
			_double = { {"/R1", " = load double, double* ", "/IN", ", align 8"}, {"/OUT", " = fptosi double ", "/R1", " to i16"}},
			_float = { {"/R1", " = load float, float* ", "/IN", ", align 4"}, {"/OUT", " = fptosi float ", "/R1", " to i16"}},
			_ulong = { {"/R1", " = load i64, i64* ", "/IN", ", align 8"}, {"/OUT", " = trunc i64 ", "/R1", " to i16"}},
			_long = { {"/R1", " = load i64, i64* ", "/IN", ", align 8"}, {"/OUT", " = trunc i64 ", "/R1", " to i16"}},
			_uint = { {"/R1", " = load i32, i32* ", "/IN", ", align 4"}, {"/OUT", " = trunc i32 ", "/R1", " to i16"}},
			_int = { {"/R1", " = load i32, i32* ", "/IN", ", align 4"}, {"/OUT", " = trunc i32 ", "/R1", " to i16"}},
			_ushort = { {"/R1", " = load i16, i16* ", "/IN", ", align 2"}},
			_short = { {"/R1", " = load i16, i16* ", "/IN", ", align 2"}},
			_byte = { {"/R1", " = load i8, i8* ", "/IN", ", align 1"}, {"/OUT", " = zext i8 ", "/R1", " to i16"}},
			_bool = { {"/R1", " = load i8, i8* ", "/IN", ", align 1"}, {"/R1", " = trunc i8 ", "/R1", " to i1"}, {"/OUT", " = zext i1 ", "/R1", " to i16"}}
		},
		unop = {
			_neg = { "/OUT", " = sub i16 0, ", "/IN" }
		},
		binop = {
			_add = { "/OUT", " = add nsw i16, ", "/IN" },
			_sub = { "/OUT", " = sub nsw i16, ", "/IN" },
			_mul = { "/OUT", " = mul nsw i16, ", "/IN" },
			_div = { "/OUT", " = sdiv i16, ", "/IN" },
			_mod = { "/OUT", " = srem i16, ", "/IN" },
			_shl = { "/OUT", " = shl nsw i16, ", "/IN" },
			_shr = { "/OUT", " = ashr i16, ", "/IN" }
		}
	},
	byte = {
		def_local  = { { "/OUT", " = alloca i8, align 1" }},
		def_global = { { "/ADR", " = internal global i8 ", "/VAL", ", align 1" }},
		default = "0",
		store = { {"store i8 ", "/IN", ", i8* ", "/ADR", ", align 1"}},
		load = { {"/OUT", " = load i8, i8* ", "/IN", ", align 1"}},
		conv = {
			_double = { {"/R1", " = load double, double* ", "/IN", ", align 8"}, {"/OUT", " = fptoui double ", "/R1", " to i8"}},
			_float = { {"/R1", " = load float, float* ", "/IN", ", align 4"}, {"/OUT", " = fptoui float ", "/R1", " to i8"}},
			_ulong = { {"/R1", " = load i64, i64* ", "/IN", ", align 8"}, {"/OUT", " = trunc i64 ", "/R1", " to i8"}},
			_long = { {"/R1", " = load i64, i64* ", "/IN", ", align 8"}, {"/OUT", " = trunc i64 ", "/R1", " to i8"}},
			_uint = { {"/R1", " = load i32, i32* ", "/IN", ", align 4"}, {"/OUT", " = trunc i32 ", "/R1", " to i8"}},
			_int = { {"/R1", " = load i32, i32* ", "/IN", ", align 4"}, {"/OUT", " = trunc i32 ", "/R1", " to i8"}},
			_ushort = { {"/R1", " = load i16, i16* ", "/IN", ", align 2"}, {"/OUT", " = trunc i16 ", "/R1", " to i8"}},
			_short = { {"/R1", " = load i16, i16* ", "/IN", ", align 2"}, {"/OUT", " = trunc i16 ", "/R1", " to i8"}},
			_byte = { {"/R1", " = load i8, i8* ", "/IN", ", align 1"}},
			_bool = { {"/R1", " = load i8, i8* ", "/IN", ", align 1"}, {"/R1", " = trunc i8 ", "/R1", " to i1"}, {"/OUT", " = zext i1 ", "/R1", " to i8"}}
		},
		unop = {
			_not = { "/OUT", " = xor i8 ", "/IN", ", -1" }
		},
		binop = {
			_add = { "/OUT", " = add nuw i8, ", "/IN" },
			_sub = { "/OUT", " = add nsw i8, ", "/IN" },
			_mul = { "/OUT", " = mul nsw i8, ", "/IN" },
			_div = { "/OUT", " = udiv i8, ", "/IN" },
			_mod = { "/OUT", " = urem i8, ", "/IN" },
			_shl = { "/OUT", " = shl i8, ", "/IN" },
			_shr = { "/OUT", " = lshr i8, ", "/IN" },
			_and = { "/OUT", " = and i8, ", "/IN" },
			_or = { "/OUT", " = or i8, ", "/IN" },
			_xor = { "/OUT", " = xor i8, ", "/IN" }
		}
	},
	bool = {
		def_local  = { { "/OUT", " = alloca i1, align 1" }},
		def_global = { { "/ADR", " = internal global i1 ", "/VAL", ", align 1" }},
		default = "false",
		store = { {"/R1", " = zext i1 ", "/IN", " to i8"}, {"store i8 ", "/R1", ", i8* ", "/ADR", ", align 1"}},
		load = { {"/R1", " = load i8, i8* ", "/IN", ", align 1"}, {"/OUT", " = trunc i8 ", "/R1", " to i1"}},
		conv = {
			_double = { {"/R1", " = load double, double* ", "/IN", ", align 8"}, {"/OUT", " = fcmp une double ", "/R1", ", 0.000000e+00"}},
			_float = { {"/R1", " = load float, float* ", "/IN", ", align 4"}, {"/OUT", " = fcmp une float ", "/R1", ", 0.000000e+00"}},
			_ulong = { {"/R1", " = load i64, i64* ", "/IN", ", align 8"}, {"/OUT", " = icmp ne i64 ", "/R1", ", 0"}},
			_long = { {"/R1", " = load i64, i64* ", "/IN", ", align 8"}, {"/OUT", " = icmp ne i64 ", "/R1", ", 0"}},
			_uint = { {"/R1", " = load i32, i32* ", "/IN", ", align 4"}, {"/OUT", " = icmp ne i32 ", "/R1", ", 0"}},
			_int = { {"/R1", " = load i32, i32* ", "/IN", ", align 4"}, {"/OUT", " = icmp ne i32 ", "/R1", ", 0"}},
			_ushort = { {"/R1", " = load i16, i16* ", "/IN", ", align 2"}, {"/OUT", " = icmp ne i16 ", "/R1", ", 0"}},
			_short = { {"/R1", " = load i16, i16* ", "/IN", ", align 2"}, {"/OUT", " = icmp ne i16 ", "/R1", ", 0"}},
			_byte = { {"/R1", " = load i8, i8* ", "/IN", ", align 1"}, {"/OUT", " = icmp ne i8 ", "/R1", ", 0"}},
			_bool = { {"/R1", " = load i8, i8* ", "/IN", ", align 1"}, {"/R1", " = trunc i8 ", "/R1", " to i1"}}
		},
		unop = {
			_not = { "/OUT", " = xor i1 ", "/IN", ", true" }
		},
		binop = {
			_and = { "/OUT", " = and i1, ", "/IN" },
			_or = { "/OUT", " = or i1, ", "/IN" },
			_xor = { "/OUT", " = xor i1, ", "/IN" }
		}
	}}}
