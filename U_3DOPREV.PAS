unit U_3doprev;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  J_level, Prender, D3D_Prender, GlobalVars;

Procedure Set3DOPrevActive(isit: boolean);
Procedure Set3DOCMP(const cmp: string);
Procedure View3DO(const name: string);
procedure Init3DOPreview;
procedure Done3DOPreview;

Procedure P3DO_SetPCHYAW(PCH, YAW: Double);

implementation

uses Jed_Main, u_3dos, ResourcePicker;

var
  isactive: boolean;
  tmpthing: TJKThing;
  render3D: TPreviewRender;
  cPCH, CYaw: Double;
  cur_cmp: string;

Procedure Set3DOCMP(const cmp: string);
begin
  cur_cmp := cmp;
end;

Procedure InitRenderer;
begin
  if render3D <> nil then
    exit;
  render3D := TD3DRenderer.CreateFromPanel(ResPicker.Panel3D);
  try
    render3D.initialize;
  except
    On Exception do
    begin
      try
        render3D.Free;
      except
        On Exception do;
      end;
      render3D := nil;
      raise;
    end;
  end;
end;

Procedure Set3DOPrevActive(isit: boolean);
begin
  if isit = isactive then
    exit;
  isactive := isit;
end;

Procedure View3DO(const name: string);
var
  a3Do: T3DO;
  box: TThingBox;
begin
  if not isactive then
    exit;
  InitRenderer;
  if render3D = nil then
    exit;
  a3Do := tmpthing.a3Do;
  tmpthing.a3Do := nil;
  a3Do.Free;
  tmpthing.a3Do := T3DO.CreateFrom3DO(name, 0);
  tmpthing.YAW := 180;
  tmpthing.a3Do.GetBBox(box);

  render3D.thing_cmp := cur_cmp;

  render3D.SetGamma(P3DGamma);
  render3D.ClearThings;
  render3D.Addthing(tmpthing);
  render3D.SetViewTothing(tmpthing);

  P3DO_SetPCHYAW(cPCH, CYaw);

  render3D.redraw;
end;

procedure Init3DOPreview;
begin
  if render3D <> nil then
    render3D.Free;
  render3D := nil;
end;

procedure Done3DOPreview;
begin
  Free3DO(tmpthing.a3Do);
  render3D.Free;
  render3D := nil;
  ResPicker.CB3DOPrev.Checked := false;
  isactive := false;
end;

Procedure P3DO_SetPCHYAW(PCH, YAW: Double);
begin
  cPCH := PCH;
  CYaw := YAW;
  if render3D = nil then
    exit;
  render3D.SetPCHYaw(PCH, YAW);
  render3D.redraw;
end;

Initialization

tmpthing := TJKThing.Create;

end.
