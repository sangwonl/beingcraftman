add_subdirectory(external/glfw-3.3-stable)

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
    target_include_directories(${EXECUTABLE_NAME} PRIVATE ${GLAD_INCLUDE} ${STB_INCLUDE})
    target_link_libraries(${EXECUTABLE_NAME} glfw)
endforeach()

