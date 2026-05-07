#include "nbody_helper.h"

#include <cmath>
#include <random>

const float G = 1.f;
const float dt = 0.05f;
const float softening = 1.0f;
const size_t n_bodies = 5000;
const float center_mass = 1000.f;
const int benchmark_steps = 1000;

static BenchmarkResult* g_benchmark_result = nullptr;

void set_benchmark_result(BenchmarkResult* result) {
  g_benchmark_result = result;
}

BenchmarkResult* get_benchmark_result() {
  return g_benchmark_result;
}

float orbital_velocity_scalar(float M, float r) {
  return std::sqrt(1.0f * M / r);
}

unsigned char clamp_color(float v) {
  if (v < 0.0f) return 0;
  if (v > 255.0f) return 255;
  return static_cast<unsigned char>(v);
}

std::vector<Body> init_bodies() {
  std::vector<Body> bodies;
  bodies.reserve(n_bodies + 1);
  bodies.push_back({0, 0, 0, 0, 0, 0, center_mass});

  std::mt19937 rng(42);
  std::uniform_real_distribution<float> angle_dist(0.0f, 2.0f * M_PI);
  std::uniform_real_distribution<float> radius_dist(50.0f, 600.0f);
  std::uniform_real_distribution<float> mass_dist(0.5f, 3.0f);

  for (size_t i = 0; i < n_bodies; ++i) {
    float angle = angle_dist(rng);
    float radius = radius_dist(rng);
    float mass = mass_dist(rng);

    float x = radius * std::cos(angle);
    float y = radius * std::sin(angle);

    float v = orbital_velocity_scalar(center_mass, radius);
    float vx = -v * std::sin(angle);
    float vy =  v * std::cos(angle);

    bodies.push_back({x, y, vx, vy, 0, 0, mass});
  }

  return bodies;
}

void init_bodies_soa(BodySoA& bodies) {
  bodies.x[0] = 0.0f;
  bodies.y[0] = 0.0f;
  bodies.vx[0] = 0.0f;
  bodies.vy[0] = 0.0f;
  bodies.m[0] = center_mass;

  std::mt19937 rng(42);
  std::uniform_real_distribution<float> angle_dist(0.0f, 2.0f * M_PI);
  std::uniform_real_distribution<float> radius_dist(50.0f, 600.0f);
  std::uniform_real_distribution<float> mass_dist(0.5f, 3.0f);

  for (size_t i = 0; i < n_bodies; ++i) {
    float angle = angle_dist(rng);
    float radius = radius_dist(rng);
    float mass = mass_dist(rng);

    float x = radius * std::cos(angle);
    float y = radius * std::sin(angle);

    float v = orbital_velocity_scalar(center_mass, radius);
    float vx = -v * std::sin(angle);
    float vy =  v * std::cos(angle);

    size_t idx = i + 1;
    bodies.x[idx] = x;
    bodies.y[idx] = y;
    bodies.vx[idx] = vx;
    bodies.vy[idx] = vy;
    bodies.m[idx] = mass;
  }
}
