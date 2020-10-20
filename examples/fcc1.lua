
return {
	constructor = {
		-- $ADR @globnam OR %regnam
		-- $OUT %regnam
		-- $IN 
		-- $VAL constant of the values type
		byte = {
                         def_local = { "$OUT", " = alloca i8, align 1" }
                         def_global = { "@", "$NAM", " = internal global i8 ", "$VAL", ", align 1" }
		},
		short = {
                         def_local = { "$OUT", " = alloca i16, align 2" }
                         def_global = { "@", "$NAM", " = internal global i16 ", "$VAL", ", align 2" }
		},
		ushort = {
                         def_local = { "$OUT", " = alloca i16, align 2" }
                         def_global = { "@", "$NAM", " = internal global i16 ", "$VAL", ", align 2" }
		},
		int = {
                         def_local = { "$OUT", " = alloca i32, align 4" }
                         def_global = { "@", "$NAM", " = internal global i32 ", "$VAL", ", align 4" }
		},
		uint = {
                         def_local = { "$OUT", " = alloca i32, align 4" }
                         def_global = { "@", "$NAM", " = internal global i32 ", "$VAL", ", align 4" }
		},
		long = {
                         def_local = { "$OUT", " = alloca i64, align 8" }
                         def_global = { "@", "$NAM", " = internal global i64 ", "$VAL", ", align 8" }
		},
		ulong = {
                         def_local = { "$OUT", " = alloca i64, align 8" }
                         def_global = { "@", "$NAM", " = internal global i64 ", "$VAL", ", align 8" }
		},
		float = {
                         def_local = { "$OUT", " = alloca float, align 4" }
                         def_global = { "@", "$NAM", " = internal global float ", "$VAL", ", align 8" }
		},
		double = {
                         def_local  = { "$OUT", " = alloca double, align 8" },
                         def_global = { "@", "$NAM", " = internal global double ", "$VAL", ", align 8" }
                         store      = { "store double ", "$IN", ", double* ", "$ADR", ", align 8" }
                         load       = { "$OUT", " = load double, double* ", "$IN", ", align 8" }
		},
	}
}

