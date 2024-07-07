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

unit DPlay;

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

const
  _FACDP = $877;
function MAKE_DPHResult(code: variant) : HResult;

const
(*
 * GUIDS used by DirectPlay objects
 *)
  IID_IDirectPlay2W: TGUID =
      (D1:$2b74f7c0;D2:$9154;D3:$11cf;D4:($a9,$cd,$00,$aa,$00,$68,$86,$e3));
  IID_IDirectPlay2A: TGUID =
      (D1:$9d460580;D2:$a822;D3:$11cf;D4:($96,$0c,$00,$80,$c7,$53,$4e,$82));
{$IFDEF UNICODE}
  IID_IDirectPlay2: TGUID =
      (D1:$2b74f7c0;D2:$9154;D3:$11cf;D4:($a9,$cd,$00,$aa,$00,$68,$86,$e3));
{$ELSE}
  IID_IDirectPlay2: TGUID =
      (D1:$9d460580;D2:$a822;D3:$11cf;D4:($96,$0c,$00,$80,$c7,$53,$4e,$82));
{$ENDIF}

  IID_IDirectPlay3W: TGUID =
      (D1:$133efe40;D2:$32dc;D3:$11d0;D4:($9c,$fb,$00,$a0,$c9,$0a,$43,$cb));
  IID_IDirectPlay3A: TGUID =
      (D1:$133efe41;D2:$32dc;D3:$11d0;D4:($9c,$fb,$00,$a0,$c9,$0a,$43,$cb));
{$IFDEF UNICODE}
  IID_IDirectPlay3: TGUID =
      (D1:$133efe40;D2:$32dc;D3:$11d0;D4:($9c,$fb,$00,$a0,$c9,$0a,$43,$cb));
{$ELSE}
  IID_IDirectPlay3: TGUID =
      (D1:$133efe41;D2:$32dc;D3:$11d0;D4:($9c,$fb,$00,$a0,$c9,$0a,$43,$cb));
{$ENDIF}

// {D1EB6D20-8923-11d0-9D97-00A0C90A43CB}
  CLSID_DirectPlay: TGUID =
      (D1:$d1eb6d20;D2:$8923;D3:$11d0;D4:($9d,$97,$00,$a0,$c9,$a,$43,$cb));
  IID_IDirectPlay: TGUID =
      (D1:$5454e9a0;D2:$db65;D3:$11ce;D4:($92,$1c,$00,$aa,$00,$6c,$49,$72));

(*
 * GUIDS used by Service Providers shipped with DirectPlay
 * Use these to identify Service Provider returned by EnumConnections
 *)

// GUID for IPX service provider
// {685BC400-9D2C-11cf-A9CD-00AA006886E3}
  DPSPGUID_IPX: TGUID =
      (D1:$685bc400;D2:$9d2c;D3:$11cf;D4:($a9,$cd,$00,$aa,$00,$68,$86,$e3));

// GUID for TCP/IP service provider
// 36E95EE0-8577-11cf-960C-0080C7534E82
  DPSPGUID_TCPIP: TGUID =
      (D1:$36E95EE0;D2:$8577;D3:$11cf;D4:($96,$0c,$00,$80,$c7,$53,$4e,$82));

// GUID for Serial service provider
// {0F1D6860-88D9-11cf-9C4E-00A0C905425E}
  DPSPGUID_SERIAL: TGUID =
      (D1:$f1d6860;D2:$88d9;D3:$11cf;D4:($9c,$4e,$00,$a0,$c9,$05,$42,$5e));

// GUID for Modem service provider
// {44EAA760-CB68-11cf-9C4E-00A0C905425E}
  DPSPGUID_MODEM: TGUID =
      (D1:$44eaa760;D2:$cb68;D3:$11cf;D4:($9c,$4e,$00,$a0,$c9,$05,$42,$5e));


(****************************************************************************
 *
 * DirectPlay Structures
 *
 * Various structures used to invoke DirectPlay.
 *
 ****************************************************************************)

type
(*
 * DPID
 * DirectPlay player and group ID
 *)
  TDPID = DWORD;
  PDPID = ^TDPID;


const
(*
 * DPID that system messages come from
 *)
  DPID_SYSMSG = 0;

(*
 * DPID representing all players in the session
 *)
  DPID_ALLPLAYERS = 0;

(*
 * DPID representing the server player
 *)
  DPID_SERVERPLAYER = 1;

(*
 * The player ID is unknown (used with e.g. DPSESSION_NOMESSAGEID)
 *)
  DPID_UNKNOWN = $FFFFFFFF;

type
(*
 * DPCAPS
 * Used to obtain the capabilities of a DirectPlay object
 *)
  PDPCaps = ^TDPCaps;
  TDPCaps = packed record
    dwSize: DWORD;              // Size of structure, in bytes
    dwFlags: DWORD;             // DPCAPS_xxx flags
    dwMaxBufferSize: DWORD;     // Maximum message size, in bytes,  for this service provider
    dwMaxQueueSize: DWORD;      // Obsolete.
    dwMaxPlayers: DWORD;        // Maximum players/groups (local + remote)
    dwHundredBaud: DWORD;       // Bandwidth in 100 bits per second units;
                                // i.e. 24 is 2400, 96 is 9600, etc.
    dwLatency: DWORD;           // Estimated latency; 0 = unknown
    dwMaxLocalPlayers: DWORD;   // Maximum # of locally created players allowed
    dwHeaderLength: DWORD;      // Maximum header length, in bytes, on messages
                                // added by the service provider
    dwTimeout: DWORD;           // Service provider's suggested timeout value
                                // This is how long DirectPlay will wait for
                                // responses to system messages
  end;

const
(*
 * This DirectPlay object is the session host.  If the host exits the
 * session, another application will become the host and receive a
 * DPSYS_HOST system message.
 *)
  DPCAPS_ISHOST = $00000002;

(*
 * The service provider bound to this DirectPlay object can optimize
 * group messaging.
 *)
  DPCAPS_GROUPOPTIMIZED = $00000008;

(*
 * The service provider bound to this DirectPlay object can optimize
 * keep alives (see DPSESSION_KEEPALIVE)
 *)
  DPCAPS_KEEPALIVEOPTIMIZED = $00000010;

(*
 * The service provider bound to this DirectPlay object can optimize
 * guaranteed message delivery.
 *)
  DPCAPS_GUARANTEEDOPTIMIZED = $00000020;

(*
 * This DirectPlay object supports guaranteed message delivery.
 *)
  DPCAPS_GUARANTEEDSUPPORTED = $00000040;

(*
 * This DirectPlay object supports digital signing of messages.
 *)
  DPCAPS_SIGNINGSUPPORTED = $00000080;

(*
 * This DirectPlay object supports encryption of messages.
 *)
  DPCAPS_ENCRYPTIONSUPPORTED = $00000100;

type
(*
 * TDPSessionDesc2
 * Used to describe the properties of a DirectPlay
 * session instance
 *)
  PDPSessionDesc2 = ^TDPSessionDesc2;
  TDPSessionDesc2 = packed record
    dwSize: DWORD;             // Size of structure
    dwFlags: DWORD;            // DPSESSION_xxx flags
    guidInstance: TGUID;       // ID for the session instance
    guidApplication: TGUID;    // GUID of the DirectPlay application.
                               // GUID_NULL for all applications.
    dwMaxPlayers: DWORD;       // Maximum # players allowed in session
    dwCurrentPlayers: DWORD;   // Current # players in session (read only)
    case integer of
      0 : (
    lpszSessionName: PCharAW;  // Name of the session
    lpszPassword: PCharAW;     // Password of the session (optional)
    dwReserved1: DWORD;        // Reserved for future MS use.
    dwReserved2: DWORD;
    dwUser1: DWORD;            // For use by the application
    dwUser2: DWORD;
    dwUser3: DWORD;
    dwUser4: DWORD;
      );
      1 : (
    lpszSessionNameA: PAnsiChar;   // Name of the session
    lpszPasswordA: PAnsiChar       // Password of the session (optional)
      );
      2 : (
    lpszSessionNameW: PWideChar;
    lpszPasswordW: PWideChar
      );
  end;

const
(*
 * Applications cannot create new players in this session.
 *)
  DPSESSION_NEWPLAYERSDISABLED = $00000001;

(*
 * If the DirectPlay object that created the session, the host,
 * quits, then the host will attempt to migrate to another
 * DirectPlay object so that new players can continue to be created
 * and new applications can join the session.
 *)
  DPSESSION_MIGRATEHOST = $00000004;

(*
 * This flag tells DirectPlay not to set the idPlayerTo and idPlayerFrom
 * fields in player messages.  This cuts two DWORD's off the message
 * overhead.
 *)
  DPSESSION_NOMESSAGEID = $00000008;

(*
 * This flag tells DirectPlay to not allow any new applications to
 * join the session.  Applications already in the session can still
 * create new players.
 *)
  DPSESSION_JOINDISABLED = $00000020;

(*
 * This flag tells DirectPlay to detect when remote players 
 * exit abnormally (e.g. their computer or modem gets unplugged)
 *)
  DPSESSION_KEEPALIVE = $00000040;

(*
 * This flag tells DirectPlay not to send a message to all players
 * when a players remote data changes
 *)
  DPSESSION_NODATAMESSAGES = $00000080;

(*
 * This flag indicates that the session belongs to a secure server
 * and needs user authentication
 *)
  DPSESSION_SECURESERVER = $00000100;

(*
 * This flag indicates that the session is private and requirs a password
 * for EnumSessions as well as Open.
 *)
  DPSESSION_PRIVATE = $00000200;

(*
 * This flag indicates that the session requires a password for joining.
 *)
  DPSESSION_PASSWORDREQUIRED = $00000400;

(*
 * This flag tells DirectPlay to route all messages through the server
 *)
  DPSESSION_MULTICASTSERVER = $00000800;

(*
 * This flag tells DirectPlay to only download information about the
 * DPPLAYER_SERVERPLAYER.
 *)
  DPSESSION_CLIENTSERVER = $00001000;

type
(*
 * TDPName
 * Used to hold the name of a DirectPlay entity
 * like a player or a group
 *)
  PDPName = ^TDPName;
  TDPName = packed record
    dwSize: DWORD;    // Size of structure
    dwFlags: DWORD;   // Not used. Must be zero.
    case Integer of
      0 : (
    lpszShortName : PCharAW; // The short or friendly name
    lpszLongName : PCharAW;  // The long or formal name
      );
      1 : (
    lpszShortNameA : PAnsiChar;
    lpszLongNameA : PAnsiChar;
      );
      2 : (
    lpszShortNameW : PWideChar;
    lpszLongNameW : PWideChar;
      );
  end;

(*
 * TDPCredentials
 * Used to hold the user name and password of a DirectPlay user
 *)

  PDPCredentials = ^TDPCredentials;
  TDPCredentials = packed record
    dwSize: DWORD;    // Size of structure
    dwFlags: DWORD;   // Not used. Must be zero.
    case Integer of
      0 : (
    lpszUsername: PCharAW;   // User name of the account
    lpszPassword: PCharAW;   // Password of the account
    lpszDomain:   PCharAW;   // Domain name of the account
      );
      1 : (
    lpszUsernameA: PAnsiChar;   // User name of the account
    lpszPasswordA: PAnsiChar;   // Password of the account
    lpszDomainA:   PAnsiChar;   // Domain name of the account
      );
      2 : (
    lpszUsernameW: PWideChar;   // User name of the account
    lpszPasswordW: PWideChar;   // Password of the account
    lpszDomainW:   PWideChar;   // Domain name of the account
      );
  end;

(*
 * TDPSecurityDesc
 * Used to describe the security properties of a DirectPlay
 * session instance
 *)
  PDPSecurityDesc = ^TDPSecurityDesc;
  TDPSecurityDesc = packed record
    dwSize: DWORD;                  // Size of structure
    dwFlags: DWORD;                 // Not used. Must be zero.
    case Integer of
      0 : (
    lpszSSPIProvider : PCharAW;  // SSPI provider name
    lpszCAPIProvider : PCharAW;  // CAPI provider name
    dwCAPIProviderType: DWORD;      // Crypto Service Provider type
    dwEncryptionAlgorithm: DWORD;   // Encryption Algorithm type
      );
      1 : (
    lpszSSPIProviderA : PAnsiChar;  // SSPI provider name
    lpszCAPIProviderA : PAnsiChar;  // CAPI provider name
      );
      2 : (
    lpszSSPIProviderW : PWideChar;  // SSPI provider name
    lpszCAPIProviderW : PWideChar;  // CAPI provider name
      );
  end;

(*
 * DPACCOUNTDESC
 * Used to describe a user membership account
 *)

  PDPAccountDesc = ^TDPAccountDesc;
  TDPAccountDesc = packed record
    dwSize: DWORD;    // Size of structure
    dwFlags: DWORD;   // Not used. Must be zero.
    case Integer of
      0 : (lpszAccountID : PCharAW);  // Account identifier
      1 : (lpszAccountIDA : PAnsiChar); 
      2 : (lpszAccountIDW : PWideChar);
  end;

(*
 * TDPLConnection
 * Used to hold all in the informaion needed to connect
 * an application to a session or create a session
 *)
  PDPLConnection = ^TDPLConnection;
  TDPLConnection = packed record
    dwSize: DWORD;                     // Size of this structure
    dwFlags: DWORD;                    // Flags specific to this structure
    lpSessionDesc: PDPSessionDesc2;    // Pointer to session desc to use on connect
    lpPlayerName: PDPName;             // Pointer to Player name structure
    guidSP: TGUID;                     // GUID of the DPlay SP to use
    lpAddress: Pointer;                // Address for service provider
    dwAddressSize: DWORD;              // Size of address data
  end;

(*
 * TDPChat
 * Used to hold the a DirectPlay chat message
 *)
  PDPChat = ^TDPChat;
  TDPChat = packed record
    dwSize: DWORD;
    dwFlags: DWORD;
    case Integer of
      0 : (lpszMessage : PCharAW);  // Message string
      1 : (lpszMessageA : PAnsiChar);
      2 : (lpszMessageW : PWideChar);  
  end;

(****************************************************************************
 *
 * Prototypes for DirectPlay callback functions
 *
 ****************************************************************************)

(*
 * Callback for IDirectPlay2::EnumSessions
 *)
  TDPEnumSessionsCallback2 = function(const lpThisSD: TDPSessionDesc2;
      var lpdwTimeOut: DWORD; dwFlags: DWORD; lpContext: Pointer) : BOOL;
      stdcall;

const
(*
 * This flag is set on the EnumSessions callback dwFlags parameter when
 * the time out has occurred. There will be no session data for this
 * callback. If *lpdwTimeOut is set to a non-zero value and the
 * EnumSessionsCallback function returns TRUE then EnumSessions will
 * continue waiting until the next timeout occurs. Timeouts are in
 * milliseconds.
 *)
  DPESC_TIMEDOUT = $00000001;

type
(*
 * Callback for IDirectPlay2::EnumPlayers
 *              IDirectPlay2::EnumGroups
 *              IDirectPlay2::EnumGroupPlayers
 *)
  TDPEnumPlayersCallback2 = function(TDPID: TDPID; dwPlayerType: DWORD;
      const lpName: TDPName; dwFlags: DWORD; lpContext: Pointer) : BOOL;
      stdcall;

(*
 * Callback for DirectPlayEnumerate
 * This callback prototype will be used if compiling
 * for ANSI strings
 *)
  TDPEnumDPCallback = function(const lpguidSP: TGUID; lpSPName: PCharAW;
      dwMajorVersion: DWORD; dwMinorVersion: DWORD; lpContext: Pointer) :
      BOOL; stdcall;

(*
 * ANSI callback for DirectPlayEnumerate
 * This callback prototype will be used if compiling
 * for ANSI strings
 *)
  TDPEnumDPCallbackA = function(const lpguidSP: TGUID; lpSPName: PAnsiChar;
      dwMajorVersion: DWORD; dwMinorVersion: DWORD; lpContext: Pointer) :
      BOOL; stdcall;

(*
 * Unicode callback for DirectPlayEnumerate
 * This callback prototype will be used if compiling
 * for Unicode strings
 *)
  TDPEnumDPCallbackW = function(const lpguidSP: TGUID; lpSPName: PWideChar;
      dwMajorVersion: DWORD; dwMinorVersion: DWORD; lpContext: Pointer) :
      BOOL; stdcall;

(*
 * Callback for IDirectPlay3(A)::EnumConnections
 *)
  TDPEnumConnectionsCallback = function(const lpguidSP: TGUID;
      lpConnection: Pointer; dwConnectionSize: DWORD; const lpName: TDPName;
      dwFlags: DWORD; lpContext: Pointer) : BOOL; stdcall;

(*
 * API's
 *)

function DirectPlayEnumerate(lpEnumDPCallback: TDPEnumDPCallback;
    lpContext: Pointer) : HResult; stdcall;
function DirectPlayEnumerateA(lpEnumDPCallback: TDPEnumDPCallbackA;
    lpContext: Pointer) : HResult; stdcall;
function DirectPlayEnumerateW(lpEnumDPCallback: TDPEnumDPCallbackW;
    lpContext: Pointer) : HResult; stdcall;


(****************************************************************************
 *
 * IDirectPlay2 (and IDirectPlay2A) Interface
 *
 ****************************************************************************)

type
{$IFDEF D2COM}
  IDirectPlay2AW = class (IUnknown)
{$ELSE}
  IDirectPlay2AW = interface (IUnknown)
{$ENDIF}
    (*** IDirectPlay2 methods ***)
    function AddPlayerToGroup(idGroup: TDPID; idPlayer: TDPID) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function Close: HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function CreateGroup(var lpidGroup: TDPID; var lpGroupName: TDPName;
        const lpData; dwDataSize: DWORD; dwFlags: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function CreatePlayer(var lpidPlayer: TDPID; var pPlayerName: TDPName;
        hEvent: THandle; lpData: pointer; dwDataSize: DWORD; dwFliags: DWORD) :
        HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function DeletePlayerFromGroup(idGroup: TDPID; idPlayer: TDPID) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function DestroyGroup(idGroup: TDPID) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function DestroyPlayer(idPlayer: TDPID) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function EnumGroupPlayers(idGroup: TDPID; const lpguidInstance: TGUID;
        lpEnumPlayersCallback2: TDPEnumPlayersCallback2; lpContext: Pointer;
        dwFlags: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function EnumGroups(const lpguidInstance: TGUID; lpEnumPlayersCallback2:
        TDPEnumPlayersCallback2; lpContext: Pointer; dwFlags: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function EnumPlayers(const lpguidInstance: TGUID; lpEnumPlayersCallback2:
        TDPEnumPlayersCallback2; lpContext: Pointer; dwFlags: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function EnumSessions(var lpsd: TDPSessionDesc2; dwTimeout: DWORD;
        lpEnumSessionsCallback2: TDPEnumSessionsCallback2; lpContext: Pointer;
        dwFlags: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetCaps(var lpDPCaps: TDPCaps; dwFlags: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetGroupData(idGroup: TDPID; var lpData; var lpdwDataSize: DWORD;
        dwFlags: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetGroupName(idGroup: TDPID; var lpData; var lpdwDataSize: DWORD) :
        HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetMessageCount(idPlayer: TDPID; var lpdwCount: DWORD) : HResult;
         {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetPlayerAddress(idPlayer: TDPID; var lpAddress;
        var lpdwAddressSize: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetPlayerCaps(idPlayer: TDPID; var lpPlayerCaps: TDPCaps;
        dwFlags: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetPlayerData(idPlayer: TDPID; var lpData; var lpdwDataSize: DWORD;
        dwFlags: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetPlayerName(idPlayer: TDPID; var lpData; var lpdwDataSize: DWORD)
        : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetSessionDesc(var lpData; var lpdwDataSize: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function Initialize(const lpGUID: TGUID) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function Open(var lpsd: TDPSessionDesc2; dwFlags: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function Receive(var lpidFrom: TDPID; var lpidTo: TDPID; dwFlags: DWORD;
        var lpData; var lpdwDataSize: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function Send(idFrom: TDPID; lpidTo: TDPID; dwFlags: DWORD; const lpData;
        lpdwDataSize: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetGroupData(idGroup: TDPID; const lpData; dwDataSize: DWORD;
        dwFlags: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetGroupName(idGroup: TDPID; const lpGroupName: TDPName;
        dwFlags: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetPlayerData(idPlayer: TDPID; const lpData; dwDataSize: DWORD;
        dwFlags: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetPlayerName(idPlayer: TDPID; const lpPlayerName: TDPName;
        dwFlags: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetSessionDesc(const lpSessDesc: TDPSessionDesc2; dwFlags: DWORD) :
        HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
  end;

{$IFDEF D2COM}
  IDirectPlay2A = IDirectPlay2AW;
  IDirectPlay2W = IDirectPlay2AW;
{$ELSE}
  IDirectPlay2W = interface (IDirectPlay2AW)
    ['{2B74F7C0-9154-11CF-A9CD-00AA006886E3}']
  end;
  IDirectPlay2A = interface (IDirectPlay2AW)
    ['{9d460580-a822-11cf-960c-0080c7534e82}']
  end;
{$ENDIF}

{$IFDEF UNICODE}
  IDirectPlay2 = IDirectPlay2W;
{$ELSE}
  IDirectPlay2 = IDirectPlay2A;
{$ENDIF}

(****************************************************************************
 *
 * IDirectPlay3 (and IDirectPlay3A) Interface
 *
 ****************************************************************************)

{$IFDEF D2COM}
  IDirectPlay3AW = class (IDirectPlay2AW)
{$ELSE}
  IDirectPlay3AW = interface (IDirectPlay2AW)
{$ENDIF}
    (*** IDirectPlay3 methods ***)
    function AddGroupToGroup(idParentGroup: TDPID; idGroup: TDPID) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function CreateGroupInGroup(idParentGroup: TDPID; var lpidGroup: TDPID;
        var lpGroupName: TDPName; const lpData; dwDataSize: DWORD;
        dwFlags: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function DeleteGroupFromGroup(idParentGroup: TDPID; idGroup: TDPID) :
        HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function EnumConnections(const lpguidApplication: TGUID;
        lpEnumCallback: TDPEnumConnectionsCallback; lpContext: Pointer;
        dwFlags: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function EnumGroupsInGroup(idGroup: TDPID; const lpguidInstance: TGUID;
        lpEnumPlayersCallback2: TDPEnumPlayersCallback2; lpContext: Pointer;
        dwFlags: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetGroupConnectionSettings(dwFlags: DWORD; idGroup: TDPID;
        var lpData; var lpdwDataSize: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function InitializeConnection(lpConnection: Pointer; dwFlags: DWORD) :
         HResult;
         {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SecureOpen(const lpsd: TDPSessionDesc2; dwFlags: DWORD;
        const lpSecurity: TDPSecurityDesc; const lpCredentials: TDPCredentials)
        : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SendChatMessage(idFrom: TDPID; idTo: TDPID; dwFlags: DWORD;
        const lpChatMessage: TDPChat) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetGroupConnectionSettings(dwFlags: DWORD; idGroup: TDPID;
        const lpConnection: TDPLConnection) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function StartSession(dwFlags: DWORD; idGroup: TDPID) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetGroupFlags(idGroup: TDPID; var lpdwFlags: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetGroupParent(idGroup: TDPID; var lpidParent: TDPID) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetPlayerAccount(idPlayer: TDPID; dwFlags: DWORD; var lpData;
        var lpdwDataSize: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetPlayerFlags(idPlayer: TDPID; var lpdwFlags: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
  end;


{$IFDEF D2COM}
  IDirectPlay3A = IDirectPlay3AW;
  IDirectPlay3W = IDirectPlay3AW;
{$ELSE}
  IDirectPlay3W = interface (IDirectPlay3AW)
    ['{133EFE40-32DC-11D0-9CFB-00A0C90A43CB}']
  end;
  IDirectPlay3A = interface (IDirectPlay3AW)
    ['{133efe41-32dc-11d0-9cfb-00a0c90a43cb}']
  end;
{$ENDIF}

{$IFDEF UNICODE}
  IDirectPlay3 = IDirectPlay3W;
{$ELSE}
  IDirectPlay3 = IDirectPlay3A;
{$ENDIF}


const
(****************************************************************************
 *
 * EnumConnections API flags
 *
 ****************************************************************************)

(*
 * Enumerate Service Providers
 *)
  DPCONNECTION_DIRECTPLAY = $00000001;

(*
 * Enumerate Lobby Providers
 *)
  DPCONNECTION_DIRECTPLAYLOBBY = $00000002;

(****************************************************************************
 *
 * EnumPlayers API flags
 *
 ****************************************************************************)

(*
 * Enumerate all players in the current session
 *)
  DPENUMPLAYERS_ALL = $00000000;
  DPENUMGROUPS_ALL = DPENUMPLAYERS_ALL;

(*
 * Enumerate only local (created by this application) players
 * or groups
 *)
  DPENUMPLAYERS_LOCAL = $00000008;
  DPENUMGROUPS_LOCAL = DPENUMPLAYERS_LOCAL;

(*
 * Enumerate only remote (non-local) players
 * or groups
 *)
  DPENUMPLAYERS_REMOTE = $00000010;
  DPENUMGROUPS_REMOTE = DPENUMPLAYERS_REMOTE;

(*
 * Enumerate groups along with the players
 *)
  DPENUMPLAYERS_GROUP = $00000020;

(*
 * Enumerate players or groups in another session 
 * (must supply lpguidInstance)
 *)
  DPENUMPLAYERS_SESSION = $00000080;
  DPENUMGROUPS_SESSION = DPENUMPLAYERS_SESSION;

(*
 * Enumerate server players
 *)
  DPENUMPLAYERS_SERVERPLAYER = $00000100;

(*
 * Enumerate spectator players
 *)
  DPENUMPLAYERS_SPECTATOR = $00000200;

(*
 * Enumerate shortcut groups
 *)
  DPENUMGROUPS_SHORTCUT = $00000400;

(*
 * Enumerate staging area groups
 *)
  DPENUMGROUPS_STAGINGAREA = $00000800;

(****************************************************************************
 *
 * CreatePlayer API flags
 *
 ****************************************************************************)

(*
 * This flag indicates that this player should be designated
 * the server player. The app should specify this at CreatePlayer.
 *)
  DPPLAYER_SERVERPLAYER = DPENUMPLAYERS_SERVERPLAYER;

(*
 * This flag indicates that this player should be designated
 * a spectator. The app should specify this at CreatePlayer.
 *)
  DPPLAYER_SPECTATOR = DPENUMPLAYERS_SPECTATOR;

(*
 * This flag indicates that this player was created locally.
 * (returned from GetPlayerFlags)
 *)
  DPPLAYER_LOCAL = DPENUMPLAYERS_LOCAL;

(****************************************************************************
 *
 * CreateGroup API flags
 *
 ****************************************************************************)

(*
 * This flag indicates that the StartSession can be called on the group.
 * The app should specify this at CreateGroup, or CreateGroupInGroup.
 *)
  DPGROUP_STAGINGAREA = DPENUMGROUPS_STAGINGAREA;

(*
 * This flag indicates that this group was created locally.
 * (returned from GetGroupFlags)
 *)
  DPGROUP_LOCAL = DPENUMGROUPS_LOCAL;

(****************************************************************************
 *
 * EnumSessions API flags
 *
 ****************************************************************************)

(*
 * Enumerate sessions which can be joined
 *)
  DPENUMSESSIONS_AVAILABLE = $00000001;

(*
 * Enumerate all sessions even if they can't be joined.
 *)
  DPENUMSESSIONS_ALL = $00000002;

(*
 * Start an asynchronous enum sessions
 *)
  DPENUMSESSIONS_ASYNC = $00000010;

(*
 * Stop an asynchronous enum sessions
 *)
  DPENUMSESSIONS_STOPASYNC = $00000020;

(*
 * Enumerate sessions even if they require a password
 *)
  DPENUMSESSIONS_PASSWORDREQUIRED = $00000040;

(*
 * Return status about progress of enumeration instead of
 * showing any status dialogs.
 *)
  DPENUMSESSIONS_RETURNSTATUS = $00000080;

(****************************************************************************
 *
 * GetCaps and GetPlayerCaps API flags
 *
 ****************************************************************************)

(*
 * The latency returned should be for guaranteed message sending.
 * Default is non-guaranteed messaging.
 *)
  DPGETCAPS_GUARANTEED = $00000001;

(****************************************************************************
 *
 * GetGroupData, GetPlayerData API flags
 * Remote and local Group/Player data is maintained separately. 
 * Default is DPGET_REMOTE.
 *
 ****************************************************************************)

(*
 * Get the remote data (set by any DirectPlay object in
 * the session using DPSET_REMOTE)
 *)
  DPGET_REMOTE = $00000000;

(*
 * Get the local data (set by this DirectPlay object 
 * using DPSET_LOCAL)
 *)
  DPGET_LOCAL = $00000001;

(****************************************************************************
 *
 * Open API flags
 *
 ****************************************************************************)

(*
 * Join the session that is described by the DPSESSIONDESC2 structure
 *)
  DPOPEN_JOIN = $00000001;

(*
 * Create a new session as described by the DPSESSIONDESC2 structure
 *)
  DPOPEN_CREATE = $00000002;

(*
 * Return status about progress of open instead of showing
 * any status dialogs.
 *)
  DPOPEN_RETURNSTATUS = DPENUMSESSIONS_RETURNSTATUS;

(****************************************************************************
 *
 * DPLCONNECTION flags
 *
 ****************************************************************************)

(*
 * This application should create a new session as
 * described by the DPSESIONDESC structure
 *)
  DPLCONNECTION_CREATESESSION = DPOPEN_CREATE;

(*
 * This application should join the session described by
 * the DPSESIONDESC structure with the lpAddress data
 *)
  DPLCONNECTION_JOINSESSION = DPOPEN_JOIN;

(****************************************************************************
 *
 * Receive API flags
 * Default is DPRECEIVE_ALL
 *
 ****************************************************************************)

(*
 * Get the first message in the queue
 *)
  DPRECEIVE_ALL = $00000001;

(*
 * Get the first message in the queue directed to a specific player 
 *)
  DPRECEIVE_TOPLAYER = $00000002;

(*
 * Get the first message in the queue from a specific player
 *)
  DPRECEIVE_FROMPLAYER = $00000004;

(*
 * Get the message but don't remove it from the queue
 *)
  DPRECEIVE_PEEK = $00000008;

(****************************************************************************
 *
 * Send API flags
 *
 ****************************************************************************)

(*
 * Send the message using a guaranteed send method.
 * Default is non-guaranteed.
 *)
  DPSEND_GUARANTEED = $00000001;

(*
 * This flag is obsolete. It is ignored by DirectPlay
 *)
  DPSEND_HIGHPRIORITY = $00000002;

(*
 * This flag is obsolete. It is ignored by DirectPlay
 *)
  DPSEND_OPENSTREAM = $00000008;

(*
 * This flag is obsolete. It is ignored by DirectPlay
 *)
  DPSEND_CLOSESTREAM = $00000010;

(*
 * Send the message digitally signed to ensure authenticity.
 *)
  DPSEND_SIGNED = $00000020;

(*
 * Send the message with encryption to ensure privacy.
 *)
  DPSEND_ENCRYPTED = $00000040;

(****************************************************************************
 *
 * SetGroupData, SetGroupName, SetPlayerData, SetPlayerName,
 * SetSessionDesc API flags.
 * Default is DPSET_REMOTE.
 *
 ****************************************************************************)

(* 
 * Propagate the data to all players in the session
 *)
  DPSET_REMOTE = $00000000;

(*
 * Do not propagate the data to other players
 *)
  DPSET_LOCAL = $00000001;

(*
 * Used with DPSET_REMOTE, use guaranteed message send to
 * propagate the data
 *)
  DPSET_GUARANTEED = $00000002;

(****************************************************************************
 *
 * DirectPlay system messages and message data structures
 *
 * All system message come 'From' player DPID_SYSMSG.  To determine what type 
 * of message it is, cast the lpData from Receive to TDPMsg_Generic and check
 * the dwType member against one of the following DPSYS_xxx constants. Once
 * a match is found, cast the lpData to the corresponding of the DPMSG_xxx
 * structures to access the data of the message.
 *
 ****************************************************************************)

(*
 * A new player or group has been created in the session
 * Use TDPMsg_CreatePlayergroup.  Check dwPlayerType to see if it
 * is a player or a group.
 *)
  DPSYS_CREATEPLAYERORGROUP = $0003;

(*
 * A player has been deleted from the session
 * Use TDPMsg_DestroyPlayergroup
 *)
  DPSYS_DESTROYPLAYERORGROUP = $0005;

(*
 * A player has been added to a group
 * Use DPMSG_ADDPLAYERTOGROUP
 *)
  DPSYS_ADDPLAYERTOGROUP = $0007;

(*
 * A player has been removed from a group
 * Use DPMSG_DELETEPLAYERFROMGROUP
 *)
  DPSYS_DELETEPLAYERFROMGROUP = $0021;

(*
 * This DirectPlay object lost its connection with all the
 * other players in the session.
 * Use DPMSG_SESSIONLOST.
 *)
  DPSYS_SESSIONLOST = $0031;

(*
 * The current host has left the session.
 * This DirectPlay object is now the host.
 * Use DPMSG_HOST.
 *)
  DPSYS_HOST = $0101;

(*
 * The remote data associated with a player or
 * group has changed. Check dwPlayerType to see
 * if it is a player or a group
 * Use DPMSG_SETPLAYERORGROUPDATA
 *)
  DPSYS_SETPLAYERORGROUPDATA = $0102;

(*
 * The name of a player or group has changed.
 * Check dwPlayerType to see if it is a player
 * or a group.
 * Use TDPMsg_SetPlayerOrGroupName
 *)
  DPSYS_SETPLAYERORGROUPNAME = $0103;

(*
 * The session description has changed.
 * Use DPMSG_SETSESSIONDESC
 *)
  DPSYS_SETSESSIONDESC = $0104;

(*
 * A group has been added to a group
 * Use TDPMsg_AddGroupToGroup
 *)
  DPSYS_ADDGROUPTOGROUP = $0105;

(*
 * A group has been removed from a group
 * Use DPMsg_DeleteGroupFromGroup
 *)
  DPSYS_DELETEGROUPFROMGROUP = $0106;

(*
 * A secure player-player message has arrived.
 * Use DPMSG_SECUREMESSAGE
 *)
  DPSYS_SECUREMESSAGE = $0107;

(*
 * Start a new session.
 * Use DPMSG_STARTSESSION
 *)
  DPSYS_STARTSESSION = $0108;

(*
 * A chat message has arrived
 * Use DPMSG_CHAT
 *)
  DPSYS_CHAT = $0109;

(*
 * Used in the dwPlayerType field to indicate if it applies to a group
 * or a player
 *)
  DPPLAYERTYPE_GROUP = $00000000;
  DPPLAYERTYPE_PLAYER = $00000001;

type
(*
 * TDPMsg_Generic
 * Generic message structure used to identify the message type.
 *)
  PDPMsg_Generic = ^TDPMsg_Generic;
  TDPMsg_Generic = packed record
    dwType: DWORD;   // Message type
  end;

(*
 * TDPMsg_CreatePlayergroup
 * System message generated when a new player or group
 * created in the session with information about it.
 *)
  PDPMsg_CreatePlayergroup = ^TDPMsg_CreatePlayergroup;
  TDPMsg_CreatePlayergroup = packed record
    dwType: DWORD;             // Message type
    dwPlayerType: DWORD;       // Is it a player or group
    TDPID: TDPID;                // ID of the player or group
    dwCurrentPlayers: DWORD;   // current # players & groups in session
    lpData: Pointer;           // pointer to remote data
    dwDataSize: DWORD;         // size of remote data
    dpnName: TDPName;           // structure with name info
                               // the following fields are only available when using
                               // the IDirectPlay3 interface or greater
    dpIdParent: TDPID;          // id of parent group
    dwFlags: DWORD;            // player or group flags
  end;

(*
 * TDPMsg_DestroyPlayergroup
 * System message generated when a player or group is being
 * destroyed in the session with information about it.
 *)
  PDPMsg_DestroyPlayergroup= ^TDPMsg_DestroyPlayergroup;
  TDPMsg_DestroyPlayergroup = packed record
    dwType: DWORD;             // Message type
    dwPlayerType: DWORD;       // Is it a player or group
    TDPID: TDPID;                // player ID being deleted
    lpLocalData: Pointer;      // copy of players local data
    dwLocalDataSize: DWORD;    // sizeof local data
    lpRemoteData: Pointer;     // copy of players remote data
    dwRemoteDataSize: DWORD;   // sizeof remote data
                               // the following fields are only available when using
                               // the IDirectPlay3 interface or greater
    dpnName: TDPName;           // structure with name info
    dpIdParent: TDPID;          // id of parent group
    dwFlags: DWORD;            // player or group flags
  end;

(*
 * DPMSG_ADDPLAYERTOGROUP
 * System message generated when a player is being added
 * to a group.
 *)
  PDPMsg_AddPlayerToGroup = ^TDPMsg_AddPlayerToGroup;
  TDPMsg_AddPlayerToGroup = packed record
    dwType: DWORD;      // Message type
    dpIdGroup: TDPID;    // group ID being added to
    dpIdPlayer: TDPID;   // player ID being added
  end;

(*
 * DPMSG_DELETEPLAYERFROMGROUP
 * System message generated when a player is being
 * removed from a group
 *)
  PDPMsg_DeletePlayerFromGroup = ^TDPMsg_DeletePlayerFromGroup;
  TDPMsg_DeletePlayerFromGroup = TDPMsg_AddPlayerToGroup;

(*
 * TDPMsg_AddGroupToGroup
 * System message generated when a group is being added
 * to a group.
 *)
  PDPMsg_AddGroupToGroup = ^TDPMsg_AddGroupToGroup;
  TDPMsg_AddGroupToGroup = packed record
    dwType: DWORD;           // Message type
    dpIdParentGroup: TDPID;   // group ID being added to
    dpIdGroup: TDPID;         // group ID being added
  end;

(*
 * DPMsg_DeleteGroupFromGroup
 * System message generated when a GROUP is being
 * removed from a group
 *)
  PDPMsg_DeleteGroupFromGroup = ^TDPMsg_DeleteGroupFromGroup;
  TDPMsg_DeleteGroupFromGroup = TDPMsg_AddGroupToGroup;

(*
 * DPMSG_SETPLAYERORGROUPDATA
 * System message generated when remote data for a player or
 * group has changed.
 *)
  PDPMsg_SetPlayerOrGroupData = ^TDPMsg_SetPlayerOrGroupData;
  TDPMsg_SetPlayerOrGroupData = packed record
    dwType: DWORD;         // Message type
    dwPlayerType: DWORD;   // Is it a player or group
    TDPID: TDPID;          // ID of player or group
    lpData: Pointer;       // pointer to remote data
    dwDataSize: DWORD;     // size of remote data
  end;

(*
 * DPMSG_SETPLAYERORGROUPNAME
 * System message generated when the name of a player or
 * group has changed.
 *)
  PDPMsg_SetPlayerOrGroupName = ^TDPMsg_SetPlayerOrGroupName;
  TDPMsg_SetPlayerOrGroupName = packed record
    dwType: DWORD;         // Message type
    dwPlayerType: DWORD;   // Is it a player or group
    TDPID: TDPID;          // ID of player or group
    dpnName: TDPName;      // structure with new name info
  end;

(*
 * DPMSG_SETSESSIONDESC
 * System message generated when session desc has changed
 *)
  PDPMsg_SetSessionDesc = ^TDPMsg_SetSessionDesc;
  TDPMsg_SetSessionDesc = packed record
    dwType: DWORD;            // Message type
    dpDesc: TDPSessionDesc2;   // Session desc
  end;

(*
 * DPMSG_HOST
 * System message generated when the host has migrated to this
 * DirectPlay object.
 *
 *)
  PDPMsg_Host = ^TDPMsg_Host;
  TDPMsg_Host = TDPMsg_Generic;

(*
 * DPMSG_SESSIONLOST
 * System message generated when the connection to the session is lost.
 *
 *)
  PDPMsg_SessionLost = ^TDPMsg_SessionLost;
  TDPMsg_SessionLost = TDPMsg_Generic;

(*
 * DPMSG_SECUREMESSAGE
 * System message generated when a player requests a secure send
 *)
  PDPMsg_SecureMessage = ^TDPMsg_SecureMessage;
  TDPMsg_SecureMessage = packed record
    dwType: DWORD;       // Message Type
    dwFlags: DWORD;      // Signed/Encrypted
    dpIdFrom: TDPID;      // ID of Sending Player
    lpData: Pointer;     // Player message
    dwDataSize: DWORD;   // Size of player message
  end;

(*
 * DPMSG_STARTSESSION
 * System message containing all information required to
 * start a new session
 *)
  PDPMsg_StartSession = ^TDPMsg_StartSession;
  TDPMsg_StartSession = packed record
    dwType: DWORD;             // Message type
    lpConn: PDPLConnection;   // TDPLConnection structure
  end;

(*
 * DPMSG_CHAT
 * System message containing a chat message
 *)
  PDPMsg_Chat = ^TDPMsg_Chat;
  TDPMsg_Chat = packed record
    dwType: DWORD;        // Message type
    dwFlags: DWORD;       // Message flags
    idFromPlayer: TDPID;  // ID of the Sending Player
    idToPlayer: TDPID;    // ID of the To Player
    idToGroup: TDPID;     // ID of the To Group
    lpChat: PDPChat;      // Pointer to a structure containing the chat message
  end;

(****************************************************************************
 *
 * DIRECTPLAY ERRORS
 *
 * Errors are represented by negative values and cannot be combined.
 *
 ****************************************************************************)
const
  DP_OK = S_OK;
  DPERR_ALREADYINITIALIZED = integer($88770000 + 5);
  DPERR_ACCESSDENIED = integer($88770000 + 10);
  DPERR_ACTIVEPLAYERS = integer($88770000 + 20);
  DPERR_BUFFERTOOSMALL = integer($88770000 + 30);
  DPERR_CANTADDPLAYER = integer($88770000 + 40);
  DPERR_CANTCREATEGROUP = integer($88770000 + 50);
  DPERR_CANTCREATEPLAYER = integer($88770000 + 60);
  DPERR_CANTCREATESESSION = integer($88770000 + 70);
  DPERR_CAPSNOTAVAILABLEYET = integer($88770000 + 80);
  DPERR_EXCEPTION = integer($88770000 + 90);
  DPERR_GENERIC = E_FAIL;
  DPERR_INVALIDCREDENTIALS = DPERR_GENERIC; // ?????????????????????????????????
  DPERR_INVALIDFLAGS = integer($88770000 + 120);
  DPERR_INVALIDOBJECT = integer($88770000 + 130);
  DPERR_INVALIDPARAM = E_INVALIDARG;
  DPERR_INVALIDPARAMS = DPERR_INVALIDPARAM;
  DPERR_INVALIDPLAYER = integer($88770000 + 150);
  DPERR_INVALIDGROUP = integer($88770000 + 155);
  DPERR_NOCAPS = integer($88770000 + 160);
  DPERR_NOCONNECTION = integer($88770000 + 170);
  DPERR_NOMEMORY = E_OUTOFMEMORY;
  DPERR_OUTOFMEMORY = DPERR_NOMEMORY;
  DPERR_NOMESSAGES = integer($88770000 + 190);
  DPERR_NONAMESERVERFOUND = integer($88770000 + 200);
  DPERR_NOPLAYERS = integer($88770000 + 210);
  DPERR_NOSESSIONS = integer($88770000 + 220);
  DPERR_PENDING = integer($80070007);
  DPERR_SENDTOOBIG = integer($88770000 + 230);
  DPERR_TIMEOUT = integer($88770000 + 240);
  DPERR_UNAVAILABLE = integer($88770000 + 250);
  DPERR_UNSUPPORTED = E_NOTIMPL;
  DPERR_BUSY = integer($88770000 + 270);
  DPERR_USERCANCEL = integer($88770000 + 280);
  DPERR_NOINTERFACE = E_NOINTERFACE;
  DPERR_CANNOTCREATESERVER = integer($88770000 + 290);
  DPERR_PLAYERLOST = integer($88770000 + 300);
  DPERR_SESSIONLOST = integer($88770000 + 310);
  DPERR_UNINITIALIZED = integer($88770000 + 320);
  DPERR_NONEWPLAYERS = integer($88770000 + 330);
  DPERR_INVALIDPASSWORD = integer($88770000 + 340);
  DPERR_CONNECTING = integer($88770000 + 350);


  DPERR_BUFFERTOOLARGE = integer($88770000 + 1000);
  DPERR_CANTCREATEPROCESS = integer($88770000 + 1010);
  DPERR_APPNOTSTARTED = integer($88770000 + 1020);
  DPERR_INVALIDINTERFACE = integer($88770000 + 1030);
  DPERR_NOSERVICEPROVIDER = integer($88770000 + 1040);
  DPERR_UNKNOWNAPPLICATION = integer($88770000 + 1050);
  DPERR_NOTLOBBIED = integer($88770000 + 1070);
  DPERR_SERVICEPROVIDERLOADED = integer($88770000 + 1080);
  DPERR_ALREADYREGISTERED = integer($88770000 + 1090);
  DPERR_NOTREGISTERED = integer($88770000 + 1100);

//
// Security related errors
//
  DPERR_AUTHENTICATIONFAILED = integer($88770000 + 2000);
  DPERR_CANTLOADSSPI = integer($88770000 + 2010);
  DPERR_ENCRYPTIONFAILED = integer($88770000 + 2020);
  DPERR_SIGNFAILED = integer($88770000 + 2030);
  DPERR_CANTLOADSECURITYPACKAGE = integer($88770000 + 2040);
  DPERR_ENCRYPTIONNOTSUPPORTED = integer($88770000 + 2050);
  DPERR_CANTLOADCAPI = integer($88770000 + 2060);
  DPERR_NOTLOGGEDIN = integer($88770000 + 2070);
  DPERR_LOGONDENIED = integer($88770000 + 2080);

(****************************************************************************
 *
 * 	dplay 1.0 obsolete structures + interfaces
 *	Included for compatibility only. New apps should
 *	use IDirectPlay2
 *
 ****************************************************************************)

  DPOPEN_OPENSESSION = DPOPEN_JOIN;
  DPOPEN_CREATESESSION = DPOPEN_CREATE;

  DPENUMSESSIONS_PREVIOUS = $00000004;

  DPENUMPLAYERS_PREVIOUS = $00000004;

  DPSEND_GUARANTEE = DPSEND_GUARANTEED;
  DPSEND_TRYONCE = $00000004;

  DPCAPS_NAMESERVICE = $00000001;
  DPCAPS_NAMESERVER = DPCAPS_ISHOST;
  DPCAPS_GUARANTEED = $00000004;

  DPLONGNAMELEN = 52;
  DPSHORTNAMELEN = 20;
  DPSESSIONNAMELEN = 32;
  DPPASSWORDLEN = 16;
  DPUSERRESERVED = 16;

  DPSYS_ADDPLAYER = $0003;
  DPSYS_DELETEPLAYER = $0005;

  DPSYS_DELETEGROUP = $0020;
  DPSYS_DELETEPLAYERFROMGRP = $0021;
  DPSYS_CONNECT = $484b;

type
  PDPMsg_AddPlayer = ^TDPMsg_AddPlayer;
  TDPMsg_AddPlayer = packed record
    dwType: DWORD;
    dwPlayerType: DWORD;
    TDPID: TDPID;
    szLongName: array[0..DPLONGNAMELEN-1] of AnsiChar;
    szShortName: array[0..DPSHORTNAMELEN-1] of AnsiChar;
    dwCurrentPlayers: DWORD;
  end;

  PDPMsg_AddGroup = ^TDPMsg_AddGroup;
  TDPMsg_AddGroup = TDPMsg_AddPlayer;

  PDPMsg_GroupAdd = ^TDPMsg_GroupAdd;
  TDPMsg_GroupAdd = packed record
    dwType: DWORD;
    dpIdGroup: TDPID;
    dpIdPlayer: TDPID;
  end;

  PDPMsg_GroupDelete = ^TDPMsg_GroupDelete;
  TDPMsg_GroupDelete = TDPMsg_GroupAdd;

  PDPMsg_DeletePlayer = ^TDPMsg_DeletePlayer;
  TDPMsg_DeletePlayer = packed record
    dwType: DWORD;
    TDPID: TDPID;
  end;

  TDPEnumPlayersCallback = function(dpId: TDPID; lpFriendlyName: PAnsiChar;
      lpFormalName: PAnsiChar; dwFlags: DWORD; lpContext: Pointer) : BOOL; stdcall;

  PDPSessionDesc = ^TDPSessionDesc;
  TDPSessionDesc = packed record
    dwSize: DWORD;
    guidSession: TGUID;
    dwSession: DWORD;
    dwMaxPlayers: DWORD;
    dwCurrentPlayers: DWORD;
    dwFlags: DWORD;
    szSessionName: Array [0..DPSESSIONNAMELEN-1] of AnsiChar;
    szUserField: Array [0..DPUSERRESERVED-1] of AnsiChar;
    dwReserved1: DWORD;
    szPassword: Array [0..DPPASSWORDLEN-1] of AnsiChar;
    dwReserved2: DWORD;
    dwUser1: DWORD;
    dwUser2: DWORD;
    dwUser3: DWORD;
    dwUser4: DWORD;
  end;

  TDPEnumSessionsCallback = function(var lpDPSessionDesc: TDPSessionDesc;
      lpContext: Pointer; var lpdwTimeOut: DWORD; dwFlags: DWORD) : BOOL;
      stdcall;

type
(*
 * IDirectPlay
 *)
{$IFDEF D2COM}
  IDirectPlay = class (IUnknown)
{$ELSE}
  IDirectPlay = interface (IUnknown)
    ['{5454e9a0-db65-11ce-921c-00aa006c4972}']
{$ENDIF}
    (*** IDirectPlay methods ***)
    function AddPlayerToGroup(pidGroup: TDPID; pidPlayer: TDPID) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function Close: HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function CreatePlayer(var lppidID: TDPID; lpPlayerFriendlyName: PAnsiChar;
        lpPlayerFormalName: PAnsiChar; lpEvent: PHandle) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function CreateGroup(var lppidID: TDPID; lpGroupFriendlyName: PAnsiChar;
        lpGroupFormalName: PAnsiChar) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function DeletePlayerFromGroup(pidGroup: TDPID; pidPlayer: TDPID) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function DestroyPlayer(pidID: TDPID) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function DestroyGroup(pidID: TDPID) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function EnableNewPlayers(bEnable: BOOL) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function EnumGroupPlayers(pidGroupPID: TDPID; lpEnumPlayersCallback:
        TDPEnumPlayersCallback; lpContext: Pointer; dwFlags: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function EnumGroups(dwSessionID: DWORD; lpEnumPlayersCallback:
        TDPEnumPlayersCallback; lpContext: Pointer; dwFlags: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function EnumPlayers(dwSessionId: DWORD; lpEnumPlayersCallback:
        TDPEnumPlayersCallback; lpContext: Pointer; dwFlags: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function EnumSessions(const lpSDesc: TDPSessionDesc; dwTimeout: DWORD;
        lpEnumSessionsCallback: TDPEnumSessionsCallback; lpContext: Pointer;
        dwFlags: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetCaps(const lpDPCaps: TDPCaps) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetMessageCount(pidID: TDPID; var lpdwCount: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetPlayerCaps(pidID: TDPID; const lpDPPlayerCaps: TDPCaps) :
        HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetPlayerName(pidID: TDPID; lpPlayerFriendlyName: PAnsiChar;
        var lpdwFriendlyNameLength: DWORD; lpPlayerFormalName: PAnsiChar;
        var lpdwFormalNameLength: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function Initialize(const lpGUID: TGUID) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function Open(const lpSDesc: TDPSessionDesc) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function Receive(var lppidFrom, lppidTo: TDPID; dwFlags: DWORD;
        var lpvBuffer; var lpdwSize: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SaveSession(lpSessionName: PAnsiChar) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function Send(pidFrom: TDPID; pidTo: TDPID; dwFlags: DWORD;
        const lpvBuffer; dwBuffSize: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetPlayerName(pidID: TDPID; lpPlayerFriendlyName: PAnsiChar;
        lpPlayerFormalName: PAnsiChar) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
  end;

function DirectPlayCreate(const lpGUID: TGUID; const lplpDP: IDirectPlay;
    pUnk: IUnknown) : HResult; stdcall;



implementation

// #define MAKE_DPHRESULT( code )    MAKE_HRESULT( 1, _FACDP, code )
function MAKE_DPHResult(code: variant) : HResult;
begin
  Result := (1 shl 31) or (_FACDP shl 16) or code;
end;

function DirectPlayEnumerateA; external 'DPlayX.dll';
function DirectPlayEnumerateW; external 'DPlayX.dll';
{$IFDEF UNICODE}
function DirectPlayEnumerate; external 'DPlayX.dll' name 'DirectPlayEnumerateW';
{$ELSE}
function DirectPlayEnumerate; external 'DPlayX.dll' name 'DirectPlayEnumerateA';
{$ENDIF}

function DirectPlayCreate; external 'DPlayX.dll';

end.
