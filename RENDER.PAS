unit Render;

{ This unit defines the abstract TRenderer
  class, an interface for rendering 3D objects }

interface

uses Windows, Forms, Geometry, misc_utils, GlobalVars;

Const
  Line_color = 1;
  vertex_color = 2;
  poly_color = 4;
  all_colors = 7;

Const
  xrange = 20000;
  yrange = 20000;
  zrange = 20000;
  ppunit = 16; { Pixels per unit }

  CL_FRONT = 0;
  CL_BACK = 0;
  CL_BACKGROUND = 1;
  { CL_GRID=2; }
  R_CULLNONE = 0;
  R_CULLBACK = -1;
  R_CULLFRONT = 1;

type

  TRColor = record
    r, g, b: byte;
  end;

  TPlane = record
    x, y, z: double;
    norm: Tvector;
  end;

  TRenderer = class
    hpal: integer;
    hViewer: HWND;
    CamX, CamY, CamZ: double;
    //Pch,Yaw,Rol:double;
    xv, yv, zv: Tvector; // view basis vectors
    scale: double;
    vpx, vpy, vpw, vph: integer; { viewport }
    FSelected: TIntList;
    Back_Clr, Front_clr, bgnd_clr: TRColor;

    { Grid variables }
    gnormal: Tvector;
    gxnormal, gynormal: Tvector;
    gridX, gridY, gridZ: double;
    gridLine, gridDot, gridStep: double;

  Private
     lplane, rplane, uplane, bplane: TPlane;

  Public
     Function DpiFactor: double;
     Function DpiScale: double;

     var perspective: boolean;

  Public
    Property Selected: TIntList read FSelected;
    Constructor Create(TheViewer: HWND);
    Destructor Destroy; Override;
    Procedure Initialize; virtual; abstract;
    Procedure SetViewPort(x, y, w, h: integer); virtual;
    Procedure SetColor(what, r, g, b: byte); virtual;
    Procedure SetPointSize(size: double); virtual; abstract;
    { Procedure SetRenderStyle(rstyle:TRenderStyle);virtual;abstract; }
    Procedure BeginScene; virtual; abstract;
    Procedure EndScene; virtual; abstract;
    Procedure SetCulling(how: integer); virtual; abstract;
    Procedure DrawPolygon(p: TPolygon); virtual; abstract;
    Procedure DrawPolygonsAt(ps: TPolygons; dx, dy, dz, pch, yaw, rol: double); virtual; abstract;
    Procedure DrawPolygons(ps: TPolygons); virtual; abstract;
    Procedure DrawLine(v1, v2: TVertex); overload; virtual; abstract;
    Procedure DrawLine(p1, p2: TVector); overload; virtual;  abstract;
    Procedure DrawLine(p, direction: TVector; length: double); overload; virtual;  abstract;
    Procedure DrawLineAt(x1, y1, z1, x2, y2, z2: double); overload; virtual; abstract;
    Procedure DrawLineAt(x, y, z: double; direction: TVector; length: double); overload; virtual; abstract;
    Procedure DrawVertex(x, y, z: double); virtual; abstract;
    Procedure DrawVertices(vxs: TVertices); virtual; abstract;
    Procedure DrawCircle(cx, cy, cz, rad: double); virtual; abstract;
    Procedure DrawGrid; virtual;
    Procedure Configure; virtual; abstract; { Setup dialog }

    Procedure BeginPick(x, y: integer); virtual; abstract;
    Procedure EndPick; virtual; abstract;
    Procedure PickPolygon(p: TPolygon; id: integer); virtual; abstract;
    Procedure PickPolygons(ps: TPolygons; id: integer); virtual; abstract;
    Procedure PickPolygonsAt(ps: TPolygons; x, y, z, pitch, yaw, roll: double; id: integer); virtual; abstract;
    Procedure PickLine(v1, v2: TVertex; id: integer); virtual; abstract;
    Procedure PickVertex(x, y, z: double; id: integer); virtual; abstract;

    Procedure BeginRectPick(x1, y1, x2, y2: integer);
    Procedure EndRectPick;
    Function IsPolygonInRect(p: TPolygon): boolean;
    Function ArePolygonsInRect(ps: TPolygons): boolean;
    Function IsLineInRect(v1, v2: TVertex): boolean;
    Function IsVertexInRect(x, y, z: double): boolean;

    Function S2MX(x: integer): double;
    Function S2MY(y: integer): double;
    Procedure SetGridNormal(dx, dy, dz: double);
    Procedure SetGridXNormal(dx, dy, dz: double);
    Procedure SetGridYNormal(dx, dy, dz: double);
    Procedure SetZ(dx, dy, dz: double);
    Procedure SetX(dx, dy, dz: double);
    Procedure SetY(dx, dy, dz: double);

    Function GetCameraAt(scX, scY: integer; var x, y, z: double): boolean; virtual; abstract; // Translate "D position to camera view position
    Function GetXYZonPlaneAt(scX, scY: integer; pnormal: Tvector; pX, pY, pZ: double; var x, y, z: double): boolean; virtual; abstract;
    Function GetGridAt(scX, scY: integer; var x, y, z: double): boolean; virtual; abstract;
    Procedure GetNearestGrid(iX, iY, iZ: double; var x, y, z: double);

    Function GetCameraForward: TVector; virtual; abstract; // returns camera forward basis vector
    Function GetCameraUp: TVector; virtual; abstract; // returns camera up basis vector
    Function GetCameraRight: TVector; virtual; abstract; // returns camera right basis vector

    Procedure ProjectPoint(x, y, z: double; Var WinX, WinY: integer); virtual; abstract;
    Procedure UnProjectPoint(WinX, WinY: double; WinZ: double; var x, y, z: double); virtual; abstract;
    Function HandleWMQueryPal: integer; virtual; abstract;
    Function HandleWMChangePal: integer; virtual; abstract;
    Function IsPolygonFacing(p: TPolygon): boolean; virtual; abstract;
  end;

implementation

uses Lev_utils, math;

Procedure TRenderer.SetViewPort(x, y, w, h: integer);
begin
  vpx := x;
  vpy := y;
  vpw := w;
  vph := h;
end;

Constructor TRenderer.Create(TheViewer: HWND);
var
  r: Trect;
begin
  FSelected := TIntList.Create;
  hViewer := TheViewer;

  GetClientRect(hViewer, r);
  vpx := r.left;
  vpy := r.right;
  vpw := r.right - r.left;
  vph := r.bottom - r.top;

  // With Viewer do
  // begin
  // vpx:=0; vpy:=0;
  // vpw:=ClientWidth; vph:=ClientHeight;
  // end;
  scale := 1;
  GridStep := 0.2;
  GridDot := 0.2;
  GridLine := 1;
  Initialize;
  SetColor(CL_FRONT, 255, 255, 255);
  SetColor(CL_BACK, 127, 127, 127);
end;

Procedure TRenderer.SetColor(what, r, g, b: byte);
begin
  case what of
    CL_FRONT:
      begin
        Front_clr.r := r;
        Front_clr.g := g;
        Front_clr.b := b;
      end;
    CL_BACKGROUND:
      begin
        bgnd_clr.r := r;
        bgnd_clr.g := g;
        bgnd_clr.b := b;
      end;
  end;
end;

Destructor TRenderer.Destroy;
begin
  FSelected.Free;
end;

Function TRenderer.S2MX(x: integer): double;
begin
  Result := (x - vpw / 2) / ppunit;
end;

Function TRenderer.S2MY(y: integer): double;
begin
  Result := (vph / 2 - y) / ppunit;
end;

{ Procedure TRenderer.RecalcGrid;
  begin
  GridD:=gnormal.dX*GridX+gnormal.dY*GridY+gnormal.dZ*GridZ;
  end; }

Procedure TRenderer.SetGridNormal(dx, dy, dz: double);
var
  g: Tvector;
begin
  g.dx := dx;
  g.dy := dy;
  g.dz := dz;
  var len := g.Normalize;//Normalize(g);

  if IsClose(len, 0) then //VLen
    exit;
  gnormal := g;
end;

Procedure TRenderer.SetGridXNormal(dx, dy, dz: double);
var
  g: Tvector;
begin
  g.dx := dx;
  g.dy := dy;
  g.dz := dz;
  var len := g.Normalize;//Normalize(g);
  if IsClose(len, 0) then // Vlen(g)
    exit;

  gxnormal := g;
  gynormal := gnormal.Cross(gxnormal);
//  VMult(gnormal.dx, gnormal.dy, gnormal.dz, gxnormal.dx, gxnormal.dy,
//    gxnormal.dz, gynormal.dx, gynormal.dy, gynormal.dz);
end;

Procedure TRenderer.SetGridYNormal(dx, dy, dz: double);
begin
  gxnormal.dx := dx;
  gxnormal.dy := dy;
  gxnormal.dz := dz;
  gxnormal.Normalize;
 // Normalize(gxnormal);

  gynormal := gnormal.Cross(gxnormal);
//  VMult(gnormal.dx, gnormal.dy, gnormal.dz, gxnormal.dx, gxnormal.dy,
//    gxnormal.dz, gynormal.dx, gynormal.dy, gynormal.dz);
end;

Procedure TRenderer.SetZ(dx, dy, dz: double);
var
  g: Tvector;
begin
  g.dx := dx;
  g.dy := dy;
  g.dz := (dz);  // TODO: If dz is negative the scene is mirroved. Abs. dz fixes this but the scene can't be then turned upside down.
  var len := g.Normalize;
  //Normalize(g);

  //if IsClose(Vlen(g), 0) then
  if IsClose(len, 0) then  
    exit;
  zv := g;
end;

Procedure TRenderer.SetX(dx, dy, dz: double);
begin
  xv.dx := dx;
  xv.dy := dy;
  xv.dz := dz;
  xv.Normalize;
  //Normalize(xv);

  yv := zv.Cross(xv);
  //VMult(zv.dx, zv.dy, zv.dz, xv.dx, xv.dy, xv.dz, yv.dx, yv.dy, yv.dz);

  xv := yv.Cross(zv);
  //VMult(yv.dx, yv.dy, yv.dz, zv.dx, zv.dy, zv.dz, xv.dx, xv.dy, xv.dz);
end;

Procedure TRenderer.SetY(dx, dy, dz: double);
begin
  yv.dx := dx;
  yv.dy := dy;
  yv.dz := dz;
  yv.Normalize;
  //Normalize(yv);

  xv := yv.Cross(zv);
  //VMult(yv.dx, yv.dy, yv.dz, zv.dx, zv.dy, zv.dz, xv.dx, xv.dy, xv.dz);

  yv := zv.Cross(xv);
  //VMult(zv.dx, zv.dy, zv.dz, xv.dx, xv.dy, xv.dz, yv.dx, yv.dy, yv.dz);
end;

Procedure TRenderer.DrawGrid;
var
  i, j, n: integer;
  x, y, z: double;
  v1, v2: TVertex;
  gsize2: double;

begin
  v1 := TVertex.Create;
  v2 := TVertex.Create;
  n := Round(GridSize / gridDot);

  For i := -(n div 2 - 1) to n div 2 - 1 do
    for j := -(n div 2 - 1) to n div 2 - 1 do
    begin
      x := gridX + (i * gridDot * gxnormal.dx) + (j * GridDot * gynormal.dx);
      y := gridY + (i * gridDot * gxnormal.dy) + (j * GridDot * gynormal.dy);
      z := gridZ + (i * gridDot * gxnormal.dz) + (j * GridDot * gynormal.dz);
      DrawVertex(x, y, z);
    end;

  n := Round(GridSize / GridLine);

  gsize2 := GridSize / 2;

  For i := -n div 2 to n div 2 do
  begin
    v1.x := gridX + (i * gridLine * gxnormal.dx) + (gsize2 * gynormal.dx);
    v1.y := gridY + (i * gridLine * gxnormal.dy) + (gsize2 * gynormal.dy);
    v1.z := gridZ + (i * gridLine * gxnormal.dz) + (gsize2 * gynormal.dz);

    v2.x := gridX + (i * gridLine * gxnormal.dx) + (-gsize2 * gynormal.dx);
    v2.y := gridY + (i * gridLine * gxnormal.dy) + (-gsize2 * gynormal.dy);
    v2.z := gridZ + (i * gridLine * gxnormal.dz) + (-gsize2 * gynormal.dz);
    DrawLine(v1, v2);
  end;

  For i := -n div 2 to n div 2 do
  begin
    v1.x := GridX + (gsize2 * gxnormal.dx) + (i * GridLine * gynormal.dx);
    v1.y := GridY + (gsize2 * gxnormal.dy) + (i * GridLine * gynormal.dy);
    v1.z := GridZ + (gsize2 * gxnormal.dz) + (i * GridLine * gynormal.dz);

    v2.x := GridX + (i * gridLine * gynormal.dx) + (-gsize2 * gxnormal.dx);
    v2.y := GridY + (i * gridLine * gynormal.dy) + (-gsize2 * gxnormal.dy);
    v2.z := GridZ + (i * gridLine * gynormal.dz) + (-gsize2 * gxnormal.dz);
    DrawLine(v1, v2);
  end;

  With clGridX do
    SetColor(CL_FRONT, r, g, b);

  v1.x := gridX;
  v1.y := gridY;
  v1.z := gridZ;
  v2.x := gridX + gsize2 * gxnormal.dx;
  v2.y := gridY + gsize2 * gxnormal.dy;
  v2.z := gridZ + gsize2 * gxnormal.dz;
  DrawLine(v1, v2);

  With clGridY do
    SetColor(CL_FRONT, r, g, b);
  v1.x := gridX;
  v1.y := gridY;
  v1.z := gridZ;
  v2.x := gridX + gsize2 * gynormal.dx;
  v2.y := gridY + gsize2 * gynormal.dy;
  v2.z := gridZ + gsize2 * gynormal.dz;
  DrawLine(v1, v2);

  v1.Free;
  v2.Free;
end;

Procedure TRenderer.GetNearestGrid(iX, iY, iZ: double; var x, y, z: double);
var
  revgs, ld: double;
  GridD: double;
  pX, pY: double;
begin
  revgs := 1 / GridStep;

  With gxnormal do
    pX := VectorDot3(dx, dy, dz, iX - gridX, iY - GridY, iZ - GridZ); // SMult

  With gynormal do
    pY := VectorDot3(dx, dy, dz, iX - gridX, iY - GridY, iZ - GridZ);

  pX := Round(pX * revgs) * GridStep;
  pY := Round(pY * revgs) * GridStep;

  x := gridX + (pX * gxnormal.dx) + (pY * gynormal.dx);
  y := gridY + (pX * gxnormal.dy) + (pY * gynormal.dy);
  z := gridZ + (pX * gxnormal.dz) + (pY * gynormal.dz);
end;

Procedure TRenderer.BeginRectPick(x1, y1, x2, y2: integer);
var
  i: integer;
  px1, py1, pz1, px2, py2, pz2, px3, py3, pz3: double;
begin
  if x1 > x2 then
  begin
    i := x2;
    x2 := x1;
    x1 := i;
  end;
  if y1 > y2 then
  begin
    i := y2;
    y2 := y1;
    y1 := i;
  end;

  { Left plane }
  UnProjectPoint(x1, y1, 0, px1, py1, pz1);
  UnProjectPoint(x1, y1, 1, px2, py2, pz2);
  UnProjectPoint(x1, y2, 0, px3, py3, pz3);

  With lplane do
  begin
    VectorCross3(px3 - px1, py3 - py1, pz3 - pz1, px2 - px1, py2 - py1, pz2 - pz1,
      norm.dx, norm.dy, norm.dz);// VMult
    //Normalize(norm);
    norm.Normalize;

    x := px1;
    y := py1;
    z := pz1;
  end;

  { Right plane }
  UnProjectPoint(x2, y1, 0, px1, py1, pz1);
  UnProjectPoint(x2, y1, 1, px2, py2, pz2);
  UnProjectPoint(x2, y2, 0, px3, py3, pz3);

  With rplane do
  begin
    VectorCross3(px2 - px1, py2 - py1, pz2 - pz1, px3 - px1, py3 - py1, pz3 - pz1,
      norm.dx, norm.dy, norm.dz);//VMult
    norm.Normalize;

    x := px1;
    y := py1;
    z := pz1;
    //Normalize(norm);
  end;

  { Top plane }
  UnProjectPoint(x1, y1, 0, px1, py1, pz1);
  UnProjectPoint(x1, y1, 1, px2, py2, pz2);
  UnProjectPoint(x2, y1, 0, px3, py3, pz3);

  With uplane do
  begin
    VectorCross3(px2 - px1, py2 - py1, pz2 - pz1, px3 - px1, py3 - py1, pz3 - pz1,
      norm.dx, norm.dy, norm.dz); //VMult
    norm.Normalize;

    x := px1;
    y := py1;
    z := pz1;
    //Normalize(norm);
  end;

  { Bottom plane }
  UnProjectPoint(x1, y2, 0, px1, py1, pz1);
  UnProjectPoint(x1, y2, 1, px2, py2, pz2);
  UnProjectPoint(x2, y2, 0, px3, py3, pz3);

  With bplane do
  begin
    VectorCross3(px3 - px1, py3 - py1, pz3 - pz1, px2 - px1, py2 - py1, pz2 - pz1,
      norm.dx, norm.dy, norm.dz); //VMult
    norm.Normalize;

    x := px1;
    y := py1;
    z := pz1;
    //Normalize(norm);
  end;

end;

Procedure TRenderer.EndRectPick;
begin
end;

Function TRenderer.IsPolygonInRect(p: TPolygon): boolean;
var
  i: integer;
begin
  Result := false;
  for i := 0 to p.vertices.count - 1 do
    With p.vertices[i] do
      if not IsVertexInRect(x, y, z) then
        exit;
  Result := true;
end;

Function TRenderer.ArePolygonsInRect(ps: TPolygons): boolean;
var
  i: integer;
begin
  Result := false;
  for i := 0 to ps.count - 1 do
    if not IsPolygonInRect(ps[i]) then
      exit;
  Result := true;
end;

Function TRenderer.IsLineInRect(v1, v2: TVertex): boolean;
begin
  Result := IsVertexInRect(v1.x, v1.y, v1.z) and
    IsVertexInRect(v2.x, v2.y, v2.z);
end;

Function IsInFront(x, y, z: double; const pl: TPlane): boolean;
begin
  Result := VectorDot3(x - pl.x, y - pl.y, z - pl.z, pl.norm.dx, pl.norm.dy, pl.norm.dz) > 0; // SMult
end;

Function TRenderer.IsVertexInRect(x, y, z: double): boolean;
begin
  Result := IsInFront(x, y, z, lplane) and IsInFront(x, y, z, rplane) and
    IsInFront(x, y, z, uplane) and IsInFront(x, y, z, bplane);
end;

Function TRenderer.DpiFactor: double;
begin
  Result := Screen.PixelsPerInch / USER_DEFAULT_SCREEN_DPI;
end;

Function TRenderer.DpiScale: double;
begin
  Result := scale / DpiFactor;
end;

end.
