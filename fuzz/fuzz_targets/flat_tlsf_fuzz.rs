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
use flat_tlsf::FlatTlsfHeap;

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

fuzz_target!(|actions: Vec<Actions>| fuzz_flat_tlsf(actions));

fn fuzz_flat_tlsf(actions: Vec<Actions>) {
    // 1. Set up standard static backing memory array
    let mut heap_mem = vec![0u8; HEAP_SIZE];
    
    // 2. Initialize the FlatTlsfHeap FFI interface
    let heap = unsafe { FlatTlsfHeap::from_raw(&mut heap_mem) };

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

                // Call standard FFI aligned allocate
                let ptr = unsafe { heap.aligned_allocate(align, size) };

                if !ptr.is_null() {
                    // Check alignment correctness
                    assert_eq!(ptr as usize % align, 0, "FFI-allocated pointer is not aligned correctly!");

                    // Fill user payload with target canary bytes
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

                    // Verify pointer alignment before free
                    assert_eq!(alloc.ptr as usize % alloc.alignment, 0, "Tracked pointer became unaligned!");

                    // Verify the canary byte integrity across the entire block payload
                    unsafe {
                        let bytes = std::slice::from_raw_parts(alloc.ptr, alloc.size);
                        for &b in bytes {
                            assert_eq!(b, alloc.canary, "Canary byte memory corruption detected!");
                        }
                    }

                    // Free the block via FFI
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

                    // Verify the old canary integrity before reallocating
                    unsafe {
                        let bytes = std::slice::from_raw_parts(alloc.ptr, alloc.size);
                        for &b in bytes {
                            assert_eq!(b, alloc.canary, "Canary memory corruption before realloc!");
                        }
                    }

                    // Reallocate the block via FFI
                    let new_ptr = unsafe { heap.reallocate(alloc.ptr, new_size) };

                    if !new_ptr.is_null() {
                        // Check base chunk unit alignment correctness (8B on 64-bit)
                        assert_eq!(new_ptr as usize % 8, 0, "Reallocated pointer is not chunk-aligned!");

                        // If the size is extended, write canary to the newly added space
                        if new_size > alloc.size {
                            unsafe {
                                new_ptr.add(alloc.size).write_bytes(alloc.canary, new_size - alloc.size);
                            }
                        }

                        // Update our tracking record
                        alloc.ptr = new_ptr;
                        alloc.size = new_size;
                        alloc.alignment = 8; // realloc uses standard chunk alignment
                    }
                }
            }
        }
    }

    // Clean up all remaining outstanding allocations at the end of the session
    for alloc in allocations {
        unsafe {
            heap.deallocate(alloc.ptr);
        }
    }
}
