unit Item_edit;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  ExtCtrls, Grids, J_Level, FieldEdit, Geometry, misc_utils, StdCtrls, Clipbrd,
  Values, u_multisel, u_undo;

Const
  ID_FLAGS = 0;
  ID_AMBIENT = 1;
  ID_EXTRA_L = 2;
  ID_COLORMAP = 3;
  ID_TINT = 4;
  ID_Sound = 5;
  ID_Snd_vol = 6;

  ID_AdjFlags = 0;
  ID_Material = 1;
  ID_SurfFlags = 2;
  ID_FaceFlags = 3;
  ID_geo = 4;
  ID_lightmode = 5;
  ID_tex = 6;
  ID_SF_extra = 7;
  ID_Adjoin = 8;
  ID_UScale = 9;
  ID_VScale = 10;
  ID_Layer = 15;

  ID_Sec = 0;
  ID_name = 1;
  ID_X = 2;
  ID_Y = 3;
  ID_Z = 4;
  ID_PCH = 5;
  ID_YAW = 6;
  ID_ROL = 7;
  ID_TH_Last = 20;

  ID_INTENSITY = 0;
  ID_RANGE = 1;
  ID_RGBRange = 10;
  ID_RGBIntensity = 11;
  ID_RGB = 12;
  ID_LFlags = 14;

  Thing_fields = 9;

  ID_EDGELEN = 2;

type
  TItemEdit = class(TForm)
    SGFields: TStringGrid;
    PNButtons: TPanel;
    Panel2: TPanel;
    ColorDlg: TColorDialog;
    BNAdd: TButton;
    BNRemove: TButton;
    BNAsFrame: TButton;
    CBOnTop: TCheckBox;
    LBCogs: TListBox;
    Panel3: TPanel;
    LBText: TLabel;
    BNPaste: TButton;
    procedure FormCreate(Sender: TObject);
    procedure FormActivate(Sender: TObject);
    procedure BNAddClick(Sender: TObject);
    procedure BNRemoveClick(Sender: TObject);
    procedure BNAsFrameClick(Sender: TObject);
    procedure FormDestroy(Sender: TObject);
    procedure FormDeactivate(Sender: TObject);
    procedure CBOnTopClick(Sender: TObject);
    procedure LBCogsDblClick(Sender: TObject);
    procedure BNPasteClick(Sender: TObject);
  private
    citemtype: integer;
    fe: TFieldEdit;
    Cur_Sec: TJKSector;
    Cur_Surf: TJKSurface;
    Cur_VX: TJKVertex;
    Cur_Thing: TJKThing;
    Cur_Light: TJedLight;
    Cur_Frame: TTPlValue;
    cedge: integer;
    Procedure ResetEdit;
    Procedure WMNCDblClck(var msg: TMessage); message WM_NCLBUTTONDBLCLK;
  public
    { Public declarations }
    Procedure LoadSector(sec: TJKSector);
    Procedure LoadSurface(surf: TJKSurface);
    Procedure LoadThing(thing: TJKThing);
    Procedure LoadFrame(th, fr: integer);
    Procedure LoadVertex(vx: TJKVertex);

    Procedure LoadExtra(ex: integer);
    Function ExtraChange(Fi: TFieldInfo): boolean;

    Procedure DblClk(Fi: TFieldInfo);
    Function SectorChange(Fi: TFieldInfo): boolean;
    Procedure SectorDblClk(Fi: TFieldInfo);
    Function SurfaceChange(Fi: TFieldInfo): boolean;
    Procedure SurfaceDblClk(Fi: TFieldInfo);
    Function ThingChange(Fi: TFieldInfo): boolean;
    Function FrameChange(Fi: TFieldInfo): boolean;
    Function VertexChange(Fi: TFieldInfo): boolean;
    Procedure ThingDblClk(Fi: TFieldInfo);
    Procedure LightDblClk(Fi: TFieldInfo);
    Function LightChange(Fi: TFieldInfo): boolean;
    Procedure LoadLight(nLight: integer);
    Procedure LoadEdge(surf: TJKSurface; edge: integer);
    Function EdgeChange(Fi: TFieldInfo): boolean;
    Procedure DoPickTexture;
    Procedure DoPickThing;
    Procedure AddCogs(ct: TCog_Type; lobj: TObject);
    Procedure SmartChangeFocus;
  end;

var
  ItemEdit: TItemEdit;

implementation

uses ResourcePicker, FlagEditor, Jed_Main, GlobalVars, U_Templates, U_tbar,
  U_SCFEdit, U_CogForm;

{$R *.DFM}

procedure TItemEdit.FormCreate(Sender: TObject);
begin
  fe := TFieldEdit.Create(SGFields);
  SetWinPos(self, IEditPos);
  CBOnTop.Checked := IEOnTop;
  fe.OnDoneEdit := SmartChangeFocus;
end;

Procedure TItemEdit.DblClk(Fi: TFieldInfo);
begin
  ResPicker.PickThing('');
end;

Procedure TItemEdit.SmartChangeFocus;
var
  p: TPoint;
begin
  GetCursorPos(p);
  if (p.x < Left) or (p.x > width + Left) or (p.y < Top) or (p.y > height + Top)
  then
    JedMain.SetFocus;
end;

Procedure TItemEdit.LoadSector(sec: TJKSector);
var
  i: integer;
begin
  Cur_Sec := sec;
  Caption := Format('Sector %d', [sec.Num]);
  ResetEdit;

  LBText.Caption := Format('%d Surfaces %d Vertices',
    [sec.surfaces.count, sec.vertices.count]);
  With sec do
  begin
    fe.AddFieldHex('+FLAGS', ID_FLAGS, Flags);
    { fe.AddFieldFloat('AMBIENT',ID_AMBIENT,Ambient); }
    fe.AddFieldFloat('EXTRA LIGHT', ID_EXTRA_L, Extra);
    fe.AddFieldStr('+COLORMAP', ID_COLORMAP, Colormap);
    fe.AddFieldStr('+TINT', ID_TINT, Sprintf('%.3f %.3f %.3f',
      [Tint.DX, Tint.Dy, Tint.Dz]));
    fe.AddFieldStr('+SOUND', ID_Sound, Sound);
    fe.AddFieldFloat('VOLUME', ID_Snd_vol, Snd_Vol);
    fe.AddFieldStr('+LAYER', ID_Layer, Level.GetLayerName(Layer));
  end;
  if not Visible then
    Show;
  fe.DoneAdding;
  AddCogs(ct_sec, Cur_Sec);
  fe.OnDblClick := SectorDblClk;
  fe.ONChange := SectorChange;
end;

Function ValSecField(const s: string; sec: TJKSector; id: integer): boolean;
var
  f: longint;
  d: double;
  Tint: TVector;
  i: integer;
begin
  Result := false;
  if sec = nil then
    exit;

  SaveSecUndo(sec, ch_changed, sc_val);

  case id of
    ID_FLAGS:
      begin
        Result := ValHex(s, f);
        if Result then
          sec.Flags := f;
      end;
    { ID_AMBIENT:begin
      Result:=ValDouble(fi.s,d);
      if Result then Cur_sec.ambient:=d;
      exit;
      end; }
    ID_EXTRA_L:
      begin
        Result := ValDouble(s, d);
        if Result then
          sec.Extra := d;
      end;
    ID_COLORMAP:
      Begin
        Result := true;
        sec.Colormap := s;
      end;
    ID_TINT:
      begin
        With Tint do
          Result := SScanf(s, '%f %f %f', [@DX, @Dy, @Dz]);
        if Result then
          sec.Tint := Tint;
        exit;
      end;
    ID_Sound:
      begin
        Result := true;
        sec.Sound := s;
        exit;
      end;
    ID_Snd_vol:
      begin
        Result := ValDouble(s, d);
        if Result then
          sec.Snd_Vol := d;
        exit;
      end;
    ID_Layer:
      begin
        Result := true;
        sec.Layer := Toolbar.AddLayer(s);
        exit;
      end;
  end;
  JedMain.SectorChanged(sec);
end;

Function TItemEdit.SectorChange(Fi: TFieldInfo): boolean;
var
  i: integer;
  scsl: TSCMultiSel;
begin
  StartUndoRec('Change Sector(s)');
  { JedMain.SaveSelSecUndo('Change sector(s)',ch_changed,sc_val); }

  Result := ValSecField(Fi.s, Cur_Sec, Fi.id);
  if not Result then
    exit;
  scsl := JedMain.scsel;
  for i := 0 to scsl.count - 1 do
    ValSecField(Fi.s, Level.Sectors[scsl.getSC(i)], Fi.id);
end;

Procedure TItemEdit.SectorDblClk(Fi: TFieldInfo);
var
  f: longint;
  Tint: TVector;
begin
  Case Fi.id of
    ID_FLAGS:
      begin
        ValHex(Fi.s, f);
        f := FlagEdit.EditSectorFlags(f);
        Fi.s := Format('%x', [f]);
      end;
    ID_AMBIENT:
      ;
    ID_EXTRA_L:
      ;
    ID_COLORMAP:
      Fi.s := ResPicker.PickCMP(Fi.s);
    ID_Sound:
      Fi.s := ResPicker.PickSecSound(Fi.s);
    ID_TINT:
      begin
        With Tint do
          SScanf(Fi.s, '%f %f %f', [@DX, @Dy, @Dz]);
        With Tint do
          ColorDlg.Color := RGB(Round(DX * 255), Round(Dy * 255),
            Round(Dz * 255));
        if ColorDlg.Execute then
        begin
          f := ColorDlg.Color;
          Fi.s := Sprintf('%.3f %.3f %.3f',
            [GetRValue(f) / 255, GetGValue(f) / 255, GetBValue(f) / 255]);
        end;
      end;
    ID_Layer:
      Fi.s := ResPicker.PickLayer(Fi.s);
  end;
end;

Procedure TItemEdit.LoadSurface(surf: TJKSurface);
var
  ast: string;
begin
  Caption := Format('Sector %d Surface %d', [surf.Sector.Num, surf.Num]);
  ResetEdit;
  LBText.Caption := Format('%d Vertices', [surf.vertices.count]);

  Cur_Surf := surf;

  With surf do
  begin
    fe.AddFieldStr('+MATERIAL', ID_Material, Material);
    fe.AddFieldFloat('U scale', ID_UScale, uscale);
    fe.AddFieldFloat('V scale', ID_VScale, vscale);
    fe.AddFieldInt('+GEO', ID_geo, geo);
    fe.AddFieldInt('+TEX', ID_tex, tex);
    fe.AddFieldInt('+LIGHT MODE', ID_lightmode, Light);
    if Adjoin = nil then
      ast := '-1'
    else
      ast := Format('%d %d', [Adjoin.Sector.Num, Adjoin.Num]);
    fe.AddFieldStr('ADJOIN', ID_Adjoin, ast);
    fe.AddFieldHex('+ADJOIN FLAGS', ID_AdjFlags, AdjoinFlags);
    fe.AddFieldHex('+SURF FLAGS', ID_SurfFlags, SurfFlags);
    fe.AddFieldHex('+FACE FLAGS', ID_FaceFlags, FaceFlags);
    { fe.AddFieldInt('LIGHT',ID_light,Light); }
    fe.AddFieldFloat('EXTRA LIGHT', ID_SF_extra, extraLight);
  end;
  if not Visible then
    Show;
  fe.DoneAdding;
  AddCogs(ct_srf, Cur_Surf);
  fe.OnDblClick := SurfaceDblClk;
  fe.ONChange := SurfaceChange;
end;

Function ValSurfField(const s: string; surf: TJKSurface; id: integer): boolean;
var
  sc, sf, i: integer;
  w: string;
  f: longint;
  d: double;
  sl: single;
begin
  Result := false;
  if surf = nil then
    exit;

  SaveSecUndo(surf.Sector, ch_changed, sc_val);

  With surf do
    case id of
      ID_Material:
        begin
          Result := true;
          Material := s;
        end;
      ID_geo:
        begin
          Result := ValInt(s, i);
          if Result then
            geo := i;
        end;
      ID_tex:
        begin
          Result := ValInt(s, i);
          if Result then
            tex := i;
        end;
      ID_lightmode:
        begin
          Result := ValInt(s, i);
          if Result then
            Light := i;
        end;
      ID_AdjFlags:
        begin
          Result := ValHex(s, f);
          if Result then
            AdjoinFlags := f;
        end;
      ID_SurfFlags:
        begin
          Result := ValHex(s, f);
          i := Ord((f and SFF_Flip) <> (SurfFlags and SFF_Flip));

          if Result then
          begin
            SurfFlags := f;
            if i > 0 then
              RecalcAll;
          end;

        end;
      ID_FaceFlags:
        begin
          Result := ValHex(s, f);
          if Result then
            FaceFlags := f;
        end;
      ID_SF_extra:
        begin
          Result := ValDouble(s, d);
          if Result then
            extraLight := d;
        end;
      ID_UScale:
        begin
          Result := ValSingle(s, sl);
          if Result then
          begin
            uscale := sl;
            RecalcAll;
          end
        end;
      ID_VScale:
        begin
          Result := ValSingle(s, sl);
          if Result then
          begin
            vscale := sl;
            RecalcAll;
          end
        end;
      ID_Adjoin:
        begin
          i := GetWord(s, 1, w);
          if w = '' then
            exit;
          if not ValInt(w, sc) then
            exit;
          if sc = -1 then
          begin
            Adjoin := nil;
            Result := true;
            exit;
          end;
          i := GetWord(s, i, w);
          if w = '' then
            exit;
          if not ValInt(w, sf) then
            exit;
          if (sc < 0) or (sc >= Level.Sectors.count) then
            exit;
          With Level.Sectors[sc] do
          begin
            if (sf < 0) or (sf >= surfaces.count) then
              exit;
            Adjoin := surfaces[sf];
            Result := true;
          end;
        end;
    end;
  JedMain.SectorChanged(surf.Sector);
end;

Function TItemEdit.SurfaceChange(Fi: TFieldInfo): boolean;
var
  i: integer;
  sfsl: TSFMultiSel;
  sc, sf: integer;
begin
  { JedMain.SaveSelSurfUndo('Change surface(s)',ch_changed,sc_val); }
  StartUndoRec('Change surface(s)');

  Result := ValSurfField(Fi.s, Cur_Surf, Fi.id);
  if not Result then
    exit;
  sfsl := JedMain.sfsel;
  for i := 0 to sfsl.count - 1 do
  begin
    sfsl.GetSCSF(i, sc, sf);
    ValSurfField(Fi.s, Level.Sectors[sc].surfaces[sf], Fi.id);
  end;
end;

Procedure TItemEdit.SurfaceDblClk(Fi: TFieldInfo);
var
  f: longint;
begin
  case Fi.id of
    ID_Material:
      begin
        if Cur_Surf = nil then
          exit;
        ResPicker.SetCMP(Level.GetMasterCMP);
        { if Cur_surf.sector.colormap<>'' then
          ResPicker.SetCMP(Level.sectors[cur_surf.sector.num].COlormap); }
        Fi.s := ResPicker.PickMAT(Fi.s);
      end;
    ID_geo:
      begin
        ValDword(Fi.s, f);
        f := SCFieldPicker.PickGeo(f);
        Fi.s := IntToStr(f);
      end;
    ID_tex:
      begin
        ValDword(Fi.s, f);
        f := SCFieldPicker.PickTex(f);
        Fi.s := IntToStr(f);
      end;
    ID_lightmode:
      begin
        ValDword(Fi.s, f);
        f := SCFieldPicker.PickLightMode(f);
        Fi.s := IntToStr(f);
      end;
    ID_AdjFlags:
      begin
        ValHex(Fi.s, f);
        f := FlagEdit.EditAdjoinFlags(f);
        Fi.s := Format('%x', [f]);
      end;
    ID_SurfFlags:
      begin
        ValHex(Fi.s, f);
        f := FlagEdit.EditSurfaceFlags(f);
        Fi.s := Format('%x', [f]);
      end;
    ID_FaceFlags:
      begin
        ValHex(Fi.s, f);
        f := FlagEdit.EditFaceFlags(f);
        Fi.s := Format('%x', [f]);
      end;
    { ID_SF_extra: extraLight }

  end;
end;

Procedure TItemEdit.LoadFrame(th, fr: integer);
var
  i, n: integer;
  x, y, z, pch, yaw, rol: double;
  fm: TForm;
begin
  ResetEdit;

  if fr < 0 then
  begin
    Caption := Format('Thing %d', [th]);
    fe.AddFieldInt('numframes', -1, Level.Things[th].nframes);
    fe.OnDblClick := nil;
    fe.ONChange := nil;
    exit;
  end;

  PNButtons.Visible := true;
  BNAsFrame.Visible := true;
  BNPaste.Visible := true;

  n := 0;
  With Level.Things[th] do
    For i := 0 to Vals.count - 1 do
    begin
      if i >= fr then
        break;
      if Vals[i].atype = at_frame then
        inc(n);
    end;

  Caption := Format('Thing %d Frame %d', [th, n]);

  Cur_Frame := Level.Things[th].Vals[fr];
  Cur_Frame.GetFrame(x, y, z, pch, yaw, rol);

  fe.AddFieldFloat('X', ID_X, x);
  fe.AddFieldFloat('Y', ID_Y, y);
  fe.AddFieldFloat('Z', ID_Z, z);
  fe.AddFieldFloat('PCH', ID_PCH, pch);
  fe.AddFieldFloat('YAW', ID_YAW, yaw);
  fe.AddFieldFloat('ROL', ID_ROL, rol);
  fe.DoneAdding;

  if not Visible then
  begin
    fm := Screen.ActiveForm;
    Show;
    fm.SetFocus;
  end;

  fe.OnDblClick := nil;
  fe.ONChange := FrameChange;
end;

Procedure TItemEdit.LoadThing(thing: TJKThing);
var
  i: integer;
begin
  Caption := Format('Thing %d', [thing.Num]);
  ResetEdit;
  BNAdd.Visible := true;
  BNRemove.Visible := true;
  BNAsFrame.Visible := true;
  BNPaste.Visible := true;
  PNButtons.Visible := true;

  Cur_Thing := thing;
  With thing do
  begin

    fe.AddFieldStr('+NAME', ID_name, Name);
    if sec = nil then
      fe.AddFieldInt('SECTOR', ID_Sec, -1)
    else
      fe.AddFieldInt('SECTOR', ID_Sec, sec.Num);
    fe.AddFieldFloat('X', ID_X, x);
    fe.AddFieldFloat('Y', ID_Y, y);
    fe.AddFieldFloat('Z', ID_Z, z);
    fe.AddFieldFloat('PCH', ID_PCH, pch);
    fe.AddFieldFloat('YAW', ID_YAW, yaw);
    fe.AddFieldFloat('ROL', ID_ROL, rol);
    fe.AddFieldStr('+LAYER', ID_Layer, Level.GetLayerName(Layer));

    for i := 0 to Vals.count - 1 do
      With Vals[i] do
        if CompareText(Name, 'thingflags') = 0 then
          fe.AddFieldStr('+' + Name, ID_TH_Last + i, AsString)
        else
          fe.AddFieldStr(Name, ID_TH_Last + i, AsString);
  end;
  fe.DoneAdding;
  AddCogs(ct_thg, Cur_Thing);
  if not Visible then
    Show;
  fe.OnDblClick := ThingDblClk;
  fe.ONChange := ThingChange;
end;

Function ValTHValue(const s: string; Cur_Thing: TJKThing; const fname: string;
  n: integer): boolean;
var
  cn, i: integer;
  v: TTPlValue;
begin
  Result := false;
  cn := 0;
  v := nil;

  for i := 0 to Cur_Thing.Vals.count - 1 do
  begin
    v := Cur_Thing.Vals[i];
    if CompareText(v.name, fname) <> 0 then
    begin
      v := nil;
      continue;
    end;
    if cn = n then
      break;
    inc(cn);
    v := nil;
  end;

  if v = nil then
    exit;

  SaveThingUndo(Cur_Thing, ch_changed);

  Result := v.Val(s);

  JedMain.ThingChanged(Cur_Thing);
end;

Function ValTHField(const s: string; Cur_Thing: TJKThing; id: word): boolean;
var
  i: integer;
  f: longint;
  d: double;
begin
  Result := false;
  if Cur_Thing = nil then
    exit;

  SaveThingUndo(Cur_Thing, ch_changed);

  i := id;

  With Cur_Thing do
    case i of
      ID_name:
        begin
          Result := true;
          Name := s;
        end;
      ID_Sec:
        begin
          Result := ValInt(s, i);
          if not Result then
            exit;
          if i = -1 then
          begin
            sec := nil;
            exit;
          end;
          if (i < 0) or (i >= Level.Sectors.count) then
          begin
            Result := false;
            exit;
          end;
          sec := Level.Sectors[i];
        end;
      ID_X:
        begin
          if not ValDouble(s, d) then
            exit;
          x := d;
        end;
      ID_Y:
        begin
          if not ValDouble(s, d) then
            exit;
          y := d;
        end;
      ID_Z:
        begin
          if not ValDouble(s, d) then
            exit;
          z := d;
        end;
      ID_PCH:
        begin
          if not ValDouble(s, d) then
            exit;
          pch := d;
        end;
      ID_YAW:
        begin
          if not ValDouble(s, d) then
            exit;
          yaw := d;
        end;
      ID_ROL:
        begin
          if not ValDouble(s, d) then
            exit;
          rol := d;
        end;
      ID_Layer:
        Layer := Toolbar.AddLayer(s);
      { ID_TH_Last..ID_TH_Last+100:
        begin
        i:=Id-ID_TH_Last;
        if i>=Vals.Count then exit;
        Vals[i].Val(s);
        Result:=true;
        end; }

    end;
  JedMain.ThingChanged(Cur_Thing);
  JedMain.Invalidate;
  Result := true;
end;

Function TItemEdit.ThingChange(Fi: TFieldInfo): boolean;
var
  i: integer;
  thsl: TTHMultiSel;
  fname: string;
  n: integer;
begin
  StartUndoRec('Change thing(s)');
  { JedMain.SaveSelThingsUndo('Change thing(s)',ch_changed); }

  if (Fi.id < ID_TH_Last) then
  begin

    Result := ValTHField(Fi.s, Cur_Thing, Fi.id);

    if not Result then
      exit;
    thsl := JedMain.thsel;
    for i := 0 to thsl.count - 1 do
    begin
      ValTHField(Fi.s, Level.Things[thsl.GetTH(i)], Fi.id);
    end;

  end
  else { thing value }
  begin
    { Find which number of the value it is. I.e. - for multiple values
      with the same name, like "frame" }
    fname := Cur_Thing.Vals[Fi.id - ID_TH_Last].name;
    n := 0;
    for i := Fi.id - ID_TH_Last - 1 downto 0 do
    begin
      if CompareText(Cur_Thing.Vals[i].name, fname) = 0 then
        inc(n);
    end;

    Result := ValTHValue(Fi.s, Cur_Thing, fname, n);
    if not Result then
      exit;

    thsl := JedMain.thsel;
    for i := 0 to thsl.count - 1 do
    begin
      ValTHValue(Fi.s, Level.Things[thsl.GetTH(i)], fname, n);
    end;
  end;

end;

Procedure TItemEdit.ThingDblClk(Fi: TFieldInfo);
var
  i: integer;
  f: longint;
begin
  if Cur_Thing = nil then
    exit;
  With Cur_Thing do
    case Fi.id of
      ID_name:
        Fi.s := ResPicker.PickThing(Fi.s);
      ID_TH_Last .. ID_TH_Last + 100:
        begin
          i := Fi.id - ID_TH_Last;
          if i >= Vals.count then
            exit;
          if CompareText(Vals[i].name, 'thingflags') <> 0 then
            exit;
          ValHex(Fi.s, f);
          f := FlagEdit.EditThingFlags(f);
          Fi.s := Format('0x%x', [f]);
        end;
      ID_Layer:
        Fi.s := ResPicker.PickLayer(Fi.s);
    end;
end;

Function ValFRField(const s: string; Cur_Frame: TTPlValue; id: integer)
  : boolean;
var
  i: integer;
  f: longint;
  d: double;
  x, y, z, pch, yaw, rol: double;
begin
  Result := false;
  if Cur_Frame = nil then
    exit;
  Cur_Frame.GetFrame(x, y, z, pch, yaw, rol);
  case id of
    ID_X:
      if ValDouble(s, d) then
        x := d
      else
        exit;
    ID_Y:
      if ValDouble(s, d) then
        y := d
      else
        exit;
    ID_Z:
      if ValDouble(s, d) then
        z := d
      else
        exit;
    ID_PCH:
      if ValDouble(s, d) then
        pch := d
      else
        exit;
    ID_YAW:
      if ValDouble(s, d) then
        yaw := d
      else
        exit;
    ID_ROL:
      if ValDouble(s, d) then
        rol := d
      else
        exit;
  else
    exit;
  end;
  Cur_Frame.SetFrame(x, y, z, pch, yaw, rol);
  JedMain.Invalidate;
  Result := true;
end;

Function TItemEdit.FrameChange(Fi: TFieldInfo): boolean;
var
  i: integer;
  frsl: TFRMultiSel;
  fr, th: integer;
begin
  Result := ValFRField(Fi.s, Cur_Frame, Fi.id);
  if not Result then
    exit;
  frsl := JedMain.frsel;
  for i := 0 to frsl.count - 1 do
  begin
    frsl.GetTHFR(i, th, fr);
    if fr = -1 then
      continue;
    ValFRField(Fi.s, Level.Things[th].Vals[fr], Fi.id);
  end;
end;

Procedure TItemEdit.LoadVertex(vx: TJKVertex);
var
  i: integer;
begin
  Caption := Format('Sector %d vertex %d', [vx.Sector.Num, vx.Num]);
  ResetEdit;

  Cur_VX := vx;
  With vx do
  begin
    fe.AddFieldFloat('X', ID_X, x);
    fe.AddFieldFloat('Y', ID_Y, y);
    fe.AddFieldFloat('Z', ID_Z, z);
  end;
  if not Visible then
    Show;
  fe.DoneAdding;
  fe.OnDblClick := nil;
  fe.ONChange := VertexChange;
end;

Procedure TItemEdit.LoadExtra(ex: integer);
var
  o: TObject;
begin
  Caption := Format(JedMain.ExtraObjsName + ' %d', [ex]);
  ResetEdit;
  fe.ONChange := nil;

  o := JedMain.ExtraObjs[ex];
  if o is TExtraVertex then
  begin
    fe.AddFieldStr('Name', ID_name, TExtraVertex(o).name);
    fe.AddFieldFloat('X', ID_X, TExtraVertex(o).x);
    fe.AddFieldFloat('Y', ID_Y, TExtraVertex(o).y);
    fe.AddFieldFloat('Z', ID_Y, TExtraVertex(o).z);
    fe.ONChange := ExtraChange;
  end;

  if o is TExtraLine then
    fe.AddFieldStr('Name', ID_name, TExtraLine(o).name);
  if o is TExtraPolygon then
    fe.AddFieldStr('Name', ID_name, TExtraPolygon(o).name);

  if not Visible then
    Show;
  fe.DoneAdding;
  fe.OnDblClick := nil;
end;

Function ValExtraVertex(const s: string; Cur_VX: TExtraVertex;
  id: integer): boolean;
var
  d: double;
  i: integer;
begin
  Result := false;
  if Cur_VX = nil then
    exit;

  With Cur_VX do
    case id of
      ID_X:
        begin
          if not ValDouble(s, d) then
            exit;
          x := d;
        end;
      ID_Y:
        begin
          if not ValDouble(s, d) then
            exit;
          y := d;
        end;
      ID_Z:
        begin
          if not ValDouble(s, d) then
            exit;
          z := d;
        end;
    end;

  Result := true;
end;

Function TItemEdit.ExtraChange(Fi: TFieldInfo): boolean;
var
  obj: TObject;
  ex: TExtraVertex;
begin
  Result := false;
  obj := JedMain.ExtraObjs[JedMain.Cur_EX];
  if not(obj is TExtraVertex) then
    exit;
  ex := TExtraVertex(obj);
  Result := ValExtraVertex(Fi.s, ex, Fi.id);
  if Result then
    if Assigned(JedMain.OnExtraMove) then
      JedMain.OnExtraMove(ex, false);
end;

Function ValVertField(const s: string; Cur_VX: TJKVertex; id: integer): boolean;
var
  d: double;
  i: integer;
begin
  Result := false;
  if Cur_VX = nil then
    exit;
  SaveSecUndo(Cur_VX.Sector, ch_changed, sc_geo);

  With Cur_VX do
    case id of
      ID_X:
        begin
          if not ValDouble(s, d) then
            exit;
          x := d;
        end;
      ID_Y:
        begin
          if not ValDouble(s, d) then
            exit;
          y := d;
        end;
      ID_Z:
        begin
          if not ValDouble(s, d) then
            exit;
          z := d;
        end;
    end;
  Result := true;
  for i := 0 to Cur_VX.Sector.surfaces.count - 1 do
    Cur_VX.Sector.surfaces[i].RecalcAll;
  JedMain.SectorChanged(Cur_VX.Sector);
end;

Function TItemEdit.VertexChange(Fi: TFieldInfo): boolean;
var
  i: integer;
  vxsl: TVXMultiSel;
  sc, vx: integer;
begin
  { JedMain.SaveSelVertUndo('Change vertices',ch_changed); }

  StartUndoRec('Change vertices');

  Result := ValVertField(Fi.s, Cur_VX, Fi.id);
  if not Result then
    exit;
  vxsl := JedMain.vxsel;
  for i := 0 to vxsl.count - 1 do
  begin
    vxsl.GetSCVX(i, sc, vx);
    ValVertField(Fi.s, Level.Sectors[sc].vertices[vx], Fi.id);
  end;
end;

Procedure TItemEdit.LoadLight(nLight: integer);
begin
  Caption := Format('Light %d', [nLight]);
  ResetEdit;
  if nLight >= Level.Lights.count then
    exit;
  Cur_Light := Level.Lights[nLight];
  With Cur_Light do
  begin

    fe.AddFieldFloat('Intensity', ID_INTENSITY, Intensity);
    fe.AddFieldFloat('Range', ID_RANGE, Range);
    fe.AddFieldFloat('X', ID_X, x);
    fe.AddFieldFloat('Y', ID_Y, y);
    fe.AddFieldFloat('Z', ID_Z, z);
    fe.AddFieldHex('+Flags', ID_LFlags, Flags);

    if IsMots then
    begin
      fe.AddFieldStr('+RGB', ID_RGB, Sprintf('%.3f %.3f %.3f', [r, g, b]));
      { fe.AddFieldFloat('RGBRange',ID_RGBRANGE,RGBRange); }
      fe.AddFieldFloat('RGBIntensity', ID_RGBIntensity, RGBIntensity);
    end;
    fe.AddFieldStr('+LAYER', ID_Layer, Level.GetLayerName(Layer));

  end;
  if not Visible then
    Show;
  fe.DoneAdding;
  fe.OnDblClick := LightDblClk;
  fe.ONChange := LightChange;
end;

Procedure TItemEdit.LightDblClk(Fi: TFieldInfo);
var
  r, g, b: single;
  f: TColor;
  fl: longint;
begin
  if Cur_Light = nil then
    exit;
  With Cur_Light do
    case Fi.id of
      ID_Layer:
        Fi.s := ResPicker.PickLayer(Fi.s);
      ID_LFlags:
        begin
          ValHex(Fi.s, fl);
          fl := FlagEdit.EditLightFlags(fl);
          Fi.s := Format('%x', [fl]);
        end;
      ID_RGB:
        begin
          SScanf(Fi.s, '%l %l %l', [@r, @g, @b]);
          ColorDlg.Color := RGB(Round(r * 255), Round(g * 255), Round(b * 255));
          if ColorDlg.Execute then
          begin
            f := ColorDlg.Color;
            Fi.s := Sprintf('%.3f %.3f %.3f',
              [GetRValue(f) / 255, GetGValue(f) / 255, GetBValue(f) / 255]);
          end;
        end;
    end;
end;

Function ValLTField(const s: string; Cur_Light: TJedLight; id: integer)
  : boolean;
var
  d: double;
  i: integer;
  ar, ag, ab: single;
  f: longint;
begin
  Result := false;
  if Cur_Light = nil then
    exit;

  SaveLightUndo(Cur_Light, ch_changed);

  i := id;
  With Cur_Light do
    case i of
      ID_X:
        begin
          if not ValDouble(s, d) then
            exit;
          x := d;
        end;
      ID_Y:
        begin
          if not ValDouble(s, d) then
            exit;
          y := d;
        end;
      ID_Z:
        begin
          if not ValDouble(s, d) then
            exit;
          z := d;
        end;
      ID_INTENSITY:
        begin
          if not ValDouble(s, d) then
            exit;
          Intensity := d;
        end;
      ID_RANGE:
        begin
          if not ValDouble(s, d) then
            exit;
          Range := d;
        end;
      ID_LFlags:
        begin
          if not ValHex(s, f) then
            exit;
          Flags := f;
        end;
      ID_Layer:
        Layer := Toolbar.AddLayer(s);
      ID_RGB:
        begin
          if not SScanf(s, '%l %l %l', [@ar, @ag, @ab]) then
            exit;
          r := ar;
          g := ag;
          b := ab;
        end;
      { ID_RGBRange: begin if not ValDouble(fi.s,d) then exit; RGBRange:=d; end; }
      ID_RGBIntensity:
        begin
          if not ValDouble(s, d) then
            exit;
          RGBIntensity := d;
        end;
    end;
  Result := true;
  JedMain.LevelChanged;
end;

Function TItemEdit.LightChange(Fi: TFieldInfo): boolean;
var
  i: integer;
  ltsl: TLTMultiSel;
  lt: TJedLight;
begin

  { JedMain.SaveSelLightsUndo('Change light(s)',ch_changed); }

  StartUndoRec('Change light(s)');

  Result := ValLTField(Fi.s, Cur_Light, Fi.id);

  if not Result then
    exit;
  ltsl := JedMain.ltsel;
  for i := 0 to ltsl.count - 1 do
  begin
    lt := Level.Lights[ltsl.GetLT(i)];
    ValLTField(Fi.s, lt, Fi.id);
  end;
end;

Procedure TItemEdit.ResetEdit;
begin
  fe.Clear;
  PNButtons.Visible := false;
  BNAdd.Visible := false;
  BNRemove.Visible := false;
  BNAsFrame.Visible := false;
  BNPaste.Visible := false;
  LBCogs.Items.Clear;
  LBText.Caption := '';
end;

procedure TItemEdit.FormActivate(Sender: TObject);
begin
  JedMain.EditObject;
end;

procedure TItemEdit.BNAddClick(Sender: TObject);
var
  s: string;
  cv, v: TTPlValue;
  i, n: integer;
  thsel: TTHMultiSel;
  th: TJKThing;
begin
  if Cur_Thing = nil then
    exit;
  s := 'THINGFLAGS';
  if not InputQuery('Value name', 'Enter value name', s) then
    exit;

  StartUndoRec('Add thing value(s)');
  thsel := JedMain.thsel;

  n := thsel.AddTH(JedMain.Cur_TH);

  for i := 0 to thsel.count - 1 do
  begin
    th := Level.Things[thsel.GetTH(i)];
    SaveThingUndo(th, ch_changed);

    cv := Templates.GetTPLField(th.name, s);
    v := TTPlValue.Create;
    if cv <> nil then
    begin
      v.Assign(cv);
      v.Val(cv.AsString);
    end;
    v.name := s;
    v.atype := GetTplType(v.name);
    v.vtype := GetTPLVType(v.name);
    th.Vals.Add(v);
  end;

  thsel.DeleteN(n);

  if CompareText(s, 'thingflags') = 0 then
    fe.AddFieldStr('+' + s, ID_TH_Last + Cur_Thing.Vals.count - 1, v.AsString)
  else
    fe.AddFieldStr(s, ID_TH_Last + Cur_Thing.Vals.count - 1, v.AsString);
  fe.DoneAdding;
end;

Function RemoveTHValue(Cur_Thing: TJKThing; const fname: string;
  n: integer): boolean;
var
  nv, cn, i: integer;
  v: TTPlValue;
begin
  Result := false;
  cn := 0;
  v := nil;

  for i := 0 to Cur_Thing.Vals.count - 1 do
  begin
    nv := i;
    v := Cur_Thing.Vals[i];
    if CompareText(v.name, fname) <> 0 then
    begin
      v := nil;
      continue;
    end;
    if cn = n then
      break;
    inc(cn);
    v := nil;
  end;

  if v = nil then
    exit;

  SaveThingUndo(Cur_Thing, ch_changed);
  Cur_Thing.Vals[nv].free;
  Cur_Thing.Vals.Delete(nv);
  JedMain.ThingChanged(Cur_Thing);
end;

procedure TItemEdit.BNRemoveClick(Sender: TObject);
var
  i, nd, tmp, n, vn: integer;
  thsel: TTHMultiSel;
  fname: string;
begin
  if Cur_Thing = nil then
    exit;
  if SGFields.Row < Thing_fields then
    exit;
  vn := SGFields.Row - Thing_fields;

  fname := Cur_Thing.Vals[vn].name;

  n := 0;
  for i := vn - 1 downto 0 do
  begin
    if CompareText(Cur_Thing.Vals[i].name, fname) = 0 then
      inc(n);
  end;

  thsel := JedMain.thsel;
  tmp := thsel.AddTH(JedMain.Cur_TH);

  StartUndoRec('Remove thing value(s)');
  for i := 0 to thsel.count - 1 do
  begin
    RemoveTHValue(Level.Things[thsel.GetTH(i)], fname, n);
  end;
  thsel.DeleteN(tmp);

  LoadThing(Cur_Thing);
end;

procedure TItemEdit.BNAsFrameClick(Sender: TObject);
var
  clp: TClipboard;
  tx, ty, tz, tpch, tyaw, trol: double;
begin
  case JedMain.Map_mode of
    MM_TH:
      begin
        if Cur_Thing = nil then
          exit;
        with Cur_Thing do
        begin
          tx := x;
          ty := y;
          tz := z;
          tpch := pch;
          tyaw := yaw;
          trol := rol;
        end;
      end;
    MM_FR:
      begin
        if Cur_Frame = nil then
          exit;
        Cur_Frame.GetFrame(tx, ty, tz, tpch, tyaw, trol);
      end;
  end;

  clp := clipboard;
  clp.AsText := Sprintf('(%.5f/%.5f/%.5f:%.5f/%.5f/%.5f)',
    [tx, ty, tz, tpch, tyaw, trol]);
end;

procedure TItemEdit.BNPasteClick(Sender: TObject);
var
  clp: TClipboard;
  tx, ty, tz, tpch, tyaw, trol: double;
  s: string;
  i: integer;
begin
  clp := clipboard;
  if not clp.HasFormat(CF_TEXT) then
    exit;
  s := clp.AsText;
  if Pos('(', s) <> 1 then
    exit;
  if Pos(')', s) <> length(s) then
    exit;

  for i := 1 to length(s) do
    if s[i] in ['\', '/', ':', '(', ')'] then
      s[i] := ' ';

  case JedMain.Map_mode of
    MM_TH:
      begin
        if Cur_Thing = nil then
          exit;
        with Cur_Thing do
          SScanf(s, '%f %f %f %f %f %f', [@x, @y, @z, @pch, @yaw, @rol]);
        JedMain.ThingChanged(Cur_Thing);
        JedMain.EditObject;
        JedMain.Invalidate;
      end;
    MM_FR:
      begin
        if Cur_Frame = nil then
          exit;
        Cur_Frame.GetFrame(tx, ty, tz, tpch, tyaw, trol);
        SScanf(s, '%f %f %f %f %f %f', [@tx, @ty, @tz, @tpch, @tyaw, @trol]);
        Cur_Frame.SetFrame(tx, ty, tz, tpch, tyaw, trol);
        JedMain.LevelChanged;
        JedMain.EditObject;
        JedMain.Invalidate;
      end;
  end;
end;

Function TItemEdit.EdgeChange(Fi: TFieldInfo): boolean;
var
  i: integer;
  edsl: TEDMultiSel;
  n: integer;
begin
  Result := false;
  { StartUndoRec('Change edge');

    Result:=ValTHField(fi.s,cur_thing,fi.id);

    if not result then exit;
    thsl:=JedMain.thsel;
    for i:=0 to thsl.count-1 do
    begin
    ValTHField(fi.s,level.Things[thsl.GetTH(i)],fi.id);
    end; }
end;

Procedure TItemEdit.LoadEdge(surf: TJKSurface; edge: integer);
var
  v1, v2: TJKVertex;
begin
  Caption := Format('Sec %d Surf %d Edge %d',
    [surf.Sector.Num, surf.Num, edge]);
  ResetEdit;

  v1 := surf.vertices[edge];
  v2 := surf.vertices[surf.NextVX(edge)];

  fe.AddFieldInt('Starting VX', -1, v1.Num);
  fe.AddFieldInt('Ending VX', -1, v2.Num);
  fe.AddFieldFloat('Length', ID_EDGELEN,
    sqrt(sqr(v2.x - v1.x) + sqr(v2.y - v1.y) + sqr(v2.z - v1.z)));

  if not Visible then
    Show;
  fe.DoneAdding;
  fe.OnDblClick := nil;
  fe.ONChange := EdgeChange;
end;

procedure TItemEdit.FormDestroy(Sender: TObject);
begin
  fe.free;
  GetWinPos(self, IEditPos);
end;

procedure TItemEdit.FormDeactivate(Sender: TObject);
begin
  fe.DeactivateHandler;
end;

Procedure TItemEdit.DoPickThing;
var
  i: integer;
  Fi: TFieldInfo;
begin
  if Cur_Thing = nil then
    exit;
  for i := 0 to fe.FieldCount - 1 do
  begin
    Fi := fe.Fields[i];
    if Fi.id = ID_name then
    begin
      ThingDblClk(Fi);
      fe.DeactivateHandler;
      exit;
    end;
  end;
end;

Procedure TItemEdit.DoPickTexture;
var
  i: integer;
  Fi: TFieldInfo;
begin
  if Cur_Surf = nil then
    exit;
  for i := 0 to fe.FieldCount - 1 do
  begin
    Fi := fe.Fields[i];
    if Fi.id = ID_Material then
    begin
      SurfaceDblClk(Fi);
      fe.DeactivateHandler;
      exit;
    end;
  end;
end;

Procedure TItemEdit.WMNCDblClck(var msg: TMessage);
begin
  if msg.wparam <> HTCAPTION then
    exit;
  if ClientHeight < 16 then
    ClientHeight := IEditPos.h
  else
  begin
    IEditPos.h := ClientHeight;
    ClientHeight := 0;
  end;
  msg.Result := 0;
end;

procedure TItemEdit.CBOnTopClick(Sender: TObject);
begin
  IEOnTop := CBOnTop.Checked;
  SetStayOnTop(self, IEOnTop);
end;

Procedure TItemEdit.AddCogs(ct: TCog_Type; lobj: TObject);
var
  i, j: integer;
  cog: TCog;
begin
  for i := 0 to Level.Cogs.count - 1 do
  begin
    cog := Level.Cogs[i];
    for j := 0 to cog.Vals.count - 1 do
      With cog.Vals[j] do
        if (cog_type = ct) and (obj = lobj) then
        begin
          LBCogs.Items.AddObject(cog.name + '\' + Name, cog);
          break;
        end;
  end;
end;

procedure TItemEdit.LBCogsDblClick(Sender: TObject);
var
  i: integer;
begin
  i := LBCogs.ItemIndex;
  if i < 0 then
    exit;
  i := Level.Cogs.IndexOf(LBCogs.Items.Objects[i]);
  if i >= 0 then
    CogForm.GotoCOG(i);
end;

end.
