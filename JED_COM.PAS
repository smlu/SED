unit JED_COM;

interface
uses OLE2, Windows, J_Level,ResourcePicker,jed_plugins,misc_utils,globalVars,
     u_undo, Forms, sysUtils, classes, Render, ogl_render,sft_render, Geometry, values, u_multisel, menus;

type

THandles=class(TList)
 oname:string;
 Function GetItem(n:integer):TObject;
 Procedure SetItem(n:integer;v:TObject);
 Function GetItemNoNIL(n:integer):TObject;

 Procedure FreeHandle(h:integer);
 Function NewHandle(v:TObject):integer;

 Constructor Create(const name:string);
 Destructor Destroy;override;

// Property Items[n:integer]:TObject read GetItem write SetItem; default;
end;


TJEDCOM=class(IJED)

  files,
  tfiles,
  conts:THandles;

  Constructor Create;

  function QueryInterface(iid: pointer; var obj): LongInt; override;
  function AddRef: Longint; override;
  function Release: Longint; override;

  Function GetVersion:double; override;
  Function GetLevel:IJEDLevel;override; {IJEDLevel object}

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

  Procedure NewLevel(mots:boolean);override;
  Procedure LoadLevel(name:pchar);override;

  {Different level editing functions}

  Procedure FindBBox(sec:integer;var box:TJEDBox);override;
  Procedure FindBoundingSphere(sec:integer;var CX,CY,CZ,Radius:double);override;
  Function FindCollideBox(sec:integer;const bbox:TJEDBox;cx,cy,cz:double;var cbox:TJEDBox):boolean;override;
  Procedure FindSurfaceCenter(sc,sf:integer;var cx,cy,cz:double);override;
  Procedure RotateVector(var vec:TJEDVector; pch,yaw,rol:double);override;
  Procedure RotatePoint(ax1,ay1,az1,ax2,ay2,az2:double;angle:double;var x,y,z:double);override;
  Procedure GetJKPYR(const x,y,z:TJEDVector;var pch,yaw,rol:double);override;
  Function IsSurfaceConvex(sc,sf:integer):boolean;override;
  Function IsSurfacePlanar(sc,sf:integer):boolean;override;
  Function IsSectorConvex(sec:integer):boolean;override;
  Function IsInSector(sec:integer;x,y,z:double):boolean;override;
  Function DoSectorsOverlap(sec1,sec2:integer):boolean;override;
  Function IsPointOnSurface(sc,sf:integer;x,y,z:double):boolean;override;
  Function FindSectorForThing(th:integer):Integer;override;
  Function FindSectorForXYZ(X,Y,Z:double):integer;override;
  Function ExtrudeSurface(sc,sf:integer; by:double):integer;override;
  Function CleaveSurface(sc,sf:integer; const cnormal:TJEDvector; cx,cy,cz:double):integer;override;
  Function CleaveSector(sec:integer; const cnormal:TJEDvector; cx,cy,cz:double):integer;override;
  Function CleaveEdge(sc,sf,ed:integer; const cnormal:TJEDvector; cx,cy,cz:double):boolean;override;
  Function JoinSurfaces(sc1,sf1,sc2,sf2:Integer):boolean;override;
  Function PlanarizeSurface(sc,sf:integer):boolean;override;
  function MergeSurfaces(sc,sf1,sf2:integer):integer;override;
  function MergeSectors(sec1,sec2:integer):integer;override;
  Procedure CalculateDefaultUVNormals(sc,sf:integer; orgvx:integer; var un,vn:TJEDVector);override;
  Procedure CalcUVNormals(sc,sf:integer; var un,vn:TJEDVector);override;
  Procedure ArrangeTexture(sc,sf:integer; orgvx:integer; const un,vn:TJEDvector);override;
  Procedure ArrangeTextureBy(sc,sf:integer;const un,vn:TJEDvector;refx,refy,refz,refu,refv:double);override;
  Function IsTextureFlipped(sc,sf:integer):boolean;override;
  Procedure RemoveSurfaceReferences(sc,sf:integer);override;
  Procedure RemoveSectorReferences(sec:integer;surfs:boolean);override;
  Function StitchSurfaces(sc1,sf1,sc2,sf2:integer):boolean;override;
  Function FindCommonEdges(sc1,sf1,sc2,sf2:integer; var v11,v12,v21,v22:integer):boolean;override;
  Function DoSurfacesOverlap(sc1,sf1,sc2,sf2:integer):boolean;override;
  Function MakeAdjoin(sc,sf:integer):boolean;override;
  Function MakeAdjoinFromSectorUp(sc,sf:integer;firstsc:integer):boolean;override;
  Function UnAdjoin(sc,sf:integer):Boolean;override;
  Function CreateCubicSector(x,y,z:double;const pnormal,edge:TJEDVector):integer;override;

  Procedure StartUndo(name:pchar);override;
  Procedure SaveUndoForThing(n:integer;change:integer);override;
  Procedure SaveUndoForLight(n:integer;change:integer);override;
  Procedure SaveUndoForSector(n:integer;change:integer;whatpart:integer);override;
  Procedure ClearUndoBuffer;override;
  Procedure ApplyUndo;override;
  {0.92}
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

  Procedure SaveJED(name:pchar);override;
  Procedure SaveJKL(name:pchar);override;
  Procedure UpdateMap;override;
  Procedure SetPickerCMP(cmp:pchar);override;
  Function PickResource(what:integer;cur:pchar):pchar;override;
  Function EditFlags(what:integer;flags:LongInt):LongInt;override;

  Procedure ReloadTemplates;override;
  Procedure PanMessage(mtype:integer;msg:pchar);override;
  Procedure SendKey(shift:integer;key:integer);override;
  Function ExecuteMenu(itemref:pchar):boolean;override;
  Function GetJEDSetting(name:pchar):variant;override;
  Function IsLayerVisible(n:integer):boolean;override;

  procedure CheckConsistencyErrors;override;
  procedure CheckResources;override;
  Function NConsistencyErrors:integer;override;
  Function GetConsErrorString(n:integer):pchar;override;
  Function GetConsErrorType(n:integer):integer;override;
  Function GetConsErrorSector(n:integer):integer;override;
  Function GetConsErrorSurface(n:integer):integer;override;
  Function GetConsErrorThing(n:integer):integer;override;
  Function GetConsErrorCog(n:integer):integer;override;
  Function IsPreviewActive:boolean;override;
  Function GetJEDString(what:integer):pchar;override;
{  Function GetProjectDir:String;
  Function GetJEDDir:String;
  Function GetLevelFile:String;, gobs, cddir, regbase, LEClogo, JKDIr, MotsDir,
  defshape, recent}
  Function GetIsMOTS:boolean;override;
  Procedure SetIsMOTS(mots:boolean);override;

  {0.93}
  Function GetJedWindow(whichone:integer):integer;override;

  Function FileExtractExt(path:pchar):pchar;override;
  Function FileExtractPath(path:pchar):pchar;override;
  Function FileExtractName(path:pchar):pchar;override;
  Function FindProjectDirFile(name:pchar):pchar;override;
  Function IsFileContainer(path:pchar):boolean;override;
  Function IsFileInContainer(path:pchar):Boolean;override;

  Function FileOpenDialog(fname:pchar;filter:pchar):pchar;override;


  Function OpenFile(name:pchar):integer;override;
  Function OpenGameFile(name:pchar):integer;override;
  Function ReadFile(handle:integer;buf:pointer;size:longint):integer;override;
  Procedure SetFilePos(handle:integer;pos:longint);override;
  Function GetFilePos(handle:integer):longint;override;
  Function GetFileSize(handle:integer):longint;override;
  Function GetFileName(handle:integer):pchar;override;
  Procedure CloseFile(handle:integer);override;

  Function OpenTextFile(name:pchar):integer;override;
  Function OpenGameTextFile(name:pchar):integer;override;
  Function GetTextFileName(handle:integer):pchar;override;
  Function ReadTextFileString(handle:integer):pchar;override;
  Function TextFileEof(handle:integer):boolean;override;
  Function TextFileCurrentLine(handle:integer):integer;override;
  Procedure CloseTextFile(handle:integer);override;

  Function OpenGOB(name:pchar):integer;override;
  Function GOBNFiles(handle:integer):integer;override;
  Function GOBNFileName(handle:integer;n:integer):pchar;override;
  Function GOBNFullFileName(handle:integer;n:integer):pchar;override;
  Function GOBGetFileSize(handle:integer;n:integer):longint;override;
  Function GOBGetFileOffset(handle:integer;n:integer):longint;override;
  Procedure CloseGOB(handle:integer);override;

  Function CreateWFRenderer(wnd:integer;whichone:integer):IJEDWFRenderer;override;

end;

 TCOMLevel=class(IJEDLevel)
  function QueryInterface(iid: pointer; var obj): LongInt; override;
  function AddRef: Longint; override;
  function Release: Longint; override;

  Procedure GetLevelHeader(var lh:TLevelHeader;rflags:integer);override;
  Procedure SetLevelHeader(const lh:TLevelHeader;rflags:integer);override;

  Function NSectors:integer;override;
  Function NThings:integer;override;
  Function NLights:integer;override;
  Function NCOgs:integer;override;

  {Sectors}
  Function AddSector:integer;override;
  Procedure DeleteSector(n:integer);override;

  Procedure GetSector(sec:integer;var rec:TJEDSectorRec;what:integer);override;
  Procedure SetSector(sec:integer;const rec:TJEDSectorRec;what:integer);override;

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
  Procedure GetSurface(sc,sf:integer;var rec:TJEDSurfaceRec;rflags:integer);override;
  Procedure SetSurface(sc,sf:integer;const rec:TJEDSurfaceRec;rflags:integer);override;
  Procedure GetSurfaceNormal(sc,sf:integer;var n:TJEDVector);override;
  Procedure SurfaceUpdate(sc,sf:integer;how:integer);override;
  Function SurfaceNVertices(sc,sf:integer):Integer;override;
  Function SurfaceGetVertexNum(sc,sf,vx:integer):integer;override;
  Procedure SurfaceSetVertexNum(sc,sf,vx:integer;secvx:integer);override;
  Function SurfaceAddVertex(sc,sf:integer;secvx:integer):Integer;override;
  Function SurfaceInsertVertex(sc,sf:integer;at:integer;secvx:integer):Integer;override;
  Procedure SurfaceDeleteVertex(sc,sf:integer;n:integer);override;
  Procedure SurfaceGetVertexUV(sc,sf,vx:integer;var u,v:single);override;
  Procedure SurfaceSetVertexUV(sc,sf,vx:integer;u,v:single);override;
  Procedure SurfaceGetVertexLight(sc,sf,vx:integer;var white,r,g,b:single);override;
  Procedure SurfaceSetVertexLight(sc,sf,vx:integer;white,r,g,b:single);override;

  {things}
  Function AddThing:Integer;override;
  Procedure DeleteThing(th:integer);override;
  Procedure GetThing(th:integer;var rec:TJEDThingRec;rflags:integer);override;
  Procedure SetThing(th:integer;const rec:TJEDThingRec;rflags:integer);override;
  Procedure ThingUpdate(th:integer);override;

  {Lights}
  Function AddLight:Integer;override;
  Procedure DeleteLight(lt:integer);override;
  Procedure GetLight(lt:integer;var rec:TJEDLightRec;rflags:integer);override;
  Procedure SetLight(lt:integer;const rec:TJEDLightRec;rflags:integer);override;

  {Layers}
  Function NLayers:integer;override;
  Function GetLayerName(n:integer):pchar;override;
  Function AddLayer(name:Pchar):integer;override;

  {0.92}

  Function NTHingValues(th:integer):integer;override;
  Function GetThingValueName(th,n:Integer):pchar;override;
  Function GetThingValueData(th,n:integer):pchar;override;
  Procedure SetThingValueData(th,n:Integer;val:pchar);override;

  Procedure GetThingFrame(th,n:Integer;var x,y,z,pch,yaw,rol:double);override;
  Procedure SetThingFrame(th,n:Integer;x,y,z,pch,yaw,rol:double);override;

  Function AddThingValue(th:integer;name,val:pchar):integer;override;
  Procedure InsertThingValue(th,n:Integer;name,val:pchar);override;
  Procedure DeleteThingValue(th,n:integer);override;

  {COGs}
  Function AddCOG(name:pchar):Integer;override;
  Procedure DeleteCOG(n:integer);override;
  Procedure UpdateCOG(cg:integer);override;

  Function GetCOGFileName(cg:integer):pchar;override;
  Function NCOGValues(cg:integer):integer;override;

  Function GetCOGValueName(cg,n:Integer):pchar;override;
  Function GetCOGValueType(cg,n:Integer):integer;override;

  Function GetCOGValue(cg,n:Integer):pchar;override;
  Function SetCOGValue(cg,n:Integer;val:pchar):boolean;override;

  Function AddCOGValue(cg:integer;name,val:pchar;vtype:integer):integer;override;
  Procedure InsertCOGValue(cg,n:Integer;name,val:pchar;vtype:integer);override;
  Procedure DeleteCOGValue(cg,n:integer);override;

 end;

 TCOMWFRenderer=class(IJEDWFRenderer)
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

  Function GetXYZonPlaneAt(scX,scY:integer;pnormal:TJedVector; pX,pY,pZ:double; var X,Y,Z:double):Boolean;override;
  Function GetGridAt(scX,scY:integer;var X,Y,Z:double):boolean;override;
  Procedure GetNearestGridNode(iX,iY,iZ:double; Var X,Y,Z:double);override;
  Procedure ProjectPoint(x,y,z:double; Var WinX,WinY:integer);override;
  Procedure UnProjectPoint(WinX,WinY:integer; WinZ:double; var x,y,z:double);override;
  Function IsSurfaceFacing(sc,sf:integer):boolean;override;

  Function HandleWMQueryPal:integer;override;
  Function HandleWMChangePal:integer;override;

 end;



Function GetJEDCOM:IJED;

implementation
uses  Item_edit, u_MsgForm, u_Tools, JED_Main,lev_utils,JED_OLE, listRes,
      u_CogForm, FlagEditor, U_SCFEdit,U_Options, u_tbar, Cons_Checker, u_Preview,
      Files,FileOperations,FileDialogs;

var jedcom:IJED;
    olejedapp:TJEDApp;
    comlevel:TCOMLevel;
    tmpstr:array[0..2047] of char;
    ForeWnd:integer;
    atmpstr:string;



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

Function GetJEDCOM:IJED;
begin
 if jedcom=nil then jedcom:=TJEDCOM.Create;
 result:=jedcom;
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


Constructor TJEDCOM.Create;
begin
 files:=THandles.Create('file');
 tfiles:=THandles.Create('text file');
 conts:=THandles.Create('GOB');
end;


function TJEDCOM.QueryInterface(iid: pointer; var obj): LongInt;
begin
 Result:= CLASS_E_CLASSNOTAVAILABLE;
end;

function TJEDCOM.AddRef: Longint;
begin
 Result:=1;
end;

function TJEDCOM.Release: Longint;
begin
 Result:=0;
end;

Function TJEDCOM.GetVersion:double;
begin
 ValDouble(JedVernum,Result);
 Result:=result*100;
end;

Function TJEDCOM.GetLevel:IJEDLevel; {IJEDLevel object}
begin
 if comlevel=nil then comlevel:=TCOMLevel.Create;
 result:=comLevel;
end;

{Level}

function TCOMLevel.QueryInterface(iid: pointer; var obj): LongInt;
begin
 Result:= CLASS_E_CLASSNOTAVAILABLE;
end;

function TCOmLevel.AddRef: Longint;
begin
 result:=1;
end;

function TCOmLevel.Release: Longint;
begin
 Result:=0;
end;

Procedure TCOMLevel.GetSector(sec:integer;var rec:TJEDSectorRec;what:integer);
begin
 with level.sectors[sec] do
 begin
  if (what and s_flags)<>0 then rec.Flags:=flags;
  if (what and s_ambient)<>0 then rec.Ambient:=Ambient;
  if (what and s_extra)<>0 then rec.extra:=extra;
  if (what and s_cmp)<>0 then rec.ColorMap:=pchar(ColorMap);
  if (what and s_tint)<>0 then
  begin
   rec.Tint_r:=Tint.dx;
   rec.Tint_g:=Tint.dy;
   rec.Tint_b:=Tint.dz;
  end;
  if (what and s_sound)<>0 then rec.Sound:=pchar(Sound);
  if (what and s_sndvol)<>0 then rec.snd_vol:=snd_vol;
  if (what and s_layer)<>0 then rec.layer:=layer;
 end;
end;

Procedure TCOMLevel.SetSector(sec:integer;const rec:TJEDSectorRec;what:integer);
begin
 with level.sectors[sec] do
 begin
  if (what and s_flags)<>0 then Flags:=rec.flags;
  if (what and s_ambient)<>0 then Ambient:=rec.Ambient;
  if (what and s_extra)<>0 then extra:=rec.extra;
  if (what and s_cmp)<>0 then ColorMap:=rec.ColorMap;
  if (what and s_tint)<>0 then SetVec(tint,rec.tint_r,rec.tint_g,rec.tint_b);
  if (what and s_sound)<>0 then Sound:=rec.Sound;
  if (what and s_sndvol)<>0 then snd_vol:=rec.snd_vol;
  if (what and s_layer)<>0 then layer:=rec.layer;
 end;
end;

Procedure TCOMLevel.GetLevelHeader(var lh:TLevelHeader;rflags:integer);
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
   lh.LODDist[0]:=LODDist[1];
   lh.LODDist[1]:=LODDist[2];
   lh.LODDist[2]:=LODDist[3];
   lh.LODDist[3]:=LODDist[4];
  end;
  if (rflags and lh_PerspDist)<>0 then lh.PerspDist:=PerspDist;
  if (rflags and lh_GouraudDist)<>0 then lh.GouraudDist:=GouraudDist;
  if (rflags and lh_ppu)<>0 then lh.PixelPerUnit:=Level.PPUnit;
  if (rflags and lh_MasterCMP)<>0 then lh.MasterCMP:=Pchar(Level.MasterCMP);

 end;
end;

Procedure TCOMLevel.SetLevelHeader(const lh:TLevelHeader;rflags:integer);
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
   LODDist[1]:=lh.LODDist[0];
   LODDist[2]:=lh.LODDist[1];
   LODDist[3]:=lh.LODDist[2];
   LODDist[4]:=lh.LODDist[3];
  end;
  if (rflags and lh_PerspDist)<>0 then PerspDist:=lh.PerspDist;
  if (rflags and lh_GouraudDist)<>0 then GouraudDist:=lh.GouraudDist;
  if (rflags and lh_ppu)<>0 then Level.ppunit:=lh.PixelPerUnit;
  if (rflags and lh_MasterCMP)<>0 then level.MasterCMP:=lh.MasterCMP;

 end;
end;

Function TCOMLevel.NSectors:integer;
begin
 result:=level.sectors.count;
end;

Function TCOMLevel.NThings:integer;
begin
 result:=level.things.count;
end;

Function TCOMLevel.NLights:integer;
begin
 result:=level.lights.count;
end;

Function TCOMLevel.NCOgs:integer;
begin
 result:=level.Cogs.Count;
end;

{Sectors}
Function TCOMLevel.AddSector:integer;
var sec:TJKSector;
begin
 sec:=Level.NewSector;
 Result:=Level.Sectors.Add(sec);
 Level.RenumSecs;
 JedMain.SectorAdded(sec);
end;

Procedure TCOMLevel.DeleteSector(n:integer);
begin
 Lev_Utils.DeleteSector(Level,n);
end;

Function TCOMLevel.SectorNVertices(sec:integer):integer;
begin
 result:=Level.Sectors[sec].vertices.count;
end;

Function TCOMLevel.SectorNSurfaces(sec:integer):integer;
begin
 result:=Level.Sectors[sec].surfaces.count;
end;

Procedure TCOMLevel.SectorGetVertex(sec,vx:integer;var x,y,z:double);
var v:TJKVertex;
begin
 v:=Level.Sectors[sec].Vertices[vx];
 x:=v.x;
 y:=v.y;
 z:=v.z;
end;

Procedure TCOMLevel.SectorSetVertex(sec,vx:integer;x,y,z:double);
var v:TJKVertex;
begin
 v:=Level.Sectors[sec].Vertices[vx];
 v.x:=x;
 v.y:=y;
 v.z:=z;
end;

Function TCOMLevel.SectorAddVertex(sec:integer;x,y,z:double):integer;
var v:TJKVertex;
begin
 v:=Level.Sectors[sec].NewVertex;
 v.x:=x;
 v.y:=y;
 v.z:=z;
 result:=level.sectors[sec].vertices.count-1;
end;

Function TCOMLevel.SectorFindVertex(sec:integer;x,y,z:double):integer;
begin
 result:=Level.Sectors[sec].FindVX(x,y,z);
end;

Function TCOMLevel.SectorDeleteVertex(sec:integer;n:integer):integer;
begin
 With Level.Sectors[sec] do
 begin
  vertices[n].Free;
  vertices.Delete(n);
  renumber;
 end;
end;

Function TCOMLevel.SectorAddSurface(sec:integer):integer;
begin
 With level.sectors[sec] do
 begin
  Result:=surfaces.add(newSurface);
 end;
end;

Procedure TCOMLevel.SectorDeleteSurface(sc,sf:integer);
begin
 With level.sectors[sc] do
 begin
  RemoveSurfRefs(level,surfaces[sf]);
  surfaces[sf].Free;
  surfaces.Delete(sf);
  Renumber;
 end;
end;

Procedure TCOMLevel.SectorUpdate(sec:integer);
var asec:TJKSector;
begin
 asec:=Level.Sectors[sec];
 asec.Renumber;
 JedMain.SectorChanged(asec);
end;

  {Surfaces}
Procedure TCOMLevel.GetSurface(sc,sf:integer;var rec:TJEDSurfaceRec;rflags:integer);
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
  if (rflags and sf_Material)<>0 then rec.Material:=pchar(material);
  if (rflags and sf_geo)<>0 then rec.geo:=geo;
  if (rflags and sf_light)<>0 then rec.light:=light;
  if (rflags and sf_tex)<>0 then rec.tex:=tex;
  if (rflags and sf_ExtraLight)<>0 then rec.ExtraLight:=ExtraLight;
  if (rflags and sf_txscale)<>0 then
  begin
   rec.uscale:=uscale;
   rec.vscale:=vscale;
  end;
 end;
end;

Procedure TCOMLevel.SetSurface(sc,sf:integer;const rec:TJEDSurfaceRec;rflags:integer);
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
  if (rflags and sf_ExtraLight)<>0 then ExtraLight:=rec.ExtraLight;
  if (rflags and sf_txscale)<>0 then
  begin
   uscale:=rec.uscale;
   vscale:=rec.vscale;
  end;
 end;
end;

Procedure TCOMLevel.GetSurfaceNormal(sc,sf:integer;var n:TJEDVector);
begin
 With Level.Sectors[sc].surfaces[sf].normal do
 begin
  n.dx:=dx;
  n.dy:=dy;
  n.dz:=dz;
 end;
end;

Procedure TCOMLevel.SurfaceUpdate(sc,sf:integer;how:integer);
var surf:TJKSurface;
begin
 surf:=Level.Sectors[sc].Surfaces[sf];
 if how and su_texture<>0 then surf.RecalcAll else surf.Recalc;
 if how and su_floorflag<>0 then surf.CheckIfFloor;
 if how and su_sector<>0 then JedMain.SectorChanged(surf.sector);
end;

Function TCOMLevel.SurfaceNVertices(sc,sf:integer):Integer;
begin
 Result:=Level.Sectors[sc].surfaces[sf].vertices.count;
end;

Function TCOMLevel.SurfaceGetVertexNum(sc,sf,vx:integer):integer;
begin
 Result:=Level.Sectors[sc].surfaces[sf].vertices[vx].num;
end;

Procedure TCOMLevel.SurfaceSetVertexNum(sc,sf,vx:integer;secvx:integer);
begin
 With Level.Sectors[sc] do
 begin
  surfaces[sf].vertices[vx]:=vertices[secvx];
 end;
end;

Function TCOMLevel.SurfaceAddVertex(sc,sf:integer;secvx:integer):Integer;
begin
 With Level.Sectors[sc] do
 begin
  result:=surfaces[sf].AddVertex(vertices[secvx]);
 end;
end;

Function TCOMLevel.SurfaceInsertVertex(sc,sf:integer;at:integer;secvx:integer):Integer;
begin
 With Level.Sectors[sc] do
 begin
  result:=surfaces[sf].InsertVertex(at,vertices[secvx]);
 end;
 result:=at;
end;

Procedure TCOMLevel.SurfaceDeleteVertex(sc,sf:integer;n:integer);
begin
 Level.Sectors[sc].surfaces[sf].DeleteVertex(n);
end;

Procedure TCOMLevel.SurfaceGetVertexUV(sc,sf,vx:integer;var u,v:single);
var tv:TTXVertex;
begin
 tv:=Level.Sectors[sc].surfaces[sf].txvertices[vx];
 u:=tv.u;
 v:=tv.v;
end;

Procedure TCOMLevel.SurfaceSetVertexUV(sc,sf,vx:integer;u,v:single);
var tv:TTXVertex;
begin
 tv:=Level.Sectors[sc].surfaces[sf].txvertices[vx];
 tv.u:=u;
 tv.v:=v;
end;

Procedure TCOMLevel.SurfaceGetVertexLight(sc,sf,vx:integer;var white,r,g,b:single);
var tv:TTXVertex;
begin
 tv:=Level.Sectors[sc].surfaces[sf].txvertices[vx];
 white:=tv.color.a;
 r:=tv.color.r;
 g:=tv.color.g;
 b:=tv.color.g;
end;

Procedure TCOMLevel.SurfaceSetVertexLight(sc,sf,vx:integer;white,r,g,b:single);
var tv:TTXVertex;
begin
 tv:=Level.Sectors[sc].surfaces[sf].txvertices[vx];
 tv.color.a:=white;
 tv.color.r:=r;
 tv.color.g:=g;
 tv.color.b:=g;
end;

{things}

Function TCOMLevel.AddThing:Integer;
var th:TJKThing;
begin
 th:=Level.NewThing;
 Result:=Level.Things.Add(th);
 Level.RenumThings;
 JedMain.ThingAdded(th);
end;

Procedure TCOMLevel.DeleteThing(th:integer);
begin
 Lev_Utils.DeleteThing(Level,th);
end;

Procedure TCOMLevel.GetThing(th:integer;var rec:TJEDThingRec;rflags:integer);
begin
 with level.things[th] do
 begin
  if (rflags and th_name)<>0 then rec.name:=pchar(name);
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

Procedure TCOMLevel.SetThing(th:integer;const rec:TJEDThingRec;rflags:integer);
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

Procedure TCOMLevel.ThingUpdate(th:integer);
begin
 Level.RenumThings;
 JedMain.ThingChanged(level.Things[th]);
end;

{Lights}

Function TCOMLevel.AddLight:Integer;
var lt:TJedLight;
begin
 lt:=Level.NewLight;
 Result:=Level.lights.Add(lt);
 JedMain.LevelChanged;
end;

Procedure TCOMLevel.DeleteLight(lt:integer);
begin
 Lev_Utils.DeleteLight(Level,lt);
end;

Procedure TCOMLevel.GetLight(lt:integer;var rec:TJEDLightRec;rflags:integer);
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

Procedure TCOMLevel.SetLight(lt:integer;const rec:TJEDLightRec;rflags:integer);
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

Function TCOMLevel.NLayers:integer;
begin
 Result:=Level.Layers.count;
end;

Function TCOMLevel.GetLayerName(n:integer):pchar;
begin
 Result:=Pchar(Level.GetLayerName(n));
end;

Function TCOMLevel.AddLayer(name:Pchar):integer;
begin
 Result:=Level.AddLayer(name);
end;


{0.92}
Function TCOMLevel.NTHingValues(th:integer):integer;
begin
 result:=Level.Things[th].Vals.count;
end;

Function TCOMLevel.GetThingValueName(th,n:Integer):pchar;
begin
 result:=PChar(Level.Things[th].Vals[n].Name);
end;

Function TCOMLevel.GetThingValueData(th,n:integer):pchar;
begin
 result:=Pchar(Level.Things[th].Vals[n].AsString);
end;

Procedure TCOMLevel.SetThingValueData(th,n:Integer;val:pchar);
var v:TTPLValue;
begin
 v:=Level.Things[th].Vals[n];
 S2TPLVal(v.name+'='+val,v);
end;

Procedure TCOMLevel.GetThingFrame(th,n:Integer;var x,y,z,pch,yaw,rol:double);
begin
 Level.Things[th].Vals[n].GetFrame(x,y,z,pch,yaw,rol);
end;

Procedure TCOMLevel.SetThingFrame(th,n:Integer;x,y,z,pch,yaw,rol:double);
begin
 Level.Things[th].Vals[n].SetFrame(x,y,z,pch,yaw,rol);
end;


Function TCOMLevel.AddThingValue(th:integer;name,val:pchar):integer;
var v:TTPLValue;
begin
 v:=TTplValue.Create;
 S2TPLVal(name+'='+val,v);
 Result:=Level.Things[th].Vals.Add(v);
end;

Procedure TCOMLevel.InsertThingValue(th,n:Integer;name,val:pchar);
var v:TTPLValue;
begin
 v:=TTplValue.Create;
 S2TPLVal(name+'='+val,v);
 Level.Things[th].Vals.Insert(n,v);
end;

Procedure TCOMLevel.DeleteThingValue(th,n:integer);
var v:TTPLValue;
begin
 v:=Level.Things[th].Vals[n];
 Level.Things[th].Vals.Delete(n);
 v.free;
end;


{COGs}
Function TCOMLevel.AddCOG(name:pchar):Integer;
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

Procedure TCOMLevel.DeleteCOG(n:integer);
var cog:TCOG;
begin
 lev_utils.DeleteCOG(level,n);
end;

Procedure TCOMLevel.UpdateCOG(cg:integer);
begin
 COgForm.UpdateCOG(cg);
end;

Function TCOMLevel.GetCOGFileName(cg:integer):pchar;
begin
 Result:=PChar(Level.COGS[cg].name);
end;

Function TCOMLevel.NCOGValues(cg:integer):integer;
begin
 Result:=Level.COGS[cg].vals.count;
end;

Function TCOMLevel.GetCOGValueName(cg,n:Integer):pchar;
begin
 Result:=PChar(Level.COGS[cg].vals[n].Name);
end;

Function TCOMLevel.GetCOGValueType(cg,n:Integer):integer;
begin
 Result:=Integer(Level.COGS[cg].vals[n].cog_type);
end;

Function TCOMLevel.GetCOGValue(cg,n:Integer):pchar;
begin
 Result:=PChar(Level.COGS[cg].vals[n].AsString);
end;

Function TCOMLevel.SetCOGValue(cg,n:Integer;val:pchar):boolean;
begin
 Result:=Level.cogs[cg].Vals[n].JedVal(val);
end;

Function TCOMLevel.AddCOGValue(cg:integer;name,val:pchar;vtype:integer):integer;
var v:TCOGValue;
begin
 v:=TCogValue.Create;
 v.name:=name;
 v.vtype:=GetVTypeFromCOGType(TCOG_TYpe(vtype));
 v.cog_type:=TCOG_Type(vtype);
 v.Val(val);
 result:=Level.cogs[cg].Vals.Add(v);
end;

Procedure TCOMLevel.InsertCOGValue(cg,n:Integer;name,val:pchar;vtype:integer);
var v:TCOGValue;
begin
 v:=TCogValue.Create;
 v.name:=name;
 v.vtype:=GetVTypeFromCOGType(TCOG_TYpe(vtype));
 v.cog_type:=TCOG_Type(vtype);
 v.Val(val);
 Level.cogs[cg].Vals.Insert(n,v);
end;

Procedure TCOMLevel.DeleteCOGValue(cg,n:integer);
var v:TCOGValue;
begin
 v:=Level.Cogs[cg].Vals[n];
 Level.Cogs[cg].Vals.Delete(n);
 v.free;
end;


{JEDCOM}

Function TJEDCOM.GetMapMode:Integer;
begin
 result:=JedMain.Map_Mode;
end;

Procedure TJEDCOM.SetMapMode(mode:integer);
begin
 JedMain.SetMapMode(Mode);
end;

Function TJEDCOM.GetCurSC:integer;
begin
 Result:=JedMain.Cur_SC;
end;

Procedure TJEDCOM.SetCurSC(sc:integer);
begin
 JedMain.SetCurSC(sc);
end;

Function TJEDCOM.GetCurTH:integer;
begin
 Result:=JedMain.Cur_TH;
end;

Procedure TJEDCOM.SetCurTH(th:integer);
begin
 JedMain.SetCurTH(th);
end;

Function TJEDCOM.GetCurLT:integer;
begin
 Result:=JedMain.Cur_LT;
end;

Procedure TJEDCOM.SetCurLT(lt:integer);
begin
 JedMain.SetCurLT(lt);
end;

Procedure TJEDCOM.GetCurVX(var sc,vx:integer);
begin
 sc:=JedMain.Cur_SC;
 vx:=JedMain.Cur_VX;
end;

Procedure TJEDCOM.SetCurVX(sc,vx:integer);
begin
 JedMain.SetCurVX(sc,vx);
end;

Procedure TJEDCOM.GetCurSF(var sc,sf:integer);
begin
 sc:=JedMain.Cur_SC;
 sf:=JedMain.Cur_SF;
end;

Procedure TJEDCOM.SetCurSF(sc,sf:integer);
begin
 JedMain.SetCurSF(sc,sf);
end;

Procedure TJEDCOM.GetCurED(var sc,sf,ed:integer);
begin
 sc:=JedMain.Cur_SC;
 sf:=JedMain.Cur_SF;
 ed:=JedMain.Cur_ED;
end;

Procedure TJEDCOM.SetCurED(sc,sf,ed:integer);
begin
 JedMain.SetCurED(sc,sf,ed);
end;

Procedure TJEDCOM.GetCurFR(var th,fr:integer);
begin
 th:=JedMain.Cur_TH;
 fr:=JedMain.Cur_FR;
end;

Procedure TJEDCOM.SetCurFR(th,fr:integer);
begin
 JedMain.SetCurFR(th,fr);
end;

Procedure TJEDCOM.NewLevel(mots:boolean);
begin
 case mots of
  true: JedMain.New1.Click;
  false: JedMain.NewMotsProject1.Click;
 end;
end;

Procedure TJEDCOM.LoadLevel(name:pchar);
begin
 JedMain.OpenProject(name,op_open);
end;

{Different level editing functions}

Procedure TJEDCOM.FindBBox(sec:integer;var box:TJEDBox);
begin
 lev_utils.FindBBox(Level.Sectors[sec],TBox(box));
end;

Procedure TJEDCOM.FindBoundingSphere(sec:integer;var CX,CY,CZ,Radius:double);
begin
 lev_utils.FindBSphere(level.Sectors[sec],cx,cy,cz,radius);
end;

Function TJEDCOM.FindCollideBox(sec:integer;const bbox:TJEDBox;cx,cy,cz:double;var cbox:TJEDBox):boolean;
begin
 Result:=lev_utils.FindCollideBox(level.Sectors[sec],TBox(bbox),cx,cy,cz,Tbox(cbox));
end;

Procedure TJEDCOM.FindSurfaceCenter(sc,sf:integer;var cx,cy,cz:double);
begin
 lev_utils.CalcSurfCenter(level.sectors[sc].surfaces[sf],cx,cy,cz);
end;

Procedure TJEDCOM.RotateVector(var vec:TJEDVector; pch,yaw,rol:double);
begin
 lev_utils.RotateVector(TVector(vec),pch,yaw,rol);
end;

Procedure TJEDCOM.RotatePoint(ax1,ay1,az1,ax2,ay2,az2:double;angle:double;var x,y,z:double);
begin
 lev_utils.RotatePoint(ax1,ay1,az1,ax2,ay2,az2,angle,x,y,z);
end;

Procedure TJEDCOM.GetJKPYR(const x,y,z:TJEDVector;var pch,yaw,rol:double);
begin
 lev_utils.GetJKPYR(Tvector(x),Tvector(y),Tvector(z),pch,yaw,rol);
end;

Function TJEDCOM.IsSurfaceConvex(sc,sf:integer):boolean;
begin
 result:=lev_utils.IsSurfConvex(Level.Sectors[sc].Surfaces[sf]);
end;

Function TJEDCOM.IsSurfacePlanar(sc,sf:integer):boolean;
begin
 result:=lev_utils.IsSurfPlanar(Level.Sectors[sc].Surfaces[sf]);
end;

Function TJEDCOM.IsSectorConvex(sec:integer):boolean;
begin
 result:=lev_utils.IsSectorConvex(Level.Sectors[sec]);
end;

Function TJEDCOM.IsInSector(sec:integer;x,y,z:double):boolean;
begin
 Result:=lev_utils.IsInSector(Level.Sectors[sec],x,y,z);
end;

Function TJEDCOM.DoSectorsOverlap(sec1,sec2:integer):boolean;
begin
 result:=lev_utils.DoSectorsOverlap(Level.Sectors[sec1],Level.Sectors[sec2]);
end;

Function TJEDCOM.IsPointOnSurface(sc,sf:integer;x,y,z:double):boolean;
begin
 result:=lev_utils.IsPointOnSurface(Level.Sectors[sc].Surfaces[sf],x,y,z);
end;

Function TJEDCOM.FindSectorForThing(th:integer):Integer;
begin
 result:=lev_utils.FindSectorForThing(Level.Things[th]);
end;

Function TJEDCOM.FindSectorForXYZ(X,Y,Z:double):integer;
begin
 result:=lev_utils.FindSectorForXYZ(level,x,y,z);
end;

Function TJEDCOM.ExtrudeSurface(sc,sf:integer; by:double):integer;
begin
 lev_utils.ExtrudeSurface(Level.Sectors[sc].Surfaces[sf],by);
 result:=level.Sectors.count-1;
end;

Function TJEDCOM.CleaveSurface(sc,sf:integer; const cnormal:TJEDvector; cx,cy,cz:double):integer;
begin
 if lev_utils.CleaveSurface(Level.Sectors[sc].Surfaces[sf],Tvector(Cnormal),cx,cy,cz) then
 result:=Level.Sectors[sc].Surfaces.count-1 else result:=-1;
end;

Function TJEDCOM.CleaveSector(sec:integer; const cnormal:TJEDvector; cx,cy,cz:double):integer;
begin
 if lev_utils.CleaveSector(Level.Sectors[sec],Tvector(Cnormal),cx,cy,cz) then
 result:=Level.Sectors.count-1 else result:=-1;
end;

Function TJEDCOM.CleaveEdge(sc,sf,ed:integer; const cnormal:TJEDvector; cx,cy,cz:double):boolean;
begin
 result:=lev_utils.CleaveEdge(Level.Sectors[sc].Surfaces[sf],ed,Tvector(Cnormal),cx,cy,cz);
end;

Function TJEDCOM.JoinSurfaces(sc1,sf1,sc2,sf2:Integer):boolean;
begin
 result:=lev_utils.ConnectSurfaces(Level.Sectors[sc1].Surfaces[sf1],
                              Level.Sectors[sc2].Surfaces[sf2]);
end;

Function TJEDCOM.PlanarizeSurface(sc,sf:integer):boolean;
begin
 result:=lev_utils.FlattenSurface(Level.Sectors[sc].Surfaces[sf]);
end;

Function TJEDCOM.MergeSurfaces(sc,sf1,sf2:integer):integer;
var surf:TJKSurface;
begin
 surf:=lev_utils.MergeSurfaces(Level.Sectors[sc].Surfaces[sf1],
                              Level.Sectors[sc].Surfaces[sf2]);
 if surf=nil then result:=-1 else result:=surf.num;
end;

Function TJEDCOM.MergeSectors(sec1,sec2:integer):integer;
var sec:TJKSector;
begin
 sec:=lev_utils.MergeSectors(Level.Sectors[sec1],
                              Level.Sectors[sec2]);
 if sec=nil then result:=-1 else result:=sec.num;
end;

Procedure TJEDCOM.CalculateDefaultUVNormals(sc,sf:integer; orgvx:integer; var un,vn:TJEDVector);
begin
 lev_utils.CalcDefaultUVNormals(Level.Sectors[sc].Surfaces[sf],
                                     orgvx,Tvector(un),Tvector(vn));
end;

Procedure TJEDCOM.CalcUVNormals(sc,sf:integer; var un,vn:TJEDVector);
begin
 lev_utils.CalcUVNormals(Level.Sectors[sc].Surfaces[sf],
                                     Tvector(un),Tvector(vn));
end;

Procedure TJEDCOM.ArrangeTexture(sc,sf:integer; orgvx:integer; const un,vn:TJEDvector);
begin
 lev_utils.ArrangeTexture(Level.Sectors[sc].Surfaces[sf],
                                     orgvx,Tvector(un),Tvector(vn));
end;

Procedure TJEDCOM.ArrangeTextureBy(sc,sf:integer;const un,vn:TJEDvector;refx,refy,refz,refu,refv:double);
begin
 lev_utils.ArrangeTextureBy(Level.Sectors[sc].Surfaces[sf],
                                     Tvector(un),Tvector(vn),refx,refy,refz,refu,refv);
end;

Function TJEDCOM.IsTextureFlipped(sc,sf:integer):boolean;
begin
 result:=lev_utils.IsTXFlipped(Level.Sectors[sc].surfaces[sf]);
end;

Procedure TJEDCOM.RemoveSurfaceReferences(sc,sf:integer);
begin
 lev_utils.RemoveSurfRefs(level,Level.Sectors[sc].surfaces[sf]);
end;

Procedure TJEDCOM.RemoveSectorReferences(sec:integer;surfs:boolean);
begin
 if surfs then lev_utils.RemoveSecRefs(level,Level.Sectors[sec],rs_surfs)
 else lev_utils.RemoveSecRefs(level,Level.Sectors[sec],0);
end;

Function TJEDCOM.StitchSurfaces(sc1,sf1,sc2,sf2:integer):boolean;
begin
 Result:=lev_utils.StitchSurfaces(Level.Sectors[sc1].surfaces[sf1],
                                 Level.Sectors[sc2].surfaces[sf2]);
end;

Function TJEDCOM.FindCommonEdges(sc1,sf1,sc2,sf2:integer; var v11,v12,v21,v22:integer):boolean;
begin
 Result:=lev_utils.FindCommonEdges(Level.Sectors[sc1].surfaces[sf1],
                                 Level.Sectors[sc2].surfaces[sf2],v11,v12,v21,v22);
end;

Function TJEDCOM.DoSurfacesOverlap(sc1,sf1,sc2,sf2:integer):boolean;
begin
 Result:=lev_utils.Do_Surf_Overlap(Level.Sectors[sc1].surfaces[sf1],
                                 Level.Sectors[sc2].surfaces[sf2]);
end;

Function TJEDCOM.MakeAdjoin(sc,sf:integer):boolean;
begin
 Result:=lev_utils.MakeAdjoin(Level.Sectors[sc].surfaces[sf]);
end;

Function TJEDCOM.MakeAdjoinFromSectorUp(sc,sf:integer;firstsc:integer):boolean;
begin
 Result:=lev_utils.MakeAdjoinSCUp(Level.Sectors[sc].surfaces[sf],
                                      firstsc);
end;

Function TJEDCOM.UnAdjoin(sc,sf:integer):Boolean;
begin
 Result:=lev_utils.UnAdjoin(Level.Sectors[sc].surfaces[sf]);
end;

Function TJEDCOM.CreateCubicSector(x,y,z:double;const pnormal,edge:TJEDVector):integer;
begin
 lev_utils.CreateCube(level,x,y,z,Tvector(pnormal),TVector(edge));
 result:=Level.sectors.count-1;
end;

Procedure TJEDCOM.StartUndo(name:pchar);
begin
 StartUndoRec(name);
end;

Procedure TJEDCOM.SaveUndoForThing(n:integer;change:integer);
begin
 case change of
  0: SaveThingUndo(Level.Things[n],ch_changed);
  1: SaveThingUndo(Level.Things[n],ch_added);
  2: SaveThingUndo(Level.Things[n],ch_deleted);
 end;
end;

Procedure TJEDCOM.SaveUndoForLight(n:integer;change:integer);
begin
 case change of
  0: SaveLightUndo(Level.Lights[n],ch_changed);
  1: SaveLightUndo(Level.Lights[n],ch_added);
  2: SaveLightUndo(Level.Lights[n],ch_deleted);
 end;
end;

Procedure TJEDCOM.SaveUndoForSector(n:integer;change:integer;whatpart:integer);
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

Procedure TJEDCOM.ClearUndoBuffer;
begin
 u_undo.ClearUndoBuffer;
end;

Procedure TJEDCOM.ApplyUndo;
begin
 u_undo.ApplyUndo;
end;

{0.92}

Function TJEDCOM.GetApplicationHandle:Integer;
begin
 result:=Application.Handle;
end;

Function TJEDCOM.JoinSectors(sec1,sec2:integer):boolean;
begin
 result:=lev_utils.ConnectSectors(Level.Sectors[sec1],level.Sectors[sec2]);
end;

Function TJEDCOM.GetNMultiselected(what:integer):integer;
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

Procedure TJEDCOM.ClearMultiselection(what:integer);
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

Procedure TJEDCOM.RemoveFromMultiselection(what,n:integer);
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

Function TJEDCOM.GetSelectedSC(n:integer):integer;
begin
 Result:=JedMain.scsel.GetSC(n);
end;

Function TJEDCOM.GetSelectedTH(n:integer):integer;
begin
 Result:=JedMain.thsel.GetTH(n);
end;

Function TJEDCOM.GetSelectedLT(n:integer):integer;
begin
 Result:=JedMain.ltsel.GetLT(n);
end;

Procedure TJEDCOM.GetSelectedSF(n:integer;var sc,sf:integer);
begin
 JedMain.sfsel.GetSCSF(n,sc,sf);
end;

Procedure TJEDCOM.GetSelectedED(n:integer;var sc,sf,ed:integer);
begin
 JedMain.edsel.GetSCSFED(n,sc,sf,ed);
end;

Procedure TJEDCOM.GetSelectedVX(n:integer;var sc,vx:integer);
begin
 JedMain.vxsel.GetSCVX(n,sc,vx);
end;

Procedure TJEDCOM.GetSelectedFR(n:integer;var th,fr:integer);
begin
 JedMain.frsel.GetTHFR(n,th,fr);
end;

Function TJEDCOM.SelectSC(sc:integer):integer;
begin
 Result:=Nmask and JedMain.scsel.AddSC(sc);
end;

Function TJEDCOM.SelectSF(sc,sf:integer):integer;
begin
 Result:=Nmask and JedMain.sfsel.AddSF(sc,sf);
end;

Function TJEDCOM.SelectED(sc,sf,ed:integer):integer;
begin
 Result:=Nmask and JedMain.edsel.AddED(sc,sf,ed);
end;

Function TJEDCOM.SelectVX(sc,vx:integer):integer;
begin
 Result:=Nmask and JedMain.vxsel.AddVX(sc,vx);
end;

Function TJEDCOM.SelectTH(th:integer):integer;
begin
 Result:=Nmask and JedMain.thsel.AddTH(th);
end;

Function TJEDCOM.SelectFR(th,fr:integer):integer;
begin
 Result:=Nmask and JedMain.frsel.AddFR(th,fr);
end;

Function TJEDCOM.SelectLT(lt:integer):integer;
begin
 Result:=Nmask and JedMain.ltsel.AddLT(lt);
end;

Function TJEDCOM.FindSelectedSC(sc:integer):integer;
begin
 Result:=JedMain.scsel.FindSC(sc);
end;

Function TJEDCOM.FindSelectedSF(sc,sf:integer):integer;
begin
 Result:=JedMain.sfsel.FindSF(sc,sf);
end;

Function TJEDCOM.FindSelectedED(sc,sf,ed:integer):integer;
begin
 Result:=JedMain.edsel.FindED(sc,sf,ed);
end;

Function TJEDCOM.FindSelectedVX(sc,vx:integer):integer;
begin
 Result:=JedMain.vxsel.FindVX(sc,vx);
end;

Function TJEDCOM.FindSelectedTH(th:integer):integer;
begin
 Result:=JedMain.thsel.FindTH(th);
end;

Function TJEDCOM.FindSelectedFR(th,fr:integer):integer;
begin
 Result:=JedMain.frsel.FindFR(th,fr);
end;

Function TJEDCOM.FindSelectedLT(lt:integer):integer;
begin
 Result:=JedMain.ltsel.FindLT(lt);
end;

Procedure TJEDCOM.SaveJED(name:pchar);
begin
 JedMain.SaveJedTo(name);
end;

Procedure TJEDCOM.SaveJKL(name:pchar);
begin
 JedMain.SaveJKLto(name);
end;

Procedure TJEDCOM.UpdateMap;
begin
 JedMain.Invalidate;
end;

Procedure TJEDCOM.SetPickerCMP(cmp:pchar);
begin
 ResPicker.SetCMP(cmp);
end;

Function TJEDCOM.PickResource(what:integer;cur:pchar):pchar;
const resstr:string='';
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
 result:=pchar(resstr);
 RestoreCurApp;
end;

Function TJEDCOM.EditFlags(what:integer;flags:LongInt):LongInt;
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

Procedure TJEDCOM.ReloadTemplates;
begin
 JedMain.LoadTemplates;
end;

Procedure TJEDCOM.PanMessage(mtype:integer;msg:pchar);
begin
 case mtype of
  0: misc_utils.PanMessage(mt_info,msg);
  1: misc_utils.PanMessage(mt_warning,msg);
  2: misc_utils.PanMessage(mt_error,msg);
 end;
end;

Procedure TJEDCOM.SendKey(shift:integer;key:integer);
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

Function TJEDCOM.ExecuteMenu(itemref:pchar):boolean;
var nmi,mi:TMenuItem;
    cname:string;
    p,cpos:integer;

Function GetNextItem(var s:string;pos:integer):integer;
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

Function TJEDCOM.GetJEDSetting(name:pchar):variant;
begin
 Result:=GetSetting(name);
end;

Function TJEDCOM.IsLayerVisible(n:integer):boolean;
begin
 Result:=ToolBar.IsLayerVisible(n);
end;

Procedure TJEDCOM.CheckConsistencyErrors;
begin
 Consistency.Check;
 Consistency.Hide;
end;

Procedure TJEDCOM.CheckResources;
begin
 Consistency.CheckResources;
 Consistency.Hide;
end;

Function TJEDCOM.NConsistencyErrors:integer;
begin
 result:=Consistency.NErrors;
end;

Function TJEDCOM.GetConsErrorString(n:integer):pchar;
begin
 result:=Pchar(Consistency.ErrorText(n));
end;

Function TJEDCOM.GetConsErrorType(n:integer):integer;
var obj:TConsistencyError;
begin
 result:=-1;
 obj:=Consistency.ErrorObject(n);
 if obj=nil then exit;
 result:=obj.etype;
end;

Function TJEDCOM.GetConsErrorSector(n:integer):integer;
var obj:TConsistencyError;
begin
 result:=-1;
 obj:=Consistency.ErrorObject(n);
 if obj.etype=et_sector then Result:=TJKSector(obj.itm).num
 else if obj.etype=et_surface then Result:=TJKSurface(obj.itm).sector.num;
end;

Function TJEDCOM.GetConsErrorSurface(n:integer):integer;
var obj:TConsistencyError;
begin
 result:=-1;
 obj:=Consistency.ErrorObject(n);
 if obj.etype=et_surface then Result:=TJKSurface(obj.itm).num;
end;

Function TJEDCOM.GetConsErrorThing(n:integer):integer;
var obj:TConsistencyError;
begin
 result:=-1;
 obj:=Consistency.ErrorObject(n);
 if obj.etype=et_thing then Result:=TJKThing(obj.itm).num;
end;

Function TJEDCOM.GetConsErrorCog(n:integer):integer;
var obj:TConsistencyError;
begin
 result:=-1;
 obj:=Consistency.ErrorObject(n);
 if obj.etype=et_cog then Result:=J_Level.Level.COGS.IndexOf(obj.itm);
end;

Function TJEDCOM.IsPreviewActive:boolean;
begin
 Result:=Preview3D.IsActive;
end;

Function TJEDCOM.GetJEDString(what:integer):pchar;
begin
case what of
 js_ProjectDir: result:=PChar(ProjectDir);
 js_JEDDir: result:=PChar(BaseDir);
 js_CDDir: result:=PChar(CDDir);
 js_GameDir: result:=PChar(GameDir);
 js_LevelFile: result:=PChar(JedMain.LevelFile);
 js_jedregkey: result:=PChar(RegBase);
 js_LECLogo: result:=PChar(LECLogo);
 js_recent1: result:=PChar(Recent1);
 js_recent2: result:=PChar(Recent2);
 js_recent3: result:=PChar(Recent3);
 js_recent4: result:=PChar(Recent4);
 js_res1gob: result:=PChar(Res1_Gob);
 js_res2gob: result:=PChar(Res2_Gob);
 js_spgob: result:=PChar(sp_Gob);
 js_mp1gob: result:=PChar(mp1_Gob);
 js_mp2gob: result:=PChar(mp2_Gob);
 js_mp3gob: result:=PChar(mp3_Gob);
else begin
      tmpstr[0]:=#0;
      result:=@tmpstr[0];
     end;
end;

end;


Function TJEDCOM.GetIsMOTS:boolean;
begin
 Result:=GlobalVars.IsMots;
end;

Procedure TJEDCOM.SetIsMOTS(mots:boolean);
begin
 SetMots(mots);
end;

Function TJEDCOM.GetJedWindow(whichone:integer):integer;
begin
case whichone of
 jw_Main: result:=JedMain.Handle;
 jw_ConsChecker: Result:=Consistency.Handle;
 jw_ItemEdit: Result:=ItemEdit.Handle;
 jw_PlacedCogs: Result:=CogForm.Handle;
 jw_MsgWindow: result:=MsgForm.Handle;
 jw_3DPreview: result:=Preview3D.Handle;
 jw_ToolWindow: result:=ToolForm.Handle;
else result:=0;
end;
end;

Function TJEDCOM.FileExtractExt(path:pchar):pchar;
begin
 atmpstr:=ExtractExt(path);
 result:=pchar(atmpstr);
end;

Function TJEDCOM.FileExtractPath(path:pchar):pchar;
begin
 atmpstr:=ExtractPath(path);
 result:=pchar(atmpstr);
end;

Function TJEDCOM.FileExtractName(path:pchar):pchar;
begin
 atmpstr:=ExtractName(path);
 result:=pchar(atmpstr);
end;

Function TJEDCOM.FindProjectDirFile(name:pchar):pchar;
begin
 atmpstr:=FindProjDirFile(name);
 result:=pchar(atmpstr);
end;

Function TJEDCOM.IsFileContainer(path:pchar):boolean;
begin
 result:=IsContainer(path);
end;

Function TJEDCOM.IsFileInContainer(path:pchar):Boolean;
begin
 result:=IsInContainer(path);
end;

Function TJEDCOM.FileOpenDialog(fname:pchar;filter:pchar):pchar;
begin
 SaveCurApp;
 GetFileOpen.FileName:=fname;
 GetFileOpen.Filter:=filter;
 if GetFileOpen.Execute then result:=pchar(GetFileOpen.FileName)
 else result:=nil;
 RestoreCurApp;
end;


Function TJEDCOM.OpenFile(name:pchar):integer;
begin
try
 result:=files.NewHandle(OpenFileRead(name,0));
except
 on Exception do result:=-1;
end;
end;

Function TJEDCOM.OpenGameFile(name:pchar):integer;
begin
try
 result:=files.NewHandle(FileOperations.OpenGameFile(name));
except
 on Exception do result:=-1;
end;
end;

Function TJEDCOM.ReadFile(handle:integer;buf:pointer;size:longint):integer;
begin
 result:=TFile(files.GetItemNoNIL(handle)).Fread(buf^,size);
end;

Procedure TJEDCOM. SetFilePos(handle:integer;pos:longint);
begin
 TFile(files.GetItemNoNIL(handle)).FSeek(pos);
end;

Function TJEDCOM.GetFilePos(handle:integer):longint;
begin
 Result:=TFile(files.GetItemNoNIL(handle)).FPos;
end;

Function TJEDCOM.GetFileSize(handle:integer):longint;
begin
 result:=TFile(files.GetItemNoNIL(handle)).FSize;
end;

Function TJEDCOM.GetFileName(handle:integer):pchar;
begin
 atmpstr:=TFile(files.GetItemNoNIL(handle)).GetFullName;
 result:=pchar(atmpstr);
end;

Procedure TJEDCOM.CloseFile(handle:integer);
begin
 files.FreeHandle(handle);
end;

Function TJEDCOM.OpenTextFile(name:pchar):integer;
begin
try
 result:=tfiles.NewHandle(TTextFile.CreateRead(OpenFileRead(name,0)));
except
 on Exception do result:=-1;
end;
end;

Function TJEDCOM.OpenGameTextFile(name:pchar):integer;
begin
try
 result:=tfiles.NewHandle(TTextFile.CreateRead(FileOperations.OpenGameFile(name)));
except
 on Exception do result:=-1;
end;
end;

Function TJEDCOM.GetTextFileName(handle:integer):pchar;
begin
 atmpstr:=TTextFile(tfiles.GetItemNoNIL(handle)).GetFullName;
 result:=pchar(atmpstr);
end;

Function TJEDCOM.ReadTextFileString(handle:integer):pchar;
begin
 TTextFile(tfiles.GetItemNoNIL(handle)).Readln(atmpstr);
 result:=pchar(atmpstr);
end;

Function TJEDCOM.TextFileEof(handle:integer):boolean;
begin
 result:=TTextFile(tfiles.GetItemNoNIL(handle)).eof;
end;

Function TJEDCOM.TextFileCurrentLine(handle:integer):integer;
begin
 result:=TTextFile(tfiles.GetItemNoNIL(handle)).CurrentLine;
end;

Procedure TJEDCOM.CloseTextFile(handle:integer);
begin
 tfiles.FreeHandle(handle);
end;

Function TJEDCOM.OpenGOB(name:pchar):integer;
begin
try
 result:=conts.NewHandle(OpenContainer(name));
except
 on Exception do result:=-1;
end;
end;

Function TJEDCOM.GOBNFiles(handle:integer):integer;
begin
 result:=TContainerFile(conts.GetItemNoNIL(handle)).Files.Count;
end;

Function TJEDCOM.GOBNFileName(handle:integer;n:integer):pchar;
begin
 result:=PChar(TContainerFile(conts.GetItemNoNIL(handle)).Files[n]);
end;

Function TJEDCOM.GOBNFullFileName(handle:integer;n:integer):pchar;
var cf:TContainerFile;
begin
 cf:=TContainerFile(conts.GetItemNoNIL(handle));
 atmpstr:=cf.Name+'>'+cf.Files[n];
 result:=PChar(atmpstr);
end;

Function TJEDCOM.GOBGetFileOffset(handle:integer;n:integer):longint;
begin
 result:=TFileInfo(TContainerFile(conts.GetItemNoNIL(handle)).Files.Objects[n]).offs;
end;

Function TJEDCOM.GOBGetFileSize(handle:integer;n:integer):longint;
begin
 result:=TFileInfo(TContainerFile(conts.GetItemNoNIL(handle)).Files.Objects[n]).size;
end;

Procedure TJEDCOM.CloseGOB(handle:integer);
begin
 conts.FreeHandle(handle);
end;

Function TJEDCOM.CreateWFRenderer(wnd:integer;whichone:integer):IJEDWFRenderer;
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

Procedure TCOMWFRenderer. GetRendererVector(what:integer;var x,y,z:double);
begin
 With Rend do
 case what of
  rv_CamPos: begin x:=CamX; y:=CamY; z:=CamZ; end;
  rv_GridPos: begin x:=GridX; y:=GridY; z:=GridZ; end;
  rv_CamXAxis: GetVec(xv,x,y,z);
  rv_CamYAxis: GetVec(yv,x,y,z);
  rv_CamZAxis: GetVec(zv,x,y,z);
  rv_GridXAxis: GetVec(gxnormal,x,y,z);
  rv_GridYAxis: GetVec(gynormal,x,y,z);
  rv_GridZAxis: GetVec(gnormal,x,y,z);
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

function TCOMWFRenderer.GetXYZonPlaneAt(scX,scY:integer;pnormal:TJedVector; pX,pY,pZ:double; var X,Y,Z:double):Boolean;
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
