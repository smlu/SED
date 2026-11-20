#pragma once

// Binary interface of the JED 0.951 / SED "IJED" plug-in API.
//
// The layout follows JED_PLUGINS.h and JED_PLUGINS.PAS from the original
// editor: three IUnknown slots followed by every method in declaration
// order. The interfaces are declared as abstract C++ classes, which is how
// DECLARE_INTERFACE_ expands; MSVC (COM) and Itanium compilers both place the
// vtable pointer first and the slots in declaration order. Constant and
// record names keep the SDK spelling so they can be compared with plug-in
// sources. Do not reorder, insert, or remove methods: plug-ins call by slot.

#include <cstddef>
#include <cstdint>

#if defined(_WIN32) && !defined(_WIN64)
#define SED_JED_CALL __stdcall
#else
#define SED_JED_CALL
#endif

namespace sed::plugins::jed
{

using JedResult = std::int32_t; // HRESULT
using JedLong = std::int32_t;   // Windows `long` (LLP64 and ILP32)

inline constexpr JedResult kOk = 0;
inline constexpr JedResult kFail = static_cast<JedResult>(0x80004005u);
inline constexpr JedResult kNotImplemented = static_cast<JedResult>(0x80004001u);
inline constexpr JedResult kInvalidArgument = static_cast<JedResult>(0x80070057u);
inline constexpr JedResult kClassNotAvailable = static_cast<JedResult>(0x80040111u);

// SDK constants (JED_PLUGINS.h).
inline constexpr int lh_version = 1;
inline constexpr int lh_gravity = 2;
inline constexpr int lh_skyZ = 4;
inline constexpr int lh_cSkyOffs = 8;
inline constexpr int lh_HorDist = 0x10;
inline constexpr int lh_HorPPR = 0x20;
inline constexpr int lh_HSkyOffs = 0x40;
inline constexpr int lh_MipMapDist = 0x80;
inline constexpr int lh_LODDist = 0x100;
inline constexpr int lh_PerspDist = 0x200;
inline constexpr int lh_GouraudDist = 0x400;
inline constexpr int lh_ppu = 0x800;
inline constexpr int lh_MasterCMP = 0x1000;
inline constexpr int lh_all = 0x1FFF;
inline constexpr int s_flags = 1;
inline constexpr int s_ambient = 2;
inline constexpr int s_extra = 4;
inline constexpr int s_cmp = 8;
inline constexpr int s_tint = 0x10;
inline constexpr int s_sound = 0x20;
inline constexpr int s_sndvol = 0x40;
inline constexpr int s_layer = 0x80;
inline constexpr int s_all = 0xFF;
inline constexpr int sf_adjoin = 1;
inline constexpr int sf_adjoinflags = 2;
inline constexpr int sf_SurfFlags = 4;
inline constexpr int sf_FaceFlags = 8;
inline constexpr int sf_Material = 0x10;
inline constexpr int sf_geo = 0x20;
inline constexpr int sf_light = 0x40;
inline constexpr int sf_tex = 0x80;
inline constexpr int sf_ExtraLight = 0x100;
inline constexpr int sf_txscale = 0x200;
inline constexpr int sf_all = 0x3FF;
inline constexpr int th_name = 1;
inline constexpr int th_sector = 2;
inline constexpr int th_position = 4;
inline constexpr int th_orientation = 8;
inline constexpr int th_layer = 0x10;
inline constexpr int th_all = 0x1F;
inline constexpr int lt_position = 1;
inline constexpr int lt_intensity = 2;
inline constexpr int lt_range = 4;
inline constexpr int lt_rgb = 8;
inline constexpr int lt_rgbintensity = 0x10;
inline constexpr int lt_rgbrange = 0x20;
inline constexpr int lt_flags = 0x40;
inline constexpr int lt_layer = 0x80;
inline constexpr int lt_all = 0xFF;
inline constexpr int MM_SC = 0;
inline constexpr int MM_SF = 1;
inline constexpr int MM_VX = 2;
inline constexpr int MM_TH = 3;
inline constexpr int MM_ED = 4;
inline constexpr int MM_LT = 5;
inline constexpr int MM_FR = 6;
inline constexpr int su_texture = 1;
inline constexpr int su_floorflag = 2;
inline constexpr int su_sector = 4;
inline constexpr int su_all = 7;
inline constexpr int uc_changed = 0;
inline constexpr int uc_added = 1;
inline constexpr int uc_deleted = 2;
inline constexpr int sc_values = 1;
inline constexpr int sc_geometry = 2;
inline constexpr int ct_unknown = 0;
inline constexpr int ct_ai = 1;
inline constexpr int ct_cog = 2;
inline constexpr int ct_key = 3;
inline constexpr int ct_mat = 4;
inline constexpr int ct_msg = 5;
inline constexpr int ct_3do = 6;
inline constexpr int ct_sec = 7;
inline constexpr int ct_wav = 8;
inline constexpr int ct_surface = 9;
inline constexpr int ct_template = 10;
inline constexpr int ct_thing = 11;
inline constexpr int ct_int = 12;
inline constexpr int ct_flex = 13;
inline constexpr int ct_vector = 14;
inline constexpr int pr_thing = 1;
inline constexpr int pr_template = 2;
inline constexpr int pr_cmp = 3;
inline constexpr int pr_secsound = 4;
inline constexpr int pr_mat = 5;
inline constexpr int pr_cog = 6;
inline constexpr int pr_layer = 7;
inline constexpr int pr_3do = 8;
inline constexpr int pr_ai = 9;
inline constexpr int pr_key = 10;
inline constexpr int pr_snd = 11;
inline constexpr int pr_pup = 12;
inline constexpr int pr_spr = 13;
inline constexpr int pr_par = 14;
inline constexpr int pr_per = 15;
inline constexpr int pr_jklsmksan = 16;
inline constexpr int ef_sector = 1;
inline constexpr int ef_surface = 2;
inline constexpr int ef_adjoin = 3;
inline constexpr int ef_thing = 4;
inline constexpr int ef_face = 5;
inline constexpr int ef_light = 6;
inline constexpr int ef_geo = 7;
inline constexpr int ef_lightmode = 8;
inline constexpr int ef_tex = 9;
inline constexpr int msg_info = 0;
inline constexpr int msg_warning = 1;
inline constexpr int msg_error = 2;
inline constexpr int sk_Ctrl = 1;
inline constexpr int sk_Shift = 2;
inline constexpr int sk_Alt = 4;
inline constexpr int ce_none = 0;
inline constexpr int ce_sector = 1;
inline constexpr int ce_surface = 2;
inline constexpr int ce_thing = 3;
inline constexpr int ce_cog = 4;
inline constexpr int js_ProjectDir = 1;
inline constexpr int js_JEDDir = 2;
inline constexpr int js_CDDir = 3;
inline constexpr int js_GameDir = 4;
inline constexpr int js_LevelFile = 5;
inline constexpr int js_jedregkey = 6;
inline constexpr int js_LECLogo = 7;
inline constexpr int js_recent1 = 8;
inline constexpr int js_recent2 = 9;
inline constexpr int js_recent3 = 10;
inline constexpr int js_recent4 = 11;
inline constexpr int js_res1gob = 12;
inline constexpr int js_res2gob = 13;
inline constexpr int js_spgob = 14;
inline constexpr int js_mp1gob = 15;
inline constexpr int js_mp2gob = 16;
inline constexpr int js_mp3gob = 17;
inline constexpr int jw_Main = 0;
inline constexpr int jw_ConsChecker = 1;
inline constexpr int jw_ItemEdit = 2;
inline constexpr int jw_PlacedCogs = 3;
inline constexpr int jw_MsgWindow = 4;
inline constexpr int jw_3DPreview = 5;
inline constexpr int jw_ToolWindow = 6;

// Records use the original packing: Light Baker's SDK header uses
// pack(4) and the Pascal SDK uses {$ALIGN 1}; both produce the same field
// offsets for these records on 32-bit and 64-bit targets.
#pragma pack(push, 4)

struct JedVector
{
    double x;
    double y;
    double z;
};

struct JedBox
{
    double x1;
    double y1;
    double z1;
    double x2;
    double y2;
    double z2;
};

struct JedLevelHeader
{
    JedLong version;
    double gravity;
    double ceilingSkyZ;
    double ceilingSkyOffs[2];
    double horDistance;
    double horPixelsPerRev;
    double horSkyOffs[2];
    double mipmapDist[4];
    double lodDist[4];
    double perspDist;
    double gouraudDist;
    double pixelPerUnit;
    char* masterCmp;
};

struct JedSectorRec
{
    JedLong flags;
    double ambient;
    double extra;
    char* colormap;
    float tintR;
    float tintG;
    float tintB;
    char* sound;
    double soundVolume;
    JedLong layer;
};

struct JedSurfaceRec
{
    JedLong adjoinSc;
    JedLong adjoinSf;
    JedLong adjoinFlags;
    JedLong surfFlags;
    JedLong faceFlags;
    char* material;
    JedLong geo;
    JedLong light;
    JedLong tex;
    double extraLight;
    float uScale;
    float vScale;
};

struct JedThingRec
{
    char* name;
    JedLong sector;
    double x;
    double y;
    double z;
    double pch;
    double yaw;
    double rol;
    JedLong layer;
};

struct JedLightRec
{
    double x;
    double y;
    double z;
    double intensity;
    double range;
    float r;
    float g;
    float b;
    float rgbIntensity;
    double rgbRange;
    JedLong flags;
    JedLong layer;
};

#pragma pack(pop)

inline constexpr std::size_t kJedPointerSize = sizeof(void*);
static_assert(offsetof(JedLevelHeader, masterCmp) == 156);
static_assert(offsetof(JedSectorRec, colormap) == 20);
static_assert(offsetof(JedSectorRec, sound) == 36 + kJedPointerSize - 4);
static_assert(offsetof(JedSurfaceRec, material) == 20);
static_assert(offsetof(JedSurfaceRec, extraLight) == 32 + kJedPointerSize);
static_assert(offsetof(JedThingRec, layer) == 52 + kJedPointerSize);
static_assert(offsetof(JedLightRec, rgbRange) == 56);
static_assert(sizeof(JedLightRec) == 72);

/** Wireframe renderer interface; SED does not provide one. */
class IJEDWFRenderer;

class IJEDLevel
{
public:
    virtual JedResult SED_JED_CALL QueryInterface(const void* iid, void** object) = 0;
    virtual JedLong SED_JED_CALL AddRef() = 0;
    virtual JedLong SED_JED_CALL Release() = 0;
    virtual JedResult SED_JED_CALL GetLevelHeader(JedLevelHeader* lh, int flags) = 0;
    virtual JedResult SED_JED_CALL SetLevelHeader(JedLevelHeader* lh, int flags) = 0;
    virtual int SED_JED_CALL NSectors() = 0;
    virtual int SED_JED_CALL NThings() = 0;
    virtual int SED_JED_CALL NLights() = 0;
    virtual int SED_JED_CALL NCOgs() = 0;
    virtual int SED_JED_CALL AddSector() = 0;
    virtual JedResult SED_JED_CALL DeleteSector(int n) = 0;
    virtual JedResult SED_JED_CALL GetSector(int sec, JedSectorRec* rec, int flags) = 0;
    virtual JedResult SED_JED_CALL SetSector(int sec, JedSectorRec* rec, int flags) = 0;
    virtual int SED_JED_CALL SectorNVertices(int sec) = 0;
    virtual int SED_JED_CALL SectorNSurfaces(int sec) = 0;
    virtual JedResult SED_JED_CALL SectorGetVertex(int sec, int vx, double* x, double* y, double* z) = 0;
    virtual JedResult SED_JED_CALL SectorSetVertex(int sec, int vx, double x, double y, double z) = 0;
    virtual int SED_JED_CALL SectorAddVertex(int sec, double x, double y, double z) = 0;
    virtual int SED_JED_CALL SectorFindVertex(int sec, double x, double y, double z) = 0;
    virtual int SED_JED_CALL SectorDeleteVertex(int sec, int n) = 0;
    virtual int SED_JED_CALL SectorAddSurface(int sec) = 0;
    virtual JedResult SED_JED_CALL SectorDeleteSurface(int sc, int sf) = 0;
    virtual JedResult SED_JED_CALL SectorUpdate(int sec) = 0;
    virtual JedResult SED_JED_CALL GetSurface(int sc, int sf, JedSurfaceRec* rec, int flags) = 0;
    virtual JedResult SED_JED_CALL SetSurface(int sc, int sf, JedSurfaceRec* rec, int flags) = 0;
    virtual JedResult SED_JED_CALL GetSurfaceNormal(int sc, int sf, JedVector* n) = 0;
    virtual JedResult SED_JED_CALL SurfaceUpdate(int sc, int sf, int how) = 0;
    virtual int SED_JED_CALL SurfaceNVertices(int sc, int sf) = 0;
    virtual int SED_JED_CALL SurfaceGetVertexNum(int sc, int sf, int vx) = 0;
    virtual JedResult SED_JED_CALL SurfaceSetVertexNum(int sc, int sf, int vx, int secvx) = 0;
    virtual int SED_JED_CALL SurfaceAddVertex(int sc, int sf, int secvx) = 0;
    virtual int SED_JED_CALL SurfaceInsertVertex(int sc, int sf, int at, int secvx) = 0;
    virtual JedResult SED_JED_CALL SurfaceDeleteVertex(int sc, int sf, int n) = 0;
    virtual JedResult SED_JED_CALL SurfaceGetVertexUV(int sc, int sf, int vx, float* u, float* v) = 0;
    virtual JedResult SED_JED_CALL SurfaceSetVertexUV(int sc, int sf, int vx, float u, float v) = 0;
    virtual JedResult SED_JED_CALL SurfaceGetVertexLight(int sc, int sf, int vx, float* white, float* r, float* g, float* b) = 0;
    virtual JedResult SED_JED_CALL SurfaceSetVertexLight(int sc, int sf, int vx, float white, float r, float g, float b) = 0;
    virtual int SED_JED_CALL AddThing() = 0;
    virtual JedResult SED_JED_CALL DeleteThing(int th) = 0;
    virtual JedResult SED_JED_CALL GetThing(int th, JedThingRec* rec, int flags) = 0;
    virtual JedResult SED_JED_CALL SetThing(int th, JedThingRec* rec, int flags) = 0;
    virtual JedResult SED_JED_CALL ThingUpdate(int th) = 0;
    virtual int SED_JED_CALL AddLight() = 0;
    virtual JedResult SED_JED_CALL DeleteLight(int lt) = 0;
    virtual JedResult SED_JED_CALL GetLight(int lt, JedLightRec* rec, int flags) = 0;
    virtual JedResult SED_JED_CALL SetLight(int lt, JedLightRec* rec, int flags) = 0;
    virtual int SED_JED_CALL NLayers() = 0;
    virtual char* SED_JED_CALL GetLayerName(int n) = 0;
    virtual int SED_JED_CALL AddLayer(char* name) = 0;
    virtual int SED_JED_CALL NTHingValues(int th) = 0;
    virtual char* SED_JED_CALL GetThingValueName(int th, int n) = 0;
    virtual char* SED_JED_CALL GetThingValueData(int th, int n) = 0;
    virtual JedResult SED_JED_CALL SetThingValueData(int th, int n, char* val) = 0;
    virtual JedResult SED_JED_CALL GetThingFrame(int th, int n, double* x, double* y, double* z, double* pch, double* yaw, double* rol) = 0;
    virtual JedResult SED_JED_CALL SetThingFrame(int th, int n, double x, double y, double z, double pch, double yaw, double rol) = 0;
    virtual int SED_JED_CALL AddThingValue(int th, char* name, char* val) = 0;
    virtual JedResult SED_JED_CALL InsertThingValue(int th, int n, char* name, char* val) = 0;
    virtual JedResult SED_JED_CALL DeleteThingValue(int th, int n) = 0;
    virtual int SED_JED_CALL AddCOG(char* name) = 0;
    virtual JedResult SED_JED_CALL DeleteCOG(int n) = 0;
    virtual JedResult SED_JED_CALL UpdateCOG(int cg) = 0;
    virtual char* SED_JED_CALL GetCOGFileName(int cg) = 0;
    virtual int SED_JED_CALL NCOGValues(int cg) = 0;
    virtual char* SED_JED_CALL GetCOGValueName(int cg, int n) = 0;
    virtual int SED_JED_CALL GetCOGValueType(int cg, int n) = 0;
    virtual char* SED_JED_CALL GetCOGValue(int cg, int n) = 0;
    virtual bool SED_JED_CALL SetCOGValue(int cg, int n, char* val) = 0;
    virtual int SED_JED_CALL AddCOGValue(int cg, char* name, char* val, int vtype) = 0;
    virtual JedResult SED_JED_CALL InsertCOGValue(int cg, int n, char* name, char* val, int vtype) = 0;
    virtual JedResult SED_JED_CALL DeleteCOGValue(int cg, int n) = 0;

protected:
    ~IJEDLevel() = default;
};

class IJED
{
public:
    virtual JedResult SED_JED_CALL QueryInterface(const void* iid, void** object) = 0;
    virtual JedLong SED_JED_CALL AddRef() = 0;
    virtual JedLong SED_JED_CALL Release() = 0;
    virtual double SED_JED_CALL GetVersion() = 0;
    virtual IJEDLevel* SED_JED_CALL GetLevel() = 0;
    virtual int SED_JED_CALL GetMapMode() = 0;
    virtual JedResult SED_JED_CALL SetMapMode(int mode) = 0;
    virtual int SED_JED_CALL GetCurSC() = 0;
    virtual JedResult SED_JED_CALL SetCurSC(int sc) = 0;
    virtual int SED_JED_CALL GetCurTH() = 0;
    virtual JedResult SED_JED_CALL SetCurTH(int th) = 0;
    virtual int SED_JED_CALL GetCurLT() = 0;
    virtual JedResult SED_JED_CALL SetCurLT(int lt) = 0;
    virtual JedResult SED_JED_CALL GetCurVX(int* sc, int* vx) = 0;
    virtual JedResult SED_JED_CALL SetCurVX(int sc, int vx) = 0;
    virtual JedResult SED_JED_CALL GetCurSF(int* sc, int* sf) = 0;
    virtual JedResult SED_JED_CALL SetCurSF(int sc, int sf) = 0;
    virtual JedResult SED_JED_CALL GetCurED(int* sc, int* sf, int* ed) = 0;
    virtual JedResult SED_JED_CALL SetCurED(int sc, int sf, int ed) = 0;
    virtual JedResult SED_JED_CALL GetCurFR(int* th, int* fr) = 0;
    virtual JedResult SED_JED_CALL SetCurFR(int th, int fr) = 0;
    virtual JedResult SED_JED_CALL NewLevel(bool mots) = 0;
    virtual JedResult SED_JED_CALL LoadLevel(char* name) = 0;
    virtual JedResult SED_JED_CALL FindBBox(int sec, JedBox* box) = 0;
    virtual JedResult SED_JED_CALL FindBoundingSphere(int sec, double* cX, double* cY, double* cZ, double* radius) = 0;
    virtual bool SED_JED_CALL FindCollideBox(int sec, JedBox* bbox, double cx, double cy, double cz, JedBox* cbox) = 0;
    virtual JedResult SED_JED_CALL FindSurfaceCenter(int sc, int sf, double* cx, double* cy, double* cz) = 0;
    virtual JedResult SED_JED_CALL RotateVector(JedVector* vec, double pch, double yaw, double rol) = 0;
    virtual JedResult SED_JED_CALL RotatePoint(double ax1, double ay1, double az1, double ax2, double ay2, double az2, double angle, double* x, double* y, double* z) = 0;
    virtual JedResult SED_JED_CALL GetJKPYR(JedVector* x, JedVector* y, JedVector* z, double* pch, double* yaw, double* rol) = 0;
    virtual bool SED_JED_CALL IsSurfaceConvex(int sc, int sf) = 0;
    virtual bool SED_JED_CALL IsSurfacePlanar(int sc, int sf) = 0;
    virtual bool SED_JED_CALL IsSectorConvex(int sec) = 0;
    virtual bool SED_JED_CALL IsInSector(int sec, double x, double y, double z) = 0;
    virtual bool SED_JED_CALL DoSectorsOverlap(int sec1, int sec2) = 0;
    virtual bool SED_JED_CALL IsPointOnSurface(int sc, int sf, double x, double y, double z) = 0;
    virtual int SED_JED_CALL FindSectorForThing(int th) = 0;
    virtual int SED_JED_CALL FindSectorForXYZ(double x, double y, double z) = 0;
    virtual int SED_JED_CALL ExtrudeSurface(int sc, int sf, double by) = 0;
    virtual int SED_JED_CALL CleaveSurface(int sc, int sf, JedVector* cnormal, double cx, double cy, double cz) = 0;
    virtual int SED_JED_CALL CleaveSector(int sec, JedVector* cnormal, double cx, double cy, double cz) = 0;
    virtual bool SED_JED_CALL CleaveEdge(int sc, int sf, int ed, JedVector* cnormal, double cx, double cy, double cz) = 0;
    virtual bool SED_JED_CALL JoinSurfaces(int sc1, int sf1, int sc2, int sf2) = 0;
    virtual bool SED_JED_CALL PlanarizeSurface(int sc, int sf) = 0;
    virtual int SED_JED_CALL MergeSurfaces(int sc, int sf1, int sf2) = 0;
    virtual int SED_JED_CALL MergeSectors(int sec1, int sec2) = 0;
    virtual JedResult SED_JED_CALL CalculateDefaultUVNormals(int sc, int sf, int orgvx, JedVector* un, JedVector* vn) = 0;
    virtual JedResult SED_JED_CALL CalcUVNormals(int sc, int sf, JedVector* un, JedVector* vn) = 0;
    virtual JedResult SED_JED_CALL ArrangeTexture(int sc, int sf, int orgvx, JedVector* un, JedVector* vn) = 0;
    virtual JedResult SED_JED_CALL ArrangeTextureBy(int sc, int sf, JedVector* un, JedVector* vn, double refx, double refy, double refz, double refu, double refv) = 0;
    virtual bool SED_JED_CALL IsTextureFlipped(int sc, int sf) = 0;
    virtual JedResult SED_JED_CALL RemoveSurfaceReferences(int sc, int sf) = 0;
    virtual JedResult SED_JED_CALL RemoveSectorReferences(int sec, bool surfs) = 0;
    virtual bool SED_JED_CALL StitchSurfaces(int sc1, int sf1, int sc2, int sf2) = 0;
    virtual bool SED_JED_CALL FindCommonEdges(int sc1, int sf1, int sc2, int sf2, int* v11, int* v12, int* v21, int* v22) = 0;
    virtual bool SED_JED_CALL DoSurfacesOverlap(int sc1, int sf1, int sc2, int sf2) = 0;
    virtual bool SED_JED_CALL MakeAdjoin(int sc, int sf) = 0;
    virtual bool SED_JED_CALL MakeAdjoinFromSectorUp(int sc, int sf, int firstsc) = 0;
    virtual bool SED_JED_CALL UnAdjoin(int sc, int sf) = 0;
    virtual int SED_JED_CALL CreateCubicSector(double x, double y, double z, JedVector* pnormal, JedVector* edge) = 0;
    virtual JedResult SED_JED_CALL StartUndo(char* name) = 0;
    virtual JedResult SED_JED_CALL SaveUndoForThing(int n, int change) = 0;
    virtual JedResult SED_JED_CALL SaveUndoForLight(int n, int change) = 0;
    virtual JedResult SED_JED_CALL SaveUndoForSector(int n, int change, int whatpart) = 0;
    virtual JedResult SED_JED_CALL ClearUndoBuffer() = 0;
    virtual JedResult SED_JED_CALL ApplyUndo() = 0;
    virtual int SED_JED_CALL GetApplicationHandle() = 0;
    virtual bool SED_JED_CALL JoinSectors(int sec1, int sec2) = 0;
    virtual int SED_JED_CALL GetNMultiselected(int what) = 0;
    virtual JedResult SED_JED_CALL ClearMultiselection(int what) = 0;
    virtual JedResult SED_JED_CALL RemoveFromMultiselection(int what, int n) = 0;
    virtual int SED_JED_CALL GetSelectedSC(int n) = 0;
    virtual int SED_JED_CALL GetSelectedTH(int n) = 0;
    virtual int SED_JED_CALL GetSelectedLT(int n) = 0;
    virtual JedResult SED_JED_CALL GetSelectedSF(int n, int* sc, int* sf) = 0;
    virtual JedResult SED_JED_CALL GetSelectedED(int n, int* sc, int* sf, int* ed) = 0;
    virtual JedResult SED_JED_CALL GetSelectedVX(int n, int* sc, int* vx) = 0;
    virtual JedResult SED_JED_CALL GetSelectedFR(int n, int* th, int* fr) = 0;
    virtual int SED_JED_CALL SelectSC(int sc) = 0;
    virtual int SED_JED_CALL SelectSF(int sc, int sf) = 0;
    virtual int SED_JED_CALL SelectED(int sc, int sf, int ed) = 0;
    virtual int SED_JED_CALL SelectVX(int sc, int vx) = 0;
    virtual int SED_JED_CALL SelectTH(int th) = 0;
    virtual int SED_JED_CALL SelectFR(int th, int fr) = 0;
    virtual int SED_JED_CALL SelectLT(int lt) = 0;
    virtual int SED_JED_CALL FindSelectedSC(int sc) = 0;
    virtual int SED_JED_CALL FindSelectedSF(int sc, int sf) = 0;
    virtual int SED_JED_CALL FindSelectedED(int sc, int sf, int ed) = 0;
    virtual int SED_JED_CALL FindSelectedVX(int sc, int vx) = 0;
    virtual int SED_JED_CALL FindSelectedTH(int th) = 0;
    virtual int SED_JED_CALL FindSelectedFR(int th, int fr) = 0;
    virtual int SED_JED_CALL FindSelectedLT(int lt) = 0;
    virtual JedResult SED_JED_CALL SaveJED(char* name) = 0;
    virtual JedResult SED_JED_CALL SaveJKL(char* name) = 0;
    virtual JedResult SED_JED_CALL UpdateMap() = 0;
    virtual JedResult SED_JED_CALL SetPickerCMP(char* cmp) = 0;
    virtual char* SED_JED_CALL PickResource(int what, char* cur) = 0;
    virtual JedLong SED_JED_CALL EditFlags(int what, JedLong flags) = 0;
    virtual JedResult SED_JED_CALL ReloadTemplates() = 0;
    virtual JedResult SED_JED_CALL PanMessage(int mtype, char* msg) = 0;
    virtual JedResult SED_JED_CALL SendKey(int shift, int key) = 0;
    virtual bool SED_JED_CALL ExecuteMenu(char* itemref) = 0;
    virtual void* SED_JED_CALL GetJEDSetting(char* name) = 0;
    virtual bool SED_JED_CALL IsLayerVisible(int n) = 0;
    virtual JedResult SED_JED_CALL CheckConsistencyErrors() = 0;
    virtual JedResult SED_JED_CALL CheckResources() = 0;
    virtual int SED_JED_CALL NConsistencyErrors() = 0;
    virtual char* SED_JED_CALL GetConsErrorString(int n) = 0;
    virtual int SED_JED_CALL GetConsErrorType(int n) = 0;
    virtual int SED_JED_CALL GetConsErrorSector(int n) = 0;
    virtual int SED_JED_CALL GetConsErrorSurface(int n) = 0;
    virtual int SED_JED_CALL GetConsErrorThing(int n) = 0;
    virtual int SED_JED_CALL GetConsErrorCog(int n) = 0;
    virtual bool SED_JED_CALL IsPreviewActive() = 0;
    virtual char* SED_JED_CALL GetJEDString(int what) = 0;
    virtual bool SED_JED_CALL GetIsMOTS() = 0;
    virtual JedResult SED_JED_CALL SetIsMOTS(bool mots) = 0;
    virtual int SED_JED_CALL GetJedWindow(int whichone) = 0;
    virtual char* SED_JED_CALL FileExtractExt(char* path) = 0;
    virtual char* SED_JED_CALL FileExtractPath(char* path) = 0;
    virtual char* SED_JED_CALL FileExtractName(char* path) = 0;
    virtual char* SED_JED_CALL FindProjectDirFile(char* name) = 0;
    virtual bool SED_JED_CALL IsFileContainer(char* path) = 0;
    virtual bool SED_JED_CALL IsFileInContainer(char* path) = 0;
    virtual char* SED_JED_CALL FileOpenDialog(char* fname, char* filter) = 0;
    virtual int SED_JED_CALL OpenFile(char* name) = 0;
    virtual int SED_JED_CALL OpenGameFile(char* name) = 0;
    virtual int SED_JED_CALL ReadFile(int handle, void* buf, JedLong size) = 0;
    virtual JedResult SED_JED_CALL SetFilePos(int handle, JedLong pos) = 0;
    virtual JedLong SED_JED_CALL GetFilePos(int handle) = 0;
    virtual JedLong SED_JED_CALL GetFileSize(int handle) = 0;
    virtual char* SED_JED_CALL GetFileName(int handle) = 0;
    virtual JedResult SED_JED_CALL CloseFile(int handle) = 0;
    virtual int SED_JED_CALL OpenTextFile(char* name) = 0;
    virtual int SED_JED_CALL OpenGameTextFile(char* name) = 0;
    virtual char* SED_JED_CALL GetTextFileName(int handle) = 0;
    virtual char* SED_JED_CALL ReadTextFileString(int handle) = 0;
    virtual bool SED_JED_CALL TextFileEof(int handle) = 0;
    virtual int SED_JED_CALL TextFileCurrentLine(int handle) = 0;
    virtual JedResult SED_JED_CALL CloseTextFile(int handle) = 0;
    virtual int SED_JED_CALL OpenGOB(char* name) = 0;
    virtual int SED_JED_CALL GOBNFiles(int handle) = 0;
    virtual char* SED_JED_CALL GOBNFileName(int handle, int n) = 0;
    virtual char* SED_JED_CALL GOBNFullFileName(int handle, int n) = 0;
    virtual JedLong SED_JED_CALL GOBGetFileSize(int handle, int n) = 0;
    virtual JedLong SED_JED_CALL GOBGetFileOffset(int handle, int n) = 0;
    virtual JedResult SED_JED_CALL CloseGOB(int handle) = 0;
    virtual IJEDWFRenderer* SED_JED_CALL CreateWFRenderer(int wnd, int whichone) = 0;

protected:
    ~IJED() = default;
};

/** Exported plug-in entry points, tried in this order (SED's own ISED
 * entry points are a different interface and are reported as unsupported). */
inline constexpr const char* kJedPluginLoadStdCallSymbol = "JEDPluginLoadStdCall";
inline constexpr const char* kJedPluginLoadSymbol = "JEDPluginLoad";
inline constexpr const char* kSedPluginLoadStdCallSymbol = "SEDPluginLoadStdCall";
inline constexpr const char* kSedPluginLoadSymbol = "SEDPluginLoad";

using JedPluginLoadStdCall = bool(SED_JED_CALL*)(IJED* jed);
// A plain Win32 export uses Delphi's register convention, while a C pointer
// would pass the IJED argument on the stack. Only call the unambiguous stdcall
// entry on Win32 x86. The conventions converge on Win64.
#if defined(_WIN32) && (defined(_M_IX86) || defined(__i386__))
inline constexpr bool kSupportsPlainEntry = false;
#else
inline constexpr bool kSupportsPlainEntry = true;
#endif
using JedPluginLoad = bool (*)(IJED* jed);

} // namespace sed::plugins::jed
