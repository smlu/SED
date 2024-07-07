(*==========================================================================;
 *
 *  Copyright (C) 1995-1997 Microsoft Corporation.  All Rights Reserved.
 *
 *  File:	d3dcaps.h
 *  Content:	Direct3D capabilities include file
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

unit D3DCaps;

{$INCLUDE COMSWITCH.INC}            

interface

uses
{$IFDEF D2COM}
  OLE2,
{$ENDIF}
  Windows,
  D3DTypes,
  DDraw;

(* Description of capabilities of transform *)

type
  TD3DTransformCaps = packed record
    dwSize: DWORD;
    dwCaps: DWORD;
  end;

const
  D3DTRANSFORMCAPS_CLIP         = $00000001; (* Will clip whilst transforming *)

(* Description of capabilities of lighting *)

type
  TD3DLightingCaps = packed record
    dwSize: DWORD;
    dwCaps: DWORD;                   (* Lighting caps *)
    dwLightingModel: DWORD;          (* Lighting model - RGB or mono *)
    dwNumLights: DWORD;              (* Number of lights that can be handled *)
  end;

const
  D3DLIGHTINGMODEL_RGB            = $00000001;
  D3DLIGHTINGMODEL_MONO           = $00000002;

  D3DLIGHTCAPS_POINT              = $00000001; (* Point lights supported *)
  D3DLIGHTCAPS_SPOT               = $00000002; (* Spot lights supported *)
  D3DLIGHTCAPS_DIRECTIONAL        = $00000004; (* Directional lights supported *)
  D3DLIGHTCAPS_PARALLELPOINT      = $00000008; (* Parallel point lights supported *)
  D3DLIGHTCAPS_GLSPOT             = $00000010; (* GL syle spot lights supported *)

(* Description of capabilities for each primitive type *)

type
  TD3DPrimCaps = packed record
    dwSize: DWORD;
    dwMiscCaps: DWORD;                 (* Capability flags *)
    dwRasterCaps: DWORD;
    dwZCmpCaps: DWORD;
    dwSrcBlendCaps: DWORD;
    dwDestBlendCaps: DWORD;
    dwAlphaCmpCaps: DWORD;
    dwShadeCaps: DWORD;
    dwTextureCaps: DWORD;
    dwTextureFilterCaps: DWORD;
    dwTextureBlendCaps: DWORD;
    dwTextureAddressCaps: DWORD;
    dwStippleWidth: DWORD;             (* maximum width and height of *)
    dwStippleHeight: DWORD;            (* of supported stipple (up to 32x32) *)
  end;

const
(* TD3DPrimCaps dwMiscCaps *)

  D3DPMISCCAPS_MASKPLANES         = $00000001;
  D3DPMISCCAPS_MASKZ              = $00000002;
  D3DPMISCCAPS_LINEPATTERNREP     = $00000004;
  D3DPMISCCAPS_CONFORMANT         = $00000008;
  D3DPMISCCAPS_CULLNONE           = $00000010;
  D3DPMISCCAPS_CULLCW             = $00000020;
  D3DPMISCCAPS_CULLCCW            = $00000040;

(* TD3DPrimCaps dwRasterCaps *)

  D3DPRASTERCAPS_DITHER           = $00000001;
  D3DPRASTERCAPS_ROP2             = $00000002;
  D3DPRASTERCAPS_XOR              = $00000004;
  D3DPRASTERCAPS_PAT              = $00000008;
  D3DPRASTERCAPS_ZTEST            = $00000010;
  D3DPRASTERCAPS_SUBPIXEL         = $00000020;
  D3DPRASTERCAPS_SUBPIXELX        = $00000040;
  D3DPRASTERCAPS_FOGVERTEX        = $00000080;
  D3DPRASTERCAPS_FOGTABLE         = $00000100;
  D3DPRASTERCAPS_STIPPLE          = $00000200;
  D3DPRASTERCAPS_ANTIALIASSORTDEPENDENT   = $00000400;
  D3DPRASTERCAPS_ANTIALIASSORTINDEPENDENT = $00000800;
  D3DPRASTERCAPS_ANTIALIASEDGES           = $00001000;
  D3DPRASTERCAPS_MIPMAPLODBIAS            = $00002000;
  D3DPRASTERCAPS_ZBIAS                    = $00004000;
  D3DPRASTERCAPS_ZBUFFERLESSHSR           = $00008000;
  D3DPRASTERCAPS_FOGRANGE                 = $00010000;
  D3DPRASTERCAPS_ANISOTROPY               = $00020000;

(* TD3DPrimCaps dwZCmpCaps, dwAlphaCmpCaps *)

const
  D3DPCMPCAPS_NEVER               = $00000001;
  D3DPCMPCAPS_LESS                = $00000002;
  D3DPCMPCAPS_EQUAL               = $00000004;
  D3DPCMPCAPS_LESSEQUAL           = $00000008;
  D3DPCMPCAPS_GREATER             = $00000010;
  D3DPCMPCAPS_NOTEQUAL            = $00000020;
  D3DPCMPCAPS_GREATEREQUAL        = $00000040;
  D3DPCMPCAPS_ALWAYS              = $00000080;

(* TD3DPrimCaps dwSourceBlendCaps, dwDestBlendCaps *)

  D3DPBLENDCAPS_ZERO              = $00000001;
  D3DPBLENDCAPS_ONE               = $00000002;
  D3DPBLENDCAPS_SRCCOLOR          = $00000004;
  D3DPBLENDCAPS_INVSRCCOLOR       = $00000008;
  D3DPBLENDCAPS_SRCALPHA          = $00000010;
  D3DPBLENDCAPS_INVSRCALPHA       = $00000020;
  D3DPBLENDCAPS_DESTALPHA         = $00000040;
  D3DPBLENDCAPS_INVDESTALPHA      = $00000080;
  D3DPBLENDCAPS_DESTCOLOR         = $00000100;
  D3DPBLENDCAPS_INVDESTCOLOR      = $00000200;
  D3DPBLENDCAPS_SRCALPHASAT       = $00000400;
  D3DPBLENDCAPS_BOTHSRCALPHA      = $00000800;
  D3DPBLENDCAPS_BOTHINVSRCALPHA   = $00001000;

(* TD3DPrimCaps dwShadeCaps *)

  D3DPSHADECAPS_COLORFLATMONO             = $00000001;
  D3DPSHADECAPS_COLORFLATRGB              = $00000002;
  D3DPSHADECAPS_COLORGOURAUDMONO          = $00000004;
  D3DPSHADECAPS_COLORGOURAUDRGB           = $00000008;
  D3DPSHADECAPS_COLORPHONGMONO            = $00000010;
  D3DPSHADECAPS_COLORPHONGRGB             = $00000020;

  D3DPSHADECAPS_SPECULARFLATMONO          = $00000040;
  D3DPSHADECAPS_SPECULARFLATRGB           = $00000080;
  D3DPSHADECAPS_SPECULARGOURAUDMONO       = $00000100;
  D3DPSHADECAPS_SPECULARGOURAUDRGB        = $00000200;
  D3DPSHADECAPS_SPECULARPHONGMONO         = $00000400;
  D3DPSHADECAPS_SPECULARPHONGRGB          = $00000800;

  D3DPSHADECAPS_ALPHAFLATBLEND            = $00001000;
  D3DPSHADECAPS_ALPHAFLATSTIPPLED         = $00002000;
  D3DPSHADECAPS_ALPHAGOURAUDBLEND         = $00004000;
  D3DPSHADECAPS_ALPHAGOURAUDSTIPPLED      = $00008000;
  D3DPSHADECAPS_ALPHAPHONGBLEND           = $00010000;
  D3DPSHADECAPS_ALPHAPHONGSTIPPLED        = $00020000;

  D3DPSHADECAPS_FOGFLAT                   = $00040000;
  D3DPSHADECAPS_FOGGOURAUD                = $00080000;
  D3DPSHADECAPS_FOGPHONG                  = $00100000;

(* TD3DPrimCaps dwTextureCaps *)

  D3DPTEXTURECAPS_PERSPECTIVE     = $00000001;
  D3DPTEXTURECAPS_POW2            = $00000002;
  D3DPTEXTURECAPS_ALPHA           = $00000004;
  D3DPTEXTURECAPS_TRANSPARENCY    = $00000008;
  D3DPTEXTURECAPS_BORDER          = $00000010;
  D3DPTEXTURECAPS_SQUAREONLY      = $00000020;

(* TD3DPrimCaps dwTextureFilterCaps *)

  D3DPTFILTERCAPS_NEAREST         = $00000001;
  D3DPTFILTERCAPS_LINEAR          = $00000002;
  D3DPTFILTERCAPS_MIPNEAREST      = $00000004;
  D3DPTFILTERCAPS_MIPLINEAR       = $00000008;
  D3DPTFILTERCAPS_LINEARMIPNEAREST = $00000010;
  D3DPTFILTERCAPS_LINEARMIPLINEAR = $00000020;

(* TD3DPrimCaps dwTextureBlendCaps *)

  D3DPTBLENDCAPS_DECAL            = $00000001;
  D3DPTBLENDCAPS_MODULATE         = $00000002;
  D3DPTBLENDCAPS_DECALALPHA       = $00000004;
  D3DPTBLENDCAPS_MODULATEALPHA    = $00000008;
  D3DPTBLENDCAPS_DECALMASK        = $00000010;
  D3DPTBLENDCAPS_MODULATEMASK     = $00000020;
  D3DPTBLENDCAPS_COPY             = $00000040;
  D3DPTBLENDCAPS_ADD		  = $00000080;

(* TD3DPrimCaps dwTextureAddressCaps *)
  D3DPTADDRESSCAPS_WRAP           = $00000001;
  D3DPTADDRESSCAPS_MIRROR         = $00000002;
  D3DPTADDRESSCAPS_CLAMP          = $00000004;
  D3DPTADDRESSCAPS_BORDER         = $00000008;
  D3DPTADDRESSCAPS_INDEPENDENTUV  = $00000010;

(*
 * Description for a device.
 * This is used to describe a device that is to be created or to query
 * the current device.
 *)

type
  PD3DDeviceDesc = ^TD3DDeviceDesc;
  TD3DDeviceDesc = packed record
    dwSize: DWORD;                       (* Size of TD3DDeviceDesc structure *)
    dwFlags: DWORD;                      (* Indicates which fields have valid data *)
    dcmColorModel: TD3DColorModel;        (* Color model of device *)
    dwDevCaps: DWORD;                    (* Capabilities of device *)
    dtcTransformCaps: TD3DTransformCaps;  (* Capabilities of transform *)
    bClipping: BOOL;                     (* Device can do 3D clipping *)
    dlcLightingCaps: TD3DLightingCaps;    (* Capabilities of lighting *)
    dpcLineCaps: TD3DPrimCaps;
    dpcTriCaps: TD3DPrimCaps;
    dwDeviceRenderBitDepth: DWORD;       (* One of DDBB_8, 16, etc.. *)
    dwDeviceZBufferBitDepth: DWORD;      (* One of DDBD_16, 32, etc.. *)
    dwMaxBufferSize: DWORD;              (* Maximum execute buffer size *)
    dwMaxVertexCount: DWORD;             (* Maximum vertex count *)
    // *** New fields for DX5 *** //

    // Width and height caps are 0 for legacy HALs.
    dwMinTextureWidth, dwMinTextureHeight  : DWORD;
    dwMaxTextureWidth, dwMaxTextureHeight  : DWORD;
    dwMinStippleWidth, dwMaxStippleWidth   : DWORD;
    dwMinStippleHeight, dwMaxStippleHeight : DWORD;
  end;

const
  D3DDEVICEDESCSIZE = sizeof(TD3DDeviceDesc);

type
  TD3DEnumDevicesCallback = function (const lpGuid: TGUID;
      lpDeviceDescription: LPSTR; lpDeviceName: LPSTR;
      const lpD3DHWDeviceDesc: TD3DDeviceDesc;
      const lpD3DHELDeviceDesc: TD3DDeviceDesc;
      lpUserArg: Pointer) : HResult; stdcall;

(* TD3DDeviceDesc dwFlags indicating valid fields *)

const
  D3DDD_COLORMODEL            = $00000001; (* dcmColorModel is valid *)
  D3DDD_DEVCAPS               = $00000002; (* dwDevCaps is valid *)
  D3DDD_TRANSFORMCAPS         = $00000004; (* dtcTransformCaps is valid *)
  D3DDD_LIGHTINGCAPS          = $00000008; (* dlcLightingCaps is valid *)
  D3DDD_BCLIPPING             = $00000010; (* bClipping is valid *)
  D3DDD_LINECAPS              = $00000020; (* dpcLineCaps is valid *)
  D3DDD_TRICAPS               = $00000040; (* dpcTriCaps is valid *)
  D3DDD_DEVICERENDERBITDEPTH  = $00000080; (* dwDeviceRenderBitDepth is valid *)
  D3DDD_DEVICEZBUFFERBITDEPTH = $00000100; (* dwDeviceZBufferBitDepth is valid *)
  D3DDD_MAXBUFFERSIZE         = $00000200; (* dwMaxBufferSize is valid *)
  D3DDD_MAXVERTEXCOUNT        = $00000400; (* dwMaxVertexCount is valid *)

(* TD3DDeviceDesc dwDevCaps flags *)

  D3DDEVCAPS_FLOATTLVERTEX        = $00000001; (* Device accepts floating point *)
                                                    (* for post-transform vertex data *)
  D3DDEVCAPS_SORTINCREASINGZ      = $00000002; (* Device needs data sorted for increasing Z*)
  D3DDEVCAPS_SORTDECREASINGZ      = $00000004; (* Device needs data sorted for decreasing Z*)
  D3DDEVCAPS_SORTEXACT            = $00000008; (* Device needs data sorted exactly *)

  D3DDEVCAPS_EXECUTESYSTEMMEMORY  = $00000010; (* Device can use execute buffers from system memory *)
  D3DDEVCAPS_EXECUTEVIDEOMEMORY   = $00000020; (* Device can use execute buffers from video memory *)
  D3DDEVCAPS_TLVERTEXSYSTEMMEMORY = $00000040; (* Device can use TL buffers from system memory *)
  D3DDEVCAPS_TLVERTEXVIDEOMEMORY  = $00000080; (* Device can use TL buffers from video memory *)
  D3DDEVCAPS_TEXTURESYSTEMMEMORY  = $00000100; (* Device can texture from system memory *)
  D3DDEVCAPS_TEXTUREVIDEOMEMORY   = $00000200; (* Device can texture from device memory *)
  D3DDEVCAPS_DRAWPRIMTLVERTEX     = $00000400; (* Device can draw TLVERTEX primitives *)
  D3DDEVCAPS_CANRENDERAFTERFLIP	  = $00000800; (* Device can render without waiting for flip to complete *)
  D3DDEVCAPS_TEXTURENONLOCALVIDMEM = $00001000; (* Device can texture from nonlocal video memory *)

  D3DFDS_COLORMODEL        = $00000001; (* Match color model *)
  D3DFDS_GUID              = $00000002; (* Match guid *)
  D3DFDS_HARDWARE          = $00000004; (* Match hardware/software *)
  D3DFDS_TRIANGLES         = $00000008; (* Match in triCaps *)
  D3DFDS_LINES             = $00000010; (* Match in lineCaps  *)
  D3DFDS_MISCCAPS          = $00000020; (* Match primCaps.dwMiscCaps *)
  D3DFDS_RASTERCAPS        = $00000040; (* Match primCaps.dwRasterCaps *)
  D3DFDS_ZCMPCAPS          = $00000080; (* Match primCaps.dwZCmpCaps *)
  D3DFDS_ALPHACMPCAPS      = $00000100; (* Match primCaps.dwAlphaCmpCaps *)
  D3DFDS_SRCBLENDCAPS      = $00000200; (* Match primCaps.dwSourceBlendCaps *)
  D3DFDS_DSTBLENDCAPS      = $00000400; (* Match primCaps.dwDestBlendCaps *)
  D3DFDS_SHADECAPS         = $00000800; (* Match primCaps.dwShadeCaps *)
  D3DFDS_TEXTURECAPS       = $00001000; (* Match primCaps.dwTextureCaps *)
  D3DFDS_TEXTUREFILTERCAPS = $00002000; (* Match primCaps.dwTextureFilterCaps *)
  D3DFDS_TEXTUREBLENDCAPS  = $00004000; (* Match primCaps.dwTextureBlendCaps *)
  D3DFDS_TEXTUREADDRESSCAPS  = $00008000; (* Match primCaps.dwTextureBlendCaps *)

(*
 * FindDevice arguments
 *)

type
  TD3DFindDeviceSearch = packed record
    dwSize: DWORD;
    dwFlags: DWORD;
    bHardware: BOOL;
    dcmColorModel: TD3DColorModel;
    guid: TGUID;
    dwCaps: DWORD;
    dpcPrimCaps: TD3DPrimCaps;
  end;

  TD3DFindDeviceResult = packed record
    dwSize: DWORD;
    guid: TGUID;               (* guid which matched *)
    ddHwDesc: TD3DDeviceDesc;   (* hardware TD3DDeviceDesc *)
    ddSwDesc: TD3DDeviceDesc;   (* software TD3DDeviceDesc *)
  end;

(*
 * Description of execute buffer.
 *)

  TD3DExecuteBufferDesc = packed record
    dwSize: DWORD;         (* size of this structure *)
    dwFlags: DWORD;        (* flags indicating which fields are valid *)
    dwCaps: DWORD;         (* capabilities of execute buffer *)
    dwBufferSize: DWORD;   (* size of execute buffer data *)
    lpData: Pointer;       (* pointer to actual data *)
  end;

(* D3DEXECUTEBUFFER dwFlags indicating valid fields *)

const
  D3DDEB_BUFSIZE          = $00000001;     (* buffer size valid *)
  D3DDEB_CAPS             = $00000002;     (* caps valid *)
  D3DDEB_LPDATA           = $00000004;     (* lpData valid *)

(* D3DEXECUTEBUFFER dwCaps *)

  D3DDEBCAPS_SYSTEMMEMORY = $00000001;     (* buffer in system memory *)
  D3DDEBCAPS_VIDEOMEMORY  = $00000002;     (* buffer in device memory *)
  D3DDEBCAPS_MEM          = (D3DDEBCAPS_SYSTEMMEMORY or D3DDEBCAPS_VIDEOMEMORY);

implementation

end.

