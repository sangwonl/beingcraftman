add_executable(gazeshot_tests
	test_main.cpp
	test_types.cpp
	test_math_vec.cpp
	test_math_mat.cpp
	test_math_transform.cpp
)

target_link_libraries(gazeshot_tests PRIVATE
	gazeshot_core
	doctest::doctest
)

add_test(NAME gazeshot_tests COMMAND gazeshot_tests)