#include "backend_registry.h"
#include "benchmark.h"
#include "nbody_helper.h"

#include <iostream>
#include <string>

static void print_usage(const char* argv0) {
  std::cerr << "Usage: " << argv0 << " [backend] [backend-args]\n";
  std::cerr << "       " << argv0 << " benchmark\n";
  std::cerr << "Available backends:\n";
  for (const auto& name : BackendRegistry::Instance().Names()) {
    std::cerr << "  " << name << "\n";
  }
}

int main(int argc, char** argv) {
  if (argc >= 2 && std::string(argv[1]) == "benchmark") {
    return run_benchmark();
  }

  std::string backend = "sequential";
  if (argc >= 2) {
    backend = argv[1];
  }

  BackendFn fn = BackendRegistry::Instance().Find(backend);
  if (!fn) {
    std::cerr << "Unknown backend: " << backend << "\n";
    print_usage(argv[0]);
    return 1;
  }

  int shifted_argc = argc - 1;
  char** shifted_argv = argv + 1;
  return fn(shifted_argc, shifted_argv);
}
