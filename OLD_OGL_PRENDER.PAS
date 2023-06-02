unit ogl_PRender;

interface
uses Windows, glunit, Prender, Classes, J_Level, Forms,
     Messages, files, FileOperations, graph_files,
     lev_utils, sysUtils, misc_utils, GlobalVars, geometry;

type pCOlorTableExt=Procedure
(target:GLenum;internalformat:GLenum;width:GLsizei;
 format:GLenum;ttype:GLenum; const table);stdcall;

     pColorSubTableExt=Procedure
      (target:GLenum; start:GLsizei; count:GLsizei; format:GLenum;
       ttype:GLenum; const table);stdcall;


TOGLTexture=class
 width,height:integer;
 cmpname:string;
 ObjIdx:GLuint;
 Constructor CreateFromMat(const Mat:string;const Pal:TCMPPal);
 Procedure SetCurrent;
 Destructor Destroy;override;
end;


TOGLPRenderer=class(TPreviewRender)
   TXList,CmpList:TStringList;
   sectors:TSectors;
   things:TThings;
   hdc,hglc:integer;
   hpal:integer;
   masterPalSet:boolean;
   MasterPal:TCMPPal;

   thLists,scLists:TIntList;

 Constructor Create(aForm:TForm);
 {Constructor CreateFromPanel(aPanel:TPanel);}
 Procedure Initialize;override;
 Procedure ClearSectors;override;
 Procedure ClearThings;override;
 Procedure AddSector(s:TJKSector);override;
 Procedure DeleteSector(s:TJKSector);override;
 Procedure UpdateSector(s:TJKSector);override;
 Procedure AddThing(th:TJKThing);override;
 Procedure DeleteThing(th:TJKThing);override;
 Procedure UpdateThing(th:TJKThing);override;
 Procedure SetViewToThing(th:TJKThing);override;

 Procedure SetViewPort(x,y,w,h:integer);override;
 Procedure Redraw;override;
 Destructor Destroy;override;
 Procedure HandleActivate(const msg:TMessage);override;
 procedure HandlePaint(hdc:integer);override;
 Function PickAt(X,Y:integer):TPickType;override;

 Procedure SetGamma(gamma:double);override;
Private
 Procedure ClearTXList;
 Function CreateOGLPalette(const pd:TPIXELFORMATDESCRIPTOR):integer;
 Function LoadOGLTexture(const name,cmp:string):TOGLTexture;
 Procedure LoadMasterPal(l:TJKLevel);
 Function SecToDList(s:TJKSector):Integer;
 Function ThingToDList(th:TJKThing):Integer;
end;


implementation

{function wglGetProcAddress; stdcall;    external opengl32;
Procedure glGenTextures(n:GLsizei; var textures:GLuint); stdcall; external opengl32;
Procedure glDeleteTextures (n:GLsizei; var textures:GLuint); stdcall; external opengl32;}


Const
 GL_COLOR_INDEX8_EXT = $80E5;
 GL_RGB8             = $8051;
 GL_RGB5             = $8050;

var
 pPalProc:pColorTableExt;

{    pfnColorTableEXT = (PFNGLCOLORTABLEEXTPROC)
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
	  GL_RGBA, GL_UNSIGNED_BYTE, YOUR_PALETTE );}

procedure DisableFPUExceptions ;
var
  FPUControlWord: WORD ;
asm
  FSTCW   FPUControlWord ;
  OR      FPUControlWord, $4 + $1 ; { Divide by zero + invalid operation }
  FLDCW   FPUControlWord ;
end ;

Constructor TOGLPRenderer.Create(aForm:TForm);
begin
 Inherited Create(aForm);
 TxList:=TStringList.Create;
 TXList.Sorted:=true;
 CmpList:=TStringList.Create;
 CmpList.Sorted:=true;
 Sectors:=TSectors.Create;
 scLists:=TIntList.Create;
 thLists:=TIntList.Create;
 things:=TThings.Create;
 WHandle:=aForm.Handle;
end;

Destructor TOGLPRenderer.Destroy;
begin
 if (hdc<>0) and (hglc<>0) then
 begin
  wglMakeCurrent(hdc,hglc);
  wglDeleteContext(hglc);
  DeleteDC(hdc);
  ClearTXList;
end;

 if hpal<>0 then DeleteObject(hPal);
 TxList.Free;
 CmpList.Free;
 Sectors.Free;
 things.Free;
 inherited Destroy;
end;

Procedure TOGLPRenderer.Initialize;
var
    pfd : TPIXELFORMATDESCRIPTOR;
    pixelFormat:integer;
    hgl:integer;
begin
 if not InitOpenGL then Raise Exception.Create('Couldn''t initialize OpenGL');
 hdc:=GetDC(WHandle);
 pfd.nsize :=  40;
 pfd.nVersion := 1;
 pfd.dwFlags :=  PFD_DRAW_TO_WINDOW + PFD_SUPPORT_OPENGL+PFD_DOUBLEBUFFER;
 pfd.iPixelType := PFD_TYPE_RGBA;
 pfd.cColorBits := 8; pfd.cRedBits := 0; pfd.cRedShift := 0; pfd.cGreenBits := 0;
 pfd.cGreenShift := 0; pfd.cBlueBits := 0; pfd.cBlueShift := 0; pfd.cAlphaBits := 0;
 pfd.cAlphaShift := 0; pfd.cAccumBits := 0; pfd.cAccumRedBits := 0; pfd.cAccumGreenBits := 0;
 pfd.cAccumBlueBits := 0; pfd.cAccumAlphaBits := 0; pfd.cDepthBits := 32;
 pfd.cStencilBits := 0; pfd.cAuxBuffers := 0; pfd.iLayerType := PFD_MAIN_PLANE;
 pfd.iLayerType := 0;
 pfd.bReserved := 0;
 pfd.dwLayerMask := 0;
 pfd.dwVisiblemask := 0;
 pfd.dwDamageMask := 0;
 pixelFormat := ChoosePixelFormat(HDC, @pfd);
 SetPixelFormat(Hdc, pixelFormat, @pfd);
 DescribePixelFormat(HDC, pixelFormat, sizeof(pfd), pfd);
 Hpal:=CreateOGLPalette(pfd);
 hglc:= wglCreateContext(hdc);
 wglMakeCurrent(hdc,hglc);
 SwapBuffers(hdc);

 glEnable(GL_CULL_FACE);
 glCullFace(GL_BACK);
 glFrontFace(GL_CCW);

 glEnable(GL_DEPTH_TEST);

 glShadeModel(GL_SMOOTH);

 @pPalProc:=wglGetProcAddress('glColorTableEXT');

 if (@pPalProc=nil) then
 begin
  PanMessage(mt_warning,'Your OpenGL implemetation doesn''t support palettized textures!');
 end;

 { glDepthRange(0,1000);
 glCullFace(GL_BACK);
 glEnable(GL_CULL_FACE);
 glFrontFace(GL_CCW);
 glClearColor(1,1,1,1);}

end;

Function TOGLPRenderer.CreateOGLPalette(const pd:TPIXELFORMATDESCRIPTOR):integer;
var ncolors:integer;
    lp:PLogPalette;
    i:integer;
    rrange,grange,brange:byte;
begin
 Result:=0;
 if pd.dwFlags and PFD_NEED_PALETTE = 0 then exit;
 ncolors:=1 shl pd.cColorBits;
 GetMem(lp,sizeof(TLOGPALETTE)+ncolors*sizeof(TPALETTEENTRY));
 lp.palVersion:=$300;
 lp.palNumEntries:=ncolors;

  RRange:=(1 shl pd.cRedBits)-1;
  GRange:=(1 shl pd.cGreenBits)-1;
  BRange:=(1 shl pd.cBlueBits)-1;

  for i:=0 to nColors-1 do
{$R-}
  With lp.palPalEntry[i] do
{$R+}
  begin
   // Fill in the 8-bit equivalents for each component
   peRed:=(i shr pd.cRedShift) and RRange;
   peRed:=Round(peRed * 255.0 / RRange);
   peGreen:=(i shr pd.cGreenShift) and GRange;
   peGreen:=Round(peGreen * 255.0 / GRange);

   peBlue:=(i shr pd.cBlueShift) and BRange;
   peBlue:=Round(peBlue* 255.0 / BRange);

   peFlags:=0;
   end;
 Result:=CreatePalette(lp^);
 SelectPalette(hDC,Result,FALSE);
 RealizePalette(hDC);
 FreeMem(lp);
end;

Procedure TOGLPRenderer.ClearTXList;
var i:integer;
begin
 for i:=0 to TXList.Count-1 do TXList.Objects[i].Free;
 TXList.Clear;
end;

Function Min(i1,i2:integer):integer;
begin
 if i1<i2 then result:=i1 else result:=i2;
end;

Procedure AdjustPalGamma(var pal:TCMPPal;gamma:double);
var i:integer;
begin
 for i:=0 to 255 do
 With Pal[i] do
 begin
  r:=Min(Round(r*gamma),255);
  g:=Min(Round(g*gamma),255);
  b:=Min(Round(b*gamma),255);
 end;
end;

Procedure TOGLPRenderer.SetGamma(gamma:double);
var
   i,j,ic:integer;
   pcmp:^TCMPPal;
   cmp:TCMPPal;
   f:tfile;
   gmult:double;
begin
 self.gamma:=gamma;
 if TXList.Count<>0 then
  PanMessage(mt_warning,'Gamma change will take effect after you reload 3D preview');
{ gmult:=gamma;
 for i:=0 to TXList.count-1 do
 if TXList.Objects[i]<>nil then
 With TOGLTexture(TXList.Objects[i]) do
 begin
  ic:=CmpList.IndexOf(cmpname);
  pcmp:=Pointer(CmpList.Objects[ic]);
  cmp:=pcmp^;
  AdjustPalGamma(cmp,gamma);
  SetCurrent;

  pSubPalProc(GL_TEXTURE_2D, 0, 256, GL_RGB, GL_UNSIGNED_BYTE, cmp );
 end;}
end;

Procedure TOGLPRenderer.ClearSectors;
var i:integer;
begin
 MasterPalSet:=false;
 ClearTXList;
 sectors.Clear;
 for i:=0 to scLists.Count-1 do glDeleteLists(scLists[i],1);
 scLists.Clear;
end;

Procedure TOGLPRenderer.ClearThings;
var i:integer;
begin
 things.Clear;
 for i:=0 to thLists.Count-1 do glDeleteLists(thLists[i],1);
 thLists.Clear;
end;

Procedure TOGLPRenderer.AddSector(s:TJKSector);
var n:integer;
begin
 wglMakeCurrent(hdc,hglc);
 n:=SecToDlist(s);
 scLists.Add(n);
 sectors.Add(s);
end;

Procedure TOGLPRenderer.DeleteSector(s:TJKSector);
var i:integer;
begin
 wglMakeCurrent(hdc,hglc);
 i:=sectors.indexof(s);
 if i<0 then exit;
 glDeleteLists(scLists[i],1);
 Sectors.Delete(i);
 scLists.Delete(i);
end;

Procedure TOGLPRenderer.UpdateSector(s:TJKSector);
var i,n:integer;
begin
 wglMakeCurrent(hdc,hglc);
 i:=sectors.indexof(s);
 if i<0 then exit;
 glDeleteLists(scLists[i],1);
 n:=SecToDList(s);
 scLists[i]:=n;
end;

Procedure TOGLPRenderer.AddThing(th:TJKThing);
var n:integer;
begin
 wglMakeCurrent(hdc,hglc);
 n:=ThingToDlist(th);
 ThLists.Add(n);
 Things.Add(th);
end;

Procedure TOGLPRenderer.DeleteThing(th:TJKThing);
var i:integer;
begin
 wglMakeCurrent(hdc,hglc);
 i:=things.indexof(th);
 if i<0 then exit;
 glDeleteLists(thLists[i],1);
 Things.Delete(i);
 ThLists.Delete(i);
end;

Procedure TOGLPRenderer.UpdateThing(th:TJKThing);
var i,n:integer;
begin
 wglMakeCurrent(hdc,hglc);
 i:=Things.indexof(th);
 if i<0 then exit;
 glDeleteLists(thLists[i],1);
 n:=ThingToDList(th);
 ThLists[i]:=n;
end;

Procedure TOGLPRenderer.SetViewToThing(th:TJKThing);
begin
end;


Procedure TOGLPRenderer.SetViewPort(x,y,w,h:integer);
begin
end;

Procedure TOGLPRenderer.Redraw;
var i:integer;
    v1,v2,v3:Tvector;
begin
 DisableFPUExceptions;
 wglMakeCurrent(hdc,hglc);
 glMatrixMode(GL_Projection);
 glLoadIdentity;
 gluPerspective(90*240/320, 320/240 , 0.05, 5000.0);
 glRotated(-90,1,0,0);
 glRotated(pch,1,0,0);
 glRotated(yaw,0,0,1);
 glTranslated(-CamX,-CamY,-CamZ);
 {glRotated(rol,0,0,1);}

 glClear(GL_COLOR_BUFFER_BIT or GL_DEPTH_BUFFER_BIT);
 for i:=0 to sclists.count-1 do
  glCallList(sclists[i]);

 for i:=0 to thlists.count-1 do
  glCallList(thlists[i]);

{ glColor3f(1,1,1);

 SetVec(v1,-1,1,1);
 SetVec(v2,-1,1,0);
 SetVec(v3,1,1,0);

 glBegin(gl_Triangles);
 With v1 do glVertex3f(dx,dy,dz);
 With v2 do glVertex3f(dx,dy,dz);
 With v3 do glVertex3f(dx,dy,dz);
 glEnd;}

 glFlush;
 SwapBuffers(hdc);
end;

Procedure TOGLPRenderer.HandleActivate(const msg:TMessage);
begin
end;

procedure TOGLPRenderer.HandlePaint(hdc:integer);
begin
end;

Function TOGLPRenderer.PickAt(X,Y:integer):TPickType;
var
    a:TViewPortArray;
    scIdx,sfIdx,thIdx,i,j,v:integer;
    selectBuf:array[1..2048] of GLInt;
    nscs,nv,nvx,hits,names:integer;
    pi:^Integer;
    dist:double;
    farX,farY,farZ:double;
    pmx,mmx:TMatrixDblArray;
    sc:TJKSector;
    surf:TJKSurface;
    lastSfId:integer;


Procedure CheckThing(n:integer);
var cdist:double;
    th:TJKThing;
begin
 th:=Things[n];
 cdist:=sqr(th.x-CamX)+sqr(th.y-CamY)+sqr(th.Z-CamZ);
 if cdist>dist then exit;
 thIdx:=n;
 dist:=cdist;
 sfIdx:=-1;
 scIdx:=-1;
end;

Procedure CheckSurf(nsc,nsf:integer);
var cdist:double;
    surf:TJKSurface;
    vx:TJKVertex;
begin
 vx:=Sectors[nsc].Surfaces[nsf].Vertices[0];
 cdist:=sqr(vx.x-CamX)+sqr(vx.y-CamY)+sqr(vx.Z-CamZ);
 if cdist>dist then exit;
 scIdx:=nsc;
 sfIdx:=nsf;
 thIdx:=-1;
 dist:=cdist;
end;

begin
 Result:=pk_nothing;
 wglMakeCurrent(hdc,hglc);
 DisableFPUExceptions;

try
 glSelectBuffer(sizeof(SelectBuf), @selectBuf);
 glRenderMode (GL_SELECT);

 glGetIntegerV(GL_VIEWPORT,@a);
 glMatrixMode (GL_PROJECTION);
 glLoadIdentity ();
 gluPickMatrix(X,a[3]-Y,2,2,a);



 gluPerspective(85, 1.0 , 0.05, 1000.0);
 glRotated(-90,1,0,0);
 glRotated(pch,1,0,0);
 glRotated(yaw,0,0,1);
 glTranslated(-CamX,-CamY,-CamZ);
 {glRotated(rol,0,0,1);}
 glMatrixMode(GL_ModelView);
 glLoadIdentity;

 glGetDoublev(GL_MODELVIEW_MATRIX,@mmx);
 glGetDoublev(GL_PROJECTION_MATRIX,@pmx);
 gluUnProject(X,a[3]-Y,0,mmx,pmx,a,farX,FarY,FarZ);


 glInitNames();
 glPushName(0);

 nscs:=sectors.count;
 lastSfId:=nscs*1024;
 {Pick faces}

 for i:=0 to sectors.count-1 do
 begin
  sc:=sectors[i];
  for j:=0 to sc.Surfaces.count-1 do
  begin
   surf:=sc.Surfaces[j];
   if (sc.Flags and SF_3DO=0)
   then if (surf.geo=0) or (surf.Material='') then continue;

   nvx:=surf.Vertices.Count;
   if nvx>24 then nvx:=24;

   glLoadName(i*1024+j);

   case nvx of
    3: glBegin(GL_TRIANGLES);
    4: glBegin(GL_QUADS);
    else glBegin(GL_POLYGON);
   end;

   for v:=0 to nvx-1 do
   begin
    if sc.Flags and SF_3DO=0 then nv:=v
    else nv:=nvx-v-1;

    With surf.Vertices[nv] do glVertex3f(x,y,z);
   end;

  glEnd;
  end;
 end;

 for i:=0 to thlists.count-1 do
 begin
  glLoadName(lastSfId+i);
  glCallList(thlists[i]);
 end;


 glFlush;

 hits:=glRenderMode(GL_SELECT);
 dist:=$7fffffff;

 thIdx:=-1;
 scIdx:=-1;
 dist:=999999;


 pi:=@SelectBuf;
 for i:=0 to hits-1 do
 begin
  names:=pi^; inc(pi);
  inc(pi);inc(pi);
  for j:=0 to names-1 do
  begin
   if pi^<LastSfId then CheckSurf(pi^ div 1024,pi^ mod 1024) else
   CheckThing(pi^-LastSfId);
   inc(pi);
  end;
 end;

 if thIdx<>-1 then
 begin
  selTH:=Things[thIdx];
  result:=pk_thing;
 end else
 if scIdx<>-1 then
 begin
  selSC:=Sectors[scIdx];
  SelSF:=sfIdx;
  result:=pk_surface;
 end;

finally
 glRenderMode(GL_RENDER);
end;
end;

Function TOGLPRenderer.LoadOGLTexture(const name,cmp:string):TOGLTexture;
var i:integer;
    Ttx:TOGLTexture;
    pcmp:^TCMPPal;
    tpal:TCMPPal;
    f:TFile;
begin
 Result:=nil;
 i:=TXlist.IndexOf(name+cmp);
 if i<>-1 then
 begin
  Result:=TOGLTexture(TXList.Objects[i]);
  exit;
 end;

 i:=CmpList.IndexOf(cmp);
 if i<>-1 then pcmp:=Pointer(CmpList.Objects[i]) else
 begin
  GetMem(pcmp,sizeof(pcmp^));
  GetLevelPal(Level,pcmp^);
  if not ApplyCMP(cmp,pcmp^) then PanMessage(mt_warning,Format('Cannot load %s',[cmp]));
  CmpList.AddObject(cmp,TObject(pcmp));
 end;

{ LoadMasterPal(level);}
  tpal:=pcmp^;
  AdjustPalGamma(tpal,gamma);

 ttx:=nil;
 try
  ttx:=TOGLTexture.CreateFromMat(name,tpal);
  ttx.cmpname:=cmp;
 finally
  TXList.AddObject(name+cmp,ttx);
  Result:=ttx;
 end;
end;


Procedure TOGLPRenderer.LoadMasterPal(l:TJKLevel);
var
 i:integer;
 pal:array[0..255] of
 record
  r,g,b,a:byte;
 end;

begin
 if MasterPalSet then exit;
 GetLevelPal(l,MasterPal);

 for i:=0 to 255 do
 With Pal[i] do
 begin
  r:=MasterPal[i].r;
  g:=MasterPal[i].g;
  b:=MasterPal[i].b;
  a:=0;
 end;

{ glEnable(GL_COLOR_INDEX8_EXT);

 pPalProc(GL_TEXTURE_2D, GL_RGB8, sizeof(Pal),
	  GL_RGBA, GL_UNSIGNED_BYTE, pal );}

 i:=glGetError;
 if i<>0 then;
 MasterPalSet:=true;
end;

Function TOGLPRenderer.SecToDList(s:TJKSector):Integer;
var i,j:integer;
    nv,nvx:integer;
    ttx:TOGLTexture;
begin
 Result:=glGenLists(1);
 glNewList(Result,GL_COMPILE);
 for i:=0 to s.surfaces.count-1 do
 With s.surfaces[i] do
 begin
  if (s.Flags and SF_3DO=0)
  then if (geo=0) or (Material='') then begin D3DID:=-1; continue; end;

  nvx:=Vertices.Count;
  if nvx>24 then nvx:=24;

  D3DID:=i;
  ttx:=nil;
  Try
   ttx:=LoadOGLTexture(Material,s.ColorMap);
   if ttx<>nil then ttx.SetCurrent;
  except
   on Exception do PanMessage(mt_warning,
      Format('Cannot load %s for sector %d surface %d',[Material,s.num,i]));
  end;

 nvx:=Vertices.Count;
 if nvx>24 then nvx:=24;

 case nvx of
  3: glBegin(GL_TRIANGLES);
  4: glBegin(GL_QUADS);
  else glBegin(GL_POLYGON);
 end;

 for j:=0 to nvx-1 do
 begin
  if s.Flags and SF_3DO=0 then nv:=j
  else nv:=nvx-j-1;

 With Vertices[nv],TXVertices[nv] do
 begin

  if (P3DFullLit) or (s.Flags and SF_3DO<>0) or (SurfFlags and (SFF_SKY or SFF_SKY1)<>0) or
      (light=0)
  then glColor3f(1,1,1) else
  case s.level.mots and P3DColoredLights of
   true: glColor3f(r+s.extra+extralight,g+s.extra+extralight,b+s.extra+extralight);
   false: glColor3f(Intensity+s.extra+extralight,Intensity+s.extra+extralight,Intensity+s.extra+extralight);
  end;

  if ttx<>nil then
     glTexCoord2f(u/ttx.width,v/ttx.height);

  glVertex3f(x,y,z);

 end;
 end;

  glEnd;

 end;
 glEndList;
end;

Function TOGLPRenderer.ThingToDList(th:TJKThing):Integer;
var i,j,k:integer;
    nv,nvx:integer;
    ttx:TOGLTexture;
    cmp,mat:string;
    ax,ay,az:double;
    mx:TMat3x3;
begin
 result:=0;
 if th.a3DO=nil then exit;
 Result:=glGenLists(1);
 glNewList(Result,GL_COMPILE);

 With th do CreateRotMatrix(mx,pch,yaw,rol);

 for i:=0 to th.a3DO.Meshes.count-1 do
 With th.a3Do.Meshes[i] do
 for k:=0 to faces.count-1 do
 With Faces[k] do
 begin
  mat:=th.A3DO.GetMat(imat);
  if (geo=0) or (Mat='') then continue;

  ttx:=nil;

Try
 ttx:=LoadOGLTexture(Mat,'dflt.cmp');
 if ttx<>nil then ttx.SetCurrent;
except
 on Exception do PanMessage(mt_warning,
    Format('Cannot load %s for thing %d',[Mat,th.num]));
end;


{ pim:=ttx.Itexture.GetImage;
 if pim=nil then;}

 nvx:=Vertices.Count;
 if nvx>24 then nvx:=24;

 case nvx of
  3: glBegin(GL_TRIANGLES);
  4: glBegin(GL_QUADS);
  else glBegin(GL_POLYGON);
 end;

 for j:=0 to nvx-1 do
 With Vertices[j],TXVertices[j] do
 begin
   ax:=x; ay:=y; az:=z;
   MultVM3(mx,ax,ay,az);
   glColor3f(1,1,1);

  if ttx<>nil then
     glTexCoord2f(u/ttx.width,v/ttx.height);

  glVertex3f(th.x+ax,th.y+ay,th.z+az);

 end;
 
  glEnd;

 end;
 glEndList;
end;


Constructor TOGLTexture.CreateFromMat(const Mat:string;const Pal:TCMPPal);
var
    i,j:integer;
    pb:pchar;
    mf:TMat;
    f:TFile;
    pl,pline:pchar;
    n:integer;
    bits:Pchar;
    c:char;

    opal:array[0..255] of record r,g,b,a:byte; end;

begin
 f:=OpenGameFile(mat);
 mf:=TMat.Create(f);

 width:=mf.info.width;
 height:=mf.info.Height;
 GetMem(pb,width*height*3);
 bits:=pb;

 GetMem(pl,width);

if @pPalProc<>nil then mf.LoadBits(pb^)
else
begin
 for i:=0 to height-1 do
 begin
  mf.getLine(pl^);

  for j:=0 to width-1 do
  begin
   c:=(pl+j)^;
   With pal[ord(c)] do
   begin
    pb^:=chr(r);
    (pb+1)^:=chr(g);
    (pb+2)^:=chr(b);
   end;
   inc(pb,3);
  end;

 end;
end;


 {for i:=0 to height-1 do
 begin
  mf.GetLine(pb^);
  inc(pb,width);
 end;}

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

 end;   }
 FreeMem(pl);
 mf.free;

 glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
 glGenTextures(1,ObjIdx);
 glBindTexture(GL_TEXTURE_2D,ObjIdx);
 glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
 glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
 glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
 glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_nearest);
 glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_nearest);
{ a:=GL_SPHERE_MAP;
 glTexGeniv(GL_S,GL_TEXTURE_GEN_MODE,@a);
 glTexGeniv(GL_T,GL_TEXTURE_GEN_MODE,@a);
 glTexGeniv(GL_R,GL_TEXTURE_GEN_MODE,@a);
 glTexGeniv(GL_Q,GL_TEXTURE_GEN_MODE,@a);}

 {glTexImage2D(GL_TEXTURE_2D, 0,3,width, height, 0,
   GL_RGB, GL_UNSIGNED_BYTE, bits^);}

 for i:=0 to 255 do
 begin
  opal[i].r:=pal[i].r;
  opal[i].g:=pal[i].g;
  opal[i].b:=pal[i].b;
  opal[i].a:=0;
 end;

  if @pPalProc<>nil then
 begin
  pPalProc(GL_TEXTURE_2D, GL_RGB8, 256,
	  GL_RGBA, GL_UNSIGNED_BYTE, opal );


  glTexImage2D( GL_TEXTURE_2D, 0, GL_COLOR_INDEX8_EXT,
          width, height, 0,
          GL_COLOR_INDEX, GL_UNSIGNED_BYTE,
          bits^ );
 end else
 begin
  glTexImage2D( GL_TEXTURE_2D, 0, 3,
          width, height, 0,
          GL_RGB, GL_UNSIGNED_BYTE,
          bits^ );
 end;


{ pPalProc(GL_TEXTURE_2D, GL_RGB8, 256,
	  GL_RGBA, GL_UNSIGNED_BYTE, opal );


 glTexImage2D( GL_TEXTURE_2D, 0, GL_COLOR_INDEX8_EXT,
          width, height, 0,
          GL_COLOR_INDEX, GL_UNSIGNED_BYTE,
          bits^ );}



 i:=glGetError;
 if i<>0 then;

 glEnable(GL_TEXTURE_2D);
 glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
 glBindTexture(GL_TEXTURE_2D,0);

 FreeMem(bits);

end;


Procedure TOGLTexture.SetCurrent;
var i:integer;
begin
 glBindTexture(GL_TEXTURE_2D,ObjIdx);
end;

Destructor TOGLTexture.Destroy;
begin
 glBindTexture(GL_TEXTURE_2D,0);
 glDeleteTextures(1,ObjIdx);
end;

end.
