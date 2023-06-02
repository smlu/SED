unit Render;

{This unit defines the abstract TRenderer
 class, an interface for rendering 3D objects}

interface
uses Windows,Forms,Geometry,misc_utils,GlobalVars;
Const
 Line_color=1;
 vertex_color=2;
 poly_color=4;
 all_colors=7;

Const xrange=20000;
      yrange=20000;
      zrange=20000;
      ppunit=16; {Pixels per unit}

CL_FRONT=0;
CL_Back=0;
CL_BackGround=1;
{CL_GRID=2;}
R_CULLNONE=0;
R_CULLBACK=-1;
R_CULLFRONT=1;

type

TRColor=record
 r,g,b:byte;
end;

TPlane=record
   x,y,z:double;
   norm:Tvector;
  end;

TRenderer=class
 hpal:integer;
 hViewer:integer;
 CamX,CamY,CamZ:double;
 {Pch,Yaw,Rol:double;}
 xv,yv,zv:TVector;
 scale:double;
 vpx,vpy,vpw,vph:integer; {viewport}
 FSelected:TIntList;
 Back_Clr,Front_clr,
 bgnd_clr:TRColor;

 {Grid variables}
 gnormal:TVector;
 gxnormal,gynormal:TVector;
 GridX,GridY,GridZ:double;
 GridLine,
 GridDot,GridStep:double;

 Private

  lplane,rplane,uplane,bplane:Tplane;

 public

 Property Selected:TIntList read Fselected;
 Constructor Create(TheViewer:integer);
 Destructor Destroy; Override;
 Procedure Initialize;virtual;abstract;
 Procedure SetViewPort(x,y,w,h:integer);virtual;
 Procedure SetColor(what,r,g,b:byte);virtual;
 Procedure SetPointSize(size:double);virtual;abstract;
 {Procedure SetRenderStyle(rstyle:TRenderStyle);virtual;abstract;}
 Procedure BeginScene;virtual;abstract;
 Procedure EndScene;virtual;abstract;
 Procedure SetCulling(how:integer);virtual;abstract;
 Procedure DrawPolygon(p:TPolygon);virtual;abstract;
 Procedure DrawPolygonsAt(ps:TPolygons;dx,dy,dz,pch,yaw,rol:double);virtual;abstract;
 Procedure DrawPolygons(ps:TPolygons);virtual;abstract;
 Procedure DrawLine(v1,v2:TVertex);virtual;abstract;
 Procedure DrawVertex(X,Y,Z:double);virtual;abstract;
 Procedure DrawVertices(vxs:TVertices);virtual;abstract;
 Procedure DrawCircle(cx,cy,cz,rad:double);virtual;abstract;
 Procedure DrawGrid;virtual;
 Procedure Configure;virtual;abstract; {Setup dialog}

 Procedure BeginPick(x,y:integer);virtual;abstract;
 Procedure EndPick;virtual;abstract;
 Procedure PickPolygon(p:TPolygon;id:integer);virtual;abstract;
 Procedure PickPolygons(ps:TPolygons;id:integer);virtual;abstract;
 Procedure PickLine(v1,v2:TVertex;id:integer);virtual;abstract;
 Procedure PickVertex(X,Y,Z:double;id:integer);virtual;abstract;

 Procedure BeginRectPick(x1,y1,x2,y2:integer);
 Procedure EndRectPick;
 Function IsPolygonInRect(p:TPolygon):boolean;
 Function ArePolygonsInRect(ps:TPolygons):boolean;
 Function IsLineInRect(v1,v2:TVertex):boolean;
 Function IsVertexInRect(X,Y,Z:double):boolean;


 Function S2MX(X:integer):double;
 Function S2MY(Y:integer):double;
 Procedure SetGridNormal(dx,dy,dz:double);
 Procedure SetGridXNormal(dx,dy,dz:double);
 Procedure SetGridYNormal(dx,dy,dz:double);
 Procedure SetZ(dx,dy,dz:double);
 Procedure SetX(dx,dy,dz:double);
 Procedure SetY(dx,dy,dz:double);
 Function GetXYZonPlaneAt(scX,scY:integer;pnormal:TVector; pX,pY,pZ:double; var X,Y,Z:double):Boolean;virtual;abstract;
 Function GetGridAt(scX,scY:integer;var X,Y,Z:double):boolean;virtual;abstract;
 Procedure GetNearestGrid(iX,iY,iZ:double; Var X,Y,Z:double);
 Procedure ProjectPoint(x,y,z:double; Var WinX,WinY:integer);virtual;abstract;
 Procedure UnProjectPoint(WinX,WinY:integer; WinZ:double; var x,y,z:double);virtual;abstract;
 Function HandleWMQueryPal:integer;virtual;abstract;
 Function HandleWMChangePal:integer;virtual;abstract;
 Function IsPolygonFacing(p:TPolygon):boolean;virtual;abstract;
end;

implementation

uses Lev_utils;

Procedure TRenderer.SetViewPort(x,y,w,h:integer);
begin
 vpx:=x;vpy:=y;vpw:=w; vph:=h;
end;

Constructor TRenderer.Create(TheViewer:integer);
var r:Trect;
begin
 FSelected:=TIntList.Create;
 hViewer:=TheViewer;

 GetClientRect(hViewer,r);
 vpx:=r.left; vpy:=r.right;
 vpw:=r.right-r.left; vph:=r.bottom-r.top;

 // With Viewer do
// begin
//  vpx:=0; vpy:=0;
//  vpw:=ClientWidth; vph:=ClientHeight;
// end;
 scale:=1;
 GridStep:=0.2;
 GridDot:=0.2;
 GridLine:=1;
 Initialize;
 SetColor(CL_FRONT,255,255,255);
 SetColor(CL_BACK,127,127,127);
end;

Procedure TRenderer.SetColor(what,r,g,b:byte);
begin
 case what of
  CL_FRONT: begin
             Front_clr.r:=r;
             Front_clr.g:=g;
             Front_clr.b:=b;
            end; 
  CL_BackGround:begin
             bgnd_clr.r:=r;
             bgnd_clr.g:=g;
             bgnd_clr.b:=b;
            end;
 end;
end;

Destructor TRenderer.Destroy;
begin
 FSelected.Free;
end;

Function TRenderer.S2MX(X:integer):double;
begin
 Result:=(x-vpw/2)/ppunit;
end;

Function TRenderer.S2MY(Y:integer):double;
begin
 Result:=(vph/2-y)/ppunit;
end;

{Procedure TRenderer.RecalcGrid;
begin
 GridD:=gnormal.dX*GridX+gnormal.dY*GridY+gnormal.dZ*GridZ;
end;}

Procedure TRenderer.SetGridNormal(dx,dy,dz:double);
var g:TVector;
begin
 g.dx:=dx;
 g.dy:=dy;
 g.dz:=dz;
 Normalize(g);

 if IsClose(Vlen(g),0) then exit;
 gnormal:=g;
end;

Procedure TRenderer.SetGridXNormal(dx,dy,dz:double);
var g:TVector;
begin
 g.dx:=dx;
 g.dy:=dy;
 g.dz:=dz;
 Normalize(g);

 if IsClose(Vlen(g),0) then exit;

 gxnormal:=g;
 VMult(gnormal.dx,gnormal.dy,gnormal.dz,
       gxnormal.dx,gxnormal.dy,gxnormal.dz,
       gynormal.dx,gynormal.dy,gynormal.dz);
end;

Procedure TRenderer.SetGridYNormal(dx,dy,dz:double);
begin
 gxnormal.dx:=dx;
 gxnormal.dy:=dy;
 gxnormal.dz:=dz;
 Normalize(gxnormal);
 VMult(gnormal.dx,gnormal.dy,gnormal.dz,
       gxnormal.dx,gxnormal.dy,gxnormal.dz,
       gynormal.dx,gynormal.dy,gynormal.dz);
end;


Procedure TRenderer.SetZ(dx,dy,dz:double);
var g:TVector;
begin
 g.dx:=dx;
 g.dy:=dy;
 g.dz:=dz;
 Normalize(g);

 if IsClose(Vlen(g),0) then exit;
 zv:=g;
end;

Procedure TRenderer.SetX(dx,dy,dz:double);
begin
 xv.dx:=dx;
 xv.dy:=dy;
 xv.dz:=dz;
 Normalize(xv);
 VMult(zv.dx,zv.dy,zv.dz,
       xv.dx,xv.dy,xv.dz,
       yv.dx,yv.dy,yv.dz);

 VMult(yv.dx,yv.dy,yv.dz,
       zv.dx,zv.dy,zv.dz,
       xv.dx,xv.dy,xv.dz);
end;

Procedure TRenderer.SetY(dx,dy,dz:double);
begin
 yv.dx:=dx;
 yv.dy:=dy;
 yv.dz:=dz;
 Normalize(yv);
 VMult(yv.dx,yv.dy,yv.dz,
       zv.dx,zv.dy,zv.dz,
       xv.dx,xv.dy,xv.dz);

 VMult(zv.dx,zv.dy,zv.dz,
       xv.dx,xv.dy,xv.dz,
       yv.dx,yv.dy,yv.dz);
end;


Procedure TRenderer.DrawGrid;
var i,j,n:integer;
    x,y,z:double;
    v1,v2:TVertex;
    gsize2:double;
begin
 v1:=TVertex.Create; v2:=TVertex.Create;
 n:=Round(GridSize/GridDot);

 For i:=-(n div 2-1) to n div 2-1 do
 for j:=-(n div 2-1) to n div 2-1 do
 begin
  x:=GridX+(i*GridDot*gxnormal.dx)+(j*GridDot*gynormal.dx);
  y:=GridY+(i*GridDot*gxnormal.dy)+(j*GridDot*gynormal.dy);
  z:=GridZ+(i*GridDot*gxnormal.dz)+(j*GridDot*gynormal.dz);
  DrawVertex(x,y,z);
 end;
 n:=Round(GridSize/GridLine);

 gsize2:=GridSize/2;

 For i:=-n div 2 to n div 2 do
 begin
  v1.x:=GridX+(i*GridLine*gxnormal.dx)+(gsize2*gynormal.dx);
  v1.y:=GridY+(i*GridLine*gxnormal.dy)+(gsize2*gynormal.dy);
  v1.z:=GridZ+(i*GridLine*gxnormal.dz)+(gsize2*gynormal.dz);

  v2.x:=GridX+(i*GridLine*gxnormal.dx)+(-gsize2*gynormal.dx);
  v2.y:=GridY+(i*GridLine*gxnormal.dy)+(-gsize2*gynormal.dy);
  v2.z:=GridZ+(i*GridLine*gxnormal.dz)+(-gsize2*gynormal.dz);
  DrawLine(v1,v2);
 end;

 For i:=-n div 2 to n div 2 do
 begin
  v1.x:=GridX+(gsize2*gxnormal.dx)+(i*GridLine*gynormal.dx);
  v1.y:=GridY+(gsize2*gxnormal.dy)+(i*GridLine*gynormal.dy);
  v1.z:=GridZ+(gsize2*gxnormal.dz)+(i*GridLine*gynormal.dz);

  v2.x:=GridX+(i*GridLine*gynormal.dx)+(-gsize2*gxnormal.dx);
  v2.y:=GridY+(i*GridLine*gynormal.dy)+(-gsize2*gxnormal.dy);
  v2.z:=GridZ+(i*GridLine*gynormal.dz)+(-gsize2*gxnormal.dz);
  DrawLine(v1,v2);
 end;

 With clGridX do SetColor(cl_Front,r,g,b);
 v1.x:=GridX;v1.y:=GridY;v1.z:=GridZ;
 v2.x:=GridX+gsize2*gxnormal.dx;
 v2.y:=GridY+gsize2*gxnormal.dy;
 v2.z:=GridZ+gsize2*gxnormal.dz;
 DrawLine(v1,v2);

 With clGridY do SetColor(cl_Front,r,g,b);
 v1.x:=GridX;v1.y:=GridY;v1.z:=GridZ;
 v2.x:=GridX+gsize2*gynormal.dx;
 v2.y:=GridY+gsize2*gynormal.dy;
 v2.z:=GridZ+gsize2*gynormal.dz;
 DrawLine(v1,v2);


 v1.free; v2.free;

end;


Procedure TRenderer.GetNearestGrid(iX,iY,iZ:double; Var X,Y,Z:double);
var revgs,ld:double;
    GridD:Double;
    px,py:double;
begin
 Revgs:=1/gridstep;


 With gxnormal do
 px:=SMult(dx,dy,dz,ix-GridX,iy-GridY,iz-GridZ);

 With gynormal do
 py:=SMult(dx,dy,dz,ix-GridX,iy-GridY,iz-GridZ);

 px:=Round(pX*revgs)*gridstep;
 py:=Round(pY*revgs)*gridstep;

  x:=GridX+(px*gxnormal.dx)+(py*gynormal.dx);
  y:=GridY+(px*gxnormal.dy)+(py*gynormal.dy);
  z:=GridZ+(px*gxnormal.dz)+(py*gynormal.dz);

end;

Procedure TRenderer.BeginRectPick(x1,y1,x2,y2:integer);
var i:integer;
    px1,py1,pz1,px2,py2,pz2,px3,py3,pz3:double;
begin
 if x1>x2 then begin i:=x2; x2:=x1; x1:=i; end;
 if y1>y2 then begin i:=y2; y2:=y1; y1:=i; end;

 {Left plane}
 UnprojectPoint(x1,y1,0,px1,py1,pz1);
 UnprojectPoint(x1,y1,1,px2,py2,pz2);
 UnprojectPoint(x1,y2,0,px3,py3,pz3);

 With lplane do
 begin
  Vmult(px3-px1,py3-py1,pz3-pz1,
        px2-px1,py2-py1,pz2-pz1,
        norm.dx,norm.dy,norm.dz);
  Normalize(norm);
  x:=px1; y:=py1; z:=pz1;
 end;

 {Right plane}
 UnprojectPoint(x2,y1,0,px1,py1,pz1);
 UnprojectPoint(x2,y1,1,px2,py2,pz2);
 UnprojectPoint(x2,y2,0,px3,py3,pz3);

 With rplane do
 begin
  Vmult(px2-px1,py2-py1,pz2-pz1,
        px3-px1,py3-py1,pz3-pz1,
        norm.dx,norm.dy,norm.dz);
  x:=px1; y:=py1; z:=pz1;
  Normalize(norm);
 end;

 {Top plane}
 UnprojectPoint(x1,y1,0,px1,py1,pz1);
 UnprojectPoint(x1,y1,1,px2,py2,pz2);
 UnprojectPoint(x2,y1,0,px3,py3,pz3);

 With uplane do
 begin
  Vmult(px2-px1,py2-py1,pz2-pz1,
        px3-px1,py3-py1,pz3-pz1,
        norm.dx,norm.dy,norm.dz);
  x:=px1; y:=py1; z:=pz1;
  Normalize(norm);
 end;

 {Bottom plane}
 UnprojectPoint(x1,y2,0,px1,py1,pz1);
 UnprojectPoint(x1,y2,1,px2,py2,pz2);
 UnprojectPoint(x2,y2,0,px3,py3,pz3);

 With bplane do
 begin
  Vmult(px3-px1,py3-py1,pz3-pz1,
        px2-px1,py2-py1,pz2-pz1,
        norm.dx,norm.dy,norm.dz);
  x:=px1; y:=py1; z:=pz1;
  Normalize(norm);
 end;

end;

Procedure TRenderer.EndRectPick;
begin
end;

Function TRenderer.IsPolygonInRect(p:TPolygon):boolean;
var i:integer;
begin
 result:=false;
 for i:=0 to p.vertices.count-1 do
 With p.vertices[i] do
 if not IsVertexInRect(x,y,z) then exit;
 result:=true;
end;

Function TRenderer.ArePolygonsInRect(ps:TPolygons):boolean;
var i:integer;
begin
 result:=false;
 for i:=0 to ps.count-1 do
 if not IsPolygonInRect(ps[i]) then exit;
 result:=true;
end;

Function TRenderer.IsLineInRect(v1,v2:TVertex):boolean;
begin
 Result:=IsVertexInRect(v1.x,v1.y,v1.z) and IsVertexInRect(v2.x,v2.y,v2.z); 
end;

Function IsInFront(X,Y,Z:double;const pl:TPlane):boolean;
begin
 result:=Smult(x-pl.x,y-pl.y,z-pl.z,pl.norm.dx,pl.norm.dy,pl.norm.dz)>0;
end;

Function TRenderer.IsVertexInRect(X,Y,Z:double):boolean;
begin
 result:=IsInFront(x,y,z,lplane) and IsInFront(x,y,z,rplane) and
         IsInFront(x,y,z,uplane) and IsInFront(x,y,z,bplane);
end;


end.
