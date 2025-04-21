unit U_Preview;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls,
  Forms, Dialogs, PRenderGL, PRenderDX, J_Level, Geometry, StdCtrls,
  GlobalVars, Menus, Prender, Clipbrd,
  u_pj3dos, u_templates;

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
    miControl: TMenuItem;
    miEdit: TMenuItem;
    miTex: TMenuItem;
    MenuRenderFullyLit: TMenuItem;
    Reload: TMenuItem;
    MenuRenderFog: TMenuItem;
    MenuHighlightFloor: TMenuItem;
    MenuHighlightSolidSurface: TMenuItem;
    MenuHighlightSpecialSurfaces: TMenuItem;
    N1: TMenuItem;
    SetCameraToItem1: TMenuItem;
    N2: TMenuItem;
    N3: TMenuItem;
    MenuHighlightSelection: TMenuItem;
    MenuNoClipSelectionOutline: TMenuItem;
    MenuSetThingToCamera: TMenuItem;

    procedure FormShow(Sender: TObject);
    procedure FormHide(Sender: TObject);
    procedure FormKeyDown(Sender: TObject; var Key: Word; Shift: TShiftState);
    procedure FormPaint(Sender: TObject);
    procedure Close1Click(Sender: TObject);
    procedure Settings1Click(Sender: TObject);
    procedure FormMouseMove(Sender: TObject; Shift: TShiftState; x, y: Integer);
    procedure FormMouseDown(Sender: TObject; Button: TMouseButton;
      Shift: TShiftState; x, y: Integer);
    procedure FormMouseUp(Sender: TObject; Button: TMouseButton;
      Shift: TShiftState; x, y: Integer);
    procedure FormClose(Sender: TObject; var Action: TCloseAction);
    procedure FormDblClick(Sender: TObject);
    procedure FormCreate(Sender: TObject);
    procedure SetViewcamera1Click(Sender: TObject);
    procedure MenuRenderFullyLitClick(Sender: TObject);
    procedure FormMouseWheel(Sender: TObject; Shift: TShiftState;
      WheelDelta: Integer; MousePos: TPoint; var Handled: Boolean);
    procedure ReloadClick(Sender: TObject);
    procedure MenuHighlightFloorClick(Sender: TObject);
    procedure MenuRenderFogClick(Sender: TObject);
    procedure MenuHighlightSolidSurfaceClick(Sender: TObject);
    procedure MenuHighlightSpecialSurfacesClick(Sender: TObject);
    procedure SetCameraToItem1Click(Sender: TObject);
    procedure MenuHighlightSelectionClick(Sender: TObject);
    procedure MenuNoClipSelectionOutlineClick(Sender: TObject);
    procedure MenuSetThingToCameraClick(Sender: TObject);
    procedure Commands1Click(Sender: TObject);
    procedure FormActivate(Sender: TObject);
    procedure FormBeforeMonitorDpiChanged(Sender: TObject; OldDPI,
      NewDPI: Integer);
    procedure FormAfterMonitorDpiChanged(Sender: TObject; OldDPI,
      NewDPI: Integer);

  private
    { Private declarations }
    render3D: TPreviewRender;
    cwPlayer: Integer;
    //renderFog: Boolean;
    viewMode: Boolean;
    mousePos: TPoint;

    scs: TStringList; { Lists of changed,added,deleted sectors and things }
    ths: TStringList;

    Procedure WMEraseBkg(var msg: TMessage); message WM_ERASEBKGND;
    procedure wmactivate(var message: TMessage); message wm_activate;
    procedure wmpaint(var message: TMessage); message wm_paint;
    Procedure SysAddSector(sec: TJKSector);
    Procedure SysUpdateSector(sec: TJKSector);
    Procedure SysAddThing(th: TJKThing);
    Procedure SysUpdateThing(th: TJKThing);
    Procedure SaveCamPos(var cpos: TCamPos);
    Procedure RestoreCamPos(const cpos: TCamPos);
    Procedure AddSecChange(sec: TJKSector; Change: Char);
    Procedure AddThingChange(th: TJKThing; Change: Char);
    Procedure ApplyChanges;
    Function AddKBItem(mi: TMenuItem; const name: string; c: Char; sc: TShiftState): TMenuItem; overload;
    Function AddKBItem(mi: TMenuItem; const name: string; c: Char; sc: TShiftState; onClick: TNotifyEvent): TMenuItem; overload;
    procedure KBCommandClick(Sender: TObject);

    Procedure MoveCameraForward;
    Procedure MoveCameraBackward;
    Procedure MoveCameraLeft;
    Procedure MoveCameraRight;
    Procedure MoveCameraUp;
    Procedure MoveCameraDown;

    Procedure TranslateSelectedThings(dx, dy, dz: double; localForward: boolean = true);
    Procedure MoveThingForward;
    Procedure MoveThingBackward;
    Procedure MoveThingLeft;
    Procedure MoveThingRight;
    Procedure MoveThingUp;
    Procedure MoveThingDown;

    Function PlaySectorSound(filename: string): Boolean;  // Note, not providing filename will continue playing existing one
    Function RestartSectorSound: Boolean;
    Procedure SetSectorSoundVolume(volume: float);
    Procedure PauseSectorSound;
    Procedure ResumeSectorSound;
    Procedure StopSectorSound;

    procedure WMActivateApp(var Msg: TWMActivateApp); message WM_ACTIVATEAPP;

  public
    { Public declarations }
    Procedure UpdateSector(s: TJKSector);
    Procedure AddSector(s: TJKSector);
    Procedure DeleteSector(s: TJKSector);

    Procedure UpdateThing(th: TJKThing);
    Procedure SetThing3DO(th: TJKThing; a3do: TPJ3DO);
    Procedure AddThing(th: TJKThing);
    Procedure DeleteThing(th: TJKThing);

    Procedure Reset; // removes all meshes
    Procedure ReloadLevel;
    Procedure EnableFogRendering(enable: boolean);
    Procedure UpdateFog;
    Procedure ShowPreview;
    Function IsActive: boolean;
    Procedure SetCam(x, y, z, pch, yaw: double);
    Procedure GoToPlayer(wPlayer: Integer; step: Integer);
    Procedure GetCam(var x, y, z, pch, yaw: double);
  end;

var
  Preview3D: TPreview3D;

implementation

uses Jed_Main, FileOperations, U_Options, Misc_utils, Item_edit, U_tbar,
     ProgressDialog, lev_utils, u_undo;

{$R *.DFM}

Procedure TPreview3D.Reset;
begin
  MenuRenderFog.Visible := (CurrentProject = TProjectType.IJIM);
  if render3D = nil then
    exit;

  scs.clear;
  ths.clear;

  StopSectorSound();

  var campos, camrot: TVector;
  with render3D.GetCamera do
    begin
      campos := position;
      camrot := rotation;
    end;

  render3D.Reset;
  render3D.GetCamera.MoveTo(campos, camrot);
  render3D.SetGamma(P3DGamma);
  render3D.highlightSelection      := MenuHighlightSelection.Checked;
  render3D.noClipSelectionOutline  := P3DNoClipSelectionOutline;
  render3D.highlightSolidSurface   := MenuHighlightSolidSurface.Checked;
  render3D.highlightSolidFloor     := MenuHighlightFloor.Checked;
  render3D.highlightSpecialSurface := MenuHighlightSpecialSurfaces.Checked;
  UpdateFog;
end;

procedure TPreview3D.ReloadClick(Sender: TObject);
begin
   ReloadLevel;
end;

Procedure TPreview3D.ReloadLevel;
var
  i: Integer;
  sec: TJKSector;
  th: TJKThing;
begin
  if render3D = nil then
    exit;

  Reset;

  Progress.Reset(Level.Sectors.Count + Level.Things.Count);
  Progress.msg := 'Loading sectors...';
  for i := 0 to level.sectors.Count - 1 do
  begin
    Progress.step;
    sec := level.sectors[i];
    if not P3DVisLayers then
      SysAddSector(sec)
    else if ToolBar.IsLayerVisible(sec.layer) then
      SysAddSector(sec);
  end;

  Progress.msg := 'Loading things...';
  for i := 0 to level.things.Count - 1 do
  begin
    Progress.step;
    th := level.things[i];
    if not P3DVisLayers then
      SysAddThing(th)
    else if ToolBar.IsLayerVisible(th.layer) then
      SysAddThing(th);
  end;

  Progress.Hide;
  Invalidate;
end;

Procedure TPreview3D.EnableFogRendering(enable: boolean);
begin
  MenuRenderFog.Checked := enable;

  if render3D = nil then
    exit;

  UpdateFog();
  Invalidate;
end;

Procedure TPreview3D.UpdateFog;
begin
  if render3D = nil then
    exit;
  if (CurrentProject = TProjectType.IJIM) and MenuRenderFog.Checked and render3D.IsFogSupported() then
    begin
      render3D.EnableFog(Level.header.Fog.Enabled);
      render3D.SetFog(Level.header.Fog.Color, level.header.Fog.FogStart, level.header.fog.FogEnd, (*density=*)1.0)
    end
  else
    begin
      render3D.EnableFog(false);
    end;
end;

Procedure TPreview3D.GoToPlayer(wPlayer: Integer; step: Integer);
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
    var ttype := Templates.GetTPLField(th.name, 'type');
    if (ttype <> nil) and (CompareText(ttype.AsString, 'player') = 0) then
    //if (compareText(th.name, 'walkplayer') = 0) or (compareText(th.name, 'shirtplayer') = 0) then
      With th do
      begin
        Render3D.SetViewToThing(th, (*zoomOut=*)0.135, (*pitch=*)17);
        //Render3D.CamZ := Render3D.CamZ + 0.05;
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
      Show;
      if Render3D = nil then
        begin
          Hide;
          exit;
        end;

      ReloadLevel;
      GoToPlayer(0, 1);
    end;
end;

procedure TPreview3D.FormShow(Sender: TObject);
begin
  PixelsPerInch := Screen.PixelsPerInch;
  Scaled := True;
  case P3DAPI of
    P3D_OGL:
      Render3D := TOGLPRenderer.Create(self);
  else
    Render3D := TD3D5PRenderer.Create(self);
    //Render3D := TD3DRenderer.Create(self);
  end;

  Try
    Render3D.Initialize;
    Render3D.SetClearColor(MakeColor(clP3DBack));
    Render3D.selectionColor               := MakeColor(clP3DSel);
    Render3D.multipleSelectionColor       := MakeColor(clP3DMultiSel);
    Render3D.floorHighlightColor          := MakeColor(clP3DSolidFloor);
    Render3D.solidSurfaceHighlightColor   := MakeColor(clP3DSolidSurface);
    Render3D.specialSurfaceHighlightColor := MakeColor(clP3DSpecialSurface);
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
  StopSectorSound();
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

  with Render3D do
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
            self.MoveCameraLeft;
          end
        else
          GetCamera().RotateYaw(-10);
      VK_RIGHT:
        if Shift = [ssShift] then
          JedMain.ShiftTexture(st_right)
        else if Shift = [ssCtrl] then
          JedMain.RotateTexture(st_right)
        else if ssAlt in Shift then
          begin
            self.MoveCameraRight;
          end
        else
          GetCamera().RotateYaw(10);
      VK_UP:
        if Shift = [ssShift] then
          JedMain.ShiftTexture(st_up)
        else if Shift = [ssCtrl] then
          JedMain.RaiseObject(ro_up)
        else
        begin
          self.MoveCameraForward;
        end;
      VK_DOWN:
        if Shift = [ssShift] then
          JedMain.ShiftTexture(st_down)
        else if Shift = [ssCtrl] then
          JedMain.RaiseObject(ro_down)
        else
          self.MoveCameraBackward;
      VK_NEXT:
          GetCamera().RotatePitch(10);
      VK_PRIOR:
        begin
          GetCamera().RotatePitch(-10);
        end;
      VK_HOME:
        if Shift = [] then
          GetCamera().rotation.pitch := 0
        else
          JedMain.StraightenTexture(ssCtrl in Shift, ssShift in Shift);

      Ord('A'):
        if viewMode then
          self.MoveCameraLeft
        else if Shift = [ssShift] then
          self.MoveThingLeft
        else if (Shift = [ssAlt]) then // Un-adjoin
          JedMain.FormKeyDown(nil, Key, Shift)
        else
          self.MoveCameraUp;
      Ord('B'):
        if Shift = [] then  // Bring Thing to surface
          JedMain.FormKeyDown(nil, Key, Shift);

      Ord('C'):
        if Shift = [ssCtrl] then  // Copy
          JedMain.FormKeyDown(nil, Key, Shift)
        else if Shift = [ssShift] then  // Cleave by Grid
          JedMain.FormKeyDown(nil, Key, Shift);
      Ord('D'):
        if viewMode then
          self.MoveCameraRight
        else if Shift = [ssShift] then
          self.MoveThingRight;
      Ord('E'):
        if viewMode then
          self.MoveCameraUp
        else if Shift = [ssShift] then
          self.MoveThingUp;
      Ord('I'):
        if Shift = [] then
          JedMain.AddThingOnCurSurf
        else if Shift = [ssShift] then
          with GetCamera() do
            JedMain.AddThingAtXYZPYR(position.x, position.y, position.z, -rotation.pitch, 360 - rotation.yaw, 0.0)
        else
          JedMain.FormKeyDown(nil, Key, Shift);
      Ord('J'):
        if Shift = [] then  // Join surface/sector
          JedMain.FormKeyDown(nil, Key, Shift);
      Ord('M'):
        if Shift = [] then  // Merge
          JedMain.FormKeyDown(nil, Key, Shift);
      Ord('Z'):
        self.MoveCameraDown;
      Ord('N'):
        GoToPlayer(cwPlayer + 1, 1);
      Ord('P'):
        if Shift = [ssCtrl] then
          JedMain.FormKeyDown(nil, Key, Shift)
        else
          GoToPlayer(cwPlayer - 1, -1);
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
          JedMain.DoStitch(ssCtrl in shift);
      { ' } 222:
        JedMain.DoStitch(ssCtrl in shift);
      { / } 191:
        JedMain.StraightenTexture(ssCtrl in Shift, ssShift in Shift);

      Ord('Q'):
        if viewMode then
          self.MoveCameraDown
        else if Shift = [ssShift] then
          self.MoveThingDown;

      Ord('S'):
        begin
          if Shift = [ssAlt] then
            begin
              with render3D.GetCamera do
                if sector <> nil then
                begin
                  JedMain.Cur_SC := sector.num;
                  JedMain.SetMapMode(MM_SC);
                end;
            end
          else if viewMode then
            self.MoveCameraBackward
          else if (Shift = [ssShift]) and (JedMain.mapMode = MM_TH) then
            self.MoveThingBackward
          else // pass on to main window
            JedMain.FormKeyDown(nil, Key, Shift);
        end;

      Ord('T'):
        if (Shift = []) then // allow only setting map mode to thing
          JedMain.FormKeyDown(nil, Key, Shift);

      Ord('V'):
        if (Shift = [ssCtrl]) or (Shift = [ssShift, ssCtrl]) then  // Paste
          JedMain.FormKeyDown(nil, Key, Shift);

      Ord('W'):
        if viewMode then
          self.MoveCameraForward
        else if Shift = [ssShift] then
          self.MoveThingForward;

      VK_DELETE:
        if (Shift = [ssAlt]) or (Shift = []) then
          JedMain.FormKeyDown(nil, Key, Shift);
      Ord('X'), 219, 221, VK_RETURN:
        if (Shift = []) or (key = Ord('X'))  then
          JedMain.FormKeyDown(nil, Key, Shift);
      Ord('F'):
        if Shift = [] Then
        begin
          with GetCamera() do
            clipboard.AsText := Sprintf('(%1.5f/%1.5f/%1.5f:%1.5f/%1.5f/%1.5f)',
              [position.x, position.y, position.z, -rotation.pitch, 360 - rotation.yaw, 0.0]);
        end;

      VK_BACK:
         JedMain.ClearMultiSelection;
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
    if not P3DVisLayers or ToolBar.IsLayerVisible(th.layer) then
    begin
      LoadThing3DO(th, false);
      Render3D.AddThing(th);
    end;
  except
    on E: Exception do
      PanMessage(mt_warning, E.message);
  end;
end;

Procedure TPreview3D.SysUpdateThing(th: TJKThing);
begin
   if not P3DVisLayers or ToolBar.IsLayerVisible(th.layer) then
    Render3D.SyncThing(th)
  else
    Render3D.DeleteThing(th)
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
          scs.Delete(i);
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
    scs.Clear;
    ths.Clear;
    exit;
  end;

  for i := 0 to scs.Count - 1 do
  begin
    sec := TJKSector(scs.Objects[i]);
    c := scs[i][1];
    case c of
      'A':
        SysAddSector(sec);
      'C':
        SysUpdateSector(sec);
      'D':
        Render3D.DeleteSector(sec);
    end;
  end;
  scs.Clear;

  for i := 0 to ths.Count - 1 do
  begin
    th := TJKThing(ths.Objects[i]);
    c := ths[i][1];
    case c of
      'A':
        SysAddThing(th);
      'C':
        SysUpdateThing(th);
      'D':
        Render3D.DeleteThing(th);
    end;
  end;
  ths.Clear;
end;

Procedure TPreview3D.SetThing3DO(th: TJKThing; a3do: TPJ3DO);
begin
  if Render3D <> nil then
    Render3D.SetThing3DO(th, a3do);
end;

Procedure TPreview3D.SysAddSector(sec: TJKSector);
begin
  try
    if not P3DVisLayers or ToolBar.IsLayerVisible(sec.layer) then
      Render3D.AddSector(sec);
  except
    on E: Exception do
      PanMessage(mt_warning, E.message);
  end;
end;

Procedure TPreview3D.SysUpdateSector(sec: TJKSector);
begin
  if not P3DVisLayers or ToolBar.IsLayerVisible(sec.layer) then
  begin
    Render3D.SyncSector(sec)
  end
  else
    Render3D.DeleteSector(sec)
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
  Render3D.Redraw;

  // Ppdate cur playing sector sound
  with Render3D do
    begin
      var sec := GetCamera().sector;
      if not P3DPlaySecSound or (sec = nil) or (Length(sec.sound) = 0) then
        StopSectorSound
      else
        begin
          PlaySectorSound(sec.sound);
          SetSectorSoundVolume(sec.soundVolume);
        end;
    end;
end;

Function TPreview3D.IsActive: boolean;
begin
  Result := Render3D <> nil;
end;

Procedure TPreview3D.SetCam(x, y, z, pch, yaw: double);
begin
  if Render3D = nil then
    exit;

  with Render3D.GetCamera() do
  begin
    position.x := x;
    position.y := y;
    position.z := z;
    rotation.pitch := -pch;
    rotation.yaw   := -yaw;
  end;
  Invalidate;
end;

procedure TPreview3D.SetCameraToItem1Click(Sender: TObject);
var
    cx, cy, cz, cpch, cyaw, crol: double;
begin
    cpch := 0;
    cyaw := 0;
    case JedMain.mapMode of
        MM_SC:
            FindCenter(level.sectors[JedMain.Cur_SC], cx, cy, cz);
        { MM_VX: with Level.Sectors[Cur_SC].Vertices[Cur_VX] do begin cx:=x; cy:=y; cz:=z; end; }
        MM_SF,
        MM_ED:
            CalcSurfCenter(level.sectors[JedMain.Cur_SC].surfaces[JedMain.Cur_SF], cx, cy, cz);
        MM_TH:
            with level.things[JedMain.Cur_TH] do
              begin
                  cx := X;
                  cy := Y;
                  cz := z;
                  cpch := pch;
                  cyaw := yaw;
              end;
        MM_LT:
            with level.lights[JedMain.Cur_LT] do
              begin
                  cx := position.X;
                  cy := position.Y;
                  cz := position.z;
              end;
        MM_FR:
          begin
            if JedMain.Cur_FR = -1 then
              with level.things[JedMain.Cur_TH] do
                begin
                  cx := x;
                  cy := y;
                  cz := z;
                  cpch := pch;
                  cyaw := yaw;
                end
            else
              level.things[JedMain.Cur_TH].Vals[JedMain.Cur_FR]
                .GetFrame(cx, cy, cz, cpch, cyaw, crol);
          end;
        else
          exit;
    end;

    SetCam(cx, cy, cz, cpch, cyaw);
end;

procedure TPreview3D.MenuSetThingToCameraClick(Sender: TObject);
begin
   case JedMain.mapMode of
      MM_TH:
        begin
          var curth := level.things[JedMain.Cur_TH];
          with Render3D.GetCamera() do
          begin
            var dx := position.x - curth.X;
            var dy := position.y - curth.Y;
            var dz := position.z - curth.Z;
            TranslateSelectedThings(dx, dy, dz, {localForward=}false);
          end;
        end;
   end;
end;

procedure TPreview3D.Close1Click(Sender: TObject);
begin
  Close;
end;

procedure TPreview3D.Commands1Click(Sender: TObject);
begin
    MenuSetThingToCamera.Enabled := false;
    if JedMain.mapMode = MM_TH then
      MenuSetThingToCamera.Enabled := true;
end;

Procedure TPreview3D.SaveCamPos(var cpos: TCamPos);
begin
  with Render3D.GetCamera() do
  begin
    cpos.x := position.x;
    cpos.y := position.y;
    cpos.z := position.z;
    cpos.pch := rotation.pitch;
    cpos.yaw := rotation.yaw;
  end;
end;

Procedure TPreview3D.RestoreCamPos(const cpos: TCamPos);
begin
  with Render3D.GetCamera() do
  begin
    position.x := cpos.x;
    position.y := cpos.y;
    position.z := cpos.z;
    rotation.pitch := cpos.pch;
    rotation.yaw := cpos.yaw;
  end;
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

procedure TPreview3D.FormMouseMove(Sender: TObject; Shift: TShiftState; X, Y: Integer);
begin

   if viewMode then     
      begin
        if Render3D = nil then
          exit;
        With Render3D.GetCamera() do
          begin
            var dx := x - mousePos.x;
            //yaw := yaw + dx;
            RotateYaw(dx);
//            if yaw > 360 then
//              yaw := yaw - 360
//            else if yaw < 0 then
//                yaw := yaw + 360;

            var dy := y - mousePos.y;
            RotatePitch(dy);
//            pch := pch + dy;
//            if pch > 90 then
//               pch := 90
//            else if pch < -90 then
//               pch := -90;

            Invalidate;
          end;
      end;
//      else if (ssLeft in Shift) and (ssCtrl in Shift) then
//        JedMain.FormMouseMove(Sender, Shift, X, Y);


   mousePos.x := x;
   mousePos.y := y;
end;

procedure TPreview3D.FormMouseDown(Sender: TObject; Button: TMouseButton;
  Shift: TShiftState; x, y: Integer);
begin
  mousePos.x := x;
  mousePos.y := y;
  case Button of
    mbLeft: ;
    mbMiddle: ;
    mbRight:
       begin
          viewMode := True;
          Screen.Cursor := crEye;
       end;
  end;
end;

procedure TPreview3D.FormMouseUp(Sender: TObject; Button: TMouseButton;
  Shift: TShiftState; x, y: Integer);
var
  i, f: Integer;
begin
  case Button of
    mbLeft:
      begin
        if Render3D = nil then
          exit;
        with Render3D do
          case PickAt(x, y) of
            pk_nothing:
              exit;
            pk_surface:
              begin
                i := level.sectors.IndexOf(selSC);
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
                i := level.things.IndexOf(selTH);
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
      end; // mbLeft
    mbRight:
      begin
        if viewMode then Screen.Cursor := crDefault;
        viewMode := False;
      end;
  end;
end;

procedure TPreview3D.FormMouseWheel(Sender: TObject; Shift: TShiftState;
  WheelDelta: Integer; MousePos: TPoint; var Handled: Boolean);
begin
    if viewMode then
      begin
        if WheelDelta > 0 then
          toolbar.UpdateP3DStep(0.005)
        else if WheelDelta < 0 then
          toolbar.UpdateP3DStep(-0.005);
        Handled := true;
      end
    else if Shift = [ssShift] then
      begin
      if WheelDelta > 0 then
          toolbar.UpdatePerpStep(0.001)
        else if WheelDelta < 0 then
          toolbar.UpdatePerpStep(-0.001);
        Handled := true;
      end
    else if Shift <> [] then  // prevents map zoom
      JedMain.FormMouseWheel(Sender, Shift, WheelDelta, MousePos, Handled);
end;

procedure TPreview3D.FormClose(Sender: TObject; var Action: TCloseAction);
begin
  P3DOnTop := GetStayOnTop(self);
  GetP3DPos(self, P3DX, P3DY, P3DWinSize);
end;

procedure TPreview3D.FormDblClick(Sender: TObject);
begin
  case JedMain.mapMode of
    MM_SF:
      ItemEdit.DoPickTexture;
    MM_TH:
      ItemEdit.DoPickThing;
  end;
end;

procedure TPreview3D.FormActivate(Sender: TObject);
begin
  ResumeSectorSound;
end;

procedure TPreview3D.FormAfterMonitorDpiChanged(Sender: TObject; OldDPI,
  NewDPI: Integer);
begin
   SetP3DRes(self, P3DWinSize);
   Invalidate;
end;

procedure TPreview3D.FormBeforeMonitorDpiChanged(Sender: TObject; OldDPI,
  NewDPI: Integer);
begin
  ScaleBy(OldDPI, NewDPI);
end;

procedure TPreview3D.FormCreate(Sender: TObject);
begin
  scs := TStringList.Create;
  ths := TStringList.Create;

  // init checkbox based on stored settings
  MenuNoClipSelectionOutline.Checked := P3DNoClipSelectionOutline;

  AddKBItem(miControl, 'View mode'#9'Right Mouse Button + W/A/S/D', #0, []);
  AddKBItem(miControl, 'Rotate right', Char(VK_RIGHT), []);
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
  AddKBItem(miControl, 'Clear multiselection', Char(VK_BACK), []);
  AddKBItem(miControl, 'Change texture/template'#9'Double-Click', #0, []);

  AddKBItem(miControl, 'Sector mode', 'S', []);
  AddKBItem(miControl, 'Select camera sector', 'S', [ssAlt]);
  AddKBItem(miControl, 'Item Editor', Char(VK_Return), []);

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
  AddKBItem(miTex, 'Stitch && Copy Surface Properties', Char(VK_INSERT), [ssShift, ssCtrl]);
  AddKBItem(miTex, 'Stitch && Copy Surface Properties', #222, [ssCtrl]);

  AddKBItem(miTex, 'Straighten Texture', Char(VK_HOME), [ssAlt]);
  AddKBItem(miTex, 'Straighten Texture', #191, []);
  AddKBItem(miTex, 'Scale texture down', #188, [ssAlt]);
  AddKBItem(miTex, 'Scale texture Up', #190, [ssAlt]);
  AddKBItem(miTex, 'Straighten/zero/rotate90 texture', #191, [ssShift, ssCtrl]);
  AddKBItem(miTex, 'Straighten/zero/rotate90 texture', Char(VK_HOME), [ssShift, ssCtrl]);
  AddKBItem(miTex, 'Straighten/zero texture', #191, [ssCtrl]);
  AddKBItem(miTex, 'Straighten/zero texture', Char(VK_HOME), [ssCtrl]);
  AddKBItem(miTex, 'Rotate texture left', #188, [ssCtrl]);
  AddKBItem(miTex, 'Rotate texture right', #190, [ssCtrl]);

  AddKBItem(miEdit, 'Delete surface', Char(VK_DELETE), [ssAlt]);
  AddKBItem(miEdit, 'Invert surface', 'I', [ssAlt]);
  AddKBItem(miEdit, 'Planarize surface', 'P', [ssCtrl]);
  AddKBItem(miEdit, 'Fix solid surface(s)', 'R', [ssCtrl], JedMain.OnFixSolidSurfaces);

  AddKBItem(miEdit, 'Unadjoin', 'A', [ssAlt]);
  AddKBItem(miEdit, 'Adjoin', 'A', [ssCtrl]);
  AddKBItem(miEdit, 'Join sectors/surfaces', 'J', []);
  AddKBItem(miEdit, 'Merge', 'M', []);

  AddKBItem(miEdit, 'Move Thing left', 'A', [ssShift]);
  AddKBItem(miEdit, 'Move Thing right', 'D', [ssShift]);
  AddKBItem(miEdit, 'Move Thing forward', 'W', [ssShift]);
  AddKBItem(miEdit, 'Move Thing backward', 'S', [ssShift]);
  AddKBItem(miEdit, 'Move Thing up', 'E', [ssShift]);
  AddKBItem(miEdit, 'Move Thing down', 'Q', [ssShift]);
  AddKBItem(miEdit, 'Rotate Thing Pitch'#9'Ctrl+Shift+Mouse Wheel', #0, []);
  AddKBItem(miEdit, 'Rotate Thing Yaw'#9'Ctrl+Mouse Wheel', #0, []);
  AddKBItem(miEdit, 'Rotate Thing Roll'#9'Ctrl+Alt+Mouse Wheel', #0, []);

  AddKBItem(miEdit, 'Raise item', Char(VK_UP), [ssCtrl]);
  AddKBItem(miEdit, 'Lower item', Char(VK_DOWN), [ssCtrl]);
  AddKBItem(miEdit, 'Raise item', #219, []);
  AddKBItem(miEdit, 'Lower item', #221, []);

  AddKBItem(miEdit, 'Delete item', Char(VK_DELETE), []);
  AddKBItem(miEdit, 'Extrude surface', 'X', []);
  AddKBItem(miEdit, 'Extrude surface by', 'X', [ssShift]);
  AddKBItem(miEdit, 'Extrude && Expand surface', 'X', [ssCtrl]);
  AddKBItem(miEdit, 'Bring thing/light to surface', 'B', []);
  AddKBItem(miEdit, 'Copy camera as frame', 'F', []);
  AddKBItem(miEdit, 'Insert thing at surface', 'I', []);
  AddKBItem(miEdit, 'Insert thing at camera', 'I', [ssShift]);
end;

procedure TPreview3D.SetViewcamera1Click(Sender: TObject);
begin
  With Render3D.GetCamera().position do
    JedMain.SetCam(x, y, z, 0, 0, 0);
end;

Function TPreview3D.AddKBItem(mi: TMenuItem; const name: string; c: Char; sc: TShiftState): TMenuItem;
var
  ext: string;
begin
  Result := TMenuItem.Create(mi);
  Result.OnClick := KBCommandClick;

  Result.Caption := name;
  Result.Tag := ShortCut(Ord(c), sc);
  if c <> #0 then
    Result.ShortCut := ShortCut(Ord(c), sc);
  mi.Add(Result);
end;

Function TPreview3D.AddKBItem(mi: TMenuItem; const name: string; c: Char; sc: TShiftState; onClick: TNotifyEvent): TMenuItem;
begin
  Result := TMenuItem.Create(mi);
  Result.OnClick := onClick;

  Result.Caption := name;
  Result.Tag := ShortCut(Ord(c), sc);
  if c <> #0 then
    Result.ShortCut := ShortCut(Ord(c), sc);
  mi.Add(Result);
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

Procedure TPreview3D.MoveCameraForward;
begin
  if Render3D <> nil then
    Render3D.Getcamera().MoveForward(P3DStep);
end;

procedure TPreview3D.MenuHighlightFloorClick(Sender: TObject);
begin
  if Render3D <> nil then
    begin
      Render3D.highlightSolidFloor := MenuHighlightFloor.Checked;
      Invalidate;
    end;
end;

procedure TPreview3D.MenuHighlightSelectionClick(Sender: TObject);
begin
 MenuNoClipSelectionOutline.Enabled := MenuHighlightSelection.Checked;
 if Render3D <> nil then
    begin
      Render3D.highlightSelection := MenuHighlightSelection.Checked;
      Invalidate;
    end;
end;

procedure TPreview3D.MenuNoClipSelectionOutlineClick(Sender: TObject);
begin
  if Render3D <> nil then
    begin
      P3DNoClipSelectionOutline := MenuNoClipSelectionOutline.Checked;
      Render3D.noClipSelectionOutline := P3DNoClipSelectionOutline;
      Invalidate;
    end;
end;

procedure TPreview3D.MenuHighlightSolidSurfaceClick(Sender: TObject);
begin
  if Render3D <> nil then
    begin
      Render3D.highlightSolidSurface := MenuHighlightSolidSurface.Checked;
      Invalidate;
    end;
end;

procedure TPreview3D.MenuHighlightSpecialSurfacesClick(Sender: TObject);
begin

  if Render3D <> nil then
    begin
      Render3D.highlightSpecialSurface := MenuHighlightSpecialSurfaces.Checked;
      Invalidate;
    end;
end;

Procedure TPreview3D.MoveCameraBackward;
begin
  if Render3D <> nil then
    Render3D.Getcamera().MoveBackward(P3DStep);
end;

Procedure TPreview3D.MoveCameraLeft;
begin
  if Render3D <> nil then
    Render3D.Getcamera().MoveLeft(P3DStep);
end;

Procedure TPreview3D.MoveCameraRight;
begin
  if Render3D <> nil then
    Render3D.Getcamera().MoveRight(P3DStep);
end;

Procedure TPreview3D.MoveCameraUp;
begin
  if Render3D <> nil then
    Render3D.Getcamera().MoveUp(P3DStep);
    //Render3D.CamZ := Render3D.CamZ + P3DStep;
end;

Procedure TPreview3D.MoveCameraDown;
begin
  if Render3D <> nil then
    Render3D.Getcamera().MoveDown(P3DStep);
end;

Procedure TPreview3D.TranslateSelectedThings(dx, dy, dz: double; localForward: boolean );
  var d: TVector;
begin
  // Function translates selected thing(s) in the direction of current selected thing
  if JedMain.mapMode = MM_TH then
    begin
      StartUndoRec('Translate Thing(s)');
      d.dx := dx;
      d.dy := dy;
      d.dz := dz;
      if localForward then
        with Level.Things[JedMain.Cur_TH] do
          RotateVector(d, 0, yaw, 0);

      TranslateThings(level, JedMain.thsel, JedMain.Cur_TH, d.dx, d.dy, d.dz, MoveFrames);
    end;
end;

Procedure TPreview3D.MoveThingForward;
begin
  TranslateSelectedThings(0, PerpStep, 0);
end;

Procedure TPreview3D.MoveThingBackward;
begin
  TranslateSelectedThings(0, -PerpStep, 0);
end;

Procedure TPreview3D.MoveThingLeft;
begin
  TranslateSelectedThings(-PerpStep, 0, 0);
end;

Procedure TPreview3D.MoveThingRight;
begin
  TranslateSelectedThings(PerpStep, 0, 0);
end;

Procedure TPreview3D.MoveThingUp;
begin
  TranslateSelectedThings(0, 0, PerpStep);
end;

Procedure TPreview3D.MoveThingDown;
begin
  TranslateSelectedThings(0, 0, -PerpStep);
end;

Function TPreview3D.PlaySectorSound(filename: string): Boolean;
begin
  try
    Result := appSoundPlayer.Play(filename, (*loop:*) True);
  finally
  end;
end;

 Function TPreview3D.RestartSectorSound: Boolean;
 begin
  Result := appSoundPlayer.Restart;
 end;

 Procedure TPreview3D.SetSectorSoundVolume(volume: Double);
 begin
  appSoundPlayer.volume := volume;
 end;

Procedure TPreview3D.PauseSectorSound;
begin
  appSoundPlayer.Pause;
end;

Procedure TPreview3D.ResumeSectorSound;
begin
  appSoundPlayer.Resume;
end;

Procedure TPreview3D.StopSectorSound;
begin
  appSoundPlayer.Stop;
end;

procedure TPreview3D.WMActivateApp(var Msg: TWMActivateApp);
begin
  if (not visible) or (render3D = nil) then
    exit;

  if Msg.Active then
    ResumeSectorSound
  else
    PauseSectorSound;
end;

procedure TPreview3D.MenuRenderFogClick(Sender: TObject);
begin
  UpdateFog();
  Invalidate
end;

procedure TPreview3D.MenuRenderFullyLitClick(Sender: TObject);
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

  with Render3D.GetCamera() do
  begin
    x := position.x;
    y := position.y;
    z := position.z;
    pch := -rotation.pitch;
    yaw := -rotation.yaw;
  end;
end;

end.
