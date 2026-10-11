; Inno Setup script for the Windows installer (decision 011).
; Build it with .\build.ps1 -Installer, which builds Release first and writes
; dist\CupIsland-Setup-<version>.exe. Needs Inno Setup 6.3+ (jrsoftware.org).
;
; No Visual C++ Redistributable step: the exe links the C++ runtime statically
; (CMakeLists.txt), so there is nothing extra to install.

#define AppName "Cup Island"
#define AppVersion "0.1.2"
#define AppExe "CupIsland.exe"

[Setup]
; Never change AppId: it's how upgrades and the uninstaller find an existing install.
AppId={{6CB3203D-53EC-4816-944E-3BC5A4DD3489}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
DefaultDirName={autopf}\{#AppName}
DisableProgramGroupPage=yes
; Per-user install by default, so no admin prompt; the user can pick all-users instead.
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; TR §3: Windows 10 2004 or later.
MinVersion=10.0.19041
OutputDir=..\dist
OutputBaseFilename=CupIsland-Setup-{#AppVersion}
SetupIconFile=..\assets\cup_island.ico
UninstallDisplayIcon={app}\{#AppExe}
WizardStyle=modern
Compression=lzma2
SolidCompression=yes
; Upgrading while the island runs: close it instead of asking to reboot.
CloseApplications=force

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; Flags: unchecked

[Files]
Source: "..\build\windows\{#AppExe}"; DestDir: "{app}"; Flags: ignoreversion
; Read from beside the exe (src/app/live_tokens.h); still live-tunable after install.
Source: "..\assets\tokens.json"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppExe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExe}"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent

[UninstallRun]
; The island has no window to close politely; stop it so its files can be removed.
Filename: "{sys}\taskkill.exe"; Parameters: "/im {#AppExe} /f"; Flags: runhidden; RunOnceId: "StopCupIsland"
; Undo the tray menu's "Start with Windows" (src/platform/autostart.cpp), if it was ticked.
Filename: "{sys}\reg.exe"; Parameters: "delete ""HKCU\Software\Microsoft\Windows\CurrentVersion\Run"" /v ""Cup Island"" /f"; Flags: runhidden; RunOnceId: "RemoveStartup"
