#define MyAppName "143 Spotlight"
#ifndef ProductName
#define ProductName "143-spotlight"
#endif
#ifndef ProductVersion
#define ProductVersion "0.1.0"
#endif
#ifndef SourceRoot
#define SourceRoot "..\release\RelWithDebInfo\143-spotlight"
#endif
#ifndef OutputDir
#define OutputDir "..\release"
#endif

[Setup]
AppId={{50C0FC21-D07B-48D5-A910-143143143143}
AppName={#MyAppName}
AppVersion={#ProductVersion}
AppPublisher=143kmh
AppPublisherURL=https://github.com/143kmh/143-spotlight
AppSupportURL=https://github.com/143kmh/143-spotlight/issues
DefaultDirName={commonappdata}\143 Spotlight
DisableDirPage=yes
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#OutputDir}
OutputBaseFilename={#ProductName}-{#ProductVersion}-windows-x64-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
CloseApplicationsFilter=obs64.exe
RestartApplications=no
UninstallDisplayName={#MyAppName}
SetupLogging=yes

[Files]
Source: "{#SourceRoot}\bin\64bit\143-spotlight.dll"; DestDir: "{code:GetPluginDir}"; Flags: ignoreversion restartreplace
Source: "{#SourceRoot}\data\locale\en-US.ini"; DestDir: "{code:GetLocaleDir}"; Flags: ignoreversion

[Run]
Filename: "{code:GetObsExe}"; WorkingDir: "{code:GetObsBinDir}"; Description: "Launch OBS Studio with 143 Spotlight"; Flags: postinstall nowait skipifsilent

[Code]
var
  ObsDirPage: TInputDirWizardPage;
  DetectedObsDir: String;

function IsObsDir(const Path: String): Boolean;
begin
  Result := FileExists(AddBackslash(Path) + 'bin\64bit\obs64.exe');
end;

function FindObsFromRegistryRoot(RootKey: Integer): String;
var
  BaseKey: String;
  Keys: TArrayOfString;
  DisplayName: String;
  InstallLocation: String;
  I: Integer;
begin
  Result := '';
  BaseKey := 'SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall';

  if not RegGetSubkeyNames(RootKey, BaseKey, Keys) then
    Exit;

  for I := 0 to GetArrayLength(Keys) - 1 do
  begin
    if RegQueryStringValue(RootKey, BaseKey + '\' + Keys[I], 'DisplayName', DisplayName) and
       (Pos('OBS Studio', DisplayName) > 0) and
       RegQueryStringValue(RootKey, BaseKey + '\' + Keys[I], 'InstallLocation', InstallLocation) and
       IsObsDir(InstallLocation) then
    begin
      Result := RemoveBackslashUnlessRoot(InstallLocation);
      Exit;
    end;
  end;
end;

function FindObsOnDrives: String;
const
  CandidateCount = 7;
var
  DriveCode: Integer;
  I: Integer;
  Root: String;
  Candidate: String;
  Candidates: array[0..CandidateCount - 1] of String;
begin
  Result := '';
  Candidates[0] := 'Program Files\obs-studio';
  Candidates[1] := 'Program Files (x86)\obs-studio';
  Candidates[2] := 'Soft\obs-studio';
  Candidates[3] := 'Apps\obs-studio';
  Candidates[4] := 'Programs\obs-studio';
  Candidates[5] := 'Tools\obs-studio';
  Candidates[6] := 'obs-studio';

  for DriveCode := Ord('C') to Ord('Z') do
  begin
    Root := Chr(DriveCode) + ':\';
    if not DirExists(Root) then
      Continue;

    for I := 0 to CandidateCount - 1 do
    begin
      Candidate := Root + Candidates[I];
      if IsObsDir(Candidate) then
      begin
        Result := Candidate;
        Exit;
      end;
    end;
  end;
end;

function FindObsDir: String;
begin
  Result := FindObsFromRegistryRoot(HKLM64);
  if Result <> '' then
    Exit;

  Result := FindObsFromRegistryRoot(HKCU);
  if Result <> '' then
    Exit;

  Result := FindObsOnDrives;
end;

procedure InitializeWizard;
begin
  DetectedObsDir := FindObsDir;
  ObsDirPage := CreateInputDirPage(wpWelcome,
    'OBS Studio location',
    'Select your OBS Studio folder',
    '143 Spotlight normally detects OBS automatically. If it did not, select the folder that contains bin and obs-plugins.');
  ObsDirPage.Add('OBS Studio folder:');

  if DetectedObsDir <> '' then
    ObsDirPage.Values[0] := DetectedObsDir
  else
    ObsDirPage.Values[0] := ExpandConstant('{autopf}\obs-studio');
end;

function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := (PageID = ObsDirPage.ID) and IsObsDir(ObsDirPage.Values[0]);
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;

  if (CurPageID = ObsDirPage.ID) and not IsObsDir(ObsDirPage.Values[0]) then
  begin
    MsgBox('OBS Studio was not found in that folder. Select the folder containing bin\64bit\obs64.exe.', mbError, MB_OK);
    Result := False;
  end;
end;

function GetObsDir(Param: String): String;
begin
  Result := RemoveBackslashUnlessRoot(ObsDirPage.Values[0]);
end;

function GetPluginDir(Param: String): String;
begin
  Result := AddBackslash(GetObsDir('')) + 'obs-plugins\64bit';
end;

function GetLocaleDir(Param: String): String;
begin
  Result := AddBackslash(GetObsDir('')) + 'data\obs-plugins\143-spotlight\locale';
end;

function GetObsBinDir(Param: String): String;
begin
  Result := AddBackslash(GetObsDir('')) + 'bin\64bit';
end;

function GetObsExe(Param: String): String;
begin
  Result := AddBackslash(GetObsBinDir('')) + 'obs64.exe';
end;

procedure EnableReplayBufferForCurrentProfile;
var
  GlobalIni: String;
  ProfileDir: String;
  ProfileIni: String;
begin
  GlobalIni := ExpandConstant('{userappdata}\obs-studio\global.ini');
  if not FileExists(GlobalIni) then
    Exit;

  ProfileDir := GetIniString('Basic', 'ProfileDir', '', GlobalIni);
  if ProfileDir = '' then
    Exit;

  ProfileIni := ExpandConstant('{userappdata}\obs-studio\basic\profiles\') + ProfileDir + '\basic.ini';
  if not FileExists(ProfileIni) then
    Exit;

  { Enable both sections. OBS will use the one matching the active Output mode. }
  SetIniString('SimpleOutput', 'RecRB', 'true', ProfileIni);
  SetIniString('SimpleOutput', 'RecRBTime', '90', ProfileIni);
  SetIniString('AdvOut', 'RecRB', 'true', ProfileIni);
  SetIniString('AdvOut', 'RecRBTime', '90', ProfileIni);
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
    EnableReplayBufferForCurrentProfile;
end;
