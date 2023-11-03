unit misc_utils;

interface

uses Classes, SysUtils, GlobalVars, Forms, U_MsgForm, StdCtrls, Graphics,TypInfo;

{ Miscellaneous help routines }

Const
  CloseEnough = 10E-5;

Type

  TMsgType = (mt_info, mt_warning, mt_error); { Message Types for PanMessage() }
  TColorF = record
    r, g, b, a: single;  // a - alpha or light intensity

    class operator Add(a, b: TColorF): TColorF;
    class operator Add(a: TColorF; b: single): TColorF;
    class operator Subtract(a, b: TColorF): TColorF;
    class operator Subtract(a: TColorF; b: single): TColorF;
    class operator Multiply(a, b: TColorF): TColorF;
    class operator Multiply(a: TColorF; b: single): TColorF;
    class operator Divide(a, b: TColorF): TColorF;
    class operator Divide(a: TColorF; b: single): TColorF;
    class operator Equal(a, b: TColorF): Boolean;
    class operator NotEqual(a, b: TColorF): Boolean;
    Procedure Clamp; overload;
    Procedure Clamp(min, max: TColorF); overload;
    Procedure SetToZero;
    Function IsBrighter(c: TColorF): Boolean;
    Function ToJedColor: TJedColor;
    Function IsZero: Boolean;
  end;
  PColorF=^TColorF;

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

  TNotifyEventWrapper = class(TComponent)
  private
    FProc: TProc<TObject>;
  public
    constructor Create(Owner: TComponent; Proc: TProc<TObject>);
  published
    procedure Event(Sender: TObject);
  end;

function LambdaNotifyEvent(Owner: TComponent; Proc: TProc<TObject>): TNotifyEvent;

Function BitTest(flags, mask: LongInt): Boolean;
Function FlagsNotSet(flags, mask: LongInt): Boolean;
Procedure BitSet(var val: Integer; flags: Integer);
Procedure BitClear(var val: Integer; flags: Integer);

Function MakeColor(r, g, b: single; a: single = 1.0): TColorF; overload;
Function MakeColor(c: TJedColor): TColorF; overload;
Function EncodeARGB(color: TColorF): UInt32;
Function EncodeABGR(color: TColorF): UInt32;
Function DecodeABGR(abgr: UInt32): TColorF;
Function EncodeBGR(color: TColorF): UInt32;
Function DecodeBGR(bgr: UInt32): TColorF;
Function IntensityToColor(i: single): TColorF;
Function RgbToIntensity(rgb: TColorF): single;
Function RgbaToIntensity(rgba: TColorF): single;
Function L2B(c:single):byte;

Function ClampD(v: Double; min: Double; max: Double): Double;
Function ClampF(v: Single; min: Single; max: Single): single;
Function ClampF_01(v: Single): single;
Function ClampI(v: Integer; min: Integer; max: Integer): Integer;

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

Function StripSScan(s: string; const format: String;
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

{ Formats TColorF object to string
  if bFormat = true then returned color string is in format (r/g/b[/a])
  else r g b [a] - when withAlpha = true }
Function ColorToStr(c: TColorF; withAlpha: Boolean = true; bFormat: Boolean = false): String;

{ handles the number -2147483648 ($80000000) properly.
  for some reason, StrToInt gets confused by it }

Function ValInt(const s: String; var I: Integer): boolean;
Function ValHex(const s: String; var h: Longint): boolean;
Function ValDword(const s: String; var d: Longint): boolean;
Function ValDouble(const s: String; var d: Double): boolean;
Function ValSingle(const s: String; var f: Single): boolean;
Function ValVector(const s: string; var x, y, z: Double): boolean;

{ Parses color from string in formats: (r/g/b), (r/g/b/a), 'r g b', 'r g b a' }
Function ValColor(const s: string; var color: TColorF): boolean;

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

Function FindDataFile(const Name: string): String;

Procedure SetEditText(ed: TEdit; const s: string); // Doesn't invoke OnChange

Function StringListFind(const list: TStringList; const str: string; var index: Integer): Boolean;

// Removes all entries in list that matches filter list
Procedure StringListFilter(list: TStringList; const filter: TStringList);


const
  ColorBlack: TColorF = (r:0;g:0;b:0;a:1);
  ColorBlue : TColorF  = (r:0;g:0;b:1;a:1);
  ColorGreen : TColorF  = (r:0;g:1;b:0;a:1);
  ColorCyan : TColorF  = (r:0;g:1;b:1;a:1);
  ColorRed : TColorF  = (r:1;g:0;b:0;a:1);
  ColorRedDark : TColorF  = (r:0.54;g:0;b:0;a:1);
  ColorRedScarlet : TColorF  = (r:1;g:0.14;b:0;a:1);
  ColorWhite: TColorF = (r:1;g:1;b:1;a:1);
  ColorYellow: TColorF = (r:1;g:1;b:0;a:1);

implementation

uses Windows, Jed_main, Math, System.Character;

class operator TColorF.Add(a, b: TColorF): TColorF;
begin
  Result.r := a.r + b.r;
  Result.g := a.g + b.g;
  Result.b := a.b + b.b;
  Result.a := a.a + b.a;
end;

class operator TColorF.Add(a: TColorF; b: single): TColorF;
begin
  Result.r := a.r + b;
  Result.g := a.g + b;
  Result.b := a.b + b;
  Result.a := a.a + b;
end;

class operator TColorF.Subtract(a, b: TColorF): TColorF;
begin
  Result.r := a.r - b.r;
  Result.g := a.g - b.g;
  Result.b := a.b - b.b;
  Result.a := a.a - b.a;
end;

class operator TColorF.Subtract(a: TColorF; b: single): TColorF;
begin
  Result.r := a.r - b;
  Result.g := a.g - b;
  Result.b := a.b - b;
  Result.a := a.a - b;
end;

class operator TColorF.Multiply(a, b: TColorF): TColorF;
begin
  Result.r := a.r * b.r;
  Result.g := a.g * b.g;
  Result.b := a.b * b.b;
  Result.a := a.a * b.a;
end;

class operator TColorF.Multiply(a: TColorF; b: single): TColorF;
begin
  Result.r := a.r * b;
  Result.g := a.g * b;
  Result.b := a.b * b;
  Result.a := a.a * b;
end;

class operator TColorF.Divide(a, b: TColorF): TColorF;
begin
  Result.r := a.r / b.r;
  Result.g := a.g / b.g;
  Result.b := a.b / b.b;
  Result.a := a.a / b.a;
end;

class operator TColorF.Divide(a: TColorF; b: single): TColorF;
begin
  Result.r := a.r / b;
  Result.g := a.g / b;
  Result.b := a.b / b;
  Result.a := a.a / b;
end;

class operator TColorF.Equal(a, b: TColorF): Boolean;
begin
  Result := (a.r = b.r) and (a.g = b.g) and (a.b = b.b) and (a.a = b.a);
end;

class operator TColorF.NotEqual(a, b: TColorF): Boolean;
begin
  Result := not (a = b);
end;

Procedure TColorF.Clamp;
begin
    r := ClampF_01(r);
    g := ClampF_01(g);
    b := ClampF_01(b);
    a := ClampF_01(a);
end;

Procedure TColorF.Clamp(min, max: TColorF);
begin
  r := ClampF(r, min.r, max.r);
  g := ClampF(g, min.g, max.g);
  b := ClampF(b, min.b, max.b);
  a := ClampF(a, min.a, max.a);
end;

Procedure TColorF.SetToZero;
begin
  r := 0.0;
  g := 0.0;
  b := 0.0;
  a := 0.0;
end;

Function TColorF.IsBrighter(c: TColorF): Boolean;
 var aa, ba: single;
begin
  aa := (r + g + b + a) / 4.0;
  ba := (c.r + c.g + c.b + c.a) / 4.0;
  Result := (aa > ba);
end;

 Function TColorF.ToJedColor: TJedColor;
 begin
    Result.r := ClampI(Round(r * 255), 0, 255);
    Result.g := ClampI(Round(g * 255), 0, 255);
    Result.b := ClampI(Round(b * 255), 0, 255);
 end;

 Function TColorF.IsZero: Boolean;
begin
  Result := Math.IsZero(r) and Math.IsZero(g) and Math.IsZero(b) and Math.IsZero(a);
end;

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


constructor TNotifyEventWrapper.Create(Owner: TComponent; Proc: TProc<TObject>);
begin
  inherited Create(Owner);
  FProc := Proc;
end;

procedure TNotifyEventWrapper.Event(Sender: TObject);
begin
  FProc(Sender);
end;

function LambdaNotifyEvent(Owner: TComponent; Proc: TProc<TObject>): TNotifyEvent;
begin
  Result := TNotifyEventWrapper.Create(Owner, Proc).Event;
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

Function ValColor(const s: string; var color: TColorF): boolean;
var
  w: string;
  p, a: Integer;
begin
  // First try to parse color formats (r/g/b/a), (r/g/b), 'r g b a'...
  Result := True;
  color.a := 0.0;
  if StripSScan(s,'%l %l %l %l',[@color.r, @color.g, @color.b, @color.a]) or
     StripSScan(s,'%l %l %l',[@color.r, @color.g, @color.b]) then
    exit;

  // Fallback to try parsing 'r g b a' format
  p := GetWord(s, 1, w);
  Val(w, color.r, a);
  Result := a = 0;
  p := GetWord(s, p, w);
  Val(w, color.g, a);
  Result := Result and (a = 0);
  p := GetWord(s, p, w);
  Val(w, color.b, a);

  // Try to get alpha from the string
  p := GetWord(s, p, w);
  Val(w, color.a, a);
  if a <> 0 then color.a := 0.0;
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

Function BitTest(flags, mask: LongInt): Boolean;
begin;
  Result := (flags and mask) = mask;
end;

Function FlagsNotSet(flags, mask: LongInt): Boolean;
begin;
  Result := (flags and mask) = 0;
end;

Procedure BitSet(var val: Integer; flags: Integer);
begin
  val := val or flags;
end;

Procedure BitClear(var val: Integer; flags: Integer);
begin
  val := val and (not flags);
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

Function MakeColor(r, g, b: single; a: single = 1.0): TColorF;
begin
  Result.r := ClampF_01(r);
  Result.g := ClampF_01(g);
  Result.b := ClampF_01(b);
  Result.a := ClampF_01(a);
end;

Function MakeColor(c: TJedColor): TColorF;
begin
  Result := MakeColor(c.r /255, c.g /255, c.b /255);
end;

Function EncodeARGB(color: TColorF): UInt32;
var
  br, bg, bb, ba: byte;
begin
  // Big-endian encoded RGBA 32 bpp
  br := ClampI(Round(color.r * 255), 0, 255);
  bg := ClampI(Round(color.g * 255), 0, 255);
  bb := ClampI(Round(color.b * 255), 0, 255);
  ba := ClampI(Round(color.a * 255), 0, 255);
  result := (bb or (bg shl 8) or (br shl 16) or (ba shl 24));
end;

Function EncodeABGR(color: TColorF): UInt32;
var
  br, bg, bb, ba: byte;
begin
  // Big-endian encoded RGBA 32 bpp
  br := ClampI(Round(color.r * 255), 0, 255);
  bg := ClampI(Round(color.g * 255), 0, 255);
  bb := ClampI(Round(color.b * 255), 0, 255);
  ba := ClampI(Round(color.a * 255), 0, 255);
  result := (ba or (bb shl 8) or (bg shl 16) or (br shl 24));
end;

Function DecodeABGR(abgr: UInt32): TColorF;
var r, g, b, a: single;
begin
  r:= ClampF_01(Byte(abgr shr 24) / 255.0);
  g:= ClampF_01(Byte(abgr shr 16) / 255.0);
  b:= ClampF_01(Byte(abgr shr 8) / 255.0);
  a:= ClampF_01(Byte(abgr) / 255.0);
  Result := MakeColor(r, g, b, a);
end;

Function EncodeBGR(color: TColorF): UInt32;
begin
  Result := EncodeABGR(color);
  Result := UInt32((Result shr 8) and not(255 shl 24)); // remove alpha
end;

Function DecodeBGR(bgr: UInt32): TColorF;
begin
  bgr := UInt32(bgr shl 8) or 255; // add 1.0 alpha component
  Result := DecodeABGR(bgr);
end;

Function IntensityToColor(i: single): TColorF;
begin
  Result := MakeColor(i, i, i, 1.0);
end;

Function RgbToIntensity(rgb: TColorF): single;
begin
  Result := (rgb.r + rgb.g + rgb.b) / 3;
end;

Function RgbaToIntensity(rgba: TColorF): single;
begin
  Result := (rgba.r + rgba.g + rgba.b + rgba.a) / 4;
end;

Function L2B(c:single):byte;
begin
 if c<0 then result:=0
 else if c>1 then result:=255
 else result:=Round(255*c);
end;

Function ClampD(v: Double; min: Double; max: Double): Double;
begin
  Result := v;
  if v < min then Result:= min
  else if v > max then Result:= max;
end;

Function ClampF(v: single; min: single; max: single): single;
begin
  Result := v;
  if v < min then Result:= min
  else if v > max then Result:= max;
end;

Function ClampF_01(v: Single): single;
begin
  Result := ClampF(v, 0.0, 1.0);
end;

Function ClampI(v: Integer; min: Integer; max: Integer): Integer;
begin
  Result := v;
  if v < min then Result:= min
  else if v > max then Result:= max;
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
  While (b <= length(s)) and (s[b] in [' ', #9])do  // #9 = tab
    inc(b);
  e := b;
  While (e <= length(s)) and (not(s[e] in [' ', #9])) do   // #9 = tab
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

{ Accepted formatters - %s %d %x %.1f - %.8f %1.1f - %9.8f}
Function SPrintf(const format: string; const Vals: array of const): string;
var
  pres, pf, pe, pp, pa: PChar;
  cv, I, width, ndig: Integer;
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

    // get 0 left pad len (max 9)
    width := 0;
    if pp^.isNumber then
      begin
        width := ord(pp^) - ord('0');
        inc(pp);
      end;

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
          ndig := 6;
          if pp^ = '.' then
            begin
              inc(pp);
              ndig := ord(pp^) - ord('0');
              inc(pp);

              if ndig > 8 then
                ndig := 8;
            end;

          if pv^.vtype <> vtExtended then
            raise EConvertError.Create('Invalid parameter type');

          var fmt := StringOfChar('0', width) + '.' + StringOfChar('#', ndig);
          I := FloatToTextFmt(pres, pv.vExtended^, fvExtended, PChar(fmt), TFormatSettings.Invariant);

//          case ndig of
//            1:
//              I := FloatToTextFmt(pres, pv.vExtended^, fvExtended, '.#', TFormatSettings.Invariant);
//            2:
//              I := FloatToTextFmt(pres, pv.vExtended^, fvExtended, '.##', TFormatSettings.Invariant);
//            3:
//              I := FloatToTextFmt(pres, pv.vExtended^, fvExtended, '.###', TFormatSettings.Invariant);
//            4:
//              I := FloatToTextFmt(pres, pv.vExtended^, fvExtended, '.####', TFormatSettings.Invariant);
//            5:
//              I := FloatToTextFmt(pres, pv.vExtended^, fvExtended, '.#####', TFormatSettings.Invariant);
//            6:
//              I := FloatToTextFmt(pres, pv.vExtended^, fvExtended, '.######', TFormatSettings.Invariant);
//            7:
//              I := FloatToTextFmt(pres, pv.vExtended^, fvExtended, '.#######', TFormatSettings.Invariant);
//            8:
//              I := FloatToTextFmt(pres, pv.vExtended^, fvExtended, '.########', TFormatSettings.Invariant);
//          else
//            I := FloatToTextFmt(pres, pv.vExtended^, fvExtended, '.######', TFormatSettings.Invariant);
//          end;

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

  Result := Result and (nval = Length(Vals));
end;

Function SScanf(const s: string; const format: String;
  const Vals: array of pointer): boolean;
begin
  Result := PScanf(Pchar(s), format, Vals);
end;

Function StripSScan(s: string; const format: String;
  const Vals: array of pointer): boolean;
begin
  for var i := 1 to length(s) do
    if s[i] in ['/', ':', '(', ')'] then s[i] := ' ';
  Result := SScanf(s, format, Vals);
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
  Result := FormatFloat('0.########', d, TFormatSettings.Invariant);
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

Function ColorToStr(c: TColorF; withAlpha: Boolean; bFormat: Boolean): String;
begin
  if bFormat then
    begin
      if (c.a = 0) and (not withAlpha) then
        Result := Sprintf('(%1.6f/%1.6f/%1.6f)', [c.r, c.g, c.b])
      else
        Result := Sprintf('(%1.6f/%1.6f/%1.6f/%1.6f)', [c.r, c.g, c.b, c.a]);
    end
  else
    begin
      if withAlpha then
        begin
          Result := Sprintf('%1.3f %1.3f %1.3f %1.3f', [c.r, c.g, c.b, c.a]);
          if Result = '' then
            Result := '0 0 0 0';
        end
      else
        begin
          Result := Sprintf('%1.3f %1.3f %1.3f', [c.r, c.g, c.b]);
          if Result = '' then
            Result := '0 0 0';
        end;
    end;
end;

Function FindDataFile(const Name: string): String;
begin
  if ProjectDir <> '' then
  begin
    Result := ProjectDir + Name;
    if FileExists(Result) then
      exit;
  end;
  Result := BaseDir + DataDir + '\' + Name;
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

Function StringListFind(const list: TStringList; const str: string; var index: Integer): Boolean;
begin
  if list.Sorted then
    Result := list.Find(str, index)
  else
    begin
      index := list.IndexOf(str);
      Result := index > -1;
    end;
end;

Procedure StringListFilter(list: TStringList; const filter: TStringList);
begin
  if filter = nil then exit;
  for var i := 0 to filter.Count - 1 do
    begin
      var lidx: Integer := -1;
      if StringListFind(list, filter[i], lidx) then
        list.Delete(lidx);
    end;
end;

Initialization

end.
