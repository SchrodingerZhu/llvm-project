#!/usr/bin/env just --justfile

default:
    @just --list

# Cargo checks all targets
check:
    cargo check --workspace --all-targets

# Runs library unit tests
test:
    cargo test -p flat_tlsf --all-targets

# Runs the random-actions benchmark
random-actions:
    cargo run -p benches --bin random_actions --release -- --name "random-actions"

# Runs the sequential/churn cycle microbenchmarks
microbench:
    cargo run -p benches --bin microbench --release

# Runs the dynamic heap space efficiency study
heap-efficiency:
    cargo run -p benches --bin heap_efficiency --release

# Runs the TCMalloc empirical size distribution replay study
tcmalloc-replay:
    cargo run -p benches --bin tcmalloc_replay --release

# Starts the cargo-fuzz LibFuzzer target
fuzz:
    cargo fuzz run flat_tlsf_fuzz
