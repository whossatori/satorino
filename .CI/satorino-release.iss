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

[Icons]
Name: "{autoprograms}\Satorino"; Filename: "{app}\satorino.exe"; AppUserModelID: "cc.satos.Satorino"
Name: "{autodesktop}\Satorino"; Filename: "{app}\satorino.exe"; AppUserModelID: "cc.satos.Satorino"; Tasks: desktopicon

[Run]
Filename: "{app}\satorino.exe"; Description: "Launch Satorino"; Flags: nowait postinstall skipifsilent
