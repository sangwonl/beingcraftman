cmake_minimum_required(VERSION 3.24)

if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.0")
	set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif()

project(gazeshot VERSION 0.1.0 LANGUAGES CXX)

# C++20 표준
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) # GNU 확장 비활성비 -> 이식성
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 빌드 타입 기본값
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Debug)
endif()

# 공통 컴파일 옵션
add_compile_options(-Wall -Wextra -Wpedantic)

# 외부 의존성
include(cmake/Dependencies.cmake)

# 모듈 (아래에서 위로, 의존 순서)
add_subdirectory(core)
add_subdirectory(platform)
add_subdirectory(renderer)
add_subdirectory(engine)
add_subdirectory(game)

# 테스트
enable_testing()
add_subdirectory(tests)

