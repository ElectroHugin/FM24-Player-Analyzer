add_library(fmcore STATIC
    Version.h
    Version.cpp
)

target_include_directories(fmcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

# Deliberately no Qt6::Widgets: fmcore must stay headless-testable.
target_link_libraries(fmcore PUBLIC
    Qt6::Core
    Qt6::Sql
    Qt6::Concurrent
)
