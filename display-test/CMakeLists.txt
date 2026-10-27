cmake_minimum_required(VERSION 3.20.0)
list(APPEND BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
list(APPEND DTS_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

target_compile_features(app PRIVATE cxx_std_20)

find_package(Python3 REQUIRED)

set(fontScript ${CMAKE_CURRENT_SOURCE_DIR}/ttf-to-cxx.py)
target_include_directories(app PRIVATE src)
zephyr_include_directories(${CMAKE_CURRENT_BINARY_DIR})

function(genFont ttf points fontName)
  cmake_parse_arguments(PARSE_ARGV 3 FONT "" "WEIGHT;INDEX;VARIATION" "")

  set(extraArgs "")
  if(FONT_WEIGHT)
    list(APPEND extraArgs --weight ${FONT_WEIGHT})
  endif()
  if(FONT_INDEX)
    list(APPEND extraArgs --index ${FONT_INDEX})
  endif()
  if(FONT_VARIATION)
    list(APPEND extraArgs --variation ${FONT_VARIATION})
  endif()


  set(fontOut ${CMAKE_CURRENT_BINARY_DIR}/${fontName}.cxx)

  target_sources(app PRIVATE ${fontOut})

  add_custom_command(
    OUTPUT  ${fontOut}
    COMMAND ${Python3_EXECUTABLE} ${fontScript} ${ttf} ${points} --name ${fontName} ${extraArgs} > ${fontOut}
    DEPENDS ${fontScript} ${ttf}
    COMMENT "Generating MONO_VTILED C++ font ${fontName} from TTF..."
    VERBATIM
  )
endfunction()

genFont(/usr/share/fonts/truetype/ubuntu/Ubuntu-B.ttf 16 UbuntuB16 VARIATION "Bold")
genFont(/usr/share/fonts/truetype/ubuntu/Ubuntu-M.ttf 15 UbuntuM15)
genFont(/usr/share/fonts/truetype/ubuntu/Ubuntu-B.ttf 14 UbuntuB14 VARIATION "Bold")
genFont(/usr/share/fonts/truetype/ubuntu/Ubuntu-M.ttf 13 UbuntuM13)
genFont(/usr/share/fonts/truetype/ubuntu/Ubuntu-B.ttf 12 UbuntuB12 VARIATION "Bold")
genFont(/usr/share/fonts/truetype/ibm-plex/IBMPlexSans-Light.ttf 12 PlexL12)

project(display)

target_include_directories(app PRIVATE ${fontGenDir})
add_subdirectory(drivers)
target_sources(app PRIVATE src/main.cxx ${FONT_OUT})
