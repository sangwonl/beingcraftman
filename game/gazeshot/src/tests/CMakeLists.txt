add_executable(gazeshot_tests
	test_types.cpp
)

target_link_libraries(gazeshot_tests PRIVATE
	gazeshot_core
	doctest::doctest
)

target_compile_definitions(gazeshot_tests PRIVATE
	DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
)

add_test(NAME gazeshot_tests COMMAND gazeshot_tests)