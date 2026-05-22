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

# Runs individual TCMalloc workload variants:
tcmalloc-0:
    cargo run -p benches --bin tcmalloc_0 --release
tcmalloc-1:
    cargo run -p benches --bin tcmalloc_1 --release
tcmalloc-2:
    cargo run -p benches --bin tcmalloc_2 --release
tcmalloc-3:
    cargo run -p benches --bin tcmalloc_3 --release
tcmalloc-4:
    cargo run -p benches --bin tcmalloc_4 --release
tcmalloc-5:
    cargo run -p benches --bin tcmalloc_5 --release
tcmalloc-6:
    cargo run -p benches --bin tcmalloc_6 --release
tcmalloc-7:
    cargo run -p benches --bin tcmalloc_7 --release
tcmalloc-8:
    cargo run -p benches --bin tcmalloc_8 --release
tcmalloc-9:
    cargo run -p benches --bin tcmalloc_9 --release

# Runs the complete benchmark suite, including all 10 TCMalloc variants sequentially!
bench-all:
    @rm -f results/tcmalloc-replay.csv
    @just random-actions
    @just microbench
    @just heap-efficiency
    @just tcmalloc-0
    @just tcmalloc-1
    @just tcmalloc-2
    @just tcmalloc-3
    @just tcmalloc-4
    @just tcmalloc-5
    @just tcmalloc-6
    @just tcmalloc-7
    @just tcmalloc-8
    @just tcmalloc-9

# Starts the cargo-fuzz LibFuzzer target
fuzz:
    cargo fuzz run flat_tlsf_fuzz
