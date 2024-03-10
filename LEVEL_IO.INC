Type
TTempVertex = class
  x,y,z: Double;
end;

TTempTXVertex = class
  u,v: Single;
end;

TAdjoin = class
   mirror,surf: Integer;
   dist: Double;
   flags: LongInt;
end;

TMatDims = class
 dim1,dim2: Double;
end;

TVXIntensity = class
 r,g,b,
 a: Single; // a - alpha or light intensity
end;

TTempSurf = class
  nmat: Integer;
  surfflags,faceflags: LongInt;
  geo,light,nadj,tex: Integer;
  extralight: TColorF;
  VXS: TIntList;
  TVXs: TIntList;
  intensities: TList;
  constructor Create;
  destructor Destroy;override;
end;

constructor TTempSurf.Create;
begin
 VXS  := TIntList.Create;
 TVXS := TIntList.Create;
 intensities := TList.Create;
end;

destructor TTempSurf.Destroy;
var i:integer;
begin
 VXS.Free;
 TVXS.Free;
 for i:=0 to Intensities.count-1 do
  TVXIntensity(Intensities[i]).Free;
 Intensities.Free;
end;

Procedure TJKLevel.LoadFromJKL(filename: TFileName);
var t: TTextFile; cpos:word; s,w1,w2: string;
    cur_line: Integer;
    a,n,nx: Integer;
    EndOfSection:boolean;
    CurSection: string;
    GVXList,GSurfList,
    GTXVXList,GAdjoins: TList;
    mats: TStringList;
    cmps: TStringList;
    vr,vg,vb: Single;

Procedure GetNextLine(var s: string; upper: boolean = true);
var cmt_pos:word; {Position of #}
begin
 s:='';
 Repeat
  if t.eof then begin EndOfSection:=true; exit; end;
  t.Readln(s);
  inc(cur_line);
  cmt_pos:=Pos('#',s);
  if cmt_pos<>0 then SetLength(s,cmt_pos-1);
  s:=Trim(s);
  if upper then s := UpperCase(s);
  
 Until s<>'';
 if SameText(s, 'END') then begin CurSection:=''; EndOfSection:=true; end;
 if SameText(GetWordN(s,1), 'SECTION:') then begin CurSection:=GetWordN(s,2); EndOfSection:=true; end;
end; {GetNextLine}

Procedure SkipToNextSection;
begin
  while not EndOfSection do GetNextLine(s);
end;

Procedure Error(s:String);
begin
  PanMessageFmt(mt_error, 'File %s Error at line %d: %s', [filename, cur_line, s]);
end;

Const
   pi_steps = 100;
var
   ppos,fsize: LongInt;

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
  result:='';
  if n >= StaticResIdxMask then
    begin
      var sr := GetStaticMatList(kind);
      var si := n and not StaticResIdxMask; // Remove stati res. idx mask
      if (sr <> nil) and (si < sr.Count) then
        Result := sr[si];
    end;

 if (n<0) or (n>=mats.count) then exit;
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
  GetNextLine(s, false);
  GetWord(s,1,w1);
  if SameText(w1, 'end') then break;

  // Parse mat name with whitespaces
  // Some materials from IJIM has spaces in their name.
  var tokens := SplitString(s, ':');
  var p := Length(tokens);
  if p = 0 then continue;
  w2 := tokens[p -1];
  p := LowerCase(w2).IndexOf('.mat');
  if p = -1 then
  begin
    PanMessageFmt(mt_warning, 'Invalid mat filename: ''%s'' at index %d line %d', [w2, i, t.curline]);
    mats.Add(w1); // Add it to the list anyway so the surface mat indices won't break
    inc(i);
    continue;
  end;

  p := p + 5; // .mat - the reason for 5 and not 4 is the Delete is one-based array indexing
  Delete(w2, p, w2.Length - p+1);
  w2 := w2.Trim;
  mats.Add(w2);
  inc(i);
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
 if kind <> TProjectType.IJIM then
  begin
   GetNextLine(s);
   SScanf(s,'WORLD COLORMAPS %d',[@n]);
   For i:=0 to n-1 do
   begin
    GetNextLine(s);
    SScanf(s,'%s %s',[@w1,@w2]);
    cmps.Add(w2);
   end;
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

  {extralight}
  if header.Version = 1 then
    begin
      p:=GetWord(s,p,w1);
      ValSingle(w1,TSurf.extralight.a);
      TSurf.extralight := IntensityToColor(TSurf.extralight.a);
    end
  else // IJIM
    begin
      p:=GetWord(s,p,w1);
      ValSingle(w1,TSurf.extralight.r);
      p:=GetWord(s,p,w1);
      ValSingle(w1,TSurf.extralight.g);
      p:=GetWord(s,p,w1);
      ValSingle(w1,TSurf.extralight.b);
      p:=GetWord(s,p,w1);
      ValSingle(w1,TSurf.extralight.a);
    end;

  p:=GetWord(s,p,w1); {Nvert}
  nvx:=StrToInt(w1);
  for j:=0 to nvx-1 do
  begin
   p:=GetWord(s,p,w1);
   pc:=Pos(',',w1);
   if pc<>0 then w1[pc]:=' ';
   if pc = Length(w1) then // if there is no UV idx in w1, get it
    begin
     p:=GetWord(s,p,w2);
     w1 := w1 + w2;
    end;
   SScanf(w1,'%d %d',[@vx,@tvx]);
   TSurf.VXS.Add(vx);
   TSurf.tvxs.Add(tvx);
  end;
  il.Clear;

  While p<=length(s) do
  begin
   p:=GetWord(s,p,w1);
   il.Add(StrToFloat(w1, TFormatSettings.Invariant));
  end;

  var viofs: integer := 0;
  if il.Count > nvx then
    begin {MOTS and IJIM}
     if header.Version = 1 then
      kind := TProjectType.MOTS;
     for j:=0 to nvx-1 do
      begin
        vxInt:=TVXIntensity.Create;
        TSurf.Intensities.Add(vxInt);
        vxInt.a := TSurf.extralight.a; // IJIM
        if header.Version = 1 then // MOTS
          begin
            vxInt.a:=il[viofs];
            Inc(viofs)
          end;
        vxInt.r:=il[viofs]; Inc(viofs);
        vxInt.g:=il[viofs]; Inc(viofs);
        vxInt.b:=il[viofs]; Inc(viofs);
      end;
    end
  else {JK}
  begin
   for j:=0 to nvx-1 do
   begin
    vxInt:=TVXIntensity.Create;
    TSurf.Intensities.Add(vxInt);
    vxInt.a:=il[j];
    vxInt.r:=vxInt.a;
    vxInt.g:=vxInt.a;
    vxInt.b:=vxInt.a;
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
  SetProjectType(kind);
 end; {LoadGeometry}

Procedure LoadSectors;
  var i,j,numSecs: integer;
      nvx: integer;
      jsec: TJKSector;
      jvx: TJKVertex;
      jsurf: TJKSurface;
      tsurf: TTempSurf;
      //scvxList: TIntList;
      bsurf,nsurfs: integer;
      isurf: integer;
      igvx,ilvx: integer;
      txvx: TTempTXVertex;
      itxvx: integer;
      vxInt: TVXIntensity;

  Procedure SetJVX(sv: TTempVertex; dv: TJKVertex);
  begin
   dv.x:=sv.x;
   dv.y:=sv.y;
   dv.z:=sv.z;
  end;
begin

try
 CurSection:='';
 GetNextLine(s);
 numSecs  := StrToInt(GetWordN(s, 3));
 //scvxList := TIntList.Create;
 Progress.Msg := 'Loading Sectors';

 For i := 0 to numSecs - 1 do
   begin
    DO_Progress;
    GetNextLine(s); {Sector N}
    //scvxList.Clear;
    jsec := NewSector;
    Sectors.Add(Jsec);

    // TODO: In case of a broken section this can loop forever.
    //       Fix: jump out after n sec.
    Repeat
      GetNextLine(s);
      w1 := GetWordN(s, 1);

      With JSec do
      if w1='FLAGS' then SScanf(s,'FLAGS'#9'%x',[@Flags])
      else if w1='AMBIENT' then
        begin
          if header.Version = 1 then // JKDF2 & MOTS
            begin
              SScanf(s,'AMBIENT LIGHT %l',[@Ambient.a]);
              Ambient := IntensityToColor(Ambient.a);
            end
          else // IJIM
            begin
              SScanf(s,'AMBIENT LIGHT %l %l %l',[@Ambient.r, @Ambient.g, @Ambient.b]);
            end;
          Ambient.a := 0.0;
        end
      else if w1='EXTRA' then
        //SScanf(s,'EXTRA LIGHT %f',[@ExtraLight])
        begin
          if header.Version = 1 then // JKDF2 & MOTS
            begin
              SScanf(s,'EXTRA LIGHT %l',[@ExtraLight.a]);
              ExtraLight := IntensityToColor(ExtraLight.a);
            end
          else // IJIM
            begin
              SScanf(s,'EXTRA LIGHT %l %l %l',[@ExtraLight.r, @ExtraLight.g, @ExtraLight.b]);
            end;
            ExtraLight.a := 0.0;
        end
      else if w1='AVERAGE' then
        begin
          w1 :=  getWordN(s,3);
          if w1 = 'INTENSITY' then
            SScanf(s,'AVERAGE LIGHT INTENSITY %l %l %l',[@PointLight.Color.r, @PointLight.Color.g, @PointLight.Color.b])
          else if w1 = 'POSITION' then
            SScanf(s,'AVERAGE LIGHT POSITION %f %f %f',[@PointLight.Position.x, @PointLight.Position.y, @PointLight.Position.z])
          else if w1 = 'FALLOFF' then
            SScanf(s,'AVERAGE LIGHT FALLOFF %f %f',[@PointLight.minRange, @PointLight.maxRange])
        end
      else if w1='SOUND' then SScanf(s,'SOUND %s %f',[@Sound,@soundVolume])
      else if w1='COLORMAP' then ColorMap:=GetCmp(StrToInt(GetWordN(s,2)))
     { else if w1='CENTER' then SScanf(s,'CENTER %f %f %f',[@Cx,@Cy,@Cz])
      else if w1='RADIUS' then SScanf(s,'RADIUS %f',[@rad])}
      else if w1='COLLIDEBOX' then
        With CollideBox do SScanf(s,'COLLIDEBOX %f %f %f %f %f %f',[@x1,@y1,@z1,@x2,@y2,@z2])
      else if w1='TINT' then SScanf(s,'TINT %l %l %l',[@Tint.r,@Tint.g,@tint.b])
      else if w1='THRUST' then SScanf(s,'THRUST %f %f %f',[@Thrust.x,@Thrust.y,@Thrust.z]);

      if w1='VERTICES' then
        begin
         nvx := StrToInt(GetWordN(s, 2));
         for j:=0 to nvx - 1 do GetNextLine(s);
            continue;
        end;

      if w1='SURFACES' then
        begin
          SScanf(s,'SURFACES %d %d',[@bsurf,@nsurfs]);
          for isurf := bsurf to bsurf + nsurfs - 1 do
            begin
             tsurf := TTempSurf(GSurfList[isurf]);
             jsurf := jsec.NewSurface;
             jsec.surfaces.Add(jsurf);

             jsurf.material   := GetMat(TSurf.nmat);
             jsurf.SurfFlags  := tsurf.SurfFlags;
             jsurf.FaceFlags  := tsurf.FaceFlags;
             jsurf.geo        := tsurf.geo;
             Jsurf.Light      := tsurf.light;
             Jsurf.tex        := tsurf.tex;
             Jsurf.nAdj       := tsurf.nAdj;
             jsurf.ExtraLight := tsurf.ExtraLight;
             //jsurf.normal     := tsurf.normal;

             // Remove FF_DoubleSided flag since world geometry is always culled
             jsurf.FaceFlags := jsurf.FaceFlags and not FF_DoubleSided;

             // TODO: Remove because scales are calculated after this section is parsed
             //if CurrentProject <>IJIM then
             //begin
      //       if tsurf.SurfFlags and SF_DOUBLERES<>0 then begin jsurf.uscale:=2; jsurf.vscale:=2; end
      //       else if tsurf.SurfFlags and SF_HALFRES<>0 then begin jsurf.uscale:=0.5; jsurf.vscale:=0.5; end
      //       else if tsurf.SurfFlags and SF_EIGHTHRES<>0 then begin jsurf.uscale:=0.125; jsurf.vscale:=0.125; end;
             //end;

             var matSize := TSize.Create(1, 1);
             if (kind = IJIM) and (not jsurf.Material.IsEmpty) then
               try
                matSize := jsurf.GetMaterialSize;
               except on E:Exception do PanMessage(mt_warning,
                  Format('Cannot load %s for sector %d surface %d: %s' +
                  '  Imported surface will have invalid UV coords!'
                  , [jsurf.Material, jsurf.Sector.num, isurf - bsurf, e.message]));
               end;

             for j := 0 to tsurf.vxs.count - 1 do
               begin
                igvx := tsurf.vxs[j]; {global index of vertex}
//                ilvx := scvxList.IndexOf(igvx); {local index}
//                if ilvx = -1 then {not found}
//                  begin
//                   jvx := jsec.NewVertex;
//                   SetJVX(GVXList[igvx], jvx);
//              {     jsec.vertices.Add(jvx);}
//                   scvxList.Add(igvx);
//                   jsurf.AddVertex(jvx);
//                  end
//                else
//                  jsurf.AddVertex(jsec.vertices[ilvx]);

                with TTempVertex(GVXList[igvx]) do
                  jvx := jsec.AddVertex(x, y, z);

                if jsurf.vertices.indexOf(jvx) <> - 1 then
                  continue;

                var jsvxi := jsurf.AddVertex(jvx);


                {Add texture vertices}
                itxvx := tsurf.tvxs[j];
                if ( itxvx < 0 ) or (itxvx >= GTXVXList.Count) then itxvx := 0;
                txvx := TTempTXVertex(GTXVXList[itxvx]);

                With jsurf.Txvertices[jsvxi] do
                  begin
                    u := txvx.u * matSize.Width;
                    v := txvx.v * matSize.Height;
                    vxInt   := TVXIntensity(tsurf.Intensities[j]);
                    color.a := vxInt.a;
                    color.r := vxInt.r;
                    color.g := vxInt.g;
                    color.b := vxInt.b;
                  end;
               end; // for j := 0 to tsurf.vxs.count - 1 do
            end; // for isurf := bsurf to bsurf + nsurfs - 1 do

          break;  // break out of repeat loop
        end; // if w1='SURFACES' then

    until false; // repeat
   end; //  For i := 0 to numSecs - 1 do

 //scvxList.Free;
 except
  on e: Exception do;
 end;
end; {LoadSectors}

Function GetSurfByN(n:integer): TJKSurface;
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

Procedure LoadTemplates;
var n,p: integer;
    w: string;
begin
  Progress.Msg:='Loading Templates';
  CurSection:='';
  GetNextLine(s);
  SScanf(s,'World templates %d',[@n]);
  For var i := 0 to n-1 do
    begin
      GetNextLine(s, false);
      if (SameText(s, 'end')) or EndOfSection then break;
      p := GetWord(s, 1, w);
      if (w = '') or (w = '#') or (w = '//') then
        continue;
      self.templates.AddFromString(s);
    end;
end;

Procedure LoadThings;
var i,j,n,p,nsec: Integer;
    th: TThing;
    pe: Integer;
    v: TTPLValue;
begin
 Progress.Msg := 'Loading Things';
 CurSection := '';
 GetNextLine(s);
 SScanf(s,'WORLD THINGS %d', [@n]);
 For i := 0 to n - 1 do
 begin
  GetNextLine(s, false);
  if (SameText(s, 'end')) or EndOfSection then break;
  th := NewThing;
  With th do
  SScanf(s,'%s %s %s %f %f %f %f %f %f %d',
        [@w1, @w1, @Name, @x, @y, @z, @pch, @yaw, @rol, @nsec]);
 try
  th.Sec := Sectors[nsec];
 except
  on Exception do th.sec := nil;
 end;

  Things.Add(th);
  p := 1;
  For j := 1 to 10 do
  begin
   p := ScanForNonSpace(s ,p);
   p := ScanForSpace(s, p);
  end;

  While p <= Length(s) do
  begin
   p := getWord(s, p, w1);
   pe := Pos('=', w1);
   if pe = 0 then continue;
   v := TTPLValue.Create;
   v.Name  := Copy(w1, 1, pe - 1);
   v.vtype := GetTplVType(v.name);
   v.atype := GetTplType(v.name);

   var vs := Copy(w1, pe + 1, length(w1) - pe);
   if not v.Val(vs) then
     begin
       PanMessageFmt(mt_warning,'Thing ''%s'' parameter ''%s'' has invalid value: %s', [th.Name, v.Name, vs]);
       // TODO: should assign string?
     end;

   th.Vals.Add(v);
  end;

 end;
end;

Procedure LoadLights;
var i,n:integer;
    lt:TSEDLight;
begin
 CurSection:='';
 GetNextLine(s);
 n:=StrToInt(GetWordN(s,3)); // editor lights
 i:=0;
 While ( i < n ) and (not EndOfSection) do
   begin
     GetNextLine(s);
     GetWord(s,1,w1);
     if w1='END' then break;

     lt := NewLight;
     With Lt do
      if not SScanf(s,'%s %x %d %f %f %f %f %f %l %l %l',
        [@w1, @flags, @Layer, @position.x,@position.y,@position.z, @range, @intensity, @color.r,@color.g,@color.b]) then
          begin
            {fallback to jkdf2}
            if not SScanf(s,'%s %x %d %f %f %f %f %f', 
              [@w1, @flags, @Layer, @position.x,@position.y,@position.z, @range, @intensity]) then
              PanMessageFmt(mt_warning, 'Corrupted entry in light section at line %d', [t.curline]);
            lt.color := ColorWhite;
          end;
     Lights.Add(lt);
   end;
end;

Procedure LoadLayers;
  var
    w: string;
    idx, p: integer;
begin
  Progress.Msg:='Loading Layers';
  CurSection:='';
  var n := 0;
  GetNextLine(s);
  SScanf(s, 'EDITOR LAYERS %d',[@n]);

  For var i := 0 to n-1 do
  begin
    GetNextLine(s, false);
    Layers.Add(s);

    // Sectors
    GetNextLine(s);
    p:=GetWord(s,1,w);
    var sn := 0;
    SScanf(w, '%d:',[@sn]);
    for var j := 0 to sn -1 do
    begin
      if p >= Length(s) then
        begin
          GetNextLine(s);
          p := 1;
        end;
      p:=GetWord(s,p,w);
      if ValInt(w, idx) then
        if idx < sectors.Count then
          sectors[idx].Layer := i;
    end;

    // Things
    GetNextLine(s);
    p:=GetWord(s,1,w);
    sn := 0;
    SScanf(w, '%d:',[@sn]);
    for var j := 0 to sn -1 do
    begin
      if p >= Length(s) then
        begin
          GetNextLine(s);
          p := 1;
        end;
      p:=GetWord(s,p,w);
      if ValInt(w, idx) then
        if idx < things.Count then
          things[idx].Layer := i;
    end;
  end;

  // Layer visibility
  GetNextLine(LVisString);
  RemoveComment(LVisString);
end; {LoadLayers}

Procedure LoadCogs;
var i,n,p,idx:integer;
    cg:TCog;
    cv:TCogValue;
    cf:TCogFile;
    nval:integer;
begin
 cf := TCogFile.Create;

 CurSection:='';
 DO_Progress;
 Progress.Msg:='Loading Cogs';
 GetNextLine(s);
 SScanf(s,'WORLD COGS %d',[@n]);

 for i:=0 to n - 1 do
 begin
  GetNextLine(s, false);
  if (SameText(s, 'end')) or EndOfSection then break;
  p := GetWord(s,1,w1);
  p := GetWord(s,p,w1);
  cg := TCog.Create;
  Cogs.Add(cg);
  cg.Name:=w1;
  cf.LoadNoLocals(cg.Name);
  nval := 0;
  While p <= Length(s) do
    begin
      p:=getWord(s,p,w1);
      if w1 = '' then continue;
      cv:=TCOGValue.Create;
      cg.Vals.Add(cv);

      if nval<cf.Count then
        cv.Assign(cf[nval]);

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
  p := GetWord(s,1,w1);
  p := GetWord(s,p,w2);

  if w1 = 'VERSION' then
    begin
      ValDword(w2, version);
      if (version = 2) or (version = 3) then
        kind := TProjectType.IJIM
      else if version <> 1 then
        raise Exception.CreateFmt('Unknown level version: %d', [Version]);
    end
  else if w1 = 'LOD' then
   SScanf(s,'LOD DISTANCES %f %f %f %f', [@lodDistances[1], @lodDistances[2], @lodDistances[3] ,@lodDistances[4]])
  else if w1 = 'FOG' then
   SScanf(s,'FOG %b %l %l %l %l %f %f', [@fog.enabled, @fog.color.r, @fog.color.g, @fog.color.b, @fog.color.a, @fog.fogStart, @fog.fogEnd])
  else if w1 = 'MIPMAP' then
   SScanf(s,'MIPMAP DISTANCES %f %f %f %f', [@mipmapDistances[1], @mipmapDistances[2], @mipmapDistances[3], @mipmapDistances[4]])
  else if w1 = 'WORLD' then SScanf(s,'WORLD GRAVITY %f', [@gravity])
  else if w1 = 'PERSPECTIVE' then SScanf(s,'PERSPECTIVE DISTANCE %f', [@perspectiveDistance])
  else if w1 = 'GOURAUD' then SScanf(s,'GOURAUD DISTANCE %f', [@gouraudDistance])
  else if w1 = 'CEILING' then
   begin
    GetWord(s,p,w1);
    if w1 = 'OFFSET' then SScanf(s,'CEILING SKY OFFSET %f %f', [@ceilingSky.offset.X, @ceilingSky.offset.Y]);
    if w1 = 'Z' then SScanf(s,'CEILING SKY Z %f', [@ceilingSky.height]);
   end
  else if w1 = 'HORIZON' then
   begin
    GetWord(s,p,w1);
    if w2 = 'DISTANCE' then ValDouble(w1, horizonSky.distance);
    if w2 = 'PIXELS' then SScanf(s,'HORIZON PIXELS PER REV %f', [@horizonSky.pixelsPerRev]);
    if w2 = 'SKY' then SScanf(s,'HORIZON SKY OFFSET %f %f', [@horizonSky.offset.X, @horizonSky.offset.Y]);
   end;
  end;
end;

var i,j:integer;
    cv:TCOGValue;
    tmpsurf:TJKSurface;
begin {LoadFromJKL begin}
  kind := TProjectType.JKDF2; // set level project type
  ppos:= 0;
  FormatSettings.DecimalSeparator := '.';
  mats := TStringList.Create;
  cmps := TStringList.Create;

  t := TTextFile.CreateRead(OpenFileRead(filename, 0));
  fsize := t.Fsize;

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

  try
    try
      repeat
      while (CurSection='') and (not t.eof) do GetNextLine(s);
       if t.eof then break;
       EndOfSection:=false;
       if CurSection='GEORESOURCE' then LoadGeometry
       else if CurSection='HEADER' then LoadHeader
       else if CurSection='SECTORS' then LoadSectors
       else if CurSection='MATERIALS' then LoadMats
       else if CurSection='COGS' then LoadCogs
       else if CurSection='TEMPLATES' then LoadTemplates
       else if CurSection='THINGS' then LoadThings
       else if CurSection='LIGHTS' then LoadLights
       else if CurSection='LAYERS' then LoadLayers
       else SkipToNextSection;
      until t.eof;
    Finally
      t.FClose;

    Progress.Hide;

    {Post process}
    for i:=0 to sectors.Count-1 do
    With sectors[i] do
      for j:= 0 to surfaces.Count-1 do
      With Surfaces[j] do
      begin
       if nadj < 0 then begin adjoin:= nil; continue; end;
       With TAdjoin(GAdjoins[nadj]) do
       begin
        Adjoin:=GetSurfByN(TAdjoin(GAdjoins[mirror]).surf);
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

     for i:=0 to sectors.Count-1 do
     With Sectors[i] do
     begin
      Renumber;
      for j:=0 to surfaces.Count-1 do
      begin
       tmpsurf:=Surfaces[j];
       tmpsurf.Recalc; // TODO: parse normals
       if IsTXFlipped(tmpsurf) then tmpsurf.FaceFlags:=longint(tmpsurf.FaceFlags or FF_SF_Flip)
       else tmpsurf.FaceFlags:=tmpsurf.FaceFlags and (not FF_SF_Flip);
       FindUVScales(tmpsurf);

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
 
 FormatSettings.DecimalSeparator:='.';
 Assign(t,f);Rewrite(t);
 WriteS('SED ' + SedVerNum);

 if kind = TProjectType.MOTS then
 begin
   WriteS('');
   WriteS('SEC: MOTS');
   WriteS('END');
   WriteS('');
 end;

 With Header do
 begin
  WriteS('SEC: HEADER');

  st := masterCMP; if st='' then st:='-';

  WriteFmt('HDR %d %.6f %.6f %.6f %.6f %s %.2f',
   [version, gravity, ceilingSky.height, perspectiveDistance, gouraudDistance, st, ppunit]);

  WriteFmt('HOR %.6f %.6f %.6f %.6f',
   [horizonSky.distance, horizonSky.pixelsPerRev, horizonSky.offset.X, horizonSky.offset.Y]);
  WriteFmt('CEIL %.6f %.6f',
   [ceilingSky.offset.X, ceilingSky.offset.Y]);
  WriteFmt('MIP %.6f %.6f %.6f %.6f',
   [mipmapDistances[1],mipmapDistances[2],mipmapDistances[3],mipmapDistances[4]]);
  WriteFmt('LOD %.6f %.6f %.6f %.6f',
   [lodDistances[1],lodDistances[2],lodDistances[3],lodDistances[4]]);
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
   [st,soundVolume,Flags,RgbToIntensity(Ambient),RgbToIntensity(ExtraLight),tint.r,tint.g,tint.b,Layer]);

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
     [st,SurfFlags,FaceFlags,geo,light,tex,RgbaToIntensity(ExtraLight),Vertices.Count,asec,asf,AdjoinFlags,uscale,vscale]);
   st:='';
   for vx:=0 to vertices.count-1 do
   With TXVertices[vx] do
   begin
     st:=Concat(st,Sprintf(' %d %.2f %.2f %.4f',
     [vertices[vx].num,u,v,color.a]));
   end;
   for vx:=0 to vertices.count-1 do
   With TXVertices[vx] do
   begin
     st:=Concat(st,Sprintf(' %.4f %.4f %.4f',[color.r,color.g,color.b]));
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
 WriteFmt('LTS %d',[Lights.count]); // TODO: Make new format JED/SED format due to fields rgbRange and RGBIntensity being removed
  for i:=0 to Lights.count-1 do
    With Lights[i] do
      WriteFmt('%.6f %.6f %.6f %d %.6f %.6f %x %.4f %.4f %.4f',
        [position.x,position.y,position.z,Layer,Intensity,range,Flags, color.r,color.g,color.b]);
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

Function ReadFmt(const fmt:string;v:array of pointer): Boolean;
begin
 t.Readln(st);
 Result := SScanf(st,fmt,v);
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
   [@version,@Gravity, @ceilingSky.height, @perspectiveDistance, @gouraudDistance, @masterCMP, @ppunit]);
  ReadFmt('HOR %f %f %f %f',
   [@horizonSky.distance, @horizonSky.pixelsPerRev, @horizonSky.offset.X, @horizonSky.offset.Y]);
  ReadFmt('CEIL %f %f',
   [@ceilingSky.offset.X, @ceilingSky.offset.Y]);
  ReadFmt('MIP %f %f %f %f',
   [@mipmapDistances[1], @mipmapDistances[2], @mipmapDistances[3], @mipmapDistances[4]]);
  ReadFmt('LOD %f %f %f %f',
   [@lodDistances[1], @lodDistances[2], @lodDistances[3], @lodDistances[4]]);
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
  ReadFmt('%s %s %f %x %l %l %l %l %l %d',
   [@ColorMap,@Sound,@soundVolume,@Flags,@Ambient.a,@ExtraLight.a,@tint.r,@tint.g,@tint.b,@Layer]);

  Ambient := IntensityToColor(Ambient.a);
  ExtraLight := IntensityToColor(ExtraLight.a);
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
    ReadFmt('%s %x %x %d %d %d %l %d %d %d %x %l %l',
     [@Material,@SurfFlags,@FaceFlags,@geo,@light,@tex,@ExtraLight.a,@nsv,@asec,@asf,@AdjoinFlags,@uscale,@vscale]);
    ExtraLight := IntensityToColor(ExtraLight.a);
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
      ValSingle(w,color.a);
     end;
    end;
    {Load RGB}
    for vx:=0 to nsv-1 do
    With TXVertices[vx] do
    begin
     p:=GetWord(st,p,w);
     if w='' then break;
     ValSingle(w,color.r);
     p:=GetWord(st,p,w);
     ValSingle(w,color.g);
     p:=GetWord(st,p,w);
     ValSingle(w,color.b);
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
    l:TSEDLight;
    i:integer;
    rgbRange: double;
    RGBIntensity: single;
begin
 Progress.Msg:='Loading Lights';
 ReadFmt('LTS %d',[@nl]);
  for i:=0 to nl-1 do
    begin
      l:=NewLight;
      Lights.Add(l);
      With l do
        if not ReadFmt('%f %f %f %d %f %f %x %l %l %l',[@position.x,@position.y,@position.z,@layer,@intensity,@range,
           @flags,@color.r,@color.g,@color.b]) then
          if not ReadFmt('%f %f %f %d %f %f %x %f %l %l %l %l',[@position.x,@position.y,@position.z,@layer,@intensity,@range,
           @flags,@RGBRange,@RGBIntensity,@color.r,@color.g,@color.b]) then
            PanMessageFmt(mt_warning, 'Corrupted entry in light section at line %d', [t.curline]);
      if kind = TProjectType.JKDF2 then
        begin
         l.color := ColorWhite;
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
 FormatSettings.DecimalSeparator:='.';
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
   else if w='MOTS' then kind := TProjectType.MOTS
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
    if IsTXFlipped(tmpsurf) then tmpsurf.FaceFlags:=tmpsurf.FaceFlags or FF_SF_Flip
    else tmpsurf.FaceFlags:=tmpsurf.FaceFlags and (not FF_SF_Flip);
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

