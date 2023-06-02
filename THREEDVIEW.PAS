unit ThreeDView;

{This unit the form for 3D Viewing of JK level}

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  ExtCtrls, GL, GLU, StdCtrls, ComCtrls, Buttons, tmp_load3do, J_Level;

Const
     id_translate=0;
     id_rotate=1;
     id_Vertical=0;
     id_Horizontal=1;
type
  TMDI_3DView = class(TForm)
    BNUp: TBitBtn;
    BNDown: TBitBtn;
    BNRight: TBitBtn;
    BNLeft: TBitBtn;
    CBWhatMove: TComboBox;
    CBAxis: TComboBox;
    procedure FormShow(Sender: TObject);
  private
    hdc:Thandle;
    hglc:Thandle;
    The3DO:T3DO;
    SelectedTriangle:integer;
    pixelFormat:Integer;
    pfd : TPIXELFORMATDESCRIPTOR;
    ArrowsRotate:boolean;
    AxisVertical:boolean;
    { Private declarations }

  public
    { Public declarations }
  end;

var
  MDI_3DView: TMDI_3DView;

implementation

{$R *.DFM}

procedure TMDI_3DView.FormShow(Sender: TObject);
var Jkl:TJKLevel;
begin
 JKL.LoadFromJKL('a.jkl');
end;

end.
