unit U_copypaste;

interface
uses J_Level, U_Multisel, Lev_Utils, values;

Procedure CopySectors(lev:TJKLevel; scsel: TSCMultiSel; Cur_SC: Integer);
Procedure CopyThings(lev:TJKLevel; thsel: TTHMultiSel; Cur_TH: Integer);
Procedure CopyLights(lev:TJKLevel; ltsel: TLTMultiSel; Cur_LT: Integer);

Function CanPasteSectors: Boolean;
Function CanPasteThings: Boolean;
Function CanPasteLights: Boolean;

Function PasteSectors(lev: TJKLevel; atX,atY,atZ: Double): Integer; {Returns first pasted Sector}
Function PasteThings(lev: TJKLevel; atX,atY,atZ: Double): Integer; {Returns first pasted Object}
Function PasteLights(lev: TJKLevel; atX,atY,atZ: Double): Integer; {Returns first pasted Object}

implementation
uses U_undo, Clipbrd, WIndows;

var CF_JKSectors:Word;
    CF_JKThings:Word;
    CF_JKLights:Word;
    at_frame:TAdjType;

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
var i,j,sf,n,msize: integer;
    sec: TJKSector;
    surf: TJKSurface;
    hg: integer;
    pg: pointer;
    clp: TClipboard;
    rx,ry,rz: double;
    pvx: ^TVXRec;

begin
  n:=scsel.AddSC(Cur_SC);

  FindCenter(Lev.Sectors[Cur_SC], rx, ry, rz);

  {Calculate the size of memory to allocate}
  msize := sizeof(longint);
  for i:=0 to scsel.count-1 do
    begin
      inc(msize, GetSecRecSize);
      sec:=lev.Sectors[scsel.GetSC(i)];

      inc(msize, SCVertSize(sec.vertices));

      for sf:=0 to sec.surfaces.count-1 do
        begin
         surf:=sec.surfaces[sf];
         inc(msize, GetSurfRecSize);
         inc(msize, SFVertSize(surf.vertices));
        end;
    end;

  hg := GlobalAlloc(GMEM_MOVEABLE or GMEM_SHARE,msize);
  pg := GlobalLock(hg);
  Longint(pg^) := scsel.count;
  inc(PAnsiChar(pg), sizeof(Longint));

  for i:=0 to scsel.count-1 do
    begin
      sec:=lev.Sectors[scsel.GetSC(i)];
      GetSec(sec,TSecRec(pg^));
      inc(PAnsiChar(pg),GetSecRecSize);

      GetSCVertices(sec.vertices,pg);

      pvx:=pg;
      for j:=0 to sec.vertices.count-1 do
        begin
         pvx^.x:=pvx^.x-rx;
         pvx^.y:=pvx^.y-ry;
         pvx^.z:=pvx^.z-rz;
         inc(pvx);
        end;

      inc(PAnsiChar(pg),SCVertSize(sec.vertices));

      for sf:=0 to sec.surfaces.count-1 do
        begin
         surf:=sec.surfaces[sf];

         GetSurf(surf,TSurfRec(pg^));
         inc(PAnsiChar(pg), GetSurfRecSize);

         GetSFVertices(surf,pg);
         inc(PAnsiChar(pg), SFVertSize(surf.vertices));
        end;
    end;

  GlobalUnlock(hg);
  Clp:=Clipboard;
  Clp.Clear;
  Clp.SetAsHandle(CF_JKSectors, hg);
  Clp.Close;

  scsel.DeleteN(n);
end;

Function PasteSectors(lev: TJKLevel; atX, atY, atZ: Double): Integer; {Returns first pasted Sector}
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
   hg := Clp.GetAsHandle(CF_JKSectors);
   if hg = 0 then exit;
   pg := GlobalLock(hg);
   n  := Longint(pg^);
   inc(PAnsiChar(pg),sizeof(longint));

   Result:=Lev.Sectors.count;

   for i:=0 to n-1 do
     begin
      sec:=Lev.NewSector;
      Lev.Sectors.Add(sec);

      psrec:=pg;
      inc(PAnsiChar(pg), GetSecRecSize);

      SetSec(sec,PSrec^);
      for j:=0 to PSrec^.nvxs-1 do sec.NewVertex;

      SetSCVertices(sec.vertices, PSrec^.nvxs, pg);
      inc(PAnsiChar(pg), SCVertSize(sec.vertices));

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
         inc(PAnsiChar(pg), GetSurfRecSize);

         SetSurf(surf,PSfRec^);

         for j:=0 to PSfRec^.Nvxs-1 do surf.AddVertex(nil);

         SetSFVertices(sec,surf,PSfRec^.nvxs,pg);
         inc(PAnsiChar(pg), SFVertSize(surf.vertices));

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

Procedure CopyLights(lev: TJKLevel; ltsel: TLTMultiSel; Cur_LT: Integer);
var hg: Integer;
    i,n: Integer;
    rx,ry,rz: double;
    pg: pointer;
    light: TSedLight;
    clp: TClipboard;
    PL: ^TlightRec;
begin
  n := ltsel.AddLT(Cur_LT);

  with lev.Lights[Cur_LT] do
    begin
      rx := position.x;
      ry := position.y;
      rz := position.z;
    end;

  hg := GlobalAlloc(GMEM_MOVEABLE or GMEM_SHARE,sizeof(integer)+ltsel.Count*GetLightRecSize);
  pg := GlobalLock(hg);
  Integer(pg^) := ltsel.count;
  Inc(PAnsiChar(pg), sizeof(Integer));

  for i := 0 to ltsel.Count - 1 do
    begin
      light:=lev.lights[ltsel.getLT(i)];
      pl := pg;
      GetLight(light,Pl^);
      pl^.position.x :=pl^.position.x - rx;
      pl^.position.y :=pl^.position.y - ry;
      pl^.position.z :=pl^.position.z - rz;
      inc(PAnsiChar(pg), GetLightRecSize);
    end;

  GlobalUnlock(hg);
  clp := Clipboard;
  clp.Clear;
  clp.SetAsHandle(CF_JKLights,hg);
  clp.Close;

  ltsel.DeleteN(n);
end;

Function PasteLights(lev: TJKLevel; atX,atY,atZ: Double): Integer; {Returns first pasted Object}
var hg: Integer;
    pg: Pointer;
    i,n: Integer;
    light: TSedLight;
    clp: TClipboard;
begin
  Result := -1;
  Clp := Clipboard;
  Clp.Open;
  try
   hg := clp.GetAsHandle(CF_JKLights);
   if hg = 0 then exit;

   pg := GlobalLock(hg);
   n  := Integer(pg^);
   Inc(PAnsiChar(pg), SizeOf(integer));

   Result := lev.lights.Count;
   for i := 0 to n - 1 do
     begin
      light := lev.NewLight;
      SetLight(light, TLightRec(pg^));
      inc(PAnsiChar(pg), GetLightRecSize);

      light.position.x := light.position.x + atX;
      light.position.y := light.position.y + atY;
      light.position.z := light.position.z + atZ;
      lev.lights.Add(light);
     end;
  finally
    GlobalUnlock(hg);
    Clp.Close;
  end;
end;

Procedure CopyThings(lev: TJKLevel;thsel:TTHMultiSel;Cur_TH:integer);
var hg: Integer;
    i,j,n: Integer;
    rx,ry,rz: Double;
    pg: Pointer;
    thing: TJKThing;
    clp: TClipboard;
    PT: PThingRec;
    x,y,z,pch,yaw,rol: Double;
    size: Integer;
    po: PAnsiChar;
begin
  n := thsel.AddTH(Cur_TH);
  With lev.things[Cur_TH] do
    begin
      rx := x;
      ry := y;
      rz := z;
    end;

  {Offset things by R#}
  for i := 0 to thsel.Count-1 do
    begin
      thing   := lev.things[thsel.getTH(i)];
      thing.x := thing.x-rx;
      thing.y := thing.y-ry;
      thing.z := thing.z-rz;
      for j := 0 to thing.Vals.count-1 do
      with thing.vals[j] do
        begin
         GetFrame(x,y,z, pch,yaw,rol);
         SetFrame(x - rx, y - ry, z - rz, pch,yaw,rol);
        end;
    end;

  try
    size := SizeOf(integer);

    for i:=0 to thsel.Count-1 do
      begin
        thing := lev.things[thsel.getTH(i)];
        Inc(size, GetThingRecSize(thing));
      end;


    hg := GlobalAlloc(GMEM_MOVEABLE or GMEM_SHARE,size);
    pg := GlobalLock(hg);
    po := pg;
    Integer(pg^) := thsel.Count; Inc(PAnsiChar(pg), SizeOf(Integer));

    for i := 0 to thsel.Count - 1 do
    begin
      thing := lev.things[thsel.getTH(i)];
      pt := pg;

      size := GetThing(thing, Pt);
      Inc(PAnsiChar(pg), size);
      { pt^.x:=pt^.x-rx;
      pt^.y:=pt^.y-ry;
      pt^.z:=pt^.z-rz;}
    end;

  finally

    {Offset things back by R#}
  for i:=0 to thsel.count-1 do
    begin
      thing   := lev.things[thsel.getTH(i)];
      thing.x := thing.x + rx;
      thing.y := thing.y + ry;
      thing.z := thing.z + rz;
      for j := 0 to thing.vals.Count - 1 do
        with thing.vals[j] do
          begin
           GetFrame(x,y,z, pch,yaw,rol);
           SetFrame(x + rx, y + ry, z + rz, pch,yaw,rol);
          end;
    end;
  end;

 GlobalUnlock(hg);
 clp := Clipboard;
 clp.Clear;
 clp.SetAsHandle(CF_JKThings,hg);
 clp.Close;

 thsel.DeleteN(n);
 if po = nil then;
end;

Function PasteThings(lev: TJKLevel; atX, atY, atZ: Double): Integer; {Returns first pasted Object}
var hg: Integer;
    pg: Pointer;
    i,j,n: Integer;
    thing: TJKThing;
    clp: TClipboard;
    x,y,z,pch,yaw,rol: Double;
    size: Integer;
begin
  Result := -1;
  clp := Clipboard;
  clp.Open;
  try
    hg := clp.GetAsHandle(CF_JKThings);
    if hg = 0 then exit;

    pg := GlobalLock(hg);
    n := Integer(pg^);
    inc(PAnsiChar(pg), sizeof(integer));

    Result := lev.things.Count;
    for i := 0 to n - 1 do
      begin
        thing := lev.NewThing;
        inc(PAnsiChar(pg), SetThing(thing, PThingRec(pg)));

        thing.x := thing.x + atX;
        thing.y := thing.y + atY;
        thing.z := thing.z + atZ;

        for j:=0 to thing.vals.Count - 1 do
        With thing.vals[j] do
        if atype = at_frame then
          begin
            GetFrame(x, y, z, pch, yaw, rol);
            SetFrame(x + atX, y + atY, z + atZ, pch, yaw, rol);
          end;

        lev.things.Add(thing);
      end;
  finally
    lev.RenumThings;
    GlobalUnlock(hg);
    clp.Close;
  end;
end;


Initialization
begin
 CF_JKThings:=RegisterClipboardFormat('JKTHINGS');
 CF_JKLights:=RegisterClipboardFormat('JKLIGHTS');
 CF_JKSectors:=RegisterClipboardFormat('JKSECTORS');
end;
end.
