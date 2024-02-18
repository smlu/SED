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


TSEDCOM = class(ISED)

  files,
  tfiles,
  conts: THandles;

  Constructor Create;

  function QueryInterface(iid: pointer; var obj): LongInt; override;
  function AddRef: Longint; override;
  function Release: Longint; override;

  Function GetVersion:double; override;
  Function GetLevel:ISEDLevel;override; {ISEDLevel object}

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
  Procedure LoadLevel(name: PChar); override;

  {Different level editing functions}

  Procedure FindBoundingBox(sec:integer;var box:TSEDBox);override;
  Procedure FindBoundingSphere(sec:integer; var center: TSEDVector3; radius: double);override;
  Function FindCollideBox(sec:integer; const bbox:TSEDBox; center: TSEDVector3;var cbox:TSEDBox):boolean;override;
  Procedure FindSurfaceCenter(sc,sf:integer; var center:TSEDVector3);override;
  Procedure RotateVector(var vec:TSEDVector3; pyr:TSEDVector3); override;
  procedure RotatePoint(p, pivot: TSEDVector3; angle: double; var p2: TSEDVector3) ;override;
  Procedure GetPYR(const x,y,z:TSEDVector3;var pyr:TSEDVector3);override;
  Function IsSurfaceConvex(sc,sf:integer):boolean;override;
  Function IsSurfacePlanar(sc,sf:integer):boolean;override;
  Function IsSectorConvex(sec:integer):boolean;override;
  Function IsInSector(sec: Integer; p: TSEDVector3):boolean;override;
  Function DoSectorsOverlap(sec1,sec2:integer):boolean;override;
  Function IsPointOnSurface(sec, surf: Integer; p: TSEDVector3):boolean;override;
  Function FindSectorForThing(th:integer):Integer;override;
  Function FindSectorForXYZ(X,Y,Z:double):Integer;override;
  Function ExtrudeSurface(sc,sf:integer; by:double):integer;override;
  Function CleaveSurface(sec, surf: Integer; const cnormal: TSEDVector3; cp: TSEDVector3):integer;override;
  Function CleaveSector(sec: Integer; const cnormal: TSEDVector3; cp: TSEDVector3):integer;override;
  Function CleaveEdge(sec, surf, ed: Integer; const cnormal: TSEDVector3; cp: TSEDVector3):boolean;override;
  Function JoinSurfaces(sc1,sf1,sc2,sf2:Integer):boolean;override;
  Function PlanarizeSurface(sc,sf:integer):boolean;override;
  function MergeSurfaces(sc,sf1,sf2:integer):integer;override;
  function MergeSectors(sec1,sec2:integer):integer;override;
  Procedure CalculateDefaultUVNormals(sc,sf:integer; orgvx:integer; var un,vn:TSEDVector3);override;
  Procedure CalculateUVNormals(sc,sf:integer; var un,vn:TSEDVector3);override;
  Procedure ArrangeTexture(sc,sf:integer; orgvx:integer; const un,vn:TSEDVector3);override;
  Procedure ArrangeTextureBy(sec, surf: Integer; const un, vn: TSEDVector3; refp: TSEDVector3; refu, refv: double);override;
  Function IsTextureFlipped(sc,sf:integer):boolean;override;
  Procedure RemoveSurfaceReferences(sc,sf:integer);override;
  Procedure RemoveSectorReferences(sec:integer;surfs:boolean);override;
  Function StitchSurfaces(sc1,sf1,sc2,sf2:integer):boolean;override;
  Function FindCommonEdges(sc1,sf1,sc2,sf2:integer; var v11,v12,v21,v22:integer):boolean;override;
  Function DoSurfacesOverlap(sc1,sf1,sc2,sf2:integer):boolean;override;
  Function MakeAdjoin(sc,sf:integer):boolean;override;
  Function MakeAdjoinFromSectorUp(sc,sf:integer;firstsc:integer):boolean;override;
  Function UnAdjoin(sc,sf:integer):Boolean;override;
  Function CreateCubicSector(p: TSEDVector3; const pnormal, edge: TSEDVector3):integer;override;

  Procedure StartUndo(name:PChar);override;
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

  Procedure SaveSED(name:PChar);override;
  Procedure Save(name:PChar);override;
  Procedure UpdateMap;override;
  Procedure SetPickerCMP(cmp:PChar);override;
  Function PickResource(what:integer;cur:PChar):PChar;override;
  Function EditFlags(what:integer;flags:LongInt):LongInt;override;

  Procedure ReloadTemplates;override;
  Procedure PanMessage(mtype:integer;msg:PChar);override;
  Procedure SendKey(shift:integer;key:integer);override;
  Function ExecuteMenu(itemref:PChar):boolean;override;
  Function GetSEDSetting(name:PChar):variant;override;
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
  Function GetSEDString(what:integer):PChar;override;

  Function GetProjectType:integer;override;                // SED 0.1.0
  Procedure SetProjectType(kind:integer);override;         // SED 0.1.0

  {JED 0.93}
  Function GetSEDWindow(whichone:integer):integer;override;

  Function FileExtractExt(path:PChar):PChar;override;
  Function FileExtractPath(path:PChar):PChar;override;
  Function FileExtractName(path:PChar):PChar;override;
  Function FindProjectDirFile(name:PChar):PChar;override;
  Function IsFileContainer(path:PChar):boolean;override;
  Function IsFileInContainer(path:PChar):Boolean;override;

  Function FileOpenDialog(fname:PChar;filter:PChar):PChar;override;
  Function OpenFile(name:PChar):integer;override;
  Function OpenGameFile(name:PChar):integer;override;
  Function ReadFile(handle:integer;buf:pointer;size:longint):integer;override;
  Procedure SetFilePos(handle:integer;pos:longint);override;
  Function GetFilePos(handle:integer):longint;override;
  Function GetFileSize(handle:integer):longint;override;
  Function GetFileName(handle:integer):PChar;override;
  Procedure CloseFile(handle:integer);override;

  Function OpenTextFile(name:PChar):integer;override;
  Function OpenGameTextFile(name:PChar):integer;override;
  Function GetTextFileName(handle:integer):PChar;override;
  Function ReadTextFileString(handle:integer):PChar;override;
  Function TextFileEOF(handle:integer):boolean;override;
  Function TextFileCurrentLine(handle:integer):integer;override;
  Procedure CloseTextFile(handle:integer);override;

  Function GOBOpen(name:PChar):integer;override;
  Function GOBNumFiles(handle:integer):integer;override;
  Function GOBGetFilename(handle:integer;n:integer):PChar;override;
  Function GOBGetFullFilename(handle:integer;n:integer):PChar;override;
  Function GOBGetFileSize(handle:integer;n:integer):longint;override;
  Function GOBGetFileOffset(handle:integer;n:integer):longint;override;
  Procedure GOBClose(handle:integer);override;

  Function CreateWFRenderer(wnd:integer;whichone:integer):ISEDWFRenderer;override;

end;

 TSEDCOMLevel=class(ISEDLevel)
  function QueryInterface(iid: pointer; var obj): LongInt; override;
  function AddRef: Longint; override;
  function Release: Longint; override;

  Procedure GetLevelHeader(var lh:TSEDLevelHeader;rflags:integer);override;
  Procedure SetLevelHeader(const lh:TSEDLevelHeader;rflags:integer);override;

  Function NumSectors :integer; override;
  Function NumThings: integer; override;
  Function NumLights: integer; override;
  Function NumCOGs: integer; override;

  {Sectors}
  Function SectorAdd: integer; override;
  Procedure SectorDelete(n: integer); override;

  Procedure SectorGet(sec:integer; var rec: TSEDSectorRec; what: integer); override;
  Procedure SectorSet(sec:integer; const rec: TSEDSectorRec; what: integer); override;

  Function SectorNumVertices(sec: integer): integer; override;
  Function SectorNumSurfaces(sec: integer): integer; override;

  Procedure SectorGetVertex(sec, vn: Integer; var vert: TSEDVector3); override;
  Procedure SectorSetVertex(sec, vn: Integer; vert: TSEDVector3); override;

  Function SectorAddVertex(sec: Integer; vert: TSEDVector3):integer; override;
  Function SectorFindVertex(sec: Integer; vert: TSEDVector3):integer; override;
  Function SectorDeleteVertex(sec: Integer; n:Integer): Integer; override;

  Function SectorAddSurface(sec: integer): integer; override;
  Procedure SectorDeleteSurface(sc, sf: integer); override;
  Procedure SectorUpdate(sec: integer); override;

  {Surfaces}
  Procedure SurfaceGet(sc,sf: integer; var rec: TSEDSurfaceRec; rflags: integer); override;
  Procedure SurfaceSet(sc, sf: integer; const rec: TSEDSurfaceRec; rflags: integer); override;
  Procedure SurfaceGetNormal(sc, sf: integer; var n: TSEDVector3); override;
  Procedure SurfaceUpdate(sc, sf: integer; how: integer); override;
  Function SurfaceNumVertices(sc, sf: integer): Integer; override;
  Function SurfaceGetVertexNum(sc, sf, vx: integer): integer; override;
  Procedure SurfaceSetVertexNum(sc, sf, vx: integer; secvx: integer); override;
  Function SurfaceAddVertex(sc, sf: integer; secvx: integer): Integer; override;
  Function SurfaceInsertVertex(sc, sf: integer; at: integer; secvx: integer): Integer; override;
  Procedure SurfaceDeleteVertex(sc, sf: integer; n: integer); override;
  Procedure SurfaceGetVertexUV(sc, sf, vx: integer; var u,v: single); override;
  Procedure SurfaceSetVertexUV(sc, sf, vx: integer; u,v: single); override;
  Procedure SurfaceGetVertexLight(sc, sf, vx: integer; var color: TSEDColor); override;
  Procedure SurfaceSetVertexLight(sc, sf, vx: integer; const color: TSEDColor); override;

  {Things}
  Function ThingAdd: Integer; override;
  Procedure ThingDelete(th: integer); override;
  Procedure ThingGet(th: integer; var rec: TSEDThingRec; rflags: integer); override;
  Procedure ThingSet(th: integer; const rec: TSEDThingRec; rflags: integer); override;
  Procedure ThingUpdate(th: integer); override;

  {Lights}
  Function LightAdd:Integer; override;
  Procedure LightDelete(lt:integer); override;
  Procedure LightGet(lt:integer; var rec: TSEDLightRec; rflags: integer);override;
  Procedure LightSet(lt:integer; const rec: TSEDLightRec; rflags: integer);override;
  Procedure LightUpdate(lt:integer); override;

  {Layers}
  Function NumLayers: integer; override;
  Function LayerGetName(n: integer): PChar; override;
  Function LayerAdd(name: PChar): integer; override;

  {JED 0.92}
  Function NumThingValues(th:integer):integer;override;
  Function ThingValueGetName(th,n:Integer):PChar;override;
  Function ThingValueGetData(th,n:integer):PChar;override;
  Procedure ThingValueSetData(th,n:Integer;val:PChar);override;

  Procedure ThingFrameGet(th,n: Integer; var pos, rot: TSEDVector3);override;
  Procedure ThingFrameSet(th,n: Integer; pos, rot: TSEDVector3);override;

  Function ThingValueAdd(th:integer;name,val:PChar):integer;override;
  Procedure ThingValueInsert(th,n:Integer;name,val:PChar);override;
  Procedure ThingValueDelete(th,n:integer);override;

  {COGs}
  Function COGAdd(name:PChar):Integer;override;
  Procedure COGDelete(n:integer);override;
  Procedure COGUpdate(cg:integer);override;

  Function COGGetFilename(cg:integer):PChar;override;
  Function NumCOGValues(cg:integer):integer;override;

  Function COGValueGetName(cg,n:Integer):PChar;override;
  Function COGValueGetType(cg,n:Integer):integer;override;

  Function COGValueGet(cg,n:Integer):PChar;override;
  Function COGValueSet(cg,n:Integer;val:PChar):boolean;override;

  Function COGValueAdd(cg:integer;name,val:PChar;vtype:integer):integer;override;
  Procedure COGValueInsert(cg,n:Integer;name,val:PChar;vtype:integer);override;
  Procedure COGValueDelete(cg,n:integer);override;

 end;

 TCOMWFRenderer = class(ISEDWFRenderer)
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

  Function GetXYZonPlaneAt(scX,scY:integer;pnormal:TSEDVector3; pX,pY,pZ:double; var X,Y,Z:double):Boolean;override;
  Function GetGridAt(scX,scY:integer;var X,Y,Z:double):boolean;override;
  Procedure GetNearestGridNode(iX,iY,iZ:double; Var X,Y,Z:double);override;
  Procedure ProjectPoint(x,y,z:double; Var WinX,WinY:integer);override;
  Procedure UnProjectPoint(WinX,WinY:integer; WinZ:double; var x,y,z:double);override;
  Function IsSurfaceFacing(sc,sf:integer):boolean;override;

  Function HandleWMQueryPal:integer;override;
  Function HandleWMChangePal:integer;override;

 end;


Function GetSEDCOM: ISED;

implementation
uses  Item_edit, u_MsgForm, u_Tools, JED_Main,lev_utils,JED_OLE, listRes,
      u_CogForm, FlagEditor, U_SCFEdit,U_Options, u_tbar, Cons_Checker, u_Preview,
      Files,FileOperations,FileDialogs;

var com: ISED;
    comlevel: TSEDCOMLevel;
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

Function GetSEDCOM: ISED;
begin
 if com=nil then com:=TSEDCOM.Create;
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


Constructor TSEDCOM.Create;
begin
 files:=THandles.Create('file');
 tfiles:=THandles.Create('text file');
 conts:=THandles.Create('GOB');
end;


function TSEDCOM.QueryInterface(iid: pointer; var obj): LongInt;
begin
 Result:= CLASS_E_CLASSNOTAVAILABLE;
end;

function TSEDCOM.AddRef: Longint;
begin
 Result:=1;
end;

function TSEDCOM.Release: Longint;
begin
 Result:=0;
end;

Function TSEDCOM.GetVersion:double;
begin
 ValDouble(SedVerNum,Result);
 Result:=result*100;
end;

Function TSEDCOM.GetLevel: ISEDLevel; {ISEDLevel object}
begin
 if comlevel=nil then comlevel:=TSEDCOMLevel.Create;
 result:=comLevel;
end;

{Level}

function TSEDCOMLevel.QueryInterface(iid: pointer; var obj): LongInt;
begin
 Result:= CLASS_E_CLASSNOTAVAILABLE;
end;

function TSEDCOMLevel.AddRef: Longint;
begin
 result:=1;
end;

function TSEDCOMLevel.Release: Longint;
begin
 Result:=0;
end;

Procedure TSEDCOMLevel.SectorGet(sec:integer;var rec:TSEDSectorRec;what:integer);
begin
  with level.sectors[sec] do
    begin
      if (what and s_flags)      <> 0 then rec.flags       := flags;
      if (what and s_ambient)    <> 0 then rec.ambient     := TSEDColor(ambient);
      if (what and s_extra)      <> 0 then rec.extraLight  := TSEDColor(extraLight);
      if (what and s_pointlight) <> 0 then rec.pointLight  := TSEDPointLight(pointLight);
      if (what and s_cmp)        <> 0 then rec.colorMap    := PChar(colormap);
      if (what and s_tint)       <> 0 then rec.tint        := TSEDColor(tint);
      if (what and s_sound)      <> 0 then rec.sound       := PChar(sound);
      if (what and s_sndvol)     <> 0 then rec.soundVolume := soundVolume;
      if (what and s_thrust)     <> 0 then rec.thrust      := TSEDVector3(thrust);
      if (what and s_layer)      <> 0 then rec.layer       := layer;
    end;
end;

Procedure TSEDCOMLevel.SectorSet(sec:integer;const rec:TSEDSectorRec;what:integer);
begin
  with level.sectors[sec] do
    begin
      if (what and s_flags)      <> 0 then flags       := rec.flags;
      if (what and s_ambient)    <> 0 then ambient     := TColorF(rec.Ambient);
      if (what and s_extra)      <> 0 then extraLight  := TColorF(rec.ExtraLight);
      if (what and s_pointlight) <> 0 then pointLight  := TPointLight(rec.pointLight);
      if (what and s_cmp)        <> 0 then colormap    := rec.ColorMap;
      if (what and s_tint)       <> 0 then tint        := TColorF(rec.Tint);
      if (what and s_sound)      <> 0 then sound       := rec.Sound;
      if (what and s_sndvol)     <> 0 then soundVolume := rec.soundVolume;
      if (what and s_thrust)     <> 0 then thrust      := TVector(rec.thrust);
      if (what and s_layer)      <> 0 then layer       := rec.layer;
    end;
end;

Procedure TSEDCOMLevel.GetLevelHeader(var lh:TSEDLevelHeader;rflags:integer);
begin
 with level.header do
 begin
  if (rflags and lh_version)<>0 then lh.version:=version;
  if (rflags and lh_gravity)<>0 then lh.Gravity:=gravity;
  if (rflags and lh_SkyZ)<>0 then lh.CeilingSkyZ:=CeilingSkyZ;
  if (rflags and lh_CSkyOffs)<>0 then
  begin
   lh.CeilingSkyOffs[0]:=CeilingSkyOffs[1];
   lh.CeilingSkyOffs[1]:=CeilingSkyOffs[2];
  end;
  if (rflags and lh_HorDist)<>0 then lh.HorDistance:=HorDistance;
  if (rflags and lh_HorPPR)<>0 then lh.HorPixelsPerRev:=HorPixelsPerRev;
  if (rflags and lh_HSkyOffs)<>0 then
  begin
   lh.HorSkyOffs[0]:=HorSkyOffs[1];
   lh.HorSkyOffs[1]:=HorSkyOffs[2];
  end;
  if (rflags and lh_MipMapDist)<>0 then
  begin
   lh.MipMapDist[0]:=MipMapDist[1];
   lh.MipMapDist[1]:=MipMapDist[2];
   lh.MipMapDist[2]:=MipMapDist[3];
   lh.MipMapDist[3]:=MipMapDist[4];
  end;
  if (rflags and lh_LODDist)<>0 then
  begin
   lh.LODDist[0] := LODDist[1];
   lh.LODDist[1] := LODDist[2];
   lh.LODDist[2] := LODDist[3];
   lh.LODDist[3] := LODDist[4];
  end;
  if (rflags and lh_PerspDist)<>0 then lh.PerspDist:=PerspDist;
  if (rflags and lh_GouraudDist)<>0 then lh.GouraudDist:=GouraudDist;
  if (rflags and lh_ppu)<>0 then lh.PixelPerUnit:=Level.PPUnit;
  if (rflags and lh_MasterCMP)<>0 then lh.MasterCMP := PChar(Level.MasterCMP);
  if (rflags and lh_Fog) <> 0 then lh.Fog := TSEDFog(Fog);

 end;
end;

Procedure TSEDCOMLevel.SetLevelHeader(const lh:TSEDLevelHeader;rflags:integer);
begin
 with level.header do
 begin
  if (rflags and lh_version)<>0 then version:=lh.version;
  if (rflags and lh_gravity)<>0 then Gravity:=lh.gravity;
  if (rflags and lh_SkyZ)<>0 then CeilingSkyZ:=lh.CeilingSkyZ;
  if (rflags and lh_CSkyOffs)<>0 then
  begin
   CeilingSkyOffs[1]:=lh.CeilingSkyOffs[0];
   CeilingSkyOffs[2]:=lh.CeilingSkyOffs[1];
  end;
  if (rflags and lh_HorDist)<>0 then HorDistance:=lh.HorDistance;
  if (rflags and lh_HorPPR)<>0 then HorPixelsPerRev:=lh.HorPixelsPerRev;
  if (rflags and lh_HSkyOffs)<>0 then
  begin
   HorSkyOffs[1]:=lh.HorSkyOffs[0];
   HorSkyOffs[2]:=lh.HorSkyOffs[1];
  end;
  if (rflags and lh_MipMapDist)<>0 then
  begin
   MipMapDist[1]:=lh.MipMapDist[0];
   MipMapDist[2]:=lh.MipMapDist[1];
   MipMapDist[3]:=lh.MipMapDist[2];
   MipMapDist[4]:=lh.MipMapDist[3];
  end;
  if (rflags and lh_LODDist)<>0 then
  begin
   LODDist[1] := lh.LODDist[0];
   LODDist[2] := lh.LODDist[1];
   LODDist[3] := lh.LODDist[2];
   LODDist[4] := lh.LODDist[3];
  end;
  if (rflags and lh_PerspDist)<>0 then PerspDist:=lh.PerspDist;
  if (rflags and lh_GouraudDist)<>0 then GouraudDist:=lh.GouraudDist;
  if (rflags and lh_ppu)<>0 then Level.ppunit:=lh.PixelPerUnit;
  if (rflags and lh_MasterCMP)<>0 then level.MasterCMP:=lh.MasterCMP;
  if (rflags and lh_Fog) <> 0 then Fog := TFog(lh.Fog);

 end;
end;

Function TSEDCOMLevel.NumSectors:integer;
begin
 result:=level.sectors.count;
end;

Function TSEDCOMLevel.NumThings:integer;
begin
 result:=level.things.count;
end;

Function TSEDCOMLevel.NumLights:integer;
begin
 result:=level.lights.count;
end;

Function TSEDCOMLevel.NumCOGs:integer;
begin
 result:=level.Cogs.Count;
end;

{Sectors}
Function TSEDCOMLevel.SectorAdd:integer;
var sec:TJKSector;
begin
 sec:=Level.NewSector;
 Result:=Level.Sectors.Add(sec);
 Level.RenumSecs;
 JedMain.SectorAdded(sec);
end;

Procedure TSEDCOMLevel.SectorDelete(n:integer);
begin
 Lev_Utils.DeleteSector(Level,n);
end;

Function TSEDCOMLevel.SectorNumVertices(sec:integer):integer;
begin
 result:=Level.Sectors[sec].vertices.count;
end;

Function TSEDCOMLevel.SectorNumSurfaces(sec:integer):integer;
begin
 result:=Level.Sectors[sec].surfaces.count;
end;

Procedure TSEDCOMLevel.SectorGetVertex(sec, vn: Integer; var vert: TSEDVector3);
var v: TJKVertex;
begin
  v := Level.Sectors[sec].Vertices[vn];
  vert.x := v.x;
  vert.y := v.y;
  vert.z := v.z;
end;

Procedure TSEDCOMLevel.SectorSetVertex(sec, vn: Integer; vert: TSEDVector3);
var v: TJKVertex;
begin
  v := Level.Sectors[sec].Vertices[vn];
  v.x := vert.x;
  v.y := vert.y;
  v.z := vert.z;
end;

Function TSEDCOMLevel.SectorAddVertex(sec: Integer; vert: TSEDVector3):integer;
var v: TJKVertex;
begin
  v := Level.Sectors[sec].NewVertex;
  v.x := vert.x;
  v.y := vert.y;
  v.z := vert.z;
  result := level.sectors[sec].vertices.count - 1;
end;

Function TSEDCOMLevel.SectorFindVertex(sec: Integer; vert: TSEDVector3):integer;
begin
 result := Level.Sectors[sec].FindVX(vert.x, vert.y, vert.z);
end;

Function TSEDCOMLevel.SectorDeleteVertex(sec:integer;n:integer):integer;
begin
 With Level.Sectors[sec] do
 begin
  vertices[n].Free;
  vertices.Delete(n);
  renumber;
 end;
end;

Function TSEDCOMLevel.SectorAddSurface(sec:integer):integer;
begin
 With level.sectors[sec] do
 begin
  Result:=surfaces.add(newSurface);
 end;
end;

Procedure TSEDCOMLevel.SectorDeleteSurface(sc,sf:integer);
begin
 With level.sectors[sc] do
 begin
  RemoveSurfRefs(level,surfaces[sf]);
  surfaces[sf].Free;
  surfaces.Delete(sf);
  Renumber;
 end;
end;

Procedure TSEDCOMLevel.SectorUpdate(sec:integer);
var asec:TJKSector;
begin
 asec:=Level.Sectors[sec];
 asec.Renumber;
 JedMain.SectorChanged(asec);
end;

  {Surfaces}
Procedure TSEDCOMLevel.SurfaceGet(sc,sf:integer;var rec:TSEDSurfaceRec;rflags:integer);
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
      if (rflags and sf_Material)    <> 0 then rec.material    := PChar(material);
      if (rflags and sf_geo)         <> 0 then rec.geo         := geo;
      if (rflags and sf_light)       <> 0 then rec.light       := light;
      if (rflags and sf_tex)         <> 0 then rec.tex         := tex;
      if (rflags and sf_ExtraLight)  <> 0 then rec.ExtraLight  := TSEDColor(ExtraLight);
      if (rflags and sf_txscale)     <> 0 then
      begin
        rec.uscale:=uscale;
        rec.vscale:=vscale;
      end;
    end;
end;

Procedure TSEDCOMLevel.SurfaceSet(sc,sf:integer;const rec:TSEDSurfaceRec;rflags:integer);
begin
  with level.sectors[sc].surfaces[sf] do
    begin
      if (rflags and sf_adjoin) <> 0 then
      begin
        if rec.adjoinSec < 0 then adjoin := nil else
        adjoin := Level.Sectors[rec.adjoinSec].Surfaces[rec.adjoinSurf];
      end;

      if (rflags and sf_adjoinflags) <> 0 then AdjoinFlags := rec.adjoinflags;
      if (rflags and sf_SurfFlags)   <> 0 then SurfFlags   := rec.surfflags;
      if (rflags and sf_FaceFlags)   <> 0 then FaceFlags   := rec.faceflags;
      if (rflags and sf_Material)    <> 0 then Material    := rec.material;
      if (rflags and sf_geo)         <> 0 then geo         := rec.geo;
      if (rflags and sf_light)       <> 0 then light       := rec.light;
      if (rflags and sf_tex)         <> 0 then tex         := rec.tex;
      if (rflags and sf_ExtraLight)  <> 0 then ExtraLight  := TColorF(rec.ExtraLight);
      if (rflags and sf_txscale)     <> 0 then
      begin
        uscale := rec.uscale;
        vscale := rec.vscale;
      end;
    end;
end;

Procedure TSEDCOMLevel.SurfaceGetNormal(sc,sf:integer;var n:TSEDVector3);
begin
 With Level.Sectors[sc].surfaces[sf].normal do
 begin
  n.dx:=dx;
  n.dy:=dy;
  n.dz:=dz;
 end;
end;

Procedure TSEDCOMLevel.SurfaceUpdate(sc,sf:integer;how:integer);
var surf:TJKSurface;
begin
 surf:=Level.Sectors[sc].Surfaces[sf];
 if how and su_texture<>0 then surf.RecalcAll else surf.Recalc;
 if how and su_floorflag<>0 then surf.CheckIfFloor;
 if how and su_sector<>0 then JedMain.SectorChanged(surf.sector);
end;

Function TSEDCOMLevel.SurfaceNumVertices(sc,sf:integer):Integer;
begin
 Result:=Level.Sectors[sc].surfaces[sf].vertices.count;
end;

Function TSEDCOMLevel.SurfaceGetVertexNum(sc,sf,vx:integer):integer;
begin
 Result:=Level.Sectors[sc].surfaces[sf].vertices[vx].num;
end;

Procedure TSEDCOMLevel.SurfaceSetVertexNum(sc,sf,vx:integer;secvx:integer);
begin
 With Level.Sectors[sc] do
 begin
  surfaces[sf].vertices[vx]:=vertices[secvx];
 end;
end;

Function TSEDCOMLevel.SurfaceAddVertex(sc,sf:integer;secvx:integer):Integer;
begin
 With Level.Sectors[sc] do
 begin
  result:=surfaces[sf].AddVertex(vertices[secvx]);
 end;
end;

Function TSEDCOMLevel.SurfaceInsertVertex(sc,sf:integer;at:integer;secvx:integer):Integer;
begin
 With Level.Sectors[sc] do
 begin
  result:=surfaces[sf].InsertVertex(at,vertices[secvx]);
 end;
 result:=at;
end;

Procedure TSEDCOMLevel.SurfaceDeleteVertex(sc,sf:integer;n:integer);
begin
 Level.Sectors[sc].surfaces[sf].DeleteVertex(n);
end;

Procedure TSEDCOMLevel.SurfaceGetVertexUV(sc,sf,vx:integer;var u,v:single);
var tv:TTXVertex;
begin
 tv:=Level.Sectors[sc].surfaces[sf].txvertices[vx];
 u:=tv.u;
 v:=tv.v;
end;

Procedure TSEDCOMLevel.SurfaceSetVertexUV(sc,sf,vx:integer;u,v:single);
var tv:TTXVertex;
begin
 tv:=Level.Sectors[sc].surfaces[sf].txvertices[vx];
 tv.u:=u;
 tv.v:=v;
end;

Procedure TSEDCOMLevel.SurfaceGetVertexLight(sc, sf, vx: integer; var color: TSEDColor);
var tv:TTXVertex;
begin
 tv := Level.Sectors[sc].surfaces[sf].txvertices[vx];
 color := TSEDColor(tv.color);
end;

Procedure TSEDCOMLevel.SurfaceSetVertexLight(sc, sf, vx: integer; const color: TSEDColor);
var tv:TTXVertex;
begin
 tv:=Level.Sectors[sc].surfaces[sf].txvertices[vx];
 tv.color := TColorF(color);
end;

{Things}

Function TSEDCOMLevel.ThingAdd:Integer;
var th:TJKThing;
begin
 th:=Level.NewThing;
 Result:=Level.Things.Add(th);
 Level.RenumThings;
 JedMain.ThingAdded(th);
end;

Procedure TSEDCOMLevel.ThingDelete(th:integer);
begin
 Lev_Utils.DeleteThing(Level,th);
end;

Procedure TSEDCOMLevel.ThingGet(th:integer;var rec:TSEDThingRec;rflags:integer);
begin
 with level.things[th] do
 begin
  if (rflags and th_name)<>0 then rec.name:=PChar(name);
  if (rflags and th_sector)<>0 then
  begin
   if sec=nil then rec.Sector:=-1
   else rec.Sector:=sec.num;
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

Procedure TSEDCOMLevel.ThingSet(th:integer;const rec:TSEDThingRec;rflags:integer);
begin
 with level.things[th] do
 begin
  if (rflags and th_name)<>0 then name:=rec.name;
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

Procedure TSEDCOMLevel.ThingUpdate(th:integer);
begin
 Level.RenumThings;
 JedMain.ThingChanged(level.Things[th]);
end;

{Lights}

Function TSEDCOMLevel.LightAdd: Integer;
var lt: TSedLight;
begin
  lt := Level.NewLight;
  Result := Level.lights.Add(lt);
  JedMain.LightAdded(lt);
end;

Procedure TSEDCOMLevel.LightDelete(lt: integer);
begin
  Lev_Utils.DeleteLight(Level, lt);
end;

Procedure TSEDCOMLevel.LightGet(lt: integer; var rec: TSEDLightRec; rflags: Integer);
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

Procedure TSEDCOMLevel.LightSet(lt:integer;const rec:TSEDLightRec;rflags:integer);
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

Procedure TSEDCOMLevel.LightUpdate(lt:integer);
begin;
  JedMain.LightChanged(level.Lights[lt]);
end;

Function TSEDCOMLevel.NumLayers:integer;
begin
  Result:=Level.Layers.count;
end;

Function TSEDCOMLevel.LayerGetName(n:integer): PChar;
begin
  Result := PChar(Level.GetLayerName(n));
end;

Function TSEDCOMLevel.LayerAdd(name: PChar):integer;
begin
  Result := Level.AddLayer(name);
end;

{0.92}
Function TSEDCOMLevel.NumThingValues(th:integer):integer;
begin
  result:=Level.Things[th].Vals.count;
end;

Function TSEDCOMLevel.ThingValueGetName(th,n:Integer): PChar;
begin
  result := PChar(Level.Things[th].Vals[n].Name);
end;

Function TSEDCOMLevel.ThingValueGetData(th,n: integer): PChar;
begin
  result := PChar(Level.Things[th].Vals[n].AsString);
end;

Procedure TSEDCOMLevel.ThingValueSetData(th,n: Integer; val: PChar);
var v: TTPLValue;
begin
  v := Level.Things[th].Vals[n];
  S2TPLVal(v.name + '=' + val,v);
end;

Procedure TSEDCOMLevel.ThingFrameGet(th, n: Integer; var pos, rot: TSEDVector3);
begin
  Level.Things[th].Vals[n].GetFrame(pos.x, pos.y, pos.z, rot.pitch, rot.yaw, rot.roll);
end;

Procedure TSEDCOMLevel.ThingFrameSet(th,n: Integer; pos, rot: TSEDVector3);
begin
  Level.Things[th].Vals[n].SetFrame(pos.x, pos.y, pos.z, rot.pitch, rot.yaw, rot.roll);
end;


Function TSEDCOMLevel.ThingValueAdd(th: integer; name,val: PChar): integer;
var v:TTPLValue;
begin
  v:=TTplValue.Create;
  S2TPLVal(name + '=' + val, v);
  Result:=Level.Things[th].Vals.Add(v);
end;

Procedure TSEDCOMLevel.ThingValueInsert(th,n: Integer; name,val: PChar);
var v:TTPLValue;
begin
  v := TTplValue.Create;
  S2TPLVal(name + '=' + val, v);
  Level.Things[th].Vals.Insert(n, v);
end;

Procedure TSEDCOMLevel.ThingValueDelete(th,n: integer);
var v:TTPLValue;
begin
 v:=Level.Things[th].Vals[n];
 Level.Things[th].Vals.Delete(n);
 v.free;
end;


{COGs}
Function TSEDCOMLevel.COGAdd(name: PChar): Integer;
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

Procedure TSEDCOMLevel.COGDelete(n: integer);
var cog:TCOG;
begin
 lev_utils.DeleteCOG(level,n);
end;

Procedure TSEDCOMLevel.COGUpdate(cg: integer);
begin
 COgForm.UpdateCOG(cg);
end;

Function TSEDCOMLevel.COGGetFilename(cg: integer): PChar;
begin
 Result := PChar(Level.COGS[cg].name);
end;

Function TSEDCOMLevel.NumCOGValues(cg: integer): integer;
begin
 Result:=Level.COGS[cg].vals.count;
end;

Function TSEDCOMLevel.COGValueGetName(cg,n: Integer): PChar;
begin
 Result := PChar(Level.COGS[cg].vals[n].Name);
end;

Function TSEDCOMLevel.COGValueGetType(cg,n:Integer): integer;
begin
 Result := Integer(Level.COGS[cg].vals[n].cog_type);
end;

Function TSEDCOMLevel.COGValueGet(cg,n:Integer): PChar;
begin
 Result := PChar(Level.COGS[cg].vals[n].AsString);
end;

Function TSEDCOMLevel.COGValueSet(cg,n: Integer; val: PChar): Boolean;
begin
 Result:=Level.cogs[cg].Vals[n].JedVal(val);
end;

Function TSEDCOMLevel.COGValueAdd(cg:integer; name,val: PChar; vtype:integer): integer;
var v:TCOGValue;
begin
 v:=TCogValue.Create;
 v.name:=name;
 v.vtype:=GetVTypeFromCOGType(TCOG_TYpe(vtype));
 v.cog_type:=TCOG_Type(vtype);
 v.Val(val);
 result:=Level.cogs[cg].Vals.Add(v);
end;

Procedure TSEDCOMLevel.COGValueInsert(cg,n:Integer; name,val: PChar; vtype:integer);
var v:TCOGValue;
begin
 v:=TCogValue.Create;
 v.name:=name;
 v.vtype:=GetVTypeFromCOGType(TCOG_TYpe(vtype));
 v.cog_type:=TCOG_Type(vtype);
 v.Val(val);
 Level.cogs[cg].Vals.Insert(n,v);
end;

Procedure TSEDCOMLevel.COGValueDelete(cg,n: integer);
var v:TCOGValue;
begin
 v:=Level.Cogs[cg].Vals[n];
 Level.Cogs[cg].Vals.Delete(n);
 v.free;
end;


{SEDCOM}

Function TSEDCOM.GetMapMode:Integer;
begin
 result:=JedMain.mapMode;
end;

Procedure TSEDCOM.SetMapMode(mode:integer);
begin
 JedMain.SetMapMode(Mode);
end;

Function TSEDCOM.GetCurrentSector:integer;
begin
 Result:=JedMain.Cur_SC;
end;

Procedure TSEDCOM.SetCurrentSector(sc:integer);
begin
 JedMain.SetCurSC(sc);
end;

Function TSEDCOM.GetCurrentThing:integer;
begin
 Result:=JedMain.Cur_TH;
end;

Procedure TSEDCOM.SetCurrentThing(th:integer);
begin
 JedMain.SetCurTH(th);
end;

Function TSEDCOM.GetCurrentLight:integer;
begin
 Result:=JedMain.Cur_LT;
end;

Procedure TSEDCOM.SetCurrentLight(lt:integer);
begin
 JedMain.SetCurLT(lt);
end;

Procedure TSEDCOM.GetCurrentVertex(var sc,vx:integer);
begin
 sc:=JedMain.Cur_SC;
 vx:=JedMain.Cur_VX;
end;

Procedure TSEDCOM.SetCurrentVertex(sc,vx:integer);
begin
 JedMain.SetCurVX(sc,vx);
end;

Procedure TSEDCOM.GetCurrentSurface(var sc,sf:integer);
begin
 sc:=JedMain.Cur_SC;
 sf:=JedMain.Cur_SF;
end;

Procedure TSEDCOM.SetCurrentSurface(sc,sf:integer);
begin
 JedMain.SetCurSF(sc,sf);
end;

Procedure TSEDCOM.GetCurrentEdge(var sc,sf,ed:integer);
begin
 sc:=JedMain.Cur_SC;
 sf:=JedMain.Cur_SF;
 ed:=JedMain.Cur_ED;
end;

Procedure TSEDCOM.SetCurrentEdge(sc,sf,ed:integer);
begin
 JedMain.SetCurED(sc,sf,ed);
end;

Procedure TSEDCOM.GetCurrentFrame(var th,fr:integer);
begin
 th:=JedMain.Cur_TH;
 fr:=JedMain.Cur_FR;
end;

Procedure TSEDCOM.SetCurrentFrame(th,fr:integer);
begin
 JedMain.SetCurFR(th,fr);
end;

Procedure TSEDCOM.NewLevel(kind: integer);
begin
 case TProjectType(kind) of
  TProjectType.JKDF2: JedMain.New1.Click;
  TProjectType.MOTS: JedMain.NewMotsProject1.Click;
  else
    JedMain.NewIJIMProject.Click;
 end;
end;

Procedure TSEDCOM.LoadLevel(name: PChar);
begin
 JedMain.OpenProject(name, op_open);
end;

{Different level editing functions}

Procedure TSEDCOM.FindBoundingBox(sec:integer;var box:TSEDBox);
begin
 lev_utils.FindBBox(Level.Sectors[sec],TBox(box));
end;

Procedure TSEDCOM.FindBoundingSphere(sec:integer; var center: TSEDVector3; radius: double);
begin
 lev_utils.FindBSphere(level.Sectors[sec], center.x, center.y, center.z, radius);
end;

Function TSEDCOM.FindCollideBox(sec:integer; const bbox:TSEDBox; center: TSEDVector3;var cbox:TSEDBox):boolean;
begin
 Result:=lev_utils.FindCollideBox(
  level.Sectors[sec],TBox(bbox),center.x, center.y,center.z,Tbox(cbox));
end;

Procedure TSEDCOM.FindSurfaceCenter(sc,sf:integer; var center:TSEDVector3);
begin
 lev_utils.CalcSurfCenter(level.sectors[sc].surfaces[sf], center.x, center.y, center.z);
end;

Procedure TSEDCOM.RotateVector(var vec:TSEDVector3; pyr:TSEDVector3);
begin
 lev_utils.RotateVector(TVector(vec), pyr.pitch, pyr.yaw, pyr.roll);
end;

Procedure TSEDCOM.RotatePoint(p, pivot: TSEDVector3; angle: double; var p2: TSEDVector3);
begin
 lev_utils.RotatePoint(p.x,p.y,p.z, pivot.x,pivot.y,pivot.z, angle, p2.x,p2.y,p2.z);
end;

Procedure TSEDCOM.GetPYR(const x,y,z:TSEDVector3;var pyr:TSEDVector3);
begin
 lev_utils.GetJKPYR(Tvector(x),Tvector(y),Tvector(z), pyr.pitch, pyr.yaw, pyr.roll);
end;

Function TSEDCOM.IsSurfaceConvex(sc,sf:integer):boolean;
begin
 result:=lev_utils.IsSurfConvex(Level.Sectors[sc].Surfaces[sf]);
end;

Function TSEDCOM.IsSurfacePlanar(sc,sf:integer):boolean;
begin
 result:=lev_utils.IsSurfPlanar(Level.Sectors[sc].Surfaces[sf]);
end;

Function TSEDCOM.IsSectorConvex(sec:integer):boolean;
begin
 result:=lev_utils.IsSectorConvex(Level.Sectors[sec]);
end;

Function TSEDCOM.IsInSector(sec: Integer; p: TSEDVector3):boolean;
begin
 Result:=lev_utils.IsInSector(Level.Sectors[sec], p.x, p.y, p.z);
end;

Function TSEDCOM.DoSectorsOverlap(sec1,sec2:integer):boolean;
begin
 result:=lev_utils.DoSectorsOverlap(Level.Sectors[sec1],Level.Sectors[sec2]);
end;

Function TSEDCOM.IsPointOnSurface(sec, surf: Integer; p: TSEDVector3):boolean;
begin
 result:=lev_utils.IsPointOnSurface(Level.Sectors[sec].Surfaces[surf], p.x, p.y, p.z);
end;

Function TSEDCOM.FindSectorForThing(th:integer):Integer;
begin
 result:=lev_utils.FindSectorForThing(Level.Things[th]);
end;

Function TSEDCOM.FindSectorForXYZ(X,Y,Z:double):integer;
begin
 result:=lev_utils.FindSectorForXYZ(level,x,y,z);
end;

Function TSEDCOM.ExtrudeSurface(sc,sf:integer; by:double):integer;
begin
 lev_utils.ExtrudeSurface(Level.Sectors[sc].Surfaces[sf],by);
 result:=level.Sectors.count-1;
end;

Function TSEDCOM.CleaveSurface(sec, surf: Integer; const cnormal: TSEDVector3; cp: TSEDVector3):integer;
begin
 if lev_utils.CleaveSurface(Level.Sectors[sec].Surfaces[surf], Tvector(Cnormal), cp.x, cp.y, cp.z) then
 result := Level.Sectors[sec].Surfaces.count - 1 else result := -1;
end;

Function TSEDCOM.CleaveSector(sec: Integer; const cnormal: TSEDVector3; cp: TSEDVector3):integer;
begin
 if lev_utils.CleaveSector(Level.Sectors[sec],Tvector(Cnormal), cp.x, cp.y, cp.z) then
 result := Level.Sectors.count - 1 else result := -1;
end;

Function TSEDCOM.CleaveEdge(sec, surf, ed: Integer; const cnormal: TSEDVector3; cp: TSEDVector3):boolean;
begin
 result:=lev_utils.CleaveEdge(Level.Sectors[sec].Surfaces[surf],ed,Tvector(Cnormal), cp.x, cp.y, cp.z);
end;

Function TSEDCOM.JoinSurfaces(sc1,sf1,sc2,sf2:Integer):boolean;
begin
 result:=lev_utils.ConnectSurfaces(Level.Sectors[sc1].Surfaces[sf1],
                              Level.Sectors[sc2].Surfaces[sf2]);
end;

Function TSEDCOM.PlanarizeSurface(sc,sf:integer):boolean;
begin
 result:=lev_utils.FlattenSurface(Level.Sectors[sc].Surfaces[sf]);
end;

Function TSEDCOM.MergeSurfaces(sc,sf1,sf2:integer):integer;
var surf:TJKSurface;
begin
 surf:=lev_utils.MergeSurfaces(Level.Sectors[sc].Surfaces[sf1],
                              Level.Sectors[sc].Surfaces[sf2]);
 if surf=nil then result:=-1 else result:=surf.num;
end;

Function TSEDCOM.MergeSectors(sec1,sec2:integer):integer;
var sec:TJKSector;
begin
 sec:=lev_utils.MergeSectors(Level.Sectors[sec1],
                              Level.Sectors[sec2]);
 if sec=nil then result:=-1 else result:=sec.num;
end;

Procedure TSEDCOM.CalculateDefaultUVNormals(sc,sf:integer; orgvx:integer; var un,vn:TSEDVector3);
begin
 lev_utils.CalcDefaultUVNormals(Level.Sectors[sc].Surfaces[sf],
                                     orgvx,Tvector(un),Tvector(vn));
end;

Procedure TSEDCOM.CalculateUVNormals(sc,sf:integer; var un,vn:TSEDVector3);
begin
 lev_utils.CalcUVNormals(Level.Sectors[sc].Surfaces[sf],
                                     Tvector(un),Tvector(vn));
end;

Procedure TSEDCOM.ArrangeTexture(sc,sf:integer; orgvx:integer; const un,vn:TSEDVector3);
begin
 lev_utils.ArrangeTexture(Level.Sectors[sc].Surfaces[sf],
                                     orgvx,Tvector(un),Tvector(vn));
end;

Procedure TSEDCOM.ArrangeTextureBy(sec, surf: Integer; const un, vn: TSEDVector3; refp: TSEDVector3; refu, refv: double);
begin
  lev_utils.ArrangeTextureBy(Level.Sectors[sec].Surfaces[surf],
    Tvector(un),Tvector(vn), refp.x, refp.y, refp.z, refu, refv);
end;

Function TSEDCOM.IsTextureFlipped(sc,sf:integer):boolean;
begin
 result:=lev_utils.IsTXFlipped(Level.Sectors[sc].surfaces[sf]);
end;

Procedure TSEDCOM.RemoveSurfaceReferences(sc,sf:integer);
begin
 lev_utils.RemoveSurfRefs(level,Level.Sectors[sc].surfaces[sf]);
end;

Procedure TSEDCOM.RemoveSectorReferences(sec:integer;surfs:boolean);
begin
 if surfs then lev_utils.RemoveSecRefs(level,Level.Sectors[sec],rs_surfs)
 else lev_utils.RemoveSecRefs(level,Level.Sectors[sec],0);
end;

Function TSEDCOM.StitchSurfaces(sc1,sf1,sc2,sf2:integer):boolean;
begin
 Result:=lev_utils.StitchSurfaces(Level.Sectors[sc1].surfaces[sf1],
                                 Level.Sectors[sc2].surfaces[sf2]);
end;

Function TSEDCOM.FindCommonEdges(sc1,sf1,sc2,sf2:integer; var v11,v12,v21,v22:integer):boolean;
begin
 Result:=lev_utils.FindCommonEdges(Level.Sectors[sc1].surfaces[sf1],
                                 Level.Sectors[sc2].surfaces[sf2],v11,v12,v21,v22);
end;

Function TSEDCOM.DoSurfacesOverlap(sc1,sf1,sc2,sf2:integer):boolean;
begin
 Result:=lev_utils.Do_Surf_Overlap(Level.Sectors[sc1].surfaces[sf1],
                                 Level.Sectors[sc2].surfaces[sf2]);
end;

Function TSEDCOM.MakeAdjoin(sc,sf:integer):boolean;
begin
 Result:=lev_utils.MakeAdjoin(Level.Sectors[sc].surfaces[sf]);
end;

Function TSEDCOM.MakeAdjoinFromSectorUp(sc,sf:integer;firstsc:integer):boolean;
begin
 Result:=lev_utils.MakeAdjoinSCUp(Level.Sectors[sc].surfaces[sf],
                                      firstsc);
end;

Function TSEDCOM.UnAdjoin(sc,sf:integer):Boolean;
begin
 Result:=lev_utils.UnAdjoin(Level.Sectors[sc].surfaces[sf]);
end;

Function TSEDCOM.CreateCubicSector(p: TSEDVector3; const pnormal, edge: TSEDVector3):integer;
begin
 lev_utils.CreateCube(level, p.x, p.y, p.z, Tvector(pnormal), TVector(edge));
 result := Level.sectors.count - 1;
end;

Procedure TSEDCOM.StartUndo(name: PChar);
begin
 StartUndoRec(name);
end;

Procedure TSEDCOM.SaveUndoForThing(n:integer;change:integer);
begin
 case change of
  0: SaveThingUndo(Level.Things[n],ch_changed);
  1: SaveThingUndo(Level.Things[n],ch_added);
  2: SaveThingUndo(Level.Things[n],ch_deleted);
 end;
end;

Procedure TSEDCOM.SaveUndoForLight(n:integer;change:integer);
begin
 case change of
  0: SaveLightUndo(Level.Lights[n],ch_changed);
  1: SaveLightUndo(Level.Lights[n],ch_added);
  2: SaveLightUndo(Level.Lights[n],ch_deleted);
 end;
end;

Procedure TSEDCOM.SaveUndoForSector(n:integer;change:integer;whatpart:integer);
var how:integer;
begin
how:=12;
if whatPart=1 then how:=8;
if whatpart=2 then how:=4;
if whatpart=3 then how:=12;

 case change of
  0: SaveSecUndo(Level.Sectors[n],ch_changed,how);
  1: SaveSecUndo(Level.Sectors[n],ch_added,how);
  2: SaveSecUndo(Level.Sectors[n],ch_deleted,how);
 end;
end;

Procedure TSEDCOM.ClearUndoBuffer;
begin
 u_undo.ClearUndoBuffer;
end;

Procedure TSEDCOM.ApplyUndo;
begin
 u_undo.ApplyUndo;
end;

{0.92}

Function TSEDCOM.GetApplicationHandle:Integer;
begin
 result:=Application.Handle;
end;

Function TSEDCOM.JoinSectors(sec1,sec2:integer):boolean;
begin
 result:=lev_utils.ConnectSectors(Level.Sectors[sec1],level.Sectors[sec2]);
end;

Function TSEDCOM.GetNumMultiselected(what:integer):integer;
begin
 With JedMain do
 case what of
  MM_SC: result:=scsel.count;
  MM_SF: result:=sfsel.count;
  MM_ED: result:=edsel.count;
  MM_VX: result:=vxsel.count;
  MM_TH: result:=thsel.count;
  MM_FR: result:=frsel.count;
  MM_LT: result:=ltsel.count;
 end;
end;

Procedure TSEDCOM.ClearMultiselection(what:integer);
begin
 With JedMain do
 case what of
  MM_SC: scsel.clear;
  MM_SF: sfsel.clear;
  MM_ED: edsel.clear;
  MM_VX: vxsel.clear;
  MM_TH: thsel.clear;
  MM_FR: frsel.clear;
  MM_LT: ltsel.clear;
 end;
 JedMain.Invalidate;
end;

Procedure TSEDCOM.RemoveFromMultiselection(what,n:integer);
begin
 With JedMain do
 case what of
  MM_SC: scsel.DeleteN(n);
  MM_SF: sfsel.DeleteN(n);
  MM_ED: edsel.DeleteN(n);
  MM_VX: vxsel.DeleteN(n);
  MM_TH: thsel.DeleteN(n);
  MM_FR: frsel.DeleteN(n);
  MM_LT: ltsel.DeleteN(n);
 end;
 JedMain.Invalidate;
end;

Function TSEDCOM.GetSelectedSector(n:integer):integer;
begin
 Result:=JedMain.scsel.GetSC(n);
end;

Function TSEDCOM.GetSelectedThing(n:integer):integer;
begin
 Result:=JedMain.thsel.GetTH(n);
end;

Function TSEDCOM.GetSelectedLight(n:integer):integer;
begin
 Result:=JedMain.ltsel.GetLT(n);
end;

Procedure TSEDCOM.GetSelectedSurface(n:integer;var sc,sf:integer);
begin
 JedMain.sfsel.GetSCSF(n,sc,sf);
end;

Procedure TSEDCOM.GetSelectedEdge(n:integer;var sc,sf,ed:integer);
begin
 JedMain.edsel.GetSCSFED(n,sc,sf,ed);
end;

Procedure TSEDCOM.GetSelectedVertex(n:integer;var sc,vx:integer);
begin
 JedMain.vxsel.GetSCVX(n,sc,vx);
end;

Procedure TSEDCOM.GetSelectedFrame(n:integer;var th,fr:integer);
begin
 JedMain.frsel.GetTHFR(n,th,fr);
end;

Function TSEDCOM.SelectSector(sc:integer):integer;
begin
 Result:=Nmask and JedMain.scsel.AddSC(sc);
end;

Function TSEDCOM.SelectSurface(sc,sf:integer):integer;
begin
 Result:=Nmask and JedMain.sfsel.AddSF(sc,sf);
end;

Function TSEDCOM.SelectEdge(sc,sf,ed:integer):integer;
begin
 Result:=Nmask and JedMain.edsel.AddED(sc,sf,ed);
end;

Function TSEDCOM.SelectVertex(sc,vx:integer):integer;
begin
 Result:=Nmask and JedMain.vxsel.AddVX(sc,vx);
end;

Function TSEDCOM.SelectThing(th:integer):integer;
begin
 Result:=Nmask and JedMain.thsel.AddTH(th);
end;

Function TSEDCOM.SelectFrame(th,fr:integer):integer;
begin
 Result:=Nmask and JedMain.frsel.AddFR(th,fr);
end;

Function TSEDCOM.SelectLight(lt:integer):integer;
begin
 Result:=Nmask and JedMain.ltsel.AddLT(lt);
end;

Function TSEDCOM.FindSelectedSector(sc:integer):integer;
begin
 Result:=JedMain.scsel.FindSC(sc);
end;

Function TSEDCOM.FindSelectedSurface(sc,sf:integer):integer;
begin
 Result:=JedMain.sfsel.FindSF(sc,sf);
end;

Function TSEDCOM.FindSelectedEdge(sc,sf,ed:integer):integer;
begin
 Result:=JedMain.edsel.FindED(sc,sf,ed);
end;

Function TSEDCOM.FindSelectedVertex(sc,vx:integer):integer;
begin
 Result:=JedMain.vxsel.FindVX(sc,vx);
end;

Function TSEDCOM.FindSelectedThing(th:integer):integer;
begin
 Result:=JedMain.thsel.FindTH(th);
end;

Function TSEDCOM.FindSelectedFrame(th,fr:integer):integer;
begin
 Result:=JedMain.frsel.FindFR(th,fr);
end;

Function TSEDCOM.FindSelectedLight(lt:integer):integer;
begin
 Result:=JedMain.ltsel.FindLT(lt);
end;

Procedure TSEDCOM.SaveSED(name: PChar);
begin
 JedMain.SaveToFile(name, 'jed');
end;

Procedure TSEDCOM.Save(name: PChar);
begin
 JedMain.SaveJKLto(name);
end;

Procedure TSEDCOM.UpdateMap;
begin
 JedMain.Invalidate;
end;

Procedure TSEDCOM.SetPickerCMP(cmp: PChar);
begin
 ResPicker.SetCMP(cmp);
end;

Function TSEDCOM.PickResource(what:integer;cur: PChar): PChar;
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

Function TSEDCOM.EditFlags(what:integer;flags:LongInt):LongInt;
begin
SaveCurApp;
case what of
 ef_sector: Result:=FlagEdit.EditSectorFlags(flags);
 ef_surface: Result:=FlagEdit.EditSurfaceFlags(flags);
 ef_adjoin: Result:=FlagEdit.EditAdjoinFlags(flags);
 ef_thing: Result:=FlagEdit.EditThingFlags(flags);
 ef_face: Result:=FlagEdit.EditFaceFlags(flags);
 ef_light: Result:=FlagEdit.EditLightFlags(flags);
 ef_geo: Result:=SCFieldPicker.PickGeo(flags);
 ef_lightmode: Result:=SCFieldPicker.PickLightMode(flags);
 ef_tex: Result:=SCFieldPicker.PickTex(flags);
end;
 RestoreCurApp;
end;

Procedure TSEDCOM.ReloadTemplates;
begin
 JedMain.LoadTemplates;
end;

Procedure TSEDCOM.PanMessage(mtype: integer; msg: PChar);
begin
 case mtype of
  0: misc_utils.PanMessage(mt_info,msg);
  1: misc_utils.PanMessage(mt_warning,msg);
  2: misc_utils.PanMessage(mt_error,msg);
 end;
end;

Procedure TSEDCOM.SendKey(shift:integer;key:integer);
var st:TShiftState;
    akey:word;
begin
 st:=[];
 if (shift and 1<>0) then st:=st+[ssCtrl];
 if (shift and 2<>0) then st:=st+[ssShift];
 if (shift and 4<>0) then st:=st+[ssAlt];
 akey:=key;
 JedMain.FormKeyDown(JedMain,akey,st);
end;

Function TSEDCOM.ExecuteMenu(itemref: PChar):boolean;
var nmi,mi:TMenuItem;
    cname:string;
    p,cpos:integer;

  Function GetNextItem(var s:string; pos :integer):integer;
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

Function TSEDCOM.GetSEDSetting(name: PChar): variant;
begin
 Result := GetSetting(name); // TODO: verify, might breake plugin due to returned string
end;

Function TSEDCOM.IsLayerVisible(n: integer):boolean;
begin
 Result:=ToolBar.IsLayerVisible(n);
end;

Procedure TSEDCOM.CheckConsistencyErrors;
begin
 Consistency.Check;
 Consistency.Hide;
end;

Procedure TSEDCOM.CheckResources;
begin
 Consistency.CheckResources;
 Consistency.Hide;
end;

Function TSEDCOM.GetNumConsistencyErrors:integer;
begin
 result:=Consistency.NErrors;
end;

Function TSEDCOM.GetConsistencyErrorString(n:integer): PChar;

begin
 result:= PChar(Consistency.ErrorText(n));
end;

Function TSEDCOM.GetConsistencyErrorType(n:integer):integer;
var obj:TConsistencyError;
begin
 result:=-1;
 obj:=Consistency.ErrorObject(n);
 if obj=nil then exit;
 result:=integer(obj.etype);
end;

Function TSEDCOM.GetConsErrorItemType(n:integer):integer;
var obj:TConsistencyError;
begin
 result:=-1;
 obj:=Consistency.ErrorObject(n);
 if obj=nil then exit;
 result:=integer(obj.itype);
end;

Function TSEDCOM.GetConsistencyErrorSector(n:integer):integer;
var obj:TConsistencyError;
begin
 result:=-1;
 obj:=Consistency.ErrorObject(n);
 if obj.itype=itSector then Result:=TJKSector(obj.item).num
 else if obj.itype=itSurface then Result:=TJKSurface(obj.item).sector.num;
end;

Function TSEDCOM.GetConsistencyErrorSurface(n:integer):integer;
var obj:TConsistencyError;
begin
 result:=-1;
 obj:=Consistency.ErrorObject(n);
 if obj.itype=itSurface then Result:=TJKSurface(obj.item).num;
end;

Function TSEDCOM.GetConsistencyErrorThing(n:integer):integer;
var obj:TConsistencyError;
begin
 result:=-1;
 obj:=Consistency.ErrorObject(n);
 if obj.itype=itThing then Result:=TJKThing(obj.item).num;
end;

Function TSEDCOM.GetConsistencyErrorCOG(n:integer):integer;
var obj:TConsistencyError;
begin
 result:=-1;
 if n >= Consistency.NErrors then exit;
 obj:=Consistency.ErrorObject(n);
 if obj.itype=itCog then Result:=J_Level.Level.COGS.IndexOf(obj.item);
end;

Function TSEDCOM.IsPreviewActive:boolean;
begin
 Result:=Preview3D.IsActive;
end;

Function TSEDCOM.GetSEDString(what: integer): PChar;
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

Function TSEDCOM.GetProjectType:integer;
begin
 Result:=integer(GlobalVars.CurrentProject);
end;

Procedure TSEDCOM.SetProjectType(kind:integer);
begin
 SetProjectType(kind);
end;

Function TSEDCOM.GetSEDWindow(whichone:integer):integer;
begin
  case whichone of
   sw_Main: result:=JedMain.Handle;
   sw_ConsChecker: Result:=Consistency.Handle;
   sw_ItemEdit: Result:=ItemEdit.Handle;
   sw_PlacedCogs: Result:=CogForm.Handle;
   sw_MsgWindow: result:=MsgForm.Handle;
   sw_3DPreview: result:=Preview3D.Handle;
   sw_ToolWindow: result:=ToolForm.Handle;
  else result:=0;
  end;
end;

Function TSEDCOM.FileExtractExt(path: PChar): PChar;
begin
  Result := PChar(ExtractExt(path));
end;

Function TSEDCOM.FileExtractPath(path: PChar): PChar;
begin
  Result := PChar(ExtractPath(path));
end;

Function TSEDCOM.FileExtractName(path: PChar): PChar;
begin
  Result := PChar(ExtractName(path));
end;

Function TSEDCOM.FindProjectDirFile(name: PChar): PChar;
begin
  Result := PChar(FindProjDirFile(name));
end;

Function TSEDCOM.IsFileContainer(path: PChar): Boolean;
begin
 result := IsContainer(path);
end;

Function TSEDCOM.IsFileInContainer(path: PChar): Boolean;
begin
 result:=IsInContainer(path);
end;

Function TSEDCOM.FileOpenDialog(fname: PChar; filter: PChar): PChar;
begin
  SaveCurApp;
  GetFileOpen.FileName:=fname;
  GetFileOpen.Filter:=filter;
  if GetFileOpen.Execute then
    begin
        result := PChar(GetFileOpen.FileName)
    end
  else result := nil;
  RestoreCurApp;
end;


Function TSEDCOM.OpenFile(name: PChar):integer;
begin
try
 result:=files.NewHandle(OpenFileRead(name,0));
except
 on Exception do result:=-1;
end;
end;

Function TSEDCOM.OpenGameFile(name: PChar): integer;
begin
try
 result := files.NewHandle(FileOperations.OpenGameFile(name));
except
 on Exception do result:=-1;
end;
end;

Function TSEDCOM.ReadFile(handle:integer;buf:pointer;size:longint):integer;
begin
 result:=TFile(files.GetItemNoNIL(handle)).Fread(buf^,size);
end;

Procedure TSEDCOM. SetFilePos(handle:integer;pos:longint);
begin
 TFile(files.GetItemNoNIL(handle)).FSeek(pos);
end;

Function TSEDCOM.GetFilePos(handle:integer):longint;
begin
 Result:=TFile(files.GetItemNoNIL(handle)).FPos;
end;

Function TSEDCOM.GetFileSize(handle:integer):longint;
begin
 result:=TFile(files.GetItemNoNIL(handle)).FSize;
end;

Function TSEDCOM.GetFileName(handle:integer): PChar;
begin
 tmpstr := TFile(files.GetItemNoNIL(handle)).GetFullName;  // GetFullName returns tmp string
 Result := PChar(tmpstr);
end;

Procedure TSEDCOM.CloseFile(handle:integer);
begin
 files.FreeHandle(handle);
end;

Function TSEDCOM.OpenTextFile(name: PChar): integer;
begin
  try
   result := tfiles.NewHandle(TTextFile.CreateRead(OpenFileRead(name,0)));
  except
   on Exception do result := -1;
  end;
end;

Function TSEDCOM.OpenGameTextFile(name: PChar): integer;
begin
  try
    result := tfiles.NewHandle(TTextFile.CreateRead(FileOperations.OpenGameFile(name)));
  except
   on Exception do result := -1;
  end;
end;

Function TSEDCOM.GetTextFileName(handle: integer): PChar;
begin
  tmpstr := TTextFile(tfiles.GetItemNoNIL(handle)).GetFullName;  // GetFullName returns tmp string
  Result := PChar(tmpstr);
end;

Function TSEDCOM.ReadTextFileString(handle: integer): PChar;
  var tmpstr: string;
begin
 TTextFile(tfiles.GetItemNoNIL(handle)).Readln(tmpstr);
 result := PChar(tmpstr);
end;

Function TSEDCOM.TextFileEOF(handle: integer):boolean;
begin
  Result := TTextFile(tfiles.GetItemNoNIL(handle)).eof;
end;

Function TSEDCOM.TextFileCurrentLine(handle:integer): integer;
begin
 result:=TTextFile(tfiles.GetItemNoNIL(handle)).CurrentLine;
end;

Procedure TSEDCOM.CloseTextFile(handle: integer);
begin
 tfiles.FreeHandle(handle);
end;

Function TSEDCOM.GOBOpen(name: PChar):integer;
begin
try
 result := conts.NewHandle(OpenContainer(name));
except
 on Exception do result:=-1;
end;
end;

Function TSEDCOM.GOBNumFiles(handle:integer):integer;
begin
 Result:=TContainerFile(conts.GetItemNoNIL(handle)).Files.Count;
end;

Function TSEDCOM.GOBGetFilename(handle:integer;n:integer): PChar;
begin
 Result := PChar(TContainerFile(conts.GetItemNoNIL(handle)).Files[n]);
end;

Function TSEDCOM.GOBGetFullFilename(handle:integer; n:integer): PChar;
var cf:TContainerFile;
begin
 cf:=TContainerFile(conts.GetItemNoNIL(handle));
 tmpstr := cf.Name + '>' + cf.Files[n];
 Result := PChar(tmpstr)
end;

Function TSEDCOM.GOBGetFileOffset(handle: integer;n:integer): longint;
begin
 result:=TFileInfo(TContainerFile(conts.GetItemNoNIL(handle)).Files.Objects[n]).offs;
end;

Function TSEDCOM.GOBGetFileSize(handle:integer;n:integer): longint;
begin
 result:=TFileInfo(TContainerFile(conts.GetItemNoNIL(handle)).Files.Objects[n]).size;
end;

Procedure TSEDCOM.GOBClose(handle:integer);
begin
 conts.FreeHandle(handle);
end;

Function TSEDCOM.CreateWFRenderer(wnd:integer;whichone:integer):ISEDWFRenderer;
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

function TCOMWFRenderer.GetXYZonPlaneAt(scX,scY:integer;pnormal:TSEDVector3; pX,pY,pZ:double; var X,Y,Z:double):Boolean;
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
