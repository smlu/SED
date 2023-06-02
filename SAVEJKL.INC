
Function CompareVX(v1,v2:TVertex):integer;
var d:double;
begin
 d:=V1.X-V2.X;
 if Abs(d)>CloseEnough then
 begin
  if d>0 then Result:=1 else Result:=-1;
  exit;
 end;
 d:=V1.Y-V2.Y;
 if Abs(d)>CloseEnough then
 begin
  if d>0 then Result:=1 else Result:=-1;
  exit;
 end;
 d:=V1.Z-V2.Z;
 if Abs(d)>CloseEnough then
 begin
  if d>0 then Result:=1 else Result:=-1;
  exit;
 end;
 Result:=0;
end;

Function CompareTXVX(v1,v2:TTXVertex):integer;
var d:double;
begin
 d:=V1.u-V2.u;
 if Abs(d)>0.01 then
 begin
  if d>0 then Result:=1 else Result:=-1;
  exit;
 end;
 d:=V1.v-V2.v;
 if Abs(d)>0.01 then
 begin
  if d>0 then Result:=1 else Result:=-1;
  exit;
 end;
 Result:=0;
end;


function FindVX(vs:TVertices;v:Tvertex; var Index: Integer): Boolean;
var
  L, H, I, C: Integer;
begin
  Result := False;
  L := 0;
  H := Vs.Count - 1;
  while L <= H do
  begin
    I := (L + H) shr 1;
    C := CompareVX(vs[i],v);
    if C < 0 then L := I + 1 else
    begin
      H := I - 1;
      if C = 0 then
      begin
        Result := True;
        L := I;
      end;
    end;
  end;
  Index := L;
end;


function FindTXVX(vs:TTXVertices;v:TTXvertex; var Index: Integer): Boolean;
var
  L, H, I, C: Integer;
begin
  Result := False;
  L := 0;
  H := Vs.Count - 1;
  while L <= H do
  begin
    I := (L + H) shr 1;
    C := CompareTXVX(vs[i],v);
    if C < 0 then L := I + 1 else
    begin
      H := I - 1;
      if C = 0 then
      begin
        Result := True;
        L := I;
      end;
    end;
  end;
  Index := L;
end;


Procedure TJKLevel.SaveToJKL(F:TFileName);
var t:textfile;
    UsedWavs,
    UsedMats,
    UsedCmps,
    UsedCogs,
    UsedSnds,
    UsedPups,
    UsedKeys,
    UsedSprs,
    UsedPars,
    Used3DOs,
    UsedTpls,
    UsedAIs:TStringList;
    i:integer;


Procedure WriteFmt(const fmt:string;vals:array of const);
begin
 Writeln(t,Sprintf(fmt,vals));
end;

Procedure WriteS(const s:string);
begin
 Writeln(t,s);
end;

Procedure WriteListSection(const Name,cntName:String;ls:TStringList);
var i:integer;
begin
 WriteS(Name);
 WriteFmt('%s %d',[CntName,ls.count]);
 For i:=0 to ls.count-1 do WriteFmt('%d: %s',[i,ls[i]]);
 WriteS('end');
 WriteS('');
end;

Procedure WriteHeaderSection;
begin
With Header do
begin
 WriteS('SECTION: HEADER');
 WriteFmt('Version %d',[version]);
 WriteFmt('World Gravity %.6f',[Gravity]);
 WriteFmt('Ceiling Sky Z %.6f',[CeilingSkyZ]);
 WriteFmt('Horizon Distance %.6f',[HorDistance]);
 WriteFmt('Horizon Pixels per Rev %.6f',[HorPixelsPerRev]);
 WriteFmt('Horizon Sky Offset %.6f %.6f',[HorSkyOffs[1],HorSkyOffs[2]]);
 WriteFmt('Ceiling Sky Offset %.6f %.6f',[CeilingSkyOffs[1],CeilingSkyOffs[2]]);
 WriteFmt('MipMap Distances %.6f %.6f %.6f %.6f',[MipMapDist[1],MipMapDist[2],MipMapDist[3],MipMapDist[4]]);
 WriteFmt('LOD Distances %.6f %.6f %.6f %.6f',[LODDist[1],LODDist[2],LODDist[3],LODDist[4]]);
 WriteFmt('Perspective distance %.6f',[PerspDist]);
 WriteFmt('Gouraud distance %.6f',[GouraudDist]);
 WriteS('');
end;
end;

Function sl_add(ls:TStringList;const s:string):integer;
begin
 Result:=-1;
 if (s='') or (CompareText(s,'none')=0) then exit;
 Result:=ls.Add(s);
end;

Procedure CompileLists;
var
 newTpls,
 newCogs,
 tcogs,
 ttpls:TStringList;
 
Procedure AddFromTemplate(const Name:string);
var n,i:integer;tpl:TTemplate;
begin
 if UsedTpls.IndexOf(name)<>-1 then exit;
 n:=Templates.IndexOfName(name);
 if n=-1 then exit;
 With u_templates.Templates[n] do
 for i:=0 to vals.count-1 do
 begin
  if CompareText(parent,'none')<>0 then sl_add(NewTpls,parent);
 With vals[i] do
 begin
  case atype of
   at_unk: ;
   at_mat: sl_add(UsedMats,s);
   at_cog: sl_add(NewCogs,s);
   at_snd: sl_add(UsedSnds,s);
   at_pup: sl_add(UsedPups,s);
   at_spr: sl_add(UsedSprs,s);
   at_par: sl_add(UsedPars,s);
   at_3do: sl_add(Used3dos,s);
   at_ai: sl_add(UsedAIs,s);
   at_tpl: sl_add(NewTpls,s);
  end;
 { if name='material' then sl_add(UsedMats,s)
  else if name='cog'  then sl_add(NewCogs,s)
  else if name='soundclass'  then sl_add(UsedSnds,s)
  else if name='puppet'  then sl_add(UsedPups,s)
  else if name='sprite'  then sl_add(UsedSprs,s)
  else if name='particle'  then sl_add(UsedPars,s)
  else if name='model3d'  then sl_add(Used3dos,s)
  else if name='aiclass'  then sl_add(UsedAIs,s)
  else if name='creatething'  then sl_add(NewTpls,s)
  else if name='explode'  then sl_add(NewTpls,s)
  else if name='fleshhit'  then sl_add(NewTpls,s)
  else if name='weapon'  then sl_add(NewTpls,s)
  else if name='weapon2'  then sl_add(NewTpls,s)
  else if name='debris'  then sl_add(NewTpls,s)
  else if name='trailthing'  then sl_add(NewTpls,s);}
 end;
 end;
end;

Procedure AddFromPlacedCogs;
var i,j:integer;
begin
  for i:=0 to COgs.Count-1 do
  With Cogs[i] do
  For j:=0 to Vals.count-1 do
  With Vals[j] do
   case Cog_type of
    ct_wav: sl_add(UsedWavs,s);
    ct_tpl: sl_add(NewTpls,s);
    ct_3do: sl_add(Used3dos,s);
    ct_key: sl_add(UsedKeys,s);
    {ct_cog: sl_add(NewCogs,s);}
    ct_mat: sl_add(UsedMats,s);
    ct_ai: sl_add(UsedAis,s);
  end;
end;

Procedure AddFromCog(const name:string);
var i:integer;
    cf:TCOgFile;
begin
 if UsedCogs.IndexOf(Name)<>-1 then exit;
 cf:=TCogFile.Create;
 try
  cf.Load(name);
  for i:=0 to cf.Count-1 do
  With Cf[i] do
   case Cog_type of
    ct_wav: sl_add(UsedWavs,s);
    ct_tpl: sl_add(NewTpls,s);
    ct_3do: sl_add(Used3dos,s);
    ct_key: sl_add(UsedKeys,s);
    ct_cog: sl_add(NewCogs,s);
    ct_mat: sl_add(UsedMats,s);
    ct_ai: sl_add(UsedAis,s);
  end;

 except
  On Exception do;
 end;
 cf.Free;
end;

var i,j:integer; newitems:integer;
    pupf:TPUPFile;
    sndf:TSndFile;
    f3do:T3DOFile;
    idat:TItemsDatFile;
    mdat:TModelsDatFile;

Procedure InitSL(sl:TStringList);
begin
 sl.Clear;
 sl.Sorted:=true;
 sl.duplicates:=dupIgnore;
end;

begin  {CompileLists}
 {Compile lists of stuff - WAV, SND, COg, etc}
 InitSL(UsedWavs); InitSL(UsedMats);
 InitSL(UsedCogs); InitSL(UsedSnds); InitSL(UsedPups);
 InitSL(UsedKeys); InitSL(UsedSprs); InitSL(Used3DOs);
 InitSL(UsedTpls); InitSL(UsedAIs); InitSL(UsedPars);

 UsedWavs.Sorted:=true; InitSL(UsedMats);
 UsedCogs.Sorted:=true; InitSL(UsedSnds);
 UsedKeys.Sorted:=true; InitSL(UsedSprs);
 UsedTpls.Sorted:=true; InitSL(UsedAIs);

 newTpls:=TStringList.Create; newTpls.Sorted:=true;
 newCogs:=TStringList.Create; newCogs.Sorted:=true;
 tcogs:=TStringList.Create;  tcogs.Sorted:=true;
 ttpls:=TStringList.Create;  ttpls.Sorted:=true;

 Ttpls.Add('+twinkle');
 Ttpls.Add('+sspks_wall');
 Ttpls.Add('+ssparks_wall');
 Ttpls.Add('+sspks_blood');
 Ttpls.Add('+ssparks_blood');
 Ttpls.Add('+sspks_saber');
 Ttpls.Add('+ssparks_saber');
 Ttpls.Add('+backpack');
 Ttpls.Add('ghostcam');

 Progress.Step;

 for i:=0 to Sectors.Count-1 do
 With Sectors[i] do
 For j:=0 to Surfaces.count-1 do
 With Surfaces[j] do sl_add(UsedMats,Material);

// UsedCMPs:=TStringList.Create;
 if MasterCMP<>'' then UsedCmps.Add(MasterCMP);

 for i:=0 to Sectors.Count-1 do
 With Sectors[i] do
 begin
  sl_add(UsedWavs,Sound);
  if UsedCMPs.IndexOf(ColorMap)=-1 then
     UsedCmps.Add(ColorMap);
 end;


 Progress.Step;

 For i:=0 to Cogs.count-1 do sl_add(TCogs,Cogs[i].Name);
 For i:=0 to things.count-1 do sl_add(TTpls,Things[i].Name);
 AddFromPlacedCogs;

 Progress.Step;

{ mdat:=TModelsDatFile.Create;
 mdat.Load('models.dat');
 for i:=0 to mDat.Snds.Count-1 do
 begin
  sndf:=TSndFile.Create;
  Sndf.Load(mDat.Snds[i]);
  for j:=0 to Sndf.Wavs.Count-1 do sl_add(UsedWavs,Sndf.Wavs[j]);
 end;
 mdat.free;
 Progress.Step;}

 idat:=TItemsDatFile.Create;
 idat.Load('items.dat');
 for i:=0 to iDat.COgs.Count-1 do AddFromCog(iDat.Cogs[i]);
 idat.Free;
 tCogs.AddStrings(newCogs);
 tTpls.AddStrings(newTpls);

 Progress.Step;

 Repeat
  newCogs.Clear; NewTpls.clear;

  for i:=0 to TCogs.Count-1 do begin AddFromCog(tCogs[i]); sl_add(UsedCogs,tCogs[i]); end;
  for i:=0 to TTpls.count-1 do begin AddFromTemplate(TTpls[i]); sl_add(UsedTpls,tTpls[i]); end;
  tCogs.Clear;
  tTpls.Clear;
  tCogs.AddStrings(newCogs);
  ttpls.AddStrings(newTpls);
 until (NewCogs.count=0) and (NewTpls.Count=0);

 Progress.Step;

 pupf:=TPUPFile.Create;
 for i:=0 to UsedPups.Count-1 do
 begin
  try
   pupf.Load(UsedPups[i]);
   for j:=0 to pupf.keys.count-1 do sl_Add(UsedKeys,pupf.keys[j]);
  except
   on Exception do;
  end;
 end;
 pupf.Free;
 Progress.Step;

 sndf:=TSNDFile.Create;
 for i:=0 to UsedSnds.Count-1 do
 begin
  try
   sndf.Load(UsedSnds[i]);
   for j:=0 to sndf.wavs.count-1 do sl_Add(UsedWavs,sndf.wavs[j]);
  except
   on Exception do;
  end;
 end;
 sndf.Free;

 Progress.Step;

for i:=0 to UsedSprs.count-1 do sl_Add(UsedMats,GetMatFromSPR(UsedSprs[i]));
for i:=0 to UsedPars.count-1 do sl_Add(UsedMats,GetMatFromPAR(UsedPars[i]));


f3do:=T3DOFile.Create;
For i:=0 to Used3DOs.Count-1 do
begin
 f3do.Load(Used3DOs[i]);
 for j:=0 to f3do.Mats.Count-1 do sl_Add(UsedMats,f3do.Mats[j]);
end;
f3do.free;
newCogs.Free;
NewTpls.Free;
tcogs.Free;
Ttpls.Free;

end;

Procedure WriteGeometry;
var VxIdx,VXList:TVertices;
    TXIdx,TXVXList:TTXVertices;

{Function AddVX(v:TVertex):integer;
var i,n:integer;s:string;
begin
 s:=Format('%.7g %.7g %.7g',[v.x,v.y,v.z]);
 n:=VxIdx.IndexOf(s);
 if n<>-1 then
 begin
  Result:=Integer(VxIdx.Objects[n]);
  exit;
 end;
  Result:=VXList.Add(v);
  VxIdx.AddObject(s,TObject(Result));
end;}

Function AddVX(v:TVertex):integer;
var i,n:integer;s:string;
begin
 if FindVX(VxIdx,v,n) then
 begin
  Result:=VxIdx[n].num;
  exit;
 end;
  Result:=VXList.Add(v);
  VxIdx.Insert(n,v);
  v.Num:=result;
end;

Function AddTXVX(tv:TTXVertex):integer;
var i,n,vx:integer;s:string;
begin
 if FindTXVX(TxIdx,tv,n) then
 begin
  Result:=TxIdx[n].num;
  exit;
 end;
  Result:=TXVXList.Add(tv);
  TxIdx.Insert(n,tv);
  tv.Num:=result;
end;


Function GetMatN(const mat:string):integer;
begin
 if mat='' then Result:=-1 else Result:=UsedMats.IndexOf(mat);
end;

Var    i,s,sf,v:integer;
       surf:TJKSurface;
       vx:TJKVertex;
       txvx:TTXVertex;
       nsurf:integer;
       Adjoins:TList;
       adj:TAdjoin;
       st:string;
       cx,cy,cz:double;
       sec:TJKSector;
begin {WriteGeometry}
 {Write VXs, TX VXs, Surfs, normals}
 {Compile list of unique vertices}

 Progress.Msg:='Pooling vertices';
 Progress.Step;


 VXList:=TVertices.Create;
 TXVXList:=TTXVertices.Create;

 VxIdx:=TVertices.Create;
 TxIdx:=TTXVertices.Create;

 for s:=0 to Sectors.Count-1 do
 With Sectors[s] do
 begin
  for v:=0 to vertices.count-1 do
  begin
   vx:=Vertices[v];
   vx.num:=AddVX(vx);
  end;
 end;

 VxIdx.Free;

 Progress.Msg:='Writing vertices';

 WriteFmt('World vertices %d',[VXList.Count]);
 WriteS('#num:	vertex:');
 for v:=0 to VXList.Count-1 do
 With VXList[v] do
  WriteFmt('%d: %.6f %.6f %.6f',[v,x,y,z]);
 WriteS('');
 VXList.Free;

 {Number surfaces}
 nsurf:=0;
 For s:=0 to Sectors.Count-1 do
 With Sectors[s] do
 begin
 for Sf:=0 to Surfaces.count-1 do
  With Surfaces[sf] do
  begin
   Num:=nsurf; inc(nsurf);
  end;
 end;

 Progress.Msg:='Pooling texture vertices';
 Progress.Step;


 {Pool texture vertices}
 for s:=0 to Sectors.Count-1 do
 With Sectors[s] do
 begin
 For sf:=0 to Surfaces.Count-1 do
  With SUrfaces[sf] do
  for v:=0 to txvertices.count-1 do
  begin
   txvx:=TxVertices[v];
   txvx.num:=AddTXVX(txvx);
  end;
 end;

 TxIdx.Free;

Progress.Msg:='Writing texture vertices';

 WriteFmt('World texture vertices %d',[TXVXList.Count]);
 WriteS('#num:	u:	v:');

 For v:=0 to TXVXList.Count-1 do
 With TXVXList[v] do
 begin
  WriteFmt('%d: %.2f %.2f',[num,u,v]);
 end;
 WriteS('');
 TXVxList.Free;

 Adjoins:=TList.Create;
 For s:=0 to Sectors.Count-1 do
 With Sectors[s] do
 for Sf:=0 to Surfaces.count-1 do
 With Surfaces[sf] do
 begin

   if Adjoin<>nil then
   begin
    Adj:=TAdjoin.Create;
    nadj:=Adjoins.Add(Adj);
    Adj.Surf:=num;
    Adj.Flags:=AdjoinFlags and $7FFFFFFF;
    Sec:=Sectors[s];
    FindCenter(Sec,cx,cy,cz);
    Adj.Dist:=DistToPlane(Surfaces[sf],cx,cy,cz);
   end
   else nadj:=-1;
 end;

 For s:=0 to Sectors.Count-1 do
 With Sectors[s] do
 for Sf:=0 to Surfaces.count-1 do
 With Surfaces[sf] do
 begin
   if Adjoin<>nil then
   begin
    TAdjoin(Adjoins[nadj]).mirror:=Adjoin.nadj;
   end;
 end;



 WriteFmt('World adjoins %d',[adjoins.count]);
 WriteS('#num:	flags:	mirror:	dist:');
 for i:=0 to Adjoins.Count-1 do
 With TAdjoin(Adjoins[i]) do
 begin
  WriteFmt('%d: 0x%x %d %.2f',[i,flags,mirror,dist]);
  free;
 end;
 Adjoins.free;
 WriteS('');
 WriteFmt('World surfaces %d',[nsurf]);
 WriteS('#num:   mat:     surfflags:	faceflags:	geo:	light:	tex:	adjoin:	extralight:	nverts:	vertices:			intensities:');
 For s:=0 to Sectors.Count-1 do
 With Sectors[s] do
 for sf:=0 to surfaces.count-1 do
 With Surfaces[sf] do
 begin
  nmat:=GetMatN(Material);
  st:=Sprintf('%d:'#9'%d'#9'0x%x'#9'0x%x'#9'%d'#9'%d'#9'%d'#9'%d'#9'%.6f'#9'%d',
  [num,nMat,SurfFlags and (not SFF_Flip),FaceFlags,geo,light,tex,nadj,extralight,vertices.Count]);

  for v:=0 to Vertices.Count-1 do
  With Vertices[v] do
    st:=Concat(st,Format(' %d,%d',[num,TXVertices[v].num]));

  for v:=0 to Vertices.Count-1 do
  With TXVertices[v] do
  begin
   if mots then st:=Concat(st,Sprintf(' %.4f %.4f %.4f %.4f',[Intensity,r,g,b]))
   else st:=Concat(st,Sprintf(' %.4f',[Intensity]));
  end;

  WriteS(st);
 end;

 {Write normals}
 WriteS('');
 WriteS('#--- Surface normals ---');
 For s:=0 to Sectors.Count-1 do
 With Sectors[s] do
 for sf:=0 to surfaces.count-1 do
 With Surfaces[sf] do
 begin
  WriteFmt('%d: %.6f %.6f %.6f',[num,normal.dx,normal.dy,normal.dz]);
 end;

end;

Procedure WriteSectors;
var s,sf,v:integer;
    cbox,bbox:TBox;
    CX,CY,CZ,Rad:double;
    nsurf:integer;

Function GetNCMP(const name:string):integer;
begin
 Result:=UsedCmps.IndexOf(name);
 if Result<0 then
 begin
  result:=0;
  PanMessageFmt(mt_warning,'Invalid colormap in sec %d',[s]);
 end;
end;

begin
 {Write sectors}
 WriteS('');
 WriteFmt('World sectors %d',[Sectors.Count]);
 WriteS('');
 nsurf:=0;
 For s:=0 to Sectors.Count-1 do
 With Sectors[s] do
 begin
  FindBBox(Sectors[s],bbox);
  FindBSphere(Sectors[s],CX,CY,CZ,Rad);

  Flags:=Flags and (not SF_3DO);

  if FindCollideBox(Sectors[s],bbox,CX,CY,CZ,cbox) then
  Flags:=Flags or $1000 else Flags:=Flags and (not $1000);

  WriteFmt('SECTOR %d',[s]);
  WriteFmt('FLAGS 0x%x',[Flags]);
  WriteFmt('AMBIENT LIGHT %.6f',[ambient]);
  WriteFmt('EXTRA LIGHT %.6f',[extra]);
  WriteFmt('COLORMAP %d',[GetNCMP(ColorMap)]);
  WriteFmt('TINT %.6f %.6f %.6f',[tint.dx,tint.dy,Tint.dz]);
 With BBox do
  WriteFmt('BOUNDBOX %.6f %.6f %.6f %.6f %.6f %.6f',[X1,Y1,Z1,X2,Y2,Z2]);

  if (Flags and $1000)<>0 then With CBox do
    WriteFmt('COLLIDEBOX %.6f %.6f %.6f %.6f %.6f %.6f',[X1,Y1,Z1,X2,Y2,Z2]);

  if Sound<>'' then WriteFmt('SOUND %s %.6f',[Sound,Snd_vol]);
  WriteFmt('CENTER %.6f %.6f %.6f',[CX,CY,CZ]);
  WriteFmt('RADIUS %.6f',[rad]);

  WriteFmt('VERTICES %d',[Vertices.Count]);
  for v:=0 to Vertices.Count-1 do
  WriteFmt('%d: %d',[v,Vertices[v].Num]);
  WriteFmt('SURFACES %d %d',[nsurf,Surfaces.Count]);
  inc(nsurf,Surfaces.Count);
  WriteS('');  
 end;
end;

Procedure WriteCogs;
var i,j:integer;
    st:string;
begin
 {Write Placed cogs}
 WriteS('');
 WriteFmt('World cogs %d',[Cogs.Count+UsedCogs.Count]);
 WriteS('#Num	Script          Symbol values');
 for i:=0 to Cogs.Count-1 do
 With Cogs[i] do
 begin
  St:=Format('%d: %s',[i,Name]);
  for j:=0 to Vals.Count-1 do st:=Concat(st,' ',Vals[j].AsString);
  WriteS(st);
 end;

end;

Procedure WriteTemplates;
var i,j,n:integer;
    wtpls:TStringList;
    tname,s:string;
    tp:TTemplate;
    vl:TTPLValue;
    norefs:boolean;
begin
 wtpls:=TStringList.Create;
 {Arrange templates in a proper order}

 {first set Objects[] to template object}
 for i:=UsedTpls.Count-1 downto 0 do
 begin
  n:=Templates.IndexOfName(UsedTpls[i]);
  if n=-1 then
  begin
   UsedTpls.delete(i);
   continue;
  end;
  UsedTpls.Objects[i]:=Templates[n];
 end;
 {First put base level objects in list}

{ For i:=UsedTpls.Count-1 downto 0 do
 begin
  tp:=TTemplate(UsedTpls.Objects[i]);
  if CompareText(tp.Parent,'none')<>0 then continue;
  wtpls.AddObject(tp.Name,tp);
  UsedTpls.Delete(i);
 end;}

 While UsedTpls.Count>0 do
 begin
  n:=UsedTpls.Count;
  For i:=UsedTpls.Count-1 downto 0 do
  begin
   tp:=TTemplate(UsedTpls.Objects[i]);
   if (CompareText(tp.parent,'none')<>0) and
      (wtpls.IndexOf(tp.parent)=-1) then continue;

   norefs:=true;
   for j:=0 to tp.Vals.count-1 do
   begin
    vl:=tp.Vals[j];
    if vl.atype<>at_tpl then continue;
    if (CompareText(vl.s,'none')<>0) and
       (wtpls.IndexOf(vl.s)=-1) then begin norefs:=false; break; end;
   end;

   if not norefs then continue;

{   if wtpls.IndexOf(tp.Parent)=-1 then continue;}
   wtpls.AddObject(tp.Name,tp);
   UsedTpls.Delete(i);
  end;
  if n=UsedTpls.Count then break;
 end;


 {Write templates - UsedTpls}
 WriteS('');
 WriteFmt('World templates %d',[WTpls.Count]);
 WriteS('#Name:           Based On:        Params:');
 For i:=0 to WTpls.Count-1 do
 begin
  tp:=TTemplate(Wtpls.Objects[i]);
  WriteS(Tp.GetAsString);
 end;
 wtpls.Free;
end;

Procedure WriteThings;
var i,j:integer;
    s:string;
    v:TValue;
    snum:integer;
begin
 {Write things}
 RenumSecs;
 WriteFmt('World things %d',[Things.Count+200]);
 WriteS('#num template:       name:         	X:		Y:		Z:		Pitch:		Yaw:		Roll:		Sector:');
 For i:=0 to Things.Count-1 do
 With Things[i] do
 begin
  if Sec=nil then snum:=-1 else snum:=Sec.Num;
  s:=Sprintf('%d: %s %s %.6f %.6f %.6f %.6f %.6f %.6f %d',[i,LowerCase(Name),LowerCase(Name),X,Y,Z,PCH,YAW,ROL,SNum]);
  if mots then s:=s+' -1';
  for j:=0 to Vals.Count-1 do
  begin
   v:=Vals[j];
   s:=Concat(s,' ',LowerCase(v.Name),'=',v.AsString);
  end;
  WriteS(s);
 end;
end;

Procedure WriteLights;
var i,j:integer;
    s:string;
    v:TValue;
    snum:integer;
begin
 {Write things}
 WriteFmt('Editor lights %d',[Lights.Count]);
 WriteS('');
 WriteS('#num:	type:	X:		Y:		Z:		intensity:	range:');
 For i:=0 to Lights.Count-1 do
 With Lights[i] do
  WriteFmt('%d: 0 %.6f %.6f %.6f %.6f %.6f',[i,x,y,z,intensity,range]);
end;


Const
 steps=10;
 list_steps=9;

begin {SaveToJKL}

 Assign(t,f); Rewrite(t);
 UsedWavs:=TStringList.Create;
 UsedMats:=TStringList.Create;
 UsedCmps:=TStringList.Create;
 UsedCogs:=TStringList.Create;
 UsedSnds:=TStringList.Create;
 UsedPups:=TStringList.Create;
 UsedKeys:=TStringList.Create;
 UsedSprs:=TStringList.Create;
 UsedPars:=TStringList.Create;
 Used3DOs:=TStringList.Create;
 UsedAIs:=TStringList.Create;
 UsedTpls:=TStringList.Create;

 Progress.Reset(steps+list_steps);

 Try
  {Write header}
  WriteS('#This file is created by Jed version '+JedVersion);
  WriteS('#Jed is (C) by Alexei Novikov');
  WriteS('');
  WriteS('SECTION: JK');
  WriteS('');
  WriteS('SECTION: COPYRIGHT');
  WriteS(LECLogo);
  WriteS('');
  WriteHeaderSection;
  Progress.Msg:='Compiling lists';
  Progress.Step;
  CompileLists;

  Progress.Msg:='Writing sounds';
  Progress.Step;

  WriteS('SECTION: SOUNDS');
  WriteFmt('World sounds %d',[UsedWavs.Count]);
  For i:=0 to UsedWavs.Count-1 do WriteS(UsedWavs[i]);
  WriteS('end');
  WriteS('');

  Progress.Msg:='Writing materials';
  Progress.Step;

  WriteS('SECTION: MATERIALS');
  WriteFmt('World materials %d',[UsedMats.Count]);
  For i:=0 to UsedMats.Count-1 do WriteFmt('%d: %s %.6f %.6f',[i,UsedMats[i],1.0,1.0]);
  WriteS('end');
  WriteS('');

  WriteS('SECTION: GEORESOURCE');
  WriteFmt('World Colormaps %d',[UsedCmps.Count]);
  For i:=0 to UsedCmps.Count-1 do WriteFmt('%d: %s',[i,UsedCmps[i]]);
  WriteS('');
  WriteGeometry;
  WriteS('');

  Progress.Msg:='Writing Sectors';
  Progress.Step;

  WriteS('Section: SECTORS');
  WriteSectors;

  Progress.Msg:='Writing Lists';
  Progress.Step;


  WriteListSection('Section: AICLASS','World AIClasses',UsedAIs);
  WriteListSection('Section: MODELS','World models',Used3DOs);
  WriteListSection('Section: SPRITES','World sprites',UsedSprs);
  WriteListSection('Section: KEYFRAMES','World keyframes',UsedKeys);
  WriteListSection('Section: ANIMCLASS','World puppets',UsedPups);
  WriteListSection('Section: Soundclass','World soundclasses',UsedSnds);
  WriteListSection('Section: cogscripts','World scripts',UsedCogs);

  WriteS('Section: cogs');
  WriteCogs;
  WriteS('end');
  WriteS('');

  Progress.Msg:='Writing Templates';
  Progress.Step;


  WriteS('Section: TEMPLATES');
  WriteTemplates;
  WriteS('end');
  WriteS('');

  Progress.Msg:='Writing Things';
  Progress.Step;


  WriteS('Section: Things');
  WriteThings;
  WriteS('end');
  WriteS('');

  if Mots then
  begin
   WriteS('Section: ARCHLIGHTING');
   WriteS('Num ArchObjects 0');
   WriteS('end');
   WriteS('');
  end; 

  WriteS('Section: Lights');
  WriteLights;
  WriteS('end');
  WriteS('');



 finally
  RenumSecs;
  RenumThings;
  for i:=0 to Sectors.count-1 do Sectors[i].Renumber;
  Progress.Hide;
  CloseFile(t);
  UsedWavs.Free;
  UsedMats.Free;
  UsedCmps.Free;
  UsedCogs.Free;
  UsedSnds.Free;
  UsedPups.Free;
  UsedKeys.Free;
  UsedSprs.Free;
  UsedPars.Free;
  Used3DOs.Free;
  UsedAIs.Free;
  UsedTpls.Free;
 end;
end;
