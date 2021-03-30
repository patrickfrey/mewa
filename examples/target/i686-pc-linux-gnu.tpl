; ModuleID = '{Source}'
source_filename = "{Source}"
target datalayout = "e-m:e-p:32:32-p270:32:32-p271:32:32-p272:64:64-f64:32:64-f80:32-n8:16:32-S128"
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
	"correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="all" "min-legal-vector-width"="0"
	"no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="true"
	"stack-protector-buffer-size"="8" "target-cpu"="pentium4" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87"
	"unsafe-fp-math"="false" "use-soft-float"="false"
}

!llvm.module.flags = !{!0, !1, !2, !3}
!llvm.ident = !{!4}

!0 = !{i32 1, !"NumRegisterParameters", i32 0}
!1 = !{i32 1, !"wchar_size", i32 4}
!2 = !{i32 7, !"PIC Level", i32 2}
!3 = !{i32 7, !"PIE Level", i32 2}
!4 = !{!"clang version 11.0.1"}
