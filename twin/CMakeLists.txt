cmake_minimum_required(VERSION 3.20)
project(NexRxTwin)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# tinycbor - lightweight CBOR encoder/decoder
FetchContent_Declare(
  tinycbor
  GIT_REPOSITORY https://github.com/intel/tinycbor.git
  GIT_TAG v0.6.0
)

FetchContent_GetProperties(tinycbor)
if(NOT tinycbor_POPULATED)
  FetchContent_Populate(tinycbor)
endif()

add_library(tinycbor STATIC
  ${tinycbor_SOURCE_DIR}/src/cborencoder.c
  ${tinycbor_SOURCE_DIR}/src/cborparser.c
  ${tinycbor_SOURCE_DIR}/src/cborerrorstrings.c
  ${tinycbor_SOURCE_DIR}/src/cborvalidation.c
)
target_include_directories(tinycbor PUBLIC ${tinycbor_SOURCE_DIR}/src)

# sol2 is header-only, for Lua bindings
FetchContent_Declare(
  sol2
  GIT_REPOSITORY https://github.com/ThePhD/sol2.git
  GIT_TAG v3.3.0
)
FetchContent_MakeAvailable(sol2)

# Xyce paths
set(XYCE_SOURCE_DIR "/home/alan/ham/xyce/Xyce-7.10")
set(XYCE_BUILD_DIR "/home/alan/ham/xyce/Xyce-7.10/build")
set(TRILINOS_LIB_DIR "/home/alan/ham/XyceLibs/Serial/lib")
set(TRILINOS_INCLUDE_DIR "/home/alan/ham/XyceLibs/Serial/include")

set(XYCE_INCLUDE_DIRS
    ${XYCE_BUILD_DIR}/src
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

set(XYCE_LIBRARY "${XYCE_BUILD_DIR}/src/.libs/libxyce.a")

include_directories(src ${XYCE_INCLUDE_DIRS})

# System libraries
find_library(FFTW3_LIBRARY fftw3 HINTS /usr/lib/x86_64-linux-gnu)
find_library(AMD_LIBRARY amd HINTS /usr/lib/x86_64-linux-gnu)
find_package(LAPACK REQUIRED)
find_package(BLAS REQUIRED)
find_library(GFORTRAN_LIBRARY gfortran HINTS /usr/lib/x86_64-linux-gnu /usr/lib/gcc/x86_64-linux-gnu/13 /usr/lib/gcc/x86_64-linux-gnu/14 /usr/lib/gcc/x86_64-linux-gnu/12)
find_library(QUADMATH_LIBRARY quadmath HINTS /usr/lib/x86_64-linux-gnu /usr/lib/gcc/x86_64-linux-gnu/13 /usr/lib/gcc/x86_64-linux-gnu/14 /usr/lib/gcc/x86_64-linux-gnu/12)

# Common sources for both executables
set(COMMON_SOURCES
  src/ControlHandler.cpp
  src/transport/TCPControlTransport.cpp
  src/transport/UDPStreamTransport.cpp
  src/stimulus/AMGenerator.cpp
  src/stimulus/SSBGenerator.cpp
  src/stimulus/TTSEngine.cpp
  src/stimulus/MorseGenerator.cpp
  src/stimulus/StimulusManager.cpp
  src/stimulus/SweepGenerator.cpp
  src/stimulus/StimulusLua.cpp
  src/sampler/ADCSampler.cpp
  src/xyce/XyceWrapper.cpp
  src/orchestrator/Orchestrator.cpp
)

# Twin - Full Xyce SPICE simulation
add_executable(nexrx-twin
  src/main.cpp
  ${COMMON_SOURCES}
)

# SignalGen - Fast functional simulator
add_executable(nexrx-signalgen
  src/SignalGen.cpp
  ${COMMON_SOURCES}
)

# Libraries
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(LUA54 lua5.4)
  pkg_check_modules(ESPEAK espeak-ng)
endif()

# Fallback for Lua
if(NOT LUA54_FOUND)
  find_package(Lua REQUIRED)
  set(LUA54_LIBRARIES ${LUA_LIBRARIES})
  set(LUA54_INCLUDE_DIRS ${LUA_INCLUDE_DIR})
endif()

foreach(target nexrx-twin nexrx-signalgen)
  target_link_libraries(${target} PRIVATE 
    tinycbor
    sol2::sol2
    ${LUA54_LIBRARIES}
    ${XYCE_LIBRARY}
    ${TRILINOS_LIBRARIES}
    ${FFTW3_LIBRARY}
    ${AMD_LIBRARY}
    ${LAPACK_LIBRARIES}
    ${BLAS_LIBRARIES}
    ${GFORTRAN_LIBRARY}
    ${QUADMATH_LIBRARY}
    pthread 
    dl
    m
  )
  target_include_directories(${target} PRIVATE ${LUA54_INCLUDE_DIRS})
  
  if(ESPEAK_FOUND)
    target_link_libraries(${target} PRIVATE ${ESPEAK_LIBRARIES})
    target_include_directories(${target} PRIVATE ${ESPEAK_INCLUDE_DIRS})
  endif()

  if(UNIX AND NOT APPLE)
    target_link_libraries(${target} PRIVATE rt)
  endif()
endforeach()
