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

function(add_baremetal_benchmarks)
  include(CheckCXXSourceCompiles)
  set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
  string(JOIN " " CMAKE_REQUIRED_FLAGS ${LIBC_COMPILE_OPTIONS_DEFAULT})
  set(CMAKE_REQUIRED_INCLUDES ${LIBC_SOURCE_DIR})
  set(CMAKE_REQUIRED_DEFINITIONS
    -DLIBC_FULL_BUILD -DLIBC_NAMESPACE=${LIBC_NAMESPACE})
  check_cxx_source_compiles(
    "#include \"benchmarks/timing/timing.h\"
int main() { return 0; }"
    LIBC_BAREMETAL_BENCHMARK_HAS_TIMING)
  if(NOT LIBC_BAREMETAL_BENCHMARK_HAS_TIMING)
    return()
  endif()
  if(LIBC_CONF_PRINTF_DISABLE_FLOAT)
    message(FATAL_ERROR
      "Baremetal benchmarks require -DLIBC_CONF_PRINTF_DISABLE_FLOAT=OFF")
  endif()

  add_baremetal_benchmark(
    isalpha_benchmark
    SRCS ${LIBC_SOURCE_DIR}/benchmarks/src/ctype/isalpha_benchmark.cpp
    DEPENDS
      libc.benchmarks.benchmark
      libc.benchmarks.timing.timing
      libc.src.ctype.isalpha
  )
  foreach(function memcpy memmove memset memcmp bcmp bzero strlen strcmp memchr strchr)
    if(function MATCHES "^(strlen|strcmp|memchr|strchr)$")
      set(source string_benchmark.cpp)
    else()
      set(source memory_benchmark.cpp)
    endif()
    if(function MATCHES "^(bcmp|bzero)$")
      set(header strings)
    else()
      set(header string)
    endif()
    string(TOUPPER ${function} upper_function)
    add_baremetal_benchmark(
      ${function}_benchmark
      SRCS ${LIBC_SOURCE_DIR}/benchmarks/src/string/${source}
      HDRS ${LIBC_SOURCE_DIR}/benchmarks/src/string/StringBenchmark.h
      COMPILE_OPTIONS -DLIBC_BENCHMARK_${upper_function}
      DEPENDS
        libc.benchmarks.benchmark
        libc.benchmarks.timing.timing
        libc.src.${header}.${function}
        libc.src.stdlib.aligned_alloc
        libc.src.stdlib.free
        libc.src.stdlib.exit
        libc.src.stdio.printf
    )
  endforeach()
endfunction()

add_baremetal_benchmarks()
