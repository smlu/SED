unit U_Options;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  StdCtrls, FileCtrl, ComCtrls, misc_utils, ExtCtrls, Buttons, D3D_PRender,
  FieldEdit, J_Level, tbar_tools;

type
  TOptions = class(TForm)
    Pages: TPageControl;
    JK: TTabSheet;
    Label2: TLabel;
    CBCD: TDriveComboBox;
    DirList: TDirectoryListBox;
    Label1: TLabel;
    OLDrive: TDriveComboBox;
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
    Label10: TLabel;
    Motsdirs: TDirectoryListBox;
    CBMOTS: TDriveComboBox;
    CBMOTSCD: TDriveComboBox;
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
    procedure BNOKClick(Sender: TObject);
    procedure FormCreate(Sender: TObject);
    procedure SBHelpClick(Sender: TObject);
    procedure LBDevicesClick(Sender: TObject);
    procedure LBColorsClick(Sender: TObject);
    procedure BNEditColorClick(Sender: TObject);
    procedure LBColorsDblClick(Sender: TObject);
    procedure CBAutoSaveClick(Sender: TObject);
  private
    { Private declarations }
    ctls: TList;
    vGamma: TValInput;
    Procedure InitControls;
    Procedure SetControls;
    Procedure GetControls;
    Procedure OnSetData;
  public
    Function SetOptions(curPage: TTabSheet): boolean;
    Function IsVarChanged(var v): boolean;
    Function CheckForJKCD: boolean;
    { Public declarations }
  end;

Procedure ReadRegistry;
Procedure WriteRegistry(IsExit: boolean);
Function SetMOTS(isit: boolean): boolean;

Function GetSetting(const name: string): variant;

const
  P3D_d3d = 0;
  P3D_ogl = 1;
  P3D_d3d5 = 2;

  WF_Software = 0;
  WF_OpenGL = 1;

var
  Options: TOptions;
  SaveSettings: boolean = true;

implementation

uses GlobalVars, Registry, FileOperations, Jed_Main;
{$R *.DFM}

Type
  TDataType = (dt_int, dt_str, dt_bool, dt_double);
  TControlType = (ct_unkn, ct_DirList, ct_DriveCB, ct_Button, ct_RGroup,
    ct_LBox, ct_cbox, ct_sbar, ct_vinput, ct_color, ct_edit);

type

  TRegValue = record
    d_type: TDataType;
    RegName: string;
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
  NoKey: boolean = false;

  RegValues: array [0 .. 74] of TRegValue = ((d_type: dt_str; RegName: 'JKDir';
    data: @JKDir; onExit: false), (d_type: dt_str; RegName: 'CDDir';
    data: @JKCDDir; onExit: false), (d_type: dt_str; RegName: 'MOTSDir';
    data: @MOTSDir; onExit: false), (d_type: dt_str; RegName: 'MOTSCDDir';
    data: @MOTSCDDir; onExit: false), (d_type: dt_int; RegName: 'IEdit.X';
    data: @IEditPos.X; onExit: true), (d_type: dt_int; RegName: 'IEdit.Y';
    data: @IEditPos.Y; onExit: true), (d_type: dt_int; RegName: 'IEdit.H';
    data: @IEditPos.H; onExit: true), (d_type: dt_int; RegName: 'IEdit.W';
    data: @IEditPos.W; onExit: true), (d_type: dt_int; RegName: 'MWin.X';
    data: @MWinPos.X; onExit: true), (d_type: dt_int; RegName: 'MWin.Y';
    data: @MWinPos.Y; onExit: true), (d_type: dt_int; RegName: 'MWin.W';
    data: @MWinPos.W; onExit: true), (d_type: dt_int; RegName: 'MWin.H';
    data: @MWinPos.H; onExit: true), (d_type: dt_bool; RegName: 'MWMaxed';
    data: @MWMaxed; onExit: true),

    (d_type: dt_int; RegName: 'TBar.X'; data: @TbarPos.X; onExit: true),
    (d_type: dt_int; RegName: 'Tbar.Y'; data: @TbarPos.Y; onExit: true),
    (d_type: dt_int; RegName: 'Tbar.W'; data: @TbarPos.W; onExit: true),
    (d_type: dt_int; RegName: 'Tbar.H'; data: @TbarPos.H; onExit: true),
    (d_type: dt_bool; RegName: 'TbOnTop'; data: @TbOnTop; onExit: true),
    (d_type: dt_str; RegName: 'D3DDevice'; data: @D3DDevice; onExit: false),
    (d_type: dt_int; RegName: 'P3DWinSize'; data: @P3DWinSize; onExit: true),
    (d_type: dt_bool; RegName: 'P3DOnTop'; data: @P3DOnTop; onExit: true),
    (d_type: dt_bool; RegName: 'P3DColored'; data: @P3DCOloredLights;
    onExit: true), (d_type: dt_bool; RegName: 'IEOnTop'; data: @IEOnTop;
    onExit: true), (d_type: dt_int; RegName: 'P3DX'; data: @P3DX; onExit: true),
    (d_type: dt_int; RegName: 'P3DY'; data: @P3DY; onExit: true),
    (d_type: dt_double; RegName: 'P3Gamma'; data: @P3DGamma; onExit: true),
    (d_type: dt_str; RegName: 'Recent1'; data: @Recent1; onExit: true),
    (d_type: dt_str; RegName: 'Recent2'; data: @Recent2; onExit: true),
    (d_type: dt_str; RegName: 'Recent3'; data: @Recent3; onExit: true),
    (d_type: dt_str; RegName: 'Recent4'; data: @Recent4; onExit: true),
    (d_type: dt_int; RegName: 'clMapBack'; data: @clMapBack.i; onExit: false),
    (d_type: dt_int; RegName: 'clMapGeo'; data: @clMapGeo.i; onExit: false),
    (d_type: dt_int; RegName: 'clMapGeoBack'; data: @clMapGeoBack.i;
    onExit: false), (d_type: dt_int; RegName: 'clMapSel'; data: @clMapSel.i;
    onExit: false), (d_type: dt_int; RegName: 'clGrid'; data: @clGrid.i;
    onExit: false), (d_type: dt_int; RegName: 'clVertex'; data: @clVertex.i;
    onExit: false), (d_type: dt_int; RegName: 'clMapSelBack';
    data: @clMapSelBack.i; onExit: false), (d_type: dt_int; RegName: 'clThing';
    data: @clThing.i; onExit: false), (d_type: dt_int; RegName: 'clFrame';
    data: @clFrame.i; onExit: false), (d_type: dt_int; RegName: 'clLight';
    data: @clLight.i; onExit: false), (d_type: dt_int; RegName: 'clMsel';
    data: @clMsel.i; onExit: false), (d_type: dt_int; RegName: 'clMselBack';
    data: @clMselBack.i; onExit: false), (d_type: dt_int; RegName: 'clSelMsel';
    data: @clSelMsel.i; onExit: false), (d_type: dt_int;
    RegName: 'clSelMselBack'; data: @clSelMselBack.i; onExit: false),
    (d_type: dt_int; RegName: 'clGridX'; data: @clGridX.i; onExit: false),
    (d_type: dt_int; RegName: 'clGridY'; data: @clGridY.i; onExit: false),
    (d_type: dt_int; RegName: 'clExtra'; data: @clExtra.i; onExit: false),

    (d_type: dt_bool; RegName: 'P3DVisLayers'; data: @P3DVisLayers;
    onExit: false), (d_type: dt_bool; RegName: 'P3DThings'; data: @P3DThings;
    onExit: false), (d_type: dt_int; RegName: 'P3DAPI'; data: @P3DAPI;
    onExit: false), (d_type: dt_int; RegName: 'WFAPI'; data: @WireframeAPI;
    onExit: false), (d_type: dt_bool; RegName: 'WFDBUF'; data: @WF_DoubleBuf;
    onExit: false), (d_type: dt_int; RegName: 'MapRot'; data: @MapRot;
    onExit: false),

    (d_type: dt_double; RegName: 'DefTxStep'; data: @DefTxStep; onExit: true),
    (d_type: dt_double; RegName: 'DefTXRotStep'; data: @DefTXRotStep;
    onExit: true), (d_type: dt_double; RegName: 'DefTXScaleStep';
    data: @DefTXScaleStep; onExit: true), (d_type: dt_double;
    RegName: 'DefPerpStep'; data: @DefPerpStep; onExit: true),
    (d_type: dt_double; RegName: 'DefP3DStep'; data: @DefP3DStep; onExit: true),
    (d_type: dt_int; RegName: 'DefThingView'; data: @DefThingView;
    onExit: true), (d_type: dt_int; RegName: 'DefMselMode'; data: @DefMselMode;
    onExit: true),

    (d_type: dt_double; RegName: 'DefGridStep'; data: @DefGridStep;
    onExit: true), (d_type: dt_double; RegName: 'DefGridLine';
    data: @DefGridLine; onExit: true), (d_type: dt_double;
    RegName: 'DefGridDot'; data: @DefGridDot; onExit: true), (d_type: dt_double;
    RegName: 'DefGridSize'; data: @DefGridSize; onExit: true),
    (d_type: dt_double; RegName: 'GridMoveStep'; data: @GridMoveStep;
    onExit: true),

    (d_type: dt_str; RegName: 'DefShape'; data: @DefShape; onExit: true),
    (d_type: dt_int; RegName: 'SaveInterval'; data: @SaveInterval;
    onExit: false), (d_type: dt_bool; RegName: 'AutoSave'; data: @AutoSave;
    onExit: false), (d_type: dt_bool; RegName: 'NewOnFloor'; data: @NewOnFloor;
    onExit: false),

    (d_type: dt_bool; RegName: 'UndoEnabled'; data: @UndoEnabled;
    onExit: false), (d_type: dt_bool; RegName: 'MoveFrames'; data: @MoveFrames;
    onExit: false), (d_type: dt_bool; RegName: 'GOBSmart'; data: @GOBSmart;
    onExit: false), (d_type: dt_bool; RegName: 'CheckOverlaps';
    data: @CheckOverlaps; onExit: false), (d_type: dt_bool;
    RegName: 'NewLightCalc'; data: @NewLightCalc; onExit: false),
    (d_type: dt_bool; RegName: 'ConfirmRevert'; data: @ConfirmRevert;
    onExit: false));

Function GetSetting(const name: string): variant;
var
  i: integer;
begin
  result := 0;
  for i := 0 to Sizeof(RegValues) div Sizeof(TRegValue) - 1 do
    With RegValues[i] do
    begin
      if CompareText(name, RegName) <> 0 then
        continue;
      case d_type of
        dt_int:
          result := integer(data^);
        dt_str:
          result := String(data^);
        dt_bool:
          result := boolean(data^);
        dt_double:
          result := Double(data^);
      end;
      exit;
    end;
end;

Procedure ReadRegistry;
var
  Reg: TRegistry;
  i: integer;
begin
  Reg := TRegistry.Create;
  if not Reg.OpenKey(RegBase, false) then
  begin
    Reg.Free;
    NoKey := true;
    exit;
  end;
  for i := 0 to Sizeof(RegValues) div Sizeof(TRegValue) - 1 do
    With RegValues[i] do
    begin
      Try
        Case d_type of
          dt_int:
            integer(data^) := Reg.ReadInteger(RegName);
          dt_str:
            String(data^) := Reg.ReadString(RegName);
          dt_bool:
            boolean(data^) := Reg.ReadBool(RegName);
          dt_double:
            Double(data^) := Reg.ReadFloat(RegName);
        end;
      except
        On ERegistryException do;
      end;
    end;
  Reg.Free;
  if D3DDevice = '' then
    D3DDevice := 'RGB Emulation';
end;

Procedure WriteRegistry(IsExit: boolean);
var
  Reg: TRegistry;
  i: integer;
begin
  Reg := TRegistry.Create;
  Reg.OpenKey(RegBase, true);
  For i := 0 to Sizeof(RegValues) div Sizeof(TRegValue) - 1 do
    With RegValues[i] do
      if IsExit = onExit then
      begin
        Case d_type of
          dt_int:
            Reg.WriteInteger(RegName, integer(data^));
          dt_str:
            Reg.WriteString(RegName, String(data^));
          dt_bool:
            Reg.WriteBool(RegName, boolean(data^));
          dt_double:
            Reg.WriteFloat(RegName, Double(data^));
        end;
      end;
  Reg.Free;
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
  GetControls;

  SetCurDir(cdir);

  if result then
    WriteRegistry(false);
end;

procedure TOptions.BNOKClick(Sender: TObject);
begin
  ModalResult := mrOK;
  Hide;
end;

(* Procedure WriteSettings;
  var reg:TRegistry;s:String;i:Integer;

  procedure WriteWinPos(const PosName:String;var wpos:TWinPos);
  begin
  Reg.WriteInteger(PosName+'.left',wpos.WLeft);
  Reg.WriteInteger(PosName+'.top',wpos.Wtop);
  Reg.WriteInteger(PosName+'.width',wpos.Wwidth);
  Reg.WriteInteger(PosName+'.height',wpos.Wheight);
  end;

  Procedure WriteFont(const Name:String;const f:TFontAttrib);
  begin
  Reg.WriteString(Name+'.Name',F.Name);
  Reg.WriteInteger(Name+'.Size',F.Size);
  Reg.WriteInteger(Name+'.Style',f.Style);
  { Reg.WriteInteger(Name+'.Color',F.Color);}
  end;


  begin
  Reg:=TRegistry.Create;
  Reg.OpenKey(regbase,true);
  Reg.WriteString('GameDir',gameDir);
  Reg.WriteString('CD Dir',CDDir);
  WriteWinPos('MapperPos',MapperPos);
  WriteWinPos('SectorEditPos',SectorEditPos);
  WriteWinPos('WallEditPos',WallEditPos);
  WriteWinPos('VertexEditPos',VertexEditPos);
  WriteWinPos('ObjectEditPos',ObjectEditPos);
  WriteWinPos('ToolsPos',ToolsPos);
  Reg.WriteBool('SectorEdit.Ontop',SCEditOnTop);
  Reg.WriteBool('WallEdit.Ontop',WLEditOnTop);
  Reg.WriteBool('VertexEdit.Ontop',VXEditOnTop);
  Reg.WriteBool('ObjectEdit.Ontop',OBEditOnTop);
  Reg.WriteBool('Tools.Ontop',ToolsOnTop);
  Reg.WriteBool('Script.OnTop',ScriptOnTop);
  Reg.WriteBool('LogTestSession',LogTestSession);
  Reg.WriteInteger('cc_FixErrors',cc_FixErrors);
  Reg.WriteInteger('lev_textures',lev_textures);

  For i:=1 to sizeof(Confirms) div sizeof(TConfirmRec) do
  With Confirms[i] do Reg.WriteBool(Rkey,Value);

  For i:=0 to Recents.Count-1 do
  Reg.WriteString('Recent'+IntToStr(i),Recents[i]);
  WriteFont('ScriptFont',scFont);
  Reg.Free;
  end;

  Function TOptions.ReadSettings:Boolean;
  var reg:TRegistry;s:String;i:Integer;

  Procedure ReadStr(const Name:String;var s:String);
  begin
  Try
  s:=Reg.ReadString(Name);
  Except
  on ERegistryException do;
  end;
  end;

  Procedure ReadBool(const Name:String;var b:Boolean);
  begin
  Try
  b:=Reg.ReadBool(Name);
  Except
  on ERegistryException do;
  end;
  end;

  Procedure ReadInt(const Name:String;var i:Integer);
  begin
  Try
  i:=Reg.ReadInteger(Name);
  Except
  on ERegistryException do;
  end;
  end;

  Procedure ReadFont(const Name:String;var f:TFontAttrib);
  begin
  Try
  F.Name:=Reg.ReadString(Name+'.Name');
  f.Size:=Reg.ReadInteger(Name+'.Size');
  f.Style:=Reg.ReadInteger(Name+'.style');
  Except
  On ERegistryException do;
  end;
  end;

  procedure ReadWinPos(const PosName:String;var pos:TWinPos);
  begin
  ReadInt(PosName+'.left',Pos.WLeft);
  ReadInt(PosName+'.top',Pos.WTop);
  ReadInt(PosName+'.width',Pos.WWidth);
  ReadInt(PosName+'.height',Pos.WHeight);
  end;

  Function GetGameLab(const Name:String):String;
  begin
  if FileExists(GameDir+Name) then Result:=GameDir+Name
  else Result:=CDDir+Name;
  end;

  begin
  Result:=true;
  Reg:=TRegistry.Create;

  if not Reg.OpenKey(regbase,false) then
  begin
  Reg.Free;
  Result:=false;
  exit;
  end;

  GameDir:=Reg.ReadString('GameDir');
  DirList.Directory:=GameDir;
  CDDir:=Reg.ReadString('CD Dir');
  s:=ExtractFileDrive(CDDir);
  if s<>'' then CBCD.Drive:=s[1];
  Outlaws_lab:=GetGameLab('outlaws.lab');
  textures_lab:=GetGameLab('oltex.lab');
  Weapons_lab:=GetGameLab('olweap.lab');
  Objects_lab:=GetGameLab('olobj.lab');
  sounds_lab:=GetGameLab('olsfx.lab');
  geo_lab:=GetGameLab('olgeo.lab');
  taunts_lab:=GetGameLab('oltaunt.lab');
  olpatch1_lab:=GetGameLab('olpatch1.lab');
  olpatch2_lab:=GetGameLab('olpatch2.lab');

  ReadInt('cc_FixErrors',cc_FixErrors);
  ReadInt('lev_textures',lev_textures);
  FixErrors.ItemIndex:=cc_FixErrors;
  LEVTextures.ItemIndex:=lev_textures;

  ReadWinPos('MapperPos',MapperPos);
  ReadWinPos('SectorEditPos',SectorEditPos);
  ReadWinPos('WallEditPos',WallEditPos);
  ReadWinPos('VertexEditPos',VertexEditPos);
  ReadWinPos('ObjectEditPos',ObjectEditPos);
  ReadWinPos('ToolsPos',ToolsPos);
  ReadBool('SectorEdit.OnTop',SCEditOnTop);
  ReadBool('WallEdit.OnTop',WLEditOnTop);
  ReadBool('VertexEdit.OnTop',VXEditOnTop);
  ReadBool('ObjectEdit.OnTop',OBEditOnTop);
  ReadBool('Tools.OnTop',ToolsOnTop);
  ReadBool('Script.OnTop',ScriptOnTop);
  CBScrEditOnTop.Checked:=ScriptOnTop;
  ReadBool('LogTestSession',LogTestSession);
  For i:=1 to sizeof(Confirms) div sizeof(TConfirmRec) do
  With Confirms[i] do ReadBool(Rkey,Value);

  For i:=0 to 3 do
  begin
  s:='';
  ReadStr('Recent'+IntToStr(i),s);
  if s<>'' then MapWindow.AddRecent(s);
  end;
  ReadFont('ScriptFont',ScFont);
  Reg.Free;
  end; *)

Procedure TOptions.InitControls;

  Procedure NewCtl(data: Pointer; d_type: TDataType; ctrl: TObject);
  var
    ctl: TCtrlData;
  begin
    ctl := TCtrlData.Create;
    ctl.data := data;
    ctl.d_type := d_type;
    ctl.control := ctrl;
    if ctrl.ClassType = TDirectoryListBox then
      ctl.c_type := ct_DirList
    else if ctrl.ClassType = TDriveComboBox then
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
    else if ctrl.ClassType = TEdit then
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
  vGamma := TValInput.Create(EBGamma);
  NewCtl(@JKDir, dt_str, DirList);
  NewCtl(@JKCDDir, dt_str, CBCD);
  NewCtl(@MOTSDir, dt_str, Motsdirs);
  NewCtl(@MOTSCDDir, dt_str, CBMOTSCD);
  NewCtl(@D3DDevice, dt_str, LBDevices);
  NewCtl(@P3DWinSize, dt_int, RGWSize);
  NewCtl(@P3DOnTop, dt_bool, CBP3DOnTop);
  NewCtl(@P3DCOloredLights, dt_bool, CBColored);
  NewCtl(@P3DFullLit, dt_bool, CBFullLight);
  NewCtl(@P3DGamma, dt_double, vGamma);
  NewCtl(@P3DVisLayers, dt_bool, CB3DLayers);
  NewCtl(@P3DThings, dt_bool, CBShowThings);
  NewCtl(@P3DAPI, dt_int, RGAPI);
  NewCtl(@WireframeAPI, dt_int, RGWireframe);
  NewCtl(@WF_DoubleBuf, dt_bool, CBDbuf);
  NewCtl(@MapRot, dt_int, RGMapRot);

  NewCtl(@AutoSave, dt_bool, CBAutoSave);
  NewCtl(@SaveInterval, dt_int, EBSaveInt);

  NewCtl(@NewOnFloor, dt_bool, CBThingsOnFloor);
  NewCtl(@UndoEnabled, dt_bool, CBUndo);
  NewCtl(@MoveFrames, dt_bool, CBMoveFrames);
  NewCtl(@GOBSmart, dt_bool, CBGobSmart);
  NewCtl(@CheckOverlaps, dt_bool, CBCheckOverlaps);
  NewCtl(@NewLightCalc, dt_bool, CBNewLightCalc);
  NewCtl(@ConfirmRevert, dt_bool, CBConfRevert);

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
              ct_LBox:
                begin
                  l := TListBox(control).Items.IndexOf(String(data^));
                  TListBox(control).ItemIndex := l;
                end;
              ct_DirList:
                TDirectoryListBox(control).Directory := String(data^);
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
            ct_DirList:
              begin
                changed := String(data^) <> TDirectoryListBox(control)
                  .Directory;
                String(data^) := TDirectoryListBox(control).Directory;
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
  if JKCDDir = '' then
    JKCDDir := 'D';
  JKCDDir := JKCDDir[1] + ':\GAMEDATA\';
  if MOTSCDDir = '' then
    MOTSCDDir := 'D';
  MOTSCDDir := MOTSCDDir[1] + ':\GAMEDATA\';
  if JKDir <> '' then
    if JKDir[Length(JKDir)] <> '\' then
      JKDir := JKDir + '\';
  if MOTSDir <> '' then
    if MOTSDir[Length(MOTSDir)] <> '\' then
      MOTSDir := MOTSDir + '\';

  SetMOTS(isMots);
  Jedmain.SaveTimer.Enabled := AutoSave;
  Jedmain.SaveTimer.Interval := SaveInterval * 60 * 1000;
  { LoadToolBar(JedMain.PtoolBar,'op'); }
end;

Function TOptions.CheckForJKCD: boolean;
var
  sr: TSearchRec;

  Function IsMOTSCD: boolean;
  begin
    result := FindFirst(MOTSCDDir + 'Resource\video\S1l1ocs.san',
      faAnyfile, sr) = 0;
    FindClose(sr);
    result := result and (sr.size > 15 * 1024 * 1024);
    result := result and (FindFirst(MOTSCDDir + 'Resource\video\S2l1ecs.san',
      faAnyfile, sr) = 0);
    FindClose(sr);
    result := result and (sr.size > 13 * 1024 * 1024);
    result := result and (FindFirst(MOTSCDDir + 'Resource\video\Jkmintro.san',
      faAnyfile, sr) = 0);
    FindClose(sr);
    result := result and (sr.size > 15 * 1024 * 1024);
  end;

  Function IsCD1: boolean;
  begin
    result := FindFirst(JKCDDir + 'Resource\video\01-02a.smk',
      faAnyfile, sr) = 0;
    FindClose(sr);
    result := result and (sr.size > 30 * 1024 * 1024);
    result := result and (FindFirst(JKCDDir + 'Resource\video\03-04a.smk',
      faAnyfile, sr) = 0);
    FindClose(sr);
    result := result and (sr.size > 30 * 1024 * 1024);
  end;

  Function IsCD2: boolean;
  begin
    result := FindFirst(JKCDDir + 'Resource\video\33-34a.smk',
      faAnyfile, sr) = 0;
    FindClose(sr);
    result := result and (sr.size > 30 * 1024 * 1024);
    result := result and (FindFirst(JKCDDir + 'Resource\video\41-42a.smk',
      faAnyfile, sr) = 0);
    FindClose(sr);
    result := result and (sr.size > 20 * 1024 * 1024);
  end;

var
  cd1, cd2, motscd: boolean;
begin
  cd1 := IsCD1;
  cd2 := IsCD2;
  motscd := IsMOTSCD;
  isMots := motscd;
  result := (cd1 <> cd2) or motscd;
end;

procedure TOptions.FormCreate(Sender: TObject);
var
  Reg: TRegistry;
begin
  ctls := TList.Create;
  InitControls;
  if NoKey then
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
        JKCDDir := Reg.ReadString('CD Path') + '\GAMEDATA\';
      end;
      if Reg.OpenKey
        ('\SOFTWARE\LucasArts Entertainment Company LLC\Mysteries of the Sith\v1.0',
        false) then
      begin
        MOTSDir := Reg.ReadString('Install Path');
        MOTSCDDir := Reg.ReadString('CD Path') + '\GAMEDATA\';
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
      WriteRegistry(false);
  end;
  { Check for JK CD }
  While not CheckForJKCD do
  begin
    MsgBox('Cannot find JK data - Insert JK CD', 'Error', mb_ok);
    if not SetOptions(JK) then
    begin
      SaveSettings := false;
      Application.Terminate;
      exit;
    end
    else
      WriteRegistry(false);
  end;
  OnSetData;
end;

procedure TOptions.SBHelpClick(Sender: TObject);
begin
  Application.helpfile := basedir + 'jedhelp.hlp';
  Application.HelpContext(Pages.ActivePage.HelpContext);
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

Function FindGob(const name: string): String;
begin
  result := '';
  if FileExists(JKDir + 'Episode\' + name) then
    result := JKDir + 'Episode\' + name
  else if FileExists(JKDir + 'Resource\' + name) then
    result := JKDir + 'Resource\' + name
  else if FileExists(JKCDDir + 'Episode\' + name) then
    result := JKCDDir + 'Episode\' + name
  else if FileExists(JKCDDir + 'Resource\' + name) then
    result := JKCDDir + 'Resource\' + name;
end;

Function FindGoo(const name: string): String;
begin
  result := '';
  if FileExists(MOTSDir + 'Episode\' + name) then
    result := MOTSDir + 'Episode\' + name
  else if FileExists(MOTSDir + 'Resource\' + name) then
    result := MOTSDir + 'Resource\' + name
  else if FileExists(MOTSCDDir + 'Episode\' + name) then
    result := MOTSCDDir + 'Episode\' + name
  else if FileExists(MOTSCDDir + 'Resource\' + name) then
    result := MOTSCDDir + 'Resource\' + name
  else if FileExists(JKCDDir + 'MININSTALL\' + name) then
    result := JKCDDir + 'MININSTALL\' + name;
end;

Function SetMOTS(isit: boolean): boolean;
begin
  isMots := isit;
  case isit of
    true:
      begin
        GameDir := MOTSDir;
        CDDir := MOTSCDDir;
        res1_gob := FindGoo('JKMsndLO.goo');
        Res2_gob := FindGoo('Jkmres.goo');
        sp_gob := FindGoo('Jkm.goo');
        mp1_gob := FindGoo('Jkm_mp.goo');
        mp2_gob := FindGoo('Jkm_kfy.goo');
        mp3_gob := FindGoo('JKM_SABER.GOO');
        result := (GameDir <> '') and (res1_gob <> '') and (Res2_gob <> '') and
          (mp1_gob <> '') and (mp2_gob <> '') and (mp3_gob <> '');
        Level.Mots := isMots;
        if not result then
          PanMessage(mt_warning,
            'Some resources weren''t found! Check MOTS settings in Options');
      end;
    false:
      begin
        CDDir := JKCDDir;
        GameDir := JKDir;
        res1_gob := FindGob('Res1hi.gob');
        if res1_gob = '' then
          res1_gob := FindGob('Res1low.gob');
        if res1_gob = '' then
        begin
          PanMessage(mt_info,
            'Check JK directory setting - it appears to be wrong');
          res1_gob := CDDir + 'MININSTALL\Res1low.gob';
        end;
        Res2_gob := FindGob('Res2.gob');
        sp_gob := FindGob('Jk1.gob');
        mp1_gob := FindGob('Jk1mp.gob');
        mp2_gob := FindGob('Jk1ctf.gob');
        mp3_gob := '';
        Level.Mots := isMots;
        result := (GameDir <> '') and (res1_gob <> '') and (Res2_gob <> '') and
          (mp1_gob <> '') and (mp2_gob <> '');
        if not result then
          PanMessage(mt_warning,
            'Some resources weren''t found! Check JK settings in Options');
      end;

  end;
end;

procedure TOptions.CBAutoSaveClick(Sender: TObject);
begin
  EBSaveInt.Enabled := CBAutoSave.Checked;
end;

Initialization

ReadRegistry;

Finalization

if SaveSettings then
  WriteRegistry(true);

end.
