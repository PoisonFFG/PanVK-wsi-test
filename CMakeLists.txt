cmake_minimum_required(VERSION 3.22.1)
project(panvk_wsi_test LANGUAGES CXX)

add_library(panvk_wsi_test SHARED panvk_wsi_test.cpp)

find_library(log-lib log)
find_library(android-lib android)
find_library(vulkan-lib vulkan)

target_link_libraries(panvk_wsi_test
    ${log-lib}
    ${android-lib}
    ${vulkan-lib}
)
