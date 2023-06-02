unit tdfx_PRender;

interface
uses gl3dfx, D3d_Prender, Classes, J_Level, Forms,
     Messages,Windows, files, FileOperations, graph_files,
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
 Constructor CreateFromMat(const Mat:string;const Pal:TCMPPal;txid:integer);
 Procedure SetCurrent;
 Destructor Destroy;override;
end;

TVXdets=class
 vx:TVertex;
 u,v,r,g,b:single;
end;

TSurf=class
 vxds:TList;
 tx:TOGLTexture;
 Constructor Create;
 Destructor Destroy;
 Function AddVXD:TVXDets;
 Function GetVXD(n:integer):TVXDets;
end;

TMesh=class
 surfs:TList;
 Constructor Create;
 Destructor Destroy;
 Function GetSurf(n:integer):TSurf;
 Function AddSurf:TSurf;
end;

TMeshes=class(TList)
 Function GetItem(n:integer):TMesh;
 Procedure SetItem(n:integer;v:TMesh);
Property Items[n:integer]:TMesh read GetItem write SetItem; default;
end;


T3DFXPRenderer=class(TPreviewRender)
   TXList,CmpList:TStringList;
   sectors:TSectors;
   things:TThings;
   hdc,hglc:integer;
   hpal:integer;
   masterPalSet:boolean;
   MasterPal:TCMPPal;

   ThList,SCList:TMeshes;

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
 lastTx:integer;
 Function GenTexture:integer;
 Procedure ClearTXList;
 Function CreateOGLPalette(const pd:TPIXELFORMATDESCRIPTOR):integer;
 Function LoadOGLTexture(const name,cmp:string):TOGLTexture;
 Procedure LoadMasterPal(l:TJKLevel);
 Function SecToMesh(s:TJKSector):TMesh;
 Function ThingToMesh(th:TJKThing):TMesh;
 Procedure DrawMesh(m:TMesh);
end;


function wglGetProcAddress(ProcName: PChar): Pointer;  stdcall;

implementation

function wglGetProcAddress; stdcall;    external '3dfxgl.dll';
Procedure glBindTexture (target: GLenum; texture:GLuint); stdcall; external '3dfxgl.dll';
Procedure glGenTextures(n:GLsizei; var textures:GLuint); stdcall; external '3dfxgl.dll';
Procedure glDeleteTextures (n:GLsizei; var textures:GLuint); stdcall; external '3dfxgl.dll';


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

Constructor T3DFXPRenderer.Create(aForm:TForm);
begin
 Inherited Create(aForm);
 TxList:=TStringList.Create;
 TXList.Sorted:=true;
 CmpList:=TStringList.Create;
 CmpList.Sorted:=true;
 Sectors:=TSectors.Create;
 scList:=TMeshes.Create;
 thList:=TMeshes.Create;
 things:=TThings.Create;
 WHandle:=aForm.Handle;
end;

Destructor T3DFXPRenderer.Destroy;
begin
 wglMakeCurrent(hdc,hglc);
 ClearTXList;
 wglDeleteContext(hglc);
 DeleteDC(hdc);

 if hpal<>0 then DeleteObject(hPal);
 TxList.Free;
 CmpList.Free;
 Sectors.Free;
 things.Free;
 inherited Destroy;
end;

Procedure T3DFXPRenderer.Initialize;
var
    pfd : TPIXELFORMATDESCRIPTOR;
    pixelFormat:integer;
    hgl:integer;
begin
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
  hgl:=GetModuleHandle('opengl32.dll');
  if hgl<>0 then @pPalProc:=GetProcAddress(hgl,'glColorTableEXT');
 end;
 
 if (@pPalProc=nil) then raise
  Exception.Create('Your OpenGL implemetation doesn''t support palettized textures!');

 { glDepthRange(0,1000);
 glCullFace(GL_BACK);
 glEnable(GL_CULL_FACE);
 glFrontFace(GL_CCW);
 glClearColor(1,1,1,1);}

end;

Function T3DFXPRenderer.CreateOGLPalette(const pd:TPIXELFORMATDESCRIPTOR):integer;
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

Procedure T3DFXPRenderer.ClearTXList;
var i:integer;
begin
 for i:=0 to TXList.Count-1 do TXList.Objects[i].Free;
 TXList.Clear;
 LastTX:=0;
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

Procedure T3DFXPRenderer.SetGamma(gamma:double);
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

Procedure T3DFXPRenderer.ClearSectors;
var i:integer;
begin
 MasterPalSet:=false;
 ClearTXList;
 sectors.Clear;
 for i:=0 to scList.Count-1 do scList[i].Free;
 scList.Clear;
end;

Procedure T3DFXPRenderer.ClearThings;
var i:integer;
begin
 things.Clear;
 for i:=0 to thList.Count-1 do thList[i].Free;
 thList.Clear;
end;

Procedure T3DFXPRenderer.AddSector(s:TJKSector);
var m:TMesh;
begin
 wglMakeCurrent(hdc,hglc);
 m:=SecToMesh(s);
 scList.Add(m);
 sectors.Add(s);
end;

Procedure T3DFXPRenderer.DeleteSector(s:TJKSector);
var i:integer;
begin
 wglMakeCurrent(hdc,hglc);
 i:=sectors.indexof(s);
 if i<0 then exit;
 scList[i].Free;
 Sectors.Delete(i);
 scList.Delete(i);
end;

Procedure T3DFXPRenderer.UpdateSector(s:TJKSector);
var i,n:integer;
    m:TMesh;
begin
 wglMakeCurrent(hdc,hglc);
 i:=sectors.indexof(s);
 if i<0 then exit;
 scList[i].Free;
 m:=SecToMesh(s);
 scList[i]:=m;
end;

Procedure T3DFXPRenderer.AddThing(th:TJKThing);
var m:TMesh;
begin
 wglMakeCurrent(hdc,hglc);
 m:=ThingToMesh(th);
 ThList.Add(m);
 Things.Add(th);
end;

Procedure T3DFXPRenderer.DeleteThing(th:TJKThing);
var i:integer;
begin
 wglMakeCurrent(hdc,hglc);
 i:=things.indexof(th);
 if i<0 then exit;
 thList[i].Free;
 Things.Delete(i);
 ThList.Delete(i);
end;

Procedure T3DFXPRenderer.UpdateThing(th:TJKThing);
var i,n:integer;
    m:TMesh;
begin
 wglMakeCurrent(hdc,hglc);
 i:=Things.indexof(th);
 if i<0 then exit;
 thList[i].Free;
 m:=ThingToMesh(th);
 ThList[i]:=m;
end;

Procedure T3DFXPRenderer.SetViewToThing(th:TJKThing);
begin
end;


Procedure T3DFXPRenderer.SetViewPort(x,y,w,h:integer);
begin
end;

Procedure T3DFXPRenderer.Redraw;
var i:integer;
    v1,v2,v3:Tvector;
begin
 DisableFPUExceptions;
 wglMakeCurrent(hdc,hglc);
 glMatrixMode(GL_Projection);
 glLoadIdentity;

 glFrustum(-1000,1000,-1000,1000,0.05,1000);

{ gluPerspective(85, 1.0 , 0.05, 1000.0);}
 glRotated(-90,1,0,0);
 glRotated(pch,1,0,0);
 glRotated(yaw,0,0,1);
 glTranslated(-CamX,-CamY,-CamZ);
 {glRotated(rol,0,0,1);}

 glClear(GL_COLOR_BUFFER_BIT or GL_DEPTH_BUFFER_BIT);
 for i:=0 to sclist.count-1 do
  DrawMesh(sclist[i]);

 for i:=0 to thlist.count-1 do
  DrawMesh(thlist[i]);

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

Procedure T3DFXPRenderer.HandleActivate(const msg:TMessage);
begin
end;

procedure T3DFXPRenderer.HandlePaint(hdc:integer);
begin
end;

Function T3DFXPRenderer.PickAt(X,Y:integer):TPickType;
begin
end;

Function T3DFXPRenderer.LoadOGLTexture(const name,cmp:string):TOGLTexture;
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
  ttx:=TOGLTexture.CreateFromMat(name,tpal,GenTexture);
  ttx.cmpname:=cmp;
 finally
  TXList.AddObject(name+cmp,ttx);
  Result:=ttx;
 end;
end;


Procedure T3DFXPRenderer.LoadMasterPal(l:TJKLevel);
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

Function T3DFXPRenderer.SecToMesh(s:TJKSector):TMesh;
var i,j:integer;
    nv,nvx:integer;
    ttx:TOGLTexture;
    surf:TSurf;
    vd:TVXDets;
begin
 Result:=TMesh.Create;
 for i:=0 to s.surfaces.count-1 do
 With s.surfaces[i] do
 begin
  if (s.Flags and SF_3DO=0)
  then if (geo=0) or (Material='') then begin D3DID:=-1; continue; end;

  surf:=Result.AddSurf;

  nvx:=Vertices.Count;
  if nvx>24 then nvx:=24;

  D3DID:=i;
  ttx:=nil;
  Try
   ttx:=LoadOGLTexture(Material,s.ColorMap);
   surf.tx:=ttx;
   {if ttx<>nil then ttx.SetCurrent;}
  except
   on Exception do PanMessage(mt_warning,
      Format('Cannot load %s for sector %d surface %d',[Material,s.num,i]));
  end;

 nvx:=Vertices.Count;
 if nvx>24 then nvx:=24;

 for j:=0 to nvx-1 do
 begin
  if s.Flags and SF_3DO=0 then nv:=j
  else nv:=nvx-j-1;

 With TXVertices[nv] do
 begin
  vd:=surf.AddVXD;

  if (P3DFullLit) or (s.Flags and SF_3DO<>0) or (SurfFlags and (SFF_SKY or SFF_SKY1)<>0)
  then begin vd.r:=1; vd.g:=1; vd.b:=1; end else
  case s.level.mots and P3DColoredLights of
   true: begin vd.r:=r+s.extra+extralight; vd.g:=g+s.extra+extralight; vd.b:=b+s.extra+extralight; end;
   false: begin vd.r:=Intensity+s.extra+extralight; vd.g:=Intensity+s.extra+extralight; vd.b:=Intensity+s.extra+extralight; end;
  end;
  if ttx<>nil then begin vd.u:=u/ttx.width; vd.v:=v/ttx.height; end;
  vd.vx:=Vertices[nv];
 end;
 end;
end;
end;

Function T3DFXPRenderer.ThingToMesh(th:TJKThing):TMesh;
var i,j,k:integer;
    nv,nvx:integer;
    ttx:TOGLTexture;
    cmp,mat:string;
    ax,ay,az:double;
    mx:TMat3x3;
    surf:TSurf;
    vd:TVXDets;
begin
 result:=nil;
 if th.a3DO=nil then exit;
 Result:=TMesh.Create;

 With th do CreateRotMatrix(mx,pch,yaw,rol);

 for i:=0 to th.a3DO.Meshes.count-1 do
 With th.a3Do.Meshes[i] do
 for k:=0 to faces.count-1 do
 With Faces[k] do
 begin
  mat:=th.A3DO.GetMat(imat);
  if (geo=0) or (Mat='') then continue;

  surf:=Result.AddSurf;
  ttx:=nil;

Try
 ttx:=LoadOGLTexture(Mat,'dflt.cmp');
 surf.tx:=ttx;
{ if ttx<>nil then ttx.SetCurrent;}
except
 on Exception do PanMessage(mt_warning,
    Format('Cannot load %s for thing %d',[Mat,th.num]));
end;


{ pim:=ttx.Itexture.GetImage;
 if pim=nil then;}

 nvx:=Vertices.Count;
 if nvx>24 then nvx:=24;

 for j:=0 to nvx-1 do
 With TXVertices[j] do
 begin
   vd:=Surf.AddVXD;
   vd.vx:=Vertices[j];
{   ax:=x;
   MultVM3(mx,ax,ay,az);}

   vd.r:=1; vd.g:=1; vd.b:=1;

  if ttx<>nil then
  begin
   vd.u:=u/ttx.width;
   vd.v:=v/ttx.height;
  end;

 { glVertex3f(th.x+ax,th.y+ay,th.z+az);}

 end;

 end;
end;

Procedure T3DFXPRenderer.DrawMesh(m:TMesh);
var i,j:integer;
    nvx:integer;
begin
for i:=0 to m.Surfs.count-1 do
With m.GetSurf(i) do
begin
 nvx:=vxds.Count;
 if tx<>nil then tx.SetCurrent;
 case nvx of
  3: glBegin(GL_TRIANGLES);
  4: glBegin(GL_QUADS);
  else glBegin(GL_POLYGON);
 end;

 for j:=0 to nvx-1 do
 With GetVXD(j) do
 begin
  glColor3f(r,g,b);

  if tx<>nil then glTexCoord2f(u,v);
  With vx do glVertex3f(x,y,z);

 end;
  glEnd;
 end;

end;


Function T3DFXPRenderer.GenTexture:integer;
begin
 inc(lastTx);
 Result:=lastTX;
end;

Constructor TOGLTexture.CreateFromMat(const Mat:string;const Pal:TCMPPal;txid:integer);
var
    i,j:integer;
    pb:pchar;
    mf:TMat;
    f:TFile;
    pl,pline:pchar;
    n:integer;
    bits:Pchar;

begin
 f:=OpenGameFile(mat);
 mf:=TMat.Create(f);

 width:=mf.info.width;
 height:=mf.info.Height;
 GetMem(pb,width*height*3);
 bits:=pb;

 GetMem(pl,width);

 for i:=0 to height-1 do
 begin
  mf.GetLine(pb^);
  inc(pb,width);
 end;

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
 ObjIdx:=TxId;
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

 pPalProc(GL_TEXTURE_2D, GL_RGB8, 256,
	  GL_RGB, GL_UNSIGNED_BYTE, pal );


 glTexImage2D( GL_TEXTURE_2D, 0, GL_COLOR_INDEX8_EXT,
          width, height, 0,
          GL_COLOR_INDEX, GL_UNSIGNED_BYTE,
          bits^ );



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


Constructor TSurf.Create;
begin
 vxds:=TList.create;
end;

Destructor TSurf.Destroy;
var i:integer;
begin
 for i:=0 to vxds.count-1 do GetVXD(i).Free;
 vxds.Free;
end;

Function TSurf.AddVXD:TVXDets;
begin
 Result:=TVXDets.Create;
 vxds.Add(Result);
end;

Function TSurf.GetVXD(n:integer):TVXDets;
begin
 Result:=TVXDets(vxds.List[n]);
end;

Constructor TMesh.Create;
begin
 Surfs:=TList.Create;
end;

Destructor TMesh.Destroy;
var i:integer;
begin
 for i:=0 to surfs.count-1 do GetSurf(i).Free;
 Surfs.Free;
end;

Function TMesh.AddSurf:TSurf;
begin
 Result:=TSurf.Create;
 surfs.add(Result);
end;


Function TMesh.GetSurf(n:integer):TSurf;
begin
 Result:=TSurf(surfs.List[n]);
end;

Function TMeshes.GetItem(n:integer):TMesh;
begin
 if (n<0) or (n>=count) then raise EListError.CreateFmt('Sector Index is out of bounds: %d',[n]);
 Result:=TMesh(List[n]);
end;

Procedure TMeshes.SetItem(n:integer;v:TMesh);
begin
 if (n<0) or (n>=count) then raise EListError.CreateFmt('Sector Index is out of bounds: %d',[n]);
 List[n]:=v;
end;


end.
