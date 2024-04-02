unit Geometry;

interface

uses GlobalVars, Classes, System.Math, misc_utils;

const
  // Polygon face flags
  FF_DoubleSided         = $01;       // No backface culling. 3DO models only
  FF_Transluent          = $02;       // texture alpha blending (adjoin surfaces and 3DO models)
  FF_TexClampX           = $04;
  FF_TexClampY           = $08;
  FF_TexNoFiltering      = $10;
  FF_ZWriteDisabled      = $20;

  // IJIM flags
  FF_IJIM_3doLedge       = $40;      // (3DO model specific) Player can hang on the ledge of polygon face (Same as surface flag `SF_IJIM_Ledge`)
  FF_IJIM_FogEnabled     = $100;     // Enables fog blending for the polygon
  FF_IJIM_3doWhipAim     = $200;     // (3DO model specific) Whip aim surface (Same as surface flag `SF_IJIM_WhipAim`)

  // Renderer flags
  FF_RD_CeilingSky       = 1 shl 24; // Render flag for surfaces with special propeties
  FF_RD_HorizonSky       = 1 shl 25; // Render flag for surfaces with special propeties
  FF_RD_SpecialSurfaceWf = 1 shl 26; // Render flag for surfaces with special propeties
  FF_RD_SolidSurfaceWf   = 1 shl 27;
  FF_RD_FloorWf          = 1 shl 28;
  FF_RD_AlphaRef         = 1 shl 29;
  FF_RD_Adjoin           = 1 shl 30;

  // SED flags
  FF_SF_FLIP             = 1 shl 31;  // SED flag

type
  TGeoMode   = (NotDrawn = 0, Vertex = 1, Wireframe = 2, Solid = 3, Texture = 4);
  TLightMode = (None = 0, Lit = 1, Diffuse = 2, Gouraud = 3, Gouraud2 = 4, Gouraud3 = 5, VertexDiffuseOnly = -1); // VertexDiffuseOnly - is internal type, to not be used in the game

  TPoint2 = record
    x, y: Double;
    class function Create(const px, py: Double): TPoint2; static; inline;
  end;

  TVector2 = record
    x, y: double;
    function IsZero: Boolean;

    function Add(const v: TVector2): TVector2; inline;
    function Cross(const v: TVector2): double; inline;
    function Distance(const v: TVector2): double; overload; inline;
    function Distance(const x, y: double): double; overload; inline;
    function DistanceSqr(const v: TVector2): double; overload; inline;
    function DistanceSqr(const x, y: double): double; overload; inline;
    function Dot(const v: TVector2): double; inline;
    function Length: double; inline;
    function LengthSqr: double; inline;
    function Normalize: double; inline; // normalize this vector and returns magnitude
    function Normalized: TVector2; inline; // returns normalized this vector
    procedure Scale(const scalar: double); inline; // scales this vector
    Function Scaled(const scalar: double): TVector2; inline; // returns scaled this vector
    Function Sub(const v: TVector2): TVector2; inline;

    class operator Add(const a, b: TVector2): TVector2; inline;
    class operator Multiply(const v: TVector2; const scalar: double): TVector2; inline;
    class operator Multiply(const scalar: double; const v: TVector2): TVector2; inline;
    class operator Negative(const v: TVector2): TVector2; inline;
    class operator Subtract(const a, b: TVector2): TVector2; inline;
    class operator Equal(const a, b: TVector2): Boolean;
    class operator NotEqual(const a, b: TVector2): Boolean;
  end;

  TVector = record
    class function back: TVector; inline; static;
    class function down: TVector; inline; static;
    class function forward: TVector; inline; static;
    class function left: TVector; inline; static;
    class function right: TVector; inline; static;
    class function up: TVector; inline; static;
    class function zero: TVector; inline; static;

    constructor Create(x, y, z: double);

    function IsZero: Boolean;
    procedure GetCoords(var x, y, z: double);
    procedure SetCoords(const dx, dy, dz: double); inline;

    function Add(const v: TVector): TVector; overload; inline;
    function Add(const x, y, z: double): TVector; overload; inline;
    function Cross(const v: TVector): TVector; overload; inline;
    function Cross(const x, y, z: double): TVector; overload; inline;
    function Distance(const v: TVector): double; overload; inline;
    function Distance(const x, y, z: double): double; overload; inline;
    function DistanceSqr(const v: TVector): double; overload; inline;
    function DistanceSqr(const x, y, z: double): double; overload; inline;
    function Dot(const v: TVector): double; overload; inline;
    function Dot(const x, y, z: double): double; overload; inline;
    function Length: double; inline;
    function LengthSqr: double; inline;
    function Normalize: double; inline; // Normalize this vector and returns magnitude
    function Normalized: TVector; inline; // Returns normalized this vector
    function Project(const v: TVector): TVector; overload; inline; // Projects this vector onto vector v resulting in projected vector
    function Project(const x, y, z: double): TVector; overload; inline; // Projects this vector onto vector (x, y, z) resulting in projected vector
    function ProjectOnToNormal(const normal: TVector): TVector; overload; inline; // Projects this vector onto normal unit vector v resulting in projected vector
    function ProjectOnToNormal(const nx, ny, nz: double): TVector; overload; inline; // Projects this normal unit vector onto vector (nx, ny, nz) resulting in projected vector
    procedure Scale(const scalar: double); inline; // Dcales this vector
    Function Scaled(const scalar: double): TVector; inline; // Returns scaled this vector
    Function Sub(const v: TVector): TVector; overload; inline;
    function Sub(const x, y, z: double): TVector; overload; inline;

    class operator Add(const a, b: TVector): TVector; inline;
    class operator Multiply(const v: TVector; const scalar: double): TVector; inline;
    class operator Multiply(const scalar: double; const v: TVector): TVector; inline;
    class operator Negative(const v: TVector): TVector; inline;
    class operator Subtract(const a, b: TVector): TVector; inline;
    class operator Equal(const a, b: TVector): Boolean;
    class operator NotEqual(const a, b: TVector): Boolean;

    case integer of
      0:
        (dx, dy, dz: double);
      1:
        (x, y, z: double);
      2:
        (pitch, yaw, roll: double);
  end;

  TBox2 = record
    x1, y1, x2, y2: double;
  end;

  TBox = record
    x1, y1, z1, x2, y2, z2: double;
  end;
  PBox = ^TBox;

  TMatrix43 = record
    rvec, lvec, uvec, dvec: TVector;

    class function identity: TMatrix43; inline; static;

    class function Create(const pos: TVector; const pyr: TVector): TMatrix43; static; inline;
    class function CreateFromLook(const lookDir: TVector): TMatrix43; static;
    class function CreateFromVectorAngle(const axis: TVector; angle: Single): TMatrix43; static;
    class function CreateRotate(const pyr: TVector): TMatrix43; static; inline;
    class function CreateScale(const scale: TVector): TMatrix43; static; inline;
    class function CreateTranslate(const vec: TVector): TMatrix43; static; inline;

    class operator Multiply(const a, b: TMatrix43): TMatrix43; inline;

    function ExtractPYR: TVector;
    procedure LookAt(const eyePos, lookPos: TVector; angle: Double);
    procedure Normalize; inline;
    function TransformPoint(const point: TVector): TVector; inline;
    function TransformVector(const vec: TVector): TVector; inline;
    function TransformVectorOrtho(const vec: TVector): TVector; inline;
    function InvertOrtho: TMatrix43;

    // Helper functions:
    // Pre: multiplies this with created matrix
    // Pos: multiplies created matrix with this
    function PreRotate(const pyr: TVector): TMatrix43;
    function PostRotate(const vecPYR: TVector): TMatrix43;

    function PreTranslate(const vec: TVector): TMatrix43;
    function PostTranslate(const vec: TVector): TMatrix43;

    function PreScale(const vec: TVector): TMatrix43;
    function PostScale(const vec: TVector): TMatrix43;
  end;

  TVertex = class
    x, y, z: Float;
    tx,ty,tz:double; {transformed}
    num: integer;
    mark: integer;
  end;

  TMat3x3  = array [0 .. 2, 0 .. 2] of double;
  TMat3x3s = array [0 .. 2, 0 .. 2] of single;

  TTXVertex = class
    u, v: single;
    color: TColorF;
//    Intensity: single;
//    r, g, b: single;
    num: integer;
    Constructor Create;
    Procedure Assign(tv: TTXVertex);
  end;

  TVertices = class(TList)
    Function GetItem(n: integer): TVertex;
    Procedure SetItem(n: integer; v: TVertex);
    Property Items[n: integer]: TVertex read GetItem write SetItem; default;
  end;

  TTXVertices = class(TList)
    Function GetItem(n: integer): TTXVertex;
    Procedure SetItem(n: integer; v: TTXVertex);
    Property Items[n: integer]: TTXVertex read GetItem write SetItem; default;
  end;

  TPolygon = class
    faceFlags: Longint;
    geo, light, tex: integer;
    extraLight: TColorF;
    normal: TVector;
    vertices: TVertices;
    txVertices: TTXVertices;
    num: integer;
    { d:double; }
    Constructor Create;
    Function AddVertex(v: TVertex): integer;
    Function InsertVertex(n: integer; v: TVertex): integer;
    Procedure DeleteVertex(n: integer);
    Destructor Destroy; override;
    Function NextVx(n: integer): integer;
    Function PrevVx(n: integer): integer;

    Procedure ReCalc;
    Procedure CalcNormal;
    Function IsOnPlane(x, y, z: double): boolean;
    Function CalcD: double;
    Procedure Planarize;
    Function SurfRad: double;
    Function ExtrudeSize: double;
  end;

  TIsolatedPolygon = class(TPolygon)
    destructor Destroy; override;
  end;

  TPolygons = class(TList)
    VXList: TVertices;
    Function GetItem(n: integer): TPolygon;
    Procedure SetItem(n: integer; p: TPolygon);
    Property Items[n: integer]: TPolygon read GetItem write SetItem; default;
  end;
  { list of polygons.
    For logically groupped polygons.
    Some of the vertices can be shared }

Function FindVX(Vertices: TVertices; x, y, z: double): integer;

Function CheckSphereFaceHit(const startPos, moveNorm: TVector;
  moveDistance, radius: Double; normal, point: TVector; var sphereHitDist: Double; flags: Integer): Boolean;

implementation

Uses lev_utils, math;


Constructor TTXVertex.Create;
begin
  color := ColorWhite;
end;

Procedure TTXVertex.Assign;
begin
  num := tv.num;
  u := tv.u;
  v := tv.v;
  color.a := tv.color.a;
  color.r := tv.color.r;
  color.g := tv.color.g;
  color.b := tv.color.b;
end;

Function TVertices.GetItem(n: integer): TVertex;
begin
  if (n < 0) or (n >= count) then
    raise EListError.CreateFmt('Vertex Index is out of bounds: %d', [n]);
  Result := TVertex(List[n]);
end;

Procedure TVertices.SetItem(n: integer; v: TVertex);
begin
  if (n < 0) or (n >= count) then
    raise EListError.CreateFmt('Vertex Index is out of bounds: %d', [n]);
  List[n] := v;
end;

Function TTXVertices.GetItem(n: integer): TTXVertex;
begin
  if (n < 0) or (n >= count) then
    raise EListError.CreateFmt('Vertex Index is out of bounds: %d', [n]);
  Result := TTXVertex(List[n]);
end;

Procedure TTXVertices.SetItem(n: integer; v: TTXVertex);
begin
  if (n < 0) or (n >= count) then
    raise EListError.CreateFmt('Vertex Index is out of bounds: %d', [n]);
  List[n] := v;
end;

Constructor TPolygon.Create;
begin
  TXVertices := TTXVertices.Create;
  Vertices := TVertices.Create;
end;

Destructor TPolygon.Destroy;
var
  i: integer;
begin
  for i := 0 to TXVertices.Count - 1 do
    txVertices[i].Free;
  txVertices.Free;

// Should not free list vertices because they can be owned by some other list e.g. TPolygons.VXList
//  for i := 0 to Vertices.count - 1 do
//    Vertices[i].Free;
  Vertices.Free;
end;

Procedure TPolygon.CalcNormal;
begin
  lev_utils.CalcNormal(Self, normal);
end;

Procedure TPolygon.Planarize;
var
  i: integer;
  v0, v: TVertex;
  d: double;
begin
  v0 := vertices[0];
  for i := 1 to vertices.Count - 1 do
  begin
    v := vertices[i];
    d := normal.Dot(v.x - v0.x, v.y - v0.y, v.z - v0.z); //SMult(normal.dx, normal.dy, normal.dz, v.x - v0.x, v.y - v0.y, v.z - v0.z);
    v.x := v.x - d * normal.dx;
    v.y := v.y - d * normal.dy;
    v.z := v.z - d * normal.dz;
  end;
end;

Function TPolygon.AddVertex(v: TVertex): integer;
begin
  Result := Vertices.Add(v);
  TXVertices.Add(TTXVertex.Create);
end;

Function TPolygon.InsertVertex(n: integer; v: TVertex): integer;
begin
  Vertices.Insert(n, v);
  Result := n;
  TXVertices.Insert(n, TTXVertex.Create);
end;

Procedure TPolygon.DeleteVertex(n: integer);
begin
  Vertices.Delete(n);
  TXVertices[n].Free;
  TXVertices.Delete(n);
end;

Function TPolygon.NextVx(n: integer): integer;
begin
  if n >= vertices.Count - 1 then
    Result := 0
  else
    Result := n + 1;
end;

Function TPolygon.PrevVx(n: integer): integer;
begin
  if n <= 0 then
    Result := vertices.Count - 1
  else
    Result := n - 1;
end;

Procedure TPolygon.ReCalc;
begin
  CalcNormal;
end;

Function TPolygon.CalcD: double;
begin
  With vertices[0] do
    Result := normal.Dot(x, y, z);//normal.dx * x + normal.dy * y + normal.dz * z;
end;

Function TPolygon.ExtrudeSize: double;
var
  xsum, ysum, zsum: double;
  i, n: integer;
  d: double;
  en: TVector;
  v1, v2: TVertex;
begin
  n := vertices.Count;
  if n = 0 then
  begin
    Result := 0;
    exit;
  end;

  xsum := 0;
  ysum := 0;
  zsum := 0;
  for i := 0 to vertices.Count - 1 do
    with vertices[i] do
    begin
      xsum := xsum + x;
      ysum := ysum + y;
      zsum := zsum + z;
    end;
  xsum := xsum / n;
  ysum := ysum / n;
  zsum := zsum / n;

  Result := 0;
  for i := 0 to vertices.Count - 1 do
  begin
    v1 := Vertices[i];
    v2 := Vertices[NextVx(i)];

    en := normal.Cross(v2.x - v1.x, v2.y - v1.y, v2.z - v1.z).Normalized;
//    VMult(normal.dx, normal.dy, normal.dz, v2.x - v1.x, v2.y - v1.y,
//      v2.z - v1.z, en.dx, en.dy, en.dz);
//    Normalize(en);

    d := Abs(en.Dot(v1.x - xsum, v1.y - ysum, v1.z - zsum));//Abs(SMult(en.dx, en.dy, en.dz, v1.x - xsum, v1.y - ysum, v1.z - zsum));
    if d > Result then
      Result := d;
  end;

  Result := JKRound(Result * 2);
  if Result < 0.0001 then
    Result := 0.01;
end;

Function TPolygon.SurfRad: double;
var
  xsum, ysum, zsum: double;
  i, n: integer;
  d: double;
begin
  n := vertices.Count;
  if n = 0 then
  begin
    Result := 0;
    exit;
  end;

  xsum := 0;
  ysum := 0;
  zsum := 0;
  for i := 0 to vertices.Count - 1 do
    With vertices[i] do
    begin
      xsum := xsum + x;
      ysum := ysum + y;
      zsum := zsum + z;
    end;
  xsum := xsum / n;
  ysum := ysum / n;
  zsum := zsum / n;

  Result := 0;
  for i := 0 to vertices.Count - 1 do
    with vertices[i] do
    begin
      d := sqr(x - xsum) + sqr(y - ysum) + sqr(z - zsum);
      if d > Result then
        Result := d;
    end;
  Result := sqrt(d);
end;

Function TPolygon.IsOnPlane(x, y, z: double): boolean;
var
  nx, ny, nz: double;
  x0, y0, z0: boolean;
  d: double;
begin
  d := CalcD;
  x0 := IsClose(normal.dx, 0);
  y0 := IsClose(normal.dy, 0);
  z0 := IsClose(normal.dz, 0);

  if (not x0) and not(y0) then
    begin
      nz := z;
      ny := (d - normal.dx * x - normal.dz * z) / normal.dy;
      nx := (d - normal.dz * z - normal.dy * y) / normal.dx;
    end
  else if (not x0) and (not z0) then
    begin
      ny := y;
      nz := (d - normal.dx * x - normal.dy * y) / normal.dz;
      nx := (d - normal.dz * z - normal.dy * y) / normal.dx;
    end
  else if (not y0) and (not z0) then
    begin
      nx := x;
      nz := (d - normal.dx * x - normal.dy * y) / normal.dz;
      ny := (d - normal.dz * z - normal.dx * x) / normal.dy;
    end
  else if x0 and y0 then
    begin
      Result := IsClose(z, vertices[0].z);
      exit;
    end
  else if x0 and z0 then
    begin
      Result := IsClose(y, vertices[0].y);
      exit;
    end
  else if y0 and z0 then
    begin
      Result := IsClose(x, vertices[0].x);
      exit;
    end;
  Result := IsClose(nx, x) and IsClose(ny, y) and IsClose(nz, z);
end;

Function TPolygons.GetItem(n: integer): TPolygon;
begin
  if (n < 0) or (n >= Count) then
    raise EListError.CreateFmt('Polygon Index is out of bounds: %d', [n]);
  Result := TPolygon(List[n]);
end;

Procedure TPolygons.SetItem(n: integer; p: TPolygon);
begin
  if (n < 0) or (n >= Count) then
    raise EListError.CreateFmt('Vertex Index is out of bounds: %d', [n]);
  List[n] := p;
end;

class function TPoint2.Create(const px, py: Double): TPoint2;
begin
  Result.x := px;
  Result.y := py;
end;

function TVector2.IsZero: Boolean;
begin
   Result := System.Math.IsZero(x) and System.Math.IsZero(y);
end;

function TVector2.Add(const v: TVector2): TVector2;
begin
  VectorAdd2(x, y, v.x, v.y, Result.x, Result.y);
end;

function TVector2.Cross(const v: TVector2): double;
begin
  Result := VectorCross2(x, y, v.x, v.y);
end;

function TVector2.Distance(const v: TVector2): double;
begin
  Result := Distance(v.x, v.y);
end;

function TVector2.Distance(const x, y: double): double;
begin
  Result := VectorDist2(self.x, self.y, x, y);
end;

function TVector2.DistanceSqr(const v: TVector2): double;
begin
  Result := DistanceSqr(v.x, v.y);
end;

function TVector2.DistanceSqr(const x, y: double): double;
begin
  Result := VectorDistSqr2(self.x, self.y, x, y);
end;

function TVector2.Dot(const v: TVector2): double;
begin
  Result := VectorDot2(x, y, v.x, v.y);
end;

function TVector2.Length: double;
begin
  Result := VectorLen2(x, y);
end;

function TVector2.LengthSqr: double;
begin
  Result := VectorLenSqr2(x, y);
end;

function TVector2.Normalize: double;
begin
  Result := VectorNormalize2(x, y);
end;

function TVector2.Normalized: TVector2;
begin
  VectorNormalize2(x, y, Result.x, Result.y);
end;

procedure TVector2.Scale(const scalar: double);
begin
  VectorScale2(x, y, scalar, x, y);
end;

Function TVector2.Scaled(const scalar: double): TVector2;
begin
  Result := self;
  Result.Scale(scalar);
end;

function TVector2.Sub(const v: TVector2): TVector2;
begin
  VectorSub2(x, y, v.x, v.y, Result.x, Result.y);
end;

class operator TVector2.Add(const a, b: TVector2): TVector2;
begin
  Result := a.Add(b);
end;

class operator TVector2.Multiply(const v: TVector2; const scalar: double): TVector2;
begin
  Result := v.Scaled(scalar);
end;

class operator TVector2.Multiply(const scalar: double; const v: TVector2): TVector2;
begin
  Result := v.Scaled(scalar);
end;

class operator TVector2.Negative(const v: TVector2): TVector2;
begin
  Result.x := -v.x;
  Result.y := -v.y;
end;

class operator TVector2.Subtract(const a, b: TVector2): TVector2;
begin
  Result := a.Sub(b);
end;

class operator TVector2.Equal(const a, b: TVector2): Boolean;
begin
  Result := SameValue(a.x, b.x, CloseEnough) and
    SameValue(a.y, b.y, CloseEnough);
end;

class operator TVector2.NotEqual(const a, b: TVector2): Boolean;
begin
  Result := not (a =  b);
end;

class function TVector.back: TVector;
begin
  Result.SetCoords(0.0, -1.0, 0.0);
end;

class function TVector.down: TVector;
begin
  Result.SetCoords(0.0, 0.0, -1.0);
end;

class function TVector.forward: TVector;
begin
  Result.SetCoords(0.0, 1.0, 0.0);
end;

class function TVector.left: TVector;
begin
  Result.SetCoords(-1.0, 0.0, 0.0);
end;

class function TVector.right: TVector;
begin
  Result.SetCoords(1.0, 0.0, 0.0);
end;

class function TVector.up: TVector;
begin
  Result.SetCoords(0.0, 0.0, 1.0);
end;

class function TVector.zero: TVector;
begin
  Result.SetCoords(0.0, 0.0, 0.0);
end;

constructor TVector.Create(x, y, z: double);
begin
  self.x := x;
  self.y := y;
  self.z := z;
end;

function TVector.IsZero: Boolean;
begin
   Result := System.Math.IsZero(x) and System.Math.IsZero(y) and System.Math.IsZero(z);
end;

procedure TVector.GetCoords(var x, y, z: double);
begin
  x := self.dx;
  y := self.dy;
  z := self.dz;
end;

procedure TVector.SetCoords(const dx, dy, dz: double);
begin
  self.dx := dx;
  self.dy := dy;
  self.dz := dz;
end;

function TVector.Add(const v: TVector): TVector;
begin
  Result := Add(v.x, v.y, v.z);
end;

function TVector.Add(const x, y, z: double): TVector;
begin
  VectorAdd3(self.x, self.y, self.z, x, y, z, Result.x, Result.y, Result.z);
end;

function TVector.Cross(const v: TVector): TVector;
begin
  Result := self.Cross(v.x, v.y, v.z);
end;

function TVector.Cross(const x, y, z: double): TVector;
begin
  VectorCross3(self.x, self.y, self.z, x, y, z, Result.x, Result.y, Result.z);
end;

function TVector.Distance(const v: TVector): double;
begin
  Result := Distance(v.x, v.y, v.z);
end;

function TVector.Distance(const x, y, z: double): double;
begin
  Result := VectorDist3(self.x, self.y, self.z, x, y, z);
end;

function TVector.DistanceSqr(const v: TVector): double;
begin
  Result := DistanceSqr(v.x, v.y, v.z);
end;

function TVector.DistanceSqr(const x, y, z: double): double;
begin
  Result := VectorDistSqr3(self.x, self.y, self.z, x, y, z);
end;

function TVector.Dot(const v: TVector): double;
begin
  Result := Dot(v.x, v.y, v.z);
end;

function TVector.Dot(const x, y, z: double): double;
begin
  Result := VectorDot3(self.x, self.y, self.z, x, y, z);
end;

function TVector.Length: double;
begin
  Result := VectorLen3(x, y, z);
end;

function TVector.LengthSqr: double;
begin
  Result := VectorLenSqr3(x, y, z);
end;

function TVector.Normalize: double;
begin
  Result := VectorNormalize3(x, y, z);
end;

function TVector.Normalized: TVector;
begin
  VectorNormalize3(x, y, z, Result.x, Result.y, Result.z);
end;

function TVector.Project(const v: TVector): TVector;
begin
  Result := Project(v.x, v.y, v.z);
end;

function TVector.Project(const x, y, z: double): TVector;
begin
  VectorProject3(self.x, self.y, self.z, x, y, z, Result.x, Result.y, Result.z);
end;

function TVector.ProjectOnToNormal(const normal: TVector): TVector;
begin
  Result := ProjectOnToNormal(normal.x, normal.y, normal.z);
end;

function TVector.ProjectOnToNormal(const nx, ny, nz: double): TVector;
begin
  VectorProjectOnToNormal3(self.x, self.y, self.z, nx, ny, nz, Result.x, Result.y, Result.z);
end;

procedure TVector.Scale(const scalar: double);
begin
    VectorScale3(x, y, z, scalar, x ,y, z);
end;

function TVector.Scaled(const scalar: double): TVector;
begin
  Result := self;
  Result.Scale(scalar);
end;

function TVector.Sub(const v: TVector): TVector;
begin
  Result := Sub( v.x, v.y, v.z);
end;

function TVector.Sub(const x, y, z: double): TVector;
begin
  VectorSub3(self.x, self.y, self.z, x, y, z, Result.x, Result.y, Result.z);
end;

class operator TVector.Add(const a, b: TVector): TVector;
begin
  Result := a.Add(b);
end;

class operator TVector.Multiply(const v: TVector; const scalar: double): TVector;
begin
  Result := v.Scaled(scalar);
end;

class operator TVector.Multiply(const scalar: double; const v: TVector): TVector;
begin
  Result := v.Scaled(scalar);
end;

class operator TVector.Negative(const v: TVector): TVector;
begin
  Result.x := -v.x;
  Result.y := -v.y;
  Result.z := -v.z;
end;

class operator TVector.Subtract(const a, b: TVector): TVector;
begin
  Result := a.Sub(b);
end;

class operator TVector.Equal(const a, b: TVector): Boolean;
begin
  Result := SameValue(a.x, b.x, CloseEnough) and
    SameValue(a.y, b.y, CloseEnough) and
    SameValue(a.z, b.z, CloseEnough);
end;

class operator TVector.NotEqual(const a, b: TVector): Boolean;
begin
  Result := not (a = b);
end;

class function TMatrix43.identity: TMatrix43;
begin
  Result.rvec.SetCoords(1.0, 0.0, 0.0);
  Result.lvec.SetCoords(0.0, 1.0, 0.0);
  Result.uvec.SetCoords(0.0, 0.0, 1.0);
  Result.dvec.SetCoords(0.0, 0.0, 0.0);
end;

class operator TMatrix43.Multiply(const a, b: TMatrix43): TMatrix43;
begin
  Result.rvec := a.TransformVector(b.rvec);
  Result.lvec := a.TransformVector(b.lvec);
  Result.uvec := a.TransformVector(b.dvec);
  Result.dvec := a.TransformPoint(b.uvec);
//  Result.rvec.x := b.rvec.z * a.uvec.x + b.rvec.y * a.lvec.x + a.rvec.x * b.rvec.x;
//  Result.rvec.y := a.rvec.y * b.rvec.x + a.lvec.y * b.rvec.y + a.uvec.y * b.rvec.z;
//  Result.rvec.z := a.rvec.z * b.rvec.x + a.uvec.z * b.rvec.z + a.lvec.z * b.rvec.y;
//
//  Result.lvec.x := b.lvec.x * a.rvec.x + b.lvec.z * a.uvec.x + b.lvec.y * a.lvec.x;
//  Result.lvec.y := b.lvec.z * a.uvec.y + b.lvec.y * a.lvec.y + b.lvec.x * a.rvec.y;
//  Result.lvec.z := b.lvec.z * a.uvec.z + a.lvec.z * b.lvec.y + b.lvec.x * a.rvec.z;
//
//  Result.uvec.x := b.uvec.x * a.rvec.x + b.uvec.y * a.lvec.x + b.uvec.z * a.uvec.x;
//  Result.uvec.y := b.uvec.y * a.lvec.y + b.uvec.x * a.rvec.y + b.uvec.z * a.uvec.y;
//  Result.uvec.z := b.uvec.z * a.uvec.z + b.uvec.x * a.rvec.z + b.uvec.y * a.lvec.z;
//
//  Result.dvec.x := b.dvec.x * a.rvec.x + b.dvec.z * a.uvec.x + b.dvec.y * a.lvec.x + a.dvec.x;
//  Result.dvec.y := b.dvec.x * a.rvec.y + b.dvec.y * a.lvec.y + b.dvec.z * a.uvec.y + a.dvec.y;
//  Result.dvec.z := b.dvec.y * a.lvec.z + b.dvec.x * a.rvec.z + b.dvec.z * a.uvec.z + a.dvec.z;
end;

function TMatrix43.ExtractPYR: TVector;
var
  yaw, roll, sqrLvecX, dotLvecXY, lenLvecXY, costh: Double;
begin
  // Calculate angles
  sqrLvecX  := lvec.x * lvec.x;
  dotLvecXY := lvec.y * lvec.y + sqrLvecX;
  lenLvecXY := Sqrt(dotLvecXY);

  // Calc yaw
  if lenLvecXY >= 0.001 then
    begin
      yaw := 90.0 - RadToDeg(ArcSin(lvec.y / lenLvecXY));
      if lvec.x > 0.0 then
        costh := -costh;
      Result.yaw := yaw
    end
  else
    begin
      roll := 90.0 - RadToDeg(ArcSin(-rvec.x));
      if (-rvec.y > 0.0) and (lvec.z > 0.0) or (-rvec.y < 0.0) and (lvec.z < 0.0) then
        roll := -roll;

      Result.roll := roll;
      Result.yaw  := 0.0;
    end;

  // Calc pitch
  if lenLvecXY >= 0.001 then
    begin
      costh := dotLvecXY / lenLvecXY;
      if costh < 1.0 then
        Result.pitch := 90.0 - RadToDeg(ArcSin(costh))
      else
        Result.pitch := 0.0;
    end
  else
    Result.pitch := 90.0;

  if lvec.z < 0.0 then
    Result.pitch := -Result.pitch;

  // Calc roll
  lenLvecXY := Sqrt(-lvec.y * -lvec.y + sqrLvecX);
  if lenLvecXY >= 0.001 then
  begin
    costh := (-rvec.y * lvec.x + -lvec.y * -rvec.x) / lenLvecXY;
    if costh < 1.0 then
    begin
      if costh > -1.0 then
        Result.roll := 90.0 - RadToDeg(ArcSin(costh))
      else
        Result.roll := 180.0;
    end
    else
      Result.roll := 0.0;

    if -rvec.z < 0.0 then
      Result.roll := -Result.roll;
  end
end;

// Test alternative
//function TMatrix43.GetPYR: TVector;
//var
//  dotLvecXY, lenLvecXY: Double;
//begin
//  dotLvecXY := lvec.x * lvec.x + lvec.y * lvec.y;
//  lenLvecXY := Sqrt(dotLvecXY);
//
//  // Calculate yaw
//  if lenLvecXY >= 0.001 then
//  begin
//    Result.yaw := ArcTan2(-lvec.y, lvec.x) * RadToDeg;
//    if Result.yaw < 0.0 then
//      Result.yaw := 360.0 + Result.yaw;
//  end
//  else
//  begin
//    Result.yaw := ArcTan2(-rvec.y, -rvec.x) * RadToDeg;
//    if (rvec.y > 0.0) and (lvec.z > 0.0) or (rvec.y < 0.0) and (lvec.z < 0.0) then
//      Result.yaw := 360.0 - Result.yaw;
//  end;
//
//  // Calculate pitch
//  Result.pitch := ArcTan2(-lvec.z, lenLvecXY) * RadToDeg;
//
//  // Calculate roll
//  Result.roll := ArcTan2(-rvec.z, lvec.z) * RadToDeg;
//end;

procedure TMatrix43.LookAt(const eyePos, lookPos: TVector; angle: Double);
var
  orient: TMatrix43;
  dest: TVector;
  dotProduct: Double;
begin
  // Calculate the direction vector from eyePos to lookPos
  lvec := lookPos - eyePos;
  lvec.Normalize;

  // Build a rotation matrix based on the direction vector and angle
  orient := TMatrix43.CreateFromVectorAngle(lvec, angle);

  // Calculate the dot product of the current lvec and the z-axis vector
  dotProduct := lvec.Dot(TVector.up);

  // Transform the z-axis vector by the rotation matrix
  if Abs(dotProduct) <= 0.999 then
    dest := orient.TransformVector(TVector.up)
  else
    begin
      if lvec.z <= 0.0 then
        dest := orient.TransformVector(TVector.forward)
      else
        dest := orient.TransformVector(-TVector.forward);
    end;

  // Calculate the right vector of the matrix
  rvec.x := lvec.y * dest.z - lvec.z * dest.y;
  rvec.y := lvec.z * dest.x - lvec.x * dest.z;
  rvec.z := lvec.x * dest.y - lvec.y * dest.x;
  rvec.Normalize;

  // Calculate the up vector of the matrix
  uvec.x := rvec.y * lvec.z - rvec.z * lvec.y;
  uvec.y := lvec.x * rvec.z - lvec.z * rvec.x;
  uvec.z := rvec.x * lvec.y - lvec.x * rvec.y;
  uvec.Normalize;

  // Set the translation part of the matrix to the eyePos
  dvec := eyePos;
end;

procedure TMatrix43.Normalize;
//var
//  z, v2, y, v4, v5, v6: Double;
begin
  uvec.x := rvec.y * lvec.z - rvec.z * lvec.y;
  uvec.y := rvec.z * lvec.x - rvec.x * lvec.z;
  uvec.z := rvec.x * lvec.y - rvec.y * lvec.x;

  lvec.Normalize;
  uvec.Normalize;

  rvec.x := uvec.z * lvec.y - uvec.y * lvec.z;
  rvec.y := lvec.z * uvec.x - uvec.z * lvec.x;
  rvec.z := uvec.y * lvec.x - lvec.y * uvec.x;

//  z := self.lvec.z;
//  v2 := self.rvec.z;
//  y := self.rvec.y;
//
//  self.uvec.x := y * z - v2 * self.lvec.y;
//  v4 := self.lvec.y * self.rvec.x;
//  self.uvec.y := v2 * self.lvec.x - z * self.rvec.x;
//  self.uvec.z := v4 - y * self.lvec.x;
//
//  self.lvec.Normalize;
//  self.uvec.Normalize;
//
//  v5 := self.uvec.z;
//  v6 := self.lvec.z * self.uvec.x;
//  self.rvec.x := v5 * self.lvec.y - self.uvec.y * self.lvec.z;
//  self.rvec.y := v6 - v5 * self.lvec.x;
//  self.rvec.z := self.uvec.y * self.lvec.x - self.lvec.y * self.uvec.x;
end;

function TMatrix43.TransformPoint(const point: TVector): TVector;
begin
  // Transforms point with matrix, same as Transform vector with exception that position dvec is added at the  end
  Result.x := self.uvec.x * point.z + self.lvec.x * point.y + self.rvec.x * point.x + self.dvec.x;
  Result.y := self.uvec.y * point.z + self.lvec.y * point.y + self.rvec.y * point.x + self.dvec.y;
  Result.z := self.lvec.z * point.y + self.uvec.z * point.z + self.rvec.z * point.x + self.dvec.z;
end;

function TMatrix43.TransformVector(const vec: TVector): TVector;
begin
  Result.x := self.uvec.x * vec.z + self.lvec.x * vec.y + self.rvec.x * vec.x;
  Result.y := self.uvec.y * vec.z + self.lvec.y * vec.y + self.rvec.y * vec.x;
  Result.z := self.lvec.z * vec.y + self.uvec.z * vec.z + self.rvec.z * vec.x;
end;

function TMatrix43.TransformVectorOrtho(const vec: TVector): TVector;
begin
  Result.x := rvec.z * vec.z + rvec.y * vec.y + vec.x * rvec.x;
  Result.y := lvec.y * vec.y + lvec.z * vec.z + lvec.x * vec.x;
  Result.z := uvec.y * vec.y + uvec.z * vec.z + uvec.x * vec.x;
end;

function TMatrix43.InvertOrtho: TMatrix43;
var
  y, z, x: Double;
begin
  Result.rvec.y := lvec.x;
  Result.rvec.z := uvec.x;
  Result.lvec.z := uvec.y;

  Result.lvec.x := rvec.y;
  Result.uvec.x := rvec.z;
  Result.uvec.y := lvec.z;

  Result.rvec.x := rvec.x;
  Result.lvec.y := lvec.y;
  Result.uvec.z := uvec.z;

  y := dvec.y;
  z := dvec.z;
  x := dvec.x;

  Result.dvec.x := -(rvec.z * z + rvec.y * y + rvec.x * x);
  Result.dvec.y := -(lvec.z * z + lvec.x * x + lvec.y * y);
  Result.dvec.z := -(uvec.x * x + uvec.y * y + uvec.z * z);
end;

//class function TMatrix43.CreateRotate(const pyr: TVector): TMatrix43;
//var
//  sinX, cosX, sinY, cosY, sinZ, cosZ: Double;
//  v3, v4, v5, v7, v9, v10, v11, v12, v13, v15, v16, v17, v18, v19, v20, v21, v22: Double;
//begin
//  SinCos(DegToRad(pyr.pitch, sinX, cosX);
//  SinCos(DegToRad(pyr.yaw, sinY, cosY);
//  SinCos(DegToRad(pyr.roll, sinZ, cosZ);
//
//  v3 := sinZ * sinY;
//  v4 := sinZ * cosY;
//  v5 := cosZ * cosY;
//  v7 := cosZ * sinY;
//  v9 := v4 * sinX;
//  v10 := sinZ;
//  v11 := sinY;
//  v12 := v7 * sinX;
//  v13 := cosY;
//
//  Result.rvec.x := -v3 * sinX + v5;
//  v15 := v13 * cosX;
//  v16 := -v11;
//  v17 := -v10 * cosX;
//  v18 := v16 * cosX;
//  v19 := v9;
//  v20 := sinX;
//
//  Result.rvec.y := v19 + v7;
//  v21 := v12 + v4;
//  Result.rvec.z := v17;
//  Result.lvec.x := v18;
//  Result.lvec.y := v15;
//  Result.lvec.z := v20;
//  Result.uvec.x := v21;
//  v22 := cosZ * cosX;
//  Result.uvec.y := -sinX * v5 + v3;
//  Result.uvec.z := v22;
//
//  // Zero out the translation vector
//  Result.dvec := TVector.zero;
//end;

class function TMatrix43.CreateRotate(const pyr: TVector): TMatrix43;
var
  sinX, cosX, sinY, cosY, sinZ, cosZ: Double;
begin
  SinCos(DegToRad(pyr.pitch), sinX, cosX);
  SinCos(DegToRad(pyr.yaw)  , sinY, cosY);
  SinCos(DegToRad(pyr.roll) , sinZ, cosZ);

  Result.rvec.x :=  sinX * -(sinY * sinZ) + cosZ * cosY;
  Result.rvec.y :=  sinX * sinZ * cosY + sinY * cosZ;
  Result.rvec.z := -sinZ * cosX;

  Result.lvec.x := -sinY * cosX;
  Result.lvec.y :=  cosY * cosX;
  Result.lvec.z :=  sinX;

  Result.uvec.x :=  cosZ * sinY * sinX + sinZ * cosY;
  Result.uvec.y := -sinX * cosZ * cosY + sinZ * sinY;;
  Result.uvec.z :=  cosZ * cosX;

  Result.dvec := TVector.zero; // Zero out the translation vector
end;

class function TMatrix43.Create(const pos: TVector; const pyr: TVector): TMatrix43;
begin
  Result := CreateRotate(pyr);
  Result.dvec := pos;
end;

class function TMatrix43.CreateFromLook(const lookDir: TVector): TMatrix43;
begin
  // Copy the lookAt vector to the lvec of the output matrix
  Result.lvec := lookDir;

  // Calculate the right vector
  Result.rvec := Result.lvec.Cross(TVector.up);

  // Normalize the right vector, if it fails, recalculate the up vector
  if Result.rvec.Normalize = 0.0 then
    begin
      Result.uvec := Result.lvec.Cross(TVector.right);
      Result.uvec.Normalize;

      // Recalculate the right vector based on the up vector and lookAt vector
      Result.rvec := Result.uvec.Cross(Result.lvec);
    end
  else
    begin
      // Calculate the up vector based on the right vector and lookAt vector
      Result.uvec := Result.lvec.Cross(Result.rvec);
    end;
end;

class function TMatrix43.CreateFromVectorAngle(const axis: TVector; angle: Single): TMatrix43;
var
  angleSin, angleCos: Single;
  axisSquaredX, axisSquaredY, remainingComponent: Single;
begin
  SinCos(DegToRad(angle), angleSin, angleCos);

  // Handle special cases when the axis is aligned with the Z-axis or its negative
  if (axis.z >= 1.0) then
  begin
    // Create rotation matrix for z-axis
    Result.rvec := TVector.Create(angleCos, angleSin, 0.0);
    Result.lvec := TVector.Create(-angleSin, angleCos, 0.0);
    Result.uvec := TVector.Create(0.0, 0.0, 1.0);
    Result.dvec := TVector.Zero;
    Exit;
  end;

  if (axis.z >= 1.0) or (axis.z <= -1.0) then
  begin
    // Create rotation matrix for z-axis
    Result.rvec := TVector.Create(angleCos, -angleSin, 0.0);
    Result.lvec := TVector.Create(angleSin, angleCos, 0.0);
    Result.uvec := TVector.Create(0.0, 0.0, 1.0);
    Result.dvec := TVector.Zero;
    Exit;
  end;

  // Calculate components for the rotation matrix
  axisSquaredX := axis.x * axis.x;
  axisSquaredY := axis.y * axis.y;
  remainingComponent := 1.0 - axisSquaredX - axisSquaredY;

  Result.rvec.x := (((angleCos * axisSquaredX) * remainingComponent + (angleCos * axisSquaredY)) / (1.0 - remainingComponent)) + (axis.z * axis.x * (1.0 - angleCos));
  Result.rvec.y := (axis.z * angleSin) + ((axis.y * axis.x) * (1.0 - angleCos));
  Result.rvec.z := ((axis.z * axis.x) * (1.0 - angleCos)) - ((axis.y) * angleSin);

  Result.lvec.x := ((axis.y * axis.x) * (1.0 - angleCos)) - (axis.z * angleSin);
  Result.lvec.y := (((angleCos * axisSquaredY) * remainingComponent + (angleCos * axisSquaredX)) / (1.0 - remainingComponent)) + axisSquaredY;
  Result.lvec.z := ((axis.z * axis.y) * (1.0 - angleCos)) + (axis.x * angleSin);

  Result.uvec.x := ((axis.z * axis.x) * (1.0 - angleCos)) + ((axis.y) * angleSin);
  Result.uvec.y := ((axis.z * axis.y) * (1.0 - angleCos)) - (axis.x * angleSin);
  Result.uvec.z := ((angleCos * axisSquaredX) + (angleCos * axisSquaredY)) + remainingComponent;

  Result.dvec := TVector.Zero;
end;

class function TMatrix43.CreateScale(const scale: TVector): TMatrix43;
begin
  Result.rvec := TVector.Create(scale.x, 0.0, 0.0);
  Result.lvec := TVector.Create(0.0, scale.y, 0.0);
  Result.uvec := TVector.Create(0.0, 0.0, scale.z);
  Result.dvec := TVector.zero;
end;

class function TMatrix43.CreateTranslate(const vec: TVector): TMatrix43;
begin
    Result := TMatrix43.identity;
    Result.dvec := vec;
end;

function TMatrix43.PreRotate(const pyr: TVector): TMatrix43;
begin
  Result := TMatrix43.CreateRotate(pyr);
  Result := self * Result;
end;

function TMatrix43.PostRotate(const vecPYR: TVector): TMatrix43;
begin
  Result := TMatrix43.CreateRotate(vecPYR);
  Result := Result * self;
end;

function TMatrix43.PreTranslate(const vec: TVector): TMatrix43;
begin
  Result := TMatrix43.CreateTranslate(vec);
  Result := self * Result;
end;

function TMatrix43.PostTranslate(const vec: TVector): TMatrix43;
begin
//  Result := mat;
//  Result.dvec := mat.dvec + vec;
  Result := TMatrix43.CreateTranslate(vec);
  Result := Result * self;
end;

function TMatrix43.PreScale(const vec: TVector): TMatrix43;
begin
  Result := TMatrix43.CreateScale(vec);
  Result := self * Result;
end;

function TMatrix43.PostScale(const vec: TVector): TMatrix43;
begin
  Result := TMatrix43.CreateScale(vec);
  Result := Result * self;
end;

function FindVX(Vertices: TVertices; x, y, z: double): integer;
var
  v: TVertex;
  i: integer;
begin
  Result := -1;
  for i := 0 to Vertices.count - 1 do
  begin
    v := Vertices[i];
    if IsClose(v.x, x) and IsClose(v.y, y) and IsClose(v.z, z) then
    begin
      Result := i;
      break;
    end;
  end;
end;

destructor TIsolatedPolygon.Destroy;
var
  i: integer;
begin
  For i := 0 to Vertices.count - 1 do
    Vertices[i].Free;
  Inherited Destroy;
end;

Function CheckSphereFaceHit(const startPos, moveNorm: TVector;
  moveDistance, radius: Double; normal, point: TVector; var sphereHitDist: Double; flags: Integer): Boolean;
var
  distToPlane, distToSphere, dot: Float;
begin
  Result := False;
  distToPlane := (startPos - point).Dot(normal); // distance to plane
  if abs(distToPlane) <= 0.00001 then
    distToPlane := 0.0;

  if distToPlane < 0.0 then
    Exit;

  // Calculate distance from start point to sphere surface along the movement direction
  distToSphere := distToPlane - radius;
  if distToSphere > moveDistance then
    Exit;

  // Calculate dot product of movement direction and normal
  dot := -moveNorm.Dot(normal);
  if distToSphere < 0.0 then
    begin
      // Oposide side
      if (flags and $400) <> 0 then
        sphereHitDist := sphereHitDist + radius
      else
        sphereHitDist := 0.0;
      Result := True;
      Exit;
    end
  else if dot > 0.0 then
    begin
      // On the same side as the plane and within collision range,
      // calculate intersection point
      if dot * moveDistance >= distToSphere then
        begin
          sphereHitDist := distToSphere / dot;
          if sphereHitDist < 0.0 then
            sphereHitDist := 0.0;
          Result := True;
          Exit;
        end
    end;
end;

end.
