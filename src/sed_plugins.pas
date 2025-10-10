unit sed_plugins;

interface
    uses System.Win.ComObj;
const
sedCloseEnough = 10e-5;

{Get/SetProjectType, NewLevel}
spt_jkdf2 = 0;
spt_mots  = 1;
spt_ijim  = 2;

{Get/SetLevelHeader flags}
lh_version     = 1;
lh_gravity     = 2;
lh_skyZ        = 4;
lh_cskyOffs    = 8;
lh_horDist     = $10;
lh_horPPR      = $20;
lh_hskyOffs    = $40;
lh_mipmapDist  = $80;
lh_lodDist     = $100;
lh_perspDist   = $200;
lh_gouraudDist = $400;
lh_ppu         = $800;
lh_masterCmp   = $1000;
lh_fog         = $2000;
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
sf_surfflags   = 4;
sf_faceflags   = 8;
sf_material    = $10;
sf_geo         = $20;
sf_light       = $40;
sf_tex         = $80;
sf_extralight  = $100;
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
mm_sc = 0; {Sectors}
mm_sf = 1; {surfaces}
mm_vx = 2; {vertices}
mm_th = 3; {Things}
mm_ed = 4; {Edges}
mm_lt = 5; {Lights}
mm_fr = 6; {frames}

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
sc_all      = 3;

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
ef_geomode   = 7;
ef_lightmode = 8;
ef_texmode   = 9;

{PanMessage constants}
msg_info    = 0;
msg_warning = 1;
msg_error   = 2;

{SendKey flags}
sk_ctrl  = 1;
sk_shift = 2;
sk_alt   = 4;

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

{GetSedString constants}
ss_projectDir  = 1;
ss_sedDir      = 2;
ss_gameDir     = 3;
ss_levelFile   = 4;
ss_configFile  = 5; // ex js_jedregkey
ss_lecLogoJK   = 6;
ss_recent1     = 7;
ss_recent2     = 8;
ss_recent3     = 9;
ss_recent4     = 10;
ss_res1Gob     = 11;
ss_res2Gob     = 12;
ss_spGob       = 13;
ss_mp1Gob      = 14;
ss_mp2Gob      = 15;
ss_mp3Gob      = 16;
ss_lecLogoIJIM = 17;

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
rc_Current    = 0;
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

TSedVector2 = record
  x, y: Double;
end;

TSedVector3 = record
  case integer of
    0: (dx,dy,dz: Double);
    1: (x,y,z: Double);
    2: (pitch, yaw, roll: Double);
end;

TSedColor = record
    r, g, b, a: Single;  // a - alpha or light intensity
end;

TSedBox = record
  p1: TSedVector3;
  p2: TSedVector3;
end;

TSedPointLight = record
  color: TSedColor;
  position: TSedVector3;
  minRange: Double;  // minimum light range at which light will still illuminate objects
  maxRange: Double;  // maximum light intensity range before it starts to falloff
end;

TSedFog = record
  enabled: Boolean;
  color: TSedColor;
  fogStart: Double;
  fogEnd: Double;
end;

TSedLevelHeader = record
  version: Longint;
  gravity: Double;
  ceilingSkyHeight: Double;
  ceilingSkyOffset: TSedVector2;
  horizonSkyDistance,
  horizonSkyPixelsPerRev: Double;
  horizonSkyOffset: TSedVector2;
  mipMapDistances: array[0..3] of Double;
  lodDistances: array[0..3] of Double;
  perspectiveDistance,
  gouraudDistance: Double;
  ppunit: Double;  // pixels per unit
  aMasterCmp: PChar;
  fog: TSedFog;
end;

TSedSectorRec = record
  flags: Longint;
  ambient: TSedColor;
  extraLight: TSedColor;
  pointLight: TSedPointLight;
  aColorMap: PChar;
  tint: TSedColor;
  aSound: PChar;
  soundVolume: Double;
  thrust: TSedVector3;
  layer: Longint;
end;

TSedSurfaceRec = record
  adjoinSec, adjoinSurf: Longint;
  adjoinflags,
  surfflags,
  faceflags: Longint;
  aMaterial: PChar;
  geo,light,tex: Longint;
  extraLight: TSedColor;
  uscale,vscale: Single;
end;

TSedThingRec = record
  aName: PChar;
  sector: Longint;
  position: TSedVector3;
  rotation: TSedVector3;
  layer: Longint;
end;

TSedLightRec = record
  flags: Longint;
  layer: Longint;
  color: TSedColor;
  intensity: Double;
  range: Double;
  position: TSedVector3;
end;

{endtype}

{ Wireframe renderer interface}
{JED 0.93}
ISedWFRenderer = class
  function QueryInterface(const IID: TGUID; out Obj): HResult; virtual; stdcall; abstract;
  function AddRef: Integer; virtual; stdcall; abstract;
  function Release: Integer; virtual; stdcall; abstract;

  {Renderer attributes}
  function GetRendererDouble(what: Integer): Double; virtual; stdcall; abstract;
  procedure SetRendererDouble(what: Integer; val: Double); virtual; stdcall; abstract;
  procedure GetRendererVector(what: Integer; var x,y,z: Double); virtual; stdcall; abstract;
  procedure SetRendererVector(what: Integer; x,y,z: Double); virtual; stdcall; abstract;

  function NSelected: Integer; virtual; stdcall; abstract;
  function GetNSelected(n: Integer): Integer; virtual; stdcall; abstract;
  procedure SetViewPort(x,y,w,h: Integer); virtual; stdcall; abstract;
  procedure SetColor(what,r,g,b: Byte); virtual; stdcall; abstract;
  procedure SetPointSize(size: Double); virtual; stdcall; abstract;
  procedure BeginScene; virtual; stdcall; abstract;
  procedure EndScene; virtual; stdcall; abstract;
  procedure SetCulling(how: Integer); virtual; stdcall; abstract;

  procedure DrawSector(sc: Integer); virtual; stdcall; abstract;
  procedure DrawSurface(sc,sf: Integer); virtual; stdcall; abstract;
  procedure DrawThing(th: Integer); virtual; stdcall; abstract;

  procedure DrawLine(x1,y1,z1, x2,y2,z2: Double); virtual; stdcall; abstract;
  procedure DrawVertex(X,Y,Z: Double); virtual; stdcall; abstract;
  procedure DrawGrid; virtual; stdcall; abstract;

  procedure BeginPick(x,y: Integer); virtual; stdcall; abstract;
  procedure EndPick; virtual; stdcall; abstract;

  procedure PickSector(sc: Integer; id: Integer); virtual; stdcall; abstract;
  procedure PickSurface(sc,sf: Integer;id: Integer); virtual; stdcall; abstract;
  procedure PickLine(x1,y1,z1, x2,y2,z2: Double;id: Integer); virtual; stdcall; abstract;
  procedure PickVertex(X,Y,Z: Double; id: Integer); virtual; stdcall; abstract;

  procedure BeginRectPick(x1,y1,x2,y2: Integer); virtual; stdcall; abstract;
  procedure EndRectPick; virtual; stdcall; abstract;
  function IsSectorInRect(sc: Integer): Boolean; virtual; stdcall; abstract;
  function IsSurfaceInRect(sc,sf: Integer): Boolean; virtual; stdcall; abstract;
  function IsLineInRect(x1,y1,z1, x2,y2,z2: Double): Boolean; virtual; stdcall; abstract;
  function IsVertexInRect(X,Y,Z: Double): Boolean; virtual; stdcall; abstract;

  function GetXYZonPlaneAt(scX,scY: Integer; const pnormal:TSedVector3; pX,pY,pZ: Double; var X,Y,Z: Double): Boolean; virtual; stdcall; abstract;
  function GetGridAt(scX,scY: Integer; var X,Y,Z: Double): Boolean; virtual; stdcall; abstract;
  procedure GetNearestGridNode(iX,iY,iZ: Double; Var X,Y,Z: Double); virtual; stdcall; abstract;
  procedure ProjectPoint(x,y,z: Double; Var winX,winY: Integer); virtual; stdcall; abstract;
  procedure UnProjectPoint(winX,winY: Integer; WinZ: Double; var x,y,z: Double); virtual; stdcall; abstract;
  function IsSurfaceFacing(sc,sf: Integer): Boolean; virtual; stdcall; abstract;

  function HandleWMQueryPal: Integer; virtual; stdcall; abstract;
  function HandleWMChangePal: Integer; virtual; stdcall; abstract;
end;

ISedLevel = class
  function QueryInterface(const IID: TGUID; out Obj): HResult; virtual; stdcall; abstract;
  function AddRef: Integer; virtual; stdcall; abstract;
  function Release: Integer; virtual; stdcall; abstract;

  {Level header}
  procedure GetLevelHeader(var lh: TSedLevelHeader; flags: Integer); virtual; stdcall; abstract;
  procedure SetLevelHeader(const lh: TSedLevelHeader; flags: Integer); virtual; stdcall; abstract;

  function NumSectors: Integer; virtual; stdcall; abstract;
  function NumThings: Integer; virtual; stdcall; abstract;
  function NumLights: Integer; virtual; stdcall; abstract;
  function NumCogs: Integer; virtual; stdcall; abstract;

  {Sectors}
  function SectorAdd: Integer; virtual; stdcall; abstract;
  procedure SectorDelete(n: Integer); virtual; stdcall; abstract;

  procedure SectorGet(sec: Integer; var rec: TSedSectorRec; flags: Integer); virtual; stdcall; abstract;
  procedure SectorSet(sec: Integer; const rec: TSedSectorRec; flags: Integer); virtual; stdcall; abstract;

  function SectorNumVertices(sec: Integer): Integer; virtual; stdcall; abstract;
  function SectorNumSurfaces(sec: Integer): Integer; virtual; stdcall; abstract;

  procedure SectorGetVertex(sec, vn: Integer; var vert: TSedVector3); virtual; stdcall; abstract;
  procedure SectorSetVertex(sec, vn: Integer; const vert: TSedVector3); virtual; stdcall; abstract;

  function SectorAddVertex(sec: Integer; const vert: TSedVector3): Integer; virtual; stdcall; abstract;
  function SectorFindVertex(sec: Integer; const vert: TSedVector3): Integer; virtual; stdcall; abstract;
  function SectorDeleteVertex(sec: Integer; vn: Integer): Integer; virtual; stdcall; abstract;

  function SectorAddSurface(sec: Integer): Integer; virtual; stdcall; abstract;
  procedure SectorDeleteSurface(sec,surf: Integer); virtual; stdcall; abstract;
  procedure SectorUpdate(sec: Integer); virtual; stdcall; abstract;

  {Surfaces}
  procedure SurfaceGet(sec, surf: Integer; var rec: TSedSurfaceRec; flags: Integer); virtual; stdcall; abstract;
  procedure SurfaceSet(sec, surf: Integer; const rec: TSedSurfaceRec; flags: Integer); virtual; stdcall; abstract;
  procedure SurfaceGetNormal(sec,surf: Integer; var normal: TSedVector3); virtual; stdcall; abstract;
  procedure SurfaceUpdate(sec, surf: Integer; how: Integer); virtual; stdcall; abstract;
  function SurfaceNumVertices(sec, surf: Integer): Integer; virtual; stdcall; abstract;
  function SurfaceGetVertexNum(sec, surf, vn: Integer): Integer; virtual; stdcall; abstract;
  procedure SurfaceSetVertexNum(sec, surf, vn: Integer; secvx: Integer); virtual; stdcall; abstract;
  function SurfaceAddVertex(sec, surf: integer; secvn: Integer): Integer; virtual; stdcall; abstract;
  function SurfaceInsertVertex(sec, surf: Integer; at: Integer; secvn: Integer): Integer; virtual; stdcall; abstract;
  procedure SurfaceDeleteVertex(sec, surf: Integer; n: Integer) ; virtual; stdcall; abstract;
  procedure SurfaceGetVertexUV(sec, surf, vn: Integer; var u,v: Single); virtual; stdcall; abstract;
  procedure SurfaceSetVertexUV(sec, surf, vn: integer; u,v: Single); virtual; stdcall; abstract;
  procedure SurfaceGetVertexLight(sec, surf, vn: Integer; var color: TSedColor);   virtual; stdcall; abstract;
  procedure SurfaceSetVertexLight(sec, surf, vn: Integer; const color: TSedColor);   virtual; stdcall; abstract;

  {Things}
  function ThingAdd: Integer;   virtual; stdcall; abstract;
  procedure ThingDelete(th: Integer); virtual; stdcall; abstract;
  procedure ThingGet(th: Integer; var rec: TSedThingRec; flags: Integer); virtual; stdcall; abstract;
  procedure ThingSet(th: Integer; const rec: TSedThingRec; flags: Integer); virtual; stdcall; abstract;
  procedure ThingUpdate(th: Integer); virtual; stdcall; abstract;

  {Lights}
  function LightAdd:Integer; virtual; stdcall; abstract;
  procedure LightDelete(lt: Integer); virtual; stdcall; abstract;
  procedure LightGet(lt: Integer; var rec: TSedLightRec; flags: Integer); virtual; stdcall; abstract;
  procedure LightSet(lt: Integer; const rec: TSedLightRec; flags: Integer); virtual; stdcall; abstract;
  procedure LightUpdate(lt: Integer); virtual; stdcall; abstract;

  {Layers}
  function NumLayers: Integer; virtual; stdcall; abstract;
  function LayerGetName(n: Integer): PChar; virtual; stdcall; abstract;
  function LayerAdd(const name: PChar): Integer; virtual; stdcall; abstract;

  {JED 0.92}
  function ThingNumValues(th: Integer): Integer; virtual; stdcall; abstract;
  function ThingValueGetName(th, n: Integer): PChar; virtual; stdcall; abstract;
  function ThingValueGetData(th, n: Integer): PChar; virtual; stdcall; abstract;
  procedure ThingValueSetData(th,n: Integer; const val: PChar); virtual; stdcall; abstract;

  function ThingValueAdd(th: integer; const name, val: PChar): Integer; virtual; stdcall; abstract;
  procedure ThingValueInsert(th,n: Integer; const name,val: PChar); virtual; stdcall; abstract;
  procedure ThingValueDelete(th,n: Integer); virtual; stdcall; abstract;

  procedure ThingFrameGet(th,n: Integer; var pos, pyr: TSedVector3); virtual; stdcall; abstract;
  procedure ThingFrameSet(th,n: Integer; const pos, pyr: TSedVector3); virtual; stdcall; abstract;

  {COGs}
  function CogAdd(const name: PChar): Integer; virtual; stdcall; abstract;
  procedure CogDelete(n: Integer); virtual; stdcall; abstract;
  procedure CogUpdate(cg: Integer); virtual; stdcall; abstract;
  function CogGetFilename(cg: Integer): PChar; virtual; stdcall; abstract;

  function CogNumValues(cg: Integer): Integer; virtual; stdcall; abstract;
  function CogValueGetName(cg,n: Integer): PChar; virtual; stdcall; abstract;
  function CogValueGetType(cg,n: Integer): Integer; virtual; stdcall; abstract;

  function CogValueGet(cg,n: Integer): PChar; virtual; stdcall; abstract;
  function CogValueSet(cg,n: Integer; const val: PChar): Boolean; virtual; stdcall; abstract;

  function CogValueAdd(cg: Integer; const name,val: PChar; vtype: Integer): Integer; virtual; stdcall; abstract;
  procedure CogValueInsert(cg,n: Integer; const name,val: PChar; vtype: Integer); virtual; stdcall; abstract;
  procedure CogValueDelete(cg,n: integer); virtual; stdcall; abstract;
end;

ISed = class
  function QueryInterface(const IID: TGUID; out Obj): HResult; virtual; stdcall; abstract;
  function AddRef: Integer; virtual; stdcall; abstract;
  function Release: Integer; virtual; stdcall; abstract;

  function GetVersion: Double; virtual; stdcall; abstract; {version}
  function GetLevel: ISedLevel; virtual; stdcall; abstract;

  function GetMapMode: Integer; virtual; stdcall; abstract;
  procedure SetMapMode(mode: Integer); virtual; stdcall; abstract;
  function GetCurrentSector: Integer; virtual; stdcall; abstract;
  procedure SetCurrentSector(sc: Integer); virtual; stdcall; abstract;
  function GetCurrentThing: Integer; virtual; stdcall; abstract;
  procedure SetCurrentThing(th: Integer); virtual; stdcall; abstract;
  function GetCurrentLight: Integer; virtual; stdcall; abstract;
  procedure SetCurrentLight(lt: Integer); virtual; stdcall; abstract;
  procedure GetCurrentVertex(var sec,vn: Integer); virtual; stdcall; abstract;
  procedure SetCurrentVertex(sec,vn: Integer); virtual; stdcall; abstract;
  procedure GetCurrentSurface(var sec,surf: Integer); virtual; stdcall; abstract;
  procedure SetCurrentSurface(sec,surf: Integer); virtual; stdcall; abstract;
  procedure GetCurrentEdge(var sec,surf,ed: Integer); virtual; stdcall; abstract;
  procedure SetCurrentEdge(sec,surf,ed: Integer); virtual; stdcall; abstract;
  procedure GetCurrentFrame(var th,fr: Integer); virtual; stdcall; abstract;
  procedure SetCurrentFrame(th,fr: Integer); virtual; stdcall; abstract;

  procedure NewLevel(kind: integer); virtual; stdcall; abstract; // SED 0.1.0
  procedure LoadLevel(const name: PChar); virtual; stdcall; abstract;

  {Different level editing functions}
  procedure RotateVector(var vec: TSedVector3; const pyr: TSedVector3); virtual; stdcall; abstract;
  procedure RotatePoint(const point, pivot: TSedVector3; angle: Double; var point2: TSedVector3); virtual; stdcall; abstract;
  procedure GetPYR(const x, y, z: TSedVector3; var pyr: TSedVector3); virtual; stdcall; abstract;

  function MergeSectors(sec1, sec2: Integer): Integer; virtual; stdcall; abstract;
  function CleaveSector(sec: Integer; const cnormal: TSedVector3; const cp: TSedVector3): Integer; virtual; stdcall; abstract;
  function CreateCubicSector(const pos: TSedVector3; const pnormal, edge: TSedVector3): Integer; virtual; stdcall; abstract;
  function IsSectorConvex(sec: Integer): Boolean; virtual; stdcall; abstract;
  function IsInSector(sec: Integer; const point: TSedVector3): Boolean; virtual; stdcall; abstract;
  function DoSectorsOverlap(sec1, sec2: Integer): Boolean; virtual; stdcall; abstract;
  procedure FindBoundingBox(sec: Integer; var box: TSedBox); virtual; stdcall; abstract;
  procedure FindBoundingSphere(sec: Integer; var center: TSedVector3; radius: Double); virtual; stdcall; abstract;
  function FindCollideBox(sec: Integer; const bbox: TSedBox; const center: TSedVector3; var cbox: TSedBox): Boolean; virtual; stdcall; abstract;
  function FindSectorForThing(th: Integer): Integer; virtual; stdcall; abstract;
  function FindSectorForXYZ(X, Y, Z: Double): Integer; virtual; stdcall; abstract;

  procedure FindSurfaceCenter(sec, surf: Integer; var center: TSedVector3); virtual; stdcall; abstract;
  function IsSurfaceConvex(sec, surf: Integer): Boolean; virtual; stdcall; abstract;
  function IsSurfacePlanar(sec, surf: Integer): Boolean; virtual; stdcall; abstract;
  function IsPointOnSurface(sec, surf: Integer; const point: TSedVector3): Boolean; virtual; stdcall; abstract;

  function CleaveSurface(sec, surf: Integer; const cnormal: TSedVector3; cp: TSedVector3): Integer; virtual; stdcall; abstract;
  function CleaveEdge(sec, surf, ed: Integer; const cnormal: TSedVector3; cp: TSedVector3): Boolean; virtual; stdcall; abstract;
  function ExtrudeSurface(sec, surf: Integer; by: Double): Integer; virtual; stdcall; abstract;
  function JoinSurfaces(sec1, surf1, sec2, surf2: Integer): Boolean; virtual; stdcall; abstract;
  function MergeSurfaces(sec, surf1, surf2: Integer): Integer; virtual; stdcall; abstract;
  function PlanarizeSurface(sec, surf: Integer): Boolean; virtual; stdcall; abstract;

  procedure CalculateDefaultUVNormals(sec, surf: Integer; orgvx: Integer; var un, vn: TSedVector3); virtual; stdcall; abstract;
  procedure CalculateUVNormals(sec, surf: Integer; var un, vn: TSedVector3); virtual; stdcall; abstract;
  procedure ArrangeTexture(sec, surf: Integer; orgvx: Integer; const un, vn: TSedVector3); virtual; stdcall; abstract;
  procedure ArrangeTextureBy(sec, surf: Integer; const un, vn: TSedVector3; const refp: TSedVector3; refu, refv: Double); virtual; stdcall; abstract;
  function IsTextureFlipped(sec, surf: Integer): Boolean; virtual; stdcall; abstract;
  function StitchSurfaces(sc1, sf1, sc2, sf2: Integer): Boolean; virtual; stdcall; abstract;

  procedure RemoveSurfaceReferences(sec, surf: Integer); virtual; stdcall; abstract;
  procedure RemoveSectorReferences(sec: Integer; surfs: Boolean); virtual; stdcall; abstract;
  function FindCommonEdges(sc1, sf1, sc2, sf2: Integer; var v11, v12, v21, v22: Integer): Boolean; virtual; stdcall; abstract;
  function DoSurfacesOverlap(sc1, sf1, sc2, sf2: Integer): Boolean; virtual; stdcall; abstract;

  function MakeAdjoin(sec, surf: integer): Boolean; virtual; stdcall; abstract;
  function MakeAdjoinFromSectorUp(sec: Integer; surf: Integer; firstSec: Integer): Boolean; virtual; stdcall; abstract;
  function UnAdjoin(sec: Integer; surf: Integer): Boolean; virtual; stdcall; abstract;

  procedure StartUndo(const name: PChar); virtual; stdcall; abstract;
  procedure SaveUndoForThing(n: Integer; change: Integer); virtual; stdcall; abstract;
  procedure SaveUndoForLight(n: Integer; change: Integer); virtual; stdcall; abstract;
  procedure SaveUndoForSector(n: Integer; change: Integer; whatPart: Integer); virtual; stdcall; abstract;
  procedure ClearUndoBuffer; virtual; stdcall; abstract;
  procedure ApplyUndo; virtual; stdcall; abstract;

  {Added in JED 0.92}
  function GetApplicationHandle: Integer; virtual; stdcall; abstract;
  function JoinSectors(sec1, sec2: Integer): Boolean; virtual; stdcall; abstract;

  function GetNumMultiselected(what: Integer): Integer; virtual; stdcall; abstract;
  procedure ClearMultiselection(what: Integer); virtual; stdcall; abstract;
  procedure RemoveFromMultiselection(what, n: Integer); virtual; stdcall; abstract;

  function GetSelectedSector(n: Integer): Integer; virtual; stdcall; abstract;
  procedure GetSelectedSurface(n: Integer; var sec, surf: Integer); virtual; stdcall; abstract;
  procedure GetSelectedEdge(n: Integer; var sec, surf, ed: Integer); virtual; stdcall; abstract;
  procedure GetSelectedVertex(n: Integer; var sec, vn: Integer); virtual; stdcall; abstract;
  function GetSelectedThing(n: Integer): Integer; virtual; stdcall; abstract;
  procedure GetSelectedFrame(n: Integer; var th, fr: Integer); virtual; stdcall; abstract;
  function GetSelectedLight(n: Integer): Integer; virtual; stdcall; abstract;

  function SelectSector(sec: Integer): Integer; virtual; stdcall; abstract;
  function SelectSurface(sec, surf: Integer): Integer; virtual; stdcall; abstract;
  function SelectEdge(sec, surf, ed: Integer): Integer; virtual; stdcall; abstract;
  function SelectVertex(sec, vn: Integer): Integer; virtual; stdcall; abstract;
  function SelectThing(th: Integer): Integer; virtual; stdcall; abstract;
  function SelectFrame(th, fr: Integer): Integer; virtual; stdcall; abstract;
  function SelectLight(lt: Integer): Integer; virtual; stdcall; abstract;

  function FindSelectedSector(sec: Integer): Integer; virtual; stdcall; abstract;
  function FindSelectedSurface(sec, surf: Integer): Integer; virtual; stdcall; abstract;
  function FindSelectedEdge(sec, surf, ed: Integer): Integer; virtual; stdcall; abstract;
  function FindSelectedVertex(sec, vn: Integer): Integer; virtual; stdcall; abstract;
  function FindSelectedThing(th: Integer): Integer; virtual; stdcall; abstract;
  function FindSelectedFrame(th, fr: Integer): Integer; virtual; stdcall; abstract;
  function FindSelectedLight(lt: Integer): Integer; virtual; stdcall; abstract;

  procedure Save(const filename: PChar); virtual; stdcall; abstract;
  procedure SaveAsSed(const filename: PChar); virtual; stdcall; abstract;
  procedure UpdateMap; virtual; stdcall; abstract;
  procedure SetPickerCmp(const cmp: PChar); virtual; stdcall; abstract;
  function PickResource(what: Integer; const cur: PChar): PChar; virtual; stdcall; abstract;
  function EditFlags(what: Integer; flags: LongInt): LongInt; virtual; stdcall; abstract;

  procedure ReloadTemplates; virtual; stdcall; abstract;
  procedure PanMessage(mType: Integer; const msg: PChar); virtual; stdcall; abstract;
  procedure SendKey(shift: Integer; key: Integer); virtual; stdcall; abstract;
  function ExecuteMenu(const itemRef: PChar): Boolean; virtual; stdcall; abstract;
  function GetSedSetting(const name: PChar): Variant; virtual; stdcall; abstract;
  function IsLayerVisible(n: Integer): Boolean; virtual; stdcall; abstract;

  procedure CheckConsistencyErrors; virtual; stdcall; abstract;
  procedure CheckResources; virtual; stdcall; abstract;
  function GetNumConsistencyErrors: Integer; virtual; stdcall; abstract;
  function GetConsistencyErrorString(n: Integer): PChar; virtual; stdcall; abstract;
  function GetConsistencyErrorType(n: Integer): Integer; virtual; stdcall; abstract;
  function GetConsistencyErrorItemType(n: Integer): Integer; virtual; stdcall; abstract; // SED 0.1.0
  function GetConsistencyErrorSector(n: Integer): Integer; virtual; stdcall; abstract;
  function GetConsistencyErrorSurface(n: Integer): Integer; virtual; stdcall; abstract;
  function GetConsistencyErrorThing(n: Integer): Integer; virtual; stdcall; abstract;
  function GetConsistencyErrorCOG(n: Integer): Integer; virtual; stdcall; abstract;
  function IsPreviewActive: Boolean; virtual; stdcall; abstract;
  function GetSedString(what: Integer): PChar; virtual; stdcall; abstract;
  function GetProjectType: Integer; virtual; stdcall; abstract; // SED 0.1.0
  procedure SetProjectType(kind: Integer); virtual; stdcall; abstract; // SED 0.1.0

  {JED 0.93}
  function GetSedWindow(whichOne: Integer): Integer; virtual; stdcall; abstract;

  function FileExtractExt(const filename: PChar): PChar; virtual; stdcall; abstract;
  function FileExtractPath(const filename: PChar): PChar; virtual; stdcall; abstract;
  function FileExtractName(const filename: PChar): PChar; virtual; stdcall; abstract;
  function FindProjectDirFile(const filename: PChar): PChar; virtual; stdcall; abstract;
  function IsFileContainer(const filename: PChar): Boolean; virtual; stdcall; abstract;
  function IsFileInContainer(const filename: PChar): Boolean; virtual; stdcall; abstract;

  function FileOpenDialog(const name: PChar; filter: PChar): PChar; virtual; stdcall; abstract;

  function OpenFile(const filename: PChar): Integer; virtual; stdcall; abstract;
  function OpenGameFile(const filename: PChar): Integer; virtual; stdcall; abstract;
  function ReadFile(handle: Integer; buf: Pointer; size: LongInt): Integer; virtual; stdcall; abstract;
  procedure SetFilePos(handle: Integer; pos: LongInt); virtual; stdcall; abstract;
  function GetFilePos(handle: Integer): LongInt; virtual; stdcall; abstract;
  function GetFileSize(handle: Integer): LongInt; virtual; stdcall; abstract;
  function GetFileName(handle: Integer): PChar; virtual; stdcall; abstract;
  procedure CloseFile(handle: Integer); virtual; stdcall; abstract;

  function OpenTextFile(const filename: PChar): Integer; virtual; stdcall; abstract;
  function OpenGameTextFile(const filename: PChar): Integer; virtual; stdcall; abstract;
  function GetTextFileName(handle: Integer): PChar; virtual; stdcall; abstract;
  function ReadTextFileString(handle: Integer): PChar; virtual; stdcall; abstract;
  function TextFileEOF(handle: Integer): Boolean; virtual; stdcall; abstract;
  function TextFileCurrentLine(handle: Integer): Integer; virtual; stdcall; abstract;
  procedure CloseTextFile(handle: Integer); virtual; stdcall; abstract;

  function GobOpen(const filename: PChar): Integer; virtual; stdcall; abstract;
  function GobNumFiles(handle: Integer): Integer; virtual; stdcall; abstract;
  function GobGetFilename(handle: Integer; n: Integer): PChar; virtual; stdcall; abstract;
  function GobGetFullFilename(handle: Integer; n: Integer): PChar; virtual; stdcall; abstract;
  function GobGetFileSize(handle: Integer; n: Integer): LongInt; virtual; stdcall; abstract;
  function GobGetFileOffset(handle: Integer; n: Integer): LongInt; virtual; stdcall; abstract;
  procedure GobClose(handle: Integer); virtual; stdcall; abstract;

  function CreateWFRenderer(wnd: Integer; whichOne: Integer): ISedWFRenderer; virtual; stdcall; abstract;
end;


{cvtstop}
{ The function that SED calls to load your plug-in
must be named SedPluginLoad (__cdecl calling) or
SedPluginLoadStdCall (standard stack calling)
and have one parameter ISed interface object. The
function should return true if the plug-in was
loaded/activated successfully and false otherwise.
Warning! Names are case-sensitive }

// This is how the Plug-in entry function should be declared
// function TSedPluginLoadStdCall(sed: ISed): Boolean; stdcall;
// The old style:
// function TSedPluginLoad(sed:ISed): Boolean;
// is still supported, but not recommended

TSedPluginLoad = Function(sed: ISed): Boolean;
TSedPluginLoadStdCall = Function(sed: ISed): Boolean; stdcall;


{ General purpose functions. Local }
procedure SetVector(var vec: TSEDVector3; dx, dy, dz: Double);
function DotProduct(const vec1, vec2: TSEDVector3): Double;

procedure CrossProduct(const vec1, vec2: TSEDVector3; var vec: TSEDVector3);
function VectorLen(const vec: TSEDVector3): Double;
function NormalizeVector(var vec: TSEDVector3): Boolean;
function DoBoxesIntersect(const box1, box2: TSEDBox): Boolean;
procedure SetBox(var box: TSEDBox; x1,x2,y1, y2,z1,z2: Double);
function IsPointInBox(const box: TSEDBox; x, y, z: Double): Boolean;
function LinePlaneIntersection(const normal: TSEDVector3; pX,pY,pZ, x1,y1,z1, x2,y2,z2: Double; var x, y, z: Double): Boolean;
function MinDouble(d1, d2: Double): Double;

implementation

function MinDouble(d1, d2: Double): Double;
begin
  if d1 < d2 then result := d1 else result := d2;
end;

procedure SetVector(var vec: TSEDVector3; dx, dy, dz: Double);
begin
  vec.dx := dx;
  vec.dy := dy;
  vec.dz := dz;
end;

function DotProduct(const vec1, vec2: TSEDVector3): Double;
begin
  result := vec1.dx * vec2.dx + vec1.dy * vec2.dy + vec1.dz * vec2.dz;
end;

procedure CrossProduct(const vec1, vec2: TSEDVector3; var vec: TSEDVector3);
begin
  vec.dx := vec1.dy * vec2.dz - vec2.dy * vec1.dz;
  vec.dy := vec2.dx * vec1.dz - vec1.dx * vec2.dz;
  vec.dz := vec1.dx * vec2.dy - vec2.dx * vec1.dy;
end;

function VectorLen(const vec: TSEDVector3): Double;
begin
  result := sqrt(sqr(vec.dx) + sqr(vec.dy) + sqr(vec.dz));
end;

function NormalizeVector(var vec: TSEDVector3): Boolean;
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

function DoBoxesIntersect(const box1, box2: TSEDBox): Boolean;
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

procedure SetBox(var box: TSEDBox; x1,x2,y1, y2,z1,z2: Double);
begin
  box.p1.x := MinDouble(x1, x2);
  box.p2.x := MinDouble(x1, x2);
  box.p1.y := MinDouble(y1, y2);
  box.p2.y := MinDouble(y1, y2);
  box.p1.z := MinDouble(z1, z2);
  box.p2.z := MinDouble(z1, z2);
end;

function IsPointInBox(const box: TSEDBox; x, y, z: Double): Boolean;
begin
  result := (x - box.p1.x >= -SEDCloseEnough) and (x - box.p2.x <= SEDCloseEnough) and
            (y - box.p1.y >= -SEDCloseEnough) and (y - box.p2.y <= SEDCloseEnough) and
            (z - box.p1.z >= -SEDCloseEnough) and (z - box.p2.z <= SEDCloseEnough);
end;

function LinePlaneIntersection(const normal: TSEDVector3; pX,pY,pZ, x1,y1,z1, x2,y2,z2: Double; var x, y, z: Double): Boolean;
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
