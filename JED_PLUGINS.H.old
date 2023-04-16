#include <objbase.h>



#define CloseEnough 10e-5
#define lh_version 1
#define lh_gravity 2
#define lh_skyZ 4
#define lh_cSkyOffs 8
#define lh_HorDist 0x10
#define lh_HorPPR 0x20
#define lh_HSkyOffs 0x40
#define lh_MipMapDist 0x80
#define lh_LODDist 0x100
#define lh_PerspDist 0x200
#define lh_GouraudDist 0x400
#define lh_ppu 0x800
#define lh_MasterCMP 0x1000
#define lh_all 0x1FFF

#define s_flags 1
#define s_ambient 2
#define s_extra 4
#define s_cmp 8
#define s_tint 0x10
#define s_sound 0x20
#define s_sndvol 0x40
#define s_layer 0x80
#define s_all 0xFF


#define sf_adjoin 1
#define sf_adjoinflags 2
#define sf_SurfFlags 4
#define sf_FaceFlags 8
#define sf_Material 0x10
#define sf_geo 0x20
#define sf_light 0x40
#define sf_tex 0x80
#define sf_ExtraLight 0x100
#define sf_txscale 0x200
#define sf_all 0x3FF

#define th_name 1
#define th_sector 2
#define th_position 4
#define th_orientation 8
#define th_layer 0x10
#define th_all 0x1F

#define lt_position 1
#define lt_intensity 2
#define lt_range 4
#define lt_rgb 8
#define lt_rgbintensity 0x10
#define lt_rgbrange 0x20
#define lt_flags 0x40
#define lt_layer 0x80
#define lt_all 0xFF

#define MM_SC 0
#define MM_SF 1
#define MM_VX 2
#define MM_TH 3
#define MM_ED 4
#define MM_LT 5
#define MM_FR 6

#define su_texture 1
#define su_floorflag 2
#define su_sector 4
#define su_all 7


#define uc_changed 0
#define uc_added 1
#define uc_deleted 2
#define sc_values 1
#define sc_geometry 2

#define ct_unknown 0
#define ct_ai 1
#define ct_cog 2
#define ct_key 3
#define ct_mat 4
#define ct_msg 5
#define ct_3do 6
#define ct_sec 7
#define ct_wav 8
#define ct_surface 9
#define ct_template 10
#define ct_thing 11
#define ct_int 12
#define ct_flex 13
#define ct_vector 14

#define pr_thing 1
#define pr_template 2
#define pr_cmp 3
#define pr_secsound 4
#define pr_mat 5
#define pr_cog 6
#define pr_layer 7
#define pr_3do 8
#define pr_ai 9
#define pr_key 10
#define pr_snd 11
#define pr_pup 12
#define pr_spr 13
#define pr_par 14
#define pr_per 15
#define pr_jklsmksan 16

#define ef_sector 1
#define ef_surface 2
#define ef_adjoin 3
#define ef_thing 4
#define ef_face 5
#define ef_light 6
#define ef_geo 7
#define ef_lightmode 8
#define ef_tex 9

#define msg_info 0
#define msg_warning 1
#define msg_error 2

#define sk_Ctrl 1
#define sk_Shift 2
#define sk_Alt 4

#define ce_none 0
#define ce_sector 1
#define ce_surface 2
#define ce_thing 3
#define ce_cog 4

#define js_ProjectDir 1
#define js_JEDDir 2
#define js_CDDir 3
#define js_GameDir 4
#define js_LevelFile 5
#define js_jedregkey 6
#define js_LECLogo 7
#define js_recent1 8
#define js_recent2 9
#define js_recent3 10
#define js_recent4 11
#define js_res1gob 12
#define js_res2gob 13
#define js_spgob 14
#define js_mp1gob 15
#define js_mp2gob 16
#define js_mp3gob 17

#define jw_Main 0
#define jw_ConsChecker 1
#define jw_ItemEdit 2
#define jw_PlacedCogs 3
#define jw_MsgWindow 4
#define jw_3DPreview 5
#define jw_ToolWindow 6

#define rd_CamX 0
#define rd_CamY 1
#define rd_CamZ 2
#define rd_Scale 3
#define rd_GridX 4
#define rd_GridY 5
#define rd_GridZ 6
#define rd_GridLine 7
#define rd_GridDot 8
#define rd_GridStep 9


#define rv_CamPos 10
#define rv_GridPos 11
#define rv_CamXAxis 12
#define rv_CamYAxis 13
#define rv_CamZAxis 14

#define rv_GridXAxis 15
#define rv_GridYAxis 16
#define rv_GridZAxis 17


#define rc_current 0
#define rc_Background 1

#define cc_CULLNONE 0
#define cc_CULLBACK -1
#define cc_CULLFRONT 1

#define cr_Default 0
#define cr_OpenGL 1
#define cr_Software 2



/* Unit header for: JED_T.C -- Made by TPTC - Translate Pascal to C */





typedef struct tjedvector { 
 /* Selector is integer */
union { 
 struct {  double       dx, dy, dz;  } s0;
 struct {  double       x, y, z;  } s1;
 } v;
} tjedvector; 

typedef struct tjedbox { 
double       x1, y1, z1, x2, y2, z2; 
} tjedbox; 

typedef struct tjedsurfacevertex { 
float        u, v; 
float        intensity, r, g, b; 
} tjedsurfacevertex; 

typedef struct tlevelheader { 
long         version; 
double       gravity; 
double       ceilingskyz; 
double       ceilingskyoffs[2]; 
double       hordistance, horpixelsperrev; 
double       horskyoffs[2]; 
double       mipmapdist[4]; 
double       loddist[4]; 
double       perspdist, gourauddist; 
double       pixelperunit; 
char         *mastercmp; 
} tlevelheader; 

typedef struct tjedsectorrec { 
long         flags; 
double       ambient; 
double       extra; 
char         *colormap; 
float        tint_r, tint_g, tint_b; 
char         *sound; 
double       snd_vol; 
long         layer; 
} tjedsectorrec; 

typedef struct tjedsurfacerec { 
long         adjoinsc, adjoinsf; 
long         adjoinflags; 
long         surfflags, faceflags; 
char         *material; 
long         geo, light, tex; 
double       extralight; 
float        uscale, vscale; 
} tjedsurfacerec; 

typedef struct tjedthingrec { 
char         *name; 
long         sector; 
double       x, y, z; 
double       pch, yaw, rol; 
long         layer; 
} tjedthingrec; 


typedef struct tjedlightrec { 
double       x, y, z; 
double       intensity; 
double       range; 
float        r, g, b; 
float        rgbintensity; 
double       rgbrange; 
long         flags; 
long         layer; 
} tjedlightrec; 




DECLARE_INTERFACE_(IJEDWFRenderer,IUnknown)
{
 STDMETHOD_(double,GetRendererDouble)(THIS_ int what) PURE;
 STDMETHOD(SetRendererDouble)(THIS_ int what, double val) PURE;
 STDMETHOD(GetRendererVector)(THIS_ int what, double* x, double* , double* ) PURE;
 STDMETHOD(SetRendererVector)(THIS_ int what, double x, double , double ) PURE;

 STDMETHOD_(int,NSelected)(THIS) PURE;
 STDMETHOD_(int,GetNSelected)(THIS_ int n) PURE;
 STDMETHOD(SetViewPort)(THIS_ int x, int , int , int ) PURE;
 STDMETHOD(SetColor)(THIS_ byte what, byte , byte , byte ) PURE;
 STDMETHOD(SetPointSize)(THIS_ double size) PURE;
 STDMETHOD(BeginScene)(THIS) PURE;
 STDMETHOD(EndScene)(THIS) PURE;
 STDMETHOD(SetCulling)(THIS_ int how) PURE;

 STDMETHOD(DrawSector)(THIS_ int sc) PURE;
 STDMETHOD(DrawSurface)(THIS_ int sc, int ) PURE;
 STDMETHOD(DrawThing)(THIS_ int th) PURE;

 STDMETHOD(DrawLine)(THIS_ double x1, double , double , double , double , double ) PURE;
 STDMETHOD(DrawVertex)(THIS_ double x, double , double ) PURE;
 STDMETHOD(DrawGrid)(THIS) PURE;

 STDMETHOD(BeginPick)(THIS_ int x, int ) PURE;
 STDMETHOD(EndPick)(THIS) PURE;

 STDMETHOD(PickSector)(THIS_ int sc, int id) PURE;
 STDMETHOD(PickSurface)(THIS_ int sc, int , int id) PURE;
 STDMETHOD(PickLine)(THIS_ double x1, double , double , double , double , double , int id) PURE;
 STDMETHOD(PickVertex)(THIS_ double x, double , double , int id) PURE;

 STDMETHOD(BeginRectPick)(THIS_ int x1, int , int , int ) PURE;
 STDMETHOD(EndRectPick)(THIS) PURE;
 STDMETHOD_(bool,IsSectorInRect)(THIS_ int sc) PURE;
 STDMETHOD_(bool,IsSurfaceInRect)(THIS_ int sc, int ) PURE;
 STDMETHOD_(bool,IsLineInRect)(THIS_ double x1, double , double , double , double , double ) PURE;
 STDMETHOD_(bool,IsVertexInRect)(THIS_ double x, double , double ) PURE;

 STDMETHOD_(bool,GetXYZonPlaneAt)(THIS_ int scx, int , tjedvector pnormal, double px, double , double , double* x, double* , double* ) PURE;
 STDMETHOD_(bool,GetGridAt)(THIS_ int scx, int , double* x, double* , double* ) PURE;
 STDMETHOD(GetNearestGridNode)(THIS_ double ix, double , double , double* x, double* , double* ) PURE;
 STDMETHOD(ProjectPoint)(THIS_ double x, double , double , int* winx, int* ) PURE;
 STDMETHOD(UnProjectPoint)(THIS_ int winx, int , double winz, double* x, double* , double* ) PURE;
 STDMETHOD_(bool,IsSurfaceFacing)(THIS_ int sc, int ) PURE;

 STDMETHOD_(int,HandleWMQueryPal)(THIS) PURE;
 STDMETHOD_(int,HandleWMChangePal)(THIS) PURE;

};




DECLARE_INTERFACE_(IJEDLevel,IUnknown)
{

 STDMETHOD(GetLevelHeader)(THIS_ tlevelheader* lh, int flags) PURE;
 STDMETHOD(SetLevelHeader)(THIS_ tlevelheader* lh, int flags) PURE;

 STDMETHOD_(int,NSectors)(THIS) PURE;
 STDMETHOD_(int,NThings)(THIS) PURE;
 STDMETHOD_(int,NLights)(THIS) PURE;
 STDMETHOD_(int,NCOgs)(THIS) PURE;

 STDMETHOD_(int,AddSector)(THIS) PURE;
 STDMETHOD(DeleteSector)(THIS_ int n) PURE;

 STDMETHOD(GetSector)(THIS_ int sec, tjedsectorrec* rec, int flags) PURE;
 STDMETHOD(SetSector)(THIS_ int sec, tjedsectorrec* rec, int flags) PURE;

 STDMETHOD_(int,SectorNVertices)(THIS_ int sec) PURE;
 STDMETHOD_(int,SectorNSurfaces)(THIS_ int sec) PURE;

 STDMETHOD(SectorGetVertex)(THIS_ int sec, int , double* x, double* , double* ) PURE;
 STDMETHOD(SectorSetVertex)(THIS_ int sec, int , double x, double , double ) PURE;

 STDMETHOD_(int,SectorAddVertex)(THIS_ int sec, double x, double , double ) PURE;
 STDMETHOD_(int,SectorFindVertex)(THIS_ int sec, double x, double , double ) PURE;
 STDMETHOD_(int,SectorDeleteVertex)(THIS_ int sec, int n) PURE;

 STDMETHOD_(int,SectorAddSurface)(THIS_ int sec) PURE;
 STDMETHOD(SectorDeleteSurface)(THIS_ int sc, int ) PURE;
 STDMETHOD(SectorUpdate)(THIS_ int sec) PURE;

 STDMETHOD(GetSurface)(THIS_ int sc, int , tjedsurfacerec* rec, int flags) PURE;
 STDMETHOD(SetSurface)(THIS_ int sc, int , tjedsurfacerec* rec, int flags) PURE;
 STDMETHOD(GetSurfaceNormal)(THIS_ int sc, int , tjedvector* n) PURE;
 STDMETHOD(SurfaceUpdate)(THIS_ int sc, int , int how) PURE;
 STDMETHOD_(int,SurfaceNVertices)(THIS_ int sc, int ) PURE;
 STDMETHOD_(int,SurfaceGetVertexNum)(THIS_ int sc, int , int ) PURE;
 STDMETHOD(SurfaceSetVertexNum)(THIS_ int sc, int , int , int secvx) PURE;
 STDMETHOD_(int,SurfaceAddVertex)(THIS_ int sc, int , int secvx) PURE;
 STDMETHOD_(int,SurfaceInsertVertex)(THIS_ int sc, int , int at, int secvx) PURE;
 STDMETHOD(SurfaceDeleteVertex)(THIS_ int sc, int , int n) PURE;
 STDMETHOD(SurfaceGetVertexUV)(THIS_ int sc, int , int , float* u, float* ) PURE;
 STDMETHOD(SurfaceSetVertexUV)(THIS_ int sc, int , int , float u, float ) PURE;
 STDMETHOD(SurfaceGetVertexLight)(THIS_ int sc, int , int , float* white, float* , float* , float* ) PURE;
 STDMETHOD(SurfaceSetVertexLight)(THIS_ int sc, int , int , float white, float , float , float ) PURE;

 STDMETHOD_(int,AddThing)(THIS) PURE;
 STDMETHOD(DeleteThing)(THIS_ int th) PURE;
 STDMETHOD(GetThing)(THIS_ int th, tjedthingrec* rec, int flags) PURE;
 STDMETHOD(SetThing)(THIS_ int th, tjedthingrec* rec, int flags) PURE;
 STDMETHOD(ThingUpdate)(THIS_ int th) PURE;

 STDMETHOD_(int,AddLight)(THIS) PURE;
 STDMETHOD(DeleteLight)(THIS_ int lt) PURE;
 STDMETHOD(GetLight)(THIS_ int lt, tjedlightrec* rec, int flags) PURE;
 STDMETHOD(SetLight)(THIS_ int lt, tjedlightrec* rec, int flags) PURE;

 STDMETHOD_(int,NLayers)(THIS) PURE;
 STDMETHOD_(char*,GetLayerName)(THIS_ int n) PURE;
 STDMETHOD_(int,AddLayer)(THIS_ char* name) PURE;


 STDMETHOD_(int,NTHingValues)(THIS_ int th) PURE;
 STDMETHOD_(char*,GetThingValueName)(THIS_ int th, int ) PURE;
 STDMETHOD_(char*,GetThingValueData)(THIS_ int th, int ) PURE;
 STDMETHOD(SetThingValueData)(THIS_ int th, int , char* val) PURE;

 STDMETHOD(GetThingFrame)(THIS_ int th, int , double* x, double* , double* , double* , double* , double* ) PURE;
 STDMETHOD(SetThingFrame)(THIS_ int th, int , double x, double , double , double , double , double ) PURE;

 STDMETHOD_(int,AddThingValue)(THIS_ int th, char* name, char* ) PURE;
 STDMETHOD(InsertThingValue)(THIS_ int th, int , char* name, char* ) PURE;
 STDMETHOD(DeleteThingValue)(THIS_ int th, int ) PURE;

 STDMETHOD_(int,AddCOG)(THIS_ char* name) PURE;
 STDMETHOD(DeleteCOG)(THIS_ int n) PURE;
 STDMETHOD(UpdateCOG)(THIS_ int cg) PURE;

 STDMETHOD_(char*,GetCOGFileName)(THIS_ int cg) PURE;
 STDMETHOD_(int,NCOGValues)(THIS_ int cg) PURE;

 STDMETHOD_(char*,GetCOGValueName)(THIS_ int cg, int ) PURE;
 STDMETHOD_(int,GetCOGValueType)(THIS_ int cg, int ) PURE;

 STDMETHOD_(char*,GetCOGValue)(THIS_ int cg, int ) PURE;
 STDMETHOD_(bool,SetCOGValue)(THIS_ int cg, int , char* val) PURE;

 STDMETHOD_(int,AddCOGValue)(THIS_ int cg, char* name, char* , int vtype) PURE;
 STDMETHOD(InsertCOGValue)(THIS_ int cg, int , char* name, char* , int vtype) PURE;
 STDMETHOD(DeleteCOGValue)(THIS_ int cg, int ) PURE;

};

DECLARE_INTERFACE_(IJED,IUnknown)
{

 STDMETHOD_(double,GetVersion)(THIS) PURE;
 STDMETHOD_(IJEDLevel *,GetLevel)(THIS) PURE;

 STDMETHOD_(int,GetMapMode)(THIS) PURE;
 STDMETHOD(SetMapMode)(THIS_ int mode) PURE;
 STDMETHOD_(int,GetCurSC)(THIS) PURE;
 STDMETHOD(SetCurSC)(THIS_ int sc) PURE;
 STDMETHOD_(int,GetCurTH)(THIS) PURE;
 STDMETHOD(SetCurTH)(THIS_ int th) PURE;
 STDMETHOD_(int,GetCurLT)(THIS) PURE;
 STDMETHOD(SetCurLT)(THIS_ int lt) PURE;
 STDMETHOD(GetCurVX)(THIS_ int* sc, int* ) PURE;
 STDMETHOD(SetCurVX)(THIS_ int sc, int ) PURE;
 STDMETHOD(GetCurSF)(THIS_ int* sc, int* ) PURE;
 STDMETHOD(SetCurSF)(THIS_ int sc, int ) PURE;
 STDMETHOD(GetCurED)(THIS_ int* sc, int* , int* ) PURE;
 STDMETHOD(SetCurED)(THIS_ int sc, int , int ) PURE;
 STDMETHOD(GetCurFR)(THIS_ int* th, int* ) PURE;
 STDMETHOD(SetCurFR)(THIS_ int th, int ) PURE;

 STDMETHOD(NewLevel)(THIS_ bool mots) PURE;
 STDMETHOD(LoadLevel)(THIS_ char* name) PURE;


 STDMETHOD(FindBBox)(THIS_ int sec, tjedbox* box) PURE;
 STDMETHOD(FindBoundingSphere)(THIS_ int sec, double* cx, double* , double* , double* ) PURE;
 STDMETHOD_(bool,FindCollideBox)(THIS_ int sec, tjedbox* bbox, double cx, double , double , tjedbox* cbox) PURE;
 STDMETHOD(FindSurfaceCenter)(THIS_ int sc, int , double* cx, double* , double* ) PURE;
 STDMETHOD(RotateVector)(THIS_ tjedvector* vec, double pch, double , double ) PURE;
 STDMETHOD(RotatePoint)(THIS_ double ax1, double , double , double , double , double , double angle, double* x, double* , double* ) PURE;
 STDMETHOD(GetJKPYR)(THIS_ tjedvector* x, tjedvector* , tjedvector* , double* pch, double* , double* ) PURE;
 STDMETHOD_(bool,IsSurfaceConvex)(THIS_ int sc, int ) PURE;
 STDMETHOD_(bool,IsSurfacePlanar)(THIS_ int sc, int ) PURE;
 STDMETHOD_(bool,IsSectorConvex)(THIS_ int sec) PURE;
 STDMETHOD_(bool,IsInSector)(THIS_ int sec, double x, double , double ) PURE;
 STDMETHOD_(bool,DoSectorsOverlap)(THIS_ int sec1, int ) PURE;
 STDMETHOD_(bool,IsPointOnSurface)(THIS_ int sc, int , double x, double , double ) PURE;
 STDMETHOD_(int,FindSectorForThing)(THIS_ int th) PURE;
 STDMETHOD_(int,FindSectorForXYZ)(THIS_ double x, double , double ) PURE;
 STDMETHOD_(int,ExtrudeSurface)(THIS_ int sc, int , double by) PURE;
 STDMETHOD_(int,CleaveSurface)(THIS_ int sc, int , tjedvector* cnormal, double cx, double , double ) PURE;
 STDMETHOD_(int,CleaveSector)(THIS_ int sec, tjedvector* cnormal, double cx, double , double ) PURE;
 STDMETHOD_(bool,CleaveEdge)(THIS_ int sc, int , int , tjedvector* cnormal, double cx, double , double ) PURE;
 STDMETHOD_(bool,JoinSurfaces)(THIS_ int sc1, int , int , int ) PURE;
 STDMETHOD_(bool,PlanarizeSurface)(THIS_ int sc, int ) PURE;
 STDMETHOD_(int,MergeSurfaces)(THIS_ int sc, int , int ) PURE;
 STDMETHOD_(int,MergeSectors)(THIS_ int sec1, int ) PURE;
 STDMETHOD(CalculateDefaultUVNormals)(THIS_ int sc, int , int orgvx, tjedvector* un, tjedvector* ) PURE;
 STDMETHOD(CalcUVNormals)(THIS_ int sc, int , tjedvector* un, tjedvector* ) PURE;
 STDMETHOD(ArrangeTexture)(THIS_ int sc, int , int orgvx, tjedvector* un, tjedvector* ) PURE;
 STDMETHOD(ArrangeTextureBy)(THIS_ int sc, int , tjedvector* un, tjedvector* , double refx, double , double , double , double ) PURE;
 STDMETHOD_(bool,IsTextureFlipped)(THIS_ int sc, int ) PURE;
 STDMETHOD(RemoveSurfaceReferences)(THIS_ int sc, int ) PURE;
 STDMETHOD(RemoveSectorReferences)(THIS_ int sec, bool surfs) PURE;
 STDMETHOD_(bool,StitchSurfaces)(THIS_ int sc1, int , int , int ) PURE;
 STDMETHOD_(bool,FindCommonEdges)(THIS_ int sc1, int , int , int , int* v11, int* , int* , int* ) PURE;
 STDMETHOD_(bool,DoSurfacesOverlap)(THIS_ int sc1, int , int , int ) PURE;
 STDMETHOD_(bool,MakeAdjoin)(THIS_ int sc, int ) PURE;
 STDMETHOD_(bool,MakeAdjoinFromSectorUp)(THIS_ int sc, int , int firstsc) PURE;
 STDMETHOD_(bool,UnAdjoin)(THIS_ int sc, int ) PURE;
 STDMETHOD_(int,CreateCubicSector)(THIS_ double x, double , double , tjedvector* pnormal, tjedvector* ) PURE;

 STDMETHOD(StartUndo)(THIS_ char* name) PURE;
 STDMETHOD(SaveUndoForThing)(THIS_ int n, int change) PURE;
 STDMETHOD(SaveUndoForLight)(THIS_ int n, int change) PURE;
 STDMETHOD(SaveUndoForSector)(THIS_ int n, int change, int whatpart) PURE;
 STDMETHOD(ClearUndoBuffer)(THIS) PURE;
 STDMETHOD(ApplyUndo)(THIS) PURE;

 STDMETHOD_(int,GetApplicationHandle)(THIS) PURE;
 STDMETHOD_(bool,JoinSectors)(THIS_ int sec1, int ) PURE;

 STDMETHOD_(int,GetNMultiselected)(THIS_ int what) PURE;
 STDMETHOD(ClearMultiselection)(THIS_ int what) PURE;
 STDMETHOD(RemoveFromMultiselection)(THIS_ int what, int ) PURE;
 STDMETHOD_(int,GetSelectedSC)(THIS_ int n) PURE;
 STDMETHOD_(int,GetSelectedTH)(THIS_ int n) PURE;
 STDMETHOD_(int,GetSelectedLT)(THIS_ int n) PURE;

 STDMETHOD(GetSelectedSF)(THIS_ int n, int* sc, int* ) PURE;
 STDMETHOD(GetSelectedED)(THIS_ int n, int* sc, int* , int* ) PURE;
 STDMETHOD(GetSelectedVX)(THIS_ int n, int* sc, int* ) PURE;
 STDMETHOD(GetSelectedFR)(THIS_ int n, int* th, int* ) PURE;

 STDMETHOD_(int,SelectSC)(THIS_ int sc) PURE;
 STDMETHOD_(int,SelectSF)(THIS_ int sc, int ) PURE;
 STDMETHOD_(int,SelectED)(THIS_ int sc, int , int ) PURE;
 STDMETHOD_(int,SelectVX)(THIS_ int sc, int ) PURE;
 STDMETHOD_(int,SelectTH)(THIS_ int th) PURE;
 STDMETHOD_(int,SelectFR)(THIS_ int th, int ) PURE;
 STDMETHOD_(int,SelectLT)(THIS_ int lt) PURE;

 STDMETHOD_(int,FindSelectedSC)(THIS_ int sc) PURE;
 STDMETHOD_(int,FindSelectedSF)(THIS_ int sc, int ) PURE;
 STDMETHOD_(int,FindSelectedED)(THIS_ int sc, int , int ) PURE;
 STDMETHOD_(int,FindSelectedVX)(THIS_ int sc, int ) PURE;
 STDMETHOD_(int,FindSelectedTH)(THIS_ int th) PURE;
 STDMETHOD_(int,FindSelectedFR)(THIS_ int th, int ) PURE;
 STDMETHOD_(int,FindSelectedLT)(THIS_ int lt) PURE;


 STDMETHOD(SaveJED)(THIS_ char* name) PURE;
 STDMETHOD(SaveJKL)(THIS_ char* name) PURE;
 STDMETHOD(UpdateMap)(THIS) PURE;
 STDMETHOD(SetPickerCMP)(THIS_ char* cmp) PURE;
 STDMETHOD_(char*,PickResource)(THIS_ int what, char* cur) PURE;
 STDMETHOD_(long,EditFlags)(THIS_ int what, long flags) PURE;

 STDMETHOD(ReloadTemplates)(THIS) PURE;
 STDMETHOD(PanMessage)(THIS_ int mtype, char* msg) PURE;
 STDMETHOD(SendKey)(THIS_ int shift, int key) PURE;
 STDMETHOD_(bool,ExecuteMenu)(THIS_ char* itemref) PURE;
 STDMETHOD_(void *,GetJEDSetting)(THIS_ char* name) PURE;
 STDMETHOD_(bool,IsLayerVisible)(THIS_ int n) PURE;

 STDMETHOD(CheckConsistencyErrors)(THIS) PURE;
 STDMETHOD(CheckResources)(THIS) PURE;
 STDMETHOD_(int,NConsistencyErrors)(THIS) PURE;
 STDMETHOD_(char*,GetConsErrorString)(THIS_ int n) PURE;
 STDMETHOD_(int,GetConsErrorType)(THIS_ int n) PURE;
 STDMETHOD_(int,GetConsErrorSector)(THIS_ int n) PURE;
 STDMETHOD_(int,GetConsErrorSurface)(THIS_ int n) PURE;
 STDMETHOD_(int,GetConsErrorThing)(THIS_ int n) PURE;
 STDMETHOD_(int,GetConsErrorCog)(THIS_ int n) PURE;
 STDMETHOD_(bool,IsPreviewActive)(THIS) PURE;
 STDMETHOD_(char*,GetJEDString)(THIS_ int what) PURE;
 STDMETHOD_(bool,GetIsMOTS)(THIS) PURE;
 STDMETHOD(SetIsMOTS)(THIS_ bool mots) PURE;

 STDMETHOD_(int,GetJedWindow)(THIS_ int whichone) PURE;

 STDMETHOD_(char*,FileExtractExt)(THIS_ char* path) PURE;
 STDMETHOD_(char*,FileExtractPath)(THIS_ char* path) PURE;
 STDMETHOD_(char*,FileExtractName)(THIS_ char* path) PURE;
 STDMETHOD_(char*,FindProjectDirFile)(THIS_ char* name) PURE;
 STDMETHOD_(bool,IsFileContainer)(THIS_ char* path) PURE;
 STDMETHOD_(bool,IsFileInContainer)(THIS_ char* path) PURE;

 STDMETHOD_(char*,FileOpenDialog)(THIS_ char* fname, char* filter) PURE;

 STDMETHOD_(int,OpenFile)(THIS_ char* name) PURE;
 STDMETHOD_(int,OpenGameFile)(THIS_ char* name) PURE;
 STDMETHOD_(int,ReadFile)(THIS_ int handle, void* buf, long size) PURE;
 STDMETHOD(SetFilePos)(THIS_ int handle, long pos) PURE;
 STDMETHOD_(long,GetFilePos)(THIS_ int handle) PURE;
 STDMETHOD_(long,GetFileSize)(THIS_ int handle) PURE;
 STDMETHOD_(char*,GetFileName)(THIS_ int handle) PURE;
 STDMETHOD(CloseFile)(THIS_ int handle) PURE;

 STDMETHOD_(int,OpenTextFile)(THIS_ char* name) PURE;
 STDMETHOD_(int,OpenGameTextFile)(THIS_ char* name) PURE;
 STDMETHOD_(char*,GetTextFileName)(THIS_ int handle) PURE;
 STDMETHOD_(char*,ReadTextFileString)(THIS_ int handle) PURE;
 STDMETHOD_(bool,TextFileEof)(THIS_ int handle) PURE;
 STDMETHOD_(int,TextFileCurrentLine)(THIS_ int handle) PURE;
 STDMETHOD(CloseTextFile)(THIS_ int handle) PURE;

 STDMETHOD_(int,OpenGOB)(THIS_ char* name) PURE;
 STDMETHOD_(int,GOBNFiles)(THIS_ int handle) PURE;
 STDMETHOD_(char*,GOBNFileName)(THIS_ int handle, int n) PURE;
 STDMETHOD_(char*,GOBNFullFileName)(THIS_ int handle, int n) PURE;
 STDMETHOD_(long,GOBGetFileSize)(THIS_ int handle, int n) PURE;
 STDMETHOD_(long,GOBGetFileOffset)(THIS_ int handle, int n) PURE;
 STDMETHOD(CloseGOB)(THIS_ int handle) PURE;

 STDMETHOD_(IJEDWFRenderer *,CreateWFRenderer)(THIS_ int wnd, int whichone) PURE;

};


