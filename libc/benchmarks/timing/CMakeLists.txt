if(LIBC_TARGET_OS_IS_GPU)
  set(timing_targets nvptx amdgpu)
elseif(LIBC_TARGET_ARCHITECTURE_IS_ARM)
  set(timing_targets cortex_m)
endif()

foreach(target IN LISTS timing_targets)
  add_subdirectory(${target})
  list(APPEND target_timing libc.benchmarks.timing.${target}.${target}_timing)
endforeach()

add_header_library(
  timing
  HDRS
    timing.h
  DEPENDS
    libc.src.__support.macros.properties.architectures
    ${target_timing}
)
