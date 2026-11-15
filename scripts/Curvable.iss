; Inno Setup script for the Windows installer.
;
; Build (from the repository root, after a Release build):
;   iscc /DAppVersion=0.1.0 scripts\Curvable.iss
; The installer is written to build\installer\.

#ifndef AppVersion
  #error Pass the version, e.g. iscc /DAppVersion=0.1.0 scripts\Curvable.iss
#endif

#define AppName "Curvable"
#define AppPublisher "asahanada"
#define AppUrl "https://github.com/asahanadadesu/curvable"
#define Vst3Bundle "..\build\Curvable_artefacts\Release\VST3\Curvable.vst3"

[Setup]
; Never change AppId: it lets new versions replace old ones
AppId={{6F1D2B7E-4C8A-4E52-9A3D-2B8C7E5F1A90}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppUrl}
AppSupportURL={#AppUrl}/issues
AppUpdatesURL={#AppUrl}/releases
DefaultDirName={commoncf64}\VST3
DirExistsWarning=no
DisableDirPage=no
DisableProgramGroupPage=yes
UsePreviousAppDir=yes
LicenseFile=..\LICENSE
OutputDir=..\build\installer
OutputBaseFilename=Curvable-{#AppVersion}-Windows-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
MinVersion=10.0
UninstallDisplayName={#AppName} {#AppVersion}
UninstallFilesDir={autoappdata}\{#AppPublisher}\{#AppName}\Uninstall
CloseApplications=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Messages]
SelectDirLabel3=Setup will install the Curvable VST3 plugin into the following folder. Most hosts, including FL Studio, scan the default folder automatically.

[Files]
Source: "{#Vst3Bundle}\*"; DestDir: "{app}\Curvable.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs

[InstallDelete]
; Remove files left by an earlier version before copying the new bundle
Type: filesandordirs; Name: "{app}\Curvable.vst3"

[UninstallDelete]
Type: dirifempty; Name: "{app}\Curvable.vst3"
