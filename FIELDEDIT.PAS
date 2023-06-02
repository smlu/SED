unit FieldEdit;

interface
uses Grids, Classes, StdCtrls;

{TFieldEdit pseudo control
 For Sector/Wall/Vertex/Object editors
}

type

TVarType=(vtString,vtDouble,vtInteger,vtDword);

TFieldInfo=class
Private
 Grid:TStringGrid;
 row:Integer;
 olds:string;
 Function GetS:String;
 Procedure SetS(const s:String);
 Procedure BadValueMsg;
Public
 id:Integer;
 changed:boolean;
 vtype:TVarType;
 Constructor Create(aGrid:TStringGrid;arow:integer);
 Property s:String read GetS write SetS;
 Procedure ReadString(var st:String);
 Procedure ReadInteger(var i:Integer);
 Procedure Read0to31(var b:Byte);
 Procedure ReadDword(var d:longint);
 Procedure ReadDouble(var d:Double);
end;

TFEHandler=Procedure(Fi:TFieldInfo) of object;
TFEChangeHandler=Function(Fi:TFieldInfo):boolean of object;
TDoneEdit=procedure of object;

TFieldEdit=class
 Private
  grid:TStringGrid;
  fl:TList;
  oldRow:integer;

  Function GetField(n:Integer):TFieldInfo;
  Function GetCurField:TFieldInfo;
  Procedure Changed(row:integer);
  Procedure CallDoneEdit;
 Public
  FieldCount:Integer;
  ONDblClick:TFEHandler;
  ONChange:TFEChangeHandler;
  OnDoneEdit:TDoneEdit;
  Constructor Create(aGrid:TStringGrid);
  Destructor Destroy;override;
  Procedure Clear;
  Procedure DeactivateHandler;
  Property CurrentField:TFieldInfo read GetCurField;
  Property Fields[n:integer]:TFieldInfo read GetField;
  procedure SelectHandler(Sender: TObject; Col, Row: Longint; var CanSelect: Boolean);
  procedure DblClickHandler(Sender: TObject);
  procedure SetTextHandler(Sender: TObject; ACol, ARow: Longint; const Text: string);
  function AddField(const Name:String;ID:Integer;vartype:TVarType):TFieldInfo;
  Procedure AddFieldInt(const Name:string;ID,V:Integer);
  Procedure AddFieldHex(const Name:string;ID:Integer;V:longint);
  Procedure AddFieldFloat(const Name:string;ID:Integer;v:double);
  Procedure AddFieldStr(const Name:string;ID:Integer;const v:string);
  Function GetFieldByID(ID:Integer):TFieldInfo;
  procedure KeyDown(Sender: TObject; var Key: Word;Shift: TShiftState);
  Procedure DoneAdding;
end;


TVIChange=function (const s:string):boolean of object;

TValInput=class
 eb:TEdit;
 cb:TComboBox;
 OldS:string;
 OnChange:TVIChange;
 changed:boolean;
 Constructor Create(aEb:TEdit);
 Constructor CreateFromCB(aCB:TComboBox);
 procedure ChangeHandler(Sender: TObject);
 Procedure SetAsInt(i:integer);
 Function GetS:String;
 Procedure SetAsFloat(d:double);
 Procedure SetAsString(s:string);
 Property S:string read GetS;
 Function AsInt:Integer;
 Function AsFloat:Double;
Private
 Procedure SetS(const s:string);
 Function IntChange(const s:string):boolean;
 Function FloatChange(const s:string):boolean;
end;

Function feValDouble(const s:string;var d:double):boolean;
Function feValInt(const s:string;var i:Integer):boolean;

implementation
uses Misc_Utils, SysUtils, Windows, Messages, Forms;

Constructor TFieldInfo.Create(aGrid:TStringGrid;arow:integer);
begin
 Grid:=aGrid;
 Row:=arow;
end;

Function TFieldInfo.GetS:String;
begin
 Result:=Grid.Cells[1,row];
end;

Procedure TFieldInfo.SetS(const s:String);
begin
 Changed:=true;
 Grid.Cells[1,row]:=s;
 {if Grid.Cells[1,row]=olds then}
end;

Procedure TFieldInfo.BadValueMsg;
begin
 MsgBox(s+' is not a valid value for '+Grid.Cells[0,row],
             'Field Editor',mb_ok);
end;

Procedure TFieldInfo.ReadString(var st:String);
begin
 st:=s;
end;

Procedure TFieldInfo.ReadInteger(var i:Integer);
var v,a:Integer;
begin
 Val(s,v,a);
 if a=0 then i:=v
 else BadValueMsg;
end;

Procedure TFieldInfo.Read0to31(var b:Byte);
var v,a:Integer;
begin
 Val(s,v,a);
 if (a=0) and (v>=-31) and (v<=31) then b:=v
 else BadValueMsg;
end;

Procedure TFieldInfo.ReadDword(var d:longint);
begin
 Try
  d:=StrToDword(s);
 except
  On EConvertError do BadValueMsg;
 end;
end;

Procedure TFieldInfo.ReadDouble(var d:Double);
var a:Integer;
    v:Double;
begin
 Val(s,v,a);
 if a=0 then d:=v
 else BadValueMsg;
end;

Constructor TFieldEdit.Create(aGrid:TStringGrid);
begin
 grid:=aGrid;
 fl:=TList.Create;
 Grid.OnDblclick:=DblClickHandler;
 Grid.OnSelectCell:=SelectHandler;
 Grid.OnSetEditText:=SetTextHandler;
 Grid.OnKeyDown:=KeyDown;
end;

Destructor TFieldEdit.Destroy;
begin
 Grid.OnDblclick:=nil;
 Grid.OnSelectCell:=nil;
 Clear;
 FL.Free;
end;

Procedure TFieldEdit.Clear;
var i:integer;
begin
 For i:=0 to FieldCount-1 do
 begin
  Grid.Cells[0,i]:='';
  Grid.Cells[1,i]:='';
  Fields[i].Free;
 end;
 FL.Clear;
 FieldCount:=0;
 OldRow:=0;
end;

Function TFieldEdit.GetField(n:Integer):TFieldInfo;
begin
 Result:=TFieldInfo(FL.List[n]);
end;

Function TFieldEdit.GetFieldByID(ID:Integer):TFieldInfo;
var i:Integer;
begin
 Result:=nil;
 for i:=0 to FieldCount-1 do
 if Fields[i].ID=ID then begin Result:=Fields[i]; exit; end;
end;

Function TFieldEdit.GetCurField:TFieldInfo;
begin
 if Grid.Row<>-1 then Result:=TFieldInfo(FL.List[Grid.Row]) else Result:=nil;
end;

procedure TFieldEdit.SelectHandler(Sender: TObject; Col, Row: Longint; var CanSelect: Boolean);
var fi:TFieldInfo;
begin
 if (col<>1) or (row>=FieldCount) then CanSelect:=false
 else
 begin
  fi:=Fields[Row];
  if fi=nil then CanSelect:=false
  else if fi.id=-1 then CanSelect:=false
  else CanSelect:=true;
  fi:=Fields[OldRow];
  if fi.s<>fi.olds then Changed(oldRow);
  oldRow:=row;
 end;
end;

procedure TFieldEdit.DblClickHandler(Sender: TObject);
var fi:TFieldInfo;
begin
 With Grid do
 begin
  if (col<>1) or (row>=FieldCount) then exit;
  fi:=Fields[row];
 end;
  if (fi<>nil) and (Assigned(OnDblClick)) then OnDblClick(fi);
end;

Function IsValid(const s:string;vt:TVarType):Boolean;
var a:Integer;
    i:Integer;
    d:Double;
begin
 a:=-1;
 Case vt of
  vtString: a:=0;
  vtInteger: Val(s,i,a);
  vtDouble: Val(s,d,a);
  vtDword: begin
            a:=0;
            Try
            i:=StrToDword(s);
            except
             On EConvertError do a:=-1;
            end;
           end;
 end;
 Result:=a=0;
end;

procedure TFieldEdit.SetTextHandler(Sender: TObject; ACol, ARow: Longint; const Text: string);
var Fi:TFieldInfo; s:string;
begin
{ if (ACol<>1) or (Arow>=FieldCount) then exit;
 fi:=Fields[aRow];
 if fi<>nil then
 begin
  s:=fi.olds;
  if Assigned(OnChange) then
   if not OnChange(fi) then begin if fi.s<>s then fi.s:=s; end
  else fi.changed:=true;
 end;}

end;

Procedure TFieldEdit.DoneAdding;
begin
 Grid.Enabled:=false;
 Grid.Enabled:=true;
end;


function TFieldEdit.AddField(const Name:String;ID:Integer;vartype:TVarType):TFieldInfo;
var fi:TFieldInfo;
begin
 fi:=TFieldInfo.Create(Grid,FieldCount);
 fi.id:=ID;
 fi.vtype:=varType;
 Grid.Cells[0,FieldCount]:=Name;
 FL.Add(fi);
 Inc(FieldCount);
 Grid.RowCount:=FieldCount;
 {Grid.row:=0; Grid.col:=1;}
 result:=fi;
end;

Procedure TFieldEdit.AddFieldHex(const Name:string;ID:Integer;V:longint);
begin
 With AddField(Name,ID,vtInteger) do
 begin
  olds:=Format('%x',[v]);
  s:=olds;
 end;
end;

Procedure TFieldEdit.AddFieldInt(const Name:string;ID,V:Integer);
begin
 With AddField(Name,ID,vtInteger) do
 begin
  olds:=IntToStr(v);
  s:=olds;
 end;
end;

Procedure TFieldEdit.AddFieldFloat(const Name:string;ID:Integer;v:double);
begin
 With AddField(Name,ID,vtInteger) do
 begin
  olds:=DoubleToStr(v);
  s:=olds;
 end;
end;

Procedure TFieldEdit.AddFieldStr(const Name:string;ID:Integer;const v:string);
begin
 With AddField(Name,ID,vtInteger) do
 begin
  olds:=v;
  s:=olds;
 end;
end;

Procedure TFieldEdit.DeactivateHandler;
begin
 if Grid.Row>=FieldCount then exit;
 With Fields[Grid.Row] do
 begin
  if s<>olds then Self.Changed(Grid.Row);
 end;
end;

Procedure TFieldEdit.Changed(row:integer);
var fi:TFieldInfo;
begin
 if not Assigned(OnChange) then exit;
 if row>=FieldCount then exit;
 fi:=Fields[row];
 if not OnChange(fi) then fi.s:=fi.olds else
 begin
  fi.changed:=true;
  fi.olds:=fi.s;
 end;
end;

Procedure TFieldEdit.CallDoneEdit;
begin
 if Assigned(OnDoneEdit) then
  OnDoneEdit;
end;


procedure TFieldEdit.KeyDown(Sender: TObject; var Key: Word;Shift: TShiftState);
var fi:TFieldInfo;
begin
 if shift<>[] then exit;
 if Grid.Row>=FieldCount then exit;
 fi:=Fields[Grid.Row];
 Case Key of
  VK_Escape: begin fi.s:=fi.Olds; CallDoneEdit; end;
  VK_Return: begin Changed(Grid.Row); CallDoneEdit; end;
  VK_F10: if (fi<>nil) and (Assigned(OnDblClick)) then OnDblClick(fi);
 end;
end;

Constructor TVAlInput.Create(aEb:TEdit);
begin
 eb:=aEb;
 eb.OnChange:=ChangeHandler;
 olds:=eb.text;
end;

Constructor TVAlInput.CreateFromCB(aCB:TComboBox);
begin
 cb:=aCB;
 cb.OnChange:=ChangeHandler;
 olds:=cb.Text;
end;


Procedure TVAlInput.SetS(const s:string);
begin
 if eb<>nil then
 begin
  eb.Onchange:=nil;
  olds:=s;
  eb.text:=s;
  eb.OnChange:=ChangeHandler;
  exit;
 end;
  cb.Onchange:=nil;
  olds:=s;
  cb.text:=s;
  cb.OnChange:=ChangeHandler;
end;

procedure TVAlInput.ChangeHandler(Sender: TObject);
begin
 changed:=true;
 if not Assigned(OnChange) then exit;
 if eb<>nil then
 begin
  if OnChange(eb.Text) then olds:=eb.text
  else SetS(olds);
  exit;
 end;
  if OnChange(cb.Text) then olds:=cb.text
  else SetS(olds);
end;

Procedure TVAlInput.SetAsInt(i:integer);
begin
 SetS(IntToStr(i));
 OnChange:=IntChange;
 changed:=false;
end;

Procedure TVAlInput.SetAsFloat(d:double);
begin
 SetS(DoubleToStr(d));
 OnChange:=FloatChange;
 changed:=false;
end;

Procedure TVAlInput.SetAsString(s:string);
begin
 SetS(s);
 OnChange:=nil;
 changed:=false;
end;

Function TVAlInput.IntChange(const s:string):boolean;
var i:integer;
    w:string;
begin
 getWord(s,1,w);
 Result:=feValInt(w,i);
end;

Function TVAlInput.GetS:String;
begin
 if eb<>nil then Result:=eb.text
 else Result:=cb.text;
end;

Function TVAlInput.AsInt:Integer;
var w:string;
begin
 getWord(s,1,w);
 if not feValInt(w,Result) then Result:=0;
end;

Function TVAlInput.AsFloat:Double;
begin
 if not feValDouble(s,Result) then Result:=0;
end;

Function feValDouble(const s:string;var d:double):boolean;
var st:string;
begin
 st:=s;
 if s='-' then begin result:=true; d:=0; exit; end;
 if (st<>'') and (st[length(st)]='.') then SetLength(st,length(st)-1);
 result:=ValDouble(st,d);
end;

Function feValInt(const s:string;var i:Integer):boolean;
var st:string;
begin
 st:=s;
 if s='-' then begin result:=true; i:=0; exit; end;
 result:=ValInt(st,i);
end;


Function TVAlInput.FloatChange(const s:string):boolean;
var d:double;
begin
 Result:=feValDouble(s,d);
end;


end.
