//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#![allow(clippy::missing_safety_doc, clippy::result_unit_err, dead_code)]

pub mod empirical;

use std::{
    alloc::{GlobalAlloc, Layout},
    ptr::NonNull,
};

use spinning_top::{RawSpinlock, lock_api::Mutex};

pub const HEAP_SIZE: usize = 1 << 27; // 128 MiB large static heap
#[repr(align(64))]
pub struct Heap(pub [u8; HEAP_SIZE]);
pub static mut HEAP: Heap = Heap([0u8; HEAP_SIZE]);

pub fn touch_the_whole_heap() {
    for i in (0..HEAP_SIZE).step_by(1024) {
        unsafe {
            let ptr = &raw mut HEAP.0[i];
            ptr.write_volatile(0xab);
            ptr.read_volatile();
        }
    }
}

pub struct NamedAllocator {
    pub name: &'static str,
    pub init_fn: unsafe fn() -> Box<dyn GlobalAlloc + Sync>,
}

pub const ARENA_ALLOCATORS: &[NamedAllocator] = &[
    NamedAllocator { name: "DLmalloc", init_fn: init_dlmalloc },
    NamedAllocator { name: "Talc", init_fn: init_talc },
    NamedAllocator { name: "FlatTlsf (FFI)", init_fn: init_flat_tlsf },
    NamedAllocator { name: "FreeList (FFI)", init_fn: init_freelist },
    NamedAllocator { name: "RLSF", init_fn: init_rlsf },
    NamedAllocator { name: "Galloc", init_fn: init_galloc },
    NamedAllocator { name: "Buddy Alloc", init_fn: init_buddy_alloc },
];

pub const SYSTEM_ALLOCATORS: &[NamedAllocator] = &[
    NamedAllocator { name: "DLmalloc", init_fn: init_dlmalloc_sys },
    NamedAllocator { name: "mimalloc", init_fn: init_mimalloc_sys },
    NamedAllocator { name: "System", init_fn: init_system },
    NamedAllocator { name: "Jemalloc", init_fn: init_jemalloc_sys },
];

/// Bias towards smaller values over larger ones.
pub fn generate_size(max: usize) -> usize {
    let cap = fastrand::usize(16..max);
    fastrand::usize(4..cap)
}

/// Strongly bias towards low alignment requirements.
pub fn generate_align() -> usize {
    align_of::<usize>() << (fastrand::u16(..).trailing_zeros() / 2)
}

pub struct AllocationWrapper<'a> {
    pub ptr: *mut u8,
    pub layout: Layout,
    pub allocator: &'a dyn GlobalAlloc,
}

impl<'a> AllocationWrapper<'a> {
    pub fn new(size: usize, align: usize, allocator: &'a dyn GlobalAlloc) -> Option<Self> {
        let layout = Layout::from_size_align(size, align).unwrap();
        let ptr = unsafe { (*allocator).alloc(layout) };
        if ptr.is_null() {
            return None;
        }
        Some(Self { ptr, layout, allocator })
    }

    pub fn realloc(&mut self, new_size: usize) -> Result<(), ()> {
        let new_ptr = unsafe { (*self.allocator).realloc(self.ptr, self.layout, new_size) };
        if new_ptr.is_null() {
            return Err(());
        }
        self.ptr = new_ptr;
        self.layout = Layout::from_size_align(new_size, self.layout.align()).unwrap();
        Ok(())
    }
}

impl<'a> Drop for AllocationWrapper<'a> {
    fn drop(&mut self) {
        unsafe { (*self.allocator).dealloc(self.ptr, self.layout) }
    }
}

static FLAT_TLSF_GLOBAL: flat_tlsf::FlatTlsfGlobal = flat_tlsf::FlatTlsfGlobal::empty();
static FREELIST_GLOBAL: flat_tlsf::FreeListGlobal = flat_tlsf::FreeListGlobal::empty();

unsafe fn init_flat_tlsf() -> Box<dyn GlobalAlloc + Sync> {
    let slice = std::slice::from_raw_parts_mut(&raw mut HEAP.0 as *mut u8, HEAP_SIZE);
    FLAT_TLSF_GLOBAL.init(slice);
    Box::new(RefAllocator(&FLAT_TLSF_GLOBAL))
}

unsafe fn init_freelist() -> Box<dyn GlobalAlloc + Sync> {
    let slice = std::slice::from_raw_parts_mut(&raw mut HEAP.0 as *mut u8, HEAP_SIZE);
    FREELIST_GLOBAL.init(slice);
    Box::new(RefAllocator(&FREELIST_GLOBAL))
}

struct RefAllocator<T: 'static>(&'static T);
unsafe impl<T: GlobalAlloc> GlobalAlloc for RefAllocator<T> {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        self.0.alloc(layout)
    }
    unsafe fn dealloc(&self, ptr: *mut u8, layout: Layout) {
        self.0.dealloc(ptr, layout)
    }
    unsafe fn realloc(&self, ptr: *mut u8, layout: Layout, new_size: usize) -> *mut u8 {
        self.0.realloc(ptr, layout, new_size)
    }
}

unsafe fn init_talc() -> Box<dyn GlobalAlloc + Sync> {
    use talc::{TalcLock, source::Manual};
    let talc: TalcLock<RawSpinlock, _> = TalcLock::new(Manual);
    talc.lock().claim((&raw mut HEAP.0).cast(), HEAP_SIZE).unwrap();
    Box::new(talc)
}

unsafe fn init_system() -> Box<dyn GlobalAlloc + Sync> {
    Box::new(std::alloc::System)
}

unsafe fn init_galloc() -> Box<dyn GlobalAlloc + Sync> {
    let galloc = good_memory_allocator::SpinLockedAllocator::<
        { good_memory_allocator::DEFAULT_SMALLBINS_AMOUNT },
        { good_memory_allocator::DEFAULT_ALIGNMENT_SUB_BINS_AMOUNT },
    >::empty();
    let boxed_galloc = Box::new(galloc);
    boxed_galloc.init(&raw mut HEAP as usize, HEAP_SIZE);
    boxed_galloc
}

unsafe fn init_rlsf() -> Box<dyn GlobalAlloc + Sync> {
    let tlsf = GlobalRLSF(Mutex::new(rlsf::Tlsf::new()));
    let slice = unsafe { std::slice::from_raw_parts_mut(&raw mut HEAP.0 as *mut u8 as *mut std::mem::MaybeUninit<u8>, HEAP_SIZE) };
    tlsf.0.lock().insert_free_block(slice);
    Box::new(tlsf)
}

unsafe fn init_buddy_alloc() -> Box<dyn GlobalAlloc + Sync> {
    use buddy_alloc::{BuddyAllocParam, FastAllocParam, NonThreadsafeAlloc};
    let ba = BuddyAllocWrapper(Mutex::new(NonThreadsafeAlloc::new(
        FastAllocParam::new((&raw mut HEAP).cast(), HEAP_SIZE / 8),
        BuddyAllocParam::new(
            (&raw mut HEAP).cast::<u8>().add(HEAP_SIZE / 8),
            HEAP_SIZE / 8 * 7,
            64,
        ),
    )));
    Box::new(ba)
}

unsafe fn init_dlmalloc() -> Box<dyn GlobalAlloc + Sync> {
    let dl = DlMallocator(Mutex::new(dlmalloc::Dlmalloc::new_with_allocator(DlmallocArena(
        std::sync::atomic::AtomicUsize::new(0),
    ))));
    Box::new(dl)
}

struct BuddyAllocWrapper(pub Mutex<RawSpinlock, buddy_alloc::NonThreadsafeAlloc>);
unsafe impl Send for BuddyAllocWrapper {}
unsafe impl Sync for BuddyAllocWrapper {}
unsafe impl GlobalAlloc for BuddyAllocWrapper {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        self.0.lock().alloc(layout)
    }
    unsafe fn dealloc(&self, ptr: *mut u8, layout: Layout) {
        self.0.lock().dealloc(ptr, layout)
    }
    unsafe fn alloc_zeroed(&self, layout: Layout) -> *mut u8 {
        self.0.lock().alloc_zeroed(layout)
    }
    unsafe fn realloc(&self, ptr: *mut u8, layout: Layout, new_size: usize) -> *mut u8 {
        self.0.lock().realloc(ptr, layout, new_size)
    }
}

struct DlMallocator(Mutex<RawSpinlock, dlmalloc::Dlmalloc<DlmallocArena>>);
unsafe impl GlobalAlloc for DlMallocator {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        self.0.lock().malloc(layout.size(), layout.align())
    }
    unsafe fn dealloc(&self, ptr: *mut u8, layout: Layout) {
        self.0.lock().free(ptr, layout.size(), layout.align());
    }
    unsafe fn realloc(&self, ptr: *mut u8, layout: Layout, new_size: usize) -> *mut u8 {
        self.0.lock().realloc(ptr, layout.size(), layout.align(), new_size)
    }
    unsafe fn alloc_zeroed(&self, layout: Layout) -> *mut u8 {
        self.0.lock().calloc(layout.size(), layout.align())
    }
}

struct DlmallocArena(std::sync::atomic::AtomicUsize);
unsafe impl dlmalloc::Allocator for DlmallocArena {
    fn alloc(&self, size: usize) -> (*mut u8, usize, u32) {
        // Round up size to standard page alignment (4096 bytes)
        let size = (size + 4095) & !4095;
        let mut current = self.0.load(core::sync::atomic::Ordering::SeqCst);
        loop {
            if current + size > HEAP_SIZE {
                return (core::ptr::null_mut(), 0, 0);
            }
            match self.0.compare_exchange_weak(
                current,
                current + size,
                core::sync::atomic::Ordering::SeqCst,
                core::sync::atomic::Ordering::SeqCst,
            ) {
                Ok(_) => {
                    let ptr = unsafe { (&raw mut HEAP.0[0] as *mut u8).add(current) };
                    return (ptr, size, 1);
                }
                Err(actual) => current = actual,
            }
        }
    }
    fn remap(&self, _ptr: *mut u8, _oldsize: usize, _newsize: usize, _can_move: bool) -> *mut u8 {
        unimplemented!()
    }
    fn free_part(&self, _ptr: *mut u8, _oldsize: usize, _newsize: usize) -> bool {
        unimplemented!()
    }
    fn free(&self, _ptr: *mut u8, _size: usize) -> bool {
        true
    }
    fn can_release_part(&self, _flags: u32) -> bool {
        false
    }
    fn allocates_zeros(&self) -> bool {
        false
    }
    fn page_size(&self) -> usize {
        4 * 1024
    }
}

struct GlobalRLSF<'p>(
    Mutex<
        RawSpinlock,
        rlsf::Tlsf<'p, usize, usize, { usize::BITS as usize - 12 }, { usize::BITS as _ }>,
    >,
);
unsafe impl<'a> GlobalAlloc for GlobalRLSF<'a> {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        self.0.lock().allocate(layout).map_or(std::ptr::null_mut(), |nn| nn.as_ptr())
    }
    unsafe fn dealloc(&self, ptr: *mut u8, layout: Layout) {
        self.0.lock().deallocate(NonNull::new_unchecked(ptr), layout.align());
    }
    unsafe fn realloc(&self, ptr: *mut u8, layout: Layout, new_size: usize) -> *mut u8 {
        self.0
            .lock()
            .reallocate(
                NonNull::new_unchecked(ptr),
                Layout::from_size_align_unchecked(new_size, layout.align()),
            )
            .map_or(std::ptr::null_mut(), |nn| nn.as_ptr())
    }
}

unsafe fn init_dlmalloc_sys() -> Box<dyn GlobalAlloc + Sync> {
    Box::new(dlmalloc::GlobalDlmalloc)
}
unsafe fn init_jemalloc_sys() -> Box<dyn GlobalAlloc + Sync> {
    Box::new(jemallocator::Jemalloc)
}
unsafe fn init_mimalloc_sys() -> Box<dyn GlobalAlloc + Sync> {
    Box::new(mimalloc::MiMalloc)
}
