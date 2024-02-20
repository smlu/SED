unit GlobalVars;

interface

uses SysUtils, Forms, graphics, Classes;

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

  TProjectType = (JKDF2 = 0, MOTS, IJIM);

Const
  IJIMDefaultExe = 'Indy3D.exe';
  StaticResIdxMask = $8000;
  MaxMsgs = 500;
  SedVerNum = '0.1.0';
  SedVersion: string = SedVerNum;
  LECLogoJK: String = '................................'#13#10 +
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

  LECLogoIJIM: String =
    '................................'#13#10 +
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
    '@.(c).lucasarts.entertainment..@'#13#10 +
    '@.........company.llc..........@'#13#10 +
    '@....(c).lucasfilm.ltd.&.tm....@'#13#10 +
    '@.....all.rights.reserved......@'#13#10 +
    '@...used.under.authorization...@'#13#10 +
    '@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@';


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

  //RegBase = '\Software\Code Alliance\Jed';

  m_800x600   = 0;
  m_1024x768  = 1;
  m_1280x960  = 2;
  m_1440x900  = 3;
  m_1600x1024 = 4;
  m_1920x1200 = 5;

  MR_old  = 1; { MapRot consts }
  MR_New  = 0;
  DataDir = 'data';

Var
  BaseDir: String;
  ProjectDir: String;
  JKDir, MOTSDir, IJIMDir: String;
  IJIMExe: String;
  GameDir: String;
  CurrentProject: TProjectType;

  D3DDevice: String = 'Direct3D HAL';

  P3DAPI: Integer = 0;
  P3DOnTop: Boolean;
  P3DFullLit: Boolean = false;
  P3DColoredLights: Boolean = true;
  P3DX, P3DY: Integer;
  P3DWinSize: Integer = 0;
  P3DGamma: double = 1;
  P3DVisLayers: Boolean = false;
  P3DThings: Boolean = true;

  WireframeAPI: Integer = 1; // 1 - OpenGlL
  WF_InvertCulling: Boolean = false;
  WF_DoubleBuf: Boolean = true;
  WF_GeoVertSize: Integer = 5;
  WF_VertMinSize: Integer = 1;
  WF_VertMaxSize: Integer = 10;
  WF_ObjVertSize: Integer = 5;

  MapRot: Integer = 0;
  VertSnapMaxDist: Double = 0.2;
  ZoomLocked: Boolean = true;
  ZoomMin: Double = 1.0;
  ZoomMax: Double = 5000;

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
  DefThingView: integer = 2; { View as wireframes }
  DefMselMode: integer = 0; { Toggle }

  DefSnapToGrid: boolean = false;
  DefGridStep: double = 0.2;
  DefGridLine: double = 1;
  DefGridDot: double = 0.2;
  DefShape: string = 'Cube';
  DefGridSize: double = 10;
  GridMoveStep: double = 0.001;

  NewOnFloor: boolean = false;
  UndoEnabled: boolean = true;
  UndoStackSize: UInt32 = 1024;
  UndoMaxStackSize: UInt32 = 1024;
  MoveFrames: boolean = true;
  GOBSmart: boolean = false;
  CheckOverlaps: boolean = false;
  NewLightCalc: boolean = true;
  ConfirmRevert: boolean = false;
  SaveEditorLights: boolean = true;
  SaveEditorLayers: boolean = true;

  Recent1, Recent2, Recent3, Recent4: string;

  clP3DBack: TJedColor = (r: 69; g: 69; b: 69);
  clP3DSel: TJedColor = (r: 255; g: 255; b: 0);
  clP3DMultiSel: TJedColor = (r: 0; g: 255; b: 255);
  clP3DSolidFloor: TJedColor = (r: 255; g: 0; b: 0);
  clP3DSolidSurface: TJedColor = (r: 128; g: 0; b: 128);
  clP3DSpecialSurface: TJedColor = (r: 0; g: 255; b: 0);
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
  clCleaveLine: TJedColor = (r: 0; g: 255; b: 0);

Function GetConfigFilePath: string;
Procedure GetWinPos(f: TForm; var p: TWinPos);
Procedure SetWinPos(f: TForm; var p: TWinPos);
Procedure SetWinPosOnly(f: TForm; var p: TWinPos);

Procedure SetP3DPos(f: TForm; X, Y, mode: integer);
Procedure GetP3DPos(f: TForm; var X, Y, mode: integer);

Function GetStayOnTop(f: TForm): boolean;
Procedure SetStayOnTop(f: TForm; ontop: boolean);

Function DefaultPixelPerUnit: double;
Function PixelPerUnit: double;

implementation

uses J_Level;

Function GetConfigFilePath: string;
begin
   Result := 'sed.ini';
end;

Function DefaultPixelPerUnit: double;
begin
//   if CurrentProject = TProjectType.IJIM then
//      result := 320 // This is hack because IM UVs are based on texture size
//    else
      result := 320;
end;

Function PixelPerUnit: double;
begin
  if Level = nil then
    result := DefaultPixelPerUnit
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
    m_1280x960:
      begin
        f.clientwidth := 1280;
        f.clientheight := 960;
      end;
    m_1440x900:
      begin
        f.clientwidth := 1440;
        f.clientheight := 900;
      end;
     m_1600x1024:
      begin
        f.clientwidth := 1600;
        f.clientheight := 1024;
      end;
     m_1920x1200:
      begin
        f.clientwidth := 1920;
        f.clientheight := 1200;
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

  if w < 1024 then
    mode := m_800x600
  else if w < 1280 then
    mode := m_1024x768
  else if w < 1440 then
    mode := m_1280x960
  else if w < 1600 then
    mode := m_1440x900
  else if w < 1920 then
    mode := m_1600x1024
  else
    mode := m_1920x1200
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

  FormatSettings := TFormatSettings.Invariant;
  FormatSettings.DecimalSeparator := '.';
end;

end.
