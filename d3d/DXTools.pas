(***************************************************************************;
 *
 *  Modyfied: 21.3.98
 *
 *  Download: http://www.sbox.tu-graz.ac.at/home/ungerik/DelphiGraphics/
 *
 ***************************************************************************)

unit DXTools;

interface

{$INCLUDE COMSWITCH.INC}

uses
{$IFDEF D2COM}
  OLE2,
{$ENDIF}
  Windows,
  Math,
  SysUtils,
  Graphics,
  DDraw,
  D3D,
  //D3DRM,
  //D3DRMDef,
  //D3DRMObj,
  D3DTypes,
  DPlay,
  DInput,
  DSetup,
  DSound;

const
  UnrecognizedError = 'Unrecognized error value';
  NoError = 'No error';
  Exceptions : boolean = True;
// This string is displayed when an EDirectX exception occurs:
  DXStat : string = '';

type
  PTrueColor = ^TTrueColor;
  TTrueColor = record
    case integer of
      1 : (Data : DWORD);
      2 : (R,G,B,A : byte);
  end;

  PColorTable = ^TColorTable;
  TColorTable = array [0..255] of TTrueColor;

  TSingleQuadruppel = array[0..4-1] of single;

  EDirectX = class (Exception)
  public
    constructor Create(Error: integer);
  end;

  EDirect3D = class (Exception)
  public
    constructor Create(Error: integer);
  end;

  EDirectDraw = class (Exception)
  public
    constructor Create(Error: integer);
  end;

  EDirectInput = class (Exception)
  public
    constructor Create(Error: integer);
  end;

  EDirectPlay = class (Exception)
  public
    constructor Create(Error: integer);
  end;

  EDirectSetup = class (Exception)
  public
    constructor Create(Error: integer);
  end;

  EDirectSound = class (Exception)
  public
    constructor Create(Error: integer);
  end;
  
const
  IdentityMatrix : TD3DMatrix = (
    _11: 1; _12: 0; _13: 0; _14: 0;
    _21: 0; _22: 1; _23: 0; _24: 0;
    _31: 0; _32: 0; _33: 1; _34: 0;
    _41: 0; _42: 0; _43: 0; _44: 1 );


  ZeroMatrix : TD3DMatrix = (
    _11: 0; _12: 0; _13: 0; _14: 0;
    _21: 0; _22: 0; _23: 0; _24: 0;
    _31: 0; _32: 0; _33: 0; _34: 0;
    _41: 0; _42: 0; _43: 0; _44: 0 );

type
  PMatrix1D = ^TMatrix1D;
  TMatrix1D = record
    case integer of
      0 : (D3DVector: TD3DVector);
      1 : (D3DColorValue: TD3DColorValue);
      2 : (a: array [0..4-1] of TD3DValue);
  end;

  PMatrix4D = ^TMatrix4D;
  TMatrix4D = record
    case integer of
      0 : (D3DMatrix: TD3DMatrix);
      1 : (_: TD3DMatrix_);
      2 : (a: array [0..4*4-1] of TD3DValue);
  end;

  TStack = class (TObject)
  private
    FItemSize : integer;
    FCount : integer;
    FTop : pointer;
    FBase : pointer;
    FStackEnd : pointer;
    FStackSize : integer;
    FIncAmount : integer;
  protected
    procedure SetCount(Value: integer);
    procedure SetTop(Value: pointer);
    procedure SetIncAmount(Value: integer);
    procedure SetStackSize(Value: integer);
    function GetStackSize : integer;
  public
    property ItemSize : integer read FItemSize;
    property Count : integer read FCount write SetCount;
    property Top : pointer read FTop write SetTop;
    property Base : pointer read FBase;
    property IncAmount : integer read FIncAmount write SetIncAmount;
    property StackSize : integer read GetStackSize write SetStackSize;
    constructor Create(ItemSize: integer);
    constructor CreateForByte;
    constructor CreateForInteger;
    constructor CreateForPointer;
    constructor CreateForSingle;
    constructor CreateForDouble;
    constructor CreateForMatrix1D;
    constructor CreateForMatrix4D;
    constructor CreateForString;
    procedure Push(const Item);
    function Pop : pointer;
    procedure PushByte(Item: byte);
    function PopByte : byte;
    procedure PushInteger(Item: integer);
    function PopInteger : integer;
    procedure PushPointer(Item: pointer);
    function PopPointer : pointer;
    procedure PushSingle(Item: single);
    function PopSingle : single;
    procedure PushMatrix1D(Item: TMatrix1D);
    function PopMatrix1D : TMatrix1D;
    procedure PushMatrix4D(Item: TMatrix4D);
    function PopMatrix4D : TMatrix4D;
    procedure PushString(Item: string);
    function PopString : string;
    procedure Increase;
    procedure Decrease;
    procedure IncreaseMulti(ItemCount: integer);
    procedure DecreaseMulti(ItemCount: integer);
    procedure IncStackSize;
    procedure Reset;
    destructor Destroy; override;
  end;

// Camera settings for Direct3D:
function ProjectionMatrix(near_plane,     // distance to near clipping plane
                          far_plane,      // distance to far clipping plane
                          fov: TD3DValue) : TD3DMatrix; // field of view angle,
                                                        // in radians
// Camera positioning for Direct3D:
function ViewMatrix(from,                  // camera location
                    at,                    // camera look-at target
                    world_up: TD3DVector;  // world's up, usually 0, 1, 0
                    roll: TD3DValue) : TD3DMatrix; // clockwise roll around
                                                 //    viewing direction,
                                                 //    in radians


function TransformationYZ(y, z: TD3DVector) : TD3DMatrix;
function TranslateMatrix(dx, dy, dz: TD3DValue) : TD3DMatrix;
function RotateXMatrix(rads: TD3DValue) : TD3DMatrix;
function RotateYMatrix(rads: TD3DValue) : TD3DMatrix;
function RotateZMatrix(rads: TD3DValue) : TD3DMatrix;
function ScaleMatrix(size: TD3DValue) : TD3DMatrix;
function MatrixMul(a, b: TD3DMatrix) : TD3DMatrix;
// Fills a DirectX-record with zero and sets the size in the first DWORD:
procedure InitRecord(var DXRecord; Size: integer);
// Computes the brightness of a RGB-color:
function GetBrightness(Red,Green,Blue: TD3DValue) : TD3DValue;
procedure SetBrightness(var Red,Green,Blue: TD3DValue; Brightness: TD3DValue);
// Releases an Object (initialised or not) and sets the pointer to nil:
function ReleaseObj(var Obj) : boolean;
// Releases an Delphi2 or 3 COM-interface (initialised or not) and sets the pointer to nil:
function ReleaseCOM(var COM) : boolean;
// Releases an initialised Delphi2 or 3 COM-interface and sets the pointer to nil:
procedure ReleaseCOMe(var COM);
// Increases the reference-counter of an Delphi2 or 3 COM-interface
function AddCOM(const COM) : pointer;
// Computes the bounding box of an retained mode frame:
//function GetFrameBox(Frame: IDirect3DRMFrame; var FrameBox: TD3DRMBox) : boolean;
// Displays a message:
procedure SM(Message: string);
// Loads colorpalette-data from a Paint Shop Pro file:
function LoadPaletteFromJASCFile(Filename: string; var Palette: TColorTable) : boolean;
// Fills a DirectDraw suface with black (0):
procedure ClearSurface(Surface: IDirectDrawSurface; Color: integer);
// Finds out, how many bit per pixel of a TBitmap are used:
function GetBitsPerPixelFromBitmap(Bitmap: Graphics.TBitmap) : integer;
procedure ReadOnlyProperty;
procedure NotReady;

// Error handling:

// Creates an Errorstring for a Direct3D returnvalue:
function D3DErrorstring(Value: HResult) : string;
// Creates an Errorstring for a DirectDraw returnvalue:
function DDrawErrorstring(Value: HResult) : string;
// Creates an Errorstring for a DirectInput returnvalue:
function DInputErrorstring(Value: HResult) : string;
// Creates an Errorstring for a DirectPlay returnvalue:
function DPlayErrorstring(Value: HResult) : string;
// Creates an Errorstring for a DirectSetup returnvalue:
function DSetupErrorstring(Value: HResult) : string;
// Creates an Errorstring for a DirectSound returnvalue:
function DSoundErrorstring(Value: HResult) : string;
// Creates an Errorstring for a DirectX returnvalue:
function DXErrorstring(Value: HResult) : string;

// Checks a Direct3D returnvalue for an error,
// and raises an EDirect3D exception if necessary:
procedure D3DCheck(Value: HResult);
// Checks a DirectDraw returnvalue for an error,
// and raises an EDirectDraw exception if necessary:
procedure DDCheck(Value: HResult);
// Checks a DirectInput returnvalue for an error,
// and raises an EDirectInput exception if necessary:
procedure DICheck(Value: HResult);
// Checks a DirectPlay returnvalue for an error,
// and raises an EDirectPlay exception if necessary:
procedure DPCheck(Value: HResult);
// Checks a DirectSound returnvalue for an error,
// and raises an EDirectSound exception if necessary:
procedure DSCheck(Value: HResult);
// Checks a DirectSetup returnvalue for an error,
// and raises an EDirectSetup exception if necessary:
procedure DSetupCheck(Value: HResult);
// Checks a DirectX returnvalue for an error,
// and raises an EDirectX exception if necessary:
procedure DXCheck(Value: HResult);

implementation



////////////////////////////////////////////////////////////////////////////////
// TStack
////////////////////////////////////////////////////////////////////////////////


procedure TStack.SetCount(Value: integer);
begin
  if (Value <> FCount) and (Value >= 0) and
     (Value < (FStackSize div FItemSize -1)) then
  begin
    FCount := Value;
    integer(FTop) := FCount * FItemSize;
  end;
end;

procedure TStack.SetTop(Value: pointer);
begin
  if (integer(Value) >= integer(FBase)) and
     (integer(Value) < integer(FStackEnd)) then FTop := Value;
end;

procedure TStack.SetIncAmount(Value: integer);
begin
  if (Value > 0) and (Value >= FIncAmount) and (Value <= 1024) then
    FIncAmount := Value
  else
    FIncAmount := FItemSize * 4;
end;

procedure TStack.SetStackSize(Value: integer);
begin
  if (integer(FBase) + Value) < integer(FStackEnd) then exit;
  integer(FStackEnd) := integer(FBase) + Value - FItemSize;
  FStackSize := Value;
  ReallocMem(FBase,FStackSize);
end;

function TStack.GetStackSize : integer;
begin
  Result := integer(FStackEnd) + FItemSize - integer(FBase);
end;

procedure TStack.Reset;
begin
  FStackSize := FIncAmount;
  ReallocMem(FBase,FStackSize);
  integer(FStackEnd) := integer(FBase) + FStackSize - FItemSize;
  FTop := FBase;
  FCount := 0;
end;

constructor TStack.Create(ItemSize: integer);
begin
  inherited Create;
  FItemSize := ItemSize;
  SetIncAmount(-1);
  FBase := nil;
  FStackEnd := nil;
  Reset;
end;

constructor TStack.CreateForByte;
begin
  Create(SizeOf(Byte));
end;

constructor TStack.CreateForInteger;
begin
  Create(SizeOf(Integer));
end;

constructor TStack.CreateForPointer;
begin
  Create(SizeOf(Pointer));
end;

constructor TStack.CreateForSingle;
begin
  Create(SizeOf(Single));
end;

constructor TStack.CreateForDouble;
begin
  Create(SizeOf(Double));
end;

constructor TStack.CreateForMatrix1D;
begin
  Create(SizeOf(TMatrix1D));
end;

constructor TStack.CreateForMatrix4D;
begin
  Create(SizeOf(TMatrix4D));
end;

constructor TStack.CreateForString;
begin
  Create(SizeOf(String));
end;

procedure TStack.Push(const Item);
begin
  Move(Item,Top^,FItemSize);
  Inc(FCount);
  Inc(integer(FTop),FItemSize);
  if integer(Top) > integer(FStackEnd) then IncStackSize;
end;

procedure TStack.IncStackSize;
begin
  Inc(FStackSize,FIncAmount);
  Inc(integer(FStackEnd),FIncAmount);
  ReAllocMem(FBase,FStackSize);
end;

function TStack.Pop : pointer;
begin
  if FCount > 0 then
  begin
    Dec(FCount);
    Dec(integer(FTop),FItemSize);
  end;
  Result := FTop;
end;

procedure TStack.Increase;
begin
  Inc(FCount);
  Inc(integer(FTop),FItemSize);
  if integer(Top) > integer(FStackEnd) then IncStackSize;
end;

procedure TStack.Decrease;
begin
  Pop;
end;

procedure TStack.IncreaseMulti(ItemCount: integer);
begin
  Inc(FCount,ItemCount);
  Inc(integer(FTop),ItemCount*FItemSize);
  if integer(Top) > integer(FStackEnd) then IncStackSize;
end;

procedure TStack.DecreaseMulti(ItemCount: integer);
begin
  Dec(FCount,ItemCount);
  Dec(integer(FTop),ItemCount*FItemSize);
  if (FCount < 0) or ((integer(FTop)-integer(FBase)) < 0) then Reset;
end;

destructor TStack.Destroy;
begin
  ReAllocMem(FBase,0);
  inherited;
end;

procedure TStack.PushByte(Item: byte);
begin
  byte(Top^) := Item;
  Inc(FCount);
  Inc(integer(FTop),SizeOf(Item));
  if integer(Top) > integer(FStackEnd) then IncStackSize;
end;

function TStack.PopByte : byte;
begin
  if FCount > 0 then
  begin
    Dec(FCount);
    Dec(integer(FTop),SizeOf(Result));
  end;
  Result := byte(FTop^);
end;

procedure TStack.PushInteger(Item: integer);
begin
  integer(Top^) := Item;
  Inc(FCount);
  Inc(integer(FTop),SizeOf(Item));
  if integer(Top) > integer(FStackEnd) then IncStackSize;
end;

function TStack.PopInteger : integer;
begin
  if FCount > 0 then
  begin
    Dec(FCount);
    Dec(integer(FTop),SizeOf(Result));
  end;
  Result := integer(FTop^);
end;

procedure TStack.PushPointer(Item: pointer);
begin
  pointer(Top^) := Item;
  Inc(FCount);
  Inc(integer(FTop),SizeOf(Item));
  if integer(Top) > integer(FStackEnd) then IncStackSize;
end;

function TStack.PopPointer : pointer;
begin
  if FCount > 0 then
  begin
    Dec(FCount);
    Dec(integer(FTop),SizeOf(Result));
  end;
  Result := pointer(FTop^);
end;

procedure TStack.PushSingle(Item: single);
begin
  single(Top^) := Item;
  Inc(FCount);
  Inc(integer(FTop),SizeOf(Item));
  if integer(Top) > integer(FStackEnd) then IncStackSize;
end;

function TStack.PopSingle : single;
begin
  if FCount > 0 then
  begin
    Dec(FCount);
    Dec(integer(FTop),SizeOf(Result));
  end;
  Result := single(FTop^);
end;

procedure TStack.PushMatrix1D(Item: TMatrix1D);
begin
  TMatrix1D(Top^) := Item;
  Inc(FCount);
  Inc(integer(FTop),SizeOf(Item));
  if integer(Top) > integer(FStackEnd) then IncStackSize;
end;

function TStack.PopMatrix1D : TMatrix1D;
begin
  if FCount > 0 then
  begin
    Dec(FCount);
    Dec(integer(FTop),SizeOf(Result));
  end;
  Result := TMatrix1D(FTop^);
end;

procedure TStack.PushMatrix4D(Item: TMatrix4D);
begin
  TMatrix4D(Top^) := Item;
  Inc(FCount);
  Inc(integer(FTop),SizeOf(Item));
  if integer(Top) > integer(FStackEnd) then IncStackSize;
end;

function TStack.PopMatrix4D : TMatrix4D;
begin
  if FCount > 0 then
  begin
    Dec(FCount);
    Dec(integer(FTop),SizeOf(Result));
  end;
  Result := TMatrix4D(FTop^);
end;

procedure TStack.PushString(Item: string);
begin
  string(Top^) := Item;
  Inc(FCount);
  Inc(integer(FTop),SizeOf(Item));
  if integer(Top) > integer(FStackEnd) then IncStackSize;
end;

function TStack.PopString : string;
begin
  if FCount > 0 then
  begin
    Dec(FCount);
    Dec(integer(FTop),SizeOf(Result));
  end;
  Result := string(FTop^);
end;

////////////////////////////////////////////////////////////////////////////////
// DXTools
////////////////////////////////////////////////////////////////////////////////

function ArcTan2(Y, X: Extended): Extended;
asm
        FLD     Y
        FLD     X
        FPATAN
        FWAIT
end;

function TransformationYZ(y, z: TD3DVector) : TD3DMatrix;
var
  ret : TD3DMatrix;
begin
  with y do
    if X <> 0.0 then
      ret := RotateZMatrix( ArcTan2(Y,X) )
    else
      ret := IdentityMatrix;

  with z do
    if Z <> 0.0 then
      ret := MatrixMul(ret, RotateYMatrix( ArcTan2(X,Z) ));

  with y do
    if Z <> 0.0 then
      ret := MatrixMul(ret, RotateXMatrix( ArcTan2(Y,Z) ));

  Result := ret;
end;

function ProjectionMatrix(near_plane,     // distance to near clipping plane
                          far_plane,      // distance to far clipping plane
           fov: TD3DValue) : TD3DMatrix;    // field of view angle, in radians
var
  c, s, Q : TD3DValue;
  ret : TD3DMatrix;
begin
    // From Jones engine
    c := cos(fov*0.5);
    s := sin(fov*0.5);
    Q := far_plane/(far_plane - near_plane); //s/(1.0 - near_plane/far_plane);

    ret := ZeroMatrix;

    ret._11 := c /s;
    ret._22 := c /s;
    ret._33 := Q;

    ret._43 := (-Q*near_plane);
    ret._34 := 1.0;
    result := ret;
end;

function TranslateMatrix(dx, dy, dz: TD3DValue) : TD3DMatrix;
var
  ret : TD3DMatrix;
begin
    ret := IdentityMatrix;
    ret._41 := dx;
    ret._42 := dy;
    ret._43 := dz;
    result := ret;
end;

function RotateXMatrix(rads: TD3DValue) : TD3DMatrix;
var
  ret : TD3DMatrix;
  cosine, sine : TD3DValue;
begin
    cosine := cos(rads);
    sine := sin(rads);
    ret := IdentityMatrix;
    ret._22 := cosine;
    ret._33 := cosine;
    ret._23 := -sine;
    ret._32 := sine;
    result := ret;
end;

function RotateYMatrix(rads: TD3DValue) : TD3DMatrix;
var
  ret : TD3DMatrix;
  cosine, sine : TD3DValue;
begin
    cosine := cos(rads);
    sine := sin(rads);
    ret := IdentityMatrix;
    ret._11 := cosine;
    ret._33 := cosine;
    ret._13 := sine;
    ret._31 := -sine;
    result := ret;
end;

function RotateZMatrix(rads: TD3DValue) : TD3DMatrix;
var
  ret : TD3DMatrix;
  cosine, sine : TD3DValue;
begin
    cosine := cos(rads);
    sine := sin(rads);
    ret := IdentityMatrix;
    ret._11 := cosine;
    ret._22 := cosine;
    ret._12 := -sine;
    ret._21 := sine;
    result := ret;
end;

function ScaleMatrix(size: TD3DValue) : TD3DMatrix;
var
  ret : TD3DMatrix;
begin
    ret := IdentityMatrix;
    ret._11 := size;
    ret._22 := size;
    ret._33 := size;
    result := ret;
end;

function ViewMatrix(from,                  // camera location
                    at,                    // camera look-at target
                    world_up: TD3DVector;  // world's up, usually 0, 1, 0
                    roll: TD3DValue) : TD3DMatrix; // clockwise roll around
                                                 //    viewing direction,
                                                 //    in radians
var
  view : TD3DMatrix;
  up, right, view_dir : TD3DVector;
begin
    view := IdentityMatrix;

    view_dir := VectorNormalize(VectorSub(at,from));
    right := VectorCrossProduct(world_up, view_dir);
    up := VectorCrossProduct(view_dir, right);

    right := VectorNormalize(right);
    up := VectorNormalize(up);

    view._11 := right.x;
    view._21 := right.y;
    view._31 := right.z;
    view._12 := up.x;
    view._22 := up.y;
    view._32 := up.z;
    view._13 := view_dir.x;
    view._23 := view_dir.y;
    view._33 := view_dir.z;

    view._41 := -VectorDotProduct(right, from);

    view._42 := -VectorDotProduct(up, from);
    view._43 := -VectorDotProduct(view_dir, from);

    if roll <> 0.0 then
        // MatrixMult function shown below
        view := MatrixMul(RotateZMatrix(-roll), TD3DMatrix(view));

    result := view;
end;

// Multiplies two matrices.
function MatrixMul(a, b: TD3DMatrix) : TD3DMatrix;
var
  ret : TMatrix4D;
  i,j,k : integer;
begin
  ret.D3DMatrix := ZeroMatrix;
  for i := 0 to 3 do
    for j := 0 to 3 do
      for k := 0 to 3 do
        ret._[i,j] := ret._[i,j] + (TD3DMatrix_(a)[k,j] * TD3DMatrix_(b)[i,k]);
  result := ret.D3DMatrix;
end;


function GetBitsPerPixelFromBitmap(Bitmap: Graphics.TBitmap) : integer;
var
  bm : Windows.TBitmap;
begin
  if GetObject(Bitmap.Handle, sizeof(bm), @bm) = 0 then Result := 0
    else Result := bm.bmBitsPixel;
end;

procedure InitRecord(var DXRecord; Size: integer);
begin
  ZeroMemory(@DXRecord,Size);
  DWORD(DXRecord) := Size;
end;

function GetDDFromDevice2(Device2: IDirect3DDevice2) : IDirectDraw;
const
  DirectDraw : IDirectDraw = nil;
  Target : IDirectDrawSurface = nil;
  Target2 : IDirectDrawSurface2 = nil;
begin
  try
    // get the render target (we need it to get the IDirectDraw)
    DxCheck( Device2.GetRenderTarget(Target) );
    // get the DirectDraw object, but first we need a IDirectDrawSurface2
    DxCheck( Target.QueryInterface(IID_IDirectDrawSurface2,Target2) );
    DxCheck( Target2.GetDDInterface(DirectDraw) );
  finally
    ReleaseCOM( Target );
    ReleaseCOM( Target2 );
    Result := DirectDraw;
  end
end;

procedure ReadOnlyProperty;
begin
  if Exceptions then Exception.Create('Property is Read-Only !');
end;

procedure NotReady;
begin
  if Exceptions then Exception.Create('Not implemented, yet !');
end;

procedure ClearSurface(Surface: IDirectDrawSurface; Color: integer);
var
  bltfx : TDDBltFX;
begin
  InitRecord(bltfx,sizeof(bltfx));
  bltfx.dwFillColor := Color;
  dxCheck( Surface.Blt(nil,nil,nil,DDBLT_COLORFILL + DDBLT_WAIT,bltfx) );
end;

function LoadPaletteFromJASCFile(Filename: string; var Palette: TColorTable) : boolean;
var
  f : text;
  i : integer;
  s : string;
  b : byte;
  Code : integer;

procedure ReadWd;
var
  c : AnsiChar;
begin
  s := '';
  repeat
    read(f,c);
    if c <> ' ' then s := s + c;
  until c = ' ';
end;

label
  ende;
begin
  Result := false;
  assign(f,Filename);
  {$i-}  reset(f);
  if ioResult <> 0 then goto ende;
  readln(f,s);
  readln(f,s);
  readln(f,s);
  for i := 0 to 255 do begin
    ReadWd;
    Val(s,b,Code);
    if Code <> 0 then goto ende;
    Palette[i].R := b;
    ReadWd;
    Val(s,b,Code);
    if Code <> 0 then goto ende;
    Palette[i].G := b;
    ReadLn(f,s);
    Val(s,b,Code);
    if Code <> 0 then goto ende;
    Palette[i].B := b;
    Palette[i].A := PC_EXPLICIT;
  end;
  Result := true;
ende:
  close(f); {$I+}
end;

function GetBrightness(Red,Green,Blue: TD3DValue) : TD3DValue;
begin
  Result := (Red * 0.3) + (Green * 0.59) + (Blue * 0.11);
end;

procedure SetBrightness(var Red,Green,Blue: TD3DValue; Brightness: TD3DValue);
// var  factor : TD3DValue;
begin
// Setzt entsprechenden Grauton:
  Red := Brightness;
  Green := Brightness;
  Blue := Brightness;
//Behält Farbe bei Helligkeitsänderung bei:
{  if GetBrightness(Red,Green,Blue) = 0.0 then begin
    Red := 0.0;
    Green := 0.0;
    Blue := 0.0;
  end else begin
    factor := Brightness / GetBrightness(Red,Green,Blue);
    Red := Red * factor;
    if Red > 1.0 then Red := 1.0;
    Green := Green * factor;
    if Green > 1.0 then Green := 1.0;
    Blue := Blue * factor;
    if Blue > 1.0 then Blue := 1.0;
  end;}
end;

procedure SM(Message: string);
begin
  MessageBox(0,PChar(Message),'DirectX-Application:',MB_APPLMODAL);
end;

function AddCOM(const COM) : pointer;
begin
{$IFDEF D2COM}
  if Assigned( IUnknown(COM) ) then IUnknown(COM).AddRef;
{$ELSE}
  if Assigned( IUnknown(COM) ) then IUnknown(COM)._AddRef;
{$ENDIF}
  Result := pointer(COM);
end;

function ReleaseObj(var Obj) : boolean;
begin
  if assigned( TObject(Obj) ) then
    begin
      TObject(Obj).Destroy;
      TObject(Obj) := nil;
      Result := True;
    end
  else
    Result := False;
end;

function ReleaseCOM(var COM) : boolean;  // Interfaceobjekt freigeben
begin
  if Assigned( IUnknown(COM) ) then // wenn Zeigerwert nicht nil dann:
    begin
{$IFDEF D2COM}
      IUnknown(COM).Release;        // Referenzzähler um eins erniedrigen
{$ELSE}
      IUnknown(COM)._Release;       // Referenzzähler um eins erniedrigen
{$ENDIF}
      IUnknown(COM) := nil;         // Zeiger auf null setzt,
      Result := True;
    end     // um weitere versehentlicher Zugriffe zu vermeiden
  else
    Result := false;
end;

procedure ReleaseCOMe(var COM);
begin
  if Assigned( IUnknown(COM) ) then
    begin
{$IFDEF D2COM}
       IUnknown(COM).Release;
{$ELSE}
       IUnknown(COM)._Release;
{$ENDIF}
       IUnknown(COM) := nil;
    end
  else
    raise Exception.Create(DXStat+#13+'ReleaseCOM of NULL object');
end;

//function GetFrameBox(Frame: IDirect3DRMFrame; var FrameBox: TD3DRMBox) : boolean;
//const
//  Visuals : IDirect3DRMVisualArray = nil;
//  Visual  : IDirect3DRMVisual = nil;
//  Mesh    : IDirect3DRMMesh = nil;
//  Meshbuilder : IDirect3DRMMeshbuilder = nil;
//var
//  Box : TD3DRMBox;
//  i,n : integer;
//begin
//  with FrameBox do begin
//    with min do begin
//      x := 0;
//      y := 0;
//      z := 0;
//    end;
//    with max do begin
//      x := 0;
//      y := 0;
//      z := 0;
//    end;
//  end;
//  Result := false;
//  if not assigned(Frame) then exit;
//  dxCheck( Frame.GetVisuals(Visuals) );
//  n := Visuals.GetSize;
//  if n = 0 then exit;
//  for i := 0 to n-1 do begin
//    Result := false;
//    dxCheck( Visuals.GetElement(i,Visual) );
//    if Visual.QueryInterface(IID_IDirect3DRMMesh,Mesh) = D3D_OK then begin
//      dxCheck( Mesh.GetBox(Box) );
//      ReleaseCOMe( Mesh );
//      Result := true;
//    end
//    else if Visual.QueryInterface(IID_IDirect3DRMMeshbuilder,Meshbuilder) = D3D_OK then begin
//      dxCheck( Meshbuilder.GetBox(Box) );
//      ReleaseCOMe( Meshbuilder );
//      Result := true;
//    end;
//    ReleaseCOMe( Visual );
//    if Box.min.x < FrameBox.min.x then FrameBox.min.x := Box.min.x;
//    if Box.min.y < FrameBox.min.y then FrameBox.min.y := Box.min.y;
//    if Box.min.z < FrameBox.min.z then FrameBox.min.z := Box.min.z;
//    if Box.max.x > FrameBox.max.x then FrameBox.max.x := Box.max.x;
//    if Box.max.y > FrameBox.max.y then FrameBox.max.y := Box.max.y;
//    if Box.max.z > FrameBox.max.z then FrameBox.max.z := Box.max.z;
//  end;
//  ReleaseCOM( Visuals );
//end;

function FormatError(ErrorString,At: string) : string;
begin
  Result := #13+#13+ErrorString+#13+#13;
  if At <> '' then Result := Result +'At: '+At+ #13+#13;
end;

constructor EDirectX.Create(Error: integer);
begin
  inherited Create( FormatError(DXErrorString(Error),DXStat) );
end;

constructor EDirect3D.Create(Error: integer);
begin
  inherited Create( FormatError(D3DErrorString(Error),DXStat) );
end;

constructor EDirectDraw.Create(Error: integer);
begin
  inherited Create( FormatError(DDrawErrorString(Error),DXStat) );
end;

constructor EDirectInput.Create(Error: integer);
begin
  inherited Create( FormatError(DInputErrorString(Error),DXStat) );
end;

constructor EDirectPlay.Create(Error: integer);
begin
  inherited Create( FormatError(DPlayErrorString(Error),DXStat) );
end;

constructor EDirectSetup.Create(Error: integer);
begin
  inherited Create( FormatError(DSetupErrorString(Error),DXStat) );
end;

constructor EDirectSound.Create(Error: integer);
begin
  inherited Create( FormatError(DSoundErrorString(Error),DXStat) );
end;

function DXErrorString(Value: HResult) : string;
begin
 if Value = 0 then
   Result := NoError
 else
   begin
     Result := D3DErrorstring(Value);
     if Result <> UnrecognizedError then exit;
     Result := DDrawErrorstring(Value);
     if Result <> UnrecognizedError then exit;
     Result := DInputErrorstring(Value);
     if Result <> UnrecognizedError then exit;
     Result := DPlayErrorstring(Value);
     if Result <> UnrecognizedError then exit;
     Result := DSetupErrorstring(Value);
     if Result <> UnrecognizedError then exit;
     Result := DSoundErrorstring(Value);
   end;
end;

procedure DXCheck(Value: HResult); { Check the Result of a COM operation }
begin
  if Value < 0 then raise EDirectX.Create(Value);
end;

procedure DXCheck_(Value: HResult); { Check the Result of a COM operation }
var
  s : string;
begin
  if Value < 0 then
  begin
    s := IntToHex(Value,8);  // for debugging
    raise EDirectX.Create(Value);
  end;
end;

procedure DDCheck(Value: HResult);
begin
  if Value < 0 then raise EDirectDraw.Create(Value);
end;

procedure D3DCheck(Value: HResult);
begin
  if Value < 0 then raise EDirect3D.Create(Value);
end;

procedure DICheck(Value: HResult);
begin
  if Value < 0 then raise EDirectInput.Create(Value);
end;

procedure DPCheck(Value: HResult);
begin
  if Value < 0 then raise EDirectPlay.Create(Value);
end;

procedure DSCheck(Value: HResult);
begin
  if Value < 0 then raise EDirectSound.Create(Value);
end;

procedure DSetupCheck(Value: HResult);
begin
  if Value < 0 then raise EDirectSetup.Create(Value);
end;

function DPlayErrorString(Value: HResult) : string;
begin

  case Value of
    CLASS_E_NOAGGREGATION: Result := 'A non-NULL value was passed for the pUnkOuter parameter in DirectPlayCreate, DirectPlayLobbyCreate, or IDirectPlayLobby2::Connect.';
    DPERR_ACCESSDENIED: Result := 'The session is full or an incorrect password was supplied.';
    DPERR_ACTIVEPLAYERS: Result := 'The requested operation cannot be performed because there are existing active players.';
    DPERR_ALREADYINITIALIZED: Result := 'This object is already initialized.';
    DPERR_APPNOTSTARTED: Result := 'The application has not been started yet.';
    DPERR_AUTHENTICATIONFAILED: Result := 'The password or credentials supplied could not be authenticated.';
    DPERR_BUFFERTOOLARGE: Result := 'The data buffer is too large to store.';
    DPERR_BUSY: Result := 'A message cannot be sent because the transmission medium is busy.';
    DPERR_BUFFERTOOSMALL: Result := 'The supplied buffer is not large enough to contain the requested data.';
    DPERR_CANTADDPLAYER: Result := 'The player cannot be added to the session.';
    DPERR_CANTCREATEGROUP: Result := 'A new group cannot be created.';
    DPERR_CANTCREATEPLAYER: Result := 'A new player cannot be created.';
    DPERR_CANTCREATEPROCESS: Result := 'Cannot start the application.';
    DPERR_CANTCREATESESSION: Result := 'A new session cannot be created.';
    DPERR_CANTLOADCAPI: Result := 'No credentials were supplied and the CryptoAPI package (CAPI) to use for cryptography services cannot be loaded.';
    DPERR_CANTLOADSECURITYPACKAGE: Result := 'The software security package cannot be loaded.';
    DPERR_CANTLOADSSPI: Result := 'No credentials were supplied and the software security package (SSPI) that will prompt for credentials cannot be loaded.';
    DPERR_CAPSNOTAVAILABLEYET: Result := 'The capabilities of the DirectPlay object have not been determined yet. This error will occur if the DirectPlay object is implemented on a connectivity solution that requires polling to determine available bandwidth and latency.';
    DPERR_CONNECTING: Result := 'The method is in the process of connecting to the network. The application should keep calling the method until it returns DP_OK, indicating successful completion, or it returns a different error.';
    DPERR_ENCRYPTIONFAILED: Result := 'The requested information could not be digitally encrypted. Encryption is used for message privacy. This error is only relevant in a secure session.';
    DPERR_EXCEPTION: Result := 'An exception occurred when processing the request.';
    DPERR_GENERIC: Result := 'An undefined error condition occurred.';
//    DPERR_INVALIDCREDENTIALS: Result := 'The credentials supplied (as to IDirectPlay3::SecureOpen) were not valid.';
    DPERR_INVALIDFLAGS: Result := 'The flags passed to this method are invalid.';
    DPERR_INVALIDGROUP: Result := 'The group ID is not recognized as a valid group ID for this game session.';
    DPERR_INVALIDINTERFACE: Result := 'The interface parameter is invalid.';
    DPERR_INVALIDOBJECT: Result := 'The DirectPlay object pointer is invalid.';
    DPERR_INVALIDPARAMS: Result := 'One or more of the parameters passed to the method are invalid.';
    DPERR_INVALIDPASSWORD: Result := 'An invalid password was supplied when attempting to join a session that requires a password.';
    DPERR_INVALIDPLAYER: Result := 'The player ID is not recognized as a valid player ID for this game session.';
    DPERR_LOGONDENIED: Result := 'The session could not be opened because credentials are required and either no credentials were supplied or the credentials were invalid.';
    DPERR_NOCAPS: Result := 'The communication link that DirectPlay is attempting to use is not capable of this function.';
    DPERR_NOCONNECTION: Result := 'No communication link was established.';
    DPERR_NOINTERFACE: Result := 'The interface is not supported.';
    DPERR_NOMESSAGES: Result := 'There are no messages in the receive queue.';
    DPERR_NONAMESERVERFOUND: Result := 'No name server (host) could be found or created. A host must exist to create a player.';
    DPERR_NONEWPLAYERS: Result := 'The session is not accepting any new players.';
    DPERR_NOPLAYERS: Result := 'There are no active players in the session.';
    DPERR_NOSESSIONS: Result := 'There are no existing sessions for this game.';
    DPERR_NOTLOBBIED: Result := 'Returned by the IDirectPlayLobby2::Connect method if the application was not started by using the IDirectPlayLobby2::RunApplication method or if there is no DPLCONNECTION structure currently initialized for this DirectPlayLobby object.';
    DPERR_NOTLOGGEDIN: Result := 'An action cannot be performed because a player or client application is not logged in. Returned by the IDirectPlay3::Send method when the client application tries to send a secure message without being logged in.';
    DPERR_OUTOFMEMORY: Result := 'There is insufficient memory to perform the requested operation.';
    DPERR_PLAYERLOST: Result := 'A player has lost the connection to the session.';
    DPERR_SENDTOOBIG: Result := 'The message being sent by the IDirectPlay3::Send method is too large.';
    DPERR_SESSIONLOST: Result := 'The connection to the session has been lost.';
    DPERR_SIGNFAILED: Result := 'The requested information could not be digitally signed. Digital signatures are used to establish the authenticity of messages.';
    DPERR_TIMEOUT: Result := 'The operation could not be completed in the specified time.';
    DPERR_UNAVAILABLE: Result := 'The requested function is not available at this time.';
    DPERR_UNINITIALIZED: Result := 'The requested object has not been initialized.';
    DPERR_UNKNOWNAPPLICATION: Result := 'An unknown application was specified.';
    DPERR_UNSUPPORTED: Result := 'The function is not available in this implementation. Returned from IDirectPlay3::GetGroupConnectionSettings and IDirectPlay3::SetGroupConnectionSettings if they are called from a session that is not a lobby session.';
    DPERR_USERCANCEL: Result := 'Can be returned in two ways. 1) The user canceled the connection process during a call to the IDirectPlay3::Open method. 2) The user clicked Cancel in one of the DirectPlay service provider dialog boxes during a call to IDirectPlay3::EnumSessions.';
    else Result := 'Unrecognized error value.';
  end;
end;

function DDrawErrorString(Value: HResult) : string;
begin
  case Value of
    DD_OK: Result := 'The request completed successfully.';
    DDERR_ALREADYINITIALIZED: Result := 'This object is already initialized.';
    DDERR_BLTFASTCANTCLIP: Result := ' if a clipper object is attached to the source surface passed into a BltFast call.';
    DDERR_CANNOTATTACHSURFACE: Result := 'This surface can not be attached to the requested surface.';
    DDERR_CANNOTDETACHSURFACE: Result := 'This surface can not be detached from the requested surface.';
    DDERR_CANTCREATEDC: Result := 'Windows can not create any more DCs.';
    DDERR_CANTDUPLICATE: Result := 'Cannot duplicate primary & 3D surfaces, or surfaces that are implicitly created.';
    DDERR_CLIPPERISUSINGHWND: Result := 'An attempt was made to set a cliplist for a clipper object that is already monitoring an hwnd.';
    DDERR_COLORKEYNOTSET: Result := 'No src color key specified for this operation.';
    DDERR_CURRENTLYNOTAVAIL: Result := 'Support is currently not available.';
    DDERR_DIRECTDRAWALREADYCREATED: Result := 'A DirectDraw object representing this driver has already been created for this process.';
    DDERR_EXCEPTION: Result := 'An exception was encountered while performing the requested operation.';
    DDERR_EXCLUSIVEMODEALREADYSET: Result := 'An attempt was made to set the cooperative level when it was already set to exclusive.';
    DDERR_GENERIC: Result := 'Generic failure.';
    DDERR_HEIGHTALIGN: Result := 'Height of rectangle provided is not a multiple of reqd alignment.';
    DDERR_HWNDALREADYSET: Result := 'The CooperativeLevel HWND has already been set. It can not be reset while the process has surfaces or palettes created.';
    DDERR_HWNDSUBCLASSED: Result := 'HWND used by DirectDraw CooperativeLevel has been subclassed, this prevents DirectDraw from restoring state.';
    DDERR_IMPLICITLYCREATED: Result := 'This surface can not be restored because it is an implicitly created surface.';
    DDERR_INCOMPATIBLEPRIMARY: Result := 'Unable to match primary surface creation request with existing primary surface.';
    DDERR_INVALIDCAPS: Result := 'One or more of the caps bits passed to the callback are incorrect.';
    DDERR_INVALIDCLIPLIST: Result := 'DirectDraw does not support the provided cliplist.';
    DDERR_INVALIDDIRECTDRAWGUID: Result := 'The GUID passed to DirectDrawCreate is not a valid DirectDraw driver identifier.';
    DDERR_INVALIDMODE: Result := 'DirectDraw does not support the requested mode.';
    DDERR_INVALIDOBJECT: Result := 'DirectDraw received a pointer that was an invalid DIRECTDRAW object.';
    DDERR_INVALIDPARAMS: Result := 'One or more of the parameters passed to the function are incorrect.';
    DDERR_INVALIDPIXELFORMAT: Result := 'The pixel format was invalid as specified.';
    DDERR_INVALIDPOSITION: Result := 'Returned when the position of the overlay on the destination is no longer legal for that destination.';
    DDERR_INVALIDRECT: Result := 'Rectangle provided was invalid.';
    DDERR_LOCKEDSURFACES: Result := 'Operation could not be carried out because one or more surfaces are locked.';
    DDERR_NO3D: Result := 'There is no 3D present.';
    DDERR_NOALPHAHW: Result := 'Operation could not be carried out because there is no alpha accleration hardware present or available.';
    DDERR_NOBLTHW: Result := 'No blitter hardware present.';
    DDERR_NOCLIPLIST: Result := 'No cliplist available.';
    DDERR_NOCLIPPERATTACHED: Result := 'No clipper object attached to surface object.';
    DDERR_NOCOLORCONVHW: Result := 'Operation could not be carried out because there is no color conversion hardware present or available.';
    DDERR_NOCOLORKEY: Result := 'Surface does not currently have a color key';
    DDERR_NOCOLORKEYHW: Result := 'Operation could not be carried out because there is no hardware support of the destination color key.';
    DDERR_NOCOOPERATIVELEVELSET: Result := 'Create function called without DirectDraw object method SetCooperativeLevel being called.';
    DDERR_NODC: Result := 'No DC was ever created for this surface.';
    DDERR_NODDROPSHW: Result := 'No DirectDraw ROP hardware.';
    DDERR_NODIRECTDRAWHW: Result := 'A hardware-only DirectDraw object creation was attempted but the driver did not support any hardware.';
    DDERR_NOEMULATION: Result := 'Software emulation not available.';
    DDERR_NOEXCLUSIVEMODE: Result := 'Operation requires the application to have exclusive mode but the application does not have exclusive mode.';
    DDERR_NOFLIPHW: Result := 'Flipping visible surfaces is not supported.';
    DDERR_NOGDI: Result := 'There is no GDI present.';
    DDERR_NOHWND: Result := 'Clipper notification requires an HWND or no HWND has previously been set as the CooperativeLevel HWND.';
    DDERR_NOMIRRORHW: Result := 'Operation could not be carried out because there is no hardware present or available.';
    DDERR_NOOVERLAYDEST: Result := 'Returned when GetOverlayPosition is called on an overlay that UpdateOverlay has never been called on to establish a destination.';
    DDERR_NOOVERLAYHW: Result := 'Operation could not be carried out because there is no overlay hardware present or available.';
    DDERR_NOPALETTEATTACHED: Result := 'No palette object attached to this surface.';
    DDERR_NOPALETTEHW: Result := 'No hardware support for 16 or 256 color palettes.';
    DDERR_NORASTEROPHW: Result := 'Operation could not be carried out because there is no appropriate raster op hardware present or available.';
    DDERR_NOROTATIONHW: Result := 'Operation could not be carried out because there is no rotation hardware present or available.';
    DDERR_NOSTRETCHHW: Result := 'Operation could not be carried out because there is no hardware support for stretching.';
    DDERR_NOT4BITCOLOR: Result := 'DirectDrawSurface is not in 4 bit color palette and the requested operation requires 4 bit color palette.';
    DDERR_NOT4BITCOLORINDEX: Result := 'DirectDrawSurface is not in 4 bit color index palette and the requested operation requires 4 bit color index palette.';
    DDERR_NOT8BITCOLOR: Result := 'DirectDrawSurface is not in 8 bit color mode and the requested operation requires 8 bit color.';
    DDERR_NOTAOVERLAYSURFACE: Result := 'Returned when an overlay member is called for a non-overlay surface.';
    DDERR_NOTEXTUREHW: Result := 'Operation could not be carried out because there is no texture mapping hardware present or available.';
    DDERR_NOTFLIPPABLE: Result := 'An attempt has been made to flip a surface that is not flippable.';
    DDERR_NOTFOUND: Result := 'Requested item was not found.';
    DDERR_NOTLOCKED: Result := 'Surface was not locked.  An attempt to unlock a surface that was not locked at all, or by this process, has been attempted.';
    DDERR_NOTPALETTIZED: Result := 'The surface being used is not a palette-based surface.';
    DDERR_NOVSYNCHW: Result := 'Operation could not be carried out because there is no hardware support for vertical blank synchronized operations.';
    DDERR_NOZBUFFERHW: Result := 'Operation could not be carried out because there is no hardware support for zbuffer blitting.';
    DDERR_NOZOVERLAYHW: Result := 'Overlay surfaces could not be z layered based on their BltOrder because the hardware does not support z layering of overlays.';
    DDERR_OUTOFCAPS: Result := 'The hardware needed for the requested operation has already been allocated.';
    DDERR_OUTOFMEMORY: Result := 'DirectDraw does not have enough memory to perform the operation.';
    DDERR_OUTOFVIDEOMEMORY: Result := 'DirectDraw does not have enough memory to perform the operation.';
    DDERR_OVERLAYCANTCLIP: Result := 'The hardware does not support clipped overlays.';
    DDERR_OVERLAYCOLORKEYONLYONEACTIVE: Result := 'Can only have ony color key active at one time for overlays.';
    DDERR_OVERLAYNOTVISIBLE: Result := 'Returned when GetOverlayPosition is called on a hidden overlay.';
    DDERR_PALETTEBUSY: Result := 'Access to this palette is being refused because the palette is already locked by another thread.';
    DDERR_PRIMARYSURFACEALREADYEXISTS: Result := 'This process already has created a primary surface.';
    DDERR_REGIONTOOSMALL: Result := 'Region passed to Clipper::GetClipList is too small.';
    DDERR_SURFACEALREADYATTACHED: Result := 'This surface is already attached to the surface it is being attached to.';
    DDERR_SURFACEALREADYDEPENDENT: Result := 'This surface is already a dependency of the surface it is being made a dependency of.';
    DDERR_SURFACEBUSY: Result := 'Access to this surface is being refused because the surface is already locked by another thread.';
    DDERR_SURFACEISOBSCURED: Result := 'Access to surface refused because the surface is obscured.';
    DDERR_SURFACELOST: Result := 'Access to this surface is being refused because the surface memory is gone. The DirectDrawSurface object representing this surface should have Restore called on it.';
    DDERR_SURFACENOTATTACHED: Result := 'The requested surface is not attached.';
    DDERR_TOOBIGHEIGHT: Result := 'Height requested by DirectDraw is too large.';
    DDERR_TOOBIGSIZE: Result := 'Size requested by DirectDraw is too large, but the individual height and width are OK.';
    DDERR_TOOBIGWIDTH: Result := 'Width requested by DirectDraw is too large.';
    DDERR_UNSUPPORTED: Result := 'Action not supported.';
    DDERR_UNSUPPORTEDFORMAT: Result := 'FOURCC format requested is unsupported by DirectDraw.';
    DDERR_UNSUPPORTEDMASK: Result := 'Bitmask in the pixel format requested is unsupported by DirectDraw.';
    DDERR_VERTICALBLANKINPROGRESS: Result := 'Vertical blank is in progress.';
    DDERR_WASSTILLDRAWING: Result := 'Informs DirectDraw that the previous Blt which is transfering information to or from this Surface is incomplete.';
    DDERR_WRONGMODE: Result := 'This surface can not be restored because it was created in a different mode.';
    DDERR_XALIGN: Result := 'Rectangle provided was not horizontally aligned on required boundary.';
    else Result := UnrecognizedError;
  end;
end;

function D3DErrorString(Value: HResult) : string;
begin
  case Value of
    D3DERR_BADMAJORVERSION: Result := 'D3DERR_BADMAJORVERSION';
    D3DERR_BADMINORVERSION: Result := 'D3DERR_BADMINORVERSION';
(*
 * An invalid device was requested by the application.
 *)
    D3DERR_INVALID_DEVICE: Result := 'D3DERR_INITFAILED';
    D3DERR_INITFAILED: Result := 'D3DERR_INITFAILED';
(*
 * SetRenderTarget attempted on a device that was
 * QI'd off the render target.
 *)
    D3DERR_DEVICEAGGREGATED: Result := 'D3DERR_DEVICEAGGREGATED';

    D3DERR_EXECUTE_CREATE_FAILED: Result := 'D3DERR_EXECUTE_CREATE_FAILED';
    D3DERR_EXECUTE_DESTROY_FAILED: Result := 'D3DERR_EXECUTE_DESTROY_FAILED';
    D3DERR_EXECUTE_LOCK_FAILED: Result := 'D3DERR_EXECUTE_LOCK_FAILED';
    D3DERR_EXECUTE_UNLOCK_FAILED: Result := 'D3DERR_EXECUTE_UNLOCK_FAILED';
    D3DERR_EXECUTE_LOCKED: Result := 'D3DERR_EXECUTE_LOCKED';
    D3DERR_EXECUTE_NOT_LOCKED: Result := 'D3DERR_EXECUTE_NOT_LOCKED';

    D3DERR_EXECUTE_FAILED: Result := 'D3DERR_EXECUTE_FAILED';
    D3DERR_EXECUTE_CLIPPED_FAILED: Result := 'D3DERR_EXECUTE_CLIPPED_FAILED';

    D3DERR_TEXTURE_NO_SUPPORT: Result := 'D3DERR_TEXTURE_NO_SUPPORT';
    D3DERR_TEXTURE_CREATE_FAILED: Result := 'D3DERR_TEXTURE_CREATE_FAILED';
    D3DERR_TEXTURE_DESTROY_FAILED: Result := 'D3DERR_TEXTURE_DESTROY_FAILED';
    D3DERR_TEXTURE_LOCK_FAILED: Result := 'D3DERR_TEXTURE_LOCK_FAILED';
    D3DERR_TEXTURE_UNLOCK_FAILED: Result := 'D3DERR_TEXTURE_UNLOCK_FAILED';
    D3DERR_TEXTURE_LOAD_FAILED: Result := 'D3DERR_TEXTURE_LOAD_FAILED';
    D3DERR_TEXTURE_SWAP_FAILED: Result := 'D3DERR_TEXTURE_SWAP_FAILED';
    D3DERR_TEXTURE_LOCKED: Result := 'D3DERR_TEXTURELOCKED';
    D3DERR_TEXTURE_NOT_LOCKED: Result := 'D3DERR_TEXTURE_NOT_LOCKED';
    D3DERR_TEXTURE_GETSURF_FAILED: Result := 'D3DERR_TEXTURE_GETSURF_FAILED';

    D3DERR_MATRIX_CREATE_FAILED: Result := 'D3DERR_MATRIX_CREATE_FAILED';
    D3DERR_MATRIX_DESTROY_FAILED: Result := 'D3DERR_MATRIX_DESTROY_FAILED';
    D3DERR_MATRIX_SETDATA_FAILED: Result := 'D3DERR_MATRIX_SETDATA_FAILED';
    D3DERR_MATRIX_GETDATA_FAILED: Result := 'D3DERR_MATRIX_GETDATA_FAILED';
    D3DERR_SETVIEWPORTDATA_FAILED: Result := 'D3DERR_SETVIEWPORTDATA_FAILED';

    D3DERR_INVALIDCURRENTVIEWPORT: Result := 'D3DERR_INVALIDCURRENTVIEWPORT';
    D3DERR_INVALIDPRIMITIVETYPE: Result := 'D3DERR_INVALIDPRIMITIVETYPE';
    D3DERR_INVALIDVERTEXTYPE: Result := 'D3DERR_INVALIDVERTEXTYPE';
    D3DERR_TEXTURE_BADSIZE: Result := 'D3DERR_TEXTURE_BADSIZE';
    D3DERR_INVALIDRAMPTEXTURE: Result := 'D3DERR_INVALIDRAMPTEXTURE';

    D3DERR_MATERIAL_CREATE_FAILED: Result := 'D3DERR_MATERIAL_CREATE_FAILED';
    D3DERR_MATERIAL_DESTROY_FAILED: Result := 'D3DERR_MATERIAL_DESTROY_FAILED';
    D3DERR_MATERIAL_SETDATA_FAILED: Result := 'D3DERR_MATERIAL_SETDATA_FAILED';
    D3DERR_MATERIAL_GETDATA_FAILED: Result := 'D3DERR_MATERIAL_GETDATA_FAILED';
    D3DERR_INVALIDPALETTE: Result := 'D3DERR_INVALIDPALETTE';

    D3DERR_ZBUFF_NEEDS_SYSTEMMEMORY: Result := 'D3DERR_ZBUFF_NEEDS_SYSTEMMEMORY';
    D3DERR_ZBUFF_NEEDS_VIDEOMEMORY: Result := 'D3DERR_ZBUFF_NEEDS_VIDEOMEMORY';
    D3DERR_SURFACENOTINVIDMEM: Result := 'D3DERR_SURFACENOTINVIDMEM';

    D3DERR_LIGHT_SET_FAILED: Result := 'D3DERR_LIGHT_SET_FAILED';
    D3DERR_LIGHTHASVIEWPORT: Result := 'D3DERR_LIGHTHASVIEWPORT';
    D3DERR_LIGHTNOTINTHISVIEWPORT: Result := 'D3DERR_LIGHTNOTINTHISVIEWPORT';

    D3DERR_SCENE_IN_SCENE: Result := 'D3DERR_SCENE_IN_SCENE';
    D3DERR_SCENE_NOT_IN_SCENE: Result := 'D3DERR_SCENE_NOT_IN_SCENE';
    D3DERR_SCENE_BEGIN_FAILED: Result := 'D3DERR_SCENE_BEGIN_FAILED';
    D3DERR_SCENE_END_FAILED: Result := 'D3DERR_SCENE_END_FAILED';

    D3DERR_INBEGIN: Result := 'D3DERR_INBEGIN';
    D3DERR_NOTINBEGIN: Result := 'D3DERR_NOTINBEGIN';
    D3DERR_NOVIEWPORTS: Result := 'D3DERR_NOVIEWPORTS';
    D3DERR_VIEWPORTDATANOTSET: Result := 'D3DERR_VIEWPORTDATANOTSET';
    D3DERR_VIEWPORTHASNODEVICE: Result := 'D3DERR_VIEWPORTHASNODEVICE';
    D3DERR_NOCURRENTVIEWPORT: Result := 'D3DERR_NOCURRENTVIEWPORT';


    //D3DRMERR_BADOBJECT: Result := 'D3DRMERR_BADOBJECT';
//    D3DRMERR_BADTYPE: Result := 'D3DRMERR_BADTYPE';
//    D3DRMERR_BADALLOC: Result := 'D3DRMERR_BADALLOC';
//    D3DRMERR_FACEUSED: Result := 'D3DRMERR_FACEUSED';
//    D3DRMERR_NOTFOUND: Result := 'D3DRMERR_NOTFOUND';
//    D3DRMERR_NOTDONEYET: Result := 'D3DRMERR_NOTDONEYET';
//    D3DRMERR_FILENOTFOUND: Result := 'The file was not found.';
//    D3DRMERR_BADFILE: Result := 'D3DRMERR_BADFILE';
//    D3DRMERR_BADDEVICE: Result := 'D3DRMERR_BADDEVICE';
//    D3DRMERR_BADVALUE: Result := 'D3DRMERR_BADVALUE';
//    D3DRMERR_BADMAJORVERSION: Result := 'D3DRMERR_BADMAJORVERSION';
//    D3DRMERR_BADMINORVERSION: Result := 'D3DRMERR_BADMINORVERSION';
//    D3DRMERR_UNABLETOEXECUTE: Result := 'D3DRMERR_UNABLETOEXECUTE';
    else Result := UnrecognizedError;
  end;
end;


function DSetupErrorString(Value: HResult) : string;
begin
  case Value of
    DSETUPERR_SUCCESS: Result := 'Setup was successful and no restart is required.';
    DSETUPERR_SUCCESS_RESTART: Result := 'Setup was successful and a restart is required.';
    DSETUPERR_BADSOURCESIZE: Result := 'A file´s size could not be verified or was incorrect.';
    DSETUPERR_BADSOURCETIME: Result := 'A file´s date and time could not be verified or were incorrect.';
    DSETUPERR_BADWINDOWSVERSION: Result := 'DirectX does not support the Windows version on the system.';
    DSETUPERR_CANTFINDDIR: Result := 'The setup program could not find the working directory.';
    DSETUPERR_CANTFINDINF: Result := 'A required .inf file could not be found.';
    DSETUPERR_INTERNAL: Result := 'An internal error occurred.';
    DSETUPERR_NOCOPY: Result := 'A file´s version could not be verified or was incorrect.';
    DSETUPERR_NOTPREINSTALLEDONNT: Result := 'The version of Windows NT on the system does not contain the current version of DirectX. An older version of DirectX may be present, or DirectX may be absent altogether.';
    DSETUPERR_OUTOFDISKSPACE: Result := 'The setup program ran out of disk space during installation.';
    DSETUPERR_SOURCEFILENOTFOUND: Result := 'One of the required source files could not be found.';
    DSETUPERR_UNKNOWNOS: Result := 'The operating system on your system is not currently supported.';
    DSETUPERR_USERHITCANCEL: Result := 'The Cancel button was pressed before the application was fully installed.';
    else Result := UnrecognizedError;
  end;
end;

function DSoundErrorstring(Value: HResult) : string;
begin
  case Value of
    DS_OK: Result := 'The request completed successfully.';
    DSERR_ALLOCATED: Result := 'The request failed because resources, such as a priority level, were already in use by another caller.';
    DSERR_ALREADYINITIALIZED: Result := 'The object is already initialized.';
    DSERR_BADFORMAT: Result := 'The specified wave format is not supported.';
    DSERR_BUFFERLOST: Result := 'The buffer memory has been lost and must be restored.';
    DSERR_CONTROLUNAVAIL: Result := 'The control (volume, pan, and so forth) requested by the caller is not available.';
    DSERR_GENERIC: Result := 'An undetermined error occurred inside the DirectSound subsystem.';
    DSERR_INVALIDCALL: Result := 'This function is not valid for the current state of this object.';
    DSERR_INVALIDPARAM: Result := 'An invalid parameter was passed to the returning function.';
    DSERR_NOAGGREGATION: Result := 'The object does not support aggregation.';
    DSERR_NODRIVER: Result := 'No sound driver is available for use.';
    DSERR_NOINTERFACE: Result := 'The requested COM interface is not available.';
    DSERR_OTHERAPPHASPRIO: Result := 'Another application has a higher priority level, preventing this call from succeeding.';
    DSERR_OUTOFMEMORY: Result := 'The DirectSound subsystem could not allocate sufficient memory to complete the caller´s request.';
    DSERR_PRIOLEVELNEEDED: Result := 'The caller does not have the priority level required for the function to succeed.';
    DSERR_UNINITIALIZED: Result := 'The IDirectSound::Initialize method has not been called or has not been called successfully before other methods were called.';
    DSERR_UNSUPPORTED: Result := 'The function called is not supported at this time.';
    else Result := UnrecognizedError;
  end;
end;

function DInputErrorString(Value: HResult) : string;
begin
  case Value of
    DI_OK: Result := 'The operation completed successfully.';
    S_FALSE: Result := '"The operation had no effect." or "The device buffer overflowed and some input was lost." or "The device exists but is not currently attached." or "The change in device properties had no effect."';
//    DI_BUFFEROVERFLOW: Result := 'The device buffer overflowed and some input was lost. This value is equal to the S_FALSE standard COM return value.';
    DI_DOWNLOADSKIPPED: Result := 'The parameters of the effect were successfully updated, but the effect could not be downloaded because the associated device was not acquired in exclusive mode.';
    DI_EFFECTRESTARTED: Result := 'The effect was stopped, the parameters were updated, and the effect was restarted.';
//    DI_NOEFFECT: Result := 'The operation had no effect. This value is equal to the S_FALSE standard COM return value.';
//    DI_NOTATTACHED: Result := 'The device exists but is not currently attached. This value is equal to the S_FALSE standard COM return value.';
    DI_POLLEDDEVICE: Result := 'The device is a polled device. As a result, device buffering will not collect any data and event notifications will not be signaled until the IDirectInputDevice2::Poll method is called.';
//    DI_PROPNOEFFECT: Result := 'The change in device properties had no effect. This value is equal to the S_FALSE standard COM return value.';
    DI_TRUNCATED: Result := 'The parameters of the effect were successfully updated, but some of them were beyond the capabilities of the device and were truncated to the nearest supported value.';
    DI_TRUNCATEDANDRESTARTED: Result := 'Equal to DI_EFFECTRESTARTED | DI_TRUNCATED.';
    DIERR_ACQUIRED: Result := 'The operation cannot be performed while the device is acquired.';
    DIERR_ALREADYINITIALIZED: Result := 'This object is already initialized';
    DIERR_BADDRIVERVER: Result := 'The object could not be created due to an incompatible driver version or mismatched or incomplete driver components.';
    DIERR_BETADIRECTINPUTVERSION: Result := 'The application was written for an unsupported prerelease version of DirectInput.';
    DIERR_DEVICEFULL: Result := 'The device is full.';
    DIERR_DEVICENOTREG: Result := 'The device or device instance is not registered with DirectInput. This value is equal to the REGDB_E_CLASSNOTREG standard COM return value.';
    DIERR_EFFECTPLAYING: Result := 'The parameters were updated in memory but were not downloaded to the device because the device does not support updating an effect while it is still playing.';
    DIERR_HASEFFECTS: Result := 'The device cannot be reinitialized because there are still effects attached to it.';
    DIERR_GENERIC: Result := 'An undetermined error occurred inside the DirectInput subsystem. This value is equal to the E_FAIL standard COM return value.';
//    DIERR_HANDLEEXISTS: Result := 'The device already has an event notification associated with it. This value is equal to the E_ACCESSDENIED standard COM return value.';
    DIERR_INCOMPLETEEFFECT: Result := 'The effect could not be downloaded because essential information is missing. For example, no axes have been associated with the effect, or no type-specific information has been supplied.';
    DIERR_INPUTLOST: Result := 'Access to the input device has been lost. It must be reacquired.';
    DIERR_INVALIDPARAM: Result := 'An invalid parameter was passed to the returning function, or the object was not in a state that permitted the function to be called. This value is equal to the E_INVALIDARG standard COM return value.';
    DIERR_MOREDATA: Result := 'Not all the requested information fitted into the buffer.';
    DIERR_NOAGGREGATION: Result := 'This object does not support aggregation.';
    DIERR_NOINTERFACE: Result := 'The specified interface is not supported by the object. This value is equal to the E_NOINTERFACE standard COM return value.';
    DIERR_NOTACQUIRED: Result := 'The operation cannot be performed unless the device is acquired.';
    DIERR_NOTBUFFERED: Result := 'The device is not buffered. Set the DIPROP_BUFFERSIZE property to enable buffering.';
    DIERR_NOTDOWNLOADED: Result := 'The effect is not downloaded.';
    DIERR_NOTEXCLUSIVEACQUIRED: Result := 'The operation cannot be performed unless the device is acquired in DISCL_EXCLUSIVE mode.';
    DIERR_NOTFOUND: Result := 'The requested object does not exist.';
    DIERR_NOTINITIALIZED: Result := 'This object has not been initialized.';
//    DIERR_OBJECTNOTFOUND: Result := 'The requested object does not exist.';
    DIERR_OLDDIRECTINPUTVERSION: Result := 'The application requires a newer version of DirectInput.';
    DIERR_OTHERAPPHASPRIO: Result := '"The device already has an event notification associated with it." or "The specified property cannot be changed." or "Another application has a higher priority level, preventing this call from succeeding. "';
    DIERR_OUTOFMEMORY: Result := 'The DirectInput subsystem could not allocate sufficient memory to complete the call. This value is equal to the E_OUTOFMEMORY standard COM return value.';
//    DIERR_READONLY: Result := 'The specified property cannot be changed. This value is equal to the E_ACCESSDENIED standard COM return value.';
    DIERR_UNSUPPORTED: Result := 'The function called is not supported at this time. This value is equal to the E_NOTIMPL standard COM return value.';
    E_PENDING: Result := 'Data is not yet available.';
    else Result := UnrecognizedError;
  end;
end;


end.
