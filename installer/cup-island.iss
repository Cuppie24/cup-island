; Inno Setup script for the Windows installer (decision 011).
; Build it with .\build.ps1 -Installer, which builds Release first and writes
; dist\CupIsland-Setup-<version>.exe. Needs Inno Setup 6.3+ (jrsoftware.org).
;
; No Visual C++ Redistributable step: the exe links the C++ runtime statically
; (CMakeLists.txt), so there is nothing extra to install.
;
; It carries the file search service's installer (cup-island-index.iss,
; decision 076), built first, and runs it at the end of an interactive
; setup. A silent install (the in-app updater) doesn't: the updated app sets
; search up itself at its next start if it's missing.

#define AppName "Cup Island"
#define AppExe "CupIsland.exe"
; The version comes from build.ps1 (/DAppVersion=..., decision 036): the
; Release workflow's, or git's for a local build. Never set it here.
#ifndef AppVersion
  #define AppVersion "0.0.0-dev"
#endif

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
; Windows 10 1809 or later (decision 015).
MinVersion=10.0.17763
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
; File search's own installer, under a fixed name the app looks for (providers/search/search_service.cpp).
Source: "..\dist\CupIsland-Index-Setup-{#AppVersion}.exe"; DestDir: "{app}"; DestName: "CupIsland-Index-Setup.exe"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppExe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExe}"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent
; A silent install is the in-app updater's (decision 036): the island quit
; to be replaced, so start it again.
Filename: "{app}\{#AppExe}"; Flags: nowait skipifnotsilent

[UninstallRun]
; The island has no window to close politely; stop it so its files can be removed.
Filename: "{sys}\taskkill.exe"; Parameters: "/im {#AppExe} /f"; Flags: runhidden; RunOnceId: "StopCupIsland"
; Undo "Start with Windows" (Settings, src/platform/autostart.cpp), on by default since decision 030.
Filename: "{sys}\reg.exe"; Parameters: "delete ""HKCU\Software\Microsoft\Windows\CurrentVersion\Run"" /v ""Cup Island"" /f"; Flags: runhidden; RunOnceId: "RemoveStartup"

[Code]
// A fresh, interactive setup: set up file search now, while the user is
// here (Windows asks for permission). Refused or failed, the app carries on
// without it and asks once more after its next update. Not in a silent
// install: the updater's, with nobody to ask, and the app does it instead.
// Nor when it's installed already: re-running setup by hand needs no second
// prompt, and the app asks the service to update itself (decision 076).
// Asked here, the app doesn't ask again for this version: setup writes the
// key ServiceHost keeps (kAskedKey, decision 078) in the providers' store, laid out
// as decision 028 says: <LocalAppData>\CupIsland\store\<provider>\<key>.txt.
procedure CurStepChanged(CurStep: TSetupStep);
var
  ErrorCode: Integer;
  Store: String;
begin
  if (CurStep = ssPostInstall) and not WizardSilent() and
     not RegKeyExists(HKLM, 'SYSTEM\CurrentControlSet\Services\CupIslandIndex') then
  begin
    ShellExec('runas', ExpandConstant('{app}\CupIsland-Index-Setup.exe'), '/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /SP-',
              '', SW_SHOW, ewWaitUntilTerminated, ErrorCode);
    Store := ExpandConstant('{localappdata}\CupIsland\store\system.service');
    ForceDirectories(Store);
    SaveStringToFile(Store + '\asked-for.txt', '{#AppVersion}', False);
  end;
end;
