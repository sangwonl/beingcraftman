cmake_minimum_required(VERSION 3.22)
project(beingcraftman_opengl LANGUAGES C CXX)

set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)

add_subdirectory(external/glfw-3.3-stable)
add_subdirectory(external/glm-1.0.3)
add_subdirectory(external/assimp)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(assimp PRIVATE -Wno-unsafe-buffer-usage)
endif()

# GLAD 설정
set(GLAD_INCLUDE external/glad/include)
set(GLAD_SRC external/glad/src/glad.c)
set(STB_INCLUDE external/stb)

# 현재 디렉토리의 모든 .cpp 파일 찾기
file(GLOB CPP_FILES "*.cpp")

# 각 .cpp 파일마다 실행 파일 생성
foreach(CPP_FILE ${CPP_FILES})
    # 파일명에서 확장자 제거하여 실행파일 이름 생성
    get_filename_component(EXECUTABLE_NAME ${CPP_FILE} NAME_WE)
    add_executable(${EXECUTABLE_NAME} ${CPP_FILE} ${GLAD_SRC})
    set_target_properties(${EXECUTABLE_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    target_include_directories(${EXECUTABLE_NAME} PRIVATE ${GLAD_INCLUDE} ${STB_INCLUDE})
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE glfw glm::glm assimp::assimp)
endforeach()
