unit U_CogForm;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  Grids, ExtCtrls, StdCtrls, ComCtrls, J_Level, Values, FieldEdit,
  Misc_utils, GlobalVars, ShellApi, files, fileoperations,
  lev_utils;

type
  TCogForm = class(TForm)
    Panel1: TPanel;
    LVCOGs: TListView;
    SGVals: TStringGrid;
    BNAdd: TButton;
    BNDelete: TButton;
    BNRefresh: TButton;
    BNEdit: TButton;
    BNGetSel: TButton;
    CBOnTop: TCheckBox;
    BNDuplicate: TButton;
    procedure BNRefreshClick(Sender: TObject);
    procedure FormCreate(Sender: TObject);
    procedure LVCOGsChange(Sender: TObject; Item: TListItem;
      Change: TItemChange);
    procedure BNAddClick(Sender: TObject);
    procedure BNDeleteClick(Sender: TObject);
    procedure SGValsExit(Sender: TObject);
    procedure BNEditClick(Sender: TObject);
    procedure BNGetSelClick(Sender: TObject);
    procedure CBOnTopClick(Sender: TObject);
    procedure BNDuplicateClick(Sender: TObject);
    procedure FormDestroy(Sender: TObject);
  private
    { Private declarations }
    Function FieldChange(fi: TFieldInfo): boolean;
    Procedure FieldDblClick(fi: TFieldInfo);
    Function AddCog(const fname: string): integer;
    Function AddCogWithVals(const fname: string; fromcog: TCOG): integer;
  public
    fe: TFieldEdit;
    Procedure RefreshList;
    Procedure GotoCOG(n: integer);
    Procedure GotoCOGVal(c,v: integer);
    Procedure UpdateCOG(n: integer);
    { Public declarations }
  end;

var
  CogForm: TCogForm;

implementation

uses ResourcePicker, Cog_utils, ListRes, Jed_Main;

{$R *.DFM}

Procedure TCogForm.UpdateCOG(n: integer);
var
  i, j: integer;
  li: TListItem;
  s: string;
  vl: TCogValue;
begin
  if not CheckCogIndex(level, n) then
    exit;

  li := LVCOGs.Items[n];
  with level.cogs[n] do
  begin
    s := '';
    for j := 0 to vals.Count - 1 do
    begin
      vl := vals[j];
      s := Concat(s, ' ', vl.name, '=', vl.AsString);
    end;
    li.SubItems.BeginUpdate;
    li.SubItems[0] := s;
    li.SubItems.EndUpdate;
    LVCOGsChange(nil, nil, ctText);  // update grid view of cog values
  end;
end;

Procedure TCogForm.RefreshList;
var
  li: TListItem;
  vl: TCogValue;
  ce: TLVChangeEvent;
begin
  var selc := LVCOGs.Selected;
  ce := LVCOGs.OnChange;
  LVCOGs.OnChange := nil;

  LVCOGs.Items.BeginUpdate;
  LVCOGs.Items.Clear;
  try
    for var i := 0 to level.cogs.Count - 1 do
      with level.cogs[i] do
      begin
        li := LVCOGs.Items.Add;
        li.Caption := Format('%d: %s', [i, name]);
        var s := '';
        for var j := 0 to vals.Count - 1 do
        begin
          vl := vals[j];
          s := Concat(s, ' ', vl.name, '=', vl.AsString);
        end;
        li.SubItems.Add(s);
      end;
  Finally
    LVCOGs.Items.EndUpdate;
    LVCOGs.OnChange := ce;
  end;

  if LVCOGs.Items.IndexOf(selc) >= 0then
  begin
    LVCOGs.ItemFocused := selc;
    LVCOGs.Selected := selc;
    LVCOGs.Selected.MakeVisible(false);
  end
  else if LVCOGs.Items.Count > 0 then
    begin
      LVCOGs.Selected := LVCOGs.Items[0];
      LVCOGs.ItemFocused := LVCOGs.Items[0];
    end
  else
    begin
      LVCOGs.Selected := nil;
      LVCOGs.ItemFocused := nil;
      LVCOGsChange(nil, nil, ctText);
    end;
  { LVCOGsChange(nil,nil,ctText); }

end;

procedure TCogForm.BNRefreshClick(Sender: TObject);
var
  i, j, k: Integer;
  cf: TCogFile;
  v, vnew: TCogValue;
  s: string;
  vvals, vnames: TStringList;

begin
  var sc := LVCOGs.ItemIndex;
  var sv := SGVals.Row;
  cf     := TCogFile.Create;
  vvals  := TStringList.Create;
  vnames := TStringList.Create;

  try
    for i := 0 to level.cogs.Count - 1 do
      with level.cogs[i] do
      begin
        vvals.Clear;
        vnames.Clear;

        for j := vals.Count - 1 downto 0 do
        begin
          v := vals[j];
          vnames.Add(v.name);
          vvals.Add(v.AsString);
          vals.Delete(j);
          v.Free;
        end;

        cf.LoadNoLocals(name);
        for j := 0 to cf.Count - 1 do
        begin
          v := TCogValue.Create;
          vals.Add(v);
          vnew := cf[j];
          v.Assign(vnew);

          k := vnames.IndexOf(vnew.name);
          if k <> -1 then
            v.Val(vvals[k])
          else
            v.Val(vnew.AsString);

          v.Resolve;
        end;
      end;
  finally
    vvals.Free;
    vnames.Free;
    cf.Free;
    RefreshList;
    GotoCOGVal(sc, sv);
  end;
end;

procedure TCogForm.FormCreate(Sender: TObject);
begin
  fe := TFieldEdit.Create(SGVals);
end;

procedure TCogForm.LVCOGsChange(Sender: TObject; Item: TListItem; Change: TItemChange);
var
  i, n: Integer;
begin
  if fe = nil then
    exit;

  fe.Clear;

  if LVCOGs.ItemFocused = nil then
    exit;

  n := LVCOGs.ItemFocused.Index;
  if not CheckCogIndex(level, n) then
    exit;

  with level.cogs[n] do
  begin
    Caption := Format('Placed Cogs - COG %d: %s', [n, name]);
    for i := 0 to vals.Count - 1 do
      with vals[i] do
        fe.AddFieldStr(name + ' (' + GetCogTypeName(cog_type) + ')', i,
          AsJedString);
  end;

  fe.DoneAdding;
  fe.OnChange   := FieldChange;
  fe.OnDblClick := FieldDblClick;
end;

Function TCogForm.FieldChange(fi: TFieldInfo): boolean;
var
  v: TCogValue;
  n: integer;
  cg: TCOG;
  d: double;
begin
  if LVCOGs.ItemFocused = nil then
    exit;

  n  := LVCOGs.ItemFocused.Index;
  cg := level.cogs[n];
  v  := cg.Vals[fi.id];
  Result := v.JedVal(fi.s);
  if not Result then
    PanMessage(mt_error, Format('Invalid value for %s: %s',
      [GetCogTypeName(v.cog_type), fi.s]));
end;

Procedure TCogForm.FieldDblClick(fi: TFieldInfo);
var
  v: TCogValue;
  i, n: Integer;
  cg: TCOG;
  d: double;
  fname: string;
begin
  if LVCOGs.ItemFocused = nil then
    exit;

  n  := LVCOGs.ItemFocused.Index;
  cg := level.cogs[n];
  v  := cg.vals[fi.id];
  case v.cog_type of
    ct_unk:
      ;
    ct_ai:
      fi.s := ResPicker.PickAI(fi.s);
    ct_cog:
      begin
        i := StrToInt(fi.s);
        if (i < 0) then
          begin
            fname := ResPicker.PickCog('');
            if fname <> '' then
              fi.s := IntToStr(AddCog(fname));
          end
        else
          GotoCOG(i);
      end;
    ct_key:
      fi.s := ResPicker.PickKEY(fi.s);
    ct_mat:
      fi.s := ResPicker.PickMat(fi.s);
    ct_msg:
      ;
    ct_3do:
      fi.s := ResPicker.Pick3DO(fi.s);
    ct_sec:
      begin
        ValInt(fi.s, n);
        if n >= 0 then
          JedMain.GotoSC(n);
      end;
    ct_wav:
      fi.s := ResPicker.PickSecSound(fi.s);
    ct_srf:
      begin
        SScanf(fi.s, '%d %d', [@i, @n]);
        if i >= 0 then
          JedMain.GotoSF(i, n);
      end;
    ct_tpl:
      fi.s := ResPicker.PickTemplate(fi.s);
    ct_thg:
      begin
        ValInt(fi.s, n);
        if n >= 0 then
          JedMain.GotoTH(n);
      end;
    ct_int, ct_float, ct_vect:
      ;
  end;

end;

Function TCogForm.AddCog(const fname: string): integer;
begin
  Result := AddCogWithVals(fname, nil);
end;

Function TCogForm.AddCogWithVals(const fname: string; fromcog: TCOG): integer;
var
  cf: TCogFile;
  cg: TCOG;
  i, n: integer;
  cv, v: TCogValue;
  li: TListItem;
  s: string;
  ce: TLVChangeEvent;
begin
  cf := TCogFile.Create;
  cf.LoadNoLocals(fname);
  cg := TCOG.Create;
  cg.name := fname;

  for i := 0 to cf.Count - 1 do
  begin
    cv := cf[i];
    v := TCogValue.Create;
    v.Assign(cv);
    v.Val(cv.AsString);
    cg.vals.Add(v);
  end;
  cf.Free;

  if fromcog <> nil then
    for i := 0 to cg.vals.Count - 1 do
    begin
      v := cg.vals[i];
      n := fromcog.vals.IndexOfName(v.name);
      if n <> -1 then
        v.JedVal(fromcog.vals[n].AsJedString);
    end;

  Result := level.cogs.Add(cg);

  ce := LVCOGs.OnChange;
  LVCOGs.OnChange := nil;

  li := LVCOGs.Items.Add;
  li.Caption := cg.name;

  s := '';
  for i := 0 to cg.vals.Count - 1 do
  begin
    v := cg.vals[i];
    s := Concat(s, ' ', v.name, '=', v.AsString);
  end;

  li.SubItems.Add(s);
  LVCOGs.OnChange := ce;
end;

procedure TCogForm.BNAddClick(Sender: TObject);
var
  fname: string;
begin
  fname := ResPicker.PickCog('');
  if fname = '' then
    exit;
  GotoCOG(AddCog(fname));
end;

procedure TCogForm.BNDeleteClick(Sender: TObject);
var
  cg: TCOG;
  n: integer;
begin
  if LVCOGs.ItemFocused = nil then
    exit;

  if MsgBox('Are you sure you want to delete COG?', 'Delete COG', MB_YESNO) <> ID_YES then
    exit;

  n := LVCOGs.ItemFocused.Index;
  lev_utils.DeleteCOG(level, n);
  if n >= LVCOGs.Items.Count then
    n := LVCOGs.Items.Count - 1;
  GotoCOG(n);
end;

procedure TCogForm.SGValsExit(Sender: TObject);
begin
  fe.DeactivateHandler;
end;

Procedure TCogForm.GotoCOG(n: integer);
begin
  if not CheckCogIndex(level, n) then
    exit;

  LVCOGs.ItemFocused := LVCOGs.Items[n];
  LVCOGs.Selected    := LVCOGs.Items[n];
  LVCOGs.Selected.MakeVisible(false);
  Show;
  ActiveControl := LVCOGs;
end;

Procedure TCogForm.GotoCOGVal(c,v: integer);
begin
  if not CheckCogIndex(level, c) then
    exit;
  GotoCOG(c);

  var cog := level.cogs[c];
  if (v < 0) or (v >= level.cogs[c].vals.Count) then
    exit;

  SGVals.Row := v;
  SGVals.SetFocus;
end;

procedure TCogForm.BNEditClick(Sender: TObject);
var
  cg: TCOG;
  n: integer;
  fname: string;
  f, f1: TFile;
begin
  if projectdir = '' then
  begin
    ShowMessage('Save project first');
    exit;
  end;

  if LVCOGs.ItemFocused = nil then
    exit;

  n  := LVCOGs.ItemFocused.Index;
  if not CheckCogIndex(level, n) then
    exit;
  cg := level.cogs[n];

  fname := '';
  if FileExists(ProjectDir + cg.name) then
    fname := ProjectDir + cg.name
  else if FileExists(projectdir + 'cog\' + cg.name) then
    fname := ProjectDir + 'cog\' + cg.name;

  if fname = '' then
  begin
    if MsgBox('The COG is inside a GOB file. Extract it to edit?', 'Question',
      MB_YESNO) <> idYes then
      exit;
    f := OpenGameFile(cg.name);
{$I-}
    MkDir(projectdir + 'cog');
    if ioresult = 0 then;
{$I+}
    fname := ProjectDir + 'cog\' + cg.name;
    f1 := OpenFileWrite(fname, 0);
    CopyFileData(f, f1, f.fsize);
    f.FClose;
    f1.FClose;
  end;

  n := ShellExecute(Application.Handle, nil, Pchar(fname), nil,
    Pchar(ExtractFilePath(fname)), SW_SHOWNORMAL);
  case n of
    SE_ERR_NOASSOC:
      PanMessage(mt_error, 'No programm is associated with COGs!');
  else
    ;
  end;

end;

procedure TCogForm.BNGetSelClick(Sender: TObject);
var
  i, n: integer;
  v: TCogValue;
  obj: TObject;
begin
  if LVCOGs.ItemFocused = nil then
  begin
    ShowMessage('No COG selected');
    exit;
  end;

  n := LVCOGs.ItemFocused.Index;
  i := SGVals.Row;
  if i < 0 then
  begin
    ShowMessage('No COG value selected');
    exit;
  end;

  v := level.cogs[n].vals[i];
  Case v.cog_type of
    ct_thg, ct_sec, ct_srf:
      begin
        obj := JedMain.GetCurObjForCog(v.cog_type);
        if obj <> nil then
          v.obj := obj;

        var tr := SGVals.TopRow;
        UpdateCOG(n);
        SGVals.TopRow := tr;
        SGVals.Row    := i;
        SGVals.SetFocus;
      end;
  else
    ShowMessage('It only works for sector/surface and thing entries');
  end;
end;

procedure TCogForm.CBOnTopClick(Sender: TObject);
begin
  CFOnTop := CBOnTop.Checked;
  SetStayOnTop(Self, CFOnTop);
end;

procedure TCogForm.BNDuplicateClick(Sender: TObject);
var
  n: integer;
begin
  if LVCOGs.ItemFocused = nil then
  begin
    ShowMessage('No COG selected');
    exit;
  end;
  n := LVCOGs.ItemFocused.Index;
  GotoCOG(AddCogWithVals(level.cogs[n].name, level.cogs[n]));
end;

procedure TCogForm.FormDestroy(Sender: TObject);
begin
  fe.Free;
  fe := nil;
end;

end.
