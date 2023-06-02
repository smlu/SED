unit pjGeometry;
interface
uses GlobalVars, Classes, misc_utils;

type
TVector=record
 dx,dy,dz:double;
end;

T3DPoint=record
 x,y,z:double;
end;

TBox=record
 X1,Y1,Z1,
 X2,Y2,Z2:double;
end;

T2DBox=record
 x1,y1,x2,y2:double;
end;


TVertex=class
 x,y,z:double;
 mark,num:integer;
 tx,ty,tz:double; {transformed}
end;

TMat3x3=array[0..2,0..2] of double;
TMat3x3s=array[0..2,0..2] of single;

TTXVertex=class
 u,v:single;
 Intensity:single;
 r,g,b:single;
 num:integer;
 Constructor Create;
 Procedure Assign(tv:TTXVertex);
end;

TVertices=class(TList)
 Function GetItem(n:integer):TVertex;
 Procedure SetItem(n:integer;v:TVertex);
Property Items[n:integer]:TVertex read GetItem write SetItem; default;
end;

TTXVertices=class(TList)
 Function GetItem(n:integer):TTXVertex;
 Procedure SetItem(n:integer;v:TTXVertex);
Property Items[n:integer]:TTXVertex read GetItem write SetItem; default;
end;

TPolygon=class
 normal:TVector;
 Vertices:TVertices;
 TXVertices:TTXVertices;
 num:integer;
{ d:double;}
 Constructor Create;
 Function AddVertex(v:TVertex):integer;
 Function InsertVertex(n:integer;v:TVertex):Integer;
 Procedure DeleteVertex(n:integer);
 Destructor Destroy;override;
 Function NextVx(n:integer):integer;
 Function PrevVx(n:integer):integer;


 Procedure ReCalc;
 Procedure CalcNormal;
 Function IsOnPlane(x,y,z:double):boolean;
 Function CalcD:double;
 Function SurfRad:double;
end;

TPolygons=class(TList)
 VXList:TVertices;
 Function GetItem(n:integer):TPolygon;
 Procedure SetItem(n:integer;p:TPolygon);
Property Items[n:integer]:TPolygon read GetItem write SetItem; default;
end;
{list of polygons.
 For logically groupped polygons.
 Some of the vertices can be shared}

Procedure SetVec(var v:TVector;dx,dy,dz:double);

implementation
{Uses lev_utils;}

Constructor TTXVertex.Create;
begin
 Intensity:=5;
 r:=5;
 g:=5;
 b:=5;
end;

Procedure TTXVertex.Assign;
begin
 u:=tv.u;
 v:=tv.v;
 Intensity:=tv.Intensity;
 r:=tv.r;
 g:=tv.g;
 b:=tv.b;
end;

Function TVertices.GetItem(n:integer):TVertex;
begin
 if (n<0) or (n>=count) then raise EListError.CreateFmt('Vertex Index is out of bounds: %d',[n]);
 Result:=TVertex(List[n]);
end;

Procedure TVertices.SetItem(n:integer;v:TVertex);
begin
 if (n<0) or (n>=count) then raise EListError.CreateFmt('Vertex Index is out of bounds: %d',[n]);
 List[n]:=v;
end;

Function TTXVertices.GetItem(n:integer):TTXVertex;
begin
 if (n<0) or (n>=count) then raise EListError.CreateFmt('Vertex Index is out of bounds: %d',[n]);
 Result:=TTXVertex(List[n]);
end;

Procedure TTXVertices.SetItem(n:integer;v:TTXVertex);
begin
 if (n<0) or (n>=count) then raise EListError.CreateFmt('Vertex Index is out of bounds: %d',[n]);
 List[n]:=v;
end;

Constructor TPolygon.Create;
begin
 TXVertices:=TTXVertices.Create;
 Vertices:=TVertices.Create;
end;

Destructor TPolygon.Destroy;
var i:integer;
begin
 for i:=0 to TXVertices.Count-1 do TXVertices[i].Free;
 TXVertices.Free;

 Vertices.free;
end;

Procedure TPolygon.CalcNormal;
begin
 {lev_utils.CalcNormal(Self,normal);}
end;


Function TPolygon.AddVertex(v:TVertex):integer;
begin
 Result:=Vertices.Add(v);
 TXVertices.Add(TTXVertex.Create);
end;

Function TPolygon.InsertVertex(n:integer;v:TVertex):Integer;
begin
 Vertices.Insert(n,v);
 Result:=n;
 TXVertices.Insert(n,TTXVertex.Create);
end;

Procedure TPolygon.DeleteVertex(n:integer);
begin
 Vertices.Delete(n);
 TXVertices[n].Free;
 TXVertices.Delete(n);
end;

Function TPolygon.NextVx(n:integer):integer;
begin
 if n>=Vertices.Count-1 then Result:=0 else Result:=n+1;
end;

Function TPolygon.PrevVx(n:integer):integer;
begin
 if n<=0 then Result:=Vertices.Count-1
  else Result:=n-1;
end;



Procedure TPolygon.ReCalc;
begin
 CalcNormal;
end;

Function TPolygon.CalcD:double;
begin
 With Vertices[0] do
 Result:=normal.dX*X+normal.dY*Y+normal.dZ*Z;
end;


Function TPolygon.SurfRad:double;
var xsum,ysum,zsum:double;
    i,n:integer;
    d:double;
begin
 n:=Vertices.count;
 if n=0 then begin result:=0; exit; end;
 xsum:=0; ysum:=0; zsum:=0;
 for i:=0 to Vertices.count-1 do
 With vertices[i] do
 begin
  xsum:=xsum+x;
  ysum:=ysum+y;
  zsum:=zsum+z;
 end;
 xsum:=xsum/n;
 ysum:=ysum/n;
 zsum:=zsum/n;

 result:=0;
 for i:=0 to Vertices.count-1 do
 With vertices[i] do
 begin
  d:=sqr(x-xsum)+sqr(y-ysum)+sqr(z-zsum);
  if d>result then result:=d;
 end;
 result:=sqrt(d);
end;

Function TPolygon.IsOnPlane(x,y,z:double):boolean;
var nx,ny,nz:double;
    x0,y0,z0:boolean;
    d:double;
begin
 d:=CalcD;
 x0:=IsClose(normal.dx,0);
 y0:=IsClose(normal.dy,0);
 z0:=IsClose(normal.dz,0);

 if (not x0) and not (y0) then
 begin
  nz:=z;
  ny:=(d-normal.dx*x-normal.dz*z)/normal.dy;
  nx:=(d-normal.dz*z-normal.dy*y)/normal.dx;
 end
 else if (not X0) and (not z0) then
 begin
  ny:=y;
  nz:=(d-normal.dx*x-normal.dy*y)/normal.dz;
  nx:=(d-normal.dz*z-normal.dy*y)/normal.dx;
 end
 else if (not y0) and (not z0) then
 begin
  nx:=x;
  nz:=(d-normal.dx*x-normal.dy*y)/normal.dz;
  ny:=(d-normal.dz*z-normal.dx*x)/normal.dy;
 end
 else if x0 and y0 then
 begin
  result:=IsClose(z,Vertices[0].Z);
  exit;
 end
 else if x0 and z0 then
 begin
  result:=IsClose(y,Vertices[0].y);
  exit;
 end
 else if y0 and z0 then
 begin
  result:=IsClose(x,Vertices[0].X);
  exit;
 end;
Result:=IsClose(nx,x) and IsClose(ny,y) and IsClose(nz,z);
end;

Function TPolygons.GetItem(n:integer):TPolygon;
begin
 if (n<0) or (n>=count) then raise EListError.CreateFmt('Polygon Index is out of bounds: %d',[n]);
 Result:=TPolygon(List[n]);
end;

Procedure TPolygons.SetItem(n:integer;p:TPolygon);
begin
 if (n<0) or (n>=count) then raise EListError.CreateFmt('Vertex Index is out of bounds: %d',[n]);
 List[n]:=p;
end;

Procedure SetVec(var v:TVector;dx,dy,dz:double);
begin
 v.dx:=dx;
 v.dy:=dy;
 v.dz:=dz;
end;

end.
