unit Q_Sectors;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  StdCtrls, Q_Utils, misc_utils, J_Level, Buttons, U_multisel, GlobalVars;

type
  TFindSectors = class(TForm)
    Label1: TLabel;
    CBNum: TComboBox;
    EBNum: TEdit;
    Label3: TLabel;
    CBFlags: TComboBox;
    EBFlags: TEdit;
    Label7: TLabel;
    CBExtraL: TComboBox;
    EBExtraL: TEdit;
    GroupBox1: TGroupBox;
    RBAdd: TRadioButton;
    RBSubtract: TRadioButton;
    RBFocus: TRadioButton;
    BNFind: TButton;
    BNCancel: TButton;
    CBSelOnly: TCheckBox;
    BNFlags: TButton;
    Label2: TLabel;
    CBCMap: TComboBox;
    EBCMap: TEdit;
    BNCMap: TButton;
    Label8: TLabel;
    CBTint_R: TComboBox;
    EBTint_R: TEdit;
    Label9: TLabel;
    CBTint_G: TComboBox;
    EBTint_G: TEdit;
    Label10: TLabel;
    CBTint_b: TComboBox;
    EBTint_B: TEdit;
    CBSound: TComboBox;
    BNSound: TButton;
    EBSound: TEdit;
    Label15: TLabel;
    Label16: TLabel;
    CBVol: TComboBox;
    EBVol: TEdit;
    Label17: TLabel;
    CBLayer: TComboBox;
    EBLayer: TEdit;
    BNLayer: TButton;
    SBHelp: TSpeedButton;
    Label4: TLabel;
    CBNsurf: TComboBox;
    EBNSurf: TEdit;
    procedure FormCreate(Sender: TObject);
    procedure BNCMapClick(Sender: TObject);
    procedure BNSoundClick(Sender: TObject);
    procedure BNLayerClick(Sender: TObject);
    procedure BNFlagsClick(Sender: TObject);
    procedure SBHelpClick(Sender: TObject);
  private
    { Private declarations }
    fi:TSectorFindInfo;
  public
    { Public declarations }
    MapWindow:TForm;
    Function Find:Boolean;
    Function FindNext(curSC:integer):boolean;
  end;

var
  FindSectors: TFindSectors;

implementation

uses ResourcePicker, FlagEditor, Jed_Main;

{$R *.DFM}

procedure TFindSectors.FormCreate(Sender: TObject);
begin
 ClientWidth:=SBHelp.Left+CBNUM.Top+SBHelp.Width;
 ClientHeight:=SBHelp.Top+CBNUM.Top+SBHelp.Height;

 fi:=TSectorFindInfo.Create;
 TQueryField.CreateInt(CBNum,EBNum,fi.Num);
 TQueryField.CreateInt(CBNSurf,EBNSurf,fi.NSurfs);
 TQueryField.CreateFlags(CBFlags,EBFlags,fi.Flags);

 TQueryField.CreateDouble(CBExtraL,EBExtraL,fi.Extra_L);
 TQueryField.CreateStr(CBCmap,EBCmap,fi.ColorMap);

 TQueryField.CreateDouble(CBTint_R,EBTint_R,fi.tint_r);
 TQueryField.CreateDouble(CBTint_G,EBTint_G,fi.tint_g);
 TQueryField.CreateDouble(CBTint_B,EBTint_B,fi.tint_b);

 TQueryField.CreateStr(CBSound,EBSound,fi.Sound);
 TQueryField.CreateDouble(CBVol,EBVol,fi.sound_vol);

 TQueryField.CreateStr(CBLayer,EBLayer,fi.Layer);

end;

procedure TFindSectors.BNCMapClick(Sender: TObject);
begin
 EBCMap.Text:=ResPicker.PickCMP(EBCMap.Text);
end;

procedure TFindSectors.BNSoundClick(Sender: TObject);
begin
 EBSound.Text:=ResPicker.PickSecSound(EBSound.Text);
end;

procedure TFindSectors.BNLayerClick(Sender: TObject);
begin
 EBLayer.Text:=ResPicker.PickLayer(EBLayer.Text);
end;

procedure TFindSectors.BNFlagsClick(Sender: TObject);
var F:Longint;
begin
 ValDword(EBFlags.Text,f);
 f:=FlagEdit.EditSectorFlags(f);
 EBFlags.Text:=DwordToStr(f);
end;

Function TFindSectors.Find;
var Lev:TJKLevel;
    nSc,s:Integer;
    Ms:TSCMultiSel;
begin
 Result:=false;

 Lev:=Level;
 if ShowModal<>mrOK then exit;

 ms:=JedMain.scsel;

if RBAdd.Checked then
begin
 s:=FindNextSector(Lev,-1,fi);
 While s<>-1 do
 begin
  ms.AddSC(s);
  s:=FindNextSector(Lev,s,fi);
  Result:=true;
 end;
end;

if RBSubtract.Checked then
begin
 s:=FindNextSector(Lev,-1,fi);
 While s<>-1 do
 begin
  nSc:=ms.FindSC(s);
  if nSc<>-1 then ms.DeleteN(nSc);
  s:=FindNextSector(Lev,s,fi);
  Result:=true;
 end;
end;

if RBFocus.Checked then
begin
 s:=FindNextSector(Lev,-1,fi);
 if s<>-1 then
 begin JedMain.GotoSC(s); Result:=true; end;
end;

if not Result then ShowMessage('No hits!');
end;

Function TFindSectors.FindNext(curSC:Integer):Boolean;
var s:Integer;
begin
 s:=FindNextSector(Level,CurSC,fi);
  if s<>-1 then
 begin
  JedMain.GotoSC(s);
  Result:=true;
 end;
Result:=s<>-1;
if not Result then ShowMessage('No more hits!');
end;


procedure TFindSectors.SBHelpClick(Sender: TObject);
begin
 Application.helpfile:=basedir+'jedhelp.hlp';
 Application.HelpContext(450);
end;

end.
