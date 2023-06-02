unit U_copypaste;

interface
uses J_Level, U_Multisel, Lev_Utils;

Procedure CopySectors(Lev:TJKLevel;scsel:TSCMultiSel;Cur_SC:integer);
Procedure CopyThings(Lev:TJKLevel;thsel:TTHMultiSel;Cur_TH:integer);
Procedure CopyLights(Lev:TJKLevel;ltsel:TLTMultiSel;Cur_LT:integer);

Function CanPasteSectors:Boolean;
Function CanPasteThings:Boolean;
Function CanPasteLights:Boolean;

Function PasteSectors(Lev:TJKLevel;atX,atY,atZ:Double):Integer; {Returns first pasted Sector}
Function PasteThings(Lev:TJKLevel;atX,atY,atZ:Double):Integer; {Returns first pasted Object}
Function PasteLights(Lev:TJKLevel;atX,atY,atZ:Double):Integer; {Returns first pasted Object}

implementation
uses U_undo, Clipbrd, WIndows;

var CF_JKSectors:Word;
    CF_JKThings:Word;
    CF_JKLights:Word;

Function CanPasteSectors:Boolean;
var Clp:TClipboard;
begin
 Clp:=Clipboard;
 Result:=Clp.HasFormat(CF_JKSectors);
end;

Function CanPasteThings:Boolean;
var Clp:TClipboard;
begin
 Clp:=Clipboard;
 Result:=Clp.HasFormat(CF_JKThings);
end;

Function CanPasteLights:Boolean;
var Clp:TClipboard;
begin
 Clp:=Clipboard;
 Result:=Clp.HasFormat(CF_JKLights);
end;

Procedure CopySectors(Lev:TJKLevel;scsel:TSCMultiSel;Cur_SC:integer);
var i,j,sf,n,msize:integer;
    sec:TJKSector;
    surf:TJKSurface;
    hg:integer;
    pg:pointer;
    clp:TClipboard;
    rx,ry,rz:double;
    pvx:^TVXRec;

begin
 n:=scsel.AddSC(Cur_SC);

 FindCenter(Lev.Sectors[Cur_SC],rx,ry,rz);

 {Calculate the size of memory to allocate}
 msize:=sizeof(longint);
 for i:=0 to scsel.count-1 do
 begin
  inc(msize,GetSecRecSize);
  sec:=lev.Sectors[scsel.GetSC(i)];

  inc(msize,SCVertSize(sec.vertices));

  for sf:=0 to sec.surfaces.count-1 do
  begin
   surf:=sec.surfaces[sf];
   inc(msize,GetSurfRecSize);
   inc(msize,SFVertSize(surf.vertices));
  end;
 end;

 hg:=GlobalAlloc(GMEM_MOVEABLE or GMEM_SHARE,msize);
 pg:=GlobalLock(hg);
 Longint(pg^):=scsel.count; inc(pchar(pg),sizeof(Longint));


 for i:=0 to scsel.count-1 do
 begin
  sec:=lev.Sectors[scsel.GetSC(i)];
  GetSec(sec,TSecRec(pg^));
  inc(pchar(pg),GetSecRecSize);

  GetSCVertices(sec.vertices,pg);

  pvx:=pg;
  for j:=0 to sec.vertices.count-1 do
  begin
   pvx^.x:=pvx^.x-rx;
   pvx^.y:=pvx^.y-ry;
   pvx^.z:=pvx^.z-rz;
   inc(pvx);
  end;

  inc(pchar(pg),SCVertSize(sec.vertices));

  for sf:=0 to sec.surfaces.count-1 do
  begin
   surf:=sec.surfaces[sf];

   GetSurf(surf,TSurfRec(pg^));
   inc(pchar(pg),GetSurfRecSize);

   GetSFVertices(surf,pg);
   inc(pchar(pg),SFVertSize(surf.vertices));
  end;
 end;

 GlobalUnlock(hg);
 Clp:=Clipboard;
 Clp.Clear;
 Clp.SetAsHandle(CF_JKSectors,hg);

 scsel.DeleteN(n);

end;

Function PasteSectors(Lev:TJKLevel;atX,atY,atZ:Double):Integer; {Returns first pasted Sector}
var hg:integer;
    pg:Pointer;
    i,j,sf,n:integer;
    sec:TJKSector;
    surf:TJKSurface;
    vx:TJKVertex;
    clp:TClipboard;
    psrec:^TSecRec;
    psfrec:^TSurfRec;
begin
 Result:=-1;
 Clp:=Clipboard;
 Clp.Open;
try
 hg:=Clp.GetAsHandle(CF_JKSectors);
 if hg=0 then exit;
 pg:=GlobalLock(hg);
 n:=Longint(pg^); inc(pchar(pg),sizeof(longint));

 Result:=Lev.Sectors.count;


 for i:=0 to n-1 do
 begin
  sec:=Lev.NewSector;
  Lev.Sectors.Add(sec);

  psrec:=pg;
  inc(pchar(pg),GetSecRecSize);

  SetSec(sec,PSrec^);

  for j:=0 to PSrec^.nvxs-1 do sec.NewVertex;

  SetSCVertices(sec.vertices,PSrec^.nvxs,pg);
  inc(pchar(pg),SCVertSize(sec.vertices));

  for j:=0 to Sec.vertices.count-1 do
  With sec.vertices[j] do
  begin
   x:=x+AtX;
   y:=y+AtY;
   z:=z+AtZ;
  end;

  sec.Renumber;

  for sf:=0 to PSrec^.nsfs-1 do
  begin
   surf:=sec.NewSurface;
   sec.Surfaces.Add(surf);

   psfRec:=pg;
   inc(pchar(pg),GetSurfRecSize);

   SetSurf(surf,PSfRec^);

   for j:=0 to PSfRec^.Nvxs-1 do surf.AddVertex(nil);

   SetSFVertices(sec,surf,PSfRec^.nvxs,pg);
   inc(pchar(pg),SFVertSize(surf.vertices));

   surf.RecalcAll;

  end;

  Sec.Renumber;

 end;
Finally
 Lev.RenumSecs;
 GlobalUnlock(hg);
 clp.Close;
end;
end;

Procedure CopyLights(Lev:TJKLevel;ltsel:TLTMultiSel;Cur_LT:integer);
var hg:integer;
    i,n:integer;
    rx,ry,rz:double;
    pg:pointer;
    light:TJedLight;
    clp:TClipboard;
    PL:^TlightRec;
begin
 n:=ltsel.AddLT(Cur_LT);

 With Lev.Lights[Cur_LT] do begin rx:=x; ry:=y; rz:=z; end;

 hg:=GlobalAlloc(GMEM_MOVEABLE or GMEM_SHARE,sizeof(integer)+ltsel.Count*GetLightRecSize);
 pg:=GlobalLock(hg);
 Integer(pg^):=ltsel.count; inc(pchar(pg),sizeof(Integer));

 for i:=0 to ltsel.count-1 do
 begin
  light:=Lev.Lights[ltsel.getLT(i)];
  pl:=pg;
  GetLight(light,Pl^);
  pl^.x:=pl^.x-rx;
  pl^.y:=pl^.y-ry;
  pl^.z:=pl^.z-rz;
  inc(pchar(pg),GetLightRecSize);
 end;

 GlobalUnlock(hg);
 Clp:=Clipboard;
 Clp.Clear;
 Clp.SetAsHandle(CF_JKLights,hg);

 ltsel.DeleteN(n);

end;

Function PasteLights(Lev:TJKLevel;atX,atY,atZ:Double):Integer; {Returns first pasted Object}
var hg:integer;
    pg:Pointer;
    i,n:integer;
    light:TJedLight;
    clp:TClipboard;
begin
 Result:=-1;
 Clp:=Clipboard;
 Clp.Open;
try
 hg:=Clp.GetAsHandle(CF_JKLights);
 if hg=0 then exit;
 pg:=GlobalLock(hg);
 n:=Integer(pg^); inc(pchar(pg),sizeof(integer));

 Result:=Lev.Lights.count;

 for i:=0 to n-1 do
 begin
  light:=lev.NewLight;
  SetLight(light,TLightRec(pg^));
  inc(pchar(pg),GetLightRecSize);

  light.x:=light.x+atX;
  light.y:=light.y+atY;
  light.z:=light.z+atZ;
  Lev.Lights.Add(light);
 end;
finally
 GlobalUnlock(hg);
 Clp.Close;
end;
end;

Procedure CopyThings(Lev:TJKLevel;thsel:TTHMultiSel;Cur_TH:integer);
var hg:integer;
    i,j,n:integer;
    rx,ry,rz:double;
    pg:pointer;
    thing:TJKThing;
    clp:TClipboard;
    PT:PThingRec;
    x,y,z,pch,yaw,rol:double;
    size:integer;
    po:Pchar;
begin
 n:=thsel.AddTH(Cur_TH);

 With Lev.Things[Cur_TH] do begin rx:=x; ry:=y; rz:=z; end;

 {Offset things by R#}
 for i:=0 to thsel.count-1 do
 begin
  thing:=Lev.Things[thsel.getTH(i)];
  thing.x:=thing.x-rx;
  thing.y:=thing.y-ry;
  thing.z:=thing.z-rz;
  for j:=0 to thing.Vals.count-1 do
  With thing.Vals[j] do
  begin
   GetFrame(x,y,z,pch,yaw,rol);
   SetFrame(x-rx,y-ry,z-rz,pch,yaw,rol);
  end;
 end;


try

 size:=sizeof(integer);

 for i:=0 to thsel.count-1 do
 begin
  thing:=Lev.Things[thsel.getTH(i)];
  inc(size,GetThingRecSize(thing));
 end;


 hg:=GlobalAlloc(GMEM_MOVEABLE or GMEM_SHARE,size);
 pg:=GlobalLock(hg);
 po:=pg;
 Integer(pg^):=thsel.count; inc(pchar(pg),sizeof(Integer));

 for i:=0 to thsel.count-1 do
 begin
  thing:=Lev.Things[thsel.getTH(i)];
  pt:=pg;


  size:=GetThing(thing,Pt);
  inc(pchar(pg),size);

 { pt^.x:=pt^.x-rx;
  pt^.y:=pt^.y-ry;
  pt^.z:=pt^.z-rz;}
 end;

finally

  {Offset things back by R#}
 for i:=0 to thsel.count-1 do
 begin
  thing:=Lev.Things[thsel.getTH(i)];
  thing.x:=thing.x+rx;
  thing.y:=thing.y+ry;
  thing.z:=thing.z+rz;
  for j:=0 to thing.Vals.count-1 do
  With thing.Vals[j] do
  begin
   GetFrame(x,y,z,pch,yaw,rol);
   SetFrame(x+rx,y+ry,z+rz,pch,yaw,rol);
  end;
 end;

end;

 GlobalUnlock(hg);
 Clp:=Clipboard;
 Clp.Clear;
 Clp.SetAsHandle(CF_JKThings,hg);

 thsel.DeleteN(n);
 if po=nil then;
end;

Function PasteThings(Lev:TJKLevel;atX,atY,atZ:Double):Integer; {Returns first pasted Object}
var hg:integer;
    pg:Pointer;
    i,j,n:integer;
    thing:TJKThing;
    clp:TClipboard;
    x,y,z,pch,yaw,rol:double;
    size:integer;
begin
 Result:=-1;
 Clp:=Clipboard;
 Clp.Open;
 hg:=Clp.GetAsHandle(CF_JKThings);
 if hg=0 then exit;
try
 pg:=GlobalLock(hg);
 n:=Integer(pg^); inc(pchar(pg),sizeof(integer));

 Result:=Lev.Things.count;

 for i:=0 to n-1 do
 begin
  thing:=lev.NewThing;
  inc(pchar(pg),SetThing(thing,PThingRec(pg)));

  thing.x:=thing.x+atX;
  thing.y:=thing.y+atY;
  thing.z:=thing.z+atZ;

  for j:=0 to thing.Vals.count-1 do
  With thing.Vals[j] do
  if atype=at_frame then
  begin
   GetFrame(x,y,z,pch,yaw,rol);
   SetFrame(x+atX,y+atY,z+atZ,pch,yaw,rol);
  end;

  Lev.Things.Add(thing);

 end;
finally
 lev.RenumThings;
 GlobalUnlock(hg);
 Clp.Close;
end;
end;


Initialization
begin
 CF_JKThings:=RegisterClipboardFormat('JKTHINGS');
 CF_JKLights:=RegisterClipboardFormat('JKLIGHTS');
 CF_JKSectors:=RegisterClipboardFormat('JKSECTORS');
end;
end.
