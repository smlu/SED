unit OGL_render;

interface

{ This unit contains an OpenGL implementation
  of TRenderer class }

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  glunit, GlobalVars, Geometry, Render, StdCtrls, misc_utils, Math;

type
  TOGL_config = class(TForm)
    GroupBox1: TGroupBox;
    RBPerspective: TRadioButton;
    RBParallel: TRadioButton;
  private
    { Private declarations }
  public
    { Public declarations }
  end;

var
  OGL_config: TOGL_config;

Type
  TOGLRenderer = class(TRenderer)
    hdc :HDC;
    hglc: HGLRC;
    glx1, glx2, glz1, glz2: double;

    Perspective: boolean;
    { mode:TRenderStyle; }
    selectBuf: array [1 .. 2048] of GLInt;
    { R_style:TRenderStyle; }

    //ccenterx, ccentery, crad: Integer;
    //ccolor: TCOLORREF;

    Procedure Initialize; override;
    Procedure BeginScene; override;
    Procedure SetViewPort(x, y, w, h: Integer); override;
    { Procedure SetRenderStyle(rstyle:TRenderStyle);override; }
    Procedure EndScene; override;

    Procedure SetColor(what, r, g, b: byte); override;
    Procedure SetCulling(how: Integer); override;

    Procedure DrawPolygon(p: TPolygon); override;
    Procedure DrawPolygons(ps: TPolygons); override;
    Procedure DrawPolygonsAt(ps: TPolygons;
      dx, dy, dz, pch, yaw, rol: double); override;
    Procedure DrawLine(v1, v2: TVertex); override;
    Procedure DrawVertex(x, y, Z: double); override;
    Procedure DrawCircle(cx, cy, cz, rad: double); override;
    Procedure DrawVertices(vxs: TVertices); override;
    Procedure Configure; override; { Setup dialog }
    Destructor Destroy; override;

    Procedure BeginPick(x, y: Integer); override;
    Procedure EndPick; override;
    Procedure PickPolygon(p: TPolygon; id: Integer); override;
    Procedure PickPolygons(ps: TPolygons; id: Integer); override;
    Procedure PickPolygonsAt(ps: TPolygons; x, y, z, pitch, yaw, roll: double; id: integer); override;
    Procedure PickLine(v1, v2: TVertex; id: Integer); override;
    Procedure PickVertex(x, y, Z: double; id: Integer); override;
    Function GetXYZonPlaneAt(scX, scY: Integer; pnormal: TVector; pX, pY, pZ: double; var x, y, Z: double): boolean; override;
    Function GetGridAt(scX, scY: Integer; var x, y, Z: double): boolean; override;
    Procedure SetPointSize(size: double); override;
    Procedure ProjectPoint(x, y, Z: double; Var WinX, WinY: Integer); override;
    Procedure UnProjectPoint(WinX, WinY: double; WinZ: double;
      var x, y, Z: double); override;
    Function HandleWMQueryPal: Integer; override;
    Function HandleWMChangePal: Integer; override;
  Private
    Procedure SetCamera;
    Procedure SetRenderMatrix;
    Procedure SetPickMatrix(x, y: Integer);
    Procedure RenderPoly(p: TPolygon);
    Procedure RenderLine(v1, v2: TVertex);
    Function CreateOGLPalette(const pd: TPIXELFORMATDESCRIPTOR): Integer;
    Function CanRenderWirePolygon(p: TPolygon): boolean;
  end;

implementation

uses Jed_Main, Lev_utils;

{$R *.DFM}

procedure DisableFPUExceptions;
var
  FPUControlWord: WORD;
  asm
    FSTCW   FPUControlWord ;
    OR      FPUControlWord, $4 + $1 ; { Divide by zero + invalid operation }
    FLDCW   FPUControlWord ;
end;

Procedure TOGLRenderer.BeginScene;
begin
  wglMakeCurrent(hdc, hglc);
  DisableFPUExceptions;
  SetRenderMatrix;
  //crad := 0;
  With bgnd_clr do
    glClearColor(r / 255, g / 255, b / 255, 0);
  glClear(GL_COLOR_BUFFER_BIT or GL_DEPTH_BUFFER_BIT);
end;

{ Procedure TOGLRenderer.SetRenderStyle;
  begin
  Case rstyle of
  DRAW_WIREFRAME: begin
  glPolygonMode(GL_Front_AND_BACK,gl_Line);
  end;
  DRAW_FLAT_POLYS: GlPolygonMode(GL_FRONT_AND_BACK,gl_fill);
  DRAW_TEXTURED: GlPolygonMode(GL_FRONT_AND_BACK,gl_fill);
  DRAW_VERTICES: GlPolygonMode(GL_Front_AND_BACK,gl_Points);
  end;
  R_Style:=rstyle;
  end; }

Procedure TOGLRenderer.EndScene;
begin
  wglMakeCurrent(hdc, hglc);
  glFlush;
  SwapBuffers(hdc);
  //wglMakeCurrent(0, 0);
//  if crad <> 0 then // Draw circle
//  begin
//    SelectObject(hdc, GetStockObject(NULL_BRUSH));
//    var pen := CreatePen(PS_SOLID, 50, ccolor);
//    var oldpen := SelectObject(hdc, pen);
//     Ellipse(hdc, ccenterx - crad, ccentery - crad, ccenterx + crad,
//      ccentery + crad);
//    SelectObject(hdc, oldpen);
//    DeleteObject(pen);
//  end;
end;

Procedure TOGLRenderer.SetCamera;
begin
  { glRotated(-90,1,0,0); }

  { glRotated(45,1,0,0); }
  { glRotated(0,0,0,1);
    glRotated(0,0,1,0); }

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity;


  //glScaleD(1.0, IfThen(zv.dz <0, -1,1) , 1.0);
  //glTranslated(CamX, CamY, CamZ);
  gluLookAt(0, 0, 0, -zv.dx, -zv.dy, -zv.dz, yv.dx, yv.dy, yv.dz);
  glTranslated(CamX, CamY, CamZ);

//  var rPch, rYaw, rRol: double;
//  Jed_Main.sysGetPYR(xv, yv, zv, rPch, rYaw, rRol);
//  glRotateD(rPch, xv.x, xv.y, xv.z);
//  glRotateD(rYaw, yv.x, yv.y, yv.z);
  //glRotateD(rRol, zv.x, zv.y, zv.z);



   { mx[0,0]:=xv.dx; mx[1,0]:=xv.dy; mx[0,2]:=xv.dz;
    mx[0,1]:=yv.dx; mx[1,1]:=yv.dy; mx[1,2]:=yv.dz;
    mx[0,2]:=zv.dx; mx[1,2]:=zv.dy; mx[2,2]:=zv.dz;

    glMultMatrixd(@mx); }
  { glScaled(1,1,-1); }
  { glTranslated(CamX,CamY,CamZ); }
  {
    xscale:=xrange/((vpw/ppunit)*scale);
    yscale:=yrange/((vph/ppunit)*scale);
    zscale:=xscale;
    glScaled(xscale,yscale,zscale); }
end;

Procedure TOGLRenderer.SetRenderMatrix;
var
  dpx, dpy: double;
begin
  wglMakeCurrent(hdc, hglc);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity;

  dpx := vpw / ppunit * DpiScale;
  dpy := vph / vpw * dpx;

  GlOrtho(-dpx / 2, dpx / 2, -dpy / 2, dpy / 2, -ZRange / 2, ZRange / 2);

   //gluPerspective(90,1,0.01,zrange);

   //glFrustum(-dpx/2,dpx/2,-dpy/2,dpy/2,0.01,Zrange/2);

  SetCamera;
end;

Procedure TOGLRenderer.SetPickMatrix(x, y: Integer);
var
  dpx, dpy: double;
  a: TViewPortArray;
begin
  wglMakeCurrent(hdc, hglc);
  glGetIntegerV(GL_VIEWPORT, @a);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPickMatrix(x, a[3] - y, 4, 4, a);

  dpx := vpw / ppunit * DpiScale;
  dpy := vph / vpw * dpx;
  { glFrustum(glx1,glx2,gly1,gly2,-ZRange/2,Zrange/2); }
  GlOrtho(-dpx / 2, dpx / 2, -dpy / 2, dpy / 2, -ZRange / 2, ZRange / 2);

  SetCamera;

//   glMatrixMode(GL_ModelView);
//    glLoadIdentity;
end;

Function TOGLRenderer.CreateOGLPalette
  (const pd: TPIXELFORMATDESCRIPTOR): Integer;
var
  ncolors: Integer;
  lp: PLogPalette;
  i: Integer;
  rrange, grange, brange: byte;
begin
  Result := 0;
  if pd.dwFlags and PFD_NEED_PALETTE = 0 then
    exit;
  ncolors := 1 shl pd.cColorBits;
  GetMem(lp, sizeof(TLOGPALETTE) + ncolors * sizeof(TPALETTEENTRY));
  lp.palVersion := $300;
  lp.palNumEntries := ncolors;

  rrange := (1 shl pd.cRedBits) - 1;
  grange := (1 shl pd.cGreenBits) - 1;
  brange := (1 shl pd.cBlueBits) - 1;

  for i := 0 to ncolors - 1 do
{$R-}
    With lp.palPalEntry[i] do
{$R+}
    begin
      // Fill in the 8-bit equivalents for each component
      peRed := (i shr pd.cRedShift) and rrange;
      peRed := Round(peRed * 255.0 / rrange);
      peGreen := (i shr pd.cGreenShift) and grange;
      peGreen := Round(peGreen * 255.0 / grange);

      peBlue := (i shr pd.cBlueShift) and brange;
      peBlue := Round(peBlue * 255.0 / brange);

      peFlags := 0;
    end;
  Result := CreatePalette(lp^);
  SelectPalette(hdc, Result, FALSE);
  RealizePalette(hdc);
  FreeMem(lp);
end;

Procedure TOGLRenderer.Initialize;
var
  pfd: TPIXELFORMATDESCRIPTOR;
  pixelFormat: Integer;
begin
  if not InitOpenGL then
    Raise Exception.Create('Couldn''t initialize OpenGL');
  hdc := GetDC(hViewer);
  pfd.nsize := 40;
  pfd.nVersion := 1;
  pfd.dwFlags := PFD_DRAW_TO_WINDOW + PFD_SUPPORT_OPENGL + PFD_DOUBLEBUFFER;
  pfd.iPixelType := PFD_TYPE_RGBA;
  pfd.cColorBits := 8;
  pfd.cRedBits := 0;
  pfd.cRedShift := 0;
  pfd.cGreenBits := 0;
  pfd.cGreenShift := 0;
  pfd.cBlueBits := 0;
  pfd.cBlueShift := 0;
  pfd.cAlphaBits := 0;
  pfd.cAlphaShift := 0;
  pfd.cAccumBits := 0;
  pfd.cAccumRedBits := 0;
  pfd.cAccumGreenBits := 0;
  pfd.cAccumBlueBits := 0;
  pfd.cAccumAlphaBits := 0;
  pfd.cDepthBits := 32;
  pfd.cStencilBits := 0;
  pfd.cAuxBuffers := 0;
  pfd.iLayerType := PFD_MAIN_PLANE;
  pfd.iLayerType := 0;
  pfd.bReserved := 0;
  pfd.dwLayerMask := 0;
  pfd.dwVisiblemask := 0;
  pfd.dwDamageMask := 0;
  pixelFormat := ChoosePixelFormat(hdc, @pfd);
  SetPixelFormat(hdc, pixelFormat, @pfd);
  DescribePixelFormat(hdc, pixelFormat, sizeof(pfd), pfd);
  Hpal := CreateOGLPalette(pfd);
  hglc := wglCreateContext(hdc);
  wglMakeCurrent(hdc, hglc);
  SwapBuffers(hdc);

  // Uncomment this block to have anti-aliased lines
  //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	//glEnable(GL_BLEND);
	//glEnable(GL_LINE_SMOOTH);
	//glHint (GL_LINE_SMOOTH_HINT, GL_NICEST);
  //glLineWidth(1.0); //1.3 for diagonal lines
  ////////////////////////////////////////////


  { glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK); }
  //glPolygonMode(GL_Front_AND_BACK, gl_Line);
  //wglMakeCurrent(0,0);

  { glEnable(GL_DEPTH_TEST);
    glDepthRange(0,1000);
    glCullFace(GL_BACK);
    glEnable(GL_CULL_FACE);
    glFrontFace(GL_CCW);
    glClearColor(1,1,1,1); }
end;

Procedure TOGLRenderer.SetViewPort(x, y, w, h: Integer);
begin
  Inherited SetViewPort(x, y, w, h);
  glViewPort(x, y, w, h);
end;

Procedure TOGLRenderer.DrawPolygon(p: TPolygon);
begin
  { glCullFace(GL_BACK);
    With back_clr do glColor3ub(r,g,b); }
  RenderPoly(p);
  { With front_clr do glColor3ub(r,g,b);
    glCullFace(GL_FRONT);
    RenderPoly(p); }
end;

Function TOGLRenderer.CanRenderWirePolygon(p: TPolygon): Boolean;
  var m: integer;
begin
  Result := BitMaskTest(p.FaceFlags, FF_DoubleSided);
  if not Result and glIsEnabled(GL_CULL_FACE) then
  begin
    // Note this method works ok only for convex polygons
    glGetIntegerv(GL_CULL_FACE_MODE, @m);
    var dot := SMult(gnormal.x, gnormal.y, gnormal.z, p.normal.x, p.normal.y, p.normal.z);
    if (m = GL_FRONT) and (dot > 0) then
      exit
    else if (m = GL_BACK) and (dot <= 0) then
      exit;
  end;
  Result := true;
end;

Procedure TOGLRenderer.DrawPolygonsAt(ps: TPolygons;
  dx, dy, dz, pch, yaw, rol: double);
var
  i, j, n: Integer;
  mx: TMat3x3;
  ax, ay, az: double;
begin

  { With front_clr do glColor3ub(r,g,b);
    glDisable(GL_CULL_FACE); }
  CreateRotMatrix(mx, pch, yaw, rol);
  try
    for i := 0 to ps.count - 1 do
    begin
      if not CanRenderWirePolygon(ps[i]) then continue;
      with ps[i] do
      begin
        n := vertices.count;
//        case n of
//          3:
//            glBegin(GL_triangles);
//          4:
//            glBegin(GL_Quads);
//        else
//          glBegin(GL_Polygon);
//        end;
        glBegin(GL_LINE_LOOP);
        for j := 0 to vertices.count - 1 do
          with vertices[j] do
          begin
            ax := x;
            ay := y;
            az := z;
            MultVM3(mx, ax, ay, az);
            glVertex3d(ax + dx, ay + dy, az + dz);
          end;
        glEnd;
      end;
    end;
  finally
    { glEnable(GL_CULL_FACE); }
  end;
end;

Procedure TOGLRenderer.DrawPolygons(ps: TPolygons);
var
  i: Integer;
begin
  { With back_clr do glColor3ub(r,g,b);
    glCullFace(GL_BACK); }

  for i := 0 to ps.count - 1 do
    RenderPoly(ps[i]);

  { glCullFace(GL_FRONT);
    With front_clr do glColor3ub(r,g,b);

    for i:=0 to ps.count-1
    do RenderPoly(ps[i]); }

end;

Procedure TOGLRenderer.RenderPoly(p: TPolygon);
var
  i, n, m: Integer;
begin
  if not CanRenderWirePolygon(p) then
    exit;

  n := p.vertices.count;
//  case n of
//    3:
//      glBegin(GL_triangles);
//    4:
//      glBegin(GL_Quads);
//  else
//    glBegin(GL_Polygon);
//  end;
  glBegin(GL_LINE_LOOP);
    for i := 0 to p.vertices.count - 1 do
      with p.vertices[i] do
      begin
        glVertex3d(x, y, Z);
        // glNormal3d(p.Normal.dx,p.normal.dz, p.normal.dy);
      end;
  glEnd;
end;

Procedure TOGLRenderer.DrawVertex(x, y, Z: double);
begin
  glBegin(gl_points);
  { With front_clr do glColor3ub(r,g,b); }
  glVertex3d(x, y, Z);
  glEnd;
end;

Procedure TOGLRenderer.DrawCircle(cx, cy, cz, rad: double);
const maxSides = 64;
begin
//  ProjectPoint(cx, cy, cz, pX, pY);
//  ProjectPoint(cx + xv.dx * rad, cy + xv.dy * rad, cz + xv.dz * rad, px1, py1);
//  ccenterx := pX;
//  ccentery := pY;
//  crad := px1 - pX;
//  ccolor := RGB(Front_clr.r, Front_clr.g, Front_clr.b);

  const th = 6.2832 / maxSides;  // 6.2832 = 2*PI
  var poly := TPolygon.Create;
  for var s := 0 to maxSides - 1 do
    begin
      const ra = s * th;
      const cr = cos(ra) * rad;
      const sr = sin(ra) * rad;

      // c + right * cos(th) * rad + forward * sin(th) * rad
      const v = TVertex.Create;
      v.x := cx + xv.x * cr + yv.x * sr;
      v.y := cy + xv.y * cr + yv.y * sr;
      v.z := cz + xv.z * cr + yv.z * sr;
      poly.Vertices.Add(v);
    end;
  DrawPolygon(poly);

  for var i := 0 to poly.Vertices.count - 1 do
      poly.Vertices[i].Free;
  poly.Free;
end;

Procedure TOGLRenderer.DrawVertices(vxs: TVertices);
var
  i: Integer;
begin
  glBegin(GL_POINTS);
  { With front_clr do glColor3ub(r,g,b); }
  for i := 0 to vxs.count - 1 do
    With vxs[i] do
      glVertex3d(x, y, Z);
  glEnd;
end;

Procedure TOGLRenderer.Configure; { Setup dialog }
begin
  with OGL_config do
  begin
    RBPerspective.checked := Perspective;
  end;

  if OGL_config.ShowModal = idOk then
    with OGL_config do
    begin
      Perspective := RBPerspective.checked;
    end;

end;

Procedure TOGLRenderer.RenderLine(v1, v2: TVertex);
begin
  glBegin(GL_LINES);
  With v1 do
    glVertex3d(x, y, Z);
  With v2 do
    glVertex3d(x, y, Z);
  glEnd;
end;

Procedure TOGLRenderer.DrawLine(v1, v2: TVertex);
begin
  { With front_clr do glColor3ub(r,g,b); }
  RenderLine(v1, v2);
end;

Destructor TOGLRenderer.Destroy;
begin
  if (hdc <> 0) and (hglc <> 0) then
  begin
    wglMakeCurrent(hdc, hglc);
    wglDeleteContext(hglc);
    DeleteDC(hdc);
  end;
  if Hpal <> 0 then
    DeleteObject(Hpal);
  wglMakeCurrent(0, 0);
  glFlush;
  inherited Destroy;
end;

Procedure TOGLRenderer.BeginPick(x, y: Integer);
begin
  glSelectBuffer(sizeof(selectBuf), @selectBuf);
  glRenderMode(GL_SELECT);

  { glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT); }

  glDisable(GL_CULL_FACE);

  glInitNames();
  glPushName(0);
  SetPickMatrix(x, y);
end;

Procedure TOGLRenderer.EndPick;
var
  i, j, hits, names: Integer;
  pi: ^Integer;
  Z: Integer;
  zs: TIntList;

  Procedure AddID(id, Z: Integer);
  var
    i, n: Integer;
  begin
    n := Selected.count;
    for i := 0 to n - 1 do
    begin
      if Z < zs[i] then
      begin
        n := i;
        break;
      end;
    end;
    Selected.Insert(n, id);
    zs.Insert(n, Z);
  end;

begin
  glFlush;
  hits := glRenderMode(GL_SELECT);
  Selected.Clear;
  zs := TIntList.Create;
  pi := @selectBuf;
  for i := 0 to hits - 1 do
  begin
    names := pi^;
    inc(pi);
    Z := pi^;
    inc(pi);
    inc(pi);
    for j := 0 to names - 1 do
    begin
      AddID(pi^, Z);
      inc(pi);
    end;
  end;
  zs.free;
  glRenderMode(GL_RENDER);
  glDisable(GL_CULL_FACE);
end;

Procedure TOGLRenderer.PickPolygon(p: TPolygon; id: Integer);
begin
  glLoadName(id);
  RenderPoly(p);
end;

Procedure TOGLRenderer.PickPolygons(ps: TPolygons; id: Integer);
var
  i: Integer;
begin
  glLoadName(id);
  DrawPolygons(ps);
end;

Procedure TOGLRenderer.PickPolygonsAt(ps: TPolygons; x, y, z, pitch, yaw, roll: double; id: integer);
begin
  glLoadName(id);
  DrawPolygonsAt(ps, x, y, z, pitch, yaw, roll);
end;

Procedure TOGLRenderer.PickLine(v1, v2: TVertex; id: Integer);
begin
  glLoadName(id);
  RenderLine(v1, v2);
end;

Procedure TOGLRenderer.PickVertex(x, y, Z: double; id: Integer);
begin
  glLoadName(id);
  DrawVertex(x, y, Z);
end;

Function TOGLRenderer.GetXYZonPlaneAt(scX, scY: Integer; pnormal: TVector;
  pX, pY, pZ: double; var x, y, Z: double): boolean;
var
  vp: TViewPortArray;
  pmx, mmx: TMatrixDblArray;
  ax1, ay1, az1, ax2, ay2, az2: double;
  d: double;
begin
  SetRenderMatrix;
  glGetIntegerV(GL_VIEWPORT, @vp);
  glGetDoublev(GL_MODELVIEW_MATRIX, @mmx);
  glGetDoublev(GL_PROJECTION_MATRIX, @pmx);

  gluUnProject(scX, vp[3] - scY, 0, mmx, pmx, vp, ax1, ay1, az1);
  gluUnProject(scX, vp[3] - scY, 1, mmx, pmx, vp, ax2, ay2, az2);

  { ay1:=-ay1;
    ay2:=-ay2; }

  With pnormal do
    d := dx * pX + dy * pY + dz * pZ;

  Result := PlaneLineXn(pnormal, d, ax1, ay1, az1, ax2, ay2, az2, x, y, Z);
  Result := Result and (abs(x) < xrange) and (abs(y) < Yrange) and
    (abs(Z) < ZRange);
  if not Result then
    exit;
  x := JKRound(x);
  y := JKRound(y);
  Z := JKRound(Z);
end;

Function TOGLRenderer.GetGridAt(scX, scY: Integer; var x, y, Z: double)
  : boolean;
begin
  Result := GetXYZonPlaneAt(scX, scY, gnormal, GridX, GridY, GridZ, x, y, Z);
  if Result then
  begin
    x := JKRound(x);
    y := JKRound(y);
    Z := JKRound(Z);
  end;
end;

Procedure TOGLRenderer.UnProjectPoint(WinX, WinY: double; WinZ: double;
  var x, y, Z: double);
var
  vp: TViewPortArray;
  pmx, mmx: TMatrixDblArray;
begin
  SetRenderMatrix;
  glGetIntegerV(GL_VIEWPORT, @vp);
  glGetDoublev(GL_MODELVIEW_MATRIX, @mmx);
  glGetDoublev(GL_PROJECTION_MATRIX, @pmx);
  gluUnProject(WinX, vp[3] - WinY, WinZ, mmx, pmx, vp, x, y, Z);
  { y:=-y; }
end;

Procedure TOGLRenderer.ProjectPoint(x, y, Z: double; Var WinX, WinY: Integer);
var
  vp: TViewPortArray;
  pmx, mmx: TMatrixDblArray;
  WX, WY, WZ: double;
begin
  SetRenderMatrix;
  glGetIntegerV(GL_VIEWPORT, @vp);
  glGetDoublev(GL_MODELVIEW_MATRIX, @mmx);
  glGetDoublev(GL_PROJECTION_MATRIX, @pmx);
  gluProject(x, y, Z, mmx, pmx, vp, WX, WY, WZ);
  WinX := Round(WX);
  WinY := vp[3] - Round(WY);
end;

Procedure TOGLRenderer.SetPointSize(size: double);
begin
  glPointSize(size);
end;

Function TOGLRenderer.HandleWMQueryPal: Integer;
begin
  Result := 0;
  if Hpal = 0 then
    exit;
  SelectPalette(hdc, Hpal, FALSE);
  Result := RealizePalette(hdc);

  InvalidateRect(hViewer, nil, FALSE);
  // Viewer.Invalidate;
end;

Function TOGLRenderer.HandleWMChangePal: Integer;
begin
  Result := 0;
  if Hpal = 0 then
    exit;
  SelectPalette(hdc, Hpal, FALSE);
  RealizePalette(hdc);
  UpdateColors(hdc);
end;

Procedure TOGLRenderer.SetColor(what, r, g, b: byte);
begin
  Inherited SetColor(what, r, g, b);
  case what of
    CL_BackGround:
      begin
        glColor3ub(r, g, b);
        bgnd_clr.r := r;
        bgnd_clr.g := g;
        bgnd_clr.b := b;
      end;
  else
    begin
      glColor3ub(r, g, b);
      Front_clr.r := r;
      Front_clr.g := g;
      Front_clr.b := b;
    end;
  end;
end;

Procedure TOGLRenderer.SetCulling(how: Integer);
begin
  case how of
    r_CULLNONE:
      glDisable(GL_CULL_FACE);
    r_CULLBACK:
      begin
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
      end;
    r_CULLFRONT:
      begin
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
      end;
  end;
end;

end.
