unit ResourcePicker;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  StdCtrls, ComCtrls, GlobalVars, misc_utils, ExtCtrls, Preview, jdh_jdl,
  J_Level, Buttons, u_templates, values, System.ImageList,
  Vcl.ImgList;

type
  TResPicker = class(TForm)
    ResList: TListBox;
    ImageList: TImageList;
    EBResName: TEdit;
    BNOK: TButton;
    BNCancel: TButton;
    TVGroups: TTreeView;
    BMPreview: TImage;
    Memo: TMemo;
    SBHelp: TSpeedButton;
    LBDetails: TLabel;
    Label1: TLabel;
    EBMask: TEdit;
    CBCMPs: TComboBox;
    CB3DOPrev: TCheckBox;
    Panel3D: TPanel;
    SBYAW: TScrollBar;
    SBPCH: TScrollBar;
    SBMatCell: TScrollBar;
    LBInfo: TLabel;
    procedure ResListClick(Sender: TObject);
    procedure EBResNameChange(Sender: TObject);
    procedure BNOKClick(Sender: TObject);
    procedure ResListDblClick(Sender: TObject);
    procedure TVGroupsChange(Sender: TObject; Node: TTreeNode);
    procedure FormShow(Sender: TObject);
    procedure FormHide(Sender: TObject);
    procedure FormCreate(Sender: TObject);
    procedure SBHelpClick(Sender: TObject);
    procedure EBMaskChange(Sender: TObject);
    procedure CBCMPsChange(Sender: TObject);
    procedure CB3DOPrevClick(Sender: TObject);
    procedure SBPCHChange(Sender: TObject);
    procedure FormDestroy(Sender: TObject);
    procedure TVGroupsCollapsing(Sender: TObject; Node: TTreeNode;
      var AllowCollapse: Boolean);
  private
    { Private declarations }
    { ProjDir:String;
      FLevel:TLevel; }
    InitPreviewRes: string;
    StdPreview: boolean;
    Pv: TPreviewThread;
    CurTXGroup, CurPalGroup, CurSndGroup, CurITMGroup, CurMSCGroup,
      curNWXGroup: string;

    Cur_CMP: string;
    loadcmp: boolean;
    oldOnClick: TNotifyEvent;

    Procedure AddGobGroup(const gob, dir: String; perm: boolean);
    Function AddProjDirGroup(const mask: String): TTreeNode;
    Function AddProjSubDirGroup(const subdir, mask: String; parent: TTreeNode = nil): TTreeNode;

    Function AddDirGroup(const dir, name, mask: String): TTreeNode;
    Procedure SaveCurrentGroup(var s: String);
    Procedure SetResName(const s: string); { Doesn't call OnChange }
    Procedure ChangeResName(const s: string); { Always calls OnChange }
    procedure TplListClick(Sender: TObject);
    procedure A3DOListClick(Sender: TObject);
    Procedure InitTPLPreview;
    Procedure DoneTplPreview;
    Procedure Inita3DOPreview;
    Procedure DoneA3DOPreview;

  public
    Procedure SetCMP(const cmp: string);
    Function PickThing(const curThing: String): String;
    Function PickTemplate(const curtpl: string): string;
    Function PickCMP(const CurCMP: string): string;
    Function PickSecSound(CurWav: string): string;
    Function PickMAT(CurMAT: string): string;
    Function PickCOG(CurCog: string): string;
    Function PickLayer(CurLayer: string): string;
    Function PickEpSeq(const seq: string): string;
    Function PickThingVal(const val: string): string;
    Function Pick3DO(const cur3do: String): String;
    Function PickAI(const CurAI: string): string;
    Function PickKEY(const CurKEY: string): string;
    Function PickSND(const CurSND: string): string;
    Function PickPUP(const CurPUP: string): string;
    Function PickSPR(const CurSPR: string): string;
    Function PickPAR(const CurPAR: string): string;
    Function PickPER(const CurPER: string): string;

    Function PickCOGVal(var icog, iVAL: Integer; ctype: TCOG_Type): boolean;

    { Function PickTexture(const curTexture:String):String;
      Function PickItem(Const CurItem:String):String;
      Function PickPalette(Const curPal:string):String;
      Function PickFloorSound(Const curSound:String):String;
      Function PickSound(Const curSound:String):String;
      Function PickTaunts(Const curTaunts:String):String;
      Function PickMusic(Const curMsc:String):String;
      Function PickSectorName(const curSC:string):String;
      Function PickLevel(const CurLVT:string):string;
      Function PickMovie(const CurMovie:string):string;
      Function PickTXT(const CurTXT:string):string;
      Function PickCHK(const CurCHK:string):string;
      Function PickForCHK(const CurFile:string):string;
      Function PickAnim(Const curAnim:String):String;
      Function PickItemID(Const CurID:String):String;
      Function PickNWX(Const curNWX:string):String;
      Function PickGeneratee(Const curGen:String):String;
      Function PickDropItem(Const curItem:String):String; }
    Procedure ClearGroups;
    Procedure ResetGroups(const curnode: String);
    { Property ProjectDirectory:String read ProjDir write ProjDir;
      Property Level:TLevel write FLevel; }
    { Public declarations }
    Procedure SetItemList(ls: TStringList);
  end;

var
  ResPicker: TResPicker;

implementation

uses Files, FileOperations, U_3doprev;

{$R *.DFM}

Procedure TResPicker.SetResName(const s: string);
var
  p: TNotifyEvent;
begin
  p := EBResName.OnChange;
  EBResName.OnChange := nil;
  EBResName.Text := s;
  EBResName.OnChange := p;
end;

Procedure TResPicker.ChangeResName(const s: string);
var
  i, n: Integer;
  sl: TStringList;
begin
  SetResName(s);

  if Assigned(EBResName.OnChange) then
    EBResName.OnChange(EBResName);

  if ResList.ItemIndex < 0 then
  begin
    for i := 0 to TVGroups.Items.Count - 1 do
      With TVGroups.Items[i] do
      begin
        sl := TStringList(Data);
        if sl = nil then
          continue;
        n := sl.IndexOf(s);
        if n <> -1 then
        begin
          TVGroups.Selected := TVGroups.Items[i];
          if Assigned(EBResName.OnChange) then
            EBResName.OnChange(EBResName);
          break;
        end;
      end;
  end;

end;

Procedure TResPicker.ClearGroups;
var
  i: Integer;
  Lists: TList;
  List: TStringList;
begin
  Lists := TList.Create;
  for i := 0 to TVGroups.Items.Count - 1 do
    With TVGroups.Items[i] do
    begin
      List := TStringList(Data);
      if List = nil then
        continue;
      if Lists.IndexOf(List) <> -1 then
        continue;
      List.Free;
      Lists.Add(List);
      Data := nil;
    end;
  Lists.Free;
  TVGroups.Items.Clear;
end;

Procedure TResPicker.SaveCurrentGroup(var s: String);
begin
  if TVGroups.Selected <> nil then
    s := TVGroups.Selected.Text
  else
    s := '';
end;

Procedure TResPicker.ResetGroups(const curnode: String);
var
  Node: TTreeNode;
  i: Integer;
  ni: TNotifyEvent;
begin
  Node := TVGroups.Items.GetFirstNode;
  for i := 0 to TVGroups.Items.Count - 1 do
    With TVGroups.Items[i] do
    begin
      if Text = curnode then
      begin
        Node := TVGroups.Items[i];
        break;
      end;
    end;
  ni := EBMask.OnChange;
  EBMask.OnChange := nil;
  EBMask.Text := '';
  EBMask.OnChange := ni;
  if Node = nil then
    Node := TVGroups.Items.GetFirstNode;
  TVGroups.Selected := Node;
  TVGroups.FullExpand;
  { TVGroupsChange(Nil,Node); }
  { TVGroups.Items[0].Focused:=true; }
end;

Procedure TResPicker.SetItemList(ls: TStringList);
var
  wcm: TWildCardMask;
  i: Integer;
  m: string;
begin
  ResList.Items.Clear;
  if ls = nil then
    exit;
  m := EBMask.Text;
  if m = '' then
  begin
    ResList.Items.Assign(ls);
    exit;
  end;
  if not(m[length(m)] in ['*', '?']) then
    m := m + '*';
  wcm := TWildCardMask.Create;
  try
    wcm.mask := m;
    for i := 0 to ls.Count - 1 do
      if wcm.match(ls[i]) then
        ResList.Items.Add(ls[i]);
  finally
    wcm.Free;
  end;
end;

Procedure TResPicker.AddGobGroup(const gob, dir: String; perm: boolean);
var
  Files, sl: TStringList;
  cf: TContainerFile;
  { filemask:TWildCardMask; }
  i: Integer;
begin
  if gob = '' then
    exit;
  Try
    if perm then
      cf := OpenGameContainer(gob)
    else
      cf := OpenContainer(gob);

    sl := TStringList.Create;
    { filemask:=TWildCardMask.Create;
      fileMask.Mask:=Mask; }
    cf.ChDir(dir);
    Files := cf.ListFiles;
    { for i:=0 to files.count-1 do
      if filemask.Match(Files[i]) then sl.Add(Files[i]); }
    sl.AddStrings(Files);
    sl.Sorted := true;
    sl.Sorted := false;
    TVGroups.Items.AddObject(nil, ExtractName(gob) + '>' + dir, sl);
    if not perm then
      cf.Free;
  except
    on E: Exception do
      PanMessage(mt_Error, E.Message);
    else
      exit;
  end;
end;

Function TResPicker.AddProjSubDirGroup(const subdir, mask: String; parent: TTreeNode): TTreeNode;
var
  sl: TStringList;
begin
  if ProjectDir = '' then
    exit;
  sl := TStringList.Create;
  ListDirMask(ProjectDir + subdir, mask, sl);
  { if sl.count=0 then sl.free else }
  Result := TVGroups.Items.AddChildObject(parent, subdir, sl);
end;

Function TResPicker.AddProjDirGroup(const mask: String): TTreeNode;
begin
  Result := AddDirGroup(ProjectDir, 'Project Directory', mask);
end;

Function TResPicker.AddDirGroup(const dir, name, mask: String): TTreeNode;
var
  sl: TStringList;
begin
  if (dir <> '') and DirectoryExists(dir) then
  begin
    sl := TStringList.Create;
    ListDirMask(dir, mask, sl);
    Result := TVGroups.Items.AddObject(nil, Name, sl);
  end;
end;

procedure TResPicker.TplListClick(Sender: TObject);
var
  n: Integer;
  tplval: TTPLvalue;
  tpname: string;
begin
  LBInfo.Caption := 'Type: None';
  SBPCH.Enabled := False;
  SBYAW.Enabled := False;
  n := ResList.ItemIndex;
  if n < 0 then
    exit;
  tpname := ResList.Items[n];
  SetResName(tpname);
  n := templates.IndexOfName(tpname);
  if n = -1 then
      LBDetails.Caption := ''
  else
  begin
    tplval := templates.GetTPLField(tpname, 'type');
    if tplval <> nil then
      LBInfo.Caption := 'Type: ' + tplval.AsString;

    tplval := templates.GetTPLField(tpname, 'model3d');
    if tplval <> nil then
      begin
        SBPCH.Enabled := CB3DOPrev.checked;
        SBYAW.Enabled := CB3DOPrev.checked;
        if Pv <> nil then
          Pv.StartPreview(tplval.AsString)
        else
          InitPreviewRes := tplval.AsString;
      end
    else
     Panel3D.Invalidate;

    LBDetails.Caption := templates[n].Desc;
  end;
end;

Procedure TResPicker.InitTPLPreview;
begin
  { Memo.Visible:=true;
    Memo.ScrollBars:=ssVertical; }
  LBInfo.Caption := '';
  LBInfo.Visible := true;
  LBDetails.Caption := '';
  LBDetails.Visible := true;
  oldOnClick := ResList.OnClick;
  ResList.OnClick := TplListClick;
  CB3DOPrev.Visible := true;
  Panel3D.Visible := true;
  SBPCH.Visible := true;
  SBYAW.Visible := true;

  StdPreview := true;
  Init3DOPreview;
end;

Procedure TResPicker.DoneTplPreview;
begin
  { Memo.Visible:=false;
    Memo.ScrollBars:=ssBoth; }
  LBInfo.Visible := false;
  LBDetails.Visible := false;
  ResList.OnClick := oldOnClick;
  CB3DOPrev.Visible := false;
  Panel3D.Visible := false;

  SBPCH.Visible := false;
  SBYAW.Visible := false;

  StdPreview := true;
  Done3DOPreview;
end;

Function TResPicker.PickTemplate(const curtpl: string): string;
var
  sl: TStringList;
  i: Integer;
begin
  InitTPLPreview;
  SetCMP(level.GetMasterCMP);
  try
    ClearGroups;
    sl := TStringList.Create;
    for i := 0 to templates.Count - 1 do
      with templates[i] do
      begin
        sl.Add(Name);
      end;

    TVGroups.Items.AddObject(nil, 'Templates', sl);

    case CurrentProject of
      TProjectType.JKDF2:
        LoadJLH('tpls.jlh', TVGroups);
      TProjectType.MOTS:
        LoadJLH('mtpls.jlh', TVGroups)
      else
        LoadJLH('ijimtpls.jlh', TVGroups);
    end;

    ResetGroups('');
    ChangeResName(curtpl);
    CB3DOPrev.checked := true;

    if ShowModal = idOK then
      Result := EBResName.Text
    else
      Result := curtpl;
    { SaveCurrentGroup(curTXGroup); }
  finally
    DoneTplPreview;
  end;
end;

Function TResPicker.PickThing(const curThing: String): String;
var
  sl: TStringList;
  i: Integer;
  oldp: TNotifyEvent;
begin
  InitTPLPreview;
  SetCMP(Level.GetMasterCMP);
  Try
    ClearGroups;
    sl := TStringList.Create;
    for i := 0 to templates.Count - 1 do
      With templates[i] do
      begin
        if not(Name[1] in ['_', '+']) then
          sl.Add(Name);
      end;
    TVGroups.Items.AddObject(nil, 'Templates', sl);

    case CurrentProject of
      TProjectType.JKDF2:
        LoadJLH('tpls.jlh', TVGroups);
      TProjectType.MOTS:
        LoadJLH('mtpls.jlh', TVGroups);
      else
        LoadJLH('ijimtpls.jlh', TVGroups);
    end;

    ResetGroups('');
    ChangeResName(curThing);
    CB3DOPrev.checked := true;

    if ShowModal = idOK then
      Result := EBResName.Text
    else
      Result := curThing;
    { SaveCurrentGroup(curTXGroup); }
  finally
    DoneTplPreview;
  end;

end;

Procedure TResPicker.Inita3DOPreview;
begin
  { Memo.Visible:=true;
    Memo.ScrollBars:=ssVertical; }
  LBInfo.Caption := '';
  LBInfo.Visible := true;
  LBDetails.Caption := '';
  LBDetails.Visible := true;
  CB3DOPrev.Visible := true;

  oldOnClick := ResList.OnClick;
  ResList.OnClick := A3DOListClick;

  Panel3D.Visible := true;
  SBPCH.Visible := true;
  SBYAW.Visible := true;

  StdPreview := true;
  Init3DOPreview;
end;

Procedure TResPicker.DoneA3DOPreview;
begin
  { Memo.Visible:=false;
    Memo.ScrollBars:=ssBoth; }
  LBInfo.Visible := false;
  LBDetails.Visible := false;
  ResList.OnClick := oldOnClick;
  CB3DOPrev.Visible := false;
  Panel3D.Visible := false;
  SBPCH.Visible := false;
  SBYAW.Visible := false;

  StdPreview := true;
  Done3DOPreview;
end;

procedure TResPicker.A3DOListClick(Sender: TObject);
var
  n: Integer;
  fname: string;
begin
  n := ResList.ItemIndex;
  if n < 0 then
    exit;
  fname := ResList.Items[n];
  SetResName(fname);
  SBPCH.Enabled := CB3DOPrev.checked;
  SBYAW.Enabled := CB3DOPrev.checked;
  if Pv <> nil then
    Pv.StartPreview(fname)
  else
    InitPreviewRes := fname;
end;

Function TResPicker.Pick3DO(const cur3do: String): String;
var
  sl: TStringList;
  i: Integer;
begin
  Inita3DOPreview;
  SetCMP(Level.GetMasterCMP);
  Try
    ClearGroups;

    var pn := AddProjDirGroup('*.3do');
    AddProjSubDirGroup('3do\', '*.3do', pn);

    AddGobGroup(Res1_gob, '3do', true);
    AddGobGroup(Res2_gob, '3do', true);

    if CurrentProject = IJIM then
      AddDirGroup(GameDir + '3do\', 'Install>3do', '*.3do');

    case CurrentProject of
      TProjectType.JKDF2:
        LoadJLH('3dos.jlh', TVGroups);
      TProjectType.MOTS:
        LoadJLH('m3dos.jlh', TVGroups)
      else // IJIM
        LoadJLH('ijim3dos.jlh', TVGroups)
    end;

    ResetGroups('');
    ChangeResName(cur3do);
    CB3DOPrev.checked := true;

    if ShowModal = idOK then
      Result := EBResName.Text
    else
      Result := cur3do;
    { SaveCurrentGroup(curTXGroup); }
  finally
    DoneA3DOPreview;
  end;
end;

{ Function TResPicker.PickSectorName(const curSC:string):String;
  var i:integer;sl:TStringList;
  begin
  end;

  Function TResPicker.PickTexture(const curTexture:String):String;
  begin
  ClearGroups;
  AddLabGroup(textures_lab,'*.pcx;*.atx',true);
  AddLabGroup(olpatch2_lab,'*.pcx;*.atx',true);
  AddProjDirGroup('*.pcx;*.atx');
  LoadOLH('textures.olh',TVGroups);
  ResetGroups(curTXGroup);
  ChangeResName(curTexture);

  if ShowModal=idOK then Result:=EBResName.Text else Result:=curTexture;
  SaveCurrentGroup(curTXGroup);
  end;

  Function TResPicker.PickPalette(Const curPal:string):String;
  begin
  ClearGroups;
  AddLabGroup(Outlaws_lab,'*.pcx',true);
  LoadOLH('palettes.olh',TVGroups);
  AddProjDirGroup('*.pcx');
  ResetGroups(curPalGroup);
  ChangeResName(curPal+'.pcx');

  if ShowModal=idOK then Result:=ChangeFileExt(EBResName.Text,'') else Result:=curPal;
  SaveCurrentGroup(curPalGroup);
  end;


  Function TResPicker.PickItem(Const CurItem:String):String;
  var sl:TStringList;
  begin
  ClearGroups;
  AddLabGroup(weapons_lab,'*.itm',true);
  AddLabGroup(objects_lab,'*.itm',true);
  AddLabGroup(olpatch1_lab,'*.itm',true);
  AddLabGroup(olpatch2_lab,'*.itm',true);
  AddProjDirGroup('*.itm');
  sl:=TStringList.Create;
  sl.Add('PLAYER');
  sl.Add('STARTPOS');
  sl.Add('BALLSTAR');
  sl.Add('DOCSTART');
  sl.Add('FLAGSTAR');

  TVGroups.Items.AddObject(nil,'Special',sl);
  LoadOLH('objects.olh',TVGroups);

  ResetGroups(CurITMGroup);
  ChangeResName(curItem+'.itm');

  if ShowModal=idOK then Result:=ChangeFileExt(EBResName.Text,'') else Result:=curItem;
  SaveCurrentGroup(CurITMGroup);
  end;


  Function GetIDFromITM(const ITM:String):Integer;
  var t:TLecTextFile;s,w:String;p:integer;
  begin
  Result:=-1;
  t:=TLecTextFile.CreateRead(OpenGameFile(ITM));
  While not t.eof do
  begin
  T.Readln(s);
  p:=GetWord(s,1,w);
  if w<>'INT' then Continue;
  p:=GetWord(s,p,w);
  if w<>'USE_ID' then continue;
  GetWord(s,p,w);
  if not ValInt(w,Result) then Result:=-1;
  break;
  end;
  t.FClose;
  end;


  procedure TResPicker.PickIDResListClick(Sender: TObject);
  var i:Integer;
  begin
  if ResList.ItemIndex<0 then exit;
  i:=GetIDFromITM(ResList.Items[ResList.ItemIndex]);
  if i<>-1 then SetResName(IntToStr(i));
  Pv.StartPreview(ResList.Items[ResList.ItemIndex]);
  end;


  Function TResPicker.PickItemID(Const CurID:String):String;
  begin
  ClearGroups;
  LoadOLH('use_id.olh',TVGroups);
  AddProjDirGroup('*.itm');
  ResetGroups('');
  ResList.OnClick:=PickIDResListClick;
  Try
  ChangeResName(curID);
  if ShowModal=idOK then Result:=ChangeFileExt(EBResName.Text,'') else Result:=curID;
  Finally
  ResList.OnClick:=ResListClick;
  end;
  end;


  Function TResPicker.PickSound(Const curSound:String):String;
  var sl:TStringList;
  begin
  ClearGroups;
  AddLabGroup(Sounds_lab,'*.wav',true);
  AddLabGroup(taunts_lab,'*.wav',true);
  sl:=TStringList.Create;
  sl.Add('NULL');
  TVGroups.Items.AddObject(nil,'Special',sl);
  LoadOLH('sounds.olh',TVGroups);
  AddProjDirGroup('*.wav');
  ResetGroups(curSndGroup);
  ChangeResName(CurSound);
  if ShowModal=idOK then Result:=EBResName.Text else Result:=curSound;
  SaveCurrentGroup(curSndGroup);
  end;


  procedure TResPicker.TauntsResListClick(Sender: TObject);
  begin
  if ResList.ItemIndex>=0 then PV.StartPreview(ResList.Items[ResList.ItemIndex]);
  end;

  procedure TResPicker.TauntsResListDblClick(Sender: TObject);
  begin
  BNToList.Click;
  end;

  Procedure TResPicker.SetTaunts(const taunts:string);
  var ps,pc:Pchar;
  s:array[0..255] of char;
  begin
  LBTauntList.Items.Clear;
  ps:=@Taunts[1];
  repeat
  pc:=StrScan(ps,',');
  if pc=nil then pc:=StrEnd(ps);
  LBTauntList.Items.Add(Trim(StrLCopy(s,ps,pc-ps)));
  ps:=pc+1;
  until pc^=#0;
  end;

  Function TResPicker.GetTaunts:String;
  var i:integer;
  begin
  Result:='';
  for i:=0 to LBTauntList.Items.Count-2 do
  Result:=Result+LBTauntList.Items[i]+',';
  Result:=Result+LBTauntList.Items[LBTauntList.Items.Count-1];
  end;


  Function TResPicker.PickTaunts(Const curTaunts:String):String;
  begin
  ClearGroups;
  AddLabGroup(Sounds_lab,'*.wav',true);
  AddLabGroup(taunts_lab,'*.wav',true);
  AddProjDirGroup('*.wav');
  LoadOLH('sounds.olh',TVGroups);
  ResetGroups(curSndGroup);

  SaveCurrentGroup(curSndGroup);

  LBTauntList.Show;
  ResList.MultiSelect:=true;
  ResList.ExtendedSelect:=true;
  ResList.OnClick:=TauntsResListClick;
  ResList.OnDblClick:=TauntsResListDblClick;
  BNToList.Show;
  BNFromList.Show;
  EBResName.Hide;

  try
  SetTaunts(CurTaunts);
  if ShowModal=idOK then Result:=GetTaunts else Result:=curTaunts;
  finally
  SaveCurrentGroup(curSndGroup);
  ResList.MultiSelect:=false;
  ResList.ExtendedSelect:=false;
  ResList.OnClick:=ResListClick;
  ResList.OnDblClick:=ResListDblClick;
  LBTauntList.Hide;
  BNToList.Hide;
  BNFromList.Hide;
  EBResName.Show;
  end;
  end;

  Function TResPicker.PickNWX(Const curNWX:string):String;
  begin
  ClearGroups;
  AddLabGroup(weapons_lab,'*.nwx',true);
  AddLabGroup(objects_lab,'*.nwx',true);
  AddLabGroup(olpatch2_lab,'*.nwx',true);
  AddProjDirGroup('*.nwx');
  LoadOLH('sprites.olh',TVGroups);
  ResetGroups(curNWXGroup);
  ChangeResName(curNWX);
  if ShowModal=idOK then Result:=EBResName.Text else Result:=curNWX;
  SaveCurrentGroup(curNWXGroup);
  end;

  Function TResPicker.PickGeneratee(Const curGen:String):String;
  begin
  ClearGroups;
  AddLabGroup(weapons_lab,'*.nwx',true);
  AddLabGroup(objects_lab,'*.nwx',true);
  AddLabGroup(olpatch2_lab,'*.nwx',true);
  AddLabGroup(Sounds_lab,'*.wav',true);
  AddLabGroup(taunts_lab,'*.wav',true);
  AddLabGroup(weapons_lab,'*.itm',true);
  AddLabGroup(objects_lab,'*.itm',true);
  AddLabGroup(olpatch1_lab,'*.itm',true);
  AddLabGroup(olpatch2_lab,'*.itm',true);
  AddProjDirGroup('*.nwx;*.itm;*.wav');
  LoadOLH('objects.olh',TVGroups);
  LoadOLH('sprites.olh',TVGroups);
  LoadOLH('sounds.olh',TVGroups);
  ResetGroups('');
  ChangeResName(curGen);
  if ShowModal=idOK then Result:=EBResName.Text else Result:=curGen;
  end;

  Function TResPicker.PickDropItem(Const curItem:String):String;
  begin
  ClearGroups;
  AddLabGroup(weapons_lab,'*.nwx',true);
  AddLabGroup(objects_lab,'*.nwx',true);
  AddLabGroup(olpatch2_lab,'*.nwx',true);
  AddLabGroup(weapons_lab,'*.itm',true);
  AddLabGroup(objects_lab,'*.itm',true);
  AddLabGroup(olpatch1_lab,'*.itm',true);
  AddLabGroup(olpatch2_lab,'*.itm',true);
  AddProjDirGroup('*.nwx;*.itm');
  LoadOLH('objects.olh',TVGroups);
  LoadOLH('sprites.olh',TVGroups);
  ResetGroups('');
  ChangeResName(curItem);
  if ShowModal=idOK then Result:=EBResName.Text else Result:=curItem;
  end;

  Function TResPicker.PickAnim(Const curAnim:String):String;
  var sl:TStringList;
  begin
  ClearGroups;
  AddLabGroup(objects_lab,'*.nwx;*.3do',true);
  AddLabGroup(weapons_lab,'*.nwx;*.3do',true);
  AddLabGroup(olpatch2_lab,'*.nwx',true);
  sl:=TStringList.Create;
  sl.Add('NULL');
  TVGroups.Items.AddObject(nil,'Special',sl);
  LoadOLH('objects.olh',TVGroups);
  LoadOLH('3dos.olh',TVGroups);
  AddProjDirGroup('*.nwx;*.3do');
  ResetGroups('');
  ChangeResName(CurAnim); }
(* if ShowModal=idOK then Result:=EBResName.Text else Result:=curAnim;
  { SaveCurrentGroup(curSndGroup);}
  end;


  Function TResPicker.PickFloorSound(Const curSound:String):String;
  var sl:TStringList;
  begin
  ClearGroups;
  AddLabGroup(Sounds_lab,'*.wav',true);
  sl:=TStringList.Create;
  sl.Add('NULL');
  TVGroups.Items.AddObject(nil,'Special',sl);
  AddProjDirGroup('*.wav');
  LoadOLH('sounds.olh',TVGroups);
  ResetGroups(CurSndGroup);
  if UpperCase(CurSound)<>'NULL' then
  ChangeResName(curSound+'.wav') else ChangeResName(CurSound);
  if ShowModal=idOK then Result:=ChangeFileExt(EBResName.Text,'') else Result:=curSound;
  SaveCurrentGroup(curSndGroup);
  end;

  Function TResPicker.PickMusic(Const curMsc:String):String;
  var sl:TStringList;
  begin
  ClearGroups;
  AddLabGroup(outlaws_lab,'*.msc',true);
  AddLabGroup(geo_lab,'*.msc',true);
  AddLabGroup(olpatch2_lab,'*.msc',true);
  sl:=TStringList.Create;
  sl.Add('NULL');
  TVGroups.Items.AddObject(nil,'Special',sl);
  AddProjDirGroup('*.msc');
  LoadOLH('music.olh',TVGroups);
  ResetGroups(curMSCGroup);
  ChangeResName(CurMsc);
  if ShowModal=idOK then Result:=EBResName.Text else Result:=curMsc;
  SaveCurrentGroup(curMSCGroup);
  end;


  Function TResPicker.PickLevel(const CurLVT:string):string;
  begin
  ClearGroups;
  AddLabGroup(geo_lab,'*.lvt',true);
  AddProjDirGroup('*.lvt');
  ResetGroups('');
  ChangeResName(ChangeFileExt(CurLVT,'.lvt'));
  if ShowModal=idOK then Result:=EBResName.Text else Result:=curLVT;
  Result:=ChangeFileExt(Result,'');
  end;

  Function TResPicker.PickMovie(const CurMovie:string):string;
  begin
  ClearGroups;
  AddDirGroup(GameDir,GameDir,'*.san');
  AddDirGroup(CDDir,CDDir,'*.san');
  AddProjDirGroup('*.san');
  LoadOLH('movies.olh',TVGroups);
  ResetGroups('');
  ChangeResName(CurMovie);
  if ShowModal=idOK then Result:=EBResName.Text else Result:=curMovie;
  end;

  Function TResPicker.PickTXT(const CurTXT:string):string;
  begin
  ClearGroups;
  AddDirGroup(GameDir,GameDir,'*.txt');
  AddDirGroup(CDDir,CDDir,'*.txt');
  AddProjDirGroup('*.txt');
  ResetGroups('');
  ChangeResName(CurTxt);
  if ShowModal=idOK then Result:=EBResName.Text else Result:=curTxt;
  end; *)

procedure TResPicker.ResListClick(Sender: TObject);
begin
  if ResList.ItemIndex < 0 then
    exit;
  SetResName(ResList.Items[ResList.ItemIndex]);
  if Pv <> nil then
    Pv.StartPreview(EBResName.Text);
end;

procedure TResPicker.EBResNameChange(Sender: TObject);
var
  i: Integer;
begin
  i := ResList.Items.IndexOf(EBResName.Text);
  if i <> -1 then
  begin
    ResList.ItemIndex := i;
    { ResListClick(ResList); }
  end;
end;

procedure TResPicker.BNOKClick(Sender: TObject);
begin
  ModalResult := mrOk;
  Hide;
end;

procedure TResPicker.ResListDblClick(Sender: TObject);
begin
  BNOK.Click;
end;

procedure TResPicker.TVGroupsChange(Sender: TObject; Node: TTreeNode);
begin
  if Node = nil then
    SetItemList(nil)
  else
    SetItemList(TStringList(Node.Data));
end;

procedure TResPicker.TVGroupsCollapsing(Sender: TObject; Node: TTreeNode;
  var AllowCollapse: Boolean);
begin
  AllowCollapse := False;
end;

Procedure TResPicker.SetCMP(const cmp: string);
begin
  if CurrentProject = TProjectType.IJIM then
    loadcmp := false
  else
    begin
      Cur_CMP := cmp;
      loadcmp := true;
    end;
end;

procedure TResPicker.FormShow(Sender: TObject);
var
  cf: TContainerFile;
  i: Integer;
begin
  SBMatCell.Visible := false;
  SBYAW.Position := 0;
  SBPCH.Position := 0;
  Pv := nil;
  if not StdPreview then
    exit;
  Pv := TPreviewThread.Create(BMPreview, Memo, LBDetails, SBMatCell);
  if loadcmp then
    begin
      CBCMPs.Visible := true;
      cf := OpenGameContainer(Res2_gob);
      cf.ChDir('misc\cmp');
      CBCMPs.Items.Assign(cf.ListFiles);
      i := CBCMPs.Items.IndexOf(Cur_CMP);
      if i = -1 then
        i := CBCMPs.Items.Add(Cur_CMP);
      CBCMPs.ItemIndex := i;
      CBCMPsChange(nil);
    end;

  if not InitPreviewRes.IsEmpty then
    Pv.StartPreview(InitPreviewRes)
  else if ResList.ItemIndex >= 0 then
    ResListClick(nil);
end;

procedure TResPicker.FormHide(Sender: TObject);
begin
  InitPreviewRes := '';
  if Pv <> nil then
  begin
    Pv.FreeOnTerminate := False;
    Pv.Terminate;
    Pv.Resume;

    Pv.WaitFor;
    Pv.Free;
    Pv := nil
  end;
  if loadcmp then
  begin
    CBCMPs.Visible := false;
    loadcmp := false;
  end;
end;

procedure TResPicker.FormCreate(Sender: TObject);
begin
  ClientHeight := BNOK.Top + BNOK.height + TVGroups.Top;
  ClientWidth := ResList.Left + ResList.Width + TVGroups.Left;
  StdPreview := true;
end;

(* Function TResPicker.PickCHK(const CurCHK:string):string;
  begin
  ClearGroups;
  AddLabGroup(geo_lab,'*.chk',true);
  AddProjDirGroup('*.chk');
  ResetGroups('');
  ChangeResName(CurCHK);
  if ShowModal=idOK then Result:=EBResName.Text else Result:=curCHK;
  end;

  Function TResPicker.PickForCHK(const CurFile:string):string;
  begin
  ClearGroups;
  AddLabGroup(outlaws_lab,'*.phy',true);
  AddLabGroup(weapons_lab,'*.itm',true);
  AddLabGroup(objects_lab,'*.itm',true);
  AddLabGroup(olpatch1_lab,'*.itm',true);
  AddLabGroup(olpatch2_lab,'*.itm',true);
  AddProjDirGroup('*.itm;*phy');
  LoadOLH('objects.olh',TVGroups);
  ResetGroups('');
  ChangeResName(CurFile);
  if ShowModal=idOK then Result:=EBResName.Text else Result:=curFile;
  end; *)

procedure TResPicker.SBHelpClick(Sender: TObject);
begin
  //Application.HelpFile := basedir + 'jedhelp.hlp';
  //Application.HelpContext(470);
end;

Function TResPicker.PickCMP(const CurCMP: string): string;
begin
  ClearGroups;

  var pn := AddProjDirGroup('*.cmp');
  AddProjSubDirGroup('misc\cmp\', '*.cmp', pn);
  AddGobGroup(Res2_gob, 'misc\cmp', true);

  ResetGroups('');
  ChangeResName(CurCMP);
  if ShowModal = idOK then
    Result := EBResName.Text
  else
    Result := CurCMP;
end;

Function TResPicker.PickSecSound(CurWav: string): string;
begin
  ClearGroups;

  var pn := AddProjDirGroup('*.wav');
  AddProjSubDirGroup('sound\', '*.wav', pn);
  if CurrentProject <> IJIM  then
    AddProjSubDirGroup('voice\', '*.wav', pn);

  AddGobGroup(Res1_gob, 'sound', true);
  AddGobGroup(Res2_gob, 'sound', true);
  if CurrentProject <> IJIM  then
    AddGobGroup(Res1_gob, 'voice', true);

  if CurrentProject = TProjectType.MOTS then
    AddGobGroup(Res2_gob, 'voiceuu', true);

  if CurrentProject = IJIM  then
    AddDirGroup(GameDir + 'sound\', 'Install>sound', '*.wav');

  case CurrentProject of
    TProjectType.JKDF2:
      LoadJLH('wavs.jlh', TVGroups);
    TProjectType.MOTS:
      LoadJLH('mwavs.jlh', TVGroups)
    else // IJIM
        LoadJLH('ijimwavs.jlh', TVGroups)
  end;

  ResetGroups('');
  ChangeResName(CurWav);
  if ShowModal = idOK then
    Result := EBResName.Text
  else
    Result := CurWav;
end;

Function TResPicker.PickMAT(CurMAT: string): string;
begin
  ClearGroups;

  var pn := AddProjDirGroup('*.mat');
  AddProjSubDirGroup('mat\', '*.mat', pn);
  if CurrentProject <> IJIM then
      AddProjSubDirGroup('3do\mat\', '*.mat', pn);

  AddGobGroup(Res1_gob, 'mat', true);
  AddGobGroup(Res2_gob, 'mat', true);
  if CurrentProject <> IJIM then
    AddGobGroup(Res2_gob, '3do\mat', true)
  else
    AddDirGroup(GameDir + 'mat\', 'Install>mat', '*.mat');

  case CurrentProject of
    TProjectType.JKDF2:
      LoadJLH('mats.jlh', TVGroups);
    TProjectType.MOTS:
      LoadJLH('mmats.jlh', TVGroups);
    else // IJIM
        LoadJLH('ijimats.jlh', TVGroups)
  end;

  ResetGroups('');
  ChangeResName(CurMAT);

  if ShowModal = idOK then
  begin
    Result := EBResName.Text;
  end
  else
    Result := CurMAT;
end;

Function TResPicker.PickCOG(CurCog: string): string;
begin
  ClearGroups;

  var pn := AddProjDirGroup('*.cog');
  AddProjSubDirGroup('cog\', '*.cog', pn);

  AddGobGroup(sp_gob, 'cog', true);
  AddGobGroup(mp1_gob, 'cog', true);
  AddGobGroup(mp2_gob, 'cog', true);
  AddGobGroup(mp3_gob, 'cog', true);
  AddGobGroup(Res1_gob, 'cog', true);
  AddGobGroup(Res2_gob, 'cog', true);

  if CurrentProject = IJIM then
      AddDirGroup(GameDir + 'cog\', 'Install>cog', '*.cog');

  case CurrentProject of
    TProjectType.JKDF2:
      LoadJLH('cogs.jlh', TVGroups);
    TProjectType.MOTS:
      LoadJLH('mcogs.jlh', TVGroups);
    else // IJIM
      LoadJLH('ijimcogs.jlh', TVGroups)
  end;

  ResetGroups('');
  ChangeResName(CurCog);
  if ShowModal = idOK then
    Result := EBResName.Text
  else
    Result := CurCog;
end;

Function TResPicker.PickLayer(CurLayer: string): string;
var
  sl: TStringList;
begin
  ClearGroups;
  sl := TStringList.Create;
  sl.Assign(Level.Layers);
  TVGroups.Items.AddObject(nil, 'Layers', sl);
  ResetGroups('');
  ChangeResName(CurLayer);
  if ShowModal = idOK then
    Result := EBResName.Text
  else
    Result := CurLayer;
end;

Function TResPicker.PickEpSeq(const seq: string): string;
var
  sl: TStringList;
begin
  ClearGroups;

  AddProjDirGroup('*.jkl;*.smk;*.san');
  AddDirGroup(GameDir + 'Resource\Video\', 'Resource\Video on HD',  '*.smk;*.san');

  ResetGroups('');
  ChangeResName(seq);
  if ShowModal = idOK then
    Result := EBResName.Text
  else
    Result := seq;
end;

Function TResPicker.PickThingVal(const val: string): string;
var
  sl: TStringList;
begin
  ClearGroups;
  sl := TStringList.Create;
  ResetGroups('');
  ChangeResName(val);
  if ShowModal = idOK then
    Result := EBResName.Text
  else
    Result := val;
end;

Function TResPicker.PickAI(const CurAI: string): string;
begin
  ClearGroups;

  var pn := AddProjDirGroup('*.ai;*.ai0;*.ai2');
  AddProjSubDirGroup('misc\ai\', '*.ai;*.ai0;*.ai2', pn);

  AddGobGroup(Res1_gob, 'misc\ai', true);
  AddGobGroup(Res2_gob, 'misc\ai', true);

  if CurrentProject = IJIM then
      AddDirGroup(GameDir + 'misc\ai\', 'Install>misc\ai', '*.ai');

  ResetGroups('');
  ChangeResName(CurAI);
  if ShowModal = idOK then
    Result := EBResName.Text
  else
    Result := CurAI;
end;

Function TResPicker.PickKEY(const CurKEY: string): string;
begin
  ClearGroups;

  var pn := AddProjDirGroup('*.key');
  AddProjSubDirGroup('3do\key\', '*.key', pn);

  AddGobGroup(Res2_gob, '3do\key', true);
  if CurrentProject = IJIM then
      AddDirGroup(GameDir + '3do\key\', 'Install>3do\key', '*.key');

  ResetGroups('');
  ChangeResName(CurKEY);
  if ShowModal = idOK then
    Result := EBResName.Text
  else
    Result := CurKEY;
end;

Function TResPicker.PickSND(const CurSND: string): string;
begin
  ClearGroups;

  var pn := AddProjDirGroup('*.snd');
  AddProjSubDirGroup('misc\snd\', '*.snd', pn);

  AddGobGroup(Res1_gob, 'misc\snd', true);
  AddGobGroup(Res2_gob, 'misc\snd', true);

  if CurrentProject = IJIM then
      AddDirGroup(GameDir + 'misc\snd\', 'Install>misc\snd', '*.snd');

  ResetGroups('');
  ChangeResName(CurSND);
  if ShowModal = idOK then
    Result := EBResName.Text
  else
    Result := CurSND;
end;

Function TResPicker.PickPUP(const CurPUP: string): string;
begin
  ClearGroups;

  var pn := AddProjDirGroup('*.pup');
  AddProjSubDirGroup('misc\pup\', '*.pup', pn);

  AddGobGroup(Res2_gob, 'misc\pup', true);
  if CurrentProject = IJIM then
      AddDirGroup(GameDir + 'misc\pup\', 'Install>misc\pup', '*.pup');

  ResetGroups('');
  ChangeResName(CurPUP);
  if ShowModal = idOK then
    Result := EBResName.Text
  else
    Result := CurPUP;
end;

Function TResPicker.PickSPR(const CurSPR: string): string;
begin
  ClearGroups;

  var pn := AddProjDirGroup('*.spr');
  AddProjSubDirGroup('misc\spr\', '*.spr', pn);

  AddGobGroup(Res1_gob, 'misc\spr', true);
  AddGobGroup(Res2_gob, 'misc\spr', true);

  if CurrentProject = IJIM then
      AddDirGroup(GameDir + 'misc\spr\', 'Install>misc\spr', '*.spr');

  ResetGroups('');
  ChangeResName(CurSPR);
  if ShowModal = idOK then
    Result := EBResName.Text
  else
    Result := CurSPR;
end;

Function TResPicker.PickPAR(const CurPAR: string): string;
begin
  ClearGroups;

  var pn := AddProjDirGroup('*.par');
  AddProjSubDirGroup('misc\par\', '*.par', pn);

  AddGobGroup(Res1_gob, 'misc\par', true);
  AddGobGroup(Res2_gob, 'misc\par', true);

  if CurrentProject = IJIM then
      AddDirGroup(GameDir + 'misc\par\', 'Install>misc\par', '*.par');

  ResetGroups('');
  ChangeResName(CurPAR);
  if ShowModal = idOK then
    Result := EBResName.Text
  else
    Result := CurPAR;
end;

Function TResPicker.PickPER(const CurPER: string): string;
begin
  ClearGroups;

  var pn := AddProjDirGroup('*.per');
  AddProjSubDirGroup('misc\per\', '*.per', pn);

  AddGobGroup(Res2_gob, 'misc\per', true);


  ResetGroups('');
  ChangeResName(CurPER);
  if ShowModal = idOK then
    Result := EBResName.Text
  else
    Result := CurPER;
end;

Function TResPicker.PickCOGVal(var icog, iVAL: Integer;
  ctype: TCOG_Type): boolean;
var
  sl: TStringList;
  i, j: Integer;
begin
  ClearGroups;

  for i := 0 to level.cogs.Count - 1 do
    with level.cogs[i] do
    begin
      sl := TStringList.Create;
      for j := 0 to vals.Count - 1 do
        With Vals[j] do
          if cog_type = ctype then
            sl.Add(Name);

      TVGroups.Items.AddObject(nil, Name, sl);
    end;

  ResetGroups('');
  SetResName('');

  if icog <> -1 then
  begin
    TVGroups.Selected := TVGroups.Items[icog];
    ResList.ItemIndex := iVAL;
  end;

  { }
  Result := ShowModal = idOK;
  if not Result then
    exit;
  if TVGroups.Selected = nil then
    exit;
  icog := TVGroups.Selected.Index;
  iVAL := level.cogs[icog].vals.IndexOfName(EBResName.Text);
  Result := (icog >= 0) and (iVAL >= 0);
end;

procedure TResPicker.EBMaskChange(Sender: TObject);
begin
  if TVGroups.Selected = nil then
    SetItemList(nil)
  else
    SetItemList(TStringList(TVGroups.Selected.Data));
end;

procedure TResPicker.CBCMPsChange(Sender: TObject);
begin
  if CompareText(Level.GetMasterCMP, CBCMPs.Text) = 0 then
  begin
    CBCMPs.Hint := '';
  end
  else
  begin
    CBCMPs.Hint := 'The selected CMP is different than "Master" CMP';
  end;

  Pv.SetCMP(CBCMPs.Text);
  if Assigned(ResList.OnClick) then
    ResList.OnClick(ResList);
end;

procedure TResPicker.CB3DOPrevClick(Sender: TObject);
begin
  Set3DOPrevActive(CB3DOPrev.checked);
  if CB3DOPrev.checked then
    ResList.OnClick(ResList)
  else
    begin
      SBPCH.Enabled := false;
      SBYAW.Enabled := false;
      Panel3D.Invalidate;
    end;
end;

procedure TResPicker.SBPCHChange(Sender: TObject);
begin
  P3DO_SetPCHYAW(SBPCH.Position, SBYAW.Position);
end;

procedure TResPicker.FormDestroy(Sender: TObject);
begin
  ClearGroups;
end;

end.
