unit Cons_checker;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  StdCtrls, ExtCtrls, J_Level, Lev_Utils, misc_utils, GlobalVars, Buttons,
  Geometry;

Const
 cc_Fix=0;
 cc_NeverFix=1;
 cc_Ask=2;
Const
     et_none=0;
     et_sector=1;
     et_surface=2;
     et_thing=3;
     et_cog=4;

type
 TConsistencyError=class
  etype:Integer;
  itm:TObject;
 end;

  TConsistency = class(TForm)
    Errors: TListBox;
    Panel: TPanel;
    BNClose: TButton;
    BNGoto: TButton;
    BNCheck: TButton;
    SBHelp: TSpeedButton;
    BNCheckRes: TButton;
    procedure ErrorsDblClick(Sender: TObject);
    procedure BNCloseClick(Sender: TObject);
    procedure BNCheckClick(Sender: TObject);
    procedure BNGotoClick(Sender: TObject);
    procedure ErrorsClick(Sender: TObject);
    procedure BNOptionsClick(Sender: TObject);
    procedure SBHelpClick(Sender: TObject);
    procedure BNCheckResClick(Sender: TObject);
  private
    { Private declarations }
    Procedure ClearErrors;
    Procedure AddError(Const Text:String;itm:TObject);
    Function FixError(const Text:String;itm:TObject):Boolean;
  public
    { Public declarations }
   Procedure Check;
   Procedure CheckResources;
   Function NErrors:integer;
   Function ErrorText(n:Integer):String;
   Function ErrorObject(n:integer):TConsistencyError;
  end;


var
 Consistency:TConsistency;

implementation
Uses U_Options, Jed_Main, Files, FileOperations, u_templates, U_CogForm;


{$R *.DFM}

procedure TConsistency.ErrorsDblClick(Sender: TObject);
begin
 BNGoto.Click;
end;

procedure TConsistency.BNCloseClick(Sender: TObject);
begin
 Close;
end;

Procedure TConsistency.ClearErrors;
var i:Integer;
begin
 for i:=0 to Errors.Items.Count-1 do Errors.Items.Objects[i].Free;
 Errors.Items.Clear;
end;

Procedure TConsistency.AddError(Const Text:String;itm:TObject);
var ce:TConsistencyError;
    ni:integer;
begin
 ce:=TConsistencyError.Create;
 if itm=nil then ce.etype:=et_none
 else if itm is TJKSector then ce.etype:=et_sector
 else if itm is TJKSurface then ce.etype:=et_surface
 else if itm is TJKThing then ce.etype:=et_thing
 else if itm is TCog then ce.etype:=et_cog
 else exit;

 ce.itm:=itm;

 Case ce.etype of
  et_sector:
  begin
   ni:=(itm as TJKSector).num;
   Errors.Items.AddObject(Format('%s; sector %d',[Text,ni]),ce);
  end;
  et_surface:
  With (itm as TJKSurface) do
  begin
   Errors.Items.AddObject(Format('%s; surface %d,%d',[Text,sector.num,num]),ce);
  end;
  et_thing:
  begin
   ni:=(itm as TJKthing).num;
   Errors.Items.AddObject(Format('%s; thing %d',[Text,ni]),ce);
  end;
  et_cog:
  begin
   ni:=Level.Cogs.IndexOf(itm);
   Errors.Items.AddObject(Format('%s; Cog %d',[Text,ni]),ce);
  end;
  et_none:
  begin
   ni:=-1;
   ce.free;
   ce:=nil;
   Errors.Items.Add(Text);
  end;
 end;
end;

Function TConsistency.FixError(const Text:String;itm:TObject):Boolean;
begin
 Case cc_FixErrors of
  cc_Fix:Result:=true;
  cc_NeverFix: Result:=false;
  cc_Ask: Result:=MsgBox(Text+' Fix?','Fix confirmation',MB_YesNo)=IDYes;
 end;
 if Result then AddError(Text+'(Fixed)',itm)
 else AddError(Text,itm);
end;


Procedure TConsistency.Check;
type
    TBoxes=array[0..10000] of TBox;
    
var i,j,v,n,k:integer;
    sec,sec1:TJKSector;
    surf:TJKSurface;
    vx:TJKVertex;
    th:TJKThing;
    d:double;
    cog:TCOG;
    ext:string;
    b:boolean;

    boxes:^TBoxes;

begin {Check}
 {if Not Visible then }Show;
 ClearErrors;

For i:=0 to level.Sectors.count-1 do
begin
 sec:=Level.Sectors[i];
 if Trim(sec.colormap)='' then AddError('No colormap set for sector!',sec);

 if sec.surfaces.count<4 then begin AddError('Sector has less than 4 surfaces!',sec); continue; end;
 if sec.vertices.count<4 then begin AddError('Sector has less than 4 vertices!',sec); continue; end;

 if (sec.flags and SF_3DO)=0 then
 if not IsSectorConvex(sec) then AddError('Sector is not convex',sec);
 
 for j:=0 to sec.Surfaces.count-1 do
 begin
  {Surface consistency}
  surf:=sec.Surfaces[j];
  if (surf.adjoin=nil) and (surf.FaceFlags and FF_Transluent<>0) then
     AddError('Incorrect flags on solid surface',surf);

  if surf.vertices.Count>24 then AddError('More than 24 vertices in a surface',surf);
  if Surf.Vertices.Count<3 then AddError('Less than 3 vertices in a surface!',surf);
  if not IsClose(vlen(surf.normal),1) then AddError('Incorrect normal - surface must be invalid!',surf);
  if Surf.Adjoin<>nil then
  begin
   if Surf.Adjoin.Adjoin<>surf then AddError('Invalid reverse adjoin',surf);
   if not Do_Surf_Overlap(Surf,Surf.Adjoin) then
     AddError('Adjoined surfaces don''t overlap',surf);
  end;

  for v:=0 to surf.Vertices.count-1 do
  begin
   vx:=surf.Vertices[v];
   With surf do
   begin
    d:=CalcD;
    if Abs(normal.dx*vx.x+normal.dy*vx.y+normal.dz*vx.z-d)>0.001 then
     begin AddError('The face is not planar',surf); break; end;
   end;
  end;
  if not IsSurfConvex(surf) then AddError('The face is not convex',surf);

 end;
end;

if CheckOverlaps then
try
 boxes:=nil;
 GetMem(boxes,Level.Sectors.count*sizeof(TBox));
 For i:=0 to Level.Sectors.count-1 do
 begin
  FindBBox(Level.Sectors[i],boxes^[i]);
 end;


For i:=0 to Level.Sectors.count-1 do
begin
   sec:=Level.Sectors[i];

   for j:=i-1 downto 0 do
   begin
    sec1:=Level.Sectors[j];
    b:=false;
    for k:=0 to sec.surfaces.count-1 do
    if sec.surfaces[k].Adjoin<>nil then
     if sec.surfaces[k].Adjoin.sector=sec1 then
     begin
      b:=true;
      break;
     end;
    if b then continue;

   if not DoBoxesIntersect(boxes^[i],boxes^[j]) then continue;

   if DoSectorsOverlap(sec,sec1) then
   begin
    AddError(Format('Sectors %d and %d overlap',[i,j]),sec);
    AddError(Format('Sectors %d and %d overlap',[i,j]),level.sectors[j]);
   end;

   end;

end;

finally
 if boxes<>nil then FreeMem(boxes);
end;


for i:=0 to Level.Things.Count-1 do
begin
 th:=Level.Things[i];
 if th.sec=nil then begin AddError('Thing not in sector',th); continue; end
 else if not IsInSector(th.sec,th.x,th.y,th.z)
   then AddError('Thing is not in specified sector',th);
end;

for i:=0 to Level.Cogs.Count-1 do
begin
 cog:=Level.Cogs[i];
 For j:=0 to cog.vals.count-1 do
 With cog.Vals[j] do
 begin
 case cog_type of
   ct_unk:;
   ct_cog:;
   ct_ai,ct_key,ct_mat,ct_wav,ct_3do:
   begin
    ext:=LowerCase(ExtractFileExt(AsString));
    case cog_type of
     ct_ai: if ext='.ai' then continue;
     ct_key: if ext='.key' then continue;
     ct_mat: if ext='.mat' then continue;
     ct_wav: if ext='.wav' then continue;
     ct_3do: if ext='.3do' then continue;
   end;
   AddError('Incorrect file name for parameter '+Name,cog);
  end;
   ct_msg:;
   ct_sec,ct_srf,ct_thg:;
   ct_tpl:;
   ct_int:;
   ct_float:;
   ct_vect:;
 end;
 end;
end;

 if Errors.ItemIndex<=0 then Errors.ItemIndex:=0 else Errors.OnClick(nil);
end;

procedure TConsistency.BNCheckClick(Sender: TObject);
begin
 Check;
end;

Procedure TConsistency.CheckResources;
var i,j:integer;
    sec:TJKSector;
    surf:TJKSurface;
    th:TJKThing;
    cmps,Mats,Wavs,Cogs:TStringList;
    cg:TCog;

Function CreateList:TStringList;
begin
 Result:=TstringList.Create;
 Result.Sorted:=true;
end;

Function NoFile(const name:string;list:TStringList):boolean;
var idx:integer;
    f:TFile;
begin
 idx:=List.IndexOf(name);
 if idx<>-1 then
 begin Result:=List.Objects[idx]<>nil; exit; end;

 Result:=true;
 try
  f:=OpenGameFile(name);
  List.AddObject(name,nil);
  result:=false;
  f.Fclose;
 except
  on Exception do List.AddObject(name,TObject(1));
 end;
end;

begin
 {if Not Visible then }Show;
  ClearErrors;
  cmps:=CreateList;
  Mats:=CreateList;
  wavs:=CreateList;
  cogs:=CreateList;

  if Level.MasterCMP<>'' then
  if NoFile(Level.MasterCmp,cmps) then
   AddError('Level''s Master CMP is not found',nil);

 try

  for i:=0 to Level.sectors.count-1 do
  begin
   sec:=Level.Sectors[i];
   if NoFile(sec.ColorMap,cmps) then
    AddError('Colormap not found',sec);
   if Sec.sound<>'' then
    if NoFile(sec.Sound,wavs) then
     AddError('Sound not found',sec);
  for j:=0 to sec.surfaces.count-1 do
  begin
   surf:=sec.Surfaces[j];
   if surf.Material<>'' then
    if NoFile(surf.Material,mats) then
     AddError('Material not found',surf);
  end;
  end;

  For i:=0 to Level.Things.count-1 do
  begin
   th:=Level.Things[i];
   if Templates.IndexOfName(th.name)=-1
    then AddError('Template not found',th);
  end;

  For i:=0 to Level.Cogs.count-1 do
  begin
   cg:=Level.Cogs[i];
   if NoFile(cg.Name,Cogs)
    then AddError('Cog not found',cg);
  end;

  if IsMots then
  begin
   if cmps.count>1 then AddError('Warning! More than 1 CMP used in the level',nil);
  end else
  begin
   if cmps.count>3 then AddError('Warning! More than 3 CMP used in the level',nil);
  end;
 finally
  cmps.Free;
  Mats.Free;
  wavs.Free;
  cogs.Free;
  if Errors.ItemIndex<=0 then Errors.ItemIndex:=0 else Errors.OnClick(nil);
 {if Not Visible then }Show;
 end;
end;

procedure TConsistency.BNCheckResClick(Sender: TObject);
begin
 CheckResources;
end;

procedure TConsistency.BNGotoClick(Sender: TObject);
var Index:Integer;
    ce:TConsistencyError;
    i,sc,sf,th:Integer;
begin
 Index:=Errors.ItemIndex;
 if Index<0 then exit;
 ce:=TConsistencyError(Errors.Items.Objects[Index]);
 if ce=nil then exit;
 Case ce.Etype of
  et_sector: begin
              sc:=Level.Sectors.IndexOf(ce.itm);
              if sc=-1 then
              begin MsgBox('Sector not found','Error',mb_OK); exit; end;
              JedMain.GotoSC(SC);
             end;
  et_surface: begin
              for i:=0 to Level.Sectors.Count-1 do
              With Level.Sectors[i] do
              begin
               sc:=i;
               sf:=Surfaces.IndexOf(ce.itm);
               if sf<>-1 then break;
              end;
              if sf=-1 then
              begin MsgBox('Surface not found','Error',mb_OK); exit; end;
              JedMain.GotoSF(Sc,sf);
             end;
  et_thing: begin
              th:=Level.Things.IndexOf(ce.itm);
              if th=-1 then
              begin MsgBox('Thing not found','Error',mb_OK); exit; end;
              JedMain.GotoTH(TH);
             end;
  et_cog:   begin
              th:=Level.Cogs.IndexOf(ce.itm);
              if th=-1 then
              begin MsgBox('Cog not found','Error',mb_OK); exit; end;
              CogForm.GotoCog(TH);
             end;
  end;
end;

procedure TConsistency.ErrorsClick(Sender: TObject);
var Index:Integer;
    ce:TConsistencyError;
begin
 Index:=Errors.ItemIndex;
 if Index=-1 then begin BNGoto.Enabled:=false; exit; end;
 ce:=TConsistencyError(Errors.Items.Objects[Index]);
 if ce=nil then BNGoto.Enabled:=false
 else BNGoto.Enabled:=true;
end;

procedure TConsistency.BNOptionsClick(Sender: TObject);
begin
{ Options.SetOptions(Options.Misc);}
end;

procedure TConsistency.SBHelpClick(Sender: TObject);
begin
 Application.HelpFile:=basedir+'jedhelp.hlp';
 Application.HelpContext(440);
end;

Function TConsistency.NErrors:integer;
begin
 result:=Errors.Items.Count;
end;

Function TConsistency.ErrorText(n:Integer):String;
begin
 result:=Errors.Items[n];
end;

Function TConsistency.ErrorObject(n:integer):TConsistencyError;
begin
 result:=TConsistencyError(Errors.Items.Objects[n]);
end;


end.
