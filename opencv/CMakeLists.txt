include(FetchContent)

set(BUILD_LIST core,imgcodecs,highgui CACHE STRING "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_PERF_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_opencv_apps OFF CACHE BOOL "" FORCE)
set(BUILD_opencv_python_bindings_generator OFF CACHE BOOL "" FORCE)
set(BUILD_JAVA OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    OpenCV
    URL https://github.com/opencv/opencv/archive/refs/tags/4.10.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(OpenCV)

if(UNIX AND NOT APPLE AND NOT GTK3_FOUND AND NOT GTK2_FOUND)
    message(FATAL_ERROR "OpenCV HighGUI requires GTK. Install libgtk-3-dev and configure again.")
endif()

file(GLOB CPP_FILES "*.cpp")

foreach(CPP_FILE ${CPP_FILES})
    get_filename_component(EXECUTABLE_NAME ${CPP_FILE} NAME_WE)
    add_executable(${EXECUTABLE_NAME} ${CPP_FILE})
    set_target_properties(${EXECUTABLE_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    target_include_directories(${EXECUTABLE_NAME} SYSTEM PRIVATE
        ${opencv_SOURCE_DIR}/modules/core/include
        ${opencv_SOURCE_DIR}/modules/imgproc/include
        ${opencv_SOURCE_DIR}/modules/imgcodecs/include
        ${opencv_SOURCE_DIR}/modules/highgui/include
        ${CMAKE_BINARY_DIR}
    )
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE opencv_core opencv_imgcodecs opencv_highgui)
endforeach()
