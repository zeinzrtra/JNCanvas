#define MyAppName "JNCanvas"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "Zein"
#define MyAppURL "https://github.com/zeinzrtra/JNCanvas"
#define MyAppExeName "obs64.exe"

#ifndef Configuration
#define Configuration "RelWithDebInfo"
#endif

[Setup]
AppId={{E6D012C4-8F7B-4A12-B2D3-3D4F5A6B7C8D}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={reg:HKLM\SOFTWARE\OBS Studio,InstallPath|{autopf}\obs-studio}
DirExistsWarning=no
EnableDirDoesntExistWarning=yes
DisableDirPage=no
OutputBaseFilename=JNCanvas-Setup-v1.0.0-Windows-x64
OutputDir=..\release
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64
ArchitecturesAllowed=x64
UninstallDisplayIcon={app}\bin\64bit\{#MyAppExeName}
PrivilegesRequired=admin
CloseApplications=yes
CloseApplicationsFilter=obs64.exe

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
; Copy plugin dll directly from the build configuration directory
Source: "..\release\{#Configuration}\jn-canvas\bin\64bit\jn-canvas.dll"; DestDir: "{app}\obs-plugins\64bit"; Flags: ignoreversion
Source: "..\data\*"; DestDir: "{app}\data\obs-plugins\jn-canvas"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "Uninstall-JNCanvas.bat"; DestDir: "{app}\obs-plugins\64bit"; Flags: ignoreversion

[UninstallDelete]
Type: files; Name: "{app}\obs-plugins\64bit\jn-canvas.*"
Type: files; Name: "{app}\obs-plugins\64bit\Uninstall-JNCanvas.bat"
Type: filesandordirs; Name: "{app}\data\obs-plugins\jn-canvas"
Type: filesandordirs; Name: "{userappdata}\obs-studio\plugin_config\jn-canvas"

[Code]
function InitializeSetup(): Boolean;
var
  OBSPath: String;
begin
  Result := True;
  if RegQueryStringValue(HKEY_LOCAL_MACHINE, 'SOFTWARE\OBS Studio', 'InstallPath', OBSPath) then
  begin
    Log('Detected OBS installation at: ' + OBSPath);
  end;
end;

function InitializeUninstall(): Boolean;
var
  ErrorCode: Integer;
begin
  Result := True;
  // Automatically terminate OBS Studio if open so DLL is unlocked
  ShellExec('open', 'taskkill.exe', '/F /IM obs64.exe', '', SW_HIDE, ewWaitUntilTerminated, ErrorCode);
  Sleep(1000);
end;
