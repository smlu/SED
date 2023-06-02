unit tdfx_PRender;

interface
uses D3d_Prender, Classes, J_Level, Forms,
     Messages,Windows, files, FileOperations, graph_files,
     lev_utils, sysUtils, misc_utils, GlobalVars, geometry, Graphics;

Const
     FrontPlane=0.05;

type

TGlideTexture=class
 width,height:integer;
 cmpname:string;
 Constructor CreateFromMat(const Mat:string;const Pal:TCMPPal;txid:integer);
 Procedure SetCurrent;
 Destructor Destroy;override;
end;

TVXdets=class
 vx:TVertex;
 u,v,r,g,b:single;
end;

TSurf=class
 vxds:TList;
 tx:TGlideTexture;
 Constructor Create;
 Destructor Destroy;
 Function AddVXD:TVXDets;
 Function GetVXD(n:integer):TVXDets;
end;

TMesh=class
 surfs:TList;
 Constructor Create;
 Destructor Destroy;
 Function GetSurf(n:integer):TSurf;
 Function AddSurf:TSurf;
end;

TMeshes=class(TList)
 Function GetItem(n:integer):TMesh;
 Procedure SetItem(n:integer;v:TMesh);
Property Items[n:integer]:TMesh read GetItem write SetItem; default;
end;


T3DFXPRenderer=class(TPreviewRender)
   TXList,CmpList:TStringList;
   sectors:TSectors;
   things:TThings;
   masterPalSet:boolean;
   MasterPal:TCMPPal;

   ThList,SCList:TMeshes;

   mx:TMat3x3s;
   dx,dy,dz:single;

   wcx,wcy:integer;
   ww,wh:integer;
   halfpsize:integer;
   Dc:TCanvas;
   viewer:TForm;
   scale:single;

   tn,bn,ln,rn:TVector; {Normals of clipping planes}

 Constructor Create(aForm:TForm);
 {Constructor CreateFromPanel(aPanel:TPanel);}
 Procedure Initialize;override;
 Procedure ClearSectors;override;
 Procedure ClearThings;override;
 Procedure AddSector(s:TJKSector);override;
 Procedure DeleteSector(s:TJKSector);override;
 Procedure UpdateSector(s:TJKSector);override;
 Procedure AddThing(th:TJKThing);override;
 Procedure DeleteThing(th:TJKThing);override;
 Procedure UpdateThing(th:TJKThing);override;
 Procedure SetViewToThing(th:TJKThing);override;

 Procedure SetViewPort(x,y,w,h:integer);override;
 Procedure Redraw;override;
 Destructor Destroy;override;
 Procedure HandleActivate(const msg:TMessage);override;
 procedure HandlePaint(hdc:integer);override;
 Function PickAt(X,Y:integer):TPickType;override;

 Procedure SetGamma(gamma:double);override;
Private
 lastTx:integer;
 Function GenTexture:integer;
 Procedure ClearTXList;
 Function LoadOGLTexture(const name,cmp:string):TGlideTexture;
 Procedure LoadMasterPal(l:TJKLevel);
 Function SecToMesh(s:TJKSector):TMesh;
 Function ThingToMesh(th:TJKThing):TMesh;
 Procedure DrawMesh(m:TMesh);
end;

implementation

procedure DisableFPUExceptions ;
var
  FPUControlWord: WORD ;
asm
  FSTCW   FPUControlWord ;
  OR      FPUControlWord, $4 + $1 ; { Divide by zero + invalid operation }
  FLDCW   FPUControlWord ;
end ;

Constructor T3DFXPRenderer.Create(aForm:TForm);
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
 dc:=aForm.Canvas;
 dc.Brush.Color:=0;
 dc.Pen.Color:=clWhite;
 ww:=aForm.ClientWidth;
 wh:=aForm.ClientHeight;
 wcx:=ww div 2;
 wcy:=wcy div 2;
 viewer:=aForm;
end;

Destructor T3DFXPRenderer.Destroy;
begin
 ClearTXList;

 TxList.Free;
 CmpList.Free;
 Sectors.Free;
 things.Free;
 inherited Destroy;
end;

Procedure T3DFXPRenderer.Initialize;
begin

end;

Procedure T3DFXPRenderer.ClearTXList;
var i:integer;
begin
 for i:=0 to TXList.Count-1 do TXList.Objects[i].Free;
 TXList.Clear;
 LastTX:=0;
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

Procedure T3DFXPRenderer.SetGamma(gamma:double);
var
   i,j,ic:integer;
   pcmp:^TCMPPal;
   cmp:TCMPPal;
   f:tfile;
   gmult:double;
begin
 self.gamma:=gamma;
 if TXList.Count<>0 then
  PanMessage(mt_warning,'Gamma change will take effect after you reload 3D preview');
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

Procedure T3DFXPRenderer.ClearSectors;
var i:integer;
begin
 MasterPalSet:=false;
 ClearTXList;
 sectors.Clear;
 for i:=0 to scList.Count-1 do scList[i].Free;
 scList.Clear;
end;

Procedure T3DFXPRenderer.ClearThings;
var i:integer;
begin
 things.Clear;
 for i:=0 to thList.Count-1 do thList[i].Free;
 thList.Clear;
end;

Procedure T3DFXPRenderer.AddSector(s:TJKSector);
var m:TMesh;
begin
 m:=SecToMesh(s);
 scList.Add(m);
 sectors.Add(s);
end;

Procedure T3DFXPRenderer.DeleteSector(s:TJKSector);
var i:integer;
begin
 i:=sectors.indexof(s);
 if i<0 then exit;
 scList[i].Free;
 Sectors.Delete(i);
 scList.Delete(i);
end;

Procedure T3DFXPRenderer.UpdateSector(s:TJKSector);
var i,n:integer;
    m:TMesh;
begin
 i:=sectors.indexof(s);
 if i<0 then exit;
 scList[i].Free;
 m:=SecToMesh(s);
 scList[i]:=m;
end;

Procedure T3DFXPRenderer.AddThing(th:TJKThing);
var m:TMesh;
begin
 m:=ThingToMesh(th);
 ThList.Add(m);
 Things.Add(th);
end;

Procedure T3DFXPRenderer.DeleteThing(th:TJKThing);
var i:integer;
begin
 i:=things.indexof(th);
 if i<0 then exit;
 thList[i].Free;
 Things.Delete(i);
 ThList.Delete(i);
end;

Procedure T3DFXPRenderer.UpdateThing(th:TJKThing);
var i,n:integer;
    m:TMesh;
begin
 i:=Things.indexof(th);
 if i<0 then exit;
 thList[i].Free;
 m:=ThingToMesh(th);
 ThList[i]:=m;
end;

Procedure T3DFXPRenderer.SetViewToThing(th:TJKThing);
begin
end;


Procedure T3DFXPRenderer.SetViewPort(x,y,w,h:integer);
begin
end;

Procedure T3DFXPRenderer.Redraw;
var i:integer;
    v1,v2,v3:Tvector;
    tmx:TMat3x3s;
begin
 DisableFPUExceptions;

 DC.Rectangle(0,0,ww,wh);

 scale:=ww/640*320;

 CreateRotMatrixs(mx,-90,0,0);

 CreateRotMatrixs(tmx,Pch,Yaw,0);
 MultM3s(mx,tmx);
 {ScaleMatrixS(mx,scale*10,scale,scale*10);}
{ dx:=scale*CamX;
 dy:=scale*CamY;
 dz:=scale*CamZ;}
 dx:=CamX;
 dy:=CamY;
 dz:=CamZ;

 for i:=0 to sclist.count-1 do
  DrawMesh(sclist[i]);

 for i:=0 to thlist.count-1 do
  DrawMesh(thlist[i]);

end;

Procedure T3DFXPRenderer.HandleActivate(const msg:TMessage);
begin
end;

procedure T3DFXPRenderer.HandlePaint(hdc:integer);
begin
end;

Function T3DFXPRenderer.PickAt(X,Y:integer):TPickType;
begin
end;

Function T3DFXPRenderer.LoadOGLTexture(const name,cmp:string):TGlideTexture;
var i:integer;
    Ttx:TGlideTexture;
    pcmp:^TCMPPal;
    tpal:TCMPPal;
    f:TFile;
begin
 Result:=nil;
 i:=TXlist.IndexOf(name+cmp);
 if i<>-1 then
 begin
  Result:=TGlideTexture(TXList.Objects[i]);
  exit;
 end;

 i:=CmpList.IndexOf(cmp);
 if i<>-1 then pcmp:=Pointer(CmpList.Objects[i]) else
 begin
  GetMem(pcmp,sizeof(pcmp^));
  GetLevelPal(Level,pcmp^);
  if not ApplyCMP(cmp,pcmp^) then PanMessage(mt_warning,Format('Cannot load %s',[cmp]));
  CmpList.AddObject(cmp,TObject(pcmp));
 end;

{ LoadMasterPal(level);}
  tpal:=pcmp^;
  AdjustPalGamma(tpal,gamma);

 ttx:=nil;
 try
  ttx:=TGlideTexture.CreateFromMat(name,tpal,GenTexture);
  ttx.cmpname:=cmp;
 finally
  TXList.AddObject(name+cmp,ttx);
  Result:=ttx;
 end;
end;


Procedure T3DFXPRenderer.LoadMasterPal(l:TJKLevel);
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

Function T3DFXPRenderer.SecToMesh(s:TJKSector):TMesh;
var i,j:integer;
    nv,nvx:integer;
    ttx:TGlideTexture;
    surf:TSurf;
    vd:TVXDets;
begin
 Result:=TMesh.Create;
 for i:=0 to s.surfaces.count-1 do
 With s.surfaces[i] do
 begin
  if (s.Flags and SF_3DO=0)
  then if (geo=0) or (Material='') then begin D3DID:=-1; continue; end;

  surf:=Result.AddSurf;

  nvx:=Vertices.Count;
  if nvx>24 then nvx:=24;

  D3DID:=i;
  ttx:=nil;
  Try
{   ttx:=LoadOGLTexture(Material,s.ColorMap);}
   surf.tx:=ttx;
   {if ttx<>nil then ttx.SetCurrent;}
  except
   on Exception do PanMessage(mt_warning,
      Format('Cannot load %s for sector %d surface %d',[Material,s.num,i]));
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
  then begin vd.r:=1; vd.g:=1; vd.b:=1; end else
  case s.level.mots and P3DColoredLights of
   true: begin vd.r:=r+s.extra+extralight; vd.g:=g+s.extra+extralight; vd.b:=b+s.extra+extralight; end;
   false: begin vd.r:=Intensity+s.extra+extralight; vd.g:=Intensity+s.extra+extralight; vd.b:=Intensity+s.extra+extralight; end;
  end;
  if ttx<>nil then begin vd.u:=u/ttx.width; vd.v:=v/ttx.height; end;
  vd.vx:=Vertices[nv];
 end;
 end;
end;
end;

Function T3DFXPRenderer.ThingToMesh(th:TJKThing):TMesh;
var i,j,k:integer;
    nv,nvx:integer;
    ttx:TGlideTexture;
    cmp,mat:string;
    ax,ay,az:double;
    mx:TMat3x3;
    surf:TSurf;
    vd:TVXDets;
begin
 result:=nil;
 if th.a3DO=nil then exit;
 Result:=TMesh.Create;

 With th do CreateRotMatrix(mx,pch,yaw,rol);

 for i:=0 to th.a3DO.Meshes.count-1 do
 With th.a3Do.Meshes[i] do
 for k:=0 to faces.count-1 do
 With Faces[k] do
 begin
  mat:=th.A3DO.GetMat(imat);
  if (geo=0) or (Mat='') then continue;

  surf:=Result.AddSurf;
  ttx:=nil;

Try
 ttx:=LoadOGLTexture(Mat,'dflt.cmp');
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
   vd.vx:=Vertices[j];
{   ax:=x;
   MultVM3(mx,ax,ay,az);}

   vd.r:=1; vd.g:=1; vd.b:=1;

  if ttx<>nil then
  begin
   vd.u:=u/ttx.width;
   vd.v:=v/ttx.height;
  end;

 { glVertex3f(th.x+ax,th.y+ay,th.z+az);}

 end;

 end;
end;

Type
    TVXData=record
     x,y,z:single;
     infront:boolean;
    end;

Procedure T3DFXPRenderer.DrawMesh(m:TMesh);
var i,j:integer;
    nvx:integer;

{Poly vars}
    poly:array[0..23] of record scx,scy:single; end;
    vfrom,vto,v0:TVXData;

    pvxs:integer;
    firstvx:boolean;

Procedure StartPoly;
begin
 pvxs:=0;
 firstvx:=true;
end;

Procedure FillVXData(var vd:TVXData;x,y,z:single);
begin
 vd.infront:=z>=FrontPlane;
 vd.x:=x;
 vd.y:=y;
 vd.z:=z;
end;

Procedure AddToPoly(x,y,z:single);
begin
 if pvxs>=24 then exit;
 With Poly[pvxs] do
 begin
  scx:=x/z*160+wcx;
  scy:=wh-y/z*160+wcy;
 end;
 inc(pvxs);
end;

Procedure AddClippedVX(const vfrom,vto:TVXData);
var lx,ly,lz,ldx,ldy,ldz:single;
begin
 if vfrom.infront=vto.infront then exit;
 ldx:=vto.x-vfrom.x;
 ldy:=vto.y-vfrom.y;
 ldz:=vto.z-vfrom.z;
 lx:=vfrom.x+ldx/ldz*(frontPlane-vfrom.z);
 ly:=vfrom.y+ldy/ldz*(frontPlane-vfrom.z);
 AddToPoly(lx,ly,frontPlane);
end;

Procedure AddVX(ax,ay,az:single);
begin
  ax:=ax+dx;
  ay:=ay+dy;
  az:=az+dz;
  MultVM3s(mx,ax,ay,az);
  if firstvx then
  begin
   FillVXData(vto,ax,ay,az);
   v0:=vto;
   if vto.infront then AddToPoly(ax,ay,az);
   firstvx:=false;
   exit;
  end;
  vfrom:=vto;
  FillVXData(vto,ax,ay,az);
  AddClippedVX(vfrom,vto);
  if vto.infront then AddToPoly(ax,ay,az)
end;

Procedure EndPoly;
var i:integer;
begin
 if pvxs<2 then exit;
 AddClippedVX(vto,v0);
 With Poly[0] do DC.MoveTo(Round(scx),Round(scy));
 for i:=1 to pvxs-1 do
 With Poly[i] do
  DC.LineTo(Round(scx),Round(scy));
 With Poly[0] do DC.LineTo(Round(scx),Round(scy));
end;

begin
for i:=0 to m.Surfs.count-1 do
With m.GetSurf(i) do
begin
 nvx:=vxds.Count;
 if tx<>nil then tx.SetCurrent;

 StartPoly;
 for j:=0 to nvx-1 do
 With GetVXD(j) do AddVX(vx.x,vx.y,vx.z);
 EndPoly;

end;

end;


Function T3DFXPRenderer.GenTexture:integer;
begin
 inc(lastTx);
 Result:=lastTX;
end;

Constructor TGlideTexture.CreateFromMat(const Mat:string;const Pal:TCMPPal;txid:integer);
var
    i,j:integer;
    pb:pchar;
    mf:TMat;
    f:TFile;
    pl,pline:pchar;
    n:integer;
    bits:Pchar;

begin
 f:=OpenGameFile(mat);
 mf:=TMat.Create(f);

 width:=mf.info.width;
 height:=mf.info.Height;
 GetMem(pb,width*height*3);
 bits:=pb;

 GetMem(pl,width);

 for i:=0 to height-1 do
 begin
  mf.GetLine(pb^);
  inc(pb,width);
 end;

{ for i:=0 to height-1 do
 begin
  pline:=pl;
  mf.GetLine(pline^);
  for j:=0 to width-1 do
  begin
   n:=ord(pline^);
   With Pal[n] do
   begin
    pb^:=chr(r);
    (pb+1)^:=chr(g);
    (pb+2)^:=chr(b);
   end;
   inc(pb,3);
   inc(pline);
  end;

 end;   }
 FreeMem(pl);
 mf.free;


 FreeMem(bits);

end;


Procedure TGlideTexture.SetCurrent;
begin
end;

Destructor TGlideTexture.Destroy;
begin
end;


Constructor TSurf.Create;
begin
 vxds:=TList.create;
end;

Destructor TSurf.Destroy;
var i:integer;
begin
 for i:=0 to vxds.count-1 do GetVXD(i).Free;
 vxds.Free;
end;

Function TSurf.AddVXD:TVXDets;
begin
 Result:=TVXDets.Create;
 vxds.Add(Result);
end;

Function TSurf.GetVXD(n:integer):TVXDets;
begin
 Result:=TVXDets(vxds.List[n]);
end;

Constructor TMesh.Create;
begin
 Surfs:=TList.Create;
end;

Destructor TMesh.Destroy;
var i:integer;
begin
 for i:=0 to surfs.count-1 do GetSurf(i).Free;
 Surfs.Free;
end;

Function TMesh.AddSurf:TSurf;
begin
 Result:=TSurf.Create;
 surfs.add(Result);
end;


Function TMesh.GetSurf(n:integer):TSurf;
begin
 Result:=TSurf(surfs.List[n]);
end;

Function TMeshes.GetItem(n:integer):TMesh;
begin
 if (n<0) or (n>=count) then raise EListError.CreateFmt('Sector Index is out of bounds: %d',[n]);
 Result:=TMesh(List[n]);
end;

Procedure TMeshes.SetItem(n:integer;v:TMesh);
begin
 if (n<0) or (n>=count) then raise EListError.CreateFmt('Sector Index is out of bounds: %d',[n]);
 List[n]:=v;
end;


end.
