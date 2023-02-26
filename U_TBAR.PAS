unit U_tbar;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  StdCtrls, ExtCtrls, Menus, J_Level, ComCtrls, Prefab;

Const
  maxlayers = 2048;

type
  TToolbar = class(TForm)
    Panel1: TPanel;
    BNShow: TButton;
    BNHide: TButton;
    BNDel: TButton;
    Panel2: TPanel;
    LVLayers: TListView;
    PGStuff: TPageControl;
    PGrid: TTabSheet;
    CBGridStep: TComboBox;
    Label1: TLabel;
    Label2: TLabel;
    CBGridLine: TComboBox;
    CBGridDot: TComboBox;
    Label3: TLabel;
    PTXing: TTabSheet;
    CBTxStep: TComboBox;
    CBRotStep: TComboBox;
    Label4: TLabel;
    CBGrid: TCheckBox;
    CBOnTop: TCheckBox;
    PShapes: TTabSheet;
    CBShapes: TComboBox;
    Label6: TLabel;
    Label5: TLabel;
    PerShift: TTabSheet;
    CBPerpStep: TComboBox;
    Label7: TLabel;
    CBScaleStep: TComboBox;
    Label8: TLabel;
    Label9: TLabel;
    CB3DPStep: TComboBox;
    BNSave: TButton;
    CBGridSize: TComboBox;
    Label10: TLabel;
    Label11: TLabel;
    CBGridMove: TComboBox;
    BNDelShape: TButton;
    procedure FormCreate(Sender: TObject);
    procedure FormDestroy(Sender: TObject);
    procedure CBGridClick(Sender: TObject);
    procedure CBGridStepChange(Sender: TObject);
    procedure CBLayersChange(Sender: TObject);
    procedure BNShowClick(Sender: TObject);
    procedure BNHideClick(Sender: TObject);
    procedure LVLayersChange(Sender: TObject; Item: TListItem;
      Change: TItemChange);
    procedure LVLayersEdited(Sender: TObject; Item: TListItem; var S: string);
    procedure LVLayersDblClick(Sender: TObject);
    procedure BNDelClick(Sender: TObject);
    procedure LVLayersDragDrop(Sender, Source: TObject; X, Y: Integer);
    procedure LVLayersDragOver(Sender, Source: TObject; X, Y: Integer;
      State: TDragState; var Accept: Boolean);
    procedure CBGridLineChange(Sender: TObject);
    procedure CBGridDotChange(Sender: TObject);
    procedure CBTxStepChange(Sender: TObject);
    procedure CBRotStepChange(Sender: TObject);
    procedure CBOnTopClick(Sender: TObject);
    procedure CBGridStepExit(Sender: TObject);
    procedure CBGridLineExit(Sender: TObject);
    procedure CBGridDotExit(Sender: TObject);
    procedure CBTxStepExit(Sender: TObject);
    procedure CBRotStepExit(Sender: TObject);
    procedure CBPerpStepChange(Sender: TObject);
    procedure CBPerpStepExit(Sender: TObject);
    procedure CBScaleStepExit(Sender: TObject);
    procedure CBScaleStepChange(Sender: TObject);
    procedure CB3DPStepChange(Sender: TObject);
    procedure CB3DPStepExit(Sender: TObject);
    procedure BNSaveClick(Sender: TObject);
    procedure CBGridSizeExit(Sender: TObject);
    procedure CBGridSizeChange(Sender: TObject);
    procedure CBGridMoveChange(Sender: TObject);
    procedure CBGridMoveExit(Sender: TObject);
    procedure BNDelShapeClick(Sender: TObject);
  private
    { Private declarations }
    clayer, nlayers: Integer;
    lVis: array [0 .. maxlayers - 1] of Boolean;
    LCTable: array [0 .. maxlayers - 1] of SmallInt;
    Procedure WMNCDblClck(var msg: TMessage); message WM_NCLBUTTONDBLCLK;
  public
    Function AddLayer(const name: string): Integer;
    Function IsLayerVisible(n: Integer): Boolean;
    Procedure LoadLayers;
    Procedure RefreshLayers(showAll: Boolean);
    Procedure ConvertLayers;
    Function GetNewShape: TJKSector;
    Function GetLVisString: String;
    Procedure SetLVis(const S: string);
    Procedure SetDefaults;
    Procedure UpdatePrefabs;
  end;

var
  Toolbar: TToolbar;

implementation

uses GlobalVars, Misc_Utils, Jed_Main, FieldEdit, U_Options;

{$R *.DFM}

procedure TToolbar.FormCreate(Sender: TObject);
var
  i, mw: Integer;
begin
  mw := CBGridStep.Left + CBGridStep.Width + CBGrid.Left;
  if TBarPos.w < mw then
    TBarPos.w := mw;
  mw := 3 * Panel1.height + Panel2.height;
  if TBarPos.h < mw then
    TBarPos.h := mw;
  SetWinPos(Self, TBarPos);
  CBOnTop.Checked := TbOnTop;
  LoadPrefabs(BaseDir + 'JedData\shapes.tpl');
  UpdatePrefabs;
end;

Procedure TToolbar.UpdatePrefabs;
var
  i: Integer;
begin
  CBShapes.Items.Clear;
  for i := 0 to Prefabs.count - 1 do
    CBShapes.Items.Add(Prefabs[i]);

  i := CBShapes.Items.IndexOf(DefShape);
  if i = -1 then
    CBShapes.ItemIndex := 0
  else
    CBShapes.ItemIndex := i;
end;

Procedure TToolbar.SetDefaults;
  Procedure SetCB(CB: TComboBox; d: double);
  begin
    CB.Text := DoubleToStr(d);
    CB.OnChange(CB);
  end;

var
  i: Integer;
begin
  i := CBShapes.Items.IndexOf(DefShape);
  if i = -1 then
    CBShapes.ItemIndex := 0
  else
    CBShapes.ItemIndex := i;

  SetCB(CBGridStep, DefGridStep);
  SetCB(CBGridLine, DefGridLine);
  SetCB(CBGridDot, DefGridDot);

  SetCB(CBTxStep, DefTXStep);
  SetCB(CBRotStep, DefTXRotStep);
  SetCB(CBScaleStep, DefTXScaleStep);
  SetCB(CBPerpStep, DefPerpStep);
  SetCB(CB3DPStep, DefP3DStep);
  SetCB(CBGridSize, DefGridSize);
end;

procedure TToolbar.FormDestroy(Sender: TObject);
begin
  GetWinPos(Self, TBarPos);
  TbOnTop := FormStyle = fsStayOnTop;
end;

procedure TToolbar.CBGridClick(Sender: TObject);
begin
  JedMain.SnapToGrid := CBGrid.Checked;
  CBGridStep.Enabled := JedMain.SnapToGrid;
end;

procedure TToolbar.CBLayersChange(Sender: TObject);
begin
  { With JedMain do
    begin
    if CBLayers.ItemIndex=-1 then exit;
    if CBLayers.ItemIndex=0 then CurLayer:=All_Layers
    else CurLayer:=CBLayers.ItemIndex-1;
    JedMain.Invalidate;
    end; }
end;

Function TToolbar.AddLayer(const name: string): Integer;
var
  i: Integer;
begin
  i := Level.Layers.count;
  Result := Level.AddLayer(name);
  if Level.Layers.count = i then
    exit;
  With LVLayers.Items.Add do
  begin
    Caption := name;
    SubItems.Add('X');
  end;
  lVis[i] := true;
  nlayers := i + 1;
end;

Function TToolbar.IsLayerVisible(n: Integer): Boolean;
begin
  Result := { (n=clayer) or } (n < 0) or lVis[n] or (n >= nlayers);
end;

Procedure TToolbar.RefreshLayers(showAll: Boolean);
var
  i: Integer;
  cproc: TLVChangeEvent;
begin
  cproc := LVLayers.OnChange;
  LVLayers.OnChange := nil;
  LVLayers.Items.BeginUpdate;
  LVLayers.Items.Clear;
  for i := 0 to Level.Layers.count - 1 do
    With LVLayers.Items.Add do
    begin
      Caption := Level.Layers[i];
      if showAll then
        lVis[i] := true;
      if lVis[i] then
        SubItems.Add('X')
      else
        SubItems.Add('');
    end;
  LVLayers.Items.EndUpdate;
  LVLayers.OnChange := cproc;

  nlayers := Level.Layers.count;
end;

Procedure TToolbar.LoadLayers;
begin
  RefreshLayers(true);
end;

Function TToolbar.GetLVisString: String;
var
  i: Integer;
begin
  Result := '';
  for i := 0 to Level.Layers.count - 1 do
    if lVis[i] then
      Result := ConCat(Result, '1')
    else
      Result := ConCat(Result, '0')
end;

Procedure TToolbar.SetLVis(const S: string);
var
  i: Integer;
begin
  for i := 1 to Level.Layers.count do
    if (i > length(S)) or (S[i] = '1') then
      lVis[i - 1] := true
    else
      lVis[i - 1] := false;
  RefreshLayers(false);
end;

procedure TToolbar.BNShowClick(Sender: TObject);
var
  i, n: Integer;
  S: string;
  sel, li: TListItem;
begin
  if LVLayers.ItemFocused = nil then
    exit
  else
    n := LVLayers.ItemFocused.Index;

  LVLayers.Items.BeginUpdate;
  for i := 0 to LVLayers.Items.count - 1 do
  begin
    li := LVLayers.Items[i];
    if not li.Selected then
      continue;
    begin
      lVis[i] := true;
      li.SubItems[0] := 'X';
    end;
  end;
  LVLayers.Items.EndUpdate;

  LVLayers.SetFocus;
  JedMain.Invalidate;
end;

procedure TToolbar.BNHideClick(Sender: TObject);
var
  i, n: Integer;
  S: string;
  li: TListItem;
begin
  if LVLayers.ItemFocused = nil then
    exit
  else
    n := LVLayers.ItemFocused.Index;
  if n < 0 then
    exit;

  LVLayers.Items.BeginUpdate;
  for i := 0 to LVLayers.Items.count - 1 do
  begin
    li := LVLayers.Items[i];
    if not li.Selected then
      continue;
    lVis[i] := false;
    li.SubItems[0] := ' ';
  end;
  LVLayers.Items.EndUpdate;
  LVLayers.SetFocus;
  JedMain.Invalidate;
end;

procedure TToolbar.LVLayersChange(Sender: TObject; Item: TListItem;
  Change: TItemChange);
var
  lastlayer: Integer;
begin
  lastlayer := clayer;
  clayer := Item.Index;
  { if (not lVis[lastlayer]) or
    (not LVis[clayer]) then JedMain.Invalidate; }
end;

procedure TToolbar.LVLayersEdited(Sender: TObject; Item: TListItem;
  var S: string);
begin
  if Level.Layers.IndexOf(S) = -1 then
    Level.Layers[Item.Index] := S
  else
    S := Level.Layers[Item.Index];
end;

procedure TToolbar.LVLayersDblClick(Sender: TObject);
var
  n: Integer;
begin
  if LVLayers.ItemFocused = nil then
    exit;
  n := LVLayers.ItemFocused.Index;
  if lVis[n] then
    BNHideClick(nil)
  else
    BNShowClick(nil);
end;

procedure TToolbar.BNDelClick(Sender: TObject);
var
  i, n, cl: Integer;
  nl: TStringList;
begin
  FillChar(LCTable, sizeof(LCTable), 0);

  n := Level.Layers.count;

  { compile layer usage table }

  for i := 0 to Level.Sectors.count - 1 do
    with Level.Sectors[i] do
    begin
      if (layer >= 0) and (layer < n) then
        LCTable[layer] := 1;
    end;

  for i := 0 to Level.Things.count - 1 do
    with Level.Things[i] do
    begin
      if (layer >= 0) and (layer < n) then
        LCTable[layer] := 1;
    end;

  for i := 0 to Level.Lights.count - 1 do
    with Level.Lights[i] do
    begin
      if (layer >= 0) and (layer < n) then
        LCTable[layer] := 1;
    end;

  { Convert layer usage table to conversion table }

  cl := 0;
  For i := 0 to n - 1 do
  begin
    if LCTable[i] = 0 then
      LCTable[i] := -1
    else
    begin
      LCTable[i] := cl;
      inc(cl);
    end;
  end;

  { Convert layers }
  ConvertLayers;

  { Compile new list of layer names }
  nl := TStringList.Create;
  for i := 0 to n - 1 do
    if LCTable[i] <> -1 then
    begin
      lVis[Level.Layers.count] := lVis[i];
      nl.Add(Level.Layers[i]);
    end;
  Level.Layers.Assign(nl);
  nl.free;
  RefreshLayers(false);

end;

Procedure TToolbar.ConvertLayers;
var
  i, n: Integer;
begin
  n := Level.Layers.count;
  for i := 0 to Level.Sectors.count - 1 do
    with Level.Sectors[i] do
    begin
      if (layer >= 0) and (layer < n) then
        layer := LCTable[layer];
    end;

  for i := 0 to Level.Things.count - 1 do
    with Level.Things[i] do
    begin
      if (layer >= 0) and (layer < n) then
        layer := LCTable[layer];
    end;

  for i := 0 to Level.Lights.count - 1 do
    with Level.Lights[i] do
    begin
      if (layer >= 0) and (layer < n) then
        layer := LCTable[layer];
    end;
end;

procedure TToolbar.LVLayersDragDrop(Sender, Source: TObject; X, Y: Integer);
var
  i, n: Integer;
  li: TListItem;
  nsels, isel, iunsel, di, ins_idx: Integer;
  nl: TStringList;
  lnctable: array [0 .. maxlayers - 1] of SmallInt;
begin
  if i = 0 then;
  li := LVLayers.GetItemAt(X, Y);
  if li = nil then
    exit;
  n := Level.Layers.count;

  nsels := LVLayers.SelCount;

  ins_idx := li.Index;

  isel := 0;
  iunsel := 0;
  for i := 0 to n - 1 do
  begin
    if LVLayers.Items[i].Selected then
    begin
      lnctable[ins_idx + isel] := i;
      inc(isel);
      continue;
    end;
    if iunsel < ins_idx then
      lnctable[iunsel] := i
    else
      lnctable[nsels + iunsel] := i;
    inc(iunsel);
  end;

  for i := 0 to n - 1 do
    LCTable[lnctable[i]] := i;

  ConvertLayers;

  nl := TStringList.Create;
  for i := 0 to n - 1 do
  begin
    LCTable[i] := ord(lVis[lnctable[i]]);
    nl.Add(Level.Layers[lnctable[i]]);
  end;

  For i := 0 to n - 1 do
  begin
    lVis[i] := Boolean(LCTable[i]);
  end;

  Level.Layers.Assign(nl);
  nl.free;
  RefreshLayers(false);

end;

procedure TToolbar.LVLayersDragOver(Sender, Source: TObject; X, Y: Integer;
  State: TDragState; var Accept: Boolean);
begin
  Accept := Source = LVLayers;
end;

Procedure SetCBDouble(CB: TComboBox; d: double);
var
  a: TNotifyEvent;
begin
  a := CB.OnChange;
  CB.OnChange := nil;
  CB.Text := DoubleToStr(d);
  CB.OnChange := a;
end;

Procedure CBValDouble(CB: TComboBox; var d: double; min, max: double);
var
  ad: double;
  valid: Boolean;
  oldp: TNotifyEvent;
begin
  valid := feValDouble(CB.Text, ad);
  if (valid) and (ad >= min) and (ad <= max) then
    d := ad
  else
    exit;
  { oldp:=CB.OnChange;
    CB.OnChange:=nil;
    CB.Text:=Format('%g',[d]);
    CB.OnChange:=oldp; }
end;

procedure TToolbar.CBGridStepChange(Sender: TObject);
begin
  CBValDouble(CBGridStep, JedMain.Renderer.GridStep, 0.0001, 10);
end;

procedure TToolbar.CBGridStepExit(Sender: TObject);
begin
  SetCBDouble(CBGridStep, JedMain.Renderer.GridStep);
end;

procedure TToolbar.CBGridLineChange(Sender: TObject);
begin
  CBValDouble(CBGridLine, JedMain.Renderer.GridLine, 0.1, 10);
  JedMain.Invalidate;
end;

procedure TToolbar.CBGridLineExit(Sender: TObject);
begin
  SetCBDouble(CBGridLine, JedMain.Renderer.GridLine);
  JedMain.Invalidate;
end;

procedure TToolbar.CBGridDotChange(Sender: TObject);
begin
  CBValDouble(CBGridDot, JedMain.Renderer.GridDot, 0.05, 10);
  JedMain.Invalidate;
end;

procedure TToolbar.CBGridDotExit(Sender: TObject);
begin
  SetCBDouble(CBGridDot, JedMain.Renderer.GridDot);
end;

procedure TToolbar.CBTxStepChange(Sender: TObject);
begin
  CBValDouble(CBTxStep, TXStep, 0, 64);
end;

procedure TToolbar.CBTxStepExit(Sender: TObject);
begin
  SetCBDouble(CBTxStep, TXStep);
end;

procedure TToolbar.CBRotStepChange(Sender: TObject);
begin
  CBValDouble(CBRotStep, TXRotStep, 0, 360);
end;

procedure TToolbar.CBRotStepExit(Sender: TObject);
begin
  SetCBDouble(CBRotStep, TXRotStep);
end;

procedure TToolbar.CBPerpStepChange(Sender: TObject);
begin
  CBValDouble(CBPerpStep, PerpStep, 0, 1);
end;

procedure TToolbar.CBPerpStepExit(Sender: TObject);
begin
  SetCBDouble(CBPerpStep, PerpStep);
end;

procedure TToolbar.CBScaleStepChange(Sender: TObject);
begin
  CBValDouble(CBScaleStep, TXScaleStep, 1, 8);
end;

procedure TToolbar.CBGridSizeChange(Sender: TObject);
begin
  CBValDouble(CBGridSize, GridSize, 1, 100);
  JedMain.Invalidate;
end;

procedure TToolbar.CBScaleStepExit(Sender: TObject);
begin
  SetCBDouble(CBScaleStep, TXScaleStep);
end;

procedure TToolbar.CB3DPStepChange(Sender: TObject);
begin
  CBValDouble(CB3DPStep, P3DStep, 0.001, 5);
end;

procedure TToolbar.CB3DPStepExit(Sender: TObject);
begin
  SetCBDouble(CB3DPStep, P3DStep);
end;

procedure TToolbar.CBGridSizeExit(Sender: TObject);
begin
  SetCBDouble(CBGridSize, GridSize);
end;

procedure TToolbar.CBGridMoveChange(Sender: TObject);
begin
  CBValDouble(CBGridMove, GridMoveStep, 0.00001, 5);
end;

procedure TToolbar.CBGridMoveExit(Sender: TObject);
begin
  SetCBDouble(CBGridMove, GridMoveStep);
end;

Procedure TToolbar.WMNCDblClck(var msg: TMessage);
begin
  if msg.wparam <> HTCAPTION then
    exit;
  if ClientHeight < 16 then
    ClientHeight := TBarPos.h
  else
  begin
    GetWinPos(Self, TBarPos);
    ClientHeight := 0;
  end;
  msg.Result := 0;
end;

procedure TToolbar.CBOnTopClick(Sender: TObject);
begin
  TbOnTop := CBOnTop.Checked;
  SetStayOnTop(Self, TbOnTop);
end;

Function TToolbar.GetNewShape: TJKSector;
begin
  Result := GetPrefab(CBShapes.ItemIndex);
end;

procedure TToolbar.BNSaveClick(Sender: TObject);
begin
  DefTXStep := TXStep;
  DefTXRotStep := TXRotStep;
  DefTXScaleStep := TXScaleStep;
  DefPerpStep := PerpStep;
  DefP3DStep := P3DStep;
  DefThingView := JedMain.Thing_view;
  DefMselMode := JedMain.Msel_mode;

  DefGridStep := JedMain.Renderer.GridStep;
  DefGridLine := JedMain.Renderer.GridLine;
  DefGridDot := JedMain.Renderer.GridDot;
  DefGridSize := GridSize;
  DefShape := CBShapes.Text;

  WriteRegistry(true);
end;

procedure TToolbar.BNDelShapeClick(Sender: TObject);
var
  n: Integer;
begin
  n := CBShapes.ItemIndex;
  if n < 1 then
  begin
    ShowMessage('You can''t delete the shape "Cube"');
    exit;
  end;
  if MsgBox('Are you sure you want to delete shape "' + CBShapes.Text + '"',
    'Confirmation', MB_YESNO) <> idYes then
    exit;
  DeletePrefab(n);
  SavePrefabs(BaseDir + 'JedData\shapes.tpl');
  UpdatePrefabs;

  if CBShapes.Items.count > n then
    CBShapes.ItemIndex := n
  else
    CBShapes.ItemIndex := CBShapes.Items.count - 1;
end;

end.
