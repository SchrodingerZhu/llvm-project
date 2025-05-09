; Test strict rounding functions for z10.
;
; RUN: llc < %s -mtriple=s390x-linux-gnu -mcpu=z10 | FileCheck %s

; Test rint for f32.
declare float @llvm.experimental.constrained.rint.f32(float, metadata, metadata)
define float @f1(float %f) #0 {
; CHECK-LABEL: f1:
; CHECK: fiebr %f0, 0, %f0
; CHECK: br %r14
  %res = call float @llvm.experimental.constrained.rint.f32(
                        float %f,
                        metadata !"round.dynamic",
                        metadata !"fpexcept.strict") #0
  ret float %res
}

; Test rint for f64.
declare double @llvm.experimental.constrained.rint.f64(double, metadata, metadata)
define double @f2(double %f) #0 {
; CHECK-LABEL: f2:
; CHECK: fidbr %f0, 0, %f0
; CHECK: br %r14
  %res = call double @llvm.experimental.constrained.rint.f64(
                        double %f,
                        metadata !"round.dynamic",
                        metadata !"fpexcept.strict") #0
  ret double %res
}

; Test rint for f128.
declare fp128 @llvm.experimental.constrained.rint.f128(fp128, metadata, metadata)
define void @f3(ptr %ptr) #0 {
; CHECK-LABEL: f3:
; CHECK: fixbr %f0, 0, %f0
; CHECK: br %r14
  %src = load fp128, ptr %ptr
  %res = call fp128 @llvm.experimental.constrained.rint.f128(
                        fp128 %src,
                        metadata !"round.dynamic",
                        metadata !"fpexcept.strict") #0
  store fp128 %res, ptr %ptr
  ret void
}

; Test nearbyint for f16.
declare half @llvm.experimental.constrained.nearbyint.f16(half, metadata, metadata)
define half @f4_half(half %f) #0 {
; CHECK-LABEL: f4_half:
; CHECK: brasl %r14, __extendhfsf2@PLT
; CHECK: brasl %r14, nearbyintf@PLT
; CHECK: brasl %r14, __truncsfhf2@PLT
; CHECK: br %r14
  %res = call half @llvm.experimental.constrained.nearbyint.f16(
                        half %f,
                        metadata !"round.dynamic",
                        metadata !"fpexcept.strict") #0
  ret half %res
}

; Test nearbyint for f32.
declare float @llvm.experimental.constrained.nearbyint.f32(float, metadata, metadata)
define float @f4(float %f) #0 {
; CHECK-LABEL: f4:
; CHECK: brasl %r14, nearbyintf@PLT
; CHECK: br %r14
  %res = call float @llvm.experimental.constrained.nearbyint.f32(
                        float %f,
                        metadata !"round.dynamic",
                        metadata !"fpexcept.strict") #0
  ret float %res
}

; Test nearbyint for f64.
declare double @llvm.experimental.constrained.nearbyint.f64(double, metadata, metadata)
define double @f5(double %f) #0 {
; CHECK-LABEL: f5:
; CHECK: brasl %r14, nearbyint@PLT
; CHECK: br %r14
  %res = call double @llvm.experimental.constrained.nearbyint.f64(
                        double %f,
                        metadata !"round.dynamic",
                        metadata !"fpexcept.strict") #0
  ret double %res
}

; Test nearbyint for f128.
declare fp128 @llvm.experimental.constrained.nearbyint.f128(fp128, metadata, metadata)
define void @f6(ptr %ptr) #0 {
; CHECK-LABEL: f6:
; CHECK: brasl %r14, nearbyintl@PLT
; CHECK: br %r14
  %src = load fp128, ptr %ptr
  %res = call fp128 @llvm.experimental.constrained.nearbyint.f128(
                        fp128 %src,
                        metadata !"round.dynamic",
                        metadata !"fpexcept.strict") #0
  store fp128 %res, ptr %ptr
  ret void
}

; Test floor for f16.
declare half @llvm.experimental.constrained.floor.f16(half, metadata)
define half @f7_half(half %f) #0 {
; CHECK-LABEL: f7_half:
; CHECK: brasl %r14, __extendhfsf2@PLT
; CHECK: brasl %r14, floorf@PLT
; CHECK: brasl %r14, __truncsfhf2@PLT
; CHECK: br %r14
  %res = call half @llvm.experimental.constrained.floor.f16(
                        half %f,
                        metadata !"fpexcept.strict") #0
  ret half %res
}

; Test floor for f32.
declare float @llvm.experimental.constrained.floor.f32(float, metadata)
define float @f7(float %f) #0 {
; CHECK-LABEL: f7:
; CHECK: brasl %r14, floorf@PLT
; CHECK: br %r14
  %res = call float @llvm.experimental.constrained.floor.f32(
                        float %f,
                        metadata !"fpexcept.strict") #0
  ret float %res
}

; Test floor for f64.
declare double @llvm.experimental.constrained.floor.f64(double, metadata)
define double @f8(double %f) #0 {
; CHECK-LABEL: f8:
; CHECK: brasl %r14, floor@PLT
; CHECK: br %r14
  %res = call double @llvm.experimental.constrained.floor.f64(
                        double %f,
                        metadata !"fpexcept.strict") #0
  ret double %res
}

; Test floor for f128.
declare fp128 @llvm.experimental.constrained.floor.f128(fp128, metadata)
define void @f9(ptr %ptr) #0 {
; CHECK-LABEL: f9:
; CHECK: brasl %r14, floorl@PLT
; CHECK: br %r14
  %src = load fp128, ptr %ptr
  %res = call fp128 @llvm.experimental.constrained.floor.f128(
                        fp128 %src,
                        metadata !"fpexcept.strict") #0
  store fp128 %res, ptr %ptr
  ret void
}

; Test ceil for f16.
declare half @llvm.experimental.constrained.ceil.f16(half, metadata)
define half @f10_half(half %f) #0 {
; CHECK-LABEL: f10_half:
; CHECK: brasl %r14, __extendhfsf2@PLT
; CHECK: brasl %r14, ceilf@PLT
; CHECK: brasl %r14, __truncsfhf2@PLT
; CHECK: br %r14
  %res = call half @llvm.experimental.constrained.ceil.f16(
                        half %f,
                        metadata !"fpexcept.strict") #0
  ret half %res
}

; Test ceil for f32.
declare float @llvm.experimental.constrained.ceil.f32(float, metadata)
define float @f10(float %f) #0 {
; CHECK-LABEL: f10:
; CHECK: brasl %r14, ceilf@PLT
; CHECK: br %r14
  %res = call float @llvm.experimental.constrained.ceil.f32(
                        float %f,
                        metadata !"fpexcept.strict") #0
  ret float %res
}

; Test ceil for f64.
declare double @llvm.experimental.constrained.ceil.f64(double, metadata)
define double @f11(double %f) #0 {
; CHECK-LABEL: f11:
; CHECK: brasl %r14, ceil@PLT
; CHECK: br %r14
  %res = call double @llvm.experimental.constrained.ceil.f64(
                        double %f,
                        metadata !"fpexcept.strict") #0
  ret double %res
}

; Test ceil for f128.
declare fp128 @llvm.experimental.constrained.ceil.f128(fp128, metadata)
define void @f12(ptr %ptr) #0 {
; CHECK-LABEL: f12:
; CHECK: brasl %r14, ceill@PLT
; CHECK: br %r14
  %src = load fp128, ptr %ptr
  %res = call fp128 @llvm.experimental.constrained.ceil.f128(
                        fp128 %src,
                        metadata !"fpexcept.strict") #0
  store fp128 %res, ptr %ptr
  ret void
}

; Test trunc for f16.
declare half @llvm.experimental.constrained.trunc.f16(half, metadata)
define half @f13_half(half %f) #0 {
; CHECK-LABEL: f13_half:
; CHECK: brasl %r14, __extendhfsf2@PLT
; CHECK: brasl %r14, truncf@PLT
; CHECK: brasl %r14, __truncsfhf2@PLT
; CHECK: br %r14
  %res = call half @llvm.experimental.constrained.trunc.f16(
                        half %f,
                        metadata !"fpexcept.strict") #0
  ret half %res
}

; Test trunc for f32.
declare float @llvm.experimental.constrained.trunc.f32(float, metadata)
define float @f13(float %f) #0 {
; CHECK-LABEL: f13:
; CHECK: brasl %r14, truncf@PLT
; CHECK: br %r14
  %res = call float @llvm.experimental.constrained.trunc.f32(
                        float %f,
                        metadata !"fpexcept.strict") #0
  ret float %res
}

; Test trunc for f64.
declare double @llvm.experimental.constrained.trunc.f64(double, metadata)
define double @f14(double %f) #0 {
; CHECK-LABEL: f14:
; CHECK: brasl %r14, trunc@PLT
; CHECK: br %r14
  %res = call double @llvm.experimental.constrained.trunc.f64(
                        double %f,
                        metadata !"fpexcept.strict") #0
  ret double %res
}

; Test trunc for f128.
declare fp128 @llvm.experimental.constrained.trunc.f128(fp128, metadata)
define void @f15(ptr %ptr) #0 {
; CHECK-LABEL: f15:
; CHECK: brasl %r14, truncl@PLT
; CHECK: br %r14
  %src = load fp128, ptr %ptr
  %res = call fp128 @llvm.experimental.constrained.trunc.f128(
                        fp128 %src,
                        metadata !"fpexcept.strict") #0
  store fp128 %res, ptr %ptr
  ret void
}

; Test round for f16.
declare half @llvm.experimental.constrained.round.f16(half, metadata)
define half @f16_half(half %f) #0 {
; CHECK-LABEL: f16_half:
; CHECK: brasl %r14, __extendhfsf2@PLT
; CHECK: brasl %r14, roundf@PLT
; CHECK: brasl %r14, __truncsfhf2@PLT
; CHECK: br %r14
  %res = call half @llvm.experimental.constrained.round.f16(
                        half %f,
                        metadata !"fpexcept.strict") #0
  ret half %res
}

; Test round for f32.
declare float @llvm.experimental.constrained.round.f32(float, metadata)
define float @f16(float %f) #0 {
; CHECK-LABEL: f16:
; CHECK: brasl %r14, roundf@PLT
; CHECK: br %r14
  %res = call float @llvm.experimental.constrained.round.f32(
                        float %f,
                        metadata !"fpexcept.strict") #0
  ret float %res
}

; Test round for f64.
declare double @llvm.experimental.constrained.round.f64(double, metadata)
define double @f17(double %f) #0 {
; CHECK-LABEL: f17:
; CHECK: brasl %r14, round@PLT
; CHECK: br %r14
  %res = call double @llvm.experimental.constrained.round.f64(
                        double %f,
                        metadata !"fpexcept.strict") #0
  ret double %res
}

; Test round for f128.
declare fp128 @llvm.experimental.constrained.round.f128(fp128, metadata)
define void @f18(ptr %ptr) #0 {
; CHECK-LABEL: f18:
; CHECK: brasl %r14, roundl@PLT
; CHECK: br %r14
  %src = load fp128, ptr %ptr
  %res = call fp128 @llvm.experimental.constrained.round.f128(
                        fp128 %src,
                        metadata !"fpexcept.strict") #0
  store fp128 %res, ptr %ptr
  ret void
}

; Test roundeven for f32.
declare float @llvm.experimental.constrained.roundeven.f32(float, metadata)
define float @f19(float %f) #0 {
; CHECK-LABEL: f19:
; CHECK: brasl %r14, roundevenf@PLT
; CHECK: br %r14
  %res = call float @llvm.experimental.constrained.roundeven.f32(
                        float %f,
                        metadata !"fpexcept.strict") #0
  ret float %res
}

; Test roundeven for f64.
declare double @llvm.experimental.constrained.roundeven.f64(double, metadata)
define double @f20(double %f) #0 {
; CHECK-LABEL: f20:
; CHECK: brasl %r14, roundeven@PLT
; CHECK: br %r14
  %res = call double @llvm.experimental.constrained.roundeven.f64(
                        double %f,
                        metadata !"fpexcept.strict") #0
  ret double %res
}

; Test roundeven for f128.
declare fp128 @llvm.experimental.constrained.roundeven.f128(fp128, metadata)
define void @f21(ptr %ptr) #0 {
; CHECK-LABEL: f21:
; CHECK: brasl %r14, roundevenl@PLT
; CHECK: br %r14
  %src = load fp128, ptr %ptr
  %res = call fp128 @llvm.experimental.constrained.roundeven.f128(
                        fp128 %src,
                        metadata !"fpexcept.strict") #0
  store fp128 %res, ptr %ptr
  ret void
}

attributes #0 = { strictfp }
