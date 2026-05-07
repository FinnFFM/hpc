#include "benchmark.h"
#include "backend_registry.h"
#include "nbody_helper.h"

#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#define CL_TARGET_OPENCL_VERSION 120
// #include <gegl-0.4/opencl/cl.h>
#include <CL/cl.h>

struct BenchmarkRow {
  std::string name;
  BenchmarkResult result;
};

static std::string get_opencl_gpu_name() {
  cl_uint platform_count = 0;
  if (clGetPlatformIDs(0, nullptr, &platform_count) != CL_SUCCESS ||
      platform_count == 0) {
    return std::string();
  }
  std::vector<cl_platform_id> platforms(platform_count);
  if (clGetPlatformIDs(platform_count, platforms.data(), nullptr) != CL_SUCCESS) {
    return std::string();
  }
  for (cl_platform_id platform : platforms) {
    cl_uint device_count = 0;
    if (clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 0, nullptr, &device_count) !=
            CL_SUCCESS ||
        device_count == 0) {
      continue;
    }
    std::vector<cl_device_id> devices(device_count);
    if (clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, device_count,
                       devices.data(), nullptr) != CL_SUCCESS) {
      continue;
    }
    cl_device_id device = devices[0];
    size_t name_size = 0;
    if (clGetDeviceInfo(device, CL_DEVICE_NAME, 0, nullptr, &name_size) !=
            CL_SUCCESS ||
        name_size == 0) {
      return std::string();
    }
    std::vector<char> name(name_size);
    if (clGetDeviceInfo(device, CL_DEVICE_NAME, name_size, name.data(), nullptr) !=
        CL_SUCCESS) {
      return std::string();
    }
    return std::string(name.data());
  }
  return std::string();
}

int run_benchmark() {
  std::cout << "[INFO] Running benchmark for " << benchmark_steps << " steps.\n";
  std::string gpu_name = get_opencl_gpu_name();
  if (!gpu_name.empty()) {
    std::cout << "[INFO] OpenCL device: " << gpu_name << ".\n";
  }
  std::vector<std::string> names = {
      "sequential",
      "simd",
      "omp",
      "simd-omp",
      "opencl",
  };
  std::vector<BenchmarkRow> rows;
  rows.reserve(names.size());
  const size_t total = names.size();
  size_t index = 0;

  for (const auto& name : names) {
    ++index;
    const int bar_width = 24;
    int filled = static_cast<int>((index * bar_width) / total);
    std::string bar(filled, '=');
    bar.append(bar_width - filled, '.');
    int percent = static_cast<int>((index * 100) / total);
    std::string line = "[" + bar + "] ";
    line += (percent < 10) ? "  " : (percent < 100) ? " " : "";
    line += std::to_string(percent) + "%  running " + name +
            " (" + std::to_string(index) + "/" + std::to_string(total) + ")";
    const size_t line_width = 93;
    if (line.size() < line_width) {
      line.append(line_width - line.size(), ' ');
    } else if (line.size() > line_width) {
      line.resize(line_width);
    }
    std::cout << "\r[INFO] " << line << std::flush;

    std::string bench_arg = "benchmark";
    const char* args[] = {name.c_str(), bench_arg.c_str()};
    BackendFn fn = BackendRegistry::Instance().Find(name);
    if (!fn) {
      BenchmarkResult result;
      result.steps = 0;
      result.ms = 0.0;
      result.threads = 0;
      rows.push_back({name, result});
      continue;
    }

    BenchmarkResult result;
    set_benchmark_result(&result);
    fn(2, const_cast<char**>(args));
    set_benchmark_result(nullptr);
    rows.push_back({name, result});
  }
  std::cout << "\n[INFO] Benchmarking finished.\n\n";

  std::cout << "RESULTS:\n";
  std::cout << std::left
            << std::setw(12) << "backend"
            << std::right << std::setw(12) << "ms"
            << std::right << std::setw(12) << "ms/step"
            << std::right << std::setw(10) << "threads"
            << std::right << std::setw(10) << "speedup"
            << "\n";
  std::cout << std::string(56, '-') << "\n";

  double seq_ms = 0.0;
  for (const auto& row : rows) {
    if (row.name == "sequential") {
      seq_ms = row.result.ms;
      break;
    }
  }

  for (const auto& row : rows) {
    const auto& r = row.result;
    bool has_data = (r.steps > 0 && r.ms > 0.0);
    double ms_per_step = has_data ? (r.ms / r.steps) : 0.0;
    double speedup = (seq_ms > 0.0 && r.ms > 0.0) ? (seq_ms / r.ms) : 0.0;
    std::cout << std::left
              << std::setw(12) << row.name;
    if (has_data) {
      std::cout << std::right << std::setw(12) << std::fixed << std::setprecision(2) << r.ms
                << std::right << std::setw(12) << std::fixed << std::setprecision(4) << ms_per_step
                << std::right << std::setw(10) << r.threads
                << std::right << std::setw(10) << std::fixed << std::setprecision(2) << speedup
                << "\n";
    } else {
      std::cout << std::right << std::setw(12) << "N/A"
                << std::right << std::setw(12) << "N/A"
                << std::right << std::setw(10) << "N/A"
                << std::right << std::setw(10) << "N/A"
                << "\n";
    }
  }
  return 0;
}
