//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

use benches::{ARENA_ALLOCATORS, NamedAllocator, touch_the_whole_heap};
use benches::empirical::EmpiricalData;
use std::time::Instant;

const STEADY_STATE_MEM: usize = 8 * 1024 * 1024; // 8 MiB target live memory
const REPLAY_OPS: usize = 100000;                // 100,000 steps trace length
const TRIALS: usize = 5;

fn main() {
    println!("=========================================================================");
    println!("  TCMALLOC EMPIRICAL DISTRIBUTION REPLAY BENCHMARK (Trace: 100k actions)");
    println!("=========================================================================\n");

    // Dynamically build the path to the vendored distributions folder inside the benches crate
    let manifest_dir = std::path::PathBuf::from(std::env::var("CARGO_MANIFEST_DIR").unwrap());
    let csv_path = manifest_dir.join("distributions/TCMALLOC_0.csv");
    let csv_path_str = csv_path.to_str().unwrap();

    // 1. Parse the empirical allocation dataset entries
    let entries = EmpiricalData::parse_csv_file(csv_path_str);
    println!("Successfully loaded {} size distribution entries from: {}", entries.len(), csv_path_str);
    println!("Target steady-state active memory: {} MB", STEADY_STATE_MEM / (1024 * 1024));
    println!("-------------------------------------------------------------------------\n");

    // Touch the static backing heap once to commit physical memory frames
    touch_the_whole_heap();

    // Benchmark each allocator side-by-side
    for NamedAllocator { name, init_fn } in ARENA_ALLOCATORS {
        println!("Benchmarking: {} ...", name);

        let mut total_time_ms = 0.0;
        let mut total_ops_executed = 0;

        for t in 0..TRIALS {
            unsafe {
                // Initialize the allocator and claim the static heap block
                let allocator = (init_fn)();
                let allocator_ref = Box::leak(allocator) as &'static dyn std::alloc::GlobalAlloc;

                // Create the simulator (which triggers the warmup phase)
                let mut simulator = EmpiricalData::new(42 + t as u64, &entries, STEADY_STATE_MEM, allocator_ref);

                // Pre-record a high-precision flat operations trace
                simulator.record_trace(REPLAY_OPS);
                let actual_ops = simulator.trace_len();


                // Start timing the pure replay loop (no randomness, no allocations loops!)
                let start = Instant::now();
                simulator.replay_trace();
                let elapsed = start.elapsed().as_secs_f64() * 1000.0;

                total_time_ms += elapsed;
                total_ops_executed += actual_ops;

                // Clean up allocations and release the leaked static reference pointer
                simulator.cleanup();
                // Release allocator: since it was boxed, reconstruct the box to drop it safely!
                let _ = Box::from_raw(allocator_ref as *const _ as *mut dyn std::alloc::GlobalAlloc);
            }
        }

        let avg_time_ms = total_time_ms / TRIALS as f64;
        let avg_ops = total_ops_executed / TRIALS;
        let mops = (avg_ops as f64 / 1000000.0) / (avg_time_ms / 1000.0);

        println!("  Avg Time: {:6.2} ms | Total Operations: {:6} | Throughput: {:8.3} MOps", 
                 avg_time_ms, avg_ops, mops);
    }
    println!("\n=========================================================================");
}
