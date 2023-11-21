cmake_minimum_required(VERSION 3.10)
project(halo-mqtt-qt)

set(THIRDPARTY_DIR ${CMAKE_CURRENT_LIST_DIR}/3rdparty)

add_subdirectory(3rdparty)
add_subdirectory(src)
