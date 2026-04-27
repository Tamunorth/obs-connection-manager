[Setup]
AppName=OBS Connection Manager
AppVersion=0.1.0
AppPublisher=Tamunorth
AppPublisherURL=https://github.com/Tamunorth/obs-connection-manager
DefaultDirName={code:GetOBSDir}
DisableDirPage=yes
DisableProgramGroupPage=yes
OutputDir=dist
OutputBaseFilename=obs-connection-manager-0.1.0-setup
Compression=lzma2
SolidCompression=yes
UninstallDisplayName=OBS Connection Manager Plugin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
SetupIconFile=compiler:SetupClassicIcon.ico
WizardStyle=modern

[Messages]
WelcomeLabel1=OBS Connection Manager Plugin
WelcomeLabel2=Adaptive bitrate manager for OBS Studio.%n%nMonitors your upload network during a live stream and adjusts the encoder bitrate to keep the stream alive when bandwidth degrades. Alerts you when the stream is breaking.

[Files]
; Plugin DLL
Source: "build\Release\obs-connection-manager.dll"; DestDir: "{app}\obs-plugins\64bit"; Flags: ignoreversion

; Locale
Source: "data\locale\en-US.ini"; DestDir: "{app}\data\obs-plugins\obs-connection-manager\locale"; Flags: ignoreversion

[Code]
function GetOBSDir(Param: String): String;
var
  Path: String;
begin
  if RegQueryStringValue(HKLM, 'SOFTWARE\OBS Studio', '', Path) then
  begin
    Result := Path;
    Exit;
  end;

  if DirExists(ExpandConstant('{pf}\obs-studio')) then
  begin
    Result := ExpandConstant('{pf}\obs-studio');
    Exit;
  end;

  Result := ExpandConstant('{pf}\obs-studio');
end;

function InitializeSetup(): Boolean;
var
  OBSDir: String;
begin
  OBSDir := GetOBSDir('');
  if not FileExists(OBSDir + '\bin\64bit\obs64.exe') then
  begin
    MsgBox('OBS Studio was not found on this computer.' + #13#10 + #13#10 +
           'Please install OBS Studio first, then run this installer again.', mbError, MB_OK);
    Result := False;
    Exit;
  end;
  Result := True;
end;

[UninstallDelete]
Type: filesandordirs; Name: "{app}\data\obs-plugins\obs-connection-manager"
