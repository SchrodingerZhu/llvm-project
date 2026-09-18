add_custom_target(gpu-benchmark)

function(add_benchmark benchmark_name)
  cmake_parse_arguments(
    "BENCHMARK"
    "" # Optional arguments
    "" # Single value arguments
    "LINK_LIBRARIES;DEPENDS" # Multi-value arguments
    ${ARGN}
  )

  if(NOT libc.src.time.clock IN_LIST TARGET_LLVMLIBC_ENTRYPOINTS)
    message(FATAL_ERROR "target does not support clock")
  endif()
  add_libc_hermetic(
    ${benchmark_name}
    IS_BENCHMARK
    LINK_LIBRARIES
      LibcGpuBenchmark
      ${BENCHMARK_LINK_LIBRARIES}
    DEPENDS
      libc.src.stdio.printf
      ${BENCHMARK_DEPENDS}
      ${BENCHMARK_UNPARSED_ARGUMENTS}
    COMPILE_OPTIONS
      -flto
  )
  get_fq_target_name(${benchmark_name} fq_target_name)
  set(fq_build_target_name ${fq_target_name}.__build__)

  add_dependencies(gpu-benchmark ${fq_target_name})
endfunction()

add_benchmark_framework_library(
  LibcGpuBenchmark
  SRCS
    LibcGpuBenchmark.cpp
    ../BenchmarkMain.cpp
  HDRS
    LibcGpuBenchmark.h
    Random.h
  DEPENDS
    libc.benchmarks.benchmark
    libc.benchmarks.timing.timing
    libc.hdr.stdint_proxy
    libc.src.__support.CPP.string
    libc.src.__support.CPP.string_view
    libc.src.__support.CPP.type_traits
    libc.src.__support.CPP.algorithm
    libc.src.__support.CPP.atomic
    libc.src.__support.CPP.array
    libc.src.__support.CPP.optional
    libc.src.__support.FPUtil.fp_bits
    libc.src.__support.FPUtil.sqrt
    libc.src.__support.sign
    libc.src.__support.fixedvector
    libc.src.__support.GPU.utils
    libc.src.__support.macros.attributes
    libc.src.__support.macros.config
    libc.src.__support.macros.properties.types
    libc.src.stdio.printf
)

target_link_libraries(LibcGpuBenchmark PRIVATE LibcBenchmark)

add_subdirectory(src)
