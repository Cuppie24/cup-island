; Inno Setup script for Cup Island File Search, the file index service
; (decisions 075, 076). Built by .\build.ps1 -Installer before the app's
; installer, which carries it: dist\CupIsland-Index-Setup-<version>.exe.
;
; Admin: a service is installed for the whole PC, into Program Files, where
; no user can replace the exe the service runs as SYSTEM. Run three ways:
;   - by the app's installer at the end of a fresh setup (one UAC prompt);
;   - by the app at its first start when search is missing (one UAC prompt);
;   - by the service itself to update, silently: it's SYSTEM already.

#define AppName "Cup Island File Search"
#define ServiceExe "CupIslandIndex.exe"
; The same version as the app, from build.ps1 (decision 076: one version, one release).
#ifndef AppVersion
  #define AppVersion "0.0.0-dev"
#endif

[Setup]
; Never change AppId: it's how upgrades and the uninstaller find an existing install.
AppId={{EFB80F5E-CFE8-4649-8F1F-26996CFE6C14}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
DefaultDirName={autopf}\{#AppName}
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableReadyPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; Windows 10 1809 or later (decision 015).
MinVersion=10.0.17763
OutputDir=..\dist
OutputBaseFilename=CupIsland-Index-Setup-{#AppVersion}
SetupIconFile=..\assets\cup_island.ico
UninstallDisplayIcon={app}\{#ServiceExe}
Compression=lzma2
SolidCompression=yes
; The service is stopped by PrepareToInstall below, not by Restart Manager.
CloseApplications=no

[Files]
Source: "..\build\windows\{#ServiceExe}"; DestDir: "{app}"; Flags: ignoreversion
; The terminal client, for trying and reporting problems (cupindex status).
Source: "..\build\windows\cupindex.exe"; DestDir: "{app}"; Flags: ignoreversion

[Run]
; Creates the service or updates its settings, then starts it (service_control.cpp).
Filename: "{app}\{#ServiceExe}"; Parameters: "--install"; Flags: runhidden waituntilterminated; StatusMsg: "Starting file search..."

[UninstallRun]
Filename: "{app}\{#ServiceExe}"; Parameters: "--uninstall"; Flags: runhidden waituntilterminated; RunOnceId: "RemoveService"

[UninstallDelete]
; The service's self-updates (self_update.h, decision 087).
Type: filesandordirs; Name: "{app}\updates"
Type: files; Name: "{app}\index-update.log"
; The service's own folder: its saved index and log (service_log.h).
Type: filesandordirs; Name: "{sys}\config\systemprofile\AppData\Local\CupIsland\index-service"
Type: dirifempty; Name: "{sys}\config\systemprofile\AppData\Local\CupIsland"

[Code]
// An update: stop the running service and wait for its process to exit,
// so its exe can be replaced. It saves its index as it stops.
function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  Exe: String;
  ResultCode: Integer;
begin
  Result := '';
  Exe := ExpandConstant('{app}\{#ServiceExe}');
  if FileExists(Exe) then
    Exec(Exe, '--stop', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
end;
