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
    procedure FormKeyDown(Sender: TObject; var Key: Word; Shift: TShiftState);
  private
    DirControl: TMaskedDirectoryControl;
    Dir: TContainerFile;
    Fname: String;
    subDir: String;
    Procedure SetFilter(const filter: String);
    Procedure SetFName(const name: String);
    Procedure SetContainer(const container: String);
    { Private declarations }
  public
    { Public declarations }
    Property FileName: String read Fname write SetFName;
    Property filter: String write SetFilter;
    Function Execute: boolean;
  end;

  TDirPicker = class
  Private
    FCaption, FDir: String;
  Public
    Property Directory: String read FDir write FDir;
    Property Caption: String read FCaption write FCaption;
    Function Execute: boolean;
  end;

var
  GetFileOpen: TGetFileOpen;

implementation

{$R *.DFM}

Function TDirPicker.Execute: boolean;
var
  Dir: Array [0 .. 255] of char;
  Bi: TBrowseInfo;
  ShellFolder: IShellFolder;
begin
  StrCopy(Dir, Pchar(FDir));
  With Bi do
  begin
    hwndOwner := Screen.ActiveForm.Handle;
    pidlRoot := nil;
    pszDisplayName := Dir;
    lpszTitle := Pchar(FCaption);
    ulFlags := BIF_RETURNONLYFSDIRS;
    lpfn := nil;
    lParam := 0;
    iImage := 0;
  end;
  if ShBrowseForFolder(Bi) = nil then
    result := false
  else
  begin
    FDir := Bi.pszDisplayName;
  end;
end;

procedure TGetFileOpen.FormCreate(Sender: TObject);
begin
  ClientWidth := Label4.Left + DirList.Left + DirList.Width;
  ClientHeight := Label4.Top + CBFilter.Top + CBFilter.Height;
  DirControl := TMaskedDirectoryControl.CreateFromLB(DirList);
  OpenDialog.FileName := '';
end;

Procedure TGetFileOpen.SetFilter(const filter: String);
begin
  OpenDialog.filter := filter;
  OpenDialog.FilterIndex := 0;
  CBFilter.filter := filter;
end;

Procedure TGetFileOpen.SetFName(Const name: String);
var
  path: String;
begin
  if IsInContainer(Name) then
  begin
    path := ExtractPath(Name);
    If path[length(path)] = '>' then
      SetLength(path, length(path) - 1);
    OpenDialog.FileName := path;
    EBFname.Text := ExtractName(Name);
  end
  else
    OpenDialog.FileName := Name;
  Fname := name;
  subDir := '';
end;

Procedure TGetFileOpen.SetContainer(Const container: String);
begin
  Caption := 'Files inside ' + container;
  Dir := OpenContainer(container);
  subDir := '';
  DirControl.SetDir(Dir);
  DirControl.SetMask(CBFilter.Mask);
  LBContainer.Caption := container;
end;

Function TGetFileOpen.Execute: boolean;
begin
  subDir := '';
  result := false;
  Repeat
    result := OpenDialog.Execute;
    CBFilter.ItemIndex := OpenDialog.FilterIndex - 1;

    if not result then
      exit;

    if IsContainer(OpenDialog.FileName) then
    begin
      SetContainer(OpenDialog.FileName);
      DirList.Sorted := true;
      if ShowModal = mrOK then
      begin
        if subDir = '' then
          Fname := OpenDialog.FileName + '>' + EBFname.Text
        else
          Fname := OpenDialog.FileName + '>' + subDir + '\' + EBFname.Text;
        DirControl.SetDir(Nil);
        Dir.Free;
        subDir := '';
        result := true;
        exit;
      end;
      Dir.Free;
      subDir := '';
      DirControl.SetDir(Nil);
    end
    else
    begin
      Fname := OpenDialog.FileName;
      exit;
    end;
  Until false;
end;

procedure TGetFileOpen.EBFnameChange(Sender: TObject);
var
  i: Integer;
begin
  i := DirList.Items.IndexOf(EBFname.Text);
  if i <> -1 then
    DirList.ItemIndex := i;
end;

procedure TGetFileOpen.DirListClick(Sender: TObject);
var
  TI: TFileInfo;
  i: Integer;
begin
  i := DirList.ItemIndex;
  If i < 0 then
    exit;
  if DirList.Items[i] = '' then;
  TI := TFileInfo(DirList.Items.Objects[i]);
  if TI <> nil then
    EBFname.Text := DirList.Items[i];
  if TI = nil then
    LBFileSize.Caption := 'Directory'
  else
    LBFileSize.Caption := IntToStr(TI.size);
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
  TI: TFileInfo;
  i: Integer;
  dname: string;
begin
  i := DirList.ItemIndex;
  TI := TFileInfo(DirList.Items.Objects[i]);
  if TI = nil then
  begin
    dname := DirList.Items[i];
    dname := Copy(dname, 2, length(dname) - 2);
    if subDir = '' then
      subDir := dname
    else
      subDir := subDir + '\' + dname;
    Dir.ChDir(subDir);
    DirControl.SetMask(CBFilter.Mask);
    exit;
  end;

  If Dir.ListFiles.IndexOf(EBFname.Text) = -1 then
    MsgBox('The file ' + EBFname.Text + ' is not in the container',
      'Error', mb_ok)
  else
  begin
    ModalResult := mrOK;
    Hide;
  end;
  begin

  end;

end;

procedure TGetFileOpen.BNDirUpClick(Sender: TObject);
var
  ps: Pchar;
begin
  if subDir = '' then
    exit;
  ps := StrRScan(@subDir[1], '\');
  if ps = nil then
    subDir := ''
  else
  begin
    subDir := Copy(subDir, 1, ps - @subDir[1]);
  end;
  Dir.ChDir(subDir);
  DirControl.SetMask(CBFilter.Mask);
end;

procedure TGetFileOpen.FormKeyDown(Sender: TObject; var Key: Word;
  Shift: TShiftState);
begin
  if Key = VK_BACK then
    BNDirUp.Click;
end;

end.
