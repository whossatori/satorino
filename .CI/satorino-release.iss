; SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
; SPDX-License-Identifier: MIT

[Setup]
AppId={{CC804039-1CF2-4868-BACF-CE61C9EAD4BB}
AppName=Satorino
AppVersion={#AppVersion}
AppPublisher=Satorino
AppPublisherURL=https://github.com/whossatori/satorino
AppSupportURL=https://github.com/whossatori/satorino/issues
DefaultDirName={localappdata}\Programs\Satorino
DefaultGroupName=Satorino
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputBaseFilename=satorino-windows-x64-setup
SetupIconFile=..\resources\icon.ico
UninstallDisplayIcon={app}\satorino.exe
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; Flags: unchecked

[Files]
Source: "{#AppSourceDir}\*"; Excludes: "\modes"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\LICENSE"; DestDir: "{app}"
Source: "satorino-installed"; DestDir: "{app}"

[Icons]
Name: "{autoprograms}\Satorino"; Filename: "{app}\satorino.exe"; AppUserModelID: "cc.satos.Satorino"
Name: "{autodesktop}\Satorino"; Filename: "{app}\satorino.exe"; AppUserModelID: "cc.satos.Satorino"; Tasks: desktopicon

[Run]
Filename: "{app}\satorino.exe"; Description: "Launch Satorino"; Flags: nowait postinstall skipifsilent

[Code]
var
  UpdateDir: String;
  RelaunchAfterUpdate: Boolean;

function OpenProcess(DesiredAccess: DWORD; InheritHandle: BOOL; ProcessId: DWORD): THandle;
  external 'OpenProcess@kernel32.dll stdcall';
function WaitForSingleObject(Handle: THandle; Milliseconds: DWORD): DWORD;
  external 'WaitForSingleObject@kernel32.dll stdcall';
function CloseHandle(Handle: THandle): BOOL;
  external 'CloseHandle@kernel32.dll stdcall';

function InitializeSetup(): Boolean;
var
  Pid: Integer;
  PreviousProcess: THandle;
  WaitResult: DWORD;
begin
  Result := True;
  Pid := StrToIntDef(ExpandConstant('{param:SATORINOPID|0}'), 0);
  if Pid <= 0 then
    Exit;

  UpdateDir := ExpandConstant('{param:DIR|}');
  if (UpdateDir = '') or (not FileExists(AddBackslash(UpdateDir) + 'satorino-installed')) then
  begin
    Result := False;
    Exit;
  end;

  { Wait for the old Satorino process to release its executable and DLLs. }
  PreviousProcess := OpenProcess($00100000, False, Pid);
  if PreviousProcess <> 0 then
  begin
    WaitResult := WaitForSingleObject(PreviousProcess, 15000);
    CloseHandle(PreviousProcess);
    if WaitResult <> 0 then
    begin
      Result := False;
      Exit;
    end;
  end;
  RelaunchAfterUpdate := True;
end;

procedure DeinitializeSetup();
var
  ExitCode: Integer;
  Application: String;
begin
  if RelaunchAfterUpdate then
  begin
    { On a failed update, launch the previous installation without retrying. }
    Application := AddBackslash(UpdateDir) + 'satorino.exe';
    if FileExists(Application) then
      Exec(Application, '--satorino-skip-update', UpdateDir, SW_SHOWNORMAL, ewNoWait, ExitCode);
  end;
end;
