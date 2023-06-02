unit Prefab;

interface
uses Classes,J_Level, geometry, lev_utils, FileOperations, Files,
     misc_utils, sysUtils;

Var
 Prefabs:TStringList;

Function GetPreFab(n:integer):TJKSector;
Procedure LoadPrefabs(const fname:string);
Procedure SavePrefabs(const fname:string);
Procedure AddPrefab(const pname:string;s:TJKSector);
procedure DeletePrefab(n:integer);

implementation

Function GetPreFab(n:integer):TJKSector;
begin
 Result:=TJKSector(Prefabs.Objects[n]);
end;

Procedure LoadPrefabs(const fname:string);
var p,i:integer;
    t:TTextFile;
    s,st,w:string;

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

Function LoadSector:TJKSector;
var sec:TJKSector;
    sf,nsf,vx,nv,nsv:integer;
    asec,asf:integer;
    v:TJKVertex;
    surf:TJKSurface;
begin
 sec:=Level.NewSector;
 Result:=sec;
try
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
    ReadFmt('%s %x %x %d %d %d %f %d %d %d %x',
     [@Material,@SurfFlags,@FaceFlags,@geo,@light,@tex,@extralight,@nsv,@asec,@asf,@AdjoinFlags]);
    if Material='-' then Material:='';
    {Num:=asec shl 16+(asf and $FFFF);}
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
 Sec.Renumber;
except
 On E:Exception do
 begin
  PanMessageFmt(mt_error,'Error loading %s at line %d: %s',
                [fname,t.curline,e.message]);
  sec.free; result:=nil;
 end;
end;
end;


var sec:TJKSector;
    sname:string;
begin
 for i:=1 to Prefabs.count-1 do Prefabs.Objects[i].Free;
 For i:=Prefabs.count-1 downto 1 do Prefabs.Delete(i);

 try
  t:=TTextFile.CreateRead(OpenFileRead(fname,0));
 except
 on E:Exception do
 begin
   PanMessage(mt_warning,E.Message+' '+fname);
   exit;
 end;
end;

 try

 while not t.eof do
 begin
  t.Readln(s);
  RemoveComment(s);
  p:=GetWord(s,1,w);
  if CompareText(w,'SEC')=0 then
  begin
   sname:=Trim(Copy(s,p,length(s)));
   sec:=LoadSector;
   if sec<>nil then Prefabs.AddObject(sname,sec);
  end;
 end;


 finally
  t.Fclose;
 end;

end;

procedure DeletePrefab(n:integer);
begin
 if n=0 then exit;
 Prefabs.Objects[n].free();
 Prefabs.Delete(n);
end;

Procedure AddPrefab(const pname:string;s:TJKSector);
var newsec:TJKSector;
    nx,ny,nz:Tvector;
    cx,cy,cz:double;
    i:integer;
begin
 cx:=99999; cy:=99999; cz:=99999;

 for i:=0 to s.vertices.count-1 do
 With s.vertices[i] do
 begin
  if x<cx then cx:=x;
  if y<cy then cy:=y;
  if z<cz then cz:=z;
 end;

 if (cx=99999) or (cy=99999) or (cz=99999) then exit; 

 SetVec(nx,1,0,0);
 SetVec(ny,0,1,0);
 SetVec(nz,0,0,1);
 newsec:=Level.NewSector;
 DuplicateSector(s,newsec,cx,cy,cz,nx,ny,nz,-cx,-cy,-cz);
 Prefabs.AddObject(pname,newsec);
end;


Procedure SavePrefabs(const fname:string);
var i,sf,vx:integer;
    asf,asec:integer;
    st:string;
    t:Text;

Procedure WriteFmt(const fmt:string;v:array of const);
begin
 Writeln(t,Sprintf(fmt,v));
end;

begin
 BackUpFile(fname);
 Assign(t,fname);
 Rewrite(t);

 for i:=1 to Prefabs.count-1 do
 With TJKSector(Prefabs.Objects[i]) do
 begin
  WriteFmt('SEC %s',[Prefabs[i]]);
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

  Writeln(t,'END');
  Writeln(t);
 end;
 close(t);
end;


Procedure InitList;
var js:TJKSector;
    z,x:TVector;
begin
 Prefabs:=TStringList.Create;
 js:=Level.NewSector;
 SetVec(z,0,0,1);
 SetVec(x,1,0,0);
 CreateCubeSec(js,0,0,0,z,x);
 Prefabs.AddObject('Cube',js);
end;

Initialization
InitList;

end.
