Procedure TJKLevel.ImportAsc(F:TFileName);
var
     w1,w2:String;
     t:TTextFile;
     n,i,p:Integer;
     s:String;

Procedure GetNextLine(var s:String);
var cmt_pos:word; {Position of #}
begin
 s:='';
 Repeat
  if t.eof then exit;
  t.Readln(s);
  cmt_pos:=Pos('#',s);
  if cmt_pos<>0 then SetLength(s,cmt_pos-1);
  s:=LowerCase(Trim(s));
 Until s<>'';
end; {GetNextLine}

Procedure FScanf(const s:string;const format:String;const Vals:array of pointer);
begin
 if not SScanf(s,format,vals) then
  PanMessage(mt_Error,SysUtils.Format('Conversion error at line %d in file %s',[t.CurrentLine,t.GetFullName]));
end;

Procedure FileError(const s:string);
begin
 PanMessageFmt(mt_warning,'Error loading file: %s at line %d',[f,t.curLine]);
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

Procedure LoadTriMesh(const s:string);
var i,j,nvx,nvxs,nsfs:integer; tmp,w:string;
    v:TJKVertex;
    sec:TJKSector;
    surf:TJKSurface;
    a,b,c:integer;
begin
if not SScanf(s,'%s vertices: %d faces: %d',[@tmp,@nvxs,@nsfs]) then exit;
 GetNextLine(tmp);
 getword(tmp,1,w);
 if w<>'vertex' then begin FileError('"Vertex List:" expected '+tmp+' found'); exit; end;


 sec:=NewSector;
 for i:=0 to nvxs-1 do
 begin
  GetNextLine(tmp);
  for j:=1 to length(tmp) do
   if tmp[j]=':' then tmp[j]:=' ';
   v:=sec.NewVertex;
   FScanf(tmp,'vertex %d x %f y %f z %f',[@nvx,@v.x,@v.y,@v.z]);
   if (i<>nvx) then FileError('Vertex '+IntToStr(nvx)+' expected '+tmp+' found');
 end;

 GetNextLine(tmp);
 getword(tmp,1,w);
 if w<>'face' then begin FileError('"Face List:" expected '+tmp+' found'); exit; end;

 i:=0;

 repeat
  if i>=nsfs then break;
  GetNextLine(tmp);
  getword(tmp,1,w);
  if w<>'face' then continue;
  for j:=1 to length(tmp) do
   if tmp[j]=':' then tmp[j]:=' ';

  FScanf(tmp,'face %d a %d b %d c %d',[@nvx,@a,@b,@c]);
  if (i<>nvx) then FileError('Face '+IntToStr(nvx)+' expected '+tmp+' found');
  inc(i);

  surf:=sec.newSurface;
  Surf.AddVertex(sec.vertices[a]);
  surf.AddVertex(sec.vertices[b]);
  surf.AddVertex(sec.vertices[c]);
  sec.surfaces.Add(surf);
 until t.eof;

 sectors.Add(sec);

 for i:=0 to sec.surfaces.count-1 do
  sec.surfaces[i].NewRecalcAll;

  Sec.Renumber;

end;

Procedure FinishUp;
var th:TJKThing;
begin
 RenumSecs;
 th:=NewThing;
 Things.Add(th);
end;

const
     pi_steps=100;
var
     fsize,ppos:integer;


Procedure DO_Progress;
var pos:integer;
begin
 pos:=Trunc(t.fpos*pi_steps/fsize);
 While pos>ppos do
 begin
  inc(ppos);
  Progress.Step;
 end;
end;

var j:integer;

begin {TLevel.ImportAsc}
 Try
 t:=TLECTextFile.CreateRead(OpenFileRead(F,0));
 Clear;
 fsize:=t.fsize;
 ppos:=0;
 Progress.Reset(pi_steps);
 Progress.Msg:='Importing ASC';
 Try
 Try
  While not t.eof do
  begin
   GetNextLine(s);
   p:=GetWord(s,1,w1); {w1 - first word, w2 - second}
   if w1='tri-mesh,' then
   begin
    LoadTriMesh(s);

    try
     DO_Progress;
     except
     on Exception do ;
    end;

   end;
  end;

 Except
  On E:EConvertError do
   Raise Exception.CreateFmt('File %s: Error at line %d: %s',[F,t.CurrentLine,E.Message]);
 end;

 Finally
  Progress.hide;
  t.Fclose;
  FinishUp;
  MasterCMP:='';
 end;

 Except
  On E:EInOutError do PanMessage(mt_error,E.Message+' '+F);
  On E:Exception do PanMessage(mt_error,E.Message);
 end;

end;
