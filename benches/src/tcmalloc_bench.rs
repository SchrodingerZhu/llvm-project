//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

use crate::{ARENA_ALLOCATORS, NamedAllocator, touch_the_whole_heap};
use crate::empirical::EmpiricalData;
use std::time::Instant;
use std::io::Write;

const STEADY_STATE_MEM: usize = 8 * 1024 * 1024; // 8 MiB target live memory
const REPLAY_OPS: usize = 100000;                // 100,000 steps trace length
const TRIALS: usize = 5;

/// The generic reusable entry point for running a specific TCMalloc distribution benchmark sweep
pub fn run_tcmalloc_benchmark(csv_filename: &str) {
    println!("=========================================================================");
    println!("  TCMALLOC REPLAY BENCHMARK: {} (Trace: 100k actions)", csv_filename);
    println!("=========================================================================\n");

    // Resolve the path dynamically relative to the benches crate root
    let manifest_dir = std::path::PathBuf::from(std::env::var("CARGO_MANIFEST_DIR").unwrap());
    let csv_path = manifest_dir.join(format!("distributions/{}", csv_filename));
    let csv_path_str = csv_path.to_str().unwrap();

    // 1. Parse the empirical allocation dataset entries
    let entries = EmpiricalData::parse_csv_file(csv_path_str);
    println!("Successfully loaded {} size distribution entries from local file.", entries.len());
    println!("Target steady-state active memory: {} MB", STEADY_STATE_MEM / (1024 * 1024));
    println!("-------------------------------------------------------------------------\n");

    // Touch the backing heap static memory array to commit frames
    touch_the_whole_heap();

    let mut scores = Vec::new();

    // Benchmark each target allocator side-by-side
    for NamedAllocator { name, init_fn } in ARENA_ALLOCATORS {
        println!("Benchmarking: {} ...", name);

        let mut total_time_ms = 0.0;
        let mut total_ops_executed = 0;

        for t in 0..TRIALS {
            unsafe {
                // Initialize clean global allocator instances
                let allocator = (init_fn)();
                let allocator_ref = Box::leak(allocator) as &'static dyn std::alloc::GlobalAlloc;

                // Create simulation state and pre-record trace steps
                let mut simulator = EmpiricalData::new(42 + t as u64, &entries, STEADY_STATE_MEM, allocator_ref);
                simulator.record_trace(REPLAY_OPS);
                let actual_ops = simulator.trace_len();

                // High-precision timing loop
                let start = Instant::now();
                simulator.replay_trace();
                let elapsed = start.elapsed().as_secs_f64() * 1000.0;

                total_time_ms += elapsed;
                total_ops_executed += actual_ops;

                // Clean up references and memory
                simulator.cleanup();
                let _ = Box::from_raw(allocator_ref as *const _ as *mut dyn std::alloc::GlobalAlloc);
            }
        }

        let avg_time_ms = total_time_ms / TRIALS as f64;
        let avg_ops = total_ops_executed / TRIALS;
        let mops = (avg_ops as f64 / 1000000.0) / (avg_time_ms / 1000.0);

        println!("  Avg Time: {:6.2} ms | Total Operations: {:6} | Throughput: {:8.3} MOps", 
                 avg_time_ms, avg_ops, mops);
        scores.push(mops);
    }

    // Write results dynamically to tcmalloc-replay.csv
    let results_dir = manifest_dir.join("../results");
    let _ = std::fs::create_dir_all(&results_dir);
    let results_file = results_dir.join("tcmalloc-replay.csv");

    // If the file does not exist, create it and write the allocator names header
    if !results_file.exists() {
        let header = format!(
            "Workload,{}\n",
            ARENA_ALLOCATORS
                .iter()
                .map(|a| a.name)
                .collect::<Vec<_>>()
                .join(",")
        );
        let _ = std::fs::write(&results_file, header);
    }

    // Append the new workload row
    let row = format!(
        "{},{}\n",
        csv_filename.replace(".csv", ""),
        scores
            .iter()
            .map(|s| format!("{:.3}", s))
            .collect::<Vec<_>>()
            .join(",")
    );
    if let Ok(mut file) = std::fs::OpenOptions::new().append(true).open(&results_file) {
        let _ = file.write_all(row.as_bytes());
    }

    println!("\n=========================================================================\n");
}
