unit lev_utils;

interface

uses Geometry, J_Level, misc_utils, GlobalVars, ProgressDialog,
graph_files, U_multisel, u_undo, u_3dos;

Const
     rs_surfs=1;

const
 rt_x=0;
 rt_y=1;
 rt_z=2;
 rt_grid=3;


const
     newLight:boolean=true;

Type
 TVecRotData=record
  sinPch,cosPch,
  sinYaw,cosYaw,
  sinRol,cosRol:double;
 end;

 TScaleData=record
  cx,cy,cz:double; {center}
  sfactor:double;
  vec:TVector;
  how:(scale_xyz,scale_x,scale_y,scale_z,scale_vec);
 end;

Function NextIdx(n,maxn:integer):integer;

function GetAngle(x,y:Double;xa,ya,xb,yb:double):Double;
Function Isabove180(x,y:Double;xa,ya,xb,yb:double):boolean;
Function DoIntersect(x11,y11,x12,y12,x21,y21,x22,y22:double):boolean;

Procedure MultVM3(const mx:TMat3x3; var x,y,z:double);

Procedure MultM3(var mx:TMat3x3; const bymx:TMat3x3);

Procedure MultVM3s(const mx:TMat3x3s; var x,y,z:single);

Procedure MultM3s(var mx:TMat3x3s; const bymx:TMat3x3s);


Procedure CreateRotMatrix(var mx:TMat3x3; pch,yaw,rol:double);
Procedure ScaleMatrix(var mx:TMat3x3;scx,scy,scz:double);

Procedure CreateRotMatrixS(var mx:TMat3x3s; pch,yaw,rol:single);
Procedure ScaleMatrixS(var mx:TMat3x3s;scx,scy,scz:single);


Procedure CalcNormal(surf:TPolygon;var normal:TVector);

Procedure FindBBox(sec:TJKSector;var box:TBox);
Procedure FindBSphere(sec:TJKSector;var CX,CY,CZ,Rad:double);
Function FindCollideBox(sec:TJKSector;const bbox:TBox;
                        CX,CY,CZ:double; var cbox:TBox):boolean;

Function DoBoxesIntersect(const box1,box2:TBox):boolean;
Procedure SetBox(var box:TBox; x1,x2,y1,y2,z1,z2:double);
Function IsPointInBox(const box:TBox; x,y,z:double):boolean;

Function Do2DBoxesIntersect(const box1,box2:T2DBox):boolean;
Procedure Set2DBox(var box:T2DBox; x1,x2,y1,y2:double);
Function IsPointIn2DBox(const box:T2DBox; x,y:double):boolean;

Procedure FindCenter(sec:TJKSector;var CX,CY,CZ:double);
Procedure CalcSecCenter(sec:TJKSector; var cx,cy,cz:double);

Procedure CalcSurfCenter(surf:TJKSurface;var cx,cy,cz:double);
Procedure CalcSurfRect(surf:TJKSurface;v1,v2,v3,v4:TVertex;by:double);

Procedure VMult(X1,Y1,Z1,X2,Y2,Z2:double;var X,Y,Z:double);
Function SMult(X1,Y1,Z1,X2,Y2,Z2:double):double;
Function VLen(const v:TVector):Double;

Procedure RotateVector(var vec:TVector; pch,yaw,rol:double);
Procedure GetJKPYR(const x,y,z:TVector;var pch,yaw,rol:double);
Procedure CalcRotVecData(var rd:TVecRotData; pch,yaw,rol:double);
Procedure RotVecbyData(var vec:TVector; var rd:TVecRotData);


Function IsSurfConvex(surf:TJKSurface):boolean;
Function IsSurfPlanar(surf:TJKSurface):boolean;
Function IsSectorConvex(sec:TJKSector):boolean;

Function IsOnLine(X1,Y1,Z1,X2,Y2,Z2,X,Y,Z:double):boolean;

Function DistToPlane(surf:TPolygon;x,y,z:double):double;

{Do line and surface intersect}
Function PlaneLineXn(const normal:TVector;D:double;x1,y1,z1,x2,y2,z2:double;var x,y,z:double):boolean;
Function PlaneLineXnNew(const normal:TVector;pX,pY,pZ,x1,y1,z1,x2,y2,z2:double;var x,y,z:double):boolean;

Function IsPointOnSurface(surf:TJKSurface;x,y,z:double):boolean;
Function IsPointWithInLine(x,y,z,x1,y1,z1,x2,y2,z2:double):boolean;
{Function SurfLineXn(surf:TJKSurface;x1,y1,z1,x2,y2,z2:double):integer;}

Function IsInSector(sec:TJKSector;x,y,z:double):boolean;
Function DoSectorsOverlap(sec1,sec2:TJKSector):boolean;

Function FindSectorForThing(thing:TJKThing):Integer;
Function FindSectorForXYZ(lev:TJKLevel;X,Y,Z:double):integer;
{Level modifications}
Procedure TranslateSectors(lev:TJKLevel;scsel:TSCMultiSel;cursc:integer;dx,dy,dz:double);
Procedure TranslateSurfaces(lev:TJKLevel;sfsel:TSFMultiSel;cursc,cursf:integer;dx,dy,dz:double);
Procedure TranslateVertices(lev:TJKLevel;vxsel:TVXMultiSel;cursc,curvx:integer; dx,dy,dz:double);

Procedure TranslateFrames(lev:TJKLevel;frsel:TFRMultisel;th,fr:integer;dx,dy,dz:double);

Procedure TranslateEdges(lev:TJKLevel;edsel:TEDMultiSel;cursc,cursf,cured:integer; dx,dy,dz:double);

Procedure TranslateThingKeepFrame(Lev:TJKLevel;curth:integer;dx,dy,dz:double);

Procedure TranslateThings(Lev:TJKLevel;thsel:TTHMultiSel;curth:integer;dx,dy,dz:double;moveframes:boolean);
Procedure TranslateLights(Lev:TJKLevel;ltsel:TLTMultiSel;curlt:integer;dx,dy,dz:double);

Procedure RotateSectors(lev:TJKLevel; scsel:TSCMultiSel; axis:Tvector; cx,cy,cz:double; angle:double);
Procedure RotateThings(lev:TJKLevel; thsel:TTHMultiSel; axis:Tvector; cx,cy,cz:double; angle:double);
Procedure RotateLights(lev:TJKLevel; ltsel:TLTMultiSel; axis:Tvector; cx,cy,cz:double; angle:double);
Procedure RotateFrames(lev:TJKLevel; frsel:TFRMultiSel; axis:Tvector; cx,cy,cz:double; angle:double);


Procedure ScaleSectors(lev:TJKLevel; scsel:TSCMultiSel;const sd:TScaleData;scaletx:boolean);
Procedure ScaleThings(lev:TJKLevel; thsel:TTHMultiSel;const sd:TScaleData);
Procedure ScaleLights(lev:TJKLevel; ltsel:TLTMultiSel;const sd:TScaleData);
Procedure ScaleFrames(lev:TJKLevel; frsel:TFRMultiSel;const sd:TScaleData);



Procedure FlipSectors(lev:TJKLevel;scsel:TSCMultiSel;cx,cy,cz:double;how:integer);
Procedure FlipSurfaces(lev:TJKLevel;sfsel:TSFMultiSel);
Procedure FlipThings(lev:TJKLevel;thsel:TTHMultiSel;cx,cy,cz:double;how:integer);

Procedure FlipThingsOverPlane(lev:TJKLevel;thsel:TTHMultiSel; pnorm:TVector; cx,cy,cz:double);
Procedure FlipLightsOverPlane(lev:TJKLevel;ltsel:TLTMultiSel; pnorm:TVector; cx,cy,cz:double);
Procedure FlipFramesOverPlane(lev:TJKLevel;frsel:TFRMultiSel; pnorm:TVector; cx,cy,cz:double);

Procedure FlipSectorsOverPlane(lev:TJKLevel;scsel:TSCMultiSel;const pnorm:TVector;px,py,pz:double);


Function Normalize(var normal:TVector):boolean;

Function GetNearestVX(lev:TJKLevel;px,py,pz:double;var rx,ry,rz:double;mdist:double):boolean;

Procedure ExtrudeSurface(surf:TJKSurface; by:double);
Procedure ExtrudeNExpandSurface(surf:TJKSurface; by:double);

Function CleaveSurface(surf:TJKSurface; const cnormal:tvector; cx,cy,cz:double):boolean;
Function CleaveSector(sec:TJKSector; const cnormal:tvector; cx,cy,cz:double):boolean;
Function CleaveEdge(surf:TJKSurface; edge:integer; const cnormal:tvector; cx,cy,cz:double):boolean;

Function ConnectSurfaces(surf1,surf2:TJKSurface):boolean;
{Creates adjoin by cleaving both surfaces to reduce them to their
 common area}

Function ConnectSectors(sec1,sec2:TJKSector):boolean;

Function BuildSector(lev:TJKLevel;sfsel:TSFMultiSel):TJKSector;
Function BuildSurface(lev:TJKLevel;vxsel:TVXMultiSel):TJKSurface;

Function FlattenSurface(surf:TJKSUrface):boolean;

function MergeSurfaces(Surf,Surf1:TJKSurface):TJKSurface;
function MergeSurfacesAt(Surf:TJKSurface;edge:integer):boolean;
function MergeSectors(sec,sec1:TJKSector):TJKSector;
function MergeVertices(lev:TJKLevel;vxsel:TVXMultiSel):integer;// returns number of vertices merged

Procedure CalcDefaultUVNormals(surf:TJKSurface; orgvx:integer; var un,vn:tvector);
Procedure CalcUVNormals(surf:TJKSurface; var un,vn:tvector);
Procedure CalcUVNormalsFrom(surf:TJKSurface; nv1,nv2:integer; var un,vn:Tvector);
Procedure CalcUV(surf:TJKSurface;orgvx:integer);
Procedure UpdateSurfUVData(surf:TJKSurface;var un,vn:Tvector);
Procedure FindUVScales(surf:TJKSurface);

Procedure ArrangeTexture(surf:TJKSurface; orgvx:integer; const un,vn:tvector);
Procedure ArrangeTextureBy(surf:TJKSurface;const un,vn:tvector;refx,refy,refz,refu,refv:double);
Function IsTXFlipped(surf:TJKSurface):boolean;
Procedure RemoveSurfRefs(Lev:TJKLevel;surf:TJKSurface);
Procedure RemoveSecRefs(Lev:TJKLevel; sec:TJKSector;opt:byte);


{Function StitchAt(surf:TJKSurface;nv1,nv2:integer):integer;

Function StitchAtEdge(surf:TJKSurface;edge:integer):integer;}

Function StitchSurfaces(surf,surf1:TJKSurface):boolean;

Function FindCommonEdges(surf,withsurf:TJKSurface; var v11,v12,v21,v22:integer):boolean;

Procedure CalcLighting(Lev:TJKLevel;scs:TSCMultiSel);
Procedure CalcLightingNew(Lev:TJKLevel;scs:TSCMultiSel);


Procedure DeleteSector(Lev:TJKLevel;n:integer);
Procedure DeleteSurface(Lev:TJKLevel;sc,sf:integer);
Procedure DeleteThing(Lev:TJKLevel;n:integer);
Function DeleteVertex(Sec:TJKSector;vx:integer;silent:boolean):boolean;
Procedure DeleteLight(Lev:TJKLevel;n:integer);
Procedure DeleteCog(Lev:TJKLevel;n:integer);

Function DO_Surf_Overlap(surf,surf1:TJKSurface):boolean;
Function DO_Surf_Match(surf,surf1:TJKSurface):boolean;

Function MakeAdjoin(surf:TJKSurface):boolean;
Function MakeAdjoinSCUP(surf:TJKSurface;sc:integer):boolean;
Function UnAdjoin(surf:TJKSurface):Boolean;
Procedure SysAdjoinSurfaces(surf1,surf2:TJKSurface);
Function AdjoinSectors(sec,sec1:TJKSector):boolean;
Function UnAdjoinSectors(sec,sec1:TJKSector):boolean;

Procedure CreateCube(lev:TJKLevel; x,y,z:double;const pnormal,edge:TVector);

Procedure CreateCubeSec(s:tjksector; x,y,z:double;const pnormal,edge:TVector);

Procedure RotatePoint(ax1,ay1,az1,ax2,ay2,az2:double;angle:double;var x,y,z:double);

Procedure DuplicateSector(sec,newsc:TJKSector;cx,cy,cz:double;nx,ny,nz:Tvector;dx,dy,dz:double);

Procedure GetLevelPal(l:TJKLevel;var pal:TCMPPal);

Procedure OffsetByNode(lev:TJKLevel;nnode:integer;var x,y,z:double);
Procedure UnOffsetByNode(lev:TJKLevel;nnode:integer;var x,y,z:double);

implementation
uses Math, SysUtils, Jed_main, u_tbar, values, u_COGForm;

Type
TXData=record
 un,vn:TVector;
 x,y,z,u,v:double;
end;

Procedure SaveTXData(surf:TJKSurface;vx:integer;var txd:TXData);forward;
Procedure ApplyTXDataToVX(surf:TJKSurface;vx:integer;const txd:TXData);forward;
Procedure ApplyTXData(surf:TJKSurface;const txd:TXData);forward;


Procedure GetLevelPal(l:TJKLevel;var pal:TCMPPal);
begin
 pal:=defcmppal;
 if Level.MasterCMP='' then if level.sectors.count<1 then exit else
 begin
  LoadCmpPal(Level.Sectors[0].ColorMap,pal);
  exit;
 end;
 LoadCmpPal(Level.MasterCMP,pal);
end;

Function NextIdx(n,maxn:integer):integer;
begin
 if n=maxn-1 then result:=0 else Result:=n+1;
end;

Function PrevIdx(n,maxn:integer):integer;
begin
 if n=0 then result:=maxn-1 else Result:=n-1;
end;


Function SysIsInSector(sec:TJKSector;x,y,z:double;onsides:boolean):boolean;
var sf:integer;
    dist:double;
    v:TJKVertex;
begin
 Result:=false;
 for sf:=0 to sec.surfaces.count-1 do
 with sec.surfaces[sf] do
 begin
  v:=Vertices[0];
  dist:=Smult(normal.dx,normal.dy,normal.dz,
              x-v.x,y-v.y,z-v.z);
  case OnSides of
   true: if dist<-0.00001 then exit;
   false: if dist<=0.001 then exit;
  end;

 end;
 result:=true;
end;

Function IsInSector(sec:TJKSector;x,y,z:double):boolean;
begin
 Result:=SysIsInSector(sec,x,y,z,true);
end;

Function IsInSecNS(sec:TJKSector;x,y,z:double):boolean;
begin
 Result:=SysIsInSector(sec,x,y,z,false);
end;

Function DoSectorsOverlap(sec1,sec2:TJKSector):boolean;

Function DoThey(sec1,sec2:TJKSector):boolean;
var i,j:integer;
    v,v0:TJKVertex;
    surf:TJKSurface;
    d:double;
    onright:boolean;
begin
 result:=false;

 for i:=0 to sec1.surfaces.count-1 do
 begin
  surf:=sec1.surfaces[i];
  if surf.vertices.count<1 then continue;
  v0:=surf.vertices[0];

  onright:=false;

  for j:=0 to sec2.vertices.count-1 do
  begin
   v:=sec2.vertices[j];

   With Surf.normal do
   d:=SMult(v.x-v0.x,v.y-v0.y,v.z-v0.z,dx,dy,dz);
    if d>0.01 then
    begin onright:=true; break; end;
  end;
   if not onright then exit;
 end;
 result:=true;
end;

begin
 result:=DoThey(sec1,sec2) and DoThey(sec2,sec1);
end;


Procedure FindBBox(sec:TJKSector;var box:TBox);
var i:integer;
begin
 Box.X1:=999999999999.0;
 Box.X2:=-999999999999.0;
 Box.Y1:=999999999999.0;
 Box.Y2:=-999999999999.0;
 Box.Z1:=999999999999.0;
 Box.Z2:=-999999999999.0;

 For i:=0 to Sec.Vertices.Count-1 do
 With Sec.Vertices[i] do
 begin
  if X<Box.X1 then Box.X1:=X;
  if X>Box.X2 then Box.X2:=X;
  if Y<Box.Y1 then Box.Y1:=Y;
  if Y>Box.Y2 then Box.Y2:=Y;
  if Z<Box.Z1 then Box.Z1:=Z;
  if Z>Box.Z2 then Box.Z2:=Z;
 end;
end;

Procedure FindCenter(sec:TJKSector;var CX,CY,CZ:double);
var XSum,Ysum,ZSum:double;
    i:integer;
begin
 XSum:=0; YSum:=0; ZSum:=0;
 For i:=0 to Sec.Vertices.Count-1 do
 With Sec.Vertices[i] do
 begin
  Xsum:=Xsum+X;
  YSum:=YSum+Y;
  ZSum:=ZSum+Z;
 end;

 CX:=XSum/Sec.Vertices.Count;
 CY:=YSum/Sec.Vertices.Count;
 CZ:=ZSum/Sec.Vertices.Count;
end;

Procedure FindBSphere(sec:TJKSector;var CX,CY,CZ,Rad:double);
var
    mDist:double;
    i:integer;
begin
 FindCenter(Sec,CX,CY,CZ);
 Rad:=0;

 For i:=0 to Sec.Vertices.Count-1 do
 With Sec.Vertices[i] do
 begin
  mDist:=sqrt(sqr(X-CX)+sqr(Y-CY)+sqr(Z-CZ));
  if mDist>Rad then Rad:=mDist;
 end;

end;

Const cbSteps=25;

Function FindCollideBox(sec:TJKSector;const bbox:TBox;
                        CX,CY,CZ:double; var cbox:TBox):boolean;
var
  xplus,xminus,yplus,yminus,zplus,zminus:double;
  x1,x2,y1,y2,z1,z2:double;
  bx1,bx2,by1,by2,bz1,bz2:boolean;
  i:integer;
begin
 Result:=false;

 bx1:=true; bx2:=true;
 by1:=true; by2:=true;
 bz1:=true; bz2:=true;

 xminus:=(bbox.x1-cx)/cbSteps;
 xplus:=(bbox.x2-cx)/cbSteps;
 yminus:=(bbox.y1-cy)/cbSteps;
 yplus:=(bbox.y2-cy)/cbSteps;
 zminus:=(bbox.z1-cz)/cbSteps;
 zplus:=(bbox.z2-cz)/cbSteps;

 x1:=cx+xminus; x2:=cx+xplus;
 y1:=cy+yminus; y2:=cy+yplus;
 z1:=cz+zminus; z2:=cz+zplus;

 if not IsInSecNS(sec,x1,y1,z1) then exit;
 if not IsInSecNS(sec,x1,y1,z2) then exit;
 if not IsInSecNS(sec,x1,y2,z2) then exit;
 if not IsInSecNS(sec,x1,y2,z1) then exit;

 if not IsInSecNS(sec,x2,y1,z1) then exit;
 if not IsInSecNS(sec,x2,y1,z2) then exit;
 if not IsInSecNS(sec,x2,y2,z2) then exit;
 if not IsInSecNS(sec,x2,y2,z1) then exit;

 For i:=0 to cbSteps-1 do
 begin
 if bx1 then
 Repeat
  x1:=x1+xminus;
  if IsInSecNS(sec,x1,y1,z1) and
     IsInSecNS(sec,x1,y1,z2) and
     IsInSecNS(sec,x1,y2,z2) and
     IsInSecNS(sec,x1,y2,z1) then break;
  x1:=x1-xminus;
  bx1:=false;
 Until true;

 if bx2 then
 Repeat
  x2:=x2+xplus;
  if IsInSecNS(sec,x2,y1,z1) and
     IsInSecNS(sec,x2,y1,z2) and
     IsInSecNS(sec,x2,y2,z2) and
     IsInSecNS(sec,x2,y2,z1) then break;
  x2:=x2-xplus;
  bx2:=false;
 Until true;

 if by1 then
 Repeat
  y1:=y1+yminus;
  if IsInSecNS(sec,x1,y1,z1) and
     IsInSecNS(sec,x1,y1,z2) and
     IsInSecNS(sec,x2,y1,z2) and
     IsInSecNS(sec,x2,y1,z1) then break;
  y1:=y1-yminus;
  by1:=false;
 Until true;

 if by2 then
 Repeat
  y2:=y2+yplus;
  if IsInSecNS(sec,x1,y2,z1) and
     IsInSecNS(sec,x1,y2,z2) and
     IsInSecNS(sec,x2,y2,z2) and
     IsInSecNS(sec,x2,y2,z1) then break;
  y2:=y2-xplus;
  by2:=false;
 Until true;

 if bz1 then
 Repeat
  z1:=z1+zminus;
  if IsInSecNS(sec,x1,y1,z1) and
     IsInSecNS(sec,x1,y2,z1) and
     IsInSecNS(sec,x2,y2,z1) and
     IsInSecNS(sec,x2,y1,z1) then break;
  z1:=z1-zminus;
  bz1:=false;
 Until true;

 if bz2 then
 Repeat
  z2:=z2+zplus;
  if IsInSecNS(sec,x1,y1,z2) and
     IsInSecNS(sec,x1,y2,z2) and
     IsInSecNS(sec,x2,y2,z2) and
     IsInSecNS(sec,x2,y1,z2) then break;
  z2:=z2-zplus;
  bz2:=false;
 Until true;

 if not (bx1 or bx2 or by1 or by2 or bz1 or bz2) then break;

 end;
 cbox.x1:=x1; cbox.x2:=x2;
 cbox.y1:=y1; cbox.y2:=y2;
 cbox.z1:=z1; cbox.z2:=z2;
 Result:=true;
end;



function GetAngle(x,y:Double;xa,ya,xb,yb:double):Double;
{calculates angle formed by the X,Z and vertices of the wall}
var  {square of triangle sides}
    d:Double;
    ax,ay,
    bx,by:double;
begin
 ax:=xa-x; ay:=ya-y;
 bx:=xb-x; by:=yb-y;
 d:=(ax*bx+ay*by)/(sqrt(sqr(ax)+sqr(ay))*sqrt(sqr(bx)+sqr(by)));
 Try
 Result:=ArcCos(d);
 except
  on EInvalidOp do if d>1 then Result:=0 else Result:=Pi;
 end;
 {Find the sign of the angle}
 if IsAbove180(x,y,xa,ya,xb,yb) then Result:=2*pi-Result;
end;

(*function GetAngle(x,y:Double;xa,ya,xb,yb:double):Double;
{calculates angle formed by the X,Z and vertices of the wall}
var a2,b2,c2:Double; {square of triangle sides}
    d:Double;
begin
 a2:=Sqr(xa-x)+sqr(ya-y);
 b2:=Sqr(xb-x)+sqr(ya-y);
 c2:=Sqr(xb-xa)+sqr(yb-ya);
 d:=(a2-c2+b2)/(2*sqrt(b2));
 Try
 Result:=ArcCos(d/sqrt(a2));
 except
  on EInvalidOp do if d/sqrt(a2)>1 then Result:=0 else Result:=Pi;
 end;
 {Find the sign of the angle}
 if IsAbove180(x,y,xa,ya,xb,yb) then Result:=2*pi-Result;
end; *)

Function Isabove180(x,y:Double;xa,ya,xb,yb:double):boolean;
var x1,y1,x2,y2:double;
begin
 x1:=x-xb;
 x2:=x-xa;
 y1:=y-yb;
 y2:=y-ya;
 Result:=(x1*y2-x2*y1)>0;
end;



Function DoIntersect(x11,y11,x12,y12,x21,y21,x22,y22:double):boolean;
var b,d,x,y,dy1,dy2,dx1,dx2:double;
begin
Result:=false;
 dy1:=Y12-Y11;
 dy2:=Y22-Y21;
 dx1:=x12-x11;
 dx2:=x22-x21;

 d:=dy1*dx2-dx1*dy2;
 if Abs(d)<0.0001 then exit;

 b:=(dx1*(y21-y11)+dy1*(x11-x21))/d;
 y:=y21+dy2*b;
 x:=X21+dx2*b;
 Result:=(y>=real_min(Y11,y12)) and (y<=real_max(Y11,Y12)) and
         (y>=real_min(Y21,y22)) and (y<=real_max(Y21,Y22));

 Result:=Result and
         (x>=real_min(X11,X12)) and (x<=real_max(X11,X12)) and
         (x>=real_min(X21,X22)) and (x<=real_max(X21,X22));

end;

Procedure CalcNormal(surf:TPolygon;var normal:TVector);
var x1,x2,x3,y1,y2,y3,z1,z2,z3:double;
    n,cmn:integer;
    cmax,cs:double;
begin
 With Surf.Vertices[0] do
 begin x1:=x; y1:=y; z1:=z; end;
 With Surf.Vertices[1] do
 begin x2:=x; y2:=y; z2:=z; end;
 n:=2;
 cmax:=0;cmn:=2;
 While n<Surf.Vertices.Count do
 begin
  With Surf.Vertices[n] do begin x3:=x; y3:=y; z3:=z; end;
  VMult(X2-X1,Y2-Y1,Z2-Z1,X3-X1,Y3-Y1,Z3-Z1,normal.dX,Normal.dY,Normal.dZ);
  cs:=Sqrt(sqr(normal.dx)+sqr(normal.dy)+sqr(normal.dz));
  if cs>0.01 then break;
{  if not IsOnLine(X1,Y1,Z1,X2,Y2,Z2,X3,Y3,Z3) then break;}
  if cs>cmax then
  begin
   cmax:=cs;
   cmn:=n;
  end;
  inc(n);
 end;

 if n>=Surf.Vertices.Count then
 begin
  With Surf.Vertices[cmn] do begin x3:=x; y3:=y; z3:=z; end;
  VMult(X2-X1,Y2-Y1,Z2-Z1,X3-X1,Y3-Y1,Z3-Z1,normal.dX,Normal.dY,Normal.dZ);
  cs:=Sqrt(sqr(normal.dx)+sqr(normal.dy)+sqr(normal.dz));
  {raise Exception.Create('Invalid polygon');}
 end;

 if cs<>0 then
 begin
  normal.dx:=normal.dx/cs;
  normal.dy:=normal.dy/cs;
  normal.dz:=normal.dz/cs;
 end;
end;

(*Function FindTri(surf:TJKSurface;nv1:integer;var nv2,nv3:integer):boolean;
var i:integer;
    v0,v1,v2:TJKVertex;
    n,nvxs:integer;
    a:Tvector;
begin
 result:=false;
 if surf.vertices.count<3 then exit;
 v0:=surf.Vertices[nv1];
 nvxs:=Surf.Vertices.count-1;

 n:=Surf.NextVX(nv1);
 for i:=0 to nvxs-1 do
 begin
  v1:=surf.Vertices[n];
  if (sqr(v1.x-v0.x)+sqr(v1.y-v0.y)+sqr(v1.z-v0.z))>1e-10 then
  begin
   Result:=true;
   nv2:=n;
   break;
  end;
  n:=Surf.NextVX(n);
  dec(nvxs);
 end;

 if not result then exit;

 v1:=surf.Vertices[nv2];
 result:=false;
 n:=Surf.NextVX(nv2);

 for i:=0 to nvxs-1 do
 begin
  v2:=surf.Vertices[n];
  VMult(v0.x-v1.x,v0.y-v1.y,v0.z-v1.z,
        v2.x-v1.x,v2.y-v1.y,v2.z-v1.z,
        a.dx,a.dy,a.dz);

  if Sqr(a.dx)+sqr(a.dy)+sqr(a.dz)>1e-18 then
  begin
   Result:=true;
   nv3:=n;
   break;
  end;
  dec(nvxs);
  n:=Surf.NextVX(n);
 end;

end; *)

Function FindTri(surf:TJKSurface;nv1:integer;var nv2,nv3:integer):boolean;
var i:integer;
    v0,v1,v2:TJKVertex;
    n,nvxs:integer;
    a:Tvector;
    cdist,dist:double;
    cn:integer;
begin
 result:=false;
 if surf.vertices.count<3 then exit;
 v0:=surf.Vertices[nv1];
 nvxs:=Surf.Vertices.count-1;

 n:=Surf.NextVX(nv1);

 cdist:=-1; cn:=-1;

 for i:=0 to nvxs-1 do
 begin
  v1:=surf.Vertices[n];

  dist:=sqr(v1.x-v0.x)+sqr(v1.y-v0.y)+sqr(v1.z-v0.z);
  if dist>cdist then begin cn:=n; cdist:=dist; end;

  if (dist)>1e-4 then
  begin
   Result:=true;
   nv2:=n;
   break;
  end;
  n:=Surf.NextVX(n);
  dec(nvxs);
 end;

 if not result then
 if cn=-1 then exit else nv2:=cn;

 v1:=surf.Vertices[nv2];
 result:=false;
 n:=Surf.NextVX(nv2);

 {Calculate normalized vector perpendicular to the
  selected edge}

 With Surf do
 VMult(v1.x-v0.x,v1.y-v0.y,v1.z-v0.z,
        normal.dx,normal.dy,normal.dz,
        a.dx,a.dy,a.dz);

 If not Normalize(a) then exit;

 cdist:=-1; cn:=-1;

 for i:=0 to nvxs-1 do
 begin
  v2:=surf.Vertices[n];

  dist:=abs(Smult(a.dx,a.dy,a.dz,v2.x-v1.x,v2.y-v1.y,v2.z-v1.z));

  if dist>cdist then begin cn:=n; cdist:=dist; end;

  if dist>1e-4 then
  begin
   Result:=true;
   nv3:=n;
   break;
  end;
  n:=Surf.NextVX(n);
 end;

 if not result then
 if cn<>-1 then begin nv3:=cn; Result:=true; end;

end;

Function IsTXFlipped(surf:TJKSurface):boolean;
var nv1,nv2,nv3:integer;
    v1,v2,v3:TJKVertex;
    tv1,tv2,tv3:TTXVertex;
 begin
 Result:=false;
 nv1:=0;
 if not FindTri(surf,nv1,nv2,nv3) then exit;
 v1:=surf.vertices[nv1];
 v2:=surf.vertices[nv2];
 v3:=surf.vertices[nv3];
 tv1:=surf.txvertices[nv1];
 tv2:=surf.txvertices[nv2];
 tv3:=surf.txvertices[nv3];

 {triangle v1,v2,v3 define the normal, thus angle v1,v2,v3 is
  always <180}

  {Check if angle is <180 in u,V}
  { if u1*v2>u2*v1 - >180}
 Result:=(tv1.u-tv2.u)*(tv3.v-tv2.v)>(tv3.u-tv2.u)*(tv1.v-tv2.v);
end;

Procedure VMult(X1,Y1,Z1,X2,Y2,Z2:double;var X,Y,Z:double);
begin
 X:=y1*z2-y2*z1;
 Y:=x2*z1-x1*z2;
 Z:=x1*y2-x2*y1;
end;

Function SMult(X1,Y1,Z1,X2,Y2,Z2:double):double;
begin
 Result:=X1*X2+Y1*Y2+Z1*Z2;
end;

Function IsOnLine(X1,Y1,Z1,X2,Y2,Z2,X,Y,Z:double):boolean;
var dx,dy,dz:double;
    nx,ny,nz:double;
begin
 dx:=X2-X1; dy:=Y2-Y1; dz:=Z2-Z1;

 if not IsClose(dz,0) then
 begin
  nx:=x1+(z-z1)*dx/dz;
  ny:=y1+(z-z1)*dy/dz;
  nz:=z;
 end
 else
 if not IsClose(dx,0) then
 begin
  nx:=x;
  ny:=y1+(x-x1)*dy/dx;
  nz:=z1+(x-x1)*dz/dx;
 end
 else
 begin
  nx:=x1+(y-y1)*dz/dy;
  ny:=y;
  nz:=z1+(y-y1)*dz/dy;
 end;

 Result:=IsClose(nx,x) and IsClose(ny,y) and IsClose(nz,z);
end;

Function DistToPlane(surf:TPolygon;x,y,z:double):double;
var vx:tvertex;
begin
 vx:=surf.Vertices[0];
 With Surf do
 Result:=Abs(SMult(normal.dX,normal.dy,normal.dz,x-vx.x,y-vx.y,z-vx.z));
end;

{Function DistToPlane(surf:TPolygon;x,y,z:double):double;
var px,py,pz:double;
    b2c2:double;
    a,b,c,d:double;
begin
 a:=surf.normal.dx; b:=surf.normal.dy; c:=surf.normal.dz;
 d:=surf.d;
 b2c2:=sqr(b)+sqr(c);
 px:=(a*(d-c*z-b*y)+x*b2c2)/(sqr(a)+b2c2);

 if IsClose(a,0) then
 begin
  py:=y;
  pz:=z;
 end
 else
 begin
  py:=b/a*(px-x)+y;
  pz:=c/a*(px-x)+z;
 end;
result:=Sqrt(sqr(px-x)+sqr(py-y)+sqr(pz-z));
end;}

Function PlaneLineXn(const normal:TVector;D:double;x1,y1,z1,x2,y2,z2:double;var x,y,z:double):boolean;
var dx,dy,dz:double;
    sm:double;
begin
 dx:=x2-x1; dy:=y2-y1; dz:=z2-z1;
 Result:=false;

 sm:=(normal.dx*dx+normal.dy*dy+normal.dz*dz);
{ if Abs(sm)<0000001 then exit;}
try
 if Not IsClose(dx,0) then
 begin
  x:=(x1*(normal.dy*dy+normal.dz*dz)-dx*(normal.dy*y1+normal.dz*z1-d))/sm;
  y:=y1+dy/dx*(x-x1);
  z:=z1+dz/dx*(x-x1);
  result:=true;
 end else
 if Not IsClose(dy,0) then
 begin
  y:=(y1*(normal.dx*dx+normal.dz*dz)-dy*(normal.dx*x1+normal.dz*z1-d))/sm;
  x:=x1+dx/dy*(y-y1);
  z:=z1+dz/dy*(y-y1);
  result:=true;
  exit;
 end else
 if Not IsClose(dz,0) then
 begin
  z:=(z1*(normal.dx*dx+normal.dy*dy)-dz*(normal.dx*x1+normal.dy*y1-d))/sm;
  y:=y1+dy/dz*(z-z1);
  x:=x1+dx/dz*(z-z1);
  result:=true;
  exit;
 end else begin Result:=false; exit; end;
result:=true;
except
 On Exception do;
end;
end;

Function PlaneLineXnNew(const normal:TVector;pX,pY,pZ,x1,y1,z1,x2,y2,z2:double;var x,y,z:double):boolean;
var
    dist1,dist2:double;
    k:double;
begin
 result:=false;
 dist1:=Smult(x1-px,y1-py,z1-pz,normal.dx,normal.dy,normal.dz);
 dist2:=Smult(x2-px,y2-py,z2-pz,normal.dx,normal.dy,normal.dz);
 if dist1=dist2 then exit;

try
 k:=dist1/(dist1-dist2);
except
 on Exception do exit;
end;
 x:=x1+k*(x2-x1);
 y:=y1+k*(y2-y1);
 z:=z1+k*(z2-z1);
 result:=true;
end;

Procedure ClearVXMarks(lev:TJKLevel);
var s,v:integer;
begin
 for s:=0 to Lev.Sectors.Count-1 do
 With Lev.Sectors[s] do
  for v:=0 to Vertices.Count-1 do Vertices[v].Mark:=0;
end;

Procedure ClearSCMarks(lev:TJKLevel);
var s:integer;
begin
 for s:=0 to Lev.Sectors.Count-1 do Lev.Sectors[s].mark:=0;
end;


Procedure TranslateVX(v:TJKvertex;dx,dy,dz:double);
begin
 if v.mark<>0 then exit;
 v.x:=v.x+dx;
 v.y:=v.y+dy;
 v.z:=v.z+dz;
 v.mark:=1;
end;

Procedure RecalcSector(sec:TJKSector);
var sf,sf1:integer;
begin
ClearSCMarks(sec.level);
 for sf:=0 to sec.surfaces.count-1 do
 with sec.surfaces[sf] do
 begin
  RecalcAll;
  if adjoin=nil then continue;
  if adjoin.sector.mark<>0 then continue;
  for sf1:=0 to Adjoin.Sector.Surfaces.count-1 do
   Adjoin.Sector.Surfaces[sf1].RecalcAll;
   Adjoin.Sector.mark:=0;
 end;
end;


Procedure TranslateSectors(lev:TJKLevel;scsel:TSCMultiSel;cursc:integer;dx,dy,dz:double);
{Procedure TranslateSector(sec:TJKSector;dx,dy,dz:double);}

Procedure MoveSec(sec:TJKSector;dx,dy,dz:double);
var sf1,sf,v,i:integer;
begin
 SaveSecUndo(sec,ch_changed,sc_geo);

 for v:=0 to sec.vertices.count-1 do
  TranslateVX(sec.vertices[v],dx,dy,dz);
 for sf:=0 to sec.surfaces.count-1 do
 with sec.surfaces[sf] do
 begin
  if adjoin=nil then continue;

  SaveSecUndo(adjoin.sector,ch_changed,sc_geo);

  for v:=0 to Adjoin.Vertices.count-1 do
    TranslateVX(Adjoin.vertices[v],dx,dy,dz);
 end;

 RecalcSector(sec);

 JedMain.SectorChanged(sec);

 for i:=0 to sec.surfaces.count-1 do
 with sec.surfaces[i] do
 begin
  if adjoin=nil then continue;
  JedMain.SectorChanged(adjoin.sector);
 end;
end;

var i:integer;
begin
 ClearVXMarks(Lev);
 if cursc>=0 then MoveSec(Lev.Sectors[cursc],dx,dy,dz);
 for i:=0 to scsel.count-1 do
  MoveSec(Lev.Sectors[scsel.getSC(i)],dx,dy,dz);
end;

Procedure TranslateSurfaces(lev:TJKLevel;sfsel:TSFMultiSel;cursc,cursf:integer;dx,dy,dz:double);

Procedure MoveSurf(surf:TJKSurface);
var v,sf:integer;
begin

 SaveSecUndo(surf.sector,ch_changed,sc_geo);

 for v:=0 to surf.vertices.count-1 do
   TranslateVX(surf.vertices[v],dx,dy,dz);

 if surf.adjoin<>nil then
 begin
   SaveSecUndo(surf.adjoin.sector,ch_changed,sc_geo);
  for v:=0 to Surf.Adjoin.Vertices.count-1 do
    TranslateVX(Surf.Adjoin.vertices[v],dx,dy,dz);
 end;

 for sf:=0 to surf.sector.surfaces.count-1 do
  surf.sector.surfaces[sf].RecalcAll;

 if Surf.Adjoin<>nil then
 for sf:=0 to surf.Adjoin.sector.surfaces.count-1 do
  surf.Adjoin.sector.surfaces[sf].RecalcAll;

 JedMain.SectorChanged(surf.sector);
  if surf.adjoin<>nil then JedMain.SectorChanged(surf.adjoin.sector);
end;

var i:integer;
    sc,sf:integer;
begin
 ClearVXMarks(Lev);
 if cursc>=0 then MoveSurf(level.Sectors[cursc].surfaces[cursf]);
 for i:=0 to sfsel.count-1 do
 begin
  sfsel.GetSCSF(i,sc,sf);
  MoveSurf(level.Sectors[sc].surfaces[sf]);
 end;
end;

Procedure TranslateEdges(lev:TJKLevel;edsel:TEDMultiSel;cursc,cursf,cured:integer; dx,dy,dz:double);

Procedure MoveED(surf:TJKSurface;ed:integer);
var nv:integer;
begin
 SaveSecUndo(surf.sector,ch_changed,sc_geo);

 TranslateVX(surf.vertices[ed],dx,dy,dz);
 TranslateVX(surf.vertices[surf.nextVX(ed)],dx,dy,dz);
 for nv:=0 to surf.sector.surfaces.count-1 do
  surf.sector.surfaces[nv].RecalcAll;
 JedMain.SectorChanged(surf.sector);
end;

var i:integer;
    sc,sf,ed:integer;
begin
 ClearVXMarks(lev);
 MoveED(Lev.Sectors[cursc].surfaces[cursf],CurED);
 for i:=0 to edsel.count-1 do
 begin
  edsel.GetSCSFED(i,sc,sf,ed);
  MoveED(Lev.Sectors[sc].surfaces[sf],ED);
 end;
end;


Procedure TranslateVertices(lev:TJKLevel;vxsel:TVXMultiSel;cursc,curvx:integer; dx,dy,dz:double);

Procedure MoveVX(vx:TJKVertex);
var nv:integer;
begin
 SaveSecUndo(vx.sector,ch_changed,sc_geo);

 TranslateVX(vx,dx,dy,dz);
 for nv:=0 to vx.sector.surfaces.count-1 do
  vx.sector.surfaces[nv].RecalcAll;
 JedMain.SectorChanged(vx.sector);
end;

var i:integer;
    sc,vx:integer;
begin
 ClearVXMarks(lev);
 MoveVX(Lev.Sectors[cursc].vertices[curvx]);
 for i:=0 to vxsel.count-1 do
 begin
  vxsel.GetSCVX(i,sc,vx);
  MoveVX(Lev.Sectors[sc].vertices[vx]);
 end;
end;

{Procedure ClearVXMarks(lev:TJKLevel);
var s,v:integer;
begin
 for s:=0 to Lev.Sectors.Count-1 do
 With Lev.Sectors[s] do
  for v:=0 to Vertices.Count-1 do Vertices[v].Mark:=0;
end;}

Procedure TranslateThingKeepFrame(Lev:TJKLevel;curth:integer;dx,dy,dz:double);
var thing:TJKThing;
begin
 thing:=Level.Things[curth];
 SaveThingUndo(thing,ch_changed);
 With thing do
 begin
  x:=x+dx;
  y:=y+dy;
  z:=z+dz;
 end;
 JedMain.ThingChanged(thing);
 JedMain.LayerThing(curth);
end;

Procedure TranslateThings(Lev:TJKLevel;thsel:TTHMultiSel;curth:integer;dx,dy,dz:double;moveframes:boolean);

procedure MoveTH(thing:TJKThing);
var tx,ty,tz,pch,yaw,rol:double;
    i:integer;
begin
 SaveThingUndo(thing,ch_changed);
 With thing do
 begin
  x:=x+dx;
  y:=y+dy;
  z:=z+dz;
 end;

 if moveframes then
 For i:=0 to thing.Vals.count-1 do
 With thing.Vals[i] do
 begin
  if atype<>at_frame then continue;
  GetFrame(tx,ty,tz,pch,yaw,rol);
  SetFrame(tx+dx,ty+dy,tz+dz,pch,yaw,rol);
 end;

 JedMain.ThingChanged(thing);
 JedMain.LayerThing(thing.Num);
end;

var i:integer;
begin
 if thsel.FindTH(curth)=-1 then MoveTH(Lev.Things[curth]);
 for i:=0 to thsel.count-1 do
  MoveTH(Lev.Things[thsel.GetTH(i)]);
end;

Procedure TranslateFrames(lev:TJKLevel;frsel:TFRMultisel;th,fr:integer;dx,dy,dz:double);
var vl:TTPLValue;
    i,n:integer;
    x,y,z,pch,yaw,rol:double;
begin
 n:=frsel.AddFR(th,fr);
 for i:=0 to frsel.count-1 do
 begin
  frsel.GetTHFR(i,th,fr);
  vl:=Lev.Things[th].Vals[fr];
  SaveThingUndo(Lev.Things[th],ch_changed);
  vl.GetFrame(x,y,z,pch,yaw,rol);
  vl.SetFrame(x+dx,y+dy,z+dz,pch,yaw,rol);
 end;
 frsel.DeleteN(n);
end;

Procedure TranslateLights(Lev:TJKLevel;ltsel:TLTMultiSel;curlt:integer;dx,dy,dz:double);

procedure MoveLT(light:TJedLight);
begin
 SaveLightUndo(light,ch_changed);
 With Light do
 begin
  x:=x+dx;
  y:=y+dy;
  z:=z+dz;
 end;
 JedMain.LevelChanged;
end;

var i:integer;
begin
 if ltsel.FindLT(curlt)=-1 then MoveLT(Lev.Lights[curlt]);
 for i:=0 to ltsel.count-1 do
  MoveLT(Lev.Lights[ltsel.GetLT(i)]);
end;




Procedure SetAdjoinParams(sf:TJKSurface);
begin
 sf.SurfFlags:=0;
 sf.FaceFlags:=0;
 sf.geo:=0;
 sf.AdjoinFlags:=7;
end;

Procedure ClearAdjoinParams(sf:TJKSurface);
begin
 sf.SurfFlags:=4;
 sf.FaceFlags:=4;
 sf.geo:=4;
 if sf.material='' then sf.material:='dflt.mat';
end;

Procedure SysAdjoinSurfaces(surf1,surf2:TJKSurface);
begin
 surf1.adjoin:=surf2;
 surf2.adjoin:=surf1;
 SetAdjoinParams(surf1);
 SetAdjoinParams(surf2);
end;


Procedure ExtrudeNExpandSurface(surf:TJKSurface; by:double);
var lev:TJKLevel;
    nsec:TJKSector;
    asurf,osurf,nsurf:TJKSurface;
    nvx:TJKVertex;

    v1,v2,v3,v4:TJKVertex;

    i,v,vc:integer;

    cx,cy,cz:double;
    cnx,cny,cnz:double;

begin
 if surf.adjoin<>nil then exit;

 {CalcSurfCenterNCorner(surf,cx,cy,cz,cnx,cny,cnz);}

 lev:=surf.Sector.level;
 nsec:=lev.NewSector;
 nsec.Assign(surf.Sector);
 nsec.num:=Lev.Sectors.Count;

 SaveSecUndo(nsec,ch_added,sc_both);
 SaveSecUndo(surf.sector,ch_changed,sc_both);

 {Construct adjoin}
 asurf:=nsec.newsurface;
 asurf.Assign(surf);

 v1:=nsec.NewVertex; v2:=nsec.NewVertex;
 v3:=nsec.NewVertex; v4:=nsec.NewVertex;

 CalcSurfRect(surf,v1,v2,v3,v4,by);



 for v:=0 to 3 do
 begin
  {nvx:=nsec.NewVertex;
  nvx.Assign(surf.vertices[v]);}
  asurf.AddVertex(nsec.vertices[v]);
 end;
 nsec.Surfaces.Add(asurf);

 {SysAdjoinSurfaces(surf,asurf);}



 {Construct opposite surface}
 osurf:=nsec.newsurface;
 osurf.Assign(surf);

 nvx:=nsec.NewVertex;
 nvx.x:=v1.x+(-surf.normal.dx)*by;
 nvx.y:=v1.y+(-surf.normal.dy)*by;
 nvx.z:=v1.z+(-surf.normal.dz)*by;

 nvx:=nsec.NewVertex;
 nvx.x:=v2.x+(-surf.normal.dx)*by;
 nvx.y:=v2.y+(-surf.normal.dy)*by;
 nvx.z:=v2.z+(-surf.normal.dz)*by;

 nvx:=nsec.NewVertex;
 nvx.x:=v3.x+(-surf.normal.dx)*by;
 nvx.y:=v3.y+(-surf.normal.dy)*by;
 nvx.z:=v3.z+(-surf.normal.dz)*by;

 nvx:=nsec.NewVertex;
 nvx.x:=v4.x+(-surf.normal.dx)*by;
 nvx.y:=v4.y+(-surf.normal.dy)*by;
 nvx.z:=v4.z+(-surf.normal.dz)*by;

 for v:=0 to 3 do
 begin
  osurf.AddVertex(nsec.vertices[7-v]);
 end;


 nsec.Surfaces.Add(osurf);
 osurf.NewRecalcAll;
 osurf.Planarize;

 {Construct other surfaces}
 vc:=4;
 
 nsurf:=nsec.newsurface; nsurf.Assign(surf);

 for i:=0 to vc-1 do
 begin
  nsurf:=nsec.newsurface;
  nsurf.Assign(surf);

  nsurf.AddVertex(asurf.vertices[i]);

  if i=0 then nsurf.AddVertex(asurf.vertices[vc-1])
  else nsurf.AddVertex(asurf.vertices[i-1]);

  if i=0 then nsurf.AddVertex(osurf.vertices[0])
  else nsurf.AddVertex(osurf.vertices[vc-i]);

  nsurf.AddVertex(osurf.vertices[vc-i-1]);
  nsec.surfaces.Add(nsurf);
 end;
 Lev.Sectors.Add(nsec);
 lev.RenumSecs;
 for i:=0 to nsec.surfaces.count-1 do nsec.surfaces[i].NewRecalcAll;
 nsec.Renumber;

 if not ConnectSurfaces(surf,asurf) then;
 
 JedMain.SectorAdded(nsec);
 JedMain.SectorChanged(surf.sector);
end;


Procedure ExtrudeSurface(surf:TJKSurface; by:double);
var lev:TJKLevel;
    nsec:TJKSector;
    asurf,osurf,nsurf:TJKSurface;
    nvx:TJKVertex;
    i,v,vc:integer;
begin
 if surf.adjoin<>nil then exit;



 lev:=surf.Sector.level;
 nsec:=lev.NewSector;
 nsec.Assign(surf.Sector);
 nsec.num:=Lev.Sectors.Count;

 SaveSecUndo(nsec,ch_added,sc_both);
 SaveSecUndo(surf.sector,ch_changed,sc_val);

 {Construct adjoin}
 asurf:=nsec.newsurface;
 asurf.Assign(surf);
 for v:=surf.vertices.count-1 downto 0 do
 begin
  nvx:=nsec.NewVertex;
  nvx.Assign(surf.vertices[v]);
  asurf.AddVertex(nvx);
 end;
 nsec.Surfaces.Add(asurf);

 SysAdjoinSurfaces(surf,asurf);

 {Construct opposite surface}
 osurf:=nsec.newsurface;
 osurf.Assign(surf);
 for v:=0 to surf.vertices.count-1 do
 begin
  nvx:=nsec.NewVertex;
  With surf.vertices[v] do
  begin
   nvx.x:=x+(-surf.normal.dx)*by;
   nvx.y:=y+(-surf.normal.dy)*by;
   nvx.z:=z+(-surf.normal.dz)*by;
  end;
  osurf.AddVertex(nvx);
 end;
 nsec.Surfaces.Add(osurf);
 osurf.NewRecalcAll;
 osurf.Planarize;

 {Construct other surfaces}
 vc:=surf.vertices.count;

 for i:=0 to vc-1 do
 begin
  nsurf:=nsec.newsurface;
  nsurf.Assign(surf);

  nsurf.AddVertex(asurf.vertices[i]);

  if i=0 then nsurf.AddVertex(asurf.vertices[vc-1])
  else nsurf.AddVertex(asurf.vertices[i-1]);

  if i=0 then nsurf.AddVertex(osurf.vertices[0])
  else nsurf.AddVertex(osurf.vertices[vc-i]);

  nsurf.AddVertex(osurf.vertices[vc-i-1]);
  nsec.surfaces.Add(nsurf);
 end;
 Lev.Sectors.Add(nsec);
 lev.RenumSecs;
 for i:=0 to nsec.surfaces.count-1 do nsec.surfaces[i].NewRecalcAll;
 nsec.Renumber;
 JedMain.SectorAdded(nsec);
 JedMain.SectorChanged(surf.sector);
end;

Type
  TCleaveData=record
   vx:TJKVertex;
  end;
  TCleaveArray=array[0..1000] of TCleaveData;

Procedure SplitVertices(vs:TJKVertices;const cnormal:tvector; cx,cy,cz:double);
{Sets .mark for vertex to
   0 - if vertex is on a plane (defined by cnormal and cx,cy,cz)
   1 - to the right from the plane
   -1 - to the left from then plane}
var i:integer;
    dist:double;
begin
 for i:=0 to vs.count-1 do
 With vs[i] do
 begin
  dist:=SMult(cnormal.dX,cnormal.dy,cnormal.dz,x-cx,y-cy,z-cz);
  if IsClose(dist,0) then Mark:=0
  else if Dist<0 then Mark:=-1
  else mark:=1;
 end;
end;

Function VXEq(v1,v2:TVertex):boolean;
begin
 result:=IsClose(v1.x,v2.x) and IsClose(V1.y,v2.y) and IsClose(v1.z,v2.z);
end;

Procedure InsertVXTintoSurf(surf:TJKSurface;v1,v2:TJKVertex;vx:TJKVertex);
{Inserts vertex VX into surface Surf between vertices v1 and v2}
var i,j:integer;
    v3,v4:TJKVertex;
    changed:boolean;
    txd:TXData;
    asurf:TJKSurface;
begin
 if surf=nil then exit;

{ SaveSecUndo(surf.sector,ch_changed,sc_geo);}

 for j:=0 to surf.sector.surfaces.count-1 do
 With surf.sector.surfaces[j] do
 begin
  changed:=false;
  asurf:=surf.sector.surfaces[j];
  SaveTXData(asurf,0,TXd);

 for i:=vertices.count-1 downto 0 do
 begin
  v3:=vertices[i];
  v4:=vertices[NextVX(i)];
  if VXEq(v3,v1) and VxEq(v4,v2) then
  begin
   InsertVertex(NextVX(i),Sector.AddVertex(vx.x,vx.y,vx.z));
   changed:=true;
  end;

  if VxEq(v3,v2) and VxEq(v4,v1) then
  begin
   InsertVertex(NextVX(i),Sector.AddVertex(vx.x,vx.y,vx.z));
   changed:=true;
  end;
 end;

 if changed then ApplyTXData(asurf,txd);

end;
 surf.sector.renumber;
 JedMain.SectorChanged(surf.sector);
end;

Function SysCleaveSurface(surf:TJKSurface; const cnormal:tvector; cx,cy,cz:double):TJKSurface;
{Returns added surface}
var lev:TJKLevel;
    i,j,ssf,sf:integer;
    cd:^TCleaveArray;
    cd1,cd2:^TCleaveData;
    vc:integer;
    nvxs:integer;
    nsf:TJKSurface;
    d:double;
    v0,v1,v2:TJKVertex;
    nleft,nright:integer;
    dist:double;
    asurf:TJKSurface;
    vxs:TJKVertices;
    ax,ay,az:double;
    rvec:TVector;
    txd,txd1:TXData;
    sfchange:boolean;
begin
 SaveTXData(surf,0,txd);

 Result:=nil;

 d:=cnormal.dX*cX+cnormal.dY*cY+cnormal.dZ*cZ;

 lev:=surf.sector.level;
 vc:=surf.Vertices.Count;
 GetMem(cd,sizeof(TCleaveData)*vc);
try
 FillChar(cd^,sizeof(TCleaveData)*vc,0);

 for sf:=0 to vc-1 do
 begin
  cd1:=@cd^[sf];
  cd2:=@cd^[NextIdx(sf,vc)];
  v1:=Surf.Vertices[sf];
  v2:=Surf.Vertices[NextIdx(sf,vc)];

  if (v1.mark=0) or (V1.mark<>-v2.mark) then continue;

   PlaneLineXn(cnormal,D,v1.x,v1.y,v1.z,v2.x,v2.y,v2.z,ax,ay,az);
   cd1^.vx:=Surf.Sector.AddVertex(ax,ay,az);
 end;

 nleft:=0; nright:=0;
 for sf:=0 to vc-1 do
 With surf.Vertices[sf] do
 begin
  if mark=1 then inc(nright);
  if mark=-1 then inc(nleft);
 end;

 if (nleft=0) or (nright=0) then exit;


 nsf:=Surf.Sector.NewSurface;
 nsf.Assign(Surf);

 {vxs:=TJKVertices.Create;}

 for sf:=0 to Surf.Vertices.Count-1 do
 With cd^[sf] do
 begin
  v1:=surf.vertices[sf];
  if (v1.mark=1) or (v1.mark=0) then nsf.AddVertex(v1);
  if vx<>nil then nsf.AddVertex(vx);
 end;
 surf.sector.surfaces.Add(nsf);
 nsf.NewRecalcAll;

 vxs:=surf.vertices;

 surf.vertices:=TJKVertices.Create;

 for sf:=0 to Vxs.Count-1 do
 With cd^[sf] do
 begin
  v1:=vxs[sf];
  if (v1.mark=-1) or (v1.mark=0) then surf.Vertices.Add(v1);
  if vx<>nil then surf.InsertVertex(Surf.Vertices.Count,vx);
 end;

{ surf.sector.surfaces.Add(nsf);
 nsf.RecalcAll;}

 Surf.RecalcAll;

 {Remove edges that lie on the clipping plane and face the wrong way}

 With surf do  {Find "right" vector}
 VMult(normal.dx,normal.dy,normal.dz,
       cnormal.dx,cnormal.dy,cnormal.dz,
       rvec.dx,rvec.dy,rvec.dz);

 For i:=surf.vertices.count-1 downto 0 do
 begin
  v0:=surf.vertices[Surf.PrevVX(i)];
  v1:=surf.vertices[i];
  v2:=surf.vertices[Surf.NextVX(i)];
  if (v0.mark<>0) or (v1.mark<>0) or (v2.mark<>0) then continue;
  if (Smult(v2.x-v1.x,v2.y-v1.y,v2.z-v1.z,rvec.dx,rvec.dy,rvec.dz)<0) or
     (Smult(v1.x-v0.x,v1.y-v0.y,v1.z-v0.z,rvec.dx,rvec.dy,rvec.dz)<0) then
      surf.vertices.Delete(i);
 end;

 For i:=nsf.vertices.count-1 downto 0 do
 begin
  v0:=nsf.vertices[nsf.PrevVX(i)];
  v1:=nsf.vertices[i];
  v2:=nsf.vertices[nsf.NextVX(i)];
  if (v0.mark<>0) or (v1.mark<>0) or (v2.mark<>0) then continue;
  if (Smult(v2.x-v1.x,v2.y-v1.y,v2.z-v1.z,rvec.dx,rvec.dy,rvec.dz)>0) or
     (Smult(v1.x-v0.x,v1.y-v0.y,v1.z-v0.z,rvec.dx,rvec.dy,rvec.dz)>0) then
  nsf.vertices.Delete(i);
 end;


 For sf:=0 to vc-1 do
 With CD^[sf] do
 begin
  if vx=nil then continue;
  for i:=0 to surf.sector.surfaces.count-1 do
  begin
   asurf:=surf.sector.surfaces[i];
   if (asurf=surf) then continue;
   SaveTXData(asurf,0,txd1);
   sfchange:=false;
   for j:=asurf.vertices.count-1 downto 0 do
   begin
    v1:=asurf.vertices[j];
    v2:=asurf.vertices[NextIdx(j,asurf.vertices.count)];
    if (v1=vxs[sf]) and (v2=vxs[NextIdx(sf,Vxs.count)]) then
    begin
     sfchange:=true;
     aSurf.InsertVertex(aSurf.NextVX(j),vx);
     InsertVXTintoSurf(aSurf.Adjoin,vxs[sf],vxs[NextIdx(sf,Vxs.count)],vx);
    end;

    if (v2=vxs[sf]) and (v1=vxs[NextIDX(sf,vxs.count)]) then
    begin
     sfchange:=true;
     aSurf.InsertVertex(NextIdx(j,asurf.vertices.count),vx);
     InsertVXTintoSurf(aSurf.Adjoin,vxs[sf],vxs[NextIdx(sf,Vxs.count)],vx);
    end;
    if sfchange then ApplyTXData(asurf,txd1);
   end;
  end;
end;

{ Surf.vertices.free;
 Surf.Vertices:=vxs;}

 vxs.free;

{ for i:=0 to nsf.sector.surfaces.count-1 do
  nsf.sector.surfaces[i].RecalcAll;}

 nSf.Sector.Renumber;

 ApplyTXData(surf,txd);
 ApplyTXData(nsf,txd);

 Result:=nsf;

finally
 FreeMem(cd);
end;
end;

Procedure CleaveAdjoin(surf:TJKSurface;cnormal:TVector;cx,cy,cz:double;nsf:TJKSurface);
var
    sf1,nsf1:TJKSurface;
begin
 if nSf=nil then exit;
 if Surf.Adjoin=nil then exit;
 sf1:=Surf.Adjoin;

 SaveSecUndo(sf1.sector,ch_changed,sc_both);

  sf1.Adjoin:=nil;
  surf.Adjoin:=nil;
  SplitVertices(Sf1.Vertices,cnormal,cx,cy,cz);


  nsf1:=SysCleaveSurface(sf1,cnormal,cx,cy,cz);

  if Do_Surf_Overlap(Surf,sf1) then
  begin
   SysAdjoinSurfaces(surf,sf1);
  end;

  if Do_Surf_Overlap(Surf,nsf1) then
  begin
   SysAdjoinSurfaces(nsf1,surf);
  end;

  if Do_Surf_Overlap(nsf,sf1) then
  begin
   SysAdjoinSurfaces(nsf,sf1);
  end;

  if Do_Surf_Overlap(nsf,nsf1) then
  begin
   SysAdjoinSurfaces(nsf,nsf1);
  end;

 JedMain.SectorChanged(sf1.sector);

end;

Function CleaveSurface(surf:TJKSurface; const cnormal:tvector; cx,cy,cz:double):boolean;
var
    nsf:TJKSurface;
begin
 SplitVertices(Surf.Vertices,cnormal,cx,cy,cz);

 SaveSecUndo(surf.sector,ch_changed,sc_both);

 nsf:=SysCleaveSurface(surf,cnormal,cx,cy,cz);
 Result:=nsf<>nil;
 CleaveAdjoin(surf,cnormal,cx,cy,cz,nsf);
 JedMain.SectorChanged(surf.sector);
end;

Function CleaveEdge(surf:TJKSurface; edge:integer; const cnormal:tvector; cx,cy,cz:double):boolean;
var nv1,nv2,v1,v2:TJKVertex;
    nvx:TJKVertex;
    i,j,n1,n2:integer;
    ax,ay,az:double;
    d:double;
begin
 Result:=false;
 SplitVertices(Surf.Vertices,cnormal,cx,cy,cz);
 v1:=Surf.Vertices[edge];
 v2:=Surf.Vertices[Surf.NextVX(edge)];
  if (v1.mark=0) or (V1.mark<>-v2.mark) then exit;

 SaveSecUndo(surf.sector,ch_changed,sc_both);

  d:=cnormal.dx*cx+cnormal.dy*cy+cnormal.dz*cz;

  With nvx do
   PlaneLineXn(cnormal,d,v1.x,v1.y,v1.z,v2.x,v2.y,v2.z,ax,ay,az);

  SaveSecUndo(surf.sector,ch_changed,sc_geo);

  nvx:=Surf.Sector.AddVertex(ax,ay,az);

  surf.InsertVertex(Surf.NextVX(edge),nvx);

 For i:=0 to surf.sector.surfaces.count-1 do
 With surf.sector.Surfaces[i] do
 begin
  for j:=0 to vertices.count-1 do
  begin
   nv1:=Vertices[j];
   nv2:=Vertices[nextVX(j)];
   if ((v1=nv1) and (v2=nv2)) or
       ((v1=nv2) and (v2=nv1)) then
    InsertVertex(nextVX(j),nvx);
  end;
 end;

 surf.sector.Renumber;

 For i:=0 to surf.Sector.Surfaces.count-1 do
  surf.Sector.Surfaces[i].RecalcAll;

JedMain.SectorChanged(surf.Sector);
end;



Procedure RelayerSCThings(sc:TJKSector);
var i,n:integer;
    th:TJKThing;
    lev:TJKLevel;
begin
 lev:=sc.Level;
 for i:=0 to lev.things.count-1 do
 begin
  th:=lev.things[i];
  if th.sec<>sc then continue;
  n:=FindSectorForThing(th);
  if n=-1 then th.sec:=nil else th.sec:=lev.Sectors[n];
 end;
end;

Procedure RemoveSecRefs(Lev:TJKLevel; sec:TJKSector;opt:byte);
var p,i,j:integer;
begin
 if opt and rs_surfs<>0 then
 for i:=0 to Lev.Sectors.count-1 do
 With Lev.Sectors[i] do
 for j:=0 to surfaces.Count-1 do
 With Surfaces[j] do
 begin
  if adjoin=nil then continue;
  if adjoin.sector=sec then
  begin
   SaveSecUndo(Lev.Sectors[i],ch_changed,sc_val);
   adjoin:=nil; ClearAdjoinParams(Surfaces[j]);
   JedMain.SectorChanged(Lev.Sectors[i]);
  end;
 end;

 For i:=0 to Lev.cogs.count-1 do
 With Lev.Cogs[i] do
 for j:=0 to vals.count-1 do
 With Vals[j] do
 begin
  if (Cog_Type=ct_sec) and (obj=sec) then obj:=nil;
  if opt and rs_surfs<>0 then
   if (Cog_type=ct_srf) and (obj<>nil) and (TJKSurface(obj).sector=sec) then obj:=nil;
 end;

 For i:=0 to Lev.Things.Count-1 do
 if Lev.Things[i].sec=sec then
 begin
  Lev.Things[i].sec:=nil;
 end;
end;


Procedure RemoveSurfRefs(Lev:TJKLevel;surf:TJKSurface);
var p,i,j:integer;
begin
 for i:=0 to Lev.Sectors.count-1 do
 With Lev.Sectors[i] do
 for j:=0 to surfaces.Count-1 do
 With Surfaces[j] do
 begin
  if adjoin=nil then continue;
  if adjoin=surf then
  begin
   SaveSecUndo(adjoin.sector,ch_changed,sc_val);
   adjoin:=nil; ClearAdjoinParams(surfaces[j]);
   JedMain.SectorChanged(Lev.Sectors[i]);
  end;
 end;

 For i:=0 to Lev.cogs.count-1 do
 With Lev.Cogs[i] do
 for j:=0 to vals.count-1 do
 With Vals[j] do
 begin
  if (Cog_type=ct_srf) and (obj=surf) then obj:=nil;
 end;

end;


Function CleaveSector(sec:TJKSector; const cnormal:tvector; cx,cy,cz:double):boolean;
var i,sf,sfside:integer;
    nsf,vxs:TJKVertices;
    v1,v2:TJKVertex;
    nv:integer;
    sec1:TJKSector;
    asurf,surf,surf1:TJKSurface;
    lev:TJKLevel;

Procedure AddVXs(vxs:TJKVertices;v1,v2:TJKVertex);
var i:integer;
    cv1,cv2:TJKVertex;
begin
 for i:=0 to vxs.count div 2-1 do
 begin
  cv1:=vxs[i*2]; cv2:=vxs[i*2+1];
  if ((cv1=v1) and (cv2=v2)) or
     ((cv1=v2) and (cv2=v1)) then exit;
 end;
 vxs.Add(v1);
 vxs.Add(v2);
end;

var nleft,nright:integer;
begin
 lev:=Sec.Level;

 SplitVertices(Sec.Vertices,cnormal,cx,cy,cz);


 nleft:=0; nright:=0;
 For i:=0 to Sec.Vertices.Count-1 do
 With Sec.Vertices[i] do
 begin
  if mark=-1 then inc(nleft);
  if mark=1 then inc(nright);
 end;

 if (nleft=0) or (nright=0) then exit;

 SaveSecUndo(sec,ch_changed,sc_both);


 For sf:=0 to Sec.Surfaces.Count-1 do
 begin
   CleaveAdjoin(Sec.Surfaces[sf],cnormal,cx,cy,cz,
    SysCleaveSurface(Sec.Surfaces[sf],cnormal,cx,cy,cz));
 end;

 vxs:=TJKVertices.Create;

try

 for sf:=0 to sec.Surfaces.Count-1 do
 With sec.Surfaces[sf] do
 begin
  nleft:=0;
  for i:=0 to Vertices.Count-1 do
   if Vertices[i].Mark<>0 then begin nleft:=1; break; end;
 {Don't take vertices from surfaces fully lying on the cleaving plane}
  if nleft=0 then continue;

 for i:=0 to Vertices.Count-1 do
 begin
  v1:=Vertices[i];
  v2:=Vertices[NextIdx(i,Vertices.Count)];
  if (v1.mark=0) and (v2.mark=0) then AddVXs(Vxs,v1,v2);
 end;
 end;

 

 if vxs.Count=0 then begin vxs.free; exit; end;


 nsf:=TJKVertices.Create;
 nsf.Add(vxs[0]);
 nsf.Add(vxs[1]);
 v1:=vxs[1];
 vxs.Delete(0);
 vxs.Delete(0);
Repeat
 nv:=vxs.count;
 For sf:=vxs.count div 2-1 downto 0 do
 begin
  if vxs[sf*2]=v1 then
  begin
   v1:=vxs[sf*2+1];
   nsf.Add(v1);
   Vxs.Delete(sf*2);
   Vxs.Delete(sf*2);
   break;
  end;

  if vxs[sf*2+1]=v1 then
  begin
   v1:=vxs[sf*2];
   nsf.Add(v1);
   Vxs.Delete(sf*2);
   Vxs.Delete(sf*2);
   break;
  end;

 end;
Until nv=vxs.count;
vxs.Clear;

if Nsf[nsf.count-1]=nsf[0] then nsf.Delete(nsf.Count-1);

 { Create new sector}

 Sec1:=Lev.NewSector; Sec1.Num:=Lev.Sectors.Count;
 Sec1.Assign(sec);
 Lev.Sectors.Add(Sec1);

 SaveSecUndo(sec1,ch_added,sc_both);


{Create a copy of these vertices - for another sector}
For i:=0 to nsf.count-1 do
begin
 v1:=sec1.NewVertex;
 v1.Assign(nsf[i]);
 vxs.Add(v1);
end;


 {Split surfaces between sectors}

 For sf:=Sec.Surfaces.Count-1 downto 0 do
 begin
  sfside:=0;
  surf:=Sec.Surfaces[sf];
  for i:=0 to surf.vertices.count-1 do
  begin
   sfside:=Surf.Vertices[i].mark;
   if sfside<>0 then break;
  end;

  Case sfside of
   -1: begin
        surf.Sector:=sec1;
        Sec1.Surfaces.Add(Surf);
        Sec.Surfaces.Delete(sf);
       end;
   0: With surf do
      if SMult(cnormal.dx,cnormal.dy,cnormal.dz,normal.dx,normal.dy,normal.dz)<0 then
      begin
        surf.Sector:=sec1;
        Sec1.Surfaces.Add(Surf);
        Sec.Surfaces.Delete(sf);
      end;
  end;
 end;

  {Split vertices between two sectors}
 For sf:=Sec.Vertices.Count-1 downto 0 do
 begin
  v1:=Sec.Vertices[sf];
  case v1.mark of
   -1: begin
        v1.Sector:=Sec1;
        Sec1.Vertices.Add(v1);
        Sec.Vertices.Delete(sf);
       end;
  end;
 end;

 {Now build the adjoining surfaces}

 surf:=Sec.NewSurface;
 For i:=0 to nsf.count-1 do
  surf.AddVertex(nsf[i]);

 surf1:=Sec.NewSurface;
 For i:=nsf.count-1 downto 0 do
  surf1.AddVertex(nsf[i]);

  if sec.surfaces.count>0 then
  begin
   surf.Assign(sec.surfaces[0]);
   surf1.Assign(sec.surfaces[0]);
  end;


  SysAdjoinSurfaces(surf,surf1);

  Surf.NewRecalcAll;
  Surf1.NewRecalcAll;

  With surf do
  if SMult(cnormal.dx,cnormal.dy,cnormal.dz,
           normal.dx,normal.dy,normal.dz)>0 then
  begin
   Sec.Surfaces.Add(surf);
   Surf1.Sector:=sec1;
   Sec1.Surfaces.Add(surf1);
  end
  else
  begin
   Sec.Surfaces.Add(surf1);
   Surf.Sector:=sec1;
   Sec1.Surfaces.Add(surf);
  end;

  {Make copies of vertices that may belong to another sector}
Finally
  For sf:=0 to sec1.Surfaces.Count-1 do
  With Sec1.Surfaces[sf] do
  for i:=0 to Vertices.Count-1 do
  begin
   v1:=Vertices[i];
   if v1.mark<>0 then continue;
   Vertices[i]:=sec1.AddVertex(v1.x,v1.y,v1.z);
{   nv:=nsf.IndexOf(v1);
   if nv<>-1 then Vertices[i]:=vxs[nv];}
  end;

 vxs.free;
 nsf.free;
 Sec.Renumber;
 JedMain.SectorChanged(sec);
 Sec1.Renumber;
 JedMain.SectorAdded(sec1);
 RelayerSCThings(sec);

end;

end;


Function ConnectSurfaces(surf1,surf2:TJKSurface):boolean;
var surf,nsurf1,nsurf2:TJKSurface;
    cx,cy,cz:double;
    d:double;
    i:integer;
    v1,v2:TJKVertex;

Procedure CleaveSurfBySurf(surf,bysurf:TJKSurface);
var i:integer;
    vec:Tvector;
    b:boolean;
begin

 for i:=0 to bysurf.vertices.count-1 do
 begin
  v1:=bysurf.vertices[i];
  v2:=bysurf.vertices[bysurf.NextVX(i)];
  SetVec(vec,v2.x-v1.x,v2.y-v1.y,v2.z-v1.z);
  if not Normalize(vec) then continue;
  Vmult(vec.dx,vec.dy,vec.dz,
        bysurf.normal.dx,bysurf.normal.dy,bysurf.normal.dz,
        vec.dx,vec.dy,vec.dz);
  b:=CleaveSurface(surf,vec,v1.x,v1.y,v1.z);
  if b then;
 end;
end;

begin
 result:=false;
 if (surf1.adjoin<>nil) or (surf2.adjoin<>nil) then
 begin
  PanMessage(mt_warning,'One (or both) surfaces are adjoined!');
  exit;
 end;
 if (surf1.sector=surf2.sector) then
 begin
  PanMessage(mt_warning,'Surfaces belong to the same sector!');
  exit;
 end;

 {Check that surfaces are back to back}

 v1:=surf1.vertices[0];
 v2:=surf2.vertices[0];

  d:=Smult(surf1.normal.dx,surf1.normal.dy,surf1.normal.dz,
       surf2.normal.dx,surf2.normal.dy,surf2.normal.dz);

 if d>-0.9999 then
 begin
  PanMessage(mt_warning,'The surfaces aren''t coplanar!');
  exit;
 end;

 if Abs(Smult(v2.x-v1.x,v2.y-v1.y,v2.z-v1.z,
          surf1.normal.dx,surf1.normal.dy,surf1.normal.dz))>0.0001
 then
 begin
  PanMessage(mt_warning,'The surfaces are not placed back to back');
  exit;
 end;

 CleaveSurfBySurf(surf2,surf1);
 CleaveSurfBySurf(surf1,surf2);

 if DO_Surf_Overlap(surf1,surf2) then
 begin
  SysAdjoinSurfaces(surf1,surf2);
  result:=true;
 end else
 PanMessage(mt_warning,'The resulting surfaces don''t overlap!');
end;

Function ConnectSectors(sec1,sec2:TJKSector):boolean;

Procedure CleaveSecBySec(sec,bysec:TJKSector);
var i:integer;
    vec:Tvector;
    b:boolean;
    surf:TJKSurface;
    v:TJKVertex;
begin

 for i:=0 to bysec.surfaces.count-1 do
 begin
  surf:=bysec.surfaces[i];
  if Vlen(surf.normal)=0 then continue;
  v:=surf.vertices[0];
  b:=CleaveSector(sec,surf.normal,v.x,v.y,v.z);
  if b then;
 end;
end;

var lsec,i:integer;
begin {ConnectSectors}
 result:=false;
 if not DoSectorsOverlap(sec1,sec2) then
 begin
  PanMessage(mt_warning,'The sectors don''t overlap');
  exit;
 end;
 lsec:=sec1.level.sectors.count;
 CleaveSecBySec(sec2,sec1);
 CleaveSecBySec(sec1,sec2);
 DeleteSector(sec2.level,sec2.num);
 for i:=0 to sec1.surfaces.count-1 do
 MakeAdjoinSCUP(sec1.surfaces[i],lsec);

 result:=true;
end;



Function VLen(const v:TVector):double;
begin
 Result:=Sqrt(sqr(v.dx)+sqr(v.dy)+sqr(v.dz));
end;

Function Normalize(var normal:TVector):boolean;
var dist:double;
begin
 Result:=false;
 dist:=Sqrt(sqr(normal.dx)+sqr(normal.dy)+sqr(normal.dz));
 if dist=0 then exit;
 Normal.dx:=Normal.dx/dist;
 Normal.dy:=Normal.dy/dist;
 Normal.dz:=Normal.dz/dist;
 Result:=true;
end;

Procedure CalcDefaultUVNormals(surf:TJKSurface; orgvx:integer; var un,vn:tvector);
var
    v1,v2:TJKVertex;
    i:integer;
begin
 v1:=surf.Vertices[orgvx];
 v2:=surf.Vertices[NextIdx(orgvx,surf.Vertices.Count)];
 With un do
 begin
  dx:=v2.x-v1.x;
  dy:=v2.y-v1.y;
  dz:=v2.z-v1.z;
 end;

 Normalize(un);
 With Surf do VMult(
                    un.dx,un.dy,un.dz,
                    normal.dx,normal.dy,normal.dz,
                    vn.dx,vn.dy,vn.dz);
end;

Function IsVFlipped(surf:TJKSurface;nv1,nv2:integer; const u,v:tvector):boolean;
var i:integer;
    vx,v1,v2:TJKvertex;
    tv,tv1,tv2:TTXVertex;
    dv:double;
begin
 Result:=false;
 v1:=surf.vertices[nv1]; tv1:=surf.txvertices[nv1];
 v2:=surf.vertices[nv2]; tv2:=surf.txvertices[nv2];
{Check if the direction of V normal is correct}
  for i:=0 to surf.vertices.count-1 do
  begin
   vx:=surf.Vertices[i];
   if VxEq(vx,v1) or VxEq(vx,V2) then continue;
   tv:=surf.TXVertices[i];
   dv:=UVRound(SMult(v.dx,v.dy,v.dz,vx.x-v1.x,vx.y-v1.y,vx.z-v1.z)*PixelPerUnit);
   result:=(dv>0)<>((tv.v-tv1.v)>0);
   exit;
  end;
end;

(*Procedure SysCalcUVNormalsFrom(surf:TJKSurface; nv1,nv2:integer; var un,vn:Tvector;flip:boolean);
var vx,v1,v2:TJKVertex;
    tv,tv1,tv2:TTXVertex;
    u,v:tvector;
    du,dv,vl:double;
    i:integer;
begin
 v1:=surf.vertices[nv1]; tv1:=surf.txvertices[nv1];
 v2:=surf.vertices[nv2]; tv2:=surf.txvertices[nv2];
  u.dx:=v2.x-v1.x; u.dy:=v2.y-v1.y; u.dz:=v2.z-v1.z;

  With Surf do VMult(u.dx,u.dy,u.dz,
                     normal.dx,normal.dy,normal.dz,
                     v.dx,v.dy,v.dz);

  If flip then SetVec(v,-v.dx,-v.dy,-v.dz);

  Normalize(u); Normalize(v);

  du:=tv2.u-tv1.u;
  dv:=tv2.v-tv1.v; vl:=sqr(du)+sqr(dv);
  if isClose(vl,0) then begin CalcDefaultUVNormals(surf,0,un,vn); exit; end;
  du:=du/vl;
  dv:=dv/vl;

  un.dx:=du*u.dx-dv*v.dx;
  un.dy:=du*u.dy-dv*v.dy;
  un.dz:=du*u.dz-dv*v.dz;

  vn.dx:=dv*u.dx+du*v.dx;
  vn.dy:=dv*u.dy+du*v.dy;
  vn.dz:=dv*u.dz+du*v.dz;

  {Check normals}
  Normalize(un);
  Normalize(vn);
{  i:=surf.NextVX(nv1);
  if i=nv2 then i:=surf.NextVX(nv2);
  v2:=surf.Vertices[i];
  du:=UVRound(tv1.u+SMult(un.dx,un.dy,un.dz,v2.x-v1.x,v2.y-v1.y,v2.z-v1.z)*PixelPerUnit);
  dv:=UVRound(tv1.v+SMult(vn.dx,vn.dy,vn.dz,v2.x-v1.x,v2.y-v1.y,v2.z-v1.z)*PixelPerUnit);
  With surf.TxVertices[i] do
  if (Abs(u-du)<1) and (Abs(v+dv)<1) then
  begin
   vn.dx:=-vn.dx;
   vn.dy:=-vn.dy;
   vn.dz:=-vn.dz;
  end;}

end; *)

Procedure UpdateSurfUVData(surf:TJKSurface;var un,vn:Tvector);
var cosa,sina:double;
    a,b:Tvector;
begin
{ if not surf.GetRefVector(a) then exit;
 cosa:=SMult(a.dx,a.dy,a.dz,un.dx,un.dy,un.dz);
 sina:=sqrt(1-sqr(cosa));
 VMult(a.dx,a.dy,a.dz,un.dx,un.dy,un.dz,b.dx,b.dy,b.dz);
 With Surf.Normal do
 if SMult(dx,dy,dz,b.dx,b.dy,b.dz)<0 then sina:=-sina;
 surf.ucosa:=cosa+16;
 surf.usina:=sina;}
end;

Procedure FindUVScales(surf:TJKSurface);
var un,vn:TVector;
    v1,v2:TJKVertex;
    tv1,tv2:TTXVertex;
    d1,d2:double;
    nv2,nv3:integer;
begin
 CalcUVNormals(surf,un,vn);

 if not FindTri(surf,0,nv2,nv3) then exit;

 v1:=surf.vertices[0]; tv1:=surf.txvertices[0];
 v2:=surf.vertices[nv2]; tv2:=surf.txvertices[nv2];

 d1:=tv2.u-tv1.u;
 if d1=0 then
 begin
  v2:=surf.vertices[nv3]; tv2:=surf.txvertices[nv3];
  d1:=tv2.u-tv1.u;
 end;

 d2:=SMult(un.dx,un.dy,un.dz,v2.x-v1.x,v2.y-v1.y,v2.z-v1.z)*PixelPerUnit;
 surf.uscale:=d1/d2;

 v2:=surf.vertices[nv2]; tv2:=surf.txvertices[nv2];

 d1:=tv2.v-tv1.v;
 if d1=0 then
 begin
  v2:=surf.vertices[nv3]; tv2:=surf.txvertices[nv3];
  d1:=tv2.v-tv1.v;
 end;

 d2:=SMult(vn.dx,vn.dy,vn.dz,v2.x-v1.x,v2.y-v1.y,v2.z-v1.z)*PixelPerUnit;
 surf.vscale:=d1/d2;


end;

Procedure SysCalcUVNormalsFrom(surf:TJKSurface; nv1:integer; var un,vn:Tvector);
var nv2,nv3:integer;
    v1,v2,v3:TJKVertex;
    tv1,tv2,tv3:TTXVertex;
    a,b,av:TVector;
    du1,dv1,du2,dv2:double;
    d:double;
begin
{ if surf.ucosa>0 then
 begin
  if not surf.GetRefVector(a) then
  begin
   PanMessageFmt(mt_warning,'Zero length of first edge in sector %d surface %d',[surf.sector.num,surf.num]);
   SetVec(un,0,0,0);
   SetVec(vn,0,0,0);
   exit;
  end;

  With Surf.Normal do
  VMult(dx,dy,dz,a.dx,a.dy,a.dz,b.dx,b.dy,b.dz);

  du1:=surf.ucosa-16;
  dv1:=surf.usina;

  un.dx:=du1*a.dx+dv1*b.dx;
  un.dy:=du1*a.dy+dv1*b.dy;
  un.dz:=du1*a.dz+dv1*b.dz;

  un.dx:=-dv1*a.dx+du1*b.dx;
  un.dy:=-dv1*a.dy+du1*b.dy;
  un.dz:=-dv1*a.dz+du1*b.dz;
  exit;
 end;}

 if not FindTri(surf,nv1,nv2,nv3) then
 begin
  SetVec(un,0,0,0);
  SetVec(vn,0,0,0);
  exit;
 end;

 v1:=surf.vertices[nv1]; tv1:=surf.txvertices[nv1];
 v2:=surf.vertices[nv2]; tv2:=surf.txvertices[nv2];
 v3:=surf.vertices[nv3]; tv3:=surf.txvertices[nv3];
 SetVec(a,v1.x-v2.x,v1.y-v2.y,v1.z-v2.z);
 SetVec(b,v3.x-v2.x,v3.y-v2.y,v3.z-v2.z);
 du1:=tv1.u-tv2.u; dv1:=tv1.v-tv2.v;
 du2:=tv3.u-tv2.u; dv2:=tv3.v-tv2.v;

 un.dx:=dv2*a.dx-dv1*b.dx;
 un.dy:=dv2*a.dy-dv1*b.dy;
 un.dz:=dv2*a.dz-dv1*b.dz;

 vn.dx:=du2*a.dx-du1*b.dx;
 vn.dy:=du2*a.dy-du1*b.dy;
 vn.dz:=du2*a.dz-du1*b.dz;

 if dv2*du1<dv1*du2 then SetVec(un,-un.dx,-un.dy,-un.dz);
 if du2*dv1<du1*dv2 then SetVec(vn,-vn.dx,-vn.dy,-vn.dz);

 if not Normalize(un) then
 begin
  un:=a;
  normalize(un);
 end;

 With Surf.Normal do
  VMult(dx,dy,dz,un.dx,un.dy,un.dz,av.dx,av.dy,av.dz);

{ if Smult(vn.dx,vn.dy,vn.dz,av.dx,av.dy,av.dz)>=0 then vn:=av
 else SetVec(vn,-av.dx,-av.dy,-av.dz);}

 vn:=av;
 UpdateSurfUVData(surf,un,vn);
end;

Procedure CalcUVNormalsFrom(surf:TJKSurface; nv1,nv2:integer; var un,vn:Tvector);
begin
 SysCalcUVNormalsFrom(surf,nv1,un,vn);
end;

Procedure CalcUVNormalsFromVX(surf:TJKSurface; nvx:integer; var un,vn:tvector);
begin
 SysCalcUVNormalsFrom(surf,nvx,un,vn);
end;

Procedure CalcUVNormals(surf:TJKSurface; var un,vn:tvector);
begin
 CalcUVNormalsFromVX(surf,0,un,vn);
end;

Procedure ArrangeTextureBy(surf:TJKSurface;const un,vn:tvector;refx,refy,refz,refu,refv:double);
var i:integer;
begin
 for i:=0 to Surf.Vertices.Count-1 do
 With Surf.TXVertices[i],Surf.Vertices[i] do
 begin
  u:=UVRound(refu+SMult(un.dx,un.dy,un.dz,x-refx,y-refy,z-refz)*PixelPerUnit*surf.uscale);

  if Surf.SurfFlags and SFF_Flip<>0 then
  v:=UVRound(refv+SMult(-vn.dx,-vn.dy,-vn.dz,x-refx,y-refy,z-refz)*PixelPerUnit*surf.vscale)
  else v:=UVRound(refv+SMult(vn.dx,vn.dy,vn.dz,x-refx,y-refy,z-refz)*PixelPerUnit*surf.vscale)
 end;
end;

Procedure ApplyTXDataToVX(surf:TJKSurface;vx:integer;const txd:TXData);
begin
 With Surf.TXVertices[vx],Surf.Vertices[vx] do
 begin
  u:=UVRound(txd.u+SMult(txd.un.dx,txd.un.dy,txd.un.dz,x-txd.x,y-txd.y,z-txd.z)*PixelPerUnit*surf.uscale);
  v:=UVRound(txd.v+SMult(txd.vn.dx,txd.vn.dy,txd.vn.dz,x-txd.x,y-txd.y,z-txd.z)*PixelPerUnit*surf.vscale);
 end;
end;

Procedure ArrangeTexture(surf:TJKSurface; orgvx:integer; const un,vn:tvector);
begin
 With surf.vertices[orgvx],surf.txvertices[orgvx] do
  ArrangeTextureBy(surf,un,vn,x,y,z,u,v);
end;

Procedure SaveTXData(surf:TJKSurface;vx:integer;var txd:TXData);
begin
try
 With surf.vertices[vx],surf.txvertices[vx] do
 begin
  txd.x:=x;txd.y:=y;txd.z:=z;
  txd.u:=u;txd.v:=v;
 end;
 CalcUVNormals(surf,txd.un,txd.vn);
except
 On Exception do FillChar(txd,sizeof(txd),0);
end;
end;

Procedure ApplyTXData(surf:TJKSurface;const txd:TXData);
begin
 With txd do
  ArrangeTextureBy(surf,un,vn,x,y,z,u,v);
end;


Procedure CalcUV(surf:TJKSurface;orgvx:integer);
var un,vn:tvector;
begin
 CalcDefaultUVNormals(surf,orgvx,un,vn);
 ArrangeTexture(surf,orgvx,un,vn);
end;

Function IsPointWithInLine(x,y,z,x1,y1,z1,x2,y2,z2:double):boolean;
begin
 result:=
   (x>=Real_Min(x1,x2)) and (x<=Real_Max(x1,x2)) and
   (y>=Real_Min(y1,y2)) and (y<=Real_Max(y1,y2)) and
   (z>=Real_Min(z1,z2)) and (z<=Real_Max(z1,z2));
end;

Function IsPointOnSurface(surf:TJKSurface;x,y,z:double):boolean;
var
    vc,i:integer;
    v1,v2:TJKVertex;
    dist:double;
    vct:TVector;
    x1,x2,y1,y2,z1,z2:double;
begin
 Result:=false;

  vc:=Surf.Vertices.Count;

{  x1:=999999999;x2:=-999999999;
  y1:=999999999;y2:=-999999999;
  z1:=999999999;z2:=-999999999;

 For i:=0 to vc-1 do
 With Surf.Vertices[i] do
 begin
  if x<x1 then x1:=x; if x>x2 then x2:=x;
  if y<y1 then y1:=y; if y>y2 then y2:=y;
  if z<z1 then z1:=z; if z>x2 then z2:=z;
 end;

 if (x<x1) or (x>x2) or (y<y1) or (y>y2) or (z<z1) or (z>z2) then exit; }

 For i:=0 to vc-1 do
 begin
  v1:=Surf.Vertices[i];
  v2:=Surf.Vertices[NextIdx(i,vc)];
  With Surf do
   VMult(normal.dx,normal.dy,normal.dz,v2.x-v1.x,v2.y-v1.y,v2.z-v1.z,
         vct.dx,vct.dy,vct.dz);
  dist:=SMult(vct.dx,vct.dy,vct.dz,
              x-v1.x,y-v1.y,z-v1.z);
  if (Dist<-CloseEnough) then exit;
 { if dist<=0 then exit;}
 end;
 Result:=true;
end;


Function FindSectorForXYZ(lev:TJKLevel;X,Y,Z:double):integer;
var i:integer;
begin
 Result:=-1;
 for i:=0 to Lev.Sectors.Count-1 do
 if IsInSector(Lev.Sectors[i],x,y,z) then
  begin
   Result:=i;
   exit;
  end;
end;


Function FindSectorForThing(thing:TJKThing):Integer;
var i:integer;
begin
 Result:=-1;
 for i:=0 to Thing.Level.Sectors.Count-1 do
 if IsInSector(Thing.Level.Sectors[i],thing.x,thing.y,thing.z) then
  begin
   Result:=i;
   exit;
  end;
end;

Const MaxLight=10;

Function DoBoxesIntersect(const box1,box2:TBox):boolean;
begin
 result:=false;
 if (box1.x2<box2.x1)then exit;
 if (box1.x1>box2.x2) then exit;
 if (box1.y2<box2.y1)then exit;
 if (box1.y1>box2.y2) then exit;
 if (box1.z2<box2.z1)then exit;
 if (box1.z1>box2.z2) then exit;
 result:=true;
end;

Function IsPointInBox(const box:TBox; x,y,z:double):boolean;
begin
 result:=(x-box.x1>=-CloseEnough) and (x-box.x2<=CloseEnough) and
         (y-box.y1>=-CloseEnough) and (y-box.y2<=CloseEnough) and
         (z-box.z1>=-CloseEnough) and (z-box.z2<=CloseEnough);
end;

Procedure SetBox(var box:TBox; x1,x2,y1,y2,z1,z2:double);
begin
 box.x1:=real_min(x1,x2);
 box.x2:=real_max(x1,x2);
 box.y1:=real_min(y1,y2);
 box.y2:=real_max(y1,y2);
 box.z1:=real_min(z1,z2);
 box.z2:=real_max(z1,z2);
end;

Function Do2DBoxesIntersect(const box1,box2:T2DBox):boolean;
begin
 result:=false;
 if (box1.x2<box2.x1)then exit;
 if (box1.x1>box2.x2) then exit;
 if (box1.y2<box2.y1)then exit;
 if (box1.y1>box2.y2) then exit;
 result:=true;
end;

Function IsPointIn2DBox(const box:T2DBox; x,y:double):boolean;
begin
 result:=(x-box.x1>=-CloseEnough) and (x-box.x2<=CloseEnough) and
         (y-box.y1>=-CloseEnough) and (y-box.y2<=CloseEnough);
end;

Procedure Set2DBox(var box:T2DBox; x1,x2,y1,y2:double);
begin
 box.x1:=real_min(x1,x2);
 box.x2:=real_max(x1,x2);
 box.y1:=real_min(y1,y2);
 box.y2:=real_max(y1,y2);
end;


Type
    TLCSurfData=record
     bbox:TBox;
     D:double;
    end;
    PLCSurfData=^TLCSurfData;

Procedure CalcSurfBBox(surf:TJKSurface;var box:TBox);
var i:integer;
begin
 box.x1:=999999999; box.x2:=-999999999;
 box.y1:=999999999; box.y2:=-999999999;
 box.z1:=999999999; box.z2:=-999999999;
 for i:=0 to surf.vertices.count-1 do
 With surf.vertices[i] do
 begin
  if x<box.x1 then box.x1:=x;
  if x>box.x2 then box.x2:=x;
  if y<box.y1 then box.y1:=y;
  if y>box.y2 then box.y2:=y;
  if z<box.z1 then box.z1:=z;
  if z>box.z2 then box.z2:=z;
 end;
end;


Function BuildSector(lev:TJKLevel;sfsel:TSFMultiSel):TJKSector;
var sec,nsec:TJKSector;
    surf:TJKSurface;
    i,j:integer;
    sc,sf,asc,asf:integer;
    tv:TTXVertex;
begin
 Result:=nil;
 if sfsel.count<4 then exit;
 sfsel.GetSCSF(0,sc,sf);
 for i:=1 to sfsel.count-1 do
 begin
  sfsel.GetSCSF(i,asc,asf);
  if asc<>sc then exit;
 end;

 sec:=level.sectors[sc];
 nsec:=level.NewSector;

 {Clear surface and vertex marks marks}
 for i:=0 to sec.surfaces.count-1 do sec.surfaces[i].mark:=0;
 for i:=0 to sec.vertices.count-1 do sec.vertices[i].mark:=0;

 {Copy surfaces and vertices to new sector}
 for i:=0 to sfsel.count-1 do
 begin
  sfsel.getSCSF(i,sc,sf);
  surf:=sec.surfaces[sf];
  surf.mark:=1;
  surf.sector:=nsec;
  for j:=0 to surf.vertices.count-1 do
  With surf.Vertices[j] do
   surf.Vertices[j]:=nsec.AddVertex(x,y,z);
  nsec.surfaces.add(surf);
 end;

{Mark used vertices and remove surfaces}
 for i:=sec.surfaces.count-1 downto 0 do
 begin
  surf:=sec.surfaces[i];
  for j:=0 to surf.vertices.count-1 do Inc(surf.vertices[j].mark);
  if surf.mark=1 then sec.surfaces.delete(i);
end;

 lev.sectors.Add(nsec);
 nsec.renumber;

 {Delete unused vertices}
 for i:=sec.vertices.count-1 downto 0 do if sec.vertices[i].mark=0 then
  sec.vertices.delete(i);

 if (sec.vertices.count=0) or (sec.surfaces.count=0) then
  DeleteSector(lev,sc);

 Lev.RenumSecs;
 JedMain.SectorAdded(nsec);
 result:=nsec;
end;

Function BuildSurface(lev:TJKLevel;vxsel:TVXMultiSel):TJKSurface;
var i,asc,avx,sc,vx:integer;
    sec:TJKSector;
    surf:TJKSurface;
begin
 Result:=nil;
 if vxsel.count<3 then exit;
 vxsel.GetSCVX(0,sc,vx);

 for i:=1 to vxsel.count-1 do
 begin
  vxsel.GetSCVX(i,asc,avx);
  if asc<>sc then exit;
 end;

 sec:=Lev.Sectors[sc];
 surf:=sec.NewSurface;
 sec.surfaces.add(surf);

 for i:=0 to vxsel.count-1 do
 begin
  vxsel.GetSCVX(i,asc,avx);
  surf.AddVertex(sec.Vertices[avx]);
 end;

 surf.NewRecalcAll;
 sec.Renumber;
 JedMain.SectorChanged(sec);
 Result:=surf;
end;

Procedure DeleteSector(Lev:TJKLevel;n:integer);
var sec:TJKSector;
begin
 sec:=Lev.Sectors[n];
 SaveSecUndo(sec,ch_deleted,sc_both);

 Lev.Sectors.Delete(n);
 RemoveSecRefs(Lev,sec,rs_surfs);
 JedMain.SectorDeleted(sec);
 sec.Free;
 Lev.RenumSecs;
end;

Procedure DeleteSurface(Lev:TJKLevel;sc,sf:integer);
var sec:TJKSector;
    surf:TJKSurface;
begin
  sec:=Lev.Sectors[sc];
  surf:=sec.surfaces[sf];
  RemoveSurfRefs(lev,surf);
  sec.surfaces.delete(sf);
  Sec.Renumber;
  JedMain.SectorChanged(sec);
end;

Procedure DeleteThing(Lev:TJKLevel;n:integer);
var sec:TJKSector;
    p,i,j:integer;
    th:TJKThing;
begin
 th:=Lev.Things[n];

 SaveThingUndo(th,ch_deleted);

For i:=0 to Lev.cogs.count-1 do
 With Lev.Cogs[i] do
 for j:=0 to vals.count-1 do
 With Vals[j] do
 begin
  if (Cog_Type=ct_thg) and (obj=th) then obj:=nil;
 end;
 JedMain.ThingDeleted(th);

 th.free;
 Lev.Things.Delete(n);
 Lev.RenumThings;
end;

Function DeleteVertex(Sec:TJKSector;vx:integer;silent:boolean):boolean;
var asf,sf1,sf2:TJKSurface;
    v1:TJKVertex;
    ns,sf,iv:integer;
    ed1,ed2:integer;
begin
 sf1:=nil;
 sf2:=nil;
 result:=false;
 v1:=Sec.Vertices[vx];
 ns:=0;
 for sf:=0 to Sec.Surfaces.Count-1 do
 begin
  asf:=Sec.Surfaces[sf];
  iv:=aSf.Vertices.IndexOf(v1);
  if iv=-1 then continue;
  Case ns of
   0: begin sf1:=asf; ed1:=iv; end;
   1: begin sf2:=asf; ed2:=iv; end;
   else begin
         if not silent then PanMessage(mt_error,'More than two surfaces use this vertex');
         exit;
        end;
  end;
  inc(ns);
 end;

 if sf2<>nil then
 begin
  if (sf1.vertices.count<4) or (sf2.vertices.count<4) then
  begin
   if not silent then PanMessage(mt_error,'One of the surfaces has less than 4 vertices!');
   exit;
  end;

  {if (sf1.adjoin<>nil) or (sf2.adjoin<>nil) then
  begin
   PanMessage(mt_error,'Can''t delete vertices of adjoined surfaces');
   exit;
  end;}

  if (sf1.Vertices[sf1.PrevVX(ed1)]<>Sf2.Vertices[sf2.NextVX(ed2)]) or
     (sf1.Vertices[sf1.NextVX(ed1)]<>Sf2.Vertices[sf2.PrevVX(ed2)]) then exit;
 end;

 SaveSecUndo(sec,ch_changed,sc_geo);

 if sf1<>nil then Sf1.DeleteVertex(ed1);
 if sf2<>nil then sf2.DeleteVertex(ed2);

 Sec.Vertices.Delete(vx);
 Sec.Renumber;
 JedMain.SectorChanged(sec);
 result:=true;
end;

Procedure DeleteLight(Lev:TJKLevel;n:integer);
begin
 SaveLightUndo(Lev.Lights[n],ch_deleted);
 Lev.Lights[n].Free;
 Lev.Lights.Delete(n);
 JedMain.LevelChanged;
end;

Procedure DeleteCog(Lev:TJKLevel;n:integer);
var i,j:integer;
    cg:TCOG;
begin
 cg:=lev.cogs[n];
 lev.cogs.Delete(n);
 cg.free;
 for i:=0 to lev.cogs.count-1 do
 with lev.cogs[i] do
 begin
  for j:=0 to vals.count-1 do
  with vals[j] do
  if cog_type=ct_cog then
  begin
   if int=n then int:=-1;
   if int>n then dec(int);
  end;
 end;
 CogForm.RefreshList;
end;


Function SysFindCommonEdge(surf1,surf2:TJKSurface;var ed1,ed2:integer):boolean;
var v1,v2:TJKVertex;
    i,j:integer;
begin
 result:=false;
 for i:=0 to surf1.vertices.count-1 do
 begin
  v1:=surf1.vertices[i];
  j:=surf2.vertices.IndexOf(v1);
  if j=-1 then continue;
  if surf1.Vertices[Surf1.NextVX(i)]=surf2.Vertices[Surf2.PrevVX(j)] then
  begin
   ed1:=i;
   ed2:=Surf2.PrevVX(j);
   result:=true;
   exit;
  end;
 end;
end;

Function SysMergeSurfs(surf1,surf2:TJKSurface;edge1,edge2:integer):TJKSUrface;
var i,j:integer;
    ipos,nv:integer;
    v,lastv:integer;
    vx:TJKVertex;
    sf1,sf2:TJKSurface;

Procedure DeleteVXIfunused(sec:TJKSector;v:TJKVertex);
var i:integer;
    surf:TJKSurface;
begin
 for i:=0 to sec.surfaces.count-1 do
 begin
  surf:=sec.surfaces[i];
  if surf=surf2 then continue;
  if surf.Vertices.IndexOf(v)<>-1 then exit;
 end;
 i:=sec.vertices.IndexOf(v);
 if i=-1 then exit;
 sec.vertices.delete(i);
 v.free;
end;

begin
 result:=nil;
 nv:=0;

 if ((surf1.adjoin=nil) and (surf2.adjoin<>nil)) or
    ((surf2.adjoin=nil) and (surf1.adjoin<>nil)) then
    begin
     PanMessage(mt_error,'Can''t merge - one surface is adjoined, the other isn''t');
     exit;
    end;

 for i:=0 to surf1.vertices.count-1 do
 begin
  vx:=surf1.vertices[i];
  if surf2.Vertices.IndexOf(vx)=-1 then inc(nv);
 end;

 for i:=0 to surf2.vertices.count-1 do
 begin
  vx:=surf2.vertices[i];
  if surf1.Vertices.IndexOf(vx)=-1 then inc(nv);
 end;

 if nv>22 then begin PanMessage(mt_error,'Can''t merge surfaces - resulting surface is >24 vertices'); exit; end;

 {Merge surfaces}

 SaveSecUndo(surf1.sector,ch_changed,sc_both);

 v:=Surf2.PrevVX(edge2); lastv:=Surf2.NextVX(edge2);
 ipos:=Surf1.NextVX(edge1);
 While v<>lastv do
 begin
  Surf1.InsertVertex(ipos,Surf2.vertices[v]);
  v:=Surf2.PrevVX(v);
 end;

 repeat
  nv:=0;
  With surf1 do
  for i:=vertices.count-1 downto 0 do
  begin
   if Vertices[PrevVX(i)]=Vertices[NextVX(i)] then
   begin
    vx:=Vertices[i];
    Surf1.DeleteVertex(i);
    DeleteVXIfunused(surf1.sector,vx);
    inc(nv);
   end;
  end;

  With surf1 do
  for i:=vertices.count-1 downto 0 do
  begin
   if Vertices[i]=Vertices[NextVX(i)] then
   begin
    Surf1.DeleteVertex(i);
    inc(nv);
   end;
  end;


 Until nv=0;

 Surf1.RecalcAll;

 {if the surfaces were adjoined, try to merge the adjoins and readjoin}

 if (surf1.adjoin<>nil) and (surf2.adjoin<>nil) then
 begin
  sf1:=surf1.adjoin;
  sf2:=surf2.adjoin;

  ClearAdjoinParams(surf1);
  ClearAdjoinParams(surf2);

  if SysFindCommonEdge(sf1,sf2,i,j) then
  begin
   ClearAdjoinParams(sf1);
   ClearAdjoinParams(sf2);
   SysMergeSurfs(sf1,sf2,i,j);
   if DO_Surf_Overlap(surf1,sf1) then
   begin
    SysAdjoinSurfaces(surf1,sf1);
    JedMain.SectorChanged(sf1.sector);
   end;
  end;

 end;

 RemoveSurfRefs(Surf1.Sector.Level,surf2);
 nv:=surf2.Sector.Surfaces.IndexOf(surf2);
 surf2.Sector.Surfaces.Delete(nv);
 Surf2.Free;

 surf1.sector.Renumber;
 JedMain.SectorChanged(surf1.sector);

 result:=surf1;
end;

function MergeSurfaces(Surf,Surf1:TJKSurface):TJKSUrface;
var ed1,ed2:integer;
begin
 result:=nil;
 if SysFindCommonEdge(surf,surf1,ed1,ed2) then
  result:=SysMergeSurfs(surf,surf1,ed1,ed2);
  if Result<>nil then
  begin
   if not IsSurfPlanar(result) then PanMessageFmt(mt_info,
   'The surface %d,%d is not planar',[surf.sector.num,surf.num]);
   if not IsSurfConvex(result) then PanMessageFmt(mt_info,
   'The surface %d,%d is not convex',[surf.sector.num,surf.num]);

  end;
end;


function MergeSurfacesAt(Surf:TJKSurface;edge:integer):boolean;
var av1,av2,v1,v2:TJKVertex;
    nv,sf,lastv,v:integer;
    sec:TJKSector;
    asf:TJKSurface;
    surf1:TJKSurface;
    edge1:integer;
    ipos:integer;
    nsf1:integer;
begin
 result:=false;
 v1:=Surf.Vertices[edge];
 With Surf do v2:=Vertices[NextVx(edge)];

 surf1:=nil;
 sec:=surf.Sector;
 for sf:=0 to Sec.Surfaces.Count-1 do
 begin
  asf:=Sec.Surfaces[sf];
  if asf=surf then continue;
  for v:=0 to asf.vertices.count-1 do
  begin
   av1:=asf.Vertices[v];
   av2:=asf.Vertices[asf.NextVx(v)];
   if (v1<>av2) or (v2<>av1) then continue;
   if surf1<>nil then begin PanMessage(mt_error,'The edge is common to more than one surface'); exit; end;
   surf1:=asf; edge1:=v;
   nsf1:=sf;
  end;
 end;
 if Surf1=nil then begin PanMessage(mt_error,'The edge belongs to just one surface'); exit; end;

 Result:=SysMergeSurfs(surf,surf1,edge,edge1)<>nil;
end;


function MergeSectors(sec,sec1:TJKSector):TJKSector;
var surf,surf1:TJKSurface;
    i,j:integer;
    sfs,sfs1:TSurfaces;
begin
 Result:=nil;
 sfs:=TSurfaces.Create;
 sfs1:=TSurfaces.Create;
 for i:=0 to sec.surfaces.count-1 do
 begin
  Surf:=sec.surfaces[i];
  if (Surf.adjoin<>nil) and (surf.adjoin.sector=sec1)
      and Do_Surf_OverLap(Surf,Surf.adjoin) then
  begin
   sfs.add(surf);
   sfs1.add(surf.adjoin);
  end;
 end;

 if sfs.count=0 then{ PanMessageFmt(mt_warning,'The sectors %d and %d aren''t adjoined or adjoins don''t overlap',[sec.num,sec1.num])}
 else
 begin

  SaveSecUndo(sec,ch_changed,sc_both);
  SaveSecUndo(sec1,ch_deleted,sc_both);


  for i:=sec1.surfaces.count-1 downto 0 do
  begin
   surf:=sec1.surfaces[i];
   {If surface is to be deleted - skip it}
   if sfs1.indexof(surf)<>-1 then continue;

  {Add all vertices used in the surface to Sec}
   for j:=0 to surf.vertices.count-1 do
   with surf.Vertices[j] do surf.vertices[j]:=sec.AddVertex(x,y,z);

   {Add surface to Sec and delete from Sec1}
   surf.sector:=sec;
   sec.surfaces.Add(surf);
   sec1.Surfaces.Delete(i);
  end;

  RemoveSecRefs(sec1.level,sec1,0);

  for i:=0 to sfs.count-1 do
   RemoveSurfRefs(sec1.level,sfs[i]);

  for i:=0 to sfs1.count-1 do
   RemoveSurfRefs(sec1.level,sfs1[i]);

  for i:=0 to sfs.count-1 do
  begin
   j:=sec.surfaces.indexof(sfs[i]);
   sec.Surfaces.Delete(j);
   sfs[i].free;
  end;

  sec.renumber;



  JedMain.SectorChanged(sec);

  i:=Sec1.Level.Sectors.IndexOf(sec1);
  Sec1.level.Sectors.Delete(i);
  JedMain.SectorDeleted(sec1);
  sec1.free;

  sec.level.RenumSecs;

  Result:=sec;

  if not IsSectorConvex(Sec) then PanMessageFmt(mt_info,
  'The sector %d is not convex',[sec.num]);

 end;

 sfs.free;
 sfs1.free;

end;

function MergeVertices(lev:TJKLevel;vxsel:TVXMultiSel):integer;// returns number of vertices merged
label next;
var i,j,nv2:integer;
    v1,v2:TJKVertex;
    sc,vx:integer;
    sec:TJKSector;
    sfchanged:boolean;
begin
 StartUndoRec('Merge vertices');
 vxsel.sort;
 result:=0;

 i:=0;
 while i<vxsel.count-1 do
 begin
  j:=i+1;
  while j<vxsel.count do
  begin
   vxsel.GetSCVX(i,sc,vx);
   v1:=lev.Sectors[sc].Vertices[vx];
   vxsel.GetSCVX(j,sc,vx);
   v2:=lev.Sectors[sc].Vertices[vx];
   nv2:=vx;
   if v1=v2 then goto next;
   if v1.sector<>v2.sector then goto next;
   if not (IsClose(v1.x,v2.x) and IsClose(v1.y,v2.y) and IsClose(v1.z,v2.z)) then goto next;
   sec:=v1.sector;

   SaveSecUndo(sec,ch_changed,sc_geo);

   for sc:=0 to sec.surfaces.count-1 do
   with sec.surfaces[sc] do
   for vx:=vertices.count-1 downto 0 do
   begin
    sfchanged:=false;
    if vertices[vx]=v2 then
    begin
     vertices[vx]:=v1;
     if vertices[NextVX(vx)]=v1 then vertices.Delete(vx);
     if vertices[PrevVX(vx)]=v1 then vertices.Delete(vx);
     sfchanged:=true;
    end;
    if sfchanged then Recalc;
   end;

   sec.vertices.Delete(nv2);
   v2.free;
   inc(result);
   JedMain.SectorChanged(sec);

   next:
    inc(j);
  end;
  inc(i);
 end;


end;

Function DO_Surf_Overlap(surf,surf1:TJKSurface):boolean;
var
    vxs0,vxs1:TVertices;

Function Check:boolean;
var i,v,fv:integer;
    v0,v1,v2:TVertex;
    e1,e2:TVector;
begin
 Result:=false;

 {Mark "important" vertices in surf}

 With Surf do
 for i:=0 to vertices.count-1 do
 begin
  v0:=Vertices[PrevVX(i)];
  v1:=Vertices[i];
  v2:=Vertices[NextVX(i)];
  SetVec(e1,v1.x-v0.x,v1.y-v0.y,v1.z-v0.z);
  SetVec(e2,v2.x-v1.x,v2.y-v1.y,v2.z-v1.z);
  if not Normalize(e1) then continue;
  Normalize(e2);
  if IsClose(e1.dx,e2.dx) and IsClose(e1.dy,e2.dy)
     and IsClose(e1.dz,e2.dz) then continue;
  vxs0.Add(v1);
 end;

 {Mark "important" vertices in surf1}

 With Surf1 do
 for i:=0 to vertices.count-1 do
 begin
  v0:=Vertices[PrevVX(i)];
  v1:=Vertices[i];
  v2:=Vertices[NextVX(i)];
  SetVec(e1,v1.x-v0.x,v1.y-v0.y,v1.z-v0.z);
  SetVec(e2,v2.x-v1.x,v2.y-v1.y,v2.z-v1.z);
  if not Normalize(e1) then continue;
  Normalize(e2);
  if IsClose(e1.dx,e2.dx) and IsClose(e1.dy,e2.dy)
     and IsClose(e1.dz,e2.dz) then continue;
  vxs1.Add(v1);
 end;

 if vxs0.count<>vxs1.count then exit;
 v0:=vxs0[0];

 fv:=-1;
 for v:=0 to vxs1.count-1 do
 begin
  v1:=vxs1[v];
  if IsClose(v0.x,v1.x) and IsClose(v0.y,v1.y) and IsClose(v0.z,v1.z) then
  begin
   fv:=v;
   break;
  end;
 end;
 if fv=-1 then exit;


 fv:=PrevIdx(fv,vxs1.count);
 For v:=1 to vxs0.count-1 do
 begin
  v0:=vxs0[v];
  v1:=vxs1[fv];
  if not (IsClose(v0.x,v1.x) and IsClose(v0.y,v1.y) and IsClose(v0.z,v1.z)) then exit;
  fv:=PrevIdx(fv,vxs1.count);
 end;
 Result:=true;
end;

begin {DO_Surf_Overlap}
 if (surf=nil) or (surf1=nil) then begin result:=false; exit; end;
 vxs0:=TVertices.create;
 vxs1:=TVertices.create;
 Result:=Check;
 vxs0.free;
 vxs1.free;
end;


Function DO_Surf_Match(surf,surf1:TJKSurface):boolean;
var i,v,fv:integer;
    v0,v1:TJKVertex;
begin
 Result:=false;
 if surf1.vertices.count<>surf.vertices.count then exit;
 v0:=surf.vertices[0];
 fv:=-1;
 for v:=0 to surf1.vertices.count-1 do
 begin
  v1:=surf1.vertices[v];
  if IsClose(v0.x,v1.x) and IsClose(v0.y,v1.y) and IsClose(v0.z,v1.z) then
  begin
   fv:=v;
   break;
  end;
 end;
 if fv=-1 then exit;

 fv:=Surf1.PrevVX(fv);
 For v:=1 to surf.vertices.count-1 do
 begin
  v0:=surf.vertices[v];
  v1:=surf1.vertices[fv];
  if not (IsClose(v0.x,v1.x) and IsClose(v0.y,v1.y) and IsClose(v0.z,v1.z)) then exit;
  fv:=surf1.PrevVX(fv);
 end;
 Result:=true;
end;


Function MakeAdjoin(surf:TJKSurface):boolean;
var s,sf:integer;
    lev:TJKLevel;
    surf1:TJKSurface;
begin
 Result:=false;
 if surf.Adjoin<>nil then exit;
 lev:=surf.sector.level;

 for s:=0 to lev.Sectors.Count-1 do
 With Lev.Sectors[s] do
 for sf:=0 to Surfaces.Count-1 do
 begin
  surf1:=Surfaces[sf];
  if DO_Surf_Overlap(surf,Surf1) then
  begin
   SaveSecUndo(surf.sector,ch_changed,sc_val);
   SaveSecUndo(surf1.sector,ch_changed,sc_val);

   SysAdjoinSurfaces(Surf,surf1);
   JedMain.SectorChanged(surf.Sector);
   JedMain.SectorChanged(surf1.Sector);
   result:=true;
   exit;
  end;
 end;
end;

Function MakeAdjoinSCUP(surf:TJKSurface;sc:integer):boolean;
var s,sf:integer;
    lev:TJKLevel;
    surf1:TJKSurface;
begin
 Result:=false;
 if surf.Adjoin<>nil then exit;
 lev:=surf.sector.level;

 for s:=sc to lev.Sectors.Count-1 do
 With Lev.Sectors[s] do
 for sf:=0 to Surfaces.Count-1 do
 begin
  surf1:=Surfaces[sf];
  if DO_Surf_Overlap(surf,Surf1) then
  begin
   SysAdjoinSurfaces(Surf,surf1);
   result:=true;
   exit;
  end;
 end;
end;


Function UnAdjoin(surf:TJKSurface):Boolean;
var sec1:TJKSector;
begin
 Result:=false;
 if surf.Adjoin=nil then exit;
 sec1:=surf.adjoin.Sector;
 if surf.Adjoin.Adjoin<>surf then
 begin
  SaveSecUndo(surf.sector,ch_changed,sc_val);
  PanMessageFmt(mt_warning,'Incorrect reverse adjoin SC %d SF %d',[surf.sector.num,surf.num]);
  ClearAdjoinParams(Surf);
  surf.Adjoin:=nil;
 end
 else
 begin
  SaveSecUndo(surf.sector,ch_changed,sc_val);
  SaveSecUndo(surf.adjoin.sector,ch_changed,sc_val);

  ClearAdjoinParams(Surf);
  ClearAdjoinParams(Surf.Adjoin);

  surf.CheckIfFloor;
  surf.adjoin.CheckIfFloor;

  surf.Adjoin.Adjoin:=nil;
  surf.Adjoin:=nil;
  JedMain.SectorChanged(sec1);
 end;
 JedMain.SectorChanged(surf.sector);
 Result:=true;
end;

Function UnAdjoinSectors(sec,sec1:TJKSector):boolean;
var i:integer;
    surf:TJKSurface;
begin
 result:=false;
 for i:=0 to sec.surfaces.count-1 do
 begin
  surf:=sec.surfaces[i];
  if surf.adjoin=nil then continue;
  if surf.adjoin.sector=sec1 then
     result:=Result or UnAdjoin(surf);
 end;
end;

Function AdjoinSectors(sec,sec1:TJKSector):boolean;
var i:integer;
    surf:TJKSurface;

Function AdjointoSec1(surf:TJKSurface):boolean;
var i:integer;
    surf1:TJKSurface;
begin
 result:=false;
 if surf.adjoin<>nil then exit;
 for i:=0 to sec1.surfaces.count-1 do
 begin
  surf1:=sec1.surfaces[i];
  if surf1.adjoin<>nil then continue;
  if DO_Surf_Overlap(surf,surf1) then
  begin
   result:=true;
   SysAdjoinSurfaces(surf,surf1);
  end;
 end;
end;

begin
 result:=false;
 for i:=0 to sec.surfaces.count-1 do
 begin
  surf:=sec.surfaces[i];
  result:=result or AdjointoSec1(surf);
 end;

 if result then
 begin
  JedMain.SectorChanged(sec);
  JedMain.SectorChanged(sec1);
 end;
end;


Procedure CreateCubeSec(s:tjksector; x,y,z:double;const pnormal,edge:TVector);
var
   nsec:TJKSector;
    asurf,osurf,nsurf:TJKSurface;
    nvx:TJKVertex;
    i,v,vc:integer;
    enormal:TVector;
    a:double;
begin
 nsec:=s;
 asurf:=nsec.newsurface;
 nsec.surfaces.add(asurf);

 VMult(pnormal.dx,pnormal.dy,pnormal.dz,
       edge.dx,edge.dy,edge.dz,
       enormal.dx,enormal.dy,enormal.dz);

 Normalize(enormal);

 nvx:=nsec.newvertex;
 nvx.x:=x; nvx.y:=y; nvx.z:=z; asurf.AddVertex(nvx);
 nvx:=nsec.newvertex;
 nvx.x:=x+edge.dx; nvx.y:=y+edge.dy; nvx.z:=z+edge.dz; asurf.AddVertex(nvx);

 a:=Vlen(edge);

 nvx:=nsec.newvertex;
 nvx.x:=x+edge.dx+a*enormal.dx;
 nvx.y:=y+edge.dy+a*enormal.dy;
 nvx.z:=z+edge.dz+a*enormal.dz;
 asurf.AddVertex(nvx);

 nvx:=nsec.newvertex;
 nvx.x:=x+a*enormal.dx;
 nvx.y:=y+a*enormal.dy;
 nvx.z:=z+a*enormal.dz;
 asurf.AddVertex(nvx);

 asurf.NewRecalcAll;


 {Construct opposite surface}
 osurf:=nsec.newsurface;
 osurf.Assign(asurf);
 for v:=asurf.vertices.count-1 downto 0 do
 begin
  nvx:=nsec.NewVertex;
  With asurf.vertices[v] do
  begin
   nvx.x:=x+(asurf.normal.dx)*a;
   nvx.y:=y+(asurf.normal.dy)*a;
   nvx.z:=z+(asurf.normal.dz)*a;
  end;
  osurf.AddVertex(nvx);
 end;
 nsec.Surfaces.Add(osurf);

 {Construct other surfaces}
 vc:=asurf.vertices.count;
 nsurf:=nsec.newsurface; nsurf.Assign(asurf);

 for i:=0 to vc-1 do
 begin
  nsurf:=nsec.newsurface;
  nsurf.Assign(asurf);

  nsurf.AddVertex(asurf.vertices[i]);

  if i=0 then nsurf.AddVertex(asurf.vertices[vc-1])
  else nsurf.AddVertex(asurf.vertices[i-1]);

  if i=0 then nsurf.AddVertex(osurf.vertices[0])
  else nsurf.AddVertex(osurf.vertices[vc-i]);

  nsurf.AddVertex(osurf.vertices[vc-i-1]);
  nsec.surfaces.Add(nsurf);
 end;
  for i:=1 to nsec.surfaces.count-1 do nsec.surfaces[i].NewRecalcAll;
  nsec.renumber;
end;

Procedure CreateCube(lev:TJKLevel; x,y,z:double;const pnormal,edge:TVector);
 var
    nsec:TJKSector;
begin
 nsec:=lev.NewSector;
 CreateCubeSec(nsec,x,y,z,pnormal,edge);
 nSec.Num:=Lev.Sectors.count;
 Lev.Sectors.Add(nsec);
 JedMain.SectorAdded(nSec);
end;

Function GetNearestVX(lev:TJKLevel;px,py,pz:double;var rx,ry,rz:double;mdist:double):boolean;
var s,v:integer;
    cmdist,cdist:double;
begin
 rx:=px; ry:=py; rz:=pz;
 Result:=false;
 cmdist:=sqr(mdist);
 for s:=0 to Lev.sectors.count-1 do
 With lev.Sectors[s] do
 For v:=0 to vertices.count-1 do
 With Vertices[v] do
 begin
  cdist:=sqr(x-px)+sqr(y-py)+sqr(z-pz);
  if cdist<=cmdist then
  begin
   cmdist:=cdist;
   rx:=x; ry:=y; rz:=z;
   result:=true;
  end;
 end;
end;

Procedure RotatePoint(ax1,ay1,az1,ax2,ay2,az2:double;angle:double;var x,y,z:double);
var nz,ny,nx:Tvector;
    px,py,pz:double;
    l,d:double;
    u,v:double;
begin
 nz.dx:=ax2-ax1; nz.dy:=ay2-ay1; nz.dz:=az2-az1;
 Normalize(nz);
 d:=nz.dx*x+nz.dy*y+nz.dz*z;
 PlaneLineXn(nz,d,ax1,ay1,az1,ax2,ay2,az2,px,py,pz);
 nx.dx:=x-px; nx.dy:=y-py; nx.dz:=z-pz;
 l:=Vlen(nx);
 if IsClose(l,0) then exit;
 Normalize(nx);
 VMult(nz.dx,nz.dy,nz.dz,
       nx.dx,nx.dy,nx.dz,
       ny.dx,ny.dy,ny.dz);
 u:=cos(angle/180*pi)*l;
 v:=sin(angle/180*pi)*l;
 x:=px+nx.dx*u+ny.dx*v;
 y:=py+nx.dy*u+ny.dy*v;
 z:=pz+nx.dz*u+ny.dz*v;
end;

(*Procedure StitchSurface(surf:TJKSurface;nv1,nv2:integer;u1,v1,u2,v2:double);
var vx1,vx2:TJKVertex;
    l2d,l3d,scale:double;
    du,cosa,sina:double;
    deu,dev,nu,nv:TVector;
    i:integer;
begin
 vx1:=surf.Vertices[nv1]; vx2:=surf.Vertices[nv2];
 du:=u2-u1;

 {Find length of line vx1-vx2 in UV and XYZ coordinates}

 l2d:=sqrt(sqr(du)+sqr(v2-v1));
 deu.dx:=vx2.x-vx1.x; deu.dy:=vx2.y-vx1.y; deu.dz:=vx2.z-vx1.z;
 l3d:=VLen(deu);
 scale:=l2d/l3d;

 {Fina angle between u1v1-u2v2 and U axis}
 cosa:=du/l2d;
 sina:=sqrt(1-sqr(cosa));

 {Find V vector}
 Normalize(deu);
 With Surf.Normal do
  Vmult(dx,dy,dz,deu.dx,deu.dy,deu.dz,dev.dx,dev.dy,dev.dz);

{Rotate U vector by angle and find V vector}
 nu.dx:=cosa*deu.dx+sina*dev.dx;
 nu.dy:=cosa*deu.dy+sina*dev.dy;
 nu.dz:=cosa*deu.dz+sina*dev.dz;

 With Surf.Normal do
  Vmult(dx,dy,dz,nu.dx,nu.dy,nu.dz,nv.dx,nv.dy,nv.dz);

 {Calculate U V coordinates}

 for i:=0 to Surf.Vertices.count-1 do
 With Surf.TXVertices[i],Surf.Vertices[i] do
 begin
  u:=u1+SMult(nu.dx,nu.dy,nu.dz,x-vx1.x,y-vx1.y,z-vx1.z)*Scale;
  v:=v1+SMult(nv.dx,nv.dy,nv.dz,x-vx1.x,y-vx1.y,z-vx1.z)*Scale;
 end;
end;

Function StitchAt(surf:TJKSurface;nv1,nv2:integer):integer;
var un,vn:tvector;
begin
 CalcUVNormalsFrom(surf,nv1,nv2,un,vn);
 ArrangeTexture(surf,nv1,un,vn);
end;

Function StitchAtEdge(surf:TJKSurface;edge:integer):integer;
var i,j:integer;
    v1,v2:TJKVertex;
    tv1,tv2:TTXVertex;
    sec:TJKSector;
    adj,sf:TJKSurface;
    av1,av2:TJKVertex;
begin
 Result:=0;
 v1:=surf.Vertices[edge];
 v2:=surf.Vertices[Surf.NextVX(edge)];
 tv1:=surf.TXVertices[edge];
 tv2:=surf.TXVertices[Surf.NextVX(edge)];

 sec:=surf.Sector;
 for i:=0 to sec.Surfaces.count-1 do
 begin
  sf:=sec.Surfaces[i];
  if sf=surf then continue;
  for j:=0 to sf.vertices.count-1 do
  begin
   if (sf.vertices[j]<>v2) or (sf.vertices[sf.NextVX(j)]<>v1) then continue;
   stitchSurface(sf,j,sf.nextVX(j),tv2.u,tv2.v,tv1.u,tv1.v);
   inc(result);
  end;
   if sf.adjoin=nil then continue;

  adj:=sf.adjoin;
  for j:=0 to adj.vertices.count-1 do
  begin
   av1:=adj.vertices[j];
   av2:=adj.vertices[adj.NextVX(j)];
   if not (IsClose(av1.x,v1.x) and IsClose(av1.y,v1.y) and IsClose(av1.z,v1.z)) then continue;
   if not (IsClose(av2.x,v2.x) and IsClose(av2.y,v2.y) and IsClose(av2.z,v2.z)) then continue;
   stitchSurface(adj,j,adj.nextVX(j),tv1.u,tv1.v,tv2.u,tv2.v);
   inc(result);
  end;

 end;
end; *)

Procedure MultVM3(const mx:TMat3x3; var x,y,z:double);
var i,j,k:integer;
    s,r:array[0..2] of double;
    sum:double;
begin
 s[0]:=x; s[1]:=y; s[2]:=z;
 for i:=0 to 2 do
 begin
  sum:=0;
  for k:=0 to 2 do sum:=sum+mx[i,k]*s[k];
  r[i]:=sum;
 end;
 x:=r[0]; y:=r[1]; z:=r[2];
end;

Procedure MultVM3s(const mx:TMat3x3s; var x,y,z:single);
var i,j,k:integer;
    s,r:array[0..2] of single;
    sum:double;
begin
 s[0]:=x; s[1]:=y; s[2]:=z;
 for i:=0 to 2 do
 begin
  sum:=0;
  for k:=0 to 2 do sum:=sum+mx[i,k]*s[k];
  r[i]:=sum;
 end;
 x:=r[0]; y:=r[1]; z:=r[2];
end;


Procedure MultM3(var mx:TMat3x3; const bymx:TMat3x3);
var i,j,k:integer;
    sum:double;
    tmx:TMat3x3;
begin
 for i:=0 to 2 do
 for j:=0 to 2 do
 begin
  sum:=0;
  for k:=0 to 2 do sum:=sum+mx[i,k]*bymx[k,j];
  tmx[i,j]:=sum;
 end;
 mx:=tmx;
end;

Procedure MultM3s(var mx:TMat3x3s; const bymx:TMat3x3s);
var i,j,k:integer;
    sum:single;
    tmx:TMat3x3s;
begin
 for i:=0 to 2 do
 for j:=0 to 2 do
 begin
  sum:=0;
  for k:=0 to 2 do sum:=sum+mx[i,k]*bymx[k,j];
  tmx[i,j]:=sum;
 end;
 mx:=tmx;
end;


Procedure CreateMatrix(var mx:TMat3x3; angle:double; axis:integer);
var cosa,sina:double;
begin
 cosa:=cos(angle/180*Pi);
 sina:=sin(angle/180*Pi);
 case axis of
  rt_x: begin
         mx[0,0]:=1; mx[0,1]:=0;     mx[0,2]:=0;
         mx[1,0]:=0; mx[1,1]:=cosa;  mx[1,2]:=sina;
         mx[2,0]:=0; mx[2,1]:=-sina; mx[2,2]:=cosa;
        end;
  rt_y: begin
         mx[0,0]:=cosa; mx[0,1]:=0; mx[0,2]:=-sina;
         mx[1,0]:=0;    mx[1,1]:=1; mx[1,2]:=0;
         mx[2,0]:=sina; mx[2,1]:=0; mx[2,2]:=cosa;
        end;
  rt_z: begin
         mx[0,0]:=cosa;  mx[0,1]:=sina; mx[0,2]:=0;
         mx[1,0]:=-sina; mx[1,1]:=cosa; mx[1,2]:=0;
         mx[2,0]:=0;     mx[2,1]:=0;    mx[2,2]:=1;
        end;
 end;
end;

Procedure CreateMatrixs(var mx:TMat3x3s; angle:single; axis:integer);
var cosa,sina:single;
begin
 cosa:=cos(angle/180*Pi);
 sina:=sin(angle/180*Pi);
 case axis of
  rt_x: begin
         mx[0,0]:=1; mx[0,1]:=0;     mx[0,2]:=0;
         mx[1,0]:=0; mx[1,1]:=cosa;  mx[1,2]:=sina;
         mx[2,0]:=0; mx[2,1]:=-sina; mx[2,2]:=cosa;
        end;
  rt_y: begin
         mx[0,0]:=cosa; mx[0,1]:=0; mx[0,2]:=-sina;
         mx[1,0]:=0;    mx[1,1]:=1; mx[1,2]:=0;
         mx[2,0]:=sina; mx[2,1]:=0; mx[2,2]:=cosa;
        end;
  rt_z: begin
         mx[0,0]:=cosa;  mx[0,1]:=sina; mx[0,2]:=0;
         mx[1,0]:=-sina; mx[1,1]:=cosa; mx[1,2]:=0;
         mx[2,0]:=0;     mx[2,1]:=0;    mx[2,2]:=1;
        end;
 end;
end;


type TRotateData=record
      cx,cy,cz:double; {center}
      m3:TMat3x3;
     end;

Procedure CalcSecCenter(sec:TJKSector; var cx,cy,cz:double);
var sx,sy,sz:double;
    i:integer;
begin
 sx:=0; sy:=0; sz:=0;
 for i:=0 to sec.vertices.count-1 do
 With sec.vertices[i] do
 begin
  sx:=sx+x;
  sy:=sy+y;
  sz:=sz+z;
 end;
 cx:=sx/sec.vertices.count;
 cy:=sy/sec.vertices.count;
 cz:=sz/sec.vertices.count;
end;

Procedure SysCalcSurfData(surf:TJKSurface;var xv,yv:Tvector;
                        var minx,miny,maxx,maxy:double);
var
    v0,v1:TJKVertex;
    i:integer;
    x,y:double;
begin
 v0:=surf.vertices[0];
 v1:=surf.vertices[1];
 SetVec(xv,v1.x-v0.x,v1.y-v0.y,v1.z-v0.z);
 if not Normalize(xv) then;
 With Surf.Normal do
 Vmult(dx,dy,dz,xv.dx,xv.dy,xv.dz,
       yv.dx,yv.dy,yv.dz);

 minx:=1E20;miny:=1E20;
 maxx:=-1E20;maxy:=-1E20;
 For i:=1 to surf.vertices.count-1 do
 begin
  v1:=surf.vertices[i];
  x:=Smult(v1.x-v0.x,v1.y-v0.y,v1.z-v0.z,xv.dx,xv.dy,xv.dz);
  y:=Smult(v1.x-v0.x,v1.y-v0.y,v1.z-v0.z,yv.dx,yv.dy,yv.dz);
  if x<minx then minx:=x;
  if y<miny then miny:=y;
  if x>maxx then maxx:=x;
  if y>maxy then maxy:=y;
 end;

end;

Procedure CalcSurfRect(surf:TJKSurface;v1,v2,v3,v4:TVertex;by:double);
var xv,yv:Tvector;
    minx,miny,maxx,maxy:double;
    v0:TVertex;
    x,y:double;

Procedure CalcVX(x,y:double;v:TVertex);
begin
 v.x:=v0.x+x*xv.dx+y*yv.dx;
 v.y:=v0.y+x*xv.dy+y*yv.dy;
 v.z:=v0.z+x*xv.dz+y*yv.dz;
end;

begin
 SysCalcSurfData(surf,xv,yv,minx,miny,maxx,maxy);
 v0:=surf.vertices[0];

 x:=(maxx-minx)/2;
 y:=(maxy-miny)/2;

 minx:=x-by/2;
 miny:=y-by/2;

 maxx:=x+by/2;
 maxy:=y+by/2;

 CalcVX(minx,miny,v1);
 CalcVX(minx,maxy,v2);
 CalcVX(maxx,maxy,v3);
 CalcVX(maxx,miny,v4);

end;

Procedure CalcSurfCenter(surf:TJKSurface;var cx,cy,cz:double);
var xv,yv:Tvector;
    minx,miny,maxx,maxy:double;
    x,y:double;
    v0:TVertex;
begin
 SysCalcSurfData(surf,xv,yv,minx,miny,maxx,maxy);
 x:=(maxx-minx)/2;
 y:=(maxy-miny)/2;
 v0:=surf.vertices[0];
 cx:=v0.x+x*xv.dx+y*yv.dx;
 cy:=v0.y+x*xv.dy+y*yv.dy;
 cz:=v0.z+x*xv.dz+y*yv.dz;
end;

Procedure RotateVertex(v:TJKVertex; const rd:TRotateData);
var x,y,z:double;
begin
 if v.mark<>0 then exit;
 x:=v.x-rd.cx;
 y:=v.y-rd.cy;
 z:=v.z-rd.cz;
 MultVM3(rd.m3,x,y,z);
 v.x:=x+rd.cx;
 v.y:=y+rd.cy;
 v.z:=z+rd.cz;
 v.mark:=1;
end;

Procedure PrepareRotData(var rd:TRotateData; const axis:Tvector;cx,cy,cz,angle:double);
Var
    vx,vy,vz:Tvector;
begin
 rd.cx:=cx;
 rd.cy:=cy;
 rd.cz:=cz;

 SetVec(vx,1,0,0);
 SetVec(vy,0,1,0);
 SetVec(vz,0,0,1);

 RotatePoint(0,0,0,axis.dx,axis.dy,axis.dz,angle,vx.dx,vx.dy,vx.dz);
 RotatePoint(0,0,0,axis.dx,axis.dy,axis.dz,angle,vy.dx,vy.dy,vy.dz);
 RotatePoint(0,0,0,axis.dx,axis.dy,axis.dz,angle,vz.dx,vz.dy,vz.dz);

{ CreateMatrix(rd.m3,60,rt_x);}
 rd.m3[0,0]:=vx.dx; rd.m3[0,1]:=vx.dy; rd.m3[0,2]:=vx.dz;
 rd.m3[1,0]:=vy.dx; rd.m3[1,1]:=vy.dy; rd.m3[1,2]:=vy.dz;
 rd.m3[2,0]:=vz.dx; rd.m3[2,1]:=vz.dy; rd.m3[2,2]:=vz.dz;
end;

Procedure RotateSectors(lev:TJKLevel; scsel:TSCMultiSel; axis:Tvector; cx,cy,cz:double; angle:double);
{Procedure RotateSector(sec:TJKSector; angle:double; axis:integer);}
var rd:TRotateData;
    sec:TJKSector;

Procedure RotSec(sec:TJKSector);
var    i,j:integer;
begin
 SaveSecUndo(sec,ch_changed,sc_geo);
 for i:=0 to sec.vertices.count-1 do
  RotateVertex(sec.Vertices[i],rd);

 for i:=0 to sec.surfaces.count-1 do
 with sec.surfaces[i] do
 begin
  if adjoin=nil then continue;
  SaveSecUndo(adjoin.sector,ch_changed,sc_geo);
  for j:=0 to adjoin.vertices.count-1 do
   RotateVertex(adjoin.vertices[j],rd);
 end;
 RecalcSector(sec);
 JedMain.SectorChanged(sec);
 for i:=0 to sec.surfaces.count-1 do
 with sec.surfaces[i] do
 begin
  if adjoin=nil then continue;
  JedMain.SectorChanged(adjoin.Sector);
 end;
end;

var i:integer;
begin

 PrepareRotData(rd,axis,cx,cy,cz,angle);

 ClearVXMarks(Lev);
 for i:=0 to scsel.count-1 do
  RotSec(Lev.Sectors[scsel.getSC(i)]);
end;

Procedure RotateXYZ(var ax,ay,az:double; const rd:TRotateData);
var x,y,z:double;
begin
 x:=ax-rd.cx;
 y:=ay-rd.cy;
 z:=az-rd.cz;
 MultVM3(rd.m3,x,y,z);
 ax:=x+rd.cx;
 ay:=y+rd.cy;
 az:=z+rd.cz;
end;


Procedure RotateThings(lev:TJKLevel; thsel:TTHMultiSel; axis:Tvector; cx,cy,cz:double; angle:double);
var rd:TRotateData;
    i:integer;
    th:TJKThing;
begin
 PrepareRotData(rd,axis,cx,cy,cz,angle);
 for i:=0 to thsel.count-1 do
 begin
  th:=Lev.Things[thsel.GetTH(i)];
  SaveThingUndo(th,ch_changed);
  RotateXYZ(th.x,th.y,th.z,rd);
  JedMain.ThingChanged(th);
 end;
end;

Procedure RotateLights(lev:TJKLevel; ltsel:TLTMultiSel; axis:Tvector; cx,cy,cz:double; angle:double);
var rd:TRotateData;
    i:integer;
    lt:TJedLight;
begin
 PrepareRotData(rd,axis,cx,cy,cz,angle);
 for i:=0 to ltsel.count-1 do
 begin
  lt:=Lev.Lights[ltsel.GetLT(i)];
  SaveLightUndo(lt,ch_changed);
  RotateXYZ(lt.x,lt.y,lt.z,rd);
  JedMain.LevelChanged;
 end;
end;

Procedure RotateFrames(lev:TJKLevel; frsel:TFRMultiSel; axis:Tvector; cx,cy,cz:double; angle:double);
var rd:TRotateData;
    i:integer;
    th,fr:integer;
    vl:TTplValue;
    x,y,z,pch,yaw,rol:double;
begin
 PrepareRotData(rd,axis,cx,cy,cz,angle);
 for i:=0 to frsel.count-1 do
 begin
  frsel.GetTHFR(i,th,fr);
  vl:=Lev.Things[th].Vals[fr];
  SaveThingUndo(Lev.Things[th],ch_changed);
  vl.GetFrame(x,y,z,pch,yaw,rol);
  RotateXYZ(x,y,z,rd);
  vl.SetFrame(x,y,z,pch,yaw,rol);
  JedMain.ThingChanged(Lev.Things[th]);
 end;
end;

Procedure ScaleVertex(v:TJKVertex; const sd:TScaleData);
var d:double;
begin
 if v.mark<>0 then exit;
 case sd.how of
  scale_xyz:
   begin
    v.x:=sd.cx+sd.sfactor*(v.x-sd.cx);
    v.y:=sd.cy+sd.sfactor*(v.y-sd.cy);
    v.z:=sd.cz+sd.sfactor*(v.z-sd.cz);
   end;
  scale_x: v.x:=sd.cx+sd.sfactor*(v.x-sd.cx);
  scale_y: v.y:=sd.cy+sd.sfactor*(v.y-sd.cy);
  scale_z: v.z:=sd.cz+sd.sfactor*(v.z-sd.cz);
  scale_vec:
   begin
    d:=Smult(v.x-sd.cx,v.y-sd.cy,v.z-sd.cz,
             sd.vec.dx,sd.vec.dy,sd.vec.dz);
    d:=(sd.sfactor-1)*d;
    v.x:=v.x+d*sd.vec.dx;
    v.y:=v.y+d*sd.vec.dy;
    v.z:=v.z+d*sd.vec.dz;
   end;
 end;
 v.mark:=1;
end;

Procedure ScaleSectors(lev:TJKLevel; scsel:TSCMultiSel;const sd:TScaleData;scaletx:boolean);
{Procedure ScaleSector(sec:TJKSector; sfactor:double);}
var 
    i:integer;

Procedure ScaleSec(sec:TJKSector);
var
    i,j:integer;
begin
 SaveSecUndo(sec,ch_changed,sc_geo);

 for i:=0 to sec.vertices.count-1 do
         ScaleVertex(sec.vertices[i],sd);

 for i:=0 to sec.surfaces.count-1 do
 With sec.surfaces[i] do
 begin
  if scaletx then begin uscale:=uscale/sd.sfactor; vscale:=vscale/sd.sfactor; end;
  if adjoin=nil then continue;
  SaveSecUndo(adjoin.sector,ch_changed,sc_geo);

  if scaletx then
  begin
   adjoin.uscale:=adjoin.uscale/sd.sfactor;
   adjoin.vscale:=adjoin.vscale/sd.sfactor;
  end;
  for j:=0 to adjoin.vertices.count-1 do
   ScaleVertex(adjoin.vertices[j],sd);
 end;
 RecalcSector(sec);
 JedMain.SectorChanged(sec);
 for i:=0 to sec.surfaces.count-1 do
 with sec.surfaces[i] do
 begin
  if adjoin=nil then continue;
  JedMain.SectorChanged(adjoin.Sector);
 end;
end;

begin
 ClearVXMarks(Level);

 for i:=0 to scsel.count-1 do
  ScaleSec(Lev.Sectors[scsel.getSC(i)]);
end;

Procedure ScaleXYZ(var x,y,z:double; const sd:TScaleData);
var d:double;
begin
 case sd.how of
  scale_xyz:
   begin
    x:=sd.cx+sd.sfactor*(x-sd.cx);
    y:=sd.cy+sd.sfactor*(y-sd.cy);
    z:=sd.cz+sd.sfactor*(z-sd.cz);
   end;
  scale_x: x:=sd.cx+sd.sfactor*(x-sd.cx);
  scale_y: y:=sd.cy+sd.sfactor*(y-sd.cy);
  scale_z: z:=sd.cz+sd.sfactor*(z-sd.cz);
  scale_vec:
   begin
    d:=Smult(x-sd.cx,y-sd.cy,z-sd.cz,
             sd.vec.dx,sd.vec.dy,sd.vec.dz);
    d:=sd.sfactor-d;
    x:=x+d*sd.vec.dx;
    y:=y+d*sd.vec.dy;
    z:=z+d*sd.vec.dz;
   end;
 end;
end;

Procedure ScaleThings(lev:TJKLevel; thsel:TTHMultiSel;const sd:TScaleData);
var rd:TRotateData;
    i:integer;
    th:TJKThing;
begin
 for i:=0 to thsel.count-1 do
 begin
  th:=Lev.Things[thsel.GetTH(i)];
  SaveThingUndo(th,ch_changed);
  ScaleXYZ(th.x,th.y,th.z,sd);
  JedMain.ThingChanged(th);
 end;
end;

Procedure ScaleLights(lev:TJKLevel; ltsel:TLTMultiSel;const sd:TScaleData);
var rd:TRotateData;
    i:integer;
    lt:TJedLight;
begin
 for i:=0 to ltsel.count-1 do
 begin
  lt:=Lev.Lights[ltsel.GetLT(i)];
  SaveLightUndo(lt,ch_changed);
  ScaleXYZ(lt.x,lt.y,lt.z,sd);
  JedMain.LevelChanged;
 end;
end;

Procedure ScaleFrames(lev:TJKLevel; frsel:TFRMultiSel;const sd:TScaleData);
var
    i:integer;
    th,fr:integer;
    vl:TTplValue;
    x,y,z,pch,yaw,rol:double;
begin
 for i:=0 to frsel.count-1 do
 begin
  frsel.GetTHFR(i,th,fr);
  vl:=Lev.Things[th].Vals[fr];
  SaveThingUndo(Lev.Things[th],ch_changed);
  vl.GetFrame(x,y,z,pch,yaw,rol);
  ScaleXYZ(x,y,z,sd);
  vl.SetFrame(x,y,z,pch,yaw,rol);
  JedMain.ThingChanged(Lev.Things[th]);
 end;
end;


Procedure FlipVertex(v:TJKVertex; cx,cy,cz:double; how:integer);
begin
 if v.mark<>0 then exit;
 case how of
  rt_x: v.x:=cx-(v.x-cx);
  rt_y: v.y:=cy-(v.y-cy);
  rt_z: v.z:=cz-(v.z-cz);
 end;
 v.mark:=1;
end;

Procedure FlipVertexOverPlane(v:TJKVertex; const pnorm:TVector; cx,cy,cz:double);
var d:double;
begin
 if v.mark<>0 then exit;
 d:=Smult(pnorm.dx,pnorm.dy,pnorm.dz,
          v.x-cx,v.y-cy,v.z-cz);
 v.x:=v.x-pnorm.dx*2*d;
 v.y:=v.y-pnorm.dy*2*d;
 v.z:=v.z-pnorm.dz*2*d;
 v.mark:=1;
end;

Procedure FlipXYZ(var x,y,z:double; const pnorm:TVector; cx,cy,cz:double);
var d:double;
begin
 d:=Smult(pnorm.dx,pnorm.dy,pnorm.dz,
          x-cx,y-cy,z-cz);
 x:=x-pnorm.dx*2*d;
 y:=y-pnorm.dy*2*d;
 z:=z-pnorm.dz*2*d;
end;


Procedure FlipSurface(surf:TJKSurface);
var n,i:integer;
    vx:TJKVertex;
    tv:TTXVertex;
begin
 for i:=0 to (surf.vertices.count div 2)-1 do
 begin
   n:=surf.vertices.count-i-1;
   vx:=surf.vertices[i];
   surf.vertices[i]:=surf.vertices[n];
   surf.vertices[n]:=vx;

   tv:=surf.txvertices[i];
   surf.txvertices[i]:=surf.txvertices[n];
   surf.txvertices[n]:=tv;
 end;
 surf.recalc;

 if IsTXFlipped(surf) then surf.surfFlags:=SetFlags(surf.surfFlags,SFF_FLIP)
 else surf.surfFlags:=ClearFlags(surf.surfFlags,SFF_FLIP);

end;

Procedure FlipThingsOverPlane(lev:TJKLevel;thsel:TTHMultiSel; pnorm:TVector; cx,cy,cz:double);
var th:TJKThing;
    i:integer;
    d:double;
begin
 for i:=0 to thsel.count-1 do
 begin
  th:=lev.Things[thsel.getTH(i)];
  SaveThingUndo(th,ch_changed);
  FlipXYZ(th.x,th.y,th.z,pnorm,cx,cy,cz);
  JedMain.ThingChanged(th);
 end;
end;

Procedure FlipLightsOverPlane(lev:TJKLevel;ltsel:TLTMultiSel; pnorm:TVector; cx,cy,cz:double);
var rd:TRotateData;
    i:integer;
    lt:TJedLight;
begin
 for i:=0 to ltsel.count-1 do
 begin
  lt:=Lev.Lights[ltsel.GetLT(i)];
  SaveLightUndo(lt,ch_changed);
  FlipXYZ(lt.x,lt.y,lt.z,pnorm,cx,cy,cz);
  JedMain.LevelChanged;
 end;
end;

Procedure FlipFramesOverPlane(lev:TJKLevel;frsel:TFRMultiSel; pnorm:TVector; cx,cy,cz:double);
var
    i:integer;
    th,fr:integer;
    vl:TTplValue;
    x,y,z,pch,yaw,rol:double;
begin
 for i:=0 to frsel.count-1 do
 begin
  frsel.GetTHFR(i,th,fr);
  vl:=Lev.Things[th].Vals[fr];
  SaveThingUndo(Lev.Things[th],ch_changed);
  vl.GetFrame(x,y,z,pch,yaw,rol);
  FlipXYZ(x,y,z,pnorm,cx,cy,cz);
  vl.SetFrame(x,y,z,pch,yaw,rol);
  JedMain.ThingChanged(Lev.Things[th]);
 end;
end;


Procedure FlipThings(lev:TJKLevel;thsel:TTHMultiSel;cx,cy,cz:double;how:integer);
var th:TJKThing;
    i:integer;
begin
 for i:=0 to thsel.count-1 do
 begin
  th:=lev.Things[thsel.getTH(i)];
  SaveThingUndo(th,ch_changed);
 case how of
  rt_x: th.x:=cx-(th.x-cx);
  rt_y: th.y:=cy-(th.y-cy);
  rt_z: th.z:=cz-(th.z-cz);
 end;
 JedMain.ThingChanged(th);
end;
end;


Procedure FlipSectorsOverPlane(lev:TJKLevel;scsel:TSCMultiSel;const pnorm:TVector;px,py,pz:double);
var sec:TJKSector;
    i,j:integer;
    surf:TJKSurface;
begin
 ClearVXMarks(lev);
 for i:=0 to scsel.count-1 do
 begin
  sec:=Lev.Sectors[scsel.GetSC(i)];
  SaveSecUndo(sec,ch_changed,sc_geo);

  for j:=0 to sec.surfaces.count-1 do
  begin
   surf:=sec.surfaces[j];
   if surf.adjoin=nil then continue;
   if scsel.FindSC(surf.adjoin.sector.num)=-1 then Unadjoin(surf);
  end;
 end;

 for i:=0 to scsel.count-1 do
 begin
  sec:=Lev.Sectors[scsel.GetSC(i)];
  for j:=0 to sec.vertices.count-1 do
    FlipVertexOverPlane(sec.vertices[j],pnorm,px,py,pz);

  for j:=0 to sec.surfaces.count-1 do
    FlipSurface(sec.surfaces[j]);
  JedMain.SectorChanged(sec);
 end;


end;

Procedure FlipSectors(lev:TJKLevel;scsel:TSCMultiSel;cx,cy,cz:double;how:integer);
var sec:TJKSector;
    i,j:integer;
    surf:TJKSurface;
begin
 ClearVXMarks(lev);
 for i:=0 to scsel.count-1 do
 begin
  sec:=Lev.Sectors[scsel.GetSC(i)];
  SaveSecUndo(sec,ch_changed,sc_geo);

  for j:=0 to sec.surfaces.count-1 do
  begin
   surf:=sec.surfaces[j];
   if surf.adjoin=nil then continue;
   if scsel.FindSC(surf.adjoin.sector.num)=-1 then Unadjoin(surf);
  end;
 end;

 for i:=0 to scsel.count-1 do
 begin
  sec:=Lev.Sectors[scsel.GetSC(i)];
  for j:=0 to sec.vertices.count-1 do
    FlipVertex(sec.vertices[j],cx,cy,cz,how);

  for j:=0 to sec.surfaces.count-1 do
    FlipSurface(sec.surfaces[j]);
  JedMain.SectorChanged(sec);
 end;


end;

Procedure FlipSurfaces(lev:TJKLevel;sfsel:TSFMultiSel);
var i:integer;
    sc,sf:integer;
    surf:TJKsurface;
begin
 for i:=0 to sfsel.count-1 do
 begin
  sfsel.GetSCSF(i,sc,sf);
  surf:=level.Sectors[sc].Surfaces[sf];
  FlipSurface(surf);
  JedMain.SectorChanged(surf.sector);
 end;
end;

Function FindCommonEdges(surf,withsurf:TJKSurface; var v11,v12,v21,v22:integer):boolean;
var v1,v2:TJKVertex;
    i,j,n11,n12,n21,n22:integer;

Function FindVX(v:TJKVertex; asurf:TJKSurface):integer;
var i:integer;
    v2:TJKVertex;
begin
 Result:=-1;
 for i:=0 to asurf.vertices.count-1 do
 begin
  v2:=asurf.vertices[i];
   if IsClose(v.x,v2.x) and IsClose(v.y,v2.y) and IsClose(v.z,v2.z) then
   begin
    Result:=i;
    exit;
   end;
  end;
end;


begin
 Result:=false;
 n11:=-1; n12:=-1;
 for i:=0 to surf.vertices.count-1 do
 begin
  n21:=FindVX(surf.vertices[i],withsurf);
  if n21<>-1 then begin n11:=i; break; end;
 end;

 if n11=-1 then exit;
 n12:=surf.NextVX(n11);
 n22:=FindVX(surf.vertices[n12],withsurf);
 if n22=-1 then
 begin
  n12:=surf.PrevVX(n11);
  n22:=FindVX(surf.vertices[n12],withsurf);
 end;
 if n22=-1 then exit;
 result:=true;
 v11:=n11; v12:=n12;
 v21:=n21; v22:=n22;
end;

Function FindCommonVXs(surf,withsurf:TJKSurface; var v1,v2:integer):boolean;
Function FindVX(v:TJKVertex; asurf:TJKSurface):integer;
var i:integer;
    v2:TJKVertex;
begin
 Result:=-1;
 for i:=0 to asurf.vertices.count-1 do
 begin
  v2:=asurf.vertices[i];
   if IsClose(v.x,v2.x) and IsClose(v.y,v2.y) and IsClose(v.z,v2.z) then
   begin
    Result:=i;
    exit;
   end;
  end;
end;

var i,j:integer;
begin
 Result:=false;
 for i:=0 to surf.vertices.count-1 do
 begin
  j:=FindVX(surf.Vertices[i],withsurf);
  if j<>-1 then
  begin
   v1:=i; v2:=j;
   result:=true;
   exit;
  end;
 end;
end;

Function StitchSurfaces(surf,surf1:TJKSurface):boolean;
var lv,p1v,p2v:TVector;
    un,vn:TVector;
    nv1,nv2:integer;
    a,b:double;
begin
 If FindCommonVXs(surf,surf1,nv1,nv2) then
 CalcUVNormalsFromVX(surf,nv1,un,vn) else
 begin CalcUVNormals(surf,un,vn); nv1:=-1; nv2:=0; end;
 VMult(surf.normal.dx,surf.normal.dy,surf.normal.dz,
       surf1.normal.dx,surf1.normal.dy,surf1.normal.dz,
       lv.dx,lv.dy,lv.dz);

 if Normalize(lv) then
 begin
  VMult(surf.normal.dx,surf.normal.dy,surf.normal.dz,
        lv.dx,lv.dy,lv.dz,
        p1v.dx,p1v.dy,p1v.dz);
  VMult(surf1.normal.dx,surf1.normal.dy,surf1.normal.dz,
        lv.dx,lv.dy,lv.dz,
        p2v.dx,p2v.dy,p2v.dz);

  a:=Smult(p1v.dx,p1v.dy,p1v.dz,un.dx,un.dy,un.dz);
  b:=Smult(lv.dx,lv.dy,lv.dz,un.dx,un.dy,un.dz);
  SetVec(un,a*p2v.dx+b*lv.dx,a*p2v.dy+b*lv.dy,a*p2v.dz+b*lv.dz);

  a:=Smult(p1v.dx,p1v.dy,p1v.dz,vn.dx,vn.dy,vn.dz);
  b:=Smult(lv.dx,lv.dy,lv.dz,vn.dx,vn.dy,vn.dz);
  SetVec(vn,a*p2v.dx+b*lv.dx,a*p2v.dy+b*lv.dy,a*p2v.dz+b*lv.dz);
 end;

 SaveSecUndo(surf1.sector,ch_changed,sc_both);

 if nv1<>-1 then
 begin
  surf1.txvertices[nv2].u:=surf.txvertices[nv1].u;
  surf1.txvertices[nv2].v:=surf.txvertices[nv1].v;
 end;


 surf1.uscale:=surf.uscale;
 surf1.vscale:=surf.vscale;

 if (surf.SurfFlags and SFF_Flip<>0) then surf1.SurfFlags:=surf1.SurfFlags or SFF_Flip
 else surf1.SurfFlags:=surf1.SurfFlags and (not SFF_Flip);

 ArrangeTexture(surf1,nv2,un,vn);

end;
(*Function StitchSurfaces(surf,surf1:TJKSurface):boolean;
var v11,v12,v21,v22:integer;
    tv1,tv2:TTXVertex;
    u,v,u1,v1,a:Tvector;
    notflipped:boolean;
begin
 result:=false;
 if FindCommonEdges(surf1,surf,v11,v12,v21,v22) then
 begin
  tv2:=surf.TXVertices[v21]; tv1:=surf1.TXVertices[v11];
  tv1.v:=tv2.v; tv1.u:=tv2.u;
  tv2:=surf.TXVertices[v22]; tv1:=surf1.TXVertices[v12];
  tv1.v:=tv2.v; tv1.u:=tv2.u;
  SysCalcUVNormalsFrom(surf,v21,v22,u1,v1,false);
  SysCalcUVNormalsFrom(surf1,v11,v12,u,v,IsVFlipped(surf,v21,v22,u1,v1));

  {Check if original normals are flipped}
{  VMult(u1.dx,u1.dy,u1.dz,v1.dx,v1.dy,v1.dz,a.dx,a.dy,a.dz);
  With Surf do
  notflipped:=IsClose(a.dx,normal.dx) and IsClose(a.dy,normal.dy) and IsClose(a.dz,normal.dz);

  VMult(u.dx,u.dy,u.dz,v.dx,v.dy,v.dz,a.dx,a.dy,a.dz);
  With Surf1 do
  if notflipped<>(IsClose(a.dx,normal.dx) and IsClose(a.dy,normal.dy) and IsClose(a.dz,normal.dz))
  then SetVec(v,-v.dx,-v.dy,-v.dz);}

  surf1.txscale:=surf.txscale;
  ArrangeTexture(surf1,v11,u,v);
  Result:=true;
 end;
end; *)

Function IsSurfPlanar(surf:TJKSurface):boolean;
var v:integer;
    vx:TJKVertex;
    d:double;
begin
 result:=false;
 d:=surf.CalcD;
 for v:=0 to surf.Vertices.count-1 do
 begin
   vx:=surf.Vertices[v];
   With surf do
    if Abs(normal.dx*vx.x+normal.dy*vx.y+normal.dz*vx.z-d)>0.001 then exit;
  end;
 result:=true;
end;

Function IsSurfConvex(surf:TJKSurface):boolean;
var i,n:integer;
    v1,v2,v3:TJKVertex;
    v:TVector;
begin
 Result:=false;
 for i:=0 to surf.Vertices.count-1 do
 With Surf do
 begin
  v1:=Vertices[i];
  n:=NextVx(i);
  v2:=Vertices[n];
  n:=NextVx(n);
  v3:=Vertices[n];
  VMult(v3.x-v2.x,v3.y-v2.y,v3.z-v2.z,
        v1.x-v2.x,v1.y-v2.y,v1.z-v2.z,
        v.dx,v.dy,v.dz);
  if Smult(v.dx,v.dy,v.dz,normal.dx,normal.dy,normal.dz)<-0.0001 then exit;
 end;
 Result:=true;
end;

Function IsSectorConvex(sec:TJKSector):boolean;
var i,j:integer;
    v0,v:TJKVertex;
    d:double;
begin
 Result:=false;
 for i:=0 to sec.surfaces.count-1 do
 With sec.Surfaces[i] do
 begin
  v0:=vertices[0];
 for j:=0 to sec.vertices.count-1 do
 begin
  v:=sec.vertices[j];
  d:=Smult(v.x-v0.x,v.y-v0.y,v.z-v0.z,
           normal.dx,normal.dy,normal.dz);
  If d<-0.001 then exit;
 end;
 end;
 Result:=true;
end;

Procedure CalcRotVecData(var rd:TVecRotData; pch,yaw,rol:double);
begin
 With rd do
 begin
  cosPch:=cos(-PCH/180*Pi); {around X}
  sinPch:=sin(-PCH/180*Pi);
  cosYaw:=cos(-YAW/180*Pi); {around Y}
  sinYaw:=sin(-YAW/180*Pi);
  cosROL:=cos(-ROL/180*Pi); {around Y}
  sinROL:=sin(-ROL/180*Pi);
 end;
end;

Procedure RotVecbyData(var vec:TVector; var rd:TVecRotData);
var cosa,sina:double;
    dx,dy,dz:double;
begin
{around X}
With Rd do
begin
 dx:=vec.dx;
 dy:=cosPch*vec.dy+sinPCH*vec.dz;
 dz:=-sinPCH*vec.dy+cosPCH*vec.dz;

{Around Z}
 vec.dx:=cosYAW*dx+sinYAW*dy;
 vec.dy:=-sinYAW*dx+cosYAW*dy;
 vec.dz:=dz;

{Around Y}
 dx:=cosROL*vec.dx-sinROL*vec.dz;
 dy:=vec.dy;
 dz:=sinROL*vec.dx+cosROL*vec.dz;

 vec.dx:=dx;
 vec.dy:=dy;
 vec.dz:=dz;
end;
end;


Procedure GetJKPYR(const x,y,z:TVector;var pch,yaw,rol:double);
{Assumes ROL,PCH, YAW
 PCH - x, YAW - z, ROL - y}
var l:double;
    ny,nz:Tvector;
begin

 l:=sqrt(sqr(y.dy)+sqr(y.dx));

 if l=0 then yaw:=0 else
 begin
  yaw:=ArcCos(y.dy/l)/pi*180;
 end;

 if y.dx>0 then yaw:=360-yaw;

 ny:=y;
 RotateVector(ny,0,-yaw,0);

 PCH:=ArcCos(ny.dy)/pi*180;
 if ny.dz<0 then pch:=360-pch;

 nz:=z;
 RotateVector(nz,0,-Yaw,0);
 RotateVector(nz,-PCH,0,0);

 rol:=ArcCos(nz.dz)/pi*180;
 if nz.dx<0 then rol:=360-rol;

end;

Procedure RotateVector(var vec:TVector; pch,yaw,rol:double);
var mx:TMat3x3;
begin
 CreateRotMatrix(mx,pch,yaw,rol);
 MultVM3(mx,vec.dx,vec.dy,vec.dz);
(* cosa:=cos(-PCH/180*Pi); {around X}
 sina:=sin(-PCH/180*Pi);
 dx:=vec.dx;
 dy:=cosa*vec.dy+sina*vec.dz;
 dz:=-sina*vec.dy+cosa*vec.dz;


 cosa:=cos(-YAW/180*Pi); {Around Z}
 sina:=sin(-YAW/180*Pi);
 vec.dx:=cosa*dx+sina*dy;
 vec.dy:=-sina*dx+cosa*dy;
 vec.dz:=dz;

 cosa:=cos(-ROL/180*Pi);  {Around Y}
 sina:=sin(-ROL/180*Pi);
 dx:=cosa*vec.dx-sina*vec.dz;
 dy:=vec.dy;
 dz:=sina*vec.dx+cosa*vec.dz;

 vec.dx:=dx;
 vec.dy:=dy;
 vec.dz:=dz;*)
end;

Procedure CreateRotMatrix(var mx:TMat3x3; pch,yaw,rol:double);
var tmx:TMat3x3;
begin
 CreateMatrix(mx,-YAW,rt_z);

 CreateMatrix(tmx,-PCH,rt_x);
 MultM3(mx,tmx);

 CreateMatrix(tmx,-ROL,rt_y);
 MultM3(mx,tmx);

end;

Procedure CreateRotMatrixs(var mx:TMat3x3s; pch,yaw,rol:single);
var tmx:TMat3x3s;
begin
 CreateMatrixs(mx,-YAW,rt_z);

 CreateMatrixs(tmx,-PCH,rt_x);

 MultM3s(mx,tmx);

 CreateMatrixs(tmx,-ROL,rt_y);
 MultM3s(mx,tmx);
end;


Procedure ScaleMatrix(var mx:TMat3x3;scx,scy,scz:double);
var tmx:TMat3x3;
begin
 FillChar(tmx,sizeof(tmx),0);
 tmx[0,0]:=scx;
 tmx[1,1]:=scy;
 tmx[2,2]:=scz;
 MultM3(mx,tmx);
end;

Procedure ScaleMatrixs(var mx:TMat3x3s;scx,scy,scz:single);
var tmx:TMat3x3s;
begin
 FillChar(tmx,sizeof(tmx),0);
 tmx[0,0]:=scx;
 tmx[1,1]:=scy;
 tmx[2,2]:=scz;
 MultM3s(mx,tmx);
end;


Procedure DuplicateSector(sec,newsc:TJKSector;cx,cy,cz:double;nx,ny,nz:Tvector;dx,dy,dz:double);

var i,j:integer;
    sf,sf1:TJKSurface;
    v,nv:TJKVertex;
    tv:TTXVertex;
begin
 newsc.Assign(sec);

  for j:=0 to sec.vertices.count-1 do
  begin
   v:=sec.vertices[j];
   nv:=newsc.NewVertex;
   nv.x:=(v.x-cx)*nx.dx+(v.y-cy)*ny.dx+(v.z-cz)*nz.dx+dx+cx;
   nv.y:=(v.x-cx)*nx.dy+(v.y-cy)*ny.dy+(v.z-cz)*nz.dy+dy+cy;
   nv.z:=(v.x-cx)*nx.dz+(v.y-cy)*ny.dz+(v.z-cz)*nz.dz+dz+cz
  end;

 sec.Renumber;

 for i:=0 to sec.surfaces.count-1 do
 begin
  sf:=sec.surfaces[i];
  sf1:=newsc.NewSurface;
  sf1.Assign(sf);
  if sf.adjoin<>nil then sf1.nadj:=1;
  NewSc.Surfaces.Add(sf1);
  for j:=0 to sf.Vertices.count-1 do
  begin
   v:=sf.Vertices[j];
   sf1.AddVertex(newSc.Vertices[v.num]);
   sf1.txvertices[j].Assign(sf.txvertices[j]);
  end;

 end;

  for i:=0 to newsc.surfaces.count-1 do newsc.Surfaces[i].RecalcAll;
  NewSc.Renumber;
end;

Function FlattenSurface(surf:TJKSUrface):boolean;
var i:integer;
    sec:TJKSector;
    a,d:double;
    vx:TJKvertex;
    vec:Tvector;
    x0,y0,z0:double;
    ax,ay,az:double;

Procedure GetLineVec(vx:TJKVertex;var vec:TVector);
var i:integer;
    sf:TJKSurface;
    sf1,sf2:TJKSurface;
    vec1:Tvector;
begin
 sf1:=nil; sf2:=nil;
 for i:=0 to sec.surfaces.count-1 do
 begin
  sf:=sec.surfaces[i];
  if sf=surf then continue;
  if sf.Vertices.IndexOf(vx)<>-1 then
  begin
   if sf1=nil then begin sf1:=sf; continue; end;
   sf2:=sf; break;
  end;
 end;
 if (sf1=nil) and (sf2=nil) then vec:=surf.normal
 else if (sf1<>nil) and (sf2=nil) then
 begin
  VMult(surf.normal.dx,surf.normal.dy,surf.normal.dz,
        sf1.normal.dx,sf1.normal.dy,sf1.normal.dz,
        vec1.dx,vec1.dy,vec1.dz);
  VMult(vec1.dx,vec1.dy,vec1.dz,
        sf1.normal.dx,sf1.normal.dy,sf1.normal.dz,
        vec.dx,vec.dy,vec.dz);
  if Vlen(vec)<0.01 then vec:=sf.normal;
 end
 else
 begin
  VMult(sf1.normal.dx,sf1.normal.dy,sf1.normal.dz,
        sf2.normal.dx,sf2.normal.dy,sf2.normal.dz,
        vec.dx,vec.dy,vec.dz);
  if Vlen(vec)<0.01 then
  begin
   VMult(surf.normal.dx,surf.normal.dy,surf.normal.dz,
         sf1.normal.dx,sf1.normal.dy,sf1.normal.dz,
         vec1.dx,vec1.dy,vec1.dz);
   VMult(vec1.dx,vec1.dy,vec1.dz,
         sf1.normal.dx,sf1.normal.dy,sf1.normal.dz,
         vec.dx,vec.dy,vec.dz);
   if Vlen(vec)<0.01 then vec:=sf.normal;
  end;
 end;
end;

begin
 result:=true;
 sec:=surf.Sector;
 SaveSecUndo(sec,ch_changed,sc_geo);
 d:=surf.calcD;
 With surf.Vertices[0] do begin x0:=x; y0:=y; z0:=z; end;
 for i:=0 to surf.Vertices.count-1 do
 begin
  vx:=surf.Vertices[i];
  With surf,vx do
  a:=Abs(d-(normal.dx*x+normal.dy*y+normal.dz*z));
  if a<=0.001 then continue;
  GetLineVec(vx,vec);
  With vx do
  if PlaneLineXn(surf.normal,D,X,Y,Z,X+vec.DX,Y+vec.DY,Z+vec.DZ,ax,ay,az) then
  begin
   if (sqr(vx.x-ax)+sqr(vx.y-ay)+sqr(vx.z-az))<1 then
   begin
    vx.x:=ax;
    vx.y:=ay;
    vx.z:=az;
   end else result:=false;
  end else
  begin
   PanMessageFmt(mt_info,'Couldn''t force vertex %d,%d to plane of surface %d,%d',
                         [sec.num,vx.num,sec.num,surf.num]);
   result:=false;
  end;
 end;
 surf.RecalcAll;
 JedMain.SectorChanged(sec);
end;


{Lighting calculation}

Procedure ZeroVertexLights(lev:TJKLevel;scs:TSCMultiSel);
var s,sf,v:integer;
begin
 For s:=0 to scs.count-1 do
 With lev.Sectors[scs.getSC(s)] do
  begin
   for sf:=0 to Surfaces.count-1 do
    With Surfaces[sf] do
    begin

     For v:=0 to TXVertices.Count-1 do
     with TXVertices[v] do
     begin
      Intensity:=0;
      r:=0;
      g:=0;
      b:=0;
     end;
    end;
  end;
end;

Procedure CalcSectorAmbients(lev:TJKLevel;scs:TSCMultiSel);
var
    vlight,sflight:double;
    v,nv,s,sf:integer;
    sec:TJKSector;
    surf:TJKSurface;
begin
 for s:=0 to scs.count-1 do
 begin
  sec:=Lev.Sectors[scs.GetSC(s)];

  vlight:=0;sflight:=0;
  nv:=0;
  for sf:=0 to sec.surfaces.count-1 do
  begin
   surf:=sec.surfaces[sf];
   sflight:=sflight+surf.extralight;
   inc(nv,surf.vertices.count);
   for v:=0 to surf.vertices.count-1 do
   With surf.TXvertices[v] do
   begin
    if Intensity<0 then Intensity:=0;
    vlight:=vlight+Intensity;
   end;
  end;
  vlight:=vlight/nv;
  sflight:=sflight/sec.surfaces.count;
  if vlight>sflight then sec.ambient:=vlight else sec.ambient:=sflight;
 end;
end;

Procedure CalcLighting(Lev:TJKLevel;scs:TSCMultiSel);
var l,s,sf,v:integer;
    surf:TJKSurface;
    cv,v0:TJKVertex;
    lt:TJedLight;
    dist:double;
    sec:TJKsector;
    nv:integer;
    sfd:PChar;
    csd:PLCSurfData;
    gsfs:integer;


function IsBlocked(x1,y1,z1,x2,y2,z2:double):boolean;
var ls,lsf:integer;surf1:TJKSurface;
    dist:double;
    x,y,z:double;
    d:double;
    lbox:TBox;
    vx:TJKVertex;

Function SurfBlocks(asurf:TJKSurface):boolean;
begin
 Result:=false;
 if asurf=surf then exit;
 if (asurf.adjoin<>nil) then
 if (asurf.AdjoinFlags and AF_BlockLight=0) then exit;
 csd:=Pointer(sfd+asurf.mark*sizeof(TLCSurfData));
 d:=csd^.d;



 if newLight then
 begin
  if not DoBoxesIntersect(csd^.bbox,lbox) then exit;
 end;

{vx:=aSurf.Vertices[0];
with aSurf do
if not PlaneLineXnNew(normal,vx.x,vx.y,vx.z,x1,y1,z1,x2,y2,z2,x,y,z) then exit;}
with aSurf do
if not PlaneLineXn(normal,D,x1,y1,z1,x2,y2,z2,x,y,z) then exit;

if not IsPointWithinLine(x,y,z,x1,y1,z1,x2,y2,z2) then exit;
{Check if the intersection is close to the end of line
 if it is, check if the surface is facing the light.
 if it doesn't, block. Otherwise - don't}
 dist:=sqr(x-x2)+sqr(y-y2)+sqr(z-z2);
 if Dist<0.0001 then exit;
 dist:=sqr(x-x1)+sqr(y-y1)+sqr(z-z1);
 if Dist<0.0001 then exit;
{    begin
     With surf1,surf1.vertices[0]
     do dist:=SMult(normal.dx,normal.dy,normal.dz,x1-x,y1-y,z1-z);
     if dist<=0 then exit else continue;
    end;}

{    if newLight then
     if not IsPointInBox(csd^.bbox,x,y,z) then continue;}

Result:=IsPointOnSurface(asurf,x,y,z);
end;

begin
 SetBox(lbox,x1,x2,y1,y2,z1,z2);
 Result:=true;

 if lt.mark<>-1 then
 With Lev.Sectors[lt.mark] do
  for lsf:=0 to Surfaces.Count-1 do
  begin
   if SurfBlocks(Surfaces[lsf]) then
   begin
    if newLight then;
    exit;
   end;
  end;

 With Lev.Sectors[s] do
  for lsf:=0 to Surfaces.Count-1 do
  begin
   if SurfBlocks(Surfaces[lsf]) then
   begin
    if newLight then;
    exit;
   end;
  end;


 for ls:=0 to Lev.Sectors.Count-1 do
 if (ls<>s) and (ls<>lt.mark) then
 With Lev.Sectors[ls] do
  for lsf:=0 to Surfaces.Count-1 do
  begin
   if SurfBlocks(Surfaces[lsf]) then
   begin
    if newLight then;
    exit;
   end;
  end;
 Result:=false;
end;

begin {CalcLighting}
 {Reset all lighting values to 0}
 gsfs:=0;

 ZeroVertexLights(lev,scs);

 {Precalculate surface data}

  For s:=0 to lev.Sectors.count-1 do
  With lev.Sectors[s] do
  begin
  for sf:=0 to Surfaces.count-1 do
   With Surfaces[sf] do
     mark:=gsfs+sf;
   inc(gsfs,surfaces.count);
  end;

  GetMem(sfd,gsfs*sizeof(TLCSurfData));

 try
  For s:=0 to lev.Sectors.count-1 do
  With lev.Sectors[s] do
  begin
  for sf:=0 to Surfaces.count-1 do
   With Surfaces[sf] do
   begin
     csd:=Pointer(sfd+mark*sizeof(TLCSurfData));
     csd^.d:=CalcD;
     CalcSurfBBox(Surfaces[sf],csd^.bbox);
   end;
  end;

 {Find sectors for lights}
 for l:=0 to Lev.Lights.count-1 do
 With Lev.Lights[l] do
 begin
  mark:=FindSectorForXYZ(Lev,x,y,z);
 end;

 {Calc lights}

 Progress.Reset(lev.Sectors.count);
 Progress.Msg:='Calculating lights';

 For s:=0 to scs.count-1 do
 With lev.Sectors[scs.getSC(s)] do
 begin

 for sf:=0 to Surfaces.count-1 do
 begin
  surf:=Surfaces[sf];
  v0:=Surf.Vertices[0];

  for l:=0 to Lev.Lights.count-1 do
  begin
   lt:=Lev.Lights[l];

   {Check if the light is in front of surface}

   With surf do
    if SMult(normal.dx,normal.dy,normal.dz,
             lt.x-v0.x,lt.y-v0.y,lt.z-v0.z)<0.001 then continue;

   for v:=0 to surf.Vertices.Count-1 do
   begin
    {Check distance from the point to light}
    cv:=surf.Vertices[v];


    dist:=Sqrt(sqr(lt.x-cv.x)+sqr(lt.y-cv.y)+sqr(lt.z-cv.z));
    if dist>=lt.range then continue;

    {if light and vertex aren't in the same sector}
    {Check if the light is blocked}
    if (s<>lt.mark) and (lt.flags and LF_NoBlock=0) then
    if IsBlocked(lt.x,lt.y,lt.z,cv.x,cv.y,cv.z) then continue;

    {Light vertex}


    With surf.TXVertices[v] do
    begin
     Intensity:=Intensity+lt.Intensity*sqr((lt.range-dist)/lt.range);
     r:=r+lt.RGBIntensity*lt.r*sqr((lt.range-dist)/lt.range);
     g:=g+lt.RGBIntensity*lt.g*sqr((lt.range-dist)/lt.range);
     b:=b+lt.RGBIntensity*lt.b*sqr((lt.range-dist)/lt.range);
    end;
    { Intensity:=Intensity+lt.Intensity*(lt.range-dist)/lt.range;}
   end;

  end;

 end;
 Progress.Step;

end;

 CalcSectorAmbients(lev,scs);

finally
 Progress.Hide;
 FreeMem(sfd);
end;
end;


Function IsPointWithInLineInc(x,y,z,x1,y1,z1,x2,y2,z2:double):boolean;
{inclusive}
begin
 result:=
   (x-Real_Min(x1,x2)>-1e-4) and (Real_Max(x1,x2)-x>-1e-4) and
   (y-Real_Min(y1,y2)>-1e-4) and (Real_Max(y1,y2)-y>-1e-4) and
   (z-Real_Min(z1,z2)>-1e-4) and (Real_Max(z1,z2)-z>-1e-4);
end;

Function IsPointOnSurfaceNew(surf:TJKSurface;x,y,z:double):boolean;
var
    xv,yv:TVector;

    ppx,ppy,
    p1x,p1y,p2x,p2y,
    x0,y0,z0:double;

    i:integer;

    v1:TJKVertex;
    dist:double;
    vct:TVector;
    x1,x2,y1,y2,z1,z2:double;
begin
 Result:=false;
 With surf.normal do
 if Abs(dx)<0.99 then Vmult(dx,dy,dz,1,0,0,xv.dx,xv.dy,xv.dz) else
 Vmult(dx,dy,dz,0,1,0,xv.dx,xv.dy,xv.dz);
 Normalize(xv);

 With surf.normal do
 Vmult(dx,dy,dz,xv.dx,xv.dy,xv.dz,yv.dx,yv.dy,yv.dz);

 With surf do
 begin
  x0:=x;
  y0:=y;
  z0:=z;
 end;

 ppx:=Smult(x-x0,y-y0,z-z0,xv.dx,xv.dy,xv.dz);
 ppy:=Smult(x-x0,y-y0,z-z0,yv.dx,yv.dy,yv.dz);

 p1x:=0;
 p1y:=0;

 For i:=0 to Surf.Vertices.Count-1 do
 begin
  v1:=Surf.Vertices[surf.NextVX(i)];
  p2x:=Smult(v1.x-x0,v1.y-y0,v1.z-z0,xv.dx,xv.dy,xv.dz);
  p2y:=Smult(v1.x-x0,v1.y-y0,v1.z-z0,yv.dx,yv.dy,yv.dz);

  dist:=(p1x-ppx)*(p2y-ppy)-(p2x-ppx)*(p1y-ppy);
  if (Dist<-CloseEnough) then exit;

  p1x:=p2x;
  p1y:=p2y;

 end;

 Result:=true;
end;



{Check if a line crosses the surface. It's meant to be used for adjoined
 surfaces, so all the marginal cases are treated as crossing}
Function IsSurfCrossed(surf:TJKSurface;x1,y1,z1,x2,y2,z2:double):boolean;
var dist1,dist2:double;
    x0,y0,z0,px,py,pz:double;
    k:double;
begin
 result:=false;
 if surf.vertices.count=0 then exit;
 with surf.vertices[0] do
 begin
  x0:=x;
  y0:=y;
  z0:=z;
 end;

 With Surf.Normal do
 begin
  dist1:=Smult(dx,dy,dz,x1-x0,y1-y0,z1-z0);
  dist2:=Smult(dx,dy,dz,x2-x0,y2-y0,z2-z0);
 end;

 if (dist1>0.0001) and (dist2>0.0001) then exit;
 if (dist1<-0.0001) and (dist2<-0.0001) then exit;

 if (Abs(dist1)<=0.0001) and (Abs(dist2)<=0.0001) then
 begin
  result:=true;
  exit;
 end;

 {Find intersection of a line with plane}

 k:=dist1/(dist1-dist2);
 px:=x1+k*(x2-x1);
 py:=y1+k*(y2-y1);
 pz:=z1+k*(z2-z1);

 {Find if intersection is on surface}

 result:=IsPointOnSurface(surf,px,py,pz);

end;


Procedure CalcLightingNew(Lev:TJKLevel;scs:TSCMultiSel);
var i,l,s,sf,v,nl:integer;
    sec:TJKSector;
    surf:TJKSurface;

    lvec:Tvector;
    vx:TJKVertex;
    lt:TJedLight;

    lsec:TJKSector;
    d,dist,dist2,range2:double;

    {debug}
    dsc,dsf,dvx,dl:integer;


Function CanReach(viasec:TJKSector):boolean;
{if a ray from lt can reach vx by passing though this sector.
 Checks recursively}
var i:integer;
    surf:TJKSurface;
    px,py,pz:double;
begin
 result:=false;
 if viasec.mark<>0 then exit;
 if viasec=vx.sector then begin result:=true; exit; end;
 viasec.mark:=1;

 for i:=0 to viasec.surfaces.count-1 do
 begin
  surf:=viasec.surfaces[i];
  if (surf.adjoin=nil) {or (surf.geo>=4)}
  or (surf.vertices.count=0)
  or (surf.AdjoinFlags and AF_BlockLight<>0) then continue;

  {If a surface doesn't face the ray, skip}
  With surf.normal do
  if Smult(lvec.dx,lvec.dy,lvec.dz,dx,dy,dz)<=-0.0001 then continue;

  {Check if ray intersects the surface}

  if IsSurfCrossed(surf,lt.x,lt.y,lt.z,vx.x,vx.y,vx.z) then
  begin
   result:=CanReach(surf.adjoin.sector);
   if result then exit;
  end;


{  With surf.vertices[0] do
  If not PlaneLineXnNew(surf.normal,x,y,z,lt.x,lt.y,lt.z,vx.x,vx.y,vx.z,px,py,pz) then
  begin
    result:=CanReach(surf.adjoin.sector);
    if result then exit;
  end;

  if not IsPointWithinLineInc(px,py,pz,lt.x,lt.y,lt.z,vx.x,vx.y,vx.z) then continue;
  if IsPointOnSurface(surf,px,py,pz) then}
 end;
end;

Function IsBlocked:boolean;
{is a ray from lt to vx blocked}
var i:integer;
begin
 for i:=0 to lev.sectors.count-1 do
  lev.sectors[i].mark:=0;
 result:=not CanReach(lsec);
end;

begin
 {Find sectors for lights}
 nl:=0;
 for l:=0 to Lev.Lights.count-1 do
 With Lev.Lights[l] do
 begin
  mark:=FindSectorForXYZ(Lev,x,y,z);
  if mark=-1 then PanMessageFmt(mt_warning,'Light %d is not in any sector',[l])
  else inc(nl);
 end;

 if nl=0 then
 begin
  PanMessage(mt_error,'No valid lights to calculate!');
  exit;
 end;

 ZeroVertexLights(lev,scs);

 {debug}
{ dsc:=13;
 dsf:=2;
 dvx:=3;
 dl:=13;}

 Progress.Reset(lev.Sectors.count);
 Progress.Msg:='Calculating lights';

 for s:=0 to scs.count-1 do
 begin
  sec:=lev.Sectors[scs.getSC(s)];
  for sf:=0 to sec.surfaces.count-1 do
  begin
   surf:=sec.surfaces[sf];
   if surf.vertices.count=0 then continue; {Just in case}

   for l:=0 to lev.lights.count-1 do
   begin
    lt:=level.Lights[l];
    if lt.mark=-1 then continue;
    lsec:=level.Sectors[lt.mark];
    range2:=sqr(lt.range);
    vx:=surf.Vertices[0];
    {if a surface doesn't face the light, skip it}

    With surf.normal do
      d:=SMult(lt.x-vx.x,lt.y-vx.y,lt.z-vx.z,dx,dy,dz);
    if d<=0 then continue;


    for v:=0 to surf.vertices.count-1 do
    begin

    {Debug}
{    if (s=dsc) and (sf=dsf) AND (v=dvx) and (dl=l) then
    begin
     panMessageFmt(mt_info,'Light %d',[l]);
    end;}

     vx:=surf.Vertices[v];
     setVec(lvec,lt.x-vx.x,lt.y-vx.y,lt.z-vx.z);

     dist2:=sqr(lvec.dx)+sqr(lvec.dy)+sqr(lvec.dz);
     if dist2>=range2 then continue; {Light is out of range}

    if (lt.flags and LF_NoBlock=0) then
     if IsBlocked then continue;

    {Normalize}
    dist:=sqrt(dist2);
{    SetVec(lvec,lvec.dx/dist,lvec.dy/dist,lvec.dz/dist);
    With surf do
    dist2:=Smult(normal.dx,normal.dy,normal.dz,
                 lvec.dx,lvec.dy,lvec.dz);}

    dist2:=sqr((lt.range-dist)/lt.range);

    With surf.TXVertices[v] do
    begin
{     Intensity:=Intensity+dist2*lt.Intensity;
     r:=r+lt.RGBIntensity*dist2*lt.r;
     g:=g+lt.RGBIntensity*dist2*lt.g;
     b:=b+lt.RGBIntensity*dist2*lt.b;}
     Intensity:=Intensity+lt.Intensity*sqr((lt.range-dist)/lt.range);
     r:=r+lt.RGBIntensity*lt.r*sqr((lt.range-dist)/lt.range);
     g:=g+lt.RGBIntensity*lt.g*sqr((lt.range-dist)/lt.range);
     b:=b+lt.RGBIntensity*lt.b*sqr((lt.range-dist)/lt.range);

    end;

    end; {for vx}

   end; {for l}
  end; {for sf}
  Progress.Step;
 end; {for s}

 CalcSectorAmbients(lev,scs);
 Progress.Hide;

end;

Procedure OffsetByNode(lev:TJKLevel;nnode:integer;var x,y,z:double);
var i,j:integer;
    hnode,hnode1:THNode;
    mx:TMat3x3;
begin

  hnode:=lev.h3donodes[nnode];

  With Hnode do CreateRotMatrix(mx,pch,yaw,rol);
  MultVM3(mx,x,y,z);
  x:=x+hnode.x;
  y:=y+hnode.y;
  z:=z+hnode.z;

  hnode1:=hnode;

  While hnode1.parent<>-1 do
  begin
   hnode1:=lev.h3donodes[hnode1.Parent];

   With hnode1 do CreateRotMatrix(mx,pch,yaw,rol);
    MultVM3(mx,x,y,z);
    x:=x+hnode1.x;
    y:=y+hnode1.y;
    z:=z+hnode1.z;
   end;

end;


Procedure UnOffsetByNode(lev:TJKLevel;nnode:integer;var x,y,z:double);
var i,j,k:integer;
    hnode,hnode1:THNode;
    mdx,mdy,mdz:double;
    branch:TIntList;
    rmx,mx:TMat3x3;
    vs:TVertices;

begin {UnoffsetMeshes}
 branch:=TIntList.Create;

 hnode:=lev.h3donodes[nnode];
 {Trace back all parent nodes}
 branch.Clear;
 branch.Add(nnode);
 hnode1:=hnode;
  While hnode1.parent<>-1 do
  begin
   branch.Add(hnode1.Parent);
   hnode1:=lev.h3donodes[hnode1.Parent];
  end;

  {Unrotate and unoffset in backwards order}

  for k:=branch.count-1 downto 0 do
  begin
   hnode1:=lev.h3donodes[branch[k]];
   With hnode1 do CreateRotMatrix(mx,pch,yaw,rol);
   rmx[0,0]:=mx[0,0];  rmx[0,1]:=mx[1,0];  rmx[0,2]:=mx[2,0];
   rmx[1,0]:=mx[0,1];  rmx[1,1]:=mx[1,1];  rmx[1,2]:=mx[2,1];
   rmx[2,0]:=mx[0,2];  rmx[2,1]:=mx[1,2];  rmx[2,2]:=mx[2,2];

   x:=x-hnode1.x;
   y:=y-hnode1.y;
   z:=z-hnode1.z;
   MultVM3(rmx,x,y,z);
  end;

 branch.free;
end;


end.


