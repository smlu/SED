unit jed_plugins2;

interface
uses OLE2;

const
   CloseEnough=10e-5;
   {Get/SetLevelHeader flags}
   lh_version=1;
   lh_gravity=2;
   lh_skyZ=4;
   lh_cSkyOffs=8;
   lh_HorDist=$10;
   lh_HorPPR=$20;
   lh_HSkyOffs=$40;
   lh_MipMapDist=$80;
   lh_LODDist=$100;
   lh_PerspDist=$200;
   lh_GouraudDist=$400;
   lh_ppu=$800;
   lh_MasterCMP=$1000;
   lh_all=$1FFF;

   {Get/SetSector flags}
   s_flags=1;
   s_ambient=2;
   s_extra=4;
   s_cmp=8;
   s_tint=$10;
   s_sound=$20;
   s_sndvol=$40;
   s_layer=$80;
   s_all=$FF;

   {Get/SetSurface flags}

   sf_adjoin=1;
   sf_adjoinflags=2;
   sf_SurfFlags=4;
   sf_FaceFlags=8;
   sf_Material=$10;
   sf_geo=$20;
   sf_light=$40;
   sf_tex=$80;
   sf_ExtraLight=$100;
   sf_txscale=$200;
   sf_all=$3FF;

   {Get/SetThing flags}
   th_name=1;
   th_sector=2;
   th_position=4;
   th_orientation=8;
   th_layer=$10;
   th_all=$1F;

   {Get/SetLight flags}
   lt_position=1;
   lt_intensity=2;
   lt_range=4;
   lt_rgb=8;
   lt_rgbintensity=$10;
   lt_rgbrange=$20;
   lt_flags=$40;
   lt_layer=$80;
   lt_all=$FF;

   {Map Mode constants}
    MM_SC=0; {Sectors}
    MM_SF=1; {surfaces}
    MM_VX=2; {vertices}
    MM_TH=3; {Things}
    MM_ED=4; {Edges}
    MM_LT=5; {Lights}
    MM_FR=6; {frames}

   {SurfaceUpdate "how" constant}
   su_texture=1;
   su_floorflag=2;
   su_sector=4;
   su_all=7;

   {Undo change constants}

   uc_changed=0;
   uc_added=1;
   uc_deleted=2;
   sc_values=1;
   sc_geometry=2;

   {COG value types}
   ct_unknown=0;
   ct_ai=1;
   ct_cog=2;
   ct_key=3;
   ct_mat=4;
   ct_msg=5;
   ct_3do=6;
   ct_sec=7;
   ct_wav=8;
   ct_surface=9;
   ct_template=10;
   ct_thing=11;
   ct_int=12;
   ct_flex=13;
   ct_vector=14;

{PickResource constants}
   pr_thing=1;
   pr_template=2;
   pr_cmp=3;
   pr_secsound=4;
   pr_mat=5;
   pr_cog=6;
   pr_layer=7;
   pr_3do=8;
   pr_ai=9;
   pr_key=10;
   pr_snd=11;
   pr_pup=12;
   pr_spr=13;
   pr_par=14;
   pr_per=15;
   pr_jklsmksan=16;

{EditFlags constants}
   ef_sector=1;
   ef_surface=2;
   ef_adjoin=3;
   ef_thing=4;
   ef_face=5;
   ef_light=6;
   ef_geo=7;
   ef_lightmode=8;
   ef_tex=9;

{PanMessage constants}
   msg_info=0;
   msg_warning=1;
   msg_error=2;

{SendKey flags}
   sk_Ctrl=1;
   sk_Shift=2;
   sk_Alt=4;

{GetConsErrorType constants}
   ce_none=0;
   ce_sector=1;
   ce_surface=2;
   ce_thing=3;
   ce_cog=4;

{GetJEDString constants}
js_ProjectDir=1;
js_JEDDir=2;
js_CDDir=3;
js_GameDir=4;
js_LevelFile=5;
js_jedregkey=6;
js_LECLogo=7;
js_recent1=8;
js_recent2=9;
js_recent3=10;
js_recent4=11;
js_res1gob=12;
js_res2gob=13;
js_spgob=14;
js_mp1gob=15;
js_mp2gob=16;
js_mp3gob=17;


type

 TJEDVector=record
 case integer of
  0: (dx,dy,dz:double);
  1: (x,y,z:double);
 end;

TJEDBox=record
 X1,Y1,Z1,
 X2,Y2,Z2:double;
end;

 TJEDSurfaceVertex=record
  u,v:single;
  intensity,r,g,b:single;
 end;

 TLevelHeader=record
  version:Longint;
  Gravity:double;
  CeilingSkyZ:double;
  CeilingSkyOffs:array[0..1] of double;
  HorDistance,HorPixelsPerRev:double;
  HorSkyOffs:array[0..1] of double;
  MipMapDist:array[0..3] of double;
  LODDist:array[0..3] of double;
  PerspDist, GouraudDist:double;
  PixelPerUnit:double;
  MasterCMP:pchar;
 end;

 TJEDSectorRec=record
  Flags:longint;
  Ambient:double;
  extra:double;
  ColorMap:pchar;
  Tint_r,
  Tint_g,
  Tint_b:single;
  Sound:pchar;
  snd_vol:double;
  layer:longint;
 end;

 TJEDSurfaceRec=record
  adjoinSC,AdjoinSF:longint;
  AdjoinFlags:Longint;
  SurfFlags,FaceFlags:Longint;
  Material:PChar;
  geo,light,tex:longint;
  ExtraLight:double;
  uscale,vscale:single;
 end;

 TJEDThingRec=record
  name:Pchar;
  Sector:Longint;
  X,Y,Z:double;
  PCH,YAW,ROL:Double;
  layer:longint;
 end;


 TJEDLightRec=record
  X,Y,Z:double;
  Intensity:double;
  Range:double;
  R,G,B:Single;
  rgbintensity:single;
  rgbrange:double;
  flags:longint;
  layer:longint;
 end;

{endtype}

 IJEDLevel=class(IUnknown)
  {Level header}
  Procedure GetLevelHeader(var lh:TLevelHeader;flags:integer);virtual;stdcall;abstract;
  Procedure SetLevelHeader(const lh:TLevelHeader;flags:integer);virtual;stdcall;abstract;

  Function NSectors:integer;virtual;stdcall;abstract;
  Function NThings:integer;virtual;stdcall;abstract;
  Function NLights:integer;virtual;stdcall;abstract;
  Function NCOgs:integer;virtual;stdcall;abstract;

  {Sectors}
  Function AddSector:integer;virtual;stdcall;abstract;
  Procedure DeleteSector(n:integer);virtual;stdcall;abstract;

  Procedure GetSector(sec:integer;var rec:TJEDSectorRec;flags:integer);virtual;stdcall;abstract;
  Procedure SetSector(sec:integer;const rec:TJEDSectorRec;flags:integer);virtual;stdcall;abstract;

  Function SectorNVertices(sec:integer):integer;virtual;stdcall;abstract;
  Function SectorNSurfaces(sec:integer):integer;virtual;stdcall;abstract;

  Procedure SectorGetVertex(sec,vx:integer;var x,y,z:double);virtual;stdcall;abstract;
  Procedure SectorSetVertex(sec,vx:integer;x,y,z:double);virtual;stdcall;abstract;

  Function SectorAddVertex(sec:integer;x,y,z:double):integer;virtual;stdcall;abstract;
  Function SectorFindVertex(sec:integer;x,y,z:double):integer;virtual;stdcall;abstract;
  Function SectorDeleteVertex(sec:integer;n:integer):integer;virtual;stdcall;abstract;

  Function SectorAddSurface(sec:integer):integer;virtual;stdcall;abstract;
  Procedure SectorDeleteSurface(sc,sf:integer);virtual;stdcall;abstract;
  Procedure SectorUpdate(sec:integer);virtual;stdcall;abstract;

  {Surfaces}
  Procedure GetSurface(sc,sf:integer;var rec:TJEDSurfaceRec;flags:integer);virtual;stdcall;abstract;
  Procedure SetSurface(sc,sf:integer;const rec:TJEDSurfaceRec;flags:integer);virtual;stdcall;abstract;
  Procedure GetSurfaceNormal(sc,sf:integer;var n:TJEDVector);virtual;stdcall;abstract;
  Procedure SurfaceUpdate(sc,sf:integer;how:integer);virtual;stdcall;abstract;
  Function SurfaceNVertices(sc,sf:integer):Integer;virtual;stdcall;abstract;
  Function SurfaceGetVertexNum(sc,sf,vx:integer):integer;virtual;stdcall;abstract;
  Procedure SurfaceSetVertexNum(sc,sf,vx:integer;secvx:integer);virtual;stdcall;abstract;
  Function SurfaceAddVertex(sc,sf:integer;secvx:integer):Integer;virtual;stdcall;abstract;
  Function SurfaceInsertVertex(sc,sf:integer;at:integer;secvx:integer):Integer;virtual;stdcall;abstract;
  Procedure SurfaceDeleteVertex(sc,sf:integer;n:integer);virtual;stdcall;abstract;
  Procedure SurfaceGetVertexUV(sc,sf,vx:integer;var u,v:single);virtual;stdcall;abstract;
  Procedure SurfaceSetVertexUV(sc,sf,vx:integer;u,v:single);virtual;stdcall;abstract;
  Procedure SurfaceGetVertexLight(sc,sf,vx:integer;var white,r,g,b:single);virtual;stdcall;abstract;
  Procedure SurfaceSetVertexLight(sc,sf,vx:integer;white,r,g,b:single);virtual;stdcall;abstract;

  {things}
  Function AddThing:Integer;virtual;stdcall;abstract;
  Procedure DeleteThing(th:integer);virtual;stdcall;abstract;
  Procedure GetThing(th:integer;var rec:TJEDThingRec;flags:integer);virtual;stdcall;abstract;
  Procedure SetThing(th:integer;const rec:TJEDThingRec;flags:integer);virtual;stdcall;abstract;
  Procedure ThingUpdate(th:integer);virtual;stdcall;abstract;

  {Lights}
  Function AddLight:Integer;virtual;stdcall;abstract;
  Procedure DeleteLight(lt:integer);virtual;stdcall;abstract;
  Procedure GetLight(lt:integer;var rec:TJEDLightRec;flags:integer);virtual;stdcall;abstract;
  Procedure SetLight(lt:integer;const rec:TJEDLightRec;flags:integer);virtual;stdcall;abstract;

  {Layers}
  Function NLayers:integer;virtual;stdcall;abstract;
  Function GetLayerName(n:integer):pchar;virtual;stdcall;abstract;
  Function AddLayer(name:Pchar):integer;virtual;stdcall;abstract;

  {0.92}

  Function NTHingValues(th:integer):integer;virtual;stdcall;abstract;
  Function GetThingValueName(th,n:Integer):pchar;virtual;stdcall;abstract;
  Function GetThingValueData(th,n:integer):pchar;virtual;stdcall;abstract;
  Procedure SetThingValueData(th,n:Integer;val:pchar);virtual;stdcall;abstract;

  Procedure GetThingFrame(th,n:Integer;var x,y,z,pch,yaw,rol:double);virtual;stdcall;abstract;
  Procedure SetThingFrame(th,n:Integer;x,y,z,pch,yaw,rol:double);virtual;stdcall;abstract;

  Function AddThingValue(th:integer;name,val:pchar):integer;virtual;stdcall;abstract;
  Procedure InsertThingValue(th,n:Integer;name,val:pchar);virtual;stdcall;abstract;
  Procedure DeleteThingValue(th,n:integer);virtual;stdcall;abstract;

  {COGs}
  Function AddCOG(name:pchar):Integer;virtual;stdcall;abstract;
  Procedure DeleteCOG(n:integer);virtual;stdcall;abstract;
  Procedure UpdateCOG(cg:integer);virtual;stdcall;abstract;

  Function GetCOGFileName(cg:integer):pchar;virtual;stdcall;abstract;
  Function NCOGValues(cg:integer):integer;virtual;stdcall;abstract;

  Function GetCOGValueName(cg,n:Integer):pchar;virtual;stdcall;abstract;
  Function GetCOGValueType(cg,n:Integer):integer;virtual;stdcall;abstract;

  Function GetCOGValue(cg,n:Integer):pchar;virtual;stdcall;abstract;
  Function SetCOGValue(cg,n:Integer;val:pchar):boolean;virtual;stdcall;abstract;

  Function AddCOGValue(cg:integer;name,val:pchar;vtype:integer):integer;virtual;stdcall;abstract;
  Procedure InsertCOGValue(cg,n:Integer;name,val:pchar;vtype:integer);virtual;stdcall;abstract;
  Procedure DeleteCOGValue(cg,n:integer);virtual;stdcall;abstract;

 end;

 IJED=class(IUnknown)
  Function GetVersion:double;virtual;stdcall;abstract; {version}
  Function GetLevel:IJEDLevel;virtual;stdcall;abstract; {JEDApp OLE interafce}

  Function GetMapMode:Integer;virtual;stdcall;abstract;
  Procedure SetMapMode(mode:integer);virtual;stdcall;abstract;
  Function GetCurSC:integer;virtual;stdcall;abstract;
  Procedure SetCurSC(sc:integer);virtual;stdcall;abstract;
  Function GetCurTH:integer;virtual;stdcall;abstract;
  Procedure SetCurTH(th:integer);virtual;stdcall;abstract;
  Function GetCurLT:integer;virtual;stdcall;abstract;
  Procedure SetCurLT(lt:integer);virtual;stdcall;abstract;
  Procedure GetCurVX(var sc,vx:integer);virtual;stdcall;abstract;
  Procedure SetCurVX(sc,vx:integer);virtual;stdcall;abstract;
  Procedure GetCurSF(var sc,sf:integer);virtual;stdcall;abstract;
  Procedure SetCurSF(sc,sf:integer);virtual;stdcall;abstract;
  Procedure GetCurED(var sc,sf,ed:integer);virtual;stdcall;abstract;
  Procedure SetCurED(sc,sf,ed:integer);virtual;stdcall;abstract;
  Procedure GetCurFR(var th,fr:integer);virtual;stdcall;abstract;
  Procedure SetCurFR(th,fr:integer);virtual;stdcall;abstract;

  Procedure NewLevel(mots:boolean);virtual;stdcall;abstract;
  Procedure LoadLevel(name:pchar);virtual;stdcall;abstract;

  {Different level editing functions}

  Procedure FindBBox(sec:integer;var box:TJEDBox);virtual;stdcall;abstract;
  Procedure FindBoundingSphere(sec:integer;var CX,CY,CZ,Radius:double);virtual;stdcall;abstract;
  Function FindCollideBox(sec:integer;const bbox:TJEDBox;cx,cy,cz:double;var cbox:TJEDBox):boolean;virtual;stdcall;abstract;
  Procedure FindSurfaceCenter(sc,sf:integer;var cx,cy,cz:double);virtual;stdcall;abstract;
  Procedure RotateVector(var vec:TJEDVector; pch,yaw,rol:double);virtual;stdcall;abstract;
  Procedure RotatePoint(ax1,ay1,az1,ax2,ay2,az2:double;angle:double;var x,y,z:double);virtual;stdcall;abstract;
  Procedure GetJKPYR(const x,y,z:TJEDVector;var pch,yaw,rol:double);virtual;stdcall;abstract;
  Function IsSurfaceConvex(sc,sf:integer):boolean;virtual;stdcall;abstract;
  Function IsSurfacePlanar(sc,sf:integer):boolean;virtual;stdcall;abstract;
  Function IsSectorConvex(sec:integer):boolean;virtual;stdcall;abstract;
  Function IsInSector(sec:integer;x,y,z:double):boolean;virtual;stdcall;abstract;
  Function DoSectorsOverlap(sec1,sec2:integer):boolean;virtual;stdcall;abstract;
  Function IsPointOnSurface(sc,sf:integer;x,y,z:double):boolean;virtual;stdcall;abstract;
  Function FindSectorForThing(th:integer):Integer;virtual;stdcall;abstract;
  Function FindSectorForXYZ(X,Y,Z:double):integer;virtual;stdcall;abstract;
  Function ExtrudeSurface(sc,sf:integer; by:double):integer;virtual;stdcall;abstract;
  Function CleaveSurface(sc,sf:integer; const cnormal:TJEDvector; cx,cy,cz:double):integer;virtual;stdcall;abstract;
  Function CleaveSector(sec:integer; const cnormal:TJEDvector; cx,cy,cz:double):integer;virtual;stdcall;abstract;
  Function CleaveEdge(sc,sf,ed:integer; const cnormal:TJEDvector; cx,cy,cz:double):boolean;virtual;stdcall;abstract;
  Function JoinSurfaces(sc1,sf1,sc2,sf2:Integer):boolean;virtual;stdcall;abstract;
  Function PlanarizeSurface(sc,sf:integer):boolean;virtual;stdcall;abstract;
  function MergeSurfaces(sc,sf1,sf2:integer):integer;virtual;stdcall;abstract;
  function MergeSectors(sec1,sec2:integer):integer;virtual;stdcall;abstract;
  Procedure CalculateDefaultUVNormals(sc,sf:integer; orgvx:integer; var un,vn:TJEDVector);virtual;stdcall;abstract;
  Procedure CalcUVNormals(sc,sf:integer; var un,vn:TJEDVector);virtual;stdcall;abstract;
  Procedure ArrangeTexture(sc,sf:integer; orgvx:integer; const un,vn:TJEDvector);virtual;stdcall;abstract;
  Procedure ArrangeTextureBy(sc,sf:integer;const un,vn:TJEDvector;refx,refy,refz,refu,refv:double);virtual;stdcall;abstract;
  Function IsTextureFlipped(sc,sf:integer):boolean;virtual;stdcall;abstract;
  Procedure RemoveSurfaceReferences(sc,sf:integer);virtual;stdcall;abstract;
  Procedure RemoveSectorReferences(sec:integer;surfs:boolean);virtual;stdcall;abstract;
  Function StitchSurfaces(sc1,sf1,sc2,sf2:integer):boolean;virtual;stdcall;abstract;
  Function FindCommonEdges(sc1,sf1,sc2,sf2:integer; var v11,v12,v21,v22:integer):boolean;virtual;stdcall;abstract;
  Function DoSurfacesOverlap(sc1,sf1,sc2,sf2:integer):boolean;virtual;stdcall;abstract;
  Function MakeAdjoin(sc,sf:integer):boolean;virtual;stdcall;abstract;
  Function MakeAdjoinFromSectorUp(sc,sf:integer;firstsc:integer):boolean;virtual;stdcall;abstract;
  Function UnAdjoin(sc,sf:integer):Boolean;virtual;stdcall;abstract;
  Function CreateCubicSector(x,y,z:double;const pnormal,edge:TJEDVector):integer;virtual;stdcall;abstract;

  Procedure StartUndo(name:pchar);virtual;stdcall;abstract;
  Procedure SaveUndoForThing(n:integer;change:integer);virtual;stdcall;abstract;
  Procedure SaveUndoForLight(n:integer;change:integer);virtual;stdcall;abstract;
  Procedure SaveUndoForSector(n:integer;change:integer;whatpart:integer);virtual;stdcall;abstract;
  Procedure ClearUndoBuffer;virtual;stdcall;abstract;
  Procedure ApplyUndo;virtual;stdcall;abstract;

  {Added in 0.92}
  Function GetApplicationHandle:Integer;virtual;stdcall;abstract;
  Function JoinSectors(sec1,sec2:integer):boolean;virtual;stdcall;abstract;

  Function GetNMultiselected(what:integer):integer;virtual;stdcall;abstract;
  Procedure ClearMultiselection(what:integer);virtual;stdcall;abstract;
  Procedure RemoveFromMultiselection(what,n:integer);virtual;stdcall;abstract;
  Function GetSelectedSC(n:integer):integer;virtual;stdcall;abstract;
  Function GetSelectedTH(n:integer):integer;virtual;stdcall;abstract;
  Function GetSelectedLT(n:integer):integer;virtual;stdcall;abstract;

  procedure GetSelectedSF(n:integer;var sc,sf:integer);virtual;stdcall;abstract;
  procedure GetSelectedED(n:integer;var sc,sf,ed:integer);virtual;stdcall;abstract;
  procedure GetSelectedVX(n:integer;var sc,vx:integer);virtual;stdcall;abstract;
  procedure GetSelectedFR(n:integer;var th,fr:integer);virtual;stdcall;abstract;

  Function SelectSC(sc:integer):integer;virtual;stdcall;abstract;
  Function SelectSF(sc,sf:integer):integer;virtual;stdcall;abstract;
  Function SelectED(sc,sf,ed:integer):integer;virtual;stdcall;abstract;
  Function SelectVX(sc,vx:integer):integer;virtual;stdcall;abstract;
  Function SelectTH(th:integer):integer;virtual;stdcall;abstract;
  Function SelectFR(th,fr:integer):integer;virtual;stdcall;abstract;
  Function SelectLT(lt:integer):integer;virtual;stdcall;abstract;

  Function FindSelectedSC(sc:integer):integer;virtual;stdcall;abstract;
  Function FindSelectedSF(sc,sf:integer):integer;virtual;stdcall;abstract;
  Function FindSelectedED(sc,sf,ed:integer):integer;virtual;stdcall;abstract;
  Function FindSelectedVX(sc,vx:integer):integer;virtual;stdcall;abstract;
  Function FindSelectedTH(th:integer):integer;virtual;stdcall;abstract;
  Function FindSelectedFR(th,fr:integer):integer;virtual;stdcall;abstract;
  Function FindSelectedLT(lt:integer):integer;virtual;stdcall;abstract;


  Procedure SaveJED(name:pchar);virtual;stdcall;abstract;
  Procedure SaveJKL(name:pchar);virtual;stdcall;abstract;
  Procedure UpdateMap;virtual;stdcall;abstract;
  Procedure SetPickerCMP(cmp:pchar);virtual;stdcall;abstract;
  Function PickResource(what:integer;cur:pchar):pchar;virtual;stdcall;abstract;
  Function EditFlags(what:integer;flags:LongInt):LongInt;virtual;stdcall;abstract;

  Procedure ReloadTemplates;virtual;stdcall;abstract;
  Procedure PanMessage(mtype:integer;msg:pchar);virtual;stdcall;abstract;
  Procedure SendKey(shift:integer;key:integer);virtual;stdcall;abstract;
  Function ExecuteMenu(itemref:pchar):boolean;virtual;stdcall;abstract;
  Function GetJEDSetting(name:pchar):variant;virtual;stdcall;abstract;
  Function IsLayerVisible(n:integer):boolean;virtual;stdcall;abstract;

  procedure CheckConsistencyErrors;virtual;stdcall;abstract;
  procedure CheckResources;virtual;stdcall;abstract;
  Function NConsistencyErrors:integer;virtual;stdcall;abstract;
  Function GetConsErrorString(n:integer):pchar;virtual;stdcall;abstract;
  Function GetConsErrorType(n:integer):integer;virtual;stdcall;abstract;
  Function GetConsErrorSector(n:integer):integer;virtual;stdcall;abstract;
  Function GetConsErrorSurface(n:integer):integer;virtual;stdcall;abstract;
  Function GetConsErrorThing(n:integer):integer;virtual;stdcall;abstract;
  Function GetConsErrorCog(n:integer):integer;virtual;stdcall;abstract;
  Function IsPreviewActive:boolean;virtual;stdcall;abstract;
  Function GetJEDString(what:integer):pchar;virtual;stdcall;abstract;
  Function GetIsMOTS:boolean;virtual;stdcall;abstract;
  Procedure SetIsMOTS(mots:boolean);virtual;stdcall;abstract;


 end;


{cvtstop}


 {The function that JED calls to load your plug-in
 must be named JEDPluginLoad (register calling) or
 JEDPluginLoadStdCall (standard stack calling)
 and have one parameter IJED interface object. The
 function should return true if the plug-in was
 loaded/activated successfully and false otherwise.
 Warning! Names are case-sensitive}


// This is how the Plug-in entry function should be declared
// Function TJEDPluginLoadStdCall(jed:IJED):boolean;stdcall;
// The old style:
// Function TJEDPluginLoad(jed:IJED):boolean;
// is still supported, but not recommended

TJEDPluginLoad=Function(jed:IJED):boolean;
TJEDPluginLoadStdCall=Function(jed:IJED):boolean;stdcall;


{General purpose functions. Local}

Procedure SetVector(var vec:TJEDVector;dx,dy,dz:double);
Function DotProduct(const vec1,vec2:TJEDVector):double;
Procedure CrossProduct(const vec1,vec2:TJEDVector;var vec:TJEDVector);
Function VectorLen(const vec:TJEDVector):double;
Function NormalizeVector(var vec:TJEDVector):boolean;
Function DoBoxesIntersect(const box1,box2:TJEDBox):boolean;
Procedure SetBox(var box:TJEDBox; x1,x2,y1,y2,z1,z2:double);
Function IsPointInBox(const box:TJEDBox; x,y,z:double):boolean;
Function LinePlaneIntersection(const normal:TJEDVector;pX,pY,pZ,x1,y1,z1,x2,y2,z2:double;var x,y,z:double):boolean;
Function MinDouble(d1,d2:double):double;

implementation

Function MinDouble(d1,d2:double):double;
begin
 if d1<d2 then result:=d1 else result:=d2;
end;

Procedure SetVector(var vec:TJEDVector;dx,dy,dz:double);
begin
 vec.dx:=dx;
 vec.dy:=dy;
 vec.dz:=dz;
end;

Function DotProduct(const vec1,vec2:TJEDVector):double;
begin
 result:=vec1.dx*vec2.dx+vec1.dy*vec2.dy+vec1.dz*vec2.dz;
end;

Procedure CrossProduct(const vec1,vec2:TJEDVector;var vec:TJEDVector);
begin
 vec.dx:=vec1.dy*vec2.dz-vec2.dy*vec1.dz;
 vec.dy:=vec2.dx*vec1.dz-vec1.dx*vec2.dz;
 vec.dz:=vec1.dx*vec2.dy-vec2.dx*vec1.dy;
end;

Function VectorLen(const vec:TJEDVector):double;
begin
 result:=sqrt(sqr(vec.dx)+sqr(vec.dy)+sqr(vec.dz));
end;

Function NormalizeVector(var vec:TJEDVector):boolean;
var len:double;
begin
 Result:=false;
 len:=VectorLen(vec);
 if len=0 then exit;
 vec.dx:=vec.dx/len;
 vec.dy:=vec.dy/len;
 vec.dz:=vec.dz/len;
 Result:=true;
end;

Function DoBoxesIntersect(const box1,box2:TJEDBox):boolean;
begin
 result:=false;
 if (box1.x2<box2.x1)then exit;
 if (box1.x1>box2.x2) then exit;
 if (box1.y2<box2.y1)then exit;
 if (box1.y1>box2.y2) then exit;
 if (box1.z2<box2.z1)then exit;
 if (box1.z1>box2.z2) then exit;
 result:=true;
end;

Procedure SetBox(var box:TJEDBox; x1,x2,y1,y2,z1,z2:double);
begin
 box.x1:=MinDouble(x1,x2);
 box.x2:=MinDouble(x1,x2);
 box.y1:=MinDouble(y1,y2);
 box.y2:=MinDouble(y1,y2);
 box.z1:=MinDouble(z1,z2);
 box.z2:=MinDouble(z1,z2);
end;

Function IsPointInBox(const box:TJEDBox; x,y,z:double):boolean;
begin
 result:=(x-box.x1>=-CloseEnough) and (x-box.x2<=CloseEnough) and
         (y-box.y1>=-CloseEnough) and (y-box.y2<=CloseEnough) and
         (z-box.z1>=-CloseEnough) and (z-box.z2<=CloseEnough);
end;

Function LinePlaneIntersection(const normal:TJEDVector;pX,pY,pZ,x1,y1,z1,x2,y2,z2:double;var x,y,z:double):boolean;
var
    v1,v2:TJEDVector;
    dist1,dist2:double;
    k:double;
begin
 result:=false;
 SetVector(v1,x1-px,y1-py,z1-pz);
 SetVector(v2,x2-px,y2-py,z2-pz);

 dist1:=DotProduct(v1,normal);
 dist2:=DotProduct(v2,normal);
 if dist1=dist2 then exit;

 k:=dist1/(dist1-dist2);

 x:=x1+k*(x2-x1);
 y:=y1+k*(y2-y1);
 z:=z1+k*(z2-z1);
 result:=true;
end;


end.
