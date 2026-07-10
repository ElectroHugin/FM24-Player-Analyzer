add_library(fmcore STATIC
    AppConfig.h
    AppConfig.cpp
    Attributes.h
    Constants.h
    Constants.cpp
    Database.h
    Database.cpp
    Definitions.h
    Definitions.cpp
    DwrsEngine.h
    DwrsEngine.cpp
    GistRainbowLut.h
    LegacyMigrator.h
    LegacyMigrator.cpp
    Player.h
    PlayerStore.h
    PlayerStore.cpp
    TalentEngine.h
    TalentEngine.cpp
    Utils.h
    Utils.cpp
    Version.h
    Version.cpp
)

target_include_directories(fmcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

# Deliberately no Qt6::Widgets: fmcore must stay headless-testable.
# Qt6::Gui is needed for QColor only.
target_link_libraries(fmcore PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Sql
    Qt6::Concurrent
)
