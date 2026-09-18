add_custom_target(libc-baremetal-benchmarks)

function(add_baremetal_benchmark benchmark_name)
  add_libc_benchmark(
    ${benchmark_name}
    SUITE libc-baremetal-benchmarks
    LINK_LIBRARIES LibcBaremetalBenchmark
    ${ARGN}
  )
endfunction()

add_benchmark_framework_library(
  LibcBaremetalBenchmark
  SRCS
    LibcBaremetalBenchmark.cpp
    ../BenchmarkMain.cpp
  DEPENDS
    libc.benchmarks.benchmark
    libc.src.stdio.printf
)

target_link_libraries(LibcBaremetalBenchmark PRIVATE LibcBenchmark)
