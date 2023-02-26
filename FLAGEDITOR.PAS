unit FlagEditor;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  StdCtrls;

type
  TFlagEdit = class(TForm)
    Bit0: TCheckBox;
    Bit1: TCheckBox;
    Bit2: TCheckBox;
    Bit3: TCheckBox;
    Bit4: TCheckBox;
    Bit5: TCheckBox;
    Bit6: TCheckBox;
    Bit7: TCheckBox;
    Bit8: TCheckBox;
    Bit9: TCheckBox;
    Bit10: TCheckBox;
    Bit11: TCheckBox;
    Bit12: TCheckBox;
    Bit13: TCheckBox;
    Bit14: TCheckBox;
    Bit15: TCheckBox;
    Bit16: TCheckBox;
    Bit17: TCheckBox;
    Bit18: TCheckBox;
    Bit19: TCheckBox;
    Bit20: TCheckBox;
    Bit21: TCheckBox;
    Bit22: TCheckBox;
    Bit23: TCheckBox;
    Bit24: TCheckBox;
    Bit25: TCheckBox;
    Bit26: TCheckBox;
    Bit27: TCheckBox;
    Bit28: TCheckBox;
    Bit29: TCheckBox;
    Bit30: TCheckBox;
    Bit31: TCheckBox;
    BNOK: TButton;
    BNCancel: TButton;
    procedure FormCreate(Sender: TObject);
  private
    { Private declarations }
    Bits: Array [0 .. 31] of TCheckBox;
    Procedure ClearNames;
    Procedure SetName(n: Byte; const Name: String);
    Procedure SetNameDesc(n: Byte; const Name, Desc: String);

    Procedure SetFlags(F: Longint);
    Function GetFlags: Longint;
  public
    Function EditSectorFlags(flags: Longint): Longint;
    Function EditSurfaceFlags(flags: Longint): Longint;
    Function EditAdjoinFlags(flags: Longint): Longint;

    Function EditThingFlags(flags: Longint): Longint;
    Function EditFaceFlags(flags: Longint): Longint;
    Function EditLevelITMFlags(flags: Longint): Longint;
    Function EditEventMask(flags: Longint): Longint;
    Function EditEntityMask(flags: Longint): Longint;
    Function EditMPModes(flags: Longint): Longint;
    Function EditLightFlags(flags: Longint): Longint;
    { Public declarations }
  end;

var
  FlagEdit: TFlagEdit;

implementation

{$R *.DFM}

Function FlagNum(n: Longint): Integer;
var
  F, i: Integer;
begin
  i := 0;
  F := 1;
  While n <> F do
  begin
    F := F shl 1;
    inc(i);
  end;
  Result := i;
end;

procedure TFlagEdit.FormCreate(Sender: TObject);
begin
  ClientWidth := Bit0.Left + Bit16.Left + Bit16.Width;
  ClientHeight := Bit0.Top + BNOK.Top + BNOK.Height;
  Bits[0] := Bit0;
  Bits[1] := Bit1;
  Bits[2] := Bit2;
  Bits[3] := Bit3;
  Bits[4] := Bit4;
  Bits[5] := Bit5;
  Bits[6] := Bit6;
  Bits[7] := Bit7;
  Bits[8] := Bit8;
  Bits[9] := Bit9;
  Bits[10] := Bit10;
  Bits[11] := Bit11;
  Bits[12] := Bit12;
  Bits[13] := Bit13;
  Bits[14] := Bit14;
  Bits[15] := Bit15;
  Bits[16] := Bit16;
  Bits[17] := Bit17;
  Bits[18] := Bit18;
  Bits[19] := Bit19;
  Bits[20] := Bit20;
  Bits[21] := Bit21;
  Bits[22] := Bit22;
  Bits[23] := Bit23;
  Bits[24] := Bit24;
  Bits[25] := Bit25;
  Bits[26] := Bit26;
  Bits[27] := Bit27;
  Bits[28] := Bit28;
  Bits[29] := Bit29;
  Bits[30] := Bit30;
  Bits[31] := Bit31;
end;

Procedure TFlagEdit.ClearNames;
var
  i: Integer;
begin
  for i := 0 to 31 do
  begin
    Bits[i].Caption := Format('Unknown/Unused (Flag %d)', [i]);
    Bits[i].Hint := '';
  end;
end;

Procedure TFlagEdit.SetNameDesc(n: Byte; const Name, Desc: String);
begin
  Bits[n].Caption := Name;
  Bits[n].Hint := Desc;
end;

Procedure TFlagEdit.SetName(n: Byte; const Name: String);
begin
  SetNameDesc(n, Name, '');
end;

Procedure TFlagEdit.SetFlags(F: Longint);
var
  i: Integer;
begin
  For i := 0 to 31 do
    Bits[i].Checked := (F and (1 shl i)) <> 0;
end;

Function TFlagEdit.GetFlags: Longint;
var
  i: Integer;
begin
  Result := 0;
  For i := 0 to 31 do
    if Bits[i].Checked then
      Result := Result or (1 shl i);
end;

Function TFlagEdit.EditMPModes(flags: Longint): Longint;
begin
  ClearNames;
  SetName(0, 'Capture the flag');
  SetName(1, 'Tag');
  SetName(2, 'Kill fool with chicken');
  SetName(3, 'Secret doc');
  SetName(4, 'Team Play');
  SetName(5, 'Death match');
  SetFlags(flags);
  if ShowModal <> mrOK then
    Result := flags
  else
    Result := GetFlags;
end;

Function TFlagEdit.EditEntityMask(flags: Longint): Longint;
begin
  ClearNames;
  SetName(0, 'Enemy');
  SetName(3, 'Weapon');
  SetName(31, 'Player');
  SetFlags(flags);
  if ShowModal <> mrOK then
    Result := flags
  else
    Result := GetFlags;
end;

Function TFlagEdit.EditEventMask(flags: Longint): Longint;
begin
  ClearNames;
  SetName(0, 'Cross from inside');
  SetName(1, 'Cross from outside');
  SetName(2, 'Enter sector');
  SetName(3, 'Leave sector');
  SetName(4, 'Nudge from inside');
  SetName(5, 'Nudge from outside');
  SetName(6, 'Blow up');
  { SetName(7,'Shoot'); }
  SetName(8, 'Shoot');
  SetName(14, 'NWX message');
  SetName(16, 'Cutom event 1');
  SetName(17, 'Cutom event 2');
  SetName(18, 'Cutom event 3');
  SetName(19, 'Cutom event 4');
  SetName(20, 'Cutom event 5');
  SetName(21, 'Cutom event 6');
  SetName(22, 'Cutom event 7');
  SetName(23, 'Cutom event 8');
  SetName(24, 'Cutom event 9');
  SetName(25, 'Cutom event 10');
  SetFlags(flags);
  if ShowModal <> mrOK then
    Result := flags
  else
    Result := GetFlags;
end;

Function TFlagEdit.EditSectorFlags(flags: Longint): Longint;
begin
  if FlagNum(1) = 0 then;
  ClearNames;
  SetName(0, 'No Gravity');
  SetNameDesc(1, 'Underwater', 'Underwater Sector');
  SetNameDesc(2, 'Used in COG', 'Refrenced by a COG');
  SetNameDesc(3, 'Has thrust', 'Sector has Thrust');
  SetNameDesc(4, 'Hide on map', 'Don''t show sector on map');
  SetNameDesc(5, 'No AI', 'AI can''t enter this sector');
  SetNameDesc(6, 'Pit', 'If you fall here, screen fades');
  SetName(7, 'Turn adjoins off');
  SetName(10, 'Draw on map');

  SetNameDesc(12, 'Has Collide box', 'System flag - ignore');

  SetNameDesc(31, 'Preview as 3DO', 'JED flag - show inverted in 3D preview');

  SetFlags(flags);
  if ShowModal <> mrOK then
    Result := flags
  else
    Result := GetFlags;
end;

Function TFlagEdit.EditSurfaceFlags(flags: Longint): Longint;
begin
  ClearNames;
  SetNameDesc(0, 'Floor', 'Appears on map and isn''t slippery');
  SetNameDesc(1, 'Used in COG', 'Refrenced by a COG');
  SetName(2, 'Impassable');
  SetName(3, 'Not walkable by AI');
  SetName(4, 'Double tx scale');
  SetName(5, 'Half tx scale');
  SetName(6, '1/8 tx scale');

  SetName(7, 'No damage from fall');
  { SetName(8,'No autofloor'); }

  SetName(9, 'Sky');
  SetName(10, 'Sky (Different type)');
  SetName(11, 'Scrolling');
  SetName(12, 'Icy');
  SetName(13, 'Very icy');
  SetNameDesc(14, 'Magsealed', 'Reflects shots');
  SetNameDesc(16, 'Metal', 'Steps make metal sound');
  SetName(17, 'Deep water');
  SetName(18, 'Shallow water');
  SetNameDesc(19, 'Dirt', 'Steps make dirt sound');
  SetName(20, 'Very deep water');
  SetName(31, 'Texture Flipped');

  SetFlags(flags);
  if ShowModal <> mrOK then
    Result := flags
  else
    Result := GetFlags;
end;

Function TFlagEdit.EditAdjoinFlags(flags: Longint): Longint;
begin
  ClearNames;
  SetName(0, 'Render past adjoin');
  SetName(1, 'Passable');
  SetName(2, 'Doesn''t block sound');
  SetName(3, 'Impassable for AI');
  SetName(4, 'Impassable for player');
  SetName(31, 'Blocks light');
  SetFlags(flags);
  if ShowModal <> mrOK then
    Result := flags
  else
    Result := GetFlags;
end;

Function TFlagEdit.EditThingFlags(flags: Longint): Longint;
begin
  ClearNames;
  SetName(0, 'Emits light');
  SetName(1, 'Dead');
  SetNameDesc(2, 'Magsealed', 'Reflects shots');
  SetNameDesc(3, 'Architecture',
    'Object should be rendered as part of world architecture');
  SetName(4, 'Invisible');
  SetName(6, 'Can be stood on');
  SetName(8, 'Controlled remotely');
  SetName(9, 'Dying');

  SetName(10, 'Used in COG');
  SetNameDesc(11, 'Not crushing', 'Won''t damage things it crushes');
  SetName(12, 'Not present on Easy');
  SetName(13, 'Not present on Medium');
  SetName(14, 'Not present on Hard');
  SetName(15, 'Not present on Multiplayer');
  SetName(16, 'Not present on Singleplayer');
  SetName(19, 'Disabled');

  SetNameDesc(22, 'Metal', 'Object made of metal');
  SetNameDesc(23, 'Earth', 'Object made of earthen materials');
  SetNameDesc(24, 'No sound', 'Object makes no sound');
  SetNameDesc(25, 'Submerged', 'Object is submerged');
  SetName(26, 'Prelit');
  SetNameDesc(27, 'Air only', 'Object can''t live in water');
  SetNameDesc(28, 'Water only', 'Object can''t live in air');
  SetName(29, 'makes splash');

  SetFlags(flags);

  if ShowModal <> mrOK then
    Result := flags
  else
    Result := GetFlags;
end;

Function TFlagEdit.EditFaceFlags(flags: Longint): Longint;
begin
  ClearNames;
  SetNameDesc(1, 'Translucent', 'Half-Transparent');
  SetFlags(flags);
  if ShowModal <> mrOK then
    Result := flags
  else
    Result := GetFlags;
end;

Function TFlagEdit.EditLevelITMFlags(flags: Longint): Longint;
begin
  ClearNames;
  SetName(0, 'Dump weapons');
  SetName(1, 'Dump ammo');
  SetName(2, 'Dump health');
  SetName(3, 'Dump oil');
  SetName(4, 'Dump all');
  SetName(5, 'Show score');
  { SetName(6,'Drop nonnative'); }
  SetFlags(flags);
  if ShowModal <> mrOK then
    Result := flags
  else
    Result := GetFlags;
end;

Function TFlagEdit.EditLightFlags(flags: Longint): Longint;
begin
  ClearNames;
  SetNameDesc(0, 'Not blocked', 'Not blocked by geometry');
  SetFlags(flags);
  if ShowModal <> mrOK then
    Result := flags
  else
    Result := GetFlags;
end;

end.
