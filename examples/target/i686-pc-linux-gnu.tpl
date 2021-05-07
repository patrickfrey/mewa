; ModuleID = '{Source}'
source_filename = "{Source}"
target triple = "{Triple}"

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
	"stack-protector-buffer-size"="8" "target-cpu"="pentium4" "target-features"="+x87"
	"unsafe-fp-math"="false" "use-soft-float"="false"
}

!llvm.module.flags = !{!0, !1, !2, !3}

!0 = !{i32 1, !"NumRegisterParameters", i32 0}
!1 = !{i32 1, !"wchar_size", i32 4}
!2 = !{i32 7, !"PIC Level", i32 2}
!3 = !{i32 7, !"PIE Level", i32 2}
