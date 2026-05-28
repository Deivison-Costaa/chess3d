cmake_minimum_required(VERSION 3.20)
project(chess3d LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(glfw3 CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(Stb REQUIRED)
find_path(TINYGLTF_INCLUDE_DIR "tiny_gltf.h" REQUIRED)

file(GLOB_RECURSE CHESS3D_LIB_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
)
list(FILTER CHESS3D_LIB_SOURCES EXCLUDE REGEX ".*/main\\.cpp$")

if(CHESS3D_LIB_SOURCES)
    add_library(chess3d_lib STATIC ${CHESS3D_LIB_SOURCES})
else()
    add_library(chess3d_lib INTERFACE)
endif()

if(TARGET chess3d_lib AND CHESS3D_LIB_SOURCES)
    set(CHESS3D_LIB_SCOPE PUBLIC)
else()
    set(CHESS3D_LIB_SCOPE INTERFACE)
endif()

target_include_directories(chess3d_lib ${CHESS3D_LIB_SCOPE}
    ${CMAKE_SOURCE_DIR}/src
    ${Stb_INCLUDE_DIR}
    ${TINYGLTF_INCLUDE_DIR}
)
find_package(Threads REQUIRED)

target_link_libraries(chess3d_lib ${CHESS3D_LIB_SCOPE}
    glfw
    glad::glad
    glm::glm
    imgui::imgui
    spdlog::spdlog
    Threads::Threads
)

if(WIN32)
    target_link_libraries(chess3d_lib ${CHESS3D_LIB_SCOPE} ws2_32)
endif()
target_compile_definitions(chess3d_lib ${CHESS3D_LIB_SCOPE}
    GLFW_INCLUDE_NONE
    CHESS3D_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets"
)

add_executable(chess3d src/main.cpp)
target_link_libraries(chess3d PRIVATE chess3d_lib)

option(CHESS3D_BUILD_TESTS "Build unit tests" ON)
if(CHESS3D_BUILD_TESTS)
    find_package(Catch2 3 CONFIG REQUIRED)
    enable_testing()

    file(GLOB CHESS3D_TEST_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/tests/*.cpp"
    )

    if(CHESS3D_TEST_SOURCES)
        add_executable(chess3d_tests ${CHESS3D_TEST_SOURCES})
        target_link_libraries(chess3d_tests PRIVATE chess3d_lib Catch2::Catch2WithMain)
        include(CTest)
        include(Catch)
        catch_discover_tests(chess3d_tests)
    endif()
endif()

message(STATUS "chess3d configured: C++${CMAKE_CXX_STANDARD}, build type=${CMAKE_BUILD_TYPE}")
