unit jed_plugins3;

interface

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

IJEDLevel=interface(IUnknown)
  {Level header}
  Procedure GetLevelHeader(var lh:TLevelHeader;flags:integer);stdcall;
  Procedure SetLevelHeader(const lh:TLevelHeader;flags:integer);stdcall;

  Function NSectors:integer;stdcall;
  Function NThings:integer;stdcall;
  Function NLights:integer;stdcall;
  Function NCOgs:integer;stdcall;

  {Sectors}
  Function AddSector:integer;stdcall;
  Procedure DeleteSector(n:integer);stdcall;

  Procedure GetSector(sec:integer;var rec:TJEDSectorRec;flags:integer);stdcall;
  Procedure SetSector(sec:integer;const rec:TJEDSectorRec;flags:integer);stdcall;

  Function SectorNVertices(sec:integer):integer;stdcall;
  Function SectorNSurfaces(sec:integer):integer;stdcall;

  Procedure SectorGetVertex(sec,vx:integer;var x,y,z:double);stdcall;
  Procedure SectorSetVertex(sec,vx:integer;x,y,z:double);stdcall;

  Function SectorAddVertex(sec:integer;x,y,z:double):integer;stdcall;
  Function SectorFindVertex(sec:integer;x,y,z:double):integer;stdcall;
  Function SectorDeleteVertex(sec:integer;n:integer):integer;stdcall;

  Function SectorAddSurface(sec:integer):integer;stdcall;
  Procedure SectorDeleteSurface(sc,sf:integer);stdcall;
  Procedure SectorUpdate(sec:integer);stdcall;

  {Surfaces}
  Procedure GetSurface(sc,sf:integer;var rec:TJEDSurfaceRec;flags:integer);stdcall;
  Procedure SetSurface(sc,sf:integer;const rec:TJEDSurfaceRec;flags:integer);stdcall;
  Procedure GetSurfaceNormal(sc,sf:integer;var n:TJEDVector);stdcall;
  Procedure SurfaceUpdate(sc,sf:integer;how:integer);stdcall;
  Function SurfaceNVertices(sc,sf:integer):Integer;stdcall;
  Function SurfaceGetVertexNum(sc,sf,vx:integer):integer;stdcall;
  Procedure SurfaceSetVertexNum(sc,sf,vx:integer;secvx:integer);stdcall;
  Function SurfaceAddVertex(sc,sf:integer;secvx:integer):Integer;stdcall;
  Function SurfaceInsertVertex(sc,sf:integer;at:integer;secvx:integer):Integer;stdcall;
  Procedure SurfaceDeleteVertex(sc,sf:integer;n:integer);stdcall;
  Procedure SurfaceGetVertexUV(sc,sf,vx:integer;var u,v:single);stdcall;
  Procedure SurfaceSetVertexUV(sc,sf,vx:integer;u,v:single);stdcall;
  Procedure SurfaceGetVertexLight(sc,sf,vx:integer;var white,r,g,b:single);stdcall;
  Procedure SurfaceSetVertexLight(sc,sf,vx:integer;white,r,g,b:single);stdcall;

  {things}
  Function AddThing:Integer;stdcall;
  Procedure DeleteThing(th:integer);stdcall;
  Procedure GetThing(th:integer;var rec:TJEDThingRec;flags:integer);stdcall;
  Procedure SetThing(th:integer;const rec:TJEDThingRec;flags:integer);stdcall;
  Procedure ThingUpdate(th:integer);stdcall;

  {Lights}
  Function AddLight:Integer;stdcall;
  Procedure DeleteLight(lt:integer);stdcall;
  Procedure GetLight(lt:integer;var rec:TJEDLightRec;flags:integer);stdcall;
  Procedure SetLight(lt:integer;const rec:TJEDLightRec;flags:integer);stdcall;

  {Layers}
  Function NLayers:integer;stdcall;
  Function GetLayerName(n:integer):pchar;stdcall;
  Function AddLayer(name:Pchar):integer;stdcall;

  {0.92}

  Function NTHingValues(th:integer):integer;stdcall;
  Function GetThingValueName(th,n:Integer):pchar;stdcall;
  Function GetThingValueData(th,n:integer):pchar;stdcall;
  Procedure SetThingValueData(th,n:Integer;val:pchar);stdcall;

  Procedure GetThingFrame(th,n:Integer;var x,y,z,pch,yaw,rol:double);stdcall;
  Procedure SetThingFrame(th,n:Integer;x,y,z,pch,yaw,rol:double);stdcall;

  Function AddThingValue(th:integer;name,val:pchar):integer;stdcall;
  Procedure InsertThingValue(th,n:Integer;name,val:pchar);stdcall;
  Procedure DeleteThingValue(th,n:integer);stdcall;

  {COGs}
  Function AddCOG(name:pchar):Integer;stdcall;
  Procedure DeleteCOG(n:integer);stdcall;
  Procedure UpdateCOG(cg:integer);stdcall;

  Function GetCOGFileName(cg:integer):pchar;stdcall;
  Function NCOGValues(cg:integer):integer;stdcall;

  Function GetCOGValueName(cg,n:Integer):pchar;stdcall;
  Function GetCOGValueType(cg,n:Integer):integer;stdcall;

  Function GetCOGValue(cg,n:Integer):pchar;stdcall;
  Function SetCOGValue(cg,n:Integer;val:pchar):boolean;stdcall;

  Function AddCOGValue(cg:integer;name,val:pchar;vtype:integer):integer;stdcall;
  Procedure InsertCOGValue(cg,n:Integer;name,val:pchar;vtype:integer);stdcall;
  Procedure DeleteCOGValue(cg,n:integer);stdcall;

 end;

IJED=interface(IUnknown)
  Function GetVersion:double;stdcall;
  Function GetLevel:pointer;stdcall;

  Function GetMapMode:Integer;stdcall;
  Procedure SetMapMode(mode:integer);stdcall;
  Function GetCurSC:integer;stdcall;
  Procedure SetCurSC(sc:integer);stdcall;
  Function GetCurTH:integer;stdcall;
  Procedure SetCurTH(th:integer);stdcall;
  Function GetCurLT:integer;stdcall;
  Procedure SetCurLT(lt:integer);stdcall;
  Procedure GetCurVX(var sc,vx:integer);stdcall;
  Procedure SetCurVX(sc,vx:integer);stdcall;
  Procedure GetCurSF(var sc,sf:integer);stdcall;
  Procedure SetCurSF(sc,sf:integer);stdcall;
  Procedure GetCurED(var sc,sf,ed:integer);stdcall;
  Procedure SetCurED(sc,sf,ed:integer);stdcall;
  Procedure GetCurFR(var th,fr:integer);stdcall;
  Procedure SetCurFR(th,fr:integer);stdcall;

  Procedure NewLevel(mots:boolean);stdcall;
  Procedure LoadLevel(name:pchar);stdcall;

  {Different level editing functions}

  Procedure FindBBox(sec:integer;var box:TJEDBox);stdcall;
  Procedure FindBoundingSphere(sec:integer;var CX,CY,CZ,Radius:double);stdcall;
  Function FindCollideBox(sec:integer;const bbox:TJEDBox;cx,cy,cz:double;var cbox:TJEDBox):boolean;stdcall;
  Procedure FindSurfaceCenter(sc,sf:integer;var cx,cy,cz:double);stdcall;
  Procedure RotateVector(var vec:TJEDVector; pch,yaw,rol:double);stdcall;
  Procedure RotatePoint(ax1,ay1,az1,ax2,ay2,az2:double;angle:double;var x,y,z:double);stdcall;
  Procedure GetJKPYR(const x,y,z:TJEDVector;var pch,yaw,rol:double);stdcall;
  Function IsSurfaceConvex(sc,sf:integer):boolean;stdcall;
  Function IsSurfacePlanar(sc,sf:integer):boolean;stdcall;
  Function IsSectorConvex(sec:integer):boolean;stdcall;
  Function IsInSector(sec:integer;x,y,z:double):boolean;stdcall;
  Function DoSectorsOverlap(sec1,sec2:integer):boolean;stdcall;
  Function IsPointOnSurface(sc,sf:integer;x,y,z:double):boolean;stdcall;
  Function FindSectorForThing(th:integer):Integer;stdcall;
  Function FindSectorForXYZ(X,Y,Z:double):integer;stdcall;
  Function ExtrudeSurface(sc,sf:integer; by:double):integer;stdcall;
  Function CleaveSurface(sc,sf:integer; const cnormal:TJEDvector; cx,cy,cz:double):integer;stdcall;
  Function CleaveSector(sec:integer; const cnormal:TJEDvector; cx,cy,cz:double):integer;stdcall;
  Function CleaveEdge(sc,sf,ed:integer; const cnormal:TJEDvector; cx,cy,cz:double):boolean;stdcall;
  Function JoinSurfaces(sc1,sf1,sc2,sf2:Integer):boolean;stdcall;
  Function PlanarizeSurface(sc,sf:integer):boolean;stdcall;
  function MergeSurfaces(sc,sf1,sf2:integer):integer;stdcall;
  function MergeSectors(sec1,sec2:integer):integer;stdcall;
  Procedure CalculateDefaultUVNormals(sc,sf:integer; orgvx:integer; var un,vn:TJEDVector);stdcall;
  Procedure CalcUVNormals(sc,sf:integer; var un,vn:TJEDVector);stdcall;
  Procedure ArrangeTexture(sc,sf:integer; orgvx:integer; const un,vn:TJEDvector);stdcall;
  Procedure ArrangeTextureBy(sc,sf:integer;const un,vn:TJEDvector;refx,refy,refz,refu,refv:double);stdcall;
  Function IsTextureFlipped(sc,sf:integer):boolean;stdcall;
  Procedure RemoveSurfaceReferences(sc,sf:integer);stdcall;
  Procedure RemoveSectorReferences(sec:integer;surfs:boolean);stdcall;
  Function StitchSurfaces(sc1,sf1,sc2,sf2:integer):boolean;stdcall;
  Function FindCommonEdges(sc1,sf1,sc2,sf2:integer; var v11,v12,v21,v22:integer):boolean;stdcall;
  Function DoSurfacesOverlap(sc1,sf1,sc2,sf2:integer):boolean;stdcall;
  Function MakeAdjoin(sc,sf:integer):boolean;stdcall;
  Function MakeAdjoinFromSectorUp(sc,sf:integer;firstsc:integer):boolean;stdcall;
  Function UnAdjoin(sc,sf:integer):Boolean;stdcall;
  Function CreateCubicSector(x,y,z:double;const pnormal,edge:TJEDVector):integer;stdcall;

  Procedure StartUndo(name:pchar);stdcall;
  Procedure SaveUndoForThing(n:integer;change:integer);stdcall;
  Procedure SaveUndoForLight(n:integer;change:integer);stdcall;
  Procedure SaveUndoForSector(n:integer;change:integer;whatpart:integer);stdcall;
  Procedure ClearUndoBuffer;stdcall;
  Procedure ApplyUndo;stdcall;

  {Added in 0.92}
  Function GetApplicationHandle:Integer;stdcall;
  Function JoinSectors(sec1,sec2:integer):boolean;stdcall;

  Function GetNMultiselected(what:integer):integer;stdcall;
  Procedure ClearMultiselection(what:integer);stdcall;
  Procedure RemoveFromMultiselection(what,n:integer);stdcall;
  Function GetSelectedSC(n:integer):integer;stdcall;
  Function GetSelectedTH(n:integer):integer;stdcall;
  Function GetSelectedLT(n:integer):integer;stdcall;

  procedure GetSelectedSF(n:integer;var sc,sf:integer);stdcall;
  procedure GetSelectedED(n:integer;var sc,sf,ed:integer);stdcall;
  procedure GetSelectedVX(n:integer;var sc,vx:integer);stdcall;
  procedure GetSelectedFR(n:integer;var th,fr:integer);stdcall;

  Function SelectSC(sc:integer):integer;stdcall;
  Function SelectSF(sc,sf:integer):integer;stdcall;
  Function SelectED(sc,sf,ed:integer):integer;stdcall;
  Function SelectVX(sc,vx:integer):integer;stdcall;
  Function SelectTH(th:integer):integer;stdcall;
  Function SelectFR(th,fr:integer):integer;stdcall;
  Function SelectLT(lt:integer):integer;stdcall;

  Function FindSelectedSC(sc:integer):integer;stdcall;
  Function FindSelectedSF(sc,sf:integer):integer;stdcall;
  Function FindSelectedED(sc,sf,ed:integer):integer;stdcall;
  Function FindSelectedVX(sc,vx:integer):integer;stdcall;
  Function FindSelectedTH(th:integer):integer;stdcall;
  Function FindSelectedFR(th,fr:integer):integer;stdcall;
  Function FindSelectedLT(lt:integer):integer;stdcall;


  Procedure SaveJED(name:pchar);stdcall;
  Procedure SaveJKL(name:pchar);stdcall;
  Procedure UpdateMap;stdcall;
  Procedure SetPickerCMP(cmp:pchar);stdcall;
  Function PickResource(what:integer;cur:pchar):pchar;stdcall;
  Function EditFlags(what:integer;flags:LongInt):LongInt;stdcall;

  Procedure ReloadTemplates;stdcall;
  Procedure PanMessage(mtype:integer;msg:pchar);stdcall;
  Procedure SendKey(shift:integer;key:integer);stdcall;
  Function ExecuteMenu(itemref:pchar):boolean;stdcall;
  Function GetJEDSetting(name:pchar):variant;stdcall;
  Function IsLayerVisible(n:integer):boolean;stdcall;

  procedure CheckConsistencyErrors;stdcall;
  procedure CheckResources;stdcall;
  Function NConsistencyErrors:integer;stdcall;
  Function GetConsErrorString(n:integer):pchar;stdcall;
  Function GetConsErrorType(n:integer):integer;stdcall;
  Function GetConsErrorSector(n:integer):integer;stdcall;
  Function GetConsErrorSurface(n:integer):integer;stdcall;
  Function GetConsErrorThing(n:integer):integer;stdcall;
  Function GetConsErrorCog(n:integer):integer;stdcall;
  Function IsPreviewActive:boolean;stdcall;
  Function GetJEDString(what:integer):pchar;stdcall;
  Function GetIsMOTS:boolean;stdcall;
  Procedure SetIsMOTS(mots:boolean);stdcall;


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
