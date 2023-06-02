unit Q_things;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  StdCtrls, Q_Utils, misc_utils, J_Level, Buttons, U_multisel, GlobalVars;

type
  TFindThings = class(TForm)
    Label1: TLabel;
    CBNum: TComboBox;
    EBNum: TEdit;
    Label2: TLabel;
    CBName: TComboBox;
    EBName: TEdit;
    GroupBox1: TGroupBox;
    RBAdd: TRadioButton;
    RBSubtract: TRadioButton;
    RBFocus: TRadioButton;
    BNFind: TButton;
    BNCancel: TButton;
    CheckBox1: TCheckBox;
    BNName: TButton;
    SBHelp: TSpeedButton;
    Label5: TLabel;
    CBPCH: TComboBox;
    EBPCH: TEdit;
    Label6: TLabel;
    CBYAW: TComboBox;
    EBYAW: TEdit;
    Label7: TLabel;
    CBROL: TComboBox;
    EBROL: TEdit;
    Label8: TLabel;
    CBX: TComboBox;
    EBX: TEdit;
    Label9: TLabel;
    CBY: TComboBox;
    EBY: TEdit;
    Label10: TLabel;
    CBZ: TComboBox;
    EBZ: TEdit;
    Label3: TLabel;
    CBLayer: TComboBox;
    EBLayer: TEdit;
    BNLayer: TButton;
    Label4: TLabel;
    CBSec: TComboBox;
    EBSec: TEdit;
    procedure FormCreate(Sender: TObject);
    procedure BNNameClick(Sender: TObject);
    procedure SBHelpClick(Sender: TObject);
    procedure BNLayerClick(Sender: TObject);
  private
    { Private declarations }
    fi:TThingFindInfo;
    LastFound:Boolean;
  public
    { Public declarations }
    Function Find:Boolean;
    Function FindNext(curOB:Integer):Boolean;
  end;

var
  FindThings: TFindThings;

implementation

uses ResourcePicker, FlagEditor, Jed_main;

{$R *.DFM}

procedure TFindThings.FormCreate(Sender: TObject);
begin

 ClientWidth:=SBHelp.Left+CBNUM.Top+SBHelp.Width;
 ClientHeight:=SBHelp.Top+CBNUM.Top+SBHelp.Height;


 fi:=TThingFindInfo.Create;
 TQueryField.CreateInt(CBNum,EBNum,fi.Num);
 TQueryField.CreateStr(CBName,EBName,fi.Name);
 TQueryField.CreateDouble(CBPCH,EBPCH,fi.PCH);
 TQueryField.CreateDouble(CBYAW,EBYAW,fi.YAW);
 TQueryField.CreateDouble(CBROL,EBROL,fi.ROL);
 TQueryField.CreateDouble(CBX,EBX,fi.X);
 TQueryField.CreateDouble(CBY,EBY,fi.Y);
 TQueryField.CreateDouble(CBZ,EBZ,fi.Z);
 TQueryField.CreateInt(CBSec,EBSec,fi.Sec);
 TQueryField.CreateStr(CBLayer,EBLayer,fi.Layer);
end;

procedure TFindThings.BNNameClick(Sender: TObject);
begin
 EBName.Text:=ResPicker.PickThing(EBName.Text);
end;

procedure TFindThings.BNLayerClick(Sender: TObject);
begin
 EBLayer.Text:=ResPicker.PickLayer(EBLayer.Text);
end;


Function TFindThings.Find;
var Lev:TJKLevel;
    nOb,o:Integer;
    Ms:TTHMultiSel;
begin
 Result:=false;
 Lev:=Level;
 {ms:=TMapWindow(MapWindow).OBMultiSel;}
 if ShowModal<>mrOK then exit;

 ms:=JedMain.thsel;

if RBAdd.Checked then
begin
 o:=FindNextThing(Lev,-1,fi);
 While o<>-1 do
 begin
  Result:=true;
  ms.AddTH(o);
  o:=FindNextThing(Lev,o,fi);
 end;
end;

if RBSubtract.Checked then
begin
 o:=FindNextThing(Lev,-1,fi);
 While o<>-1 do
 begin
  Result:=true;
  nOb:=ms.FindTH(o);
  if nOb<>-1 then ms.DeleteN(nOb);
  o:=FindNextThing(Lev,o,fi);
 end;
end;

if RBFocus.Checked then
begin
 o:=FindNextThing(Lev,-1,fi);
 if o<>-1 then
 begin
  JedMain.GotoTH(o);
  Result:=true;
 end;
end;

if not Result then ShowMessage('No hits!');

end;

Function TFindThings.FindNext(curOB:Integer):Boolean;
var o:Integer;
begin
 o:=FindNextThing(Level,CurOB,fi);
  if o<>-1 then
 begin
  JedMain.GotoTH(o);
  Result:=true;
 end;
Result:=o<>-1;
if not Result then ShowMessage('No more hits!');
end;


procedure TFindThings.SBHelpClick(Sender: TObject);
begin
 Application.helpfile:=basedir+'jedhelp.hlp';
 Application.HelpContext(450);
end;


end.
