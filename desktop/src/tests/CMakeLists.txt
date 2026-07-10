add_executable(fmcore_tests
    test_smoke.cpp
)

target_link_libraries(fmcore_tests PRIVATE
    fmcore
    Qt6::Test
)

add_test(NAME fmcore_tests COMMAND fmcore_tests)

# Qt DLLs are not on PATH on Windows; point the test at the Qt bin directory.
set_tests_properties(fmcore_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
)
