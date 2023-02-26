unit Files;

{ This unit defines abstract file and directory
  classes as well as implementation of simple
  disk file, buffered file and a text file and
  a disk directory. Little addition }

interface

uses Classes, SysUtils, StdCtrls, FileCtrl, misc_utils;

Const
  { Constants for text files }
  txt_bufsize = 8096;
  txt_maxlinesize = 4096;

Type

  TFileInfo = class
    size, offs: longint;
    Function GetString(n: Integer): String; virtual;
  end;

  TFileChangeControl = class
    Fsize: longint;
    Ftime: Integer;
    Fname: String;
    Procedure SetFile(name: String);
    Function Changed: boolean;
  end;

  { TFileList=class(TStringList)
    private
    Procedure SetFInfo(n:Integer;fi:TFileInfo);
    Function GetFInfo(n:Integer):TFileInfo;
    public
    Property FileInfo[N:Integer]:TFileInfo read GetFInfo write SetFInfo;
    end; }

  TFile = class
    Function GetFullName: String; virtual; abstract;
    Function GetShortName: String; virtual; abstract;
    Function Fread(var buf; size: Integer): Integer; virtual; abstract;
    Function Fwrite(const buf; size: Integer): Integer; virtual; abstract;
    Function Fsize: longint; virtual; abstract;
    Function Fpos: longint; virtual; abstract;
    Procedure Fseek(Pos: longint); virtual; abstract;
    Destructor FClose; virtual; abstract;
  end;

  TContainerCreator = class { Service object. Used to modify containers }
    cf: TFile;
    Constructor Create(name: String);
    Procedure PrepareHeader(newfiles: TStringList); virtual; abstract;
    Procedure AddFile(F: TFile); virtual; abstract;
    Function ValidateName(name: String): String; virtual; abstract;
    Destructor Destroy; override;
  end;

  TContainerFile = class
    Permanent: boolean;
    name: String;
    Files: TStringList;
    Dirs: TStringList;
    tmpflist: TStringList;
    tmpdlist: TStringList;
    F: TFile;
    fc: TFileChangeControl;
    HasDirs: boolean;
    CurDir: String;
    Constructor CreateOpen(path: String);
    Procedure Free;
    Destructor Destroy; Override;
    Function GetColName(n: Integer): String; dynamic;
    Function GetColWidth(n: Integer): Integer; dynamic;
    Function ColumnCount: Integer; dynamic;
    Function GetFullName: String; virtual;
    Function GetShortName: String; dynamic;
    Function OpenFile(Fname: TFileName; mode: Integer): TFile; virtual;
    Function OpenFilebyFI(fi: TFileInfo): TFile; virtual;
    Function ChDir(const d: String): boolean; virtual;
    Procedure RenameFile(Fname, newname: TFileName); virtual; abstract;
    Function FileExists(const name: string): boolean; { full path }
    Function ListFiles: TStringList; virtual;
    Function ListDirs: TStringList; virtual;
    Function GetFileInfo(Fname: String): TFileInfo; virtual;
    Function AddFiles(AddList: TStringList): boolean; virtual;
    Function DeleteFiles(DelList: TStringList): boolean; virtual; abstract;
    Function AddFile(Fname: String): boolean; virtual;
    Function DeleteFile(Fname: String): boolean;
    Function GetContainerCreator(name: String): TContainerCreator; virtual;
      abstract; { used for adding/deleting files }
  Protected
    Procedure ClearIndex;
    { Below go the methods that have to be overridden }
    Procedure Refresh; virtual; abstract;
    { Must fill the .Files filed Format:
      Strings[n] - name
      Objects[n] - TFileInfo for the file }
  end;

  TSubFile = class(TFile)
    offs, size: longint;
    F: TFile;
    Fname: String;
    Function GetShortName: String; Override;
    Function GetFullName: String; Override;
    Constructor CreateRead(const fn, name: String; apos, asize: longint);
    Function Fread(var buf; size: Integer): Integer; override;
    Function Fsize: longint; override;
    Function Fpos: longint; override;
    Procedure Fseek(Pos: longint); override;
    Destructor FClose; override;
    Destructor Destroy; override;
  end;

  TBufFile = class(TFile)
    F: TFile;
    pbuf: PByte;
    spos: longint;
    bpos, bsize: word;
    bcapacity: word;
    fmode: (f_read, f_write);
    Constructor CreateRead(bf: TFile);
    Constructor CreateWrite(bf: TFile);
    Function GetFullName: String; override;
    Function GetShortName: String; override;
    Function Fread(var buf; size: Integer): Integer; override;
    Function Fwrite(const buf; size: Integer): Integer; override;
    Function Fsize: longint; override;
    Function Fpos: longint; override;
    Procedure Fseek(Pos: longint); override;
    Destructor FClose; override;
  Private
    Procedure InitBuffer(size: word);
    Procedure ReadBuffer;
    Procedure WriteBuffer;
  end;

  TTextFile = Class
    F: TFile;
    bpos, bsize: word;
    buffer: array [0 .. txt_bufsize] of AnsiChar;
    curline: Integer;
    Function GetFullName: String;
    Constructor CreateRead(bf: TFile);
    Constructor CreateWrite(bf: TFile);
    Procedure Readln(var s: String);
    Procedure Writeln(s: String);
    Procedure WritelnFmt(const fmt: String; const args: array of const);
    Function Eof: boolean;
    Destructor FClose;
    Destructor Destroy; override;
    Function Fsize: longint;
    Function Fpos: longint;
    Property CurrentLine: Integer read curline;
  Private
    Procedure LoadBuffer;
    Procedure SeekEoln;
  end;

  TLECTextFile = Class(TTextFile)
    FComment: String;
    Procedure Readln(var s: String);
    Property Comment: String read FComment;
  end;

  { TParsedTextFile=class(TTextFile)
    private
    fsp:TStringParser;
    Public
    Constructor CreateRead(bf:TFile);
    Procedure ReadLine;
    Property Parsed: TStringParser read fsp;
    Destructor FClose;
    end; }

  TDiskDirectory = class(TContainerFile)
    name: String;
    Constructor CreateFromPath(Dname: TFileName);
    Function GetFullName: String; override;
    Function GetShortName: String; override;
    Function OpenFile(Fname: TFileName; mode: Integer): TFile; override;
    Procedure RenameFile(Fname, newname: TFileName); override;
    Function DeleteFiles(DelList: TStringList): boolean; override;
    Function ListFiles: TStringList; override;
    Function AddFiles(AddList: TStringList): boolean; override;
  end;

  TDiskFile = class(TFile)
    F: File;
    Constructor CreateRead(path: TFileName);
    Constructor CreateWrite(path: TFileName);
    Function GetFullName: String; override;
    Function GetShortName: String; override;
    Function Fread(var buf; size: Integer): Integer; override;
    Function Fwrite(const buf; size: Integer): Integer; override;
    Function Fsize: longint; override;
    Function Fpos: longint; override;
    Procedure Fseek(Pos: longint); override;
    Destructor FClose; override;
    Destructor Destroy; override;
  end;

implementation

Uses FileOperations;

{ Procedure TFileList.SetFInfo(n:Integer;fi:TFileInfo);
  begin
  Objects[n]:=fi;
  end;

  Function TFileList.GetFInfo(n:Integer):TFIleInfo;
  begin
  Result:=TFileInfo(Objects[n]);
  end; }

Function TFileInfo.GetString(n: Integer): String;
begin
  if n = 1 then
    Result := IntToStr(size)
  else
    Result := '';
end;

Constructor TDiskFile.CreateRead(path: TFileName);
begin
  Assign(F, path);
  FileMode := 0;
  Reset(F, 1);
end;

Constructor TDiskFile.CreateWrite(path: TFileName);
begin
  Assign(F, path);
  ReWrite(F, 1);
end;

Function TDiskFile.GetFullName: String;
begin
  Result := TFileRec(F).name;
end;

Function TDiskFile.GetShortName: String;
begin
  Result := ExtractFileName(GetFullName);
end;

Function TDiskFile.Fread(var buf; size: Integer): Integer;
begin
  BlockRead(F, buf, size, Result);
end;

Function TDiskFile.Fwrite(const buf; size: Integer): Integer;
begin
  BlockWrite(F, buf, size, Result);
end;

Function TDiskFile.Fsize: longint;
begin
  Result := FileSize(F);
end;

Function TDiskFile.Fpos: longint;
begin
  Result := FilePos(F);
end;

Procedure TDiskFile.Fseek(Pos: longint);
begin
  Seek(F, Pos);
end;

Destructor TDiskFile.FClose;
begin
  CloseFile(F);
end;

Destructor TDiskFile.Destroy;
begin
  CloseFile(F);
end;

{ TTextFile methods }

Destructor TTextFile.FClose;
begin
  F.FClose;
end;

Destructor TTextFile.Destroy;
begin
  F.FClose;
end;

Function TTextFile.GetFullName: String;
begin
  Result := F.GetFullName;
end;

Constructor TTextFile.CreateRead(bf: TFile);
begin
  F := bf;
  F.Fseek(0);
  LoadBuffer;
end;

Function TTextFile.Fsize: longint;
begin
  Result := F.Fsize;
end;

Function TTextFile.Fpos: longint;
begin
  Result := F.Fpos + bsize - bpos;
end;

Constructor TTextFile.CreateWrite(bf: TFile);
begin
  F := bf;
end;

Procedure TTextFile.SeekEoln;
var
  ps: PAnsiChar;
begin
  ps := StrScan(@buffer[bpos], AnsiChar(#10));
  if ps <> nil then { EoLn found }
  begin
    bpos := ps - @buffer + 1;
    if bpos = bsize then
      LoadBuffer;
  end
  else { Eoln not found }
  begin
    Repeat
      LoadBuffer;
      if Eof then
        exit;
      ps := StrScan(buffer, #10);
    Until (ps <> nil);
    bpos := ps - @buffer;
    SeekEoln;
  end;
end;

Procedure TTextFile.Readln(var s: String);
var
  ps, pend: PAnsiChar;
  tmp: array [0 .. txt_maxlinesize - 1] of AnsiChar;
  ssize: word;
  l: word;
begin
  s := '';
  if bpos = bsize then
    LoadBuffer;
  if Eof then
    exit;

  Inc(curline);

  ps := buffer + bpos;
  pend := StrScan(ps, AnsiChar(#10));
  if pend <> nil then
  begin
    ssize := pend - ps;
    if ssize > txt_maxlinesize then
      ssize := txt_maxlinesize; { Limit string size to 255 chars }
    s := StrLCopy(tmp, ps, ssize);
    l := Length(s);
    if l <> 0 then
      if s[l] = #13 then
        SetLength(s, l - 1);
    Inc(bpos, ssize);
    SeekEoln;
  end
  else
  begin
    ssize := bsize - bpos;
    if ssize > txt_maxlinesize then
      ssize := txt_maxlinesize;
    s := StrLCopy(tmp, ps, ssize); { copy the tail of the buffer }
    LoadBuffer;
    if Eof then
      exit;
    pend := StrScan(buffer, #10);
    if pend = nil then
      ssize := bsize
    else
      ssize := pend - @buffer;
    if ssize + Length(s) > txt_maxlinesize then
      ssize := txt_maxlinesize - Length(s);
    s := ConCat(s, StrLCopy(tmp, buffer, ssize));
    Inc(bpos, ssize);
    l := Length(s);
    if l <> 0 then
      if s[l] = #10 then
        SetLength(s, l - 1);
    l := Length(s);
    if l <> 0 then
      if s[l] = #13 then
        SetLength(s, l - 1);
    SeekEoln;
  end;
end;

Procedure TTextFile.Writeln;
const
  eol: array [0 .. 0] of char = #10;
begin
  F.Fwrite(s[1], Length(s));
  F.Fwrite(eol, sizeof(eol));
end;

Procedure TTextFile.WritelnFmt(const fmt: String; const args: array of const);
begin
  Writeln(Format(fmt, args));
end;

Function TTextFile.Eof: boolean;
begin
  Result := bsize = 0;
end;

Procedure TTextFile.LoadBuffer;
var
  bytes: longint;
begin
  bytes := F.Fsize - F.Fpos;
  if bytes > txt_bufsize then
    bytes := txt_bufsize;
  F.Fread(buffer, bytes);
  bpos := 0;
  bsize := bytes;
  buffer[bsize] := #0;
end;

{ TLECTextFile }
Procedure TLECTextFile.Readln(var s: String);
var
  p: Integer;
begin
  Inherited Readln(s);
  p := Pos('#', s);
  if p = 0 then
    FComment := ''
  else
  begin
    FComment := Copy(s, p, Length(s) - p);
    SetLength(s, p - 1);
  end;
end;

{ TParsedTextFile }

{ Constructor TParsedTextFile.CreateRead(bf:TFile);
  begin
  fsp:=TStringParser.Create;
  Inherited CreateRead(bf);
  end;

  Procedure TParsedTextFile.ReadLine;
  var s:String;p:Integer;
  begin
  Readln(s);
  p:=pos('#',s); if p<>0 then SetLength(s,p-1);
  fsp.ParseString(s);
  end;

  Destructor TParsedTextFile.FClose;
  begin
  fsp.free;
  Inherited;
  end; }

{ TBufFile methods }

Procedure TBufFile.InitBuffer(size: word);
begin
  bcapacity := size;
  GetMem(pbuf, bcapacity);
end;

Procedure TBufFile.ReadBuffer;
var
  bytes: Integer;
begin
  bytes := F.Fsize - spos;
  if bytes > bcapacity then
    bytes := bcapacity;
  bsize := bytes;
  F.Fread(pbuf^, bsize);
  Inc(spos, bsize);
end;

Procedure TBufFile.WriteBuffer;
begin
  F.Fwrite(pbuf^, bpos);
  bpos := 0;
end;

Constructor TBufFile.CreateRead(bf: TFile);
begin
  InitBuffer(2048);
  F := bf;
  ReadBuffer;
  fmode := f_read;
end;

Constructor TBufFile.CreateWrite(bf: TFile);
begin
  fmode := f_write;
end;

Function TBufFile.GetFullName: String;
begin
  Result := F.GetFullName;
end;

Function TBufFile.GetShortName: String;
begin
  Result := F.GetShortName;
end;

Function TBufFile.Fread(var buf; size: Integer): Integer;
var
  bleft: Integer;
begin
  if bpos + size < bsize then
  begin
    move((pbuf + bpos)^, buf, size);
    Inc(bpos, size);
  end
  else
  begin
    bleft := bsize - bpos;
    move((pbuf + bpos)^, buf, bleft);
    Fread((PAnsiChar(@buf) + bleft)^, size - bleft);
    Inc(spos, bpos + size);
    bpos := 0;
    bsize := 0;
  end;
end;

Function TBufFile.Fwrite(const buf; size: Integer): Integer;
begin
  if bpos + size < bcapacity then
  begin
    move(buf, (pbuf + bpos)^, size);
    Inc(bpos, size);
  end
  else
  begin
    WriteBuffer;
    F.Fwrite(buf, size);
    Inc(spos, size);
  end;
end;

Function TBufFile.Fsize: longint;
begin
  Result := F.Fsize;
end;

Function TBufFile.Fpos: longint;
begin
  Result := spos + bpos;
end;

Procedure TBufFile.Fseek(Pos: longint);
begin
  if fmode = f_write then
    WriteBuffer;
  F.Fseek(Pos);
  spos := Pos;
  if fmode = f_read then
    ReadBuffer;
end;

Destructor TBufFile.FClose;
begin
  if fmode = f_write then
    WriteBuffer;
  F.FClose;
  FreeMem(pbuf);
end;

Constructor TDiskDirectory.CreateFromPath(Dname: TFileName);
begin
  if (Dname <> '') and (Dname[Length(Dname)] <> '\') and
    (Dname[Length(Dname)] <> ':') then
    Name := Dname + '\'
  else
    Name := Dname;
  If DirectoryExists(Dname) then
  else
    MkDir(Dname);
end;

Function TDiskDirectory.OpenFile(Fname: TFileName; mode: Integer): TFile;
begin
  Result := TDiskFile.CreateRead(name + Fname);
end;

Procedure TDiskDirectory.RenameFile(Fname, newname: TFileName);
begin
  SysUtils.RenameFile(Name + Fname, Name + newname);
end;

Function TDiskDirectory.DeleteFiles(DelList: TStringList): boolean;
var
  i: Integer;
begin
  for i := 0 to Files.count - 1 do
    SysUtils.DeleteFile(Files[i]);
  Result := true;
end;

Function TDiskDirectory.GetShortName: String;
begin
  Result := name;
end;

Function TDiskDirectory.GetFullName: String;
begin
  Result := Name;
end;

Function TDiskDirectory.ListFiles: TStringList;
var
  sr: TSearchRec;
  Files: TStringList;
  res: Integer;
  fi: TFileInfo;
  i: Integer;
begin
  if Files <> nil then
  begin
    for i := 0 to Files.count - 1 do
      Files.objects[i].Free;
    Files.Free;
  end;

  Files := TStringList.Create;
  res := FindFirst(Name + '*.*', faHidden + faSysFile + faArchive +
    faReadOnly, sr);
  While res = 0 do
  begin
    fi := TFileInfo.Create;
    fi.size := sr.size;
    Files.AddObject(sr.name, fi);
    res := FindNext(sr);
  end;
  FindClose(sr);
  Result := Files;
end;

Function TDiskDirectory.AddFiles(AddList: TStringList): boolean;
begin
end;

Constructor TContainerCreator.Create(name: String);
begin
  cf := OpenFileWrite(name, fm_Create or fm_LetReWrite);
end;

Destructor TContainerCreator.Destroy;
begin
  if cf <> nil then
    cf.FClose;
end;

Procedure TContainerFile.Free;
begin
  if not Permanent then
    Destroy;
end;

Function TContainerFile.GetColWidth(n: Integer): Integer;
begin
  case n of
    0:
      Result := 13;
    1:
      Result := 10;
  else
    Result := 10;
  end;
end;

Function TContainerFile.GetColName(n: Integer): String;
begin
  case n of
    0:
      Result := 'Name';
    1:
      Result := 'Size';
  else
    Result := '';
  end;
end;

Function TContainerFile.ColumnCount: Integer;
begin
  Result := 2;
end;

Function TContainerFile.OpenFile(Fname: TFileName; mode: Integer): TFile;
var
  i: Integer;
begin
  if fc.Changed then
    Refresh;
  if mode <> 0 then
    raise Exception.Create('Cannot write file inside container: ' + Fname);
  i := Files.IndexOf(Fname);
  if i = -1 then
    raise Exception.Create('File not found: ' + Fname);
  With TFileInfo(Files.objects[i]) do
    Result := TSubFile.CreateRead(Name, Fname, offs, size);
end;

Function TContainerFile.OpenFilebyFI;
var
  i: Integer;
begin
  if fc.Changed then
    raise Exception.Create('File has be changed! reload.');
  i := Files.IndexOfObject(fi);
  if i = -1 then
    raise Exception.Create('OpenFileByFI: Cannot find file');
  Result := TSubFile.CreateRead(Name, Files[i], fi.offs, fi.size);
end;

Function GetDirAt(const s: string; p: Integer): string;
var
  sp: pchar;
begin
  sp := StrScan(@s[p], '\');
  if sp = nil then
  Begin
    Result := StrPas(PChar(@s[p]));
    exit;
  end;
  Result := Copy(s, p, sp - @s[p]);
end;

Function IsLastinPath(const path: string; p: Integer): boolean;
begin
  Result := StrScan(@path[p], '\') = nil;
end;

Function TContainerFile.ListDirs: TStringList;
var
  i: Integer;
  Dname, dir: string;
begin
  if fc.Changed then
    Refresh;
  tmpdlist.clear;
  dir := CurDir;
  if dir <> '' then
    dir := dir + '\';
  for i := 0 to Dirs.count - 1 do
  begin
    Dname := Dirs[i];
    if CompareText(Copy(Dname, 1, Length(dir)), dir) <> 0 then
      continue;
    if not IsLastinPath(Dname, Length(dir) + 1) then
      continue;
    tmpdlist.Add(GetDirAt(Dname, Length(dir) + 1));
  end;
  Result := tmpdlist;
end;

Function TContainerFile.ChDir(const d: String): boolean;
var
  i: Integer;
begin
  Result := false;
  if (d <> '') and (Dirs.IndexOf(d) = -1) then
    exit;
  CurDir := d;
  Result := true;
end;

Function TContainerFile.DeleteFile(Fname: String): boolean;
var
  sl: TStringList;
begin
  sl := TStringList.Create;
  sl.Add(name);
  DeleteFiles(sl);
  sl.Free;
end;

Function TContainerFile.AddFile(Fname: String): boolean;
var
  sl: TStringList;
begin
  sl := TStringList.Create;
  sl.Add(Fname);
  AddFiles(sl);
  sl.Free;
end;

Constructor TContainerFile.CreateOpen;
begin
  Name := path;
  Files := TStringList.Create;
  Dirs := TStringList.Create;
  tmpflist := TStringList.Create;
  tmpdlist := TStringList.Create;
  fc := TFileChangeControl.Create;
  fc.SetFile(name);
  Refresh;
end;

Destructor TContainerFile.Destroy;
var
  i: Integer;
begin
  for i := 0 to Files.count - 1 do
    Files.objects[i].Free;
  for i := 0 to Dirs.count - 1 do
    Dirs.objects[i].Free;
  if tmpflist <> nil then
    tmpflist.Free;
  if tmpdlist <> nil then
    tmpdlist.Free;
  fc.Free;
  Files.Free;
  Dirs.Free;
end;

Function TContainerFile.ListFiles: TStringList;
var
  i: Integer;
  dir: string;
  Fname: string;
begin
  if fc.Changed then
    Refresh;
  tmpflist.clear;
  dir := CurDir;
  if dir <> '' then
    dir := dir + '\';
  for i := 0 to Files.count - 1 do
  begin
    Fname := Files[i];
    if CompareText(Copy(Fname, 1, Length(dir)), dir) <> 0 then
      continue;
    if not IsLastinPath(Fname, Length(dir) + 1) then
      continue;
    tmpflist.AddObject(GetDirAt(Fname, Length(dir) + 1), Files.objects[i]);
  end;
  Result := tmpflist;
end;

Function TContainerFile.FileExists(const name: string): boolean;
begin
  Result := Files.IndexOf(name) <> -1;
end;

Procedure TContainerFile.ClearIndex;
var
  i: Integer;
begin
  for i := 0 to Files.count - 1 do
    Files.objects[i].Free;
  Files.clear;
end;

Function TContainerFile.GetFileInfo(Fname: String): TFileInfo;
var
  i: Integer;
begin
  i := Files.IndexOf(Fname);
  if i = -1 then
    Result := nil
  else
    Result := TFileInfo(Files.objects[i]);
end;

Function TContainerFile.GetFullName: String;
begin
  Result := name;
end;

Function TContainerFile.GetShortName: String;
begin
  Result := ExtractFileName(GetFullName);
end;

Function TContainerFile.AddFiles(AddList: TStringList): boolean;
var
  CCreator: TContainerCreator;
  newname: String;
  newfiles: TStringList;
  cName, Fname: String;
  i, n: Integer;
  F: TFile;
begin
  newname := ChangeFileExt(name, '.tmp');
  CCreator := GetContainerCreator(newname);
  newfiles := TStringList.Create;
  newfiles.Assign(Files);
  for i := 0 to AddList.count - 1 do
  begin
    Fname := AddList[i];
    n := Files.IndexOf(ExtractName(Fname));
    if n = -1 then
      newfiles.Add(Fname)
    else
    begin
      newfiles[n] := Fname;
      newfiles.objects[n] := nil;
    end;
  end;
  CCreator.PrepareHeader(newfiles);
  For i := 0 to newfiles.count - 1 do
  begin
    if newfiles.objects[i] = nil then { From Container }
      F := OpenFile(newfiles[i], 0)
    else { Outside file }
      F := OpenFileRead(newfiles[i], 0);
    CCreator.AddFile(F);
    F.FClose;
  end;
  newfiles.Free;
  CCreator.Free;
  BackUpFile(Name);
  RenameFile(newname, Name);
  Refresh;
end;

Constructor TSubFile.CreateRead(const fn, name: String; apos, asize: longint);
begin
  Fname := fn + '>' + Name;
  F := OpenFileRead(fn, 0);
  offs := apos;
  size := asize;
  F.Fseek(offs);
end;

Function TSubFile.GetFullName: String;
begin
  Result := Fname;
end;

Function TSubFile.GetShortName: String;
begin
  Result := ExtractName(Fname);
end;

Function TSubFile.Fread(var buf; size: Integer): Integer;
begin
  Result := F.Fread(buf, size);
end;

Function TSubFile.Fsize: longint;
begin
  Result := size;
end;

Function TSubFile.Fpos: longint;
begin
  Result := F.Fpos - offs;
end;

Procedure TSubFile.Fseek(Pos: longint);
begin
  F.Fseek(offs + Pos);
end;

Destructor TSubFile.FClose;
begin
  F.FClose;
end;

Destructor TSubFile.Destroy;
begin
  F.FClose;
end;

Procedure TFileChangeControl.SetFile(name: String);
var
  sr: TSearchRec;
begin
  Fname := name;
  FindFirst(Fname, faAnyFile, sr);
  Ftime := sr.time;
  Fsize := sr.size;
  FindClose(sr);
end;

Function TFileChangeControl.Changed: boolean;
var
  sr: TSearchRec;
begin
  FindFirst(Fname, faAnyFile, sr);
  Result := (Ftime <> sr.time) or (Fsize <> sr.size);
  FindClose(sr);
end;

end.
