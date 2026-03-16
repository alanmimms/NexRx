cmake_minimum_required(VERSION 3.20)
project(uitest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# sol2
FetchContent_Declare(
  sol2
  GIT_REPOSITORY https://github.com/ThePhD/sol2.git
  GIT_TAG v3.3.0
)
FetchContent_MakeAvailable(sol2)

# raylib
FetchContent_Declare(
  raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG 5.0
)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib)

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
  if(TARGET SDL2::SDL2)
    set(SDL2_LIBRARIES SDL2::SDL2)
  else()
    set(SDL2_LIBRARIES SDL2)
  endif()
endif()

# OpenGL
find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)

add_executable(uitest
  src/main.cpp
  src/UIEngine.cpp
  src/LuaBridge.cpp
)

target_include_directories(uitest PRIVATE
  src
  ${LUA54_INCLUDE_DIRS}
  ${SDL2_INCLUDE_DIRS}
  ${OPENGL_INCLUDE_DIR}
)

target_link_libraries(uitest PRIVATE
  raylib
  sol2::sol2
  ${LUA54_LIBRARIES}
  ${SDL2_LIBRARIES}
  ${OPENGL_LIBRARIES}
  Threads::Threads
)

if(APPLE)
  find_library(COREAUDIO_LIBRARY CoreAudio)
  find_library(AUDIOUNIT_LIBRARY AudioUnit)
  find_library(COREFOUNDATION_LIBRARY CoreFoundation)
  target_link_libraries(uitest PRIVATE ${COREAUDIO_LIBRARY} ${AUDIOUNIT_LIBRARY} ${COREFOUNDATION_LIBRARY})
elseif(WIN32)
  target_link_libraries(uitest PRIVATE winmm ws2_32)
else()
  target_link_libraries(uitest PRIVATE dl m)
endif()

# Copy fonts and lua files to build directory
file(COPY fonts DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY lua DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
