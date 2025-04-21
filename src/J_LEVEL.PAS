unit J_Level;
{ This unit defines the classes for JK
  level }

interface

uses Classes, Files, Tools, SysUtils, misc_utils,
  GlobalVars, Geometry, FileOperations, Values, U_3dos, StrUtils,
  u_templates, Generics.Collections, System.Types;

const
  // Sector flags
  SECF_Underwater   = $2;         // sector underwater
  SECF_CollideBox   = $1000;      // sector uses collide box info

  // IJIM Sector flags
  SECF_IJIM_UseThrust = $8;      // sector uses thrust field
  SECF_IJIM_Aetherim  = $100;    // sector is in aetherium

  // SED sector flags (not used in the game)
  SECF_NORGBAMBINETLIGHT = 1 shl 29;   // marks sector to use use only light intensity for ambient light when calculating lights
  SECF_NOAMBINETLIGHT    = 1 shl 30;   // marks sector to not calculate ambient light when calculating lights
  SECF_3DO               = 1 shl 31;   // marks 3DO sec mesh

  // Surface flags
  SF_Floor        = $1;
  SF_CogLinked    = $2;
  SF_Collision    = $4;
  SF_DoubleRes    = $10;      // Used by SlideWall cog function
  SF_HalfRes      = $20;      // Used by SlideWall cog function
  SF_EighthRes    = $40;      // Used by SlideWall cog function
  SF_SkyHorizon   = $200;
  SF_SkyCeiling   = $400;
  SF_Water        = $20000;   // Surface is part of underwater sector
  SF_QuaterRes    = $4000000; // Used by SlideWall cog function
  SF_QuadrupleRes = $8000000; // Used by SlideWall cog function

  // IJIM Surface flags
  SF_IJIM_Aetherim   = $80;       // Surface is part of aetherium sector
  SF_IJIM_KillFloor  = $1000;     // Player dies if it touches this surface
  SF_IJIM_Climable   = $2000;     // Climable surface
  SF_IJIM_Track      = $4000;     // Railroad track
  SF_IJIM_Ledge      = $1000000;  // Player can hang on this surface
  SF_IJIM_WaterLedge = $2000000;  // Water climb out ledge
  SF_IJIM_WhipAim    = $10000000; // Player starts aiming with whip on this surface

  // Surface Adjoin flags
  SAF_Visible        = $01;
  SAF_Move           = $02;       // Allow movement trough the adjoin
  SAF_AllowSoundPass = $04;       // JKDF specific
  SAF_NoAiMove       = $08;       // Only player can move through adjoin
  SAF_NoPlayerMove   = $10;       // Don't allow player to move throug adjoin
  SAF_BlockLight     = LongInt($80000000); // SED specifc flag to block ligh pass through adjoin to another sector.

  // Light flags
  LF_NoBlock = 1;

  TX_DFLT = 0;
  TX_KEEP = 1;

  // Thing flags
  TF_INVISIBLE= $10;    // thing is invisible
  TF_NOEASY   = $1000;  // Object will not appear in "EASY" mode (DF)
  TF_NOMEDIUM = $2000;  // Object will not appear in "MEDIUM" mode (DF)
  TF_NOHARD   = $4000;  // Object will not appear in "HARD" mode (DF)
  TF_NOMULTI  = $8000;  // Object will not appear in Multiplyer modes (DF)
  TF_DISABLED = $80000; // thing is disabled and not visible

type

  TJKLevel = class;

  TJKSector = class;

  TJKSurface = class;

  TJKVertex = class(TVertex)
    Sector: TJKSector; { Owning sector }
    Procedure Assign(v: TJKVertex);
    Function GetUV(): TTXVertex;
  end;

  TSedLight = class
    ID: integer; { Unique ID. Used in Undo }
    flags: longint;
    mark: integer;
    layer: integer;

    color: TColorF;
    intensity: double;
    range: double;

    position: TVector;

    Constructor Create;
    Procedure Assign(l: TSedLight);
  end;

  TSedLights = class(TList)
    Function GetItem(n: integer): TSedLight;
    Procedure SetItem(n: integer; v: TSedLight);
    Property Items[n: integer]: TSedLight read GetItem write SetItem; default;
    Function Last: TSedLight;
  end;

  TJKVertices = class(TVertices)
    Function GetItem(n: integer): TJKVertex;
    Procedure SetItem(n: integer; v: TJKVertex);
    Property Items[n: integer]: TJKVertex read GetItem write SetItem; default;
  end;

  TJKLine = class
    v1, v2: TJKVertex;
  end;

  TSectors = class(TList)
    Function GetItem(n: integer): TJKSector;
    Procedure SetItem(n: integer; v: TJKSector);
    Property Items[n: integer]: TJKSector read GetItem write SetItem; default;
    Function Last: TJKSector;
  end;

  TThing = class
    private
      Function GetFlags: longint;
      Function GetLight: TColorF;
    public

      ID: integer; { Unique ID. Used in Undo }

      level: TJKLevel;
      num: integer;
      Sec: TJKSector;
      name: String;
      x, y, z: double;
      pch, yaw, rol: double;
      vals: TTPLValues;
      layer: integer;
      a3DO: T3DO;
      bbox: TThingBox;

      Property flags: longint read GetFlags;
      Property light: TColorF read GetLight;

      Constructor Create;
      Destructor Destroy; override;
      Procedure Assign(thing: TThing);
      Function AddValue(const name, s: string): TTplValue;
      Function InsertValue(n: integer; const name, s: string): TTplValue;
      Function NextFrame(n: integer): integer;
      Function PrevFrame(n: integer): integer;
      Function Nframes: integer;
  end;

  TJKThing = TThing;

  TThings = class(TList)
    Function Last: TThing;
    Function GetItem(n: integer): TThing;
    Procedure SetItem(n: integer; v: TThing);
    Property Items[n: integer]: TThing read GetItem write SetItem; default;
  end;

  TJKSurface = class(TPolygon)
    surfflags: longint;
    adjoin: TJKSurface;
    adjoinflags: longint;
    material: String;
    sector: TJKSector;
//    geo, light, tex: integer;
//    ExtraLight: TColorF;
    mark, D3DID: integer;
    num: integer;
    nadj, nmat: integer;
    uscale, vscale: Single;

    { ucosa,usina:double; }

    Function GetVXs: TJKVertices;
    Procedure SetVXs(vxs: TJKVertices);
    Property Vertices: TJKVertices read GetVXs write SetVXs;
    Constructor Create(owner: TJKSector);
    procedure Assign(surf: TJKSurface);
    Procedure NewRecalcAll;
    Procedure RecalcAll;
    Procedure CheckIfFloor;
    Function GetRefVector(var v: TVector): boolean;
    Function GetMaterialSize: TSize;
  end;

  TSurfaces = class(TPolygons)
    Function GetItem(n: integer): TJKSurface;
    Procedure SetItem(n: integer; v: TJKSurface);
    Property Items[n: integer]: TJKSurface read GetItem write SetItem; default;
  end;

  TPointLight = record
    color: TColorF;
    position: TVector;
    minRange: double;  // minimum light range at which light will still illuminate
    maxRange: double;  // maximum light intensity range before it starts to falloff
  end;

  TJKSector = class { (TPolygons) }
    ID: integer; { Unique ID. Used in Undo }
    num: integer;
    flags: longint;
    ambient: TColorF;
    extraLight: TColorF;
    pointLight: TPointLight;
    colormap: String;
    tint: TColorF;
    sound: string;
    soundVolume: double;
    thrust: TVector;

    level: TJKLevel;
    surfaces: TSurfaces;
    vertices: TJKVertices;
    collideBox: TBox;
    layer: integer;
    mark: integer;

    Procedure Assign(Sec: TJKSector);
    Constructor Create(owner: TJKLevel);
    Destructor Destroy; override;
    Function NewVertex: TJKVertex;
    Function NewSurface: TJKSurface;
    Procedure Renumber;
    Function FindVX(X, Y, Z: double): integer;
    Function AddVertex(X, Y, Z: double): TJKVertex;
  end;

  TCOG = class
    name: string;
    vals: TCOGValues;
    Constructor Create;
    Destructor Destroy; override;
    Procedure GetValues;
  end;

  TCOGs = class(TList)
    Function GetItem(n: integer): TCOG;
    Procedure SetItem(n: integer; v: TCOG);
    Property Items[n: integer]: TCOG read GetItem write SetItem; default;
  end;

  TFog = record
    enabled: Boolean;
    color: TColorF;
    fogStart: double;
    fogEnd: double;
  end;

  TCeilingSky = record
    height: Float;
    offset: TPoint2;
  end;

  THorizonSky = record
    distance : Float;
    pixelsPerRev: Float;
    offset: TPoint2;
  end;

  TJKLHeader = record
    version: longint;
    gravity: float;
    horizonSky: THorizonSky;
    ceilingSky: TCeilingSky;
    mipmapDistances: array [1 .. 4] of float;
    lodDistances: array [1 .. 4] of float;
    perspectiveDistance,
    gouraudDistance: float;
    fog: TFog;
  end;

  TJKLevel = class
    header: TJKLHeader;
    kind: TProjectType;
    sectors: TSectors;
    cogs: TCOGs;
    templates: TTemplates;
    things: TThings;
    lights: TSedLights;
    layers: TStringList;
    h3donodes: THNodes;

    masterCMP: String;
    ppunit: double;

    lvisstring: string; { layer visibility string - in form 0101 }

    curID: integer;

    Constructor Create;
    Destructor Destroy; override;
    Procedure Clear;
    Procedure LoadFromJKL(filename: TFileName);
    Procedure JKLPostLoad;
    Procedure LoadFromJed(F: TFileName);
    Procedure SaveToJKL(F: TFileName);
    Procedure SaveToJed(F: TFileName);
    Procedure ImportLEV(F: TFileName; scfactor: double; txhow: integer);
    Procedure ImportAsc(F: TFileName);
    Procedure SetDefaultHeader;
    Procedure RenumSecs;
    Procedure RenumThings;

    Function NewSector: TJKSector;
    Function NewThing: TJKThing;
    Function NewLight: TSedLight;
    Function New3DONode: THNode;

    Function GetThingByID(ID: integer): integer;
    Function GetSectorByID(ID: integer): integer;
    Function GetLightByID(ID: integer): integer;
    Function GetNodeByID(ID: integer): integer;

    { for resolving references }
    Function GetSectorN(n: integer): TJKSector;
    Function GetSurfaceN(n: integer): TJKSurface;
    Function GetSectorSurfaceN(sc, sf: integer): TJKSurface;
    Function GetSectorVertexN(sc, vx: integer): TJKVertex;
    Function GetThingN(n: integer): TJKThing;
    Function GetLightN(n: integer): TSedLight;
    Function GetGlobalSFN(sc, sf: integer): integer;
    Function GetLayerName(n: integer): String;
    Function AddLayer(const name: string): integer;
    Procedure AddMissingLayers;
    Function GetMasterCMP: string;

    private
    Function GetNewID: Integer;
  end;

Var
  level: TJKLevel;
  IJIMStaticSoundList: TStringList; // wav list found in Jones3DStatic
  IJIMStaticMatList: TStringList;   // mat list found in Jones3DStatic
  IJIMStatic3doList: TStringList;   // 3do list found in Jones3DStatic
  IJIMStaticSprList: TStringList;   // sprites list found in Jones3DStatic
  IJIMStaticKeyList: TStringList;   // key list found in Jones3DStatic
  IJIMStaticCogScriptList: TStringList; // cog script list found in Jones3DStatic
  StaticTemplates:  array[0..3 - 1] of TArray<String>; // templates required by the game engine

Function SFtoInt(sc, sf: integer): integer;
Function EDtoInt(sc, sf, ed: integer): integer;
Function VXToInt(sc, vx: integer): integer;
Function FRToInt(th, fr: integer): integer;
Function GetsfSC(scsf: integer): integer;
Function GetsfSF(scsf: integer): integer;
Function GetedSC(scsfed: integer): integer;
Function GetedSF(scsfed: integer): integer;
Function GetedED(scsfed: integer): integer;
Function GetvxSC(scvx: integer): integer;
Function GetvxVX(scvx: integer): integer;
Function GetfrTH(thfr: integer): integer;
Function GetfrFR(thfr: integer): integer;

implementation

uses ProgressDialog, lev_utils, ListRes, U_CogForm,
  U_options, graph_files, System.Math;

{ Lights }

Constructor TSedLight.Create;
begin
  Range := 2;
  intensity := 10;
  color := ColorWhite;
end;

Procedure TSedLight.Assign(l: TSedLight);
begin
  flags     := l.flags;
  intensity := l.intensity;
  range     := l.range;
  color     := l.color;
  position  := l.position;
  layer     := l.layer;
end;

Function TSedLights.Last: TSedLight;
begin
   Result := TSedLight(inherited Last);
end;

Function TSedLights.GetItem(n: integer): TSedLight;
begin
  if (n < 0) or (n >= count) then
    raise EListError.CreateFmt('Light Index is out of bounds: %d', [n]);
  Result := TSedLight(List[n]);
end;

Procedure TSedLights.SetItem(n: integer; v: TSedLight);
begin
  if (n < 0) or (n >= count) then
    raise EListError.CreateFmt('Light Index is out of bounds: %d', [n]);
  List[n] := v;
end;

{ Vertices }

Procedure TJKVertex.Assign(v: TJKVertex);
begin
  x := v.x;
  y := v.y;
  z := v.z;
end;

Function TJKVertex.GetUV(): TTXVertex;
begin
  if Sector = nil then exit;
  for var i := 0 to Sector.surfaces.Count - 1 do
    begin
      var surf := sector.surfaces[i];
      for var j := 0 to surf.vertices.Count - 1 do
        begin
           var v := surf.vertices[j];
           if IsClose(v.x, x) and IsClose(v.y, y) and IsClose(v.z, z) then
           begin
              Result := surf.txVertices[j];
              break;
           end
        end;
    end;
end;

Function TJKVertices.GetItem(n: integer): TJKVertex;
begin
  if (n < 0) or (n >= Count) then
    raise EListError.CreateFmt('Vertex Index is out of bounds: %d', [n]);
  Result := TJKVertex(List[n]);
end;

Procedure TJKVertices.SetItem(n: integer; v: TJKVertex);
begin
  if (n < 0) or (n >= Count) then
    raise EListError.CreateFmt('Vertex Index is out of bounds: %d', [n]);
  List[n] := v;
end;

Constructor TThing.Create;
begin
  vals := TTPLValues.Create;
  name := 'walkplayer';
end;

Destructor TThing.Destroy;
var
  i: integer;
begin
  For i := 0 to vals.count - 1 do
    vals[i].free;
  vals.free;
end;

Function TThing.InsertValue(n: integer; const name, s: string): TTplValue;
var
  v: TTplValue;
begin
  v := TTplValue.Create;
  v.name  := name;
  v.vtype := GetTPLVType(v.name);
  v.atype := GetTPLType(v.name);

  if not v.Val(s) then
   begin
     PanMessageFmt(mt_warning,'Thing ''%s'' parameter ''%s'' has invalid value: %s', [self.Name, v.Name, s]);
     // TODO: should assign string?
   end;

  vals.Insert(n, v);
  Result := v;
end;

Function TThing.AddValue(const name, s: string): TTplValue;
begin
  Result := InsertValue(vals.count, name, s);
end;

Function TThing.Nframes: integer;
var
  i: integer;
begin
  Result := 0;
  for i := 0 to vals.count - 1 do
    if CompareText(vals[i].name, 'frame') = 0 then
      Inc(Result);
end;

Function TThing.PrevFrame(n: integer): integer;
var
  v: TTplValue;
begin
  Result := -1;
  dec(n);
  while n >= 0 do
  begin
    if CompareText(vals[n].name, 'frame') = 0 then
    begin
      Result := n;
      exit;
    end;
    dec(n);
  end;
end;

Function TThing.NextFrame(n: integer): integer;
var
  v: TTplValue;
begin
  Result := -1;
  Inc(n);
  while n < vals.count do
  begin
    if CompareText(vals[n].name, 'frame') = 0 then
    begin
      Result := n;
      exit;
    end;
    Inc(n);
  end;
end;

Procedure TThing.Assign;
var
  i: integer;
  v: TTplValue;
begin
  name := thing.name;
  X    := thing.X;
  Y    := thing.Y;
  Z    := thing.Z;
  PCH  := thing.PCH;
  YAW  := thing.YAW;
  ROL  := thing.ROL;
  for i := 0 to thing.vals.count - 1 do
  begin
    v := TTplValue.Create;
    v.Assign(thing.vals[i]);
    vals.Add(v);
  end;
  layer := thing.layer;
end;

Function TThing.GetFlags: longint;
  var tfv: TTPLValue;
begin
  Result := 0;
  var i := vals.IndexOfName('thingflags');
  if i > -1 then
    tfv := vals[i]
  else
    tfv := Templates.GetTPLField(name, 'thingflags');

  if (tfv <> nil) and (tfv.vtype = vt_flag) then
    Result := tfv.int;
end;

Function TThing.GetLight: TColorF;
  var tfv: TTPLValue;
begin
  Result := ColorBlack;
  var i := vals.IndexOfName('light');
  if i > -1 then
    tfv := vals[i]
  else
    tfv := Templates.GetTPLField(name, 'light');

  if (tfv <> nil) then
    if (CurrentProject = IJIM) then
      ValColor(tfv.s, Result)
    else
      begin
        if tfv.vtype = vt_float then
          Result := IntensityToColor(tfv.float)
        else
          begin
            ValSingle(tfv.s, Result.a);
            Result := IntensityToColor(Result.a);
          end;
      end;
end;


{ TCog methods }

Constructor TCOG.Create;
begin
  vals := TCOGValues.Create;
end;

Destructor TCOG.Destroy;
begin
  vals.Free;
end;

Procedure TCOG.GetValues;
begin
end;

Function TCOGs.GetItem(n: integer): TCOG;
begin
  if (n < 0) or (n >= count) then
    raise EListError.CreateFmt('COG Index is out of bounds: %d', [n]);
  Result := TCOG(List[n]);
end;

Procedure TCOGs.SetItem(n: integer; v: TCOG);
begin
  if (n < 0) or (n >= count) then
    raise EListError.CreateFmt('COG Index is out of bounds: %d', [n]);
  List[n] := v;
end;

Function TSectors.GetItem(n: integer): TJKSector;
begin
  if (n < 0) or (n >= count) then
    raise EListError.CreateFmt('Sector Index is out of bounds: %d', [n]);
  Result := TJKSector(List[n]);
end;

Procedure TSectors.SetItem(n: integer; v: TJKSector);
begin
  if (n < 0) or (n >= count) then
    raise EListError.CreateFmt('Sector Index is out of bounds: %d', [n]);
  List[n] := v;
end;

Function TSectors.Last: TJKSector;
begin
   Result := TJKSector(inherited Last);
end;

Function TThings.Last: TThing;
begin
   Result := TThing(inherited Last);
end;

Function TThings.GetItem(n: integer): TThing;
begin
  if (n < 0) or (n >= Count) then
    raise EListError.CreateFmt('Thing Index is out of bounds: %d', [n]);
  Result := TThing(List[n]);
end;

Procedure TThings.SetItem(n: integer; v: TThing);
begin
  if (n < 0) or (n >= Count) then
    raise EListError.CreateFmt('Thing Index is out of bounds: %d', [n]);
  List[n] := v;
end;

Function TSurfaces.GetItem(n: integer): TJKSurface;
begin
  if (n < 0) or (n >= Count) then
    raise EListError.CreateFmt('Surface Index is out of bounds: %d', [n]);
  Result := TJKSurface(List[n]);
end;

Procedure TSurfaces.SetItem(n: integer; v: TJKSurface);
begin
  if (n < 0) or (n >= Count) then
    raise EListError.CreateFmt('Surface Index is out of bounds: %d', [n]);
  List[n] := v;
end;

{ TJKSurface Methods }

Constructor TJKSurface.Create;
begin
  Inherited Create;
  sector := owner;

  adjoinflags := SAF_Visible or SAF_Move;
  if CurrentProject <> IJIM then
    BitSet(adjoinflags, SAF_AllowSoundPass);

  material  := 'dflt.mat';
  surfflags := SF_Collision;
  faceFlags := 0;
  geo       := integer(Texture);
  light     := integer(Gouraud);

  tex := 1;
  if CurrentProject = IJIM then tex := 3;

  uscale := 1;
  vscale := 1;
end;

Procedure TJKSurface.CheckIfFloor;
var
  cosa: double;
begin
  cosa := normal.Dot(TVector.Up); //Smult(normal.dx, normal.dy, normal.dz, 0, 0, 1);
  if (cosa <= 1) and (cosa > cos(PI / 4)) then //Round(Math.RadToDeg(Math.ArcCos(cosa))) <= 45 then//(cosa > cos(PI / 4)) then
    surfflags := surfflags or SF_Floor or SF_Collision
  else
    surfflags := surfflags and (-1 xor SF_Floor);
end;

Function TJKSurface.GetRefVector(var v: TVector): boolean;
var
  v1, v2: TJKVertex;
begin
  Result := false;
  if vertices.Count < 2 then
    exit;
  v1 := vertices[0];
  v2 := vertices[1];
  v.SetCoords(v2.X - v1.X, v2.Y - v1.Y, v2.Z - v1.Z);
  Result := v.Normalize <> 0; //Normalize(v);
end;

Function TJKSurface.GetVXs: TJKVertices;
begin
  Result := TJKVertices(TPolygon(self).vertices);
end;

Procedure TJKSurface.SetVXs(vxs: TJKVertices);
begin
  TPolygon(self).vertices := vxs;
end;

procedure TJKSurface.Assign(surf: TJKSurface);
begin
  adjoinFlags := surf.adjoinFlags;
  material    := surf.material;
  surfflags   := surf.surfflags;
  faceflags   := surf.faceflags;
  geo         := surf.geo;
  light       := surf.light;
  tex         := surf.tex;
  extraLight  := surf.extraLight;
  uscale      := surf.uscale;
  vscale      := surf.vscale;
end;

Procedure TJKSurface.NewRecalcAll;
begin
  Recalc;
  CalcUV(self, 0);
  CheckIfFloor;
end;

Procedure TJKSurface.RecalcAll;
var
  un, vn: TVector;
begin
  Recalc;
  CalcUVNormals(self, un, vn);
  ArrangeTexture(self, 0, un, vn);
end;

Function TJKSurface.GetMaterialSize: TSize;

begin
  // Note, this will put lots of unneeded IO operation if querying is done for the same file multiple times
  // TODO: Optimize by loading into cache and attach mat cache to surface
  var mf: TMat := nil;
  Result := TSize.Create(0, 0);
  if not Material.IsEmpty then
    begin
      try
        mf := TMat.Create(OpenGameFile(Material), 0);
        Result.Width  := mf.Info.width;
        Result.Height := mf.Info.height;
      finally
        if mf <> nil then mf.Free; // closes also file
      end;
    end
end;

{ TJKSector Methods }

Constructor TJKSector.Create;
begin
  surfaces := TSurfaces.Create;
  vertices := TJKVertices.Create;
  surfaces.VXList := vertices;
  level := owner;

  if CurrentProject = IJIM then
    ambient := MakeColor(1, 1, 1, 1)
  else
    ambient := MakeColor(5, 5, 5, 5);

  ColorMap := 'dflt.cmp';
end;

Destructor TJKSector.Destroy;
var
  i: integer;
begin
  for i := 0 to surfaces.Count - 1 do
    surfaces[i].free;
  surfaces.free;
  for i := 0 to vertices.Count - 1 do
    vertices[i].Free;
  vertices.Free;
end;

Function TJKSector.NewVertex: TJKVertex;
begin
  Result := TJKVertex.Create;
  Result.sector := self;
  Result.num := vertices.Add(Result) - 1;
end;

Function TJKSector.NewSurface: TJKSurface;
begin
  Result := TJKSurface.Create(self);
  Result.extraLight := MakeColor(0.0, 0.0, 0.0, 1.0);
end;

Procedure TJKSector.Renumber;
var
  i: integer;
begin
  for i := 0 to vertices.Count - 1 do
    vertices[i].num := i;
  for i := 0 to surfaces.Count - 1 do
    surfaces[i].num := i;
end;

Function TJKSector.FindVX(X, Y, Z: double): integer;
var
  v: TJKVertex;
  i: integer;
begin
  Result := -1;
  for i := 0 to vertices.Count - 1 do
  begin
    v := vertices[i];
    if IsClose(v.X, X) and IsClose(v.Y, Y) and IsClose(v.Z, Z) then
    begin
      Result := i;
      break;
    end;
  end;
end;

Function TJKSector.AddVertex(X, Y, Z: double): TJKVertex;
var
  i: integer;
begin
  i := FindVX(X, Y, Z);
  if i <> -1 then
    Result := vertices[i]
  else
    begin
      Result   := NewVertex;
      Result.X := X;
      Result.Y := Y;
      Result.Z := Z;
    end;
end;

Procedure TJKSector.Assign(Sec: TJKSector);
begin
  flags       := Sec.flags;
  ambient     := Sec.Ambient;
  extraLight  := Sec.ExtraLight;
  pointLight  := Sec.PointLight;
  colorMap    := Sec.ColorMap;
  tint        := Sec.Tint;
  sound       := Sec.Sound;
  soundVolume := Sec.soundVolume;
  thrust      := Sec.Thrust;
  collideBox  := Sec.CollideBox;
  layer       := Sec.layer;
end;

{ TJKLevel Methods }

Function TJKLevel.GetNewID: Integer;
begin
  if self = nil then
  begin
    Result := 0;
    exit;
  end;
  Result := CurID;
  Inc(CurID);
end;

Function TJKLevel.GetThingByID(ID: integer): integer;
var
  i: integer;
begin
  Result := -1;
  for i := 0 to things.Count - 1 do
    If things[i].ID = ID then
    begin
      Result := i;
      exit;
    end;
end;

Function TJKLevel.GetSectorByID(ID: integer): integer;
var
  i: integer;
begin
  Result := -1;
  for i := 0 to sectors.Count - 1 do
    If sectors[i].ID = ID then
    begin
      Result := i;
      exit;
    end;
end;

Function TJKLevel.GetLightByID(ID: integer): integer;
var
  i: integer;
begin
  Result := -1;
  for i := 0 to lights.Count - 1 do
    If lights[i].ID = ID then
    begin
      Result := i;
      exit;
    end;
end;

Function TJKLevel.GetNodeByID(ID: integer): integer;
var
  i: integer;
begin
  Result := -1;
  for i := 0 to h3donodes.Count - 1 do
    If h3donodes[i].ID = ID then
    begin
      Result := i;
      exit;
    end;
end;

Function TJKLevel.NewSector: TJKSector;
begin
  Result := TJKSector.Create(self);
  Result.ID := GetNewID;
end;

Function TJKLevel.NewThing: TThing;
begin
  Result := TThing.Create { (self) };
  Result.level := self;
  Result.ID := GetNewID;
end;

Function TJKLevel.NewLight: TSedLight;
begin
  Result := TSedLight.Create;
  Result.ID := GetNewID;
end;

Function TJKLevel.New3DONode: THNode;
begin
  Result := THNode.Create;
  Result.ID := GetNewID;
end;

Procedure TJKLevel.RenumSecs;
var
  i: integer;
begin
  for i := 0 to sectors.Count - 1 do
    sectors[i].num := i;
end;

Procedure TJKLevel.RenumThings;
var
  i: integer;
begin
  for i := 0 to things.Count - 1 do
    things[i].num := i;
end;

Function TJKLevel.GetLayerName(n: integer): String;
begin
  if (n < 0) or (n >= layers.Count) then
    Result := Format('LAYER%d', [n])
  else
    Result := layers[n];
end;

Function TJKLevel.AddLayer(const name: string): integer;
begin
  Result := layers.IndexOf(name);
  if Result = -1 then
    Result := layers.Add(name);
end;

Function TJKLevel.GetMasterCMP: string;
begin
  Result := masterCMP;
  if (Result = '') and (sectors.Count > 0) then
    Result := sectors[0].ColorMap;
  if Result = '' then
    Result := 'dflt.cmp';
end;

Constructor TJKLevel.Create;
begin
  header.fog.enabled  := False;
  header.fog.color    := colorWhite;
  header.fog.fogStart := 0.0;
  header.fog.fogEnd   := 250.0;

  sectors := TSectors.Create;
  self.templates := TTemplates.Create;
  things := TThings.Create;
  cogs   := TCOGs.Create;
  lights := TSedLights.Create;
  layers := TStringList.Create;
  h3donodes := THNodes.Create;
end;

Procedure TJKLevel.Clear;
var
  i, j, k: integer;
begin
  for i := 0 to sectors.Count - 1 do
    sectors[i].Free;
  sectors.Clear;

  self.templates.Clear;

  for i := 0 to things.Count - 1 do
    things[i].Free;
  things.Clear;

  for i := 0 to cogs.Count - 1 do
    cogs[i].Free;
  cogs.Clear;

  for i := 0 to lights.Count - 1 do
    lights[i].Free;

  for i := 0 to h3donodes.Count - 1 do
    h3donodes[i].Free;
  h3donodes.Clear;

  lights.Clear;
  layers.Clear;
  kind := TProjectType.JKDF2;
  masterCMP := '';
  lvisstring := '';

  ppunit := DefaultPixelPerUnit;
  CurID := 0;
end;

Destructor TJKLevel.Destroy;
begin
  Clear;
  sectors.Free;
  self.things.Free;
  templates.Free;
  lights.Free;
  layers.Free;
end;

Procedure TJKLevel.SetDefaultHeader;
begin
  with header do
  begin
    version := 1;
    Gravity := 4;
    if CurrentProject = IJIM then
      begin
        version := 3;
        Gravity := 2;
      end;

    ceilingSky.height := 15;
    horizonSky.distance := 100;
    horizonSky.pixelsPerRev := 768;
    horizonSky.offset := TPoint2.Create(0, 0);
    ceilingSky.offset := TPoint2.Create(0, 0);

    mipmapDistances[1] := 1;
    mipmapDistances[2] := 2;
    mipmapDistances[3] := 3;
    mipmapDistances[4] := 4;

    lodDistances[1] := 0.3;
    lodDistances[2] := 0.6;
    lodDistances[3] := 0.9;
    lodDistances[4] := 1.2;

    perspectiveDistance := 2;
    gouraudDistance     := 2;

    fog.enabled  := False;
    fog.color    := MakeColor(1.0, 1.0, 1.0, 1.0);
    fog.fogStart := 0.0;
    fog.fogEnd   := 250.0;
  end;
end;

Procedure TJKLevel.AddMissingLayers;
var
  i: integer;
begin
  for i := 0 to sectors.Count - 1 do
    with sectors[i] do
    begin
      if (layer < 0) or (layer >= layers.Count) then
        layer := AddLayer(Format('Layer%d', [layer]));
    end;

  for i := 0 to things.count - 1 do
    with things[i] do
    begin
      if (layer < 0) or (layer >= layers.Count) then
        layer := AddLayer(Format('Layer%d', [layer]));
    end;

  for i := 0 to lights.Count - 1 do
    with lights[i] do
    begin
      if (layer < 0) or (layer >= layers.Count) then
        layer := AddLayer(Format('Layer%d', [layer]));
    end;
end;

Function TJKLevel.GetSectorN(n: integer): TJKSector;
begin
  if n < 0 then
  begin
    Result := nil;
    exit;
  end;

  try
    Result := sectors[n];
  except
    on Exception do
      Result := nil;
  end;
end;

Function TJKLevel.GetSurfaceN(n: integer): TJKSurface;
var
  s, nsf: integer;
begin
  nsf := 0;
  if n < 0 then
  begin
    Result := nil;
    exit;
  end;

  for s := 0 to sectors.Count - 1 do
    with sectors[s] do
    begin
      if (n >= nsf) and (n < nsf + surfaces.Count) then
      begin
        Result := surfaces[n - nsf];
        exit;
      end;
      Inc(nsf, surfaces.Count);
    end;
  Result := nil;
end;

Function TJKLevel.GetSectorSurfaceN(sc, sf: integer): TJKSurface;
begin
  Result := nil;

  if sf < 0 then
      exit;
  var sec := GetSectorN(sc);
  if sec = nil then
    exit;

  try
    Result := sec.surfaces[sf];
  except
    on Exception do
      Result := nil;
  end;
end;

Function TJKLevel.GetSectorVertexN(sc, vx: integer): TJKVertex;
begin
  Result := nil;

  if vx < 0 then
      exit;
  var sec := GetSectorN(sc);
  if sec = nil then
    exit;

  Try
    Result := sec.vertices[vx];
  except
    On Exception do
      Result := nil;
  end;
end;

Function TJKLevel.GetThingN(n: integer): TJKThing;
begin
  if n < 0 then
  begin
    Result := nil;
    exit;
  end;

  try
    Result := things[n];
  except
    on Exception do
    begin
      Result := nil;
    end;
  end;
end;

Function TJKLevel.GetLightN(n: integer): TSedLight;
begin
    if n < 0 then
  begin
    Result := nil;
    exit;
  end;

  try
    Result := lights[n];
  except
    on Exception do
    begin
      Result := nil;
    end;
  end;
end;

Function TJKLevel.GetGlobalSFN(sc, sf: integer): integer;
var
  i, nsf: integer;
begin
  if sc = -1 then
  begin
    Result := -1;
    exit;
  end;

  nsf := 0;
  for i := 0 to sc - 1 do
    with sectors[i] do
    begin
      Inc(nsf, surfaces.count);
    end;
  Result := nsf + sf;
end;

Function SFtoInt(sc, sf: integer): integer;
begin
  Result := sc shl 16 + sf;
end;

Function EDtoInt(sc, sf, ed: integer): integer;
begin
  Result := sc shl 18 + sf shl 8 + ed;
end;

Function VXToInt(sc, vx: integer): integer;
begin
  Result := sc shl 16 + vx;
end;

Function FRToInt(th, fr: integer): integer;
begin
  Result := th shl 16 + fr;
end;

Function GetsfSC(scsf: integer): integer;
begin
  Result := scsf shr 16;
end;

Function GetsfSF(scsf: integer): integer;
begin
  Result := scsf and $FFFF;
end;

Function GetedSC(scsfed: integer): integer;
begin
  Result := scsfed shr 18;
end;

Function GetedSF(scsfed: integer): integer;
begin
  Result := (scsfed shr 8) and $3FF;
end;

Function GetedED(scsfed: integer): integer;
begin
  Result := scsfed and $FF;
end;

Function GetvxSC(scvx: integer): integer;
begin
  Result := scvx shr 16;
end;

Function GetvxVX(scvx: integer): integer;
begin
  Result := scvx and $FFFF;
end;

Function GetfrTH(thfr: integer): integer;
begin
  Result := thfr shr 16;
end;

Function GetfrFR(thfr: integer): integer;
begin
  Result := thfr and $FFFF;
end;

Function GetStaticSoundList(kind: TProjectType): TStringList;
begin
  Result := nil;
  case kind of
    TProjectType.IJIM:
      Result := IJIMStaticSoundList;
  end;
end;

Function GetStaticMatList(kind: TProjectType): TStringList;
begin
  Result := nil;
  case kind of
    TProjectType.IJIM:
      Result := IJIMStaticMatList;
  end;
end;

Function GetStatic3doList(kind: TProjectType): TStringList;
begin
  Result := nil;
  case kind of
    TProjectType.IJIM:
      Result := IJIMStatic3doList;
  end;
end;

Function GetStaticsprList(kind: TProjectType): TStringList;
begin
  Result := nil;
  case kind of
    TProjectType.IJIM:
      Result := IJIMStaticSprList;
  end;
end;

Function GetStaticKeyList(kind: TProjectType): TStringList;
begin
  Result := nil;
  case kind of
    TProjectType.IJIM:
      Result := IJIMStaticKeyList;
  end;
end;

Function GetStaticCogScriptList(kind: TProjectType): TStringList;
begin
  Result := nil;
  case kind of
    TProjectType.IJIM:
      Result := IJIMStaticCogScriptList;
  end;
end;

Procedure InitStaticResources();
begin
  // Infernal Machine static sounds
  IJIMStaticSoundList.Add('mus_map.wav');
  IJIMStaticSoundList.Add('mus_gen_maploadloop.wav');
  IJIMStaticSoundList.Add('mus_enddemo.wav');
  IJIMStaticSoundList.Add('gen_whip_fire.wav');
  IJIMStaticSoundList.Add('gen_whip_pull.wav');
  IJIMStaticSoundList.Add('gen_whip_put.wav');
  IJIMStaticSoundList.Add('gen_pistol_fire.wav');
  IJIMStaticSoundList.Add('gen_pistol_pull.wav');
  IJIMStaticSoundList.Add('gen_pistol_put.wav');
  IJIMStaticSoundList.Add('gen_machete_fire.wav');
  IJIMStaticSoundList.Add('gen_machete_pull.wav');
  IJIMStaticSoundList.Add('gen_machete_put.wav');
  IJIMStaticSoundList.Add('gen_zippo_open.wav');
  IJIMStaticSoundList.Add('gen_zippo_close.wav');
  IJIMStaticSoundList.Add('fol_in_lwalkmetal.wav');
  IJIMStaticSoundList.Add('fol_in_rwalkmetal.wav');
  IJIMStaticSoundList.Add('fol_in_lrunmetal.wav');
  IJIMStaticSoundList.Add('fol_in_rrunmetal.wav');
  IJIMStaticSoundList.Add('fol_in_lwalkearth.wav');
  IJIMStaticSoundList.Add('fol_in_rwalkearth.wav');
  IJIMStaticSoundList.Add('fol_in_lrunearth.wav');
  IJIMStaticSoundList.Add('fol_in_rrunearth.wav');
  IJIMStaticSoundList.Add('fol_in_lwalkhard.wav');
  IJIMStaticSoundList.Add('fol_in_rwalkhard.wav');
  IJIMStaticSoundList.Add('fol_in_lrunhard.wav');
  IJIMStaticSoundList.Add('fol_in_rrunhard.wav');
  IJIMStaticSoundList.Add('fol_in_lwalkwater.wav');
  IJIMStaticSoundList.Add('fol_in_rwalkwater.wav');
  IJIMStaticSoundList.Add('fol_in_lrunwater.wav');
  IJIMStaticSoundList.Add('fol_in_rrunwater.wav');
  IJIMStaticSoundList.Add('fol_in_rwalkpuddle.wav');
  IJIMStaticSoundList.Add('fol_in_lwalkpuddle.wav');
  IJIMStaticSoundList.Add('fol_in_lrunpuddle.wav');
  IJIMStaticSoundList.Add('fol_in_rrunpuddle.wav');
  IJIMStaticSoundList.Add('fol_in_jumpearth.wav');
  IJIMStaticSoundList.Add('fol_in_landearth.wav');
  IJIMStaticSoundList.Add('fol_in_landhard.wav');
  IJIMStaticSoundList.Add('fol_in_landmetal.wav');
  IJIMStaticSoundList.Add('fol_in_landpuddle.wav');
  IJIMStaticSoundList.Add('fol_in_enterwater.wav');
  IJIMStaticSoundList.Add('fol_in_landhurt.wav');
  IJIMStaticSoundList.Add('fol_in_enterwaterslow.wav');
  IJIMStaticSoundList.Add('fol_in_exitwater.wav');
  IJIMStaticSoundList.Add('fol_in_hithard.wav');
  IJIMStaticSoundList.Add('fol_in_hitmetal.wav');
  IJIMStaticSoundList.Add('fol_in_hitearth.wav');
  IJIMStaticSoundList.Add('fol_in_lswimsurface.wav');
  IJIMStaticSoundList.Add('fol_in_rswimsurface.wav');
  IJIMStaticSoundList.Add('fol_in_treadsurface.wav');
  IJIMStaticSoundList.Add('fol_in_swimunder.wav');
  IJIMStaticSoundList.Add('fol_in_treadunder.wav');
  IJIMStaticSoundList.Add('gen_underwater_a.wav');
  IJIMStaticSoundList.Add('inv_close.wav');
  IJIMStaticSoundList.Add('inv_expand.wav');
  IJIMStaticSoundList.Add('inv_move.wav');
  IJIMStaticSoundList.Add('alt_arrive.wav');
  IJIMStaticSoundList.Add('inv_slide_to.wav');
  IJIMStaticSoundList.Add('mus_find.wav');
  IJIMStaticSoundList.Add('mus_flourish.wav');
  IJIMStaticSoundList.Add('inxj087.wav');
  IJIMStaticSoundList.Add('inxj088.wav');
  IJIMStaticSoundList.Add('inxj089.wav');
  IJIMStaticSoundList.Add('inxj092.wav');
  IJIMStaticSoundList.Add('gen_fists_fire.wav');
  IJIMStaticSoundList.Add('gen_bazooka_fire.wav');
  IJIMStaticSoundList.Add('gen_bazooka_pull.wav');
  IJIMStaticSoundList.Add('gen_bazooka_put.wav');
  IJIMStaticSoundList.Add('gen_mauser_fire.wav');
  IJIMStaticSoundList.Add('gen_rifle_pull.wav');
  IJIMStaticSoundList.Add('gen_rifle_put.wav');
  IJIMStaticSoundList.Add('gen_tokarev_fire.wav');
  IJIMStaticSoundList.Add('fol_in_lrunsnow.wav');
  IJIMStaticSoundList.Add('fol_in_rrunsnow.wav');
  IJIMStaticSoundList.Add('fol_in_lwalksnow.wav');
  IJIMStaticSoundList.Add('fol_in_rwalksnow.wav');
  IJIMStaticSoundList.Add('gen_simonov_fire.wav');
  IJIMStaticSoundList.Add('gen_submachine_fire.wav');
  IJIMStaticSoundList.Add('gen_shotgun_fire.wav');
  IJIMStaticSoundList.Add('gen_grenade_fire.wav');
  IJIMStaticSoundList.Add('inv_quicksave.wav');
  IJIMStaticSoundList.Add('gen_machete_hit_flesh.wav');
  IJIMStaticSoundList.Add('gen_machete_hit_stone.wav');
  IJIMStaticSoundList.Add('gen_machete_hit_metal.wav');
  IJIMStaticSoundList.Add('gen_machete_hit_wood.wav');
  IJIMStaticSoundList.Add('gen_machete_hit_vine.wav');
  IJIMStaticSoundList.Add('gen_machete_hit_web.wav');
  IJIMStaticSoundList.Add('gen_submachine_trail.wav');
  IJIMStaticSoundList.Add('gen_grenade_tick.wav');
  IJIMStaticSoundList.Add('gen_grenade_bounce.wav');
  IJIMStaticSoundList.Add('fol_in_lwalkwood.wav');
  IJIMStaticSoundList.Add('fol_in_rwalkwood.wav');
  IJIMStaticSoundList.Add('fol_in_lrunwood.wav');
  IJIMStaticSoundList.Add('fol_in_rrunwood.wav');
  IJIMStaticSoundList.Add('fol_in_landsnow.wav');
  IJIMStaticSoundList.Add('fol_in_landwood.wav');
  IJIMStaticSoundList.Add('gen_satchel_fire.wav');
  IJIMStaticSoundList.Add('gen_satchel_tick.wav');
  IJIMStaticSoundList.Add('gen_satchel_trip.wav');
  IJIMStaticSoundList.Add('mus_guybrush.wav');
  IJIMStaticSoundList.Add('gen_rocket.wav');
  IJIMStaticSoundList.Add('gen_whipswing.wav');
  IJIMStaticSoundList.Add('inxj139.wav');
  IJIMStaticSoundList.Add('inxj142.wav');
  IJIMStaticSoundList.Add('inxj002.wav');
  IJIMStaticSoundList.Add('inxj143.wav');
  IJIMStaticSoundList.Add('inxj008a.wav');
  IJIMStaticSoundList.Add('inxj146.wav');
  IJIMStaticSoundList.Add('inxj147.wav');
  IJIMStaticSoundList.Add('fol_in_jumpmetal.wav');
  IJIMStaticSoundList.Add('fol_in_jumpwater.wav');
  IJIMStaticSoundList.Add('fol_in_jumpsnow.wav');
  IJIMStaticSoundList.Add('fol_in_jumpwood.wav');
  IJIMStaticSoundList.Add('gen_grenade_pull.wav');
  IJIMStaticSoundList.Add('gen_grenade_put.wav');
  IJIMStaticSoundList.Add('imp1_chargeup.wav');
  IJIMStaticSoundList.Add('imp1_powerup.wav');
  IJIMStaticSoundList.Add('imp1_powerdown.wav');
  IJIMStaticSoundList.Add('imp1_on_loop.wav');
  IJIMStaticSoundList.Add('imp2_activate.wav');
  IJIMStaticSoundList.Add('imp2_powerup.wav');
  IJIMStaticSoundList.Add('imp2_powerdown.wav');
  IJIMStaticSoundList.Add('imp2_on_loop.wav');
  IJIMStaticSoundList.Add('imp3_activate.wav');
  IJIMStaticSoundList.Add('imp3_powerup.wav');
  IJIMStaticSoundList.Add('imp3_powerdown.wav');
  IJIMStaticSoundList.Add('imp3_on_loop.wav');
  IJIMStaticSoundList.Add('imp4_activate.wav');
  IJIMStaticSoundList.Add('imp4_powerup.wav');
  IJIMStaticSoundList.Add('imp4_powerdown.wav');
  IJIMStaticSoundList.Add('imp4_on_loop.wav');
  IJIMStaticSoundList.Add('imp5_chargeup.wav');
  IJIMStaticSoundList.Add('imp5_powerup.wav');
  IJIMStaticSoundList.Add('imp5_powerdown.wav');
  IJIMStaticSoundList.Add('imp5_on_loop.wav');
  IJIMStaticSoundList.Add('riv_splash_sm.wav');
  IJIMStaticSoundList.Add('lag_torp_explo_c.wav');
  IJIMStaticSoundList.Add('mus_gen_indydies1.wav');
  IJIMStaticSoundList.Add('inv_treasure.wav');
  IJIMStaticSoundList.Add('gen_machete_pull_uw.wav');
  IJIMStaticSoundList.Add('gen_machete_put_uw.wav');
  IJIMStaticSoundList.Add('gen_machete_fire_uw.wav');
  IJIMStaticSoundList.Add('gen_ricochet_a.wav');
  IJIMStaticSoundList.Add('gen_ricochet_b.wav');
  IJIMStaticSoundList.Add('inxj154.wav');
  IJIMStaticSoundList.Add('inxj155.wav');
  IJIMStaticSoundList.Add('inxj156.wav');
  IJIMStaticSoundList.Add('inxj157.wav');
  IJIMStaticSoundList.Add('gen_whip_unravel.wav');
  IJIMStaticSoundList.Add('gen_whip_ravel.wav');
  IJIMStaticSoundList.Add('fol_in_slide_1.wav');
  IJIMStaticSoundList.Add('fol_in_slide_2.wav');
  IJIMStaticSoundList.Add('fol_in_slide_3.wav');
  IJIMStaticSoundList.Add('fol_in_jumphard.wav');
  IJIMStaticSoundList.Add('fol_in_jumphardecho.wav');
  IJIMStaticSoundList.Add('fol_in_jumpwoodecho.wav');
  IJIMStaticSoundList.Add('fol_in_jumpearthecho.wav');
  IJIMStaticSoundList.Add('fol_in_jumpaet.wav');
  IJIMStaticSoundList.Add('fol_in_climb_down_onto.wav');
  IJIMStaticSoundList.Add('fol_in_climb_horiz.wav');
  IJIMStaticSoundList.Add('fol_in_climb_onto.wav');
  IJIMStaticSoundList.Add('fol_in_climb_vert_left.wav');
  IJIMStaticSoundList.Add('fol_in_climb_vert_right.wav');
  IJIMStaticSoundList.Add('inxj061.wav');
  IJIMStaticSoundList.Add('inxj224.wav');
  IJIMStaticSoundList.Add('gen_in_gasp.wav');
  IJIMStaticSoundList.Add('gen_in_die_uw.wav');
  IJIMStaticSoundList.Add('fol_in_aet_sector.wav');
  IJIMStaticSoundList.Add('gen_rocket_loop.wav');
  IJIMStaticSoundList.Add('gen_chalk.wav');
  IJIMStaticSoundList.Add('inxj016g.wav');
  IJIMStaticSoundList.Add('inxj017.wav');
  IJIMStaticSoundList.Add('inxj017a.wav');
  IJIMStaticSoundList.Add('inxj017e.wav');
  IJIMStaticSoundList.Add('inxj017f.wav');
  IJIMStaticSoundList.Add('inxj017g.wav');

  // Infernal Machine static mats
  IJIMStaticMatList.Add('gen_4indy_shadow.mat');
  IJIMStaticMatList.Add('gen_4indy_shadow_stretch.mat');
  IJIMStaticMatList.Add('jep_4shadow.mat');
  IJIMStaticMatList.Add('inshoe.mat');
  IJIMStaticMatList.Add('insole.mat');
  IJIMStaticMatList.Add('inleg.mat');
  IJIMStaticMatList.Add('inthigh.mat');
  IJIMStaticMatList.Add('inpouch.mat');
  IJIMStaticMatList.Add('inholst.mat');
  IJIMStaticMatList.Add('inbutt.mat');
  IJIMStaticMatList.Add('inhip.mat');
  IJIMStaticMatList.Add('gen_4gun_bazooka.mat');
  IJIMStaticMatList.Add('bgpalm.mat');
  IJIMStaticMatList.Add('bgback.mat');
  IJIMStaticMatList.Add('bgtop.mat');
  IJIMStaticMatList.Add('bgstrings4.mat');
  IJIMStaticMatList.Add('weapon_mchte_hndl.mat');
  IJIMStaticMatList.Add('weapon_mchte_blde.mat');
  IJIMStaticMatList.Add('weapon_4mauser_sde.mat');
  IJIMStaticMatList.Add('ppsh-41_4.mat');
  IJIMStaticMatList.Add('gen_4gun_rev.mat');
  IJIMStaticMatList.Add('simonov4.mat');
  IJIMStaticMatList.Add('weapon_4tokarev_sde.mat');
  IJIMStaticMatList.Add('toz4.mat');
  IJIMStaticMatList.Add('icon_camera_top.mat');
  IJIMStaticMatList.Add('icon_camera_view.mat');
  IJIMStaticMatList.Add('icon_camera_shtr.mat');
  IJIMStaticMatList.Add('icon_camera_dial.mat');
  IJIMStaticMatList.Add('icon_camera_ktop.mat');
  IJIMStaticMatList.Add('icon_camera_ssde.mat');
  IJIMStaticMatList.Add('icon_camera_lenz.mat');
  IJIMStaticMatList.Add('icon_camera_knob.mat');
  IJIMStaticMatList.Add('icon_camera_lsde.mat');
  IJIMStaticMatList.Add('icon_camera_frnt.mat');
  IJIMStaticMatList.Add('tchest_fr.mat');
  IJIMStaticMatList.Add('tchest_cover.mat');
  IJIMStaticMatList.Add('tchest_gold.mat');
  IJIMStaticMatList.Add('icon_exit_frnt.mat');
  IJIMStaticMatList.Add('icon_film_lid.mat');
  IJIMStaticMatList.Add('icon_4film_tab.mat');
  IJIMStaticMatList.Add('icon_film_lablea.mat');
  IJIMStaticMatList.Add('icon_film_lableb.mat');
  IJIMStaticMatList.Add('icon_film_rim.mat');
  IJIMStaticMatList.Add('icon_hlthbig_top.mat');
  IJIMStaticMatList.Add('icon_hlthbig_side.mat');
  IJIMStaticMatList.Add('icon_hlthbig_hndl.mat');
  IJIMStaticMatList.Add('icon_hlthbig_frnt.mat');
  IJIMStaticMatList.Add('gen_icon_hlthsml.mat');
  IJIMStaticMatList.Add('icon_radio_top.mat');
  IJIMStaticMatList.Add('icon_radio_knob.mat');
  IJIMStaticMatList.Add('icon_radio_frnt.mat');
  IJIMStaticMatList.Add('icon_radio_back.mat');
  IJIMStaticMatList.Add('icon_radio_side.mat');
  IJIMStaticMatList.Add('telgrf_speed_cap.mat');
  IJIMStaticMatList.Add('telgrf_speed_hndl.mat');
  IJIMStaticMatList.Add('telgrf_speed_bolt.mat');
  IJIMStaticMatList.Add('telgrf_4speed_cntrl.mat');
  IJIMStaticMatList.Add('telgrf_speed_rim.mat');
  IJIMStaticMatList.Add('telgrf_speed_panl.mat');
  IJIMStaticMatList.Add('icon_tv_top.mat');
  IJIMStaticMatList.Add('icon_tv_side.mat');
  IJIMStaticMatList.Add('icon_tv_screen.mat');
  IJIMStaticMatList.Add('icon_tv_back.mat');
  IJIMStaticMatList.Add('icon_tv_knob.mat');
  IJIMStaticMatList.Add('icon_type_top.mat');
  IJIMStaticMatList.Add('icon_type_side.mat');
  IJIMStaticMatList.Add('icon_type_back.mat');
  IJIMStaticMatList.Add('icon_type_frnt.mat');
  IJIMStaticMatList.Add('zibody.mat');
  IJIMStaticMatList.Add('zilighter.mat');
  IJIMStaticMatList.Add('idol_gold_base.mat');
  IJIMStaticMatList.Add('idol_gold_frnt.mat');
  IJIMStaticMatList.Add('idol_slvr_base.mat');
  IJIMStaticMatList.Add('idol_slvr_frnt.mat');
  IJIMStaticMatList.Add('coins_gold_bot.mat');
  IJIMStaticMatList.Add('coins_gold_top.mat');
  IJIMStaticMatList.Add('coins_slvr_bot.mat');
  IJIMStaticMatList.Add('coins_slvr_top.mat');
  IJIMStaticMatList.Add('ingot_gold_top.mat');
  IJIMStaticMatList.Add('ingot_gold_end.mat');
  IJIMStaticMatList.Add('ingot_slvr_top.mat');
  IJIMStaticMatList.Add('ingot_slvr_end.mat');
  IJIMStaticMatList.Add('weapon_grnad_top.mat');
  IJIMStaticMatList.Add('weapon_grnad_hed.mat');
  IJIMStaticMatList.Add('weapon_grnad_sde.mat');
  IJIMStaticMatList.Add('weapon_grnad_pin.mat');
  IJIMStaticMatList.Add('weapon_grnad_hndl.mat');
  IJIMStaticMatList.Add('weapon_4grnad_rng.mat');
  IJIMStaticMatList.Add('inskin.mat');
  IJIMStaticMatList.Add('inhanbm.mat');
  IJIMStaticMatList.Add('inhandf.mat');
  IJIMStaticMatList.Add('inhanbk.mat');
  IJIMStaticMatList.Add('inhansd.mat');
  IJIMStaticMatList.Add('inindex.mat');
  IJIMStaticMatList.Add('ziflame4_01.mat');
  IJIMStaticMatList.Add('gen_a4sfx_lighterglow.mat');
  IJIMStaticMatList.Add('handoptp.mat');
  IJIMStaticMatList.Add('handopplm.mat');
  IJIMStaticMatList.Add('whip.mat');
  IJIMStaticMatList.Add('intorsb.mat');
  IJIMStaticMatList.Add('intorsf.mat');
  IJIMStaticMatList.Add('sheath.mat');
  IJIMStaticMatList.Add('inshtorsbk.mat');
  IJIMStaticMatList.Add('inshtorsf.mat');
  IJIMStaticMatList.Add('inwhip.mat');
  IJIMStaticMatList.Add('rftop.mat');
  IJIMStaticMatList.Add('rfbm.mat');
  IJIMStaticMatList.Add('gen_4icon_bonusmap.mat');
  IJIMStaticMatList.Add('gen_4arrow_down.mat');
  IJIMStaticMatList.Add('gen_4arrow_up.mat');
  IJIMStaticMatList.Add('hud_health_base.mat');
  IJIMStaticMatList.Add('hud_health_overlay.mat');
  IJIMStaticMatList.Add('hud_item_pickup.mat');
  IJIMStaticMatList.Add('hud_breath_overlay.mat');
  IJIMStaticMatList.Add('hinticon.mat');
  IJIMStaticMatList.Add('indyicon.mat');
  IJIMStaticMatList.Add('icon_4hint_sde.mat');
  IJIMStaticMatList.Add('icon_hint_botb.mat');
  IJIMStaticMatList.Add('icon_hint_scrw.mat');
  IJIMStaticMatList.Add('icon_4hint_top.mat');
  IJIMStaticMatList.Add('icon_4hint_flmt.mat');
  IJIMStaticMatList.Add('safront.mat');
  IJIMStaticMatList.Add('saback.mat');
  IJIMStaticMatList.Add('satag.mat');
  IJIMStaticMatList.Add('inhdsd.mat');
  IJIMStaticMatList.Add('inhdfr.mat');
  IJIMStaticMatList.Add('inhair.mat');
  IJIMStaticMatList.Add('inear.mat');
  IJIMStaticMatList.Add('inmouthins.mat');
  IJIMStaticMatList.Add('white.mat');
  IJIMStaticMatList.Add('imp1sd_4.mat');
  IJIMStaticMatList.Add('imp2bm.mat');
  IJIMStaticMatList.Add('imp2sd.mat');
  IJIMStaticMatList.Add('lxtop.mat');
  IJIMStaticMatList.Add('imp2red4.mat');
  IJIMStaticMatList.Add('imp2blue4.mat');
  IJIMStaticMatList.Add('imp2s.mat');
  IJIMStaticMatList.Add('imp3_peg.mat');
  IJIMStaticMatList.Add('imp3_side.mat');
  IJIMStaticMatList.Add('imp3_circles.mat');
  IJIMStaticMatList.Add('imp3_y.mat');
  IJIMStaticMatList.Add('imp3_moon.mat');
  IJIMStaticMatList.Add('imp4_tube4.mat');
  IJIMStaticMatList.Add('imp4_ringsd.mat');
  IJIMStaticMatList.Add('imp4_stick.mat');
  IJIMStaticMatList.Add('imp4_light4_01.mat');
  IJIMStaticMatList.Add('gen_a4sprite_gunshot.mat');
  IJIMStaticMatList.Add('gen_a4sprite_autoshot.mat');
  IJIMStaticMatList.Add('gen_a4sfx_rbbeam_b.mat');
  IJIMStaticMatList.Add('gen_4red.mat');
  IJIMStaticMatList.Add('gen_a4sfx_lightning.mat');
  IJIMStaticMatList.Add('gen_4x_x.mat');
  IJIMStaticMatList.Add('rckt_bzoka_tail.mat');
  IJIMStaticMatList.Add('rckt_bzoka_tip.mat');
  IJIMStaticMatList.Add('gen_a4sfx_rokt_flame.mat');
  IJIMStaticMatList.Add('gen_4x_check.mat');
  IJIMStaticMatList.Add('gen_4x_circle.mat');
  IJIMStaticMatList.Add('gen_4x_guy.mat');
  IJIMStaticMatList.Add('gen_4x_happy.mat');
  IJIMStaticMatList.Add('gen_4x_indy.mat');
  IJIMStaticMatList.Add('gen_4x_la.mat');
  IJIMStaticMatList.Add('gen_4x_usa.mat');
  IJIMStaticMatList.Add('gen_4x_here.mat');
  IJIMStaticMatList.Add('gen_4x_love.mat');
  IJIMStaticMatList.Add('gen_4x_indy2.mat');
  IJIMStaticMatList.Add('chalk_invnty_sde.mat');
  IJIMStaticMatList.Add('chalk_invnty_top.mat');
  IJIMStaticMatList.Add('in_face.mat');
  IJIMStaticMatList.Add('bullet_top.mat');
  IJIMStaticMatList.Add('gen_ammo_toz34.mat');
  IJIMStaticMatList.Add('gen_ammo_tokarev.mat');
  IJIMStaticMatList.Add('gen_ammo_tokatop.mat');
  IJIMStaticMatList.Add('gen_ammo_tokabult.mat');
  IJIMStaticMatList.Add('gen_ammo_simonov.mat');
  IJIMStaticMatList.Add('gen_ammo_ppsh41.mat');
  IJIMStaticMatList.Add('bullet_side.mat');
  IJIMStaticMatList.Add('gen_a4sprite_hit.mat');
  IJIMStaticMatList.Add('gen_a4sprite_gunpuff_a.mat');
  IJIMStaticMatList.Add('gen_a4sprite_gunpuff_b.mat');
  IJIMStaticMatList.Add('gen_a4sprite_blood.mat');
  IJIMStaticMatList.Add('gen_4sfx_bazooka_smoke.mat');
  IJIMStaticMatList.Add('gen_a4sprite_xpld3.mat');
  IJIMStaticMatList.Add('gen_4sprite_bubble.mat');
  IJIMStaticMatList.Add('gen_4sfx_ripple_ring_b.mat');
  IJIMStaticMatList.Add('gen_icon_help_rim.mat');
  IJIMStaticMatList.Add('gen_icon_help_frnt.mat');
  IJIMStaticMatList.Add('gen_icon_help_back.mat');
  IJIMStaticMatList.Add('hud_health_overlay_hit.mat');
  IJIMStaticMatList.Add('hud_health_overlay_skull.mat');
  IJIMStaticMatList.Add('gen_4gem_red.mat');
  IJIMStaticMatList.Add('gen_4gem_green.mat');
  IJIMStaticMatList.Add('gen_4gem_blue.mat');
  IJIMStaticMatList.Add('bab_tudoor_key_frnt.mat');
  IJIMStaticMatList.Add('bab_tudoor_key_sd.mat');
  IJIMStaticMatList.Add('tblt_lck_frnt.mat');
  IJIMStaticMatList.Add('tblt_lck_rim.mat');
  IJIMStaticMatList.Add('idol_mrdk_wngs.mat');
  IJIMStaticMatList.Add('idol_mrdk_chst.mat');
  IJIMStaticMatList.Add('idol_mrdk_bck.mat');
  IJIMStaticMatList.Add('idol_mrdk_bot.mat');
  IJIMStaticMatList.Add('idol_mrdk_face.mat');
  IJIMStaticMatList.Add('kit_ptch_sde.mat');
  IJIMStaticMatList.Add('kit_ptch_top.mat');
  IJIMStaticMatList.Add('cndl_wind_sdea.mat');
  IJIMStaticMatList.Add('cndl_wind_topa.mat');
  IJIMStaticMatList.Add('cndl_4wind_wik.mat');
  IJIMStaticMatList.Add('cndl_wind_sdeb.mat');
  IJIMStaticMatList.Add('cndl_wind_topb.mat');
  IJIMStaticMatList.Add('cndl_wind_sdec.mat');
  IJIMStaticMatList.Add('cndl_wind_topc.mat');
  IJIMStaticMatList.Add('cndl_wind_sded.mat');
  IJIMStaticMatList.Add('cndl_wind_topd.mat');
  IJIMStaticMatList.Add('riv_coin.mat');
  IJIMStaticMatList.Add('shs_mondr_key_fr.mat');
  IJIMStaticMatList.Add('shs_mondr_key_sd.mat');
  IJIMStaticMatList.Add('flpotsd.mat');
  IJIMStaticMatList.Add('flpottp.mat');
  IJIMStaticMatList.Add('flbulb.mat');
  IJIMStaticMatList.Add('flleaf.mat');
  IJIMStaticMatList.Add('flpetalbig.mat');
  IJIMStaticMatList.Add('jar_oil_hndl.mat');
  IJIMStaticMatList.Add('jar_oil_cork.mat');
  IJIMStaticMatList.Add('dksdblue.mat');
  IJIMStaticMatList.Add('dkchainblue4.mat');
  IJIMStaticMatList.Add('dkchain4.mat');
  IJIMStaticMatList.Add('key_pdlck_rim.mat');
  IJIMStaticMatList.Add('key_pdlck_tip.mat');
  IJIMStaticMatList.Add('key_4pdlck_hndl.mat');
  IJIMStaticMatList.Add('key_pdlck_sde.mat');
  IJIMStaticMatList.Add('shvl_fold_spad.mat');
  IJIMStaticMatList.Add('shvl_fold_brkt.mat');
  IJIMStaticMatList.Add('lag_zero_prop.mat');
  IJIMStaticMatList.Add('hammer_sde.mat');
  IJIMStaticMatList.Add('hammer_hndl.mat');
  IJIMStaticMatList.Add('hammer_nose.mat');
  IJIMStaticMatList.Add('dvice_armng_pin.mat');
  IJIMStaticMatList.Add('dvice_armng_cap.mat');
  IJIMStaticMatList.Add('dvice_armng_dsc.mat');
  IJIMStaticMatList.Add('dvice_armng_tube.mat');
  IJIMStaticMatList.Add('crnk_rsty_sde.mat');
  IJIMStaticMatList.Add('key_statue_side.mat');
  IJIMStaticMatList.Add('key_statue_face.mat');
  IJIMStaticMatList.Add('key_statueb_side.mat');
  IJIMStaticMatList.Add('key_statueb_face.mat');
  IJIMStaticMatList.Add('key_mnky_bmbu.mat');
  IJIMStaticMatList.Add('key_mnky_body.mat');
  IJIMStaticMatList.Add('tem_sharkey.mat');
  IJIMStaticMatList.Add('key_water_sdea.mat');
  IJIMStaticMatList.Add('key_water_tooth.mat');
  IJIMStaticMatList.Add('key_water_sdeb.mat');
  IJIMStaticMatList.Add('idol_bird_sde.mat');
  IJIMStaticMatList.Add('idol_bird_bck.mat');
  IJIMStaticMatList.Add('idol_fish_bse.mat');
  IJIMStaticMatList.Add('idol_fish_sde.mat');
  IJIMStaticMatList.Add('idol_jag_back.mat');
  IJIMStaticMatList.Add('idol_jag_side.mat');
  IJIMStaticMatList.Add('mirror_a_statue.mat');
  IJIMStaticMatList.Add('minecar_wheelside.mat');
  IJIMStaticMatList.Add('minecar_gascap.mat');
  IJIMStaticMatList.Add('gem_4eye_front.mat');
  IJIMStaticMatList.Add('watop.mat');
  IJIMStaticMatList.Add('wabk.mat');
  IJIMStaticMatList.Add('wabm.mat');
  IJIMStaticMatList.Add('waknob.mat');
  IJIMStaticMatList.Add('wa1ring.mat');
  IJIMStaticMatList.Add('wa1chain.mat');
  IJIMStaticMatList.Add('elvatr_rock_chain.mat');
  IJIMStaticMatList.Add('jcan_redfrnt.mat');
  IJIMStaticMatList.Add('jcan_redbot.mat');
  IJIMStaticMatList.Add('kndlng_cone_side.mat');
  IJIMStaticMatList.Add('kndlng_cone_top.mat');
  IJIMStaticMatList.Add('oilcan_oz_top.mat');
  IJIMStaticMatList.Add('oilcan_oz_noz.mat');
  IJIMStaticMatList.Add('oilcan_4oz_hndl.mat');
  IJIMStaticMatList.Add('fuse_rplc_tip.mat');
  IJIMStaticMatList.Add('fuse_4rplc_gls.mat');
  IJIMStaticMatList.Add('fuse_rplc_end.mat');
  IJIMStaticMatList.Add('fuse_rplc_in.mat');
  IJIMStaticMatList.Add('sol_4gem_green.mat');
  IJIMStaticMatList.Add('sol_4gem_blue.mat');
  IJIMStaticMatList.Add('sol_4gem_red.mat');
  IJIMStaticMatList.Add('pymd_key_top.mat');
  IJIMStaticMatList.Add('pymd_key_eye.mat');
  IJIMStaticMatList.Add('gen_lever_arm.mat');
  IJIMStaticMatList.Add('gen_lever_grip.mat');
  IJIMStaticMatList.Add('anlhandm.mat');
  IJIMStaticMatList.Add('rigear.mat');
  IJIMStaticMatList.Add('key_elvtr_rim.mat');
  IJIMStaticMatList.Add('key_elvtr_teth.mat');
  IJIMStaticMatList.Add('key_elvtr_pin.mat');
  IJIMStaticMatList.Add('rbeye.mat');
  IJIMStaticMatList.Add('pikes_floor_frnt.mat');
  IJIMStaticMatList.Add('pikes_floor_top.mat');
  IJIMStaticMatList.Add('fsfr.mat');
  IJIMStaticMatList.Add('fssd.mat');
  IJIMStaticMatList.Add('key_sqr_sdea.mat');
  IJIMStaticMatList.Add('key_sqr_tooth.mat');
  IJIMStaticMatList.Add('key_sqr_sdeb.mat');
  IJIMStaticMatList.Add('shvl_fold_hndl.mat');
  IJIMStaticMatList.Add('key_mnky_end.mat');
  IJIMStaticMatList.Add('minecar_bumper.mat');
  IJIMStaticMatList.Add('gem_4eye_ball.mat');
  IJIMStaticMatList.Add('jcan_redtop.mat');
  IJIMStaticMatList.Add('key_elvtr_hndl.mat');
  IJIMStaticMatList.Add('minecar_wheelrim.mat');
  IJIMStaticMatList.Add('inshleg.mat');
  IJIMStaticMatList.Add('inshthigh.mat');
  IJIMStaticMatList.Add('inshbutt.mat');
  IJIMStaticMatList.Add('inshhip.mat');
  IJIMStaticMatList.Add('insh4arm.mat');
  IJIMStaticMatList.Add('insharm.mat');
  IJIMStaticMatList.Add('inhat.mat');
  IJIMStaticMatList.Add('inneck.mat');
  IJIMStaticMatList.Add('in4arm.mat');
  IJIMStaticMatList.Add('inarm.mat');
  IJIMStaticMatList.Add('bullet_4streak.mat');
  IJIMStaticMatList.Add('gen_4imp1_circle.mat');
  IJIMStaticMatList.Add('gen_a4sfx_imp2fire.mat');
  IJIMStaticMatList.Add('gen_a4sfx_splash.mat');
  IJIMStaticMatList.Add('gen_a4sfx_wake.mat');
  IJIMStaticMatList.Add('gen_a4xplod_blast.mat');
  IJIMStaticMatList.Add('gen_a4xplod_fire.mat');
  IJIMStaticMatList.Add('gen_a4xplod_smoke.mat');
  IJIMStaticMatList.Add('iq_icon.mat');
  IJIMStaticMatList.Add('dflt.mat');
  IJIMStaticMatList.Add('dksd.mat');
  IJIMStaticMatList.Add('in33_whip.mat');
  IJIMStaticMatList.Add('jar_oil_sde.mat');
  IJIMStaticMatList.Add('in33_gunhand.mat');
  IJIMStaticMatList.Add('in33_hand.mat');
  IJIMStaticMatList.Add('in33_shirtfrn.mat');
  IJIMStaticMatList.Add('gen_icon_bite.mat');
  IJIMStaticMatList.Add('vol_tram_car_plate.mat');
  IJIMStaticMatList.Add('vol_tram_car_wheel.mat');
  IJIMStaticMatList.Add('tozbarl.mat');
  IJIMStaticMatList.Add('inpaddl.mat');
  IJIMStaticMatList.Add('obj_sol_4key_oil.mat');
  IJIMStaticMatList.Add('vol_wall_logs_weathered.mat');
  IJIMStaticMatList.Add('obj_vol_4key_cuffs.mat');
  IJIMStaticMatList.Add('mrhdfr.mat');
  IJIMStaticMatList.Add('hoist_boat_hndl.mat');
  IJIMStaticMatList.Add('hoist_4boat_whl.mat');
  IJIMStaticMatList.Add('hoist_boat_pin.mat');
  IJIMStaticMatList.Add('potshard.mat');
  IJIMStaticMatList.Add('potshardbk.mat');
  IJIMStaticMatList.Add('obj_shs_4key_lady.mat');
  IJIMStaticMatList.Add('gearbridge4.mat');
  IJIMStaticMatList.Add('gearcsmoon4.mat');
  IJIMStaticMatList.Add('bucket_wood_hndl.mat');
  IJIMStaticMatList.Add('bucket_wood_bot.mat');
  IJIMStaticMatList.Add('bucket_wood_sde.mat');
  IJIMStaticMatList.Add('flpetalout.mat');
  IJIMStaticMatList.Add('gen_herb_stem.mat');
  IJIMStaticMatList.Add('gen_4herb_leaf.mat');
  IJIMStaticMatList.Add('gen_a4sprite_breath.mat');
  IJIMStaticMatList.Add('gen_4sfx_bazooka_exhaust.mat');
  IJIMStaticMatList.Add('rckt_a4bzoka_fire.mat');
  IJIMStaticMatList.Add('sign_dngr_frame.mat');
  IJIMStaticMatList.Add('gen_icon_bite_bot.mat');
  IJIMStaticMatList.Add('aet_4sprite_glow_fairy_dust.mat');
  IJIMStaticMatList.Add('obj_vol_4key_sharkgate.mat');
  IJIMStaticMatList.Add('gen_chicken_leg.mat');
  IJIMStaticMatList.Add('gen_4chicken.mat');
  IJIMStaticMatList.Add('gen_a4weap_fire.mat');
  IJIMStaticMatList.Add('wmgem4.mat');
  IJIMStaticMatList.Add('gbtorsfr.mat');
  IJIMStaticMatList.Add('gbtorsbk.mat');
  IJIMStaticMatList.Add('gen_a4_mc_exhaust.mat');
  IJIMStaticMatList.Add('iq_iconsd.mat');
  IJIMStaticMatList.Add('ziflame4.mat');
  IJIMStaticMatList.Add('gen_4sprite_glow_on_imp.mat');
  IJIMStaticMatList.Add('imp_handle.mat');
  IJIMStaticMatList.Add('gen_4icon_chalk_diamond.mat');
  IJIMStaticMatList.Add('imp5_cube4.mat');
  IJIMStaticMatList.Add('obj_aet_4gem.mat');
  IJIMStaticMatList.Add('gen_4_1947.mat');
  IJIMStaticMatList.Add('gen_4_triangle.mat');
  IJIMStaticMatList.Add('gen_4_r2d2.mat');
  IJIMStaticMatList.Add('gen_4_sankarastones.mat');
  IJIMStaticMatList.Add('gen_4_selfportrait.mat');
  IJIMStaticMatList.Add('gen_4_skull.mat');
  IJIMStaticMatList.Add('gen_4_snake.mat');
  IJIMStaticMatList.Add('gen_4_square.mat');
  IJIMStaticMatList.Add('gen_4_star.mat');
  IJIMStaticMatList.Add('gen_4_sunrise.mat');
  IJIMStaticMatList.Add('gen_4_tentacle.mat');
  IJIMStaticMatList.Add('gen_4_five.mat');
  IJIMStaticMatList.Add('gen_4_hammer&sickle.mat');
  IJIMStaticMatList.Add('gen_4_hawk.mat');
  IJIMStaticMatList.Add('gen_4_holygrail.mat');
  IJIMStaticMatList.Add('gen_4_pound.mat');
  IJIMStaticMatList.Add('gen_4_pyramid.mat');
  IJIMStaticMatList.Add('gen_4_questionmark.mat');
  IJIMStaticMatList.Add('gen_4_ark.mat');
  IJIMStaticMatList.Add('gen_4_coronadocross.mat');
  IJIMStaticMatList.Add('gen_4_eye.mat');
  IJIMStaticMatList.Add('gen_4_alien.mat');
  IJIMStaticMatList.Add('gen_4_ankh.mat');
  IJIMStaticMatList.Add('gen_4_tictactoe.mat');
  IJIMStaticMatList.Add('mirror_a_statue_back.mat');
  IJIMStaticMatList.Add('obj_1box_cash_hnge.mat');
  IJIMStaticMatList.Add('obj_box_cash_side.mat');
  IJIMStaticMatList.Add('obj_1box_cash_latch.mat');
  IJIMStaticMatList.Add('obj_box_cash_in.mat');
  IJIMStaticMatList.Add('obj_box_cash_top.mat');
  IJIMStaticMatList.Add('obj_box_cash_lid.mat');
  IJIMStaticMatList.Add('dksdred.mat');
  IJIMStaticMatList.Add('imp2red4off.mat');
  IJIMStaticMatList.Add('imp2blue4off.mat');
  IJIMStaticMatList.Add('inteeth.mat');
  IJIMStaticMatList.Add('gen_4gun_rev_inv.mat');
  IJIMStaticMatList.Add('weapon_4tokarev_sde_inv.mat');
  IJIMStaticMatList.Add('weapon_4mauser_sde_inv.mat');
  IJIMStaticMatList.Add('simonov4_inv.mat');
  IJIMStaticMatList.Add('ppsh-41_4_inv.mat');
  IJIMStaticMatList.Add('toz4_inv.mat');
  IJIMStaticMatList.Add('gen_4gun_bazooka_inv.mat');
  IJIMStaticMatList.Add('mrlegs.mat');
  IJIMStaticMatList.Add('gen_a4xplod_floater.mat');
  IJIMStaticMatList.Add('gen_a4xplod_uw.mat');
  IJIMStaticMatList.Add('gen_a4sprite_blood_grn.mat');
  IJIMStaticMatList.Add('gen_1particle_water_splash.mat');
  IJIMStaticMatList.Add('obj_gen_raft_icon_sde.mat');
  IJIMStaticMatList.Add('obj_gen_raft_icon_cap.mat');

  // Infernal Machine static models
  IJIMStatic3doList.Add('weap_gloves_inv.3do');
  IJIMStatic3doList.Add('weap_whip_inv.3do');
  IJIMStatic3doList.Add('weap_revolver_inv.3do');
  IJIMStatic3doList.Add('weap_tokarev_inv.3do');
  IJIMStatic3doList.Add('weap_simonov_inv.3do');
  IJIMStatic3doList.Add('weap_ppsh41_inv.3do');
  IJIMStatic3doList.Add('weap_mauser_inv.3do');
  IJIMStatic3doList.Add('weap_toz34_inv.3do');
  IJIMStatic3doList.Add('weap_bazooka_inv.3do');
  IJIMStatic3doList.Add('weap_grenade_inv.3do');
  IJIMStatic3doList.Add('weap_satchel_inv.3do');
  IJIMStatic3doList.Add('weap_machete_inv.3do');
  IJIMStatic3doList.Add('gen_icon_hlthsml.3do');
  IJIMStatic3doList.Add('gen_icon_hlthbig.3do');
  IJIMStatic3doList.Add('gen_icon_film.3do');
  IJIMStatic3doList.Add('gen_icon_camera.3do');
  IJIMStatic3doList.Add('gen_icon_type.3do');
  IJIMStatic3doList.Add('gen_icon_tv.3do');
  IJIMStatic3doList.Add('gen_icon_radio.3do');
  IJIMStatic3doList.Add('gen_icon_telgrf.3do');
  IJIMStatic3doList.Add('gen_icon_zippo.3do');
  IJIMStatic3doList.Add('gen_icon_chest.3do');
  IJIMStatic3doList.Add('gen_icon_exit.3do');
  IJIMStatic3doList.Add('gen_coins_gold.3do');
  IJIMStatic3doList.Add('gen_coins_slvr.3do');
  IJIMStatic3doList.Add('gen_idol_gold.3do');
  IJIMStatic3doList.Add('gen_idol_slvr.3do');
  IJIMStatic3doList.Add('gen_ingots_gold.3do');
  IJIMStatic3doList.Add('gen_ingots_slvr.3do');
  IJIMStatic3doList.Add('hand_in_ziplit.3do');
  IJIMStatic3doList.Add('hand_in_zippo.3do');
  IJIMStatic3doList.Add('weap_whip.3do');
  IJIMStatic3doList.Add('weap_whipcoil.3do');
  IJIMStatic3doList.Add('weap_revolver.3do');
  IJIMStatic3doList.Add('weap_tokarev.3do');
  IJIMStatic3doList.Add('weap_simonov.3do');
  IJIMStatic3doList.Add('weap_ppsh41.3do');
  IJIMStatic3doList.Add('weap_mauser.3do');
  IJIMStatic3doList.Add('weap_toz34.3do');
  IJIMStatic3doList.Add('weap_bazooka.3do');
  IJIMStatic3doList.Add('weap_machete.3do');
  IJIMStatic3doList.Add('holster_in_revolver.3do');
  IJIMStatic3doList.Add('holster_in_mauser.3do');
  IJIMStatic3doList.Add('holster_in_tokarev.3do');
  IJIMStatic3doList.Add('torso_in_bazooka.3do');
  IJIMStatic3doList.Add('torso_in_machete.3do');
  IJIMStatic3doList.Add('torso_in_sheath.3do');
  IJIMStatic3doList.Add('torso_in_ppsh-41.3do');
  IJIMStatic3doList.Add('torso_in_simonov.3do');
  IJIMStatic3doList.Add('torso_in_toz-34.3do');
  IJIMStatic3doList.Add('torso_shirt_bazooka.3do');
  IJIMStatic3doList.Add('torso_shirt_machete.3do');
  IJIMStatic3doList.Add('torso_shirt_sheath.3do');
  IJIMStatic3doList.Add('torso_shirt_ppsh-41.3do');
  IJIMStatic3doList.Add('torso_shirt_simonov.3do');
  IJIMStatic3doList.Add('torso_shirt_toz-34.3do');
  IJIMStatic3doList.Add('weap_whip_hip.3do');
  IJIMStatic3doList.Add('riv_raft_inflt1_inv.3do');
  IJIMStatic3doList.Add('weap_grenade.3do');
  IJIMStatic3doList.Add('gen_icon_bonusmap.3do');
  IJIMStatic3doList.Add('gen_icon_hint_on.3do');
  IJIMStatic3doList.Add('weap_satchel.3do');
  IJIMStatic3doList.Add('head_in_asound.3do');
  IJIMStatic3doList.Add('head_in_amsound.3do');
  IJIMStatic3doList.Add('head_in_osound.3do');
  IJIMStatic3doList.Add('head_in_asound_gh.3do');
  IJIMStatic3doList.Add('head_in_amsound_gh.3do');
  IJIMStatic3doList.Add('head_in_osound_gh.3do');
  IJIMStatic3doList.Add('gen_icon_chalk.3do');
  IJIMStatic3doList.Add('imp1_vibrat_inv.3do');
  IJIMStatic3doList.Add('imp1_vibrat.3do');
  IJIMStatic3doList.Add('imp1_vibrat_on.3do');
  IJIMStatic3doList.Add('imp2_invis_inv.3do');
  IJIMStatic3doList.Add('imp2_invis.3do');
  IJIMStatic3doList.Add('imp2_invis_on.3do');
  IJIMStatic3doList.Add('imp3_levit_inv.3do');
  IJIMStatic3doList.Add('imp3_levit.3do');
  IJIMStatic3doList.Add('imp3_levit_on.3do');
  IJIMStatic3doList.Add('imp4_energ_inv.3do');
  IJIMStatic3doList.Add('imp4_energ.3do');
  IJIMStatic3doList.Add('imp4_energ_on.3do');
  IJIMStatic3doList.Add('weap_mauser_fire.3do');
  IJIMStatic3doList.Add('weap_ppsh41_fire.3do');
  IJIMStatic3doList.Add('weap_revolver_fire.3do');
  IJIMStatic3doList.Add('weap_simonov_fire.3do');
  IJIMStatic3doList.Add('weap_tokarev_fire.3do');
  IJIMStatic3doList.Add('weap_toz34_fire.3do');
  IJIMStatic3doList.Add('gen_rckt_bzooka.3do');
  IJIMStatic3doList.Add('gen_ammo_mauser.3do');
  IJIMStatic3doList.Add('gen_ammo_ppsh41.3do');
  IJIMStatic3doList.Add('gen_ammo_simonov.3do');
  IJIMStatic3doList.Add('gen_ammo_tokarev.3do');
  IJIMStatic3doList.Add('gen_ammo_toz34.3do');
  IJIMStatic3doList.Add('gen_icon_help.3do');
  IJIMStatic3doList.Add('gen_gem_red.3do');
  IJIMStatic3doList.Add('gen_gem_green.3do');
  IJIMStatic3doList.Add('gen_gem_blue.3do');
  IJIMStatic3doList.Add('bab_tu_key.3do');
  IJIMStatic3doList.Add('bab_tblt_a.3do');
  IJIMStatic3doList.Add('bab_tblt_b.3do');
  IJIMStatic3doList.Add('bab_tblt_c.3do');
  IJIMStatic3doList.Add('bab_idol_mrdk.3do');
  IJIMStatic3doList.Add('riv_kit_ptch.3do');
  IJIMStatic3doList.Add('riv_cndl_awind.3do');
  IJIMStatic3doList.Add('riv_cndl_bwind.3do');
  IJIMStatic3doList.Add('riv_cndl_cwind.3do');
  IJIMStatic3doList.Add('riv_cndl_dwind.3do');
  IJIMStatic3doList.Add('riv_coin.3do');
  IJIMStatic3doList.Add('shs_key_sealdoor.3do');
  IJIMStatic3doList.Add('shs_bulb_pot.3do');
  IJIMStatic3doList.Add('shs_bulb_leafstem.3do');
  IJIMStatic3doList.Add('shs_bulb_bud.3do');
  IJIMStatic3doList.Add('shw_jar_oil.3do');
  IJIMStatic3doList.Add('shw_divkeyblue.3do');
  IJIMStatic3doList.Add('shw_divkeygold.3do');
  IJIMStatic3doList.Add('lag_key_pdlck.3do');
  IJIMStatic3doList.Add('lag_shovel_fold.3do');
  IJIMStatic3doList.Add('lag_plane_prop.3do');
  IJIMStatic3doList.Add('lag_hammer_small.3do');
  IJIMStatic3doList.Add('lag_dvice_armng.3do');
  IJIMStatic3doList.Add('lag_crnk_rsty.3do');
  IJIMStatic3doList.Add('tem_key_statue.3do');
  IJIMStatic3doList.Add('tem_key_statue_b.3do');
  IJIMStatic3doList.Add('tem_key_mnky.3do');
  IJIMStatic3doList.Add('tem_sharkey.3do');
  IJIMStatic3doList.Add('teo_key_water.3do');
  IJIMStatic3doList.Add('teo_idol_bird.3do');
  IJIMStatic3doList.Add('teo_idol_fish.3do');
  IJIMStatic3doList.Add('teo_idol_jag.3do');
  IJIMStatic3doList.Add('teo_mirror.3do');
  IJIMStatic3doList.Add('pyr_minecar_whl.3do');
  IJIMStatic3doList.Add('pyr_gem_eye.3do');
  IJIMStatic3doList.Add('pyr_watch.3do');
  IJIMStatic3doList.Add('pyr_ngin_chain.3do');
  IJIMStatic3doList.Add('bab_jcan_gas.3do');
  IJIMStatic3doList.Add('pyr_kndlng_cone.3do');
  IJIMStatic3doList.Add('sol_oilcan_oz.3do');
  IJIMStatic3doList.Add('sol_fuse_rplc.3do');
  IJIMStatic3doList.Add('sol_gem_a.3do');
  IJIMStatic3doList.Add('sol_gem_b.3do');
  IJIMStatic3doList.Add('sol_gem_c.3do');
  IJIMStatic3doList.Add('sol_pymd_key.3do');
  IJIMStatic3doList.Add('gen_lever_stick.3do');
  IJIMStatic3doList.Add('nub_anubis_arm.3do');
  IJIMStatic3doList.Add('nub_gear_turtle.3do');
  IJIMStatic3doList.Add('nub_key_elvtr.3do');
  IJIMStatic3doList.Add('nub_rb_eye.3do');
  IJIMStatic3doList.Add('tem_pikes_floor.3do');
  IJIMStatic3doList.Add('pru_idol.3do');
  IJIMStatic3doList.Add('pru_key_sqr.3do');
  IJIMStatic3doList.Add('iq_icon.3do');
  IJIMStatic3doList.Add('gen_icon_bite.3do');
  IJIMStatic3doList.Add('vol_tram_wheel.3do');
  IJIMStatic3doList.Add('hand_ir_lpad.3do');
  IJIMStatic3doList.Add('hand_ir_rpad.3do');
  IJIMStatic3doList.Add('sol_key_oil.3do');
  IJIMStatic3doList.Add('shs_plank_2m.3do');
  IJIMStatic3doList.Add('vol_key_cuffs.3do');
  IJIMStatic3doList.Add('aet_marduk_key.3do');
  IJIMStatic3doList.Add('inf_robo_head.3do');
  IJIMStatic3doList.Add('gen_icon_crnk.3do');
  IJIMStatic3doList.Add('cyn_potshard.3do');
  IJIMStatic3doList.Add('shs_key_lady.3do');
  IJIMStatic3doList.Add('gen_bronz_key.3do');
  IJIMStatic3doList.Add('bab_gear_elev.3do');
  IJIMStatic3doList.Add('vol_key_sharkgate.3do');
  IJIMStatic3doList.Add('imp5_dial_inv.3do');
  IJIMStatic3doList.Add('imp5_dial.3do');
  IJIMStatic3doList.Add('imp5_dial_on.3do');
  IJIMStatic3doList.Add('gen_bucket_wood.3do');
  IJIMStatic3doList.Add('weap_machete_sheath.3do');
  IJIMStatic3doList.Add('gen_herb_small.3do');
  IJIMStatic3doList.Add('gen_herb_big.3do');
  IJIMStatic3doList.Add('head_in_goodhair.3do');
  IJIMStatic3doList.Add('hand_in_hat.3do');
  IJIMStatic3doList.Add('gen_chicken.3do');
  IJIMStatic3doList.Add('torso_guy_bazooka.3do');
  IJIMStatic3doList.Add('torso_guy_machete.3do');
  IJIMStatic3doList.Add('torso_guy_ppsh-41.3do');
  IJIMStatic3doList.Add('torso_guy_sheath.3do');
  IJIMStatic3doList.Add('torso_guy_simonov.3do');
  IJIMStatic3doList.Add('torso_guy_toz-34.3do');
  IJIMStatic3doList.Add('gen_tracer.3do');
  IJIMStatic3doList.Add('aet_gem04.3do');
  IJIMStatic3doList.Add('riv_box_cash.3do');
  IJIMStatic3doList.Add('shw_divkeyred.3do');
  IJIMStatic3doList.Add('weap_rocket_pack.3do');
  IJIMStatic3doList.Add('weap_grenade_pack.3do');
  IJIMStatic3doList.Add('weap_satchel_pack.3do');
  IJIMStatic3doList.Add('aet_mirror.3do');
  IJIMStatic3doList.Add('hand_in_mirror.3do');

  // Infernal Machine static sprites
  IJIMStaticSprList.Add('pistol_hit.spr');
  IJIMStaticSprList.Add('pistol_smoke1.spr');
  IJIMStaticSprList.Add('pistol_smoke2.spr');
  IJIMStaticSprList.Add('gen_bloodsplort.spr');
  IJIMStaticSprList.Add('bazooka_smoke.spr');
  IJIMStaticSprList.Add('gen_explode3.spr');
  IJIMStaticSprList.Add('zippo.spr');
  IJIMStaticSprList.Add('bubbles.spr');
  IJIMStaticSprList.Add('waterripple.spr');
  IJIMStaticSprList.Add('x1947.spr');
  IJIMStaticSprList.Add('xalien.spr');
  IJIMStaticSprList.Add('xankh.spr');
  IJIMStaticSprList.Add('xark.spr');
  IJIMStaticSprList.Add('xcccr.spr');
  IJIMStaticSprList.Add('xcheck.spr');
  IJIMStaticSprList.Add('xcircle.spr');
  IJIMStaticSprList.Add('xcross.spr');
  IJIMStaticSprList.Add('xeye.spr');
  IJIMStaticSprList.Add('xfive.spr');
  IJIMStaticSprList.Add('xgrail.spr');
  IJIMStaticSprList.Add('xguy.spr');
  IJIMStaticSprList.Add('xhappy.spr');
  IJIMStaticSprList.Add('xhawk.spr');
  IJIMStaticSprList.Add('xhere.spr');
  IJIMStaticSprList.Add('xindy.spr');
  IJIMStaticSprList.Add('xindy2.spr');
  IJIMStaticSprList.Add('xla.spr');
  IJIMStaticSprList.Add('xlove.spr');
  IJIMStaticSprList.Add('xportrait.spr');
  IJIMStaticSprList.Add('xpound.spr');
  IJIMStaticSprList.Add('xpyramid.spr');
  IJIMStaticSprList.Add('xquestion.spr');
  IJIMStaticSprList.Add('xr2d2.spr');
  IJIMStaticSprList.Add('xskull.spr');
  IJIMStaticSprList.Add('xsnake.spr');
  IJIMStaticSprList.Add('xsquare.spr');
  IJIMStaticSprList.Add('xstar.spr');
  IJIMStaticSprList.Add('xstones.spr');
  IJIMStaticSprList.Add('xsunrise.spr');
  IJIMStaticSprList.Add('xtentacle.spr');
  IJIMStaticSprList.Add('xtictactoe.spr');
  IJIMStaticSprList.Add('xtriangle.spr');
  IJIMStaticSprList.Add('xusa.spr');
  IJIMStaticSprList.Add('xmark.spr');
  IJIMStaticSprList.Add('bazooka_exhaust.spr');
  IJIMStaticSprList.Add('bazooka_expblast.spr');
  IJIMStaticSprList.Add('bazooka_expfire.spr');
  IJIMStaticSprList.Add('bazooka_expsmoke.spr');
  IJIMStaticSprList.Add('bazooka_rocket.spr');
  IJIMStaticSprList.Add('imp1_fire.spr');
  IJIMStaticSprList.Add('imp1_glow.spr');
  IJIMStaticSprList.Add('imp2_fire.spr');
  IJIMStaticSprList.Add('imp2_glow.spr');
  IJIMStaticSprList.Add('imp3_glow.spr');
  IJIMStaticSprList.Add('imp4_glow.spr');
  IJIMStaticSprList.Add('imp5_glow.spr');
  IJIMStaticSprList.Add('breath.spr');
  IJIMStaticSprList.Add('exhaust.spr');
  IJIMStaticSprList.Add('aet_expfloater.spr');
  IJIMStaticSprList.Add('uw_explode.spr');
  IJIMStaticSprList.Add('gen_vulcansplort.spr');

  // Infernal Machine static keyframes
  IJIMStaticKeyList.Add('in_stand.key');
  IJIMStaticKeyList.Add('in_walk_fwd.key');
  IJIMStaticKeyList.Add('in_run_fwd.key');
  IJIMStaticKeyList.Add('in_walk_back.key');
  IJIMStaticKeyList.Add('in_hop_back.key');
  IJIMStaticKeyList.Add('in_jump_left_roll.key');
  IJIMStaticKeyList.Add('in_jump_right_roll.key');
  IJIMStaticKeyList.Add('in_walk_left.key');
  IJIMStaticKeyList.Add('in_walk_right.key');
  IJIMStaticKeyList.Add('in_rotate_left.key');
  IJIMStaticKeyList.Add('in_rotate_right.key');
  IJIMStaticKeyList.Add('in_slide_fwd.key');
  IJIMStaticKeyList.Add('in_slide_back.key');
  IJIMStaticKeyList.Add('in_run_jump_fwd.key');
  IJIMStaticKeyList.Add('in_jump_anticipate.key');
  IJIMStaticKeyList.Add('in_jump_up.key');
  IJIMStaticKeyList.Add('in_jump_fwd.key');
  IJIMStaticKeyList.Add('in_jump_fall.key');
  IJIMStaticKeyList.Add('in_die_buckle.key');
  IJIMStaticKeyList.Add('in_die_flung.key');
  IJIMStaticKeyList.Add('in_reach_low.key');
  IJIMStaticKeyList.Add('in_pushpull_anticipate.key');
  IJIMStaticKeyList.Add('in_push_block.key');
  IJIMStaticKeyList.Add('in_pull_block.key');
  IJIMStaticKeyList.Add('in_pull_up.key');
  IJIMStaticKeyList.Add('in_hang_idle.key');
  IJIMStaticKeyList.Add('in_shimmy_left.key');
  IJIMStaticKeyList.Add('in_shimmy_right.key');
  IJIMStaticKeyList.Add('in_climb_on.key');
  IJIMStaticKeyList.Add('in_climb_idle.key');
  IJIMStaticKeyList.Add('in_climb_up.key');
  IJIMStaticKeyList.Add('in_climb_down.key');
  IJIMStaticKeyList.Add('in_climb_left.key');
  IJIMStaticKeyList.Add('in_climb_right.key');
  IJIMStaticKeyList.Add('in_clmb_pull_onto_ledge.key');
  IJIMStaticKeyList.Add('in_whipclimb_on.key');
  IJIMStaticKeyList.Add('in_whipclimb_idle.key');
  IJIMStaticKeyList.Add('in_whipclimb_up.key');
  IJIMStaticKeyList.Add('in_whipclimb_down.key');
  IJIMStaticKeyList.Add('in_whipclimb_rotl.key');
  IJIMStaticKeyList.Add('in_whipclimb_rotr.key');
  IJIMStaticKeyList.Add('in_jump_off.key');
  IJIMStaticKeyList.Add('in_attack_whip_strut.key');
  IJIMStaticKeyList.Add('in_whipswing.key');
  IJIMStaticKeyList.Add('in_mount.key');
  IJIMStaticKeyList.Add('in_climb_onto_2m.key');
  IJIMStaticKeyList.Add('in_jump_backflip.key');
  IJIMStaticKeyList.Add('in_jump_fwdroll.key');
  IJIMStaticKeyList.Add('in_jump_up_land.key');
  IJIMStaticKeyList.Add('in_run_jumpl.key');
  IJIMStaticKeyList.Add('in_run_jumpr.key');
  IJIMStaticKeyList.Add('in_activate_medium.key');
  IJIMStaticKeyList.Add('in_reach_high.key');
  IJIMStaticKeyList.Add('in_attack_pull_fists.key');
  IJIMStaticKeyList.Add('in_attack_put_fists.key');
  IJIMStaticKeyList.Add('in_attack_fistl_hook.key');
  IJIMStaticKeyList.Add('in_attack_fistl_jab.key');
  IJIMStaticKeyList.Add('in_attack_fistr_straight.key');
  IJIMStaticKeyList.Add('in_attack_fistr_upper.key');
  IJIMStaticKeyList.Add('in_stand_bd_crawl.key');
  IJIMStaticKeyList.Add('in_climb_down_onto_wall.key');
  IJIMStaticKeyList.Add('in_climb_down_into_hang.key');
  IJIMStaticKeyList.Add('in_attack_pull_whip.key');
  IJIMStaticKeyList.Add('in_attack_aim_whip.key');
  IJIMStaticKeyList.Add('in_attack_put_whip.key');
  IJIMStaticKeyList.Add('in_attack_fire_whip.key');
  IJIMStaticKeyList.Add('in_attack_unravel_whip.key');
  IJIMStaticKeyList.Add('in_attack_ravel_whip.key');
  IJIMStaticKeyList.Add('in_attack_pull_gun.key');
  IJIMStaticKeyList.Add('in_attack_aim_gun.key');
  IJIMStaticKeyList.Add('in_attack_put_gun.key');
  IJIMStaticKeyList.Add('in_attack_fire_gun.key');
  IJIMStaticKeyList.Add('in_attack_pull_rifle.key');
  IJIMStaticKeyList.Add('in_attack_aim_rifle.key');
  IJIMStaticKeyList.Add('in_attack_put_rifle.key');
  IJIMStaticKeyList.Add('in_attack_fire_rifle.key');
  IJIMStaticKeyList.Add('in_attack_unaim_rifle.key');
  IJIMStaticKeyList.Add('in_attack_aim_shotgun.key');
  IJIMStaticKeyList.Add('in_attack_fire_shotgun.key');
  IJIMStaticKeyList.Add('in_attack_fire_submachine.key');
  IJIMStaticKeyList.Add('in_attack_unaim_shotgun.key');
  IJIMStaticKeyList.Add('in_attack_pull_bazooka.key');
  IJIMStaticKeyList.Add('in_attack_put_bazooka.key');
  IJIMStaticKeyList.Add('in_attack_fire_bazooka.key');
  IJIMStaticKeyList.Add('in_attack_pull_grenade.key');
  IJIMStaticKeyList.Add('in_attack_aim_grenade.key');
  IJIMStaticKeyList.Add('in_attack_put_grenade.key');
  IJIMStaticKeyList.Add('in_attack_fire_grenade.key');
  IJIMStaticKeyList.Add('in_attack_pull_satchel.key');
  IJIMStaticKeyList.Add('in_attack_put_satchel.key');
  IJIMStaticKeyList.Add('in_attack_fire_satchel.key');
  IJIMStaticKeyList.Add('in_attack_pull_machete.key');
  IJIMStaticKeyList.Add('in_attack_put_machete.key');
  IJIMStaticKeyList.Add('in_attack_fire_macheter.key');
  IJIMStaticKeyList.Add('in_attack_fire_machetel.key');
  IJIMStaticKeyList.Add('in_pull_zippo.key');
  IJIMStaticKeyList.Add('in_put_zippo.key');
  IJIMStaticKeyList.Add('in_swim_float.key');
  IJIMStaticKeyList.Add('in_swim_fwd.key');
  IJIMStaticKeyList.Add('in_uw_swim.key');
  IJIMStaticKeyList.Add('in_swim_back.key');
  IJIMStaticKeyList.Add('in_swim_rotate_l.key');
  IJIMStaticKeyList.Add('in_swim_rotate_r.key');
  IJIMStaticKeyList.Add('in_uw_float.key');
  IJIMStaticKeyList.Add('in_uw_die.key');
  IJIMStaticKeyList.Add('in_uw_pickup.key');
  IJIMStaticKeyList.Add('in_swim_pullup.key');
  IJIMStaticKeyList.Add('in_submerge.key');
  IJIMStaticKeyList.Add('in_uw_pullswitch.key');
  IJIMStaticKeyList.Add('in_uw_attack_put_fists.key');
  IJIMStaticKeyList.Add('in_uw_attack_put_whip.key');
  IJIMStaticKeyList.Add('in_uw_attack_pull_machete.key');
  IJIMStaticKeyList.Add('in_uw_attack_put_machete.key');
  IJIMStaticKeyList.Add('in_uw_attack_fire_machete.key');
  IJIMStaticKeyList.Add('in_uw_attack_put_bazooka.key');
  IJIMStaticKeyList.Add('in_uw_attack_put_grenade.key');
  IJIMStaticKeyList.Add('in_uw_attack_put_gun.key');
  IJIMStaticKeyList.Add('in_uw_attack_put_imp.key');
  IJIMStaticKeyList.Add('in_uw_attack_put_satchel.key');
  IJIMStaticKeyList.Add('in_uw_attack_put_mirror.key');
  IJIMStaticKeyList.Add('in_uw_put_zippo.key');
  IJIMStaticKeyList.Add('in_crawl_idle.key');
  IJIMStaticKeyList.Add('in_crawl_fwd.key');
  IJIMStaticKeyList.Add('in_crawl_back.key');
  IJIMStaticKeyList.Add('in_crawl_rotatel.key');
  IJIMStaticKeyList.Add('in_crawl_rotater.key');
  IJIMStaticKeyList.Add('in_crawl_bd_stand.key');
  IJIMStaticKeyList.Add('in_crawl_activate.key');
  IJIMStaticKeyList.Add('in_chalk.key');
  IJIMStaticKeyList.Add('in_hit_wall_soft.key');
  IJIMStaticKeyList.Add('in_hit_wall_hard.key');
  IJIMStaticKeyList.Add('in_jump_up_grab.key');
  IJIMStaticKeyList.Add('in_slide_fwd_recover.key');
  IJIMStaticKeyList.Add('in_stand_bd_walk.key');
  IJIMStaticKeyList.Add('in_walk_bd_stand.key');
  IJIMStaticKeyList.Add('in_attack_pull_imp.key');
  IJIMStaticKeyList.Add('in_attack_put_imp.key');
  IJIMStaticKeyList.Add('in_imp3_float.key');
  IJIMStaticKeyList.Add('in_pickup_low.key');
  IJIMStaticKeyList.Add('in_pickup_med.key');
  IJIMStaticKeyList.Add('in_swim_mount.key');
  IJIMStaticKeyList.Add('in_rotate90_left.key');
  IJIMStaticKeyList.Add('in_rotate90_right.key');
  IJIMStaticKeyList.Add('in_attack_pull_mirror.key');
  IJIMStaticKeyList.Add('in_attack_put_mirror.key');
  IJIMStaticKeyList.Add('in_pickup_dark.key');
  IJIMStaticKeyList.Add('1h_stand.key');
  IJIMStaticKeyList.Add('1h_stand_idle.key');
  IJIMStaticKeyList.Add('1h_walk_fwd.key');
  IJIMStaticKeyList.Add('1h_walk_back.key');
  IJIMStaticKeyList.Add('1h_run_fwd.key');
  IJIMStaticKeyList.Add('1h_rotate_left.key');
  IJIMStaticKeyList.Add('1h_rotate_right.key');
  IJIMStaticKeyList.Add('1h_die_roll.key');
  IJIMStaticKeyList.Add('1h_die_forward.key');
  IJIMStaticKeyList.Add('1h_die_flung.key');
  IJIMStaticKeyList.Add('1h_getup.key');
  IJIMStaticKeyList.Add('1h_attack_pull_fists.key');
  IJIMStaticKeyList.Add('1h_attack_fistl_hook.key');
  IJIMStaticKeyList.Add('1h_attack_fistl_jab.key');
  IJIMStaticKeyList.Add('1h_attack_fistr_straight.key');
  IJIMStaticKeyList.Add('1h_attack_fistr_upper.key');
  IJIMStaticKeyList.Add('1h_attack_pull_gun.key');
  IJIMStaticKeyList.Add('1h_attack_aim_gun.key');
  IJIMStaticKeyList.Add('1h_attack_fire_gun.key');
  IJIMStaticKeyList.Add('1h_attack_fire_grenade.key');
  IJIMStaticKeyList.Add('1h_roll_left.key');
  IJIMStaticKeyList.Add('1h_roll_right.key');
  IJIMStaticKeyList.Add('2h_stand.key');
  IJIMStaticKeyList.Add('2h_stand_idle.key');
  IJIMStaticKeyList.Add('2h_walk_fwd.key');
  IJIMStaticKeyList.Add('2h_walk_back.key');
  IJIMStaticKeyList.Add('2h_run_fwd.key');
  IJIMStaticKeyList.Add('2h_rotate_left.key');
  IJIMStaticKeyList.Add('2h_rotate_right.key');
  IJIMStaticKeyList.Add('2h_die_roll.key');
  IJIMStaticKeyList.Add('2h_die_forward.key');
  IJIMStaticKeyList.Add('2h_die_flung.key');
  IJIMStaticKeyList.Add('2h_getup.key');
  IJIMStaticKeyList.Add('2h_attack_pull_fists.key');
  IJIMStaticKeyList.Add('2h_attack_fistl_hook.key');
  IJIMStaticKeyList.Add('2h_attack_fistl_jab.key');
  IJIMStaticKeyList.Add('2h_attack_fistr_straight.key');
  IJIMStaticKeyList.Add('2h_attack_fistr_upper.key');
  IJIMStaticKeyList.Add('2h_attack_pull_rifle.key');
  IJIMStaticKeyList.Add('2h_attack_aim_rifle.key');
  IJIMStaticKeyList.Add('2h_attack_unaim_rifle.key');
  IJIMStaticKeyList.Add('2h_attack_fire_rifle.key');
  IJIMStaticKeyList.Add('2h_attack_aim_shotgun.key');
  IJIMStaticKeyList.Add('2h_attack_unaim_shotgun.key');
  IJIMStaticKeyList.Add('2h_attack_fire_shotgun.key');
  IJIMStaticKeyList.Add('2h_attack_fire_submachine.key');
  IJIMStaticKeyList.Add('2h_roll_left.key');
  IJIMStaticKeyList.Add('2h_roll_right.key');
  IJIMStaticKeyList.Add('gen_chicken.key');
  IJIMStaticKeyList.Add('wh_attack_whip_fire.key');
  IJIMStaticKeyList.Add('wh_attack_whip_unravel.key');
  IJIMStaticKeyList.Add('wh_attack_whip_ravel.key');
  IJIMStaticKeyList.Add('wh_whipclimb_on.key');
  IJIMStaticKeyList.Add('wh_whipclimb_idle.key');
  IJIMStaticKeyList.Add('wh_whipswing.key');
  IJIMStaticKeyList.Add('wc_stand.key');
  IJIMStaticKeyList.Add('wc_jump_off.key');
  IJIMStaticKeyList.Add('in_climb_down_onto_angledwall.key');

  // Infernal Machine COG scripts
  IJIMStaticCogScriptList.Add('item_pickuplines.cog');
  IJIMStaticCogScriptList.Add('item_healthbig.cog');
  IJIMStaticCogScriptList.Add('item_healthsml.cog');
  IJIMStaticCogScriptList.Add('item_yerbisma.cog');
  IJIMStaticCogScriptList.Add('item_yerbita.cog');
  IJIMStaticCogScriptList.Add('item_poisonkit.cog');
  IJIMStaticCogScriptList.Add('weap_zippo.cog');
  IJIMStaticCogScriptList.Add('weap_fists.cog');
  IJIMStaticCogScriptList.Add('weap_whip.cog');
  IJIMStaticCogScriptList.Add('weap_pistol.cog');
  IJIMStaticCogScriptList.Add('weap_tokarev.cog');
  IJIMStaticCogScriptList.Add('weap_mauser.cog');
  IJIMStaticCogScriptList.Add('weap_simonov.cog');
  IJIMStaticCogScriptList.Add('weap_submachine.cog');
  IJIMStaticCogScriptList.Add('weap_shotgun.cog');
  IJIMStaticCogScriptList.Add('weap_bazooka.cog');
  IJIMStaticCogScriptList.Add('weap_machete.cog');
  IJIMStaticCogScriptList.Add('weap_grenade.cog');
  IJIMStaticCogScriptList.Add('weap_satchel.cog');
  IJIMStaticCogScriptList.Add('weap_imp1.cog');
  IJIMStaticCogScriptList.Add('weap_imp2.cog');
  IJIMStaticCogScriptList.Add('weap_imp3.cog');
  IJIMStaticCogScriptList.Add('weap_imp4.cog');
  IJIMStaticCogScriptList.Add('weap_imp5.cog');
  IJIMStaticCogScriptList.Add('weap_mirror.cog');
  IJIMStaticCogScriptList.Add('com_fists.cog');
  IJIMStaticCogScriptList.Add('com_tokarev.cog');
  IJIMStaticCogScriptList.Add('com_mauser.cog');
  IJIMStaticCogScriptList.Add('com_simonov.cog');
  IJIMStaticCogScriptList.Add('com_submachine.cog');
  IJIMStaticCogScriptList.Add('com_shotgun.cog');
  IJIMStaticCogScriptList.Add('cheat_allweapons.cog');
  IJIMStaticCogScriptList.Add('cheat_whatailsya.cog');

  // DF2 Templates
  StaticTemplates[Integer(TProjectType.JKDF2)] := TArray<String>.Create(
    '+twinkle',
    '+sspks_wall',
    '+ssparks_wall',
    '+ssparks_blood',
    '+sspks_saber',
    '+ssparks_saber',
    '+backpack',
    'ghostcam'
  );

  StaticTemplates[Integer(TProjectType.MOTS)] := TArray<String>.Create(
    '+twinkle',
    '+sspks_wall',
    '+ssparks_wall',
    '+ssparks_blood',
    '+sspks_saber',
    '+ssparks_saber',
    '+backpack',
    'ghostcam'
  );

  StaticTemplates[Integer(TProjectType.IJIM)] := TArray<String>.Create(
    // objects
    'ghost',
    'spritlemist',
    'paddlesplash',
    'minecarsparks',

    // chalk marks
    '+1947_mark',
    '+alien_mark',
    '+ankh_mark',
    '+ark_mark',
    '+cccr_mark',
    '+check_mark',
    '+cir_mark',
    '+cross_mark',
    '+eye_mark',
    '+five_mark',
    '+grail_mark',
    '+guy_mark',
    '+happy_mark',
    '+hawk_mark',
    '+here_mark',
    '+indy2_mark',
    '+indy_mark',
    '+la_mark',
    '+love_mark',
    '+portrait_mark',
    '+pound_mark',
    '+pyramid_mark',
    '+question_mark',
    '+r2d2_mark',
    '+skull_mark',
    '+snake_mark',
    '+square_mark',
    '+star_mark',
    '+stones_mark',
    '+sunrise_mark',
    '+tentacle_mark',
    '+tictactoe_mark',
    '+triangle_mark',
    '+usa_mark',
    '+x_mark',

    // weapons
    '+fists_hit',
    '+fists',
    '+whip',
    '+whip_actor',
    '+whipc_actor',
    '+pistol_hit',
    '+pistol',
    '+pis_spr_hit',
    '+pis_spr_smoke1',
    '+pis_spr_smoke2',
    '+pistol_smoke1',
    '+pistol_smoke2',
    '+tokarev',
    '+mauser',
    '+simonov',
    '+machete_hit',
    '+machete',
    '+submachine',
    '+shotgun',
    '+bazooka_exp_smoke',
    '+bazooka_exp_fire',
    '+bazooka_exp_debris',
    '+bazooka_exp',
    '+bazooka',
    '+bazooka_chicken',
    '+baz_spr_exhaust',
    '+bazooka_exhaust',
    '+baz_spr_exp_blast',
    '+baz_spr_exp_fire',
    '+baz_spr_exp_smoke',
    '+baz_spr_smoke',
    '+bazooka_smoke',
    '+baz_spr_rocket',
    '+grenade',
    '+satchel',

    // infernal machine parts
    '+imp1',
    '+imp1_glow',
    '+imp1_spr_fire',
    '+imp1_fire',
    '+imp2_glow',
    '+imp2_dust',
    '+imp2_trail',
    '+imp3_glow',
    '+imp4',
    '+imp4_glow',
    '+imp5',
    '+imp5_glow',

    // fx
    //'+backpack',
    '+plcog',
    '+plcogend',
    '+laserbeam',
    '+raft_splash',
    '+lightning',
    'zippo_glow',
    '+exhaust',
    '+bubbles',
    '+ripples',
    '+spray',
    '+bloodsplort',
    '+vulcansplort',
    '+raft_wake',
    '+mardukhit',
    '+uw_spr_blast',
    '+uw_spr_explode',
    '+uw_explode',
    '+uw_blast'
  );

end;

{$I level_io.inc}
{$I DF_import.inc}
{$I Savejkl.inc}
{$I asc_import.inc}

Initialization

begin
  // Create static lists.
  // Note, must not be sorted
  IJIMStaticSoundList     := TStringList.Create(dupIgnore, false, false);
  IJIMStaticMatList       := TStringList.Create(dupIgnore, false, false);
  IJIMStatic3doList       := TStringList.Create(dupIgnore, false, false);
  IJIMStaticSprList       := TStringList.Create(dupIgnore, false, false);
  IJIMStaticKeyList       := TStringList.Create(dupIgnore, false, false);
  IJIMStaticCogScriptList := TStringList.Create(dupIgnore, false, false);

  level := TJKLevel.Create;
  InitStaticResources();
end;

Finalization

begin
  level.Clear;
  level.Free;

  IJIMStaticCogScriptList.Free;
  IJIMStaticKeyList.Free;
  IJIMStaticSprList.Free;
  IJIMStatic3doList.Free;
  IJIMStaticMatList.Free;
  IJIMStaticSoundList.Free;
end;

end.
