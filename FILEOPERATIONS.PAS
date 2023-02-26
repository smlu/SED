unit FileOperations;

{ This unit contains the service functions and
  procedures for file and directory operations. }

interface

uses Windows, files, Classes, ComCTRLS, StdCtrls, SysUtils,
  {ProgressDialog,} Forms, Graphics, ShlObj, GlobalVars, COntainers;

Const
  { OpenFileWrite flags }
  fm_Create = 1; { Create new file }
  fm_LetReWrite = 2; { Let rewrite file
    if exists - OpenFileWrite }
  fm_AskUser = 4; { Ask user if something }
  fm_CreateAskRewrite = fm_Create + fm_LetReWrite + fm_AskUser;
  { OpenFileRead&Write flags }
  fm_Share = 8; { Let share file }
  fm_Buffered = 16;

Type

  TWildCardMask = class
  private
    masks: TStringList;
    Procedure SetMask(s: string);
    Function GetMask: String;
  Public
    Property mask: String read GetMask Write SetMask;
    Procedure AddTrailingAsterisks;
    Function Match(s: String): boolean;
    Destructor Destroy; override;
  end;

  TMaskedDirectoryControl = class
    Dir: TContainerFile;
    LBDir: TListBox;
    LVDir: TListView;
    mask: String;
    control_type: (LBox, LView);
    LastViewStyle: TViewStyle;
    Constructor CreateFromLB(L: TListBox); { ListBox }
    Constructor CreateFromLV(L: TListView); { ListView }
    Procedure SetDir(D: TContainerFile);
    Procedure SetMask(mask: string);
  Private
    Procedure AddFile(s: string; fi: TFileInfo);
    Procedure ClearControl;
    Procedure BeginUpdate;
    Procedure EndUpdate;
  end;

  TFileTStream = class(TStream)
    f: TFile;
    Constructor CreateFromTFile(af: TFile);
    function Read(var Buffer; Count: Longint): Longint; override;
    function Write(const Buffer; Count: Longint): Longint; override;
    function Seek(Offset: Longint; Origin: Word): Longint; override;
  end;

Function OpenFileRead(const path: TFileName; mode: Word): TFile;
Function OpenFileWrite(const path: TFileName; mode: Word): TFile;
Function IsContainer(const path: TFileName): boolean;
Function IsInContainer(const path: TFileName): boolean;
Function OpenContainer(const path: TFileName): TContainerFile;
Function OpenGameContainer(const path: TFileName): TContainerFile;
Function OpenGameFile(const name: TFileName): TFile;
Function FindProjDirFile(const name: string): string;
{ opens it just once and keeps it open then only returns references on it.
  Used for game data files }

Function ExtractExt(path: String): String;
Function ExtractPath(path: String): String;
Function ExtractName(path: String): String;
Procedure CopyFileData(Ffrom, Fto: TFile; size: Longint);
Function CopyAllFile(const Fname, ToName: String): boolean;
Function BackupFile(const name: String): String;
Procedure ListDirMask(const path, mask: String; sl: TStringList);
Function ChangeExt(path: String; const newExt: String): String;

Function ConcPath(const path1, path2: string): string;

Function GetCurDir: string;
Procedure SetCurDir(const Dir: string);

implementation

uses Misc_utils;

var
  CopyBuf: array [0 .. $8000 - 1] of byte;
  COntainers: TStringList;

Function BackupFile(Const name: String): String;
var
  cext: String;
begin
  if not FileExists(name) then
    exit;
  cext := ExtractExt(Name);
  Insert('~', cext, 2);
  if length(cext) > 4 then
    setLength(cext, 4);
  Result := ChangeFileExt(Name, cext);
  if FileExists(Result) then
    DeleteFile(Result);
  RenameFile(Name, Result);
end;

Procedure ListDirMask(const path, mask: String; sl: TStringList);
var
  sr: TSearchRec;
  Res: Integer;
  CurMask: array [0 .. 128] of char;
  P, PM: Pchar;
begin
  sl.Clear;
  PM := Pchar(mask);
  Repeat
    P := StrScan(PM, ';');
    if P = nil then
      P := StrEnd(PM);
    StrLCopy(CurMask, PM, P - PM);

    Res := FindFirst(path + CurMask, faAnyFile, sr);
    While Res = 0 do
    begin
      if (sr.Attr and (faVolumeID + faDirectory)) = 0 then
        sl.Add(sr.name);
      Res := FindNext(sr);
    end;
    FindClose(sr);
    if P^ = #0 then
      break
    else
      PM := P + 1;
  Until False;
end;

Function CopyAllFile(const Fname, ToName: String): boolean;
var
  f, f1: TFile;
begin
  Result := true;
  f := OpenFileRead(Fname, 0);
  f1 := OpenFileWrite(ToName, 0);
  CopyFileData(f, f1, f.Fsize);
  f.Fclose;
  f1.Fclose;
end;

Procedure CopyFileData(Ffrom, Fto: TFile; size: Longint);
begin
  While size > sizeof(CopyBuf) do
  begin
    Ffrom.Fread(CopyBuf, sizeof(CopyBuf));
    Fto.FWrite(CopyBuf, sizeof(CopyBuf));
    dec(size, sizeof(CopyBuf));
  end;
  Ffrom.Fread(CopyBuf, size);
  Fto.FWrite(CopyBuf, size);
end;

Function GetQuote(ps, quote: Pchar): Pchar;
var
  P, p1: Pchar;
begin
  if ps^ in ['?', '*'] then
  begin
    GetQuote := ps + 1;
    quote^ := ps^;
    (quote + 1)^ := #0;
    exit;
  end;
  P := StrScan(ps, '?');
  if P = nil then
    P := StrEnd(ps);
  p1 := StrScan(ps, '*');
  if p1 = nil then
    p1 := StrEnd(ps);
  if P > p1 then
    P := p1;
  StrLCopy(quote, ps, P - ps);
  GetQuote := P;
end;

Function WildCardMatch(mask, s: string): boolean;
var
  pmask, ps, P: Pchar;
  quote: array [0 .. 100] of char;
begin
  { mask[length(mask)+1]:=#0;
    s[length(s)+1]:=#0; }
  Result := False;
  pmask := @mask[1];
  ps := @s[1];
  While pmask^ <> #0 do
  begin
    pmask := GetQuote(pmask, quote);
    case quote[0] of
      '?':
        if ps^ <> #0 then
          inc(ps);
      '*':
        begin
          P := GetQuote(pmask, quote);
          if quote[0] in ['*', '?'] then
            continue;
          if quote[0] = #0 then
          begin
            ps := StrEnd(ps);
            continue;
          end;
          pmask := P;
          P := StrPos(ps, quote);
          if P = nil then
            exit;
          ps := P + StrLen(quote);
        end;
    else
      if StrLComp(ps, quote, StrLen(quote)) = 0 then
        inc(ps, StrLen(quote))
      else
        exit;
    end;
  end;
  if ps^ = #0 then
    Result := true;
end;

Function ParseMasks(m: string): TStringList;
{ mask -> masks string list. ie to handle "*.txt;*.asc" type masks }
var
  P, ps: Pchar;
  s: array [0 .. 255] of char;
  Msk: TStringList;
begin
  Msk := TStringList.Create;
  if m = '' then
  begin
    Msk.Add('');
    Result := Msk;
    exit;
  end;
  ps := @m[1];
  Repeat
    P := StrScan(ps, ';');
    if P = nil then
      P := StrEnd(ps);
    StrLCopy(s, ps, P - ps);
    Msk.Add(UpperCase(s));
    ps := P;
    if ps^ = ';' then
      inc(ps);
  Until ps^ = #0;
  Result := Msk;
end;

Procedure TWildCardMask.SetMask(s: string);
begin
  if masks <> nil then
  begin
    masks.free;
    masks := nil;
  end;
  masks := ParseMasks(s);
end;

Destructor TWildCardMask.Destroy;
begin
  masks.free;
end;

Function TWildCardMask.GetMask: String;
var
  i: Integer;
begin
  Result := '';
  for i := 0 to masks.Count - 1 do
    Result := Concat(Result, masks[i]);
end;

Procedure TWildCardMask.AddTrailingAsterisks;
var
  i: Integer;
  s: string;
begin
  for i := 0 to masks.Count - 1 do
  begin
    s := masks[i];
    if s = '' then
      s := '*'
    else if s[length(s)] <> '*' then
      s := s + '*';
    masks[i] := s;
  end;
end;

Function TWildCardMask.Match(s: String): boolean;
var
  i: Integer;
begin
  s := UpperCase(s);
  Result := False;
  for i := 0 to masks.Count - 1 do
  begin
    Result := Result or WildCardMatch(masks.Strings[i], s);
    if Result then
      break;
  end;
end;

Type
  ct_type = (ct_unknown, ct_gob, ct_gob2, ct_goo, ct_wad, ct_lab, ct_lfd,
    ct_notfound);

Function WhatContainer(path: String): ct_type;
var
  ext: String;
  buf: array [1 .. 4] of char;
  f: TFile;
begin
  Result := ct_unknown;
  if not FileExists(path) then
  begin
    Result := ct_notfound;
    exit;
  end;

  ext := UpperCase(ExtractFileExt(path));
  if ext = '.WAD' then
    Result := ct_wad
  else if ext = '.GOB' then
    Result := ct_gob
  else if ext = '.LAB' then
    Result := ct_lab
  else if ext = '.LFD' then
    Result := ct_lfd
  else if ext = '.GOO' then
    Result := ct_goo;
  if Result = ct_gob then
  begin
    Try
      f := OpenFileRead(path, 0);
      f.Fread(buf, 4);
      if buf = 'GOB ' then
        Result := ct_gob2;
      f.Fclose;
    except
      on Exception do
        Result := ct_unknown;
    end;

  end;
end;

Function IsInContainer(const path: TFileName): boolean;
begin
  Result := Pos('>', path) <> 0;
end;

Function IsContainer(Const path: TFileName): boolean;
begin
  Result := WhatContainer(path) <> ct_unknown;
end;

Function OpenContainer(Const path: TFileName): TContainerFile;
begin
  Case WhatContainer(path) of
    ct_gob:
      Result := TGOBDirectory.CreateOpen(path);
    { ct_wad: Result:=TWADDirectory.CreateOpen(path);
      ct_lab: Result:=TLABDirectory.CreateOpen(path);
      ct_lfd: Result:=TFLDDirectory.CreateOpen(path); }
    ct_gob2:
      Result := TGOB2Directory.CreateOpen(path);
    ct_goo:
      Result := TGOB2Directory.CreateOpen(path);
    ct_notfound:
      Raise Exception.Create(path + ' not found');
  else
    Raise Exception.Create(path + ' is not a container');
  end;
end;

Function OpenFileRead(Const path: TFileName; mode: Word): TFile;
var
  ps, i: Integer;
  Fname, ContName: String;
  cf: TContainerFile;
begin
  Result := nil;
  ps := Pos('>', path);
  if ps = 0 then
    Result := TDiskFile.CreateRead(path)
  else
  begin
    ContName := Copy(path, 1, ps - 1);
    Fname := Copy(path, ps + 1, length(path) - ps);
    i := COntainers.IndexOf(ContName);
    if i <> -1 then
      cf := TContainerFile(COntainers.Objects[i])
    else
    begin
      if COntainers.Count > 10 then
        for i := 0 to COntainers.Count - 1 do
          With COntainers.Objects[i] as TContainerFile do
            if not Permanent then
            begin
              free;
              COntainers.Delete(i);
              break;
            end;
      cf := OpenContainer(ContName);
      COntainers.AddObject(ContName, cf);
    end;
    Result := cf.OpenFile(Fname, 0);
  end;
end;

Function FindProjDirFile(const name: string): string;
var
  ext: string;

  Function Check(const Dir: string; var Res: string): boolean;
  begin
    Result := False;
    if FileExists(ProjectDir + Dir + name) then
    begin
      Result := true;
      Res := ProjectDir + Dir + name;
    end;
  end;

begin
  ext := LowerCase(ExtractFileExt(name));

  if Check('', Result) then
    exit;

  if ext = '.uni' then
  begin
    if Check('misc\', Result) then
      exit;
    if Check('ui\', Result) then
      exit;
  end;

end;

Function OpenGameContainer(const path: string): TContainerFile;
var
  i: Integer;
begin
  i := COntainers.IndexOf(path);
  if i <> -1 then
    Result := TContainerFile(COntainers.Objects[i])
  else
  begin
    Result := OpenContainer(path);
    COntainers.AddObject(path, Result);
  end;
  Result.Permanent := true;
end;

Function OpenGameFile(const name: TFileName): TFile;
var
  cf: TContainerFile;
  ext: String;

  function IsInGob(const GobName, Fname: String): boolean;
  begin
    if GobName = '' then
    begin
      Result := False;
      exit;
    end;
    Try
      cf := OpenGameContainer(GobName);
      Result := cf.FileExists(Fname);
    except
      On Exception do
        Result := False;
    end;
  end;

  Function TryFile(const name: string): TFile;
  begin
    Result := nil;
    if FileExists(ProjectDir + Name) then
      Result := OpenFileRead(ProjectDir + Name, 0);
  end;

begin
  Result := nil;
  if FileExists(ProjectDir + Name) then
  begin
    Result := OpenFileRead(ProjectDir + Name, 0);
    exit;
  end;
  ext := UpperCase(ExtractExt(name));

  if ext = '.WAV' then
  begin
    Result := TryFile('sound\' + Name);
    if Result <> nil then
      exit;
    Result := TryFile('voice\' + Name);
    if Result <> nil then
      exit;
    Result := TryFile('voiceuu\' + Name);
    if Result <> nil then
      exit;

    if IsInGob(Res1_Gob, 'sound\' + Name) then
    begin
      Result := OpenFileRead(Res1_Gob + '>sound\' + name, 0);
      exit;
    end;
    if IsInGob(Res1_Gob, 'voice\' + Name) then
    begin
      Result := OpenFileRead(Res1_Gob + '>voice\' + name, 0);
      exit;
    end;
    if IsMots then
      if IsInGob(Res2_Gob, 'voiceuu\' + Name) then
      begin
        Result := OpenFileRead(Res2_Gob + '>voiceuu\' + name, 0);
        exit;
      end;

  end;

  if (ext = '.MAT') then
  begin
    Result := TryFile('mat\' + Name);
    if Result <> nil then
      exit;
    Result := TryFile('3do\mat\' + Name);
    if Result <> nil then
      exit;

    if IsInGob(Res2_Gob, 'mat\' + Name) then
    begin
      Result := OpenFileRead(Res2_Gob + '>mat\' + name, 0);
      exit;
    end;
    if IsInGob(Res2_Gob, '3do\mat\' + Name) then
    begin
      Result := OpenFileRead(Res2_Gob + '>3do\mat\' + name, 0);
      exit;
    end;
  end;

  if (ext = '.DAT') then
  begin
    Result := TryFile('misc\' + Name);
    if Result <> nil then
      exit;
    if IsInGob(Res2_Gob, 'misc\' + Name) then
    begin
      Result := OpenFileRead(Res2_Gob + '>misc\' + name, 0);
      exit;
    end;
  end;

  if (ext = '.3DO') then
  begin
    Result := TryFile('3do\' + Name);
    if Result <> nil then
      exit;
    if IsInGob(Res2_Gob, '3do\' + Name) then
    begin
      Result := OpenFileRead(Res2_Gob + '>3do\' + name, 0);
      exit;
    end;
  end;

  if (ext = '.KEY') then
  begin
    Result := TryFile('\3do\key\' + Name);
    if Result <> nil then
      exit;
    if IsInGob(Res2_Gob, '3do\key\' + Name) then
    begin
      Result := OpenFileRead(Res2_Gob + '>3do\key\' + name, 0);
      exit;
    end;
  end;

  if (ext = '.AI') or (ext = '.AI0') or (ext = '.AI2') then
  begin
    Result := TryFile('misc\ai\' + Name);
    if Result <> nil then
      exit;
    if IsInGob(Res2_Gob, 'misc\ai\' + Name) then
    begin
      Result := OpenFileRead(Res2_Gob + '>misc\ai\' + name, 0);
      exit;
    end;
  end;

  if (ext = '.CMP') then
  begin
    Result := TryFile('misc\cmp\' + Name);
    if Result <> nil then
      exit;
    if IsInGob(Res2_Gob, 'misc\cmp\' + Name) then
    begin
      Result := OpenFileRead(Res2_Gob + '>misc\cmp\' + name, 0);
      exit;
    end;
  end;

  if (ext = '.PAR') then
  begin
    Result := TryFile('misc\par' + Name);
    if Result <> nil then
      exit;
    if IsInGob(Res2_Gob, 'misc\par\' + Name) then
    begin
      Result := OpenFileRead(Res2_Gob + '>misc\par\' + name, 0);
      exit;
    end;
  end;

  if (ext = '.PER') then
  begin
    Result := TryFile('misc\per\' + Name);
    if Result <> nil then
      exit;
    if IsInGob(Res2_Gob, 'misc\per\' + Name) then
    begin
      Result := OpenFileRead(Res2_Gob + '>misc\per\' + name, 0);
      exit;
    end;
  end;

  if (ext = '.PUP') then
  begin
    Result := TryFile('misc\pup\' + Name);
    if Result <> nil then
      exit;
    if IsInGob(Res2_Gob, 'misc\pup\' + Name) then
    begin
      Result := OpenFileRead(Res2_Gob + '>misc\pup\' + name, 0);
      exit;
    end;
  end;

  if (ext = '.SND') then
  begin
    Result := TryFile('misc\snd\' + Name);
    if Result <> nil then
      exit;
    if IsInGob(Res2_Gob, 'misc\snd\' + Name) then
    begin
      Result := OpenFileRead(Res2_Gob + '>misc\snd\' + name, 0);
      exit;
    end;
  end;

  if (ext = '.SPR') then
  begin
    Result := TryFile('misc\spr\' + Name);
    if Result <> nil then
      exit;
    if IsInGob(Res2_Gob, 'misc\spr\' + Name) then
    begin
      Result := OpenFileRead(Res2_Gob + '>misc\spr\' + name, 0);
      exit;
    end;
  end;

  if (ext = '.COG') then
  begin
    Result := TryFile('cog\' + Name);
    if Result <> nil then
      exit;

    if IsInGob(Res2_Gob, 'cog\' + Name) then
    begin
      Result := OpenFileRead(Res2_Gob + '>cog\' + name, 0);
      exit;
    end;
    if IsInGob(sp_gob, 'cog\' + Name) then
    begin
      Result := OpenFileRead(sp_gob + '>cog\' + name, 0);
      exit;
    end;
    if IsInGob(mp1_gob, 'cog\' + Name) then
    begin
      Result := OpenFileRead(mp1_gob + '>cog\' + name, 0);
      exit;
    end;
    if IsInGob(mp2_gob, 'cog\' + Name) then
    begin
      Result := OpenFileRead(mp2_gob + '>cog\' + name, 0);
      exit;
    end;
    if IsInGob(mp3_gob, 'cog\' + Name) then
    begin
      Result := OpenFileRead(mp3_gob + '>cog\' + name, 0);
      exit;
    end;

  end;
  PanMessage(mt_warning, 'Can''t find file anywhere: ' + Name);
  Raise Exception.Create('Can''t find file anywhere: ' + Name);
end;

Function OpenFileWrite(Const path: TFileName; mode: Word): TFile;
begin
  Result := TDiskFile.CreateWrite(path);
end;

Procedure TMaskedDirectoryControl.ClearControl;
begin
  Case control_type of
    LBox:
      begin
        LBDir.Items.BeginUpdate;
        LBDir.Items.Clear;
        LBDir.Items.EndUpdate;
      end;
    LView:
      begin
        LVDir.Items.BeginUpdate;
        LVDir.Items.Clear;
        LVDir.Items.EndUpdate;
      end;
  end;
end;

Procedure TMaskedDirectoryControl.SetDir(D: TContainerFile);
var
  i: Integer;
  lc: TListColumn;
  FontWidth: Integer;
  Function LineWidth(n: Integer): Integer;
  begin
    Result := n * FontWidth;
  end;

begin
  Dir := D;
  if D = nil then
    exit;
  case control_type of
    LBox:
      FontWidth := LBDir.Font.size;
    LView:
      FontWidth := LVDir.Font.size;
  end;

  Case control_type of
    LBox:
      begin
        LBDir.Columns := LBDir.Width div LineWidth(Dir.GetColWidth(0));
      end;
    LView:
      begin
        LVDir.Columns.Clear;
        for i := 0 to Dir.ColumnCount - 1 do
        begin
          lc := LVDir.Columns.Add;
          lc.Caption := Dir.GetColName(i);
          lc.Width := LineWidth(Dir.GetColWidth(i));
        end;
      end;
  end;
end;

Constructor TMaskedDirectoryControl.CreateFromLB(L: TListBox);
begin
  LBDir := L;
  control_type := LBox;
  mask := '*.*';
end;

Constructor TMaskedDirectoryControl.CreateFromLV(L: TListView);
begin
  LVDir := L;
  control_type := LView;
  mask := '*.*'
end;

Procedure TMaskedDirectoryControl.BeginUpdate;
begin
  Case control_type of
    LBox:
      begin
        LBDir.Sorted := False;
        LBDir.Items.BeginUpdate;
      end;
    LView:
      begin
        LastViewStyle := LVDir.ViewStyle;
        LVDir.ViewStyle := vsReport;
        LVDir.Items.BeginUpdate;
      end;
  end;
end;

Procedure TMaskedDirectoryControl.EndUpdate;
begin
  Case control_type of
    LBox:
      begin
        LBDir.Items.EndUpdate;
      end;
    LView:
      begin
        LVDir.ViewStyle := LastViewStyle;
        LVDir.Items.EndUpdate;
      end;
  end;
end;

procedure TMaskedDirectoryControl.AddFile;
var
  LI: TListItem;
begin
  Case control_type of
    LBox:
      if fi = nil then
        LBDir.Items.AddObject('[' + s + ']', fi)
      else
        LBDir.Items.AddObject(s, fi);
    LView:
      With LVDir.Items do
      begin
        LI := Add;
        LI.Caption := s;
        LI.SubItems.Add(IntToStr(fi.size));
        if fi = nil then
          LI.ImageIndex := 1
        else
          LI.ImageIndex := 0;
        LI.Data := fi;
      end;
  end;
end;

Procedure TMaskedDirectoryControl.SetMask(mask: string);
var
  Ts: TStringList;
  i: Integer;
  Matcher: TWildCardMask;
begin
  if Dir = nil then
    exit;
  ClearControl;
  Matcher := TWildCardMask.Create;
  Matcher.mask := mask;

  BeginUpdate;
  { Progress.Reset(ts.count); }
  Ts := Dir.ListDirs;
  for i := 0 to Ts.Count - 1 do
    AddFile(Ts[i], nil);

  Ts := Dir.ListFiles;
  for i := 0 to Ts.Count - 1 do
  begin
    if Matcher.Match(Ts[i]) then
      AddFile(Ts[i], TFileInfo(Ts.Objects[i]));
    { Progress.Step; }
  end;
  { Progress.Hide; }
  EndUpdate;
  Matcher.free;
end;

Function ExtractExt(path: String): String;
var
  P: Integer;
begin
  P := Pos('>', path);
  if P <> 0 then
    path[P] := '\';
  Result := ExtractFileExt(path);
end;

Function ExtractPath(path: String): String;
var
  P: Integer;
begin
  P := Pos('>', path);
  if P <> 0 then
    path[P] := '\';
  Result := ExtractFilePath(path);
  if P <> 0 then
    Result[P] := '>';
end;

Function ExtractName(path: String): String;
var
  P: Integer;
begin
  P := Pos('>', path);
  if P <> 0 then
    path[P] := '\';
  Result := ExtractFileName(path);
end;

Function ChangeExt(path: String; const newExt: String): String;
var
  P: Integer;
begin
  P := Pos('>', path);
  if P <> 0 then
    path[P] := '\';
  Result := ChangeFileExt(path, newExt);
  if P <> 0 then
    Result[P] := '>';
end;

Constructor TFileTStream.CreateFromTFile(af: TFile);
begin
  f := af;
end;

function TFileTStream.Read(var Buffer; Count: Longint): Longint;
begin
  Result := f.Fread(Buffer, Count);
end;

function TFileTStream.Write(const Buffer; Count: Longint): Longint;
begin
  Result := f.FWrite(Buffer, Count);
end;

function TFileTStream.Seek(Offset: Longint; Origin: Word): Longint;
begin
  Result := 0;
  Case Origin of
    soFromBeginning:
      begin
        f.Fseek(Offset);
        Result := Offset;
      end;
    soFromCurrent:
      begin
        f.Fseek(f.FPos + Offset);
        Result := f.FPos;
      end;
    soFromEnd:
      begin
        f.Fseek(f.Fsize + Offset);
        Result := f.FPos;
      end;
  end;
end;

Function ConcPath(const path1, path2: string): string;
begin
  if path1 = '' then
  begin
    Result := path2;
    exit;
  end;
  if not(path1[length(path1)] in ['\', ':']) then
    Result := path1 + '\' + path2
  else
    Result := path1 + path2;
end;

Function GetCurDir: string;
begin
  GetDir(0, Result);
end;

Procedure SetCurDir(const Dir: string);
begin
  try
    ChDir(Dir);
  except
    on Exception do;
  end;
end;

Procedure FreeContainers;
var
  i: Integer;
begin
  for i := 0 to COntainers.Count - 1 do
    COntainers.Objects[i].free;
  COntainers.free;
end;

Initialization

begin
  COntainers := TStringList.Create;
end;

Finalization

FreeContainers;

end.
