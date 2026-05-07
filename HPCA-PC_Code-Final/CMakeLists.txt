cmake_minimum_required(VERSION 3.16)
project(nbody_test_registry LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build Type Default auf Release setzen, falls nicht angegeben
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O3" CACHE STRING "Release flags" FORCE)
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g" CACHE STRING "Debug flags" FORCE)
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -march=native" CACHE STRING "Release flags" FORCE)

# --- Sources ---
add_executable(nbody
  nbody.cpp
  src/backend_registry.cpp
  src/benchmark.cpp
  src/nbody_helper.cpp
  src/body_soa.cpp
  src/backends/sequential.cpp
  src/backends/omp.cpp
  src/backends/simd.cpp
  src/backends/simd_omp.cpp
  src/backends/opencl.cpp
)

# --- SFML Setup (Behalten wie es war) ---
set(SFML_ROOT "$ENV{HOME}/SFML")
set(SFML_INCLUDE_DIR "${SFML_ROOT}/include")
set(SFML_LIB_DIR "${SFML_ROOT}/lib")
set(SFML_LIBS sfml-graphics sfml-window sfml-system)

target_include_directories(nbody PRIVATE
  "${SFML_INCLUDE_DIR}"
  "${CMAKE_SOURCE_DIR}/include"
)
target_link_directories(nbody PRIVATE "${SFML_LIB_DIR}")
target_link_libraries(nbody PRIVATE ${SFML_LIBS})

# --- Font Copy ---
add_custom_command(
  TARGET nbody
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_SOURCE_DIR}/OpenSans-Bold.ttf"
          "$<TARGET_FILE_DIR:nbody>/OpenSans-Bold.ttf"
)

# --- OpenMP ---
find_package(OpenMP)
if(OpenMP_CXX_FOUND)
  target_link_libraries(nbody PRIVATE OpenMP::OpenMP_CXX)
endif()

# --- Compiler Options ---
target_compile_options(nbody PRIVATE -fno-tree-vectorize)

# --- OpenCL (KORRIGIERT) ---
find_package(OpenCL REQUIRED)

# Header Pfade hinzufügen (manchmal nötig, auch wenn OpenCL::OpenCL das meistens macht)
target_include_directories(nbody PRIVATE ${OpenCL_INCLUDE_DIRS})

# Verlinken mit dem modernen CMake Target
target_link_libraries(nbody PRIVATE OpenCL::OpenCL)

# --- OpenCL Kernel Copy ---
add_custom_command(
  TARGET nbody
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_SOURCE_DIR}/opencl/nbody.cl"
          "$<TARGET_FILE_DIR:nbody>/nbody.cl"
)