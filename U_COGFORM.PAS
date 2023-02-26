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
  if n >= LVCOGs.Items.COunt then
    exit;
  li := LVCOGs.Items[n];

  With Level.Cogs[n] do
  begin
    li.Caption := Name;
    s := '';
    for j := 0 to Vals.COunt - 1 do
    begin
      vl := Vals[j];
      s := Concat(s, ' ', vl.Name, '=', vl.AsString);
    end;
    li.SubItems[0] := s;
  end;
end;

Procedure TCogForm.RefreshList;
var
  i, j: integer;
  li: TListItem;
  s: string;
  vl: TCogValue;
  ce: TLVChangeEvent;
begin
  ce := LVCOGs.OnChange;
  LVCOGs.OnChange := nil;

  LVCOGs.Items.BeginUpdate;
  LVCOGs.Items.Clear;
  try
    for i := 0 to Level.Cogs.COunt - 1 do
      With Level.Cogs[i] do
      begin
        li := LVCOGs.Items.Add;
        li.Caption := Format('%d: %s', [i, Name]);
        s := '';
        for j := 0 to Vals.COunt - 1 do
        begin
          vl := Vals[j];
          s := Concat(s, ' ', vl.Name, '=', vl.AsString);
        end;
        li.SubItems.Add(s);
      end;
  Finally
    LVCOGs.Items.EndUpdate;
    LVCOGs.OnChange := ce;
  end;

  if LVCOGs.Items.COunt > 0 then
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
  i, j, k: integer;
  cf: TCogFile;
  v, vnew: TCogValue;
  s: string;
  vvals, vnames: TStringList;

begin
  cf := TCogFile.Create;
  vvals := TStringList.Create;
  vnames := TStringList.Create;

  try
    for i := 0 to Level.Cogs.COunt - 1 do
      with Level.Cogs[i] do
      begin
        vvals.Clear;
        vnames.Clear;

        for j := Vals.COunt - 1 downto 0 do
        begin
          v := Vals[j];
          vnames.Add(v.Name);
          vvals.Add(v.AsString);
          Vals.Delete(j);
          v.free;
        end;

        cf.LoadNoLocals(Name);
        for j := 0 to cf.COunt - 1 do
        begin
          v := TCogValue.Create;
          Vals.Add(v);
          vnew := cf[j];
          v.Assign(vnew);

          k := vnames.IndexOf(vnew.Name);
          if k <> -1 then
            v.Val(vvals[k])
          else
            v.Val(vnew.AsString);

          v.Resolve;
        end;

      end;
  finally
    vvals.free;
    vnames.free;
    cf.free;
    RefreshList;
  end;
end;

procedure TCogForm.FormCreate(Sender: TObject);
begin
  fe := TFieldEdit.Create(SGVals);
end;

procedure TCogForm.LVCOGsChange(Sender: TObject; Item: TListItem;
  Change: TItemChange);
var
  i, n: integer;
begin
  if fe = nil then
    exit;
  fe.Clear;
  if LVCOGs.ItemFocused = nil then
    exit;
  n := LVCOGs.ItemFocused.Index;
  if n >= Level.Cogs.COunt then
    exit;
  fe.Clear;
  With Level.Cogs[n] do
  begin
    Caption := Format('Placed Cogs - COG %d: %s', [n, Name]);
    for i := 0 to Vals.COunt - 1 do
      With Vals[i] do
        fe.AddFieldStr(Name + ' (' + GetCogTypeName(cog_type) + ')', i,
          AsJedString);
  end;
  fe.DoneAdding;
  fe.OnChange := FieldChange;
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
  n := LVCOGs.ItemFocused.Index;
  cg := Level.Cogs[n];
  v := cg.Vals[fi.id];
  Result := v.JedVal(fi.s);
  if not Result then
    PanMessage(mt_error, Format('Invalid value for %s: %s',
      [GetCogTypeName(v.cog_type), fi.s]));
end;

Procedure TCogForm.FieldDblClick(fi: TFieldInfo);
var
  v: TCogValue;
  i, n: integer;
  cg: TCOG;
  d: double;
  fname: string;
begin
  if LVCOGs.ItemFocused = nil then
    exit;
  n := LVCOGs.ItemFocused.Index;
  cg := Level.Cogs[n];
  v := cg.Vals[fi.id];
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
  cg.Name := fname;
  for i := 0 to cf.COunt - 1 do
  begin
    cv := cf[i];
    v := TCogValue.Create;
    v.Assign(cv);
    v.Val(cv.AsString);
    cg.Vals.Add(v);
  end;
  cf.free;

  if fromcog <> nil then
    for i := 0 to cg.Vals.COunt - 1 do
    begin
      v := cg.Vals[i];
      n := fromcog.Vals.IndexOfName(v.Name);
      if n <> -1 then
        v.JedVal(fromcog.Vals[n].AsJedString);
    end;

  Result := Level.Cogs.Add(cg);

  ce := LVCOGs.OnChange;
  LVCOGs.OnChange := nil;

  li := LVCOGs.Items.Add;
  li.Caption := cg.Name;

  s := '';
  for i := 0 to cg.Vals.COunt - 1 do
  begin
    v := cg.Vals[i];
    s := Concat(s, ' ', v.Name, '=', v.AsString);
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
  n := LVCOGs.ItemFocused.Index;
  lev_utils.DeleteCOG(Level, n);
  if n >= LVCOGs.Items.COunt then
    n := LVCOGs.Items.COunt - 1;
  GotoCOG(n);
end;

procedure TCogForm.SGValsExit(Sender: TObject);
begin
  fe.DeactivateHandler;
end;

Procedure TCogForm.GotoCOG(n: integer);
begin
  if (n < 0) or (n >= Level.Cogs.COunt) then
    exit;
  LVCOGs.ItemFocused := LVCOGs.Items[n];
  LVCOGs.Selected := LVCOGs.Items[n];
  LVCOGs.Selected.MakeVisible(false);
  Show;
  ActiveControl := LVCOGs;

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
  n := LVCOGs.ItemFocused.Index;
  cg := Level.Cogs[n];

  fname := '';
  if FileExists(projectdir + cg.Name) then
    fname := projectdir + cg.Name
  else if FileExists(projectdir + 'cog\' + cg.Name) then
    fname := projectdir + 'cog\' + cg.Name;

  if fname = '' then
  begin
    if MsgBox('The COG is inside a GOB file. Extract it to edit?', 'Question',
      MB_YESNO) <> idYes then
      exit;
    f := OpenGameFile(cg.Name);
{$I-}
    MkDir(projectdir + 'cog');
    if ioresult = 0 then;
{$I+}
    fname := projectdir + 'cog\' + cg.Name;
    f1 := OpenFileWrite(fname, 0);
    CopyFileData(f, f1, f.fsize);
    f.Fclose;
    f1.Fclose;
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
  v := Level.Cogs[n].Vals[i];

  Case v.cog_type of
    ct_thg, ct_sec, ct_srf:
      begin
        obj := JedMain.GetCurObjForCog(v.cog_type);
        if obj <> nil then
          v.obj := obj;
        UpdateCOG(n);
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
  GotoCOG(AddCogWithVals(Level.Cogs[n].Name, Level.Cogs[n]));
end;

procedure TCogForm.FormDestroy(Sender: TObject);
begin
  fe.free;
  fe := nil;
end;

end.
