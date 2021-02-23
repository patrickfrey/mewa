; ModuleID = '{Source}'
source_filename = "{Source}"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "{Tripple}"
@llvm.global_ctors = appending global [1 x { i32, void ()*, i8* }] [{ i32, void ()*, i8* } { i32 65535, void ()* @_GLOBAL__ctors, i8* null }]
@llvm.global_dtors = appending global [1 x { i32, void ()*, i8* }] [{ i32, void ()*, i8* } { i32 65535, void ()* @_GLOBAL__dtors, i8* null }]

{Typedefs}{Auto}

define internal void @_GLOBAL__ctors() nounwind uwtable readnone optsize ssp section ".text.startup" {
  {GlobalCtors}ret void
}

define internal void @_GLOBAL__dtors() nounwind uwtable readnone optsize ssp section ".text.startup" {
  {GlobalDtors}ret void
}

{Code}

attributes #0 = {
	"disable-tail-calls"="false" "frame-pointer"="all" "min-legal-vector-width"="0" 
	"correctly-rounded-divide-sqrt-fp-math"="false" "less-precise-fpmad"="false" "no-infs-fp-math"="false" 
	"no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "unsafe-fp-math"="false" 
	"no-jump-tables"="false" "stack-protector-buffer-size"="8" 
	"target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "use-soft-float"="false"
}
attributes #1 = {
	"no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "stack-protector-buffer-size"="8" }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 10.0.0-4ubuntu1 "}
