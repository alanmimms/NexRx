cmake_minimum_required(VERSION 3.20)
project(uitest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CheckCXXCompilerFlag)
check_cxx_compiler_flag(-Wno-template-body COMPILER_SUPPORTS_WNO_TEMPLATE_BODY)
if(COMPILER_SUPPORTS_WNO_TEMPLATE_BODY)
  add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wno-template-body>)
endif()

include(FetchContent)

# sol2
if(NOT TARGET sol2)
  FetchContent_Declare(
    sol2
    GIT_REPOSITORY https://github.com/ThePhD/sol2.git
    GIT_TAG v3.3.0
  )
  FetchContent_MakeAvailable(sol2)
endif()

# raylib
if(NOT TARGET raylib)
  FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 6.0
  )
  set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(raylib)
endif()

# Lua 5.4 - FetchContent pinned to v5.4.9
if(TARGET lua)
  # Target already defined
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../app/build/_deps/lua-src")
  set(lua_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../app/build/_deps/lua-src")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../twin/build/_deps/lua-src")
  set(lua_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../twin/build/_deps/lua-src")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/build/_deps/lua-src")
  set(lua_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/build/_deps/lua-src")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../build/_deps/lua-src")
  set(lua_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../build/_deps/lua-src")
else()
  FetchContent_Declare(
    lua
    GIT_REPOSITORY https://github.com/lua/lua.git
    GIT_TAG v5.4.9
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

# Find Dependencies
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(SDL2 sdl2)
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
  ${SDL2_INCLUDE_DIRS}
  ${OPENGL_INCLUDE_DIR}
)

target_link_libraries(uitest PRIVATE
  raylib
  sol2::sol2
  lua
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
