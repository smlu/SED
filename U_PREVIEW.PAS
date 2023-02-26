unit U_Preview;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls,
  Forms, Dialogs, D3D_Prender, J_Level, Geometry, StdCtrls,
  GlobalVars, Menus, OGL_Prender, Prender, Clipbrd,
  d3d_NPrender, u_pj3dos;

type
  TCamPos = record
    x, y, z, pch, yaw: double;
  end;

  TPreview3D = class(TForm)
    MainMenu1: TMainMenu;
    Preview: TMenuItem;
    Close1: TMenuItem;
    Settings1: TMenuItem;
    Commands1: TMenuItem;
    SetViewcamera1: TMenuItem;
    Keyboard1: TMenuItem;
    miControl: TMenuItem;
    miEdit: TMenuItem;
    miTex: TMenuItem;
    ToggleFullyLit1: TMenuItem;
    procedure FormShow(Sender: TObject);
    procedure FormHide(Sender: TObject);
    procedure FormKeyDown(Sender: TObject; var Key: Word; Shift: TShiftState);
    procedure FormPaint(Sender: TObject);
    procedure Close1Click(Sender: TObject);
    procedure Settings1Click(Sender: TObject);
    procedure FormMouseUp(Sender: TObject; Button: TMouseButton;
      Shift: TShiftState; x, y: Integer);
    procedure FormClose(Sender: TObject; var Action: TCloseAction);
    procedure FormDblClick(Sender: TObject);
    procedure FormCreate(Sender: TObject);
    procedure SetViewcamera1Click(Sender: TObject);
    procedure ToggleFullyLit1Click(Sender: TObject);
  private
    { Private declarations }
    Render3D: TPreviewRender;
    cwPlayer: Integer;

    scs: TStringList; { Lists of changed,added,deleted sectors and things }
    ths: TStringList;

    Procedure WMEraseBkg(var msg: TMessage); message WM_ERASEBKGND;
    procedure wmactivate(var message: TMessage); message wm_activate;
    procedure wmpaint(var message: TMessage); message wm_paint;
    Procedure SysAddSector(s: TJKSector);
    Procedure SysAddThing(th: TJKThing);
    Procedure SaveCamPos(var cpos: TCamPos);
    Procedure RestoreCamPos(const cpos: TCamPos);
    Procedure AddSecChange(sec: TJKSector; Change: Char);
    Procedure AddThingChange(th: TJKThing; Change: Char);
    Procedure ApplyChanges;
    Procedure AddKBItem(mi: TMenuItem; const name: string; c: Char;
      sc: TShiftState);
    procedure KBCommandClick(Sender: TObject);
  public
    { Public declarations }
    Procedure UpdateSector(s: TJKSector);
    Procedure AddSector(s: TJKSector);
    Procedure DeleteSector(s: TJKSector);

    Procedure UpdateThing(th: TJKThing);
    Procedure SetThing3DO(th: TJKThing; a3do: TPJ3DO);
    Procedure AddThing(th: TJKThing);
    Procedure DeleteThing(th: TJKThing);

    Procedure ReloadLevel;
    Procedure ShowPreview;
    Function IsActive: boolean;
    Procedure SetCam(x, y, z, pch, yaw: double);
    Procedure GotoWPlayer(wPlayer: Integer; step: Integer);
    Procedure GetCam(var x, y, z, pch, yaw: double);
  end;

var
  Preview3D: TPreview3D;

implementation

uses Jed_Main, U_Options, Misc_utils, Item_edit, U_tbar, ProgressDialog;

{$R *.DFM}

Procedure TPreview3D.ReloadLevel;
var
  i: Integer;
  sec: TJKSector;
  th: TJKThing;
begin
  if Render3D = nil then
    exit;
  scs.clear;
  ths.clear;
  Render3D.ClearThings;
  Render3D.ClearSectors;
  Render3D.SetGamma(P3DGamma);

  Progress.Reset(Level.Sectors.Count + Level.Things.Count);

  Progress.msg := 'Loading sectors...';

  for i := 0 to Level.Sectors.Count - 1 do
  begin
    Progress.step;
    sec := Level.Sectors[i];
    if not P3DVisLayers then
      SysAddSector(sec)
    else if ToolBar.IsLayerVisible(sec.Layer) then
      SysAddSector(sec);
  end;

  Progress.msg := 'Loading things...';

  for i := 0 to Level.Things.Count - 1 do
  begin
    Progress.step;
    th := Level.Things[i];
    if not P3DVisLayers then
      SysAddThing(th)
    else if ToolBar.IsLayerVisible(th.Layer) then
      SysAddThing(th);
  end;

  Progress.Hide;

  Invalidate;
end;

Procedure TPreview3D.GotoWPlayer(wPlayer: Integer; step: Integer);
var
  i: Integer;
  th: TJKThing;
begin
  if wPlayer >= Level.Things.Count then
    wPlayer := 0;
  if wPlayer < 0 then
    wPlayer := Level.Things.Count - 1;

  for i := 0 to Level.Things.Count - 1 do
  begin
    th := Level.Things[wPlayer];

    if compareText(th.name, 'walkplayer') = 0 then
      With th do
      begin
        Render3D.CamX := x;
        Render3D.CamY := y;
        Render3D.CamZ := z;
        Render3D.pch := -pch;
        Render3D.yaw := -yaw;
        cwPlayer := wPlayer;
        exit;
      end;

    inc(wPlayer, step);
    if wPlayer >= Level.Things.Count then
      wPlayer := 0;
    if wPlayer < 0 then
      wPlayer := Level.Things.Count - 1;

  end;

end;

Procedure TPreview3D.ShowPreview;
begin
  if visible then
  begin
    Invalidate;
    show;
  end
  else
  begin
    SetP3DPos(self, P3DX, P3DY, P3DWinSize);
    SetStayOnTop(self, P3DOnTop);
    show;
    if Render3D = nil then
    begin
      Hide;
      exit;
    end;
    ReloadLevel;
    GotoWPlayer(0, 1);
  end;
end;

procedure TPreview3D.FormShow(Sender: TObject);
begin
  case P3DAPI of
    P3D_OGL:
      Render3D := TOGLPRenderer.Create(self);
    P3D_d3d5:
      Render3D := TD3D5PRenderer.Create(self);
    { P3D_3dfx: Render3D:=T3DFXPRenderer.Create(Self); }
  else
    Render3D := TD3DRenderer.Create(self);
  end;

  Try
    Render3D.Initialize;
  except
    on E: Exception do
    begin
      Render3D.Free;
      Render3D := nil;
      PanMessage(mt_error, E.message);
    end;
  end;
end;

procedure TPreview3D.FormHide(Sender: TObject);
begin
  if Render3D <> nil then
    Render3D.Free;
  Render3D := nil;
  P3DX := left;
  P3DY := top;
end;

Procedure TPreview3D.WMEraseBkg(var msg: TMessage);
begin
  msg.Result := 0;
end;

procedure TPreview3D.FormKeyDown(Sender: TObject; var Key: Word;
  Shift: TShiftState);
begin
  if Render3D = nil then
    exit;

  With Render3D do
    case Key of
      VK_ADD:
        If P3DGamma < 254.9 then
        begin
          P3DGamma := P3DGamma + 0.1;
          if Render3D.SetGamma(P3DGamma) = 1 then
            ReloadLevel;
          Invalidate;
        end;
      VK_SUBTRACT:
        If P3DGamma > 0.1 then
        begin
          P3DGamma := P3DGamma - 0.1;
          if Render3D.SetGamma(P3DGamma) = 1 then
            ReloadLevel;
          Invalidate;
        end;
      VK_LEFT:
        if Shift = [ssShift] then
          JedMain.ShiftTexture(st_left)
        else if Shift = [ssCtrl] then
          JedMain.RotateTexture(st_left)
        else if ssAlt in Shift then
        begin
          CamY := CamY + sin(yaw / 180 * PI) * P3DStep;
          CamX := CamX - cos(yaw / 180 * PI) * P3DStep;
        end
        else
        begin
          yaw := yaw - 10;
          if yaw < 0 then
            yaw := yaw + 360;
        end;
      VK_RIGHT:
        if Shift = [ssShift] then
          JedMain.ShiftTexture(st_right)
        else if Shift = [ssCtrl] then
          JedMain.RotateTexture(st_right)
        else if ssAlt in Shift then
        begin
          CamY := CamY - sin(yaw / 180 * PI) * P3DStep;
          CamX := CamX + cos(yaw / 180 * PI) * P3DStep;
        end
        else
        begin
          yaw := yaw + 10;
          if yaw > 360 then
            yaw := yaw - 360;
        end;
      VK_UP:
        if Shift = [ssShift] then
          JedMain.ShiftTexture(st_up)
        else if Shift = [ssCtrl] then
          JedMain.RaiseObject(ro_up)
        else
        begin
          CamX := CamX + sin(yaw / 180 * PI) * P3DStep;
          CamY := CamY + cos(yaw / 180 * PI) * P3DStep;
        end;
      VK_DOWN:
        if Shift = [ssShift] then
          JedMain.ShiftTexture(st_down)
        else if Shift = [ssCtrl] then
          JedMain.RaiseObject(ro_down)
        else
        begin
          CamX := CamX - sin(yaw / 180 * PI) * P3DStep;
          CamY := CamY - cos(yaw / 180 * PI) * P3DStep;
        end;
      VK_NEXT:
        if pch >= -80 then
          pch := pch - 10;
      VK_PRIOR:
        if pch <= 80 then
          pch := pch + 10;
      VK_HOME:
        if Shift = [] then
          pch := 0
        else
          JedMain.StraightenTexture(ssCtrl in Shift, ssShift in Shift);

      Ord('A'):
        if Shift = [ssCtrl] then
          JedMain.FormKeyDown(nil, Key, [])
        else if (Shift = [ssAlt]) or (Shift = [ssShift]) then
          JedMain.FormKeyDown(nil, Key, Shift)
        else
          CamZ := CamZ + P3DStep;
      Ord('I'):
        if Shift = [] then
          JedMain.AddThingAtSurf
        else if Shift = [ssShift] then
          JedMain.AddThingAtXYZPYR(CamX, CamY, CamZ, -pch, 360 - yaw, 0.0)
        else
          JedMain.FormKeyDown(nil, Key, Shift);

      Ord('Z'):
        CamZ := CamZ - P3DStep;
      Ord('N'):
        GotoWPlayer(cwPlayer + 1, 1);
      Ord('P'):
        if Shift = [ssCtrl] then
          JedMain.FormKeyDown(nil, Key, Shift)
        else
          GotoWPlayer(cwPlayer - 1, -1);
      { < } 188:
        if Shift = [ssShift] then
          JedMain.ShiftTexture(st_up)
        else if Shift = [ssCtrl] then
          JedMain.RotateTexture(st_left)
        else if Shift = [ssAlt] then
          JedMain.ScaleTexture(st_down)
        else
          JedMain.ShiftTexture(st_left);
      { > } 190:
        if Shift = [ssShift] then
          JedMain.ShiftTexture(st_down)
        else if Shift = [ssCtrl] then
          JedMain.RotateTexture(st_right)
        else if Shift = [ssAlt] then
          JedMain.ScaleTexture(st_up)
        else
          JedMain.ShiftTexture(st_right);
      { ; } 186:
        JedMain.StartStitch;
      VK_INSERT:
        if Shift = [ssCtrl] then
          JedMain.StartStitch
        else
          JedMain.DoStitch;
      { ' } 222:
        JedMain.DoStitch;
      { / } 191:
        JedMain.StraightenTexture(ssCtrl in Shift, ssShift in Shift);

      Ord('S'):
        if (Shift = [ssShift]) or (Shift = []) then
          JedMain.FormKeyDown(nil, Key, Shift);
      VK_DELETE:
        if (Shift = [ssAlt]) or (Shift = []) then
          JedMain.FormKeyDown(nil, Key, Shift);
      Ord('X'), 219, 221, VK_Return:
        if Shift = [] then
          JedMain.FormKeyDown(nil, Key, Shift);
      Ord('F'):
        if Shift = [] Then
        begin
          clipboard.AsText := Sprintf('(%.5f/%.5f/%.5f:%.5f/%.5f/%.5f)',
            [CamX, CamY, CamZ, -pch, 360 - yaw, 0.0]);
        end;

    else
      exit;
    end;
  Invalidate;

end;

procedure TPreview3D.wmactivate(var message: TMessage);
begin
  if Render3D <> nil then
    Render3D.HandleActivate(message);
  inherited;
end;

procedure TPreview3D.wmpaint(var message: TMessage);
var
  r: trect;
  ps: tpaintstruct;
begin
  if Render3D <> nil then
  begin
    if getupdaterect(Handle, r, false) then
    begin
      beginpaint(Handle, ps);
      Render3D.HandlePaint(ps.hdc);
      endpaint(Handle, ps);
    end;
  end;
  inherited;
end;

Procedure TPreview3D.DeleteSector(s: TJKSector);
begin
  AddSecChange(s, 'D');
  Invalidate;
end;

Procedure TPreview3D.SysAddThing(th: TJKThing);
begin
  if not P3DThings then
    exit;
  try
    LoadThing3DO(th, false);
    Render3D.AddThing(th);
  except
    on E: Exception do
      PanMessage(mt_warning, E.message);
  end;
end;

Procedure TPreview3D.AddSecChange(sec: TJKSector; Change: Char);
var
  i: Integer;
begin
  case Change of
    'A':
      scs.AddObject(Change, sec);
    'C':
      begin
        i := scs.IndexOfObject(sec);
        if (i <> -1) and (scs[i] = 'C') then
          exit;
        scs.AddObject(Change, sec);
      end;
    'D':
      begin
        i := scs.IndexOfObject(sec);
        if i <> -1 then
          scs.delete(i);
        scs.AddObject(Change, sec);
      end;
  end;
end;

Procedure TPreview3D.AddThingChange(th: TJKThing; Change: Char);
var
  i: Integer;
begin
  { i:=ths.IndexOfObject(th);
    if i=-1 then ths.AddObject(Change,th) else
    ths[i]:=Change; }

  case Change of
    'A':
      ths.AddObject(Change, th);
    'C':
      begin
        i := ths.IndexOfObject(th);
        if (i <> -1) and (ths[i] = 'C') then
          exit;
        ths.AddObject(Change, th);
      end;
    'D':
      begin
        i := ths.IndexOfObject(th);
        if i <> -1 then
          ths.delete(i);
        ths.AddObject(Change, th);
      end;
  end;
end;

Procedure TPreview3D.ApplyChanges;
var
  i: Integer;
  sec: TJKSector;
  th: TJKThing;
  c: Char;
begin
  if Render3D = nil then
  begin
    scs.clear;
    ths.clear;
    exit;
  end;

  for i := 0 to scs.Count - 1 do
  begin
    sec := TJKSector(scs.Objects[i]);
    c := scs[i][1];
    case c of
      'A':
        SysAddSector(sec);
      'D':
        Render3D.DeleteSector(sec);
      'C':
        Render3D.UpdateSector(sec);
    end;
  end;

  scs.clear;

  for i := 0 to ths.Count - 1 do
  begin
    th := TJKThing(ths.Objects[i]);
    c := ths[i][1];
    case c of
      'A':
        SysAddThing(th);
      'D':
        Render3D.DeleteThing(th);
      'C':
        Render3D.UpdateThing(th);
    end;
  end;
  ths.clear;
end;

Procedure TPreview3D.SetThing3DO(th: TJKThing; a3do: TPJ3DO);
begin
  if Render3D <> nil then
    Render3D.SetThing3DO(th, a3do);
end;

Procedure TPreview3D.SysAddSector(s: TJKSector);
begin
  try
    Render3D.AddSector(s);
  except
    on E: Exception do
      PanMessage(mt_warning, E.message);
  end;
end;

Procedure TPreview3D.UpdateSector(s: TJKSector);
begin
  AddSecChange(s, 'C');
  Invalidate;
end;

Procedure TPreview3D.AddSector(s: TJKSector);
begin
  AddSecChange(s, 'A');
  Invalidate;
end;

Procedure TPreview3D.UpdateThing(th: TJKThing);
begin
  AddThingChange(th, 'C');
  Invalidate;
end;

Procedure TPreview3D.AddThing(th: TJKThing);
begin
  AddThingChange(th, 'A');
  Invalidate;
end;

Procedure TPreview3D.DeleteThing(th: TJKThing);
begin
  AddThingChange(th, 'D');
  Invalidate;
end;

procedure TPreview3D.FormPaint(Sender: TObject);
begin
  ApplyChanges;
  Render3D.redraw;
end;

Function TPreview3D.IsActive: boolean;
begin
  Result := Render3D <> nil;
end;

Procedure TPreview3D.SetCam(x, y, z, pch, yaw: double);
begin
  if Render3D = nil then
    exit;
  Render3D.CamX := x;
  Render3D.CamY := y;
  Render3D.CamZ := z;
  Render3D.pch := -pch;
  Render3D.yaw := -yaw;
  Invalidate;
end;

procedure TPreview3D.Close1Click(Sender: TObject);
begin
  Close;
end;

Procedure TPreview3D.SaveCamPos(var cpos: TCamPos);
begin
  cpos.x := Render3D.CamX;
  cpos.y := Render3D.CamY;
  cpos.z := Render3D.CamZ;
  cpos.pch := Render3D.pch;
  cpos.yaw := Render3D.yaw;
end;

Procedure TPreview3D.RestoreCamPos(const cpos: TCamPos);
begin
  Render3D.CamX := cpos.x;
  Render3D.CamY := cpos.y;
  Render3D.CamZ := cpos.z;
  Render3D.pch := cpos.pch;
  Render3D.yaw := cpos.yaw;
end;

procedure TPreview3D.Settings1Click(Sender: TObject);
var
  cpos: TCamPos;
begin
  With Options do
  begin
    if not SetOptions(PPreview) then
      exit;
    SaveCamPos(cpos);
    self.Hide;
    ShowPreview;
    RestoreCamPos(cpos);
  end;
end;

procedure TPreview3D.FormMouseUp(Sender: TObject; Button: TMouseButton;
  Shift: TShiftState; x, y: Integer);
var
  i, f: Integer;
begin
  if Render3D = nil then
    exit;
  With Render3D do
    Case PickAt(x, y) of
      pk_nothing:
        exit;
      pk_surface:
        begin
          i := Level.Sectors.IndexOf(selSC);
          if i <> -1 then
          begin
            JedMain.SetMapMode(MM_SF);
            JedMain.SetCurSF(i, SelSF);
            if Shift = [ssShift] then
              JedMain.DO_MultiSelect
            else
              JedMain.ClearMultiSelection;
          end
          else
            PanMessage(mt_warning,
              '3D preview is out of sync with the level! Reload it');
        end;
      pk_thing:
        begin
          i := Level.Things.IndexOf(selTH);
          if i <> -1 then
          begin
            JedMain.SetMapMode(MM_TH);
            JedMain.SetCurTH(i);
            if Shift = [ssShift] then
              JedMain.DO_MultiSelect;
          end
          else
            PanMessage(mt_warning,
              '3D preview is out of sync with the level! Reload it');
        end;
    end;
end;

procedure TPreview3D.FormClose(Sender: TObject; var Action: TCloseAction);
begin
  P3DOnTop := GetStayOnTop(self);
  GetP3DPos(self, P3DX, P3DY, P3DWinSize);
end;

procedure TPreview3D.FormDblClick(Sender: TObject);
begin
  case JedMain.Map_mode of
    MM_SF:
      ItemEdit.DoPickTexture;
    MM_TH:
      ItemEdit.DoPickThing;
  end;
end;

procedure TPreview3D.FormCreate(Sender: TObject);
begin
  scs := TStringList.Create;
  ths := TStringList.Create;

  AddKBItem(miControl, 'Rotate right', Char(VK_RIGHT), []);
  AddKBItem(miControl, 'Rotate left', Char(VK_LEFT), []);
  AddKBItem(miControl, 'Move forth', Char(VK_UP), []);
  AddKBItem(miControl, 'Move back', Char(VK_DOWN), []);
  AddKBItem(miControl, 'Sidestep left', Char(VK_LEFT), [ssAlt]);
  AddKBItem(miControl, 'Sidestep right', Char(VK_RIGHT), [ssAlt]);

  AddKBItem(miControl, 'Look up', Char(VK_NEXT), []);
  AddKBItem(miControl, 'Look down', Char(VK_PRIOR), []);
  AddKBItem(miControl, 'Move up', 'A', []);
  AddKBItem(miControl, 'Move down', 'Z', []);

  AddKBItem(miControl, 'Select surface/thing'#9'Click', #0, []);
  AddKBItem(miControl, 'Change texture/template'#9'Double-Click', #0, []);

  AddKBItem(miControl, 'Snap grid to item', 'S', [ssShift]);
  AddKBItem(miControl, 'Snap grid to item', 'W', [ssShift]);
  AddKBItem(miControl, 'Swap Grid axis', 'G', [ssShift]);
  AddKBItem(miControl, 'Sector mode', 'S', []);
  AddKBItem(miControl, 'Item Editor', Chr(VK_Return), []);

  AddKBItem(miControl, 'Next player start', 'N', []);
  AddKBItem(miControl, 'Prev player start', 'P', []);

  AddKBItem(miTex, 'Scroll up', #188, [ssShift]);
  AddKBItem(miTex, 'Scroll down', #190, [ssShift]);

  AddKBItem(miTex, 'Scroll left', #188, []);
  AddKBItem(miTex, 'Scroll right', #190, []);

  AddKBItem(miTex, 'Start stitch', Char(VK_INSERT), [ssCtrl]);
  AddKBItem(miTex, 'Start stitch', #186, []);

  AddKBItem(miTex, 'Stitch', Char(VK_INSERT), [ssShift]);
  AddKBItem(miTex, 'Stitch', #222, []);

  AddKBItem(miTex, 'Straighten Texture', Char(VK_HOME), [ssAlt]);
  AddKBItem(miTex, 'Straighten Texture', #191, []);
  AddKBItem(miTex, 'Scale texture down', #188, [ssAlt]);
  AddKBItem(miTex, 'Scale texture Up', #190, [ssAlt]);
  AddKBItem(miTex, 'Straighten/zero/rotate90 texture', #191, [ssShift, ssCtrl]);
  AddKBItem(miTex, 'Straighten/zero/rotate90 texture', Char(VK_HOME),
    [ssShift, ssCtrl]);
  AddKBItem(miTex, 'Straighten/zero texture', #191, [ssCtrl]);
  AddKBItem(miTex, 'Straighten/zero texture', Char(VK_HOME), [ssCtrl]);
  AddKBItem(miTex, 'Rotate texture left', #188, [ssCtrl]);
  AddKBItem(miTex, 'Rotate texture right', #190, [ssCtrl]);

  AddKBItem(miEdit, 'Delete surface', Char(VK_DELETE), [ssAlt]);
  AddKBItem(miEdit, 'Invert surface', 'I', [ssAlt]);
  AddKBItem(miEdit, 'Planarize surface', 'P', [ssCtrl]);

  AddKBItem(miEdit, 'Assign Thing to Sector', 'A', [ssShift]);
  AddKBItem(miEdit, 'Unadjoin', 'A', [ssAlt]);
  AddKBItem(miEdit, 'Adjoin', 'A', [ssCtrl]);

  AddKBItem(miEdit, 'Raise item', Char(VK_UP), [ssCtrl]);
  AddKBItem(miEdit, 'Lower item', Char(VK_DOWN), [ssCtrl]);
  AddKBItem(miEdit, 'Raise item', #219, []);
  AddKBItem(miEdit, 'Lower item', #221, []);

  AddKBItem(miEdit, 'Delete item', Char(VK_DELETE), []);
  AddKBItem(miEdit, 'Extrude surface', 'X', []);
  AddKBItem(miEdit, 'Copy camera as frame', 'F', []);
  AddKBItem(miEdit, 'Insert thing at surface', 'I', []);
  AddKBItem(miEdit, 'Insert thing at camera', 'I', [ssShift]);

end;

procedure TPreview3D.SetViewcamera1Click(Sender: TObject);
begin
  With Render3D do
    JedMain.SetCam(CamX, CamY, CamZ, 0, 0, 0);
end;

Procedure TPreview3D.AddKBItem(mi: TMenuItem; const name: string; c: Char;
  sc: TShiftState);
var
  nmi: TMenuItem;
  ext: string;
begin
  nmi := TMenuItem.Create(mi);
  nmi.OnClick := KBCommandClick;
  case c of
    #0:
      ext := '';
    Char(VK_ADD):
      ext := #9'+';
    Char(VK_SUBTRACT):
      ext := #9'-';
    Char(VK_MULTIPLY):
      ext := #9'*';
  else
    ext := #9 + ShortCutToText(ShortCut(Ord(c), sc));
  end;

  nmi.Caption := name + ext;
  nmi.Tag := ShortCut(Ord(c), sc);
  mi.Add(nmi);
end;

procedure TPreview3D.KBCommandClick(Sender: TObject);
var
  Key: Word;
  sc: TShiftState;
begin
  with (Sender as TMenuItem) do
  begin
    ShortCutToKey(Tag, Key, sc);
    FormKeyDown(self, Key, sc);
  end;
end;

procedure TPreview3D.ToggleFullyLit1Click(Sender: TObject);
var
  cpos: TCamPos;
begin
  P3DFullLit := not P3DFullLit;
  SaveCamPos(cpos);
  self.Hide;
  ShowPreview;
  RestoreCamPos(cpos);
end;

Procedure TPreview3D.GetCam(var x, y, z, pch, yaw: double);
begin
  x := 0;
  y := 0;
  z := 0;
  pch := 0;
  yaw := 0;
  if Render3D = nil then
    exit;
  x := Render3D.CamX;
  y := Render3D.CamY;
  z := Render3D.CamZ;
  pch := -Render3D.pch;
  yaw := -Render3D.yaw;
end;

end.
