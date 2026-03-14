cmake_minimum_required(VERSION 3.12)
project(NexRxUITest)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Raylib
FetchContent_Declare(
  raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG 5.0
)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib)

# sol2
FetchContent_Declare(
  sol2
  GIT_REPOSITORY https://github.com/ThePhD/sol2.git
  GIT_TAG v3.3.0
)
FetchContent_MakeAvailable(sol2)

include_directories(src ../app/extern)

add_executable(uitest-app
  src/main.cpp
  src/GUIEngine.cpp
  src/FontRenderer.cpp
)

# Find Lua
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(LUA54 lua5.4)
endif()

if(NOT LUA54_FOUND)
  find_package(Lua REQUIRED)
  set(LUA54_LIBRARIES ${LUA_LIBRARIES})
  set(LUA54_INCLUDE_DIRS ${LUA_INCLUDE_DIR})
endif()

target_include_directories(uitest-app PRIVATE 
  ${LUA54_INCLUDE_DIRS}
  ${raylib_SOURCE_DIR}/src
)

target_link_libraries(uitest-app PRIVATE 
  raylib
  sol2::sol2
  ${LUA54_LIBRARIES}
)

# Platform-specific libs for raylib/sol2
if(APPLE)
  find_library(COREAUDIO_LIBRARY CoreAudio)
  find_library(AUDIOUNIT_LIBRARY AudioUnit)
  find_library(COREFOUNDATION_LIBRARY CoreFoundation)
  find_library(IOKIT_LIBRARY IOKit)
  find_library(COCOA_LIBRARY Cocoa)
  find_library(OPENGL_LIBRARY OpenGL)
  target_link_libraries(uitest-app PRIVATE ${COREAUDIO_LIBRARY} ${AUDIOUNIT_LIBRARY} ${COREFOUNDATION_LIBRARY} ${IOKIT_LIBRARY} ${COCOA_LIBRARY} ${OPENGL_LIBRARY})
elseif(WIN32)
  target_link_libraries(uitest-app PRIVATE winmm ws2_32 gdi32 user32 shell32)
else()
  find_package(Threads REQUIRED)
  target_link_libraries(uitest-app PRIVATE m pthread dl GL rt X11)
endif()
