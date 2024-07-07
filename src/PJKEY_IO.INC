Constructor TKEYFile.CreateFromKEY(const name:string);
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


Procedure LoadNodes;
var i,j,k,n,p,pc:integer;
    nnodes,nentries:integer;
    node:TKEYNode;
    ke:TKEYEntry;
    w:string;
begin
 CurSection:='';

 GetNextLine(s);
 SScanf(s,'NODES %d',[@nnodes]);

 for i:=0 to nnodes-1 do
 begin
  node:=TKEYNode.Create;
  nodes.Add(node);
  GetNextLine(s); {Node N}
  GetNextLine(s); {Mesh Name}
  SScanf(s,'MESH NAME %s',[@node.nodename]);

  GetNextLine(s); {Entries N}
  SScanf(s,'ENTRIES %d',[@nentries]);
  for j:=0 to nentries-1 do
  begin
   ke:=TKeyEntry.Create;
   node.Entries.Add(ke);
   GetNextLine(s);
   With ke do
   begin
    SScanf(s,'%s %d %x %f %f %f %f %f %f',[@w,@framenum,@Flags,@cx,@cy,@cz,@cpch,@cyaw,@crol]);
    GetNextLine(s);
    SScanf(s,'%f %f %f %f %f %f',[@dx,@dy,@dz,@dpch,@dyaw,@drol]);
   end; 
  end;
 end;
end;


Procedure LoadHeader;
var w,w2:string;
begin
 CurSection:='';
 GetNextLine(s);
 While not EndOfSection do
 begin
  GetWord(s,GetWord(s,1,w),w2);

  if w='FLAGS' then ValInt(w2,flags)
  else if w='TYPE' then
  else if w='FRAMES' then ValInt(w2,nframes)
  else if w='FPS' then ValInt(w2,fps)
  else if w='JOINTS' then;
  GetNextLine(s);
 end;
end;

var i,j:integer;
begin {CreateFrom3DO begin}
 CreateNew;
 FormatSettings.DecimalSeparator:='.';

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
 if CurSection='KEYFRAME' then LoadNodes
 else if CurSection='HEADER' then LoadHeader
 else SkipToNextSection;
until t.eof;

Finally

t.FClose;
{Post process}
end;
{Free all stuff}

except
 On e:exception do PanMessage(mt_warning,e.message);
end;

end; {CreateFrom3DO end}
