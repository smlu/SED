unit FlagEditor;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  StdCtrls, GlobalVars;

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
    procedure FormHide(Sender: TObject);
    procedure FormShow(Sender: TObject);

  private
    { Private declarations }
    appHintHidePause : integer;
    Bits: Array [0 .. 31] of TCheckBox;
    DefaultClickHandler: TNotifyEvent;
    Procedure ResetBits;
    Procedure SetName(n: Byte; const Name: String);
    Procedure SetNameDesc(n: Byte; const Name, Desc: String);
    Procedure SetFlagClickHandler(n: Byte; lambda: TProc<TObject>);
    Procedure ResetClickHandler(cb: TCheckBox);
    Procedure ResetClickHandlers;

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
uses misc_utils;

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
  // save current app hint puse time
  appHintHidePause := Application.HintHidePause;

  ClientWidth := Bit0.Left + Bit31.Left + Bit31.Width;
  ClientHeight := Bit0.Top + BNOK.Top + BNOK.Height;
  DefaultClickHandler := Bit0.OnClick;

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

  ResetBits;
end;


procedure TFlagEdit.FormHide(Sender: TObject);
begin
  // Restore app hint hide time
  Application.HintHidePause := appHintHidePause;

  ResetClickHandlers;
  ResetBits;
end;

procedure TFlagEdit.FormShow(Sender: TObject);
begin
  // Disable auto hint hide
  appHintHidePause := Application.HintHidePause;
  Application.HintHidePause := 10000000;
end;

Procedure TFlagEdit.ResetBits;
var
  i: Integer;
begin
  for i := 0 to 31 do
  begin
    Bits[i].Caption := Format('0x%x - Unknown/Unused', [1 shl i]);
    Bits[i].Hint := '';
    Bits[i].Enabled := true;
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

Procedure TFlagEdit.SetFlagClickHandler(n: Byte; lambda: TProc<TObject>);
begin
    const cb = Bits[n];
    ResetClickHandler(cb);
    cb.OnClick := LambdaNotifyEvent(Bits[n], lambda);
end;

Procedure TFlagEdit.ResetClickHandler(cb: TCheckBox);
begin
  cb.DestroyComponents; // Remove any OnClick handler via LambdaNotifyEvent
  cb.OnClick := DefaultClickHandler;
end;

Procedure TFlagEdit.ResetClickHandlers;
begin
    For var i := 0 to 31 do
      ResetClickHandler(Bits[i]);
end;

Procedure TFlagEdit.SetFlags(F: Longint);
var
  i: Integer;
begin
  For i := 0 to 31 do
    begin
      var clkcHndl := Bits[i].OnClick;
      Bits[i].OnClick := DefaultClickHandler;
      Bits[i].Checked := (F and (1 shl i)) <> 0;
      Bits[i].OnClick := clkcHndl;
    end;
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
  SetNameDesc(0, '0x01 - No Gravity', 'Sector has no gravity.');
  SetNameDesc(1, '0x02 - Underwater', 'Sector is underwater.');
  SetNameDesc(2, '0x04 - COG Linked', 'Sector is linked to COG(s).');
  SetNameDesc(3, '0x08 - Use Thrust', 'Sector has thrust.');
  SetNameDesc(4, '0x10 - Hide on map', 'Don''t show sector on the map.');
  SetNameDesc(5, '0x20 - No AI actor', 'AI actor can''t enter this sector.');
  SetNameDesc(6, '0x40 - Fall death', 'Player is instantly killed falling into sector.');  // original flag name SITH_SECTOR_FALLDEATH
  SetNameDesc(7, '0x80 - Adjoins off', 'All sector adjoins are not rendered. (engine flag)');

  if CurrentProject = IJIM then
    SetNameDesc(8, '0x100 - Aetherium', 'Sector is part of the aetherium.');

  SetNameDesc(12, '0x1000 - Collide box', 'Sector has collision box set. (auto set)');
  SetNameDesc(14, '0x4000 - Seen', 'Sector has been seen by camera. (engine flag)');

  if CurrentProject = IJIM then
    SetNameDesc(29, 'No RGB Ambient Light', 'Calculate only ambient light intensity when calculating light(s). (SED flag)');

  SetNameDesc(30, 'No Ambient Light', 'Don''t calculate ambient light when calculating light(s). (SED flag)');
  SetNameDesc(31, 'Preview as 3DO', 'Show inverted in 3D preview. (SED flag)');

  Bits[7].Enabled := false; // disable setting flag 0x80 - Adjoins off

  SetFlags(flags);
  if ShowModal <> mrOK then
    Result := flags
  else
    Result := GetFlags;
end;

Function TFlagEdit.EditSurfaceFlags(flags: Longint): Longint;
  procedure SetSurfaceTypeClickHandler(n: Byte);
  begin
     SetFlagClickHandler(n,
      procedure (Sender: TObject)
      begin
        var cb := (Sender as TCheckBox);
        if not cb.Checked then exit;

        // Clear all set surface types
        var curhandler := cb.OnClick;
        cb.OnClick := nil;

        Bit9.Checked := False;
        Bit10.Checked := False;
        Bit16.Checked := False;
        Bit18.Checked := False;
        Bit19.Checked := False;

        if CurrentProject = IJIM then
        begin
          Bit7.Checked := False;
          Bit17.Checked := False;
          Bit20.Checked := False;
          Bit21.Checked := False;
          Bit22.Checked := False;
          Bit23.Checked := False;
          Bit29.Checked := False;
          Bit30.Checked := False;
          Bit31.Checked := False;
        end;

        // Check surf type cb
        cb.Checked := True;
        cb.OnClick := curhandler;
      end
    );
  end;

begin
  SetNameDesc(0,  '0x01 - Floor', 'Surface is ground floor a Thing object can move on.');
  SetNameDesc(1,  '0x02 - COG Linked', 'Surface is linked to COG(s).');
  SetNameDesc(2,  '0x04 - Collision', 'Solid surface with face collision.');
  SetNameDesc(3,  '0x08 - No AI move', 'AI actor can''t move onto the surface.');
  SetNameDesc(4,  '0x10 - 2x scrolling size', 'Double scrolling surface size. Affects the speed of SlideWall COG function.');
  SetNameDesc(5,  '0x20 - 1/2 scrolling size', 'Half scrolling surface size. Affects the speed of SlideWall COG function.');
  SetNameDesc(6,  '0x40 - 1/8 scrolling size', 'Eight scrolling surface size. Affects the speed of SlideWall COG function.');
  SetNameDesc(9,  '0x200 - Horizon sky', 'Surface is part of skybox.');
  SetNameDesc(10, '0x400 - Ceiling sky', 'Surface is part of ceiling sky.');
  SetNameDesc(11, '0x800 - Scrolling', 'Surface is scrolling/sliding. Set by SlideWall COG function.');
  SetNameDesc(16, '0x10000 - Metal', 'Metal surface. Makes metal sound fx when surface is hit or walked on');
  SetNameDesc(18, '0x40000 - Shallow water', 'Makes water sound fx when surface is touched or walked on.');
  SetNameDesc(19, '0x80000 - Dirt', 'Dirt surface. Makes dirt sound fx when surface is hit or walked on');

  if CurrentProject = IJIM then
    begin
      SetNameDesc(7,  '0x80 - Aetherium', 'Aetherium surface. Makes "aetherium" sound fx when surface is hit or walked on.');
      SetNameDesc(12, '0x1000 - Kill floor', 'Actor, player or weapon object is instantly destroyed if surface is touched.');
      SetNameDesc(13, '0x2000 - Climbable', 'Player can climb on the surface. e.g.: ladder.');
      SetNameDesc(14, '0x4000 - Track', 'Surface is a railroad track, e.g.: mine cart track.');
      SetNameDesc(15, '0x8000 - Changed', 'Set by engine to mark surface has changed.');
      SetNameDesc(17, '0x20000 - Water', 'Surface is part of underwater sector.');
      SetNameDesc(20, '0x100000 - Web', 'Spider web surface.');
      SetNameDesc(21, '0x200000 - Lava', 'Lava surface. Actor or player is instantly killed if surface is touched. ');
      SetNameDesc(22, '0x400000 - Snow', 'Snow surface. Makes snow sound fx when surface is hit or walked on.');
      SetNameDesc(23, '0x800000 - Wood', 'Wood surface. Makes wood sound fx when surface is hit or walked on.');
      SetNameDesc(24, '0x1000000 - Ledge', 'Surface is a ledge that player can grab onto and hang from.');
      SetNameDesc(25, '0x2000000 - Water ledge', 'Climb out of water ledge.');
      SetNameDesc(26, '0x4000000 - 1/4 scrolling size', 'Quarter scrolling surface size. Affects the speed of SlideWall COG function.');
      SetNameDesc(27, '0x8000000 - 4x scrolling size' , 'Quadruple scrolling surface size. Affects the speed of SlideWall COG function.');
      SetNameDesc(28, '0x10000000 - Whip aim', 'Triggers whip aim system for the player when they move onto the surface.');
      SetNameDesc(29, '0x20000000 - Echo', 'Makes indoor/echo sound fx when surface is hit or walked on.');
      SetNameDesc(30, '0x40000000 - Wood echo', 'Wood surface. Makes wood indoor/hollow sound fx when surface is hit or walked on.');
      SetNameDesc(31, '0x80000000 - Dirt echo', 'Dirt surface. Makes dirt indoor/hollow sound fx when surface is hit or walked on.');

      Bits[15].Enabled := False; // disable setting flag 0x8000 - Changed
    end
  else
    begin
      SetName(7, '0x80 - No damage from fall');
      { SetName(8,'No autofloor'); }

      SetName(12, '0x1000 - Icy');
      SetName(13, '0x2000 - Very icy');
      SetNameDesc(14, '0x4000 - Magsealed', 'Reflects shots'); // SITH_SURFACE_MAGSEALED
      SetName(17, '0x20000 - Deep water');
      SetName(20, '0x100000 - Very deep water');
    end;

  // Set special click handlers
  SetFlagClickHandler(0, // 0x01 - Floor
      procedure (Sender: TObject)
      begin
        var cb := (Sender as TCheckBox);
        if cb.checked then
          Bit2.checked := True // 0x04 - Collision
        else
          begin
            // Uncheck all special floor flags
            if CurrentProject = IJIM then
              begin
                Bit12.checked := False; // 0x1000     - Kill floor
                Bit14.checked := False; // 0x4000     - Track
                Bit28.checked := False; // 0x10000000 - Whip aim
              end;
          end;
      end
  );

  SetFlagClickHandler(2, // 0x04 - Collision
    procedure (Sender: TObject)
    begin
      var cb := (Sender as TCheckBox);
      if not cb.checked then // Uncheck all special collision flags
        begin
          Bit0.checked := False; // 0x01 - Floor
          if CurrentProject = IJIM then
            begin
              Bit13.checked := False; // 0x2000    - Climable
              Bit24.checked := False; // 0x1000000 - Ledge
              Bit25.checked := False; // 0x2000000 - Water ledge
            end;
        end;
    end
  );

  SetFlagClickHandler(12, // 0x1000 - Kill floor
    procedure (Sender: TObject)
    begin
      var cb := (Sender as TCheckBox);
      if cb.checked then
        if CurrentProject = IJIM then
            Bit0.checked := True; // 0x01 - Floor
    end
  );

  SetFlagClickHandler(13, // 0x2000 - Climbable
    procedure (Sender: TObject)
    begin
      var cb := (Sender as TCheckBox);
      if cb.checked then
        if CurrentProject = IJIM then
            Bit2.checked := True; // 0x04 - Collision
    end
  );

  SetFlagClickHandler(14, // 0x4000 - Track
    procedure (Sender: TObject)
    begin
      var cb := (Sender as TCheckBox);
      if cb.checked then
        if CurrentProject = IJIM then
            Bit0.checked := True; // 0x01 - Floor
    end
  );

  SetFlagClickHandler(24, // 0x1000000 - Ledge
    procedure (Sender: TObject)
    begin
      var cb := (Sender as TCheckBox);
      if cb.checked then
        if CurrentProject = IJIM then
            Bit2.checked := True; // 0x04 - Collision
    end
  );

  SetFlagClickHandler(25, // 0x2000000 - Water ledge
    procedure (Sender: TObject)
    begin
      var cb := (Sender as TCheckBox);
      if cb.checked then
        if CurrentProject = IJIM then
            Bit2.checked := True; // 0x04 - Collision
    end
  );

  SetFlagClickHandler(28, // 0x10000000 - Whip aim
    procedure (Sender: TObject)
    begin
      var cb := (Sender as TCheckBox);
      if cb.checked then
        if CurrentProject = IJIM then
            Bit0.checked := True; // 0x01 - Floor
    end
  );

  // surface type flags
  SetSurfaceTypeClickHandler(9); // 0x200 - Horizon sky
  SetSurfaceTypeClickHandler(10); // 0x400 - Ceiling sky
  SetSurfaceTypeClickHandler(16); // 0x10000 - Metal
  SetSurfaceTypeClickHandler(18); // 0x40000 - Shallow wate
  SetSurfaceTypeClickHandler(19); // 0x80000 - Dirt

  if CurrentProject = IJIM then
    begin
      SetSurfaceTypeClickHandler(7);  // 0x80 - Aetherium
      SetSurfaceTypeClickHandler(17); // 0x20000 - Water
      SetSurfaceTypeClickHandler(20); // 0x100000 - Web
      SetSurfaceTypeClickHandler(21); // 0x200000 - Lava
      SetSurfaceTypeClickHandler(22); // 0x400000 - Snow
      SetSurfaceTypeClickHandler(23); // 0x800000 - Wood
      SetSurfaceTypeClickHandler(29); // 0x20000000 - Echo
      SetSurfaceTypeClickHandler(30); // 0x40000000 - Wood echo
      SetSurfaceTypeClickHandler(31); // 0x80000000 - Dirt echo
  end;

  SetFlags(flags);
  if ShowModal <> mrOK then
    Result := flags
  else
    Result := GetFlags;
end;

Function TFlagEdit.EditAdjoinFlags(flags: Longint): Longint;
begin
  SetNameDesc(0, '0x01 - Visible', 'Render past adjoin surface.');
  SetNameDesc(1, '0x02 - Move', 'Enable movement through adjoin surface.');  // original flag name: ADJOIN_MOVE
  if CurrentProject <> IJIM then
    SetName(2, '0x04 - Doesn''t block sound');
  SetNameDesc(3, '0x08 - No AI move', 'Dissallow movement through adjoin for AI actor. Use in combination with "Move" flag.');
  SetNameDesc(4, '0x10 - No player move', 'Dissallow movement through adjoin for player. Use in combination with "Move" flag.');
  if CurrentProject = IJIM then
    SetNameDesc(5, '0x20 - Turned off by setor', 'Rendering of adjoin was turned off by the sector. (game engine flag set via SectorSet).');
  SetNameDesc(31, '0x80000000 - Block light', 'Block light pass to another sector. (SED flag)');

  SetFlagClickHandler(3, // 0x08 - No AI move
    procedure (Sender: TObject)
    begin
      var cb := (Sender as TCheckBox);
      if cb.checked then
            Bit1.checked := True; // 0x02 - Move
    end
  );

  SetFlagClickHandler(4, // 0x10 - No player move
    procedure (Sender: TObject)
    begin
      var cb := (Sender as TCheckBox);
      if cb.checked then
            Bit1.checked := True; // 0x02 - Move
    end
  );

  SetFlags(flags);
  if ShowModal <> mrOK then
    Result := flags
  else
    Result := GetFlags;
end;

Function TFlagEdit.EditThingFlags(flags: Longint): Longint;
begin
  SetNameDesc(0, '0x01 - Emits light', 'Object emits light.' );
  SetName(1, '0x02 - Dead');
  SetNameDesc(4, '0x10 - Invisible', 'Object is invisible but interactable unlike disabled.');
  SetNameDesc(6, '0x40 - Stand on', 'Object can be stood on.');
  SetNameDesc(9, '0x200 - Dying', 'Object is about to be removed from the game. This flag is often used in combination with the ''LIFELEFT'' param.'); // original flag name: SITH_TF_DYING
  SetNameDesc(10, '0x400 - COG Linked', 'Object is linked in COG(s)');
  SetNameDesc(11, '0x800 - No crush', 'Don''t damage impacting object.');
  SetNameDesc(19, '0x80000 - Disabled', 'Object is disabled and hidden.');  // original flag name: SITH_TF_DISABLED
  SetNameDesc(22, '0x400000 - Metal', 'Object is made of metal. Makes metal sound fx when object is hit.');
  SetNameDesc(23, '0x800000 - Dirt', 'Object is dirt. Makes dirt sound fx when object is hit.');
  SetNameDesc(24, '0x1000000 - No sound', 'Object makes no sound.');
  SetNameDesc(27, '0x8000000 - Destroyed in water', 'Object is destroyed when enters the water.');
  SetNameDesc(28, '0x10000000 - Destroyed in air', 'Object is destroyed when enters the air.');
  SetNameDesc(29, '0x20000000 - Makes splash', 'Sends splash message to linked COG(s).');

  if CurrentProject = IJIM then
    begin
      SetName(2, '0x04 - No weapon collision');
      SetNameDesc(3, '0x08 - Whip climb', 'Player can whip this object to climb up the whip.');
      SetName(7, '0x80 - Mountable');
      SetNameDesc(8, '0x100 - Remote', 'Non-local object controlled remotely over the network.');
      SetNameDesc(13, '0x2000 - Wood', 'Object is made of wood. Makes wood sound fx when object is hit.');
      SetNameDesc(14, '0x4000 - Has shadow', 'Object has shadow rendered.');
      SetNameDesc(15, '0x8000 - Jeep block', 'Object blocks jeep player movement and doesn''t collide with other objects.');
      SetNameDesc(16, '0x10000 - Snow', 'Object is made of snow. Makes snow sound fx when object is hit.');
      SetNameDesc(17, '0x20000 - Pulse set', 'Object has pulse timer set and sends pulse events to linked COG. Set by game engine.');
      SetNameDesc(18, '0x40000 - Timer set', 'Object has timer set and sends timer events to linked COG. Set by game engine.');
      SetNameDesc(20, '0x100000 - Seen', 'Object has been seen by the camera. Set by game engine');
      SetNameDesc(25, '0x2000000 - Submerged', 'Object is submerged in water or aetherium. Set by game engine.');
      SetName(26, '0x4000000 - Climbable create');
      SetNameDesc(30, '0x40000000 - Movable', 'Object is pushable/pullable.');
      SetNameDesc(31, '0x80000000 - Whip swing', 'Player can whip this object and swing over.');
    end
  else
    begin
      SetNameDesc(2, '0x04 - Magsealed', 'Reflects shots');
      SetNameDesc(3, '0x08 - Architecture', 'Object should be rendered as part of world architecture');
      SetName(8, '0x100 - Controlled remotely');
      SetName(12, '0x1000 - Not present on Easy');
      SetName(13, '0x2000 - Not present on Medium');
      SetName(14, '0x4000 - Not present on Hard');
      SetName(15, '0x8000 - Not present on Multiplayer');
      SetName(16, '0x10000 - Not present on Singleplayer');
      SetNameDesc(25, '0x2000000 - Submerged', 'Object is submerged');
      SetName(26, '0x4000000 - Prelit');
    end;

  SetFlags(flags);

  if ShowModal <> mrOK then
    Result := flags
  else
    Result := GetFlags;
end;

Function TFlagEdit.EditFaceFlags(flags: Longint): Longint;
begin
  SetNameDesc(0, '0x01 - Double-sided', 'Disable backface culling. Not available for world geometry surface.');
  SetNameDesc(1, '0x02 - Translucent', 'Enable texture transparency. (Adjoin surface only)');
  SetNameDesc(2, '0x04 - Clamp U', 'Clamp texture in U coordinate. (Default wrap)');
  SetNameDesc(3, '0x08 - Clamp V', 'Clamp texture in V coordinate. (Default wrap)');
  SetNameDesc(4, '0x10 - No texture filtering', 'Set texture min/max texture filtering to nearest.');
  SetNameDesc(5, '0x20 - ZWrite disabled', 'Disable write to the depth buffer.');
  if CurrentProject = IJIM then
    SetNameDesc(8, '0x100 - Fog enabled', 'Enable fog blending. Set automatically for all but sky surfaces');
  SetNameDesc(31, '0x80000000 - Texture Flipped', 'Flip texture. (SED flag)');

  SetFlags(flags);
  Bits[0].Enabled := false; // disable editing of flag 0x01 - DoubleSided

  if ShowModal <> mrOK then
    Result := flags
  else
    Result := GetFlags;
end;

Function TFlagEdit.EditLevelITMFlags(flags: Longint): Longint;
begin
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
  SetNameDesc(0, 'Not blocked', 'Not blocked by geometry');
  SetFlags(flags);
  if ShowModal <> mrOK then
    Result := flags
  else
    Result := GetFlags;
end;

end.
