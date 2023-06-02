unit U_CogForm;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  Grids, ExtCtrls, StdCtrls, ComCtrls, J_Level, Values, FieldEdit,
  Misc_utils, GlobalVars, ShellApi, files, fileoperations,
  lev_utils;

type
  TCogForm = class(TForm)
    Panel1: TPanel;
    LVCOGs: TListView;
    SGVals: TStringGrid;
    BNAdd: TButton;
    BNDelete: TButton;
    BNRefresh: TButton;
    BNEdit: TButton;
    BNGetSel: TButton;
    CBOnTop: TCheckBox;
    BNDuplicate: TButton;
    procedure BNRefreshClick(Sender: TObject);
    procedure FormCreate(Sender: TObject);
    procedure LVCOGsChange(Sender: TObject; Item: TListItem;
      Change: TItemChange);
    procedure BNAddClick(Sender: TObject);
    procedure BNDeleteClick(Sender: TObject);
    procedure SGValsExit(Sender: TObject);
    procedure BNEditClick(Sender: TObject);
    procedure BNGetSelClick(Sender: TObject);
    procedure CBOnTopClick(Sender: TObject);
    procedure BNDuplicateClick(Sender: TObject);
    procedure FormDestroy(Sender: TObject);
  private
    { Private declarations }
   Function FieldChange(fi:TFieldInfo):boolean;
   Procedure FieldDblClick(fi:TFieldInfo);
   Function AddCog(const fname:string):integer;
   Function AddCogWithVals(const fname:string;fromcog:TCOG):integer;
  public
   fe:TFieldEdit;
   Procedure RefreshList;
   Procedure GotoCOG(n:integer);
   Procedure UpdateCOG(n:Integer);
    { Public declarations }
  end;

var
  CogForm: TCogForm;

implementation

uses ResourcePicker, Cog_utils, ListRes, Jed_Main;

{$R *.DFM}

Procedure TCogForm.UpdateCOG(n:Integer);
var i,j:integer;
    li:TListItem;
    s:string;
    vl:TCogValue;
begin
 if n>=LVCogs.Items.COunt then exit;
 li:=LVCogs.Items[n];

 With Level.Cogs[n] do
 begin
  Li.Caption:=Name;
  s:='';
  for j:=0 to Vals.Count-1 do
  begin
   vl:=Vals[j];
   s:=Concat(s,' ',vl.Name,'=',vl.AsString);
  end;
  li.SubItems[0]:=s;
 end;
end;

Procedure TCogForm.RefreshList;
var i,j:integer;
    li:TListItem;
    s:string;
    vl:TCogValue;
    ce:TLVChangeEvent;
begin
 ce:=LVCOGs.OnChange;
 LVCOGs.OnChange:=nil;

 LVCogs.Items.BeginUpdate;
 LVCogs.Items.Clear;
try
 for i:=0 to Level.Cogs.Count-1 do
 With Level.Cogs[i] do
 begin
  li:=LVCogs.Items.Add;
  Li.Caption:=Format('%d: %s',[i,Name]);
  s:='';
  for j:=0 to Vals.Count-1 do
  begin
   vl:=Vals[j];
   s:=Concat(s,' ',vl.Name,'=',vl.AsString);
  end;
  li.SubItems.Add(s);
 end;
 Finally
  LVCogs.Items.EndUpdate;
  LVCOGs.OnChange:=ce;
 end;

 if LVCOGs.Items.count>0 then
 begin
  LVCOGs.Selected:=LVCOGs.Items[0];
  LVCOGs.ItemFocused:=LVCOGs.Items[0];
 end else
 begin
  LVCOGs.Selected:=nil;
  LVCOGs.ItemFocused:=nil;
  LVCOGsChange(nil,nil,ctText);
 end;
{LVCOGsChange(nil,nil,ctText);}

end;


procedure TCogForm.BNRefreshClick(Sender: TObject);
var i,j,k:integer;
    cf:TCogFile;
    v,vnew:TCogValue;
    s:string;
    vvals,vnames:TStringList;

begin
 cf:=tCOgFile.Create;
 vvals:=TStringList.Create;
 vnames:=TStringList.Create;

 try
 for i:=0 to Level.Cogs.count-1 do
  with Level.Cogs[i] do
  begin
   vvals.clear;
   vnames.clear;

   for j:=Vals.count-1 downto 0 do
   begin
    v:=Vals[j];
    vnames.Add(v.name);
    vvals.Add(v.AsString);
    Vals.Delete(j);
    v.free;
   end;

   cf.LoadNoLocals(Name);
   for j:=0 to cf.Count-1 do
   begin
    v:=TCogValue.Create;
    Vals.Add(v);
    vnew:=cf[j];
    v.Assign(vnew);

    k:=vnames.IndexOf(vnew.name);
    if k<>-1 then v.Val(vvals[k])
    else v.Val(vnew.AsString);

    v.Resolve;
   end;

 end;
 finally
  vvals.free;
  vnames.free;
  cf.Free;
  RefreshList;
 end;
end;

procedure TCogForm.FormCreate(Sender: TObject);
begin
 fe:=TFieldEdit.Create(SGVals);
end;

procedure TCogForm.LVCOGsChange(Sender: TObject; Item: TListItem;
  Change: TItemChange);
var i,n:integer;
begin
 if fe=nil then exit;
 fe.clear;
 if LVCogs.ItemFocused=nil then exit;
 n:=LVCogs.ItemFocused.Index;
 if n>=Level.Cogs.COunt then exit;
 fe.Clear;
 With Level.Cogs[n] do
 begin
  Caption:=Format('Placed Cogs - COG %d: %s',[n,Name]);
  for i:=0 to Vals.Count-1 do
  With Vals[i] do
   fe.AddFieldStr(Name+' ('+GetCogTypeName(cog_type)+')',i,AsJedString);
 end;
 fe.DoneAdding;
 fe.OnChange:=FieldChange;
 fe.OnDblClick:=FieldDblClick;
end;

Function TCogForm.FieldChange(fi:TFieldInfo):boolean;
var
   v:TCogValue;
   n:integer;
   cg:TCOg;
   d:double;
begin
 if LVCogs.ItemFocused=nil then exit;
 n:=LVCogs.ItemFocused.Index;
 cg:=Level.Cogs[n];
 v:=cg.vals[fi.id];
 Result:=V.JedVal(fi.s);
 if not result then PanMessage(mt_error,Format('Invalid value for %s: %s',
   [GetCogTypeName(v.cog_type),fi.s]));
end;

Procedure TCogForm.FieldDblClick(fi:TFieldInfo);
var
   v:TCogValue;
   i,n:integer;
   cg:TCOg;
   d:double;
   fname:string;
begin
 if LVCogs.ItemFocused=nil then exit;
 n:=LVCogs.ItemFocused.Index;
 cg:=Level.Cogs[n];
 v:=cg.vals[fi.id];
 case v.Cog_type of
   ct_unk:;
   ct_ai: fi.s:=ResPicker.PickAI(fi.s);
   ct_cog: begin
            i:=StrToInt(fi.s);
            if (i<0) then
            begin
             fname:=ResPicker.PickCog('');
             if fname<>'' then fi.s:=IntToStr(AddCog(fname));
            end
            else GotoCOG(i);
           end; 
   ct_key: fi.s:=ResPicker.PickKEY(fi.s);
   ct_mat: fi.s:=ResPicker.PickMat(fi.s);
   ct_msg: ;
   ct_3do: fi.s:=ResPicker.Pick3DO(fi.s);
   ct_sec: begin ValInt(fi.s,n); if n>=0 then JedMain.GotoSC(n); end;
   ct_wav: fi.s:=ResPicker.PickSecSound(fi.s);
   ct_srf: begin SScanf(fi.s,'%d %d',[@i,@n]); if i>=0 then JedMain.GotoSF(i,n); end;
   ct_tpl: fi.s:=ResPicker.PickTemplate(fi.s);
   ct_thg: begin ValInt(fi.s,n); if n>=0 then JedMain.GotoTH(n); end;
   ct_int,ct_float,ct_vect:;
 end;

end;

Function TCogForm.AddCog(const fname:string):integer;
begin
 result:=AddCogWithVals(fname,nil);
end;

Function TCogForm.AddCogWithVals(const fname:string;fromcog:TCOG):integer;
var cf:TCogFile;
    cg:TCog;
    i,n:integer;
    cv,v:TCogValue;
    li:TListItem;
    s:string;
    ce:TLVChangeEvent;
begin
 cf:=TCogFile.Create;
 cf.LoadNoLocals(fname);
 cg:=TCog.Create;
 cg.name:=fname;
 for i:=0 to cf.Count-1 do
 begin
  cv:=cf[i];
  v:=TCogValue.Create;
  v.Assign(cv);
  v.Val(cv.AsString);
  cg.Vals.Add(v);
 end;
 cf.free;

 if fromcog<>nil then
 for i:=0 to cg.vals.count-1 do
 begin
  v:=cg.vals[i];
  n:=fromcog.vals.IndexOfName(v.name);
  if n<>-1 then v.JedVal(fromcog.vals[n].AsJedString);
 end;

 result:=Level.COgs.Add(cg);

 ce:=LVCOGs.OnChange;
 LVCOGs.OnChange:=nil;


 li:=LVCogs.Items.Add;
 Li.Caption:=cg.Name;

 s:='';
 for i:=0 to cg.Vals.Count-1 do
 begin
  v:=cg.Vals[i];
  s:=Concat(s,' ',v.Name,'=',v.AsString);
 end;

 li.SubItems.Add(s);
 LVCOGs.OnChange:=ce;

end;

procedure TCogForm.BNAddClick(Sender: TObject);
var
    fname:string;
begin
 fname:=ResPicker.PickCog('');
 if fname='' then exit;
 GotoCOG(AddCog(fname));
end;

procedure TCogForm.BNDeleteClick(Sender: TObject);
var cg:TCog;
    n:integer;
begin
 if LVCogs.ItemFocused=nil then exit;
 n:=LVCogs.ItemFocused.Index;
 lev_utils.DeleteCOG(level,n);
 if n>=LVCOGs.Items.Count then n:=LVCOGs.Items.Count-1;
 GotoCOG(n);
end;

procedure TCogForm.SGValsExit(Sender: TObject);
begin
 fe.DeactivateHandler;
end;

Procedure TCogForm.GotoCOG(n:integer);
begin
 if (n<0) or (n>=Level.Cogs.Count) then exit;
 LVCogs.ItemFocused:=LVCogs.Items[n];
 LVCogs.Selected:=LVCogs.Items[n];
 LVCogs.Selected.MakeVisible(false);
 Show;
 ActiveControl:=LVCogs;

end;


procedure TCogForm.BNEditClick(Sender: TObject);
var cg:TCog;
    n:integer;
    fname:string;
    f,f1:TFile;
begin
 if projectdir='' then begin ShowMessage('Save project first'); exit; end;

 if LVCogs.ItemFocused=nil then exit;
 n:=LVCogs.ItemFocused.Index;
 cg:=Level.Cogs[n];

 fname:='';
 if FileExists(ProjectDir+cg.Name) then fname:=ProjectDir+cg.Name
 else if FileExists(ProjectDir+'cog\'+cg.Name) then fname:=ProjectDir+'cog\'+cg.Name;

 if fname='' then
 begin
  if MsgBox('The COG is inside a GOB file. Extract it to edit?','Question',MB_YESNO)<>idYes then exit;
  f:=OpenGameFile(cg.name);
  {$i-}
  MkDir(ProjectDir+'cog');
  if ioresult=0 then;
  {$i+}
  fname:=ProjectDir+'cog\'+cg.name;
  f1:=OpenFileWrite(fname,0);
  CopyFileData(f,f1,f.fsize);
  f.Fclose;
  f1.Fclose;
 end;

 n:=ShellExecute(Application.Handle,nil,Pchar(fname),nil,Pchar(ExtractFilePath(fname)),SW_SHOWNORMAL);
 case n of
  SE_ERR_NOASSOC: PanMessage(mt_Error,'No programm is associated with COGs!');
 else;
 end;

end;

procedure TCogForm.BNGetSelClick(Sender: TObject);
var i,n:integer;
    v:TCOgValue;
    obj:TObject;
begin
 if LVCogs.ItemFocused=nil then begin ShowMessage('No COG selected'); exit; end;
 n:=LVCogs.ItemFocused.Index;
 i:=SGVals.Row;
 if i<0 then begin ShowMessage('No COG value selected'); exit; end;
 v:=Level.COgs[n].Vals[i];

 Case v.Cog_type of
  ct_thg,ct_sec,ct_srf:
  begin
   obj:=JedMain.GetCurObjForCog(v.cog_type);
   if obj<>nil then v.obj:=obj;
   UpdateCog(n);
  end;
  else ShowMessage('It only works for sector/surface and thing entries');
  end;

end;

procedure TCogForm.CBOnTopClick(Sender: TObject);
begin
 CFOnTop:=CBOnTop.Checked;
 SetStayOnTop(Self,CFOnTop);
end;

procedure TCogForm.BNDuplicateClick(Sender: TObject);
var n:integer;
begin
 if LVCogs.ItemFocused=nil then begin ShowMessage('No COG selected'); exit; end;
 n:=LVCogs.ItemFocused.Index;
 GotoCOG(AddCOGWithVals(Level.COgs[n].Name,Level.COgs[n]));
end;

procedure TCogForm.FormDestroy(Sender: TObject);
begin
 fe.Free;
 fe:=nil;
end;

end.
