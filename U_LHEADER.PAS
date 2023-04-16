unit U_lheader;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  StdCtrls, ExtCtrls, FieldEdit, J_Level, misc_utils, GlobalVars;

type
  TLHEdit = class(TForm)
    Panel1: TPanel;
    Label1: TLabel;
    EBGravity: TEdit;
    EBSkyZ: TEdit;
    Label3: TLabel;
    Label2: TLabel;
    EBHorDist: TEdit;
    Label4: TLabel;
    EBHorPPRev: TEdit;
    Label5: TLabel;
    EBMip1: TEdit;
    EBMip2: TEdit;
    EBMip3: TEdit;
    EBMip4: TEdit;
    EBLOD1: TEdit;
    EBLOD2: TEdit;
    EBLOD3: TEdit;
    EBLOD4: TEdit;
    Label6: TLabel;
    EBGouraud: TEdit;
    Label7: TLabel;
    EBPerspective: TEdit;
    Label8: TLabel;
    EBSkyOffsX: TEdit;
    EBSkyOffsY: TEdit;
    Label9: TLabel;
    EBHorSkyY: TEdit;
    EBHorSkyX: TEdit;
    Label10: TLabel;
    EBCMP: TEdit;
    Label11: TLabel;
    BNOK: TButton;
    Button1: TButton;
    BNCmp: TButton;
    EBPPU: TEdit;
    Label12: TLabel;
    CBFog: TCheckBox;
    ColorDialog1: TColorDialog;
    EBFogColor: TEdit;
    EBFogStart: TEdit;
    EBFogEnd: TEdit;
    BNFogColor: TButton;
    GBFog: TGroupBox;
    procedure FormCreate(Sender: TObject);
    procedure BNCmpClick(Sender: TObject);
    procedure CBFogClick(Sender: TObject);
    procedure BNFogColorClick(Sender: TObject);
  private
    { Private declarations }
    vGrav, vSkyZ, vSkyX, vSkyY, vHorDist, vPar, vHorX, vHorY, vMM1, vMM2, vMM3,
      vMM4, vLOD1, vLOD2, vLOD3, vLOD4, vGouraud, vPerp, vCMP, vPPu, vFogColor,
      vFogStart, vFogEnd: TValInput;

    Procedure EnableFog(enable: Boolean);
    Procedure InitProjectMode;
  public
    { Public declarations }
    Function EditHeader: boolean;
  end;

var
  LHEdit: TLHEdit;

implementation

uses ResourcePicker;

{$R *.DFM}

procedure TLHEdit.FormCreate(Sender: TObject);
begin
  vGrav := TValInput.Create(EBGravity);
  vSkyZ := TValInput.Create(EBSkyZ);
  vSkyX := TValInput.Create(EBSkyOffsX);
  vSkyY := TValInput.Create(EBSkyOffsY);
  vHorDist := TValInput.Create(EBHorDist);
  vPar := TValInput.Create(EBHorPPRev);
  vHorX := TValInput.Create(EBHorSkyX);
  vHorY := TValInput.Create(EBHorSkyY);
  vMM1 := TValInput.Create(EBMip1);
  vMM2 := TValInput.Create(EBMip2);
  vMM3 := TValInput.Create(EBMip3);
  vMM4 := TValInput.Create(EBMip4);
  vLOD1 := TValInput.Create(EBLOD1);
  vLOD2 := TValInput.Create(EBLOD2);
  vLOD3 := TValInput.Create(EBLOD3);
  vLOD4 := TValInput.Create(EBLOD4);
  vGouraud := TValInput.Create(EBGouraud);
  vPerp := TValInput.Create(EBPerspective);
  vCMP := TValInput.Create(EBCMP);
  vPPu := TValInput.Create(EBPPU);
  vFogColor := TValInput.Create(EBFogColor);
  vFogStart := TValInput.Create(EBFogStart);
  vFogEnd := TValInput.Create(EBFogEnd);
end;

procedure TLHEdit.BNCmpClick(Sender: TObject);
begin
  vCMP.SetAsString(ResPicker.PickCMP(vCMP.s));
end;

procedure TLHEdit.BNFogColorClick(Sender: TObject);
var f: TColor;
  c: TColorF;
begin
  ValColor(vFogColor.s, c);
  ColorDialog1.Color := RGB(
    Round(c.r * 255),
    Round(c.g * 255),
    Round(c.b * 255)
  );
  if ColorDialog1.Execute(self.Handle) then
    begin
      f := ColorDialog1.Color;
      if f = -1 then  f :=0;       
      c := MakeColor(
        GetRValue(f) / 255,
        GetGValue(f) / 255,
        GetBValue(f) / 255,
        c.a
      );
      vFogColor.SetAsString(ColorToStr(c, true));
    end;
end;

procedure TLHEdit.CBFogClick(Sender: TObject);
begin
     EnableFog(CBFog.Checked);
end;

Function TLHEdit.EditHeader: boolean;
begin
  Result := false;
  With Level.Header do
  begin
    InitProjectMode();
    vGrav.SetAsFloat(Gravity);
    vSkyZ.SetAsFloat(CeilingSkyZ);
    vSkyX.SetAsFloat(CeilingSkyOffs[1]);
    vSkyY.SetAsFloat(CeilingSkyOffs[2]);
    vHorDist.SetAsFloat(HorDistance);
    vPar.SetAsFloat(HorPixelsPerRev);
    vHorX.SetAsFloat(HorSkyOffs[1]);
    vHorY.SetAsFloat(HorSkyOffs[2]);
    vMM1.SetAsFloat(MipMapDist[1]);
    vMM2.SetAsFloat(MipMapDist[2]);
    vMM3.SetAsFloat(MipMapDist[3]);
    vMM4.SetAsFloat(MipMapDist[4]);
    vLOD1.SetAsFloat(LODDist[1]);
    vLOD2.SetAsFloat(LODDist[2]);
    vLOD3.SetAsFloat(LODDist[3]);
    vLOD4.SetAsFloat(LODDist[4]);
    vGouraud.SetAsFloat(GouraudDist);
    vPerp.SetAsFloat(PerspDist);
    vPPu.SetAsFloat(Level.PPUnit);
    vCMP.SetAsString(Level.MasterCMP);
    EnableFog(Fog.Enabled);
    vFogColor.SetAsString(ColorToStr(Fog.Color, true));
    vFogStart.SetAsFloat(Fog.FogStart);
    vFogEnd.SetAsFloat(Fog.FogEnd);

    if ShowModal <> MrOK then
      exit;
    Gravity := vGrav.AsFloat;
    CeilingSkyZ := vSkyZ.AsFloat;
    CeilingSkyOffs[1] := vSkyX.AsFloat;
    CeilingSkyOffs[2] := vSkyY.AsFloat;
    HorDistance := vHorDist.AsFloat;
    HorPixelsPerRev := vPar.AsFloat;
    HorSkyOffs[1] := vHorX.AsFloat;
    HorSkyOffs[2] := vHorY.AsFloat;
    MipMapDist[1] := vMM1.AsFloat;
    MipMapDist[2] := vMM2.AsFloat;
    MipMapDist[3] := vMM3.AsFloat;
    MipMapDist[4] := vMM4.AsFloat;
    LODDist[1] := vLOD1.AsFloat;
    LODDist[2] := vLOD2.AsFloat;
    LODDist[3] := vLOD3.AsFloat;
    LODDist[4] := vLOD4.AsFloat;
    GouraudDist := vGouraud.AsFloat;
    Level.PPUnit := vPPu.AsFloat;
    PerspDist := vPerp.AsFloat;
    Result := CompareText(Level.MasterCMP, vCMP.s) <> 0;
    Level.MasterCMP := vCMP.s;
    Fog.Enabled := CBFog.Checked;
    if not ValColor(vFogColor.s, Fog.Color) and Fog.Enabled then
       MsgBox('The fog color was not set because the color value is invalid', 'Warning', MB_OK);
    Fog.FogStart := vFogStart.AsFloat;
    Fog.FogEnd := vFogEnd.AsFloat;

    Result:=True;
  end;
end;

Procedure TLHEdit.EnableFog(enable: Boolean);
begin
  CBFog.Checked := enable;
  EBFogColor.Enabled := enable;
  BNFogColor.Enabled := enable;
  EBFogStart.Enabled := enable;
  EBFogEnd.Enabled := enable;
end;

Procedure TLHEdit.InitProjectMode;
begin
  if CurrentProject = TProjectType.IJIM then
    begin
      Label5.Visible := false;
      EBMip1.Visible := false;
      EBMip2.Visible := false;
      EBMip3.Visible := false;
      EBMip4.Visible := false;

      Label8.Visible        := false;
      EBGouraud.Visible     := false;
      Label7.Visible        := false;
      EBPerspective.Visible := false;
      Label11.Visible       := false;
      EBCMP.Visible         := false;
      BNCmp.Visible         := false;
      Label12.Visible       := false;
      EBPPU.Visible         := false;

      GBFog.Visible         := true;
    end
  else
    begin
      Label5.Visible := true;
      EBMip1.Visible := true;
      EBMip2.Visible := true;
      EBMip3.Visible := true;
      EBMip4.Visible := true;

      Label8.Visible        := true;
      EBGouraud.Visible     := true;
      Label7.Visible        := true;
      EBPerspective.Visible := true;
      Label11.Visible       := true;
      EBCMP.Visible         := true;
      BNCmp.Visible         := true;
      Label12.Visible       := true;
      EBPPU.Visible         := true;

      GBFog.Visible         := false;
    end;
end;

end.
