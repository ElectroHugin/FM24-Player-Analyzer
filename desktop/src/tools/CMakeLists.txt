add_executable(fmmigrate migrate_cli.cpp)
target_link_libraries(fmmigrate PRIVATE fmcore)

add_executable(fmgolden golden_cli.cpp)
target_link_libraries(fmgolden PRIVATE fmcore)
