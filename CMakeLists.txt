cmake_minimum_required(VERSION 3.8)

project(ecs)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS "ON")

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

add_subdirectory(thirdparty/entt)

file(GLOB_RECURSE ECS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp") # sources
add_executable(ecs ${ECS_SOURCES})
target_include_directories(ecs PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src") # headers
target_link_libraries(ecs EnTT)