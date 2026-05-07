#include "backend_registry.h"
#include "nbody_helper.h"
#include "body_soa.h"
#include <SFML/Graphics.hpp>
#include <cmath>
#include <chrono>
#include <iostream>
#include <string>
#include <experimental/simd>

// SIMD Definitionen
namespace stdx=std::experimental;
using float_v = stdx::native_simd<float>;
using int_v = stdx::native_simd<int>;
using Mask = stdx::simd_mask<int_v>;
using stdx::reduce;
constexpr size_t VEC_SIZE = float_v::size();

// Erstellt einen Index-Vektor [base, base+1, ..., base+VEC_SIZE-1]
inline int_v make_index_vector(size_t base)
{
    alignas(int_v) int idx[VEC_SIZE];
    for (size_t k = 0; k < VEC_SIZE; ++k)
        idx[k] = int(base + k);
    return int_v(idx, stdx::element_aligned);
}

// Kraftberechnung
static void compute_forces_simd(BodySoA& bodies, float G, float soft) {
  
  size_t n = bodies.n;
  size_t np = bodies.n_padded;
  // Für jeden Körper (sequentiell)
  for (int i = 0; i < n; i++) {
    bodies.ax[i] = 0.0f;
    bodies.ay[i] = 0.0f;

    float_v ax_v = 0.0f;
    float_v ay_v = 0.0f;

    // Für jeden Körper (Vektorisiert)
    for(size_t j = 0; j < np; j += VEC_SIZE) {

    // Lade Vektoren aus den SoA Arrays
    float_v x_SOA(&bodies.x[j], stdx::element_aligned);
    float_v y_SOA(&bodies.y[j], stdx::element_aligned);
    float_v m_SOA(&bodies.m[j], stdx::element_aligned);
    
    // i == j filtern
    int_v idx = make_index_vector(j);
    auto mask = idx != int(i);

    // Berechne Kräfte
    float_v dx = x_SOA - float_v(bodies.x[i]);
    float_v dy = y_SOA - float_v(bodies.y[i]);
    float_v distSqr = dx * dx + dy * dy + float_v(soft * soft);
    float_v invDist  = float_v(1.0f) / sqrt(distSqr);
    float_v invDist3 = invDist * invDist * invDist;
    float_v force = float_v(G) * m_SOA * invDist3;  
    
    alignas(float_v) float force_tmp[VEC_SIZE];
    force.copy_to(force_tmp, stdx::element_aligned);

    for (size_t k = 0; k < VEC_SIZE; ++k) {
        if (!mask[k]) force_tmp[k] = 0.0f;
    }

    force = float_v(force_tmp, stdx::element_aligned);
    ax_v += dx * force;
    ay_v += dy * force;
    }
    // Reduziere Vektor zu Skalar
    bodies.ax[i] += reduce(ax_v);
    bodies.ay[i] += reduce(ay_v);
    
}
}

static void kick_drift_simd(BodySoA& bodies, float dt) {
  
  size_t np = bodies.n_padded;
  const float half_dt = 0.5f * dt;
  float_v dt_v(dt);
  float_v half_dt_v(half_dt);

  // Für jeden Körper (Vektorisiert)
  for (size_t i = 0; i < np; i += VEC_SIZE) {
    float_v vx(&bodies.vx[i], stdx::element_aligned);
    float_v vy(&bodies.vy[i], stdx::element_aligned);
    float_v ax(&bodies.ax[i], stdx::element_aligned);
    float_v ay(&bodies.ay[i], stdx::element_aligned);
    float_v x (&bodies.x [i], stdx::element_aligned);
    float_v y (&bodies.y [i], stdx::element_aligned);

        // kick
        vx += ax * half_dt_v;
        vy += ay * half_dt_v;

        // drift
        x += vx * dt_v;
        y += vy * dt_v;
    
    // Speichere zurück in SoA Arrays
    vx.copy_to(&bodies.vx[i], stdx::element_aligned);
    vy.copy_to(&bodies.vy[i], stdx::element_aligned);
    x .copy_to(&bodies.x [i], stdx::element_aligned);
    y .copy_to(&bodies.y [i], stdx::element_aligned);
}
}
static void kick_simd(BodySoA& bodies, float dt)
{
    size_t np = bodies.n_padded;
    const float half_dt = 0.5f * dt;
     
    float_v half_dt_v(half_dt);

    // Für jeden Körper (Vektorisiert)
    for (size_t i = 0; i < np; i += VEC_SIZE) {

        float_v vx(&bodies.vx[i], stdx::element_aligned);
        float_v vy(&bodies.vy[i], stdx::element_aligned);
        float_v ax(&bodies.ax[i], stdx::element_aligned);
        float_v ay(&bodies.ay[i], stdx::element_aligned);

        vx += ax * half_dt_v;
        vy += ay * half_dt_v;

        vx.copy_to(&bodies.vx[i], stdx::element_aligned);
        vy.copy_to(&bodies.vy[i], stdx::element_aligned);
    }
}

static int run_simd(int argc, char** argv) {
  // Initialisierung (SOA Layout, Anfangskräfte)
  BodySoA bodies(n_bodies);
  init_bodies_soa(bodies);
  compute_forces_simd(bodies, G, softening);

  if (argc >= 2 && std::string(argv[1]) == "benchmark") {
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int s = 0; s < benchmark_steps; ++s) {
      kick_drift_simd(bodies, dt);
      compute_forces_simd(bodies, G, softening);
      kick_simd(bodies, dt);
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    BenchmarkResult* result = get_benchmark_result();
    if (result) {
      result->steps = benchmark_steps;
      result->ms = ms;
      result->threads = 1;
      result->first_x = bodies.x[0];
      result->first_y = bodies.y[0];
    } else {
      std::cout << "[simd] steps=" << benchmark_steps
                << " time_ms=" << ms
                << " first=(" << bodies.x[0] << "," << bodies.y[0] << ")\n";
    }
    return 0;
  }

  // GUI
  const int WIDTH = 1280, HEIGHT = 720;
  sf::RenderWindow window(sf::VideoMode(WIDTH, HEIGHT), "N-Body (simd)");

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

    kick_drift_simd(bodies, dt);
    compute_forces_simd(bodies, G, softening);
    kick_simd(bodies, dt);

    window.clear(sf::Color::Black);
    for (size_t i = 0; i < bodies.n_padded; ++i) {
            sf::CircleShape circle(bodies.m[i] > 50.0f ? 6.f : 2.f);
            circle.setFillColor(mass_to_color(bodies.m[i]));
            circle.setOrigin(circle.getRadius(), circle.getRadius());
            circle.setPosition(
                WIDTH  / 2.f + bodies.x[i],
                HEIGHT / 2.f + bodies.y[i]
            );
            window.draw(circle);
        }

    float fps = 1.0f / fpsClock.restart().asSeconds();
    fpsText.setString("FPS: " + std::to_string((int)fps));
    window.draw(fpsText);
    window.display();

    sf::Time elapsed = frameClock.getElapsedTime();
    if (elapsed < FRAME_DURATION)
      sf::sleep(FRAME_DURATION - elapsed);
    frameClock.restart();
  }
  return 0;
}

static BackendRegister reg("simd", run_simd);