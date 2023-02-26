unit GlobalVars;

interface

uses SysUtils, Forms, graphics;

Type
  Float = double;
  TInt = SmallInt;
  TFileName = String;

  TThingBox = record
    x1, x2, y1, y2, z1, z2: single;
  end;

  TWinPos = record
    X, Y, w, h: integer;

  end;

  TJedColor = record
    case byte of
      0:
        (col: TColor);
      1:
        (r, g, b: byte);
      2:
        (i: integer);
  end;

Const
  MaxMsgs = 500;
  JedVerNum = '0.951';
  JedVersion: string = JedVerNum + ' beta';
  LECLogo: String = '................................'#13#10 +
    '................@...@...@...@...'#13#10 +
    '.............@...@..@..@...@....'#13#10 +
    '................@.@.@.@.@.@.....'#13#10 +
    '@@@@@@@@......@...........@.....'#13#10 +
    '@@@@@@@@....@@......@@@....@....'#13#10 +
    '@@.....@.....@......@@@.....@@..'#13#10 +
    '@@.@@@@@......@.....@@@......@@.'#13#10 +
    '@@@@@@@@.......@....@@.....@@...'#13#10 +
    '@@@@@@@@.........@@@@@@@@@@.....'#13#10 +
    '@@@@@@@@..........@@@@@@........'#13#10 +
    '@@.....@..........@@@@@.........'#13#10 +
    '@@.@@@@@.........@@@@@@.........'#13#10 +
    '@@.....@.........@@@@@@.........'#13#10 +
    '@@@@@@@@.........@@@@@@.........'#13#10 +
    '@@@@@@@@.........@@@@@@@........'#13#10 +
    '@@@...@@.........@@@@@@@........'#13#10 +
    '@@.@@@.@.........@.....@........'#13#10 +
    '@@..@..@........@.......@.......'#13#10 +
    '@@@@@@@@........@.......@.......'#13#10 +
    '@@@@@@@@.......@........@.......'#13#10 +
    '@@..@@@@.......@........@.......'#13#10 +
    '@@@@..@@......@.........@.......'#13#10 +
    '@@@@.@.@......@.........@.......'#13#10 +
    '@@....@@........................'#13#10 +
    '@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@'#13#10 +
    '@@@@@@@@@@@@@.@@@@@@@@@@@@@@@@@@'#13#10 +
    '@@.@@..@@@@@..@@@@@@@@@@.@@@@@@@'#13#10 +
    '@@.@.@.@@@@.@.@@@.@..@@...@@@..@'#13#10 +
    '@@..@@@@@@....@@@..@@@@@.@@@@.@@'#13#10 +
    '@@@@@@@@...@@.@@@.@@@@@..@@...@@'#13#10 +
    '@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@'#13#10 +
    '@.copyright.(c).1997.lucasarts.@'#13#10 +
    '@@@@@@..entertainment.co..@@@@@@';

  EpisodeJKTpl: String = '"New Level"'#13#10 +
    '# Title must be first line.  A translated version is looked up in jkStrings.uni'#13#10
    + '# Please use TABs for alignment!'#13#10 + #13#10 +
    'TYPE  1  # single-player'#13#10 + #13#10 + 'SEQ   1'#13#10 + #13#10 +
    '# <lightpow> and <darkpow> are the inventory bin numbers for that power.'#13#10
    + #13#10 +
    '# <line> <cd>  <level>  <type>   <file>         <lightpow>  <darkpow>   <gotoA>  <gotoB>'#13#10
    + #13#10 +
    '10:      1     1        LEVEL    %s   0           0           -1    -1    # Level 1: NewLevel'#13#10
    + #13#10 + 'end';

  RegBase = '\Software\Code Alliance\Jed';

  m_320x240 = 0;
  m_400x300 = 1;
  m_512x384 = 2;
  m_640x480 = 3;
  m_800x600 = 4;
  m_1024x768 = 5;

  MR_old = 1; { MapRot consts }
  MR_New = 0;

Var
  BaseDir: String;
  ProjectDir: String;
  CDDir: String;
  JKDir, MOTSDir: String;
  JKCDDir: String = 'D:\GAMEDATA\';
  MOTSCDDir: String = 'D:\GAMEDATA\';

  GameDir: String;

  IsMots: boolean;

  D3DDevice: String = 'RGB Emulation';

  P3DAPI: integer = 0;
  P3DOnTop: boolean;
  P3DFullLit: boolean;
  P3DColoredLights: boolean = true;
  P3DX, P3DY: integer;
  P3DWinSize: integer = 0;
  P3DGamma: double = 1;
  P3DVisLayers: boolean = false;
  P3DThings: boolean = false;

  WireframeAPI: integer = 0;
  WF_DoubleBuf: boolean = true;
  MapRot: integer = 0;

  AutoSave: boolean = false;
  SaveInterval: integer = 0;

  cc_FixErrors: integer;

  sm_ShowWarnings: boolean = true;
  sm_ShowInfo: boolean = true;
  Res1_gob, Res2_gob, sp_gob, mp1_gob, mp2_gob, mp3_gob: String;

  IEditPos, MWinPos, TBarPos: TWinPos;
  MWMaxed: boolean;

  TbOnTop: boolean;
  IEOnTop: boolean;
  CFOnTop: boolean;

  TxStep: double = 0.5;
  TXRotStep: double = 1;
  TXScaleStep: double = 2;
  PerpStep: double = 0.02;
  P3DStep: double = 0.1;
  GridSize: double = 10;

  DefTxStep: double = 0.5;
  DefTXRotStep: double = 1;
  DefTXScaleStep: double = 2;
  DefPerpStep: double = 0.02;
  DefP3DStep: double = 0.1;
  DefThingView: integer = 1; { View as Boxes }
  DefMselMode: integer = 0; { Toggle }

  DefGridStep: double = 0.2;
  DefGridLine: double = 1;
  DefGridDot: double = 0.2;
  DefShape: string = 'Cube';
  defGridSize: double = 10;
  GridMoveStep: double = 0.001;

  NewOnFloor: boolean = false;
  UndoEnabled: boolean = true;
  MoveFrames: boolean = true;
  GOBSmart: boolean = false;
  CheckOverlaps: boolean = false;
  NewLightCalc: boolean = true;
  ConfirmRevert: boolean = false;

  Recent1, Recent2, Recent3, Recent4: string;

  clMapBack: TJedColor = (r: 0; g: 0; b: 0);
  clMapGeo: TJedColor = (r: 255; g: 255; b: 255);
  clMapGeoBack: TJedColor = (r: 127; g: 127; b: 127);
  clMapSel: TJedColor = (r: 255; g: 0; b: 0);
  clMapSelBack: TJedColor = (r: 127; g: 0; b: 0);
  clGrid: TJedColor = (r: 255; g: 0; b: 255);
  clVertex: TJedColor = (r: 0; g: 0; b: 255);
  clThing: TJedColor = (r: 0; g: 255; b: 0);
  clFrame: TJedColor = (r: 0; g: 127; b: 0);
  clLight: TJedColor = (r: 255; g: 255; b: 0);
  clMSel: TJedColor = (r: 0; g: 255; b: 255);
  clMSelBack: TJedColor = (r: 0; g: 127; b: 127);
  clSelMsel: TJedColor = (r: 255; g: 127; b: 0);
  clSelMselBack: TJedColor = (r: 127; g: 63; b: 0);
  clGridX: TJedColor = (r: 255; g: 255; b: 0);
  clGridY: TJedColor = (r: 0; g: 0; b: 255);
  clExtra: TJedColor = (r: 0; g: 255; b: 128);

Procedure GetWinPos(f: TForm; var p: TWinPos);
Procedure SetWinPos(f: TForm; var p: TWinPos);
Procedure SetWinPosOnly(f: TForm; var p: TWinPos);

Procedure SetP3DPos(f: TForm; X, Y, mode: integer);
Procedure GetP3DPos(f: TForm; var X, Y, mode: integer);

Function GetStayOnTop(f: TForm): boolean;
Procedure SetStayOnTop(f: TForm; ontop: boolean);

Function PixelPerUnit: double;

implementation

uses J_Level;

Function PixelPerUnit: double;
begin
  if Level = nil then
    result := 320
  else
    result := Level.ppunit;
end;

Procedure SetP3DPos(f: TForm; X, Y, mode: integer);
begin
  if (X > 0) and (X < Screen.Width - 20) then
    f.left := X;
  if (Y > 0) and (Y < Screen.height - 20) then
    f.top := Y;

  case mode of
    m_320x240:
      begin
        f.clientwidth := 320;
        f.clientheight := 240;
      end;
    m_400x300:
      begin
        f.clientwidth := 400;
        f.clientheight := 300;
      end;
    m_512x384:
      begin
        f.clientwidth := 512;
        f.clientheight := 384;
      end;
    m_640x480:
      begin
        f.clientwidth := 640;
        f.clientheight := 480;
      end;
    m_800x600:
      begin
        f.clientwidth := 800;
        f.clientheight := 600;
      end;
    m_1024x768:
      begin
        f.clientwidth := 1024;
        f.clientheight := 768;
      end;
  end;
end;

Procedure GetP3DPos(f: TForm; var X, Y, mode: integer);
var
  w: integer;
begin
  X := f.left;
  Y := f.top;
  w := f.clientwidth;
  if w < 360 then
    mode := m_320x240
  else if w < 458 then
    mode := m_400x300
  else if w < 576 then
    mode := m_512x384
  else if w < 720 then
    mode := m_640x480
  else if w < 912 then
    mode := m_800x600
  else
    mode := m_1024x768;
end;

Function GetStayOnTop(f: TForm): boolean;
begin
  result := f.FormStyle = fsStayOnTop;
end;

Procedure SetStayOnTop(f: TForm; ontop: boolean);
begin
  if ontop then
    f.FormStyle := fsStayOnTop
  else
    f.FormStyle := fsNormal;
end;

Procedure GetWinPos(f: TForm; var p: TWinPos);
begin
  p.X := f.left;
  p.Y := f.top;
  if (f.clientwidth < 16) or (f.clientheight < 16) then
    exit;
  p.w := f.Width;
  p.h := f.height;
end;

Procedure SetWinPos(f: TForm; var p: TWinPos);
begin
  if (p.X = 0) or (p.Y = 0) then
    exit;
  if (p.X > 0) and (p.X < Screen.Width - 20) then
    f.left := p.X;
  if (p.Y > 0) and (p.Y < Screen.height - 20) then
    f.top := p.Y;
  if (p.w < 32) or (p.h < 32) then
    exit;
  f.Width := p.w;
  f.height := p.h;
end;

Procedure SetWinPosOnly(f: TForm; var p: TWinPos);
begin
  if (p.X = 0) or (p.Y = 0) then
    exit;
  if (p.X > 0) and (p.X < Screen.Width - 20) then
    f.left := p.X;
  if (p.Y > 0) and (p.Y < Screen.height - 20) then
    f.top := p.Y;

end;

Initialization

begin
  BaseDir := ExtractFilePath(Paramstr(0));
  FormatSettings.DecimalSeparator := '.';
end;

end.
