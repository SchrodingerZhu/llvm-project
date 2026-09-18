function(add_benchmark_framework_library name)
  cmake_parse_arguments(
    "TEST_LIB"
    "" # No optional arguments
    "" # No single value arguments
    "SRCS;HDRS;DEPENDS" # Multi value arguments
    ${ARGN}
  )
  if(NOT TEST_LIB_SRCS)
    message(FATAL_ERROR "'add_benchmark_framework_library' requires SRCS; for "
      "header only libraries, use 'add_header_library'")
  endif()

  add_library(
    ${name}
    STATIC
    EXCLUDE_FROM_ALL
      ${TEST_LIB_SRCS}
      ${TEST_LIB_HDRS}
  )
  target_include_directories(${name} PRIVATE ${LIBC_SOURCE_DIR})
  if(TARGET libc.src.time.clock)
    target_compile_definitions(${name} PRIVATE TARGET_SUPPORTS_CLOCK)
  endif()

  _get_hermetic_test_compile_options(compile_options "" "")
  target_include_directories(${name} PRIVATE ${LIBC_INCLUDE_DIR})
  target_compile_options(${name} PRIVATE ${compile_options} -nostdinc++)

  add_dependencies(${name} ${TEST_LIB_DEPENDS})
endfunction()

