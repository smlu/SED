unit u_StrEdit;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  StdCtrls, ExtCtrls,  U_medit;

type
  TStrEdit = class(TForm)
    LBStrs: TListBox;
    Panel1: TPanel;
    EBKey: TEdit;
    EBStr: TEdit;
    Label1: TLabel;
    Label2: TLabel;
    BNAdd: TButton;
    BNDelete: TButton;
    procedure LBStrsClick(Sender: TObject);
    procedure BNDeleteClick(Sender: TObject);
    procedure BNAddClick(Sender: TObject);
  private
    { Private declarations }
    cstrs:TCogStrings;
  public
   Procedure EditStrings(strs:TCogStrings);
    { Public declarations }
  end;

var
  StrEdit: TStrEdit;

implementation

{$R *.DFM}


Procedure TStrEdit.EditStrings(strs:TCogStrings);
var i:integer;
begin
 LBStrs.Items.Clear;
 for i:=0 to strs.strings.count-1 do
  LBStrs.Items.Add(strs.Keys[i]+': '+strs.strings[i]);
 LBStrs.ItemIndex:=0;
 cstrs:=strs;
 ShowModal;
end;

procedure TStrEdit.LBStrsClick(Sender: TObject);
var i:integer;
begin
 i:=LBStrs.ItemIndex;
 if i<0 then exit;
 EBKey.Text:=cstrs.Keys[i];
 EBStr.Text:=cstrs.Strings[i];
end;

procedure TStrEdit.BNDeleteClick(Sender: TObject);
var i:integer;
begin
 for i:=LBStrs.Items.count-1 downto 0 do
 if LBStrs.Selected[i] then
 begin
  LBStrs.Items.Delete(i);
  cstrs.Keys.Delete(i);
  cstrs.Strings.Delete(i);
 end;
end;

procedure TStrEdit.BNAddClick(Sender: TObject);
var i:integer;
    s:string;
begin
 s:=EBKey.Text;
 i:=cstrs.Keys.IndexOf(s);
 if i<>-1 then
 begin
  cstrs.Strings[i]:=EBStr.Text;
  LBStrs.Items[i]:=cstrs.Keys[i]+': '+cstrs.strings[i];
 end
 else
 begin
  cstrs.AddString(EBKey.Text,EBStr.Text);
  i:=cstrs.Keys.IndexOf(s);
  LBStrs.Items.Insert(i,cstrs.Keys[i]+': '+cstrs.strings[i]);
 end;
 LBStrs.ItemIndex:=i;
 LBStrs.Selected[i]:=true;
end;

end.
