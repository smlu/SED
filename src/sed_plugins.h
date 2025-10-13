#ifndef SED_PLUGINS_H
#define SED_PLUGINS_H
#include <stdint.h>
#include <stdbool.h>
#include <objbase.h>

/* Constants */
#define sedCloseEnough 10e-5

/* Get/SetProjectType, NewLevel */
#define spt_jkdf2 0
#define spt_mots  1
#define spt_ijim  2

/* Get/SetLevelHeader flags */
#define lh_version     1
#define lh_gravity     2
#define lh_skyZ        4
#define lh_cskyOffs    8
#define lh_horDist     0x10
#define lh_horPPR      0x20
#define lh_hskyOffs    0x40
#define lh_mipmapDist  0x80
#define lh_lodDist     0x100
#define lh_perspDist   0x200
#define lh_gouraudDist 0x400
#define lh_ppu         0x800
#define lh_masterCmp   0x1000
#define lh_fog         0x2000
#define lh_all         0x2FFF

/* Get/SetSector flags */
#define s_flags      1
#define s_ambient    2
#define s_extra      4
#define s_pointlight 8
#define s_cmp        0x10
#define s_tint       0x20
#define s_sound      0x40
#define s_sndvol     0x80
#define s_thrust     0x100
#define s_layer      0x200
#define s_all        0xFFFF

/* Get/SetSurface flags */
#define sf_adjoin      1
#define sf_adjoinflags 2
#define sf_surfflags   4
#define sf_faceflags   8
#define sf_material    0x10
#define sf_geo         0x20
#define sf_light       0x40
#define sf_tex         0x80
#define sf_extralight  0x100
#define sf_txscale     0x200
#define sf_all         0x3FF

/* Get/SetThing flags */
#define th_name        1
#define th_sector      2
#define th_position    4
#define th_orientation 8
#define th_layer       0x10
#define th_all         0x1F

/* Get/SetLight flags */
#define lt_position     1
#define lt_intensity    2
#define lt_range        4
#define lt_color        8
#define lt_flags        0x10
#define lt_layer        0x20
#define lt_all          0xFF

/* Map Mode constants */
#define mm_sc 0 /* Sectors */
#define mm_sf 1 /* surfaces */
#define mm_vx 2 /* vertices */
#define mm_th 3 /* Things */
#define mm_ed 4 /* Edges */
#define mm_lt 5 /* Lights */
#define mm_fr 6 /* frames */

/* SurfaceUpdate "how" constant */
#define su_texture   1
#define su_floorflag 2
#define su_sector    4
#define su_all       0xFF

/* Undo change constants */
#define uc_changed  0
#define uc_added    1
#define uc_deleted  2
#define sc_values   1
#define sc_geometry 2
#define sc_all      3

/* COG value types */
#define ct_unknown  0
#define ct_ai       1
#define ct_cog      2
#define ct_key      3
#define ct_mat      4
#define ct_msg      5
#define ct_3do      6
#define ct_sec      7
#define ct_wav      8
#define ct_surface  9
#define ct_template 10
#define ct_thing    11
#define ct_int      12
#define ct_flex     13
#define ct_vector   14

/* PickResource constants */
#define pr_thing     1
#define pr_template  2
#define pr_cmp       3
#define pr_secsound  4
#define pr_mat       5
#define pr_cog       6
#define pr_layer     7
#define pr_3do       8
#define pr_ai        9
#define pr_key       10
#define pr_snd       11
#define pr_pup       12
#define pr_spr       13
#define pr_par       14
#define pr_per       15
#define pr_jklsmksan 16

/* EditFlags constants */
#define ef_sector    1
#define ef_surface   2
#define ef_adjoin    3
#define ef_thing     4
#define ef_face      5
#define ef_light     6
#define ef_geomode   7
#define ef_lightmode 8
#define ef_texmode   9

/* PanMessage constants */
#define msg_info    0
#define msg_warning 1
#define msg_error   2

/* SendKey flags */
#define sk_ctrl  1
#define sk_shift 2
#define sk_alt   4

/* GetConsErrorType constants */
#define ce_normal  0
#define ce_warning 1
#define ce_error   2

/* GetConsErrorItemType constants */
#define cei_none    0
#define cei_sector  1
#define cei_surface 2
#define cei_thing   3
#define cei_cog     4

/* GetSedString constants */
#define ss_projectDir  1
#define ss_sedDir      2
#define ss_gameDir     3
#define ss_levelFile   4
#define ss_configFile  5 /* ex js_jedregkey */
#define ss_lecLogoJK   6
#define ss_recent1     7
#define ss_recent2     8
#define ss_recent3     9
#define ss_recent4     10
#define ss_res1Gob     11
#define ss_res2Gob     12
#define ss_spGob       13
#define ss_mp1Gob      14
#define ss_mp2Gob      15
#define ss_mp3Gob      16
#define ss_lecLogoIJIM 17

/* GetJEDWindow constants */
#define sw_Main        0
#define sw_ConsChecker 1
#define sw_ItemEdit    2
#define sw_PlacedCogs  3
#define sw_MsgWindow   4
#define sw_3DPreview   5
#define sw_ToolWindow  6

/* Get/SetRendererDoube constants */
#define rd_CamX     0
#define rd_CamY     1
#define rd_CamZ     2
#define rd_Scale    3
#define rd_GridX    4
#define rd_GridY    5
#define rd_GridZ    6
#define rd_GridLine 7
#define rd_GridDot  8
#define rd_GridStep 9

/* Get/SetRendererVector constants */
#define rv_CamPos    10
#define rv_GridPos   11
#define rv_CamXAxis  12
#define rv_CamYAxis  13
#define rv_CamZAxis  14
#define rv_GridXAxis 15
#define rv_GridYAxis 16
#define rv_GridZAxis 17

/* SetRenderColor constants */
#define rc_Current    0
#define rc_Background 1

/* SetCulling constants */
#define cc_CullNone  0
#define cc_CullBack  -1
#define cc_CullFront 1

/* CreateWFRenderer constants */
#define cr_Default  0
#define cr_OpenGL   1
#define cr_Software 2

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(push, 8)

typedef struct
{
    double x;
    double y;
} TSedVector2;

typedef union 
{
    struct { double dx, dy, dz; };
    struct { double x, y, z; };
    struct { double pitch, yaw, roll; };
} TSedVector3;

typedef struct 
{
    float r, g, b, a;  // a - alpha or light intensity
} TSedColor;

typedef struct
{
    TSedVector3 p1;
    TSedVector3 p2;
} TSedBox;

typedef struct 
{
    TSedColor color;
    TSedVector3 position;
    double minRange;  // minimum light range at which light will still illuminate objects
    double maxRange;  // maximum light intensity range before it starts to falloff
} TSedPointLight;

typedef struct 
{
    bool enabled;
    TSedColor color;
    double fogStart;
    double fogEnd;
} TSedFog;

typedef struct
{
    int32_t version;
    double gravity;
    double ceilingSkyHeight;
    TSedVector2 ceilingSkyOffset;
    double horizonSkyDistance;
    double horizonSkyPixelsPerRev;
    TSedVector2 horizonSkyOffset;
    double mipMapDistances[4];
    double lodDistances[4];
    double perspectiveDistance;
    double gouraudDistance;
    double ppunit; // Pixels per unit
    const wchar_t* aMasterCmp;
    TSedFog fog;
} TSedLevelHeader;

typedef struct
{
    int32_t flags;
    TSedColor ambient;
    TSedColor extraLight;
    TSedPointLight pointLight;
    const wchar_t* aColorMap;
    TSedColor tint;
    const wchar_t* aSound;
    double soundVolume;
    TSedVector3 thrust;
    int32_t layer;
} TSedSectorRec;

typedef struct 
{
    int32_t adjoinSec;
    int32_t adjoinSurf;
    int32_t adjoinflags;
    int32_t surfflags;
    int32_t faceflags;
    const wchar_t* aMaterial;
    int32_t geo;
    int32_t light;
    int32_t tex;
    TSedColor extraLight;
    float uscale;
    float vscale;
} TSedSurfaceRec;

typedef struct {
    const wchar_t* aName;
    int32_t sector;
    TSedVector3 position;
    TSedVector3 rotation;
    int32_t layer;
} TSedThingRec;

typedef struct 
{
    int32_t flags;
    int32_t layer;
    TSedColor color;
    double intensity;
    double range;
    TSedVector3 position;
} TSedLightRec;

#pragma pack(pop)

typedef interface ISed ISed;
typedef interface ISedLevel ISedLevel;
typedef interface ISedWFRenderer ISedWFRenderer;

/* Wireframe renderer interface */
/* JED 0.93 */
#undef INTERFACE
#define INTERFACE ISedWFRenderer
DECLARE_INTERFACE_(ISedWFRenderer, IUnknown)
{
    BEGIN_INTERFACE

    /* IUnknown methods */
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, void **ppvObject) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;

    /* Renderer attributes */
    STDMETHOD_(double, GetRendererDouble)(THIS_ int what) PURE;
    STDMETHOD_(void, SetRendererDouble)(THIS_ int what, double val) PURE;
    STDMETHOD_(void, GetRendererVector)(THIS_ int what, double* x, double* y, double* z) PURE;
    STDMETHOD_(void, SetRendererVector)(THIS_ int what, double x, double y, double z) PURE;
    STDMETHOD_(int, NSelected)(THIS) PURE;
    STDMETHOD_(int, GetNSelected)(THIS_ int n) PURE;
    STDMETHOD_(void, SetViewPort)(THIS_ int x, int y, int w, int h) PURE;
    STDMETHOD_(void, SetColor)(THIS_ int what, unsigned char r, unsigned char g, unsigned char b) PURE;
    STDMETHOD_(void, SetPointSize)(THIS_ double size) PURE;
    STDMETHOD_(void, BeginScene)(THIS) PURE;
    STDMETHOD_(void, EndScene)(THIS) PURE;
    STDMETHOD_(void, SetCulling)(THIS_ int how) PURE;
    STDMETHOD_(void, DrawSector)(THIS_ int sc) PURE;
    STDMETHOD_(void, DrawSurface)(THIS_ int sc, int sf) PURE;
    STDMETHOD_(void, DrawThing)(THIS_ int th) PURE;
    STDMETHOD_(void, DrawLine)(THIS_ double x1, double y1, double z1, double x2, double y2, double z2) PURE;
    STDMETHOD_(void, DrawVertex)(THIS_ double X, double Y, double Z) PURE;
    STDMETHOD_(void, DrawGrid)(THIS) PURE;
    STDMETHOD_(void, BeginPick)(THIS_ int x, int y) PURE;
    STDMETHOD_(void, EndPick)(THIS) PURE;
    STDMETHOD_(void, PickSector)(THIS_ int sc, int id) PURE;
    STDMETHOD_(void, PickSurface)(THIS_ int sc, int sf, int id) PURE;
    STDMETHOD_(void, PickLine)(THIS_ double x1, double y1, double z1, double x2, double y2, double z2, int id) PURE;
    STDMETHOD_(void, PickVertex)(THIS_ double X, double Y, double Z, int id) PURE;
    STDMETHOD_(void, BeginRectPick)(THIS_ int x1, int y1, int x2, int y2) PURE;
    STDMETHOD_(void, EndRectPick)(THIS) PURE;
    STDMETHOD_(int, IsSectorInRect)(THIS_ int sc) PURE;
    STDMETHOD_(int, IsSurfaceInRect)(THIS_ int sc, int sf) PURE;
    STDMETHOD_(int, IsLineInRect)(THIS_ double x1, double y1, double z1, double x2, double y2, double z2) PURE;
    STDMETHOD_(int, IsVertexInRect)(THIS_ double X, double Y, double Z) PURE;
    STDMETHOD_(int, GetXYZonPlaneAt)(THIS_ int scX, int scY, const double* pnormal, double pX, double pY, double pZ, double* X, double* Y, double* Z) PURE;
    STDMETHOD_(int, GetGridAt)(THIS_ int scX, int scY, double* X, double* Y, double* Z) PURE;
    STDMETHOD_(void, GetNearestGridNode)(THIS_ double iX, double iY, double iZ, double* X, double* Y, double* Z) PURE;
    STDMETHOD_(void, ProjectPoint)(THIS_ double x, double y, double z, int* WinX, int* WinY) PURE;
    STDMETHOD_(void, UnProjectPoint)(THIS_ int WinX, int WinY, double WinZ, double* x, double* y, double* z) PURE;
    STDMETHOD_(int, IsSurfaceFacing)(THIS_ int sc, int sf) PURE;
    STDMETHOD_(int, HandleWMQueryPal)(THIS) PURE;
    STDMETHOD_(int, HandleWMChangePal)(THIS) PURE;

    END_INTERFACE
};

#undef INTERFACE
#define INTERFACE ISedLevel
DECLARE_INTERFACE_(ISedLevel, IUnknown)
{
    BEGIN_INTERFACE

    /* IUnknown methods */
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, void **ppvObject) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;

    /* Level header */
    STDMETHOD_(void, GetLevelHeader)(THIS_ TSedLevelHeader* lh, int flags) PURE;
    STDMETHOD_(void, SetLevelHeader)(THIS_ const TSedLevelHeader* lh, int flags) PURE;
    STDMETHOD_(int, NumSectors)(THIS) PURE;
    STDMETHOD_(int, NumThings)(THIS) PURE;
    STDMETHOD_(int, NumLights)(THIS) PURE;
    STDMETHOD_(int, NumCogs)(THIS) PURE;

    /* Sectors */
    STDMETHOD_(int, SectorAdd)(THIS) PURE;
    STDMETHOD_(void, SectorDelete)(THIS_ int n) PURE;
    STDMETHOD_(void, SectorGet)(THIS_ int sec, TSedSectorRec* rec, int flags) PURE;
    STDMETHOD_(void, SectorSet)(THIS_ int sec, const TSedSectorRec* rec, int flags) PURE;
    STDMETHOD_(int, SectorNumVertices)(THIS_ int sec) PURE;
    STDMETHOD_(int, SectorNumSurfaces)(THIS_ int sec) PURE;
    STDMETHOD_(void, SectorGetVertex)(THIS_ int sec, int vn, TSedVector3* vert) PURE;
    STDMETHOD_(void, SectorSetVertex)(THIS_ int sec, int vn, const TSedVector3* vert) PURE;
    STDMETHOD_(int, SectorAddVertex)(THIS_ int sec, const TSedVector3* vert) PURE;
    STDMETHOD_(int, SectorFindVertex)(THIS_ int sec, const TSedVector3* vert) PURE;
    STDMETHOD_(int, SectorDeleteVertex)(THIS_ int sec, int vn) PURE;
    STDMETHOD_(int, SectorAddSurface)(THIS_ int sec) PURE;
    STDMETHOD_(void, SectorDeleteSurface)(THIS_ int sec, int surf) PURE;
    STDMETHOD_(void, SectorUpdate)(THIS_ int sec) PURE;

    /* Surfaces */
    STDMETHOD_(void, SurfaceGet)(THIS_ int sec, int surf, TSedSurfaceRec* rec, int flags) PURE;
    STDMETHOD_(void, SurfaceSet)(THIS_ int sec, int surf, const TSedSurfaceRec* rec, int flags) PURE;
    STDMETHOD_(void, SurfaceGetNormal)(THIS_ int sec, int surf, TSedVector3* normal) PURE;
    STDMETHOD_(void, SurfaceUpdate)(THIS_ int sec, int surf, int how) PURE;
    STDMETHOD_(int, SurfaceNumVertices)(THIS_ int sec, int surf) PURE;
    STDMETHOD_(int, SurfaceGetVertexNum)(THIS_ int sec, int surf, int vn) PURE;
    STDMETHOD_(void, SurfaceSetVertexNum)(THIS_ int sec, int surf, int vn, int secvx) PURE;
    STDMETHOD_(int, SurfaceAddVertex)(THIS_ int sec, int surf, int secvn) PURE;
    STDMETHOD_(int, SurfaceInsertVertex)(THIS_ int sec, int surf, int at, int secvn) PURE;
    STDMETHOD_(void, SurfaceDeleteVertex)(THIS_ int sec, int surf, int n) PURE;
    STDMETHOD_(void, SurfaceGetVertexUV)(THIS_ int sec, int surf, int vn, float* u, float* v) PURE;
    STDMETHOD_(void, SurfaceSetVertexUV)(THIS_ int sec, int surf, int vn, float u, float v) PURE;
    STDMETHOD_(void, SurfaceGetVertexLight)(THIS_ int sec, int surf, int vn, TSedColor* color) PURE;
    STDMETHOD_(void, SurfaceSetVertexLight)(THIS_ int sec, int surf, int vn, const TSedColor* color) PURE;

    /* Things */
    STDMETHOD_(int, ThingAdd)(THIS) PURE;
    STDMETHOD_(void, ThingDelete)(THIS_ int th) PURE;
    STDMETHOD_(void, ThingGet)(THIS_ int th, TSedThingRec* rec, int flags) PURE;
    STDMETHOD_(void, ThingSet)(THIS_ int th, const TSedThingRec* rec, int flags) PURE;
    STDMETHOD_(void, ThingUpdate)(THIS_ int th) PURE;
    STDMETHOD_(int, ThingNumValues)(THIS_ int th) PURE;
    STDMETHOD_(const wchar_t*, ThingValueGetName)(THIS_ int th, int n) PURE;
    STDMETHOD_(const wchar_t*, ThingValueGetData)(THIS_ int th, int n) PURE;
    STDMETHOD_(void, ThingValueSetData)(THIS_ int th, int n, const wchar_t* val) PURE;
    STDMETHOD_(int, ThingValueAdd)(THIS_ int th, const wchar_t* name, const wchar_t* val) PURE;
    STDMETHOD_(void, ThingValueInsert)(THIS_ int th, int n, const wchar_t* name, const wchar_t* val) PURE;
    STDMETHOD_(void, ThingValueDelete)(THIS_ int th, int n) PURE;
    STDMETHOD_(void, ThingFrameGet)(THIS_ int th, int n, TSedVector3* pos, TSedVector3* pyr) PURE;
    STDMETHOD_(void, ThingFrameSet)(THIS_ int th, int n, const TSedVector3* pos, const TSedVector3* pyr) PURE;

    /* Lights */
    STDMETHOD_(int, LightAdd)(THIS) PURE;
    STDMETHOD_(void, LightDelete)(THIS_ int lt) PURE;
    STDMETHOD_(void, LightGet)(THIS_ int lt, TSedLightRec* rec, int flags) PURE;
    STDMETHOD_(void, LightSet)(THIS_ int lt, const TSedLightRec* rec, int flags) PURE;
    STDMETHOD_(void, LightUpdate)(THIS_ int lt) PURE;

    /* Layers */
    STDMETHOD_(int, NumLayers)(THIS) PURE;
    STDMETHOD_(const wchar_t*, LayerGetName)(THIS_ int n) PURE;
    STDMETHOD_(int, LayerAdd)(THIS_ const wchar_t* name) PURE;

    /* COGs */
    STDMETHOD_(int, CogAdd)(THIS_ const wchar_t* name) PURE;
    STDMETHOD_(void, CogDelete)(THIS_ int n) PURE;
    STDMETHOD_(void, CogUpdate)(THIS_ int cg) PURE;
    STDMETHOD_(const wchar_t*, CogGetFilename)(THIS_ int cg) PURE;
    STDMETHOD_(int, CogNumValues)(THIS_ int cg) PURE;
    STDMETHOD_(const wchar_t*, CogValueGetName)(THIS_ int cg, int n) PURE;
    STDMETHOD_(int, CogValueGetType)(THIS_ int cg, int n) PURE;
    STDMETHOD_(const wchar_t*, CogValueGet)(THIS_ int cg, int n) PURE;
    STDMETHOD_(int, CogValueSet)(THIS_ int cg, int n, const wchar_t* val) PURE;
    STDMETHOD_(int, CogValueAdd)(THIS_ int cg, const wchar_t* name, const wchar_t* val, int vtype) PURE;
    STDMETHOD_(void, CogValueInsert)(THIS_ int cg, int n, const wchar_t* name, const wchar_t* val, int vtype) PURE;
    STDMETHOD_(void, CogValueDelete)(THIS_ int cg, int n) PURE;

    END_INTERFACE
};

#undef INTERFACE
#define INTERFACE ISed
DECLARE_INTERFACE_(ISed, IUnknown)
{
    BEGIN_INTERFACE

    /* IUnknown methods */
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, void **ppvObject) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;

    STDMETHOD_(double, GetVersion)(THIS) PURE;
    STDMETHOD_(ISedLevel*, GetLevel)(THIS) PURE;

    STDMETHOD_(int, GetMapMode)(THIS) PURE;
    STDMETHOD(SetMapMode)(THIS_ int mode) PURE;
    STDMETHOD_(int, GetCurrentSector)(THIS) PURE;
    STDMETHOD(SetCurrentSector)(THIS_ int sc) PURE;
    STDMETHOD_(int, GetCurrentThing)(THIS) PURE;
    STDMETHOD(SetCurrentThing)(THIS_ int th) PURE;
    STDMETHOD_(int, GetCurrentLight)(THIS) PURE;
    STDMETHOD(SetCurrentLight)(THIS_ int lt) PURE;
    STDMETHOD(GetCurrentVertex)(THIS_ int* sec, int* vn) PURE;
    STDMETHOD(SetCurrentVertex)(THIS_ int sec, int vn) PURE;
    STDMETHOD(GetCurrentSurface)(THIS_ int* sec, int* surf) PURE;
    STDMETHOD(SetCurrentSurface)(THIS_ int sec, int surf) PURE;
    STDMETHOD(GetCurrentEdge)(THIS_ int* sec, int* surf, int* ed) PURE;
    STDMETHOD(SetCurrentEdge)(THIS_ int sec, int surf, int ed) PURE;
    STDMETHOD(GetCurrentFrame)(THIS_ int* th, int* fr) PURE;
    STDMETHOD(SetCurrentFrame)(THIS_ int th, int fr) PURE;

    STDMETHOD(NewLevel)(THIS_ int kind) PURE;
    STDMETHOD(LoadLevel)(THIS_ const wchar_t* name) PURE;

    STDMETHOD(RotateVector)(THIS_ TSedVector3* vec, const TSedVector3* pyr) PURE;
    STDMETHOD(RotatePoint)(THIS_ const TSedVector3* p, const TSedVector3* pivot, double angle, TSedVector3* p2) PURE;
    STDMETHOD(GetPYR)(THIS_ const TSedVector3* x, const TSedVector3* y, const TSedVector3* z, TSedVector3* pyr) PURE;

    STDMETHOD_(int, MergeSectors)(THIS_ int sec1, int sec2) PURE;
    STDMETHOD_(int, CleaveSector)(THIS_ int sec, const TSedVector3* cnormal, const TSedVector3* cp) PURE;
    STDMETHOD_(int, CreateCubicSector)(THIS_ const TSedVector3* pos, const TSedVector3* pnormal, const TSedVector3* edge) PURE;
    STDMETHOD_(BOOL, IsSectorConvex)(THIS_ int sec) PURE;
    STDMETHOD_(BOOL, IsInSector)(THIS_ int sec, const TSedVector3* p) PURE;
    STDMETHOD_(BOOL, DoSectorsOverlap)(THIS_ int sec1, int sec2) PURE;
    STDMETHOD(FindBoundingBox)(THIS_ int sec, TSedBox* box) PURE;
    STDMETHOD(FindBoundingSphere)(THIS_ int sec, TSedVector3* center, double* radius) PURE;
    STDMETHOD_(BOOL, FindCollideBox)(THIS_ int sec, const TSedBox* bbox, const TSedVector3* center, TSedBox* cbox) PURE;
    STDMETHOD_(int, FindSectorForThing)(THIS_ int th) PURE;
    STDMETHOD_(int, FindSectorForXYZ)(THIS_ double X, double Y, double Z) PURE;

    STDMETHOD(FindSurfaceCenter)(THIS_ int sec, int surf, TSedVector3* center) PURE;
    STDMETHOD_(BOOL, IsSurfaceConvex)(THIS_ int sec, int surf) PURE;
    STDMETHOD_(BOOL, IsSurfacePlanar)(THIS_ int sec, int surf) PURE;
    STDMETHOD_(BOOL, IsPointOnSurface)(THIS_ int sec, int surf, const TSedVector3* p) PURE;

    STDMETHOD_(int, CleaveSurface)(THIS_ int sec, int surf, const TSedVector3* cnormal, const TSedVector3* cp) PURE;
    STDMETHOD_(BOOL, CleaveEdge)(THIS_ int sec, int surf, int ed, const TSedVector3* cnormal, const TSedVector3* cp) PURE;
    STDMETHOD_(int, ExtrudeSurface)(THIS_ int sec, int surf, double by) PURE;
    STDMETHOD_(BOOL, JoinSurfaces)(THIS_ int sec1, int surf1, int sec2, int surf2) PURE;
    STDMETHOD_(int, MergeSurfaces)(THIS_ int sec, int surf1, int surf2) PURE;
    STDMETHOD_(BOOL, PlanarizeSurface)(THIS_ int sec, int surf) PURE;

    STDMETHOD(CalculateDefaultUVNormals)(THIS_ int sec, int surf, int orgvx, TSedVector3* un, TSedVector3* vn) PURE;
    STDMETHOD(CalculateUVNormals)(THIS_ int sec, int surf, TSedVector3* un, TSedVector3* vn) PURE;
    STDMETHOD(ArrangeTexture)(THIS_ int sec, int surf, int orgvx, const TSedVector3* un, const TSedVector3* vn) PURE;
    STDMETHOD(ArrangeTextureBy)(THIS_ int sec, int surf, const TSedVector3* un, const TSedVector3* vn, const TSedVector3* refp, double refu, double refv) PURE;
    STDMETHOD_(BOOL, IsTextureFlipped)(THIS_ int sec, int surf) PURE;
    STDMETHOD_(BOOL, StitchSurfaces)(THIS_ int sc1, int sf1, int sc2, int sf2) PURE;

    STDMETHOD(RemoveSurfaceReferences)(THIS_ int sec, int surf) PURE;
    STDMETHOD(RemoveSectorReferences)(THIS_ int sec, BOOL surfs) PURE;
    STDMETHOD_(BOOL, FindCommonEdges)(THIS_ int sc1, int sf1, int sc2, int sf2, int* v11, int* v12, int* v21, int* v22) PURE;
    STDMETHOD_(BOOL, DoSurfacesOverlap)(THIS_ int sc1, int sf1, int sc2, int sf2) PURE;

    STDMETHOD_(BOOL, MakeAdjoin)(THIS_ int sec, int surf) PURE;
    STDMETHOD_(BOOL, MakeAdjoinFromSectorUp)(THIS_ int sec, int surf, int firstSec) PURE;
    STDMETHOD_(BOOL, UnAdjoin)(THIS_ int sec, int surf) PURE;

    STDMETHOD(StartUndo)(THIS_ const wchar_t* name) PURE;
    STDMETHOD(SaveUndoForThing)(THIS_ int n, int change) PURE;
    STDMETHOD(SaveUndoForLight)(THIS_ int n, int change) PURE;
    STDMETHOD(SaveUndoForSector)(THIS_ int n, int change, int whatPart) PURE;
    STDMETHOD(ClearUndoBuffer)(THIS) PURE;
    STDMETHOD(ApplyUndo)(THIS) PURE;

    STDMETHOD_(int, GetApplicationHandle)(THIS) PURE;
    STDMETHOD_(BOOL, JoinSectors)(THIS_ int sec1, int sec2) PURE;

    STDMETHOD_(int, GetNumMultiselected)(THIS_ int what) PURE;
    STDMETHOD(ClearMultiselection)(THIS_ int what) PURE;
    STDMETHOD(RemoveFromMultiselection)(THIS_ int what, int n) PURE;

    STDMETHOD_(int, GetSelectedSector)(THIS_ int n) PURE;
    STDMETHOD(GetSelectedSurface)(THIS_ int n, int* sec, int* surf) PURE;
    STDMETHOD(GetSelectedEdge)(THIS_ int n, int* sec, int* surf, int* ed) PURE;
    STDMETHOD(GetSelectedVertex)(THIS_ int n, int* sec, int* vn) PURE;
    STDMETHOD_(int, GetSelectedThing)(THIS_ int n) PURE;
    STDMETHOD(GetSelectedFrame)(THIS_ int n, int* th, int* fr) PURE;
    STDMETHOD_(int, GetSelectedLight)(THIS_ int n) PURE;

    STDMETHOD_(int, SelectSector)(THIS_ int sec) PURE;
    STDMETHOD_(int, SelectSurface)(THIS_ int sec, int surf) PURE;
    STDMETHOD_(int, SelectEdge)(THIS_ int sec, int surf, int ed) PURE;
    STDMETHOD_(int, SelectVertex)(THIS_ int sec, int vn) PURE;
    STDMETHOD_(int, SelectThing)(THIS_ int th) PURE;
    STDMETHOD_(int, SelectFrame)(THIS_ int th, int fr) PURE;
    STDMETHOD_(int, SelectLight)(THIS_ int lt) PURE;

    STDMETHOD_(int, FindSelectedSector)(THIS_ int sec) PURE;
    STDMETHOD_(int, FindSelectedSurface)(THIS_ int sec, int surf) PURE;
    STDMETHOD_(int, FindSelectedEdge)(THIS_ int sec, int surf, int ed) PURE;
    STDMETHOD_(int, FindSelectedVertex)(THIS_ int sec, int vn) PURE;
    STDMETHOD_(int, FindSelectedThing)(THIS_ int th) PURE;
    STDMETHOD_(int, FindSelectedFrame)(THIS_ int th, int fr) PURE;
    STDMETHOD_(int, FindSelectedLight)(THIS_ int lt) PURE;

    STDMETHOD(Save)(THIS_ const wchar_t* filename) PURE;
    STDMETHOD(SaveAsSed)(THIS_ const wchar_t* filename) PURE;
    STDMETHOD(UpdateMap)(THIS) PURE;
    STDMETHOD(SetPickerCmp)(THIS_ const wchar_t* cmp) PURE;
    STDMETHOD_(const wchar_t*, PickResource)(THIS_ int what, const wchar_t* cur) PURE;
    STDMETHOD_(int, EditFlags)(THIS_ int what, int flags) PURE;

    STDMETHOD(ReloadTemplates)(THIS) PURE;
    STDMETHOD(PanMessage)(THIS_ int mType, const wchar_t* msg) PURE;
    STDMETHOD(SendKey)(THIS_ int shift, int key) PURE;
    STDMETHOD(ExecuteMenu)(THIS_ const wchar_t* itemRef) PURE;
    STDMETHOD(GetSedSetting)(THIS_ const wchar_t* name) PURE;
    STDMETHOD_(BOOL, IsLayerVisible)(THIS_ int n) PURE;

    STDMETHOD(CheckConsistencyErrors)(THIS) PURE;
    STDMETHOD(CheckResources)(THIS) PURE;
    STDMETHOD_(int, GetNumConsistencyErrors)(THIS) PURE;
    STDMETHOD_(const wchar_t*, GetConsistencyErrorString)(THIS_ int n) PURE;
    STDMETHOD_(int, GetConsistencyErrorType)(THIS_ int n) PURE;
    STDMETHOD_(int, GetConsistencyErrorItemType)(THIS_ int n) PURE;
    STDMETHOD_(int, GetConsistencyErrorSector)(THIS_ int n) PURE;
    STDMETHOD_(int, GetConsistencyErrorSurface)(THIS_ int n) PURE;
    STDMETHOD_(int, GetConsistencyErrorThing)(THIS_ int n) PURE;
    STDMETHOD_(int, GetConsistencyErrorCOG)(THIS_ int n) PURE;
    STDMETHOD_(BOOL, IsPreviewActive)(THIS) PURE;
    STDMETHOD_(const wchar_t*, GetSedString)(THIS_ int what) PURE;
    STDMETHOD_(int, GetProjectType)(THIS) PURE;
    STDMETHOD(SetProjectType)(THIS_ int kind) PURE;

    STDMETHOD_(int, GetSedWindow)(THIS_ int whichOne) PURE;

    STDMETHOD_(const wchar_t*, FileExtractExt)(THIS_ const wchar_t* filename) PURE;
    STDMETHOD_(const wchar_t*, FileExtractPath)(THIS_ const wchar_t* filename) PURE;
    STDMETHOD_(const wchar_t*, FileExtractName)(THIS_ const wchar_t* filename) PURE;
    STDMETHOD_(const wchar_t*, FindProjectDirFile)(THIS_ const wchar_t* filename) PURE;
    STDMETHOD_(BOOL, IsFileContainer)(THIS_ const wchar_t* filename) PURE;
    STDMETHOD_(BOOL, IsFileInContainer)(THIS_ const wchar_t* filename) PURE;

    STDMETHOD_(const wchar_t*, FileOpenDialog)(THIS_ const wchar_t* name, const wchar_t* filter) PURE;

    STDMETHOD_(int, OpenFile)(THIS_ const wchar_t* filename) PURE;
    STDMETHOD_(int, OpenGameFile)(THIS_ const wchar_t* filename) PURE;
    STDMETHOD(ReadFile)(THIS_ int handle, void* buf, int size) PURE;
    STDMETHOD(SetFilePos)(THIS_ int handle, int pos) PURE;
    STDMETHOD_(int, GetFilePos)(THIS_ int handle) PURE;
    STDMETHOD_(int, GetFileSize)(THIS_ int handle) PURE;
    STDMETHOD_(const wchar_t*, GetFileName)(THIS_ int handle) PURE;
    STDMETHOD(CloseFile)(THIS_ int handle) PURE;

    STDMETHOD_(int, OpenTextFile)(THIS_ const wchar_t* filename) PURE;
    STDMETHOD_(int, OpenGameTextFile)(THIS_ const wchar_t* filename) PURE;
    STDMETHOD_(const wchar_t*, GetTextFileName)(THIS_ int handle) PURE;
    STDMETHOD_(const wchar_t*, ReadTextFileString)(THIS_ int handle) PURE;
    STDMETHOD_(BOOL, TextFileEOF)(THIS_ int handle) PURE;
    STDMETHOD_(int, TextFileCurrentLine)(THIS_ int handle) PURE;
    STDMETHOD(CloseTextFile)(THIS_ int handle) PURE;

    STDMETHOD_(int, GobOpen)(THIS_ const wchar_t* filename) PURE;
    STDMETHOD_(int, GobNumFiles)(THIS_ int handle) PURE;
    STDMETHOD_(const wchar_t*, GobGetFilename)(THIS_ int handle, int n) PURE;
    STDMETHOD_(const wchar_t*, GobGetFullFilename)(THIS_ int handle, int n) PURE;
    STDMETHOD_(int, GobGetFileSize)(THIS_ int handle, int n) PURE;
    STDMETHOD_(int, GobGetFileOffset)(THIS_ int handle, int n) PURE;
    STDMETHOD(GobClose)(THIS_ int handle) PURE;

    STDMETHOD(CreateWFRenderer)(THIS_ int wnd, int whichOne, ISedWFRenderer** ppRenderer) PURE;

    END_INTERFACE
};

#ifdef __cplusplus
}
#endif

#endif //SED_PLUGINS_H
