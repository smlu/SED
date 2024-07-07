(*==========================================================================;
 *
 *  Copyright (C) 1996-1997 Microsoft Corporation.  All Rights Reserved.
 *
 *  File:       dinput.h
 *  Content:    DirectInput include file
 *
 *  DirectX 5 Delphi adaptation by Erik Unger
 *  Generic source created with PERL Header Converter 0.71 from David Sisson
 *
 *  Modyfied: 21.3.98
 *
 *  Download: http://www.sbox.tu-graz.ac.at/home/ungerik/DelphiGraphics/
 *  E-Mail: h_unger@magnet.at
 *
 ***************************************************************************)
 
unit DInput;

{$INCLUDE COMSWITCH.INC}
{$INCLUDE STRINGSWITCH.INC}

interface

uses
{$IFDEF D2COM}
  OLE2,
{$ENDIF}
  Windows,
  MMSystem;

type
  TRefGUID = PGUID;

  REFGUID = packed record
    case integer of
    1: (guid : PGUID);
    2: (dwFlags : DWORD);
  end;

const
  DIRECTINPUT_VERSION = $0500;

(****************************************************************************
 *
 *      Class IDs
 *
 ****************************************************************************)

  CLSID_DirectInput: TGUID =
      (D1:$25E609E0;D2:$B259;D3:$11CF;D4:($BF,$C7,$44,$45,$53,$54,$00,$00));
  CLSID_DirectInputDevice: TGUID =
      (D1:$25E609E1;D2:$B259;D3:$11CF;D4:($BF,$C7,$44,$45,$53,$54,$00,$00));

(****************************************************************************
 *
 *      Interfaces
 *
 ****************************************************************************)

  IID_IDirectInputW: TGUID =
      (D1:$89521361;D2:$AA8A;D3:$11CF;D4:($BF,$C7,$44,$45,$53,$54,$00,$00));
  IID_IDirectInputA: TGUID =
      (D1:$89521360;D2:$AA8A;D3:$11CF;D4:($BF,$C7,$44,$45,$53,$54,$00,$00));
{$IFDEF UNICODE}
  IID_IDirectInput: TGUID =
      (D1:$89521361;D2:$AA8A;D3:$11CF;D4:($BF,$C7,$44,$45,$53,$54,$00,$00));
{$ELSE}
  IID_IDirectInput: TGUID =
      (D1:$89521360;D2:$AA8A;D3:$11CF;D4:($BF,$C7,$44,$45,$53,$54,$00,$00));
{$ENDIF}

  IID_IDirectInput2W: TGUID =
      (D1:$5944E663;D2:$AA8A;D3:$11CF;D4:($BF,$C7,$44,$45,$53,$54,$00,$00));
  IID_IDirectInput2A: TGUID =
      (D1:$5944E662;D2:$AA8A;D3:$11CF;D4:($BF,$C7,$44,$45,$53,$54,$00,$00));
{$IFDEF UNICODE}
  IID_IDirectInput2: TGUID =
      (D1:$5944E663;D2:$AA8A;D3:$11CF;D4:($BF,$C7,$44,$45,$53,$54,$00,$00));
{$ELSE}
  IID_IDirectInput2: TGUID =
      (D1:$5944E662;D2:$AA8A;D3:$11CF;D4:($BF,$C7,$44,$45,$53,$54,$00,$00));
{$ENDIF}


  IID_IDirectInputDeviceW: TGUID =
      (D1:$5944E681;D2:$C92E;D3:$11CF;D4:($BF,$C7,$44,$45,$53,$54,$00,$00));
  IID_IDirectInputDeviceA: TGUID =
      (D1:$5944E680;D2:$C92E;D3:$11CF;D4:($BF,$C7,$44,$45,$53,$54,$00,$00));
{$IFDEF UNICODE}
  IID_IDirectInputDevice: TGUID =
      (D1:$5944E681;D2:$C92E;D3:$11CF;D4:($BF,$C7,$44,$45,$53,$54,$00,$00));
{$ELSE}
  IID_IDirectInputDevice: TGUID =
      (D1:$5944E680;D2:$C92E;D3:$11CF;D4:($BF,$C7,$44,$45,$53,$54,$00,$00));
{$ENDIF}

  IID_IDirectInputDevice2W: TGUID =
      (D1:$5944E683;D2:$C92E;D3:$11CF;D4:($BF,$C7,$44,$45,$53,$54,$00,$00));
  IID_IDirectInputDevice2A: TGUID =
      (D1:$5944E682;D2:$C92E;D3:$11CF;D4:($BF,$C7,$44,$45,$53,$54,$00,$00));
{$IFDEF UNICODE}
  IID_IDirectInputDevice2: TGUID =
      (D1:$5944E683;D2:$C92E;D3:$11CF;D4:($BF,$C7,$44,$45,$53,$54,$00,$00));
{$ELSE}
  IID_IDirectInputDevice2: TGUID =
      (D1:$5944E682;D2:$C92E;D3:$11CF;D4:($BF,$C7,$44,$45,$53,$54,$00,$00));
{$ENDIF}

  IID_IDirectInputEffect: TGUID =
      (D1:$E7E1F7C0;D2:$88D2;D3:$11D0;D4:($9A,$D0,$00,$A0,$C9,$A0,$6E,$35));

(****************************************************************************
 *
 *      Predefined object types
 *
 ****************************************************************************)

  GUID_XAxis: TGUID =
      (D1:$A36D02E0;D2:$C9F3;D3:$11CF;D4:($BF,$C7,$44,$45,$53,$54,$00,$00));
  GUID_YAxis: TGUID =
      (D1:$A36D02E1;D2:$C9F3;D3:$11CF;D4:($BF,$C7,$44,$45,$53,$54,$00,$00));
  GUID_ZAxis: TGUID =
      (D1:$A36D02E2;D2:$C9F3;D3:$11CF;D4:($BF,$C7,$44,$45,$53,$54,$00,$00));
  GUID_RxAxis: TGUID =
      (D1:$A36D02F4;D2:$C9F3;D3:$11CF;D4:($BF,$C7,$44,$45,$53,$54,$00,$00));
  GUID_RyAxis: TGUID =
      (D1:$A36D02F5;D2:$C9F3;D3:$11CF;D4:($BF,$C7,$44,$45,$53,$54,$00,$00));
  GUID_RzAxis: TGUID =
      (D1:$A36D02E3;D2:$C9F3;D3:$11CF;D4:($BF,$C7,$44,$45,$53,$54,$00,$00));
  GUID_Slider: TGUID =
      (D1:$A36D02E4;D2:$C9F3;D3:$11CF;D4:($BF,$C7,$44,$45,$53,$54,$00,$00));

  GUID_Button: TGUID =
      (D1:$A36D02F0;D2:$C9F3;D3:$11CF;D4:($BF,$C7,$44,$45,$53,$54,$00,$00));
  GUID_Key: TGUID =
      (D1:$55728220;D2:$D33C;D3:$11CF;D4:($BF,$C7,$44,$45,$53,$54,$00,$00));

  GUID_POV: TGUID =
      (D1:$A36D02F2;D2:$C9F3;D3:$11CF;D4:($BF,$C7,$44,$45,$53,$54,$00,$00));

  GUID_Unknown: TGUID =
      (D1:$A36D02F3;D2:$C9F3;D3:$11CF;D4:($BF,$C7,$44,$45,$53,$54,$00,$00));

(****************************************************************************
 *
 *      Predefined product GUIDs
 *
 ****************************************************************************)

  GUID_SysMouse: TGUID =
      (D1:$6F1D2B60;D2:$D5A0;D3:$11CF;D4:($BF,$C7,$44,$45,$53,$54,$00,$00));
  GUID_SysKeyboard: TGUID =
      (D1:$6F1D2B61;D2:$D5A0;D3:$11CF;D4:($BF,$C7,$44,$45,$53,$54,$00,$00));
  GUID_Joystick: TGUID =
      (D1:$6F1D2B70;D2:$D5A0;D3:$11CF;D4:($BF,$C7,$44,$45,$53,$54,$00,$00));

(****************************************************************************
 *
 *      Predefined force feedback effects
 *
 ****************************************************************************)

  GUID_ConstantForce: TGUID =
      (D1:$13541C20;D2:$8E33;D3:$11D0;D4:($9A,$D0,$00,$A0,$C9,$A0,$6E,$35));
  GUID_RampForce: TGUID =
      (D1:$13541C21;D2:$8E33;D3:$11D0;D4:($9A,$D0,$00,$A0,$C9,$A0,$6E,$35));
  GUID_Square: TGUID =
      (D1:$13541C22;D2:$8E33;D3:$11D0;D4:($9A,$D0,$00,$A0,$C9,$A0,$6E,$35));
  GUID_Sine: TGUID =
      (D1:$13541C23;D2:$8E33;D3:$11D0;D4:($9A,$D0,$00,$A0,$C9,$A0,$6E,$35));
  GUID_Triangle: TGUID =
      (D1:$13541C24;D2:$8E33;D3:$11D0;D4:($9A,$D0,$00,$A0,$C9,$A0,$6E,$35));
  GUID_SawtoothUp: TGUID =
      (D1:$13541C25;D2:$8E33;D3:$11D0;D4:($9A,$D0,$00,$A0,$C9,$A0,$6E,$35));
  GUID_SawtoothDown: TGUID =
      (D1:$13541C26;D2:$8E33;D3:$11D0;D4:($9A,$D0,$00,$A0,$C9,$A0,$6E,$35));
  GUID_Spring: TGUID =
      (D1:$13541C27;D2:$8E33;D3:$11D0;D4:($9A,$D0,$00,$A0,$C9,$A0,$6E,$35));
  GUID_Damper: TGUID =
      (D1:$13541C28;D2:$8E33;D3:$11D0;D4:($9A,$D0,$00,$A0,$C9,$A0,$6E,$35));
  GUID_Inertia: TGUID =
      (D1:$13541C29;D2:$8E33;D3:$11D0;D4:($9A,$D0,$00,$A0,$C9,$A0,$6E,$35));
  GUID_Friction: TGUID =
      (D1:$13541C2A;D2:$8E33;D3:$11D0;D4:($9A,$D0,$00,$A0,$C9,$A0,$6E,$35));
  GUID_CustomForce: TGUID =
      (D1:$13541C2B;D2:$8E33;D3:$11D0;D4:($9A,$D0,$00,$A0,$C9,$A0,$6E,$35));



(****************************************************************************
 *
 *      Interfaces and Structures...
 *
 ****************************************************************************)

(****************************************************************************
 *
 *      IDirectInputEffect
 *
 ****************************************************************************)

const
  DIEFT_ALL                   = $00000000;

  DIEFT_CONSTANTFORCE         = $00000001;
  DIEFT_RAMPFORCE             = $00000002;
  DIEFT_PERIODIC              = $00000003;
  DIEFT_CONDITION             = $00000004;
  DIEFT_CUSTOMFORCE           = $00000005;
  DIEFT_HARDWARE              = $000000FF;

  DIEFT_FFATTACK              = $00000200;
  DIEFT_FFFADE                = $00000400;
  DIEFT_SATURATION            = $00000800;
  DIEFT_POSNEGCOEFFICIENTS    = $00001000;
  DIEFT_POSNEGSATURATION      = $00002000;
  DIEFT_DEADBAND              = $00004000;

function DIEFT_GETTYPE(n: variant) : byte;

const
  DI_DEGREES                  =     100;
  DI_FFNOMINALMAX             =   10000;
  DI_SECONDS                  = 1000000;

type
  PDIConstantForce = ^TDIConstantForce;
  TDIConstantForce = packed record
    lMagnitude : longint;
  end;

  PDIRampForce = ^TDIRampForce;
  TDIRampForce = packed record
    lSart : longint;
    lEnd : longint;
  end;

  PDIPeriodic = ^TDIPeriodic;
  TDIPeriodic = packed record
    dwMagnitude : DWORD;
    lOffset : longint;
    dwPhase : DWORD;
    dwPeriod : DWORD;
  end;

  PDICondition = ^TDICondition;
  TDICondition = packed record
    lOffset : longint;
    lPositiveCoefficient : longint;
    lNegativeCoefficient : longint;
    dwPositiveSaturation : DWORD;
    dwNegativeSaturation : DWORD;
    lDeadBand : longint;
  end;

  PDICustomForce = ^TDICustomForce;
  TDICustomForce = packed record
    cChannels : DWORD;
    dwSamplePeriod : DWORD;
    cSamples : DWORD;
    rglForceData : ^longint;
  end;

  PDIEnvelope = ^TDIEnvelope;
  TDIEnvelope = packed record
    dwSize : DWORD;                   (* sizeof(DIENVELOPE)   *)
    dwAttackLevel : DWORD;
    dwAttackTime : DWORD;             (* Microseconds         *)
    dwFadeLevel : DWORD;
    dwFadeTime : DWORD;               (* Microseconds         *)
  end;

  PDIEffect = ^TDIEffect;
  TDIEffect = packed record
    dwSize : DWORD;                   (* sizeof(DIEFFECT)     *)
    dwFlags : DWORD;                  (* DIEFF_*              *)
    dwDuration : DWORD;               (* Microseconds         *)
    dwSamplePeriod : DWORD;           (* Microseconds         *)
    dwGain : DWORD;
    dwTriggerButton : DWORD;          (* or DIEB_NOTRIGGER    *)
    dwTriggerRepeatInterval : DWORD;  (* Microseconds         *)
    cAxes : DWORD;                    (* Number of axes       *)
    rgdwAxes : ^DWORD;                (* Array of axes        *)
    rglDirection : ^longint;          (* Array of directions  *)
    lpEnvelope : PDIEnvelope;         (* Optional             *)
    cbTypeSpecificParams : DWORD;     (* Size of params       *)
    lpvTypeSpecificParams : pointer;  (* Pointer to params    *)
  end;

const
  DIEFF_OBJECTIDS             = $00000001;
  DIEFF_OBJECTOFFSETS         = $00000002;
  DIEFF_CARTESIAN             = $00000010;
  DIEFF_POLAR                 = $00000020;
  DIEFF_SPHERICAL             = $00000040;

  DIEP_DURATION               = $00000001;
  DIEP_SAMPLEPERIOD           = $00000002;
  DIEP_GAIN                   = $00000004;
  DIEP_TRIGGERBUTTON          = $00000008;
  DIEP_TRIGGERREPEATINTERVAL  = $00000010;
  DIEP_AXES                   = $00000020;
  DIEP_DIRECTION              = $00000040;
  DIEP_ENVELOPE               = $00000080;
  DIEP_TYPESPECIFICPARAMS     = $00000100;
  DIEP_ALLPARAMS              = $000001FF;
  DIEP_START                  = $20000000;
  DIEP_NORESTART              = $40000000;
  DIEP_NODOWNLOAD             = $80000000;
  DIEB_NOTRIGGER              = $FFFFFFFF;

  DIES_SOLO                   = $00000001;
  DIES_NODOWNLOAD             = $80000000;

  DIEGES_PLAYING              = $00000001;
  DIEGES_EMULATED             = $00000002;


type
  PDIEffEscape = ^TDIEffEscape;
  TDIEffEscape = packed record
    dwSize : DWORD;
    dwCommand : DWORD;
    lpvInBuffer : pointer;
    cbInBuffer : DWORD;
    lpvOutBuffer : pointer;
    cbOutBuffer : DWORD;
  end;


//
// IDirectSoundCapture
//
{$IFDEF D2COM}
  IDirectInputEffect = class (IUnknown)
{$ELSE}
  IDirectInputEffect = interface (IUnknown)
    ['{E7E1F7C0-88D2-11D0-9AD0-00A0C9A06E35}']
{$ENDIF}
    (** IDirectInputEffect methods ***)
    function Initialize(hinst: THandle; dwVersion: DWORD;
        rguid: TGUID) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetEffectGuid(var pguid: TGUID) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetParameters(const peff: TDIEffect; dwFlags: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetParameters(const peff: TDIEffect; dwFlags: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function Start(dwIterations: DWORD; dwFlags: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function Stop : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetEffectStatus(var pdwFlags : DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function Download : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function Unload : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function Escape(var pesc: TDIEffEscape) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
  end;

(****************************************************************************
 *
 *      IDirectInputDevice
 *
 ****************************************************************************)

const
  DIDEVTYPE_DEVICE = 1;
  DIDEVTYPE_MOUSE = 2;
  DIDEVTYPE_KEYBOARD = 3;
  DIDEVTYPE_JOYSTICK = 4;
  DIDEVTYPE_HID = $00010000;

  DIDEVTYPEMOUSE_UNKNOWN = 1;
  DIDEVTYPEMOUSE_TRADITIONAL = 2;
  DIDEVTYPEMOUSE_FINGERSTICK = 3;
  DIDEVTYPEMOUSE_TOUCHPAD = 4;
  DIDEVTYPEMOUSE_TRACKBALL = 5;

  DIDEVTYPEKEYBOARD_UNKNOWN = 0;
  DIDEVTYPEKEYBOARD_PCXT = 1;
  DIDEVTYPEKEYBOARD_OLIVETTI = 2;
  DIDEVTYPEKEYBOARD_PCAT = 3;
  DIDEVTYPEKEYBOARD_PCENH = 4;
  DIDEVTYPEKEYBOARD_NOKIA1050 = 5;
  DIDEVTYPEKEYBOARD_NOKIA9140 = 6;
  DIDEVTYPEKEYBOARD_NEC98 = 7;
  DIDEVTYPEKEYBOARD_NEC98LAPTOP = 8;
  DIDEVTYPEKEYBOARD_NEC98106 = 9;
  DIDEVTYPEKEYBOARD_JAPAN106 = 10;
  DIDEVTYPEKEYBOARD_JAPANAX = 11;
  DIDEVTYPEKEYBOARD_J3100 = 12;

  DIDEVTYPEJOYSTICK_UNKNOWN = 1;
  DIDEVTYPEJOYSTICK_TRADITIONAL = 2;
  DIDEVTYPEJOYSTICK_FLIGHTSTICK = 3;
  DIDEVTYPEJOYSTICK_GAMEPAD = 4;
  DIDEVTYPEJOYSTICK_RUDDER = 5;
  DIDEVTYPEJOYSTICK_WHEEL = 6;
  DIDEVTYPEJOYSTICK_HEADTRACKER = 7;

function GET_DIDEVICE_TYPE(dwDevType: variant) : byte;
function GET_DIDEVICE_SUBTYPE(dwDevType: variant) : byte;

type
  PDIDevCaps_DX3 = ^TDIDevCaps_DX3;
  TDIDevCaps_DX3 = packed record
    dwSize: DWORD;
    dwFlags: DWORD;
    dwDevType: DWORD;
    dwAxes: DWORD;
    dwButtons: DWORD;
    dwPOVs: DWORD;
  end;

  PDIDevCaps = ^TDIDevCaps;
  TDIDevCaps = packed record
    dwSize: DWORD;
    dwFlags: DWORD;
    dwDevType: DWORD;
    dwAxes: DWORD;
    dwButtons: DWORD;
    dwPOVs: DWORD;
    dwFFSamplePeriod: DWORD;
    dwFFMinTimeResolution: DWORD;
    dwFirmwareRevision: DWORD;
    dwHardwareRevision: DWORD;
    dwFFDriverVersion: DWORD;
  end;

const
  DIDC_ATTACHED = $00000001;
  DIDC_POLLEDDEVICE = $00000002;
  DIDC_EMULATED = $00000004;
  DIDC_POLLEDDATAFORMAT = $00000008;
  DIDC_FORCEFEEDBACK      = $00000100;
  DIDC_FFATTACK           = $00000200;
  DIDC_FFFADE             = $00000400;
  DIDC_SATURATION         = $00000800;
  DIDC_POSNEGCOEFFICIENTS = $00001000;
  DIDC_POSNEGSATURATION   = $00002000;
  DIDC_DEADBAND           = $00004000;

  DIDFT_ALL = $00000000;

  DIDFT_RELAXIS = $00000001;
  DIDFT_ABSAXIS = $00000002;
  DIDFT_AXIS = $00000003;

  DIDFT_PSHBUTTON = $00000004;
  DIDFT_TGLBUTTON = $00000008;
  DIDFT_BUTTON = $0000000C;

  DIDFT_POV = $00000010;

  DIDFT_COLLECTION = $00000040;
  DIDFT_NODATA = $00000080;

  DIDFT_ANYINSTANCE = $00FFFF00;
  DIDFT_INSTANCEMASK = DIDFT_ANYINSTANCE;
function DIDFT_MAKEINSTANCE(n: variant) : DWORD;
function DIDFT_GETTYPE(n: variant) : byte;
function DIDFT_GETINSTANCE(n: variant) : DWORD;
const
  DIDFT_FFACTUATOR = $01000000;
  DIDFT_FFEFFECTTRIGGER = $02000000;

function DIDFT_ENUMCOLLECTION(n: word) : DWORD;
const
  DIDFT_NOCOLLECTION = $00FFFF00;



type
  PDIObjectDataFormat = ^TDIObjectDataFormat;
  TDIObjectDataFormat = packed record
    pguid: ^TGUID;
    dwOfs: DWORD;
    dwType: DWORD;
    dwFlags: DWORD;
  end;

  PDIDataFormat = ^TDIDataFormat;
  TDIDataFormat = packed record
    dwSize: DWORD;   
    dwObjSize: DWORD;   
    dwFlags: DWORD;   
    dwDataSize: DWORD;   
    dwNumObjs: DWORD;   
    rgodf: PDIObjectDataFormat;
  end;

const
  DIDF_ABSAXIS = $00000001;
  DIDF_RELAXIS = $00000002;

type
  PDIDeviceObjectInstance_DX3A = ^TDIDeviceObjectInstance_DX3A;
  TDIDeviceObjectInstance_DX3A = packed record
    dwSize: DWORD;
    guidType: TGUID;
    dwOfs: DWORD;
    dwType: DWORD;
    dwFlags: DWORD;
    tszName: Array [0..MAX_PATH-1] of AnsiCHAR;
  end;

  PDIDeviceObjectInstance_DX3W = ^TDIDeviceObjectInstance_DX3W;
  TDIDeviceObjectInstance_DX3W = packed record
    dwSize: DWORD;
    guidType: TGUID;
    dwOfs: DWORD;
    dwType: DWORD;
    dwFlags: DWORD;
    tszName: Array [0..MAX_PATH-1] of WCHAR;
  end;


  PDIDeviceObjectInstance_DX3 = ^TDIDeviceObjectInstance_DX3;
{$IFDEF UNICODE}
  TDIDeviceObjectInstance_DX3 = TDIDeviceObjectInstance_DX3W;
{$ELSE}
  TDIDeviceObjectInstance_DX3 = TDIDeviceObjectInstance_DX3A;
{$ENDIF}

  PDIDeviceObjectInstanceA = ^TDIDeviceObjectInstanceA;
  TDIDeviceObjectInstanceA = packed record
    dwSize: DWORD;
    guidType: TGUID;
    dwOfs: DWORD;
    dwType: DWORD;
    dwFlags: DWORD;
    tszName: Array [0..MAX_PATH-1] of AnsiCHAR;
    dwFFMaxForce: DWORD;
    dwFFForceResolution: DWORD;
    wCollectionNumber: WORD;
    wDesignatorIndex: WORD;
    wUsagePage: WORD;
    wUsage: WORD;
    dwDimension: DWORD;
    wExponent: WORD;
    wReserved: WORD;
  end;

  PDIDeviceObjectInstanceW = ^TDIDeviceObjectInstanceW;
  TDIDeviceObjectInstanceW = packed record
    dwSize: DWORD;
    guidType: TGUID;
    dwOfs: DWORD;
    dwType: DWORD;
    dwFlags: DWORD;
    tszName: Array [0..MAX_PATH-1] of WCHAR;
    dwFFMaxForce: DWORD;
    dwFFForceResolution: DWORD;
    wCollectionNumber: WORD;
    wDesignatorIndex: WORD;
    wUsagePage: WORD;
    wUsage: WORD;
    dwDimension: DWORD;
    wExponent: WORD;
    wReserved: WORD;
  end;

  PDIDeviceObjectInstance = ^TDIDeviceObjectInstance;
{$IFDEF UNICODE}
  TDIDeviceObjectInstance = TDIDeviceObjectInstanceW;
{$ELSE}
  TDIDeviceObjectInstance = TDIDeviceObjectInstanceA;
{$ENDIF}


  TDIEnumDeviceObjectsCallbackA = function (
      const lpddoi: TDIDeviceObjectInstanceA; pvRef: Pointer) : BOOL; stdcall;
  TDIEnumDeviceObjectsCallbackW = function (
      const lpddoi: TDIDeviceObjectInstanceW; pvRef: Pointer) : BOOL; stdcall;
  TDIEnumDeviceObjectsCallback = function (
      const lpddoi: TDIDeviceObjectInstance; pvRef: Pointer) : BOOL; stdcall;

  TDIEnumDeviceObjectsProc = function (
      const lpddoi: TDIDeviceObjectInstance; pvRef: Pointer): BOOL; stdcall;

const
  DIDOI_FFACTUATOR        = $00000001;
  DIDOI_FFEFFECTTRIGGER   = $00000002;
  DIDOI_POLLED            = $00008000;
  DIDOI_ASPECTPOSITION    = $00000100;
  DIDOI_ASPECTVELOCITY    = $00000200;
  DIDOI_ASPECTACCEL       = $00000300;
  DIDOI_ASPECTFORCE       = $00000400;
  DIDOI_ASPECTMASK        = $00000F00;

type
  PDIPropHeader = ^TDIPropHeader;
  TDIPropHeader = packed record
    dwSize: DWORD;
    dwHeaderSize: DWORD;
    dwObj: DWORD;
    dwHow: DWORD;
  end;

const
  DIPH_DEVICE = 0;
  DIPH_BYOFFSET = 1;
  DIPH_BYID = 2;

type
  PDIPropDWord = ^TDIProDWord;
  TDIProDWord = packed record
    diph: TDIPropHeader;
    dwData: DWORD;
  end;

  PDIPropRange = ^TDIPropRange;
  TDIPropRange = packed record
    diph: TDIPropHeader;
    lMin: Longint;
    lMax: Longint;
  end;

const
  DIPROPRANGE_NOMIN = $80000000;
  DIPROPRANGE_NOMAX = $7FFFFFFF;

type
  MAKEDIPROP = TRefGUID;

const
  DIPROP_BUFFERSIZE = MAKEDIPROP(1);

  DIPROP_AXISMODE = MAKEDIPROP(2);

  DIPROPAXISMODE_ABS = 0;
  DIPROPAXISMODE_REL = 1;

  DIPROP_GRANULARITY = MAKEDIPROP(3);

  DIPROP_RANGE = MAKEDIPROP(4);

  DIPROP_DEADZONE = MAKEDIPROP(5);

  DIPROP_SATURATION = MAKEDIPROP(6);

  DIPROP_FFGAIN = MAKEDIPROP(7);

  DIPROP_FFLOAD = MAKEDIPROP(8);

  DIPROP_AUTOCENTER = MAKEDIPROP(9);

  DIPROPAUTOCENTER_OFF = 0;
  DIPROPAUTOCENTER_ON = 1;

  DIPROP_CALIBRATIONMODE = MAKEDIPROP(10);

  DIPROPCALIBRATIONMODE_COOKED = 0;
  DIPROPCALIBRATIONMODE_RAW = 1;

type
  PDIDeviceObjectData = ^TDIDeviceObjectData;
  TDIDeviceObjectData = packed record
    dwOfs: DWORD;
    dwData: DWORD;
    dwTimeStamp: DWORD;
    dwSequence: DWORD;
  end;

const
  DIGDD_PEEK = $00000001;
{
#define DISEQUENCE_COMPARE(dwSequence1, cmp, dwSequence2) \
                         (int) ((dwSequence1) - (dwSequence2))  cmp 0
}
  DISCL_EXCLUSIVE = $00000001;
  DISCL_NONEXCLUSIVE = $00000002;
  DISCL_FOREGROUND = $00000004;
  DISCL_BACKGROUND = $00000008;


type
  PDIDeviceInstanceA = ^TDIDeviceInstanceA;
  TDIDeviceInstanceA = packed record
    dwSize: DWORD;
    guidInstance: TGUID;
    guidProduct: TGUID;
    dwDevType: DWORD;
    tszInstanceName: Array [0..MAX_PATH-1] of AnsiCHAR;
    tszProductName: Array [0..MAX_PATH-1] of AnsiCHAR;
    guidFFDriver: TGUID;
    wUsagePage: WORD;
    wUsage: WORD;
  end;

  PDIDeviceInstanceW = ^TDIDeviceInstanceW;
  TDIDeviceInstanceW = packed record
    dwSize: DWORD;
    guidInstance: TGUID;
    guidProduct: TGUID;
    dwDevType: DWORD;
    tszInstanceName: Array [0..MAX_PATH-1] of WCHAR;
    tszProductName: Array [0..MAX_PATH-1] of WCHAR;
    guidFFDriver: TGUID;
    wUsagePage: WORD;
    wUsage: WORD;
  end;


  PDIDeviceInstance = ^TDIDeviceInstance;
{$IFDEF UNICODE}
  TDIDeviceInstance = TDIDeviceInstanceW;
{$ELSE}
  TDIDeviceInstance = TDIDeviceInstanceA;
{$ENDIF}


{$IFDEF D2COM}
  IDirectInputDeviceAW = class (IUnknown)
{$ELSE}
  IDirectInputDeviceAW = interface (IUnknown)
{$ENDIF}
    (*** IDirectInputDeviceW methods ***)
    function GetCapabilities(var lpDIDevCaps: TDIDevCaps) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function EnumObjects(lpCallback: TDIEnumDeviceObjectsCallbackW;
        pvRef: Pointer; dwFlags: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetProperty(rguidProp: TRefGUID; var pdiph: TDIPropHeader) :
        HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetProperty(rguidProp: TRefGUID; const pdiph: TDIPropHeader) :
        HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function Acquire : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function Unacquire : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetDeviceState(cbData: DWORD; lpvData: Pointer) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetDeviceData(cbObjectData: DWORD; var rgdod: TDIDeviceObjectData;
        var pdwInOut: DWORD; dwFlags: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetDataFormat(const lpdf: TDIDataFormat) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetEventNotification(hEvent: THandle) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetCooperativeLevel(hwnd: HWND; dwFlags: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetObjectInfo(var pdidoi: TDIDeviceObjectInstanceW; dwObj: DWORD;
        dwHow: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetDeviceInfo(var pdidi: TDIDeviceInstanceW) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function RunControlPanel(hwndOwner: HWND; dwFlags: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function Initialize(hinst: THandle; dwVersion: DWORD; rguid: PGUID) :
        HRESULT;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
  end;

{$IFDEF D2COM}
  IDirectInputDeviceA = IDirectInputDeviceAW;
  IDirectInputDeviceW = IDirectInputDeviceAW;
{$ELSE}
  IDirectInputDeviceW = interface (IDirectInputDeviceAW)
    ['{5944E681-C92E-11CF-BFC7-444553540000}']
  end;
  IDirectInputDeviceA = interface (IDirectInputDeviceAW)
    ['{5944E680-C92E-11CF-BFC7-444553540000}']
  end;
{$ENDIF}

{$IFDEF UNICODE}
  IDirectInputDevice = IDirectInputDeviceW;
{$ELSE}
  IDirectInputDevice = IDirectInputDeviceA;
{$ENDIF}

const
  DISFFC_RESET            = $00000001;
  DISFFC_STOPALL          = $00000002;
  DISFFC_PAUSE            = $00000004;
  DISFFC_CONTINUE         = $00000008;
  DISFFC_SETACTUATORSON   = $00000010;
  DISFFC_SETACTUATORSOFF  = $00000020;

  DIGFFS_EMPTY            = $00000001;
  DIGFFS_STOPPED          = $00000002;
  DIGFFS_PAUSED           = $00000004;
  DIGFFS_ACTUATORSON      = $00000010;
  DIGFFS_ACTUATORSOFF     = $00000020;
  DIGFFS_POWERON          = $00000040;
  DIGFFS_POWEROFF         = $00000080;
  DIGFFS_SAFETYSWITCHON   = $00000100;
  DIGFFS_SAFETYSWITCHOFF  = $00000200;
  DIGFFS_USERFFSWITCHON   = $00000400;
  DIGFFS_USERFFSWITCHOFF  = $00000800;
  DIGFFS_DEVICELOST       = $80000000;

type
  PDIEffectInfoA = ^TDIEffectInfoA;
  TDIEffectInfoA = packed record
    dwSize : DWORD;
    guid : TGUID;
    dwEffType : DWORD;
    dwStaticParams : DWORD;
    dwDynamicParams : DWORD;
    tszName : array [0..MAX_PATH-1] of AnsiCHAR;
  end;

  PDIEffectInfoW = ^TDIEffectInfoW;
  TDIEffectInfoW = packed record
    dwSize : DWORD;
    guid : TGUID;
    dwEffType : DWORD;
    dwStaticParams : DWORD;
    dwDynamicParams : DWORD;
    tszName : array [0..MAX_PATH-1] of WCHAR;
  end;


  PDIEffectInfo = ^TDIEffectInfo;
{$IFDEF UNICODE}
  TDIEffectInfo = TDIEffectInfoW;
{$ELSE}
  TDIEffectInfo = TDIEffectInfoA;
{$ENDIF}

  TDIEnumEffectsCallbackA =
      function(const pdei: TDIEffectInfoA; pvRef: pointer) : BOOL; stdcall;
  TDIEnumEffectsCallbackW =
      function(const pdei: TDIEffectInfoW; pvRef: pointer) : BOOL; stdcall;
  TDIEnumEffectsCallback =
      function(const pdei: TDIEffectInfo; pvRef: pointer) : BOOL; stdcall;
  TDIEnumEffectsProc = TDIEnumEffectsCallback;


  TDIEnumCreatedEffectObjectsCallback =
      function(peff: IDirectInputEffect; pvRev: pointer) : BOOL; stdcall;
  TDIEnumCreatedEffectObjectsProc = TDIEnumCreatedEffectObjectsCallback;

{$IFDEF D2COM}
  IDirectInputDevice2AW = class (IDirectInputDeviceAW)
{$ELSE}
  IDirectInputDevice2AW = interface (IDirectInputDeviceAW)
{$ENDIF}
    (*** IDirectInputDevice2W methods ***)
    function CreateEffect(rguid: PGUID; lpeff: PDIEffect;
        var ppdeff: IDirectInputEffect; punkOuter: IUnknown) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function EnumEffects(lpCallback: TDIEnumEffectsCallbackW;
        pvRef: pointer; dwEffType: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetEffectInfo(pdei: TDIEffectInfoW; rguid: PGUID) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetForceFeedbackState(var pdwOut: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SendForceFeedbackCommand(dwFlags: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function EnumCreatedEffectObjects(lpCallback:
        TDIEnumCreatedEffectObjectsCallback;
        pvRef: pointer; fl: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function Escape(var pesc: TDIEffEscape) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function Poll : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SendDeviceData(Arg1: DWORD; var Arg2: TDIDeviceObjectData;
        var Arg3: DWORD; Arg4: DWORD) : HResult; //?
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
  end;

{$IFDEF D2COM}
  IDirectInputDevice2A = IDirectInputDevice2AW;
  IDirectInputDevice2W = IDirectInputDevice2AW;
{$ELSE}
  IDirectInputDevice2W = interface (IDirectInputDevice2AW)
    ['{5944E683-C92E-11CF-BFC7-444553540000}']
  end;
  IDirectInputDevice2A = interface (IDirectInputDevice2AW)
    ['{5944E682-C92E-11CF-BFC7-444553540000}']
  end;
{$ENDIF}

{$IFDEF UNICODE}
  IDirectInputDevice2 = IDirectInputDevice2W;
{$ELSE}
  IDirectInputDevice2 = IDirectInputDevice2A;
{$ENDIF}



(****************************************************************************
 *
 *      Mouse
 *
 ****************************************************************************)


  PDIMouseState = ^TDIMouseState;
  TDIMouseState = packed record
    lX: Longint;
    lY: Longint;
    lZ: Longint;
    rgbButtons: Array [0..3] of BYTE;
  end;

const
  DIMOFS_X       = 0;
  DIMOFS_Y       = 4;
  DIMOFS_Z       = 8;
  DIMOFS_BUTTON0 = 12;
  DIMOFS_BUTTON1 = 13;
  DIMOFS_BUTTON2 = 14;
  DIMOFS_BUTTON3 = 15;

const
  _c_dfDIMouse_Objects: array[0..1] of TDIObjectDataFormat = (
    (  pguid: nil;
       dwOfs: 0;
       dwType: DIDFT_RELAXIS or DIDFT_ANYINSTANCE;
       dwFlags: 0),
    (  pguid: @GUID_Button;
       dwOfs: 12;
       dwType: DIDFT_BUTTON or DIDFT_ANYINSTANCE;
       dwFlags: 0)
  );

  c_dfDIMouse: TDIDataFormat = (
    dwSize: Sizeof(c_dfDIMouse);
    dwObjSize: Sizeof(TDIObjectDataFormat);
    dwFlags: DIDF_RELAXIS;
    dwDataSize: Sizeof(TDIMouseState);
    dwNumObjs: High(_c_dfDIMouse_Objects)+1;
    rgodf: @_c_dfDIMouse_Objects[Low(_c_dfDIMouse_Objects)]
  );


(****************************************************************************
 *
 *      Keyboard
 *
 ****************************************************************************)

type
  TDIKeyboardState = array[0..255] of Byte;

const
  _c_dfDIKeyboard_Objects: array[0..0] of TDIObjectDataFormat = (
    (  pguid: @GUID_Key;
       dwOfs: 1;
       dwType: DIDFT_BUTTON or DIDFT_ANYINSTANCE;
       dwFlags: 0)
  );

  c_dfDIKeyboard: TDIDataFormat = (
    dwSize: Sizeof(c_dfDIKeyboard);
    dwObjSize: Sizeof(TDIObjectDataFormat);
    dwFlags: DIDF_RELAXIS;
    dwDataSize: Sizeof(TDIKeyboardState);
    dwNumObjs: High(_c_dfDIKeyboard_Objects)+1;
    rgodf: @_c_dfDIKeyboard_Objects[Low(_c_dfDIKeyboard_Objects)]
  );

(****************************************************************************
 *
 *      DirectInput keyboard scan codes
 *
 ****************************************************************************)

const
  DIK_ESCAPE          = $01;
  DIK_1               = $02;
  DIK_2               = $03;
  DIK_3               = $04;
  DIK_4               = $05;
  DIK_5               = $06;
  DIK_6               = $07;
  DIK_7               = $08;
  DIK_8               = $09;
  DIK_9               = $0A;
  DIK_0               = $0B;
  DIK_MINUS           = $0C;    (* - on main keyboard *)
  DIK_EQUALS          = $0D;
  DIK_BACK            = $0E;    (* backspace *)
  DIK_TAB             = $0F;
  DIK_Q               = $10;
  DIK_W               = $11;
  DIK_E               = $12;
  DIK_R               = $13;
  DIK_T               = $14;
  DIK_Y               = $15;
  DIK_U               = $16;
  DIK_I               = $17;
  DIK_O               = $18;
  DIK_P               = $19;
  DIK_LBRACKET        = $1A;
  DIK_RBRACKET        = $1B;
  DIK_RETURN          = $1C;    (* Enter on main keyboard *)
  DIK_LCONTROL        = $1D;
  DIK_A               = $1E;
  DIK_S               = $1F;
  DIK_D               = $20;
  DIK_F               = $21;
  DIK_G               = $22;
  DIK_H               = $23;
  DIK_J               = $24;
  DIK_K               = $25;
  DIK_L               = $26;
  DIK_SEMICOLON       = $27;
  DIK_APOSTROPHE      = $28;
  DIK_GRAVE           = $29;    (* accent grave *)
  DIK_LSHIFT          = $2A;
  DIK_BACKSLASH       = $2B;
  DIK_Z               = $2C;
  DIK_X               = $2D;
  DIK_C               = $2E;
  DIK_V               = $2F;
  DIK_B               = $30;
  DIK_N               = $31;
  DIK_M               = $32;
  DIK_COMMA           = $33;
  DIK_PERIOD          = $34;    (* . on main keyboard *)
  DIK_SLASH           = $35;    (* / on main keyboard *)
  DIK_RSHIFT          = $36;
  DIK_MULTIPLY        = $37;    (* * on numeric keypad *)
  DIK_LMENU           = $38;    (* left Alt *)
  DIK_SPACE           = $39;
  DIK_CAPITAL         = $3A;
  DIK_F1              = $3B;
  DIK_F2              = $3C;
  DIK_F3              = $3D;
  DIK_F4              = $3E;
  DIK_F5              = $3F;
  DIK_F6              = $40;
  DIK_F7              = $41;
  DIK_F8              = $42;
  DIK_F9              = $43;
  DIK_F10             = $44;
  DIK_NUMLOCK         = $45;
  DIK_SCROLL          = $46;    (* Scroll Lock *)
  DIK_NUMPAD7         = $47;
  DIK_NUMPAD8         = $48;
  DIK_NUMPAD9         = $49;
  DIK_SUBTRACT        = $4A;    (* - on numeric keypad *)
  DIK_NUMPAD4         = $4B;
  DIK_NUMPAD5         = $4C;
  DIK_NUMPAD6         = $4D;
  DIK_ADD             = $4E;    (* + on numeric keypad *)
  DIK_NUMPAD1         = $4F;
  DIK_NUMPAD2         = $50;
  DIK_NUMPAD3         = $51;
  DIK_NUMPAD0         = $52;
  DIK_DECIMAL         = $53;    (* . on numeric keypad *)
  DIK_F11             = $57;
  DIK_F12             = $58;

  DIK_F13             = $64;    (*                     (NEC PC98) *)
  DIK_F14             = $65;    (*                     (NEC PC98) *)
  DIK_F15             = $66;    (*                     (NEC PC98) *)

  DIK_KANA            = $70;    (* (Japanese keyboard)            *)
  DIK_CONVERT         = $79;    (* (Japanese keyboard)            *)
  DIK_NOCONVERT       = $7B;    (* (Japanese keyboard)            *)
  DIK_YEN             = $7D;    (* (Japanese keyboard)            *)
  DIK_NUMPADEQUALS    = $8D;    (* = on numeric keypad (NEC PC98) *)
  DIK_CIRCUMFLEX      = $90;    (* (Japanese keyboard)            *)
  DIK_AT              = $91;    (*                     (NEC PC98) *)
  DIK_COLON           = $92;    (*                     (NEC PC98) *)
  DIK_UNDERLINE       = $93;    (*                     (NEC PC98) *)
  DIK_KANJI           = $94;    (* (Japanese keyboard)            *)
  DIK_STOP            = $95;    (*                     (NEC PC98) *)
  DIK_AX              = $96;    (*                     (Japan AX) *)
  DIK_UNLABELED       = $97;    (*                        (J3100) *)
  DIK_NUMPADENTER     = $9C;    (* Enter on numeric keypad *)
  DIK_RCONTROL        = $9D;
  DIK_NUMPADCOMMA     = $B3;    (* , on numeric keypad (NEC PC98) *)
  DIK_DIVIDE          = $B5;    (* / on numeric keypad *)
  DIK_SYSRQ           = $B7;
  DIK_RMENU           = $B8;    (* right Alt *)
  DIK_HOME            = $C7;    (* Home on arrow keypad *)
  DIK_UP              = $C8;    (* UpArrow on arrow keypad *)
  DIK_PRIOR           = $C9;    (* PgUp on arrow keypad *)
  DIK_LEFT            = $CB;    (* LeftArrow on arrow keypad *)
  DIK_RIGHT           = $CD;    (* RightArrow on arrow keypad *)
  DIK_END             = $CF;    (* End on arrow keypad *)
  DIK_DOWN            = $D0;    (* DownArrow on arrow keypad *)
  DIK_NEXT            = $D1;    (* PgDn on arrow keypad *)
  DIK_INSERT          = $D2;    (* Insert on arrow keypad *)
  DIK_DELETE          = $D3;    (* Delete on arrow keypad *)
  DIK_LWIN            = $DB;    (* Left Windows key *)
  DIK_RWIN            = $DC;    (* Right Windows key *)
  DIK_APPS            = $DD;    (* AppMenu key *)

(*
 *  Alternate names for keys, to facilitate transition from DOS.
 *)
  DIK_BACKSPACE      = DIK_BACK    ;        (* backspace *)
  DIK_NUMPADSTAR     = DIK_MULTIPLY;        (* * on numeric keypad *)
  DIK_LALT           = DIK_LMENU   ;        (* left Alt *)
  DIK_CAPSLOCK       = DIK_CAPITAL ;        (* CapsLock *)
  DIK_NUMPADMINUS    = DIK_SUBTRACT;        (* - on numeric keypad *)
  DIK_NUMPADPLUS     = DIK_ADD     ;        (* + on numeric keypad *)
  DIK_NUMPADPERIOD   = DIK_DECIMAL ;        (* . on numeric keypad *)
  DIK_NUMPADSLASH    = DIK_DIVIDE  ;        (* / on numeric keypad *)
  DIK_RALT           = DIK_RMENU   ;        (* right Alt *)
  DIK_UPARROW        = DIK_UP      ;        (* UpArrow on arrow keypad *)
  DIK_PGUP           = DIK_PRIOR   ;        (* PgUp on arrow keypad *)
  DIK_LEFTARROW      = DIK_LEFT    ;        (* LeftArrow on arrow keypad *)
  DIK_RIGHTARROW     = DIK_RIGHT   ;        (* RightArrow on arrow keypad *)
  DIK_DOWNARROW      = DIK_DOWN    ;        (* DownArrow on arrow keypad *)
  DIK_PGDN           = DIK_NEXT    ;        (* PgDn on arrow keypad *)


(****************************************************************************
 *
 *      Joystick
 *
 ****************************************************************************)


type
  PDIJoyState = ^TDIJoyState;
  TDIJoyState = packed record
    lX: Longint;   (* x-axis position              *)
    lY: Longint;   (* y-axis position              *)
    lZ: Longint;   (* z-axis position              *)
    lRx: Longint;   (* x-axis rotation              *)
    lRy: Longint;   (* y-axis rotation              *)
    lRz: Longint;   (* z-axis rotation              *)
    rglSlider: Array [0..1] of Longint;   (* extra axes positions         *)
    rgdwPOV: Array [0..3] of DWORD;   (* POV directions               *)
    rgbButtons: Array [0..31] of BYTE;   (* 32 buttons                   *)
  end;

  PDIJoyState2 = ^TDIJoyState2;
  TDIJoyState2 = packed record
    lX: Longint;   (* x-axis position              *)
    lY: Longint;   (* y-axis position              *)
    lZ: Longint;   (* z-axis position              *)
    lRx: Longint;   (* x-axis rotation              *)
    lRy: Longint;   (* y-axis rotation              *)
    lRz: Longint;   (* z-axis rotation              *)
    rglSlider: Array [0..1] of Longint;   (* extra axes positions         *)
    rgdwPOV: Array [0..3] of DWORD;   (* POV directions               *)
    rgbButtons: Array [0..127] of BYTE;   (* 128 buttons                  *)
    lVX: Longint;   (* x-axis velocity              *)
    lVY: Longint;   (* y-axis velocity              *)
    lVZ: Longint;   (* z-axis velocity              *)
    lVRx: Longint;   (* x-axis angular velocity      *)
    lVRy: Longint;   (* y-axis angular velocity      *)
    lVRz: Longint;   (* z-axis angular velocity      *)
    rglVSlider: Array [0..1] of Longint;   (* extra axes velocities        *)
    lAX: Longint;   (* x-axis acceleration          *)
    lAY: Longint;   (* y-axis acceleration          *)
    lAZ: Longint;   (* z-axis acceleration          *)
    lARx: Longint;   (* x-axis angular acceleration  *)
    lARy: Longint;   (* y-axis angular acceleration  *)
    lARz: Longint;   (* z-axis angular acceleration  *)
    rglASlider: Array [0..1] of Longint;   (* extra axes accelerations     *)
    lFX: Longint;   (* x-axis force                 *)
    lFY: Longint;   (* y-axis force                 *)
    lFZ: Longint;   (* z-axis force                 *)
    lFRx: Longint;   (* x-axis torque                *)
    lFRy: Longint;   (* y-axis torque                *)
    lFRz: Longint;   (* z-axis torque                *)
    rglFSlider: Array [0..1] of Longint;   (* extra axes forces            *)
  end;

const
  DIJOFS_X  =0;
  DIJOFS_Y  =4;
  DIJOFS_Z  =8;
  DIJOFS_RX =12;
  DIJOFS_RY =16;
  DIJOFS_RZ =20;

function DIJOFS_SLIDER(n: variant) : variant;

function DIJOFS_POV(n: variant) : variant;

function DIJOFS_BUTTON(n: variant) : variant;
const
  DIJOFS_BUTTON_ = 48;

const
  DIJOFS_BUTTON0 = DIJOFS_BUTTON_ + 0;
  DIJOFS_BUTTON1 = DIJOFS_BUTTON_ + 1;
  DIJOFS_BUTTON2 = DIJOFS_BUTTON_ + 2;
  DIJOFS_BUTTON3 = DIJOFS_BUTTON_ + 3;
  DIJOFS_BUTTON4 = DIJOFS_BUTTON_ + 4;
  DIJOFS_BUTTON5 = DIJOFS_BUTTON_ + 5;
  DIJOFS_BUTTON6 = DIJOFS_BUTTON_ + 6;
  DIJOFS_BUTTON7 = DIJOFS_BUTTON_ + 7;
  DIJOFS_BUTTON8 = DIJOFS_BUTTON_ + 8;
  DIJOFS_BUTTON9 = DIJOFS_BUTTON_ + 9;
  DIJOFS_BUTTON10 = DIJOFS_BUTTON_ + 10;
  DIJOFS_BUTTON11 = DIJOFS_BUTTON_ + 11;
  DIJOFS_BUTTON12 = DIJOFS_BUTTON_ + 12;
  DIJOFS_BUTTON13 = DIJOFS_BUTTON_ + 13;
  DIJOFS_BUTTON14 = DIJOFS_BUTTON_ + 14;
  DIJOFS_BUTTON15 = DIJOFS_BUTTON_ + 15;
  DIJOFS_BUTTON16 = DIJOFS_BUTTON_ + 16;
  DIJOFS_BUTTON17 = DIJOFS_BUTTON_ + 17;
  DIJOFS_BUTTON18 = DIJOFS_BUTTON_ + 18;
  DIJOFS_BUTTON19 = DIJOFS_BUTTON_ + 19;
  DIJOFS_BUTTON20 = DIJOFS_BUTTON_ + 20;
  DIJOFS_BUTTON21 = DIJOFS_BUTTON_ + 21;
  DIJOFS_BUTTON22 = DIJOFS_BUTTON_ + 22;
  DIJOFS_BUTTON23 = DIJOFS_BUTTON_ + 23;
  DIJOFS_BUTTON24 = DIJOFS_BUTTON_ + 24;
  DIJOFS_BUTTON25 = DIJOFS_BUTTON_ + 25;
  DIJOFS_BUTTON26 = DIJOFS_BUTTON_ + 26;
  DIJOFS_BUTTON27 = DIJOFS_BUTTON_ + 27;
  DIJOFS_BUTTON28 = DIJOFS_BUTTON_ + 28;
  DIJOFS_BUTTON29 = DIJOFS_BUTTON_ + 29;
  DIJOFS_BUTTON30 = DIJOFS_BUTTON_ + 30;
  DIJOFS_BUTTON31 = DIJOFS_BUTTON_ + 31;


const
  _c_dfDIJoystick_Objects: array[0..1] of TDIObjectDataFormat = (
    (  pguid: nil;
       dwOfs: 0;
       dwType: DIDFT_AXIS or DIDFT_ANYINSTANCE;
       dwFlags: 0),
    (  pguid: nil;
       dwOfs: 48;
       dwType: DIDFT_BUTTON or DIDFT_ANYINSTANCE;
       dwFlags: 0)
  );

  c_dfDIJoystick: TDIDataFormat = (
    dwSize: Sizeof(c_dfDIJoystick);
    dwObjSize: Sizeof(TDIObjectDataFormat);
    dwFlags: DIDF_ABSAXIS;
    dwDataSize: SizeOf(TDIJoyState);
    dwNumObjs: High(_c_dfDIJoystick_Objects)+1;
    rgodf: @_c_dfDIJoystick_Objects[Low(_c_dfDIJoystick_Objects)]
  );

  _c_dfDIJoystick2_Objects: array[0..1] of TDIObjectDataFormat = (
    (  pguid: nil;
       dwOfs: 0;
       dwType: DIDFT_AXIS or DIDFT_ANYINSTANCE;
       dwFlags: 0),
    (  pguid: nil;
       dwOfs: 48;
       dwType: DIDFT_BUTTON or DIDFT_ANYINSTANCE;
       dwFlags: 0)
  );

  c_dfDIJoystick2: TDIDataFormat = (
    dwSize: Sizeof(c_dfDIJoystick2);
    dwObjSize: Sizeof(TDIObjectDataFormat);
    dwFlags: DIDF_ABSAXIS;
    dwDataSize: SizeOf(TDIJoyState);
    dwNumObjs: High(_c_dfDIJoystick2_Objects)+1;
    rgodf: @_c_dfDIJoystick2_Objects[Low(_c_dfDIJoystick2_Objects)]
  );

(****************************************************************************
 *
 *  IDirectInput
 *
 ****************************************************************************)


  DIENUM_STOP = 0;
  DIENUM_CONTINUE = 1;

type
  TDIEnumDevicesCallbackA = function (const lpddi: TDIDeviceInstanceA;
      pvRef: Pointer): BOOL; stdcall;
  TDIEnumDevicesCallbackW = function (const lpddi: TDIDeviceInstanceW;
      pvRef: Pointer): BOOL; stdcall;
  TDIEnumDevicesCallback = function (const lpddi: TDIDeviceInstance;
      pvRef: Pointer): BOOL; stdcall;
  TDIEnumDevicesProc = TDIEnumDevicesCallback;

const
  DIEDFL_ALLDEVICES       = $00000000;
  DIEDFL_ATTACHEDONLY     = $00000001;
  DIEDFL_FORCEFEEDBACK    = $00000100;

type

{$IFDEF D2COM}
  IDirectInputW = class (IUnknown)
{$ELSE}
  IDirectInputW = interface (IUnknown)
    ['{89521361-AA8A-11CF-BFC7-444553540000}']
{$ENDIF}
    (*** IDirectInputW methods ***)
    function CreateDevice(rguid: PGUID; var lplpDirectInputDevice:
        IDirectInputDeviceW; pUnkOuter: IUnknown) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function EnumDevices(dwDevType: DWORD; lpCallback: TDIEnumDevicesCallbackW;
        pvRef: Pointer; dwFlags: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetDeviceStatus(rguidInstance: PGUID) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function RunControlPanel(hwndOwner: HWND; dwFlags: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function Initialize(hinst: THandle; dwVersion: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
  end;

{$IFDEF D2COM}
  IDirectInputA = class (IUnknown)
{$ELSE}
  IDirectInputA = interface (IUnknown)
    ['{89521360-AA8A-11CF-BFC7-444553540000}']
{$ENDIF}
    (*** IDirectInputA methods ***)
    function CreateDevice(rguid: PGUID; var lplpDirectInputDevice:
        IDirectInputDeviceA; pUnkOuter: IUnknown) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function EnumDevices(dwDevType: DWORD; lpCallback: TDIEnumDevicesCallbackA;
        pvRef: Pointer; dwFlags: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetDeviceStatus(rguidInstance: PGUID) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function RunControlPanel(hwndOwner: HWND; dwFlags: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function Initialize(hinst: THandle; dwVersion: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
  end;

{$IFDEF UNICODE}
  IDirectInput = IDirectInputW;
{$ELSE}
  IDirectInput = IDirectInputA;
{$ENDIF}


{$IFDEF D2COM}
  IDirectInput2W = class (IDirectInputW)
{$ELSE}
  IDirectInput2W = interface (IDirectInputW)
    ['{5944E663-AA8A-11CF-BFC7-444553540000}']
{$ENDIF}
    (*** IDirectInput2W methods ***)
    function FindDevice(Arg1: PGUID; Arg2: PWideChar; Arg3: PGUID): HRESULT;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
  end;

{$IFDEF D2COM}
  IDirectInput2A = class (IDirectInputA)
{$ELSE}
  IDirectInput2A = interface (IDirectInputA)
    ['{5944E662-AA8A-11CF-BFC7-444553540000}']
{$ENDIF}
    (*** IDirectInput2A methods ***)
    function FindDevice(Arg1: PGUID; Arg2: PAnsiChar; Arg3: PGUID): HRESULT;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
  end;

{$IFDEF UNICODE}
  IDirectInput2 = IDirectInput2W;
{$ELSE}
  IDirectInput2 = IDirectInput2A;
{$ENDIF}

function DirectInputCreateA(hinst: THandle; dwVersion: DWORD;
    var ppDI: IDirectInputA; punkOuter: IUnknown) : HResult; stdcall;
function DirectInputCreateW(hinst: THandle; dwVersion: DWORD;
    var ppDI: IDirectInputW; punkOuter: IUnknown) : HResult; stdcall;
function DirectInputCreate(hinst: THandle; dwVersion: DWORD;
    var ppDI: IDirectInput; punkOuter: IUnknown) : HResult; stdcall;


(****************************************************************************
 *
 *  Return Codes
 *
 ****************************************************************************)

(*
 *  The operation completed successfully.
 *)
const
  DI_OK = S_OK;

(*
 *  The device exists but is not currently attached.
 *)
  DI_NOTATTACHED = S_FALSE;

(*
 *  The device buffer overflowed.  Some input was lost.
 *)
  DI_BUFFEROVERFLOW = S_FALSE;

(*
 *  The change in device properties had no effect.
 *)
  DI_PROPNOEFFECT = S_FALSE;

(*
 *  The operation had no effect.
 *)
  DI_NOEFFECT = S_FALSE;

(*
 *  The device is a polled device.  As a result, device buffering
 *  will not collect any data and event notifications will not be
 *  signalled until GetDeviceState is called.
 *)
  DI_POLLEDDEVICE = $00000002;

(*
 *  The parameters of the effect were successfully updated by
 *  IDirectInputEffect::SetParameters, but the effect was not
 *  downloaded because the device is not exclusively acquired
 *  or because the DIEP_NODOWNLOAD flag was passed.
 *)
  DI_DOWNLOADSKIPPED = $00000003;

(*
 *  The parameters of the effect were successfully updated by
 *  IDirectInputEffect::SetParameters, but in order to change
 *  the parameters, the effect needed to be restarted.
 *)
  DI_EFFECTRESTARTED = $00000004;

(*
 *  The parameters of the effect were successfully updated by
 *  IDirectInputEffect::SetParameters, but some of them were
 *  beyond the capabilities of the device and were truncated.
 *)
  DI_TRUNCATED = $00000008;

(*
 *  Equal to DI_EFFECTRESTARTED | DI_TRUNCATED.
 *)
  DI_TRUNCATEDANDRESTARTED = $0000000C;

  SEVERITY_ERROR_FACILITY_WIN32 =
      (SEVERITY_ERROR shl 31) or (FACILITY_WIN32 shl 16);

(*
 *  The application requires a newer version of DirectInput.
 *)

  DIERR_OLDDIRECTINPUTVERSION = SEVERITY_ERROR_FACILITY_WIN32
      + ERROR_OLD_WIN_VERSION;

(*
 *  The application was written for an unsupported prerelease version
 *  of DirectInput.
 *)
  DIERR_BETADIRECTINPUTVERSION = SEVERITY_ERROR_FACILITY_WIN32
      + ERROR_RMODE_APP;

(*
 *  The object could not be created due to an incompatible driver version
 *  or mismatched or incomplete driver components.
 *)
  DIERR_BADDRIVERVER = SEVERITY_ERROR_FACILITY_WIN32
      + ERROR_BAD_DRIVER_LEVEL;

(*
 * The device or device instance or effect is not registered with DirectInput.
 *)
  DIERR_DEVICENOTREG = REGDB_E_CLASSNOTREG;

(*
 * The requested object does not exist.
 *)
  DIERR_NOTFOUND = SEVERITY_ERROR_FACILITY_WIN32
      + ERROR_FILE_NOT_FOUND;

(*
 * The requested object does not exist.
 *)
  DIERR_OBJECTNOTFOUND = SEVERITY_ERROR_FACILITY_WIN32
      + ERROR_FILE_NOT_FOUND;

(*
 * An invalid parameter was passed to the returning function,
 * or the object was not in a state that admitted the function
 * to be called.
 *)
  DIERR_INVALIDPARAM = E_INVALIDARG;

(*
 * The specified interface is not supported by the object
 *)
  DIERR_NOINTERFACE = E_NOINTERFACE;

(*
 * An undetermined error occured inside the DInput subsystem
 *)
  DIERR_GENERIC = E_FAIL;

(*
 * The DInput subsystem couldn't allocate sufficient memory to complete the
 * caller's request.
 *)
  DIERR_OUTOFMEMORY = E_OUTOFMEMORY;

(*
 * The function called is not supported at this time
 *)
  DIERR_UNSUPPORTED = E_NOTIMPL;

(*
 * This object has not been initialized
 *)
  DIERR_NOTINITIALIZED = SEVERITY_ERROR_FACILITY_WIN32
      + ERROR_NOT_READY;

(*
 * This object is already initialized
 *)
  DIERR_ALREADYINITIALIZED = SEVERITY_ERROR_FACILITY_WIN32
      + ERROR_ALREADY_INITIALIZED;

(*
 * This object does not support aggregation
 *)
  DIERR_NOAGGREGATION = CLASS_E_NOAGGREGATION;

(*
 * Another app has a higher priority level, preventing this call from
 * succeeding.
 *)
  DIERR_OTHERAPPHASPRIO = E_ACCESSDENIED;

(*
 * Access to the device has been lost.  It must be re-acquired.
 *)
  DIERR_INPUTLOST = SEVERITY_ERROR_FACILITY_WIN32
      + ERROR_READ_FAULT;

(*
 * The operation cannot be performed while the device is acquired.
 *)
  DIERR_ACQUIRED = SEVERITY_ERROR_FACILITY_WIN32
      + ERROR_BUSY;

(*
 * The operation cannot be performed unless the device is acquired.
 *)
  DIERR_NOTACQUIRED = SEVERITY_ERROR_FACILITY_WIN32
      + ERROR_INVALID_ACCESS;

(*
 * The specified property cannot be changed.
 *)
  DIERR_READONLY = E_ACCESSDENIED;

(*
 * The device already has an event notification associated with it.
 *)
  DIERR_HANDLEEXISTS = E_ACCESSDENIED;

(*
 * Data is not yet available.
 *)
  E_PENDING = integer($80070007);

(*
 * Unable to IDirectInputJoyConfig_Acquire because the user
 * does not have sufficient privileges to change the joystick
 * configuration.
 *)
  DIERR_INSUFFICIENTPRIVS = integer($80040200);

(*
 * The device is full.
 *)
  DIERR_DEVICEFULL = integer($80040201);

(*
 * Not all the requested information fit into the buffer.
 *)
  DIERR_MOREDATA = integer($80040202);

(*
 * The effect is not downloaded.
 *)
  DIERR_NOTDOWNLOADED = integer($80040203);

(*
 *  The device cannot be reinitialized because there are still effects
 *  attached to it.
 *)
  DIERR_HASEFFECTS = integer($80040204);

(*
 *  The operation cannot be performed unless the device is acquired
 *  in DISCL_EXCLUSIVE mode.
 *)
  DIERR_NOTEXCLUSIVEACQUIRED = integer($80040205);

(*
 *  The effect could not be downloaded because essential information
 *  is missing.  For example, no axes have been associated with the
 *  effect, or no type-specific information has been created.
 *)
  DIERR_INCOMPLETEEFFECT = integer($80040206);

(*
 *  Attempted to read buffered device data from a device that is
 *  not buffered.
 *)
  DIERR_NOTBUFFERED = integer($80040207);

(*
 *  An attempt was made to modify parameters of an effect while it is
 *  playing.  Not all hardware devices support altering the parameters
 *  of an effect while it is playing.
 *)
  DIERR_EFFECTPLAYING = integer($80040208);


(****************************************************************************
 *
 *  Definitions for non-IDirectInput (VJoyD) features defined more recently
 *  than the current sdk files
 *
 ****************************************************************************)

(*
 * Flag to indicate that the dwReserved2 field of the JOYINFOEX structure
 * contains mini-driver specific data to be passed by VJoyD to the mini-
 * driver instead of doing a poll.
 *)
  JOY_PASSDRIVERDATA          = $10000000;

(*
 * Informs the joystick driver that the configuration has been changed
 * and should be reloaded from the registery.
 * dwFlags is reserved and should be set to zero
 *)

function joyConfigChanged(dwFlags: DWORD) : MMRESULT; stdcall;

const
(*
 * Hardware Setting indicating that the device is a headtracker
 *)
  JOY_HWS_ISHEADTRACKER       = $02000000;

(*
 * Hardware Setting indicating that the VxD is used to replace
 * the standard analog polling
 *)
  JOY_HWS_ISGAMEPORTDRIVER    = $04000000;

(*
 * Hardware Setting indicating that the driver needs a standard
 * gameport in order to communicate with the device.
 *)
  JOY_HWS_ISANALOGPORTDRIVER  = $08000000;

(*
 * Hardware Setting indicating that VJoyD should not load this
 * driver, it will be loaded externally and will register with
 * VJoyD of it's own accord.
 *)
  JOY_HWS_AUTOLOAD            = $10000000;

(*
 * Hardware Setting indicating that the driver acquires any 
 * resources needed without needing a devnode through VJoyD.
 *)
  JOY_HWS_NODEVNODE           = $20000000;

(*
 * Hardware Setting indicating that the VxD can be used as
 * a port 201h emulator.
 *)
  JOY_HWS_ISGAMEPORTEMULATOR  = $40000000;


(*
 * Usage Setting indicating that the settings are volatile and
 * should be removed if still present on a reboot.
 *)
  JOY_US_VOLATILE             = $00000008;

(****************************************************************************
 *
 *  Definitions for non-IDirectInput (VJoyD) features defined more recently
 *  than the current ddk files
 *
 ****************************************************************************)

(*
 * Poll type in which the do_other field of the JOYOEMPOLLDATA
 * structure contains mini-driver specific data passed from an app.
 *)
  JOY_OEMPOLL_PASSDRIVERDATA  = 7;





implementation

function DIEFT_GETTYPE(n: variant) : byte;
begin
  Result := byte(n);
end;

function GET_DIDEVICE_TYPE(dwDevType: variant) : byte;
begin
  Result := byte(dwDevType);
end;

function GET_DIDEVICE_SUBTYPE(dwDevType: variant) : byte;
begin
  Result := hi(word(dwDevType));
end;

function DIDFT_MAKEINSTANCE(n: variant) : DWORD;
begin
  Result := n shl 8;
end;

function DIDFT_GETTYPE(n: variant) : byte;
begin
  Result := byte(n);
end;

function DIDFT_GETINSTANCE(n: variant) : DWORD;
begin
  Result := n shr 8;
end;

function DIDFT_ENUMCOLLECTION(n: word) : DWORD;
begin
  Result := n shl 8;
end;

function DIJOFS_SLIDER(n: variant) : variant;
begin
  Result := n * 4 + 24;
end;

function DIJOFS_POV(n: variant) : variant;
begin
  Result := n * 4 + 32;
end;

function DIJOFS_BUTTON(n: variant) : variant;
begin
  Result := 48 + n;
end;

function DirectInputCreateA; external 'dinput.dll';
function DirectInputCreateW; external 'dinput.dll';
{$IFDEF UNICODE}
function DirectInputCreate; external 'dinput.dll' name 'DirectInputCreateW';
{$ELSE}
function DirectInputCreate; external 'dinput.dll' name 'DirectInputCreateW';
{$ENDIF}

function joyConfigChanged(dwFlags: DWORD) : MMRESULT; external 'winmm.dll';

end.
