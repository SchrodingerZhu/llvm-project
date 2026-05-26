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

    let generate_flamegraph = std::env::var("GENERATE_FLAMEGRAPH").is_ok();
    let trials = if generate_flamegraph { 1 } else { TRIALS };
    let profile_loop_count = if generate_flamegraph { 150 } else { 1 };
    let workload_index = csv_filename.replace("TCMALLOC_", "").replace(".csv", "");

    if generate_flamegraph {
        let framegraphs_dir = manifest_dir.join("../results/flamegraphs");
        let _ = std::fs::create_dir_all(&framegraphs_dir);
    }

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
        if let Ok(only_env) = std::env::var("ONLY_ALLOCATOR") {
            let targets: Vec<&str> = only_env.split(',').map(|s| s.trim()).collect();
            if !targets.contains(&name) {
                continue;
            }
        }
        println!("Benchmarking: {} ...", name);

        let safe_allocator_name = name.replace(" (FFI)", "").replace(" ", "_").to_lowercase();
        let flamegraph_output_path = manifest_dir.join(format!("../results/flamegraphs/tcmalloc_{}_{}.svg", safe_allocator_name, workload_index));

        let mut total_time_ms = 0.0;
        let mut total_ops_executed = 0;

        for t in 0..trials {
            unsafe {
                // Initialize clean global allocator instances
                let allocator = (init_fn)();
                let allocator_ref = Box::leak(allocator) as &'static dyn std::alloc::GlobalAlloc;

                // Create simulation state and pre-record trace steps
                let mut simulator = EmpiricalData::new(42 + t as u64, &entries, STEADY_STATE_MEM, allocator_ref);
                simulator.record_trace(REPLAY_OPS);
                let actual_ops = simulator.trace_len();

                let mut _profiler_guard = None;
                if t == 0 && generate_flamegraph {
                    _profiler_guard = Some(pprof::ProfilerGuardBuilder::default()
                        .frequency(997)
                        .build()
                        .unwrap());
                }

                // High-precision timing loop with safety recreation when profiling
                let mut total_replay_time_ms = 0.0;

                if generate_flamegraph {
                    for _ in 0..profile_loop_count {
                        let mut temp_sim = EmpiricalData::new(42 + t as u64, &entries, STEADY_STATE_MEM, allocator_ref);
                        temp_sim.trace = simulator.trace.clone();
                        
                        let start = Instant::now();
                        temp_sim.replay_trace();
                        let elapsed = start.elapsed().as_secs_f64() * 1000.0;
                        total_replay_time_ms += elapsed;
                        
                        temp_sim.cleanup();
                    }
                } else {
                    let start = Instant::now();
                    simulator.replay_trace();
                    let elapsed = start.elapsed().as_secs_f64() * 1000.0;
                    total_replay_time_ms = elapsed;
                }

                if let Some(guard) = _profiler_guard.take() {
                    if let Ok(report) = guard.report().build() {
                        let mut file = std::fs::File::create(&flamegraph_output_path).unwrap();
                        if let Err(e) = report.flamegraph(&mut file) {
                            eprintln!("Failed to generate flamegraph: {:?}", e);
                        } else {
                            println!("Generated flamegraph at {:?}", flamegraph_output_path);
                        }
                    }
                }

                let mut elapsed = total_replay_time_ms;
                if generate_flamegraph {
                    elapsed /= profile_loop_count as f64;
                }

                total_time_ms += elapsed;
                total_ops_executed += actual_ops;

                // Clean up references and memory
                simulator.cleanup();
                let _ = Box::from_raw(allocator_ref as *const _ as *mut dyn std::alloc::GlobalAlloc);
            }
        }

        let avg_time_ms = total_time_ms / trials as f64;
        let avg_ops = total_ops_executed / trials;
        let mops = (avg_ops as f64 / 1000000.0) / (avg_time_ms / 1000.0);

        println!("  Avg Time: {:6.2} ms | Total Operations: {:6} | Throughput: {:8.3} MOps", 
                 avg_time_ms, avg_ops, mops);
        scores.push(mops);
    }

    if std::env::var("ONLY_ALLOCATOR").is_err() {
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
    }

    println!("\n=========================================================================\n");
}
