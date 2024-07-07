unit D3D_PRender;

interface

uses
  Render, J_Level, Windows, OLE2, SysUtils, Forms, Classes,
  (*D3DRMObj,*) D3DTypes, (*D3DRMDef,*) DDraw, D3D, (*D3Drm,*) dxtools,
  (*D3DrmWin,*) D3Dcaps, Graph_files, Files, FileOperations,
  Messages, GlobalVars, misc_utils, u_3dos, ExtCtrls, Prender,
  u_pj3dos, images, geometry;

//Type

//  TD3DTexture = class
//    ITexture: IDirect3DRMTexture;
//    imh: TD3DRMIMAGE;
//    cmpname: string;
//    Constructor CreateFromMat(const Mat: string; ppal: PTCMPPal;
//      D3Drm: IDirect3DRM; gamma: double);
//    Destructor Destroy; override;
//  end;

//  TD3DRenderer = class(TPreviewRender)
//    DDBD: dword;
//    FBPP: dword;
//     elaps: Uint32;
//    FDriverIndex: Integer;
//    FDriverGUID: PGUID;
//
//    FD3DRM: IDirect3DRM;
//    FClip: IDirectDrawClipper;
//    FDev: IDirect3DRMDevice;
//    FWDev: IDirect3DRMWinDevice;
//
//    FCamera: IDirect3DRMFrame;
//    FScene: IDirect3DRMFrame;
//    FView: IDirect3DRMViewport;
//    FSubScene: IDirect3DRMFrame;
//    nomore: boolean;
//    TXList, CmpList: TStringList;
//    sectors: TSectors;
//    Meshes: TList;
//    things: TThings;
//    THMeshes: TList;
//    Constructor Create(aForm: TForm);
//    Constructor CreateFromPanel(aPanel: TPanel);
//    Procedure Initialize; override;
//    Procedure ClearSectors; override;
//    Procedure ClearThings; override;
//    Procedure AddSector(s: TJKSector); override;
//    Procedure DeleteSector(s: TJKSector); override;
//    Procedure UpdateSector(s: TJKSector); override;
//    Procedure AddThing(th: TJKThing); override;
//    Procedure DeleteThing(th: TJKThing); override;
//    Procedure UpdateThing(th: TJKThing); override;
//    Procedure SetViewToThing(th: TJKThing); override;
//    Procedure SetPCHYAW(pch, yaw: double); override;
//
//    Procedure SetViewPort(x, y, w, h: Integer); override;
//    Procedure Redraw; override;
//    Destructor Destroy; override;
//    Procedure HandleActivate(const msg: TMessage); override;
//    procedure HandlePaint(hdc: HDC); override;
//    Function PickAt(x, y: Integer): TPickType; override;
//
//    Function SetGamma(gamma: double): Integer; override;
//    Procedure SetThing3DO(th: TJKThing; a3do: TPJ3DO); override;
//  Private
//    Procedure DeleteMesh(mesh: IDirect3DRMMesh);
//    Function SecToMesh(s: TJKSector): IDirect3DRMMesh { Builder };
//    Function ThingToMesh(th: TJKThing): IDirect3DRMMesh;
//    Function BadThingMesh(th: TJKThing): IDirect3DRMMesh;
//    Function PJ3DOToMesh(th: TJKThing; a3do: TPJ3DO): IDirect3DRMMesh;
//    Procedure InitializeDevice;
//    Procedure SetDriver(n: Integer);
//    procedure RebuildDevice(bForce: boolean);
//    Procedure ClearTXList;
//    Function LoadD3DTexture(const name, cmp: string): TD3DTexture;
//    function createdevandview(driver: Integer; width, height: Integer): boolean;
//  end;
//
type
  TD3DDriverInfo = record
    id: TGUID;
    DeviceDescription: string;
    DeviceName: string;
    HWDeviceDesc: TD3DDeviceDesc;
    HELDeviceDesc: TD3DDeviceDesc;
  end;

const
  MAXDRIVERS = 16;

Const
  directx5: boolean = false;

var
  { a few global variables storing driver info }
  _D3Ddrivers: array [0 .. MAXDRIVERS - 1] of TD3DDriverInfo;
  _D3DdriverCount: Integer;
  _bDriversInitialized: boolean;

Procedure EnumDevices;
Function GetDeviceNum(const name: string): Integer;
Procedure DXFailCheck(r: HResult; const msg: string);
Procedure DDFailCheck(r: HResult; const msg: string);
procedure DXCheck(r: HResult; const msg: string);
procedure DDCheck(r: HResult; const msg: string);

implementation

uses Lev_utils;

Procedure DXFailCheck(r: HResult; const msg: string);
var
  s: string;
begin
  if r = DD_OK then
    exit;
  raise EDirectX.CreateFmt('%s %s', [D3DErrorString(r), msg]);
end;

Procedure DDFailCheck(r: HResult; const msg: string);
var
  s: string;
begin
  if r = DD_OK then
    exit;
  raise EDirectX.CreateFmt('%s %s', [DDRAWErrorString(r), msg]);
end;

Procedure COMRelease(iu: IUnknown);
begin
  if iu <> nil then
    iu.Release;
end;

procedure DXCheck(r: HResult; const msg: string);
begin
  if r = DD_OK then
    exit;
  PanMessage(mt_warning, D3DErrorString(r) + ' ' + msg);
end;

procedure DDCheck(r: HResult; const msg: string);
begin
  if r = DD_OK then
    exit;
  PanMessage(mt_warning, DDRAWErrorString(r) + ' ' + msg);
end;

Function Min(i1, i2: Integer): Integer;
begin
  if i1 < i2 then
    result := i1
  else
    result := i2;
end;

function _EnumCallBack(const lpGuid: TGUID; lpDeviceDescription: LPSTR;
  lpDeviceName: LPSTR; const lpD3DHWDeviceDesc: TD3DDeviceDesc;
  const lpD3DHELDeviceDesc: TD3DDeviceDesc; lpUserArg: pointer)
  : HResult; stdcall;
var
  dev: ^TD3DDeviceDesc;
  DDBD: dword;
begin
  dev := @lpD3DHWDeviceDesc;
  DDBD := dword(lpUserArg);
  result := D3DENUMRET_OK;
  if Integer(lpD3DHWDeviceDesc.dcmColorModel) = 0 then
    dev := @lpD3DHELDeviceDesc;

//   if ( memcmp(lpGuid, &IID_IDirect3DNullDevice, 0x10u) = 0 )
//      return 1;

   if CompareMem(@lpGuid, @IID_IDirect3DRGBDevice, 16) then// emulated device - HEL
      exit;
   if CompareMem(@lpGuid, @IID_IDirect3DRampDevice, 16) then
      exit;
   if CompareMem(@lpGuid, @IID_IDirect3DMMXDevice, 16) then
      exit;
    // TODO
//    if ( !memcmp(lpGuid, &IID_IDirect3DRefDevice, 0x10u) )
//      return 1;

  if (dev^.dwDeviceRenderBitDepth and DDBD) <> 0 then
  begin
    { current bit depth is supported by this driver }
    with _D3Ddrivers[_D3DdriverCount] do
    begin
      Move(lpGuid, id, sizeof(TGUID));
      Move(lpD3DHWDeviceDesc, HWDeviceDesc, sizeof(TD3DDeviceDesc));
      Move(lpD3DHELDeviceDesc, HELDeviceDesc, sizeof(TD3DDeviceDesc));
      DeviceDescription := StrPas(lpDeviceDescription);
      DeviceName := StrPas(lpDeviceName);
    end;
    inc(_D3DdriverCount)
  end;

  if _D3DdriverCount >= MAXDRIVERS then
    result := D3DENUMRET_CANCEL;
end;

procedure _InitializeDrivers(Bdepth: dword);
var
  D3D: IDirect3D;
  d3d2: IDirect3D2;
  dd: IDirectDraw;
begin
  D3D := nil;
  if not _bDriversInitialized then
  begin
    DDFailCheck(DirectDrawCreate(nil, dd, nil), 'in _InitializeDrivers');
    try
      directx5 := true;
      if dd.QueryInterface(IID_IDirect3D2, d3d2) <> DD_OK then
      begin
        DXFailCheck(dd.QueryInterface(IID_IDirect3D, D3D),
          'in _InitializeDrivers');
        directx5 := false;
      end;

      try
        _bDriversInitialized := true;
        if d3d2 = nil then
          DXFailCheck(D3D.EnumDevices(_EnumCallBack, pointer(Bdepth)),
            'in _InitializeDrivers')
        else
          DXFailCheck(d3d2.EnumDevices(_EnumCallBack, pointer(Bdepth)),
            'in _InitializeDrivers');

      finally
        if D3D <> nil then
          COMRelease(D3D);
        if d3d2 <> nil then
          COMRelease(d3d2);
      end;
    finally
      dd.Release;
    end;
  end;
end;

Procedure EnumDevices;
begin
  if _D3DdriverCount = 0 then
    _InitializeDrivers(DDBD_24 or DDBD_32);
    //_InitializeDrivers(DDBD_8 + DDBD_16 + DDBD_24);
end;

Function GetDeviceNum(const name: string): Integer;
var
  i: Integer;
begin
  result := 0;
  for i := 0 to _D3DdriverCount - 1 do
    With _D3Ddrivers[i] do
    begin
      if CompareText(DeviceName, name) = 0 then
      begin
        result := i;
        exit;
      end;
    end;
end;


//Constructor TD3DTexture.CreateFromMat(const Mat: string; ppal: PTCMPPal;
//  D3Drm: IDirect3DRM; gamma: double);
//var
//  i: Integer;
//  pe: ^TD3DRMPALETTEENTRY;
//  pb: PAnsiChar;
//  mf: TMat;
//  f: TFile;
//  pw: ^word;
//begin
//  f := OpenGameFile(Mat);
//  mf := TMat.Create(f, 0);
//
//  // TODO: implement 24,32 bpp
//  if mf.info.storedAs = ByLines16 then
//    With imh do
//    begin
//      width := mf.info.width;
//      height := mf.info.height;
//      bytes_per_line := width * 2;
//      aspectx := 1;
//      aspecty := 1;
//      depth := 16;
//      rgb := 1;
//      GetMem(pb, width * height * 2);
//      buffer1 := pb;
//      mf.LoadBits(pb^);
//
//      pw := pointer(pb);
//      for i := 0 to width * height - 1 do
//      begin
//        pw^ := Min(Round((pw^ and $1F) * gamma), 31) +
//          Min(Round((pw^ shr 5 and $3F) * gamma), 63) shl 5 +
//          Min(Round((pw^ shr 11 and $1F) * gamma), 31) shl 11;
//        inc(pw);
//      end;
//
//      buffer2 := nil;
//      red_mask := $F800;
//      green_mask := $7E0;
//      blue_mask := $1F;
//      alpha_mask := 0;
//      palette_size := 0;
//      palette := NIL;
//    end
//  else if ppal <> nil then
//    With imh do
//    begin
//      width := mf.info.width;
//      height := mf.info.height;
//      bytes_per_line := width;
//      aspectx := 1;
//      aspecty := 1;
//      depth := 8;
//      rgb := 0;
//      GetMem(pb, width * height);
//      buffer1 := pb;
//
//      mf.LoadBits(pb^);
//      { for i:=0 to height-1 do
//        begin
//        mf.GetLine((pb+i*width)^);
//        end; }
//
//      buffer2 := nil;
//      red_mask := $FF;
//      green_mask := $FF;
//      blue_mask := $FF;
//      alpha_mask := 0;
//      palette_size := 256;
//      GetMem(palette, sizeof(TD3DRMPALETTEENTRY) * 256);
//      for i := 0 to 255 do
//        With ppal[i] do
//        begin
//          pe := pointer(PAnsiChar(palette) + i * sizeof(TD3DRMPALETTEENTRY));
//          pe^.red := Min(Round(r * gamma), 255);
//          pe^.green := Min(Round(g * gamma), 255);
//          pe^.blue := Min(Round(b * gamma), 255);
//          pe^.flags := 0; { D3DRMPALETTE_FREE }
//        end;
//    end
//    else
//      PanMessageFmt(mt_warning, 'TD3DTexture: No pallete was set when loading material: %s!', [Mat]);
//
//  mf.free;
//  DXCheck(D3Drm.CreateTexture(imh, ITexture), 'in TD3DTexture.CreateFromMat');
//  if ITexture <> nil then
//    begin
//      ITexture.SetColors(256);
//      ITexture.SetShades(64);
//    end;
//end;
//
//Destructor TD3DTexture.Destroy;
//begin
//  if ITexture <> nil then
//    COMRelease(ITexture);
//  FreeMem(imh.buffer1);
//  if imh.palette <> nil then
//    FreeMem(imh.palette);
//end;
//



//
//Constructor TD3DRenderer.CreateFromPanel(aPanel: TPanel);
//begin
//  Whandle := aPanel.handle;
//  VWidth := aPanel.width;
//  VHeight := aPanel.height;
//  gamma := 1;
//  TXList := TStringList.Create;
//  TXList.Sorted := true;
//  CmpList := TStringList.Create;
//  CmpList.Sorted := true;
//  sectors := TSectors.Create;
//  Meshes := TList.Create;
//  things := TThings.Create;
//  THMeshes := TList.Create;
//end;
//
//Constructor TD3DRenderer.Create(aForm: TForm);
//begin
//  Inherited Create(aForm);
//  TXList := TStringList.Create;
//  TXList.Sorted := true;
//  CmpList := TStringList.Create;
//  CmpList.Sorted := true;
//  sectors := TSectors.Create;
//  Meshes := TList.Create;
//  things := TThings.Create;
//  THMeshes := TList.Create;
//end;
//

//Procedure TD3DRenderer.Initialize;
//var
//  lightframe: IDirect3DRMFrame;
//  light1, light2: IDirect3DRMLight;
//  dc: hdc;
//begin
//elaps:= 0;
//  { retrieve bits per pixel }
//  DisableFPUExceptions;
//  DXFailCheck(Direct3DRMCreate(FD3DRM), 'in Initialize');
//
//  DDBD := DDBD_8 + DDBD_16 + DDBD_24;
//  dc := GetDC(Whandle);
//  FBPP := GetDeviceCaps(dc, BITSPIXEL);
//  ReleaseDC(Whandle, dc);
//
//  EnumDevices;
//
//  { _InitializeDrivers(DDBD_8+DDBD_16+DDBD_24); }
//
//  { create scene }
//  DXFailCheck(FD3DRM.CreateFrame(nil, FScene),
//    'in Initialize when creating a scene');
//  DXFailCheck(FD3DRM.CreateFrame(FScene, FCamera),
//    'in Initialize when creating camera');
//  DXFailCheck(FCamera.SetPosition(FScene, 0.0, 0.0, 0.0),
//    'in Initialize when setting camera');
//
//  DXFailCheck(DirectDrawCreateClipper(0, FClip, nil),
//    'in Initialize when creating clipper');
//  DXFailCheck(FClip.SetHWnd(0, Whandle), 'in Initialize when setting window');
//
//  if not createdevandview(GetDeviceNum(D3DDevice), VWidth, VHeight) then;
//
//  DXFailCheck(FD3DRM.CreateFrame(FScene, FSubScene),
//    'in Initialize when creating subscene');
//
//  { DXFailCheck(Fd3drm.createframe(Fscene,lightframe));
//    DXFailCheck(Fd3drm.createlightrgb(d3drmlight_directional,0,0,0,light1));
//    DXFailCheck(lightframe.addlight(light1));
//
//    DXFailCheck(Fd3drm.createlightrgb(d3drmlight_ambient,0.9,0.9,0.9,light2));
//    DXFailCheck(Fscene.addlight(light2));
//    DXFailCheck(lightframe.setposition(Fscene,0,0,0));
//    DXFailCheck(lightframe.setrotation(Fscene,0.1,2.5,3.5,4.5));
//    DXFailCheck(lightframe.move(10));
//    DXFailCheck(lightframe.setrotation(Fscene,0,0,0,0));
//    ComRelease(IUnknown(Light1));
//    ComRelease(IUnknown(Light2)); }
//
//  DXFailCheck(FCamera.SetPosition(FScene, 0, 0, 0),
//    'in Initialize when setting camera');
//  DXFailCheck(FCamera.setorientation(FScene, 0, 0, 1, 0, 1, 0),
//    'in Initialize when setting camera');
//
//  DXFailCheck(FSubScene.SetPosition(FScene, 0, 0, 0),
//    'in Initialize when setting scene');
//  DXFailCheck(FSubScene.setorientation(FScene, 0, 1, 0, 0, 0, 1),
//    'in Initialize when setting scene');
//
//end;
//
//procedure TD3DRenderer.InitializeDevice;
//const
//  Idev: string = 'in InitializeDevice';
//var
//  w, h: Integer;
//  r: HResult;
//begin
//  DXFailCheck(FD3DRM.CreateDeviceFromClipper(FClip, FDriverGUID, VWidth,
//    VHeight, FDev), 'in InitializeDevice when creating device');
//  DXFailCheck(FDev.QueryInterface(IID_IDirect3DRMWinDevice, FWDev), Idev);
//  FClip.SetHWnd(0, whandle);
//  w := FDev.GetWidth;
//  h := FDev.GetHeight;
//  r := FD3DRM.CreateViewport(FDev, FCamera, 0, 0, w, h, FView);
//  if r <> D3D_OK then
//  begin
//    FWDev.Release;
//    FDev.Release;
//    DXCheck(r, Idev);
//  end;
//  r := FView.SetBack(5000.0);
//  if r <> D3D_OK then
//  begin
//    FView.Release;
//    FWDev.Release;
//    FDev.Release;
//  end;
//
//  { set render quality }
//  DXCheck(FDev.SetQuality(D3DRMFILL_SOLID or D3DRMSHADE_FLAT or
//    d3drmlight_ON), Idev);
//  DXCheck(FDev.SetTextureQuality(d3drmtexture_nearest), Idev);
//
//  FDev.GetQUality;
//
//  { render quality part 2 - standard incantation }
//  if FBPP = 1 then
//  begin
//    DXCheck(FDev.SetShades(4), Idev);
//    DXCheck(FD3DRM.SetDefaultTextureShades(4), Idev);
//  end
//  else if FBPP = 16 then
//  begin
//    DXCheck(FDev.SetShades(32), Idev);
//    DXCheck(FD3DRM.SetDefaultTextureColors(64), Idev);
//    DXCheck(FD3DRM.SetDefaultTextureShades(32), Idev);
//  end
//  else if (FBPP = 24) or (FBPP = 32) then
//  begin
//    DXCheck(FDev.SetShades(256), 'in InitializeDevice');
//    DXCheck(FD3DRM.SetDefaultTextureColors(64), 'in InitializeDevice');
//    DXCheck(FD3DRM.SetDefaultTextureShades(256), 'in InitializeDevice');
//  end;
//
//end;
//
//Procedure TD3DRenderer.Redraw;
//begin
////    var lpVisuals: IDirect3DRMVisualArray;
////    FSubScene.GetVisuals(lpVisuals);
////    for var i:=lpVisuals.GetSize-1 downto 0 do
////      begin
////        var lpVisual: IDirect3DRMVisual;
////        lpVisuals.getElement(i,lpVisual);
////        FSubScene.DeleteVisual(lpVisual);
////      end;
//  { FScene.Move(0); }
//  DisableFPUExceptions;
//
//  //FCamera.SetColor($FF00FF00);
//
//  FScene.SetSceneBackground($0000BB);
//
//  //FSubScene.SetSceneBackground($FF00AA00);
//
//  DXCheck(FView.Clear, 'in TD3dRender.Redraw - clear');
//
//  FCamera.AddRotation(D3DRMCOMBINE_BEFORE, 1, 0, 0, pch / 180 * PI);
//  FCamera.AddRotation(D3DRMCOMBINE_BEFORE, 0, 1, 0, yaw / 180 * PI);
//  FCamera.AddTranslation(D3DRMCOMBINE_AFTER, CamX, CamZ, CamY);
//
//  { FCamera.AddScale(D3DRMCOMBINE_BEFORE,scale,scale,scale); }
//
////  FCamera.AddRotation(D3DRMCOMBINE_BEFORE, 1, 0, 0, pch / 180 * PI);
////  FCamera.AddRotation(D3DRMCOMBINE_BEFORE, 0, 1, 0, yaw / 180 * PI);
////  FCamera.AddTranslation(D3DRMCOMBINE_AFTER, CamX, CamZ, CamY);
//  { FCamera.AddRotation(D3DRMCOMBINE_BEFORE,0,0,1,PI); }
//  try
//    try
//
//      DXCheck(FView.Render(FScene), 'in TD3dRender.Redraw - render');
//      DXCheck(FView.ForceUpdate(0, 0, self.vwidth, self.vheight), 'in TD3dRender.Redraw - f render');
//
//    finally
//      DXCheck(FDev.Update, 'in TD3dRender.Redraw - update');
//      DXCheck(FD3DRM.Tick(elaps), 'in TD3dRender.Redraw - Tick');
//      elaps := elaps + 1;
//
//    end;
//  except
//    on exception do;
//  end;
//end;
//
//Function TD3DRenderer.PickAt(x, y: Integer): TPickType;
//var
//  pa: IDIRECT3DRMPICKEDARRAY;
//  pd: TD3DRMPICKDESC;
//  iv: IDirect3DRMVisual;
//  fa: IDirect3DRMFrameArray;
//  i, nsc, nth: Integer;
//begin
//  result := pk_nothing;
//  FView.Pick(x, y, pa);
//  Try
//    if pa.GetSize = 0 then
//      exit;
//    pa.GetPick(0, iv, fa, pd);
//    fa.Release;
//    nth := -1;
//    nsc := Meshes.IndexOf(iv);
//    if nsc = -1 then
//      nth := THMeshes.IndexOf(iv);
//    iv.Release;
//    if (nsc = -1) and (nth = -1) then
//      exit;
//
//    if nsc <> -1 then
//    begin
//      selSC := sectors[nsc];
//      if Level.sectors.IndexOf(selSC) = -1 then
//      begin
//        PanMessage(mt_warning,
//          '3D preview is out of sync with the level! Reload it');
//        exit;
//      end;
//      for i := 0 to selSC.surfaces.count - 1 do
//        if selSC.surfaces[i].D3DID = pd.lGroupIdx then
//        begin
//          SelSF := i;
//          result := pk_surface;
//          break;
//        end;
//      exit;
//    end;
//
//    SelTH := things[nth];
//    result := pk_thing;
//
//  finally
//    if pa <> nil then
//      pa.Release;
//  end;
//end;
//
//Procedure TD3DRenderer.SetViewPort(x, y, w, h: Integer);
//begin
//  FView.Configure(x, y, w, h);
//end;
//
//(* Function TD3DRenderer.SecToMesh(s:TJKSector):IDirect3DRMMeshBuilder;
//  const
//  bsize=2048;
//  var i,j:integer;
//  Mb:IDirect3DRMMeshBuilder;
//  Vxs:LPD3DVector;
//  cvx:LPD3DVector;
//  Norm:D3DVector;
//  face:IDirect3DRMFace;
//  faces:IDiRECT3DRMFACEARRAY;
//  cface,nfaces:integer;
//  p:integer;
//  nvx:integer;
//  ttx:TD3DTexture;
//  pim:LPD3DRMIMAGE;
//  group:array[0..bsize-1] of dword;
//
//  begin
//  nvx:=0; p:=0;
//
//  for i:=0 to s.surfaces.count-1 do
//  With s.surfaces[i] do
//  begin
//  if Material='' then continue;
//  group[p]:=Vertices.count; inc(p);
//  for j:=0 to vertices.count-1 do
//  begin
//  group[p]:=nvx+j;
//  inc(p);
//  end;
//  inc(nvx,Vertices.count);
//  end;
//
//  group[p]:=0;
//
//  GetMem(vxs,nvx*sizeof(D3DVector));
//
//
//  { GetMem(norms,s.surfaces.count*sizeof(D3DVector));}
//  try
//  DXCheck(FD3DRM.CreateMeshBuilder(Mb));
//  mb.SetColorSource(D3DRMCOLOR_FROMVERTEX);
//
//  cvx:=vxs;
//
//  for i:=0 to s.surfaces.count-1 do
//  With s.surfaces[i] do
//  begin
//  if Material='' then continue;
//  for j:=0 to vertices.count-1 do
//  With Vertices[j] do
//  begin
//  cvx^.x:=x;
//  cvx^.y:=y;
//  cvx^.z:=z;
//  inc(cvx);
//  end;
//  end;
//
//  DXCheck(mb.AddFaces(nvx,vxs^,0,norm,group[0],faces));
//
//
//  cface:=0; p:=0;
//
//  For i:=0 to s.surfaces.count-1 do
//  With s.Surfaces[i] do
//  begin
//  if Material='' then continue;
//  faces.GetElement(cface,face);
//  ttx:=LoadD3DTexture(Material,s.ColorMap);
//  face.SetColorRGB(1,1,1);
//  for j:=0 to TXvertices.count-1 do
//  With TXVertices[j] do
//  begin
//  DXCheck(mb.SetVertexColorRGB(p+j,Intensity,Intensity,Intensity));
//  face.SetTextureCoordinates(j,u/ttx.imh.width,v/ttx.imh.height);
//  end;
//
//  DXCheck(Face.SetTexture(ttx.Itexture));
//
//  ComRelease(IUNknown(face));
//  inc(cface); inc(p,Vertices.Count);
//  end;
//
//  ComRelease(IUnknown(faces));
//  finally
//  FreeMem(vxs);
//  { FreeMem(norms);}
//  end;
//  mb.SetQuality(D3DRMFILL_SOLID or D3DRMSHADE_GOURAUD or d3drmlight_off);
//  mb.SetPerspective(true);
//  {mb.GenerateNormals;}
//  Result:=mb;
//  end; *)
//
////Function GetEncodedColor(color: TColorF): Longint;
////var
////  ii: byte;
////  br, bg, bb: byte;
////begin
////  br := ClampI(Round(color.r * 255), 0, 255);
////  bg := ClampI(Round(color.g * 255), 0, 255);
////  bb := ClampI(Round(color.b * 255), 0, 255);
////  result := (bb or (bg shl 8) or (br shl 16));
////end;
//
//Function GetJKColor(inten, elight: single): Longint;
//var
//  ii: byte;
//begin
//  if (inten + elight) < 0 then
//  begin
//    result := 0;
//    exit;
//  end;
//  ii := Min(Round((inten + elight) * 255), 255);
//  result := ii + ii shl 8 + ii shl 16;
//end;
//
//Function GetMOTSColor(r, g, b, elight: single): Longint;
//var
//  ii: byte;
//  br, bg, bb: byte;
//begin
//  if (r + elight) < 0 then
//    br := 0
//  else
//    br := Min(Round((r + elight) * 255), 255);
//  if (g + elight) < 0 then
//    bg := 0
//  else
//    bg := Min(Round((g + elight) * 255), 255);
//  if (b + elight) < 0 then
//    bb := 0
//  else
//    bb := Min(Round((b + elight) * 255), 255);
//  result := bb + bg shl 8 + br shl 16;
//end;
//
//Function TD3DRenderer.SecToMesh(s: TJKSector): IDirect3DRMMesh;
//const
//  maxvx = 24;
//var
//  i, j, nv: Integer;
//  mesh: IDirect3DRMMesh;
//  Vxs: array [0 .. maxvx - 1] of TD3DRMVertex;
//  cvx: ^TD3DRMVertex;
//  nvx: Integer;
//  id: TD3DRMGROUPINDEX;
//  ttx: TD3DTexture;
//  pim: PD3DRMIMAGE;
//  group: array [0 .. 1024] of dword;
//  nsurfs: Integer;
//begin
//  try
//    DXFailCheck(FD3DRM.CreateMesh(mesh), 'in SecToMesh when creating mesh');
//
//    nsurfs := 0;
//
//    For i := 0 to maxvx - 1 do
//      group[i] := i;
//
//    for i := 0 to s.surfaces.count - 1 do
//      With s.surfaces[i] do
//      begin
//        if (s.flags and SECF_3DO = 0) then
//          if (geo = 0) or (Material = '') then
//          begin
//            D3DID := -1;
//            continue;
//          end;
//
//        nvx := Vertices.count;
////        if nvx > 24 then
////          nvx := 24;
//
//        DXCheck(mesh.AddGroup(nvx, 1, nvx, group[0], id),
//          'when adding group in SecToMesh');
//
//        D3DID := id;
//
//        ttx := nil;
//
//        Try
//          ttx := LoadD3DTexture(Material, s.ColorMap);
//          if ttx <> nil then
//            mesh.SetGroupTexture(id, ttx.ITexture);
//        except
//          on exception do
//            PanMessage(mt_warning,
//              Format('Cannot load %s for sector %d surface %d',
//              [Material, s.num, i]));
//        end;
//
//        { pim:=ttx.Itexture.GetImage;
//          if pim=nil then; }
//
//        nvx := Vertices.count;
//        FillChar(Vxs, sizeof(Vxs), 0);
////        if nvx > 24 then
////          nvx := 24;
//
//        for j := 0 to nvx - 1 do
//        begin
//          if s.flags and SECF_3DO = 0 then
//            nv := j
//          else
//            nv := nvx - j - 1;
//
//          With Vertices[nv], TXVertices[nv] do
//          begin
//            cvx := @Vxs[nvx - j - 1];
//            cvx^.position.x := -x;
//            cvx^.position.y := y;
//            cvx^.position.z := z;
//            cvx^.normal.x := normal.dx;
//            cvx^.normal.y := normal.dy;
//            cvx^.normal.z := normal.dz;
//
//            if (P3DFullLit) or (s.flags and SECF_3DO <> 0) or
//              (SurfFlags and (SF_SkyHorizon or SF_SkyCeiling) <> 0) or (light = 0) then
//              cvx^.Color := $FFFFFF
//            else
//              cvx^.Color := EncodeBGR(s.ExtraLight + ExtraLight + color); //GetEncodedColor(s.ExtraLight + ExtraLight + color);
////              case s.Level.mots and P3DColoredLights of
////                true:
////                  cvx^.Color := GetMOTSColor(color.r, color.g, color.b, s.extra + extralight);
////                false:
////                  cvx^.Color := GetJKColor(color.a, s.extra + extralight);
////              end;
//
//            if ttx <> nil then
//            begin
//              // TODO: implement for IJIM
//              cvx^.tu := u;
//              cvx^.tv := v;
//              if CurrentProject <> TProjectType.IJIM then
//                begin
//                  cvx^.tu := u / ttx.imh.width;
//                  cvx^.tv := v / ttx.imh.height;
//                end;
//            end;
//          end;
//          mesh.SetVertices(id, 0, nvx, Vxs[0]);
//          if tex = 0 then
//            mesh.SetGroupMapping(id, 0)
//          else
//            mesh.SetGroupMapping(id, D3DRMMAP_PERSPCORRECT);
//          mesh.SetGroupQuality(id, D3DRMFILL_SOLID or D3DRMSHADE_GOURAUD or
//            d3drmlight_ON);
//          inc(nsurfs);
//        end;
//
//      end;
//
//    if nsurfs = 0 then
//    begin
//      mesh.Release;
//      result := nil
//    end
//    else
//      result := mesh;
//
//  except
//    on exception do
//      result := nil;
//  end;
//end;
//
//Function TD3DRenderer.BadThingMesh(th: TJKThing): IDirect3DRMMesh;
//var
//  mesh: IDirect3DRMMesh;
//  Vxs: array [0 .. 3] of TD3DRMVertex;
//  group: array [0 .. 1024] of dword;
//  cvx: ^TD3DRMVertex;
//  id: TD3DRMGROUPINDEX;
//  i: Integer;
//begin
//  DXFailCheck(FD3DRM.CreateMesh(mesh), 'in BadThingMesh when creating mesh');
//
//  With Vxs[0] do
//  begin
//    position.z := th.z + 0.05;
//    position.y := th.y;
//    position.x := -th.x - 0.05;
//    Color := $FFFFFF;
//  end;
//
//  With Vxs[1] do
//  begin
//    position.z := th.z - 0.05;
//    position.y := th.y;
//    position.x := -th.x - 0.05;
//    Color := $FFFFFF;
//  end;
//
//  With Vxs[2] do
//  begin
//    position.z := th.z - 0.05;
//    position.y := th.y;
//    position.x := -th.x + 0.05;
//    Color := $FFFFFF;
//  end;
//
//  With Vxs[3] do
//  begin
//    position.z := th.z + 0.05;
//    position.y := th.y;
//    position.x := -th.x + 0.05;
//    Color := $FFFFFF;
//  end;
//
//  for i := 0 to 3 do
//  begin
//    group[i] := i;
//    group[i + 4] := 3 - i;
//  end;
//
//  DXCheck(mesh.AddGroup(4, 2, 4, group[0], id),
//    'in BadThingMesh when adding group');
//  DXCheck(mesh.SetVertices(id, 0, 4, Vxs[0]),
//    'in BadThingMesh when setting vertices');
//  mesh.SetGroupQuality(id, D3DRMFILL_SOLID or D3DRMSHADE_GOURAUD or
//    d3drmlight_ON);
//  result := mesh;
//end;
//
//(*
//  Function TNewPRenderer.A3DOToMesh(th:TJKThing;a3do:TPJ3DO):T3DPMesh;
//  var i,j,k:integer;
//  nv,nvx:integer;
//  ttx:T3DPTexture;
//  cmp,mat:string;
//  ax,ay,az:single;
//  mx:TMat3x3s;
//  surf:T3DPSurf;
//  vd:TVXDets;
//  begin
//  result:=nil;
//  if (a3do=nil) then exit;
//  Result:=T3DPMesh.Create;
//
//  With th do CreateRotMatrixs(mx,pch,yaw,rol);
//
//  for i:=0 to a3DO.Meshes.count-1 do
//  With a3Do.Meshes[i] do
//  for k:=0 to faces.count-1 do
//  With Faces[k] do
//  begin
//  mat:=A3DO.GetMat(imat);
//  if (geo=0) or (Mat='') then continue;
//
//  surf:=Result.AddSurf;
//  CalcNormal;
//  surf.normal.dx:=normal.dx;
//  surf.normal.dy:=normal.dy;
//  surf.normal.dz:=normal.dz;
//  ttx:=nil;
//
//  Try
//  ttx:=GetTexture(Mat,thing_cmp);
//  surf.tx:=ttx;
//  { if ttx<>nil then ttx.SetCurrent;}
//  except
//  on Exception do PanMessage(mt_warning,
//  Format('Cannot load %s for thing %d',[Mat,th.num]));
//  end;
//
//
//  { pim:=ttx.Itexture.GetImage;
//  if pim=nil then;}
//
//  nvx:=Vertices.Count;
//  if nvx>24 then nvx:=24;
//
//  for j:=0 to nvx-1 do
//  With TXVertices[j] do
//  begin
//  vd:=Surf.AddVXD;
//
//  ax:=Vertices[j].tx;
//  ay:=Vertices[j].ty;
//  az:=Vertices[j].tz;
//
//  MultVM3s(mx,ax,ay,az);
//
//  vd.x:=th.x+ax;
//  vd.y:=th.y+ay;
//  vd.z:=th.z+az;
//
//
//  vd.r:=255; vd.g:=255; vd.b:=255;
//
//  if ttx<>nil then
//  begin
//  vd.u:=u/ttx.width;
//  vd.v:=v/ttx.height;
//  end;
//
//  { glVertex3f(th.x+ax,th.y+ay,th.z+az);}
//
//  end;
//
//  end;
//
//  result.CalculateSphere;
//
//  end;
//*)
//
//Function TD3DRenderer.PJ3DOToMesh(th: TJKThing; a3do: TPJ3DO): IDirect3DRMMesh;
//const
//  maxvx = 24;
//var
//  i, j, k: Integer;
//  mesh: IDirect3DRMMesh;
//  Vxs: array [0 .. maxvx - 1] of TD3DRMVertex;
//  cvx: ^TD3DRMVertex;
//  nvx: Integer;
//  id: TD3DRMGROUPINDEX;
//  ttx: TD3DTexture;
//  pim: PD3DRMIMAGE;
//  group: array [0 .. 1024] of dword;
//  ii: byte;
//  c3domesh: T3DOMesh;
//  cmp, Mat: string;
//  ax, ay, az: double;
//  mx: TMat3x3;
//  nsurfs: Integer;
//begin
//  result := nil;
//  if th.a3do = nil then
//    exit;
//  DXFailCheck(FD3DRM.CreateMesh(mesh), 'in ThingToMesh when creating mesh');
//
//  nsurfs := 0;
//
//  For i := 0 to maxvx - 1 do
//    group[i] := i;
//
//  With th do
//    CreateRotMatrix(mx, pch, yaw, rol);
//
//  for i := 0 to th.a3do.Meshes.count - 1 do
//    With th.a3do.Meshes[i] do
//      for k := 0 to faces.count - 1 do
//        With faces[k] do
//        begin
//          Mat := th.a3do.GetMat(imat);
//          if (geo = 0) or (Mat = '') then
//            continue;
//
//          DXCheck(mesh.AddGroup(Vertices.count, 1, Vertices.count, group[0],
//            id), 'in ThingtoMesh when adding group');
//
//          inc(nsurfs);
//
//          ttx := nil;
//
//          Try
//            ttx := LoadD3DTexture(Mat, thing_cmp);
//            if ttx <> nil then
//              mesh.SetGroupTexture(id, ttx.ITexture);
//          except
//            on exception do
//              PanMessage(mt_warning, Format('Cannot load %s for thing %d',
//                [Mat, th.num]));
//          end;
//
//          { pim:=ttx.Itexture.GetImage;
//            if pim=nil then; }
//
//          nvx := Vertices.count;
//          FillChar(Vxs, sizeof(Vxs), 0);
////          if nvx > 24 then
////            nvx := 24;
//
//          for j := 0 to nvx - 1 do
//            With Vertices[j], TXVertices[j] do
//            begin
//              ax := x;
//              ay := y;
//              az := z;
//              MultVM3(mx, ax, ay, az);
//
//              cvx := @Vxs[nvx - j - 1];
//              cvx^.position.x := -(ax + th.x);
//              cvx^.position.y := (ay + th.y);
//              cvx^.position.z := (az + th.z);
//              cvx^.normal.x := -normal.dx;
//              cvx^.normal.y := normal.dy;
//              cvx^.normal.z := normal.dz;
//
//              cvx^.Color := $FFFFFFFF  ;
//              if ttx <> nil then
//              begin
//                // TODO: implement for IJIM
//                cvx^.tu := u;
//                cvx^.tv := v;
//                if ProjectType <> TProjectType.IJIM then
//                  begin
//                    cvx^.tu := u / ttx.imh.width;
//                    cvx^.tv := v / ttx.imh.height;
//                  end;
//              end;
//            end;
//          mesh.SetVertices(id, 0, nvx, Vxs[0]);
//          if tex = 0 then
//            mesh.SetGroupMapping(id, 0)
//          else
//            mesh.SetGroupMapping(id, D3DRMMAP_PERSPCORRECT);
//          mesh.SetGroupQuality(id, D3DRMFILL_SOLID or D3DRMSHADE_GOURAUD or
//            d3drmlight_ON);
//
//        end;
//  if nsurfs > 0 then
//    result := mesh
//  else
//  begin
//    mesh.Release;
//    result := nil
//  end;
//end;
//
//// Start here tomorrow!
//
//Procedure TD3DRenderer.SetThing3DO(th: TJKThing; a3do: TPJ3DO);
//begin
//end;
//
//Function TD3DRenderer.ThingToMesh(th: TJKThing): IDirect3DRMMesh;
//const
//  maxvx = 64;
//var
//  i, j, k: Integer;
//  mesh: IDirect3DRMMesh;
//  Vxs: array [0 .. maxvx - 1] of TD3DRMVertex;
//  cvx: ^TD3DRMVertex;
//  nvx: Integer;
//  id: TD3DRMGROUPINDEX;
//  ttx: TD3DTexture;
//  pim: PD3DRMIMAGE;
//  group: array [0 .. 1024] of dword;
//  ii: byte;
//  c3domesh: T3DOMesh;
//  cmp, Mat: string;
//  ax, ay, az: double;
//  mx: TMat3x3;
//  nsurfs: Integer;
//begin
//  result := nil;
//  if th.a3do = nil then
//    exit;
//  DXFailCheck(FD3DRM.CreateMesh(mesh), 'in ThingToMesh when creating mesh');
//
//  nsurfs := 0;
//
//  For i := 0 to maxvx - 1 do
//    group[i] := i;
//
//  With th do
//    CreateRotMatrix(mx, pch, yaw, rol);
//
//  for i := 0 to th.a3do.Meshes.count - 1 do
//    With th.a3do.Meshes[i] do
//      for k := 0 to faces.count - 1 do
//        With faces[k] do
//        begin
//          Mat := th.a3do.GetMat(imat);
//          if (geo = 0) or (Mat = '') then
//            continue;
//
//          DXCheck(mesh.AddGroup(Vertices.count, 1, Vertices.count, group[0],
//            id), 'in ThingtoMesh when adding group');
//
//          inc(nsurfs);
//
//          ttx := nil;
//
//          Try
//            ttx := LoadD3DTexture(Mat, thing_cmp);
//            if ttx <> nil then
//              mesh.SetGroupTexture(id, ttx.ITexture);
//          except
//            on exception do
//              PanMessage(mt_warning, Format('Cannot load %s for thing %d',
//                [Mat, th.num]));
//          end;
//
//          { pim:=ttx.Itexture.GetImage;
//            if pim=nil then; }
//
//          nvx := Vertices.count;
//          FillChar(Vxs, sizeof(Vxs), 0);
////          if nvx > 24 then
////            nvx := 24;
//
//          for j := 0 to nvx - 1 do
//            With Vertices[j], TXVertices[j] do
//            begin
//              ax := x;
//              ay := y;
//              az := z;
//              MultVM3(mx, ax, ay, az);
//
//              cvx := @Vxs[nvx - j - 1];
//              cvx^.position.x := -(ax + th.x);
//              cvx^.position.y := (ay + th.y);
//              cvx^.position.z := (az + th.z);
//              cvx^.normal.x := -normal.dx;
//              cvx^.normal.y := normal.dy;
//              cvx^.normal.z := normal.dz;
//
//              cvx^.Color := $FFFFFF;
//              if ttx <> nil then
//              begin
//                // TODO: implement for IJIM
//                cvx^.tu := u;
//                cvx^.tv := v;
//                if ProjectType <> TProjectType.IJIM then
//                  begin
//                    cvx^.tu := u / ttx.imh.width;
//                    cvx^.tv := v / ttx.imh.height;
//                  end;
//              end;
//            end;
//          mesh.SetVertices(id, 0, nvx, Vxs[0]);
//          if tex = 0 then
//            mesh.SetGroupMapping(id, 0)
//          else
//            mesh.SetGroupMapping(id, D3DRMMAP_PERSPCORRECT);
//          mesh.SetGroupColorRGB(id, 1.0, 1.0, 1.0);
//          mesh.SetGroupQuality(id, D3DRMFILL_SOLID or D3DRMSHADE_GOURAUD or
//            d3drmlight_ON);
//
//        end;
//  if nsurfs > 0 then
//    result := mesh
//  else
//  begin
//    mesh.Release;
//    result := nil
//  end;
//end;
//
//procedure TD3DRenderer.RebuildDevice(bForce: boolean);
//var
//  oldDither: BOOL;
//  oldQuality: TD3DRMRENDERQUALITY;
//  oldShades: dword;
//  w, h, w0, h0: Integer;
//  r: HResult;
//begin
//  w0 := VWidth;
//  h0 := VHeight;
//  if Assigned(FDev) and ((FDev.GetWidth <> w0) or (FDev.GetHeight <> h0) or
//    bForce) then
//  begin
//    oldDither := FDev.GetDither;
//    oldQuality := FDev.GetQUality;
//    oldShades := FDev.GetShades;
//
//    FView.Release;
//    FView.Release;
//    FWDev.Release;
//    FDev.Release;
//
//    DXCheck(FD3DRM.CreateDeviceFromClipper(FClip, FDriverGUID, w0, h0,
//      FDev), '');
//    DXCheck(FDev.QueryInterface(IID_IDirect3DRMWinDevice, FWDev), '');
//    w := FDev.GetWidth;
//    h := FDev.GetHeight;
//    r := FD3DRM.CreateViewport(FDev, FCamera, 0, 0, w, h, FView);
//    if r <> D3D_OK then
//    begin
//      FWDev.Release;
//      COMRelease(IUnknown(FDev));
//      DXCheck(r, '');
//    end;
//    r := FView.SetBack(5000.0);
//    if r <> D3D_OK then
//    begin
//      COMRelease(IUnknown(FView));
//      COMRelease(IUnknown(FWDev));
//      COMRelease(IUnknown(FDev));
//      DXCheck(r, '');
//    end;
//    DXCheck(FDev.SetQuality(oldQuality), '');
//    DXCheck(FDev.SetDither(oldDither), '');
//    DXCheck(FDev.SetShades(oldShades), '');
//  end;
//end;
//
//procedure TD3DRenderer.SetDriver(n: Integer);
//var
//  pid: PGUID;
//begin
//  if (n >= 0) and (n < _D3DdriverCount) then
//  begin
//    pid := @_D3Ddrivers[n].id;
//    FDriverIndex := n;
//  end
//  else
//  begin
//    { default driver }
//    pid := nil;
//    FDriverIndex := -1;
//  end;
//  FDriverGUID := pid;
//  RebuildDevice(true);
//end;
//
//Destructor TD3DRenderer.Destroy;
//begin
//  if Assigned(FSubScene) then
//  begin
//    ClearThings;
//    ClearSectors;
//  end;
//  if Assigned(FView) then
//    COMRelease(IUnknown(FView));
//  if Assigned(FWDev) then
//    COMRelease(IUnknown(FWDev));
//  if Assigned(FDev) then
//    COMRelease(IUnknown(FDev));
//  if Assigned(FSubScene) then
//    COMRelease(IUnknown(FSubScene));
//  if Assigned(FCamera) then
//    COMRelease(IUnknown(FCamera));
//  if Assigned(FScene) then
//    COMRelease(IUnknown(FScene));
//  if Assigned(FClip) then
//    COMRelease(IUnknown(FClip));
//  if Assigned(FD3DRM) then
//    COMRelease(IUnknown(FD3DRM));
//  TXList.free;
//  CmpList.free;
//  sectors.free;
//  things.free;
//  THMeshes.free;
//  Meshes.free;
//  inherited Destroy;
//end;
//
//Procedure TD3DRenderer.ClearSectors;
//var
//  i: Integer;
//begin
//  for i := 0 to Meshes.count - 1 do
//  begin
//    DeleteMesh(IDirect3DRMMesh(Meshes[i]));
//  end;
//
//  sectors.Clear;
//  Meshes.Clear;
//  ClearTXList;
//end;
//
//Procedure TD3DRenderer.ClearThings;
//var
//  i: Integer;
//begin
//  for i := 0 to THMeshes.count - 1 do
//  begin
//    DeleteMesh(IDirect3DRMMesh(THMeshes[i]));
//  end;
//
//  things.Clear;
//  THMeshes.Clear;
//end;
//
//Procedure TD3DRenderer.AddSector(s: TJKSector);
//var
//  mesh: IDirect3DRMMesh;
//begin
//  mesh := SecToMesh(s);
//  if mesh = nil then
//    exit;
//  DXCheck(FSubScene.AddVisual(mesh), 'in AddSector');
//  sectors.Add(s);
//  Meshes.Add(mesh);
//end;
//
//Procedure TD3DRenderer.DeleteMesh(mesh: IDirect3DRMMesh);
//begin
//  if mesh = nil then
//    exit;
//  DXCheck(FSubScene.DeleteVisual(mesh), 'in DeleteMesh');
//  mesh.Release;
//end;
//
//Procedure TD3DRenderer.DeleteSector(s: TJKSector);
//var
//  i: Integer;
//begin
//  i := sectors.IndexOf(s);
//  if i < 0 then
//    exit;
//  DeleteMesh(Meshes[i]);
//  sectors.Delete(i);
//  Meshes.Delete(i);
//end;
//
//Procedure TD3DRenderer.UpdateSector(s: TJKSector);
//var
//  i: Integer;
//  mesh: IDirect3DRMMesh;
//begin
//  i := sectors.IndexOf(s);
//  if i < 0 then
//    exit;
//  mesh := Meshes[i];
//  DeleteMesh(mesh);
//  mesh := SecToMesh(s);
//  if mesh <> nil then
//    DXCheck(FSubScene.AddVisual(mesh), 'in UpdateSector');
//  Meshes[i] := mesh;
//end;
//
//Procedure TD3DRenderer.AddThing(th: TJKThing);
//var
//  mesh: IDirect3DRMMesh;
//begin
//  mesh := ThingToMesh(th);
//  if mesh = nil then
//    mesh := BadThingMesh(th);
//  DXCheck(FSubScene.AddVisual(mesh), 'in AddThing');
//  things.Add(th);
//  THMeshes.Add(mesh);
//end;
//
//Procedure TD3DRenderer.DeleteThing(th: TJKThing);
//var
//  i: Integer;
//begin
//  i := things.IndexOf(th);
//  if i < 0 then
//    exit;
//  DeleteMesh(THMeshes[i]);
//  things.Delete(i);
//  THMeshes.Delete(i);
//end;
//
//Procedure TD3DRenderer.UpdateThing(th: TJKThing);
//var
//  i: Integer;
//  mesh: IDirect3DRMMesh;
//begin
//  i := things.IndexOf(th);
//  if i < 0 then
//    exit;
//  mesh := THMeshes[i];
//  DeleteMesh(mesh);
//  mesh := ThingToMesh(th);
//  if mesh = nil then
//    mesh := BadThingMesh(th);
//  DXCheck(FSubScene.AddVisual(mesh), 'in UpdateThing');
//  THMeshes[i] := mesh;
//end;
//
//Procedure TD3DRenderer.ClearTXList;
//var
//  i: Integer;
//begin
//  for i := 0 to TXList.count - 1 do
//    TXList.Objects[i].free;
//  TXList.Clear;
//  for i := 0 to CmpList.count - 1 do
//    FreeMem(pointer(CmpList.Objects[i]));
//  CmpList.Clear;
//end;
//
//Function TD3DRenderer.LoadD3DTexture(const name, cmp: string): TD3DTexture;
//var
//  i: Integer;
//  ttx: TD3DTexture;
//  pcmp: PTCMPPal;
//  f: TFile;
//begin
//  pcmp := nil;
//  result := nil;
//  i := TXList.IndexOf(name + cmp);
//  if i <> -1 then
//  begin
//    result := TD3DTexture(TXList.Objects[i]);
//    exit;
//  end;
//
//  if ProjectType <> TProjectType.IJIM then
//    begin
//    i := CmpList.IndexOf(cmp);
//    if i <> -1 then
//      pcmp := pointer(CmpList.Objects[i])
//    else
//    begin
//      GetMem(pcmp, sizeof(pcmp^));
//      GetLevelPal(Level, pcmp^);
//      if not ApplyCMP(cmp, pcmp^) then
//        PanMessage(mt_warning, Format('Cannot load %s', [cmp]));
//      CmpList.AddObject(cmp, TObject(pcmp));
//    end;
//  end;
//
//  ttx := nil;
//  try
//    ttx := TD3DTexture.CreateFromMat(name, pcmp, FD3DRM, gamma);
//    ttx.cmpname := cmp;
//  finally
//    TXList.AddObject(name + cmp, ttx);
//    result := ttx;
//  end;
//end;
//
//Function TD3DRenderer.SetGamma(gamma: double): Integer;
//var
//  i, j, ic: Integer;
//  pcmp: ^TCMPPal;
//  cmp: TCMPPal;
//  f: TFile;
//  pe: ^TD3DRMPALETTEENTRY;
//  gmult: double;
//begin
//  self.gamma := gamma;
//  gmult := gamma;
//  for i := 0 to TXList.count - 1 do
//    if TXList.Objects[i] <> nil then
//      With TD3DTexture(TXList.Objects[i]) do
//      begin
//        ic := CmpList.IndexOf(cmpname);
//        if ic > -1 then
//        begin
//          pcmp := pointer(CmpList.Objects[ic]);
//          With imh do
//            for j := 0 to 255 do
//              With pcmp^[j] do
//              begin
//                if palette = nil then
//                begin
//                  continue;
//                end;
//                pe := pointer(PAnsiChar(palette) + j * sizeof(TD3DRMPALETTEENTRY));
//                pe^.red := Min(Round(r * gmult), 255);
//                pe^.green := Min(Round(g * gmult), 255);
//                pe^.blue := Min(Round(b * gmult), 255);
//                pe^.flags := 0; { D3DRMPALETTE_FREE }
//              end;
//        end;
//        if ITexture <> nil then
//          ITexture.Changed(true, true);
//      end;
//  result := 0;
//end;
//
//procedure TD3DRenderer.HandleActivate(const msg: TMessage);
//var
//  d3drmwindev: IDirect3DRMWinDevice;
//begin
//  if FDev <> nil then
//  begin
//    DXCheck(FDev.QueryInterface(IID_IDirect3DRMWinDevice, d3drmwindev),
//      'in HandleActivate');
//    DXCheck(d3drmwindev.HandleActivate(msg.wparam), 'in HandleActivate');
//    COMRelease(IUnknown(d3drmwindev));
//  end;
//end;
//
//procedure TD3DRenderer.HandlePaint(hdc: HDC);
//var
//  d3drmwindev: IDirect3DRMWinDevice;
//begin
//  if FDev = nil then
//    exit;
//  DXCheck(FDev.QueryInterface(IID_IDirect3DRMWinDevice, d3drmwindev),
//    'in HandlePaint');
//  DXCheck(d3drmwindev.HandlePaint(hdc), 'in HandlePaint');
//  COMRelease(IUnknown(d3drmwindev));
//end;
//
//function TD3DRenderer.createdevandview(driver: Integer;
//  width, height: Integer): boolean;
//var
//  rval: HResult;
//begin
//  //TODO: verify way the hardware device doesn't work
//  //  Temp wayaround is by setting to soft. device at idx 0
//  DXFailCheck(FD3DRM.CreateDeviceFromClipper(FClip, @_D3Ddrivers[0(*driver*)].id,
//    width, height, FDev), 'In createDeviceandView when creating device');
//
//  width := FDev.GetWidth;
//  height := FDev.GetHeight;
//  DXFailCheck(FD3DRM.CreateViewport(FDev, FCamera, 0, 0, width, height, FView),
//    'In createDeviceandView when creating viewport');
//  { if rval <> d3drm_ok then begin
//    ComRelease(IUnknown(Fdev));
//    Result := false;
//    exit;
//    end; }
//  DXFailCheck(FView.SetBack(5000.0), 'in CreateDevandView');
//  { if rval <> d3drm_ok then begin
//    ComRelease(IUnknown(Fdev));
//    ComRelease(IUnknown(Fview));
//    Result := false;
//    exit;
//    end; }
//  DXFailCheck(FView.setfront(0.05), 'in CreateDevandView');
//  DXFailCheck(FView.setPlane(-0.05, 0.05, -0.05, 0.05), 'in CreateDevandView');
//
//  rval := FDev.SetQuality(d3drmlight_on or D3DRMFILL_SOLID or
//    D3DRMSHADE_FLAT);
//
//  { if hardware then globs3d.dev.settexturequality(d3drmtexture_linear) else }
//  FDev.SetTextureQuality(d3drmtexture_nearest);
//
//  createdevandview := true;
//end;
//
//{ Procedure TD3DRenderer.SetFullLight(full:boolean);
//  var i,j:integer;
//  vs:IDirect3DRMVisualArray;
//  mesh:IDirect3DRMMesh;
//  begin
//  FDev.SetQuality(0);
//  end; }
//
//Procedure TD3DRenderer.SetViewToThing(th: TJKThing);
//var
//  i: Integer;
//  bbox: TThingBox;
//  l, r, b, t: TD3DVALUE;
//  a, camH, CamW, camD, thH, thW, thD: double;
//  v1, v2: TVector;
//
//var
//  tcen_x, tcen_y, tcen_z: double; { thing center }
//  trad: double; { radius }
//  nd, d: double;
//
//begin
//  if th.a3do = nil then
//    exit;
//  th.a3do.GetBBox(bbox);
//
//  tcen_x := bbox.x1 + (bbox.x2 - bbox.x1) / 2;
//  tcen_y := bbox.y1 + (bbox.y2 - bbox.y1) / 2;
//  tcen_z := bbox.z1 + (bbox.z2 - bbox.z1) / 2;
//  trad := sqrt(sqr(tcen_x - bbox.x1) + sqr(tcen_y - bbox.y1) +
//    sqr(tcen_z - bbox.z1));
//
//  camD := FView.GetFront;
//  FView.GetPlane(l, r, b, t);
//  CamW := (r - l) / 2;
//  camH := (t - b) / 2;
//
//  trad := trad;
//  d := camD + trad;
//
//  nd := trad / CamW * camD;
//  if nd > d then
//    d := nd;
//
//  nd := trad / camH * camD;
//  if nd > d then
//    d := nd;
//
//  CamX := tcen_x;
//  CamY := (tcen_y - d);
//  CamZ := tcen_z;
//
//  { Rotate bbox }
//  { With box do
//    begin
//    SetVec(v1,x1,y1,z1);
//    SetVec(v2,x2,y2,z2);
//    end;
//
//    With th do
//    begin
//    RotateVector(v1,pch,yaw,rol);
//    RotateVector(v2,pch,yaw,rol);
//    end;
//
//    box.x1:=real_min(v1.dx,v2.dx); box.x2:=real_max(v1.dx,v2.dx);
//    box.y1:=real_min(v1.dy,v2.dy); box.y2:=real_max(v1.dy,v2.dy);
//    box.z1:=real_min(v1.dz,v2.dz); box.z2:=real_max(v1.dz,v2.dz);
//
//
//    thW:=box.x2-box.x1;
//    thH:=box.z2-box.z1;
//    camD:=Fview.GetFront/10;
//    FView.GetPlane(l,r,b,t);
//    camW:=(r-l)/10;
//    CamH:=(t-b)/10;
//    thD:=(thW*CamD)/CamW+0.02;
//    a:=(thH*CamD)/CamH;
//    if a>thD then thD:=a;
//    if CamD>thD then ThD:=CamD;
//    camX:=box.x1+(box.x2-box.x1)/2;
//    camY:=box.y1-thD;
//    camZ:=box.z1+(box.z2-box.z1)/2; }
//end;
//
//Procedure TD3DRenderer.SetPCHYAW(pch, yaw: double);
//var
//  mesh: IDirect3DRMMesh;
//  th: TJKThing;
//  box: TThingBox;
//begin
//  if things.count = 0 then
//    exit;
//  mesh := THMeshes[0];
//  th := things[0];
//  if th.a3do = nil then
//    exit;
//  th.a3do.GetBBox(box);
//  FSubScene.setorientation(FScene, 0, 1, 0, 0, 0, 1);
//  FSubScene.AddRotation(D3DRMCOMBINE_AFTER, 0, 1, 0, yaw / 180 * PI);
//  FSubScene.AddRotation(D3DRMCOMBINE_AFTER, 1, 0, 0, pch / 180 * PI);
//end;

end.
