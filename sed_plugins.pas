unit sed_plugins;

interface

const

SEDCloseEnough = 10e-5;

{Get/SetProjectType, NewLevel}
spt_JKDF2 = 0;
spt_MOTS  = 1;
spt_IJIM  = 2;

{Get/SetLevelHeader flags}
lh_version     = 1;
lh_gravity     = 2;
lh_skyZ        = 4;
lh_cSkyOffs    = 8;
lh_HorDist     = $10;
lh_HorPPR      = $20;
lh_HSkyOffs    = $40;
lh_MipMapDist  = $80;
lh_LODDist     = $100;
lh_PerspDist   = $200;
lh_GouraudDist = $400;
lh_ppu         = $800;
lh_MasterCMP   = $1000;
lh_Fog         = $2000;
lh_all         = $2FFF;

{Get/SetSector flags}
s_flags      = 1;
s_ambient    = 2;
s_extra      = 4;
s_pointlight = 8;
s_cmp        = $10;
s_tint       = $20;
s_sound      = $40;
s_sndvol     = $80;
s_thrust     = $100;
s_layer      = $200;
s_all        = $FFFF;

{Get/SetSurface flags}
sf_adjoin      = 1;
sf_adjoinflags = 2;
sf_SurfFlags   = 4;
sf_FaceFlags   = 8;
sf_Material    = $10;
sf_geo         = $20;
sf_light       = $40;
sf_tex         = $80;
sf_ExtraLight  = $100;
sf_txscale     = $200;
sf_all         = $3FF;

{Get/SetThing flags}
th_name        = 1;
th_sector      = 2;
th_position    = 4;
th_orientation = 8;
th_layer       = $10;
th_all         = $1F;

{Get/SetLight flags}
lt_position     = 1;
lt_intensity    = 2;
lt_range        = 4;
lt_color        = 8;
lt_flags        = $10;
lt_layer        = $20;
lt_all          = $FF;

{Map Mode constants}
MM_SC = 0; {Sectors}
MM_SF = 1; {surfaces}
MM_VX = 2; {vertices}
MM_TH = 3; {Things}
MM_ED = 4; {Edges}
MM_LT = 5; {Lights}
MM_FR = 6; {frames}

{SurfaceUpdate "how" constant}
su_texture    = 1;
su_floorflag  = 2;
su_sector     = 4;
su_all        = $FF;

{Undo change constants}
uc_changed  = 0;
uc_added    = 1;
uc_deleted  = 2;
sc_values   = 1;
sc_geometry = 2;

{COG value types}
ct_unknown  = 0;
ct_ai       = 1;
ct_cog      = 2;
ct_key      = 3;
ct_mat      = 4;
ct_msg      = 5;
ct_3do      = 6;
ct_sec      = 7;
ct_wav      = 8;
ct_surface  = 9;
ct_template = 10;
ct_thing    = 11;
ct_int      = 12;
ct_flex     = 13;
ct_vector   = 14;

{PickResource constants}
pr_thing     = 1;
pr_template  = 2;
pr_cmp       = 3;
pr_secsound  = 4;
pr_mat       = 5;
pr_cog       = 6;
pr_layer     = 7;
pr_3do       = 8;
pr_ai        = 9;
pr_key       = 10;
pr_snd       = 11;
pr_pup       = 12;
pr_spr       = 13;
pr_par       = 14;
pr_per       = 15;
pr_jklsmksan = 16;

{EditFlags constants}
ef_sector    = 1;
ef_surface   = 2;
ef_adjoin    = 3;
ef_thing     = 4;
ef_face      = 5;
ef_light     = 6;
ef_geo       = 7;
ef_lightmode = 8;
ef_tex       = 9;

{PanMessage constants}
msg_info    = 0;
msg_warning = 1;
msg_error   = 2;

{SendKey flags}
sk_Ctrl  = 1;
sk_Shift = 2;
sk_Alt   = 4;

{GetConsErrorType constants}
ce_normal  = 0;
ce_warning = 1;
ce_error   = 2;

{GetConsErrorItemType constants}
cei_none    = 0;
cei_sector  = 1;
cei_surface = 2;
cei_thing   = 3;
cei_cog     = 4;

{GetSEDString constants}
SS_ProjectDir  = 1;
SS_SEDDir      = 2;
SS_GameDir     = 3;
SS_LevelFile   = 4;
SS_ConfigFile  = 5; // ex js_jedregkey
SS_LECLogoJK   = 6;
SS_Recent1     = 7;
SS_Recent2     = 8;
SS_Recent3     = 9;
SS_Recent4     = 10;
SS_Res1gob     = 11;
SS_Res2gob     = 12;
SS_SPGOB       = 13;
SS_MP1GOB      = 14;
SS_MP2GOB      = 15;
SS_MP3GOB      = 16;
SS_LECLogoIJIM = 17;

{GetJEDWindow constants}
sw_Main         =0;
sw_ConsChecker  =1;
sw_ItemEdit     =2;
sw_PlacedCogs   =3;
sw_MsgWindow    =4;
sw_3DPreview    =5;
sw_ToolWindow   =6;

// Get/SetRendererDoube constants
rd_CamX     = 0;
rd_CamY     = 1;
rd_CamZ     = 2;
rd_Scale    = 3;
rd_GridX    = 4;
rd_GridY    = 5;
rd_GridZ    = 6;
rd_GridLine = 7;
rd_GridDot  = 8;
rd_GridStep = 9;

// Get/SetRendererVector constants
rv_CamPos    = 10;
rv_GridPos   = 11;
rv_CamXAxis  = 12;
rv_CamYAxis  = 13;
rv_CamZAxis  = 14;

rv_GridXAxis = 15;
rv_GridYAxis = 16;
rv_GridZAxis = 17;

{SetRenderColor constants}
rc_current    = 0;
rc_Background = 1;

{SetCulling constants}
cc_CullNone  = 0;
cc_CullBack  = -1;
cc_CullFront = 1;

{CreateWFRenderer constants}
cr_Default  = 0;
cr_OpenGL   = 1;
cr_Software = 2;

type

TSEDVector3 = record
  case integer of
    0: (dx,dy,dz: double);
    1: (x,y,z: double);
    2: (pitch,yaw,roll: double);
end;

TSEDColor = record
    r, g, b, a: single;  // a - alpha or light intensity
end;

TSEDBox = record
  p1: TSEDVector3;
  p2: TSEDVector3;
end;

TSEDPointLight = record
  color: TSEDColor;
  position: TSEDVector3;
  minRange: double;  // minimum light range at which light will still illuminate
  maxRange: double;  // maximum light intensity range before it starts to falloff
end;

TSEDFog = record
  enabled: Boolean;
  color: TSEDColor;
  fogStart: double;
  fogEnd: double;
end;

TSEDLevelHeader = record
  version: Longint;
  gravity: double;
  ceilingSkyZ: double;
  ceilingSkyOffs: array[0..1] of double;
  horDistance,
  horPixelsPerRev: double;
  horSkyOffs: array[0..1] of double;
  mipMapDist: array[0..3] of double;
  lodDist: array[0..3] of double;
  perspDist, GouraudDist: double;
  pixelPerUnit: double;
  masterCMP: PChar;
  fog: TSEDFog;
end;

TSEDSectorRec = record
  flags: longint;
  ambient: TSEDColor;
  extraLight: TSEDColor;
  pointLight: TSEDPointLight;
  colorMap: PChar;
  tint: TSEDColor;
  sound: PChar;
  soundVolume: double;
  thrust: TSEDVector3;
  layer: longint;
end;

TSEDSurfaceRec = record
  adjoinSec, AdjoinSurf: longint;
  adjoinflags,
  surfflags,
  faceflags: Longint;
  material: PChar;
  geo,light,tex: longint;
  extraLight: TSEDColor;
  uscale,vscale: single;
end;

TSEDThingRec = record
  name: PChar;
  Sector: Longint;
  position: TSEDVector3;
  rotation: TSEDVector3;
  layer: longint;
end;

TSEDLightRec = record
  flags: longint;
  layer: longint;
  color: TSEDColor;
  intensity: double;
  range: double;
  position: TSEDVector3;
end;

{endtype}

{ Wireframe renderer interface}
{JED 0.93}
ISEDWFRenderer = class
  {OLE2 crap. ignore}
  function QueryInterface(iid: pointer; var obj): LongInt; virtual; stdcall; abstract;
  function AddRef: Longint; virtual; stdcall; abstract;
  function Release: Longint; virtual; stdcall; abstract;

  {Renderer attributes}
  Function GetRendererDouble(what:integer):double;virtual;stdcall;abstract;
  Procedure SetRendererDouble(what:integer;val:double);virtual;stdcall;abstract;
  Procedure GetRendererVector(what:integer;var x,y,z:double);virtual;stdcall;abstract;
  Procedure SetRendererVector(what:integer;x,y,z:double);virtual;stdcall;abstract;

  Function NSelected:integer; virtual;stdcall;abstract;
  Function GetNSelected(n:integer):integer; virtual;stdcall;abstract;
  Procedure SetViewPort(x,y,w,h:integer);virtual;stdcall;abstract;
  Procedure SetColor(what,r,g,b:byte);virtual;stdcall;abstract;
  Procedure SetPointSize(size:double);virtual;stdcall;abstract;
  Procedure BeginScene;virtual;stdcall;abstract;
  Procedure EndScene;virtual;stdcall;abstract;
  Procedure SetCulling(how:integer);virtual;stdcall;abstract;

  Procedure DrawSector(sc:integer);virtual;stdcall;abstract;
  Procedure DrawSurface(sc,sf:integer);virtual;stdcall;abstract;
  Procedure DrawThing(th:integer);virtual;stdcall;abstract;

  Procedure DrawLine(x1,y1,z1,x2,y2,z2:double);virtual;stdcall;abstract;
  Procedure DrawVertex(X,Y,Z:double);virtual;stdcall;abstract;
  Procedure DrawGrid;virtual;stdcall;abstract;

  Procedure BeginPick(x,y:integer);virtual;stdcall;abstract;
  Procedure EndPick;virtual;stdcall;abstract;

  Procedure PickSector(sc:integer;id:integer);virtual;stdcall;abstract;
  Procedure PickSurface(sc,sf:integer;id:integer);virtual;stdcall;abstract;
  Procedure PickLine(x1,y1,z1,x2,y2,z2:double;id:integer);virtual;stdcall;abstract;
  Procedure PickVertex(X,Y,Z:double;id:integer);virtual;stdcall;abstract;

  Procedure BeginRectPick(x1,y1,x2,y2:integer);virtual;stdcall;abstract;
  Procedure EndRectPick;virtual;stdcall;abstract;
  Function IsSectorInRect(sc:integer):boolean;virtual;stdcall;abstract;
  Function IsSurfaceInRect(sc,sf:integer):boolean;virtual;stdcall;abstract;
  Function IsLineInRect(x1,y1,z1,x2,y2,z2:double):boolean;virtual;stdcall;abstract;
  Function IsVertexInRect(X,Y,Z:double):boolean;virtual;stdcall;abstract;

  Function GetXYZonPlaneAt(scX,scY:integer;pnormal:TSEDVector3; pX,pY,pZ:double; var X,Y,Z:double):Boolean;virtual;stdcall;abstract;
  Function GetGridAt(scX,scY:integer;var X,Y,Z:double):boolean;virtual;stdcall;abstract;
  Procedure GetNearestGridNode(iX,iY,iZ:double; Var X,Y,Z:double);virtual;stdcall;abstract;
  Procedure ProjectPoint(x,y,z:double; Var WinX,WinY:integer);virtual;stdcall;abstract;
  Procedure UnProjectPoint(WinX,WinY:integer; WinZ:double; var x,y,z:double);virtual;stdcall;abstract;
  Function IsSurfaceFacing(sc,sf:integer):boolean;virtual;stdcall;abstract;

  Function HandleWMQueryPal:integer;virtual;stdcall;abstract;
  Function HandleWMChangePal:integer;virtual;stdcall;abstract;

end;

ISEDLevel = class
  {OLE2 crap. ignore}
  function QueryInterface(iid: pointer; var obj): LongInt; virtual; stdcall; abstract;
  function AddRef: Longint; virtual; stdcall; abstract;
  function Release: Longint; virtual; stdcall; abstract;

  {Level header}
  Procedure GetLevelHeader(var lh: TSEDLevelHeader; flags: Integer); virtual;stdcall;abstract;
  Procedure SetLevelHeader(const lh: TSEDLevelHeader; flags: Integer); virtual;stdcall;abstract;

  Function NumSectors: Integer; virtual;stdcall;abstract;
  Function NumThings: Integer; virtual;stdcall;abstract;
  Function NumLights: Integer; virtual;stdcall;abstract;
  Function NumCOGs: Integer; virtual;stdcall;abstract;

  {Sectors}
  Function SectorAdd: Integer;virtual;stdcall;abstract;
  Procedure SectorDelete(n: Integer);virtual;stdcall;abstract;

  Procedure SectorGet(sec: Integer; var rec: TSEDSectorRec; flags: Integer); virtual;stdcall;abstract;
  Procedure SectorSet(sec: Integer; const rec: TSEDSectorRec; flags: Integer); virtual;stdcall;abstract;

  Function SectorNumVertices(sec: Integer): Integer; virtual;stdcall;abstract;
  Function SectorNumSurfaces(sec: Integer): Integer; virtual;stdcall;abstract;

  Procedure SectorGetVertex(sec, vn: Integer; var vert: TSEDVector3); virtual;stdcall;abstract;
  Procedure SectorSetVertex(sec, vn: Integer; vert: TSEDVector3); virtual;stdcall;abstract;

  Function SectorAddVertex(sec: Integer; vert: TSEDVector3): Integer; virtual;stdcall;abstract;
  Function SectorFindVertex(sec: Integer; vert: TSEDVector3): Integer; virtual;stdcall;abstract;
  Function SectorDeleteVertex(sec: Integer; vn: Integer): Integer; virtual;stdcall;abstract;

  Function SectorAddSurface(sec: Integer): Integer; virtual;stdcall;abstract;
  Procedure SectorDeleteSurface(sec,surf: Integer); virtual;stdcall;abstract;
  Procedure SectorUpdate(sec: Integer); virtual;stdcall;abstract;

  {Surfaces}
  Procedure SurfaceGet(sec, surf: Integer; var rec: TSEDSurfaceRec; flags: Integer); virtual;stdcall;abstract;
  Procedure SurfaceSet(sec, surf: Integer; const rec: TSEDSurfaceRec; flags: Integer); virtual;stdcall;abstract;
  Procedure SurfaceGetNormal(sec,surf: Integer; var normal: TSEDVector3); virtual;stdcall;abstract;
  Procedure SurfaceUpdate(sec, surf: Integer; how: Integer); virtual;stdcall;abstract;
  Function SurfaceNumVertices(sec, surf: Integer): Integer; virtual;stdcall;abstract;
  Function SurfaceGetVertexNum(sec, surf, vn: Integer): Integer; virtual;stdcall;abstract;
  Procedure SurfaceSetVertexNum(sec, surf, vn: Integer; secvx: Integer); virtual;stdcall;abstract;
  Function SurfaceAddVertex(sec, surf: integer; secvn: Integer): Integer; virtual;stdcall;abstract;
  Function SurfaceInsertVertex(sec, surf: Integer; at: Integer; secvn: Integer): Integer; virtual;stdcall;abstract;
  Procedure SurfaceDeleteVertex(sec, surf: Integer; n: Integer) ;virtual;stdcall;abstract;
  Procedure SurfaceGetVertexUV(sec, surf, vn: Integer; var u,v: single); virtual;stdcall;abstract;
  Procedure SurfaceSetVertexUV(sec, surf, vn: integer; u,v: single); virtual;stdcall;abstract;
  Procedure SurfaceGetVertexLight(sec, surf, vn: Integer; var color: TSEDColor); virtual; stdcall; abstract;
  Procedure SurfaceSetVertexLight(sec, surf, vn: Integer; const color: TSEDColor); virtual; stdcall; abstract;

  {Things}
  Function ThingAdd: Integer; virtual; stdcall;abstract;
  Procedure ThingDelete(th: Integer); virtual;stdcall;abstract;
  Procedure ThingGet(th: Integer; var rec:TSEDThingRec; flags: Integer); virtual;stdcall;abstract;
  Procedure ThingSet(th: Integer; const rec:TSEDThingRec; flags: Integer); virtual;stdcall;abstract;
  Procedure ThingUpdate(th: Integer); virtual;stdcall;abstract;

  {Lights}
  Function LightAdd:Integer; virtual;stdcall;abstract;
  Procedure LightDelete(lt: Integer);virtual;stdcall;abstract;
  Procedure LightGet(lt: Integer; var rec: TSEDLightRec; flags: Integer); virtual;stdcall;abstract;
  Procedure LightSet(lt: Integer; const rec: TSEDLightRec; flags: Integer); virtual;stdcall;abstract;
  Procedure LightUpdate(lt:integer); virtual;stdcall;abstract;

  {Layers}
  Function NumLayers: Integer; virtual;stdcall;abstract;
  Function LayerGetName(n: Integer): PChar; virtual;stdcall;abstract;
  Function LayerAdd(name: PChar): Integer; virtual;stdcall;abstract;

  {JED 0.92}
  Function NumThingValues(th: Integer): Integer; virtual;stdcall;abstract;
  Function ThingValueGetName(th, n: Integer): PChar; virtual;stdcall;abstract;
  Function ThingValueGetData(th, n: Integer): PChar; virtual;stdcall;abstract;
  Procedure ThingValueSetData(th,n: Integer;val: PChar); virtual;stdcall;abstract;

  Function ThingValueAdd(th: integer; name,val: PChar): Integer; virtual;stdcall;abstract;
  Procedure ThingValueInsert(th,n: Integer; name,val: PChar); virtual;stdcall;abstract;
  Procedure ThingValueDelete(th,n: Integer); virtual;stdcall;abstract;

  Procedure ThingFrameGet(th,n: Integer; var pos, rot: TSEDVector3); virtual;stdcall;abstract;
  Procedure ThingFrameSet(th,n: Integer; pos, rot: TSEDVector3); virtual;stdcall;abstract;

  {COGs}
  Function COGAdd(name: PChar): Integer; virtual;stdcall;abstract;
  Procedure COGDelete(n: Integer); virtual;stdcall;abstract;
  Procedure COGUpdate(cg: Integer); virtual;stdcall;abstract;
  Function COGGetFilename(cg: Integer): PChar; virtual;stdcall;abstract;

  Function NumCOGValues(cg: Integer): Integer; virtual;stdcall;abstract;

  Function COGValueGetName(cg,n: Integer): PChar; virtual;stdcall;abstract;
  Function COGValueGetType(cg,n: Integer): Integer; virtual;stdcall;abstract;

  Function COGValueGet(cg,n: Integer): PChar; virtual;stdcall;abstract;
  Function COGValueSet(cg,n: Integer; val:PChar): Boolean; virtual;stdcall;abstract;

  Function COGValueAdd(cg: Integer; name,val: PChar; vtype: Integer): Integer; virtual;stdcall;abstract;
  Procedure COGValueInsert(cg,n: Integer; name,val:PChar; vtype: Integer); virtual;stdcall;abstract;
  Procedure COGValueDelete(cg,n: integer); virtual;stdcall;abstract;
end;

ISED = class
  {OLE2 crap. Ignore}
  function QueryInterface(iid: pointer; var obj): LongInt; virtual; stdcall; abstract;
  function AddRef: Longint; virtual; stdcall; abstract;
  function Release: Longint; virtual; stdcall; abstract;

  Function GetVersion: double; virtual;stdcall;abstract; {version}
  Function GetLevel: ISEDLevel; virtual;stdcall;abstract; {SEDApp OLE interafce}

  Function GetMapMode: Integer; virtual;stdcall;abstract;
  Procedure SetMapMode(mode: Integer); virtual;stdcall;abstract;
  Function GetCurrentSector: Integer; virtual;stdcall;abstract;
  Procedure SetCurrentSector(sc: Integer); virtual;stdcall;abstract;
  Function GetCurrentThing: Integer; virtual;stdcall;abstract;
  Procedure SetCurrentThing(th: Integer); virtual;stdcall;abstract;
  Function GetCurrentLight: Integer; virtual;stdcall;abstract;
  Procedure SetCurrentLight(lt: Integer); virtual;stdcall;abstract;
  Procedure GetCurrentVertex(var sec,vn: Integer); virtual;stdcall;abstract;
  Procedure SetCurrentVertex(sec,vn: Integer); virtual;stdcall;abstract;
  Procedure GetCurrentSurface(var sec,surf: Integer); virtual;stdcall;abstract;
  Procedure SetCurrentSurface(sec,surf: Integer); virtual;stdcall;abstract;
  Procedure GetCurrentEdge(var sec,surf,ed: Integer); virtual;stdcall;abstract;
  Procedure SetCurrentEdge(sec,surf,ed: Integer); virtual;stdcall;abstract;
  Procedure GetCurrentFrame(var th,fr: Integer); virtual;stdcall;abstract;
  Procedure SetCurrentFrame(th,fr: Integer);virtual;stdcall;abstract;

  Procedure NewLevel(kind: integer); virtual;stdcall;abstract; // SED 0.1.0
  Procedure LoadLevel(name: PChar); virtual;stdcall;abstract;

  {Different level editing functions}
  procedure RotateVector(var vec: TSEDVector3; pyr: TSEDVector3); virtual; stdcall; abstract;
  procedure RotatePoint(p, pivot: TSEDVector3; angle: double; var p2: TSEDVector3); virtual; stdcall; abstract;
  procedure GetPYR(const x, y, z: TSEDVector3; var pyr: TSEDVector3); virtual; stdcall; abstract;

  function MergeSectors(sec1, sec2: Integer): Integer; virtual; stdcall; abstract;
  function CleaveSector(sec: Integer; const cnormal: TSEDVector3; cp: TSEDVector3): Integer; virtual; stdcall; abstract;
  Function CreateCubicSector(p: TSEDVector3; const pnormal, edge: TSEDVector3): Integer; virtual; stdcall; abstract;
  function IsSectorConvex(sec: Integer): boolean; virtual; stdcall; abstract;
  function IsInSector(sec: Integer; p: TSEDVector3): boolean; virtual; stdcall; abstract;
  function DoSectorsOverlap(sec1, sec2: Integer): boolean; virtual; stdcall; abstract;
  procedure FindBoundingBox(sec: Integer; var box: TSEDBox); virtual; stdcall; abstract;
  procedure FindBoundingSphere(sec: Integer; var center: TSEDVector3; radius: double); virtual; stdcall; abstract;
  function FindCollideBox(sec: Integer; const bbox: TSEDBox; center: TSEDVector3; var cbox: TSEDBox): boolean; virtual; stdcall; abstract;
  function FindSectorForThing(th: Integer): Integer; virtual; stdcall; abstract;
  function FindSectorForXYZ(X, Y, Z: double): Integer; virtual; stdcall; abstract;

  procedure FindSurfaceCenter(sec, surf: Integer; var center: TSEDVector3); virtual; stdcall; abstract;
  function IsSurfaceConvex(sec, surf: Integer): boolean; virtual; stdcall; abstract;
  function IsSurfacePlanar(sec, surf: Integer): boolean; virtual; stdcall; abstract;
  function IsPointOnSurface(sec, surf: Integer; p: TSEDVector3): boolean; virtual; stdcall; abstract;

  function CleaveSurface(sec, surf: Integer; const cnormal: TSEDVector3; cp: TSEDVector3): Integer; virtual; stdcall; abstract;
  function CleaveEdge(sec, surf, ed: Integer; const cnormal: TSEDVector3; cp: TSEDVector3): boolean; virtual; stdcall; abstract;
  function ExtrudeSurface(sec, surf: Integer; by: double): Integer; virtual; stdcall; abstract;
  function JoinSurfaces(sec1, surf1, sec2, surf2: Integer): boolean; virtual; stdcall; abstract;
  function MergeSurfaces(sec, surf1, surf2: Integer): Integer; virtual; stdcall; abstract;
  function PlanarizeSurface(sec, surf: Integer): boolean; virtual; stdcall; abstract;

  procedure CalculateDefaultUVNormals(sec, surf: Integer; orgvx: Integer; var un, vn: TSEDVector3); virtual; stdcall; abstract;
  procedure CalculateUVNormals(sec, surf: Integer; var un, vn: TSEDVector3); virtual; stdcall; abstract;
  procedure ArrangeTexture(sec, surf: Integer; orgvx: Integer; const un, vn: TSEDVector3); virtual; stdcall; abstract;
  procedure ArrangeTextureBy(sec, surf: Integer; const un, vn: TSEDVector3; refp: TSEDVector3; refu, refv: double); virtual; stdcall; abstract;
  function IsTextureFlipped(sec, surf: Integer): boolean; virtual; stdcall; abstract;
  function StitchSurfaces(sc1, sf1, sc2, sf2: Integer): boolean; virtual; stdcall; abstract;

  procedure RemoveSurfaceReferences(sec, surf: Integer); virtual; stdcall; abstract;
  procedure RemoveSectorReferences(sec: Integer; surfs: boolean); virtual; stdcall; abstract;
  function FindCommonEdges(sc1, sf1, sc2, sf2: Integer; var v11, v12, v21, v22: Integer): boolean; virtual; stdcall; abstract;
  function DoSurfacesOverlap(sc1, sf1, sc2, sf2: Integer): boolean; virtual; stdcall; abstract;

  function MakeAdjoin(sec, surf: integer): boolean; virtual; stdcall; abstract;
  Function MakeAdjoinFromSectorUp(sec: Integer; surf: Integer; firstSec: Integer): Boolean; virtual; stdcall; abstract;
  Function UnAdjoin(sec: Integer; surf: Integer): Boolean; virtual; stdcall; abstract;

  Procedure StartUndo(name: PChar); virtual; stdcall; abstract;
  Procedure SaveUndoForThing(n: Integer; change: Integer); virtual; stdcall; abstract;
  Procedure SaveUndoForLight(n: Integer; change: Integer); virtual; stdcall; abstract;
  Procedure SaveUndoForSector(n: Integer; change: Integer; whatPart: Integer); virtual; stdcall; abstract;
  Procedure ClearUndoBuffer; virtual; stdcall; abstract;
  Procedure ApplyUndo; virtual; stdcall; abstract;

  {Added in JED 0.92}
  Function GetApplicationHandle: Integer; virtual; stdcall; abstract;
  Function JoinSectors(sec1, sec2: Integer): Boolean; virtual; stdcall; abstract;

  Function GetNumMultiselected(what: Integer): Integer; virtual; stdcall; abstract;
  Procedure ClearMultiselection(what: Integer); virtual; stdcall; abstract;
  Procedure RemoveFromMultiselection(what, n: Integer); virtual; stdcall; abstract;

  Function GetSelectedSector(n: Integer): Integer; virtual; stdcall; abstract;
  procedure GetSelectedSurface(n: Integer; var sec, surf: Integer); virtual; stdcall; abstract;
  procedure GetSelectedEdge(n: Integer; var sec, surf, ed: Integer); virtual; stdcall; abstract;
  procedure GetSelectedVertex(n: Integer; var sec, vn: Integer); virtual; stdcall; abstract;
  Function GetSelectedThing(n: Integer): Integer; virtual; stdcall; abstract;
  procedure GetSelectedFrame(n: Integer; var th, fr: Integer); virtual; stdcall; abstract;
  Function GetSelectedLight(n: Integer): Integer; virtual; stdcall; abstract;

  Function SelectSector(sec: Integer): Integer; virtual; stdcall; abstract;
  Function SelectSurface(sec, surf: Integer): Integer; virtual; stdcall; abstract;
  Function SelectEdge(sec, surf, ed: Integer): Integer; virtual; stdcall; abstract;
  Function SelectVertex(sec, vn: Integer): Integer; virtual; stdcall; abstract;
  Function SelectThing(th: Integer): Integer; virtual; stdcall; abstract;
  Function SelectFrame(th, fr: Integer): Integer; virtual; stdcall; abstract;
  Function SelectLight(lt: Integer): Integer; virtual; stdcall; abstract;

  Function FindSelectedSector(sec: Integer): Integer; virtual; stdcall; abstract;
  Function FindSelectedSurface(sec, surf: Integer): Integer; virtual; stdcall; abstract;
  Function FindSelectedEdge(sec, surf, ed: Integer): Integer; virtual; stdcall; abstract;
  Function FindSelectedVertex(sec, vn: Integer): Integer; virtual; stdcall; abstract;
  Function FindSelectedThing(th: Integer): Integer; virtual; stdcall; abstract;
  Function FindSelectedFrame(th, fr: Integer): Integer; virtual; stdcall; abstract;
  Function FindSelectedLight(lt: Integer): Integer; virtual; stdcall; abstract;

  Procedure Save(name: PChar); virtual; stdcall; abstract;
  Procedure SaveSED(name: PChar); virtual; stdcall; abstract;
  Procedure UpdateMap; virtual; stdcall; abstract;
  Procedure SetPickerCMP(cmp: PChar); virtual; stdcall; abstract;
  Function PickResource(what: Integer; cur: PChar): PChar; virtual; stdcall; abstract;
  Function EditFlags(what: Integer; flags: LongInt): LongInt; virtual; stdcall; abstract;

  Procedure ReloadTemplates; virtual; stdcall; abstract;
  Procedure PanMessage(mType: Integer; msg: PChar); virtual; stdcall; abstract;
  Procedure SendKey(shift: Integer; key: Integer); virtual; stdcall; abstract;
  Function ExecuteMenu(itemRef: PChar): Boolean; virtual; stdcall; abstract;
  Function GetSEDSetting(name: PChar): Variant; virtual; stdcall; abstract;
  Function IsLayerVisible(n: Integer): Boolean; virtual; stdcall; abstract;

  procedure CheckConsistencyErrors; virtual; stdcall; abstract;
  procedure CheckResources; virtual; stdcall; abstract;
  Function GetNumConsistencyErrors: Integer; virtual; stdcall; abstract;
  Function GetConsistencyErrorString(n: Integer): PChar; virtual; stdcall; abstract;
  Function GetConsistencyErrorType(n: Integer): Integer; virtual; stdcall; abstract;
  Function GetConsistencyErrorItemType(n: Integer): Integer; virtual; stdcall; abstract; // SED 0.1.0
  Function GetConsistencyErrorSector(n: Integer): Integer; virtual; stdcall; abstract;
  Function GetConsistencyErrorSurface(n: Integer): Integer; virtual; stdcall; abstract;
  Function GetConsistencyErrorThing(n: Integer): Integer; virtual; stdcall; abstract;
  Function GetConsistencyErrorCOG(n: Integer): Integer; virtual; stdcall; abstract;
  Function IsPreviewActive: Boolean; virtual; stdcall; abstract;
  Function GetSEDString(what: Integer): PChar; virtual; stdcall; abstract;
  Function GetProjectType: Integer; virtual; stdcall; abstract; // SED 0.1.0
  Procedure SetProjectType(kind: Integer); virtual; stdcall; abstract; // SED 0.1.0

  {JED 0.93}
  Function GetSEDWindow(whichOne: Integer): Integer; virtual; stdcall; abstract;

  Function FileExtractExt(path: PChar): PChar; virtual; stdcall; abstract;
  Function FileExtractPath(path: PChar): PChar; virtual; stdcall; abstract;
  Function FileExtractName(path: PChar): PChar; virtual; stdcall; abstract;
  Function FindProjectDirFile(name: PChar): PChar; virtual; stdcall; abstract;
  Function IsFileContainer(path: PChar): Boolean; virtual; stdcall; abstract;
  Function IsFileInContainer(path: PChar): Boolean; virtual; stdcall; abstract;

  Function FileOpenDialog(fName: PChar; filter: PChar): PChar; virtual; stdcall; abstract;

  Function OpenFile(name: PChar): Integer; virtual; stdcall; abstract;
  Function OpenGameFile(name: PChar): Integer; virtual; stdcall; abstract;
  Function ReadFile(handle: Integer; buf: Pointer; size: LongInt): Integer; virtual; stdcall; abstract;
  Procedure SetFilePos(handle: Integer; pos: LongInt); virtual; stdcall; abstract;
  Function GetFilePos(handle: Integer): LongInt; virtual; stdcall; abstract;
  Function GetFileSize(handle: Integer): LongInt; virtual; stdcall; abstract;
  Function GetFileName(handle: Integer): PChar; virtual; stdcall; abstract;
  Procedure CloseFile(handle: Integer); virtual; stdcall; abstract;

  Function OpenTextFile(name: PChar): Integer; virtual; stdcall; abstract;
  Function OpenGameTextFile(name: PChar): Integer; virtual; stdcall; abstract;
  Function GetTextFileName(handle: Integer): PChar; virtual; stdcall; abstract;
  Function ReadTextFileString(handle: Integer): PChar; virtual; stdcall; abstract;
  Function TextFileEOF(handle: Integer): Boolean; virtual; stdcall; abstract;
  Function TextFileCurrentLine(handle: Integer): Integer; virtual; stdcall; abstract;
  Procedure CloseTextFile(handle: Integer); virtual; stdcall; abstract;

  Function GOBOpen(name: PChar): Integer; virtual; stdcall; abstract;
  Function GOBNumFiles(handle: Integer): Integer; virtual; stdcall; abstract;
  Function GOBGetFilename(handle: Integer; n: Integer): PChar; virtual; stdcall; abstract;
  Function GOBGetFullFilename(handle: Integer; n: Integer): PChar; virtual; stdcall; abstract;
  Function GOBGetFileSize(handle: Integer; n: Integer): LongInt; virtual; stdcall; abstract;
  Function GOBGetFileOffset(handle: Integer; n: Integer): LongInt; virtual; stdcall; abstract;
  Procedure GOBClose(handle: Integer); virtual; stdcall; abstract;

  Function CreateWFRenderer(wnd: Integer; whichOne: Integer): ISEDWFRenderer; virtual; stdcall; abstract;
end;


{cvtstop}


 {The function that SED calls to load your plug-in
 must be named SEDPluginLoad (register calling) or
 SEDPluginLoadStdCall (standard stack calling)
 and have one parameter ISED interface object. The
 function should return true if the plug-in was
 loaded/activated successfully and false otherwise.
 Warning! Names are case-sensitive}


// This is how the Plug-in entry function should be declared
// Function TSEDPluginLoadStdCall(sed:ISED):boolean;stdcall;
// The old style:
// Function TSEDPluginLoad(sed:ISED):boolean;
// is still supported, but not recommended

TSEDPluginLoad = Function(sed:ISED):boolean;
TSEDPluginLoadStdCall = Function(sed:ISED):Boolean;stdcall;


{General purpose functions. Local}

Procedure SetVector(var vec: TSEDVector3; dx, dy, dz: Double);
Function DotProduct(const vec1, vec2: TSEDVector3): Double;

Procedure CrossProduct(const vec1, vec2: TSEDVector3; var vec: TSEDVector3);
Function VectorLen(const vec: TSEDVector3): Double;
Function NormalizeVector(var vec: TSEDVector3): Boolean;
Function DoBoxesIntersect(const box1, box2: TSEDBox): Boolean;
Procedure SetBox(var box: TSEDBox; x1, x2, y1, y2, z1, z2: Double);
Function IsPointInBox(const box: TSEDBox; x, y, z: Double): Boolean;
Function LinePlaneIntersection(const normal: TSEDVector3; pX, pY, pZ, x1, y1, z1, x2, y2, z2: Double; var x, y, z: Double): Boolean;
Function MinDouble(d1, d2: Double): Double;

implementation

Function MinDouble(d1, d2: Double): Double;
begin
  if d1 < d2 then result := d1 else result := d2;
end;

Procedure SetVector(var vec: TSEDVector3; dx, dy, dz: Double);
begin
  vec.dx := dx;
  vec.dy := dy;
  vec.dz := dz;
end;

Function DotProduct(const vec1, vec2: TSEDVector3): Double;
begin
  result := vec1.dx * vec2.dx + vec1.dy * vec2.dy + vec1.dz * vec2.dz;
end;

Procedure CrossProduct(const vec1, vec2: TSEDVector3; var vec: TSEDVector3);
begin
  vec.dx := vec1.dy * vec2.dz - vec2.dy * vec1.dz;
  vec.dy := vec2.dx * vec1.dz - vec1.dx * vec2.dz;
  vec.dz := vec1.dx * vec2.dy - vec2.dx * vec1.dy;
end;

Function VectorLen(const vec: TSEDVector3): Double;
begin
  result := sqrt(sqr(vec.dx) + sqr(vec.dy) + sqr(vec.dz));
end;

Function NormalizeVector(var vec: TSEDVector3): Boolean;
var
  len: Double;
begin
  Result := false;
  len := VectorLen(vec);
  if len = 0 then exit;
  vec.dx := vec.dx / len;
  vec.dy := vec.dy / len;
  vec.dz := vec.dz / len;
  Result := true;
end;

Function DoBoxesIntersect(const box1, box2: TSEDBox): Boolean;
begin
  result := false;
  if (box1.p2.x < box2.p1.x) then exit;
  if (box1.p1.x > box2.p2.x) then exit;
  if (box1.p2.y < box2.p1.y) then exit;
  if (box1.p1.y > box2.p2.y) then exit;
  if (box1.p2.z < box2.p1.z) then exit;
  if (box1.p1.z > box2.p2.z) then exit;
  result := true;
end;

Procedure SetBox(var box: TSEDBox; x1, x2, y1, y2, z1, z2: Double);
begin
  box.p1.x := MinDouble(x1, x2);
  box.p2.x := MinDouble(x1, x2);
  box.p1.y := MinDouble(y1, y2);
  box.p2.y := MinDouble(y1, y2);
  box.p1.z := MinDouble(z1, z2);
  box.p2.z := MinDouble(z1, z2);
end;

Function IsPointInBox(const box: TSEDBox; x, y, z: Double): Boolean;
begin
  result := (x - box.p1.x >= -SEDCloseEnough) and (x - box.p2.x <= SEDCloseEnough) and
            (y - box.p1.y >= -SEDCloseEnough) and (y - box.p2.y <= SEDCloseEnough) and
            (z - box.p1.z >= -SEDCloseEnough) and (z - box.p2.z <= SEDCloseEnough);
end;

Function LinePlaneIntersection(const normal: TSEDVector3; pX, pY, pZ, x1, y1, z1, x2, y2, z2: Double; var x, y, z: Double): Boolean;
var
  v1, v2: TSEDVector3;
  dist1, dist2: Double;
  k: Double;
begin
  result := false;
  SetVector(v1, x1 - pX, y1 - pY, z1 - pZ);
  SetVector(v2, x2 - pX, y2 - pY, z2 - pZ);

  dist1 := DotProduct(v1, normal);
  dist2 := DotProduct(v2, normal);
  if dist1 = dist2 then exit;

  k := dist1 / (dist1 - dist2);

  x := x1 + k * (x2 - x1);
  y := y1 + k * (y2 - y1);
  z := z1 + k * (z2 - z1);
  result := true;
end;

end.
