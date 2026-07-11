# Path to the legacy Python app: source of fixture data (definitions.json)
# and golden-master reference outputs.
set(LEGACY_DIR "${CMAKE_SOURCE_DIR}/../legacy")

function(fm_add_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE fmcore Qt6::Test)
    target_compile_definitions(${name} PRIVATE LEGACY_DIR="${LEGACY_DIR}")
    add_test(NAME ${name} COMMAND ${name})
    # Qt DLLs are not on PATH on Windows; point the test at the Qt bin directory.
    set_tests_properties(${name} PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    )
endfunction()

fm_add_test(test_smoke)
fm_add_test(test_utils)
fm_add_test(test_definitions)
fm_add_test(test_appconfig)
fm_add_test(test_playerstore)
fm_add_test(test_migrator)
fm_add_test(test_dwrs)
fm_add_test(test_htmlimporter)
