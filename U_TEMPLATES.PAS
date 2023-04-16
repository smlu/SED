unit u_templates;

interface

uses Classes, Values, Files, FileOperations, Misc_utils, GlobalVars, Windows, sysutils;

type

  TTemplate = class
    Name, parent: String;
    vals: TTPLValues;
    desc: string;
    bbox: TThingBox;
    Constructor Create;
    Procedure Assign(const tpl: TTemplate);
    Function Clone: TTemplate;
    Destructor Destroy; override;
    Function GetAsString: string;
    Function ValsAsString: String;
  private
    Procedure ClearVals;
  end;

  TTemplates = class(TList)
    Constructor Create;
    Function GetItem(n: integer): TTemplate;
    Procedure SetItem(n: integer; v: TTemplate);
    Property Items[n: integer]: TTemplate read GetItem write SetItem; default;
    Procedure Clear;
    Procedure LoadFromFile(const fname: string);
    Procedure SaveToFile(const fname: string);
    Function AddOrUpdate(const t: TTemplate; update: Boolean = True): integer;
    Function AddFromString(const s: string): integer;
    Procedure Merge(const tlist: TTemplates);
    function GetAsString(n: integer): String;
    Function IndexOfName(const Name: string): integer;
    Function GetTPLField(const tpl, field: string): TTPLValue;
    Function GetNTPLField(ntpl: integer; const field: string): TTPLValue;
    Procedure DeleteTemplate(n: integer);
  Private
    cbox: TThingBox;
    cdesc: string;
    names: TStringList;
  end;

var
  Templates: TTemplates;

implementation

uses Forms;

Constructor TTemplate.Create;
begin
  vals := TTPLValues.Create;
end;

Procedure TTemplate.Assign(const tpl: TTemplate);
begin
  ClearVals;
  self.Name   := tpl.Name;
  self.parent := tpl.parent;
  desc        := tpl.desc;
  CopyMemory(@self.bbox, @tpl.bbox, sizeof(self.bbox));

  for var v in tpl.vals do
    vals.Add(TTplValue(v).Clone);
end;

Function TTemplate.Clone: TTemplate;
begin
  Result := TTemplate.Create;
  Result.Assign(self);
end;

Procedure TTemplate.ClearVals;
begin
  for var i := 0 to vals.Count - 1 do
    TTplValue(vals[i]).Free;
end;

Destructor TTemplate.Destroy;
begin
  ClearVals;
  vals.Free;
end;

Function TTemplate.GetAsString: string;
var
  i: integer;
begin
  Result := PadRight(Name, 17) + ' ' + PadRight(parent, 17) + ' ';
  for i := 0 to vals.Count - 1 do
    With vals[i] do
      Result := Concat(Result, ' ', Name, '=', AsString);
end;

Function TTemplate.ValsAsString: String;
var
  i: integer;
begin
  Result := '';
  for i := 0 to vals.Count - 1 do
    With vals[i] do
      Result := Concat(Result, ' ', Name, '=', AsString);
end;

Constructor TTemplates.Create;
begin
  names := TStringList.Create;
  names.sorted := true;
end;

Function TTemplates.GetItem(n: integer): TTemplate;
begin
  if (n < 0) or (n >= Count) then
    raise EListError.CreateFmt('Template Index is out of bounds: %d', [n]);
  Result := TTemplate(List[n]);
end;

Procedure TTemplates.SetItem(n: integer; v: TTemplate);
begin
  if (n < 0) or (n >= Count) then
    raise EListError.CreateFmt('Template Index is out of bounds: %d', [n]);
  List[n] := v;
end;

Procedure TTemplates.Clear;
var
  i: integer;
begin
  names.Clear;
  for i := 0 to Count - 1 do
    TTemplate(self[i]).Free;
  Inherited Clear;
end;

Function TTemplates.IndexOfName(const Name: string): integer;
var
  i: integer;
begin
  Result := -1;
  i := names.IndexOf(name);
  if i <> -1 then
    Result := integer(names.Objects[i]);
  { for i:=0 to count-1 do
    if CompareText(Items[i].Name,name)=0 then
    begin
    Result:=i;
    break;
    end; }
end;

Procedure TTemplates.DeleteTemplate(n: integer);
var
  tpl: TTemplate;
  i: integer;
begin
  tpl := GetItem(n);
  i := names.IndexOf(tpl.Name);
  if i <> -1 then
    names.Delete(i);
  Delete(n);
  tpl.Free;
end;

Function TTemplates.AddOrUpdate(const t: TTemplate; update: Boolean): integer;
begin
  Result := IndexOfName(t.Name);
  if (Result <> -1) then
    begin
      if not update then exit;
      TTemplate(self[Result]).Free;
      self[Result] := t;
    end
  else
    begin
      Result := inherited Add(t);
      names.AddObject(t.Name, TObject(Result));
    end;
end;

Function TTemplates.AddFromString(const s: string): integer;
var
  p, peq: integer;
  w: string;
  tpl: TTemplate;
  vl: TTPLValue;
begin
  Result := -1;
  p := GetWord(s, 1, w);
  if w = '' then
    exit;
  if IndexOfName(w) <> -1 then
    exit;
  tpl := TTemplate.Create;
  tpl.Name := w;

  Result := AddOrUpdate(tpl);

  //names.AddObject(w, TObject(Result));

  p := GetWord(s, p, w);
  tpl.parent := w;

  While p < length(s) do
  begin
    p := GetWord(s, p, w);
    if w = '' then
      continue;
    peq := Pos('=', w);
    if peq = 0 then
      continue;

    vl := TTPLValue.Create;
    tpl.vals.Add(vl);
    vl.Name := Copy(w, 1, peq - 1);
    vl.vtype := GetTplVType(vl.Name);
    vl.atype := GetTplType(vl.Name);

    var vs := Copy(w, peq + 1, length(w) - peq);
    if not vl.Val(vs) then
      begin
        PanMessageFmt(mt_warning,'Template ''%s'' parameter ''%s'' has invalid value: %s', [tpl.Name, vl.Name, vs]);
        // TODO: should assign string?
      end;
  end;
  tpl.bbox := cbox;
  tpl.desc := cdesc;
  FillChar(cbox, sizeof(cbox), 0);
  cdesc := '';
end;

Procedure TTemplates.Merge(const tlist: TTemplates);
begin
  for var t in tlist do
    begin
      self.AddOrUpdate(TTemplate(t).Clone, (*replace=*)true);
    end;
end;

Procedure TTemplates.LoadFromFile(const fname: string);
var
  t: TTextFile;
  s, w: string;
  f: TFile;
  i, p: integer;
begin
  try
    f := OpenFileRead(fname, 0);
  except
    on Exception do
    begin
      MsgBox('Cannot open master template - ' + fname, 'Error', mb_ok);
      //Application.Terminate; // TODO: verify if this is safe
      exit;
    end;
  end;
  t := TTextFile.CreateRead(f);
  Clear;
  Try
    While not t.eof do
    begin
      t.Readln(s);
      p := GetWord(s, 1, w);
      if w = '' then
        continue;
      if w = '#' then
      begin
        p := GetWord(s, p, w);
        if CompareText(w, 'DESC:') = 0 then
          cdesc := Trim(Copy(s, p, length(s)))
        else if CompareText(w, 'BBOX:') = 0 then
          With cbox do
            SScanf(s, '# BBOX: %l %l %l %l %l %l',
              [@x1, @y1, @z1, @x2, @y2, @z2]);
      end
      else
        AddFromString(s);
    end;
  finally
    for i := 0 to Count - 1 do
      with self[i] do
      begin
        { GetTPLField }
      end;
    t.FClose;
  end;
end;

Function TTemplates.GetAsString(n: integer): String;
var
  l, i: integer;
begin
  Result := Items[n].GetAsString;
end;

Procedure TTemplates.SaveToFile(const fname: string);
var
  t: Textfile;
  i: integer;
begin
  AssignFile(t, fname);
  Rewrite(t);
  Try
    for i := 0 to Count - 1 do
      With Items[i] do
      begin
        Writeln(t, '# DESC: ', desc);
        With bbox do
          Writeln(t, SPrintf('# BBOX: %1.6f %1.6f %1.6f %1.6f %1.6f %1.6f',
            [x1, y1, z1, x2, y2, z2]));
        Writeln(t, Self.GetAsString(i));
      end;
  finally
    CloseFile(t);
  end;
end;

Function TTemplates.GetNTPLField(ntpl: integer; const field: string): TTPLValue;
begin
end;

Function TTemplates.GetTPLField(const tpl, field: string): TTPLValue;
var
  tp: TTemplate;
  vl: TTPLValue;
  i: integer;
  ctpl: string;
  n: integer;
begin
  Result := nil;
  ctpl := tpl;
  n := 0;
  Repeat
    inc(n);
    if n >= 1000 then
      exit;
    i := IndexOfName(ctpl);
    if i = -1 then
      exit;
    tp := Items[i];
    for i := 0 to tp.vals.Count - 1 do
    begin
      vl := tp.vals[i];
      if CompareText(vl.Name, field) = 0 then
      begin
        Result := vl;
        exit;
      end;
    end;
    ctpl := tp.parent;
  until CompareText(ctpl, 'none') = 0;
end;

Initialization

begin
  InitValues;
  Templates := TTemplates.Create;
  { templates.LoadFromFile(BaseDir+'dataDir + '\' + master.tpl'); }
end;

Finalization

begin
  Templates.Clear;
  Templates.Free;
end;

end.
