Type
TTempVertex=class
  x,y,z:double;
end;

TTempTXVertex=class
  u,v:single;
end;

TAdjoin=class
   mirror,surf:integer;
   dist:double;
   flags:longint;
end;

TMatDims=class
 dim1,dim2:double;
end;

TVXIntensity=class
 intensity,
 r,g,b:single;
end;

TTempSurf=class
  nmat:integer;
  surfflags,faceflags:longint;
  geo,light,nadj,tex:integer;
  extra:double;
  VXS:TIntList;
  TVXs:TIntList;
  intensities:TList;
  Constructor Create;
  Destructor Destroy;override;
end;

Constructor TTempSurf.Create;
begin
 VXS:=TIntList.Create;
 TVXS:=TIntList.Create;
 intensities:=TList.Create;
end;

Destructor TTempSurf.Destroy;
var i:integer;
begin
 VXS.Free;
 TVXS.Free;
 for i:=0 to Intensities.count-1 do
  TVXIntensity(Intensities[i]).Free;
 Intensities.Free;
end;

Procedure TJKLevel.LoadFromJKL(F:TFileName);
var t:TTextFile; cpos:word; s,w1,w2:String;
    cur_line:integer;
    a,n,nx:Integer;
    EndOfSection:boolean;
    CurSection:String;
    GVXList,GSurfList,
    GTXVXList,GAdjoins:TList;
    mats:TStringList;
    cmps:TStringList;
    vr,vg,vb:single;
Procedure GetNextLine(var s:String);
var cmt_pos:word; {Position of #}
begin
 s:='';
 Repeat
  if t.eof then begin EndOfSection:=true; exit; end;
  t.Readln(s);
  inc(cur_line);
  cmt_pos:=Pos('#',s);
  if cmt_pos<>0 then SetLength(s,cmt_pos-1);
  s:=UpperCase(Trim(s));
 Until s<>'';
 if s='END' then begin CurSection:=''; EndOfSection:=true; end;
 if GetWordN(s,1)='SECTION:' then begin CurSection:=GetWordN(s,2); EndOfSection:=true; end;
end; {GetNextLine}

Procedure SkipToNextSection;
begin
 While not EndOfSection do GetNextLine(s);
end;

Procedure Error(s:String);
begin
 PanMessage(mt_error,'File '+f+Format(' Error at line %d: %s',[cur_line,s]));
end;

Const
   pi_steps=100;
var
   ppos,fsize:longint;

Procedure DO_Progress;
var pos:integer;
begin
 pos:=Round(t.fpos*pi_steps/fsize);
 While pos>ppos do
 begin
  inc(ppos);
  Progress.Step;
 end;
end;

Function GetCmp(n:integer):String;
begin
 if (n<0) or (n>=cmps.count) then n:=0;
 result:=cmps[n];
end;

Function GetMat(n:integer):String;
begin
 if (n<0) or (n>=mats.count) then begin result:=''; exit; end;
 result:=mats[n];
end;

Procedure LoadMats;
var i,n:integer;
begin
 CurSection:='';
 GetNextLine(s);
 n:=StrToInt(GetWordN(s,3));
 i:=0;
 While (i<n) and (not EndOfSection) do
 begin
  GetNextLine(s);
  GetWord(s,1,w1);
  if w1='END' then break;
  SScanf(s,'%s %s',[@w1,@w2]);
  mats.Add(w2);
 end;
end;

Procedure LoadGeometry;
var i,j,n,p,pc:integer;
    tempVX:TTempVertex;
    nvx,vx,tvx:integer;
    TSurf:TTempSurf;
    txvx:TTempTXVertex;
    adj:TAdjoin;
    vxInt:TVXIntensity;
    il:TSingleList;
begin
 il:=TSingleList.Create;
 
 CurSection:='';
 {ColorMaps}
 GetNextLine(s);

 SScanf(s,'WORLD COLORMAPS %d',[@n]);
 For i:=0 to n-1 do
 begin
  GetNextLine(s);
  SScanf(s,'%s %s',[@w1,@w2]);
  cmps.Add(w2);
 end;
 {Vertices}
try
 DO_Progress;
except
 on Exception do ;
end;

 Progress.Msg:='Loading Vertices';
 GetNextLine(s);
{ n:=StrToInt(GetWordN(s,3));}
 SScanf(s,'WORLD VERTICES %d',[@n]);
 For i:=0 to n-1 do
 begin
  GetNextLine(s);
  p:=pos(':',s);
  w1:=copy(s,1,p-1);
  if StrToInt(w1)<>i then Raise Exception.Create('');
  TempVX:=TTempVertex.Create;
  GVXList.Add(TempVX);
  With TempVX do SSCanf(s,'%s %f %f %f',[@w1,@x,@y,@z]);
 end;
 {Texture Vertices}
 Progress.Msg:='Loading Texture Vertices';
 GetNextLine(s);
 SScanf(s,'WORLD TEXTURE VERTICES %d',[@n]);
 For i:=0 to n-1 do
 begin
  DO_Progress;
  GetNextLine(s);
  txvx:=TTempTXVertex.Create;
  GTXVXList.Add(txvx);
  SScanf(s,'%s %l %l',[@w1,@txvx.u,@txvx.v]);
 end;

 {Adjoins}
 GetNextLine(s);
 DO_Progress;
 Progress.Msg:='Loading Adjoins';
 SScanf(s,'WORLD ADJOINS %d',[@n]);
 For i:=0 to n-1 do
 begin
  GetNextLine(s);
  adj:=TAdjoin.Create;
{!}  SScanf(s,'%s %x %d',[@w1,@adj.flags,@adj.mirror]);
  GAdjoins.Add(adj);
  adj.surf:=-1;
 end;
{Surfaces}
 GetNextLine(s);
 Progress.Msg:='Loading Surfaces';

 SSCanf(s,'WORLD SURFACES %d',[@n]);
 For i:=0 to n-1 do
 begin
  DO_Progress;
  GetNextLine(s);
  TSurf:=TTempSurf.Create;
  GSurfList.Add(TSurf);
  p:=GetWord(s,1,w1);
  p:=GetWord(s,p,w1); {material}
  ValInt(w1,TSurf.nmat);
  p:=GetWord(s,p,w1); {surfflags}
  ValHex(w1,TSurf.SurfFlags);
  p:=GetWord(s,p,w1); {faceflags}
  ValHex(w1,TSurf.FaceFlags);
  p:=GetWord(s,p,w1); {geo}
  ValInt(w1,TSurf.geo);
  p:=GetWord(s,p,w1); {light}
  ValInt(w1,TSurf.light);
  p:=GetWord(s,p,w1); {tex}
  ValInt(w1,TSurf.tex);
  p:=GetWord(s,p,w1); {adjoin}
  ValInt(w1,TSurf.nadj);
  if TSurf.nadj<>-1 then
   TAdjoin(GAdjoins[TSurf.nadj]).Surf:=i;
  p:=GetWord(s,p,w1); {extralight}
  ValDouble(w1,TSurf.extra);
  p:=GetWord(s,p,w1); {Nvert}
  nvx:=StrToInt(w1);
  for j:=0 to nvx-1 do
  begin
   p:=GetWord(s,p,w1);
   pc:=Pos(',',w1);
   if pc<>0 then w1[pc]:=' ';
   SScanf(w1,'%d %d',[@vx,@tvx]);
   TSurf.VXS.Add(vx);
   TSurf.tvxs.Add(tvx);
  end;
  il.Clear;

  While p<=length(s) do
  begin
   p:=GetWord(s,p,w1);
   il.Add(StrToFloat(w1));
  end;

  if il.Count>nvx then
  begin {MOTS}
   mots:=true;
   for j:=0 to nvx-1 do
   begin
    vxInt:=TVXIntensity.Create;
    TSurf.Intensities.Add(vxInt);
    vxInt.Intensity:=il[j*4];
    vxInt.r:=il[j*4+1];
    vxInt.g:=il[j*4+2];
    vxInt.b:=il[j*4+3];
   end;
  end
  else {JK}
  begin
   for j:=0 to nvx-1 do
   begin
    vxInt:=TVXIntensity.Create;
    TSurf.Intensities.Add(vxInt);
    vxInt.Intensity:=il[j];
    vxInt.r:=vxInt.Intensity;
    vxInt.g:=vxInt.Intensity;
    vxInt.b:=vxInt.Intensity;
   end;
  end;
 end;
{Normals}
 il.free;

 DO_Progress;
 Progress.Msg:='Loading Normals';
 For i:=0 to n-1 do
 begin
  GetNextLine(s);
 end;

 {To ensure that current project mode is correct when
  COGS start loading}
  SetMots(Mots);

 end;

Procedure LoadSectors;
var i,j,n:integer;
    nvx:integer;
    jsec:TJKSector;
    jvx:TJKVertex;
    jsurf:TJKSurface;
    Tsurf:TTempSurf;
    SCVXList:TIntList;
    bsurf,nsurfs:integer;
    isurf:integer;
    igvx,ilvx:integer;
    txvx:TTempTXVertex;
    itxvx:integer;
    vxInt:TVXIntensity;

Procedure SetJVX(sv:TTempVertex;dv:TJKVertex);
begin
 dv.x:=sv.x;
 dv.y:=sv.y;
 dv.z:=sv.z;
end;

begin

Try
 CurSection:='';
 GetNextLine(s);
 n:=StrToInt(GetWordN(s,3));
 SCVXList:=TIntList.Create;
 Progress.Msg:='Loading Sectors';
 For i:=0 to n-1 do
 begin
  DO_Progress;
  GetNextLine(s); {Sector N}
  SCVXList.Clear;
  Jsec:=NewSector;
  Sectors.Add(Jsec);

  Repeat
  GetNextLine(s);
  w1:=getWordN(s,1);

  With JSec do
  if w1='FLAGS' then SScanf(s,'FLAGS'#9'%x',[@Flags])
  else if w1='AMBIENT' then SScanf(s,'AMBIENT LIGHT %f',[@Ambient])
  else if w1='EXTRA' then SScanf(s,'EXTRA LIGHT %f',[@Extra])
  else if w1='SOUND' then SScanf(s,'SOUND %s %f',[@Sound,@Snd_vol])
  else if w1='COLORMAP' then ColorMap:=GetCmp(StrToInt(GetWordN(s,2)))
 { else if w1='CENTER' then SScanf(s,'CENTER %f %f %f',[@Cx,@Cy,@Cz])
  else if w1='RADIUS' then SScanf(s,'RADIUS %f',[@rad])
  else if w1='COLLIDEBOX' then
   With CBox do SScanf(s,'COLLIDEBOX %f %f %f %f %f %f',[@x1,@y1,@z1,@x2,@y2,@z2])}
  else if w1='TINT' then SScanf(s,'TINT %f %f %f',[@Tint.dx,@Tint.dy,@tint.dz]);

  if w1='VERTICES' then
  begin
   nvx:=StrToInt(GetWordN(s,2));
   for j:=0 to nvx-1 do GetNextLine(s);
   continue;
  end;

 if w1='SURFACES' then
 begin
  SScanf(s,'SURFACES %d %d',[@bsurf,@nsurfs]);
  for isurf:=bsurf to bsurf+nsurfs-1 do
  begin
   Tsurf:=TTempSurf(GSurfList[isurf]);
   jsurf:=jsec.NewSurface;
   jsec.surfaces.Add(jsurf);
   jsurf.material:=GetMat(TSurf.nmat);
   jsurf.SurfFlags:=Tsurf.SurfFlags;
   jsurf.FaceFlags:=Tsurf.FaceFlags;
   jsurf.geo:=tSurf.geo;
   Jsurf.Light:=tSurf.light;
   Jsurf.tex:=TSurf.tex;
   Jsurf.nAdj:=TSurf.nAdj;
   if tsurf.SurfFlags and SFF_DOUBLERES<>0 then begin jsurf.uscale:=2; jsurf.vscale:=2; end
   else if tsurf.SurfFlags and SFF_HALFRES<>0 then begin jsurf.uscale:=0.5; jsurf.vscale:=0.5; end
   else if tsurf.SurfFlags and SFF_EIGHTHRES<>0 then begin jsurf.uscale:=0.125; jsurf.vscale:=0.125; end;


   for j:=0 to TSurf.vxs.count-1 do
   begin
    igvx:=TSurf.vxs[j]; {global index of vertex}
    ilvx:=SCVXList.IndexOf(igvx); {local index}
    if ilvx=-1 then {not found}
    begin
     jvx:=jsec.NewVertex;
     SetJVX(GVXList[igvx],jvx);
{     jsec.vertices.Add(jvx);}
     SCVXList.Add(igvx);
     jsurf.AddVertex(jvx);
    end;
    if ilvx<>-1 then jsurf.addVertex(jsec.vertices[ilvx]);

     {Add texture vertices}

     itxvx:=tSurf.tvxs[j];
     if (itxvx<0) or (itxvx>=GTXVXList.Count) then itxvx:=0;
     txvx:=TTempTXVertex(GTXVXList[itxvx]);
     With Jsurf.Txvertices[j] do
     begin
      u:=txvx.u;
      v:=txvx.v;
      vxInt:=TVXIntensity(tSurf.Intensities[j]);
      intensity:=vxInt.intensity;
      r:=vxInt.r;
      g:=vxInt.g;
      b:=vxInt.b;
     end;

   end;
  end;
   break;
  end;
 until false;
 end;
 SCVXList.Free;

 Except
  on e:Exception do;
 end;
end;

Function GetSurfByN(n:integer):TJKSurface;
var i,ns:integer;
begin
 result:=nil;
 ns:=0;
 For i:=0 to Sectors.Count-1 do
 With Sectors[i] do
 begin
  if (n>=ns) and (n<ns+Surfaces.COunt) then
  begin
   Result:=Surfaces[n-ns];
   exit;
  end;
  inc(ns,Surfaces.Count);
 end;
end;

Procedure LoadThings;
var i,j,n,p,nsec:integer;
    th:TThing;
    pe:integer;
    v:TTPLValue;
begin
 Progress.Msg:='Loading Things';
 CurSection:='';
 GetNextLine(s);
 SScanf(s,'WORLD THINGS %d',[@n]);
 For i:=0 to n-1 do
 begin
  GetNextLine(s);
  if (s='END') or EndOfSection then break;
  th:=NewThing;
  With th do
  SScanf(s,'%s %s %s %f %f %f %f %f %f %d',
        [@w1,@w1,@Name,@x,@y,@z,@pch,@yaw,@rol,@nsec]);
 try
  th.Sec:=Sectors[nsec];
 except
  on Exception do th.sec:=nil;
 end;

  Things.Add(th);
  p:=1;
  For j:=1 to 10 do
  begin
   p:=ScanForNonSpace(s,p);
   p:=ScanForSpace(s,p);
  end;

  While p<=Length(s) do
  begin
   p:=getWord(s,p,w1);
   pe:=Pos('=',w1);
   if pe=0 then continue;
   v:=TTPLValue.Create;
   v.Name:=Copy(w1,1,pe-1);
   v.s:=Copy(w1,pe+1,length(w1)-pe);
   v.vtype:=GetTPLVType(v.name);
   v.atype:=GetTPLType(v.name);
   th.Vals.Add(v);
  end;

 end;
end;

Procedure LoadLights;
var i,n,l:integer;
    lt:TJEDLight;
begin
 CurSection:='';
 GetNextLine(s);
 n:=StrToInt(GetWordN(s,3));
 i:=0;
 While (i<n) and (not EndOfSection) do
 begin
  GetNextLine(s);
  GetWord(s,1,w1);
  if w1='END' then break;
  lt:=NewLight;
  With Lt do
  SScanf(s,'%s %d %f %f %f %f %f',[@w1,@l,@x,@y,@z,@Intensity,@range]);

  lt.rgbintensity:=lt.intensity;
  lt.r:=1;
  lt.g:=1;
  lt.b:=1;

  Lights.Add(lt);
 end;
end;



Procedure LoadCogs;
var i,n,p,idx:integer;
    cg:TCog;
    cv:TCogValue;
    cf:TCogFile;
    nval:integer;
begin
 cf:=TCogFile.Create;

 CurSection:='';
 DO_Progress;
 Progress.Msg:='Loading Cogs';
 GetNextLine(s);
 SScanf(s,'WORLD COGS %d',[@n]);
 for i:=0 to n-1 do
 begin
  GetNextLine(s);
  if (s='END') or EndOfSection then break;
  p:=GetWord(s,1,w1);
  p:=GetWord(s,p,w1);
  cg:=TCog.Create;
  Cogs.Add(cg);
  cg.Name:=w1;
  cf.LoadNoLocals(cg.Name);
  nval:=0;
  While p<=Length(s) do
  begin
   p:=getWord(s,p,w1);
   if w1='' then continue;
   cv:=TCOGValue.Create;
   cg.Vals.Add(cv);

   if nval<cf.Count then cv.Assign(cf[nval]);
   cv.Val(w1);
    inc(nval);
  end;
 end;
 cf.free;
end;

Procedure LoadHeader;
var p:integer;
begin
 CurSection:='';
 With header do
 While not EndOfSection do
 begin
  GetNextLine(s);
  p:=GetWord(s,1,w1);
  p:=GetWord(s,p,w2);

 if w1='VERSION' then ValDword(w2,Version)
 else if w1='LOD' then
   SScanf(s,'LOD DISTANCES %f %f %f %f',[@LODDist[1],@LODDist[2],@LODDist[3],@LODDist[4]])
 else if w1='MIPMAP' then
   SScanf(s,'MIPMAP DISTANCES %f %f %f %f',[@MipMapDist[1],@MipMapDist[2],@MipMapDist[3],@MipMapDist[4]])
 else if w1='WORLD' then SScanf(s,'WORLD GRAVITY %f',[@Gravity])
 else if w1='PERSPECTIVE' then SScanf(s,'PERSPECTIVE DISTANCE %f',[@PerspDist])
 else if w1='GOURAUD' then SScanf(s,'GOURAUD DISTANCE %f',[@GouraudDist])
 else if w1='CEILING' then
 begin
  GetWord(s,p,w1);
  if w1='OFFSET' then SScanf(s,'CEILING SKY OFFSET %f %f',[@CeilingSkyOffs[1],@CeilingSkyOffs[2]]);
  if w1='Z' then SScanf(s,'CEILING SKY Z %f',[@CeilingSkyZ]);
 end
 else if w1='HORIZON' then
 begin
  GetWord(s,p,w1);
  if w2='DISTANCE' then ValDouble(w1,HorDistance);
  if w2='PIXELS' then SScanf(s,'HORIZON PIXELS PER REV %f',[@HorPixelsPerRev]);
  if w2='SKY' then SScanf(s,'HORIZON SKY OFFSET %f %f',[@HorSkyOffs[1],@HorSkyOffs[2]]);
 end;
 end;
end;

var i,j:integer;
    cv:TCOGValue;
    tmpsurf:TJKSurface;
begin {LoadFromJKL begin}
 ppos:=0;
 DecimalSeparator:='.';
 mats:=TStringList.Create;
 cmps:=TStringList.Create;
 t:=TTextFile.CreateRead(OpenFileRead(f,0));
 fsize:=t.Fsize;
 Clear;
 cur_line:=0;
 EndOfSection:=false;
 CurSection:='';
 SetDefaultHeader;
 GVXList:=TVertices.Create;
 GSurfList:=TList.Create;
 GTXVXList:=TList.Create;
 GAdjoins:=TList.Create;
 Progress.Reset(pi_steps);
Try
Try
Repeat
While (CurSection='') and (not t.eof) do GetNextLine(s);
 if t.eof then break;
 EndOfSection:=false;
 if CurSection='GEORESOURCE' then LoadGeometry
 else if CurSection='HEADER' then LoadHeader
 else if CurSection='SECTORS' then LoadSectors
 else if CurSection='MATERIALS' then LoadMats
 else if CurSection='THINGS' then LoadThings
 else if CurSection='COGS' then LoadCogs
 else if CurSection='LIGHTS' then LoadLights
 else SkipToNextSection;
until t.eof;
Finally
t.FClose;
Progress.Hide;
{Post process}
for i:=0 to Sectors.Count-1 do
With Sectors[i] do
for j:=0 to Surfaces.Count-1 do
With Surfaces[j] do
begin
 if nadj<0 then begin Adjoin:=nil; continue; end;
 With TAdjoin(GAdjoins[nadj]) do
 begin
  Adjoin:=GetSurfByN(TAdjoin(GAdjoins[Mirror]).Surf);
  AdjoinFlags:=Flags;
 end;
end;
{Free all stuff}
for i:=0 to GVXList.Count-1 do TTempVertex(GVXList[i]).Free;
GVXList.Free;
For i:=0 to GSurfList.Count-1 do TTempSurf(GSurfList[i]).Free;
GSurfList.Free;
For i:=0 to GTXVXList.Count-1 do TTempTXVertex(GTXVXList[i]).Free;
GTXVXList.Free;
For i:=0 to GAdjoins.Count-1 do TAdjoin(GAdjoins[i]).Free;
GAdjoins.Free;

 RenumSecs;
 RenumThings;

 for i:=0 to Sectors.Count-1 do
 With Sectors[i] do
 begin
  Renumber;
  for j:=0 to Surfaces.Count-1 do
  begin
   tmpsurf:=Surfaces[j];
   tmpsurf.Recalc;
   if IsTXFlipped(tmpsurf) then tmpsurf.surfFlags:=tmpsurf.surfFlags or SFF_Flip
   else tmpsurf.surfFlags:=tmpsurf.surfFlags and (not SFF_Flip);

  end;
 end;

  For i:=0 to Cogs.Count-1 do
 With Cogs[i] do
 for j:=0 to Vals.count-1 do
   Vals[j].Resolve;

 AddMissingLayers;
 for i:=0 to Mats.Count-1 do Mats.Objects[i].Free;
 Mats.Free;
 Cmps.Free;
end;
except
 On e:exception do PanMessage(mt_warning,e.message);
end;

end; {LoadFromJKL end}

Procedure TJKLevel.JKLPostLoad;
var i,j:integer;
begin
end;

Procedure TJKLevel.SaveToJed(F:TFileName);
var i,s,sf,vx:integer;
    t:TextFile;
    st:string;
    asec,asf:integer;
Procedure WriteFmt(const fmt:string;v:array of const);
begin
 Writeln(t,Sprintf(fmt,v));
end;

Procedure WriteS(const s:string);
begin
 Writeln(t,s);
end;

begin
 RenumSecs;
 for i:=0 to Sectors.Count-1 do Sectors[i].Renumber;
 
 DecimalSeparator:='.';
 Assign(t,f);Rewrite(t);
 WriteS('JED '+JedVerNum);

 if Mots then
 begin
   WriteS('');
   WriteS('SEC: MOTS');
   WriteS('END');
   WriteS('');
 end;

 With Header do
 begin
  WriteS('SEC: HEADER');

  st:=MasterCMP; if st='' then st:='-';

  WriteFmt('HDR %d %.6f %.6f %.6f %.6f %s %.2f',
   [version,Gravity, CeilingSkyZ,PerspDist, GouraudDist,st,ppunit]);

  WriteFmt('HOR %.6f %.6f %.6f %.6f',
   [HorDistance,HorPixelsPerRev,HorSkyOffs[1],HorSkyOffs[2]]);
  WriteFmt('CEIL %.6f %.6f',
   [CeilingSkyOffs[1],CeilingSkyOffs[2]]);
  WriteFmt('MIP %.6f %.6f %.6f %.6f',
   [MipMapDist[1],MipMapDist[2],MipMapDist[3],MipMapDist[4]]);
  WriteFmt('LOD %.6f %.6f %.6f %.6f',
   [LODDist[1],LODDist[2],LODDist[3],LODDist[4]]);
  WriteS('END');
  WriteS('');
 end;
 RenumSecs;
 For i:=0 to Sectors.Count-1 do Sectors[i].Renumber;
 WriteS('SEC: GEO');
 WriteFmt('SECS %d',[sectors.count]);
Try
 for s:=0 to sectors.count-1 do
 With Sectors[s] do
 begin
  if Colormap='' then st:='-' else st:=Colormap;
  if Sound='' then st:=st+' -' else st:=st+' '+Sound;

  WriteFmt('%s %.6f %x %.6f %.6f %.6f %.6f %.6f %d',
   [st,snd_vol,Flags,Ambient,extra,tint.dx,tint.dy,tint.dz,Layer]);

  WriteFmt('VXS %d',[vertices.count]);
  for vx:=0 to vertices.count-1 do
  With vertices[vx] do
  begin
   WriteFmt('%.6f %.6f %.6f',[x,y,z]);
  end;
  WriteFmt('SURFS %d',[surfaces.count]);
  for sf:=0 to surfaces.count-1 do
  With surfaces[sf] do
  begin
   if adjoin<>nil then begin asf:=Adjoin.Num; asec:=Adjoin.Sector.Num; end
   else begin asf:=-1; asec:=-1; end;
   st:=Material;
   if st='' then st:='-';
   WriteFmt('%s %x %x %d %d %d %.6f %d %d %d %x %.4f %.4f',
     [st,SurfFlags,FaceFlags,geo,light,tex,extralight,Vertices.Count,asec,asf,AdjoinFlags,uscale,vscale]);
   st:='';
   for vx:=0 to vertices.count-1 do
   With TXVertices[vx] do
   begin
     st:=Concat(st,Sprintf(' %d %.2f %.2f %.4f',
     [vertices[vx].num,u,v,intensity]));
   end;
   for vx:=0 to vertices.count-1 do
   With TXVertices[vx] do
   begin
     st:=Concat(st,Sprintf(' %.4f %.4f %.4f',[r,g,b]));
   end;
   Writeln(t,st);
  end;
 end;
 WriteS('END');
 WriteS('');
 WriteS('SEC: THINGS');
 WriteFmt('THS %d',[Things.Count]);
 For s:=0 to Things.Count-1 do
 With Things[s] do
 begin
  if sec=nil then asec:=-1 else asec:=Sec.num;
  WriteFmt('%s %.6f %.6f %.6f %.6f %.6f %.6f %d %d',
   [Name,X,Y,Z,PCH,YAW,ROL,aSec,Layer]);
  st:='';
  For i:=0 to Vals.Count-1 do
  With Vals[i] do
   st:=Concat(st,' ',Name,'=',AsString);
  WriteS(st);
 end;
 WriteS('END');
 WriteS('');
 WriteS('SEC: LIGHTS');
 WriteFmt('LTS %d',[Lights.count]);
  for i:=0 to Lights.count-1 do
  With Lights[i] do
    WriteFmt('%.6f %.6f %.6f %d %.6f %.6f %x %.6f %.6f %.4f %.4f %.4f',
      [X,Y,Z,Layer,Intensity,range,Flags,RGBRange,RGBIntensity,R,g,b]);
 WriteS('END');
 WriteS('');
 WriteS('SEC: COGS');
 WriteFmt('COGS %d',[Cogs.Count]);
 For i:=0 to Cogs.Count-1 do
 With Cogs[i] do
 begin
  st:=name;
  For s:=0 to Vals.Count-1 do
  With Vals[s] do
   st:=Concat(st,' ',Name,':',GetCogTypeName(cog_type),'=',AsString);
  WriteS(st);
 end;
 WriteS('END');

 WriteS('');
 WriteS('SEC: LAYERS');
 WriteFmt('LAYERS %d',[Layers.Count]);
 For i:=0 to Layers.Count-1 do
  WriteS(Layers[i]);
 WriteS('END');

 WriteS('');
 WriteS('SEC: LVIS');
 WriteS(LVisString);
 WriteS('END');

 WriteS('');
 WriteS('SEC: HIERARCHYDEF');
 WriteFmt('NODES %d',[h3donodes.count]);
 for i:=0 to h3donodes.count-1 do
  WriteS(HNodeToString(h3donodes[i],i));
 WriteS('END');



finally
 Close(t);
end;
end;



Procedure TJKLevel.LoadFromJed(F:TFileName);
var p,i,j,s,sf,vx:integer;
    t:TTextFile;
    st,w:string;
    asec,asf:integer;
    ppos,fsize:longint;
    jedver:double;
Const
    pi_steps=200;

Procedure DO_Progress;
var pos:integer;
begin
 pos:=Round(t.fpos*pi_steps/fsize);
 While pos>ppos do
 begin
  inc(ppos);
  Progress.Step;
 end;
end;

Procedure Error(s:String);
begin
 PanMessage(mt_error,Format(' Error at line %d: %s',[t.curline,s]));
end;

Procedure ReadFmt(const fmt:string;v:array of pointer);
begin
 t.Readln(st);
 SScanf(st,fmt,v);
end;

Procedure ReadS(Var s:string);
begin
 t.Readln(s);
 RemoveComment(s);
end;

Procedure SkipToEOS;
var s:string;
begin
While not t.eof do
begin
 t.Readln(s);
 RemoveComment(s);
 if CompareText(s,'end')=0 then exit;
end;
end;

Procedure LoadHeader;
begin
With Header do
begin
  ReadFmt('HDR %d %f %f %f %f %s %d',
   [@version,@Gravity, @CeilingSkyZ,@PerspDist, @GouraudDist,@MasterCMP,@ppunit]);
  ReadFmt('HOR %f %f %f %f',
   [@HorDistance,@HorPixelsPerRev,@HorSkyOffs[1],@HorSkyOffs[2]]);
  ReadFmt('CEIL %f %f',
   [@CeilingSkyOffs[1],@CeilingSkyOffs[2]]);
  ReadFmt('MIP %f %f %f %f',
   [@MipMapDist[1],@MipMapDist[2],@MipMapDist[3],@MipMapDist[4]]);
  ReadFmt('LOD %f %f %f %f',
   [@LODDist[1],@LODDist[2],@LODDist[3],@LODDist[4]]);
end;
if MasterCMp='-' then MasterCMP:='';
end;

Procedure LoadGeometry;
var s,sf,vx,ns,nv,nsf:integer;
    sec:TJKSector;
    V:TJKVertex;
    surf:TJKSurface;
    asec,asf:integer;
    p,nsv:integer;
    st,w:string;
begin
 Progress.Msg:='Loading Geometry';

 ReadFmt('SECS %d',[@ns]);
 for s:=0 to ns-1 do
 begin

try
 DO_Progress;
except
 on Exception do if i=0 then;
end;

 sec:=NewSector;
 Sectors.Add(sec);
 With Sec do
 begin
  ReadFmt('%s %s %f %x %f %f %f %f %f %d',
   [@ColorMap,@Sound,@snd_vol,@Flags,@Ambient,@extra,@tint.dx,@tint.dy,@tint.dz,@Layer]);
  if ColorMap='-' then ColorMap:='';
  if Sound='-' then Sound:='';

  ReadFmt('VXS %d',[@nv]);
  for vx:=0 to nv-1 do
  begin
   V:=Sec.NewVertex;
   {Sec.Vertices.Add(v);}
   With V do ReadFmt('%f %f %f',[@x,@y,@z]);
  end;
  ReadFmt('SURFS %d',[@nsf]);
  for sf:=0 to nsf-1 do
  begin
   surf:=Sec.NewSurface;
   Sec.Surfaces.Add(Surf);
   With Surf do
   begin
    ReadFmt('%s %x %x %d %d %d %f %d %d %d %x %l %l',
     [@Material,@SurfFlags,@FaceFlags,@geo,@light,@tex,@extralight,@nsv,@asec,@asf,@AdjoinFlags,@uscale,@vscale]);
    if Material='-' then Material:='';
    Num:=asec shl 16+(asf and $FFFF);
    ReadS(st);
    p:=1;
    for vx:=0 to nsv-1 do
    begin
     p:=GetWord(st,p,w);
     ValInt(w,nv);
     AddVertex(Sec.Vertices[nv]);
     With TXVertices[vx] do
     begin
      p:=GetWord(st,p,w);
      ValSingle(w,u);
      p:=GetWord(st,p,w);
      ValSingle(w,v);
      p:=GetWord(st,p,w);
      ValSingle(w,Intensity);
     end;
    end;
    {Load RGB}
    for vx:=0 to nsv-1 do
    With TXVertices[vx] do
    begin
     p:=GetWord(st,p,w);
     if w='' then break;
     ValSingle(w,r);
     p:=GetWord(st,p,w);
     ValSingle(w,g);
     p:=GetWord(st,p,w);
     ValSingle(w,b);
    end;

   end;
  end;
 end;
 end;
 For s:=0 to Sectors.Count-1 do
 With Sectors[s] do
 for sf:=0 to Surfaces.Count-1 do

 With Surfaces[sf] do
 begin
  asec:=Num shr 16;
  asf:=Num and 65535;
 try
  if (ASec=65535) or (aSf=65535) then Adjoin:=nil
  else Adjoin:=Sectors[Asec].Surfaces[aSf];
 except
  on e:exception do PanMessage(mt_warning,e.message);
 end;
 end;
end;

Procedure LoadThings;
var t,nt,ns,p:integer;
    th:TThing;
    st,w:string;
    v:TTPLValue;
begin
 Progress.Msg:='Loading Things';
 
 ReadFmt('THS %d',[@nt]);
 For t:=0 to nt-1 do
 begin
  DO_Progress;
  th:=NewThing;
  Things.Add(th);
  With th do
   ReadFmt('%s %f %f %f %f %f %f %d %d',
   [@Name,@X,@Y,@Z,@PCH,@YAW,@ROL,@ns,@Layer]);
   if ns<0 then Th.Sec:=nil else th.Sec:=Sectors[ns];
  ReadS(st);
  p:=1;
  With th do
  While p<Length(st) do
  begin
   p:=GetWord(st,p,w);
   v:=TTPLValue.Create;
   Vals.Add(v);
   S2TPLVal(w,v);
  end;
 end;
end;

Procedure LoadCogs;
var cg:TCog;
    i,p,ncg:integer;
    st,w:string;
    v:TCOGvalue;
begin
Progress.Msg:='Loading Cogs';
ReadFmt('COGS %d',[@ncg]);
 For i:=0 to ncg-1 do
 begin
  DO_Progress;
  ReadS(st);
  cg:=TCog.Create;
  Cogs.Add(cg);
  p:=GetWord(st,1,w);
  cg.Name:=w;
  With cg do
  While p<length(st) do
  begin
   p:=GetWord(st,p,w);
   v:=TCOGValue.Create;
   Vals.Add(v);
   GetJedVal(w,v);
  end;
end;
end;

Procedure LoadLights;
var nl:integer;
    l:TJedLight;
    i:integer;
begin
 Progress.Msg:='Loading Lights';
 ReadFmt('LTS %d',[@nl]);
  for i:=0 to nl-1 do
  begin
   l:=NewLight;
   Lights.Add(l);
  With l do
    ReadFmt('%f %f %f %d %f %f %x %f %l %l %l %l',[@X,@Y,@Z,@Layer,@Intensity,@range,
     @Flags,@RGBRange,@RGBIntensity,@R,@g,@b]);

  if not Mots then
  begin
   l.rgbintensity:=l.intensity;
   l.r:=1;
   l.g:=1;
   l.b:=1;
  end;

  end;
end;

Procedure LoadLayers;
var i,nl:integer;
    s:string;
begin
 Progress.Msg:='Loading Layers';
 ReadFmt('LAYERS %d',[@nl]);
  for i:=0 to nl-1 do
  begin
   ReadS(s);
   Layers.Add(s);
  end;
end;

Procedure LoadLVis;
begin
 ReadS(LVisString);
end;

Procedure LoadHierarrchy;
var i,nl:integer;
    s:string;
    hnode:THNode;
begin
 Progress.Msg:='Loading Layers';
 ReadFmt('NODES %d',[@nl]);
  for i:=0 to nl-1 do
  begin
   ReadS(s);
   hnode:=THNode.Create;
   StringToHNode(s,hnode);
   h3donodes.Add(hnode);
  end;
end;

var tmpsurf:TJKSurface;

begin
 DecimalSeparator:='.';
 t:=TTextFile.CreateRead(OpenFileRead(f,0));
 Clear;
 {ReadS(St);}
 ppos:=0; fsize:=T.Fsize;
 Progress.Reset(pi_steps);

Try
 try
 jedver:=0.1;
 ReadFmt('JED %f',[@jedver]);

 While not t.eof do
 begin
  ReadS(st);
  p:=GetWord(st,1,w);
  if w='SEC:' then
  begin
   p:=GetWord(st,p,w);
   if w='HEADER' then LoadHeader
   else if w='GEO' then LoadGeometry
   else if w='THINGS' then LoadThings
   else if w='LIGHTS' then LoadLights
   else if w='LAYERS' then LoadLayers
   else if w='COGS' then LoadCogs
   else if w='MOTS' then Mots:=true
   else if w='LVIS' then LoadLVis
   else if w='HIERARCHYDEF' then LoadHierarrchy;
   SkipToEOS;
  end;

 end;

 finally
  RenumThings;
  RenumSecs;
  For i:=0 to Cogs.Count-1 do
  With Cogs[i] do
  begin
   for j:=0 to Vals.count-1 do Vals[j].Resolve;
  end;

  For s:=0 to Sectors.Count-1 do
  With Sectors[s] do
  begin
   Renumber;

   for j:=0 to Surfaces.Count-1 do
   begin
    tmpsurf:=Surfaces[j];
    tmpsurf.Recalc;
    if JedVer<0.81 then
    if IsTXFlipped(tmpsurf) then tmpsurf.surfFlags:=tmpsurf.surfFlags or SFF_Flip
    else tmpsurf.surfFlags:=tmpsurf.surfFlags and (not SFF_Flip);
    if JedVer<0.81 then tmpsurf.vscale:=tmpsurf.uscale;
   end;

  end;

  AddMissingLayers;
  Progress.Hide;
  t.Fclose;
 end;

except
 On e:Exception do Error(e.Message);
end;

end;

