
Type
TTempTXVertex=class
  u,v:single;
end;

Procedure StringToHNode(const s:string;node:THNode);
var w1:string;
begin
  With node do
  SSCanf(s,'%s %s %x %d %d %s %s %s %f %f %f %f %f %f %f %f %f %s',
           [@w1,@w1,@ntype,@nmesh,@parent,@w1,@w1,@w1,@x,@y,@z,@pch,@yaw,@rol,@pivotx,@pivoty,@pivotz,@nodename]);
end;

Function HNodeToString(node:THNode;n:integer):string;
begin
 With node do
 Result:=  Format('%2d:     0x0    0x%x    %2d     %2d      %2d     %2d       %2d     %3.6f %3.6f %3.6f  %4.6f %4.6f %4.6f %4.6f %4.6f %4.6f %s',

           [n,            ntype, nmesh,  parent,     0,      0,       0,     x,      y,    z,    pch,   yaw,  rol, pivotx,pivoty, pivotz,nodename]);

end;

Constructor T3DO.CreateFrom3DO(const name: string;lod:integer; var fileVersion: double);
var t:TTextFile; cpos:word; s,w1,w2:String;
    cur_line:integer;
    a,n,nx:Integer;
    EndOfSection:boolean;
    CurSection:String;
    GTXVXList:TList;
    insX,insY,insZ:double;

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
 PanMessage(mt_error,'File '+name+Format(' Error at line %d: %s',[cur_line,s]));
end;

Procedure LoadHeader(var version: double);
begin
 CurSection:='';
 GetNextLine(s);
 SScanf(s,'3DO %f',[@version]);
end;

Function GetMat(n:integer):String;
begin
 if (n<0) or (n>=mats.count) then begin result:=''; exit; end;
 result:=mats[n];
end;

Procedure LoadMats;
var p,i,n:integer;
begin
 CurSection:='';
 GetNextLine(s);
 n:=StrToInt(GetWordN(s,2));
 i:=0;
 While (i<n) and (not EndOfSection) do
 begin
  GetNextLine(s);
  p:=GetWord(s,1,w1);
  if w1='END' then break;
  var tokens := SplitString(s, ':');
  var matofs := Length(tokens);
  if matofs < 1 then
    raise Exception.CreateFMT('Invalid entry in material section at index: %d', [n]);
  matofs := matofs -1;
  mats.Add(tokens[matofs].Trim);
  inc(i);
 end;
end;

Function GetVXWord(const s:string;p:integer;var w:string):integer;
var ps:pchar;np:integer;
begin
 w:='';
 if p>length(s) then begin result:=p; exit; end;
 ps:=StrScan(@s[p],',');
 if ps=nil then
 begin
  w:=trim(PChar(@s[p]));
  result:=length(s)+1;
 end else
 begin
  np:=ps-@s[1];
  w:=trim(copy(s,p,np-p+1));
  result:=np+2;
 end;
end;

Procedure LoadGeometry(const version: double);
var i,j,k,m,n,p,pc:integer;
    nvx,vx,tvx:integer;
    txvx:TTempTXVertex;

    mvx:TVertex;

    ngeos:integer;
    nmeshes,nvxs,nfaces:integer;
    mesh:T3DOMesh;
    face:T3DOFace;
    v:TVertex;
    tv:TTempTXVertex;
    vertIntensities: array of TColorF;

Function GetTXVX(n:integer):TTempTXVertex;
begin
 if (n<0) or (n>=GTXVXList.count) then result:=nil
 else result:=TTempTXVertex(GTXVXList[n]);
end;

begin
 GTXVXList:=TList.Create;

 CurSection:='';
 {ColorMaps}
 GetNextLine(s); {Radius - don't care}
 GetNextLine(s); {Insert}
 SScanf(s,'INSERT OFFSET  %f %f %f', [@InsX, @InsY, @InsZ]);
 GetNextLine(s); {Geosets}
 SScanf(s,'GEOSETS %d', [@ngeos]);

 if lod > ngeos - 1 then lod := ngeos - 1;

 while not EndOfSection do
 begin
  GetNextLine(s);
  p := GetWord(s, 1, w1);
  if w1 <> 'GEOSET' then continue;
  GetWord(s, p, w1);
  if StrToInt(w1) <> lod then continue;
  break;
 end;
 if EndOfSection then exit;

 // Meshes section
 GetNextLine(s); {Meshes}
 SScanf(s,'MESHES %d', [@nmeshes]);

 for i := 0 to nmeshes - 1 do
 begin
  While not EndOfSection do
    begin
     GetNextLine(s);
     p := GetWord(s, 1, w1);
     if w1 <> 'MESH' then continue;
     break;
    end;
  if EndOfSection then exit;

  mesh := NewMesh;
  meshes.Add(mesh);

  GetNextLine(s); {Name} mesh.name := GetWordN(s, 2);

  while not EndOfSection do
  begin
   GetNextLine(s);
   p := GetWord(s, 1, w1);

   // Note: parsing mesh radius is skipped, can be calculated
   if w1 = 'GEOMETRYMODE' then
    SScanf(s,'GEOMETRYMODE %d', [@mesh.geo])
   else if w1 = 'LIGHTINGMODE' then
    SScanf(s,'LIGHTINGMODE %d', [@mesh.light])
   else if w1 = 'TEXTUREMODE' then
    SScanf(s,'TEXTUREMODE %d', [@mesh.tex])
   else if w1 = 'VERTICES' then
   begin
     SScanf(s, 'VERTICES %d', [@Nvxs]);
     SetLength(vertIntensities, Nvxs);
     break;
   end;
  end;
  if EndOfSection then exit;

  for j := 0 to nvxs - 1 do
  begin
   GetNextLine(s);
   v := TVertex.Create;
   var vcolor: TColorF;

   if IsClose(version, 2.1) then
      begin
        var vi: Double;
        SScanf(s,'%s %f %f %f %f',[@w1,@v.x,@v.y,@v.z, @vi]);
        vcolor := IntensityToColor(vi);
      end
    else if IsClose(version, 2.2) then
      begin
        SScanf(s,'%s %f %f %f %l %l %l',[@w1, @v.x, @v.y, @v.z, @vcolor.r, @vcolor.g, @vcolor.b]);
        vcolor.a := 1.0;
      end
    else
      begin // 2.3
        SScanf(s,'%s %f %f %f %l %l %l %l',[@w1, @v.x ,@v.y ,@v.z, @vcolor.r, @vcolor.g, @vcolor.b, @vcolor.a]);
      end;

   vertIntensities[j] := vcolor;
   mesh.vertices.Add(v);
  end;

  GetNextLine(s); {Texture vertices}
  SScanf(s,'TEXTURE VERTICES %d',[@nvxs]);

  for j:=0 to nvxs-1 do
  begin
   GetNextLine(s);
   txvx:=TTempTXVertex.Create;
   SScanf(s,'%s %l %l',[@w1,@txvx.u,@txvx.v]);
   GTXVXList.Add(txvx);
  end;

  While not EndOfSection do
  begin
   GetNextLine(s);
   p:=GetWord(s,1,w1);
   if w1 <> 'FACES' then continue;
   break;
  end;
  if EndOfSection then exit;

  GetWord(s,p,w1);
  nfaces:=StrToInt(w1);

   For j:=0 to nfaces-1 do
   begin
    GetNextLine(s);
    Face:=T3DOFace.Create;
    Mesh.Faces.Add(face);

    p:=GetWord(s,1,w1); {num}
    p:=GetWord(s,p,w1); {material}
    ValInt(w1,face.imat);
    p:=GetWord(s,p,w1); {type}
    ValHex(w1,face.faceflags);
    p:=GetWord(s,p,w1); {geo}
    ValInt(w1,face.geo);
    p:=GetWord(s,p,w1); {light}
    ValInt(w1,face.light);
    p:=GetWord(s,p,w1); {tex}
    ValInt(w1,face.tex);

    {extralight}
    if IsClose(version, 2.1) then
      begin
        p:=GetWord(s,p,w1);
        ValSingle(w1,face.extraLight.a);
        face.extraLight := IntensityToColor(face.extraLight.a);
      end
    else // version 2.2 (r/g/b) and version 2.3 (r/g/b/a)
      begin
        p:=GetWord(s,p,w1);
        if not ValColor(w1, face.extraLight) then
          PanMessage(mt_Error, SysUtils.Format('Extra light conversion error at line %d in file %s',[t.CurrentLine, t.GetFullName]));
      end;

    p:=GetWord(s,p,w1); {Nvert}
    nvxs:=StrToInt(w1);
    for k:=0 to nvxs-1 do
      begin
       // vert
       p:=GetVXWord(s,p,w1);
       ValInt(w1,nvx);
       Face.AddVertex(Mesh.Vertices[nvx]);
       var vcolor := vertIntensities[nvx];

       // uv
       p:=GetWord(s,p,w1);
       ValInt(w1,nvx);
       tv:=GetTXVX(nvx);
       if tv<>nil then
         With Face.TXVertices[k] do
           begin
            u := tv.u;
            v := tv.v;
            color := vcolor;
           end;
      end;
   end; // face


   // face normals
   While not EndOfSection do
    begin
     GetNextLine(s);
     p:=GetWord(s,1,w1);
     p:=GetWord(s,p,w2);
     if (w1 <> 'FACE') and (w2 <> 'NORMALS') then continue;
      break;
    end;
   if EndOfSection then exit;

   For j:=0 to nfaces - 1 do
     begin
      GetNextLine(s);
      face := Mesh.Faces[j];
      SScanf(s,'%s %f %f %f',[@w1, @face.normal.x, @face.normal.y, @face.normal.z]);
     end;

   for m:=0 to GTXVXList.Count-1 do
    TTempTXVertex(GTXVXList[m]).Free;
   GTXVXList.Clear;
 end; // mesh
 GTXVXList.Free;
end;


Procedure LoadHierarchy;
var i,j,n,p,nsec:integer;
    hnode:THNode;
begin
 CurSection:='';
 GetNextLine(s);
 SScanf(s,'HIERARCHY NODES %d',[@n]);
 for i:=0 to n-1 do
 begin
  GetNextLine(s);
{#  num:   flags:   type:    mesh:  parent:  child:  sibling:  numChildren:        x:         y:         z:     pitch:       yaw:      roll:    pivotx:    pivoty:    pivotz:  hnodename:
     0:  0x0000 0x00010       -1       -1       1        -1             2   0.000000   0.000000   0.000000   0.000000   0.000000   0.000000   0.000000   0.000000   0.000000  $$$dummy}

  hnode:=THNode.Create;
  StringToHNode(s,hnode);
  {  With hnode do
  SSCanf(s,'%s %s %x %d %d %s %s %s %f %f %f %f %f %f %f %f %f %s',
           [@w1,@w1,@ntype,@nmesh,@parent,@w1,@w1,@w1,@x,@y,@z,@pch,@yaw,@rol,@pivotx,@pivoty,@pivotz,@nodename]);}
  HNodes.Add(Hnode);
 end;
end;

Procedure OffsetMeshes;
var i, j: integer;
    hnode, hnode1: THNode;
    mdx, mdy, mdz: double;
    mx: TMat3x3;
    vs: TVertices;
begin
 for i := 0 to hnodes.Count-1 do
 begin
  hnode := hnodes[i];
  if hnode.nmesh = -1 then continue;

  vs := meshes[hnode.nmesh].vertices;
  with hnode do CreateRotMatrix(mx, pch, yaw, rol);
  for j := 0 to vs.Count - 1 do
    With vs[j] do
      begin
       x := x + hnode.pivotx;
       y := y + hnode.pivoty;
       z := z + hnode.pivotz;
       MultVM3(mx, x, y, z);

       x := x + hnode.x;
       y := y + hnode.y;
       z := z + hnode.z;
      end;

(*  mdx:=hnode.x+hnode.pivotx{+InsX};
  mdy:=hnode.y+hnode.pivoty{+InsY};
  mdz:=hnode.z+hnode.pivotz{+InsZ}; *)

  hnode1 := hnode;
  While hnode1.parent <> - 1 do
    begin
     hnode1 := hnodes[hnode1.parent];
     with hnode1 do CreateRotMatrix(mx, pch, yaw, rol);
     for j := 0 to vs.Count - 1 do
       With vs[j] do
         begin
          MultVM3(mx, x, y, z);
          x := x + hnode1.x;
          y := y + hnode1.y;
          z := z + hnode1.z;
         end;
    end;
 end;
end;

var i, j: integer;
begin {CreateFrom3DO begin}
 CreateNew;
 self.name := ExtractFileName(name);

 FormatSettings.DecimalSeparator:='.';
 if ExtractFilePath(name) <> '' then
    t := TTextFile.CreateRead(OpenFileRead(name, 0))
 else
    t := TTextFile.CreateRead(OpenGameFile(name));
 cur_line:=0;
 EndOfSection:=false;
 CurSection:='';
Try
  Try
  Repeat
  While (CurSection='') and (not t.eof) do GetNextLine(s);
   if t.eof then break;
   EndOfSection:=false;
   if CurSection='GEOMETRYDEF' then LoadGeometry(fileVersion)
   else if CurSection='HEADER' then LoadHeader(fileVersion)
   else if CurSection='MODELRESOURCE' then LoadMats
   else if CurSection='HIERARCHYDEF' then LoadHierarchy
   else SkipToNextSection;
  until t.eof;

Finally

OffsetMeshes;

t.FClose;
{Post process}
end;
{Free all stuff}

except
 On e:exception do PanMessage(mt_warning,e.message);
end;

end; {CreateFrom3DO end}


Function CompareTXVX(v1,v2:TTXVertex):integer;
var d:double;
begin
 d:=V1.u-V2.u;
 if Abs(d)>CloseEnough then
 begin
  if d>0 then Result:=1 else Result:=-1;
  exit;
 end;
 d:=V1.v-V2.v;
 if Abs(d)>CloseEnough then
 begin
  if d>0 then Result:=1 else Result:=-1;
  exit;
 end;
 Result:=0;
end;

Procedure T3DO.SaveToFile(const name: string; const version: double);
var t:TextFile;
    TXVXList,TxIdx:TTXVertices;
    txvx:TTXvertex;
    VXNorms:TList;
    Pn:^TVector;

Procedure UnOffsetMeshes;
var i,j,k:integer;
    hnode,hnode1:THNode;
    mdx,mdy,mdz:double;
    branch:TIntList;
    rmx,mx:TMat3x3;
    vs:TVertices;

begin {UnoffsetMeshes}
 branch:=TIntList.Create;

 for i:=0 to HNodes.Count-1 do
 begin
  hnode:=HNodes[i];
  if hnode.nmesh=-1 then continue;


  {Trace back all parent nodes}
  branch.Clear;
  branch.Add(i);
  hnode1:=hnode;
  While hnode1.parent<>-1 do
  begin
   branch.Add(hnode1.Parent);
   hnode1:=HNodes[hnode1.Parent];
  end;

  {Unrotate and unoffset in backwards order}

  vs:=Meshes[hnode.nmesh].Vertices;
  for k:=branch.count-1 downto 0 do
  begin
   hnode1:=Hnodes[branch[k]];
   With hnode1 do CreateRotMatrix(mx,pch,yaw,rol);
   rmx[0,0]:=mx[0,0];  rmx[0,1]:=mx[1,0];  rmx[0,2]:=mx[2,0];
   rmx[1,0]:=mx[0,1];  rmx[1,1]:=mx[1,1];  rmx[1,2]:=mx[2,1];
   rmx[2,0]:=mx[0,2];  rmx[2,1]:=mx[1,2];  rmx[2,2]:=mx[2,2];

   for j:=0 to vs.count-1 do
   With vs[j] do
   begin
    x:=x-hnode1.x;
    y:=y-hnode1.y;
    z:=z-hnode1.z;
    MultVM3(rmx,x,y,z);
   end;
  end;

   for j:=0 to vs.count-1 do
   With vs[j] do
   begin
    x:=x-hnode.pivotx;
    y:=y-hnode.pivoty;
    z:=z-hnode.pivotz;
   end;

  end;

 branch.free;
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



Procedure Writes(const s:string);
begin
 Writeln(t,s);
end;

Procedure WriteFmt(const fmt:string; vs:Array of const);
begin
 Writeln(t,Sprintf(fmt,vs));
end;

var i,j,k,v,nc:integer;
    s:string;
    rad:double;

begin
 Assign(t, name);
 Rewrite(t);

 UnOffsetMeshes;

Try
 WriteFmt('# MODEL %s created by SED %s',[name, SedVersion]);
 WriteS('');
 WriteS('SECTION: HEADER');
 WriteS('');
 WriteFmt('3DO %.1f', [version]);
 WriteS('');
 WriteS('SECTION: MODELRESOURCE');
 WriteS('');
 WriteFmt('MATERIALS %d',[Mats.count]);
 For i:=0 to mats.count-1 do
 begin
  WriteFmt('%d: %s',[i,Mats[i]]);
 end;

 Rad:=FindRadius;

 WriteS('');
 WriteS('SECTION: GEOMETRYDEF');
 WriteS('');
 WriteFmt('RADIUS %.6f',[rad]);
 WriteS('');
 WriteS('INSERT OFFSET 0 0 0');
 WriteS('');
 WriteS('GEOSETS 1');
 WriteS('');
 WriteS('GEOSET 0');
 WriteS('');
 WriteFmt('MESHES %d',[Meshes.count]);

 for i:=0 to Meshes.count-1 do
 With meshes[i] do
 begin
  WriteS('');
  WriteFmt('MESH %d',[i]);
  WriteS('NAME ' + name);
  Rad:=1;

  rad:=FindRadius;

  WriteFmt('RADIUS %.6f',[rad]);
  WriteS('GEOMETRYMODE	4');
  WriteS('LIGHTINGMODE	3');
  if IsClose(version, 2.3) then WriteS('TEXTUREMODE	3') // IJIM
  else WriteS('TEXTUREMODE	1');
  WriteS('');

  WriteFmt('VERTICES %d',[Vertices.count]);
  WriteS('');
  For j:=0 to Vertices.count-1 do
  With Vertices[j] do
    if IsClose(version, 2.3) then
      WriteFmt('%d: %.6f %.6f %.6f 0.000000 0.000000 0.000000 1.000000',[j,X,Y,Z]) // IJIM
    else
      WriteFmt('%d: %.6f %.6f %.6f 0',[j,X,Y,Z]);
  WriteS('');

  {!!!! Texture vertices !!!}
  txIdx:=TTxVertices.Create;
  TXVXList:=TTxVertices.Create;

  For j:=0 to faces.Count-1 do
  With faces[j] do
  for k:=0 to txvertices.count-1 do
  begin
   txvx:=TxVertices[k];
   txvx.num:=AddTXVX(txvx);
  end;

  WriteFmt('TEXTURE VERTICES %d',[TxIdx.count]);
  WriteS('');
  txIdx.free;

  For j:=0 to txVXList.count-1 do
  With txVXList[j] do
  begin
   WriteFmt('%d: %.2f %.2f',[j,u,v]);
  end;
  WriteS('');
  TXVXList.Free;

  {!!!! vertex normals !!!}
  For j:=0 to Faces.count-1 do Faces[j].Recalc;

  VXNorms:=TList.Create;

  For j:=0 to Vertices.count-1 do
  With Vertices[j] do
  begin
   New(pn);
   pn.dx:=0; pn.dy:=0; pn.dz:=0;
   VXNorms.Add(pn);

   num:=j; {num will be number of vertex}
   mark:=0; {mark will be number of references to VX}
  end;

  For j:=0 to Faces.count-1 do
  With Faces[j] do
  For k:=0 to vertices.count-1 do
  With Vertices[k] do
  begin
   pn:=VXNorms[num];
   pn.dx:=pn.dx+normal.dx;
   pn.dy:=pn.dy+normal.dy;
   pn.dz:=pn.dz+normal.dz;
   inc(mark);
  end;

  WriteS('VERTEX NORMALS');
  WriteS('');

 For j:=0 to Vertices.count-1 do
 With Vertices[j] do
 begin
  pn:=VXNorms[j];
  if mark<>0 then
  begin
   pn.dx:=pn.dx/mark;
   pn.dy:=pn.dy/mark;
   pn.dz:=pn.dz/mark;
  end;
  WriteFmt('%d: %.6f %.6f %.6f',[j,pn.dx,pn.dy,pn.dz]);
  Dispose(pn);
 end;
  VXNorms.free;
  WriteS('');

  WriteFmt('FACES %d',[Faces.count]);
  WriteS('');

  For j:=0 to Faces.count-1 do
  With Faces[j] do
  begin
   {#  num:  material:   type:  geo:  light:   tex:  extralight:  verts:}

   if IsClose(version, 2.3) then// IJIM
      s:=Sprintf('%d: %d 0x%x %d %d %d (%.6f/%.6f/%.6f/%.6f) %d',
        [j,imat,faceflags,geo,light,tex,extraLight.r,extraLight.g,extraLight.b,extraLight.a,vertices.count])
    else
      s:=Sprintf('%d: %d 0x%x %d %d %d %.4f %d',
        [j,imat,faceflags,geo,light,tex,RgbaToIntensity(extraLight),vertices.count]);

   for v:=0 to Vertices.Count-1 do
   With Vertices[v] do
    s:=Concat(s,Format(' %d, %d',[num,TXVertices[v].num]));
   WriteS(s);
  end;

 WriteS('');
 WriteS('FACE NORMALS');
 WriteS('');

 for j:=0 to Faces.count-1 do
 With Faces[j] do
 begin
  WriteFmt('%d: %.6f %.6f %.6f',[j,normal.dx,normal.dy,normal.dz]);
 end;
 end;

 WriteS('');
 WriteS('SECTION: HIERARCHYDEF');
 WriteS('');

 WriteFmt('HIERARCHY NODES %d',[hnodes.count]);
   WriteS('# num: flags: type: mesh: parent: child:  sibling:  numChildren: x:   y:  z:  pitch: yaw: roll: pivotx: pivoty: pivotz: hnodename:');
 for i:=0 to hnodes.count-1 do
 begin
  // Count number of children,  find values for child and sibling
  nc:=0; k:=-1; v:=-1;

  for j:=0 to hnodes.count-1 do
  begin
   if hnodes[j].parent<>i then continue;
   inc(nc);
   if k=-1 then k:=j;
  end;

  for j:=i+1 to hnodes.count-1 do
  begin
   if hnodes[j].parent=hnodes[i].parent then begin v:=j; break; end;
  end;

  with hnodes[i] do
  WriteS(
//     #  num:   flags:   type:    mesh:  parent:  child:  sibling:  numChildren:        x:         y:         z:     pitch:       yaw:      roll:    pivotx:    pivoty:    pivotz:  hnodename:

  Format('%2d:     0x0    0x%x    %2d     %2d      %2d     %2d       %2d     %3.6f %3.6f %3.6f  %4.6f %4.6f %4.6f %4.6f %4.6f %4.6f %s',

           [i,            ntype, nmesh,  parent,     k,      v,       nc,     x,      y,    z,    pch,   yaw,  rol, pivotx, pivoty, pivotz,              nodename]));
 end;

{

 if meshes.count=1 then
 begin
  WriteFmt('HIERARCHY NODES %d',[Meshes.count]);
  WriteS('# num: flags: type: mesh: parent: child:  sibling:  numChildren: x: y: z: pitch: yaw: roll: pivotx: pivoty: pivotz: hnodename:');
  WriteFmt('0:     0x0    0x1   0     -1      -1      -1        0            0  0  0  0.0    0.0  0.0   0.0     0.0    0.0     %s',
           [Meshes[0].Name]);
 end
 else
 begin
  WriteFmt('HIERARCHY NODES %d',[Meshes.count+1]);
  WriteS('# num: flags: type: mesh: parent: child:  sibling:  numChildren: x: y: z: pitch: yaw: roll: pivotx: pivoty: pivotz: hnodename:');
  WriteFmt('0:     0x0    0x1   -1     -1       1      -1        %d            0  0  0  0.0    0.0  0.0   0.0     0.0    0.0    $$dummy',
         [Meshes.Count]);
  For i:=0 to Meshes.count-1 do
  begin
   if i=meshes.count-1 then j:=-1 else j:=i+2;
  WriteFmt('%d:     0x0    0x1    %d     -1      -1      %d        0            0  0  0  0.0    0.0  0.0   0.0     0.0    0.0     %s',
            [i+1,i,j,Meshes[i].Name]);
  end;
 end;}


finally
  Close(t);
//  OffsetMeshes;
end;

end;


