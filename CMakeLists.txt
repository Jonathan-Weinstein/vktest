cmake_minimum_required(VERSION 2.8)

project(vktest)
add_executable(${PROJECT_NAME} "main.cpp" "vk_simple_init.cpp" "ext_raster_multisample_test.cpp" "unity_build.cpp" "vk_util.cpp" "uav_load_oob.cpp" "clipdistance_tessellation.cpp" "xfb_pingpong_bug.cpp" "yuy2_r32_copy.cpp")
target_link_libraries(${PROJECT_NAME} dl)
add_definitions(-DVK_NO_PROTOTYPES)
