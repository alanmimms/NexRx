# NexRx Digital Twin - CMake Build System
# Stage 1: Foundation with Xyce integration

cmake_minimum_required(VERSION 3.20)
project(NexRxTwin
    VERSION 0.1.0
    DESCRIPTION "NexRx Digital Twin - Software-in-the-Loop Simulation"
    LANGUAGES CXX
)

# C++20 required
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type defaults to RelWithDebInfo
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

# Export compile commands for IDE integration
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# -----------------------------------------------------------------------------
# Paths to Xyce and Trilinos (adjust if needed)
# -----------------------------------------------------------------------------
set(XYCE_SOURCE_DIR "/home/alan/ham/xyce/Xyce-7.10" CACHE PATH "Xyce source directory")
set(XYCE_BUILD_DIR "/home/alan/ham/xyce/Xyce-7.10/build" CACHE PATH "Xyce build directory")
set(TRILINOS_LIB_DIR "/home/alan/ham/XyceLibs/Serial/lib" CACHE PATH "Trilinos library directory")
set(TRILINOS_INCLUDE_DIR "/home/alan/ham/XyceLibs/Serial/include" CACHE PATH "Trilinos include directory")

# -----------------------------------------------------------------------------
# Find required system libraries
# -----------------------------------------------------------------------------
find_package(Threads REQUIRED)

# FFTW3
find_library(FFTW3_LIBRARY fftw3 HINTS /usr/lib/x86_64-linux-gnu)
if(NOT FFTW3_LIBRARY)
    message(FATAL_ERROR "FFTW3 library not found")
endif()

# AMD (SuiteSparse)
find_library(AMD_LIBRARY amd HINTS /usr/lib/x86_64-linux-gnu)
if(NOT AMD_LIBRARY)
    message(WARNING "AMD library not found - may be included in system LAPACK")
    set(AMD_LIBRARY "")
endif()

# LAPACK/BLAS
find_package(LAPACK REQUIRED)
find_package(BLAS REQUIRED)

# Fortran runtime (needed for some Trilinos libs)
find_library(GFORTRAN_LIBRARY gfortran
    HINTS
        /usr/lib/x86_64-linux-gnu
        /usr/lib/gcc/x86_64-linux-gnu/13
        /usr/lib/gcc/x86_64-linux-gnu/12
        /usr/lib/gcc/x86_64-linux-gnu/11
)
find_library(QUADMATH_LIBRARY quadmath
    HINTS
        /usr/lib/x86_64-linux-gnu
        /usr/lib/gcc/x86_64-linux-gnu/13
        /usr/lib/gcc/x86_64-linux-gnu/12
        /usr/lib/gcc/x86_64-linux-gnu/11
)

# Handle case where Fortran libs aren't needed or found
if(NOT GFORTRAN_LIBRARY)
    message(WARNING "gfortran library not found - linking may fail")
    set(GFORTRAN_LIBRARY "")
endif()
if(NOT QUADMATH_LIBRARY)
    message(WARNING "quadmath library not found - linking may fail")
    set(QUADMATH_LIBRARY "")
endif()

# -----------------------------------------------------------------------------
# Xyce include directories
# -----------------------------------------------------------------------------
set(XYCE_INCLUDE_DIRS
    ${XYCE_BUILD_DIR}/src                          # Generated headers (Xyce_config.h)
    ${XYCE_SOURCE_DIR}/src/AnalysisPKG
    ${XYCE_SOURCE_DIR}/src/CircuitPKG
    ${XYCE_SOURCE_DIR}/src/DeviceModelPKG/Core
    ${XYCE_SOURCE_DIR}/src/DeviceModelPKG/OpenModels
    ${XYCE_SOURCE_DIR}/src/ErrorHandlingPKG
    ${XYCE_SOURCE_DIR}/src/IOInterfacePKG
    ${XYCE_SOURCE_DIR}/src/LinearAlgebraServicesPKG
    ${XYCE_SOURCE_DIR}/src/LoaderServicesPKG
    ${XYCE_SOURCE_DIR}/src/NonlinearSolverPKG
    ${XYCE_SOURCE_DIR}/src/ParallelDistPKG
    ${XYCE_SOURCE_DIR}/src/TimeIntegrationPKG
    ${XYCE_SOURCE_DIR}/src/TopoManagerPKG
    ${XYCE_SOURCE_DIR}/src/UtilityPKG
    ${TRILINOS_INCLUDE_DIR}
)

# -----------------------------------------------------------------------------
# Xyce and Trilinos libraries (order matters for static linking!)
# -----------------------------------------------------------------------------
set(XYCE_LIBRARY "${XYCE_BUILD_DIR}/src/.libs/libxyce.a")

# Trilinos libraries in dependency order
set(TRILINOS_LIBRARIES
    ${TRILINOS_LIB_DIR}/libtrilinoscouplings.a
    ${TRILINOS_LIB_DIR}/libstokhos_amesos2.a
    ${TRILINOS_LIB_DIR}/libstokhos_tpetra.a
    ${TRILINOS_LIB_DIR}/libstokhos_sacado.a
    ${TRILINOS_LIB_DIR}/libstokhos.a
    ${TRILINOS_LIB_DIR}/liblocaepetra.a
    ${TRILINOS_LIB_DIR}/liblocalapack.a
    ${TRILINOS_LIB_DIR}/libloca.a
    ${TRILINOS_LIB_DIR}/libnoxepetra.a
    ${TRILINOS_LIB_DIR}/libnoxlapack.a
    ${TRILINOS_LIB_DIR}/libnox.a
    ${TRILINOS_LIB_DIR}/libamesos2.a
    ${TRILINOS_LIB_DIR}/libbelostpetra.a
    ${TRILINOS_LIB_DIR}/libbelosepetra.a
    ${TRILINOS_LIB_DIR}/libbelos.a
    ${TRILINOS_LIB_DIR}/libifpack.a
    ${TRILINOS_LIB_DIR}/libaztecoo.a
    ${TRILINOS_LIB_DIR}/libtpetraext.a
    ${TRILINOS_LIB_DIR}/libtpetrainout.a
    ${TRILINOS_LIB_DIR}/libtpetra.a
    ${TRILINOS_LIB_DIR}/libtpetraclassiclinalg.a
    ${TRILINOS_LIB_DIR}/libtpetraclassicnodeapi.a
    ${TRILINOS_LIB_DIR}/libtpetraclassic.a
    ${TRILINOS_LIB_DIR}/libsacado.a
    ${TRILINOS_LIB_DIR}/libkokkoskernels.a
    ${TRILINOS_LIB_DIR}/libkokkosalgorithms.a
    ${TRILINOS_LIB_DIR}/libkokkoscontainers.a
    ${TRILINOS_LIB_DIR}/libamesos.a
    ${TRILINOS_LIB_DIR}/libtrilinosss.a
    ${TRILINOS_LIB_DIR}/libepetraext.a
    ${TRILINOS_LIB_DIR}/libtriutils.a
    ${TRILINOS_LIB_DIR}/libepetra.a
    ${TRILINOS_LIB_DIR}/libteuchoskokkoscomm.a
    ${TRILINOS_LIB_DIR}/libteuchoskokkoscompat.a
    ${TRILINOS_LIB_DIR}/libteuchosremainder.a
    ${TRILINOS_LIB_DIR}/libteuchosnumerics.a
    ${TRILINOS_LIB_DIR}/libteuchoscomm.a
    ${TRILINOS_LIB_DIR}/libteuchosparameterlist.a
    ${TRILINOS_LIB_DIR}/libteuchoscore.a
    ${TRILINOS_LIB_DIR}/libkokkoscore.a
)

# -----------------------------------------------------------------------------
# Twin library - core components
# -----------------------------------------------------------------------------
add_library(twin_core STATIC
    src/xyce/XyceWrapper.cpp
    src/orchestrator/Orchestrator.cpp
)

target_include_directories(twin_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${XYCE_INCLUDE_DIRS}
)

target_link_libraries(twin_core PUBLIC
    ${XYCE_LIBRARY}
    ${TRILINOS_LIBRARIES}
    ${FFTW3_LIBRARY}
    ${AMD_LIBRARY}
    ${LAPACK_LIBRARIES}
    ${BLAS_LIBRARIES}
    ${GFORTRAN_LIBRARY}
    ${QUADMATH_LIBRARY}
    Threads::Threads
    dl
    m
)

# Compiler warnings
target_compile_options(twin_core PRIVATE
    -Wall -Wextra -Wpedantic
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-O3>
    $<$<CONFIG:RelWithDebInfo>:-O2 -g>
)

# -----------------------------------------------------------------------------
# Transport library - IPC for Host<->Firmware communication
# -----------------------------------------------------------------------------
add_library(twin_transport STATIC
    src/transport/SharedMemTransport.cpp
    src/transport/UnixSocketTransport.cpp
)

target_include_directories(twin_transport PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(twin_transport PUBLIC
    rt          # POSIX shared memory
    Threads::Threads
)

target_compile_options(twin_transport PRIVATE
    -Wall -Wextra -Wpedantic
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-O3>
    $<$<CONFIG:RelWithDebInfo>:-O2 -g>
)

# -----------------------------------------------------------------------------
# Protobuf generation (optional - enable with -DWITH_PROTOBUF=ON)
# -----------------------------------------------------------------------------
option(WITH_PROTOBUF "Enable protobuf message generation" OFF)
if(WITH_PROTOBUF)
    find_package(Protobuf REQUIRED)
    protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS proto/nexrx.proto)
    add_library(twin_proto STATIC ${PROTO_SRCS} ${PROTO_HDRS})
    target_include_directories(twin_proto PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
    target_link_libraries(twin_proto PUBLIC protobuf::libprotobuf-lite)
endif()

# -----------------------------------------------------------------------------
# Sampler library - ADC sampling from Xyce
# -----------------------------------------------------------------------------
add_library(twin_sampler STATIC
    src/sampler/AdcSampler.cpp
)

target_include_directories(twin_sampler PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${XYCE_INCLUDE_DIRS}
)

target_link_libraries(twin_sampler PUBLIC
    twin_core
)

target_compile_options(twin_sampler PRIVATE
    -Wall -Wextra -Wpedantic
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-O3>
    $<$<CONFIG:RelWithDebInfo>:-O2 -g>
)

# -----------------------------------------------------------------------------
# Main executable
# -----------------------------------------------------------------------------
add_executable(twin src/main.cpp)
target_link_libraries(twin PRIVATE twin_core twin_transport twin_sampler)
if(WITH_PROTOBUF)
    target_link_libraries(twin PRIVATE twin_proto)
endif()

# -----------------------------------------------------------------------------
# Pipeline test executable
# -----------------------------------------------------------------------------
add_executable(pipeline_test src/pipeline_test.cpp)
target_link_libraries(pipeline_test PRIVATE twin_core twin_transport twin_sampler)

# -----------------------------------------------------------------------------
# Stimulus test executable (header-only stimulus library)
# -----------------------------------------------------------------------------
add_executable(stimulus_test src/stimulus_test.cpp)
target_include_directories(stimulus_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# -----------------------------------------------------------------------------
# Host library - DSP pipeline and visualization
# -----------------------------------------------------------------------------
add_library(twin_host STATIC
    src/host/HostApp.cpp
    src/host/DspPipeline.cpp
    src/host/Visualizer.cpp
)

target_include_directories(twin_host PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(twin_host PUBLIC
    twin_transport
    Threads::Threads
)

target_compile_options(twin_host PRIVATE
    -Wall -Wextra -Wpedantic
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-O3>
    $<$<CONFIG:RelWithDebInfo>:-O2 -g>
)

# -----------------------------------------------------------------------------
# Host test executable
# -----------------------------------------------------------------------------
add_executable(host_test src/host_test.cpp)
target_include_directories(host_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(host_test PRIVATE twin_host twin_transport)

# -----------------------------------------------------------------------------
# Install rules
# -----------------------------------------------------------------------------
install(TARGETS twin
    RUNTIME DESTINATION bin
)

# -----------------------------------------------------------------------------
# Summary
# -----------------------------------------------------------------------------
message(STATUS "")
message(STATUS "NexRx Digital Twin Configuration:")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "  Xyce source: ${XYCE_SOURCE_DIR}")
message(STATUS "  Xyce build: ${XYCE_BUILD_DIR}")
message(STATUS "  Trilinos libs: ${TRILINOS_LIB_DIR}")
message(STATUS "")
