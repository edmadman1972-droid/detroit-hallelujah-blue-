; Inno Setup script for the Windows installer.
; Built by scripts\package_windows.ps1 with /DAppVersion, /DSourceDir and /DOutputDir.

#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif

[Setup]
AppName=Detroit Hallelujah Plug-ins
AppVersion={#AppVersion}
AppPublisher=Detroit Hallelujah Audio
DefaultDirName={autopf}\Detroit Hallelujah
DisableDirPage=yes
DisableProgramGroupPage=yes
OutputDir={#OutputDir}
OutputBaseFilename=DetroitHallelujah-{#AppVersion}-Windows-Installer
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Components]
Name: "blue";       Description: "Detroit Hallelujah Blue (VST3)"; Types: full compact custom; Flags: fixed
Name: "duck";       Description: "Hallelujah Duck (VST3)";         Types: full
Name: "standalone"; Description: "Detroit Hallelujah Blue standalone app"; Types: full

[Files]
Source: "{#SourceDir}\VST3\Detroit Hallelujah Blue.vst3\*"; DestDir: "{commoncf64}\VST3\Detroit Hallelujah Blue.vst3"; Components: blue; Flags: recursesubdirs ignoreversion
Source: "{#SourceDir}\VST3\Hallelujah Duck.vst3\*";         DestDir: "{commoncf64}\VST3\Hallelujah Duck.vst3";         Components: duck; Flags: recursesubdirs ignoreversion
Source: "{#SourceDir}\Standalone\*.exe";                     DestDir: "{app}";                                          Components: standalone; Flags: ignoreversion
Source: "{#SourceDir}\README.md";                            DestDir: "{app}";                                          Flags: ignoreversion

[Icons]
Name: "{autoprograms}\Detroit Hallelujah Blue"; Filename: "{app}\Detroit Hallelujah Blue.exe"; Components: standalone
