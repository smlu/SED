unit jlh_jll;

interface
Uses ComCtrls;

Procedure LoadjLH(const FName:String;TV:TTreeView);

implementation
uses Classes, Misc_utils, SysUtils, GlobalVars;

Function LoadOLL(const Name:String):TStringList;
var t:Text;s:String;sl:TStringList;
    p:Integer; MainList:TStringList;
begin
 Assign(t,FindLMDataFile(Name)); Reset(t);
Try
 MainList:=TStringList.Create;
 sl:=nil;
 While not eof(t) do
 begin
  Readln(t,s);
  RemoveComment(s);
  if s='' then continue;
  if s[1]='[' then
  begin
   sl:=TStringList.Create;
   p:=Pos(']',s); if p=0 then p:=Length(s);
   s:=Copy(s,2,p-2);
   MainList.AddObject(s,sl);
   Continue;
  end;
  if sl<>nil then sl.Add(Trim(s));
 end;
 Result:=MainList;
Finally
 Close(t);
end;
end;

Procedure LoadJLH(const FName:String;TV:TTreeView);
var sl:TStringList;
    t:Text;
    stack:TList;
    SP:Integer;
    CurNode:TTreeNode;
    OllName,s,w:String;
    p:Integer;


Procedure PushNode(Node:TTreeNode);
begin
 Sp:=Stack.Add(Node);
end;

Function PopNode:TTreeNode;
begin
 if SP=0 then begin result:=nil; exit; end;
 Result:=TTreeNode(Stack[SP-1]);
 Stack.Delete(SP-1);
 Dec(SP);
end;

Procedure ReadList(const s:string;p:Integer);
var np,li:Integer;
begin
 np:=GetWord(s,p,w);
 li:=sl.IndexOf(w);
 if li=-1 then begin PanMessage(mt_warning,Format('Cannot find list %s in %s',[w,OllName])); exit; end;
 w:=Copy(s,np,Length(s)-np+1);
 TV.Items.AddChildObject(CurNode,w,sl.Objects[li]);
end;

Procedure AddNode(const s:string;p:Integer);
begin
 PushNode(CurNode);
 w:=Copy(s,p,Length(s)-p+1);
 CurNode:=TV.Items.AddChild(CurNode,w);
end;

begin
Try
 CurNode:=nil;
 Stack:=TList.Create; SP:=0;
 Assign(t,FindLMDataFile(FName));
 Reset(t);
 OllName:=ChangeFileExt(Fname,'.oll');
 sl:=LoadOLL(OllName);

Try
 While not eof(t) do
 begin
  Readln(t,s);
  RemoveComment(s);
  if s='' then continue;
  p:=GetWord(s,1,w);
  if w='LIST' then ReadList(s,p)
  else if w='SUB' then AddNode(s,p)
  else if w='SUBEND' then CurNode:=PopNode;
 end;

Finally
 Stack.Free;
 sl.Free;
 Close(t);
end;

Except
 On E:EInOutError do PanMessage(mt_Warning,E.Message+' '+FName);
end;


end;

end.
