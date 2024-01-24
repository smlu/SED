unit Geometry;

interface

uses GlobalVars, Classes, Math, misc_utils;

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

  TVector2d = record
    x, y: double;
    Function IsZero: Boolean;
  end;

  TVector = record
    Function IsZero: Boolean;
    Procedure GetCoords(var x, y, z: double);
    Procedure SetCoords(dx, dy, dz: double);

    case integer of
      0:
        (dx, dy, dz: double);
      1:
        (x, y, z: double);
  end;


  T3DPoint = record
    x, y, z: double;
    Function IsZero: Boolean;
  end;

  TBox = record
    X1, Y1, Z1, X2, Y2, Z2: double;
  end;

  PBox = ^TBox;

  T2DBox = record
    X1, Y1, X2, Y2: double;
  end;

  TVertex = class
    x, y, z: Float;
    tx,ty,tz:double; {transformed}
    num: integer;
    mark: integer;
  end;

  TMat3x3 = array [0 .. 2, 0 .. 2] of double;
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
    FaceFlags: Longint;
    geo, light, tex: integer;
    ExtraLight: TColorF;
    normal: TVector;
    Vertices: TVertices;
    TXVertices: TTXVertices;
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

implementation

Uses lev_utils;

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
  for i := 0 to TXVertices.count - 1 do
    TXVertices[i].Free;
  TXVertices.Free;

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
  v0 := Vertices[0];
  for i := 1 to Vertices.count - 1 do
  begin
    v := Vertices[i];
    d := SMult(normal.dx, normal.dy, normal.dz, v.x - v0.x, v.y - v0.y,
      v.z - v0.z);
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
  if n >= Vertices.count - 1 then
    Result := 0
  else
    Result := n + 1;
end;

Function TPolygon.PrevVx(n: integer): integer;
begin
  if n <= 0 then
    Result := Vertices.count - 1
  else
    Result := n - 1;
end;

Procedure TPolygon.ReCalc;
begin
  CalcNormal;
end;

Function TPolygon.CalcD: double;
begin
  With Vertices[0] do
    Result := normal.dx * x + normal.dy * y + normal.dz * z;
end;

Function TPolygon.ExtrudeSize: double;
var
  xsum, ysum, zsum: double;
  i, n: integer;
  d: double;
  en: TVector;
  v1, v2: TVertex;
begin
  n := Vertices.count;
  if n = 0 then
  begin
    Result := 0;
    exit;
  end;
  xsum := 0;
  ysum := 0;
  zsum := 0;
  for i := 0 to Vertices.count - 1 do
    With Vertices[i] do
    begin
      xsum := xsum + x;
      ysum := ysum + y;
      zsum := zsum + z;
    end;
  xsum := xsum / n;
  ysum := ysum / n;
  zsum := zsum / n;

  Result := 0;
  for i := 0 to Vertices.count - 1 do
  begin
    v1 := Vertices[i];
    v2 := Vertices[NextVx(i)];
    VMult(normal.dx, normal.dy, normal.dz, v2.x - v1.x, v2.y - v1.y,
      v2.z - v1.z, en.dx, en.dy, en.dz);
    Normalize(en);
    d := Abs(SMult(en.dx, en.dy, en.dz, v1.x - xsum, v1.y - ysum, v1.z - zsum));
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
  n := Vertices.count;
  if n = 0 then
  begin
    Result := 0;
    exit;
  end;
  xsum := 0;
  ysum := 0;
  zsum := 0;
  for i := 0 to Vertices.count - 1 do
    With Vertices[i] do
    begin
      xsum := xsum + x;
      ysum := ysum + y;
      zsum := zsum + z;
    end;
  xsum := xsum / n;
  ysum := ysum / n;
  zsum := zsum / n;

  Result := 0;
  for i := 0 to Vertices.count - 1 do
    With Vertices[i] do
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
    Result := IsClose(z, Vertices[0].z);
    exit;
  end
  else if x0 and z0 then
  begin
    Result := IsClose(y, Vertices[0].y);
    exit;
  end
  else if y0 and z0 then
  begin
    Result := IsClose(x, Vertices[0].x);
    exit;
  end;
  Result := IsClose(nx, x) and IsClose(ny, y) and IsClose(nz, z);
end;

Function TPolygons.GetItem(n: integer): TPolygon;
begin
  if (n < 0) or (n >= count) then
    raise EListError.CreateFmt('Polygon Index is out of bounds: %d', [n]);
  Result := TPolygon(List[n]);
end;

Procedure TPolygons.SetItem(n: integer; p: TPolygon);
begin
  if (n < 0) or (n >= count) then
    raise EListError.CreateFmt('Vertex Index is out of bounds: %d', [n]);
  List[n] := p;
end;

function TVector2d.IsZero: Boolean;
begin
   Result := Math.IsZero(x) and Math.IsZero(y);
end;

function TVector.IsZero: Boolean;
begin
   Result := Math.IsZero(x) and Math.IsZero(y) and Math.IsZero(z);
end;

procedure TVector.GetCoords(var x, y, z: double);
begin
  x := self.dx;
  y := self.dy;
  z := self.dz;
end;

Procedure TVector.SetCoords(dx, dy, dz: double);
begin
  self.dx := dx;
  self.dy := dy;
  self.dz := dz;
end;

function T3DPoint.IsZero: Boolean;
begin
   Result := Math.IsZero(x) and Math.IsZero(y) and Math.IsZero(z);
end;

Function FindVX(Vertices: TVertices; x, y, z: double): integer;
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

end.
