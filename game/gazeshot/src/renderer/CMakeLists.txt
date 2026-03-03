add_library(gazeshot_renderer INTERFACE)

target_include_directories(gazeshot_renderer INTERFACE
	${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(gazeshot_renderer INTERFACE gazeshot_core)