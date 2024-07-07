(*==========================================================================;
 *
 *  Copyright (C) 1996-1997 Microsoft Corporation.  All Rights Reserved.
 *
 *  File:	dvp.h
 *  Content:	DirectDrawVideoPort include file
 *
 *  DirectX 5 Delphi adaptation by Erik Unger
 *
 *  Modyfied: 15.4.98
 *
 *  Download: http://www.sbox.tu-graz.ac.at/home/ungerik/DelphiGraphics/
 *  E-Mail: h_unger@magnet.at
 *
 ***************************************************************************)

unit DVP;

{$INCLUDE COMSWITCH.INC}
{$INCLUDE STRINGSWITCH.INC}

interface

uses
{$IFDEF D2COM}
  OLE2,
{$ENDIF}
  Windows,
  DDraw;

const
(*
 * GUIDS used by DirectDrawVideoPort objects
 *)
  IID_IDDVideoPortContainer: TGUID =
      (D1:$6C142760;D2:$A733;D3:$11CE;D4:($A5,$21,$00,$20,$AF,$0B,$E5,$60));
  IID_IDirectDrawVideoPort: TGUID =
      (D1:$B36D93E0;D2:$2B43;D3:$11CF;D4:($A2,$DE,$00,$AA,$00,$B9,$33,$56));

  DDVPTYPE_E_HREFH_VREFH: TGUID =
      (D1:$54F39980;D2:$DA60;D3:$11CF;D4:($9B,$06,$00,$A0,$C9,$03,$A3,$B8));
  DDVPTYPE_E_HREFH_VREFL: TGUID =
      (D1:$92783220;D2:$DA60;D3:$11CF;D4:($9B,$06,$00,$A0,$C9,$03,$A3,$B8));
  DDVPTYPE_E_HREFL_VREFH: TGUID =
      (D1:$A07A02E0;D2:$DA60;D3:$11CF;D4:($9B,$06,$00,$A0,$C9,$03,$A3,$B8));
  DDVPTYPE_E_HREFL_VREFL: TGUID =
      (D1:$E09C77E0;D2:$DA60;D3:$11CF;D4:($9B,$06,$00,$A0,$C9,$03,$A3,$B8));
  DDVPTYPE_CCIR656: TGUID =
      (D1:$FCA326A0;D2:$DA60;D3:$11CF;D4:($9B,$06,$00,$A0,$C9,$03,$A3,$B8));
  DDVPTYPE_BROOKTREE: TGUID =
      (D1:$1352A560;D2:$DA61;D3:$11CF;D4:($9B,$06,$00,$A0,$C9,$03,$A3,$B8));
  DDVPTYPE_PHILIPS: TGUID =
      (D1:$332CF160;D2:$DA61;D3:$11CF;D4:($9B,$06,$00,$A0,$C9,$03,$A3,$B8));

(*
 * GUIDS used to describe connections
 *)

(*============================================================================
 *
 * DirectDraw Structures
 *
 * Various structures used to invoke DirectDraw.
 *
 *==========================================================================*)

type

(*
 * DDVIDEOPORTCONNECT
 *)
  DDVIDEOPORTCONNECT = record
    dwSize: DWORD;        // size of the DDVIDEOPORTCONNECT structure
    dwPortWidth: DWORD;   // Width of the video port
    guidTypeID: TGUID;    // Description of video port connection
    dwFlags: DWORD;       // Connection flags
    dwReserved1: DWORD;   // Reserved, set to zero.
  end;
  LPDDVIDEOPORTCONNECT = ^DDVIDEOPORTCONNECT;

(*
 * DDVIDEOPORTCAPS
 *)
  DDVIDEOPORTCAPS = record
    dwSize: DWORD;                          // size of the DDVIDEOPORTCAPS structure
    dwFlags: DWORD;                         // indicates which fields contain data
    dwMaxWidth: DWORD;                      // max width of the video port field
    dwMaxVBIWidth: DWORD;                   // max width of the VBI data
    dwMaxHeight: DWORD;                     // max height of the video port field
    dwVideoPortID: DWORD;                   // Video port ID (0 - (dwMaxVideoPorts -1))
    dwCaps: DWORD;                          // Video port capabilities
    dwFX: DWORD;                            // More video port capabilities
    dwNumAutoFlipSurfaces: DWORD;           // Number of autoflippable surfaces
    dwAlignVideoPortBoundary: DWORD;        // Byte restriction of placement within the surface
    dwAlignVideoPortPrescaleWidth: DWORD;   // Byte restriction of width after prescaling
    dwAlignVideoPortCropBoundary: DWORD;    // Byte restriction of left cropping
    dwAlignVideoPortCropWidth: DWORD;       // Byte restriction of cropping width
    dwPreshrinkXStep: DWORD;                // Width can be shrunk in steps of 1/x
    dwPreshrinkYStep: DWORD;                // Height can be shrunk in steps of 1/x
    dwNumVBIAutoFlipSurfaces: DWORD;        // Number of VBI autoflippable surfaces
    dwReserved1: DWORD;                     // Reserved for future use
    dwReserved2: DWORD;                     // Reserved for future use
  end;
  LPDDVIDEOPORTCAPS = ^DDVIDEOPORTCAPS;

const
(*
 * The dwMaxWidth and dwMaxVBIWidth members are valid
 *)
  DDVPD_WIDTH = $00000001;

(*
 * The dwMaxHeight member is valid
 *)
  DDVPD_HEIGHT = $00000002;

(*
 * The dwVideoPortID member is valid
 *)
  DDVPD_ID = $00000004;

(*
 * The dwCaps member is valid
 *)
  DDVPD_CAPS = $00000008;

(*
 * The dwFX member is valid
 *)
  DDVPD_FX = $00000010;

(*
 * The dwNumAutoFlipSurfaces member is valid
 *)
  DDVPD_AUTOFLIP = $00000020;

(*
 * All of the alignment members are valid
 *)
  DDVPD_ALIGN = $00000040;

type
(*
 * DDVIDEOPORTDESC
 *)
  DDVIDEOPORTDESC = record
    dwSize: DWORD;                       // size of the DDVIDEOPORTDESC structure
    dwFieldWidth: DWORD;                 // width of the video port field
    dwVBIWidth: DWORD;                   // width of the VBI data
    dwFieldHeight: DWORD;                // height of the video port field
    dwMicrosecondsPerField: DWORD;       // Microseconds per video field
    dwMaxPixelsPerSecond: DWORD;         // Maximum pixel rate per second
    dwVideoPortID: DWORD;                // Video port ID (0 - (dwMaxVideoPorts -1))
    dwReserved1: DWORD;                  // Reserved for future use - set to zero
    VideoPortType: DDVIDEOPORTCONNECT;   // Description of video port connection
    dwReserved2: DWORD;                  // Reserved for future use - set to zero
    dwReserved3: DWORD;                  // Reserved for future use - set to zero
  end;
  LPDDVIDEOPORTDESC = ^DDVIDEOPORTDESC;

(*
 * DDVIDEOPORTINFO
 *)
  DDVIDEOPORTINFO = record
    dwSize: DWORD;                            // Size of the structure
    dwOriginX: DWORD;                         // Placement of the video data within the surface.
    dwOriginY: DWORD;                         // Placement of the video data within the surface.
    dwVPFlags: DWORD;                         // Video port options
    rCrop: TRect;                             // Cropping rectangle (optional).
    dwPrescaleWidth: DWORD;                   // Determines pre-scaling/zooming in the X direction (optional).
    dwPrescaleHeight: DWORD;                  // Determines pre-scaling/zooming in the Y direction (optional).
    lpddpfInputFormat: PDDPixelFormat;       // Video format written to the video port
    lpddpfVBIInputFormat: PDDPixelFormat;    // Input format of the VBI data
    lpddpfVBIOutputFormat: PDDPixelFormat;   // Output format of the data
    dwVBIHeight: DWORD;                       // Specifies the number of lines of data within the vertical blanking interval.
    dwReserved1: DWORD;                       // Reserved for future use - set to zero
    dwReserved2: DWORD;                       // Reserved for future use - set to zero
  end;
  LPDDVIDEOPORTINFO = ^DDVIDEOPORTINFO;

(*
 * DDVIDEOPORTBANDWIDTH
 *)
  DDVIDEOPORTBANDWIDTH = record
    dwSize: DWORD;                 // Size of the structure
    dwCaps: DWORD;
    dwOverlay: DWORD;              // Zoom factor at which overlay is supported
    dwColorkey: DWORD;             // Zoom factor at which overlay w/ colorkey is supported
    dwYInterpolate: DWORD;         // Zoom factor at which overlay w/ Y interpolation is supported
    dwYInterpAndColorkey: DWORD;   // Zoom factor at which ovelray w/ Y interpolation and colorkeying is supported
    dwReserved1: DWORD;            // Reserved for future use - set to zero
    dwReserved2: DWORD;            // Reserved for future use - set to zero
  end;
  LPDDVIDEOPORTBANDWIDTH = ^DDVIDEOPORTBANDWIDTH;

(*
 * DDVIDEOPORTSTATUS
 *)
  DDVIDEOPORTSTATUS = record
    dwSize: DWORD;                       // Size of the structure
    bInUse: BOOL;                        // TRUE if video port is currently being used
    dwFlags: DWORD;                      // Currently not used
    dwReserved1: DWORD;                  // Reserved for future use
    VideoPortType: DDVIDEOPORTCONNECT;   // Information about the connection
    dwReserved2: DWORD;                  // Reserved for future use
    dwReserved3: DWORD;                  // Reserved for future use
  end;
  LPDDVIDEOPORTSTATUS = ^DDVIDEOPORTSTATUS;

const
(*============================================================================
 *
 * Video Port Flags
 *
 * All flags are bit flags.
 *
 *==========================================================================*)

(****************************************************************************
 *
 * VIDEOPORT DDVIDEOPORTCONNECT FLAGS
 *
 ****************************************************************************)

(*
 * When this is set by the driver and passed to the client, this
 * indicates that the video port is capable of double clocking the data.
 * When this is set by the client, this indicates that the video port
 * should enable double clocking.  This flag is only valid with external
 * syncs.
 *)
  DDVPCONNECT_DOUBLECLOCK = $00000001;

(*
 * When this is set by the driver and passed to the client, this
 * indicates that the video port is capable of using an external VACT
 * signal. When this is set by the client, this indicates that the
 * video port should use the external VACT signal.
 *)
  DDVPCONNECT_VACT = $00000002;

(*
 * When this is set by the driver and passed to the client, this
 * indicates that the video port is capable of treating even fields
 * like odd fields and visa versa.  When this is set by the client,
 * this indicates that the video port should treat even fields like odd
 * fields.
 *)
  DDVPCONNECT_INVERTPOLARITY = $00000004;

(*
 * Indicates that any data written to the video port during the VREF
 * period will not be written into the frame buffer. This flag is read only.
 *)
  DDVPCONNECT_DISCARDSVREFDATA = $00000008;

(*
 * Device will write half lines into the frame buffer, sometimes causing
 * the data to not be displayed correctly.
 *)
  DDVPCONNECT_HALFLINE = $00000010;

(*
 * Indicates that the signal is interlaced. This flag is only
 * set by the client.
 *)
  DDVPCONNECT_INTERLACED = $00000020;

(*
 * Indicates that video port is shareable and that this video port
 * will use the even fields.  This flag is only set by the client.
 *)
  DDVPCONNECT_SHAREEVEN = $00000040;

(*
 * Indicates that video port is shareable and that this video port
 * will use the odd fields.  This flag is only set by the client.
 *)
  DDVPCONNECT_SHAREODD = $00000080;

(****************************************************************************
 *
 * VIDEOPORT DDVIDEOPORTDESC CAPS
 *
 ****************************************************************************)

(*
 * Flip can be performed automatically to avoid tearing.
 *)
  DDVPCAPS_AUTOFLIP = $00000001;

(*
 * Supports interlaced video
 *)
  DDVPCAPS_INTERLACED = $00000002;

(*
 * Supports non-interlaced video
 *)
  DDVPCAPS_NONINTERLACED = $00000004;

(*
 * Indicates that the device can return whether the current field
 * of an interlaced signal is even or odd.
 *)
  DDVPCAPS_READBACKFIELD = $00000008;

(*
 * Indicates that the device can return the current line of video
 * being written into the frame buffer.
 *)
  DDVPCAPS_READBACKLINE = $00000010;

(*
 * Allows two gen-locked video streams to share a single video port,
 * where one stream uses the even fields and the other uses the odd
 * fields. Separate parameters (including address, scaling,
 * cropping, etc.) are maintained for both fields.)
 *)
  DDVPCAPS_SHAREABLE = $00000020;

(*
 * Even fields of video can be automatically discarded.
 *)
  DDVPCAPS_SKIPEVENFIELDS = $00000040;

(*
 * Odd fields of video can be automatically discarded.
 *)
  DDVPCAPS_SKIPODDFIELDS = $00000080;

(*
 * Indicates that the device is capable of driving the graphics
 * VSYNC with the video port VSYNC.
 *)
  DDVPCAPS_SYNCMASTER = $00000100;

(*
 * Indicates that data within the vertical blanking interval can
 * be written to a different surface.
 *)
  DDVPCAPS_VBISURFACE = $00000200;

(*
 * Indicates that the video port can perform color operations
 * on the incoming data before it is written to the frame buffer.
 *)
  DDVPCAPS_COLORCONTROL = $00000400;

(*
 * Indicates that the video port can accept VBI data in a different
 * width or format than the regular video data.
 *)
  DDVPCAPS_OVERSAMPLEDVBI = $00000800;

(*
 * Indicates that the video port can write data directly to system memory
 *)
  DDVPCAPS_SYSTEMMEMORY = $00001000;

(****************************************************************************
 *
 * VIDEOPORT DDVIDEOPORTDESC FX
 *
 ****************************************************************************)

(*
 * Limited cropping is available to crop out the vertical interval data.
 *)
  DDVPFX_CROPTOPDATA = $00000001;

(*
 * Incoming data can be cropped in the X direction before it is written
 * to the surface.
 *)
  DDVPFX_CROPX = $00000002;

(*
 * Incoming data can be cropped in the Y direction before it is written
 * to the surface.
 *)
  DDVPFX_CROPY = $00000004;

(*
 * Supports interleaving interlaced fields in memory.
 *)
  DDVPFX_INTERLEAVE = $00000008;

(*
 * Supports mirroring left to right as the video data is written
 * into the frame buffer.
 *)
  DDVPFX_MIRRORLEFTRIGHT = $00000010;

(*
 * Supports mirroring top to bottom as the video data is written
 * into the frame buffer.
 *)
  DDVPFX_MIRRORUPDOWN = $00000020;

(*
 * Data can be arbitrarily shrunk in the X direction before it
 * is written to the surface.
 *)
  DDVPFX_PRESHRINKX = $00000040;

(*
 * Data can be arbitrarily shrunk in the Y direction before it
 * is written to the surface.
 *)
  DDVPFX_PRESHRINKY = $00000080;

(*
 * Data can be binary shrunk (1/2, 1/4, 1/8, etc.) in the X
 * direction before it is written to the surface.
 *)
  DDVPFX_PRESHRINKXB = $00000100;

(*
 * Data can be binary shrunk (1/2, 1/4, 1/8, etc.) in the Y
 * direction before it is written to the surface.
 *)
  DDVPFX_PRESHRINKYB = $00000200;

(*
 * Data can be shrunk in increments of 1/x in the X direction
 * (where X is specified in the DDVIDEOPORTCAPS.dwPreshrinkXStep)
 * before it is written to the surface.
 *)
  DDVPFX_PRESHRINKXS = $00000400;

(*
 * Data can be shrunk in increments of 1/x in the Y direction
 * (where X is specified in the DDVIDEOPORTCAPS.dwPreshrinkYStep)
 * before it is written to the surface.
 *)
  DDVPFX_PRESHRINKYS = $00000800;

(*
 * Data can be arbitrarily stretched in the X direction before
 * it is written to the surface.
 *)
  DDVPFX_PRESTRETCHX = $00001000;

(*
 * Data can be arbitrarily stretched in the Y direction before
 * it is written to the surface.
 *)
  DDVPFX_PRESTRETCHY = $00002000;

(*
 * Data can be integer stretched in the X direction before it is
 * written to the surface.
 *)
  DDVPFX_PRESTRETCHXN = $00004000;

(*
 * Data can be integer stretched in the Y direction before it is
 * written to the surface.
 *)
  DDVPFX_PRESTRETCHYN = $00008000;

(*
 * Indicates that data within the vertical blanking interval can
 * be converted independently of the remaining video data.
 *)
  DDVPFX_VBICONVERT = $00010000;

(*
 * Indicates that scaling can be disabled for data within the
 * vertical blanking interval.
 *)
  DDVPFX_VBINOSCALE = $00020000;

(*
 * Indicates that the video data can ignore the left and right
 * cropping coordinates when cropping oversampled VBI data.
 *)
  DDVPFX_IGNOREVBIXCROP = $00040000;

(****************************************************************************
 *
 * VIDEOPORT DDVIDEOPORTINFO FLAGS
 *
 ****************************************************************************)

(*
 * Perform automatic flipping.   Auto-flipping is performed between
 * the overlay surface that was attached to the video port using
 * IDirectDrawVideoPort::AttachSurface and the overlay surfaces that
 * are attached to the surface via the IDirectDrawSurface::AttachSurface
 * method.  The flip order is the order in which the overlay surfaces
 * were. attached.
 *)
  DDVP_AUTOFLIP = $00000001;

(*
 * Perform conversion using the ddpfOutputFormat information.
 *)
  DDVP_CONVERT = $00000002;

(*
 * Perform cropping using the specified rectangle.
 *)
  DDVP_CROP = $00000004;

(*
 * Indicates that interlaced fields should be interleaved in memory.
 *)
  DDVP_INTERLEAVE = $00000008;

(*
 * Indicates that the data should be mirrored left to right as it's
 * written into the frame buffer.
 *)
  DDVP_MIRRORLEFTRIGHT = $00000010;

(*
 * Indicates that the data should be mirrored top to bottom as it's
 * written into the frame buffer.
 *)
  DDVP_MIRRORUPDOWN = $00000020;

(*
 * Perform pre-scaling/zooming based on the pre-scale parameters.
 *)
  DDVP_PRESCALE = $00000040;

(*
 * Ignore input of even fields.
 *)
  DDVP_SKIPEVENFIELDS = $00000080;

(*
 * Ignore input of odd fields.
 *)
  DDVP_SKIPODDFIELDS = $00000100;

(*
 * Drive the graphics VSYNCs using the video port VYSNCs.
 *)
  DDVP_SYNCMASTER = $00000200;

(*
 * The ddpfVBIOutputFormatFormat member contains data that should be used
 * to convert the data within the vertical blanking interval.
 *)
  DDVP_VBICONVERT = $00000400;

(*
 * Indicates that data within the vertical blanking interval
 * should not be scaled.
 *)
  DDVP_VBINOSCALE = $00000800;

(*
 * Indicates that these bob/weave decisions should not be
 * overriden by other interfaces.
 *)
  DDVP_OVERRIDEBOBWEAVE = $00001000;

(*
 * Indicates that the video data should ignore the left and right
 * cropping coordinates when cropping the VBI data.
 *)
  DDVP_IGNOREVBIXCROP = $00002000;

(****************************************************************************
 *
 * DIRIRECTDRAWVIDEOPORT GETINPUTFORMAT/GETOUTPUTFORMAT FLAGS
 *
 ****************************************************************************)

(*
 * Return formats for the video data
 *)
  DDVPFORMAT_VIDEO = $00000001;

(*
 * Return formats for the VBI data
 *)
  DDVPFORMAT_VBI = $00000002;

(****************************************************************************
 *
 * DIRIRECTDRAWVIDEOPORT SETTARGETSURFACE FLAGS
 *
 ****************************************************************************)

(*
 * Surface should receive video data (and VBI data if a surface
 * is not explicitly attached for that purpose)
 *)
  DDVPTARGET_VIDEO = $00000001;

(*
 * Surface should receive VBI data
 *)
  DDVPTARGET_VBI = $00000002;

(****************************************************************************
 *
 * DIRIRECTDRAWVIDEOPORT WAITFORSYNC FLAGS
 *
 ****************************************************************************)

(*
 * Waits until the beginning of the next VSYNC
 *)
  DDVPWAIT_BEGIN = $00000001;

(*
 * Waits until the end of the next/current VSYNC
 *)
  DDVPWAIT_END = $00000002;

(*
 * Waits until the beginning of the specified line
 *)
  DDVPWAIT_LINE = $00000003;

(****************************************************************************
 *
 * DIRECTDRAWVIDEOPORT FLIP FLAGS
 *
 ****************************************************************************)

(*
 * Flips the normal video surface
 *)
  DDVPFLIP_VIDEO = $00000001;

(*
 * Flips the VBI surface
 *)
  DDVPFLIP_VBI = $00000002;

(****************************************************************************
 *
 * DIRIRECTDRAWVIDEOPORT GETVIDEOSIGNALSTATUS VALUES
 *
 ****************************************************************************)

(*
 * No video signal is present at the video port
 *)
  DDVPSQ_NOSIGNAL = $00000001;

(*
 * A valid video signal is present at the video port
 *)
  DDVPSQ_SIGNALOK = $00000002;

(****************************************************************************
 *
 * VIDEOPORTBANDWIDTH Flags
 *
 ****************************************************************************)

(*
 * The specified height/width refer to the size of the video port data
 * written into memory, after prescaling has occured.
 *)
  DDVPB_VIDEOPORT = $00000001;

(*
 * The specified height/width refer to the source size of the overlay.
 *)
  DDVPB_OVERLAY = $00000002;

(*
 * This is a query for the device to return which caps this device requires.
 *)
  DDVPB_TYPE = $00000004;

(****************************************************************************
 *
 * VIDEOPORTBANDWIDTH Caps
 *
 ****************************************************************************)

(*
 * The bandwidth for this device is dependant on the overlay source size.
 *)
  DDVPBCAPS_SOURCE = $00000001;

(*
 * The bandwidth for this device is dependant on the overlay destination
 * size.
 *)
  DDVPBCAPS_DESTINATION = $00000002;

type
(*
 * API's
 *)

  LPDDENUMVIDEOCALLBACK = function(lpDDVideoPortCaps: DDVIDEOPORTCAPS;
      lpContext: Pointer) : HResult; stdcall;

(*
 * INTERACES FOLLOW:
 *	IDirectDrawVideoPort
 *	IVideoPort
 *)


(*
 * IDirectDrawVideoPort
 *)
{$IFDEF D2COM}
  IDirectDrawVideoPort = class (IUnknown)
{$ELSE}
  IDirectDrawVideoPort = interface (IUnknown)
    ['{B36D93E0-2B43-11CF-A2DE-00AA00B93356}']
{$ENDIF}
    (*** IDirectDrawVideoPort methods ***)
    function Flip(lpDDSurface: IDirectDrawSurface; dwFlags: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetBandwidthInfo(const lpddpfFormat: TDDPixelFormat;
        dwWidth: DWORD; dwHeight: DWORD; dwFlags: DWORD;
        var lpBandwidth: DDVIDEOPORTBANDWIDTH) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetColorControls(var lpColorControl: TDDColorControl) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetInputFormats(var lpNumFormats: DWORD; var lpFormats:
        TDDPixelFormat; dwFlags: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetOutputFormats(const lpInputFormat: TDDPixelFormat;
        var lpNumFormats: DWORD; var lpFormats: TDDPixelFormat; dwFlags: DWORD)
        : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetFieldPolarity(var lpbVideoField: BOOL) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetVideoLine(var lpdwLine: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetVideoSignalStatus(varlpdwStatus: DWORD) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetColorControls(const lpColorControl: TDDColorControl) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function SetTargetSurface(lpDDSurface: IDirectDrawSurface; dwFlags: DWORD) :
        HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function StartVideo(const lpVideoInfo: DDVIDEOPORTINFO) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function StopVideo: HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function UpdateVideo(const lpVideoInfo: DDVIDEOPORTINFO) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function WaitForSync(dwFlags: DWORD; dwLine: DWORD; dwTimeout: DWORD) :
        HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
  end;

(*
 * IDirectDrawVideoPortContainer
 *)
{$IFDEF D2COM}
  IDDVideoPortContainer = class (IUnknown)
{$ELSE}
  IDDVideoPortContainer = interface (IUnknown)
    ['{6C142760-A733-11CE-A521-0020AF0BE560}']
{$ENDIF}
    (*** IDDVideoPortContainer methods ***)
    function CreateVideoPort(dwFlags: DWORD; const lpDDVideoPortDesc:
        DDVIDEOPORTDESC; var lplpDDVideoPort: IDirectDrawVideoPort;
        pUnkOuter: IUnknown) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function EnumVideoPorts(dwFlags: DWORD;
        const lpDDVideoPortCaps: DDVIDEOPORTCAPS; lpContext: Pointer;
        lpEnumVideoCallback: LPDDENUMVIDEOCALLBACK) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function GetVideoPortConnectInfo(dwPortId: DWORD; var lpNumEntries: DWORD;
        var lpConnectInfo: DDVIDEOPORTCONNECT) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
    function QueryVideoPortStatus(dwPortId: DWORD;
        var lpVPStatus: DDVIDEOPORTSTATUS) : HResult;
        {$IFDEF D2COM} virtual; stdcall; abstract; {$ELSE} stdcall; {$ENDIF}
  end;
  
implementation

end.
 