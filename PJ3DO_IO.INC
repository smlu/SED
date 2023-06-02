
Type
TTempTXVertex=class
  u,v:single;
end;

Constructor TPJ3DO.CreateFrom3DO(const name:string;lod:integer);
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
  GetWord(s,p,w1);
  mats.Add(w1);
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

Procedure LoadGeometry;
var i,j,k,n,p,pc:integer;
    nvx,vx,tvx:integer;
    txvx:TTempTXVertex;

    mvx:TVertex;

    ngeos:integer;
    nmeshes,nvxs,nfaces:integer;
    mesh:T3DOMesh;
    face:T3DOFace;
    v:TVertex;
    tv:TTempTXVertex;

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
 SScanf(s,'INSERT OFFSET  %f %f %f',[@InsX,@InsY,@InsZ]);
 GetNextLine(s); {Geosets}
 SScanf(s,'GEOSETS %d',[@ngeos]);

 if lod>ngeos-1 then lod:=ngeos-1; 


 While not EndOfSection do
 begin
  GetNextLine(s);
  p:=GetWord(s,1,w1);
  if w1<>'GEOSET' then continue;
  GetWord(s,p,w1);
  if StrToInt(w1)<>lod then continue;
  break;
 end;
 if EndOfSection then exit;

 GetNextLine(s); {Meshes}
 SScanf(s,'MESHES %d',[@nmeshes]);

 for i:=0 to nmeshes-1 do
 begin

  While not EndOfSection do
  begin
   GetNextLine(s);
   p:=GetWord(s,1,w1);
   if w1<>'MESH' then continue;
   break;
  end;
  if EndOfSection then exit;

  Mesh:=NewMesh;
  Meshes.Add(Mesh);

  GetNextLine(s); {Name} Mesh.Name:=GetWordN(s,2);
(*  GetNextLine(s); {Radius}
  GetNextLine(s); {GEOMETRYMODE}
  GetNextLine(s); {LIGHTINGMODE}
  GetNextLine(s); {TEXTUREMODE}*)

  While not EndOfSection do
  begin
   GetNextLine(s);
   p:=GetWord(s,1,w1);
   if w1<>'VERTICES' then continue;
   SScanf(s,'VERTICES %d',[@Nvxs]);
   break;
  end;
  if EndOfSection then exit;


(*  GetNextLine(s); {Vertices} *)

  SScanf(s,'VERTICES %d',[@Nvxs]);
  for j:=0 to nvxs-1 do
  begin
   GetNextLine(s);
   v:=TVertex.Create;
   SScanf(s,'%s %f %f %f',[@w1,@v.x,@v.y,@v.z]);
   Mesh.Vertices.Add(v);
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
   if w1<>'FACES' then continue;
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
  ValHex(w1,face.ftype);
  p:=GetWord(s,p,w1); {geo}
  ValInt(w1,face.geo);
  p:=GetWord(s,p,w1); {light}
  ValInt(w1,face.light);
  p:=GetWord(s,p,w1); {tex}
  ValInt(w1,face.tex);
  p:=GetWord(s,p,w1); {extralight}
  ValSingle(w1,face.extra_L);
  p:=GetWord(s,p,w1); {Nvert}
  nvxs:=StrToInt(w1);
  for k:=0 to nvxs-1 do
  begin
   p:=GetVXWord(s,p,w1);
   ValInt(w1,nvx);
   Face.AddVertex(Mesh.Vertices[nvx]);
   p:=GetWord(s,p,w1);
   ValInt(w1,nvx);
   tv:=GetTXVX(nvx);
   if tv<>nil then
   With Face.TXVertices[k] do
   begin
    u:=tv.u;
    v:=tv.v;
   end;
  end;
end;
  for i:=0 to GTXVXList.Count-1 do
  TTempTXVertex(GTXVXList[i]).Free;
  GTXVXList.Clear;
end;
GTXVXList.Free;
end;


Procedure LoadHierarchy;
var i,j,n,p,nsec:integer;
    hnode:T3DOHnode;
begin
 CurSection:='';
 GetNextLine(s);
 SScanf(s,'HIERARCHY NODES %d',[@n]);
 for i:=0 to n-1 do
 begin
  GetNextLine(s);
{#  num:   flags:   type:    mesh:  parent:  child:  sibling:  numChildren:        x:         y:         z:     pitch:       yaw:      roll:    pivotx:    pivoty:    pivotz:  hnodename:
     0:  0x0000 0x00010       -1       -1       1        -1             2   0.000000   0.000000   0.000000   0.000000   0.000000   0.000000   0.000000   0.000000   0.000000  $$$dummy}

  hnode:=T3DOHNode.Create;
  With hnode do
  SSCanf(s,'%s %s %s %d %d %s %s %s %f %f %f %f %f %f %f %f %f %s',
           [@w1,@w1,@w1,@nmesh,@parent,@w1,@w1,@w1,@Orgx,@Orgy,@Orgz,@Orgpch,@Orgyaw,@Orgrol,@pivotx,@pivoty,@pivotz,@meshname]);
  {if nmesh>0 then hnode.Mesh:=Meshes[nmesh];}
  HNodes.Add(Hnode);
 end;
end;


var i,j:integer;
begin {CreateFrom3DO begin}
 CreateNew;
 DecimalSeparator:='.';

 if ExtractFilePath(name)<>'' then t:=TTextFile.CreateRead(OpenFileRead(name,0)) else
 t:=TTextFile.CreateRead(OpenGameFile(name));
 cur_line:=0;
 EndOfSection:=false;
 CurSection:='';
Try
Try
Repeat
While (CurSection='') and (not t.eof) do GetNextLine(s);
 if t.eof then break;
 EndOfSection:=false;
 if CurSection='GEOMETRYDEF' then LoadGeometry
 else if CurSection='HEADER' then CurSection:=''
 else if CurSection='MODELRESOURCE' then LoadMats
 else if CurSection='HIERARCHYDEF' then LoadHierarchy
 else SkipToNextSection;
until t.eof;

Finally
  SetDefaultOffsets;
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



