; GoTiengViet versioned installer (Inno Setup 6).
; Built by CI: make -f Makefile.win setup VERSION=<x.y.z>
;   iscc.exe /Q "/DAppVersion=<x.y.z>" "/DAppVerNum=<x.y.z.0>" /DSourceDir=..\release-pkg windows/installer.iss
; Output: gotiengviet-<x.y.z>-x64-setup.exe (per-user, no admin needed).

#ifndef AppVersion
  #define AppVersion "0.6.1"
#endif
#ifndef AppVerNum
  #define AppVerNum "0.6.1.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\release-pkg"
#endif
#define AppId "{{2dd52301-591f-470a-a8ca-06381a0169d8}}"

[Setup]
AppId={#AppId}
AppName=GoTiengViet
AppVersion={#AppVersion}
VersionInfoVersion={#AppVerNum}
AppVerName=GoTiengViet {#AppVersion}
AppPublisher=GoTiengViet Project
AppPublisherURL=https://github.com/isthaison/gotiengviet
AppSupportURL=https://github.com/isthaison/gotiengviet
AppUpdatesURL=https://github.com/isthaison/gotiengviet/releases
DefaultDirName={localappdata}\Programs\GoTiengViet
PrivilegesRequired=lowest
OutputDir=..
OutputBaseFilename=gotiengviet-{#AppVersion}-x64-setup
Compression=lzma2/max
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; The running tray app holds this mutex (see windows/main.c); setup asks
; the user to close it instead of failing on locked exe/dlls.
AppMutex=GoTiengViet_Single_Instance_Mutex
WizardStyle=modern
DisableProgramGroupPage=yes
UninstallDisplayIcon={app}\gotiengviet.exe
LicenseFile=..\LICENSE
SetupIconFile=icons\gotiengviet.ico
ShowLanguageDialog=no

[Languages]
; English-only on purpose: Inno's bundled Vietnamese.isl is unofficial and
; the choco-installed compiler does not ship it. The app UI is Vietnamese.
Name: "english"; MessagesFile: "compiler:Default.isl"

[CustomMessages]
english.StartupDesc=Start with Windows
english.DesktopDesc=Create a desktop icon

[Tasks]
Name: "startup"; Description: "{cm:StartupDesc}"; GroupDescription: "{cm:AdditionalIcons}"
Name: "desktopicon"; Description: "{cm:DesktopDesc}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#SourceDir}\gotiengviet.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\gspawn-win64-helper*.exe"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
; gtv_tsf.dll (TSF text service) included via the glob below; restartreplace
; because host apps may still hold it while updating.
Source: "{#SourceDir}\*.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist restartreplace
Source: "{#SourceDir}\data\*"; DestDir: "{app}\data"; Flags: ignoreversion recursesubdirs
Source: "{#SourceDir}\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\LICENSE"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{userprograms}\GoTiengViet\GoTiengViet"; Filename: "{app}\gotiengviet.exe"
Name: "{userprograms}\GoTiengViet\Gõ Tiếng Việt"; Filename: "{app}\gotiengviet.exe"; Comment: "Mở GoTiengViet"
Name: "{userprograms}\GoTiengViet\Gỡ bỏ cài đặt"; Filename: "{uninstallexe}"
Name: "{userdesktop}\GoTiengViet"; Filename: "{app}\gotiengviet.exe"; Tasks: desktopicon

[Registry]
; Same Run value the tray menu manages, so both stay in sync.
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "GoTiengViet"; ValueData: """{app}\gotiengviet.exe"""; Flags: uninsdeletevalue; Tasks: startup

[Run]
Filename: "regsvr32.exe"; Parameters: "/s ""{app}\gtv_tsf.dll"""; Flags: runhidden
Filename: "{app}\gotiengviet.exe"; Description: "{cm:LaunchProgram,GoTiengViet}"; Flags: nowait postinstall skipifsilent

[UninstallRun]
Filename: "regsvr32.exe"; Parameters: "/s /u ""{app}\gtv_tsf.dll"""; Flags: runhidden
