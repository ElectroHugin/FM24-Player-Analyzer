; Inno Setup script for the FM Player Analyzer desktop app.
; Built by scripts\package.ps1, which stages the app (exe + Qt DLLs via
; windeployqt) into installer\staging\ before invoking ISCC on this file.

#define MyAppName "FM Player Analyzer"
#ifndef MyAppVersion
  #define MyAppVersion "0.1.0"
#endif
#define MyAppExeName "fmplayeranalyzer.exe"

[Setup]
; AppId is a STABLE identity — never change it, or updates won't replace an
; existing installation.
AppId={{8F2B6E7A-40A1-4C0D-9E3B-5D71C2A94F08}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher=Edmund Jochim
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=output
OutputBaseFilename=FMPlayerAnalyzer-Setup-{#MyAppVersion}
SetupIconFile=..\src\app\resources\favicon.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; User data (Datenbanken, config.ini, definitions.json) liegt in
; %LOCALAPPDATA%\FM24PlayerAnalyzer und wird vom Installer nie angefasst.

[Languages]
Name: "german"; MessagesFile: "compiler:Languages\German.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "staging\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#MyAppName}}"; Flags: nowait postinstall skipifsilent
