unit U_Tools;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  StdCtrls, ComCtrls, ExtCtrls, FieldEdit, Math, globalVars, geometry;

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
    vangle,
    vdx,vdy,vdz,
    vsfactor:TvalInput;

    vGridX,vGridY,vGridZ,
    vGridPCH,vGridYAW,vGridROL,
    vCamX,vCamY,vCamZ,
    vCamPCH,vCamYAW,vCamROL,
    v3DPX,v3DPY,v3DPZ,
    v3DPPCH,v3DPYAW:TvalInput;

  public

    { Public declarations }
  end;

var
  ToolForm: TToolForm;

implementation

uses Jed_Main,J_Level,Misc_utils,Lev_utils, U_Preview;

{$R *.DFM}


procedure TToolForm.BNCloseClick(Sender: TObject);
begin
 Hide;
end;

procedure TToolForm.FormCreate(Sender: TObject);
begin
 vangle:=TValInput.Create(EBAngle);
 vAngle.SetAsFloat(0);
 vsfactor:=TValInput.Create(EBSFactor);
 vsfactor.SetAsFloat(1);
 vdx:=TValInput.Create(EBDX); vdx.SetAsFloat(0);
 vdy:=TValInput.Create(EBDY); vdy.SetAsFloat(0);
 vdz:=TValInput.Create(EBDZ); vdz.SetAsFloat(0);

 vGridX:=TValInput.Create(EBGridX); vGridX.SetAsFloat(0);
 vGridY:=TValInput.Create(EBGridY); vGridY.SetAsFloat(0);
 vGridZ:=TValInput.Create(EBGridZ); vGridZ.SetAsFloat(0);

 vGridPCH:=TValInput.Create(EBGridPCH); vGridPCH.SetAsFloat(0);
 vGridYAW:=TValInput.Create(EBGridYAW); vGridYAW.SetAsFloat(0);
 vGridROL:=TValInput.Create(EBGridROL); vGridROL.SetAsFloat(0);

 vCamX:=TValInput.Create(EBCamX); vCamX.SetAsFloat(0);
 vCamY:=TValInput.Create(EBCamY); vCamY.SetAsFloat(0);
 vCamZ:=TValInput.Create(EBCamZ); vCamZ.SetAsFloat(0);

 vCamPCH:=TValInput.Create(EBCamPCH); vCamPCH.SetAsFloat(0);
 vCamYAW:=TValInput.Create(EBCamYAW); vCamPCH.SetAsFloat(0);
 vCamROL:=TValInput.Create(EBCamROL); vCamROL.SetAsFloat(0);

 v3DPX:=TValInput.Create(EB3DPX); v3DPX.SetAsFloat(0);
 v3DPY:=TValInput.Create(EB3DPY); v3DPY.SetAsFloat(0);
 v3DPZ:=TValInput.Create(EB3DPZ); v3DPZ.SetAsFloat(0);

 v3DPPCH:=TValInput.Create(EB3DPPCH); v3DPPCH.SetAsFloat(0);
 v3DPYAW:=TValInput.Create(EB3DPYAW); v3DPPCH.SetAsFloat(0);


end;

procedure TToolForm.BNRotateClick(Sender: TObject);
var angle:double;
begin
 feValDouble(vangle.s,angle);
 JedMain.RotateObject(angle,RGAxis.ItemIndex);
end;

procedure TToolForm.BNFlipClick(Sender: TObject);
begin
 JedMain.FlipObject(RGAxis.ItemIndex);
end;

procedure TToolForm.BNScaleClick(Sender: TObject);
var sfactor:double;
    how:integer;
begin
 feValDouble(vsfactor.s,sfactor);
 how:=0;
 if CBScaleTX.Checked then how:=sc_ScaleTX;
 if CBScaleXYZ.Checked then
 else
 case RGAxis.ItemIndex of
  0: how:=how or sc_ScaleX;
  1: how:=how or sc_ScaleY;
  2: how:=how or sc_ScaleZ;
  3: how:=how or sc_ScaleGrid;
 end;

 JedMain.ScaleObject(sfactor,how);
end;

procedure TToolForm.BNTranslateClick(Sender: TObject);
var dx,dy,dz:double;
begin
 feValDouble(vdx.s,dx);
 feValDouble(vdy.s,dy);
 feValDouble(vdz.s,dz);
 JedMain.TranslateObject(dx,dy,dz);
end;

procedure TToolForm.BNDoorClick(Sender: TObject);
begin
 JedMain.MakeDoor;
end;

procedure TToolForm.BNStraightenClick(Sender: TObject);
var i,j:integer;
begin
 for i:=0 to Level.Sectors.count-1 do
 With Level.Sectors[i] do
 for j:=0 to surfaces.count-1 do
 With Surfaces[j] do RecalcAll;
 Preview3D.ReloadLevel;
 JedMain.LevelChanged;
end;


procedure TToolForm.BNCalcAngleClick(Sender: TObject);
var surf1,surf2:TJKSurface;
    d:double;
    n:integer;
    sc,sf:integer;
    lvec,vec:TVector;
    px,py,pz:double;
begin
 With JedMain do
 begin
  if map_mode<>MM_SF then
  begin
   ShowMessage('You must be in surface mode');
   exit;
  end;

  n:=sfsel.AddSF(Cur_SC,Cur_SF);
  if sfsel.count<>2 then
  begin
   sfSel.DeleteN(n);
   ShowMessage('You must have 2 surfaces selected');
   exit;
  end;

  if sfsel.FindSF(Cur_SC,Cur_SF)=0 then
  begin
   sfsel.GetSCSF(0,sc,sf);
   surf1:=Level.Sectors[sc].surfaces[sf];
   sfsel.GetSCSF(1,sc,sf);
   surf2:=Level.Sectors[sc].surfaces[sf];
  end
  else
  begin
   sfsel.GetSCSF(0,sc,sf);
   surf2:=Level.Sectors[sc].surfaces[sf];
   sfsel.GetSCSF(1,sc,sf);
   surf1:=Level.Sectors[sc].surfaces[sf];
  end;

  sfsel.Clear;

  d:=SMult(surf1.normal.dx,surf1.normal.dy,surf1.normal.dz,
           surf2.normal.dx,surf2.normal.dy,surf2.normal.dz);
  d:=180-ArcCos(d)/pi*180;
  vAngle.SetAsFloat(d);

  VMult(surf1.normal.dx,surf1.normal.dy,surf1.normal.dz,
        surf2.normal.dx,surf2.normal.dy,surf2.normal.dz,
        vec.dx,vec.dy,vec.dz);
  if not Normalize(vec) then
  begin
   ShowMessage('The surfaces are already coplanar');
   exit;
  end;

  VMult(surf2.normal.dx,surf2.normal.dy,surf2.normal.dz,
        vec.dx,vec.dy,vec.dz,
        lvec.dx,lvec.dy,lvec.dz);


  With Surf1.vertices[0] do
  begin
   px:=x;
   py:=y;
   pz:=z;
  end;

  With surf2.vertices[0] do
  PlaneLineXnNew(surf1.normal,px,py,pz,x,y,z,x+lvec.dx,y+lvec.dy,z+lvec.dz,px,py,pz);

  With JedMain.Renderer do
  begin
   GridX:=px;
   GridY:=py;
   GridZ:=pz;
   SetGridNormal(vec.dx,vec.dy,vec.dz);
   SetGridXnormal(lvec.dx,lvec.dy,lvec.dz);
  end;

  RGAxis.ItemIndex:=rt_Grid;
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
var x,y,z,pch,yaw,rol:double;
begin
 vGridX.SetAsFloat(JedMain.Renderer.GridX);
 vGridY.SetAsFloat(JedMain.Renderer.GridY);
 vGridZ.SetAsFloat(JedMain.Renderer.GridZ);
 With JedMain.Renderer do
 sysGetPYR(gxnormal,gynormal,gnormal,pch,yaw,rol);

 vGridPCH.SetAsFloat(pch);
 vGridYAW.SetAsFloat(yaw);
 vGridROL.SetAsFloat(rol);

 JedMain.GetCam(x,y,z,pch,yaw,rol);
 vCamX.SetAsFloat(X);
 vCamY.SetAsFloat(Y);
 vCamZ.SetAsFloat(Z);

 vCamPCH.SetAsFloat(PCH);
 vCamYAW.SetAsFloat(YAW);
 vCamROL.SetAsFloat(ROL);

 Preview3D.GetCam(x,y,z,pch,yaw);

 v3DPX.SetAsFloat(x);
 v3DPY.SetAsFloat(y);
 v3DPZ.SetAsFloat(z);
 v3DPPCH.SetAsFloat(pch);
 v3DPYAW.SetAsFloat(yaw);

end;

procedure TToolForm.BNSetGridClick(Sender: TObject);
var pch,yaw,rol:double;
    x,y,z:Tvector;
begin
With JedMain.Renderer do
begin
 feValDouble(vGridX.s,GridX);
 feValDouble(vGridY.s,GridY);
 feValDouble(vGridZ.s,GridZ);

 feValDouble(vGridPCH.s,pch);
 feValDouble(vGridYAW.s,yaw);
 feValDouble(vGridROL.s,rol);

 SetVec(x,1,0,0);
 SetVec(y,0,1,0);
 SetVec(z,0,0,1);
 {PCH,ROL,YAW}
 RotateVector(x,PCH,0,0); RotateVector(x,0,ROL,0); RotateVector(x,0,0,YAW);
 RotateVector(y,PCH,0,0); RotateVector(y,0,ROL,0); RotateVector(y,0,0,YAW);
 RotateVector(z,PCH,0,0); RotateVector(z,0,ROL,0); RotateVector(z,0,0,YAW);
 {So PCH - x , YAW - y, ROL - z}
 JedMain.Renderer.SetGridNormal(z.dx,z.dy,z.dz);
 JedMain.Renderer.SetGridXNormal(x.dx,x.dy,x.dz);

end;
JedMain.Invalidate;
end;

procedure TToolForm.Button1Click(Sender: TObject);
var x,y,z,pch,yaw,rol:double;
begin
 feValDouble(vCamX.s,X);
 feValDouble(vCamY.s,Y);
 feValDouble(vCamZ.s,Z);
 feValDouble(vCamPCH.s,PCH);
 feValDouble(vCamYAW.s,YAW);
 feValDouble(vCamROL.s,ROL);
 JedMain.SetCam(x,y,z,pch,yaw,rol);
end;

procedure TToolForm.Button2Click(Sender: TObject);
var x,y,z,pch,yaw:double;
begin
 feValDouble(v3DPX.s,X);
 feValDouble(v3DPY.s,Y);
 feValDouble(v3DPZ.s,Z);
 feValDouble(v3DPPCH.s,PCH);
 feValDouble(v3DPYAW.s,YAW);
 Preview3D.SetCam(x,y,z,pch,yaw);
end;

end.
