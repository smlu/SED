unit J_Level;
{This unit defines the classes for JK
 level}

interface
uses Classes, Files, Tools, SysUtils, misc_utils,
GlobalVars, Geometry, FileOperations, Values, U_3dos;

Const
     SF_Floor=1;
     FF_Transluent=2;
     SF_3DO=1 shl 31;
     SFF_IMPASSABLE=4;
     SFF_SKY=512;
     SFF_SKY1=1024;
     SFF_DOUBLERES=$10;
     SFF_HALFRES=$20;
     SFF_EIGHTHRES=$40;
     SFF_FLIP=$80000000;
     LF_NoBlock=1;

     TX_DFLT=0;
     TX_KEEP=1;

     TF_NOEASY = $1000; // Object will not appear in "EASY" mode
     TF_NOMEDIUM = $2000; // Object will not appear in "MEDIUM" mode
     TF_NOHARD = $4000; // Object will not appear in "HARD" mode
     TF_NOMULTI = $8000; // Object will not appear in Multiplyer modes

     AF_BlockLight=$80000000;


type


TJKLevel=class;

TJKSector=class;

TJKSurface=class;


TJKVertex=class(TVertex)
 Sector:TJKSector; {Owning sector}
 Procedure Assign(v:TJKVertex);
end;

TJedLight=class
 ID:integer; {Unique ID. Used in Undo}

 X,Y,Z:double;
 mark:integer;
 Intensity:double;
 Range:double;
 layer:integer;
 flags:longint;
 R,G,B:Single;
 rgbintensity:single;
 rgbrange:double;
 Constructor Create;
 Procedure Assign(l:TJedLight);
end;

TJedLights=class(TList)
 Function GetItem(n:integer):TJedLight;
 Procedure SetItem(n:integer;v:TJedLight);
Property Items[n:integer]:TJedLight read GetItem write SetItem; default;
end;

TJKVertices=class(TVertices)
 Function GetItem(n:integer):TJKVertex;
 Procedure SetItem(n:integer;v:TJKVertex);
Property Items[n:integer]:TJKVertex read GetItem write SetItem; default;
end;


TJKLine=class
 v1,v2:TJKVertex;
end;

TSectors=class(TList)
 Function GetItem(n:integer):TJKSector;
 Procedure SetItem(n:integer;v:TJKSector);
Property Items[n:integer]:TJKSector read GetItem write SetItem; default;
end;

TThing=class
 ID:integer; {Unique ID. Used in Undo}

 level:TJKLevel;
 num:integer;
 Sec:TJKSector;
 name:String;
 X,Y,Z:double;
 PCH,YAW,ROL:Double;
 vals:TTPLValues;
 layer:integer;
 a3DO:T3DO;
 bbox:TThingBox;
 Constructor Create;
 Destructor Destroy;override;
 Procedure Assign(thing:TThing);
 Function AddValue(const name,s:string):TTplValue;
 Function InsertValue(n:integer;const name,s:string):TTPlValue;
 Function NextFrame(n:integer):integer;
 Function PrevFrame(n:integer):integer;
 Function Nframes:Integer;
end;

TJKThing=TThing;

TThings=class(TList)
 Function GetItem(n:integer):TThing;
 Procedure SetItem(n:integer;v:TThing);
Property Items[n:integer]:TThing read GetItem write SetItem; default;
end;

TJKSurface=class(TPolygon)
 Adjoin:TJKSurface;
 AdjoinFlags:Longint;
 Material:String;
 SurfFlags,FaceFlags:Longint;
 sector:TJKSector;
 geo,light,tex:integer;
 extraLight:double;
 mark,D3DID:integer;
 num:integer;
 nadj,nmat:integer;
 uscale,vscale:single;

{ ucosa,usina:double;}

 Function GetVXs:TJKVertices;
 Procedure SetVXs(vxs:TJKVertices);
 Property Vertices:TJKVertices read GetVXs write SetVXs;
 Constructor Create(owner:TJKSector);
 procedure Assign(surf:TJKSurface);
 Procedure NewRecalcAll;
 Procedure RecalcAll;
 Procedure CheckIfFloor;
 Function GetRefVector(var v:TVector):boolean;
end;

TSurfaces=class(TPolygons)
 Function GetItem(n:integer):TJKSurface;
 Procedure SetItem(n:integer;v:TJKSurface);
Property Items[n:integer]:TJKSurface read GetItem write SetItem; default;
end;


TJKSector=class{(TPolygons)}
 ID:integer; {Unique ID. Used in Undo}
 num:integer;
 Flags:longint;
 Ambient:double;
 extra:double;
 ColorMap:String;
 Tint:TVector;
 Sound:string;
 snd_vol:double;

 level:TJKLevel;
 surfaces:TSurfaces;
 vertices:TJKVertices;
 layer:integer;
 mark:integer;
 Procedure Assign(sec:TJKSector);
 Constructor Create(owner:TJKLevel);
 Destructor Destroy;override;
 Function NewVertex:TJKVertex;
 Function NewSurface:TJKSurface;
 Procedure Renumber;
 Function FindVX(x,y,z:double):integer;
 Function AddVertex(x,y,z:double):TJKVertex;
end;

TCOG=class
 name:string;
 vals:TCOGValues;
 Constructor Create;
 Destructor Destroy;override;
 Procedure GetValues;
end;

TCOGs=class(TList)
 Function GetItem(n:integer):TCOG;
 Procedure SetItem(n:integer;v:TCOG);
Property Items[n:integer]:TCOG read GetItem write SetItem; default;
end;


TJKLHeader=record
 version:Longint;
 Gravity, CeilingSkyZ,
 HorDistance,HorPixelsPerRev:float;
 HorSkyOffs:array[1..2] of float;
 CeilingSkyOffs:array[1..2] of float;
 MipMapDist:array[1..4] of float;
 LODDist:array[1..4] of float;
 PerspDist, GouraudDist:float;
end;

TJKLevel=class

 header:TJKLHeader;
 mots:boolean;
 sectors:TSectors;
 Cogs:TCOGs;
 Things:TThings;
 Lights:TJedLights;
 Layers:TStringList;
 h3donodes:THNodes;


 masterCMP:String;
 ppunit:double;

 lvisstring:string; {layer visibility string - in form 0101}

 CurID:Integer;

 Constructor Create;
 Destructor Destroy;override;
 Procedure Clear;
 Procedure LoadFromJKL(F:TFileName);
 Procedure JKLPostLoad;
 Procedure LoadFromJed(F:TFileName);
 Procedure SaveToJKL(F:TFileName);
 Procedure SaveToJed(F:TFileName);
 Procedure ImportLEV(F:TFileName;scfactor:double;txhow:integer);
 Procedure ImportAsc(F:TFileName);
 Procedure SetDefaultHeader;
 Procedure RenumSecs;
 Procedure RenumThings;

 Function getNewID:integer;

 Function NewSector:TJKSector;
 Function NewThing:TJKThing;
 Function NewLight:TJedLight;
 Function New3DONode:THNode;

 Function GetThingByID(id:integer):Integer;
 Function GetSectorByID(id:integer):Integer;
 Function GetLightByID(id:integer):Integer;
 Function GetNodeByID(id:integer):Integer;


 {for resolving references}
 Function GetSectorN(n:integer):TJKSector;
 Function GetSurfaceN(n:integer):TJKSurface;
 Function GetThingN(n:integer):TJKthing;
 Function GetGlobalSFN(sc,sf:integer):Integer;
 Function GetLayerName(n:integer):String;
 Function AddLayer(const name:string):integer;
 Procedure AddMissingLayers;
 Function GetMasterCMP:string;
end;

Var
    level:TJKLevel;

Function SFtoInt(sc,sf:integer):integer;
Function EDtoInt(sc,sf,ed:integer):integer;
Function VXToInt(sc,vx:integer):integer;
Function FRToInt(th,fr:integer):integer;
Function GetsfSC(scsf:integer):integer;
Function GetsfSF(scsf:integer):integer;
Function GetedSC(scsfed:integer):integer;
Function GetedSF(scsfed:integer):integer;
Function GetedED(scsfed:integer):integer;
Function GetvxSC(scvx:integer):integer;
Function GetvxVX(scvx:integer):integer;
Function GetfrTH(thfr:integer):integer;
Function GetfrFR(thfr:integer):integer;

implementation
uses ProgressDialog, lev_utils, u_templates, cog_utils, ListRes, U_CogForm,
     U_options;

{Lights}

Constructor TJedLight.Create;
begin
 Range:=2;
 Intensity:=10;
 R:=1; g:=1; b:=1;
 RGBIntensity:=10;
end;

Procedure TJedLight.Assign(l:TJedLight);
begin
 intensity:=l.intensity;
 range:=l.range;
 r:=l.r;
 g:=l.g;
 b:=l.g;
 RGBIntensity:=l.RGBIntensity;
 layer:=l.Layer;
end;

Function TJedLights.GetItem(n:integer):TJedLight;
begin
 if (n<0) or (n>=count) then raise EListError.CreateFmt('Light Index is out of bounds: %d',[n]);
 Result:=TJedLight(List[n]);
end;

Procedure TJedLights.SetItem(n:integer;v:TJedLight);
begin
 if (n<0) or (n>=count) then raise EListError.CreateFmt('Light Index is out of bounds: %d',[n]);
 List[n]:=v;
end;

{Vertices}

Procedure TJKVertex.Assign(v:TJKVertex);
begin
 x:=v.x;
 y:=v.y;
 z:=v.z;
end;

Function TJKVertices.GetItem(n:integer):TJKVertex;
begin
 if (n<0) or (n>=count) then raise EListError.CreateFmt('Vertex Index is out of bounds: %d',[n]);
 Result:=TJKVertex(List[n]);
end;

Procedure TJKVertices.SetItem(n:integer;v:TJKVertex);
begin
 if (n<0) or (n>=count) then raise EListError.CreateFmt('Vertex Index is out of bounds: %d',[n]);
 List[n]:=v;
end;

Constructor TThing.Create;
begin
 Vals:=TTPLValues.Create;
 name:='walkplayer';
end;

Destructor TThing.Destroy;
var i:integer;
begin
 For i:=0 to vals.Count-1 do vals[i].free;
 vals.free;
end;

Function TThing.InsertValue(n:integer;const name,s:string):TTPlValue;
var v:TTplValue;
begin
 v:=TTPLValue.Create;
 v.Name:=name;
 v.vtype:=GetTPLVType(v.name);
 v.atype:=GetTPLType(v.name);
 v.Val(s);
 Vals.Insert(n,v);
 result:=v;
end;

Function TThing.AddValue(const name,s:string):TTplValue;
begin
 Result:=InsertValue(Vals.count,name,s);
end;

Function TThing.Nframes:Integer;
var i:integer;
begin
 Result:=0;
 for i:=0 to vals.count-1 do
 if CompareText(Vals[i].name,'frame')=0 then Inc(Result);
end;

Function TThing.PrevFrame(n:integer):integer;
var v:TTplValue;
begin
 result:=-1;
 dec(n);
 while n>=0 do
 begin
  if CompareText(vals[n].name,'frame')=0 then
  begin
   result:=n;
   exit;
  end;
  dec(n);
 end;
end;


Function TThing.NextFrame(n:integer):integer;
var v:TTplValue;
begin
 result:=-1;
 inc(n);
 while n<Vals.count do
 begin
  if CompareText(vals[n].name,'frame')=0 then
  begin
   result:=n;
   exit;
  end;
  inc(n);
 end;
end;

Procedure TThing.Assign;
var i:Integer;
    v:TTPLvalue;
begin
 name:=thing.name;
 X:=thing.X;
 Y:=thing.Y;
 Z:=thing.Z;
 PCH:=thing.PCH;
 YAW:=thing.YAW;
 ROL:=thing.ROL;
 for i:=0 to Thing.Vals.Count-1 do
 begin
  v:=TTplvalue.Create;
  v.Assign(Thing.Vals[i]);
  vals.Add(v);
 end;
 layer:=thing.layer;
end;

{TCog methods}

Constructor TCog.Create;
begin
 Vals:=TCOGValues.Create;
end;

Destructor TCog.Destroy;
begin
 Vals.Free;
end;

Procedure TCog.GetValues;
begin
end;

Function TCOGs.GetItem(n:integer):TCOG;
begin
 if (n<0) or (n>=count) then raise EListError.CreateFmt('COG Index is out of bounds: %d',[n]);
 Result:=TCOG(List[n]);
end;

Procedure TCOGs.SetItem(n:integer;v:TCog);
begin
 if (n<0) or (n>=count) then raise EListError.CreateFmt('COG Index is out of bounds: %d',[n]);
 List[n]:=v;
end;


Function TSectors.GetItem(n:integer):TJKSector;
begin
 if (n<0) or (n>=count) then raise EListError.CreateFmt('Sector Index is out of bounds: %d',[n]);
 Result:=TJKSector(List[n]);
end;

Procedure TSectors.SetItem(n:integer;v:TJKSector);
begin
 if (n<0) or (n>=count) then raise EListError.CreateFmt('Sector Index is out of bounds: %d',[n]);
 List[n]:=v;
end;

Function TThings.GetItem(n:integer):TThing;
begin
 if (n<0) or (n>=count) then raise EListError.CreateFmt('Thing Index is out of bounds: %d',[n]);
 Result:=TThing(List[n]);
end;

Procedure TThings.SetItem(n:integer;v:TThing);
begin
 if (n<0) or (n>=count) then raise EListError.CreateFmt('Thing Index is out of bounds: %d',[n]);
 List[n]:=v;
end;

Function TSurfaces.GetItem(n:integer):TJKSurface;
begin
 if (n<0) or (n>=count) then raise EListError.CreateFmt('Surface Index is out of bounds: %d',[n]);
 Result:=TJKSurface(List[n]);
end;

Procedure TSurfaces.SetItem(n:integer;v:TJKSurface);
begin
 if (n<0) or (n>=count) then raise EListError.CreateFmt('Surface Index is out of bounds: %d',[n]);
 List[n]:=v;
end;


{TJKSurface Methods}

Constructor TJKSurface.Create;
begin
 Inherited Create;
 sector:=owner;

 AdjoinFlags:=7;
 Material:='dflt.mat';
 SurfFlags:=4;
 FaceFlags:=4;
 geo:=4;
 light:=3;
 tex:=1;
 uscale:=1;
 vscale:=1;
end;

Procedure TJKSurface.CheckIfFloor;
var cosa:double;
begin
 cosa:=Smult(normal.dx,normal.dy,normal.dz,0,0,1);
 if (cosa<=1) and (cosa>cos(Pi/4)) then SurfFlags:=SurfFlags or SF_Floor
 else SurfFlags:=SurfFlags and (-1 xor SF_Floor);
end;

Function TJKSurface.GetRefVector(var v:TVector):boolean;
var v1,v2:TJKVertex;
begin
 result:=false;
 if vertices.count<2 then exit;
 v1:=vertices[0];
 v2:=vertices[1];
 SetVec(v,v2.x-v1.x,v2.y-v1.y,v2.z-v1.z);
 Result:=Normalize(v);
end;


Function TJKSurface.GetVXs:TJKVertices;
begin
 Result:=TJKVertices(Tpolygon(self).Vertices);
end;

Procedure TJKSurface.SetVXs(vxs:TJKVertices);
begin
 Tpolygon(self).Vertices:=vxs;
end;

procedure TJKSurface.Assign(surf:TJKSurface);
begin
 if surf.AdjoinFlags<>0 then AdjoinFlags:=surf.AdjoinFlags;
 if surf.Material<>'' then Material:=surf.Material;
 if surf.SurfFlags<>0 then SurfFlags:=surf.SurfFlags;
 if surf.FaceFlags<>0 then FaceFlags:=surf.FaceFlags;
 if surf.geo<>0 then geo:=surf.geo;
 light:=surf.light;
 tex:=surf.tex;
 extraLight:=surf.extraLight;
 uscale:=surf.uscale;
 vscale:=surf.vscale;
end;

Procedure TJKSurface.NewRecalcAll;
begin
 Recalc;
 CalcUV(Self,0);
 CheckIfFloor;
end;

Procedure TJKSurface.RecalcAll;
var un,vn:tvector;
begin
 Recalc;
 CalcUVNormals(self,un,vn);
 ArrangeTexture(self,0,un,vn);
end;


{TJKSector Methods}

Constructor TJKSector.Create;
begin
 Surfaces:=TSurfaces.Create;
 vertices:=TJKVertices.Create;
 Surfaces.VXList:=Vertices;
 Level:=owner;

 ColorMap:='dflt.cmp';
 Ambient:=5;
end;

Destructor TJKSector.Destroy;
var i:integer;
begin
 for i:=0 to Surfaces.count-1 do Surfaces[i].free;
 surfaces.free;
 for i:=0 to vertices.count-1 do vertices[i].free;
 vertices.free;
end;

Function TJKSector.NewVertex:TJKVertex;
begin
 Result:=TJKVertex.Create;
 Result.Sector:=self;
 result.num:=Vertices.Add(Result)-1;
end;

Function TJKSector.NewSurface:TJKSurface;
begin
 Result:=TJKSurface.Create(self);
end;

Procedure TJKSector.Renumber;
var i:integer;
begin
 for i:=0 to vertices.count-1 do vertices[i].num:=i;
 for i:=0 to surfaces.count-1 do surfaces[i].num:=i;
end;

Function TJKSector.FindVX(x,y,z:double):integer;
var v:TJKvertex;
    i:integer;
begin
 Result:=-1;
 for i:=0 to vertices.count-1 do
 begin
  v:=Vertices[i];
  if IsClose(v.x,x) and IsClose(v.y,y) and IsClose(v.z,z) then
  begin
   Result:=i;
   break;
  end;
 end;
end;

Function TJKSector.AddVertex(x,y,z:double):TJKVertex;
var i:integer;
begin
 i:=FindVX(x,y,z);
 if i<>-1 then Result:=Vertices[i]
 else
 begin
  Result:=NewVertex;
  Result.x:=x;
  Result.y:=y;
  Result.z:=z;
 end;
end;

Procedure TJKSector.Assign;
begin
 Flags:=sec.Flags;
 Ambient:=sec.Ambient;
 extra:=sec.extra;
 ColorMap:=sec.ColorMap;
 Tint:=sec.Tint;
 Sound:=sec.Sound;
 snd_vol:=sec.snd_vol;
 layer:=sec.layer;
end;

{TJKLevel Methods}

Function TJKLevel.getNewID:integer;
begin
 if self=nil then begin result:=0; exit; end;
 Result:=CurID;
 Inc(CurID);
end;

Function TJKLevel.GetThingByID(id:integer):Integer;
var i:integer;
begin
 Result:=-1;
 for i:=0 to Things.count-1 do
 If Things[i].ID=id then
 begin
  Result:=i;
  exit;
 end;
end;

Function TJKLevel.GetSectorByID(id:integer):Integer;
var i:integer;
begin
 Result:=-1;
 for i:=0 to Sectors.count-1 do
 If Sectors[i].ID=id then
 begin
  Result:=i;
  exit;
 end;
end;

Function TJKLevel.GetLightByID(id:integer):Integer;
var i:integer;
begin
 Result:=-1;
 for i:=0 to Lights.count-1 do
 If Lights[i].ID=id then
 begin
  Result:=i;
  exit;
 end;
end;

Function TJKLevel.GetNodeByID(id:integer):Integer;
var i:integer;
begin
 Result:=-1;
 for i:=0 to h3donodes.count-1 do
 If h3donodes[i].ID=id then
 begin
  Result:=i;
  exit;
 end;
end;


Function TJKLevel.NewSector:TJKSector;
begin
 Result:=TJKSector.Create(self);
 result.ID:=getNewID;
end;

Function TJKLevel.NewThing:TThing;
begin
 Result:=TThing.Create{(self)};
 Result.Level:=self;
 result.ID:=getNewID;
end;

Function TJKLevel.NewLight:TJedLight;
begin
 Result:=TJedLight.Create;
 result.ID:=getNewID;
end;

Function TJKLevel.New3DONode:THnode;
begin
 Result:=THNode.Create;
 result.ID:=getNewID;
end;


Procedure TJKLevel.RenumSecs;
var i:integer;
begin
 for i:=0 to Sectors.Count-1 do Sectors[i].num:=i;
end;

Procedure TJKLevel.RenumThings;
var i:integer;
begin
 for i:=0 to Things.Count-1 do Things[i].num:=i;
end;

Function TJKLevel.GetLayerName(n:integer):String;
begin
 if (n<0) or (n>=Layers.count) then Result:=Format('LAYER%d',[n])
 else Result:=Layers[n];
end;

Function TJKLevel.AddLayer(const name:string):integer;
begin
 Result:=Layers.IndexOf(name);
 if Result=-1 then Result:=Layers.Add(name);
end;

Function TJKLevel.GetMasterCMP:string;
begin
 Result:=MasterCMP;
 if (Result='') and (sectors.count>0) then Result:=sectors[0].ColorMap;
 if result='' then result:='dflt.cmp';
end;

Constructor TJKLevel.Create;
begin
 Sectors:=TSectors.Create;
 Things:=TThings.Create;
 cogs:=TCOgs.Create;
 Lights:=TJedLights.Create;
 Layers:=TStringList.Create;
 h3donodes:=THNodes.Create;
end;

Procedure TJKLevel.Clear;
var i,j,k:Integer;
begin
 For i:=0 to sectors.count-1 do sectors[i].free;
  sectors.clear;
 For i:=0 to things.count-1 do things[i].free;
  Things.clear;

 For i:=0 to cogs.count-1 do cogs[i].free;
  cogs.clear;

 For i:=0 to lights.count-1 do lights[i].free;

 for i:=0 to h3donodes.count-1 do h3donodes[i].Free;
 h3donodes.Clear;

  lights.clear;
  Layers.Clear;
  mots:=false;
  MasterCmp:='';
  LVisString:='';
  ppunit:=320;
  CurID:=0;
end;

Destructor TJKLevel.Destroy;
begin
 Clear;
 Sectors.Free;
 Things.free;
 Lights.Free;
 Layers.Free;
end;

Procedure TJKLevel.SetDefaultHeader;
begin
With Header do
begin
 version:=1;
 Gravity:=4;
 CeilingSkyZ:=15;
 HorDistance:=100;
 HorPixelsPerRev:=768;
 HorSkyOffs[1]:=0; HorSkyOffs[2]:=0;
 CeilingSkyOffs[1]:=0;  CeilingSkyOffs[2]:=0;
 MipMapDist[1]:=1; MipMapDist[2]:=2; MipMapDist[3]:=3; MipMapDist[4]:=4;
 LODDist[1]:=0.3; LODDist[2]:=0.6; LODDist[3]:=0.9; LODDist[4]:=1.2;
 PerspDist:=2;
 GouraudDist:=2;
end;
end;

Procedure TJKLevel.AddMissingLayers;
var i:integer;
begin
 for i:=0 to sectors.count-1 do
 With sectors[i] do
 begin
  if (layer<0) or (layer>=layers.count) then
   layer:=AddLayer(Format('Layer%d',[layer]));
 end;
 for i:=0 to things.count-1 do
 With things[i] do
 begin
  if (layer<0) or (layer>=layers.count) then
   layer:=AddLayer(Format('Layer%d',[layer]));
 end;
 for i:=0 to Lights.count-1 do
 With Lights[i] do
 begin
  if (layer<0) or (layer>=layers.count) then
   layer:=AddLayer(Format('Layer%d',[layer]));
 end;

end;


Function TJKLevel.GetSectorN(n:integer):TJKSector;
begin
 if n<0 then begin Result:=nil; exit; end;
 Try
  Result:=Sectors[n];
 except
  On Exception do result:=nil;
 end;
end;

Function TJKLevel.GetSurfaceN(n:integer):TJKSurface;
var s,nsf:integer;
begin
 nsf:=0;
 if n<0 then begin Result:=nil; exit; end;
 
 for s:=0 to Sectors.count-1 do
 With Sectors[s] do
 begin
  if (n>=nsf) and (n<nsf+Surfaces.count) then
  begin
   Result:=Surfaces[n-nsf];
   exit;
  end;
   inc(nsf,Surfaces.count);
 end;
 Result:=nil;
end;

Function TJKLevel.GetThingN(n:integer):TJKthing;
begin
 if n<0 then begin Result:=nil; exit; end;
 Try
  Result:=Things[n];
 except
  On Exception do begin result:=nil; end;  
 end;
end;

Function TJKLevel.GetGlobalSFN(sc,sf:integer):Integer;
var i,nsf:integer;
begin
 if sc=-1 then begin Result:=-1; exit; end;
 nsf:=0;
 For i:=0 to sc-1 do
 With Sectors[i] do
 begin
  inc(nsf,Surfaces.Count);
 end;
 Result:=nsf+sf;
end;

Function SFtoInt(sc,sf:integer):integer;
begin
 result:=sc shl 16+sf;
end;

Function EDtoInt(sc,sf,ed:integer):integer;
begin
 result:=sc shl 18+sf shl 8+ed;
end;

Function VXToInt(sc,vx:integer):integer;
begin
 result:=sc shl 16+vx;
end;

Function FRToInt(th,fr:integer):integer;
begin
 result:=th shl 16+fr;
end;


Function GetsfSC(scsf:integer):integer;
begin
 result:=scsf shr 16;
end;

Function GetsfSF(scsf:integer):integer;
begin
 result:=scsf and $FFFF;
end;

Function GetedSC(scsfed:integer):integer;
begin
 result:=scsfed shr 18;
end;

Function GetedSF(scsfed:integer):integer;
begin
 result:=(scsfed shr 8) and $3FF;
end;

Function GetedED(scsfed:integer):integer;
begin
 result:=scsfed and $FF;
end;

Function GetvxSC(scvx:integer):integer;
begin
 result:=scvx shr 16;
end;

Function GetvxVX(scvx:integer):integer;
begin
 result:=scvx and $FFFF;
end;

Function GetfrTH(thfr:integer):integer;
begin
 result:=thfr shr 16;
end;

Function GetfrFR(thfr:integer):integer;
begin
 result:=thfr and $FFFF;
end;



{$i level_io.inc}
{$i DF_import.inc}
{$i Savejkl.inc}
{$i asc_import.inc}
Initialization
begin
 level:=TJKLevel.Create;
end;

Finalization
begin
 Level.Clear;
 level.Free;
end;

end.
