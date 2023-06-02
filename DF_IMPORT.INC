const
   jkdffactor:double=35;

   {DF->JK
    x=x
    y=z;
    z=-y}

Procedure DFtoJK(dfx,dfy,dfz:double;var jkx,jky,jkz:double);
begin
 jkx:=dfx/jkdffactor;
 jky:=dfz/jkdffactor;
 jkz:=-dfy/jkdffactor;
end;

Procedure SetSecLight(s:TJKSector;l:integer);
var i,j:integer;
begin
 s.ambient:=l/31;
 for i:=0 to s.surfaces.count-1 do
 With S.Surfaces[i] do
 for j:=0 to txvertices.count-1 do
 With txVertices[j] do
 begin
  Intensity:=l/31;
  r:=Intensity;
  g:=Intensity;
  b:=Intensity;
 end;
end;

type
TDFWall=class
  v1,v2:integer;
  light:byte;
  iMID,iBOT,iTOP:integer;
  adjoin,mirror:integer;
  flags1,flags2:longint;
  mark:integer;
 end;

TDFVertex=class
 X,Z:double;
 mark:integer;
end;

TDFWalls=class(TList)
 Function GetItem(n:integer):TDFWall;
 Procedure SetItem(n:integer;v:TDFWall);
{ Procedure Clear;}
 Function NextWL(n:integer):integer;
 Function prevWL(n:integer):integer;
Property Items[n:integer]:TDFWall read GetItem write SetItem; default;
end;

Function TDFWalls.GetItem(n:integer):TDFWall;
begin
 if (n<0) or (n>=count) then raise EListError.CreateFmt('Wall Index is out of bounds: %d',[n]);
 Result:=TDFWall(List[n]);
end;

Procedure TDFWalls.SetItem(n:integer;v:TDFWall);
begin
 if (n<0) or (n>=count) then raise EListError.CreateFmt('Wall Index is out of bounds: %d',[n]);
 List[n]:=v;
end;

{Procedure TDFWalls.Clear;
var i:integer;
begin
 for i:=0 to Count-1 do Items[i].Free;
 inherited Clear;
end;}

Function TDFWalls.NextWL(n:integer):integer;
begin
 if n>=(count-1) then result:=0 else result:=n+1;
end;

Function TDFWalls.prevWL(n:integer):integer;
begin
 if n<=0 then result:=count-1 else result:=n-1;
end;

type

TDFWallsArr=class(TList)
 Function GetItem(n:integer):TDFWalls;
 Procedure SetItem(n:integer;v:TDFWalls);
{ Procedure Clear;}
Property Items[n:integer]:TDFWalls read GetItem write SetItem; default;
end;

Function TDFWallsArr.GetItem(n:integer):TDFWalls;
begin
 if (n<0) or (n>=count) then raise EListError.CreateFmt('Wall Index is out of bounds: %d',[n]);
 Result:=TDFWalls(List[n]);
end;

Procedure TDFWallsArr.SetItem(n:integer;v:TDFWalls);
begin
 if (n<0) or (n>=count) then raise EListError.CreateFmt('Wall Index is out of bounds: %d',[n]);
 List[n]:=v;
end;


type

TDFVertices=class(TList)
 Function GetItem(n:integer):TDFVertex;
 Procedure SetItem(n:integer;v:TDFVertex);
Property Items[n:integer]:TDFVertex read GetItem write SetItem; default;
end;

Function TDFVertices.GetItem(n:integer):TDFVertex;
begin
 if (n<0) or (n>=count) then raise EListError.CreateFmt('Vertex Index is out of bounds: %d',[n]);
 Result:=TDFVertex(List[n]);
end;


Procedure TDFVertices.SetItem(n:integer;v:TDFVertex);
begin
 if (n<0) or (n>=count) then raise EListError.CreateFmt('Vertex Index is out of bounds: %d',[n]);
 List[n]:=v;
end;

{Procedure TDFVertices.Clear;
var i:integer;
begin
 for i:=0 to Count-1 do Items[i].Free;
 inherited Clear;
end;}


type

 TDFSector=class
  ambient:byte;
  Floor_Y,Ceiling_Y:double;
  floor_tx,ceiling_tx:integer;
  sec_Y:double;
  flags:longint;
  layer:integer;
 end;

TDFSectors=class(TList)
 Function GetItem(n:integer):TDFSector;
 Procedure SetItem(n:integer;v:TDFSector);
 Procedure Clear;
Property Items[n:integer]:TDFSector read GetItem write SetItem; default;
end;

Function TDFSectors.GetItem(n:integer):TDFSector;
begin
 if (n<0) or (n>=count) then raise EListError.CreateFmt('Wall Index is out of bounds: %d',[n]);
 Result:=TDFSector(List[n]);
end;

Procedure TDFSectors.SetItem(n:integer;v:TDFSector);
begin
 if (n<0) or (n>=count) then raise EListError.CreateFmt('Wall Index is out of bounds: %d',[n]);
 List[n]:=v;
end;

Procedure TDFSectors.Clear;
var i:integer;
begin
 for i:=0 to Count-1 do Items[i].Free;
 inherited Clear;
end;

Procedure TJKLevel.ImportLEV(F:TFileName;scfactor:double;txhow:integer);
var
     w1,w2:String;
     t:TLECTextFile;
     Textures:TStringList;
     DFSectors:TDFSectors;
     n,i,p:Integer;
     s:String;

     Sector:TDFSector;
     VXList:TDFVertices;
     WLList:TDFWalls;
     nSector:Integer;


Procedure FScanf(const s:string;const format:String;const Vals:array of pointer);
begin
 if not SScanf(s,format,vals) then
  PanMessage(mt_Error,SysUtils.Format('Conversion error at line %d in file %s',[t.CurrentLine,t.GetFullName]));
end;

Function ReadInt(const s:string):Integer;
begin
 if not ValInt(s,Result) then
  PanMessage(mt_Error,Format('Invalid integer at line %d in file %s: %s',[t.CurrentLine,t.GetFullName,s]));
end;

Function ReadDouble(const s:string):Double;
begin
 if not ValDouble(s,Result) then
  PanMessage(mt_Error,Format('Invalid float at line %d in file %s: %s',[t.CurrentLine,t.GetFullName,s]));
end;

Function ReadDword(const s:string):Longint;
begin
 if not ValDword(s,Result) then
  PanMessage(mt_Error,Format('Invalid dword at line %d in file %s: %s',[t.CurrentLine,t.GetFullName,s]));
end;

Function GetTexture(i:Integer):String;
begin
 if i=-1 then begin Result:=''; exit; end;
 if txhow=TX_DFLT then begin Result:='DFLT.MAT'; exit; end;
 if (i<0) or (i>=Textures.Count) then Result:='DFLT.MAT'
 else Result:=ChangeFileExt(Textures[i],'.mat');
end;

Function CalcAngle(nv1,nv2,nv3:integer):double;
var v1,v2,v3:TDFVertex;
begin
 v1:=VXList[nv1]; v2:=VXList[nv2];v3:=VXList[nv3];
 Result:=GetAngle(v2.x,v2.z,v1.x,v1.z,v3.x,v3.z);
end;

{Function IsAngleAbove180(nv1,nv2,nv3:integer):boolean;
var v1,v2,v3:TDFVertex;
begin
 v1:=VXList[nv1]; v2:=VXList[nv2];v3:=VXList[nv3];
 Result:=IsAbove180(v2.x,v2.z,v1.x,v1.z,v3.x,v3.z);
end;}

Function Is0to180ex(nv1,nv2,nv3:integer):boolean; {checks if angle is >0 and <180}
var v1,v2,v3:TDFVertex;
    dx1,dx2,dy1,dy2:double;
begin
 v1:=VXList[nv1]; v2:=VXList[nv2]; v3:=VXList[nv3];
 dx1:=v1.x-v2.x; dy1:=v1.z-v2.z;
 dx2:=v3.x-v2.x; dy2:=v3.z-v2.z;
 Result:=(dx1*dy2-dx2*dy1)>0;
end;

Function is0to180inc(nv1,nv2,nv3:integer):boolean; {checks if angle is >0 and <180}
var v1,v2,v3:TDFVertex;
    dx1,dx2,dy1,dy2:double;
begin
 v1:=VXList[nv1]; v2:=VXList[nv2]; v3:=VXList[nv3];
 dx1:=v1.x-v2.x; dy1:=v1.z-v2.z;
 dx2:=v3.x-v2.x; dy2:=v3.z-v2.z;
 Result:=(dx1*dy2-dx2*dy1)>=0;
end;

Procedure CreateUnderWater(poly:TDFWalls;cl,fl:double);
var jsec:TJKSector;
    jv:TJKVertex;
    v:TDFVertex;
    fsurf,csurf,jsurf:TJKSurface;
    nv:integer;
    i,j:integer;
    un,vn:Tvector;
    vu,vv:double;
    a:double;
begin
 jsec:=NewSector;
 jsec.colormap:='dflt.cmp';
 jsec.mark:=nSector;
 jsec.Layer:=Sector.Layer;
 jsec.Flags:=2;
 Sectors.Add(jsec);

 fsurf:=jsec.NewSurface;
 fsurf.mark:=-1;
 jsec.surfaces.Add(fsurf);
 {Build floor}
 for i:=poly.count-1 downto 0 do
 With poly[i] do
 begin
  v:=VXList[v1];
  jv:=jsec.NewVertex;
  DfToJK(v.x,sector.Floor_y,v.z,jv.x,jv.y,jv.z);
  jv.z:=fl;
  fsurf.AddVertex(jv);
 end;

 fsurf.SurfFlags:=fsurf.SurfFlags or SF_Floor;
 {fsurf.material:=GetTexture(Sector.Floor_TX);}

 SetVec(un,1,0,0);
 SetVec(vn,0,1,0);
 UpdateSurfUVData(fsurf,un,vn);
 ArrangeTexture(fsurf,0,un,vn);

{ With fsurf.Vertices[0],fsurf.TXVertices[0] do
 begin
  vu:=X-Int(X/64)*64;
  vv:=Y-Int(Y/64)*64;
  u:=vu;
  v:=vv;
 end;}

 {Build Ceiling}
 csurf:=jsec.NewSurface;
 jsec.surfaces.Add(csurf);
 csurf.mark:=nsector;

 for i:=0 to poly.count-1 do
 With poly[i] do
 begin
  v:=VXList[v1];
  jv:=jsec.NewVertex;
  DfToJK(v.x,sector.Ceiling_y,v.z,jv.x,jv.y,jv.z);
  jv.z:=cl;
  csurf.AddVertex(jv);
 end;
 SetVec(un,1,0,0);
 SetVec(vn,0,1,0);
 {csurf.material:=GetTexture(Sector.Ceiling_TX);}
 UpdateSurfUVData(fsurf,un,vn);
 ArrangeTexture(csurf,0,un,vn);

 {Build Sides}
 nv:=poly.count;

 for i:=0 to poly.count-1 do
 begin
  jsurf:=jsec.NewSurface;
  jsurf.mark:=-1;
  jsec.surfaces.Add(jsurf);
  j:=(nv-i-1);
  jsurf.AddVertex(fsurf.vertices[j]);
  jsurf.AddVertex(fsurf.vertices[fsurf.PrevVX(j)]);

  jsurf.AddVertex(csurf.vertices[csurf.NextVX(i)]);
  jsurf.AddVertex(csurf.vertices[i]);
  jsurf.NewRecalcAll;
 end;

end;

Procedure PolyToJKSector(poly:TDFWalls);
var jsec:TJKSector;
    jv:TJKVertex;
    v:TDFVertex;
    fsurf,csurf,jsurf:TJKSurface;
    nv:integer;
    i,j:integer;
    un,vn:Tvector;
    vu,vv:double;
    az,awz,a:double;

begin
 jsec:=NewSector;
 jsec.colormap:='dflt.cmp';
 jsec.mark:=nSector;
 jsec.Layer:=Sector.Layer;
 Sectors.Add(jsec);

 fsurf:=jsec.NewSurface;
 fsurf.mark:=-1;
 jsec.surfaces.Add(fsurf);
 {Build floor}
 for i:=poly.count-1 downto 0 do
 With poly[i] do
 begin
  v:=VXList[v1];
  jv:=jsec.NewVertex;
  DfToJK(v.x,sector.Floor_y,v.z,jv.x,jv.y,jv.z);
  fsurf.AddVertex(jv);
 end;

 fsurf.SurfFlags:=fsurf.SurfFlags or SF_Floor;
 fsurf.material:=GetTexture(Sector.Floor_TX);

 if sector.flags and 128<>0 then
  fsurf.SurfFlags:=fsurf.SurfFlags or SFF_SKY;

{ With fsurf.Vertices[0],fsurf.TXVertices[0] do
 begin
  vu:=PixelPerUnit*x;
  vu:=Round(vu/64)*64;
  vv:=PixelPerUnit*y;
  vv:=Round(vv/64)*64;
 end;}

 SetVec(un,1,0,0);
 SetVec(vn,0,1,0);
 UpdateSurfUVData(fsurf,un,vn);
 ArrangeTexture(fsurf,0,un,vn);


 {Build Ceiling}
 csurf:=jsec.NewSurface;
 jsec.surfaces.Add(csurf);
 csurf.mark:=-1;

 for i:=0 to poly.count-1 do
 With poly[i] do
 begin
  v:=VXList[v1];
  jv:=jsec.NewVertex;
  DfToJK(v.x,sector.Ceiling_y,v.z,jv.x,jv.y,jv.z);
  csurf.AddVertex(jv);
 end;
 SetVec(un,1,0,0);
 SetVec(vn,0,1,0);
 csurf.material:=GetTexture(Sector.Ceiling_TX);
 UpdateSurfUVData(fsurf,un,vn);
 ArrangeTexture(csurf,0,un,vn);
 if sector.flags and 1<>0 then
  csurf.SurfFlags:=csurf.SurfFlags or SFF_SKY;

 {Build Sides}
 nv:=poly.count;

 for i:=0 to poly.count-1 do
 begin
  jsurf:=jsec.NewSurface;
  With poly[i] do
  begin
   jsurf.mark:=adjoin;
   jsurf.nmat:=ibot+(itop shl 16);
   jsurf.Material:=GetTexture(IMID);
  end;

  jsec.surfaces.Add(jsurf);
  j:=(nv-i-1);
  jsurf.AddVertex(fsurf.vertices[j]);
  jsurf.AddVertex(fsurf.vertices[fsurf.PrevVX(j)]);

  jsurf.AddVertex(csurf.vertices[csurf.NextVX(i)]);
  jsurf.AddVertex(csurf.vertices[i]);
  jsurf.NewRecalcAll;
 end;

 if sector.sec_y>0 then
 begin
  a:=0;
  DfToJK(a,sector.Floor_y,a,a,a,az);
  DfToJK(a,sector.Floor_y+sector.sec_y,a,a,a,awz);
  CreateUnderWater(poly,az,awz);
 end;

end;

Procedure MergePolys(polys:TDFWallsArr); {Merges polys untils it's no longer possible}

Function FindBackWall(wl:TDFWall;var np,nw:integer):boolean;
var i,j:integer;
    pl:TDFWalls;
begin
 Result:=true;
 for i:=0 to polys.count-1 do
 begin
  pl:=Polys[i];
  for j:=0 to pl.count-1 do
  With pl[j] do if (v1=wl.v2) and (v2=wl.v1) then begin  np:=i;  nw:=j; exit; end;
 end;
 Result:=false;
end;

Function WouldbeConvex(np1,nw1,np2,nw2:integer):boolean;
var wl1,wl2:TDFWall;
begin
 Result:=false;
 With polys[np1] do wl1:=Items[PrevWL(nw1)];
 With polys[np2] do wl2:=Items[NextWL(nw2)];
 if not Is0to180inc(wl1.v1,wl1.v2,wl2.v2) then exit;
 With polys[np2] do wl1:=Items[PrevWL(nw2)];
 With polys[np1] do wl2:=Items[NextWL(nw1)];
 if not Is0to180inc(wl1.v1,wl1.v2,wl2.v2) then exit;
 Result:=true;
end;

Procedure DoMergePolys(np1,nw1,np2,nw2:integer);
var i:integer;
    p1,p2:TDFWalls;
    n:integer;
begin
 p1:=polys[np1];
 p2:=polys[np2];
 p1.Delete(nw1);
 n:=p2.PrevWL(nw2);
 for i:=0 to p2.count-2 do
 begin
  p1.Insert(nw1,p2[n]);
  n:=p2.PrevWL(n);
 end;
 Polys.Delete(np2);
end;

var i,j,p,w:integer;
    wl1,wl2:TDFWall;
    pl:TDFWalls;
    merged:boolean;
begin
repeat
 merged:=false;
 for i:=0 to polys.count-1 do
 begin
  pl:=polys[i];
  if pl.count>=24 then continue;
  for j:=0 to pl.count-1 do
  if FindBackWall(pl[j],p,w) then
  if WouldbeConvex(i,j,p,w) then
  if Polys[p].count<24 then
  begin
   DoMergePolys(i,j,p,w);
   merged:=true;
   break;
  end;
  if merged then break;
 end;
until not merged;
end;

Procedure ImportCycle(cycle:TDFWalls);
var
    cpoly:TDFWalls;
    polys:TDFWallsArr;


Function ArePointsInTri(nv1,nv2,nv3:integer):boolean;
var i,iv:integer;
begin
 result:=true;
for i:=0 to cycle.count-1 do
begin
 iv:=cycle[i].v1;
 if (iv=nv1) or (iv=nv2) or (iv=nv3) then
 else
 if Is0to180inc(nv1,nv2,iv) and
    Is0to180inc(nv2,nv3,iv) and
    Is0to180inc(nv3,nv1,iv) then exit;
end;
Result:=false;
end;

Procedure AddTriangle(wl1,wl2,wl3:TDFWall);
var wl:TDFWall;
begin
 cPoly:=TDFWalls.Create;
 cpoly.Add(wl1);
 cpoly.Add(wl2);
 if wl3<>nil then cpoly.Add(wl3)
 else
 begin
  wl:=TDFWall.Create; WlList.Add(wl);
  wl.v1:=wl2.v2;
  wl.v2:=wl1.v1;
  wl.adjoin:=nsector;
  cpoly.Add(wl);
 end;
 polys.Add(cpoly);
end;

Procedure SubtractTri(swall:integer);
var i,j:integer;
    wl:TDFWall;
    ewall:integer;
begin
 i:=swall;
 ewall:=cycle.NextWL(swall);
 wl:=TDFWall.Create; WLList.Add(wl);
 wl.v1:=cycle[swall].v1;
 wl.v2:=cycle[ewall].v2;
 wl.adjoin:=nsector;

 Cycle.Delete(swall);
 if swall>=cycle.Count then
 begin
  Cycle.Delete(0);
  Cycle.Add(wl)
 end
 else
 begin
  Cycle.Delete(swall); Cycle.Insert(swall,wl);
 end;
end;

var i:integer;
    iwl:integer;
    cwl,nwl:TDFWall;
    nfwl:integer;
    b1,b2:Boolean;
begin {Import Cycle}

 polys:=TDFWallsArr.Create;

 {Check if poly is convex already}
 b1:=false;
for i:=0 to cycle.count-1 do
 With cycle[i] do
  if not Is0to180inc(cycle[cycle.prevWL(i)].v1,v1,v2) then
  begin
   b1:=true; break;
  end;

 if (not b1) and (cycle.count<=24) then begin PolyToJKSector(cycle); exit; end;


 Repeat
   {Check if sectors is a triangle already}
   if cycle.count=3 then begin AddTriangle(cycle[0],cycle[1],cycle[2]); break; end;

   {Try connecting vertices forward to get triangles}
   b1:=false;
   for i:=0 to cycle.count-1 do
   begin
    cwl:=cycle[i]; iwl:=cycle.NextWL(i);
    nwl:=cycle[iwl];
    {Check any angle of triangle for being >180. If it is - nonconvex}
    if not Is0to180ex(cwl.v1,cwl.v2,nwl.v2) then continue;
    if ArePointsInTri(cwl.v1,cwl.v2,nwl.v2) then continue;
    AddTriangle(cwl,nwl,nil);
    SubtractTri(i);
    b1:=true; break;
   end;
{   if (cycle[i].v1=19) and (Cycle[i].v2=4) then
   begin
    if i=1 then;
   end;}
   if cycle.count=3 then begin AddTriangle(cycle[0],cycle[1],cycle[2]); break; end;

   {Try connecting vertices backward to get triangles}
   b2:=false;
   for i:=0 to cycle.count-1 do
   begin
    iwl:=cycle.PrevWL(i);
    nwl:=cycle[i];
    cwl:=cycle[iwl];
    {Check any angle of triangle for being >180. If it is - nonconvex}
    if not Is0to180ex(cwl.v1,cwl.v2,nwl.v2) then continue;
    if ArePointsInTri(cwl.v1,cwl.v2,nwl.v2) then continue;
    AddTriangle(cwl,nwl,nil);
    SubtractTri(iwl);
    b2:=true; break;
   end;

   if not b1 then break; {Couldn't connect - error}


 Until false;

 MergePolys(polys);

 for i:=0 to polys.count-1 do PolyToJKSector(polys[i]);
 polys.free;
end;


Procedure ImportSector(s:TDFSector;svxs:TDFVertices;swls:TDFWalls);
var ncycles:integer;
    cycles:array[0..20] of TDFWalls;
    wls:TDFWalls;
    i,j:Integer;
    fvx,nwl:integer;
    nvx:integer;

Procedure TakeWall(n:Integer);
begin
  wls.Add(swls[n]);
  swls[n].mark:=1;
end;

Procedure TakeFirstWall(n:integer);
begin
 wls:=TDFWalls.Create; cycles[ncycles]:=wls;
 fvx:=swls[n].v1; TakeWall(n);
end;


Function DoesLineIntersect(nv1,nv2:integer):boolean;
var vx1,vx2,vx3,vx4:TDFVertex;
    cyc:TDFWalls;
    i,j:integer;
begin
 Result:=true;
 vx1:=VXList[nv1];
 vx2:=VXList[nv2];
 for i:=0 to ncycles-1 do
 begin
  cyc:=cycles[i];
  for j:=0 to cyc.count-1 do
  with cyc[j] do
  begin
   if (v1=nv1) or (v1=nv2) or (v2=nv1) or (v2=nv2) then continue;
   vx3:=VXList[v1];
   vx4:=VXList[v2];
   if DoIntersect(vx1.x,vx1.z,vx2.x,vx2.z,
                                    vx3.x,vx3.z,vx4.x,vx4.z) then exit;
  end;
 end;
 Result:=false;
end;

Function FindNonIntersectingWall(cycle:integer;var nwl:integer):integer;
{finds wall in main cycle which starts at the vertex
 which being connected to first vertex of given wall
 gives a line that doesn't intersect with any wall in sector}
var i,iwl0:integer;
    wls,wls0:TDFWalls;
    v1,v2:integer;
    cdist,dist:double;
    vx,vx1:TDFvertex;
    n:integer;
begin
 result:=-1;
 wls0:=cycles[0];
 wls:=cycles[cycle];

for iwl0:=0 to wls.count-1 do
begin
 v1:=cycles[cycle][iwl0].v1;
 vx:=VXList[v1];

n:=0; dist:=99999;
for i:=0 to wls0.count-1 do
begin
 vx1:=VXList[wls0[i].v1];
 cdist:=sqr(vx1.x-vx.x)+sqr(vx1.z-vx.z);
 if cdist<dist then begin dist:=cdist; n:=i; end;
end;

i:=n;

for n:=0 to wls0.count-1 do
begin
 v2:=wls0[i].v1;
 {if VXList[v2].mark=1 then continue;}
 if DoesLineIntersect(v1,v2) then begin i:=wls0.NextWL(i); continue; end;
 result:=i;
 nwl:=iwl0;
 exit;
end;
end;
end;

Procedure MergeCycleAt(ncyc,nwl,nwl0:integer);
{merges cycle ncyc with cycle 0
 connecting first vertices of walls
 0 and nwl0}
var w:integer;
    wls,wls0:TDFWalls;
    wl:TDFWall;
    vx0,vxn:integer;
    n:integer;
begin
 wls0:=cycles[0];
 vx0:=wls0[nwl0].V1;

 wls:=cycles[ncyc];
 vxn:=cycles[ncyc][nwl].V1;

 wl:=TDFWall.Create; WLList.Add(wl);
 wl.V1:=vx0;
 wl.V2:=vxn;
 wl.adjoin:=nsector;

 wls0.Insert(nwl0,wl);

 wl:=TDFWall.Create; WLList.Add(wl);
 wl.V1:=vxn;
 wl.V2:=vx0;
 wl.adjoin:=nsector;

 wls0.Insert(nwl0+1,wl);
 inc(nwl0);

 n:=wls.PrevWL(nwl);

 For w:=0 to wls.count-1 do
 begin
  wls0.Insert(nwl0,wls[n]);
  n:=wls.PrevWL(n);
 end;

end;

var asum:double;

begin {ImportSector}
if nsector=21 then
begin
 if nsector=5 then;
end;
 for i:=0 to swls.Count-1 do swls[i].mark:=0;
 ncycles:=0; nwl:=0;
 TakeFirstWall(0);
 Repeat
  nvx:=swls[nwl].v2;
  if nvx=fvx then
  begin
   inc(ncycles);
   nwl:=-1;
   for i:=0 to swls.count-1 do if swls[i].mark=0 then begin nwl:=i; break; end;
   if nwl=-1 then break;
   TakeFirstWall(nwl);
   nvx:=swls[nwl].v2;
  end;
  {find wall that starts at vertex nvx}
  nwl:=-1;
  for i:=0 to swls.Count-1 do
  With swls[i] do
  begin
   if mark=1 then continue;
   if V1=nvx then begin nwl:=i; break; end;
  end;
  if nwl=-1 then
   begin
    PanMessage(mt_warning,Format('Icomplete cycle in sector %d',[nSector]));
    {cycles[ncycles-1].Free;}
    {dec(ncycles);}
    break;
   end;
   TakeWall(nwl);
 until false;
 if ncycles=0 then
 begin
  {Raise Exception.CreateFmt('No closed cycles in sector %d',[nSector]);}
  exit;
 end;

 n:=-1;
 for i:=0 to ncycles-1 do
 begin
  asum:=0;
  wls:=cycles[i];
  for j:=0 to wls.count-1 do
  With wls[j] do
  begin
   asum:=asum+CalcAngle(Wls[wls.PrevWL(j)].V1,v1,v2);
  end;
  if Abs(asum-((cycles[i].count-2)*pi))<0.1 then
  begin
   n:=i;
   break;
  end;
 end;
 if n<>0 then
 begin
  if n=0 then;
 end;

 if n=-1 then
 begin
  PanMessageFmt(mt_warning,'The sector is all inside out %d',[nsector]);
  exit;
 end;

 wls:=cycles[0];
 cycles[0]:=cycles[n];
 cycles[n]:=wls;

 {Merge subsectors with the main cycle}
 for i:=0 to VXList.count-1 do VXList[i].mark:=0;

 if nsector=45 then
 begin
  if i=0 then;
 end;

 for i:=ncycles-1 downto 1 do
 begin
 try
  n:=FindNonIntersectingWall(i,nwl);
 except
  on Exception do;
 end;
  if n=-1 then begin PanMessageFmt(mt_warning,'Sector %d too complex. Part ignored',[nsector]); continue; end;
  VXList[cycles[0][n].v1].mark:=1;
  MergeCycleAt(i,nwl,n);
  cycles[i].free; Dec(nCycles);
 end;
 {single cycle in cycles[0] Ready for breakup}

 ImportCycle(Cycles[0]);


end;


Function ConvertSecFlags(f:Longint):LongInt;
begin
 Result:=0;
{ if f and 1<>0 then Result:=Result or SECFLAG1_EXTCEILING
 else if f and 128<>0    then Result:=Result or SECFLAG1_EXTFLOOR
 else if f and 8<>0      then Result:=Result or SECFLAG1_EXTTOPADJOIN
 else if f and 256<>0    then Result:=Result or SECFLAG1_EXTBTMADJOIN
 else if f and 1024<>0   then Result:=Result or SECFLAG1_NOWALLS
 else if f and 256<>0    then Result:=Result or SECFLAG1_DOOR
 else if f and 2048<>0   then Result:=Result or SECFLAG1_SECDAMAGE_SML
 else if f and 4096<>0   then Result:=Result or SECFLAG1_SECDAMAGE_LGE
 else if f and 524288<>0 then Result:=Result or SECFLAG1_SECRET_TAG;}
end;

Function ConvertWallFlags(f1,f2:Longint):LongInt;
begin
 Result:=0;
{ if f1 and 1<>0 then Result:=Result or WALLFLAG1_MIDTEX
 else if f1 and 2<>0      then Result:=Result or WALLFLAG1_LITSIGN
 else if f1 and 4<>0      then Result:=Result or WALLFLAG1_HFLIP
 else if f1 and 16<>0     then Result:=Result or WALLFLAG1_ANCHOR
 else if f1 and 4096<>0   then Result:=Result or WALLFLAG1_ANCHORSIGN
 else if f1 and 32<>0     then Result:=Result or WALLFLAG1_MOVE
 else if f1 and 64<>0     then Result:=Result or WALLFLAG1_SCROLLTOP
 else if f1 and 128<>0    then Result:=Result or WALLFLAG1_SCROLLMID
 else if f1 and 256<>0    then Result:=Result or WALLFLAG1_SCROLLBOTTOM
 else if f1 and 512<>0    then Result:=Result or WALLFLAG1_SCROLLSIGN
 else if f1 and 1024<>0   then Result:=Result or WALLFLAG1_NOSHOW
 else if f2 and 1<>0      then Result:=Result or WALLFLAG1_FORCEPASS
 else if f2 and 2<>0      then Result:=Result or WALLFLAG1_NOPASS
 else if f2 and 4<>0      then Result:=Result or WALLFLAG1_FENCE;}
end;

Procedure LoadTextures;
var i,n:Integer;
begin
 n:=StrToInt(W2);
 For i:=0 to n-1 do
 begin
  t.ReadLn(s);
  FScanf(s,'TEXTURE: %s',[@W1]);
  Textures.Add(w1);
 end;
end;

Procedure LoadSectors;
var i,n:Integer;

Procedure LoadVertices(n:integer);
var i:Integer;
    Vertex:TDFVertex;
begin
 For i:=0 to n-1 do
 begin
  Vertex:=TDFVertex.Create;
  VXList.Add(Vertex);
  t.ReadLn(s);
  With Vertex do
   FScanf(s,'X: %f Z: %f',[@X,@Z]);
 end;
end;

Procedure LoadWalls(n:integer);
var i,sign:Integer;
    Wall:TDFWall;
    a:double;
begin
 i:=0;
 While (not t.eof) and (i<n) do
 begin
   {Loading wall}
   T.ReadLn(s);
   if trim(s)='' then continue;
   inc(i);
  Wall:=TDFWall.Create;
  With Wall do
  begin
   {Setting defaults}
  FScanf(s,'WALL LEFT: %d RIGHT: %d MID: %d %f %f TOP: %d %f %f BOT: %d %f %f SIGN: %d %f %f ADJOIN: %d MIRROR: %d FLAGS: %s 0 %s LIGHT: %d',
            [@V1,@V2,@IMid,@a,@a,@ITop,@a,@a,@IBot,@a,@a,@Sign,@a,@a,@Adjoin,@Mirror,@w1,@w2,@light]);
   if sign<>-1 then ;
{   Mid.OffsY:=-MID.OffsY;
   BOT.OffsY:=-BOT.OffsY;
   TOP.OffsY:=-TOP.OffsY;
   Overlay.OffsY:=-Overlay.OffsY;}
{   MID.Name:=GetTexture(IMid);
   TOP.Name:=GetTexture(ITop);
   BOT.Name:=GetTexture(IBot);}
   Flags1:=ReadInt(w1);
   Flags2:=ReadInt(w2);
  end;
  WLList.Add(Wall);
 end;
end;

Procedure LoadSector;
var a:integer;
    f:double;
    i:integer;
begin
Sector:=TDFSector.Create;
DFSectors.Add(sector);

for i:=0 to WLList.count-1 do WLList[i].Free;
WLList.Clear;

for i:=0 to VXList.count-1 do VXList[i].Free;
VXList.Clear;


 With Sector do
 begin
 {Setting defaults}
  Floor_Tx:=0;
  Ceiling_Tx:=0;

  {Loading sector}

 Repeat
  t.Readln(s);
  p:=GetWord(s,1,w1);
  p:=GetWord(s,p,w2);
  if w1='' then break;

  if w1='NAME' then {sec.Name:=UpperCase(w2)}
  else if w1='AMBIENT' then ambient:=ReadInt(w2)
  else if (w1='FLOOR') and (w2='ALTITUDE') then begin FScanf(s,'FLOOR ALTITUDE %f',[@Floor_Y]);  end
  else if (w1='CEILING') and (w2='ALTITUDE') then begin FScanf(s,'CEILING ALTITUDE %f',[@Ceiling_Y]); end
  else if (w1='SECOND') and (w2='ALTITUDE') then begin FScanf(s,'SECOND ALTITUDE %f',[@Sec_Y]); end
  else if (w1='FLOOR') and (w2='TEXTURE') then
  begin
   FScanf(s,'FLOOR TEXTURE %d %f %f',[@floor_tx,@F,@F]);
   {Floor_Texture.Name:=GetTexture(a);}
  end
  else if (w1='CEILING') and (w2='TEXTURE') then
  begin
   FScanf(s,'CEILING TEXTURE %d %f %f',[@ceiling_tx,@f,@f]);
   {Ceiling_Texture.Name:=GetTexture(a);}
  end
  else if w1='FLAGS' then
  begin
   flags:=ReadDWord(w2);
  end
  else if w1='LAYER' then begin Layer:=ReadInt(w2); break; end;

 Until false;

 Repeat
  t.Readln(s);
  GetWord(s,1,w1);
 until (w1='VERTICES') or t.eof;

 LoadVertices(StrToInt(GetWordN(s,2)));

 Repeat
  t.Readln(s);
  GetWord(s,1,w1);
 until (w1='WALLS') or t.eof;

 LoadWalls(StrToInt(GetWordN(s,2)));

 ImportSector(sector,vxList,wlList);

 end;
end;

begin {LoadSectors}
 DFSectors:=TDFSectors.Create;
 n:=StrToInt(w2);
 nSector:=0;
 Progress.Reset(n);
 Progress.Msg:='Loading Sectors...';
 While (not t.eof) and (nSector<=n) do
 begin
  t.Readln(s);
  p:=GetWord(s,1,w1);
  p:=GetWord(s,p,w2);
  if w1='' then continue;

  if w1='SECTOR' then begin LoadSector; inc(nSector); Progress.Step; end;
 end;
end;

Procedure AdjoinSurf(surf:TJKSurface);
var i,j:integer;
    sf1:TJKSurface;
begin
 for i:=0 to sectors.count-1 do
 with Sectors[i] do
 begin
  if surf.mark<>mark then continue;
 for j:=0 to surfaces.count-1 do
 begin
  sf1:=surfaces[j];
  if Do_Surf_Match(surf,sf1) then
   SysAdjoinSurfaces(surf,sf1);
 end;
 end;
end;

Procedure ArrangeAdjoins;
var i,j:integer;
    cz,fz,fz1,cz1:double;
    a:double;
    csec_FZ,csec_CZ,
    sec2_FZ,sec2_CZ:double;
    surf:TJKSurface;
    v1,v2,v3,v4:TJKVertex;
    secnum,adjsc:integer;
begin
 {First check if adjoins need BOT and TOP surfs and add them if needed}
for i:=0 to sectors.count-1 do
 with sectors[i] do
 begin
  secnum:=mark;
  With DFSectors[secnum] do
  begin
   DFtoJK(a,Floor_Y,a,a,a,csec_FZ);
   DFtoJK(a,Ceiling_Y,a,a,a,csec_CZ);
  end;

 for j:=surfaces.count-1 downto 0 do
 with Surfaces[j] do
 begin
  if (mark<0) or (mark=secnum) then continue;
  adjsc:=mark;
  With DFSectors[mark] do
  begin
   DFtoJK(a,Floor_Y,a,a,a,sec2_FZ);
   DFtoJK(a,Ceiling_Y,a,a,a,sec2_CZ);
  end;

   if ((sec2_FZ<=csec_FZ) and (sec2_CZ>=csec_CZ)) or
      (sec2_FZ>=csec_CZ) or (sec2_CZ<=csec_FZ) then continue;

   if (sec2_FZ>csec_FZ) and (sec2_CZ>=csec_CZ) then {BOT}
   begin
    {Add BOT surface}
    v1:=vertices[0]; v2:=vertices[1];
    surf:=NewSurface; surf.AddVertex(v1); surf.AddVertex(v2);

    v3:=NewVertex; v3.x:=v2.x; v3.y:=v2.y; v3.z:=sec2_FZ; surf.AddVertex(v3);
    v4:=NewVertex; v4.x:=v1.x; v4.y:=v1.y; v4.z:=sec2_FZ; surf.AddVertex(v4);
    surf.mark:=-1;
    surf.Material:=GetTexture(nmat and 65535);
    surf.NewRecalcAll;
    Surfaces.Add(surf);

    {Add MID surface}
    v1:=v4; v2:=v3;
    v3:=Vertices[2]; v4:=Vertices[3];
    surf:=NewSurface;
    surf.AddVertex(v1); surf.AddVertex(v2);
    surf.AddVertex(v3); surf.AddVertex(v4);
    surf.mark:=adjsc;
    surf.Material:=Material;
    surf.NewRecalcAll;
    Surfaces.Add(surf);
   end;

   if (sec2_FZ<=csec_FZ) and (sec2_CZ<csec_CZ) then {TOP}
   begin
    {Add MID surface}
    v1:=vertices[0]; v2:=vertices[1];
    surf:=NewSurface; surf.AddVertex(v1); surf.AddVertex(v2);
    v3:=NewVertex; v3.x:=v2.x; v3.y:=v2.y; v3.z:=sec2_CZ; surf.AddVertex(v3);
    v4:=NewVertex; v4.x:=v1.x; v4.y:=v1.y; v4.z:=sec2_CZ; surf.AddVertex(v4);
    surf.mark:=adjsc;
    surf.Material:=Material;
    surf.NewRecalcAll;
    Surfaces.Add(surf);
    {Add TOP surface}
    v1:=v4; v2:=v3;
    v3:=Vertices[2]; v4:=Vertices[3];
    surf:=NewSurface;
    surf.AddVertex(v1); surf.AddVertex(v2);
    surf.AddVertex(v3); surf.AddVertex(v4);
    surf.mark:=-1;
    Surf.Material:=GetTexture(nmat shr 16);
    surf.NewRecalcAll;
    Surfaces.Add(surf);
   end;
   if (sec2_FZ>csec_FZ) and (sec2_CZ<csec_CZ) then  {BOT and TOP}
   begin
    {Add BOT surface}
    v1:=vertices[0]; v2:=vertices[1];
    surf:=NewSurface; surf.AddVertex(v1); surf.AddVertex(v2);
    v3:=NewVertex; v3.x:=v2.x; v3.y:=v2.y; v3.z:=sec2_FZ; surf.AddVertex(v3);
    v4:=NewVertex; v4.x:=v1.x; v4.y:=v1.y; v4.z:=sec2_FZ; surf.AddVertex(v4);
    surf.mark:=-1;
    surf.Material:=GetTexture(nMat and 65535);
    surf.NewRecalcAll;
    Surfaces.Add(surf);
    {Add MID surface}
    v1:=v4; v2:=v3;
    v3:=NewVertex; v3.x:=v2.x; v3.y:=v2.y; v3.z:=sec2_CZ;
    v4:=NewVertex; v4.x:=v1.x; v4.y:=v1.y; v4.z:=sec2_CZ;
    surf:=NewSurface;
    surf.AddVertex(v1); surf.AddVertex(v2);
    surf.AddVertex(v3); surf.AddVertex(v4);
    surf.mark:=adjsc;
    Surf.Material:=Material;
    surf.NewRecalcAll;
    Surfaces.Add(surf);
    {Add TOP surface}
    v1:=v4; v2:=v3;
    v3:=Vertices[2]; v4:=Vertices[3];
    surf:=NewSurface;
    surf.AddVertex(v1); surf.AddVertex(v2);
    surf.AddVertex(v3); surf.AddVertex(v4);
    surf.mark:=-1;
    surf.Material:=GetTexture(nmat shr 16);
    surf.NewRecalcAll;
    Surfaces.Add(surf);
   end;

   Surfaces.Delete(j);
  end;
 end;

 {Adjoin surfaces}

 for i:=0 to sectors.count-1 do
 with sectors[i] do
 begin
 for j:=surfaces.count-1 downto 0 do
 with Surfaces[j] do
 begin
  if mark<0 then continue;
  if adjoin<>nil then continue;
  AdjoinSurf(surfaces[j]);
 end;
 end;
end;


Procedure LoadObjects;
var i,n:Integer;
    a:Integer;dif:Integer;
    th:TJKThing;
    ax,ay,az:double;
    dflogics,jktpls:TStringList;

Procedure LoadConvTable;
var cf:TTextFile;
    s,w:string;
    p,i:integer;
begin
 dflogics.sorted:=true;
 try
  cf:=TTextFile.CreateRead(OpenFileRead(BaseDir+'JedData\df2jk.lst',0));
 except
  on Exception do
  begin
   PanMessage(mt_Warning,'Can''t open file '+BaseDir+'JedData\df2jk.lst');
   exit;
  end;
 end;

try
 While not cf.eof do
 begin
  cf.Readln(s);
  RemoveComment(s);
  p:=GetWord(s,1,w);
  if w='' then continue;
  i:=dfLogics.Add(w);
  p:=GetWord(s,p,w);
  jktpls.Insert(i,w);
 end;

finally
 cf.FClose;
end;
end;

Procedure ConvertLogic(const logic:string;var jktpl:string);
var
   lind:integer;
begin
 lind:=dflogics.IndexOf(logic);
 if lind=-1 then exit;
 jktpl:=jktpls[lind];
end;

var v:TTplValue;
    f:Longint;

begin {LoadObjects}
 n:=StrToInt(w2);
 i:=0;
 th:=nil;
 dflogics:=TStringList.Create;
 jktpls:=TStringList.Create;

 LoadConvTable;

try
 While (not t.eof) and (i<n) do
 begin
  t.Readln(s);
  p:=GetWord(s,1,w1);
  p:=GetWord(s,p,w2);
  if w1='' then continue;

  if w1='CLASS:' then
  begin
   th:=NewThing;
   Things.Add(th);
   With th do
   FScanf(s,'CLASS: %s DATA: %d X: %f Y: %f Z: %f PCH: %f YAW: %f ROL: %f DIFF: %d',
             [@w1, @a, @ax, @ay, @az, @pch, @Yaw, @Rol, @Dif]);

   DFToJK(ax,ay,az,th.x,th.y,th.z);
   th.name:='ghost'; if (w1='SPIRIT') or (w1='SAFE') then th.name:='walkplayer';
  end else
  if w1='SEQ' then
  begin
   if th=nil then continue;

   Repeat
    t.readln(s);
    p:=GetWord(s,1,w1);
    if (w1='') or (w1='SEQEND') then break;
    if (w1='LOGIC:') or (w1='TYPE:') then
    begin
     p:=GetWord(s,p,w1);
     if w1='ITEM' then p:=GetWord(s,p,w1);
     ConvertLogic(w1,th.name);
    end;
   until t.eof;

 Case dif of
  -2,-1,2,3:
  begin
   f:=0;
   v:=Templates.GetTPLField(th.name,'thingflags');
   if v<>nil then ValHex(v.AsString,f);
   case dif of
    -2: f:=f or TF_NOHARD;
    -1: f:=f or TF_NOHARD or TF_NOMEDIUM;
     2: f:=f or TF_NOEASY;
     3: f:=f or TF_NOEASY or TF_NOMEDIUM;
   end;
   Th.AddValue('thingflags',Format('0x%x',[f]));
  end;
 end;

 end;

 end;
Finally
 dfLogics.free;
 jktpls.free;
end;
end;

Procedure ConvertLayers;
var i,n:integer;
    s:string;
begin
 for i:=0 to Sectors.count-1 do
 With Sectors[i] do
 begin
  s:=Format('Layer%d',[layer]);
  n:=Layers.IndexOf(s);
  if n=-1 then layer:=Layers.Add(s)
  else layer:=n;
 end;
{ n:=Layers.Add('Things');
 For i:=0 to Things.count-1 do
 things[i].Layer:=n;}
end;

var j:integer;

begin {TLevel.ImportLev}
 VXList:=TDFVertices.Create;
 WLList:=TDFWalls.Create;
 Try
 t:=TLECTextFile.CreateRead(OpenFileRead(F,0));
 Textures:=TStringList.Create;
 Clear;

 jkdffactor:=scfactor;
 ppunit:=320*scfactor/40;

 Try
 Try
   {Filling the unused values with defaults}
{   Music:='NULL';
   Version:=1535.022197;
   SetDefaultShades;}
 {Reading file}
  While not t.eof do
  begin
   t.readln(s);
   p:=GetWord(s,1,w1); {w1 - first word, w2 - second}
   p:=GetWord(s,p,w2);
   if w1='' then continue;

   if w1='LEV' then
   else if w1='LEVELNAME' then
   else if w1='TEXTURES' then LoadTextures
   else if w1='PARALLAX' then {FScanf(s,'PARALLAX %f %f',[@Parallax_x,@Parallax_y])}
   else if w1='NUMSECTORS' then LoadSectors;
  end;

 Except
  On E:EConvertError do
   Raise Exception.CreateFmt('File %s: Error at line %d: %s',[F,t.CurrentLine,E.Message]);
 end;

 Finally
  t.Fclose;
  if Assigned(VXList) then
  begin
   for i:=0 to VXList.Count-1 do VXList[i].Free;
   VXList.Free;
  end;

  if Assigned(WLList) then
  begin
   For i:=0 to WLList.Count-1 do WLList[i].Free;
   WLlist.Free;
  end;

  ArrangeAdjoins;

  For i:=0 to Sectors.count-1 do
   SetSecLight(Sectors[i],DFSectors[Sectors[i].mark].ambient);

  if Assigned(DFSectors) then
  begin
   For i:=0 to DFSectors.count-1 do DFSectors[i].Free;
   DFSectors.Free;
  end;

  textures.free;
  MasterCMP:='';
  RenumSecs;
  for i:=0 to Sectors.count-1 do
  With Sectors[i] do
  begin
   Renumber;
   for j:=0 to surfaces.count-1 do surfaces[j].recalc;
  end;
 { WLList.Clear;
 WLList.Free;}
 end;

 Except
  On E:EInOutError do PanMessage(mt_error,E.Message+' '+F);
  On E:Exception do PanMessage(mt_error,E.Message);
 end;

Try
t:=TLECTextFile.CreateRead(OpenFileRead(ChangeExt(F,'.O'),0));
Try
Try
  While not t.eof do
  begin
   t.readln(s);
   p:=GetWord(s,1,w1); {w1 - first word, w2 - second}
   p:=GetWord(s,p,w2);
   if w1='' then continue;

   if w1='O' then
   else if w1='LEVENAME' then
   else if w1='PODS' then
   else if w1='SPRS' then
   else if w1='FMES' then
   else if w1='SOUNDS' then
   else if w1='OBJECTS' then LoadObjects;
  end;
 Except
  On E:EConvertError do
   Raise Exception.CreateFmt('File %s: Error at line %d: %s',[F,t.CurrentLine,E.Message]);
 end;

finally
 RenumThings;
 ConvertLayers;
 T.Fclose;
 Progress.Hide;
end;

 Except
  On E:EInOutError do PanMessage(mt_error,E.Message+' '+ChangeExt(F,'.O'));
  On E:Exception do PanMessage(mt_error,E.Message);
end;

end;