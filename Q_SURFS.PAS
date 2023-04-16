unit Q_Surfs;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  StdCtrls, Q_Utils, Misc_Utils, J_Level, Buttons, U_multisel, GlobalVars;

type
  TFindSurfs = class(TForm)
    Label1: TLabel;
    CBNum: TComboBox;
    EBNum: TEdit;
    Label2: TLabel;
    CBMat: TComboBox;
    EBMat: TEdit;
    Label3: TLabel;
    CBAdjFlags: TComboBox;
    EBAdjFlags: TEdit;
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
    BNMat: TButton;
    BNAdjFlags: TButton;
    SBHelp: TSpeedButton;
    Label8: TLabel;
    CBAdjSC: TComboBox;
    EBAdjSC: TEdit;
    Label9: TLabel;
    CBAdjSF: TComboBox;
    EBAdjSF: TEdit;
    Label11: TLabel;
    CBGeo: TComboBox;
    EBGeo: TEdit;
    Label12: TLabel;
    CBSurfFlags: TComboBox;
    BNSurfFlags: TButton;
    Label10: TLabel;
    CBFaceFlags: TComboBox;
    BNFaceFlags: TButton;
    EBFaceFlags: TEdit;
    BNGeo: TButton;
    Label6: TLabel;
    CBLight: TComboBox;
    EBLight: TEdit;
    BNLight: TButton;
    Label13: TLabel;
    CBTex: TComboBox;
    EBTex: TEdit;
    BNTex: TButton;
    EBSurfFlags: TEdit;
    CBLayer: TComboBox;
    EBLayer: TEdit;
    BNLayer: TButton;
    Label4: TLabel;
    procedure FormCreate(Sender: TObject);
    procedure BNMatClick(Sender: TObject);
    procedure BNAdjFlagsClick(Sender: TObject);
    procedure SBHelpClick(Sender: TObject);
    procedure BNSurfFlagsClick(Sender: TObject);
    procedure BNFaceFlagsClick(Sender: TObject);
    procedure BNGeoClick(Sender: TObject);
    procedure BNLightClick(Sender: TObject);
    procedure BNTexClick(Sender: TObject);
    procedure BNLayerClick(Sender: TObject);
  private
    fi: TSurfFindInfo;
    { Private declarations }
  public
    { Public declarations }
    MapWindow: TForm;
    Function Find: Boolean;
    Function FindNext(curSC, curWL: integer): Boolean;
  end;

var
  FindSurfs: TFindSurfs;

implementation

uses ResourcePicker, FlagEditor, Jed_Main, U_SCFEdit;

{$R *.DFM}

procedure TFindSurfs.FormCreate(Sender: TObject);
begin

  ClientWidth := SBHelp.Left + CBNum.Top + SBHelp.Width;
  ClientHeight := SBHelp.Top + CBNum.Top + SBHelp.Height;

  fi := TSurfFindInfo.Create;
  TQueryField.CreateInt(CBNum, EBNum, fi.Num);
  TQueryField.CreateStr(CBMat, EBMat, fi.Material);

  TQueryField.CreateInt(CBAdjSC, EBAdjSC, fi.AdjoinSC);
  TQueryField.CreateInt(CBAdjSF, EBAdjSF, fi.AdjoinSF);

  TQueryField.CreateFlags(CBAdjFlags, EBAdjFlags, fi.AdjoinFlags);
  TQueryField.CreateFlags(CBSurfFlags, EBSurfFlags, fi.SurfFlags);
  TQueryField.CreateFlags(CBFaceFlags, EBFaceFlags, fi.FaceFlags);

  TQueryField.CreateInt(CBGeo, EBGeo, fi.Geo);
  TQueryField.CreateInt(CBLight, EBLight, fi.Light);
  TQueryField.CreateInt(CBTex, EBTex, fi.tex);

  TQueryField.CreateColor(CBExtraL, EBExtraL, fi.extra_l);

  TQueryField.CreateStr(CBLayer, EBLayer, fi.Layer);
end;

procedure TFindSurfs.BNMatClick(Sender: TObject);
begin
  EBMat.Text := ResPicker.PickMAT(EBMat.Text);
end;

procedure TFindSurfs.BNAdjFlagsClick(Sender: TObject);
var
  F: Longint;
begin
  ValDword(EBAdjFlags.Text, F);
  F := FlagEdit.EditAdjoinFlags(F);
  EBAdjFlags.Text := DwordToStr(F);
end;

procedure TFindSurfs.BNSurfFlagsClick(Sender: TObject);
var
  F: Longint;
begin
  ValDword(EBSurfFlags.Text, F);
  F := FlagEdit.EditSurfaceFlags(F);
  EBSurfFlags.Text := DwordToStr(F);
end;

procedure TFindSurfs.BNFaceFlagsClick(Sender: TObject);
var
  F: Longint;
begin
  ValDword(EBFaceFlags.Text, F);
  F := FlagEdit.EditFaceFlags(F);
  EBFaceFlags.Text := DwordToStr(F);
end;

procedure TFindSurfs.BNGeoClick(Sender: TObject);
var
  F: Longint;
begin
  ValDword(EBGeo.Text, F);
  F := SCFieldPicker.PickGeo(F);
  EBGeo.Text := DwordToStr(F);
end;

procedure TFindSurfs.BNLightClick(Sender: TObject);
var
  F: Longint;
begin
  ValDword(EBLight.Text, F);
  F := SCFieldPicker.PickLightMode(F);
  EBLight.Text := DwordToStr(F);
end;

procedure TFindSurfs.BNTexClick(Sender: TObject);
var
  F: Longint;
begin
  ValDword(EBTex.Text, F);
  F := SCFieldPicker.PickTex(F);
  EBTex.Text := DwordToStr(F);
end;

Function TFindSurfs.Find;
var
  Lev: TJKLevel;
  nWl, s, w: integer;
  Ms: TSFMultiSel;
  OldCursor: HCursor;
begin
  Result := false;
  Lev := Level;
  if ShowModal <> mrOK then
    exit;

  OldCursor := SetCursor(LoadCursor(0, IDC_WAIT));
  Ms := JedMain.sfsel;

  if RBAdd.Checked then
  begin
    w := FindNextSurf(Lev, 0, -1, fi, s);
    While w <> -1 do
    begin
      Result := true;
      Ms.AddSF(s, w);
      w := FindNextSurf(Lev, s, w, fi, s);
    end;
  end;

  if RBSubtract.Checked then
  begin
    w := FindNextSurf(Lev, 0, -1, fi, s);
    While w <> -1 do
    begin
      Result := true;
      nWl := Ms.FindSF(s, w);
      if nWl <> -1 then
        Ms.DeleteN(nWl);
      w := FindNextSurf(Lev, s, w, fi, s);
    end;
  end;

  if RBFocus.Checked then
  begin
    w := FindNextSurf(Lev, 0, -1, fi, s);
    if w <> -1 then
    begin
      JedMain.GotoSF(s, w);
      Result := true;
    end;
  end;

  SetCursor(OldCursor);

  if not Result then
    ShowMessage('No hits!');

end;

Function TFindSurfs.FindNext(curSC, curWL: integer): Boolean;
var
  s, w: integer;
begin
  w := FindNextSurf(Level, curSC, curWL, fi, s);
  if w <> -1 then
  begin
    JedMain.GotoSF(s, w);
    Result := true;
  end;
  Result := w <> -1;
  if not Result then
    ShowMessage('No more hits!');
end;

procedure TFindSurfs.SBHelpClick(Sender: TObject);
begin
  //Application.helpfile := basedir + 'jedhelp.hlp';
  //Application.HelpContext(450);
end;

procedure TFindSurfs.BNLayerClick(Sender: TObject);
begin
  EBLayer.Text := ResPicker.PickLayer(EBLayer.Text);
end;

end.
