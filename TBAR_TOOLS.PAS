unit tbar_tools;

interface
uses Classes,ExtCtrls,Buttons,misc_utils;

Procedure LoadToolBar(bar:TPanel;const str:string);

implementation
uses JED_Main;

Type
 TButtonProc=procedure;

 TButtonObj=class
  comment:string;
  proc:TButtonProc;
  Procedure Onclick(Sender: TObject);
 end;

var buttons:TStringList;

Procedure TButtonObj.Onclick;
begin
 proc;
end;

Procedure AddButton(code:string;const comment:string;proc:TButtonProc);
var nb:TButtonObj;
begin
 nb:=TButtonObj.Create;
 nb.comment:=comment;
 nb.proc:=proc;
 buttons.AddObject(code,nb);
end;

Procedure LoadToolBar(bar:TPanel;const str:string);
var i:integer;
    bn:TSpeedButton;
    w:string;
    p,nb:integer;
    bo:TButtonObj;
begin
 for i:=bar.controlcount-1 downto 0 do bar.controls[i].free;
 p:=1;
 Repeat
  p:=GetWord(str,p,w);
  nb:=buttons.indexof(w);
  if nb<>-1 then
  begin
   bo:=TButtonObj(buttons.objects[nb]);
   bn:=TSpeedButton.Create(bar.owner);
   bn.parent:=bar;
   bn.hint:=bo.comment;
   bn.ShowHint:=true;
   bn.onClick:=bo.OnClick;
   bn.left:=2; bn.top:=2;
  end;
Until p>length(str);
end;

Procedure OpenProj;
begin
 JedMain.OpenMenu.Click;
end;

Initialization
begin
 buttons:=TStringList.Create;
 buttons.Sorted:=true;
 buttons.duplicates:=dupError;
 AddButton('op','Open Project',OpenProj);
end;

end.
