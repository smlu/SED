unit FileDialogs;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  StdCtrls, FileCtrl, Grids, Shlobj, ComCtrls, Files, FileOperations,
  Misc_utils, GlobalVars, Buttons;

type
  TGetFileOpen = class(TForm)
    EBFname: TEdit;
    BNOpen: TButton;
    BNCancel: TButton;
    CBFilter: TFilterComboBox;
    Label1: TLabel;
    Label2: TLabel;
    Label4: TLabel;
    DirList: TListBox;
    Label3: TLabel;
    LBContainer: TLabel;
    LBFileSize: TLabel;
    OpenDialog: TOpenDialog;
    BNDirUp: TBitBtn;
    procedure FormCreate(Sender: TObject);
    procedure EBFnameChange(Sender: TObject);
    procedure DirListClick(Sender: TObject);
    procedure CBFilterChange(Sender: TObject);
    procedure DirListDblClick(Sender: TObject);
    procedure BNOpenClick(Sender: TObject);
    procedure BNDirUpClick(Sender: TObject);
    procedure FormKeyDown(Sender: TObject; var Key: Word;
      Shift: TShiftState);
  private
    DirControl:TMaskedDirectoryControl;
    Dir:TContainerFile;
    Fname:String;
    subDir:String;
    Procedure SetFilter(const filter:String);
    Procedure SetFName(const name:String);
    Procedure SetContainer(const container:String);
    { Private declarations }
  public
    { Public declarations }
    Property FileName:String read Fname write SetFname;
    Property Filter:String write SetFilter;
    Function Execute:boolean;
  end;

TDirPicker=class
Private
 FCaption,
 FDir:String;
Public
 Property Directory:String read FDir write FDir;
 Property Caption:String read FCaption write FCaption;
 Function Execute:boolean;
end;


var
  GetFileOpen: TGetFileOpen;

implementation
{$R *.DFM}

Function TDirPicker.Execute:boolean;
var Dir:Array[0..255] of char;
    Bi:TBrowseInfo;
    ShellFolder:IShellFolder;
begin
StrCopy(Dir,Pchar(FDir));
With Bi do
begin
 hwndOwner:=Screen.ActiveForm.Handle;
 pidlRoot:=nil;
 pszDisplayName:=Dir;
 lpszTitle:=PChar(FCaption);
 ulFlags:=BIF_RETURNONLYFSDIRS;
 lpfn:=nil;
 lParam:=0;
 iImage:=0;
end;
if ShBrowseForFolder(bi)=nil then result:=false
else
begin
 FDir:=bi.pszDisplayName;
end;
end;


procedure TGetFileOpen.FormCreate(Sender: TObject);
begin
 ClientWidth:=Label4.Left+DirList.Left+DirList.Width;
 ClientHeight:=Label4.Top+CBFilter.Top+CBFilter.Height;
 DirControl:=TMaskedDirectoryControl.CreateFromLB(DirList);
 OpenDialog.FileName:='';
end;

Procedure TGetFileOpen.SetFilter(const filter:String);
begin
 OpenDialog.Filter:=filter;
 OpenDialog.FilterIndex:=0;
 CBFilter.Filter:=Filter;
end;

Procedure TGetFileOpen.SetFname(Const name:String);
var path:String;
begin
 if IsInContainer(Name) then
 begin
  path:=ExtractPath(Name);
  If Path[length(Path)]='>' then SetLength(Path,length(path)-1);
  OpenDialog.FileName:=Path;
  EBFname.Text:=ExtractName(Name);
 end
 else OpenDialog.FileName:=Name;
 FName:=name;
 SubDir:='';
end;

Procedure TGetFileOpen.SetContainer(Const container:String);
begin
 Caption:='Files inside '+Container;
 Dir:=OpenContainer(Container);
 SubDir:='';
 DirControl.SetDir(Dir);
 DirControl.SetMask(CBFilter.Mask);
 LBContainer.Caption:=Container;
end;

Function  TGetFileOpen.Execute:boolean;
begin
 SubDir:='';
 Result:=false;
 Repeat
   result:=OpenDialog.Execute;
   CBFilter.ItemIndex:=OpenDialog.FilterIndex-1;

   if not result then exit;
  
  if IsContainer(OpenDialog.FileName) then
  begin
   SetContainer(OpenDialog.FileName);
   DirList.Sorted:=true;
   if ShowModal=mrOK then
    begin
     if SubDir='' then Fname:=OpenDialog.FileName+'>'+EBFname.Text
     else Fname:=OpenDialog.FileName+'>'+subdir+'\'+EBFname.Text;
     DirControl.SetDir(Nil);
     Dir.Free;
     SubDir:='';
     result:=true;
     exit;
    end;
    Dir.Free;
    SubDir:='';
    DirControl.SetDir(Nil);
  end
  else begin FName:=OpenDialog.FileName; exit; end;
 Until false;
end;

procedure TGetFileOpen.EBFnameChange(Sender: TObject);
var i:Integer;
begin
 i:=DirList.Items.IndexOf(EBFname.Text);
 if i<>-1 then DirList.ItemIndex:=i;
end;

procedure TGetFileOpen.DirListClick(Sender: TObject);
var TI:TFileInfo;
    i:Integer;
begin
 i:=DirList.ItemIndex;
 If i<0 then exit;
 if DirList.Items[i]='' then;
 Ti:=TFileInfo(DirList.Items.Objects[i]);
 if ti<>nil then EBFName.Text:=DirList.Items[i];
 if ti=nil then LBFileSize.Caption:='Directory' else LBFileSize.Caption:=IntToStr(ti.size);
end;

procedure TGetFileOpen.CBFilterChange(Sender: TObject);
begin
 DirControl.SetMask(CBFilter.Mask);
end;

procedure TGetFileOpen.DirListDblClick(Sender: TObject);
begin
 BNOpen.Click;
end;

procedure TGetFileOpen.BNOpenClick(Sender: TObject);
var
   ti:TFileInfo;
   i:integer;
   dname:string;
begin
 i:=DirList.ItemIndex;
 Ti:=TFileInfo(DirList.Items.Objects[i]);
 if ti=nil then
 begin
  dName:=DirList.Items[i];
  dname:=Copy(dname,2,length(dName)-2);
  if SubDir='' then SubDir:=dname else SubDir:=Subdir+'\'+dname;
  Dir.ChDir(subDir);
  DirControl.SetMask(CBFilter.Mask);
  exit;
 end;

 If Dir.ListFiles.IndexOf(EBFname.Text)=-1 then
 MsgBox('The file '+EBFname.Text+' is not in the container','Error',mb_ok)
 else begin ModalResult:=mrOk; Hide; end;
 begin

 end;

end;

procedure TGetFileOpen.BNDirUpClick(Sender: TObject);
var ps:Pchar;
begin
 if SubDir='' then exit;
 ps:=StrRScan(@SubDir[1],'\');
 if ps=nil then SubDir:='' else
 begin
  SubDir:=Copy(Subdir,1,ps-@Subdir[1]);
 end;
  Dir.ChDir(SubDir);
  DirControl.SetMask(CBFilter.Mask);
end;

procedure TGetFileOpen.FormKeyDown(Sender: TObject; var Key: Word;
  Shift: TShiftState);
begin
   if Key=VK_BACK then BNDirUp.CLick;
end;

end.
