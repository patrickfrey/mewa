-- Centralized list of the ordering of the reduction rules determined by their weights, we force an order of reductions by defining the weight sums as polynomials:
rdw_conv = 1.0			-- Reduction weight of conversion
rdw_constexpr = 0.0675		-- Minimum weight of a reduction involving a constexpr value
rdw_load = 0.25			-- Reduction weight of loading a value
rdw_sign = 0.125		-- Conversion of integers changing sign
rdw_float = 0.375		-- Conversion of floating point to integers or integer to floating point
rdw_bool = 0.50			-- Conversion of numeric value to boolean or boolean to numeric value
rdw_strip_r_1 = 0.25 / (1*1)	-- Reduction weight of stripping reference (fetch value) from type having 1 qualifier
rdw_strip_r_2 = 0.25 / (2*2)	-- Reduction weight of stripping reference (fetch value) from type having 2 qualifiers
rdw_strip_r_3 = 0.25 / (3*3)	-- Reduction weight of stripping reference (fetch value) from type having 3 qualifiers
rwd_inheritance = 0.125 / (4*4)	-- Reduction weight of class inheritance
rwd_control = 0.125 / (4*4)	-- Reduction weight of boolean type (control true/false type <-> boolean value) conversions

weightEpsilon = 1E-8		-- Epsilon used for comparing weights for equality

