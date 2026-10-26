cmake_minimum_required(VERSION 3.20.0)
list(APPEND BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
list(APPEND DTS_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

target_compile_features(app PRIVATE cxx_std_20)

find_package(Python3 REQUIRED)

target_include_directories(app PRIVATE src})
target_sources(app PRIVATE ${FONT_OUT})
zephyr_include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(FONT_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/ttf-to-cxx.py)
set(FONT_TTF    /usr/share/fonts/truetype/ubuntu/Ubuntu-M.ttf)
set(FONT_NAME   UbuntuM16)
set(FONT_OUT    ${CMAKE_CURRENT_BINARY_DIR}/${FONT_NAME}.cxx)

add_custom_command(
    OUTPUT  ${FONT_OUT}
    COMMAND ${Python3_EXECUTABLE} ${FONT_SCRIPT} ${FONT_TTF} 16 --name ${FONT_NAME} > ${FONT_OUT}
    DEPENDS ${FONT_SCRIPT} ${FONT_TTF}
    COMMENT "Generating MONO_VTILED C++ font ${FONT_NAME} from TTF..."
)

project(display)

target_include_directories(app PRIVATE ${fontGenDir})
add_subdirectory(drivers)
target_sources(app PRIVATE src/main.cxx)
