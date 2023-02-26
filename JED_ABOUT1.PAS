unit Jed_about1;

interface

uses Windows, SysUtils, Classes, Graphics, Forms, Controls, StdCtrls,
  Buttons, ExtCtrls, SHellAPI, GlobalVars;

type
  TJed_about = class(TForm)
    Panel1: TPanel;
    ProgramIcon: TImage;
    LBVersion: TLabel;
    OKButton: TButton;
    JEDHome: TButton;
    Timer: TTimer;
    Memo: TMemo;
    ScrollBox: TScrollBox;
    Label1: TLabel;
    procedure OKButtonClick(Sender: TObject);
    procedure JEDHomeClick(Sender: TObject);
    procedure TimerTimer(Sender: TObject);
    procedure FormShow(Sender: TObject);
    procedure FormHide(Sender: TObject);
  private
    { Private declarations }
    scrollPos: integer;
    Procedure ScrollStep;
    Procedure ScrollTo(pos: integer);
  public
    { Public declarations }
  end;

var
  Jed_about: TJed_about;

implementation

{$R *.DFM}

procedure TJed_about.OKButtonClick(Sender: TObject);
begin
  Jed_about.Hide;
end;

procedure TJed_about.JEDHomeClick(Sender: TObject);
begin
  if ShellExecute(Handle, 'open', 'http://www.code-alliance.com/', nil, nil,
    0) < 32 then
  begin
    MessageBeep(0);
  end;
End;

Procedure TJed_about.ScrollStep;
begin
  ScrollBox.ScrollBy(0, -1);
  Dec(scrollPos);
end;

Procedure TJed_about.ScrollTo(pos: integer);
begin
  ScrollBox.ScrollBy(0, pos - scrollPos);
  scrollPos := pos;
end;

procedure TJed_about.TimerTimer(Sender: TObject);
begin
  { if cnt=0 then ScrollBy(-10,0) else ScrollBy(10,0);
    cnt:=cnt xor 1; }
  ScrollStep;
  if (-scrollPos) > Label1.Height + 5 then
    ScrollTo(ScrollBox.Height);
end;

procedure TJed_about.FormShow(Sender: TObject);
begin
  Timer.Enabled := true;
  Label1.Caption := Memo.Lines.Text;
  Label1.AutoSize := true;
  LBVersion.Caption := 'JED Version ' + JedVersion;
end;

procedure TJed_about.FormHide(Sender: TObject);
begin
  Timer.Enabled := false;
end;

end.
