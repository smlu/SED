unit sft_persp_render;

interface

{This unit contains a Software implementation
 of TRenderer class}

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  GlobalVars, Geometry, Render, StdCtrls, misc_utils;

Const
     lTblSize=128;

Type
TPSFTRenderer=class(TRenderer)
 wnd:integer;
 hdc:Integer;
 Dc:TCanvas;
 {mode:TRenderStyle;}
 docull:boolean;
 cull_front:boolean;
 mx:TMat3x3s;
 rmx:TMat3x3s; {Reverse matrix}
 cdx,cdy,cdz:single;

 scscale:single;

 bm:TBitmap;
 wcx,wcy:integer;
 halfpsize:integer;

 LineTbl:array[0..ltblSize-1,0..ltblSize-1] of boolean;

 Procedure Initialize;override;
 Procedure BeginScene;override;
 Procedure SetViewPort(x,y,w,h:integer);override;
 {Procedure SetRenderStyle(rstyle:TRenderStyle);override;}
 Procedure SetColor(what,r,g,b:byte);override;

 Procedure EndScene;override;
 Procedure SetCulling(how:integer);override;
 Procedure DrawPolygon(p:TPolygon);override;
 Procedure DrawPolygons(ps:TPolygons);override;
 Procedure DrawPolygonsAt(ps:TPolygons;dx,dy,dz,pch,yaw,rol:double);override;
 Procedure DrawLine(v1,v2:TVertex);override;
 Procedure DrawVertex(X,Y,Z:double);override;
 Procedure DrawVertices(vxs:TVertices);override;
 Destructor Destroy; override;

 Procedure BeginPick(x,y:integer);override;
 Procedure EndPick;override;
 Procedure PickPolygon(p:TPolygon;id:integer);override;
 Procedure PickPolygons(ps:TPolygons;id:integer);override;
 Procedure PickLine(v1,v2:TVertex;id:integer);override;
 Procedure PickVertex(X,Y,Z:double;id:integer);override;
 Function GetXYZonPlaneAt(scX,scY:integer;pnormal:TVector; pX,pY,pZ:double; var X,Y,Z:double):Boolean;override;
 Function GetGridAt(scX,scY:integer;var X,Y,Z:double):boolean;override;
 Procedure SetPointSize(size:double);override;
 Procedure ProjectPoint(x,y,z:double; Var WinX,WinY:integer);override;
 Procedure UnProjectPoint(WinX,WinY:integer; WinZ:double; var x,y,z:double);override;
 Function HandleWMQueryPal:integer;override;
 Function HandleWMChangePal:integer;override;
 Function IsPolygonFacing(p:TPolygon):boolean;override;
Private
 Procedure SetCamera;
 Procedure SetRenderMatrix;
 Procedure SetPickMatrix(x,y:integer);
 Procedure RenderPoly(p:TPolygon);
 Procedure RenderLine(v1,v2:TVertex);
end;

implementation

uses Jed_Main, Lev_utils;


procedure DisableFPUExceptions ;
var
  FPUControlWord: WORD ;
asm
  FSTCW   FPUControlWord ;
  OR      FPUControlWord, $4 + $1 ; { Divide by zero + invalid operation }
  FLDCW   FPUControlWord ;
end ;

Procedure TPSFTRenderer.BeginScene;
begin
 DisableFPUExceptions;
 SetRenderMatrix;
 Dc.Rectangle(0,0,vpw,vph);
end;

{Procedure TPSFTRenderer.SetRenderStyle;
begin
 Case rstyle of
  DRAW_WIREFRAME: begin
                  end;
  DRAW_FLAT_POLYS:;
  DRAW_TEXTURED: ;
  DRAW_VERTICES: ;
 end;
 R_Style:=rstyle;
end;}

Procedure TPSFTRenderer.EndScene;
begin
 {Viewer.Canvas.Draw(0,0,bm);}
end;

Procedure TPSFTRenderer.SetCamera;
{var xscale,yscale,zscale:double;}
begin
 {CreateRotMatrixs(mx,-pch,-yaw,-rol);}
{ mx[0,0]:=xv.dx; mx[1,0]:=xv.dy; mx[0,2]:=xv.dz;
 mx[0,1]:=yv.dx; mx[1,1]:=yv.dy; mx[1,2]:=yv.dz;
 mx[0,2]:=zv.dx; mx[1,2]:=zv.dy; mx[2,2]:=zv.dz;}

 mx[0,0]:=xv.dx; mx[1,0]:=yv.dx; mx[0,2]:=zv.dx;
 mx[0,1]:=xv.dy; mx[1,1]:=yv.dy; mx[1,2]:=zv.dy;
 mx[0,2]:=xv.dz; mx[1,2]:=yv.dz; mx[2,2]:=zv.dz;

 scscale:=200*scale;


end;

Procedure TPSFTRenderer.SetRenderMatrix;
var dpx,dpy:single;
begin
 SetCamera;
 {dpx:=vpw/(ppunit*scale);
 dpy:=vph/vpw*dpx;

 dpx:=vpw/ppunit*scale;
 dpy:=vph/vpw*dpx;}

 dpx:=16;

{ ScaleMatrixs(mx,dpx,dpx,dpx);}
 cdx:={dpx*-}CamX;
 cdy:={dpx*-}CamY;
 cdz:={dpx*-}CamZ;
end;

Procedure TPSFTRenderer.SetPickMatrix(x,y:integer);
var dpx,dpy:single;
begin
 SetCamera;
 dpx:=vpw/(ppunit*scale);
 dpy:=vph/vpw*dpx;
 ScaleMatrixs(mx,dpx,dpy,dpx);

end;

Procedure TPSFTRenderer.Initialize;
begin
 wnd:=hViewer;
// vpx:=0; vpy:=0;
// vpw:=Viewer.ClientWidth;
// vph:=Viewer.ClientHeight;
{ bm:=TBitMap.Create;
 bm.width:=vpw;
 bm.height:=vph;}
 DC:=TCanvas.Create;
 DC.Handle:=GetDC(hViewer);
// DC:=Viewer.Canvas;
{ Dc:=bm.Canvas;}
 dc.Brush.Color:=0;
end;

Procedure TPSFTRenderer.SetViewPort(x,y,w,h:integer);
begin
 Inherited SetViewPort(x,y,w,h);
 wcx:=w div 2;
 wcy:=h div 2;
end;

Function TPSFTRenderer.IsPolygonFacing(p:TPolygon):boolean;
var dx,dy,dz:single;
begin
 dx:=p.normal.dx;
 dy:=p.normal.dy;
 dz:=p.normal.dz;
 MultVM3s(mx,dx,dy,dz);
 {Smult dx,dy,dz on 0,-1,0
  Basically - check dy}

 Result:=dy<0;
end;


Procedure TPSFTRenderer.DrawPolygon(p:TPolygon);
begin
 if doCull and (IsPolygonFacing(p)<>cull_front) then exit;
 RenderPoly(p);
end;

Procedure TPSFTRenderer.DrawPolygonsAt(ps:TPolygons;dx,dy,dz,pch,yaw,rol:double);
var i,j,n:integer;
    rmx:TMat3x3s;
    vx,vy,vz:single;
    shx,shy,shz:single;
    p:Tpolygon;
    x,y,x0,y0:integer;
    v:TVertex;
begin
 CreateRotMatrixs(rmx,pch,yaw,rol);
 for j:=0 to ps.count-1 do
 begin
  p:=ps[j];
 for i:=0 to p.Vertices.count-1 do
 begin
  v:=p.vertices[i];
  vx:=cdx+v.x; vy:=cdy+v.y; vz:=cdz+v.z;
  MultVM3s(rmx,vx,vy,vz);
  vx:=vx+dx; vy:=vy+dy; vz:=vz+dz;
  MultVM3s(mx,vx,vy,vz);
{  vx:=vx;
  vy:=vy;
  vz:=vz;}
  
  x:=wcx+round(vx);
  y:=wcy+round(vy);
  if i=0 then begin DC.MoveTo(x,y); x0:=x; y0:=y; end
  else DC.LineTo(x,y);
  DC.LineTo(x,y);
 end;
 if p.Vertices.count>0 then DC.LineTo(x0,y0);

 end;
end;

Procedure TPSFTRenderer.DrawPolygons(ps:TPolygons);
var i:integer;
begin
 for i:=0 to ps.count-1 do DrawPolygon(ps[i]);
end;


{Procedure TPSFTRenderer.DrawPolygons(ps:TPolygons);
var vx,vy,vz:single;
    i,j:integer;
    v:TVertex;
    x0,y0,x,y:integer;
    p:TPolygon;
begin
 fillchar(lineTbl,sizeof(lineTbl),0);

 for j:=0 to ps.count-1 do
 begin
  p:=ps[j];

 for i:=0 to p.Vertices.count-1 do
 begin
  v:=p.vertices[i];
  vx:=v.x; vy:=v.y; vz:=v.z;
  MultVM3s(mx,vx,vy,vz);
  vx:=CamX+vx;
  vy:=CamY+vy;
  vz:=CamZ+vz;
  x:=wcx+round(vx);
  y:=wcy+Round(vy);
  if i=0 then begin DC.MoveTo(x,y); x0:=x; y0:=y; end
  else DC.LineTo(x,y);
  DC.LineTo(x,y);
 end;
 if p.Vertices.count>0 then DC.LineTo(x0,y0);
end;

end; for j:=

end;}

Procedure TPSFTRenderer.RenderPoly(p:TPolygon);
var vx,vy,vz:single;
    i:integer;
    v:TVertex;
    x0,y0,x,y:integer;
begin
 for i:=0 to p.Vertices.count-1 do
 begin
  v:=p.vertices[i];
  vx:=cdx+v.x; vy:=cdy+v.y; vz:=cdz+v.z;
  MultVM3s(mx,vx,vy,vz);

{  vx:=cdx+vx;
  vy:=cdy+vy;
  vz:=cdz+vz;}

  {Project}

  x:=wcx+round(vx);
  y:=wcy+Round(vy);

  {if vz<1 then continue;
  x:=wcx+round(vx/vz);
  y:=wcy+Round(vy/vz);}

  if i=0 then begin DC.MoveTo(x,y); x0:=x; y0:=y; end
  else DC.LineTo(x,y);
  DC.LineTo(x,y);
 end;
 if p.Vertices.count>0 then DC.LineTo(x0,y0);
end;

Procedure TPSFTRenderer.DrawVertex(X,Y,Z:double);
var vx,vy,vz:single;
    px,py:integer;
begin
  vx:=cdx+x; vy:=cdy+y; vz:=cdz+z;
  MultVM3s(mx,vx,vy,vz);
{  vx:=vx;
  vy:=vy;
  vz:=vz;}
  {Project}
  px:=wcx+round(vx);
  py:=wcy+Round(vy);

{  if vz<1 then exit;
  px:=wcx+round(vx/vz);
  py:=wcy+Round(vy/vz);}

  Dc.Rectangle(px-halfpsize,py-halfpsize,px+halfpsize,py+halfpsize);
end;

Procedure TPSFTRenderer.DrawVertices(vxs:TVertices);
var i:integer;
begin
end;


Procedure TPSFTRenderer.RenderLine(v1,v2:TVertex);
begin
end;

Procedure TPSFTRenderer.DrawLine(v1,v2:TVertex);
var vx,vy,vz:single;
    px,py:integer;
begin
  vx:=cdx+v1.x; vy:=cdy+v1.y; vz:=cdz+v1.z;
  MultVM3s(mx,vx,vy,vz);
{  vx:=vx;
  vy:=vy;
  vz:=vz;}

  {Project}
  px:=wcx+round(vx);
  py:=wcy+Round(vy);
{  if vz<1 then exit;
  px:=wcx+round(vx/vz);
  py:=wcy+Round(vy/vz);}


  Dc.MoveTo(px,py);
  vx:=cdx+v2.x; vy:=cdy+v2.y; vz:=cdz+v2.z;
  MultVM3s(mx,vx,vy,vz);
{  vx:=vx;
  vy:=vy;
  vz:=vz;}

  {Project}
  px:=wcx+round(vx);
  py:=wcy+Round(vy);
{  if vz<1 then exit;
  px:=wcx+round(vx/vz);
  py:=wcy+Round(vy/vz);}

  Dc.LineTo(px,py);
end;

Destructor TPSFTRenderer.Destroy;
begin
end;

Procedure TPSFTRenderer.BeginPick(x,y:integer);
begin
 SetPickMatrix(x,y);
 Selected.Clear;

end;

Procedure TPSFTRenderer.EndPick;
var i,j,hits,names:integer;pi:^Integer;
    z:integer;
    zs:TIntList;

Procedure AddID(id,z:integer);
var i,n:integer;
begin
 n:=Selected.Count;
 for i:=0 to n-1 do
 begin
  if z<zs[i] then begin n:=i; break; end;
 end;
 Selected.Insert(n,ID);
 zs.Insert(n,z);
end;

begin
 Selected.Clear;
 zs:=TIntList.Create;
 zs.free;
end;

Procedure TPSFTRenderer.PickPolygon(p:TPolygon;id:integer);
begin
end;

Procedure TPSFTRenderer.PickPolygons(ps:TPolygons;id:integer);
var i:integer;
begin
end;

Procedure TPSFTRenderer.PickLine(v1,v2:TVertex;id:integer);
begin
end;

Procedure TPSFTRenderer.PickVertex(X,Y,Z:double;id:integer);
begin
end;

Function TPSFTRenderer.GetXYZonPlaneAt(scX,scY:integer;pnormal:TVector; pX,pY,pZ:double; var X,Y,Z:double):Boolean;
var
    ax1,ay1,az1,
    ax2,ay2,az2:double;
    d:double;
begin
 SetRenderMatrix;
 ax1:=-1; ax2:=1;
 ay1:=-1; ay2:=1;
 az1:=-1; az2:=1;

 { gluUnProject(scX,vp[3]-scY,0,mmx,pmx,vp,ax1,az1,ay1);
 gluUnProject(scX,vp[3]-scY,1,mmx,pmx,vp,ax2,az2,ay2);}

 With pnormal do d:=dx*pX+dy*pY+dz*pZ;

 result:=PlaneLineXn(pnormal,D,ax1,ay1,az1,ax2,ay2,az2,x,y,z);
 Result:=Result and (abs(x)<xrange) and (abs(y)<Yrange) and (abs(Z)<ZRange);
 if not result then exit;
 x:=JKRound(x); y:=JKRound(y); z:=JKRound(z);
end;

Function TPSFTRenderer.GetGridAt(scX,scY:integer;var X,Y,Z:double):boolean;
begin
 Result:=GetXYZonPlaneAt(scX,ScY,gnormal,GridX,GridY,GridZ,x,y,z);
 if result then begin x:=JKRound(x); y:=JKRound(y); z:=JKRound(z); end;
end;

Procedure TPSFTRenderer.UnProjectPoint(WinX,WinY:integer; WinZ:double; var x,y,z:double);
begin
 SetRenderMatrix;
end;

Procedure TPSFTRenderer.ProjectPoint(x,y,z:double; Var WinX,WinY:integer);
var
    WX,WY,WZ:double;
begin
 SetRenderMatrix;
end;


Procedure TPSFTRenderer.SetPointSize(size:double);
begin
 halfpsize:=round(size/2);
end;

Function TPSFTRenderer.HandleWMQueryPal:integer;
begin
 Result:=0;
end;

Function TPSFTRenderer.HandleWMChangePal:integer;
begin
 Result:=0;
end;

Procedure TPSFTRenderer.SetCulling(how:integer);
begin
 docull:=how<>r_CULLNONE;
 cull_Front:=how=r_CULLFRONT;
end;

Procedure TPSFTRenderer.SetColor(what,r,g,b:byte);
begin
 case what of
  CL_FRONT: begin DC.Pen.Color:=RGB(r,g,b); DC.Brush.Color:=RGB(r,g,b); end;
  CL_BackGround: DC.Brush.Color:=RGB(r,g,b);
 end;
end;

end.

