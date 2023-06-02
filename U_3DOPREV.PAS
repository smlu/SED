unit U_3doprev;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  J_level, Prender, D3D_Prender, GlobalVars;

Procedure Set3DOPrevActive(isit:boolean);
Procedure Set3DOCMP(const cmp:string);
Procedure View3DO(const name:string);
procedure Init3DOPreview;
procedure Done3DOPreview;

Procedure P3DO_SetPCHYAW(PCH,YAW:Double);

implementation
uses   Jed_Main, u_3dos, ResourcePicker;

var
    isactive:boolean;
    tmpthing:TJKThing;
    render3D:TPreviewRender;
    cPCH,CYaw:double;
    cur_cmp:string;

Procedure Set3DOCMP(const cmp:string);
begin
 cur_cmp:=cmp;
end;

Procedure InitRenderer;
begin
 if Render3D<>nil then exit;
 Render3D:=TD3DRenderer.CreateFromPanel(ResPicker.Panel3D);
 try
  Render3D.initialize;
 except
 On Exception do
 begin
  try
   Render3D.Free;
  except
   On Exception do;
  end;
  Render3D:=nil;
  raise;
 end;
 end;
end;

Procedure Set3DOPrevActive(isit:boolean);
begin
 if isit=isactive then exit;
 isactive:=isit;
end;

Procedure View3DO(const name:string);
var
    a3Do:T3DO;
    box:TThingBox;
begin
 if not isactive then exit;
 InitRenderer;
 if Render3D=nil then exit;
 a3DO:=tmpthing.a3DO;
 tmpthing.a3DO:=nil;
 a3DO.Free;
 tmpthing.a3DO:=T3Do.CreateFrom3DO(name,0);
 tmpthing.YAW:=180;
 tmpthing.a3Do.GetBBox(box);

 Render3D.thing_cmp:=cur_cmp;

 Render3D.SetGamma(P3DGamma);
 Render3D.ClearThings;
 Render3D.Addthing(tmpthing);
 Render3D.SetViewTothing(tmpthing);

 P3DO_SetPCHYAW(cPCH,cYAW);

 Render3D.redraw;
end;


procedure Init3DOPreview;
begin
 if Render3D<>nil then Render3d.Free;
 Render3D:=nil;
end;

procedure Done3DOPreview;
begin
 Free3DO(tmpthing.a3DO);
 Render3D.free;
 Render3D:=nil;
 Respicker.CB3DOPrev.Checked:=false;
 isActive:=false;
end;

Procedure P3DO_SetPCHYAW(PCH,YAW:Double);
begin
 cPCH:=PCH;
 cYAw:=YAW;
 if render3D=nil then exit;
 Render3D.SetPCHYaw(PCH,Yaw);
 Render3D.redraw;
end;

Initialization
tmpthing:=TJKThing.Create;

end.
