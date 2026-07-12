add_executable(fmplayeranalyzer WIN32
    app.rc
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
    ImportRunner.h
    ImportRunner.cpp
    PlayerActions.h
    PlayerActions.cpp
    RecalcHelper.h
    RecalcHelper.cpp
    pages/AssignRolesPage.h
    pages/AssignRolesPage.cpp
    pages/BestXiPage.h
    pages/BestXiPage.cpp
    pages/DashboardPage.h
    pages/DashboardPage.cpp
    pages/DwrsProgressPage.h
    pages/DwrsProgressPage.cpp
    pages/EditPlayerPage.h
    pages/EditPlayerPage.cpp
    pages/GapAnalysisPage.h
    pages/GapAnalysisPage.cpp
    pages/NationalBestXiPage.h
    pages/NationalBestXiPage.cpp
    pages/NationalDashboardPage.h
    pages/NationalDashboardPage.cpp
    pages/NationalSquadMatrixPage.h
    pages/NationalSquadMatrixPage.cpp
    pages/NationalSquadSelectionPage.h
    pages/NationalSquadSelectionPage.cpp
    pages/NewRolePage.h
    pages/NewRolePage.cpp
    pages/NewTacticPage.h
    pages/NewTacticPage.cpp
    pages/PageBase.h
    pages/PageHelpers.h
    pages/PlaceholderPage.h
    pages/PlayerComparisonPage.h
    pages/PlayerComparisonPage.cpp
    pages/PlayerProfilePage.h
    pages/PlayerProfilePage.cpp
    pages/RoleAnalysisPage.h
    pages/RoleAnalysisPage.cpp
    pages/SettingsPage.h
    pages/SettingsPage.cpp
    pages/SquadMatrixPage.h
    pages/SquadMatrixPage.cpp
    pages/TacticExplorerPage.h
    pages/TacticExplorerPage.cpp
    pages/TransfersPage.h
    pages/TransfersPage.cpp
    theming/ThemeManager.h
    theming/ThemeManager.cpp
    theming/ThemePresets.h
    theming/ThemePresets.cpp
    widgets/CellStyleDelegate.h
    widgets/CellStyleDelegate.cpp
    widgets/Charts.h
    widgets/Charts.cpp
    widgets/PersonalityFilterWidget.h
    widgets/PersonalityFilterWidget.cpp
    widgets/PlayerTableModel.h
    widgets/PlayerTableModel.cpp
    widgets/StrengthGridWidget.h
    widgets/StrengthGridWidget.cpp
    widgets/TacticPitchWidget.h
    widgets/TacticPitchWidget.cpp
    resources/app.qrc
)

target_include_directories(fmplayeranalyzer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(fmplayeranalyzer PRIVATE
    fmcore
    Qt6::Widgets
    Qt6::Charts
)

# --- Translations -----------------------------------------------------------
# The source strings are German; English is shipped as a compiled .qm loaded by
# default (German = source fallback, no translator). lrelease compiles the
# hand-maintained .ts at build time and the result is embedded under
# :/i18n/fmplayeranalyzer_en.qm. Regenerate the .ts after adding tr() strings:
#   lupdate src -ts translations/fmplayeranalyzer_en.ts -no-obsolete
set(FM_TS_FILE ${CMAKE_SOURCE_DIR}/translations/fmplayeranalyzer_en.ts)
set(FM_QM_FILE ${CMAKE_CURRENT_BINARY_DIR}/fmplayeranalyzer_en.qm)
add_custom_command(
    OUTPUT ${FM_QM_FILE}
    COMMAND Qt6::lrelease ${FM_TS_FILE} -qm ${FM_QM_FILE}
    DEPENDS ${FM_TS_FILE}
    VERBATIM
    COMMENT "Compiling translations (lrelease)"
)
set_source_files_properties(${FM_QM_FILE} PROPERTIES QT_RESOURCE_ALIAS fmplayeranalyzer_en.qm)
qt_add_resources(fmplayeranalyzer "translations"
    PREFIX "/i18n"
    FILES ${FM_QM_FILE}
)
