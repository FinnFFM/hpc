#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

#include <SFML/Graphics.hpp>

#include "body.h"
#include "body_soa.h"

extern const float G;
extern const float dt;
extern const float softening;
extern const size_t n_bodies;
extern const float center_mass;
extern const int benchmark_steps;

struct BenchmarkResult {
  int steps = 0;
  int threads = 0;
  double ms = 0.0;
  float first_x = 0.0f;
  float first_y = 0.0f;
};

void set_benchmark_result(BenchmarkResult* result);
BenchmarkResult* get_benchmark_result();

float orbital_velocity_scalar(float M, float r);
unsigned char clamp_color(float v);
std::vector<Body> init_bodies();
void init_bodies_soa(BodySoA& bodies);

template <typename T>
inline sf::Color mass_to_color(T m) {
  float mf = static_cast<float>(m);
  float norm = std::min(1.0f, mf / 50.0f);
  return sf::Color(clamp_color(255.0f * norm), 50, clamp_color(255.0f * (1 - norm)));
}
