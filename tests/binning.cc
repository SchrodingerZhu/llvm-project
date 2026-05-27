#include <algorithm>
#include <array>
#include <limits>
#include <optional>

#include "flat_tlsf/flat_tlsf.h"
#include "gtest/gtest.h"

namespace flat_tlsf {
namespace {

template <typename F>
size_t find_binning_boundary(uint32_t next_bin, size_t base, size_t end,
                             F &&size_to_bin) {
  while (base < end) {
    size_t mid = base + (end - base) / 2;
    if (size_to_bin(mid) >= next_bin)
      end = mid;
    else
      base = mid + 1;
  }
  return base;
}

template <typename F1, typename F2>
void find_binning_boundaries(size_t start_from_size,
                             std::optional<uint32_t> stop_at_bin,
                             F1 &&size_to_bin, F2 &&bin_boundary_callback) {
  size_t prev_size = start_from_size;
  size_t size = start_from_size;
  size_t increment = 1;

  std::optional<uint32_t> prev_bin;

  while (true) {
    uint32_t bin = size_to_bin(size);

    if (!prev_bin.has_value() || prev_bin.value() != bin) {
      if (prev_bin.has_value())
        size = find_binning_boundary(prev_bin.value() + 1, prev_size, size,
                                     size_to_bin);

      bin_boundary_callback(bin, size);

      increment = std::max<size_t>((size - prev_size) / 4, 1);
      prev_size = size;

      prev_bin = bin;
    }

    if (size != std::numeric_limits<size_t>::max()) {
      if (size <= std::numeric_limits<size_t>::max() - increment)
        size += increment;
      else
        size = std::numeric_limits<size_t>::max();
    } else {
      break;
    }

    if (stop_at_bin.has_value() && stop_at_bin.value() == bin)
      break;
  }
}

template <typename F>
void check_binning_properties(std::optional<uint32_t> stop_at_bin,
                              F &&size_to_bin) {
  std::optional<uint32_t> prev_bin;

  auto callback = [&](uint32_t bin, size_t size) {
    if (prev_bin.has_value())
      EXPECT_EQ(prev_bin.value() + 1, bin);
    prev_bin = bin;

    EXPECT_TRUE(
        !stop_at_bin.has_value() || bin <= stop_at_bin.value() ||
        (bin == std::numeric_limits<uint32_t>::max() && size < CHUNK_UNIT));
  };

  find_binning_boundaries(CHUNK_UNIT - 1, stop_at_bin, size_to_bin, callback);
}

TEST(FlatTlsfTest, CheckFindBinningBoundary) {
  std::array<uint32_t, 12> size_to_bin = {0, 1, 1, 1, 2, 2, 2, 2, 2, 2, 3, 5};
  auto size_to_bin_fn = [&](size_t s) { return size_to_bin[s]; };

  EXPECT_EQ(find_binning_boundary(2, 1, 3, size_to_bin_fn), 3);
  EXPECT_EQ(find_binning_boundary(2, 3, 5, size_to_bin_fn), 4);
  EXPECT_EQ(find_binning_boundary(2, 2, 4, size_to_bin_fn), 4);
  EXPECT_EQ(find_binning_boundary(2, 4, 6, size_to_bin_fn), 4);
  EXPECT_EQ(find_binning_boundary(2, 5, 7, size_to_bin_fn), 5);

  EXPECT_EQ(find_binning_boundary(2, 2, 11, size_to_bin_fn), 4);
  EXPECT_EQ(find_binning_boundary(2, 0, 7, size_to_bin_fn), 4);

  EXPECT_EQ(find_binning_boundary(4, 0, 11, size_to_bin_fn), 11);
}

TEST(FlatTlsfTest, CheckFindBinningBoundaries) {
  std::array<uint32_t, 12> size_to_bin = {0, 1, 1, 1, 2, 2, 2, 2, 2, 2, 3, 3};
  std::array<size_t, 4> boundary_sizes = {0, 1, 4, 10};

  size_t i = 0;
  auto verifier = [&](uint32_t bin, size_t size) {
    ASSERT_LT(i, boundary_sizes.size());
    EXPECT_EQ(size, boundary_sizes[i]);
    EXPECT_EQ(bin, size_to_bin[size]);
    i++;
  };

  auto size_to_bin_fn = [&](size_t s) { return size_to_bin[s]; };

  find_binning_boundaries(0, 3, size_to_bin_fn, verifier);
  EXPECT_EQ(i, 4);
}

TEST(FlatTlsfTest, TestLinearExtentThenLinearlyDividedExponentialBinning) {
  auto size_to_bin_fn = [](size_t size) {
    return Binning::linear_extend_then_linearly_divided_expotential_binning<8,
                                                                            4>(
        size);
  };
  check_binning_properties(std::nullopt, size_to_bin_fn);
}

} // namespace
} // namespace flat_tlsf
