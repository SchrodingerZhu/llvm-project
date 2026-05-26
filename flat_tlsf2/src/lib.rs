//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#![allow(clippy::missing_safety_doc)]

use std::alloc::{GlobalAlloc, Layout};
use std::ffi::c_void;
use std::ptr::null_mut;
use spinning_top::Spinlock;

// Low-level C-linkage entry points mapping to the C++ FFI library
extern "C" {
    fn flat_tlsf2_create(mem: *mut c_void, size: usize) -> *mut c_void;
    fn flat_tlsf2_destroy(heap: *mut c_void);
    fn flat_tlsf2_allocate(heap: *mut c_void, size: usize) -> *mut c_void;
    fn flat_tlsf2_aligned_allocate(heap: *mut c_void, alignment: usize, size: usize) -> *mut c_void;
    fn flat_tlsf2_free(heap: *mut c_void, ptr: *mut c_void);
    fn flat_tlsf2_realloc(heap: *mut c_void, ptr: *mut c_void, size: usize) -> *mut c_void;
    fn flat_tlsf2_calloc(heap: *mut c_void, num: usize, size: usize) -> *mut c_void;
    fn flat_tlsf2_get_free_mem(heap: *mut c_void) -> usize;

    // FreeList FFI entry points
    fn freelist2_create(mem: *mut c_void, size: usize) -> *mut c_void;
    fn freelist2_destroy(heap: *mut c_void);
    fn freelist2_allocate(heap: *mut c_void, size: usize) -> *mut c_void;
    fn freelist2_aligned_allocate(heap: *mut c_void, alignment: usize, size: usize) -> *mut c_void;
    fn freelist2_free(heap: *mut c_void, ptr: *mut c_void);
    fn freelist2_realloc(heap: *mut c_void, ptr: *mut c_void, size: usize) -> *mut c_void;
    fn freelist2_calloc(heap: *mut c_void, num: usize, size: usize) -> *mut c_void;
}

/// A safe, ergonomic Rust wrapper around the static C++ FlatTlsfHeap.
pub struct FlatTlsfHeap {
    ffi_ptr: *mut c_void,
    // Pin raw heap backing storage here to prevent it from dropping/relocating
    _storage: Option<Vec<u8>>,
}

// Thread-safety markers since operations are protected by Mutex wrappers at high level
unsafe impl Send for FlatTlsfHeap {}
unsafe impl Sync for FlatTlsfHeap {}

impl FlatTlsfHeap {
    /// Creates a new heap instance by dynamically allocating a heap backing buffer in Rust.
    pub fn new(capacity: usize) -> Self {
        assert!(capacity >= 32, "Heap capacity must be at least 32 bytes");
        // Ensure 64-byte alignment of the backing buffer
        let mut storage = vec![0u8; capacity];
        let ffi_ptr = unsafe {
            flat_tlsf2_create(storage.as_mut_ptr() as *mut _, capacity)
        };
        assert!(!ffi_ptr.is_null(), "Failed to initialize FlatTlsfHeap inside C++ FFI");
        Self {
            ffi_ptr,
            _storage: Some(storage),
        }
    }

    /// Creates a new heap instance using a raw, static, or user-owned external memory buffer.
    /// 
    /// # Safety
    /// The user must ensure the backing memory slice remains valid and is not modified/relocated
    /// for the entire lifetime of this heap instance.
    pub unsafe fn from_raw(mem: &mut [u8]) -> Self {
        assert!(mem.len() >= 32, "Heap capacity must be at least 32 bytes");
        let ffi_ptr = flat_tlsf2_create(mem.as_mut_ptr() as *mut _, mem.len());
        assert!(!ffi_ptr.is_null(), "Failed to initialize FlatTlsfHeap inside C++ FFI");
        Self {
            ffi_ptr,
            _storage: None,
        }
    }

    /// Allocates raw byte space.
    /// 
    /// # Safety
    /// Returns a raw pointer. Dereferencing it is unsafe and standard memory safety rules apply.
    pub unsafe fn allocate(&self, size: usize) -> *mut u8 {
        if size == 0 {
            return null_mut();
        }
        flat_tlsf2_allocate(self.ffi_ptr, size) as *mut u8
    }

    /// Allocates aligned byte space. Rounds up requested size to the nearest multiple of
    /// alignment to comply with strict C++ target check constraints.
    /// 
    /// # Safety
    /// Standard raw pointer allocation rules apply.
    pub unsafe fn aligned_allocate(&self, alignment: usize, size: usize) -> *mut u8 {
        if size == 0 {
            return null_mut();
        }
        // Round up size to a multiple of alignment to respect strict C++ aligned_allocate checks!
        let aligned_size = (size + alignment - 1) & !(alignment - 1);
        flat_tlsf2_aligned_allocate(self.ffi_ptr, alignment, aligned_size) as *mut u8
    }

    /// Reclaims allocated pointer space.
    /// 
    /// # Safety
    /// Deallocates the pointer. Passing a pointer not allocated by this heap leads to undefined behavior.
    pub unsafe fn deallocate(&self, ptr: *mut u8) {
        if ptr.is_null() {
            return;
        }
        flat_tlsf2_free(self.ffi_ptr, ptr as *mut c_void);
    }

    /// Reallocates standard pointer.
    /// 
    /// # Safety
    /// Standard reallocation lifetime safety rules apply.
    pub unsafe fn reallocate(&self, ptr: *mut u8, new_size: usize) -> *mut u8 {
        flat_tlsf2_realloc(self.ffi_ptr, ptr as *mut c_void, new_size) as *mut u8
    }

    /// Allocates and zeroes out memory.
    /// 
    /// # Safety
    /// Standard dynamic calloc safety rules apply.
    pub unsafe fn callocate(&self, num: usize, size: usize) -> *mut u8 {
        flat_tlsf2_calloc(self.ffi_ptr, num, size) as *mut u8
    }

    /// Returns total unallocated free bytes currently left inside the heap.
    pub fn get_free_mem(&self) -> usize {
        unsafe { flat_tlsf2_get_free_mem(self.ffi_ptr) }
    }
}

impl Drop for FlatTlsfHeap {
    fn drop(&mut self) {
        unsafe {
            flat_tlsf2_destroy(self.ffi_ptr);
        }
    }
}

/// A thread-safe global static allocator wrapping FlatTlsfHeap with standard mutual exclusion locks.
pub struct FlatTlsfGlobal {
    inner: Spinlock<Option<FlatTlsfHeap>>,
}

impl FlatTlsfGlobal {
    /// Creates a new uninitialized static global allocator shell.
    pub const fn empty() -> Self {
        Self {
            inner: Spinlock::new(None),
        }
    }

    /// Initializes the static global allocator using a raw, static memory array.
    /// 
    /// # Safety
    /// The user must guarantee that this function is called only once, before any allocations occur,
    /// and that the slice storage lifetime extends for the entire program execution frame.
    pub unsafe fn init(&self, mem: &'static mut [u8]) {
        let heap = FlatTlsfHeap::from_raw(mem);
        *self.inner.lock() = Some(heap);
    }
}

unsafe impl GlobalAlloc for FlatTlsfGlobal {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        let lock = self.inner.lock();
        if let Some(ref heap) = *lock {
            heap.aligned_allocate(layout.align(), layout.size())
        } else {
            null_mut()
        }
    }

    unsafe fn dealloc(&self, ptr: *mut u8, _layout: Layout) {
        let lock = self.inner.lock();
        if let Some(ref heap) = *lock {
            heap.deallocate(ptr);
        }
    }

    unsafe fn realloc(&self, ptr: *mut u8, _layout: Layout, new_size: usize) -> *mut u8 {
        let lock = self.inner.lock();
        if let Some(ref heap) = *lock {
            heap.reallocate(ptr, new_size)
        } else {
            null_mut()
        }
    }
}

#[cfg(feature = "allocator-api")]
unsafe impl core::alloc::Allocator for FlatTlsfHeap {
    fn allocate(&self, layout: Layout) -> Result<std::ptr::NonNull<[u8]>, core::alloc::AllocError> {
        let ptr = unsafe { self.aligned_allocate(layout.align(), layout.size()) };
        if ptr.is_null() {
            Err(core::alloc::AllocError)
        } else {
            Ok(unsafe {
                std::ptr::NonNull::new_unchecked(std::slice::from_raw_parts_mut(ptr, layout.size()))
            })
        }
    }

    unsafe fn deallocate(&self, ptr: std::ptr::NonNull<u8>, _layout: Layout) {
        self.deallocate(ptr.as_ptr());
    }
}

/// A safe, ergonomic Rust wrapper around the static C++ FreeListHeap.
pub struct FreeListHeap {
    ffi_ptr: *mut c_void,
    _storage: Option<Vec<u8>>,
}

unsafe impl Send for FreeListHeap {}
unsafe impl Sync for FreeListHeap {}

impl FreeListHeap {
    pub fn new(capacity: usize) -> Self {
        assert!(capacity >= 32);
        let mut storage = vec![0u8; capacity];
        let ffi_ptr = unsafe {
            freelist2_create(storage.as_mut_ptr() as *mut _, capacity)
        };
        assert!(!ffi_ptr.is_null());
        Self {
            ffi_ptr,
            _storage: Some(storage),
        }
    }

    pub unsafe fn from_raw(mem: &mut [u8]) -> Self {
        assert!(mem.len() >= 32);
        let ffi_ptr = freelist2_create(mem.as_mut_ptr() as *mut _, mem.len());
        assert!(!ffi_ptr.is_null());
        Self {
            ffi_ptr,
            _storage: None,
        }
    }

    pub unsafe fn allocate(&self, size: usize) -> *mut u8 {
        if size == 0 {
            return null_mut();
        }
        freelist2_allocate(self.ffi_ptr, size) as *mut u8
    }

    pub unsafe fn aligned_allocate(&self, alignment: usize, size: usize) -> *mut u8 {
        if size == 0 {
            return null_mut();
        }
        let aligned_size = (size + alignment - 1) & !(alignment - 1);
        freelist2_aligned_allocate(self.ffi_ptr, alignment, aligned_size) as *mut u8
    }

    pub unsafe fn deallocate(&self, ptr: *mut u8) {
        if ptr.is_null() {
            return;
        }
        freelist2_free(self.ffi_ptr, ptr as *mut c_void);
    }

    pub unsafe fn reallocate(&self, ptr: *mut u8, new_size: usize) -> *mut u8 {
        freelist2_realloc(self.ffi_ptr, ptr as *mut c_void, new_size) as *mut u8
    }

    pub unsafe fn callocate(&self, num: usize, size: usize) -> *mut u8 {
        freelist2_calloc(self.ffi_ptr, num, size) as *mut u8
    }
}

impl Drop for FreeListHeap {
    fn drop(&mut self) {
        unsafe {
            freelist2_destroy(self.ffi_ptr);
        }
    }
}

#[cfg(feature = "allocator-api")]
unsafe impl core::alloc::Allocator for FreeListHeap {
    fn allocate(&self, layout: Layout) -> Result<std::ptr::NonNull<[u8]>, core::alloc::AllocError> {
        let ptr = unsafe { self.aligned_allocate(layout.align(), layout.size()) };
        if ptr.is_null() {
            Err(core::alloc::AllocError)
        } else {
            Ok(unsafe {
                std::ptr::NonNull::new_unchecked(std::slice::from_raw_parts_mut(ptr, layout.size()))
            })
        }
    }

    unsafe fn deallocate(&self, ptr: std::ptr::NonNull<u8>, _layout: Layout) {
        self.deallocate(ptr.as_ptr());
    }
}

/// A thread-safe global static allocator wrapping FreeListHeap with standard locks.
pub struct FreeListGlobal {
    inner: Spinlock<Option<FreeListHeap>>,
}

impl FreeListGlobal {
    pub const fn empty() -> Self {
        Self {
            inner: Spinlock::new(None),
        }
    }

    pub unsafe fn init(&self, mem: &'static mut [u8]) {
        let heap = FreeListHeap::from_raw(mem);
        *self.inner.lock() = Some(heap);
    }
}

unsafe impl GlobalAlloc for FreeListGlobal {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        let lock = self.inner.lock();
        if let Some(ref heap) = *lock {
            heap.aligned_allocate(layout.align(), layout.size())
        } else {
            null_mut()
        }
    }

    unsafe fn dealloc(&self, ptr: *mut u8, _layout: Layout) {
        let lock = self.inner.lock();
        if let Some(ref heap) = *lock {
            heap.deallocate(ptr);
        }
    }

    unsafe fn realloc(&self, ptr: *mut u8, _layout: Layout, new_size: usize) -> *mut u8 {
        let lock = self.inner.lock();
        if let Some(ref heap) = *lock {
            heap.reallocate(ptr, new_size)
        } else {
            null_mut()
        }
    }
}



