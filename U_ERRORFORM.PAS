unit u_errorform;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  StdCtrls, ExtCtrls, GlobalVars;

type
  TErrForm = class(TForm)
    Panel1: TPanel;
    BNCopy: TButton;
    MMError: TMemo;
    BNSave: TButton;
    BNClose: TButton;
    procedure BNCloseClick(Sender: TObject);
    procedure BNCopyClick(Sender: TObject);
    procedure BNSaveClick(Sender: TObject);
  private
    { Private declarations }
  public
   Procedure ReportError(exObj:Exception;adr:pointer);
    { Public declarations }
  end;

var
  ErrForm: TErrForm;

implementation

{$R *.DFM}

procedure TErrForm.BNCloseClick(Sender: TObject);
begin
 Hide;
end;

procedure TErrForm.BNCopyClick(Sender: TObject);
begin
 MMError.SelectAll;
 MMError.CopyToClipboard;
end;

procedure TErrForm.BNSaveClick(Sender: TObject);
begin
try
 MMError.Lines.SaveToFile(BaseDir+'Jed.log');
except
 on Exception do ;
end;
end;

Procedure TErrForm.ReportError(exObj:Exception;adr:pointer);
type
    Tstk=array[0..35,0..7] of Integer;
var stk,s:string;
    ps:^Tstk;
    i,j:integer;

begin
 asm
  mov ps,ebp
 end;

{ dec(Pchar(ps),372);}

 stk:='';
Try
 For i:=0 to 35 do
 begin
 stk:=stk+#13#10;
 For j:=0 to 7 do
  stk:=stk+IntToHex(ps^[i,j],8)+' ';
 end;

except
 On Exception do;
end;

 {if adr<>nil then dec(pchar(adr),Longint(@TextStart)-616);}

 s:=Format('%s: %s at %p'#13#10#13#10'Jed %s'#13#10'D3DDevice %s'#13#10'P3DAPI %d'#13#10'WFAPI %d'#13#10'GameDir %s'#13#10'BaseDir %s'#13#10'Base %x'#13#10'Stack: %s',
 [exObj.ClassName,exObj.message,adr,JedVersion,D3DDevice,P3DAPI,WireframeAPI,GameDir,BaseDir,Longint(@TextStart)-640,stk]);


 if (self=nil) or (MMError=nil) then ShowMessage(s) else
 begin
  MMError.Lines.Text:=s;
  if not visible then show;
 end;
end;

end.
