#pragma once

#include <cstddef>
#include <vector>

struct BodySoA {
  size_t n = 0;
  size_t n_padded = 0;
  std::vector<float> x;
  std::vector<float> y;
  std::vector<float> vx;
  std::vector<float> vy;
  std::vector<float> ax;
  std::vector<float> ay;
  std::vector<float> m;

  explicit BodySoA(size_t n_, size_t n_padded_ = 0);
};
