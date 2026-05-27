cmake_minimum_required(VERSION 3.14)
project(flat_tlsf VERSION 0.1.0 LANGUAGES CXX)

# Force C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Standard CMake options
option(BUILD_TESTING "Build tests" ON)

# Define header-only library
add_library(flat_tlsf INTERFACE)

target_include_directories(flat_tlsf INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

# Enable testing if requested
if(BUILD_TESTING)
  enable_testing()
  
  # Fetch GTest
  include(FetchContent)
  
  # Avoid timestamp warning with CMake 3.24+
  if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
  endif()

  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  )
  # For Windows: Prevent overriding the parent project's compiler/linker settings
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
  
  add_subdirectory(tests)
endif()
