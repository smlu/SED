unit u_3dos;

interface

uses classes, geometry, SysUtils, misc_utils, GlobalVars, StrUtils, System.Generics.Collections;

Type

  T3DOFace = class(TPolygon)
    imat: integer;
    //faceflags: Longint;
//    geo, light, tex: integer;
//    extraLight: TColorF;
  end;

  T3DOFaces = class(TPolygons)
    Function GetItem(n: integer): T3DOFace;
    Procedure SetItem(n: integer; v: T3DOFace);
    Property Items[n: integer]: T3DOFace read GetItem write SetItem; default;
  end;

  T3DOMesh = class
    name: string;
    faces: T3DOFaces;

    Function GetVXs: TVertices;
    Property vertices: TVertices read GetVXs;

    Constructor Create;

    Destructor Destroy; override;
    Function FindRadius: double;
    Function AddVertex(x, y, z: double; duplicates: Boolean = False): integer;
  end;

  T3DOMeshes = class(TList)
    Function GetItem(n: integer): T3DOMesh;
    Procedure SetItem(n: integer; v: T3DOMesh);
    Property Items[n: integer]: T3DOMesh read GetItem write SetItem; default;
  end;

  THNode = class
    id: integer;
    nodename: string;
    ntype: Longint;
    nmesh: integer;
    parent: integer;
    x, y, z: double;
    pch, yaw, rol: double;
    pivotx, pivoty, pivotz: double;
    Constructor Create;
    Procedure Assign(node: THNode);
  end;

  THNodes = class(TList)
    Function GetItem(n: integer): THNode;
    Property Items[n: integer]: THNode read GetItem; default;
  end;

  T3DO = class
    name: string;
    ucount: integer;
    mats: TStringList;
    meshes: T3DOMeshes;
    hnodes: THNodes;
    Constructor CreateNew;
    Constructor CreateFrom3DO(const name: string; lod: integer); overload;
    Constructor CreateFrom3DO(const name: string; lod: integer; var fileVersion: double); overload;
    Destructor Destroy; override;
    Function NewMesh: T3DOMesh;
    Function GetMat(n: integer): string;
    Procedure GetBBox(var box: TThingBox);
    Procedure SaveToFile(const name: string; const version: double);
    Function FindRadius: double;
  end;

Function Load3DO(const name: string): T3DO;
Procedure Free3DO(var a3DO: T3DO);
{ These function must be use to load and free
  3DOs, not T3DO.CreateFrom3DO, T3Do.Free }
Procedure StringToHNode(const s: string; node: THNode);
Function HNodeToString(node: THNode; n: integer): string;

implementation

uses files, FileOperations, lev_utils;

var
  L3DOs: TStringList;

Constructor THNode.Create;
begin
  ntype := 1;
  nmesh := -1;
  parent := -1;
end;

Procedure THNode.Assign(node: THNode);
begin
  nodename := node.nodename;
  ntype := node.ntype;
  nmesh := node.nmesh;
  parent := node.parent;
  x := node.x;
  y := node.y;
  z := node.z;
  pch := node.pch;
  yaw := node.yaw;
  rol := node.rol;
  pivotx := node.pivotx;
  pivoty := node.pivoty;
  pivotz := node.pivotz;
end;

Function Load3DO(const name: string): T3DO;
var
  i: integer;
begin
  i := L3DOs.IndexOf(name);
  if i <> -1 then
    begin
      Result := T3DO(L3DOs.Objects[i]);
      inc(Result.ucount);
    end
  else
    begin
      try
        Result := T3DO.CreateFrom3DO(name, 0);
        L3DOs.AddObject(name, Result);
        Result.ucount := 1;
      except
        on Exception do
        begin
          Result := nil;
          PanMessageFmt(mt_warning, 'Cannot load %s', [name]);
        end;
      end;
    end;
end;

Procedure Free3DO(var a3DO: T3DO);
var
  i: integer;
begin
  if a3DO = nil then
    exit;
  try
    try
      Dec(a3DO.ucount);
      if a3DO.ucount <= 0 then
      begin
        i := L3DOs.IndexOfObject(a3DO);
        if i <> -1 then
          L3DOs.Delete(i);
        a3DO.Destroy;
      end;
    finally
      a3DO := nil;
    end;
  except
    On Exception do;
  end;
end;

Function T3DOFaces.GetItem(n: integer): T3DOFace;
begin
  if (n < 0) or (n >= count) then
    raise EListError.CreateFmt('3DO Face Index is out of bounds: %d', [n]);
  Result := T3DOFace(List[n]);
end;

Procedure T3DOFaces.SetItem(n: integer; v: T3DOFace);
begin
  if (n < 0) or (n >= count) then
    raise EListError.CreateFmt('3DO Face Index is out of bounds: %d', [n]);
  List[n] := v;
end;

Function T3DOMeshes.GetItem(n: integer): T3DOMesh;
begin
  if (n < 0) or (n >= count) then
    raise EListError.CreateFmt('3DO Mesh Index is out of bounds: %d', [n]);
  Result := T3DOMesh(List[n]);
end;

Procedure T3DOMeshes.SetItem(n: integer; v: T3DOMesh);
begin
  if (n < 0) or (n >= count) then
    raise EListError.CreateFmt('3DO Mesh Index is out of bounds: %d', [n]);
  List[n] := v;
end;

Function THNodes.GetItem(n: integer): THNode;
begin
  if (n < 0) or (n >= count) then
    raise EListError.CreateFmt('3DO Node Index is out of bounds: %d', [n]);
  Result := THNode(List[n]);
end;

Constructor T3DO.CreateNew;
begin
  Mats := TStringList.Create;
  Meshes := T3DOMeshes.Create;
  hnodes := THNodes.Create;
end;

Constructor T3DO.CreateFrom3DO(const name: string; lod: integer);
  var version: double;
begin
  CreateFrom3DO(name, lod, version);
end;

Destructor T3DO.Destroy;
var
  i, j: integer;
begin
  try
    for i := 0 to hnodes.count - 1 do
      hnodes[i].Free;
    for i := 0 to Meshes.count - 1 do
      With Meshes[i] do
      begin
        For j := 0 to vertices.count - 1 do
          vertices[j].Free;
        For j := 0 to faces.count - 1 do
          faces[j].Free;
        Free;
      end;
  finally
    Meshes.Free;
    Mats.Free;
    hnodes.Free;
  end;
  Inherited Destroy;
end;

Function T3DO.NewMesh: T3DOMesh;
begin
  Result := T3DOMesh.Create;
end;

Function T3DO.GetMat(n: integer): string;
begin
  if (n < 0) or (n >= Mats.count) then
    Result := ''
  else
    Result := Mats[n];
end;

Procedure T3DO.GetBBox(var box: TThingBox);
var
  i, j: integer;
  x1, x2, y1, y2, z1, z2: double;
begin
  x1 := 99999;
  x2 := -99999;
  y1 := 99999;
  y2 := -99999;
  z1 := 99999;
  z2 := -99999;

  for i := 0 to Meshes.count - 1 do
    With Meshes[i] do
      for j := 0 to vertices.count - 1 do
        With vertices[j] do
        begin
          if x < x1 then
            x1 := x;
          if x > x2 then
            x2 := x;
          if y < y1 then
            y1 := y;
          if y > y2 then
            y2 := y;
          if z < z1 then
            z1 := z;
          if z > z2 then
            z2 := z;
        end;
  if x1 = 99999 then
    FillChar(box, sizeof(box), 0)
  else
  begin
    box.x1 := x1;
    box.x2 := x2;
    box.y1 := y1;
    box.y2 := y2;
    box.z1 := z1;
    box.z2 := z2;
  end;
end;

COnstructor T3DOMesh.Create;
begin
  faces := T3DOFaces.Create;
  faces.VXList := TVertices.Create;
end;

Function T3DOMesh.GetVXs: TVertices;
begin
  Result := faces.VXList;
end;

Destructor T3DOMesh.Destroy;
begin
  faces.VXList.Destroy;
  faces.Destroy;
end;

Function T3DOMesh.FindRadius: double;
var
  i: integer;
  crad: double;
begin
  Result := 0;
  for i := 0 to vertices.count - 1 do
    With vertices[i] do
    begin
      crad := Sqrt(sqr(x) + sqr(y) + sqr(z));
      if crad > Result then
        Result := crad;
    end;
end;

 Function T3DOMesh.AddVertex(x, y, z: double; duplicates: Boolean): integer;
var
  i: integer;
  v: Tvertex;
begin
  if not duplicates then
    begin
      i := FindVX(vertices, x, y, z);
      if i <> -1 then
        begin
          Result := i;
          exit;
        end;
    end;
  v := Tvertex.Create;
  v.x := x;
  v.y := y;
  v.z := z;
  Result := vertices.Add(v);

end;

Function T3DO.FindRadius: double;
var
  i, j: integer;
  crad: double;
begin
  Result := 0;
  for i := 0 to Meshes.count - 1 do
  begin
    crad := Meshes[i].FindRadius;
    if crad > Result then
      Result := crad;
  end;
end;

{$I 3do_io.inc}

Initialization

begin
  L3DOs := TStringList.Create;
  L3DOs.Sorted := true;
end;

end.

  Types: 0 x10 or 0 x00010 This node is apart of the lower body
(hip).0 x1 or 0 x00001 This node is apart of the upper body(torso).0 x20 or
  0 x00020 This node is apart of the left leg(lcalf, lfoot, lthigh).0 x2 or
  0 x00002 This node is apart of the left arm(lforearm, lhand, lshoulder)
  .0 x40 or 0 x00040 This node is apart of the right leg(rcalf, rfoot, rthigh)
  .0 x4 or 0 x00004 This node is apart of the right arm(rforearm, rhand,
  rshoulder).0 x8 or 0 x00008 This node is apart of the head(head, neck).
