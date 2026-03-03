add_library(gazeshot_engine INTERFACE)

target_include_directories(gazeshot_engine INTERFACE
	${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(gazeshot_engine INTERFACE gazeshot_renderer)