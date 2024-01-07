unit Cons_checker;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  StdCtrls, ExtCtrls, J_Level, Lev_Utils, misc_utils, GlobalVars, Buttons,
  Geometry, values;

Const
  cc_Fix = 0;
  cc_NeverFix = 1;
  cc_Ask = 2;
  df_MaxSurfVerts = 24;
  ijim_MaxSurfVerts = 64;

//Const
//  et_none = 0;
//  et_sector = 1;
//  et_surface = 2;
//  et_thing = 3;
//  et_cog = 4;


type
  CCItemType = (itNone = 0, itSector = 1, itSurface = 2, itThing = 3, itCog = 4);
  CCErrorType = (etNormal = 0, etWarning = 1, etError = 2);

  TConsistencyError = class
    itype: CCItemType;
    item: TObject;
    etype: CCErrorType;
  end;

  TConsistency = class(TForm)
    Errors: TListBox;
    Panel: TPanel;
    BNClose: TButton;
    BNGoto: TButton;
    BNCheck: TButton;
    SBHelp: TSpeedButton;
    BNCheckRes: TButton;
    LBLTotal: TLabel;
    LBLTotalCnt: TLabel;
    procedure ErrorsDblClick(Sender: TObject);
    procedure BNCloseClick(Sender: TObject);
    procedure BNCheckClick(Sender: TObject);
    procedure BNGotoClick(Sender: TObject);
    procedure ErrorsClick(Sender: TObject);
    procedure BNOptionsClick(Sender: TObject);
    procedure SBHelpClick(Sender: TObject);
    procedure BNCheckResClick(Sender: TObject);
    procedure ErrorsDrawItem(Control: TWinControl; Index: Integer; Rect: TRect;
      State: TOwnerDrawState);
  private
    { Private declarations }
    Procedure ClearErrors;
    Procedure AddItem(Const Text: String; itm: TObject; et: CCErrorType);
    Procedure AddNormal(Const Text: String; itm: TObject);
    Procedure AddError(Const Text: String; itm: TObject);
    Procedure AddWarning(Const Text: String; itm: TObject);
    //Function FixError(const Text: String; itm: TObject): Boolean;
  public
    { Public declarations }
    Procedure Check;
    Procedure CheckResources;
    Function NErrors: Integer;
    Function ErrorText(n: Integer): String;
    Function ErrorObject(n: Integer): TConsistencyError;
  end;

var
  Consistency: TConsistency;

implementation

Uses U_Options, Jed_Main, Files, FileOperations, u_templates, U_CogForm;

{$R *.DFM}

procedure TConsistency.ErrorsDblClick(Sender: TObject);
begin
  BNGoto.Click;
end;

function GetBrushColor(brush:TBrush; ce: TConsistencyError; State: TOwnerDrawState): TColor;
begin
  Result := brush.Color;
  if (ce = nil) or (odSelected in State) then
    exit;
  Case ce.etype of
      etError:
        Result := clRed;
      etWarning:
        Result := clYellow;
    End;
end;

function GetFontColor(font:TFont; ce: TConsistencyError; State: TOwnerDrawState): TColor;
begin
  Result := font.Color;
  if (ce = nil) or (odSelected in State) then
    exit;
  Case ce.etype of
      etError:
        Result := clWhite;
      etWarning:
        Result := clBlack;
    End;
end;

procedure TConsistency.ErrorsDrawItem(Control: TWinControl; Index: Integer;
  Rect: TRect; State: TOwnerDrawState);
begin
  var lbox := Control as TListBox;
  with lbox.Canvas do begin
    var ce := TConsistencyError( lbox.Items.Objects[Index] );

    var bcolor_old := Brush.Color;
    Brush.Color := GetBrushColor(Brush, ce, State);
    FillRect(Rect);

    var fcolor_old := Font.Color;
    Font.Color := GetFontColor(Font, ce, State);
    TextOut(Rect.Left, Rect.Top, lbox.Items[Index]);

    Font.Color  := fcolor_old;
    Brush.Color := bcolor_old;
  end;
end;

procedure TConsistency.BNCloseClick(Sender: TObject);
begin
  Close;
end;

Procedure TConsistency.ClearErrors;
var
  i: Integer;
begin
  for i := 0 to Errors.Items.Count - 1 do
    Errors.Items.Objects[i].Free;
  Errors.Items.Clear;
end;

Procedure TConsistency.AddNormal(Const Text: String; itm: TObject);
begin
  AddItem(Text, itm, etNormal);
end;

Procedure TConsistency.AddError(Const Text: String; itm: TObject);
begin
  AddItem(Text, itm, etError);
end;

Procedure TConsistency.AddWarning(Const Text: String; itm: TObject);
begin
  AddItem(Text, itm, etWarning);
end;

Procedure TConsistency.AddItem(Const Text: String; itm: TObject; et: CCErrorType);
var
  ce: TConsistencyError;
  ni: Integer;
begin
  ce := TConsistencyError.Create;
  ce.etype := et;
  if itm = nil then
    ce.itype := itNone
  else if itm is TJKSector then
    ce.itype := itSector
  else if itm is TJKSurface then
    ce.itype := itSurface
  else if itm is TJKThing then
    ce.itype := itThing
  else if itm is TCog then
    ce.itype := itCog
  else
    exit;

  ce.item := itm;

  Case ce.itype of
    itSector:
      begin
        ni := (itm as TJKSector).num;
        Errors.AddItem(Format('%s: sector %d', [Text, ni]), ce);
      end;
    itSurface:
      With (itm as TJKSurface) do
      begin
        Errors.AddItem(Format('%s: surface %d,%d', [Text, sector.num,
          num]), ce);
      end;
    itThing:
      begin
        ni := (itm as TJKThing).num;
        //Errors.Items.AddObject(Format('%s; thing %d', [Text, ni]), ce);
        Errors.AddItem(Format('%s: thing %d', [Text, ni]), ce);
      end;
    itCog:
      begin
        ni := Level.Cogs.IndexOf(itm);
        Errors.AddItem(Format('%s: Cog %d', [Text, ni]), ce);
      end;
    itNone:
      begin
        ni := -1;
        ce.Free;
        ce := nil;
        Errors.Items.Add(Text);
      end;
  end;
end;

//Function TConsistency.FixError(const Text: String; itm: TObject): Boolean;
//begin
//  Case cc_FixErrors of
//    cc_Fix:
//      Result := true;
//    cc_NeverFix:
//      Result := false;
//    cc_Ask:
//      Result := MsgBox(Text + ' Fix?', 'Fix confirmation', MB_YesNo) = IDYes;
//  end;
//  if Result then
//    AddError(Text + '(Fixed)', itm)
//  else
//    AddError(Text, itm);
//end;

Procedure TConsistency.Check;
type
  TBoxes = array [0 .. 10000] of TBox;

var
  i, j, v, n, k: Integer;
  sec, sec1: TJKSector;
  surf: TJKSurface;
  vx: TJKVertex;
  th: TJKThing;
  d: double;
  cog: TCog;
  ext: string;
  b: Boolean;

  boxes: ^TBoxes;

begin { Check }
  { if Not Visible then } Show;
  ClearErrors;

  var maxSurfVerts := df_MaxSurfVerts;
  if level.kind = IJIM then
    maxSurfVerts := ijim_MaxSurfVerts;

  For i := 0 to Level.Sectors.Count - 1 do
  begin
    sec := Level.Sectors[i];
    if (level.kind <> IJIM) and (Trim(sec.colormap) = '') then
      AddError('No colormap set for sector!', sec);

    if sec.surfaces.Count < 4 then
    begin
      AddError('Sector has less than 4 surfaces!', sec);
      continue;
    end;
    if sec.vertices.Count < 4 then
    begin
      AddError('Sector has less than 4 vertices!', sec);
      continue;
    end;

    if (sec.flags and SECF_3DO) = 0 then
      if not IsSectorConvex(sec) then
        AddError('Sector is not convex', sec);

    for j := 0 to sec.surfaces.Count - 1 do
    begin
      { Surface consistency }
      surf := sec.surfaces[j];
      if (surf.adjoin = nil) and (surf.FaceFlags and FF_Transluent <> 0) then
        AddWarning('Incorrect face flags on solid surface', surf);

      if ((surf.SurfFlags and SF_Floor) <> 0) and ((surf.SurfFlags and SF_Collision) = 0)  then
        AddWarning('Missing collision flag on floor surface', surf);

      if surf.vertices.Count > maxSurfVerts then
        AddWarning(Format('More than %d vertices in a surface', [maxSurfVerts]), surf);
      if surf.vertices.Count < 3 then
        AddError('Less than 3 vertices in a surface!', surf);
      if not IsClose(vlen(surf.normal), 1) then
      begin
        AddWarning('Incorrect normal - surface must be invalid!', surf);
      end;
      if surf.adjoin <> nil then
      begin
        if surf.adjoin.adjoin <> surf then
          AddError('Invalid reverse adjoin', surf);
        if not Do_Surf_Overlap(surf, surf.adjoin) then
          AddWarning('Adjoined surfaces don''t overlap', surf);
      end;

      for v := 0 to surf.vertices.Count - 1 do
      begin
        vx := surf.vertices[v];
        With surf do
        begin
          d := CalcD;
          if Abs(normal.dx * vx.x + normal.dy * vx.y + normal.dz * vx.z - d) > 0.001
          then
          begin
            AddNormal('The face is not planar', surf);
            break;
          end;
        end;
      end;
      if not IsSurfConvex(surf) then
        AddNormal('The face is not convex', surf);

    end;
  end;

  if CheckOverlaps then
    try
      boxes := nil;
      GetMem(boxes, Level.Sectors.Count * sizeof(TBox));
      For i := 0 to Level.Sectors.Count - 1 do
      begin
        FindBBox(Level.Sectors[i], boxes^[i]);
      end;

      For i := 0 to Level.Sectors.Count - 1 do
      begin
        sec := Level.Sectors[i];

        for j := i - 1 downto 0 do
        begin
          sec1 := Level.Sectors[j];
          b := false;
          for k := 0 to sec.surfaces.Count - 1 do
            if sec.surfaces[k].adjoin <> nil then
              if sec.surfaces[k].adjoin.sector = sec1 then
              begin
                b := true;
                break;
              end;
          if b then
            continue;

          if not DoBoxesIntersect(boxes^[i], boxes^[j]) then
            continue;

          if DoSectorsOverlap(sec, sec1) then
          begin
            AddNormal(Format('Sectors %d and %d overlap', [i, j]), sec);
            AddNormal(Format('Sectors %d and %d overlap', [i, j]),
              Level.Sectors[j]);
          end;

        end;

      end;

    finally
      if boxes <> nil then
        FreeMem(boxes);
    end;

  for i := 0 to Level.Things.Count - 1 do
  begin
    th := Level.Things[i];
    if th.sec = nil then
    begin
      AddError('Thing not in sector', th);
      continue;
    end
    else if not IsInSector(th.sec, th.x, th.y, th.z) then
      AddError('Thing is not in specified sector', th);
  end;

  for i := 0 to Level.Cogs.Count - 1 do
  begin
    cog := Level.Cogs[i];
    For j := 0 to cog.vals.Count - 1 do
      With cog.vals[j] do
      begin
        case cog_type of
          ct_unk:
            ;
          ct_cog:
            ;
          ct_ai, ct_key, ct_mat, ct_wav, ct_3do:
            begin
              ext := LowerCase(ExtractFileExt(AsString));
              case cog_type of
                ct_ai:
                  if ext = '.ai' then
                    continue;
                ct_key:
                  if ext = '.key' then
                    continue;
                ct_mat:
                  if ext = '.mat' then
                    continue;
                ct_wav:
                  if ext = '.wav' then
                    continue;
                ct_3do:
                  if ext = '.3do' then
                    continue;
              end;
              AddWarning('Incorrect file name for parameter ' + Name, cog);
            end;
          ct_msg:
            ;
          ct_sec, ct_srf, ct_thg:
            ;
          ct_tpl:
            ;
          ct_int:
            ;
          ct_float:
            ;
          ct_vect:
            ;
        end;
      end;
  end;

  LBLTotalCnt.Caption := IntToStr(NErrors);
  if Errors.ItemIndex <= 0 then
    Errors.ItemIndex := 0
  else
    Errors.OnClick(nil);
end;

procedure TConsistency.BNCheckClick(Sender: TObject);
begin
  Check;
end;

Procedure TConsistency.CheckResources;
var
  i, j: Integer;
  sec: TJKSector;
  surf: TJKSurface;
  th: TJKThing;
  cmps, Mats, Wavs, Cogs: TStringList;
  cg: TCog;

  Function CreateList: TStringList;
  begin
    Result := TStringList.Create;
    Result.Sorted := true;
  end;

  Function NoFile(const name: string; list: TStringList): Boolean;
  var
    idx: Integer;
    f: TFile;
  begin
    idx := list.IndexOf(name);
    if idx <> -1 then
    begin
      Result := list.Objects[idx] <> nil;
      exit;
    end;

    Result := true;
    try
      f := OpenGameFile(name);
      list.AddObject(name, nil);
      Result := false;
      f.Fclose;
    except
      on Exception do
        list.AddObject(name, TObject(1));
    end;
  end;

begin
  { if Not Visible then } Show;
  ClearErrors;
  cmps := CreateList;
  Mats := CreateList;
  Wavs := CreateList;
  Cogs := CreateList;

  if (Level.kind <> IJIM) and (Level.MasterCMP <> '') then
    if NoFile(Level.MasterCMP, cmps) then
      AddError('Level''s Master CMP is not found', nil);

  try
    for i := 0 to Level.Sectors.Count - 1 do
      begin
        sec := Level.Sectors[i];
        if (Level.kind <> IJIM) and NoFile(sec.colormap, cmps) then
          AddError('Colormap not found', sec);
        if sec.sound <> '' then
          if NoFile(sec.sound, Wavs) then
            AddError('Sound not found', sec);
        for j := 0 to sec.surfaces.Count - 1 do
        begin
          surf := sec.surfaces[j];
          if surf.Material <> '' then
            if NoFile(surf.Material, Mats) then
              AddError('Material not found', surf);
        end;
      end;

    For i := 0 to Level.Things.Count - 1 do
    begin
      th := Level.Things[i];
      if Templates.IndexOfName(th.name) = -1 then
        AddError('Template not found', th);
    end;

    For i := 0 to Level.Cogs.Count - 1 do
    begin
      cg := Level.Cogs[i];
      if NoFile(cg.name, Cogs) then
        AddError('Cog not found', cg);
    end;

    if CurrentProject = MOTS then
    begin
      if cmps.Count > 1 then
        AddWarning('Warning! More than 1 CMP used in the level', nil);
    end
    else if CurrentProject = JKDF2 then
    begin
      if cmps.Count > 3 then
        AddWarning('Warning! More than 3 CMP used in the level', nil);
    end;
  finally
    cmps.Free;
    Mats.Free;
    Wavs.Free;
    Cogs.Free;

    LBLTotalCnt.Caption := IntToStr(NErrors);
    if Errors.ItemIndex <= 0 then
      Errors.ItemIndex := 0
    else
      Errors.OnClick(nil);
    { if Not Visible then } Show;
  end;
end;

procedure TConsistency.BNCheckResClick(Sender: TObject);
begin
  CheckResources;
end;

procedure TConsistency.BNGotoClick(Sender: TObject);
var
  Index: Integer;
  ce: TConsistencyError;
  i, sc, sf, th: Integer;
begin
  Index := Errors.ItemIndex;
  if Index < 0 then
    exit;
  ce := TConsistencyError(Errors.Items.Objects[Index]);
  if ce = nil then
    exit;
  Case ce.itype of
    itSector:
      begin
        sc := Level.Sectors.IndexOf(ce.item);
        if sc = -1 then
        begin
          MsgBox('Sector not found', 'Error', mb_OK);
          exit;
        end;
        JedMain.GotoSC(sc);
      end;
    itSurface:
      begin
        for i := 0 to Level.Sectors.Count - 1 do
          With Level.Sectors[i] do
          begin
            sc := i;
            sf := surfaces.IndexOf(ce.item);
            if sf <> -1 then
              break;
          end;
        if sf = -1 then
        begin
          MsgBox('Surface not found', 'Error', mb_OK);
          exit;
        end;
        JedMain.GotoSF(sc, sf);
      end;
    itThing:
      begin
        th := Level.Things.IndexOf(ce.item);
        if th = -1 then
        begin
          MsgBox('Thing not found', 'Error', mb_OK);
          exit;
        end;
        JedMain.GotoTH(th);
      end;
    itCog:
      begin
        th := Level.Cogs.IndexOf(ce.item);
        if th = -1 then
        begin
          MsgBox('Cog not found', 'Error', mb_OK);
          exit;
        end;
        CogForm.GotoCog(th);
      end;
  end;
end;

procedure TConsistency.ErrorsClick(Sender: TObject);
var
  Index: Integer;
  ce: TConsistencyError;
begin
  Index := Errors.ItemIndex;
  if Index = -1 then
  begin
    BNGoto.Enabled := false;
    exit;
  end;
  ce := TConsistencyError(Errors.Items.Objects[Index]);
  if ce = nil then
    BNGoto.Enabled := false
  else
    BNGoto.Enabled := true;
end;

procedure TConsistency.BNOptionsClick(Sender: TObject);
begin
  { Options.SetOptions(Options.Misc); }
end;

procedure TConsistency.SBHelpClick(Sender: TObject);
begin
  //Application.HelpFile := basedir + 'jedhelp.hlp';
  //Application.HelpContext(440);
end;

Function TConsistency.NErrors: Integer;
begin
  Result := Errors.Items.Count;
end;

Function TConsistency.ErrorText(n: Integer): String;
begin
  Result := Errors.Items[n];
end;

Function TConsistency.ErrorObject(n: Integer): TConsistencyError;
begin
  Result := TConsistencyError(Errors.Items.Objects[n]);
end;

end.
