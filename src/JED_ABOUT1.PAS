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
    Timer: TTimer;
    Memo: TMemo;
    ScrollBox: TScrollBox;
    Label1: TLabel;
    ScrollBox1: TScrollBox;
    Label2: TLabel;
    LabelMemo: TLabel;
    LBHomePageUrl: TLabel;
    procedure OKButtonClick(Sender: TObject);
    procedure TimerTimer(Sender: TObject);
    procedure FormShow(Sender: TObject);
    procedure FormHide(Sender: TObject);
    procedure LBHomePageUrlMouseEnter(Sender: TObject);
    procedure LBHomePageUrlMouseLeave(Sender: TObject);
    procedure LBHomePageUrlMouseUp(Sender: TObject; Button: TMouseButton;
      Shift: TShiftState; X, Y: Integer);
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
 uses Clipbrd;

{$R *.DFM}

procedure TJed_about.OKButtonClick(Sender: TObject);
begin
  Jed_about.Hide;
end;

procedure TJed_about.LBHomePageUrlMouseEnter(Sender: TObject);
begin
  LBHomePageUrl.Font.Color := $00FF0080;
  Screen.Cursor := crHandPoint;
end;

procedure TJed_about.LBHomePageUrlMouseLeave(Sender: TObject);
begin
  LBHomePageUrl.Font.Color := clBlue;
  Screen.Cursor := crDefault;
end;

procedure TJed_about.LBHomePageUrlMouseUp(Sender: TObject; Button: TMouseButton;
  Shift: TShiftState; X, Y: Integer);
begin
  case Button of
    TMouseButton.mbLeft: ShellExecute(Handle, 'open', 'https://github.com/smlu/sed', nil, nil, SW_SHOWNORMAL);
    TMouseButton.mbRight: Clipboard.AsText := LBHomePageUrl.Caption;
    TMouseButton.mbMiddle: Clipboard.AsText := LBHomePageUrl.Caption;
  end;

end;

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
  //Timer.Enabled := true;
  //Label1.Caption := Memo.Lines.Text;
  LabelMemo.Caption := Memo.Lines.Text;
  LabelMemo.AutoSize := true;
  LabelMemo.Width := 463;
  Label1.AutoSize := true;
  LBVersion.Caption := 'Sith Editor v' + SedVersion;
end;

procedure TJed_about.FormHide(Sender: TObject);
begin
  Timer.Enabled := false;
end;

end.
