(*==========================================================================;
 *
 *  Copyright (C) 1995-1997 Microsoft Corporation.  All Rights Reserved.
 *
 *  File:       dsound.h
 *  Content:    DirectSound include file
 *
 *  DirectX 5 Delphi adaptation by Erik Unger
 *  Generic source created with PERL Header Converter 0.74 from David Sisson
 *
 *  Modyfied: 21.3.98
 *
 *  Download: http://www.sbox.tu-graz.ac.at/home/ungerik/DelphiGraphics/
 *  E-Mail: h_unger@magnet.at
 *
 ***************************************************************************)

unit DSound;

{$INCLUDE COMSWITCH.INC}
{$INCLUDE STRINGSWITCH.INC}

interface

uses
{$IFDEF D2COM}
  OLE2,
{$ENDIF}
  Windows,
  SysUtils,
  MMSystem,
  D3Dtypes;


const
  _FACDS = $878;
function MAKE_DSHResult(code: variant) : HResult;

const
// Direct Sound Component GUID {47D4D946-62E8-11cf-93BC-444553540000}
  CLSID_DirectSound: TGUID =
      (D1:$47d4d946;D2:$62e8;D3:$11cf;D4:($93,$bc,$44,$45,$53,$54,$00,$0));

// DirectSound Capture Component GUID {B0210780-89CD-11d0-AF08-00A0C925CD16}
  CLSID_DirectSoundCapture: TGUID =
      (D1:$b0210780;D2:$89cd;D3:$11d0;D4:($af,$8,$00,$a0,$c9,$25,$cd,$16));

//
// GUID's for all the objects
//
const
  IID_IDirectSound: TGUID =
      (D1:$279AFA83;D2:$4981;D3:$11CE;D4:($A5,$21,$00,$20,$AF,$0B,$E5,$60));
  IID_IDirectSoundBuffer: TGUID =
      (D1:$279AFA85;D2:$4981;D3:$11CE;D4:($A5,$21,$00,$20,$AF,$0B,$E5,$60));
  IID_IDirectSound3DListener: TGUID =
      (D1:$279AFA84;D2:$4981;D3:$11CE;D4:($A5,$21,$00,$20,$AF,$0B,$E5,$60));
  IID_IDirectSound3DBuffer: TGUID =
      (D1:$279AFA86;D2:$4981;D3:$11CE;D4:($A5,$21,$00,$20,$AF,$0B,$E5,$60));
  IID_IDirectSoundCapture: TGUID =
      (D1:$b0210781;D2:$89cd;D3:$11d0;D4:($af,$08,$00,$a0,$c9,$25,$cd,$16));
  IID_IDirectSoundCaptureBuffer: TGUID =
      (D1:$b0210782;D2:$89cd;D3:$11d0;D4:($af,$08,$00,$a0,$c9,$25,$cd,$16));
  IID_IDirectSoundNotify: TGUID =
      (D1:$b0210783;D2:$89cd;D3:$11d0;D4:($af,$08,$00,$a0,$c9,$25,$cd,$16));
  IID_IKsPropertySet: TGUID =
      (D1:$31efac30;D2:$515c;D3:$11d0;D4:($a9,$aa,$00,$aa,$00,$61,$be,$93));



//
// Structures
//
type
{$IFDEF D2COM}
  IDirectSound = class;
  IDirectSoundBuffer = class;
  IDirectSound3DListener = class;
  IDirectSound3DBuffer = class;
  IDirectSoundCapture = class;
  IDirectSoundCaptureBuffer = class;
  IDirectSoundNotify = class;
  IKsPropertySet = class;
{$ELSE}
  IDirectSound = interface;
  IDirectSoundBuffer = interface;
  IDirectSound3DListener = interface;
  IDirectSound3DBuffer = interface;
  IDirectSoundCapture = interface;
  IDirectSoundCaptureBuffer = interface;
  IDirectSoundNotify = interface;
  IKsPropertySet = interface;
{$ENDIF}


  PDSCaps = ^TDSCaps;
  TDSCaps = packed record
    dwSize: DWORD;
    dwFlags: DWORD;
    dwMinSecondarySampleRate: DWORD;
    dwMaxSecondarySampleRate: DWORD;
    dwPrimaryBuffers: DWORD;
    dwMaxHwMixingAllBuffers: DWORD;
    dwMaxHwMixingStaticBuffers: DWORD;
    dwMaxHwMixingStreamingBuffers: DWORD;
    dwFreeHwMixingAllBuffers: DWORD;
    dwFreeHwMixingStaticBuffers: DWORD;
    dwFreeHwMixingStreamingBuffers: DWORD;
    dwMaxHw3DAllBuffers: DWORD;
    dwMaxHw3DStaticBuffers: DWORD;
    dwMaxHw3DStreamingBuffers: DWORD;
    dwFreeHw3DAllBuffers: DWORD;
    dwFreeHw3DStaticBuffers: DWORD;
    dwFreeHw3DStreamingBuffers: DWORD;
    dwTotalHwMemBytes: DWORD;
    dwFreeHwMemBytes: DWORD;
    dwMaxContigFreeHwMemBytes: DWORD;
    dwUnlockTransferRateHwBuffers: DWORD;
    dwPlayCpuOverheadSwBuffers: DWORD;
    dwReserved1: DWORD;
    dwReserved2: DWORD;
  end;
  PCDSCaps = ^TDSCaps;

  PDSBCaps = ^TDSBCaps;
  TDSBCaps = packed record
    dwSize: DWORD;
    dwFlags: DWORD;
    dwBufferBytes: DWORD;
    dwUnlockTransferRate: DWORD;
    dwPlayCpuOverhead: DWORD;
  end;
  PCDSBCaps = ^TDSBCaps;

  PDSBufferDesc = ^TDSBufferDesc;
  TDSBufferDesc = packed record
    dwSize: DWORD;
    dwFlags: DWORD;
    dwBufferBytes: DWORD;
    dwReserved: DWORD;
    lpwfxFormat: PWaveFormatEx;
  end;
  PCDSBufferDesc = ^TDSBufferDesc;

  PDS3DBuffer = ^TDS3DBuffer;
  TDS3DBuffer = packed record
    dwSize: DWORD;
    vPosition: TD3DVector;
    vVelocity: TD3DVector;
    dwInsideConeAngle: DWORD;
    dwOutsideConeAngle: DWORD;
    vConeOrientation: TD3DVector;
    lConeOutsideVolume: Longint;
    flMinDistance: TD3DValue;
    flMaxDistance: TD3DValue;
    dwMode: DWORD;
  end;
  TCDS3DBuffer = ^TDS3DBuffer;

  PDS3DListener = ^TDS3DListener;
  TDS3DListener = packed record
    dwSize: DWORD;
    vPosition: TD3DVector;
    vVelocity: TD3DVector;
    vOrientFront: TD3DVector;
    vOrientTop: TD3DVector;
    flDistanceFactor: TD3DValue;
    flRolloffFactor: TD3DValue;
    flDopplerFactor: TD3DValue;
  end;
  PCDS3DListener = ^TDS3DListener;

  PDSCCaps = ^TDSCCaps;
  TDSCCaps = packed record
    dwSize: DWORD;
    dwFlags: DWORD;
    dwFormats: DWORD;
    dwChannels: DWORD;
  end;
  PCDSCCaps = ^TDSCCaps;

  PDSCBufferDesc = ^TDSCBufferDesc;
  TDSCBufferDesc = packed record
    dwSize: DWORD;
    dwFlags: DWORD;
    dwBufferBytes: DWORD;
    dwReserved: DWORD;
    lpwfxFormat: PWaveFormatEx;
  end;
  PCDSCBufferDesc = ^TDSCBufferDesc;

  PDSCBCaps = ^TDSCBCaps;
  TDSCBCaps = packed record
    dwSize: DWORD;
    dwFlags: DWORD;
    dwBufferBytes: DWORD;
    dwReserved: DWORD;
  end;
  PCDSCBCaps = ^TDSCBCaps;

  PDSBPositionNotify = ^TDSBPositionNotify;
  TDSBPositionNotify = packed record
    dwOffset: DWORD;
    hEventNotify: THandle;
  end;
  PCDSBPositionNotify = ^TDSBPositionNotify;

//
// DirectSound API
//
  TDSEnumCallbackW = function (lpGuid: PGUID; lpstrDescription: PWideChar;
      lpstrModule: PWideChar; lpContext: Pointer) : BOOL; stdcall;
  TDSEnumCallbackA = function (lpGuid: PGUID; lpstrDescription: PAnsiChar;
      lpstrModule: PAnsiChar; lpContext: Pointer) : BOOL; stdcall;


{$IFDEF UNICODE}
  TDSEnumCallback = TDSEnumCallbackW;
{$ELSE}
  TDSEnumCallback = TDSEnumCallbackA;
{$ENDIF}

//
// IDirectSound
//
{$IFDEF D2COM}
  IDirectSound = class (IUnknown)
{$ELSE}
  IDirectSound = interface (IUnknown)
    ['{279AFA83-4981-11CE-A521-0020AF0BE560}']
{$ENDIF}
    // IDirectSound methods
    function CreateSoundBuffer(const lpDSBufferDesc: TDSBufferDesc;
        var lpIDirectSoundBuffer: IDirectSoundBuffer;
        pUnkOuter: IUnknown) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetCaps(var Arg1: TDSCaps) : HResult; 
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function DuplicateSoundBuffer(lpDsbOriginal: IDirectSoundBuffer;
        var lpDsbDuplicate: IDirectSoundBuffer) : HResult;  
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetCooperativeLevel(Arg1: HWND; Arg2: DWORD) : HResult; 
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function Compact: HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetSpeakerConfig(var Arg1: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetSpeakerConfig(Arg1: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function Initialize(Arg1: PGUID) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
  end;


//
// IDirectSoundBuffer
//
{$IFDEF D2COM}
  IDirectSoundBuffer = class (IUnknown)
{$ELSE}
  IDirectSoundBuffer = interface (IUnknown)
    ['{279AFA85-4981-11CE-A521-0020AF0BE560}']
{$ENDIF}
    // IDirectSoundBuffer methods
    function GetCaps(var Arg1: TDSBCaps) : HResult; 
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetCurrentPosition(var Arg1: DWORD; var Arg2: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetFormat(Arg1: PWaveFormatEx; Arg2: DWORD; var Arg3: DWORD) :
        HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetVolume(var Arg1: Longint) : HResult;  
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetPan(var Arg1: Longint) : HResult;  
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetFrequency(var Arg1: DWORD) : HResult;  
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetStatus(var Arg1: DWORD) : HResult;  
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function Initialize(Arg1: IDirectSound; const Arg2: TDSBufferDesc) :
        HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function Lock(Arg1: DWORD; Arg2: DWORD; var Arg3: Pointer;
        var Arg4: DWORD; var Arg5: Pointer; var Arg6: DWORD; Arg7: DWORD) :
        HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function Play(Arg1: DWORD; Arg2: DWORD; Arg3: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetCurrentPosition(Arg1: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetFormat(const Arg1: TWaveFormatEx) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetVolume(Arg1: Longint) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetPan(Arg1: Longint) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetFrequency(Arg1: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function Stop: HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function Unlock(Arg1: Pointer; Arg2: DWORD; Arg3: Pointer; Arg4: DWORD) :
        HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function Restore: HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
  end;


//
// IDirectSound3DListener
//
{$IFDEF D2COM}
  IDirectSound3DListener = class (IUnknown)
{$ELSE}
  IDirectSound3DListener = interface (IUnknown)
    ['{279AFA84-4981-11CE-A521-0020AF0BE560}']
{$ENDIF}
    // IDirectSound3D methods
    function GetAllParameters(var Arg1: TDS3DListener) : HResult; 
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetDistanceFactor(var Arg1: TD3DValue) : HResult; 
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetDopplerFactor(var Arg1: TD3DValue) : HResult; 
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetOrientation(var Arg1: TD3DVector; var Arg2: TD3DVector) :
        HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetPosition(var Arg1: TD3DVector) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetRolloffFactor(var Arg1: TD3DValue) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetVelocity(var Arg1: TD3DVector) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetAllParameters(const Arg1: TDS3DListener; Arg2: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetDistanceFactor(Arg1: TD3DValue; Arg2: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetDopplerFactor(Arg1: TD3DValue; Arg2: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetOrientation(Arg1: TD3DValue; Arg2: TD3DValue; Arg3: TD3DValue;
        Arg4: TD3DValue; Arg5: TD3DValue; Arg6: TD3DValue; Arg7: DWORD) :
        HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetPosition(Arg1: TD3DValue; Arg2: TD3DValue; Arg3: TD3DValue;
        Arg4: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetRolloffFactor(Arg1: TD3DValue; Arg2: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetVelocity(Arg1: TD3DValue; Arg2: TD3DValue; Arg3: TD3DValue;
        Arg4: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function CommitDeferredSettings: HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
  end;


//
// IDirectSound3DBuffer
//
{$IFDEF D2COM}
  IDirectSound3DBuffer = class (IUnknown)
{$ELSE}
  IDirectSound3DBuffer = interface (IUnknown)
    ['{279AFA86-4981-11CE-A521-0020AF0BE560}']
{$ENDIF}
    // IDirectSoundBuffer3D methods
    function GetAllParameters(var Arg1: TDS3DBuffer) : HResult;  
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetConeAngles(var Arg1: DWORD; var Arg2: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetConeOrientation(var Arg1: TD3DVector) : HResult;  
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetConeOutsideVolume(var Arg1: Longint) : HResult;  
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetMaxDistance(var Arg1: TD3DValue) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetMinDistance(var Arg1: TD3DValue) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetMode(var Arg1: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetPosition(var Arg1: TD3DVector) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetVelocity(var Arg1: TD3DVector) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetAllParameters(const Arg1: TDS3DBuffer; Arg2: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetConeAngles(Arg1: DWORD; Arg2: DWORD; Arg3: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetConeOrientation(Arg1: TD3DValue; Arg2: TD3DValue;
        Arg3: TD3DValue; Arg4: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetConeOutsideVolume(Arg1: Longint; Arg2: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetMaxDistance(Arg1: TD3DValue; Arg2: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetMinDistance(Arg1: TD3DValue; Arg2: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetMode(Arg1: DWORD; Arg2: DWORD) : HResult; 
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetPosition(Arg1: TD3DValue; Arg2: TD3DValue; Arg3: TD3DValue;
        Arg4: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetVelocity(Arg1: TD3DValue; Arg2: TD3DValue; Arg3: TD3DValue;
        Arg4: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
  end;


//
// IDirectSoundCapture
//
{$IFDEF D2COM}
  IDirectSoundCapture = class (IUnknown)
{$ELSE}
  IDirectSoundCapture = interface (IUnknown)
    ['{b0210781-89cd-11d0-af08-00a0c925cd16}']
{$ENDIF}
    // IDirectSoundCapture methods
    function CreateCaptureBuffer(const Arg1: TDSCBufferDesc;
        var Arg2: IDirectSoundCAPTUREBUFFER; Arg3: IUnknown) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetCaps(var Arg1: TDSCCaps) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function Initialize(Arg1: PGUID) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
  end;


//
// IDirectSoundCaptureBuffer
//
{$IFDEF D2COM}
  IDirectSoundCaptureBuffer = class (IUnknown)
{$ELSE}
  IDirectSoundCaptureBuffer = interface (IUnknown)
    ['{b0210782-89cd-11d0-af08-00a0c925cd16}']
{$ENDIF}
    // IDirectSoundCaptureBuffer methods
    function GetCaps(var Arg1: TDSCBCaps) : HResult; 
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetCurrentPosition(var Arg1: DWORD; var Arg2: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetFormat(Arg1: PWaveFormatEx; Arg2: DWORD; var Arg3: DWORD) :
        HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetStatus(var Arg1: DWORD) : HResult;  
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function Initialize(Arg1: IDirectSoundCapture; 
        const Arg2: TDSCBufferDesc) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function Lock(Arg1: DWORD; Arg2: DWORD; var Arg3: Pointer; 
        var Arg4: DWORD; var Arg5: Pointer; var Arg6: DWORD; Arg7: DWORD) : 
        HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function Start(Arg1: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function Stop: HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function Unlock(Arg1: Pointer; Arg2: DWORD; Arg3: Pointer; Arg4: DWORD) :
        HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
  end;


//
// IDirectSoundNotify
//
{$IFDEF D2COM}
  IDirectSoundNotify = class (IUnknown)
{$ELSE}
  IDirectSoundNotify = interface (IUnknown)
    ['{b0210783-89cd-11d0-af08-00a0c925cd16}']
{$ENDIF}
    // IDirectSoundNotify methods
    function SetNotificationPositions(Arg1: DWORD;
        const Arg2: TDSBPositionNotify) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
  end;

//
// IKsPropertySet
//
{$IFDEF D2COM}
  IKsPropertySet = class (IUnknown)
{$ELSE}
  IKsPropertySet = interface (IUnknown)
    ['{31efac30-515c-11d0-a9aa-00aa0061be93}']
{$ENDIF}
    // IKsPropertySet methods
    function Get(Arg1: PGUID; Arg2: DWORD; Arg3: Pointer; Arg4: DWORD;
        Arg5: Pointer; Arg6: DWORD; Arg7: PULONG) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    // Warning: The following function is defined as Set() in DirectX
    //          which is a reserved word in Delphi!
    function SetProperty(Arg1: PGUID; Arg2: DWORD; Arg3: Pointer; Arg4: DWORD;
        Arg5: Pointer; Arg6: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function QuerySupport(Arg1: PGUID; Arg2: DWORD; Arg3: PULONG) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
  end;


const
  KSPROPERTY_SUPPORT_GET = $00000001;
  KSPROPERTY_SUPPORT_SET = $00000002;
  
//
// Creation Routines
//
function DirectSoundCreate(Arg1: PGUID; var Arg2: IDirectSound;
    Arg3: IUnknown) : HResult; stdcall;
function DirectSoundEnumerateW(Arg1: TDSEnumCallbackW; Arg2: Pointer) :
    HResult; stdcall;
function DirectSoundEnumerateA(Arg1: TDSEnumCallbackA; Arg2: Pointer) :
    HResult; stdcall;
function DirectSoundEnumerate(Arg1: TDSEnumCallbackA; Arg2: Pointer) :
    HResult; stdcall;

function DirectSoundCaptureCreate(Arg1: PGUID;
    var Arg2: IDirectSoundCapture; Arg3: IUnknown) : HResult; stdcall;
function DirectSoundCaptureEnumerateW(Arg1: TDSEnumCallbackW;
    Arg2: Pointer) : HResult; stdcall;
function DirectSoundCaptureEnumerateA(Arg1: TDSEnumCallbackA;
    Arg2: Pointer) : HResult; stdcall;
function DirectSoundCaptureEnumerate(Arg1: TDSEnumCallbackA;
    Arg2: Pointer) : HResult; stdcall;


//
// Return Codes
//

const
  DS_OK = 0;

// The call failed because resources (such as a priority level)
// were already being used by another caller.
  DSERR_ALLOCATED = integer($88780000 + 10);

// The control (vol,pan,etc.) requested by the caller is not available.
  DSERR_CONTROLUNAVAIL = integer($88780000 + 30);

// An invalid parameter was passed to the returning function
  DSERR_INVALIDPARAM = E_INVALIDARG;

// This call is not valid for the current state of this object
  DSERR_INVALIDCALL = integer($88780000 + 50);

// An undetermined error occured inside the DirectSound subsystem
  DSERR_GENERIC = E_FAIL;

// The caller does not have the priority level required for the function to
// succeed.
  DSERR_PRIOLEVELNEEDED = integer($88780000 + 70);

// Not enough free memory is available to complete the operation
  DSERR_OUTOFMEMORY = E_OUTOFMEMORY;

// The specified WAVE format is not supported
  DSERR_BADFORMAT = integer($88780000 + 100);

// The function called is not supported at this time
  DSERR_UNSUPPORTED = E_NOTIMPL;

// No sound driver is available for use
  DSERR_NODRIVER = integer($88780000 + 120);

// This object is already initialized
  DSERR_ALREADYINITIALIZED = integer($88780000 + 130);

// This object does not support aggregation
  DSERR_NOAGGREGATION = CLASS_E_NOAGGREGATION;

// The buffer memory has been lost, and must be restored.
  DSERR_BUFFERLOST = integer($88780000 + 150);

// Another app has a higher priority level, preventing this call from
// succeeding.
  DSERR_OTHERAPPHASPRIO = integer($88780000 + 160);

// This object has not been initialized
  DSERR_UNINITIALIZED = integer($88780000 + 170);

// The requested COM interface is not available
  DSERR_NOINTERFACE = E_NOINTERFACE;

//
// Flags
//

  DSCAPS_PRIMARYMONO = $00000001;
  DSCAPS_PRIMARYSTEREO = $00000002;
  DSCAPS_PRIMARY8BIT = $00000004;
  DSCAPS_PRIMARY16BIT = $00000008;
  DSCAPS_CONTINUOUSRATE = $00000010;
  DSCAPS_EMULDRIVER = $00000020;
  DSCAPS_CERTIFIED = $00000040;
  DSCAPS_SECONDARYMONO = $00000100;
  DSCAPS_SECONDARYSTEREO = $00000200;
  DSCAPS_SECONDARY8BIT = $00000400;
  DSCAPS_SECONDARY16BIT = $00000800;

  DSBPLAY_LOOPING = $00000001;
      
  DSBSTATUS_PLAYING = $00000001;
  DSBSTATUS_BUFFERLOST = $00000002;
  DSBSTATUS_LOOPING = $00000004;

  DSBLOCK_FROMWRITECURSOR = $00000001;
  DSBLOCK_ENTIREBUFFER = $00000002;

  DSSCL_NORMAL = $00000001;
  DSSCL_PRIORITY = $00000002;
  DSSCL_EXCLUSIVE = $00000003;
  DSSCL_WRITEPRIMARY = $00000004;

  DS3DMODE_NORMAL = $00000000;
  DS3DMODE_HEADRELATIVE = $00000001;
  DS3DMODE_DISABLE = $00000002;

  DS3D_IMMEDIATE = $00000000;
  DS3D_DEFERRED = $00000001;

  DS3D_MINDISTANCEFACTOR = 0.0;
  DS3D_MAXDISTANCEFACTOR = 10.0;
  DS3D_DEFAULTDISTANCEFACTOR = 1.0;

  DS3D_MINROLLOFFFACTOR = 0.0;
  DS3D_MAXROLLOFFFACTOR = 10.0;
  DS3D_DEFAULTROLLOFFFACTOR = 1.0;

  DS3D_MINDOPPLERFACTOR = 0.0;
  DS3D_MAXDOPPLERFACTOR = 10.0;
  DS3D_DEFAULTDOPPLERFACTOR = 1.0;

  DS3D_DEFAULTMINDISTANCE = 1.0;
  DS3D_DEFAULTMAXDISTANCE = 1000000000.0;

  DS3D_MINCONEANGLE = 0;
  DS3D_MAXCONEANGLE = 360;
  DS3D_DEFAULTCONEANGLE = 360;

  DS3D_DEFAULTCONEOUTSIDEVOLUME = 0;

  DSBCAPS_PRIMARYBUFFER = $00000001;
  DSBCAPS_STATIC = $00000002;
  DSBCAPS_LOCHARDWARE = $00000004;
  DSBCAPS_LOCSOFTWARE = $00000008;
  DSBCAPS_CTRL3D = $00000010;
  DSBCAPS_CTRLFREQUENCY = $00000020;
  DSBCAPS_CTRLPAN = $00000040;
  DSBCAPS_CTRLVOLUME = $00000080;
  DSBCAPS_CTRLPOSITIONNOTIFY = $00000100;
  DSBCAPS_CTRLDEFAULT = $000000E0;
  DSBCAPS_CTRLALL = $000001F0;
  DSBCAPS_STICKYFOCUS = $00004000;
  DSBCAPS_GLOBALFOCUS = $00008000;
  DSBCAPS_GETCURRENTPOSITION2 = $00010000;
  DSBCAPS_MUTE3DATMAXDISTANCE = $00020000;

  DSCBCAPS_WAVEMAPPED = $80000000;

  DSSPEAKER_HEADPHONE = $00000001;
  DSSPEAKER_MONO = $00000002;
  DSSPEAKER_QUAD = $00000003;
  DSSPEAKER_STEREO = $00000004;
  DSSPEAKER_SURROUND = $00000005;


// #define DSSPEAKER_COMBINED(c, g)
// ((DWORD)(((BYTE)(c)) | ((DWORD)((BYTE)(g))) << 16))
  function DSSPEAKER_COMBINED(c, g: DWORD) : DWORD;
// #define DSSPEAKER_CONFIG(a)         ((BYTE)(a))
  function DSSPEAKER_CONFIG(a: DWORD) : byte;
// #define DSSPEAKER_GEOMETRY(a)       ((BYTE)(((DWORD)(a) >> 16) & 0x00FF))
  function DSSPEAKER_GEOMETRY(a: DWORD) : byte;

const
  DSCCAPS_EMULDRIVER = $00000020;

  DSCBLOCK_ENTIREBUFFER = $00000001;

  DSCBSTATUS_CAPTURING = $00000001;
  DSCBSTATUS_LOOPING = $00000002;

  DSCBSTART_LOOPING = $00000001;

  DSBFREQUENCY_MIN = 100;
  DSBFREQUENCY_MAX = 100000;
  DSBFREQUENCY_ORIGINAL = 0;

  DSBPAN_LEFT = -10000;
  DSBPAN_CENTER = 0;
  DSBPAN_RIGHT = 10000;

  DSBVOLUME_MIN = -10000;
  DSBVOLUME_MAX = 0;

  DSBSIZE_MIN = 4;
  DSBSIZE_MAX = $0FFFFFFF;

  DSBPN_OFFSETSTOP = $FFFFFFFF;


implementation

function DirectSoundCreate; external 'DSound.dll';

function DirectSoundEnumerateW; external 'DSound.dll';
function DirectSoundEnumerateA; external 'DSound.dll';
{$IFDEF UNICODE}
function DirectSoundEnumerate;
    external 'DSound.dll' name 'DirectSoundEnumerateW';
{$ELSE}
function DirectSoundEnumerate;
    external 'DSound.dll' name 'DirectSoundEnumerateA';
{$ENDIF}

function DirectSoundCaptureCreate; external 'DSound.dll';

function DirectSoundCaptureEnumerateW; external 'DSound.dll';
function DirectSoundCaptureEnumerateA; external 'DSound.dll';
{$IFDEF UNICODE}
function DirectSoundCaptureEnumerate;
    external 'DSound.dll' name 'DirectSoundCaptureEnumerateW';
{$ELSE}
function DirectSoundCaptureEnumerate;
    external 'DSound.dll' name 'DirectSoundCaptureEnumerateA';
{$ENDIF}


// #define MAKE_DSHResult(code)  MAKE_HResult(1, _FACDS, code)
function MAKE_DSHResult(code: variant) : HResult;
begin
  Result := (1 shl 31) or (_FACDS shl 16) or code;
end;

// #define DSSPEAKER_COMBINED(c, g)
// ((DWORD)(((BYTE)(c)) | ((DWORD)((BYTE)(g))) << 16))
function DSSPEAKER_COMBINED(c, g: DWORD) : DWORD;
begin
  Result := (byte(c) or byte(g)) shl 16;
end;

// #define DSSPEAKER_CONFIG(a)         ((BYTE)(a))
function DSSPEAKER_CONFIG(a: DWORD) : byte;
begin
  Result := byte(a);
end;

// #define DSSPEAKER_GEOMETRY(a)       ((BYTE)(((DWORD)(a) >> 16) & 0x00FF))
function DSSPEAKER_GEOMETRY(a: DWORD) : byte;
begin
  Result := byte(a shr 16 and $FF);
end;


end.
