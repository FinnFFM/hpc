#include "backend_registry.h"
#include "nbody_helper.h"

#include <SFML/Graphics.hpp>
#include <omp.h>
#include <cmath>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

static void compute_forces_omp(std::vector<Body>& bodies, float G, float soft) {
   size_t n = bodies.size();
  #pragma omp parallel for schedule(static)
  for (size_t i = 0; i < n; ++i) {
    bodies[i].ax = 0.0f;
    bodies[i].ay = 0.0f;
    
    for (size_t j = 0; j < n; ++j) {
      if (i == j) continue;
      
      float dx = bodies[j].x - bodies[i].x;
      float dy = bodies[j].y - bodies[i].y;
      float distSqr = dx * dx + dy * dy + soft * soft;
      float invDist = 1.0f / std::sqrt(distSqr);
      float invDist3 = invDist * invDist * invDist;
      float force = G * bodies[j].m * invDist3;
      
      bodies[i].ax += dx * force;
      bodies[i].ay += dy * force;
    }
  }
}

static void kick_drift_omp(std::vector<Body>& bodies, float dt) {
    size_t n = bodies.size();
  #pragma omp parallel for schedule(static)
  for (size_t i = 0; i < n; ++i) {
    bodies[i].vx += bodies[i].ax * (0.5f * dt);
    bodies[i].vy += bodies[i].ay * (0.5f * dt);
    bodies[i].x += bodies[i].vx * dt;
    bodies[i].y += bodies[i].vy * dt;
  }
}

static void kick_omp(std::vector<Body>& bodies, float dt) {
    size_t n = bodies.size();
  #pragma omp parallel for schedule(static)
  for (size_t i = 0; i < n; ++i) {
    bodies[i].vx += bodies[i].ax * (0.5f * dt);
    bodies[i].vy += bodies[i].ay * (0.5f * dt);
  }
}

static int run_omp(int argc, char** argv) {

  int num_threads = omp_get_max_threads(); // Default to max available (fastest at 8)
  bool benchmark_mode = false;

  // Parser to find "benchmark" flag or thread count number
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "benchmark") {
      benchmark_mode = true;
    } else if (arg != "omp") {
        // Try to parse as thread number if it's not the backend name
        try {
            int t = std::stoi(arg);
            if (t > 0) num_threads = t;
        } catch (...) {
            // Ignore non-integer args
        }
    }
  }

  omp_set_num_threads(num_threads);

  // Initialize
  std::vector<Body> bodies = init_bodies();
  compute_forces_omp(bodies, G, softening);

  // Benchmark Mode
  if (benchmark_mode) {
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int s = 0; s < benchmark_steps; ++s) {
      kick_drift_omp(bodies, dt);
      compute_forces_omp(bodies, G, softening);
      kick_omp(bodies, dt);
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    BenchmarkResult* result = get_benchmark_result();
    if (result) {
      result->steps = benchmark_steps;
      result->ms = ms;
      result->threads = num_threads;
      result->first_x = bodies[0].x;
      result->first_y = bodies[0].y;
    } else {
      std::cout << "[omp] threads=" << num_threads 
                << " steps=" << benchmark_steps
                << " time_ms=" << ms
                << " first=(" << bodies[0].x << "," << bodies[0].y << ")\n";
    }
    return 0;
  }

  // GUI Mode
  const int WIDTH = 1280, HEIGHT = 720;
  sf::RenderWindow window(sf::VideoMode(WIDTH, HEIGHT), "N-Body (OpenMP)");

  sf::Font font;
  if (!font.loadFromFile("OpenSans-Bold.ttf")) {
    std::cerr << "Failed to load font\n";
  }
  sf::Text fpsText("", font, 18);
  fpsText.setFillColor(sf::Color::White);
  fpsText.setPosition(10, 5);

  constexpr float TARGET_FPS = 165.f;
  const sf::Time FRAME_DURATION = sf::seconds(1.f / TARGET_FPS);
  sf::Clock frameClock;
  sf::Clock fpsClock;

  while (window.isOpen()) {
    sf::Event e;
    while (window.pollEvent(e))
      if (e.type == sf::Event::Closed)
        window.close();

    kick_drift_omp(bodies, dt);
    compute_forces_omp(bodies, G, softening);
    kick_omp(bodies, dt);

    window.clear(sf::Color::Black);
    for (const auto& b : bodies) {
      sf::CircleShape circle(b.m > 50.0f ? 6 : 2);
      circle.setFillColor(mass_to_color(b.m));
      circle.setPosition(WIDTH / 2 + b.x, HEIGHT / 2 + b.y);
      circle.setOrigin(circle.getRadius(), circle.getRadius());
      window.draw(circle);
    }

    float fps = 1.0f / fpsClock.restart().asSeconds();
    fpsText.setString("FPS: " + std::to_string((int)fps) + " (" + std::to_string(num_threads) + " threads)");
    window.draw(fpsText);
    window.display();

    sf::Time elapsed = frameClock.getElapsedTime();
    if (elapsed < FRAME_DURATION)
      sf::sleep(FRAME_DURATION - elapsed);
    frameClock.restart();
  }
  return 0;
}

static BackendRegister reg("omp", run_omp);