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
s_flags   = 1;
s_ambient = 2;
s_extra   = 4;
s_cmp     = 8;
s_tint    = $10;
s_sound   = $20;
s_sndvol  = $40;
s_layer   = $80;
s_all     = $FF;

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
lt_rgb          = 8;
lt_rgbintensity = $10;
lt_rgbrange     = $20;
lt_flags        = $40;
lt_layer        = $80;
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
su_texture   = 1;
su_floorflag = 2;
su_sector    = 4;
su_all       = 7;

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
SS_CDDir       = 3;
SS_GameDir     = 4;
SS_LevelFile   = 5;
SS_ConfigFile  = 6; // ex js_jedregkey
SS_LECLogoJK   = 7;
SS_Recent1     = 8;
SS_Recent2     = 9;
SS_Recent3     = 10;
SS_Recent4     = 11;
SS_Res1gob     = 12;
SS_Res2gob     = 13;
SS_SPGOB       = 14;
SS_MP1GOB      = 15;
SS_MP2GOB      = 16;
SS_MP3GOB      = 17;
SS_LECLogoIJIM = 18;

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

TSEDVector = record
  case integer of
    0: (dx,dy,dz:double);
    1: (x,y,z:double);
end;

TSEDColorF = record
    r, g, b, a: single;  // a - alpha or light intensity
end;

TSEDBox = record
  X1,Y1,Z1,
  X2,Y2,Z2: double;
end;

TSEDFog = record
  Enabled: Boolean;
  Color: TSEDColorF;
  FogStart: double;
  FogEnd: double;
end;

TSEDLevelHeader = record
  version: Longint;
  Gravity: double;
  CeilingSkyZ: double;
  CeilingSkyOffs:array[0..1] of double;
  HorDistance,HorPixelsPerRev:double;
  HorSkyOffs:array[0..1] of double;
  MipMapDist:array[0..3] of double;
  LODDist:array[0..3] of double;
  PerspDist, GouraudDist: double;
  PixelPerUnit: double;
  MasterCMP: PChar;
  Fog: TSEDFog;
end;

TSEDSectorRec=record
  Flags: longint;
  Ambient: TSEDColorF;
  ExtraLight: TSEDColorF;
  ColorMap: PChar;
  Tint: TSEDColorF;
  Sound: PChar;
  snd_vol: double;
  layer: longint;
end;

TSEDSurfaceRec=record
  adjoinSC,AdjoinSF: longint;
  AdjoinFlags: Longint;
  SurfFlags,FaceFlags: Longint;
  Material: PChar;
  geo,light,tex: longint;
  ExtraLight: TSEDColorF;
  uscale,vscale: single;
end;

TSEDThingRec=record
  name: PChar;
  Sector: Longint;
  X,Y,Z: double;
  PCH,YAW,ROL: Double;
  layer: longint;
end;

TSEDLightRec=record
  X,Y,Z: double;
  Intensity: double;
  Range: double;
  R,G,B: Single;
  rgbintensity: single;
  rgbrange: double;
  flags: longint;
  layer: longint;
end;

{endtype}

{ Wireframe renderer interface}
{JED 0.93}
ISEDWFRenderer=class
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

  Function GetXYZonPlaneAt(scX,scY:integer;pnormal:TSEDVector; pX,pY,pZ:double; var X,Y,Z:double):Boolean;virtual;stdcall;abstract;
  Function GetGridAt(scX,scY:integer;var X,Y,Z:double):boolean;virtual;stdcall;abstract;
  Procedure GetNearestGridNode(iX,iY,iZ:double; Var X,Y,Z:double);virtual;stdcall;abstract;
  Procedure ProjectPoint(x,y,z:double; Var WinX,WinY:integer);virtual;stdcall;abstract;
  Procedure UnProjectPoint(WinX,WinY:integer; WinZ:double; var x,y,z:double);virtual;stdcall;abstract;
  Function IsSurfaceFacing(sc,sf:integer):boolean;virtual;stdcall;abstract;

  Function HandleWMQueryPal:integer;virtual;stdcall;abstract;
  Function HandleWMChangePal:integer;virtual;stdcall;abstract;

end;

ISEDLevel=class
  {OLE2 crap. ignore}
  function QueryInterface(iid: pointer; var obj): LongInt; virtual; stdcall; abstract;
  function AddRef: Longint; virtual; stdcall; abstract;
  function Release: Longint; virtual; stdcall; abstract;

  {Level header}
  Procedure GetLevelHeader(var lh:TSEDLevelHeader;flags:integer);virtual;stdcall;abstract;
  Procedure SetLevelHeader(const lh:TSEDLevelHeader;flags:integer);virtual;stdcall;abstract;

  Function NSectors:integer;virtual;stdcall;abstract;
  Function NThings:integer;virtual;stdcall;abstract;
  Function NLights:integer;virtual;stdcall;abstract;
  Function NCOgs:integer;virtual;stdcall;abstract;

  {Sectors}
  Function AddSector:integer;virtual;stdcall;abstract;
  Procedure DeleteSector(n:integer);virtual;stdcall;abstract;

  Procedure GetSector(sec:integer;var rec:TSEDSectorRec;flags:integer);virtual;stdcall;abstract;
  Procedure SetSector(sec:integer;const rec:TSEDSectorRec;flags:integer);virtual;stdcall;abstract;

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
  Procedure GetSurface(sc,sf:integer;var rec:TSEDSurfaceRec;flags:integer);virtual;stdcall;abstract;
  Procedure SetSurface(sc,sf:integer;const rec:TSEDSurfaceRec;flags:integer);virtual;stdcall;abstract;
  Procedure GetSurfaceNormal(sc,sf:integer;var n:TSEDVector);virtual;stdcall;abstract;
  Procedure SurfaceUpdate(sc,sf:integer;how:integer);virtual;stdcall;abstract;
  Function SurfaceNVertices(sc,sf:integer):Integer;virtual;stdcall;abstract;
  Function SurfaceGetVertexNum(sc,sf,vx:integer):integer;virtual;stdcall;abstract;
  Procedure SurfaceSetVertexNum(sc,sf,vx:integer;secvx:integer);virtual;stdcall;abstract;
  Function SurfaceAddVertex(sc,sf:integer;secvx:integer):Integer;virtual;stdcall;abstract;
  Function SurfaceInsertVertex(sc,sf:integer;at:integer;secvx:integer):Integer;virtual;stdcall;abstract;
  Procedure SurfaceDeleteVertex(sc,sf:integer;n:integer);virtual;stdcall;abstract;
  Procedure SurfaceGetVertexUV(sc,sf,vx:integer;var u,v:single);virtual;stdcall;abstract;
  Procedure SurfaceSetVertexUV(sc,sf,vx:integer;u,v:single);virtual;stdcall;abstract;
  Procedure SurfaceGetVertexLight(sc, sf, vx: integer; var color: TSEDColorF); virtual; stdcall; abstract;
  Procedure SurfaceSetVertexLight(sc, sf, vx: integer; const color: TSEDColorF); virtual; stdcall; abstract;

  {Things}
  Function AddThing:Integer;virtual;stdcall;abstract;
  Procedure DeleteThing(th:integer);virtual;stdcall;abstract;
  Procedure GetThing(th:integer;var rec:TSEDThingRec;flags:integer);virtual;stdcall;abstract;
  Procedure SetThing(th:integer;const rec:TSEDThingRec;flags:integer);virtual;stdcall;abstract;
  Procedure ThingUpdate(th:integer);virtual;stdcall;abstract;

  {Lights}
  Function AddLight:Integer;virtual;stdcall;abstract;
  Procedure DeleteLight(lt:integer);virtual;stdcall;abstract;
  Procedure GetLight(lt:integer;var rec:TSEDLightRec;flags:integer);virtual;stdcall;abstract;
  Procedure SetLight(lt:integer;const rec:TSEDLightRec;flags:integer);virtual;stdcall;abstract;

  {Layers}
  Function NLayers:integer; virtual;stdcall;abstract;
  Function GetLayerName(n:integer): PChar; virtual;stdcall;abstract;
  Function AddLayer(name: PChar): integer; virtual;stdcall;abstract;

  {JED 0.92}
  Function NTHingValues(th:integer):integer; virtual;stdcall;abstract;
  Function GetThingValueName(th,n:Integer): PChar; virtual;stdcall;abstract;
  Function GetThingValueData(th,n:integer): PChar; virtual;stdcall;abstract;
  Procedure SetThingValueData(th,n:Integer;val: PChar); virtual;stdcall;abstract;

  Procedure GetThingFrame(th,n:Integer;var x,y,z,pch,yaw,rol:double);virtual;stdcall;abstract;
  Procedure SetThingFrame(th,n:Integer;x,y,z,pch,yaw,rol:double);virtual;stdcall;abstract;

  Function AddThingValue(th: integer; name,val: PChar): integer; virtual;stdcall;abstract;
  Procedure InsertThingValue(th,n: Integer; name,val: PChar); virtual;stdcall;abstract;
  Procedure DeleteThingValue(th,n: integer); virtual;stdcall;abstract;

  {COGs}
  Function AddCOG(name: PChar): Integer;virtual;stdcall;abstract;
  Procedure DeleteCOG(n: integer); virtual;stdcall;abstract;
  Procedure UpdateCOG(cg: integer); virtual;stdcall;abstract;

  Function GetCOGFileName(cg: integer): PChar;virtual;stdcall;abstract;
  Function NCOGValues(cg:integer): integer; virtual;stdcall;abstract;

  Function GetCOGValueName(cg,n: Integer): PChar; virtual;stdcall;abstract;
  Function GetCOGValueType(cg,n: Integer): integer; virtual;stdcall;abstract;

  Function GetCOGValue(cg,n: Integer): PChar; virtual;stdcall;abstract;
  Function SetCOGValue(cg,n: Integer; val:PChar): Boolean; virtual;stdcall;abstract;

  Function AddCOGValue(cg: integer; name,val: PChar; vtype: integer): integer; virtual;stdcall;abstract;
  Procedure InsertCOGValue(cg,n: Integer; name,val:PChar; vtype:integer); virtual;stdcall;abstract;
  Procedure DeleteCOGValue(cg,n: integer); virtual;stdcall;abstract;
end;

ISED=class
  {OLE2 crap. Ignore}
  function QueryInterface(iid: pointer; var obj): LongInt; virtual; stdcall; abstract;
  function AddRef: Longint; virtual; stdcall; abstract;
  function Release: Longint; virtual; stdcall; abstract;

  Function GetVersion:double;virtual;stdcall;abstract; {version}
  Function GetLevel:ISEDLevel;virtual;stdcall;abstract; {SEDApp OLE interafce}

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

  Procedure NewLevel(kind: integer); virtual;stdcall;abstract; // SED 0.1.0
  Procedure LoadLevel(name: PChar); virtual;stdcall;abstract;

  {Different level editing functions}

  Procedure FindBBox(sec:integer;var box:TSEDBox);virtual;stdcall;abstract;
  Procedure FindBoundingSphere(sec:integer;var CX,CY,CZ,Radius:double);virtual;stdcall;abstract;
  Function FindCollideBox(sec:integer;const bbox:TSEDBox;cx,cy,cz:double;var cbox:TSEDBox):boolean;virtual;stdcall;abstract;
  Procedure FindSurfaceCenter(sc,sf:integer;var cx,cy,cz:double);virtual;stdcall;abstract;
  Procedure RotateVector(var vec:TSEDVector; pch,yaw,rol:double);virtual;stdcall;abstract;
  Procedure RotatePoint(ax1,ay1,az1,ax2,ay2,az2:double;angle:double;var x,y,z:double);virtual;stdcall;abstract;
  Procedure GetJKPYR(const x,y,z:TSEDVector;var pch,yaw,rol:double);virtual;stdcall;abstract;
  Function IsSurfaceConvex(sc,sf:integer):boolean;virtual;stdcall;abstract;
  Function IsSurfacePlanar(sc,sf:integer):boolean;virtual;stdcall;abstract;
  Function IsSectorConvex(sec:integer):boolean;virtual;stdcall;abstract;
  Function IsInSector(sec:integer;x,y,z:double):boolean;virtual;stdcall;abstract;
  Function DoSectorsOverlap(sec1,sec2:integer):boolean;virtual;stdcall;abstract;
  Function IsPointOnSurface(sc,sf:integer;x,y,z:double):boolean;virtual;stdcall;abstract;
  Function FindSectorForThing(th:integer):Integer;virtual;stdcall;abstract;
  Function FindSectorForXYZ(X,Y,Z:double):integer;virtual;stdcall;abstract;
  Function ExtrudeSurface(sc,sf:integer; by:double):integer;virtual;stdcall;abstract;
  Function CleaveSurface(sc,sf:integer; const cnormal:TSEDVector; cx,cy,cz:double):integer;virtual;stdcall;abstract;
  Function CleaveSector(sec:integer; const cnormal:TSEDVector; cx,cy,cz:double):integer;virtual;stdcall;abstract;
  Function CleaveEdge(sc,sf,ed:integer; const cnormal:TSEDVector; cx,cy,cz:double):boolean;virtual;stdcall;abstract;
  Function JoinSurfaces(sc1,sf1,sc2,sf2:Integer):boolean;virtual;stdcall;abstract;
  Function PlanarizeSurface(sc,sf:integer):boolean;virtual;stdcall;abstract;
  function MergeSurfaces(sc,sf1,sf2:integer):integer;virtual;stdcall;abstract;
  function MergeSectors(sec1,sec2:integer):integer;virtual;stdcall;abstract;
  Procedure CalculateDefaultUVNormals(sc,sf:integer; orgvx:integer; var un,vn:TSEDVector);virtual;stdcall;abstract;
  Procedure CalcUVNormals(sc,sf:integer; var un,vn:TSEDVector);virtual;stdcall;abstract;
  Procedure ArrangeTexture(sc,sf:integer; orgvx:integer; const un,vn:TSEDVector);virtual;stdcall;abstract;
  Procedure ArrangeTextureBy(sc,sf:integer;const un,vn:TSEDVector;refx,refy,refz,refu,refv:double);virtual;stdcall;abstract;
  Function IsTextureFlipped(sc,sf:integer):boolean;virtual;stdcall;abstract;
  Procedure RemoveSurfaceReferences(sc,sf:integer);virtual;stdcall;abstract;
  Procedure RemoveSectorReferences(sec:integer;surfs:boolean);virtual;stdcall;abstract;
  Function StitchSurfaces(sc1,sf1,sc2,sf2:integer):boolean;virtual;stdcall;abstract;
  Function FindCommonEdges(sc1,sf1,sc2,sf2:integer; var v11,v12,v21,v22:integer):boolean;virtual;stdcall;abstract;
  Function DoSurfacesOverlap(sc1,sf1,sc2,sf2:integer):boolean;virtual;stdcall;abstract;
  Function MakeAdjoin(sc,sf:integer):boolean;virtual;stdcall;abstract;
  Function MakeAdjoinFromSectorUp(sc,sf:integer;firstsc:integer):boolean;virtual;stdcall;abstract;
  Function UnAdjoin(sc,sf:integer):Boolean;virtual;stdcall;abstract;
  Function CreateCubicSector(x,y,z:double;const pnormal,edge:TSEDVector):integer;virtual;stdcall;abstract;

  Procedure StartUndo(name: PChar); virtual;stdcall;abstract;
  Procedure SaveUndoForThing(n: integer; change: integer); virtual;stdcall;abstract;
  Procedure SaveUndoForLight(n: integer; change: integer); virtual;stdcall;abstract;
  Procedure SaveUndoForSector(n: integer;change: integer; whatpart: integer); virtual;stdcall;abstract;
  Procedure ClearUndoBuffer; virtual;stdcall;abstract;
  Procedure ApplyUndo; virtual;stdcall;abstract;

  {Added in JED 0.92}
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

  Procedure SaveSED(name: PChar); virtual;stdcall;abstract;
  Procedure SaveJKL(name: PChar); virtual;stdcall;abstract;
  Procedure UpdateMap; virtual;stdcall;abstract;
  Procedure SetPickerCMP(cmp: PChar); virtual;stdcall;abstract;
  Function PickResource(what: integer; cur: PChar): PChar; virtual;stdcall;abstract;
  Function EditFlags(what: integer; flags: LongInt): LongInt; virtual;stdcall;abstract;

  Procedure ReloadTemplates; virtual;stdcall;abstract;
  Procedure PanMessage(mtype: integer; msg:PChar); virtual;stdcall;abstract;
  Procedure SendKey(shift: integer; key: integer); virtual;stdcall;abstract;
  Function ExecuteMenu(itemref: PChar): Boolean; virtual;stdcall;abstract;
  Function GetSEDSetting(name: PChar): variant; virtual;stdcall;abstract;
  Function IsLayerVisible(n: integer): boolean; virtual;stdcall;abstract;

  procedure CheckConsistencyErrors; virtual;stdcall;abstract;
  procedure CheckResources; virtual;stdcall;abstract;
  Function NConsistencyErrors: integer;virtual;stdcall;abstract;
  Function GetConsErrorString(n: integer): PChar;virtual;stdcall;abstract;
  Function GetConsErrorType(n: integer):integer;virtual;stdcall;abstract;
  Function GetConsErrorItemType(n: integer):integer;virtual;stdcall;abstract; // SED 0.1.0
  Function GetConsErrorSector(n: integer):integer;virtual;stdcall;abstract;
  Function GetConsErrorSurface(n: integer):integer;virtual;stdcall;abstract;
  Function GetConsErrorThing(n: integer):integer;virtual;stdcall;abstract;
  Function GetConsErrorCog(n: integer):integer;virtual;stdcall;abstract;
  Function IsPreviewActive: Boolean; virtual;stdcall;abstract;
  Function GetSEDString(what: integer): PChar; virtual;stdcall;abstract;
  Function GetProjectType: integer; virtual;stdcall;abstract;         // SED 0.1.0
  Procedure SetProjectType(kind: integer); virtual;stdcall;abstract;  // SED 0.1.0

  {JED 0.93}
  Function GetJEDWindow(whichone: integer):integer;virtual;stdcall;abstract;

  Function FileExtractExt(path: PChar): PChar; virtual;stdcall;abstract;
  Function FileExtractPath(path: PChar): PChar; virtual;stdcall;abstract;
  Function FileExtractName(path: PChar): PChar; virtual;stdcall;abstract;
  Function FindProjectDirFile(name: PChar):PChar; virtual;stdcall;abstract;
  Function IsFileContainer(path: PChar): Boolean; virtual;stdcall;abstract;
  Function IsFileInContainer(path: PChar):Boolean; virtual;stdcall;abstract;

  Function FileOpenDialog(fname: PChar; filter: PChar): PChar; virtual;stdcall;abstract;

  Function OpenFile(name: PChar): integer; virtual;stdcall;abstract;
  Function OpenGameFile(name: PChar): integer; virtual;stdcall;abstract;
  Function ReadFile(handle: integer; buf: pointer; size:longint):integer; virtual;stdcall;abstract;
  Procedure SetFilePos(handle: integer; pos: longint); virtual;stdcall;abstract;
  Function GetFilePos(handle: integer): longint; virtual;stdcall;abstract;
  Function GetFileSize(handle: integer): longint; virtual;stdcall;abstract;
  Function GetFileName(handle: integer): PChar; virtual;stdcall;abstract;
  Procedure CloseFile(handle: integer); virtual;stdcall;abstract;

  Function OpenTextFile(name: PChar): integer; virtual;stdcall;abstract;
  Function OpenGameTextFile(name: PChar): integer; virtual;stdcall;abstract;
  Function GetTextFileName(handle: integer): PChar; virtual;stdcall;abstract;
  Function ReadTextFileString(handle:integer): PChar; virtual;stdcall;abstract;
  Function TextFileEof(handle: integer): Boolean; virtual;stdcall;abstract;
  Function TextFileCurrentLine(handle: integer): integer; virtual;stdcall;abstract;
  Procedure CloseTextFile(handle: integer); virtual;stdcall;abstract;

  Function OpenGOB(name: PChar): integer;virtual;stdcall;abstract;
  Function GOBNFiles(handle: integer): integer;virtual;stdcall;abstract;
  Function GOBNFileName(handle: integer; n: integer): PChar; virtual;stdcall;abstract;
  Function GOBNFullFileName(handle: integer; n: integer): PChar; virtual;stdcall;abstract;
  Function GOBGetFileSize(handle: integer; n: integer): longint; virtual;stdcall;abstract;
  Function GOBGetFileOffset(handle: integer; n: integer): longint; virtual;stdcall;abstract;
  Procedure CloseGOB(handle: integer); virtual;stdcall;abstract;

  Function CreateWFRenderer(wnd:integer;whichone:integer):ISEDWFRenderer;virtual;stdcall;abstract;

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

TSEDPluginLoad=Function(sed:ISED):boolean;
TSEDPluginLoadStdCall=Function(sed:ISED):Boolean;stdcall;


{General purpose functions. Local}

Procedure SetVector(var vec:TSEDVector;dx,dy,dz:double);
Function DotProduct(const vec1,vec2:TSEDVector):double;
Procedure CrossProduct(const vec1,vec2:TSEDVector;var vec:TSEDVector);
Function VectorLen(const vec:TSEDVector):double;
Function NormalizeVector(var vec:TSEDVector):boolean;
Function DoBoxesIntersect(const box1,box2:TSEDBox):boolean;
Procedure SetBox(var box:TSEDBox; x1,x2,y1,y2,z1,z2:double);
Function IsPointInBox(const box:TSEDBox; x,y,z:double):boolean;
Function LinePlaneIntersection(const normal:TSEDVector;pX,pY,pZ,x1,y1,z1,x2,y2,z2:double;var x,y,z:double):boolean;
Function MinDouble(d1,d2:double):double;

implementation

Function MinDouble(d1,d2:double):double;
begin
 if d1<d2 then result:=d1 else result:=d2;
end;

Procedure SetVector(var vec:TSEDVector;dx,dy,dz:double);
begin
 vec.dx:=dx;
 vec.dy:=dy;
 vec.dz:=dz;
end;

Function DotProduct(const vec1,vec2:TSEDVector):double;
begin
 result:=vec1.dx*vec2.dx+vec1.dy*vec2.dy+vec1.dz*vec2.dz;
end;

Procedure CrossProduct(const vec1,vec2:TSEDVector;var vec:TSEDVector);
begin
 vec.dx:=vec1.dy*vec2.dz-vec2.dy*vec1.dz;
 vec.dy:=vec2.dx*vec1.dz-vec1.dx*vec2.dz;
 vec.dz:=vec1.dx*vec2.dy-vec2.dx*vec1.dy;
end;

Function VectorLen(const vec:TSEDVector):double;
begin
 result:=sqrt(sqr(vec.dx)+sqr(vec.dy)+sqr(vec.dz));
end;

Function NormalizeVector(var vec:TSEDVector):boolean;
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

Function DoBoxesIntersect(const box1,box2:TSEDBox):boolean;
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

Procedure SetBox(var box:TSEDBox; x1,x2,y1,y2,z1,z2:double);
begin
 box.x1:=MinDouble(x1,x2);
 box.x2:=MinDouble(x1,x2);
 box.y1:=MinDouble(y1,y2);
 box.y2:=MinDouble(y1,y2);
 box.z1:=MinDouble(z1,z2);
 box.z2:=MinDouble(z1,z2);
end;

Function IsPointInBox(const box:TSEDBox; x,y,z:double):boolean;
begin
 result:=(x-box.x1>=-SEDCloseEnough) and (x-box.x2<=SEDCloseEnough) and
         (y-box.y1>=-SEDCloseEnough) and (y-box.y2<=SEDCloseEnough) and
         (z-box.z1>=-SEDCloseEnough) and (z-box.z2<=SEDCloseEnough);
end;

Function LinePlaneIntersection(const normal:TSEDVector;pX,pY,pZ,x1,y1,z1,x2,y2,z2:double;var x,y,z:double):boolean;
var
    v1,v2:TSEDVector;
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
