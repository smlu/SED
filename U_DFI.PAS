unit u_DFI;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  StdCtrls, ExtCtrls, FieldEdit;

type
  TDFImport = class(TForm)
    RGTX: TRadioGroup;
    EBScale: TEdit;
    Label1: TLabel;
    BNProceed: TButton;
    BNCancel: TButton;
    procedure FormCreate(Sender: TObject);
  private
    { Private declarations }
  public
    { Public declarations }
    vScale:TValInput;
  end;

var
  DFImport: TDFImport;

implementation

{$R *.DFM}

procedure TDFImport.FormCreate(Sender: TObject);
begin
 ClientWidth:=RGTX.Left+BNCancel.Left+BNCancel.Width;
 ClientHeight:=RGTX.Left+BNCancel.top+BNCancel.Height;
 vScale:=TValInput.Create(EBScale);
 vScale.SetAsFloat(40);
end;

end.
