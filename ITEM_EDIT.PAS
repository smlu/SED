unit Item_edit;

interface

uses
  Windows, Geometry, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  ExtCtrls, Grids, J_Level, FieldEdit, misc_utils, StdCtrls, Clipbrd,
  Values, u_multisel, u_undo;

Const
  ID_SEC_FLAGS                 = 0;
  ID_SEC_AMBIENT               = 1;
  ID_SEC_EXTRA_LIGHT           = 2;
  ID_SEC_POINT_LIGHT_COLOR     = 3;
  ID_SEC_POINT_LIGHT_POS       = 4;
  ID_SEC_POINT_LIGHT_MIN_RANGE = 5;
  ID_SEC_POINT_LIGHT_MAX_RANGE = 6;
  ID_SEC_COLORMAP              = 7;
  ID_SEC_TINT                  = 8;
  ID_SEC_SOUND                 = 9;
  ID_SEC_SOUND_VOLUME          = 10;
  ID_SEC_THRUST                = 11;

  ID_SURF_ADJOINFLAGS = 0;
  ID_SURF_MATERIAL    = 1;
  ID_SURF_SURFFLAGS   = 2;
  ID_SURF_FACEFLAGS   = 3;
  ID_SURF_GEOMODE     = 4;
  ID_SURF_LIGHTMODE   = 5;
  ID_SURF_TEXMODE     = 6;
  ID_SURF_EXTRA_LIGHT = 7;
  ID_SURF_ADJOIN      = 8;
  ID_SURF_USCALE      = 9;
  ID_SURF_VSCALE      = 10;

  ID_TH_SECTOR  = 0;
  ID_TH_NAME    = 1;
  ID_X          = 2;
  ID_Y          = 3;
  ID_Z          = 4;
  ID_PCH        = 5;
  ID_YAW        = 6;
  ID_ROL        = 7;
  ID_TH_LAST    = 20;

  ID_LT_INTENSITY = 0;
  ID_LT_RANGE     = 1;
  ID_LT_COLOR     = 12;
  ID_LT_FLAGS     = 14;

  ID_VERT_COLOR  = 0;
  ID_LAYER       = 15;

  NUM_THING_FIELDS = 9;
  ID_ED_LENGTH = 2;

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
    Cur_Light: TSedLight;
    Cur_Frame: TTPlValue;
    cedge: integer;
    Procedure ResetEdit;
    Procedure WMNCDblClck(var msg: TMessage); message WM_NCLBUTTONDBLCLK;
    Function DoPickColor(initColor: TColorF): TColorF;
    procedure SetNameColWidth(width: Integer); overload;
    procedure SetNameColWidth(width, dpi: Integer); overload;

  public
    { Public declarations }
    Procedure UpdateSectorField(id: Integer);
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
    Procedure VertexDblClk(Fi: TFieldInfo);
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

procedure VertexChanged(vx: TJKVertex);
begin
  for var i := 0 to vx.Sector.surfaces.count - 1 do
    vx.Sector.surfaces[i].RecalcAll;
  JedMain.SectorChanged(vx.Sector);
end;

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

Procedure TItemEdit.UpdateSectorField(id: Integer);
begin
  case id of
    ID_SEC_FLAGS :
    begin
      var fe := fe.Fields[ID_SEC_FLAGS];
      fe.s := Format('0x%x', [Cur_Sec.flags]);
    end;
  end;
end;

Procedure TItemEdit.LoadSector(sec: TJKSector);
var
  i: integer;
begin
  Cur_Sec := sec;
  Caption := Format('Sector %d', [sec.Num]);
  ResetEdit;

  SetNameColWidth(100);
  LBText.Caption := Format('%d Surfaces %d Vertices',
    [sec.surfaces.count, sec.vertices.count]);

  With sec do
  begin
    fe.AddFieldHex('+Flags', ID_SEC_FLAGS, Flags);
    if CurrentProject = IJIM then
      begin
        fe.AddFieldColor('+Ambient', ID_SEC_AMBIENT, Ambient, (*withAlpha=*) false);
        fe.AddFieldColor('+Extra Light', ID_SEC_EXTRA_LIGHT, ExtraLight, (*withAlpha=*) false);

        fe.AddFieldLabel('Point Light');
        fe.AddFieldColor('    +Color', ID_SEC_POINT_LIGHT_COLOR, PointLight.Color, (*withAlpha=*) false);
        fe.AddFieldStr('    Position', ID_SEC_POINT_LIGHT_POS,
          Sprintf('%1.6f %1.6f %1.6f', [PointLight.Position.x, PointLight.Position.y, PointLight.Position.z]));
        fe.AddFieldStr('    Min Range', ID_SEC_POINT_LIGHT_MIN_RANGE,
          Sprintf('%1.6f', [PointLight.minRange]));
        fe.AddFieldStr('    Max Range', ID_SEC_POINT_LIGHT_MAX_RANGE,
          Sprintf('%1.6f', [PointLight.maxRange]));
      end
    else
      begin
        fe.AddFieldFloat('Ambient', ID_SEC_AMBIENT, RgbToIntensity(ambient));
        fe.AddFieldFloat('Extra Light', ID_SEC_EXTRA_LIGHT, RgbToIntensity(extraLight));
        fe.AddFieldStr('+Colormap', ID_SEC_COLORMAP, colormap);
        fe.AddFieldColor('+Tint', ID_SEC_TINT, Tint, (*withAlpha=*) false);
      end;

    fe.AddFieldStr('+Sound', ID_SEC_SOUND, Sound);
    fe.AddFieldFloat('Volume', ID_SEC_SOUND_VOLUME, soundVolume);
    if CurrentProject = IJIM then
      fe.AddFieldStr('Thrust', ID_SEC_THRUST,
        Sprintf('%1.6f %1.6f %1.6f', [Thrust.x, Thrust.y, Thrust.z]));

    fe.AddFieldStr('+Layer', ID_LAYER, Level.GetLayerName(Layer));

    // TODO: add field for IM point light

  end;
  if not Visible then
    Show;
  fe.DoneAdding;
  AddCogs(ct_sec, Cur_Sec);
  fe.OnDblClick := SectorDblClk;
  fe.ONChange := SectorChange;
end;


Procedure SectorSurfacesSetSurfFlags(sec: TJKSector; flags: longint);
begin
  for var i := 0 to sec.surfaces.count - 1 do
    with sec.surfaces[i] do
      BitSet(SurfFlags, flags);
end;

Procedure SectorSurfacesClearSurfFlags(sec: TJKSector; flags: longint);
begin
  for var i := 0 to sec.surfaces.count - 1 do
    with sec.surfaces[i] do
      BitClear(SurfFlags, flags);
end;

Function ValSecField(const s: string; sec: TJKSector; id: integer): boolean;
var
  f: longint;
  d: double;
  Tint: TVector;
  i: integer;
  c: TColorF;
begin
  Result := false;
  if sec = nil then
    exit;

  SaveSecUndo(sec, ch_changed, sc_val);

  case id of
    ID_SEC_FLAGS:
      begin
        Result := ValHex(s, f);
        if Result then
          begin
            if (((sec.Flags xor f) and SECF_Underwater) <> 0 ) then
              begin
                if (f and SECF_Underwater) <> 0 then
                  SectorSurfacesSetSurfFlags(sec, SF_Water)
                else
                   SectorSurfacesClearSurfFlags(sec, SF_Water);
              end;

            if CurrentProject = TProjectType.IJIM then
              if (((sec.Flags xor f) and SECF_IJIM_Aetherim) <> 0 ) then
                begin
                  if (f and SECF_IJIM_Aetherim) <> 0 then
                    SectorSurfacesSetSurfFlags(sec, SF_IJIM_Aetherim)
                  else
                     SectorSurfacesClearSurfFlags(sec, SF_IJIM_Aetherim);
                end;

            sec.flags := f;
          end;
      end;
    ID_SEC_AMBIENT:
    begin
      begin
        if CurrentProject = TProjectType.IJIM then
          Result := ValColor(s, c)
        else
          begin
            Result := ValDouble(s, d);
            c := MakeColor(d, d, d, d);
          end;
        if Result then
          sec.Ambient := c;
      end;
    end;
    ID_SEC_EXTRA_LIGHT:
      begin
        if CurrentProject = TProjectType.IJIM then
          Result := ValColor(s, c)
        else
          begin
            Result := ValDouble(s, d);
            c := MakeColor(d, d, d, d);
          end;
        if Result then
          sec.ExtraLight := c;
      end;
    ID_SEC_POINT_LIGHT_COLOR:
      begin
        Result := ValColor(s, c);
        if Result then
          sec.PointLight.Color := c;
      end;
    ID_SEC_POINT_LIGHT_POS:
      Result := SScanf(s, '%f %f %f', [@sec.PointLight.Position.x, @sec.PointLight.Position.y, @sec.PointLight.Position.z]);
    ID_SEC_POINT_LIGHT_MIN_RANGE:
      Result := SScanf(s, '%f %f', [@sec.PointLight.minRange]);
     ID_SEC_POINT_LIGHT_MAX_RANGE:
      Result := SScanf(s, '%f %f', [@sec.PointLight.maxRange]);
    ID_SEC_COLORMAP:
      Begin
        Result := true;
        sec.Colormap := s;
      end;
    ID_SEC_TINT:
      begin
        Result   := ValColor(s, c);
        if Result then
          sec.Tint := c;
      end;
    ID_SEC_SOUND:
      begin
        Result := true;
        sec.Sound := s;
        exit;
      end;
    ID_SEC_SOUND_VOLUME:
      begin
        Result := ValDouble(s, d);
        if Result then
          sec.soundVolume := d;
        exit;
      end;
    ID_SEC_THRUST:
      begin
        Result := SScanf(s, '%f %f %f', [@sec.Thrust.x, @sec.Thrust.y, @sec.Thrust.z]);

        // Update flags
        var flags := sec.flags;
        if sec.Thrust.IsZero() then
          BitClear(sec.flags, SECF_IJIM_UseThrust)
        else
          BitSet(sec.flags, SECF_IJIM_UseThrust);

        if (sec.flags <> flags) then
          ItemEdit.UpdateSectorField(ID_SEC_FLAGS);
      end;
    ID_LAYER:
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
  c: TColorF;
  changed: Boolean;
begin
  Case Fi.id of
    ID_SEC_FLAGS:
      begin
        ValHex(Fi.s, f);
        var newf: longint := FlagEdit.EditSectorFlags(f);
        changed := f <> newf;
        Fi.s := Format('0x%x', [newf]);
      end;
    ID_SEC_AMBIENT:
      begin
        if CurrentProject = IJIM then
        begin
          ValColor(Fi.s, c);
          c := DoPickColor(c);
          var s := ColorToStr(c, (*withAlpha=*)false);
          changed := s <> Fi.s;
          Fi.s := s;
        end;
      end;
    ID_SEC_EXTRA_LIGHT:
      begin
        if CurrentProject = IJIM then
        begin
          ValColor(Fi.s, c);
          c := DoPickColor(c);
          var s := ColorToStr(c, (*withAlpha=*)false);
          changed := s <> Fi.s;
          Fi.s := s;
        end;
      end;
    ID_SEC_POINT_LIGHT_COLOR:
      begin
        ValColor(Fi.s, c);
        c := DoPickColor(c);
        var s := ColorToStr(c, (*withAlpha=*)false);
        changed := s <> Fi.s;
        Fi.s := s;
      end;
    ID_SEC_COLORMAP:
      begin
        var s := ResPicker.PickCMP(Fi.s);
        changed := s <> Fi.s;
        Fi.s := s;
      end;
    ID_SEC_SOUND:
      begin
        var s := ResPicker.PickSecSound(Fi.s);
        changed := s <> Fi.s;
        Fi.s := s;
      end;
    ID_SEC_TINT:
      begin
        ValColor(Fi.s, c);
        c := DoPickColor(c);
        var s := ColorToStr(c, (*withAlpha=*)false);
        changed := s <> Fi.s;
        Fi.s := s;
      end;
    ID_LAYER:
      begin
        var s := ResPicker.PickLayer(Fi.s);
        changed := s <> Fi.s;
        Fi.s := s;
      end;
  end;
  if changed then
    SectorChange(Fi);
end;

Procedure TItemEdit.LoadSurface(surf: TJKSurface);
var
  ast: string;
begin
  Caption := Format('Sector %d Surface %d', [surf.Sector.Num, surf.Num]);
  ResetEdit;

  SetNameColWidth(100);
  LBText.Caption := Format('%d Vertices', [surf.vertices.count]);
  Cur_Surf := surf;

  With surf do
  begin
    fe.AddFieldHex('+Surface Flags', ID_SURF_SURFFLAGS, SurfFlags);
    fe.AddFieldHex('+Face Flags', ID_SURF_FACEFLAGS, FaceFlags);

    fe.AddFieldStr('+Material', ID_SURF_MATERIAL, Material);

    fe.AddFieldInt('+Gometry', ID_SURF_GEOMODE, geo);
    fe.AddFieldInt('+Lighting', ID_SURF_LIGHTMODE, Light);
    if CurrentProject <> IJIM then
      fe.AddFieldInt('+Texture', ID_SURF_TEXMODE, tex);

    if Adjoin = nil then
      ast := '-1'
    else
      ast := Format('%d %d', [Adjoin.Sector.Num, Adjoin.Num]);
    fe.AddFieldStr('Adjoin', ID_SURF_ADJOIN, ast);
    fe.AddFieldHex('+Adjoin Flags', ID_SURF_ADJOINFLAGS, AdjoinFlags);

    { fe.AddFieldInt('LIGHT',ID_light,Light); }
    if CurrentProject = IJIM then
      fe.AddFieldColor('+Extra Light', ID_SURF_EXTRA_LIGHT, ExtraLight, (*withAlpha=*)true)
    else
      fe.AddFieldFloat('Extra Light', ID_SURF_EXTRA_LIGHT, RgbaToIntensity(ExtraLight));

    fe.AddFieldFloat('U Scale', ID_SURF_USCALE, uscale);
    fe.AddFieldFloat('V Scale', ID_SURF_VSCALE, vscale);
  end;

  if not Visible then
    Show;
  fe.DoneAdding;
  AddCogs(ct_srf, Cur_Surf);
  fe.OnDblClick := SurfaceDblClk;
  fe.ONChange := SurfaceChange;
end;

Function ParseMirrorAdjoin(const s: string; var sec, surf: Integer): Boolean;
  var w: string;
begin
  Result := false;
  var i := GetWord(s, 1, w);
  if w = '' then
    exit;

  if not ValInt(w, sec) then
    exit;

  if sec = -1 then
    begin
      Result := true;
      exit;
    end;

  if (sec < 0) or (sec >= Level.Sectors.count) then
    exit;

  i := GetWord(s, i, w);
  if w = '' then
    exit;
  if not ValInt(w, surf) then
    exit;

  if (surf < 0) or (surf >= Level.Sectors[sec].surfaces.count) then
        exit;
  Result := true;
end;

Function ValSurfField(const s: string; surf: TJKSurface; id: integer): boolean;
var
  sc, sf, i: integer;
  w: string;
  f: longint;
  d: double;
  sl: single;
  c: TColorF;
begin
  Result := false;
  if surf = nil then
    exit;

  SaveSecUndo(surf.Sector, ch_changed, sc_val);

  With surf do
    case id of
      ID_SURF_MATERIAL:
        begin
          Result := true;
          Material := s;
        end;
      ID_SURF_GEOMODE:
        begin
          Result := ValInt(s, i);
          if Result then
            geo := i;
        end;
      ID_SURF_TEXMODE:
        begin
          Result := ValInt(s, i);
          if Result then
            tex := i;
        end;
      ID_SURF_LIGHTMODE:
        begin
          Result := ValInt(s, i);
          if Result then
            Light := i;
        end;
      ID_SURF_ADJOINFLAGS:
        begin
          Result := ValHex(s, f);
          if Result then
            AdjoinFlags := f;
        end;
      ID_SURF_SURFFLAGS:
        begin
          Result := ValHex(s, f);
          if Result then
            SurfFlags := f;
        end;
      ID_SURF_FACEFLAGS:
        begin
          Result := ValHex(s, f);
          f := f and not FF_DoubleSided; // Remove 0x01 flag
          i := Ord((f and FF_SF_FLIP) <> (FaceFlags and FF_SF_FLIP));

          if Result then
          begin
            FaceFlags := f;
            if i > 0 then
              RecalcAll;
          end;
        end;
      ID_SURF_EXTRA_LIGHT:
        begin
          if CurrentProject = IJIM then
            Result := ValColor(s, c)
          else
            begin
              Result := ValDouble(s, d);
              c := MakeColor(d, d, d, d);
            end;
          if Result then
            begin
              ExtraLight := c;
            end;
        end;
      ID_SURF_USCALE:
        begin
          Result := ValSingle(s, sl);
          if Result then
          begin
            uscale := sl;
            RecalcAll;
          end
        end;
      ID_SURF_VSCALE:
        begin
          Result := ValSingle(s, sl);
          if Result then
          begin
            vscale := sl;
            RecalcAll;
          end
        end;
      ID_SURF_ADJOIN:
        begin
          if not ParseMirrorAdjoin(s, sc, sf) then
            exit;

          Result := true;
          if sc = -1 then
            Adjoin := nil
          else
            Adjoin := Level.Sectors[sc].surfaces[sf];
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
  StartUndoRec('Change Surface(s)');

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
  c: TColorF;
  sc, sf: integer;
begin
  case Fi.id of
    ID_SURF_MATERIAL:
      begin
        if Cur_Surf = nil then
          exit;
        ResPicker.SetCMP(Level.GetMasterCMP);
        { if Cur_surf.sector.colormap<>'' then
          ResPicker.SetCMP(Level.sectors[cur_surf.sector.num].COlormap); }
        Fi.s := ResPicker.PickMAT(Fi.s);
      end;
    ID_SURF_GEOMODE:
      begin
        ValDword(Fi.s, f);
        f := SCFieldPicker.PickGeo(f);
        Fi.s := IntToStr(f);
      end;
    ID_SURF_TEXMODE:
      begin
        ValDword(Fi.s, f);
        f := SCFieldPicker.PickTex(f);
        Fi.s := IntToStr(f);
      end;
    ID_SURF_LIGHTMODE:
      begin
        ValDword(Fi.s, f);
        f := SCFieldPicker.PickLightMode(f);
        Fi.s := IntToStr(f);
      end;
    ID_SURF_ADJOIN:
      begin
        // Select adjoin surface
        if not ParseMirrorAdjoin(Fi.s, sc, sf) or (sc = -1) then
          exit;
        JedMain.SetCurSF(sc, sf);
        exit;
      end;
    ID_SURF_ADJOINFLAGS:
      begin
        ValHex(Fi.s, f);
        f := FlagEdit.EditAdjoinFlags(f);
        Fi.s := Format('0x%x', [f]);
      end;
    ID_SURF_SURFFLAGS:
      begin
        ValHex(Fi.s, f);
        f := FlagEdit.EditSurfaceFlags(f);
        Fi.s := Format('0x%x', [f]);
      end;
    ID_SURF_FACEFLAGS:
      begin
        ValHex(Fi.s, f);
        f := FlagEdit.EditFaceFlags(f);
        f := f and not FF_DoubleSided; // Remove 0x01 flag
        Fi.s := Format('0x%x', [f]);
      end;
    ID_SURF_EXTRA_LIGHT:
      begin
        if CurrentProject = IJIM then
        begin
          ValColor(Fi.s, c);
          c := DoPickColor(c);
          Fi.s := ColorToStr(c, (*withAlpha=*)true);
        end;
      end;

  end;
  SurfaceChange(Fi);
end;

Procedure TItemEdit.LoadFrame(th, fr: integer);
var
  i, n: integer;
  x, y, z, pch, yaw, rol: double;
  fm: TForm;
begin
  ResetEdit;
  SetNameColWidth(100);

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

  Cur_Thing := Level.Things[th];
  Cur_Frame := Cur_Thing.Vals[fr];
  Cur_Frame.GetFrame(x, y, z, pch, yaw, rol);

  fe.AddFieldFloat('x', ID_X, x);
  fe.AddFieldFloat('y', ID_Y, y);
  fe.AddFieldFloat('z', ID_Z, z);
  fe.AddFieldFloat('Pitch', ID_PCH, pch);
  fe.AddFieldFloat('Yaw', ID_YAW, yaw);
  fe.AddFieldFloat('Roll', ID_ROL, rol);
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


Function IsUIEditProp(name: string): Boolean;
  begin
    Result := False;
    if CompareText(Name, 'thingflags') = 0 then
      Result := True
    else if CurrentProject = IJIM then
      begin
        if CompareText(Name, 'light') = 0 then
          Result := True
        else if CompareText(Name, 'lightintensity') = 0 then
          Result := True
      end;
  end;

Procedure TItemEdit.LoadThing(thing: TJKThing);
var
  i: integer;
begin
  Caption := Format('Thing %d', [thing.Num]);
  ResetEdit;

  SetNameColWidth(100);
  BNAdd.Visible     := true;
  BNRemove.Visible  := true;
  BNAsFrame.Visible := true;
  BNPaste.Visible   := true;
  PNButtons.Visible := true;

  Cur_Thing := thing;

  With thing do
  begin
    fe.AddFieldStr('+Name', ID_TH_NAME, Name);
    if sec = nil then
      fe.AddFieldInt('Sector', ID_TH_SECTOR, -1)
    else
      fe.AddFieldInt('Sector', ID_TH_SECTOR, sec.Num);
    fe.AddFieldFloat('x', ID_X, x);
    fe.AddFieldFloat('y', ID_Y, y);
    fe.AddFieldFloat('z', ID_Z, z);
    fe.AddFieldFloat('Pitch', ID_PCH, pch);
    fe.AddFieldFloat('Yaw', ID_YAW, yaw);
    fe.AddFieldFloat('Roll', ID_ROL, rol);
    fe.AddFieldStr('+Layer', ID_LAYER, Level.GetLayerName(Layer));

    for i := 0 to Vals.count - 1 do
      With Vals[i] do
        if IsUIEditProp(Name) then
          fe.AddFieldStr('+' + Name, ID_TH_LAST + i, AsString)
        else
          fe.AddFieldStr(Name, ID_TH_LAST + i, AsString);
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
      ID_TH_NAME:
        begin
          Result := true;
          name := s;
        end;
      ID_TH_SECTOR:
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
      ID_LAYER:
        Layer := Toolbar.AddLayer(s);
      { ID_TH_LAST..ID_TH_LAST+100:
        begin
        i:=Id-ID_TH_LAST;
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
  StartUndoRec('Change Thing(s)');
  { JedMain.SaveSelThingsUndo('Change thing(s)',ch_changed); }

  if (Fi.id < ID_TH_LAST) then
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
    { Find which number of the value it is. i.e. - for multiple values
      with the same name, like "frame" }
    fname := Cur_Thing.Vals[Fi.id - ID_TH_LAST].name;
    n := 0;
    for i := Fi.id - ID_TH_LAST - 1 downto 0 do
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
  c: TColorF;
begin
  if Cur_Thing = nil then
    exit;

  var bChanged := False;
  With Cur_Thing do
    case Fi.id of
      ID_TH_NAME:
        begin
          var tmpl := ResPicker.PickThing(Fi.s);
          bChanged := CompareText(Fi.s, tmpl) <> 0;
          Fi.s := tmpl;
        end;
      ID_TH_LAST .. ID_TH_LAST + 100:
        begin
          i := Fi.id - ID_TH_LAST;
          if i >= Vals.count then
            exit;

          if not IsUIEditProp(Vals[i].name) then
            exit;

          if CompareText(Vals[i].name, 'thingflags') = 0 then
            begin
              ValHex(Fi.s, f);
              var nf := FlagEdit.EditThingFlags(f);
              bChanged := f <> nf;
              Fi.s := Format('0x%x', [nf]);
            end
          else if (CompareText(Vals[i].name, 'light') = 0) or
                  (CompareText(Vals[i].name, 'lightintensity') = 0) then
          begin
            ValColor(Fi.s, c);
            c := DoPickColor(c);
            var s:= ColorToStr(c, {withAlpha=} false, {bFormat=}true);
            bChanged := s <> Fi.s;
            Fi.s := s;
          end;

        end;
      ID_LAYER:
        Fi.s := ResPicker.PickLayer(Fi.s);
    end;
  if bChanged then
    ThingChange(Fi);
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
  PNButtons.Visible := true;
  BNAsFrame.Visible := true;
  BNPaste.Visible   := true;
  SetNameColWidth(100);

  With vx do
    begin
      fe.AddFieldFloat('x', ID_X, x);
      fe.AddFieldFloat('y', ID_Y, y);
      fe.AddFieldFloat('z', ID_Z, z);

  // Vert colors editing doesn't work correct yet for seector surfaces that share same vertices
  // and only UV for 1 vertice can be selected. Needs fixing to be able to select vertex per face.
  //    var uv := GetUV();
  //    if (CurrentProject = MOTS) or (CurrentProject = IJIM) then
  //      fe.AddFieldColor('+COLOR', ID_VERT_COLOR, uv.color, (*withAlpha=*)false)
  //    else
  //      fe.AddFieldFloat('COLOR', ID_VERT_COLOR, RgbaToIntensity(uv.color));

    end;

  if not Visible then
    Show;
  fe.DoneAdding;
  fe.OnDblClick := VertexDblClk;
  fe.ONChange   := VertexChange;
end;

Procedure TItemEdit.LoadExtra(ex: integer);
var
  o: TObject;
begin
  Caption := Format(JedMain.ExtraObjsName + ' %d', [ex]);
  ResetEdit;

  SetNameColWidth(100);
  fe.ONChange := nil;

  o := JedMain.ExtraObjs[ex];
  if o is TExtraVertex then
  begin
    fe.AddFieldStr('Name', ID_TH_NAME, TExtraVertex(o).name);
    fe.AddFieldFloat('x', ID_X, TExtraVertex(o).x);
    fe.AddFieldFloat('y', ID_Y, TExtraVertex(o).y);
    fe.AddFieldFloat('z', ID_Y, TExtraVertex(o).z);
    fe.ONChange := ExtraChange;
  end;

  if o is TExtraLine then
    fe.AddFieldStr('Name', ID_TH_NAME, TExtraLine(o).name);
  if o is TExtraPolygon then
    fe.AddFieldStr('Name', ID_TH_NAME, TExtraPolygon(o).name);

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
  c: TColorF;
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
  c: TColorF;
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
      ID_VERT_COLOR:
        begin
          if CurrentProject <> TProjectType.JKDF2 then
            Result := ValColor(s, c)
          else
            begin
              Result := ValDouble(s, d);
              c := MakeColor(d, d, d, d);
            end;

          if not Result then exit;
          GetUV().color := c;
        end;
    end;

  Result := true;
  VertexChanged(Cur_VX);
end;


Procedure TItemEdit.VertexDblClk(Fi: TFieldInfo);
var
  f: longint;
  c: TColorF;
begin
  var changed := False;
  case Fi.id of
    ID_VERT_COLOR:
      begin
        if CurrentProject <> JKDF2 then
          begin
            ValColor(Fi.s, c);
            c := DoPickColor(c);
            var s:= ColorToStr(c, (*withAlpha=*)false);
            changed := s <> Fi.s;
            Fi.s := s;
          end;
      end;
  end;

  if changed then
    VertexChange(Fi);
end;

Function TItemEdit.VertexChange(Fi: TFieldInfo): boolean;
var
  i: integer;
  vxsl: TVXMultiSel;
  sc, vx: integer;
begin
  { JedMain.SaveSelVertUndo('Change vertices',ch_changed); }

  StartUndoRec('Change Vertex');

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
  SetNameColWidth(100);

  if nLight >= Level.Lights.count then
    exit;

  Cur_Light := Level.Lights[nLight];
  With Cur_Light do
    begin
      fe.AddFieldHex('+Flags', ID_LT_FLAGS, Flags);
      fe.AddFieldFloat('Range', ID_LT_RANGE, Range);
      fe.AddFieldFloat('Intensity', ID_LT_INTENSITY, Intensity);
      if CurrentProject <> JKDF2 then
      begin
        fe.AddFieldColor('+Color', ID_LT_COLOR, color, (*withAlpha=*) false);
      end;

      fe.AddFieldFloat('x', ID_X, position.x);
      fe.AddFieldFloat('y', ID_Y, position.y);
      fe.AddFieldFloat('z', ID_Z, position.z);
      fe.AddFieldStr('+Layer', ID_LAYER, Level.GetLayerName(Layer));
    end;

  if CurrentProject = IJIM then
    begin
      BNAsFrame.Caption  := '&Set to Sector(s)';
      BNAsFrame.Hint     := 'Set light as point light to selected Sector(s)';
      BNAsFrame.ShowHint := True;
      BNAsFrame.Visible  := True;
      PNButtons.Visible  := True;
    end;

  if not Visible then
    Show;

  fe.DoneAdding;
  fe.OnDblClick := LightDblClk;
  fe.ONChange   := LightChange;
end;

Procedure TItemEdit.LightDblClk(Fi: TFieldInfo);
var
  r, g, b: single;
  c: TColorF;
  fl: longint;
begin
  if Cur_Light = nil then
    exit;
  With Cur_Light do
    case Fi.id of
      ID_LAYER:
        Fi.s := ResPicker.PickLayer(Fi.s);
      ID_LT_FLAGS:
        begin
          ValHex(Fi.s, fl);
          fl := FlagEdit.EditLightFlags(fl);
          Fi.s := Format('0x%x', [fl]);
        end;
      ID_LT_COLOR:
        begin
          ValColor(Fi.s, c);
          c := DoPickColor(c);
          Fi.s := ColorToStr(c, (*withAlpha=*)false);
        end;
    end;
end;

Function ValLTField(const s: string; Cur_Light: TSedLight; id: integer)
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
          position.x := d;
        end;
      ID_Y:
        begin
          if not ValDouble(s, d) then
            exit;
          position.y := d;
        end;
      ID_Z:
        begin
          if not ValDouble(s, d) then
            exit;
          position.z := d;
        end;
      ID_LT_INTENSITY:
        begin
          if not ValDouble(s, d) then
            exit;
          Intensity := d;
        end;
      ID_LT_RANGE:
        begin
          if not ValDouble(s, d) then
            exit;
          Range := d;
        end;
      ID_LT_FLAGS:
        begin
          if not ValHex(s, f) then
            exit;
          Flags := f;
        end;
      ID_LAYER:
        Layer := Toolbar.AddLayer(s);
      ID_LT_COLOR:
        begin
          if not ValColor(s, color) then
            exit;
        end;
    end;
  Result := true;
  JedMain.LevelChanged;
end;

Function TItemEdit.LightChange(Fi: TFieldInfo): boolean;
var
  i: integer;
  ltsl: TLTMultiSel;
  lt: TSedLight;
begin

  { JedMain.SaveSelLightsUndo('Change light(s)',ch_changed); }

  StartUndoRec('Change Light(s)');

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
  PNButtons.Visible  := false;
  BNAdd.Visible      := false;
  BNRemove.Visible   := false;
  BNAsFrame.Visible  := false;
  BNAsFrame.Caption  := '&Copy as Frame';
  BNAsFrame.Hint     := '';
  BNAsFrame.ShowHint := false;
  BNPaste.Visible    := false;
  LBCogs.Items.Clear;
  LBText.Caption := '';
end;

procedure TItemEdit.FormActivate(Sender: TObject);
begin
  JedMain.EditObject;
end;

procedure TItemEdit.SetNameColWidth(width: Integer);
begin
  SetNameColWidth(width, Screen.PixelsPerInch);
end;

procedure TItemEdit.SetNameColWidth(width, dpi: Integer);
begin
  fe.NameColWidth := DpiScale(width, dpi);
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
  s := 'thingflags';
  if not InputQuery('Value name', 'Enter value name', s) then
    exit;

  StartUndoRec('Add Thing value(s)');
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
    v.name  := s;
    v.atype := GetTplType(v.name);
    v.vtype := GetTplVType(v.name);
    th.Vals.Add(v);
  end;

  thsel.DeleteN(n);

  if IsUIEditProp(s) then
    fe.AddFieldStr('+' + s, ID_TH_LAST + Cur_Thing.Vals.count - 1, v.AsString)
  else
    fe.AddFieldStr(s, ID_TH_LAST + Cur_Thing.Vals.count - 1, v.AsString);
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
  if SGFields.Row < NUM_THING_FIELDS then
    exit;
  vn := SGFields.Row - NUM_THING_FIELDS;

  fname := Cur_Thing.Vals[vn].name;

  n := 0;
  for i := vn - 1 downto 0 do
  begin
    if CompareText(Cur_Thing.Vals[i].name, fname) = 0 then
      inc(n);
  end;

  thsel := JedMain.thsel;
  tmp := thsel.AddTH(JedMain.Cur_TH);

  StartUndoRec('Remove Thing value(s)');
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
    MM_VX:
      begin
        if Cur_VX = nil then
          exit;

        tx := Cur_VX.x;
        ty := Cur_VX.y;
        tz := Cur_VX.z;
        tpch := 0;
        tyaw := 0;
        trol := 0;
      end;
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
    MM_LT: // handle light set light to sector light (IJIM)
      begin
          StartUndoRec('Set Light to Sector(s)');
          var scsl := JedMain.scsel;
          var si := scsl.AddSC(JedMain.Cur_SC);
          var lt := Level.Lights[JedMain.Cur_LT];
          for var i := 0 to scsl.count - 1 do
            begin
              var sec := Level.Sectors[scsl.getSC(i)];
              SaveSecUndo(sec, ch_changed, sc_val);
              with sec.PointLight do
                begin
                  Color := lt.color;
                  Position := T3DPoint(lt.position);
                  minRange := lt.Range;
                  maxRange := lt.Range / 4; // in IJIM levels the light max range is at 1/4 of min range
                end;
              JedMain.SectorChanged(sec);
            end;
          scsl.DeleteN(si);
        exit;
      end;
  end;

  clp := clipboard;
  clp.AsText := Sprintf('(%1.8f/%1.8f/%1.8f:%1.6f/%1.6f/%1.6f)',
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
    MM_VX:
      begin
        if Cur_VX = nil then
          exit;

        StartUndoRec('Change Vertex');
        SaveSecUndo(Cur_VX.Sector, ch_changed, sc_geo);
        with Cur_VX do
          SScanf(s, '%f %f %f', [@x, @y, @z]);
        VertexChanged(Cur_VX);
      end;
    MM_TH:
      begin
        if Cur_Thing = nil then
          exit;

        StartUndoRec('Change Thing');
        SaveThingUndo(Cur_Thing, ch_changed);
        with Cur_Thing do
          SScanf(s, '%f %f %f %f %f %f', [@x, @y, @z, @pch, @yaw, @rol]);
        JedMain.ThingChanged(Cur_Thing);
      end;
    MM_FR:
      begin
        if Cur_Frame = nil then
          exit;

        StartUndoRec('Change Frame');
        SaveThingUndo(Cur_Thing, ch_changed);
        Cur_Frame.GetFrame(tx, ty, tz, tpch, tyaw, trol);
        SScanf(s, '%f %f %f %f %f %f', [@tx, @ty, @tz, @tpch, @tyaw, @trol]);
        Cur_Frame.SetFrame(tx, ty, tz, tpch, tyaw, trol);
        JedMain.LevelChanged;
      end;
    else
      exit;
  end;

  JedMain.EditObject;
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

  SetNameColWidth(100);
  v1 := surf.vertices[edge];
  v2 := surf.vertices[surf.NextVX(edge)];

  fe.AddFieldInt('Starting VX', -1, v1.Num);
  fe.AddFieldInt('Ending VX', -1, v2.Num);
  fe.AddFieldFloat('Length', ID_ED_LENGTH,
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
    if Fi.id = ID_TH_NAME then
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
    if Fi.id = ID_SURF_MATERIAL then
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

Function TItemEdit.DoPickColor(initColor: TColorF): TColorF;
  var f: TColor;
  c: TColorF;
begin
  Result := initColor;
  initColor.Normalize; // make sure all color components has valid ranges 0.0 - 1.0

  ColorDlg.Color := RGB(
    Round(initColor.r * 255),
    Round(initColor.g * 255),
    Round(initColor.b * 255));
  if ColorDlg.Execute(self.Handle) then
  begin
    f := ColorDlg.Color;
    if f = -1 then f := 0;
    var a :=  Result.a;
    Result := MakeColor(
      GetRValue(f) / 255,
      GetGValue(f) / 255,
      GetBValue(f) / 255);
    Result.a := a;
  end;
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
