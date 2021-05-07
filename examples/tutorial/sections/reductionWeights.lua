-- Centralized list of the ordering of the reduction rules determined by their weights, we force an order of reductions by defining the weight sums as polynomials:
local rdw_conv = 1.0			-- Reduction weight of conversion
local rdw_constexpr = 0.0675		-- Minimum weight of a reduction involving a constexpr value
local rdw_load = 0.25			-- Reduction weight of loading a const expression
local rdw_sign = 0.125			-- Conversion of integers changing sign
local rdw_float = 0.25			-- Conversion switching floating point with integers
local rdw_bool = 0.25			-- Conversion of numeric value to boolean
local rdw_strip_r_1 = 0.25 / (1*1)	-- Reduction weight of stripping reference (fetch value) from type having 1 qualifier
local rdw_strip_r_2 = 0.25 / (2*2)	-- Reduction weight of stripping reference (fetch value) from type having 2 qualifiers
local rdw_strip_r_3 = 0.25 / (3*3)	-- Reduction weight of stripping reference (fetch value) from type having 3 qualifiers
local rwd_inheritance = 0.125 / (4*4)	-- Reduction weight of class inheritance
local rwd_boolean = 0.125 / (4*4)	-- Reduction weight of boolean type (control true/false type <-> boolean value) conversions

function preferWeight( flag, w1, w2) if flag then return w1 else return w2 end end -- Prefer one weight to the other
function factorWeight( fac, w1) return fac * w1	end
function combineWeight( w1, w2) return w1 + (w2 * 127.0 / 128.0) end -- Combining two reductions slightly less weight compared with applying both singularily
function scalarDeductionWeight( sizeweight) return 0.125*(1.0-sizeweight) end -- Deduction weight of this element for scalar operators
local weightEpsilon = 1E-8		-- Epsilon used for comparing weights for equality

