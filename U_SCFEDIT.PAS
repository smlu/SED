unit U_SCFEdit;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  StdCtrls, ExtCtrls;

type
  TSCFieldPicker = class(TForm)
    Panel1: TPanel;
    RGItems: TRadioGroup;
    BNOK: TButton;
    BNCancel: TButton;
  private
    { Private declarations }
    procedure SetCaptions(const name:string);
    Procedure ClearItems;
    Procedure AddItem(const name:string);
  public
    { Public declarations }
    Function PickGeo(geo:integer):integer;
    Function PickLightMode(lmode:integer):integer;
    Function PickTEX(tex:integer):integer;
  end;

var
  SCFieldPicker: TSCFieldPicker;

implementation

{$R *.DFM}

procedure TSCFieldPicker.SetCaptions(const name:string);
begin
 Caption:=name;
 RGItems.Caption:=name;
end;

Procedure TSCFieldPicker.ClearItems;
begin
 RGItems.Items.Clear;
end;

Procedure TSCFieldPicker.AddItem(const name:string);
begin
 RGItems.Items.Add(name);
end;

Function TSCFieldPicker.PickGeo(geo:integer):integer;
begin
 SetCaptions('GEO mode');
 if geo>5 then geo:=5;
 ClearItems;
 AddItem('Not rendered');
 AddItem('Draw vertices');
 AddItem('Draw wireframe');
 AddItem('Draw solid color');
 AddItem('Draw textured');
 AddItem('Draw textured');
 RGItems.ItemIndex:=geo;
 if ShowModal=mrOk then result:=RGItems.ItemIndex else result:=geo;
end;

Function TSCFieldPicker.PickLightMode(lmode:integer):integer;
begin
 SetCaptions('Lighting mode');
 if lmode>5 then lmode:=5;
 ClearItems;
 AddItem('Fully lit');
 AddItem('Not lit');
 AddItem('Diffuse');
 AddItem('Gouraud');
 AddItem('Gouraud');
 AddItem('Gouraud');
 RGItems.ItemIndex:=lmode;
 if ShowModal=mrOk then result:=RGItems.ItemIndex else result:=lmode;
end;

{SurfaceTypes
NORMAL			0x0	Normal face (one sided no overlays/masks)
TWOSIDED		0x1	Face is 2 sided.
TRANSLUCENT		0x2	Face is translucent"}

Function TSCFieldPicker.PickTEX(tex:integer):integer;
begin
 SetCaptions('Texturing mode');
 if tex>3 then tex:=3;
 ClearItems;
 AddItem('Affine');
 AddItem('Perspective');
 AddItem('Perspective');
 AddItem('Perspective');
 RGItems.ItemIndex:=tex;
 if ShowModal=mrOk then result:=RGItems.ItemIndex else result:=tex;
end;

end.
