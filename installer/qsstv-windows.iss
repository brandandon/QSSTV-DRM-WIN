; One-click installer for the Windows build of QSSTV (HamDRM digital SSTV).
; Built automatically by .github/workflows/windows-build.yml.
; Installs for the current user only, so no administrator password is needed.

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#define AppName "QSSTV for Windows"
#define AppExe  "qsstv.exe"

[Setup]
AppId={{6F3C9A52-1D7B-4E0B-9B57-2C2A6D0E8F41}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=SSTV net (based on QSSTV by ON4QZ)
AppPublisherURL=https://github.com/ON4QZ/QSSTV
DefaultDirName={localappdata}\Programs\QSSTV
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
OutputBaseFilename=QSSTV-Windows-Setup-{#AppVersion}
SetupIconFile=..\src\icons\qsstv.ico
UninstallDisplayIcon={app}\{#AppExe}
LicenseFile=..\LICENSE
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
CloseApplications=yes

[Tasks]
Name: "desktopicon"; Description: "Put an icon on the desktop"; GroupDescription: "Shortcuts:"

[Files]
Source: "..\dist\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#AppName}";              Filename: "{app}\{#AppExe}"
Name: "{group}\Setup tips";               Filename: "{app}\README-WINDOWS.txt"
Name: "{group}\Uninstall {#AppName}";    Filename: "{uninstallexe}"
Name: "{userdesktop}\{#AppName}";        Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Run]
Filename: "{app}\README-WINDOWS.txt"; Description: "Show the setup tips"; Flags: postinstall shellexec skipifsilent unchecked
Filename: "{app}\{#AppExe}"; Description: "Start {#AppName} now"; Flags: postinstall nowait skipifsilent
