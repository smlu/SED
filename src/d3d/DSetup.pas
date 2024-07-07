(*==========================================================================;
 *
 *  Copyright (C) 1995-1997 Microsoft Corporation.  All Rights Reserved.
 *
 *  File:       dsetup.h
 *  Content:    DirectXSetup, error codes and flags
 *
 *  DirectX 5 Delphi adaptation by Erik Unger
 *
 *  Modyfied: 15.4.98
 *
 *  Download: http://www.sbox.tu-graz.ac.at/home/ungerik/DelphiGraphics/
 *  E-Mail: h_unger@magnet.at
 *
 ***************************************************************************)

unit DSetup;

{$INCLUDE COMSWITCH.INC}
{$INCLUDE STRINGSWITCH.INC}

interface

uses
{$IFDEF D2COM}
  OLE2,
{$ENDIF}
  Windows;

type
{$IFDEF UNICODE}
  PCharAW = PWideChar;
{$ELSE}
  PCharAW = PAnsiChar;
{$ENDIF}
  
// DSETUP Error Codes, must remain compatible with previous setup.
const
  DSETUPERR_SUCCESS_RESTART     = 1;
  DSETUPERR_SUCCESS             = 0;
  DSETUPERR_BADWINDOWSVERSION   = -1;
  DSETUPERR_SOURCEFILENOTFOUND  = -2;
  DSETUPERR_BADSOURCESIZE       = -3;
  DSETUPERR_BADSOURCETIME       = -4;
  DSETUPERR_NOCOPY              = -5;
  DSETUPERR_OUTOFDISKSPACE      = -6;
  DSETUPERR_CANTFINDINF         = -7;
  DSETUPERR_CANTFINDDIR         = -8;
  DSETUPERR_INTERNAL            = -9;
  DSETUPERR_NTWITHNO3D          = -10;  // REM: obsolete, you'll never see this 
  DSETUPERR_UNKNOWNOS           = -11;
  DSETUPERR_USERHITCANCEL       = -12;
  DSETUPERR_NOTPREINSTALLEDONNT = -13;

// DSETUP flags. DirectX 5.0 apps should use these flags only.
  DSETUP_DDRAWDRV     = $00000008;   (* install DirectDraw Drivers           *)
  DSETUP_DSOUNDDRV    = $00000010;   (* install DirectSound Drivers          *)
  DSETUP_DXCORE       = $00010000;   (* install DirectX runtime              *)
  DSETUP_DIRECTX = DSETUP_DXCORE or DSETUP_DDRAWDRV or DSETUP_DSOUNDDRV;
  DSETUP_TESTINSTALL  = $00020000;   (* just test install, don't do anything *)

// These OBSOLETE flags are here for compatibility with pre-DX5 apps only.
// They are present to allow DX3 apps to be recompiled with DX5 and still work.
// DO NOT USE THEM for DX5. They will go away in future DX releases.

  DSETUP_DDRAW         = $00000001; (* OBSOLETE. install DirectDraw           *)
  DSETUP_DSOUND        = $00000002; (* OBSOLETE. install DirectSound          *)
  DSETUP_DPLAY         = $00000004; (* OBSOLETE. install DirectPlay           *)
  DSETUP_DPLAYSP       = $00000020; (* OBSOLETE. install DirectPlay Providers *)
  DSETUP_DVIDEO        = $00000040; (* OBSOLETE. install DirectVideo          *)
  DSETUP_D3D           = $00000200; (* OBSOLETE. install Direct3D             *)
  DSETUP_DINPUT        = $00000800; (* OBSOLETE. install DirectInput          *)
  DSETUP_DIRECTXSETUP  = $00001000; (* OBSOLETE. install DirectXSetup DLL's   *)
  DSETUP_NOUI          = $00002000; (* OBSOLETE. install DirectX with NO UI   *)
  DSETUP_PROMPTFORDRIVERS = $10000000; (* OBSOLETE. prompt when replacing display/audio drivers *)
  DSETUP_RESTOREDRIVERS = $20000000;(* OBSOLETE. restore display/audio drivers *)

//******************************************************************
// DirectX Setup Callback mechanism
//******************************************************************

// DSETUP Message Info Codes, passed to callback as Reason parameter.
  DSETUP_CB_MSG_NOMESSAGE                 = 0;
  DSETUP_CB_MSG_CANTINSTALL_UNKNOWNOS     = 1;
  DSETUP_CB_MSG_CANTINSTALL_NT            = 2;
  DSETUP_CB_MSG_CANTINSTALL_BETA          = 3;
  DSETUP_CB_MSG_CANTINSTALL_NOTWIN32      = 4;
  DSETUP_CB_MSG_CANTINSTALL_WRONGLANGUAGE = 5;
  DSETUP_CB_MSG_CANTINSTALL_WRONGPLATFORM = 6;
  DSETUP_CB_MSG_PREINSTALL_NT             = 7;
  DSETUP_CB_MSG_NOTPREINSTALLEDONNT       = 8;
  DSETUP_CB_MSG_SETUP_INIT_FAILED         = 9;
  DSETUP_CB_MSG_INTERNAL_ERROR            = 10;
  DSETUP_CB_MSG_CHECK_DRIVER_UPGRADE      = 11;
  DSETUP_CB_MSG_OUTOFDISKSPACE            = 12;
  DSETUP_CB_MSG_BEGIN_INSTALL             = 13;
  DSETUP_CB_MSG_BEGIN_INSTALL_RUNTIME     = 14;
  DSETUP_CB_MSG_BEGIN_INSTALL_DRIVERS     = 15;
  DSETUP_CB_MSG_BEGIN_RESTORE_DRIVERS     = 16;
  DSETUP_CB_MSG_FILECOPYERROR             = 17;


  DSETUP_CB_UPGRADE_TYPE_MASK      = $000F;
  DSETUP_CB_UPGRADE_KEEP           = $0001;
  DSETUP_CB_UPGRADE_SAFE           = $0002;
  DSETUP_CB_UPGRADE_FORCE          = $0004;
  DSETUP_CB_UPGRADE_UNKNOWN        = $0008;

  DSETUP_CB_UPGRADE_HASWARNINGS    = $0100;
  DSETUP_CB_UPGRADE_CANTBACKUP     = $0200;

  DSETUP_CB_UPGRADE_DEVICE_ACTIVE  = $0800;

  DSETUP_CB_UPGRADE_DEVICE_DISPLAY = $1000;
  DSETUP_CB_UPGRADE_DEVICE_MEDIA   = $2000;


type
  PDSetup_CB_UpgradeInfo = ^TDSetup_CB_UpgradeInfo;
  TDSetup_CB_UpgradeInfo = record
    UpgradeFlags: DWORD;
  end;

  PDSetup_CB_FileCopyError = ^TDSetup_CB_FileCopyError;
  TDSetup_CB_FileCopyError = record
    dwError: DWORD;
  end;

//
// Data Structures
//
  PDirectXRegisterAppA = ^TDirectXRegisterAppA;
  TDirectXRegisterAppA = record
    dwSize: DWORD;
    dwFlags: DWORD;
    lpszApplicationName: PAnsiChar;
    lpGUID: PGUID;
    lpszFilename: PAnsiChar;
    lpszCommandLine: PAnsiChar;
    lpszPath: PAnsiChar;
    lpszCurrentDirectory: PAnsiChar;
  end;

  PDirectXRegisterAppW = ^TDirectXRegisterAppW;
  TDirectXRegisterAppW = record
    dwSize: DWORD;
    dwFlags: DWORD;
    lpszApplicationName: PWideChar;
    lpGUID: PGUID;
    lpszFilename: PWideChar;
    lpszCommandLine: PWideChar;
    lpszPath: PWideChar;
    lpszCurrentDirectory: PWideChar;
  end;


  PDirectXRegisterApp = ^TDirectXRegisterApp;
{$IFDEF UNICODE}
  TDirectXRegisterApp = TDirectXRegisterAppW;
{$ELSE}
  TDirectXRegisterApp = TDirectXRegisterAppA;
{$ENDIF}

//
// API
//
function DirectXSetupW(hWnd: HWND; lpszRootPath: PWideChar; dwFlags: DWORD) :
    Integer; stdcall;
function DirectXSetupA(hWnd: HWND; lpszRootPath: PAnsiChar; dwFlags: DWORD) :
    Integer; stdcall;
function DirectXSetup(hWnd: HWND; lpszRootPath: PCharAW; dwFlags: DWORD) :
    Integer; stdcall;

function DirectXDeviceDriverSetupW(hWnd: HWND; lpszDriverClass: PWideChar;
    lpszDriverPath: PWideChar; dwFlags: DWORD) : Integer; stdcall;
function DirectXDeviceDriverSetupA(hWnd: HWND; lpszDriverClass: PAnsiChar;
    lpszDriverPath: PAnsiChar; dwFlags: DWORD) : Integer; stdcall;
function DirectXDeviceDriverSetup(hWnd: HWND; lpszDriverClass: PCharAW;
    lpszDriverPath: PCharAW; dwFlags: DWORD) : Integer; stdcall;

function DirectXRegisterApplicationW(hWnd: HWND;
    var lpDXRegApp: TDirectXRegisterAppW) : Integer; stdcall;
function DirectXRegisterApplicationA(hWnd: HWND;
    var lpDXRegApp: TDirectXRegisterAppA) : Integer; stdcall;
function DirectXRegisterApplication(hWnd: HWND;
    var lpDXRegApp: TDirectXRegisterApp) : Integer; stdcall;

function DirectXUnRegisterApplication(hWnd: HWND; const lpGUID: TGUID) :
    Integer; stdcall;

type
  TDSetup_Callback = function (Reason: DWORD; MsgType: DWORD; // Same as flags to MessageBox 
      szMessage: PAnsiChar; szName: PAnsiChar; pInfo: Pointer) : DWORD; stdcall;

function DirectXSetupSetCallback(Callback: TDSetup_Callback) : Integer; stdcall;

function DirectXSetupGetVersion(var lpdwVersion: DWORD;
    var lpdwMinorVersion: DWORD) : Integer; stdcall;

implementation

function DirectXSetupA; external 'DSetup.dll';
function DirectXSetupW; external 'DSetup.dll';
{$IFDEF UNICODE}
function DirectXSetup; external 'DSetup.dll' name 'DirectXSetupW';
{$ELSE}
function DirectXSetup; external 'DSetup.dll' name 'DirectXSetupA';
{$ENDIF}

function DirectXDeviceDriverSetupA; external 'DSetup.dll';
function DirectXDeviceDriverSetupW; external 'DSetup.dll';
{$IFDEF UNICODE}
function DirectXDeviceDriverSetup;
    external 'DSetup.dll' name 'DirectXDeviceDriverSetupW';
{$ELSE}
function DirectXDeviceDriverSetup;
    external 'DSetup.dll' name 'DirectXDeviceDriverSetupA';
{$ENDIF}

function DirectXRegisterApplicationA; external 'DSetup.dll';
function DirectXRegisterApplicationW; external 'DSetup.dll';
{$IFDEF UNICODE}
function DirectXRegisterApplication;
    external 'DSetup.dll' name 'DirectXRegisterApplicationW';
{$ELSE}
function DirectXRegisterApplication;
    external 'DSetup.dll' name 'DirectXRegisterApplicationA';
{$ENDIF}

function DirectXUnRegisterApplication; external 'DSetup.dll';

function DirectXSetupSetCallback; external 'DSetup.dll';

function DirectXSetupGetVersion; external 'DSetup.dll';

end.
 