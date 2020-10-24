return {
constructor = {
	-- /IN variable address (@name) or register (%id)
	-- /OUT output register (%id)
	-- /ADR variable address (@name) or register (%id)
	-- /VAL constant value of a matching LLVM type
	double = {
		def_local  = { { "/OUT", " = alloca double, align 8" }},
		def_global = { { "/ADR", " = internal global double ", "/VAL", ", align 8" }},
		store = { {"store double ", "/IN", ", double* ", "/ADR", ", align 8"}},
		load = { {"/OUT", " = load double, double* ", "/IN", ", align 8"}},
		conv = {
			double = { {"/R1", " = load double, double* ", "/IN", ", align 8"}}
			float = { {"/R1", " = load float, float* ", "/IN", ", align 4"}, {"/OUT", " = fpext float ", "/R1", " to double"}}
			ulong = { {"/R1", " = load i64, i64* ", "/IN", ", align 8"}, {"/OUT", " = uitofp i64 ", "/R1", " to double"}}
			long = { {"/R1", " = load i64, i64* ", "/IN", ", align 8"}, {"/OUT", " = sitofp i64 ", "/R1", " to double"}}
			uint = { {"/R1", " = load i32, i32* ", "/IN", ", align 4"}, {"/OUT", " = uitofp i32 ", "/R1", " to double"}}
			int = { {"/R1", " = load i32, i32* ", "/IN", ", align 4"}, {"/OUT", " = sitofp i32 ", "/R1", " to double"}}
			ushort = { {"/R1", " = load i16, i16* ", "/IN", ", align 2"}, {"/OUT", " = uitofp i16 ", "/R1", " to double"}}
			short = { {"/R1", " = load i16, i16* ", "/IN", ", align 2"}, {"/OUT", " = sitofp i16 ", "/R1", " to double"}}
			byte = { {"/R1", " = load i8, i8* ", "/IN", ", align 1"}, {"/OUT", " = uitofp i8 ", "/R1", " to double"}}
			bool = { {"/R1", " = load i8, i8* ", "/IN", ", align 1"}, {"/R1", " = trunc i8 ", "/R1", " to i1"}, {"/R2", " = zext i1 ", "/R1", " to i32"},{"/OUT", " = sitofp i32 ", "/R2", " to double"}}
		}
		unop = {
			{ "/OUT", " = fneg double 0, ", "/IN" }
		}
		binop = {
			add = { "/OUT", " = fadd double, ", "/IN" }
			sub = { "/OUT", " = fsub double, ", "/IN" }
			mul = { "/OUT", " = fmul double, ", "/IN" }
			div = { "/OUT", " = fdiv double, ", "/IN" }
			mod = { "/OUT", " = frem double, ", "/IN" }
		}
	}
	float = {
		def_local  = { { "/OUT", " = alloca float, align 4" }},
		def_global = { { "/ADR", " = internal global float ", "/VAL", ", align 4" }},
		store = { {"store float ", "/IN", ", float* ", "/ADR", ", align 4"}},
		load = { {"/OUT", " = load float, float* ", "/IN", ", align 4"}},
		conv = {
			double = { {"/R1", " = load double, double* ", "/IN", ", align 8"}, {"/OUT", " = fptrunc double ", "/R1", " to float"}}
			float = { {"/R1", " = load float, float* ", "/IN", ", align 4"}}
			ulong = { {"/R1", " = load i64, i64* ", "/IN", ", align 8"}, {"/OUT", " = uitofp i64 ", "/R1", " to float"}}
			long = { {"/R1", " = load i64, i64* ", "/IN", ", align 8"}, {"/OUT", " = sitofp i64 ", "/R1", " to float"}}
			uint = { {"/R1", " = load i32, i32* ", "/IN", ", align 4"}, {"/OUT", " = uitofp i32 ", "/R1", " to float"}}
			int = { {"/R1", " = load i32, i32* ", "/IN", ", align 4"}, {"/OUT", " = sitofp i32 ", "/R1", " to float"}}
			ushort = { {"/R1", " = load i16, i16* ", "/IN", ", align 2"}, {"/OUT", " = uitofp i16 ", "/R1", " to float"}}
			short = { {"/R1", " = load i16, i16* ", "/IN", ", align 2"}, {"/OUT", " = sitofp i16 ", "/R1", " to float"}}
			byte = { {"/R1", " = load i8, i8* ", "/IN", ", align 1"}, {"/OUT", " = uitofp i8 ", "/R1", " to float"}}
			bool = { {"/R1", " = load i8, i8* ", "/IN", ", align 1"}, {"/R1", " = trunc i8 ", "/R1", " to i1"}, {"/R2", " = zext i1 ", "/R1", " to i32"},{"/OUT", " = sitofp i32 ", "/R2", " to float"}}
		}
		unop = {
			{ "/OUT", " = fneg float 0, ", "/IN" }
		}
		binop = {
			add = { "/OUT", " = fadd float, ", "/IN" }
			sub = { "/OUT", " = fsub float, ", "/IN" }
			mul = { "/OUT", " = fmul float, ", "/IN" }
			div = { "/OUT", " = fdiv float, ", "/IN" }
			mod = { "/OUT", " = frem float, ", "/IN" }
		}
	}
	ulong = {
		def_local  = { { "/OUT", " = alloca i64, align 8" }},
		def_global = { { "/ADR", " = internal global i64 ", "/VAL", ", align 8" }},
		store = { {"store i64 ", "/IN", ", i64* ", "/ADR", ", align 8"}},
		load = { {"/OUT", " = load i64, i64* ", "/IN", ", align 8"}},
		conv = {
			double = { {"/R1", " = load double, double* ", "/IN", ", align 8"}, {"/OUT", " = fptoui double ", "/R1", " to i64"}}
			float = { {"/R1", " = load float, float* ", "/IN", ", align 4"}, {"/OUT", " = fptoui float ", "/R1", " to i64"}}
			ulong = { {"/R1", " = load i64, i64* ", "/IN", ", align 8"}}
			long = { {"/R1", " = load i64, i64* ", "/IN", ", align 8"}}
			uint = { {"/R1", " = load i32, i32* ", "/IN", ", align 4"}, {"/OUT", " = zext i32 ", "/R1", " to i64"}}
			int = { {"/R1", " = load i32, i32* ", "/IN", ", align 4"}, {"/OUT", " = sext i32 ", "/R1", " to i64"}}
			ushort = { {"/R1", " = load i16, i16* ", "/IN", ", align 2"}, {"/OUT", " = zext i16 ", "/R1", " to i64"}}
			short = { {"/R1", " = load i16, i16* ", "/IN", ", align 2"}, {"/OUT", " = sext i16 ", "/R1", " to i64"}}
			byte = { {"/R1", " = load i8, i8* ", "/IN", ", align 1"}, {"/OUT", " = zext i8 ", "/R1", " to i64"}}
			bool = { {"/R1", " = load i8, i8* ", "/IN", ", align 1"}, {"/R1", " = trunc i8 ", "/R1", " to i1"}, {"/OUT", " = zext i1 ", "/R1", " to i64"}}
		}
		unop = {
			{ "/OUT", " = xor i64 ", "/IN", ", -1" }
		}
		binop = {
			add = { "/OUT", " = add nuw i64, ", "/IN" }
			sub = { "/OUT", " = add nsw i64, ", "/IN" }
			mul = { "/OUT", " = mul nsw i64, ", "/IN" }
			div = { "/OUT", " = udiv i64, ", "/IN" }
			mod = { "/OUT", " = urem i64, ", "/IN" }
			shl = { "/OUT", " = shl i64, ", "/IN" }
			shr = { "/OUT", " = lshr i64, ", "/IN" }
			and = { "/OUT", " = and i64, ", "/IN" }
			or = { "/OUT", " = or i64, ", "/IN" }
			xor = { "/OUT", " = xor i64, ", "/IN" }
		}
	}
	long = {
		def_local  = { { "/OUT", " = alloca i64, align 8" }},
		def_global = { { "/ADR", " = internal global i64 ", "/VAL", ", align 8" }},
		store = { {"store i64 ", "/IN", ", i64* ", "/ADR", ", align 8"}},
		load = { {"/OUT", " = load i64, i64* ", "/IN", ", align 8"}},
		conv = {
			double = { {"/R1", " = load double, double* ", "/IN", ", align 8"}, {"/OUT", " = fptosi double ", "/R1", " to i64"}}
			float = { {"/R1", " = load float, float* ", "/IN", ", align 4"}, {"/OUT", " = fptosi float ", "/R1", " to i64"}}
			ulong = { {"/R1", " = load i64, i64* ", "/IN", ", align 8"}}
			long = { {"/R1", " = load i64, i64* ", "/IN", ", align 8"}}
			uint = { {"/R1", " = load i32, i32* ", "/IN", ", align 4"}, {"/OUT", " = zext i32 ", "/R1", " to i64"}}
			int = { {"/R1", " = load i32, i32* ", "/IN", ", align 4"}, {"/OUT", " = sext i32 ", "/R1", " to i64"}}
			ushort = { {"/R1", " = load i16, i16* ", "/IN", ", align 2"}, {"/OUT", " = zext i16 ", "/R1", " to i64"}}
			short = { {"/R1", " = load i16, i16* ", "/IN", ", align 2"}, {"/OUT", " = sext i16 ", "/R1", " to i64"}}
			byte = { {"/R1", " = load i8, i8* ", "/IN", ", align 1"}, {"/OUT", " = zext i8 ", "/R1", " to i64"}}
			bool = { {"/R1", " = load i8, i8* ", "/IN", ", align 1"}, {"/R1", " = trunc i8 ", "/R1", " to i1"}, {"/OUT", " = zext i1 ", "/R1", " to i64"}}
		}
		unop = {
			{ "/OUT", " = sub i64 0, ", "/IN" }
		}
		binop = {
			add = { "/OUT", " = add nsw i64, ", "/IN" }
			sub = { "/OUT", " = sub nsw i64, ", "/IN" }
			mul = { "/OUT", " = mul nsw i64, ", "/IN" }
			div = { "/OUT", " = sdiv i64, ", "/IN" }
			mod = { "/OUT", " = srem i64, ", "/IN" }
			shl = { "/OUT", " = shl nsw i64, ", "/IN" }
			shr = { "/OUT", " = ashr i64, ", "/IN" }
		}
	}
	uint = {
		def_local  = { { "/OUT", " = alloca i32, align 4" }},
		def_global = { { "/ADR", " = internal global i32 ", "/VAL", ", align 4" }},
		store = { {"store i32 ", "/IN", ", i32* ", "/ADR", ", align 4"}},
		load = { {"/OUT", " = load i32, i32* ", "/IN", ", align 4"}},
		conv = {
			double = { {"/R1", " = load double, double* ", "/IN", ", align 8"}, {"/OUT", " = fptoui double ", "/R1", " to i32"}}
			float = { {"/R1", " = load float, float* ", "/IN", ", align 4"}, {"/OUT", " = fptoui float ", "/R1", " to i32"}}
			ulong = { {"/R1", " = load i64, i64* ", "/IN", ", align 8"}, {"/OUT", " = trunc i64 ", "/R1", " to i32"}}
			long = { {"/R1", " = load i64, i64* ", "/IN", ", align 8"}, {"/OUT", " = trunc i64 ", "/R1", " to i32"}}
			uint = { {"/R1", " = load i32, i32* ", "/IN", ", align 4"}}
			int = { {"/R1", " = load i32, i32* ", "/IN", ", align 4"}}
			ushort = { {"/R1", " = load i16, i16* ", "/IN", ", align 2"}, {"/OUT", " = zext i16 ", "/R1", " to i32"}}
			short = { {"/R1", " = load i16, i16* ", "/IN", ", align 2"}, {"/OUT", " = sext i16 ", "/R1", " to i32"}}
			byte = { {"/R1", " = load i8, i8* ", "/IN", ", align 1"}, {"/OUT", " = zext i8 ", "/R1", " to i32"}}
			bool = { {"/R1", " = load i8, i8* ", "/IN", ", align 1"}, {"/R1", " = trunc i8 ", "/R1", " to i1"}, {"/OUT", " = zext i1 ", "/R1", " to i32"}}
		}
		unop = {
			{ "/OUT", " = xor i32 ", "/IN", ", -1" }
		}
		binop = {
			add = { "/OUT", " = add nuw i32, ", "/IN" }
			sub = { "/OUT", " = add nsw i32, ", "/IN" }
			mul = { "/OUT", " = mul nsw i32, ", "/IN" }
			div = { "/OUT", " = udiv i32, ", "/IN" }
			mod = { "/OUT", " = urem i32, ", "/IN" }
			shl = { "/OUT", " = shl i32, ", "/IN" }
			shr = { "/OUT", " = lshr i32, ", "/IN" }
			and = { "/OUT", " = and i32, ", "/IN" }
			or = { "/OUT", " = or i32, ", "/IN" }
			xor = { "/OUT", " = xor i32, ", "/IN" }
		}
	}
	int = {
		def_local  = { { "/OUT", " = alloca i32, align 4" }},
		def_global = { { "/ADR", " = internal global i32 ", "/VAL", ", align 4" }},
		store = { {"store i32 ", "/IN", ", i32* ", "/ADR", ", align 4"}},
		load = { {"/OUT", " = load i32, i32* ", "/IN", ", align 4"}},
		conv = {
			double = { {"/R1", " = load double, double* ", "/IN", ", align 8"}, {"/OUT", " = fptosi double ", "/R1", " to i32"}}
			float = { {"/R1", " = load float, float* ", "/IN", ", align 4"}, {"/OUT", " = fptosi float ", "/R1", " to i32"}}
			ulong = { {"/R1", " = load i64, i64* ", "/IN", ", align 8"}, {"/OUT", " = trunc i64 ", "/R1", " to i32"}}
			long = { {"/R1", " = load i64, i64* ", "/IN", ", align 8"}, {"/OUT", " = trunc i64 ", "/R1", " to i32"}}
			uint = { {"/R1", " = load i32, i32* ", "/IN", ", align 4"}}
			int = { {"/R1", " = load i32, i32* ", "/IN", ", align 4"}}
			ushort = { {"/R1", " = load i16, i16* ", "/IN", ", align 2"}, {"/OUT", " = zext i16 ", "/R1", " to i32"}}
			short = { {"/R1", " = load i16, i16* ", "/IN", ", align 2"}, {"/OUT", " = sext i16 ", "/R1", " to i32"}}
			byte = { {"/R1", " = load i8, i8* ", "/IN", ", align 1"}, {"/OUT", " = zext i8 ", "/R1", " to i32"}}
			bool = { {"/R1", " = load i8, i8* ", "/IN", ", align 1"}, {"/R1", " = trunc i8 ", "/R1", " to i1"}, {"/OUT", " = zext i1 ", "/R1", " to i32"}}
		}
		unop = {
			{ "/OUT", " = sub i32 0, ", "/IN" }
		}
		binop = {
			add = { "/OUT", " = add nsw i32, ", "/IN" }
			sub = { "/OUT", " = sub nsw i32, ", "/IN" }
			mul = { "/OUT", " = mul nsw i32, ", "/IN" }
			div = { "/OUT", " = sdiv i32, ", "/IN" }
			mod = { "/OUT", " = srem i32, ", "/IN" }
			shl = { "/OUT", " = shl nsw i32, ", "/IN" }
			shr = { "/OUT", " = ashr i32, ", "/IN" }
		}
	}
	ushort = {
		def_local  = { { "/OUT", " = alloca i16, align 2" }},
		def_global = { { "/ADR", " = internal global i16 ", "/VAL", ", align 2" }},
		store = { {"store i16 ", "/IN", ", i16* ", "/ADR", ", align 2"}},
		load = { {"/OUT", " = load i16, i16* ", "/IN", ", align 2"}},
		conv = {
			double = { {"/R1", " = load double, double* ", "/IN", ", align 8"}, {"/OUT", " = fptoui double ", "/R1", " to i16"}}
			float = { {"/R1", " = load float, float* ", "/IN", ", align 4"}, {"/OUT", " = fptoui float ", "/R1", " to i16"}}
			ulong = { {"/R1", " = load i64, i64* ", "/IN", ", align 8"}, {"/OUT", " = trunc i64 ", "/R1", " to i16"}}
			long = { {"/R1", " = load i64, i64* ", "/IN", ", align 8"}, {"/OUT", " = trunc i64 ", "/R1", " to i16"}}
			uint = { {"/R1", " = load i32, i32* ", "/IN", ", align 4"}, {"/OUT", " = trunc i32 ", "/R1", " to i16"}}
			int = { {"/R1", " = load i32, i32* ", "/IN", ", align 4"}, {"/OUT", " = trunc i32 ", "/R1", " to i16"}}
			ushort = { {"/R1", " = load i16, i16* ", "/IN", ", align 2"}}
			short = { {"/R1", " = load i16, i16* ", "/IN", ", align 2"}}
			byte = { {"/R1", " = load i8, i8* ", "/IN", ", align 1"}, {"/OUT", " = zext i8 ", "/R1", " to i16"}}
			bool = { {"/R1", " = load i8, i8* ", "/IN", ", align 1"}, {"/R1", " = trunc i8 ", "/R1", " to i1"}, {"/OUT", " = zext i1 ", "/R1", " to i16"}}
		}
		unop = {
			{ "/OUT", " = xor i16 ", "/IN", ", -1" }
		}
		binop = {
			add = { "/OUT", " = add nuw i16, ", "/IN" }
			sub = { "/OUT", " = add nsw i16, ", "/IN" }
			mul = { "/OUT", " = mul nsw i16, ", "/IN" }
			div = { "/OUT", " = udiv i16, ", "/IN" }
			mod = { "/OUT", " = urem i16, ", "/IN" }
			shl = { "/OUT", " = shl i16, ", "/IN" }
			shr = { "/OUT", " = lshr i16, ", "/IN" }
			and = { "/OUT", " = and i16, ", "/IN" }
			or = { "/OUT", " = or i16, ", "/IN" }
			xor = { "/OUT", " = xor i16, ", "/IN" }
		}
	}
	short = {
		def_local  = { { "/OUT", " = alloca i16, align 2" }},
		def_global = { { "/ADR", " = internal global i16 ", "/VAL", ", align 2" }},
		store = { {"store i16 ", "/IN", ", i16* ", "/ADR", ", align 2"}},
		load = { {"/OUT", " = load i16, i16* ", "/IN", ", align 2"}},
		conv = {
			double = { {"/R1", " = load double, double* ", "/IN", ", align 8"}, {"/OUT", " = fptosi double ", "/R1", " to i16"}}
			float = { {"/R1", " = load float, float* ", "/IN", ", align 4"}, {"/OUT", " = fptosi float ", "/R1", " to i16"}}
			ulong = { {"/R1", " = load i64, i64* ", "/IN", ", align 8"}, {"/OUT", " = trunc i64 ", "/R1", " to i16"}}
			long = { {"/R1", " = load i64, i64* ", "/IN", ", align 8"}, {"/OUT", " = trunc i64 ", "/R1", " to i16"}}
			uint = { {"/R1", " = load i32, i32* ", "/IN", ", align 4"}, {"/OUT", " = trunc i32 ", "/R1", " to i16"}}
			int = { {"/R1", " = load i32, i32* ", "/IN", ", align 4"}, {"/OUT", " = trunc i32 ", "/R1", " to i16"}}
			ushort = { {"/R1", " = load i16, i16* ", "/IN", ", align 2"}}
			short = { {"/R1", " = load i16, i16* ", "/IN", ", align 2"}}
			byte = { {"/R1", " = load i8, i8* ", "/IN", ", align 1"}, {"/OUT", " = zext i8 ", "/R1", " to i16"}}
			bool = { {"/R1", " = load i8, i8* ", "/IN", ", align 1"}, {"/R1", " = trunc i8 ", "/R1", " to i1"}, {"/OUT", " = zext i1 ", "/R1", " to i16"}}
		}
		unop = {
			{ "/OUT", " = sub i16 0, ", "/IN" }
		}
		binop = {
			add = { "/OUT", " = add nsw i16, ", "/IN" }
			sub = { "/OUT", " = sub nsw i16, ", "/IN" }
			mul = { "/OUT", " = mul nsw i16, ", "/IN" }
			div = { "/OUT", " = sdiv i16, ", "/IN" }
			mod = { "/OUT", " = srem i16, ", "/IN" }
			shl = { "/OUT", " = shl nsw i16, ", "/IN" }
			shr = { "/OUT", " = ashr i16, ", "/IN" }
		}
	}
	byte = {
		def_local  = { { "/OUT", " = alloca i8, align 1" }},
		def_global = { { "/ADR", " = internal global i8 ", "/VAL", ", align 1" }},
		store = { {"store i8 ", "/IN", ", i8* ", "/ADR", ", align 1"}},
		load = { {"/OUT", " = load i8, i8* ", "/IN", ", align 1"}},
		conv = {
			double = { {"/R1", " = load double, double* ", "/IN", ", align 8"}, {"/OUT", " = fptoui double ", "/R1", " to i8"}}
			float = { {"/R1", " = load float, float* ", "/IN", ", align 4"}, {"/OUT", " = fptoui float ", "/R1", " to i8"}}
			ulong = { {"/R1", " = load i64, i64* ", "/IN", ", align 8"}, {"/OUT", " = trunc i64 ", "/R1", " to i8"}}
			long = { {"/R1", " = load i64, i64* ", "/IN", ", align 8"}, {"/OUT", " = trunc i64 ", "/R1", " to i8"}}
			uint = { {"/R1", " = load i32, i32* ", "/IN", ", align 4"}, {"/OUT", " = trunc i32 ", "/R1", " to i8"}}
			int = { {"/R1", " = load i32, i32* ", "/IN", ", align 4"}, {"/OUT", " = trunc i32 ", "/R1", " to i8"}}
			ushort = { {"/R1", " = load i16, i16* ", "/IN", ", align 2"}, {"/OUT", " = trunc i16 ", "/R1", " to i8"}}
			short = { {"/R1", " = load i16, i16* ", "/IN", ", align 2"}, {"/OUT", " = trunc i16 ", "/R1", " to i8"}}
			byte = { {"/R1", " = load i8, i8* ", "/IN", ", align 1"}}
			bool = { {"/R1", " = load i8, i8* ", "/IN", ", align 1"}, {"/R1", " = trunc i8 ", "/R1", " to i1"}, {"/OUT", " = zext i1 ", "/R1", " to i8"}}
		}
		unop = {
			{ "/OUT", " = xor i8 ", "/IN", ", -1" }
		}
		binop = {
			add = { "/OUT", " = add nuw i8, ", "/IN" }
			sub = { "/OUT", " = add nsw i8, ", "/IN" }
			mul = { "/OUT", " = mul nsw i8, ", "/IN" }
			div = { "/OUT", " = udiv i8, ", "/IN" }
			mod = { "/OUT", " = urem i8, ", "/IN" }
			shl = { "/OUT", " = shl i8, ", "/IN" }
			shr = { "/OUT", " = lshr i8, ", "/IN" }
			and = { "/OUT", " = and i8, ", "/IN" }
			or = { "/OUT", " = or i8, ", "/IN" }
			xor = { "/OUT", " = xor i8, ", "/IN" }
		}
	}
	bool = {
		def_local  = { { "/OUT", " = alloca i1, align 1" }},
		def_global = { { "/ADR", " = internal global i1 ", "/VAL", ", align 1" }},
		store = { {"/R1", " = zext i1 ", "/IN", " to i8"}, {"store i8 ", "/R1", ", i8* ", "/ADR", ", align 1"}},
		load = { {"/R1", " = load i8, i8* ", "/IN", ", align 1"}, {"/OUT", " = trunc i8 ", "/R1", " to i1"}},
		conv = {
			double = { {"/R1", " = load double, double* ", "/IN", ", align 8"}, {"/OUT", " = fcmp une double ", "/R1", ", 0.000000e+00"}}
			float = { {"/R1", " = load float, float* ", "/IN", ", align 4"}, {"/OUT", " = fcmp une float ", "/R1", ", 0.000000e+00"}}
			ulong = { {"/R1", " = load i64, i64* ", "/IN", ", align 8"}, {"/OUT", " = icmp ne i64 ", "/R1", ", 0"}}
			long = { {"/R1", " = load i64, i64* ", "/IN", ", align 8"}, {"/OUT", " = icmp ne i64 ", "/R1", ", 0"}}
			uint = { {"/R1", " = load i32, i32* ", "/IN", ", align 4"}, {"/OUT", " = icmp ne i32 ", "/R1", ", 0"}}
			int = { {"/R1", " = load i32, i32* ", "/IN", ", align 4"}, {"/OUT", " = icmp ne i32 ", "/R1", ", 0"}}
			ushort = { {"/R1", " = load i16, i16* ", "/IN", ", align 2"}, {"/OUT", " = icmp ne i16 ", "/R1", ", 0"}}
			short = { {"/R1", " = load i16, i16* ", "/IN", ", align 2"}, {"/OUT", " = icmp ne i16 ", "/R1", ", 0"}}
			byte = { {"/R1", " = load i8, i8* ", "/IN", ", align 1"}, {"/OUT", " = icmp ne i8 ", "/R1", ", 0"}}
			bool = { {"/R1", " = load i8, i8* ", "/IN", ", align 1"}, {"/R1", " = trunc i8 ", "/R1", " to i1"}}
		}
		unop = {
			{ "/OUT", " = xor i1 ", "/IN", ", true" }
		}
		binop = {
			and = { "/OUT", " = and i1, ", "/IN" }
			or = { "/OUT", " = or i1, ", "/IN" }
			xor = { "/OUT", " = xor i1, ", "/IN" }
		}
	}
}}
