unit U_copypaste;

interface
uses J_Level, U_Multisel, Lev_Utils, values;

Procedure CopySectors(lev:TJKLevel; scsel: TSCMultiSel; Cur_SC: Integer);
Procedure CopyThings(lev:TJKLevel; thsel: TTHMultiSel; Cur_TH: Integer);
Procedure CopyLights(lev:TJKLevel; ltsel: TLTMultiSel; Cur_LT: Integer);

Function CanPasteSectors: Boolean;
Function CanPasteThings: Boolean;
Function CanPasteLights: Boolean;

Function PasteSectors(lev: TJKLevel; atX,atY,atZ: Double; bTranslate: Boolean): Integer; {Returns first pasted Sector}
Function PasteThings(lev: TJKLevel; atX,atY,atZ: Double; bTranslate: Boolean): Integer; {Returns first pasted Object}
Function PasteLights(lev: TJKLevel; atX,atY,atZ: Double; bTranslate: Boolean): Integer; {Returns first pasted Object}

implementation
uses U_undo, Clipbrd, WIndows;

var CF_JKSectors: Word;
    CF_JKThings: Word;
    CF_JKLights: Word;
    at_frame: TAdjType;

Function CanPasteSectors:Boolean;
var Clp:TClipboard;
begin
 Clp := Clipboard;
 Result := Clp.HasFormat(CF_JKSectors);
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

Procedure CopySectors(lev: TJKLevel; scsel:TSCMultiSel; Cur_SC: Integer);
var i,j,sf,n,msize: integer;
    sec: TJKSector;
    surf: TJKSurface;
    hg: integer;
    pg: pointer;
    clp: TClipboard;
    //rx,ry,rz: double;
    //pvx: ^TVXRec;

begin
  n := scsel.AddSC(Cur_SC);

  //FindCenter(lev.sectors[Cur_SC], rx, ry, rz);

  {Calculate the size of memory to allocate}
  msize := sizeof(longint);
  for i := 0 to scsel.Count-1 do
    begin
      inc(msize, GetSecRecSize);
      sec := lev.sectors[scsel.GetSC(i)];

      inc(msize, SCVertSize(sec.vertices));

      for sf:=0 to sec.surfaces.Count-1 do
        begin
         surf := sec.surfaces[sf];
         inc(msize, GetSurfRecSize);
         inc(msize, SFVertSize(surf.vertices));
        end;
    end;

  hg := GlobalAlloc(GMEM_MOVEABLE or GMEM_SHARE, msize);
  pg := GlobalLock(hg);
  LongInt(pg^) := scsel.Count;
  inc(PAnsiChar(pg), sizeof(Longint));

  for i := 0 to scsel.Count - 1 do
    begin
      sec := lev.sectors[scsel.GetSC(i)];
      GetSec(sec, TSecRec(pg^));
      inc(PAnsiChar(pg), GetSecRecSize);

      GetSCVertices(sec.vertices, pg);

//      pvx := pg;
//      for j:=0 to sec.vertices.Count - 1 do
//        begin
////         pvx^.x := pvx^.x - rx;
////         pvx^.y := pvx^.y - ry;
////         pvx^.z := pvx^.z - rz;
//         Inc(pvx);
//        end;

      Inc(PAnsiChar(pg),SCVertSize(sec.vertices));

      for sf := 0 to sec.surfaces.Count - 1 do
        begin
         surf := sec.surfaces[sf];

         GetSurf(surf, TSurfRec(pg^));
         inc(PAnsiChar(pg), GetSurfRecSize);

         GetSFVertices(surf,pg);
         inc(PAnsiChar(pg), SFVertSize(surf.vertices));
        end;
    end;

  GlobalUnlock(hg);
  Clp := Clipboard;
  Clp.Clear;
  Clp.SetAsHandle(CF_JKSectors, hg);
  Clp.Close;

  scsel.DeleteN(n);
end;

Function PasteSectors(lev: TJKLevel; atX, atY, atZ: Double; bTranslate: Boolean): Integer; {Returns first pasted Sector}
var hg: Integer;
    pg: Pointer;
    i,j,sf,n: Integer;
    sec: TJKSector;
    surf: TJKSurface;
    vx: TJKVertex;
    clp: TClipboard;
    psrec: ^TSecRec;
    psfrec: ^TSurfRec;
    tx, ty, tz: Double;
begin
  Result := -1;
  Clp := Clipboard;
  Clp.Open;

  try
   hg := Clp.GetAsHandle(CF_JKSectors);
   if hg = 0 then exit;
   pg := GlobalLock(hg);
   n  := Longint(pg^);
   Inc(PAnsiChar(pg), sizeof(longint));

   Result := lev.sectors.Count;
   for i := 0 to n - 1 do
     begin
      sec := lev.NewSector;
      lev.sectors.Add(sec);

      psrec := pg;
      Inc(PAnsiChar(pg), GetSecRecSize);

      SetSec(sec, PSrec^);
      for j := 0 to PSrec^.nvxs - 1 do sec.NewVertex;

      SetSCVertices(sec.vertices, PSrec^.nvxs, pg);
      Inc(PAnsiChar(pg), SCVertSize(sec.vertices));

//      for j := 0 to sec.vertices.Count - 1 do
//      with sec.vertices[j] do
//        begin
//         x := x + AtX;
//         y := y + AtY;
//         z := z + AtZ;
//        end;

      sec.Renumber;

      for sf := 0 to PSrec^.nsfs - 1 do
        begin
         surf := sec.NewSurface;
         sec.surfaces.Add(surf);

         psfRec := pg;
         Inc(PAnsiChar(pg), GetSurfRecSize);

         SetSurf(surf, PSfRec^);

         for j := 0 to PSfRec^.Nvxs - 1 do surf.AddVertex(nil);

         SetSFVertices(sec, surf, PSfRec^.nvxs, pg);
         Inc(PAnsiChar(pg), SFVertSize(surf.vertices));

         surf.RecalcAll;
        end;

      sec.Renumber;
     end;

    if bTranslate then
    begin
      sec := lev.sectors.Last; // should be the selected sector when selection was copied
      FindCenter(sec, tx, ty, tz);
      tx := (atX - tx) * Ord(atX <> 0);
      ty := (atY - ty) * Ord(atY <> 0);
      tz := (atZ - tz) * Ord(atZ <> 0);

      for i := lev.sectors.Count - n to lev.sectors.Count - 1 do
        begin
          sec := lev.sectors[i];
          for j := 0 to sec.vertices.Count - 1 do
            begin
             var v := sec.vertices[j];
             v.x := v.x + tx;
             v.y := v.y + ty;
             v.z := v.z + tz;
            end;
            sec.Renumber;

          for j := 0 to sec.surfaces.Count - 1 do
             sec.surfaces[j].RecalcAll;
          sec.Renumber;
        end;
    end;
  finally
   lev.RenumSecs;
   GlobalUnlock(hg);
   clp.Close;
  end;
end;

Procedure CopyLights(lev: TJKLevel; ltsel: TLTMultiSel; Cur_LT: Integer);
var hg: Integer;
    i,n: Integer;
    //rx,ry,rz: double;
    pg: pointer;
    light: TSedLight;
    clp: TClipboard;
    PL: ^TlightRec;
begin
  n := ltsel.AddLT(Cur_LT);

  with lev.Lights[Cur_LT] do

  hg := GlobalAlloc(GMEM_MOVEABLE or GMEM_SHARE,sizeof(integer)+ltsel.Count*GetLightRecSize);
  pg := GlobalLock(hg);
  Integer(pg^) := ltsel.count;
  Inc(PAnsiChar(pg), sizeof(Integer));

  for i := 0 to ltsel.Count - 1 do
    begin
      light := lev.lights[ltsel.getLT(i)];
      pl := pg;
      GetLight(light,Pl^);
      Inc(PAnsiChar(pg), GetLightRecSize);
    end;

  GlobalUnlock(hg);
  clp := Clipboard;
  clp.Clear;
  clp.SetAsHandle(CF_JKLights,hg);
  clp.Close;

  ltsel.DeleteN(n);
end;

Function PasteLights(lev: TJKLevel; atX,atY,atZ: Double; bTranslate: Boolean): Integer; {Returns first pasted Object}
var hg: Integer;
    pg: Pointer;
    i,n: Integer;
    light: TSedLight;
    clp: TClipboard;
    tx, ty, tz: Double;
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
      Inc(PAnsiChar(pg), GetLightRecSize);

      light.position.x := light.position.x;
      light.position.y := light.position.y;
      light.position.z := light.position.z;
      lev.lights.Add(light);
     end;

    if bTranslate then
    begin
      light := lev.lights.Last; // should be the selected light when selection was copied
      tx := (atX - light.position.x) * Ord(atX <> 0);
      ty := (atY - light.position.y) * Ord(atY <> 0);
      tz := (atZ - light.position.z) * Ord(atZ <> 0);

      for i := lev.lights.Count - n to lev.lights.Count - 1 do
        begin
          light := lev.lights[i];
          light.position.x := light.position.x + tx;
          light.position.y := light.position.y + ty;
          light.position.z := light.position.z + tz;
        end;
    end;

  finally
    GlobalUnlock(hg);
    Clp.Close;
  end;
end;

Procedure CopyThings(lev: TJKLevel;thsel:TTHMultiSel;Cur_TH:integer);
var hg: Integer;
    i,j,n: Integer;
    //rx,ry,rz: Double;
    pg: Pointer;
    thing: TJKThing;
    clp: TClipboard;
    PT: PThingRec;
    x,y,z,pch,yaw,rol: Double;
    size: Integer;
    po: PAnsiChar;
begin
  n := thsel.AddTH(Cur_TH);
  try
    size := SizeOf(integer);
    for i := 0 to thsel.Count - 1 do
      begin
        thing := lev.things[thsel.getTH(i)];
        Inc(size, GetThingRecSize(thing));
      end;

    hg := GlobalAlloc(GMEM_MOVEABLE or GMEM_SHARE, size);
    pg := GlobalLock(hg);
    po := pg;
    Integer(pg^) := thsel.Count; Inc(PAnsiChar(pg), SizeOf(Integer));

    for i := 0 to thsel.Count - 1 do
    begin
      thing := lev.things[thsel.getTH(i)];
      pt := pg;

      size := GetThing(thing, Pt);
      Inc(PAnsiChar(pg), size);
    end;

  finally

  end;

 GlobalUnlock(hg);
 clp := Clipboard;
 clp.Clear;
 clp.SetAsHandle(CF_JKThings,hg);
 clp.Close;

 thsel.DeleteN(n);
 if po = nil then;
end;

Function PasteThings(lev: TJKLevel; atX, atY, atZ: Double; bTranslate: Boolean): Integer; {Returns first pasted Object}
var hg: Integer;
    pg: Pointer;
    i,j,n: Integer;
    thing: TJKThing;
    clp: TClipboard;
    x,y,z,pch,yaw,rol: Double;
    tx,ty,tz: Double;
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
    Inc(PAnsiChar(pg), sizeof(integer));

    if n < 1 then
      exit;

    Result := lev.things.Count;
    for i := 0 to n - 1 do
      begin
        thing := lev.NewThing;
        Inc(PAnsiChar(pg), SetThing(thing, PThingRec(pg)));
        lev.things.Add(thing);
      end;

      if bTranslate then
      begin
        thing := lev.things.Last; // should be the selected thing when selection was copied
        tx := (atX - thing.x) * Ord(atX <> 0);
        ty := (atY - thing.y) * Ord(atY <> 0);
        tz := (atZ - thing.z) * Ord(atZ <> 0);

        for i := lev.things.Count - n to lev.things.Count - 1 do
          begin
            thing := lev.things[i];
            thing.x := thing.x + tx;
            thing.y := thing.y + ty;
            thing.z := thing.z + tz;

            // Transform thing frames
            for j := 0 to thing.vals.Count - 1 do
            with thing.vals[j] do
            if atype = at_frame then
              begin
                GetFrame(x, y, z, pch, yaw, rol);
                SetFrame(x + tx, y + ty, z + tz, pch, yaw, rol);
              end;
          end;

      end;
  finally
    lev.RenumThings;
    GlobalUnlock(hg);
    clp.Close;
  end;
end;


Initialization
begin
 CF_JKThings  := RegisterClipboardFormat('SEDTHINGS');
 CF_JKLights  := RegisterClipboardFormat('SEDLIGHTS');
 CF_JKSectors := RegisterClipboardFormat('SEDSECTORS');
end;
end.
