-- List of reduction weights:
rdw_conv = 1.0               -- Weight of conversion
rdw_constexpr_float = 0.375  -- Conversion of constexpr float -> int or int -> float
rdw_constexpr_bool = 0.50    -- Conversion of constexpr num -> bool or bool -> num
rdw_load = 0.25              -- Weight of loading a value
rdw_strip_r_1 = 0.25 / (1*1) -- Weight of fetch value type with 1 qualifier
rdw_strip_r_2 = 0.25 / (2*2) -- Weight of fetch value type with 2 qualifiers
rdw_strip_r_3 = 0.25 / (3*3) -- Weight of fetch value type with 3 qualifiers
rwd_control = 0.125 / (4*4)  -- Weight of control true/false type <-> bool

weightEpsilon = 1E-8         -- Epsilon used for comparing weights for equality
