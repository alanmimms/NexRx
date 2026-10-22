cmake_minimum_required(VERSION 3.20.0)
list(APPEND BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
list(APPEND DTS_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(display)

include(${ZEPHYR_BASE}/cmake/cfb.cmake NO_POLICY_SCOPE)

set(fontGenDir ${ZEPHYR_BINARY_DIR}/include/generated)
set(fontHeader ${fontGenDir}/cfb_font_12_7x16.h)

generate_cfb_font_for_target(
  app
  "/usr/share/fonts/truetype/ubuntu/UbuntuMono-B.ttf"
  ${fontHeader}
  7
  16
  -s 12
  --first 32
  --last 126
)

target_include_directories(app PRIVATE ${fontGenDir})
add_subdirectory(drivers)
target_sources(app PRIVATE src/main.c)
