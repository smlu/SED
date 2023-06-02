unit dxtools;

{DXTools - delphi helper functions for managing DirectX things}
{(c) February 1997 by Luc Cluitmans}
{This source is provided free as part of an example of using Direct3D in Delphi.
 No guarantees; I am not responsible for nasty things that may happen to you
 or your computer by using this code}
{Sorry for the lack of documentary comments :-)}


interface

uses
  OLE2, Windows, Messages, SysUtils, D3DRMObj, D3DTypes, D3DRMDef, DDraw,
  D3D, D3Drm, Dialogs, misc_utils;

type
  EDirectX = class(Exception);
  
procedure COMRelease(var p: IUnknown); // safe release of COM object

{---------------------------------------------------------------------}
{Error handling}
function COMFailed(var r2: HRESULT; r: HRESULT): Boolean;
         {assigns r to r2; returns True when r indicates failure}
procedure DXCheck(r: HResult; const msg:string);
         {Throws a EDirectX exception if r indicates  a failure}
         {Returns normally when r is DD_OK or an equivalent}
{example 1: (special processing required on error)
   if COMFailed(r, lpDDClipper.SetHWnd(0, Handle)) then
   begin
     COMRelease(lpDDClipper);
     DXCheck(r);
   end;

 example 2: (no special processing required on error)
   DXCheck(Direct3DRMCreate(lpD3DRM));
}

Procedure DXFailCheck(r: HResult;const msg:string);

implementation

procedure COMRelease(var p: IUnknown);
var i:integer;
begin
  if Assigned(p) then
  begin
    p.Release;
    p := nil;
  end
  else
  begin
    ShowMessage('COMRelease of NULL object');
  end;
end;

function COMFailed(var r2: HRESULT; r: HRESULT): Boolean;
begin
  r2 := r;
  Result := (r <> DD_OK);
end;


Function GetErrorStr(r:Hresult):string;
var
  s: string;
begin
  Result:='';
  if r <> DD_OK then
  begin
    Case r of
        DDERR_ALREADYINITIALIZED: S:='This object is already initialized.';
        DDERR_BLTFASTCANTCLIP: S:=' if a clipper object is attached to the source surface passed into a BltFast call.';
        DDERR_CANNOTATTACHSURFACE: S:='This surface can not be attached to the requested surface.';
        DDERR_CANNOTDETACHSURFACE: S:='This surface can not be detached from the requested surface.';
        DDERR_CANTCREATEDC: S:='Windows can not create any more DCs.';
        DDERR_CANTDUPLICATE: S:='Cannot duplicate primary & 3D surfaces, or surfaces that are implicitly created.';
        DDERR_CLIPPERISUSINGHWND: S:='An attempt was made to set a cliplist for a clipper object that is already monitoring an hwnd.';
        DDERR_COLORKEYNOTSET: S:='No src color key specified for this operation.';
        DDERR_CURRENTLYNOTAVAIL: S:='Support is currently not available.';
        DDERR_DIRECTDRAWALREADYCREATED: S:='A DirectDraw object representing this driver has already been created for this process.';
        DDERR_EXCEPTION: S:='An exception was encountered while performing the requested operation.';
        DDERR_EXCLUSIVEMODEALREADYSET: S:='An attempt was made to set the cooperative level when it was already set to exclusive.';
        DDERR_GENERIC: S:='Generic failure.';
        DDERR_HEIGHTALIGN: S:='Height of rectangle provided is not a multiple of reqd alignment.';
        DDERR_HWNDALREADYSET: S:='The CooperativeLevel HWND has already been set. It can not be reset while the process has surfaces or palettes created.';
        DDERR_HWNDSUBCLASSED: S:='HWND used by DirectDraw CooperativeLevel has been subclassed, this prevents DirectDraw from restoring state.';
        DDERR_IMPLICITLYCREATED: S:='This surface can not be restored because it is an implicitly created surface.';
        DDERR_INCOMPATIBLEPRIMARY: S:='Unable to match primary surface creation request with existing primary surface.';
        DDERR_INVALIDCAPS: S:='One or more of the caps bits passed to the callback are incorrect.';
        DDERR_INVALIDCLIPLIST: S:='DirectDraw does not support the provided cliplist.';
        DDERR_INVALIDDIRECTDRAWGUID: S:='The GUID passed to DirectDrawCreate is not a valid DirectDraw driver identifier.';
        DDERR_INVALIDMODE: S:='DirectDraw does not support the requested mode.';
        DDERR_INVALIDOBJECT: S:='DirectDraw received a pointer that was an invalid DIRECTDRAW object.';
        DDERR_INVALIDPARAMS: S:='One or more of the parameters passed to the function are incorrect.';
        DDERR_INVALIDPIXELFORMAT: S:='The pixel format was invalid as specified.';
        DDERR_INVALIDPOSITION: S:='Returned when the position of the overlay on the destination is no longer legal for that destination.';
        DDERR_INVALIDRECT: S:='Rectangle provided was invalid.';
        DDERR_LOCKEDSURFACES: S:='Operation could not be carried out because one or more surfaces are locked.';
        DDERR_NO3D: S:='There is no 3D present.';
        DDERR_NOALPHAHW: S:='Operation could not be carried out because there is no alpha accleration hardware present or available.';
        DDERR_NOBLTHW: S:='No blitter hardware present.';
        DDERR_NOCLIPLIST: S:='No cliplist available.';
        DDERR_NOCLIPPERATTACHED: S:='No clipper object attached to surface object.';
        DDERR_NOCOLORCONVHW: S:='Operation could not be carried out because there is no color conversion hardware present or available.';
        DDERR_NOCOLORKEY: S:='Surface does not currently have a color key';
        DDERR_NOCOLORKEYHW: S:='Operation could not be carried out because there is no hardware support of the destination color key.';
        DDERR_NOCOOPERATIVELEVELSET: S:='Create function called without DirectDraw object method SetCooperativeLevel being called.';
        DDERR_NODC: S:='No DC was ever created for this surface.';
        DDERR_NODDROPSHW: S:='No DirectDraw ROP hardware.';
        DDERR_NODIRECTDRAWHW: S:='A hardware-only DirectDraw object creation was attempted but the driver did not support any hardware.';
        DDERR_NOEMULATION: S:='Software emulation not available.';
        DDERR_NOEXCLUSIVEMODE: S:='Operation requires the application to have exclusive mode but the application does not have exclusive mode.';
        DDERR_NOFLIPHW: S:='Flipping visible surfaces is not supported.';
        DDERR_NOGDI: S:='There is no GDI present.';
        DDERR_NOHWND: S:='Clipper notification requires an HWND or no HWND has previously been set as the CooperativeLevel HWND.';
        DDERR_NOMIRRORHW: S:='Operation could not be carried out because there is no hardware present or available.';
        DDERR_NOOVERLAYDEST: S:='Returned when GetOverlayPosition is called on an overlay that UpdateOverlay has never been called on to establish a destination.';
        DDERR_NOOVERLAYHW: S:='Operation could not be carried out because there is no overlay hardware present or available.';
        DDERR_NOPALETTEATTACHED: S:='No palette object attached to this surface.';
        DDERR_NOPALETTEHW: S:='No hardware support for 16 or 256 color palettes.';
        DDERR_NORASTEROPHW: S:='Operation could not be carried out because there is no appropriate raster op hardware present or available.';
        DDERR_NOROTATIONHW: S:='Operation could not be carried out because there is no rotation hardware present or available.';
        DDERR_NOSTRETCHHW: S:='Operation could not be carried out because there is no hardware support for stretching.';
        DDERR_NOT4BITCOLOR: S:='DirectDrawSurface is not in 4 bit color palette and the requested operation requires 4 bit color palette.';
        DDERR_NOT4BITCOLORINDEX: S:='DirectDrawSurface is not in 4 bit color index palette and the requested operation requires 4 bit color index palette.';
        DDERR_NOT8BITCOLOR: S:='DirectDrawSurface is not in 8 bit color mode and the requested operation requires 8 bit color.';
        DDERR_NOTAOVERLAYSURFACE: S:='Returned when an overlay member is called for a non-overlay surface.';
        DDERR_NOTEXTUREHW: S:='Operation could not be carried out because there is no texture mapping hardware present or available.';
        DDERR_NOTFLIPPABLE: S:='An attempt has been made to flip a surface that is not flippable.';
        DDERR_NOTFOUND: S:='Requested item was not found.';
        DDERR_NOTLOCKED: S:='Surface was not locked.  An attempt to unlock a surface that was not locked at all, or by this process, has been attempted.';
        DDERR_NOTPALETTIZED: S:='The surface being used is not a palette-based surface.';
        DDERR_NOVSYNCHW: S:='Operation could not be carried out because there is no hardware support for vertical blank synchronized operations.';
        DDERR_NOZBUFFERHW: S:='Operation could not be carried out because there is no hardware support for zbuffer blitting.';
        DDERR_NOZOVERLAYHW: S:='Overlay surfaces could not be z layered based on their BltOrder because the hardware does not support z layering of overlays.';
        DDERR_OUTOFCAPS: S:='The hardware needed for the requested operation has already been allocated.';
        DDERR_OUTOFMEMORY: S:='DirectDraw does not have enough memory to perform the operation.';
        DDERR_OUTOFVIDEOMEMORY: S:='DirectDraw does not have enough memory to perform the operation.';
        DDERR_OVERLAYCANTCLIP: S:='The hardware does not support clipped overlays.';
        DDERR_OVERLAYCOLORKEYONLYONEACTIVE: S:='Can only have ony color key active at one time for overlays.';
        DDERR_OVERLAYNOTVISIBLE: S:='Returned when GetOverlayPosition is called on a hidden overlay.';
        DDERR_PALETTEBUSY: S:='Access to this palette is being refused because the palette is already locked by another thread.';
        DDERR_PRIMARYSURFACEALREADYEXISTS: S:='This process already has created a primary surface.';
        DDERR_REGIONTOOSMALL: S:='Region passed to Clipper::GetClipList is too small.';
        DDERR_SURFACEALREADYATTACHED: S:='This surface is already attached to the surface it is being attached to.';
        DDERR_SURFACEALREADYDEPENDENT: S:='This surface is already a dependency of the surface it is being made a dependency of.';
        DDERR_SURFACEBUSY: S:='Access to this surface is being refused because the surface is already locked by another thread.';
        DDERR_SURFACEISOBSCURED: S:='Access to surface refused because the surface is obscured.';
        DDERR_SURFACELOST: S:='Access to this surface is being refused because the surface memory is gone. The DirectDrawSurface object representing this surface should have Restore called on it.';
        DDERR_SURFACENOTATTACHED: S:='The requested surface is not attached.';
        DDERR_TOOBIGHEIGHT: S:='Height requested by DirectDraw is too large.';
        DDERR_TOOBIGSIZE: S:='Size requested by DirectDraw is too large, but the individual height and width are OK.';
        DDERR_TOOBIGWIDTH: S:='Width requested by DirectDraw is too large.';
        DDERR_UNSUPPORTED: S:='Action not supported.';
        DDERR_UNSUPPORTEDFORMAT: S:='FOURCC format requested is unsupported by DirectDraw.';
        DDERR_UNSUPPORTEDMASK: S:='Bitmask in the pixel format requested is unsupported by DirectDraw.';
        DDERR_VERTICALBLANKINPROGRESS: S:='Vertical blank is in progress.';
        DDERR_WASSTILLDRAWING: S:='Informs DirectDraw that the previous Blt which is transfering information to or from this Surface is incomplete.';
        DDERR_WRONGMODE: S:='This surface can not be restored because it was created in a different mode.';
        DDERR_XALIGN: S:='Rectangle provided was not horizontally aligned on required boundary.';
        D3DERR_BADMAJORVERSION: S:='D3DERR_BADMAJORVERSION';
        D3DERR_BADMINORVERSION: S:='D3DERR_BADMINORVERSION';
        D3DERR_EXECUTE_LOCKED: S:='D3DERR_EXECUTE_LOCKED';
        D3DERR_EXECUTE_NOT_LOCKED: S:='D3DERR_EXECUTE_NOT_LOCKED';
        D3DERR_EXECUTE_CREATE_FAILED: S:='D3DERR_EXECUTE_CREATE_FAILED';
        D3DERR_EXECUTE_DESTROY_FAILED: S:='D3DERR_EXECUTE_DESTROY_FAILED';
        D3DERR_EXECUTE_LOCK_FAILED: S:='D3DERR_EXECUTE_LOCK_FAILED';
        D3DERR_EXECUTE_UNLOCK_FAILED: S:='D3DERR_EXECUTE_UNLOCK_FAILED';
        D3DERR_EXECUTE_FAILED: S:='D3DERR_EXECUTE_FAILED';
        D3DERR_EXECUTE_CLIPPED_FAILED: S:='D3DERR_EXECUTE_CLIPPED_FAILED';
        D3DERR_TEXTURE_NO_SUPPORT: S:='D3DERR_TEXTURE_NO_SUPPORT';
        D3DERR_TEXTURE_NOT_LOCKED: S:='D3DERR_TEXTURE_NOT_LOCKED';
        D3DERR_TEXTURE_LOCKED: S:='D3DERR_TEXTURELOCKED';
        D3DERR_TEXTURE_CREATE_FAILED: S:='D3DERR_TEXTURE_CREATE_FAILED';
        D3DERR_TEXTURE_DESTROY_FAILED: S:='D3DERR_TEXTURE_DESTROY_FAILED';
        D3DERR_TEXTURE_LOCK_FAILED: S:='D3DERR_TEXTURE_LOCK_FAILED';
        D3DERR_TEXTURE_UNLOCK_FAILED: S:='D3DERR_TEXTURE_UNLOCK_FAILED';
        D3DERR_TEXTURE_LOAD_FAILED: S:='D3DERR_TEXTURE_LOAD_FAILED';
        D3DERR_MATRIX_CREATE_FAILED: S:='D3DERR_MATRIX_CREATE_FAILED';
        D3DERR_MATRIX_DESTROY_FAILED: S:='D3DERR_MATRIX_DESTROY_FAILED';
        D3DERR_MATRIX_SETDATA_FAILED: S:='D3DERR_MATRIX_SETDATA_FAILED';
        D3DERR_SETVIEWPORTDATA_FAILED: S:='D3DERR_SETVIEWPORTDATA_FAILED';
        D3DERR_MATERIAL_CREATE_FAILED: S:='D3DERR_MATERIAL_CREATE_FAILED';
        D3DERR_MATERIAL_DESTROY_FAILED: S:='D3DERR_MATERIAL_DESTROY_FAILED';
        D3DERR_MATERIAL_SETDATA_FAILED: S:='D3DERR_MATERIAL_SETDATA_FAILED';
        D3DERR_LIGHT_SET_FAILED: S:='D3DERR_LIGHT_SET_FAILED';
        D3DRMERR_BADOBJECT: S:='D3DRMERR_BADOBJECT';
        D3DRMERR_BADTYPE: S:='D3DRMERR_BADTYPE';
        D3DRMERR_BADALLOC: S:='D3DRMERR_BADALLOC';
        D3DRMERR_FACEUSED: S:='D3DRMERR_FACEUSED';
        D3DRMERR_NOTFOUND: S:='D3DRMERR_NOTFOUND';
        D3DRMERR_NOTDONEYET: S:='D3DRMERR_NOTDONEYET';
        D3DRMERR_FILENOTFOUND: S:='The file was not found.';
        D3DRMERR_BADFILE: S:='D3DRMERR_BADFILE';
        D3DRMERR_BADDEVICE: S:='D3DRMERR_BADDEVICE';
        D3DRMERR_BADVALUE: S:='D3DRMERR_BADVALUE';
        D3DRMERR_BADMAJORVERSION: S:='D3DRMERR_BADMAJORVERSION';
        D3DRMERR_BADMINORVERSION: S:='D3DRMERR_BADMINORVERSION';
        D3DRMERR_UNABLETOEXECUTE: S:='D3DRMERR_UNABLETOEXECUTE';
        Else S:='Unrecognized error value.';
    end;

    S := Format ('DirectX call failed: %x%s%s', [r, #13, s]);
    Result:=s;
    {raise EDirectX.Create (S);}
  end;
end;

Procedure DXFailCheck(r: HResult;const msg:string);
var s:string;
begin
 if r=DD_OK then exit;
 raise EDirectX.Create (GetErrorStr(r)+' '+msg);
end;

procedure DXCheck(r: HResult; const msg:string);
begin
 if r=DD_OK then exit;
 PanMessage(mt_warning,GetErrorStr(r)+' '+msg);
end;

end.
