unit U_Options;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  StdCtrls, FileCtrl, ComCtrls, misc_utils, ExtCtrls, Buttons, D3D_PRender,
  FieldEdit, J_Level, tbar_tools, GlobalVars, iniFiles, Vcl.Mask;

type
  TTextEdit = TEdit;
  TOptions = class(TForm)
    Pages: TPageControl;
    JK: TTabSheet;
    Label1: TLabel;
    Panel1: TPanel;
    BNOK: TButton;
    BNCancel: TButton;
    SBHelp: TSpeedButton;
    PPreview: TTabSheet;
    LBDevices: TListBox;
    RGWSize: TRadioGroup;
    MMDevDesc: TMemo;
    Label3: TLabel;
    Label4: TLabel;
    CBP3DOnTop: TCheckBox;
    CBFullLight: TCheckBox;
    LBGamma: TLabel;
    EBGamma: TEdit;
    PGEnv: TTabSheet;
    LBColors: TListBox;
    ColorDlg: TColorDialog;
    SColor: TShape;
    BNEditColor: TButton;
    CB3DLayers: TCheckBox;
    CBShowThings: TCheckBox;
    Label5: TLabel;
    Label6: TLabel;
    Label7: TLabel;
    Label8: TLabel;
    Label9: TLabel;
    CBColored: TCheckBox;
    RGAPI: TRadioGroup;
    Label11: TLabel;
    RGWireframe: TRadioGroup;
    RGMapRot: TRadioGroup;
    CBDbuf: TCheckBox;
    EBSaveInt: TEdit;
    CBAutoSave: TCheckBox;
    UDSaveInt: TUpDown;
    Label12: TLabel;
    Miscoptions: TTabSheet;
    ScrollBox1: TScrollBox;
    CBThingsOnFloor: TCheckBox;
    CBMoveFrames: TCheckBox;
    CBUndo: TCheckBox;
    CBGobSmart: TCheckBox;
    CBCheckOverlaps: TCheckBox;
    CBNewLightCalc: TCheckBox;
    CBConfRevert: TCheckBox;
    PToolbar: TTabSheet;
    JkDirEdit: TEdit;
    BtnBrowseJkDir: TButton;
    MotsDirEdit: TEdit;
    BtnBrowseMotsDir: TButton;
    GBJKDF2: TGroupBox;
    GBMOTS: TGroupBox;
    BtnBrowseIJIMDir: TButton;
    IJIMDirEdit: TEdit;
    Label13: TLabel;
    IJIMExeEdit: TEdit;
    Label14: TLabel;
    BtnBrowseIJIMExe: TButton;
    EBUndoStackSize: TEdit;
    UDUndoStackSize: TUpDown;
    Label15: TLabel;
    CBSaveEditorLights: TCheckBox;
    CBSaveEditorLayers: TCheckBox;
    GBIJIM: TGroupBox;
    cbInvrtCull: TCheckBox;
    ebVertSnapMaxDist: TLabeledEdit;
    ebGeoVertSize: TLabeledEdit;
    gbRenderOptions: TGroupBox;
    ebObjVertSize: TLabeledEdit;
    udGeoVertSize: TUpDown;
    udObjVertSize: TUpDown;
    gbMapEdit: TGroupBox;
    gbLockZoom: TGroupBox;
    ebZoomMin: TLabeledEdit;
    ebZoomMax: TLabeledEdit;
    cbLockZoom: TCheckBox;
    cbPlaySecSnd: TCheckBox;
    procedure BNOKClick(Sender: TObject);
    procedure FormCreate(Sender: TObject);
    procedure SBHelpClick(Sender: TObject);
    procedure LBDevicesClick(Sender: TObject);
    procedure LBColorsClick(Sender: TObject);
    procedure BNEditColorClick(Sender: TObject);
    procedure LBColorsDblClick(Sender: TObject);
    procedure CBAutoSaveClick(Sender: TObject);
    procedure BtnBrowseJkDirClick(Sender: TObject);
    procedure BtnBrowseMotsDirClick(Sender: TObject);
    procedure BtnBrowseIJIMDirClick(Sender: TObject);
    procedure BtnBrowseIJIMExeClick(Sender: TObject);
    procedure CBUndoClick(Sender: TObject);
    procedure cbLockZoomClick(Sender: TObject);

  private
    { Private declarations }
    ctls: TList;
    viGamma: TValInput;
    viVertSnapMaxDist: TValInput;
    viZoomMin: TValInput;
    viZoomMax: TValInput;

    Procedure InitControls;
    Procedure SetControls;
    Procedure GetControls;
    Procedure OnSetData;

  public
    Function SetOptions(curPage: TTabSheet): boolean;
    Function IsVarChanged(var v): boolean;
    { Public declarations }
  end;

Procedure ReadConfigFile;
Procedure WriteConfigFile(bExit: boolean);
Function SetProjectType(kind: TProjectType): boolean;

Function GetSetting(const settingName: string): variant;

const
  P3D_d3d5 = 0;
  P3D_ogl = 1;

  WF_Software = 0;
  WF_OpenGL = 1;

var
  Options: TOptions;
  SaveSettings: boolean = true;

implementation

uses Registry, FileOperations, Jed_Main, FileDialogs, U_Tools;
{$R *.DFM}

Type
  TDataType = (dt_int, dt_str, dt_bool, dt_double);
  TControlType = (ct_unkn, ct_DriveCB, ct_Button, ct_RGroup,
    ct_LBox, ct_cbox, ct_sbar, ct_vinput, ct_color, ct_edit);

type

  TRegValue = record
    d_type: TDataType;
    section: string;
    name: string;
    data: Pointer;
    onExit: boolean;
  end;

  TOptColor = class
    col: TJedColor;
  end;

  TCtrlData = class
    control: TObject;
    c_type: TControlType;
    d_type: TDataType;
    data: Pointer;
    changed: boolean;
  end;

const
  NoConfile: boolean = false;

  regValues: array [0 .. 93] of TRegValue = (
    (d_type: dt_str; Section: 'JKDF2'; Name: 'Dir'; data: @JKDir; onExit: false),
    (d_type: dt_str; Section: 'MOTS'; Name: 'Dir'; data: @MOTSDir; onExit: false),

    (d_type: dt_str; Section: 'IJIM'; Name: 'Dir'; data: @IJIMDir; onExit: false),
    (d_type: dt_str; Section: 'IJIM'; Name: 'Exe'; data: @IJIMExe; onExit: false),

    (d_type: dt_int; Section: '3D Preview'; Name: '3DAPI'; data: @P3DAPI; onExit: false),
    (d_type: dt_str; Section: '3D Preview'; Name: '3DDevice'; data: @D3DDevice; onExit: false),
    (d_type: dt_int; Section: '3D Preview'; Name: 'clBackground'; data: @clP3DBack.i; onExit: false),
    (d_type: dt_int; Section: '3D Preview'; Name: 'clSelection'; data: @clP3DSel.i; onExit: false),
    (d_type: dt_int; Section: '3D Preview'; Name: 'clMultiSelection'; data: @clP3DMultiSel.i; onExit: false),
    (d_type: dt_int; Section: '3D Preview'; Name: 'clSolidFloor'; data: @clP3DSolidFloor.i; onExit: false),
    (d_type: dt_int; Section: '3D Preview'; Name: 'clSolidSurface'; data: @clP3DSolidSurface.i; onExit: false),
    (d_type: dt_int; Section: '3D Preview'; Name: 'clP3DSpecialSurface'; data: @clP3DSpecialSurface.i; onExit: false),
    (d_type: dt_int; Section: '3D Preview'; Name: 'WinSize'; data: @P3DWinSize; onExit: true),
    (d_type: dt_bool; Section: '3D Preview'; Name: 'OnTop'; data: @P3DOnTop; onExit: true),
    (d_type: dt_bool; Section: '3D Preview'; Name: 'Colored'; data: @P3DCOloredLights; onExit: true),
    (d_type: dt_bool; Section: '3D Preview'; Name: 'PlaySecSound'; data: @P3DPlaySecSound; onExit: false),
    (d_type: dt_int; Section: '3D Preview'; Name: 'X'; data: @P3DX; onExit: true),
    (d_type: dt_int; Section: '3D Preview'; Name: 'Y'; data: @P3DY; onExit: true),
    (d_type: dt_double; Section: '3D Preview'; Name: 'Gamma'; data: @P3DGamma; onExit: true),
    (d_type: dt_bool; Section: '3D Preview'; Name: 'VisLayers'; data: @P3DVisLayers; onExit: false),
    (d_type: dt_bool; Section: '3D Preview'; Name: 'Things'; data: @P3DThings; onExit: false),

    (d_type: dt_int; Section: 'Editor'; Name: 'WFAPI'; data: @WireframeAPI; onExit: false),
    (d_type: dt_int; Section: 'Editor'; Name: 'WFInvertCulling'; data: @WF_InvertCulling; onExit: false),
    (d_type: dt_bool; Section: 'Editor'; Name: 'WFDBUF'; data: @WF_DoubleBuf; onExit: false),
    (d_type: dt_int; Section: 'Editor'; Name: 'WFGeoVertSize'; data: @WF_GeoVertSize; onExit: false),
    (d_type: dt_int; Section: 'Editor'; Name: 'WFObjVertSize'; data: @WF_ObjVertSize; onExit: false),

    (d_type: dt_bool; Section: 'Editor'; Name: 'UndoEnabled'; data: @UndoEnabled; onExit: false),
    (d_type: dt_int; Section: 'Editor'; Name: 'UndoSize'; data: @UndoStackSize; onExit: false),
    (d_type: dt_int; Section: 'Editor'; Name: 'MapRot'; data: @MapRot; onExit: false),
    (d_type: dt_double; Section: 'Editor'; Name: 'VertSnapMaxDist'; data: @VertSnapMaxDist; onExit: false),
    (d_type: dt_bool; Section: 'Editor'; Name: 'ZoomLocked'; data: @ZoomLocked; onExit: false),
    (d_type: dt_double; Section: 'Editor'; Name: 'ZoomMin'; data: @ZoomMin; onExit: false),
    (d_type: dt_double; Section: 'Editor'; Name: 'ZoomMax'; data: @ZoomMax; onExit: false),

    (d_type: dt_int; Section: 'Editor'; Name: 'SaveInterval'; data: @SaveInterval; onExit: false),
    (d_type: dt_bool; Section: 'Editor'; Name: 'AutoSave'; data: @AutoSave; onExit: false),

    (d_type: dt_bool; Section: 'Editor'; Name: 'NewOnFloor'; data: @NewOnFloor; onExit: false),
    (d_type: dt_bool; Section: 'Editor'; Name: 'MoveFrames'; data: @MoveFrames; onExit: false),
    (d_type: dt_bool; Section: 'Editor'; Name: 'GOBSmart'; data: @GOBSmart; onExit: false),
    (d_type: dt_bool; Section: 'Editor'; Name: 'CheckOverlaps'; data: @CheckOverlaps; onExit: false),
    (d_type: dt_bool; Section: 'Editor'; Name: 'NewLightCalc'; data: @NewLightCalc; onExit: false),
    (d_type: dt_bool; Section: 'Editor'; Name: 'ConfirmRevert'; data: @ConfirmRevert; onExit: false),

    (d_type: dt_bool; Section: 'Editor'; Name: 'SaveEditorLights'; data: @SaveEditorLights; onExit: false),
    (d_type: dt_bool; Section: 'Editor'; Name: 'SaveEditorLayers'; data: @SaveEditorLayers; onExit: false),

    (d_type: dt_double; Section: 'Editor'; Name: 'DefTxStep'; data: @DefTxStep; onExit: true),
    (d_type: dt_double; Section: 'Editor'; Name: 'DefTXRotStep'; data: @DefTXRotStep; onExit: true),
    (d_type: dt_double; Section: 'Editor'; Name: 'DefTXScaleStep'; data: @DefTXScaleStep; onExit: true),
    (d_type: dt_double; Section: 'Editor'; Name: 'DefPerpStep'; data: @DefPerpStep; onExit: true),
    (d_type: dt_double; Section: 'Editor'; Name: 'DefP3DStep'; data: @DefP3DStep; onExit: true),
    (d_type: dt_int; Section: 'Editor'; Name: 'DefThingView'; data: @DefThingView; onExit: true),
    (d_type: dt_int; Section: 'Editor'; Name: 'DefMselMode'; data: @DefMselMode; onExit: true),
    (d_type: dt_bool; Section: 'Editor'; Name: 'DefSnapToGrid'; data: @DefSnapToGrid; onExit: true),
    (d_type: dt_double; Section: 'Editor'; Name: 'DefGridStep'; data: @DefGridStep; onExit: true),
    (d_type: dt_double; Section: 'Editor'; Name: 'DefGridLine'; data: @DefGridLine; onExit: true),
    (d_type: dt_double; Section: 'Editor'; Name: 'DefGridDot'; data: @DefGridDot; onExit: true),
    (d_type: dt_double; Section: 'Editor'; Name: 'DefGridSize'; data: @DefGridSize; onExit: true),
    (d_type: dt_double; Section: 'Editor'; Name: 'GridMoveStep'; data: @GridMoveStep; onExit: true),
    (d_type: dt_str; Section: 'Editor'; Name: 'DefShape'; data: @DefShape; onExit: true),

    (d_type: dt_str; Section: 'Editor'; Name: 'Recent1'; data: @Recent1; onExit: true),
    (d_type: dt_str; Section: 'Editor'; Name: 'Recent2'; data: @Recent2; onExit: true),
    (d_type: dt_str; Section: 'Editor'; Name: 'Recent3'; data: @Recent3; onExit: true),
    (d_type: dt_str; Section: 'Editor'; Name: 'Recent4'; data: @Recent4; onExit: true),

    (d_type: dt_int; Section: 'Editor'; Name: 'X'; data: @MWinPos.X; onExit: true),
    (d_type: dt_int; Section: 'Editor'; Name: 'Y'; data: @MWinPos.Y; onExit: true),
    (d_type: dt_int; Section: 'Editor'; Name: 'W'; data: @MWinPos.W; onExit: true),
    (d_type: dt_int; Section: 'Editor'; Name: 'H'; data: @MWinPos.H; onExit: true),
    (d_type: dt_bool; Section: 'Editor'; Name: 'Maxed'; data: @MWMaxed; onExit: true),

    (d_type: dt_int; Section: 'Editor'; Name: 'clMapBack'; data: @clMapBack.i; onExit: false),
    (d_type: dt_int; Section: 'Editor'; Name: 'clMapGeo'; data: @clMapGeo.i; onExit: false),
    (d_type: dt_int; Section: 'Editor'; Name: 'clMapGeoBack'; data: @clMapGeoBack.i; onExit: false),
    (d_type: dt_int; Section: 'Editor'; Name: 'clMapSel'; data: @clMapSel.i; onExit: false),
    (d_type: dt_int; Section: 'Editor'; Name: 'clGrid'; data: @clGrid.i; onExit: false),
    (d_type: dt_int; Section: 'Editor'; Name: 'clVertex'; data: @clVertex.i; onExit: false),
    (d_type: dt_int; Section: 'Editor'; Name: 'clMapSelBack'; data: @clMapSelBack.i; onExit: false),
    (d_type: dt_int; Section: 'Editor'; Name: 'clThing'; data: @clThing.i; onExit: false),
    (d_type: dt_int; Section: 'Editor'; Name: 'clFrame'; data: @clFrame.i; onExit: false),
    (d_type: dt_int; Section: 'Editor'; Name: 'clLight'; data: @clLight.i; onExit: false),
    (d_type: dt_int; Section: 'Editor'; Name: 'clMsel'; data: @clMsel.i; onExit: false),
    (d_type: dt_int; Section: 'Editor'; Name: 'clMselBack'; data: @clMselBack.i; onExit: false),
    (d_type: dt_int; Section: 'Editor'; Name: 'clSelMsel'; data: @clSelMsel.i; onExit: false),
    (d_type: dt_int; Section: 'Editor'; Name: 'clSelMselBack'; data: @clSelMselBack.i; onExit: false),
    (d_type: dt_int; Section: 'Editor'; Name: 'clGridX'; data: @clGridX.i; onExit: false),
    (d_type: dt_int; Section: 'Editor'; Name: 'clGridY'; data: @clGridY.i; onExit: false),
    (d_type: dt_int; Section: 'Editor'; Name: 'clExtra'; data: @clExtra.i; onExit: false),
    (d_type: dt_int; Section: 'Editor'; Name: 'clCleaveLine'; data: @clCleaveLine.i; onExit: false),

    (d_type: dt_int; Section: 'Editor'; Name: 'IEdit.X'; data: @IEditPos.X; onExit: true),
    (d_type: dt_int; Section: 'Editor'; Name: 'IEdit.Y'; data: @IEditPos.Y; onExit: true),
    (d_type: dt_int; Section: 'Editor'; Name: 'IEdit.H'; data: @IEditPos.H; onExit: true),
    (d_type: dt_int; Section: 'Editor'; Name: 'IEdit.W'; data: @IEditPos.W; onExit: true),
    (d_type: dt_bool; Section: 'Editor'; Name: 'IEdit.OnTop'; data: @IEOnTop; onExit: true),

    (d_type: dt_int; Section: 'Editor'; Name: 'TBar.X'; data: @TbarPos.X; onExit: true),
    (d_type: dt_int; Section: 'Editor'; Name: 'Tbar.Y'; data: @TbarPos.Y; onExit: true),
    (d_type: dt_int; Section: 'Editor'; Name: 'Tbar.W'; data: @TbarPos.W; onExit: true),
    (d_type: dt_int; Section: 'Editor'; Name: 'Tbar.H'; data: @TbarPos.H; onExit: true),
    (d_type: dt_bool; Section: 'Editor'; Name: 'Tbar.OnTop'; data: @TbOnTop; onExit: true));

Function GetSetting(const settingName: string): variant;
var
  i: integer;
begin
  result := 0;
  for i := 0 to Sizeof(regValues) div Sizeof(TRegValue) - 1 do
    With regValues[i] do
    begin
      if CompareText(name, settingName) <> 0 then
        continue;
      case d_type of
        dt_int:
          result := Integer(data^);
        dt_str:
          result := string(data^);
        dt_bool:
          result := boolean(data^);
        dt_double:
          result := Double(data^);
      end;
      exit;
    end;
end;

Procedure ReadConfigFile;
begin
  const confile = GetConfigFilePath();
  if not FileExists(confile) then
    begin
      NoConfile := true;
      exit;
    end;

  const conf = TMemIniFile.Create(confile, TEncoding.UTF8, (*caseSensitive=*)false);
  try
    try
      FormatSettings.DecimalSeparator:='.';
      for var i := 0 to SizeOf(regValues) div SizeOf(TRegValue) - 1 do
        With regValues[i] do
        begin
          Try
            Case d_type of
              dt_int:
                integer(data^) := conf.ReadInteger(section, name, integer(data^));
              dt_str:
                String(data^) := conf.ReadString(section, name, String(data^));
              dt_bool:
                boolean(data^) := conf.ReadBool(section, name, boolean(data^));
              dt_double:
              begin
                var d := conf.ReadFloat(section, Name, 0.0);
                Double(data^) := conf.ReadFloat(Section, Name, Double(data^));
              end;
            end;
          except
            On e: Exception do
              PanMessageFmt(mt_error, 'Failed to read config entry section:%s name:%s : %s', [Section, Name, e.Message]);
          end;
        end;
    except
    on e: Exception do
      PanMessageFmt(mt_error, 'Failed to read config file: %s', [e.Message]);
    end;
  finally
    conf.Free;
    if D3DDevice = '' then
    D3DDevice := 'Direct3D HAL';
  end;

end;

Procedure WriteConfigFile(bExit: boolean);
  var confile: TMemIniFile;
begin
  try
    confile := TMemIniFile.Create(GetConfigFilePath(), TEncoding.UTF8, (*caseSensitive=*)false);
    try
      For var i := 0 to SizeOf(regValues) div SizeOf(TRegValue) - 1 do
        With regValues[i] do
          if bExit = onExit then
          begin
            Case d_type of
              dt_int:
                confile.WriteInteger(Section, name, integer(data^));
              dt_str:
                confile.WriteString(Section, name, String(data^));
              dt_bool:
                confile.WriteBool(Section, name, boolean(data^));
              dt_double:
                confile.WriteFloat(Section, name, Double(data^));
            end;
          end;
    except
      on e: Exception do
        PanMessageFmt(mt_error, 'An error has occored while trying to write config file: %s', [e.Message]);
    end;
  finally
    if confile <> nil then
      begin
        confile.UpdateFile;
        confile.Free;
      end;
  end;
end;

Function TOptions.SetOptions(curPage: TTabSheet): boolean;
var
  cdir: string;
begin
  if curPage <> nil then
    Pages.ActivePage := curPage;

  cdir := GetCurDir;
  SetControls;
  result := ShowModal = mrOK;
  if result then GetControls;

  SetCurDir(cdir);

  if result then
    WriteConfigFile(false);
end;

procedure TOptions.BNOKClick(Sender: TObject);
begin
  ModalResult := mrOK;
  Hide;
end;

procedure TOptions.BtnBrowseJkDirClick(Sender: TObject);
begin
  var dir: string := JkDirEdit.Text;
  var dirs:  TArray<string>;
  if FileCtrl.SelectDirectory(dir, dirs, [], 'Select JKDF2 Install Directory') then
    JkDirEdit.Text := dirs[0];
end;

procedure TOptions.BtnBrowseMotsDirClick(Sender: TObject);
begin
  var dir: string := MotsDirEdit.Text;
  var dirs:  TArray<string>;
  if FileCtrl.SelectDirectory(dir, dirs, [], 'Select MOTS Install Directory') then
    MotsDirEdit.Text := dirs[0];
end;

procedure TOptions.BtnBrowseIJIMDirClick(Sender: TObject);
begin
  var dir: string := IJIMDirEdit.Text;
  var dirs:  TArray<string>;
  if FileCtrl.SelectDirectory(dir, dirs, [], 'Select Infernal Machine Install Directory') then
    begin
      IJIMDirEdit.Text := dirs[0];
      if IJIMExeEdit.Text = '' then
        IJIMExeEdit.Text := IJIMDefaultExe;
    end;

end;

procedure TOptions.BtnBrowseIJIMExeClick(Sender: TObject);
begin
   With GetFileOpen do
    begin
        Filter := 'Indy3D.exe|*.exe';
        If Execute then
          IJIMExeEdit.Text := FileName;
    end;
end;

Procedure TOptions.InitControls;

  Procedure NewCtl(data: Pointer; d_type: TDataType; ctrl: TObject);
  var
    ctl: TCtrlData;
  begin
    ctl := TCtrlData.Create;
    ctl.data := data;
    ctl.d_type := d_type;
    ctl.control := ctrl;
    if ctrl.ClassType = TDriveComboBox then
      ctl.c_type := ct_DriveCB
    else if ctrl.ClassType = TButton then
      ctl.c_type := ct_Button
    else if ctrl.ClassType = TListBox then
      ctl.c_type := ct_LBox
    else if ctrl.ClassType = TRadioGroup then
      ctl.c_type := ct_RGroup
    else if ctrl.ClassType = TButton then
      ctl.c_type := ct_Button
    else if ctrl.ClassType = TCheckBox then
      ctl.c_type := ct_cbox
    else if ctrl.ClassType = TScrollBar then
      ctl.c_type := ct_sbar
    else if ctrl.ClassType = TValInput then
      ctl.c_type := ct_vinput
    else if (ctrl.ClassType = TEdit) or (ctrl.ClassType = TLabeledEdit) then
      ctl.c_type := ct_edit
    else
      ctl.c_type := ct_unkn;

    ctls.Add(ctl);
  end;

  Procedure AddColor(const name: string; var col: TJedColor);
  var
    ctl: TCtrlData;
  begin
    ctl := TCtrlData.Create;
    ctl.data := @col;
    ctl.c_type := ct_color;
    ctl.d_type := dt_int;
    ctl.control := TOptColor.Create;
    ctls.Add(ctl);
    LBColors.Items.AddObject(name, ctl.control);
  end;

begin
  viGamma := TValInput.Create(EBGamma);
  viVertSnapMaxDist := TValInput.Create(TEdit(ebVertSnapMaxDist));
  viZoomMin := TValInput.Create(TEdit(ebZoomMin));
  viZoomMax := TValInput.Create(TEdit(ebZoomMax));

  NewCtl(@JKDir, dt_str, JkDirEdit);
  NewCtl(@MOTSDir, dt_str, MotsDirEdit);
  NewCtl(@IJIMDir, dt_str, IJIMDirEdit);
  NewCtl(@IJIMExe, dt_str, IJIMExeEdit);
  NewCtl(@D3DDevice, dt_str, LBDevices);
  NewCtl(@P3DWinSize, dt_int, RGWSize);
  NewCtl(@P3DOnTop, dt_bool, CBP3DOnTop);
  NewCtl(@P3DCOloredLights, dt_bool, CBColored);
  NewCtl(@P3DPlaySecSound, dt_bool, cbPlaySecSnd);
  NewCtl(@P3DFullLit, dt_bool, CBFullLight);
  NewCtl(@P3DGamma, dt_double, viGamma);
  NewCtl(@P3DVisLayers, dt_bool, CB3DLayers);
  NewCtl(@P3DThings, dt_bool, CBShowThings);
  NewCtl(@P3DAPI, dt_int, RGAPI);
  NewCtl(@WireframeAPI, dt_int, RGWireframe);
  NewCtl(@WF_InvertCulling, dt_bool, cbInvrtCull);
  NewCtl(@WF_DoubleBuf, dt_bool, CBDbuf);

  NewCtl(@WF_GeoVertSize, dt_int, ebGeoVertSize);
  udGeoVertSize.Max := WF_VertMaxSize;
  udGeoVertSize.Min := WF_VertMinSize;
  udGeoVertSize.Position := WF_GeoVertSize;

  NewCtl(@WF_ObjVertSize, dt_int, ebObjVertSize);
  udObjVertSize.Max := WF_VertMaxSize;
  udObjVertSize.Min := WF_VertMinSize;
  udObjVertSize.Position := WF_ObjVertSize;

  NewCtl(@MapRot, dt_int, RGMapRot);
  NewCtl(@VertSnapMaxDist, dt_double, viVertSnapMaxDist);
  NewCtl(@ZoomLocked, dt_bool, cbLockZoom);
  NewCtl(@ZoomMin, dt_double, viZoomMin);
  NewCtl(@ZoomMax, dt_double, viZoomMax);

  NewCtl(@AutoSave, dt_bool, CBAutoSave);
  NewCtl(@SaveInterval, dt_int, EBSaveInt);
  UDSaveInt.Position := SaveInterval;

  NewCtl(@UndoEnabled, dt_bool, CBUndo);
  NewCtl(@UndoStackSize, dt_int, EBUndoStackSize);
  UDUndoStackSize.Max := UndoMaxStackSize;
  UDUndoStackSize.Position := UndoStackSize;

  NewCtl(@NewOnFloor, dt_bool, CBThingsOnFloor);
  NewCtl(@MoveFrames, dt_bool, CBMoveFrames);
  NewCtl(@GOBSmart, dt_bool, CBGobSmart);
  NewCtl(@CheckOverlaps, dt_bool, CBCheckOverlaps);
  NewCtl(@NewLightCalc, dt_bool, CBNewLightCalc);
  NewCtl(@ConfirmRevert, dt_bool, CBConfRevert);

  NewCtl(@SaveEditorLights, dt_bool, CBSaveEditorLights);
  NewCtl(@SaveEditorLayers, dt_bool, CBSaveEditorLayers);

  AddColor('3D preview background', clP3DBack);
  AddColor('3D preview selection', clP3DSel);
  AddColor('3D preview multi selection', clP3DMultiSel);
  AddColor('3D preview solid floor', clP3DSolidFloor);
  AddColor('3D preview solid surface', clP3DSolidSurface);
  AddColor('3D preview special surface', clP3DSpecialSurface);

  AddColor('Cleave Line', clCleaveLine);
  AddColor('Map background', clMapBack);
  AddColor('Geometry', clMapGeo);
  AddColor('Geometry - backside', clMapGeoBack);
  AddColor('Map selection', clMapSel);
  AddColor('Map selection - backside', clMapSelBack);
  AddColor('Grid', clGrid);
  AddColor('Vertices', clVertex);
  AddColor('Things', clThing);
  AddColor('Frames', clFrame);
  AddColor('Lights', clLight);
  AddColor('Multiselection', clMsel);
  AddColor('Multiselection - backside', clMselBack);
  AddColor('Multi&Selected', clSelMsel);
  AddColor('Multi&Selected - backside', clSelMselBack);
  AddColor('Grid X', clGridX);
  AddColor('Grid Y', clGridY);
  AddColor('Plug-in objects', clExtra);
end;

Procedure TOptions.SetControls;
var
  i, l: integer;
  s: string;
begin
  EnumDevices;
  LBDevices.Items.Clear;
  for i := 0 to _D3DdriverCount - 1 do
    With _D3Ddrivers[i] do
      LBDevices.Items.Add(DeviceName);
  LBDevices.ItemIndex := GetDeviceNum(D3DDevice);

  For i := 0 to ctls.Count - 1 do
    With TCtrlData(ctls[i]) do
    begin
      try
        changed := false;
        Case d_type of
          dt_bool:
            case c_type of
              ct_cbox:
                TCheckBox(control).Checked := boolean(data^);
            end;
          dt_int:
            case c_type of
              ct_edit:
                TEdit(control).Text := IntToStr(integer(data^));
              ct_LBox:
                TListBox(control).ItemIndex := integer(data^);
              ct_RGroup:
                TRadioGroup(control).ItemIndex := integer(data^);
              ct_color:
                TOptColor(control).col := TJedColor(data^);
            end;
          dt_double:
            case c_type of
              ct_sbar:
                with TScrollBar(control) do
                  position := Round(Double(data^) * max);
              ct_vinput:
                TValInput(control).SetAsFloat(Double(data^));
            end;
          dt_str:
            Case c_type of
              ct_edit:
                TEdit(control).Text := String(data^);
              ct_LBox:
                begin
                  l := TListBox(control).Items.IndexOf(String(data^));
                  TListBox(control).ItemIndex := l;
                end;
              ct_DriveCB:
                begin
                  s := String(data^);
                  if s <> '' then
                    TDriveComboBox(control).Drive := s[1];
                end;
              ct_Button:
                TButton(control).Caption := String(data^);
            end;
        end;
      except
        on exception do;
      end;
    end;

    // Select first item in the list
    LBColors.ItemIndex := 0;
    LBColorsClick(nil);
end;

Function TOptions.IsVarChanged(var v): boolean;
var
  i: integer;
begin
  result := false;
  For i := 0 to ctls.Count - 1 do
    With TCtrlData(ctls[i]) do
      if data = @v then
      begin
        result := changed;
        exit;
      end;
end;

Procedure TOptions.GetControls;
var
  i, l: integer;
begin
  For i := 0 to ctls.Count - 1 do
    With TCtrlData(ctls[i]) do
    begin
      Case d_type of
        dt_bool:
          case c_type of
            ct_cbox:
              begin
                changed := boolean(data^) <> TCheckBox(control).Checked;
                boolean(data^) := TCheckBox(control).Checked;
              end;
          end;
        dt_int:
          case c_type of
            ct_edit:
              begin
                changed := integer(data^) <> StrToInt(TEdit(control).Text);
                integer(data^) := StrToInt(TEdit(control).Text);
              end;
            ct_LBox:
              begin
                changed := integer(data^) <> TListBox(control).ItemIndex;
                integer(data^) := TListBox(control).ItemIndex;
              end;
            ct_RGroup:
              begin
                changed := integer(data^) <> TRadioGroup(control).ItemIndex;
                integer(data^) := TRadioGroup(control).ItemIndex;
              end;
            ct_color:
              TJedColor(data^) := TOptColor(control).col;
          end;
        dt_double:
          case c_type of
            ct_sbar:
              begin
                with TScrollBar(control) do
                begin
                  changed := Double(data^) <> position / max;
                  Double(data^) := position / max;
                end;
              end;
            ct_vinput:
              begin
                changed := Double(data^) <> TValInput(control).AsFloat;
                Double(data^) := TValInput(control).AsFloat;
              end;
          end;
        dt_str:
          Case c_type of
            ct_edit:
              begin
                changed := String(data^) <> TEdit(control).Text;
                String(data^) := TEdit(control).Text;
              end;
            ct_LBox:
              begin
                changed := false;
                l := TListBox(control).ItemIndex;
                if l <> -1 then
                begin
                  changed := String(data^) <> TListBox(control).Items[l];
                  String(data^) := TListBox(control).Items[l];
                end;
              end;
            ct_DriveCB:
              begin
                changed := String(data^) <> TDriveComboBox(control).Drive;
                String(data^) := TDriveComboBox(control).Drive;
              end;
            ct_Button:
              begin
                changed := String(data^) <> TButton(control).Caption;
                String(data^) := TButton(control).Caption;
              end;
          end;
      end;
    end;
  OnSetData;
end;

Procedure TOptions.OnSetData;
begin
  if JKDir <> '' then
    if JKDir[Length(JKDir)] <> '\' then
      JKDir := JKDir + '\';

  if MOTSDir <> '' then
    if MOTSDir[Length(MOTSDir)] <> '\' then
      MOTSDir := MOTSDir + '\';
    if JKDir = '' then
      CurrentProject := MOTS;

  if IJIMDir <> '' then
    if IJIMDir[Length(IJIMDir)] <> '\' then
      IJIMDir := IJIMDir + '\';
    if (JKDir = '') and (MOTSDir = '') then
      CurrentProject := IJIM;

  SetProjectType(CurrentProject);
  Jedmain.SaveTimer.Enabled := AutoSave;
  Jedmain.SaveTimer.Interval := SaveInterval * 60 * 1000;

  { LoadToolBar(JedMain.PtoolBar,'op'); }
end;

procedure TOptions.FormCreate(Sender: TObject);
var
  Reg: TRegistry;
begin
  ctls := TList.Create;
  InitControls;
  if NoConfile then
  begin
    try { Extract data from registry }
      Reg := TRegistry.Create;
      { HKEY_LOCAL_MACHINE }
      Reg.RootKey := HKEY_LOCAL_MACHINE;
      if Reg.OpenKey
        ('\SOFTWARE\LucasArts Entertainment Company\JediKnight\v1.0', false)
      then
      begin
        JKDir := Reg.ReadString('Install Path');
      end;
      if Reg.OpenKey
        ('\SOFTWARE\LucasArts Entertainment Company LLC\Mysteries of the Sith\v1.0',
        false) then
      begin
        MOTSDir := Reg.ReadString('Install Path');
      end;
      Reg.Free;
    except
      On exception do;
    end;

    try { Extract IMdata from registry }
      Reg := TRegistry.Create;
      { HKEY_LOCAL_MACHINE }
      Reg.RootKey := HKEY_LOCAL_MACHINE;
      if Reg.OpenKey
        ('\SOFTWARE\LucasArts Entertainment Company LLC\Indiana Jones and the Infernal Machine\v1.0', false)
      then
      begin
        IJIMDir := Reg.ReadString('Install Path');
        CurrentProject := IJIM;
      end;
      Reg.Free;
    except
      On exception do;
    end;

    if not SetOptions(JK) then
      begin
        SaveSettings := false;
        Application.Terminate;
      end
    else
      WriteConfigFile(false);
  end;

  CurrentProject := JKDF2;
  if MOTSDir <> '' then
       CurrentProject := MOTS;
  if IJIMDir <> '' then
       CurrentProject := IJIM;
  OnSetData;
end;

procedure TOptions.SBHelpClick(Sender: TObject);
begin
  //Application.helpfile := basedir + 'jedhelp.hlp';
  //Application.HelpContext(Pages.ActivePage.HelpContext);
end;

procedure TOptions.LBDevicesClick(Sender: TObject);
begin
  if LBDevices.ItemIndex < 0 then
  begin
    MMDevDesc.Lines.Text := '';
    exit;
  end;
  MMDevDesc.Lines.Text := _D3Ddrivers[LBDevices.ItemIndex].DeviceDescription;
end;

procedure TOptions.LBColorsClick(Sender: TObject);
var
  i: integer;
  cl: TOptColor;
begin
  i := LBColors.ItemIndex;
  if i < 0 then
    exit;
  cl := TOptColor(LBColors.Items.Objects[i]);
  SColor.Brush.Color := cl.col.col;
end;

procedure TOptions.BNEditColorClick(Sender: TObject);
var
  i: integer;
  cl: TOptColor;
begin
  i := LBColors.ItemIndex;
  if i < 0 then
    exit;
  cl := TOptColor(LBColors.Items.Objects[i]);
  ColorDlg.Color := cl.col.col;
  if not ColorDlg.Execute then
    exit;
  cl.col.col := ColorDlg.Color;
  LBColorsClick(nil);
end;

procedure TOptions.LBColorsDblClick(Sender: TObject);
begin
  BNEditColorClick(nil);
end;

Function FindGobJK(const name: string): String;
begin
  result := '';
  if FileExists(JKDir + 'Episode\' + name) then
    result := JKDir + 'Episode\' + name
  else if FileExists(JKDir + 'Resource\' + name) then
    result := JKDir + 'Resource\' + name
end;

Function FindGoo(const name: string): String;
begin
  result := '';
  if FileExists(MOTSDir + 'Episode\' + name) then
    result := MOTSDir + 'Episode\' + name
  else if FileExists(MOTSDir + 'Resource\' + name) then
    result := MOTSDir + 'Resource\' + name
end;

Function FindGobIJIM(const name: string): String;
begin
  result := '';
  if FileExists(IJIMDir + 'Resource\' + name) then
    result := IJIMDir + 'Resource\' + name
  else if FileExists(IJIMDir  + name) then
    result := IJIMDir + name
end;

Function SetProjectType(kind: TProjectType): boolean;
begin
  CurrentProject          := kind;
  Level.kind              := kind;
  Level.header.version    := 1;
  Level.ppunit            := DefaultPixelPerUnit;
  ToolForm.BNDoor.visible := true;

  case kind of
    JKDF2:
      begin
        GameDir  := JKDir;
        res1_gob := FindGobJK('Res1hi.gob');
        if res1_gob = '' then
          res1_gob := FindGobJK('Res1low.gob');
        if res1_gob = '' then
        begin
          PanMessage(mt_info,
            'Check JK directory setting - it appears to be wrong');
        end;
        Res2_gob := FindGobJK('Res2.gob');
        sp_gob   := FindGobJK('Jk1.gob');
        mp1_gob  := FindGobJK('Jk1mp.gob');
        mp2_gob  := FindGobJK('Jk1ctf.gob');
        mp3_gob  := '';
        result := (GameDir <> '') and (res1_gob <> '') and (Res2_gob <> '') and
          (mp1_gob <> '') and (mp2_gob <> '');
        if not result then
          PanMessage(mt_warning,
            'Some resources weren''t found! Check ''Dark Forces II'' settings in Options');
      end;
    MOTS:
      begin
        GameDir  := MOTSDir;
        res1_gob := FindGoo('JKMsndLO.goo');
        Res2_gob := FindGoo('Jkmres.goo');
        sp_gob   := FindGoo('Jkm.goo');
        mp1_gob  := FindGoo('Jkm_mp.goo');
        mp2_gob  := FindGoo('Jkm_kfy.goo');
        mp3_gob  := FindGoo('JKM_SABER.GOO');
        result   := (GameDir <> '') and (res1_gob <> '') and (Res2_gob <> '') and
          (mp1_gob <> '') and (mp2_gob <> '') and (mp3_gob <> '');
        if not result then
          PanMessage(mt_warning,
            'Some resources weren''t found! Check ''Mysteries of the Sith'' settings in Options');
      end
    else // IJIM
      begin
        Level.header.version := 3;
        ToolForm.BNDoor.visible := false;
        GameDir := IJIMDir;
        res1_gob := FindGobIJIM('CD1.GOB');
        if res1_gob = '' then
          begin
            PanMessage(mt_info,
              'Infernal Machine install directory is missing CD1.GOB');
          end;
        Res2_gob := FindGobIJIM('CD2.GOB');
        sp_gob   := '';
        mp1_gob  := '';
        mp2_gob  := '';
        mp3_gob  := '';
        result := (GameDir <> '') and (res1_gob <> '') and (Res2_gob <> '');
        if not result then
          PanMessage(mt_warning,
            'Some resources weren''t found! Check ''Indiana Jones and the Infernal Machine'' settings in Options');
      end
  end;
end;

procedure TOptions.CBAutoSaveClick(Sender: TObject);
begin
  EBSaveInt.Enabled := CBAutoSave.Checked;
end;

procedure TOptions.cbLockZoomClick(Sender: TObject);
begin
  ebZoomMin.Enabled := cbLockZoom.Checked;
  ebZoomMax.Enabled := cbLockZoom.Checked;
end;

procedure TOptions.CBUndoClick(Sender: TObject);
begin
  EBUndoStackSize.Enabled := CBUndo.Checked;
  UDUndoStackSize.Enabled := CBUndo.Checked;
end;

Initialization

ReadConfigFile;

Finalization

if SaveSettings then
  WriteConfigFile(true);
end.
