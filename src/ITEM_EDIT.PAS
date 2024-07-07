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
    bUpdating: Boolean;
    Procedure ResetEdit;
    Procedure WMNCDblClck(var msg: TMessage); message WM_NCLBUTTONDBLCLK;
    Function DoPickColor(initColor: TColorF): TColorF;
    procedure SetNameColWidth(width: Integer); overload;
    procedure SetNameColWidth(width, dpi: Integer); overload;
    procedure BeginUpdate;
    procedure EndUpdate;

  public
    { Public declarations }
    Procedure LoadSector(sec: TJKSector);
    Procedure LoadSurface(surf: TJKSurface);
    Procedure LoadEdge(surf: TJKSurface; edge: integer);
    Procedure LoadVertex(vx: TJKVertex);
    Procedure LoadThing(thing: TJKThing);
    Procedure LoadFrame(th, fr: integer);
    Procedure LoadLight(nLight: integer);
    Procedure LoadExtra(ex: integer);

    Procedure DblClk(Fi: TFieldInfo);
    Function SectorChange(Fi: TFieldInfo): boolean;
    Procedure SectorDblClk(Fi: TFieldInfo);
    Procedure UpdateSectorField(id: Integer);
    Function SurfaceChange(Fi: TFieldInfo): boolean;
    Procedure SurfaceDblClk(Fi: TFieldInfo);
    Function ThingChange(Fi: TFieldInfo): boolean;
    Function FrameChange(Fi: TFieldInfo): boolean;
    Function VertexChange(Fi: TFieldInfo): boolean;
    Procedure VertexDblClk(Fi: TFieldInfo);
    Procedure ThingDblClk(Fi: TFieldInfo);
    Procedure LightDblClk(Fi: TFieldInfo);
    Function LightChange(Fi: TFieldInfo): boolean;
    Function EdgeChange(Fi: TFieldInfo): boolean;
    Function ExtraChange(Fi: TFieldInfo): boolean;
    Procedure DoPickTexture;
    Procedure DoPickThing;
    Procedure AddCogs(ct: TCog_Type; lobj: TObject);
    Procedure SmartChangeFocus;
  end;

var
  ItemEdit: TItemEdit;

implementation

uses ResourcePicker, FlagEditor, Jed_Main, GlobalVars, U_Templates, U_tbar,
  U_SCFEdit, U_CogForm, lev_utils, U_Preview;

{$R *.DFM}

procedure VertexChanged(vx: TJKVertex);
begin
  for var i := 0 to vx.sector.surfaces.Count - 1 do
    vx.sector.surfaces[i].RecalcAll;
  JedMain.SectorChanged(vx.sector);
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
  if bUpdating then
    exit;

  Cur_Sec := sec;
  Caption := Format('Sector %d', [sec.num]);
  ResetEdit;

  SetNameColWidth(100);
  LBText.Caption := Format('%d Surfaces %d Vertices',
    [sec.surfaces.count, sec.vertices.Count]);

  with sec do
  begin
    fe.AddFieldHex('+Flags', ID_SEC_FLAGS, flags);
    if CurrentProject = IJIM then
      begin
        fe.AddFieldColor('+Ambient', ID_SEC_AMBIENT, ambient, (*withAlpha=*) false);
        fe.AddFieldColor('+Extra Light', ID_SEC_EXTRA_LIGHT, extraLight, (*withAlpha=*) false);

        fe.AddFieldLabel('Point Light');
        fe.AddFieldColor(' +Color', ID_SEC_POINT_LIGHT_COLOR, pointLight.color, (*withAlpha=*) false);
        fe.AddFieldStr('    Position', ID_SEC_POINT_LIGHT_POS,
          Sprintf('%1.6f %1.6f %1.6f', [pointLight.position.x, pointLight.position.y, pointLight.position.z]));
        fe.AddFieldStr('    Min Range', ID_SEC_POINT_LIGHT_MIN_RANGE,
          Sprintf('%1.6f', [pointLight.minRange]));
        fe.AddFieldStr('    Max Range', ID_SEC_POINT_LIGHT_MAX_RANGE,
          Sprintf('%1.6f', [pointLight.maxRange]));
      end
    else
      begin
        fe.AddFieldFloat('Ambient', ID_SEC_AMBIENT, RgbToIntensity(ambient));
        fe.AddFieldFloat('Extra Light', ID_SEC_EXTRA_LIGHT, RgbToIntensity(extraLight));
        fe.AddFieldStr('+Colormap', ID_SEC_COLORMAP, colormap);
        fe.AddFieldColor('+Tint', ID_SEC_TINT, Tint, (*withAlpha=*) false);
      end;

    fe.AddFieldLabel('Sound');
    fe.AddFieldStr(' +Name', ID_SEC_SOUND, Sound);
    fe.AddFieldFloat('    Volume', ID_SEC_SOUND_VOLUME, soundVolume);

    if CurrentProject = IJIM then
      fe.AddFieldStr('Thrust', ID_SEC_THRUST,
        Sprintf('%1.6f %1.6f %1.6f', [thrust.x, thrust.y, thrust.z]));

    fe.AddFieldStr('+Layer', ID_LAYER, Level.GetLayerName(layer));
  end;

  if not Visible then
    Show;

  fe.DoneAdding;
  AddCogs(ct_sec, Cur_Sec);
  fe.OnDblClick := SectorDblClk;
  fe.ONChange   := SectorChange;
end;

Procedure SectorSurfacesSetSurfFlags(sec: TJKSector; flags: longint);
begin
  for var i := 0 to sec.surfaces.Count - 1 do
    with sec.surfaces[i] do
      BitSet(surfflags, flags);
end;

Procedure SectorSurfacesClearSurfFlags(sec: TJKSector; flags: longint);
begin
  for var i := 0 to sec.surfaces.Count - 1 do
    with sec.surfaces[i] do
      BitClear(surfflags, flags);
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
            if (((sec.flags xor f) and SECF_Underwater) <> 0 ) then
              begin
                if (f and SECF_Underwater) <> 0 then
                  SectorSurfacesSetSurfFlags(sec, SF_Water)
                else
                   SectorSurfacesClearSurfFlags(sec, SF_Water);
              end;

            if CurrentProject = TProjectType.IJIM then
              if (((sec.flags xor f) and SECF_IJIM_Aetherim) <> 0 ) then
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
          sec.ambient := c;
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
          sec.extraLight := c;
      end;
    ID_SEC_POINT_LIGHT_COLOR:
      begin
        Result := ValColor(s, c);
        if Result then
          sec.pointLight.color := c;
      end;
    ID_SEC_POINT_LIGHT_POS:
      Result := SScanf(s, '%f %f %f', [@sec.pointLight.position.x, @sec.pointLight.position.y, @sec.pointLight.position.z]);
    ID_SEC_POINT_LIGHT_MIN_RANGE:
      Result := SScanf(s, '%f %f', [@sec.pointLight.minRange]);
     ID_SEC_POINT_LIGHT_MAX_RANGE:
      Result := SScanf(s, '%f %f', [@sec.pointLight.maxRange]);
    ID_SEC_COLORMAP:
      Begin
        Result := true;
        sec.colormap := s;
      end;
    ID_SEC_TINT:
      begin
        Result := ValColor(s, c);
        if Result then
          sec.tint := c;
      end;
    ID_SEC_SOUND:
      begin
        Result := true;
        sec.sound := s;
        if not P3DPlaySecSound or not Preview3D.Visible then
          exit;
      end;
    ID_SEC_SOUND_VOLUME:
      begin
        Result := ValDouble(s, d);
        if Result then
          sec.soundVolume := d;
        if not P3DPlaySecSound or not Preview3D.Visible then
          exit;
      end;
    ID_SEC_THRUST:
      begin
        Result := SScanf(s, '%f %f %f', [@sec.thrust.x, @sec.thrust.y, @sec.thrust.z]);

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
        var prevlayer := sec.layer;
        sec.layer := Toolbar.AddLayer(s);
        if prevlayer = sec.layer then
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

    try
      BeginUpdate;
      Result := ValSecField(Fi.s, Cur_Sec, Fi.id);
      if not Result then
        exit;
      scsl := JedMain.scsel;
      for i := 0 to scsl.Count - 1 do
        ValSecField(Fi.s, Level.Sectors[scsl.getSC(i)], Fi.id);
  finally
    EndUpdate;
  end;
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
        if not changed and PReview3D.Visible then // make sure current sector sound continues to play in preview window
          PReview3D.Invalidate;
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
        changed := CompareText(s, Fi.s) <> 0;
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
  if bUpdating then
    exit;

  Caption := Format('Sector %d Surface %d', [surf.sector.num, surf.num]);
  ResetEdit;

  SetNameColWidth(100);
  LBText.Caption := Format('%d Vertices', [surf.vertices.Count]);
  Cur_Surf := surf;

  with surf do
  begin
    fe.AddFieldHex('+Surface Flags', ID_SURF_SURFFLAGS, surfflags);
    fe.AddFieldHex('+Face Flags', ID_SURF_FACEFLAGS, faceFlags);

    fe.AddFieldStr('+Material', ID_SURF_MATERIAL, material);

    fe.AddFieldInt('+Gometry', ID_SURF_GEOMODE, geo);
    fe.AddFieldInt('+Lighting', ID_SURF_LIGHTMODE, light);
    if CurrentProject <> IJIM then
      fe.AddFieldInt('+Texture', ID_SURF_TEXMODE, tex);

    if adjoin = nil then
      ast := '-1'
    else
      ast := Format('%d %d', [adjoin.sector.num, adjoin.num]);
    fe.AddFieldStr('Adjoin', ID_SURF_ADJOIN, ast);
    fe.AddFieldHex('+Adjoin Flags', ID_SURF_ADJOINFLAGS, adjoinFlags);

    { fe.AddFieldInt('LIGHT',ID_light,Light); }
    if CurrentProject = IJIM then
      fe.AddFieldColor('+Extra Light', ID_SURF_EXTRA_LIGHT, extraLight, (*withAlpha=*)true)
    else
      fe.AddFieldFloat('Extra Light', ID_SURF_EXTRA_LIGHT, RgbaToIntensity(extraLight));

    fe.AddFieldFloat('U Scale', ID_SURF_USCALE, uscale);
    fe.AddFieldFloat('V Scale', ID_SURF_VSCALE, vscale);
  end;

  if not Visible then
    Show;

  fe.DoneAdding;
  AddCogs(ct_srf, Cur_Surf);
  fe.OnDblClick := SurfaceDblClk;
  fe.ONChange   := SurfaceChange;
end;

Function ParseMirrorAdjoin(const s: string; var sec, surf: Integer): Boolean;
  var w: string;
begin
  Result := False;
  var i := GetWord(s, 1, w);
  if w = '' then
    exit;

  if not ValInt(w, sec) then
    exit;

  if sec = -1 then
    begin
      Result := True;
      exit;
    end;

  if not CheckSectorIndex(level, sec) then
    exit;

  i := GetWord(s, i, w);
  if w = '' then
    exit;
  if not ValInt(w, surf) then
    exit;

  Result := CheckSectorSurfaceIndex(level.sectors[sec], surf);
end;

Function ParseSector(const s: string; var sec: Integer): Boolean;
  var w: string;
begin
  Result := False;
  var i := GetWord(s, 1, w);
  if w = '' then
    exit;

  if not ValInt(w, sec) then
    exit;

  if sec = -1 then
    begin
      Result := True;
      exit;
    end;

  Result := CheckSectorIndex(level, sec);
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

  SaveSecUndo(surf.sector, ch_changed, sc_val);

  with surf do
    case id of
      ID_SURF_MATERIAL:
        begin
          Result := true;
          material := s;
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
            light := i;
        end;
      ID_SURF_ADJOINFLAGS:
        begin
          Result := ValHex(s, f);
          if Result then
            adjoinFlags := f;
        end;
      ID_SURF_SURFFLAGS:
        begin
          Result := ValHex(s, f);
          if Result then
            surfflags := f;
        end;
      ID_SURF_FACEFLAGS:
        begin
          Result := ValHex(s, f);
          f := f and not FF_DoubleSided; // Remove 0x01 flag
          i := Ord((f and FF_SF_FLIP) <> (FaceFlags and FF_SF_FLIP));

          if Result then
          begin
            faceflags := f;
            if i > 0 then
            begin
              SaveSecUndo(surf.sector, ch_changed, sc_geo);  // RecalcAll changes UVs
              RecalcAll;
            end;
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
              extraLight := c;
            end;
        end;
      ID_SURF_USCALE:
        begin
          Result := ValSingle(s, sl);
          if Result then
          begin
            SaveSecUndo(surf.sector, ch_changed, sc_geo);  // RecalcAll changes UVs
            uscale := sl;
            RecalcAll;
          end
        end;
      ID_SURF_VSCALE:
        begin
          Result := ValSingle(s, sl);
          if Result then
          begin
            SaveSecUndo(surf.sector, ch_changed, sc_geo); // RecalcAll changes UVs
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
            adjoin := nil
          else
            adjoin := level.sectors[sc].surfaces[sf];
        end;
    end;

  JedMain.SectorChanged(surf.sector);
end;

Function TItemEdit.SurfaceChange(Fi: TFieldInfo): boolean;
var
  i: integer;
  sfsl: TSFMultiSel;
  sc, sf: integer;
begin
  { JedMain.SaveSelSurfUndo('Change surface(s)',ch_changed,sc_val); }
  StartUndoRec('Change Surface(s)');

  try
    BeginUpdate;
    Result := ValSurfField(Fi.s, Cur_Surf, Fi.id);
    if not Result then
      exit;

    sfsl := JedMain.sfsel;
    for i := 0 to sfsl.Count - 1 do
    begin
      sfsl.GetSCSF(i, sc, sf);
      ValSurfField(Fi.s, level.sectors[sc].surfaces[sf], Fi.id);
    end;
  finally
    EndUpdate;
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
        ResPicker.SetCMP(level.GetMasterCMP);
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
        JedMain.Invalidate;
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
  if bUpdating then
    exit;

  ResetEdit;
  SetNameColWidth(100);

  if fr < 0 then
  begin
    Caption := Format('Thing %d', [th]);
    fe.AddFieldInt('numframes', -1, level.things[th].nframes);
    fe.OnDblClick := nil;
    fe.ONChange := nil;
    exit;
  end;

  PNButtons.Visible := true;
  BNAsFrame.Visible := true;
  BNPaste.Visible := true;

  n := 0;
  with level.things[th] do
    For i := 0 to vals.Count - 1 do
    begin
      if i >= fr then
        break;
      if vals[i].atype = at_frame then
        Inc(n);
    end;

  Caption := Format('Thing %d Frame %d', [th, n]);

  Cur_Thing := level.things[th];
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
  if bUpdating then
    exit;

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
    fe.AddFieldStr('+Layer', ID_LAYER, level.GetLayerName(Layer));

    for i := 0 to vals.Count - 1 do
      with vals[i] do
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

  for i := 0 to Cur_Thing.vals.count - 1 do
  begin
    v := Cur_Thing.vals[i];
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
  with Cur_Thing do
    case i of
      ID_TH_NAME:
        begin
          Result := true;
          name := s;
        end;
      ID_TH_SECTOR:
        begin
          Result := ParseSector(s, i);
          if not Result then
            exit;

          if i = -1 then
            sec := nil
          else
            sec := level.sectors[i];
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
        begin
          Result := true; // should return True in order to affect other selected things
          var prevlayer := layer;
          layer := Toolbar.AddLayer(s);
          if prevlayer = layer then
            exit;
        end;
      { ID_TH_LAST..ID_TH_LAST+100:
        begin
        i:=Id-ID_TH_LAST;
        if i>=Vals.Count then exit;
        Vals[i].Val(s);
        Result:=true;
        end; }
    end;

  JedMain.ThingChanged(Cur_Thing);
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

  try
    BeginUpdate;
    if (Fi.id < ID_TH_LAST) then
      begin
        Result := ValTHField(Fi.s, Cur_Thing, Fi.id);

        if not Result then
          exit;
        thsl := JedMain.thsel;
        for i := 0 to thsl.Count - 1 do
        begin
          ValTHField(Fi.s, level.things[thsl.GetTH(i)], Fi.id);
        end;
      end
    else { thing value }
      begin
        { Find which number of the value it is. i.e. - for multiple values
          with the same name, like "frame" }
        fname := Cur_Thing.vals[Fi.id - ID_TH_LAST].name;
        n := 0;
        for i := Fi.id - ID_TH_LAST - 1 downto 0 do
        begin
          if CompareText(Cur_Thing.vals[i].name, fname) = 0 then
            inc(n);
        end;

        Result := ValTHValue(Fi.s, Cur_Thing, fname, n);
        if not Result then
          exit;

        thsl := JedMain.thsel;
        for i := 0 to thsl.Count - 1 do
        begin
          ValTHValue(Fi.s, level.things[thsl.GetTH(i)], fname, n);
        end;
      end;
  finally
    EndUpdate;
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
      ID_TH_SECTOR:
        begin
          // Select thing sector
          if not ParseSector(Fi.s, i) or (i = -1) then
            exit;
          JedMain.SetCurSC(i);
          JedMain.SetMapMode(MM_SC);
          exit;
        end;
      ID_TH_LAST .. ID_TH_LAST + 100:
        begin
          i := Fi.id - ID_TH_LAST;
          if i >= vals.Count then
            exit;

          if not IsUIEditProp(vals[i].name) then
            exit;

          if CompareText(vals[i].name, 'thingflags') = 0 then
            begin
              ValHex(Fi.s, f);
              var nf := FlagEdit.EditThingFlags(f);
              bChanged := f <> nf;
              Fi.s := Format('0x%x', [nf]);
            end
          else if (CompareText(vals[i].name, 'light') = 0) or
                  (CompareText(vals[i].name, 'lightintensity') = 0) then
          begin
            ValColor(Fi.s, c);
            c := DoPickColor(c);
            var s:= ColorToStr(c, {withAlpha=} false, {bFormat=}true);
            bChanged := s <> Fi.s;
            Fi.s := s;
          end;

        end;
      ID_LAYER:
        begin
          var prevlayer := Fi.s;
          Fi.s := ResPicker.PickLayer(Fi.s);
          bChanged := CompareText(Fi.s, prevlayer) <> 0;
        end;
    end;
  if bChanged then
    ThingChange(Fi);
end;

Function ValFRField(const s: string; Cur_Thing: TJKThing; Cur_Frame: TTPlValue; id: integer)
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

  SaveThingUndo(Cur_Thing, ch_changed);
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
  JedMain.ThingChanged(Cur_Thing);
  JedMain.Invalidate;
  Result := true;
end;

Function TItemEdit.FrameChange(Fi: TFieldInfo): boolean;
var
  i: integer;
  frsl: TFRMultiSel;
  fr, th: integer;
begin
  try
    BeginUpdate;
    StartUndoRec('Change Frame(s)');

    Result := ValFRField(Fi.s, Cur_Thing, Cur_Frame, Fi.id);
    if not Result then
      exit;

    frsl := JedMain.frsel;
    for i := 0 to frsl.count - 1 do
    begin
      frsl.GetTHFR(i, th, fr);
      if fr = -1 then
        continue;
      ValFRField(Fi.s, level.things[th], level.things[th].vals[fr], Fi.id);
    end;
  finally
    EndUpdate;
  end;
end;

Procedure TItemEdit.LoadVertex(vx: TJKVertex);
var
  i: integer;
begin
  if bUpdating then
    exit;

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
  if bUpdating then
    exit;

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
  try
    BeginUpdate;
    Result := false;
    obj := JedMain.ExtraObjs[JedMain.Cur_EX];
    if not(obj is TExtraVertex) then
      exit;

    ex := TExtraVertex(obj);
    Result := ValExtraVertex(Fi.s, ex, Fi.id);
    if Result then
      if Assigned(JedMain.OnExtraMove) then
        JedMain.OnExtraMove(ex, false);
  finally
    EndUpdate;
  end;
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
  with Cur_VX do
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

  try
    BeginUpdate;
    StartUndoRec('Change Vertex');

    Result := ValVertField(Fi.s, Cur_VX, Fi.id);
    if not Result then
      exit;
    vxsl := JedMain.vxsel;
    for i := 0 to vxsl.count - 1 do
    begin
      vxsl.GetSCVX(i, sc, vx);
      ValVertField(Fi.s, level.sectors[sc].vertices[vx], Fi.id);
    end;
  finally
    EndUpdate;
  end;
end;

Procedure TItemEdit.LoadLight(nLight: integer);
begin
  if bUpdating then
    exit;

  Caption := Format('Light %d', [nLight]);
  ResetEdit;
  SetNameColWidth(100);

  if nLight >= level.lights.Count then
    exit;

  Cur_Light := level.lights[nLight];
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
      fe.AddFieldStr('+Layer', ID_LAYER, level.GetLayerName(Layer));
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
  changed: Boolean;
begin
  if Cur_Light = nil then
    exit;

  changed := False;
  With Cur_Light do
    case Fi.id of
      ID_LAYER:
        begin
          var nl := ResPicker.PickLayer(Fi.s);
          changed := nl <> Fi.s;
          Fi.s := nl;
        end;
      ID_LT_FLAGS:
        begin
          ValHex(Fi.s, fl);
          var nfl := FlagEdit.EditLightFlags(fl);
          changed := nfl <> fl;
          Fi.s := Format('0x%x', [nfl]);
        end;
      ID_LT_COLOR:
        begin
          ValColor(Fi.s, c);
          var nc := DoPickColor(c);
          changed := nc <> c;
          Fi.s := ColorToStr(nc, (*withAlpha=*)false);
        end;
    end;

  if changed then
    LightChange(Fi);
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
        layer := Toolbar.AddLayer(s);
      ID_LT_COLOR:
        begin
          if not ValColor(s, color) then
            exit;
        end;
    end;
  Result := true;
  JedMain.LightChanged(Cur_Light);
end;

Function TItemEdit.LightChange(Fi: TFieldInfo): boolean;
var
  i: integer;
  ltsl: TLTMultiSel;
  lt: TSedLight;
begin
  try
     BeginUpdate;
     StartUndoRec('Change Light(s)');

    Result := ValLTField(Fi.s, Cur_Light, Fi.id);
    if not Result then
      exit;

    ltsl := JedMain.ltsel;
    for i := 0 to ltsl.Count - 1 do
    begin
      lt := level.lights[ltsl.GetLT(i)];
      ValLTField(Fi.s, lt, Fi.id);
    end;
  finally
    EndUpdate;
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

procedure TItemEdit.BeginUpdate;
begin
   bUpdating := true;
end;

procedure TItemEdit.EndUpdate;
begin
  JedMain.Update; // force window update so in case cur object was changed it's editor load function will catche bEditorChange = true
  bUpdating := false;
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
    th := level.things[thsel.GetTH(i)];
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
    fe.AddFieldStr('+' + s, ID_TH_LAST + Cur_Thing.vals.Count - 1, v.AsString)
  else
    fe.AddFieldStr(s, ID_TH_LAST + Cur_Thing.vals.Count - 1, v.AsString);
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

  for i := 0 to Cur_Thing.vals.count - 1 do
  begin
    nv := i;
    v := Cur_Thing.vals[i];
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
  Cur_Thing.vals[nv].free;
  Cur_Thing.vals.Delete(nv);
  JedMain.ThingChanged(Cur_Thing);
end;

procedure TItemEdit.BNRemoveClick(Sender: TObject);
var
  i, nd, tmp, n, vn: Integer;
  thsel: TTHMultiSel;
  fname: string;
begin
  if Cur_Thing = nil then
    exit;
  if SGFields.Row < NUM_THING_FIELDS then
    exit;
  vn := SGFields.Row - NUM_THING_FIELDS;

  fname := Cur_Thing.vals[vn].name;

  n := 0;
  for i := vn - 1 downto 0 do
  begin
    if CompareText(Cur_Thing.vals[i].name, fname) = 0 then
      Inc(n);
  end;

  thsel := JedMain.thsel;
  tmp := thsel.AddTH(JedMain.Cur_TH);

  StartUndoRec('Remove Thing value(s)');
  for i := 0 to thsel.Count - 1 do
  begin
    RemoveTHValue(level.things[thsel.GetTH(i)], fname, n);
  end;
  thsel.DeleteN(tmp);

  LoadThing(Cur_Thing);
end;

procedure TItemEdit.BNAsFrameClick(Sender: TObject);
var
  clp: TClipboard;
  tx, ty, tz, tpch, tyaw, trol: double;
begin
  case JedMain.mapMode of
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
    MM_LT: // handle set light to sector light (IJIM)
      begin
          StartUndoRec('Set Light to Sector(s)');
          var scsl := JedMain.scsel;
          var si := scsl.AddSC(JedMain.Cur_SC);
          var lt := level.lights[JedMain.Cur_LT];
          for var i := 0 to scsl.Count - 1 do
            begin
              var sec := level.sectors[scsl.getSC(i)];
              SaveSecUndo(sec, ch_changed, sc_val);
              with sec.PointLight do
                begin
                  color    := lt.color;
                  position := lt.position;
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

  case JedMain.mapMode of
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
        JedMain.ThingChanged(Cur_Thing);
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
  if bUpdating then
    exit;

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
  for i := 0 to level.cogs.Count - 1 do
  begin
    cog := level.cogs[i];
    for j := 0 to cog.vals.Count - 1 do
      with cog.vals[j] do
        if (cog_type = ct) and (obj = lobj) then
        begin
          LBCogs.Items.AddObject(cog.name + '\' + name, cog);
        end;
  end;
end;

function ExtractCogSymName(const AStr: string): string;
var
  Index: Integer;
begin
  Index := LastDelimiter('\', AStr);
  if Index > 0 then
    Result := Copy(AStr, Index + 1, Length(AStr) - Index)
  else
    Result := '';
end;

procedure TItemEdit.LBCogsDblClick(Sender: TObject);
begin
  var i := LBCogs.ItemIndex;
  if i < 0 then
    exit;

  var cog := TCog(LBCogs.Items.Objects[i]);
  var sname := ExtractCogSymName(LBCogs.Items.Strings[i]);
  var ci := level.cogs.IndexOf(cog);
  if ci >= 0 then
    begin
      var vi := cog.vals.IndexOfName(sname);
      if vi >= 0 then
        CogForm.GotoCOGVal(ci, vi)
      else
        CogForm.GotoCOG(ci);
    end;
end;

end.
