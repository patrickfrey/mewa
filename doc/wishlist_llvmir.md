# Wishlist to LLVM IR

1. Capability to use named labels for attribute lists instead of integers
	Instead of
	```LLVM
	attributes #0 = {
		"disable-tail-calls"="false" "frame-pointer"="all" "min-legal-vector-width"="0" 
		"correctly-rounded-divide-sqrt-fp-math"="false" "less-precise-fpmad"="false" "no-infs-fp-math"="false" 
		"no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "unsafe-fp-math"="false" 
		"no-jump-tables"="false" "stack-protector-buffer-size"="8" 
		"target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "use-soft-float"="false"
	}
	attributes #1 = {
		"no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "stack-protector-buffer-size"="8" }
	```
	the ability to use identifiers for labeling attribute lists:
	```LLVM
	attributes #module_function = {
		"disable-tail-calls"="false" "frame-pointer"="all" "min-legal-vector-width"="0" 
		"correctly-rounded-divide-sqrt-fp-math"="false" "less-precise-fpmad"="false" "no-infs-fp-math"="false" 
		"no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "unsafe-fp-math"="false" 
		"no-jump-tables"="false" "stack-protector-buffer-size"="8" 
		"target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "use-soft-float"="false"
	}
	attributes #extern_function = {
		"no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "stack-protector-buffer-size"="8" }
	```
2. Floating point constants
	The following code leads to an error:
	```LLVM
		store float 1.21, float* %r1
	```
	while
	```LLVM
		store float 1.25, float* %r1
	```
	is correct. I understand that the first is not representable with accurancy. 
	But still, I would like to configure the mapping as part of the backend.

