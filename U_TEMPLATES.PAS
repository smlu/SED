unit u_templates;

interface
uses Classes, Values, Files, FileOperations, Misc_utils, GlobalVars, Windows;

type

TTemplate=class
 Name,parent:String;
 vals:TTPLValues;
 desc:string;
 bbox:TThingBox;
 Constructor Create;
 Destructor Destroy;override;
 Function GetAsString:string;
 Function ValsAsString:String;
end;

TTemplates=class(TList)
 Constructor Create;
 Function GetItem(n:integer):TTemplate;
 Procedure SetItem(n:integer;v:TTemplate);
Property Items[n:integer]:TTemplate read GetItem write SetItem; default;
 Procedure Clear;
 Procedure LoadFromFile(const fname:string);
 Procedure SaveToFile(const fname:string);
 Function AddFromString(const s:string):Integer;
 function GetAsString(n:integer):String;
 Function IndexOfName(const name:string):integer;
 Function GetTPLField(const tpl,field:string):TTPLValue;
 Function GetNTPLField(ntpl:integer;const field:string):TTPLValue;
 Procedure DeleteTemplate(n:integer);
Private
 cbox:TThingBox;
 cdesc:string;
 names:TStringList;
end;


var
 Templates:TTemplates;

implementation

uses Forms;

Constructor TTemplate.Create;
begin
 vals:=TTPLValues.Create;
end;

Destructor TTemplate.Destroy;
var i:integer;
begin
 for i:=0 to Vals.Count-1 do Vals[i].Free;
 Vals.Free;
end;

Function TTemplate.GetAsString:string;
var i:integer;
begin
 Result:=PadRight(Name,17)+' '+PadRight(Parent,17)+' ';
 for i:=0 to Vals.count-1 do
 With Vals[i] do Result:=Concat(Result,' ',Name,'=',AsString);
end;

Function TTemplate.ValsAsString:String;
var i:integer;
begin
 result:='';
 for i:=0 to Vals.count-1 do
 With Vals[i] do Result:=Concat(Result,' ',Name,'=',AsString);
end;

Constructor TTemplates.Create;
begin
 names:=TStringList.Create;
 names.sorted:=true;
end;

Function TTemplates.GetItem(n:integer):TTemplate;
begin
 if (n<0) or (n>=count) then raise EListError.CreateFmt('Template Index is out of bounds: %d',[n]);
 Result:=TTemplate(List[n]);
end;

Procedure TTemplates.SetItem(n:integer;v:TTemplate);
begin
 if (n<0) or (n>=count) then raise EListError.CreateFmt('Template Index is out of bounds: %d',[n]);
 List[n]:=v;
end;

Procedure TTemplates.Clear;
var i:integer;
begin
 Names.Clear;
 for i:=0 to Templates.count-1 do Templates[i].Free;
 Inherited Clear;
end;

Function TTemplates.IndexOfName(const name:string):integer;
var i:integer;
begin
 Result:=-1;
 i:=Names.IndexOf(name);
 if i<>-1 then Result:=Integer(names.Objects[i]);
{ for i:=0 to count-1 do
  if CompareText(Items[i].Name,name)=0 then
  begin
   Result:=i;
   break;
  end;}
end;

Procedure TTemplates.DeleteTemplate(n:integer);
var tpl:TTemplate;
    i:integer;
begin
 tpl:=GetItem(n);
 i:=Names.IndexOf(tpl.name);
 if i<>-1 then Names.Delete(i);
 Delete(n);
 tpl.free;
end;

Function TTemplates.AddFromString(const s:string):Integer;
var p,peq:integer;w:string;
    tpl:TTemplate;
    vl:TTPLValue;
begin
 result:=-1;
 p:=GetWord(s,1,w);
 if w='' then exit;
 if IndexOfName(w)<>-1 then exit;
 tpl:=TTemplate.Create;
 tpl.name:=w;

 Result:=Add(tpl);

 Names.AddObject(w,TObject(Result));

 p:=GetWord(s,p,w); tpl.parent:=w;

 While p<length(s) do
 begin
  p:=GetWord(s,p,w);
  if w='' then continue;
  peq:=Pos('=',w);
  if peq=0 then continue;
  vl:=TTPLValue.Create;
  tpl.vals.Add(vl);
  vl.Name:=Copy(w,1,peq-1);
  vl.vtype:=GetTPLVType(vl.Name);
  vl.atype:=GetTPLType(vl.Name);
  vl.s:=Copy(w,peq+1,Length(w)-peq);
 end;
 tpl.bbox:=cbox;
 tpl.desc:=cdesc;
 FillChar(cbox,sizeof(cbox),0);
 cdesc:='';
end;

Procedure TTemplates.LoadFromFile(const fname:string);
var t:TTextFile;
    s,w:string;
    f:TFile;
    i,p:integer;
begin
try
 f:=OpenFileRead(fname,0);
except
 on Exception do
 begin
  MsgBox('Cannot open master template - '+fname,'Error',mb_ok);
  Application.Terminate;
  exit;
 end;
end;
 t:=TTextFile.CreateRead(f);
 Clear;
 Try
  While not t.eof do
  begin
   T.Readln(s);
   p:=getword(s,1,w);
   if w='' then continue;
   if w='#' then
   begin
    p:=getWord(s,p,w);
    if CompareText(w,'DESC:')=0 then cdesc:=Trim(Copy(s,p,length(s)))
    else if CompareText(w,'BBOX:')=0 then
             With Cbox do SScanf(s,'# BBOX: %l %l %l %l %l %l',[@x1,@y1,@z1,@x2,@y2,@z2]);
   end
   else AddFromString(s);
  end;
 finally
  for i:=0 to templates.count-1 do
  with Templates[i] do
  begin
   {GetTPLField}
  end;
  t.FClose;
 end;
end;

Function TTemplates.GetAsString(n:integer):String;
var l,i:integer;
begin
 Result:=Items[n].GetAsString;
end;

Procedure TTemplates.SaveToFile(const fname:string);
var t:Textfile;
    i:integer;
begin
 Assign(t,fname); Rewrite(t);
 Try
 for i:=0 to Count-1 do
 With Items[i] do
 begin
  Writeln(t,'# DESC: ',desc);
  With bbox do Writeln(t,SPrintf('# BBOX: %.6f %.6f %.6f %.6f %.6f %.6f',[x1,y1,z1,x2,y2,z2]));
  Writeln(t,Self.GetAsString(i));
 end;
 finally
 CloseFile(t);
 end;
end;

Function TTemplates.GetNTPLField(ntpl:integer;const field:string):TTPLValue;
begin
end;


Function TTemplates.GetTPLField(const tpl,field:string):TTPLValue;
var tp:TTemplate;
    vl:TTPlValue;
    i:integer;
    ctpl:string;
    n:integer;
begin
 Result:=nil;
 ctpl:=tpl;
 n:=0;
Repeat
 inc(n);
 if n>=100 then exit;
 i:=IndexOfName(ctpl);
 if i=-1 then exit;
 tp:=Items[i];
 for i:=0 to tp.vals.count-1 do
 begin
  vl:=tp.Vals[i];
  if CompareText(vl.name,field)=0 then
  begin
   result:=vl; exit;
  end;
 end;
 ctpl:=tp.parent;
until CompareText(ctpl,'none')=0;
end;

Initialization
begin
 InitValues;
 Templates:=TTemplates.Create;
 {templates.LoadFromFile(BaseDir+'jeddata\master.tpl');}
end;

Finalization
begin
 Templates.Clear;
 Templates.Free;
end;


end.
