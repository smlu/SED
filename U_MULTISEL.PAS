unit u_multisel;

interface
uses classes, J_Level;

const
  NMask=Longint(-1) shr 1;
  NBit=1 shl 31;

Type

TMultisel=class(TList)

  Procedure DeleteN(n:integer);

  Procedure Sort;
Private
  Function GetInt(n:Integer):Integer;
  Procedure SetInt(n:integer;I:Integer);
  Function AddInt(i:integer):integer;
  Procedure InsertInt(n:integer;i:integer);
  Property Ints[n:integer]:Integer read GetInt write SetInt; default;
  Function AddIntUnique(v:integer):integer;
end;

TSCMultiSel=class(TMultiSel)
  Function FindSC(sc:integer):integer;
  function AddSC(sc:integer):integer;
  Function GetSC(n:integer):integer;
end;

TSFMultiSel=class(TMultiSel)
  Function FindSF(sc,sf:integer):integer;
  function AddSF(sc,sf:integer):integer;
  Procedure GetSCSF(n:integer;var sc,sf:integer);
end;

TTHMultiSel=class(TMultiSel)
  Function FindTH(th:integer):integer;
  function AddTH(th:integer):integer;
  Function GetTH(n:integer):integer;
end;

TLTMultiSel=class(TMultiSel)
  Function FindLT(lt:integer):integer;
  function AddLT(lt:integer):integer;
  Function GetLT(n:integer):integer;
end;

TEDMultiSel=class(TMultiSel)
  Function FindED(sc,sf,ed:integer):integer;
  function AddED(sc,sf,ed:integer):integer;
  Procedure GetSCSFED(n:integer;var sc,sf,ed:integer);
end;

TVXMultiSel=class(TMultiSel)
  Function FindVX(sc,vx:integer):integer;
  function AddVX(sc,vx:integer):integer;
  Procedure GetSCVX(n:integer;var sc,vx:integer);
end;

TFRMultiSel=class(TMultiSel)
  Function FindFR(th,fr:integer):integer;
  function AddFR(th,fr:integer):integer;
  Procedure GetTHFR(n:integer;var th,fr:integer);
end;

implementation

{function FindInt(vs:TMultisel;v:longint; var Index: Integer): Boolean;
var
  L, H, I, C, A: Integer;
begin
  Result := False;
  L := 0;
  H := Vs.Count - 1;
  while L <= H do
  begin
    I := (L + H) shr 1;
    a:=vs[i];
    if a>v then c:=1 else
    if a<v then c:=-1 else c:=0;
    if C < 0 then L := I + 1 else
    begin
      H := I - 1;
      if C = 0 then
      begin
        Result := True;
        L := I;
      end;
    end;
  end;
  Index := L;
end;}

function FindInt(vs:TMultisel;v:longint; var Index: Integer): Boolean;
begin
 Index:=vs.IndexOf(pointer(v));
 result:=Index<>-1;
end;

Function TMultisel.GetInt(n:Integer):Integer;
begin
 Result:=Integer(Items[n]);
end;

Procedure TMultisel.SetInt(n:integer;I:Integer);
begin
 Items[n]:=Pointer(i);
end;

Procedure TMultisel.InsertInt(n:integer;i:integer);
begin
 Inherited Insert(n,Pointer(i));
end;

Function TMultisel.AddInt(i:integer):integer;
begin
 Result:=Inherited Add(Pointer(i));
end;

{Public functions}

Function TSCMultisel.FindSC(sc:integer):integer;
var i:integer;
begin
 if FindInt(self,sc,i) then result:=i else result:=-1;
end;

Function TSFMultisel.FindSF(sc,sf:integer):integer;
var i:integer;
begin
 if FindInt(self,SfToInt(sc,sf),i) then result:=i else result:=-1;
end;

Function TTHMultisel.FindTH(th:integer):integer;
var i:integer;
begin
 if FindInt(self,th,i) then result:=i else result:=-1;
end;

Function TLTMultisel.FindLT(lt:integer):integer;
var i:integer;
begin
 if FindInt(self,lt,i) then result:=i else result:=-1;
end;


Function TEDMultisel.FindED(sc,sf,ed:integer):integer;
var i:integer;
begin
 if FindInt(self,edToInt(sc,sf,ed),i) then result:=i else result:=-1;
end;

Function TVXMultisel.FindVX(sc,vx:integer):integer;
var i:integer;
begin
 if FindInt(self,vxToInt(sc,vx),i) then result:=i else result:=-1;
end;

Function TFRMultisel.FindFR(th,fr:integer):integer;
var i:integer;
begin
 if FindInt(self,frToInt(th,fr),i) then result:=i else result:=-1;
end;


Function TMultisel.AddIntUnique(v:integer):integer;
begin
 if not FindInt(self,v,Result) then Result:=AddInt(v) else
 result:=result or NBit;
end;

Procedure TMultisel.DeleteN(n:integer);
begin
 if n and NBit=0 then
  if n<Count then Delete(n);
end;


Function TSCMultisel.AddSC(sc:integer):integer;
begin
 Result:=AddIntUnique(sc);
end;

Function TSFMultisel.AddSF(sc,sf:integer):integer;
begin
 Result:=AddIntUnique(SfToInt(sc,sf));
end;

Function TTHMultisel.AddTH(th:integer):integer;
begin
 Result:=AddIntUnique(th);
end;

Function TLTMultisel.AddLT(lt:integer):integer;
begin
 Result:=AddIntUnique(lt);
end;

Function TEDMultisel.AddED(sc,sf,ed:integer):integer;
begin
 result:=AddIntUnique(edtoInt(sc,sf,ed));
end;

Function TVXMultisel.AddVX(sc,vx:integer):integer;
begin
 Result:=AddIntUnique(vxtoInt(sc,vx));
end;

function TFRMultisel.AddFR(th,fr:integer):integer;
begin
 if fr=-1 then begin result:=-1; exit; end;
 Result:=AddIntUnique(frtoInt(th,fr));
end;


Function TSCMultisel.GetSC(n:integer):integer;
begin
 Result:=GetInt(n);
end;

Function TTHMultisel.GetTH(n:integer):integer;
begin
 Result:=GetInt(n);
end;

Function TLTMultisel.GetLT(n:integer):integer;
begin
 Result:=GetInt(n);
end;

Procedure TSFMultisel.GetSCSF(n:integer;var sc,sf:integer);
var i:integer;
begin
 i:=GetInt(n);
 sc:=GetsfSC(i);
 sf:=GetsfSF(i);
end;

Procedure TVXMultisel.GetSCVX(n:integer;var sc,vx:integer);
var i:integer;
begin
 i:=GetInt(n);
 sc:=GetvxSC(i);
 vx:=GetvxVX(i);
end;

Procedure TEDMultisel.GetSCSFED(n:integer;var sc,sf,ed:integer);
var i:integer;
begin
 i:=GetInt(n);
 sc:=GetedSC(i);
 sf:=GetedSF(i);
 ed:=GetedED(i);
end;

Procedure TFRMultisel.GetTHFR(n:integer;var th,fr:integer);
var i:integer;
begin
 i:=GetInt(n);
 th:=GetfrTH(i);
 fr:=GetfrFR(i);
end;


function CmpInt(Item1, Item2: Pointer): Integer;
var i1:integer absolute item1;
    i2:integer absolute item2;
begin
 if i1>i2 then result:=1 else
 if i1<i2 then result:=-1 else
 result:=0;
end;

Procedure TMultisel.Sort;
begin
 Inherited Sort(CmpInt);
end;


end.
