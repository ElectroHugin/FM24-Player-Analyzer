add_executable(fmplayeranalyzer WIN32
    main.cpp
)

target_link_libraries(fmplayeranalyzer PRIVATE
    fmcore
    Qt6::Widgets
    Qt6::Charts
)
