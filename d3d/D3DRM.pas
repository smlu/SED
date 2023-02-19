(*==========================================================================;
 *
 *  Copyright (C) 1995-1997 Microsoft Corporation.  All Rights Reserved.
 *
 *  File:	d3drm.h
 *  Content:	Direct3DRM include file
 *
 *  DirectX 5 Delphi adaptation by Erik Unger
 *  (based on Blake Stone's DirectX 3 adaptation)
 *
 *  Modyfied: 21.3.98
 *
 *  Download: http://www.sbox.tu-graz.ac.at/home/ungerik/DelphiGraphics/
 *  E-Mail: h_unger@magnet.at
 *
 ***************************************************************************)

unit D3DRM;

{$INCLUDE COMSWITCH.INC}

interface

uses
{$IFDEF D2COM}
  OLE2,
{$ENDIF}
  Windows,
  D3DRMObj,
  D3DTypes,
  D3DRMDef,
  DDraw,
  D3D;

type
  TD3DRMDevicePaletteCallback = procedure (lpDirect3DRMDev: IDirect3DRMDevice;
      lpArg: Pointer; dwIndex: DWORD; red, green, blue: LongInt); cdecl;

const
  IID_IDirect3DRM: TGUID =
      (D1:$2bc49361;D2:$8327;D3:$11cf;D4:($ac,$4a,$0,$0,$c0,$38,$25,$a1));
  IID_IDirect3DRM2: TGUID =
      (D1:$4516ecc8;D2:$8f20;D3:$11d0;D4:($9b,$6d,$00,$00,$c0,$78,$1b,$c3));

type
{$IFDEF D2COM}
  IDirect3DRM = class (IUnknown)
{$ELSE}
  IDirect3DRM = interface (IUnknown)
    ['{2bc49361-8327-11cf-ac4a-0000c03825a1}']
{$ENDIF}
    function CreateObject (rclsid: TGUID; pUnkOuter: IUnknown;
        riid: TGUID; var ppv: Pointer) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function CreateFrame (lpD3DRMFrame: IDirect3DRMFrame;
        var lplpD3DRMFrame: IDirect3DRMFrame) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function CreateMesh (var lplpD3DRMMesh: IDirect3DRMMesh) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function CreateMeshBuilder (var lplpD3DRMMeshBuilder:
        IDirect3DRMMeshBuilder) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function CreateFace (var lplpd3drmFace: IDirect3DRMFace) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function CreateAnimation (var lplpD3DRMAnimation: IDirect3DRMAnimation) :
        HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function CreateAnimationSet (var lplpD3DRMAnimationSet:
        IDirect3DRMAnimationSet) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function CreateTexture (const lpImage: TD3DRMImage;
        var lplpD3DRMTexture: IDirect3DRMTexture) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function CreateLight (d3drmltLightType: TD3DRMLightType;
        cColor: TD3DColor; var lplpD3DRMLight: IDirect3DRMLight) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function CreateLightRGB (ltLightType: TD3DRMLightType; vRed,
        vGreen, vBlue: TD3DValue; var lplpD3DRMLight: IDirect3DRMLight) :
        HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function CreateMaterial (vPower: TD3DValue; var lplpD3DRMMaterial:
        IDirect3DRMMaterial) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function CreateDevice (dwWidth, dwHeight: DWORD; var lplpD3DRMDevice:
        IDirect3DRMDevice) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}

    (* Create a Windows Device using DirectDraw surfaces *)
    function CreateDeviceFromSurface (lpGUID: PGUID; lpDD: IDirectDraw;
        lpDDSBack: IDirectDrawSurface; var lplpD3DRMDevice: IDirect3DRMDevice) :
        HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}

      (* Create a Windows Device using D3D objects *)
    function CreateDeviceFromD3D (lpD3D: IDirect3D; lpD3DDev: IDirect3DDevice;
        var lplpD3DRMDevice: IDirect3DRMDevice) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}

    function CreateDeviceFromClipper (lpDDClipper: IDirectDrawClipper;
        lpGUID: PGUID; width, height: Integer; var lplpD3DRMDevice:
        IDirect3DRMDevice) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}

    function CreateTextureFromSurface ( lpDDS: IDirectDrawSurface;
        var lplpD3DRMTexture: IDirect3DRMTexture) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}

    function CreateShadow (lpVisual: IDirect3DRMVisual;
        lpLight: IDirect3DRMLight; px, py, pz, nx, ny, nz: TD3DValue;
        var lplpShadow: IDirect3DRMVisual) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function CreateViewport (lpDev: IDirect3DRMDevice;
        lpCamera: IDirect3DRMFrame; dwXPos, dwYPos, dwWidth, dwHeight: DWORD;
        var lplpD3DRMViewport: IDirect3DRMViewport) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function CreateWrap (wraptype: TD3DRMWrapType; lpRef: IDirect3DRMFrame;
        ox, oy, oz, dx, dy, dz, ux, uy, uz, ou, ov, su, sv: TD3DValue;
        var lplpD3DRMWrap: IDirect3DRMWrap) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function CreateUserVisual (fn: TD3DRMUserVisualCallback; lpArg: Pointer;
        var lplpD3DRMUV: IDirect3DRMUserVisual) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function LoadTexture (lpFileName: LPSTR; var lplpD3DRMTexture:
        IDirect3DRMTexture) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function LoadTextureFromResource (rs: HRSRC; var lplpD3DRMTexture:
        IDirect3DRMTexture) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}

    function SetSearchPath (lpPath: LPSTR) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function AddSearchPath (lpPath: LPSTR) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetSearchPath (var lpdwSize: DWORD; lpszPath: LPSTR) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetDefaultTextureColors (dwColors: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetDefaultTextureShades (dwShades: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}

    function GetDevices (var lplpDevArray: IDirect3DRMDeviceArray) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetNamedObject (lpName: LPSTR; var lplpD3DRMObject:
        IDirect3DRMObject) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}

    function EnumerateObjects (func: TD3DRMObjectCallback; lpArg: Pointer) :
        HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}

    function Load (lpvObjSource, lpvObjID: Pointer; var lplpGUIDs: PGUID;
        dwcGUIDs: DWORD; d3drmLOFlags: TD3DRMLoadOptions; d3drmLoadProc:
        TD3DRMLoadCallback; lpArgLP: Pointer; d3drmLoadTextureProc:
        TD3DRMLoadTextureCallback; lpArgLTP: Pointer; lpParentFrame:
        IDirect3DRMFrame) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function Tick (d3dvalTick: TD3DValue) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
  end;

type
{$IFDEF D2COM}
  IDirect3DRM2 = class (IUnknown)
{$ELSE}
  IDirect3DRM2 = interface (IUnknown)
    ['{4516ecc8-8f20-11d0-9b6d-0000c0781bc3}']
{$ENDIF}
    function CreateObject (rclsid: TGUID; pUnkOuter: IUnknown;
        riid: TGUID; var ppv: Pointer) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function CreateFrame (lpD3DRMFrame: IDirect3DRMFrame;
        var lplpD3DRMFrame: IDirect3DRMFrame) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function CreateMesh (var lplpD3DRMMesh: IDirect3DRMMesh) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function CreateMeshBuilder (var lplpD3DRMMeshBuilder:
        IDirect3DRMMeshBuilder) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function CreateFace (var lplpd3drmFace: IDirect3DRMFace) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function CreateAnimation (var lplpD3DRMAnimation: IDirect3DRMAnimation) :
        HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function CreateAnimationSet (var lplpD3DRMAnimationSet:
        IDirect3DRMAnimationSet) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function CreateTexture (const lpImage: TD3DRMImage;
        var lplpD3DRMTexture: IDirect3DRMTexture) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function CreateLight (d3drmltLightType: TD3DRMLightType;
        cColor: TD3DColor; var lplpD3DRMLight: IDirect3DRMLight) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function CreateLightRGB (ltLightType: TD3DRMLightType; vRed,
        vGreen, vBlue: TD3DValue; var lplpD3DRMLight: IDirect3DRMLight) :
        HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function CreateMaterial (vPower: TD3DValue; var lplpD3DRMMaterial:
        IDirect3DRMMaterial) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function CreateDevice (dwWidth, dwHeight: DWORD; var lplpD3DRMDevice:
        IDirect3DRMDevice) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}

    (* Create a Windows Device using DirectDraw surfaces *)
    function CreateDeviceFromSurface (lpGUID: PGUID; lpDD: IDirectDraw;
        lpDDSBack: IDirectDrawSurface; var lplpD3DRMDevice: IDirect3DRMDevice) :
        HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}

      (* Create a Windows Device using D3D objects *)
    function CreateDeviceFromD3D (lpD3D: IDirect3D; lpD3DDev: IDirect3DDevice;
        var lplpD3DRMDevice: IDirect3DRMDevice) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}

    function CreateDeviceFromClipper (lpDDClipper: IDirectDrawClipper;
        lpGUID: PGUID; width, height: Integer; var lplpD3DRMDevice:
        IDirect3DRMDevice) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}

    function CreateTextureFromSurface ( lpDDS: IDirectDrawSurface;
        var lplpD3DRMTexture: IDirect3DRMTexture) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}

    function CreateShadow (lpVisual: IDirect3DRMVisual;
        lpLight: IDirect3DRMLight; px, py, pz, nx, ny, nz: TD3DValue;
        var lplpShadow: IDirect3DRMVisual) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function CreateViewport (lpDev: IDirect3DRMDevice;
        lpCamera: IDirect3DRMFrame; dwXPos, dwYPos, dwWidth, dwHeight: DWORD;
        var lplpD3DRMViewport: IDirect3DRMViewport) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function CreateWrap (wraptype: TD3DRMWrapType; lpRef: IDirect3DRMFrame;
        ox, oy, oz, dx, dy, dz, ux, uy, uz, ou, ov, su, sv: TD3DValue;
        var lplpD3DRMWrap: IDirect3DRMWrap) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function CreateUserVisual (fn: TD3DRMUserVisualCallback; lpArg: Pointer;
        var lplpD3DRMUV: IDirect3DRMUserVisual) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function LoadTexture (lpFileName: LPSTR; var lplpD3DRMTexture:
        IDirect3DRMTexture) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function LoadTextureFromResource (rs: HRSRC; var lplpD3DRMTexture:
        IDirect3DRMTexture) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}

    function SetSearchPath (lpPath: LPSTR) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function AddSearchPath (lpPath: LPSTR) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetSearchPath (var lpdwSize: DWORD; lpszPath: LPSTR) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetDefaultTextureColors (dwColors: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetDefaultTextureShades (dwShades: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}

    function GetDevices (var lplpDevArray: IDirect3DRMDeviceArray) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetNamedObject (lpName: LPSTR; var lplpD3DRMObject:
        IDirect3DRMObject) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}

    function EnumerateObjects (func: TD3DRMObjectCallback; lpArg: Pointer) :
        HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}

    function Load (lpvObjSource, lpvObjID: Pointer; var lplpGUIDs: PGUID;
        dwcGUIDs: DWORD; d3drmLOFlags: TD3DRMLoadOptions; d3drmLoadProc:
        TD3DRMLoadCallback; lpArgLP: Pointer; d3drmLoadTextureProc:
        TD3DRMLoadTextureCallback; lpArgLTP: Pointer; lpParentFrame:
        IDirect3DRMFrame) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function Tick (d3dvalTick: TD3DValue) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function CreateProgressiveMesh (var lplpD3DRMProgressiveMesh:
        IDirect3DRMProgressiveMesh) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
  end;

const
  D3DRM_OK                        = DD_OK;
  D3DRMERR_BADOBJECT              = integer($88760000 + 781);
  D3DRMERR_BADTYPE                = integer($88760000 + 782);
  D3DRMERR_BADALLOC               = integer($88760000 + 783);
  D3DRMERR_FACEUSED               = integer($88760000 + 784);
  D3DRMERR_NOTFOUND               = integer($88760000 + 785);
  D3DRMERR_NOTDONEYET             = integer($88760000 + 786);
  D3DRMERR_FILENOTFOUND           = integer($88760000 + 787);
  D3DRMERR_BADFILE                = integer($88760000 + 788);
  D3DRMERR_BADDEVICE              = integer($88760000 + 789);
  D3DRMERR_BADVALUE               = integer($88760000 + 790);
  D3DRMERR_BADMAJORVERSION        = integer($88760000 + 791);
  D3DRMERR_BADMINORVERSION        = integer($88760000 + 792);
  D3DRMERR_UNABLETOEXECUTE        = integer($88760000 + 793);
  D3DRMERR_LIBRARYNOTFOUND        = integer($88760000 + 794);
  D3DRMERR_INVALIDLIBRARY         = integer($88760000 + 795);
  D3DRMERR_PENDING                = integer($88760000 + 796);
  D3DRMERR_NOTENOUGHDATA          = integer($88760000 + 797);
  D3DRMERR_REQUESTTOOLARGE        = integer($88760000 + 798);
  D3DRMERR_REQUESTTOOSMALL        = integer($88760000 + 799);
  D3DRMERR_CONNECTIONLOST         = integer($88760000 + 800);
  D3DRMERR_LOADABORTED            = integer($88760000 + 801);
  D3DRMERR_NOINTERNET             = integer($88760000 + 802);
  D3DRMERR_BADCACHEFILE           = integer($88760000 + 803);
  D3DRMERR_BOXNOTSET	          = integer($88760000 + 804);
  D3DRMERR_BADPMDATA              = integer($88760000 + 805);

(* Create a Direct3DRM API *)
function Direct3DRMCreate (var lplpDirect3DRM: IDirect3DRM) : HResult; stdcall;

implementation

function Direct3DRMCreate; external 'D3DRM.DLL';

end.

