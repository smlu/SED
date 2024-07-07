(*==========================================================================;
 *
 *  Copyright (C) Microsoft Corporation.  All Rights Reserved.
 *
 *  File:       dplay.h
 *  Content:    DirectPlay include file
 *
 *  DirectX 5 Delphi adaptation by Erik Unger
 *
 *  Modyfied: 17.4.98
 *
 *  Download: http://www.sbox.tu-graz.ac.at/home/ungerik/DelphiGraphics/
 *  E-Mail: h_unger@magnet.at
 *
 ***************************************************************************)

unit DPLobby;

{$INCLUDE COMSWITCH.INC}
{$INCLUDE STRINGSWITCH.INC}

interface

uses
{$IFDEF D2COM}
  OLE2,
{$ENDIF}
  Windows,
  DPlay;

(*
 * GUIDS used by DirectPlay objects
 *)

const
(* {AF465C71-9588-11cf-A020-00AA006157AC} *)
  IID_IDirectPlayLobbyW: TGUID =
      (D1:$af465c71;D2:$9588;D3:$11cf;D4:($a0,$20,$0,$aa,$0,$61,$57,$ac));
(* {26C66A70-B367-11cf-A024-00AA006157AC} *)
  IID_IDirectPlayLobbyA: TGUID =
      (D1:$26c66a70;D2:$b367;D3:$11cf;D4:($a0,$24,$0,$aa,$0,$61,$57,$ac));
{$IFDEF UNICODE}
  IID_IDirectPlayLobby: TGUID =
      (D1:$af465c71;D2:$9588;D3:$11cf;D4:($a0,$20,$0,$aa,$0,$61,$57,$ac));
{$ELSE}
  IID_IDirectPlayLobby: TGUID =
      (D1:$26c66a70;D2:$b367;D3:$11cf;D4:($a0,$24,$0,$aa,$0,$61,$57,$ac));
{$ENDIF}


(* {0194C220-A303-11d0-9C4F-00A0C905425E} *)
  IID_IDirectPlayLobby2W: TGUID =
      (D1:$194c220;D2:$a303;D3:$11d0;D4:($9c,$4f,$0,$a0,$c9,$5,$42,$5e));
(* {1BB4AF80-A303-11d0-9C4F-00A0C905425E} *)
  IID_IDirectPlayLobby2A: TGUID =
      (D1:$1bb4af80;D2:$a303;D3:$11d0;D4:($9c,$4f,$0,$a0,$c9,$5,$42,$5e));
{$IFDEF UNICODE}
  IID_IDirectPlayLobby2: TGUID =
      (D1:$194c220;D2:$a303;D3:$11d0;D4:($9c,$4f,$0,$a0,$c9,$5,$42,$5e));
{$ELSE}
  IID_IDirectPlayLobby2: TGUID =
      (D1:$1bb4af80;D2:$a303;D3:$11d0;D4:($9c,$4f,$0,$a0,$c9,$5,$42,$5e));
{$ENDIF}

(* {2FE8F810-B2A5-11d0-A787-0000F803ABFC} *)
  CLSID_DirectPlayLobby: TGUID =
      (D1:$2fe8f810;D2:$b2a5;D3:$11d0;D4:($a7,$87,$0,$0,$f8,$3,$ab,$fc));

(****************************************************************************
 *
 * IDirectPlayLobby Structures
 *
 * Various structures used to invoke DirectPlayLobby.
 *
 ****************************************************************************)

type
(*
 * TDPLAppInfo
 * Used to hold information about a registered DirectPlay
 * application
 *)
  PDPLAppInfo = ^TDPLAppInfo;
  TDPLAppInfo = packed record
    dwSize: DWORD;            // Size of this structure
    guidApplication: TGUID;   // GUID of the Application
    case Integer of           // Pointer to the Application Name
      0: (lpszAppName: PCharAW);
      1: (lpszAppNameW: PWideChar);
      3: (lpszAppNameA: PAnsiChar);
  end;

(*
 * TDPCompoundAddressElement
 *
 * An array of these is passed to CreateCompoundAddresses()
 *)
  PDPCompoundAddressElement = ^TDPCompoundAddressElement;
  TDPCompoundAddressElement = packed record
    guidDataType: TGUID;
    dwDataSize: DWORD;
    lpData: Pointer;
  end;

(****************************************************************************
 *
 * Enumeration Method Callback Prototypes
 *
 ****************************************************************************)

(*
 * Callback for EnumAddress()
 *)
  TDPEnumAdressCallback = function(const guidDataType: TGUID;
      dwDataSize: DWORD; lpData: Pointer; lpContext: Pointer) : BOOL; stdcall;

(*
 * Callback for EnumAddressTypes()
 *)
  TDPLEnumAddressTypesCallback = function(const guidDataType: TGUID;
      lpContext: Pointer; dwFlags: DWORD) : BOOL; stdcall;

(*
 * Callback for EnumLocalApplications()
 *)
  TDPLEnumLocalApplicationsCallback = function(const lpAppInfo: TDPLAppInfo;
      lpContext: Pointer; dwFlags: DWORD) : BOOL; stdcall;

(****************************************************************************
 *
 * IDirectPlayLobby (and IDirectPlayLobbyA) Interface
 *
 ****************************************************************************)

type
{$IFDEF D2COM}
  IDirectPlayLobbyAW = class (IUnknown)
{$ELSE}
  IDirectPlayLobbyAW = interface (IUnknown)
{$ENDIF}
    (*** IDirectPlayLobby methods ***)
    function Connect(dwFlags: DWORD; const lplpDP: IDirectPlay2;
        pUnk: IUnknown) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function CreateAddress(const guidSP, guidDataType: TGUID; const lpData;
        dwDataSize: DWORD; var lpAddress; var lpdwAddressSize: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function EnumAddress(lpEnumAddressCallback: TDPEnumAdressCallback;
        const lpAddress; dwAddressSize: DWORD; lpContext : Pointer) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function EnumAddressTypes(lpEnumAddressTypeCallback:
        TDPLEnumAddressTypesCallback; const guidSP: TGUID; lpContext: Pointer;
        dwFlags: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function EnumLocalApplications(lpEnumLocalAppCallback:
        TDPLEnumLocalApplicationsCallback; lpContext: Pointer; dwFlags: DWORD)
        : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetConnectionSettings(dwAppID: DWORD; lpData: PDPLConnection;
        var lpdwDataSize: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function ReceiveLobbyMessage(dwFlags: DWORD; dwAppID: DWORD;
        var lpdwMessageFlags: DWORD; var lpData; var lpdwDataSize: DWORD) :
        HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function RunApplication(dwFlags: DWORD; var lpdwAppId: DWORD;
        const lpConn: TDPLConnection; hReceiveEvent: THandle) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SendLobbyMessage(dwFlags: DWORD; dwAppID: DWORD; const lpData;
        dwDataSize: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetConnectionSettings(dwFlags: DWORD; dwAppID: DWORD;
        const lpConn: TDPLConnection) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetLobbyMessageEvent(dwFlags: DWORD; dwAppID: DWORD;
        hReceiveEvent: THandle) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
  end;

{$IFDEF D2COM}
  IDirectPlayLobbyA = IDirectPlayLobbyAW;
  IDirectPlayLobbyW = IDirectPlayLobbyAW;
{$ELSE}
  IDirectPlayLobbyW = interface (IDirectPlayLobbyAW)
    ['{AF465C71-9588-11CF-A020-00AA006157AC}']
  end;
  IDirectPlayLobbyA = interface (IDirectPlayLobbyAW)
    ['{26C66A70-B367-11cf-A024-00AA006157AC}']
  end;
{$ENDIF}

{$IFDEF UNICODE}
  IDirectPlayLobby = IDirectPlayLobbyW;
{$ELSE}
  IDirectPlayLobby = IDirectPlayLobbyA;
{$ENDIF}


(****************************************************************************
 *
 * IDirectPlayLobby2 (and IDirectPlayLobby2A) Interface
 *
 ****************************************************************************)

{$IFDEF D2COM}
  IDirectPlayLobby2AW = class (IDirectPlayLobbyAW)
{$ELSE}
  IDirectPlayLobby2AW = interface(IDirectPlayLobbyAW)
{$ENDIF}
    (*** IDirectPlayLobby2 methods ***)
    function CreateCompoundAddress(const lpElements: TDPCompoundAddressElement;
        dwElementCount: DWORD; var lpAddress; var lpdwAddressSize: DWORD) :
        HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
  end;

{$IFDEF D2COM}
  IDirectPlayLobby2A = IDirectPlayLobby2AW;
  IDirectPlayLobby2W = IDirectPlayLobby2AW;
{$ELSE}
  IDirectPlayLobby2W = interface (IDirectPlayLobby2AW)
    ['{0194C220-A303-11D0-9C4F-00A0C905425E}']
  end;
  IDirectPlayLobby2A = interface (IDirectPlayLobby2AW)
    ['{1BB4AF80-A303-11d0-9C4F-00A0C905425E}']
  end;
{$ENDIF}

{$IFDEF UNICODE}
  IDirectPlayLobby2 = IDirectPlayLobby2W;
{$ELSE}
  IDirectPlayLobby2 = IDirectPlayLobby2A;
{$ENDIF}

(****************************************************************************
 *
 * DirectPlayLobby API Prototypes
 *
 ****************************************************************************)

function DirectPlayLobbyCreateW(lpguidSP: PGUID; var lplpDPL:
    IDirectPlayLobbyW; lpUnk: IUnknown; lpData: Pointer; dwDataSize: DWORD) :
    HResult; stdcall;
function DirectPlayLobbyCreateA(lpguidSP: PGUID; var lplpDPL:
    IDirectPlayLobbyA; lpUnk: IUnknown; lpData: Pointer; dwDataSize: DWORD) :
    HResult; stdcall;
function DirectPlayLobbyCreate(lpguidSP: PGUID; var lplpDPL:
    IDirectPlayLobby; lpUnk: IUnknown; lpData: Pointer; dwDataSize: DWORD) :
    HResult; stdcall;

const
(****************************************************************************
 *
 * DirectPlayLobby Flags
 *
 ****************************************************************************)

(*
 *	This is a message flag used by ReceiveLobbyMessage.  It can be
 *	returned in the dwMessageFlags parameter to indicate a message from
 *	the system.
 *)
  DPLMSG_SYSTEM = $00000001;

(*
 *	This is a message flag used by ReceiveLobbyMessage and SendLobbyMessage.
 *  It is used to indicate that the message is a standard lobby message.
 *  TDPLMsg_SetProperty, TDPLMsg_SetPropertyResponse, TDPLMsg_GetProperty,
 *	TDPLMsg_GetPropertyResponse
 *)
  DPLMSG_STANDARD = $00000002;

type
(****************************************************************************
 *
 * DirectPlayLobby messages and message data structures
 *
 * All system messages have a dwMessageFlags value of DPLMSG_SYSTEM returned
 * from a call to ReceiveLobbyMessage.
 *
 * All standard messages have a dwMessageFlags value of DPLMSG_STANDARD returned
 * from a call to ReceiveLobbyMessage.
 *
 ****************************************************************************)

(*
 * TDPLMsg_Generic
 * Generic message structure used to identify the message type.
 *)
  PDPLMsg_Generic = ^TDPLMsg_Generic;
  TDPLMsg_Generic = packed record
    dwType: DWORD;   // Message type
  end;

(*
 *  TDPLMsg_SetProperty
 *  Standard message sent by an application to a lobby to set a
 *  property
 *)
  PDPLMsg_SetProperty = ^TDPLMsg_SetProperty;
  TDPLMsg_SetProperty = packed record
    dwType: DWORD;                           // Message type
    dwRequestID: DWORD;                      // Request ID (DPL_NOCONFIRMATION if no confirmation desired)
    guidPlayer: TGUID;                       // Player GUID
    guidPropertyTag: TGUID;                  // Property GUID
    dwDataSize: DWORD;                       // Size of data
    dwPropertyData: array[0..0] of DWORD;    // Buffer containing data
  end;

const
  DPL_NOCONFIRMATION = 0;

type
(*
 *  TDPLMsg_SetPropertyResponse
 *  Standard message returned by a lobby to confirm a 
 *  TDPLMsg_SetProperty message.
 *)
  PDPLMsg_SetPropertyResponse = ^TDPLMsg_SetPropertyResponse;
  TDPLMsg_SetPropertyResponse = packed record
    dwType: DWORD;            // Message type
    dwRequestID: DWORD;       // Request ID
    guidPlayer: TGUID;        // Player GUID
    guidPropertyTag: TGUID;   // Property GUID
    hr: HResult;              // Return Code
  end;

(*
 *  TDPLMsg_GetProperty
 *  Standard message sent by an application to a lobby to request
 *	the current value of a property
 *)
  PDPLMsg_GetProperty = ^TDPLMsg_GetProperty;
  TDPLMsg_GetProperty = packed record
    dwType: DWORD;            // Message type
    dwRequestID: DWORD;       // Request ID
    guidPlayer: TGUID;        // Player GUID
    guidPropertyTag: TGUID;   // Property GUID
  end;
  LPDPLMSG_GETPROPERTY = ^TDPLMsg_GetProperty;

(*
 *  TDPLMsg_GetPropertyResponse
 *  Standard message returned by a lobby in response to a
 *	TDPLMsg_GetProperty message.
 *)
  PDPLMsg_GetPropertyResponse = ^TDPLMsg_GetPropertyResponse;
  TDPLMsg_GetPropertyResponse = packed record
    dwType: DWORD;                           // Message type
    dwRequestID: DWORD;                      // Request ID
    guidPlayer: TGUID;                       // Player GUID
    guidPropertyTag: TGUID;                  // Property GUID
    hr: HResult;                             // Return Code
    dwDataSize: DWORD;                       // Size of data
    dwPropertyData: array[0..0] of DWORD;    // Buffer containing data
  end;

const
(******************************************
 *
 *	DirectPlay Lobby message dwType values
 *
 *****************************************)

(*
 *  The application has read the connection settings.
 *  It is now O.K. for the lobby client to release
 *  its IDirectPlayLobby interface.
 *)
  DPLSYS_CONNECTIONSETTINGSREAD = $00000001;

(*
 *  The application's call to DirectPlayConnect failed
 *)
  DPLSYS_DPLAYCONNECTFAILED = $00000002;

(*
 *  The application has created a DirectPlay session.
 *)
  DPLSYS_DPLAYCONNECTSUCCEEDED = $00000003;

(*
 *  The application has terminated.
 *)
  DPLSYS_APPTERMINATED = $00000004;

(*
 *  The message is a TDPLMsg_SetProperty message.
 *)
  DPLSYS_SETPROPERTY = $00000005;

(*
 *  The message is a TDPLMsg_SetPropertyResponse message.
 *)
  DPLSYS_SETPROPERTYRESPONSE = $00000006;

(*
 *  The message is a TDPLMsg_GetProperty message.
 *)
  DPLSYS_GETPROPERTY = $00000007;

(*
 *  The message is a TDPLMsg_GetPropertyResponse message.
 *)
  DPLSYS_GETPROPERTYRESPONSE = $00000008;

(****************************************************************************
 *
 * DirectPlay defined property GUIDs and associated data structures
 *
 ****************************************************************************)

(*
 * DPLPROPERTY_MessagesSupported
 *
 * Request whether the lobby supports standard.  Lobby with respond with either
 * TRUE or FALSE or may not respond at all.
 * 
 * Property data is a single BOOL with TRUE or FALSE
 *)
// {762CCDA1-D916-11d0-BA39-00C04FD7ED67}
  DPLPROPERTY_MessagesSupported: TGUID =
      (D1:$762ccda1;D2:$d916;D3:$11d0;D4:($ba,$39,$00,$c0,$4f,$d7,$ed,$67));

(*
 * DPLPROPERTY_LobbyGuid
 *
 * Request the GUID that identifies the lobby software that the application
 * is communicating with.
 *
 * Property data is a single GUID.
 *)
// {F56920A0-D218-11d0-BA39-00C04FD7ED67}
  DPLPROPERTY_LobbyGuid: TGUID =
      (D1:$F56920A0;D2:$D218;D3:$11d0;D4:($ba,$39,$00,$c0,$4f,$d7,$ed,$67));

(*
 * DPLPROPERTY_PlayerGuid
 *
 * Request the GUID that identifies the player on this machine for sending
 * property data back to the lobby.
 *
 * Property data is the DPLDATA_PLAYERDATA structure
 *)
// {B4319322-D20D-11d0-BA39-00C04FD7ED67}
  DPLPROPERTY_PlayerGuid: TGUID =
      (D1:$b4319322;D2:$d20d;D3:$11d0;D4:($ba,$39,$00,$c0,$4f,$d7,$ed,$67));

type
(*
 * TDPLData_PlayerGUID
 *
 * Data structure to hold the GUID of the player and player creation flags
 * from the lobby.
 *)
  PDPLData_PlayerGUID = ^TDPLData_PlayerGUID;
  TDPLData_PlayerGUID = packed record
    guidPlayer: TGUID;
    dwPlayerFlags: DWORD;
  end;

const
(*
 * DPLPROPERTY_PlayerScore
 *
 * Used to send an array of long integers to the lobby indicating the 
 * score of a player.
 *
 * Property data is the TDPLData_PlayerScore structure.
 *)
// {48784000-D219-11d0-BA39-00C04FD7ED67}
  DPLPROPERTY_PlayerScore: TGUID =
      (D1:$48784000;D2:$d219;D3:$11d0;D4:($ba,$39,$00,$c0,$4f,$d7,$ed,$67));

type
(*
 * TDPLData_PlayerScore
 *
 * Data structure to hold an array of long integers representing a player score.
 * Application must allocate enough memory to hold all the scores.
 *)
  PDPLData_PlayerScore = ^TDPLData_PlayerScore;
  TDPLData_PlayerScore = packed record
    dwScoreCount: DWORD;
    Score: array[0..0] of Longint;
  end;

(****************************************************************************
 *
 * DirectPlay Address ID's
 *
 ****************************************************************************)

(* DirectPlay Address
 *
 * A DirectPlay address consists of multiple chunks of data, each tagged
 * with a GUID signifying the type of data in the chunk. The chunk also
 * has a length so that unknown chunk types can be skipped.
 *
 * The EnumAddress() function is used to parse these address data chunks.
 *)

(*
 * TDPAddress
 *
 * Header for block of address data elements
 *)
  PDPAddress = ^TDPAddress;
  TDPAddress = packed record
    guidDataType: TGUID;
    dwDataSize: DWORD;
  end;

const
(*
 * DPAID_TotalSize
 *
 * Chunk is a DWORD containing size of entire TDPAddress structure
 *)

// {1318F560-912C-11d0-9DAA-00A0C90A43CB}
  DPAID_TotalSize: TGUID =
      (D1:$1318f560;D2:$912c;D3:$11d0;D4:($9d,$aa,$0,$a0,$c9,$a,$43,$cb));

(*
 * DPAID_ServiceProvider
 *
 * Chunk is a GUID describing the service provider that created the chunk.
 * All addresses must contain this chunk.
 *)

// {07D916C0-E0AF-11cf-9C4E-00A0C905425E}
  DPAID_ServiceProvider: TGUID =
      (D1:$7d916c0;D2:$e0af;D3:$11cf;D4:($9c,$4e,$0,$a0,$c9,$5,$42,$5e));

(*
 * DPAID_LobbyProvider
 *
 * Chunk is a GUID describing the lobby provider that created the chunk.
 * All addresses must contain this chunk.
 *)

// {59B95640-9667-11d0-A77D-0000F803ABFC}
  DPAID_LobbyProvider: TGUID =
      (D1:$59b95640;D2:$9667;D3:$11d0;D4:($a7,$7d,$0,$0,$f8,$3,$ab,$fc));

(*
 * DPAID_Phone and DPAID_PhoneW
 *
 * Chunk is a string containing a phone number (i.e. "1-800-555-1212")
 * in ANSI or UNICODE format
 *)

// {78EC89A0-E0AF-11cf-9C4E-00A0C905425E}
  DPAID_Phone: TGUID =
      (D1:$78ec89a0;D2:$e0af;D3:$11cf;D4:($9c,$4e,$0,$a0,$c9,$5,$42,$5e));

// {BA5A7A70-9DBF-11d0-9CC1-00A0C905425E}
  DPAID_PhoneW: TGUID =
      (D1:$ba5a7a70;D2:$9dbf;D3:$11d0;D4:($9c,$c1,$0,$a0,$c9,$5,$42,$5e));

(*
 * DPAID_Modem and DPAID_ModemW
 *
 * Chunk is a string containing a modem name registered with TAPI
 * in ANSI or UNICODE format
 *)

// {F6DCC200-A2FE-11d0-9C4F-00A0C905425E}
  DPAID_Modem: TGUID =
      (D1:$f6dcc200;D2:$a2fe;D3:$11d0;D4:($9c,$4f,$0,$a0,$c9,$5,$42,$5e));

// {01FD92E0-A2FF-11d0-9C4F-00A0C905425E}
  DPAID_ModemW: TGUID =
      (D1:$1fd92e0;D2:$a2ff;D3:$11d0;D4:($9c,$4f,$0,$a0,$c9,$5,$42,$5e));

(*
 * DPAID_Inet and DPAID_InetW
 *
 * Chunk is a string containing a TCP/IP host name or an IP address
 * (i.e. "dplay.microsoft.com" or "137.55.100.173") in ANSI or UNICODE format
 *)

// {C4A54DA0-E0AF-11cf-9C4E-00A0C905425E}
  DPAID_INet: TGUID =
      (D1:$c4a54da0;D2:$e0af;D3:$11cf;D4:($9c,$4e,$0,$a0,$c9,$5,$42,$5e));

// {E63232A0-9DBF-11d0-9CC1-00A0C905425E}
  DPAID_INetW: TGUID =
      (D1:$e63232a0;D2:$9dbf;D3:$11d0;D4:($9c,$c1,$0,$a0,$c9,$5,$42,$5e));

(*
 * TDPComPortAddress
 *
 * Used to specify com port settings. The constants that define baud rate,
 * stop bits and parity are defined in WINBASE.H. The constants for flow
 * control are given below.
 *)

  DPCPA_NOFLOW       = 0;           // no flow control
  DPCPA_XONXOFFFLOW  = 1;           // software flow control
  DPCPA_RTSFLOW      = 2;           // hardware flow control with RTS
  DPCPA_DTRFLOW      = 3;           // hardware flow control with DTR
  DPCPA_RTSDTRFLOW   = 4;           // hardware flow control with RTS and DTR

type
  PDPComPortAddress = ^TDPComPortAddress;
  TDPComPortAddress = packed record
    dwComPort: DWORD;       // COM port to use (1-4)
    dwBaudRate: DWORD;      // baud rate (100-256k)
    dwStopBits: DWORD;      // no. stop bits (1-2)
    dwParity: DWORD;        // parity (none, odd, even, mark)
    dwFlowControl: DWORD;   // flow control (none, xon/xoff, rts, dtr)
  end;

const
(*
 * DPAID_ComPort
 *
 * Chunk contains a TDPComPortAddress structure defining the serial port.
 *)

// {F2F0CE00-E0AF-11cf-9C4E-00A0C905425E}
  DPAID_ComPort: TGUID =
      (D1:$f2f0ce00;D2:$e0af;D3:$11cf;D4:($9c,$4e,$0,$a0,$c9,$5,$42,$5e));

(****************************************************************************
 *
 * 	dplobby 1.0 obsolete definitions
 *	Included for compatibility only.
 *
 ****************************************************************************)

  DPLAD_SYSTEM = DPLMSG_SYSTEM;


implementation

function DirectPlayLobbyCreateW; external 'DPlayX.dll';
function DirectPlayLobbyCreateA; external 'DPlayX.dll';
{$IFDEF UNICODE}
function DirectPlayLobbyCreate;
    external 'DPlayX.dll' name 'DirectPlayLobbyCreateW';
{$ELSE}
function DirectPlayLobbyCreate;
    external 'DPlayX.dll' name 'DirectPlayLobbyCreateA';
{$ENDIF}

end.
