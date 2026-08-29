cmake_minimum_required(VERSION 3.20)
project(NexRx)

# Global language settings for the architecture
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Hardware subprojects
add_subdirectory(fpga)

# Future software subprojects (to be uncommented as they are integrated)
# add_subdirectory(app)
# add_subdirectory(fw)
# add_subdirectory(test-app)
# add_subdirectory(twin)
