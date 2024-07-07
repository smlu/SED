unit u_pj3dos;
// TODO: Replace this file with u_3dos
interface
uses geometry, SysUtils, misc_utils, GlobalVars, classes;

Type

T3DOFace=class(TPolygon)
 imat:integer;
// geo,light,tex:integer;
 extra_l:single;
end;

T3DOFaces = class(TPolygons)
 Function GetItem(n:integer):T3DOface;
 Procedure SetItem(n:integer;v:T3DOface);
 Property Items[n:integer]:T3DOface read GetItem write SetItem; default;
end;

T3DOMesh = class
 name: string;
 geo, light, tex: Integer;
 faces: T3DOFaces;
 Constructor Create;
 Function GetVXs:TVertices;
 Property vertices:Tvertices read GetVXs;
 Destructor Destroy;override;
 Function FindRadius:double;
end;

T3DOMeshes = class(TList)
 Function GetItem(n:integer):T3DOMesh;
 Procedure SetItem(n:integer;v:T3DOMesh);
Property Items[n:integer]:T3DOMesh read GetItem write SetItem; default;
end;

T3DOHNode = class
 meshname: string;
 nmesh: Integer;
 parent:integer;
 orgX,orgY,orgZ,
 orgPCH,orgYAW,orgROL:double;
 x,y,z:double;
 pch,yaw,rol:double;
 pivotx,pivoty,pivotz:double;
end;

T3DOHNodes = class(TList)
 Function GetItem(n:integer):T3DOHNode;
 Procedure SetItem(n:integer;v:T3DOHNode);
Property Items[n:integer]:T3DOHNode read GetItem write SetItem; default;
end;


TPJ3DO=class
 ucount:integer;
 mats:TStringList;
 meshes:T3DOMeshes;
 hnodes:T3DOHNodes;
 Constructor CreateNew;
 Constructor CreateFrom3DO(const name: string; lod: integer); overload;
 Constructor CreateFrom3DO(const name: string; lod: integer; var fileVersion: double); overload;
 Destructor Destroy;override;
 Function NewMesh: T3DOMesh;
 Function GetMat(n:integer):string;
 Procedure GetBBox(var box:TThingBox);
 Function FindRadius:double;
 Procedure OffsetMeshes;
 Procedure SetDefaultOffsets;
 Function IndexOfNode(const name:string):integer;
end;

Function Load3DO(const name:string):TPJ3DO;
Procedure Free3DO(var a3DO:TPJ3DO);
{These function must be use to load and free
 3DOs, not TPJ3DO.CreateFrom3DO, TPJ3DO.Free}

implementation

uses files, FileOperations, geo_utils;

var
  L3DOs:TStringList;


Function Load3DO(const name:string):TPJ3DO;
var i:integer;
begin
 i:=L3DOs.IndexOf(name);
 if i<>-1 then begin Result:=TPJ3DO(L3DOs.Objects[i]); inc(Result.ucount); end
 else
 begin
  try
   Result:=TPJ3DO.CreateFrom3DO(name,0);
   L3DOs.AddObject(name,Result);
   Result.ucount:=1;
  except
   on Exception do begin result:=nil; PanMessageFmt(mt_warning,'Cannot load %s',[name]); end;
  end;
 end;
end;

Procedure Free3DO(var a3DO:TPJ3DO);
var i:integer;
begin
 if a3DO=nil then exit;
try
try
 Dec(a3DO.ucount);
 if A3Do.ucount<=0 then
 begin
   i:=L3DOs.IndexOfObject(a3DO);
   if i<>-1 then L3DOs.Delete(i);
  a3DO.Destroy;
 end;
finally
 a3DO:=nil;
end;
except
 On Exception do;
end;
end;



Function T3DOFaces.GetItem(n:integer):T3DOFace;
begin
 if (n<0) or (n>=count) then raise EListError.CreateFmt('3DO Face Index is out of bounds: %d',[n]);
 Result:=T3DOFace(List[n]);
end;

Procedure T3DOFaces.SetItem(n:integer;v:T3DOFace);
begin
 if (n<0) or (n>=count) then raise EListError.CreateFmt('3DO Face Index is out of bounds: %d',[n]);
 List[n]:=v;
end;

Function T3DOMeshes.GetItem(n:integer):T3DOMesh;
begin
 if (n<0) or (n>=count) then raise EListError.CreateFmt('3DO Mesh Index is out of bounds: %d',[n]);
 Result:=T3DOMesh(List[n]);
end;

Procedure T3DOMeshes.SetItem(n:integer;v:T3DOMesh);
begin
 if (n<0) or (n>=count) then raise EListError.CreateFmt('3DO Mesh Index is out of bounds: %d',[n]);
 List[n]:=v;
end;


Constructor TPJ3DO.CreateNew;
begin
 Mats:=TStringList.create;
 Meshes:=T3DOMeshes.Create;
 hnodes:=T3DOHnodes.Create;
end;

Constructor TPJ3DO.CreateFrom3DO(const name: string; lod: integer);
  var version: double;
begin
  CreateFrom3DO(name, lod, version);
end;

Destructor TPJ3DO.Destroy;
var i,j:integer;
begin
try
 for i:=0 to Meshes.count-1 do
 With Meshes[i] do
 begin
  For j:=0 to Vertices.count-1 do Vertices[j].free;
  For j:=0 to Faces.count-1 do faces[j].free;
  free;
 end;
 for i:=0 to hnodes.count-1 do hnodes[i].free;
 hnodes.free;
finally
 if i=0 then;
 if j<>0 then;
end;
 mats.free;
 Inherited destroy;
end;

Function TPJ3DO.NewMesh:T3DOMesh;
begin
 Result:=T3DOMesh.Create;
end;

Function TPJ3DO.GetMat(n:integer):string;
begin
 if (n<0) or (n>=mats.count) then result:=''
 else result:=Mats[n];
end;

Procedure TPJ3DO.GetBBox(var box:TThingBox);
var i,j:integer;
    x1,x2,y1,y2,z1,z2:double;
begin
 x1:=99999;
 x2:=-99999;
 y1:=99999;
 y2:=-99999;
 z1:=99999;
 z2:=-99999;

 for i:=0 to Meshes.count-1 do
 With Meshes[i] do
 for j:=0 to Vertices.count-1 do
 With Vertices[j] do
 begin
  if tx<x1 then x1:=tx;
  if tx>x2 then x2:=tx;
  if ty<y1 then y1:=ty;
  if ty>y2 then y2:=ty;
  if tz<z1 then z1:=tz;
  if tz>z2 then z2:=tz;
 end;
 if x1=99999 then FillChar(Box,sizeof(Box),0)
 else
 begin
  box.x1:=x1;
  box.x2:=x2;
  box.y1:=y1;
  box.y2:=y2;
  box.z1:=z1;
  box.z2:=z2;
 end;
end;

COnstructor T3DOMesh.Create;
begin
 Faces:=T3DOFaces.Create;
 Faces.VXList:=TVertices.Create;
end;

Function T3DOMesh.GetVXs:TVertices;
begin
 Result:=faces.VXList;
end;

Destructor T3DOMesh.Destroy;
begin
 Faces.VXList.Destroy;
 Faces.Destroy;
end;

Function T3DOMesh.FindRadius:double;
var i:integer;
    crad:double;
begin
 Result:=0;
 for i:=0 to Vertices.count-1 do
 With Vertices[i] do
 begin
  crad:=Sqrt(sqr(x)+sqr(y)+sqr(z));
  if crad>result then Result:=crad;
 end;
end;


Function TPJ3DO.FindRadius:double;
var i,j:integer;
    crad:double;
begin
 Result:=0;
 for i:=0 to Meshes.count-1 do
 begin
  crad:=Meshes[i].FindRadius;
  if crad>result then Result:=crad;
 end;
end;

Function TPJ3DO.IndexOfNode(const name:string):integer;
var i:integer;
begin
 Result:=-1;
 for i:=0 to hnodes.count-1 do
 begin
  if CompareText(hnodes[i].meshname,name)=0 then
  begin
   result:=i;
   exit;
  end;
 end;
end;

Procedure TPJ3DO.SetDefaultOffsets;
var i:integer;
begin
 for i:=0 to HNodes.Count-1 do
 With hNodes[i] do
 begin
  x:=orgx;
  y:=orgy;
  z:=orgz;
  pch:=orgpch;
  yaw:=orgyaw;
  rol:=orgrol;
end;
end;

Procedure TPJ3DO.OffsetMeshes;
var i,j:integer;
    hnode,hnode1:T3DOHNode;
    mdx,mdy,mdz:double;
    mx:TMat3x3;
    vs:TVertices;
begin
 for i:=0 to HNodes.Count-1 do
 begin
  hnode:=HNodes[i];
  if hnode.nmesh=-1 then continue;

  vs:=Meshes[hnode.nmesh].Vertices;

  With Hnode do CreateRotMatrix(mx,pch,yaw,rol);
  for j:=0 to vs.count-1 do
  With vs[j] do
  begin
   tx:=x+hnode.pivotx;
   ty:=y+hnode.pivoty;
   tz:=z+hnode.pivotz;
   MultVM3(mx,tx,ty,tz);
   tx:=tx+hnode.x;
   ty:=ty+hnode.y;
   tz:=tz+hnode.z;
  end;

(*  mdx:=hnode.x+hnode.pivotx{+InsX};
  mdy:=hnode.y+hnode.pivoty{+InsY};
  mdz:=hnode.z+hnode.pivotz{+InsZ}; *)

  hnode1:=hnode;
  While hnode1.parent<>-1 do
  begin
   hnode1:=HNodes[hnode1.Parent];

   With hnode1 do CreateRotMatrix(mx,pch,yaw,rol);

   for j:=0 to vs.count-1 do
   With vs[j] do
   begin
    MultVM3(mx,tx,ty,tz);
    tx:=tx+hnode1.x;
    ty:=ty+hnode1.y;
    tz:=tz+hnode1.z;
   end;

  end;

 end;
end;

Function T3DOHNodes.GetItem(n:integer):T3DOHNode;
begin
 if (n<0) or (n>=count) then raise EListError.CreateFmt('3DO Node Index is out of bounds: %d',[n]);
 Result:=T3DOHNode(List[n]);
end;

Procedure T3DOHNodes.SetItem(n:integer;v:T3DOHNode);
begin
 if (n<0) or (n>=count) then raise EListError.CreateFmt('3DO Node Index is out of bounds: %d',[n]);
 List[n]:=v;
end;

{$i pj3do_io.inc}



Initialization
begin
 L3DOs:=TStringList.Create;
 L3DOs.Sorted:=true;
end;

end.

