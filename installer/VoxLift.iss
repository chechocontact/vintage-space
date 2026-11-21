; Inno Setup 6 script. The version comes from CMakeLists.txt via /DAppVersion=x.y.z (see build.yml).

#ifndef AppVersion
  #define AppVersion "0.0.0-dev"
#endif

[Setup]
AppId={{5B0E2C1A-8F3D-4C6E-9A71-3D2F6B8E4A90}
AppName=VoxLift
AppVersion={#AppVersion}
AppPublisher=Enless Smile Si Si
DefaultDirName={commoncf64}\VST3\VoxLift.vst3
DisableDirPage=yes
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.17763
OutputDir=..\dist
OutputBaseFilename=VoxLift-{#AppVersion}-Setup
Compression=lzma2
SolidCompression=yes
LicenseFile=..\LICENSE
UninstallDisplayName=VoxLift (VST3)

[Files]
Source: "..\build\VoxLift_artefacts\Release\VST3\VoxLift.vst3\*"; DestDir: "{commoncf64}\VST3\VoxLift.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "..\LICENSE"; DestDir: "{commoncf64}\VST3\VoxLift.vst3"; Flags: ignoreversion
Source: "..\README.md"; DestDir: "{commoncf64}\VST3\VoxLift.vst3"; Flags: ignoreversion

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\VoxLift.vst3"
