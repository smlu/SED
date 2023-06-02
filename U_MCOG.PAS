unit U_Mcog;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  StdCtrls, Spin, Buttons;

type
  TMasterCOG = class(TForm)
    Label5: TLabel;
    Label7: TLabel;
    Label8: TLabel;
    Label9: TLabel;
    Label10: TLabel;
    Label11: TLabel;
    Label12: TLabel;
    Label6: TLabel;
    CBRifle: TCheckBox;
    CBCrossbow: TCheckBox;
    CBRepeater: TCheckBox;
    CBRailgun: TCheckBox;
    CBSaber: TCheckBox;
    CBConcrifle: TCheckBox;
    Rank: TComboBox;
    StartObj: TListBox;
    GenerateBtn: TBitBtn;
    CloseBtn: TBitBtn;
    SaveCOG: TSaveDialog;
    Label1: TLabel;
    Label2: TLabel;
    Label3: TLabel;
    Label4: TLabel;
    Label13: TLabel;
    Label14: TLabel;
    Label15: TLabel;
    Label16: TLabel;
    Label17: TLabel;
    Label18: TLabel;
    Label19: TLabel;
    Label20: TLabel;
    Label21: TLabel;
    Label22: TLabel;
    Label23: TLabel;
    procedure FormCreate(Sender: TObject);
    procedure GenerateBtnClick(Sender: TObject);
  private
    { Private declarations }
  public
    { Public declarations }
  end;

var
  MasterCOG: TMasterCOG;

implementation

uses Unit1;

{$R *.DFM}

procedure TMasterCOG.FormCreate(Sender: TObject);
begin
 Rank.ItemIndex := 0; // set default rank to uninitiated
end;

procedure TMasterCOG.GenerateBtnClick(Sender: TObject);
var COG : TextFile;
    T : char;
    i : integer;
begin
 T := chr($09);  // tab

 if SaveCOG.execute then
  BEGIN
   {Create COG File and write junk at top}
   AssignFile(COG, SaveCOG.FileName);
   Rewrite(COG);
   Writeln(COG, '# Jedi Knight COG Script');
   Writeln(COG, '#');
   Writeln(COG, '# Master COG file for ');
   Writeln(COG, '#');

   {symbols section}
   Writeln(COG, '# ========================================================================================');
   Writeln(COG, '');
   Writeln(COG, 'symbols');
   Writeln(COG, 'message' + T + 'startup');
   Writeln(COG, 'int' + T + T + 'player' + T + 'local');
   Writeln(COG, '');
   Writeln(COG, 'end');
   Writeln(COG, '');
   Writeln(COG, '# ========================================================================================');
   Writeln(COG, '');

   {code section}
   Writeln(COG, 'code');
   Writeln(COG, '');
   Writeln(COG, 'startup:');
   Writeln(COG, T + '// Register COG as master COG');
   Writeln(COG, T + 'SetMasterCOG(GetSelfCOG());');
   Writeln(COG, '');
   Writeln(COG, T + 'player = GetLocalPlayerThing();');
   Writeln(COG, '');

   {initialize and display goals}
   Writeln(COG, T + '// Initialise Goals');
   Writeln(COG, T + 'SetInv(player, 99, 1000);');
   Writeln(COG, '');
   Writeln(COG, T + '// Display Goals');

   for i := 0 to StartObj.Items.Count - 1 do
    if StartObj.Selected[i] then
     Writeln(COG, T + 'SetGoalFlags(player, ' + IntToStr(i) + ', 1);');

   {give weapons to player}
   Writeln(COG, '');
   Writeln(COG, T + '// Give player weapons and ammo');
   Writeln(COG, T + 'SetInv(player, 1, 1);  // Fists');
   Writeln(COG, T + 'SetInv(player, 2, 1);  // Bryar Pistol');
   if CBRifle.Checked     then Writeln(COG, T + 'SetInv(player, 3, 1);  // ST rifle');
   if CBCrossbow.Checked  then Writeln(COG, T + 'SetInv(player, 5, 1);  // Crossbow');
   if CBRepeater.Checked  then Writeln(COG, T + 'SetInv(player, 6, 1);  // Repeater');
   if CBRailgun.Checked   then Writeln(COG, T + 'SetInv(player, 7, 1);  // Rail Gun');
   if CBConcRifle.Checked then Writeln(COG, T + 'SetInv(player, 9, 1);  // Concussion Rifle');
   if CBSaber.Checked     then Writeln(COG, T + 'SetInv(player, 10, 1); // Lightsaber');

   {give ammo to player}
   if SEEnergy.Value > 0  then Writeln(COG, T + 'SetInv(player, 11, ' + IntToStr(SEEnergy.Value) + '); // Energy');
   if SEPower.Value > 0   then Writeln(COG, T + 'SetInv(player, 12, ' + IntToStr(SEPower.Value) + '); // Power');
   if SECharges.Value > 0 then Writeln(COG, T + 'SetInv(player, 15, ' + IntToStr(SECharges.Value) + '); // Rail Charges');
   if SETD.Value > 0      then Writeln(COG, T + 'SetInv(player, 4, ' + IntToStr(SETD.Value) + '); // Thermal Detonators');
   if SESeqChg.Value > 0  then Writeln(COG, T + 'SetInv(player, 8, ' + IntToStr(SESeqChg.Value) + '); // Sequencer Charges');
   Writeln(COG, '');

   {force powers}
   Writeln(COG, T + '// Give player force powers');
   Writeln(COG, T + 'SetInvAvailable(player, 21, 1); // Jump');
   Writeln(COG, T + 'SetInvAvailable(player, 22, 1); // Speed');
   Writeln(COG, T + 'SetInvAvailable(player, 23, 1); // Seeing');
   Writeln(COG, T + 'SetInvAvailable(player, 24, 1); // Pull');
   Writeln(COG, T + 'SetInvAvailable(player, 25, 1); // Healing');
   Writeln(COG, T + 'SetInvAvailable(player, 26, 1); // Persuasion');
   Writeln(COG, T + 'SetInvAvailable(player, 27, 1); // Blinding');
   Writeln(COG, T + 'SetInvAvailable(player, 28, 1); // Absorb');
   Writeln(COG, T + 'SetInvAvailable(player, 29, 1); // Protection');
   Writeln(COG, T + 'SetInvAvailable(player, 30, 1); // Throw');
   Writeln(COG, T + 'SetInvAvailable(player, 31, 1); // Grip');
   Writeln(COG, T + 'SetInvAvailable(player, 32, 1); // Lightning');
   Writeln(COG, T + 'SetInvAvailable(player, 33, 1); // Destruction');
   Writeln(COG, T + 'SetInvAvailable(player, 34, 1); // DeadlySight');

   if SEJump.Value > 0     then Writeln(COG, T + 'SetInv(player, 21, ' + IntToStr(SEJump.Value) + ');');
   if SESpeed.Value > 0    then Writeln(COG, T + 'SetInv(player, 22, ' + IntToStr(SESpeed.Value) + ');');
   if SESeeing.Value > 0   then Writeln(COG, T + 'SetInv(player, 23, ' + IntToStr(SESeeing.Value) + ');');
   if SEPull.Value > 0     then Writeln(COG, T + 'SetInv(player, 24, ' + IntToStr(SEPull.Value) + ');');
   if SEHealing.Value > 0  then Writeln(COG, T + 'SetInv(player, 25, ' + IntToStr(SEHealing.Value) + ');');
   if SEPersuade.Value > 0 then Writeln(COG, T + 'SetInv(player, 26, ' + IntToStr(SEPersuade.Value) + ');');
   if SEBlinding.Value > 0 then Writeln(COG, T + 'SetInv(player, 27, ' + IntToStr(SEBLinding.Value) + ');');
   if SEAbsorb.Value > 0   then Writeln(COG, T + 'SetInv(player, 28, ' + IntToStr(SEAbsorb.Value) + ');');
   if SEProtect.Value > 0  then Writeln(COG, T + 'SetInv(player, 29, ' + IntToStr(SEProtect.Value) + ');');
   if SEThrow.Value > 0    then Writeln(COG, T + 'SetInv(player, 30, ' + IntToStr(SEThrow.Value) + ');');
   if SEGrip.Value > 0     then Writeln(COG, T + 'SetInv(player, 31, ' + IntToStr(SEGrip.Value) + ');');
   if SELightng.Value > 0  then Writeln(COG, T + 'SetInv(player, 32, ' + IntToStr(SELightng.Value) + ');');
   if SEDestruct.Value > 0 then Writeln(COG, T + 'SetInv(player, 33, ' + IntToStr(SEDestruct.Value) + ');');
   if SEDeadly.Value > 0   then Writeln(COG, T + 'SetInv(player, 34, ' + IntToStr(SEDeadly.Value) + ');');
   Writeln(COG, '');

   {Ranking}
   Writeln(COG, T + '// Ranking');
   if Rank.ItemIndex > 0 then Writeln(COG, T + 'SetInv(player, 20, ' + IntToStr(Rank.ItemIndex) + ');');
   Writeln(COG, '');

   {initialize weapon}
   Writeln(COG, T + '// Initialize weapon.');
   Writeln(COG, T + 'SetFireWait(player, -1);');
   Writeln(COG, T + 'SetMountWait(player, 0);');
   Writeln(COG, T + 'SetCurInvWeapon(player, 0);');
   Writeln(COG, T + 'SelectWeapon(player, AutoSelectWeapon(player, 1));');
   Writeln(COG, '');

   Writeln(COG, T + 'Return;');
   Writeln(COG, '');
   Writeln(COG, 'end');

   CloseFile(COG);
  END;
end;

end.
