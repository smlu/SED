unit ogl_PRender;

interface

uses Windows, GLContext, dglOpenGL, Prender, Classes, J_Level, Forms,
  Messages, files, FileOperations, graph_files,
  lev_utils, sysUtils, misc_utils, GlobalVars, geometry,
  Math, images, ExtCtrls;

type
  pCOlorTableExt = Procedure(target: GLenum; internalformat: GLenum;
    width: GLsizei; format: GLenum; ttype: GLenum; const table); stdcall;

  pColorSubTableExt = Procedure(target: GLenum; start: GLsizei; count: GLsizei;
    format: GLenum; ttype: GLenum; const table); stdcall;

  TOGLTexture = class(T3DPTexture)
    ObjIdx: GLuint;
    Constructor CreateFromMat(const Mat: string; const ppal: PTCMPPal;
      gamma: double);
    Procedure SetCurrent; { override; }
    Destructor Destroy; override;
  end;

  // TODO: Implement fog rendering
  TOGLPRenderer = class(TNewPRenderer)
    hdc: HDC;
    //hglc: HGLRC;
    hpal: integer;

    Destructor Destroy; override;
    Procedure Initialize; override;
    Function IsFogSupported(): Boolean;override;
    Procedure SetFog(color: TColorF; fogStart, fogEnd: double; density: Double = 1.0);override;
    Function LoadTexture(const name: string; const ppal: PTCMPPal; const pcmp: PTCMPTable): T3DPTexture; override;

    Procedure EnableAlphaTest(bEnable: Boolean); override;
    Procedure EnableZTest(enable: Boolean); override;
    Procedure DrawPolys(const [Ref] polys: TArray<T3DPoly>; count: Integer = -1); override;
    //Procedure DrawMesh(m: T3DPMesh); override;

    Procedure GetWorldLine(X, Y: integer; var X1, Y1, Z1, X2, Y2, Z2: double); override;
    Function ProjectPoint(x, y, z: double; Var ClipX, ClipY, ClipW: double): Boolean; override;
    // Procedure ClearTXList; override;

    Procedure SetClearColor(color: TColorF); override;
    Procedure Redraw; override;
  Private
    //alphaPass: boolean;
    Procedure SetMatrix;
    Function CreateOGLPalette(const pd: TPIXELFORMATDESCRIPTOR): integer;
    Procedure SetRendererState(const faceflags: longint);

    var glctx: TGLContext;
  end;

implementation
uses System.Math;

var
  ntex: integer = 0;

Const
  GL_COLOR_INDEX8_EXT = $80E5;
  GL_RGB8 = $8051;
  GL_RGB5 = $8050;

var
  pPalProc: pCOlorTableExt;

  { pfnColorTableEXT = (PFNGLCOLORTABLEEXTPROC)
    wglGetProcAddress("glColorTableEXT");
    if (pfnColorTableEXT == NULL)
    return FALSE;
    pfnColorSubTableEXT = (PFNGLCOLORSUBTABLEEXTPROC)
    wglGetProcAddress("glColorSubTableEXT");
    if (pfnColorSubTableEXT == NULL)
    return FALSE;

    glTexImage2D( GL_TEXTURE_2D, 0, GL_COLOR_INDEX8_EXT,
    sizeX, sizeY, 0,
    GL_COLOR_INDEX, GL_UNSIGNED_BYTE,
    _data );

    pfnColorTableEXT(GL_TEXTURE_2D, GL_RGB8, sizeof(YOUR_PALETTE),
    GL_RGBA, GL_UNSIGNED_BYTE, YOUR_PALETTE ); }

procedure DisableFPUExceptions;
var
  FPUControlWord: WORD;
  asm
    FSTCW   FPUControlWord ;
    OR      FPUControlWord, $4 + $1 ; { Divide by zero + invalid operation }
    FLDCW   FPUControlWord ;
end;

Destructor TOGLPRenderer.Destroy;
begin
  glctx.Free();

  if (hdc <> 0) then
    DeleteDC(hdc);

  if hpal <> 0 then
    DeleteObject(hpal);

  glFlush;
  inherited Destroy;
end;

Procedure TOGLPRenderer.Initialize;
var
  pfd: TPIXELFORMATDESCRIPTOR;
  pixelFormat: integer;
  hgl: integer;
begin
//  if not InitOpenGL then
//    Raise Exception.Create('Couldn''t initialize OpenGL');

  hdc := GetDC(WHandle);
//  pfd.nsize := 40;
//  pfd.nVersion := 1;
//  pfd.dwFlags := PFD_DRAW_TO_WINDOW + PFD_SUPPORT_OPENGL + PFD_DOUBLEBUFFER;
//  pfd.iPixelType := PFD_TYPE_RGBA;
//  pfd.cColorBits := 8;
//  pfd.cRedBits := 0;
//  pfd.cRedShift := 0;
//  pfd.cGreenBits := 0;
//  pfd.cGreenShift := 0;
//  pfd.cBlueBits := 0;
//  pfd.cBlueShift := 0;
//  pfd.cAlphaBits := 0;
//  pfd.cAlphaShift := 0;
//  pfd.cAccumBits := 0;
//  pfd.cAccumRedBits := 0;
//  pfd.cAccumGreenBits := 0;
//  pfd.cAccumBlueBits := 0;
//  pfd.cAccumAlphaBits := 0;
//  pfd.cDepthBits := 32;
//  pfd.cStencilBits := 0;
//  pfd.cAuxBuffers := 0;
//  pfd.iLayerType := PFD_MAIN_PLANE;
//  pfd.iLayerType := 0;
//  pfd.bReserved := 0;
//  pfd.dwLayerMask := 0;
//  pfd.dwVisiblemask := 0;
//  pfd.dwDamageMask := 0;
//
//  pixelFormat := ChoosePixelFormat(hdc, @pfd);
//  SetPixelFormat(hdc, pixelFormat, @pfd);
//  DescribePixelFormat(hdc, pixelFormat, sizeof(pfd), pfd);
//
//  hpal := CreateOGLPalette(pfd);
//


  // Initialize and create OpenGL context
  glctx := TGLContext.Create(hdc);
  try
    glctx.Acquire();

    SwapBuffers(hdc);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glShadeModel(GL_SMOOTH);

    // Wire line
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glLineWidth(2.3);

    // Enable MSAA  for GL_SAMPLE_ALPHA_TO_COVERAGE
    glEnable(GL_MULTISAMPLE);

    @pPalProc := wglGetProcAddress('glColorTableEXT');
    if (@pPalProc = nil) then
    begin
      PanMessage(mt_info,
        'Your OpenGL implemetation doesn''t support palettized textures!');
    end;

    if CurrentProject = TProjectType.IJIM then
      begin
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LEQUAL);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      end;

    // Fog
    if not IsFogSupported() then
      fog.Enabled := false;
    glDisable(GL_FOG);

    // Set clear color
    glClearColor(1, 1, 1, 1);
  finally
    glctx.Release();
  end;
end;

Function TOGLPRenderer.CreateOGLPalette
  (const pd: TPIXELFORMATDESCRIPTOR): integer;
var
  ncolors: Word;
  lp: PLogPalette;
  i: integer;
  rrange, grange, brange: byte;
begin
  Result := 0;
  if pd.dwFlags and PFD_NEED_PALETTE = 0 then
    exit;
  ncolors := Word(1 shl pd.cColorBits);
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

Procedure TOGLPRenderer.SetRendererState(const faceflags: longint);
begin
  if BitMaskTest(faceflags, FF_DoubleSided) then
    glDisable(GL_CULL_FACE)
  else
    glEnable(GL_CULL_FACE);

  // Set tex wrap mode
  var texWrapX := GL_REPEAT;
  var texWrapY := GL_REPEAT;
  if BitMaskTest(faceflags, FF_TexClampX) then texWrapX := GL_CLAMP_TO_EDGE;
  if BitMaskTest(faceflags, FF_TexClampY) then texWrapY := GL_CLAMP_TO_EDGE;
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, texWrapX);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, texWrapY);

  // Set tex filtering
  var magFilter := GL_LINEAR; // Bilinear
  var minFilter := GL_LINEAR_MIPMAP_LINEAR; // trilinear
  if Addr(glGenerateMipmap) = nil then
     minFilter := GL_LINEAR;  // Bilinear

  if BitMaskTest(faceflags, FF_TexNoFiltering) then
  begin
    magFilter := GL_NEAREST; // Bilinear
    minFilter := GL_NEAREST; // trilinear
  end;
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);

  // Set Z-write mode
  var zwriteEnabled := GL_TRUE;
  if BitMaskTest(faceflags, FF_ZWriteDisabled) then zwriteEnabled := GL_FALSE;
  glDepthMask(zwriteEnabled);

  if (CurrentProject = IJIM) and fog.Enabled and ((faceflags and FF_IJIM_FogEnabled) <> 0) then
    glEnable(GL_FOG)
  else
    glDisable(GL_FOG);
end;

Function Min(i1, i2: integer): integer;
begin
  if i1 < i2 then
    Result := i1
  else
    Result := i2;
end;

{ Procedure TOGLPRenderer.SetViewPort(x,y,w,h:integer);
  begin
  end; }

Procedure TOGLPRenderer.EnableAlphaTest(bEnable: Boolean);
begin
  if bEnable then
    begin
      glEnable(GL_ALPHA_TEST);
      glAlphaFunc(GL_GEQUAL, 0.01);
      glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    end
  else
    begin
      glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
      glDisable(GL_ALPHA_TEST);
    end;
end;

Procedure TOGLPRenderer.EnableZTest(enable: Boolean);
begin
  if enable then
    glEnable(GL_DEPTH_TEST)
  else
    glDisable(GL_DEPTH_TEST);
end;

Procedure DrawVertex(vertex: TVXDets; vertexColor: TColorF; setUV, hasAlpha: Boolean);
begin
  if hasAlpha then
    glColor4ub(L2B(vertexColor.r), L2B(vertexColor.g), L2B(vertexColor.b), L2B(vertexColor.a))
  else
    glColor3ub(L2B(vertexColor.r), L2B(vertexColor.g), L2B(vertexColor.b));

  if setUV then
    glTexCoord4f(vertex.u * vertex.w, vertex.v * vertex.w, 0.0, vertex.w);
    //glTexCoord2f(vertex.u, vertex.v);

  // Draw vertices
  glVertex3f(vertex.X, vertex.Y, vertex.z);
end;

Procedure TOGLPRenderer.DrawPolys(const [Ref] polys: TArray<T3DPoly>; count: Integer);
var
  poly: T3DPoly;
  vxd: TVXdets;
begin
  if (count < 0) or (count > Length(polys)) then
    count := Length(polys);

  if polys = nil then
    exit;

  var drawMode   := GL_LINE_LOOP;
  const solidDraw = self.geoMode > Wireframe;
  if solidDraw then drawMode := GL_TRIANGLES;

  for var i := 0 to count - 1 do
  begin
    poly := polys[i];
    if poly = nil then continue;

    var tex: T3DPTexture := nil;
    var fflags := 0;
    if solidDraw then
      begin
        fflags := poly.faceflags;
        if poly.geo = Texture then
          tex := poly.tx;
      end;

    TOGLTexture(tex).SetCurrent;
    SetRendererState(fflags);

    var setUV    := solidDraw and (tex <> nil);
    var hasAlpha := solidDraw and BitMaskTest(poly.faceflags, FF_Transluent);

    glBegin(drawMode);
      if (poly.vxds.count <= 3) or not solidDraw then
        begin
            for var j := 0 to poly.vxds.count - 1 do
              begin
                vxd := poly.getVXD(j);
                var vcolor := wfColor;
                if solidDraw then
                  vcolor := poly.GetLitColor(vxd.intensity);
                DrawVertex(vxd, vcolor, setUV, hasAlpha);
              end;
        end
      else
        begin
          const totalTris = poly.vxds.count - 2;
          var vertIdx0 := 0;
          var vertIdx1 := 1;
          var vertIdx2 := poly.vxds.count - 1;
          for var idx := 0 to totalTris do
            begin
              vxd := poly.getVXD(vertIdx0);
              var vcolor := wfColor;
              if solidDraw then
                vcolor := poly.GetLitColor(vxd.intensity);
              DrawVertex(vxd, vcolor, setUV, hasAlpha);

              vxd := poly.getVXD(vertIdx1);
              if solidDraw then
                vcolor := poly.GetLitColor(vxd.intensity);
              DrawVertex(vxd, vcolor, setUV, hasAlpha);

              vxd := poly.getVXD(vertIdx2);
              if solidDraw then
                vcolor := poly.GetLitColor(vxd.intensity);
              DrawVertex(vxd, vcolor, setUV, hasAlpha);

              if (idx and 1) = 0 then //((not idx) and 1) <> 0 then  i.e.: if even
                begin
                  vertIdx0 := vertIdx1;
                  vertIdx1 := vertIdx1 + 1;
                end
              else // if idx is odd
                begin
                  vertIdx0 := vertIdx2;
                  vertIdx2 := vertIdx2 - 1;
                end;
            end;
        end;
    glEnd;
  end;
end;

Procedure TOGLPRenderer.GetWorldLine(X, Y: integer;
  var X1, Y1, Z1, X2, Y2, Z2: double);
var
  vp: TVector4i;
  pmx, mmx: TGLMatrixd4;
  { ax1,ay1,az1,
    ax2,ay2,az2:double; }
  d: double;
begin
  try
    glctx.Acquire();

    SetMatrix;
    glGetIntegerv(GL_VIEWPORT, @vp);
    glGetDoublev(GL_MODELVIEW_MATRIX, @mmx);
    glGetDoublev(GL_PROJECTION_MATRIX, @pmx);

    gluUnProject(X, vp[3] - Y, 0, mmx, pmx, vp, @X1, @Y1, @Z1);
    gluUnProject(X, vp[3] - Y, 1, mmx, pmx, vp, @X2, @Y2, @Z2);
  finally
      glctx.Release();
  end;
end;

Function TOGLPRenderer.ProjectPoint(x, y, z: double; Var ClipX, ClipY, ClipW: double): Boolean;
var
  vp: TGLVectori4;
  pmx, mmx: TGLMatrixd4;
  WX, WY, WZ: double;
  i:integer;
  eyePos, objPos, clipPos: TGLVectord4;
begin
  SetMatrix;
  glGetIntegerv(GL_VIEWPORT, @vp);
  glGetDoublev(GL_MODELVIEW_MATRIX, @mmx);
  glGetDoublev(GL_PROJECTION_MATRIX, @pmx);

  objPos[0] := x;
  objPos[1] := y;
  objPos[2] := z;
  objPos[3] := 1.0;

  // Transform object to eye space
  for i := 0 to 3 do
  begin
    eyePos[i] := mmx[0][i] * objPos[0] + mmx[1][i] * objPos[1] + mmx[2][i] * objPos[2] + mmx[3][i] * objPos[3];
  end;

  // Apply Projection Transformation (Eye space to Clip space)
  for i := 0 to 3 do
  begin
    clipPos[i] := pmx[0][i] * eyePos[0] + pmx[1][i] * eyePos[1] + pmx[2][i] * eyePos[2] + pmx[3][i] * eyePos[3];
  end;

  ClipX :=  clipPos[0];
  ClipY := -clipPos[1]; // Y is flipped in GL
  ClipW :=  clipPos[3];

  Result := True;
end;

Procedure TOGLPRenderer.SetMatrix;
begin
  try
    glctx.Acquire();

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity;

    with curCamera do
    begin
      //var fova: double := 2 * arctan(vheight / vwidth) / pi * 180;
      var fovY := 2 * RadToDeg(ArcTan(Tan(DegToRad(fov) / 2) / aspect)); // Convert camera fovX to fovY
      with frustum do
        gluPerspective(fovY, aspect, nearPlane.distance, farPlane.distance);

      glMatrixMode(GL_MODELVIEW);
      glLoadIdentity;

      // Rotate
      glRotated(-90, 1, 0, 0);
      glRotated(rotation.pitch, 1, 0, 0);
      glRotated(rotation.yaw, 0, 0, 1);

      // Move
      glTranslated(-position.x, -position.y, -position.z);
    end;
  finally
    glctx.Release();
  end;
end;

Procedure TOGLPRenderer.SetClearColor(color: TColorF);
begin
  glctx.Acquire();
  glClearColor(color.r, color.g, color.b, color.a);
  glctx.Release();
end;

Procedure TOGLPRenderer.Redraw;
var
  i: integer;
  v1, v2, v3: Tvector;
begin
  DisableFPUExceptions;
  try
    glctx.Acquire();
    glClear(GL_COLOR_BUFFER_BIT or GL_DEPTH_BUFFER_BIT);

    // Update Fog
    if fog.enabled then
    begin
      glFogfv(GL_FOG_COLOR, @fog.color);
      glFogi(GL_FOG_MODE, GL_LINEAR);

      var ff := 0.03459; // TODO: this is hack for world space fog depth
      glFogf(GL_FOG_START, GLFloat(fog.fogStart * ff));
      var ffogEnd: GLFloat := (2.0 - fogDensity) * fog.FogEnd * ff;
      glFogf(GL_FOG_END, ffogEnd);
    end;

    SetMatrix;

    // Enable line offset to fix z fighting
    // between wireframe frawn over solid geometry
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0, 1.0);

    // Now do the drawing
    //alphaPass := False;
    inherited Redraw;
    //alphaPass := True;
    //inherited Redraw;
     glDisable(GL_POLYGON_OFFSET_FILL);

    SwapBuffers(hdc);

  finally
    glctx.Release();
  end;
end;

Function TOGLPRenderer.IsFogSupported(): Boolean;
begin
  Result := ((@glFogf <> nil) and (@glFogfv <> nil) and (@glFogi <> nil) and (@glFogiv <> nil));
end;

Procedure TOGLPRenderer.SetFog(color: TColorF; fogStart, fogEnd: double; density: Double = 1.0);
begin
  Inherited SetFog(color, fogStart, fogEnd, density);
  if fogDensity = 0.0 then
    begin
      fogStart := 0;
      fogEnd   := 0;
    end;

  try
    glctx.Acquire();
    glFogfv(GL_FOG_COLOR, @color);
    glFogi(GL_FOG_MODE, GL_LINEAR);

    var ff := 0.03459; // TODO: this is hack for world space fog depth
    glFogf(GL_FOG_START, GLFloat(fogStart * ff));
    var ffogEnd: GLFloat := (2.0 - fogDensity) * fog.FogEnd * ff;
    glFogf(GL_FOG_END, ffogEnd);
  finally
    glctx.Release();
  end;

end;

Function TOGLPRenderer.LoadTexture(const name: string; const ppal: PTCMPPal;
  const pcmp: PTCMPTable): T3DPTexture;
var
  i: integer;
  Ttx: TOGLTexture;
  pnew: TCMPPal;
  ppnew: PTCMPPal;
begin
  try
    ppnew := nil;
    Result := nil;

    glctx.Acquire();
    if (ppal <> nil) and (pcmp <> nil) then
      begin
        pnew := ppal^;
        ApplyCMPTable(pnew, pcmp^);
        ppnew := @pnew;
      end;
    Ttx := TOGLTexture.CreateFromMat(name, ppnew, gamma);
    Result := Ttx;
    //wglMakeCurrent(0, 0);
  finally
    glctx.Release();
  end;
end;

//Procedure TOGLPRenderer.ClearTXList;
//var
//  i: integer;
//  var DayHour : array[0..1] of GLuint;
//begin
//  wglMakeCurrent(hdc, hglc);
//  glBindTexture(GL_TEXTURE_2D, 0);
//  //glFlush;
//  for i := txlist.count - 1 downto 0 do
//  begin
//  var pp: PGLuint := @TOGLTexture(txlist.Objects[i]).ObjIdx;
//  var p:=  TOGLTexture(txlist.Objects[i]).ObjIdx;
//  var uu := GLuint(@p);
//    if @glDeleteTextures <> nil then
//      glDeleteTextures(1, @pp);
//  end;
//  inherited ClearTXList;
//end;

Constructor TOGLTexture.CreateFromMat(const Mat: string; const ppal: PTCMPPal; gamma: double);
var
  i, j: integer;
  pb: PByte;
  mf: TMat;
  f: TFile;
  //pl, pline: PAnsiChar;
  n: integer;
  bits: PByte;
  //c: AnsiChar;
  pw: PWord;
  cf: TColorInfo;
  var opal: array [0 .. 255] of record r, g, b, a: byte; end;

  Procedure LoadRGB;
  begin
    if mf.Info.ci.mode = Indexed then
    begin
      if ppal = nil then
      begin
        PanMessage(mt_warning,
            Format('TOGLTexture: Failed to load bitmap for MAT file %s. No pallette in use!',[mat]));
        exit;
      end;
      gamma := 1.0; // See AdjustPalGamma
    end;

    // Read texture pixelmap
    mf.ReadImage(pb, cf, width * cf.PixelSize, ppal, gamma);
  end;

begin

  f := OpenGameFile(Mat);
  mf := TMat.Create(f, 0);
  try
    begin
      name   := ExtractFileName(mat);
      ci     := mf.Info.ci;
      width  := mf.Info.width;
      height := mf.Info.height;

      var components := GL_COLOR_INDEX8_EXT;
      var format     := GL_COLOR_INDEX;
      var usepalette: Boolean := false;//(ppal <> nil) and (mf.info.IsIndexed) and (@pPalProc <> nil);
      if not usepalette then
      begin
        cf := BGR24;
        format := GL_RGB;
        components := cf.PixelSize;
        if ci.mode = RGBA then
        begin
          cf := ABGR32;
          format := GL_RGBA;
          components := cf.PixelSize;
        end;
      end;

      if components = GL_COLOR_INDEX8_EXT then
        GetMem(pb, width * height * 1)
      else
        GetMem(pb, width * height * components);
      bits := pb;

      //GetMem(pl, width * 2);

      if usepalette then
        mf.LoadBits(pb^)
      else
        LoadRGB;

      { for i:=0 to height-1 do
        begin
        mf.GetLine(pb^);
        inc(pb,width);
        end; }

      { for i:=0 to height-1 do
        begin
        pline:=pl;
        mf.GetLine(pline^);
        for j:=0 to width-1 do
        begin
        n:=ord(pline^);
        With Pal[n] do
        begin
        pb^:=chr(r);
        (pb+1)^:=chr(g);
        (pb+2)^:=chr(b);
        end;
        inc(pb,3);
        inc(pline);
        end;

        end; }
      //FreeMem(pl);

      glGenTextures(1, @ObjIdx);
      glBindTexture(GL_TEXTURE_2D, ObjIdx);

      glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
      glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

      var magFilter := GL_LINEAR; // Bilinear
      var minFilter := GL_LINEAR_MIPMAP_LINEAR; // trilinear
      if Addr(glGenerateMipmap) = nil then
         minFilter := GL_LINEAR;  // Bilinear
      glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
      glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);

      { a:=GL_SPHERE_MAP;
        glTexGeniv(GL_S,GL_TEXTURE_GEN_MODE,@a);
        glTexGeniv(GL_T,GL_TEXTURE_GEN_MODE,@a);
        glTexGeniv(GL_R,GL_TEXTURE_GEN_MODE,@a);
        glTexGeniv(GL_Q,GL_TEXTURE_GEN_MODE,@a); }

      { glTexImage2D(GL_TEXTURE_2D, 0,3,width, height, 0,
        GL_RGB, GL_UNSIGNED_BYTE, bits^); }

      if usepalette then
        begin
            for i := 0 to 255 do
            begin
              opal[i].r := ppal[i].r;
              opal[i].g := ppal[i].g;
              opal[i].b := 0;//ppal[i].b;
              opal[i].a := 0;
            end;

          pPalProc(GL_TEXTURE_2D, GL_RGB8, 256, GL_RGBA, GL_UNSIGNED_BYTE, opal);
          glTexImage2D(GL_TEXTURE_2D, 0, components, width, height, 0,
            format, GL_UNSIGNED_BYTE, bits);
        end
      else
        begin
          glTexImage2D(GL_TEXTURE_2D, 0, components, width, height, 0, format,
            GL_UNSIGNED_BYTE, bits);

          if Addr(glGenerateMipmap) <> nil then
            glGenerateMipmap(GL_TEXTURE_2D);
        end;

      { pPalProc(GL_TEXTURE_2D, GL_RGB8, 256,
        GL_RGBA, GL_UNSIGNED_BYTE, opal );


        glTexImage2D( GL_TEXTURE_2D, 0, GL_COLOR_INDEX8_EXT,
        width, height, 0,
        GL_COLOR_INDEX, GL_UNSIGNED_BYTE,
        bits^ ); }

      i := glGetError;
      if i <> 0 then;

      glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST); { GL_NICEST); }
      { glBindTexture(GL_TEXTURE_2D,0); }

      FreeMem(bits);
    end
  finally
    begin
      mf.Free;
    end;
  end;
end;

Procedure TOGLTexture.SetCurrent;
begin
  if self <> nil then
  begin
    glBindTexture(GL_TEXTURE_2D, ObjIdx);
  end
  else
    glBindTexture(GL_TEXTURE_2D, 0);
end;

Destructor TOGLTexture.Destroy;
begin
  glDeleteTextures(1, @ObjIdx);
  Inherited Destroy;
end;

end.
