unit U_lheader;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  StdCtrls, ExtCtrls, FieldEdit, J_Level;

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
    procedure FormCreate(Sender: TObject);
    procedure BNCmpClick(Sender: TObject);
  private
    { Private declarations }
    vGrav, vSkyZ, vSkyX, vSkyY, vHorDist, vPar, vHorX, vHorY, vMM1, vMM2, vMM3,
      vMM4, vLOD1, vLOD2, vLOD3, vLOD4, vGouraud, vPerp, vCMP, vPPu: TValInput;

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
end;

procedure TLHEdit.BNCmpClick(Sender: TObject);
begin
  vCMP.SetAsString(ResPicker.PickCMP(vCMP.s));
end;

Function TLHEdit.EditHeader: boolean;
begin
  Result := false;
  With Level.Header do
  begin
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
  end;
end;

end.
