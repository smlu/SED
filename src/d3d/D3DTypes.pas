(*==========================================================================;
 *
 *  Copyright (C) 1995-1997 Microsoft Corporation.  All Rights Reserved.
 *
 *  File:	d3dtypes.h
 *  Content:	Direct3D types include file
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

unit D3DTypes;


{$INCLUDE COMSWITCH.INC}

interface

uses
{$IFDEF D2COM}
  OLE2,
{$ENDIF}
  Windows,
  DDraw;

(* TD3DValue is the fundamental Direct3D fractional data type *)

type
  TRefClsID = TGUID;

type
  TD3DValue = single;
  TD3DFixed = LongInt;
  float = TD3DValue;
  PD3DColor = ^TD3DColor;
  TD3DColor = DWORD;

// #define D3DVALP(val, prec) ((float)(val))
// #define D3DVAL(val) ((float)(val))
function D3DVAL(val: variant) : float;
function D3DDivide(a,b: variant) : variant;
function D3DMultiply(a,b: variant) : variant;

(*
 * Format of CI colors is
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |    alpha      |         color index           |   fraction    |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *)

// #define CI_GETALPHA(ci)    ((ci) >> 24)
function CI_GETALPHA(ci: DWORD) : DWORD;

// #define CI_GETINDEX(ci)    (((ci) >> 8) & 0xffff)
function CI_GETINDEX(ci: DWORD) : DWORD;

// #define CI_GETFRACTION(ci) ((ci) & 0xff)
function CI_GETFRACTION(ci: DWORD) : DWORD;

// #define CI_ROUNDINDEX(ci)  CI_GETINDEX((ci) + 0x80)
function CI_ROUNDINDEX(ci: DWORD) : DWORD;

// #define CI_MASKALPHA(ci)   ((ci) & 0xffffff)
function CI_MASKALPHA(ci: DWORD) : DWORD;

// #define CI_MAKE(a, i, f)    (((a) << 24) | ((i) << 8) | (f))
function CI_MAKE(a,i,f: DWORD) : DWORD;

(*
 * Format of RGBA colors is
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |    alpha      |      red      |     green     |     blue      |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *)

// #define RGBA_GETALPHA(rgb)      ((rgb) >> 24)
function RGBA_GETALPHA(rgb: TD3DColor) : DWORD;

// #define RGBA_GETRED(rgb)        (((rgb) >> 16) & 0xff)
function RGBA_GETRED(rgb: TD3DColor) : DWORD;

// #define RGBA_GETGREEN(rgb)      (((rgb) >> 8) & 0xff)
function RGBA_GETGREEN(rgb: TD3DColor) : DWORD;

// #define RGBA_GETBLUE(rgb)       ((rgb) & 0xff)
function RGBA_GETBLUE(rgb: TD3DColor) : DWORD;

// #define RGBA_MAKE(r, g, b, a)   ((TD3DColor) (((a) << 24) | ((r) << 16) | ((g) << 8) | (b)))
function RGBA_MAKE(r, g, b, a: DWORD) : TD3DColor;

(* D3DRGB and D3DRGBA may be used as initialisers for D3DCOLORs
 * The float values must be in the range 0..1
 *)

// #define D3DRGB(r, g, b) \
//     (0xff000000L | (((long)((r) * 255)) << 16) | (((long)((g) * 255)) << 8) | (long)((b) * 255))
function D3DRGB(r, g, b: float) : TD3DColor;

// #define D3DRGBA(r, g, b, a) \
//     (  (((long)((a) * 255)) << 24) | (((long)((r) * 255)) << 16) \
//     |   (((long)((g) * 255)) << 8) | (long)((b) * 255) \
//    )
function D3DRGBA(r, g, b, a: float) : TD3DColor;

(*
 * Format of RGB colors is
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |    ignored    |      red      |     green     |     blue      |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *)

// #define RGB_GETRED(rgb)         (((rgb) >> 16) & 0xff)
function RGB_GETRED(rgb: TD3DColor) : DWORD;

// #define RGB_GETGREEN(rgb)       (((rgb) >> 8) & 0xff)
function RGB_GETGREEN(rgb: TD3DColor) : DWORD;

// #define RGB_GETBLUE(rgb)        ((rgb) & 0xff)
function RGB_GETBLUE(rgb: TD3DColor) : DWORD;

// #define RGBA_SETALPHA(rgba, x) (((x) << 24) | ((rgba) & 0x00ffffff))
function RGBA_SETALPHA(rgba: TD3DColor; x: DWORD) : TD3DColor;

// #define RGB_MAKE(r, g, b)       ((TD3DColor) (((r) << 16) | ((g) << 8) | (b)))
function RGB_MAKE(r, g, b: DWORD) : TD3DColor;

// #define RGBA_TORGB(rgba)       ((TD3DColor) ((rgba) & 0xffffff))
function RGBA_TORGB(rgba: TD3DColor) : TD3DColor;

// #define RGB_TORGBA(rgb)        ((TD3DColor) ((rgb) | 0xff000000))
function RGB_TORGBA(rgb: TD3DColor) : TD3DColor;

(*
 * Flags for Enumerate functions
 *)

(*
 * Stop the enumeration
 *)

const
  D3DENUMRET_CANCEL                        = DDENUMRET_CANCEL;

(*
 * Continue the enumeration
 *)

  D3DENUMRET_OK                            = DDENUMRET_OK;

type
// typedef HResult (WINAPI* LPD3DVALIDATECALLBACK)(LPVOID lpUserArg, DWORD dwOffset);
  TD3DValidateCallback = function (lpUserArg: Pointer;
      dwOffset: DWORD): HResult; stdcall;
// typedef HResult (WINAPI* LPD3DENUMTEXTUREFORMATSCALLBACK)(LPDDSURFACEDESC lpDdsd, LPVOID lpContext);
  TD3DEnumTextureFormatsCallback = function (const lpDdsd: TDDSurfaceDesc;
      lpUserArg: Pointer): HResult; stdcall;

  PD3DMaterialHandle = ^TD3DMaterialHandle;
  TD3DMaterialHandle = DWORD;

  PD3DTextureHandle = ^TD3DTextureHandle;
  TD3DTextureHandle = DWORD;

  PD3DMatrixHandle = ^TD3DMatrixHandle;
  TD3DMatrixHandle = DWORD;

  PD3DColorValue = ^TD3DColorValue;
  TD3DColorValue = packed record
    case Integer of
    0: (
      r: TD3DValue;
      g: TD3DValue;
      b: TD3DValue;
      a: TD3DValue;
     );
    1: (
      dvR: TD3DValue;
      dvG: TD3DValue;
      dvB: TD3DValue;
      dvA: TD3DValue;
     );
  end;

  PD3DRect = ^TD3DRect;
  TD3DRect = packed record
    case Integer of
    0: (
      x1: LongInt;
      y1: LongInt;
      x2: LongInt;
      y2: LongInt;
     );
    1: (
      lX1: LongInt;
      lY1: LongInt;
      lX2: LongInt;
      lY2: LongInt;
     );
     2: (
       a: array[0..3] of LongInt;
     );
  end;

  PD3DVector = ^TD3DVector;
  TD3DVector = packed record
    case Integer of
    0: (
      x: TD3DValue;
      y: TD3DValue;
      z: TD3DValue;
     );
    1: (
      dvX: TD3DValue;
      dvY: TD3DValue;
      dvZ: TD3DValue;
     );
  end;

    // Addition and subtraction
  function VectorAdd(const v1, v2: TD3DVector) : TD3DVector;
  function VectorSub(const v1, v2: TD3DVector) : TD3DVector;
    // Scalar multiplication and division
  function VectorMulS(const v: TD3DVector; s: TD3DValue) : TD3DVector;
  function VectorDivS(const v: TD3DVector; s: TD3DValue) : TD3DVector;
    // Memberwise multiplication and division
  function VectorMul(const v1, v2: TD3DVector) : TD3DVector;
  function VectorDiv(const v1, v2: TD3DVector) : TD3DVector;
    // Vector dominance
  function VectorSmaller(v1, v2: TD3DVector) : boolean;
  function VectorSmallerEquel(v1, v2: TD3DVector) : boolean;
    // Bitwise equality
  function VectorEquel(v1, v2: TD3DVector) : boolean;
    // Length-related functions
  function VectorSquareMagnitude(v: TD3DVector) : TD3DValue;
  function VectorMagnitude(v: TD3DVector) : TD3DValue;
    // Returns vector with same direction and unit length
  function VectorNormalize(const v: TD3DVector) : TD3DVector;
    // Return min/max component of the input vector
  function VectorMin(v: TD3DVector) : TD3DValue;
  function VectorMax(v: TD3DVector) : TD3DValue;
    // Return memberwise min/max of input vectors
  function VectorMinimize(const v1, v2: TD3DVector) : TD3DVector;
  function VectorMaximize(const v1, v2: TD3DVector) : TD3DVector;
    // Dot and cross product
  function VectorDotProduct(v1, v2: TD3DVector) : TD3DValue;
  function VectorCrossProduct(const v1, v2: TD3DVector) : TD3DVector;

type
(*
 * Vertex data types supported in an ExecuteBuffer.
 *)

(*
 * Homogeneous vertices
 *)

  TD3DHVertex = packed record
    dwFlags: DWORD;        (* Homogeneous clipping flags *)
    case Integer of
    0: (
      hx: TD3DValue;
      hy: TD3DValue;
      hz: TD3DValue;
     );
    1: (
      dvHX: TD3DValue;
      dvHY: TD3DValue;
      dvHZ: TD3DValue;
     );
  end;

(*
 * Transformed/lit vertices
 *)

  TD3DTLVertex = packed record
    case Integer of
    0: (
      sx: TD3DValue;             (* Screen coordinates *)
      sy: TD3DValue;
      sz: TD3DValue;
      rhw: TD3DValue;            (* Reciprocal of homogeneous w *)
      color: TD3DColor;          (* Vertex color *)
      specular: TD3DColor;       (* Specular component of vertex *)
      tu: TD3DValue;             (* Texture coordinates *)
      tv: TD3DValue;
     );
    1: (
      dvSX: TD3DValue;
      dvSY: TD3DValue;
      dvSZ: TD3DValue;
      dvRHW: TD3DValue;
      dcColor: TD3DColor;
      dcSpecular: TD3DColor;
      dvTU: TD3DValue;
      dvTV: TD3DValue;
     );
  end;

(*
 * Untransformed/lit vertices
 *)

  TD3DLVertex = packed record
    case Integer of
    0: (
      x: TD3DValue;             (* Homogeneous coordinates *)
      y: TD3DValue;
      z: TD3DValue;
      dwReserved: DWORD;
      color: TD3DColor;         (* Vertex color *)
      specular: TD3DColor;      (* Specular component of vertex *)
      tu: TD3DValue;            (* Texture coordinates *)
      tv: TD3DValue;
     );
    1: (
      dvX: TD3DValue;
      dvY: TD3DValue;
      dvZ: TD3DValue;
      UNIONFILLER1d: DWORD;
      dcColor: TD3DColor;
      dcSpecular: TD3DColor;
      dvTU: TD3DValue;
      dvTV: TD3DValue;
     );
  end;

(*
 * Untransformed/unlit vertices
 *)

  TD3DVertex = packed record
    case Integer of
    0: (
      x: TD3DValue;             (* Homogeneous coordinates *)
      y: TD3DValue;
      z: TD3DValue;
      nx: TD3DValue;            (* Normal *)
      ny: TD3DValue;
      nz: TD3DValue;
      tu: TD3DValue;            (* Texture coordinates *)
      tv: TD3DValue;
     );
    1: (
      dvX: TD3DValue;
      dvY: TD3DValue;
      dvZ: TD3DValue;
      dvNX: TD3DValue;
      dvNY: TD3DValue;
      dvNZ: TD3DValue;
      dvTU: TD3DValue;
      dvTV: TD3DValue;
     );
  end;

(*
 * Matrix, viewport, and tranformation structures and definitions.
 *)

  TD3DMatrix = packed record
    _11, _12, _13, _14: TD3DValue;
    _21, _22, _23, _24: TD3DValue;
    _31, _32, _33, _34: TD3DValue;
    _41, _42, _43, _44: TD3DValue;
  end;

  TD3DMatrix_ = array [0..3, 0..3] of TD3DValue;

  TD3DViewport = packed record
    dwSize: DWORD;
    dwX: DWORD;
    dwY: DWORD;               (* Top left *)
    dwWidth: DWORD;
    dwHeight: DWORD;          (* Dimensions *)
    dvScaleX: TD3DValue;       (* Scale homogeneous to screen *)
    dvScaleY: TD3DValue;       (* Scale homogeneous to screen *)
    dvMaxX: TD3DValue;         (* Min/max homogeneous x coord *)
    dvMaxY: TD3DValue;         (* Min/max homogeneous y coord *)
    dvMinZ: TD3DValue;
    dvMaxZ: TD3DValue;         (* Min/max homogeneous z coord *)
  end;

  TD3DViewport2 = packed record
    dwSize: DWORD;
    dwX: DWORD;
    dwY: DWORD;               (* Top left *)
    dwWidth: DWORD;
    dwHeight: DWORD;          (* Dimensions *)
    dvClipX: TD3DValue;		(* Top left of clip volume *)
    dvClipY: TD3DValue;
    dvClipWidth: TD3DValue;	(* Clip Volume Dimensions *)
    dvClipHeight: TD3DValue;
    dvMinZ: TD3DValue;
    dvMaxZ: TD3DValue;         (* Min/max homogeneous z coord *)
  end;

(*
 * Values for clip fields.
 *)

const
  D3DCLIP_LEFT                            = $00000001;
  D3DCLIP_RIGHT                           = $00000002;
  D3DCLIP_TOP                             = $00000004;
  D3DCLIP_BOTTOM                          = $00000008;
  D3DCLIP_FRONT                           = $00000010;
  D3DCLIP_BACK                            = $00000020;
  D3DCLIP_GEN0                            = $00000040;
  D3DCLIP_GEN1                            = $00000080;
  D3DCLIP_GEN2                            = $00000100;
  D3DCLIP_GEN3                            = $00000200;
  D3DCLIP_GEN4                            = $00000400;
  D3DCLIP_GEN5                            = $00000800;

(*
 * Values for d3d status.
 *)

  D3DSTATUS_CLIPUNIONLEFT                 = D3DCLIP_LEFT;
  D3DSTATUS_CLIPUNIONRIGHT                = D3DCLIP_RIGHT;
  D3DSTATUS_CLIPUNIONTOP                  = D3DCLIP_TOP;
  D3DSTATUS_CLIPUNIONBOTTOM               = D3DCLIP_BOTTOM;
  D3DSTATUS_CLIPUNIONFRONT                = D3DCLIP_FRONT;
  D3DSTATUS_CLIPUNIONBACK                 = D3DCLIP_BACK;
  D3DSTATUS_CLIPUNIONGEN0                 = D3DCLIP_GEN0;
  D3DSTATUS_CLIPUNIONGEN1                 = D3DCLIP_GEN1;
  D3DSTATUS_CLIPUNIONGEN2                 = D3DCLIP_GEN2;
  D3DSTATUS_CLIPUNIONGEN3                 = D3DCLIP_GEN3;
  D3DSTATUS_CLIPUNIONGEN4                 = D3DCLIP_GEN4;
  D3DSTATUS_CLIPUNIONGEN5                 = D3DCLIP_GEN5;

  D3DSTATUS_CLIPINTERSECTIONLEFT          = $00001000;
  D3DSTATUS_CLIPINTERSECTIONRIGHT         = $00002000;
  D3DSTATUS_CLIPINTERSECTIONTOP           = $00004000;
  D3DSTATUS_CLIPINTERSECTIONBOTTOM        = $00008000;
  D3DSTATUS_CLIPINTERSECTIONFRONT         = $00010000;
  D3DSTATUS_CLIPINTERSECTIONBACK          = $00020000;
  D3DSTATUS_CLIPINTERSECTIONGEN0          = $00040000;
  D3DSTATUS_CLIPINTERSECTIONGEN1          = $00080000;
  D3DSTATUS_CLIPINTERSECTIONGEN2          = $00100000;
  D3DSTATUS_CLIPINTERSECTIONGEN3          = $00200000;
  D3DSTATUS_CLIPINTERSECTIONGEN4          = $00400000;
  D3DSTATUS_CLIPINTERSECTIONGEN5          = $00800000;
  D3DSTATUS_ZNOTVISIBLE                   = $01000000;

  D3DSTATUS_CLIPUNIONALL = (
            D3DSTATUS_CLIPUNIONLEFT or
            D3DSTATUS_CLIPUNIONRIGHT or
            D3DSTATUS_CLIPUNIONTOP or
            D3DSTATUS_CLIPUNIONBOTTOM or
            D3DSTATUS_CLIPUNIONFRONT or
            D3DSTATUS_CLIPUNIONBACK or
            D3DSTATUS_CLIPUNIONGEN0 or
            D3DSTATUS_CLIPUNIONGEN1 or
            D3DSTATUS_CLIPUNIONGEN2 or
            D3DSTATUS_CLIPUNIONGEN3 or
            D3DSTATUS_CLIPUNIONGEN4 or
            D3DSTATUS_CLIPUNIONGEN5);

  D3DSTATUS_CLIPINTERSECTIONALL = (
            D3DSTATUS_CLIPINTERSECTIONLEFT or
            D3DSTATUS_CLIPINTERSECTIONRIGHT or
            D3DSTATUS_CLIPINTERSECTIONTOP or
            D3DSTATUS_CLIPINTERSECTIONBOTTOM or
            D3DSTATUS_CLIPINTERSECTIONFRONT or
            D3DSTATUS_CLIPINTERSECTIONBACK or
            D3DSTATUS_CLIPINTERSECTIONGEN0 or
            D3DSTATUS_CLIPINTERSECTIONGEN1 or
            D3DSTATUS_CLIPINTERSECTIONGEN2 or
            D3DSTATUS_CLIPINTERSECTIONGEN3 or
            D3DSTATUS_CLIPINTERSECTIONGEN4 or
            D3DSTATUS_CLIPINTERSECTIONGEN5);

  D3DSTATUS_DEFAULT = (
            D3DSTATUS_CLIPINTERSECTIONALL or
            D3DSTATUS_ZNOTVISIBLE);

(*
 * Options for direct transform calls
 *)

  D3DTRANSFORM_CLIPPED       = $00000001;
  D3DTRANSFORM_UNCLIPPED     = $00000002;

type
  TD3DTransformData = packed record
    dwSize: DWORD;
    lpIn: Pointer;             (* Input vertices *)
    dwInSize: DWORD;           (* Stride of input vertices *)
    lpOut: Pointer;            (* Output vertices *)
    dwOutSize: DWORD;          (* Stride of output vertices *)
    lpHOut: ^TD3DHVertex;       (* Output homogeneous vertices *)
    dwClip: DWORD;             (* Clipping hint *)
    dwClipIntersection: DWORD;
    dwClipUnion: DWORD;        (* Union of all clip flags *)
    drExtent: TD3DRect;         (* Extent of transformed vertices *)
  end;

(*
 * Structure defining position and direction properties for lighting.
 *)

  TD3DLightingElement = packed record
    dvPosition: TD3DVector;           (* Lightable point in model space *)
    dvNormal: TD3DVector;             (* Normalised unit vector *)
  end;

(*
 * Structure defining material properties for lighting.
 *)

  TD3DMaterial = packed record
    dwSize: DWORD;
    case Integer of
    0: (
      diffuse: TD3DColorValue;        (* Diffuse color RGBA *)
      ambient: TD3DColorValue;        (* Ambient color RGB *)
      specular: TD3DColorValue;       (* Specular 'shininess' *)
      emissive: TD3DColorValue;       (* Emissive color RGB *)
      power: TD3DValue;               (* Sharpness if specular highlight *)
      hTexture: TD3DTextureHandle;    (* Handle to texture map *)
      dwRampSize: DWORD;
     );
    1: (
      dcvDiffuse: TD3DColorValue;
      dcvAmbient: TD3DColorValue;
      dcvSpecular: TD3DColorValue;
      dcvEmissive: TD3DColorValue;
      dvPower: TD3DValue;
     );
  end;

  TD3DLightType = (
    D3DLIGHT_INVALID_0,
    D3DLIGHT_POINT,
    D3DLIGHT_SPOT,
    D3DLIGHT_DIRECTIONAL,
    D3DLIGHT_PARALLELPOINT,
    D3DLIGHT_GLSPOT);

(*
 * Structure defining a light source and its properties.
 *)

  TD3DLight = packed record
    dwSize: DWORD;
    dltType: TD3DLightType;     (* Type of light source *)
    dcvColor: TD3DColorValue;   (* Color of light *)
    dvPosition: TD3DVector;     (* Position in world space *)
    dvDirection: TD3DVector;    (* Direction in world space *)
    dvRange: TD3DValue;         (* Cutoff range *)
    dvFalloff: TD3DValue;       (* Falloff *)
    dvAttenuation0: TD3DValue;  (* Constant attenuation *)
    dvAttenuation1: TD3DValue;  (* Linear attenuation *)
    dvAttenuation2: TD3DValue;  (* Quadratic attenuation *)
    dvTheta: TD3DValue;         (* Inner angle of spotlight cone *)
    dvPhi: TD3DValue;           (* Outer angle of spotlight cone *)
  end;

(*
 * Structure defining a light source and its properties.
 *)

(* flags bits *)
const
  D3DLIGHT_ACTIVE			= $00000001;
  D3DLIGHT_NO_SPECULAR	                = $00000002;

(* maximum valid light range *)
//...  D3DLIGHT_RANGE_MAX		= sqrt(FLT_MAX);

type
  TD3DLight2 = packed record
    dwSize: DWORD;
    dltType: TD3DLightType;     (* Type of light source *)
    dcvColor: TD3DColorValue;   (* Color of light *)
    dvPosition: TD3DVector;     (* Position in world space *)
    dvDirection: TD3DVector;    (* Direction in world space *)
    dvRange: TD3DValue;         (* Cutoff range *)
    dvFalloff: TD3DValue;       (* Falloff *)
    dvAttenuation0: TD3DValue;  (* Constant attenuation *)
    dvAttenuation1: TD3DValue;  (* Linear attenuation *)
    dvAttenuation2: TD3DValue;  (* Quadratic attenuation *)
    dvTheta: TD3DValue;         (* Inner angle of spotlight cone *)
    dvPhi: TD3DValue;           (* Outer angle of spotlight cone *)
    dwFlags: DWORD;
  end;

  TD3DLightData = packed record
    dwSize: DWORD;
    lpIn: ^TD3DLightingElement;   (* Input positions and normals *)
    dwInSize: DWORD;             (* Stride of input elements *)
    lpOut: ^TD3DTLVertex;         (* Output colors *)
    dwOutSize: DWORD;            (* Stride of output colors *)
  end;

(*
 * Before DX5, these values were in an enum called
 * TD3DColorModel. This was not correct, since they are
 * bit flags. A driver can surface either or both flags
 * in the dcmColorModel member of D3DDEVICEDESC.
 *)

  TD3DColorModel = (
    D3DCOLOR_INVALID_0,
    D3DCOLOR_MONO,
    D3DCOLOR_RGB
 );

(*
 * Options for clearing
 *)

const
  D3DCLEAR_TARGET            = $00000001; (* Clear target surface *)
  D3DCLEAR_ZBUFFER           = $00000002; (* Clear target z buffer *)

(*
 * Execute buffers are allocated via Direct3D.  These buffers may then
 * be filled by the application with instructions to execute along with
 * vertex data.
 *)

(*
 * Supported op codes for execute instructions.
 *)

type
  TD3DOpcode = (
    D3DOP_INVALID_0,
    D3DOP_POINT,
    D3DOP_LINE,
    D3DOP_TRIANGLE,
    D3DOP_MATRIXLOAD,
    D3DOP_MATRIXMULTIPLY,
    D3DOP_STATETRANSFORM,
    D3DOP_STATELIGHT,
    D3DOP_STATERENDER,
    D3DOP_PROCESSVERTICES,
    D3DOP_TEXTURELOAD,
    D3DOP_EXIT,
    D3DOP_BRANCHFORWARD,
    D3DOP_SPAN,
    D3DOP_SETSTATUS);

  TD3DInstruction = packed record
    bOpcode: BYTE;   (* Instruction opcode *)
    bSize: BYTE;     (* Size of each instruction data unit *)
    wCount: WORD;    (* Count of instruction data units to follow *)
  end;

(*
 * Structure for texture loads
 *)

  TD3DTextureLoad = packed record
    hDestTexture: TD3DTextureHandle;
    hSrcTexture: TD3DTextureHandle;
  end;

(*
 * Structure for picking
 *)

  TD3DPickRecord = packed record
    bOpcode: BYTE;
    bPad: BYTE;
    dwOffset: DWORD;
    dvZ: TD3DValue;
  end;

(*
 * The following defines the rendering states which can be set in the
 * execute buffer.
 *)

  TD3DShadeMode = (
    D3DSHADE_INVALID_0,
    D3DSHADE_FLAT,
    D3DSHADE_GOURAUD,
    D3DSHADE_PHONG);

  TD3DFillMode = (
    D3DFILL_INVALID_0,
    D3DFILL_POINT,
    D3DFILL_WIREFRAME,
    D3DFILL_SOLID);

  TD3DLinePattern = packed record
    wRepeatFactor: WORD;
    wLinePattern: WORD;
  end;

  TD3DTextureFilter = (
    D3DFILTER_INVALID_0,
    D3DFILTER_NEAREST,
    D3DFILTER_LINEAR,
    D3DFILTER_MIPNEAREST,
    D3DFILTER_MIPLINEAR,
    D3DFILTER_LINEARMIPNEAREST,
    D3DFILTER_LINEARMIPLINEAR);

  TD3DBlend = (
    D3DBLEND_INVALID_0,
    D3DBLEND_ZERO,
    D3DBLEND_ONE,
    D3DBLEND_SRCCOLOR,
    D3DBLEND_INVSRCCOLOR,
    D3DBLEND_SRCALPHA,
    D3DBLEND_INVSRCALPHA,
    D3DBLEND_DESTALPHA,
    D3DBLEND_INVDESTALPHA,
    D3DBLEND_DESTCOLOR,
    D3DBLEND_INVDESTCOLOR,
    D3DBLEND_SRCALPHASAT,
    D3DBLEND_BOTHSRCALPHA,
    D3DBLEND_BOTHINVSRCALPHA);

  TD3DTextureBlend = (
    D3DTBLEND_INVALID_0,
    D3DTBLEND_DECAL,
    D3DTBLEND_MODULATE,
    D3DTBLEND_DECALALPHA,
    D3DTBLEND_MODULATEALPHA,
    D3DTBLEND_DECALMASK,
    D3DTBLEND_MODULATEMASK,
    D3DTBLEND_COPY,
    D3DTBLEND_ADD);

  TD3DTextureAddress = (
    D3DTADDRESS_INVALID_0,
    D3DTADDRESS_WRAP,
    D3DTADDRESS_MIRROR,
    D3DTADDRESS_CLAMP,
    D3DTADDRESS_BORDER);

  TD3DCull = (
    D3DCULL_INVALID_0,
    D3DCULL_NONE,
    D3DCULL_CW,
    D3DCULL_CCW);

  TD3DCmpFunc = (
    D3DCMP_INVALID_0,
    D3DCMP_NEVER,
    D3DCMP_LESS,
    D3DCMP_EQUAL,
    D3DCMP_LESSEQUAL,
    D3DCMP_GREATER,
    D3DCMP_NOTEQUAL,
    D3DCMP_GREATEREQUAL,
    D3DCMP_ALWAYS);

  TD3DFogMode = (
    D3DFOG_NONE,
    D3DFOG_EXP,
    D3DFOG_EXP2,
    D3DFOG_LINEAR);

  TD3DAntialiasMode = (
    D3DANTIALIAS_NONE,
    D3DANTIALIAS_SORTDEPENDENT,
    D3DANTIALIAS_SORTINDEPENDENT);

// Vertex types supported by Direct3D
  TD3DVertexType = (
    D3DVT_INVALID_0,
    D3DVT_VERTEX,
    D3DVT_LVERTEX,
    D3DVT_TLVERTEX);

// Primitives supported by draw-primitive API
  TD3DPrimitiveType = (
    D3DPT_INVALID_0,
    D3DPT_POINTLIST,
    D3DPT_LINELIST,
    D3DPT_LINESTRIP,
    D3DPT_TRIANGLELIST,
    D3DPT_TRIANGLESTRIP,
    D3DPT_TRIANGLEFAN);

(*
 * Amount to add to a state to generate the override for that state.
 *)

const
  D3DSTATE_OVERRIDE_BIAS          = 256;

(*
 * A state which sets the override flag for the specified state type.
 *)

// #define D3DSTATE_OVERRIDE(type) ((DWORD) (type) + D3DSTATE_OVERRIDE_BIAS)

function D3DSTATE_OVERRIDE(StateType: DWORD) : DWORD;

type
  TD3DTransformStateType = (
    D3DTRANSFORMSTATE_INVALID_0,
    D3DTRANSFORMSTATE_WORLD,
    D3DTRANSFORMSTATE_VIEW,
    D3DTRANSFORMSTATE_PROJECTION);

  TD3DLightStateType = (
    D3DLIGHTSTATE_INVALID_0,
    D3DLIGHTSTATE_MATERIAL,
    D3DLIGHTSTATE_AMBIENT,
    D3DLIGHTSTATE_COLORMODEL,
    D3DLIGHTSTATE_FOGMODE,
    D3DLIGHTSTATE_FOGSTART,
    D3DLIGHTSTATE_FOGEND,
    D3DLIGHTSTATE_FOGDENSITY);

  TD3DRenderStateType = (
    D3DRENDERSTATE_INVALID_0,
    D3DRENDERSTATE_TEXTUREHANDLE = 1,   (* Texture handle *)
    D3DRENDERSTATE_ANTIALIAS,           (* Antialiasing prim edges *)
    D3DRENDERSTATE_TEXTUREADDRESS,      (* D3DTextureAddress      *)
    D3DRENDERSTATE_TEXTUREPERSPECTIVE,  (* TRUE for perspective correction *)
    D3DRENDERSTATE_WRAPU,               (* TRUE for wrapping in u *)
    D3DRENDERSTATE_WRAPV,               (* TRUE for wrapping in v *)
    D3DRENDERSTATE_ZENABLE,             (* TRUE to enable z test *)
    D3DRENDERSTATE_FILLMODE,            (* D3DFILL_MODE            *)
    D3DRENDERSTATE_SHADEMODE,           (* TD3DShadeMode *)
    D3DRENDERSTATE_LINEPATTERN,         (* TD3DLinePattern *)
    D3DRENDERSTATE_MONOENABLE,          (* TRUE to enable mono rasterization *)
    D3DRENDERSTATE_ROP2,                (* ROP2 *)
    D3DRENDERSTATE_PLANEMASK,           (* DWORD physical plane mask *)
    D3DRENDERSTATE_ZWRITEENABLE,        (* TRUE to enable z writes *)
    D3DRENDERSTATE_ALPHATESTENABLE,     (* TRUE to enable alpha tests *)
    D3DRENDERSTATE_LASTPIXEL,           (* TRUE for last-pixel on lines *)
    D3DRENDERSTATE_TEXTUREMAG,          (* TD3DTextureFilter *)
    D3DRENDERSTATE_TEXTUREMIN,          (* TD3DTextureFilter *)
    D3DRENDERSTATE_SRCBLEND,            (* TD3DBlend *)
    D3DRENDERSTATE_DESTBLEND,           (* TD3DBlend *)
    D3DRENDERSTATE_TEXTUREMAPBLEND,     (* TD3DTextureBlend *)
    D3DRENDERSTATE_CULLMODE,            (* TD3DCull *)
    D3DRENDERSTATE_ZFUNC,               (* TD3DCmpFunc *)
    D3DRENDERSTATE_ALPHAREF,            (* TD3DFixed *)
    D3DRENDERSTATE_ALPHAFUNC,           (* TD3DCmpFunc *)
    D3DRENDERSTATE_DITHERENABLE,        (* TRUE to enable dithering *)
    D3DRENDERSTATE_BLENDENABLE,         (* TRUE to enable alpha blending *)
    D3DRENDERSTATE_FOGENABLE,           (* TRUE to enable fog *)
    D3DRENDERSTATE_SPECULARENABLE,      (* TRUE to enable specular *)
    D3DRENDERSTATE_ZVISIBLE,            (* TRUE to enable z checking *)
    D3DRENDERSTATE_SUBPIXEL,            (* TRUE to enable subpixel correction *)
    D3DRENDERSTATE_SUBPIXELX,           (* TRUE to enable correction in X only *)
    D3DRENDERSTATE_STIPPLEDALPHA,       (* TRUE to enable stippled alpha *)
    D3DRENDERSTATE_FOGCOLOR,            (* TD3DColor *)
    D3DRENDERSTATE_FOGTABLEMODE,        (* TD3DFogMode *)
    D3DRENDERSTATE_FOGTABLESTART,       (* Fog table start        *)
    D3DRENDERSTATE_FOGTABLEEND,         (* Fog table end          *)
    D3DRENDERSTATE_FOGTABLEDENSITY,     (* Fog table density      *)
    D3DRENDERSTATE_STIPPLEENABLE,       (* TRUE to enable stippling *)
    D3DRENDERSTATE_EDGEANTIALIAS = 40,  (* TRUE to enable edge antialiasing *)
    D3DRENDERSTATE_COLORKEYENABLE,      (* TRUE to enable source colorkeyed textures *)
    D3DRENDERSTATE_BORDERCOLOR = 43,    (* Border color for texturing w/border *)
    D3DRENDERSTATE_TEXTUREADDRESSU,     (* Texture addressing mode for U coordinate *)
    D3DRENDERSTATE_TEXTUREADDRESSV,     (* Texture addressing mode for V coordinate *)
    D3DRENDERSTATE_MIPMAPLODBIAS,       (* TD3DValue Mipmap LOD bias *)
    D3DRENDERSTATE_ZBIAS,               (* LONG Z bias *)
    D3DRENDERSTATE_RANGEFOGENABLE,      (* Enables range-based fog *)
    D3DRENDERSTATE_ANISOTROPY,          (* Max. anisotropy. 1 = no anisotropy *)
	  D3DRENDERSTATE_FLUSHBATCH,          (* Explicit flush for DP batching (DX5 Only) *)
    D3DRENDERSTATE_INVALID_51,
    D3DRENDERSTATE_INVALID_52,
    D3DRENDERSTATE_INVALID_53,
    D3DRENDERSTATE_INVALID_54,
    D3DRENDERSTATE_INVALID_55,
    D3DRENDERSTATE_INVALID_56,
    D3DRENDERSTATE_INVALID_57,
    D3DRENDERSTATE_INVALID_58,
    D3DRENDERSTATE_INVALID_59,
    D3DRENDERSTATE_INVALID_60,
    D3DRENDERSTATE_INVALID_61,
    D3DRENDERSTATE_INVALID_62,
    D3DRENDERSTATE_INVALID_63,
    D3DRENDERSTATE_STIPPLEPATTERN00,   (* Stipple pattern 01...  *)
    D3DRENDERSTATE_STIPPLEPATTERN01,
    D3DRENDERSTATE_STIPPLEPATTERN02,
    D3DRENDERSTATE_STIPPLEPATTERN03,
    D3DRENDERSTATE_STIPPLEPATTERN04,
    D3DRENDERSTATE_STIPPLEPATTERN05,
    D3DRENDERSTATE_STIPPLEPATTERN06,
    D3DRENDERSTATE_STIPPLEPATTERN07,
    D3DRENDERSTATE_STIPPLEPATTERN08,
    D3DRENDERSTATE_STIPPLEPATTERN09,
    D3DRENDERSTATE_STIPPLEPATTERN10,
    D3DRENDERSTATE_STIPPLEPATTERN11,
    D3DRENDERSTATE_STIPPLEPATTERN12,
    D3DRENDERSTATE_STIPPLEPATTERN13,
    D3DRENDERSTATE_STIPPLEPATTERN14,
    D3DRENDERSTATE_STIPPLEPATTERN15,
    D3DRENDERSTATE_STIPPLEPATTERN16,
    D3DRENDERSTATE_STIPPLEPATTERN17,
    D3DRENDERSTATE_STIPPLEPATTERN18,
    D3DRENDERSTATE_STIPPLEPATTERN19,
    D3DRENDERSTATE_STIPPLEPATTERN20,
    D3DRENDERSTATE_STIPPLEPATTERN21,
    D3DRENDERSTATE_STIPPLEPATTERN22,
    D3DRENDERSTATE_STIPPLEPATTERN23,
    D3DRENDERSTATE_STIPPLEPATTERN24,
    D3DRENDERSTATE_STIPPLEPATTERN25,
    D3DRENDERSTATE_STIPPLEPATTERN26,
    D3DRENDERSTATE_STIPPLEPATTERN27,
    D3DRENDERSTATE_STIPPLEPATTERN28,
    D3DRENDERSTATE_STIPPLEPATTERN29,
    D3DRENDERSTATE_STIPPLEPATTERN30,
    D3DRENDERSTATE_STIPPLEPATTERN31);

// #define D3DRENDERSTATE_STIPPLEPATTERN(y) (D3DRENDERSTATE_STIPPLEPATTERN00 + (y))

  TD3DState = packed record
    case Integer of
    0: (
      dtstTransformStateType: TD3DTransformStateType;
      dwArg: Array [ 0..0 ] of DWORD;
     );
    1: (
      dlstLightStateType: TD3DLightStateType;
      dvArg: Array [ 0..0 ] of TD3DValue;
     );
    2: (
      drstRenderStateType: TD3DRenderStateType;
     );
  end;

(*
 * Operation used to load matrices
 * hDstMat = hSrcMat
 *)

  TD3DMatrixLoad = packed record
    hDestMatrix: TD3DMatrixHandle;   (* Destination matrix *)
    hSrcMatrix: TD3DMatrixHandle;    (* Source matrix *)
  end;

(*
 * Operation used to multiply matrices
 * hDstMat = hSrcMat1 * hSrcMat2
 *)

  TD3DMatrixMultiply = packed record
    hDestMatrix: TD3DMatrixHandle;   (* Destination matrix *)
    hSrcMatrix1: TD3DMatrixHandle;   (* First source matrix *)
    hSrcMatrix2: TD3DMatrixHandle;   (* Second source matrix *)
  end;

(*
 * Operation used to transform and light vertices.
 *)

  TD3DProcessVertices = packed record
    dwFlags: DWORD;           (* Do we transform or light or just copy? *)
    wStart: WORD;             (* Index to first vertex in source *)
    wDest: WORD;              (* Index to first vertex in local buffer *)
    dwCount: DWORD;           (* Number of vertices to be processed *)
    dwReserved: DWORD;        (* Must be zero *)
  end;

const
  D3DPROCESSVERTICES_TRANSFORMLIGHT       = $00000000;
  D3DPROCESSVERTICES_TRANSFORM            = $00000001;
  D3DPROCESSVERTICES_COPY                 = $00000002;
  D3DPROCESSVERTICES_OPMASK               = $00000007;

  D3DPROCESSVERTICES_UPDATEEXTENTS        = $00000008;
  D3DPROCESSVERTICES_NOCOLOR              = $00000010;

(*
 * Triangle flags
 *)

(*
 * Tri strip and fan flags.
 * START loads all three vertices
 * EVEN and ODD load just v3 with even or odd culling
 * START_FLAT contains a count from 0 to 29 that allows the
 * whole strip or fan to be culled in one hit.
 * e.g. for a quad len = 1
 *)

  D3DTRIFLAG_START                        = $00000000;
// #define D3DTRIFLAG_STARTFLAT(len) (len)         (* 0 < len < 30 *)
function D3DTRIFLAG_STARTFLAT(len: DWORD) : DWORD;

const
  D3DTRIFLAG_ODD                          = $0000001e;
  D3DTRIFLAG_EVEN                         = $0000001f;

(*
 * Triangle edge flags
 * enable edges for wireframe or antialiasing
 *)

  D3DTRIFLAG_EDGEENABLE1                  = $00000100; (* v0-v1 edge *)
  D3DTRIFLAG_EDGEENABLE2                  = $00000200; (* v1-v2 edge *)
  D3DTRIFLAG_EDGEENABLE3                  = $00000400; (* v2-v0 edge *)
  D3DTRIFLAG_EDGEENABLETRIANGLE = (
      D3DTRIFLAG_EDGEENABLE1 or D3DTRIFLAG_EDGEENABLE2 or D3DTRIFLAG_EDGEENABLE3);

(*
 * Primitive structures and related defines.  Vertex offsets are to types
 * TD3DVertex, TD3DLVertex, or TD3DTLVertex.
 *)

(*
 * Triangle list primitive structure
 *)

type
  TD3DTriangle = packed record
    case Integer of
    0: (
      v1: WORD;            (* Vertex indices *)
      v2: WORD;
      v3: WORD;
      wFlags: WORD;        (* Edge (and other) flags *)
     );
    1: (
      wV1: WORD;
      wV2: WORD;
      wV3: WORD;
     );
  end;

(*
 * Line strip structure.
 * The instruction count - 1 defines the number of line segments.
 *)

  TD3DLine = packed record
    case Integer of
    0: (
      v1: WORD;            (* Vertex indices *)
      v2: WORD;
     );
    1: (
      wV1: WORD;
      wV2: WORD;
     );
  end;

(*
 * Span structure
 * Spans join a list of points with the same y value.
 * If the y value changes, a new span is started.
 *)

  TD3DSpan = packed record
    wCount: WORD;        (* Number of spans *)
    wFirst: WORD;        (* Index to first vertex *)
  end;

(*
 * Point structure
 *)

  TD3DPoint = packed record
    wCount: WORD;        (* number of points         *)
    wFirst: WORD;        (* index to first vertex    *)
  end;

(*
 * Forward branch structure.
 * Mask is logically anded with the driver status mask
 * if the result equals 'value', the branch is taken.
 *)

  TD3DBranch = packed record
    dwMask: DWORD;         (* Bitmask against D3D status *)
    dwValue: DWORD;
    bNegate: BOOL;         (* TRUE to negate comparison *)
    dwOffset: DWORD;       (* How far to branch forward (0 for exit)*)
  end;

(*
 * Status used for set status instruction.
 * The D3D status is initialised on device creation
 * and is modified by all execute calls.
 *)

  TD3DStatus = packed record
    dwFlags: DWORD;        (* Do we set extents or status *)
    dwStatus: DWORD;       (* D3D status *)
    drExtent: TD3DRect;
  end;

const
  D3DSETSTATUS_STATUS   = $00000001;
  D3DSETSTATUS_EXTENTS  = $00000002;
  D3DSETSTATUS_ALL      = (D3DSETSTATUS_STATUS or D3DSETSTATUS_EXTENTS);

type
  TD3DClipStatus = packed record
    dwFlags : DWORD; (* Do we set 2d extents, 3D extents or status *)
    dwStatus : DWORD; (* Clip status *)
    minx, maxx : single; (* X extents *)
    miny, maxy : single; (* Y extents *)
    minz, maxz : single; (* Z extents *)
  end;

const
  D3DCLIPSTATUS_STATUS        = $00000001;
  D3DCLIPSTATUS_EXTENTS2      = $00000002;
  D3DCLIPSTATUS_EXTENTS3      = $00000004;

(*
 * Statistics structure
 *)

type
  D3DStats = packed record
    dwSize: DWORD;
    dwTrianglesDrawn: DWORD;
    dwLinesDrawn: DWORD;
    dwPointsDrawn: DWORD;
    dwSpansDrawn: DWORD;
    dwVerticesProcessed: DWORD;
  end;

(*
 * Execute options.
 * When calling using D3DEXECUTE_UNCLIPPED all the primitives
 * inside the buffer must be contained within the viewport.
 *)

const
  D3DEXECUTE_CLIPPED       = $00000001;
  D3DEXECUTE_UNCLIPPED     = $00000002;

type
  TD3DExecuteData = packed record
    dwSize: DWORD;
    dwVertexOffset: DWORD;
    dwVertexCount: DWORD;
    dwInstructionOffset: DWORD;
    dwInstructionLength: DWORD;
    dwHVertexOffset: DWORD;
    dsStatus: TD3DStatus;       (* Status after execute *)
  end;

(*
 * Palette flags.
 * This are or'ed with the peFlags in the PALETTEENTRYs passed to DirectDraw.
 *)

const
  D3DPAL_FREE     = $00;    (* Renderer may use this entry freely *)
  D3DPAL_READONLY = $40;    (* Renderer may not set this entry *)
  D3DPAL_RESERVED = $80;    (* Renderer may not use this entry *)




implementation

function D3DVAL(val: variant) : float;
begin
  Result := val;
end;

function D3DDivide(a,b: variant) : variant;
begin
  Result := a / b;
end;

function D3DMultiply(a,b: variant) : variant;
begin
  Result := a * b;
end;

// #define CI_GETALPHA(ci)    ((ci) >> 24)
function CI_GETALPHA(ci: DWORD) : DWORD;
begin
  Result := ci shr 24;
end;

// #define CI_GETINDEX(ci)    (((ci) >> 8) & 0xffff)
function CI_GETINDEX(ci: DWORD) : DWORD;
begin
  Result := (ci shr 8) and $ffff;
end;

// #define CI_GETFRACTION(ci) ((ci) & 0xff)
function CI_GETFRACTION(ci: DWORD) : DWORD;
begin
  Result := ci and $ff;
end;

// #define CI_ROUNDINDEX(ci)  CI_GETINDEX((ci) + 0x80)
function CI_ROUNDINDEX(ci: DWORD) : DWORD;
begin
  Result := CI_GETINDEX(ci + $80);
end;

// #define CI_MASKALPHA(ci)   ((ci) & 0xffffff)
function CI_MASKALPHA(ci: DWORD) : DWORD;
begin
  Result := ci and $ffffff;
end;

// #define CI_MAKE(a, i, f)    (((a) << 24) | ((i) << 8) | (f))
function CI_MAKE(a,i,f: DWORD) : DWORD;
begin
  Result := (a shl 24) or (i shl 8) or f;
end;

// #define RGBA_GETALPHA(rgb)      ((rgb) >> 24)
function RGBA_GETALPHA(rgb: TD3DColor) : DWORD;
begin
  Result := rgb shr 24;
end;

// #define RGBA_GETRED(rgb)        (((rgb) >> 16) & 0xff)
function RGBA_GETRED(rgb: TD3DColor) : DWORD;
begin
  Result := (rgb shr 16) and $ff;
end;

// #define RGBA_GETGREEN(rgb)      (((rgb) >> 8) & 0xff)
function RGBA_GETGREEN(rgb: TD3DColor) : DWORD;
begin
  Result := (rgb shr 8) and $ff;
end;

// #define RGBA_GETBLUE(rgb)       ((rgb) & 0xff)
function RGBA_GETBLUE(rgb: TD3DColor) : DWORD;
begin
  Result := rgb and $ff;
end;

// #define RGBA_MAKE(r, g, b, a)   ((TD3DColor) (((a) << 24) | ((r) << 16) | ((g) << 8) | (b)))
function RGBA_MAKE(r, g, b, a: DWORD) : TD3DColor;
begin
  Result := (a shl 24) or (r shl 16) or (g shl 8) or b;
end;

// #define D3DRGB(r, g, b) \
//     (0xff000000L | (((long)((r) * 255)) << 16) | (((long)((g) * 255)) << 8) | (long)((b) * 255))
function D3DRGB(r, g, b: float) : TD3DColor;
begin
  Result := $ff000000 or (round(r * 255) shl 16)
                      or (round(g * 255) shl 8)
                      or round(b * 255);
end;

// #define D3DRGBA(r, g, b, a) \
//     (  (((long)((a) * 255)) << 24) | (((long)((r) * 255)) << 16) \
//     |   (((long)((g) * 255)) << 8) | (long)((b) * 255) \
//    )
function D3DRGBA(r, g, b, a: float) : TD3DColor;
begin
  Result := (round(a * 255) shl 24) or (round(r * 255) shl 16)
                                    or (round(g * 255) shl 8)
                                    or round(b * 255);
end;

// #define RGB_GETRED(rgb)         (((rgb) >> 16) & 0xff)
function RGB_GETRED(rgb: TD3DColor) : DWORD;
begin
  Result := (rgb shr 16) and $ff;
end;

// #define RGB_GETGREEN(rgb)       (((rgb) >> 8) & 0xff)
function RGB_GETGREEN(rgb: TD3DColor) : DWORD;
begin
  Result := (rgb shr 8) and $ff;
end;

// #define RGB_GETBLUE(rgb)        ((rgb) & 0xff)
function RGB_GETBLUE(rgb: TD3DColor) : DWORD;
begin
  Result := rgb and $ff;
end;

// #define RGBA_SETALPHA(rgba, x) (((x) << 24) | ((rgba) & 0x00ffffff))
function RGBA_SETALPHA(rgba: TD3DColor; x: DWORD) : TD3DColor;
begin
  Result := (x shl 24) or (rgba and $00ffffff);
end;

// #define RGB_MAKE(r, g, b)       ((TD3DColor) (((r) << 16) | ((g) << 8) | (b)))
function RGB_MAKE(r, g, b: DWORD) : TD3DColor;
begin
  Result := (r shl 16) or (g shl 8) or b;
end;

// #define RGBA_TORGB(rgba)       ((TD3DColor) ((rgba) & 0xffffff))
function RGBA_TORGB(rgba: TD3DColor) : TD3DColor;
begin
  Result := rgba and $00ffffff;
end;

// #define RGB_TORGBA(rgb)        ((TD3DColor) ((rgb) | 0xff000000))
function RGB_TORGBA(rgb: TD3DColor) : TD3DColor;
begin
  Result := rgb or $ff000000;
end;


function D3DSTATE_OVERRIDE(StateType: DWORD) : DWORD;
begin
  Result := StateType + D3DSTATE_OVERRIDE_BIAS;
end;

function D3DTRIFLAG_STARTFLAT(len: DWORD) : DWORD;
begin
  if not (len in [1..29]) then len := 0;
  result := len;
end;

    // Addition and subtraction
function VectorAdd(const v1, v2: TD3DVector) : TD3DVector;
begin
  result.x := v1.x+v2.x;
  result.y := v1.y+v2.y;
  result.z := v1.z+v2.z;
end;

function VectorSub(const v1, v2: TD3DVector) : TD3DVector;
begin
  result.x := v1.x-v2.x;
  result.y := v1.y-v2.y;
  result.z := v1.z-v2.z;
end;

    // Scalar multiplication and division
function VectorMulS(const v: TD3DVector; s: TD3DValue) : TD3DVector;
begin
  result.x := v.x*s;
  result.y := v.y*s;
  result.z := v.z*s;
end;

function VectorDivS(const v: TD3DVector; s: TD3DValue) : TD3DVector;
begin
  result.x := v.x/s;
  result.y := v.y/s;
  result.z := v.z/s;
end;

    // Memberwise multiplication and division
function VectorMul(const v1, v2: TD3DVector) : TD3DVector;
begin
  result.x := v1.x*v2.x;
  result.y := v1.y*v2.y;
  result.z := v1.z*v2.z;
end;

function VectorDiv(const v1, v2: TD3DVector) : TD3DVector;
begin
  result.x := v1.x/v2.x;
  result.y := v1.y/v2.y;
  result.z := v1.z/v2.z;
end;

    // Vector dominance
function VectorSmaller(v1, v2: TD3DVector) : boolean;
begin
  result := (v1.x < v2.x) and (v1.y < v2.y) and (v1.z < v2.z);
end;

function VectorSmallerEquel(v1, v2: TD3DVector) : boolean;
begin
  result := (v1.x <= v2.x) and (v1.y <= v2.y) and (v1.z <= v2.z);
end;

    // Bitwise equality
function VectorEquel(v1, v2: TD3DVector) : boolean;
begin
  result := (v1.x = v2.x) and (v1.y = v2.y) and (v1.z = v2.z);
end;

    // Length-related functions
function VectorSquareMagnitude(v: TD3DVector) : TD3DValue;
begin
  result := (v.x*v.x) + (v.y*v.y) + (v.z*v.z);
end;

function VectorMagnitude(v: TD3DVector) : TD3DValue;
begin
  result := sqrt((v.x*v.x) + (v.y*v.y) + (v.z*v.z));
end;

    // Returns vector with same direction and unit length
function VectorNormalize(const v: TD3DVector) : TD3DVector;
begin
  result := VectorDivS(v,VectorMagnitude(v));
end;

    // Return min/max component of the input vector
function VectorMin(v: TD3DVector) : TD3DValue;
var
  ret : TD3DValue;
begin
  ret := v.x;
  if (v.y < ret) then ret := v.y;
  if (v.z < ret) then ret := v.z;
  result := ret;
end;

function VectorMax(v: TD3DVector) : TD3DValue;
var
  ret : TD3DValue;
begin
  ret := v.x;
  if (ret < v.y) then ret := v.y;
  if (ret < v.z) then ret := v.z;
  result := ret;
end;

    // Return memberwise min/max of input vectors
function VectorMinimize(const v1, v2: TD3DVector) : TD3DVector;
begin
  if v1.x < v2.x then result.x := v1.x else result.x := v2.x;
  if v1.y < v2.y then result.y := v1.y else result.y := v2.y;
  if v1.z < v2.z then result.z := v1.z else result.z := v2.z;
end;

function VectorMaximize(const v1, v2: TD3DVector) : TD3DVector;
begin
  if v1.x > v2.x then result.x := v1.x else result.x := v2.x;
  if v1.y > v2.y then result.y := v1.y else result.y := v2.y;
  if v1.z > v2.z then result.z := v1.z else result.z := v2.z;
end;

    // Dot and cross product
function VectorDotProduct(v1, v2: TD3DVector) : TD3DValue;
begin
  result := (v1.x*v2.x) + (v1.y * v2.y) + (v1.z*v2.z);
end;

function VectorCrossProduct(const v1, v2: TD3DVector) : TD3DVector;
begin
  result.x := (v1.y*v2.z) - (v1.z*v2.y);
  result.y := (v1.z*v2.x) - (v1.x*v2.z);
  result.z := (v1.x*v2.y) - (v1.y*v2.x);
end;

end.

