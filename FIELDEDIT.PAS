unit FieldEdit;

interface

uses misc_utils, Grids, Classes, StdCtrls;

{ TFieldEdit pseudo control
  For Sector/Wall/Vertex/Object editors
}

type

  TVarType = (vtString, vtDouble, vtInteger, vtDword, vtColor);

  TFieldInfo = class
  Private
    Grid: TStringGrid;
    row: Integer;
    olds: string;
    Function GetS: String;
    Procedure SetS(const s: String);
    Procedure BadValueMsg;
  Public
    id: Integer;
    changed: boolean;
    vtype: TVarType;
    Constructor Create(aGrid: TStringGrid; arow: Integer);
    Property s: String read GetS write SetS;
    Procedure ReadString(var st: String);
    Procedure ReadInteger(var i: Integer);
    Procedure Read0to31(var b: Byte);
    Procedure ReadDword(var d: longint);
    Procedure ReadDouble(var d: Double);
  end;

  TFEHandler = Procedure(Fi: TFieldInfo) of object;
  TFEChangeHandler = Function(Fi: TFieldInfo): boolean of object;
  TDoneEdit = procedure of object;

  TFieldEdit = class
  Private
    Grid: TStringGrid;
    fl: TList;
    oldRow: Integer;

    Function GetField(n: Integer): TFieldInfo;
    Function GetCurField: TFieldInfo;
    Procedure Changed(row: Integer);
    Procedure CallDoneEdit;
    procedure SetNameColWidth(width: Integer);
    Function GetNameColWidth: Integer;
    procedure SetDefaultColWidth(width: Integer);
    Function GetDefaultColWidth: Integer;

  Public
    FieldCount: Integer;
    ONDblClick: TFEHandler;
    ONChange: TFEChangeHandler;
    OnDoneEdit: TDoneEdit;

    Property NameColWidth: Integer read GetNameColWidth write SetNameColWidth;
    Property DefaultColWidth: Integer read GetDefaultColWidth write SetDefaultColWidth;

    Constructor Create(aGrid: TStringGrid);
    Destructor Destroy; override;
    Procedure Clear;
    Procedure DeactivateHandler;
    Property CurrentField: TFieldInfo read GetCurField;
    Property Fields[n: Integer]: TFieldInfo read GetField;
    procedure SelectHandler(Sender: TObject; Col, row: longint;
      var CanSelect: boolean);
    procedure DblClickHandler(Sender: TObject);
    procedure SetTextHandler(Sender: TObject; ACol, arow: longint;
      const Text: string);
    function AddField(const Name: String; id: Integer; vartype: TVarType)
      : TFieldInfo;
    Procedure AddFieldInt(const Name: string; id, V: Integer);
    Procedure AddFieldHex(const Name: string; id: Integer; V: longint);
    Procedure AddFieldFloat(const Name: string; id: Integer; V: Double);
    Procedure AddFieldColor(const Name: string; id: Integer;
      const V: TColorF; withAlpha: Boolean = true);
    Procedure AddFieldStr(const Name: string; id: Integer; const V: string);
    Procedure AddFieldLabel(const Name: string);
    Function GetFieldByID(id: Integer): TFieldInfo;
    procedure KeyDown(Sender: TObject; var Key: Word; Shift: TShiftState);
    Procedure DoneAdding;
  end;

  TVIChange = function(const s: string): boolean of object;

  TValInput = class
    eb: TEdit;
    cb: TComboBox;
    olds: string;
    ONChange: TVIChange;
    changed: boolean;
    Constructor Create(aEb: TEdit);
    Constructor CreateFromCB(aCB: TComboBox);
    procedure ChangeHandler(Sender: TObject);
    Procedure SetAsInt(i: Integer);
    Function GetS: String;
    Procedure SetAsFloat(d: Double);
    Procedure SetAsColor(c: TColorF; withAlpha: Boolean = true);
    Procedure SetAsString(s: string);
    Property s: string read GetS;
    Function AsInt: Integer;
    Function AsFloat: Double;
    Function AsColor: TColorF;
  Private
    Procedure SetS(const s: string);
    Function IntChange(const s: string): boolean;
    Function FloatChange(const s: string): boolean;
    Function ColorChange(const s: string): boolean;
  end;

Function feValDouble(const s: string; var d: Double): boolean;
Function feValInt(const s: string; var i: Integer): boolean;
Function feValColor(const s: string; var c: TColorF): boolean;

implementation

uses SysUtils, Windows, Messages, Forms;

Constructor TFieldInfo.Create(aGrid: TStringGrid; arow: Integer);
begin
  Grid := aGrid;
  row := arow;
end;

Function TFieldInfo.GetS: String;
begin
  Result := Grid.Cells[1, row];
end;

Procedure TFieldInfo.SetS(const s: String);
begin
  changed := true;
  Grid.Cells[1, row] := s;
  { if Grid.Cells[1,row]=olds then }
end;

Procedure TFieldInfo.BadValueMsg;
begin
  MsgBox(s + ' is not a valid value for ' + Grid.Cells[0, row],
    'Field Editor', mb_ok);
end;

Procedure TFieldInfo.ReadString(var st: String);
begin
  st := s;
end;

Procedure TFieldInfo.ReadInteger(var i: Integer);
var
  V, a: Integer;
begin
  Val(s, V, a);
  if a = 0 then
    i := V
  else
    BadValueMsg;
end;

Procedure TFieldInfo.Read0to31(var b: Byte);
var
  V, a: Integer;
begin
  Val(s, V, a);
  if (a = 0) and (V >= -31) and (V <= 31) then
    b := V
  else
    BadValueMsg;
end;

Procedure TFieldInfo.ReadDword(var d: longint);
begin
  Try
    d := StrToDword(s);
  except
    On EConvertError do
      BadValueMsg;
  end;
end;

Procedure TFieldInfo.ReadDouble(var d: Double);
var
  a: Integer;
  V: Double;
begin
  Val(s, V, a);
  if a = 0 then
    d := V
  else
    BadValueMsg;
end;

Constructor TFieldEdit.Create(aGrid: TStringGrid);
begin
  Grid := aGrid;
  fl := TList.Create;
  Grid.ONDblClick := DblClickHandler;
  Grid.OnSelectCell := SelectHandler;
  Grid.OnSetEditText := SetTextHandler;
  Grid.OnKeyDown := KeyDown;
end;

Destructor TFieldEdit.Destroy;
begin
  Grid.ONDblClick := nil;
  Grid.OnSelectCell := nil;
  Clear;
  fl.Free;
end;

Procedure TFieldEdit.Clear;
var
  i: Integer;
begin
  Grid.ColWidths[0] := Grid.DefaultColWidth;
  Grid.ColWidths[1] := Grid.DefaultColWidth;
  For i := 0 to FieldCount - 1 do
  begin
    Grid.Cells[0, i] := '';
    Grid.Cells[1, i] := '';
    Fields[i].Free;
  end;
  fl.Clear;
  FieldCount := 0;
  oldRow := 0;
end;

Function TFieldEdit.GetField(n: Integer): TFieldInfo;
begin
  Result := TFieldInfo(fl.List[n]);
end;

Function TFieldEdit.GetFieldByID(id: Integer): TFieldInfo;
var
  i: Integer;
begin
  Result := nil;
  for i := 0 to FieldCount - 1 do
    if Fields[i].id = id then
    begin
      Result := Fields[i];
      exit;
    end;
end;

Function TFieldEdit.GetCurField: TFieldInfo;
begin
  if Grid.row <> -1 then
    Result := TFieldInfo(fl.List[Grid.row])
  else
    Result := nil;
end;

procedure TFieldEdit.SelectHandler(Sender: TObject; Col, row: longint;
  var CanSelect: boolean);
var
  Fi: TFieldInfo;
begin
  if (Col <> 1) or (row >= FieldCount) then
    CanSelect := false
  else
  begin
    Fi := Fields[row];
    if Fi = nil then
      CanSelect := false
    else if Fi.id = -1 then
      CanSelect := false
    else
      CanSelect := true;

    Fi := Fields[oldRow];
    if Fi.s <> Fi.olds then
      Changed(oldRow);
    oldRow := row;
  end;
end;

procedure TFieldEdit.DblClickHandler(Sender: TObject);
var
  Fi: TFieldInfo;
begin
  With Grid do
  begin
    if (Col <> 1) or (row >= FieldCount) then
      exit;
    Fi := Fields[row];
  end;
  if (Fi <> nil) and (Assigned(ONDblClick)) then
    ONDblClick(Fi);
end;

Function IsValid(const s: string; vt: TVarType): boolean;
var
  a: Integer;
  i: Integer;
  d: Double;
  c: TColorF;
begin
  a := -1;
  Case vt of
    vtString:
      a := 0;
    vtInteger:
      Val(s, i, a);
    vtDouble:
      Val(s, d, a);
    vtDword:
      begin
        a := 0;
        Try
          i := StrToDword(s);
        except
          On EConvertError do
            a := -1;
        end;
      end;
    vtColor:
      if not ValColor(s, c) then a := -1;
  end;
  Result := a = 0;
end;

procedure TFieldEdit.SetTextHandler(Sender: TObject; ACol, arow: longint;
  const Text: string);
var
  Fi: TFieldInfo;
  s: string;
begin
  { if (ACol<>1) or (Arow>=FieldCount) then exit;
    fi:=Fields[aRow];
    if fi<>nil then
    begin
    s:=fi.olds;
    if Assigned(OnChange) then
    if not OnChange(fi) then begin if fi.s<>s then fi.s:=s; end
    else fi.changed:=true;
    end; }

end;

Procedure TFieldEdit.DoneAdding;
begin
  Grid.Enabled := false;
  Grid.Enabled := true;
end;

function TFieldEdit.AddField(const Name: String; id: Integer; vartype: TVarType)
  : TFieldInfo;
var
  Fi: TFieldInfo;
begin
  Fi := TFieldInfo.Create(Grid, FieldCount);
  Fi.id := id;
  Fi.vtype := vartype;
  Grid.Cells[0, FieldCount] := Name;

  fl.Add(Fi);
  Inc(FieldCount);
  Grid.RowCount := FieldCount;
  Result := Fi;
end;

Procedure TFieldEdit.AddFieldHex(const Name: string; id: Integer; V: longint);
begin
  With AddField(Name, id, vtInteger) do
  begin
    olds := Format('0x%x', [V]);
    s := olds;
  end;
end;

procedure TFieldEdit.SetNameColWidth(width: Integer);
begin
        Grid.ColWidths[0] := width;
        var dw := Grid.DefaultColWidth - width;
        if dw > 0 then
          Grid.ColWidths[1] := Grid.DefaultColWidth + dw
        else
           Grid.ColWidths[1] := Grid.DefaultColWidth;
end;

Function TFieldEdit.GetNameColWidth: Integer;
begin
  Result := Grid.ColWidths[0];
end;

Procedure TFieldEdit.AddFieldInt(const Name: string; id, V: Integer);
begin
  With AddField(Name, id, vtInteger) do
  begin
    olds := IntToStr(V);
    s := olds;
  end;
end;

procedure TFieldEdit.SetDefaultColWidth(width: Integer);
begin
  Grid.DefaultColWidth := width;
end;

Function TFieldEdit.GetDefaultColWidth: Integer;
begin
  Result := Grid.DefaultColWidth;
end;

Procedure TFieldEdit.AddFieldFloat(const Name: string; id: Integer; V: Double);
begin
  With AddField(Name, id, vtInteger) do
  begin
    olds := DoubleToStr(V);
    s := olds;
  end;
end;

Procedure TFieldEdit.AddFieldColor(const Name: string; id: Integer;
  const V: TColorF; withAlpha: Boolean);
begin
  With AddField(Name, id, vtColor) do
  begin
    olds := ColorToStr(V, withAlpha);
    s := olds;
  end;
end;

Procedure TFieldEdit.AddFieldStr(const Name: string; id: Integer;
  const V: string);
begin
  With AddField(Name, id, vtString) do
  begin
    olds := V;
    s := olds;
  end;
end;

Procedure TFieldEdit.AddFieldLabel(const Name: string);
begin
  AddField(Name, -1, vtString);
end;

Procedure TFieldEdit.DeactivateHandler;
begin
  if Grid.row >= FieldCount then
    exit;
  With Fields[Grid.row] do
  begin
    if s <> olds then
      Self.changed(Grid.row);
  end;
end;

Procedure TFieldEdit.Changed(row: Integer);
var
  Fi: TFieldInfo;
begin
  if not Assigned(ONChange) then
    exit;
  if row >= FieldCount then
    exit;
  Fi := Fields[row];
  if not ONChange(Fi) then
    Fi.s := Fi.olds
  else
  begin
    Fi.changed := true;
    Fi.olds := Fi.s;
  end;
end;

Procedure TFieldEdit.CallDoneEdit;
begin
  if Assigned(OnDoneEdit) then
    OnDoneEdit;
end;

procedure TFieldEdit.KeyDown(Sender: TObject; var Key: Word;
  Shift: TShiftState);
var
  Fi: TFieldInfo;
begin
  if Shift <> [] then
    exit;
  if Grid.row >= FieldCount then
    exit;
  Fi := Fields[Grid.row];
  Case Key of
    VK_Escape:
      begin
        Fi.s := Fi.olds;
        CallDoneEdit;
      end;
    VK_Return:
      begin
        changed(Grid.row);
        CallDoneEdit;
      end;
    VK_F10:
      if (Fi <> nil) and (Assigned(ONDblClick)) then
        ONDblClick(Fi);
  end;
end;

Constructor TValInput.Create(aEb: TEdit);
begin
  eb := aEb;
  eb.ONChange := ChangeHandler;
  olds := eb.Text;
end;

Constructor TValInput.CreateFromCB(aCB: TComboBox);
begin
  cb := aCB;
  cb.ONChange := ChangeHandler;
  olds := cb.Text;
end;

Procedure TValInput.SetS(const s: string);
begin
  if eb <> nil then
  begin
    eb.ONChange := nil;
    olds := s;
    eb.Text := s;
    eb.ONChange := ChangeHandler;
    exit;
  end;
  cb.ONChange := nil;
  olds := s;
  cb.Text := s;
  cb.ONChange := ChangeHandler;
end;

procedure TValInput.ChangeHandler(Sender: TObject);
begin
  changed := true;
  if not Assigned(ONChange) then
    exit;
  if eb <> nil then
  begin
    if ONChange(eb.Text) then
      olds := eb.Text
    else
      SetS(olds);
    exit;
  end;
  if ONChange(cb.Text) then
    olds := cb.Text
  else
    SetS(olds);
end;

Procedure TValInput.SetAsInt(i: Integer);
begin
  SetS(IntToStr(i));
  ONChange := IntChange;
  changed := false;
end;

Procedure TValInput.SetAsFloat(d: Double);
begin
  SetS(DoubleToStr(d));
  ONChange := FloatChange;
  changed := false;
end;

Procedure TValInput.SetAsColor(c: TColorF; withAlpha: Boolean);
begin
  SetS(ColorToStr(c));
  ONChange := ColorChange;
  changed := false;
end;

Procedure TValInput.SetAsString(s: string);
begin
  SetS(s);
  ONChange := nil;
  changed := false;
end;

Function TValInput.IntChange(const s: string): boolean;
var
  i: Integer;
  w: string;
begin
  getWord(s, 1, w);
  Result := feValInt(w, i);
end;

Function TValInput.GetS: String;
begin
  if eb <> nil then
    Result := eb.Text
  else
    Result := cb.Text;
end;

Function TValInput.AsInt: Integer;
var
  w: string;
begin
  getWord(s, 1, w);
  if not feValInt(w, Result) then
    Result := 0;
end;

Function TValInput.AsFloat: Double;
begin
  if not feValDouble(s, Result) then
    Result := 0;
end;

Function TValInput.AsColor: TColorF;
begin
  if not feValColor(s, Result) then
    Result := MakeColor(0.0, 0.0, 0.0, 0.0);
end;

Function feValDouble(const s: string; var d: Double): boolean;
var
  st: string;
begin
  st := s;
  if s = '-' then
  begin
    Result := true;
    d := 0;
    exit;
  end;
  if (st <> '') and (st[length(st)] = '.') then
    SetLength(st, length(st) - 1);
  Result := ValDouble(st, d);
end;

Function feValInt(const s: string; var i: Integer): boolean;
var
  st: string;
begin
  st := s;
  if s = '-' then
  begin
    Result := true;
    i := 0;
    exit;
  end;
  Result := ValInt(st, i);
end;

Function feValColor(const s: string; var c: TColorF): boolean;
var
  st: string;
begin
  st := s;
  if s = '-' then
  begin
    Result := true;
    c := MakeColor(0.0, 0.0, 0.0, 0.0);
    exit;
  end;
  Result := ValColor(st, c);
end;

Function TValInput.FloatChange(const s: string): boolean;
var
  d: Double;
begin
  Result := feValDouble(s, d);
end;

Function TValInput.ColorChange(const s: string): boolean;
var
  c: TColorF;
begin
  Result := feValColor(s, c);
end;

end.
