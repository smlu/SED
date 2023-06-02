unit Q_Utils;

interface
uses J_Level, SysUtils, StdCtrls, Misc_Utils, Windows;

Type
 TCompAction=(ca_None,ca_Equal,ca_NotEqual,ca_Above,ca_Below,
              ca_In,ca_NotIn);

 TQInt=record
         V:Integer;
         act:TCompAction;
       end;
 TQString=record
         V:String;
         act:TCompAction;
       end;
 TQFlags=record
         V:Longint;
         act:TCompAction;
        end;
 TQDouble=record
           V:Double;
           act:TCompAction;
          end;

 TSectorFindInfo=class
  num:TQint;
  NSurfs:TQInt;
  Flags: TQFlags;
  extra_l:TQdouble;
  ColorMap:TQString;
  tint_r:TQdouble;
  tint_g:TQdouble;
  tint_b:TQdouble;
  sound:TQString;
  sound_vol:TQDouble;
  Layer:TQString;
end;

TSurfFindInfo=class
 num:TQint;
 material:TQstring;
 AdjoinSC:TQInt;
 AdjoinSF:TQInt;
 AdjoinFlags:TQFlags;
 SurfFlags:TQFlags;
 FaceFlags:TQFlags;
 geo,light,tex:TQint;
 extra_l:TQDouble;
 layer:TQString;
end;

TThingFindInfo=class
 num:TQInt;
 name:TQString;
 sec:TQint;
 X,Y,Z:TQDouble;
 PCH,YAW,ROL:TQDouble;
 layer: TQString;
end;

TQueryField=class
 CB: TComboBox;
 EB:TEdit;
 vType:Integer;
 P:Pointer;
 Constructor Create(aCB:TComboBox;aEB:TEdit);
 Constructor CreateHex(aCB:TComboBox;aEB:TEdit;var H:TQInt);
 Constructor CreateStr(aCB:TComboBox;aEB:TEdit;var S:TQString);
 Constructor CreateInt(aCB:TComboBox;aEB:TEdit;var I:TQInt);
 Constructor CreateDouble(aCB:TComboBox;aEB:TEdit;var D:TQDouble);
 Constructor CreateFlags(aCB:TComboBox;aEB:TEdit;var F:TQFlags);
 procedure EBChangeHandler(Sender: TObject);
 Procedure CBChangeHandler(Sender: TObject);
end;

Function TestStr(const s1,s2:String;action:TCompAction):Boolean;
Function TestDouble(d1,d2:double;action:TCompAction):Boolean;
Function TestFlags(f1,f2:Longint;action:TCompAction):Boolean;
Function TestInt(const f1,f2:Integer;action:TCompAction):Boolean;

Function SectorMatches(S:TJKSector;fi:TSectorFindInfo):Boolean;
Function SurfMatches(W:TJKSurface;fi:TSurfFindInfo):Boolean;
Function ThingMatches(O:TJKThing;fi:TThingFindInfo):Boolean;

Function FindNextSector(L:TJKLevel;LastSC:Integer;fi:TSectorFindInfo):Integer;
Function FindNextSurf(L:TJKLevel;LastSC,LastSF:Integer;fi:TSurfFindInfo;var NewSC:Integer):Integer;
Function FindNextThing(L:TJKLevel;LastTH:Integer;fi:TThingFindInfo):Integer;

implementation
uses FieldEdit;

const
     vtInt=1;
     vtStr=2;
     vtDouble=3;
     vtFlags=4;
     vtHex=5;

Constructor TQueryField.Create(aCB:TComboBox;aEB:TEdit);
begin
 CB:=aCB;
 EB:=aEB;
 EB.OnChange:=EBChangeHandler;
 CB.OnChange:=CBChangeHandler;
end;

Constructor TQueryField.CreateStr(aCB:TComboBox;aEB:TEdit;var S:TQString);
begin
 Create(aCB,aEB);
 P:=@S;
 VType:=vtStr;
end;

Constructor TQueryField.CreateHex(aCB:TComboBox;aEB:TEdit;var H:TQInt);
begin
 Create(aCB,aEB);
 P:=@H;
 VType:=vtHex;
end;

Constructor TQueryField.CreateInt(aCB:TComboBox;aEB:TEdit;var I:TQInt);
begin
 Create(aCB,aEB);
 P:=@I;
 VType:=vtInt;
end;

Constructor TQueryField.CreateDouble(aCB:TComboBox;aEB:TEdit;var D:TQDouble);
begin
 Create(aCB,aEB);
 P:=@D;
 VType:=vtDouble;
end;

Constructor TQueryField.CreateFlags(aCB:TComboBox;aEB:TEdit;var F:TQFlags);
begin
 Create(aCB,aEB);
 P:=@F;
 VType:=vtFlags;
end;

procedure TQueryField.EBChangeHandler(Sender: TObject);
var s:String;a:Integer;
    l:Longint;
    i:Integer;
    d:Double;
begin
 s:=EB.Text;
 If CB.ItemIndex<=0 then begin CB.ItemIndex:=1; CB.OnChange(nil); end;
 Case vType of
  vtStr: TQString(P^).V:=s;
  vtInt: With TQInt(P^) do
          if ValInt(s,i) then V:=i else
          begin
           if s='-' then exit;
           MsgBox(s+' is not a valid integer','Error',mb_ok);
           EB.Text:=IntToStr(V);
          end;
  vtDouble: With TQDouble(P^) do
          if feValDouble(s,d) then V:=d else
          begin
           EB.Text:=FloatToStr(v);
           MsgBox(s+' is not a valid double','Error',mb_ok);
          end;
  vtFlags: With TQInt(P^) do
          if ValDword(s,l) then V:=l else
          begin
           MsgBox(s+' is not a valid dword','Error',mb_ok);
           EB.Text:=DwordToStr(V);
          end;
  vtHex: With TQInt(P^) do
          if ValHex(s,l) then V:=l else
          begin
           MsgBox(s+' is not a valid hex','Error',mb_ok);
           EB.Text:=Format('%x',[V]);
          end;
 end;
end;

procedure TQueryField.CBChangeHandler(Sender: TObject);
var s:String; act:TCompAction;
begin
 s:=UpperCase(CB.Text);
 act:=ca_None;

 if s='' then act:=ca_None
 else if s='=' then act:=ca_Equal
 else if s='>' then act:=ca_Above
 else if s='<' then act:=ca_Below
 else if s='<>' then act:=ca_NotEqual
 else if s='IS' then act:=ca_Equal
 else if s='IS NOT' then act:=ca_NotEqual
 else if s[1] in ['S','C'] then act:=ca_In {'Set' or 'Contains'}
 else if s[1] in ['N','D'] then act:=ca_NotIn; {'Not set' or 'Doesn't contain'}

 case vtype of
  vtStr: TQString(p^).act:=act;
  vtInt,vtHex: TQInt(p^).act:=act;
  vtDouble: TQDouble(p^).act:=act;
  vtFlags: TQFlags(p^).act:=act;
 end;
end;


Function QTestInt(const qi:TQInt;I:Integer):Boolean;
begin
 Result:=TestInt(i,qi.v,qi.act);
end;

Function QTestStr(const qi:TQString;s:String):Boolean;
begin
 Result:=TestStr(qi.v,s,qi.act);
end;

Function QTestFlags(const qi:TQFlags;f:LongInt):Boolean;
begin
 Result:=TestFlags(f,qi.v,qi.act);
end;

Function QTestDouble(const qi:TQDouble;d:double):Boolean;
begin
 Result:=TestDouble(d,qi.v,qi.act);
end;

Function SectorMatches(S:TJKSector;fi:TSectorFindInfo):Boolean;
begin
 With fi do Result:=
  QTestInt(num,s.num) and
  QTestInt(NSurfs,s.surfaces.count) and
  QTestFlags(Flags,s.flags) and
  QTestDouble(extra_l,s.extra) and
  QTestStr(ColorMap,s.ColorMap) and
  QTestDouble(tint_r,s.tint.dx) and
  QTestDouble(tint_g,s.tint.dy) and
  QTestDouble(tint_b,s.tint.dz) and
  QTestStr(sound,s.sound) and
  QTestDouble(sound_vol,s.snd_vol) and
  QTestStr(Layer,s.Level.GetLayerName(s.Layer));
end;

Function SurfMatches(W:TJKSurface;fi:TSurfFindInfo):Boolean;

function getSCnum(s:TJKSurface):integer;
begin
 if s=nil then result:=-1 else result:=s.sector.num;
end;

function getSFnum(s:TJKSurface):integer;
begin
 if s=nil then result:=-1 else result:=s.num;
end;

begin
 With fi do
 Result:=
  QTestInt(num,w.num) and
  QTestStr(Material,w.Material) and
  QTestInt(AdjoinSC,GetSCNum(w.adjoin)) and
  QTestInt(AdjoinSF,getSFNum(w.adjoin)) and
  QTestFlags(AdjoinFlags,w.AdjoinFlags) and
  QTestFlags(SurfFlags,w.SurfFlags) and
  QTestFlags(FaceFlags,w.FaceFlags) and
  QTestInt(geo,w.geo) and
  QTestInt(light,w.light) and
  QTestInt(tex,w.tex) and
  QTestDouble(extra_l,w.extralight) and
  QTestStr(Layer,w.sector.Level.GetLayerName(w.sector.Layer));

end;

Function ThingMatches(O:TJKThing;fi:TThingFindInfo):Boolean;

function GetSCNum(s:TJKSector):integer;
begin
 if s=nil then result:=-1 else result:=s.num;
end;

begin
With fi do
 Result:=
  QTestInt(num,o.num) and
  QTestStr(name,o.name) and
  QTestInt(sec,GetSCNum(o.sec)) and
  QTestDouble(X,o.x) and
  QTestDouble(Y,o.y) and
  QTestDouble(Z,o.z) and
  QTestDouble(PCH,o.pch) and
  QTestDouble(YAW,o.yaw) and
  QTestDouble(ROL,o.rol) and
  QTestStr(Layer,o.Level.GetLayerName(o.Layer));
end;

Function FindNextSector(L:TJKLevel;LastSC:Integer;fi:TSectorFindInfo):Integer;
var s:Integer;
begin
 Result:=-1;
 For s:=LastSC+1 to L.Sectors.Count-1 do
  if SectorMatches(L.Sectors[s],fi) Then
     begin
      Result:=s;
      exit;
     end;
end;

Function FindNextSurf(L:TJKLevel;LastSC,LastSF:Integer;fi:TSurfFindInfo;var NewSC:Integer):Integer;
var s,w:Integer;
begin
Result:=-1;
NewSC:=-1;

for s:=LastSC to L.Sectors.count-1 do
With L.Sectors[s] do
begin
 for w:=LastSF+1 to Surfaces.Count-1 do
 begin
  if SurfMatches(Surfaces[w],fi) then
     begin
      NewSC:=s;
      Result:=w;
      exit;
     end;
 end;
 LastSF:=-1;
end;
end;



Function FindNextThing(L:TJKLevel;LastTH:Integer;fi:TThingFindInfo):Integer;
var o:Integer;
begin
 Result:=-1;
 for o:=LastTH+1 to L.things.Count-1 do
 begin
  if ThingMatches(L.Things[o],fi) then
     begin
      Result:=o;
      exit;
     end;
 end;
end;

Function TestStr(const s1,s2:String;action:TCompAction):Boolean;
begin
 Case action of
     ca_none: Result:=true;
    ca_Equal: Result:=CompareText(s1,s2)=0;
 ca_NotEqual: Result:=CompareText(s1,s2)<>0;
    ca_Above: Result:=CompareText(s1,s2)>0;
    ca_Below: Result:=CompareText(s1,s2)<0;
       ca_in: Result:=Pos(UpperCase(s1),UpperCase(s2))<>0;
    ca_NotIn: Result:=Pos(UpperCase(s1),UpperCase(s2))=0;
 else Result:=false;
 end;
end;

Function TestDouble(d1,d2:double;action:TCompAction):Boolean;
begin
 Case action of
     ca_none: Result:=true;
    ca_Equal: Result:=d1=d2;
 ca_NotEqual: Result:=d1<>d2;
    ca_Above: Result:=d1>d2;
    ca_Below: Result:=d1<d2;
 else Result:=false;
 end;
end;

Function TestFlags(f1,f2:Longint;action:TCompAction):Boolean;
begin
 Case action of
     ca_none: Result:=true;
    ca_Equal: Result:=f1=f2;
 ca_NotEqual: Result:=f1<>f2;
    ca_Above: Result:=f1>f2;
    ca_Below: Result:=f1<f2;
       ca_in: Result:=f1 and f2<>0;
    ca_NotIn: Result:=f1 and f2=0;
 else Result:=false;
 end;
end;

Function TestInt(const f1,f2:Integer;action:TCompAction):Boolean;
begin
 Case action of
     ca_none: Result:=true;
    ca_Equal: Result:=f1=f2;
 ca_NotEqual: Result:=f1<>f2;
    ca_Above: Result:=f1>f2;
    ca_Below: Result:=f1<f2;
       ca_in: Result:=f1 and f2<>0;
    ca_NotIn: Result:=f1 and f2=0;
 else Result:=false;
 end;
end;


end.
