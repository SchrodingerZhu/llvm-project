# flat_tlsf

A standard C++20 header-only project template with Google Test (gtest) integration using CMake.

## Structure

*   `include/flat_tlsf/`: Public headers (header-only library).
*   `tests/`: Unit tests using Google Test.
*   `build/`: Build directory (ignored by git).

## Prerequisites

*   CMake 3.14 or higher
*   A C++20 compatible compiler (e.g., GCC 12+, Clang 17+, MSVC 2022+)

## Using the Library

As a header-only library, you can simply include the headers in your project. If you are using CMake, you can link against the `flat_tlsf` interface target.

## Building and Running Tests

We use CMake for building the tests. It is recommended to perform an out-of-source build:

```bash
# Configure the project (creates the build directory)
cmake -B build -S .

# Build the tests
cmake --build build

# Run all tests
ctest --test-dir build --output-on-failure
```

Alternatively, you can run the test executable directly:

```bash
./build/tests/flat_tlsf_test
```

## Code Formatting

This project uses `clang-format` with Google style. To format your code:

```bash
# Format a file
clang-format -i include/flat_tlsf/flat_tlsf.h tests/flat_tlsf_test.cc
```
