# 현재 디렉토리의 모든 .cpp 파일 찾기
file(GLOB CPP_FILES "*.cpp")

# 각 .cpp 파일마다 실행 파일 생성
foreach(CPP_FILE ${CPP_FILES})
    # 파일명에서 확장자 제거하여 실행파일 이름 생성
    get_filename_component(EXECUTABLE_NAME ${CPP_FILE} NAME_WE)
    add_executable(${EXECUTABLE_NAME} ${CPP_FILE})
endforeach()
