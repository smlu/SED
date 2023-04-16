unit Q_Utils;

interface

uses Geometry, J_Level, SysUtils, StdCtrls, Misc_Utils, Windows;

Type
  TCompAction = (ca_None, ca_Equal, ca_NotEqual, ca_Above, ca_Below, ca_In,
    ca_NotIn);

  TQInt = record
    V: Integer;
    act: TCompAction;
  end;

  TQString = record
    V: String;
    act: TCompAction;
  end;

  TQFlags = record
    V: Longint;
    act: TCompAction;
  end;

  TQDouble = record
    V: Double;
    act: TCompAction;
  end;

  TQColor = record
    V: TColorF;
    act: TCompAction;
  end;

  TSectorFindInfo = class
    num: TQInt;
    NSurfs: TQInt;
    Flags: TQFlags;
    extra_l: TQColor;
    ColorMap: TQString;
    tint: TQColor;
    sound: TQString;
    sound_vol: TQDouble;
    Layer: TQString;
  end;

  TSurfFindInfo = class
    num: TQInt;
    material: TQString;
    AdjoinSC: TQInt;
    AdjoinSF: TQInt;
    AdjoinFlags: TQFlags;
    SurfFlags: TQFlags;
    FaceFlags: TQFlags;
    geo, light, tex: TQInt;
    extra_l: TQColor;
    Layer: TQString;
  end;

  TThingFindInfo = class
    num: TQInt;
    name: TQString;
    sec: TQInt;
    X, Y, Z: TQDouble;
    PCH, YAW, ROL: TQDouble;
    Layer: TQString;
  end;

  TQueryField = class
    CB: TComboBox;
    EB: TEdit;
    vType: Integer;
    P: Pointer;
    Constructor Create(aCB: TComboBox; aEB: TEdit);
    Constructor CreateHex(aCB: TComboBox; aEB: TEdit; var H: TQInt);
    Constructor CreateStr(aCB: TComboBox; aEB: TEdit; var S: TQString);
    Constructor CreateInt(aCB: TComboBox; aEB: TEdit; var I: TQInt);
    Constructor CreateDouble(aCB: TComboBox; aEB: TEdit; var D: TQDouble);
    Constructor CreateFlags(aCB: TComboBox; aEB: TEdit; var F: TQFlags);
    Constructor CreateColor(aCB: TComboBox; aEB: TEdit; var c: TQColor);
    procedure EBChangeHandler(Sender: TObject);
    Procedure CBChangeHandler(Sender: TObject);
  end;

Function TestStr(const s1, s2: String; action: TCompAction): Boolean;
Function TestDouble(d1, d2: Double; action: TCompAction): Boolean;
Function TestFlags(f1, f2: Longint; action: TCompAction): Boolean;
Function TestInt(const f1, f2: Integer; action: TCompAction): Boolean;
Function TestColor(c1, c2: TColorF; action: TCompAction): Boolean;

Function SectorMatches(S: TJKSector; fi: TSectorFindInfo): Boolean;
Function SurfMatches(W: TJKSurface; fi: TSurfFindInfo): Boolean;
Function ThingMatches(O: TJKThing; fi: TThingFindInfo): Boolean;

Function FindNextSector(L: TJKLevel; LastSC: Integer;
  fi: TSectorFindInfo): Integer;
Function FindNextSurf(L: TJKLevel; LastSC, LastSF: Integer; fi: TSurfFindInfo;
  var NewSC: Integer): Integer;
Function FindNextThing(L: TJKLevel; LastTH: Integer;
  fi: TThingFindInfo): Integer;

implementation

uses FieldEdit;

const
  vtInt = 1;
  vtStr = 2;
  vtDouble = 3;
  vtFlags = 4;
  vtHex = 5;
  vtColor = 6;

Constructor TQueryField.Create(aCB: TComboBox; aEB: TEdit);
begin
  CB := aCB;
  EB := aEB;
  EB.OnChange := EBChangeHandler;
  CB.OnChange := CBChangeHandler;
end;

Constructor TQueryField.CreateStr(aCB: TComboBox; aEB: TEdit; var S: TQString);
begin
  Create(aCB, aEB);
  P := @S;
  vType := vtStr;
end;

Constructor TQueryField.CreateHex(aCB: TComboBox; aEB: TEdit; var H: TQInt);
begin
  Create(aCB, aEB);
  P := @H;
  vType := vtHex;
end;

Constructor TQueryField.CreateInt(aCB: TComboBox; aEB: TEdit; var I: TQInt);
begin
  Create(aCB, aEB);
  P := @I;
  vType := vtInt;
end;

Constructor TQueryField.CreateDouble(aCB: TComboBox; aEB: TEdit;
  var D: TQDouble);
begin
  Create(aCB, aEB);
  P := @D;
  vType := vtDouble;
end;

Constructor TQueryField.CreateFlags(aCB: TComboBox; aEB: TEdit; var F: TQFlags);
begin
  Create(aCB, aEB);
  P := @F;
  vType := vtFlags;
end;

Constructor TQueryField.CreateColor(aCB: TComboBox; aEB: TEdit; var c: TQColor);
begin
  Create(aCB, aEB);
  P := @c;
  vType := vtColor;
end;


procedure TQueryField.EBChangeHandler(Sender: TObject);
var
  S: String;
  a: Integer;
  L: Longint;
  I: Integer;
  D: Double;
  C: TColorF;
begin
  S := EB.Text;
  If CB.ItemIndex <= 0 then
  begin
    CB.ItemIndex := 1;
    CB.OnChange(nil);
  end;
  Case vType of
    vtStr:
      TQString(P^).V := S;
    vtInt:
      With TQInt(P^) do
        if ValInt(S, I) then
          V := I
        else
        begin
          if S = '-' then
            exit;
          MsgBox(S + ' is not a valid integer', 'Error', mb_ok);
          EB.Text := IntToStr(V);
        end;
    vtDouble:
      With TQDouble(P^) do
        if feValDouble(S, D) then
          V := D
        else
        begin
          EB.Text := FloatToStr(V, TFormatSettings.Invariant);
          MsgBox(S + ' is not a valid double', 'Error', mb_ok);
        end;
    vtFlags:
      With TQInt(P^) do
        if ValDword(S, L) then
          V := L
        else
        begin
          MsgBox(S + ' is not a valid dword', 'Error', mb_ok);
          EB.Text := DwordToStr(V);
        end;
    vtHex:
      With TQInt(P^) do
        if ValHex(S, L) then
          V := L
        else
        begin
          MsgBox(S + ' is not a valid hex', 'Error', mb_ok);
          EB.Text := Format('%x', [V]);
        end;
    vtColor:
      With TQColor(P^) do
        if feValColor(S, C) then
          V := C
        else
        begin
          EB.Text := ColorToStr(V);
          MsgBox(S + ' is not a valid color', 'Error', mb_ok);
        end;
  end;
end;

procedure TQueryField.CBChangeHandler(Sender: TObject);
var
  S: String;
  act: TCompAction;
begin
  S := UpperCase(CB.Text);
  act := ca_None;

  if S = '' then
    act := ca_None
  else if S = '=' then
    act := ca_Equal
  else if S = '>' then
    act := ca_Above
  else if S = '<' then
    act := ca_Below
  else if S = '<>' then
    act := ca_NotEqual
  else if S = 'IS' then
    act := ca_Equal
  else if S = 'IS NOT' then
    act := ca_NotEqual
  else if S[1] in ['S', 'C'] then
    act := ca_In { 'Set' or 'Contains' }
  else if S[1] in ['N', 'D'] then
    act := ca_NotIn; { 'Not set' or 'Doesn't contain' }

  case vType of
    vtStr:
      TQString(P^).act := act;
    vtInt, vtHex:
      TQInt(P^).act := act;
    vtDouble:
      TQDouble(P^).act := act;
    vtFlags:
      TQFlags(P^).act := act;
    vtColor:
      TQColor(P^).act := act;
  end;
end;

Function QTestInt(const qi: TQInt; I: Integer): Boolean;
begin
  Result := TestInt(I, qi.V, qi.act);
end;

Function QTestStr(const qi: TQString; S: String): Boolean;
begin
  Result := TestStr(qi.V, S, qi.act);
end;

Function QTestFlags(const qi: TQFlags; F: Longint): Boolean;
begin
  Result := TestFlags(F, qi.V, qi.act);
end;

Function QTestDouble(const qi: TQDouble; D: Double): Boolean;
begin
  Result := TestDouble(D, qi.V, qi.act);
end;

Function QTestColor(const qi: TQColor; c: TColorF): Boolean;
begin
  Result := TestColor(c, qi.V, qi.act);
end;

Function SectorMatches(S: TJKSector; fi: TSectorFindInfo): Boolean;
begin
  With fi do
    Result := QTestInt(num, S.num) and QTestInt(NSurfs, S.surfaces.count) and
      QTestFlags(Flags, S.Flags) and QTestColor(extra_l, S.ExtraLight) and
      QTestStr(ColorMap, S.ColorMap) and QTestColor(tint, S.Tint) and
      QTestStr(sound, S.sound) and QTestDouble(sound_vol, S.snd_vol) and
      QTestStr(Layer, S.Level.GetLayerName(S.Layer));
end;

Function SurfMatches(W: TJKSurface; fi: TSurfFindInfo): Boolean;

  function getSCnum(S: TJKSurface): Integer;
  begin
    if S = nil then
      Result := -1
    else
      Result := S.sector.num;
  end;

  function getSFnum(S: TJKSurface): Integer;
  begin
    if S = nil then
      Result := -1
    else
      Result := S.num;
  end;

begin
  With fi do
    Result := QTestInt(num, W.num) and QTestStr(material, W.material) and
      QTestInt(AdjoinSC, getSCnum(W.adjoin)) and
      QTestInt(AdjoinSF, getSFnum(W.adjoin)) and
      QTestFlags(AdjoinFlags, W.AdjoinFlags) and
      QTestFlags(SurfFlags, W.SurfFlags) and QTestFlags(FaceFlags, W.FaceFlags)
      and QTestInt(geo, W.geo) and QTestInt(light, W.light) and
      QTestInt(tex, W.tex) and QTestColor(extra_l, W.ExtraLight) and
      QTestStr(Layer, W.sector.Level.GetLayerName(W.sector.Layer));

end;

Function ThingMatches(O: TJKThing; fi: TThingFindInfo): Boolean;

  function getSCnum(S: TJKSector): Integer;
  begin
    if S = nil then
      Result := -1
    else
      Result := S.num;
  end;

begin
  With fi do
    Result := QTestInt(num, O.num) and QTestStr(name, O.name) and
      QTestInt(sec, getSCnum(O.sec)) and QTestDouble(X, O.X) and
      QTestDouble(Y, O.Y) and QTestDouble(Z, O.Z) and QTestDouble(PCH, O.PCH)
      and QTestDouble(YAW, O.YAW) and QTestDouble(ROL, O.ROL) and
      QTestStr(Layer, O.Level.GetLayerName(O.Layer));
end;

Function FindNextSector(L: TJKLevel; LastSC: Integer;
  fi: TSectorFindInfo): Integer;
var
  S: Integer;
begin
  Result := -1;
  For S := LastSC + 1 to L.Sectors.count - 1 do
    if SectorMatches(L.Sectors[S], fi) Then
    begin
      Result := S;
      exit;
    end;
end;

Function FindNextSurf(L: TJKLevel; LastSC, LastSF: Integer; fi: TSurfFindInfo;
  var NewSC: Integer): Integer;
var
  S, W: Integer;
begin
  Result := -1;
  NewSC := -1;

  for S := LastSC to L.Sectors.count - 1 do
    With L.Sectors[S] do
    begin
      for W := LastSF + 1 to surfaces.count - 1 do
      begin
        if SurfMatches(surfaces[W], fi) then
        begin
          NewSC := S;
          Result := W;
          exit;
        end;
      end;
      LastSF := -1;
    end;
end;

Function FindNextThing(L: TJKLevel; LastTH: Integer;
  fi: TThingFindInfo): Integer;
var
  O: Integer;
begin
  Result := -1;
  for O := LastTH + 1 to L.things.count - 1 do
  begin
    if ThingMatches(L.things[O], fi) then
    begin
      Result := O;
      exit;
    end;
  end;
end;

Function TestStr(const s1, s2: String; action: TCompAction): Boolean;
begin
  Case action of
    ca_None:
      Result := true;
    ca_Equal:
      Result := CompareText(s1, s2) = 0;
    ca_NotEqual:
      Result := CompareText(s1, s2) <> 0;
    ca_Above:
      Result := CompareText(s1, s2) > 0;
    ca_Below:
      Result := CompareText(s1, s2) < 0;
    ca_In:
      Result := Pos(UpperCase(s1), UpperCase(s2)) <> 0;
    ca_NotIn:
      Result := Pos(UpperCase(s1), UpperCase(s2)) = 0;
  else
    Result := false;
  end;
end;

Function TestDouble(d1, d2: Double; action: TCompAction): Boolean;
begin
  Case action of
    ca_None:
      Result := true;
    ca_Equal:
      Result := d1 = d2;
    ca_NotEqual:
      Result := d1 <> d2;
    ca_Above:
      Result := d1 > d2;
    ca_Below:
      Result := d1 < d2;
  else
    Result := false;
  end;
end;

Function TestFlags(f1, f2: Longint; action: TCompAction): Boolean;
begin
  Case action of
    ca_None:
      Result := true;
    ca_Equal:
      Result := f1 = f2;
    ca_NotEqual:
      Result := f1 <> f2;
    ca_Above:
      Result := f1 > f2;
    ca_Below:
      Result := f1 < f2;
    ca_In:
      Result := f1 and f2 <> 0;
    ca_NotIn:
      Result := f1 and f2 = 0;
  else
    Result := false;
  end;
end;

Function TestInt(const f1, f2: Integer; action: TCompAction): Boolean;
begin
  Case action of
    ca_None:
      Result := true;
    ca_Equal:
      Result := f1 = f2;
    ca_NotEqual:
      Result := f1 <> f2;
    ca_Above:
      Result := f1 > f2;
    ca_Below:
      Result := f1 < f2;
    ca_In:
      Result := f1 and f2 <> 0;
    ca_NotIn:
      Result := f1 and f2 = 0;
  else
    Result := false;
  end;
end;

Function TestColor(c1, c2: TColorF; action: TCompAction): Boolean;
 var rt,gt,bt,at : Boolean;
begin
  rt := TestDouble(c1.r, c2.r, action);
  gt := TestDouble(c1.g, c2.g, action);
  bt := TestDouble(c1.b, c2.b, action);
  at := TestDouble(c1.a, c2.a, action);
  Result := rt and gt and bt and at;
end;

end.
