unit U_msgForm;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  StdCtrls, Menus;

type
  TMsgForm = class(TForm)
    msgs: TMemo;
    MainMenu1: TMainMenu;
    File1: TMenuItem;
    Clear1: TMenuItem;
    Close1: TMenuItem;
    procedure Clear1Click(Sender: TObject);
    procedure Close1Click(Sender: TObject);
  private
    { Private declarations }
  public
    { Public declarations }
    Procedure AddMessage(const msg: String);
  end;

var
  MsgForm: TMsgForm;

implementation

uses GlobalVars;
{$R *.DFM}

Procedure TMsgForm.AddMessage(const msg: String);
var
  n: Integer;
begin
  n := msgs.Lines.Count;
  if n >= MaxMsgs then
    msgs.Lines.Delete(n - 1);
  msgs.Lines.Insert(0, msg);
end;

procedure TMsgForm.Clear1Click(Sender: TObject);
begin
  msgs.Lines.Clear;
end;

procedure TMsgForm.Close1Click(Sender: TObject);
begin
  Hide;
end;

end.
