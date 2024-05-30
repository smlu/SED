unit u_undo;

interface

uses J_Level, misc_utils, SysUtils, Classes, GlobalVars,
     values, u_3dos, Geometry;

{$A-} // no alignment

const
{Undo change constants}
ch_added   = 0;
ch_deleted = 1;
ch_changed = 2;

{Sector change constants}
sc_geo = 4;
sc_val = 8;
sc_both = sc_geo + sc_val;

Type
PWord = ^Word;
PInt  = ^SmallInt;

TVXRec = record
  x,y,z: double;
end;
PVXRec = ^TVXRec;

TSFVX = record
  nvx: word;
  u,v: single;
  color: TColorF;
  //  intensity:single;
  //  r,g,b:single;
end;
PSFVX = ^TSFVX;

TSecRec = record
  flags: longint;
  ambient: TColorF;
  extraLight: TColorF;
  pointLight: TPointLight;
  colormap: array[0..31] of char;
  tint: TColorF;
  Sound: array[0..63] of char;
  soundVolume: double;
  thrust: TVector;
  layer: Integer;
  nvxs,nsfs: Integer;
end;
PSecRec = ^TSecRec;

TSurfRec = record
  adjSC,adjSF: Integer;
  adjoinflags: Longint;
  material: array[0..63] of char;
  surfflags, faceflags: Longint;
  geo, light, tex: Integer;
  extraLight: TColorF;
  uscale, vscale: single;
  nvxs: Integer;
  adjID: Integer;
end;
PSurfRec = ^TSurfRec;

TLightRec = record
  flags: longint;
  layer: Integer;
  position: TVector;
  color: TColorF;
  intensity: double;
  range: double;
end;

TThingRec = record
  sec: Integer;
  name: array[0..63] of char;
  x,y,z: double;
  PCH,YAW,ROL: Double;
  layer: Integer;
  vals:array[0..0] of char;
end;
PThingRec = ^TThingRec;

TCogRec = record
  name: array[0..63] of char;
  vals: array[0..255] of char;
  {vals:TCOGValues;}
end;

TURQRec = record
  nodename: array[0..63] of char;
  x,y,z: double;
  pch,yaw,rol: double;
  ntype: Integer;
  parent: Integer;
end;

Procedure StartUndoRec(const name: string);
Procedure SaveSecUndo(sec: TJKSector; change: Integer; how: Integer);
Procedure SaveThingUndo(th: TJKThing; change: Integer);
Procedure SaveLightUndo(lt: TSedLight; change: Integer);
Procedure SaveCOGUndo(cog: TCOG);
Procedure SaveNodeUndo(node: THnode; change: Integer);
Procedure ClearNodesUndo;

Procedure EndUndoRec;
Function GetUndoRecName: string;
Procedure ApplyUndo;
Procedure ClearUndoBuffer;

Procedure GetLight(light: TSedLight; var lrec: TLightRec);
Procedure SetLight(light: TSedLight; var lrec: TLightRec);
Function GetLightRecSize: Integer;

Function GetThingRecSize(thing: TJKThing): Integer;
Function GetThing(thing: TJKThing; trec: PThingRec): Integer;
Function SetThing(thing: TJKThing; trec: PThingRec): Integer;

Function GetThingValSize(thing: TJKThing): Integer;
Function GetThingVals(thing: TJKThing; pvals: PChar): Integer;
Function SetThingVals(thing: TJKThing; pvals: PChar): Integer;

Function GetCOGRecRecSize: Integer;
Procedure GetCOG(cog: TCog; var crec: TCogRec);
Procedure SetCOG(cog: TCog; var crec: TCogRec);

Function GetSecRecSize: Integer;
Procedure GetSec(sec: TJKSector; var srec: TSecRec);
Procedure SetSec(var sec: TJKSector; srec: TSecRec);
Procedure GetSurf(surf: TJKSurface; var sfrec: TSurfRec);
Function GetSurfRecSize: Integer;
Procedure SetSurf(surf: TJKSurface; var sfrec: TSurfRec);
Function SCVertSize(vs: TJKVertices): Integer;
Procedure GetSCVertices(vs: TJKVertices; vxrecs: PVXRec);
Procedure SetSCVertices(vs: TJKVertices; nvxs: Integer; vxrecs: PVXRec);
Function SFVertSize(vs: TJKVertices): Integer;
Procedure GetSFVertices(surf: TJKSurface; pvxs: PSFVX);
Procedure SetSFVertices(sec: TJKSector; surf: TJKSUrface; nvxs: Integer; pvxs: PSFVX);

Function GetURQRecSize: Integer;
Procedure GetURQRec(node: THNode; var nrec: TURQRec);
Procedure SetURQRec(node: THNode; var nrec: TURQRec);

implementation

uses Jed_main, lev_utils, u_3doform, System.Math;

Type

TUndoChange = class
  change: Integer; {ch_ constants}
  data: pointer; {undo data}
  Procedure SaveUndo(obj: TObject; ch:Integer); virtual;abstract;
  procedure Undo(objid: Integer); virtual;abstract;
  Destructor Destroy; override;
end;


TLightUndo = class(TUndoChange)
  Procedure SaveUndo(obj: TObject; ch: Integer); override;
  procedure Undo(objid: Integer); override;
end;

TThingUndo = class(TUndoChange)
  Procedure SaveUndo(obj: TObject; ch: Integer); override;
  procedure Undo(objid: Integer); override;
end;

TSecValUndo = class(TUndoChange)
  nsfs: integer;
  Procedure SaveUndo(obj: TObject;ch: Integer); override;
  procedure Undo(objid: Integer); override;
end;

TSecGeoUndo = class(TUndoChange)
  nvxs,nsfs: Integer;
  Procedure SaveUndo(obj: TObject; ch: Integer); override;
  procedure Undo(objid: Integer); override;
end;

TURQUndo = class(TUndoChange)
  Procedure SaveUndo(obj: TObject; ch: Integer); override;
  procedure Undo(objid: Integer); override;
end;

TObjList = Class(TList)
  ids: TIntList;
  Constructor Create;
  Destructor Destroy; override;
  Procedure AddObject(obj: TObject; objid: Integer);
  Function FindObjID(objid: integer): Integer;
  Function GetObj(n: Integer):TUndoChange;
  Function GetObjID(n: Integer): Integer;
  {  Procedure SetIdxObj(n:Integer; idxobj:TObject);}
end;

TUndoRec = class
  name:string;
  thchanges: TObjList;
  ltchanges: TObjList;
  cgchanges: TObjList;
  scvchanges: TObjList;
  scgchanges: TObjList;
  nodechanges: TObjList;
  Constructor Create;
  Destructor Destroy; override;
end;

var
  UndoStack: TList;
  curUndoRec: TUndoRec;

Destructor TUndoChange.Destroy;
begin
  if data <> nil then
    begin
      FreeMem(data);
      data := nil;
    end;
end;

Constructor TObjList.Create;
begin
  ids := TIntList.Create;
end;

Destructor TObjList.Destroy;
var i: Integer;
begin
  for i := 0 to count - 1 do
    begin
      GetObj(i).Free;
      //Delete(i);
    end;

  Clear;

  if ids <> nil then
    begin
      ids.Free;
      ids := nil;
    end;
  Inherited Destroy;
end;

Procedure TObjList.AddObject(obj: TObject; objid: Integer);
begin
  ids.Add(objid);
  Add(obj);
end;

Function TObjList.FindObjID(objid: Integer): Integer;
begin
  Result := ids.IndexOf(ObjID);
end;

Function TObjList.GetObj(n: Integer): TUndoChange;
begin
 Result := TUndoChange(Items[n]);
end;

Function TObjList.GetObjID(n: Integer): Integer;
begin
  Result := ids[n];
end;

{Procedure TObjList.SetIdxObj(n:Integer;idxobj:TObject);
begin
 idx[n]:=idxobj;
end;}

Constructor TUndoRec.Create;
begin
  thchanges  := TObjList.Create;
  ltchanges  := TObjList.Create;
  cgchanges  := TObjList.Create;
  scvchanges  := TObjList.Create;
  scgchanges  := TObjList.Create;
  nodechanges := TObjList.Create;
end;

Destructor TUndoRec.Destroy;
begin
  thchanges.Free;
  ltchanges.Free;
  cgchanges.Free;
  scvchanges.Free;
  scgchanges.Free;
  nodechanges.Free;
end;


Function GetSecRecSize: Integer;
begin
  result := sizeof(TSecRec);
end;

Procedure GetSec(sec: TJKSector; var srec: TSecRec);
begin
  with sec do
    begin
      srec.flags      := flags;
      srec.ambient    := ambient;
      srec.extraLight := extraLight;
      srec.pointLight := pointLight;
      srec.tint       := tint;

      StrLCopy(srec.colorMap, PChar(colormap), Length(srec.colormap) - 1);
      StrLCopy(srec.sound, PChar(sound), Length(srec.sound) - 1);

      srec.soundVolume := soundVolume;
      srec.thrust      := thrust;
      srec.layer       := layer;
      srec.nvxs        := vertices.count;
      srec.nsfs        := surfaces.count;
    end;
end;

Procedure SetSec(var sec: TJKSector; srec: TSecRec);
begin
  with sec do
    begin
      flags       := srec.flags;
      ambient     := srec.ambient;
      extraLight  := srec.extraLight;
      pointLight  := srec.pointLight;
      colormap    := srec.colormap;
      tint        := srec.tint;
      sound       := Srec.sound;
      soundVolume := srec.soundVolume;
      thrust      := srec.thrust;
      layer       := srec.layer;
    end;
end;

Procedure GetSurf(surf: TJKSurface; var sfrec: TSurfRec);
begin
  With surf do
    begin
      if adjoin = nil then
        begin
          sfrec.adjSC := -1;
          sfrec.adjSF := 0;
          sfrec.adjID := -1;
        end
      else
        begin
          sfrec.adjSC := adjoin.sector.num;
          sfrec.adjSF := adjoin.num;
          sfrec.adjID := adjoin.sector.id;
        end;

      FillChar(sfrec.material, SizeOf(sfrec.material), #0);
      StrLCopy(sfrec.material, PChar(material), System.Math.Min(material.Length, Length(sfrec.material) - 1));

      sfrec.adjoinflags := adjoinflags;
      sfrec.surfflags   := surfflags;
      sfrec.faceflags   := faceflags;
      sfrec.geo         := geo;
      sfrec.light       := light;
      sfrec.tex         := tex;
      sfrec.extraLight  := extralight;
      sfrec.uscale      := uscale;
      sfrec.vscale      := vscale;
      sfrec.nvxs        := vertices.Count;
    end;
end;

Function GetSurfRecSize: Integer;
begin
  Result := sizeof(TSurfRec);
end;

Procedure SetSurf(surf:TJKSurface; var sfrec:TSurfRec);
begin
 surf.mark        := sfrec.AdjSC * 65536 + sfrec.AdjSF;
 surf.nadj        := sfrec.adjID;
 surf.adjoinflags := sfrec.adjoinflags;
 surf.material    := sfrec.material;
 //SetString(surf.Material, sfrec.Material, Length(sfrec.Material));
 surf.surfflags   := sfrec.surfFlags;
 surf.faceFlags   := sfrec.faceflags;
 surf.geo         := sfrec.geo;
 surf.light       := sfrec.light;
 surf.tex         := sfrec.tex;
 surf.extraLight  := sfrec.extraLight;
 surf.uscale      := sfrec.uscale;
 surf.vscale      := sfrec.vscale;
end;

Function SCVertSize(vs:TJKVertices): Integer;
begin
  result := vs.count * sizeof(TVXRec);
end;

Procedure GetSCVertices(vs:TJKVertices;vxrecs:PVXRec);
var i: Integer;
begin
  for i:= 0 to vs.Count-1 do
    with vs[i] do
    begin
      vxrecs^.x := x;
      vxrecs^.y := y;
      vxrecs^.z := z;
      inc(vxrecs);
    end;
end;

Procedure SetSCVertices(vs: TJKVertices; nvxs: Integer; vxrecs: PVXRec);
var i: Integer;
begin
  for i:=0 to nvxs - 1 do
    with vs[i] do
      begin
        x := vxrecs^.x;
        y := vxrecs^.y;
        z := vxrecs^.z;
        inc(vxrecs);
      end;
end;


Function SFVertSize(vs: TJKVertices): Integer;
begin
 result := vs.count * sizeof(TSFVX);
end;

Procedure GetSFVertices(surf: TJKSurface; pvxs: PSFVX);
var i: Integer;
begin
  for i := 0 to surf.vertices.count - 1 do
    with surf.vertices[i], surf.txvertices[i] do
    begin
      pvxs^.nvx := surf.vertices[i].num;
      pvxs^.u := u;
      pvxs^.v := v;
      pvxs^.color.a := color.a;
      pvxs^.color.r := color.r;
      pvxs^.color.g := color.g;
      pvxs^.color.b := color.b;
      Inc(pvxs);
    end;
end;

Procedure SetSFVertices(sec: TJKSector; surf: TJKSUrface; nvxs: Integer; pvxs: PSFVX);
var i: Integer;
begin
  for i := 0 to nvxs - 1 do
    with surf.txvertices[i] do
    begin
      surf.vertices[i] := sec.vertices[pvxs^.nvx];
      u := pvxs^.u;
      v := pvxs^.v;
      color.a := pvxs^.color.a;
      color.r := pvxs^.color.r;
      color.g := pvxs^.color.g;
      color.b := pvxs^.color.b;
      Inc(pvxs);
    end;
end;

Function GetLightRecSize: Integer;
begin
 Result := Sizeof(TLightRec);
end;

Procedure GetLight(light: TSedLight; var lrec: TLightRec);
begin
  With light do
    begin
      lrec.flags     := flags;
      lrec.layer     := layer;
      lrec.color     := color;
      lrec.intensity := intensity;
      lrec.range     := range;
      lrec.position  := position;
    end;
end;

Procedure SetLight(light:TSedLight;var lrec:TLightRec);
begin
  With light do
    begin
      flags     := lrec.flags;
      layer     := lrec.layer;
      color     := lrec.color;
      intensity := lrec.intensity;
      range     := lrec.range;
      position  := lrec.position;
    end;
end;

Function GetThingRecSize(thing:TJKThing):integer;
begin
  Result := Sizeof(TThingRec) + GetThingValSize(thing);
end;

Function GetThing(thing: TJKThing; trec: PThingRec):integer;
begin
  With thing do
  begin
    if sec = nil then trec.Sec := -1 else trec.sec := Sec.num;
    StrLCopy(trec.Name, PChar(Name), Length(trec.Name) - 1);
    trec.X  :=X;
    trec.Y := Y;
    trec.Z := Z;
    trec.PCH := PCH;
    trec.YAW := YAW;
    trec.ROL := ROL;
    trec.layer := layer;
  end;
  Result := sizeof(TThingRec) + GetThingVals(thing, trec.vals);
end;

Function SetThing(thing: TJKThing; trec: PThingRec): integer;
begin
 With thing do
 begin
  {Mark := trec.sec;}
  name := trec.Name;
  X := trec.X;
  Y := trec.Y;
  Z := trec.Z;
  PCH := trec.PCH;
  YAW := trec.YAW;
  ROL := trec.ROL;
  layer := trec.layer;
 end;
 result := sizeof(TThingRec) + SetThingVals(thing, trec.vals);
end;

Function GetThingValSize(thing: TJKThing): integer;
var i:integer;
begin
  result:=0;
  for i:=0 to thing.Vals.count-1 do
  with thing.vals[i] do
    begin
      inc(result, sizeof(char)); // #32 - space char
      inc(result, ByteLength(name));
      inc(result, sizeof(char));  // =
      inc(result, ByteLength(AsString));
    end;
end;

Function GetThingVals(thing: TJKThing; pvals: Pchar): integer;
var st: string;
    i: integer;
begin
  st:='';
  for i:=0 to thing.vals.count - 1 do
    With thing.vals[i] do
      begin
        if st = '' then st := ConCat(name, '=', AsString)
        else st := ConCat(st, #32, name, '=', AsString);
      end;
  StrCopy(pvals, pchar(st));
  Result := ByteLength(st);
end;

Function SetThingVals(thing: TJKThing; pvals: PChar):integer;
var
    n,p: integer;
    v: TTPLValue;
    s,w: string;
begin
  s := pvals;
  result := ByteLength(s);
  p := 1; n := 0;
  while p <= Length(s) do
    begin
      p := GetWord(s, p, w);
      if n < thing.Vals.count then
        v := thing.vals[n]
      else
        begin
          v := TTPLValue.Create;
          thing.Vals.Add(v);
        end;
      S2TPLVal(w, v);
      inc(n);
    end;

  for p:=thing.Vals.Count-1 downto n do
    begin
      thing.vals[p].Free;
      thing.vals.Delete(p);
    end;
end;


Procedure ResolveThingRefs(thing: TJKThing; var trec: TThingRec);
begin
  if not CheckSectorIndex(level, trec.sec) then
    thing.sec := nil
  else thing.Sec := Level.Sectors[trec.sec];
end;

Function GetCOGRecRecSize:integer;
begin
 result := Sizeof(TCOGRec);
end;

Procedure GetCOG(cog:TCog;var crec:TCogRec);
var s: string;
    i: integer;
begin
  StrLCopy(crec.Name, pchar(cog.Name), Length(crec.Name) - 1);
  s := '';
  for i := 0 to cog.vals.count - 1 do
    begin
    // TODO: ...
    end;
end;

Procedure SetCOG(cog: TCog; var crec: TCogRec);
begin
  cog.name := crec.name;
end;

Function GetURQRecSize:integer;
begin
  result := sizeof(TURQRec);
end;

Procedure GetURQRec(node:THNode;var nrec:TURQRec);
begin
  StrLCopy(nrec.NodeName, PChar(node.NodeName), Length(nrec.NodeName) - 1);
  nrec.x := node.x;
  nrec.y := node.y;
  nrec.z := node.z;
  nrec.pch := node.pch;
  nrec.yaw := node.yaw;
  nrec.rol := node.rol;
  nrec.ntype := node.ntype;
  nrec.parent := node.parent;
end;

Procedure SetURQRec(node:THNode;var nrec:TURQRec);
begin
  StrLCopy(nrec.NodeName, PChar(node.NodeName), Length(nrec.NodeName) - 1);
  node.x := nrec.x;
  node.y := nrec.y;
  node.z := nrec.z;
  node.pch := nrec.pch;
  node.yaw := nrec.yaw;
  node.rol := nrec.rol;
  node.ntype := nrec.ntype;
  node.parent := nrec.parent;
end;

Procedure StartUndoRec(const name:string);
begin
  if not UndoEnabled then
    begin
      ClearUndoBuffer;
      exit;
    end;

  While UndoStack.Count>UndoStackSize do
    begin
      TUndoRec(UndoStack[0]).Free;
      UndoStack.Delete(0);
    end;

  curUndoRec := TUndoRec.Create;
  CurUndoRec.Name := name;
  UndoStack.Add(CurUndoRec);
end;

Procedure TLightUndo.SaveUndo(obj: TObject; ch:integer);
begin
  change:=ch;
  case change of
    ch_added:;
    ch_deleted, ch_changed:
      begin
        GetMem(Data,GetLightRecSize);
        GetLight(TSedLight(obj), TLightRec(data^));
      end;
  end;
end;

procedure TLightUndo.Undo(objid: Integer);
var n: integer;
    lt: TSedLight;
begin
  n := level.GetLightByID(objid);
  case change of
    ch_added: DeleteLight(Level, n, {force=}True);
    ch_deleted, ch_changed:
      begin
        if (change = ch_deleted) then
          begin
            if CheckLightIndex(level, n) then exit;  // must not exist
            lt := level.NewLight;
            level.lights.Add(lt);
          end;
        if (change = ch_changed) then
          begin
            if not CheckLightIndex(level, n) then exit;
            lt := level.lights[n];
          end;

        SetLight(lt, TLightRec(data^));
        if (change = ch_deleted) then
          JedMain.LightAdded(lt)
        else
          JedMain.LightChanged(lt);
      end;
  end;
end;

Procedure TThingUndo.SaveUndo(obj: TObject; ch: integer);
begin
  change := ch;
  case change of
    ch_added:;
    ch_deleted,ch_changed:
      begin
        GetMem(Data, GetThingRecSize(TJKThing(obj)));
        GetThing(TJKThing(obj), PThingRec(data));
      end;
  end;
end;

procedure TThingUndo.Undo(objid: Integer);
var n: Integer;
    th: TJKThing;
    tmpur: TUndoRec;
begin
  n := level.GetThingByID(objid);
  case change of
    ch_added:
        begin
          if not CheckThingIndex(level, n) then exit;
          tmpur := CurUndoRec;
          CurUndoRec := nil;
          DeleteThing(level, n, {force=}True);
          CurUndoRec := tmpur;
        end;
    ch_deleted:
      begin
        if CheckThingIndex(level, n) then exit;  // must not exist
        th := level.NewThing;
        SetThing(th, PThingRec(data));
        ResolveThingRefs(th, TThingRec(data^));
        level.things.Add(th);
        level.RenumThings;
        JedMain.ThingAdded(th);
      end;
    ch_changed:
      begin
        if not CheckThingIndex(level, n) then exit;
        th := level.things[n];
        SetThing(th, PThingRec(data));
        ResolveThingRefs(th, TThingRec(data^));
        JedMain.ThingChanged(th);
      end;
  end;
end;

Procedure TSecValUndo.SaveUndo(obj: TObject; ch: integer);
var sec: TJKSector;
    pd: PAnsiChar;
    i: integer;
begin
  change := ch;
  if (ch <> ch_changed) and (ch <> ch_deleted) then exit;

  sec  := TJKSector(obj);
  nsfs := sec.surfaces.Count;
  GetMem(Data, GetSecRecSize + GetSurfRecSize * nsfs);

  pd := Data;
  GetSec(sec, PSecRec(pd)^);
  Inc(pd, GetSecRecSize);

  for i:= 0 to sec.surfaces.Count - 1 do
  begin
    GetSurf(sec.surfaces[i], PSurfRec(pd)^);
    Inc(pd, GetSurfRecSize);
  end;
end;

procedure TSecValUndo.Undo(objid: Integer);
var sec: TJKSector;
    pd: PAnsiChar;
    i ,n: integer;

  Procedure ResolveRefs;
    var asec:TJKSector;
        surf,asurf:TJKSurface;
        i:integer;
        sc,sf:integer;
  begin
    for i := 0 to sec.surfaces.Count - 1 do
      begin
        surf := sec.surfaces[i];
        surf.adjoin := nil;
        sc := surf.mark shr 16;
        sf := surf.mark and 65535;
        if sc = 65535 then continue;

        sc := level.GetSectorByID(surf.nadj);
        if not CheckSectorIndex(level, sc) then continue;
        asec := level.sectors[sc];

        if not CheckSectorSurfaceIndex(asec, sf) then continue;
        asurf := asec.surfaces[sf];

        {  if (asurf.adjoin<>nil) and (asurf.adjoin.sector<>sec) then continue;}
        surf.adjoin  := asurf;
        asurf.adjoin := surf;
      end;
  end; {end of ResolveRefs}

{TSecValUndo.Undo}
begin
  case change of
    ch_changed, ch_deleted:
      begin
        n := level.GetSectorByID(objid);
        if not CheckSectorIndex(level, n) then exit;
        sec := level.sectors[n];
      end;
    else exit;
  end;

  pd := Data;
  SetSec(sec, PSecRec(pd)^);
  Inc(pd, GetSecRecSize);

  n := sec.surfaces.Count;
  if n > nsfs then n := nsfs;

  for i := 0 to n - 1 do
    begin
      SetSurf(sec.surfaces[i], PSurfRec(pd)^);
      Inc(pd, GetSurfRecSize);
      sec.surfaces[i].adjoin := nil;
    end;

  ResolveRefs;
  JedMain.SectorChanged(sec);
end;

Procedure TSecGeoUndo.SaveUndo(obj: TObject; ch: integer);
type
    pword = ^word;

var pd: PAnsiChar;
    size,i: integer;
    sec: TJKSector;
    surf: TJKSurface;
begin
  change := ch;
  case change of
    ch_added:;
    ch_deleted, ch_changed:
    begin
      sec := TJKSector(Obj);
      sec.Renumber;
      nsfs := sec.surfaces.Count;
      nvxs := sec.vertices.Count;

      size := SCVertSize(sec.vertices);
      for i := 0 to sec.surfaces.Count - 1 do
        Inc(size, 2 + SFVertSize(sec.surfaces[i].vertices)); // 2 is size of uint16

      GetMem(Data, size);
      pd := data;
      GetSCVertices(sec.vertices, PVXRec(pd));
      Inc(pd, SCVertSize(sec.vertices));

      for i:= 0 to sec.surfaces.Count - 1 do
      begin
        surf := sec.surfaces[i];

        pword(pd)^ := surf.vertices.Count;
        Inc(pd, 2);

        GetSFVertices(surf, PSFVX(pd));
        Inc(pd, SFVertSize(surf.vertices));
      end;
    end;
  end;
end;

procedure TSecGeoUndo.Undo(objid: Integer);
  type
      pword = ^word;
  var n: integer;
      sec: TJKSector;
      tmpur: TUndoRec;
      pd: PAnsiChar;

  Procedure SetSector;
    var i,j,n: integer;
        surf: TJKSurface;
  begin
    tmpur := CurUndoRec;
    CurUndoRec := nil;
    try
      pd := data;

      if nvxs < sec.vertices.Count then
        for i := sec.vertices.Count - 1 downto nvxs do
          begin
            sec.vertices[i].Free;
            sec.vertices.Delete(i);
          end;

      if nvxs > sec.vertices.Count then
        for i := sec.vertices.Count to nvxs - 1 do
          sec.NewVertex;

      SetSCVertices(sec.vertices, nvxs, PVXRec(pd));
      Inc(pd, SCVertSize(sec.vertices));

      if nsfs < sec.surfaces.Count then
        for i := sec.surfaces.Count - 1 downto nsfs do
          begin
            surf := sec.surfaces[i];
            RemoveSurfRefs(level, surf);
            surf.Free;
            sec.surfaces.Delete(i);
          end;

      if nsfs > sec.surfaces.Count then
        for i := sec.surfaces.Count to nsfs - 1 do
          begin
            surf := sec.NewSurface;
            sec.surfaces.Add(surf);
          end;

      for i:= 0 to nsfs - 1 do
        begin
          surf := sec.surfaces[i];
          n := pword(pd)^;
          Inc(pd, 2); // 2 is size of uint16

          if n<surf.vertices.Count then
            for j := surf.vertices.Count - 1 downto n do
              surf.DeleteVertex(j);

          if n > surf.vertices.Count then
            for j := surf.vertices.Count to n - 1 do
              surf.AddVertex(nil);

          SetSFVertices(sec, surf, n, PSFVX(pd));
          Inc(pd, SFVertSize(surf.vertices));
        end;

      finally
        sec.Renumber;

      for i := 0 to sec.surfaces.Count - 1 do
        sec.surfaces[i].Recalc;

      CurUndoRec := tmpur;
    end;
  end; {SetSector}

begin
  n := level.GetSectorByID(objID);
  case change of
    ch_added:
        begin
          if not CheckSectorIndex(level, n) then exit;
          tmpur := CurUndoRec;
          CurUndoRec := nil;
          DeleteSector(level, n, {force=}True);
          CurUndoRec := tmpur;
        end;
    ch_deleted:
      begin
        if CheckSectorIndex(level, n) then exit; // must not exist
        sec := level.NewSector;
        sec.ID := objID;
        SetSector;
        level.sectors.Add(sec);
        level.RenumSecs;
        JedMain.SectorAdded(sec);
      end;
    ch_changed:
      begin
        if not CheckSectorIndex(level, n) then exit;
        sec := level.sectors[n];
        SetSector;
        JedMain.SectorChanged(sec);
      end;
  end;
end;

Procedure TURQUndo.SaveUndo(obj: TObject; ch: integer);
begin
  change := ch;
  case change of
    ch_added:;
    ch_deleted, ch_changed:
    begin
      GetMem(Data, GetURQRecSize);
      GetURQRec(THNode(obj), TURQRec(data^));
    end;
  end;
end;

procedure TURQUndo.Undo(objid:Integer);
var n: integer;
    node: THNode;
begin
  n := Level.GetNodeByID(objid);
  case change of
    ch_added: if n = -1 then exit else Level.h3donodes.Delete(n);
    ch_deleted, ch_changed:
    begin
      if (change=ch_deleted) then
        begin
          if n <> -1 then exit;
          node := THNode.Create; Level.h3donodes.Add(node);
        end;

      if (change = ch_changed) then
      if n = -1 then exit else node := Level.h3donodes[n];

      SetURQRec(node, TURQRec(data^));
    end;
  end;

  UrqForm.UpdateMeshOffsets;
  UrqForm.reload;
  JedMain.LevelChanged;
end;

Procedure SaveNodeUndo(node:THnode;change:integer);
var nch: TUndoChange;
begin
  if CurUndoRec = nil then exit;

  if CurUndoRec.nodechanges.FindObjID(node.id) <> -1 then exit; {already saved}

  nch := TURQUndo.Create;
  nch.SaveUndo(node, change);
  CurUndoRec.nodechanges.AddObject(nch, node.id);
end;

Procedure SaveLightUndo(lt: TSedLight; change: integer);
var lch:TUndoChange;
begin
  if CurUndoRec = nil then exit;
  if CurUndoRec.ltchanges.FindObjID(lt.id) <> -1 then exit; {already saved}

  lch := TLightUndo.Create;
  lch.SaveUndo(lt, change);
  CurUndoRec.ltchanges.AddObject(lch, lt.id);
end;

Procedure SaveSecUndo(sec: TJKSector; change: integer;how: integer);
var lch:TUndoChange;
begin
  if CurUndoRec = nil then exit;

  if (how and sc_geo) <> 0 then
  begin
    var objId := CurUndoRec.scgchanges.FindObjID(sec.id);
    if objId = -1 then
      begin
        lch := TSecGeoUndo.Create;
        lch.SaveUndo(sec, change);
        CurUndoRec.scgchanges.AddObject(lch, sec.id);
      end
    else if change = ch_deleted then
      begin
        // if sector was changed before and now deleted (same undo frame)
        // make sure the undo is marked as deleted, so in case of cur rect
        // being undone a new sector will be created.
        var sgch := TSecGeoUndo(CurUndoRec.scgchanges[objId]);
        sgch.change := change;
      end;
  end;

  if (how and sc_val) <> 0 then
    if CurUndoRec.scvchanges.FindObjID(sec.id) = -1 then
      begin
        lch := TSecValUndo.Create;
        lch.SaveUndo(sec, change);
        CurUndoRec.scvchanges.AddObject(lch, sec.id);
      end;
end;

Procedure SaveThingUndo(th: TJKThing; change: integer);
var tch: TUndoChange;
begin
  if CurUndoRec = nil then exit;
  if CurUndoRec.thchanges.FindObjID(th.id) <> -1 then exit; {already saved}

  tch := TThingUndo.Create;
  tch.SaveUndo(th, change);
  CurUndoRec.thchanges.AddObject(tch, th.id);
end;

Procedure SaveCOGUndo(cog: TCOG);
begin
end;

Procedure EndUndoRec;
begin
end;

Function GetUndoRecName:string;
begin
 Result := '';
 if CurUndoRec = nil then exit;
 result := CurUndoRec.Name;
end;

Procedure ApplyUndo;
var i: integer;
    id: Integer;
    uobj: TUndoChange;
begin
  if CurUndoRec = nil then exit;
  with CurUndoRec do
    begin
      {Undo sec geometry}
      for i := 0 to scgchanges.Count - 1 do
        begin
          id := scgchanges.GetObjID(i);
          uobj := scgchanges.GetObj(i);
          uobj.Undo(id);
        end;

      {Undo sec values}
      for i := 0 to scvchanges.Count - 1 do
        begin
          id := scvchanges.GetObjID(i);
          uobj := scvchanges.GetObj(i);
          uobj.Undo(id);
        end;

      {Undo lights}
      for i := 0 to ltchanges.Count - 1 do
        begin
          id := ltchanges.GetObjID(i);
          uobj := ltchanges.GetObj(i);
          uobj.Undo(id);
        end;

      {Undo things}
      for i:=0 to thchanges.Count-1 do
      begin
       id:=thchanges.GetObjID(i);
       uobj:=thchanges.GetObj(i);
       uobj.Undo(id);
      end;

      for i := 0 to nodechanges.Count - 1 do
        begin
          id := nodechanges.GetObjID(i);
          uobj := nodechanges.GetObj(i);
          uobj.Undo(id);
        end;
    end;

  CurUndoRec.Free;
  CurUndoRec := nil;
  UndoStack.Delete(UndoStack.Count - 1);
  if UndoStack.Count <> 0 then CurUndoRec := TUndoRec(UndoStack[UndoStack.Count - 1]);
end;

Procedure ClearUndoBuffer;
var i:integer;
begin
  CurUndoRec := nil;
  for i := 0 to UndoStack.count - 1 do
    TUndoRec(UndoStack[i]).Free;
  UndoStack.Clear;
end;

Procedure ClearNodesUndo;
var i:integer;
begin
  CurUndoRec := nil;
  for i := UndoStack.count - 1 downto 0 do
    if TUndoRec(UndoStack[i]).nodechanges.count <> 0 then
    begin
      TUndoRec(UndoStack[i]).Free;
      UndoStack.Delete(i);
    end;
end;


var i: integer;

Initialization
  UndoStack := TList.Create;

Finalization
begin
  For i := 0 to UndoStack.Count - 1 do
    TObject(UndoStack[i]).Free;
  UndoStack.Free;
end;

end.
