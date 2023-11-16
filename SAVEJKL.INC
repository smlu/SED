
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
 if Abs(d)>0.00000000 then
 begin
  if d>0 then Result:=1 else Result:=-1;
  exit;
 end;
 d:=V1.v-V2.v;
 if Abs(d)>0.00000000 then
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

Procedure WriteListSection(const Name,cntName: String; ls:TStringList; const bufferSize: Integer);
var i:integer;
begin
 WriteS(Name);
 WriteFmt('%s %d',[CntName, bufferSize]);
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
 WriteFmt('World Gravity %.8f',[Gravity]);
 WriteFmt('Ceiling Sky Z %.8f',[CeilingSkyZ]);
 WriteFmt('Horizon Distance %.8f',[HorDistance]);
 WriteFmt('Horizon Pixels per Rev %.6f',[HorPixelsPerRev]);
 WriteFmt('Horizon Sky Offset %.8f %.8f',[HorSkyOffs[1],HorSkyOffs[2]]);
 WriteFmt('Ceiling Sky Offset %.8f %.8f',[CeilingSkyOffs[1],CeilingSkyOffs[2]]);
 if kind <> TProjectType.IJIM then
  WriteFmt('MipMap Distances %.6f %.6f %.6f %.6f',[MipMapDist[1],MipMapDist[2],MipMapDist[3],MipMapDist[4]]);
 WriteFmt('LOD Distances %.6f %.6f %.6f %.6f',[LODDist[1],LODDist[2],LODDist[3],LODDist[4]]);
 if kind = TProjectType.IJIM then
  WriteFmt('Fog %d %.8f %.8f %.8f %.8f %.8f %.8f',
    [byte(Fog.Enabled), Fog.color.r, Fog.color.g, Fog.color.b, Fog.color.a, Fog.FogStart, Fog.FogEnd])
 else
  begin
   WriteFmt('Perspective distance %.6f',[PerspDist]);
   WriteFmt('Gouraud distance %.6f',[GouraudDist]);
  end;
 WriteS('');
end;
end;

Function sl_add(ls:TStringList;const s:string):integer;
begin
 Result:=-1;
 if (s='') or (CompareText(s,'none')=0) then exit;
 Result:=ls.Add(s);
end;

// Function for compiling lists of used game assets
Procedure CompileLists(levelKind: TProjectType);
var
 newTpls,
 newCogs,
 tcogs,
 ttpls:TStringList;
 
Procedure AddFromTemplate(const Name:string);
var n,i:integer;tpl:TTemplate;
begin
 if UsedTpls.IndexOf(name)<>-1 then exit;
 n:=u_templates.Templates.IndexOfName(name);
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

 Ttpls.AddStrings(
  StaticTemplates[Integer(levelKind)]);

 Progress.Step;

 for i:=0 to Sectors.Count-1 do
 With Sectors[i] do
 For j:=0 to Surfaces.count-1 do
 With Surfaces[j] do sl_add(UsedMats, Material);

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

// Remove resources from static file
StringListFilter(UsedWavs, GetStaticSoundList(kind));
StringListFilter(UsedMats, GetStaticMatList(kind));
StringListFilter(Used3DOs, GetStatic3doList(kind));
StringListFilter(UsedSprs, GetStaticSprList(kind));
StringListFilter(UsedKeys, GetStaticKeyList(kind));
StringListFilter(UsedCogs, GetStaticCogScriptList(kind));

f3do.free;
newCogs.Free;
NewTpls.Free;
tcogs.Free;
Ttpls.Free;

end;   {CompileLists}

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
    Result := -1;
    if mat <> '' then
      begin
        var sml := GetStaticMatList(kind);
        if (sml <> nil) and StringListFind(sml, mat, Result) then
          Result := Result or StaticResIdxMask // Add static resource idx mask
        else
          Result := UsedMats.IndexOf(mat);
      end;
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
  WriteFmt('%d: %.8f %.8f %.8f',[v,x,y,z]);
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
 //txvx := TTXVertex.Create;
 for s:=0 to Sectors.Count-1 do
 With Sectors[s] do
   begin
   For sf:=0 to Surfaces.Count-1 do
    begin
      const uvsurf = Surfaces[sf];
      var matSize := TSize.Create(1,1);
      if (kind = IJIM) and (not uvsurf.Material.IsEmpty) then
       try
        matSize := uvsurf.GetMaterialSize;
       except on E:Exception do PanMessage(mt_warning,
          Format('Cannot load %s for sector %d surface %d: %s' + sLineBreak
            + '  Surface UV coords will be saved but are invalid!',
            [uvsurf.Material, uvsurf.Sector.num, uvsurf.num, e.message]));
       end;

      for v:=0 to uvsurf.txvertices.count-1 do
      begin
       txvx := TTXVertex.Create;
       txvx.Assign(uvsurf.TxVertices[v]);
       //txvx := uvsurf.TxVertices[v];
       txvx.u := txvx.u / matSize.Width;
       txvx.v := txvx.v / matSize.Height;
       uvsurf.TxVertices[v].num := AddTXVX(txvx);
      end;
    end;
   end;
 TxIdx.Free;

Progress.Msg:='Writing texture vertices';

 WriteFmt('World texture vertices %d',[TXVXList.Count]);
 WriteS('#num:	u:	v:');

 For v:=0 to TXVXList.Count-1 do
 begin
  txvx := TXVXList[v];
  with txvx do
    WriteFmt('%d: %.8f %.8f',[num,u,v]);
  txvx.Free;
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
    WriteFmt('%d: 0x%x %d %.2f',[i, flags, mirror, dist]);
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
      if kind = TProjectType.IJIM then
        st:=Sprintf('%d:'#9'%d'#9'0x%x'#9'0x%x'#9'%d'#9'%d'#9'%d'#9'%d'#9'%.8f %.8f %.8f %.8f'#9'%d',
          [num,nMat,SurfFlags,FaceFlags and (not FF_SF_FLIP),geo,light,tex,nadj,
          ExtraLight.r, ExtraLight.g, ExtraLight.b, ExtraLight.a,vertices.Count])
      else
        st:=Sprintf('%d:'#9'%d'#9'0x%x'#9'0x%x'#9'%d'#9'%d'#9'%d'#9'%d'#9'%.6f'#9'%d',
          [num,nMat,SurfFlags,FaceFlags and (not FF_SF_FLIP),geo,light,tex,nadj,RgbaToIntensity(ExtraLight),vertices.Count]);

      for v:=0 to Vertices.Count-1 do
      With Vertices[v] do
        st:=Concat(st,Format(#9'%d,%d',[num,TXVertices[v].num]));

      for v:=0 to Vertices.Count-1 do
      begin
        With TXVertices[v] do
        begin
         if kind = TProjectType.MOTS then
          st:=Concat(st,Sprintf(#9'%.4f'#9'%.4f'#9'%.4f'#9'%.4f',[color.a,color.r,color.g,color.b]))
         else if kind = TProjectType.IJIM then
          st:=Concat(st,Sprintf(#9'%.8f'#9'%.8f'#9'%.8f',[color.r,color.g,color.b]))
         else st:=Concat(st,Sprintf(#9'%.4f',[color.a]));
        end;
      end;
    //  With TXVertices[v] do
    //  begin
    //   if kind = TProjectType.MOTS then
    //    st:=Concat(st,Sprintf(#9'%.4f'#9'%.4f'#9'%.4f'#9'%.4f',[color.a,color.r,color.g,color.b]))
    //   else if kind = TProjectType.IJIM then
    //    st:=Concat(st,Sprintf(#9'%.4f'#9'%.4f'#9'%.4f',[color.r,color.g,color.b]))
    //   else st:=Concat(st,Sprintf(#9'%.4f',[color.a]));
    //  end;

      WriteS(st);
     end; // surface loop

 {Write normals}
 WriteS('');
 WriteS('#--- Surface normals ---');
 For s:=0 to Sectors.Count-1 do
 With Sectors[s] do
 for sf:=0 to surfaces.count-1 do
 With Surfaces[sf] do
 begin
  WriteFmt('%d:'#9'%.8f'#9'%.8f'#9'%.8f',[num,normal.dx,normal.dy,normal.dz])
 end;

end;

Procedure WriteSectors;
var s,sf,v:integer;
    (*cbox,*)bbox:TBox;
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
 WriteFmt('World sectors'#9'%d',[Sectors.Count]);
 WriteS('');
 nsurf:=0;
 For s:=0 to Sectors.Count-1 do
 With Sectors[s] do
 begin
  FindBBox(Sectors[s],bbox);
  FindBSphere(Sectors[s],CX,CY,CZ,Rad);

  Flags:=Flags and (not SECF_3DO);

//  if FindCollideBox(Sectors[s],bbox,CX,CY,CZ,cbox) then
//  Flags:=Flags or $1000 else Flags:=Flags and (not $1000);

  WriteFmt('SECTOR'#9'%d',[s]);
  WriteFmt('FLAGS'#9'0x%x',[Flags]);

  if kind = TProjectType.IJIM then
    begin
      WriteFmt('AMBIENT LIGHT'#9'%.8f %.8f %.8f',[Ambient.r, Ambient.g, Ambient.b]);
      WriteFmt('EXTRA LIGHT'#9'%.8f %.8f %.8f',[ExtraLight.r, ExtraLight.g, ExtraLight.b]);
      if not tint.IsZero() then
         WriteFmt('TINT'#9'%.8f %.8f %.8f',[Tint.r,Tint.g,Tint.b]);
      if not PointLight.Color.IsZero() then
        WriteFmt('AVERAGE LIGHT INTENSITY'#9'%.8f %.8f %.8f',
          [PointLight.Color.r, PointLight.Color.g, PointLight.Color.b]);
      if not PointLight.Position.IsZero() then
        WriteFmt('AVERAGE LIGHT POSITION'#9'%.8f %.8f %.8f',
          [PointLight.Position.x, PointLight.Position.y, PointLight.Position.z]);
       if not Math.IsZero(PointLight.minRange) or not Math.IsZero(PointLight.maxRange)  then
        WriteFmt('AVERAGE LIGHT FALLOFF'#9'%.8f %.8f',
          [PointLight.minRange, PointLight.maxRange]);
    end
  else
    begin
      WriteFmt('AMBIENT LIGHT'#9'%.6f',[RgbToIntensity(ambient)]);
      WriteFmt('EXTRA LIGHT'#9'%.6f',[RgbToIntensity(ExtraLight)]);
      WriteFmt('COLORMAP'#9'%d',[GetNCMP(ColorMap)]);
      WriteFmt('TINT'#9'%.6f'#9'%.6f'#9'%.6f',[Tint.r,Tint.g,Tint.b]);
    end;

  With BBox do
    WriteFmt('BOUNDBOX'#9'%.8f %.8f %.8f %.8f %.8f %.8f',[X1,Y1,Z1,X2,Y2,Z2]);

  if BitMaskTest(Flags, SECF_CollideBox) then With CollideBox do
    WriteFmt('COLLIDEBOX'#9'%.8f %.8f %.8f %.8f %.8f %.8f',[X1,Y1,Z1,X2,Y2,Z2]);

  if Sound <> '' then WriteFmt('SOUND'#9'%s %.6f',[Sound,soundVolume]);
  if (kind = TProjectType.IJIM) and not Thrust.IsZero() then
    WriteFmt('THRUST'#9'%.8f'#9'%.8f'#9'%.8f',[Thrust.x,Thrust.y,Thrust.z]);

  WriteFmt('CENTER'#9'%.8f %.8f %.8f',[CX,CY,CZ]);
  WriteFmt('RADIUS'#9'%.8f',[rad]);

  WriteFmt('VERTICES'#9'%d',[Vertices.Count]);
  for v:=0 to Vertices.Count-1 do
  WriteFmt('%d: %d',[v,Vertices[v].Num]);
  WriteFmt('SURFACES'#9'%d'#9'%d',[nsurf,Surfaces.Count]);
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
 WriteFmt('World cogs'#9'%d',[Cogs.Count+UsedCogs.Count]);
 WriteS('#Num	Script          Symbol values');
 for i:=0 to Cogs.Count-1 do
 With Cogs[i] do
 begin
  St:=Format('%d:'#9'%s',[i,Name]);
  for j:=0 to Vals.Count-1 do st:=Concat(st,#9,Vals[j].AsString);
  WriteS(st);
 end;

end;

Procedure WriteTemplates;
var i,j,n,ti:integer;
    wtpls:TStringList;
    tname,s:string;
    tp:TTemplate;
    vl:TTPLValue;
    norefs:boolean;
begin
  wtpls:=TStringList.Create;
  {Arrange templates in a proper order}

  {first set Objects[] to template object}
  for i := UsedTpls.Count-1 downto 0 do
    begin
    ti := u_templates.Templates.IndexOfName(UsedTpls[i]);
    if ti = -1 then
      begin
        PanMessageFmt(mt_warning,
          'Skipping template ''%s''. Template not found.', [UsedTpls[i]]);
        UsedTpls.delete(i);
        continue;
      end;
    UsedTpls.Objects[i] := u_templates.Templates[ti];
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
      n := UsedTpls.Count;
      For i := UsedTpls.Count - 1 downto 0 do
        begin
          tp := TTemplate(UsedTpls.Objects[i]);
          if (CompareText(tp.parent,'none') <> 0) and
            (wtpls.IndexOf(tp.parent) = -1) then
            begin
              if (UsedTpls.IndexOf(tp.parent) = -1) then
                begin
                  // Find parent template and insert it into used list
                  ti := u_templates.Templates.IndexOfName(tp.parent);
                  if ti = -1 then
                  begin
                    PanMessageFmt(mt_warning,
                      'Skipping template ''%s'' because parent template ''%s'' was''t found.', [tp.name, tp.parent]);
                    UsedTpls.Delete(i);
                    continue;
                  end;
                  UsedTpls.AddObject(tp.parent, u_templates.Templates[ti]);
                end;
              continue;
            end;

          norefs:=true;
          for j:=0 to tp.Vals.count-1 do
            begin
              vl := tp.Vals[j];
              if vl.atype<>at_tpl then continue;
              if (CompareText(vl.s,'none')<>0) and
                 (wtpls.IndexOf(vl.s) = -1) then begin norefs := false; break; end;
            end;

          if not norefs then continue;

          { if wtpls.IndexOf(tp.Parent)=-1 then continue;}
          wtpls.AddObject(tp.Name,tp);
          UsedTpls.Delete(i);
        end; // For i:=UsedTpls.Count-1 downto 0 do
      if n = UsedTpls.Count then break;
    end;

  {Write templates - UsedTpls}
  WriteS('');
  WriteFmt('World templates %d',[WTpls.Count]);
  WriteS('#Name:           Based On:        Params:');
  For i := 0 to WTpls.Count-1 do
    begin
      tp := TTemplate(Wtpls.Objects[i]);
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
 WriteFmt('World things %d',[Things.Count + 256]);
 WriteS('#num template:       name:         	X:		Y:		Z:		Pitch:		Yaw:		Roll:		Sector:');
 For i:=0 to Things.Count-1 do
 With Things[i] do
 begin
  if Sec=nil then snum:=-1 else snum:=Sec.Num;
  s:=Sprintf('%d: %s %s %.8f %.8f %.8f %.8f %.8f %.8f %d',[i,LowerCase(Name),LowerCase(Name),X,Y,Z,PCH,YAW,ROL,SNum]);
  if kind = TProjectType.MOTS then s:=s+' -1';
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
 WriteFmt('Editor lights %d',[Lights.Count]);
 WriteS('');

 if kind = JKDF2 then
  WriteS('#num:  flags:  layer:  x:    y:    z:    range:  intensity:')
 else
  WriteS('#num:  flags:  layer:  x:    y:    z:    range:  intensity:  r:    g:    b:');

 For i:=0 to Lights.Count-1 do
 With Lights[i] do
   if kind = JKDF2 then
    WriteFmt('%d: %x %d %.8f %.8f %.8f %.8f %.8f',
      [i, flags, layer, position.x,position.y,position.z, range, intensity])
   else
    WriteFmt('%d: %x %d %.8f %.8f %.8f %.8f %.8f %.8f %.8f %.8f',
      [i, flags, layer, position.x,position.y,position.z, range, intensity, color.r,color.g, color.b]);
end;

Procedure WriteLayers;
begin
  var lsecs := TObjectDictionary<integer, TList<integer>>.Create([doOwnsValues]);
  var lths := TObjectDictionary<integer, TList<integer>>.Create([doOwnsValues]);
  for var i := 0 to sectors.Count -1 do
    begin
      const l = sectors[i].Layer;
      if not lsecs.ContainsKey(l) then
         lsecs.Add(l, TList<integer>.Create);
      lsecs[l].Add(i);
    end;
  for var i := 0 to things.Count -1 do
    begin
      const l = things[i].Layer;
      if not lths.ContainsKey(l) then
         lths.Add(l, TList<integer>.Create);
      lths[l].Add(i);
    end;

  WriteFmt('Editor layers %d',[Layers.Count]);
  WriteS('');
  WriteS('#name secs things');
  For var i:=0 to Layers.Count-1 do
    begin
      WriteS(Layers[i]);
      var lix : string := '';

      // Sectors
      if lsecs.ContainsKey(i) then
        begin
          lix := Sprintf('%d:', [lsecs[i].Count]);
          for var j := 0 to lsecs[i].Count -1 do
            begin
              if (j > 0) and  (j mod 64 = 0) then lix := lix + sLineBreak;
              lix := Concat(lix, Sprintf(#9'%d',[lsecs[i][j]]));
            end;
        end
      else
        lix := '0:';

      // Things
      if lths.ContainsKey(i) then
        begin
          lix := lix + sLineBreak + Sprintf('%d:', [lths[i].Count]);
          for var j := 0 to lths[i].Count -1 do
            begin
              if (j > 0) and  (j mod 64 = 0) then lix := lix + sLineBreak;
              lix := Concat(lix, Sprintf(#9'%d',[lths[i][j]]));
            end;
        end
      else
         lix := lix + sLineBreak + '0:';

      WriteS(lix);
    end;

  WriteS('');
  WriteS('#Visibility');
  WriteS(LVisString);

  lsecs.Free;
  lths.Free;
end;

Const
 steps=10;
 list_steps=9;

begin {SaveToJKL}

 Assign(t,f); Rewrite(t);
 UsedWavs := TStringList.Create;
 UsedMats := TStringList.Create;
 UsedCmps := TStringList.Create;
 UsedCogs := TStringList.Create;
 UsedSnds := TStringList.Create;
 UsedPups := TStringList.Create;
 UsedKeys := TStringList.Create;
 UsedSprs := TStringList.Create;
 UsedPars := TStringList.Create;
 Used3DOs := TStringList.Create;
 UsedAIs  := TStringList.Create;
 UsedTpls := TStringList.Create;

 Progress.Reset(steps+list_steps);

 Try
  {Write header}
  //WriteS('#This file is created by Jed version '+JedVersion);
  //WriteS('#Jed is (C) by Alexei Novikov');
  //WriteS('');
  if kind <> TProjectType.IJIM then
    begin
      WriteS('SECTION: JK');
      WriteS('');
    end;

  WriteS('SECTION: COPYRIGHT');
  var logo: String := LECLogoJK;
  if kind = TProjectType.IJIM then logo := LECLogoIJIM;
  WriteS(logo);
  WriteS('');

  WriteHeaderSection;
  Progress.Msg:='Compiling lists';
  Progress.Step;
  CompileLists(kind);

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
  var matBufSize := UsedMats.Count;
  if kind = TProjectType.IJIM then matBufSize := matBufSize + 64;
  WriteFmt('World materials %d',[matBufSize]); // TODO: Remove mats of static container from UsedMats
  For i:=0 to UsedMats.Count-1 do
    begin
      if kind = TProjectType.IJIM then
        WriteFmt('%d: %s',[i,UsedMats[i]])
      else WriteFmt('%d: %s %.6f %.6f',[i,UsedMats[i],1.0,1.0]);
    end;
  WriteS('end');
  WriteS('');

  WriteS('SECTION: GEORESOURCE');
  if kind <> TProjectType.IJIM then
    begin
      WriteFmt('World Colormaps %d',[UsedCmps.Count]);
      For i:=0 to UsedCmps.Count-1 do WriteFmt('%d: %s',[i,UsedCmps[i]]);
      WriteS('');
    end;
  WriteGeometry;
  WriteS('');

  Progress.Msg:='Writing Sectors';
  Progress.Step;

  WriteS('Section: SECTORS');
  WriteSectors;

  Progress.Msg:='Writing Lists';
  Progress.Step;


  WriteListSection('Section: AICLASS'    ,'World AIClasses',    UsedAIs  , Max(UsedAIs.count  , 64));
  WriteListSection('Section: MODELS'     ,'World models',       Used3DOs , Max(Used3DOs.count , 1024));
  WriteListSection('Section: SPRITES'    ,'World sprites',      UsedSprs , Max(UsedSprs.count , 128));
  WriteListSection('Section: KEYFRAMES'  ,'World keyframes',    UsedKeys , Max(UsedKeys.count , 512));
  WriteListSection('Section: ANIMCLASS'  ,'World puppets',      UsedPups , Max(UsedPups.count , 64));
  WriteListSection('Section: Soundclass' ,'World soundclasses', UsedSnds , Max(UsedSnds.count , 128));
  WriteListSection('Section: cogscripts' ,'World scripts',      UsedCogs , Max(UsedCogs.count , 128));

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

  if kind = TProjectType.MOTS then
  begin
   WriteS('Section: ARCHLIGHTING');
   WriteS('Num ArchObjects 0');
   WriteS('end');
   WriteS('');
  end; 

  if SaveEditorLights then
    begin
      WriteS('Section: Lights');
      WriteLights;
      WriteS('end');
      WriteS('');
    end;

  if SaveEditorLayers then
  begin
    WriteS('Section: Layers');
    WriteLayers;
    WriteS('end');
    WriteS('');
  end;

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
