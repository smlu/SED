unit U_Tools;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  StdCtrls, ComCtrls, ExtCtrls, FieldEdit, globalVars, geometry;

type
  TToolForm = class(TForm)
    Pages: TPageControl;
    Panel1: TPanel;
    BNClose: TButton;
    Label3: TLabel;
    PGTrans: TTabSheet;
    RGAxis: TRadioGroup;
    EBAngle: TEdit;
    BNRotate: TButton;
    Label5: TLabel;
    EBSfactor: TEdit;
    BNScale: TButton;
    Label6: TLabel;
    EBDX: TEdit;
    EBDY: TEdit;
    EBDZ: TEdit;
    BNTranslate: TButton;
    Label7: TLabel;
    Label8: TLabel;
    Label9: TLabel;
    PDoor: TTabSheet;
    BNDoor: TButton;
    BNStraighten: TButton;
    BNFlip: TButton;
    CBScaleTX: TCheckBox;
    BNCalcAngle: TButton;
    GridNCamera: TTabSheet;
    Label1: TLabel;
    Label2: TLabel;
    Label4: TLabel;
    EB3DPX: TEdit;
    EB3DPY: TEdit;
    EB3DPZ: TEdit;
    EB3DPPCH: TEdit;
    EB3DPYAW: TEdit;
    Label10: TLabel;
    EBGridX: TEdit;
    EBGridY: TEdit;
    EBGridZ: TEdit;
    EBCamX: TEdit;
    EBCamY: TEdit;
    EBCamZ: TEdit;
    EBCamPCH: TEdit;
    EBCamYAW: TEdit;
    EBCamROL: TEdit;
    Label12: TLabel;
    BNSetGrid: TButton;
    Button1: TButton;
    Button2: TButton;
    EBGridPCH: TEdit;
    EBGridYAW: TEdit;
    EBGridROL: TEdit;
    Label11: TLabel;
    CBScaleXYZ: TCheckBox;
    procedure BNCloseClick(Sender: TObject);
    procedure FormCreate(Sender: TObject);
    procedure BNRotateClick(Sender: TObject);
    procedure BNScaleClick(Sender: TObject);
    procedure BNTranslateClick(Sender: TObject);
    procedure BNDoorClick(Sender: TObject);
    procedure BNStraightenClick(Sender: TObject);
    procedure BNFlipClick(Sender: TObject);
    procedure BNCalcAngleClick(Sender: TObject);
    procedure FormActivate(Sender: TObject);
    procedure GridNCameraEnter(Sender: TObject);
    procedure BNSetGridClick(Sender: TObject);
    procedure Button1Click(Sender: TObject);
    procedure Button2Click(Sender: TObject);
  private
    { Private declarations }
    vangle, vdx, vdy, vdz, vsfactor: TvalInput;

    vGridX, vGridY, vGridZ, vGridPCH, vGridYAW, vGridROL, vCamX, vCamY, vCamZ,
      vCamPCH, vCamYAW, vCamROL, v3DPX, v3DPY, v3DPZ, v3DPPCH,
      v3DPYAW: TvalInput;

  public

    { Public declarations }
  end;

var
  ToolForm: TToolForm;

implementation

uses Jed_Main, J_Level, Misc_utils, Lev_utils, U_Preview, System.Math;

{$R *.DFM}

procedure TToolForm.BNCloseClick(Sender: TObject);
begin
  Hide;
end;

procedure TToolForm.FormCreate(Sender: TObject);
begin
  vangle := TvalInput.Create(EBAngle);
  vangle.SetAsFloat(0);
  vsfactor := TvalInput.Create(EBSfactor);
  vsfactor.SetAsFloat(1);
  vdx := TvalInput.Create(EBDX);
  vdx.SetAsFloat(0);
  vdy := TvalInput.Create(EBDY);
  vdy.SetAsFloat(0);
  vdz := TvalInput.Create(EBDZ);
  vdz.SetAsFloat(0);

  vGridX := TvalInput.Create(EBGridX);
  vGridX.SetAsFloat(0);
  vGridY := TvalInput.Create(EBGridY);
  vGridY.SetAsFloat(0);
  vGridZ := TvalInput.Create(EBGridZ);
  vGridZ.SetAsFloat(0);

  vGridPCH := TvalInput.Create(EBGridPCH);
  vGridPCH.SetAsFloat(0);
  vGridYAW := TvalInput.Create(EBGridYAW);
  vGridYAW.SetAsFloat(0);
  vGridROL := TvalInput.Create(EBGridROL);
  vGridROL.SetAsFloat(0);

  vCamX := TvalInput.Create(EBCamX);
  vCamX.SetAsFloat(0);
  vCamY := TvalInput.Create(EBCamY);
  vCamY.SetAsFloat(0);
  vCamZ := TvalInput.Create(EBCamZ);
  vCamZ.SetAsFloat(0);

  vCamPCH := TvalInput.Create(EBCamPCH);
  vCamPCH.SetAsFloat(0);
  vCamYAW := TvalInput.Create(EBCamYAW);
  vCamPCH.SetAsFloat(0);
  vCamROL := TvalInput.Create(EBCamROL);
  vCamROL.SetAsFloat(0);

  v3DPX := TvalInput.Create(EB3DPX);
  v3DPX.SetAsFloat(0);
  v3DPY := TvalInput.Create(EB3DPY);
  v3DPY.SetAsFloat(0);
  v3DPZ := TvalInput.Create(EB3DPZ);
  v3DPZ.SetAsFloat(0);

  v3DPPCH := TvalInput.Create(EB3DPPCH);
  v3DPPCH.SetAsFloat(0);
  v3DPYAW := TvalInput.Create(EB3DPYAW);
  v3DPPCH.SetAsFloat(0);

end;

procedure TToolForm.BNRotateClick(Sender: TObject);
var
  angle: double;
begin
  feValDouble(vangle.s, angle);
  JedMain.RotateObject(angle, RGAxis.ItemIndex);
end;

procedure TToolForm.BNFlipClick(Sender: TObject);
begin
  JedMain.FlipObject(RGAxis.ItemIndex);
end;

procedure TToolForm.BNScaleClick(Sender: TObject);
var
  sfactor: double;
  how: integer;
begin
  feValDouble(vsfactor.s, sfactor);
  how := 0;
  if CBScaleTX.Checked then
    how := sc_ScaleTX;
  if CBScaleXYZ.Checked then
  else
    case RGAxis.ItemIndex of
      0:
        how := how or sc_ScaleX;
      1:
        how := how or sc_ScaleY;
      2:
        how := how or sc_ScaleZ;
      3:
        how := how or sc_ScaleGrid;
    end;

  JedMain.ScaleObject(sfactor, how);
end;

procedure TToolForm.BNTranslateClick(Sender: TObject);
var
  dx, dy, dz: double;
begin
  feValDouble(vdx.s, dx);
  feValDouble(vdy.s, dy);
  feValDouble(vdz.s, dz);
  JedMain.TranslateObject(dx, dy, dz);
end;

procedure TToolForm.BNDoorClick(Sender: TObject);
begin
  JedMain.MakeDoor;
end;

procedure TToolForm.BNStraightenClick(Sender: TObject);
var
  i, j: integer;
begin
  for i := 0 to Level.Sectors.count - 1 do
    With Level.Sectors[i] do
      for j := 0 to surfaces.count - 1 do
        With surfaces[j] do
          RecalcAll;
  Preview3D.ReloadLevel;
  JedMain.LevelChanged;
end;

procedure TToolForm.BNCalcAngleClick(Sender: TObject);
var
  surf1, surf2: TJKSurface;
  d: double;
  n: integer;
  sc, sf: integer;
  lvec, vec: TVector;
  px, py, pz: double;
begin
  With JedMain do
  begin
    if mapMode <> MM_SF then
    begin
      ShowMessage('You must be in surface mode');
      exit;
    end;

    n := sfsel.AddSF(Cur_SC, Cur_SF);
    if sfsel.count <> 2 then
    begin
      sfsel.DeleteN(n);
      ShowMessage('You must have 2 surfaces selected');
      exit;
    end;

    if sfsel.FindSF(Cur_SC, Cur_SF) = 0 then
      begin
        sfsel.GetSCSF(0, sc, sf);
        surf1 := level.sectors[sc].surfaces[sf];
        sfsel.GetSCSF(1, sc, sf);
        surf2 := level.sectors[sc].surfaces[sf];
      end
    else
      begin
        sfsel.GetSCSF(0, sc, sf);
        surf2 := level.sectors[sc].surfaces[sf];

        sfsel.GetSCSF(1, sc, sf);
        surf1 := level.sectors[sc].surfaces[sf];
      end;

    sfsel.Clear;

    d := surf1.normal.Dot(surf2.normal); //SMult(surf1.normal.dx, surf1.normal.dy, surf1.normal.dz, surf2.normal.dx, surf2.normal.dy, surf2.normal.dz);
    d := 180 - ArcCos(d) / pi * 180;
    vangle.SetAsFloat(d);

    vec := surf1.normal.Cross(surf2.normal);
//    VMult(surf1.normal.dx, surf1.normal.dy, surf1.normal.dz, surf2.normal.dx,
//      surf2.normal.dy, surf2.normal.dz, vec.dx, vec.dy, vec.dz);
    //if not Normalize(vec) then
    if vec.Normalize = 0 then     
    begin
      ShowMessage('The surfaces are already coplanar');
      exit;
    end;

    lvec := surf2.normal.Cross(vec);
//    VMult(surf2.normal.dx, surf2.normal.dy, surf2.normal.dz, vec.dx, vec.dy,
//      vec.dz, lvec.dx, lvec.dy, lvec.dz);

    with surf1.vertices[0] do
    begin
      px := x;
      py := y;
      pz := z;
    end;

    with surf2.vertices[0] do
      PlaneLineXnNew(surf1.normal, px, py, pz, x, y, z, x + lvec.dx,
        y + lvec.dy, z + lvec.dz, px, py, pz);

    with JedMain.Renderer do
    begin
      GridX := px;
      GridY := py;
      GridZ := pz;
      SetGridNormal(vec.dx, vec.dy, vec.dz);
      SetGridXnormal(lvec.dx, lvec.dy, lvec.dz);
    end;

    RGAxis.ItemIndex := rt_Grid;
    JedMain.SetMapMode(MM_SC);
    JedMain.Invalidate;
  end;
end;

procedure TToolForm.FormActivate(Sender: TObject);
begin
  if Assigned(Pages.ActivePage.OnEnter) then
    Pages.ActivePage.OnEnter(Pages.ActivePage);
end;

procedure TToolForm.GridNCameraEnter(Sender: TObject);
var
  x, y, z, pch, yaw, rol: double;
begin
  vGridX.SetAsFloat(JedMain.Renderer.GridX);
  vGridY.SetAsFloat(JedMain.Renderer.GridY);
  vGridZ.SetAsFloat(JedMain.Renderer.GridZ);
  With JedMain.Renderer do
    sysGetPYR(gxnormal, gynormal, gnormal, pch, yaw, rol);

  vGridPCH.SetAsFloat(pch);
  vGridYAW.SetAsFloat(yaw);
  vGridROL.SetAsFloat(rol);

  JedMain.GetCam(x, y, z, pch, yaw, rol);
  vCamX.SetAsFloat(x);
  vCamY.SetAsFloat(y);
  vCamZ.SetAsFloat(z);

  vCamPCH.SetAsFloat(pch);
  vCamYAW.SetAsFloat(yaw);
  vCamROL.SetAsFloat(rol);

  Preview3D.GetCam(x, y, z, pch, yaw);

  v3DPX.SetAsFloat(x);
  v3DPY.SetAsFloat(y);
  v3DPZ.SetAsFloat(z);
  v3DPPCH.SetAsFloat(pch);
  v3DPYAW.SetAsFloat(yaw);

end;

procedure TToolForm.BNSetGridClick(Sender: TObject);
var
  pch, yaw, rol: double;
  x, y, z: TVector;
begin
  With JedMain.Renderer do
  begin
    feValDouble(vGridX.s, GridX);
    feValDouble(vGridY.s, GridY);
    feValDouble(vGridZ.s, GridZ);

    feValDouble(vGridPCH.s, pch);
    feValDouble(vGridYAW.s, yaw);
    feValDouble(vGridROL.s, rol);

    x.SetCoords(1, 0, 0);
    y.SetCoords(0, 1, 0);
    z.SetCoords(0, 0, 1);

    { PCH,ROL,YAW }
    RotateVector(x, pch, 0, 0);
    RotateVector(x, 0, rol, 0);
    RotateVector(x, 0, 0, yaw);
    RotateVector(y, pch, 0, 0);
    RotateVector(y, 0, rol, 0);
    RotateVector(y, 0, 0, yaw);
    RotateVector(z, pch, 0, 0);
    RotateVector(z, 0, rol, 0);
    RotateVector(z, 0, 0, yaw);

    { So PCH - x , YAW - y, ROL - z }
    JedMain.Renderer.SetGridNormal(z.dx, z.dy, z.dz);
    JedMain.Renderer.SetGridXnormal(x.dx, x.dy, x.dz);

  end;
  JedMain.Invalidate;
end;

procedure TToolForm.Button1Click(Sender: TObject);
var
  x, y, z, pch, yaw, rol: double;
begin
  feValDouble(vCamX.s, x);
  feValDouble(vCamY.s, y);
  feValDouble(vCamZ.s, z);
  feValDouble(vCamPCH.s, pch);
  feValDouble(vCamYAW.s, yaw);
  feValDouble(vCamROL.s, rol);
  JedMain.SetCam(x, y, z, pch, yaw, rol);
end;

procedure TToolForm.Button2Click(Sender: TObject);
var
  x, y, z, pch, yaw: double;
begin
  feValDouble(v3DPX.s, x);
  feValDouble(v3DPY.s, y);
  feValDouble(v3DPZ.s, z);
  feValDouble(v3DPPCH.s, pch);
  feValDouble(v3DPYAW.s, yaw);
  Preview3D.SetCam(x, y, z, pch, yaw);
end;

end.
