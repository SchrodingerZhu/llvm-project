//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

fn main() {
    let mut build = cc::Build::new();
    build.compiler("clang++");
    build.cpp(true)
        .std("c++17")
        .file("src/cpp/flat_tlsf_ffi.cpp")
        .file("src/cpp/vendor/freelist.cpp")
        .file("src/cpp/vendor/freetrie.cpp")
        .include("src/cpp")
        .include("src/cpp/vendor")
        .flag("-O3")
        .flag("-g")
        .flag("-fomit-frame-pointer")
        .flag("-march=native")
        .flag("-flto=thin");



    build.compile("flat_tlsf2_ffi");

    println!("cargo:rerun-if-changed=src/cpp/flat_tlsf_heap.h");
    println!("cargo:rerun-if-changed=src/cpp/flat_tlsf_ffi.h");
    println!("cargo:rerun-if-changed=src/cpp/flat_tlsf_ffi.cpp");
    println!("cargo:rerun-if-changed=src/cpp/llvm_libc_support.h");
}
