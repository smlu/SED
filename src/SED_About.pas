unit SED_About;

interface

uses Windows, SysUtils, Classes, Graphics, Forms, Controls, StdCtrls,
  Buttons, ExtCtrls, SHellAPI, GlobalVars;

type
  TSedAbout = class(TForm)
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
  sedAbout: TSedAbout;

implementation
 uses Clipbrd;

{$R *.DFM}

procedure TSedAbout.OKButtonClick(Sender: TObject);
begin
  sedAbout.Hide;
end;

procedure TSedAbout.LBHomePageUrlMouseEnter(Sender: TObject);
begin
  LBHomePageUrl.Font.Color := $00FF0080;
  Screen.Cursor := crHandPoint;
end;

procedure TSedAbout.LBHomePageUrlMouseLeave(Sender: TObject);
begin
  LBHomePageUrl.Font.Color := clBlue;
  Screen.Cursor := crDefault;
end;

procedure TSedAbout.LBHomePageUrlMouseUp(Sender: TObject; Button: TMouseButton;
  Shift: TShiftState; X, Y: Integer);
begin
  case Button of
    TMouseButton.mbLeft: ShellExecute(Handle, 'open', 'https://github.com/smlu/sed', nil, nil, SW_SHOWNORMAL);
    TMouseButton.mbRight: Clipboard.AsText := LBHomePageUrl.Caption;
    TMouseButton.mbMiddle: Clipboard.AsText := LBHomePageUrl.Caption;
  end;

end;

Procedure TSedAbout.ScrollStep;
begin
  ScrollBox.ScrollBy(0, -1);
  Dec(scrollPos);
end;

Procedure TSedAbout.ScrollTo(pos: integer);
begin
  ScrollBox.ScrollBy(0, pos - scrollPos);
  scrollPos := pos;
end;

procedure TSedAbout.TimerTimer(Sender: TObject);
begin
  { if cnt=0 then ScrollBy(-10,0) else ScrollBy(10,0);
    cnt:=cnt xor 1; }
  ScrollStep;
  if (-scrollPos) > Label1.Height + 5 then
    ScrollTo(ScrollBox.Height);
end;

procedure TSedAbout.FormShow(Sender: TObject);
begin
  //Timer.Enabled := true;
  //Label1.Caption := Memo.Lines.Text;
  LabelMemo.Caption := Memo.Lines.Text;
  LabelMemo.AutoSize := true;
  LabelMemo.Width := 463;
  Label1.AutoSize := true;
  LBVersion.Caption := 'Sith Editor v' + SedVersion;
end;

procedure TSedAbout.FormHide(Sender: TObject);
begin
  Timer.Enabled := false;
end;

end.
