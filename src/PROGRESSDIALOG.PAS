unit ProgressDialog;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  StdCtrls, ComCtrls, Math;

Const
  PD_TooLong = 2000; { the number of Msec after which
    progress dialog will appear. Anything that
    takes less time won't bring up a dialog }

type
  TProgress = class(TForm)
    ProgressBar: TProgressBar;
    Pmsg: TLabel;
    procedure FormHide(Sender: TObject);

  private
    sec, msec: word;
    shown: boolean;
    Lpos: Longint;
    lMax: Longint;
    LargeMax, bAppActive: boolean;
    { Private declarations }
    procedure SetMsg(const s: String);
    Function ItIsTime: boolean;
    procedure WMActivateApp(var Msg: TWMActivateApp); message WM_ACTIVATEAPP;

  public
    Procedure Reset(steps: Integer);
    Procedure Step;
    Procedure StepBy(steps: Integer);
    Property Msg: String write SetMsg;
  end;

var
  Progress: TProgress;

implementation
uses System.Math;

{$R *.DFM}

procedure TProgress.SetMsg(const s: String);
begin
  Caption := 'Progress - ' + s;
  { Pmsg.Caption:=s;
    Update; }
end;

function TProgress.ItIsTime: boolean;
var
  nhr, nmin, nsec, nmsec: word;
  minpassed, secpassed, msecpassed: Integer;
begin
  DecodeTime(Time, nhr, nmin, nsec, nmsec);
  secpassed := nsec - sec;
  if secpassed < 0 then
    Inc(secpassed, 60);
  msecpassed := nmsec - msec;
  if msecpassed < 0 then
    Inc(msecpassed, 1000);
  Result := secpassed * 1000 + msecpassed > PD_TooLong;
end;

procedure TProgress.WMActivateApp(var Msg: TWMActivateApp);
begin
  bAppActive :=  Msg.Active;
end;

procedure TProgress.Reset(steps: Integer);
var
  hr, min: word;
begin
  ProgressBar.min := 0;
  ProgressBar.Step := 1;
  ProgressBar.Position := 0;
  if steps <= 5000 then
  begin
    ProgressBar.Max := System.Math.Max(steps - 1, 0);
    LargeMax := false;
  end
  else
  begin
    ProgressBar.Max := 300;
    lMax := steps;
    LargeMax := true;
  end;
  Lpos := 0;
  DecodeTime(Time, hr, min, sec, msec);
  shown := false;
  Hide;
end;

Procedure TProgress.Step;
begin
  if not shown then
    if ItIsTime then
    begin
      if bAppActive then
        Show
      else
        begin
          ShowWindow(Handle, SW_SHOWNOACTIVATE);
          Visible := True;
        end;

      shown := true;
    end;
  ProgressBar.StepIt;
end;

Procedure TProgress.StepBy(steps: Integer);
var
  Npos: Integer;
begin
  if not shown then
    if ItIsTime then
    begin
      if bAppActive then
        Show
      else
        begin
          ShowWindow(Handle, SW_SHOWNOACTIVATE);
          Visible := True;
        end;
      shown := true;
    end;
  if LargeMax then
  begin
    Inc(Lpos, steps);
    Npos := Round(Lpos / lMax * 300);
    if Npos <> ProgressBar.Position then
      ProgressBar.Position := Npos;
  end
  else
    ProgressBar.Position := ProgressBar.Position + steps;
end;

procedure TProgress.FormHide(Sender: TObject);
begin
  Msg := '';
end;

end.
