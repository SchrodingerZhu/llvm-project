//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

use std::alloc::{GlobalAlloc, Layout};
use std::fs::File;
use std::io::{BufRead, BufReader};


/// A discrete search probability sum-tree sampler (Dijkstra's Adjustable Sampler).
/// Exposes logarithmic-time random sampling and weight adjustments.
#[derive(Clone, Debug, PartialEq)]
pub struct AdjustableSampler {
    n: usize,
    tree: Vec<f64>,
}

impl AdjustableSampler {
    pub fn new(weights: &[f64]) -> Self {
        assert!(!weights.is_empty(), "Sampler weights list cannot be empty!");
        let n = weights.len().next_power_of_two();
        let mut tree = vec![0.0f64; 2 * n - 1];
        
        // Copy raw weights to the leaf nodes
        for (i, &w) in weights.iter().enumerate() {
            tree[n - 1 + i] = w;
        }

        // Parent nodes sum their kids up to the root
        let mut i = n - 1;
        while i > 0 {
            i -= 1;
            tree[i] = tree[2 * i + 1] + tree[2 * i + 2];
        }

        Self { n, tree }
    }

    /// Sample a random index from {0, 1, ..., N - 1} based on current weights
    pub fn sample(&self, rng: &mut fastrand::Rng) -> usize {
        let total_weight = self.tree[0];
        if total_weight <= 0.0 {
            return 0;
        }
        let mut x = rng.f64() * total_weight;
        let mut i = 0;
        
        while i < (self.n - 1) {
            let left = self.tree[2 * i + 1];
            if x <= left {
                i = 2 * i + 1;
            } else {
                x -= left;
                i = 2 * i + 2;
            }
        }
        i - (self.n - 1)
    }

    /// Adjust the weight of item i by delta
    pub fn adjust_weight(&mut self, i: usize, delta: f64) {
        let current_w = self.weight(i);
        self.set_weight(i, current_w + delta);
    }

    pub fn set_weight(&mut self, mut idx: usize, weight: f64) {
        idx += self.n - 1;
        self.tree[idx] = weight;
        while idx != 0 {
            idx = (idx - 1) / 2;
            self.tree[idx] = self.tree[2 * idx + 1] + self.tree[2 * idx + 2];
        }
    }

    pub fn weight(&self, i: usize) -> f64 {
        self.tree[i + self.n - 1]
    }

    pub fn total_weight(&self) -> f64 {
        self.tree[0]
    }
}

/// A standard size distribution point loaded from TCMALLOC CSV data
#[derive(Clone, Debug)]
pub struct EmpiricalEntry {
    pub size: usize,
    pub alloc_rate: f64,
    pub num_live: f64,
}

/// Standard flat trace action record step for cache-friendly fast replay
#[derive(Clone, Copy, Debug)]
pub enum TraceAction {
    Alloc { size_idx: usize, slot_idx: usize },
    Free { size_idx: usize, slot_idx: usize },
}

pub struct SizeState {
    pub size: usize,
    pub death_rate: f64,
    pub objs: Vec<*mut u8>,
}

pub struct EmpiricalData {
    rng: fastrand::Rng,
    allocator: &'static dyn GlobalAlloc,
    
    state: Vec<SizeState>,
    birth_sampler: AdjustableSampler,
    death_sampler: AdjustableSampler,
    
    // Trace player structures
    pub trace: Vec<TraceAction>,
    birth_pointers: Vec<*mut u8>,
    death_pointers: Vec<*mut u8>,
}

unsafe impl Send for EmpiricalData {}
unsafe impl Sync for EmpiricalData {}

impl EmpiricalData {
    pub fn new(
        seed: u64,
        entries: &[EmpiricalEntry],
        total_mem: usize,
        allocator: &'static dyn GlobalAlloc,
    ) -> Self {
        let rng = fastrand::Rng::with_seed(seed);
        
        let mut state = Vec::new();
        let mut birth_weights = Vec::new();
        let mut death_weights = Vec::new();

        for entry in entries {
            state.push(SizeState {
                size: entry.size,
                death_rate: entry.alloc_rate / entry.num_live,
                objs: Vec::new(),
            });
            birth_weights.push(entry.alloc_rate);
            death_weights.push(0.0); // Initially no live objects, so death weights are 0
        }

        let birth_sampler = AdjustableSampler::new(&birth_weights);
        let death_sampler = AdjustableSampler::new(&death_weights);

        let mut data = Self {
            rng,
            allocator,
            state,
            birth_sampler,
            death_sampler,
            trace: Vec::new(),
            birth_pointers: Vec::new(),
            death_pointers: Vec::new(),
        };

        // Warmup Phase: Allocate live targets up to target steady memory bounds
        data.warmup(total_mem);
        data
    }

    /// Allocates and populates our active sizes lists to bring heap to steady-state limit
    fn warmup(&mut self, total_mem: usize) {
        let mut current_mem = 0;
        
        while current_mem < total_mem {
            let size_idx = self.birth_sampler.sample(&mut self.rng);
            let size = self.state[size_idx].size;

            let ptr = unsafe {
                let layout = Layout::from_size_align_unchecked(size, 8);
                self.allocator.alloc(layout)
            };

            if !ptr.is_null() {
                self.state[size_idx].objs.push(ptr);
                current_mem += size;
                
                // Adjust death weight (more live objects = higher cumulative death probability!)
                let death_rate = self.state[size_idx].death_rate;
                self.death_sampler.adjust_weight(size_idx, death_rate);
            }
        }
    }

    /// Simulates steps and records a flat fast-replay action trace list
    pub fn record_trace(&mut self, total_ops: usize) {
        self.trace.clear();
        
        // Circular buffer slot index tracker for each size class
        let mut virtual_counts: Vec<usize> = self.state.iter().map(|s| s.objs.len()).collect();

        for _ in 0..total_ops {
            let total_birth = self.birth_sampler.total_weight();
            let total_death = self.death_sampler.total_weight();
            let total_sum = total_birth + total_death;

            if total_sum <= 0.0 {
                continue;
            }

            let roll = self.rng.f64() * total_sum;

            if roll < total_birth {
                // Birth operation
                let size_idx = self.birth_sampler.sample(&mut self.rng);
                let slot_idx = virtual_counts[size_idx];
                virtual_counts[size_idx] += 1;

                self.trace.push(TraceAction::Alloc { size_idx, slot_idx });

                // Adjust death probability weights
                let death_rate = self.state[size_idx].death_rate;
                self.death_sampler.adjust_weight(size_idx, death_rate);
            } else {
                // Death operation
                let size_idx = self.death_sampler.sample(&mut self.rng);
                if virtual_counts[size_idx] > 0 {
                    let slot_idx = self.rng.usize(0..virtual_counts[size_idx]);
                    virtual_counts[size_idx] -= 1;

                    self.trace.push(TraceAction::Free { size_idx, slot_idx });

                    // Adjust death weights down
                    let death_rate = self.state[size_idx].death_rate;
                    self.death_sampler.adjust_weight(size_idx, -death_rate);
                }
            }
        }
    }

    /// Replays the recorded trace sequence at raw C++ pointer speeds
    /// with absolutely zero random loops or branching overhead!
    pub unsafe fn replay_trace(&mut self) {
        // Allocate temporary birth buffers for trace tracking
        let mut active_slots: Vec<Vec<*mut u8>> = self.state.iter().map(|s| s.objs.clone()).collect();

        for action in &self.trace {
            match *action {
                TraceAction::Alloc { size_idx, slot_idx } => {
                    let size = self.state[size_idx].size;
                    let layout = Layout::from_size_align_unchecked(size, 8);
                    
                    let ptr = self.allocator.alloc(layout);
                    assert!(!ptr.is_null(), "OOM during replay loop!");

                    // Insert or push slot
                    if slot_idx < active_slots[size_idx].len() {
                        active_slots[size_idx][slot_idx] = ptr;
                    } else {
                        active_slots[size_idx].push(ptr);
                    }
                }
                TraceAction::Free { size_idx, slot_idx } => {
                    if !active_slots[size_idx].is_empty() {
                        // Swap remove to emulate the random index deletion in-place
                        let ptr = active_slots[size_idx].swap_remove(slot_idx);
                        let size = self.state[size_idx].size;
                        let layout = Layout::from_size_align_unchecked(size, 8);
                        
                        self.allocator.dealloc(ptr, layout);
                    }
                }
            }
        }

        // Clean up any extra items born in the replay that are still live
        for (size_idx, mut slots) in active_slots.into_iter().enumerate() {
            let original_count = self.state[size_idx].objs.len();
            if slots.len() > original_count {
                let size = self.state[size_idx].size;
                let layout = Layout::from_size_align_unchecked(size, 8);
                for ptr in slots.drain(original_count..) {
                    self.allocator.dealloc(ptr, layout);
                }
            }
            self.state[size_idx].objs = slots;
        }
    }

    /// Parses standard comma-separated distribution files
    pub fn parse_csv_file(csv_path: &str) -> Vec<EmpiricalEntry> {
        let f = File::open(csv_path).expect("Could not open empirical CSV file");
        let reader = BufReader::new(f);
        let mut entries = Vec::new();

        for line in reader.lines() {
            let line = line.expect("Could not read line from CSV");
            let parts: Vec<&str> = line.split(',').map(|s| s.trim()).collect();
            if parts.len() >= 3 {
                let size: usize = parts[0].parse().expect("Invalid size parsed");
                let alloc_rate: f64 = parts[1].parse().expect("Invalid alloc rate");
                let num_live: f64 = parts[2].parse().expect("Invalid num_live");
                entries.push(EmpiricalEntry {
                    size,
                    alloc_rate,
                    num_live,
                });
            }
        }
        entries
    }

    /// Clean reclaim of all remaining steady-state live allocations
    pub fn cleanup(self) {
        for state in self.state {
            let layout = unsafe { Layout::from_size_align_unchecked(state.size, 8) };
            for ptr in state.objs {
                unsafe {
                    self.allocator.dealloc(ptr, layout);
                }
            }
        }
    }

    pub fn trace_len(&self) -> usize {
        self.trace.len()
    }
}
