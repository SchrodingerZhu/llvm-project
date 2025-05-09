; NOTE: Assertions have been autogenerated by utils/update_test_checks.py UTC_ARGS: --version 5
; RUN: opt -p 'require<profile-summary>,function(codegenprepare)' -S %s \
; RUN:   | FileCheck %s --check-prefix=SLOW
; RUN: opt -p 'require<profile-summary>,function(codegenprepare)' -S --mattr=+zvbb %s \
; RUN:   | FileCheck %s --check-prefix=FAST
; REQUIRES: riscv-registered-target

target datalayout = "e-m:e-p:64:64-i64:64-i128:128-n32:64-S128"
target triple = "riscv64"

define <4 x i1> @test_ult_2(<4 x i64> %x) {
; SLOW-LABEL: define <4 x i1> @test_ult_2(
; SLOW-SAME: <4 x i64> [[X:%.*]]) {
; SLOW-NEXT:    [[TMP0:%.*]] = add <4 x i64> [[X]], splat (i64 -1)
; SLOW-NEXT:    [[TMP1:%.*]] = and <4 x i64> [[X]], [[TMP0]]
; SLOW-NEXT:    [[CMP1:%.*]] = icmp eq <4 x i64> [[TMP1]], zeroinitializer
; SLOW-NEXT:    ret <4 x i1> [[CMP1]]
;
; FAST-LABEL: define <4 x i1> @test_ult_2(
; FAST-SAME: <4 x i64> [[X:%.*]]) #[[ATTR0:[0-9]+]] {
; FAST-NEXT:    [[CTPOP:%.*]] = call <4 x i64> @llvm.ctpop.v4i64(<4 x i64> [[X]])
; FAST-NEXT:    [[CMP1:%.*]] = icmp ult <4 x i64> [[CTPOP]], splat (i64 2)
; FAST-NEXT:    ret <4 x i1> [[CMP1]]
;
  %ctpop = call <4 x i64> @llvm.ctpop(<4 x i64> %x)
  %cmp = icmp ult <4 x i64> %ctpop, splat (i64 2)
  ret <4 x i1> %cmp
}

define <4 x i1> @test_ugt_1(<4 x i64> %x) {
; SLOW-LABEL: define <4 x i1> @test_ugt_1(
; SLOW-SAME: <4 x i64> [[X:%.*]]) {
; SLOW-NEXT:    [[TMP0:%.*]] = add <4 x i64> [[X]], splat (i64 -1)
; SLOW-NEXT:    [[TMP1:%.*]] = and <4 x i64> [[X]], [[TMP0]]
; SLOW-NEXT:    [[CMP1:%.*]] = icmp ne <4 x i64> [[TMP1]], zeroinitializer
; SLOW-NEXT:    ret <4 x i1> [[CMP1]]
;
; FAST-LABEL: define <4 x i1> @test_ugt_1(
; FAST-SAME: <4 x i64> [[X:%.*]]) #[[ATTR0]] {
; FAST-NEXT:    [[CTPOP:%.*]] = call <4 x i64> @llvm.ctpop.v4i64(<4 x i64> [[X]])
; FAST-NEXT:    [[CMP1:%.*]] = icmp ugt <4 x i64> [[CTPOP]], splat (i64 1)
; FAST-NEXT:    ret <4 x i1> [[CMP1]]
;
  %ctpop = call <4 x i64> @llvm.ctpop(<4 x i64> %x)
  %cmp = icmp ugt <4 x i64> %ctpop, splat (i64 1)
  ret <4 x i1> %cmp
}

define <4 x i1> @test_eq_1(<4 x i64> %x) {
; SLOW-LABEL: define <4 x i1> @test_eq_1(
; SLOW-SAME: <4 x i64> [[X:%.*]]) {
; SLOW-NEXT:    [[TMP0:%.*]] = add <4 x i64> [[X]], splat (i64 -1)
; SLOW-NEXT:    [[TMP1:%.*]] = xor <4 x i64> [[X]], [[TMP0]]
; SLOW-NEXT:    [[TMP2:%.*]] = icmp ugt <4 x i64> [[TMP1]], [[TMP0]]
; SLOW-NEXT:    ret <4 x i1> [[TMP2]]
;
; FAST-LABEL: define <4 x i1> @test_eq_1(
; FAST-SAME: <4 x i64> [[X:%.*]]) #[[ATTR0]] {
; FAST-NEXT:    [[CTPOP:%.*]] = call <4 x i64> @llvm.ctpop.v4i64(<4 x i64> [[X]])
; FAST-NEXT:    [[CMP1:%.*]] = icmp eq <4 x i64> [[CTPOP]], splat (i64 1)
; FAST-NEXT:    ret <4 x i1> [[CMP1]]
;
  %ctpop = call <4 x i64> @llvm.ctpop(<4 x i64> %x)
  %cmp = icmp eq <4 x i64> %ctpop, splat (i64 1)
  ret <4 x i1> %cmp
}

define <4 x i1> @test_ne_1(<4 x i64> %x) {
; SLOW-LABEL: define <4 x i1> @test_ne_1(
; SLOW-SAME: <4 x i64> [[X:%.*]]) {
; SLOW-NEXT:    [[TMP0:%.*]] = add <4 x i64> [[X]], splat (i64 -1)
; SLOW-NEXT:    [[TMP1:%.*]] = xor <4 x i64> [[X]], [[TMP0]]
; SLOW-NEXT:    [[TMP2:%.*]] = icmp ule <4 x i64> [[TMP1]], [[TMP0]]
; SLOW-NEXT:    ret <4 x i1> [[TMP2]]
;
; FAST-LABEL: define <4 x i1> @test_ne_1(
; FAST-SAME: <4 x i64> [[X:%.*]]) #[[ATTR0]] {
; FAST-NEXT:    [[CTPOP:%.*]] = call <4 x i64> @llvm.ctpop.v4i64(<4 x i64> [[X]])
; FAST-NEXT:    [[CMP1:%.*]] = icmp ne <4 x i64> [[CTPOP]], splat (i64 1)
; FAST-NEXT:    ret <4 x i1> [[CMP1]]
;
  %ctpop = call <4 x i64> @llvm.ctpop(<4 x i64> %x)
  %cmp = icmp ne <4 x i64> %ctpop, splat (i64 1)
  ret <4 x i1> %cmp
}
