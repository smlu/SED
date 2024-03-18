{$A1,B-,C-,D+,E-,F-,G+,H+,I+,J+,K-,L+,M-,N+,O+,P+,Q+,R+,S+,T-,U+,V+,W-,X+,Y+,Z1}
{$MINSTACKSIZE $00004000}
{$MAXSTACKSIZE $00100000}
{$IMAGEBASE $00400000}
{$APPTYPE GUI}
{$WARN SYMBOL_DEPRECATED ON}
{$WARN SYMBOL_LIBRARY ON}
{$WARN SYMBOL_PLATFORM ON}
{$WARN SYMBOL_EXPERIMENTAL ON}
{$WARN UNIT_LIBRARY ON}
{$WARN UNIT_PLATFORM ON}
{$WARN UNIT_DEPRECATED ON}
{$WARN UNIT_EXPERIMENTAL ON}
{$WARN HRESULT_COMPAT ON}
{$WARN HIDING_MEMBER ON}
{$WARN HIDDEN_VIRTUAL ON}
{$WARN GARBAGE ON}
{$WARN BOUNDS_ERROR ON}
{$WARN ZERO_NIL_COMPAT ON}
{$WARN STRING_CONST_TRUNCED ON}
{$WARN FOR_LOOP_VAR_VARPAR ON}
{$WARN TYPED_CONST_VARPAR ON}
{$WARN ASG_TO_TYPED_CONST ON}
{$WARN CASE_LABEL_RANGE ON}
{$WARN FOR_VARIABLE ON}
{$WARN CONSTRUCTING_ABSTRACT ON}
{$WARN COMPARISON_FALSE ON}
{$WARN COMPARISON_TRUE ON}
{$WARN COMPARING_SIGNED_UNSIGNED ON}
{$WARN COMBINING_SIGNED_UNSIGNED ON}
{$WARN UNSUPPORTED_CONSTRUCT ON}
{$WARN FILE_OPEN ON}
{$WARN FILE_OPEN_UNITSRC ON}
{$WARN BAD_GLOBAL_SYMBOL ON}
{$WARN DUPLICATE_CTOR_DTOR ON}
{$WARN INVALID_DIRECTIVE ON}
{$WARN PACKAGE_NO_LINK ON}
{$WARN PACKAGED_THREADVAR ON}
{$WARN IMPLICIT_IMPORT ON}
{$WARN HPPEMIT_IGNORED ON}
{$WARN NO_RETVAL ON}
{$WARN USE_BEFORE_DEF ON}
{$WARN FOR_LOOP_VAR_UNDEF ON}
{$WARN UNIT_NAME_MISMATCH ON}
{$WARN NO_CFG_FILE_FOUND ON}
{$WARN IMPLICIT_VARIANTS ON}
{$WARN UNICODE_TO_LOCALE ON}
{$WARN LOCALE_TO_UNICODE ON}
{$WARN IMAGEBASE_MULTIPLE ON}
{$WARN SUSPICIOUS_TYPECAST ON}
{$WARN PRIVATE_PROPACCESSOR ON}
{$WARN UNSAFE_TYPE OFF}
{$WARN UNSAFE_CODE OFF}
{$WARN UNSAFE_CAST OFF}
{$WARN OPTION_TRUNCATED ON}
{$WARN WIDECHAR_REDUCED ON}
{$WARN DUPLICATES_IGNORED ON}
{$WARN UNIT_INIT_SEQ ON}
{$WARN LOCAL_PINVOKE ON}
{$WARN MESSAGE_DIRECTIVE ON}
{$WARN TYPEINFO_IMPLICITLY_ADDED ON}
{$WARN RLINK_WARNING ON}
{$WARN IMPLICIT_STRING_CAST ON}
{$WARN IMPLICIT_STRING_CAST_LOSS ON}
{$WARN EXPLICIT_STRING_CAST OFF}
{$WARN EXPLICIT_STRING_CAST_LOSS OFF}
{$WARN CVT_WCHAR_TO_ACHAR ON}
{$WARN CVT_NARROWING_STRING_LOST ON}
{$WARN CVT_ACHAR_TO_WCHAR ON}
{$WARN CVT_WIDENING_STRING_LOST ON}
{$WARN NON_PORTABLE_TYPECAST ON}
{$WARN XML_WHITESPACE_NOT_ALLOWED ON}
{$WARN XML_UNKNOWN_ENTITY ON}
{$WARN XML_INVALID_NAME_START ON}
{$WARN XML_INVALID_NAME ON}
{$WARN XML_EXPECTED_CHARACTER ON}
{$WARN XML_CREF_NO_RESOLVE ON}
{$WARN XML_NO_PARM ON}
{$WARN XML_NO_MATCHING_PARM ON}
{$WARN IMMUTABLE_STRINGS OFF}

unit PRender;

interface

uses u_pj3dos, Geometry, Classes, J_Level, Forms, Windows, Messages,
     Images, graph_files, Graphics, files, fileOperations, misc_utils,
     lev_utils, sysUtils, GlobalVars, ExtCtrls, Generics.Collections,
     Generics.Defaults, Jed_Main, System.Math, RenderUtils;

type

T3DPTexture=class
  name: string;  // material name
  cmpname: string;
  width, height: integer;
  ci: TColorInfo;
  {Procedure SetCurrent;virtual;abstract;}
end;

T3DPVertex=record
  x,y,z:single;
end;

TVXdets = class
  {vx:^T3DPVertex;}
  x, y, z: single;
  u, v: single;
  intensity: TColorF;
end;

T3DPoly=class
  faceflags: Longint;
  geo: TGeoMode;
  light: TLightMode;

  ambientLight: TColorF; // sector colors
  extraLight: TColorF;   // face light color
  tint: TColorF;         // sector tint

  normal: TVector;
  vxds: TList;
  tx: T3DPTexture;

  distance: Double; // Distance from camera

  Constructor Create;
  Destructor Destroy; override;
  Function AddVXD:TVXDets;
  Function GetVXD(n:integer): TVXDets;
  Procedure RecalcDistance;

  Function GetLitColor(color: TColorF): TColorF;
end;

T3DPMesh = class
  polys:  TList;
  center: T3DPVertex;
  radius: single;

  sec: TJKSector;
  extraLight: TColorF;

  Constructor Create;
  Constructor Cube(size: float; color: TColorF);
  Destructor Destroy;override;
  Function GetPoly(n: integer): T3DPoly;
  Function AddPoly: T3DPoly;
  Procedure CalculateSphere;
end;

TMeshes = class(TList)
  Function GetItem(n: integer): T3DPMesh;
  Procedure SetItem(n: integer; v: T3DPMesh);
  Property Items[n: integer]: T3DPMesh read GetItem write SetItem; default;
end;




TPickType = (pk_nothing, pk_surface, pk_thing);



TPreviewRender = class
  whandle:HWND;
  vwidth,vheight:integer;
  gamma:double;
  fog: TFog;
  fogDensity: Double;
  SelSC:TJKSector;
  SelSF:integer;
  SelTH:TJKThing;
  thing_cmp:string;

  geoMode: TGeoMode;
  lightMode: TLightMode;

  wfColor: TColorF; // wireframe color
  selectionColor: TColorF;
  multipleSelectionColor: TColorF;
  floorHighlightColor: TColorF;
  solidSurfaceHighlightColor: TColorF;
  specialSurfaceHighlightColor: TColorF;
  debugPolysColor: TColorF;

  highlightSelection: Boolean;
  noClipSelectionOutline: Boolean;
  highlightSolidFloor: Boolean;
  highlightSolidSurface: Boolean;
  highlightSpecialSurface: Boolean;

  Constructor Create(aForm: TForm; geoMode: TGeoMode = Texture; lightMode: TLightMode = Gouraud);
  Constructor CreateFromPanel(aPanel:TPanel; geoMode: TGeoMode = Texture; lightMode: TLightMode = Gouraud);
  Destructor Destroy; override;

  Procedure Initialize;virtual;abstract;
  Procedure Reset; virtual;
  Procedure ClearSectors;virtual;abstract;
  Procedure ClearThings;virtual;abstract;
  Procedure AddSector(sec: TJKSector);virtual;abstract;
  Procedure DeleteSector(sec:TJKSector);virtual;abstract;
  Procedure UpdateSector(sec:TJKSector);virtual;abstract;
  Procedure SyncSector(sec:TJKSector);virtual;abstract; // updates or adds sector
  Procedure AddThing(th:TJKThing);virtual;abstract;
  Procedure DeleteThing(th:TJKThing);virtual;abstract;
  Procedure UpdateThing(th:TJKThing);virtual;abstract;
  Procedure SyncThing(th:TJKThing);virtual;abstract; // updates or adds thing
  Procedure SetViewPort(x,y,w,h:integer);virtual;abstract;
  Procedure EnableZTest(enable: Boolean);virtual; abstract;
  Procedure DrawPolys(const [Ref] polys: TArray<T3DPoly>; count: Integer = -1); virtual;abstract;
  Procedure Redraw;virtual;abstract;
  Procedure HandleActivate(const msg:TMessage);virtual;abstract;
  procedure HandlePaint(hdc:HDC);virtual;abstract;
  Function PickAt(X,Y:integer):TPickType;virtual;abstract;
  Function SetGamma(gamma:double):integer;virtual;abstract;

  Procedure SetViewToThing(th:TJKThing; zoomOut: Double = 0.0; pitchOffset: double = 0; yawOffset: double = 0); virtual; abstract;
  {Arranges camera so that the camera looks at the thing with max possible size}
  Procedure SetThing3DO(th:TJKThing;a3do:TPJ3DO);virtual;abstract;
  Procedure SetClearColor(color: TColorF);virtual;abstract;

  Function IsFogSupported(): Boolean;virtual;abstract;
  Procedure EnableFog(enable: Boolean);virtual;
  Procedure SetFog(color: TColorF; fogStart, fogEnd: double; density: Double = 1.0);virtual;

  Function GetCamera(): TCamera;
  Property camera: TCamera read GetCamera;

  protected
    curCamera: TCamera;

    Procedure Init(handle: HWND; width, height:integer; geoMode: TGeoMode; lightMode: TLightMode);
    Function IsSpecialSurface(surf: TJKSurface): Boolean;
    Function IsSpecial3doPoly(face: TPolygon): Boolean;
end;


TRDPolyCache = class
  sort: Boolean; // sort polys in list by distance before draw

  Constructor Create(const renderer: TPreviewRender; const cacheSize: Cardinal);
  Destructor Destroy;
  Procedure AddPoly(const poly: T3DPoly);
  Procedure Flush; // Pushes cached polys to renderer to draw
  Function GetCount: Cardinal;
  Function GetCapacity: Cardinal;

  property Count: Cardinal read GetCount; // Number of entries used in cache
  property Capacity: Cardinal read GetCapacity;  // The cache size

private
  sfunc: IComparer<T3DPoly>;
  polys: TArray<T3DPoly>;
  numUsed: Cardinal;
  renderer: TPreviewRender;
end;

TNewPRenderer = class(TPreviewRender)
  TXList,CmpList:TStringList;
  sectors: TSectors;
  things: TThings;
  masterPalSet: boolean;
  MasterPal: TCMPPal;

  ThList, SCList: TMeshes;
  //tn,bn,ln,rn:TVector; {Normals of clipping planes}

  Constructor Create(aForm:TForm; geoMode: TGeoMode = Texture; lightMode: TLightMode = Gouraud);
  Constructor CreateFromPanel(aPanel:TPanel; geoMode: TGeoMode = Texture; lightMode: TLightMode = Gouraud);
  Destructor Destroy; override;

  Procedure EnableDebug(enabled: Boolean);

  Procedure ClearSectors; override;
  Procedure ClearThings; override;
  Procedure AddSector(sec: TJKSector); override;
  Procedure DeleteSector(sec: TJKSector); override;
  Procedure UpdateSector(sec: TJKSector); override;
  Procedure SyncSector(sec:TJKSector); override; // updates or adds sector
  Procedure AddThing(th: TJKThing); override;
  Procedure DeleteThing(th: TJKThing); override;
  Procedure UpdateThing(th: TJKThing); override;
  Procedure SyncThing(th: TJKThing); override; // updates or adds sector

  Procedure SetViewToThing(th: TJKThing; zoomOut: Double = 0.0; pitchOffset: double = 0; yawOffset: double = 0); override;

  Procedure Redraw; override;

  Function PickAt(X,Y:integer):TPickType; override;
  Function SetGamma(gamma:double):integer; override;

  {Virtuals}

  Function LoadTexture(const name: string; const pal: PTCMPPal; const cmp :PTCMPTable): T3DPTexture; virtual; abstract;
  //Procedure DrawMesh(m:T3DPMesh);virtual;abstract;
  Procedure GetWorldLine(X,Y:integer;var X1,Y1,Z1,X2,Y2,Z2:double);virtual;abstract;
  {Must also override Initialize;}

  { Optionally Destroy; redraw; and }

  Procedure HandleActivate(const msg:TMessage);override;
  procedure HandlePaint(hdc:HDC);override;

  Procedure ClearTXList;virtual;
  Procedure SetThing3DO(th:TJKThing;a3do:TPJ3DO);override;

Protected
  class Function IsInFront(const p,n: TVector; const c: T3DPVertex; radius: single) : Integer; overload; static;
  class Function IsInFront(const [ref] f: TClipFrustum; m: T3DPMesh): Boolean; overload; static;
  Function GetCameraFrustum: TClipFrustum;

Private
  rdCache: TRDPolyCache; // cache od opaque polys
  rdAlphaCache: TRDPolyCache;
  rdAlphaAdjoinsPolys: TList<T3DPoly>;
  rdFloorPolys: TList<T3DPoly>;
  rdSolidSurfacePolys: TList<T3DPoly>;
  rdSpecialSurfacePolys: TList<T3DPoly>;

  // Sky variables
  ceilingSkyZ: TVector;
  ceilingSkyNormal: TVector;
  horizonSkyPPR: double; // pixels per rev.
  horizonSkyDistance: double;
  horizonSkyScale: double;
  horizonSkyRollSin: double;
  horizonSkyRollCos: double;
  horizonSkyYaw: double;
  horizonSkyPitch: double;

  bDebug: Boolean;
  rdDebugPolys: TList<T3DPoly>;
  //rdDebugPickedPoly: T3DPoly;

  const rdPolysCacheCapacity = 1024;

  Procedure Init;
  Function GetTexture(const name,cmp: string):T3DPTexture;
  Procedure LoadMasterPal(l: TJKLevel);
  Function SecToMesh(sec: TJKSector):T3DPMesh;
  Function ThingToMesh(th: TJKThing):T3DPMesh;
  Function BadThingMesh(th: TJKThing):T3DPMesh;
  Function A3DOToMesh(th: TJKThing; a3do: TPJ3DO):T3DPMesh;
  Procedure RenderCircle(polys: TList<T3DPoly>; cx, cy, cz, rad: double);

  Procedure UpdateSky();
  Procedure UpdateCeilingSkySurface(const surf: T3DPoly);
  Function UpdateHorizonSkySurface(const surf: T3DPoly): T3DPoly;

  Procedure RenderWorldGeometry(const [Ref] frustrum: TClipFrustum);
  Procedure RenderThings(const [Ref] frustrum: TClipFrustum);
  Procedure RenderAlphaAdjoins;
  Procedure RenderFloorWireframe;
  Procedure RenderSolidSurfaceWireframe;
  Procedure RenderSpecialSurfaceWireframe;
  Procedure RenderDebugPolys;
  Procedure RenderSelection(const [Ref] frustrum: TClipFrustum);
  Procedure RenderWireframe(const polys: TList<T3DPoly>; wireColor: TColorF);
  Procedure FlushCaches; // flushes rdCach & rdAlphaCache
end;


implementation
uses math;

Constructor TPreviewRender.Create(aForm: TForm; geoMode: TGeoMode; lightMode: TLightMode);
begin
  init(aForm.Handle, aForm.ClientWidth, aForm.ClientHeight, geoMode, lightMode);
end;

Constructor TPreviewRender.CreateFromPanel(aPanel:TPanel; geoMode: TGeoMode; lightMode: TLightMode);
begin
  init(aPanel.Handle, aPanel.ClientWidth, aPanel.ClientHeight, geoMode, lightMode);
end;

Destructor TPreviewRender.Destroy;
begin
  curCamera.Free;
end;

Procedure TPreviewRender.Reset;
begin
  curCamera.Reset;
  ClearThings;
  ClearSectors;
end;

Procedure TPreviewRender.Init(handle: HWND; width, height:integer; geoMode: TGeoMode; lightMode: TLightMode);
begin
  whandle := handle;
  vwidth  := width;
  vheight := height;

  curCamera := TCamera.CreatePerspective(width, height, 90, 0.01, 5000.0); // jones3D engine 0.0156 & 64.0

  gamma      := 1;
  fogDensity := 1.0;
  thing_cmp  := 'dflt.cmp';

  self.geoMode   := geoMode;
  self.lightMode := lightMode;

  wfColor                      := ColorWhite;
  selectionColor               := ColorYellow;
  multipleSelectionColor       := ColorCyan;
  floorHighlightColor          := ColorRed;
  solidSurfaceHighlightColor   := ColorRedDark;
  specialSurfaceHighlightColor := ColorGreen;
  debugPolysColor              := ColorRed;

  highlightSelection      := False;
  noClipSelectionOutline  := False;
  highlightSolidFloor     := False;
  highlightSolidSurface   := False;
  highlightSpecialSurface := False;
end;

Function TPreviewRender.IsSpecialSurface(surf: TJKSurface): Boolean;
begin
  var surfMask := 0;
  var faceMask := 0;
  if CurrentProject = IJIM then
    begin
      surfMask := SF_CogLinked
        or SF_IJIM_KillFloor
        or SF_IJIM_Climable
        or SF_IJIM_Track
        or SF_IJIM_Ledge
        or SF_IJIM_WaterLedge
        or SF_IJIM_WhipAim;
    end
  else // JKDF2 & MOTS
    begin
      surfMask := SF_CogLinked
    end;
  Result := (surf.SurfFlags and surfMask) <> 0;
end;

Function TPreviewRender.IsSpecial3doPoly(face: TPolygon): Boolean;
begin
  var faceMask := 0;
  if CurrentProject = IJIM then
    begin
      faceMask := FF_IJIM_3doLedge or FF_IJIM_3doWhipAim;
    end
  else // JKDF2 & MOTS
    begin
    end;
  Result := (face.FaceFlags and faceMask) <> 0;
end;


Procedure TPreviewRender.EnableFog(enable: Boolean);
begin
  if not IsFogSupported() then
    fog.Enabled := false
  else
    fog.Enabled := enable;
end;

Procedure TPreviewRender.SetFog(color: TColorF; fogStart, fogEnd: double; density: Double = 1.0);
begin
  fog.color    := color;
  fog.FogStart := FogStart;
  fog.FogEnd   := FogEnd;
  fogDensity   := density;
end;

Function TPreviewRender.GetCamera(): TCamera;
begin
  Result := curCamera;
end;


Constructor TRDPolyCache.Create(const renderer: TPreviewRender; const cacheSize: Cardinal);
begin
  self.renderer := renderer;
  polys := TArray<T3DPoly>.Create();
  SetLength(polys, cacheSize);
  sfunc := TComparer<T3DPoly>.Construct(
    function(const left, right: T3DPoly): Integer
    begin
      Result := 0;
      if (left = nil) or (right = nil) then
        exit;

      if right.distance = left.distance then
        exit;

      Result := 1;
      if right.distance <= left.distance then
        Result := -1;
    end
  );
end;

Destructor TRDPolyCache.Destroy;
begin
  if numUsed > 0 then Flush;
  self.renderer := nil;
end;

Procedure TRDPolyCache.AddPoly(const poly: T3DPoly);
begin
  if numUsed >= Capacity then Flush;
  polys[numUsed] := poly;
  Inc(numUsed);
end;

Procedure TRDPolyCache.Flush;
begin
  if numUsed > 0 then
  begin
    if sort then TArray.Sort<T3DPoly>(polys, sfunc, 0, numUsed - 1);
    renderer.DrawPolys(polys, numUsed);
    numUsed := 0;
  end;
end;

Function TRDPolyCache.GetCapacity: Cardinal;
begin
  Result := Length(polys);
end;

Function TRDPolyCache.GetCount: Cardinal;
begin
  Result := numUsed;
end;

procedure DisableFPUExceptions ;
var
  FPUControlWord: WORD ;
asm
  FSTCW   FPUControlWord ;
  OR      FPUControlWord, $4 + $1 ; { Divide by zero + invalid operation }
  FLDCW   FPUControlWord ;
end ;

Constructor TNewPRenderer.Create(aForm: TForm; geoMode: TGeoMode; lightMode: TLightMode);
begin
  Inherited Create(aForm, geoMode, lightMode);
  init;
end;

Constructor TNewPRenderer.CreateFromPanel(aPanel:TPanel; geoMode: TGeoMode; lightMode: TLightMode);
begin
  Inherited CreateFromPanel(aPanel, geoMode, lightMode);
  init;
// TxList:=TStringList.Create;
// TXList.Sorted:=true;
// CmpList:=TStringList.Create;
// CmpList.Sorted:=true;
// Sectors:=TSectors.Create;
// scList:=TMeshes.Create;
// thList:=TMeshes.Create;
// things:=TThings.Create;
// WHandle:=aPanel.Handle;
end;

Procedure TNewPRenderer.EnableDebug(enabled: Boolean);
begin
     bDebug := enabled;
end;

Procedure TNewPRenderer.Init;
begin
  bDebug := False;

  TxList := TStringList.Create;
  TXList.Sorted := true;

  CmpList := TStringList.Create;
  CmpList.Sorted := true;

  sectors := TSectors.Create;
  scList  := TMeshes.Create;
  thList  := TMeshes.Create;
  things  := TThings.Create;
  //WHandle:=aForm.Handle;

  rdCache := TRdPolyCache.Create(self, 2048);
  rdCache.sort := False;

  rdAlphaCache := TRdPolyCache.Create(self, 2048);
  rdAlphaCache.sort := True;

  rdAlphaAdjoinsPolys := TList<T3DPoly>.Create();
  rdAlphaAdjoinsPolys.Capacity := rdPolysCacheCapacity;

  rdFloorPolys := TList<T3DPoly>.Create();
  rdFloorPolys.Capacity := rdPolysCacheCapacity;

  rdSolidSurfacePolys := TList<T3DPoly>.Create();
  rdSolidSurfacePolys.Capacity := rdPolysCacheCapacity;

  rdSpecialSurfacePolys := TList<T3DPoly>.Create();
  rdSpecialSurfacePolys.Capacity := rdPolysCacheCapacity;

  rdDebugPolys := TList<T3DPoly>.Create();
  rdDebugPolys.Capacity := rdPolysCacheCapacity;

  ceilingSkyNormal := TVector.down;
end;

Destructor TNewPRenderer.Destroy;
begin
  //FlushCaches;
  rdDebugPolys.Free;
  rdSpecialSurfacePolys.Free;
  rdSolidSurfacePolys.Free;
  rdFloorPolys.Free;
  rdAlphaAdjoinsPolys.Free;
  rdAlphaCache.Free;
  rdCache.Free;

  ClearTXList;
  ClearThings;
  ClearSectors;
  THList.Free;
  SCList.Free;

  TxList.Free;
  CmpList.Free;
  sectors.Free;
  things.Free;
  inherited Destroy;
end;

Procedure TNewPRenderer.ClearTXList;
var i:integer;
begin
 for i:=0 to TXList.Count-1 do TXList.Objects[i].Free;
 TXList.Clear;
 for i:=0 to CMPList.Count-1 do FreeMem(Pointer(CMPList.Objects[i]));
 CMPList.Clear;
end;

Function Min(i1,i2:integer):integer;
begin
 if i1 < i2 then result := i1 else result := i2;
end;

Procedure AdjustPalGamma(var pal:TCMPPal;gamma:double);
var i:integer;
begin
 for i := 0 to 255 do
   With Pal[i] do
     begin
      r := Min(Round(r * gamma), 255);
      g := Min(Round(g * gamma), 255);
      b := Min(Round(b * gamma), 255);
     end;
end;

Function TNewPRenderer.SetGamma(gamma:double):integer;
var
   i,j,ic:integer;
   {p cmp:^TCMPPal;}
   f:tfile;
   gmult:double;
begin
 MasterPalSet:=false;
 ClearTXList;
 self.gamma:=gamma;

 result:=1;


 // if TXList.Count<>0 then
//  PanMessage(mt_warning,'Gamma change will take effect after you reload 3D preview');
{ gmult:=gamma;
 for i:=0 to TXList.count-1 do
 if TXList.Objects[i]<>nil then
 With TOGLTexture(TXList.Objects[i]) do
 begin
  ic:=CmpList.IndexOf(cmpname);
  pcmp:=Pointer(CmpList.Objects[ic]);
  cmp:=pcmp^;
  AdjustPalGamma(cmp,gamma);
  SetCurrent;

  pSubPalProc(GL_TEXTURE_2D, 0, 256, GL_RGB, GL_UNSIGNED_BYTE, cmp );
 end;}
end;

Procedure TNewPRenderer.ClearSectors;
var i:integer;
begin
 MasterPalSet:=false;
 ClearTXList;
 sectors.Clear;
 for i := 0 to scList.Count - 1 do scList[i].Free;
 scList.Clear;
end;

Procedure TNewPRenderer.ClearThings;
var i:integer;
begin
 things.Clear;
 for i := 0 to thList.Count - 1 do thList[i].Free;
 thList.Clear;
end;

Procedure TNewPRenderer.AddSector(sec: TJKSector);
var m:T3DPMesh;
begin
 var i := sectors.IndexOf(sec);
 if i <> -1 then exit;

 m := SecToMesh(sec);
 scList.Add(m);
 sectors.Add(sec);
end;

Procedure TNewPRenderer.DeleteSector(sec: TJKSector);
var i:integer;
begin
 i := sectors.IndexOf(sec);
 if i < 0 then exit;
 scList[i].Free;
 sectors.Delete(i);
 scList.Delete(i);
end;

Procedure TNewPRenderer.UpdateSector(sec: TJKSector);
var i,n:integer;
    m:T3DPMesh;
begin
 i := sectors.IndexOf(sec);
 if i < 0 then exit;
 scList[i].Free;
 m := SecToMesh(sec);
 scList[i] := m;
end;

Procedure TNewPRenderer.SyncSector(sec: TJKSector);
begin
  if sectors.IndexOf(sec) > -1 then
    UpdateSector(sec)
  else
    AddSector(sec);
end;

Procedure TNewPRenderer.AddThing(th: TJKThing);
var m:T3DPMesh;
begin
 m := ThingToMesh(th);
 if m = nil then m := BadThingMesh(th);
 ThList.Add(m);
 Things.Add(th);
end;

Procedure TNewPRenderer.DeleteThing(th: TJKThing);
var i:integer;
begin
 i := things.IndexOf(th);
 if i < 0 then exit;
 thList[i].Free;
 Things.Delete(i);
 ThList.Delete(i);
end;

Procedure TNewPRenderer.UpdateThing(th: TJKThing);
var i,n:integer;
    m:T3DPMesh;
begin
 i := Things.IndexOf(th);
 if i < 0 then exit;

 thList[i].Free;
 m := ThingToMesh(th);
 if m = nil then
   m := BadThingMesh(th);
 ThList[i] := m;
end;

Procedure TNewPRenderer.SyncThing(th: TJKThing);
begin
  if Things.IndexOf(th) > -1 then
    UpdateThing(th)
  else
    AddThing(th);
end;

Procedure TNewPRenderer.SetThing3DO(th:TJKThing;a3do:TPJ3DO);
var i,n:integer;
    m:T3DPMesh;
begin
 i := Things.IndexOf(th);
 if i < 0 then exit;
 thList[i].Free;
 m := A3DOToMesh(th,a3do);
 if m = nil then m := BadThingMesh(th);
 ThList[i] := m;
end;

 Procedure TNewPRenderer.SetViewToThing(th:TJKThing; zoomOut: Double; pitchOffset, yawOffset: double);
  var bbox: TThingBox;
begin
  if (th.a3do = nil) or (Things.IndexOf(th) < 0) then exit;

  th.a3do.GetBBox(bbox);
  var tcen_x: double := bbox.x1 + (bbox.x2 - bbox.x1) /2;
  var tcen_y: double := bbox.y1 + (bbox.y2 - bbox.y1) /2;
  var tcen_z: double := bbox.z1 + (bbox.z2 - bbox.z1) /2;
  var trad: double := sqrt(sqr(tcen_x - bbox.x1) + sqr(tcen_y - bbox.y1) +
    sqr(tcen_z - bbox.z1)) + zoomOut;

  with curCamera do
  begin
    rotation.SetCoords(-th.pch + pitchOffset, -th.yaw + yawOffset, 0);
//    self.pch := -th.pch + pitchOffset;
//    self.yaw := -th.yaw + yawOffset;

    const pchRadian = rotation.pitch * (PI / 180); // X rotation
    const yawRadian = rotation.yaw * (PI / 180); // Y rotation
    const newPosX = trad * sin(yawRadian) * cos(pchRadian);
    const newPosY = trad * cos(yawRadian) * cos(pchRadian);
    const newPosZ = trad * -sin(pchRadian);

    position.SetCoords(th.x - newPosX, th.y - newPosY, th.z - newPosZ);
  end;
end;

class Function TNewPRenderer.IsInFront(const p,n: TVector; const c: T3DPVertex; radius: single): Integer;
begin
  Result := 0;
  var d: double := n.Dot(c.x - p.x, c.y - p.y, c.z - p.z);  // VectorDot3(c.x - p.x, c.y - p.y, c.z - p.z, n.dx, n.dy, n.dz); //Smult
  if d >  radius then Result := 1 else
  if d > -radius then Result := 2 else result := 0;
end;

class Function TNewPRenderer.IsInFront(const [ref] f: TClipFrustum; m: T3DPMesh): Boolean;
begin
  Result :=
     (IsInFront(f.nearPlane.position  , f.nearPlane.normal  , m.center, m.radius) <> 0) and
     (IsInFront(f.leftPlane.position  , f.leftPlane.normal  , m.center, m.radius) <> 0) and
     (IsInFront(f.rightPlane.position , f.rightPlane.normal , m.center, m.radius) <> 0) and
     (IsInFront(f.topPlane.position   , f.topPlane.normal   , m.center, m.radius) <> 0) and
     (IsInFront(f.bottomPlane.position, f.bottomPlane.normal, m.center, m.radius) <> 0);
end;


Function TNewPRenderer.GetCameraFrustum: TClipFrustum;
var fp,lp,rp,tp,bp:Tvector; {Plane points}
  fn,ln,rn,tn,bn:TVector; {Plane's D normals}
  ax1,ay1,az1,ax2,ay2,az2:double;
  bx1,by1,bz1,bx2,by2,bz2:double;
begin
  Result := curCamera.frustum;
  GetWorldLine(vwidth div 2, vheight div 2, ax1, ay1, az1, ax2, ay2, az2);
  fn.SetCoords(ax2-ax1, ay2-ay1, az2-az1);

  {Front plane}
  //Normalize(fn);
  fn.Normalize;
  with curCamera.position do
    fp := fn.Scaled(Result.nearPlane.distance).Add(x, y, z);
  //fp.SetCoords(CamX+FrontPlane*fn.dx, CamY+FrontPlane*fn.dy, CamZ+FrontPlane*fn.dz);

  {Left plane}
  GetWorldLine(0,0,ax1,ay1,az1,ax2,ay2,az2);
  GetWorldLine(0,vheight,bx1,by1,bz1,bx2,by2,bz2);

  VectorCross3(bx1-ax1, by1-ay1, bz1-az1, ax2-ax1, ay2-ay1, az2-az1, ln.dx, ln.dy, ln.dz); // Vmult
  ln.Normalize;
  //Normalize(ln);
  lp.SetCoords(ax1, ay1, az1);

  {Right plane}
  GetWorldLine(vwidth, 0, ax1, ay1, az1, ax2, ay2, az2);
  GetWorldLine(vwidth, vheight, bx1, by1, bz1, bx2, by2, bz2);

  VectorCross3(ax2-ax1, ay2-ay1, az2-az1, bx1-ax1, by1-ay1, bz1-az1, rn.dx, rn.dy, rn.dz); // Vmult
  rn.Normalize;
  //Normalize(rn);
  rp.SetCoords(ax1, ay1, az1);

  {Top plane}
  GetWorldLine(0, 0, ax1, ay1, az1, ax2, ay2, az2);
  GetWorldLine(vwidth, 0, bx1, by1, bz1, bx2, by2, bz2);

  VectorCross3(ax2-ax1, ay2-ay1, az2-az1, bx1-ax1, by1-ay1, bz1-az1, tn.dx, tn.dy, tn.dz); // Vmult
  //Normalize(tn);
  tn.Normalize;
  tp.SetCoords(ax1, ay1, az1);

  {Bottom plane}
  GetWorldLine(0, vheight, ax1 ,ay1, az1, ax2, ay2, az2);
  GetWorldLine(vwidth, vheight, bx1, by1, bz1, bx2, by2, bz2);

  VectorCross3(bx1-ax1,by1-ay1,bz1-az1, ax2-ax1,ay2-ay1,az2-az1, bn.dx,bn.dy,bn.dz);  // Vmult
  bn.Normalize;
  //Normalize(bn);
  bp.SetCoords(ax1, ay1, az1);

  Result.nearPlane.position := fp;
  Result.nearPlane.normal := fn;

  Result.leftPlane.position := lp;
  Result.leftPlane.normal := ln;

  Result.topPlane.position := tp;
  Result.topPlane.normal := tn;

  Result.rightPlane.position := rp;
  Result.rightPlane.normal := rn;

  Result.bottomPlane.position := bp;
  Result.bottomPlane.normal := bn;
end;

Procedure TNewPRenderer.RenderWorldGeometry(const [Ref] frustrum: TClipFrustum);
begin
  const alphaAdjoinMask = (FF_RD_Adjoin or FF_Transluent);
  for var i := 0 to sclist.Count - 1 do
    begin
      var m := sclist[i];
      if m = nil then continue;
      if not IsInFront(frustrum, m) then continue;

      for var j := 0 to m.polys.Count - 1 do
        begin
          var poly := m.GetPoly(j);
          //if poly.geo = NotDrawn then continue;
          if poly.geo > geoMode then poly.geo := geoMode;
          if poly.light > lightMode then poly.light := lightMode;
          if BitMaskTest(poly.faceflags, alphaAdjoinMask) then//(poly.faceflags and alphaAdjoinMask) = alphaAdjoinMask then
            begin
              rdAlphaAdjoinsPolys.Add(poly);
            end
          else if (poly.geo <> NotDrawn) or BitMaskTest(poly.faceflags, FF_RD_HorizonSky) then
            begin
              if BitMaskTest(poly.faceflags, FF_RD_CeilingSky) then
                UpdateCeilingSkySurface(poly);
//              else if BitMaskTest(poly.faceflags, FF_RD_HorizonSky) then
//                poly := UpdateHorizonSkySurface(poly);

              rdCache.AddPoly(poly);
              if highlightSolidFloor and BitMaskTest(poly.faceflags, FF_RD_FloorWf) then
                rdFloorPolys.Add(poly)
            end;

          if highlightSolidSurface and BitMaskTest(poly.faceflags, FF_RD_SolidSurfaceWf) then
            rdSolidSurfacePolys.Add(poly);

          if highlightSpecialSurface and BitMaskTest(poly.faceflags, FF_RD_SpecialSurfaceWf) then
            rdSpecialSurfacePolys.Add(poly);
        end;
    end;
  rdCache.Flush;
end;

Procedure TNewPRenderer.RenderThings(const [Ref] frustrum: TClipFrustum);
begin
  for var i := 0 to ThList.count - 1 do
    begin
      var m := ThList[i];
      if m = nil then continue;
      if not IsInFront(frustrum, m) then continue;

      var ambientLight := ColorWhite;
      var tint := ColorBlack;
      if (m.sec <> nil) then
        begin
          ambientLight   := m.extraLight + m.sec.Ambient + m.sec.ExtraLight;
          ambientLight.a := 0;
          ambientLight.Normalize;
          tint := m.sec.tint;
        end;

      if bDebug then // render object radius
        RenderCircle(rdDebugPolys, m.center.x, m.center.y, m.center.z, m.radius);

      for var j := 0 to m.polys.Count - 1 do
        begin
          var poly := m.GetPoly(j);
          if poly.geo = NotDrawn then continue;
          if poly.geo > geoMode then poly.geo := geoMode;
          if poly.light > lightMode then poly.light := lightMode;

          poly.ambientLight := ambientLight;
          poly.tint := tint;

          if highlightSpecialSurface and BitMaskTest(poly.faceflags, FF_RD_SpecialSurfaceWf) then
            rdSpecialSurfacePolys.Add(poly);

          var isTransluent := BitMaskTest(poly.faceflags, FF_Transluent);
          if poly.tx <> nil then
            isTransluent := isTransluent or (poly.tx.ci.mode = RGBA);

          if isTransluent then
            begin
              BitSet(poly.faceflags, FF_Transluent);
              if rdAlphaCache.Count = rdAlphaCache.Capacity - 1 then
                FlushCaches;
              poly.RecalcDistance;
              rdAlphaCache.AddPoly(poly);
            end
          else
            rdCache.AddPoly(poly);
        end;
    end;

  FlushCaches;
end;

Procedure TNewPRenderer.RenderAlphaAdjoins;
begin
  for var p in rdAlphaAdjoinsPolys do
    begin
      if rdAlphaCache.Count = rdAlphaCache.Capacity - 1 then
        FlushCaches;
      p.RecalcDistance;
      rdAlphaCache.AddPoly(p);
    end;

  rdAlphaAdjoinsPolys.Clear;
  rdAlphaAdjoinsPolys.Capacity := rdPolysCacheCapacity;
  rdAlphaCache.Flush;
end;


Procedure TNewPRenderer.RenderWireframe(const polys: TList<T3DPoly>; wireColor: TColorF);
begin
  if polys.Count < 1 then exit;

  var curWfColor := colorRed;
  var curGeo := geoMode;
  geoMode := Wireframe;
  wfColor := wireColor;
  try
    begin
      for var p in polys do
        begin
          if rdCache.Count = rdCache.Capacity - 1 then
            FlushCaches;
          rdCache.AddPoly(p);
        end
    end
  finally
    begin
      FlushCaches;
      geoMode := curGeo;
      wfColor := CurWfColor;
    end;
  end;
end;

Procedure TNewPRenderer.RenderFloorWireframe;
begin
  RenderWireframe(rdFloorPolys, floorHighlightColor);
  rdFloorPolys.Clear;
  rdFloorPolys.Capacity := rdPolysCacheCapacity;
end;

Procedure TNewPRenderer.RenderSolidSurfaceWireframe;
begin
  RenderWireframe(rdSolidSurfacePolys, solidSurfaceHighlightColor);
  rdSolidSurfacePolys.Clear;
  rdSolidSurfacePolys.Capacity := rdPolysCacheCapacity;
end;

Procedure TNewPRenderer.RenderSpecialSurfaceWireframe;
begin
  RenderWireframe(rdSpecialSurfacePolys, specialSurfaceHighlightColor);
  rdSpecialSurfacePolys.Clear;
  rdSpecialSurfacePolys.Capacity := rdPolysCacheCapacity;
end;

Procedure TNewPRenderer.RenderDebugPolys;
begin
//  if rdDebugPickedPoly <> nil then    // buggy code rdDebugPickedPoly could be deleted
//    rdDebugPolys.Add(rdDebugPickedPoly);

  RenderWireframe(rdDebugPolys, debugPolysColor);
  rdDebugPolys.Clear;
  rdDebugPolys.Capacity := rdPolysCacheCapacity;
end;

Procedure TNewPRenderer.RenderSelection(const [Ref] frustrum: TClipFrustum);
  Function GetMesh(meshes: TMeshes; objects: TList; obj: TObject; objNum: integer): T3DPMesh;
  begin
    Result := nil;
    if P3DVisLayers then
      begin
        var i := objects.IndexOf(obj);
        if i < 0 then exit;
        Result := meshes[i]
      end
    else
      begin
        if objNum < meshes.Count then
          Result := meshes[objNum];
      end;
  end;

  Procedure AddMesh(m: T3DPMesh);
    begin
      if m = nil then exit;      
      if not IsInFront(frustrum, m) then exit;
      for var j := 0 to m.polys.count - 1 do
        rdCache.AddPoly(m.GetPoly(j));
    end;

  Procedure AddMeshSurface(m: T3DPMesh; surfIdx: integer);
    begin
      if not IsInFront(frustrum, m) then exit;
      if surfIdx >= m.polys.Count then exit;
      rdCache.AddPoly(m.GetPoly(surfIdx));
    end;

  Procedure AddSectorMesh(secNum: integer);
    begin
      if not CheckSectorIndex(level, secNum) then // can be out of bound index in case it was just deleted
        exit;
      AddMesh(GetMesh(SCList, sectors, level.sectors[secNum], secNum));
    end;

  Procedure AddSectorSurface(secNum: integer; surfIdx: integer);
    begin
      if not CheckSurfaceIndex(level, secNum, surfIdx) then // can be out of bound index in case it was just deleted
        exit;
      var m := GetMesh(SCList, sectors, level.sectors[secNum], secNum);
      if m = nil then exit;
      AddMeshSurface(m, surfIdx);
    end;

  Procedure AddThingMesh(thNum: integer);
    begin
      if not CheckThingIndex(level, thNum) then // can be out of bound index in case it was just deleted
        exit;
      AddMesh(GetMesh(THList, things, level.things[thNum], thNum));
    end;

begin
  var curWfColor := wfColor;
  var curGeo := geoMode;
  geoMode := Wireframe;
  EnableZTest(noClipSelectionOutline = false);
  try
    begin
      case JedMain.mapMode of
        MM_SC:
          begin
            // Current multiple selection
            wfColor := multipleSelectionColor;
            for var i := 0 to JedMain.scsel.Count - 1 do
              begin
                const sc = JedMain.scsel.GetSC(i);
                if sc = JedMain.Cur_SC then continue;
                AddSectorMesh(sc);
              end;

            if JedMain.scsel.Count > 0 then FlushCaches;

            // Current selection
            wfColor := selectionColor;
            AddSectorMesh(JedMain.Cur_SC);
          end;
        MM_SF:
          begin
            // Current multiple selection
            wfColor := multipleSelectionColor;
            for var i := 0 to JedMain.sfsel.Count - 1 do
              begin
                  var sc, sf: integer;
                  JedMain.sfsel.GetSCSF(i, sc, sf);
                  if (JedMain.Cur_SC = sc) and (sf = JedMain.Cur_SF) then
                    continue;
                  AddSectorSurface(sc, sf);
              end;
            if JedMain.sfsel.Count > 0 then FlushCaches;

            // Current selection
            wfColor := selectionColor;
            AddSectorSurface(JedMain.Cur_SC, JedMain.Cur_SF);
          end;
        MM_TH:
          begin
            // Current multiple selection
            wfColor := multipleSelectionColor;
            for var i := 0 to JedMain.thsel.Count - 1 do
              begin
              const th = JedMain.thsel.GetTH(i);
              if th = JedMain.Cur_TH then continue;
              AddThingMesh(th);
              end;
            if JedMain.thsel.Count > 0 then FlushCaches;

            // Current selection
            wfColor := selectionColor;
            AddThingMesh(JedMain.Cur_TH);
          end;
      end;
    end;
  finally
    begin
      FlushCaches;
      geoMode := curGeo;
      wfColor := CurWfColor;
      EnableZTest(true);
    end;
  end;
end;

Procedure TNewPRenderer.Redraw;
begin
  DisableFPUExceptions;

  curCamera.Update;
  UpdateSky();

  var frustum := GetCameraFrustum;
  RenderWorldGeometry(frustum);
  RenderThings(frustum);

  if highlightSolidSurface then
    RenderSolidSurfaceWireframe;
  if highlightSolidFloor then
    RenderFloorWireframe;
  if highlightSpecialSurface then
    RenderSpecialSurfaceWireframe;
  if bDebug then
    RenderDebugPolys;

  RenderAlphaAdjoins;
  FlushCaches;

  if highlightSelection then
    RenderSelection(frustum);
end;
//Procedure TNewPRenderer.Redraw;
//var i:integer;
//    fp,lp,rp,tp,bp:Tvector; {Plane points}
//    fn,ln,rn,tn,bn:TVector; {Plane's D normals}
//    ax1,ay1,az1,ax2,ay2,az2:double;
//    bx1,by1,bz1,bx2,by2,bz2:double;
//    m:T3DPMesh;
//begin
// DisableFPUExceptions;
//
// getWorldLine(vwidth div 2,vheight div 2,ax1,ay1,az1,ax2,ay2,az2);
// setVec(fn,ax2-ax1,ay2-ay1,az2-az1);
// {Front plane}
// Normalize(fn);
// SetVec(fp,CamX+FrontPlane*fn.dx,CamY+FrontPlane*fn.dy,CamZ+FrontPlane*fn.dz);
//
// {Left plane}
// getWorldLine(0,0,ax1,ay1,az1,ax2,ay2,az2);
// getWorldLine(0,vheight,bx1,by1,bz1,bx2,by2,bz2);
//
// Vmult(bx1-ax1,by1-ay1,bz1-az1,
//       ax2-ax1,ay2-ay1,az2-az1,
//       ln.dx,ln.dy,ln.dz);
//  Normalize(ln);
//  SetVec(lp,ax1,ay1,az1);
//
// {Right plane}
// getWorldLine(vwidth,0,ax1,ay1,az1,ax2,ay2,az2);
// getWorldLine(vwidth,vheight,bx1,by1,bz1,bx2,by2,bz2);
//
// Vmult(ax2-ax1,ay2-ay1,az2-az1,
//       bx1-ax1,by1-ay1,bz1-az1,
//       rn.dx,rn.dy,rn.dz);
//  Normalize(rn);
//  SetVec(rp,ax1,ay1,az1);
//
// {Top plane}
// getWorldLine(0,0,ax1,ay1,az1,ax2,ay2,az2);
// getWorldLine(vwidth,0,bx1,by1,bz1,bx2,by2,bz2);
//
// Vmult(ax2-ax1,ay2-ay1,az2-az1,
//       bx1-ax1,by1-ay1,bz1-az1,
//       tn.dx,tn.dy,tn.dz);
//  Normalize(tn);
//  SetVec(tp,ax1,ay1,az1);
//
// {Bottom plane}
// getWorldLine(0,vheight,ax1,ay1,az1,ax2,ay2,az2);
// getWorldLine(vwidth,vheight,bx1,by1,bz1,bx2,by2,bz2);
//
// Vmult(bx1-ax1,by1-ay1,bz1-az1,
//       ax2-ax1,ay2-ay1,az2-az1,
//       bn.dx,bn.dy,bn.dz);
//  Normalize(bn);
//  SetVec(bp,ax1,ay1,az1);
//
// // Draw meshes of sectors
// for i:=0 to sclist.count-1 do
// begin
//  m:=sclist[i];
//  if m=nil then continue;
//  if (isInFront(fp,fn,m.center,m.radius)=0) or
//     (isInFront(lp,ln,m.center,m.radius)=0) or
//     (isInFront(rp,rn,m.center,m.radius)=0) or
//     (isInFront(tp,tn,m.center,m.radius)=0) or
//     (isInFront(bp,bn,m.center,m.radius)=0) then continue;
//  DrawMesh(m);
// end;
//
// // Draw meshes of Things
// for i:=0 to thlist.count-1 do
// begin
//  m:=thlist[i];
//  if m=nil then continue;
//  if (isInFront(fp,fn,m.center,m.radius)=0) or
//     (isInFront(lp,ln,m.center,m.radius)=0) or
//     (isInFront(rp,rn,m.center,m.radius)=0) or
//     (isInFront(tp,tn,m.center,m.radius)=0) or
//     (isInFront(bp,bn,m.center,m.radius)=0) then continue;
//  DrawMesh(m);
// end;
//
//end;


Function IsPointOn3DPSurf(const poly: T3DPoly; x, y, z: double): boolean;
var
  vc,i: integer;
  v1,v2: TVXDets;
  dist: double;
  vct: TVector;
  x1,x2,y1,y2,z1,z2: double;
begin
  Result := false;

  vc := poly.vxds.Count;
  For i := 0 to vc - 1 do
    begin
      v1 := poly.GetVXD(i);
      v2 := poly.GetVXD(NextIdx(i,vc));
      With poly do
        vct := normal.Cross(v2.x - v1.x, v2.y - v1.y, v2.z - v1.z);
//        VMult(normal.dx, normal.dy, normal.dz,
//              v2.x - v1.x, v2.y - v1.y, v2.z - v1.z,
//              vct.dx, vct.dy, vct.dz);

      dist := vct.Dot(x - v1.x, y - v1.y, z - v1.z);
//      dist := SMult(vct.dx, vct.dy, vct.dz,
//                  x - v1.x, y - v1.y, z - v1.z);
      if (dist < -CloseEnough ) then exit;
      { if dist<=0 then exit;}
    end;
    Result := true;
end;


Function TNewPRenderer.PickAt(X,Y:integer):TPickType;
var
    ax1,ay1,az1,ax2,ay2,az2:double;
    i,j,n:integer;
    cdist:double;
    m:T3DPMesh;
    lvec:TVector;

  Function GetIntersectingSurf(m: T3DPMesh; var cdist: double): Integer;
  var
      a,b,c2: double;
      ax,ay,az: double;
      cv: TVector;
      i: integer;
      p: T3DPoly;
      vxd: TVXDets;
      dist: double;
  begin
   Result := -1;
   if m = nil then exit;
   cv.SetCoords(m.center.x - ax1, m.center.y - ay1, m.center.z - az1);
   c2 := sqr(cv.dx) + sqr(cv.dy) + sqr(cv.dz);
   a  := lvec.Dot(cv); //SMult(lvec.dx, lvec.dy, lvec.dz, cv.dx, cv.dy, cv.dz);

   if a < (-m.radius) then exit; {If behind the plane}

   b := sqrt(c2 - sqr(a));
   if b > m.radius then exit; {if look line doesn't intersect bounding sphere}

   for i := 0 to m.polys.count - 1 do
     begin
      p := m.GetPoly(i);
      if p.geo < Wireframe then continue;

      //if SMult(p.normal.dx, p.normal.dy, p.normal.dz, lvec.dx, lvec.dy, lvec.dz) > 0 then continue;
      if p.normal.Dot(lvec) > 0 then continue;

      if p.vxds.count = 0 then continue;
      vxd := p.getVXD(0);
      if Not PlaneLineXnNew(p.normal, vxd.X, vxd.y, vxd.z, ax1, ay1, az1, ax2, ay2, az2, ax, ay, az)
       then continue;

       {Debug version}
    {   if not IsPointOn3DPSurf(s,ax,ay,az) then continue;
       dist:=SMult(ax-ax1,ay-ay1,az-az1,lvec.dx,lvec.dy,lvec.dz);
      if dist<0 then continue;
      if dist>cdist then continue;}

      dist := VectorDot3(ax - ax1, ay - ay1, az - az1, lvec.dx, lvec.dy, lvec.dz); //SMult
      if dist < 0 then continue;
      if dist > cdist then continue;
      if not IsPointOn3DPSurf(p, ax, ay, az) then continue;

      cdist  := dist;
      Result := i;
     end
  end;

begin
 cdist := 1E100;
 result := pk_nothing;

 //getWorldLine(0,0,ax1,ay1,az1,ax2,ay2,az2);
 //rdDebugPickedPoly := nil;
 GetWorldLine(X, Y, ax1, ay1, az1, ax2, ay2, az2);
 lvec.SetCoords(ax2-ax1, ay2-ay1, az2-az1);
 lvec.Normalize;
 //Normalize(Lvec);

 var frustum := GetCameraFrustum;

 for i := 0 to sclist.count - 1 do
 begin
  m := sclist[i];
  n := GetIntersectingSurf(m, cdist);
  if n = -1 then continue;

  //rdDebugPickedPoly := m.polys[n];
  result := pk_surface;
  selSC  := Sectors[i];
  selSF  := n;
  for j := 0 to selSC.surfaces.count - 1 do
   if selSC.surfaces[j].D3DID = n then
     begin
      SelSF := j;
      break;
     end;
 end;

 for i := 0 to thlist.count - 1 do
   begin
    m := thlist[i];
    if not IsInFront(frustum, m) then continue;

    n := GetIntersectingSurf(m, cdist);
    if n = -1 then continue;

    //rdDebugPickedPoly := m.polys[n];
    result := pk_thing;
    selTH  := Things[i];
   end;
end;


Function TNewPRenderer.GetTexture(const name,cmp: string):T3DPTexture;
var i:integer;
    Ttx:T3DPTexture;
    pcmp:PTCMPTable;
    tpal:TCMPPal;
    ptpal:PTCMPPal;
    f:TFile;
begin
  pcmp:= nil;
  ptpal:= nil;
  Result:=nil;

  i := TXlist.IndexOf(name + cmp);
  if i <> -1 then
    begin
      Result := T3DPTexture(TXList.Objects[i]);
      exit;
    end;

  // Load palette
  if CurrentProject <> IJIM then
    begin
      i := CmpList.IndexOf(cmp);
      if i <> -1 then pcmp := Pointer(CmpList.Objects[i]) else
        begin
          GetMem(pcmp, sizeof(pcmp^));
          if not LoadCmpTable(cmp, pcmp^) then PanMessage(mt_warning, Format('Cannot load %s', [cmp]));
          CmpList.AddObject(cmp, TObject(pcmp));
        end;

      LoadMasterPal(level);
      tpal:=MasterPal;
      AdjustPalGamma(tpal,gamma);
      ptpal := @tpal;
    end;

 ttx := nil;

 try
  ttx := LoadTexture(name, ptpal, pcmp);
{  TGlideTexture.CreateFromMat(name,tpal,GenTexture);}
  if (ttx <> nil) and (CurrentProject <> IJIM ) then
    ttx.cmpname := cmp;
 finally
  TXList.AddObject(name + cmp, ttx);
  Result := ttx;
 end;
end;


Procedure TNewPRenderer.LoadMasterPal(l:TJKLevel);
var
 i:integer;
 pal:array[0..255] of
 record
  r,g,b,a:byte;
 end;

begin
 if MasterPalSet then exit;
 GetLevelPal(l, MasterPal);

 for i:=0 to 255 do
   With Pal[i] do
     begin
      r := MasterPal[i].r;
      g := MasterPal[i].g;
      b := MasterPal[i].b;
      a := 0;
     end;

{ glEnable(GL_COLOR_INDEX8_EXT);

 pPalProc(GL_TEXTURE_2D, GL_RGB8, sizeof(Pal),
	  GL_RGBA, GL_UNSIGNED_BYTE, pal );}

 MasterPalSet:=true;
end;



Function TNewPRenderer.SecToMesh(sec: TJKSector): T3DPMesh;
var poly:T3DPoly;
    vd:TVXDets;
begin
  DisableFPUExceptions;
  Result        := T3DPMesh.Create;
  Result.sec    := sec;
 {getMem(vxs,sizeof(T3DPVertex)*s.vertices.count);}

  for var i:=0 to sec.surfaces.count-1 do
    With sec.surfaces[i] do
      begin
        //if (sec.Flags and SECF_3DO) = 0
          //then if (geo = 0) then begin D3DID:=-1; continue; end;

        poly              := Result.AddPoly;
        poly.faceflags    := FaceFlags;
        poly.geo          := TGeoMode(geo);
        poly.light        := TLightMode(light);
        poly.ambientLight := sec.ExtraLight;
        poly.extraLight   := ExtraLight;
        poly.tint         := sec.Tint;

        poly.normal := normal;
        if BitMaskTest(sec.Flags, SECF_3DO) then
          poly.normal.SetCoords(-normal.dx, -normal.dy, -normal.dz);

        // Make sky poly unlit
        if BitMaskTest(sec.Flags, SECF_3DO) or
          BitMaskTest(surfflags, SF_SkyHorizon) or
          BitMaskTest(surfflags, SF_SkyCeiling) then
          poly.light := TLightMode.None;

        if sec.level.kind = JKDF2 then // JKDF2 doesn't support tex clamping?
          BitClear(poly.faceflags, FF_TexClampX or FF_TexClampY);

        // Mark poly to render fog (IJIM)
        if BitTest(surfflags, SF_SkyHorizon or SF_SkyCeiling) = 0 then
          BitSet(poly.faceflags, FF_IJIM_FogEnabled)
        else
           BitSet(poly.faceflags, IfThen(BitMaskTest(surfflags, SF_SkyHorizon), FF_RD_HorizonSky, FF_RD_CeilingSky ) );

        // Mark poly as floor or has collision
        const sfFloor = SF_Floor or SF_Collision;
        if Adjoin <> nil then
          begin
            poly.faceflags := LongInt(poly.faceflags or FF_RD_Adjoin);
            if ((AdjoinFlags and not SAF_Move) = AdjoinFlags) or BitMaskTest(AdjoinFlags, SAF_NoPlayerMove) then
               BitSet(poly.faceflags, FF_RD_SolidSurfaceWf);
          end
        else if BitMaskTest(surfflags, sfFloor) then
          BitSet(poly.faceflags, FF_RD_FloorWf)
        else if BitMaskTest(surfflags, SF_Collision) then
          BitSet(poly.faceflags, FF_RD_SolidSurfaceWf);

        // Mark poly as special
        if IsSpecialSurface(sec.surfaces[i]) then
          BitSet(poly.faceflags, FF_RD_SpecialSurfaceWf);

        D3DID := Result.polys.count - 1;
        Try
          poly.tx := nil;
          if Length(Material) > 0 then
            poly.tx := GetTexture(Material, sec.ColorMap);
        except
          on E:Exception do PanMessage(mt_warning,
            Format('Cannot load %s for sector %d surface %d: %s', [Material, sec.num, i ,e.message]));
        end;

        if (poly.tx <> nil) and (poly.tx.ci.alphaBPP = 1) then
          poly.faceflags := LongInt(poly.faceflags or FF_RD_AlphaRef);

        var nvx := vertices.Count;
        var nv: integer;
        //if nvx>24 then nvx:=24;

        var invMatWidth := 1.0;
        var invMatHeight := 1.0;

        if (nvx > 0) and (poly.tx <> nil) then
         begin
           invMatWidth := 1 / poly.tx.width;
           invMatHeight := 1 / poly.tx.height;
         end;

        for var j := 0 to nvx - 1 do
          begin
            if (sec.Flags and SECF_3DO) = 0 then
              nv := j
            else
              nv := nvx - j - 1;

            with txvertices[nv] do
              begin
                vd   := poly.AddVXD;
                vd.x := vertices[nv].x;
                vd.y := vertices[nv].y;
                vd.z := vertices[nv].z;
                vd.intensity := color;
                if CurrentProject = IJIM then
                  vd.intensity.a := extraLight.a;

                if poly.tx <> nil then
                  begin
                      vd.u := u * invMatWidth;
                      vd.v := v * invMatHeight;
                  end;
              end;
          end;
      end;

  result.CalculateSphere;
end;

Procedure TNewPRenderer.HandleActivate(const msg:TMessage);
begin
end;

procedure TNewPRenderer.HandlePaint(hdc:HDC);
begin
end;



Function TNewPRenderer.BadThingMesh(th:TJKThing):T3DPMesh;
var
    mx:TMat3x3s;
    surf:T3DPoly;
    vd:TVXDets;
begin
 // Functions crates mesh for Things without 3DO models, e.g. ghost Thing

 Result        := T3DPMesh.Cube(0.015, MakeColor(0.89, 0.89, 0.89));
 // assigning sector is skipped so colors won't apply

 // Translate and rotate object
 With th do CreateRotMatrixS(mx,pch,yaw,rol);
 for var i := 0 to Result.polys.Count -1 do
    begin
      with Result.GetPoly(i) do
        for var j:=0 to vxds.Count-1 do
          With GetVXD(j) do
            begin
              var ax: single := x;
              var ay: single := y;
              var az: single := z;

              MultVM3s(mx, ax, ay, az);
              x:=th.x + ax;
              y:=th.y + ay;
              z:=th.z + az;
            end;
    end;

 result.CalculateSphere;
end;

Function TNewPRenderer.ThingToMesh(th: TJKThing):T3DPMesh;
var i,j,k:integer;
    nv,nvx:integer;
    ttx:T3DPTexture;
    cmp,mat:string;
    ax,ay,az:single;
    mx:TMat3x3s;
    poly:T3DPoly;
    vd:TVXDets;
begin
  result := nil;
  if th.a3DO = nil then exit;

  Result     := T3DPMesh.Create;
  Result.sec := th.sec;

  var thlight := th.light;
  if thlight.a <= 0.01 then  // IJIM makes baked light from dynamic light when radius <= 0.01
    Result.extraLight := thlight;

  const jkdf2Lights: Boolean = ((th.level <> nil) and (th.level.kind = JKDF2) or (CurrentProject = JKDF2))
    or not P3DColoredLights;

  With th do CreateRotMatrixS(mx, pch, yaw, rol);

  var alpha := 1.0; // TODO: assign thing.alpha? jonesEngine3D
//  if (th.flags and (TF_INVISIBLE or TF_DISABLED)) <> 0 then
//    alpha := 0.80;

  for i := 0 to th.a3DO.meshes.count - 1 do
    With th.a3Do.meshes[i] do
      for k := 0 to faces.Count - 1 do
        With faces[k] do
          begin
            mat := th.A3DO.GetMat(imat);
            if (geo = 0) or (mat = '') then continue;

            poly            := Result.AddPoly;
            poly.geo        := TGeoMode(geo);
            poly.light      := TLightMode(light);
            poly.extraLight := extraLight;
            poly.normal     := normal;
            lev_utils.CalcNormal(faces[k], poly.normal);
            faces[k].CalcNormal;

            // Enable rendering fog for poly (IJIM) and mark poly as special
            poly.faceflags := LongInt(FaceFlags or FF_IJIM_FogEnabled);
            if IsSpecial3doPoly(Faces[k]) then
              BitSet(poly.faceflags, FF_RD_SpecialSurfaceWf);

            ttx := nil;
            Try
              ttx     := GetTexture(Mat, thing_cmp);
              poly.tx := ttx;
            { if ttx<>nil then ttx.SetCurrent;}
            except
              on Exception do PanMessage(mt_warning,
                Format('Cannot load %s for thing %d', [Mat, th.num]));
              end;

            if (poly.tx <> nil) and (poly.tx.ci.alphaBPP > 0) then
            begin
              BitSet(poly.faceflags, FF_Transluent);
              if (poly.tx.ci.alphaBPP = 1) then
                BitSet(poly.faceflags, FF_RD_AlphaRef);
            end;

            if alpha < 1.0 then
            begin
              BitSet(poly.faceflags, FF_Transluent);
              poly.extraLight.a := 1.0; // this will make sure only vertex intensity alpha is applied
            end;

            { pim:=ttx.Itexture.GetImage;
            if pim=nil then;}

            nvx := vertices.Count;
            //if nvx>24 then nvx:=24;

            for j := 0 to nvx - 1 do
              With txVertices[j] do
                begin
                  ax := vertices[j].x;
                  ay := vertices[j].y;
                  az := vertices[j].z;
                  MultVM3s(mx, ax, ay, az);

                  vd   := poly.AddVXD;
                  vd.x := th.x + ax;
                  vd.y := th.y + ay;
                  vd.z := th.z + az;

                  vd.intensity := color;
                  vd.intensity.a := 1.0; // Make sure model is opaque
                  if alpha < 1.0 then
                    vd.intensity.a := alpha;

                  // Tex uv
                  if ttx <> nil then
                    begin
                     vd.u := u;
                     vd.v := v;
                     if CurrentProject <> IJIM then
                       begin
                        vd.u:= u / ttx.width;
                        vd.v:= v / ttx.height;
                       end;
                    end;
                end;
          end;
 result.CalculateSphere;
end;

Function TNewPRenderer.A3DOToMesh(th:TJKThing; a3do:TPJ3DO):T3DPMesh;
var i,j,k:integer;
    nv,nvx:integer;
    ttx:T3DPTexture;
    cmp,mat:string;
    ax,ay,az:single;
    mx:TMat3x3s;
    surf:T3DPoly;
    vd:TVXDets;
begin
 { TODO: join this function with ThingToMesh }

 result := nil;
 if (a3do = nil) then exit;

 Result        := T3DPMesh.Create;
 Result.sec    := th.Sec;

 With th do CreateRotMatrixS(mx, pch, yaw, rol);

 for i := 0 to a3DO.Meshes.count - 1 do
 With a3Do.Meshes[i] do
   for k := 0 to faces.count - 1 do
     With Faces[k] do
       begin
          mat := A3DO.GetMat(imat);
          if (geo = 0) or (Mat = '') then continue;

          surf            := Result.AddPoly;
          surf.geo        := TGeoMode(geo);
          surf.light      := TLightMode(light);
          surf.extraLight := ExtraLight;
          surf.normal     := normal;
          CalcNormal;

          surf.faceflags := LongInt(FaceFlags or FF_IJIM_FogEnabled);
          if IsSpecial3doPoly(Faces[k]) then
            surf.faceflags := LongInt(surf.faceflags or FF_RD_SpecialSurfaceWf);

          ttx := nil;
          Try
           ttx     := GetTexture(Mat,thing_cmp);
           surf.tx := ttx;
          { if ttx<>nil then ttx.SetCurrent;}
          except
            on Exception do PanMessage(mt_warning,
                Format('Cannot load %s for thing %d', [Mat, th.num]));
            end;

          if (surf.tx <> nil) and (surf.tx.ci.alphaBPP = 1) then
            surf.faceflags := LongInt(surf.faceflags or FF_RD_AlphaRef);

          { pim:=ttx.Itexture.GetImage;
          if pim=nil then;}

          nvx := Vertices.Count;
           //if nvx>24 then nvx:=24;

          for j := 0 to nvx - 1 do
            With TXVertices[j] do
              begin
                vd := Surf.AddVXD;

                ax := Vertices[j].tx;
                ay := Vertices[j].ty;
                az := Vertices[j].tz;

                MultVM3s(mx, ax, ay, az);

                vd.x := th.x + ax;
                vd.y := th.y + ay;
                vd.z := th.z + az;
                vd.intensity := color;

                if ttx<>nil then
                  begin
                   vd.u := u;
                   vd.v := v;
                   if CurrentProject <> IJIM then
                     begin
                      vd.u := u / ttx.width;
                      vd.v := v / ttx.height;
                     end;
                  end;

                 { glVertex3f(th.x+ax,th.y+ay,th.z+az);}
              end;
       end;

 result.CalculateSphere;

end;

Procedure TNewPRenderer.RenderCircle(polys: TList<T3DPoly>; cx, cy, cz, rad: double);
  const maxSides = 64;
  var mx: TMat3x3;
begin
  with curCamera.rotation do
    CreateRotMatrix(mx, -pitch, -yaw, 0);

  const th = 6.2832 / maxSides;  // 6.2832 = 2*PI
  var poly   := T3DPoly.Create;
  poly.geo   := WireFrame;
  poly.light := None;
  for var s := 0 to maxSides - 1 do
    begin
      const ra = s * th;
      const cr = cos(ra) * rad;
      const sr = sin(ra) * rad;
        const v = poly.AddVXD;

        var ax: double := cr * cos(0);
        var ay: double := cr * sin(0);
        var az: double := sr;
        MultVM3(mx, ax, ay, az);

        v.x := cx + ax;
        v.y := cy + ay;
        v.z := cz + az;
    end;

  polys.Add(poly);
end;

Procedure TNewPRenderer.UpdateSky();
begin
  ceilingSkyZ.SetCoords(0, 0, level.header.ceilingSky.height);

  horizonSkyPPR := level.header.horizonSky.pixelsPerRev * 0.0027777778; // OpenJKDF2, 0.0027777778 = 1/ 360
  if level.kind = IJIM then
    horizonSkyPPR := 0.0083333338; // hardcoded; 0.0083333338 - 1 / 120

  horizonSkyDistance := level.header.horizonSky.distance / 256;
  horizonSkyScale    := horizonSkyDistance / curCamera.fovNearPlane ;

  var rot := curCamera.orient.ExtractPYR; //curCamera.rotation; //curCamera.orient.ExtractPYR;
  SinCos(DegToRad(rot.roll), horizonSkyRollSin, horizonSkyRollCos);
  horizonSkyYaw   := (rot.yaw * horizonSkyPPR);
  horizonSkyPitch := (rot.pitch * horizonSkyPPR);
end;

Procedure TNewPRenderer.UpdateCeilingSkySurface(const surf: T3DPoly);
begin
  if surf.tx = nil then
    exit;

  var invMatWidth  := 1 / surf.tx.width;
  var invMatHeight := 1 / surf.tx.height;

  var skyvrt: TVector;
  for var j := 0 to surf.vxds.count - 1 do
  begin
    var vxd := surf.getVXD(j);
     with vxd do
      skyvrt.SetCoords(x, y, z);

    skyvrt := skyvrt -  curCamera.position;
    skyvrt.Normalize;

    var skydist: Double := 1000.0;
    CheckSphereFaceHit(curCamera.position, skyvrt, skydist, 0.0, ceilingSkyNormal, ceilingSkyZ, skydist, 0);

    skyvrt := skyvrt * skydist;
    skyvrt := skyvrt + curCamera.position;

    // Note, the original engine adds sky offset at the end of multiplication which makes sky not to shift in any direction
    vxd.u := invMatWidth  * ((skyvrt.x + level.header.ceilingSky.offset.X) * 16);
    vxd.v := invMatHeight * ((skyvrt.y + level.header.ceilingSky.offset.Y) * 16);
  end;
end;

Function TNewPRenderer.UpdateHorizonSkySurface(const surf: T3DPoly) : T3DPoly;
begin
  // Note this code projects the horizon sky verts to horizontal 2D sky plane
  // The code still doesn't work correct as the texture gets trached when rotating around
  // + there are sometimes holes in the sky or sky disappears all together.

  Result := surf;
  if surf.tx = nil then
      exit;

  var skypoly := T3DPoly.Create;
  skypoly.geo    := Texture;
  skypoly.light  := None;
  skypoly.tx := surf.tx;
  var skyvrt: TVector;
  for var j := 0 to surf.vxds.count - 1 do
  begin
    var vxd := surf.getVXD(j);
    with vxd do
      skyvrt.SetCoords(x, y, z);

    skyvrt := curCamera.view.TransformPoint(skyvrt);    // Transfrom to camera space

    // perspective project to clip space 2d plane
    var w := 1 / horizonSkyDistance;
    skyvrt.x := (curCamera.fovNearPlane / skyvrt.y) * skyvrt.x;
    skyvrt.z := skyvrt.z * (curCamera.fovNearPlane / skyvrt.y);
    skyvrt.y := 256;// horizonSkyDistance;

    skyvrt :=  skyvrt * w;
    skyvrt := curCamera.view.TransformPoint(skyvrt); // transform back to world space

    // Assign vert
    var skyvx := skypoly.AddVXD;
    skyvx.x := skyvrt.x;
    skyvx.y := skyvrt.y;
    skyvx.z := skyvrt.z;

    var center_x := 1;// vwidth  * 0.5;
    var center_y := 1; //vheight * 0.5;
    var sx := (-skyvrt.x + center_x) * horizonSkyScale ;
    var sy := (-skyvrt.z + center_y ) * horizonSkyScale ;
    skyvx.u  := (sx * horizonSkyRollCos - sy * horizonSkyRollSin + horizonSkyYaw   + level.header.horizonSky.offset.x) ;
    skyvx.v  := (sy * horizonSkyRollCos + sx * horizonSkyRollSin + horizonSkyPitch + level.header.horizonSky.offset.y) ;
  end;

  surf.geo := NotDrawn; // make it not to render
  Result := skypoly;
end;

Procedure TNewPRenderer.FlushCaches;
begin
  rdCache.Flush;
  rdAlphaCache.Flush;
end;

Type
  TVXData = record
    x,y,z:single;
    infront: boolean;
  end;

Constructor T3DPoly.Create;
begin
 vxds := TList.create;
end;

Destructor T3DPoly.Destroy;
var i:integer;
begin
 for i:=0 to vxds.count-1 do GetVXD(i).Free;
 vxds.Free;
end;

Function T3DPoly.AddVXD: TVXDets;
begin
 Result := TVXDets.Create;
 vxds.Add(Result);
end;

Function T3DPoly.GetVXD(n:integer):TVXDets;
begin
 Result := TVXDets(vxds.List[n]);
end;

Procedure T3DPoly.RecalcDistance;
begin
  var dist: single := 3.4028235e38; // constant from IJIM, aka max float
  For var i := 0 to vxds.Count - 1 do
  begin
    var fwd := GetVXD(i).y;  // original directx screen coordinate sz is used
    if fwd < dist then
      dist := fwd;
  end;
  self.distance := dist;
end;

Function T3DPoly.GetLitColor(color: TColorF): TColorF;
begin
    Result := ColorWhite;
    Result.a := color.a;  // IJIM sets alpha also in cases when lmode is fully lit or none

    if light = VertexDiffuseOnly then
      Result := color
    else if (light >= TLightMode.Diffuse) and (not P3DFullLit) then
      begin
        if CurrentProject = IJIM then
          Result := ambientLight + extraLight
        else
          Result := extraLight;

        if light = Gouraud then
          begin
            var a := Result.a;
            Result := Result + color;

            if a >= 1.0 then
              Result.a := color.a;
          end;

        if (CurrentProject <> IJIM) then
          begin
            // Approximation of how light color is clamped (taken from OpenJKDF2 source code)
            Result.Clamp(ambientLight, ColorWhite);

            if not (IsZero(tint.r) and IsZero(tint.g) and IsZero(tint.b)) then
              begin
                // Approximation of how TINT is Applied in JKDF2 (taken from OpenJKDF2 source code)
                Result.r := Result.r + Result.r * (tint.r - (0.5 * (tint.g + tint.b)));
                Result.g := Result.g + Result.g * (tint.g - (0.5 * (tint.r + tint.b)));
                Result.b := Result.b + Result.b * (tint.b - (0.5 * (tint.g + tint.r)));
              end;
          end;
      end;

      Result.Normalize;
end;

Constructor T3DPMesh.Create;
begin
  sec := nil;
  polys := TList.Create;
end;

Constructor T3DPMesh.Cube(size: float; color: TColorF);
begin
  sec := nil;
  polys := TList.Create;

  size := size * 0.5; //half size

  // Front
  var poly := AddPoly;
  poly.faceflags := FF_Transluent;
  poly.geo   := Solid;
  poly.light := VertexDiffuseOnly;
  poly.normal.SetCoords(0, 1, 0);
  poly.tx := nil;

  var vd := poly.AddVXD; vd.intensity := color;
  vd.z := size;
  vd.y := size;
  vd.x := size;
  vd := poly.AddVXD; vd.intensity := color;
  vd.z := -size;
  vd.y := size;
  vd.x := size;
  vd := poly.AddVXD; vd.intensity := color;
  vd.z := -size;
  vd.y := size;
  vd.x := -size;
  vd := poly.AddVXD; vd.intensity := color;
  vd.z := size;
  vd.y := size;
  vd.x := -size;

  // Back
  color := color * 0.95; // dim color

  poly       := AddPoly;
  poly.geo   := Solid;
  poly.light := VertexDiffuseOnly;
  poly.tx    := nil;
  poly.normal.SetCoords(0, -1, 0);

  vd := poly.AddVXD; vd.intensity := color;
  vd.z := size;
  vd.y := -size;
  vd.x := -size;
  vd := poly.AddVXD; vd.intensity := color;
  vd.z := -size;
  vd.y := -size;
  vd.x := -size;
  vd := poly.AddVXD; vd.intensity := color;
  vd.z := -size;
  vd.y := -size;
  vd.x := size;
  vd := poly.AddVXD; vd.intensity := color;
  vd.z := size;
  vd.y := -size;
  vd.x := size;

  // Left
  color := color * 0.95; // dim color

  poly       := AddPoly;
  poly.geo   := Solid;
  poly.light := VertexDiffuseOnly;
  poly.tx    := nil;
  poly.normal.SetCoords(-1, 0, 0);

  vd := poly.AddVXD; vd.intensity := color;
  vd.z := size;
  vd.y := size;
  vd.x := -size;
  vd := poly.AddVXD; vd.intensity := color;
  vd.z := -size;
  vd.y := size;
  vd.x := -size;
  vd := poly.AddVXD; vd.intensity := color;
  vd.z := -size;
  vd.y := -size;
  vd.x := -size;
  vd := poly.AddVXD; vd.intensity := color;
  vd.z := size;
  vd.y := -size;
  vd.x := -size;

  // Right
  color := color * 0.95; // dim color

  poly       := AddPoly;
  poly.geo   := Solid;
  poly.light := VertexDiffuseOnly;
  poly.tx    := nil;
  poly.normal.SetCoords(1, 0, 0);

  vd := poly.AddVXD; vd.intensity := color;
  vd.z := size;
  vd.y := -size;
  vd.x := size;
  vd := poly.AddVXD; vd.intensity := color;
  vd.z := -size;
  vd.y := -size;
  vd.x := size;
  vd := poly.AddVXD; vd.intensity := color;
  vd.z := -size;
  vd.y := size;
  vd.x := size;
  vd := poly.AddVXD; vd.intensity := color;
  vd.z := size;
  vd.y := size;
  vd.x := size;

  // Top
  color := color * 0.95; // dim color

  poly       := AddPoly;
  poly.geo   := Solid;
  poly.light := VertexDiffuseOnly;
  poly.tx    := nil;
  poly.normal.SetCoords(0, 0, 1);

  vd := poly.AddVXD; vd.intensity := color;
  vd.z := size;
  vd.y := -size;
  vd.x := -size;
  vd := poly.AddVXD;
  vd.intensity := color;
  vd.z := size;
  vd.y := -size;
  vd.x := size;
  vd := poly.AddVXD; vd.intensity := color;
  vd.z := size;
  vd.y := size;
  vd.x := size;
  vd := poly.AddVXD; vd.intensity := color;
  vd.z := size;
  vd.y := size;
  vd.x := -size;

  // Bottom
  color := color * 0.95; // dim color

  poly       := AddPoly;
  poly.geo   := Solid;
  poly.light := VertexDiffuseOnly;
  poly.tx    := nil;
  poly.normal.SetCoords(0, 0, -1);

  vd := poly.AddVXD; vd.intensity := color;
  vd.z := -size;
  vd.y := -size;
  vd.x := size;
  vd := poly.AddVXD;
  vd.intensity := color;
  vd.z := -size;
  vd.y := -size;
  vd.x := -size;
  vd := poly.AddVXD; vd.intensity := color;
  vd.z := -size;
  vd.y := size;
  vd.x := -size;
  vd := poly.AddVXD; vd.intensity := color;
  vd.z := -size;
  vd.y := size;
  vd.x := size;
end;

Destructor T3DPMesh.Destroy;
var i:integer;
begin
  for i := 0 to polys.count - 1 do GetPoly(i).Free;
  polys.Free;
end;

Function T3DPMesh.AddPoly:T3DPoly;
begin
  Result := T3DPoly.Create;
  polys.add(Result);
end;

Function T3DPMesh.GetPoly(n:integer): T3DPoly;
begin
  Result := T3DPoly(polys.List[n]);
end;

Procedure T3DPMesh.CalculateSphere;
var i,j:integer;
    x1,y1,z1,x2,y2,z2:single;
    surf:T3DPoly;
    vxd:TVXDets;
    isempty:boolean;
begin
  center.x := 0;
  center.y := 0;
  center.z := 0;
  radius   := 0;
  isempty  := true;

  for i := 0 to polys.count - 1 do
    begin
      surf := GetPoly(i);
      for j := 0 to surf.vxds.count - 1 do
        begin
          vxd := surf.GetVXD(j);
          if isempty then
            begin
              x1 := vxd.x; x2 := vxd.x;
              y1 := vxd.y; y2 := vxd.y;
              z1 := vxd.z; z2 := vxd.z;
              isempty := false;
              continue;
            end;

          if vxd.x < x1 then x1 := vxd.x;
          if vxd.x > x2 then x2 := vxd.x;
          if vxd.y < y1 then y1 := vxd.y;
          if vxd.y > y2 then y2 := vxd.y;
          if vxd.z < z1 then z1 := vxd.z;
          if vxd.z > z2 then z2 := vxd.z;
        end;
    end;

  if isempty then exit;

  center.x := x1 + (x2 - x1) /2;
  center.y := y1 + (y2 - y1) /2;
  center.z := z1 + (z2 - z1) /2;

  for i := 0 to polys.count - 1 do
  begin
    surf := GetPoly(i);
    for j := 0 to surf.vxds.count - 1 do
      begin
        vxd := surf.GetVXD(j);
        x1 := sqr(vxd.x - center.x) + sqr(vxd.y - center.y) + sqr(vxd.z - center.z);
        if x1 > radius then radius := x1;
      end;
  end;

  radius := sqrt(radius);
end;


Function TMeshes.GetItem(n:integer):T3DPMesh;
begin
 if (n<0) or (n>=count) then raise EListError.CreateFmt('TMeshes Index is out of bounds: %d',[n]);
 Result:=T3DPMesh(List[n]);
end;

Procedure TMeshes.SetItem(n:integer;v:T3DPMesh);
begin
 if (n<0) or (n>=count) then raise EListError.CreateFmt('TMeshes Index is out of bounds: %d',[n]);
 List[n]:=v;
end;

end.
