unit ListRes;

interface

uses Values, Classes;

type

  TCOGFile = class
    Vals: TCOgValues;
    Constructor Create;
    Destructor Destroy; override;
    Procedure Load(const fname: string);
    Procedure LoadNoLocals(const fname: string);
    Function GetCount: integer;
    Function GetItem(i: integer): TCOGValue;
    Property Count: integer read GetCount;
    Property Items[i: integer]: TCOGValue read GetItem; default;
  Private
    Procedure Clear;
    Procedure LoadFile(const fname: string; locals: boolean);
  end;

  TPUPFile = class
    Keys: TStringList;
    Constructor Create;
    Destructor Destroy; override;
    Procedure Load(const fname: string);
  end;

  TSNDFile = class
    wavs: TStringList;
    Constructor Create;
    Destructor Destroy; override;
    Procedure Load(const fname: string);
  end;

  T3DOFile = class
    mats: TStringList;
    Constructor Create;
    Destructor Destroy; override;
    Procedure Load(const fname: string);
  end;

  TITEMSDATFile = class
    cogs: TStringList;
    Constructor Create;
    Destructor Destroy; override;
    Procedure Load(const fname: string);
  end;

  TMODELSDATFile = class
    snds: TStringList;
    a3dos: TStringList;
    Constructor Create;
    Destructor Destroy; override;
    Procedure Load(const fname: string);
  end;

Function GetMatFromSPR(const fname: string): string;
Function GetMatFromPAR(const fname: string): string;

Procedure LoadJKLLists(const jklname: string; sl: TStringList);

implementation

uses Files, FileOperations, misc_utils, SysUtils;

Constructor TCOGFile.Create;
begin
  Vals := TCOgValues.Create;
end;

Procedure TCOGFile.Clear;
var
  i: integer;
begin
  For i := 0 to Vals.Count - 1 do
    Vals[i].Free;
  Vals.Clear;
end;

Destructor TCOGFile.Destroy;
begin
  Clear;
  Vals.Free;
end;

Procedure TCOGFile.LoadNoLocals(const fname: string);
begin
  LoadFile(fname, false);
end;

Procedure TCOGFile.Load(const fname: string);
begin
  LoadFile(fname, true);
end;

Procedure TCOGFile.LoadFile(const fname: string; locals: boolean);
var
  t: TTextFile;
  s, w, w1: string;
  p, pe: integer;
  v: TCOGValue;
begin

  for p := 0 to Vals.Count - 1 do
    Vals[p].Free;
  Vals.Clear;

  try
    t := TTextFile.CreateRead(OpenGameFile(fname));
  except
    On Exception do
      exit;
  end;

  Try

    Repeat
      t.Readln(s);
      GetWord(s, 1, w);
    until (t.eof) or (CompareText(w, 'symbols') = 0);

    if not t.eof then
      While not t.eof do
      begin

        t.Readln(s);
        RemoveComment(s);
        p := GetWord(s, 1, w);
        if CompareText(w, 'end') = 0 then
          break;

        if w <> '' then
        begin
          v := TCOGValue.Create;
          GetCOGVal(s, v);
          if not locals then
          begin
            if (v.local) or (v.cog_type = ct_msg) then
            begin
              v.Free;
              continue;
            end;
          end;
          Vals.Add(v);

        end;

      end;

  finally
    t.Fclose;
  end;
end;

Function TCOGFile.GetCount: integer;
begin
  Result := Vals.Count;
end;

Function TCOGFile.GetItem(i: integer): TCOGValue;
begin
  Result := TCOGValue(Vals[i]);
end;

Constructor TPUPFile.Create;
begin
  Keys := TStringList.Create;
end;

Destructor TPUPFile.Destroy;
begin
  Keys.Free;
end;

Procedure TPUPFile.Load(const fname: string);
var
  t: TTextFile;
  s, w: string;
  p: integer;
begin
  Keys.Clear;
  try
    t := TTextFile.CreateRead(OpenGameFile(fname));
  except
    On Exception do
      exit;
  end;

  Try
    While not t.eof do
    begin
      t.Readln(s);
      RemoveComment(s);
      p := GetWord(s, 1, w);
      if w <> '' then
      begin
        GetWord(s, p, w);
        if UpperCase(ExtractFileExt(w)) = '.KEY' then
          Keys.Add(w);
      end;
    end;
  finally
    t.Fclose;
  end;
end;

Constructor TSNDFile.Create;
begin
  wavs := TStringList.Create;
end;

Destructor TSNDFile.Destroy;
begin
  wavs.Free;
end;

Procedure TSNDFile.Load(const fname: string);
var
  t: TTextFile;
  s, w: string;
  p: integer;
begin
  wavs.Clear;
  try
    t := TTextFile.CreateRead(OpenGameFile(fname));
  except
    On Exception do
      exit;
  end;

  Try
    While not t.eof do
    begin
      t.Readln(s);
      RemoveComment(s);
      p := GetWord(s, 1, w);
      if w <> '' then
      begin
        GetWord(s, p, w);
        wavs.Add(w);
      end;
    end;
  finally
    t.Fclose;
  end;
end;

Function GetMatFromSPR(const fname: string): string;
var
  t: TTextFile;
  s, w: string;
begin
  Result := '';
  try
    t := TTextFile.CreateRead(OpenGameFile(fname));
  except
    On Exception do
      exit;
  end;

  Try
    While not t.eof do
    begin
      t.Readln(s);
      RemoveComment(s);
      GetWord(s, 1, w);
      if w <> '' then
      begin
        Result := w;
        exit;
      end;
    end;
  finally
    t.Fclose;
  end;
end;

Function GetMatFromPAR(const fname: string): string;
var
  t: TTextFile;
  s, w: string;
  p: integer;
begin
  Result := '';
  try
    t := TTextFile.CreateRead(OpenGameFile(fname));
  except
    On Exception do
      exit;
  end;

  Try
    While not t.eof do
    begin
      t.Readln(s);
      RemoveComment(s);
      p := GetWord(s, 1, w);
      if w = 'MATERIAL' then
      begin
        GetWord(s, p, Result);
        exit;
      end;
    end;
  finally
    t.Fclose;
  end;
end;

Constructor T3DOFile.Create;
begin
  mats := TStringList.Create;
end;

Destructor T3DOFile.Destroy;
begin
  mats.Free;
end;

Procedure T3DOFile.Load(const fname: string);
var
  t: TTextFile;
  s, w: string;
  i, n, p, nm: integer;
begin
  mats.Clear;
  try
    t := TTextFile.CreateRead(OpenGameFile(fname));
  except
    On Exception do
      exit;
  end;

  try

    Repeat
      t.Readln(s);
      p := GetWord(s, 1, w);
    until (t.eof) or (w = 'MATERIALS');
    if not t.eof then
    begin
      RemoveComment(s);

      GetWord(s, p, w);
      n := StrToInt(w);

      nm := 0;
      While (nm < n) or t.eof do
      begin
        t.Readln(s);
        RemoveComment(s);
        p := GetWord(s, 1, w);
        if w = '' then
          continue;
        GetWord(s, p, w);
        mats.Add(w);
        inc(nm);
      end;
    end;

  finally
    t.Fclose;
  end;

end;

Constructor TITEMSDATFile.Create;
begin
  cogs := TStringList.Create;
end;

Destructor TITEMSDATFile.Destroy;
begin
  cogs.Free;
end;

Procedure TITEMSDATFile.Load(const fname: string);
var
  t: TTextFile;
  s, w: string;
  i, n, p: integer;
begin
  cogs.Clear;
  try
    t := TTextFile.CreateRead(OpenGameFile(fname));
  except
    On Exception do
      exit;
  end;

  try
    While not t.eof do
    begin
      t.Readln(s);
      RemoveComment(s);
      p := pos('cog=', s);
      if p <> 0 then
      begin
        GetWord(s, p + 4, w);
        if w <> '' then
          cogs.Add(w);
      end;
    end;

  finally
    t.Fclose;
  end;

end;

Constructor TMODELSDATFile.Create;
begin
  snds := TStringList.Create;
  a3dos := TStringList.Create;
end;

Destructor TMODELSDATFile.Destroy;
begin
  snds.Free;
  a3dos.Free;
end;

Procedure TMODELSDATFile.Load(const fname: string);
var
  t: TTextFile;
  s, w: string;
  i, n, p: integer;
begin
  snds.Clear;
  try
    t := TTextFile.CreateRead(OpenGameFile(fname));
  except
    On Exception do
      exit;
  end;

  try
    While not t.eof do
    begin
      t.Readln(s);
      RemoveComment(s);
      p := GetWord(s, 1, w);
      p := GetWord(s, p, w);
      if w <> '' then
        a3dos.Add(w);

      p := GetWord(s, p, w);
      if w <> '' then
        snds.Add(w);
    end;

  finally
    t.Fclose;
  end;

end;

Procedure LoadJKLLists(const jklname: string; sl: TStringList);
var
  EndOfSection: boolean;
  CurSection: string;
  t: TTextFile;
  s: string;

  Procedure GetNextLine(var s: String);
  var
    cmt_pos: word; { Position of # }
  begin
    s := '';
    Repeat
      if t.eof then
      begin
        EndOfSection := true;
        exit;
      end;
      t.Readln(s);
      cmt_pos := pos('#', s);
      if cmt_pos <> 0 then
        SetLength(s, cmt_pos - 1);
      s := UpperCase(Trim(s));
    Until s <> '';
    if s = 'END' then
    begin
      CurSection := '';
      EndOfSection := true;
    end;
    if GetWordN(s, 1) = 'SECTION:' then
    begin
      CurSection := GetWordN(s, 2);
      EndOfSection := true;
    end;
  end; { GetNextLine }

  Procedure SkipToNextSection;
  begin
    While not EndOfSection do
      GetNextLine(s);
  end;

  Procedure LoadSounds;
  begin
    CurSection := '';
    GetNextLine(s);
    While (not EndOfSection) do
    begin
      GetNextLine(s);
      sl.Add(s);
    end;
  end;

  Procedure LoadListSec;
  begin
    CurSection := '';
    GetNextLine(s);
    While (not EndOfSection) do
    begin
      GetNextLine(s);
      sl.Add(GetWordN(s, 2));
    end;
  end;

begin { LoadJKLLists }

  Try

    t := TTextFile.CreateRead(OpenFileRead(jklname, 0));

    Try

      Repeat
        While (CurSection = '') and (not t.eof) do
          GetNextLine(s);
        if t.eof then
          break;
        EndOfSection := false;
        if CurSection = 'SOUNDS' then
          LoadSounds
        else if CurSection = 'MATERIALS' then
          LoadListSec
        else if CurSection = 'MODELS' then
          LoadListSec
        else if CurSection = 'MATERIALS' then
          LoadListSec
        else if CurSection = 'AICLASS' then
          LoadListSec
        else if CurSection = 'SPRITES' then
          LoadListSec
        else if CurSection = 'KEYFRAMES' then
          LoadListSec
        else if CurSection = 'ANIMCLASS' then
          LoadListSec
        else if CurSection = 'SOUNDCLASS' then
          LoadListSec
        else if CurSection = 'COGSCRIPTS' then
          LoadListSec
        else
          SkipToNextSection;
      until t.eof;

    Finally
      t.Fclose;
    end;

  Except
    on Exception do;
  end;

end;

end.
