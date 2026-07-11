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
    RecalcHelper.h
    RecalcHelper.cpp
    pages/AssignRolesPage.h
    pages/AssignRolesPage.cpp
    pages/DashboardPage.h
    pages/DashboardPage.cpp
    pages/PageBase.h
    pages/PlaceholderPage.h
    pages/RoleAnalysisPage.h
    pages/RoleAnalysisPage.cpp
    pages/SettingsPage.h
    pages/SettingsPage.cpp
    pages/SquadMatrixPage.h
    pages/SquadMatrixPage.cpp
    pages/TransfersPage.h
    pages/TransfersPage.cpp
    theming/ThemeManager.h
    theming/ThemeManager.cpp
    widgets/PersonalityFilterWidget.h
    widgets/PersonalityFilterWidget.cpp
    widgets/PlayerTableModel.h
    widgets/PlayerTableModel.cpp
    widgets/StrengthGridWidget.h
    widgets/StrengthGridWidget.cpp
    resources/app.qrc
)

target_include_directories(fmplayeranalyzer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(fmplayeranalyzer PRIVATE
    fmcore
    Qt6::Widgets
    Qt6::Charts
)
