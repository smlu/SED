unit Tools;

interface
uses SysUtils, Classes;
Type

TStringParser=class(TStringList)
 Procedure ParseString(var s:STring);
end;

implementation

Function GetWord(s:string;p:integer;var w:string):integer;
var b,e:integer;
begin
 if s='' then begin w:=''; result:=1; exit; end;
 b:=p;
 While (s[b] in [' ',#9]) and (b<=length(s)) do inc(b);
 e:=b;
 While (not (s[e] in [' ',#9])) and (e<=length(s)) do inc(e);
 w:=Copy(s,b,e-b);
 GetWord:=e;
end;

Procedure TStringParser.ParseString(var s:STring);
var cpos:Word;w:String;
begin
 Clear;
 cpos:=1;
 While Cpos<Length(s) do
 begin
  cPos:=GetWord(s,cpos,w);
  Add(UpperCase(w));
 end;
end;

end.
