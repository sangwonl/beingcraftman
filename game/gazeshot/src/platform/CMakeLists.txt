add_library(gazeshot_platform STATIC
	src/Window.cpp
)

target_include_directories(gazeshot_platform PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(gazeshot_platform
    PUBLIC gazeshot_core
	PRIVATE SDL3::SDL3
)