//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#![no_main]

use libfuzzer_sys::arbitrary::Arbitrary;
use libfuzzer_sys::fuzz_target;
use flat_tlsf::FreeListHeap;

#[derive(Arbitrary, Debug)]
enum Actions {
    /// Allocate memory with the given size and align of 1 << (align_bit % 12)
    Alloc { size: u16, align_bit: u8 },
    /// Dealloc the ith allocation
    Dealloc { index: u8 },
    /// Realloc the ith allocation
    Realloc { index: u8, new_size: u16 },
}

const HEAP_SIZE: usize = 256 * 1024; // 256 KB static fuzz heap

struct TrackedAlloc {
    ptr: *mut u8,
    size: usize,
    alignment: usize,
    canary: u8,
}

fuzz_target!(|actions: Vec<Actions>| fuzz_free_list(actions));

fn fuzz_free_list(actions: Vec<Actions>) {
    let mut heap_mem = vec![0u8; HEAP_SIZE];
    let heap = unsafe { FreeListHeap::from_raw(&mut heap_mem) };

    let mut allocations: Vec<TrackedAlloc> = vec![];
    let mut canary_counter: u8 = 0;

    for action in actions {
        match action {
            Actions::Alloc { size, align_bit } => {
                if size == 0 || align_bit > 12 {
                    continue;
                }
                let align = 1 << align_bit;
                let size = size as usize;

                let ptr = unsafe { heap.aligned_allocate(align, size) };

                if !ptr.is_null() {
                    assert_eq!(ptr as usize % align, 0, "FreeListHeap allocated pointer is not aligned!");

                    unsafe {
                        ptr.write_bytes(canary_counter, size);
                    }

                    allocations.push(TrackedAlloc {
                        ptr,
                        size,
                        alignment: align,
                        canary: canary_counter,
                    });
                    canary_counter = canary_counter.wrapping_add(1);
                }
            }
            Actions::Dealloc { index } => {
                if !allocations.is_empty() {
                    let idx = index as usize % allocations.len();
                    let alloc = allocations.swap_remove(idx);

                    assert_eq!(alloc.ptr as usize % alloc.alignment, 0, "Pointer became unaligned!");

                    unsafe {
                        let bytes = std::slice::from_raw_parts(alloc.ptr, alloc.size);
                        for &b in bytes {
                            assert_eq!(b, alloc.canary, "Canary memory corruption detected!");
                        }
                    }

                    unsafe {
                        heap.deallocate(alloc.ptr);
                    }
                }
            }
            Actions::Realloc { index, new_size } => {
                if !allocations.is_empty() && new_size > 0 {
                    let idx = index as usize % allocations.len();
                    let alloc = &mut allocations[idx];
                    let new_size = new_size as usize;

                    unsafe {
                        let bytes = std::slice::from_raw_parts(alloc.ptr, alloc.size);
                        for &b in bytes {
                            assert_eq!(b, alloc.canary, "Canary memory corruption before realloc!");
                        }
                    }

                    let new_ptr = unsafe { heap.reallocate(alloc.ptr, new_size) };

                    if !new_ptr.is_null() {
                        assert_eq!(new_ptr as usize % 8, 0, "Reallocated pointer is not aligned!");

                        if new_size > alloc.size {
                            unsafe {
                                new_ptr.add(alloc.size).write_bytes(alloc.canary, new_size - alloc.size);
                            }
                        }

                        alloc.ptr = new_ptr;
                        alloc.size = new_size;
                        alloc.alignment = 8;
                    }
                }
            }
        }
    }

    for alloc in allocations {
        unsafe {
            heap.deallocate(alloc.ptr);
        }
    }
}
