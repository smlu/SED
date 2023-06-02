unit u_cscene;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  StdCtrls, ComCtrls, J_Level, Prender, u_pj3dos, U_pjkey,ResourcePicker,
  CommCtrl, u_templates, values, u_Preview, Buttons, ExtCtrls;

type
  TKeyForm = class(TForm)
    LVThings: TListView;
    EBTime: TEdit;
    Label1: TLabel;
    BNKey: TButton;
    BNShow: TButton;
    PBScene: TPaintBox;
    TrackBar1: TTrackBar;
    BNPlay: TBitBtn;
    procedure LVThingsChange(Sender: TObject; Item: TListItem;
      Change: TItemChange);
    procedure EBTimeChange(Sender: TObject);
    procedure BNKeyClick(Sender: TObject);
    procedure FormCreate(Sender: TObject);
    procedure BNShowClick(Sender: TObject);
  private
    { Private declarations }
  public
    { Public declarations }
    Procedure ShowKeyDialog;
  end;

var
  KeyForm: TKeyForm;

implementation

{$R *.DFM}

Procedure TKeyForm.ShowKeyDialog;
var i:integer;
    li:TListItem;
begin
 LVThings.Items.BeginUpdate;
try
 LVThings.Items.Clear;

 for i:=0 to Level.things.count-1 do
 begin
  li:=LVThings.Items.Add;
  li.Caption:=IntToStr(i);
  li.SubItems.Add(level.things[i].name);
  li.SubItems.Add('');
  li.SubItems.Add('0');
 end;

finally
 LVThings.Items.EndUpdate;
 Preview3D.ShowPreview;
 Show;
end;
end;


procedure TKeyForm.LVThingsChange(Sender: TObject; Item: TListItem;
  Change: TItemChange);
begin
 if Item.SubItems.COunt<3 then exit;
 EBTime.Text:=Item.Subitems[2];
end;

procedure TKeyForm.EBTimeChange(Sender: TObject);
var i,a:integer;
begin
 if LVThings.ItemFocused=nil then exit;
 Val(EBTime.Text,i,a);
 if a=0 then LVThings.ItemFocused.SubItems[2]:=IntToStr(i);
end;

procedure TKeyForm.BNKeyClick(Sender: TObject);
var li:TListItem;
begin
 li:=LVThings.ItemFocused;
 if li=nil then exit;
 li.SubItems[1]:=ResPicker.PickKEY(li.SubItems[1]);
{ if li.Data<>nil then TKeyFile(li.Data).Destroy;
 li.Data:=nil;
 li.Data:=TKeyFile.CreateFromKEY(li.SubItems[1]);}
end;

procedure TKeyForm.FormCreate(Sender: TObject);
var
  Styles: DWORD;
const
 LVM_SETEXTENDEDLISTVIEWSTYLE = LVM_FIRST + 54;
 LVM_GETEXTENDEDLISTVIEWSTYLE = LVM_FIRST + 55;
 LVS_EX_FULLROWSELECT    = $00000020;
begin
 Styles:=LVThings.Perform(LVM_GETEXTENDEDLISTVIEWSTYLE, 0, 0);
 LVThings.Perform(LVM_SETEXTENDEDLISTVIEWSTYLE, 0, Styles or LVS_EX_FULLROWSELECT);
end;

procedure TKeyForm.BNShowClick(Sender: TObject);
var i:integer;
    a3DO,a3DO1:TPJ3DO;
    th:TJKThing;
    itpl:integer;
    tval:TTPLValue;
    li:TListItem;
    kf:TKeyFile;
begin
 a3Do:=nil;
 for i:=0 to LVThings.Items.Count-1 do
 begin
  li:=LVThings.Items[i];
  if li.SubItems[1]='' then continue;
  th:=Level.Things[i];
  itpl:=Templates.IndexOfName(th.name);
  if itpl=-1 then continue;
  tval:=Templates.GetTPLField(th.name,'model3d');
  if tval=nil then a3Do:=nil else a3Do1:=Load3DO(tval.AsString);
  Free3DO(a3DO);
  a3DO:=a3DO1;
  if li.SubItems[1]<>'' then
  begin
   kf:=TKeyFile.CreateFromKEY(li.SubItems[1]);
   kf.ApplyKey(a3DO,StrToInt(li.SubItems[2]));
   kf.free;
  end;
  Preview3D.SetThing3DO(th,a3do);
 end;
 Preview3D.Invalidate;
end;

end.
