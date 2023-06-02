unit d3d_NPRender;

{$O-}

interface
uses Windows, glunit, Prender, Classes, J_Level, Forms,
     Messages, files, FileOperations, graph_files,
     lev_utils, sysUtils, misc_utils, GlobalVars, geometry,
     D3DRMObj, D3DTypes, D3DRMDef, DDraw, D3D, D3Drm, dxtools,
     D3DrmWin, D3Dcaps, graphics;

type

TD3D5PRenderer=class(TNewPRenderer)
 hardware:boolean;
 ramp:boolean;
 idd2:IDirectDraw2;
 id3d2:IDirect3D2;
 id3ddev:IDirect3DDevice2;
 iview:IDirect3DViewPort2;
 ids:IDirectDrawSurface;
 idsback:IDirectDrawSurface;
 idz:IDirectDrawSurface;
 PalettedTextures:boolean;
 ipal:IDirectDrawPalette;

 irmat:idirect3dmaterial2;

 Destructor Destroy;override;
 Procedure Initialize;override;
 Function LoadTexture(const name:string;const pal:TCMPPal;const cmp:TCMPTable):T3DPTexture;override;
 Procedure DrawMesh(m:T3DPMesh);override;
 Procedure GetWorldLine(X,Y:integer;var X1,Y1,Z1,X2,Y2,Z2:double);override;

 Procedure Redraw;override;
Private
 Function GetD3DPalette(const pal:TCMPPal):IDirectDrawPalette;
end;


TD3DTexture=class(T3DPTexture)
 itx:IDirect3DTexture2;
 itxs:IDirectDrawSurface;
 dxr:TD3D5PRenderer;
 htx:integer;
 Constructor CreateFromMat(const Mat:string;const pal:TCMPPal;const cmp:TCMPTable;adxr:TD3D5PRenderer;gamma:double);
 Procedure SetCurrent;{override;}
 Destructor Destroy;override;
end;


implementation
uses D3D_PRender;

var curdxr:TD3D5PRenderer;

Function D3DVECTOR(dx,dy,dz:single):TD3DVector;
begin
 Result.x:=dx;
 Result.y:=dy;
 Result.z:=dz;
end;

function _EnumTXFormats(const ddsd: TDDSURFACEDESC; arg:pointer): HRESULT ; stdcall ;
begin
 result:=D3DENUMRET_OK;
 if (ddsd.ddpfPixelFormat.dwFlags and DDPF_PALETTEINDEXED8)<>0
 then
 begin {Found paletted texture}
  Boolean(arg^):=true;
  Result:=D3DENUMRET_CANCEL;
 end;

end;

Function GetZBufferBits(flags:integer):integer;
begin
 if Flags and DDBD_16<>0 then begin result:=16; exit; end;
 if Flags and DDBD_24<>0 then begin result:=24; exit; end;
 if Flags and DDBD_8<>0 then begin result:=8; exit; end;
 if Flags and DDBD_32<>0 then begin result:=32; exit; end;
end;


Procedure TD3D5PRenderer.Initialize;
var res:HResult;
    idd:IDirectDraw;
    ddsd:TDDSURFACEDESC;

    iclp:IDirectDrawClipper;
    dview:TD3DVIEWPORT;
    rc:TRect;
    ndevice:integer;
    material:td3dmaterial;
    imat:idirect3dmaterial2;
    hmat:td3dmaterialhandle;
    pdd:^TD3DDeviceDesc;
begin
 EnumDevices;
 if not DirectX5 then Raise
 Exception.Create('DirectX 5.0 or higher required to use D3D IM renderer. Change to RM renderer in Options');

 ndevice:=GetDeviceNum(D3DDevice);
 hardware:=_D3Ddrivers[ndevice].HWDeviceDesc.dcmColorModel<>D3DCOLOR_INVALID_0;

 if hardware then pdd:=@_D3Ddrivers[ndevice].HWDeviceDesc
 else pdd:=@_D3Ddrivers[ndevice].HELDeviceDesc;

 ramp:=pdd^.dcmColorModel=D3DCOLOR_MONO;

 if Ramp then Raise
 Exception.Create('Ramp device cannot be used with D3D IM renderer');

 if (DirectDrawCreate(NIL,idd,NIL)<>DD_OK) then exit;

 if (idd.QueryInterface(IID_IDirectDraw2, iDD2)<> S_OK) then exit;
 res:=iDD2.SetCooperativeLevel(WHandle,DDSCL_NORMAL);
 DDCheck(res,'in SetCooperativeLevel');

 res:=idd2.QueryInterface(IID_IDirect3D2, id3d2);

 DXFailCheck(res,'Getting Direct3D2 inteface');

 {Enumerate Z buffer formats - skipped}


 {Create Windowed }
 GetClientRect(whandle,rc);



 {Create front buffer}
 FillChar(ddsd,sizeof(ddsd),0);
 ddsd.dwSize := sizeof(ddsd);
 ddsd.dwFlags := DDSD_CAPS;

 {if (hardware) then } ddsd.ddsCaps.dwCaps:=DDSCAPS_PRIMARYSURFACE;
// else ddsd.ddsCaps.dwCaps:=DDSCAPS_PRIMARYSURFACE or DDSCAPS_SYSTEMMEMORY;

 res := iDD2.CreateSurface(ddsd,ids, NIL);
 DDFailCheck(res,'Creating Primary Surface');

 {Create Clipper}

 res:= iDD2.CreateClipper(0, iClp, NIL);
 DDFailCheck(res,'Creating clipper');

 res:=iclp.SetHWnd(0,whandle);
 DDFailCheck(res,'Setting clipper''s window');

 res:=ids.SetClipper(iClp);
 DDFailCheck(res,'Setting clipper');

 iclp.Release;


 {Create back buffer}


 ddsd.dwSize := sizeof(ddsd);
 res:=idd2.GetDisplayMode(ddsd);

 ddsd.dwFlags := DDSD_CAPS or DDSD_HEIGHT or DDSD_WIDTH or DDSD_PIXELFORMAT;

 if (hardware) then ddsd.ddsCaps.dwCaps := DDSCAPS_OFFSCREENPLAIN or DDSCAPS_3DDEVICE or DDSCAPS_VIDEOMEMORY
 else ddsd.ddsCaps.dwCaps := DDSCAPS_OFFSCREENPLAIN or DDSCAPS_3DDEVICE or DDSCAPS_SYSTEMMEMORY;

 ddsd.dwWidth := vwidth;
 ddsd.dwHeight := vheight;

 res:=iDD2.CreateSurface(ddsd, idsback, NIL);
 DDFailCheck(res,'Creating back buffer');

 {res:=ids.AddAttachedSurface(idsback);}


 {Create Z buffer - skipped}
 FillChar(ddsd,sizeof(ddsd),0);
 ddsd.dwSize := sizeof(ddsd);
 ddsd.dwFlags := DDSD_CAPS or DDSD_HEIGHT or DDSD_WIDTH or DDSD_ZBUFFERBITDEPTH;

 if (hardware) then ddsd.ddsCaps.dwCaps := DDSCAPS_ZBUFFER or DDSCAPS_VIDEOMEMORY
 else ddsd.ddsCaps.dwCaps := DDSCAPS_ZBUFFER or DDSCAPS_SYSTEMMEMORY;

 ddsd.dwZBufferBitDepth:=GetZBufferBits(pdd^.dwDeviceZBufferBitDepth);

 ddsd.dwWidth := vwidth;
 ddsd.dwHeight := vheight;

 res:=iDD2.CreateSurface(ddsd, idz, NIL);
 DDFailCheck(res,'Creating Z buffer');

 res:=idsback.AddAttachedSurface( idz);
 DDFailCheck(res,'Attaching Z buffer');

  {res:=ids.QueryInterface(_D3Ddrivers[0].id,iD3DDev);}
 res:=iD3D2.CreateDevice(_D3Ddrivers[ndevice].id, idsback, id3dDev);
 DXFailCheck(res,'Creating device');

 {Check if supports paletted textures }
 res:=id3ddev.EnumTextureFormats(_EnumTXFormats, @PalettedTextures);
 DXCheck(res,'Checking texture formats');

 res:=id3d2.CreateViewport(iview,NIL);
 DXFailCheck(res,'Creating viewport');
 res:=id3ddev.AddViewport(iview);
 DXFailCheck(res,'Adding viewport');

  FillChar(dview,sizeof(dview),0);
  With dview do
  begin
     dwSize := sizeof(TD3DVIEWPORT);
     dwX := 0;
     dwY := 0;
     dwWidth := vWidth;
     dwHeight := vHeight;
     dvScaleX := dwWidth / 2.0;
     dvScaleY := dwWidth / 2.0;
     dvMaxX := D3DDivide(D3DVAL(dwWidth),D3DVAL(2 *dvScaleX));
     dvMaxY := D3DDivide(D3DVAL(dwHeight),D3DVAL(2 *dvScaleY));
  end;
  res := iview.SetViewport(dview);
  DXFailCheck(res,'Setting viewport');
  res:=id3ddev.SetCurrentViewport(iview);
  DXFailCheck(res,'Setting current viewport');

 FillChar(material,sizeof(material),0);
 material.dwsize := sizeof(material);
 material.dcvdiffuse.r := 0;
 material.dcvdiffuse.g := 0;
 material.dcvdiffuse.b := 0;
 material.dcvambient.r := 0;
 material.dcvambient.g := 0;
 material.dcvambient.b := 0;
 material.dcvspecular.r := 0;
 material.dcvspecular.g := 0;
 material.dcvspecular.b := 0;
 material.dvpower := 0;
 material.dwrampsize := 1;

 res:=id3d2.creatematerial(imat,nil);
 DXCheck(res,'Creating material');

 res:=imat.setmaterial(material);
 DXCheck(res,'Setting material');

 res:=imat.gethandle(id3ddev,hmat);
 DXCheck(res,'Getting material handle');

 res:=IView.SetBackGround(hmat);
 DXCheck(res,'Setting background');


 Id3dDev.SetRenderState(D3DRENDERSTATE_TEXTUREPERSPECTIVE,Integer(TRUE));
 Id3dDev.SetRenderState(D3DRENDERSTATE_CULLMODE,Integer(D3DCULL_CW));
{ Id3dDev.SetRenderState(D3DRENDERSTATE_ZENABLE,Integer(true));
 Id3dDev.SetRenderState(D3DRENDERSTATE_ZWRITEENABLE,Integer(true));}

{if ramp then
begin
 FillChar(material,sizeof(material),0);
 material.dwsize := sizeof(material);
 material.dcvdiffuse.r := 0;
 material.dcvdiffuse.g := 0;
 material.dcvdiffuse.b := 0;
 material.dcvambient.r := 0;
 material.dcvambient.g := 0;
 material.dcvambient.b := 0;
 material.dcvspecular.r := 0;
 material.dcvspecular.g := 0;
 material.dcvspecular.b := 0;
 material.dvpower := 0;
 material.dwrampsize := 1;

 res:=id3d2.creatematerial(irmat,nil);
 DXCheck(res,'Creating material');

 res:=irmat.setmaterial(material);
 DXCheck(res,'Setting material');
end;}

end;

procedure FreeUnk(Iu:IUnknown);
var n:integer;
begin
 if iu<>nil then
 begin
  n:=iu.Release;
  if n<>0 then
  begin
   if (n=1) then;
  end;
 end;
end;

Destructor TD3D5PRenderer.Destroy;
begin
 FreeUnk(idsback);
 FreeUnk(ids);
 FreeUnk(idz);
 FreeUnk(iview);
 FreeUnk(id3ddev);
 FreeUnk(id3d2);
 FreeUnk(idd2);
 FreeUnk(irmat);
 Inherited destroy;
end;

Function TD3D5PRenderer.GetD3DPalette(const pal:TCMPPal):IDirectDrawPalette;
var i,res:Integer;
    wpal:array[0..255] of TPALETTEENTRY;
begin
 if TXList.count=0 then
 begin
  if ipal<>nil then begin ipal.release; ipal:=nil; end; 
 end;

 if ipal=nil then
 begin
  for i:=0 to 255 do
  begin
   wpal[i].peRed:=pal[i].r;
   wpal[i].peGreen:=pal[i].g;
   wpal[i].peBlue:=pal[i].b;
   wpal[i].peFlags:=0;
  end;
  res:=idd2.CreatePalette(DDPCAPS_8BIT, @wpal,ipal,nil);
  DDCheck(res,'Creating palette');
 end;
 result:=ipal;
end;

Function TD3D5PRenderer.LoadTexture(const name:string;const pal:TCMPPal;const cmp:TCMPTable):T3DPTexture;
var i:integer;
    Ttx:TD3DTexture;
begin
 Result:=nil;
 ttx:=TD3DTexture.CreateFromMat(name,pal,cmp,self,gamma);
 Result:=ttx;
end;

Procedure TD3D5PRenderer.DrawMesh(m:T3DPMesh);
var i,j:integer;
    surf:T3DPSurf;
    vxd:TVXdets;
    d3dvx:array[0..23] of TD3DLVERTEX;
    res,n:integer;
begin
 if m=nil then exit;
 for i:=0 to m.surfs.count-1 do
 begin
  surf:=m.GetSurf(i);
  TD3Dtexture(surf.tx).SetCurrent;
  n:=surf.vxds.count;
  if n>24 then n:=24;

  for j:=0 to n-1 do
  With d3dvx[j] do
  begin
   vxd:=surf.getVXD(j);
   x:=vxd.x;
   y:=vxd.y;
   z:=vxd.z;
   color:=vxd.r shl 16+vxd.g shl 8+vxd.b;
   dcSpecular:=0;
   tu:=vxd.u;
   tv:=vxd.v;
  end;

  res:=id3ddev.DrawPrimitive(D3DPT_TRIANGLEFAN,D3DVT_LVERTEX,@d3dvx,n,0);
  DXCheck(res,'Drawing triangles');
 end;
end;

Procedure TD3D5PRenderer.GetWorldLine(X,Y:integer;var X1,Y1,Z1,X2,Y2,Z2:double);
var xv,yv,zv:TVector;
    px,py:double;
    vec:TVector;
begin
 SetVec(yv,0,1,0);
 RotateVector(yv,-PCH,0,0);
 RotateVector(yv,0,-YAW,0);
 SetVec(zv,0,0,1);
 RotateVector(zv,-PCH,0,0);
 RotateVector(zv,0,-YAW,0);
 SetVec(xv,1,0,0);
 RotateVector(xv,-PCH,0,0);
 RotateVector(xv,0,-YAW,0);

 {The projection rectangle is 0.1x0.075 units and 0.05 units away
  from camera}

 px:=(X-vwidth/2)/vwidth*0.1;
 py:=(vheight/2-y)/vheight*0.075;


 X1:=CamX+px*xv.dx+py*zv.dx+0.05*yv.dx;
 Y1:=CamY+px*xv.dy+py*zv.dy+0.05*yv.dy;
 Z1:=CamZ+px*xv.dz+py*zv.dz+0.05*yv.dz;

PlaneLineXnNew(yv,CamX+yv.dx*5000,CamX+yv.dy*5000,CamX+yv.dz*5000,
               CamX,CamY,CamZ,x1,y1,z1,x2,y2,z2);

{

 X2:=CamX+px*xv.dx+py*zv.dx+5000*yv.dx;
 Y2:=CamY+px*xv.dy+py*zv.dy+5000*yv.dy;
 Z2:=CamZ+px*xv.dz+py*zv.dz+5000*yv.dz;}
end;

Procedure TD3D5PRenderer.Redraw;
var bltfx:TDDBLTFX;
    res:Integer;
    r,sr:TRect;
    m_proj,m_view,m_world:TD3dMatrix;
    vec,zvec:TVector;
    rec:TD3DRECT;
begin
 curdxr:=self;
 DisableFPUExceptions;

 FillChar(bltfx,sizeof(bltfx),0);
 bltfx.dwSize := sizeof(bltfx);

 {res:=idsback.Blt(NIL,NIL,NIL,DDBLT_WAIT or DDBLT_COLORFILL,bltfx);}

 rec.x1:=0;
 rec.y1:=0;
 rec.x2:=vwidth;
 rec.y2:=vheight;


 res := iview.clear(1,rec,d3dclear_target or d3dclear_zbuffer);
 DXCheck(res,'Clearing viewport');

 
 m_World := IdentityMatrix;
 m_World._11:=-1;

 {MatrixMul(TranslateMatrix(CamX,CamY,CamZ),RotateXmatrix(-pi/2));}

 SetVec(vec,0,1,0);
 RotateVector(vec,-PCH,0,0);
 RotateVector(vec,0,-YAW,0);


{ RotateVector(zvec,-PCH,0,0);
 RotateVector(zvec,0,-YAW,0);}

 SetVec(zvec,0,0,1);
 if Abs(vec.dz)>0.99 then
 begin
  Vmult(1,0,0,
        vec.dx,vec.dy,vec.dz,
        zvec.dx,zvec.dy,zvec.dz);
  RotateVector(zvec,0,YAW,0);
 end;

 m_View := ViewMatrix(D3DVECTOR(-CamX,CamY,CamZ),
 D3DVECTOR(-CamX-vec.dx,CamY+vec.dy,CamZ+vec.dz), D3DVECTOR(zvec.dx,zvec.dy,zvec.dz), 0);

 m_Proj := ProjectionMatrix(0.05, 5000, (90*PI/180)); // 60 degree FOV}

 Id3dDev.SetTransform(D3DTRANSFORMSTATE_WORLD, m_World);
 Id3dDev.SetTransform(D3DTRANSFORMSTATE_VIEW, m_View);
 Id3dDev.SetTransform(D3DTRANSFORMSTATE_PROJECTION, m_Proj);

 res:=id3ddev.BeginScene;
 DXCheck(res,'Beginnning scene');

 inherited Redraw;
 res:=id3ddev.EndScene;
 DXCheck(res,'Ending scene');

 GetClientRect(whandle,sr);
{ r:=sr;}
 
 ClientToScreen(whandle,sr.topleft);
 ClientToScreen(whandle,sr.bottomright);


 {r.left:=0; r.top:=0;
 r.bottom:=vheight-1; r.right:=vwidth-1;}


 res:=ids.Blt( @sr, idsback, nil, DDBLT_WAIT,bltfx);
 if res=DDERR_SURFACELOST then
 begin
  ids.Restore;
  idsback.Restore;
 end;
 DDCheck(res,'Blitting from back to front');

{ res:=ids.BltFast(sr.left,sr.top, idsback, Trect(nil^), DDBLTFAST_WAIT or DDBLTFAST_NOCOLORKEY);}

end;

{Texture}

Function Min(i1,i2:integer):integer;
begin
 if i1<i2 then result:=i1 else result:=i2;
end;


Constructor TD3DTexture.CreateFromMat(const Mat:string;const pal:TCMPPal;const cmp:TCMPTable;adxr:TD3D5PRenderer;gamma:double);
var
    i,j:integer;
    pb:pchar;
    mf:TMat;
    f:TFile;
    pl,pline:pchar;
    res, n:integer;
{    bits:Pchar;}
    c:char;
    w:word;
    usepalette:boolean;
 var ddsd:TDDSURFACEDESC;
     ads:IDirectDrawSurface;
     bltfx:TDDBLTFX;
Procedure LoadPaletted;
var i,j:integer;
begin
 for i:=0 to height-1 do
 begin
  mf.getLine(pl^);

  for j:=0 to width-1 do
  begin
   (pl+j)^:=Chr(cmp[ord((pl+j)^)]);
  end;
  Move(pl^,pb^,width);
  inc(pb,ddsd.lPitch);
 end;
end;

Procedure LoadRGB;
var bpp:integer;
    rsh,gsh,bsh:integer;
    rpsh,gpsh,bpsh:integer;
    i,j:integer;
    pw:^word;
begin

 if mf.info.storedAs=byLines16 then
 begin
 for i:=0 to height-1 do
 begin
  mf.getLine(pb^);
  pw:=pointer(pb);
  for j:=0 to width-1 do
  begin
   pw^:=min(Round((pw^ shr 11 and $1F)*gamma),31) shl 11+
        min(Round((pw^ shr 5 and $3F)*gamma),63) shl 5+
        min(Round((pw^ and $1F)*gamma),31){ shl 11};
   inc(pw);
  end;

  inc(pb,ddsd.lPitch);
 end;
 exit;
 end;

 for i:=0 to height-1 do
 begin
  mf.getLine(pl^);

  for j:=0 to width-1 do
  begin
   c:=(pl+j)^;
   With pal[ord(c)] do
   begin
    w:=(b shr 3)+((g shr 2) shl 5)+((r shr 3) shl 11);
   end;
    Word(Pointer(pb)^):=w;
   inc(pb,2);
  end;

 end;
end;

Procedure SetSD(var ddsd:TDDSURFACEDESC;hw:boolean);
begin
 FillChar(ddsd, sizeof(ddsd),0);
 ddsd.dwSize := sizeof(ddsd);
 ddsd.dwFlags := DDSD_CAPS or DDSD_WIDTH or DDSD_HEIGHT;

 if usepalette then
 begin
  ddsd.dwFlags:=ddsd.dwFlags or DDSD_PIXELFORMAT;
  ddsd.ddpfPixelFormat.dwSize:=sizeof(ddsd.ddpfPixelFormat);
  ddsd.ddpfPixelFormat.dwFlags:=DDPF_PALETTEINDEXED8 or DDPF_RGB;
{  ddsd.ddpfPixelFormat.dwZBufferBitDepth:=8;
  ddsd.ddpfPixelFormat.dwAlphaBitDepth:=8;}
  ddsd.ddpfPixelFormat.dwRGBBitCount:=8;
 end
 else
 begin
  ddsd.dwFlags:=ddsd.dwFlags or DDSD_PIXELFORMAT;
  ddsd.ddpfPixelFormat.dwSize:=sizeof(ddsd.ddpfPixelFormat);
  ddsd.ddpfPixelFormat.dwFlags:=DDPF_RGB;
{  ddsd.ddpfPixelFormat.dwZBufferBitDepth:=8;
  ddsd.ddpfPixelFormat.dwAlphaBitDepth:=8;}
  ddsd.ddpfPixelFormat.dwRGBBitCount:=16;
  ddsd.ddpfPixelFormat.dwRBitMask:=$F800;
  ddsd.ddpfPixelFormat.dwGBitMask:=$7E0;
  ddsd.ddpfPixelFormat.dwBBitMask:=$1F;
 end;

 ddsd.dwWidth := width;
 ddsd.dwHeight := height;
 if hw then ddsd.ddsCaps.dwCaps := DDSCAPS_TEXTURE or DDSCAPS_VIDEOMEMORY
 else ddsd.ddsCaps.dwCaps := DDSCAPS_TEXTURE or DDSCAPS_SYSTEMMEMORY;
end;

begin
 dxr:=adxr;
 f:=OpenGameFile(mat);
 mf:=TMat.Create(f,0);

 usepalette:=(mf.info.storedAs<>byLines16) and dxr.PalettedTextures;

 width:=mf.info.width;
 height:=mf.info.Height;

 {Create surface}

 SetSD(ddsd,false);
 Res:=dxr.idd2.CreateSurface(ddsd, ads, NIL);
 DDFailCheck(res,'Creating buffer');

 {Set Palette}

 if usepalette then
 begin
  res:=ads.SetPalette(dxr.GetD3DPalette(pal));
  DDFailCheck(res,'Setting buffer palette');
 end;


 {Set surface}

 res:=ads.Lock(nil,ddsd,DDLOCK_NOSYSLOCK or DDLOCK_SURFACEMEMORYPTR or DDLOCK_WRITEONLY or DDLOCK_WAIT,0);
 DDFailCheck(res,'Locking buffer surface');

 {}
 GetMem(pl,width);

 pb:=ddsd.lpSurface;

 if usepalette then LoadPaletted
 else LoadRGB;

 FreeMem(pl);
 mf.free;


 res:=ads.UnLock(nil);
 DDFailCheck(res,'Unlocking buffer surface');


{ res:=itxs.GetDC(dc);
 DDFailCheck(res,'Getting texture DC');
 bm:=mf.LoadBitmap(width,height);
 res:=Integer(Windows.BitBlt(dc,0,0,width,height,bm.canvas.handle,0,0,SRCCOPY));
 res:=itxs.ReleaseDC(dc);
 DDCheck(res,'Releasing texture DC');

 mf.free;
 bm.free;}


 if not dxr.hardware then itxs:=ads else
 begin
  SetSD(ddsd,true);
  Res:=dxr.idd2.CreateSurface(ddsd, itxs, NIL);
  DDFailCheck(res,'Creating texture');
  if usepalette then
  begin
   res:=itxs.SetPalette(dxr.GetD3DPalette(pal));
   DDFailCheck(res,'Setting texture palette');
  end;

  FillChar(bltfx,sizeof(bltfx),0);
  bltfx.dwSize := sizeof(bltfx);
  res:=itxs.Blt( nil, ads, nil, DDBLT_WAIT,bltfx);
  DDCheck(res,'Blitting from buffer to texture');
  ads.Release;
 end;


 res:=itxs.QueryInterface(IID_IDirect3DTexture2, itx);
 DXFailCheck(res,'Getting IDirect3DTexture2 interface');


end;

Procedure TD3DTexture.SetCurrent;
var res,handle:integer;
    material:td3dmaterial;
begin
 if self=nil then
 begin
  if curdxr<>nil then
   res:=curdxr.id3ddev.SetRenderState(D3DRENDERSTATE_TEXTUREHANDLE,0);
  exit;
 end;

 if itx=nil then handle:=0
 else
 begin
  if htx=0 then res:=itx.GetHandle(dxr.id3ddev,htx);
  handle:=htx;
 end;
 res:=dxr.id3ddev.SetRenderState(D3DRENDERSTATE_TEXTUREHANDLE,handle);
 DXCheck(res,'Setting texture handle');

{ if dxr.ramp and (handle<>0) then
 begin
 FillChar(material,sizeof(material),0);
 material.dwsize := sizeof(material);
 material.dcvdiffuse.r := 0;
 material.dcvdiffuse.g := 0;
 material.dcvdiffuse.b := 0;
 material.dcvambient.r := 0;
 material.dcvambient.g := 0;
 material.dcvambient.b := 0;
 material.dcvspecular.r := 0;
 material.dcvspecular.g := 0;
 material.dcvspecular.b := 0;
 material.dvpower := 0;
 material.dwrampsize := 1;
 material.hTexture:=handle;
 res:=dxr.irmat.SetMaterial(material);
 DXCheck(res,'Setting texture material');
 res:=dxr.irmat.GetHandle(dxr.id3ddev,htx);
 DXCheck(res,'Getting material handle');
 res:=dxr.id3ddev.SetLightState(D3DLIGHTSTATE_MATERIAL,htx);

 end;}

end;

Destructor TD3DTexture.Destroy;
begin
 FreeUnk(itx);
 FreeUnk(itxs);
end;

end.

