unit RenderGL;

interface

{ This unit contains an OpenGL implementation
  of TRenderer class }

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  GLContext, dglOpenGL, GlobalVars, Geometry, Render, StdCtrls, misc_utils, Math;

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
    hdc : HDC;
    hglc: HGLRC;
    glx1, glx2, glz1, glz2: double;

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

    Function GetPointSize: double; override;
    Procedure SetPointSize(size: double); override;
    Procedure SetColor(what, r, g, b: byte); override;
    Procedure SetCulling(how: Integer); override;
    Procedure SetLineDrawMode(mode: RdLineDrawMode); override;

    Procedure DrawPolygon(p: TPolygon); override;
    Procedure DrawPolygons(ps: TPolygons); override;
    Procedure DrawPolygonsAt(ps: TPolygons;
      dx, dy, dz, pch, yaw, rol: double); override;
    Procedure DrawLine(v1, v2: TVertex); overload; override;
    Procedure DrawLine(p1, p2: TVector); overload; override;
    Procedure DrawLine(p, direction: TVector; length: double); overload; override;
    Procedure DrawLineAt(x1, y1, z1, x2, y2, z2: double); overload; override;
    Procedure DrawLineAt(x, y, z: double; direction: TVector; length: double); overload; override;
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

    Function GetCameraAt(scX, scY: integer; var x, y, z: double): boolean; override;
    Function GetXYZonPlaneAt(scX, scY: Integer; pnormal: TVector; pX, pY, pZ: double; var x, y, Z: double): boolean; override;
    Function GetGridAt(scX, scY: Integer; var x, y, Z: double): boolean; override;

    Function GetCameraForward: TVector; override; // returns camera forward vector
    Function GetCameraUp: TVector; override; // returns camera up vector
    Function GetCameraRight: TVector; override; // returns camera right vector

    Procedure ProjectPoint(x, y, Z: double; Var WinX, WinY: Integer); override;
    Procedure UnProjectPoint(WinX, WinY: double; WinZ: double; var x, y, Z: double); override;
    Function HandleWMQueryPal: Integer; override;
    Function HandleWMChangePal: Integer; override;

  private
    Function SetDCPixelFormat: TPixelFormatDescriptor;
    Function GetDCPixelFormat(hdc: HDC): TPixelFormatDescriptor;

    Procedure SetCamera;
    Procedure SetProjection;
    Procedure SetRenderMatrix;
    Procedure SetPickMatrix(x, y, size: Integer);
    Procedure RenderPoly(p: TPolygon);
    Procedure RenderLine(v1, v2: TVertex);
    Procedure RenderLineAt(x1, y1, z1, x2, y2, z2: double);
    Function CreateOGLPalette(const pd: TPIXELFORMATDESCRIPTOR): Integer;
    Function IsWirePolygonInView(p: TPolygon): boolean;


    var glctx: TGLContext;
  end;

implementation

uses Jed_Main, Lev_utils,  Prender, PRenderGL, files, graph_files, Images, J_Level;

 // TODO: Vars should be part of render cass
var
 txList, cmpList: TStringList;
  masterPalSet: boolean;
  MasterPal: TCMPPal;

{$R *.DFM}

procedure DisableFPUExceptions;
var
  FPUControlWord: WORD;
  asm
    FSTCW   FPUControlWord ;
    OR      FPUControlWord, $4 + $1 ; { Divide by zero + invalid operation }
    FLDCW   FPUControlWord ;
end;

Procedure LoadMasterPal(l:TJKLevel);
var
 i:integer;
 pal:array[0..255] of
 record
  r,g,b,a:byte;
 end;

begin
 if MasterPalSet then exit;
 GetLevelPal(l, MasterPal);

 for i:=0 to 255 do
   With Pal[i] do
     begin
      r := MasterPal[i].r;
      g := MasterPal[i].g;
      b := MasterPal[i].b;
      a := 0;
     end;

{ glEnable(GL_COLOR_INDEX8_EXT);

 pPalProc(GL_TEXTURE_2D, GL_RGB8, sizeof(Pal),
	  GL_RGBA, GL_UNSIGNED_BYTE, pal );}

 MasterPalSet:=true;
end;

Function Min(i1,i2:integer):integer;
begin
 if i1 < i2 then result := i1 else result := i2;
end;

Procedure AdjustPalGamma(var pal:TCMPPal;gamma:double);
var i:integer;
begin
 for i := 0 to 255 do
   With Pal[i] do
     begin
      r := Min(Round(r * gamma), 255);
      g := Min(Round(g * gamma), 255);
      b := Min(Round(b * gamma), 255);
     end;
end;

Function LoadTexture(const name: string; const ppal: PTCMPPal;
  const pcmp: PTCMPTable): T3DPTexture;
var
  i: integer;
  Ttx: TOGLTexture;
  pnew: TCMPPal;
  ppnew: PTCMPPal;
begin

    ppnew := nil;
    Result := nil;

    if (ppal <> nil) and (pcmp <> nil) then
      begin
        pnew := ppal^;
        ApplyCMPTable(pnew, pcmp^);
        ppnew := @pnew;
      end;
    Ttx := TOGLTexture.CreateFromMat(name, ppnew, (*gamma=*)1.0);
    Result := Ttx;
end;

Function GetTexture(const name,cmp: string):T3DPTexture;
var i:integer;
    Ttx:T3DPTexture;
    pcmp:PTCMPTable;
    tpal:TCMPPal;
    ptpal:PTCMPPal;
    f:TFile;
begin
  pcmp:= nil;
  ptpal:= nil;
  Result:=nil;

  i := TXlist.IndexOf(name + cmp);
  if i <> -1 then
    begin
      Result := T3DPTexture(TXList.Objects[i]);
      exit;
    end;

  // Load palette
  if CurrentProject <> IJIM then
    begin
      i := CmpList.IndexOf(cmp);
      if i <> -1 then pcmp := Pointer(CmpList.Objects[i]) else
        begin
          GetMem(pcmp, sizeof(pcmp^));
          if not LoadCmpTable(cmp, pcmp^) then PanMessage(mt_warning, Format('Cannot load %s', [cmp]));
          CmpList.AddObject(cmp, TObject(pcmp));
        end;

      LoadMasterPal(level);
      tpal:=MasterPal;
      //AdjustPalGamma(tpal,gamma);
      ptpal := @tpal;
    end;

 ttx := nil;

 try
  ttx := LoadTexture(name, ptpal, pcmp);
{  TGlideTexture.CreateFromMat(name,tpal,GenTexture);}
  if (ttx <> nil) and (CurrentProject <> IJIM ) then
    ttx.cmpname := cmp;
 finally
  TXList.AddObject(name + cmp, ttx);
  Result := ttx;
 end;
end;

Procedure ClearTXList;
var i: Integer;
begin
  for i := 0 to txList.Count - 1 do
    txList.Objects[i].Free;
  txList.Clear;

  for i := 0 to cmpList.Count - 1 do
    FreeMem(Pointer(cmpList.Objects[i]));
  cmpList.Clear;
end;

Procedure TOGLRenderer.BeginScene;
begin
  glctx.Acquire; // Should be released in EndScene

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
  //wglMakeCurrent(hdc, hglc);
  //glctx.Acquire;
  try
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
  finally
    glctx.Release; // Should be acquired in BeginScene
  end;
end;

Procedure TOGLRenderer.SetCamera;
begin
  { glRotated(-90,1,0,0); }

  { glRotated(45,1,0,0); }
  { glRotated(0,0,0,1);
    glRotated(0,0,1,0); }

  try
    glctx.Acquire;

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity;

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
  finally
    glctx.Release;
  end;
end;

Procedure TOGLRenderer.SetProjection;
begin
  if perspective then
    gluPerspective(60, vpw / vph, 0.01, zrange)
  else
    begin
      var dpx := vpw / ppunit / DpiScale;
      var dpy := vph / vpw * dpx;
      glOrtho(-dpx / 2, dpx / 2, -dpy / 2, dpy / 2, -zrange / 2, zrange / 2);
    end;
end;

Procedure TOGLRenderer.SetRenderMatrix;
begin
  //wglMakeCurrent(hdc, hglc);

  try
    glctx.Acquire;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity;

    SetProjection;
    SetCamera;
  finally
    glctx.Release;
  end;
end;

Procedure TOGLRenderer.SetPickMatrix(x, y, size: Integer);
var
  vpa: TVector4i;
begin
 // wglMakeCurrent(hdc, hglc);

  try
    glctx.Acquire;

    glGetIntegerv(GL_VIEWPORT, @vpa);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPickMatrix(x, vpa[3] - y, size, size, vpa);

    SetProjection;
    SetCamera;
  finally
    glctx.Release;
  end;
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
begin
//  if not InitOpenGL then
//    Raise Exception.Create('Couldn''t initialize OpenGL');

  hdc := GetDC(hViewer);
//  var pfd := SetDCPixelFormat;

//  hglc := wglCreateContext(hdc);
//  wglMakeCurrent(hdc, hglc);

 // Initialize and create OpenGL context
 // Note, can thrwo exception if OpenGL cannot be initialized
 glctx := TGLContext.Create(hdc);
 glctx.Acquire;

 // Create pal
 // Note, pfd was created by TGLContext.Create
 var pfd := GetDCPixelFormat(hdc);
 hpal := CreateOGLPalette(pfd);


  SwapBuffers(hdc);



   glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glShadeModel(GL_SMOOTH);

  glctx.Release;
  // Uncomment to enable zbuffer.
  // Note, can cause lines z fighting
//  glEnable(GL_DEPTH_TEST);
//  glDepthFunc(GL_LEQUAL);
//  glDepthMask(GL_TRUE);

  // Uncomment this block to have anti-aliased lines
//  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
//  glEnable(GL_BLEND);
//  glEnable(GL_LINE_SMOOTH);
//  glHint (GL_LINE_SMOOTH_HINT, GL_NICEST);
//  glLineWidth(1.0); //1.3 for diagonal lines
  ////////////////////////////////////////////


  { glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK); }
  //glPolygonMode(GL_Front_AND_BACK, gl_Line);
  //wglMakeCurrent(0,0);


   {glEnable(GL_DEPTH_TEST);
   glDepthRange(0,1000);
    glCullFace(GL_BACK);
    glEnable(GL_CULL_FACE);
    glFrontFace(GL_CCW);
    glClearColor(1,1,1,1); }
end;

Function TOGLRenderer.SetDCPixelFormat: TPixelFormatDescriptor;
begin
  Result.nsize :=  SizeOf(TPixelFormatDescriptor);
  Result.nVersion := 1;
  Result.dwFlags := PFD_DRAW_TO_WINDOW or PFD_SUPPORT_OPENGL or PFD_DOUBLEBUFFER;
  Result.iPixelType := PFD_TYPE_RGBA;
  Result.cColorBits := 24;
  Result.cRedBits := 0;
  Result.cRedShift := 0;
  Result.cGreenBits := 0;
  Result.cGreenShift := 0;
  Result.cBlueBits := 0;
  Result.cBlueShift := 0;
  Result.cAlphaBits := 0;
  Result.cAlphaShift := 0;
  Result.cAccumBits := 0;
  Result.cAccumRedBits := 0;
  Result.cAccumGreenBits := 0;
  Result.cAccumBlueBits := 0;
  Result.cAccumAlphaBits := 0;
  Result.cDepthBits := 32;
  Result.cStencilBits := 0;
  Result.cAuxBuffers := 0;
  Result.iLayerType := PFD_MAIN_PLANE;
  Result.iLayerType := 0;
  Result.bReserved := 0;
  Result.dwLayerMask := 0;
  Result.dwVisiblemask := 0;
  Result.dwDamageMask := 0;

  var pixfmt := ChoosePixelFormat(hdc, @Result);
  SetPixelFormat(hdc, pixfmt, @Result);
  DescribePixelFormat(hdc, pixfmt, sizeof(Result), Result);
end;

Function TOGLRenderer.GetDCPixelFormat(hdc: HDC): TPixelFormatDescriptor;
begin
 var PixelFormat := GetPixelFormat(hdc);
  if PixelFormat = 0 then
    raise Exception.Create('Failed to get pixel format');

  // Describe the pixel format
  FillChar(Result, SizeOf(Result), 0);
  Result.nSize := SizeOf(Result);
  Result.nVersion := 1;
  if not DescribePixelFormat(hdc, PixelFormat, SizeOf(Result), Result) then
    raise Exception.Create('Failed to describe pixel format');
end;

Procedure TOGLRenderer.SetViewPort(x, y, w, h: Integer);
begin
  Inherited SetViewPort(x, y, w, h);
  try
    glctx.Acquire;
    glViewPort(x, y, w, h);
  finally
    glctx.Release;
  end;
end;

Procedure TOGLRenderer.DrawPolygon(p: TPolygon);
begin
  RenderPoly(p);
end;

Function TOGLRenderer.IsWirePolygonInView(p: TPolygon): Boolean;
  var cullmode: integer;
    vp: TVector4i;
  pmx, mmx: TGLMatrixd4;
  campos, vdir: TVector;

begin
  Result := BitMaskTest(p.FaceFlags, FF_DoubleSided);
  if not Result and glIsEnabled(GL_CULL_FACE) then
  begin
    glGetIntegerv(GL_CULL_FACE_MODE, @cullmode);
    if cullmode = GL_FRONT_AND_BACK then
      exit;

    // Note, the glctx should be already acquired at this point
    if perspective then
      begin
        //SetRenderMatrix;
        glGetIntegerv(GL_VIEWPORT, @vp);
        glGetDoublev(GL_MODELVIEW_MATRIX, @mmx);
        glGetDoublev(GL_PROJECTION_MATRIX, @pmx);

        gluUnProject(0, vp[3] - 0, 0, mmx, pmx, vp, @campos.x, @campos.y, @campos.z);
        with p.vertices[0] do
          vdir := -campos.Sub(x, y, z);
      end
    else // ortho
      vdir := GetCameraForward;

    // Note this method works ok only for convex polygons
    var dot := vdir.Dot(p.normal);
    if (cullmode = GL_FRONT) and (dot < 0) then
      exit
    else if (cullmode = GL_BACK) and (dot >= 0) then
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
      if not IsWirePolygonInView(ps[i]) then continue;
      with ps[i] do
      begin

        var drawMode :=  GL_LINE_LOOP;
        var bSolid := False;
        if (geo = 4) and (Length(material) > 0) then
        begin
          drawMode :=  GL_TRIANGLES;
          TOGLTexture(GetTexture(material, '')).SetCurrent;
          bSolid := True;
        end
        else
          TOGLTexture(nil).SetCurrent;


        n := vertices.count;
//        case n of
//          3:
//            glBegin(GL_triangles);
//          4:
//            glBegin(GL_Quads);
//        else
//          glBegin(GL_Polygon);
//        end;
        glBegin(drawMode);
        for j := 0 to vertices.count - 1 do
          with vertices[j] do
          begin
            ax := x;
            ay := y;
            az := z;
            MultVM3(mx, ax, ay, az);
            glVertex3d(ax + dx, ay + dy, az + dz);
            if bSolid then
              with txVertices[j] do
                glTexCoord2f(u, v);
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
  if not IsWirePolygonInView(p) then
    exit;


  var drawMode :=  GL_LINE_LOOP;
  var bSolid := False;
  var invMatWidth := 1 / 1;
  var invMatHeight := 1 / 1;

  if (p.geo = 4) and (Length(p.material) > 0) then
  begin
    drawMode :=  GL_TRIANGLE_FAN;
    var tx:= GetTexture(p.material, '') ;
    invMatWidth := 1 / tx.width;
    invMatHeight := 1 / tx.height;

    TOGLTexture(tx).SetCurrent;
    bSolid := True;

  end
  else
    TOGLTexture(nil).SetCurrent;


  n := p.vertices.Count;
  glBegin(drawMode);
    for i := 0 to p.vertices.Count - 1 do
      with p.vertices[i] do
      begin
         if bSolid then
              with p.txVertices[i] do
                glTexCoord2f(u*invMatWidth , v *invMatHeight );
        glVertex3d(x, y, z);
        // glNormal3d(p.Normal.dx,p.normal.dz, p.normal.dy);

      end;
  glEnd;
end;

Procedure TOGLRenderer.DrawVertex(x, y, Z: double);
begin
  glBegin(gl_points);
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

      // c + right * cos(th) * rad + up * sin(th) * rad
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
  RenderLineAt(v1.x, v1.y, v1.z, v2.x, v2.y, v2.z);
end;

Procedure TOGLRenderer.RenderLineAt(x1, y1, z1, x2, y2, z2: double);
begin
  glBegin(GL_LINES);
  glVertex3d(x1, y1, z1);
  glVertex3d(x2, y2, z2);
  glEnd;
end;

Procedure TOGLRenderer.DrawLine(v1, v2: TVertex);
begin
  RenderLine(v1, v2);
end;

Procedure TOGLRenderer.DrawLine(p1, p2: TVector);
begin
  RenderLineAt(p1.x, p1.y, p1.z, p2.x, p2.y, p2.z);
end;

Procedure TOGLRenderer.DrawLine(p, direction: TVector; length: double);
begin
   DrawLineAt(p.x, p.y, p.z, direction, length);
end;

Procedure TOGLRenderer.DrawLineAt(x1, y1, z1, x2, y2, z2: double);
begin
   RenderLineAt(x1, y1, z1, x2, y2, z2);
end;

Procedure TOGLRenderer.DrawLineAt(x, y, z: double; direction: TVector; length: double);
begin
  RenderLineAt(x, y, z, x + direction.x * length, y + direction.y * length, z + direction.z * length);
end;

Destructor TOGLRenderer.Destroy;
begin
  glctx.Free;

  if (hdc <> 0) then
  begin
    DeleteDC(hdc);
  end;

  if Hpal <> 0 then
    DeleteObject(Hpal);

  glFlush;
  inherited Destroy;
end;

Procedure TOGLRenderer.BeginPick(x, y: Integer);
begin
  glctx.Acquire;  // Should be released in EndPick

  glSelectBuffer(sizeof(selectBuf), @selectBuf);
  glRenderMode(GL_SELECT);

  { glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT); }

  glDisable(GL_CULL_FACE);

  glInitNames();
  glPushName(0);
  SetPickMatrix(x, y, 4);
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
  try
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

    zs.Free;

    glRenderMode(GL_RENDER);
    glDisable(GL_CULL_FACE);

  finally
    glctx.Release; // Should be acquired in BeginPick
  end;

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

Function TOGLRenderer.GetCameraAt(scX, scY: integer; var x, y, z: double): boolean;
var
  vp: TVector4i;
  pmx, mmx: TGLMatrixd4;
  ax1, ay1, az1, ax2, ay2, az2: double;
  d: double;
  cx, cy,cz: double;
begin
  try
    glctx.Acquire;
    SetRenderMatrix;

    glGetIntegerv(GL_VIEWPORT, @vp);
    glGetDoublev(GL_MODELVIEW_MATRIX, @mmx);
    glGetDoublev(GL_PROJECTION_MATRIX, @pmx);

    gluUnProject(scX, vp[3] - scY, 0, mmx, pmx, vp, @x, @y, @z);
    gluUnProject(scX, vp[3] - scY, 1, mmx, pmx, vp, @ax2, @ay2, @az2);
    //VectorSub3(ax2, ay2, az2, ax1, ay1, az1, x, y, z);
    Result := True;
    Result := GetXYZonPlaneAt(scX, scY, zv, -CamX, -CamY, -CamZ, x, y, z);
    if Result then
      begin
        x := JKRound(x);
        y := JKRound(y);
        Z := JKRound(Z);
      end;
  finally
    glctx.Release;
  end;
end;

Function TOGLRenderer.GetXYZonPlaneAt(scX, scY: Integer; pnormal: TVector;
  pX, pY, pZ: double; var x, y, Z: double): boolean;
var
  vp: TVector4i;
  pmx, mmx: TGLMatrixd4;
  ax1, ay1, az1, ax2, ay2, az2: double;
  d: double;
  cx, cy,cz: double;
begin
  try
    glctx.Acquire;
    SetRenderMatrix;

    glGetIntegerv(GL_VIEWPORT, @vp);
    glGetDoublev(GL_MODELVIEW_MATRIX, @mmx);
    glGetDoublev(GL_PROJECTION_MATRIX, @pmx);

    gluUnProject(scX, vp[3] - scY, 0, mmx, pmx, vp, @ax1, @ay1, @az1);
    gluUnProject(scX, vp[3] - scY, 1, mmx, pmx, vp, @ax2, @ay2, @az2);

    { ay1:=-ay1;
      ay2:=-ay2; }

    With pnormal do
      d := dx * pX + dy * pY + dz * pZ;

    Result := PlaneLineXn(pnormal, d, ax1, ay1, az1, ax2, ay2, az2, x, y, Z);
    //Result := PlaneLineXnNew(pnormal, pX, pY, pZ, ax1, ay1, az1, ax2, ay2, az2, x, y, Z);
    Result := Result and (abs(x) < xrange) and (abs(y) < Yrange) and
      (abs(Z) < ZRange);
    if not Result then
      exit;

    // note, this rounding makes flickering when
    // moving viewport with mouse at  super zoom-in level.
    x := JKRound(x);
    y := JKRound(y);
    Z := JKRound(Z);
  finally
    glctx.Release;
  end;
end;

Function TOGLRenderer.GetGridAt(scX, scY: Integer; var x, y, Z: double)
  : boolean;
begin
  Result := GetXYZonPlaneAt(scX, scY, gnormal, GridX, GridY, GridZ, x, y, Z);
//  if Result then
//  begin
//    x := JKRound(x);
//    y := JKRound(y);
//    Z := JKRound(Z);
//  end;
end;

Function TOGLRenderer.GetCameraForward: TVector;
  var vp: TVector4i;
      pmx, mmx: TGLMatrixd4;
      ax1, ay1, az1, ax2, ay2, az2: double;
begin
  if not perspective then // ortho
    Result := -zv
  else
    begin
      try
        glctx.Acquire;
        SetRenderMatrix;

        glGetIntegerv(GL_VIEWPORT, @vp);
        glGetDoublev(GL_MODELVIEW_MATRIX, @mmx);
        glGetDoublev(GL_PROJECTION_MATRIX, @pmx);

        gluUnProject(0, vp[3] - 0, 0, mmx, pmx, vp, @ax1, @ay1, @az1);
        gluUnProject(0, vp[3] - 0, 1, mmx, pmx, vp, @ax2, @ay2, @az2);
        VectorSub3(ax2, ay2, az2, ax1, ay1, az1, Result.x, Result.y, Result.z);
        Result.Normalize;
      finally
        glctx.Release
      end;
    end;
end;

Function TOGLRenderer.GetCameraUp: TVector;
begin
  Result := yv;
end;

Function TOGLRenderer.GetCameraRight: TVector;
begin
  Result := xv;
end;

Procedure TOGLRenderer.UnProjectPoint(WinX, WinY: double; WinZ: double; var x, y, z: double);
var
  vp: TVector4i;
  pmx, mmx: TGLMatrixd4;
begin
  try
    glctx.Acquire;
    SetRenderMatrix;

    glGetIntegerv(GL_VIEWPORT, @vp);
    glGetDoublev(GL_MODELVIEW_MATRIX, @mmx);
    glGetDoublev(GL_PROJECTION_MATRIX, @pmx);
    gluUnProject(WinX, vp[3] - WinY, WinZ, mmx, pmx, vp, @x, @y, @z);
    { y:=-y; }
  finally
     glctx.Release;
  end;
end;

Procedure TOGLRenderer.ProjectPoint(x, y, z: double; Var WinX, WinY: Integer);
var
  vp: TVector4i;
  pmx, mmx: TGLMatrixd4;
  WX, WY, WZ: double;
begin
  try
    glctx.Acquire;
    SetRenderMatrix;

    glGetIntegerv(GL_VIEWPORT, @vp);
    glGetDoublev(GL_MODELVIEW_MATRIX, @mmx);
    glGetDoublev(GL_PROJECTION_MATRIX, @pmx);
    gluProject(x, y, z, mmx, pmx, vp, @WX, @WY, @WZ);

    WinX := Round(WX);
    WinY := vp[3] - Round(WY);
  finally
    glctx.Release;
  end;
end;

Function TOGLRenderer.GetPointSize: double;
begin
   glctx.Acquire;
   glGetDoublev(GL_POINT_SIZE, @Result);
   glctx.Release;
end;

Procedure TOGLRenderer.SetPointSize(size: double);
begin
  glctx.Acquire;
  glPointSize(size);
  glctx.Release;
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
  glctx.Acquire;
  try
    case what of
      CL_BACKGROUND:
        begin
          glColor3ub(r, g, b);
          bgnd_clr.r := r;
          bgnd_clr.g := g;
          bgnd_clr.b := b;
        end;
      else // front or back
        begin
          glColor3ub(r, g, b);
          Front_clr.r := r;
          Front_clr.g := g;
          Front_clr.b := b;
        end;
    end;
  finally
    glctx.Release;
  end;
end;

Procedure TOGLRenderer.SetCulling(how: Integer);
begin
  case how of
    R_CULLNONE:
      glDisable(GL_CULL_FACE);
    R_CULLBACK:
      begin
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
      end;
    R_CULLFRONT:
      begin
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
      end;
  end;
end;

Procedure TOGLRenderer.SetLineDrawMode(mode: RdLineDrawMode);
begin
   Inherited SetLineDrawMode(mode);

   case mode of
     RdSolidLine: glDisable(GL_LINE_STIPPLE);
     RdDashedLine:
     begin
        glEnable(GL_LINE_STIPPLE);
        glLineStipple(1, $0101);
     end;
   end;
end;

Initialization
begin
  TxList := TStringList.Create;
  TXList.Sorted := true;

  CmpList := TStringList.Create;
  CmpList.Sorted := true;

end;
end.
