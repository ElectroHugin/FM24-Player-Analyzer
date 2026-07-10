add_executable(fmplayeranalyzer WIN32
    main.cpp
    AppContext.h
    AppContext.cpp
    AppPaths.h
    AppPaths.cpp
    FirstRunDialog.h
    FirstRunDialog.cpp
    MainWindow.h
    MainWindow.cpp
    MigrationWizard.h
    MigrationWizard.cpp
    pages/PageBase.h
    pages/PlaceholderPage.h
    pages/SettingsPage.h
    pages/SettingsPage.cpp
    theming/ThemeManager.h
    theming/ThemeManager.cpp
    resources/app.qrc
)

target_include_directories(fmplayeranalyzer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(fmplayeranalyzer PRIVATE
    fmcore
    Qt6::Widgets
    Qt6::Charts
)
