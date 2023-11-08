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
  Function GetCurSC:integer;override;
  Procedure SetCurSC(sc:integer);override;
  Function GetCurTH:integer;override;
  Procedure SetCurTH(th:integer);override;
  Function GetCurLT:integer;override;
  Procedure SetCurLT(lt:integer);override;
  Procedure GetCurVX(var sc,vx:integer);override;
  Procedure SetCurVX(sc,vx:integer);override;
  Procedure GetCurSF(var sc,sf:integer);override;
  Procedure SetCurSF(sc,sf:integer);override;
  Procedure GetCurED(var sc,sf,ed:integer);override;
  Procedure SetCurED(sc,sf,ed:integer);override;
  Procedure GetCurFR(var th,fr:integer);override;
  Procedure SetCurFR(th,fr:integer);override;

  Procedure NewLevel(kind: integer); override;
  Procedure LoadLevel(name: PChar); override;

  {Different level editing functions}

  Procedure FindBBox(sec:integer;var box:TSEDBox);override;
  Procedure FindBoundingSphere(sec:integer; var CX,CY,CZ,Radius:double);override;
  Function FindCollideBox(sec:integer; const bbox:TSEDBox;cx,cy,cz:double;var cbox:TSEDBox):boolean;override;
  Procedure FindSurfaceCenter(sc,sf:integer; var cx,cy,cz:double);override;
  Procedure RotateVector(var vec:TSEDVector; pch,yaw,rol:double);override;
  Procedure RotatePoint(ax1,ay1,az1,ax2,ay2,az2:double;angle:double;var x,y,z:double);override;
  Procedure GetJKPYR(const x,y,z:TSEDVector;var pch,yaw,rol:double);override;
  Function IsSurfaceConvex(sc,sf:integer):boolean;override;
  Function IsSurfacePlanar(sc,sf:integer):boolean;override;
  Function IsSectorConvex(sec:integer):boolean;override;
  Function IsInSector(sec:integer;x,y,z:double):boolean;override;
  Function DoSectorsOverlap(sec1,sec2:integer):boolean;override;
  Function IsPointOnSurface(sc,sf:integer;x,y,z:double):boolean;override;
  Function FindSectorForThing(th:integer):Integer;override;
  Function FindSectorForXYZ(X,Y,Z:double):Integer;override;
  Function ExtrudeSurface(sc,sf:integer; by:double):integer;override;
  Function CleaveSurface(sc,sf:integer; const cnormal:TSEDVector; cx,cy,cz:double):integer;override;
  Function CleaveSector(sec:integer; const cnormal:TSEDVector; cx,cy,cz:double):integer;override;
  Function CleaveEdge(sc,sf,ed:integer; const cnormal:TSEDVector; cx,cy,cz:double):boolean;override;
  Function JoinSurfaces(sc1,sf1,sc2,sf2:Integer):boolean;override;
  Function PlanarizeSurface(sc,sf:integer):boolean;override;
  function MergeSurfaces(sc,sf1,sf2:integer):integer;override;
  function MergeSectors(sec1,sec2:integer):integer;override;
  Procedure CalculateDefaultUVNormals(sc,sf:integer; orgvx:integer; var un,vn:TSEDVector);override;
  Procedure CalcUVNormals(sc,sf:integer; var un,vn:TSEDVector);override;
  Procedure ArrangeTexture(sc,sf:integer; orgvx:integer; const un,vn:TSEDVector);override;
  Procedure ArrangeTextureBy(sc,sf:integer;const un,vn: TSEDVector; refx,refy,refz,refu,refv:double);override;
  Function IsTextureFlipped(sc,sf:integer):boolean;override;
  Procedure RemoveSurfaceReferences(sc,sf:integer);override;
  Procedure RemoveSectorReferences(sec:integer;surfs:boolean);override;
  Function StitchSurfaces(sc1,sf1,sc2,sf2:integer):boolean;override;
  Function FindCommonEdges(sc1,sf1,sc2,sf2:integer; var v11,v12,v21,v22:integer):boolean;override;
  Function DoSurfacesOverlap(sc1,sf1,sc2,sf2:integer):boolean;override;
  Function MakeAdjoin(sc,sf:integer):boolean;override;
  Function MakeAdjoinFromSectorUp(sc,sf:integer;firstsc:integer):boolean;override;
  Function UnAdjoin(sc,sf:integer):Boolean;override;
  Function CreateCubicSector(x,y,z:double;const pnormal,edge:TSEDVector):integer;override;

  Procedure StartUndo(name:PChar);override;
  Procedure SaveUndoForThing(n:integer;change:integer);override;
  Procedure SaveUndoForLight(n:integer;change:integer);override;
  Procedure SaveUndoForSector(n:integer;change:integer;whatpart:integer);override;
  Procedure ClearUndoBuffer;override;
  Procedure ApplyUndo;override;

  {JED 0.92}
  Function GetApplicationHandle:Integer;override;
  Function JoinSectors(sec1,sec2:integer):boolean;override;

  Function GetNMultiselected(what:integer):integer;override;
  Procedure ClearMultiselection(what:integer);override;
  Procedure RemoveFromMultiselection(what,n:integer);override;
  Function GetSelectedSC(n:integer):integer;override;
  Function GetSelectedTH(n:integer):integer;override;
  Function GetSelectedLT(n:integer):integer;override;

  procedure GetSelectedSF(n:integer;var sc,sf:integer);override;
  procedure GetSelectedED(n:integer;var sc,sf,ed:integer);override;
  procedure GetSelectedVX(n:integer;var sc,vx:integer);override;
  procedure GetSelectedFR(n:integer;var th,fr:integer);override;

  Function SelectSC(sc:integer):integer;override;
  Function SelectSF(sc,sf:integer):integer;override;
  Function SelectED(sc,sf,ed:integer):integer;override;
  Function SelectVX(sc,vx:integer):integer;override;
  Function SelectTH(th:integer):integer;override;
  Function SelectFR(th,fr:integer):integer;override;
  Function SelectLT(lt:integer):integer;override;

  Function FindSelectedSC(sc:integer):integer;override;
  Function FindSelectedSF(sc,sf:integer):integer;override;
  Function FindSelectedED(sc,sf,ed:integer):integer;override;
  Function FindSelectedVX(sc,vx:integer):integer;override;
  Function FindSelectedTH(th:integer):integer;override;
  Function FindSelectedFR(th,fr:integer):integer;override;
  Function FindSelectedLT(lt:integer):integer;override;

  Procedure SaveSED(name:PChar);override;
  Procedure SaveJKL(name:PChar);override;
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
  Function NConsistencyErrors:integer;override;
  Function GetConsErrorString(n:integer):PChar;override;
  Function GetConsErrorType(n:integer):integer;override;
  Function GetConsErrorItemType(n: integer):integer;virtual;stdcall; // SED 0.1.0
  Function GetConsErrorSector(n:integer):integer;override;
  Function GetConsErrorSurface(n:integer):integer;override;
  Function GetConsErrorThing(n:integer):integer;override;
  Function GetConsErrorCog(n:integer):integer;override;
  Function IsPreviewActive:boolean;override;
  Function GetSEDString(what:integer):PChar;override;

  Function GetProjectType:integer;override;                // SED 0.1.0
  Procedure SetProjectType(kind:integer);override;         // SED 0.1.0

  {JED 0.93}
  Function GetJEDWindow(whichone:integer):integer;override;

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
  Function TextFileEof(handle:integer):boolean;override;
  Function TextFileCurrentLine(handle:integer):integer;override;
  Procedure CloseTextFile(handle:integer);override;

  Function OpenGOB(name:PChar):integer;override;
  Function GOBNFiles(handle:integer):integer;override;
  Function GOBNFileName(handle:integer;n:integer):PChar;override;
  Function GOBNFullFileName(handle:integer;n:integer):PChar;override;
  Function GOBGetFileSize(handle:integer;n:integer):longint;override;
  Function GOBGetFileOffset(handle:integer;n:integer):longint;override;
  Procedure CloseGOB(handle:integer);override;

  Function CreateWFRenderer(wnd:integer;whichone:integer):ISEDWFRenderer;override;

end;

 TSEDCOMLevel=class(ISEDLevel)
  function QueryInterface(iid: pointer; var obj): LongInt; override;
  function AddRef: Longint; override;
  function Release: Longint; override;

  Procedure GetLevelHeader(var lh:TSEDLevelHeader;rflags:integer);override;
  Procedure SetLevelHeader(const lh:TSEDLevelHeader;rflags:integer);override;

  Function NSectors:integer;override;
  Function NThings:integer;override;
  Function NLights:integer;override;
  Function NCOgs:integer;override;

  {Sectors}
  Function AddSector:integer;override;
  Procedure DeleteSector(n:integer);override;

  Procedure GetSector(sec:integer;var rec:TSEDSectorRec;what:integer);override;
  Procedure SetSector(sec:integer;const rec:TSEDSectorRec;what:integer);override;

  Function SectorNVertices(sec:integer):integer;override;
  Function SectorNSurfaces(sec:integer):integer;override;

  Procedure SectorGetVertex(sec,vx:integer;var x,y,z:double);override;
  Procedure SectorSetVertex(sec,vx:integer;x,y,z:double);override;

  Function SectorAddVertex(sec:integer;x,y,z:double):integer;override;
  Function SectorFindVertex(sec:integer;x,y,z:double):integer;override;
  Function SectorDeleteVertex(sec:integer;n:integer):integer;override;

  Function SectorAddSurface(sec:integer):integer;override;
  Procedure SectorDeleteSurface(sc,sf:integer);override;
  Procedure SectorUpdate(sec:integer);override;

  {Surfaces}
  Procedure GetSurface(sc,sf:integer;var rec:TSEDSurfaceRec;rflags:integer);override;
  Procedure SetSurface(sc,sf:integer;const rec:TSEDSurfaceRec;rflags:integer);override;
  Procedure GetSurfaceNormal(sc,sf:integer;var n:TSEDVector);override;
  Procedure SurfaceUpdate(sc,sf:integer;how:integer);override;
  Function SurfaceNVertices(sc,sf:integer):Integer;override;
  Function SurfaceGetVertexNum(sc,sf,vx:integer):integer;override;
  Procedure SurfaceSetVertexNum(sc,sf,vx:integer;secvx:integer);override;
  Function SurfaceAddVertex(sc,sf:integer;secvx:integer):Integer;override;
  Function SurfaceInsertVertex(sc,sf:integer;at:integer;secvx:integer):Integer;override;
  Procedure SurfaceDeleteVertex(sc,sf:integer;n:integer);override;
  Procedure SurfaceGetVertexUV(sc,sf,vx:integer;var u,v:single);override;
  Procedure SurfaceSetVertexUV(sc,sf,vx:integer;u,v:single);override;
  Procedure SurfaceGetVertexLight(sc, sf, vx: integer; var color: TSEDColorF); override;
  Procedure SurfaceSetVertexLight(sc, sf, vx: integer; const color: TSEDColorF); override;

  {Things}
  Function AddThing:Integer;override;
  Procedure DeleteThing(th:integer);override;
  Procedure GetThing(th:integer;var rec:TSEDThingRec;rflags:integer);override;
  Procedure SetThing(th:integer;const rec:TSEDThingRec;rflags:integer);override;
  Procedure ThingUpdate(th:integer);override;

  {Lights}
  Function AddLight:Integer;override;
  Procedure DeleteLight(lt:integer);override;
  Procedure GetLight(lt:integer;var rec:TSEDLightRec;rflags:integer);override;
  Procedure SetLight(lt:integer;const rec:TSEDLightRec;rflags:integer);override;

  {Layers}
  Function NLayers:integer;override;
  Function GetLayerName(n:integer):PChar;override;
  Function AddLayer(name:PChar):integer;override;

  {JED 0.92}
  Function NTHingValues(th:integer):integer;override;
  Function GetThingValueName(th,n:Integer):PChar;override;
  Function GetThingValueData(th,n:integer):PChar;override;
  Procedure SetThingValueData(th,n:Integer;val:PChar);override;

  Procedure GetThingFrame(th,n:Integer;var x,y,z,pch,yaw,rol:double);override;
  Procedure SetThingFrame(th,n:Integer;x,y,z,pch,yaw,rol:double);override;

  Function AddThingValue(th:integer;name,val:PChar):integer;override;
  Procedure InsertThingValue(th,n:Integer;name,val:PChar);override;
  Procedure DeleteThingValue(th,n:integer);override;

  {COGs}
  Function AddCOG(name:PChar):Integer;override;
  Procedure DeleteCOG(n:integer);override;
  Procedure UpdateCOG(cg:integer);override;

  Function GetCOGFileName(cg:integer):PChar;override;
  Function NCOGValues(cg:integer):integer;override;

  Function GetCOGValueName(cg,n:Integer):PChar;override;
  Function GetCOGValueType(cg,n:Integer):integer;override;

  Function GetCOGValue(cg,n:Integer):PChar;override;
  Function SetCOGValue(cg,n:Integer;val:PChar):boolean;override;

  Function AddCOGValue(cg:integer;name,val:PChar;vtype:integer):integer;override;
  Procedure InsertCOGValue(cg,n:Integer;name,val:PChar;vtype:integer);override;
  Procedure DeleteCOGValue(cg,n:integer);override;

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

  Function GetXYZonPlaneAt(scX,scY:integer;pnormal:TSEDVector; pX,pY,pZ:double; var X,Y,Z:double):Boolean;override;
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

Procedure TSEDCOMLevel.GetSector(sec:integer;var rec:TSEDSectorRec;what:integer);
begin
  with level.sectors[sec] do
    begin
      if (what and s_flags)   <> 0 then rec.Flags      := flags;
      if (what and s_ambient) <> 0 then rec.Ambient    := TSEDColorF(Ambient);
      if (what and s_extra)   <> 0 then rec.ExtraLight := TSEDColorF(ExtraLight);
      if (what and s_cmp)     <> 0 then rec.ColorMap   := PChar(ColorMap);
      if (what and s_tint)    <> 0 then rec.Tint       := TSEDColorF(Tint);
      if (what and s_sound)   <> 0 then rec.Sound      := PChar(Sound);
      if (what and s_sndvol)  <> 0 then rec.snd_vol    := snd_vol;
      if (what and s_layer)   <> 0 then rec.layer      := layer;
    end;
end;

Procedure TSEDCOMLevel.SetSector(sec:integer;const rec:TSEDSectorRec;what:integer);
begin
  with level.sectors[sec] do
    begin
      if (what and s_flags)   <> 0 then Flags      := rec.flags;
      if (what and s_ambient) <> 0 then Ambient    := TColorF(rec.Ambient);
      if (what and s_extra)   <> 0 then ExtraLight := TColorF(rec.ExtraLight);
      if (what and s_cmp)     <> 0 then ColorMap   := rec.ColorMap;
      if (what and s_tint)    <> 0 then Tint       := TColorF(rec.Tint);
      if (what and s_sound)   <> 0 then Sound      := rec.Sound;
      if (what and s_sndvol)  <> 0 then snd_vol    := rec.snd_vol;
      if (what and s_layer)   <> 0 then layer      := rec.layer;
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

Function TSEDCOMLevel.NSectors:integer;
begin
 result:=level.sectors.count;
end;

Function TSEDCOMLevel.NThings:integer;
begin
 result:=level.things.count;
end;

Function TSEDCOMLevel.NLights:integer;
begin
 result:=level.lights.count;
end;

Function TSEDCOMLevel.NCOgs:integer;
begin
 result:=level.Cogs.Count;
end;

{Sectors}
Function TSEDCOMLevel.AddSector:integer;
var sec:TJKSector;
begin
 sec:=Level.NewSector;
 Result:=Level.Sectors.Add(sec);
 Level.RenumSecs;
 JedMain.SectorAdded(sec);
end;

Procedure TSEDCOMLevel.DeleteSector(n:integer);
begin
 Lev_Utils.DeleteSector(Level,n);
end;

Function TSEDCOMLevel.SectorNVertices(sec:integer):integer;
begin
 result:=Level.Sectors[sec].vertices.count;
end;

Function TSEDCOMLevel.SectorNSurfaces(sec:integer):integer;
begin
 result:=Level.Sectors[sec].surfaces.count;
end;

Procedure TSEDCOMLevel.SectorGetVertex(sec,vx:integer;var x,y,z:double);
var v:TJKVertex;
begin
 v:=Level.Sectors[sec].Vertices[vx];
 x:=v.x;
 y:=v.y;
 z:=v.z;
end;

Procedure TSEDCOMLevel.SectorSetVertex(sec,vx:integer;x,y,z:double);
var v:TJKVertex;
begin
 v:=Level.Sectors[sec].Vertices[vx];
 v.x:=x;
 v.y:=y;
 v.z:=z;
end;

Function TSEDCOMLevel.SectorAddVertex(sec:integer;x,y,z:double):integer;
var v:TJKVertex;
begin
 v:=Level.Sectors[sec].NewVertex;
 v.x:=x;
 v.y:=y;
 v.z:=z;
 result:=level.sectors[sec].vertices.count-1;
end;

Function TSEDCOMLevel.SectorFindVertex(sec:integer;x,y,z:double):integer;
begin
 result:=Level.Sectors[sec].FindVX(x,y,z);
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
Procedure TSEDCOMLevel.GetSurface(sc,sf:integer;var rec:TSEDSurfaceRec;rflags:integer);
begin
 with level.sectors[sc].surfaces[sf] do
 begin
  if (rflags and sf_adjoin)<>0 then
  begin
   if adjoin=nil then begin rec.adjoinSC:=-1; rec.AdjoinSF:=-1; end
   else begin rec.adjoinSC:=adjoin.sector.num; rec.AdjoinSF:=adjoin.num; end;
  end;
  if (rflags and sf_adjoinflags)<>0 then rec.AdjoinFlags:=AdjoinFlags;
  if (rflags and sf_SurfFlags)<>0 then rec.SurfFlags:=SurfFlags;
  if (rflags and sf_FaceFlags)<>0 then rec.FaceFlags:=FaceFlags;
  if (rflags and sf_Material)<>0 then rec.Material:=PChar(material);
  if (rflags and sf_geo)<>0 then rec.geo:=geo;
  if (rflags and sf_light)<>0 then rec.light:=light;
  if (rflags and sf_tex)<>0 then rec.tex:=tex;
  if (rflags and sf_ExtraLight)<>0 then rec.ExtraLight:=TSEDColorF(ExtraLight);
  if (rflags and sf_txscale)<>0 then
  begin
   rec.uscale:=uscale;
   rec.vscale:=vscale;
  end;
 end;
end;

Procedure TSEDCOMLevel.SetSurface(sc,sf:integer;const rec:TSEDSurfaceRec;rflags:integer);
begin
 with level.sectors[sc].surfaces[sf] do
 begin
  if (rflags and sf_adjoin)<>0 then
  begin
   if rec.adjoinSC<0 then adjoin:=nil else
   adjoin:=Level.Sectors[rec.AdjoinSC].Surfaces[rec.AdjoinSF];
  end;
  if (rflags and sf_adjoinflags)<>0 then AdjoinFlags:=rec.AdjoinFlags;
  if (rflags and sf_SurfFlags)<>0 then SurfFlags:=rec.SurfFlags;
  if (rflags and sf_FaceFlags)<>0 then FaceFlags:=rec.FaceFlags;
  if (rflags and sf_Material)<>0 then Material:=rec.material;
  if (rflags and sf_geo)<>0 then geo:=rec.geo;
  if (rflags and sf_light)<>0 then light:=rec.light;
  if (rflags and sf_tex)<>0 then tex:=rec.tex;
  if (rflags and sf_ExtraLight)<>0 then ExtraLight:=TColorF(rec.ExtraLight);
  if (rflags and sf_txscale)<>0 then
  begin
   uscale:=rec.uscale;
   vscale:=rec.vscale;
  end;
 end;
end;

Procedure TSEDCOMLevel.GetSurfaceNormal(sc,sf:integer;var n:TSEDVector);
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

Function TSEDCOMLevel.SurfaceNVertices(sc,sf:integer):Integer;
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

Procedure TSEDCOMLevel.SurfaceGetVertexLight(sc, sf, vx: integer; var color: TSEDColorF);
var tv:TTXVertex;
begin
 tv := Level.Sectors[sc].surfaces[sf].txvertices[vx];
 color := TSEDColorF(tv.color);
end;

Procedure TSEDCOMLevel.SurfaceSetVertexLight(sc, sf, vx: integer; const color: TSEDColorF);
var tv:TTXVertex;
begin
 tv:=Level.Sectors[sc].surfaces[sf].txvertices[vx];
 tv.color := TColorF(color);
end;

{Things}

Function TSEDCOMLevel.AddThing:Integer;
var th:TJKThing;
begin
 th:=Level.NewThing;
 Result:=Level.Things.Add(th);
 Level.RenumThings;
 JedMain.ThingAdded(th);
end;

Procedure TSEDCOMLevel.DeleteThing(th:integer);
begin
 Lev_Utils.DeleteThing(Level,th);
end;

Procedure TSEDCOMLevel.GetThing(th:integer;var rec:TSEDThingRec;rflags:integer);
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
   rec.X:=x;
   rec.Y:=y;
   rec.Z:=z;
  end;

  if (rflags and th_orientation)<>0 then
  begin
   rec.PCH:=PCH;
   rec.YAW:=YAW;
   rec.ROL:=ROL;
  end;

  if (rflags and th_layer)<>0 then rec.layer:=layer;

 end;
end;

Procedure TSEDCOMLevel.SetThing(th:integer;const rec:TSEDThingRec;rflags:integer);
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
   X:=rec.x;
   Y:=rec.y;
   Z:=rec.z;
  end;

  if (rflags and th_orientation)<>0 then
  begin
   PCH:=rec.PCH;
   YAW:=rec.YAW;
   ROL:=rec.ROL;
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

Function TSEDCOMLevel.AddLight:Integer;
var lt:TJedLight;
begin
 lt:=Level.NewLight;
 Result:=Level.lights.Add(lt);
 JedMain.LevelChanged;
end;

Procedure TSEDCOMLevel.DeleteLight(lt:integer);
begin
 Lev_Utils.DeleteLight(Level,lt);
end;

Procedure TSEDCOMLevel.GetLight(lt:integer;var rec:TSEDLightRec;rflags:integer);
begin
 with level.lights[lt] do
 begin
  if (rflags and lt_position)<>0 then
  begin
   rec.X:=x;
   rec.Y:=y;
   rec.Z:=z;
  end;

  if (rflags and lt_intensity)<>0 then rec.Intensity:=Intensity;
  if (rflags and lt_range)<>0 then rec.Range:=range;
  if (rflags and lt_rgb)<>0 then
  begin
   rec.R:=r;
   rec.G:=g;
   rec.B:=b;
  end;

  if (rflags and lt_rgbintensity)<>0 then rec.rgbintensity:=rgbintensity;
  if (rflags and lt_rgbrange)<>0 then rec.rgbrange:=rgbrange;
  if (rflags and lt_flags)<>0 then rec.flags:=flags;
  if (rflags and lt_layer)<>0 then rec.layer:=layer;

 end;
end;

Procedure TSEDCOMLevel.SetLight(lt:integer;const rec:TSEDLightRec;rflags:integer);
begin
 with level.lights[lt] do
 begin
  if (rflags and lt_position)<>0 then
  begin
   X:=rec.x;
   Y:=rec.y;
   Z:=rec.z;
  end;

  if (rflags and lt_intensity)<>0 then Intensity:=rec.Intensity;
  if (rflags and lt_range)<>0 then Range:=rec.range;
  if (rflags and lt_rgb)<>0 then
  begin
   R:=rec.r;
   G:=rec.g;
   B:=rec.b;
  end;

  if (rflags and lt_rgbintensity)<>0 then rgbintensity:=rec.rgbintensity;
  if (rflags and lt_rgbrange)<>0 then rgbrange:=rec.rgbrange;
  if (rflags and lt_flags)<>0 then flags:=rec.flags;
  if (rflags and lt_layer)<>0 then layer:=rec.layer;

 end;
end;

Function TSEDCOMLevel.NLayers:integer;
begin
 Result:=Level.Layers.count;
end;

Function TSEDCOMLevel.GetLayerName(n:integer): PChar;
begin
 Result := PChar(Level.GetLayerName(n));
end;

Function TSEDCOMLevel.AddLayer(name: PChar):integer;
begin
 Result := Level.AddLayer(name);
end;


{0.92}
Function TSEDCOMLevel.NTHingValues(th:integer):integer;
begin
  result:=Level.Things[th].Vals.count;
end;

Function TSEDCOMLevel.GetThingValueName(th,n:Integer): PChar;
begin
  result := PChar(Level.Things[th].Vals[n].Name);
end;

Function TSEDCOMLevel.GetThingValueData(th,n: integer): PChar;
begin
  result := PChar(Level.Things[th].Vals[n].AsString);
end;

Procedure TSEDCOMLevel.SetThingValueData(th,n: Integer; val: PChar);
var v: TTPLValue;
begin
  v := Level.Things[th].Vals[n];
  S2TPLVal(v.name + '=' + val,v);
end;

Procedure TSEDCOMLevel.GetThingFrame(th,n:Integer;var x,y,z,pch,yaw,rol:double);
begin
  Level.Things[th].Vals[n].GetFrame(x,y,z,pch,yaw,rol);
end;

Procedure TSEDCOMLevel.SetThingFrame(th,n: Integer; x,y,z, pch,yaw,rol: double);
begin
  Level.Things[th].Vals[n].SetFrame(x, y, z, pch, yaw, rol);
end;


Function TSEDCOMLevel.AddThingValue(th: integer; name,val: PChar): integer;
var v:TTPLValue;
begin
  v:=TTplValue.Create;
  S2TPLVal(name + '=' + val, v);
  Result:=Level.Things[th].Vals.Add(v);
end;

Procedure TSEDCOMLevel.InsertThingValue(th,n: Integer; name,val: PChar);
var v:TTPLValue;
begin
  v := TTplValue.Create;
  S2TPLVal(name + '=' + val, v);
  Level.Things[th].Vals.Insert(n, v);
end;

Procedure TSEDCOMLevel.DeleteThingValue(th,n: integer);
var v:TTPLValue;
begin
 v:=Level.Things[th].Vals[n];
 Level.Things[th].Vals.Delete(n);
 v.free;
end;


{COGs}
Function TSEDCOMLevel.AddCOG(name: PChar): Integer;
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

Procedure TSEDCOMLevel.DeleteCOG(n: integer);
var cog:TCOG;
begin
 lev_utils.DeleteCOG(level,n);
end;

Procedure TSEDCOMLevel.UpdateCOG(cg: integer);
begin
 COgForm.UpdateCOG(cg);
end;

Function TSEDCOMLevel.GetCOGFileName(cg: integer): PChar;
begin
 Result := PChar(Level.COGS[cg].name);
end;

Function TSEDCOMLevel.NCOGValues(cg: integer): integer;
begin
 Result:=Level.COGS[cg].vals.count;
end;

Function TSEDCOMLevel.GetCOGValueName(cg,n: Integer): PChar;
begin
 Result := PChar(Level.COGS[cg].vals[n].Name);
end;

Function TSEDCOMLevel.GetCOGValueType(cg,n:Integer): integer;
begin
 Result := Integer(Level.COGS[cg].vals[n].cog_type);
end;

Function TSEDCOMLevel.GetCOGValue(cg,n:Integer): PChar;
begin
 Result := PChar(Level.COGS[cg].vals[n].AsString);
end;

Function TSEDCOMLevel.SetCOGValue(cg,n: Integer; val: PChar): Boolean;
begin
 Result:=Level.cogs[cg].Vals[n].JedVal(val);
end;

Function TSEDCOMLevel.AddCOGValue(cg:integer; name,val: PChar; vtype:integer): integer;
var v:TCOGValue;
begin
 v:=TCogValue.Create;
 v.name:=name;
 v.vtype:=GetVTypeFromCOGType(TCOG_TYpe(vtype));
 v.cog_type:=TCOG_Type(vtype);
 v.Val(val);
 result:=Level.cogs[cg].Vals.Add(v);
end;

Procedure TSEDCOMLevel.InsertCOGValue(cg,n:Integer; name,val: PChar; vtype:integer);
var v:TCOGValue;
begin
 v:=TCogValue.Create;
 v.name:=name;
 v.vtype:=GetVTypeFromCOGType(TCOG_TYpe(vtype));
 v.cog_type:=TCOG_Type(vtype);
 v.Val(val);
 Level.cogs[cg].Vals.Insert(n,v);
end;

Procedure TSEDCOMLevel.DeleteCOGValue(cg,n: integer);
var v:TCOGValue;
begin
 v:=Level.Cogs[cg].Vals[n];
 Level.Cogs[cg].Vals.Delete(n);
 v.free;
end;


{SEDCOM}

Function TSEDCOM.GetMapMode:Integer;
begin
 result:=JedMain.Map_Mode;
end;

Procedure TSEDCOM.SetMapMode(mode:integer);
begin
 JedMain.SetMapMode(Mode);
end;

Function TSEDCOM.GetCurSC:integer;
begin
 Result:=JedMain.Cur_SC;
end;

Procedure TSEDCOM.SetCurSC(sc:integer);
begin
 JedMain.SetCurSC(sc);
end;

Function TSEDCOM.GetCurTH:integer;
begin
 Result:=JedMain.Cur_TH;
end;

Procedure TSEDCOM.SetCurTH(th:integer);
begin
 JedMain.SetCurTH(th);
end;

Function TSEDCOM.GetCurLT:integer;
begin
 Result:=JedMain.Cur_LT;
end;

Procedure TSEDCOM.SetCurLT(lt:integer);
begin
 JedMain.SetCurLT(lt);
end;

Procedure TSEDCOM.GetCurVX(var sc,vx:integer);
begin
 sc:=JedMain.Cur_SC;
 vx:=JedMain.Cur_VX;
end;

Procedure TSEDCOM.SetCurVX(sc,vx:integer);
begin
 JedMain.SetCurVX(sc,vx);
end;

Procedure TSEDCOM.GetCurSF(var sc,sf:integer);
begin
 sc:=JedMain.Cur_SC;
 sf:=JedMain.Cur_SF;
end;

Procedure TSEDCOM.SetCurSF(sc,sf:integer);
begin
 JedMain.SetCurSF(sc,sf);
end;

Procedure TSEDCOM.GetCurED(var sc,sf,ed:integer);
begin
 sc:=JedMain.Cur_SC;
 sf:=JedMain.Cur_SF;
 ed:=JedMain.Cur_ED;
end;

Procedure TSEDCOM.SetCurED(sc,sf,ed:integer);
begin
 JedMain.SetCurED(sc,sf,ed);
end;

Procedure TSEDCOM.GetCurFR(var th,fr:integer);
begin
 th:=JedMain.Cur_TH;
 fr:=JedMain.Cur_FR;
end;

Procedure TSEDCOM.SetCurFR(th,fr:integer);
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

Procedure TSEDCOM.FindBBox(sec:integer;var box:TSEDBox);
begin
 lev_utils.FindBBox(Level.Sectors[sec],TBox(box));
end;

Procedure TSEDCOM.FindBoundingSphere(sec:integer;var CX,CY,CZ,Radius:double);
begin
 lev_utils.FindBSphere(level.Sectors[sec],cx,cy,cz,radius);
end;

Function TSEDCOM.FindCollideBox(sec:integer;const bbox:TSEDBox;cx,cy,cz:double;var cbox:TSEDBox):boolean;
begin
 Result:=lev_utils.FindCollideBox(level.Sectors[sec],TBox(bbox),cx,cy,cz,Tbox(cbox));
end;

Procedure TSEDCOM.FindSurfaceCenter(sc,sf:integer;var cx,cy,cz:double);
begin
 lev_utils.CalcSurfCenter(level.sectors[sc].surfaces[sf],cx,cy,cz);
end;

Procedure TSEDCOM.RotateVector(var vec:TSEDVector; pch,yaw,rol:double);
begin
 lev_utils.RotateVector(TVector(vec),pch,yaw,rol);
end;

Procedure TSEDCOM.RotatePoint(ax1,ay1,az1,ax2,ay2,az2:double;angle:double;var x,y,z:double);
begin
 lev_utils.RotatePoint(ax1,ay1,az1,ax2,ay2,az2,angle,x,y,z);
end;

Procedure TSEDCOM.GetJKPYR(const x,y,z:TSEDVector;var pch,yaw,rol:double);
begin
 lev_utils.GetJKPYR(Tvector(x),Tvector(y),Tvector(z),pch,yaw,rol);
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

Function TSEDCOM.IsInSector(sec:integer;x,y,z:double):boolean;
begin
 Result:=lev_utils.IsInSector(Level.Sectors[sec],x,y,z);
end;

Function TSEDCOM.DoSectorsOverlap(sec1,sec2:integer):boolean;
begin
 result:=lev_utils.DoSectorsOverlap(Level.Sectors[sec1],Level.Sectors[sec2]);
end;

Function TSEDCOM.IsPointOnSurface(sc,sf:integer;x,y,z:double):boolean;
begin
 result:=lev_utils.IsPointOnSurface(Level.Sectors[sc].Surfaces[sf],x,y,z);
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

Function TSEDCOM.CleaveSurface(sc,sf:integer; const cnormal:TSEDVector; cx,cy,cz:double):integer;
begin
 if lev_utils.CleaveSurface(Level.Sectors[sc].Surfaces[sf],Tvector(Cnormal),cx,cy,cz) then
 result:=Level.Sectors[sc].Surfaces.count-1 else result:=-1;
end;

Function TSEDCOM.CleaveSector(sec:integer; const cnormal:TSEDVector; cx,cy,cz:double):integer;
begin
 if lev_utils.CleaveSector(Level.Sectors[sec],Tvector(Cnormal),cx,cy,cz) then
 result:=Level.Sectors.count-1 else result:=-1;
end;

Function TSEDCOM.CleaveEdge(sc,sf,ed:integer; const cnormal:TSEDVector; cx,cy,cz:double):boolean;
begin
 result:=lev_utils.CleaveEdge(Level.Sectors[sc].Surfaces[sf],ed,Tvector(Cnormal),cx,cy,cz);
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

Procedure TSEDCOM.CalculateDefaultUVNormals(sc,sf:integer; orgvx:integer; var un,vn:TSEDVector);
begin
 lev_utils.CalcDefaultUVNormals(Level.Sectors[sc].Surfaces[sf],
                                     orgvx,Tvector(un),Tvector(vn));
end;

Procedure TSEDCOM.CalcUVNormals(sc,sf:integer; var un,vn:TSEDVector);
begin
 lev_utils.CalcUVNormals(Level.Sectors[sc].Surfaces[sf],
                                     Tvector(un),Tvector(vn));
end;

Procedure TSEDCOM.ArrangeTexture(sc,sf:integer; orgvx:integer; const un,vn:TSEDVector);
begin
 lev_utils.ArrangeTexture(Level.Sectors[sc].Surfaces[sf],
                                     orgvx,Tvector(un),Tvector(vn));
end;

Procedure TSEDCOM.ArrangeTextureBy(sc,sf:integer;const un,vn:TSEDVector;refx,refy,refz,refu,refv:double);
begin
 lev_utils.ArrangeTextureBy(Level.Sectors[sc].Surfaces[sf],
                                     Tvector(un),Tvector(vn),refx,refy,refz,refu,refv);
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

Function TSEDCOM.CreateCubicSector(x,y,z:double;const pnormal,edge:TSEDVector):integer;
begin
 lev_utils.CreateCube(level,x,y,z,Tvector(pnormal),TVector(edge));
 result:=Level.sectors.count-1;
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

Function TSEDCOM.GetNMultiselected(what:integer):integer;
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

Function TSEDCOM.GetSelectedSC(n:integer):integer;
begin
 Result:=JedMain.scsel.GetSC(n);
end;

Function TSEDCOM.GetSelectedTH(n:integer):integer;
begin
 Result:=JedMain.thsel.GetTH(n);
end;

Function TSEDCOM.GetSelectedLT(n:integer):integer;
begin
 Result:=JedMain.ltsel.GetLT(n);
end;

Procedure TSEDCOM.GetSelectedSF(n:integer;var sc,sf:integer);
begin
 JedMain.sfsel.GetSCSF(n,sc,sf);
end;

Procedure TSEDCOM.GetSelectedED(n:integer;var sc,sf,ed:integer);
begin
 JedMain.edsel.GetSCSFED(n,sc,sf,ed);
end;

Procedure TSEDCOM.GetSelectedVX(n:integer;var sc,vx:integer);
begin
 JedMain.vxsel.GetSCVX(n,sc,vx);
end;

Procedure TSEDCOM.GetSelectedFR(n:integer;var th,fr:integer);
begin
 JedMain.frsel.GetTHFR(n,th,fr);
end;

Function TSEDCOM.SelectSC(sc:integer):integer;
begin
 Result:=Nmask and JedMain.scsel.AddSC(sc);
end;

Function TSEDCOM.SelectSF(sc,sf:integer):integer;
begin
 Result:=Nmask and JedMain.sfsel.AddSF(sc,sf);
end;

Function TSEDCOM.SelectED(sc,sf,ed:integer):integer;
begin
 Result:=Nmask and JedMain.edsel.AddED(sc,sf,ed);
end;

Function TSEDCOM.SelectVX(sc,vx:integer):integer;
begin
 Result:=Nmask and JedMain.vxsel.AddVX(sc,vx);
end;

Function TSEDCOM.SelectTH(th:integer):integer;
begin
 Result:=Nmask and JedMain.thsel.AddTH(th);
end;

Function TSEDCOM.SelectFR(th,fr:integer):integer;
begin
 Result:=Nmask and JedMain.frsel.AddFR(th,fr);
end;

Function TSEDCOM.SelectLT(lt:integer):integer;
begin
 Result:=Nmask and JedMain.ltsel.AddLT(lt);
end;

Function TSEDCOM.FindSelectedSC(sc:integer):integer;
begin
 Result:=JedMain.scsel.FindSC(sc);
end;

Function TSEDCOM.FindSelectedSF(sc,sf:integer):integer;
begin
 Result:=JedMain.sfsel.FindSF(sc,sf);
end;

Function TSEDCOM.FindSelectedED(sc,sf,ed:integer):integer;
begin
 Result:=JedMain.edsel.FindED(sc,sf,ed);
end;

Function TSEDCOM.FindSelectedVX(sc,vx:integer):integer;
begin
 Result:=JedMain.vxsel.FindVX(sc,vx);
end;

Function TSEDCOM.FindSelectedTH(th:integer):integer;
begin
 Result:=JedMain.thsel.FindTH(th);
end;

Function TSEDCOM.FindSelectedFR(th,fr:integer):integer;
begin
 Result:=JedMain.frsel.FindFR(th,fr);
end;

Function TSEDCOM.FindSelectedLT(lt:integer):integer;
begin
 Result:=JedMain.ltsel.FindLT(lt);
end;

Procedure TSEDCOM.SaveSED(name: PChar);
begin
 JedMain.SaveToFile(name, 'jed');
end;

Procedure TSEDCOM.SaveJKL(name: PChar);
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

Function TSEDCOM.NConsistencyErrors:integer;
begin
 result:=Consistency.NErrors;
end;

Function TSEDCOM.GetConsErrorString(n:integer): PChar;

begin
 result:= PChar(Consistency.ErrorText(n));
end;

Function TSEDCOM.GetConsErrorType(n:integer):integer;
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

Function TSEDCOM.GetConsErrorSector(n:integer):integer;
var obj:TConsistencyError;
begin
 result:=-1;
 obj:=Consistency.ErrorObject(n);
 if obj.itype=itSector then Result:=TJKSector(obj.item).num
 else if obj.itype=itSurface then Result:=TJKSurface(obj.item).sector.num;
end;

Function TSEDCOM.GetConsErrorSurface(n:integer):integer;
var obj:TConsistencyError;
begin
 result:=-1;
 obj:=Consistency.ErrorObject(n);
 if obj.itype=itSurface then Result:=TJKSurface(obj.item).num;
end;

Function TSEDCOM.GetConsErrorThing(n:integer):integer;
var obj:TConsistencyError;
begin
 result:=-1;
 obj:=Consistency.ErrorObject(n);
 if obj.itype=itThing then Result:=TJKThing(obj.item).num;
end;

Function TSEDCOM.GetConsErrorCog(n:integer):integer;
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
      SS_CDDir:       result := PChar(CDDir);
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

Function TSEDCOM.GetJEDWindow(whichone:integer):integer;
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

Function TSEDCOM.TextFileEof(handle: integer):boolean;
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

Function TSEDCOM.OpenGOB(name: PChar):integer;
begin
try
 result := conts.NewHandle(OpenContainer(name));
except
 on Exception do result:=-1;
end;
end;

Function TSEDCOM.GOBNFiles(handle:integer):integer;
begin
 Result:=TContainerFile(conts.GetItemNoNIL(handle)).Files.Count;
end;

Function TSEDCOM.GOBNFileName(handle:integer;n:integer): PChar;
begin
 Result := PChar(TContainerFile(conts.GetItemNoNIL(handle)).Files[n]);
end;

Function TSEDCOM.GOBNFullFileName(handle:integer; n:integer): PChar;
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

Procedure TSEDCOM.CloseGOB(handle:integer);
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

function TCOMWFRenderer.GetXYZonPlaneAt(scX,scY:integer;pnormal:TSEDVector; pX,pY,pZ:double; var X,Y,Z:double):Boolean;
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
