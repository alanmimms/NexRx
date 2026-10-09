cmake_minimum_required(VERSION 3.20)
project(NexRxTwin)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_definitions(SOL_HAS_STD_OPTIONAL=1 SOL_USE_STD_OPTIONAL=1)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
endif()

include(CheckCXXCompilerFlag)
check_cxx_compiler_flag(-Wno-template-body COMPILER_SUPPORTS_WNO_TEMPLATE_BODY)
if(COMPILER_SUPPORTS_WNO_TEMPLATE_BODY)
  add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wno-template-body>)
endif()

include(FetchContent)

# tinycbor - lightweight CBOR encoder/decoder
if(NOT TARGET tinycbor)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../app/build/_deps/tinycbor-src")
    set(tinycbor_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../app/build/_deps/tinycbor-src")
  elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../test-app/build/_deps/tinycbor-src")
    set(tinycbor_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../test-app/build/_deps/tinycbor-src")
  elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/build/_deps/tinycbor-src")
    set(tinycbor_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/build/_deps/tinycbor-src")
  elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../build/_deps/tinycbor-src")
    set(tinycbor_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../build/_deps/tinycbor-src")
  else()
    FetchContent_Declare(
      tinycbor
      GIT_REPOSITORY https://github.com/intel/tinycbor.git
      GIT_TAG v0.6.0
    )
    FetchContent_GetProperties(tinycbor)
    if(NOT tinycbor_POPULATED)
      FetchContent_Populate(tinycbor)
    endif()
  endif()

  add_library(tinycbor STATIC
    ${tinycbor_SOURCE_DIR}/src/cborencoder.c
    ${tinycbor_SOURCE_DIR}/src/cborparser.c
    ${tinycbor_SOURCE_DIR}/src/cborerrorstrings.c
    ${tinycbor_SOURCE_DIR}/src/cborvalidation.c
  )
  target_include_directories(tinycbor PUBLIC ${tinycbor_SOURCE_DIR}/src)
endif()

# sol2 is header-only, for Lua bindings
if(NOT TARGET sol2)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../app/build/_deps/sol2-src")
    add_library(sol2 INTERFACE)
    add_library(sol2::sol2 ALIAS sol2)
    target_include_directories(sol2 INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/../app/build/_deps/sol2-src/include")
  elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../uitest/build/_deps/sol2-src")
    add_library(sol2 INTERFACE)
    add_library(sol2::sol2 ALIAS sol2)
    target_include_directories(sol2 INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/../uitest/build/_deps/sol2-src/include")
  elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/build/_deps/sol2-src")
    add_library(sol2 INTERFACE)
    add_library(sol2::sol2 ALIAS sol2)
    target_include_directories(sol2 INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/build/_deps/sol2-src/include")
  elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../build/_deps/sol2-src")
    add_library(sol2 INTERFACE)
    add_library(sol2::sol2 ALIAS sol2)
    target_include_directories(sol2 INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/../build/_deps/sol2-src/include")
  else()
    FetchContent_Declare(
      sol2
      GIT_REPOSITORY https://github.com/ThePhD/sol2.git
      GIT_TAG v3.3.0
    )
    FetchContent_MakeAvailable(sol2)
  endif()
endif()

# Lua 5.4 - FetchContent pinned to v5.4.8
if(TARGET lua)
  # Target already defined
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../app/build/_deps/lua-src")
  set(lua_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../app/build/_deps/lua-src")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../uitest/build/_deps/lua-src")
  set(lua_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../uitest/build/_deps/lua-src")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/build/_deps/lua-src")
  set(lua_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/build/_deps/lua-src")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../build/_deps/lua-src")
  set(lua_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../build/_deps/lua-src")
else()
  FetchContent_Declare(
    lua
    GIT_REPOSITORY https://github.com/lua/lua.git
    GIT_TAG v5.4.8
  )
  FetchContent_GetProperties(lua)
  if(NOT lua_POPULATED)
    FetchContent_Populate(lua)
  endif()
endif()

if(NOT TARGET lua)
  if(EXISTS "${lua_SOURCE_DIR}/onelua.c")
    add_library(lua STATIC ${lua_SOURCE_DIR}/onelua.c)
    target_compile_definitions(lua PRIVATE MAKE_LIB)
  else()
    file(GLOB LUA_SOURCES "${lua_SOURCE_DIR}/*.c")
    list(FILTER LUA_SOURCES EXCLUDE REGEX ".*(lua|luac|ltests)\\.c$")
    add_library(lua STATIC ${LUA_SOURCES})
  endif()
  add_library(lua::lua ALIAS lua)
  set_target_properties(lua PROPERTIES C_STANDARD 99)
  target_include_directories(lua PUBLIC ${lua_SOURCE_DIR})
  if(UNIX AND NOT APPLE)
    target_compile_definitions(lua PRIVATE LUA_USE_LINUX)
    target_link_libraries(lua PUBLIC m dl)
  elseif(APPLE)
    target_compile_definitions(lua PRIVATE LUA_USE_MACOSX)
    target_link_libraries(lua PUBLIC m)
  endif()
endif()

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

option(BUILD_TWIN_XYCE "Build full Xyce SPICE simulator twin" OFF)

# System libraries (for Xyce SPICE twin)
if(BUILD_TWIN_XYCE AND EXISTS "${XYCE_LIBRARY}")
  find_library(FFTW3_LIBRARY fftw3 HINTS /usr/lib/x86_64-linux-gnu)
  find_library(AMD_LIBRARY amd HINTS /usr/lib/x86_64-linux-gnu)
  find_package(LAPACK QUIET)
  find_package(BLAS QUIET)
  find_library(GFORTRAN_LIBRARY gfortran HINTS /usr/lib/x86_64-linux-gnu /usr/lib/gcc/x86_64-linux-gnu/13 /usr/lib/gcc/x86_64-linux-gnu/14 /usr/lib/gcc/x86_64-linux-gnu/12)
  find_library(QUADMATH_LIBRARY quadmath HINTS /usr/lib/x86_64-linux-gnu /usr/lib/gcc/x86_64-linux-gnu/13 /usr/lib/gcc/x86_64-linux-gnu/14 /usr/lib/gcc/x86_64-linux-gnu/12)
endif()

# Common sources for both executables
set(SIGNALGEN_SOURCES
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
)

set(COMMON_SOURCES
  ${SIGNALGEN_SOURCES}
  src/sampler/ADCSampler.cpp
  src/xyce/XyceWrapper.cpp
  src/orchestrator/Orchestrator.cpp
)

# Twin - Full Xyce SPICE simulation (if enabled and Xyce is built)
if(BUILD_TWIN_XYCE AND EXISTS "${XYCE_LIBRARY}")
add_executable(nexrx-twin
  src/main.cpp
  ${COMMON_SOURCES}
)
endif()

# SignalGen - Fast functional simulator
add_executable(nexrx-signalgen
  src/SignalGen.cpp
  ${SIGNALGEN_SOURCES}
)

target_compile_options(nexrx-signalgen PRIVATE -O3 -march=native)

# Libraries
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(ESPEAK espeak-ng)
endif()

target_link_libraries(nexrx-signalgen PRIVATE 
  tinycbor
  sol2::sol2
  lua
  pthread 
  dl
  m
)

if(ESPEAK_FOUND)
  target_link_libraries(nexrx-signalgen PRIVATE ${ESPEAK_LIBRARIES})
  target_include_directories(nexrx-signalgen PRIVATE ${ESPEAK_INCLUDE_DIRS})
endif()

if(UNIX AND NOT APPLE)
  target_link_libraries(nexrx-signalgen PRIVATE rt)
endif()

if(BUILD_TWIN_XYCE AND EXISTS "${XYCE_LIBRARY}")
  target_link_libraries(nexrx-twin PRIVATE 
    tinycbor
    sol2::sol2
    lua
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
endif()
