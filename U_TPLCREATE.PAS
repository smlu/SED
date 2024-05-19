unit U_tplcreate;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  StdCtrls;

type
  TTPLCreator = class(TForm)
    BNSave: TButton;
    EBTPL: TEdit;
    EBParent: TEdit;
    Label1: TLabel;
    Label2: TLabel;
    MMFields: TMemo;
    BN3DO: TButton;
    BNAI: TButton;
    BNSND: TButton;
    BNPup: TButton;
    BNTPL: TButton;
    BNCancel: TButton;
    BNParent: TButton;
    BNUpdate: TButton;
    BNCog: TButton;
    EBDesc: TEdit;
    Label3: TLabel;
    EBBBox: TEdit;
    Label4: TLabel;
    procedure BNTPLClick(Sender: TObject);
    procedure MMFieldsChange(Sender: TObject);
    procedure BNParentClick(Sender: TObject);
    procedure BNUpdateClick(Sender: TObject);
    procedure BN3DOClick(Sender: TObject);
    procedure BNAIClick(Sender: TObject);
    procedure BNSNDClick(Sender: TObject);
    procedure BNPupClick(Sender: TObject);
    procedure BNCogClick(Sender: TObject);
    procedure FormCloseQuery(Sender: TObject; var CanClose: Boolean);
    procedure BNCancelClick(Sender: TObject);
    procedure BNSaveClick(Sender: TObject);
    procedure FormCreate(Sender: TObject);
  private
    { Private declarations }
    changed:boolean;
  public
    { Public declarations }
    Procedure CreateTemplate;
    Function CheckChanged:boolean;
    Procedure SetField(const fname,fval:string);
  end;

var
  TPLCreator: TTPLCreator;

implementation

uses ResourcePicker, Misc_utils, U_templates, GlobalVars, U_3dos, J_Level,
     Jed_Main;

{$R *.DFM}

procedure TTPLCreator.BNTPLClick(Sender: TObject);
var tpl:string;
    i:integer;
begin
 tpl := ResPicker.PickTemplate(EBTPL.Text);
 EBTPL.Text := tpl;
 i := templates.IndexOfName(tpl);
 if i = -1 then exit;
 EBParent.Text := templates[i].parent;
end;

Procedure TTPLCreator.CreateTemplate;
begin
 if ProjectDir='' then
 begin
  ShowMessage('Save project first');
  exit;
 end;

 EbTPL.Text    := '';
 EBParent.Text := 'none';
 EBDesc.Text   := '';
 EBBBox.Text   := '';
 MMFields.Lines.Clear;
 changed := False;
 if ShowModal = mrOK then;
end;

Function TTPLCreator.CheckChanged:boolean;
begin
 Result := True;
 if Changed then
  Result := MsgBox('You''re about to lose changes you''ve made. Proceed?',
                 'Warning', MB_YESNO) = idYes;
end;

procedure TTPLCreator.MMFieldsChange(Sender: TObject);
begin
 Changed := True;
end;

procedure TTPLCreator.BNParentClick(Sender: TObject);
begin
 EBParent.Text := ResPicker.PickTemplate(EBParent.Text);
end;

procedure TTPLCreator.BNUpdateClick(Sender: TObject);
var tname:string;
    i:integer;
    tpl: TTemplate;
begin
 if not CheckChanged then exit;
 tname := EBTpl.Text;
 i := templates.IndexOfName(tname);
 MMFields.Lines.Clear;
 if i <> -1 then
 begin
  tpl := templates[i];
  EBDesc.Text := tpl.Desc;
  with tpl.Bbox do EBBBox.Text := Sprintf('%1f %1f %1f %1f %1f %1f',
   [x1,y1,z1,x2,y2,z2]);

  for i := 0 to tpl.vals.Count-1 do
  with tpl.vals[i] do
  begin
   MMFields.Lines.Add(Name + '=' + AsString);
  end;
 end;
 changed := False;
end;

Procedure TTPLCreator.SetField(const fname, fval:string);
var i,p:integer;
    f,s:string;
begin
 for i:=0 to MMFields.Lines.Count-1 do
 begin
  s := MMFields.Lines[i];
  p := Pos('=',s);
  if p = 0 then
    f := s
  else
    f := Copy(s, 1, p - 1);

  if CompareText(f, fname) = 0 then
  begin
   MMFields.Lines[i] := fname + '=' + fval;
   exit;
  end;
 end;
 MMFields.Lines.Add(fname + '=' + fval);
end;

procedure TTPLCreator.BN3DOClick(Sender: TObject);
var s:string;
    a3DO:T3DO;
    bbox: TThingBox;
begin
 s := ResPicker.Pick3DO('');
 if s='' then exit;
 a3DO := Load3DO(s);
 if a3Do <> nil then
 begin
  a3DO.GetBBox(bbox);
  with Bbox do EBBBox.Text := Sprintf('%1f %1f %1f %1f %1f %1f',
   [x1,y1,z1,x2,y2,z2]);

  SetField('size', DoubleToStr(a3DO.FindRadius));
  SetField('movesize', DoubleToStr(a3DO.FindRadius));
  Free3DO(a3DO);
 end;

 SetField('model3d', s);
end;

procedure TTPLCreator.BNAIClick(Sender: TObject);
var s:string;
begin
 s:=ResPicker.PickAI('');
 if s='' then exit;
 SetField('aiclass', s);
end;

procedure TTPLCreator.BNSNDClick(Sender: TObject);
var s:string;
begin
 s:=ResPicker.PickSND('');
 if s='' then exit;
 SetField('soundclass',s);
end;


procedure TTPLCreator.BNPupClick(Sender: TObject);
var s:string;
begin
 s:=ResPicker.PickPUP('');
 if s='' then exit;
 SetField('puppet',s);
end;

procedure TTPLCreator.BNCogClick(Sender: TObject);
var s:string;
begin
 s:=ResPicker.PickCOG('');
 if s='' then exit;
 SetField('cog', s);
end;

procedure TTPLCreator.FormCloseQuery(Sender: TObject;
  var CanClose: Boolean);
begin
 CanClose:=CheckChanged;
end;

procedure TTPLCreator.BNCancelClick(Sender: TObject);
begin
 Close;
end;

procedure TTPLCreator.BNSaveClick(Sender: TObject);
var s,t:string;
    i,j:integer;
    tpl:TTemplate;
    th:TJKthing;
begin
 i := templates.IndexOfName(EBTPL.Text);
 if i <> -1 then
 begin
  if MsgBox(Format('Template %s already exists! Replace?', [EBTPL.Text]), 'Warning', MB_YESNO) <> idYes
    then exit;
 end;

 s := Format('%18s %18s', [EBTPL.Text, EBParent.Text]);
 for i := 0 to MMFields.Lines.Count - 1 do
 begin
  t := MMFields.Lines[i];
  for j := Length(t) downto 1 do if (t[j] in [' ', #9]) then Delete(t, j, 1);
  s := s + ' '+ t;
 end;

 i := templates.AddFromString(s, (*replace=*)True);
 if i <> -1 then
 begin
  tpl := templates[i];
  tpl.Desc := EBDesc.Text;

  with tpl.bbox do
  if not SScanf(EBBBox.Text,'%l %l %l %l %l %l',[@x1, @y1 ,@z1, @x2, @y2, @z2])
   then FillChar(tpl.Bbox, sizeof(tpl.BBox), 0);

  case CurrentProject of
    TProjectType.JKDF2:
      t := 'master.tpl';
    TProjectType.MOTS:
      t := 'mots.tpl';
    else // IJIM
      t := 'ijim.tpl';
  end;

  templates.SaveToFile(ProjectDir + t);

  // Update level template
  i := level.templates.IndexOfName(tpl.name);
  if i <> -1 then
  begin
    level.templates.AddOrUpdate(tpl.Clone, (*update=*)True);
  end;

  for i := 0 to level.things.Count - 1 do
  begin
   th := level.things[i];
   if CompareText(th.name, tpl.name) = 0 then JedMain.ThingChanged(th);
  end;
 end;

 changed := False;
 close;
end;

procedure TTPLCreator.FormCreate(Sender: TObject);
begin
 ClientWidth  := BNCancel.Left + BNCancel.width  + BN3DO.Top;
 ClientHeight := BNCancel.top  + BNCancel.Height + BN3DO.Top;
end;

end.
