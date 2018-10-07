# CMakeLists for FAudio
# Initial contribution by Renaud "cybik" Lepage

cmake_minimum_required(VERSION 3.1)
project(FAudio)
enable_language(C)

include(${CMAKE_CURRENT_LIST_DIR}/src/FAudio.cmake)

include_directories(${CMAKE_CURRENT_LIST_DIR}/src)

add_library(
    FAudio
    SHARED
        ${FAUDIO_SRC}
        ${FAUDIO_H}
)