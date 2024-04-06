unit lev_utils;

interface

uses Geometry, J_Level, misc_utils, ProgressDialog,
  graph_files, U_multisel, u_3dos, Images, System.Types;

Const
  rs_surfs = 1;

const
  rt_x    = 0;
  rt_y    = 1;
  rt_z    = 2;
  rt_grid = 3;

  df_MaxSurfVerts        = 24;
  ijim_MaxSurfVerts      = 64;
  ijim_MaxTrackSurfVerts = 4;

const
  newLight: boolean = true;

Type
  TVecRotData = record
    sinPch, cosPch, sinYaw, cosYaw, sinRol, cosRol: double;
  end;

  TScaleData = record
    cx, cy, cz: double; { center }
    sfactor: double;
    vec: TVector;
    how: (scale_xyz, scale_x, scale_y, scale_z, scale_vec);
  end;

function MaxSurfaceVertices(const lev: TJKLevel): integer; // returns max number of vertices surface can have baseed on level kind

function NextIdx(n, maxn: integer): integer;

function GetAngle(x, y: double; xa, ya, xb, yb: double): double;
function Isabove180(x, y: double; xa, ya, xb, yb: double): boolean;
function DoIntersect(x11, y11, x12, y12, x21, y21, x22, y22: double): boolean;

procedure MultVM3(const mx: TMat3x3; var x, y, z: double);
procedure MultM3(var mx: TMat3x3; const bymx: TMat3x3);
procedure MultVM3s(const mx: TMat3x3s; var x, y, z: single);
procedure MultM3s(var mx: TMat3x3s; const bymx: TMat3x3s);

procedure CreateRotMatrix(var mx: TMat3x3; pch, yaw, rol: double);
procedure ScaleMatrix(var mx: TMat3x3; scx, scy, scz: double);

procedure CreateRotMatrixS(var mx: TMat3x3s; pch, yaw, rol: single);
procedure ScaleMatrixS(var mx: TMat3x3s; scx, scy, scz: single);

procedure CalcNormal(const surf: TPolygon; var normal: TVector);

procedure FindBBox(const sec: TJKSector; var box: TBox);
procedure FindBSphere(const sec: TJKSector; var cx, cy, cz, Rad: double);
function FindCollideBox(const sec: TJKSector; const bbox: TBox; cx, cy, cz: double;
  var cbox: TBox): boolean;

function DoBoxesIntersect(const box1, box2: TBox): boolean;
procedure SetBox(var box: TBox; x1, x2, y1, y2, z1, z2: double);
function IsPointInBox(const box: TBox; x, y, z: double): boolean;

function Do2DBoxesIntersect(const box1, box2: TBox2): boolean;
procedure Set2DBox(var box: TBox2; x1, x2, y1, y2: double);
function IsPointIn2DBox(const box: TBox2; x, y: double): boolean;

procedure FindCenter(const sec: TJKSector; var cx, cy, cz: double);
procedure CalcSecCenter(const sec: TJKSector; var cx, cy, cz: double);

procedure CalcSurfCenter(const surf: TJKSurface; var cx, cy, cz: double);
procedure CalcSurfRect(const surf: TJKSurface; var v1, v2, v3, v4: TJKVertex; by: double);

procedure RotateVector(var vec: TVector; pch, yaw, rol: double);
procedure GetJKPYR(const x, y, z: TVector; var pch, yaw, rol: double);
procedure CalcRotVecData(var rd: TVecRotData; pch, yaw, rol: double);
procedure RotVecbyData(var vec: TVector; var rd: TVecRotData);

function IsSurfConvex(const surf: TJKSurface): boolean;
function IsSurfPlanar(const surf: TJKSurface): boolean;
function IsSectorConvex(const sec: TJKSector): boolean;
function IsOnLine(x1, y1, z1, x2, y2, z2, x, y, z: double): boolean;
function DistToPlane(const surf: TPolygon; x, y, z: double): double;

{ Do line and surface intersect }
function PlaneLineXn(const normal: TVector; D: double;
  x1, y1, z1, x2, y2, z2: double; var x, y, z: double): boolean;
function PlaneLineXnNew(const normal: TVector; pX, pY, pZ, x1, y1, z1, x2, y2,
  z2: double; var x, y, z: double): boolean;

function IsPointOnSurface(const surf: TJKSurface; x, y, z: double): boolean;
function IsPointWithInLine(x, y, z, x1, y1, z1, x2, y2, z2: double): boolean;
{ Function SurfLineXn(surf:TJKSurface;x1,y1,z1,x2,y2,z2:double):integer; }

function IsInSector(const sec: TJKSector; x, y, z: double): boolean;
function DoSectorsOverlap(const sec1, sec2: TJKSector): boolean;

function FindSectorForThing(const thing: TJKThing): Integer;
function FindSectorForXYZ(const lev: TJKLevel; x, y, z: double): Integer; overload;
function FindSectorForXYZ(const lev: TJKLevel; x, y, z: double; visibleOnly: Boolean): Integer; overload;
function GetPreviousSector(const lev: TJKLevel; n: integer): Integer; // Returns index of the previous sector on the visible layer or -1
function GetPreviousThing(const lev: TJKLevel; n: integer): Integer;  // Returns index of the previous thing on the visible layer or -1
function GetPreviousLight(const lev: TJKLevel; n: integer): Integer;  // Returns index of the previous light on the visible layer or -1

{ Level modifications }
procedure TranslateSectors(lev: TJKLevel; scsel: TSCMultiSel; cursc: integer;
  dx, dy, dz: double);
procedure TranslateSurfaces(lev: TJKLevel; sfsel: TSFMultiSel;
  cursc, cursf: integer; dx, dy, dz: double);
procedure TranslateVertices(lev: TJKLevel; vxsel: TVXMultiSel;
  cursc, curvx: integer; dx, dy, dz: double);

procedure TranslateFrames(lev: TJKLevel; frsel: TFRMultisel; th, fr: integer;
  dx, dy, dz: double);

procedure TranslateEdges(lev: TJKLevel; edsel: TEDMultiSel;
  cursc, cursf, cured: integer; dx, dy, dz: double);

procedure TranslateThingKeepFrame(lev: TJKLevel; curth: integer;
  dx, dy, dz: double);

procedure TranslateThings(lev: TJKLevel; thsel: TTHMultiSel; curth: integer;
  dx, dy, dz: double; moveframes: boolean);
procedure TranslateLights(lev: TJKLevel; ltsel: TLTMultiSel; curlt: integer;
  dx, dy, dz: double);

procedure RotateSectors(lev: TJKLevel; scsel: TSCMultiSel; axis: TVector;
  cx, cy, cz: double; angle: double);
procedure RotateSurfaces(level: TJKLevel; sfsel: TSFMultiSel; axis: TVector;
  cx, cy, cz: double; angle: double);
procedure RotateThings(lev: TJKLevel; thsel: TTHMultiSel; axis: TVector;
  cx, cy, cz: double; angle: double; bRotatePYR: Boolean = False);
procedure RotateLights(lev: TJKLevel; ltsel: TLTMultiSel; axis: TVector;
  cx, cy, cz: double; angle: double);
procedure RotateFrames(lev: TJKLevel; frsel: TFRMultisel; axis: TVector;
  cx, cy, cz: double; angle: double; bRotatePYR: Boolean = False);

procedure ScaleSectors(lev: TJKLevel; scsel: TSCMultiSel; const sd: TScaleData;
  scaletx: boolean);
procedure ScaleSurfaces(level: TJKLevel; sfsel: TSFMultiSel; const sd: TScaleData;
  scaletx: boolean);
procedure ScaleThings(lev: TJKLevel; thsel: TTHMultiSel; const sd: TScaleData);
procedure ScaleLights(lev: TJKLevel; ltsel: TLTMultiSel; const sd: TScaleData);
procedure ScaleFrames(lev: TJKLevel; frsel: TFRMultisel; const sd: TScaleData);

procedure FlipSectors(lev: TJKLevel; scsel: TSCMultiSel; cx, cy, cz: double;
  how: integer);
procedure FlipSurfaces(lev: TJKLevel; sfsel: TSFMultiSel);
procedure FlipThings(lev: TJKLevel; thsel: TTHMultiSel; cx, cy, cz: double;
  how: integer);

procedure FlipThingsOverPlane(lev: TJKLevel; thsel: TTHMultiSel; const pnorm: TVector;
  cx, cy, cz: double);
procedure FlipLightsOverPlane(lev: TJKLevel; ltsel: TLTMultiSel; const pnorm: TVector;
  cx, cy, cz: double);
procedure FlipFramesOverPlane(lev: TJKLevel; frsel: TFRMultisel; const pnorm: TVector;
  cx, cy, cz: double);

procedure FlipSectorsOverPlane(lev: TJKLevel; scsel: TSCMultiSel;
  const pnorm: TVector; pX, pY, pZ: double);

// Functions for retrieving nearest vertex to the line defined by lx, ly, lt and lineNormal
function GetNearestVertexToLine(vertices: TVertices; lx, ly, lz: double; lineNormal: TVector; radius: double; var vx, vy, vz: double): boolean; overload;
function GetNearestVertexToLine(vertices: TVertices; lx, ly, lz: double; lineNormal: TVector; radius: double; var vx, vy, vz, minDistanceSqr: double): boolean; overload;
function GetNearestWorldVertexToLine(lev: TJKLevel; lx, ly, lz: double; lineNormal: TVector; radius: double; var vx, vy, vz: double): boolean; overload;
function GetNearestWorldVertexToLine(lev: TJKLevel; linePoint: Tvector; lineNormal: TVector; radius: double; var vx, vy, vz: double): boolean; overload;
function GetNearestWorldVertexToLine(lev: TJKLevel; linePoint: Tvector; lineNormal: TVector; radius: double; var v: TVector): boolean; overload;

procedure ExtrudeSurface(surf: TJKSurface; by: double);
procedure ExtrudeAndExpandSurface(surf: TJKSurface; by: double);

function CleaveSurface(surf: TJKSurface; const cnormal: TVector;
  cx, cy, cz: double): boolean;
function CleaveSector(sec: TJKSector; const cnormal: TVector;
  cx, cy, cz: double): boolean;
function CleaveEdge(surf: TJKSurface; edge: integer; const cnormal: TVector;
  cx, cy, cz: double): boolean;

{ Creates adjoin by cleaving both surfaces to reduce them to their
  common area }
function ConnectSurfaces(surf1, surf2: TJKSurface): boolean;


function ConnectSectors(sec1, sec2: TJKSector): boolean;

function BuildSector(lev: TJKLevel; sfsel: TSFMultiSel): TJKSector;
function BuildSurface(lev: TJKLevel; vxsel: TVXMultiSel): TJKSurface;

function FlattenSurface(surf: TJKSurface): boolean;

function MergeSurfaces(surf, surf1: TJKSurface): TJKSurface;
function MergeSurfacesAt(surf: TJKSurface; edge: integer): boolean;
function MergeSectors(sec, sec1: TJKSector): TJKSector;
function MergeVertices(lev: TJKLevel; vxsel: TVXMultiSel): integer;  // returns number of vertices merged

procedure CalcDefaultUVNormals(const surf: TJKSurface; orgvx: integer; var un, vn: TVector);
procedure CalcUVNormals(const surf: TJKSurface; var un, vn: TVector);
procedure CalcUVNormalsFrom(const surf: TJKSurface; nv1, nv2: integer; var un, vn: TVector);
procedure CalcUV(const surf: TJKSurface; orgvx: integer);
procedure UpdateSurfUVData(const surf: TJKSurface; var un, vn: TVector);
procedure FindUVScales(surf: TJKSurface);

procedure ArrangeTexture(const surf: TJKSurface; orgvx: integer; const un, vn: TVector);
procedure ArrangeTextureBy(const surf: TJKSurface; const un, vn: TVector;
  refx, refy, refz, refu, refv: double);
function IsTXFlipped(const surf: TJKSurface): boolean;
procedure RemoveSurfRefs(lev: TJKLevel; surf: TJKSurface);
procedure RemoveSecRefs(lev: TJKLevel; sec: TJKSector; opt: byte);

{ Function StitchAt(surf:TJKSurface;nv1,nv2:integer):integer;

  Function StitchAtEdge(surf:TJKSurface;edge:integer):integer; }

function StitchSurfaces(surf, surf1: TJKSurface): boolean;

function FindCommonEdges(const surf, withsurf: TJKSurface;
  var v11, v12, v21, v22: integer): boolean;

procedure CalcLighting(lev: TJKLevel; scs: TSCMultiSel);
procedure CalcLightingNew(lev: TJKLevel; scs: TSCMultiSel);

procedure DeleteSector(lev: TJKLevel; n: integer; force: Boolean = False); // force param skips check for sector edit lock
procedure DeleteSurface(lev: TJKLevel; sc, sf: integer; force: Boolean = False);  // force param skips check for surface edit lock
procedure DeleteThing(lev: TJKLevel; n: integer; force: Boolean = False); // force param skips check for thing edit lock
function DeleteVertex(sec: TJKSector; vx: integer; silent: boolean): boolean;
procedure DeleteLight(lev: TJKLevel; n: integer; force: Boolean = False);
procedure DeleteCog(lev: TJKLevel; n: integer);

function DO_Surf_Overlap(surf, surf1: TJKSurface): boolean;
function DO_Surf_Match(surf, surf1: TJKSurface): boolean;

function MakeAdjoin(surf: TJKSurface): boolean;
function MakeAdjoinSCUP(surf: TJKSurface; sc: integer): boolean;
function UnAdjoin(surf: TJKSurface): boolean;
procedure SysAdjoinSurfaces(surf1, surf2: TJKSurface);
function AdjoinSectors(sec, sec1: TJKSector): boolean;
function UnAdjoinSectors(sec, sec1: TJKSector): boolean;

procedure SetAdjoinParams(sf: TJKSurface);
procedure ClearAdjoinParams(sf: TJKSurface);

procedure CreateCube(lev: TJKLevel; x, y, z: double;
  const pnormal, edge: TVector);

procedure CreateCubeSec(s: TJKSector; x, y, z: double;
  const pnormal, edge: TVector);

procedure RotatePoint(ax1, ay1, az1, ax2, ay2, az2: double; angle: double;
  var x, y, z: double);

procedure DuplicateSector(sec, newsc: TJKSector; cx, cy, cz: double;
  nx, ny, nz: TVector; dx, dy, dz: double);

procedure GetLevelPal(l: TJKLevel; var pal: TCMPPal);

procedure OffsetByNode(const lev: TJKLevel; nnode: integer; var x, y, z: double);
procedure UnOffsetByNode(const lev: TJKLevel; nnode: integer; var x, y, z: double);

// index bound checking functions
function CheckSectorIndex(const lev: TJKLevel; sc: integer): boolean;
function CheckSectorSurfaceIndex(const sec: TJKSector; sf: integer): boolean;
function CheckSectorVertexIndex(const sec: TJKSector; vx: integer): boolean;
function CheckSurfaceIndex(const lev: TJKLevel; sc, sf: integer): boolean;
function CheckSurfaceEdgeIndex(const surf: TJKSurface; ed: integer): boolean;
function CheckEdgeIndex(const lev: TJKLevel; sc, sf, ed: integer): boolean;
function CheckVertexIndex(const lev: TJKLevel; sc, vx: integer): boolean;
function CheckTemplateIndex(const lev: TJKLevel; tmpl: integer): boolean;
function CheckThingIndex(const lev: TJKLevel; th: integer): boolean;
function CheckFrameIndex(const lev: TJKLevel; th, fr: integer): boolean;
function CheckCogIndex(const lev: TJKLevel; co: integer): boolean;
function CheckLightIndex(const lev: TJKLevel; lt: integer): boolean;

// Function snaprs point x,y to XY axis based on origin point ox,oy based on which coord is closest to the origin point.
// Returns either x,oy if y is closest to oy or ox,y if x is closest to the ox.
procedure SnapPointToAxis(const x, y: Integer; ox, oy: Integer; var sx, sy: Integer); overload;
procedure SnapPointToAxis(const point: TPoint; origin: TPoint; var spoint: TPoint); overload;
procedure SnapPointToAxis(const x, y: Integer; origin: TPoint;  var sx, sy: Integer); overload;

implementation

uses System.Math, SysUtils, GlobalVars, Jed_main, math, u_tbar, values,
     u_COGForm, U_Options, u_undo;

Type
  TXData = record
    un, vn: TVector;
    x, y, z, u, v: double;
  end;

function MaxSurfaceVertices(const lev: TJKLevel): integer;
begin
  Result := IfThen(lev.kind = IJIM, ijim_MaxSurfVerts, df_MaxSurfVerts);
end;

Procedure SaveTXData(const surf: TJKSurface; vx: integer; var txd: TXData); forward;
Procedure ApplyTXDataToVX(const surf: TJKSurface; vx: integer; const txd: TXData); forward;
Procedure ApplyTXData(const surf: TJKSurface; const txd: TXData); forward;

Procedure GetLevelPal(l: TJKLevel; var pal: TCMPPal);
begin
  pal := defcmppal;
  if level.MasterCMP = '' then
    if level.sectors.count < 1 then
      exit
    else
    begin
      LoadCmpPal(level.sectors[0].colormap, pal);
      exit;
    end;
  LoadCmpPal(level.MasterCMP, pal);
end;

Function NextIdx(n, maxn: integer): integer;
begin
  if n = maxn - 1 then
    result := 0
  else
    result := n + 1;
end;

Function PrevIdx(n, maxn: integer): integer;
begin
  if n = 0 then
    result := maxn - 1
  else
    result := n - 1;
end;

Function SysIsInSector(const sec: TJKSector; x, y, z: double;
  onsides: boolean): boolean;
var
  sf: integer;
  dist: double;
  v: TJKVertex;
begin
  result := false;
  for sf := 0 to sec.surfaces.Count - 1 do
    with sec.surfaces[sf] do
    begin
      v := Vertices[0];
      dist := normal.Dot(x - v.x, y - v.y, z - v.z);//SMult(normal.dx, normal.dy, normal.dz, x - v.x, y - v.y, z - v.z);
      case onsides of
        true:
          if dist < -0.00001 then
            exit;
        false:
          if dist <= 0.001 then
            exit;
      end;

    end;
  result := true;
end;

Function IsInSector(const sec: TJKSector; x, y, z: double): boolean;
begin
  result := SysIsInSector(sec, x, y, z, true);
end;

Function IsInSecNS(const sec: TJKSector; x, y, z: double): boolean;
begin
  result := SysIsInSector(sec, x, y, z, false);
end;

Function DoSectorsOverlap(const sec1, sec2: TJKSector): boolean;

  Function DoThey(const sec1, sec2: TJKSector): boolean;
  var
    onright: boolean;
  begin
    result := false;

    for var i := 0 to sec1.surfaces.Count - 1 do
    begin
      var surf := sec1.surfaces[i];
      if surf.vertices.Count < 1 then
        continue;

      var v0 := surf.Vertices[0];
      onright := false;

      for var j := 0 to sec2.vertices.Count - 1 do
      begin
//        With surf.normal do
//          D := VectorDot3(v.x - v0.x, v.y - v0.y, v.z - v0.z, dx, dy, dz); //SMult
        var v := sec2.vertices[j];
        var D := surf.normal.Dot(v.x - v0.x, v.y - v0.y, v.z - v0.z);
        if D > 0.01 then
        begin
          onright := true;
          break;
        end;
      end;

      if not onright then
        exit;
    end;
    result := true;
  end;

begin
  result := DoThey(sec1, sec2) and DoThey(sec2, sec1);
end;

Procedure FindBBox(const sec: TJKSector; var box: TBox);
var
  i: integer;
begin
  box.x1 := 999999999999.0;
  box.x2 := -999999999999.0;
  box.y1 := 999999999999.0;
  box.y2 := -999999999999.0;
  box.z1 := 999999999999.0;
  box.z2 := -999999999999.0;

  For i := 0 to sec.Vertices.count - 1 do
    With sec.Vertices[i] do
    begin
      if x < box.x1 then
        box.x1 := x;
      if x > box.x2 then
        box.x2 := x;
      if y < box.y1 then
        box.y1 := y;
      if y > box.y2 then
        box.y2 := y;
      if z < box.z1 then
        box.z1 := z;
      if z > box.z2 then
        box.z2 := z;
    end;
end;

Procedure FindCenter(const sec: TJKSector; var cx, cy, cz: double);
var
  XSum, Ysum, ZSum: double;
  i: integer;
begin
  XSum := 0;
  Ysum := 0;
  ZSum := 0;
  For i := 0 to sec.Vertices.count - 1 do
    With sec.Vertices[i] do
    begin
      XSum := XSum + x;
      Ysum := Ysum + y;
      ZSum := ZSum + z;
    end;

  cx := XSum / sec.Vertices.count;
  cy := Ysum / sec.Vertices.count;
  cz := ZSum / sec.Vertices.count;
end;

Procedure FindBSphere(const sec: TJKSector; var cx, cy, cz, Rad: double);
var
  mdist: double;
  i: integer;
begin
  FindCenter(sec, cx, cy, cz);
  Rad := 0;

  For i := 0 to sec.Vertices.count - 1 do
    With sec.Vertices[i] do
    begin
      mdist := sqrt(sqr(x - cx) + sqr(y - cy) + sqr(z - cz));
      if mdist > Rad then
        Rad := mdist;
    end;

end;

Const
  cbSteps = 25;

Function FindCollideBox(const sec: TJKSector; const bbox: TBox; cx, cy, cz: double;
  var cbox: TBox): boolean;
var
  xplus, xminus, yplus, yminus, zplus, zminus: double;
  x1, x2, y1, y2, z1, z2: double;
  bx1, bx2, by1, by2, bz1, bz2: boolean;
  i: integer;
begin
  result := false;

  bx1 := true;
  bx2 := true;
  by1 := true;
  by2 := true;
  bz1 := true;
  bz2 := true;

  xminus := (bbox.x1 - cx) / cbSteps;
  xplus := (bbox.x2 - cx) / cbSteps;
  yminus := (bbox.y1 - cy) / cbSteps;
  yplus := (bbox.y2 - cy) / cbSteps;
  zminus := (bbox.z1 - cz) / cbSteps;
  zplus := (bbox.z2 - cz) / cbSteps;

  x1 := cx + xminus;
  x2 := cx + xplus;
  y1 := cy + yminus;
  y2 := cy + yplus;
  z1 := cz + zminus;
  z2 := cz + zplus;

  if not IsInSecNS(sec, x1, y1, z1) then
    exit;
  if not IsInSecNS(sec, x1, y1, z2) then
    exit;
  if not IsInSecNS(sec, x1, y2, z2) then
    exit;
  if not IsInSecNS(sec, x1, y2, z1) then
    exit;

  if not IsInSecNS(sec, x2, y1, z1) then
    exit;
  if not IsInSecNS(sec, x2, y1, z2) then
    exit;
  if not IsInSecNS(sec, x2, y2, z2) then
    exit;
  if not IsInSecNS(sec, x2, y2, z1) then
    exit;

  For i := 0 to cbSteps - 1 do
  begin
    if bx1 then
      Repeat
        x1 := x1 + xminus;
        if IsInSecNS(sec, x1, y1, z1) and IsInSecNS(sec, x1, y1, z2) and
          IsInSecNS(sec, x1, y2, z2) and IsInSecNS(sec, x1, y2, z1) then
          break;
        x1 := x1 - xminus;
        bx1 := false;
      Until true;

    if bx2 then
      Repeat
        x2 := x2 + xplus;
        if IsInSecNS(sec, x2, y1, z1) and IsInSecNS(sec, x2, y1, z2) and
          IsInSecNS(sec, x2, y2, z2) and IsInSecNS(sec, x2, y2, z1) then
          break;
        x2 := x2 - xplus;
        bx2 := false;
      Until true;

    if by1 then
      Repeat
        y1 := y1 + yminus;
        if IsInSecNS(sec, x1, y1, z1) and IsInSecNS(sec, x1, y1, z2) and
          IsInSecNS(sec, x2, y1, z2) and IsInSecNS(sec, x2, y1, z1) then
          break;
        y1 := y1 - yminus;
        by1 := false;
      Until true;

    if by2 then
      Repeat
        y2 := y2 + yplus;
        if IsInSecNS(sec, x1, y2, z1) and IsInSecNS(sec, x1, y2, z2) and
          IsInSecNS(sec, x2, y2, z2) and IsInSecNS(sec, x2, y2, z1) then
          break;
        y2 := y2 - xplus;
        by2 := false;
      Until true;

    if bz1 then
      Repeat
        z1 := z1 + zminus;
        if IsInSecNS(sec, x1, y1, z1) and IsInSecNS(sec, x1, y2, z1) and
          IsInSecNS(sec, x2, y2, z1) and IsInSecNS(sec, x2, y1, z1) then
          break;
        z1 := z1 - zminus;
        bz1 := false;
      Until true;

    if bz2 then
      Repeat
        z2 := z2 + zplus;
        if IsInSecNS(sec, x1, y1, z2) and IsInSecNS(sec, x1, y2, z2) and
          IsInSecNS(sec, x2, y2, z2) and IsInSecNS(sec, x2, y1, z2) then
          break;
        z2 := z2 - zplus;
        bz2 := false;
      Until true;

    if not(bx1 or bx2 or by1 or by2 or bz1 or bz2) then
      break;

  end;
  cbox.x1 := x1;
  cbox.x2 := x2;
  cbox.y1 := y1;
  cbox.y2 := y2;
  cbox.z1 := z1;
  cbox.z2 := z2;
  result := true;
end;

function GetAngle(x, y: double; xa, ya, xb, yb: double): double;
{ calculates angle formed by the X,Z and vertices of the wall }
var { square of triangle sides }
  D: double;
  ax, ay, bx, by: double;
begin
  ax := xa - x;
  ay := ya - y;
  bx := xb - x;
  by := yb - y;
  D := (ax * bx + ay * by) /
    (sqrt(sqr(ax) + sqr(ay)) * sqrt(sqr(bx) + sqr(by)));
  Try
    result := ArcCos(D);
  except
    on EInvalidOp do
      if D > 1 then
        result := 0
      else
        result := Pi;
  end;
  { Find the sign of the angle }
  if Isabove180(x, y, xa, ya, xb, yb) then
    result := 2 * Pi - result;
end;

(* function GetAngle(x,y:Double;xa,ya,xb,yb:double):Double;
  {calculates angle formed by the X,Z and vertices of the wall}
  var a2,b2,c2:Double; {square of triangle sides}
  d:Double;
  begin
  a2:=Sqr(xa-x)+sqr(ya-y);
  b2:=Sqr(xb-x)+sqr(ya-y);
  c2:=Sqr(xb-xa)+sqr(yb-ya);
  d:=(a2-c2+b2)/(2*sqrt(b2));
  Try
  Result:=ArcCos(d/sqrt(a2));
  except
  on EInvalidOp do if d/sqrt(a2)>1 then Result:=0 else Result:=Pi;
  end;
  {Find the sign of the angle}
  if IsAbove180(x,y,xa,ya,xb,yb) then Result:=2*pi-Result;
  end; *)

Function Isabove180(x, y: double; xa, ya, xb, yb: double): boolean;
var
  x1, y1, x2, y2: double;
begin
  x1 := x - xb;
  x2 := x - xa;
  y1 := y - yb;
  y2 := y - ya;
  result := (x1 * y2 - x2 * y1) > 0;
end;

Function DoIntersect(x11, y11, x12, y12, x21, y21, x22, y22: double): boolean;
var
  b, D, x, y, dy1, dy2, dx1, dx2: double;
begin
  result := false;
  dy1 := y12 - y11;
  dy2 := y22 - y21;
  dx1 := x12 - x11;
  dx2 := x22 - x21;

  D := dy1 * dx2 - dx1 * dy2;
  if Abs(D) < 0.0001 then
    exit;

  b := (dx1 * (y21 - y11) + dy1 * (x11 - x21)) / D;
  y := y21 + dy2 * b;
  x := x21 + dx2 * b;
  result := (y >= real_min(y11, y12)) and (y <= real_max(y11, y12)) and
    (y >= real_min(y21, y22)) and (y <= real_max(y21, y22));

  result := result and (x >= real_min(x11, x12)) and (x <= real_max(x11, x12))
    and (x >= real_min(x21, x22)) and (x <= real_max(x21, x22));

end;

Procedure CalcNormal(const surf: TPolygon; var normal: TVector);
begin
  // Newell's method to calculate polygon normal
  normal := TVector.zero;
  for var i := 0 to surf.vertices.Count - 1 do
    begin
      var vc := surf.vertices[i];
      var vn := surf.vertices[(i + 1) mod surf.vertices.Count];
      normal.x := normal.x + ((vc.y - vn.y) * (vc.z + vn.z));
      normal.y := normal.y + ((vc.z - vn.z) * (vc.x + vn.x));
      normal.z := normal.z + ((vc.x - vn.x) * (vc.y + vn.y));
    end;
  normal.Normalize;
end;


(* Function FindTri(surf:TJKSurface;nv1:integer;var nv2,nv3:integer):boolean;
  var i:integer;
  v0,v1,v2:TJKVertex;
  n,nvxs:integer;
  a:Tvector;
  begin
  result:=false;
  if surf.vertices.count<3 then exit;
  v0:=surf.Vertices[nv1];
  nvxs:=Surf.Vertices.count-1;

  n:=Surf.NextVX(nv1);
  for i:=0 to nvxs-1 do
  begin
  v1:=surf.Vertices[n];
  if (sqr(v1.x-v0.x)+sqr(v1.y-v0.y)+sqr(v1.z-v0.z))>1e-10 then
  begin
  Result:=true;
  nv2:=n;
  break;
  end;
  n:=Surf.NextVX(n);
  dec(nvxs);
  end;

  if not result then exit;

  v1:=surf.Vertices[nv2];
  result:=false;
  n:=Surf.NextVX(nv2);

  for i:=0 to nvxs-1 do
  begin
  v2:=surf.Vertices[n];
  VMult(v0.x-v1.x,v0.y-v1.y,v0.z-v1.z,
  v2.x-v1.x,v2.y-v1.y,v2.z-v1.z,
  a.dx,a.dy,a.dz);

  if Sqr(a.dx)+sqr(a.dy)+sqr(a.dz)>1e-18 then
  begin
  Result:=true;
  nv3:=n;
  break;
  end;
  dec(nvxs);
  n:=Surf.NextVX(n);
  end;

  end; *)

Function FindTri(surf: TJKSurface; nv1: integer; var nv2, nv3: integer)
  : boolean;
var
  i: integer;
  v0, v1, v2: TJKVertex;
  n, nvxs: integer;
  a: TVector;
  cdist, dist: double;
  cn: integer;
begin
  result := False;
  if surf.vertices.Count < 3 then
    exit;

  v0 := surf.vertices[nv1];
  nvxs := surf.vertices.Count - 1;

  n     := surf.NextVX(nv1);
  cdist := -1;
  cn    := -1;

  for i := 0 to nvxs - 1 do
  begin
    v1 := surf.vertices[n];

    dist := sqr(v1.x - v0.x) + sqr(v1.y - v0.y) + sqr(v1.z - v0.z);
    if dist > cdist then
    begin
      cn := n;
      cdist := dist;
    end;

    if (dist) > 1E-4 then
    begin
      result := True;
      nv2 := n;
      break;
    end;
    n := surf.NextVX(n);
    dec(nvxs);
  end;

  if not result then
    if cn = -1 then
      exit
    else
      nv2 := cn;

  v1 := surf.vertices[nv2];
  result := False;
  n := surf.NextVX(nv2);

  { Calculate normalized vector perpendicular to the
    selected edge }

  with surf do
    VectorCross3(v1.x - v0.x, v1.y - v0.y, v1.z - v0.z, normal.dx, normal.dy,
      normal.dz, a.dx, a.dy, a.dz); //VMult

  if a.Normalize = 0 then
    exit;

  cdist := -1;
  cn := -1;

  for i := 0 to nvxs - 1 do
  begin
    v2 := surf.vertices[n];

    dist := Abs(a.Dot(v2.x - v1.x, v2.y - v1.y, v2.z - v1.z)); //Abs(SMult(a.dx, a.dy, a.dz, v2.x - v1.x, v2.y - v1.y, v2.z - v1.z));
    if dist > cdist then
    begin
      cn := n;
      cdist := dist;
    end;

    if dist > 1E-4 then
    begin
      result := True;
      nv3 := n;
      break;
    end;
    n := surf.NextVX(n);
  end;

  if not result then
    if cn <> -1 then
    begin
      nv3 := cn;
      result := True;
    end;
end;

Function IsTXFlipped(const surf: TJKSurface): boolean;
var
  nv1, nv2, nv3: integer;
  v1, v2, v3: TJKVertex;
  tv1, tv2, tv3: TTXVertex;
begin
  result := false;
  nv1 := 0;
  if not FindTri(surf, nv1, nv2, nv3) then
    exit;

  v1 := surf.vertices[nv1];
  v2 := surf.vertices[nv2];
  v3 := surf.vertices[nv3];
  tv1 := surf.txvertices[nv1];
  tv2 := surf.txvertices[nv2];
  tv3 := surf.txvertices[nv3];

  { triangle v1,v2,v3 define the normal, thus angle v1,v2,v3 is
    always <180 }

  { Check if angle is <180 in u,V }
  { if u1*v2>u2*v1 - >180 }
  result := (tv1.u - tv2.u) * (tv3.v - tv2.v) > (tv3.u - tv2.u) *
    (tv1.v - tv2.v);
end;

Function IsOnLine(x1, y1, z1, x2, y2, z2, x, y, z: double): boolean;
var
  dx, dy, dz: double;
  nx, ny, nz: double;
begin
  dx := x2 - x1;
  dy := y2 - y1;
  dz := z2 - z1;

  if not IsClose(dz, 0) then
    begin
      nx := x1 + (z - z1) * dx / dz;
      ny := y1 + (z - z1) * dy / dz;
      nz := z;
    end
  else if not IsClose(dx, 0) then
    begin
      nx := x;
      ny := y1 + (x - x1) * dy / dx;
      nz := z1 + (x - x1) * dz / dx;
  end
  else
    begin
      nx := x1 + (y - y1) * dz / dy;
      ny := y;
      nz := z1 + (y - y1) * dz / dy;
    end;

  result := IsClose(nx, x) and IsClose(ny, y) and IsClose(nz, z);
end;

Function DistToPlane(const surf: TPolygon; x, y, z: double): double;
var
  vx: TVertex;
begin
  vx := surf.vertices[0];
  With surf do
    Result := Abs(normal.Dot(x - vx.x, y - vx.y, z - vx.z));//Abs(SMult(normal.dx, normal.dy, normal.dz, x - vx.x, y - vx.y, z - vx.z));
end;

{ Function DistToPlane(surf:TPolygon;x,y,z:double):double;
  var px,py,pz:double;
  b2c2:double;
  a,b,c,d:double;
  begin
  a:=surf.normal.dx; b:=surf.normal.dy; c:=surf.normal.dz;
  d:=surf.d;
  b2c2:=sqr(b)+sqr(c);
  px:=(a*(d-c*z-b*y)+x*b2c2)/(sqr(a)+b2c2);

  if IsClose(a,0) then
  begin
  py:=y;
  pz:=z;
  end
  else
  begin
  py:=b/a*(px-x)+y;
  pz:=c/a*(px-x)+z;
  end;
  result:=Sqrt(sqr(px-x)+sqr(py-y)+sqr(pz-z));
  end; }

Function PlaneLineXn(const normal: TVector; D: double;
  x1, y1, z1, x2, y2, z2: double; var x, y, z: double): boolean;
var
  dx, dy, dz: double;
  sm: double;
begin
  dx := x2 - x1;
  dy := y2 - y1;
  dz := z2 - z1;
  result := false;

  sm := (normal.dx * dx + normal.dy * dy + normal.dz * dz);
  { if Abs(sm)<0000001 then exit; }
  try
    if Not IsClose(dx, 0) then
    begin
      x := (x1 * (normal.dy * dy + normal.dz * dz) - dx *
        (normal.dy * y1 + normal.dz * z1 - D)) / sm;
      y := y1 + dy / dx * (x - x1);
      z := z1 + dz / dx * (x - x1);
      result := true;
    end
    else if Not IsClose(dy, 0) then
    begin
      y := (y1 * (normal.dx * dx + normal.dz * dz) - dy *
        (normal.dx * x1 + normal.dz * z1 - D)) / sm;
      x := x1 + dx / dy * (y - y1);
      z := z1 + dz / dy * (y - y1);
      result := true;
      exit;
    end
    else if Not IsClose(dz, 0) then
    begin
      z := (z1 * (normal.dx * dx + normal.dy * dy) - dz *
        (normal.dx * x1 + normal.dy * y1 - D)) / sm;
      y := y1 + dy / dz * (z - z1);
      x := x1 + dx / dz * (z - z1);
      result := true;
      exit;
    end
    else
    begin
      result := false;
      exit;
    end;
    result := true;
  except
    On Exception do;
  end;
end;

Function PlaneLineXnNew(const normal: TVector; pX, pY, pZ, x1, y1, z1, x2, y2,
  z2: double; var x, y, z: double): boolean;
var
  dist1, dist2: double;
  k: double;
begin
  result := false;
  dist1 := VectorDot3(x1 - pX, y1 - pY, z1 - pZ, normal.dx, normal.dy, normal.dz); //SMult
  dist2 := VectorDot3(x2 - pX, y2 - pY, z2 - pZ, normal.dx, normal.dy, normal.dz);
  if dist1 = dist2 then
    exit;

  try
    k := dist1 / (dist1 - dist2);
  except
    on Exception do
      exit;
  end;
  x := x1 + k * (x2 - x1);
  y := y1 + k * (y2 - y1);
  z := z1 + k * (z2 - z1);
  result := true;
end;

Procedure ClearVXMarks(lev: TJKLevel);
var
  s, v: integer;
begin
  for s := 0 to lev.sectors.count - 1 do
    With lev.sectors[s] do
      for v := 0 to Vertices.count - 1 do
        Vertices[v].Mark := 0;
end;

Procedure ClearSCMarks(lev: TJKLevel);
var
  s: integer;
begin
  for s := 0 to lev.sectors.count - 1 do
    lev.sectors[s].Mark := 0;
end;

Procedure TranslateVX(v: TJKVertex; dx, dy, dz: double);
begin
  if v.mark <> 0 then
    exit;
  v.x := v.x + dx;
  v.y := v.y + dy;
  v.z := v.z + dz;
  v.mark := 1;
end;

Procedure RecalcSector(sec: TJKSector);
var
  sf, sf1: integer;
begin
  ClearSCMarks(sec.level);
  for sf := 0 to sec.surfaces.Count - 1 do
    with sec.surfaces[sf] do
    begin
      RecalcAll;
      if adjoin = nil then
        continue;
      if adjoin.sector.Mark <> 0 then
        continue;
      for sf1 := 0 to adjoin.sector.surfaces.Count - 1 do
        adjoin.sector.surfaces[sf1].RecalcAll;
      adjoin.sector.mark := 0;
    end;
end;

Procedure TranslateSectors(lev: TJKLevel; scsel: TSCMultiSel; cursc: integer;
  dx, dy, dz: double);
{ Procedure TranslateSector(sec:TJKSector;dx,dy,dz:double); }

  Procedure MoveSec(sec: TJKSector; dx, dy, dz: double);
  var
    sf1, sf, v, i: integer;
  begin

    // First check that all affected sectors are on visible layer
    if not ToolBar.IsLayerVisible(sec.layer) then
      exit;
    for sf := 0 to sec.surfaces.count - 1 do   // check adjoin sectors
      with sec.surfaces[sf] do
        if (adjoin <> nil) and (not ToolBar.IsLayerVisible(adjoin.sector.layer)) then
          exit;

    // Now move sector(s)
    SaveSecUndo(sec, ch_changed, sc_geo);

    for v := 0 to sec.vertices.count - 1 do
      TranslateVX(sec.vertices[v], dx, dy, dz);

    for sf := 0 to sec.surfaces.count - 1 do
      with sec.surfaces[sf] do
      begin
        if adjoin = nil then
          continue;

        SaveSecUndo(adjoin.sector, ch_changed, sc_geo);
        for v := 0 to adjoin.vertices.count - 1 do
          TranslateVX(adjoin.vertices[v], dx, dy, dz);
      end;

    RecalcSector(sec);
    JedMain.SectorChanged(sec);

    for i := 0 to sec.surfaces.count - 1 do
      with sec.surfaces[i] do
      begin
        if adjoin = nil then
          continue;
        JedMain.SectorChanged(adjoin.sector);
      end;
  end;

var
  i: integer;
begin
  ClearVXMarks(lev);
  if CheckSectorIndex(lev, cursc) and (scsel.FindSC(cursc) = -1) then
    MoveSec(lev.sectors[cursc], dx, dy, dz);

  for i := 0 to scsel.Count - 1 do
    MoveSec(lev.sectors[scsel.getSC(i)], dx, dy, dz);
end;

Procedure TranslateSurfaces(lev: TJKLevel; sfsel: TSFMultiSel;
  cursc, cursf: integer; dx, dy, dz: double);

  Procedure MoveSurf(surf: TJKSurface);
  var
    v, sf: integer;
  begin
    // Prevent moving surface if surface or it's adjoin are not on visable layer
    if not ToolBar.IsLayerVisible(surf.sector.layer) then
      exit;
    if (surf.adjoin <> nil) and (not ToolBar.IsLayerVisible(surf.adjoin.sector.layer)) then
      exit;

    // Nove do the surface move
    SaveSecUndo(surf.sector, ch_changed, sc_geo);

    for v := 0 to surf.Vertices.count - 1 do
      TranslateVX(surf.Vertices[v], dx, dy, dz);

    if surf.adjoin <> nil then
    begin
      SaveSecUndo(surf.adjoin.sector, ch_changed, sc_geo);
      for v := 0 to surf.adjoin.vertices.count - 1 do
        TranslateVX(surf.adjoin.vertices[v], dx, dy, dz);
    end;

    for sf := 0 to surf.sector.surfaces.count - 1 do
      surf.sector.surfaces[sf].RecalcAll;

    if surf.adjoin <> nil then
      for sf := 0 to surf.adjoin.sector.surfaces.count - 1 do
        surf.adjoin.sector.surfaces[sf].RecalcAll;

    JedMain.SectorChanged(surf.sector);
    if surf.adjoin <> nil then
      JedMain.SectorChanged(surf.adjoin.sector);
  end;

var
  i: integer;
  sc, sf: integer;
begin
  ClearVXMarks(lev);
  if CheckSurfaceIndex(lev, cursc, cursf) and (sfsel.FindSF(cursc, cursf) = -1) then
    MoveSurf(Level.sectors[cursc].surfaces[cursf]);

  for i := 0 to sfsel.count - 1 do
  begin
    sfsel.GetSCSF(i, sc, sf);
    MoveSurf(level.sectors[sc].surfaces[sf]);
  end;
end;

Procedure TranslateEdges(lev: TJKLevel; edsel: TEDMultiSel;
  cursc, cursf, cured: integer; dx, dy, dz: double);

  Procedure MoveED(surf: TJKSurface; ed: integer);
  var
    nv: integer;
  begin
    // Prevent moving edge if not on visible surface
    if not ToolBar.IsLayerVisible(surf.sector.layer) then
      exit;

    SaveSecUndo(surf.sector, ch_changed, sc_geo);

    TranslateVX(surf.vertices[ed], dx, dy, dz);
    TranslateVX(surf.vertices[surf.NextVX(ed)], dx, dy, dz);
    for nv := 0 to surf.sector.surfaces.count - 1 do
      surf.sector.surfaces[nv].RecalcAll;
    JedMain.SectorChanged(surf.sector);
  end;

var
  i: integer;
  sc, sf, ed: integer;
begin
  ClearVXMarks(lev);
  if CheckEdgeIndex(level, cursc, cursf, cured) and (edsel.FindED(cursc, cursf, cured) = -1) then
    MoveED(lev.sectors[cursc].surfaces[cursf], cured);

  for i := 0 to edsel.count - 1 do
  begin
    edsel.GetSCSFED(i, sc, sf, ed);
    MoveED(lev.sectors[sc].surfaces[sf], ed);
  end;
end;

Procedure TranslateVertices(lev: TJKLevel; vxsel: TVXMultiSel;
  cursc, curvx: integer; dx, dy, dz: double);

  Procedure MoveVX(vx: TJKVertex);
  var
    nv: integer;
  begin
    // Prevent moving vertex if not on visible surface
    if not ToolBar.IsLayerVisible(vx.sector.layer) then
      exit;

    SaveSecUndo(vx.sector, ch_changed, sc_geo);

    TranslateVX(vx, dx, dy, dz);
    for nv := 0 to vx.sector.surfaces.count - 1 do
      vx.sector.surfaces[nv].RecalcAll;
    JedMain.SectorChanged(vx.sector);
  end;

var
  i: integer;
  sc, vx: integer;
begin
  ClearVXMarks(lev);
  if  CheckVertexIndex(level, cursc, curvx) and (vxsel.FindVX(cursc, curvx) = -1) then
    MoveVX(lev.sectors[cursc].vertices[curvx]);

  for i := 0 to vxsel.count - 1 do
  begin
    vxsel.GetSCVX(i, sc, vx);
    MoveVX(lev.sectors[sc].vertices[vx]);
  end;
end;

{ Procedure ClearVXMarks(lev:TJKLevel);
  var s,v:integer;
  begin
  for s:=0 to Lev.Sectors.Count-1 do
  With Lev.Sectors[s] do
  for v:=0 to Vertices.Count-1 do Vertices[v].Mark:=0;
  end; }

Procedure TranslateThingKeepFrame(lev: TJKLevel; curth: integer;
  dx, dy, dz: double);
var
  thing: TJKThing;
begin
  if not CheckThingIndex(lev, curth) then
    exit;

  thing := level.things[curth];
  if not ToolBar.IsLayerVisible(thing.layer) then // Prevent moving thing on invisible layer
    exit;

  SaveThingUndo(thing, ch_changed);
  With thing do
  begin
    x := x + dx;
    y := y + dy;
    z := z + dz;
  end;
  JedMain.ThingChanged(thing);
  JedMain.LayerThing(curth);
end;

Procedure TranslateThings(lev: TJKLevel; thsel: TTHMultiSel; curth: integer;
  dx, dy, dz: double; moveframes: boolean);

  procedure MoveTH(thing: TJKThing);
  var
    tx, ty, tz, pch, yaw, rol: double;
    i: integer;
  begin
    // Prevent moving thing on invisible layer
    if not ToolBar.IsLayerVisible(thing.layer) then
      exit;

    SaveThingUndo(thing, ch_changed);
    with thing do
    begin
      x := x + dx;
      y := y + dy;
      z := z + dz;
    end;

    if moveframes then
      For i := 0 to thing.vals.count - 1 do
        With thing.vals[i] do
        begin
          if atype <> at_frame then
            continue;
          GetFrame(tx, ty, tz, pch, yaw, rol);
          SetFrame(tx + dx, ty + dy, tz + dz, pch, yaw, rol);
        end;

    JedMain.ThingChanged(thing);
    JedMain.LayerThing(thing.num);
  end;

var
  i: integer;
begin
  if CheckThingIndex(lev, curth) and (thsel.FindTH(curth) = -1) then
    MoveTH(lev.Things[curth]);

  for i := 0 to thsel.count - 1 do
    MoveTH(lev.Things[thsel.GetTH(i)]);
end;

Procedure TranslateFrames(lev: TJKLevel; frsel: TFRMultisel; th, fr: integer;
  dx, dy, dz: double);
var
  vl: TTPLValue;
  i, n: integer;
  x, y, z, pch, yaw, rol: double;
begin
  if not CheckThingIndex(lev, th) then
    exit;

  n := frsel.AddFR(th, fr);
  for i := 0 to frsel.count - 1 do
  begin
    frsel.GetTHFR(i, th, fr);
    var thing := lev.things[th];

    // Prevent moving thing on invisible layer
    if not ToolBar.IsLayerVisible(thing.layer) then
      continue;

    vl := lev.things[th].vals[fr];
    SaveThingUndo(thing, ch_changed);
    vl.GetFrame(x, y, z, pch, yaw, rol);
    vl.SetFrame(x + dx, y + dy, z + dz, pch, yaw, rol);
    JedMain.ThingChanged(thing);
  end;

  frsel.DeleteN(n);
end;

Procedure TranslateLights(lev: TJKLevel; ltsel: TLTMultiSel; curlt: integer;
  dx, dy, dz: double);

  procedure MoveLT(light: TSedLight);
  begin
     // Prevent moving light on invisible layer
    if not ToolBar.IsLayerVisible(light.layer) then
      exit;

    SaveLightUndo(light, ch_changed);
    with light do
    begin
      position.x := position.x + dx;
      position.y := position.y + dy;
      position.z := position.z + dz;
    end;
    JedMain.LightChanged(light);
  end;

var
  i: integer;
begin
  if CheckLightIndex(lev, curlt) and (ltsel.FindLT(curlt) = -1) then
    MoveLT(lev.Lights[curlt]);

  for i := 0 to ltsel.count - 1 do
    MoveLT(lev.lights[ltsel.GetLT(i)]);
end;

Procedure SetAdjoinParams(sf: TJKSurface);
begin
  sf.SurfFlags := 0;
  sf.FaceFlags := 0;
  sf.geo       := 0;
  sf.AdjoinFlags := SAF_Visible or SAF_Move;
  if CurrentProject <> IJIM then
    BitSet(sf.AdjoinFlags, SAF_AllowSoundPass);
end;

Procedure ClearAdjoinParams(sf: TJKSurface);
begin
  sf.SurfFlags := SF_Collision;
  sf.FaceFlags := 0;
  sf.geo       := 4;
  if sf.material = '' then
    sf.material := 'dflt.mat';
end;

Procedure SysAdjoinSurfaces(surf1, surf2: TJKSurface);
begin
  surf1.adjoin := surf2;
  surf2.adjoin := surf1;
  SetAdjoinParams(surf1);
  SetAdjoinParams(surf2);
end;

Procedure ExtrudeAndExpandSurface(surf: TJKSurface; by: double);
var
  lev: TJKLevel;
  nsec: TJKSector;
  asurf, osurf, nsurf: TJKSurface;
  nvx: TJKVertex;

  v1, v2, v3, v4: TJKVertex;

  i, v, vc: integer;

  cx, cy, cz: double;
  cnx, cny, cnz: double;

begin
  if surf.adjoin <> nil then
    exit;

  { CalcSurfCenterNCorner(surf,cx,cy,cz,cnx,cny,cnz); }

  lev := surf.sector.Level;
  nsec := lev.NewSector;
  nsec.Assign(surf.sector);
  nsec.Num := lev.sectors.Count;

  SaveSecUndo(nsec, ch_added, sc_both);
  SaveSecUndo(surf.sector, ch_changed, sc_both);

  { Construct adjoin }
  asurf := nsec.NewSurface;
  asurf.Assign(surf);

  v1 := nsec.NewVertex;
  v2 := nsec.NewVertex;
  v3 := nsec.NewVertex;
  v4 := nsec.NewVertex;

  CalcSurfRect(surf, v1, v2, v3, v4, by);

  for v := 0 to 3 do
    begin
      { nvx:=nsec.NewVertex;
        nvx.Assign(surf.vertices[v]); }
      asurf.AddVertex(nsec.Vertices[v]);
    end;
  nsec.surfaces.Add(asurf);

  { SysAdjoinSurfaces(surf,asurf); }

  { Construct opposite surface }
  osurf := nsec.NewSurface;
  osurf.Assign(surf);

  nvx := nsec.NewVertex;
  nvx.x := v1.x + (-surf.normal.dx) * by;
  nvx.y := v1.y + (-surf.normal.dy) * by;
  nvx.z := v1.z + (-surf.normal.dz) * by;

  nvx := nsec.NewVertex;
  nvx.x := v2.x + (-surf.normal.dx) * by;
  nvx.y := v2.y + (-surf.normal.dy) * by;
  nvx.z := v2.z + (-surf.normal.dz) * by;

  nvx := nsec.NewVertex;
  nvx.x := v3.x + (-surf.normal.dx) * by;
  nvx.y := v3.y + (-surf.normal.dy) * by;
  nvx.z := v3.z + (-surf.normal.dz) * by;

  nvx := nsec.NewVertex;
  nvx.x := v4.x + (-surf.normal.dx) * by;
  nvx.y := v4.y + (-surf.normal.dy) * by;
  nvx.z := v4.z + (-surf.normal.dz) * by;

  for v := 0 to 3 do
    begin
      osurf.AddVertex(nsec.vertices[7 - v]);
    end;

  nsec.surfaces.Add(osurf);
  osurf.NewRecalcAll;
  osurf.Planarize;

  { Construct other surfaces }
  vc := 4;

  nsurf := nsec.NewSurface;
  nsurf.Assign(surf);

  for i := 0 to vc - 1 do
  begin
    nsurf := nsec.NewSurface;
    nsurf.Assign(surf);

    nsurf.AddVertex(asurf.vertices[i]);

    if i = 0 then
      nsurf.AddVertex(asurf.vertices[vc - 1])
    else
      nsurf.AddVertex(asurf.vertices[i - 1]);

    if i = 0 then
      nsurf.AddVertex(osurf.vertices[0])
    else
      nsurf.AddVertex(osurf.vertices[vc - i]);

    nsurf.AddVertex(osurf.vertices[vc - i - 1]);
    nsec.surfaces.Add(nsurf);
  end;

  // Update level and new sector
  lev.sectors.Add(nsec);
  lev.RenumSecs;
  for i := 0 to nsec.surfaces.Count - 1 do
    nsec.surfaces[i].NewRecalcAll;
  nsec.Renumber;

  if not ConnectSurfaces(surf, asurf) then;

  JedMain.SectorAdded(nsec);
  JedMain.SectorChanged(surf.sector);
end;

Procedure ExtrudeSurface(surf: TJKSurface; by: double);
var
  lev: TJKLevel;
  nsec: TJKSector;
  asurf, osurf, nsurf: TJKSurface;
  nvx: TJKVertex;
  i, v, vc: integer;
begin
  if surf.adjoin <> nil then
    exit;

  lev := surf.sector.Level;
  nsec := lev.NewSector;
  nsec.Assign(surf.sector);
  nsec.Num := lev.sectors.count;

  SaveSecUndo(nsec, ch_added, sc_both);
  SaveSecUndo(surf.sector, ch_changed, sc_val);

  { Construct adjoin }
  asurf := nsec.newsurface;
  asurf.Assign(surf);
  for v := surf.Vertices.count - 1 downto 0 do
  begin
    nvx := nsec.NewVertex;
    nvx.Assign(surf.Vertices[v]);
    asurf.AddVertex(nvx);
  end;
  nsec.surfaces.Add(asurf);

  { Construct opposite surface }
  osurf := nsec.newsurface;
  osurf.Assign(surf);
  for v := 0 to surf.Vertices.count - 1 do
  begin
    nvx := nsec.NewVertex;
    With surf.Vertices[v] do
    begin
      nvx.x := x + (-surf.normal.dx) * by;
      nvx.y := y + (-surf.normal.dy) * by;
      nvx.z := z + (-surf.normal.dz) * by;
    end;
    osurf.AddVertex(nvx);
  end;
  nsec.surfaces.Add(osurf);
  osurf.NewRecalcAll;
  osurf.Planarize;

  { Construct other surfaces }
  vc := surf.Vertices.count;

  for i := 0 to vc - 1 do
  begin
    nsurf := nsec.newsurface;
    nsurf.Assign(surf);

    nsurf.AddVertex(asurf.Vertices[i]);

    if i = 0 then
      nsurf.AddVertex(asurf.Vertices[vc - 1])
    else
      nsurf.AddVertex(asurf.Vertices[i - 1]);

    if i = 0 then
      nsurf.AddVertex(osurf.Vertices[0])
    else
      nsurf.AddVertex(osurf.Vertices[vc - i]);

    nsurf.AddVertex(osurf.Vertices[vc - i - 1]);
    nsec.surfaces.Add(nsurf);
  end;


  // Update level and new sector
  lev.sectors.Add(nsec);
  lev.RenumSecs;
  for i := 0 to nsec.surfaces.count - 1 do
    nsec.surfaces[i].NewRecalcAll;
  nsec.Renumber;

  // Map adjoins
  SysAdjoinSurfaces(surf, asurf);

  JedMain.SectorAdded(nsec);
  JedMain.SectorChanged(surf.sector);
end;

Type
  TCleaveData = record
    vx: TJKVertex;
  end;

  TCleaveArray = array [0 .. 1000] of TCleaveData;

Procedure SplitVertices(vs: TJKVertices; const cnormal: TVector;
  cx, cy, cz: double);
{ Sets .mark for vertex to
  0 - if vertex is on a plane (defined by cnormal and cx,cy,cz)
  1 - to the right from the plane
  -1 - to the left from then plane }
var
  i: integer;
  dist: double;
begin
  for i := 0 to vs.count - 1 do
    With vs[i] do
    begin
      dist := cnormal.Dot(x - cx, y - cy, z - cz);//SMult(cnormal.dx, cnormal.dy, cnormal.dz, x - cx, y - cy, z - cz);
      if IsClose(dist, 0) then
        Mark := 0
      else if dist < 0 then
        Mark := -1
      else
        Mark := 1;
    end;
end;

Function VXEq(v1, v2: TVertex): boolean;
begin
  result := IsClose(v1.x, v2.x) and IsClose(v1.y, v2.y) and IsClose(v1.z, v2.z);
end;

Procedure InsertVXTintoSurf(surf: TJKSurface; v1, v2: TJKVertex; vx: TJKVertex);
{ Inserts vertex VX into surface Surf between vertices v1 and v2 }
var
  i, j: integer;
  v3, v4: TJKVertex;
  changed: boolean;
  txd: TXData;
  asurf: TJKSurface;
begin
  if surf = nil then
    exit;

  { SaveSecUndo(surf.sector,ch_changed,sc_geo); }

  for j := 0 to surf.sector.surfaces.count - 1 do
    With surf.sector.surfaces[j] do
    begin
      changed := false;
      asurf := surf.sector.surfaces[j];
      SaveTXData(asurf, 0, txd);

      for i := Vertices.count - 1 downto 0 do
      begin
        v3 := Vertices[i];
        v4 := Vertices[NextVX(i)];
        if VXEq(v3, v1) and VXEq(v4, v2) then
        begin
          InsertVertex(NextVX(i), sector.AddVertex(vx.x, vx.y, vx.z));
          changed := true;
        end;

        if VXEq(v3, v2) and VXEq(v4, v1) then
        begin
          InsertVertex(NextVX(i), sector.AddVertex(vx.x, vx.y, vx.z));
          changed := true;
        end;
      end;

      if changed then
        ApplyTXData(asurf, txd);

    end;
  surf.sector.Renumber;
  JedMain.SectorChanged(surf.sector);
end;

Function SysCleaveSurface(surf: TJKSurface; const cnormal: TVector;
  cx, cy, cz: double): TJKSurface;
{ Returns added surface }
var
  lev: TJKLevel;
  i, j, ssf, sf: integer;
  cd: ^TCleaveArray;
  cd1, cd2: ^TCleaveData;
  vc: integer;
  nvxs: integer;
  nsf: TJKSurface;
  D: double;
  v0, v1, v2: TJKVertex;
  nleft, nright: integer;
  dist: double;
  asurf: TJKSurface;
  vxs: TJKVertices;
  ax, ay, az: double;
  rvec: TVector;
  txd, txd1: TXData;
  sfchange: boolean;
begin
  SaveTXData(surf, 0, txd);

  result := nil;

  D := cnormal.Dot(cx, cy, cz);//cnormal.dx * cx + cnormal.dy * cy + cnormal.dz * cz;

  lev := surf.sector.level;
  vc := surf.vertices.Count;
  GetMem(cd, sizeof(TCleaveData) * vc);
  try
    FillChar(cd^, sizeof(TCleaveData) * vc, 0);

    for sf := 0 to vc - 1 do
      begin
        cd1 := @cd^[sf];
        cd2 := @cd^[NextIdx(sf, vc)];
        v1 := surf.vertices[sf];
        v2 := surf.vertices[NextIdx(sf, vc)];

        if (v1.Mark = 0) or (v1.Mark <> -v2.Mark) then
          continue;

        PlaneLineXn(cnormal, D, v1.x, v1.y, v1.z, v2.x, v2.y, v2.z, ax, ay, az);
        cd1^.vx := surf.sector.AddVertex(ax, ay, az);
      end;

    nleft := 0;
    nright := 0;
    for sf := 0 to vc - 1 do
      With surf.vertices[sf] do
        begin
          if mark = 1 then
            inc(nright);
          if mark = -1 then
            inc(nleft);
        end;

    if (nleft = 0) or (nright = 0) then
      exit;

    nsf := surf.sector.NewSurface;
    nsf.Assign(surf);

    { vxs:=TJKVertices.Create; }

    for sf := 0 to surf.vertices.Count - 1 do
      With cd^[sf] do
        begin
          v1 := surf.vertices[sf];
          if (v1.Mark = 1) or (v1.Mark = 0) then
            nsf.AddVertex(v1);
          if vx <> nil then
            nsf.AddVertex(vx);
        end;

    surf.sector.surfaces.Add(nsf);
    nsf.NewRecalcAll;

    vxs := surf.vertices;
    surf.vertices := TJKVertices.Create;

    for sf := 0 to vxs.Count - 1 do
      With cd^[sf] do
        begin
          v1 := vxs[sf];
          if (v1.mark = -1) or (v1.mark = 0) then
            surf.vertices.Add(v1);
          if vx <> nil then
            surf.InsertVertex(surf.vertices.Count, vx);
        end;

    { surf.sector.surfaces.Add(nsf);
      nsf.RecalcAll; }

    surf.RecalcAll;

    { Remove edges that lie on the clipping plane and face the wrong way }

    With surf do { Find "right" vector }
      rvec := normal.Cross(cnormal);
//      VMult(normal.dx, normal.dy, normal.dz, cnormal.dx, cnormal.dy, cnormal.dz,
//        rvec.dx, rvec.dy, rvec.dz);

    For i := surf.vertices.Count - 1 downto 0 do
      begin
        v0 := surf.vertices[surf.PrevVX(i)];
        v1 := surf.vertices[i];
        v2 := surf.vertices[surf.NextVX(i)];
        if (v0.mark <> 0) or (v1.mark <> 0) or (v2.mark <> 0) then
          continue;

        if (VectorDot3(v2.x - v1.x, v2.y - v1.y, v2.z - v1.z, rvec.dx, rvec.dy, rvec.dz) < 0)
          or (VectorDot3(v1.x - v0.x, v1.y - v0.y, v1.z - v0.z, rvec.dx, rvec.dy, rvec.dz) < 0) then //SMult
          surf.vertices.Delete(i);
      end;

    For i := nsf.vertices.Count - 1 downto 0 do
      begin
        v0 := nsf.vertices[nsf.PrevVX(i)];
        v1 := nsf.vertices[i];
        v2 := nsf.vertices[nsf.NextVX(i)];
        if (v0.mark <> 0) or (v1.mark <> 0) or (v2.mark <> 0) then
          continue;

        if (VectorDot3(v2.x - v1.x, v2.y - v1.y, v2.z - v1.z, rvec.dx, rvec.dy, rvec.dz) > 0)
          or (VectorDot3(v1.x - v0.x, v1.y - v0.y, v1.z - v0.z, rvec.dx, rvec.dy, rvec.dz) > 0) then //SMult
          nsf.vertices.Delete(i);
      end;

    For sf := 0 to vc - 1 do
      With cd^[sf] do
      begin
        if vx = nil then
          continue;
        for i := 0 to surf.sector.surfaces.Count - 1 do
        begin
          asurf := surf.sector.surfaces[i];
          if (asurf = surf) then
            continue;

          SaveTXData(asurf, 0, txd1);
          sfchange := false;
          for j := asurf.vertices.Count - 1 downto 0 do
            begin
              v1 := asurf.vertices[j];
              v2 := asurf.vertices[NextIdx(j, asurf.vertices.Count)];
              if (v1 = vxs[sf]) and (v2 = vxs[NextIdx(sf, vxs.Count)]) then
                begin
                  sfchange := true;
                  asurf.InsertVertex(asurf.NextVX(j), vx);
                  InsertVXTintoSurf(asurf.adjoin, vxs[sf],
                    vxs[NextIdx(sf, vxs.Count)], vx);
                end;

              if (v2 = vxs[sf]) and (v1 = vxs[NextIdx(sf, vxs.Count)]) then
                begin
                  sfchange := true;
                  asurf.InsertVertex(NextIdx(j, asurf.vertices.Count), vx);
                  InsertVXTintoSurf(asurf.adjoin, vxs[sf],
                    vxs[NextIdx(sf, vxs.Count)], vx);
                end;

              if sfchange then
                ApplyTXData(asurf, txd1);
            end;
        end;
      end;

    { Surf.vertices.free;
      Surf.Vertices:=vxs; }

    vxs.Free;

    { for i:=0 to nsf.sector.surfaces.count-1 do
      nsf.sector.surfaces[i].RecalcAll; }

    nsf.sector.Renumber;

    ApplyTXData(surf, txd);
    ApplyTXData(nsf, txd);
    result := nsf;

  finally
    FreeMem(cd);
  end;
end;

Procedure CleaveAdjoin(surf: TJKSurface; cnormal: TVector; cx, cy, cz: double;
  nsf: TJKSurface);
var
  sf1, nsf1: TJKSurface;
begin
  if nsf = nil then
    exit;
  if surf.adjoin = nil then
    exit;
  sf1 := surf.adjoin;

  SaveSecUndo(sf1.sector, ch_changed, sc_both);

  sf1.adjoin := nil;
  surf.adjoin := nil;
  SplitVertices(sf1.Vertices, cnormal, cx, cy, cz);

  nsf1 := SysCleaveSurface(sf1, cnormal, cx, cy, cz);

  if DO_Surf_Overlap(surf, sf1) then
  begin
    SysAdjoinSurfaces(surf, sf1);
  end;

  if DO_Surf_Overlap(surf, nsf1) then
  begin
    SysAdjoinSurfaces(nsf1, surf);
  end;

  if DO_Surf_Overlap(nsf, sf1) then
  begin
    SysAdjoinSurfaces(nsf, sf1);
  end;

  if DO_Surf_Overlap(nsf, nsf1) then
  begin
    SysAdjoinSurfaces(nsf, nsf1);
  end;

  JedMain.SectorChanged(surf.sector);
  JedMain.SectorChanged(sf1.sector);
end;

Function CleaveSurface(surf: TJKSurface; const cnormal: TVector;
  cx, cy, cz: double): boolean;
var
  nsf: TJKSurface;
begin
  SplitVertices(surf.Vertices, cnormal, cx, cy, cz);

  SaveSecUndo(surf.sector, ch_changed, sc_both);

  nsf := SysCleaveSurface(surf, cnormal, cx, cy, cz);
  result := nsf <> nil;
  CleaveAdjoin(surf, cnormal, cx, cy, cz, nsf);
  JedMain.SectorChanged(surf.sector);
end;

Function CleaveEdge(surf: TJKSurface; edge: integer; const cnormal: TVector;
  cx, cy, cz: double): boolean;
var
  nv1, nv2, v1, v2: TJKVertex;
  nvx: TJKVertex;
  i, j, n1, n2: integer;
  ax, ay, az: double;
  D: double;
begin
  result := false;
  SplitVertices(surf.Vertices, cnormal, cx, cy, cz);
  v1 := surf.Vertices[edge];
  v2 := surf.Vertices[surf.NextVX(edge)];
  if (v1.Mark = 0) or (v1.Mark <> -v2.Mark) then
    exit;

  SaveSecUndo(surf.sector, ch_changed, sc_both);

  D := cnormal.dx * cx + cnormal.dy * cy + cnormal.dz * cz;

  With nvx do
    PlaneLineXn(cnormal, D, v1.x, v1.y, v1.z, v2.x, v2.y, v2.z, ax, ay, az);

  SaveSecUndo(surf.sector, ch_changed, sc_geo);

  nvx := surf.sector.AddVertex(ax, ay, az);

  surf.InsertVertex(surf.NextVX(edge), nvx);

  For i := 0 to surf.sector.surfaces.count - 1 do
    With surf.sector.surfaces[i] do
    begin
      for j := 0 to Vertices.count - 1 do
      begin
        nv1 := Vertices[j];
        nv2 := Vertices[NextVX(j)];
        if ((v1 = nv1) and (v2 = nv2)) or ((v1 = nv2) and (v2 = nv1)) then
          InsertVertex(NextVX(j), nvx);
      end;
    end;

  surf.sector.Renumber;

  For i := 0 to surf.sector.surfaces.count - 1 do
    surf.sector.surfaces[i].RecalcAll;

  JedMain.SectorChanged(surf.sector);
end;

Procedure RelayerSCThings(sc: TJKSector);
var
  i, n: integer;
  th: TJKThing;
  lev: TJKLevel;
begin
  lev := sc.level;
  for i := 0 to lev.things.Count - 1 do
  begin
    th := lev.things[i];
    if th.sec <> sc then
      continue;

    n := FindSectorForThing(th);
    if n = -1 then
      th.sec := nil
    else
      th.sec := lev.sectors[n];
  end;
end;

Procedure RemoveSecRefs(lev: TJKLevel; sec: TJKSector; opt: byte);
var
  p, i, j: integer;
begin
  if opt and rs_surfs <> 0 then
    for i := 0 to lev.sectors.Count - 1 do
      with lev.sectors[i] do
        for j := 0 to surfaces.Count - 1 do
          With surfaces[j] do
          begin
            if adjoin = nil then
              continue;
            if adjoin.sector = sec then
            begin
              SaveSecUndo(lev.sectors[i], ch_changed, sc_val);
              adjoin := nil;
              ClearAdjoinParams(surfaces[j]);
              JedMain.SectorChanged(lev.sectors[i]);
            end;
          end;

  for i := 0 to lev.cogs.Count - 1 do
    with lev.cogs[i] do
      for j := 0 to Vals.Count - 1 do
        with Vals[j] do
        begin
          if (Cog_Type = ct_sec) and (obj = sec) then
            obj := nil;
          if opt and rs_surfs <> 0 then
            if (Cog_Type = ct_srf) and (obj <> nil) and
              (TJKSurface(obj).sector = sec) then
              obj := nil;
        end;

  for i := 0 to lev.things.Count - 1 do
    if lev.Things[i].sec = sec then
    begin
      lev.Things[i].sec := nil;
    end;
end;

Procedure RemoveSurfRefs(lev: TJKLevel; surf: TJKSurface);
var
  p, i, j: integer;
begin
  for i := 0 to lev.sectors.Count - 1 do
    with lev.sectors[i] do
      for j := 0 to surfaces.Count - 1 do
        with surfaces[j] do
          begin
            if adjoin = nil then
              continue;
            if adjoin = surf then
              begin
                //SaveSecUndo(adjoin.sector, ch_changed, sc_val, True);
                SaveSecUndo(lev.sectors[i], ch_changed, sc_val); // new change
                adjoin := nil;
                ClearAdjoinParams(surfaces[j]);
                JedMain.SectorChanged(lev.sectors[i]);
              end;
          end;

  for i := 0 to lev.cogs.Count - 1 do
    with lev.cogs[i] do
      for j := 0 to Vals.Count - 1 do
        with Vals[j] do
          begin
            if (Cog_Type = ct_srf) and (obj = surf) then
              obj := nil;
          end;
end;

Function CleaveSector(sec: TJKSector; const cnormal: TVector;
  cx, cy, cz: double): boolean;
var
  i, sf, sfside: integer;
  nsf, vxs: TJKVertices;
  v1, v2: TJKVertex;
  nv: integer;
  sec1: TJKSector;
  asurf, surf, surf1: TJKSurface;
  lev: TJKLevel;

  Procedure AddVXs(vxs: TJKVertices; v1, v2: TJKVertex);
  var
    i: integer;
    cv1, cv2: TJKVertex;
  begin
    for i := 0 to vxs.Count div 2 - 1 do
    begin
      cv1 := vxs[i * 2];
      cv2 := vxs[i * 2 + 1];
      if ((cv1 = v1) and (cv2 = v2)) or ((cv1 = v2) and (cv2 = v1)) then
        exit;
    end;
    vxs.Add(v1);
    vxs.Add(v2);
  end;

var
  nleft, nright: integer;
begin
  Result := False;
  lev := sec.level;
  SplitVertices(sec.vertices, cnormal, cx, cy, cz);

  nleft  := 0;
  nright := 0;
  for i := 0 to sec.vertices.Count - 1 do
    with sec.vertices[i] do
    begin
      if Mark = -1 then
        Inc(nleft);
      if Mark = 1 then
        Inc(nright);
    end;

  if (nleft = 0) or (nright = 0) then
    exit;

  SaveSecUndo(sec, ch_changed, sc_both);

  for sf := 0 to sec.surfaces.Count - 1 do
  begin
    CleaveAdjoin(sec.surfaces[sf], cnormal, cx, cy, cz,
      SysCleaveSurface(sec.surfaces[sf], cnormal, cx, cy, cz));
  end;

  vxs := TJKVertices.Create;
  try
    for sf := 0 to sec.surfaces.Count - 1 do
      with sec.surfaces[sf] do
      begin
        nleft := 0;
        for i := 0 to vertices.Count - 1 do
          if vertices[i].Mark <> 0 then
          begin
            nleft := 1;
            break;
          end;

        { Don't take vertices from surfaces fully lying on the cleaving plane }
        if nleft = 0 then
          continue;

        for i := 0 to vertices.Count - 1 do
        begin
          v1 := vertices[i];
          v2 := vertices[NextIdx(i, Vertices.Count)];
          if (v1.Mark = 0) and (v2.Mark = 0) then
            AddVXs(vxs, v1, v2);
        end;
      end;

    if vxs.Count = 0 then
    begin
      vxs.Free;
      exit;
    end;

    nsf := TJKVertices.Create;
    nsf.Add(vxs[0]);
    nsf.Add(vxs[1]);

    v1 := vxs[1];
    vxs.Delete(0);
    vxs.Delete(0);

    repeat
      nv := vxs.count;
      for sf := vxs.Count div 2 - 1 downto 0 do
      begin
        if vxs[sf * 2] = v1 then
        begin
          v1 := vxs[sf * 2 + 1];
          nsf.Add(v1);
          vxs.Delete(sf * 2);
          vxs.Delete(sf * 2);
          break;
        end;

        if vxs[sf * 2 + 1] = v1 then
        begin
          v1 := vxs[sf * 2];
          nsf.Add(v1);
          vxs.Delete(sf * 2);
          vxs.Delete(sf * 2);
          break;
        end;
      end;
    until nv = vxs.count;

    vxs.Clear;

    if nsf[nsf.Count - 1] = nsf[0] then
      nsf.Delete(nsf.Count - 1);

    { Create new sector }

    sec1 := lev.NewSector;
    sec1.Num := lev.sectors.Count;
    sec1.Assign(sec);
    lev.sectors.Add(sec1);

    SaveSecUndo(sec1, ch_added, sc_both);

    { Create a copy of these vertices - for another sector }
    for i := 0 to nsf.Count - 1 do
    begin
      v1 := sec1.NewVertex;
      v1.Assign(nsf[i]);
      vxs.Add(v1);
    end;

    { Split surfaces between sectors }

    for sf := sec.surfaces.Count - 1 downto 0 do
    begin
      sfside := 0;
      surf := sec.surfaces[sf];
      for i := 0 to surf.vertices.Count - 1 do
      begin
        sfside := surf.vertices[i].Mark;
        if sfside <> 0 then
          break;
      end;

      Case sfside of
        - 1:
          begin
            surf.sector := sec1;
            sec1.surfaces.Add(surf);
            sec.surfaces.Delete(sf);
          end;
        0:
          With surf do
            if cnormal.Dot(normal) < 0 then
            //if SMult(cnormal.dx, cnormal.dy, cnormal.dz, normal.dx, normal.dy, normal.dz) < 0 then
            begin
              surf.sector := sec1;
              sec1.surfaces.Add(surf);
              sec.surfaces.Delete(sf);
            end;
      end;
    end;

    { Split vertices between two sectors }
    for sf := sec.vertices.Count - 1 downto 0 do
    begin
      v1 := sec.vertices[sf];
      case v1.Mark of
        - 1:
          begin
            v1.sector := sec1;
            sec1.vertices.Add(v1);
            sec.vertices.Delete(sf);
          end;
      end;
    end;

    { Now build the adjoining surfaces }

    surf := sec.NewSurface;
    for i := 0 to nsf.Count - 1 do
      surf.AddVertex(nsf[i]);

    surf1 := sec.NewSurface;
    for i := nsf.Count - 1 downto 0 do
      surf1.AddVertex(nsf[i]);

    if sec.surfaces.Count > 0 then
    begin
      surf.Assign(sec.surfaces[0]);
      surf1.Assign(sec.surfaces[0]);
    end;

    SysAdjoinSurfaces(surf, surf1);

    surf.NewRecalcAll;
    surf1.NewRecalcAll;

    with surf do
      if cnormal.Dot(normal) > 0 then
      //if SMult(cnormal.dx, cnormal.dy, cnormal.dz, normal.dx, normal.dy, normal.dz) > 0 then
      begin
        sec.surfaces.Add(surf);
        surf1.sector := sec1;
        sec1.surfaces.Add(surf1);
      end
      else
      begin
        sec.surfaces.Add(surf1);
        surf.sector := sec1;
        sec1.surfaces.Add(surf);
      end;

    Result := True;
  finally
    { Make copies of vertices that may belong to another sector }
    for sf := 0 to sec1.surfaces.Count - 1 do
      with sec1.surfaces[sf] do
        for i := 0 to vertices.Count - 1 do
        begin
          v1 := vertices[i];
          if v1.Mark <> 0 then
            continue;
          vertices[i] := sec1.AddVertex(v1.x, v1.y, v1.z);
          { nv:=nsf.IndexOf(v1);
            if nv<>-1 then Vertices[i]:=vxs[nv]; }
        end;

    vxs.Free;
    nsf.Free;

    sec.Renumber;
    JedMain.SectorChanged(sec);

    sec1.Renumber;
    JedMain.SectorAdded(sec1);

    RelayerSCThings(sec);
  end;
end;

Function ConnectSurfaces(surf1, surf2: TJKSurface): boolean;
var
  surf, nsurf1, nsurf2: TJKSurface;
  cx, cy, cz: double;
  D: double;
  i: integer;
  v1, v2: TJKVertex;

  Procedure CleaveSurfBySurf(surf, bysurf: TJKSurface);
  var
    i: integer;
    vec: TVector;
    b: boolean;
  begin

    for i := 0 to bysurf.vertices.Count - 1 do
    begin
      v1 := bysurf.vertices[i];
      v2 := bysurf.vertices[bysurf.NextVX(i)];
      vec.SetCoords(v2.x - v1.x, v2.y - v1.y, v2.z - v1.z);
      //if not Normalize(vec) then
      if vec.Normalize = 0 then      
        continue;

      vec := vec.Cross(bysurf.normal);
//      VMult(vec.dx, vec.dy, vec.dz, bysurf.normal.dx, bysurf.normal.dy,
//        bysurf.normal.dz, vec.dx, vec.dy, vec.dz);
      b := CleaveSurface(surf, vec, v1.x, v1.y, v1.z);
      if b then;
    end;
  end;

begin
  result := false;
  if (surf1.adjoin <> nil) or (surf2.adjoin <> nil) then
  begin
    PanMessage(mt_warning, 'One (or both) surfaces are adjoined!');
    exit;
  end;
  if (surf1.sector = surf2.sector) then
  begin
    PanMessage(mt_warning, 'Surfaces belong to the same sector!');
    exit;
  end;

  { Check that surfaces are back to back }

  v1 := surf1.vertices[0];
  v2 := surf2.vertices[0];

   D := surf1.normal.Dot(surf2.normal);
  //D := SMult(surf1.normal.dx, surf1.normal.dy, surf1.normal.dz, surf2.normal.dx, surf2.normal.dy, surf2.normal.dz);

  if D > -0.9999 then
  begin
    PanMessage(mt_warning, 'The surfaces aren''t coplanar!');
    exit;
  end;

  if Abs(surf1.normal.Dot(v2.x - v1.x, v2.y - v1.y, v2.z - v1.z)) > 0.0001 then //SMult,    VectorDot3(v2.x - v1.x, v2.y - v1.y, v2.z - v1.z, surf1.normal.dx, surf1.normal.dy, surf1.normal.dz)
  begin
    PanMessage(mt_warning, 'The surfaces are not placed back to back');
    exit;
  end;

  CleaveSurfBySurf(surf2, surf1);
  CleaveSurfBySurf(surf1, surf2);

  if DO_Surf_Overlap(surf1, surf2) then
  begin
    SysAdjoinSurfaces(surf1, surf2);
    result := true;
  end
  else
    PanMessage(mt_warning, 'The resulting surfaces don''t overlap!');
end;

Function ConnectSectors(sec1, sec2: TJKSector): boolean;

  Procedure CleaveSecBySec(sec, bysec: TJKSector);
  var
    i: integer;
    vec: TVector;
    b: boolean;
    surf: TJKSurface;
    v: TJKVertex;
  begin
    for i := 0 to bysec.surfaces.Count - 1 do
    begin
      surf := bysec.surfaces[i];
      if surf.normal.Length = 0 then  //Vlen
        continue;
      v := surf.vertices[0];
      b := CleaveSector(sec, surf.normal, v.x, v.y, v.z);
      if b then;
    end;
  end;

begin { ConnectSectors }
  Result := False;
  if not DoSectorsOverlap(sec1, sec2) then
  begin
    PanMessage(mt_warning, 'Sectors can''t be joined because they don''t overlap!');
    exit;
  end;

  var lecNum := sec1.level.sectors.Count;
  CleaveSecBySec(sec2, sec1);
  CleaveSecBySec(sec1, sec2);

  for var i := 0 to sec1.surfaces.Count - 1 do
    MakeAdjoinSCUP(sec1.surfaces[i], lecNum);

  DeleteSector(sec2.level, sec2.num);
  Result := true;
end;

//Function VLen(const v: TVector): double;
//begin
//  result := sqrt(sqr(v.dx) + sqr(v.dy) + sqr(v.dz));
//end;

//Function Normalize(var normal: TVector): boolean;
//var
//  dist: double;
//begin
//  result := false;
//  dist := sqrt(sqr(normal.dx) + sqr(normal.dy) + sqr(normal.dz));
//  if dist = 0 then
//    exit;
//  normal.dx := normal.dx / dist;
//  normal.dy := normal.dy / dist;
//  normal.dz := normal.dz / dist;
//  result := true;
//end;

Procedure CalcDefaultUVNormals(const surf: TJKSurface; orgvx: integer; var un, vn: TVector);
var
  v1, v2: TJKVertex;
  i: integer;
begin
  v1 := surf.vertices[orgvx];
  v2 := surf.vertices[NextIdx(orgvx, surf.vertices.Count)];
  with un do
  begin
    dx := v2.x - v1.x;
    dy := v2.y - v1.y;
    dz := v2.z - v1.z;
  end;

  un.Normalize;
  vn := un.Cross(surf.normal);

  // Normalize(un);
// With surf do
//    VMult(un.dx, un.dy, un.dz, normal.dx, normal.dy, normal.dz, vn.dx,
//      vn.dy, vn.dz);
end;

Function IsVFlipped(surf: TJKSurface; nv1, nv2: integer; const u, v: TVector): boolean;
var
  i: integer;
  vx, v1, v2: TJKVertex;
  tv, tv1, tv2: TTXVertex;
  dv: double;
begin
  result := false;
  v1 := surf.Vertices[nv1];
  tv1 := surf.txvertices[nv1];
  v2 := surf.Vertices[nv2];
  tv2 := surf.txvertices[nv2];
  { Check if the direction of V normal is correct }
  for i := 0 to surf.Vertices.count - 1 do
  begin
    vx := surf.Vertices[i];
    if VXEq(vx, v1) or VXEq(vx, v2) then
      continue;
    tv := surf.txvertices[i];
    dv := UVRound(v.Dot(vx.x - v1.x, vx.y - v1.y, vx.z - v1.z) * PixelPerUnit);
//    dv := UVRound(SMult(v.dx, v.dy, v.dz, vx.x - v1.x, vx.y - v1.y, vx.z - v1.z)
//      * PixelPerUnit);
    result := (dv > 0) <> ((tv.v - tv1.v) > 0);
    exit;
  end;
end;

(* Procedure SysCalcUVNormalsFrom(surf:TJKSurface; nv1,nv2:integer; var un,vn:Tvector;flip:boolean);
  var vx,v1,v2:TJKVertex;
  tv,tv1,tv2:TTXVertex;
  u,v:tvector;
  du,dv,vl:double;
  i:integer;
  begin
  v1:=surf.vertices[nv1]; tv1:=surf.txvertices[nv1];
  v2:=surf.vertices[nv2]; tv2:=surf.txvertices[nv2];
  u.dx:=v2.x-v1.x; u.dy:=v2.y-v1.y; u.dz:=v2.z-v1.z;

  With Surf do VMult(u.dx,u.dy,u.dz,
  normal.dx,normal.dy,normal.dz,
  v.dx,v.dy,v.dz);

  If flip then SetVec(v,-v.dx,-v.dy,-v.dz);

  Normalize(u); Normalize(v);

  du:=tv2.u-tv1.u;
  dv:=tv2.v-tv1.v; vl:=sqr(du)+sqr(dv);
  if isClose(vl,0) then begin CalcDefaultUVNormals(surf,0,un,vn); exit; end;
  du:=du/vl;
  dv:=dv/vl;

  un.dx:=du*u.dx-dv*v.dx;
  un.dy:=du*u.dy-dv*v.dy;
  un.dz:=du*u.dz-dv*v.dz;

  vn.dx:=dv*u.dx+du*v.dx;
  vn.dy:=dv*u.dy+du*v.dy;
  vn.dz:=dv*u.dz+du*v.dz;

  {Check normals}
  Normalize(un);
  Normalize(vn);
  {  i:=surf.NextVX(nv1);
  if i=nv2 then i:=surf.NextVX(nv2);
  v2:=surf.Vertices[i];
  du:=UVRound(tv1.u+SMult(un.dx,un.dy,un.dz,v2.x-v1.x,v2.y-v1.y,v2.z-v1.z)*PixelPerUnit);
  dv:=UVRound(tv1.v+SMult(vn.dx,vn.dy,vn.dz,v2.x-v1.x,v2.y-v1.y,v2.z-v1.z)*PixelPerUnit);
  With surf.TxVertices[i] do
  if (Abs(u-du)<1) and (Abs(v+dv)<1) then
  begin
  vn.dx:=-vn.dx;
  vn.dy:=-vn.dy;
  vn.dz:=-vn.dz;
  end;}

  end; *)

Procedure UpdateSurfUVData(const surf: TJKSurface; var un, vn: TVector);
var
  cosa, sina: double;
  a, b: TVector;
begin
  { if not surf.GetRefVector(a) then exit;
    cosa:=SMult(a.dx,a.dy,a.dz,un.dx,un.dy,un.dz);
    sina:=sqrt(1-sqr(cosa));
    VMult(a.dx,a.dy,a.dz,un.dx,un.dy,un.dz,b.dx,b.dy,b.dz);
    With Surf.Normal do
    if SMult(dx,dy,dz,b.dx,b.dy,b.dz)<0 then sina:=-sina;
    surf.ucosa:=cosa+16;
    surf.usina:=sina; }
end;

Procedure FindUVScales(surf: TJKSurface);
var
  un, vn: TVector;
  v1, v2: TJKVertex;
  tv1, tv2: TTXVertex;
  d1, d2: double;
  nv2, nv3: integer;
begin
  CalcUVNormals(surf, un, vn);

  if not FindTri(surf, 0, nv2, nv3) then
    exit;

  v1 := surf.Vertices[0];
  tv1 := surf.txvertices[0];
  v2 := surf.Vertices[nv2];
  tv2 := surf.txvertices[nv2];

  d1 := tv2.u - tv1.u;
  if IsZero(d1) or (CompareValue(d1,0) = LessThanValue) then
  begin
    v2 := surf.Vertices[nv3];
    tv2 := surf.txvertices[nv3];
    d1 := tv2.u - tv1.u;
  end;

  d2 := un.Dot(v2.x - v1.x, v2.y - v1.y, v2.z - v1.z) * PixelPerUnit;
  //d2 := SMult(un.dx, un.dy, un.dz, v2.x - v1.x, v2.y - v1.y, v2.z - v1.z) * PixelPerUnit;
  surf.uscale := Abs(d1 / d2);
  surf.uscale := IfThen(IsNan(surf.uscale) or IsInfinite(surf.uscale), 1, surf.uscale);

  v2 := surf.Vertices[nv2];
  tv2 := surf.txvertices[nv2];

  d1 := tv2.v - tv1.v;
  if IsZero(d1) or (CompareValue(d1,0) = LessThanValue) then
  begin
    v2 := surf.Vertices[nv3];
    tv2 := surf.txvertices[nv3];
    d1 := tv2.v - tv1.v;
  end;

  d2 := vn.Dot(v2.x - v1.x, v2.y - v1.y, v2.z - v1.z) * PixelPerUnit;
  // d2 := SMult(vn.dx, vn.dy, vn.dz, v2.x - v1.x, v2.y - v1.y, v2.z - v1.z) * PixelPerUnit;
  surf.vscale := Abs(d1 / d2);
  surf.vscale := IfThen(IsNan(surf.vscale) or IsInfinite(surf.vscale), 1, surf.vscale);
end;

Procedure SysCalcUVNormalsFrom(const surf: TJKSurface; nv1: integer; var un, vn: TVector);
var
  nv2, nv3: integer;
  v1, v2, v3: TJKVertex;
  tv1, tv2, tv3: TTXVertex;
  a, b: TVector;
  du1, dv1, du2, dv2: double;
  D: double;
begin
  { if surf.ucosa>0 then
    begin
    if not surf.GetRefVector(a) then
    begin
    PanMessageFmt(mt_warning,'Zero length of first edge in sector %d surface %d',[surf.sector.num,surf.num]);
    SetVec(un,0,0,0);
    SetVec(vn,0,0,0);
    exit;
    end;

    With Surf.Normal do
    VMult(dx,dy,dz,a.dx,a.dy,a.dz,b.dx,b.dy,b.dz);

    du1:=surf.ucosa-16;
    dv1:=surf.usina;

    un.dx:=du1*a.dx+dv1*b.dx;
    un.dy:=du1*a.dy+dv1*b.dy;
    un.dz:=du1*a.dz+dv1*b.dz;

    un.dx:=-dv1*a.dx+du1*b.dx;
    un.dy:=-dv1*a.dy+du1*b.dy;
    un.dz:=-dv1*a.dz+du1*b.dz;
    exit;
    end; }

  if not FindTri(surf, nv1, nv2, nv3) then
  begin
    un.SetCoords(0, 0, 0);
    vn.SetCoords(0, 0, 0);
    exit;
  end;

  v1  := surf.vertices[nv1];
  tv1 := surf.txvertices[nv1];

  v2  := surf.vertices[nv2];
  tv2 := surf.txvertices[nv2];

  v3  := surf.vertices[nv3];
  tv3 := surf.txvertices[nv3];

  a.SetCoords(v1.x - v2.x, v1.y - v2.y, v1.z - v2.z);
  b.SetCoords(v3.x - v2.x, v3.y - v2.y, v3.z - v2.z);

  du1 := tv1.u - tv2.u;
  dv1 := tv1.v - tv2.v;

  du2 := tv3.u - tv2.u;
  dv2 := tv3.v - tv2.v;

  un := dv2 * a - dv1 * b;
//  un.dx := dv2 * a.dx - dv1 * b.dx;
//  un.dy := dv2 * a.dy - dv1 * b.dy;
//  un.dz := dv2 * a.dz - dv1 * b.dz;

  vn := du2 * a - du1 * b;
//  vn.dx := du2 * a.dx - du1 * b.dx;
//  vn.dy := du2 * a.dy - du1 * b.dy;
//  vn.dz := du2 * a.dz - du1 * b.dz;

  if dv2 * du1 < dv1 * du2 then
    un := -un;
    //un.SetCoords(-un.dx, -un.dy, -un.dz);
  if du2 * dv1 < du1 * dv2 then
    vn := -vn;
    //vn.SetCoords(-vn.dx, -vn.dy, -vn.dz);

  //if not Normalize(un) then
  if un.Normalize = 0 then   
  begin
    un := a;
    un.Normalize; //Normalize(un);
  end;

  var av := surf.normal.Cross(un);
//  with surf.normal do
//    VMult(dx, dy, dz, un.dx, un.dy, un.dz, av.dx, av.dy, av.dz);

  { if Smult(vn.dx,vn.dy,vn.dz,av.dx,av.dy,av.dz)>=0 then vn:=av
    else SetVec(vn,-av.dx,-av.dy,-av.dz); }

  vn := av;
  UpdateSurfUVData(surf, un, vn);
end;

Procedure CalcUVNormalsFrom(const surf: TJKSurface; nv1, nv2: integer; var un, vn: TVector);
begin
  SysCalcUVNormalsFrom(surf, nv1, un, vn);
end;

Procedure CalcUVNormalsFromVX(const surf: TJKSurface; nvx: integer; var un, vn: TVector);
begin
  SysCalcUVNormalsFrom(surf, nvx, un, vn);
end;

Procedure CalcUVNormals(const surf: TJKSurface; var un, vn: TVector);
begin
  CalcUVNormalsFromVX(surf, 0, un, vn);
end;


Function GetSurfResScale(surfflags: integer): double;
begin
  Result := 1;
  if BitMaskTest(surfflags, SF_EIGHTHRES) then
    Result := 0.125
  else if BitMaskTest(surfflags, SF_QUATERRES) then
    Result := 0.25
  else if BitMaskTest(surfflags, SF_HALFRES) then
    Result := 0.5
  else if BitMaskTest(surfflags, SF_DOUBLERES) then
    Result := 2.0
  else if BitMaskTest(surfflags, SF_QUADRUPLERES) then
    Result := 4.0;
end;


//Function PixlePerRef(surfflags: integer): double;
//begin
//  Result := 320.0;
//  if ( (surfflags and $4000000) <>0 ) then
//    Result := 80.0
//  else if ((surfflags and $20) <>0  )  then
//    Result := 160.0
//  else if ((surfflags and $10) <>0  ) then
//    Result := 640.0
//  else if ((surfflags and $8000000) <>0  )then
//    Result :=1280.0
//  else if ((surfflags and $40) <>0  ) then
//    Result := 40.0;
//end;

Procedure ArrangeTextureBy(const surf: TJKSurface; const un, vn: TVector;
  refx, refy, refz, refu, refv: double);
begin
  for var i := 0 to surf.Vertices.Count - 1 do
    with surf.txvertices[i], surf.vertices[i] do
    begin
        u := UVRound(refu + un.Dot(x - refx, y - refy, z - refz) * PixelPerUnit * surf.uscale);
//      u := UVRound(refu + SMult(un.dx, un.dy, un.dz, x - refx, y - refy,
//        z - refz) * PixelPerUnit * surf.uscale);

      //surf.FaceFlags and FF_SF_FLIP <> 0
      if BitTest(surf.FaceFlags, FF_SF_FLIP) <> 0 then
        v := UVRound(refv + (-vn).Dot(x - refx, y - refy, z - refz) * PixelPerUnit * surf.vscale)
//        v := UVRound(refv + SMult(-vn.dx, -vn.dy, -vn.dz, x - refx, y - refy,
//          z - refz) * PixelPerUnit * surf.vscale)
      else
        v := UVRound(refv + vn.Dot(x - refx, y - refy, z - refz) * PixelPerUnit * surf.vscale)
//        v := UVRound(refv + SMult(vn.dx, vn.dy, vn.dz, x - refx, y - refy,
//          z - refz) * PixelPerUnit * surf.vscale);
    end;
end;

Procedure ApplyTXDataToVX(const surf: TJKSurface; vx: integer; const txd: TXData);
begin
  with surf.txvertices[vx], surf.vertices[vx] do
  begin
    u := UVRound(txd.u + txd.un.Dot(x - txd.x, y - txd.y, z - txd.z) * PixelPerUnit * surf.uscale);
    v := UVRound(txd.v + txd.vn.Dot(x - txd.x, y - txd.y, z - txd.z) * PixelPerUnit * surf.vscale);
//    u := UVRound(txd.u + SMult(txd.un.dx, txd.un.dy, txd.un.dz, x - txd.x,
//      y - txd.y, z - txd.z) * PixelPerUnit * surf.uscale);
//    v := UVRound(txd.v + SMult(txd.vn.dx, txd.vn.dy, txd.vn.dz, x - txd.x,
//      y - txd.y, z - txd.z) * PixelPerUnit * surf.vscale);
  end;
end;

Procedure ArrangeTexture(const surf: TJKSurface; orgvx: integer; const un, vn: TVector);
begin
  with surf.vertices[orgvx], surf.txVertices[orgvx] do
    ArrangeTextureBy(surf, un, vn, x, y, z, u, v);
end;

Procedure SaveTXData(const surf: TJKSurface; vx: integer; var txd: TXData);
begin
  try
    with surf.vertices[vx], surf.txvertices[vx] do
    begin
      txd.x := x;
      txd.y := y;
      txd.z := z;
      txd.u := u;
      txd.v := v;
    end;
    CalcUVNormals(surf, txd.un, txd.vn);
  except
    On Exception do
      FillChar(txd, sizeof(txd), 0);
  end;
end;

Procedure ApplyTXData(const surf: TJKSurface; const txd: TXData);
begin
  with txd do
    ArrangeTextureBy(surf, un, vn, x, y, z, u, v);
end;

Procedure CalcUV(const surf: TJKSurface; orgvx: integer);
var
  un, vn: TVector;
begin
  CalcDefaultUVNormals(surf, orgvx, un, vn);
  ArrangeTexture(surf, orgvx, un, vn);
end;

Function IsPointWithInLine(x, y, z, x1, y1, z1, x2, y2, z2: double): boolean;
begin
  result :=
    (x >= real_min(x1, x2)) and (x <= real_max(x1, x2)) and
    (y >= real_min(y1, y2)) and (y <= real_max(y1, y2)) and
    (z >= real_min(z1, z2)) and (z <= real_max(z1, z2));
end;

Function IsPointOnSurface(const surf: TJKSurface; x, y, z: double): boolean;
var
  vc, i: integer;
  v1, v2: TJKVertex;
  dist: double;
  vct: TVector;
  x1, x2, y1, y2, z1, z2: double;
begin
  result := false;

  vc := surf.vertices.count;

  { x1:=999999999;x2:=-999999999;
    y1:=999999999;y2:=-999999999;
    z1:=999999999;z2:=-999999999;

    For i:=0 to vc-1 do
    With Surf.Vertices[i] do
    begin
    if x<x1 then x1:=x; if x>x2 then x2:=x;
    if y<y1 then y1:=y; if y>y2 then y2:=y;
    if z<z1 then z1:=z; if z>x2 then z2:=z;
    end;

    if (x<x1) or (x>x2) or (y<y1) or (y>y2) or (z<z1) or (z>z2) then exit; }

  for i := 0 to vc - 1 do
    begin
      v1 := surf.vertices[i];
      v2 := surf.vertices[NextIdx(i, vc)];

      vct := surf.normal.Cross(v2.x - v1.x, v2.y - v1.y, v2.z - v1.z);
      dist := vct.Dot(x - v1.x, y - v1.y, z - v1.z);
  //    With surf do
  //      VMult(normal.dx, normal.dy, normal.dz, v2.x - v1.x, v2.y - v1.y,
  //        v2.z - v1.z, vct.dx, vct.dy, vct.dz);
      //dist := SMult(vct.dx, vct.dy, vct.dz, x - v1.x, y - v1.y, z - v1.z);
      if (dist < -CloseEnough) then
        exit;
      { if dist<=0 then exit; }
    end;
  result := true;
end;

function FindSectorForXYZ(const lev: TJKLevel; x, y, z: double; visibleOnly: Boolean): Integer;
begin
  result := -1;
  for var i := 0 to lev.sectors.Count - 1 do
  begin
    var sec := lev.sectors[i];
    if visibleOnly and not ToolBar.IsLayerVisible(sec.layer) then
      continue;

    if IsInSector(lev.sectors[i], x, y, z) then
    begin
      result := i;
      exit;
    end;
  end;
end;

Function FindSectorForXYZ(const lev: TJKLevel; x, y, z: double): integer;
begin
  Result := FindSectorForXYZ(lev, x, y, z, False);
end;

Function FindSectorForThing(const thing: TJKThing): integer;
var
  i: integer;
begin
  result := FindSectorForXYZ(thing.level, thing.x, thing.y, thing.z);
end;

Function GetPreviousSector(const lev: TJKLevel; n: integer): integer;
begin
  Result := -1;
  if level.sectors.Count = 0 then
    exit;

  n := Clampi(n, 0, level.sectors.Count - 1);
  var cur := n;
  repeat
      if (cur < 1) then
          cur := level.sectors.Count - 1
      else
          dec(cur);
      if cur = n then
          exit;
  until ToolBar.IsLayerVisible(level.sectors[cur].Layer);
  Result := cur;
end;

Function GetPreviousThing(const lev: TJKLevel; n: integer): integer;
begin
  Result := -1;
  if level.things.Count = 0 then
    exit;

  n := Clampi(n, 0, level.things.Count - 1);
  var cur := n;
  repeat
      if (cur < 1) then
          cur := level.things.Count - 1
      else
          dec(cur);
      if cur = n then
          exit;
  until ToolBar.IsLayerVisible(level.things[cur].Layer);
  Result := cur;
end;

Function GetPreviousLight(const lev: TJKLevel; n: integer): integer;
begin
  Result := -1;
  if Level.Lights.Count = 0 then
    exit;

  n := Clampi(n, 0,  Level.Lights.Count - 1);
  var cur := n;
  repeat
      if (cur < 1) then
          cur := Level.Lights.Count - 1
      else
          dec(cur);
      if cur = n then
          exit;
  until ToolBar.IsLayerVisible(Level.Lights[cur].Layer);
  Result := cur;
end;

Const
  MaxLight = 10;

Function DoBoxesIntersect(const box1, box2: TBox): boolean;
begin
  result := false;
  if (box1.x2 < box2.x1) then
    exit;
  if (box1.x1 > box2.x2) then
    exit;
  if (box1.y2 < box2.y1) then
    exit;
  if (box1.y1 > box2.y2) then
    exit;
  if (box1.z2 < box2.z1) then
    exit;
  if (box1.z1 > box2.z2) then
    exit;
  result := true;
end;

Function IsPointInBox(const box: TBox; x, y, z: double): boolean;
begin
  result := (x - box.x1 >= -CloseEnough) and (x - box.x2 <= CloseEnough) and
    (y - box.y1 >= -CloseEnough) and (y - box.y2 <= CloseEnough) and
    (z - box.z1 >= -CloseEnough) and (z - box.z2 <= CloseEnough);
end;

Procedure SetBox(var box: TBox; x1, x2, y1, y2, z1, z2: double);
begin
  box.x1 := real_min(x1, x2);
  box.x2 := real_max(x1, x2);
  box.y1 := real_min(y1, y2);
  box.y2 := real_max(y1, y2);
  box.z1 := real_min(z1, z2);
  box.z2 := real_max(z1, z2);
end;

Function Do2DBoxesIntersect(const box1, box2: TBox2): boolean;
begin
  result := false;
  if (box1.x2 < box2.x1) then
    exit;
  if (box1.x1 > box2.x2) then
    exit;
  if (box1.y2 < box2.y1) then
    exit;
  if (box1.y1 > box2.y2) then
    exit;
  result := true;
end;

Function IsPointIn2DBox(const box: TBox2; x, y: double): boolean;
begin
  result := (x - box.x1 >= -CloseEnough) and (x - box.x2 <= CloseEnough) and
    (y - box.y1 >= -CloseEnough) and (y - box.y2 <= CloseEnough);
end;

Procedure Set2DBox(var box: TBox2; x1, x2, y1, y2: double);
begin
  box.x1 := real_min(x1, x2);
  box.x2 := real_max(x1, x2);
  box.y1 := real_min(y1, y2);
  box.y2 := real_max(y1, y2);
end;

Type
  TLCSurfData = record
    bbox: TBox;
    D: double;
  end;

  PLCSurfData = ^TLCSurfData;

Procedure CalcSurfBBox(surf: TJKSurface; var box: TBox);
var
  i: integer;
begin
  box.x1 := 999999999;
  box.x2 := -999999999;
  box.y1 := 999999999;
  box.y2 := -999999999;
  box.z1 := 999999999;
  box.z2 := -999999999;
  for i := 0 to surf.Vertices.count - 1 do
    With surf.Vertices[i] do
    begin
      if x < box.x1 then
        box.x1 := x;
      if x > box.x2 then
        box.x2 := x;
      if y < box.y1 then
        box.y1 := y;
      if y > box.y2 then
        box.y2 := y;
      if z < box.z1 then
        box.z1 := z;
      if z > box.z2 then
        box.z2 := z;
    end;
end;

Function BuildSector(lev: TJKLevel; sfsel: TSFMultiSel): TJKSector;
var
  sec, nsec: TJKSector;
  surf: TJKSurface;
  i, j: integer;
  sc, sf, asc, asf: integer;
  tv: TTXVertex;
begin
  result := nil;
  if sfsel.Count < 4 then
  begin
    PanMessage(mt_info, 'Cannot create sector: Less than 4 surfaces selected!');
    exit;
  end;

  sfsel.GetSCSF(0, sc, sf);
  for i := 1 to sfsel.Count - 1 do
  begin
    sfsel.GetSCSF(i, asc, asf);
    if asc <> sc then
    begin
      PanMessageFmt(mt_warning, 'Cannot create sector %d: The selected surface %d is part of sector %d!', [sc, asf, asc]);
      exit;
    end;
  end;

  sec := Level.sectors[sc];
  nsec := Level.NewSector;

  { Clear surface and vertex marks marks }
  for i := 0 to sec.surfaces.Count - 1 do
    sec.surfaces[i].mark := 0;
  for i := 0 to sec.vertices.Count - 1 do
    sec.vertices[i].mark := 0;

  { Copy surfaces and vertices to new sector }
  for i := 0 to sfsel.Count - 1 do
  begin
    sfsel.GetSCSF(i, sc, sf);
    surf := sec.surfaces[sf];
    surf.mark   := 1;
    surf.sector := nsec;
    for j := 0 to surf.vertices.Count - 1 do
      with surf.vertices[j] do
        surf.vertices[j] := nsec.AddVertex(x, y, z);
    nsec.surfaces.Add(surf);
  end;

  { Mark used vertices and remove surfaces }
  for i := sec.surfaces.Count - 1 downto 0 do
  begin
    surf := sec.surfaces[i];
    for j := 0 to surf.Vertices.Count - 1 do
      inc(surf.vertices[j].mark);
    if surf.mark = 1 then
      sec.surfaces.Delete(i);
  end;

  lev.sectors.Add(nsec);
  nsec.Renumber;

  { Delete unused vertices }
  for i := sec.vertices.Count - 1 downto 0 do
    if sec.vertices[i].mark = 0 then
      sec.Vertices.Delete(i);

  if (sec.vertices.Count = 0) or (sec.surfaces.Count = 0) then
    DeleteSector(lev, sc);

  lev.RenumSecs;
  JedMain.SectorAdded(nsec);
  result := nsec;
end;

Function BuildSurface(lev: TJKLevel; vxsel: TVXMultiSel): TJKSurface;
var
  i, asc, avx, sc, vx: integer;
  sec: TJKSector;
  surf: TJKSurface;
begin
  result := nil;
  if vxsel.Count < 3 then
  begin
    PanMessage(mt_info, 'Cannot create surface for sector: Less than 3 vertices selected!');
    exit;
  end;

  vxsel.GetSCVX(0, sc, vx);

  for i := 1 to vxsel.Count - 1 do
  begin
    vxsel.GetSCVX(i, asc, avx);
    if asc <> sc then
    begin
      PanMessageFmt(mt_warning, 'Cannot create surface for sector %d: The selected vertex %d is part of sector %d!', [sc, avx, asc]);
      exit;
    end;
  end;

  sec := lev.sectors[sc];
  surf := sec.newsurface;
  sec.surfaces.Add(surf);

  for i := 0 to vxsel.Count - 1 do
  begin
    vxsel.GetSCVX(i, asc, avx);
    surf.AddVertex(sec.Vertices[avx]);
  end;

  surf.NewRecalcAll;
  sec.Renumber;
  JedMain.SectorChanged(sec);
  result := surf;
end;

Procedure DeleteSector(lev: TJKLevel; n: integer; force: Boolean);
var
  sec: TJKSector;
begin
  if not CheckSectorIndex(lev, n) then
    exit;

  sec := lev.sectors[n];
  if not force and not ToolBar.IsLayerVisible(sec.layer) then
    exit;

  SaveSecUndo(sec, ch_deleted, sc_both);

  if JedMain.Cur_SC = n then
    JedMain.SetCurSC(Max(0, GetPreviousSector(lev, n)));

  RemoveSecRefs(lev, sec, rs_surfs);
  var cursec := lev.sectors[JedMain.Cur_SC];
  lev.sectors.Delete(n);
  JedMain.SectorDeleted(sec);
  sec.Free;

  lev.RenumSecs;
  JedMain.SetCurSC(cursec.num);
end;

Procedure DeleteSurface(lev: TJKLevel; sc, sf: integer; force: Boolean);
var
  sec: TJKSector;
  surf: TJKSurface;
begin
  if not CheckSectorIndex(lev, sc) then
    exit;
  if not CheckSectorSurfaceIndex(lev.sectors[sc], sf) then
    exit;

  sec  := lev.sectors[sc];
  surf := sec.surfaces[sf];
  if not force and not ToolBar.IsLayerVisible(sec.layer) then
    exit;

  RemoveSurfRefs(lev, surf);
  sec.surfaces.Delete(sf);
  surf.Free;

  sec.Renumber;
  JedMain.SectorChanged(sec);
end;

Procedure DeleteThing(lev: TJKLevel; n: integer; force: Boolean);
var
  sec: TJKSector;
  p, i, j: integer;
  th: TJKThing;
begin
  if not CheckThingIndex(lev, n) then
    exit;

  th := lev.things[n];
  if not force and not ToolBar.IsLayerVisible(th.layer) then
    exit;

  SaveThingUndo(th, ch_deleted);

  for i := 0 to lev.cogs.Count - 1 do
    with lev.cogs[i] do
      for j := 0 to Vals.Count - 1 do
        with Vals[j] do
        begin
          if (Cog_Type = ct_thg) and (obj = th) then
            obj := nil;
        end;

  if JedMain.Cur_TH = n then
    JedMain.SetCurTH(Max(0, GetPreviousThing(lev, n)));

  var curth := lev.things[JedMain.Cur_TH];
  lev.things.Delete(n);
  JedMain.ThingDeleted(th);
  th.Free;

  lev.RenumThings;
  JedMain.SetCurTH(curth.num);
end;

Function DeleteVertex(sec: TJKSector; vx: integer; silent: boolean): boolean;
var
  asf, sf1, sf2: TJKSurface;
  v1: TJKVertex;
  ns, sf, iv: integer;
  ed1, ed2: integer;
begin
  if not CheckSectorVertexIndex(sec, vx) then
    exit;

  if not ToolBar.IsLayerVisible(sec.layer) then
    exit;

  sf1 := nil;
  sf2 := nil;
  result := false;
  v1 := sec.vertices[vx];
  ns := 0;
  for sf := 0 to sec.surfaces.Count - 1 do
  begin
    asf := sec.surfaces[sf];
    iv := asf.vertices.IndexOf(v1);
    if iv = -1 then
      continue;
    Case ns of
      0:
        begin
          sf1 := asf;
          ed1 := iv;
        end;
      1:
        begin
          sf2 := asf;
          ed2 := iv;
        end;
    else
      begin
        if not silent then
          PanMessage(mt_error, 'More than two surfaces use this vertex');
        exit;
      end;
    end;
    inc(ns);
  end;

  if sf2 <> nil then
  begin
    if (sf1.vertices.count < 4) or (sf2.vertices.count < 4) then
    begin
      if not silent then
        PanMessage(mt_error, 'One of the surfaces has less than 4 vertices!');
      exit;
    end;

    { if (sf1.adjoin<>nil) or (sf2.adjoin<>nil) then
      begin
      PanMessage(mt_error,'Can''t delete vertices of adjoined surfaces');
      exit;
      end; }

    if (sf1.vertices[sf1.PrevVX(ed1)] <> sf2.vertices[sf2.NextVX(ed2)]) or
      (sf1.vertices[sf1.NextVX(ed1)] <> sf2.vertices[sf2.PrevVX(ed2)]) then
      exit;
  end;

  SaveSecUndo(sec, ch_changed, sc_geo);

  if sf1 <> nil then
    sf1.DeleteVertex(ed1);
  if sf2 <> nil then
    sf2.DeleteVertex(ed2);

  sec.vertices.Delete(vx);
  sec.Renumber;
  JedMain.SectorChanged(sec);
  result := true;
end;

Procedure DeleteLight(lev: TJKLevel; n: integer; force: Boolean);
begin
  if not CheckLightIndex(lev, n) then
    exit;

  var lt := lev.lights[n];
  if not force and not ToolBar.IsLayerVisible(lt.layer) then
    exit;

  SaveLightUndo(lt, ch_deleted);

  if JedMain.Cur_LT = n then
    JedMain.SetCurLT(Max(0, GetPreviousLight(lev, n)));

  lev.lights.Delete(n);
  JedMain.LightDeleted(lt);
  lt.Free;
end;

Procedure DeleteCog(lev: TJKLevel; n: integer);
var
  i, j: integer;
  cg: TCOG;
begin
  if not CheckCogIndex(lev, n) then
    exit;

  cg := lev.cogs[n];
  lev.cogs.Delete(n);
  cg.Free;

  for i := 0 to lev.cogs.Count - 1 do
    with lev.cogs[i] do
    begin
      for j := 0 to vals.Count - 1 do
        with vals[j] do
          if Cog_Type = ct_cog then
          begin
            if int = n then
              int := -1;
            if int > n then
              Dec(int);
          end;
    end;
  CogForm.RefreshList;
end;

Function SysFindCommonEdge(surf1, surf2: TJKSurface;
  var ed1, ed2: integer): boolean;
var
  v1, v2: TJKVertex;
  i, j: integer;
begin
  result := false;
  for i := 0 to surf1.vertices.Count - 1 do
  begin
    v1 := surf1.vertices[i];
    j := surf2.vertices.IndexOf(v1);
    if j = -1 then
      continue;
    if surf1.vertices[surf1.NextVX(i)] = surf2.vertices[surf2.PrevVX(j)] then
    begin
      ed1 := i;
      ed2 := surf2.PrevVX(j);
      result := true;
      exit;
    end;
  end;
end;

Function SysMergeSurfs(surf1, surf2: TJKSurface; edge1, edge2: integer)
  : TJKSurface;
var
  i, j: integer;
  ipos, nv: integer;
  v, lastv: integer;
  vx: TJKVertex;
  sf1, sf2: TJKSurface;

  Procedure DeleteVXIfunused(sec: TJKSector; v: TJKVertex);
  var
    i: integer;
    surf: TJKSurface;
  begin
    for i := 0 to sec.surfaces.count - 1 do
    begin
      surf := sec.surfaces[i];
      if surf = surf2 then
        continue;
      if surf.vertices.IndexOf(v) <> -1 then
        exit;
    end;

    i := sec.vertices.IndexOf(v);
    if i = -1 then
      exit;
    sec.vertices.Delete(i);
    v.free;
  end;

begin
  result := nil;
  nv := 0;

  // Prevent modifying locked sector
  if not ToolBar.IsLayerVisible(surf1.sector.Layer) then
    begin
      PanMessageFmt(mt_info, 'Can''t merge surface %d, %d - locked layer',
        [surf1.sector.num, surf1.num]);
      exit;
    end;
  if not ToolBar.IsLayerVisible(surf2.sector.Layer) then
    begin
      PanMessageFmt(mt_info, 'Can''t merge surface %d, %d - locked layer',
        [surf2.sector.num, surf2.num]);
      exit;
    end;

  if ((surf1.adjoin = nil) and (surf2.adjoin <> nil)) or
    ((surf2.adjoin = nil) and (surf1.adjoin <> nil)) then
  begin
    PanMessage(mt_error,
      'Can''t merge - one surface is adjoined, the other isn''t');
    exit;
  end;

  for i := 0 to surf1.vertices.Count - 1 do
  begin
    vx := surf1.vertices[i];
    if surf2.vertices.IndexOf(vx) = -1 then
      Inc(nv);
  end;

  for i := 0 to surf2.vertices.Count - 1 do
  begin
    vx := surf2.vertices[i];
    if surf1.vertices.IndexOf(vx) = -1 then
      Inc(nv);
  end;

  if nv > MaxSurfaceVertices(level) then
  begin
    PanMessageFmt(mt_error, 'Can''t merge surfaces - resulting surface exceeds max number of vertices (%d)!',
        [MaxSurfaceVertices(level)]);
    exit;
  end;

  { Merge surfaces }

  SaveSecUndo(surf1.sector, ch_changed, sc_both);

  v := surf2.PrevVX(edge2);
  lastv := surf2.NextVX(edge2);
  ipos  := surf1.NextVX(edge1);
  while v <> lastv do
  begin
    surf1.InsertVertex(ipos, surf2.vertices[v]);
    v := surf2.PrevVX(v);
  end;

  repeat
    nv := 0;
    With surf1 do
      for i := vertices.Count - 1 downto 0 do
      begin
        if vertices[PrevVX(i)] = vertices[NextVX(i)] then
        begin
          vx := vertices[i];
          surf1.DeleteVertex(i);
          DeleteVXIfunused(surf1.sector, vx);
          inc(nv);
        end;
      end;

    With surf1 do
      for i := vertices.Count - 1 downto 0 do
      begin
        if vertices[i] = vertices[NextVX(i)] then
        begin
          surf1.DeleteVertex(i);
          Inc(nv);
        end;
      end;

  Until nv = 0;

  surf1.RecalcAll;

  { if the surfaces were adjoined, try to merge the adjoins and readjoin }

  if (surf1.adjoin <> nil) and (surf2.adjoin <> nil) then
  begin
    sf1 := surf1.adjoin;
    sf2 := surf2.adjoin;

    ClearAdjoinParams(surf1);
    ClearAdjoinParams(surf2);

    if SysFindCommonEdge(sf1, sf2, i, j) then
    begin
      ClearAdjoinParams(sf1);
      ClearAdjoinParams(sf2);
      SysMergeSurfs(sf1, sf2, i, j);
      if DO_Surf_Overlap(surf1, sf1) then
      begin
        SysAdjoinSurfaces(surf1, sf1);
        JedMain.SectorChanged(sf1.sector);
      end;
    end;

  end;

  RemoveSurfRefs(surf1.sector.level, surf2);
  nv := surf2.sector.surfaces.IndexOf(surf2);
  surf2.sector.surfaces.Delete(nv);
  surf2.Free;

  surf1.sector.Renumber;
  JedMain.SectorChanged(surf1.sector);

  result := surf1;
end;

function MergeSurfaces(surf, surf1: TJKSurface): TJKSurface;
var
  ed1, ed2: integer;
begin
  result := nil;
  if SysFindCommonEdge(surf, surf1, ed1, ed2) then
    result := SysMergeSurfs(surf, surf1, ed1, ed2);
  if result <> nil then
  begin
    if not IsSurfPlanar(result) then
      PanMessageFmt(mt_info, 'The surface %d, %d is not planar',
        [surf.sector.Num, surf.Num]);
    if not IsSurfConvex(result) then
      PanMessageFmt(mt_info, 'The surface %d, %d is not convex',
        [surf.sector.Num, surf.Num]);
  end;
end;

function MergeSurfacesAt(surf: TJKSurface; edge: integer): boolean;
var
  av1, av2, v1, v2: TJKVertex;
  nv, sf, lastv, v: integer;
  sec: TJKSector;
  asf: TJKSurface;
  surf1: TJKSurface;
  edge1: integer;
  ipos: integer;
  nsf1: integer;
begin
  result := false;
  v1 := surf.vertices[edge];
  With surf do
    v2 := vertices[NextVX(edge)];

  surf1 := nil;
  sec := surf.sector;

  for sf := 0 to sec.surfaces.Count - 1 do
  begin
    asf := sec.surfaces[sf];
    if asf = surf then
      continue;

    for v := 0 to asf.vertices.Count - 1 do
    begin
      av1 := asf.vertices[v];
      av2 := asf.vertices[asf.NextVX(v)];
      if (v1 <> av2) or (v2 <> av1) then
        continue;

      if surf1 <> nil then
      begin
        PanMessage(mt_error, 'The edge is common to more than one surface');
        exit;
      end;

      surf1 := asf;
      edge1 := v;
      nsf1 := sf;
    end;
  end;

  if surf1 = nil then
  begin
    PanMessage(mt_error, 'The edge belongs to just one surface');
    exit;
  end;

  result := SysMergeSurfs(surf, surf1, edge, edge1) <> nil;
end;

function MergeSectors(sec, sec1: TJKSector): TJKSector;
var
  surf, surf1: TJKSurface;
  i, j: integer;
  sfs, sfs1: TSurfaces;
begin
  result := nil;
  if not ToolBar.IsLayerVisible(sec.layer) or
     not ToolBar.IsLayerVisible(sec1.layer) then
  begin
    PanMessageFmt(mt_info, 'Cannot merge sectors %d and %d - locked layer(s)', [sec.num, sec1.num]);
    exit;
  end;

  sfs := TSurfaces.Create;
  sfs1 := TSurfaces.Create;
  for i := 0 to sec.surfaces.Count - 1 do
  begin
    surf := sec.surfaces[i];
    if (surf.adjoin <> nil) and (surf.adjoin.sector = sec1) and
      DO_Surf_Overlap(surf, surf.adjoin) then
    begin
      sfs.Add(surf);
      sfs1.Add(surf.adjoin);
    end;
  end;

  if sfs.count = 0
    then PanMessageFmt(mt_warning, 'Sectors %d and %d are not adjoined or adjoins don''t overlap', [sec.num,sec1.num])
  else
    begin

      SaveSecUndo(sec, ch_changed, sc_both);
      SaveSecUndo(sec1, ch_deleted, sc_both);

      for i := sec1.surfaces.Count - 1 downto 0 do
      begin
        surf := sec1.surfaces[i];
        { If surface is to be deleted - skip it }
        if sfs1.IndexOf(surf) <> -1 then
          continue;

        { Add all vertices used in the surface to Sec }
        for j := 0 to surf.vertices.Count - 1 do
          with surf.vertices[j] do
            surf.Vertices[j] := sec.AddVertex(x, y, z);

        { Add surface to Sec and delete from Sec1 }
        surf.sector := sec;
        sec.surfaces.Add(surf);
        sec1.surfaces.Delete(i);
      end;

      RemoveSecRefs(sec1.level, sec1, 0);

      for i := 0 to sfs.Count - 1 do
        RemoveSurfRefs(sec1.level, sfs[i]);

      for i := 0 to sfs1.Count - 1 do
        RemoveSurfRefs(sec1.Level, sfs1[i]);

      for i := 0 to sfs.Count - 1 do
      begin
        j := sec.surfaces.IndexOf(sfs[i]);
        sec.surfaces.Delete(j);
        sfs[i].Free;
      end;

      sec.Renumber;

      JedMain.SectorChanged(sec);

      i := sec1.level.sectors.IndexOf(sec1);
      sec1.Level.sectors.Delete(i);
      JedMain.SectorDeleted(sec1);
      sec1.Free;

      sec.level.RenumSecs;
      result := sec;

      if not IsSectorConvex(sec) then
        PanMessageFmt(mt_info, 'The sector %d is not convex', [sec.num]);
    end;

  sfs.Free;
  sfs1.Free;

end;

function MergeVertices(lev: TJKLevel; vxsel: TVXMultiSel): integer;
// returns number of vertices merged
label next;
var
  i, j, nv2: integer;
  v1, v2: TJKVertex;
  sc, vx: integer;
  sec: TJKSector;
  sfchanged: boolean;
begin
  StartUndoRec('Merge Vertices');
  vxsel.sort;
  result := 0;

  i := 0;
  while i < vxsel.count - 1 do
  begin
    j := i + 1;
    while j < vxsel.count do
    begin
      vxsel.GetSCVX(i, sc, vx);
      v1 := lev.sectors[sc].Vertices[vx];
      vxsel.GetSCVX(j, sc, vx);
      v2 := lev.sectors[sc].Vertices[vx];
      nv2 := vx;

      if v1 = v2 then
        goto next;
      if v1.sector <> v2.sector then
        goto next;
      if not(IsClose(v1.x, v2.x) and IsClose(v1.y, v2.y) and IsClose(v1.z, v2.z)) then
        goto next;

      sec := v1.sector;
      if not ToolBar.IsLayerVisible(sec.Layer) then // Prevent modifying sector on invisible layer
        goto next;

      SaveSecUndo(sec, ch_changed, sc_geo);

      for sc := 0 to sec.surfaces.count - 1 do
        with sec.surfaces[sc] do
          for vx := Vertices.count - 1 downto 0 do
          begin
            sfchanged := false;
            if Vertices[vx] = v2 then
            begin
              Vertices[vx] := v1;
              if Vertices[NextVX(vx)] = v1 then
                Vertices.Delete(vx);
              if Vertices[PrevVX(vx)] = v1 then
                Vertices.Delete(vx);
              sfchanged := true;
            end;
            if sfchanged then
              Recalc;
          end;

      sec.Vertices.Delete(nv2);
      v2.free;
      inc(result);
      JedMain.SectorChanged(sec);
    next:
      inc(j);
    end;
    inc(i);
  end;

end;

Function DO_Surf_Overlap(surf, surf1: TJKSurface): boolean;
var
  vxs0, vxs1: TVertices;

  Function Check: boolean;
  var
    i, v, fv: integer;
    v0, v1, v2: TVertex;
    e1, e2: TVector;
  begin
    result := false;

    { Mark "important" vertices in surf }

    With surf do
      for i := 0 to Vertices.count - 1 do
      begin
        v0 := vertices[PrevVX(i)];
        v1 := vertices[i];
        v2 := vertices[NextVX(i)];

        e1.SetCoords(v1.x - v0.x, v1.y - v0.y, v1.z - v0.z);
        e2.SetCoords(v2.x - v1.x, v2.y - v1.y, v2.z - v1.z);

        //if not Normalize(e1) then
        if e1.Normalize = 0 then
          continue;

        //Normalize(e2);
        e2.Normalize;
        if e1 = e2 then
          continue;
//        if IsClose(e1.dx, e2.dx) and IsClose(e1.dy, e2.dy) and
//          IsClose(e1.dz, e2.dz) then
//          continue;
        vxs0.Add(v1);
      end;

    { Mark "important" vertices in surf1 }

    With surf1 do
      for i := 0 to Vertices.count - 1 do
      begin
        v0 := Vertices[PrevVX(i)];
        v1 := Vertices[i];
        v2 := Vertices[NextVX(i)];

        e1.SetCoords(v1.x - v0.x, v1.y - v0.y, v1.z - v0.z);
        e2.SetCoords(v2.x - v1.x, v2.y - v1.y, v2.z - v1.z);

        //if not Normalize(e1) then
        if e1.Normalize = 0 then
          continue;

        e2.Normalize;
        if e1 = e2 then
          continue;
//        Normalize(e2);
//        if IsClose(e1.dx, e2.dx) and IsClose(e1.dy, e2.dy) and
//          IsClose(e1.dz, e2.dz) then
//          continue;
        vxs1.Add(v1);
      end;

    if vxs0.count <> vxs1.count then
      exit;
    v0 := vxs0[0];

    fv := -1;
    for v := 0 to vxs1.count - 1 do
    begin
      v1 := vxs1[v];
      if IsClose(v0.x, v1.x) and IsClose(v0.y, v1.y) and IsClose(v0.z, v1.z)
      then
      begin
        fv := v;
        break;
      end;
    end;
    if fv = -1 then
      exit;

    fv := PrevIdx(fv, vxs1.count);
    For v := 1 to vxs0.count - 1 do
    begin
      v0 := vxs0[v];
      v1 := vxs1[fv];
      if not(IsClose(v0.x, v1.x) and IsClose(v0.y, v1.y) and IsClose(v0.z, v1.z))
      then
        exit;
      fv := PrevIdx(fv, vxs1.count);
    end;
    result := true;
  end;

begin { DO_Surf_Overlap }
  if (surf = nil) or (surf1 = nil) then
  begin
    result := false;
    exit;
  end;
  vxs0 := TVertices.Create;
  vxs1 := TVertices.Create;
  result := Check;
  vxs0.free;
  vxs1.free;
end;

Function DO_Surf_Match(surf, surf1: TJKSurface): boolean;
var
  i, v, fv: integer;
  v0, v1: TJKVertex;
begin
  result := false;
  if surf1.Vertices.count <> surf.Vertices.count then
    exit;
  v0 := surf.Vertices[0];
  fv := -1;
  for v := 0 to surf1.Vertices.count - 1 do
  begin
    v1 := surf1.Vertices[v];
    if IsClose(v0.x, v1.x) and IsClose(v0.y, v1.y) and IsClose(v0.z, v1.z) then
    begin
      fv := v;
      break;
    end;
  end;
  if fv = -1 then
    exit;

  fv := surf1.PrevVX(fv);
  For v := 1 to surf.Vertices.count - 1 do
  begin
    v0 := surf.Vertices[v];
    v1 := surf1.Vertices[fv];
    if not(IsClose(v0.x, v1.x) and IsClose(v0.y, v1.y) and IsClose(v0.z, v1.z))
    then
      exit;
    fv := surf1.PrevVX(fv);
  end;
  result := true;
end;

Function MakeAdjoin(surf: TJKSurface): boolean;
var
  s, sf: integer;
  lev: TJKLevel;
  surf1: TJKSurface;
begin
  result := false;
  if surf.adjoin <> nil then
    exit;

  if not ToolBar.IsLayerVisible(surf.sector.layer) then // prevent editing of locked sector
    exit;

  lev := surf.sector.Level;
  for s := 0 to lev.sectors.count - 1 do
    With lev.sectors[s] do
      for sf := 0 to surfaces.count - 1 do
      begin
        surf1 := surfaces[sf];
        if DO_Surf_Overlap(surf, surf1) and ToolBar.IsLayerVisible(surf1.sector.layer) then
        begin
          SaveSecUndo(surf.sector, ch_changed, sc_val);
          SaveSecUndo(surf1.sector, ch_changed, sc_val);

          SysAdjoinSurfaces(surf, surf1);
          JedMain.SectorChanged(surf.sector);
          JedMain.SectorChanged(surf1.sector);
          result := true;
          exit;
        end;
      end;
end;

Function MakeAdjoinSCUP(surf: TJKSurface; sc: integer): boolean;
var
  s, sf: integer;
  lev: TJKLevel;
  surf1: TJKSurface;
begin
  Result := False;
  if surf.adjoin <> nil then
    exit;
  lev := surf.sector.level;

  for s := sc to lev.sectors.Count - 1 do
    with lev.sectors[s] do
      for sf := 0 to surfaces.Count - 1 do
      begin
        surf1 := surfaces[sf];
        if DO_Surf_Overlap(surf, surf1) then
        begin
          SysAdjoinSurfaces(surf, surf1);
          JedMain.SectorChanged(surf.sector);
          JedMain.SectorChanged(surf1.sector);
          Result := True;
          exit;
        end;
      end;
end;

Function UnAdjoin(surf: TJKSurface): boolean;
var
  sec1: TJKSector;
begin
  result := false;
  if surf.adjoin = nil then
    exit;
  sec1 := surf.adjoin.sector;

  // Prevent editing of locked sectors
  if not ToolBar.IsLayerVisible(surf.sector.layer) or
     not ToolBar.IsLayerVisible(sec1.layer) then
     exit;

  if surf.adjoin.adjoin <> surf then
    begin
      SaveSecUndo(surf.sector, ch_changed, sc_val);
      PanMessageFmt(mt_warning, 'Incorrect reverse adjoin SC %d SF %d',
        [surf.sector.Num, surf.Num]);
      ClearAdjoinParams(surf);
      surf.adjoin := nil;
    end
  else
    begin
      SaveSecUndo(surf.sector, ch_changed, sc_val);
      SaveSecUndo(surf.adjoin.sector, ch_changed, sc_val);

      ClearAdjoinParams(surf);
      ClearAdjoinParams(surf.adjoin);

      surf.CheckIfFloor;
      surf.adjoin.CheckIfFloor;

      surf.adjoin.adjoin := nil;
      surf.adjoin := nil;
      JedMain.SectorChanged(sec1);
    end;
  JedMain.SectorChanged(surf.sector);
  result := true;
end;

Function UnAdjoinSectors(sec, sec1: TJKSector): boolean;
var
  i: integer;
  surf: TJKSurface;
begin
  result := false;

  // Prevent editing of invisible sectors
  if not ToolBar.IsLayerVisible(sec.layer) or
     not ToolBar.IsLayerVisible(sec1.layer)  then
          exit;

  for i := 0 to sec.surfaces.count - 1 do
  begin
    surf := sec.surfaces[i];
    if surf.adjoin = nil then
      continue;
    if surf.adjoin.sector = sec1 then
      result := result or UnAdjoin(surf);
  end;
end;

Function AdjoinSectors(sec, sec1: TJKSector): boolean;
var
  i: integer;
  surf: TJKSurface;

  Function AdjointoSec1(surf: TJKSurface): boolean;
  var
    i: integer;
    surf1: TJKSurface;
  begin
    result := false;
    if surf.adjoin <> nil then
      exit;
    for i := 0 to sec1.surfaces.count - 1 do
    begin
      surf1 := sec1.surfaces[i];
      if surf1.adjoin <> nil then
        continue;
      if DO_Surf_Overlap(surf, surf1) then
      begin
        result := true;
        SysAdjoinSurfaces(surf, surf1);
      end;
    end;
  end;

begin
  Result := false;

  // Prevent editing of invisible sectors
  if not ToolBar.IsLayerVisible(sec.layer) or
     not ToolBar.IsLayerVisible(sec1.layer)  then
          exit;

  for i := 0 to sec.surfaces.count - 1 do
  begin
    surf := sec.surfaces[i];
    Result := Result or AdjointoSec1(surf);
  end;

  if Result then
  begin
    JedMain.SectorChanged(sec);
    JedMain.SectorChanged(sec1);
  end;
end;

Procedure CreateCubeSec(s: TJKSector; x, y, z: double; const pnormal, edge: TVector);
var
  nsec: TJKSector;
  asurf, osurf, nsurf: TJKSurface;
  nvx: TJKVertex;
  i, v, vc: integer;
  enormal: TVector;
  a: double;
begin
  nsec := s;
  asurf := nsec.newsurface;
  nsec.surfaces.Add(asurf);

  // Returns cross product of pnormal x edge, i.e.: perpendicular up vector
  enormal := pnormal.Cross(edge).Normalized;
//  VMult(pnormal.dx, pnormal.dy, pnormal.dz, edge.dx, edge.dy, edge.dz, enormal.dx, enormal.dy, enormal.dz);
//  Normalize(enormal);

  // lower edge
  nvx := nsec.NewVertex;
  nvx.x := x;
  nvx.y := y;
  nvx.z := z;
  asurf.AddVertex(nvx);

  nvx := nsec.NewVertex;
  nvx.x := x + edge.dx;
  nvx.y := y + edge.dy;
  nvx.z := z + edge.dz;
  asurf.AddVertex(nvx);

  // upper edge
  a := edge.Length; //Vlen(edge);

  nvx := nsec.NewVertex;
  nvx.x := x + edge.dx + a * enormal.dx;
  nvx.y := y + edge.dy + a * enormal.dy;
  nvx.z := z + edge.dz + a * enormal.dz;
  asurf.AddVertex(nvx);

  nvx := nsec.NewVertex;
  nvx.x := x + a * enormal.dx;
  nvx.y := y + a * enormal.dy;
  nvx.z := z + a * enormal.dz;
  asurf.AddVertex(nvx);

  asurf.NewRecalcAll;

  { Construct opposite surface }
  osurf := nsec.newsurface;
  osurf.Assign(asurf);
  for v := asurf.Vertices.count - 1 downto 0 do
  begin
    nvx := nsec.NewVertex;
    With asurf.Vertices[v] do
    begin
      nvx.x := x + (asurf.normal.dx) * a;
      nvx.y := y + (asurf.normal.dy) * a;
      nvx.z := z + (asurf.normal.dz) * a;
    end;
    osurf.AddVertex(nvx);
  end;
  nsec.surfaces.Add(osurf);

  { Construct other surfaces }
  vc := asurf.Vertices.count;
  nsurf := nsec.newsurface;
  nsurf.Assign(asurf);

  for i := 0 to vc - 1 do
  begin
    nsurf := nsec.newsurface;
    nsurf.Assign(asurf);

    nsurf.AddVertex(asurf.Vertices[i]);

    if i = 0 then
      nsurf.AddVertex(asurf.Vertices[vc - 1])
    else
      nsurf.AddVertex(asurf.Vertices[i - 1]);

    if i = 0 then
      nsurf.AddVertex(osurf.Vertices[0])
    else
      nsurf.AddVertex(osurf.Vertices[vc - i]);

    nsurf.AddVertex(osurf.Vertices[vc - i - 1]);
    nsec.surfaces.Add(nsurf);
  end;
  for i := 1 to nsec.surfaces.count - 1 do
    nsec.surfaces[i].NewRecalcAll;
  nsec.Renumber;
end;

Procedure CreateCube(lev: TJKLevel; x, y, z: double; const pnormal, edge: TVector);
var
  nsec: TJKSector;
begin
  nsec := lev.NewSector;
  CreateCubeSec(nsec, x, y, z, pnormal, edge);
  nsec.Num := lev.sectors.count;
  lev.sectors.Add(nsec);
  JedMain.SectorAdded(nsec);
end;

function GetNearestVertexToLine(vertices: TVertices; lx, ly, lz: double; lineNormal: TVector; radius: double; var vx, vy, vz, minDistanceSqr: double): boolean;
begin
  Result := false;

  minDistanceSqr := sqr(radius);
  for var vix := 0 to vertices.Count - 1 do
    begin
      // Calculate vector rejection to get closest vector between view line ray and vertex
      //
      //        V
      //        x
      //       /|
      //      / |
      //  lv /  | p
      //    /   |
      //   /    |
      //  /     |
      // x--->--x------- line
      // L |
      //  line normal
      //
      var v := vertices[vix];
      var lv: TVector;
      VectorSub3(v.x, v.y, v.z, lx, ly, lz, lv.x, lv.y, lv.z);

      var p := lv - lv.ProjectOnToNormal(lineNormal);
      var dist := p.LengthSqr;
      if dist <= minDistanceSqr then
      begin
        minDistanceSqr := dist;
        vx := v.x;
        vy := v.y;
        vz := v.z;
        Result := true;
      end;
    end;
end;
function GetNearestVertexToLine(vertices: TVertices; lx, ly, lz: double; lineNormal: TVector; radius: double; var vx, vy, vz: double): boolean;
  var ds : double;
begin
  Result := GetNearestVertexToLine(vertices, lx, ly, lz, lineNormal, radius, vx, vy, vz, ds);
end;

function GetNearestWorldVertexToLine(lev: TJKLevel; lx, ly, lz: double; lineNormal: TVector; radius: double;  var vx, vy, vz: double): boolean;
  var dx, dy, dz, ddist: double;
begin
  vx := lx;
  vy := ly;
  vz := lz;
  Result := false;
  var minDistSqr := sqr(radius);

  for var s := 0 to lev.sectors.count - 1 do
    With lev.sectors[s] do
    begin
      if not ToolBar.IsLayerVisible(layer) then
        continue;

      if GetNearestVertexToLine(vertices, lx, ly, lz, lineNormal, radius, dx, dy, dz, ddist) then
      begin
        if ddist < minDistSqr then
        begin
          Result := true;
          vx := dx;
          vy := dy;
          vz := dz;
        end;
      end;
    end;
end;

function GetNearestWorldVertexToLine(lev: TJKLevel; linePoint: Tvector; lineNormal: TVector; radius: double;  var vx, vy, vz: double): boolean;
begin
  Result := GetNearestWorldVertexToLine(level, linePoint.x, linePoint.y, linePoint.z, lineNormal, radius, vx, vy, vz);
end;

function GetNearestWorldVertexToLine(lev: TJKLevel; linePoint: Tvector; lineNormal: TVector; radius: double;  var v: TVector): boolean; overload;
begin
  Result := GetNearestWorldVertexToLine(level, linePoint.x, linePoint.y, linePoint.z, lineNormal, radius, v.x, v.y, v.z);
end;

Procedure RotatePoint(ax1, ay1, az1, ax2, ay2, az2: double; angle: double;
  var x, y, z: double);
var
  nz, ny, nx: TVector;
  pX, pY, pZ: double;
  l, D: double;
  u, v: double;
begin
  nz.dx := ax2 - ax1;
  nz.dy := ay2 - ay1;
  nz.dz := az2 - az1;
  nz.Normalize;
  //Normalize(nz);

  D := nz.dx * x + nz.dy * y + nz.dz * z;
  PlaneLineXn(nz, D, ax1, ay1, az1, ax2, ay2, az2, pX, pY, pZ);
  nx.dx := x - pX;
  nx.dy := y - pY;
  nx.dz := z - pZ;
  l := nx.Normalize; //Vlen(nx);
  if IsClose(l, 0) then
    exit;
  //Normalize(nx);

  ny := nz.Cross(nx);
  //VMult(nz.dx, nz.dy, nz.dz, nx.dx, nx.dy, nx.dz, ny.dx, ny.dy, ny.dz);

  u := cos(angle / 180 * Pi) * l;
  v := sin(angle / 180 * Pi) * l;
  x := pX + nx.dx * u + ny.dx * v;
  y := pY + nx.dy * u + ny.dy * v;
  z := pZ + nx.dz * u + ny.dz * v;
end;

(* Procedure StitchSurface(surf:TJKSurface;nv1,nv2:integer;u1,v1,u2,v2:double);
  var vx1,vx2:TJKVertex;
  l2d,l3d,scale:double;
  du,cosa,sina:double;
  deu,dev,nu,nv:TVector;
  i:integer;
  begin
  vx1:=surf.Vertices[nv1]; vx2:=surf.Vertices[nv2];
  du:=u2-u1;

  {Find length of line vx1-vx2 in UV and XYZ coordinates}

  l2d:=sqrt(sqr(du)+sqr(v2-v1));
  deu.dx:=vx2.x-vx1.x; deu.dy:=vx2.y-vx1.y; deu.dz:=vx2.z-vx1.z;
  l3d:=VLen(deu);
  scale:=l2d/l3d;

  {Fina angle between u1v1-u2v2 and U axis}
  cosa:=du/l2d;
  sina:=sqrt(1-sqr(cosa));

  {Find V vector}
  Normalize(deu);
  With Surf.Normal do
  Vmult(dx,dy,dz,deu.dx,deu.dy,deu.dz,dev.dx,dev.dy,dev.dz);

  {Rotate U vector by angle and find V vector}
  nu.dx:=cosa*deu.dx+sina*dev.dx;
  nu.dy:=cosa*deu.dy+sina*dev.dy;
  nu.dz:=cosa*deu.dz+sina*dev.dz;

  With Surf.Normal do
  Vmult(dx,dy,dz,nu.dx,nu.dy,nu.dz,nv.dx,nv.dy,nv.dz);

  {Calculate U V coordinates}

  for i:=0 to Surf.Vertices.count-1 do
  With Surf.TXVertices[i],Surf.Vertices[i] do
  begin
  u:=u1+SMult(nu.dx,nu.dy,nu.dz,x-vx1.x,y-vx1.y,z-vx1.z)*Scale;
  v:=v1+SMult(nv.dx,nv.dy,nv.dz,x-vx1.x,y-vx1.y,z-vx1.z)*Scale;
  end;
  end;

  Function StitchAt(surf:TJKSurface;nv1,nv2:integer):integer;
  var un,vn:tvector;
  begin
  CalcUVNormalsFrom(surf,nv1,nv2,un,vn);
  ArrangeTexture(surf,nv1,un,vn);
  end;

  Function StitchAtEdge(surf:TJKSurface;edge:integer):integer;
  var i,j:integer;
  v1,v2:TJKVertex;
  tv1,tv2:TTXVertex;
  sec:TJKSector;
  adj,sf:TJKSurface;
  av1,av2:TJKVertex;
  begin
  Result:=0;
  v1:=surf.Vertices[edge];
  v2:=surf.Vertices[Surf.NextVX(edge)];
  tv1:=surf.TXVertices[edge];
  tv2:=surf.TXVertices[Surf.NextVX(edge)];

  sec:=surf.Sector;
  for i:=0 to sec.Surfaces.count-1 do
  begin
  sf:=sec.Surfaces[i];
  if sf=surf then continue;
  for j:=0 to sf.vertices.count-1 do
  begin
  if (sf.vertices[j]<>v2) or (sf.vertices[sf.NextVX(j)]<>v1) then continue;
  stitchSurface(sf,j,sf.nextVX(j),tv2.u,tv2.v,tv1.u,tv1.v);
  inc(result);
  end;
  if sf.adjoin=nil then continue;

  adj:=sf.adjoin;
  for j:=0 to adj.vertices.count-1 do
  begin
  av1:=adj.vertices[j];
  av2:=adj.vertices[adj.NextVX(j)];
  if not (IsClose(av1.x,v1.x) and IsClose(av1.y,v1.y) and IsClose(av1.z,v1.z)) then continue;
  if not (IsClose(av2.x,v2.x) and IsClose(av2.y,v2.y) and IsClose(av2.z,v2.z)) then continue;
  stitchSurface(adj,j,adj.nextVX(j),tv1.u,tv1.v,tv2.u,tv2.v);
  inc(result);
  end;

  end;
  end; *)

Procedure MultVM3(const mx: TMat3x3; var x, y, z: double);
var
  i, j, k: integer;
  s, r: array [0 .. 2] of double;
  sum: double;
begin
  s[0] := x;
  s[1] := y;
  s[2] := z;
  for i := 0 to 2 do
  begin
    sum := 0;
    for k := 0 to 2 do
      sum := sum + mx[i, k] * s[k];
    r[i] := sum;
  end;
  x := r[0];
  y := r[1];
  z := r[2];
end;

Procedure MultVM3s(const mx: TMat3x3s; var x, y, z: single);
var
  i, j, k: integer;
  s, r: array [0 .. 2] of single;
  sum: double;
begin
  s[0] := x;
  s[1] := y;
  s[2] := z;
  for i := 0 to 2 do
  begin
    sum := 0;
    for k := 0 to 2 do
      sum := sum + mx[i, k] * s[k];
    r[i] := sum;
  end;
  x := r[0];
  y := r[1];
  z := r[2];
end;

Procedure MultM3(var mx: TMat3x3; const bymx: TMat3x3);
var
  i, j, k: integer;
  sum: double;
  tmx: TMat3x3;
begin
  for i := 0 to 2 do
    for j := 0 to 2 do
    begin
      sum := 0;
      for k := 0 to 2 do
        sum := sum + mx[i, k] * bymx[k, j];
      tmx[i, j] := sum;
    end;
  mx := tmx;
end;

Procedure MultM3s(var mx: TMat3x3s; const bymx: TMat3x3s);
var
  i, j, k: integer;
  sum: single;
  tmx: TMat3x3s;
begin
  for i := 0 to 2 do
    for j := 0 to 2 do
    begin
      sum := 0;
      for k := 0 to 2 do
        sum := sum + mx[i, k] * bymx[k, j];
      tmx[i, j] := sum;
    end;
  mx := tmx;
end;

Procedure CreateMatrix(var mx: TMat3x3; angle: double; axis: integer);
var
  cosa, sina: double;
begin
  cosa := cos(angle / 180 * Pi);
  sina := sin(angle / 180 * Pi);
  case axis of
    rt_x:
      begin
        mx[0, 0] := 1;
        mx[0, 1] := 0;
        mx[0, 2] := 0;
        mx[1, 0] := 0;
        mx[1, 1] := cosa;
        mx[1, 2] := sina;
        mx[2, 0] := 0;
        mx[2, 1] := -sina;
        mx[2, 2] := cosa;
      end;
    rt_y:
      begin
        mx[0, 0] := cosa;
        mx[0, 1] := 0;
        mx[0, 2] := -sina;
        mx[1, 0] := 0;
        mx[1, 1] := 1;
        mx[1, 2] := 0;
        mx[2, 0] := sina;
        mx[2, 1] := 0;
        mx[2, 2] := cosa;
      end;
    rt_z:
      begin
        mx[0, 0] := cosa;
        mx[0, 1] := sina;
        mx[0, 2] := 0;
        mx[1, 0] := -sina;
        mx[1, 1] := cosa;
        mx[1, 2] := 0;
        mx[2, 0] := 0;
        mx[2, 1] := 0;
        mx[2, 2] := 1;
      end;
  end;
end;

Procedure CreateMatrixs(var mx: TMat3x3s; angle: single; axis: integer);
var
  cosa, sina: single;
begin
  cosa := cos(angle / 180 * Pi);
  sina := sin(angle / 180 * Pi);
  case axis of
    rt_x:
      begin
        mx[0, 0] := 1;
        mx[0, 1] := 0;
        mx[0, 2] := 0;
        mx[1, 0] := 0;
        mx[1, 1] := cosa;
        mx[1, 2] := sina;
        mx[2, 0] := 0;
        mx[2, 1] := -sina;
        mx[2, 2] := cosa;
      end;
    rt_y:
      begin
        mx[0, 0] := cosa;
        mx[0, 1] := 0;
        mx[0, 2] := -sina;
        mx[1, 0] := 0;
        mx[1, 1] := 1;
        mx[1, 2] := 0;
        mx[2, 0] := sina;
        mx[2, 1] := 0;
        mx[2, 2] := cosa;
      end;
    rt_z:
      begin
        mx[0, 0] := cosa;
        mx[0, 1] := sina;
        mx[0, 2] := 0;
        mx[1, 0] := -sina;
        mx[1, 1] := cosa;
        mx[1, 2] := 0;
        mx[2, 0] := 0;
        mx[2, 1] := 0;
        mx[2, 2] := 1;
      end;
  end;
end;

type
  TRotateData = record
    cx, cy, cz: double; { center }
    m3: TMat3x3;
  end;

Procedure CalcSecCenter(const sec: TJKSector; var cx, cy, cz: double);
var
  sx, sy, sz: double;
  i: integer;
begin
  sx := 0;
  sy := 0;
  sz := 0;
  for i := 0 to sec.vertices.Count - 1 do
    with sec.vertices[i] do
    begin
      sx := sx + x;
      sy := sy + y;
      sz := sz + z;
    end;

  cx := sx / sec.vertices.count;
  cy := sy / sec.vertices.count;
  cz := sz / sec.vertices.count;
end;

Procedure SysCalcSurfData(const surf: TJKSurface; var xv, yv: TVector;
  var minx, miny, maxx, maxy: double);
var
  v0, v1: TJKVertex;
  i: integer;
  x, y: double;
begin
  v0 := surf.vertices[0];
  v1 := surf.vertices[1];
  xv.SetCoords(v1.x - v0.x, v1.y - v0.y, v1.z - v0.z);

  //if not Normalize(xv) then;
  if xv.Normalize = 0 then;

  yv := surf.normal.Cross(xv);
//  With surf.normal do
//    VMult(dx, dy, dz, xv.dx, xv.dy, xv.dz, yv.dx, yv.dy, yv.dz);

  minx := 1E20;
  miny := 1E20;
  maxx := -1E20;
  maxy := -1E20;

  for i := 1 to surf.vertices.Count - 1 do
  begin
    v1 := surf.vertices[i];
    x := xv.Dot(v1.x - v0.x, v1.y - v0.y, v1.z - v0.z); // VectorDot3(v1.x - v0.x, v1.y - v0.y, v1.z - v0.z, xv.dx, xv.dy, xv.dz);   // SMult
    y := yv.Dot(v1.x - v0.x, v1.y - v0.y, v1.z - v0.z); // VectorDot3(v1.x - v0.x, v1.y - v0.y, v1.z - v0.z, yv.dx, yv.dy, yv.dz);
    if x < minx then
      minx := x;
    if y < miny then
      miny := y;
    if x > maxx then
      maxx := x;
    if y > maxy then
      maxy := y;
  end;

end;

Procedure CalcSurfRect(const surf: TJKSurface; var v1, v2, v3, v4: TJKVertex; by: double);
var
  xv, yv: TVector;
  minx, miny, maxx, maxy: double;
  v0: TVertex;
  x, y: double;

  Procedure CalcVX(x, y: double; v: TVertex);
  begin
    v.x := v0.x + x * xv.dx + y * yv.dx;
    v.y := v0.y + x * xv.dy + y * yv.dy;
    v.z := v0.z + x * xv.dz + y * yv.dz;
  end;

begin
  SysCalcSurfData(surf, xv, yv, minx, miny, maxx, maxy);
  v0 := surf.vertices[0];

  x := (maxx - minx) / 2;
  y := (maxy - miny) / 2;

  minx := x - by / 2;
  miny := y - by / 2;

  maxx := x + by / 2;
  maxy := y + by / 2;

  CalcVX(minx, miny, v1);
  CalcVX(minx, maxy, v2);
  CalcVX(maxx, maxy, v3);
  CalcVX(maxx, miny, v4);
end;

Procedure CalcSurfCenter(const surf: TJKSurface; var cx, cy, cz: double);
var
  xv, yv: TVector;
  minx, miny, maxx, maxy: double;
  x, y: double;
  v0: TVertex;
begin
  SysCalcSurfData(surf, xv, yv, minx, miny, maxx, maxy);
  x := (maxx - minx) / 2;
  y := (maxy - miny) / 2;
  v0 := surf.vertices[0];

  cx := v0.x + x * xv.dx + y * yv.dx;
  cy := v0.y + x * xv.dy + y * yv.dy;
  cz := v0.z + x * xv.dz + y * yv.dz;
end;

Procedure PrepareRotData(var rd: TRotateData; const axis: TVector;
  cx, cy, cz, angle: double);
Var
  vx, vy, vz: TVector;
begin
  rd.cx := cx;
  rd.cy := cy;
  rd.cz := cz;

  vx.SetCoords(1, 0, 0);
  vy.SetCoords(0, 1, 0);
  vz.SetCoords(0, 0, 1);

  RotatePoint(0, 0, 0, axis.dx, axis.dy, axis.dz, angle, vx.dx, vx.dy, vx.dz);
  RotatePoint(0, 0, 0, axis.dx, axis.dy, axis.dz, angle, vy.dx, vy.dy, vy.dz);
  RotatePoint(0, 0, 0, axis.dx, axis.dy, axis.dz, angle, vz.dx, vz.dy, vz.dz);

  { CreateMatrix(rd.m3,60,rt_x); }
  rd.m3[0, 0] := vx.dx;
  rd.m3[0, 1] := vx.dy;
  rd.m3[0, 2] := vx.dz;

  rd.m3[1, 0] := vy.dx;
  rd.m3[1, 1] := vy.dy;
  rd.m3[1, 2] := vy.dz;

  rd.m3[2, 0] := vz.dx;
  rd.m3[2, 1] := vz.dy;
  rd.m3[2, 2] := vz.dz;
end;


Procedure RotateXYZ(var ax, ay, az: double; const rd: TRotateData);
var
  x, y, z: double;
begin
  x := ax - rd.cx;
  y := ay - rd.cy;
  z := az - rd.cz;

  MultVM3(rd.m3, x, y, z);

  ax := x + rd.cx;
  ay := y + rd.cy;
  az := z + rd.cz;
end;

Procedure RotateVertex(v: TJKVertex; const rd: TRotateData);
var
  x, y, z: double;
begin
  if v.Mark <> 0 then
    exit;

  x := v.x - rd.cx;
  y := v.y - rd.cy;
  z := v.z - rd.cz;

  MultVM3(rd.m3, x, y, z);

  v.x := x + rd.cx;
  v.y := y + rd.cy;
  v.z := z + rd.cz;
  v.Mark := 1;
end;

Procedure RotateSectors(lev: TJKLevel; scsel: TSCMultiSel; axis: TVector;
  cx, cy, cz: double; angle: double);
{ Procedure RotateSector(sec:TJKSector; angle:double; axis:integer); }
var
  rd: TRotateData;
  sec: TJKSector;

  Procedure RotSec(sec: TJKSector);
  var
    i, j: integer;
  begin
    // First check that all affected sectors are on visible layer
    if not ToolBar.IsLayerVisible(sec.layer) then
      exit;
    for i := 0 to sec.surfaces.count - 1 do   // check adjoin sectors
      with sec.surfaces[i] do
        if (adjoin <> nil) and (not ToolBar.IsLayerVisible(adjoin.sector.layer)) then
          exit;

    SaveSecUndo(sec, ch_changed, sc_geo);
    for i := 0 to sec.vertices.Count - 1 do
      RotateVertex(sec.vertices[i], rd);

    for i := 0 to sec.surfaces.Count - 1 do
      with sec.surfaces[i] do
      begin
        if adjoin = nil then
          continue;

        SaveSecUndo(adjoin.sector, ch_changed, sc_geo);
        for j := 0 to adjoin.vertices.Count - 1 do
          RotateVertex(adjoin.vertices[j], rd);
      end;

    RecalcSector(sec);
    JedMain.SectorChanged(sec);

    for i := 0 to sec.surfaces.count - 1 do
      with sec.surfaces[i] do
      begin
        if adjoin = nil then
          continue;
        JedMain.SectorChanged(adjoin.sector);
      end;
  end;

var
  i: integer;
begin
  PrepareRotData(rd, axis, cx, cy, cz, angle);
  ClearVXMarks(lev);
  for i := 0 to scsel.count - 1 do
    RotSec(lev.sectors[scsel.getSC(i)]);
end;

Procedure RotateSurfaces(level: TJKLevel; sfsel: TSFMultiSel; axis: TVector;
  cx, cy, cz: double; angle: double);
var
  rd: TRotateData;

  Procedure RotSurf(surf: TJKSurface);
  var
    i, j: integer;
  begin
    // First check that all affected sectors are on visible layer
    if not ToolBar.IsLayerVisible(surf.sector.layer) then
      exit;
    if (surf.adjoin <> nil) and not ToolBar.IsLayerVisible(surf.adjoin.sector.layer) then
      exit;

    SaveSecUndo(surf.sector, ch_changed, sc_geo);
    for i := 0 to surf.vertices.Count - 1 do
      RotateVertex(surf.vertices[i], rd);

    if surf.adjoin <> nil then
      begin
        SaveSecUndo(surf.adjoin.sector, ch_changed, sc_geo);
        for j := 0 to surf.Adjoin.Vertices.count - 1 do
          RotateVertex(surf.Adjoin.Vertices[j], rd);
      end;

    RecalcSector(surf.Sector);
    JedMain.SectorChanged(surf.Sector);
    if surf.Adjoin <> nil then
      JedMain.SectorChanged(surf.Adjoin.Sector);
  end;

var
  i, sc, sf: integer;

begin
  PrepareRotData(rd, axis, cx, cy, cz, angle);
  ClearVXMarks(level);
  for i := 0 to sfsel.count - 1 do
    begin
      sfsel.GetSCSF(i, sc, sf);
      var surf := level.sectors[sc].surfaces[sf];
      RotSurf(surf);
    end;
end;

procedure RotatePYR(var pitch, yaw, roll: double; const axis: TVector; const angle: double);
    function ClampAngle(const angle: double) : double;
    begin
        Result := angle;
        if Result > 359 then
            Result := Result - 360//*(Result / 360)
        else if Result < -359 then
            Result := Result + 360;;//*(-Result / 360);
    end;

begin
    pitch := ClampAngle(pitch + axis.x * angle);
    yaw   := ClampAngle(yaw   + axis.z * angle);
    roll  := ClampAngle(roll  + axis.y * angle);
end;

Procedure RotateThings(lev: TJKLevel; thsel: TTHMultiSel; axis: TVector;
  cx, cy, cz: double; angle: double; bRotatePYR: Boolean);
var
  rd: TRotateData;
  i: integer;
  th: TJKThing;
begin
  // TODO: when object is rotated in any of the axis the new rotation is wrong.

  PrepareRotData(rd, axis, cx, cy, cz, angle);
  for i := 0 to thsel.count - 1 do
  begin
    // First check that thing is on visible layer
    th := lev.things[thsel.GetTH(i)];
    if not ToolBar.IsLayerVisible(th.layer) then
      continue;

    SaveThingUndo(th, ch_changed);
    if bRotatePYR then
      begin
        RotatePYR(th.PCH, th.YAW, th.ROL, axis, -angle);
      end;

    RotateXYZ(th.x, th.y, th.z, rd);
    JedMain.ThingChanged(th);
  end;
end;

Procedure RotateLights(lev: TJKLevel; ltsel: TLTMultiSel; axis: TVector;
  cx, cy, cz: double; angle: double);
var
  rd: TRotateData;
  i: integer;
  lt: TSedLight;
begin
  PrepareRotData(rd, axis, cx, cy, cz, angle);
  for i := 0 to ltsel.Count - 1 do
  begin
    // First check that light is on visible layer
    lt := lev.lights[ltsel.GetLT(i)];
    if not ToolBar.IsLayerVisible(lt.layer) then
      continue;

    SaveLightUndo(lt, ch_changed);
    RotateXYZ(lt.position.x, lt.position.y, lt.position.z, rd);
    JedMain.LightChanged(lt);
  end;
end;

Procedure RotateFrames(lev: TJKLevel; frsel: TFRMultisel; axis: TVector;
  cx, cy, cz: double; angle: double; bRotatePYR: Boolean);
var
  rd: TRotateData;
  i: integer;
  th, fr: integer;
  vl: TTPLValue;
  x, y, z, pch, yaw, rol: double;
begin
  PrepareRotData(rd, axis, cx, cy, cz, angle);
  for i := 0 to frsel.Count - 1 do
  begin
    frsel.GetTHFR(i, th, fr);
    var thing := lev.things[th];
    if not ToolBar.IsLayerVisible(thing.layer) then
      continue;

    SaveThingUndo(thing, ch_changed);

    vl := thing.vals[fr];
    vl.GetFrame(x, y, z, pch, yaw, rol);
    RotateXYZ(x, y, z, rd);
    if bRotatePYR then
      RotatePYR(pch, yaw, rol, axis, -angle);

    vl.SetFrame(x, y, z, pch, yaw, rol);
    JedMain.ThingChanged(thing);
  end;
end;

Procedure ScaleVertex(v: TJKVertex; const sd: TScaleData);
var
  D: double;
begin
  if v.Mark <> 0 then
    exit;
  case sd.how of
    scale_xyz:
      begin
        v.x := sd.cx + sd.sfactor * (v.x - sd.cx);
        v.y := sd.cy + sd.sfactor * (v.y - sd.cy);
        v.z := sd.cz + sd.sfactor * (v.z - sd.cz);
      end;
    scale_x:
      v.x := sd.cx + sd.sfactor * (v.x - sd.cx);
    scale_y:
      v.y := sd.cy + sd.sfactor * (v.y - sd.cy);
    scale_z:
      v.z := sd.cz + sd.sfactor * (v.z - sd.cz);
    scale_vec:
      begin
        D := VectorDot3(v.x - sd.cx, v.y - sd.cy, v.z - sd.cz, sd.vec.dx, sd.vec.dy,
          sd.vec.dz); // SMult
        D := (sd.sfactor - 1) * D;
        v.x := v.x + D * sd.vec.dx;
        v.y := v.y + D * sd.vec.dy;
        v.z := v.z + D * sd.vec.dz;
      end;
  end;
  v.Mark := 1;
end;

Procedure ScaleSectors(lev: TJKLevel; scsel: TSCMultiSel; const sd: TScaleData;
  scaletx: boolean);
{ Procedure ScaleSector(sec:TJKSector; sfactor:double); }
  Procedure ScaleSec(sec: TJKSector);
  var
    i, j: integer;
  begin
    // First check that all affected sectors are on visible layer
    if not ToolBar.IsLayerVisible(sec.layer) then
      exit;
    for i := 0 to sec.surfaces.count - 1 do   // check adjoin sectors
      with sec.surfaces[i] do
        if (adjoin <> nil) and (not ToolBar.IsLayerVisible(adjoin.sector.layer)) then
          exit;

    SaveSecUndo(sec, ch_changed, sc_geo);

    for i := 0 to sec.vertices.Count - 1 do
      ScaleVertex(sec.vertices[i], sd);

    for i := 0 to sec.surfaces.Count - 1 do
      With sec.surfaces[i] do
      begin
        if scaletx then
          begin
            uscale := uscale / sd.sfactor;
            vscale := vscale / sd.sfactor;
          end;
        if adjoin = nil then
          continue;

        SaveSecUndo(adjoin.sector, ch_changed, sc_geo);

        if scaletx then
        begin
          adjoin.uscale := adjoin.uscale / sd.sfactor;
          adjoin.vscale := adjoin.vscale / sd.sfactor;
        end;
        for j := 0 to adjoin.vertices.Count - 1 do
          ScaleVertex(adjoin.vertices[j], sd);
      end;

    RecalcSector(sec);
    JedMain.SectorChanged(sec);

    for i := 0 to sec.surfaces.Count - 1 do
      with sec.surfaces[i] do
      begin
        if adjoin = nil then
          continue;
        JedMain.SectorChanged(adjoin.sector);
      end;
  end;

begin
  ClearVXMarks(Level);
  for var i := 0 to scsel.count - 1 do
    ScaleSec(lev.sectors[scsel.getSC(i)]);
end;


Procedure ScaleSurfaces(level: TJKLevel; sfsel: TSFMultiSel; const sd: TScaleData;
  scaletx: boolean);
var
  sc, sf: integer;

  Procedure ScaleSurf(surf: TJKSurface);
  var
    i, j: integer;
  begin
    // First check that all affected sectors are on visible layer
    if not ToolBar.IsLayerVisible(surf.sector.layer) then
      exit;
    if (surf.adjoin <> nil) and not ToolBar.IsLayerVisible(surf.adjoin.sector.layer) then
      exit;

    with surf do
    begin
      SaveSecUndo(Sector, ch_changed, sc_geo);

      for i := 0 to vertices.Count - 1 do
        ScaleVertex(vertices[i], sd);

      if scaletx then
        begin
          uscale := uscale / sd.sfactor;
          vscale := vscale / sd.sfactor;
        end;

      if adjoin <> nil then
        begin
          SaveSecUndo(adjoin.sector, ch_changed, sc_geo);

          if scaletx then
            begin
              adjoin.uscale := adjoin.uscale / sd.sfactor;
              adjoin.vscale := adjoin.vscale / sd.sfactor;
            end;
          for j := 0 to adjoin.vertices.Count - 1 do
            ScaleVertex(adjoin.vertices[j], sd);
        end;

      RecalcSector(sector);
      JedMain.SectorChanged(sector);
      if adjoin <> nil then
        JedMain.SectorChanged(adjoin.sector);
    end;
  end;

begin
  ClearVXMarks(Level);

  for var i := 0 to sfsel.Count - 1 do
  begin
    sfsel.GetSCSF(i, sc, sf);
    var surf := level.sectors[sc].surfaces[sf];
    ScaleSurf(surf);
  end;
end;

Procedure ScaleXYZ(var x, y, z: double; const sd: TScaleData);
var
  D: double;
begin
  case sd.how of
    scale_xyz:
      begin
        x := sd.cx + sd.sfactor * (x - sd.cx);
        y := sd.cy + sd.sfactor * (y - sd.cy);
        z := sd.cz + sd.sfactor * (z - sd.cz);
      end;
    scale_x:
      x := sd.cx + sd.sfactor * (x - sd.cx);
    scale_y:
      y := sd.cy + sd.sfactor * (y - sd.cy);
    scale_z:
      z := sd.cz + sd.sfactor * (z - sd.cz);
    scale_vec:
      begin
//        D := VectorDot3(x - sd.cx, y - sd.cy, z - sd.cz, sd.vec.dx, sd.vec.dy,
//          sd.vec.dz); //SMult

        D := sd.vec.Dot(x - sd.cx, y - sd.cy, z - sd.cz);
        D := sd.sfactor - D;
        x := x + D * sd.vec.dx;
        y := y + D * sd.vec.dy;
        z := z + D * sd.vec.dz;
      end;
  end;
end;

Procedure ScaleThings(lev: TJKLevel; thsel: TTHMultiSel; const sd: TScaleData);
var
  rd: TRotateData;
  i: integer;
  th: TJKThing;
begin
  for i := 0 to thsel.Count - 1 do
  begin
    // Check that thing is on visible layer
    th := lev.things[thsel.GetTH(i)];
    if not ToolBar.IsLayerVisible(th.layer) then
      continue;

    SaveThingUndo(th, ch_changed);
    ScaleXYZ(th.x, th.y, th.z, sd);
    JedMain.ThingChanged(th);
  end;
end;

Procedure ScaleLights(lev: TJKLevel; ltsel: TLTMultiSel; const sd: TScaleData);
var
  rd: TRotateData;
  i: integer;
  lt: TSedLight;
begin
  for i := 0 to ltsel.Count - 1 do
  begin
    // Check that light is on visible layer
    lt := lev.lights[ltsel.GetLT(i)];
    if not ToolBar.IsLayerVisible(lt.layer) then
      continue;

    SaveLightUndo(lt, ch_changed);
    ScaleXYZ(lt.position.x, lt.position.y, lt.position.z, sd);
    JedMain.LightChanged(lt);
  end;
end;

Procedure ScaleFrames(lev: TJKLevel; frsel: TFRMultisel; const sd: TScaleData);
var
  i: integer;
  th, fr: integer;
  vl: TTPLValue;
  x, y, z, pch, yaw, rol: double;
begin
  for i := 0 to frsel.Count - 1 do
  begin
    frsel.GetTHFR(i, th, fr);
    var thing := lev.things[th];

    // Check that light is on visible layer
    if not ToolBar.IsLayerVisible(thing.layer) then
      continue;

    SaveThingUndo(thing, ch_changed);
    vl := thing.vals[fr];
    vl.GetFrame(x, y, z, pch, yaw, rol);

    ScaleXYZ(x, y, z, sd);
    vl.SetFrame(x, y, z, pch, yaw, rol);
    JedMain.ThingChanged(thing);
  end;
end;

Procedure FlipVertex(v: TJKVertex; cx, cy, cz: double; how: integer);
begin
  if v.mark <> 0 then
    exit;
  case how of
    rt_x:
      v.x := cx - (v.x - cx);
    rt_y:
      v.y := cy - (v.y - cy);
    rt_z:
      v.z := cz - (v.z - cz);
  end;
  v.Mark := 1;
end;

Procedure FlipXYZ(var x, y, z: double; const pnorm: TVector; cx, cy, cz: double);
var
  D: double;
begin
  D := pnorm.Dot(x - cx, y - cy, z - cz); //SMult(pnorm.dx, pnorm.dy, pnorm.dz, x - cx, y - cy, z - cz);
  x := x - pnorm.dx * 2 * D;
  y := y - pnorm.dy * 2 * D;
  z := z - pnorm.dz * 2 * D;
end;

Procedure FlipVertexOverPlane(v: TJKVertex; const pnorm: TVector; cx, cy, cz: double);
var
  D: double;
begin
  if v.mark <> 0 then
    exit;
//  D := pnorm.Dot(v.x - cx, v.y - cy, v.z - cz); //SMult(pnorm.dx, pnorm.dy, pnorm.dz, v.x - cx, v.y - cy, v.z - cz);
//  v.x := v.x - pnorm.dx * 2 * D;
//  v.y := v.y - pnorm.dy * 2 * D;
//  v.z := v.z - pnorm.dz * 2 * D;
  FlipXYZ(v.x, v.y, v.z, pnorm, cx, cy, cz);
  v.Mark := 1;
end;

Procedure FlipSurface(surf: TJKSurface);
var
  n, i: integer;
  vx: TJKVertex;
  tv: TTXVertex;
begin
  for i := 0 to (surf.vertices.count div 2) - 1 do
    begin
      n := surf.vertices.count - i - 1;
      vx := surf.vertices[i];
      surf.vertices[i] := surf.vertices[n];
      surf.vertices[n] := vx;

      tv := surf.txvertices[i];
      surf.txvertices[i] := surf.txvertices[n];
      surf.txvertices[n] := tv;
    end;

  surf.Recalc;

  if IsTXFlipped(surf) then
    BitSet(surf.FaceFlags, Integer(FF_SF_FLIP))
  else
    BitClear(surf.FaceFlags, Integer(FF_SF_FLIP));
end;

Procedure FlipThingsOverPlane(lev: TJKLevel; thsel: TTHMultiSel; const pnorm: TVector;
  cx, cy, cz: double);
var
  th: TJKThing;
  i: integer;
  D: double;
begin
  for i := 0 to thsel.Count - 1 do
  begin
    th := lev.things[thsel.GetTH(i)];
    if not ToolBar.IsLayerVisible(th.layer) then
      continue;

    SaveThingUndo(th, ch_changed);
    FlipXYZ(th.x, th.y, th.z, pnorm, cx, cy, cz);
    JedMain.ThingChanged(th);
  end;
end;

Procedure FlipLightsOverPlane(lev: TJKLevel; ltsel: TLTMultiSel; const pnorm: TVector;
  cx, cy, cz: double);
var
  rd: TRotateData;
  i: integer;
  lt: TSedLight;
begin
  for i := 0 to ltsel.Count - 1 do
  begin
    lt := lev.lights[ltsel.GetLT(i)];
    if not ToolBar.IsLayerVisible(lt.layer) then
      continue;

    SaveLightUndo(lt, ch_changed);
    FlipXYZ(lt.position.x, lt.position.y, lt.position.z, pnorm, cx, cy, cz);
    JedMain.LightChanged(lt);
  end;
end;

Procedure FlipFramesOverPlane(lev: TJKLevel; frsel: TFRMultisel; const pnorm: TVector;
  cx, cy, cz: double);
var
  i: integer;
  th, fr: integer;
  vl: TTPLValue;
  x, y, z, pch, yaw, rol: double;
begin
  for i := 0 to frsel.Count - 1 do
  begin
    frsel.GetTHFR(i, th, fr);
    var thing := lev.things[th];
    if not ToolBar.IsLayerVisible(thing.layer) then
      continue;

    SaveThingUndo(thing, ch_changed);

    vl := thing.Vals[fr];
    vl.GetFrame(x, y, z, pch, yaw, rol);
    FlipXYZ(x, y, z, pnorm, cx, cy, cz);

    vl.SetFrame(x, y, z, pch, yaw, rol);
    JedMain.ThingChanged(thing);
  end;
end;

Procedure FlipThings(lev: TJKLevel; thsel: TTHMultiSel; cx, cy, cz: double;
  how: integer);
var
  th: TJKThing;
  i: integer;
begin
  for i := 0 to thsel.Count - 1 do
  begin
    th := lev.things[thsel.GetTH(i)];
    if not ToolBar.IsLayerVisible(th.layer) then
      continue;

    SaveThingUndo(th, ch_changed);
    case how of
      rt_x:
        th.x := cx - (th.x - cx);
      rt_y:
        th.y := cy - (th.y - cy);
      rt_z:
        th.z := cz - (th.z - cz);
    end;
    JedMain.ThingChanged(th);
  end;
end;

Procedure FlipSectorsOverPlane(lev: TJKLevel; scsel: TSCMultiSel;
  const pnorm: TVector; pX, pY, pZ: double);
var
  sec: TJKSector;
  i, j: integer;
  surf: TJKSurface;
begin
  ClearVXMarks(lev);
  for i := 0 to scsel.Count - 1 do
  begin
    sec := lev.sectors[scsel.getSC(i)];
    if not ToolBar.IsLayerVisible(sec.layer) then
      continue;

    SaveSecUndo(sec, ch_changed, sc_geo);

    for j := 0 to sec.surfaces.count - 1 do
    begin
      surf := sec.surfaces[j];
      if surf.adjoin = nil then
        continue;
      if scsel.FindSC(surf.adjoin.sector.Num) = -1 then
        UnAdjoin(surf);
    end;
  end;

  for i := 0 to scsel.count - 1 do
  begin
    sec := lev.sectors[scsel.getSC(i)];
    if not ToolBar.IsLayerVisible(sec.layer) then
      continue;

    for j := 0 to sec.Vertices.count - 1 do
      FlipVertexOverPlane(sec.Vertices[j], pnorm, pX, pY, pZ);

    for j := 0 to sec.surfaces.count - 1 do
      FlipSurface(sec.surfaces[j]);
    JedMain.SectorChanged(sec);
  end;

end;

Procedure FlipSectors(lev: TJKLevel; scsel: TSCMultiSel; cx, cy, cz: double;
  how: integer);
var
  sec: TJKSector;
  i, j: integer;
  surf: TJKSurface;
begin
  ClearVXMarks(lev);
  for i := 0 to scsel.count - 1 do
  begin
    sec := lev.sectors[scsel.getSC(i)];
    // First check that sector is on visible layer
    if not ToolBar.IsLayerVisible(sec.layer) then
      continue;

    SaveSecUndo(sec, ch_changed, sc_geo);

    for j := 0 to sec.surfaces.count - 1 do
    begin
      surf := sec.surfaces[j];
      if surf.adjoin = nil then
        continue;
      if scsel.FindSC(surf.adjoin.sector.num) = -1 then
        UnAdjoin(surf);
    end;
  end;

  for i := 0 to scsel.count - 1 do
  begin
    sec := lev.sectors[scsel.getSC(i)];
    if not ToolBar.IsLayerVisible(sec.layer) then
      continue;

    for j := 0 to sec.Vertices.count - 1 do
      FlipVertex(sec.Vertices[j], cx, cy, cz, how);

    for j := 0 to sec.surfaces.count - 1 do
      FlipSurface(sec.surfaces[j]);
    JedMain.SectorChanged(sec);
  end;

end;

Procedure FlipSurfaces(lev: TJKLevel; sfsel: TSFMultiSel);
var
  i: integer;
  sc, sf: integer;
  surf: TJKSurface;
begin
  for i := 0 to sfsel.Count - 1 do
  begin
    sfsel.GetSCSF(i, sc, sf);
    surf := level.sectors[sc].surfaces[sf];
    if not ToolBar.IsLayerVisible(surf.sector.layer) then
      continue;

    FlipSurface(surf);
    JedMain.SectorChanged(surf.sector);
  end;
end;

Function FindCommonEdges(const surf, withsurf: TJKSurface;
  var v11, v12, v21, v22: integer): boolean;
var
  v1, v2: TJKVertex;
  i, j, n11, n12, n21, n22: integer;

  Function FindVX(v: TJKVertex; asurf: TJKSurface): integer;
  var
    i: integer;
    v2: TJKVertex;
  begin
    result := -1;
    for i := 0 to asurf.vertices.Count - 1 do
    begin
      v2 := asurf.vertices[i];
      if IsClose(v.x, v2.x) and IsClose(v.y, v2.y) and IsClose(v.z, v2.z) then
      begin
        result := i;
        exit;
      end;
    end;
  end;

begin
  Result := False;

  n11 := -1;
  n12 := -1;
  for i := 0 to surf.vertices.Count - 1 do
    begin
      n21 := FindVX(surf.vertices[i], withsurf);
      if n21 <> -1 then
      begin
        n11 := i;
        break;
      end;
    end;

  if n11 = -1 then
    exit;

  n12 := surf.NextVX(n11);
  n22 := FindVX(surf.vertices[n12], withsurf);
  if n22 = -1 then
    begin
      n12 := surf.PrevVX(n11);
      n22 := FindVX(surf.vertices[n12], withsurf);
    end;

  if n22 = -1 then
    exit;

  v11 := n11;
  v12 := n12;
  v21 := n21;
  v22 := n22;
  Result := True;
end;

Function FindCommonVXs(const surf, withsurf: TJKSurface; var v1, v2: integer)
  : boolean;
  Function FindVX(v: TJKVertex; asurf: TJKSurface): integer;
  var
    i: integer;
    v2: TJKVertex;
  begin
    Result := -1;

    for i := 0 to asurf.vertices.Count - 1 do
    begin
      v2 := asurf.Vertices[i];
      if IsClose(v.x, v2.x) and IsClose(v.y, v2.y) and IsClose(v.z, v2.z) then
        begin
          result := i;
          exit;
        end;
    end;
  end;

var
  i, j: integer;
begin
  Result := False;

  for i := 0 to surf.vertices.Count - 1 do
  begin
    j := FindVX(surf.vertices[i], withsurf);
    if j <> -1 then
      begin
        v1 := i;
        v2 := j;
        Result := True;
        exit;
      end;
  end;
end;

Function StitchSurfaces(surf, surf1: TJKSurface): boolean;
var
  lv, p1v, p2v: TVector;
  un, vn: TVector;
  nv1, nv2: integer;
  a, b: double;
begin
  If FindCommonVXs(surf, surf1, nv1, nv2) then
    CalcUVNormalsFromVX(surf, nv1, un, vn)
  else
    begin
      CalcUVNormals(surf, un, vn);
      nv1 := -1;
      nv2 := 0;
    end;

  lv := surf.normal.Cross(surf1.normal);
//  VMult(surf.normal.dx, surf.normal.dy, surf.normal.dz, surf1.normal.dx,
//    surf1.normal.dy, surf1.normal.dz, lv.dx, lv.dy, lv.dz);

  //if Normalize(lv) then
  if lv.Normalize <> 0 then
  begin
    p1v := surf.normal.Cross(lv);
//    VMult(surf.normal.dx, surf.normal.dy, surf.normal.dz, lv.dx, lv.dy, lv.dz,
//      p1v.dx, p1v.dy, p1v.dz);

    p2v := surf1.normal.Cross(lv);
//    VMult(surf1.normal.dx, surf1.normal.dy, surf1.normal.dz, lv.dx, lv.dy,
//      lv.dz, p2v.dx, p2v.dy, p2v.dz);

    a := p1v.Dot(un); //SMult(p1v.dx, p1v.dy, p1v.dz, un.dx, un.dy, un.dz);
    b := lv.Dot(un); //SMult(lv.dx, lv.dy, lv.dz, un.dx, un.dy, un.dz);
    un := p2v * a + lv * b;
//    un.SetCoords(a * p2v.dx + b * lv.dx, a * p2v.dy + b * lv.dy,
//      a * p2v.dz + b * lv.dz);

    a := p1v.Dot(vn); // SMult(p1v.dx, p1v.dy, p1v.dz, vn.dx, vn.dy, vn.dz);
    b := lv.Dot(vn); // SMult(lv.dx, lv.dy, lv.dz, vn.dx, vn.dy, vn.dz);
    vn := p2v * a + lv * b;
//    vn.SetCoords(a * p2v.dx + b * lv.dx, a * p2v.dy + b * lv.dy,
//      a * p2v.dz + b * lv.dz);
  end;

  SaveSecUndo(surf1.sector, ch_changed, sc_both);

  if nv1 <> -1 then
  begin
    surf1.txvertices[nv2].u := surf.txvertices[nv1].u;
    surf1.txvertices[nv2].v := surf.txvertices[nv1].v;
  end;

  surf1.uscale := surf.uscale;
  surf1.vscale := surf.vscale;

  if BitMaskTest(surf.FaceFlags, FF_SF_FLIP) then
    BitSet(surf1.FaceFlags, FF_SF_FLIP)
  else
    BitClear(surf1.FaceFlags, FF_SF_FLIP);

  ArrangeTexture(surf1, nv2, un, vn);
end;
(* Function StitchSurfaces(surf,surf1:TJKSurface):boolean;
  var v11,v12,v21,v22:integer;
  tv1,tv2:TTXVertex;
  u,v,u1,v1,a:Tvector;
  notflipped:boolean;
  begin
  result:=false;
  if FindCommonEdges(surf1,surf,v11,v12,v21,v22) then
  begin
  tv2:=surf.TXVertices[v21]; tv1:=surf1.TXVertices[v11];
  tv1.v:=tv2.v; tv1.u:=tv2.u;
  tv2:=surf.TXVertices[v22]; tv1:=surf1.TXVertices[v12];
  tv1.v:=tv2.v; tv1.u:=tv2.u;
  SysCalcUVNormalsFrom(surf,v21,v22,u1,v1,false);
  SysCalcUVNormalsFrom(surf1,v11,v12,u,v,IsVFlipped(surf,v21,v22,u1,v1));

  {Check if original normals are flipped}
  {  VMult(u1.dx,u1.dy,u1.dz,v1.dx,v1.dy,v1.dz,a.dx,a.dy,a.dz);
  With Surf do
  notflipped:=IsClose(a.dx,normal.dx) and IsClose(a.dy,normal.dy) and IsClose(a.dz,normal.dz);

  VMult(u.dx,u.dy,u.dz,v.dx,v.dy,v.dz,a.dx,a.dy,a.dz);
  With Surf1 do
  if notflipped<>(IsClose(a.dx,normal.dx) and IsClose(a.dy,normal.dy) and IsClose(a.dz,normal.dz))
  then SetVec(v,-v.dx,-v.dy,-v.dz);}

  surf1.txscale:=surf.txscale;
  ArrangeTexture(surf1,v11,u,v);
  Result:=true;
  end;
  end; *)

Function IsSurfPlanar(const surf: TJKSurface): boolean;
var
  v: integer;
  vx: TJKVertex;
  D: double;
begin
  Result := false;
  D := surf.CalcD;
  for v := 0 to surf.vertices.Count - 1 do
  begin
    vx := surf.vertices[v];
    With surf do
      if Abs(normal.Dot(vx.x, vx.y, vx.z) - D) > 0.001 then
      //if Abs(normal.dx * vx.x + normal.dy * vx.y + normal.dz * vx.z - D) > 0.001 then
        exit;
  end;
  result := true;
end;

Function IsSurfConvex(const surf: TJKSurface): boolean;
var
  i, n: integer;
  v1, v2, v3: TJKVertex;
  v: TVector;
begin
  result := false;
  for i := 0 to surf.vertices.Count - 1 do
    with surf do
    begin
      v1 := vertices[i];

      n  := NextVX(i);
      v2 := vertices[n];

      n  := NextVX(n);
      v3 := vertices[n];

      VectorCross3(v3.x - v2.x, v3.y - v2.y, v3.z - v2.z,
        v1.x - v2.x, v1.y - v2.y, v1.z - v2.z,
        v.dx, v.dy, v.dz); //VMult
      //if SMult(v.dx, v.dy, v.dz, normal.dx, normal.dy, normal.dz) < -0.0001 then
      if v.Dot(normal) < -0.0001 then
        exit;
    end;
  result := true;
end;

Function IsSectorConvex(const sec: TJKSector): boolean;
var
  i, j: integer;
  v0, v: TJKVertex;
  D: double;
begin
  Result := False;
  for i := 0 to sec.surfaces.Count - 1 do
    with sec.surfaces[i] do
    begin
      v0 := vertices[0];
      for j := 0 to sec.vertices.Count - 1 do
      begin
        v := sec.vertices[j];
        D := normal.Dot(v.x - v0.x, v.y - v0.y, v.z - v0.z); // VectorDot3(v.x - v0.x, v.y - v0.y, v.z - v0.z, normal.dx, normal.dy, normal.dz); //SMult
        If D < -0.001 then
          exit;
      end;
    end;
  Result := True;
end;

Procedure CalcRotVecData(var rd: TVecRotData; pch, yaw, rol: double);
begin
  With rd do
  begin
    cosPch := cos(-pch / 180 * Pi); { around X }
    sinPch := sin(-pch / 180 * Pi);
    cosYaw := cos(-yaw / 180 * Pi); { around Y }
    sinYaw := sin(-yaw / 180 * Pi);
    cosRol := cos(-rol / 180 * Pi); { around Y }
    sinRol := sin(-rol / 180 * Pi);
  end;
end;

Procedure RotVecbyData(var vec: TVector; var rd: TVecRotData);
var
  cosa, sina: double;
  dx, dy, dz: double;
begin
  { around X }
  With rd do
  begin
    dx := vec.dx;
    dy := cosPch * vec.dy + sinPch * vec.dz;
    dz := -sinPch * vec.dy + cosPch * vec.dz;

    { Around Z }
    vec.dx := cosYaw * dx + sinYaw * dy;
    vec.dy := -sinYaw * dx + cosYaw * dy;
    vec.dz := dz;

    { Around Y }
    dx := cosRol * vec.dx - sinRol * vec.dz;
    dy := vec.dy;
    dz := sinRol * vec.dx + cosRol * vec.dz;

    vec.dx := dx;
    vec.dy := dy;
    vec.dz := dz;
  end;
end;

Procedure GetJKPYR(const x, y, z: TVector; var pch, yaw, rol: double);
{ Assumes ROL,PCH, YAW
  PCH - x, YAW - z, ROL - y }
var
  l: double;
  ny, nz: TVector;
begin

  l := sqrt(sqr(y.dy) + sqr(y.dx));

  if l = 0 then
    yaw := 0
  else
  begin
    yaw := ArcCos(y.dy / l) / Pi * 180;
  end;

  if y.dx > 0 then
    yaw := 360 - yaw;

  ny := y;
  RotateVector(ny, 0, -yaw, 0);

  pch := ArcCos(ny.dy) / Pi * 180;
  if ny.dz < 0 then
    pch := 360 - pch;

  nz := z;
  RotateVector(nz, 0, -yaw, 0);
  RotateVector(nz, -pch, 0, 0);

  rol := ArcCos(nz.dz) / Pi * 180;
  if nz.dx < 0 then
    rol := 360 - rol;
end;

Procedure RotateVector(var vec: TVector; pch, yaw, rol: double);
var
  mx: TMat3x3;
begin
  CreateRotMatrix(mx, pch, yaw, rol);
  MultVM3(mx, vec.dx, vec.dy, vec.dz);
  (* cosa:=cos(-PCH/180*Pi); {around X}
    sina:=sin(-PCH/180*Pi);
    dx:=vec.dx;
    dy:=cosa*vec.dy+sina*vec.dz;
    dz:=-sina*vec.dy+cosa*vec.dz;


    cosa:=cos(-YAW/180*Pi); {Around Z}
    sina:=sin(-YAW/180*Pi);
    vec.dx:=cosa*dx+sina*dy;
    vec.dy:=-sina*dx+cosa*dy;
    vec.dz:=dz;

    cosa:=cos(-ROL/180*Pi);  {Around Y}
    sina:=sin(-ROL/180*Pi);
    dx:=cosa*vec.dx-sina*vec.dz;
    dy:=vec.dy;
    dz:=sina*vec.dx+cosa*vec.dz;

    vec.dx:=dx;
    vec.dy:=dy;
    vec.dz:=dz; *)
end;

Procedure CreateRotMatrix(var mx: TMat3x3; pch, yaw, rol: double);
var
  tmx: TMat3x3;
begin
  CreateMatrix(mx, -yaw, rt_z);

  CreateMatrix(tmx, -pch, rt_x);
  MultM3(mx, tmx);

  CreateMatrix(tmx, -rol, rt_y);
  MultM3(mx, tmx);

end;

Procedure CreateRotMatrixS(var mx: TMat3x3s; pch, yaw, rol: single);
var
  tmx: TMat3x3s;
begin
  CreateMatrixs(mx, -yaw, rt_z);

  CreateMatrixs(tmx, -pch, rt_x);

  MultM3s(mx, tmx);

  CreateMatrixs(tmx, -rol, rt_y);
  MultM3s(mx, tmx);
end;

Procedure ScaleMatrix(var mx: TMat3x3; scx, scy, scz: double);
var
  tmx: TMat3x3;
begin
  FillChar(tmx, sizeof(tmx), 0);
  tmx[0, 0] := scx;
  tmx[1, 1] := scy;
  tmx[2, 2] := scz;
  MultM3(mx, tmx);
end;

Procedure ScaleMatrixS(var mx: TMat3x3s; scx, scy, scz: single);
var
  tmx: TMat3x3s;
begin
  FillChar(tmx, sizeof(tmx), 0);
  tmx[0, 0] := scx;
  tmx[1, 1] := scy;
  tmx[2, 2] := scz;
  MultM3s(mx, tmx);
end;

Procedure DuplicateSector(sec, newsc: TJKSector; cx, cy, cz: double;
  nx, ny, nz: TVector; dx, dy, dz: double);

var
  i, j: integer;
  sf, sf1: TJKSurface;
  v, nv: TJKVertex;
  tv: TTXVertex;
begin
  newsc.Assign(sec);

  for j := 0 to sec.Vertices.count - 1 do
  begin
    v := sec.vertices[j];
    nv := newsc.NewVertex;
    nv.x := (v.x - cx) * nx.dx + (v.y - cy) * ny.dx + (v.z - cz) * nz.dx
      + dx + cx;
    nv.y := (v.x - cx) * nx.dy + (v.y - cy) * ny.dy + (v.z - cz) * nz.dy
      + dy + cy;
    nv.z := (v.x - cx) * nx.dz + (v.y - cy) * ny.dz + (v.z - cz) * nz.dz
      + dz + cz
  end;

  sec.Renumber;

  for i := 0 to sec.surfaces.Count - 1 do
  begin
    sf := sec.surfaces[i];
    sf1 := newsc.NewSurface;
    sf1.Assign(sf);

    if sf.adjoin <> nil then
      sf1.nadj := 1;
    newsc.surfaces.Add(sf1);

    for j := 0 to sf.vertices.Count - 1 do
    begin
      v := sf.vertices[j];
      sf1.AddVertex(newsc.vertices[v.num]);
      sf1.txvertices[j].Assign(sf.txvertices[j]);
    end;
  end;

  for i := 0 to newsc.surfaces.Count - 1 do
    newsc.surfaces[i].RecalcAll;
  newsc.Renumber;
end;

Function FlattenSurface(surf: TJKSurface): boolean;
var
  i: integer;
  sec: TJKSector;
  a, D: double;
  vx: TJKVertex;
  vec: TVector;
  x0, y0, z0: double;
  ax, ay, az: double;

  Procedure GetLineVec(vx: TJKVertex; var vec: TVector);
  var
    i: integer;
    sf: TJKSurface;
    sf1, sf2: TJKSurface;
    vec1: TVector;
  begin
    sf1 := nil;
    sf2 := nil;
    for i := 0 to sec.surfaces.Count - 1 do
    begin
      sf := sec.surfaces[i];
      if sf = surf then
        continue;
      if sf.Vertices.IndexOf(vx) <> -1 then
      begin
        if sf1 = nil then
        begin
          sf1 := sf;
          continue;
        end;
        sf2 := sf;
        break;
      end;
    end;

    if (sf1 = nil) and (sf2 = nil) then
      vec := surf.normal
    else if (sf1 <> nil) and (sf2 = nil) then
      begin
        vec1 := surf.normal.Cross(sf1.normal);
  //      VMult(surf.normal.dx, surf.normal.dy, surf.normal.dz, sf1.normal.dx,
  //        sf1.normal.dy, sf1.normal.dz, vec1.dx, vec1.dy, vec1.dz);

        vec := vec1.Cross(sf1.normal);
  //      VMult(vec1.dx, vec1.dy, vec1.dz, sf1.normal.dx, sf1.normal.dy,
  //        sf1.normal.dz, vec.dx, vec.dy, vec.dz);
        //if VLen(vec) < 0.01 then
        if vec.Length < 0.01 then
          vec := sf.normal;
      end
    else
      begin
        vec := sf1.normal.Cross(sf2.normal);
  //      VMult(sf1.normal.dx, sf1.normal.dy, sf1.normal.dz, sf2.normal.dx,
  //        sf2.normal.dy, sf2.normal.dz, vec.dx, vec.dy, vec.dz);
  //      if VLen(vec) < 0.01 then
        if vec.Length < 0.01 then
        begin
          vec1 := surf.normal.Cross(sf1.normal);
  //        VMult(surf.normal.dx, surf.normal.dy, surf.normal.dz, sf1.normal.dx,
  //          sf1.normal.dy, sf1.normal.dz, vec1.dx, vec1.dy, vec1.dz);

          vec := vec1.Cross(sf1.normal);
  //        VMult(vec1.dx, vec1.dy, vec1.dz, sf1.normal.dx, sf1.normal.dy,
  //          sf1.normal.dz, vec.dx, vec.dy, vec.dz);
  //        if VLen(vec) < 0.01 then
          if vec.Length < 0.01 then
            vec := sf.normal;
        end;
      end;
  end;

begin
  Result := False;
  sec := surf.sector;
  if not ToolBar.IsLayerVisible(sec.layer) then
    exit;

  Result := True;
  SaveSecUndo(sec, ch_changed, sc_geo);

  D := surf.CalcD;
  with surf.vertices[0] do
  begin
    x0 := x;
    y0 := y;
    z0 := z;
  end;

  for i := 0 to surf.vertices.Count - 1 do
    begin
      vx := surf.vertices[i];
      With surf, vx do
        a := Abs(D - normal.Dot(x, y, z));
        //a := Abs(D - (normal.dx * x + normal.dy * y + normal.dz * z));
      if a <= 0.001 then
        continue;

      GetLineVec(vx, vec);
      With vx do
        if PlaneLineXn(surf.normal, D, x, y, z, x + vec.dx, y + vec.dy,
          z + vec.dz, ax, ay, az) then
          begin
            if (sqr(vx.x - ax) + sqr(vx.y - ay) + sqr(vx.z - az)) < 1 then
            begin
              vx.x := ax;
              vx.y := ay;
              vx.z := az;
            end
            else
              result := false;
          end
        else
          begin
            PanMessageFmt(mt_info,
              'Couldn''t force vertex %d,%d to plane of surface %d,%d',
              [sec.Num, vx.Num, sec.Num, surf.Num]);
            result := false;
          end;
    end;
  surf.RecalcAll;
  JedMain.SectorChanged(sec);
end;

{ Lighting calculation }

Procedure ZeroVertexLights(lev: TJKLevel; scs: TSCMultiSel);
var
  s, sf, v: integer;
begin
  For s := 0 to scs.count - 1 do
    with lev.sectors[scs.getSC(s)] do
    begin
      for sf := 0 to surfaces.count - 1 do
        With surfaces[sf] do
        begin

          For v := 0 to txvertices.count - 1 do
            with txvertices[v] do
            begin
              // Note: do not change alpha;
              color.r := 0;
              color.g := 0;
              color.b := 0;
            end;
        end;
    end;
end;

Procedure CalcSectorAmbients(lev: TJKLevel; scs: TSCMultiSel);
var
  vlight, sflight: TColorF;
  v, nv, s, sf: integer;
  sec: TJKSector;
  surf: TJKSurface;
begin
  for s := 0 to scs.Count - 1 do
  begin
    sec := lev.sectors[scs.getSC(s)];
    if BitMaskTest(sec.flags, SECF_NOAMBINETLIGHT) then // skip calculating ambient light
      continue;

    vlight.SetToZero;
    sflight.SetToZero;
    nv := 0;

    for sf := 0 to sec.surfaces.Count - 1 do
    begin
      surf := sec.surfaces[sf];
      sflight := sflight + surf.extraLight;
      Inc(nv, surf.vertices.Count);

      for v := 0 to surf.vertices.Count - 1 do
        with surf.txvertices[v] do
        begin
          color.Normalize;
          vlight := vlight + color;
        end;
    end;

    vlight := vlight / nv;
    sflight := sflight / sec.surfaces.Count;
    if BitMaskTest(sec.flags, SECF_NORGBAMBINETLIGHT) then
    begin
      vlight  := MakeColor(vlight.Intensity);
      sflight := MakeColor(sflight.Intensity);
    end;

    if vlight.Intensity > sflight.Intensity then
      sec.ambient := vlight
    else
      sec.ambient := sflight;
  end;
end;

Procedure CalcLighting(lev: TJKLevel; scs: TSCMultiSel);
var
  l, sc, sf, v: integer;
  surf: TJKSurface;
  cv, v0: TJKVertex;
  lt: TSedLight;
  dist: double;
  sec: TJKSector;
  nv: integer;
  sfd: PChar;
  csd: PLCSurfData;
  gsfs: integer;

  function IsBlocked(x1, y1, z1, x2, y2, z2: double): boolean;
  var
    ls, lsf: integer;
    surf1: TJKSurface;
    dist: double;
    x, y, z: double;
    D: double;
    lbox: TBox;
    vx: TJKVertex;

    Function SurfBlocks(asurf: TJKSurface): boolean;
    begin
      result := false;
      if asurf = surf then
        exit;
      if (asurf.adjoin <> nil) then
        if not BitMaskTest(asurf.adjoinFlags, SAF_BlockLight) then
          exit;
      csd := Pointer(sfd + asurf.mark * sizeof(TLCSurfData));
      D := csd^.D;

      if newLight then
      begin
        if not DoBoxesIntersect(csd^.bbox, lbox) then
          exit;
      end;

      { vx:=aSurf.Vertices[0];
        with aSurf do
        if not PlaneLineXnNew(normal,vx.x,vx.y,vx.z,x1,y1,z1,x2,y2,z2,x,y,z) then exit; }
      with asurf do
        if not PlaneLineXn(normal, D, x1, y1, z1, x2, y2, z2, x, y, z) then
          exit;

      if not IsPointWithInLine(x, y, z, x1, y1, z1, x2, y2, z2) then
        exit;
      { Check if the intersection is close to the end of line
        if it is, check if the surface is facing the light.
        if it doesn't, block. Otherwise - don't }
      dist := VectorDistSqr3(x, y, z, x2, y2, z2);// sqr(x - x2) + sqr(y - y2) + sqr(z - z2);
      if dist < 0.0001 then
        exit;

      dist := VectorDistSqr3(x, y, z, x1, y1, z1); //sqr(x - x1) + sqr(y - y1) + sqr(z - z1);
      if dist < 0.0001 then
        exit;
      { begin
        With surf1,surf1.vertices[0]
        do dist:=SMult(normal.dx,normal.dy,normal.dz,x1-x,y1-y,z1-z);
        if dist<=0 then exit else continue;
        end; }

      { if newLight then
        if not IsPointInBox(csd^.bbox,x,y,z) then continue; }

      result := IsPointOnSurface(asurf, x, y, z);
    end;

  begin
    SetBox(lbox, x1, x2, y1, y2, z1, z2);
    result := true;

    if lt.mark <> -1 then
      With lev.sectors[lt.mark] do
        for lsf := 0 to surfaces.Count - 1 do
        begin
          if SurfBlocks(surfaces[lsf]) then
          begin
            if newLight then;
            exit;
          end;
        end;

    With lev.sectors[sc] do
      for lsf := 0 to surfaces.Count - 1 do
      begin
        if SurfBlocks(surfaces[lsf]) then
        begin
          if newLight then;
          exit;
        end;
      end;

    for ls := 0 to lev.sectors.Count - 1 do
      if (ls <> sc) and (ls <> lt.mark) then
        With lev.sectors[ls] do
          for lsf := 0 to surfaces.Count - 1 do
          begin
            if SurfBlocks(surfaces[lsf]) then
            begin
              if newLight then;
              exit;
            end;
          end;

    Result := False;
  end;

begin { CalcLighting }
  { Reset all lighting values to 0 }
  gsfs := 0;

  ZeroVertexLights(lev, scs);

  { Precalculate surface data }

  for sc := 0 to lev.sectors.Count - 1 do
    with lev.sectors[sc] do
    begin
      for sf := 0 to surfaces.Count - 1 do
        with surfaces[sf] do
          mark := gsfs + sf;
      Inc(gsfs, surfaces.count);
    end;

  GetMem(sfd, gsfs * sizeof(TLCSurfData));

  try
    for sc := 0 to lev.sectors.Count - 1 do
      with lev.sectors[sc] do
      begin
        for sf := 0 to surfaces.Count - 1 do
          with surfaces[sf] do
          begin
            csd := Pointer(sfd + mark * sizeof(TLCSurfData));
            csd^.D := CalcD;
            CalcSurfBBox(surfaces[sf], csd^.bbox);
          end;
      end;

    { Find sectors for lights }
    for l := 0 to lev.lights.Count - 1 do
      with lev.lights[l] do
      begin
        mark := FindSectorForXYZ(lev, position.x, position.y, position.z);
      end;

    { Calc lights }

    Progress.Reset(lev.sectors.Count);
    Progress.Msg := 'Calculating lights';

    for sc := 0 to scs.Count - 1 do
      with lev.sectors[scs.getSC(sc)] do
      begin

        for sf := 0 to surfaces.Count - 1 do
        begin
          surf := surfaces[sf];
          v0 := surf.vertices[0];

          for l := 0 to lev.lights.Count - 1 do
          begin
            lt := lev.lights[l];

            { Check if the light is in front of surface }
            if surf.normal.Dot(lt.position.Sub(v0.x, v0.y, v0.z)) < 0.001 then
              continue;
//            With surf do
//              if SMult(normal.dx, normal.dy, normal.dz, lt.position.x - v0.x,
//                lt.position.y - v0.y, lt.position.z - v0.z) < 0.001 then
//                continue;

            for v := 0 to surf.vertices.Count - 1 do
            begin
              { Check distance from the point to light }
              cv := surf.vertices[v];

              dist := lt.position.Distance(cv.x, cv.y, cv.z); //sqrt(sqr(lt.position.x - cv.x) + sqr(lt.position.y - cv.y) +sqr(lt.position.z - cv.z));
              if dist >= lt.range then
                continue;

              { if light and vertex aren't in the same sector }
              { Check if the light is blocked }
              if (sc <> lt.mark) and not BitMaskTest(lt.flags, LF_NoBlock) then
                if IsBlocked(lt.position.x, lt.position.y, lt.position.z, cv.x, cv.y, cv.z) then
                  continue;

              { Light vertex }
              with surf.txvertices[v] do
              begin
                  var intf := lt.Intensity * sqr((lt.range - dist) / lt.range);
                  if CurrentProject = JKDF2 then
                     color := MakeColor(color.Intensity + intf)
                  else
                    begin
                      color.r := color.r + lt.color.r * intf;
                      color.g := color.g + lt.color.g * intf;
                      color.b := color.b + lt.color.b * intf;
                    end;
              end;
              { Intensity:=Intensity+lt.Intensity*(lt.range-dist)/lt.range; }
            end;
          end;
        end;

        Progress.Step;
      end;

    CalcSectorAmbients(lev, scs);

  finally
    Progress.Hide;
    FreeMem(sfd);
  end;
end;

Function IsPointWithInLineInc(x, y, z, x1, y1, z1, x2, y2, z2: double): boolean;
{ inclusive }
begin
  result := (x - real_min(x1, x2) > -1E-4) and (real_max(x1, x2) - x > -1E-4)
    and (y - real_min(y1, y2) > -1E-4) and (real_max(y1, y2) - y > -1E-4) and
    (z - real_min(z1, z2) > -1E-4) and (real_max(z1, z2) - z > -1E-4);
end;

Function IsPointOnSurfaceNew(const surf: TJKSurface; const x, y, z: double): boolean;
var
  xv, yv: TVector;

  ppx, ppy, p1x, p1y, p2x, p2y, x0, y0, z0: double;

  i: integer;

  v1: TJKVertex;
  dist: double;
  vct: TVector;
  x1, x2, y1, y2, z1, z2: double;
begin
  result := false;
  if Abs(surf.normal.dx) < 0.99 then
    xv := surf.normal.Cross(TVector.right).Normalized
  else
    xv := surf.normal.Cross(TVector.forward).Normalized;

//  With surf.normal do
//    if Abs(dx) < 0.99 then
//      VMult(dx, dy, dz, 1, 0, 0, xv.dx, xv.dy, xv.dz)
//    else
//      VMult(dx, dy, dz, 0, 1, 0, xv.dx, xv.dy, xv.dz);
//  Normalize(xv);

  yv := surf.normal.Cross(xv);
//  With surf.normal do
//    VMult(dx, dy, dz, xv.dx, xv.dy, xv.dz, yv.dx, yv.dy, yv.dz);

  with surf do
  begin
    x0 := x;
    y0 := y;
    z0 := z;
  end;

  ppx := VectorDot3(x - x0, y - y0, z - z0, xv.dx, xv.dy, xv.dz); // SMult
  ppy := VectorDot3(x - x0, y - y0, z - z0, yv.dx, yv.dy, yv.dz);

  p1x := 0;
  p1y := 0;

  for i := 0 to surf.vertices.count - 1 do
  begin
    v1 := surf.vertices[surf.NextVX(i)];
    p2x := VectorDot3(v1.x - x0, v1.y - y0, v1.z - z0, xv.dx, xv.dy, xv.dz); // SMult
    p2y := VectorDot3(v1.x - x0, v1.y - y0, v1.z - z0, yv.dx, yv.dy, yv.dz);

    dist := (p1x - ppx) * (p2y - ppy) - (p2x - ppx) * (p1y - ppy);
    if (dist < -CloseEnough) then
      exit;

    p1x := p2x;
    p1y := p2y;
  end;

  result := true;
end;

{ Check if a line crosses the surface. It's meant to be used for adjoined
  surfaces, so all the marginal cases are treated as crossing }
Function IsSurfCrossed(const surf: TJKSurface;
  x1, y1, z1, x2, y2, z2: double): boolean;
var
  dist1, dist2: double;
  x0, y0, z0, pX, pY, pZ: double;
  k: double;
begin
  result := false;
  if surf.vertices.Count = 0 then
    exit;

  with surf.vertices[0] do
  begin
    x0 := x;
    y0 := y;
    z0 := z;
  end;

  dist1 := surf.normal.Dot(x1 - x0, y1 - y0, z1 - z0);
  dist2 := surf.normal.Dot(x2 - x0, y2 - y0, z2 - z0);
//  With surf.normal do
//  begin
//    dist1 := SMult(dx, dy, dz, x1 - x0, y1 - y0, z1 - z0);
//    dist2 := SMult(dx, dy, dz, x2 - x0, y2 - y0, z2 - z0);
//  end;

  if (dist1 > 0.0001) and (dist2 > 0.0001) then
    exit;
  if (dist1 < -0.0001) and (dist2 < -0.0001) then
    exit;

  if (Abs(dist1) <= 0.0001) and (Abs(dist2) <= 0.0001) then
  begin
    result := true;
    exit;
  end;

  { Find intersection of a line with plane }

  k := dist1 / (dist1 - dist2);
  pX := x1 + k * (x2 - x1);
  pY := y1 + k * (y2 - y1);
  pZ := z1 + k * (z2 - z1);

  { Find if intersection is on surface }

  result := IsPointOnSurface(surf, pX, pY, pZ);
end;

Procedure CalcLightingNew(lev: TJKLevel; scs: TSCMultiSel);
var
  i, l, sc, sf, v, nl: integer;
  sec: TJKSector;
  surf: TJKSurface;

  lvec: TVector;
  vx: TJKVertex;
  lt: TSedLight;

  lsec: TJKSector;
  D, dist, dist2, range2: double;

  { debug }
  dsc, dsf, dvx, dl: integer;

  Function CanReach(viasec: TJKSector): boolean;
  { if a ray from lt can reach vx by passing though this sector.
    Checks recursively }
  var
    i: integer;
    surf: TJKSurface;
    pX, pY, pZ: double;
  begin
    result := false;
    if viasec.mark <> 0 then
      exit;

    if viasec = vx.sector then
    begin
      result := true;
      exit;
    end;

    viasec.mark := 1;

    for i := 0 to viasec.surfaces.Count - 1 do
    begin
      surf := viasec.surfaces[i];
      if (surf.adjoin = nil) or (surf.vertices.Count = 0) or
          BitMaskTest(surf.adjoinFlags, SAF_BlockLight)
      then
        continue;

      // If a surface doesn't face the ray, skip
      if lvec.Dot(surf.normal) <= -0.0001 then //SMult(lvec.dx, lvec.dy, lvec.dz, dx, dy, dz)
          continue;

      { Check if ray intersects the surface }
      if IsSurfCrossed(surf, lt.position.x, lt.position.y, lt.position.z, vx.x, vx.y, vx.z) then
      begin
        result := CanReach(surf.adjoin.sector);
        if result then
          exit;
      end;

      { With surf.vertices[0] do
        If not PlaneLineXnNew(surf.normal,x,y,z,lt.x,lt.y,lt.z,vx.x,vx.y,vx.z,px,py,pz) then
        begin
        result:=CanReach(surf.adjoin.sector);
        if result then exit;
        end;

        if not IsPointWithinLineInc(px,py,pz,lt.x,lt.y,lt.z,vx.x,vx.y,vx.z) then continue;
        if IsPointOnSurface(surf,px,py,pz) then }
    end;
  end;

  Function IsBlocked: boolean;
  { is a ray from lt to vx blocked }
  var
    i: integer;
  begin
    for i := 0 to lev.sectors.Count - 1 do
      lev.sectors[i].mark := 0;
    Result := not CanReach(lsec);
  end;

begin
  { Find sectors for lights }
  nl := 0;
  for l := 0 to lev.lights.Count - 1 do
    with lev.lights[l] do
      begin
        mark := FindSectorForXYZ(lev, position.x, position.y, position.z);
        if mark = -1 then
          PanMessageFmt(mt_warning, 'Light %d is not in any sector', [l])
        else
          Inc(nl);
      end;

  if nl = 0 then
  begin
    PanMessage(mt_error, 'No valid lights to calculate!');
    exit;
  end;

  ZeroVertexLights(lev, scs);

  { debug }
  { dsc:=13;
    dsf:=2;
    dvx:=3;
    dl:=13; }

  Progress.Reset(lev.sectors.Count);
  Progress.Msg := 'Calculating lights';

  for sc := 0 to scs.Count - 1 do
  begin
    sec := lev.sectors[scs.getSC(sc)];
    for sf := 0 to sec.surfaces.Count - 1 do
    begin
      surf := sec.surfaces[sf];
      if surf.vertices.Count = 0 then
        continue; { Just in case }

      for l := 0 to lev.lights.Count - 1 do
      begin
        lt := level.lights[l];
        if lt.mark = -1 then
          continue;

        lsec   := level.sectors[lt.mark];
        range2 := sqr(lt.range);
        vx := surf.vertices[0];

        // if a surface doesn't face the light, skip it
        D := lt.position.Sub(vx.x, vx.y, vx.z).Dot(surf.normal);// With surf.normal do SMult(lt.position.x - vx.x, lt.position.y - vx.y, lt.position.z - vx.z, dx, dy, dz);
        if D <= 0 then
          continue;

        for v := 0 to surf.vertices.Count - 1 do
        begin

          { Debug }
          { if (s=dsc) and (sf=dsf) AND (v=dvx) and (dl=l) then
            begin
            panMessageFmt(mt_info,'Light %d',[l]);
            end; }

          vx   := surf.vertices[v];
          lvec := lt.position.Sub(vx.x, vx.y, vx.z);
          //lvec.SetCoords(lt.position.x - vx.x, lt.position.y - vx.y, lt.position.z - vx.z);

          dist2 := lvec.Dot(lvec); //sqr(lvec.dx) + sqr(lvec.dy) + sqr(lvec.dz);
          if dist2 >= range2 then
            continue; { Light is out of range }

          if not BitMaskTest(lt.flags, LF_NoBlock) then
            if IsBlocked then
              continue;

          { Normalize }
          dist := sqrt(dist2);
          { SetVec(lvec,lvec.dx/dist,lvec.dy/dist,lvec.dz/dist);
            With surf do
            dist2:=Smult(normal.dx,normal.dy,normal.dz,
            lvec.dx,lvec.dy,lvec.dz); }

          dist2 := sqr((lt.range - dist) / lt.range);

          with surf.txVertices[v] do
          begin
            { Intensity:=Intensity+dist2*lt.Intensity;
              r:=r+lt.Intensity*dist2*lt.r;
              g:=g+lt.Intensity*dist2*lt.g;
              b:=b+lt.Intensity*dist2*lt.b; }
            var intf := lt.intensity * sqr((lt.range - dist) / lt.range);
            if CurrentProject = JKDF2 then
               color := MakeColor(color.Intensity + intf)
            else
              begin
                color.r := color.r + lt.color.r * intf;
                color.g := color.g + lt.color.g * intf;
                color.b := color.b + lt.color.b * intf;
              end;
          end;
        end; { for vx }
      end; { for l }
    end; { for sf }

    Progress.Step;
  end; { for s }

  CalcSectorAmbients(lev, scs);
  Progress.Hide;
end;

Procedure OffsetByNode(const lev: TJKLevel; nnode: integer; var x, y, z: double);
var
  i, j: integer;
  hnode, hnode1: THNode;
  mx: TMat3x3;
begin

  hnode := lev.h3donodes[nnode];

  With hnode do
    CreateRotMatrix(mx, pch, yaw, rol);
  MultVM3(mx, x, y, z);
  x := x + hnode.x;
  y := y + hnode.y;
  z := z + hnode.z;

  hnode1 := hnode;
  while hnode1.parent <> -1 do
  begin
    hnode1 := lev.h3donodes[hnode1.parent];

    with hnode1 do
      CreateRotMatrix(mx, pch, yaw, rol);
    MultVM3(mx, x, y, z);
    x := x + hnode1.x;
    y := y + hnode1.y;
    z := z + hnode1.z;
  end;

end;

Procedure UnOffsetByNode(const lev: TJKLevel; nnode: integer; var x, y, z: double);
var
  i, j, k: integer;
  hnode, hnode1: THNode;
  mdx, mdy, mdz: double;
  branch: TIntList;
  rmx, mx: TMat3x3;
  vs: TVertices;

begin { UnoffsetMeshes }
  branch := TIntList.Create;

  hnode := lev.h3donodes[nnode];
  { Trace back all parent nodes }
  branch.Clear;
  branch.Add(nnode);

  hnode1 := hnode;
  while hnode1.parent <> -1 do
  begin
    branch.Add(hnode1.parent);
    hnode1 := lev.h3donodes[hnode1.parent];
  end;

  { Unrotate and unoffset in backwards order }

  for k := branch.count - 1 downto 0 do
  begin
    hnode1 := lev.h3donodes[branch[k]];
    with hnode1 do
      CreateRotMatrix(mx, pch, yaw, rol);
    rmx[0, 0] := mx[0, 0];
    rmx[0, 1] := mx[1, 0];
    rmx[0, 2] := mx[2, 0];
    rmx[1, 0] := mx[0, 1];
    rmx[1, 1] := mx[1, 1];
    rmx[1, 2] := mx[2, 1];
    rmx[2, 0] := mx[0, 2];
    rmx[2, 1] := mx[1, 2];
    rmx[2, 2] := mx[2, 2];

    x := x - hnode1.x;
    y := y - hnode1.y;
    z := z - hnode1.z;
    MultVM3(rmx, x, y, z);
  end;

  branch.free;
end;

Function CheckSectorIndex(const lev: TJKLevel; sc: integer): boolean;
begin
  Result := (sc >= 0) and (sc < lev.sectors.Count);
end;

Function CheckSectorSurfaceIndex(const sec: TJKSector; sf: integer): boolean;
begin
  Result := (sf >= 0) and (sf < sec.surfaces.Count);
end;

Function CheckSectorVertexIndex(const sec: TJKSector; vx: integer): boolean;
begin
  Result := (vx >= 0) and (vx < sec.vertices.Count);
end;

Function CheckSurfaceIndex(const lev: TJKLevel; sc, sf: integer): boolean;
begin
  Result := (sf >= 0) and CheckSectorIndex(lev, sc);
  if not Result then
    exit;
  Result := sf < lev.sectors[sc].surfaces.Count;
end;

Function CheckSurfaceEdgeIndex(const surf: TJKSurface; ed: integer): boolean;
begin
  Result := (ed >= 0) and (ed < surf.vertices.Count);
end;

Function CheckEdgeIndex(const lev: TJKLevel; sc, sf, ed: integer): boolean;
begin
  Result := (ed >= 0) and CheckSectorIndex(lev, sc);
  if not Result then
    exit;

  var sec := lev.sectors[sc];
  Result := CheckSectorSurfaceIndex(sec, sf);
  if not Result then
    exit;

  Result := ed < sec.surfaces[sf].vertices.Count;
end;

Function CheckVertexIndex(const lev: TJKLevel; sc, vx: integer): boolean;
begin
  Result := (vx >= 0) and CheckSectorIndex(lev, sc);
  if not Result then
    exit;
  Result := vx < lev.sectors[sc].vertices.Count;
end;

Function CheckTemplateIndex(const lev: TJKLevel; tmpl: integer): boolean;
begin
  Result := (tmpl >= 0) and (tmpl < lev.templates.Count);
end;

Function CheckThingIndex(const lev: TJKLevel; th: integer): boolean;
begin
  Result := (th >= 0) and (th < lev.things.Count);
end;

Function CheckFrameIndex(const lev: TJKLevel; th, fr: integer): boolean;
begin
  Result := (fr >= -1) and CheckThingIndex(lev, th);
  if not Result then
    exit;
  Result := fr < lev.things[th].vals.Count;
end;

Function CheckCogIndex(const lev: TJKLevel; co: integer): boolean;
begin
  Result := (co >= 0) and (co < lev.cogs.Count);
end;

Function CheckLightIndex(const lev: TJKLevel; lt: integer): boolean;
begin
  Result := (lt >= 0) and (lt < lev.lights.Count);
end;

procedure SnapPointToAxis(const x, y: Integer; ox, oy: Integer; var sx, sy: Integer);
begin
  var distX := Abs(y - oy); // Distance to x-axis
  var distY := Abs(x - ox); // Distance to y-axis
  if distX < distY then
    sy := oy // Snap to the y-coordinate of the origin point
  else // Snap to y-axis
    sx := ox // Snap to the x-coordinate of the origin point
end;

procedure SnapPointToAxis(const point: TPoint; origin: TPoint; var spoint: TPoint);
begin
  SnapPointToAxis(point.X, point.Y, origin.X, origin.Y, spoint.X, spoint.Y);
end;

procedure SnapPointToAxis(const x, y: Integer; origin: TPoint;  var sx, sy: Integer);
begin
  SnapPointToAxis(x, y, origin.X, origin.Y, sx, sy);
end;

end.
