unit U_Preview;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls,
   Forms, Dialogs, D3D_Prender, J_Level, Geometry, StdCtrls,
   GlobalVars, Menus, OGL_Prender, Prender, Clipbrd,
   d3d_NPrender, u_pj3dos;

type
  TCamPos=record
   x,y,z,pch,yaw:double;
  end;

  TPreview3D = class(TForm)
    MainMenu1: TMainMenu;
    Preview: TMenuItem;
    Close1: TMenuItem;
    Settings1: TMenuItem;
    Commands1: TMenuItem;
    SetViewcamera1: TMenuItem;
    Keyboard1: TMenuItem;
    miControl: TMenuItem;
    miEdit: TMenuItem;
    miTex: TMenuItem;
    ToggleFullyLit1: TMenuItem;
    procedure FormShow(Sender: TObject);
    procedure FormHide(Sender: TObject);
    procedure FormKeyDown(Sender: TObject; var Key: Word;
      Shift: TShiftState);
    procedure FormPaint(Sender: TObject);
    procedure Close1Click(Sender: TObject);
    procedure Settings1Click(Sender: TObject);
    procedure FormMouseUp(Sender: TObject; Button: TMouseButton;
      Shift: TShiftState; X, Y: Integer);
    procedure FormClose(Sender: TObject; var Action: TCloseAction);
    procedure FormDblClick(Sender: TObject);
    procedure FormCreate(Sender: TObject);
    procedure SetViewcamera1Click(Sender: TObject);
    procedure ToggleFullyLit1Click(Sender: TObject);
  private
    { Private declarations }
   Render3D:TPreviewRender;
   cwPlayer:integer;

   scs:TStringList; {Lists of changed,added,deleted sectors and things}
   ths:TStringList;

   Procedure WMEraseBkg(var msg:TMessage); message WM_ERASEBKGND;
   procedure wmactivate(var message : tmessage); message wm_activate;
   procedure wmpaint(var message : tmessage); message wm_paint;
   Procedure SysAddSector(s:TJKSector);
   Procedure SysAddThing(th:TJKThing);
   Procedure SaveCamPos(var cpos:TCamPos);
   Procedure RestoreCamPos(const cpos:TCamPos);
   Procedure AddSecChange(sec:TJKSector;Change:Char);
   Procedure AddThingChange(th:TJKThing;Change:Char);
   Procedure ApplyChanges;
   Procedure AddKBItem(mi:TMenuItem;const name:string;c:char;sc:TShiftState);
   procedure KBCommandClick(Sender: TObject);
  public
    { Public declarations }
   Procedure UpdateSector(s:TJKSector);
   Procedure AddSector(s:TJKSector);
   Procedure DeleteSector(s:TJKSector);

   Procedure UpdateThing(th:TJKthing);
   Procedure SetThing3DO(th:TJKthing;a3do:TPJ3DO);
   Procedure AddThing(th:TJKthing);
   Procedure DeleteThing(th:TJKthing);

   Procedure ReloadLevel;
   Procedure ShowPreview;
   Function IsActive:boolean;
   Procedure SetCam(x,y,z,pch,yaw:double);
   Procedure GotoWPlayer(wPlayer:integer;step:integer);
   Procedure GetCam(var x,y,z,pch,yaw:double);
  end;

var
  Preview3D: TPreview3D;

implementation

uses Jed_Main, U_Options, Misc_utils, Item_edit, U_tbar, ProgressDialog;

{$R *.DFM}

Procedure TPreview3D.ReloadLevel;
var i:integer;
    sec:TJKSector;
    th:TJKThing;
begin
 if Render3D=nil then exit;
 scs.clear;
 ths.clear;
 Render3D.ClearThings;
 Render3D.ClearSectors;
 Render3D.SetGamma(P3DGamma);

 Progress.Reset(Level.Sectors.Count+Level.Things.Count);

Progress.Msg:='Loading sectors...';

 for i:=0 to Level.Sectors.Count-1 do
 begin
  Progress.Step;
  sec:=Level.Sectors[i];
  if not P3DVisLayers then SysAddSector(sec)
  else if ToolBar.IsLayerVisible(sec.Layer) then SysAddSector(sec);
 end;

 Progress.Msg:='Loading things...';

 for i:=0 to Level.Things.Count-1 do
 begin
  Progress.Step;
  th:=Level.Things[i];
  if not P3DVisLayers then SysAddThing(th)
  else if ToolBar.IsLayerVisible(th.Layer) then SysAddThing(th);
 end;

 Progress.Hide;

 Invalidate;
end;

Procedure TPreview3D.GotoWPlayer(wPlayer:integer;step:integer);
var i:integer;
    th:TJKThing;
begin
 if wplayer>=Level.Things.count then wplayer:=0;
 if wplayer<0 then wplayer:=Level.Things.count-1;
 
 for i:=0 to Level.Things.count-1 do
 begin
  th:=Level.Things[wplayer];

  if compareText(th.name,'walkplayer')=0 then
  With th do
  begin
   Render3D.CamX:=x;
   Render3D.CamY:=y;
   Render3D.CamZ:=z;
   Render3D.Pch:=-pch;
   Render3D.Yaw:=-yaw;
   cwplayer:=wplayer;
   exit;
  end;

  inc(wplayer,step);
  if wplayer>=Level.Things.count then wplayer:=0;
  if wplayer<0 then wplayer:=Level.Things.count-1;


 end;

end;

Procedure TPreview3D.ShowPreview;
begin
 if visible then begin Invalidate; show; end else
 begin
  SetP3DPos(self,P3DX,P3DY,P3DWinSize);
  SetStayOnTop(self,P3DOnTop);
  Show;
  if Render3D=nil then begin hide; exit; end;
  ReloadLevel;
  GotoWPlayer(0,1);
 end;
end;

procedure TPreview3D.FormShow(Sender: TObject);
begin
  case P3DAPI of
   P3D_OGL: Render3D:=TOGLPRenderer.Create(Self);
   P3D_d3d5: Render3D:=TD3D5PRenderer.Create(Self);
   {P3D_3dfx: Render3D:=T3DFXPRenderer.Create(Self);}
  else Render3D:=TD3DRenderer.Create(Self);
  end;

Try
  Render3D.Initialize;
except
 on E:Exception do
  begin
   Render3D.Free;
   Render3D:=nil;
   PanMessage(mt_error,e.message);
  end;
end;
end;

procedure TPreview3D.FormHide(Sender: TObject);
begin
 if Render3D<>nil then Render3D.Free;
 Render3D:=nil;
 P3DX:=left; P3DY:=top;
end;

Procedure TPreview3D.WMEraseBkg(var msg:TMessage);
begin
  msg.Result:=0;
end;

procedure TPreview3D.FormKeyDown(Sender: TObject; var Key: Word;
  Shift: TShiftState);
begin
 if Render3D=nil then exit;
 
 With Render3D do
 case key of
  VK_ADD: If P3DGamma<254.9 then
          begin
           P3DGamma:=P3DGamma+0.1;
           if Render3D.SetGamma(P3DGamma)=1 then ReloadLevel;
           Invalidate;
          end;
  VK_SUBTRACT: If P3DGamma>0.1 then
          begin
           P3DGamma:=P3DGamma-0.1;
           if Render3D.SetGamma(P3DGamma)=1 then ReloadLevel;
           Invalidate;
          end;
  VK_LEFT:  if shift=[ssShift] then JedMain.ShiftTexture(st_left)
            else if shift=[ssCtrl] then JedMain.RotateTexture(st_left)
            else if ssAlt in Shift then
            begin
             CamY:=CamY+sin(YAW/180*PI)*P3DStep;
             CamX:=CamX-cos(YAW/180*PI)*P3DStep;
            end else
            begin
             YAW:=YAW-10; if YAW<0 then YAW:=YAW+360;
            end;
  VK_RIGHT: if shift=[ssShift] then JedMain.ShiftTexture(st_right)
            else if shift=[ssCtrl] then JedMain.RotateTexture(st_right)
            else if ssAlt in Shift then
            begin
             CamY:=CamY-sin(YAW/180*PI)*P3DStep;
             CamX:=CamX+cos(YAW/180*PI)*P3DStep;
            end else
            begin YAW:=YAW+10; if YAW>360 then YAW:=YAW-360; end;
  VK_UP: if shift=[ssShift] then JedMain.ShiftTexture(st_up) else
         if shift=[ssCtrl] then JedMain.RaiseObject(ro_up)
         else
         begin
          CamX:=CamX+sin(YAW/180*PI)*P3DStep;
          CamY:=CamY+cos(YAW/180*PI)*P3DStep;
         end;
  VK_DOWN: if shift=[ssShift] then JedMain.ShiftTexture(st_down) else
           if shift=[ssCtrl] then JedMain.RaiseObject(ro_down)
           else
           begin
            CamX:=CamX-sin(YAW/180*PI)*P3DStep;
            CamY:=CamY-cos(YAW/180*PI)*P3DStep;
           end;
  VK_NEXT: if PCH>=-80 then PCH:=PCH-10;
  VK_PRIOR: if PCH<=80 then PCH:=PCH+10;
  VK_HOME: if Shift=[] then PCH:=0
           else JedMain.StraightenTexture(ssCtrl in shift,ssShift in shift);

  Ord('A'): if shift=[ssCtrl] then JedMain.FormKeyDown(nil,Key,[]) else
            if (shift=[ssAlt]) or (shift=[ssShift]) then JedMain.FormKeyDown(nil,Key,shift)
            else CamZ:=CamZ+P3DStep;
  Ord('I'): if shift=[] then JedMain.AddThingAtSurf else
            if shift=[ssShift] then JedMain.AddThingAtXYZPYR(CamX,CamY,CamZ,-PCH,360-YAW,0.0) else
            JedMain.FormKeyDown(nil,Key,shift);

  Ord('Z'): CamZ:=CamZ-P3DStep;
  Ord('N'): GotoWPlayer(cwPlayer+1,1);
  Ord('P'): if Shift=[ssCtrl] then JedMain.FormKeyDown(nil,Key,shift) else GotoWPlayer(cwPlayer-1,-1);
  {<}  188: if shift=[ssShift] then JedMain.ShiftTexture(st_up)
            else if shift=[ssCtrl] then JedMain.RotateTexture(st_left)
            else if shift=[ssAlt] then JedMain.ScaleTexture(st_down)
            else JedMain.ShiftTexture(st_left);
  {>}  190: if shift=[ssShift] then JedMain.ShiftTexture(st_down)
            else if shift=[ssCtrl] then JedMain.RotateTexture(st_right)
            else if shift=[ssAlt] then JedMain.ScaleTexture(st_up)
            else JedMain.ShiftTexture(st_right);
  {;}  186: JedMain.StartStitch;
  VK_INSERT: if Shift=[ssCtrl] then JedMain.StartStitch
             else JedMain.DoStitch;
  {'}  222: JedMain.DoStitch;
  {/}  191: JedMain.StraightenTexture(ssCtrl in shift,ssShift in shift);

  Ord('S'): if (shift=[ssShift]) or (shift=[]) then JedMain.FormKeyDown(nil,Key,shift);
  VK_DELETE: if (shift=[ssAlt]) or (shift=[]) then JedMain.FormKeyDown(nil,Key,shift);
  Ord('X'),219,221,VK_Return: if Shift=[] then JedMain.FormKeyDown(nil,Key,shift);
  Ord('F'): if Shift=[] Then
            begin
             clipboard.AsText:=
             Sprintf('(%.5f/%.5f/%.5f:%.5f/%.5f/%.5f)',
             [CamX,CamY,CamZ,-PCH,360-YAW,0.0]);
            end;

  else exit;
 end;
  Invalidate;

end;

procedure TPreview3D.wmactivate(var message : tmessage);
begin
 if Render3D<>nil then Render3D.HandleActivate(message);
 inherited;
end;

procedure TPreview3D.wmpaint(var message : tmessage);
var r : trect;
    ps : tpaintstruct;
begin
 if render3D<>nil then
 begin
  if getupdaterect(Handle,r,false) then begin
   beginpaint(Handle,ps);
   Render3D.HandlePaint(ps.hdc);
   endpaint(Handle,ps);
  end;
 end;
 inherited;
end;

Procedure TPreview3D.DeleteSector(s:TJKSector);
begin
 AddSecChange(s,'D');
 Invalidate;
end;

Procedure TPreview3D.SysAddThing(th:TJKThing);
begin
if not P3DThings then exit;
try
 LoadThing3DO(th,false);
 Render3D.AddThing(th);
except
 on E:Exception do PanMessage(mt_warning,e.message);
end;
end;


Procedure TPreview3D.AddSecChange(sec:TJKSector;Change:Char);
var i:integer;
begin
 case change of
  'A': scs.AddObject(Change,sec);
  'C': begin
        i:=scs.IndexOfObject(sec);
        if (i<>-1) and (scs[i]='C') then exit;
        scs.AddObject(change,sec);
       end;
  'D': begin
        i:=scs.IndexOfObject(sec);
        if i<>-1 then scs.delete(i);
        scs.AddObject(change,sec);
       end;
 end;
end;

Procedure TPreview3D.AddThingChange(th:TJKThing;Change:Char);
var i:integer;
begin
{ i:=ths.IndexOfObject(th);
 if i=-1 then ths.AddObject(Change,th) else
 ths[i]:=Change;}

 case change of
  'A': ths.AddObject(Change,th);
  'C': begin
        i:=ths.IndexOfObject(th);
        if (i<>-1) and (ths[i]='C') then exit;
        ths.AddObject(change,th);
       end;
  'D': begin
        i:=ths.IndexOfObject(th);
        if i<>-1 then ths.delete(i);
        ths.AddObject(change,th);
       end;
  end;
end;

Procedure TPreview3D.ApplyChanges;
var i:integer;
    sec:TJKSector;
    th:TJKThing;
    c:char;
begin
 if Render3D=nil then
 begin
  scs.clear;
  ths.clear;
  exit;
 end;

 for i:=0 to scs.count-1 do
 begin
  sec:=TJKSector(scs.Objects[i]);
  c:=scs[i][1];
  case c of
   'A': SysAddSector(sec);
   'D': Render3D.DeleteSector(sec);
   'C': Render3D.UpdateSector(sec);
  end;
 end;

 scs.clear;

 for i:=0 to ths.count-1 do
 begin
  th:=TJKThing(ths.Objects[i]);
  c:=ths[i][1];
  case c of
   'A': SysAddThing(th);
   'D': Render3D.DeleteThing(th);
   'C': Render3D.UpdateThing(th);
  end;
 end;
 ths.clear;
end;

Procedure TPreview3D.SetThing3DO(th:TJKthing;a3do:TPJ3DO);
begin
 if Render3D<>nil then Render3D.SetThing3DO(th,a3do);
end;

Procedure TPreview3D.SysAddSector(s:TJKSector);
begin
try
 Render3D.AddSector(s);
except
 on E:Exception do PanMessage(mt_warning,e.message);
end;
end;

Procedure TPreview3D.UpdateSector(s:TJKSector);
begin
 AddSecChange(s,'C');
 Invalidate;
end;

Procedure TPreview3D.AddSector(s:TJKSector);
begin
 AddSecChange(s,'A');
 Invalidate;
end;

Procedure TPreview3D.UpdateThing(th:TJKthing);
begin
 AddThingChange(th,'C');
 Invalidate;
end;

Procedure TPreview3D.AddThing(th:TJKthing);
begin
 AddThingChange(th,'A');
 Invalidate;
end;

Procedure TPreview3D.DeleteThing(th:TJKthing);
begin
 AddThingChange(th,'D');
 Invalidate;
end;

procedure TPreview3D.FormPaint(Sender: TObject);
begin
 ApplyChanges;
 Render3d.redraw;
end;

Function TPreview3D.IsActive:boolean;
begin
 Result:=Render3D<>nil;
end;

Procedure TPreview3D.SetCam(x,y,z,pch,yaw:double);
begin
 if Render3D=nil then exit;
 Render3D.CamX:=x;
 Render3D.CamY:=Y;
 Render3D.CamZ:=Z;
 Render3D.PCH:=-pch;
 Render3D.YAW:=-yaw;
 Invalidate;
end;


procedure TPreview3D.Close1Click(Sender: TObject);
begin
 Close;
end;

Procedure TPreview3D.SaveCamPos(var cpos:TCamPos);
begin
 cpos.x:=render3D.CamX;
 cpos.y:=render3D.CamY;
 cpos.Z:=render3D.CamZ;
 cpos.pch:=render3D.PCH;
 cpos.yaw:=render3D.YAW;
end;

Procedure TPreview3D.RestoreCamPos(const cpos:TCamPos);
begin
 render3D.CamX:=cpos.x;
 render3D.CamY:=cpos.Y;
 render3D.CamZ:=cpos.Z;
 render3D.PCH:=cpos.pch;
 render3D.YAW:=cpos.yaw;
end;

procedure TPreview3D.Settings1Click(Sender: TObject);
var cpos:TcamPos;
begin
 With Options do
 begin
  if not SetOptions(PPreview) then exit;
  SaveCamPos(cpos);
  Self.Hide;
  ShowPreview;
  RestoreCamPos(cpos);
 end;
end;

procedure TPreview3D.FormMouseUp(Sender: TObject; Button: TMouseButton;
  Shift: TShiftState; X, Y: Integer);
var
    i,f:integer;
begin
 if Render3D=nil then exit;
 With Render3D do
 Case PickAt(X,Y) of
  pk_nothing: exit;
  pk_surface:
   begin
    i:=Level.Sectors.IndexOf(selSC);
    if i<>-1 then
    begin
     JedMain.SetMapMode(MM_SF);
     JedMain.SetCurSF(i,SelSF);
     if Shift=[ssShift] then JedMain.DO_MultiSelect
     else JedMain.ClearMultiSelection;
    end
    else PanMessage(mt_warning,'3D preview is out of sync with the level! Reload it');
   end;
  pk_thing:
   begin
    i:=Level.Things.IndexOf(selTH);
    if i<>-1 then
    begin
     JedMain.SetMapMode(MM_TH);
     JedMain.SetCurTH(i);
     if Shift=[ssShift] then JedMain.DO_MultiSelect;
    end
    else PanMessage(mt_warning,'3D preview is out of sync with the level! Reload it');
   end;
 end;
end;

procedure TPreview3D.FormClose(Sender: TObject; var Action: TCloseAction);
begin
 P3dOnTop:=GetStayOnTop(self);
 GetP3DPos(self,P3DX,P3DY,P3DWinSize);
end;

procedure TPreview3D.FormDblClick(Sender: TObject);
begin
 case JedMain.Map_mode of
  MM_SF: ItemEdit.DoPickTexture;
  MM_TH: ItemEdit.DoPickThing;
 end;
end;

procedure TPreview3D.FormCreate(Sender: TObject);
begin
 scs:=TStringList.Create;
 ths:=TStringList.Create;

 AddKBItem(miControl,'Rotate right',char(VK_Right),[]);
 AddKBItem(miControl,'Rotate left',char(VK_Left),[]);
 AddKBItem(miControl,'Move forth',char(VK_Up),[]);
 AddKBItem(miControl,'Move back',char(VK_down),[]);
 AddKBItem(miControl,'Sidestep left',char(VK_Left),[ssAlt]);
 AddKBItem(miControl,'Sidestep right',char(VK_right),[ssAlt]);

 AddKBItem(miControl,'Look up',char(VK_Next),[]);
 AddKBItem(miControl,'Look down',char(VK_Prior),[]);
 AddKBItem(miControl,'Move up','A',[]);
 AddKBItem(miControl,'Move down','Z',[]);

 AddKBItem(miControl,'Select surface/thing'#9'Click',#0,[]);
 AddKBItem(miControl,'Change texture/template'#9'Double-Click',#0,[]);


 AddKBItem(miControl,'Snap grid to item','S',[ssShift]);
 AddKBItem(miControl,'Snap grid to item','W',[ssShift]);
 AddKBItem(miControl,'Swap Grid axis','G',[ssShift]);
 AddKBItem(miControl,'Sector mode','S',[]);
 AddKBItem(miControl,'Item Editor',Chr(VK_return),[]);

 AddKBItem(miControl,'Next player start','N',[]);
 AddKBItem(miControl,'Prev player start','P',[]);

 AddKBItem(MiTex,'Scroll up',#188,[ssShift]);
 AddKBItem(MiTex,'Scroll down',#190,[ssShift]);

 AddKBItem(MiTex,'Scroll left',#188,[]);
 AddKBItem(MiTex,'Scroll right',#190,[]);

 AddKBItem(MiTex,'Start stitch',Char(VK_INSERT),[ssCtrl]);
 AddKBItem(MiTex,'Start stitch',#186,[]);

 AddKBItem(MiTex,'Stitch',char(VK_INSERT),[ssShift]);
 AddKBItem(MiTex,'Stitch',#222,[]);

 AddKBItem(MiTex,'Straighten Texture',char(VK_HOME),[ssAlt]);
 AddKBItem(MiTex,'Straighten Texture',#191,[]);
 AddKBItem(MiTex,'Scale texture down',#188,[ssAlt]);
 AddKBItem(MiTex,'Scale texture Up',#190,[ssAlt]);
 AddKBItem(MiTex,'Straighten/zero/rotate90 texture',#191,[ssShift,ssCtrl]);
 AddKBItem(MiTex,'Straighten/zero/rotate90 texture',Char(VK_HOME),[ssShift,ssCtrl]);
 AddKBItem(MiTex,'Straighten/zero texture',#191,[ssCtrl]);
 AddKBItem(MiTex,'Straighten/zero texture',Char(VK_HOME),[ssCtrl]);
 AddKBItem(MiTex,'Rotate texture left',#188,[ssCtrl]);
 AddKBItem(MiTex,'Rotate texture right',#190,[ssCtrl]);

 AddKBItem(miEdit,'Delete surface',Char(VK_DELETE),[ssAlt]);
 AddKBItem(miEdit,'Invert surface','I',[ssAlt]);
 AddKBItem(miEdit,'Planarize surface','P',[ssCtrl]);

 AddKBItem(miEdit,'Assign Thing to Sector','A',[ssShift]);
 AddKBItem(miEdit,'Unadjoin','A',[ssAlt]);
 AddKBItem(miEdit,'Adjoin','A',[ssCtrl]);

 AddKBItem(miEdit,'Raise item',char(VK_UP),[ssCtrl]);
 AddKBItem(miEdit,'Lower item',char(VK_Down),[ssCtrl]);
 AddKBItem(miEdit,'Raise item',#219,[]);
 AddKBItem(miEdit,'Lower item',#221,[]);

 AddKBItem(miEdit,'Delete item',Char(VK_DELETE),[]);
 AddKBItem(miEdit,'Extrude surface','X',[]);
 AddKBItem(miEdit,'Copy camera as frame','F',[]);
 AddKBItem(miEdit,'Insert thing at surface','I',[]);
 AddKBItem(miEdit,'Insert thing at camera','I',[ssShift]);


end;

procedure TPreview3D.SetViewcamera1Click(Sender: TObject);
begin
 With Render3D do JedMain.SetCam(CamX,CamY,CamZ,0,0,0);
end;

Procedure TPreview3D.AddKBItem(mi:TMenuItem;const name:string;c:char;sc:TShiftState);
var nmi:TMenuItem;
    ext:string;
begin
 nmi:=TMenuItem.Create(mi);
 nmi.OnClick:=KBCommandClick;
 case c of
  #0: ext:='';
  Char(VK_ADD):ext:=#9'+';
  Char(VK_SUBTRACT):ext:=#9'-';
  Char(VK_MULTIPLY):ext:=#9'*';
 else ext:=#9+ShortCutToText(ShortCut(ord(c),sc));
 end;

 nmi.Caption:=name+ext;
 nmi.Tag:=ShortCut(ord(c),sc);
 mi.Add(nmi);
end;

procedure TPreview3D.KBCommandClick(Sender: TObject);
var key:word;
    sc:TShiftState;
begin
 with (sender as TMenuItem) do
 begin
  ShortCutToKey(Tag,key,sc);
  FormKeyDown(Self, Key, sc);
 end;
end;



procedure TPreview3D.ToggleFullyLit1Click(Sender: TObject);
var cpos:TcamPos;
begin
 P3DFullLit:=not P3DFullLit;
 SaveCamPos(cpos);
 Self.Hide;
 ShowPreview;
 RestoreCamPos(cpos);
end;

Procedure TPreview3D.GetCam(var x,y,z,pch,yaw:double);
begin
 x:=0; y:=0; z:=0; pch:=0; yaw:=0;
 if Render3D=nil then exit;
 x:=Render3D.CamX;
 y:=Render3D.CamY;
 z:=Render3D.CamZ;
 pch:=-Render3D.PCH;
 yaw:=-Render3D.YAW;
end;


end.
