add_header_library(
  dispatch
  HDRS
    dispatch.h
  DEPENDS
    .common
    libc.src.__support.common
)

add_header_library(
  common
  HDRS
    common.h
  DEPENDS
    libc.src.__support.builtin_wrappers
)