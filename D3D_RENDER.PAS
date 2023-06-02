unit D3D_Render;

interface
uses
 Render,Geometry, Windows, OLE2, SysUtils,
 D3DRMObj, D3DTypes, D3DRMDef, DDraw, D3D, D3Drm, dxtools, D3DrmWin, D3Dcaps;

Type

TD3DRenderer=class(TRenderer)
   DDBD: dword;
   FBPP:Dword;

   FDriverIndex: Integer;
   FDriverGUID: PGUID;

   FD3DRM:     IDirect3DRM;
   FClip:      IDirectDrawClipper;
   FDev:       IDirect3DRMDevice;
   FWDev:      IDirect3DRMWinDevice;

   FCamera:    IDirect3DRMFrame;
   FScene:     IDirect3DRMFrame;
   FView:      IDirect3DRMViewport;
   FSubScene:  IDirect3DRMFrame;
   nomore:boolean;
   group:array[0..1000] of dword; 
 Procedure Initialize;override;
 Procedure BeginScene;override;
 Procedure SetViewPort(x,y,w,h:integer);override;
 Procedure SetRenderStyle(rstyle:TRenderStyle);override;
 Procedure EndScene;override;
 Procedure SetColor(what,r,g,b:byte);override;
 Procedure DrawPolygon(p:TPolygon);override;
 Procedure DrawPolygons(ps:TPolygons);override;
 Procedure DrawLine(v1,v2:TVertex);override;
 Procedure DrawVertex(X,Y,Z:double);override;
 Procedure Configure;override; {Setup dialog}
 Destructor Destroy; override;

 Procedure BeginPick(x,y:integer);override;
 Procedure EndPick;override;
 Procedure PickPolygon(p:TPolygon;id:integer);override;
 Procedure PickPolygons(ps:TPolygons;id:integer);override;
 Procedure PickLine(v1,v2:TVertex;id:integer);override;
 Procedure PickVertex(X,Y,Z:double;id:integer);override;

Private
 Procedure InitializeDevice;
 Procedure SetDriver(n:integer);
 procedure RebuildDevice(bForce: Boolean);
end;

implementation

type
  TD3DDriverInfo = record
    id:                TGUID;
    DeviceDescription: string;
    DeviceName:        string;
    HWDeviceDesc:      D3DDeviceDesc ;
    HELDeviceDesc:     D3DDeviceDesc ;
  end;

const
  MAXDRIVERS = 8;

var
  {a few global variables storing driver info}
  _D3Ddrivers: array[0.. MAXDRIVERS-1] of TD3DDriverInfo;
  _D3DdriverCount: Integer;
  _bDriversInitialized: Boolean;

function _EnumCallBack(lpGuid: PGUID ;
      lpDeviceDescription: LPSTR ; lpDeviceName: LPSTR ;
      const lpD3DHWDeviceDesc: D3DDeviceDesc ;
      const lpD3DHELDeviceDesc: D3DDeviceDesc ;
      lpUserArg: Pointer ): HRESULT ; stdcall ;
var
  dev: ^D3DDeviceDesc;
  DDBD: DWORD;
begin
  dev := @lpD3DHWDeviceDesc;
  DDBD := DWORD(lpUserArg);
  if Integer(lpD3DHWDeviceDesc.dcmColorModel) = 0 then dev := @lpD3DHELDeviceDesc;
  if (dev^.dwDeviceRenderBitDepth and DDBD) <> 0 then
  begin
    {current bit depth is supported by this driver}
    with _D3DDrivers[_D3DdriverCount] do
    begin
      Move(lpGUID^, id, SizeOf(TGUID));
      Move(lpD3DHWDeviceDesc, HWDeviceDesc, SizeOf(D3DDeviceDesc));
      Move(lpD3DHELDeviceDesc, HELDeviceDesc, SizeOf(D3DDeviceDesc));
      DeviceDescription := StrPas(lpDeviceDescription);
      DeviceName := StrPas(lpDeviceName);
    end;
    Inc(_D3DdriverCount)
  end;
  Result := D3DENUMRET_OK;
  if _D3DDriverCount >= MAXDRIVERS then Result := D3DENUMRET_CANCEL;
end;

procedure _InitializeDrivers(Renderer: TD3DRenderer);
var
  d3d: IDirect3D;
  dd: IDirectDraw;
begin
  if not _bDriversInitialized then
  begin
    DXCheck(DirectDrawCreate(nil, dd, nil));
    try
      DXCheck(dd.QueryInterface(IID_IDirect3D, d3d));
      try
        _bDriversInitialized := True;
        DXCheck(d3d.EnumDevices(_EnumCallback, Pointer(Renderer.DDBD)));
      finally
        COMRelease(IUnknown(d3d));
      end;
    finally
      COMRelease(IUnknown(dd));
    end;
  end;
end;


Procedure TD3DRenderer.Initialize;
var
  lightframe: IDirect3DRMFrame;
  light1, light2: IDirect3DRMLight;
  dc: HDC;
begin
 {retrieve bits per pixel}
 DXCheck(Direct3DRMCreate(FD3DRM));
 
 DDBD:=8+16+24;
 dc := GetDC(Viewer.Handle);
 FBPP := GetDeviceCaps(dc, BITSPIXEL);
 ReleaseDC(Viewer.Handle, dc);
 _InitializeDrivers(Self);

 {create scene}
 DXCheck(DirectDrawCreateClipper(0, FClip, nil));
 DXCheck(FClip.SetHWnd(0, Viewer.Handle));
 DXCheck(FD3DRM.CreateFrame(nil, FScene));
 DXCheck(FD3DRM.CreateFrame(FScene, FCamera));
 DXCheck(FCamera.SetPosition(FScene, 0.0, 0.0, 0.0));
 DXCheck(FD3DRM.CreateFrame(FScene, lightframe));
 try
   {create lights}
   DXCheck(FD3DRM.CreateLightRGB(D3DRMLIGHT_DIRECTIONAL, 0.9, 0.9, 0.9, light1));
   try
     DXCheck(lightframe.AddLight(light1));
   finally
     COMRelease(IUnknown(light1));
   end;
   DXCheck(FD3DRM.CreateLightRGB(D3DRMLIGHT_AMBIENT, 0.1, 0.1, 0.1, light2));
   try
     DXCheck(lightframe.AddLight(light2));
   finally
     COMRelease(IUnknown(light2));
   end;
   DXCheck(lightframe.SetPosition(FScene, 2.0, 0.0, 22.0));
 finally
   COMRelease(IUnknown(lightframe));
 end;

 {create 'world' frame that will contain the user-defined objects}
 DXCheck(FD3DRM.CreateFrame(FScene, FSubScene));

 {set positions and orientations of frames}
 DXCheck(FCamera.SetPosition(FScene, 0.0, 0.0, -15.0));
 DXCheck(FCamera.SetOrientation(FScene, 0.0, 0.0, 1.0, 0.0, 1.0, 0.0));
      
 DXCheck(FSubScene.SetPosition(FScene, 0.0, 0.0, 0.0));
 DXCheck(FSubScene.SetOrientation(FScene, 0.0, 0.0, 1.0, 0.0, 1.0, 0.0));

 {create device and vieport}
 FDriverGUID:=nil;
 InitializeDevice;
 FView.SetProjection(D3DRMPROJECT_ORTHOGRAPHIC);
 FSubScene.SetZBufferMode(D3DRMZBUFFER_DISABLE);


{ FInitialized := True;}

 {user callback for getting content of scene}

end;

procedure TD3DRenderer.InitializeDevice;
var
  w, h: Integer;
  r : HResult;
begin
  DXCheck(FD3DRM.CreateDeviceFromClipper(FClip, FDriverGUID, Viewer.ClientWidth, Viewer.ClientHeight, FDev));
  DXCheck(FDev.QueryInterface(IID_IDirect3DRMWinDevice, FWDev));
  w := FDev.GetWidth;
  h := FDev.GetHeight;
  if COMFailed(r, FD3DRM.CreateViewport(FDev, FCamera, 0, 0, w, h, FView)) then
  begin
    COMRelease(IUnknown(FWDev));
    COMRelease(IUnknown(FDev));
    DXCheck(r);
  end;
  if COMFailed(r, FView.SetBack(5000.0)) then
  begin
    COMRelease(IUnknown(FView));
    COMRelease(IUnknown(FWDev));
    COMRelease(IUnknown(FDev));
    DXCheck(r);
  end;

  {set render quality}
  DXCheck(FDev.SetQuality(D3DRMRENDER_FLAT));
  {render quality part 2 - standard incantation}
  if FBPP = 1 then
  begin
    DXCheck(FDev.SetShades(4));
    DXCheck(FD3DRM.SetDefaultTextureShades(4));
  end
  else if FBPP = 16 then
  begin
    DXCheck(FDev.SetShades(32));
    DXCheck(FD3DRM.SetDefaultTextureColors(64));
    DXCheck(FD3DRM.SetDefaultTextureShades(32));
  end
  else
  if (FBPP = 24) or (FBPP = 32) then
  begin
    DXCheck(FDev.SetShades(256));
    DXCheck(FD3DRM.SetDefaultTextureColors(64));
    DXCheck(FD3DRM.SetDefaultTextureShades(256));
  end;

end;


Procedure TD3DRenderer.BeginScene;
var vs:IDIRECT3DRMVISUALARRAY;
    v:IDIRECT3DRMVISUAL;
    i:integer;
begin
 DXCheck(FView.Clear);
if not nomore then
begin
 FSubScene.GetVisuals(vs);
 for i:=vs.GetSize-1 downto 0 do
 begin
  vs.getElement(i,v);
  FSubScene.DeleteVisual(v);
 end;
end;
 DXCheck(FCamera.AddTranslation(D3DRMCOMBINE_REPLACE ,CamX,CamY,CamZ));
 FCamera.AddScale(D3DRMCOMBINE_BEFORE,scale,scale,scale);
 FCamera.AddRotation(D3DRMCOMBINE_AFTER,1,0,0,PCH/180*PI);
 FCamera.AddRotation(D3DRMCOMBINE_AFTER,0,1,0,YAW/180*PI);
 FCamera.AddRotation(D3DRMCOMBINE_AFTER,0,0,1,ROL/180*PI);
end;

Procedure TD3DRenderer.SetViewPort(x,y,w,h:integer);
begin
end;

Procedure TD3DRenderer.SetRenderStyle(rstyle:TRenderStyle);
begin
end;

Procedure TD3DRenderer.EndScene;
begin
 DXCheck(FView.Render(FScene));
 FDev.Update;
end;

Procedure TD3DRenderer.SetColor(what,r,g,b:byte);
begin
end;

Procedure TD3DRenderer.DrawPolygon(p:TPolygon);
begin

end;

Procedure TD3DRenderer.DrawPolygons(ps:TPolygons);
const
     maxvx=10;
var i,j:integer;
    Mesh:IDirect3DRMMesh;
    Vxs:array[0..maxvx-1] of D3DRMVertex;
    cvx:^D3DRMVertex;
    nvx,p:integer;
    id:D3DRMGROUPINDEX;
begin
 if nomore then exit;
 DXCheck(FD3DRM.CreateMesh(Mesh));
 p:=0;
 for i:=0 to ps.count-1 do
 With ps[i] do
 begin
  group[p]:=Vertices.Count; inc(p);
  for j:=0 to Vertices.Count-1 do
  begin
   group[p]:=Vertices[j].num;
   inc(p);
  end;
 end;
 if PS.VXList.Count<>0 then
begin
  DXCHeck(Mesh.AddGroup(PS.VXList.Count,ps.count,0,group[0],ID));

 nvx:=ps.VXList.Count;
 FillChar(vxs,sizeof(vxs),0);

 While nvx>=maxvx do
 begin
  cvx:=@vxs[0];
  for i:=nvx-maxvx to nvx-1 do
  With ps.VXList[i] do
  begin
   CVX^.position.x:=x;
   CVX^.position.y:=y;
   CVX^.position.z:=z;
   CVX^.Color:=255;
  inc(cvx);
  end;
  Mesh.SetVertices(ID,nvx-maxvx,maxvx,vxs[0]);
  Dec(nvx,maxvx);
 end;

 cvx:=@vxs[0];
 for i:=0 to nvx-1 do
 With ps.VXList[i] do
 begin
   CVX^.position.x:=x;
   CVX^.position.y:=y;
   CVX^.position.z:=z;
   CVX^.Color:=255;
  inc(cvx);
 end;
 DXCheck(Mesh.SetVertices(ID,0,nvx,vxs[0]));
 DXCheck(Mesh.SetGroupQuality(ID,D3DRMRENDER_WIREFRAME));
 DXCheck(FSubScene.AddVisual(Mesh));
end;
end;

Procedure TD3DRenderer.DrawLine(v1,v2:TVertex);
begin
end;

Procedure TD3DRenderer.DrawVertex(X,Y,Z:double);
begin
end;

procedure TD3DRenderer.RebuildDevice(bForce: Boolean);
var
  oldDither: BOOL;
  oldQuality: D3DRMRENDERQUALITY;
  oldShades: DWORD;
  w, h, w0, h0: Integer;
  r: HRESULT;
begin
  w0 := Viewer.ClientWidth;
  h0 := Viewer.ClientHeight;
  if Assigned(FDev) and ((FDev.GetWidth<>w0) or (FDev.GetHeight<>h0) or bForce) then
  begin
    oldDither := FDev.GetDither;
    oldQuality := FDev.GetQuality;
    oldShades := FDev.GetShades;

    COMRelease(IUnknown(FView));
    COMRelease(IUnknown(FWDev));
    COMRelease(IUnknown(FDev));

    DXCheck(FD3DRM.CreateDeviceFromClipper(FClip, FDriverGUID, w0, h0, FDev));
    DXCheck(FDev.QueryInterface(IID_IDirect3DRMWinDevice, FWDev));
    w := FDev.GetWidth;
    h := FDev.GetHeight;
    if COMFailed(r, FD3DRM.CreateViewport(FDev, FCamera, 0, 0, w, h, FView)) then
    begin
      COMRelease(IUnknown(FWDev));
      COMRelease(IUnknown(FDev));
      DXCheck(r);
    end;
    if COMFailed(r, FView.SetBack(5000.0)) then
    begin
      COMRelease(IUnknown(FView));
      COMRelease(IUnknown(FWDev));
      COMRelease(IUnknown(FDev));
      DXCheck(r);
    end;
    DXCheck(FDev.SetQuality(oldQuality));
    DXCheck(FDev.SetDither(oldDither));
    DXCheck(FDev.SetShades(oldShades));
  end;
end;


procedure TD3DRenderer.SetDriver(n: Integer);
var
  pid: PGUID;
begin
  if (n>=0) and (n<_D3DdriverCount) then
  begin
    pid := @_D3Ddrivers[n].id;
    FDriverIndex := n;
  end
  else
  begin
    {default driver}
    pid := nil;
    FDriverIndex := -1;
  end;
  FDriverGUID := pid;
  RebuildDevice(True);
end;


Procedure TD3DRenderer.Configure; {Setup dialog}
begin
end;

Destructor TD3DRenderer.Destroy;
begin
    if Assigned(FView) then COMRelease(IUnknown(FView));
    if Assigned(FWDev) then COMRelease(IUnknown(FWDev));
    if Assigned(FDev) then COMRelease(IUnknown(FDev));
    COMRelease(IUnknown(FSubScene));
    COMRelease(IUnknown(FCamera));
    COMRelease(IUnknown(FScene));
    COMRelease(IUnknown(FClip));
  COMRelease(IUnknown(FD3DRM));
  inherited Destroy;
end;

Procedure TD3DRenderer.BeginPick(x,y:integer);
begin
end;

Procedure TD3DRenderer.EndPick;
begin
end;

Procedure TD3DRenderer.PickPolygon(p:TPolygon;id:integer);
begin
end;

Procedure TD3DRenderer.PickPolygons(ps:TPolygons;id:integer);
begin
end;

Procedure TD3DRenderer.PickLine(v1,v2:TVertex;id:integer);
begin
end;

Procedure TD3DRenderer.PickVertex(X,Y,Z:double;id:integer);
begin
end;

end.
