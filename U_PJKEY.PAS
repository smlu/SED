unit u_pjkey;

interface

uses classes, files, fileoperations, misc_utils, u_pj3dos;

type

TKEYEntry=class
 flags:integer;
 framenum:integer;
 cx,cy,cz,cpch,cyaw,crol:double;
 dx,dy,dz,dpch,dyaw,drol:double;
end;

TKEYEntries=class(TList)
 Function GetItem(n:integer):TKEYEntry;
 Procedure SetItem(n:integer;v:TKEYEntry);
Property Items[n:integer]:TKEYEntry read GetItem write SetItem; default;
end;


TKEYNode=class
 nodenum:integer;
 nodename:string;
 entries:TKeyEntries;
 constructor Create;
 Procedure GetFrame(n:integer;var x,y,z,pch,yaw,rol:double);
 destructor Destroy;
end;

TKEYNodes=class(TList)
 Function GetItem(n:integer):TKEYNode;
 Procedure SetItem(n:integer;v:TKEYNode);
Property Items[n:integer]:TKEYNode read GetItem write SetItem; default;
end;



TKeyFile=class
 name:string;
 nframes:integer;
 flags:integer;
 fps:integer;
 nodes:TKeyNodes;
 Constructor CreateNew;
 Constructor CreateFromKEY(const name:string);
 Destructor Destroy;
 Procedure ApplyKey(a3DO:TPJ3DO;frame:integer);
end;


implementation

Function TKEYEntries.GetItem(n:integer):TKEYEntry;
begin
 if (n<0) or (n>=count) then raise EListError.CreateFmt('KEY Entry Index is out of bounds: %d',[n]);
 Result:=TKEYEntry(List[n]);
end;

Procedure TKEYEntries.SetItem(n:integer;v:TKEYEntry);
begin
 if (n<0) or (n>=count) then raise EListError.CreateFmt('KEY Entry Index is out of bounds: %d',[n]);
 List[n]:=v;
end;

Function TKEYNodes.GetItem(n:integer):TKEYNode;
begin
 if (n<0) or (n>=count) then raise EListError.CreateFmt('KEY Node Index is out of bounds: %d',[n]);
 Result:=TKEYNode(List[n]);
end;

Procedure TKEYNodes.SetItem(n:integer;v:TKEYNode);
begin
 if (n<0) or (n>=count) then raise EListError.CreateFmt('KEY Node Index is out of bounds: %d',[n]);
 List[n]:=v;
end;


constructor TKEYNode.Create;
begin
 entries:=TKEYEntries.Create;
end;

Procedure TKEYNode.GetFrame(n:integer;var x,y,z,pch,yaw,rol:double);
var i,df:integer;
begin
for i:=0 to entries.count-1 do if n<entries[i].framenum then break;
With entries[i-1] do
begin
df:=n-framenum;
x:=cx+(dx*df);
y:=cy+(dx*df);
z:=cz+(dz*df);

pch:=cpch+(dpch*df);
yaw:=cyaw+(dyaw*df);
rol:=crol+(drol*df);

{pch:=Round(cpch+(dpch*df)) mod 360;
if pch<0 then pch:=360+pch;
yaw:=Round(cyaw+(dyaw*df)) mod 360;
if yaw<0 then yaw:=360+yaw;
rol:=Round(crol+(drol*df)) mod 360;
if rol<0 then rol:=360+rol;}
end;
end;

(*Procedure TKEYNode.GetFrame(n:integer;var x,y,z,pch,yaw,rol:double);
var c,i:integer;
    ke:TKeyEntry;
    d:double;
    df:integer;
    npentry,noentry:integer;
begin
 npentry:=-1;
 noentry:=-1;

 for i:=0 to entries.count-1 do
 With entries[i] do
 begin
  if n<framenum then break;
  if (flags and 1)<>0 then npentry:=i;
  if (flags and 2)<>0 then noentry:=i;
  if n=framenum then break;
 end;

 if npentry<>-1 then
 begin
  ke:=entries[npentry];
  df:=n-ke.framenum;
{   if abs then
   begin}
{    x:=ke.cx+ke.dx*df;
    y:=ke.cy+ke.dy*df;
    z:=ke.cz+ke.dz*df;}
{   end
   else
   begin}
 {   x:=x+ke.cx+ke.dx*df;
    y:=y+ke.cy+ke.dy*df;
    z:=z+ke.cz+ke.dz*df;}
{   end;}
 end;

 if noentry<>-1 then
 begin
  ke:=entries[noentry];
  df:=n-ke.framenum;
  pch:=ke.cpch+ke.dpch*df;
  yaw:=ke.cyaw+ke.dyaw*df;
  rol:=ke.crol+ke.drol*df;
 end;

end; *)

destructor TKEYNode.Destroy;
var i:integer;
begin
 for i:=0 to entries.count-1 do entries[i].Free;
 entries.free;
end;

Constructor TKEYFile.CreateNew;
begin
 name:='Untitled.key';
 nframes:=0;
 flags:=0;
 fps:=15;
 nodes:=TKeyNodes.Create;
end;

{$i pjkey_io.inc}

Destructor TKEYFile.Destroy;
var i:integer;
begin
 for i:=0 to nodes.count-1 do nodes[i].free;
 nodes.free;
end;

Procedure TKEYFile.ApplyKey(a3DO:TPJ3DO;frame:integer);
var i,j:integer;
    anode:T3DOHNode;
    transfd:array[0..500] of boolean;

Function GetMeshIdx(const name:string):integer;
var i:integer;
begin
 result:=-1;
 for i:=0 to a3DO.hnodes.count-1 do
 if not transfd[i] then
  if CompareText(a3DO.hnodes[i].MeshName,name)=0 then
  begin
   result:=i;
   transfd[i]:=true;
   break;
  end;
end;

begin
 fillChar(transfd,sizeof(transfd),0);
 a3DO.setdefaultoffsets;
 for i:=0 to nodes.count-1 do
 With nodes[i] do
 begin
  j:=GetMeshIdx(nodename);
  if j=-1 then continue;

  aNode:=a3DO.Hnodes[j];
  With aNode do GetFrame(frame,x,y,z,pch,yaw,rol);
 end;
 a3Do.OffsetMeshes;
end;

end.




