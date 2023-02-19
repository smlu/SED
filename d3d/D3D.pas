(*==========================================================================;
 *
 *  Copyright (C) 1995-1997 Microsoft Corporation.  All Rights Reserved.
 *
 *  File:	d3d.h
 *  Content:	Direct3D include file
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

unit D3D;

{$INCLUDE COMSWITCH.INC}

interface

uses
{$IFDEF D2COM}
  OLE2,
{$ENDIF}
  Windows,
  D3DTypes,
  D3DCaps,
  DDraw;

{
 The methods Begin and End from the Interface IDirect3DDevice2
 are renamed to Begin_ and End_ to fit the Pascal-Syntax !
 use IDirect3DDevice2.Begin_
 and IDirect3DDevice2.End_
}

(*
 * Interface IID's
 *)

const
  IID_IDirect3D: TGUID =
      (D1:$3BBA0080;D2:$2421;D3:$11CF;D4:($A3,$1A,$00,$AA,$00,$B9,$33,$56));
  IID_IDirect3D2: TGUID =
      (D1:$6aae1ec1;D2:$662a;D3:$11d0;D4:($88,$9d,$00,$aa,$00,$bb,$b7,$6a));

  IID_IDirect3DRampDevice: TGUID =
      (D1:$F2086B20;D2:$259F;D3:$11CF;D4:($A3,$1A,$00,$AA,$00,$B9,$33,$56));
  IID_IDirect3DRGBDevice: TGUID =
      (D1:$A4665C60;D2:$2673;D3:$11CF;D4:($A3,$1A,$00,$AA,$00,$B9,$33,$56));
  IID_IDirect3DHALDevice: TGUID =
      (D1:$84E63dE0;D2:$46AA;D3:$11CF;D4:($81,$6F,$00,$00,$C0,$20,$15,$6E));
  IID_IDirect3DMMXDevice: TGUID =
      (D1:$881949a1;D2:$d6f3;D3:$11d0;D4:($89,$ab,$00,$a0,$c9,$05,$41,$29));

  IID_IDirect3DDevice: TGUID =
      (D1:$64108800;D2:$957d;D3:$11d0;D4:($89,$ab,$00,$a0,$c9,$05,$41,$29));
  IID_IDirect3DDevice2: TGUID =
      (D1:$93281501;D2:$8cf8;D3:$11d0;D4:($89,$ab,$00,$a0,$c9,$05,$41,$29));
  IID_IDirect3DTexture: TGUID =
      (D1:$2CDCD9E0;D2:$25A0;D3:$11CF;D4:($A3,$1A,$00,$AA,$00,$B9,$33,$56));
  IID_IDirect3DTexture2: TGUID =
      (D1:$93281502;D2:$8cf8;D3:$11d0;D4:($89,$ab,$00,$a0,$c9,$05,$41,$29));
  IID_IDirect3DLight: TGUID =
      (D1:$4417C142;D2:$33AD;D3:$11CF;D4:($81,$6F,$00,$00,$C0,$20,$15,$6E));
  IID_IDirect3DMaterial: TGUID =
      (D1:$4417C144;D2:$33AD;D3:$11CF;D4:($81,$6F,$00,$00,$C0,$20,$15,$6E));
  IID_IDirect3DMaterial2: TGUID =
      (D1:$93281503;D2:$8cf8;D3:$11d0;D4:($89,$ab,$00,$a0,$c9,$5,$41,$29));
  IID_IDirect3DExecuteBuffer: TGUID =
      (D1:$4417C145;D2:$33AD;D3:$11CF;D4:($81,$6F,$00,$00,$C0,$20,$15,$6E));
  IID_IDirect3DViewport: TGUID =
      (D1:$4417C146;D2:$33AD;D3:$11CF;D4:($81,$6F,$00,$00,$C0,$20,$15,$6E));
  IID_IDirect3DViewport2: TGUID =
      (D1:$93281500;D2:$8cf8;D3:$11d0;D4:($89,$ab,$00,$a0,$c9,$05,$41,$29));

(*
 * Data structures
 *)

type
{$IFDEF D2COM}
  IDirect3D = class;
  IDirect3D2 = class;
  IDirect3DDevice = class;
  IDirect3DDevice2 = class;
  IDirect3DExecuteBuffer = class;
  IDirect3DLight = class;
  IDirect3DMaterial = class;
  IDirect3DMaterial2 = class;
  IDirect3DTexture = class;
  IDirect3DTexture2 = class;
  IDirect3DViewport = class;
  IDirect3DViewport2 = class;
{$ELSE}
  IDirect3D = interface;
  IDirect3D2 = interface;
  IDirect3DDevice = interface;
  IDirect3DDevice2 = interface;
  IDirect3DExecuteBuffer = interface;
  IDirect3DLight = interface;
  IDirect3DMaterial = interface;
  IDirect3DMaterial2 = interface;
  IDirect3DTexture = interface;
  IDirect3DTexture2 = interface;
  IDirect3DViewport = interface;
  IDirect3DViewport2 = interface;
{$ENDIF}

(*
 * IDirect3D
 *)

{$IFDEF D2COM}
  IDirect3D = class (IUnknown)
{$ELSE}
  IDirect3D = interface (IUnknown)
    ['{3BBA0080-2421-11CF-A31A-00AA00B93356}']
{$ENDIF}
    (*** IDirect3D methods ***)
    function Initialize (lpREFIID: {REFIID} PGUID) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function EnumDevices (lpEnumDevicesCallback: TD3DEnumDevicesCallback;
        lpUserArg: Pointer) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function CreateLight (var lplpDirect3Dlight: IDirect3DLight;
        pUnkOuter: IUnknown) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function CreateMaterial (var lplpDirect3DMaterial: IDirect3DMaterial;
        pUnkOuter: IUnknown) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function CreateViewport (var lplpD3DViewport: IDirect3DViewport;
        pUnkOuter: IUnknown) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function FindDevice (const lpD3DFDS: TD3DFindDeviceSearch;
        var lpD3DFDR: TD3DFindDeviceResult) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
  end;

(*
 * IDirect3D2
 *)

{$IFDEF D2COM}
  IDirect3D2 = class (IUnknown)
{$ELSE}
  IDirect3D2 = interface (IUnknown)
    ['{3BBA0080-2421-11CF-A31A-00AA00B93356}']
{$ENDIF}
    (*** IDirect3D methods ***)
    function EnumDevices(lpEnumDevicesCallback: TD3DEnumDevicesCallback;
        lpUserArg: pointer) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function CreateLight (var lplpDirect3Dlight: IDirect3DLight;
        pUnkOuter: IUnknown) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function CreateMaterial (var lplpDirect3DMaterial2: IDirect3DMaterial2;
        pUnkOuter: IUnknown) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function CreateViewport (var lplpD3DViewport2: IDirect3DViewport2;
        pUnkOuter: IUnknown) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function FindDevice (const lpD3DFDS: TD3DFindDeviceSearch;
        var lpD3DFDR: TD3DFindDeviceResult) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function CreateDevice (const rclsid: TRefClsID; lpDDS: IDirectDrawSurface;
        var lplpD3DDevice2: IDirect3DDevice2) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
  end;

(*
 * IDirect3DDevice
 *)

{$IFDEF D2COM}
  IDirect3DDevice = class (IUnknown)
{$ELSE}
  IDirect3DDevice = interface (IUnknown)
    ['{64108800-957d-11d0-89ab-00a0c9054129}']
{$ENDIF}
    (*** IDirect3DDevice methods ***)
    function Initialize (lpd3d: IDirect3D; lpGUID: PGUID;
        const lpd3ddvdesc: TD3DDeviceDesc) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetCaps (var lpD3DHWDevDesc: TD3DDeviceDesc;
        var lpD3DHELDevDesc: TD3DDeviceDesc) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SwapTextureHandles (lpD3DTex1: IDirect3DTexture;
        lpD3DTex2: IDirect3DTexture) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function CreateExecuteBuffer (const lpDesc: TD3DExecuteBufferDesc ;
        var lplpDirect3DExecuteBuffer: IDirect3DExecuteBuffer;
        pUnkOuter: IUnknown) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetStats (var lpD3DStats: D3DSTATS) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function Execute (lpDirect3DExecuteBuffer: IDirect3DExecuteBuffer;
        lpDirect3DViewport: IDirect3DViewport; dwFlags: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function AddViewport (lpDirect3DViewport: IDirect3DViewport) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function DeleteViewport (lpDirect3DViewport: IDirect3DViewport) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function NextViewport (lpDirect3DViewport: IDirect3DViewport;
        var lplpDirect3DViewport: IDirect3DViewport; dwFlags: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function Pick (lpDirect3DExecuteBuffer: IDirect3DExecuteBuffer;
        lpDirect3DViewport: IDirect3DViewport; dwFlags: DWORD;
        const lpRect: TD3DRect) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetPickRecords (var lpCount: DWORD;
        var lpD3DPickRec: TD3DPickRecord) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function EnumTextureFormats (lpd3dEnumTextureProc:
        TD3DEnumTextureFormatsCallback; lpArg: Pointer) :
        HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function CreateMatrix (var lpD3DMatHandle: TD3DMatrixHandle) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetMatrix (d3dMatHandle: TD3DMatrixHandle;
        const lpD3DMatrix: TD3DMatrix) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetMatrix (const lpD3DMatHandle: TD3DMatrixHandle;
        var lpD3DMatrix: TD3DMatrix) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function DeleteMatrix (d3dMatHandle: TD3DMatrixHandle) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function BeginScene: HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function EndScene: HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetDirect3D (var lpD3D: IDirect3D) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
  end;

(*
 * IDirect3DDevice2
 *)

{$IFDEF D2COM}
  IDirect3DDevice2 = class (IUnknown)
{$ELSE}
  IDirect3DDevice2 = interface (IUnknown)
    ['{93281501-8cf8-11d0-89ab-00a0c9054129}']
{$ENDIF}
    (*** IDirect3DDevice2 methods ***)
    function GetCaps (var lpD3DHWDevDesc: TD3DDeviceDesc;
        var lpD3DHELDevDesc: TD3DDeviceDesc) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SwapTextureHandles (lpD3DTex1: IDirect3DTexture2;
        lpD3DTex2: IDirect3DTexture2) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetStats (var lpD3DStats: D3DSTATS) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function AddViewport (lpDirect3DViewport2: IDirect3DViewport2) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function DeleteViewport (lpDirect3DViewport: IDirect3DViewport2) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function NextViewport (lpDirect3DViewport: IDirect3DViewport2;
        var lplpDirect3DViewport: IDirect3DViewport2; dwFlags: DWORD) :
        HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function EnumTextureFormats (
        lpd3dEnumTextureProc: TD3DEnumTextureFormatsCallback; lpArg: Pointer) :
        HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function BeginScene: HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function EndScene: HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetDirect3D (var lpD3D: IDirect3D2) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}

    (*** DrawPrimitive API ***)
    function SetCurrentViewport (lpd3dViewport2: IDirect3DViewport2)
        : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetCurrentViewport (var lplpd3dViewport2: IDirect3DViewport2)
        : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}

    function SetRenderTarget (lpNewRenderTarget: IDirectDrawSurface)
        : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetRenderTarget (var lplpNewRenderTarget: IDirectDrawSurface)
        : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}

    function Begin_ (d3dpt: TD3DPrimitiveType; d3dvt: TD3DVertexType;
        dwFlags: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function BeginIndexed (dptPrimitiveType: TD3DPrimitiveType; dvtVertexType:
        TD3DVertexType; lpvVertices: pointer; dwNumVertices: DWORD;
        dwFlags: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function Vertex (lpVertexType: pointer) : HResult;
         {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function Index (wVertexIndex: WORD) : HResult;
         {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function End_ (dwFlags: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}

    function GetRenderState (dwRenderStateType: TD3DRenderStateType;
        var lpdwRenderState: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetRenderState (dwRenderStateType: TD3DRenderStateType;
        dwRenderState: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetLightState (dwLightStateType: TD3DLightStateType;
        var lpdwLightState: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetLightState (dwLightStateType: TD3DLightStateType;
        dwLightState: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetTransform (dtstTransformStateType: TD3DTransformStateType;
        const lpD3DMatrix: TD3DMatrix) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetTransform (dtstTransformStateType: TD3DTransformStateType;
        var lpD3DMatrix: TD3DMatrix) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function MultiplyTransform (dtstTransformStateType: TD3DTransformStateType;
        var lpD3DMatrix: TD3DMatrix) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}

    function DrawPrimitive (dptPrimitiveType: TD3DPrimitiveType;
        dvtVertexType: TD3DVertexType; lpvVertices: pointer; dwVertexCount,
        dwFlags: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function DrawIndexedPrimitive (dptPrimitiveType: TD3DPrimitiveType;
        dvtVertexType: TD3DVertexType; lpvVertices: pointer; dwVertexCount:
        DWORD; var dwIndices; dwIndexCount,
        dwFlags: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}

    function SetClipStatus (const lpD3DClipStatus: TD3DClipStatus) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetClipStatus (var lpD3DClipStatus: TD3DClipStatus) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
  end;

(*
 * IDirect3DExecuteBuffer
 *)

{$IFDEF D2COM}
  IDirect3DExecuteBuffer = class (IUnknown)
{$ELSE}
  IDirect3DExecuteBuffer = interface (IUnknown)
    ['{4417C145-33AD-11CF-816F-0000C020156E}']
{$ENDIF}
    (*** IDirect3DExecuteBuffer methods ***)
    function Initialize (lpDirect3DDevice: IDirect3DDevice;
        const lpDesc: TD3DExecuteBufferDesc) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function Lock (const lpDesc: TD3DExecuteBufferDesc) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function Unlock: HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetExecuteData (const lpData: TD3DExecuteData) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetExecuteData (var lpData: TD3DExecuteData) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function Validate (var lpdwOffset: DWORD; lpFunc: TD3DValidateCallback;
        lpUserArg: Pointer; dwReserved: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    (*** Warning!  Optimize is defined differently in the header files
         and the online documentation ***)
    function Optimize (dwFlags: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
  end;

(*
 * IDirect3DLight
 *)

{$IFDEF D2COM}
  IDirect3DLight = class (IUnknown)
{$ELSE}
  IDirect3DLight = interface (IUnknown)
    ['{4417C142-33AD-11CF-816F-0000C020156E}']
{$ENDIF}
    (*** IDirect3DLight methods ***)
    function Initialize (lpDirect3D: IDirect3D) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetLight (const lpLight) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetLight (var lpLight: TD3DLight) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
  end;

(*
 * IDirect3DMaterial
 *)

{$IFDEF D2COM}
  IDirect3DMaterial = class (IUnknown)
{$ELSE}
  IDirect3DMaterial = interface (IUnknown)
    ['{4417C144-33AD-11CF-816F-0000C020156E}']
{$ENDIF}
    (*** IDirect3DMaterial methods ***)
    function Initialize (lpDirect3D: IDirect3D) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetMaterial (const lpMat: TD3DMaterial) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetMaterial (var lpMat: TD3DMaterial) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetHandle (lpDirect3DDevice: IDirect3DDevice;
        var lpHandle: TD3DMaterialHandle) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function Reserve: HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function Unreserve: HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
  end;

(*
 * IDirect3DMaterial2
 *)

{$IFDEF D2COM}
  IDirect3DMaterial2 = class (IUnknown)
{$ELSE}
  IDirect3DMaterial2 = interface (IUnknown)
    ['{93281503-8cf8-11d0-89ab-00a0c9054129}']
{$ENDIF}
    (*** IDirect3DMaterial2 methods ***)
    function SetMaterial (const lpMat: TD3DMaterial) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetMaterial (var lpMat: TD3DMaterial) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetHandle (lpDirect3DDevice: IDirect3DDevice2;
        var lpHandle: TD3DMaterialHandle) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
  end;

(*
 * IDirect3DTexture
 *)

{$IFDEF D2COM}
  IDirect3DTexture = class (IUnknown)
{$ELSE}
  IDirect3DTexture = interface (IUnknown)
    ['{2CDCD9E0-25A0-11CF-A31A-00AA00B93356}']
{$ENDIF}
    (*** IDirect3DTexture methods ***)
    function Initialize (lpD3DDevice: IDirect3DDevice;
        lpDDSurface: IDirectDrawSurface) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetHandle (lpDirect3DDevice: IDirect3DDevice;
        var lpHandle: TD3DTextureHandle) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function PaletteChanged (dwStart: DWORD; dwCount: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function Load (lpD3DTexture: IDirect3DTexture) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function Unload: HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
  end;

(*
 * IDirect3DTexture2
 *)

{$IFDEF D2COM}
  IDirect3DTexture2 = class (IUnknown)
{$ELSE}
  IDirect3DTexture2 = interface (IUnknown)
    ['{93281502-8cf8-11d0-89ab-00a0c9054129}']
{$ENDIF}
    (*** IDirect3DTexture2 methods ***)
    function GetHandle (lpDirect3DDevice2: IDirect3DDevice2;
        var lpHandle: TD3DTextureHandle) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function PaletteChanged (dwStart: DWORD; dwCount: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function Load (lpD3DTexture2: IDirect3DTexture2) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
  end;

(*
 * IDirect3DViewport
 *)

{$IFDEF D2COM}
  IDirect3DViewport = class (IUnknown)
{$ELSE}
  IDirect3DViewport = interface (IUnknown)
    ['{4417C146-33AD-11CF-816F-0000C020156E}']
{$ENDIF}
    (*** IDirect3DViewport methods ***)
    function Initialize (lpDirect3D: IDirect3D) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetViewport (var lpData: TD3DViewport) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetViewport (const lpData: TD3DViewport) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function TransformVertices (dwVertexCount: DWORD;
        var lpData: TD3DTransformData; dwFlags: DWORD;
        var lpOffscreen: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function LightElements (dwElementCount: DWORD;
        var lpData: TD3DLightData) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetBackground (hMat: TD3DMaterialHandle) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetBackground (hMat: TD3DMaterialHandle) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetBackgroundDepth (lpDDSurface: IDirectDrawSurface) :
        HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetBackgroundDepth (var lplpDDSurface: IDirectDrawSurface;
        var lpValid: BOOL) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function Clear (dwCount: DWORD; const lpRects: TD3DRect; dwFlags: DWORD) :
        HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function AddLight (lpDirect3DLight: IDirect3DLight) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function DeleteLight (lpDirect3DLight: IDirect3DLight) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function NextLight (lpDirect3DLight: IDirect3DLight;
        var lplpDirect3DLight: IDirect3DLight; dwFlags: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
  end;

(*
 * IDirect3DViewport2
 *)

{$IFDEF D2COM}
  IDirect3DViewport2 = class (IUnknown)
{$ELSE}
  IDirect3DViewport2 = interface (IUnknown)
    ['{93281500-8cf8-11d0-89ab-00a0c9054129}']
{$ENDIF}
    (*** IDirect3DViewport2 methods ***)
    function Initialize (lpDirect3D: IDirect3D) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetViewport (var lpData: TD3DViewport) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetViewport (const lpData: TD3DViewport) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function TransformVertices (dwVertexCount: DWORD;
        var lpData: TD3DTransformData; dwFlags: DWORD;
        var lpOffscreen: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function LightElements (dwElementCount: DWORD;
        var lpData: TD3DLightData) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetBackground (hMat: TD3DMaterialHandle) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetBackground (hMat: TD3DMaterialHandle) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetBackgroundDepth (lpDDSurface: IDirectDrawSurface) :
        HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetBackgroundDepth (var lplpDDSurface: IDirectDrawSurface;
        var lpValid: BOOL) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function Clear (dwCount: DWORD; const lpRects: TD3DRect; dwFlags: DWORD) :
        HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function AddLight (lpDirect3DLight: IDirect3DLight) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function DeleteLight (lpDirect3DLight: IDirect3DLight) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function NextLight (lpDirect3DLight: IDirect3DLight;
        var lplpDirect3DLight: IDirect3DLight; dwFlags: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    (*** IDirect3DViewport2 methods ***)
    function GetViewport2 (var lpData: TD3DViewport2) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetViewport2 (const lpData: TD3DViewport2) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
  end;

const
(****************************************************************************
 *
 * Flags for IDirect3DDevice::NextViewport
 *
 ****************************************************************************)

(*
 * Return the next viewport
 *)
  D3DNEXT_NEXT =	$00000001;

(*
 * Return the first viewport
 *)
  D3DNEXT_HEAD =	$00000002;

(*
 * Return the last viewport
 *)
  D3DNEXT_TAIL =	$00000004;


(****************************************************************************
 *
 * Flags for DrawPrimitive/DrawIndexedPrimitive
 *   Also valid for Begin/BeginIndexed
 *
 ****************************************************************************)

(*
 * Wait until the device is ready to draw the primitive
 * This will cause DP to not return DDERR_WASSTILLDRAWING
 *)
  D3DDP_WAIT =					$00000001;


(*
 * Hint that the primitives have been clipped by the application.
 *)
  D3DDP_DONOTCLIP =				$00000004;

(*
 * Hint that the extents need not be updated.
 *)
  D3DDP_DONOTUPDATEEXTENTS =	$00000008;


(*
 * Direct3D Errors
 * DirectDraw error codes are used when errors not specified here.
 *)

const
  D3D_OK                          = DD_OK;
  D3DERR_BADMAJORVERSION          = integer($88760000 + 700);
  D3DERR_BADMINORVERSION          = integer($88760000 + 701);

(*
 * An invalid device was requested by the application.
 *)
  D3DERR_INVALID_DEVICE   = integer($88760000 + 705);
  D3DERR_INITFAILED       = integer($88760000 + 706);

(*
 * SetRenderTarget attempted on a device that was
 * QI'd off the render target.
 *)
  D3DERR_DEVICEAGGREGATED = integer($88760000 + 707);

  D3DERR_EXECUTE_CREATE_FAILED    = integer($88760000 + 710);
  D3DERR_EXECUTE_DESTROY_FAILED   = integer($88760000 + 711);
  D3DERR_EXECUTE_LOCK_FAILED      = integer($88760000 + 712);
  D3DERR_EXECUTE_UNLOCK_FAILED    = integer($88760000 + 713);
  D3DERR_EXECUTE_LOCKED           = integer($88760000 + 714);
  D3DERR_EXECUTE_NOT_LOCKED       = integer($88760000 + 715);

  D3DERR_EXECUTE_FAILED           = integer($88760000 + 716);
  D3DERR_EXECUTE_CLIPPED_FAILED   = integer($88760000 + 717);

  D3DERR_TEXTURE_NO_SUPPORT       = integer($88760000 + 720);
  D3DERR_TEXTURE_CREATE_FAILED    = integer($88760000 + 721);
  D3DERR_TEXTURE_DESTROY_FAILED   = integer($88760000 + 722);
  D3DERR_TEXTURE_LOCK_FAILED      = integer($88760000 + 723);
  D3DERR_TEXTURE_UNLOCK_FAILED    = integer($88760000 + 724);
  D3DERR_TEXTURE_LOAD_FAILED      = integer($88760000 + 725);
  D3DERR_TEXTURE_SWAP_FAILED      = integer($88760000 + 726);
  D3DERR_TEXTURE_LOCKED           = integer($88760000 + 727);
  D3DERR_TEXTURE_NOT_LOCKED       = integer($88760000 + 728);
  D3DERR_TEXTURE_GETSURF_FAILED   = integer($88760000 + 729);

  D3DERR_MATRIX_CREATE_FAILED     = integer($88760000 + 730);
  D3DERR_MATRIX_DESTROY_FAILED    = integer($88760000 + 731);
  D3DERR_MATRIX_SETDATA_FAILED    = integer($88760000 + 732);
  D3DERR_MATRIX_GETDATA_FAILED    = integer($88760000 + 733);
  D3DERR_SETVIEWPORTDATA_FAILED   = integer($88760000 + 734);

  D3DERR_INVALIDCURRENTVIEWPORT   = integer($88760000 + 735);
  D3DERR_INVALIDPRIMITIVETYPE     = integer($88760000 + 736);
  D3DERR_INVALIDVERTEXTYPE        = integer($88760000 + 737);
  D3DERR_TEXTURE_BADSIZE          = integer($88760000 + 738);
  D3DERR_INVALIDRAMPTEXTURE	  = integer($88760000 + 739);

  D3DERR_MATERIAL_CREATE_FAILED   = integer($88760000 + 740);
  D3DERR_MATERIAL_DESTROY_FAILED  = integer($88760000 + 741);
  D3DERR_MATERIAL_SETDATA_FAILED  = integer($88760000 + 742);
  D3DERR_MATERIAL_GETDATA_FAILED  = integer($88760000 + 743);
  D3DERR_INVALIDPALETTE	          = integer($88760000 + 744);

  D3DERR_ZBUFF_NEEDS_SYSTEMMEMORY = integer($88760000 + 745);
  D3DERR_ZBUFF_NEEDS_VIDEOMEMORY  = integer($88760000 + 746);
  D3DERR_SURFACENOTINVIDMEM       = integer($88760000 + 747);

  D3DERR_LIGHT_SET_FAILED         = integer($88760000 + 750);
  D3DERR_LIGHTHASVIEWPORT	  = integer($88760000 + 751);
  D3DERR_LIGHTNOTINTHISVIEWPORT   = integer($88760000 + 752);

  D3DERR_SCENE_IN_SCENE           = integer($88760000 + 760);
  D3DERR_SCENE_NOT_IN_SCENE       = integer($88760000 + 761);
  D3DERR_SCENE_BEGIN_FAILED       = integer($88760000 + 762);
  D3DERR_SCENE_END_FAILED         = integer($88760000 + 763);

  D3DERR_INBEGIN                  = integer($88760000 + 770);
  D3DERR_NOTINBEGIN               = integer($88760000 + 771);
  D3DERR_NOVIEWPORTS              = integer($88760000 + 772);
  D3DERR_VIEWPORTDATANOTSET       = integer($88760000 + 773);
  D3DERR_VIEWPORTHASNODEVICE      = integer($88760000 + 774);
  D3DERR_NOCURRENTVIEWPORT        = integer($88760000 + 775);

procedure DisableFPUExceptions;
procedure EnableFPUExceptions;

implementation

procedure DisableFPUExceptions;
var
  FPUControlWord: WORD;
asm
  FSTCW   FPUControlWord;
  OR      FPUControlWord, $4 + $1; { Divide by zero + invalid operation }
  FLDCW   FPUControlWord;
end;

procedure EnableFPUExceptions;
var
  FPUControlWord: WORD;
asm
  FSTCW   FPUControlWord;
  AND     FPUControlWord, $FFFF - $4 - $1; { Divide by zero + invalid operation }
  FLDCW   FPUControlWord;
end;

initialization
  DisableFPUExceptions;
end.
