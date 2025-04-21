unit PRenderGL;

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

  TOGLVertex = packed record
    x, y, z: Single;       // Position
    //color: Cardinal;       // Color
    color: TColorF;
     tu, tv, tw, tq: Single; // Texture coordinates with W component
  end;

  TOGLTexture = class(T3DPTexture)
    ObjIdx: GLuint;
    Constructor CreateFromMat(const Mat: string; const ppal: PTCMPPal;
      gamma: double);
    Procedure SetCurrent; { override; }
    Destructor Destroy; override;
  end;

  TOGLPRenderer = class(TNewPRenderer)
    hdc: HDC;
    hpal: integer;

    Destructor Destroy; override;
    Procedure Initialize; override;
    Function IsFogSupported(): Boolean;override;
    Procedure SetFog(color: TColorF; fogStart, fogEnd: double; density: Double = 1.0);override;
    Function LoadTexture(const name: string; const ppal: PTCMPPal; const pcmp: PTCMPTable): T3DPTexture; override;

    Procedure EnableAlphaTest(bEnable: Boolean); override;
    Procedure EnableZTest(enable: Boolean); override;
    Procedure DrawPolys(const [Ref] polys: TArray<T3DPoly>; count: Integer = -1); override;

    Procedure GetWorldLine(X, Y: integer; var X1, Y1, Z1, X2, Y2, Z2: double); override;
    Function ProjectPoint(x, y, z: double; Var ClipX, ClipY, ClipW: double): Boolean; override;

    Procedure SetClearColor(color: TColorF); override;
    Procedure Redraw; override;
  Private
    procedure DrawWiredPolys(const [Ref] polys: TArray<T3DPoly>; count: Integer);
    procedure DrawSolidPolys(const [Ref] polys: TArray<T3DPoly>; count: Integer);

    Procedure SetMatrix;
    Function CreateOGLPalette(const pd: TPIXELFORMATDESCRIPTOR): integer;
    Procedure SetRendererState(const faceflags: longint; bTexture: Boolean);

    var glctx: TGLContext;
    vboId: GLuint;         // Vertex buffer object
    iboId: GLuint;         // Index buffer object
    vertices: TArray<TOGLVertex>;
    indices: TArray<GLushort>;
  end;

implementation
uses System.Math;

var
  ntex: integer = 0;

var
  pPalProc: pCOlorTableExt;

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
  // Delete VBO and IBO
  glDeleteBuffers(1, @vboId);
  glDeleteBuffers(1, @iboId);

  // Delete GL context
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

  hdc := GetDC(WHandle);

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
        // Hide message at this time as HW pal tex is not used atm
//      PanMessage(mt_info,
//        'Your OpenGL implemetation doesn''t support palettized textures!');
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

    // Create VBO and IBO
    glGenBuffers(1, @vboId);
    glGenBuffers(1, @iboId);

    // Initialize vertex and index arrays
    var maxVertices: GLint;
    glGetIntegerv(GL_MAX_ELEMENTS_VERTICES, @maxVertices);

    SetLength(vertices, maxVertices);
    SetLength(indices, maxVertices * 3); // For triangles

    // Allocate the maximum size for VBO and IBO once
    glBindBuffer(GL_ARRAY_BUFFER, vboId);
    glBufferData(GL_ARRAY_BUFFER, Length(vertices) * SizeOf(TOGLVertex), nil, GL_STREAM_DRAW );

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iboId);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, Length(indices) * SizeOf(GLushort), nil, GL_STREAM_DRAW );

    // Unbind buffers
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

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

Procedure TOGLPRenderer.SetRendererState(const faceflags: longint; bTexture: Boolean);
begin
  if BitMaskTest(faceflags, FF_DoubleSided) then
    glDisable(GL_CULL_FACE)
  else
    glEnable(GL_CULL_FACE);

  // Only set texture parameters if a texture is actually bound
  if bTexture then
  begin
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
  end;

//  // Set tex wrap mode
//  var texWrapX := GL_REPEAT;
//  var texWrapY := GL_REPEAT;
//  if BitMaskTest(faceflags, FF_TexClampX) then texWrapX := GL_CLAMP_TO_EDGE;
//  if BitMaskTest(faceflags, FF_TexClampY) then texWrapY := GL_CLAMP_TO_EDGE;
//  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, texWrapX);
//  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, texWrapY);
//
//  // Set tex filtering
//  var magFilter := GL_LINEAR; // Bilinear
//  var minFilter := GL_LINEAR_MIPMAP_LINEAR; // trilinear
//  if Addr(glGenerateMipmap) = nil then
//     minFilter := GL_LINEAR;  // Bilinear
//
//  if BitMaskTest(faceflags, FF_TexNoFiltering) then
//  begin
//    magFilter := GL_NEAREST; // Bilinear
//    minFilter := GL_NEAREST; // trilinear
//  end;
//  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
//  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);

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

//Procedure DrawVertex(vertex: TVXDets; vertexColor: TColorF; setUV, hasAlpha: Boolean);
//begin
//  if hasAlpha then
//    glColor4ub(L2B(vertexColor.r), L2B(vertexColor.g), L2B(vertexColor.b), L2B(vertexColor.a))
//  else
//    glColor3ub(L2B(vertexColor.r), L2B(vertexColor.g), L2B(vertexColor.b));
//
//  if setUV then
//    glTexCoord4f(vertex.u * vertex.w, vertex.v * vertex.w, 0.0, vertex.w);
//
//  // Draw vertices
//  glVertex3f(vertex.X, vertex.Y, vertex.z);
//end;

// Old immidiate mode
//Procedure TOGLPRenderer.DrawPolys(const [Ref] polys: TArray<T3DPoly>; count: Integer);
//var
//  poly: T3DPoly;
//  vxd: TVXdets;
//begin
//  if (count < 0) or (count > Length(polys)) then
//    count := Length(polys);
//
//  if polys = nil then
//    exit;
//
//  var drawMode   := GL_LINE_LOOP;
//  const solidDraw = self.geoMode > Wireframe;
//  if solidDraw then drawMode := GL_TRIANGLES;
//
//  for var i := 0 to count - 1 do
//  begin
//    poly := polys[i];
//    if poly = nil then continue;
//
//    var tex: T3DPTexture := nil;
//    var fflags := 0;
//    if solidDraw then
//      begin
//        fflags := poly.faceflags;
//        if poly.geo = Texture then
//          tex := poly.tx;
//      end;
//
//    TOGLTexture(tex).SetCurrent;
//    SetRendererState(fflags);
//
//    var setUV    := solidDraw and (tex <> nil);
//    var hasAlpha := solidDraw and BitMaskTest(poly.faceflags, FF_Transluent);
//
//    glBegin(drawMode);
//      if (poly.vxds.count <= 3) or not solidDraw then
//        begin
//            for var j := 0 to poly.vxds.count - 1 do
//              begin
//                vxd := poly.getVXD(j);
//                var vcolor := wfColor;
//                if solidDraw then
//                  vcolor := poly.GetLitColor(vxd.intensity);
//                DrawVertex(vxd, vcolor, setUV, hasAlpha);
//              end;
//        end
//      else
//        begin
//          const totalTris = poly.vxds.count - 2;
//          var vertIdx0 := 0;
//          var vertIdx1 := 1;
//          var vertIdx2 := poly.vxds.count - 1;
//          for var idx := 0 to totalTris do
//            begin
//              vxd := poly.getVXD(vertIdx0);
//              var vcolor := wfColor;
//              if solidDraw then
//                vcolor := poly.GetLitColor(vxd.intensity);
//              DrawVertex(vxd, vcolor, setUV, hasAlpha);
//
//              vxd := poly.getVXD(vertIdx1);
//              if solidDraw then
//                vcolor := poly.GetLitColor(vxd.intensity);
//              DrawVertex(vxd, vcolor, setUV, hasAlpha);
//
//              vxd := poly.getVXD(vertIdx2);
//              if solidDraw then
//                vcolor := poly.GetLitColor(vxd.intensity);
//              DrawVertex(vxd, vcolor, setUV, hasAlpha);
//
//              if (idx and 1) = 0 then //((not idx) and 1) <> 0 then  i.e.: if even
//                begin
//                  vertIdx0 := vertIdx1;
//                  vertIdx1 := vertIdx1 + 1;
//                end
//              else // if idx is odd
//                begin
//                  vertIdx0 := vertIdx2;
//                  vertIdx2 := vertIdx2 - 1;
//                end;
//            end;
//        end;
//    glEnd;
//  end;
//end;


procedure TOGLPRenderer.DrawPolys(const [Ref] polys: TArray<T3DPoly>; count: Integer);
begin
  if polys = nil then exit;

  if (count < 0) or (count > Length(polys)) then
    count := Length(polys);

  if geoMode <= Wireframe then
    DrawWiredPolys(polys, count)
  else
    DrawSolidPolys(polys, count);
end;

procedure TOGLPRenderer.DrawWiredPolys(const [Ref] polys: TArray<T3DPoly>; count: Integer);
var
  maxVerts: Integer;
  numVerts: Integer;
  tempIndices: array[0..1] of GLushort;
begin
  maxVerts := Length(vertices);

  // Note, don't change the order
  TOGLTexture(nil).SetCurrent;
  SetRendererState(0, False);

  for var i := 0 to count-1 do
  begin
    numVerts := polys[i].vxds.count;
    if numVerts > maxVerts then
    begin
      PanMessageFmt(mt_warning, 'TOGLPRenderer.RenderWiredPolys: Polygon has more vertices [%d] than max vertices [%d]!', [numVerts, maxVerts]);
      numVerts := maxVerts;
    end;

    // Fill vertex data
    for var j := 0 to numVerts-1 do
    begin
      var vxd := polys[i].getVXD(j);
      with vertices[j] do
      begin
        x := vxd.x;
        y := vxd.y;
        z := vxd.z;
        color := wfColor;//EncodeARGB(wfColor);
        tu := 0;
        tv := 0;
      end;
    end;

    // Bind vertex buffer and upload data
    glBindBuffer(GL_ARRAY_BUFFER, vboId);
    glBufferSubData(GL_ARRAY_BUFFER, 0, numVerts * SizeOf(TOGLVertex), @vertices[0]);

    // Set up vertex arrays
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);

    // Specify vertex data layout
    glVertexPointer(3, GL_FLOAT, SizeOf(TOGLVertex), Pointer(0));
    glColorPointer(4,  GL_FLOAT, SizeOf(TOGLVertex), Pointer(SizeOf(Single)*3));

    if geoMode = Vertex then
    begin
      // Draw points
      glDrawArrays(GL_POINTS, 0, numVerts);
    end
    else
    begin
      // Draw lines
      glDrawArrays(GL_LINE_STRIP, 0, numVerts);

      // Draw closing line
      tempIndices[0] := numVerts - 1;
      tempIndices[1] := 0;

      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iboId);
      glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, 2 * SizeOf(GLushort), @tempIndices[0]);
      glDrawElements(GL_LINES, 2, GL_UNSIGNED_SHORT, nil);
    end;

    // Clean up
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
  end;

  // Unbind buffers
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
end;

Function IsEqualTex(const tex1, tex2: T3DPTexture): Boolean;
begin
  Result := False;
  if (tex1 = tex2) then
    Result := True
  else if (tex1 <> nil) and (tex2 <> nil) then
    Result := (tex1.name = tex2.name);
end;

procedure TOGLPRenderer.DrawSolidPolys(const [Ref] polys: TArray<T3DPoly>; count: Integer);

begin
  const maxVerts = Length(vertices);

  // Enable client states
  glEnableClientState(GL_VERTEX_ARRAY);
  glEnableClientState(GL_COLOR_ARRAY);
  glEnableClientState(GL_TEXTURE_COORD_ARRAY);

  // Bind buffers
  glBindBuffer(GL_ARRAY_BUFFER, vboId);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iboId);

  try
    var i := 0;
    while i < count do
    begin
      var poly := polys[i];
      if (poly = nil) or (poly.geo = NotDrawn) then
      begin
        i := i + 1;
        continue;
      end;

      var curFaceFlags := poly.faceflags;
      var curGeo       := poly.geo;
      var curTex       := poly.tx;
      if curGeo <> Texture then
        curTex := nil;

      // Set texture and render state
      // Note, don't switch the order as state is bind to texture
      TOGLTexture(curTex).SetCurrent;
      SetRendererState(curFaceFlags, curTex <> nil);

      var totalVerts := 0;
      var totalIdxs  := 0;

      // Batch as many polygons as possible
      while i < count do
      begin

        poly := polys[i];
        var numVerts := poly.vxds.Count;
        if numVerts > maxVerts then
        begin
          PanMessageFmt(mt_warning, 'TOGLPRenderer.RenderSolidPolys: Polygon has more vertices [%d] than max vertices [%d]!', [numVerts, maxVerts]);
          numVerts := maxVerts;
        end;

        // Fill vertex data
        for var j := 0 to numVerts-1 do
        begin
          var vxd := poly.getVXD(j);
          with vertices[totalVerts + j] do
          begin
            x := vxd.x;
            y := vxd.y;
            z := vxd.z;

            tu := vxd.u * vxd.w;
            tv := vxd.v * vxd.w;
            tw := 0;
            tq := vxd.w;

            var lcolor := poly.GetLitColor(vxd.intensity);
            if BitMaskTest(poly.faceflags, FF_Transluent) then
              color := lcolor//EncodeRGB(lcolor) // TODO should encode RGB
            else
              color := lcolor;//EncodeRGB(lcolor);
          end;
        end;

        // Triangulation
        if numVerts <= 3 then
        begin
          indices[totalIdxs]     := totalVerts;
          indices[totalIdxs + 1] := totalVerts + 1;
          indices[totalIdxs + 2] := totalVerts + 2;
          totalIdxs := totalIdxs + 3;
        end
        else
        begin
          const totalTris = numVerts - 2;
          var ofsTriVert0 := 0;
          var ofsTriVert1 := 1;
          var ofsTriVert2 := numVerts - 1;
          for var idx := 0 to totalTris do
          begin
            indices[totalIdxs]     := totalVerts + ofsTriVert0;
            indices[totalIdxs + 1] := totalVerts + ofsTriVert1;
            indices[totalIdxs + 2] := totalVerts + ofsTriVert2;
            totalIdxs := totalIdxs + 3;
            if (idx and 1) = 0 then
            begin
              ofsTriVert0 := ofsTriVert1;
              ofsTriVert1 := ofsTriVert1 + 1;
            end
            else
            begin
              ofsTriVert0 := ofsTriVert2;
              ofsTriVert2 := ofsTriVert2 - 1;
            end;
          end;
        end;

        totalVerts := totalVerts + numVerts;

        // Check if we can add the next polygon to the batch
        if (i + 1) < count then
        begin
          var nextPoly := polys[i + 1];
          if ((3 * (numVerts - 2) + totalIdxs) >= maxVerts) // i.e. totalIndices + num required triangle indices for next poly >= maxVerts
          or (nextPoly.faceflags <> curFaceFlags)
          or ((nextPoly.vxds.Count + totalVerts) >= maxVerts)
          or (nextPoly.geo <> curGeo)
          or (not IsEqualTex(nextPoly.tx, curTex)) then
            break;
        end;

        i := i + 1;
      end;

      // Upload vertex data
      glBufferSubData(GL_ARRAY_BUFFER, 0, totalVerts * SizeOf(TOGLVertex), @vertices[0]);

      // Upload indicesdata
      glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, totalIdxs * SizeOf(GLushort), @indices[0]);

      // Specify vertex data layout
      glVertexPointer(3, GL_FLOAT, SizeOf(TOGLVertex), Pointer(0));
      // glColorPointer(4, GL_UNSIGNED_BYTE, SizeOf(TOGLVertex), Pointer(12));
      glColorPointer(4,  GL_FLOAT, SizeOf(TOGLVertex), Pointer(SizeOf(Single)*3));
      glTexCoordPointer(4, GL_FLOAT, SizeOf(TOGLVertex), Pointer(SizeOf(Single)*3 + SizeOf(TColorF)));

      // Draw triangles
      glDrawElements(GL_TRIANGLES, totalIdxs, GL_UNSIGNED_SHORT, nil);

      i := i + 1;
    end;

  finally
    // Unbind buffers
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    // Cleanup client states
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
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
    inherited Redraw;

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
  finally
    glctx.Release();
  end;
end;

Constructor TOGLTexture.CreateFromMat(const Mat: string; const ppal: PTCMPPal; gamma: double);
var
  i, j: integer;
  pb: PByte;
  mf: TMat;
  f: TFile;
  n: integer;
  bits: PByte;
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

      if usepalette then
        mf.LoadBits(pb^)
      else
        LoadRGB;

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

      if usepalette then
        begin
            for i := 0 to 255 do
            begin
              opal[i].r := ppal[i].r;
              opal[i].g := ppal[i].g;
              opal[i].b := ppal[i].b;
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

      i := glGetError;
      if i <> 0 then;  // TODO: log error?

      glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
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
      glEnable(GL_TEXTURE_2D);
      glBindTexture(GL_TEXTURE_2D, ObjIdx);
    end
  else
    begin
      // No texture for this polygon
      glDisable(GL_TEXTURE_2D);
      //glBindTexture(GL_TEXTURE_2D, 0);
    end;
end;

Destructor TOGLTexture.Destroy;
begin
  glDeleteTextures(1, @ObjIdx);
  Inherited Destroy;
end;

end.
