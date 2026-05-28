cmake_minimum_required(VERSION 3.20)
project(chess3d VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# Build auto-contido p/ distribuição: Windows = .exe único auto-extraível
# (assets+engines embutidos via RCDATA); Linux = binário p/ empacotar em AppImage.
option(CHESS3D_PACKAGE "Gera build auto-contido (assets/engines embutidos)" OFF)

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

install(TARGETS chess3d RUNTIME DESTINATION bin)

# ─── Packaging auto-contido ──────────────────────────────────────────────────
if(CHESS3D_PACKAGE)
    target_compile_definitions(chess3d_lib ${CHESS3D_LIB_SCOPE}
        CHESS3D_PACKAGED
        CHESS3D_VERSION="${PROJECT_VERSION}")

    if(WIN32)
        # Baixa Stockfish + Berserk → ${CHESS3D_ENGINES_STAGING}
        include(${CMAKE_SOURCE_DIR}/cmake/FetchEngines.cmake)

        set(_payload_dir "${CMAKE_BINARY_DIR}/payload")
        set(_payload_zip "${CMAKE_BINARY_DIR}/payload.zip")

        # Monta a árvore de assets (sem .bak) + engines e zipa em payload.zip.
        add_custom_command(
            OUTPUT  "${_payload_zip}"
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${_payload_dir}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${_payload_dir}/assets/models"
            COMMAND ${CMAKE_COMMAND} -E copy
                    "${CMAKE_SOURCE_DIR}/assets/models/chessboard.glb"
                    "${_payload_dir}/assets/models/"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${CMAKE_SOURCE_DIR}/assets/textures" "${_payload_dir}/assets/textures"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${CMAKE_SOURCE_DIR}/assets/shaders"  "${_payload_dir}/assets/shaders"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${CHESS3D_ENGINES_STAGING}" "${_payload_dir}/assets/engines"
            COMMAND ${CMAKE_COMMAND} -E chdir "${_payload_dir}"
                    ${CMAKE_COMMAND} -E tar cf "${_payload_zip}" --format=zip assets
            DEPENDS "${CHESS3D_ENGINES_STAGING}/stockfish.exe"
                    "${CHESS3D_ENGINES_STAGING}/berserk.exe"
                    "${CMAKE_SOURCE_DIR}/assets/models/chessboard.glb"
            COMMENT "Empacotando payload.zip (assets + engines)"
            VERBATIM)

        # Embute payload.zip como RCDATA via windres.
        enable_language(RC)
        configure_file(
            "${CMAKE_SOURCE_DIR}/packaging/windows/resources.rc.in"
            "${CMAKE_BINARY_DIR}/resources.rc" @ONLY)
        set_source_files_properties("${CMAKE_BINARY_DIR}/resources.rc"
            PROPERTIES OBJECT_DEPENDS "${_payload_zip}")
        target_sources(chess3d PRIVATE "${CMAKE_BINARY_DIR}/resources.rc")

        # Link estático: .exe sem DLLs do MinGW (só usa opengl32.dll do sistema).
        target_link_options(chess3d PRIVATE -static -static-libgcc -static-libstdc++)
        message(STATUS "chess3d: packaging Windows (.exe auto-extraível) habilitado")
    else()
        message(STATUS "chess3d: packaging Linux (binário p/ AppImage) habilitado")
    endif()
endif()

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
        # Testes rápidos (sem tag [slow])
        catch_discover_tests(chess3d_tests
            TEST_SPEC "~[slow]"
            PROPERTIES LABELS "fast"
        )
        # Testes lentos (tag [slow] — perft, profundidade 4, etc.)
        catch_discover_tests(chess3d_tests
            TEST_SPEC "[slow]"
            TEST_SUFFIX " [slow]"
            PROPERTIES LABELS "slow"
        )
    endif()
endif()

message(STATUS "chess3d configured: C++${CMAKE_CXX_STANDARD}, build type=${CMAKE_BUILD_TYPE}")
