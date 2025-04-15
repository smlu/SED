unit d3d_NPRender;

{$O-}

interface
uses Ole2, Windows, glunit, Prender, Classes, J_Level, Forms,
     Messages, files, FileOperations, graph_files,
     lev_utils, sysUtils, misc_utils, GlobalVars, geometry,
     (*D3DRMObj, D3DrmWin, D3DRMDef,D3Drm,*) D3DTypes, DDraw, D3D, dxtools,
     D3Dcaps, graphics, images, ExtCtrls, Generics.Collections, D3DUtils;

//const
//  maxVerts = 4096;

type

TTexFormat = class
  ddpfPixelFormat: TDDPixelFormat;
  ci: TColorInfo;
  constructor Create(ddpixfmt: TDDPixelFormat; ci: TColorInfo);
end;

TD3D5PRenderer = class(TNewPRenderer)
 hardware:boolean;
 ramp:boolean;
 idd2:IDirectDraw2;
 id3d2:IDirect3D2;
 id3ddev:IDirect3DDevice2;
 iview:IDirect3DViewPort2;
 ids:IDirectDrawSurface;
 idsback:IDirectDrawSurface;
 idz:IDirectDrawSurface;
 backmat:IDirect3DMaterial2;
 ipal:IDirectDrawPalette;
 pdd:^TD3DDeviceDesc;

 PalettedTextures:boolean;
 texFormats: TObjectList<TTexFormat>;

 irmat:idirect3dmaterial2;

 //Constructor CreateFromPanel(aPanel:TPanel; geoMode: TGeoMode = Texture; lightMode: TLightMode = Gouraud);
 Destructor Destroy;override;
 Procedure Initialize;override;
 Function IsFogSupported(): Boolean;override;
 Procedure SetFog(color: TColorF; fogStart, fogEnd: double; density: Double = 1.0); override;
 Function LoadTexture(const name: string; const ppal: PTCMPPal; const pcmp: PTCMPTable): T3DPTexture; override;
 Procedure EnableZTest(enable: Boolean); override;
 Procedure DrawPolys(const [Ref] polys: TArray<T3DPoly>; count: Integer = -1); override;

 Function ProjectPoint(x, y, z: double; Var WinX, WinY, WinZ: double): Boolean; override;
 Procedure GetWorldLine(X, Y: integer;var X1,Y1,Z1, X2,Y2,Z2:double);override;
 Procedure SetClearColor(color: TColorF); override;
 Procedure Redraw;override;

Private
 d3dvxs: TArray<TD3DLVERTEX>;
 d3didxs: TArray<WORD>;
 Function GetD3DPalette(const pal:TCMPPal):IDirectDrawPalette;
 Procedure SetRendererState(const faceflags: longint);
 Procedure DrawWiredPolys(const [Ref] polys: TArray<T3DPoly>; count: Integer);
 Procedure DrawSolidPolys(const [Ref] polys: TArray<T3DPoly>; count: Integer);
end;

PTD3D5PRenderer = ^TD3D5PRenderer;


TD3DTexture = class(T3DPTexture)
 itx: IDirect3DTexture2;
 itxs: IDirectDrawSurface;
 dxr: TD3D5PRenderer;
 htx: TD3DTextureHandle;
 Constructor CreateFromMat(const Mat: string; const ppal: PTCMPPal; const pcmp: PTCMPTable; adxr: TD3D5PRenderer; gamma: double);
 Procedure SetCurrent;{override;}
 Destructor Destroy;override;
end;


implementation
uses D3D_PRender, math;

var curdxr: TD3D5PRenderer;

Function D3DVECTOR(dx,dy,dz:single): TD3DVector;
begin
 Result.x := dx;
 Result.y := dy;
 Result.z := dz;
end;

Function IsRgbaFormat(const ddpixfmt: TDDPixelFormat; const ci: TColorInfo): Boolean; inline;
begin
  const dwRMask:DWORD  = $ff shl ci.redShl;
  const dwBMask:DWORD  = $ff shl ci.blueShl;
  const dwAMask:DWORD  = $ff shl ci.alphaShl;
  Result := (dwRMask = ddpixfmt.dwRBitMask)
    and (dwBMask = ddpixfmt.dwBBitMask)
    and (dwAMask = ddpixfmt.dwRGBAlphaBitMask);
end;

Function IsRgbFormat(const ddpixfmt: TDDPixelFormat; const ci: TColorInfo): Boolean; inline;
begin
  const dwRMask:DWORD = $ff shl ci.redShl;
  const dwBMask:DWORD = $ff shl ci.blueShl;
  Result := (dwRMask = ddpixfmt.dwRBitMask) and (dwBMask = ddpixfmt.dwBBitMask);
end;

Function _EnumTXFormats(const ddsd: TDDSURFACEDESC; arg:pointer): HRESULT ; stdcall ;
begin
  result := D3DENUMRET_OK;
  var pd3drd: PTD3D5PRenderer := arg;

  if BitMaskTest(ddsd.ddpfPixelFormat.dwFlags, DDPF_PALETTEINDEXED8) then
    pd3drd.PalettedTextures := true
  else if BitMaskTest(ddsd.ddpfPixelFormat.dwFlags, DDPF_RGB) and (ddsd.ddpfPixelFormat.dwRGBBitCount >= 24) then
    begin
      if ddsd.ddpfPixelFormat.dwRGBBitCount = 24 then
        begin
          if IsRgbFormat(ddsd.ddpfPixelFormat, RGB24) then
            pd3drd.texFormats.Add(TTexFormat.Create(ddsd.ddpfPixelFormat, RGB24))
          else if IsRgbFormat(ddsd.ddpfPixelFormat, BGR24) then
            pd3drd.texFormats.Add(TTexFormat.Create(ddsd.ddpfPixelFormat, BGR24))
        end
      else // 32
        begin
          if BitMaskTest(ddsd.ddpfPixelFormat.dwFlags, DDPF_ALPHAPIXELS) then
            begin
              if IsRgbaFormat(ddsd.ddpfPixelFormat, RGBA32) then
                pd3drd.texFormats.Add(TTexFormat.Create(ddsd.ddpfPixelFormat, RGBA32))
              else if IsRgbaFormat(ddsd.ddpfPixelFormat, ABGR32) then
                pd3drd.texFormats.Add(TTexFormat.Create(ddsd.ddpfPixelFormat, ABGR32))
              else if IsRgbaFormat(ddsd.ddpfPixelFormat, ARGB32) then
                pd3drd.texFormats.Add(TTexFormat.Create(ddsd.ddpfPixelFormat, ARGB32))
              else if IsRgbaFormat(ddsd.ddpfPixelFormat, BGRA32) then
                pd3drd.texFormats.Add(TTexFormat.Create(ddsd.ddpfPixelFormat, BGRA32))
            end
          else
            begin
              if IsRgbFormat(ddsd.ddpfPixelFormat, RGB32) then
                pd3drd.texFormats.Add(TTexFormat.Create(ddsd.ddpfPixelFormat, RGB32))
              else if IsRgbFormat(ddsd.ddpfPixelFormat, BGR32) then
                pd3drd.texFormats.Add(TTexFormat.Create(ddsd.ddpfPixelFormat, BGR32))
            end;
        end;
    end;

// begin {Found paletted texture}
//
//  //Result:=D3DENUMRET_CANCEL;
// end;

end;

Function GetZBufferBits(flags: integer):integer;
begin
  if BitMaskTest(Flags, DDBD_16) then begin result := 16; exit; end;
  if BitMaskTest(Flags, DDBD_8)  then begin result := 8; exit; end;
  if BitMaskTest(Flags, DDBD_24) then begin result := 24; exit; end;
  if BitMaskTest(Flags, DDBD_32) then begin result := 32; exit; end;
end;

Function EnumZBufferPFCallback(lpDDPixFmt: PDDPixelFormat; lpContext:PDDPixelFormat):HRESULT;
begin
  Result := D3DENUMRET_CANCEL;
  if (lpDDPixFmt <> nil) and (lpContext <> nil) then
  begin
    if (lpDDPixFmt.dwFlags and DDPF_ZBUFFER) <> 0 then
      begin
        var bpp := lpDDPixFmt.dwRGBBitCount;
        if (bpp >= 16) and (bpp > lpContext.dwRGBBitCount) then
          lpContext^ := lpDDPixFmt^;
      end;
    Result := D3DENUMRET_OK;
  end;
end;

Constructor TTexFormat.Create(ddpixfmt: TDDPixelFormat; ci: TColorInfo);
begin
   self.ddpfPixelFormat := ddpixfmt;
   self.ci := ci;
end;

Procedure TD3D5PRenderer.Initialize;
var res:HResult;
    idd:IDirectDraw;
    ddsd:TDDSURFACEDESC;

    iclp:IDirectDrawClipper;
    dview:TD3DVIEWPORT;
    rc:TRect;
    ndevice:integer;
    //material:td3dmaterial;
    //imat:idirect3dmaterial2;
    // var hmat: TD3DMaterialHandle;
    hmat:TD3DMaterialHandle;
begin
  EnumDevices;
  if not DirectX5 then Raise
    Exception.Create('DirectX 5.0 or higher required!');

  ndevice := GetDeviceNum(D3DDevice);
  hardware := _D3Ddrivers[ndevice].HWDeviceDesc.dcmColorModel <> D3DCOLOR_INVALID_0;

  if hardware then pdd:=@_D3Ddrivers[ndevice].HWDeviceDesc
  else pdd := @_D3Ddrivers[ndevice].HELDeviceDesc;

  ramp := pdd^.dcmColorModel=D3DCOLOR_MONO;

  if Ramp then
    Raise Exception.Create('Ramp device cannot be used with D3D IM renderer');

  if DirectDrawCreate(NIL, idd, NIL) <> DD_OK then exit;
  if idd.QueryInterface(IID_IDirectDraw2, iDD2) <> S_OK then exit;

    // Set vertex buffer size
  SetLength(d3dvxs, pdd.dwMaxVertexCount);
  SetLength(d3didxs, pdd.dwMaxVertexCount * 3); // 3 triangles

  res := iDD2.SetCooperativeLevel(WHandle, DDSCL_NORMAL);
  DDCheck(res,'in SetCooperativeLevel');

  res := idd2.QueryInterface(IID_IDirect3D2, id3d2);
  DXFailCheck(res,'Getting Direct3D2 inteface');

  {Create Windowed }
  GetClientRect(whandle,rc);

  {Create front buffer}
  FillChar(ddsd,sizeof(ddsd),0);
  ddsd.dwSize  := sizeof(ddsd);
  ddsd.dwFlags := DDSD_CAPS;

  {if (hardware) then } ddsd.ddsCaps.dwCaps:=DDSCAPS_PRIMARYSURFACE or DDSCAPS_VIDEOMEMORY;
  // else ddsd.ddsCaps.dwCaps:=DDSCAPS_PRIMARYSURFACE or DDSCAPS_SYSTEMMEMORY;

  res := iDD2.CreateSurface(ddsd,ids, NIL);
  DDFailCheck(res,'Creating Primary Surface');

  {Create Clipper}
  res := iDD2.CreateClipper(0, iClp, NIL);
  DDFailCheck(res,'Creating clipper');

  res := iclp.SetHWnd(0,whandle);
  DDFailCheck(res,'Setting clipper''s window');

  res := ids.SetClipper(iClp);
  DDFailCheck(res,'Setting clipper');

  iclp.Release;

  {Create back buffer}
  ddsd.dwSize := sizeof(ddsd);
  res := idd2.GetDisplayMode(ddsd);

  ddsd.dwFlags := DDSD_CAPS or DDSD_HEIGHT or DDSD_WIDTH;

  if (hardware) then
    ddsd.ddsCaps.dwCaps := DDSCAPS_OFFSCREENPLAIN or DDSCAPS_3DDEVICE or DDSCAPS_VIDEOMEMORY
  else
    ddsd.ddsCaps.dwCaps := DDSCAPS_OFFSCREENPLAIN or DDSCAPS_3DDEVICE or DDSCAPS_SYSTEMMEMORY;

  ddsd.dwWidth  := vwidth;
  ddsd.dwHeight := vheight;

  res := iDD2.CreateSurface(ddsd, idsback, NIL);
  DDFailCheck(res,'Creating back buffer');

  {res:=ids.AddAttachedSurface(idsback);}

  {Create Z buffer - skipped}
  FillChar(ddsd,sizeof(ddsd),0);
  ddsd.dwSize  := sizeof(ddsd);
  ddsd.dwFlags := DDSD_CAPS or DDSD_HEIGHT or DDSD_WIDTH or DDSD_ZBUFFERBITDEPTH;

  if (hardware) then
    ddsd.ddsCaps.dwCaps := DDSCAPS_ZBUFFER or DDSCAPS_VIDEOMEMORY
  else
    ddsd.ddsCaps.dwCaps := DDSCAPS_ZBUFFER or DDSCAPS_SYSTEMMEMORY;

   // TODO: directx6
  {Enumerate Z buffer formats - skipped}
  //id3d2.EnumDevices()(IID_IDirect3DTnLHalDevice , EnumZBufferPFCallback, @ddsd.PDDPixelFormat);
  ddsd.dwZBufferBitDepth := GetZBufferBits(pdd^.dwDeviceZBufferBitDepth);
  ddsd.dwWidth  := vwidth;
  ddsd.dwHeight := vheight;

  res := iDD2.CreateSurface(ddsd, idz, NIL);
  DDFailCheck(res,'Creating Z buffer');

  res := idsback.AddAttachedSurface(idz);
  DDFailCheck(res,'Attaching Z buffer');

  {res:=ids.QueryInterface(_D3Ddrivers[0].id,iD3DDev);}
  res := iD3D2.CreateDevice(_D3Ddrivers[ndevice].id, idsback, id3dDev);
  DXFailCheck(res,'Creating device');

  {Get supported texture formats }
  texFormats := TObjectList<TTexFormat>.Create((*AOwnsObjects=*)true);
  res := id3ddev.EnumTextureFormats(_EnumTXFormats, @self);
  DXCheck(res,'Checking texture formats');

  if ((CurrentProject = IJIM) or (not self.PalettedTextures)) and (texFormats.Count < 1) then
    raise Exception.Create('No supported RGB(A) texture format found');

  // Create viewport
  res := id3d2.CreateViewport(iview, nil);
  DXFailCheck(res,'Creating viewport');

  res := id3ddev.AddViewport(iview);
  DXFailCheck(res,'Adding viewport');

  FillChar(dview, sizeof(dview), 0);
  with dview do
    begin
       dwSize   := sizeof(TD3DVIEWPORT);
       dwX      := 0;
       dwY      := 0;
       dwWidth  := vWidth;
       dwHeight := vHeight;
       dvScaleX := dwWidth / 2.0;
       dvScaleY := dwWidth / 2.0;
       dvMaxX   := D3DDivide(D3DVAL(dwWidth), D3DVAL(2 * dvScaleX));
       dvMaxY   := D3DDivide(D3DVAL(dwHeight), D3DVAL(2 * dvScaleY));
    end;

  res := iview.SetViewport(dview);
  DXFailCheck(res,'Setting viewport');
  res := id3ddev.SetCurrentViewport(iview);
  DXFailCheck(res,'Setting current viewport');

  // Set viewport background material
  res := id3d2.CreateMaterial(backmat, nil);
  DXCheck(res,'Creating material');

  res := backmat.GetHandle(id3ddev, hmat);
  DXCheck(res,'Getting material handle');

  res := iview.SetBackground(hmat);
  DXCheck(res,'Setting background');

  // Init renderer state
  DXCheck(Id3dDev.SetRenderState(D3DRENDERSTATE_FILLMODE, Integer(D3DFILL_SOLID)), 'Setting render state: Set fill mode to solid');
  DXCheck(Id3dDev.SetRenderState(D3DRENDERSTATE_TEXTUREPERSPECTIVE, Integer(TRUE)), 'Setting render state: Enable texture perspective mode');
  DXCheck(Id3dDev.SetRenderState(D3DRENDERSTATE_CULLMODE, Integer(D3DCULL_CW)), 'Setting render state: Set cull mode to CW');
  DXCheck(Id3dDev.SetRenderState(D3DRENDERSTATE_FOGENABLE, Integer(FALSE)), 'Setting render state: Disable fog');

  // Texture filtering
  var texFilter := D3DFILTER_LINEAR;  // bilinear
  if not BitMaskTest(pdd.dpcTriCaps.dwTextureFilterCaps, D3DPTFILTERCAPS_LINEAR) then
    texFilter := D3DFILTER_NEAREST;
  DXCheck(Id3dDev.SetRenderState(D3DRENDERSTATE_TEXTUREMAG, Integer(texFilter)), 'Setting render state: Texture mag. filter');
  DXCheck(Id3dDev.SetRenderState(D3DRENDERSTATE_TEXTUREMIN, Integer(texFilter)), 'Setting render state: Texture mag. filter');


  DXCheck(Id3dDev.SetRenderState(D3DRENDERSTATE_ZENABLE, 1), 'Setting render state: Enable Z buffer');
  DXCheck(Id3dDev.SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, 1), 'Setting render state: Enable Z write');
  DXCheck(Id3dDev.SetRenderState(D3DRENDERSTATE_ZFUNC, Integer(D3DCMP_LESSEQUAL)), 'Setting render state: Set Z function');

 if CurrentProject = IJIM then
   begin
    // Jones Engine
    DXCheck(Id3dDev.SetRenderState(D3DRENDERSTATE_SUBPIXEL, Integer(TRUE)), 'Setting render state: Enable subpixel');
    DXCheck(Id3dDev.SetRenderState(D3DRENDERSTATE_TEXTUREADDRESSU, Integer(D3DTADDRESS_WRAP)), 'Setting render state: Set texture address U to wrap');
    DXCheck(Id3dDev.SetRenderState(D3DRENDERSTATE_TEXTUREADDRESSV, Integer(D3DTADDRESS_WRAP)), 'Setting render state: Set texture address V to wrap');

    if    BitMaskTest(pdd.dpcTriCaps.dwTextureCaps, D3DPTEXTURECAPS_TRANSPARENCY)
      and BitMaskTest(pdd.dpcTriCaps.dwSrcBlendCaps, D3DPBLENDCAPS_SRCALPHA)
      and BitMaskTest(pdd.dpcTriCaps.dwDestBlendCaps, D3DPBLENDCAPS_INVSRCALPHA)
    then
      begin
        DXCheck(Id3dDev.SetRenderState(D3DRENDERSTATE_BLENDENABLE, Integer(TRUE)), 'Setting render state: Enable blend');
        DXCheck(Id3dDev.SetRenderState(D3DRENDERSTATE_TEXTUREMAPBLEND, Integer(D3DTBLEND_MODULATEALPHA)), 'Setting render state: Set texture blend to D3DTBLEND_MODULATE');

        DXCheck(Id3dDev.SetRenderState(D3DRENDERSTATE_SRCBLEND, Integer(D3DBLEND_SRCALPHA)), 'Setting render state: Set src blend to SRCALPHA');
        DXCheck(Id3dDev.SetRenderState(D3DRENDERSTATE_DESTBLEND, Integer(D3DBLEND_INVSRCALPHA)), 'Setting render state: Set dest blend to INVSRCALPHA');
        DXCheck(Id3dDev.SetRenderState(D3DRENDERSTATE_ALPHATESTENABLE, Integer(TRUE)), 'Setting render state: Enable alpha test');
        if BitMaskTest(pdd.dpcTriCaps.dwAlphaCmpCaps, D3DPCMPCAPS_GREATER) then
        begin
          DXCheck(Id3dDev.SetRenderState(D3DRENDERSTATE_ALPHAFUNC, integer(D3DCMP_GREATER)), 'Setting render state: Enable COLORKEYENABLE');
          DXCheck(Id3dDev.SetRenderState(D3DRENDERSTATE_ALPHAREF, $00), 'Setting render state: Set alpha ref. to 0x00');
        end;
      end;

    //DXCheck(Id3dDev.SetRenderState(D3DRENDERSTATE_COLORKEYENABLE, integer(TRUE)), 'Setting render state: Enable COLORKEYENABLE');
    //DXCheck(Id3dDev.SetRenderState(D3DRENDERSTATE_STIPPLEDALPHA, integer(TRUE)), 'Setting render state: Enable COLORKEYENABLE');

    DXCheck(Id3dDev.SetRenderState(D3DRENDERSTATE_SHADEMODE, Integer(D3DSHADE_GOURAUD)), 'Setting render state: Set shade mode to gouraud');
    DXCheck(Id3dDev.SetRenderState(D3DRENDERSTATE_MONOENABLE, Integer(FALSE)), 'Setting render state: Disable mono');

    if not IsFogSupported() then
      fog.Enabled := false;

    DXCheck(Id3dDev.SetRenderState(D3DRENDERSTATE_SPECULARENABLE, Integer(FALSE)), 'Setting render state: Disable speculare');
    DXCheck(Id3dDev.SetRenderState(D3DRENDERSTATE_DITHERENABLE, Integer(TRUE)), 'Setting render state: Enable dithere');
    DXCheck(Id3dDev.SetRenderState(D3DRENDERSTATE_ANTIALIAS, Integer(D3DANTIALIAS_SORTINDEPENDENT)), 'Setting render state: Enable antialias');
   end;

   DXCheck(Id3dDev.SetRenderState(D3DRENDERSTATE_EDGEANTIALIAS, Integer(True)), 'Setting render state: Enable antialias');
end;

procedure FreeUnk(Iu:IUnknown);
var n:integer;
begin
 if iu <> nil then
 begin
  n := iu.Release;
  if n <> 0 then
  begin
   if (n = 1) then;
  end;
 end;
end;

//Constructor TD3D5PRenderer.CreateFromPanel(aPanel: TPanel; geoMode: TGeoMode; lightMode: TLightMode);
//begin
//  Inherited CreateFromPanel(aPanel, geoMode, lightMode);
//  Whandle := aPanel.handle;
//end;

Destructor TD3D5PRenderer.Destroy;
begin
 FreeUnk(idsback);
 FreeUnk(ids);
 FreeUnk(idz);
 FreeUnk(iview);
 FreeUnk(id3ddev);
 FreeUnk(backmat);
 FreeUnk(id3d2);
 FreeUnk(idd2);
 FreeUnk(irmat);
 texFormats.Free;
 Inherited destroy;
end;

Function TD3D5PRenderer.IsFogSupported(): Boolean;
begin
  var rcaps := pdd.dpcTriCaps.dwRasterCaps;
  Result := (rcaps and D3DPRASTERCAPS_FOGTABLE) <> 0;
end;

Procedure TD3D5PRenderer.SetFog(color: TColorF; fogStart, fogEnd: double; density: Double = 1.0);
begin
  Inherited SetFog(color, fogStart, fogEnd, density);

  var res := Id3dDev.SetRenderState(D3DRENDERSTATE_FOGCOLOR, EncodeRGB(fog.Color));
  DXCheck(res, 'Setting render state: Set fog color');
  if res <> DD_OK then
    exit;

  if fogDensity = 0.0 then
    DXCheck(Id3dDev.SetRenderState(D3DRENDERSTATE_FOGTABLEMODE, Integer(D3DFOG_NONE)), 'Setting render state: Set fog table mode to none')
  else
    begin
      DXCheck(Id3dDev.SetLightState(D3DLIGHTSTATE_FOGMODE, Integer(D3DFOG_NONE)), 'Setting light state: Set fog mode to none');
      DXCheck(Id3dDev.SetRenderState(D3DRENDERSTATE_RANGEFOGENABLE, Integer(True)), 'Setting render state: Disable range fog');

      DXCheck(Id3dDev.SetRenderState(D3DRENDERSTATE_FOGTABLEMODE, Integer(D3DFOG_LINEAR)), 'Setting render state: Set fog table mode to linear');

      var ff := 0.0311; // TODO: Note, this is just a hack for eye-relative fog
      var ffogStart: Single := fog.FogStart * ff;
      res := Id3dDev.SetRenderState(D3DRENDERSTATE_FOGTABLESTART, LPDWORD(@ffogStart)^);
      DXCheck(res, 'Setting render state: Set fog table start');
      if res <> DD_OK then
        exit;
      var ffogEnd: Single := ((2.0 - fogDensity) * fog.FogEnd) * ff;
      DXCheck(Id3dDev.SetRenderState(D3DRENDERSTATE_FOGTABLEEND, LPDWORD(@ffogEnd)^), 'Setting render state: Set fog table end');
    end;
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

Procedure TD3D5PRenderer.SetRendererState(const faceflags: longint);
begin
  var cullMode := D3DCULL_CW;
  if BitMaskTest(faceflags, FF_DOUBLESIDED) then cullMode := D3DCULL_NONE;
  DXCheck(Id3dDev.SetRenderState(D3DRENDERSTATE_CULLMODE, Integer(cullMode)), 'Setting render state: Set cull mode');

  var alphaRef := $00;
  if BitMaskTest(faceflags, FF_RD_AlphaRef)then
    alphaRef := $A0;
  DXCheck(Id3dDev.SetRenderState(D3DRENDERSTATE_ALPHAREF, alphaRef), 'Setting render state: Set alpha ref.');

  var textAddrU := D3DTADDRESS_WRAP;
  var textAddrV := D3DTADDRESS_WRAP;
  if BitMaskTest(faceflags, FF_TexClampX) then textAddrU := D3DTADDRESS_CLAMP;
  if BitMaskTest(faceflags, FF_TexClampY) then textAddrV := D3DTADDRESS_CLAMP;

  DXCheck(Id3dDev.SetRenderState(D3DRENDERSTATE_TEXTUREADDRESSU, Integer(textAddrU)), 'Setting render state: Set texture address U');
  DXCheck(Id3dDev.SetRenderState(D3DRENDERSTATE_TEXTUREADDRESSV, Integer(textAddrV)), 'Setting render state: Set texture address V');

  var texFilter := D3DFILTER_LINEAR;  // bilinear
  if BitMaskTest(faceflags, FF_TexNoFiltering) or not BitMaskTest(pdd.dpcTriCaps.dwTextureFilterCaps, D3DPTFILTERCAPS_LINEAR) then
    texFilter := D3DFILTER_NEAREST;
  DXCheck(Id3dDev.SetRenderState(D3DRENDERSTATE_TEXTUREMAG, Integer(texFilter)), 'Setting render state: Texture mag. filter');
  DXCheck(Id3dDev.SetRenderState(D3DRENDERSTATE_TEXTUREMIN, Integer(texFilter)), 'Setting render state: Texture mag. filter');

  var zwriteEnabled := true;
  if BitMaskTest(faceflags, FF_ZWriteDisabled) then zwriteEnabled := false;
  DXCheck(Id3dDev.SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, Integer(zwriteEnabled)), 'Setting render state: Set Z write');

  if CurrentProject = IJIM then
    begin
       if fog.Enabled and BitMaskTest(faceflags, FF_IJIM_FogEnabled) then
        DXCheck(Id3dDev.SetRenderState(D3DRENDERSTATE_FOGENABLE, Integer(TRUE)), 'Setting render state: Enable fog')
       else
        DXCheck(Id3dDev.SetRenderState(D3DRENDERSTATE_FOGENABLE, Integer(FALSE)), 'Setting render state: Disable fog');
    end;
end;

Function TD3D5PRenderer.LoadTexture(const name: string;const ppal: PTCMPPal; const pcmp: PTCMPTable): T3DPTexture;
var i: integer;
    Ttx: TD3DTexture;
begin
 Result := nil;
 ttx := TD3DTexture.CreateFromMat(name, ppal, pcmp, self, gamma);
 Result := ttx;
end;

Function IsEqualTex(const tex1, tex2: T3DPTexture): Boolean;
begin
  Result := False;
  if (tex1 = tex2) then
    Result := True
  else if (tex1 <> nil) and (tex2 <> nil) then
    Result := (tex1.name = tex2.name);
end;

Procedure TD3D5PRenderer.EnableZTest(enable: Boolean);
begin
  DXCheck(Id3dDev.SetRenderState(D3DRENDERSTATE_ZENABLE, Integer(enable)), 'Setting render state: Enable/Disable Z buffer');
end;

Procedure TD3D5PRenderer.DrawPolys(const [Ref] polys: TArray<T3DPoly>; count: Integer);
begin
  if polys = nil then exit;

  if (count < 0) or (count > Length(polys)) then
    count := Length(polys);

  if geoMode <= Wireframe then
    DrawWiredPolys(polys, count)
  else
    begin
      DrawSolidPolys(polys, count);
    end;
end;

Procedure TD3D5PRenderer.DrawWiredPolys(const [Ref] polys: TArray<T3DPoly>; count: Integer);
  var
    res: HRESULT;
    lfv: array [0..3] of TD3DLVERTEX;
begin
  var maxDxVerts := Length(d3dvxs);
  SetRendererState(0);
  TD3Dtexture(nil).SetCurrent;

  for var i := 0 to count-1 do
  begin
      var numVerts := polys[i].vxds.count;
      if numVerts > maxDxVerts then
        begin
          PanMessageFmt(mt_warning, 'TD3D5PRenderer.RenderWiredPolys: Polygon has more vertices [%d] than D3Device max vertices [%d]!', [numVerts, maxDxVerts]);
          numVerts := maxDxVerts;
        end;

      for var j := 0 to numVerts-1 do
        With d3dvxs[j] do
          begin
            var vxd:=polys[i].getVXD(j);
            x:=vxd.x;
            y:=vxd.y;
            z:=vxd.z;
            color := EncodeARGB(wfColor);
          end;

     if geoMode = Vertex then
      begin
        res := id3ddev.DrawPrimitive(D3DPT_POINTLIST, D3DVT_LVERTEX, @d3dvxs[0], numVerts, D3DDP_DONOTUPDATEEXTENTS);
      end
     else
      begin
        res := id3ddev.DrawPrimitive(D3DPT_LINESTRIP, D3DVT_LVERTEX, @d3dvxs[0], numVerts, D3DDP_DONOTUPDATEEXTENTS);
        if res = DD_OK then
        begin // Add close line
          CopyMemory(@lfv[0], @d3dvxs[numVerts - 1], sizeof(lfv[0]));
          CopyMemory(@lfv[1], @d3dvxs[0], sizeof(lfv[1]));
          res := id3ddev.DrawPrimitive(D3DPT_LINESTRIP, D3DVT_LVERTEX, @lfv[0], 2, D3DDP_DONOTUPDATEEXTENTS);
        end;
      end;
     DXCheck(res,'DrawPrimitive');
  end;
end;

Procedure TD3D5PRenderer.DrawSolidPolys(const [Ref] polys: TArray<T3DPoly>; count: Integer);
begin
  var maxDxVerts   := Length(d3dvxs);
  var numDxVerts   := 0;
  var numIdxs      := 0;
  var curFaceFlags := 0;
  var curGeo: TGeoMode;
  var curTex: T3DPTexture := nil;
  var i := 0;
  while i < count do
    begin
      var poly := polys[i];
      if (poly = nil) or (poly.geo = NotDrawn) then
        begin
          i := i + 1;
          continue;
        end;

      curFaceFlags := poly.faceflags;
      curGeo := poly.geo;
      curTex := poly.tx;
      if curGeo <> Texture then
        curTex := nil;

      SetRendererState(curFaceFlags);
      TD3Dtexture(curTex).SetCurrent;

      while i < count do // Try to put on rd vertices cache as many polys before actually drawing
        begin
          var numVerts := poly.vxds.Count;
          if numVerts > maxDxVerts then
            begin
              PanMessageFmt(mt_warning, 'TD3D5PRenderer.RenderSolidPolys: Polygon has more vertices [%d] than D3Device max vertices [%d]!', [numVerts, maxDxVerts]);
              numVerts := maxDxVerts;
            end;

          for var j := 0 to numVerts-1 do
          With d3dvxs[numDxVerts + j] do
            begin
              var vxd := poly.getVXD(j);
              x := vxd.x;
              y := vxd.y;
              z := vxd.z;
              tu := vxd.u;
              tv := vxd.v;

              dcSpecular := 0;

              var lcolor := poly.GetLitColor(vxd.intensity);
              if BitMaskTest(poly.faceflags, FF_Transluent) then
                color := D3DRGBA(lcolor.r, lcolor.g, lcolor.b, lcolor.a)
              else
                color := D3DRGB(lcolor.r, lcolor.g, lcolor.b);
            end;

          // Triangulaton
          if numVerts <= 3 then
            begin
              d3didxs[numIdxs]     := numDxVerts;
              d3didxs[numIdxs + 1] := numDxVerts + 1;
              d3didxs[numIdxs + 2] := numDxVerts + 2;
              numIdxs := numIdxs + 3;
            end
          else
            begin
              const totalTris = numVerts - 2;
              var ofsTriVert0 := 0;
              var ofsTriVert1 := 1;
              var ofsTriVert2 := numVerts - 1;
              for var idx := 0 to totalTris do
                begin
                  d3didxs[numIdxs]     := numDxVerts + ofsTriVert0;
                  d3didxs[numIdxs + 1] := numDxVerts + ofsTriVert1;
                  d3didxs[numIdxs + 2] := numDxVerts + ofsTriVert2;
                  numIdxs := numIdxs + 3;
                  if (idx and 1) = 0 then //((not idx) and 1) <> 0 then  //if even
                    begin
                      ofsTriVert0 := ofsTriVert1;
                      ofsTriVert1 := ofsTriVert1 + 1;
                    end
                  else // if idx is odd
                    begin
                      ofsTriVert0 := ofsTriVert2;
                      ofsTriVert2 := ofsTriVert2 - 1;
                    end;
                end;
            end;

          numDxVerts := numDxVerts + numVerts;
          if (i + 1) < count then // Try to squeeze the next poly on the rd vert cache
            begin
              poly := polys[i + 1];
              if (poly.faceflags <> curFaceFlags)
              or ((poly.vxds.Count + numDxVerts) >= maxDxVerts)
              or (poly.geo <> curGeo)
              or (not IsEqualTex(poly.tx, curTex)) then
                break;
             end;
          i := i + 1;
        end;

      // Jones engine DirectX6 calls DrawIndexedPrimitive for world geometry with
      //  dwVertexTypeDesc = D3DFVF_TEX0 | D3DFVF_DIFFUSE |D3DFVF_SPECULAR | D3DFVF_XYZRHW
      //  dwFlags = D3DDP_DONOTUPDATEEXTENTS | D3DDP_DONOTLIGHT - Hint that the lighting should not be applied on vertices.
      //var res := id3ddev.DrawPrimitive(D3DPT_TRIANGLEFAN, D3DVT_LVERTEX, @d3dvxs[0], numVerts, D3DDP_DONOTUPDATEEXTENTS);

      var pIdices := d3didxs[0];
      var res := id3ddev.DrawIndexedPrimitive(D3DPT_TRIANGLELIST, D3DVT_LVERTEX, @d3dvxs[0], numDxVerts, d3didxs[0], numIdxs, D3DDP_DONOTUPDATEEXTENTS);
      DXCheck(res,'DrawIndexedPrimitive');
      numDxVerts := 0;
      numIdxs    := 0;
      i := i + 1;
    end;
end;

procedure RenderSolidPoly(const [Ref] poly: T3DPoly; var vertices: TArray<TD3DLVERTEX>; var startIndex: Integer; var indices: TArray<WORD>; var curIdx: Integer); overload;
begin
  var numVerts := poly.vxds.Count;
  for var j := 0 to numVerts - 1 do
  With vertices[startIndex + j] do
    begin
      var vxd := poly.getVXD(j);
      x := vxd.x;
      y := vxd.y;
      z := vxd.z;
      tu := vxd.u;
      tv := vxd.v;

      dcSpecular := 0;

      var lcolor := poly.GetLitColor(vxd.intensity);
      if BitMaskTest(poly.faceflags, FF_Transluent) then
        color := D3DRGBA(lcolor.r, lcolor.g, lcolor.b, lcolor.a)
      else
        color := D3DRGB(lcolor.r, lcolor.g, lcolor.b);
    end;

  // Triangulaton
  if numVerts <= 3 then
    begin
      indices[curIdx]     := startIndex;
      indices[curIdx + 1] := startIndex + 1;
      indices[curIdx + 2] := startIndex + 2;
      curIdx := curIdx + 3;
    end
  else
    begin
      const totalTris = numVerts - 2;
      var ofsTriVert0 := 0;
      var ofsTriVert1 := 1;
      var ofsTriVert2 := numVerts - 1;
      for var idx := 0 to totalTris do
        begin
          indices[curIdx]     := startIndex + ofsTriVert0;
          indices[curIdx + 1] := startIndex + ofsTriVert1;
          indices[curIdx + 2] := startIndex + ofsTriVert2;
          curIdx := curIdx + 3;
          if (idx and 1) = 0 then //((not idx) and 1) <> 0 then  //if even
            begin
              ofsTriVert0 := ofsTriVert1;
              ofsTriVert1 := ofsTriVert1 + 1;
            end
          else // if idx is odd
            begin
              ofsTriVert0 := ofsTriVert2;
              ofsTriVert2 := ofsTriVert2 - 1;
            end;
        end;
    end;
    startIndex := startIndex + poly.vxds.Count;
end;

Function TD3D5PRenderer.ProjectPoint(x, y, z: double; Var WinX, WinY, WinZ: double): Boolean;
var
  vp: TD3DViewport;
  mWorld, mView, mProj, mWVP: TD3DMatrix;
  pt, clipPt: TD3DVector;  // Assume TD3DVector = record X, Y, Z, W: Single end;
  w: Single;
  hr: HRESULT;
begin
  // Retrieve the viewport from the Direct3D ViewPort interface.
  FillChar(vp, SizeOf(vp), 0);
  vp.dwSize := SizeOf(TD3DViewport);
  hr := iview.GetViewport(vp);
  if Failed(hr) then
  begin
    Result := False;
    Exit;
  end;

  // Retrieve transformation matrices from the device.
  hr := id3ddev.GetTransform(D3DTRANSFORMSTATE_WORLD, mWorld);
  if Failed(hr) then Exit(False);
  hr := id3ddev.GetTransform(D3DTRANSFORMSTATE_VIEW, mView);
  if Failed(hr) then Exit(False);
  hr := id3ddev.GetTransform(D3DTRANSFORMSTATE_PROJECTION, mProj);
  if Failed(hr) then Exit(False);


//  Id3dDev.SetTransform(D3DTRANSFORMSTATE_WORLD, m_World);
//  Id3dDev.SetTransform(D3DTRANSFORMSTATE_VIEW, m_View);
//  Id3dDev.SetTransform(D3DTRANSFORMSTATE_PROJECTION, m_Proj);

  // Compute the composite World-View-Projection matrix.
  mWVP := D3DMath_MatrixMultiply(mWorld, mView);
  mWVP := D3DMath_MatrixMultiply(mWVP, mProj);

  // Set up the point in homogeneous coordinates.
  pt.X := x;
  pt.Y := y;
  pt.Z := z;

//  if Failed(D3DMath_VectorMatrixMultiply(clipPt, clipPt, mWVP)) then
//    begin
//    Result := False;
//    Exit;
//  end;


  // Transform the point into clip space.
  clipPt.X := mWVP._11 * pt.X + mWVP._21 * pt.Y + mWVP._31 * pt.Z + mWVP._41;
  clipPt.Y := mWVP._12 * pt.X + mWVP._22 * pt.Y + mWVP._32 * pt.Z + mWVP._42;
  clipPt.Z := mWVP._13 * pt.X + mWVP._23 * pt.Y + mWVP._33 * pt.Z + mWVP._43 ;
  w        := mWVP._14 * pt.X + mWVP._24 * pt.Y + mWVP._34 * pt.Z + mWVP._44 ;

  // Avoid division by zero.
  if w = 0 then
  begin
    Result := False;
    Exit;
  end;
//
//  // Perform perspective division to obtain normalized device coordinates (NDC).
//  clipPt.X := clipPt.X / w;
//  clipPt.Y := clipPt.Y / w;
//  clipPt.Z := clipPt.Z / w;

  // Map NDC [-1,1] to window coordinates.
  // In DirectX, NDC X and Y range from -1 to 1.
//  WinX := vp.dwX + ((clipPt.X + 1) * 0.5 * vp.dwWidth);
//  WinY := vp.dwY + ((1 - clipPt.Y) * 0.5 * vp.dwHeight);
//  WinZ := clipPt.Z;  // Depth remains in NDC

  WinX :=  clipPt.X;
  WinY :=  clipPt.Y;
  WinZ :=  w;

  Result := True;
end;

Procedure TD3D5PRenderer.GetWorldLine(X,Y: integer; var x1,y1,z1, x2,y2,z2: double);
var xv,yv,zv:TVector;
    px,py:double;
    vec:TVector;
begin
  // Set camera rotation
  yv := TVector.forward;
  //yv.SetCoords(0,1,0);

  with curCamera.rotation do
    begin
//      yv := view.lvec;   // Note, can occlude too much
//      zv := view.uvec;
//      xv := view.rvec;
//
      RotateVector(yv, -pitch, 0, 0);
      RotateVector(yv, 0, -yaw, 0);

      zv := TVector.up;
      //zv.SetCoords(0,0,1);
      RotateVector(zv, -pitch, 0, 0);
      RotateVector(zv, 0, -yaw, 0);

      xv := TVector.right;
      //xv.SetCoords(1,0,0);
      RotateVector(xv, -pitch, 0, 0);
      RotateVector(xv, 0, -yaw, 0);
    end;


  {The projection rectangle is 0.1x0.075 units and 0.05 units away
  from camera}

  px := (X - vwidth/2) / vwidth * 0.1;
  py := (vheight/2 - Y) / vheight * 0.075;

  with curCamera.position do
    begin
      x1 := x + px*xv.dx + py*zv.dx + 0.05 * yv.dx;
      y1 := y + px*xv.dy + py*zv.dy + 0.05 * yv.dy;
      z1 := z + px*xv.dz + py*zv.dz + 0.05 * yv.dz;

//      var tp := curCamera.view.TransformPoint(TVector.Create(px, py, 0.05)); // TODO: fix it; Note, curCamera.view can't be use because of too narrow view
//      x1 := tp.x;
//      y1 := tp.y;
//      z1 := tp.z;


      PlaneLineXnNew(yv, x + yv.dx * 5000, x + yv.dy * 5000, x + yv.dz * 5000,
                     x, y, z, x1, y1, z1, x2, y2, z2);
    end;

{

 X2:=CamX+px*xv.dx+py*zv.dx+5000*yv.dx;
 Y2:=CamY+px*xv.dy+py*zv.dy+5000*yv.dy;
 Z2:=CamZ+px*xv.dz+py*zv.dz+5000*yv.dz;}
end;
  //var mat: TD3DMaterial;


Procedure TD3D5PRenderer.SetClearColor(color: TColorF);
  var mat: TD3DMaterial;
begin
  FillChar(mat, sizeof(mat), 0);
  mat.dwsize := sizeof(mat);
  mat.diffuse.r := color.r;
  mat.diffuse.g := color.g;
  mat.diffuse.b := color.b;
  mat.diffuse.a := 1.0;
  mat.ambient.r := 0;
  mat.ambient.g := 0;
  mat.ambient.b := 0;
  mat.specular.r := 0;
  mat.specular.g := 0;
  mat.specular.b := 0;
  mat.dwrampsize := 1;
  backmat.SetMaterial(mat);
end;

Procedure TD3D5PRenderer.Redraw;
var bltfx: TDDBLTFX;
    res: Integer;
    r, sr: TRect;
    m_proj, m_view, m_world: TD3dMatrix;
    vec, zvec: TVector;
    rec: TD3DRECT;
begin
  curdxr:=self;
  DisableFPUExceptions;

  FillChar(bltfx,sizeof(bltfx),0);
  bltfx.dwSize := sizeof(bltfx);

  {res:=idsback.Blt(NIL,NIL,NIL,DDBLT_WAIT or DDBLT_COLORFILL,bltfx);}

  rec.x1 := 0;
  rec.y1 := 0;
  rec.x2 := vwidth;
  rec.y2 := vheight;
  res := iview.Clear(1, rec, D3DCLEAR_TARGET or D3DCLEAR_ZBUFFER);
  DXCheck(res, 'Clearing viewport');

  m_World := IdentityMatrix;
  m_World._11 := -1;

  {MatrixMul(TranslateMatrix(CamX,CamY,CamZ),RotateXmatrix(-pi/2));}

  vec := TVector.forward;
  //vec.SetCoords(0,1,0);
  with curCamera.rotation do
    begin
      RotateVector(vec, -pitch, 0, 0);
      RotateVector(vec, 0, -yaw, 0);
    end;

  { RotateVector(zvec,-PCH,0,0);
  RotateVector(zvec,0,-YAW,0);}

  zvec := TVector.up;
  //zvec.SetCoords(0, 0, 1);
  if Abs(vec.dz) > 0.99 then
    begin
      zvec := TVector.right.Cross(vec);
      //VectorCross3(1, 0, 0, vec.dx, vec.dy, vec.dz, zvec.dx, zvec.dy, zvec.dz); //Vmult
      RotateVector(zvec, 0, curCamera.rotation.yaw, 0);
    end;

  with curCamera.position do
    m_View := ViewMatrix(
     D3DVECTOR(-x, y, z),
     D3DVECTOR( -x - vec.dx, y + vec.dy, z + vec.dz),
     D3DVECTOR(zvec.dx, zvec.dy, zvec.dz),
     0);

  with curCamera.frustum do
    m_Proj := ProjectionMatrix(curCamera.fov, curCamera.aspect,
      nearPlane.distance, farPlane.distance);
      //nearPlane.distance, farPlane.distance, (curCamera.fov * PI / 180));

  Id3dDev.SetTransform(D3DTRANSFORMSTATE_WORLD, m_World);
  Id3dDev.SetTransform(D3DTRANSFORMSTATE_VIEW, m_View);
  Id3dDev.SetTransform(D3DTRANSFORMSTATE_PROJECTION, m_Proj);

  res := id3ddev.BeginScene;
  DXCheck(res, 'Beginnning scene');

  // parent renders the scene
  inherited Redraw;

  res := id3ddev.EndScene;
  DXCheck(res, 'Ending scene');

  GetClientRect(whandle, sr);
  { r:=sr;}

  ClientToScreen(whandle, sr.topleft);
  ClientToScreen(whandle, sr.bottomright);

  {r.left:=0; r.top:=0;
  r.bottom:=vheight-1; r.right:=vwidth-1;}
  {res:=idsback.Blt(NIL,NIL,NIL,DDBLT_WAIT or DDBLT_COLORFILL,bltfx);}

  res := ids.Blt(@sr, idsback, nil, DDBLT_WAIT, bltfx);

  if res = DDERR_SURFACELOST then
    begin
      ids.Restore;
      idsback.Restore;
    end;
  DDCheck(res, 'Blitting from back to front');

{ res:=ids.BltFast(sr.left,sr.top, idsback, Trect(nil^), DDBLTFAST_WAIT or DDBLTFAST_NOCOLORKEY);}
end;

{Texture}

Function Min(i1,i2:integer):integer;
begin
 if i1<i2 then result:=i1 else result:=i2;
end;

Constructor TD3DTexture.CreateFromMat(const Mat: string;const ppal: PTCMPPal; const pcmp: PTCMPTable; adxr: TD3D5PRenderer; gamma: double);
var
    i,j:integer;
    pb:PAnsiChar;
    mf:TMat;
    f:TFile;
    res, n:integer;
{    bits:PAnsiChar;}
    c:AnsiChar;
    w:word;
    usepalette:boolean;
 var ddsd:TDDSURFACEDESC;
     ads:IDirectDrawSurface;
     bltfx:TDDBLTFX;
     texFmt: TTexFormat;

  Procedure LoadPaletted;
  var i,j: integer;
      pl: PAnsiChar;
  begin
     GetMem(pl,width);
     for i:=0 to height-1 do
       begin
          mf.ReadRow(pl^);
          for j:=0 to width-1 do
            (pl+j)^ := AnsiChar(Chr(pcmp[ord((pl+j)^)]));
          Move(pl^, pb^,width);
          inc(pb,ddsd.lPitch);
       end;
     FreeMem(pl);
  end;

  Procedure LoadRGB;
//  var bpp:integer;
//      rsh,gsh,bsh:integer;
//      rpsh,gpsh,bpsh:integer;
//      i,j:integer;
//      pw:^word;
  begin
    if mf.Info.ci.mode = Indexed then
      begin
        if ppal = nil then
        begin
          PanMessage(mt_warning,
              Format('TD3DTexture: LoadRGB failed for MAT file %s. No pallette in use!',[mat]));
          exit;
        end;
        gamma := 1.0; // See AdjustPalGamma
      end;

    // Read texture pixelmap
    mf.ReadImage(pb, texFmt.ci, ddsd.lPitch, ppal, gamma);
  end;

  Procedure SetSD(var ddsd:TDDSURFACEDESC;hw:boolean);
  begin
   FillChar(ddsd, sizeof(ddsd),0);
   ddsd.dwSize := sizeof(ddsd);
   ddsd.dwFlags := DDSD_CAPS or DDSD_WIDTH or DDSD_HEIGHT or DDSD_PIXELFORMAT;
   if usepalette then
     begin
      //ddsd.dwFlags:=ddsd.dwFlags or DDSD_PIXELFORMAT;
      ddsd.ddpfPixelFormat.dwSize:=sizeof(ddsd.ddpfPixelFormat);
      ddsd.ddpfPixelFormat.dwFlags:=DDPF_PALETTEINDEXED8 or DDPF_RGB;
    {  ddsd.ddpfPixelFormat.dwZBufferBitDepth:=8;
      ddsd.ddpfPixelFormat.dwAlphaBitDepth:=8;}
      ddsd.ddpfPixelFormat.dwRGBBitCount:=8;
     end
   else
     begin
      ddsd.ddpfPixelFormat := texFmt.ddpfPixelFormat;
    end;

   ddsd.dwWidth := width;
   ddsd.dwHeight := height;
   if hw then ddsd.ddsCaps.dwCaps := DDSCAPS_TEXTURE or DDSCAPS_VIDEOMEMORY //or DDSCAPS_3DDEVICE or DDSCAPS_PALETTE or DDSCAPS_OFFSCREENPLAIN or DDSCAPS_BACKBUFFER // or  <-- TODO
   else ddsd.ddsCaps.dwCaps := DDSCAPS_TEXTURE or DDSCAPS_SYSTEMMEMORY;
  end;

begin
  // Read MAT file
  dxr := adxr;
  f   := OpenGameFile(mat);
  mf  := TMat.Create(f, 0);

  try
    begin
      // Get txt format
      usepalette := (ppal <> nil) and (mf.info.IsIndexed) and dxr.PalettedTextures;
      if not usepalette then
      begin
        var cmode := TColorMode.RGB;
        if mf.Info.ci.mode = RGBA then
          cmode := RGBA;

        texFmt := dxr.texFormats.First;
        for var e in dxr.texFormats do
        begin
          if e.ci.mode = cmode then
            begin
              texFmt := e;
              break;
            end;
        end;

        if (texFmt = nil)then
        begin
          PanMessage(mt_warning,
            Format('TD3DTexture: LoadRGB failed for MAT file %s. No DX texture format available!',[mat]));
          exit;
        end;
      end;

      name   := ExtractFileName(mat);
      ci     := mf.Info.ci;
      width  := mf.Info.width;
      height := mf.Info.Height;

      {Create buffer surface}
      SetSD(ddsd,false);
      Res:=dxr.idd2.CreateSurface(ddsd, ads, NIL);
      DDFailCheck(res,'Creating buffer');

      {Set Palette}
      if usepalette then
        begin
          res:=ads.SetPalette(dxr.GetD3DPalette(ppal^));
          DDFailCheck(res,'Setting buffer palette');
        end;

      {Set surface}
      res:=ads.Lock(nil, ddsd, DDLOCK_NOSYSLOCK or DDLOCK_SURFACEMEMORYPTR or DDLOCK_WRITEONLY or DDLOCK_WAIT, 0);
      DDFailCheck(res, 'Locking buffer surface');

      pb := ddsd.lpSurface;
      if usepalette then
        LoadPaletted
      else
        LoadRGB;

      res := ads.UnLock(nil);
      DDFailCheck(res, 'Unlocking buffer surface');

      { res:=itxs.GetDC(dc);
      DDFailCheck(res,'Getting texture DC');
      bm:=mf.LoadBitmap(width,height);
      res:=Integer(Windows.BitBlt(dc,0,0,width,height,bm.canvas.handle,0,0,SRCCOPY));
      res:=itxs.ReleaseDC(dc);
      DDCheck(res,'Releasing texture DC');

      mf.free;
      bm.free;}

      if not dxr.hardware then itxs:=ads
      else
        begin
          SetSD(ddsd,true);
          Res:=dxr.idd2.CreateSurface(ddsd, itxs, NIL);
          DDFailCheck(res,'Creating texture');
          if usepalette then
            begin
             res:=itxs.SetPalette(dxr.GetD3DPalette(ppal^));
             DDFailCheck(res,'Setting texture palette');
            end;

          FillChar(bltfx,sizeof(bltfx),0);
          bltfx.dwSize := sizeof(bltfx);
          res:=itxs.Blt( nil, ads, nil, DDBLT_WAIT, bltfx);
          DDCheck(res,'Blitting from buffer to texture');
          ads.Release;
        end;

      res:=itxs.QueryInterface(IID_IDirect3DTexture2, itx);
      DXFailCheck(res,'Getting IDirect3DTexture2 interface');
    end
  finally
    begin
      mf.Free;
    end;
  end;
end;
Procedure TD3DTexture.SetCurrent;
var res: DWORD;
    handle:TD3DTextureHandle;
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

