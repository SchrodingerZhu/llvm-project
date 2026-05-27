add_executable(flat_tlsf_test
  binning.cc
  bitfield.cc
)

target_link_libraries(flat_tlsf_test
  PRIVATE
    flat_tlsf
    gtest_main
)

# Discover tests
include(GoogleTest)
gtest_discover_tests(flat_tlsf_test)
