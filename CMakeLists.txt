cmake_minimum_required(VERSION 3.0)

project(poseidons-plunder)

set(CMAKE_CXX_STANDARD 14)

find_package(OpenGL REQUIRED)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")

    set(KIT_DIR "${CMAKE_SOURCE_DIR}/kit-libs-win")
    set(SDL2_INCLUDE_DIRS "${KIT_DIR}/out/include/SDL2")
    set(SDL2_LIBRARIES "${KIT_DIR}/out/lib/SDL2.lib" "${KIT_DIR}/out/lib/SDL2main.lib")
    set(PNG_INCLUDE_DIRS "${KIT_DIR}/out/libpng")
    set(PNG_LIBRARIES "${KIT_DIR}/out/libpng/libpng.lib" "${KIT_DIR}/out/zlib/zlib.lib")
    set(GLM_INCLUDE_DIRS "${KIT_DIR}/out/include")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4146 /wd4297")

else ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")

    find_package(SDL2 REQUIRED)
    find_package(glm REQUIRED)
    find_package(PNG REQUIRED)
    find_package(Bullet REQUIRED)

endif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")

set(COMMON
        Connection.cpp
        GameState.cpp
        Scene.cpp
        data_path.cpp
        Load.cpp)

set(SERVER_FILES
        server.cpp)

set(CLIENT_FILES
        load_save_png.cpp
        main.cpp
        compile_program.cpp
        vertex_color_program.cpp
        texture_program.cpp
        depth_program.cpp
        Mode.cpp
        GameMode.cpp
        MenuMode.cpp
        LobbyMode.cpp
        MeshBuffer.cpp
        draw_text.cpp
        Sound.cpp)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    set(CLIENT_FILES ${CLIENT_FILES} gl_shims.cpp)
endif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")

add_custom_target(SDL2CopyBinaries
        COMMAND ${CMAKE_COMMAND} -E copy ${KIT_DIR}/out/dist/SDL2.dll ${CMAKE_BINARY_DIR}/
        )

add_custom_target(CopyAssets
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/dist/menu.p ${CMAKE_BINARY_DIR}/
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/dist/test_level_complex.pnc ${CMAKE_BINARY_DIR}/
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/dist/test_level_complex.scene ${CMAKE_BINARY_DIR}/
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/dist/test_level_complex.collision ${CMAKE_BINARY_DIR}/
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/dist/textures ${CMAKE_BINARY_DIR}/textures
)

add_executable(client ${COMMON} ${CLIENT_FILES})

target_include_directories(client PUBLIC ${SDL2_INCLUDE_DIRS} ${PNG_INCLUDE_DIRS} ${GLM_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS})

target_link_libraries(client ${OPENGL_LIBRARIES} ${SDL2_LIBRARIES} ${PNG_LIBRARIES} ${BULLET_LIBRARIES})

add_executable(server ${COMMON} ${SERVER_FILES})

target_include_directories(server PUBLIC ${SDL2_INCLUDE_DIRS} ${PNG_INCLUDE_DIRS} ${GLM_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS})

target_link_libraries(server ${OPENGL_LIBRARIES} ${SDL2_LIBRARIES} ${PNG_LIBRARIES} ${BULLET_LIBRARIES})

add_dependencies(client CopyAssets)
add_dependencies(server CopyAssets)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    add_dependencies(client SDL2CopyBinaries)
endif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")