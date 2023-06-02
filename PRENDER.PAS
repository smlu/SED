unit PRender;

interface

uses u_pj3dos, Geometry, Classes, J_Level, Forms, Windows, Messages,
     graph_files, Graphics, files, fileOperations, misc_utils,
     lev_utils, sysUtils, GlobalVars;

const
FrontPlane=5E-2;
BackPlane=5000;

type

T3DPTexture=class
 cmpname:string;
 width,height:integer;
 {Procedure SetCurrent;virtual;abstract;}
end;

T3DPVertex=record
 x,y,z:single;
end;


TVXdets=class
 {vx:^T3DPVertex;}
 x,y,z:single;
 u,v:single;
 r,g,b:byte;
end;

T3DPSurf=class
 normal:Tvector;
 vxds:TList;
 tx:T3DPTexture;
 Constructor Create;
 Destructor Destroy;override;
 Function AddVXD:TVXDets;
 Function GetVXD(n:integer):TVXDets;
end;

T3DPMesh=class
 surfs:TList;
 center:T3DPVertex;
 radius:single;
 Constructor Create;
 Destructor Destroy;override;
 Function GetSurf(n:integer):T3DPSurf;
 Function AddSurf:T3DPSurf;
 Procedure CalculateSphere;
end;

TMeshes=class(TList)
 Function GetItem(n:integer):T3DPMesh;
 Procedure SetItem(n:integer;v:T3DPMesh);
Property Items[n:integer]:T3DPMesh read GetItem write SetItem; default;
end;


TPickType=(pk_nothing,pk_surface,pk_thing);

TPreviewRender=class
 CamX,CamY,CamZ:Double;
 PCH,YAW:Double;
 whandle:integer;
 vwidth,vheight:integer;
 gamma:double;
 SelSC:TJKSector;
 SelSF:integer;
 SelTH:TJKThing;
 thing_cmp:string;
 Constructor Create(aForm:TForm);
 Procedure Initialize;virtual;abstract;
 Procedure ClearSectors;virtual;abstract;
 Procedure ClearThings;virtual;abstract;
 Procedure AddSector(s:TJKSector);virtual;abstract;
 Procedure DeleteSector(s:TJKSector);virtual;abstract;
 Procedure UpdateSector(s:TJKSector);virtual;abstract;
 Procedure AddThing(th:TJKThing);virtual;abstract;
 Procedure DeleteThing(th:TJKThing);virtual;abstract;
 Procedure UpdateThing(th:TJKThing);virtual;abstract;
 Procedure SetViewPort(x,y,w,h:integer);virtual;abstract;
 Procedure Redraw;virtual;abstract;
 Procedure HandleActivate(const msg:TMessage);virtual;abstract;
 procedure HandlePaint(hdc:integer);virtual;abstract;
 Function PickAt(X,Y:integer):TPickType;virtual;abstract;
 Function SetGamma(gamma:double):integer;virtual;abstract;
 Procedure SetViewToThing(th:TJKThing);virtual;abstract;
 Procedure SetPCHYAW(pch,yaw:double);virtual;abstract;
 {Arranges camera so that the camera looks at the thing with max possible size}
 Procedure SetThing3DO(th:TJKThing;a3do:TPJ3DO);virtual;abstract;
end;



TNewPRenderer=class(TPreviewRender)
   TXList,CmpList:TStringList;
   sectors:TSectors;
   things:TThings;
   masterPalSet:boolean;
   MasterPal:TCMPPal;

   ThList,SCList:TMeshes;

   tn,bn,ln,rn:TVector; {Normals of clipping planes}

 Constructor Create(aForm:TForm);
 {Constructor CreateFromPanel(aPanel:TPanel);}
 Procedure ClearSectors;override;
 Procedure ClearThings;override;
 Procedure AddSector(s:TJKSector);override;
 Procedure DeleteSector(s:TJKSector);override;
 Procedure UpdateSector(s:TJKSector);override;
 Procedure AddThing(th:TJKThing);override;
 Procedure DeleteThing(th:TJKThing);override;
 Procedure UpdateThing(th:TJKThing);override;

 Procedure Redraw;override;
 Destructor Destroy;override;
 Function PickAt(X,Y:integer):TPickType;override;

 Function SetGamma(gamma:double):integer;override;

 {Virtuals}

 Function LoadTexture(const name:string;const pal:TCMPPal;const cmp:TCMPTable):T3DPTexture;virtual;abstract;
 Procedure DrawMesh(m:T3DPMesh);virtual;abstract;
 Procedure GetWorldLine(X,Y:integer;var X1,Y1,Z1,X2,Y2,Z2:double);virtual;abstract;
 {Must also override Initialize;}

 { Optionally Destroy; redraw; and }

 Procedure HandleActivate(const msg:TMessage);override;
 procedure HandlePaint(hdc:integer);override;

 Procedure ClearTXList;virtual;
 Procedure SetThing3DO(th:TJKThing;a3do:TPJ3DO);override;
Private
 Function GetTexture(const name,cmp:string):T3DPTexture;
 Procedure LoadMasterPal(l:TJKLevel);
 Function SecToMesh(s:TJKSector):T3DPMesh;
 Function ThingToMesh(th:TJKThing):T3DPMesh;
 Function BadThingMesh(th:TJKThing):T3DPMesh;
 Function A3DOToMesh(th:TJKThing;a3do:TPJ3DO):T3DPMesh;
end;


implementation

Constructor TPreviewRender.Create(aForm:TForm);
begin
 whandle:=aForm.Handle;
 vwidth:=aForm.ClientWidth;
 vheight:=aForm.ClientHeight;
 gamma:=1;
 thing_cmp:='dflt.cmp';
end;

procedure DisableFPUExceptions ;
var
  FPUControlWord: WORD ;
asm
  FSTCW   FPUControlWord ;
  OR      FPUControlWord, $4 + $1 ; { Divide by zero + invalid operation }
  FLDCW   FPUControlWord ;
end ;

Constructor TNewPRenderer.Create(aForm:TForm);
begin
 Inherited Create(aForm);
 TxList:=TStringList.Create;
 TXList.Sorted:=true;
 CmpList:=TStringList.Create;
 CmpList.Sorted:=true;
 Sectors:=TSectors.Create;
 scList:=TMeshes.Create;
 thList:=TMeshes.Create;
 things:=TThings.Create;
 WHandle:=aForm.Handle;
end;

Destructor TNewPRenderer.Destroy;
begin
 ClearTXList;
 ClearThings;
 ClearSectors;
 THList.Free;
 SCList.Free;

 TxList.Free;
 CmpList.Free;
 Sectors.Free;
 things.Free;
 inherited Destroy;
end;

Procedure TNewPRenderer.ClearTXList;
var i:integer;
begin
 for i:=0 to TXList.Count-1 do TXList.Objects[i].Free;
 TXList.Clear;
 for i:=0 to CMPList.Count-1 do FreeMem(Pointer(CMPList.Objects[i]));
 CMPList.Clear;
end;

Function Min(i1,i2:integer):integer;
begin
 if i1<i2 then result:=i1 else result:=i2;
end;

Procedure AdjustPalGamma(var pal:TCMPPal;gamma:double);
var i:integer;
begin
 for i:=0 to 255 do
 With Pal[i] do
 begin
  r:=Min(Round(r*gamma),255);
  g:=Min(Round(g*gamma),255);
  b:=Min(Round(b*gamma),255);
 end;
end;

Function TNewPRenderer.SetGamma(gamma:double):integer;
var
   i,j,ic:integer;
   {p cmp:^TCMPPal;}
   f:tfile;
   gmult:double;
begin
 MasterPalSet:=false;
 ClearTXList;
 self.gamma:=gamma;

 result:=1;


 // if TXList.Count<>0 then
//  PanMessage(mt_warning,'Gamma change will take effect after you reload 3D preview');
{ gmult:=gamma;
 for i:=0 to TXList.count-1 do
 if TXList.Objects[i]<>nil then
 With TOGLTexture(TXList.Objects[i]) do
 begin
  ic:=CmpList.IndexOf(cmpname);
  pcmp:=Pointer(CmpList.Objects[ic]);
  cmp:=pcmp^;
  AdjustPalGamma(cmp,gamma);
  SetCurrent;

  pSubPalProc(GL_TEXTURE_2D, 0, 256, GL_RGB, GL_UNSIGNED_BYTE, cmp );
 end;}
end;

Procedure TNewPRenderer.ClearSectors;
var i:integer;
begin
 MasterPalSet:=false;
 ClearTXList;
 sectors.Clear;
 for i:=0 to scList.Count-1 do scList[i].Free;
 scList.Clear;
end;

Procedure TNewPRenderer.ClearThings;
var i:integer;
begin
 things.Clear;
 for i:=0 to thList.Count-1 do thList[i].Free;
 thList.Clear;
end;

Procedure TNewPRenderer.AddSector(s:TJKSector);
var m:T3DPMesh;
begin
 m:=SecToMesh(s);
 scList.Add(m);
 sectors.Add(s);
end;

Procedure TNewPRenderer.DeleteSector(s:TJKSector);
var i:integer;
begin
 i:=sectors.indexof(s);
 if i<0 then exit;
 scList[i].Free;
 Sectors.Delete(i);
 scList.Delete(i);
end;

Procedure TNewPRenderer.UpdateSector(s:TJKSector);
var i,n:integer;
    m:T3DPMesh;
begin
 i:=sectors.indexof(s);
 if i<0 then exit;
 scList[i].Free;
 m:=SecToMesh(s);
 scList[i]:=m;
end;

Procedure TNewPRenderer.AddThing(th:TJKThing);
var m:T3DPMesh;
begin
 m:=ThingToMesh(th);
 if m=nil then m:=BadThingMesh(th);
 ThList.Add(m);
 Things.Add(th);
end;

Procedure TNewPRenderer.DeleteThing(th:TJKThing);
var i:integer;
begin
 i:=things.indexof(th);
 if i<0 then exit;
 thList[i].Free;
 Things.Delete(i);
 ThList.Delete(i);
end;

Procedure TNewPRenderer.UpdateThing(th:TJKThing);
var i,n:integer;
    m:T3DPMesh;
begin
 i:=Things.indexof(th);
 if i<0 then exit;
 thList[i].Free;
 m:=ThingToMesh(th);
 if m=nil then m:=BadThingMesh(th);
 ThList[i]:=m;
end;

Procedure TNewPRenderer.SetThing3DO(th:TJKThing;a3do:TPJ3DO);
var i,n:integer;
    m:T3DPMesh;
begin
 i:=Things.indexof(th);
 if i<0 then exit;
 thList[i].Free;
 m:=A3DOToMesh(th,a3do);
 if m=nil then m:=BadThingMesh(th);
 ThList[i]:=m;
end;


Procedure TNewPRenderer.Redraw;
var i:integer;
    fp,lp,rp,tp,bp:Tvector; {Plane points}
    fn,ln,rn,tn,bn:TVector; {Plane's D normals}
    ax1,ay1,az1,ax2,ay2,az2:double;
    bx1,by1,bz1,bx2,by2,bz2:double;
    m:T3DPMesh;

Function IsInFront(const p,n:TVector;c:T3DPVertex;rad:single):integer;
var d:double;
begin
 result:=0;
 d:=Smult(c.x-p.x,c.y-p.y,c.z-p.z,n.dx,n.dy,n.dz);
 if d>rad then result:=1 else
 if d>-rad then result:=2 else result:=0;
end;

begin
 DisableFPUExceptions;

 getWorldLine(vwidth div 2,vheight div 2,ax1,ay1,az1,ax2,ay2,az2);
 setVec(fn,ax2-ax1,ay2-ay1,az2-az1);
 {Front plane}
 Normalize(fn);
 SetVec(fp,CamX+FrontPlane*fn.dx,CamY+FrontPlane*fn.dy,CamZ+FrontPlane*fn.dz);

 {Left plane}
 getWorldLine(0,0,ax1,ay1,az1,ax2,ay2,az2);
 getWorldLine(0,vheight,bx1,by1,bz1,bx2,by2,bz2);

 Vmult(bx1-ax1,by1-ay1,bz1-az1,
       ax2-ax1,ay2-ay1,az2-az1,
       ln.dx,ln.dy,ln.dz);
  Normalize(ln);
  SetVec(lp,ax1,ay1,az1);

 {Right plane}
 getWorldLine(vwidth,0,ax1,ay1,az1,ax2,ay2,az2);
 getWorldLine(vwidth,vheight,bx1,by1,bz1,bx2,by2,bz2);

 Vmult(ax2-ax1,ay2-ay1,az2-az1,
       bx1-ax1,by1-ay1,bz1-az1,
       rn.dx,rn.dy,rn.dz);
  Normalize(rn);
  SetVec(rp,ax1,ay1,az1);

 {Top plane}
 getWorldLine(0,0,ax1,ay1,az1,ax2,ay2,az2);
 getWorldLine(vwidth,0,bx1,by1,bz1,bx2,by2,bz2);

 Vmult(ax2-ax1,ay2-ay1,az2-az1,
       bx1-ax1,by1-ay1,bz1-az1,
       tn.dx,tn.dy,tn.dz);
  Normalize(tn);
  SetVec(tp,ax1,ay1,az1);

 {Bottom plane}
 getWorldLine(0,vheight,ax1,ay1,az1,ax2,ay2,az2);
 getWorldLine(vwidth,vheight,bx1,by1,bz1,bx2,by2,bz2);

 Vmult(bx1-ax1,by1-ay1,bz1-az1,
       ax2-ax1,ay2-ay1,az2-az1,
       bn.dx,bn.dy,bn.dz);
  Normalize(bn);
  SetVec(bp,ax1,ay1,az1);


 for i:=0 to sclist.count-1 do
 begin
  m:=sclist[i];
  if m=nil then continue;
  if (isInFront(fp,fn,m.center,m.radius)=0) or
     (isInFront(lp,ln,m.center,m.radius)=0) or
     (isInFront(rp,rn,m.center,m.radius)=0) or
     (isInFront(tp,tn,m.center,m.radius)=0) or
     (isInFront(bp,bn,m.center,m.radius)=0) then continue;
  DrawMesh(m);
 end;

 for i:=0 to thlist.count-1 do
 begin
  m:=thlist[i];
  if m=nil then continue;
  if (isInFront(fp,fn,m.center,m.radius)=0) or
     (isInFront(lp,ln,m.center,m.radius)=0) or
     (isInFront(rp,rn,m.center,m.radius)=0) or
     (isInFront(tp,tn,m.center,m.radius)=0) or
     (isInFront(bp,bn,m.center,m.radius)=0) then continue;
  DrawMesh(m);
 end;


end;


Function IsPointOn3DPSurf(surf:T3DPSurf;x,y,z:double):boolean;
var
    vc,i:integer;
    v1,v2:TVXDets;
    dist:double;
    vct:TVector;
    x1,x2,y1,y2,z1,z2:double;

begin
 Result:=false;

  vc:=Surf.vxds.Count;

 For i:=0 to vc-1 do
 begin
  v1:=Surf.GetVXD(i);
  v2:=Surf.GetVXD(NextIdx(i,vc));
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



Function TNewPRenderer.PickAt(X,Y:integer):TPickType;
var
    ax1,ay1,az1,ax2,ay2,az2:double;
    i,j,n:integer;
    cdist:double;
    m:T3DPMesh;
    lvec:TVector;

Function GetIntersectingSurf(m:T3DPMesh;var cdist:double):Integer;
var
    a,b,c2:double;
    ax,ay,az:double;
    cv:TVector;
    i:integer;
    s:T3DPSurf;
    vxd:TVXDets;
    dist:double;
begin
 result:=-1;
 if m=nil then exit;
 SetVec(cv,m.center.x-ax1,m.center.y-ay1,m.center.z-az1);
 c2:=sqr(cv.dx)+sqr(cv.dy)+sqr(cv.dz);
 a:=SMult(lvec.dx,lvec.dy,lvec.dz,cv.dx,cv.dy,cv.dz);

 if a<(-m.radius) then exit; {If behind the plane}

 b:=sqrt(c2-sqr(a));
 if b>m.radius then exit; {if look line doesn't intersect bounding sphere}

 for i:=0 to m.surfs.count-1 do
 begin
  s:=m.getSurf(i);

  if SMult(s.normal.dx,s.normal.dy,s.normal.dz,lvec.dx,lvec.dy,lvec.dz)>0 then continue;

  if s.vxds.count=0 then continue;
  vxd:=s.getVXD(0);
  if Not PlaneLineXnNew(s.normal,vxd.X,vxd.y,vxd.z,ax1,ay1,az1,ax2,ay2,az2,ax,ay,az)
   then continue;

   {Debug version}
{   if not IsPointOn3DPSurf(s,ax,ay,az) then continue;
   dist:=SMult(ax-ax1,ay-ay1,az-az1,lvec.dx,lvec.dy,lvec.dz);
  if dist<0 then continue;
  if dist>cdist then continue;}



   dist:=SMult(ax-ax1,ay-ay1,az-az1,lvec.dx,lvec.dy,lvec.dz);
  if dist<0 then continue;
  if dist>cdist then continue;
  if not IsPointOn3DPSurf(s,ax,ay,az) then continue;


  cdist:=dist;
  result:=i;
 end
end;

begin
 cdist:=1E100;
 result:=pk_nothing;

 getWorldLine(0,0,ax1,ay1,az1,ax2,ay2,az2);

 getWorldLine(X,Y,ax1,ay1,az1,ax2,ay2,az2);
 SetVec(lvec,ax2-ax1,ay2-ay1,az2-az1);
 Normalize(Lvec);

 for i:=0 to sclist.count-1 do
 begin
  m:=sclist[i];
  n:=GetIntersectingSurf(m,cdist);
  if n=-1 then continue;
  result:=pk_surface;
  selSC:=Sectors[i];
  selSF:=n;
  for j:=0 to selSC.Surfaces.count-1 do
   if selSC.surfaces[j].D3DID=n then
   begin
    SelSF:=j;
    break;
   end;
 end;

 for i:=0 to thlist.count-1 do
 begin
  m:=thlist[i];
  n:=GetIntersectingSurf(m,cdist);
  if n=-1 then continue;
  result:=pk_thing;
  selTH:=Things[i];
 end;

end;


Function TNewPRenderer.GetTexture(const name,cmp:string):T3DPTexture;
var i:integer;
    Ttx:T3DPTexture;
    pcmp:^TCMPTable;
    tpal:TCMPPal;
    f:TFile;
begin
 Result:=nil;
 i:=TXlist.IndexOf(name+cmp);
 if i<>-1 then
 begin
  Result:=T3DPTexture(TXList.Objects[i]);
  exit;
 end;

 i:=CmpList.IndexOf(cmp);
 if i<>-1 then pcmp:=Pointer(CmpList.Objects[i]) else
 begin
  GetMem(pcmp,sizeof(pcmp^));
  if not LoadCmpTable(cmp,pcmp^) then PanMessage(mt_warning,Format('Cannot load %s',[cmp]));
  CmpList.AddObject(cmp,TObject(pcmp));
 end;

 LoadMasterPal(level);
 tpal:=MasterPal;
 AdjustPalGamma(tpal,gamma);

 ttx:=nil;
 try
  ttx:=LoadTexture(name,tpal,pcmp^);
{  TGlideTexture.CreateFromMat(name,tpal,GenTexture);}
  if ttx<>nil then ttx.cmpname:=cmp;
 finally
  TXList.AddObject(name+cmp,ttx);
  Result:=ttx;
 end;
end;


Procedure TNewPRenderer.LoadMasterPal(l:TJKLevel);
var
 i:integer;
 pal:array[0..255] of
 record
  r,g,b,a:byte;
 end;

begin
 if MasterPalSet then exit;
 GetLevelPal(l,MasterPal);

 for i:=0 to 255 do
 With Pal[i] do
 begin
  r:=MasterPal[i].r;
  g:=MasterPal[i].g;
  b:=MasterPal[i].b;
  a:=0;
 end;

{ glEnable(GL_COLOR_INDEX8_EXT);

 pPalProc(GL_TEXTURE_2D, GL_RGB8, sizeof(Pal),
	  GL_RGBA, GL_UNSIGNED_BYTE, pal );}

 MasterPalSet:=true;
end;

Function L2B(c:single):byte;
begin
 if c<0 then result:=0
 else if c>1 then result:=255
 else result:=Round(255*c);
end;


Function TNewPRenderer.SecToMesh(s:TJKSector):T3DPMesh;
var i,j:integer;
    nv,nvx:integer;
    ttx:T3DPTexture;
    surf:T3DPSurf;
    vd:TVXDets;
    elt:single;
begin
 DisableFPUExceptions;
 Result:=T3DPMesh.Create;
 {getMem(vxs,sizeof(T3DPVertex)*s.vertices.count);}


 for i:=0 to s.surfaces.count-1 do
 With s.surfaces[i] do
 begin
  if (s.Flags and SF_3DO=0)
  then if (geo=0) or (Material='') then begin D3DID:=-1; continue; end;

  surf:=Result.AddSurf;
  surf.normal:=normal;
  if s.Flags and SF_3DO<>0 then SetVec(surf.normal,-normal.dx,-normal.dy,-normal.dz);

  nvx:=Vertices.Count;
  if nvx>24 then nvx:=24;

  D3DID:=Result.surfs.count-1;
  ttx:=nil;
  Try
   ttx:=GetTexture(Material,s.ColorMap);
   surf.tx:=ttx;
   {if ttx<>nil then ttx.SetCurrent;}
  except
   on E:Exception do PanMessage(mt_warning,
      Format('Cannot load %s for sector %d surface %d: %s',[Material,s.num,i,e.message]));
  end;

 nvx:=Vertices.Count;
 if nvx>24 then nvx:=24;

 for j:=0 to nvx-1 do
 begin
  if s.Flags and SF_3DO=0 then nv:=j
  else nv:=nvx-j-1;

 With TXVertices[nv] do
 begin
  vd:=surf.AddVXD;
  if (P3DFullLit) or (s.Flags and SF_3DO<>0) or (SurfFlags and (SFF_SKY or SFF_SKY1)<>0)
  then begin vd.r:=255; vd.g:=255; vd.b:=255; end else
  case s.level.mots and P3DColoredLights of
   true: begin
          elt:=s.extra+extralight;
          vd.r:=L2B(r+elt);
          vd.g:=L2B(g+elt);
          vd.b:=L2B(b+elt);
         end;
   false: begin
{          Try}
           elt:=Intensity+s.extra+extralight;
           vd.r:=L2B(elt);
           vd.g:=vd.r; vd.b:=vd.r;
{          except
           on Exception do begin vd.r:=0; vd.g:=0; vd.b:=0; end;
          end;}
          end;
  end;
  if ttx<>nil then begin vd.u:=u/ttx.width; vd.v:=v/ttx.height; end;
  vd.x:=Vertices[nv].x;
  vd.y:=Vertices[nv].y;
  vd.z:=Vertices[nv].z;
 end;
 end;
end;
 result.CalculateSphere;
end;

Procedure TNewPRenderer.HandleActivate(const msg:TMessage);
begin
end;

procedure TNewPRenderer.HandlePaint(hdc:integer);
begin
end;

Function TNewPRenderer.BadThingMesh(th:TJKThing):T3DPMesh;
var
    surf:T3DPSurf;
    vd:TVXDets;
begin
 Result:=T3DPMesh.Create;
 surf:=Result.AddSurf;
 SetVec(surf.normal,0,1,0);
 surf.tx:=nil;
 vd:=Surf.AddVXD;vd.r:=255;vd.g:=255;vd.b:=255;
 vd.z:=th.z+0.05;
 vd.y:=th.y;
 vd.x:=th.x+0.05;
 vd:=Surf.AddVXD;vd.r:=255;vd.g:=255;vd.b:=255;
 vd.z:=th.z-0.05;
 vd.y:=th.y;
 vd.x:=th.x+0.05;
 vd:=Surf.AddVXD;vd.r:=255;vd.g:=255;vd.b:=255;
 vd.z:=th.z-0.05;
 vd.y:=th.y;
 vd.x:=th.x-0.05;
 vd:=Surf.AddVXD;vd.r:=255;vd.g:=255;vd.b:=255;
 vd.z:=th.z+0.05;
 vd.y:=th.y;
 vd.x:=th.x-0.05;

 surf:=Result.AddSurf;
 SetVec(surf.normal,0,-1,0);
 surf.tx:=nil;
 vd:=Surf.AddVXD;vd.r:=255;vd.g:=255;vd.b:=255;
 vd.z:=th.z+0.05;
 vd.y:=th.y;
 vd.x:=th.x-0.05;
 vd:=Surf.AddVXD;vd.r:=255;vd.g:=255;vd.b:=255;
 vd.z:=th.z-0.05;
 vd.y:=th.y;
 vd.x:=th.x-0.05;
 vd:=Surf.AddVXD;vd.r:=255;vd.g:=255;vd.b:=255;
 vd.z:=th.z-0.05;
 vd.y:=th.y;
 vd.x:=th.x+0.05;
 vd:=Surf.AddVXD;vd.r:=255;vd.g:=255;vd.b:=255;
 vd.z:=th.z+0.05;
 vd.y:=th.y;
 vd.x:=th.x+0.05;


 result.CalculateSphere;
end;

Function TNewPRenderer.ThingToMesh(th:TJKThing):T3DPMesh;
var i,j,k:integer;
    nv,nvx:integer;
    ttx:T3DPTexture;
    cmp,mat:string;
    ax,ay,az:single;
    mx:TMat3x3s;
    surf:T3DPSurf;
    vd:TVXDets;
begin
 result:=nil;
 if th.a3DO=nil then exit;
 Result:=T3DPMesh.Create;

 With th do CreateRotMatrixs(mx,pch,yaw,rol);

 for i:=0 to th.a3DO.Meshes.count-1 do
 With th.a3Do.Meshes[i] do
 for k:=0 to faces.count-1 do
 With Faces[k] do
 begin
  mat:=th.A3DO.GetMat(imat);
  if (geo=0) or (Mat='') then continue;

  surf:=Result.AddSurf;
  CalcNormal;
  surf.normal:=normal;
  ttx:=nil;

Try
 ttx:=GetTexture(Mat,thing_cmp);
 surf.tx:=ttx;
{ if ttx<>nil then ttx.SetCurrent;}
except
 on Exception do PanMessage(mt_warning,
    Format('Cannot load %s for thing %d',[Mat,th.num]));
end;


{ pim:=ttx.Itexture.GetImage;
 if pim=nil then;}

 nvx:=Vertices.Count;
 if nvx>24 then nvx:=24;

 for j:=0 to nvx-1 do
 With TXVertices[j] do
 begin
   vd:=Surf.AddVXD;

   ax:=Vertices[j].x;
   ay:=Vertices[j].y;
   az:=Vertices[j].z;

   MultVM3s(mx,ax,ay,az);

   vd.x:=th.x+ax;
   vd.y:=th.y+ay;
   vd.z:=th.z+az;


   vd.r:=255; vd.g:=255; vd.b:=255;

  if ttx<>nil then
  begin
   vd.u:=u/ttx.width;
   vd.v:=v/ttx.height;
  end;

 { glVertex3f(th.x+ax,th.y+ay,th.z+az);}

 end;

 end;

 result.CalculateSphere;

end;

Function TNewPRenderer.A3DOToMesh(th:TJKThing;a3do:TPJ3DO):T3DPMesh;
var i,j,k:integer;
    nv,nvx:integer;
    ttx:T3DPTexture;
    cmp,mat:string;
    ax,ay,az:single;
    mx:TMat3x3s;
    surf:T3DPSurf;
    vd:TVXDets;
begin
 result:=nil;
 if (a3do=nil) then exit;
 Result:=T3DPMesh.Create;

 With th do CreateRotMatrixs(mx,pch,yaw,rol);

 for i:=0 to a3DO.Meshes.count-1 do
 With a3Do.Meshes[i] do
 for k:=0 to faces.count-1 do
 With Faces[k] do
 begin
  mat:=A3DO.GetMat(imat);
  if (geo=0) or (Mat='') then continue;

  surf:=Result.AddSurf;
  CalcNormal;
  surf.normal.dx:=normal.dx;
  surf.normal.dy:=normal.dy;
  surf.normal.dz:=normal.dz;
  ttx:=nil;

Try
 ttx:=GetTexture(Mat,thing_cmp);
 surf.tx:=ttx;
{ if ttx<>nil then ttx.SetCurrent;}
except
 on Exception do PanMessage(mt_warning,
    Format('Cannot load %s for thing %d',[Mat,th.num]));
end;


{ pim:=ttx.Itexture.GetImage;
 if pim=nil then;}

 nvx:=Vertices.Count;
 if nvx>24 then nvx:=24;

 for j:=0 to nvx-1 do
 With TXVertices[j] do
 begin
   vd:=Surf.AddVXD;

   ax:=Vertices[j].tx;
   ay:=Vertices[j].ty;
   az:=Vertices[j].tz;

   MultVM3s(mx,ax,ay,az);

   vd.x:=th.x+ax;
   vd.y:=th.y+ay;
   vd.z:=th.z+az;


   vd.r:=255; vd.g:=255; vd.b:=255;

  if ttx<>nil then
  begin
   vd.u:=u/ttx.width;
   vd.v:=v/ttx.height;
  end;

 { glVertex3f(th.x+ax,th.y+ay,th.z+az);}

 end;

 end;

 result.CalculateSphere;

end;

Type
    TVXData=record
     x,y,z:single;
     infront:boolean;
    end;

Constructor T3DPSurf.Create;
begin
 vxds:=TList.create;
end;

Destructor T3DPSurf.Destroy;
var i:integer;
begin
 for i:=0 to vxds.count-1 do GetVXD(i).Free;
 vxds.Free;
end;

Function T3DPSurf.AddVXD:TVXDets;
begin
 Result:=TVXDets.Create;
 vxds.Add(Result);
end;

Function T3DPSurf.GetVXD(n:integer):TVXDets;
begin
 Result:=TVXDets(vxds.List[n]);
end;

Constructor T3DPMesh.Create;
begin
 Surfs:=TList.Create;
end;

Destructor T3DPMesh.Destroy;
var i:integer;
begin
 for i:=0 to surfs.count-1 do GetSurf(i).Free;
 Surfs.Free;
end;

Function T3DPMesh.AddSurf:T3DPSurf;
begin
 Result:=T3DPSurf.Create;
 surfs.add(Result);
end;


Function T3DPMesh.GetSurf(n:integer):T3DPSurf;
begin
 Result:=T3DPSurf(surfs.List[n]);
end;

Procedure T3DPMesh.CalculateSphere;
var i,j:integer;
    x1,y1,z1,x2,y2,z2:single;
    surf:T3DPSurf;
    vxd:TVXDets;
    isempty:boolean;
begin
 center.x:=0;
 center.y:=0;
 center.z:=0;
 radius:=0;
 isempty:=true;

 for i:=0 to surfs.count-1 do
 begin
  surf:=GetSurf(i);
  for j:=0 to surf.vxds.count-1 do
  begin
   vxd:=surf.GetVXD(j);
   if isempty then
   begin
    x1:=vxd.x;x2:=vxd.x;
    y1:=vxd.y;y2:=vxd.y;
    z1:=vxd.z;z2:=vxd.z;
    isempty:=false;
    continue;
   end;
   if vxd.x<x1 then x1:=vxd.x;
   if vxd.x>x2 then x2:=vxd.x;
   if vxd.y<y1 then y1:=vxd.y;
   if vxd.y>y2 then y2:=vxd.y;
   if vxd.z<z1 then z1:=vxd.z;
   if vxd.z>z2 then z2:=vxd.z;
  end;
 end;

 if isempty then exit;

 center.x:=x1+(x2-x1)/2;
 center.y:=y1+(y2-y1)/2;
 center.z:=z1+(z2-z1)/2;

 for i:=0 to surfs.count-1 do
 begin
  surf:=GetSurf(i);
  for j:=0 to surf.vxds.count-1 do
   begin
    vxd:=surf.GetVXD(j);
    x1:=sqr(vxd.x-center.x)+sqr(vxd.y-center.y)+sqr(vxd.z-center.z);
    if x1>radius then radius:=x1;
   end;
 end;


 radius:=sqrt(radius);
end;


Function TMeshes.GetItem(n:integer):T3DPMesh;
begin
 if (n<0) or (n>=count) then raise EListError.CreateFmt('Sector Index is out of bounds: %d',[n]);
 Result:=T3DPMesh(List[n]);
end;

Procedure TMeshes.SetItem(n:integer;v:T3DPMesh);
begin
 if (n<0) or (n>=count) then raise EListError.CreateFmt('Sector Index is out of bounds: %d',[n]);
 List[n]:=v;
end;



end.
