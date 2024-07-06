unit SED_COM;

interface
uses Windows, J_Level, ResourcePicker, sed_plugins, misc_utils, GlobalVars,
     u_undo, Forms, SysUtils, classes, Render, ogl_render, sft_render, Geometry,
     values, u_multisel, menus;

type

THandles = class(TList)
 oname: string;
 Function GetItem(n:integer):TObject;
 Procedure SetItem(n:integer;v:TObject);
 Function GetItemNoNIL(n:integer):TObject;

 Procedure FreeHandle(h:integer);
 Function NewHandle(v:TObject):integer;

 Constructor Create(const name:string);
 Destructor Destroy;override;

// Property Items[n:integer]:TObject read GetItem write SetItem; default;
end;


TSedCOM = class(ISed)
  files,
  tfiles,
  conts: THandles;

  Constructor Create;

  function QueryInterface(iid: pointer; var obj): LongInt; override;
  function AddRef: Longint; override;
  function Release: Longint; override;

  Function GetVersion:double; override;
  Function GetLevel:ISedLevel;override; {ISEDLevel object}

  Function GetMapMode:Integer;override;
  Procedure SetMapMode(mode:integer);override;
  Function GetCurrentSector:integer;override;
  Procedure SetCurrentSector(sc:integer);override;
  Function GetCurrentThing:integer;override;
  Procedure SetCurrentThing(th:integer);override;
  Function GetCurrentLight:integer;override;
  Procedure SetCurrentLight(lt:integer);override;
  Procedure GetCurrentVertex(var sc,vx:integer);override;
  Procedure SetCurrentVertex(sc,vx:integer);override;
  Procedure GetCurrentSurface(var sc,sf:integer);override;
  Procedure SetCurrentSurface(sc,sf:integer);override;
  Procedure GetCurrentEdge(var sc,sf,ed:integer);override;
  Procedure SetCurrentEdge(sc,sf,ed:integer);override;
  Procedure GetCurrentFrame(var th,fr:integer);override;
  Procedure SetCurrentFrame(th,fr:integer);override;

  Procedure NewLevel(kind: integer); override;
  Procedure LoadLevel(const filename: PChar); override;

  {Different level editing functions}

  Procedure FindBoundingBox(sec:integer;var box:TSedBox);override;
  Procedure FindBoundingSphere(sec:integer; var center: TSedVector3; radius: double);override;
  Function FindCollideBox(sec:integer; const bbox:TSedBox; const center: TSedVector3; var cbox:TSedBox):boolean;override;
  Procedure FindSurfaceCenter(sc,sf:integer; var center:TSedVector3);override;
  Procedure RotateVector(var vec: TSedVector3; const pyr: TSedVector3); override;
  procedure RotatePoint(const p, pivot: TSedVector3; angle: double; var p2: TSedVector3) ;override;
  Procedure GetPYR(const x,y,z:TSedVector3;var pyr:TSedVector3);override;
  Function IsSurfaceConvex(sc,sf:integer):boolean;override;
  Function IsSurfacePlanar(sc,sf:integer):boolean;override;
  Function IsSectorConvex(sec:integer):boolean;override;
  Function IsInSector(sec: Integer; const p: TSedVector3):boolean;override;
  Function DoSectorsOverlap(sec1,sec2:integer):boolean;override;
  Function IsPointOnSurface(sec, surf: Integer; const p: TSedVector3):boolean;override;
  Function FindSectorForThing(th:integer):Integer;override;
  Function FindSectorForXYZ(X,Y,Z:double):Integer;override;
  Function ExtrudeSurface(sc,sf:integer; by:double):integer;override;
  Function CleaveSurface(sec, surf: Integer; const cnormal: TSedVector3; cp: TSedVector3):integer;override;
  Function CleaveSector(sec: Integer; const cnormal: TSedVector3; const cp: TSedVector3):integer;override;
  Function CleaveEdge(sec, surf, ed: Integer; const cnormal: TSedVector3; cp: TSedVector3):boolean;override;
  Function JoinSurfaces(sc1,sf1,sc2,sf2:Integer):boolean;override;
  Function PlanarizeSurface(sc,sf:integer):boolean;override;
  function MergeSurfaces(sc,sf1,sf2:integer):integer;override;
  function MergeSectors(sec1,sec2:integer):integer;override;
  Procedure CalculateDefaultUVNormals(sc,sf:integer; orgvx:integer; var un,vn:TSedVector3);override;
  Procedure CalculateUVNormals(sc,sf:integer; var un,vn:TSedVector3);override;
  Procedure ArrangeTexture(sc,sf:integer; orgvx:integer; const un,vn:TSedVector3);override;
  Procedure ArrangeTextureBy(sec, surf: Integer; const un, vn: TSedVector3; const refp: TSedVector3; refu, refv: double);override;
  Function IsTextureFlipped(sc,sf:integer):boolean;override;
  Procedure RemoveSurfaceReferences(sc,sf:integer);override;
  Procedure RemoveSectorReferences(sec:integer;surfs:boolean);override;
  Function StitchSurfaces(sc1,sf1,sc2,sf2:integer):boolean;override;
  Function FindCommonEdges(sc1,sf1,sc2,sf2:integer; var v11,v12,v21,v22:integer):boolean;override;
  Function DoSurfacesOverlap(sc1,sf1,sc2,sf2:integer):boolean;override;
  Function MakeAdjoin(sc,sf:integer):boolean;override;
  Function MakeAdjoinFromSectorUp(sc,sf:integer;firstsc:integer):boolean;override;
  Function UnAdjoin(sc,sf:integer):Boolean;override;
  Function CreateCubicSector(const pos: TSedVector3; const pnormal, edge: TSedVector3):integer;override;

  Procedure StartUndo(const name: PChar);override;
  Procedure SaveUndoForThing(n:integer;change:integer);override;
  Procedure SaveUndoForLight(n:integer;change:integer);override;
  Procedure SaveUndoForSector(n:integer;change:integer;whatpart:integer);override;
  Procedure ClearUndoBuffer;override;
  Procedure ApplyUndo;override;

  {JED 0.92}
  Function GetApplicationHandle:Integer;override;
  Function JoinSectors(sec1,sec2:integer):boolean;override;

  Function GetNumMultiselected(what:integer):integer;override;
  Procedure ClearMultiselection(what:integer);override;
  Procedure RemoveFromMultiselection(what,n:integer);override;
  Function GetSelectedSector(n:integer):integer;override;
  Function GetSelectedThing(n:integer):integer;override;
  Function GetSelectedLight(n:integer):integer;override;

  procedure GetSelectedSurface(n:integer;var sc,sf:integer);override;
  procedure GetSelectedEdge(n:integer;var sc,sf,ed:integer);override;
  procedure GetSelectedVertex(n:integer;var sc,vx:integer);override;
  procedure GetSelectedFrame(n:integer;var th,fr:integer);override;

  Function SelectSector(sc:integer):integer;override;
  Function SelectSurface(sc,sf:integer):integer;override;
  Function SelectEdge(sc,sf,ed:integer):integer;override;
  Function SelectVertex(sc,vx:integer):integer;override;
  Function SelectThing(th:integer):integer;override;
  Function SelectFrame(th,fr:integer):integer;override;
  Function SelectLight(lt:integer):integer;override;

  Function FindSelectedSector(sc:integer):integer;override;
  Function FindSelectedSurface(sc,sf:integer):integer;override;
  Function FindSelectedEdge(sc,sf,ed:integer):integer;override;
  Function FindSelectedVertex(sc,vx:integer):integer;override;
  Function FindSelectedThing(th:integer):integer;override;
  Function FindSelectedFrame(th,fr:integer):integer;override;
  Function FindSelectedLight(lt:integer):integer;override;

  Procedure Save(const filename: PChar);override;
  Procedure SaveAsSed(const filename: PChar);override;
  Procedure UpdateMap;override;
  Procedure SetPickerCMP(const cmp: PChar);override;
  Function PickResource(what:integer; const cur: PChar):PChar;override;
  Function EditFlags(what: integer; flags:LongInt):LongInt;override;

  Procedure ReloadTemplates; override;
  Procedure PanMessage(mtype: integer; const msg:PChar);override;
  Procedure SendKey(shift: integer; key: integer);override;
  Function ExecuteMenu(const itemref: PChar): boolean;override;
  Function GetSedSetting(const name: PChar):variant;override;
  Function IsLayerVisible(n:integer):boolean;override;

  procedure CheckConsistencyErrors;override;
  procedure CheckResources;override;
  Function GetNumConsistencyErrors:integer;override;
  Function GetConsistencyErrorString(n:integer):PChar;override;
  Function GetConsistencyErrorType(n:integer):integer;override;
  Function GetConsErrorItemType(n: integer):integer;virtual;stdcall; // SED 0.1.0
  Function GetConsistencyErrorSector(n:integer):integer;override;
  Function GetConsistencyErrorSurface(n:integer):integer;override;
  Function GetConsistencyErrorThing(n:integer):integer;override;
  Function GetConsistencyErrorCOG(n:integer):integer;override;
  Function IsPreviewActive:boolean;override;
  Function GetSedString(what:integer):PChar;override;

  Function GetProjectType:integer;override;                // SED 0.1.0
  Procedure SetProjectType(kind:integer);override;         // SED 0.1.0

  {JED 0.93}
  Function GetSedWindow(whichone:integer):integer;override;

  Function FileExtractExt(const filename:PChar):PChar;override;
  Function FileExtractPath(const filename:PChar):PChar;override;
  Function FileExtractName(const filename:PChar):PChar;override;
  Function FindProjectDirFile(const filename:PChar):PChar;override;
  Function IsFileContainer(const filename:PChar):boolean;override;
  Function IsFileInContainer(const filename:PChar):Boolean;override;

  Function FileOpenDialog(const filename:PChar;filter:PChar):PChar;override;
  Function OpenFile(const filename:PChar):integer;override;
  Function OpenGameFile(const filename:PChar):integer;override;
  Function ReadFile(handle:integer;buf:pointer;size:longint):integer;override;
  Procedure SetFilePos(handle:integer;pos:longint);override;
  Function GetFilePos(handle:integer):longint;override;
  Function GetFileSize(handle:integer):longint;override;
  Function GetFileName(handle:integer):PChar;override;
  Procedure CloseFile(handle:integer);override;

  Function OpenTextFile(const filename:PChar):integer;override;
  Function OpenGameTextFile(const filename:PChar):integer;override;
  Function GetTextFileName(handle:integer):PChar;override;
  Function ReadTextFileString(handle:integer):PChar;override;
  Function TextFileEOF(handle:integer):boolean;override;
  Function TextFileCurrentLine(handle:integer):integer;override;
  Procedure CloseTextFile(handle:integer);override;

  Function GobOpen(const filename:PChar):integer;override;
  Function GobNumFiles(handle:integer):integer;override;
  Function GobGetFilename(handle:integer;n:integer):PChar;override;
  Function GobGetFullFilename(handle:integer;n:integer):PChar;override;
  Function GobGetFileSize(handle:integer;n:integer):longint;override;
  Function GobGetFileOffset(handle:integer;n:integer):longint;override;
  Procedure GobClose(handle:integer);override;

  Function CreateWFRenderer(wnd:integer;whichone:integer):ISedWFRenderer;override;
end;

 TSedCOMLevel=class(ISedLevel)
  function QueryInterface(iid: pointer; var obj): LongInt; override;
  function AddRef: Longint; override;
  function Release: Longint; override;

  Procedure GetLevelHeader(var lh:TSedLevelHeader;rflags:integer);override;
  Procedure SetLevelHeader(const lh:TSedLevelHeader;rflags:integer);override;

  Function NumSectors :integer; override;
  Function NumThings: integer; override;
  Function NumLights: integer; override;
  Function NumCogs: integer; override;

  {Sectors}
  Function SectorAdd: integer; override;
  Procedure SectorDelete(n: integer); override;

  Procedure SectorGet(sec:integer; var rec: TSedSectorRec; what: integer); override;
  Procedure SectorSet(sec:integer; const rec: TSedSectorRec; what: integer); override;

  Function SectorNumVertices(sec: integer): integer; override;
  Function SectorNumSurfaces(sec: integer): integer; override;

  Procedure SectorGetVertex(sec, vn: Integer; var vert: TSedVector3); override;
  Procedure SectorSetVertex(sec, vn: Integer; const vert: TSedVector3); override;

  Function SectorAddVertex(sec: Integer; const vert: TSedVector3):integer; override;
  Function SectorFindVertex(sec: Integer; const vert: TSedVector3):integer; override;
  Function SectorDeleteVertex(sec: Integer; n:Integer): Integer; override;

  Function SectorAddSurface(sec: integer): integer; override;
  Procedure SectorDeleteSurface(sc, sf: integer); override;
  Procedure SectorUpdate(sec: integer); override;

  {Surfaces}
  Procedure SurfaceGet(sc,sf: integer; var rec: TSedSurfaceRec; rflags: integer); override;
  Procedure SurfaceSet(sc, sf: integer; const rec: TSedSurfaceRec; rflags: integer); override;
  Procedure SurfaceGetNormal(sc, sf: integer; var n: TSedVector3); override;
  Procedure SurfaceUpdate(sc, sf: integer; how: integer); override;
  Function SurfaceNumVertices(sc, sf: integer): Integer; override;
  Function SurfaceGetVertexNum(sc, sf, vx: integer): integer; override;
  Procedure SurfaceSetVertexNum(sc, sf, vx: integer; secvx: integer); override;
  Function SurfaceAddVertex(sc, sf: integer; secvx: integer): Integer; override;
  Function SurfaceInsertVertex(sc, sf: integer; at: integer; secvx: integer): Integer; override;
  Procedure SurfaceDeleteVertex(sc, sf: integer; n: integer); override;
  Procedure SurfaceGetVertexUV(sc, sf, vx: integer; var u,v: single); override;
  Procedure SurfaceSetVertexUV(sc, sf, vx: integer; u,v: single); override;
  Procedure SurfaceGetVertexLight(sc, sf, vx: integer; var color: TSedColor); override;
  Procedure SurfaceSetVertexLight(sc, sf, vx: integer; const color: TSedColor); override;

  {Things}
  Function ThingAdd: Integer; override;
  Procedure ThingDelete(th: integer); override;
  Procedure ThingGet(th: integer; var rec: TSedThingRec; rflags: integer); override;
  Procedure ThingSet(th: integer; const rec: TSedThingRec; rflags: integer); override;
  Procedure ThingUpdate(th: integer); override;

  {Lights}
  Function LightAdd:Integer; override;
  Procedure LightDelete(lt:integer); override;
  Procedure LightGet(lt:integer; var rec: TSedLightRec; rflags: integer);override;
  Procedure LightSet(lt:integer; const rec: TSedLightRec; rflags: integer);override;
  Procedure LightUpdate(lt:integer); override;

  {Layers}
  Function NumLayers: integer; override;
  Function LayerGetName(n: integer): PChar; override;
  Function LayerAdd(const name: PChar): integer; override;

  {JED 0.92}
  Function ThingNumValues(th:integer):integer;override;
  Function ThingValueGetName(th,n:Integer):PChar;override;
  Function ThingValueGetData(th,n:integer):PChar;override;
  Procedure ThingValueSetData(th,n:Integer; const val:PChar);override;

  Procedure ThingFrameGet(th,n: Integer; var pos, pyr: TSedVector3);override;
  Procedure ThingFrameSet(th,n: Integer; const pos, pyr: TSedVector3);override;

  Function ThingValueAdd(th:integer; const name,val: PChar):integer;override;
  Procedure ThingValueInsert(th,n:Integer; const name,val: PChar);override;
  Procedure ThingValueDelete(th,n:integer);override;

  {COGs}
  Function CogAdd(const name:PChar):Integer;override;
  Procedure CogDelete(n:integer);override;
  Procedure CogUpdate(cg:integer);override;

  Function CogGetFilename(cg:integer):PChar;override;
  Function CogNumValues(cg: integer):integer;override;

  Function CogValueGetName(cg,n:Integer):PChar;override;
  Function CogValueGetType(cg,n:Integer):integer;override;

  Function CogValueGet(cg,n:Integer):PChar;override;
  Function CogValueSet(cg,n:Integer; const val: PChar):boolean;override;

  Function CogValueAdd(cg:integer; const name,val: PChar; vtype: integer):integer;override;
  Procedure CogValueInsert(cg,n:Integer; const name,val: PChar; vtype: integer);override;
  Procedure CogValueDelete(cg,n:integer); override;
 end;

 TCOMWFRenderer = class(ISedWFRenderer)
  Rend:TRenderer;
  {OLE2 crap. ignore}

  Constructor Create(wnd:integer;which:integer);

  function QueryInterface(iid: pointer; var obj): LongInt; override;
  function AddRef: Longint; override;
  function Release: Longint; override;

  {Renderer attributes}
  Function GetRendererDouble(what:integer):double;override;
  Procedure SetRendererDouble(what:integer;val:double);override;
  Procedure GetRendererVector(what:integer;var x,y,z:double);override;
  Procedure SetRendererVector(what:integer;x,y,z:double);override;

  Function NSelected:integer; override;
  Function GetNSelected(n:integer):integer; override;
  Procedure SetViewPort(x,y,w,h:integer);override;
  Procedure SetColor(what,r,g,b:byte);override;
  Procedure SetPointSize(size:double);override;
  Procedure BeginScene;override;
  Procedure EndScene;override;
  Procedure SetCulling(how:integer);override;

  Procedure DrawSector(sc:integer);override;
  Procedure DrawSurface(sc,sf:integer);override;

  Procedure DrawLine(x1,y1,z1,x2,y2,z2:double);override;
  Procedure DrawVertex(X,Y,Z:double);override;
  Procedure DrawGrid;override;

  Procedure BeginPick(x,y:integer);override;
  Procedure EndPick;override;

  Procedure PickSector(sc:integer;id:integer);override;
  Procedure PickSurface(sc,sf:integer;id:integer);override;
  Procedure PickLine(x1,y1,z1,x2,y2,z2:double;id:integer);override;
  Procedure PickVertex(X,Y,Z:double;id:integer);override;

  Procedure BeginRectPick(x1,y1,x2,y2:integer);override;
  Procedure EndRectPick;override;
  Function IsSectorInRect(sc:integer):boolean;override;
  Function IsSurfaceInRect(sc,sf:integer):boolean;override;
  Function IsLineInRect(x1,y1,z1,x2,y2,z2:double):boolean;override;
  Function IsVertexInRect(X,Y,Z:double):boolean;override;

  Function GetXYZonPlaneAt(scX,scY:integer; const pnormal:TSedVector3; pX,pY,pZ:double; var X,Y,Z:double):Boolean;override;
  Function GetGridAt(scX,scY:integer;var X,Y,Z:double):boolean;override;
  Procedure GetNearestGridNode(iX,iY,iZ:double; Var X,Y,Z:double);override;
  Procedure ProjectPoint(x,y,z:double; Var WinX,WinY:integer);override;
  Procedure UnProjectPoint(WinX,WinY:integer; WinZ:double; var x,y,z:double);override;
  Function IsSurfaceFacing(sc,sf:integer):boolean;override;

  Function HandleWMQueryPal:integer;override;
  Function HandleWMChangePal:integer;override;
 end;


Function GetSedCOM: ISed;

implementation
uses  Item_edit, u_MsgForm, u_Tools, JED_Main,lev_utils,(*JED_OLE,*) listRes,
      u_CogForm, FlagEditor, U_SCFEdit,U_Options, u_tbar, Cons_Checker, u_Preview,
      Files, FileOperations, FileDialogs;

var com: ISed;
    comlevel: TSedCOMLevel;
    tmpstr: string;
    tmpastr: array[0..2047] of Char;
    ForeWnd:integer;

Function THandles.GetItemNoNIL(n:integer):TObject;
begin
 if (n<0) or (n>=count) then raise EListError.CreateFmt('Invalid %s handle: %d',[oname,n]);
 Result:=TObject(List[n]);
 if (result=nil) then raise EListError.CreateFmt('Invalid %s handle: %d',[oname,n]);
end;

Function THandles.GetItem(n:integer):TObject;
begin
 if (n<0) or (n>=count) then raise EListError.CreateFmt('Invalid %s handle: %d',[oname,n]);
 Result:=TObject(List[n]);
end;

Procedure THandles.SetItem(n:integer;v:TObject);
begin
 if (n<0) or (n>=count) then raise EListError.CreateFmt('Invalid %s handle: %d',[oname,n]);
 List[n]:=v;
end;

Procedure THandles.FreeHandle(h:integer);
var v:TObject;
begin
 v:=GetItem(h);
 v.free;
 SetItem(h,nil);
end;

Function THandles.NewHandle(v:TObject):integer;
var i:integer;
begin
 if v=nil then begin result:=-1; exit; end;

 for i:=0 to Count-1 do
 if Items[i]=nil then
 begin
  Items[i]:=v;
  result:=i;
  exit;
 end;
 result:=Add(v);
end;

Constructor THandles.Create(const name:string);
begin
 oname:=name;
end;

Destructor THandles.Destroy;
var i:integer;
begin
 for i:=0 to Count-1 do
  TObject(List[i]).Free;
end;

Function GetSedCOM: ISed;
begin
 if com=nil then com:=TSedCOM.Create;
 result:=com;
end;

Procedure SaveCurApp;
begin
 ForeWnd:=GetForeGroundWindow;
 SetForeGroundWindow(JedMain.Handle);
end;

Procedure RestoreCurApp;
begin
 SetForeGroundWindow(ForeWnd);
end;


Constructor TSedCOM.Create;
begin
 files:=THandles.Create('file');
 tfiles:=THandles.Create('text file');
 conts:=THandles.Create('GOB');
end;


function TSedCOM.QueryInterface(iid: pointer; var obj): LongInt;
begin
 Result:= CLASS_E_CLASSNOTAVAILABLE;
end;

function TSedCOM.AddRef: Longint;
begin
 Result := 1;
end;

function TSedCOM.Release: Longint;
begin
 Result := 0;
end;

Function TSedCOM.GetVersion:double;
begin
 ValDouble(SedVerNum,Result);
 Result := result * 100;
end;

Function TSedCOM.GetLevel: ISedLevel; {ISEDLevel object}
begin
 if comlevel = nil then comlevel := TSedCOMLevel.Create;
 result := comLevel;
end;

{Level}

function TSedCOMLevel.QueryInterface(iid: pointer; var obj): LongInt;
begin
 Result:= CLASS_E_CLASSNOTAVAILABLE;
end;

function TSedCOMLevel.AddRef: Longint;
begin
 result:=1;
end;

function TSedCOMLevel.Release: Longint;
begin
 Result:=0;
end;

Procedure TSedCOMLevel.SectorGet(sec:integer;var rec:TSedSectorRec;what:integer);
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

Procedure TSedCOMLevel.SectorSet(sec:integer;const rec:TSedSectorRec;what:integer);
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

Procedure TSedCOMLevel.GetLevelHeader(var lh:TSedLevelHeader;rflags:integer);
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

  if (rflags and lh_HorDist)  <> 0 then lh.horizonSkyDistance := horizonSky.distance;
  if (rflags and lh_HorPPR)   <> 0 then lh.horizonSkyPixelsPerRev :=horizonSky.pixelsPerRev;
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
  if (rflags and lh_MasterCMP)   <> 0 then lh.aMasterCmp := PChar(level.masterCMP);
  if (rflags and lh_Fog)         <> 0 then lh.fog := TSedFog(Fog);
 end;
end;

Procedure TSedCOMLevel.SetLevelHeader(const lh:TSedLevelHeader;rflags:integer);
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
  if (rflags and lh_Fog)         <> 0 then fog := TFog(lh.fog);
 end;
end;

Function TSedCOMLevel.NumSectors:integer;
begin
 result := level.sectors.Count;
end;

Function TSedCOMLevel.NumThings:integer;
begin
 result := level.things.Count;
end;

Function TSedCOMLevel.NumLights:integer;
begin
 result := level.lights.Count;
end;

Function TSedCOMLevel.NumCogs:integer;
begin
 result := level.cogs.Count;
end;

{Sectors}
Function TSedCOMLevel.SectorAdd:integer;
var sec:TJKSector;
begin
 sec := level.NewSector;
 Result := level.sectors.Add(sec);
 level.RenumSecs;
 JedMain.SectorAdded(sec);
end;

Procedure TSedCOMLevel.SectorDelete(n:integer);
begin
 lev_utils.DeleteSector(level, n);
end;

Function TSedCOMLevel.SectorNumVertices(sec:integer):integer;
begin
 result := level.sectors[sec].vertices.Count;
end;

Function TSedCOMLevel.SectorNumSurfaces(sec:integer):integer;
begin
 result := level.sectors[sec].surfaces.Count;
end;

Procedure TSedCOMLevel.SectorGetVertex(sec, vn: Integer; var vert: TSedVector3);
var v: TJKVertex;
begin
  v := level.sectors[sec].vertices[vn];
  vert.x := v.x;
  vert.y := v.y;
  vert.z := v.z;
end;

Procedure TSedCOMLevel.SectorSetVertex(sec, vn: Integer; const vert: TSedVector3);
var v: TJKVertex;
begin
  v := level.sectors[sec].vertices[vn];
  v.x := vert.x;
  v.y := vert.y;
  v.z := vert.z;
end;

Function TSedCOMLevel.SectorAddVertex(sec: Integer; const vert: TSedVector3):integer;
var v: TJKVertex;
begin
  v := level.sectors[sec].NewVertex;
  v.x := vert.x;
  v.y := vert.y;
  v.z := vert.z;
  result := level.sectors[sec].vertices.Count - 1;
end;

Function TSedCOMLevel.SectorFindVertex(sec: Integer; const vert: TSedVector3): integer;
begin
 result := level.sectors[sec].FindVX(vert.x, vert.y, vert.z);
end;

Function TSedCOMLevel.SectorDeleteVertex(sec:integer;n:integer):integer;
begin
 with level.sectors[sec] do
 begin
  vertices[n].Free;
  vertices.Delete(n);
  renumber;
 end;
end;

Function TSedCOMLevel.SectorAddSurface(sec: integer): integer;
begin
 with level.sectors[sec] do
 begin
  Result := surfaces.add(newSurface);
 end;
end;

Procedure TSedCOMLevel.SectorDeleteSurface(sc, sf: Integer);
begin
 lev_utils.DeleteSurface(level, sc, sf);
end;

Procedure TSedCOMLevel.SectorUpdate(sec:integer);
var asec:TJKSector;
begin
 asec := level.sectors[sec];
 asec.Renumber;
 JedMain.SectorChanged(asec);
end;

  {Surfaces}
Procedure TSedCOMLevel.SurfaceGet(sc,sf:integer;var rec:TSedSurfaceRec;rflags:integer);
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

Procedure TSedCOMLevel.SurfaceSet(sc,sf: Integer; const rec: TSedSurfaceRec; rflags: Integer);
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

Procedure TSedCOMLevel.SurfaceGetNormal(sc,sf: Integer; var n: TSedVector3);
begin
 with level.sectors[sc].surfaces[sf].normal do
 begin
  n.dx := dx;
  n.dy := dy;
  n.dz := dz;
 end;
end;

Procedure TSedCOMLevel.SurfaceUpdate(sc,sf:integer;how:integer);
var surf:TJKSurface;
begin
 surf := level.sectors[sc].surfaces[sf];
 if how and su_texture   <> 0 then surf.RecalcAll else surf.Recalc;
 if how and su_floorflag <> 0 then surf.CheckIfFloor;
 if how and su_sector    <> 0 then JedMain.SectorChanged(surf.sector);
end;

Function TSedCOMLevel.SurfaceNumVertices(sc,sf:integer):Integer;
begin
 Result := level.sectors[sc].surfaces[sf].vertices.count;
end;

Function TSedCOMLevel.SurfaceGetVertexNum(sc,sf,vx:integer):integer;
begin
 Result := level.sectors[sc].surfaces[sf].vertices[vx].num;
end;

Procedure TSedCOMLevel.SurfaceSetVertexNum(sc,sf,vx:integer;secvx:integer);
begin
 with level.sectors[sc] do
 begin
  surfaces[sf].vertices[vx] := vertices[secvx];
 end;
end;

Function TSedCOMLevel.SurfaceAddVertex(sc,sf:integer;secvx:integer):Integer;
begin
 with level.sectors[sc] do
 begin
  result := surfaces[sf].AddVertex(vertices[secvx]);
 end;
end;

Function TSedCOMLevel.SurfaceInsertVertex(sc,sf:integer;at:integer;secvx:integer):Integer;
begin
 with level.sectors[sc] do
 begin
  result := surfaces[sf].InsertVertex(at, vertices[secvx]);
 end;
 result := at;
end;

Procedure TSedCOMLevel.SurfaceDeleteVertex(sc,sf:integer;n:integer);
begin
 level.sectors[sc].surfaces[sf].DeleteVertex(n);
end;

Procedure TSedCOMLevel.SurfaceGetVertexUV(sc,sf,vx:integer;var u,v:single);
var tv:TTXVertex;
begin
 tv := level.sectors[sc].surfaces[sf].txvertices[vx];
 u := tv.u;
 v := tv.v;
end;

Procedure TSedCOMLevel.SurfaceSetVertexUV(sc,sf,vx:integer;u,v:single);
var tv: TTXVertex;
begin
 tv := level.sectors[sc].surfaces[sf].txvertices[vx];
 tv.u := u;
 tv.v := v;
end;

Procedure TSedCOMLevel.SurfaceGetVertexLight(sc, sf, vx: integer; var color: TSedColor);
var tv:TTXVertex;
begin
 tv := level.sectors[sc].surfaces[sf].txvertices[vx];
 color := TSedColor(tv.color);
end;

Procedure TSedCOMLevel.SurfaceSetVertexLight(sc, sf, vx: integer; const color: TSedColor);
var tv:TTXVertex;
begin
 tv := level.sectors[sc].surfaces[sf].txvertices[vx];
 tv.color := TColorF(color);
end;

{Things}

Function TSedCOMLevel.ThingAdd:Integer;
var th:TJKThing;
begin
 th := level.NewThing;
 Result := level.things.Add(th);
 level.RenumThings;
 JedMain.ThingAdded(th);
end;

Procedure TSedCOMLevel.ThingDelete(th: integer);
begin
 Lev_Utils.DeleteThing(level, th);
end;

Procedure TSedCOMLevel.ThingGet(th: Integer;var rec: TSedThingRec; rflags:integer);
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
   rec.rotation.yaw  := YAW;
   rec.rotation.roll := ROL;
  end;

  if (rflags and th_layer)<>0 then rec.layer:=layer;

 end;
end;

Procedure TSedCOMLevel.ThingSet(th:integer;const rec:TSedThingRec;rflags:integer);
begin
 with level.things[th] do
 begin
  if (rflags and th_name)<>0 then name:=rec.aName;
  if (rflags and th_sector)<>0 then
  begin
   if rec.Sector<0 then sec:=nil else
   sec:=Level.Sectors[rec.Sector];
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

Procedure TSedCOMLevel.ThingUpdate(th:integer);
begin
 Level.RenumThings;
 JedMain.ThingChanged(level.Things[th]);
end;

{Lights}

Function TSedCOMLevel.LightAdd: Integer;
var lt: TSedLight;
begin
  lt := Level.NewLight;
  Result := Level.lights.Add(lt);
  JedMain.LightAdded(lt);
end;

Procedure TSedCOMLevel.LightDelete(lt: integer);
begin
  Lev_Utils.DeleteLight(Level, lt);
end;

Procedure TSedCOMLevel.LightGet(lt: integer; var rec: TSedLightRec; rflags: Integer);
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

Procedure TSedCOMLevel.LightSet(lt:integer;const rec:TSedLightRec;rflags:integer);
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

Procedure TSedCOMLevel.LightUpdate(lt:integer);
begin;
  JedMain.LightChanged(level.Lights[lt]);
end;

Function TSedCOMLevel.NumLayers:integer;
begin
  Result:=Level.Layers.count;
end;

Function TSedCOMLevel.LayerGetName(n:integer): PChar;
begin
  Result := PChar(Level.GetLayerName(n));
end;

Function TSedCOMLevel.LayerAdd(const name: PChar):integer;
begin
  Result := Level.AddLayer(name);
end;

{0.92}
Function TSedCOMLevel.ThingNumValues(th:integer):integer;
begin
  result:=Level.Things[th].Vals.count;
end;

Function TSedCOMLevel.ThingValueGetName(th,n:Integer): PChar;
begin
  result := PChar(Level.Things[th].Vals[n].Name);
end;

Function TSedCOMLevel.ThingValueGetData(th,n: integer): PChar;
begin
  result := PChar(Level.Things[th].Vals[n].AsString);
end;

Procedure TSedCOMLevel.ThingValueSetData(th,n: Integer; const val: PChar);
var v: TTPLValue;
begin
  v := Level.Things[th].Vals[n];
  S2TPLVal(v.name + '=' + val,v);
end;

Procedure TSedCOMLevel.ThingFrameGet(th, n: Integer; var pos, pyr: TSedVector3);
begin
  Level.Things[th].Vals[n].GetFrame(pos.x, pos.y, pos.z, pyr.pitch, pyr.yaw, pyr.roll);
end;

Procedure TSedCOMLevel.ThingFrameSet(th,n: Integer; const pos, pyr: TSedVector3);
begin
  Level.Things[th].Vals[n].SetFrame(pos.x, pos.y, pos.z, pyr.pitch, pyr.yaw, pyr.roll);
end;


Function TSedCOMLevel.ThingValueAdd(th: integer; const name,val: PChar): integer;
var v:TTPLValue;
begin
  v:=TTplValue.Create;
  S2TPLVal(name + '=' + val, v);
  Result:=Level.Things[th].Vals.Add(v);
end;

Procedure TSedCOMLevel.ThingValueInsert(th,n: Integer; const name,val: PChar);
var v:TTPLValue;
begin
  v := TTplValue.Create;
  S2TPLVal(name + '=' + val, v);
  Level.Things[th].Vals.Insert(n, v);
end;

Procedure TSedCOMLevel.ThingValueDelete(th,n: integer);
var v:TTPLValue;
begin
 v:=Level.Things[th].Vals[n];
 Level.Things[th].Vals.Delete(n);
 v.free;
end;


{COGs}
Function TSedCOMLevel.CogAdd(const name: PChar): Integer;
var cg:TCOG;
    cf:TCogFile;
    i:integer;
    cv:TCOGValue;
begin
 cg:=TCOG.Create;
 cg.Name:=name;

 cf:=TCogFile.Create;
 cf.LoadNoLocals(Name);

 for i:=0 to cf.count-1 do
 begin
  cv:=TCOGValue.Create;
  cg.Vals.Add(cv);
  cv.Assign(cf[i]);
 end;

 cf.Free;

 Result:=Level.Cogs.Add(cg);
 CogForm.RefreshList;
 JedMain.LevelChanged;
end;

Procedure TSedCOMLevel.CogDelete(n: integer);
var cog:TCOG;
begin
 lev_utils.DeleteCOG(level,n);
end;

Procedure TSedCOMLevel.CogUpdate(cg: integer);
begin
 COgForm.UpdateCOG(cg);
end;

Function TSedCOMLevel.CogGetFilename(cg: integer): PChar;
begin
 Result := PChar(level.cogs[cg].name);
end;

Function TSedCOMLevel.CogNumValues(cg: integer): integer;
begin
 Result := level.cogs[cg].vals.count;
end;

Function TSedCOMLevel.CogValueGetName(cg,n: Integer): PChar;
begin
 Result := PChar(level.cogs[cg].vals[n].Name);
end;

Function TSedCOMLevel.COGValueGetType(cg,n:Integer): integer;
begin
 Result := Integer(level.cogs[cg].vals[n].cog_type);
end;

Function TSedCOMLevel.CogValueGet(cg,n:Integer): PChar;
begin
 Result := PChar(level.cogs[cg].vals[n].AsString);
end;

Function TSedCOMLevel.CogValueSet(cg,n: Integer; const val: PChar): Boolean;
begin
 Result:=level.cogs[cg].vals[n].JedVal(val);
end;

Function TSedCOMLevel.CogValueAdd(cg:integer; const name,val: PChar; vtype:integer): integer;
var v:TCOGValue;
begin
 v := TCogValue.Create;
 v.name  := name;
 v.vtype := GetVTypeFromCOGType(TCOG_TYpe(vtype));
 v.cog_type := TCOG_Type(vtype);
 v.Val(val);
 result := level.cogs[cg].vals.Add(v);
end;

Procedure TSedCOMLevel.CogValueInsert(cg,n:Integer; const name,val: PChar; vtype:integer);
var v:TCOGValue;
begin
 v := TCogValue.Create;
 v.name  := name;
 v.vtype := GetVTypeFromCOGType(TCOG_TYpe(vtype));
 v.cog_type := TCOG_Type(vtype);
 v.Val(val);
 level.cogs[cg].vals.Insert(n,v);
end;

Procedure TSedCOMLevel.COGValueDelete(cg,n: integer);
var v:TCOGValue;
begin
 v := level.cogs[cg].Vals[n];
 level.cogs[cg].Vals.Delete(n);
 v.free;
end;


{SEDCOM}

Function TSedCOM.GetMapMode:Integer;
begin
 result:=JedMain.mapMode;
end;

Procedure TSedCOM.SetMapMode(mode:integer);
begin
 JedMain.SetMapMode(Mode);
end;

Function TSedCOM.GetCurrentSector:integer;
begin
 Result:=JedMain.Cur_SC;
end;

Procedure TSedCOM.SetCurrentSector(sc:integer);
begin
 JedMain.SetCurSC(sc);
end;

Function TSedCOM.GetCurrentThing:integer;
begin
 Result:=JedMain.Cur_TH;
end;

Procedure TSedCOM.SetCurrentThing(th:integer);
begin
 JedMain.SetCurTH(th);
end;

Function TSedCOM.GetCurrentLight:integer;
begin
 Result:=JedMain.Cur_LT;
end;

Procedure TSedCOM.SetCurrentLight(lt:integer);
begin
 JedMain.SetCurLT(lt);
end;

Procedure TSedCOM.GetCurrentVertex(var sc,vx:integer);
begin
 sc:=JedMain.Cur_SC;
 vx:=JedMain.Cur_VX;
end;

Procedure TSedCOM.SetCurrentVertex(sc,vx:integer);
begin
 JedMain.SetCurVX(sc,vx);
end;

Procedure TSedCOM.GetCurrentSurface(var sc,sf:integer);
begin
 sc:=JedMain.Cur_SC;
 sf:=JedMain.Cur_SF;
end;

Procedure TSedCOM.SetCurrentSurface(sc,sf:integer);
begin
 JedMain.SetCurSF(sc,sf);
end;

Procedure TSedCOM.GetCurrentEdge(var sc,sf,ed:integer);
begin
 sc:=JedMain.Cur_SC;
 sf:=JedMain.Cur_SF;
 ed:=JedMain.Cur_ED;
end;

Procedure TSedCOM.SetCurrentEdge(sc,sf,ed:integer);
begin
 JedMain.SetCurED(sc,sf,ed);
end;

Procedure TSedCOM.GetCurrentFrame(var th,fr:integer);
begin
 th:=JedMain.Cur_TH;
 fr:=JedMain.Cur_FR;
end;

Procedure TSedCOM.SetCurrentFrame(th,fr:integer);
begin
 JedMain.SetCurFR(th,fr);
end;

Procedure TSedCOM.NewLevel(kind: integer);
begin
 case TProjectType(kind) of
  TProjectType.JKDF2: JedMain.New1.Click;
  TProjectType.MOTS: JedMain.NewMotsProject1.Click;
  else
    JedMain.NewIJIMProject.Click;
 end;
end;

Procedure TSedCOM.LoadLevel(const filename: PChar);
begin
 JedMain.OpenProject(filename, op_open);
end;

{Different level editing functions}

Procedure TSedCOM.FindBoundingBox(sec:integer;var box:TSedBox);
begin
 lev_utils.FindBBox(Level.Sectors[sec],TBox(box));
end;

Procedure TSedCOM.FindBoundingSphere(sec:integer; var center: TSedVector3; radius: double);
begin
 lev_utils.FindBSphere(level.Sectors[sec], center.x, center.y, center.z, radius);
end;

Function TSedCOM.FindCollideBox(sec:integer; const bbox:TSedBox; const center: TSedVector3; var cbox:TSedBox):boolean;
begin
 Result := lev_utils.FindCollideBox(
  level.sectors[sec],TBox(bbox),center.x, center.y,center.z,Tbox(cbox));
end;

Procedure TSedCOM.FindSurfaceCenter(sc,sf:integer; var center:TSedVector3);
begin
 lev_utils.CalcSurfCenter(level.sectors[sc].surfaces[sf], center.x, center.y, center.z);
end;

Procedure TSedCOM.RotateVector(var vec:TSedVector3; const pyr:TSedVector3);
begin
 lev_utils.RotateVector(TVector(vec), pyr.pitch, pyr.yaw, pyr.roll);
end;

Procedure TSedCOM.RotatePoint(const p, pivot: TSedVector3; angle: double; var p2: TSedVector3);
begin
 lev_utils.RotatePoint(p.x,p.y,p.z, pivot.x,pivot.y,pivot.z, angle, p2.x,p2.y,p2.z);
end;

Procedure TSedCOM.GetPYR(const x,y,z:TSedVector3;var pyr:TSedVector3);
begin
 lev_utils.GetJKPYR(Tvector(x),Tvector(y),Tvector(z), pyr.pitch, pyr.yaw, pyr.roll);
end;

Function TSedCOM.IsSurfaceConvex(sc,sf:integer):boolean;
begin
 result:=lev_utils.IsSurfConvex(Level.Sectors[sc].Surfaces[sf]);
end;

Function TSedCOM.IsSurfacePlanar(sc,sf:integer):boolean;
begin
 result:=lev_utils.IsSurfPlanar(Level.Sectors[sc].Surfaces[sf]);
end;

Function TSedCOM.IsSectorConvex(sec:integer):boolean;
begin
 result:=lev_utils.IsSectorConvex(level.sectors[sec]);
end;

Function TSedCOM.IsInSector(sec: Integer; const p: TSedVector3):boolean;
begin
 Result:=lev_utils.IsInSector(level.sectors[sec], p.x, p.y, p.z);
end;

Function TSedCOM.DoSectorsOverlap(sec1,sec2:integer):boolean;
begin
 result:=lev_utils.DoSectorsOverlap(level.sectors[sec1], level.sectors[sec2]);
end;

Function TSedCOM.IsPointOnSurface(sec, surf: Integer; const p: TSedVector3):boolean;
begin
 result:=lev_utils.IsPointOnSurface(level.sectors[sec].surfaces[surf], p.x, p.y, p.z);
end;

Function TSedCOM.FindSectorForThing(th:integer):Integer;
begin
 result := lev_utils.FindSectorForThing(level.things[th]);
end;

Function TSedCOM.FindSectorForXYZ(X,Y,Z:double):integer;
begin
 result := lev_utils.FindSectorForXYZ(level, x,y,z);
end;

Function TSedCOM.ExtrudeSurface(sc,sf:integer; by:double):integer;
begin
 lev_utils.ExtrudeSurface(level.sectors[sc].surfaces[sf],by);
 result := level.Sectors.count - 1;
end;

Function TSedCOM.CleaveSurface(sec, surf: Integer; const cnormal: TSedVector3; cp: TSedVector3):integer;
begin
 if lev_utils.CleaveSurface(level.sectors[sec].surfaces[surf], Tvector(Cnormal), cp.x, cp.y, cp.z) then
 result := level.sectors[sec].surfaces.count - 1 else result := -1;
end;

Function TSedCOM.CleaveSector(sec: Integer; const cnormal: TSedVector3; const cp: TSedVector3):integer;
begin
 if lev_utils.CleaveSector(level.sectors[sec], Tvector(Cnormal), cp.x, cp.y, cp.z) then
 result := level.sectors.count - 1 else result := -1;
end;

Function TSedCOM.CleaveEdge(sec, surf, ed: Integer; const cnormal: TSedVector3; cp: TSedVector3):boolean;
begin
 result:=lev_utils.CleaveEdge(level.sectors[sec].surfaces[surf], ed, Tvector(Cnormal), cp.x, cp.y, cp.z);
end;

Function TSedCOM.JoinSurfaces(sc1,sf1,sc2,sf2:Integer):boolean;
begin
 result:=lev_utils.ConnectSurfaces(level.sectors[sc1].surfaces[sf1],
                              level.sectors[sc2].surfaces[sf2]);
end;

Function TSedCOM.PlanarizeSurface(sc,sf:integer):boolean;
begin
 result:=lev_utils.FlattenSurface(level.sectors[sc].surfaces[sf]);
end;

Function TSedCOM.MergeSurfaces(sc,sf1,sf2:integer):integer;
var surf:TJKSurface;
begin
 surf:=lev_utils.MergeSurfaces(level.sectors[sc].surfaces[sf1], level.sectors[sc].surfaces[sf2]);
 if surf = nil then result := -1 else result := surf.num;
end;

Function TSedCOM.MergeSectors(sec1,sec2: integer):integer;
var sec:TJKSector;
begin
 sec:=lev_utils.MergeSectors(level.sectors[sec1], level.sectors[sec2]);
 if sec=nil then result := -1 else result := sec.num;
end;

Procedure TSedCOM.CalculateDefaultUVNormals(sc,sf:integer; orgvx:integer; var un,vn:TSedVector3);
begin
 lev_utils.CalcDefaultUVNormals(level.sectors[sc].surfaces[sf], orgvx, Tvector(un), Tvector(vn));
end;

Procedure TSedCOM.CalculateUVNormals(sc,sf:integer; var un,vn:TSedVector3);
begin
 lev_utils.CalcUVNormals(level.sectors[sc].surfaces[sf], Tvector(un), Tvector(vn));
end;

Procedure TSedCOM.ArrangeTexture(sc,sf:integer; orgvx:integer; const un,vn:TSedVector3);
begin
 lev_utils.ArrangeTexture(level.sectors[sc].surfaces[sf], orgvx, Tvector(un), Tvector(vn));
end;

Procedure TSedCOM.ArrangeTextureBy(sec, surf: Integer; const un, vn: TSedVector3; const refp: TSedVector3; refu, refv: double);
begin
  lev_utils.ArrangeTextureBy(
    level.sectors[sec].surfaces[surf], Tvector(un), Tvector(vn),
    refp.x, refp.y, refp.z, refu, refv);
end;

Function TSedCOM.IsTextureFlipped(sc,sf:integer):boolean;
begin
 Result := lev_utils.IsTXFlipped(level.sectors[sc].surfaces[sf]);
end;

Procedure TSedCOM.RemoveSurfaceReferences(sc,sf:integer);
begin
 lev_utils.RemoveSurfRefs(level, level.sectors[sc].surfaces[sf]);
end;

Procedure TSedCOM.RemoveSectorReferences(sec:integer; surfs:boolean);
begin
 if surfs then lev_utils.RemoveSecRefs(level, level.sectors[sec], rs_surfs)
 else lev_utils.RemoveSecRefs(level, level.sectors[sec], 0);
end;

Function TSedCOM.StitchSurfaces(sc1,sf1,sc2,sf2:integer):boolean;
begin
 Result:=lev_utils.StitchSurfaces(level.sectors[sc1].surfaces[sf1],
                                 level.sectors[sc2].surfaces[sf2]);
end;

Function TSedCOM.FindCommonEdges(sc1,sf1,sc2,sf2:integer; var v11,v12,v21,v22:integer):boolean;
begin
 Result:=lev_utils.FindCommonEdges(level.sectors[sc1].surfaces[sf1],
                                 level.sectors[sc2].surfaces[sf2],v11,v12,v21,v22);
end;

Function TSedCOM.DoSurfacesOverlap(sc1,sf1,sc2,sf2:integer):boolean;
begin
 Result:=lev_utils.Do_Surf_Overlap(level.sectors[sc1].surfaces[sf1],
                                 level.sectors[sc2].surfaces[sf2]);
end;

Function TSedCOM.MakeAdjoin(sc,sf:integer):boolean;
begin
 Result:=lev_utils.MakeAdjoin(level.sectors[sc].surfaces[sf]);
end;

Function TSedCOM.MakeAdjoinFromSectorUp(sc,sf:integer;firstsc:integer):boolean;
begin
 Result:=lev_utils.MakeAdjoinSCUp(level.sectors[sc].surfaces[sf], firstsc);
end;

Function TSedCOM.UnAdjoin(sc,sf:integer):Boolean;
begin
 Result:=lev_utils.UnAdjoin(level.sectors[sc].surfaces[sf]);
end;

Function TSedCOM.CreateCubicSector(const pos: TSedVector3; const pnormal, edge: TSedVector3):integer;
begin
 lev_utils.CreateCube(level, pos.x, pos.y, pos.z, Tvector(pnormal), TVector(edge));
 result := Level.sectors.count - 1;
end;

Procedure TSedCOM.StartUndo(const name: PChar);
begin
 StartUndoRec(name);
end;

Procedure TSedCOM.SaveUndoForThing(n:integer;change:integer);
begin
  case change of
    0: SaveThingUndo(level.things[n], ch_changed);
    1: SaveThingUndo(level.things[n], ch_added);
    2: SaveThingUndo(level.things[n], ch_deleted);
  end;
end;

Procedure TSedCOM.SaveUndoForLight(n:integer;change:integer);
begin
  case change of
    0: SaveLightUndo(level.lights[n], ch_changed);
    1: SaveLightUndo(level.lights[n], ch_added);
    2: SaveLightUndo(level.lights[n], ch_deleted);
  end;
end;

Procedure TSedCOM.SaveUndoForSector(n:integer;change:integer;whatpart:integer);
var how:integer;
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

Procedure TSedCOM.ClearUndoBuffer;
begin
 u_undo.ClearUndoBuffer;
end;

Procedure TSedCOM.ApplyUndo;
begin
 u_undo.ApplyUndo;
end;

{0.92}

Function TSedCOM.GetApplicationHandle:Integer;
begin
 result := Application.Handle;
end;

Function TSedCOM.JoinSectors(sec1,sec2:integer):boolean;
begin
 result := lev_utils.ConnectSectors(level.sectors[sec1], level.sectors[sec2]);
end;

Function TSedCOM.GetNumMultiselected(what: integer):integer;
begin
  With JedMain do
    case what of
      sed_plugins.mm_sc: result := scsel.count;
      sed_plugins.mm_sf: result := sfsel.count;
      sed_plugins.mm_ed: result := edsel.count;
      sed_plugins.mm_vx: result := vxsel.count;
      sed_plugins.mm_th: result := thsel.count;
      sed_plugins.mm_fr: result := frsel.count;
      sed_plugins.mm_lt: result := ltsel.count;
    end;
end;

Procedure TSedCOM.ClearMultiselection(what: integer);
begin
  With JedMain do
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

Procedure TSedCOM.RemoveFromMultiselection(what,n:integer);
begin
  With JedMain do
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

Function TSedCOM.GetSelectedSector(n:integer):integer;
begin
 Result := JedMain.scsel.GetSC(n);
end;

Function TSedCOM.GetSelectedThing(n:integer):integer;
begin
 Result := JedMain.thsel.GetTH(n);
end;

Function TSedCOM.GetSelectedLight(n:integer):integer;
begin
 Result := JedMain.ltsel.GetLT(n);
end;

Procedure TSedCOM.GetSelectedSurface(n:integer;var sc,sf:integer);
begin
 JedMain.sfsel.GetSCSF(n, sc, sf);
end;

Procedure TSedCOM.GetSelectedEdge(n:integer;var sc,sf,ed:integer);
begin
 JedMain.edsel.GetSCSFED(n,sc,sf,ed);
end;

Procedure TSedCOM.GetSelectedVertex(n:integer;var sc,vx:integer);
begin
 JedMain.vxsel.GetSCVX(n,sc,vx);
end;

Procedure TSedCOM.GetSelectedFrame(n:integer;var th,fr:integer);
begin
 JedMain.frsel.GetTHFR(n,th,fr);
end;

Function TSedCOM.SelectSector(sc:integer):integer;
begin
 Result:=Nmask and JedMain.scsel.AddSC(sc);
end;

Function TSedCOM.SelectSurface(sc,sf:integer):integer;
begin
 Result:=Nmask and JedMain.sfsel.AddSF(sc,sf);
end;

Function TSedCOM.SelectEdge(sc,sf,ed:integer):integer;
begin
 Result:=Nmask and JedMain.edsel.AddED(sc,sf,ed);
end;

Function TSedCOM.SelectVertex(sc,vx:integer):integer;
begin
 Result:=Nmask and JedMain.vxsel.AddVX(sc,vx);
end;

Function TSedCOM.SelectThing(th:integer):integer;
begin
 Result:=Nmask and JedMain.thsel.AddTH(th);
end;

Function TSedCOM.SelectFrame(th,fr:integer):integer;
begin
 Result:=Nmask and JedMain.frsel.AddFR(th,fr);
end;

Function TSedCOM.SelectLight(lt:integer):integer;
begin
 Result:=Nmask and JedMain.ltsel.AddLT(lt);
end;

Function TSedCOM.FindSelectedSector(sc:integer):integer;
begin
 Result:=JedMain.scsel.FindSC(sc);
end;

Function TSedCOM.FindSelectedSurface(sc,sf:integer):integer;
begin
 Result:=JedMain.sfsel.FindSF(sc,sf);
end;

Function TSedCOM.FindSelectedEdge(sc,sf,ed:integer):integer;
begin
 Result:=JedMain.edsel.FindED(sc,sf,ed);
end;

Function TSedCOM.FindSelectedVertex(sc,vx:integer):integer;
begin
 Result:=JedMain.vxsel.FindVX(sc,vx);
end;

Function TSedCOM.FindSelectedThing(th:integer):integer;
begin
 Result:=JedMain.thsel.FindTH(th);
end;

Function TSedCOM.FindSelectedFrame(th,fr:integer):integer;
begin
 Result:=JedMain.frsel.FindFR(th,fr);
end;

Function TSedCOM.FindSelectedLight(lt:integer):integer;
begin
 Result:=JedMain.ltsel.FindLT(lt);
end;

Procedure TSedCOM.SaveAsSed(const filename: PChar);
begin
 JedMain.SaveToFile(filename, 'jed');
end;

Procedure TSedCOM.Save(const filename: PChar);
begin
 JedMain.SaveJKLto(filename);
end;

Procedure TSedCOM.UpdateMap;
begin
 JedMain.Invalidate;
end;

Procedure TSedCOM.SetPickerCmp(const cmp: PChar);
begin
 ResPicker.SetCMP(cmp);
end;

Function TSedCOM.PickResource(what:integer; const cur: PChar): PChar;
const resstr: AnsiString='';
begin
SaveCurApp;
case what of
 pr_thing: resstr:=ResPicker.PickThing(cur);
 pr_template: resstr:=ResPicker.PickTemplate(cur);
 pr_cmp: resstr:=ResPicker.PickCMP(cur);
 pr_secsound: resstr:=ResPicker.PickSecSound(cur);
 pr_mat: resstr:=ResPicker.PickMAT(cur);
 pr_cog: resstr:=ResPicker.PickCOG(cur);
 pr_layer: resstr:=ResPicker.PickLayer(cur);
 pr_3do: resstr:=ResPicker.Pick3DO(cur);
 pr_ai: resstr:=ResPicker.PickAI(cur);
 pr_key: resstr:=ResPicker.PickKEY(cur);
 pr_snd: resstr:=ResPicker.PickSND(cur);
 pr_pup: resstr:=ResPicker.PickPUP(cur);
 pr_spr: resstr:=ResPicker.PickSPR(cur);
 pr_par: resstr:=ResPicker.PickPAR(cur);
 pr_per: resstr:=ResPicker.PickPER(cur);
 pr_jklsmksan: resstr:=ResPicker.PickEpSeq(cur);
else resstr:=cur;
end;
 result:= PChar(resstr);
 RestoreCurApp;
end;

Function TSedCOM.EditFlags(what:integer;flags:LongInt):LongInt;
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

Procedure TSedCOM.ReloadTemplates;
begin
 JedMain.LoadTemplates;
end;

Procedure TSedCOM.PanMessage(mtype: integer; const msg: PChar);
begin
  case mtype of
    sed_plugins.msg_info: misc_utils.PanMessage(mt_info, msg);
    sed_plugins.msg_warning: misc_utils.PanMessage(mt_warning, msg);
    sed_plugins.msg_error: misc_utils.PanMessage(mt_error, msg);
  end;
end;

Procedure TSedCOM.SendKey(shift:integer;key:integer);
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

Function TSedCOM.ExecuteMenu(const itemref: PChar):boolean;
var nmi,mi:TMenuItem;
    cname:string;
    p,cpos:integer;

  Function GetNextItem(var s: string; pos :integer):integer;
  begin
   Result:=pos;
   While result<length(ItemRef) do
    if ItemRef[result]<>'\' then inc(result) else break;

   if result=length(itemRef) then  s:=Copy(itemref,pos,Length(ItemRef))
   else s:=Copy(itemref,pos,result-pos);

   inc(result);

  end;

var i:integer;s:string;
begin
 Result:=false;
 mi:=nil;
 cpos:=getNextItem(cname,1);
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
  cpos:=GetNextItem(cname,cpos);
  nmi:=nil;

  for i:=0 to mi.Count-1 do
  begin
   s:=mi.Items[i].Caption;
   if Pos(#9,s)<>0 then SetLength(s,Pos(#9,s)-1);

   While pos('&',s)<>0 do Delete(s,pos('&',s),1);

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
 Result:=true;
end;

Function TSedCOM.GetSedSetting(const name: PChar): variant;
begin
 Result := GetSetting(name); // TODO: verify, might breake plugin due to returned string
end;

Function TSedCOM.IsLayerVisible(n: integer):boolean;
begin
 Result:=ToolBar.IsLayerVisible(n);
end;

Procedure TSedCOM.CheckConsistencyErrors;
begin
 Consistency.Check;
 Consistency.Hide;
end;

Procedure TSedCOM.CheckResources;
begin
 Consistency.CheckResources;
 Consistency.Hide;
end;

Function TSedCOM.GetNumConsistencyErrors:integer;
begin
 result:=Consistency.NErrors;
end;

Function TSedCOM.GetConsistencyErrorString(n:integer): PChar;

begin
 result:= PChar(Consistency.ErrorText(n));
end;

Function TSedCOM.GetConsistencyErrorType(n:integer):integer;
var obj:TConsistencyError;
begin
 result:=-1;
 obj:=Consistency.ErrorObject(n);
 if obj=nil then exit;
 result:=integer(obj.etype);
end;

Function TSedCOM.GetConsErrorItemType(n:integer):integer;
var obj:TConsistencyError;
begin
 result:=-1;
 obj:=Consistency.ErrorObject(n);
 if obj=nil then exit;
 result:=integer(obj.itype);
end;

Function TSedCOM.GetConsistencyErrorSector(n:integer):integer;
var obj:TConsistencyError;
begin
 result:=-1;
 obj:=Consistency.ErrorObject(n);
 if obj.itype=itSector then Result:=TJKSector(obj.item).num
 else if obj.itype=itSurface then Result:=TJKSurface(obj.item).sector.num;
end;

Function TSedCOM.GetConsistencyErrorSurface(n:integer):integer;
var obj:TConsistencyError;
begin
 result:=-1;
 obj:=Consistency.ErrorObject(n);
 if obj.itype=itSurface then Result:=TJKSurface(obj.item).num;
end;

Function TSedCOM.GetConsistencyErrorThing(n:integer):integer;
var obj:TConsistencyError;
begin
 result:=-1;
 obj:=Consistency.ErrorObject(n);
 if obj.itype=itThing then Result:=TJKThing(obj.item).num;
end;

Function TSedCOM.GetConsistencyErrorCOG(n:integer):integer;
var obj:TConsistencyError;
begin
 result:=-1;
 if n >= Consistency.NErrors then exit;
 obj:=Consistency.ErrorObject(n);
 if obj.itype=itCog then Result:=J_Level.Level.COGS.IndexOf(obj.item);
end;

Function TSedCOM.IsPreviewActive:boolean;
begin
 Result:=Preview3D.IsActive;
end;

Function TSedCOM.GetSedString(what: integer): PChar;
  begin
    case what of
      SS_ProjectDir:  result := PChar(ProjectDir);
      SS_SEDDir:      result := PChar(BaseDir);
      SS_GameDir:     result := PChar(GameDir);
      SS_LevelFile:   result := PChar(JedMain.LevelFile);
      SS_ConfigFile:  result := PChar(GetConfigFilePath());
      SS_LECLogoJK:   result := PChar(LECLogoJK);
      SS_Recent1:     result := PChar(Recent1);
      SS_Recent2:     result := PChar(Recent2);
      SS_Recent3:     result := PChar(Recent3);
      SS_Recent4:     result := PChar(Recent4);
      SS_Res1gob:     result := PChar(Res1_Gob);
      SS_Res2gob:     result := PChar(Res2_Gob);
      SS_SPGOB:       result := PChar(sp_Gob);
      SS_MP1GOB:      result := PChar(mp1_Gob);
      SS_MP2GOB:      result := PChar(mp2_Gob);
      SS_MP3GOB:      result := PChar(mp3_Gob);
      SS_LECLogoIJIM: result := PChar(LECLogoIJIM);
  else
    begin
      tmpastr[0] := #0;
       result := @tmpastr[0];
    end;
end;

end;

Function TSedCOM.GetProjectType:integer;
begin
 Result:=integer(GlobalVars.CurrentProject);
end;

Procedure TSedCOM.SetProjectType(kind:integer);
begin
 SetProjectType(kind);
end;

Function TSedCOM.GetSedWindow(whichone:integer):integer;
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

Function TSedCOM.FileExtractExt(const filename: PChar): PChar;
begin
  Result := PChar(ExtractExt(filename));
end;

Function TSedCOM.FileExtractPath(const filename: PChar): PChar;
begin
  Result := PChar(ExtractPath(filename));
end;

Function TSedCOM.FileExtractName(const filename: PChar): PChar;
begin
  Result := PChar(ExtractName(filename));
end;

Function TSedCOM.FindProjectDirFile(const filename: PChar): PChar;
begin
  Result := PChar(FindProjDirFile(filename));
end;

Function TSedCOM.IsFileContainer(const filename: PChar): Boolean;
begin
 result := IsContainer(filename);
end;

Function TSedCOM.IsFileInContainer(const filename: PChar): Boolean;
begin
 result := IsInContainer(filename);
end;

Function TSedCOM.FileOpenDialog(const filename: PChar; filter: PChar): PChar;
begin
  SaveCurApp;
  GetFileOpen.FileName := filename;
  GetFileOpen.Filter := filter;
  if GetFileOpen.Execute then
    begin
        result := PChar(GetFileOpen.FileName)
    end
  else result := nil;
  RestoreCurApp;
end;


Function TSedCOM.OpenFile(const filename: PChar):integer;
begin
try
 result:=files.NewHandle(OpenFileRead(filename,0));
except
 on Exception do result:=-1;
end;
end;

Function TSedCOM.OpenGameFile(const filename: PChar): integer;
begin
try
 result := files.NewHandle(FileOperations.OpenGameFile(filename));
except
 on Exception do result:=-1;
end;
end;

Function TSedCOM.ReadFile(handle:integer;buf:pointer;size:longint):integer;
begin
 result:=TFile(files.GetItemNoNIL(handle)).Fread(buf^,size);
end;

Procedure TSedCOM. SetFilePos(handle:integer;pos:longint);
begin
 TFile(files.GetItemNoNIL(handle)).FSeek(pos);
end;

Function TSedCOM.GetFilePos(handle:integer):longint;
begin
 Result:=TFile(files.GetItemNoNIL(handle)).FPos;
end;

Function TSedCOM.GetFileSize(handle:integer):longint;
begin
 result:=TFile(files.GetItemNoNIL(handle)).FSize;
end;

Function TSedCOM.GetFileName(handle:integer): PChar;
begin
 tmpstr := TFile(files.GetItemNoNIL(handle)).GetFullName;  // GetFullName returns tmp string
 Result := PChar(tmpstr);
end;

Procedure TSedCOM.CloseFile(handle:integer);
begin
 files.FreeHandle(handle);
end;

Function TSedCOM.OpenTextFile(const filename: PChar): integer;
begin
  try
   result := tfiles.NewHandle(TTextFile.CreateRead(OpenFileRead(filename, 0)));
  except
   on Exception do result := -1;
  end;
end;

Function TSedCOM.OpenGameTextFile(const filename: PChar): integer;
begin
  try
    result := tfiles.NewHandle(TTextFile.CreateRead(FileOperations.OpenGameFile(filename)));
  except
   on Exception do result := -1;
  end;
end;

Function TSedCOM.GetTextFileName(handle: integer): PChar;
begin
  tmpstr := TTextFile(tfiles.GetItemNoNIL(handle)).GetFullName;  // GetFullName returns tmp string
  Result := PChar(tmpstr);
end;

Function TSedCOM.ReadTextFileString(handle: integer): PChar;
  var tmpstr: string;
begin
 TTextFile(tfiles.GetItemNoNIL(handle)).Readln(tmpstr);
 result := PChar(tmpstr);
end;

Function TSedCOM.TextFileEOF(handle: integer):boolean;
begin
  Result := TTextFile(tfiles.GetItemNoNIL(handle)).eof;
end;

Function TSedCOM.TextFileCurrentLine(handle:integer): integer;
begin
 result:=TTextFile(tfiles.GetItemNoNIL(handle)).CurrentLine;
end;

Procedure TSedCOM.CloseTextFile(handle: integer);
begin
 tfiles.FreeHandle(handle);
end;

Function TSedCOM.GobOpen(const filename: PChar):integer;
begin
try
 result := conts.NewHandle(OpenContainer(filename));
except
 on Exception do result := -1;
end;
end;

Function TSedCOM.GobNumFiles(handle:integer):integer;
begin
 Result:=TContainerFile(conts.GetItemNoNIL(handle)).Files.Count;
end;

Function TSedCOM.GobGetFilename(handle:integer;n:integer): PChar;
begin
 Result := PChar(TContainerFile(conts.GetItemNoNIL(handle)).Files[n]);
end;

Function TSedCOM.GobGetFullFilename(handle:integer; n:integer): PChar;
var cf:TContainerFile;
begin
 cf:=TContainerFile(conts.GetItemNoNIL(handle));
 tmpstr := cf.Name + '>' + cf.Files[n];
 Result := PChar(tmpstr)
end;

Function TSedCOM.GobGetFileOffset(handle: integer;n:integer): longint;
begin
 result:=TFileInfo(TContainerFile(conts.GetItemNoNIL(handle)).Files.Objects[n]).offs;
end;

Function TSedCOM.GobGetFileSize(handle:integer;n:integer): longint;
begin
 result:=TFileInfo(TContainerFile(conts.GetItemNoNIL(handle)).Files.Objects[n]).size;
end;

Procedure TSedCOM.GobClose(handle:integer);
begin
 conts.FreeHandle(handle);
end;

Function TSedCOM.CreateWFRenderer(wnd:integer;whichone:integer):ISedWFRenderer;
begin
 try
  result:=TCOMWFRenderer.Create(wnd,whichone);
 except
  on Exception do result:=nil;
 end;
end;


{---------------------- WF Renderer -------------------------------}

constructor TCOMWFRenderer.Create(wnd:integer;which:integer);
begin
 case which of
  cr_Default: if WireFrameAPI=WF_OPENGL then which:=cr_OpenGL else which:=cr_Software;
 end;

 case which of
  WF_Software: Rend:=TSFTRenderer.Create(wnd);
  WF_OpenGL: Rend:=TOGLRenderer.Create(wnd);
 else Rend:=TSFTRenderer.Create(wnd);
 end;
end;


function TCOMWFRenderer.QueryInterface(iid: pointer; var obj): LongInt;
begin
 Result:= CLASS_E_CLASSNOTAVAILABLE;
end;

function TCOMWFRenderer.AddRef: Longint;
begin
 Result:=1;
end;

function TCOMWFRenderer.Release: Longint;
begin
 Free();
 Result:=0;
end;

{Renderer attributes}
function TCOMWFRenderer.GetRendererDouble(what:integer):double;
begin
 case what of
  rd_CamX: result:=Rend.CamX;
  rd_CamY: result:=Rend.CamY;
  rd_CamZ: result:=Rend.CamZ;
  rd_Scale: result:=Rend.scale;
  rd_GridX: result:=Rend.GridX;
  rd_GridY: result:=Rend.GridY;
  rd_GridZ: result:=Rend.GridZ;
  rd_GridLine: result:=Rend.GridLine;
  rd_GridDot: result:=Rend.GridDot;
  rd_GridStep: result:=Rend.GridStep;
 else result:=0;
 end;
end;

Procedure TCOMWFRenderer.SetRendererDouble(what:integer;val:double);
begin
 case what of
  rd_CamX: Rend.CamX:=val;
  rd_CamY: Rend.CamY:=val;
  rd_CamZ: Rend.CamZ:=val;
  rd_Scale: Rend.scale:=val;
  rd_GridX: Rend.GridX:=val;
  rd_GridY: Rend.GridY:=val;
  rd_GridZ: Rend.GridZ:=val;
  rd_GridLine: Rend.GridLine:=val;
  rd_GridDot: Rend.GridDot:=val;
  rd_GridStep: Rend.GridStep:=val;
 end;
end;

Procedure TCOMWFRenderer.GetRendererVector(what:integer;var x,y,z:double);
begin
 With Rend do
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

Procedure TCOMWFRenderer.SetRendererVector(what:integer;x,y,z:double);
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

function TCOMWFRenderer.NSelected:integer;
begin
 result:=Rend.Selected.Count;
end;

function TCOMWFRenderer.GetNSelected(n:integer):integer;
begin
 result:=Rend.Selected[n];
end;

Procedure TCOMWFRenderer.SetViewPort(x,y,w,h:integer);
begin
 Rend.SetViewPort(x,y,w,h);
end;

Procedure TCOMWFRenderer.SetColor(what,r,g,b:byte);
begin
 Rend.SetViewPort(what,r,g,b);
end;

Procedure TCOMWFRenderer.SetPointSize(size:double);
begin
 Rend.SetPointSize(size);
end;

Procedure TCOMWFRenderer.BeginScene;
begin
 Rend.BeginScene;
end;

Procedure TCOMWFRenderer.EndScene;
begin
 Rend.EndScene;
end;

Procedure TCOMWFRenderer.SetCulling(how:integer);
begin
 Rend.SetCulling(how);
end;

Procedure TCOMWFRenderer.DrawSector(sc:integer);
begin
 Rend.DrawPolygons(Level.Sectors[sc].Surfaces);
end;

Procedure TCOMWFRenderer.DrawSurface(sc,sf:integer);
begin
 Rend.DrawPolygon(Level.Sectors[sc].Surfaces[sf]);
end;


Procedure TCOMWFRenderer.DrawLine(x1,y1,z1,x2,y2,z2:double);
var v1,v2:TVertex;
begin
 v1:=Tvertex.Create;
 v2:=Tvertex.Create;
 v1.x:=x1; v1.y:=y1; v1.z:=z1;
 v2.x:=x2; v2.y:=y2; v2.z:=z2;
 Rend.DrawLine(v1,v2);
 v1.free;
 v2.free;
end;

Procedure TCOMWFRenderer.DrawVertex(X,Y,Z:double);
begin
 Rend.DrawVertex(x,y,z);
end;

Procedure TCOMWFRenderer.DrawGrid;
begin
 Rend.DrawGrid;
end;

Procedure TCOMWFRenderer. BeginPick(x,y:integer);
begin
 Rend.BeginPick(x,y);
end;

Procedure TCOMWFRenderer. EndPick;
begin
 Rend.EndPick;
end;

Procedure TCOMWFRenderer.PickSector(sc:integer;id:integer);
begin
 Rend.PickPolygons(Level.Sectors[sc].Surfaces,id);
end;

Procedure TCOMWFRenderer. PickSurface(sc,sf:integer;id:integer);
begin
 Rend.PickPolygon(Level.Sectors[sc].Surfaces[sf],id);
end;

Procedure TCOMWFRenderer. PickLine(x1,y1,z1,x2,y2,z2:double;id:integer);
var v1,v2:TVertex;
begin
 v1:=Tvertex.Create;
 v2:=Tvertex.Create;
 v1.x:=x1; v1.y:=y1; v1.z:=z1;
 v2.x:=x2; v2.y:=y2; v2.z:=z2;
 Rend.PickLine(v1,v2,id);
 v1.free;
 v2.free;
end;

Procedure TCOMWFRenderer.PickVertex(X,Y,Z:double;id:integer);
begin
 rend.PickVertex(x,y,z,id);
end;

Procedure TCOMWFRenderer. BeginRectPick(x1,y1,x2,y2:integer);
begin
 Rend.BeginRectPick(x1,y1,x2,y2);
end;

Procedure TCOMWFRenderer.EndRectPick;
begin
 Rend.EndRectPick;
end;

function TCOMWFRenderer.IsSectorInRect(sc:integer):boolean;
begin
 result:=Rend.ArePolygonsInRect(Level.Sectors[sc].Surfaces);
end;

function TCOMWFRenderer.IsSurfaceInRect(sc,sf:integer):boolean;
begin
 result:=Rend.IsPolygonInRect(Level.Sectors[sc].Surfaces[sf]);
end;

function TCOMWFRenderer.IsLineInRect(x1,y1,z1,x2,y2,z2:double):boolean;
var v1,v2:TVertex;
begin
 v1:=Tvertex.Create;
 v2:=Tvertex.Create;
 v1.x:=x1; v1.y:=y1; v1.z:=z1;
 v2.x:=x2; v2.y:=y2; v2.z:=z2;
 result:=Rend.IsLineInRect(v1,v2);
 v1.free;
 v2.free;
end;

function TCOMWFRenderer.IsVertexInRect(X,Y,Z:double):boolean;
begin
 Result:=Rend.IsVertexInRect(x,y,z);
end;

function TCOMWFRenderer.GetXYZonPlaneAt(scX,scY:integer; const pnormal: TSedVector3; pX,pY,pZ:double; var X,Y,Z:double):Boolean;
begin
 result:=Rend.GetXYZonPlaneAt(scX,scY,Tvector(pnormal),pX,pY,pZ,X,Y,Z);
end;

function TCOMWFRenderer.GetGridAt(scX,scY:integer;var X,Y,Z:double):boolean;
begin
 result:=rend.GetGridAt(scX,scY,X,Y,Z);
end;

Procedure TCOMWFRenderer.GetNearestGridNode(iX,iY,iZ:double; Var X,Y,Z:double);
begin
 rend.GetNearestGrid(iX,iY,iZ,X,Y,Z);
end;

Procedure TCOMWFRenderer.ProjectPoint(x,y,z:double; Var WinX,WinY:integer);
begin
 rend.ProjectPoint(x,y,z,WinX,WinY);
end;

Procedure TCOMWFRenderer.UnProjectPoint(WinX,WinY:integer; WinZ:double; var x,y,z:double);
begin
 rend.UnProjectPoint(WinX,WinY,WinZ,x,y,z);
end;

function TCOMWFRenderer.IsSurfaceFacing(sc,sf:integer):boolean;
begin
 result:=rend.IsPolygonFacing(Level.Sectors[sc].Surfaces[sf]);
end;

function TCOMWFRenderer.HandleWMQueryPal:integer;
begin
 Result:=Rend.HandleWMQueryPal;
end;

function TCOMWFRenderer.HandleWMChangePal:integer;
begin
 Result:=Rend.HandleWMChangePal;
end;

end.
