#ifndef Sed_PLUGINS_H
#define Sed_PLUGINS_H
#include <math.h>
#include <objbase.h>
#include <stdbool.h>
#include <stdint.h>

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

/* Special SED editor Sector flags */
#define SED_SECTOR_NORGBAMBINETLIGHT 0x20000000
#define SED_SECTOR_NOAMBINETLIGHT    0x40000000

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

/**
 * Defines a SED plugin entry point
 * @param funcName - Name of the plugin initialization function
 * @param varName  - Name of the ISed* parameter (type is ISed*)
 * 
 * Signature: bool funcName(ISed* varName)
 * Returns: true on success, false on failure
 */
#define SED_PLUGIN_ENTRY(funcName, varName) \
    __pragma(comment(linker, "/EXPORT:SEDPluginLoadStdCall=_" #funcName "@4")) \
    extern "C" __declspec(dllexport) bool __stdcall funcName(ISed* varName)


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

#if !defined(__cplusplus) || defined(CINTERFACE)
#define ISedWFRenderer_QueryInterface(This, riid, ppvObject) (This)->lpVtbl->QueryInterface(This, riid, ppvObject)
#define ISedWFRenderer_AddRef(This) (This)->lpVtbl->AddRef(This)
#define ISedWFRenderer_Release(This) (This)->lpVtbl->Release(This)
#define ISedWFRenderer_GetRendererDouble(This, what) (This)->lpVtbl->GetRendererDouble(This, what)
#define ISedWFRenderer_SetRendererDouble(This, what, val) (This)->lpVtbl->SetRendererDouble(This, what, val)
#define ISedWFRenderer_GetRendererVector(This, what, x, y, z) (This)->lpVtbl->GetRendererVector(This, what, x, y, z)
#define ISedWFRenderer_SetRendererVector(This, what, x, y, z) (This)->lpVtbl->SetRendererVector(This, what, x, y, z)
#define ISedWFRenderer_NSelected(This) (This)->lpVtbl->NSelected(This)
#define ISedWFRenderer_GetNSelected(This, n) (This)->lpVtbl->GetNSelected(This, n)
#define ISedWFRenderer_SetViewPort(This, x, y, w, h) (This)->lpVtbl->SetViewPort(This, x, y, w, h)
#define ISedWFRenderer_SetColor(This, what, r, g, b) (This)->lpVtbl->SetColor(This, what, r, g, b)
#define ISedWFRenderer_SetPointSize(This, size) (This)->lpVtbl->SetPointSize(This, size)
#define ISedWFRenderer_BeginScene(This) (This)->lpVtbl->BeginScene(This)
#define ISedWFRenderer_EndScene(This) (This)->lpVtbl->EndScene(This)
#define ISedWFRenderer_SetCulling(This, how) (This)->lpVtbl->SetCulling(This, how)
#define ISedWFRenderer_DrawSector(This, sc) (This)->lpVtbl->DrawSector(This, sc)
#define ISedWFRenderer_DrawSurface(This, sc, sf) (This)->lpVtbl->DrawSurface(This, sc, sf)
#define ISedWFRenderer_DrawThing(This, th) (This)->lpVtbl->DrawThing(This, th)
#define ISedWFRenderer_DrawLine(This, x1, y1, z1, x2, y2, z2) (This)->lpVtbl->DrawLine(This, x1, y1, z1, x2, y2, z2)
#define ISedWFRenderer_DrawVertex(This, X, Y, Z) (This)->lpVtbl->DrawVertex(This, X, Y, Z)
#define ISedWFRenderer_DrawGrid(This) (This)->lpVtbl->DrawGrid(This)
#define ISedWFRenderer_BeginPick(This, x, y) (This)->lpVtbl->BeginPick(This, x, y)
#define ISedWFRenderer_EndPick(This) (This)->lpVtbl->EndPick(This)
#define ISedWFRenderer_PickSector(This, sc, id) (This)->lpVtbl->PickSector(This, sc, id)
#define ISedWFRenderer_PickSurface(This, sc, sf, id) (This)->lpVtbl->PickSurface(This, sc, sf, id)
#define ISedWFRenderer_PickLine(This, x1, y1, z1, x2, y2, z2, id) (This)->lpVtbl->PickLine(This, x1, y1, z1, x2, y2, z2, id)
#define ISedWFRenderer_PickVertex(This, X, Y, Z, id) (This)->lpVtbl->PickVertex(This, X, Y, Z, id)
#define ISedWFRenderer_BeginRectPick(This, x1, y1, x2, y2) (This)->lpVtbl->BeginRectPick(This, x1, y1, x2, y2)
#define ISedWFRenderer_EndRectPick(This) (This)->lpVtbl->EndRectPick(This)
#define ISedWFRenderer_IsSectorInRect(This, sc) (This)->lpVtbl->IsSectorInRect(This, sc)
#define ISedWFRenderer_IsSurfaceInRect(This, sc, sf) (This)->lpVtbl->IsSurfaceInRect(This, sc, sf)
#define ISedWFRenderer_IsLineInRect(This, x1, y1, z1, x2, y2, z2) (This)->lpVtbl->IsLineInRect(This, x1, y1, z1, x2, y2, z2)
#define ISedWFRenderer_IsVertexInRect(This, X, Y, Z) (This)->lpVtbl->IsVertexInRect(This, X, Y, Z)
#define ISedWFRenderer_GetXYZonPlaneAt(This, scX, scY, pnormal, pX, pY, pZ, X, Y, Z) (This)->lpVtbl->GetXYZonPlaneAt(This, scX, scY, pnormal, pX, pY, pZ, X, Y, Z)
#define ISedWFRenderer_GetGridAt(This, scX, scY, X, Y, Z) (This)->lpVtbl->GetGridAt(This, scX, scY, X, Y, Z)
#define ISedWFRenderer_GetNearestGridNode(This, iX, iY, iZ, X, Y, Z) (This)->lpVtbl->GetNearestGridNode(This, iX, iY, iZ, X, Y, Z)
#define ISedWFRenderer_ProjectPoint(This, x, y, z, WinX, WinY) (This)->lpVtbl->ProjectPoint(This, x, y, z, WinX, WinY)
#define ISedWFRenderer_UnProjectPoint(This, WinX, WinY, WinZ, x, y, z) (This)->lpVtbl->UnProjectPoint(This, WinX, WinY, WinZ, x, y, z)
#define ISedWFRenderer_IsSurfaceFacing(This, sc, sf) (This)->lpVtbl->IsSurfaceFacing(This, sc, sf)
#define ISedWFRenderer_HandleWMQueryPal(This) (This)->lpVtbl->HandleWMQueryPal(This)
#define ISedWFRenderer_HandleWMChangePal(This) (This)->lpVtbl->HandleWMChangePal(This)
#else
#endif

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

#if !defined(__cplusplus) || defined(CINTERFACE) 
#define ISedLevel_QueryInterface(This, riid, ppvObject) (This)->lpVtbl->QueryInterface(This, riid, ppvObject)
#define ISedLevel_AddRef(This) (This)->lpVtbl->AddRef(This)
#define ISedLevel_Release(This) (This)->lpVtbl->Release(This)
#define ISedLevel_GetLevelHeader(This, lh, flags) (This)->lpVtbl->GetLevelHeader(This, lh, flags)
#define ISedLevel_SetLevelHeader(This, lh, flags) (This)->lpVtbl->SetLevelHeader(This, lh, flags)
#define ISedLevel_NumSectors(This) (This)->lpVtbl->NumSectors(This)
#define ISedLevel_NumThings(This) (This)->lpVtbl->NumThings(This)
#define ISedLevel_NumLights(This) (This)->lpVtbl->NumLights(This)
#define ISedLevel_NumCogs(This) (This)->lpVtbl->NumCogs(This)
#define ISedLevel_SectorAdd(This) (This)->lpVtbl->SectorAdd(This)
#define ISedLevel_SectorDelete(This, n) (This)->lpVtbl->SectorDelete(This, n)
#define ISedLevel_SectorGet(This, sec, rec, flags) (This)->lpVtbl->SectorGet(This, sec, rec, flags)
#define ISedLevel_SectorSet(This, sec, rec, flags) (This)->lpVtbl->SectorSet(This, sec, rec, flags)
#define ISedLevel_SectorNumVertices(This, sec) (This)->lpVtbl->SectorNumVertices(This, sec)
#define ISedLevel_SectorNumSurfaces(This, sec) (This)->lpVtbl->SectorNumSurfaces(This, sec)
#define ISedLevel_SectorGetVertex(This, sec, vn, vert) (This)->lpVtbl->SectorGetVertex(This, sec, vn, vert)
#define ISedLevel_SectorSetVertex(This, sec, vn, vert) (This)->lpVtbl->SectorSetVertex(This, sec, vn, vert)
#define ISedLevel_SectorAddVertex(This, sec, vert) (This)->lpVtbl->SectorAddVertex(This, sec, vert)
#define ISedLevel_SectorFindVertex(This, sec, vert) (This)->lpVtbl->SectorFindVertex(This, sec, vert)
#define ISedLevel_SectorDeleteVertex(This, sec, vn) (This)->lpVtbl->SectorDeleteVertex(This, sec, vn)
#define ISedLevel_SectorAddSurface(This, sec) (This)->lpVtbl->SectorAddSurface(This, sec)
#define ISedLevel_SectorDeleteSurface(This, sec, surf) (This)->lpVtbl->SectorDeleteSurface(This, sec, surf)
#define ISedLevel_SectorUpdate(This, sec) (This)->lpVtbl->SectorUpdate(This, sec)
#define ISedLevel_SurfaceGet(This, sec, surf, rec, flags) (This)->lpVtbl->SurfaceGet(This, sec, surf, rec, flags)
#define ISedLevel_SurfaceSet(This, sec, surf, rec, flags) (This)->lpVtbl->SurfaceSet(This, sec, surf, rec, flags)
#define ISedLevel_SurfaceGetNormal(This, sec, surf, normal) (This)->lpVtbl->SurfaceGetNormal(This, sec, surf, normal)
#define ISedLevel_SurfaceUpdate(This, sec, surf, how) (This)->lpVtbl->SurfaceUpdate(This, sec, surf, how)
#define ISedLevel_SurfaceNumVertices(This, sec, surf) (This)->lpVtbl->SurfaceNumVertices(This, sec, surf)
#define ISedLevel_SurfaceGetVertexNum(This, sec, surf, vn) (This)->lpVtbl->SurfaceGetVertexNum(This, sec, surf, vn)
#define ISedLevel_SurfaceSetVertexNum(This, sec, surf, vn, secvx) (This)->lpVtbl->SurfaceSetVertexNum(This, sec, surf, vn, secvx)
#define ISedLevel_SurfaceAddVertex(This, sec, surf, secvn) (This)->lpVtbl->SurfaceAddVertex(This, sec, surf, secvn)
#define ISedLevel_SurfaceInsertVertex(This, sec, surf, at, secvn) (This)->lpVtbl->SurfaceInsertVertex(This, sec, surf, at, secvn)
#define ISedLevel_SurfaceDeleteVertex(This, sec, surf, n) (This)->lpVtbl->SurfaceDeleteVertex(This, sec, surf, n)
#define ISedLevel_SurfaceGetVertexUV(This, sec, surf, vn, u, v) (This)->lpVtbl->SurfaceGetVertexUV(This, sec, surf, vn, u, v)
#define ISedLevel_SurfaceSetVertexUV(This, sec, surf, vn, u, v) (This)->lpVtbl->SurfaceSetVertexUV(This, sec, surf, vn, u, v)
#define ISedLevel_SurfaceGetVertexLight(This, sec, surf, vn, color) (This)->lpVtbl->SurfaceGetVertexLight(This, sec, surf, vn, color)
#define ISedLevel_SurfaceSetVertexLight(This, sec, surf, vn, color) (This)->lpVtbl->SurfaceSetVertexLight(This, sec, surf, vn, color)
#define ISedLevel_ThingAdd(This) (This)->lpVtbl->ThingAdd(This)
#define ISedLevel_ThingDelete(This, th) (This)->lpVtbl->ThingDelete(This, th)
#define ISedLevel_ThingGet(This, th, rec, flags) (This)->lpVtbl->ThingGet(This, th, rec, flags)
#define ISedLevel_ThingSet(This, th, rec, flags) (This)->lpVtbl->ThingSet(This, th, rec, flags)
#define ISedLevel_ThingUpdate(This, th) (This)->lpVtbl->ThingUpdate(This, th)
#define ISedLevel_ThingNumValues(This, th) (This)->lpVtbl->ThingNumValues(This, th)
#define ISedLevel_ThingValueGetName(This, th, n) (This)->lpVtbl->ThingValueGetName(This, th, n)
#define ISedLevel_ThingValueGetData(This, th, n) (This)->lpVtbl->ThingValueGetData(This, th, n)
#define ISedLevel_ThingValueSetData(This, th, n, val) (This)->lpVtbl->ThingValueSetData(This, th, n, val)
#define ISedLevel_ThingValueAdd(This, th, name, val) (This)->lpVtbl->ThingValueAdd(This, th, name, val)
#define ISedLevel_ThingValueInsert(This, th, n, name, val) (This)->lpVtbl->ThingValueInsert(This, th, n, name, val)
#define ISedLevel_ThingValueDelete(This, th, n) (This)->lpVtbl->ThingValueDelete(This, th, n)
#define ISedLevel_ThingFrameGet(This, th, n, pos, pyr) (This)->lpVtbl->ThingFrameGet(This, th, n, pos, pyr)
#define ISedLevel_ThingFrameSet(This, th, n, pos, pyr) (This)->lpVtbl->ThingFrameSet(This, th, n, pos, pyr)
#define ISedLevel_LightAdd(This) (This)->lpVtbl->LightAdd(This)
#define ISedLevel_LightDelete(This, lt) (This)->lpVtbl->LightDelete(This, lt)
#define ISedLevel_LightGet(This, lt, rec, flags) (This)->lpVtbl->LightGet(This, lt, rec, flags)
#define ISedLevel_LightSet(This, lt, rec, flags) (This)->lpVtbl->LightSet(This, lt, rec, flags)
#define ISedLevel_LightUpdate(This, lt) (This)->lpVtbl->LightUpdate(This, lt)
#define ISedLevel_NumLayers(This) (This)->lpVtbl->NumLayers(This)
#define ISedLevel_LayerGetName(This, n) (This)->lpVtbl->LayerGetName(This, n)
#define ISedLevel_LayerAdd(This, name) (This)->lpVtbl->LayerAdd(This, name)
#define ISedLevel_CogAdd(This, name) (This)->lpVtbl->CogAdd(This, name)
#define ISedLevel_CogDelete(This, n) (This)->lpVtbl->CogDelete(This, n)
#define ISedLevel_CogUpdate(This, cg) (This)->lpVtbl->CogUpdate(This, cg)
#define ISedLevel_CogGetFilename(This, cg) (This)->lpVtbl->CogGetFilename(This, cg)
#define ISedLevel_CogNumValues(This, cg) (This)->lpVtbl->CogNumValues(This, cg)
#define ISedLevel_CogValueGetName(This, cg, n) (This)->lpVtbl->CogValueGetName(This, cg, n)
#define ISedLevel_CogValueGetType(This, cg, n) (This)->lpVtbl->CogValueGetType(This, cg, n)
#define ISedLevel_CogValueGet(This, cg, n) (This)->lpVtbl->CogValueGet(This, cg, n)
#define ISedLevel_CogValueSet(This, cg, n, val) (This)->lpVtbl->CogValueSet(This, cg, n, val)
#define ISedLevel_CogValueAdd(This, cg, name, val, vtype) (This)->lpVtbl->CogValueAdd(This, cg, name, val, vtype)
#define ISedLevel_CogValueInsert(This, cg, n, name, val, vtype) (This)->lpVtbl->CogValueInsert(This, cg, n, name, val, vtype)
#define ISedLevel_CogValueDelete(This, cg, n) (This)->lpVtbl->CogValueDelete(This, cg, n)
#else
#endif

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
    STDMETHOD(RotatePoint)(THIS_ const TSedVector3* point, const TSedVector3* pivot, double angle, TSedVector3* point2) PURE;
    STDMETHOD(GetPYR)(THIS_ const TSedVector3* x, const TSedVector3* y, const TSedVector3* z, TSedVector3* pyr) PURE;

    STDMETHOD_(int, MergeSectors)(THIS_ int sec1, int sec2) PURE;
    STDMETHOD_(int, CleaveSector)(THIS_ int sec, const TSedVector3* cnormal, const TSedVector3* cp) PURE;
    STDMETHOD_(int, CreateCubicSector)(THIS_ const TSedVector3* pos, const TSedVector3* pnormal, const TSedVector3* edge) PURE;
    STDMETHOD_(BOOL, IsSectorConvex)(THIS_ int sec) PURE;
    STDMETHOD_(BOOL, IsInSector)(THIS_ int sec, const TSedVector3* point) PURE;
    STDMETHOD_(BOOL, DoSectorsOverlap)(THIS_ int sec1, int sec2) PURE;
    STDMETHOD(FindBoundingBox)(THIS_ int sec, TSedBox* box) PURE;
    STDMETHOD(FindBoundingSphere)(THIS_ int sec, TSedVector3* center, double* radius) PURE;
    STDMETHOD_(BOOL, FindCollideBox)(THIS_ int sec, const TSedBox* bbox, const TSedVector3* center, TSedBox* cbox) PURE;
    STDMETHOD_(int, FindSectorForThing)(THIS_ int th) PURE;
    STDMETHOD_(int, FindSectorForXYZ)(THIS_ double X, double Y, double Z) PURE;

    STDMETHOD(FindSurfaceCenter)(THIS_ int sec, int surf, TSedVector3* center) PURE;
    STDMETHOD_(BOOL, IsSurfaceConvex)(THIS_ int sec, int surf) PURE;
    STDMETHOD_(BOOL, IsSurfacePlanar)(THIS_ int sec, int surf) PURE;
    STDMETHOD_(BOOL, IsPointOnSurface)(THIS_ int sec, int surf, const TSedVector3* point) PURE;

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

#if !defined(__cplusplus) || defined(CINTERFACE)
#define ISed_QueryInterface(This, riid, ppvObject) (This)->lpVtbl->QueryInterface(This, riid, ppvObject)
#define ISed_AddRef(This) (This)->lpVtbl->AddRef(This)
#define ISed_Release(This) (This)->lpVtbl->Release(This)
#define ISed_GetVersion(This) (This)->lpVtbl->GetVersion(This)
#define ISed_GetLevel(This) (This)->lpVtbl->GetLevel(This)
#define ISed_GetMapMode(This) (This)->lpVtbl->GetMapMode(This)
#define ISed_SetMapMode(This, mode) (This)->lpVtbl->SetMapMode(This, mode)
#define ISed_GetCurrentSector(This) (This)->lpVtbl->GetCurrentSector(This)
#define ISed_SetCurrentSector(This, sc) (This)->lpVtbl->SetCurrentSector(This, sc)
#define ISed_GetCurrentThing(This) (This)->lpVtbl->GetCurrentThing(This)
#define ISed_SetCurrentThing(This, th) (This)->lpVtbl->SetCurrentThing(This, th)
#define ISed_GetCurrentLight(This) (This)->lpVtbl->GetCurrentLight(This)
#define ISed_SetCurrentLight(This, lt) (This)->lpVtbl->SetCurrentLight(This, lt)
#define ISed_GetCurrentVertex(This, sec, vn) (This)->lpVtbl->GetCurrentVertex(This, sec, vn)
#define ISed_SetCurrentVertex(This, sec, vn) (This)->lpVtbl->SetCurrentVertex(This, sec, vn)
#define ISed_GetCurrentSurface(This, sec, surf) (This)->lpVtbl->GetCurrentSurface(This, sec, surf)
#define ISed_SetCurrentSurface(This, sec, surf) (This)->lpVtbl->SetCurrentSurface(This, sec, surf)
#define ISed_GetCurrentEdge(This, sec, surf, ed) (This)->lpVtbl->GetCurrentEdge(This, sec, surf, ed)
#define ISed_SetCurrentEdge(This, sec, surf, ed) (This)->lpVtbl->SetCurrentEdge(This, sec, surf, ed)
#define ISed_GetCurrentFrame(This, th, fr) (This)->lpVtbl->GetCurrentFrame(This, th, fr)
#define ISed_SetCurrentFrame(This, th, fr) (This)->lpVtbl->SetCurrentFrame(This, th, fr)
#define ISed_NewLevel(This, kind) (This)->lpVtbl->NewLevel(This, kind)
#define ISed_LoadLevel(This, name) (This)->lpVtbl->LoadLevel(This, name)
#define ISed_RotateVector(This, vec, pyr) (This)->lpVtbl->RotateVector(This, vec, pyr)
#define ISed_RotatePoint(This, point, pivot, angle, point2) (This)->lpVtbl->RotatePoint(This, point, pivot, angle, point2)
#define ISed_GetPYR(This, x, y, z, pyr) (This)->lpVtbl->GetPYR(This, x, y, z, pyr)
#define ISed_MergeSectors(This, sec1, sec2) (This)->lpVtbl->MergeSectors(This, sec1, sec2)
#define ISed_CleaveSector(This, sec, cnormal, cp) (This)->lpVtbl->CleaveSector(This, sec, cnormal, cp)
#define ISed_CreateCubicSector(This, pos, pnormal, edge) (This)->lpVtbl->CreateCubicSector(This, pos, pnormal, edge)
#define ISed_IsSectorConvex(This, sec) (This)->lpVtbl->IsSectorConvex(This, sec)
#define ISed_IsInSector(This, sec, point) (This)->lpVtbl->IsInSector(This, sec, point)
#define ISed_DoSectorsOverlap(This, sec1, sec2) (This)->lpVtbl->DoSectorsOverlap(This, sec1, sec2)
#define ISed_FindBoundingBox(This, sec, box) (This)->lpVtbl->FindBoundingBox(This, sec, box)
#define ISed_FindBoundingSphere(This, sec, center, radius) (This)->lpVtbl->FindBoundingSphere(This, sec, center, radius)
#define ISed_FindCollideBox(This, sec, bbox, center, cbox) (This)->lpVtbl->FindCollideBox(This, sec, bbox, center, cbox)
#define ISed_FindSectorForThing(This, th) (This)->lpVtbl->FindSectorForThing(This, th)
#define ISed_FindSectorForXYZ(This, X, Y, Z) (This)->lpVtbl->FindSectorForXYZ(This, X, Y, Z)
#define ISed_FindSurfaceCenter(This, sec, surf, center) (This)->lpVtbl->FindSurfaceCenter(This, sec, surf, center)
#define ISed_IsSurfaceConvex(This, sec, surf) (This)->lpVtbl->IsSurfaceConvex(This, sec, surf)
#define ISed_IsSurfacePlanar(This, sec, surf) (This)->lpVtbl->IsSurfacePlanar(This, sec, surf)
#define ISed_IsPointOnSurface(This, sec, surf, point) (This)->lpVtbl->IsPointOnSurface(This, sec, surf, point)
#define ISed_CleaveSurface(This, sec, surf, cnormal, cp) (This)->lpVtbl->CleaveSurface(This, sec, surf, cnormal, cp)
#define ISed_CleaveEdge(This, sec, surf, ed, cnormal, cp) (This)->lpVtbl->CleaveEdge(This, sec, surf, ed, cnormal, cp)
#define ISed_ExtrudeSurface(This, sec, surf, by) (This)->lpVtbl->ExtrudeSurface(This, sec, surf, by)
#define ISed_JoinSurfaces(This, sec1, surf1, sec2, surf2) (This)->lpVtbl->JoinSurfaces(This, sec1, surf1, sec2, surf2)
#define ISed_MergeSurfaces(This, sec, surf1, surf2) (This)->lpVtbl->MergeSurfaces(This, sec, surf1, surf2)
#define ISed_PlanarizeSurface(This, sec, surf) (This)->lpVtbl->PlanarizeSurface(This, sec, surf)
#define ISed_CalculateDefaultUVNormals(This, sec, surf, orgvx, un, vn) (This)->lpVtbl->CalculateDefaultUVNormals(This, sec, surf, orgvx, un, vn)
#define ISed_CalculateUVNormals(This, sec, surf, un, vn) (This)->lpVtbl->CalculateUVNormals(This, sec, surf, un, vn)
#define ISed_ArrangeTexture(This, sec, surf, orgvx, un, vn) (This)->lpVtbl->ArrangeTexture(This, sec, surf, orgvx, un, vn)
#define ISed_ArrangeTextureBy(This, sec, surf, un, vn, refp, refu, refv) (This)->lpVtbl->ArrangeTextureBy(This, sec, surf, un, vn, refp, refu, refv)
#define ISed_IsTextureFlipped(This, sec, surf) (This)->lpVtbl->IsTextureFlipped(This, sec, surf)
#define ISed_StitchSurfaces(This, sc1, sf1, sc2, sf2) (This)->lpVtbl->StitchSurfaces(This, sc1, sf1, sc2, sf2)
#define ISed_RemoveSurfaceReferences(This, sec, surf) (This)->lpVtbl->RemoveSurfaceReferences(This, sec, surf)
#define ISed_RemoveSectorReferences(This, sec, surfs) (This)->lpVtbl->RemoveSectorReferences(This, sec, surfs)
#define ISed_FindCommonEdges(This, sc1, sf1, sc2, sf2, v11, v12, v21, v22) (This)->lpVtbl->FindCommonEdges(This, sc1, sf1, sc2, sf2, v11, v12, v21, v22)
#define ISed_DoSurfacesOverlap(This, sc1, sf1, sc2, sf2) (This)->lpVtbl->DoSurfacesOverlap(This, sc1, sf1, sc2, sf2)
#define ISed_MakeAdjoin(This, sec, surf) (This)->lpVtbl->MakeAdjoin(This, sec, surf)
#define ISed_MakeAdjoinFromSectorUp(This, sec, surf, firstSec) (This)->lpVtbl->MakeAdjoinFromSectorUp(This, sec, surf, firstSec)
#define ISed_UnAdjoin(This, sec, surf) (This)->lpVtbl->UnAdjoin(This, sec, surf)
#define ISed_StartUndo(This, name) (This)->lpVtbl->StartUndo(This, name)
#define ISed_SaveUndoForThing(This, n, change) (This)->lpVtbl->SaveUndoForThing(This, n, change)
#define ISed_SaveUndoForLight(This, n, change) (This)->lpVtbl->SaveUndoForLight(This, n, change)
#define ISed_SaveUndoForSector(This, n, change, whatPart) (This)->lpVtbl->SaveUndoForSector(This, n, change, whatPart)
#define ISed_ClearUndoBuffer(This) (This)->lpVtbl->ClearUndoBuffer(This)
#define ISed_ApplyUndo(This) (This)->lpVtbl->ApplyUndo(This)
#define ISed_GetApplicationHandle(This) (This)->lpVtbl->GetApplicationHandle(This)
#define ISed_JoinSectors(This, sec1, sec2) (This)->lpVtbl->JoinSectors(This, sec1, sec2)
#define ISed_GetNumMultiselected(This, what) (This)->lpVtbl->GetNumMultiselected(This, what)
#define ISed_ClearMultiselection(This, what) (This)->lpVtbl->ClearMultiselection(This, what)
#define ISed_RemoveFromMultiselection(This, what, n) (This)->lpVtbl->RemoveFromMultiselection(This, what, n)
#define ISed_GetSelectedSector(This, n) (This)->lpVtbl->GetSelectedSector(This, n)
#define ISed_GetSelectedSurface(This, n, sec, surf) (This)->lpVtbl->GetSelectedSurface(This, n, sec, surf)
#define ISed_GetSelectedEdge(This, n, sec, surf, ed) (This)->lpVtbl->GetSelectedEdge(This, n, sec, surf, ed)
#define ISed_GetSelectedVertex(This, n, sec, vn) (This)->lpVtbl->GetSelectedVertex(This, n, sec, vn)
#define ISed_GetSelectedThing(This, n) (This)->lpVtbl->GetSelectedThing(This, n)
#define ISed_GetSelectedFrame(This, n, th, fr) (This)->lpVtbl->GetSelectedFrame(This, n, th, fr)
#define ISed_GetSelectedLight(This, n) (This)->lpVtbl->GetSelectedLight(This, n)
#define ISed_SelectSector(This, sec) (This)->lpVtbl->SelectSector(This, sec)
#define ISed_SelectSurface(This, sec, surf) (This)->lpVtbl->SelectSurface(This, sec, surf)
#define ISed_SelectEdge(This, sec, surf, ed) (This)->lpVtbl->SelectEdge(This, sec, surf, ed)
#define ISed_SelectVertex(This, sec, vn) (This)->lpVtbl->SelectVertex(This, sec, vn)
#define ISed_SelectThing(This, th) (This)->lpVtbl->SelectThing(This, th)
#define ISed_SelectFrame(This, th, fr) (This)->lpVtbl->SelectFrame(This, th, fr)
#define ISed_SelectLight(This, lt) (This)->lpVtbl->SelectLight(This, lt)
#define ISed_FindSelectedSector(This, sec) (This)->lpVtbl->FindSelectedSector(This, sec)
#define ISed_FindSelectedSurface(This, sec, surf) (This)->lpVtbl->FindSelectedSurface(This, sec, surf)
#define ISed_FindSelectedEdge(This, sec, surf, ed) (This)->lpVtbl->FindSelectedEdge(This, sec, surf, ed)
#define ISed_FindSelectedVertex(This, sec, vn) (This)->lpVtbl->FindSelectedVertex(This, sec, vn)
#define ISed_FindSelectedThing(This, th) (This)->lpVtbl->Fin
#else
#endif

/* Sets vector components */
inline void Sed_SetVector(TSedVector3* vec, double x, double y, double z)
{
    vec->x = x;
    vec->y = y;
    vec->z = z;
}

/* Calculates dot product of two vectors */
inline double Sed_VectorDot(const TSedVector3* vec1, const TSedVector3* vec2)
{
    return vec1->x * vec2->x + vec1->y * vec2->y + vec1->z * vec2->z;
}

/* Calculates cross product of two vectors */
inline void Sed_VectorCross(const TSedVector3* vec1, const TSedVector3* vec2, TSedVector3* vec)
{
    vec->x = vec1->y * vec2->z - vec2->y * vec1->z;
    vec->y = vec2->x * vec1->z - vec1->x * vec2->z;
    vec->z = vec1->x * vec2->y - vec2->x * vec1->y;
}

/* Calculates the length (magnitude) of a vector */
inline double Sed_VectorLen(const TSedVector3* vec)
{
    return sqrt(vec->x * vec->x + vec->y * vec->y + vec->z * vec->z);
}

/* Normalizes a vector to unit length
   Returns true if successful, false if vector length is zero */
inline bool Sed_VectorNormalize(TSedVector3* vec)
{
    double len = Sed_VectorLen(vec);
    if ( len == 0.0 )
    {
        return false;
    }
    
    vec->dx /= len;
    vec->dy /= len;
    vec->dz /= len;
    return true;
}

/* Tests if two bounding boxes intersect */
static bool Sed_CheckBoxIntersect(const TSedBox* box1, const TSedBox* box2)
{
    if (box1->p2.x < box2->p1.x) return false;
    if (box1->p1.x > box2->p2.x) return false;
    if (box1->p2.y < box2->p1.y) return false;
    if (box1->p1.y > box2->p2.y) return false;
    if (box1->p2.z < box2->p1.z) return false;
    if (box1->p1.z > box2->p2.z) return false;
    return true;
}

/* Sets up a bounding box with proper min/max coordinates */
inline void Sed_SetBox(TSedBox* box, double x1, double x2, double y1, double y2, double z1, double z2)
{
    box->p1.x = fmin(x1, x2);
    box->p2.x = fmax(x1, x2);
    box->p1.y = fmin(y1, y2);
    box->p2.y = fmax(y1, y2);
    box->p1.z = fmin(z1, z2);
    box->p2.z = fmax(z1, z2);
}

/* Tests if a point lies within a bounding box (with tolerance) */
inline bool Sed_IsPointInBox(const TSedBox* box, double x, double y, double z)
{
    return (x - box->p1.x >= -sedCloseEnough) && (x - box->p2.x <= sedCloseEnough) &&
           (y - box->p1.y >= -sedCloseEnough) && (y - box->p2.y <= sedCloseEnough) &&
           (z - box->p1.z >= -sedCloseEnough) && (z - box->p2.z <= sedCloseEnough);
}

/* Calculates intersection of line segment with plane
   Returns true if intersection found, false otherwise
   Plane defined by: normal vector and point (pX, pY, pZ)
   Line segment defined by: two points (x1,y1,z1) to (x2,y2,z2)
   Intersection point returned in (x, y, z) */
static bool Sed_CheckLinePlaneIntersect(const TSedVector3* normal, double pX, double pY, double pZ,
                                        double x1, double y1, double z1, double x2, double y2, double z2,
                                        double* x, double* y, double* z)
{
    TSedVector3 v1;
    Sed_SetVector(&v1, x1 - pX, y1 - pY, z1 - pZ);

    TSedVector3 v2;
    Sed_SetVector(&v2, x2 - pX, y2 - pY, z2 - pZ);

    double dist1 = Sed_VectorDot(&v1, normal);
    double dist2 = Sed_VectorDot(&v2, normal);

    if ( dist1 == dist2 )
    {
        return false;
    }

    double k = dist1 / (dist1 - dist2);
    *x = x1 + k * (x2 - x1);
    *y = y1 + k * (y2 - y1);
    *z = z1 + k * (z2 - z1);

    return true;
}

#ifdef __cplusplus
}
#endif

#endif //Sed_PLUGINS_H

