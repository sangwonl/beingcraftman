cmake_minimum_required(VERSION 3.10)
project(MathPractices)

# C++20 표준 사용
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 디버그 빌드 설정 (디버깅 심볼 포함)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")

add_subdirectory(math/practices)
