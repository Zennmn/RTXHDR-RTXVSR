#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\artifacts\release-v1.0.0\RTX Video Converter"
#endif
#ifndef OutputDir
  #define OutputDir "..\dist\v1.0.0"
#endif

#define AppName "RTX Video Converter"
#define AppPublisher "Zennmn"
#define AppURL "https://github.com/Zennmn/RTXHDR-RTXVSR"
#define AppExeName "RTXVideoConverter.WinUI.exe"
#define AppIconName "app-icon-rounded-v100.ico"

[Setup]
AppId={{5A8CB3EE-36AF-489C-987D-68AC18A81843}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}/issues
AppUpdatesURL={#AppURL}/releases
VersionInfoVersion={#AppVersion}.0
VersionInfoProductVersion={#AppVersion}.0
DefaultDirName={localappdata}\Programs\{#AppName}
DefaultGroupName={#AppName}
UninstallDisplayIcon={app}\Assets\{#AppIconName}
LicenseFile={#SourceDir}\DISTRIBUTION_TERMS.txt
SetupIconFile=..\frontend\rtx-video-converter-winui\Assets\app-icon.ico
OutputDir={#OutputDir}
OutputBaseFilename=RTX.Video.Converter_{#AppVersion}_x64-setup
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.17763
DisableProgramGroupPage=yes
CloseApplications=yes
RestartApplications=no
UsePreviousAppDir=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "创建桌面快捷方式"; GroupDescription: "附加任务："; Flags: unchecked

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppExeName}"; WorkingDir: "{app}"; IconFilename: "{app}\Assets\{#AppIconName}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; WorkingDir: "{app}"; IconFilename: "{app}\Assets\{#AppIconName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExeName}"; Description: "启动 {#AppName}"; Flags: nowait postinstall skipifsilent
