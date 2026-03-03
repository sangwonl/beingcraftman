add_library(gazeshot_core INTERFACE) # 헤더 온리

target_include_directories(gazeshot_core INTERFACE
	${CMAKE_CURRENT_SOURCE_DIR}/include
)