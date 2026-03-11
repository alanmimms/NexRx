cmake_minimum_required(VERSION 3.20)
project(NexRxApp)

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

include_directories(src ../twin/src extern)

add_executable(nexrx-app
  src/main.cpp
  src/DSPEngine.cpp
  src/GUIEngine.cpp
  src/TwinConn.cpp
  src/TCPControlClient.cpp
  src/UDPStreamClient.cpp
  src/Socket.cpp
  src/AudioEngine.cpp
  src/BasebandFilter.cpp
  src/FontRenderer.cpp
  src/WaterfallRenderer.cpp
)

# Find Dependencies
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(LUA54 lua5.4)
  pkg_check_modules(SDL2 sdl2)
endif()

# Fallback for Lua
if(NOT LUA54_FOUND)
  find_package(Lua REQUIRED)
  set(LUA54_LIBRARIES ${LUA_LIBRARIES})
  set(LUA54_INCLUDE_DIRS ${LUA_INCLUDE_DIR})
endif()

# Fallback for SDL2
if(NOT SDL2_FOUND)
  find_package(SDL2 CONFIG REQUIRED)
  set(SDL2_LIBRARIES SDL2::SDL2)
endif()

# OpenGL
find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)

target_include_directories(nexrx-app PRIVATE 
  ${LUA54_INCLUDE_DIRS}
  ${SDL2_INCLUDE_DIRS}
  ${OPENGL_INCLUDE_DIR}
)

target_link_libraries(nexrx-app PRIVATE 
  tinycbor
  sol2::sol2
  ${LUA54_LIBRARIES}
  ${SDL2_LIBRARIES}
  ${OPENGL_LIBRARIES}
  Threads::Threads
  pthread
  m
)

# Platform-specific audio libraries for miniaudio
if(APPLE)
  find_library(COREAUDIO_LIBRARY CoreAudio)
  find_library(AUDIOUNIT_LIBRARY AudioUnit)
  find_library(COREFOUNDATION_LIBRARY CoreFoundation)
  target_link_libraries(nexrx-app PRIVATE ${COREAUDIO_LIBRARY} ${AUDIOUNIT_LIBRARY} ${COREFOUNDATION_LIBRARY})
elseif(WIN32)
  target_link_libraries(nexrx-app PRIVATE winmm ws2_32)
else()
  target_link_libraries(nexrx-app PRIVATE dl)
endif()
