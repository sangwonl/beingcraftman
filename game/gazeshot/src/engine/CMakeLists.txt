add_library(gazeshot_engine STATIC
	src/Input.cpp
)

target_include_directories(gazeshot_engine PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(gazeshot_engine
	PUBLIC gazeshot_core
	PUBLIC gazeshot_renderer
)