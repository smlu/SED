unit geo_utils;

interface
uses pjgeometry;

const
 rt_x=0;
 rt_y=1;
 rt_z=2;

Procedure MultVM3(const mx:TMat3x3; var x,y,z:double);
Procedure MultM3(var mx:TMat3x3; const bymx:TMat3x3);
Procedure MultVM3s(const mx:TMat3x3s; var x,y,z:single);
Procedure MultM3s(var mx:TMat3x3s; const bymx:TMat3x3s);



Procedure CreateRotMatrix(var mx:TMat3x3; pch,yaw,rol:double);
Procedure ScaleMatrix(var mx:TMat3x3;scx,scy,scz:double);

Procedure CreateRotMatrixS(var mx:TMat3x3s; pch,yaw,rol:single);
Procedure ScaleMatrixS(var mx:TMat3x3s;scx,scy,scz:single);
Procedure RotateVector(var vec:TVector; pch,yaw,rol:double);

implementation

Procedure MultVM3(const mx:TMat3x3; var x,y,z:double);
var i,j,k:integer;
    s,r:array[0..2] of double;
    sum:double;
begin
 s[0]:=x; s[1]:=y; s[2]:=z;
 for i:=0 to 2 do
 begin
  sum:=0;
  for k:=0 to 2 do sum:=sum+mx[i,k]*s[k];
  r[i]:=sum;
 end;
 x:=r[0]; y:=r[1]; z:=r[2];
end;

Procedure MultVM3s(const mx:TMat3x3s; var x,y,z:single);
var i,j,k:integer;
    s,r:array[0..2] of single;
    sum:double;
begin
 s[0]:=x; s[1]:=y; s[2]:=z;
 for i:=0 to 2 do
 begin
  sum:=0;
  for k:=0 to 2 do sum:=sum+mx[i,k]*s[k];
  r[i]:=sum;
 end;
 x:=r[0]; y:=r[1]; z:=r[2];
end;


Procedure MultM3(var mx:TMat3x3; const bymx:TMat3x3);
var i,j,k:integer;
    sum:double;
    tmx:TMat3x3;
begin
 for i:=0 to 2 do
 for j:=0 to 2 do
 begin
  sum:=0;
  for k:=0 to 2 do sum:=sum+mx[i,k]*bymx[k,j];
  tmx[i,j]:=sum;
 end;
 mx:=tmx;
end;

Procedure MultM3s(var mx:TMat3x3s; const bymx:TMat3x3s);
var i,j,k:integer;
    sum:single;
    tmx:TMat3x3s;
begin
 for i:=0 to 2 do
 for j:=0 to 2 do
 begin
  sum:=0;
  for k:=0 to 2 do sum:=sum+mx[i,k]*bymx[k,j];
  tmx[i,j]:=sum;
 end;
 mx:=tmx;
end;


Procedure CreateMatrix(var mx:TMat3x3; angle:double; axis:integer);
var cosa,sina:double;
begin
 cosa:=cos(angle/180*Pi);
 sina:=sin(angle/180*Pi);
 case axis of
  rt_x: begin
         mx[0,0]:=1; mx[0,1]:=0;     mx[0,2]:=0;
         mx[1,0]:=0; mx[1,1]:=cosa;  mx[1,2]:=sina;
         mx[2,0]:=0; mx[2,1]:=-sina; mx[2,2]:=cosa;
        end;
  rt_y: begin
         mx[0,0]:=cosa; mx[0,1]:=0; mx[0,2]:=-sina;
         mx[1,0]:=0;    mx[1,1]:=1; mx[1,2]:=0;
         mx[2,0]:=sina; mx[2,1]:=0; mx[2,2]:=cosa;
        end;
  rt_z: begin
         mx[0,0]:=cosa;  mx[0,1]:=sina; mx[0,2]:=0;
         mx[1,0]:=-sina; mx[1,1]:=cosa; mx[1,2]:=0;
         mx[2,0]:=0;     mx[2,1]:=0;    mx[2,2]:=1;
        end;
 end;
end;


Procedure CreateRotMatrix(var mx:TMat3x3; pch,yaw,rol:double);
var tmx:TMat3x3;
begin
 CreateMatrix(mx,-YAW,rt_z);

 CreateMatrix(tmx,-PCH,rt_x);
 MultM3(mx,tmx);

 CreateMatrix(tmx,-ROL,rt_y);
 MultM3(mx,tmx);

end;

Procedure CreateMatrixs(var mx:TMat3x3s; angle:single; axis:integer);
var cosa,sina:single;
begin
 cosa:=cos(angle/180*Pi);
 sina:=sin(angle/180*Pi);
 case axis of
  rt_x: begin
         mx[0,0]:=1; mx[0,1]:=0;     mx[0,2]:=0;
         mx[1,0]:=0; mx[1,1]:=cosa;  mx[1,2]:=sina;
         mx[2,0]:=0; mx[2,1]:=-sina; mx[2,2]:=cosa;
        end;
  rt_y: begin
         mx[0,0]:=cosa; mx[0,1]:=0; mx[0,2]:=-sina;
         mx[1,0]:=0;    mx[1,1]:=1; mx[1,2]:=0;
         mx[2,0]:=sina; mx[2,1]:=0; mx[2,2]:=cosa;
        end;
  rt_z: begin
         mx[0,0]:=cosa;  mx[0,1]:=sina; mx[0,2]:=0;
         mx[1,0]:=-sina; mx[1,1]:=cosa; mx[1,2]:=0;
         mx[2,0]:=0;     mx[2,1]:=0;    mx[2,2]:=1;
        end;
 end;
end;



Procedure CreateRotMatrixs(var mx:TMat3x3s; pch,yaw,rol:single);
var tmx:TMat3x3s;
begin
 CreateMatrixs(mx,-ROL,rt_y);

 CreateMatrixs(tmx,-YAW,rt_z);
 MultM3s(mx,tmx);

 CreateMatrixs(tmx,-PCH,rt_x);
 MultM3s(mx,tmx);
end;


Procedure ScaleMatrix(var mx:TMat3x3;scx,scy,scz:double);
var tmx:TMat3x3;
begin
 FillChar(tmx,sizeof(tmx),0);
 tmx[0,0]:=scx;
 tmx[1,1]:=scy;
 tmx[2,2]:=scz;
 MultM3(mx,tmx);
end;

Procedure ScaleMatrixs(var mx:TMat3x3s;scx,scy,scz:single);
var tmx:TMat3x3s;
begin
 FillChar(tmx,sizeof(tmx),0);
 tmx[0,0]:=scx;
 tmx[1,1]:=scy;
 tmx[2,2]:=scz;
 MultM3s(mx,tmx);
end;

Procedure RotateVector(var vec:TVector; pch,yaw,rol:double);
var mx:TMat3x3;
begin
 CreateRotMatrix(mx,pch,yaw,rol);
 MultVM3(mx,vec.dx,vec.dy,vec.dz);
(* cosa:=cos(-PCH/180*Pi); {around X}
 sina:=sin(-PCH/180*Pi);
 dx:=vec.dx;
 dy:=cosa*vec.dy+sina*vec.dz;
 dz:=-sina*vec.dy+cosa*vec.dz;


 cosa:=cos(-YAW/180*Pi); {Around Z}
 sina:=sin(-YAW/180*Pi);
 vec.dx:=cosa*dx+sina*dy;
 vec.dy:=-sina*dx+cosa*dy;
 vec.dz:=dz;

 cosa:=cos(-ROL/180*Pi);  {Around Y}
 sina:=sin(-ROL/180*Pi);
 dx:=cosa*vec.dx-sina*vec.dz;
 dy:=vec.dy;
 dz:=sina*vec.dx+cosa*vec.dz;

 vec.dx:=dx;
 vec.dy:=dy;
 vec.dz:=dz;*)
end;


end.
