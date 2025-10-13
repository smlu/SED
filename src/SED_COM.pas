unit SED_COM;

interface
uses Windows, J_Level, ResourcePicker, sed_plugins, misc_utils, GlobalVars,
     u_undo, Forms, SysUtils, classes, Render, RenderGL, RenderSW, Geometry,
     values, u_multisel, menus, System.Win.ComObj;

type

THandles = class(TList)
  oname: string;
  function GetItem(n: Integer): TObject;
  procedure SetItem(n: Integer; v: TObject);
  function GetItemNoNIL(n: Integer): TObject;

  procedure FreeHandle(h: Integer);
  function NewHandle(v: TObject): Integer;

  Constructor Create(const name: string);
  Destructor Destroy; override;

// Property Items[n: Integer]: TObject read GetItem write SetItem; default;
end;


TSedCOM = class(ISed)
  function QueryInterface(const IID: TGUID; out Obj): HResult; override; stdcall;
  function AddRef: Integer; override; stdcall;
  function Release: Integer; override; stdcall;

  function GetVersion: Double; override; stdcall;
  function GetLevel:ISedLevel; override; stdcall; {ISEDLevel object}

  function GetMapMode: Integer; override; stdcall;
  procedure SetMapMode(mode: Integer); override; stdcall;
  function GetCurrentSector: Integer; override; stdcall;
  procedure SetCurrentSector(sc: Integer); override; stdcall;
  function GetCurrentThing: Integer; override; stdcall;
  procedure SetCurrentThing(th: Integer); override; stdcall;
  function GetCurrentLight: Integer; override; stdcall;
  procedure SetCurrentLight(lt: Integer); override; stdcall;
  procedure GetCurrentVertex(var sc,vx: Integer); override; stdcall;
  procedure SetCurrentVertex(sc,vx: Integer); override; stdcall;
  procedure GetCurrentSurface(var sc,sf: Integer); override; stdcall;
  procedure SetCurrentSurface(sc,sf: Integer); override; stdcall;
  procedure GetCurrentEdge(var sc,sf,ed: Integer); override; stdcall;
  procedure SetCurrentEdge(sc,sf,ed: Integer); override; stdcall;
  procedure GetCurrentFrame(var th,fr: Integer); override; stdcall;
  procedure SetCurrentFrame(th,fr: Integer); override; stdcall;

  procedure NewLevel(kind: Integer); override; stdcall;
  procedure LoadLevel(const filename: PChar); override; stdcall;

  {Different level editing functions}

  procedure FindBoundingBox(sec: Integer; var box:TSedBox); override; stdcall;
  procedure FindBoundingSphere(sec: Integer; var center: TSedVector3; radius: Double); override; stdcall;
  function FindCollideBox(sec: Integer; const bbox:TSedBox; const center: TSedVector3; var cbox:TSedBox): Boolean; override; stdcall;
  procedure FindSurfaceCenter(sc,sf: Integer; var center:TSedVector3); override; stdcall;
  procedure RotateVector(var vec: TSedVector3; const pyr: TSedVector3); override; stdcall;
  procedure RotatePoint(const point, pivot: TSedVector3; angle: Double; var point2: TSedVector3); override; stdcall;
  procedure GetPYR(const x,y,z:TSedVector3; var pyr:TSedVector3); override; stdcall;
  function IsSurfaceConvex(sc,sf: Integer): Boolean; override; stdcall;
  function IsSurfacePlanar(sc,sf: Integer): Boolean; override; stdcall;
  function IsSectorConvex(sec: Integer): Boolean; override; stdcall;
  function IsInSector(sec: Integer; const point: TSedVector3): Boolean; override; stdcall;
  function DoSectorsOverlap(sec1,sec2: Integer): Boolean; override; stdcall;
  function IsPointOnSurface(sec, surf: Integer; const point: TSedVector3): Boolean; override; stdcall;
  function FindSectorForThing(th: Integer): Integer; override; stdcall;
  function FindSectorForXYZ(x, y, z: Double): Integer; override; stdcall;
  function ExtrudeSurface(sc,sf: Integer; by: Double): Integer; override; stdcall;
  function CleaveSurface(sec, surf: Integer; const cnormal: TSedVector3; cp: TSedVector3): Integer; override; stdcall;
  function CleaveSector(sec: Integer; const cnormal: TSedVector3; const cp: TSedVector3): Integer; override; stdcall;
  function CleaveEdge(sec, surf, ed: Integer; const cnormal: TSedVector3; cp: TSedVector3): Boolean; override; stdcall;
  function JoinSurfaces(sc1,sf1, sc2,sf2: Integer): Boolean; override; stdcall;
  function PlanarizeSurface(sc,sf: Integer): Boolean; override; stdcall;
  function MergeSurfaces(sc,sf1,sf2: Integer): Integer; override; stdcall;
  function MergeSectors(sec1,sec2: Integer): Integer; override; stdcall;
  procedure CalculateDefaultUVNormals(sc,sf: Integer; orgvx: Integer; var un,vn:TSedVector3); override; stdcall;
  procedure CalculateUVNormals(sc,sf: Integer; var un,vn:TSedVector3); override; stdcall;
  procedure ArrangeTexture(sc,sf: Integer; orgvx: Integer; const un,vn:TSedVector3); override; stdcall;
  procedure ArrangeTextureBy(sec, surf: Integer; const un, vn: TSedVector3; const refp: TSedVector3; refu, refv: Double); override; stdcall;
  function IsTextureFlipped(sc,sf: Integer): Boolean; override; stdcall;
  procedure RemoveSurfaceReferences(sc,sf: Integer); override; stdcall;
  procedure RemoveSectorReferences(sec: Integer;surfs: Boolean); override; stdcall;
  function StitchSurfaces(sc1,sf1, sc2,sf2: Integer): Boolean; override; stdcall;
  function FindCommonEdges(sc1,sf1, sc2,sf2: Integer; var v11,v12, v21,v22: Integer): Boolean; override; stdcall;
  function DoSurfacesOverlap(sc1,sf1, sc2,sf2: Integer): Boolean; override; stdcall;
  function MakeAdjoin(sc,sf: Integer): Boolean; override; stdcall;
  function MakeAdjoinFromSectorUp(sc,sf: Integer; firstsc: Integer): Boolean; override; stdcall;
  function UnAdjoin(sc,sf: Integer): Boolean; override; stdcall;
  function CreateCubicSector(const pos: TSedVector3; const pnormal, edge: TSedVector3): Integer; override; stdcall;

  procedure StartUndo(const name: PChar); override; stdcall;
  procedure SaveUndoForThing(n: Integer; change: Integer); override; stdcall;
  procedure SaveUndoForLight(n: Integer; change: Integer); override; stdcall;
  procedure SaveUndoForSector(n: Integer; change: Integer;whatpart: Integer); override; stdcall;
  procedure ClearUndoBuffer; override; stdcall;
  procedure ApplyUndo; override; stdcall;

  {JED 0.92}
  function GetApplicationHandle: Integer; override; stdcall;
  function JoinSectors(sec1,sec2: Integer): Boolean; override; stdcall;

  function GetNumMultiselected(what: Integer): Integer; override; stdcall;
  procedure ClearMultiselection(what: Integer); override; stdcall;
  procedure RemoveFromMultiselection(what,n: Integer); override; stdcall;
  function GetSelectedSector(n: Integer): Integer; override; stdcall;
  function GetSelectedThing(n: Integer): Integer; override; stdcall;
  function GetSelectedLight(n: Integer): Integer; override; stdcall;

  procedure GetSelectedSurface(n: Integer; var sc,sf: Integer); override; stdcall;
  procedure GetSelectedEdge(n: Integer; var sc,sf,ed: Integer); override; stdcall;
  procedure GetSelectedVertex(n: Integer; var sc,vx: Integer); override; stdcall;
  procedure GetSelectedFrame(n: Integer; var th,fr: Integer); override; stdcall;

  function SelectSector(sc: Integer): Integer; override; stdcall;
  function SelectSurface(sc,sf: Integer): Integer; override; stdcall;
  function SelectEdge(sc,sf,ed: Integer): Integer; override; stdcall;
  function SelectVertex(sc,vx: Integer): Integer; override; stdcall;
  function SelectThing(th: Integer): Integer; override; stdcall;
  function SelectFrame(th,fr: Integer): Integer; override; stdcall;
  function SelectLight(lt: Integer): Integer; override; stdcall;

  function FindSelectedSector(sc: Integer): Integer; override; stdcall;
  function FindSelectedSurface(sc,sf: Integer): Integer; override; stdcall;
  function FindSelectedEdge(sc,sf,ed: Integer): Integer; override; stdcall;
  function FindSelectedVertex(sc,vx: Integer): Integer; override; stdcall;
  function FindSelectedThing(th: Integer): Integer; override; stdcall;
  function FindSelectedFrame(th,fr: Integer): Integer; override; stdcall;
  function FindSelectedLight(lt: Integer): Integer; override; stdcall;

  procedure Save(const filename: PChar); override; stdcall;
  procedure SaveAsSed(const filename: PChar); override; stdcall;
  procedure UpdateMap; override; stdcall;
  procedure SetPickerCMP(const cmp: PChar); override; stdcall;
  function PickResource(what: Integer; const cur: PChar):PChar; override; stdcall;
  function EditFlags(what: Integer; flags:LongInt):LongInt; override; stdcall;

  procedure ReloadTemplates; override; stdcall;
  procedure PanMessage(mtype: Integer; const msg:PChar); override; stdcall;
  procedure SendKey(shift: Integer; key: Integer); override; stdcall;
  function ExecuteMenu(const itemref: PChar): Boolean; override; stdcall;
  function GetSedSetting(const name: PChar):variant; override; stdcall;
  function IsLayerVisible(n: Integer): Boolean; override; stdcall;

  procedure CheckConsistencyErrors; override; stdcall;
  procedure CheckResources; override; stdcall;
  function GetNumConsistencyErrors: Integer; override; stdcall;
  function GetConsistencyErrorString(n: Integer):PChar; override; stdcall;
  function GetConsistencyErrorType(n: Integer): Integer; override; stdcall;
  function GetConsistencyErrorItemType(n: Integer): Integer; override; stdcall; // SED 0.1.0
  function GetConsistencyErrorSector(n: Integer): Integer; override; stdcall;
  function GetConsistencyErrorSurface(n: Integer): Integer; override; stdcall;
  function GetConsistencyErrorThing(n: Integer): Integer; override; stdcall;
  function GetConsistencyErrorCOG(n: Integer): Integer; override; stdcall;
  function IsPreviewActive: Boolean; override; stdcall;
  function GetSedString(what: Integer):PChar; override; stdcall;

  function GetProjectType: Integer; override; stdcall;                // SED 0.1.0
  procedure SetProjectType(kind: Integer); override; stdcall;         // SED 0.1.0

  {JED 0.93}
  function GetSedWindow(whichone: Integer): Integer; override; stdcall;

  function FileExtractExt(const filename:PChar):PChar; override; stdcall;
  function FileExtractPath(const filename:PChar):PChar; override; stdcall;
  function FileExtractName(const filename:PChar):PChar; override; stdcall;
  function FindProjectDirFile(const filename:PChar):PChar; override; stdcall;
  function IsFileContainer(const filename:PChar): Boolean; override; stdcall;
  function IsFileInContainer(const filename:PChar): Boolean; override; stdcall;

  function FileOpenDialog(const filename:PChar;filter:PChar):PChar; override; stdcall;
  function OpenFile(const filename:PChar): Integer; override; stdcall;
  function OpenGameFile(const filename:PChar): Integer; override; stdcall;
  function ReadFile(handle: Integer;buf: Pointer;size: Longint): Integer; override; stdcall;
  procedure SetFilePos(handle: Integer;pos: Longint); override; stdcall;
  function GetFilePos(handle: Integer): Longint; override; stdcall;
  function GetFileSize(handle: Integer): Longint; override; stdcall;
  function GetFileName(handle: Integer):PChar; override; stdcall;
  procedure CloseFile(handle: Integer); override; stdcall;

  function OpenTextFile(const filename:PChar): Integer; override; stdcall;
  function OpenGameTextFile(const filename:PChar): Integer; override; stdcall;
  function GetTextFileName(handle: Integer):PChar; override; stdcall;
  function ReadTextFileString(handle: Integer):PChar; override; stdcall;
  function TextFileEOF(handle: Integer): Boolean; override; stdcall;
  function TextFileCurrentLine(handle: Integer): Integer; override; stdcall;
  procedure CloseTextFile(handle: Integer); override; stdcall;

  function GobOpen(const filename:PChar): Integer; override; stdcall;
  function GobNumFiles(handle: Integer): Integer; override; stdcall;
  function GobGetFilename(handle: Integer;n: Integer):PChar; override; stdcall;
  function GobGetFullFilename(handle: Integer;n: Integer):PChar; override; stdcall;
  function GobGetFileSize(handle: Integer;n: Integer): Longint; override; stdcall;
  function GobGetFileOffset(handle: Integer;n: Integer): Longint; override; stdcall;
  procedure GobClose(handle: Integer); override; stdcall;

  function CreateWFRenderer(wnd: Integer; whichone: Integer): ISedWFRenderer; override; stdcall;

  Constructor Create;

  private
    files,
    tfiles,
    conts: THandles;
end;

TSedCOMLevel = class (ISedLevel)
  function QueryInterface(const IID: TGUID; out Obj): HResult; override; stdcall;
  function AddRef: Integer; override; stdcall;
  function Release: Integer; override; stdcall;

  procedure GetLevelHeader(var lh:TSedLevelHeader;rflags: Integer); override; stdcall;
  procedure SetLevelHeader(const lh:TSedLevelHeader;rflags: Integer); override; stdcall;

  function NumSectors : Integer; override; stdcall;
  function NumThings: Integer; override; stdcall;
  function NumLights: Integer; override; stdcall;
  function NumCogs: Integer; override; stdcall;

  {Sectors}
  function SectorAdd: Integer; override; stdcall;
  procedure SectorDelete(n: Integer); override; stdcall;

  procedure SectorGet(sec: Integer; var rec: TSedSectorRec; what: Integer); override; stdcall;
  procedure SectorSet(sec: Integer; const rec: TSedSectorRec; what: Integer); override; stdcall;

  function SectorNumVertices(sec: Integer): Integer; override; stdcall;
  function SectorNumSurfaces(sec: Integer): Integer; override; stdcall;

  procedure SectorGetVertex(sec, vn: Integer; var vert: TSedVector3); override; stdcall;
  procedure SectorSetVertex(sec, vn: Integer; const vert: TSedVector3); override; stdcall;

  function SectorAddVertex(sec: Integer; const vert: TSedVector3): Integer; override; stdcall;
  function SectorFindVertex(sec: Integer; const vert: TSedVector3): Integer; override; stdcall;
  function SectorDeleteVertex(sec: Integer; n: Integer): Integer; override; stdcall;

  function SectorAddSurface(sec: Integer): Integer; override; stdcall;
  procedure SectorDeleteSurface(sc, sf: Integer); override; stdcall;
  procedure SectorUpdate(sec: Integer); override; stdcall;

  {Surfaces}
  procedure SurfaceGet(sc,sf: Integer; var rec: TSedSurfaceRec; rflags: Integer); override; stdcall;
  procedure SurfaceSet(sc, sf: Integer; const rec: TSedSurfaceRec; rflags: Integer); override; stdcall;
  procedure SurfaceGetNormal(sc, sf: Integer; var n: TSedVector3); override; stdcall;
  procedure SurfaceUpdate(sc, sf: Integer; how: Integer); override; stdcall;
  function SurfaceNumVertices(sc, sf: Integer): Integer; override; stdcall;
  function SurfaceGetVertexNum(sc, sf, vx: Integer): Integer; override; stdcall;
  procedure SurfaceSetVertexNum(sc, sf, vx: Integer; secvx: Integer); override; stdcall;
  function SurfaceAddVertex(sc, sf: Integer; secvx: Integer): Integer; override; stdcall;
  function SurfaceInsertVertex(sc, sf: Integer; at: Integer; secvx: Integer): Integer; override; stdcall;
  procedure SurfaceDeleteVertex(sc, sf: Integer; n: Integer); override; stdcall;
  procedure SurfaceGetVertexUV(sc, sf, vx: Integer; var u,v: single); override; stdcall;
  procedure SurfaceSetVertexUV(sc, sf, vx: Integer; u,v: single); override; stdcall;
  procedure SurfaceGetVertexLight(sc, sf, vx: Integer; var color: TSedColor); override; stdcall;
  procedure SurfaceSetVertexLight(sc, sf, vx: Integer; const color: TSedColor); override; stdcall;

  {Things}
  function ThingAdd: Integer; override; stdcall;
  procedure ThingDelete(th: Integer); override; stdcall;
  procedure ThingGet(th: Integer; var rec: TSedThingRec; rflags: Integer); override; stdcall;
  procedure ThingSet(th: Integer; const rec: TSedThingRec; rflags: Integer); override; stdcall;
  procedure ThingUpdate(th: Integer); override; stdcall;

  {Lights}
  function LightAdd: Integer; override; stdcall;
  procedure LightDelete(lt: Integer); override; stdcall;
  procedure LightGet(lt: Integer; var rec: TSedLightRec; rflags: Integer); override; stdcall;
  procedure LightSet(lt: Integer; const rec: TSedLightRec; rflags: Integer); override; stdcall;
  procedure LightUpdate(lt: Integer); override; stdcall;

  {Layers}
  function NumLayers: Integer; override; stdcall;
  function LayerGetName(n: Integer): PChar; override; stdcall;
  function LayerAdd(const name: PChar): Integer; override; stdcall;

  {JED 0.92}
  function ThingNumValues(th: Integer): Integer; override; stdcall;
  function ThingValueGetName(th,n: Integer):PChar; override; stdcall;
  function ThingValueGetData(th,n: Integer):PChar; override; stdcall;
  procedure ThingValueSetData(th,n: Integer; const val:PChar); override; stdcall;

  procedure ThingFrameGet(th,n: Integer; var pos, pyr: TSedVector3); override; stdcall;
  procedure ThingFrameSet(th,n: Integer; const pos, pyr: TSedVector3); override; stdcall;

  function ThingValueAdd(th: Integer; const name,val: PChar): Integer; override; stdcall;
  procedure ThingValueInsert(th,n: Integer; const name,val: PChar); override; stdcall;
  procedure ThingValueDelete(th,n: Integer); override; stdcall;

  {COGs}
  function CogAdd(const name:PChar): Integer; override; stdcall;
  procedure CogDelete(n: Integer); override; stdcall;
  procedure CogUpdate(cg: Integer); override; stdcall;

  function CogGetFilename(cg: Integer):PChar; override; stdcall;
  function CogNumValues(cg: Integer): Integer; override; stdcall;

  function CogValueGetName(cg,n: Integer):PChar; override; stdcall;
  function CogValueGetType(cg,n: Integer): Integer; override; stdcall;

  function CogValueGet(cg,n: Integer):PChar; override; stdcall;
  function CogValueSet(cg,n: Integer; const val: PChar): Boolean; override; stdcall;

  function CogValueAdd(cg: Integer; const name,val: PChar; vtype: Integer): Integer; override; stdcall;
  procedure CogValueInsert(cg,n: Integer; const name,val: PChar; vtype: Integer); override; stdcall;
  procedure CogValueDelete(cg,n: Integer); override; stdcall;
end;

TCOMWFRenderer = class(ISedWFRenderer)
  function QueryInterface(const IID: TGUID; out Obj): HResult; override; stdcall;
  function AddRef: Integer; override; stdcall;
  function Release: Integer; override; stdcall;

  {Renderer attributes}
  function GetRendererDouble(what: Integer): Double; override; stdcall;
  procedure SetRendererDouble(what: Integer;val: Double); override; stdcall;
  procedure GetRendererVector(what: Integer; var x,y,z: Double); override; stdcall;
  procedure SetRendererVector(what: Integer;x,y,z: Double); override; stdcall;

  function NSelected: Integer; override; stdcall;
  function GetNSelected(n: Integer): Integer; override; stdcall;
  procedure SetViewPort(x,y,w,h: Integer); override; stdcall;
  procedure SetColor(what,r,g,b:byte); override; stdcall;
  procedure SetPointSize(size: Double); override; stdcall;
  procedure BeginScene; override; stdcall;
  procedure EndScene; override; stdcall;
  procedure SetCulling(how: Integer); override; stdcall;

  procedure DrawSector(sc: Integer); override; stdcall;
  procedure DrawSurface(sc,sf: Integer); override; stdcall;
  procedure DrawThing(th: Integer); override; stdcall;

  procedure DrawLine(x1,y1,z1, x2,y2,z2: Double); override; stdcall;
  procedure DrawVertex(x, y, z: Double); override; stdcall;
  procedure DrawGrid; override; stdcall;

  procedure BeginPick(x,y: Integer); override; stdcall;
  procedure EndPick; override; stdcall;

  procedure PickSector(sc: Integer;id: Integer); override; stdcall;
  procedure PickSurface(sc,sf: Integer;id: Integer); override; stdcall;
  procedure PickLine(x1,y1,z1, x2,y2,z2: Double;id: Integer); override; stdcall;
  procedure PickVertex(x, y, z: Double;id: Integer); override; stdcall;

  procedure BeginRectPick(x1,y1, x2,y2: Integer); override; stdcall;
  procedure EndRectPick; override; stdcall;
  function IsSectorInRect(sc: Integer): Boolean; override; stdcall;
  function IsSurfaceInRect(sc,sf: Integer): Boolean; override; stdcall;
  function IsLineInRect(x1,y1,z1, x2,y2,z2: Double): Boolean; override; stdcall;
  function IsVertexInRect(x, y, z: Double): Boolean; override; stdcall;

  function GetXYZonPlaneAt(scX,scY: Integer; const pnormal:TSedVector3; pX,pY,pZ: Double; var x, y, z: Double): Boolean; override; stdcall;
  function GetGridAt(scX,scY: Integer; var x, y, z: Double): Boolean; override; stdcall;
  procedure GetNearestGridNode(iX,iY,iZ: Double; Var x, y, z: Double); override; stdcall;
  procedure ProjectPoint(x,y,z: Double; Var winX, winY: Integer); override; stdcall;
  procedure UnProjectPoint(winX, winY: Integer; WinZ: Double; var x,y,z: Double); override; stdcall;
  function IsSurfaceFacing(sc,sf: Integer): Boolean; override; stdcall;

  function HandleWMQueryPal: Integer; override; stdcall;
  function HandleWMChangePal: Integer; override; stdcall;

  Constructor Create(wnd: Integer;which: Integer);

  private
    Rend: TRenderer;
end;


function GetSedCOM: ISed;

implementation
uses  Item_edit, u_MsgForm, u_Tools, JED_Main,lev_utils,(*JED_OLE,*) listRes,
      u_CogForm, FlagEditor, U_SCFEdit,U_Options, u_tbar, Cons_Checker, u_Preview,
      Files, FileOperations, FileDialogs;

var com: ISed;
    comlevel: TSedCOMLevel;
    tmpstr: string;
    tmpastr: array[0..2047] of Char;
    ForeWnd: Integer;

function THandles.GetItemNoNIL(n: Integer): TObject;
begin
  if (n < 0) or (n>=count) then raise EListError.CreateFmt('Invalid %s handle: %d', [oname, n]);
    Result := TObject(List[n]);
  if (result=nil) then
    raise EListError.CreateFmt('Invalid %s handle: %d', [oname, n]);
end;

function THandles.GetItem(n: Integer): TObject;
begin
  if (n < 0) or (n>=count) then
    raise EListError.CreateFmt('Invalid %s handle: %d', [oname, n]);
  Result := TObject(List[n]);
end;

procedure THandles.SetItem(n: Integer;v: TObject);
begin
  if (n < 0) or (n>=count) then
    raise EListError.CreateFmt('Invalid %s handle: %d', [oname, n]);
  List[n]:=v;
end;

procedure THandles.FreeHandle(h: Integer);
var v: TObject;
begin
  v := GetItem(h);
  v.free;
  SetItem(h,nil);
end;

function THandles.NewHandle(v: TObject): Integer;
var i: Integer;
begin
  if v=nil then begin
    Result := -1;
    exit;
  end;

  for i := 0 to Count - 1 do
    if Items[i] = nil then
    begin
      Items[i] := v;
      Result   := i;
      exit;
    end;

  Result := Add(v);
end;

Constructor THandles.Create(const name: string);
begin
  oname := name;
end;

Destructor THandles.Destroy;
var i: Integer;
begin
  for i:=0 to Count-1 do
     TObject(List[i]).Free;
end;

function GetSedCOM: ISed;
begin
   if com = nil then com := TSedCOM.Create;
   Result := com;
end;

procedure SaveCurApp;
begin
  ForeWnd:=GetForeGroundWindow;
  SetForeGroundWindow(JedMain.Handle);
end;

procedure RestoreCurApp;
begin
  SetForeGroundWindow(ForeWnd);
end;

Constructor TSedCOM.Create;
begin
  files  := THandles.Create('file');
  tfiles := THandles.Create('text file');
  conts  := THandles.Create('GOB');
end;

function TSedCOM.GetVersion: Double;
begin
 ValDouble(SedVerNum, Result);
 Result := result * 100;
end;

function TSedCOM.GetLevel: ISedLevel; {ISEDLevel object}
begin
 if comlevel = nil then comlevel := TSedCOMLevel.Create;
 Result := comLevel;
end;

function TSedCOM.QueryInterface(const IID: TGUID; out Obj): HResult;
begin
  Result := CLASS_E_CLASSNOTAVAILABLE;
end;

function TSedCOM.AddRef: Integer;
begin
  Result := 1;
end;

function TSedCOM.Release: Integer;
begin
  Result := 0;
end;

{Level}

function TSedCOMLevel.QueryInterface(const IID: TGUID; out Obj): HResult;
begin
  Result := CLASS_E_CLASSNOTAVAILABLE;
end;

function TSedCOMLevel.AddRef: Integer;
begin
  Result := 1;
end;

function TSedCOMLevel.Release: Integer;
begin
  Result := 0;
end;

procedure TSedCOMLevel.SectorGet(sec: Integer; var rec:TSedSectorRec;what: Integer);
begin
  with level.sectors[sec] do
    begin
      if (what and s_flags)      <> 0 then rec.flags       := flags;
      if (what and s_ambient)    <> 0 then rec.ambient     := TSedColor(ambient);
      if (what and s_extra)      <> 0 then rec.extraLight  := TSedColor(extraLight);
      if (what and s_pointlight) <> 0 then rec.pointLight  := TSedPointLight(pointLight);
      if (what and s_cmp)        <> 0 then rec.aColorMap   := PChar(colormap);
      if (what and s_tint)       <> 0 then rec.tint        := TSedColor(tint);
      if (what and s_sound)      <> 0 then rec.aSound      := PChar(sound);
      if (what and s_sndvol)     <> 0 then rec.soundVolume := soundVolume;
      if (what and s_thrust)     <> 0 then rec.thrust      := TSedVector3(thrust);
      if (what and s_layer)      <> 0 then rec.layer       := layer;
    end;
end;

procedure TSedCOMLevel.SectorSet(sec: Integer;const rec:TSedSectorRec;what: Integer);
begin
  with level.sectors[sec] do
    begin
      if (what and s_flags)      <> 0 then flags       := rec.flags;
      if (what and s_ambient)    <> 0 then ambient     := TColorF(rec.ambient);
      if (what and s_extra)      <> 0 then extraLight  := TColorF(rec.extraLight);
      if (what and s_pointlight) <> 0 then pointLight  := TPointLight(rec.pointLight);
      if (what and s_cmp)        <> 0 then colormap    := rec.aColorMap;
      if (what and s_tint)       <> 0 then tint        := TColorF(rec.tint);
      if (what and s_sound)      <> 0 then sound       := rec.aSound;
      if (what and s_sndvol)     <> 0 then soundVolume := rec.soundVolume;
      if (what and s_thrust)     <> 0 then thrust      := TVector(rec.thrust);
      if (what and s_layer)      <> 0 then layer       := rec.layer;
    end;
end;

procedure TSedCOMLevel.GetLevelHeader(var lh:TSedLevelHeader;rflags: Integer);
begin
  with level.header do
  begin
    if (rflags and lh_version)  <> 0 then lh.version := version;
    if (rflags and lh_gravity)  <> 0 then lh.gravity := gravity;
    if (rflags and lh_SkyZ)     <> 0 then lh.ceilingSkyHeight := ceilingSky.height;
    if (rflags and lh_CSkyOffs) <> 0 then
    begin
      lh.ceilingSkyOffset.X := ceilingSky.offset.X;
      lh.ceilingSkyOffset.Y := ceilingSky.offset.Y;
    end;

    if (rflags and lh_HorDist)  <> 0 then lh.horizonSkyDistance     := horizonSky.distance;
    if (rflags and lh_HorPPR)   <> 0 then lh.horizonSkyPixelsPerRev := horizonSky.pixelsPerRev;
    if (rflags and lh_HSkyOffs) <> 0 then
    begin
      lh.horizonSkyOffset.X := horizonSky.offset.X;
      lh.horizonSkyOffset.Y := horizonSky.offset.Y;
    end;

    if (rflags and lh_MipMapDist) <> 0 then
    begin
      lh.mipMapDistances[0] := mipMapDistances[1];
      lh.mipMapDistances[1] := mipMapDistances[2];
      lh.mipMapDistances[2] := mipMapDistances[3];
      lh.mipMapDistances[3] := mipMapDistances[4];
    end;

    if (rflags and lh_LODDist) <> 0 then
    begin
      lh.lodDistances[0] := lodDistances[1];
      lh.lodDistances[1] := lodDistances[2];
      lh.lodDistances[2] := lodDistances[3];
      lh.lodDistances[3] := lodDistances[4];
    end;

    if (rflags and lh_PerspDist)   <> 0 then lh.perspectiveDistance := perspectiveDistance;
    if (rflags and lh_GouraudDist) <> 0 then lh.gouraudDistance := gouraudDistance;
    if (rflags and lh_ppu)         <> 0 then lh.ppunit := level.ppunit;
    if (rflags and lh_MasterCMP)   <> 0 then
      begin
        lh.aMasterCmp := nil;
        if (level.kind <> IJIM) and (not level.masterCMP.IsEmpty)
          then lh.aMasterCmp := PChar(level.masterCMP);
      end;

    if (rflags and lh_Fog) <> 0 then
    begin
      lh.fog.enabled  := fog.enabled;
      lh.fog.color.r  := fog.color.r;
      lh.fog.color.g  := fog.color.g;
      lh.fog.color.b  := fog.color.b;
      lh.fog.color.a  := fog.color.a;
      lh.fog.fogStart := fog.fogStart;
      lh.fog.fogEnd   := fog.fogEnd;
    end;
  end;
end;

procedure TSedCOMLevel.SetLevelHeader(const lh:TSedLevelHeader;rflags: Integer);
begin
 with level.header do
 begin
  if (rflags and lh_version)  <> 0 then version := lh.version;
  if (rflags and lh_gravity)  <> 0 then gravity := lh.gravity;
  if (rflags and lh_SkyZ)     <> 0 then ceilingSky.height := lh.ceilingSkyHeight;
  if (rflags and lh_CSkyOffs) <> 0 then
  begin
   ceilingSky.offset.X := lh.ceilingSkyOffset.X;
   ceilingSky.offset.Y := lh.ceilingSkyOffset.Y;
  end;

  if (rflags and lh_HorDist)  <> 0 then horizonSky.distance := lh.horizonSkyDistance;
  if (rflags and lh_HorPPR)   <> 0 then horizonSky.pixelsPerRev := lh.horizonSkyPixelsPerRev;
  if (rflags and lh_HSkyOffs) <> 0 then
  begin
   horizonSky.offset.X := lh.horizonSkyOffset.X;
   horizonSky.offset.Y := lh.horizonSkyOffset.Y;
  end;

  if (rflags and lh_MipMapDist) <> 0 then
  begin
   mipmapDistances[1] := lh.mipmapDistances[0];
   mipmapDistances[2] := lh.mipmapDistances[1];
   mipmapDistances[3] := lh.mipmapDistances[2];
   mipmapDistances[4] := lh.mipmapDistances[3];
  end;

  if (rflags and lh_LODDist) <> 0 then
  begin
   lodDistances[1] := lh.lodDistances[0];
   lodDistances[2] := lh.lodDistances[1];
   lodDistances[3] := lh.lodDistances[2];
   lodDistances[4] := lh.lodDistances[3];
  end;

  if (rflags and lh_PerspDist)   <> 0 then perspectiveDistance := lh.perspectiveDistance;
  if (rflags and lh_GouraudDist) <> 0 then gouraudDistance := lh.gouraudDistance;
  if (rflags and lh_ppu)         <> 0 then level.ppunit := lh.ppunit;
  if (rflags and lh_MasterCMP)   <> 0 then level.masterCMP := lh.aMasterCmp;

  if (rflags and lh_Fog) <> 0 then
    begin
      fog.enabled  := lh.fog.enabled;
      fog.color.r  := lh.fog.color.r;
      fog.color.g  := lh.fog.color.g;
      fog.color.b  := lh.fog.color.b;
      fog.color.a  := lh.fog.color.a;
      fog.fogStart := lh.fog.fogStart;
      fog.fogEnd   := lh.fog.fogEnd;
    end;
 end;
end;

function TSedCOMLevel.NumSectors: Integer;
begin
  Result := level.sectors.Count;
end;

function TSedCOMLevel.NumThings: Integer;
begin
  Result := level.things.Count;
end;

function TSedCOMLevel.NumLights: Integer;
begin
  Result := level.lights.Count;
end;

function TSedCOMLevel.NumCogs: Integer;
begin
  Result := level.cogs.Count;
end;

{Sectors}
function TSedCOMLevel.SectorAdd: Integer;
var sec:TJKSector;
begin
  sec := level.NewSector;
  Result := level.sectors.Add(sec);
  level.RenumSecs;
  JedMain.SectorAdded(sec);
end;

procedure TSedCOMLevel.SectorDelete(n: Integer);
begin
  lev_utils.DeleteSector(level, n);
end;

function TSedCOMLevel.SectorNumVertices(sec: Integer): Integer;
begin
  Result := level.sectors[sec].vertices.Count;
end;

function TSedCOMLevel.SectorNumSurfaces(sec: Integer): Integer;
begin
  Result := level.sectors[sec].surfaces.Count;
end;

procedure TSedCOMLevel.SectorGetVertex(sec, vn: Integer; var vert: TSedVector3);
var v: TJKVertex;
begin
  v := level.sectors[sec].vertices[vn];
  vert.x := v.x;
  vert.y := v.y;
  vert.z := v.z;
end;

procedure TSedCOMLevel.SectorSetVertex(sec, vn: Integer; const vert: TSedVector3);
var v: TJKVertex;
begin
  v := level.sectors[sec].vertices[vn];
  v.x := vert.x;
  v.y := vert.y;
  v.z := vert.z;
end;

function TSedCOMLevel.SectorAddVertex(sec: Integer; const vert: TSedVector3): Integer;
var v: TJKVertex;
begin
  v := level.sectors[sec].NewVertex;
  v.x := vert.x;
  v.y := vert.y;
  v.z := vert.z;
  Result := level.sectors[sec].vertices.Count - 1;
end;

function TSedCOMLevel.SectorFindVertex(sec: Integer; const vert: TSedVector3): Integer;
begin
  Result := level.sectors[sec].FindVX(vert.x, vert.y, vert.z);
end;

function TSedCOMLevel.SectorDeleteVertex(sec: Integer;n: Integer): Integer;
begin
  with level.sectors[sec] do
  begin
    vertices[n].Free;
    vertices.Delete(n);
    renumber;
  end;
end;

function TSedCOMLevel.SectorAddSurface(sec: Integer): Integer;
begin
  with level.sectors[sec] do
  begin
    Result := surfaces.add(newSurface);
  end;
end;

procedure TSedCOMLevel.SectorDeleteSurface(sc, sf: Integer);
begin
  lev_utils.DeleteSurface(level, sc, sf);
end;

procedure TSedCOMLevel.SectorUpdate(sec: Integer);
var asec:TJKSector;
begin
   asec := level.sectors[sec];
   asec.Renumber;
   JedMain.SectorChanged(asec);
end;

  {Surfaces}
procedure TSedCOMLevel.SurfaceGet(sc,sf: Integer; var rec:TSedSurfaceRec;rflags: Integer);
begin
  with level.sectors[sc].surfaces[sf] do
  begin
    if (rflags and sf_adjoin ) <> 0 then
    begin
      if adjoin = nil then begin rec.adjoinSec := -1; rec.adjoinSurf := -1; end
      else begin rec.adjoinSec := adjoin.sector.num; rec.adjoinSurf := adjoin.num; end;
    end;
    if (rflags and sf_adjoinflags) <> 0 then rec.adjoinflags := AdjoinFlags;
    if (rflags and sf_SurfFlags)   <> 0 then rec.surfflags   := SurfFlags;
    if (rflags and sf_FaceFlags)   <> 0 then rec.faceflags   := FaceFlags;
    if (rflags and sf_Material)    <> 0 then rec.aMaterial    := PChar(material);
    if (rflags and sf_geo)         <> 0 then rec.geo         := geo;
    if (rflags and sf_light)       <> 0 then rec.light       := light;
    if (rflags and sf_tex)         <> 0 then rec.tex         := tex;
    if (rflags and sf_ExtraLight)  <> 0 then rec.ExtraLight  := TSedColor(ExtraLight);
    if (rflags and sf_txscale)     <> 0 then
    begin
      rec.uscale:=uscale;
      rec.vscale:=vscale;
    end;
  end;
end;

procedure TSedCOMLevel.SurfaceSet(sc,sf: Integer; const rec: TSedSurfaceRec; rflags: Integer);
begin
  with level.sectors[sc].surfaces[sf] do
  begin
    if (rflags and sf_adjoin) <> 0 then
    begin
      if rec.adjoinSec < 0 then adjoin := nil else
      adjoin := level.sectors[rec.adjoinSec].surfaces[rec.adjoinSurf];
    end;

    if (rflags and sf_adjoinflags) <> 0 then adjoinflags := rec.adjoinflags;
    if (rflags and sf_SurfFlags)   <> 0 then surfflags   := rec.surfflags;
    if (rflags and sf_FaceFlags)   <> 0 then faceflags   := rec.faceflags;
    if (rflags and sf_Material)    <> 0 then material    := rec.aMaterial;
    if (rflags and sf_geo)         <> 0 then geo         := rec.geo;
    if (rflags and sf_light)       <> 0 then light       := rec.light;
    if (rflags and sf_tex)         <> 0 then tex         := rec.tex;
    if (rflags and sf_ExtraLight)  <> 0 then extraLight  := TColorF(rec.extraLight);
    if (rflags and sf_txscale)     <> 0 then
    begin
      uscale := rec.uscale;
      vscale := rec.vscale;
    end;
  end;
end;

procedure TSedCOMLevel.SurfaceGetNormal(sc,sf: Integer; var n: TSedVector3);
begin
  with level.sectors[sc].surfaces[sf].normal do
  begin
    n.dx := dx;
    n.dy := dy;
    n.dz := dz;
  end;
end;

procedure TSedCOMLevel.SurfaceUpdate(sc,sf: Integer;how: Integer);
var surf:TJKSurface;
begin
  surf := level.sectors[sc].surfaces[sf];
  if how and su_texture   <> 0 then surf.RecalcAll else surf.Recalc;
  if how and su_floorflag <> 0 then surf.CheckIfFloor;
  if how and su_sector    <> 0 then JedMain.SectorChanged(surf.sector);
end;

function TSedCOMLevel.SurfaceNumVertices(sc,sf: Integer): Integer;
begin
  Result := level.sectors[sc].surfaces[sf].vertices.count;
end;

function TSedCOMLevel.SurfaceGetVertexNum(sc,sf,vx: Integer): Integer;
begin
  Result := level.sectors[sc].surfaces[sf].vertices[vx].num;
end;

procedure TSedCOMLevel.SurfaceSetVertexNum(sc,sf,vx: Integer;secvx: Integer);
begin
  with level.sectors[sc] do
  begin
    surfaces[sf].vertices[vx] := vertices[secvx];
  end;
end;

function TSedCOMLevel.SurfaceAddVertex(sc,sf: Integer;secvx: Integer): Integer;
begin
  with level.sectors[sc] do
  begin
    Result := surfaces[sf].AddVertex(vertices[secvx]);
  end;
end;

function TSedCOMLevel.SurfaceInsertVertex(sc,sf: Integer;at: Integer;secvx: Integer): Integer;
begin
  with level.sectors[sc] do
  begin
    Result := surfaces[sf].InsertVertex(at, vertices[secvx]);
  end;
  Result := at;
end;

procedure TSedCOMLevel.SurfaceDeleteVertex(sc,sf: Integer;n: Integer);
begin
  level.sectors[sc].surfaces[sf].DeleteVertex(n);
end;

procedure TSedCOMLevel.SurfaceGetVertexUV(sc,sf,vx: Integer; var u,v:single);
var tv:TTXVertex;
begin
  tv := level.sectors[sc].surfaces[sf].txvertices[vx];
  u := tv.u;
  v := tv.v;
end;

procedure TSedCOMLevel.SurfaceSetVertexUV(sc,sf,vx: Integer;u,v:single);
var tv: TTXVertex;
begin
  tv := level.sectors[sc].surfaces[sf].txvertices[vx];
  tv.u := u;
  tv.v := v;
end;

procedure TSedCOMLevel.SurfaceGetVertexLight(sc, sf, vx: Integer; var color: TSedColor);
var tv:TTXVertex;
begin
  tv := level.sectors[sc].surfaces[sf].txvertices[vx];
  color := TSedColor(tv.color);
end;

procedure TSedCOMLevel.SurfaceSetVertexLight(sc, sf, vx: Integer; const color: TSedColor);
var tv:TTXVertex;
begin
  tv := level.sectors[sc].surfaces[sf].txvertices[vx];
  tv.color := TColorF(color);
end;

{Things}

function TSedCOMLevel.ThingAdd: Integer;
var th:TJKThing;
begin
  th := level.NewThing;
  Result := level.things.Add(th);
  level.RenumThings;
  JedMain.ThingAdded(th);
end;

procedure TSedCOMLevel.ThingDelete(th: Integer);
begin
  Lev_Utils.DeleteThing(level, th);
end;

procedure TSedCOMLevel.ThingGet(th: Integer; var rec: TSedThingRec; rflags: Integer);
begin
  with level.things[th] do
  begin
    if (rflags and th_name) <> 0 then rec.aName := PChar(name);
    if (rflags and th_sector) <> 0 then
    begin
      if sec = nil then rec.sector := -1
      else rec.sector := sec.num;
    end;

    if (rflags and th_position)<>0 then
    begin
      rec.position.x := x;
      rec.position.y := y;
      rec.position.z := z;
    end;

    if (rflags and th_orientation)<>0 then
    begin
      rec.rotation.pitch := PCH;
      rec.rotation.yaw   := YAW;
      rec.rotation.roll  := ROL;
    end;

    if (rflags and th_layer) <> 0 then rec.layer := layer;
  end;
end;

procedure TSedCOMLevel.ThingSet(th: Integer;const rec: TSedThingRec; rflags: Integer);
begin
  with level.things[th] do
  begin
    if (rflags and th_name) <> 0 then name := rec.aName;
    if (rflags and th_sector) <> 0 then
    begin
      if rec.Sector < 0 then sec := nil else
      sec := Level.Sectors[rec.Sector];
    end;

    if (rflags and th_position)<>0 then
    begin
      X:=rec.position.x;
      Y:=rec.position.y;
      Z:=rec.position.z;
    end;

    if (rflags and th_orientation)<>0 then
    begin
      PCH:=rec.rotation.pitch;
      YAW:=rec.rotation.yaw;
      ROL:=rec.rotation.roll;
    end;

    if (rflags and th_layer)<>0 then layer:=rec.layer;
  end;
end;

procedure TSedCOMLevel.ThingUpdate(th: Integer);
begin
 Level.RenumThings;
 JedMain.ThingChanged(level.Things[th]);
end;

{Lights}

function TSedCOMLevel.LightAdd: Integer;
var lt: TSedLight;
begin
  lt := Level.NewLight;
  Result := Level.lights.Add(lt);
  JedMain.LightAdded(lt);
end;

procedure TSedCOMLevel.LightDelete(lt: Integer);
begin
  Lev_Utils.DeleteLight(Level, lt);
end;

procedure TSedCOMLevel.LightGet(lt: Integer; var rec: TSedLightRec; rflags: Integer);
begin
  with level.lights[lt] do
    begin
      if (rflags and lt_position)<>0 then
        begin
          rec.position.x := position.x;
          rec.position.y := position.y;
          rec.position.z := position.z;
        end;

      if (rflags and lt_intensity) <> 0 then rec.Intensity:=Intensity;
      if (rflags and lt_range) <> 0 then rec.Range:=range;
      if (rflags and lt_color) <> 0 then
        begin
          rec.color.r := color.r;
          rec.color.g := color.g;
          rec.color.b := color.b;
          rec.color.a := color.a
        end;

      if (rflags and lt_flags) <> 0 then rec.flags:=flags;
      if (rflags and lt_layer) <> 0 then rec.layer:=layer;
    end;
end;

procedure TSedCOMLevel.LightSet(lt: Integer;const rec:TSedLightRec;rflags: Integer);
begin
  with level.lights[lt] do
    begin
    if (rflags and lt_position)<>0 then
      begin
        position.x := rec.position.x;
        position.y := rec.position.y;
        position.z := rec.position.z;
      end;

    if (rflags and lt_intensity)<>0 then intensity := rec.intensity;
    if (rflags and lt_range)<>0 then range := rec.range;
    if (rflags and lt_color)<>0 then
    begin
      color.r := rec.color.r;
      color.g := rec.color.g;
      color.b := rec.color.b;
      color.a := rec.color.a
    end;

    if (rflags and lt_flags) <> 0 then flags := rec.flags;
    if (rflags and lt_layer) <> 0 then layer := rec.layer;
  end;
end;

procedure TSedCOMLevel.LightUpdate(lt: Integer);
begin;
  JedMain.LightChanged(level.Lights[lt]);
end;

function TSedCOMLevel.NumLayers: Integer;
begin
  Result := Level.Layers.count;
end;

function TSedCOMLevel.LayerGetName(n: Integer): PChar;
begin
  Result := PChar(Level.GetLayerName(n));
end;

function TSedCOMLevel.LayerAdd(const name: PChar): Integer;
begin
  Result := Level.AddLayer(name);
end;

{0.92}
function TSedCOMLevel.ThingNumValues(th: Integer): Integer;
begin
  Result := Level.Things[th].Vals.count;
end;

function TSedCOMLevel.ThingValueGetName(th,n: Integer): PChar;
begin
  Result := PChar(Level.Things[th].Vals[n].Name);
end;

function TSedCOMLevel.ThingValueGetData(th,n: Integer): PChar;
begin
  Result := PChar(Level.Things[th].Vals[n].AsString);
end;

procedure TSedCOMLevel.ThingValueSetData(th,n: Integer; const val: PChar);
var v: TTPLValue;
begin
  v := Level.Things[th].Vals[n];
  S2TPLVal(v.name + '=' + val,v);
end;

procedure TSedCOMLevel.ThingFrameGet(th, n: Integer; var pos, pyr: TSedVector3);
begin
  Level.Things[th].Vals[n].GetFrame(pos.x, pos.y, pos.z, pyr.pitch, pyr.yaw, pyr.roll);
end;

procedure TSedCOMLevel.ThingFrameSet(th,n: Integer; const pos, pyr: TSedVector3);
begin
  Level.Things[th].Vals[n].SetFrame(pos.x, pos.y, pos.z, pyr.pitch, pyr.yaw, pyr.roll);
end;

function TSedCOMLevel.ThingValueAdd(th: Integer; const name, val: PChar): Integer;
var v:TTPLValue;
begin
  v:=TTplValue.Create;
  S2TPLVal(name + '=' + val, v);
  Result := Level.Things[th].Vals.Add(v);
end;

procedure TSedCOMLevel.ThingValueInsert(th,n: Integer; const name,val: PChar);
var v:TTPLValue;
begin
  v := TTplValue.Create;
  S2TPLVal(name + '=' + val, v);
  Level.Things[th].Vals.Insert(n, v);
end;

procedure TSedCOMLevel.ThingValueDelete(th,n: Integer);
var v:TTPLValue;
begin
  v := Level.Things[th].Vals[n];
  Level.Things[th].Vals.Delete(n);
  v.free;
end;


{COGs}
function TSedCOMLevel.CogAdd(const name: PChar): Integer;
var cg:TCOG;
    cf:TCogFile;
    i: Integer;
    cv:TCOGValue;
begin
   cg := TCOG.Create;
   cg.Name:=name;

   cf := TCogFile.Create;
   cf.LoadNoLocals(Name);

   for i := 0 to cf.count - 1 do
   begin
    cv:=TCOGValue.Create;
    cg.Vals.Add(cv);
    cv.Assign(cf[i]);
   end;

   cf.Free;

   Result := Level.Cogs.Add(cg);
   CogForm.RefreshList;
   JedMain.LevelChanged;
end;

procedure TSedCOMLevel.CogDelete(n: Integer);
var cog:TCOG;
begin
  lev_utils.DeleteCOG(level,n);
end;

procedure TSedCOMLevel.CogUpdate(cg: Integer);
begin
  COgForm.UpdateCOG(cg);
end;

function TSedCOMLevel.CogGetFilename(cg: Integer): PChar;
begin
  Result := PChar(level.cogs[cg].name);
end;

function TSedCOMLevel.CogNumValues(cg: Integer): Integer;
begin
  Result := level.cogs[cg].vals.count;
end;

function TSedCOMLevel.CogValueGetName(cg,n: Integer): PChar;
begin
  Result := PChar(level.cogs[cg].vals[n].Name);
end;

function TSedCOMLevel.COGValueGetType(cg,n: Integer): Integer;
begin
  Result := Integer(level.cogs[cg].vals[n].cog_type);
end;

function TSedCOMLevel.CogValueGet(cg,n: Integer): PChar;
begin
  Result := PChar(level.cogs[cg].vals[n].AsString);
end;

function TSedCOMLevel.CogValueSet(cg,n: Integer; const val: PChar): Boolean;
begin
  Result := level.cogs[cg].vals[n].JedVal(val);
end;

function TSedCOMLevel.CogValueAdd(cg: Integer; const name,val: PChar; vtype: Integer): Integer;
var v:TCOGValue;
begin
   v := TCogValue.Create;
   v.name     := name;
   v.vtype    := GetVTypeFromCOGType(TCOG_TYpe(vtype));
   v.cog_type := TCOG_Type(vtype);
   v.Val(val);
   Result := level.cogs[cg].vals.Add(v);
end;

procedure TSedCOMLevel.CogValueInsert(cg,n: Integer; const name,val: PChar; vtype: Integer);
var v:TCOGValue;
begin
   v := TCogValue.Create;
   v.name     := name;
   v.vtype    := GetVTypeFromCOGType(TCOG_TYpe(vtype));
   v.cog_type := TCOG_Type(vtype);
   v.Val(val);
   level.cogs[cg].vals.Insert(n,v);
end;

procedure TSedCOMLevel.COGValueDelete(cg,n: Integer);
var v:TCOGValue;
begin
   v := level.cogs[cg].Vals[n];
   level.cogs[cg].Vals.Delete(n);
   v.free;
end;


{SEDCOM}

function TSedCOM.GetMapMode: Integer;
begin
  Result := JedMain.mapMode;
end;

procedure TSedCOM.SetMapMode(mode: Integer);
begin
  JedMain.SetMapMode(Mode);
end;

function TSedCOM.GetCurrentSector: Integer;
begin
  Result := JedMain.Cur_SC;
end;

procedure TSedCOM.SetCurrentSector(sc: Integer);
begin
  JedMain.SetCurSC(sc);
end;

function TSedCOM.GetCurrentThing: Integer;
begin
  Result := JedMain.Cur_TH;
end;

procedure TSedCOM.SetCurrentThing(th: Integer);
begin
  JedMain.SetCurTH(th);
end;

function TSedCOM.GetCurrentLight: Integer;
begin
  Result := JedMain.Cur_LT;
end;

procedure TSedCOM.SetCurrentLight(lt: Integer);
begin
  JedMain.SetCurLT(lt);
end;

procedure TSedCOM.GetCurrentVertex(var sc,vx: Integer);
begin
  sc := JedMain.Cur_SC;
  vx := JedMain.Cur_VX;
end;

procedure TSedCOM.SetCurrentVertex(sc,vx: Integer);
begin
  JedMain.SetCurVX(sc,vx);
end;

procedure TSedCOM.GetCurrentSurface(var sc,sf: Integer);
begin
  sc := JedMain.Cur_SC;
  sf := JedMain.Cur_SF;
end;

procedure TSedCOM.SetCurrentSurface(sc,sf: Integer);
begin
  JedMain.SetCurSF(sc,sf);
end;

procedure TSedCOM.GetCurrentEdge(var sc,sf,ed: Integer);
begin
  sc := JedMain.Cur_SC;
  sf := JedMain.Cur_SF;
  ed := JedMain.Cur_ED;
end;

procedure TSedCOM.SetCurrentEdge(sc,sf,ed: Integer);
begin
  JedMain.SetCurED(sc,sf,ed);
end;

procedure TSedCOM.GetCurrentFrame(var th,fr: Integer);
begin
  th := JedMain.Cur_TH;
  fr := JedMain.Cur_FR;
end;

procedure TSedCOM.SetCurrentFrame(th,fr: Integer);
begin
  JedMain.SetCurFR(th,fr);
end;

procedure TSedCOM.NewLevel(kind: Integer);
begin
  case TProjectType(kind) of
    TProjectType.JKDF2: JedMain.New1.Click;
    TProjectType.MOTS: JedMain.NewMotsProject1.Click;
  else
    JedMain.NewIJIMProject.Click;
  end;
end;

procedure TSedCOM.LoadLevel(const filename: PChar);
begin
  JedMain.OpenProject(filename, op_open);
end;

{Different level editing functions}

procedure TSedCOM.FindBoundingBox(sec: Integer; var box:TSedBox);
begin
  lev_utils.FindBBox(Level.Sectors[sec],TBox(box));
end;

procedure TSedCOM.FindBoundingSphere(sec: Integer; var center: TSedVector3; radius: Double);
begin
  lev_utils.FindBSphere(level.Sectors[sec], center.x, center.y, center.z, radius);
end;

function TSedCOM.FindCollideBox(sec: Integer; const bbox:TSedBox; const center: TSedVector3; var cbox:TSedBox): Boolean;
begin
  Result := lev_utils.FindCollideBox(
    level.sectors[sec],TBox(bbox),center.x, center.y,center.z,Tbox(cbox));
end;

procedure TSedCOM.FindSurfaceCenter(sc,sf: Integer; var center:TSedVector3);
begin
  lev_utils.CalcSurfCenter(level.sectors[sc].surfaces[sf], center.x, center.y, center.z);
end;

procedure TSedCOM.RotateVector(var vec:TSedVector3; const pyr:TSedVector3);
begin
  lev_utils.RotateVector(TVector(vec), pyr.pitch, pyr.yaw, pyr.roll);
end;

procedure TSedCOM.RotatePoint(const point, pivot: TSedVector3; angle: Double; var point2: TSedVector3);
begin
  lev_utils.RotatePoint(point.x,point.y,point.z, pivot.x,pivot.y,pivot.z, angle, point2.x,point2.y,point2.z);
end;

procedure TSedCOM.GetPYR(const x,y,z:TSedVector3; var pyr:TSedVector3);
begin
  lev_utils.GetJKPYR(Tvector(x),Tvector(y),Tvector(z), pyr.pitch, pyr.yaw, pyr.roll);
end;

function TSedCOM.IsSurfaceConvex(sc,sf: Integer): Boolean;
begin
  Result := lev_utils.IsSurfConvex(Level.Sectors[sc].Surfaces[sf]);
end;

function TSedCOM.IsSurfacePlanar(sc,sf: Integer): Boolean;
begin
  Result := lev_utils.IsSurfPlanar(Level.Sectors[sc].Surfaces[sf]);
end;

function TSedCOM.IsSectorConvex(sec: Integer): Boolean;
begin
  Result := lev_utils.IsSectorConvex(level.sectors[sec]);
end;

function TSedCOM.IsInSector(sec: Integer; const point: TSedVector3): Boolean;
begin
  Result := lev_utils.IsInSector(level.sectors[sec], point.x, point.y, point.z);
end;

function TSedCOM.DoSectorsOverlap(sec1,sec2: Integer): Boolean;
begin
  Result := lev_utils.DoSectorsOverlap(level.sectors[sec1], level.sectors[sec2]);
end;

function TSedCOM.IsPointOnSurface(sec, surf: Integer; const point: TSedVector3): Boolean;
begin
  Result := lev_utils.IsPointOnSurface(level.sectors[sec].surfaces[surf], point.x, point.y, point.z);
end;

function TSedCOM.FindSectorForThing(th: Integer): Integer;
begin
  Result := lev_utils.FindSectorForThing(level.things[th]);
end;

function TSedCOM.FindSectorForXYZ(x, y, z: Double): Integer;
begin
  Result := lev_utils.FindSectorForXYZ(level, x,y,z);
end;

function TSedCOM.ExtrudeSurface(sc,sf: Integer; by: Double): Integer;
begin
  lev_utils.ExtrudeSurface(level.sectors[sc].surfaces[sf],by);
  Result := level.Sectors.count - 1;
end;

function TSedCOM.CleaveSurface(sec, surf: Integer; const cnormal: TSedVector3; cp: TSedVector3): Integer;
begin
  if lev_utils.CleaveSurface(level.sectors[sec].surfaces[surf], Tvector(Cnormal), cp.x, cp.y, cp.z) then
    Result := level.sectors[sec].surfaces.count - 1
  else Result := -1;
end;

function TSedCOM.CleaveSector(sec: Integer; const cnormal: TSedVector3; const cp: TSedVector3): Integer;
begin
  if lev_utils.CleaveSector(level.sectors[sec], Tvector(Cnormal), cp.x, cp.y, cp.z) then
    Result := level.sectors.count - 1
  else Result := -1;
end;

function TSedCOM.CleaveEdge(sec, surf, ed: Integer; const cnormal: TSedVector3; cp: TSedVector3): Boolean;
begin
 Result := lev_utils.CleaveEdge(level.sectors[sec].surfaces[surf], ed, Tvector(Cnormal), cp.x, cp.y, cp.z);
end;

function TSedCOM.JoinSurfaces(sc1,sf1, sc2,sf2: Integer): Boolean;
begin
  Result := lev_utils.ConnectSurfaces(level.sectors[sc1].surfaces[sf1],
                                      level.sectors[sc2].surfaces[sf2]);
end;

function TSedCOM.PlanarizeSurface(sc,sf: Integer): Boolean;
begin
  Result := lev_utils.FlattenSurface(level.sectors[sc].surfaces[sf]);
end;

function TSedCOM.MergeSurfaces(sc,sf1,sf2: Integer): Integer;
var surf:TJKSurface;
begin
  surf := lev_utils.MergeSurfaces(level.sectors[sc].surfaces[sf1], level.sectors[sc].surfaces[sf2]);
  if surf = nil then Result := -1
  else Result := surf.num;
end;

function TSedCOM.MergeSectors(sec1,sec2: Integer): Integer;
var sec:TJKSector;
begin
  sec := lev_utils.MergeSectors(level.sectors[sec1], level.sectors[sec2]);
  if sec=nil then Result := -1
  else Result := sec.num;
end;

procedure TSedCOM.CalculateDefaultUVNormals(sc,sf: Integer; orgvx: Integer; var un,vn:TSedVector3);
begin
  lev_utils.CalcDefaultUVNormals(level.sectors[sc].surfaces[sf], orgvx, Tvector(un), Tvector(vn));
end;

procedure TSedCOM.CalculateUVNormals(sc,sf: Integer; var un,vn:TSedVector3);
begin
  lev_utils.CalcUVNormals(level.sectors[sc].surfaces[sf], Tvector(un), Tvector(vn));
end;

procedure TSedCOM.ArrangeTexture(sc,sf: Integer; orgvx: Integer; const un,vn:TSedVector3);
begin
  lev_utils.ArrangeTexture(level.sectors[sc].surfaces[sf], orgvx, Tvector(un), Tvector(vn));
end;

procedure TSedCOM.ArrangeTextureBy(sec, surf: Integer; const un, vn: TSedVector3; const refp: TSedVector3; refu, refv: Double);
begin
  lev_utils.ArrangeTextureBy(
    level.sectors[sec].surfaces[surf], Tvector(un), Tvector(vn),
    refp.x, refp.y, refp.z, refu, refv);
end;

function TSedCOM.IsTextureFlipped(sc,sf: Integer): Boolean;
begin
  Result := lev_utils.IsTXFlipped(level.sectors[sc].surfaces[sf]);
end;

procedure TSedCOM.RemoveSurfaceReferences(sc,sf: Integer);
begin
  lev_utils.RemoveSurfRefs(level, level.sectors[sc].surfaces[sf]);
end;

procedure TSedCOM.RemoveSectorReferences(sec: Integer; surfs: Boolean);
begin
  if surfs then lev_utils.RemoveSecRefs(level, level.sectors[sec], rs_surfs)
  else lev_utils.RemoveSecRefs(level, level.sectors[sec], 0);
end;

function TSedCOM.StitchSurfaces(sc1,sf1, sc2,sf2: Integer): Boolean;
begin
  Result := lev_utils.StitchSurfaces(level.sectors[sc1].surfaces[sf1],
                                     level.sectors[sc2].surfaces[sf2]);
end;

function TSedCOM.FindCommonEdges(sc1,sf1, sc2,sf2: Integer; var v11,v12, v21,v22: Integer): Boolean;
begin
  Result := lev_utils.FindCommonEdges(level.sectors[sc1].surfaces[sf1],
                                      level.sectors[sc2].surfaces[sf2],v11,v12, v21,v22);
end;

function TSedCOM.DoSurfacesOverlap(sc1,sf1, sc2,sf2: Integer): Boolean;
begin
  Result := lev_utils.Do_Surf_Overlap(level.sectors[sc1].surfaces[sf1],
                                      level.sectors[sc2].surfaces[sf2]);
end;

function TSedCOM.MakeAdjoin(sc,sf: Integer): Boolean;
begin
  Result := lev_utils.MakeAdjoin(level.sectors[sc].surfaces[sf]);
end;

function TSedCOM.MakeAdjoinFromSectorUp(sc,sf: Integer; firstsc: Integer): Boolean;
begin
  Result := lev_utils.MakeAdjoinSCUp(level.sectors[sc].surfaces[sf], firstsc);
end;

function TSedCOM.UnAdjoin(sc,sf: Integer): Boolean;
begin
  Result := lev_utils.UnAdjoin(level.sectors[sc].surfaces[sf]);
end;

function TSedCOM.CreateCubicSector(const pos: TSedVector3; const pnormal, edge: TSedVector3): Integer;
begin
  lev_utils.CreateCube(level, pos.x, pos.y, pos.z, Tvector(pnormal), TVector(edge));
  Result := Level.sectors.count - 1;
end;

procedure TSedCOM.StartUndo(const name: PChar);
begin
  StartUndoRec(name);
end;

procedure TSedCOM.SaveUndoForThing(n: Integer;change: Integer);
begin
  case change of
    0: SaveThingUndo(level.things[n], ch_changed);
    1: SaveThingUndo(level.things[n], ch_added);
    2: SaveThingUndo(level.things[n], ch_deleted);
  end;
end;

procedure TSedCOM.SaveUndoForLight(n: Integer;change: Integer);
begin
  case change of
    0: SaveLightUndo(level.lights[n], ch_changed);
    1: SaveLightUndo(level.lights[n], ch_added);
    2: SaveLightUndo(level.lights[n], ch_deleted);
  end;
end;

procedure TSedCOM.SaveUndoForSector(n: Integer;change: Integer;whatpart: Integer);
var how: Integer;
begin
  how := sc_both;
  if whatPart = sed_plugins.sc_values   then how := sc_val;
  if whatpart = sed_plugins.sc_geometry then how := sc_geo;
  if whatpart = sed_plugins.sc_all      then how := sc_both;

  case change of
    sed_plugins.uc_changed: SaveSecUndo(level.sectors[n], ch_changed, how);
    sed_plugins.uc_added: SaveSecUndo(level.sectors[n], ch_added, how);
    sed_plugins.uc_deleted: SaveSecUndo(level.sectors[n], ch_deleted, how);
  end;
end;

procedure TSedCOM.ClearUndoBuffer;
begin
  u_undo.ClearUndoBuffer;
end;

procedure TSedCOM.ApplyUndo;
begin
  u_undo.ApplyUndo;
end;

{0.92}

function TSedCOM.GetApplicationHandle: Integer;
begin
  Result := Application.Handle;
end;

function TSedCOM.JoinSectors(sec1,sec2: Integer): Boolean;
begin
  Result := lev_utils.ConnectSectors(level.sectors[sec1], level.sectors[sec2]);
end;

function TSedCOM.GetNumMultiselected(what: Integer): Integer;
begin
  with JedMain do
    case what of
      sed_plugins.mm_sc: Result := scsel.count;
      sed_plugins.mm_sf: Result := sfsel.count;
      sed_plugins.mm_ed: Result := edsel.count;
      sed_plugins.mm_vx: Result := vxsel.count;
      sed_plugins.mm_th: Result := thsel.count;
      sed_plugins.mm_fr: Result := frsel.count;
      sed_plugins.mm_lt: Result := ltsel.count;
    end;
end;

procedure TSedCOM.ClearMultiselection(what: Integer);
begin
  with JedMain do
  case what of
    sed_plugins.mm_sc: scsel.clear;
    sed_plugins.mm_sf: sfsel.clear;
    sed_plugins.mm_ed: edsel.clear;
    sed_plugins.mm_vx: vxsel.clear;
    sed_plugins.mm_th: thsel.clear;
    sed_plugins.mm_fr: frsel.clear;
    sed_plugins.mm_lt: ltsel.clear;
  end;
  JedMain.Invalidate;
end;

procedure TSedCOM.RemoveFromMultiselection(what,n: Integer);
begin
  with JedMain do
  case what of
    sed_plugins.mm_sc: scsel.DeleteN(n);
    sed_plugins.mm_sf: sfsel.DeleteN(n);
    sed_plugins.mm_ed: edsel.DeleteN(n);
    sed_plugins.mm_vx: vxsel.DeleteN(n);
    sed_plugins.mm_th: thsel.DeleteN(n);
    sed_plugins.mm_fr: frsel.DeleteN(n);
    sed_plugins.mm_lt: ltsel.DeleteN(n);
  end;
  JedMain.Invalidate;
end;

function TSedCOM.GetSelectedSector(n: Integer): Integer;
begin
  Result := JedMain.scsel.GetSC(n);
end;

function TSedCOM.GetSelectedThing(n: Integer): Integer;
begin
  Result := JedMain.thsel.GetTH(n);
end;

function TSedCOM.GetSelectedLight(n: Integer): Integer;
begin
  Result := JedMain.ltsel.GetLT(n);
end;

procedure TSedCOM.GetSelectedSurface(n: Integer; var sc,sf: Integer);
begin
  JedMain.sfsel.GetSCSF(n, sc, sf);
end;

procedure TSedCOM.GetSelectedEdge(n: Integer; var sc,sf,ed: Integer);
begin
  JedMain.edsel.GetSCSFED(n,sc,sf,ed);
end;

procedure TSedCOM.GetSelectedVertex(n: Integer; var sc,vx: Integer);
begin
  JedMain.vxsel.GetSCVX(n,sc,vx);
end;

procedure TSedCOM.GetSelectedFrame(n: Integer; var th,fr: Integer);
begin
  JedMain.frsel.GetTHFR(n,th,fr);
end;

function TSedCOM.SelectSector(sc: Integer): Integer;
begin
  Result := Nmask and JedMain.scsel.AddSC(sc);
end;

function TSedCOM.SelectSurface(sc,sf: Integer): Integer;
begin
  Result := Nmask and JedMain.sfsel.AddSF(sc,sf);
end;

function TSedCOM.SelectEdge(sc,sf,ed: Integer): Integer;
begin
  Result := Nmask and JedMain.edsel.AddED(sc,sf,ed);
end;

function TSedCOM.SelectVertex(sc,vx: Integer): Integer;
begin
  Result := Nmask and JedMain.vxsel.AddVX(sc,vx);
end;

function TSedCOM.SelectThing(th: Integer): Integer;
begin
  Result := Nmask and JedMain.thsel.AddTH(th);
end;

function TSedCOM.SelectFrame(th,fr: Integer): Integer;
begin
  Result := Nmask and JedMain.frsel.AddFR(th,fr);
end;

function TSedCOM.SelectLight(lt: Integer): Integer;
begin
  Result := Nmask and JedMain.ltsel.AddLT(lt);
end;

function TSedCOM.FindSelectedSector(sc: Integer): Integer;
begin
  Result := JedMain.scsel.FindSC(sc);
end;

function TSedCOM.FindSelectedSurface(sc,sf: Integer): Integer;
begin
  Result := JedMain.sfsel.FindSF(sc,sf);
end;

function TSedCOM.FindSelectedEdge(sc,sf,ed: Integer): Integer;
begin
  Result := JedMain.edsel.FindED(sc,sf,ed);
end;

function TSedCOM.FindSelectedVertex(sc,vx: Integer): Integer;
begin
  Result := JedMain.vxsel.FindVX(sc,vx);
end;

function TSedCOM.FindSelectedThing(th: Integer): Integer;
begin
  Result := JedMain.thsel.FindTH(th);
end;

function TSedCOM.FindSelectedFrame(th,fr: Integer): Integer;
begin
  Result := JedMain.frsel.FindFR(th,fr);
end;

function TSedCOM.FindSelectedLight(lt: Integer): Integer;
begin
  Result := JedMain.ltsel.FindLT(lt);
end;

procedure TSedCOM.SaveAsSed(const filename: PChar);
begin
  JedMain.SaveToFile(filename, 'jed');
end;

procedure TSedCOM.Save(const filename: PChar);
begin
  JedMain.SaveJKLto(filename);
end;

procedure TSedCOM.UpdateMap;
begin
  JedMain.Invalidate;
end;

procedure TSedCOM.SetPickerCmp(const cmp: PChar);
begin
  ResPicker.SetCMP(cmp);
end;

function TSedCOM.PickResource(what: Integer; const cur: PChar): PChar;
const resstr: AnsiString = '';
begin
SaveCurApp;
case what of
 pr_thing: resstr := ResPicker.PickThing(cur);
 pr_template: resstr := ResPicker.PickTemplate(cur);
 pr_cmp: resstr := ResPicker.PickCMP(cur);
 pr_secsound: resstr := ResPicker.PickSecSound(cur);
 pr_mat: resstr := ResPicker.PickMAT(cur);
 pr_cog: resstr := ResPicker.PickCOG(cur);
 pr_layer: resstr := ResPicker.PickLayer(cur);
 pr_3do: resstr := ResPicker.Pick3DO(cur);
 pr_ai: resstr := ResPicker.PickAI(cur);
 pr_key: resstr := ResPicker.PickKEY(cur);
 pr_snd: resstr := ResPicker.PickSND(cur);
 pr_pup: resstr := ResPicker.PickPUP(cur);
 pr_spr: resstr := ResPicker.PickSPR(cur);
 pr_par: resstr := ResPicker.PickPAR(cur);
 pr_per: resstr := ResPicker.PickPER(cur);
 pr_jklsmksan: resstr := ResPicker.PickEpSeq(cur);
else resstr := cur;
end;
 Result :=  PChar(resstr);
 RestoreCurApp;
end;

function TSedCOM.EditFlags(what: Integer;flags:LongInt):LongInt;
begin
  SaveCurApp;
  case what of
    ef_sector:    Result := FlagEdit.EditSectorFlags(flags);
    ef_surface:   Result := FlagEdit.EditSurfaceFlags(flags);
    ef_adjoin:    Result := FlagEdit.EditAdjoinFlags(flags);
    ef_thing:     Result := FlagEdit.EditThingFlags(flags);
    ef_face:      Result := FlagEdit.EditFaceFlags(flags);
    ef_light:     Result := FlagEdit.EditLightFlags(flags);
    ef_geomode:   Result := SCFieldPicker.PickGeo(flags);
    ef_lightmode: Result := SCFieldPicker.PickLightMode(flags);
    ef_texmode:   Result := SCFieldPicker.PickTex(flags);
  end;
  RestoreCurApp;
end;

procedure TSedCOM.ReloadTemplates;
begin
 JedMain.LoadTemplates;
end;

procedure TSedCOM.PanMessage(mtype: Integer; const msg: PChar);
begin
  case mtype of
    sed_plugins.msg_info: misc_utils.PanMessage(mt_info, msg);
    sed_plugins.msg_warning: misc_utils.PanMessage(mt_warning, msg);
    sed_plugins.msg_error: misc_utils.PanMessage(mt_error, msg);
  end;
end;

procedure TSedCOM.SendKey(shift: Integer;key: Integer);
var st:TShiftState;
    akey:word;
begin
  st := [];
  if (BitTest(shift, sed_plugins.sk_Ctrl) <> 0) then st := st + [ssCtrl];
  if (BitTest(shift, sed_plugins.sk_Shift) <> 0) then st := st + [ssShift];
  if (BitTest(shift, sed_plugins.sk_Alt)<> 0) then st := st + [ssAlt];
  akey := key;
  JedMain.FormKeyDown(JedMain, akey, st);
end;

function TSedCOM.ExecuteMenu(const itemref: PChar): Boolean;
var nmi,mi:TMenuItem;
    cname: string;
    p,cpos: Integer;

  function GetNextItem(var s: string; pos : Integer): Integer;
  begin
   Result := pos;
   while result<length(ItemRef) do
    if ItemRef[result]<>'\' then inc(result) else break;

   if result = length(itemRef) then  s:=Copy(itemref,pos,Length(ItemRef))
   else s:=Copy(itemref,pos,result-pos);

   inc(result);
  end;

var i: Integer; s: string;
begin
  Result := false;
  mi := nil;
  cpos := GetNextItem(cname,1);
  With Jedmain.Menu do
  for i:=0 to 6 do {Number of top menu items!}
  begin
  s:=Items[i].Caption;
  While pos('&',s)<>0 do Delete(s,pos('&',s),1);

  if CompareText(s,cname)=0 then
  begin
   mi:=Items[i];
   break;
  end;
  end;

  if mi=nil then exit;

  while cpos<length(itemRef) do
  begin
  cpos := GetNextItem(cname,cpos);
  nmi := nil;

  for i:=0 to mi.Count-1 do
  begin
    s:=mi.Items[i].Caption;
    if Pos(#9,s)<>0 then SetLength(s,Pos(#9,s)-1);

    while pos('&',s)<>0 do Delete(s,pos('&',s),1);

    if CompareText(s,cname)=0 then
    begin
      nmi:=mi.Items[i];
      break;
    end;
  end;

  if nmi=nil then exit;
  mi:=nmi;

  end;

  mi.Click;
  Result := true;
end;

function TSedCOM.GetSedSetting(const name: PChar): variant;
begin
  Result := GetSetting(name); // TODO: verify, might breake plugin due to returned string
end;

function TSedCOM.IsLayerVisible(n: Integer): Boolean;
begin
  Result := ToolBar.IsLayerVisible(n);
end;

procedure TSedCOM.CheckConsistencyErrors;
begin
  Consistency.Check;
  Consistency.Hide;
end;

procedure TSedCOM.CheckResources;
begin
  Consistency.CheckResources;
  Consistency.Hide;
end;

function TSedCOM.GetNumConsistencyErrors: Integer;
begin
  Result := Consistency.NErrors;
end;

function TSedCOM.GetConsistencyErrorString(n: Integer): PChar;

begin
  Result :=  PChar(Consistency.ErrorText(n));
end;

function TSedCOM.GetConsistencyErrorType(n: Integer): Integer;
var obj:TConsistencyError;
begin
  Result := -1;
  obj := Consistency.ErrorObject(n);
  if obj = nil then exit;
  Result := Integer(obj.etype);
end;

function TSedCOM.GetConsistencyErrorItemType(n: Integer): Integer;
var obj:TConsistencyError;
begin
  Result := -1;
  obj := Consistency.ErrorObject(n);
  if obj = nil then exit;
  Result := Integer(obj.itype);
end;

function TSedCOM.GetConsistencyErrorSector(n: Integer): Integer;
var obj:TConsistencyError;
begin
  Result := -1;
  obj := Consistency.ErrorObject(n);
  if obj.itype = itSector then Result := TJKSector(obj.item).num
  else if obj.itype=itSurface then Result := TJKSurface(obj.item).sector.num;
end;

function TSedCOM.GetConsistencyErrorSurface(n: Integer): Integer;
var obj:TConsistencyError;
begin
   Result := -1;
   obj := Consistency.ErrorObject(n);
   if obj.itype=itSurface then Result := TJKSurface(obj.item).num;
end;

function TSedCOM.GetConsistencyErrorThing(n: Integer): Integer;
var obj:TConsistencyError;
begin
  Result := -1;
  obj:=Consistency.ErrorObject(n);
  if obj.itype=itThing then Result := TJKThing(obj.item).num;
end;

function TSedCOM.GetConsistencyErrorCOG(n: Integer): Integer;
var obj:TConsistencyError;
begin
  Result := -1;
  if n >= Consistency.NErrors then exit;

  obj:=Consistency.ErrorObject(n);
  if obj.itype=itCog then Result := J_Level.Level.COGS.IndexOf(obj.item);
end;

function TSedCOM.IsPreviewActive: Boolean;
begin
  Result := Preview3D.IsActive;
end;

function TSedCOM.GetSedString(what: Integer): PChar;
  begin
    case what of
      SS_ProjectDir:  Result := PChar(ProjectDir);
      SS_SEDDir:      Result := PChar(BaseDir);
      SS_GameDir:     Result := PChar(GameDir);
      SS_LevelFile:   Result := PChar(JedMain.LevelFile);
      SS_ConfigFile:  Result := PChar(GetConfigFilePath());
      SS_LECLogoJK:   Result := PChar(LECLogoJK);
      SS_Recent1:     Result := PChar(Recent1);
      SS_Recent2:     Result := PChar(Recent2);
      SS_Recent3:     Result := PChar(Recent3);
      SS_Recent4:     Result := PChar(Recent4);
      SS_Res1gob:     Result := PChar(Res1_Gob);
      SS_Res2gob:     Result := PChar(Res2_Gob);
      SS_SPGOB:       Result := PChar(sp_Gob);
      SS_MP1GOB:      Result := PChar(mp1_Gob);
      SS_MP2GOB:      Result := PChar(mp2_Gob);
      SS_MP3GOB:      Result := PChar(mp3_Gob);
      SS_LECLogoIJIM: Result := PChar(LECLogoIJIM);
  else
    begin
      tmpastr[0] := #0;
       Result := @tmpastr[0];
    end;
end;

end;

function TSedCOM.GetProjectType: Integer;
begin
  Result := Integer(GlobalVars.CurrentProject);
end;

procedure TSedCOM.SetProjectType(kind: Integer);
begin
  SetProjectType(kind);
end;

function TSedCOM.GetSedWindow(whichone: Integer): Integer;
begin
  case whichone of
    sw_Main: Result := JedMain.Handle;
    sw_ConsChecker: Result := Consistency.Handle;
    sw_ItemEdit: Result := ItemEdit.Handle;
    sw_PlacedCogs: Result := CogForm.Handle;
    sw_MsgWindow: Result := MsgForm.Handle;
    sw_3DPreview: Result := Preview3D.Handle;
    sw_ToolWindow: Result := ToolForm.Handle;
  else Result := 0;
  end;
end;

function TSedCOM.FileExtractExt(const filename: PChar): PChar;
begin
  Result := PChar(ExtractExt(filename));
end;

function TSedCOM.FileExtractPath(const filename: PChar): PChar;
begin
  Result := PChar(ExtractPath(filename));
end;

function TSedCOM.FileExtractName(const filename: PChar): PChar;
begin
  Result := PChar(ExtractName(filename));
end;

function TSedCOM.FindProjectDirFile(const filename: PChar): PChar;
begin
  Result := PChar(FindProjDirFile(filename));
end;

function TSedCOM.IsFileContainer(const filename: PChar): Boolean;
begin
 Result := IsContainer(filename);
end;

function TSedCOM.IsFileInContainer(const filename: PChar): Boolean;
begin
 Result := IsInContainer(filename);
end;

function TSedCOM.FileOpenDialog(const filename: PChar; filter: PChar): PChar;
begin
  SaveCurApp;
  GetFileOpen.FileName := filename;
  GetFileOpen.Filter := filter;
  if GetFileOpen.Execute then
    begin
        Result := PChar(GetFileOpen.FileName)
    end
  else Result := nil;
  RestoreCurApp;
end;


function TSedCOM.OpenFile(const filename: PChar): Integer;
begin
try
 Result := files.NewHandle(OpenFileRead(filename,0));
except
 on Exception do Result := -1;
end;
end;

function TSedCOM.OpenGameFile(const filename: PChar): Integer;
begin
  try
    Result := files.NewHandle(FileOperations.OpenGameFile(filename));
  except
    on Exception do Result := -1;
  end;
end;

function TSedCOM.ReadFile(handle: Integer; buf: Pointer; size: Longint): Integer;
begin
 Result := TFile(files.GetItemNoNIL(handle)).Fread(buf^,size);
end;

procedure TSedCOM. SetFilePos(handle: Integer;pos: Longint);
begin
 TFile(files.GetItemNoNIL(handle)).FSeek(pos);
end;

function TSedCOM.GetFilePos(handle: Integer): Longint;
begin
 Result := TFile(files.GetItemNoNIL(handle)).FPos;
end;

function TSedCOM.GetFileSize(handle: Integer): Longint;
begin
 Result := TFile(files.GetItemNoNIL(handle)).FSize;
end;

function TSedCOM.GetFileName(handle: Integer): PChar;
begin
 tmpstr := TFile(files.GetItemNoNIL(handle)).GetFullName;  // GetFullName returns tmp string
 Result := PChar(tmpstr);
end;

procedure TSedCOM.CloseFile(handle: Integer);
begin
 files.FreeHandle(handle);
end;

function TSedCOM.OpenTextFile(const filename: PChar): Integer;
begin
  try
   Result := tfiles.NewHandle(TTextFile.CreateRead(OpenFileRead(filename, 0)));
  except
   on Exception do Result := -1;
  end;
end;

function TSedCOM.OpenGameTextFile(const filename: PChar): Integer;
begin
  try
    Result := tfiles.NewHandle(TTextFile.CreateRead(FileOperations.OpenGameFile(filename)));
  except
   on Exception do Result := -1;
  end;
end;

function TSedCOM.GetTextFileName(handle: Integer): PChar;
begin
  tmpstr := TTextFile(tfiles.GetItemNoNIL(handle)).GetFullName;  // GetFullName returns tmp string
  Result := PChar(tmpstr);
end;

function TSedCOM.ReadTextFileString(handle: Integer): PChar;
  var tmpstr: string;
begin
 TTextFile(tfiles.GetItemNoNIL(handle)).Readln(tmpstr);
 Result := PChar(tmpstr);
end;

function TSedCOM.TextFileEOF(handle: Integer): Boolean;
begin
  Result := TTextFile(tfiles.GetItemNoNIL(handle)).eof;
end;

function TSedCOM.TextFileCurrentLine(handle: Integer): Integer;
begin
 Result := TTextFile(tfiles.GetItemNoNIL(handle)).CurrentLine;
end;

procedure TSedCOM.CloseTextFile(handle: Integer);
begin
 tfiles.FreeHandle(handle);
end;

function TSedCOM.GobOpen(const filename: PChar): Integer;
begin
try
 Result := conts.NewHandle(OpenContainer(filename));
except
 on Exception do Result := -1;
end;
end;

function TSedCOM.GobNumFiles(handle: Integer): Integer;
begin
 Result := TContainerFile(conts.GetItemNoNIL(handle)).Files.Count;
end;

function TSedCOM.GobGetFilename(handle: Integer;n: Integer): PChar;
begin
 Result := PChar(TContainerFile(conts.GetItemNoNIL(handle)).Files[n]);
end;

function TSedCOM.GobGetFullFilename(handle: Integer; n: Integer): PChar;
var cf:TContainerFile;
begin
 cf:=TContainerFile(conts.GetItemNoNIL(handle));
 tmpstr := cf.Name + '>' + cf.Files[n];
 Result := PChar(tmpstr)
end;

function TSedCOM.GobGetFileOffset(handle: Integer;n: Integer): longint;
begin
 Result := TFileInfo(TContainerFile(conts.GetItemNoNIL(handle)).Files.Objects[n]).offs;
end;

function TSedCOM.GobGetFileSize(handle: Integer;n: Integer): longint;
begin
 Result := TFileInfo(TContainerFile(conts.GetItemNoNIL(handle)).Files.Objects[n]).size;
end;

procedure TSedCOM.GobClose(handle: Integer);
begin
 conts.FreeHandle(handle);
end;

function TSedCOM.CreateWFRenderer(wnd: Integer;whichone: Integer): ISedWFRenderer;
begin
 try
  Result := TCOMWFRenderer.Create(wnd, whichone);
 except
  on Exception do Result := nil;
 end;
end;


{---------------------- WF Renderer -------------------------------}

constructor TCOMWFRenderer.Create(wnd: Integer;which: Integer);
begin
  case which of
    cr_Default: if WireFrameAPI=WF_OPENGL then which:=cr_OpenGL else which:=cr_Software;
  end;

  case which of
    WF_Software: Rend := TSFTRenderer.Create(wnd);
    WF_OpenGL: Rend := TOGLRenderer.Create(wnd);
  else Rend := TSFTRenderer.Create(wnd);
  end;
end;

function TCOMWFRenderer.QueryInterface(const IID: TGUID; out Obj): HResult;
begin
  Result := CLASS_E_CLASSNOTAVAILABLE;
end;

function TCOMWFRenderer.AddRef: Integer;
begin
  Result := 1;
end;

function TCOMWFRenderer.Release: Integer;
begin
  Result := 0;
end;

{Renderer attributes}
function TCOMWFRenderer.GetRendererDouble(what: Integer): Double;
begin
  case what of
    rd_CamX: Result := Rend.CamX;
    rd_CamY: Result := Rend.CamY;
    rd_CamZ: Result := Rend.CamZ;
    rd_Scale: Result := Rend.scale;
    rd_GridX: Result := Rend.GridX;
    rd_GridY: Result := Rend.GridY;
    rd_GridZ: Result := Rend.GridZ;
    rd_GridLine: Result := Rend.GridLine;
    rd_GridDot: Result := Rend.GridDot;
    rd_GridStep: Result := Rend.GridStep;
  else Result := 0;
  end;
end;

procedure TCOMWFRenderer.SetRendererDouble(what: Integer;val: Double);
begin
  case what of
    rd_CamX: Rend.CamX := val;
    rd_CamY: Rend.CamY := val;
    rd_CamZ: Rend.CamZ := val;
    rd_Scale: Rend.scale := val;
    rd_GridX: Rend.GridX := val;
    rd_GridY: Rend.GridY := val;
    rd_GridZ: Rend.GridZ := val;
    rd_GridLine: Rend.GridLine := val;
    rd_GridDot: Rend.GridDot := val;
    rd_GridStep: Rend.GridStep := val;
  end;
end;

procedure TCOMWFRenderer.GetRendererVector(what: Integer; var x,y,z: Double);
begin
 with Rend do
    case what of
      rv_CamPos: begin x:=CamX; y:=CamY; z:=CamZ; end;
      rv_GridPos: begin x:=GridX; y:=GridY; z:=GridZ; end;
      rv_CamXAxis: xv.GetCoords(x,y,z);
      rv_CamYAxis: yv.GetCoords(x,y,z);
      rv_CamZAxis: zv.GetCoords(x,y,z);
      rv_GridXAxis: gxnormal.GetCoords(x,y,z);
      rv_GridYAxis: gynormal.GetCoords(x,y,z);
      rv_GridZAxis: gnormal.GetCoords(x,y,z);
    else begin x:=0; y:=0; z:=0; end;
  end;
end;

procedure TCOMWFRenderer.SetRendererVector(what: Integer;x,y,z: Double);
begin
  With Rend do
    case what of
      rv_CamPos: begin CamX:=x; CamY:=y; CamZ:=z; end;
      rv_GridPos: begin GridX:=x; GridY:=y; GridZ:=z; end;
      rv_CamXAxis: SetX(x,y,z);
      rv_CamYAxis: SetY(x,y,z);
      rv_CamZAxis: SetZ(x,y,z);
      rv_GridXAxis: SetGridNormal(x,y,z);
      rv_GridYAxis: SetGridXNormal(x,y,z);
      rv_GridZAxis: SetGridYNormal(x,y,z);
  end;
end;

function TCOMWFRenderer.NSelected: Integer;
begin
  Result := Rend.Selected.Count;
end;

function TCOMWFRenderer.GetNSelected(n: Integer): Integer;
begin
  Result := Rend.Selected[n];
end;

procedure TCOMWFRenderer.SetViewPort(x,y,w,h: Integer);
begin
  Rend.SetViewPort(x,y,w,h);
end;

procedure TCOMWFRenderer.SetColor(what,r,g,b:byte);
begin
  Rend.SetViewPort(what,r,g,b);
end;

procedure TCOMWFRenderer.SetPointSize(size: Double);
begin
  Rend.SetPointSize(size);
end;

procedure TCOMWFRenderer.BeginScene;
begin
  Rend.BeginScene;
end;

procedure TCOMWFRenderer.EndScene;
begin
  Rend.EndScene;
end;

procedure TCOMWFRenderer.SetCulling(how: Integer);
begin
 Rend.SetCulling(how);
end;

procedure TCOMWFRenderer.DrawSector(sc: Integer);
begin
  if not CheckSectorIndex(Level, sc) then
    begin
      misc_utils.PanMessageFmt(mt_error, 'TCOMWFRenderer.DrawThing: Invalid sector index %d,%d', [sc]);
      exit;
    end;
  Rend.DrawPolygons(Level.Sectors[sc].Surfaces);
end;

procedure TCOMWFRenderer.DrawSurface(sc,sf: Integer);
begin
  if not CheckSurfaceIndex(Level, sc, sf) then
    begin
      misc_utils.PanMessageFmt(mt_error, 'TCOMWFRenderer.DrawThing: Invalid sector or surface indices: %d,%d', [sc, sf]);
      exit;
    end;
  Rend.DrawPolygon(Level.Sectors[sc].Surfaces[sf]);
end;

procedure TCOMWFRenderer.DrawThing(th: Integer);
begin
  if not CheckThingIndex(Level, th) then
  begin
    misc_utils.PanMessageFmt(mt_error, 'TCOMWFRenderer.DrawThing: Invalid index %d', [th]);
    exit;
  end;

  var thng := Level.Things[th];
  if thng.a3DO <> nil then
  begin
    for var i := 0 to thng.a3DO.Meshes.Count - 1 do
        with thng.a3DO.meshes[i] do
          Rend.DrawPolygonsAt(faces, thng.x, thng.y, thng.z, thng.pch, thng.yaw, thng.rol);
  end;
end;

procedure TCOMWFRenderer.DrawLine(x1,y1,z1, x2,y2,z2: Double);
var v1,v2:TVertex;
begin
  v1:=Tvertex.Create;
  v1.x:=x1; v1.y:=y1; v1.z:=z1;

  v2:=Tvertex.Create;
  v2.x:=x2; v2.y:=y2; v2.z:=z2;
  Rend.DrawLine(v1,v2);

  v1.free;
  v2.free;
end;

procedure TCOMWFRenderer.DrawVertex(x, y, z: Double);
begin
  Rend.DrawVertex(x,y,z);
end;

procedure TCOMWFRenderer.DrawGrid;
begin
  Rend.DrawGrid;
end;

procedure TCOMWFRenderer.BeginPick(x,y: Integer);
begin
 Rend.BeginPick(x,y);
end;

procedure TCOMWFRenderer.EndPick;
begin
  Rend.EndPick;
end;

procedure TCOMWFRenderer.PickSector(sc: Integer;id: Integer);
begin
  Rend.PickPolygons(Level.Sectors[sc].Surfaces,id);
end;

procedure TCOMWFRenderer.PickSurface(sc,sf: Integer;id: Integer);
begin
  Rend.PickPolygon(Level.Sectors[sc].Surfaces[sf],id);
end;

procedure TCOMWFRenderer.PickLine(x1,y1,z1, x2,y2,z2: Double;id: Integer);
var v1,v2:TVertex;
begin
  v1 := Tvertex.Create;
  v1.x := x1; v1.y:=y1; v1.z:=z1;

  v2 := Tvertex.Create;
  v2.x := x2; v2.y:=y2; v2.z:=z2;
  Rend.PickLine(v1,v2,id);

  v1.Free;
  v2.Free;
end;

procedure TCOMWFRenderer.PickVertex(x, y, z: Double;id: Integer);
begin
  rend.PickVertex(x,y,z,id);
end;

procedure TCOMWFRenderer. BeginRectPick(x1,y1, x2,y2: Integer);
begin
 Rend.BeginRectPick(x1,y1, x2,y2);
end;

procedure TCOMWFRenderer.EndRectPick;
begin
  Rend.EndRectPick;
end;

function TCOMWFRenderer.IsSectorInRect(sc: Integer): Boolean;
begin
  Result := Rend.ArePolygonsInRect(Level.Sectors[sc].Surfaces);
end;

function TCOMWFRenderer.IsSurfaceInRect(sc,sf: Integer): Boolean;
begin
  Result := Rend.IsPolygonInRect(Level.Sectors[sc].Surfaces[sf]);
end;

function TCOMWFRenderer.IsLineInRect(x1,y1,z1, x2,y2,z2: Double): Boolean;
var v1,v2:TVertex;
begin
  v1 := Tvertex.Create;
  v2 := Tvertex.Create;
  v1.x := x1; v1.y:=y1; v1.z:=z1;
  v2.x := x2; v2.y:=y2; v2.z:=z2;
  Result := Rend.IsLineInRect(v1,v2);
  v1.free;
  v2.free;
end;

function TCOMWFRenderer.IsVertexInRect(x, y, z: Double): Boolean;
begin
  Result := Rend.IsVertexInRect(x,y,z);
end;

function TCOMWFRenderer.GetXYZonPlaneAt(scX,scY: Integer; const pnormal: TSedVector3; pX,pY,pZ: Double; var x, y, z: Double): Boolean;
begin
  Result := Rend.GetXYZonPlaneAt(scX,scY, Tvector(pnormal), pX,pY,pZ, x, y, z);
end;

function TCOMWFRenderer.GetGridAt(scX,scY: Integer; var x, y, z: Double): Boolean;
begin
  Result := rend.GetGridAt(scX,scY,x, y, z);
end;

procedure TCOMWFRenderer.GetNearestGridNode(iX,iY,iZ: Double; Var x, y, z: Double);
begin
  rend.GetNearestGrid(iX,iY,iZ,x, y, z);
end;

procedure TCOMWFRenderer.ProjectPoint(x,y,z: Double; Var winX, winY: Integer);
begin
  rend.ProjectPoint(x,y,z,winX, winY);
end;

procedure TCOMWFRenderer.UnProjectPoint(winX, winY: Integer; WinZ: Double; var x,y,z: Double);
begin
  rend.UnProjectPoint(winX, winY,WinZ,x,y,z);
end;

function TCOMWFRenderer.IsSurfaceFacing(sc,sf: Integer): Boolean;
begin
  Result := rend.IsPolygonFacing(Level.Sectors[sc].Surfaces[sf]);
end;

function TCOMWFRenderer.HandleWMQueryPal: Integer;
begin
  Result := Rend.HandleWMQueryPal;
end;

function TCOMWFRenderer.HandleWMChangePal: Integer;
begin
  Result := Rend.HandleWMChangePal;
end;

end.
