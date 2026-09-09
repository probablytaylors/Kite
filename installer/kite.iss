#ifndef AppVersion
  #define AppVersion "0.3.2"
#endif
#define AppName "Kite"
#define AppPublisher "Kite"
#define AppExe "kite.exe"
#define AppUrl "https://github.com/probablytaylors/Kite"

[Setup]
AppId={{C81C624E-7D60-4AE6-BE58-AC7A1711567A}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
AppSupportURL={#AppUrl}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
LicenseFile=..\LICENSE
OutputDir=output
OutputBaseFilename=kite-setup
SetupIconFile=kite.ico
UninstallDisplayIcon={app}\{#AppExe}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible
ChangesEnvironment=yes

[Tasks]
Name: "addtopath"; Description: "Add Kite to the system PATH"
Name: "associate"; Description: "Associate .kite files with Kite"

[Files]
Source: "..\build\Release\{#AppExe}"; DestDir: "{app}"; Flags: ignoreversion
Source: "kite.ico"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\std\*"; DestDir: "{app}\std"; Flags: ignoreversion recursesubdirs
Source: "..\docs\*"; DestDir: "{app}\docs"; Flags: ignoreversion recursesubdirs
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\CHANGELOG.md"; DestDir: "{app}"; Flags: ignoreversion

[Registry]
Root: HKLM; Subkey: "SYSTEM\CurrentControlSet\Control\Session Manager\Environment"; \
    ValueType: expandsz; ValueName: "Path"; ValueData: "{olddata};{app}"; \
    Tasks: addtopath; Check: NeedsAddPath('{app}')

Root: HKLM; Subkey: "Software\Classes\.kite"; ValueType: string; ValueName: ""; \
    ValueData: "Kite.Script"; Flags: uninsdeletevalue; Tasks: associate
Root: HKLM; Subkey: "Software\Classes\Kite.Script"; ValueType: string; ValueName: ""; \
    ValueData: "Kite source file"; Flags: uninsdeletekey; Tasks: associate
Root: HKLM; Subkey: "Software\Classes\Kite.Script\DefaultIcon"; ValueType: string; ValueName: ""; \
    ValueData: "{app}\kite.ico"; Tasks: associate
Root: HKLM; Subkey: "Software\Classes\Kite.Script\shell\open\command"; ValueType: string; ValueName: ""; \
    ValueData: """{app}\{#AppExe}"" run ""%1"""; Tasks: associate

[Icons]
Name: "{group}\Kite README"; Filename: "{app}\README.md"
Name: "{group}\{cm:UninstallProgram,{#AppName}}"; Filename: "{uninstallexe}"

[Code]
function NeedsAddPath(Param: string): Boolean;
var
  OrigPath: string;
begin
  if not RegQueryStringValue(HKLM,
    'SYSTEM\CurrentControlSet\Control\Session Manager\Environment', 'Path', OrigPath) then
  begin
    Result := True;
    exit;
  end;
  Result := Pos(';' + Uppercase(ExpandConstant(Param)) + ';',
    ';' + Uppercase(OrigPath) + ';') = 0;
end;
