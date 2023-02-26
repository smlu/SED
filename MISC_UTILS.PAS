unit misc_utils;

interface

uses Classes, SysUtils, GlobalVars, Forms, U_MsgForm, StdCtrls, Graphics;

{ Miscellaneous help routines }

Const
  CloseEnough = 10E-5;

Type

  TMsgType = (mt_info, mt_warning, mt_error); { Message Types for PanMessage() }

  TIntList = class(TList)
    Function GetInt(n: Integer): Integer;
    Procedure SetInt(n: Integer; I: Integer);
    Function Add(I: Integer): Integer;
    Procedure Insert(n: Integer; I: Integer);
    Function IndexOf(I: Integer): Integer;
    Property Ints[n: Integer]: Integer read GetInt write SetInt; default;
  end;

  TDoubleList = class(TList)
    Function GetD(n: Integer): Double;
    Procedure SetD(n: Integer; v: Double);
    Function Add(v: Double): Integer;
    Property Ints[n: Integer]: Double read GetD write SetD; default;
    Destructor Destroy; override;
  end;

  TSingleList = class(TList)
    Function GetV(n: Integer): Single;
    Procedure SetV(n: Integer; v: Single);
    Function Add(v: Single): Integer;
    Property Items[n: Integer]: Single read GetV write SetV; default;
  end;

  TObjList = class(TList)
    Procedure AddObject(obj: TObject);
    Function FindObject(obj: TObject): Integer;
    Procedure DeleteAndFree(obj: TObject);
    Destructor FreeAll;
    Function GetObj(n: Integer): TObject;
    Procedure SetObj(n: Integer; obj: TObject);
    Property Objs[n: Integer]: TObject read GetObj write SetObj; default;
  end;

Function MsgBox(Txt, Caption: String; flags: Integer): Integer;
Function GetWord(const s: string; p: Integer; var w: string): Integer;
Function GetWordN(const s: String; n: Integer): String;
Function PGetWord(ps: Pchar; pw: Pchar): Pchar;
{ Same thing, but for PChar strings }
Function StrToDouble(const s: String): Double;
Function StrToLongInt(const s: String): Longint;
Function HexToDword(const s: String): Longint;
Function StrToDword(const s: String): Longint;
Function DwordToStr(d: Longint): String;
Function PScanf(ps: Pchar; const format: String;
  const Vals: array of pointer): boolean;
Function SScanf(const s: string; const format: String;
  const Vals: array of pointer): boolean;

{ Lightweight version of Format(). formats floats differently -
  accepts %f and %.#f and puts max of fractional # digits in the string }
Function SPrintf(const format: string; const Vals: array of const): string;

Procedure PanMessage(mt: TMsgType; const msg: String);
Procedure PanMessageFmt(mt: TMsgType; const fmt: String;
  const v: array of const);
Function Real_Min(a, b: Double): Double;
Function Real_Max(a, b: Double): Double;
Function JKRound(d: Double): Double; { Rounds to 6 decimal points }
Function UVRound(d: Single): Single; { Round to 2 decimal digits }

Function DoubleToStr(d: Double): String;

{ handles the number -2147483648 ($80000000) properly.
  for some reason, StrToInt gets confused by it }

Function ValInt(const s: String; var I: Integer): boolean;
Function ValHex(const s: String; var h: Longint): boolean;
Function ValDword(const s: String; var d: Longint): boolean;
Function ValDouble(const s: String; var d: Double): boolean;
Function ValSingle(const s: String; var f: Single): boolean;
Function ValVector(const s: string; var x, y, z: Double): boolean;

Function PadRight(const s: string; tolen: Integer): String;
Procedure RemoveComment(var s: String);

Function GetMSecs: Longint;
Function SubMSecs(startms, endms: Longint): Longint;
Function StrMSecs(ms: Longint): string;

Procedure SizeFromToFit(f: TForm);
Function ScanForSpace(const s: string; p: Integer): Integer;
{ if p is negative - scan backwards }
Function ScanForNonSpace(const s: string; p: Integer): Integer;
{ if p is negative - scan backwards }

Function IsClose(d1, d2: Double): boolean;

Function FindJedDataFile(const Name: string): String;

Function SetFlags(val, flags: Integer): Integer;
Function ClearFlags(val, flags: Integer): Integer;

Procedure SetEditText(ed: TEdit; const s: string); // Doesn't invoke OnChange

implementation

uses Windows, Jed_main;

Function TIntList.GetInt(n: Integer): Integer;
begin
  Result := Integer(Items[n]);
end;

Procedure TIntList.SetInt(n: Integer; I: Integer);
begin
  Items[n] := pointer(I);
end;

Procedure TIntList.Insert(n: Integer; I: Integer);
begin
  Inherited Insert(n, pointer(I));
end;

Function TIntList.Add(I: Integer): Integer;
begin
  Result := Inherited Add(pointer(I));
end;

Function TIntList.IndexOf(I: Integer): Integer;
begin
  Result := inherited IndexOf(pointer(I));
end;

Function TSingleList.GetV(n: Integer): Single;
begin
  if (n < 0) or (n >= Count) then
    Result := 0
  else
    Result := Single(List[n]);
end;

Procedure TSingleList.SetV(n: Integer; v: Single);
begin
  List[n] := pointer(v);
end;

Function TSingleList.Add(v: Single): Integer;
begin
  Result := Inherited Add(pointer(v));
end;

Function TDoubleList.GetD(n: Integer): Double;
begin
  Result := Double(Items[n]^);
end;

Procedure TDoubleList.SetD(n: Integer; v: Double);
begin
  Double(Items[n]^) := v;
end;

Function TDoubleList.Add(v: Double): Integer;
var
  pv: pointer;
begin
  GetMem(pv, sizeof(Double));
  Double(pv^) := v;
  Inherited Add(pv);
end;

Destructor TDoubleList.Destroy;
var
  I: Integer;
begin
  for I := 0 to Count - 1 do
    FreeMem(Items[I], sizeof(Double));
  inherited Destroy;
end;

Procedure TObjList.AddObject(obj: TObject);
begin
  Add(obj);
end;

Function TObjList.FindObject(obj: TObject): Integer;
begin
  Result := IndexOf(obj);
end;

Procedure TObjList.DeleteAndFree(obj: TObject);
var
  I: Integer;
begin
  I := FindObject(obj);
  if I <> -1 then
    Delete(I);
  obj.Destroy;
end;

Destructor TObjList.FreeAll;
var
  I: Integer;
begin
  for I := 0 to Count - 1 do
    Objs[I].Destroy;
  Clear;
  Destroy;
end;

Function TObjList.GetObj(n: Integer): TObject;
begin
  Result := TObject(Items[n]);
end;

Procedure TObjList.SetObj(n: Integer; obj: TObject);
begin
  Items[n] := obj;
end;

Function PadRight(const s: string; tolen: Integer): String;
var
  I, len: Integer;
begin
  Result := s;
  len := length(Result);
  if len < tolen then
    SetLength(Result, tolen);
  For I := len + 1 to tolen do
    Result[I] := ' ';
end;

Function ScanForSpace(const s: string; p: Integer): Integer;
{ if p is negative - scan backwards }
begin
  if p < 0 then { backwards }
  begin
    p := -p;
    while (p > 1) and not(s[p] in [' ', #9]) do
      dec(p);
    Result := p;
  end
  else
  begin
    While (p <= length(s)) and not(s[p] in [' ', #9]) do
      inc(p);
    Result := p;
  end;
end;

Function ScanForNonSpace(const s: string; p: Integer): Integer;
{ if p is negative - scan backwards }
begin
  if p < 0 then { backwards }
  begin
    p := -p;
    while (p > 1) and (s[p] in [' ', #9]) do
      dec(p);
    Result := p;
  end
  else
  begin
    While (p <= length(s)) and (s[p] in [' ', #9]) do
      inc(p);
    Result := p;
  end;
end;

Function GetWordN(const s: String; n: Integer): String;
var
  I, p: Integer;
begin
  p := 1;
  for I := 1 to n do
  begin
    p := ScanForNonSpace(s, p);
    if I <> n then
      p := ScanForSpace(s, p);
  end;
  I := ScanForSpace(s, p);
  Result := UpperCase(Copy(s, p, I - p));
end;

Procedure RemoveComment(var s: String);
var
  p: Integer;
begin
  p := Pos('#', s);
  if p <> 0 then
    SetLength(s, p - 1);
end;

Function Real_Min(a, b: Double): Double;
begin
  if a > b then
    Result := b
  else
    Result := a;
end;

Function Real_Max(a, b: Double): Double;
begin
  if a > b then
    Result := a
  else
    Result := b;
end;

Function JKRound(d: Double): Double;
begin
  Result := Round(d * 10000) / 10000;
end;

Function UVRound(d: Single): Single;
begin
  Result := Round(d * 1000) / 1000;
end;

Function StrToDouble(const s: String): Double;
var
  a: Integer;
begin
  if s = '' then
  begin
    Result := 0;
    exit;
  end;
  val(s, Result, a);
  if a <> 0 then
    raise EConvertError.Create('Invalid number: ' + s);
end;

Function StrToLongInt(const s: String): Longint;
var
  a: Integer;
begin
  val(s, Result, a);
end;

Function HexToDword(const s: String): Longint;
var
  a: Integer;
begin
  if s = '' then
  begin
    Result := 0;
    exit;
  end;
  val('$' + s, Result, a);
end;

Function ValInt(const s: String; var I: Integer): boolean;
var
  a: Integer;
begin
  Result := true;
  if s = '' then
  begin
    I := 0;
    exit;
  end;
  val(s, I, a);
  Result := a = 0;
end;

Function ValHex(const s: String; var h: Longint): boolean;
var
  a: Integer;
  s1: string;
begin
  Result := true;
  if s = '' then
  begin
    h := 0;
    exit;
  end;
  if (length(s) > 2) and (s[2] in ['X', 'x']) then
  begin
    s1 := '$' + Copy(s, 3, length(s) - 2);
    val(s1, h, a);
    Result := a = 0;
    exit;
  end;
  val('$' + s, h, a);
  Result := a = 0;
end;

Function ValDword(const s: String; var d: Longint): boolean;
var
  a: Integer;
begin
  Result := true;
  if s = '' then
  begin
    d := 0;
    exit;
  end;
  val(s, d, a);
  if a = 10 then
    if s[10] in ['0' .. '9'] then
    begin
      d := d * 10 + (ord(s[10]) - ord('0'));
      a := 0;
    end;
  Result := not((a <> 0) and (s[a] <> #0));
end;

Function ValSingle(const s: String; var f: Single): boolean;
var
  a: Integer;
begin
  Result := true;
  if s = '' then
  begin
    f := 0;
    exit;
  end;
  val(s, f, a);
  Result := a = 0;
end;

Function ValDouble(const s: String; var d: Double): boolean;
var
  a: Integer;
begin
  Result := true;
  if s = '' then
  begin
    d := 0;
    exit;
  end;
  val(s, d, a);
  Result := a = 0;
end;

Function ValVector(const s: string; var x, y, z: Double): boolean;
var
  w: string;
  p, a: Integer;
begin
  p := GetWord(s, 1, w);
  val(w, x, a);
  Result := a = 0;
  p := GetWord(s, p, w);
  val(w, y, a);
  Result := Result and (a = 0);
  p := GetWord(s, p, w);
  val(w, z, a);
  Result := Result and (a = 0);
end;

Function StrToDword(const s: String): Longint;
var
  a: Integer;
begin
  if s = '' then
  begin
    Result := 0;
    exit;
  end;
  val(s, Result, a);
  if a = 10 then
    if s[10] in ['0' .. '9'] then
    begin
      Result := Result * 10 + (ord(s[10]) - ord('0'));
      a := 0;
    end;
  if (a <> 0) and (s[a] <> #0) then
    Raise EConvertError.Create(s + ' is not a valid number');
end;

Function DwordToStr(d: Longint): String;
var
  c: AnsiChar;
  a: Integer;
begin
  if d >= 0 then
    Str(d, Result)
  else { 32th bit set }
  begin
    asm { Divides D by 10 treating it as unsigned integer }
      Mov eax,d
      xor edx,edx
      mov ecx,10
      Div ecx
      add dl,'0'
      mov c,dl
      mov d,eax
    end;
    Str(d, Result);
    Result := Result + c;
  end
end;

Function MsgBox(Txt, Caption: String; flags: Integer): Integer;
begin
  Result := Application.MessageBox(Pchar(Txt), Pchar(Caption), flags);
end;

Function GetWord(const s: string; p: Integer; var w: string): Integer;
var
  b, e: Integer;
begin
  if s = '' then
  begin
    w := '';
    Result := 1;
    exit;
  end;
  b := p;
  While (b <= length(s)) and (s[b] in [' ', #9])do
    inc(b);
  e := b;
  While (e <= length(s)) and (not(s[e] in [' ', #9])) do
    inc(e);
  w := Copy(s, b, e - b);
  GetWord := e;
end;

Function PGetWord(ps: Pchar; pw: Pchar): Pchar;
var
  pb, pe: Pchar;
begin
  if ps^ = #0 then
  begin
    pw^ := #0;
    Result := ps;
    exit;
  end;
  pb := ps;
  while pb^ in [' ', #9] do
    inc(pb);
  pe := pb;
  while not(pe^ in [' ', #9, #0]) do
    inc(pe);
  StrLCopy(pw, pb, pe - pb);
  Result := pe;
end;

{ Accepted formatters - %s %d %x %.1f - %.6f }
Function SPrintf(const format: string; const Vals: array of const): string;
var
  pres, pf, pe, pp, pa: Pchar;
  cv, I, ndig: Integer;
  pv: ^TVarRec;
  buf: array [0 .. 4095] of Char;
begin
  Result := '';
  FillChar(buf, 4096, 0);
  pf := @format[1];
  pres := @buf;
  cv := 0;
  pa := @buf;
  repeat
    pp := StrScan(pf, '%');
    if pp = nil then
      pe := strEnd(pf)
    else
      pe := pp;
    StrLCopy(pres, pf, pe - pf);
    pres := strEnd(pres);
    if pp = nil then
      break;
    inc(pp);

    if cv > high(Vals) then
      raise EConvertError.Create('Not enough parameters');
    pv := @Vals[cv];

    case pp^ of
      's', 'S':
        begin
          if pv^.vtype <> vtUnicodeString then
            raise EConvertError.Create('Invalid parameter type');
          if pv^.vUnicodeString <> nil then
            pres := StrECopy(pres, Pchar(pv^.vUnicodeString));
        end;
      'd', 'D':
        begin
          if pv^.vtype <> vtInteger then
            raise EConvertError.Create('Invalid parameter type');
          pres := StrECopy(pres, Pchar(IntToStr(pv^.vInteger)));
        end;
      'x', 'X':
        begin
          if pv^.vtype <> vtInteger then
            raise EConvertError.Create('Invalid parameter type');
          pres := StrECopy(pres, Pchar(IntToHex(pv^.vInteger, 1)));
        end;
      'f', 'F', '.':
        begin
          if pp^ = '.' then
          begin
            inc(pp);
            ndig := ord(pp^) - ord('0');
            inc(pp);
          end
          else
            ndig := 2;
          if pv^.vtype <> vtExtended then
            raise EConvertError.Create('Invalid parameter type');

          case ndig of
            1:
              I := FloatToTextFmt(pres, pv.vExtended^, fvExtended, '.#');
            2:
              I := FloatToTextFmt(pres, pv.vExtended^, fvExtended, '.##');
            3:
              I := FloatToTextFmt(pres, pv.vExtended^, fvExtended, '.###');
            4:
              I := FloatToTextFmt(pres, pv.vExtended^, fvExtended, '.####');
            5:
              I := FloatToTextFmt(pres, pv.vExtended^, fvExtended, '.#####');
            6:
              I := FloatToTextFmt(pres, pv.vExtended^, fvExtended, '.######');
          else
            I := FloatToTextFmt(pres, pv.vExtended^, fvExtended, '.##');
          end;
          if I = 0 then
          begin
            pres^ := '0';
            inc(pres);
          end
          else
            inc(pres, I);
        end;
    else
      if I = 0 then;
    end;
    if pp^ <> #0 then
      inc(pp);
    pf := pp;
    pres := strEnd(pres);
    inc(cv);
  until false;
  Result := buf;
  if pa = nil then;
end;

Function PScanf(ps: Pchar; const format: String;
  const Vals: array of pointer): boolean;
var
  pp, { position of % in format string }
  pb, { beginning of the prefix }
  pv, { position of the value }
  pe, { end of the prefix }
  pf: Pchar;
  tmp: array [0 .. 99] of Char;
  ptmp: Pchar;
  len: Integer; { Lenth of prefix string }
  a, { Dummy variable for Val() }
  nval: Integer; { Index in vals[] array }
  lastdigit, dw: Dword;
  c: Char;
begin
{$R-}
  Result := true;
  nval := 0;
  pf := Pchar(format);
  Repeat
    if ps^ = #0 then
      break;
    pp := StrScan(pf, '%');
    if pp = nil then
      break;
    pb := pf;
    while (pb^ in [' ', #9]) and (pb < pp) do
      inc(pb);

    if pp = pb then
    begin
      len := 0;
      pv := ps;
    end
    else
    begin
      pe := pp - 1;
      while (pe^ in [' ', #9]) and (pe > pb) do
        dec(pe);
      len := pe - pb + 1;
      StrLCopy(tmp, pb, len);
      pv := StrPos(ps, tmp);
      if pv = nil then
      begin
        pf := pp + 1;
        if pf^ <> #0 then
          inc(pf);
        inc(nval);
        continue;
      end;
      pv := pv + len;
    end;

    ptmp := @tmp[1];
    ps := PGetWord(pv, ptmp);
    inc(pp);
    a := 0;
    case pp^ of { format specifier }
      'd', 'D':
        val(ptmp, Integer(Vals[nval]^), a);
      'u', 'U':
        begin
          val(ptmp, dw, a);
          if a <> 0 then
          begin
            c := (ptmp + a - 1)^;
            if c in ['0' .. '9'] then
            begin
              lastdigit := ord(c) - ord('0');
              dw := dw * 10 + lastdigit;
              a := 0;
            end;
          end;
          Dword(Vals[nval]^) := dw;
        end;
      'f', 'F':
        val(ptmp, Double(Vals[nval]^), a);
      'l', 'L':
        val(ptmp, Single(Vals[nval]^), a);
      'b', 'B':
        val(ptmp, byte(Vals[nval]^), a);
      'c', 'C':
        Char(Vals[nval]^) := ptmp^;
      's', 'S':
        String(Vals[nval]^) := ptmp;
      'x', 'X':
        begin
          if tmp[2] in ['x', 'X'] then
          begin
            ptmp := @tmp[2];
            tmp[2] := '$';
          end
          else
          begin
            ptmp := @tmp;
            tmp[0] := '$';
          end;
          val(ptmp, Integer(Vals[nval]^), a);
        end;
    else
      a := 1;
    end;
    if a <> 0 then
      Result := false;
    pf := pp;
    if pf^ <> #0 then
      inc(pf);
    inc(nval);
  until false;
{$R+}
end;

Function SScanf(const s: string; const format: String;
  const Vals: array of pointer): boolean;
begin
  Result := PScanf(Pchar(s), format, Vals);
end;

Procedure PanMessageFmt(mt: TMsgType; const fmt: String;
  const v: array of const);
begin
  PanMessage(mt, format(fmt, v));
end;

Procedure PanMessage(mt: TMsgType; const msg: String);
begin
  case mt of
    mt_info:
      begin
        if sm_ShowInfo then
          MsgForm.AddMessage('Info: ' + msg);
        JedMain.Pmsg.Caption := msg;
        JedMain.Pmsg.Font.Color := clBlack;
      end;
    mt_warning:
      begin
        if sm_ShowWarnings then
          MsgForm.AddMessage('Warning: ' + msg);
        JedMain.Pmsg.Caption := msg;
        JedMain.Pmsg.Font.Color := clRed;
        MsgForm.Show;
      end;
    mt_error:
      begin
        MsgBox(msg, 'Error', mb_OK);
        MsgForm.AddMessage('Error: ' + msg);
      end;
  end;
end;

Function StrToDoubleDef(const s: String; def: Double): Double;
var
  a: Integer;
begin
  val(s, Result, a);
  if a <> 0 then
    Result := def;
end;

{ Procedure GetFormPos(f:TForm;var wpos:TWinPos);
  begin
  With WPos,f do
  begin
  Wwidth:=width;
  Wheight:=height;
  Wtop:=top;
  Wleft:=left;
  end;
  end;

  Procedure SetFormPos(f:TForm;const wpos:TWinPos);
  begin
  if WPos.WWidth=0 then exit;
  With WPos do
  F.SetBounds(Wleft,Wtop,Wwidth,Wheight);
  end; }

Procedure SizeFromToFit(f: TForm);
var
  xmin, xmax, ymin, ymax: Integer;
begin
end;

Function IsClose(d1, d2: Double): boolean;
begin
  Result := Abs(d2 - d1) < CloseEnough;
end;

Const
  MsPerDay = 24 * 60 * 60 * 1000;
  MsPerHour = 60 * 60 * 1000;
  MsPerMin = 60 * 1000;

Function GetMSecs: Longint;
var
  h, m, s, ms: word;
begin
  DecodeTime(Now, h, m, s, ms);
  Result := h * MsPerHour + m * MsPerMin + s * 1000 + ms;
end;

Function SubMSecs(startms, endms: Longint): Longint;
begin
  if endms >= startms then
    Result := endms - startms
  else
    Result := endms + MsPerDay - startms;
end;

Function StrMSecs(ms: Longint): string;
Var
  rem: Longint;
begin
  Result := '';
  rem := ms mod MsPerHour;
  if ms > rem then
    Result := format('%d hours ', [ms div MsPerHour]);
  ms := rem;
  rem := ms mod MsPerMin;
  if ms > rem then
    Result := format('%s%d mins ', [Result, ms div MsPerMin]);
  ms := rem;
  rem := ms mod 1000;
  if ms > rem then
    Result := format('%s%d secs ', [Result, ms div 1000]);
  Result := format('%s%d ms', [Result, rem]);
end;

Function DoubleToStr(d: Double): String;
var
  I: Integer;
begin
  Result := FormatFloat('.######', d);
  if Result = '' then
    Result := '0';
  { Format('%.6f',[d]);
    For i:=length(Result) downto 1 do
    begin
    if Result[i]='0' then SetLength(Result,Length(Result)-1)
    else if Result[i]='.' then begin SetLength(Result,Length(Result)-1); break; end
    else break;
    end; }
end;

Function FindJedDataFile(const Name: string): String;
begin
  if ProjectDir <> '' then
  begin
    Result := ProjectDir + Name;
    if FileExists(Result) then
      exit;
  end;
  Result := BaseDir + 'JedDATA\' + Name;
end;

Function SetFlags(val, flags: Integer): Integer;
begin
  Result := val or flags;
end;

Function ClearFlags(val, flags: Integer): Integer;
begin
  Result := val and (not flags);
end;

Procedure SetEditText(ed: TEdit; const s: string); // Doesn't invoke OnChange
var
  a: TNotifyEvent;
begin
  a := ed.OnChange;
  ed.OnChange := nil;
  ed.Text := s;
  ed.OnChange := a;
end;

Initialization

end.
