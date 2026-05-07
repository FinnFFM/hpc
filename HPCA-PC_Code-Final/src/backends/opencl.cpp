#define CL_TARGET_OPENCL_VERSION 120

#include "backend_registry.h"
#include "nbody_helper.h"
#include "body_soa.h"
#include <SFML/Graphics.hpp>
// #include <gegl-0.4/opencl/cl.h>
#include <CL/cl.h> // for Ubuntu

#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>

static cl_context       context = nullptr;
static cl_command_queue queue   = nullptr;
static cl_program       program = nullptr;
static cl_device_id     device  = nullptr;

static cl_kernel k_compute_forces = nullptr;
static cl_kernel k_kick_drift     = nullptr;
static cl_kernel k_kick           = nullptr;

void checkError(cl_int err, const char* operation) {
    if (err != CL_SUCCESS) {
        std::cerr << "Error during operation '" << operation << "': " << err << std::endl;
        exit(1);
    }
}

std::string readKernelSource(const char* filename) {
    std::ifstream file(filename);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void init_opencl() {
    
cl_int err;
cl_uint platformCount;
cl_platform_id platform;
err = clGetPlatformIDs(1, &platform, &platformCount);
checkError(err, "clGetPlatformIDs");

cl_uint deviceCount;
err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_ALL, 1, &device, &deviceCount);
checkError(err, "clGetDeviceIDs");

context = clCreateContext(NULL, 1, &device, NULL, NULL, &err);
checkError(err, "clCreateContext");

queue = clCreateCommandQueue(context, device, 0, &err);
checkError(err, "clCreateCommandQueue");

std::string sourceStr = readKernelSource("../opencl/nbody.cl");
const char* source = sourceStr.c_str();

program = clCreateProgramWithSource(context, 1, &source, NULL, &err);
checkError(err, "clCreateProgramWithSource");

err = clBuildProgram(program, 1, &device, NULL, NULL, NULL);
if (err != CL_SUCCESS) {
        size_t log_size;
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
        std::vector<char> log(log_size);
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, log_size, log.data(), NULL);
        std::cerr << "Error during operation 'clBuildProgram': " << err << std::endl;
        std::cerr << "Build log:" << std::endl << log.data() << std::endl;
        exit(1);
    }

 k_compute_forces = clCreateKernel(program, "compute_forces", &err);
 checkError(err, "clCreateKernel(compute_forces)");

 k_kick_drift = clCreateKernel(program, "kick_drift", &err);
 checkError(err, "clCreateKernel(kick_drift)");

 k_kick = clCreateKernel(program, "kick", &err);
 checkError(err, "clCreateKernel(kick)");

}




static int run_opencl(int argc, char** argv)
{ 
    static bool opencl_initialized = false;
    if (!opencl_initialized) {
    init_opencl();
    opencl_initialized = true;
    }

    const size_t N = n_bodies;
    size_t global = ((N + 255) / 256) * 256;
    size_t local  = 256;

    cl_int err;
    // ---------- Host-Daten ----------
    BodySoA bodies(N);
    init_bodies_soa(bodies);

    // ---------- OpenCL Buffers ----------
    cl_mem buf_x  = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                   sizeof(float) * N, bodies.x.data(), &err);
    checkError(err, "clCreateBuffer(buf_x)");
    cl_mem buf_y  = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                   sizeof(float) * N, bodies.y.data(), &err);
    checkError(err, "clCreateBuffer(buf_y)");
    cl_mem buf_vx = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                   sizeof(float) * N, bodies.vx.data(), &err);
    checkError(err, "clCreateBuffer(buf_vx)");
    cl_mem buf_vy = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                   sizeof(float) * N, bodies.vy.data(), &err);
    checkError(err, "clCreateBuffer(buf_vy)");
    cl_mem buf_ax = clCreateBuffer(context, CL_MEM_READ_WRITE,
                                   sizeof(float) * N, nullptr, &err);
    checkError(err, "clCreateBuffer(buf_ax)");
    cl_mem buf_ay = clCreateBuffer(context, CL_MEM_READ_WRITE,
                                   sizeof(float) * N, nullptr, &err);
    checkError(err, "clCreateBuffer(buf_ay)");
    cl_mem buf_m  = clCreateBuffer(context, CL_MEM_READ_ONLY  | CL_MEM_COPY_HOST_PTR,
                                   sizeof(float) * N, bodies.m.data(), &err);
    checkError(err, "clCreateBuffer(buf_m)");

    // ---------- Kernel Args ----------
    auto set_compute_args = [&]() {
        clSetKernelArg(k_compute_forces, 0, sizeof(cl_mem), &buf_x);
        clSetKernelArg(k_compute_forces, 1, sizeof(cl_mem), &buf_y);
        clSetKernelArg(k_compute_forces, 2, sizeof(cl_mem), &buf_m);
        clSetKernelArg(k_compute_forces, 3, sizeof(cl_mem), &buf_ax);
        clSetKernelArg(k_compute_forces, 4, sizeof(cl_mem), &buf_ay);
        clSetKernelArg(k_compute_forces, 5, sizeof(unsigned), &N);
        clSetKernelArg(k_compute_forces, 6, sizeof(float), &G);
        clSetKernelArg(k_compute_forces, 7, sizeof(float), &softening);
    };

    auto set_kick_drift_args = [&]() {
        clSetKernelArg(k_kick_drift, 0, sizeof(cl_mem), &buf_x);
        clSetKernelArg(k_kick_drift, 1, sizeof(cl_mem), &buf_y);
        clSetKernelArg(k_kick_drift, 2, sizeof(cl_mem), &buf_vx);
        clSetKernelArg(k_kick_drift, 3, sizeof(cl_mem), &buf_vy);
        clSetKernelArg(k_kick_drift, 4, sizeof(cl_mem), &buf_ax);
        clSetKernelArg(k_kick_drift, 5, sizeof(cl_mem), &buf_ay);
        clSetKernelArg(k_kick_drift, 6, sizeof(unsigned), &N);
        clSetKernelArg(k_kick_drift, 7, sizeof(float), &dt);
    };

    auto set_kick_args = [&]() {
        clSetKernelArg(k_kick, 0, sizeof(cl_mem), &buf_vx);
        clSetKernelArg(k_kick, 1, sizeof(cl_mem), &buf_vy);
        clSetKernelArg(k_kick, 2, sizeof(cl_mem), &buf_ax);
        clSetKernelArg(k_kick, 3, sizeof(cl_mem), &buf_ay);
        clSetKernelArg(k_kick, 4, sizeof(unsigned), &N);
        clSetKernelArg(k_kick, 5, sizeof(float), &dt);
    };

    // ---------- Initial force ----------
    set_compute_args();
    clEnqueueNDRangeKernel(queue, k_compute_forces, 1, nullptr, &global, &local, 0, nullptr, nullptr);

    // ---------- Benchmark ----------
    if (argc >= 2 && std::string(argv[1]) == "benchmark") {

        auto t0 = std::chrono::high_resolution_clock::now();

        for (int s = 0; s < benchmark_steps; ++s) {
            set_kick_drift_args();
            err = clEnqueueNDRangeKernel(queue, k_kick_drift, 1, nullptr, &global, &local, 0, nullptr, nullptr);
            checkError(err, "clEnqueueNDRangeKernel(k_kick_drift)");

            set_compute_args();
            err = clEnqueueNDRangeKernel(queue, k_compute_forces, 1, nullptr, &global, &local, 0, nullptr, nullptr);
            checkError(err, "clEnqueueNDRangeKernel(k_compute_forces)");

            set_kick_args();
            err = clEnqueueNDRangeKernel(queue, k_kick, 1, nullptr, &global, &local, 0, nullptr, nullptr);
            checkError(err, "clEnqueueNDRangeKernel(k_kick)");
        }

        clFinish(queue);

        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

        clEnqueueReadBuffer(queue, buf_x, CL_TRUE, 0, sizeof(float), bodies.x.data(), 0, nullptr, nullptr);
        clEnqueueReadBuffer(queue, buf_y, CL_TRUE, 0, sizeof(float), bodies.y.data(), 0, nullptr, nullptr);

        BenchmarkResult* result = get_benchmark_result();
        if (result) {
            result->steps = benchmark_steps;
            result->ms = ms;
            result->threads = 0; // GPU
            result->first_x = bodies.x[0];
            result->first_y = bodies.y[0];
        }
        return 0;
    }

    // ---------- SFML ----------
    const int WIDTH = 1280, HEIGHT = 720;
    sf::RenderWindow window(sf::VideoMode(WIDTH, HEIGHT), "N-Body (OpenCL)");

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

        set_kick_drift_args();
        err = clEnqueueNDRangeKernel(queue, k_kick_drift, 1, nullptr, &global, &local, 0, nullptr, nullptr);
        checkError(err, "clEnqueueNDRangeKernel(k_kick_drift)");

        set_compute_args();
        err = clEnqueueNDRangeKernel(queue, k_compute_forces, 1, nullptr, &global, &local, 0, nullptr, nullptr);
        checkError(err, "clEnqueueNDRangeKernel(k_compute_forces)");

        set_kick_args();
        err = clEnqueueNDRangeKernel(queue, k_kick, 1, nullptr, &global, &local, 0, nullptr, nullptr);
        checkError(err, "clEnqueueNDRangeKernel(k_kick)");

        // Read back positions
        clEnqueueReadBuffer(queue, buf_x, CL_TRUE, 0, sizeof(float)*N, bodies.x.data(), 0, nullptr, nullptr);
        clEnqueueReadBuffer(queue, buf_y, CL_TRUE, 0, sizeof(float)*N, bodies.y.data(), 0, nullptr, nullptr);

        window.clear(sf::Color::Black);
        for (size_t i = 0; i < N; ++i) {
            sf::CircleShape circle(bodies.m[i] > 50.f ? 6.f : 2.f);
            circle.setOrigin(circle.getRadius(), circle.getRadius());
            circle.setFillColor(mass_to_color(bodies.m[i]));
            circle.setPosition(WIDTH/2.f + bodies.x[i], HEIGHT/2.f + bodies.y[i]);
            window.draw(circle);
        }

        float fps = 1.0f / fpsClock.restart().asSeconds();
        fpsText.setString("FPS: " + std::to_string((int)fps) + " (OpenCL)");
        window.draw(fpsText);
        window.display();

        sf::Time elapsed = frameClock.getElapsedTime();
        if (elapsed < FRAME_DURATION)
            sf::sleep(FRAME_DURATION - elapsed);
        frameClock.restart();
    }
    clReleaseMemObject(buf_x);
    clReleaseMemObject(buf_y);
    clReleaseMemObject(buf_vx);
    clReleaseMemObject(buf_vy);
    clReleaseMemObject(buf_ax);
    clReleaseMemObject(buf_ay);
    clReleaseMemObject(buf_m);
    clReleaseKernel(k_compute_forces);
    clReleaseKernel(k_kick_drift);
    clReleaseKernel(k_kick);
    clReleaseProgram(program);
    clReleaseCommandQueue(queue);
    clReleaseContext(context);
    return 0;
}

static BackendRegister reg("opencl", run_opencl);